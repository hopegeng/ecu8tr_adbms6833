#!/usr/bin/env python3
"""PyQt5/Kvaser GUI for LTC6812 Gen 3.0 EEPROM feature testing.

The tool sends the BorgWarner M001 EEPROM CAN commands implemented by the
firmware. Kvaser access is through python-can's "kvaser" backend.

Usage:
    python ltc6812_eeprom_test.py --channel 0 --bitrate 500000
"""

import argparse
import struct
import sys
import threading
import time
from dataclasses import dataclass

import can
from PyQt5 import QtCore, QtWidgets


# CAN IDs, matching src/app/dbc/dbc_trace_interface.h.
ID_M001_BMS_INFO = 0x10005801
ID_M001_EEPROM_DATA = 0x10007801
ID_M001_SEND_EEPROM_EOL = 0x1000F801
ID_M001_WRITE_EEPROM = 0x10087801
ID_M001_START_LTC = 0x10088801
ID_M001_EEPROM_CHANGE_DYN_AREA = 0x10089801
ID_M001_CONFIGURATION = 0x1808D801

START_MAGIC = 0x953CA8
EEPROM_READ_MUX = 0xEE5200

MUX_CELL_TYPE = 0xCE11AA55
MUX_IC_TYPE = 0xCEA1AA55
MUX_MODULE_TYPE = 0xADADAA55
MUX_PCB_TYPE = 0x11CEAA55
MUX_MODULE_SERIAL_LO = 0xD0D0AA55
MUX_MODULE_SERIAL_HI = 0xDADAAA55


@dataclass(frozen=True)
class StartAction:
    index: int
    name: str
    detail: str
    dangerous: bool = False


START_ACTIONS = (
    StartAction(0, "Write Safety Area", "Recalculate CRC16, write ID Page + ID Backup", True),
    StartAction(1, "Write Static Area", "Recalculate CRC32, write Static Area 1 + 2", True),
    StartAction(2, "Write Dynamic Area", "Recalculate CRC32, write selected/repair dynamic area", True),
    StartAction(3, "Reread All EEPROM", "Read main EEPROM + ID Page and update CRC/status flags"),
    StartAction(4, "Read ID Page Status", "Refresh ID Page lock/status flags"),
    StartAction(5, "Lock ID Page", "Irreversibly lock the M24C08 ID/ODP page", True),
    StartAction(6, "EOL Init Dynamic Area", "Clear both dynamic mirrors, CRC32, write both areas", True),
)


def payload_start_ltc(action_index: int) -> bytes:
    data = bytearray(8)
    data[0] = START_MAGIC & 0xFF
    data[1] = (START_MAGIC >> 8) & 0xFF
    data[2] = (START_MAGIC >> 16) & 0xFF
    data[3] = action_index & 0xFF
    return bytes(data)


def payload_eeprom_write(index: int, data4: bytes) -> bytes:
    data = bytearray(8)
    data[3] = index & 0xFF
    data[4:8] = bytes(data4[:4]).ljust(4, b"\x00")
    return bytes(data)


def payload_eeprom_read(index: int) -> bytes:
    data = bytearray(8)
    data[0] = EEPROM_READ_MUX & 0xFF
    data[1] = (EEPROM_READ_MUX >> 8) & 0xFF
    data[2] = (EEPROM_READ_MUX >> 16) & 0xFF
    data[3] = index & 0xFF
    return bytes(data)


def payload_config_2bytes(mux: int, major: int, minor: int) -> bytes:
    data = bytearray(8)
    struct.pack_into("<I", data, 0, mux & 0xFFFFFFFF)
    data[4] = major & 0xFF
    data[5] = minor & 0xFF
    return bytes(data)


def payload_config_4bytes(mux: int, data4: bytes) -> bytes:
    data = bytearray(8)
    struct.pack_into("<I", data, 0, mux & 0xFFFFFFFF)
    data[4:8] = bytes(data4[:4]).ljust(4, b"\x00")
    return bytes(data)


def hex_bytes(data: bytes) -> str:
    return " ".join(f"{b:02X}" for b in data)


def parse_data4(text: str) -> bytes:
    cleaned = text.replace(",", " ").replace("0x", "").replace("0X", "")
    parts = [p for p in cleaned.split() if p]
    if len(parts) == 1 and len(parts[0]) == 8:
        token = parts[0]
        return bytes(int(token[i:i + 2], 16) for i in range(0, 8, 2))
    values = [int(p, 16) for p in parts]
    if len(values) != 4:
        raise ValueError("enter exactly 4 bytes, for example: 11 22 33 44")
    if any(v < 0 or v > 0xFF for v in values):
        raise ValueError("byte value out of range")
    return bytes(values)


def annotate_eeprom(index: int, data4: bytes) -> str:
    addr = index * 4
    d = bytes(data4)
    if addr == 0x0000:
        return f"ID Backup CRC16=0x{d[1]:02X}{d[0]:02X}, CellType={d[2]}.{d[3]}"
    if addr == 0x0004:
        return f"ModuleType={d[0]}.{d[1]}, ModuleSerial[0..1]={d[2]:02X} {d[3]:02X}"
    if addr == 0x0008:
        return f"ModuleSerial[2..5]={hex_bytes(d)}"
    if addr == 0x000C:
        return f"ModuleSerial[6..7]={d[0]:02X} {d[1]:02X}, PcbType={d[2]}.{d[3]}"
    if addr in (0x0010, 0x00B0, 0x0150, 0x02A0):
        return f"CRC32/store word=0x{int.from_bytes(d, 'little'):08X}"
    if addr in (0x0014, 0x00B4):
        return f"SerialCells={d[0]}, ParallelCells={d[1]}, PcbSerial[0..1]={d[2]:02X} {d[3]:02X}"
    if addr in (0x001C, 0x00BC):
        return f"PcbSerial[6..7]={d[0]:02X} {d[1]:02X}, IcType={d[2]}.{d[3]}"
    return ""


def decode_bms_info_flags(data: bytes) -> str:
    if len(data) < 8:
        return ""
    flags = []
    if data[0] & 0x10:
        flags.append("EEPROMError")
    if data[4] & 0x20:
        flags.append("CompleteSafetyCRCError")
    if data[4] & 0x40:
        flags.append("CompleteStaticCRCError")
    if data[4] & 0x80:
        flags.append("CompleteDynamicCRCError")
    if data[5] & 0x02:
        flags.append("SingleSafetyCRCError")
    if data[5] & 0x04:
        flags.append("SingleStaticCRCError")
    if data[5] & 0x08:
        flags.append("SingleDynamicCRCError")
    if data[5] & 0x10:
        flags.append("SafetyAreaMismatch")
    if data[5] & 0x20:
        flags.append("StaticAreaMismatch")
    if data[5] & 0x40:
        flags.append("IDPageNotLocked")
    if data[5] & 0x80:
        flags.append("I2CCommunicationFail")
    if data[6] & 0x01:
        flags.append("EEPROMReadFail")
    if data[6] & 0x02:
        flags.append("IDPageStatusReadFail")
    return ", ".join(flags) if flags else "no EEPROM/status flags set"


class CanWorker(QtCore.QObject):
    message_received = QtCore.pyqtSignal(object)
    status_changed = QtCore.pyqtSignal(str)
    tx_done = QtCore.pyqtSignal(int, bytes)

    def __init__(self, channel: int, bitrate: int):
        super().__init__()
        self.channel = channel
        self.bitrate = bitrate
        self.bus = None
        self.thread = None
        self.running = False

    def start(self) -> None:
        try:
            self.bus = can.Bus(interface="kvaser", channel=self.channel, bitrate=self.bitrate)
        except Exception as exc:
            self.status_changed.emit(f"CAN open failed: {exc}")
            return
        self.running = True
        self.thread = threading.Thread(target=self._rx_loop, daemon=True)
        self.thread.start()
        self.status_changed.emit(f"Kvaser channel {self.channel}, {self.bitrate} bit/s connected")

    def stop(self) -> None:
        self.running = False
        if self.thread is not None:
            self.thread.join(timeout=1.0)
        if self.bus is not None:
            self.bus.shutdown()
            self.bus = None

    def send(self, arbitration_id: int, data: bytes) -> None:
        if self.bus is None:
            self.status_changed.emit("CAN bus is not open")
            return
        payload = bytes(data).ljust(8, b"\x00")[:8]
        msg = can.Message(arbitration_id=arbitration_id, is_extended_id=True, data=payload)
        try:
            self.bus.send(msg, timeout=0.2)
        except Exception as exc:
            self.status_changed.emit(f"TX failed 0x{arbitration_id:08X}: {exc}")
            return
        self.tx_done.emit(arbitration_id, payload)

    def _rx_loop(self) -> None:
        while self.running:
            try:
                msg = self.bus.recv(timeout=0.1)
            except Exception as exc:
                self.status_changed.emit(f"RX failed: {exc}")
                time.sleep(0.5)
                continue
            if msg is not None:
                self.message_received.emit(msg)


class MainWindow(QtWidgets.QMainWindow):
    def __init__(self, worker: CanWorker):
        super().__init__()
        self.worker = worker
        self.setWindowTitle("LTC6812 EEPROM Test - PyQt5/Kvaser")
        self.resize(1180, 760)

        self.worker.message_received.connect(self.on_message)
        self.worker.status_changed.connect(self.on_status)
        self.worker.tx_done.connect(self.on_tx_done)

        central = QtWidgets.QWidget()
        root = QtWidgets.QVBoxLayout(central)

        top = QtWidgets.QHBoxLayout()
        self.status = QtWidgets.QLabel("CAN closed")
        self.status.setMinimumWidth(420)
        top.addWidget(self.status)
        top.addStretch(1)
        clear_btn = QtWidgets.QPushButton("Clear Log")
        clear_btn.clicked.connect(lambda: self.log.clear())
        top.addWidget(clear_btn)
        root.addLayout(top)

        split = QtWidgets.QSplitter()
        split.addWidget(self._build_controls())
        split.addWidget(self._build_log())
        split.setStretchFactor(0, 0)
        split.setStretchFactor(1, 1)
        root.addWidget(split, 1)

        self.setCentralWidget(central)

    def _build_controls(self) -> QtWidgets.QWidget:
        panel = QtWidgets.QWidget()
        layout = QtWidgets.QVBoxLayout(panel)

        actions = QtWidgets.QGroupBox("M001_StartLTCTransmission Actions")
        grid = QtWidgets.QGridLayout(actions)
        for row, action in enumerate(START_ACTIONS):
            btn = QtWidgets.QPushButton(f"{action.index}: {action.name}")
            btn.clicked.connect(lambda checked=False, a=action: self.send_start_action(a))
            if action.dangerous:
                btn.setStyleSheet("QPushButton { background:#b91c1c; color:white; }")
            grid.addWidget(btn, row, 0)
            detail = QtWidgets.QLabel(action.detail)
            detail.setWordWrap(True)
            grid.addWidget(detail, row, 1)
        layout.addWidget(actions)

        dyn = QtWidgets.QGroupBox("Dynamic Area Selection")
        dyn_layout = QtWidgets.QHBoxLayout(dyn)
        self.dyn_area_a = QtWidgets.QRadioButton("Area A next")
        self.dyn_area_b = QtWidgets.QRadioButton("Area B next")
        self.dyn_area_a.setChecked(True)
        dyn_layout.addWidget(self.dyn_area_a)
        dyn_layout.addWidget(self.dyn_area_b)
        send_dyn = QtWidgets.QPushButton("Send M001_EEPROMChangeDynArea")
        send_dyn.clicked.connect(self.send_dynamic_area_select)
        dyn_layout.addWidget(send_dyn)
        layout.addWidget(dyn)

        raw = QtWidgets.QGroupBox("Raw Mirror Read / Write")
        form = QtWidgets.QGridLayout(raw)
        self.index_spin = QtWidgets.QSpinBox()
        self.index_spin.setRange(0, 255)
        self.index_spin.setValue(0)
        self.index_spin.valueChanged.connect(self.update_address_label)
        self.addr_label = QtWidgets.QLabel("address 0x0000")
        self.data_edit = QtWidgets.QLineEdit("11 22 33 44")
        read_btn = QtWidgets.QPushButton("Read 4 Bytes")
        read_btn.clicked.connect(self.send_eeprom_read)
        write_btn = QtWidgets.QPushButton("Mirror Write 4 Bytes")
        write_btn.clicked.connect(self.send_eeprom_write)
        form.addWidget(QtWidgets.QLabel("Index"), 0, 0)
        form.addWidget(self.index_spin, 0, 1)
        form.addWidget(self.addr_label, 0, 2)
        form.addWidget(QtWidgets.QLabel("Data"), 1, 0)
        form.addWidget(self.data_edit, 1, 1, 1, 2)
        form.addWidget(read_btn, 2, 1)
        form.addWidget(write_btn, 2, 2)
        layout.addWidget(raw)

        cfg = QtWidgets.QGroupBox("Configuration Message Mirror Writes")
        cfg_layout = QtWidgets.QGridLayout(cfg)
        self.cfg_combo = QtWidgets.QComboBox()
        for label, mux, kind in (
            ("Cell Type", MUX_CELL_TYPE, "2"),
            ("IC Type", MUX_IC_TYPE, "2"),
            ("Module Type", MUX_MODULE_TYPE, "2"),
            ("PCB Type", MUX_PCB_TYPE, "2"),
            ("Module Serial Low", MUX_MODULE_SERIAL_LO, "4"),
            ("Module Serial High", MUX_MODULE_SERIAL_HI, "4"),
        ):
            self.cfg_combo.addItem(label, (mux, kind))
        self.major_spin = QtWidgets.QSpinBox()
        self.major_spin.setRange(0, 255)
        self.minor_spin = QtWidgets.QSpinBox()
        self.minor_spin.setRange(0, 255)
        self.serial_edit = QtWidgets.QLineEdit("01 02 03 04")
        cfg_send = QtWidgets.QPushButton("Send M001_ConfigurationMessage")
        cfg_send.clicked.connect(self.send_config)
        cfg_layout.addWidget(QtWidgets.QLabel("Field"), 0, 0)
        cfg_layout.addWidget(self.cfg_combo, 0, 1, 1, 2)
        cfg_layout.addWidget(QtWidgets.QLabel("Major / Minor"), 1, 0)
        cfg_layout.addWidget(self.major_spin, 1, 1)
        cfg_layout.addWidget(self.minor_spin, 1, 2)
        cfg_layout.addWidget(QtWidgets.QLabel("4-byte serial data"), 2, 0)
        cfg_layout.addWidget(self.serial_edit, 2, 1, 1, 2)
        cfg_layout.addWidget(cfg_send, 3, 1, 1, 2)
        layout.addWidget(cfg)

        quick = QtWidgets.QGroupBox("Useful Read Indices")
        quick_layout = QtWidgets.QGridLayout(quick)
        quick_indices = (
            (0, "ID Backup 0"),
            (1, "ID Backup 1"),
            (2, "ID Backup 2"),
            (3, "ID Backup 3"),
            (4, "Static1 CRC"),
            (44, "Static2 CRC"),
            (84, "Dynamic1 CRC"),
            (168, "Dynamic2 CRC"),
        )
        for i, (index, label) in enumerate(quick_indices):
            btn = QtWidgets.QPushButton(f"{label} ({index})")
            btn.clicked.connect(lambda checked=False, idx=index: self.send_eeprom_read_index(idx))
            quick_layout.addWidget(btn, i // 2, i % 2)
        layout.addWidget(quick)

        layout.addStretch(1)
        return panel

    def _build_log(self) -> QtWidgets.QWidget:
        panel = QtWidgets.QWidget()
        layout = QtWidgets.QVBoxLayout(panel)
        self.last_eeprom = QtWidgets.QLabel("Last EEPROM response: none")
        self.last_flags = QtWidgets.QLabel("Last BMSInfo flags: none")
        self.last_flags.setWordWrap(True)
        layout.addWidget(self.last_eeprom)
        layout.addWidget(self.last_flags)
        self.log = QtWidgets.QPlainTextEdit()
        self.log.setReadOnly(True)
        layout.addWidget(self.log, 1)
        return panel

    def update_address_label(self) -> None:
        self.addr_label.setText(f"address 0x{self.index_spin.value() * 4:04X}")

    def log_line(self, text: str) -> None:
        timestamp = time.strftime("%H:%M:%S")
        self.log.appendPlainText(f"{timestamp}  {text}")

    def confirm_dangerous(self, action: StartAction) -> bool:
        if not action.dangerous:
            return True
        response = QtWidgets.QMessageBox.question(
            self,
            "Confirm EEPROM Action",
            f"Send ActionIndex {action.index}: {action.name}?\n\n{action.detail}",
            QtWidgets.QMessageBox.Yes | QtWidgets.QMessageBox.No,
            QtWidgets.QMessageBox.No,
        )
        return response == QtWidgets.QMessageBox.Yes

    def send_start_action(self, action: StartAction) -> None:
        if not self.confirm_dangerous(action):
            return
        self.worker.send(ID_M001_START_LTC, payload_start_ltc(action.index))

    def send_dynamic_area_select(self) -> None:
        payload = bytes([0x01 if self.dyn_area_a.isChecked() else 0x00, 0, 0, 0, 0, 0, 0, 0])
        self.worker.send(ID_M001_EEPROM_CHANGE_DYN_AREA, payload)

    def send_eeprom_read(self) -> None:
        self.send_eeprom_read_index(self.index_spin.value())

    def send_eeprom_read_index(self, index: int) -> None:
        self.index_spin.setValue(index)
        self.worker.send(ID_M001_WRITE_EEPROM, payload_eeprom_read(index))

    def send_eeprom_write(self) -> None:
        try:
            data4 = parse_data4(self.data_edit.text())
        except ValueError as exc:
            QtWidgets.QMessageBox.warning(self, "Invalid Data", str(exc))
            return
        self.worker.send(ID_M001_WRITE_EEPROM, payload_eeprom_write(self.index_spin.value(), data4))

    def send_config(self) -> None:
        mux, kind = self.cfg_combo.currentData()
        if kind == "2":
            payload = payload_config_2bytes(mux, self.major_spin.value(), self.minor_spin.value())
        else:
            try:
                payload = payload_config_4bytes(mux, parse_data4(self.serial_edit.text()))
            except ValueError as exc:
                QtWidgets.QMessageBox.warning(self, "Invalid Serial Data", str(exc))
                return
        self.worker.send(ID_M001_CONFIGURATION, payload)

    def on_tx_done(self, arbitration_id: int, payload: bytes) -> None:
        self.log_line(f"TX 0x{arbitration_id:08X}  {hex_bytes(payload)}")

    def on_status(self, text: str) -> None:
        self.status.setText(text)
        self.log_line(text)

    def on_message(self, msg) -> None:
        data = bytes(msg.data)
        if msg.arbitration_id == ID_M001_EEPROM_DATA and len(data) >= 8:
            index = data[0]
            dyn = "A" if (data[1] & 0x01) else "B"
            data4 = data[4:8]
            note = annotate_eeprom(index, data4)
            text = f"M001_EEPROMData idx={index} addr=0x{index * 4:04X} dynNext={dyn} data={hex_bytes(data4)}"
            if note:
                text += f"  {note}"
            self.last_eeprom.setText(text)
            self.log_line(f"RX {text}")
        elif msg.arbitration_id == ID_M001_BMS_INFO:
            flags = decode_bms_info_flags(data)
            self.last_flags.setText(f"Last BMSInfo flags: {flags}")
            self.log_line(f"RX M001_BMSInfo flags: {flags}")
        elif msg.arbitration_id == ID_M001_SEND_EEPROM_EOL:
            mux = int.from_bytes(data[0:4], "little")
            self.log_line(f"RX M001_SendEEPROMDataEOL mux=0x{mux:08X} data={hex_bytes(data[4:8])}")

    def closeEvent(self, event) -> None:
        self.worker.stop()
        event.accept()


def main() -> int:
    parser = argparse.ArgumentParser(description="LTC6812 EEPROM PyQt5/Kvaser tester")
    parser.add_argument("--channel", type=int, default=0, help="Kvaser channel index")
    parser.add_argument("--bitrate", type=int, default=500000, help="CAN bitrate")
    args = parser.parse_args()

    app = QtWidgets.QApplication(sys.argv)
    worker = CanWorker(args.channel, args.bitrate)
    window = MainWindow(worker)
    window.show()
    worker.start()
    return app.exec_()


if __name__ == "__main__":
    raise SystemExit(main())
