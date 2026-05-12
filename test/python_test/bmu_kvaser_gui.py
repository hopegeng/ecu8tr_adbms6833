#!/usr/bin/env python3
"""PyQt5/Kvaser tester for the Ecu8TR BMU ADBMS/LTC measurement path."""

import argparse
import queue
import struct
import sys
import threading
import time

import can
from PyQt5 import QtCore, QtWidgets


ID_START_LTC = 0x10088801
ID_BALANCE_CELLS = 0x12181801
ID_CELL_FIRST = 0x10000C01
ID_CELL_LAST = 0x10000C14
ID_M001_TERMINAL_TEMPS = 0x10001801
ID_M001_MODULE_STATUS = 0x10002801
ID_M001_MAX_MIN_VOLTAGES = 0x10003801
ID_M001_IC_INTERNAL_TEMP = 0x10008801
ID_M001_IC_CELL_SUM = 0x1000C801
ID_BMMP_CELL_VOLTAGES = 0x1E000001
ID_BMMP_CELL_TEMPS = 0x1E001001
ID_BMMP_TERMINAL_TEMPS = 0x1E002001

START_MAGIC = 0xA83C95
START_ACTION = 0x04


def u16(data, offset):
    return data[offset] | (data[offset + 1] << 8)


def s16(data, offset):
    return struct.unpack_from("<h", bytes(data), offset)[0]


class CanWorker(QtCore.QObject):
    message_received = QtCore.pyqtSignal(object)
    status_changed = QtCore.pyqtSignal(str)

    def __init__(self, channel, bitrate):
        super().__init__()
        self.channel = channel
        self.bitrate = bitrate
        self.bus = None
        self.rx_thread = None
        self.running = False
        self.tx_queue = queue.Queue()

    def start(self):
        try:
            self.bus = can.Bus(interface="kvaser", channel=self.channel, bitrate=self.bitrate)
        except Exception as exc:
            self.status_changed.emit(f"CAN open failed: {exc}")
            return

        self.running = True
        self.rx_thread = threading.Thread(target=self._run, daemon=True)
        self.rx_thread.start()
        self.status_changed.emit(f"Kvaser channel {self.channel}, {self.bitrate} bit/s")

    def stop(self):
        self.running = False
        if self.rx_thread is not None:
            self.rx_thread.join(timeout=1.0)
        if self.bus is not None:
            self.bus.shutdown()
            self.bus = None

    def send_start(self):
        payload = bytearray(8)
        payload[0] = START_MAGIC & 0xFF
        payload[1] = (START_MAGIC >> 8) & 0xFF
        payload[2] = (START_MAGIC >> 16) & 0xFF
        payload[3] = START_ACTION
        self._send(ID_START_LTC, payload)

    def send_balance_mask(self, mask):
        payload = bytearray(8)
        payload[0:4] = int(mask & 0x000FFFFF).to_bytes(4, "little")
        self._send(ID_BALANCE_CELLS, payload)

    def _send(self, arbitration_id, payload):
        if self.bus is None:
            self.status_changed.emit("CAN bus is not open")
            return
        msg = can.Message(arbitration_id=arbitration_id, is_extended_id=True, data=bytes(payload))
        try:
            self.bus.send(msg, timeout=0.2)
        except Exception as exc:
            self.status_changed.emit(f"TX failed 0x{arbitration_id:08X}: {exc}")

    def _run(self):
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
    def __init__(self, worker):
        super().__init__()
        self.worker = worker
        self.cell_voltage_labels = []
        self.cell_gpio_labels = []
        self.cell_balance_rx = []
        self.balance_checks = []
        self.temp_labels = {}
        self.rx_count = 0
        self.rx_count_label = None
        self.setWindowTitle("Ecu8TR BMU ADBMS/LTC Tester")
        self._build_ui()

        self.worker.message_received.connect(self._handle_can)
        self.worker.status_changed.connect(self.statusBar().showMessage)

    def _build_ui(self):
        root = QtWidgets.QWidget()
        layout = QtWidgets.QVBoxLayout(root)
        layout.setContentsMargins(16, 16, 16, 16)
        layout.setSpacing(12)

        self.setStyleSheet("""
            QWidget {
                background: #f5f7fb;
                color: #1f2937;
                font-family: Segoe UI, Arial;
                font-size: 10pt;
            }
            QGroupBox {
                background: #ffffff;
                border: 1px solid #d8dee9;
                border-radius: 6px;
                margin-top: 14px;
                padding: 10px;
                font-weight: 600;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 4px;
                color: #334155;
            }
            QPushButton {
                background: #2563eb;
                color: white;
                border: 0;
                border-radius: 5px;
                padding: 8px 13px;
                font-weight: 600;
            }
            QPushButton:hover { background: #1d4ed8; }
            QPushButton#secondaryButton {
                background: #e5e7eb;
                color: #111827;
            }
            QPushButton#secondaryButton:hover { background: #d1d5db; }
            QLabel#metricValue {
                background: #f8fafc;
                border: 1px solid #e2e8f0;
                border-radius: 4px;
                padding: 5px 8px;
                font-family: Consolas, Segoe UI;
                font-size: 10pt;
            }
            QLabel#smallStatus {
                color: #475569;
                font-family: Consolas, Segoe UI;
            }
            QTableWidget {
                background: #ffffff;
                alternate-background-color: #f8fafc;
                gridline-color: #e5e7eb;
                border: 1px solid #d8dee9;
                border-radius: 6px;
                selection-background-color: #dbeafe;
            }
            QHeaderView::section {
                background: #eef2f7;
                color: #334155;
                border: 0;
                border-right: 1px solid #d8dee9;
                padding: 6px;
                font-weight: 600;
            }
            QCheckBox { background: transparent; }
        """)

        top = QtWidgets.QHBoxLayout()
        start_btn = QtWidgets.QPushButton("Start measurement")
        start_btn.clicked.connect(self.worker.send_start)
        send_balance_btn = QtWidgets.QPushButton("Send balance flags")
        send_balance_btn.clicked.connect(self._send_balance)
        clear_balance_btn = QtWidgets.QPushButton("Clear balance")
        clear_balance_btn.setObjectName("secondaryButton")
        clear_balance_btn.clicked.connect(self._clear_balance)
        self.rx_count_label = QtWidgets.QLabel("RX frames: 0")
        self.rx_count_label.setObjectName("smallStatus")
        top.addWidget(start_btn)
        top.addWidget(send_balance_btn)
        top.addWidget(clear_balance_btn)
        top.addStretch()
        top.addWidget(self.rx_count_label)
        layout.addLayout(top)

        main_split = QtWidgets.QHBoxLayout()
        main_split.setSpacing(12)
        layout.addLayout(main_split, 1)

        cell_group = QtWidgets.QGroupBox("Cell Messages")
        cell_layout = QtWidgets.QVBoxLayout(cell_group)
        cell_table = QtWidgets.QTableWidget(20, 5)
        cell_table.setHorizontalHeaderLabels(["Cell", "Voltage mV", "GPIO mV", "AFE balance", "Command"])
        cell_table.verticalHeader().setVisible(False)
        cell_table.setAlternatingRowColors(True)
        cell_table.setEditTriggers(QtWidgets.QAbstractItemView.NoEditTriggers)
        cell_table.setSelectionMode(QtWidgets.QAbstractItemView.NoSelection)
        cell_table.horizontalHeader().setSectionResizeMode(0, QtWidgets.QHeaderView.ResizeToContents)
        cell_table.horizontalHeader().setSectionResizeMode(1, QtWidgets.QHeaderView.Stretch)
        cell_table.horizontalHeader().setSectionResizeMode(2, QtWidgets.QHeaderView.Stretch)
        cell_table.horizontalHeader().setSectionResizeMode(3, QtWidgets.QHeaderView.Stretch)
        cell_table.horizontalHeader().setSectionResizeMode(4, QtWidgets.QHeaderView.ResizeToContents)
        cell_table.setMinimumWidth(440)

        for i in range(20):
            cell_item = QtWidgets.QTableWidgetItem(f"{i + 1:02d}")
            v = QtWidgets.QTableWidgetItem("-")
            gpio = QtWidgets.QTableWidgetItem("-")
            rx_bal = QtWidgets.QTableWidgetItem("-")
            cell_item.setTextAlignment(QtCore.Qt.AlignCenter)
            v.setTextAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)
            gpio.setTextAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)
            rx_bal.setTextAlignment(QtCore.Qt.AlignCenter)
            chk = QtWidgets.QCheckBox()
            chk_box = QtWidgets.QWidget()
            chk_layout = QtWidgets.QHBoxLayout(chk_box)
            chk_layout.setContentsMargins(0, 0, 0, 0)
            chk_layout.addWidget(chk, 0, QtCore.Qt.AlignCenter)
            self.cell_voltage_labels.append(v)
            self.cell_gpio_labels.append(gpio)
            self.cell_balance_rx.append(rx_bal)
            self.balance_checks.append(chk)
            cell_table.setItem(i, 0, cell_item)
            cell_table.setItem(i, 1, v)
            cell_table.setItem(i, 2, gpio)
            cell_table.setItem(i, 3, rx_bal)
            cell_table.setCellWidget(i, 4, chk_box)

        cell_layout.addWidget(cell_table)
        main_split.addWidget(cell_group, 2)

        metrics_panel = QtWidgets.QVBoxLayout()
        metrics_panel.setSpacing(12)
        main_split.addLayout(metrics_panel, 1)

        metrics_panel.addWidget(self._make_metric_group("Voltage Summary", [
            "Module voltage V",
            "M001 cell min mV", "M001 cell max mV", "M001 cell avg mV",
            "BMMp cell min mV", "BMMp cell max mV", "BMMp cell avg mV",
            "BMMp max cell number", "BMMp min cell number",
        ]))
        metrics_panel.addWidget(self._make_metric_group("Temperatures", [
            "Cell min C", "Cell max C", "Cell avg C",
            "Cell max temp number", "Cell min temp number",
            "Terminal neg C", "Terminal pos C",
            "Terminal min C", "Terminal max C", "Terminal avg C",
            "Terminal max number", "Terminal min number",
        ]))
        metrics_panel.addWidget(self._make_metric_group("Sensors and IC", [
            "Dew sensor mV",
            "IC1 sum V", "IC2 sum V",
            "IC1 temp C", "IC2 temp C",
        ]))
        metrics_panel.addStretch()

        self.setCentralWidget(root)
        self.resize(980, 760)

    def _make_metric_group(self, title, names):
        group = QtWidgets.QGroupBox(title)
        grid = QtWidgets.QGridLayout(group)
        grid.setColumnStretch(1, 1)

        for row, name in enumerate(names):
            title_label = QtWidgets.QLabel(name)
            label = QtWidgets.QLabel("-")
            label.setObjectName("metricValue")
            label.setMinimumWidth(90)
            label.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)
            self.temp_labels[name] = label
            grid.addWidget(title_label, row, 0)
            grid.addWidget(label, row, 1)

        return group

    def _balance_mask(self):
        mask = 0
        for i, chk in enumerate(self.balance_checks):
            if chk.isChecked():
                mask |= 1 << i
        return mask

    def _send_balance(self):
        self.worker.send_balance_mask(self._balance_mask())

    def _clear_balance(self):
        for chk in self.balance_checks:
            chk.setChecked(False)
        self.worker.send_balance_mask(0)

    def _handle_can(self, msg):
        data = bytes(msg.data).ljust(8, b"\x00")
        can_id = msg.arbitration_id
        self.rx_count += 1
        if self.rx_count_label is not None:
            self.rx_count_label.setText(f"RX frames: {self.rx_count}")

        if ID_CELL_FIRST <= can_id <= ID_CELL_LAST:
            idx = can_id - ID_CELL_FIRST
            self.cell_voltage_labels[idx].setText(f"{u16(data, 0) * 0.1:.1f}")
            self.cell_gpio_labels[idx].setText(f"{u16(data, 4) * 0.1:.1f}")
            self.cell_balance_rx[idx].setText("ON" if (data[6] & 0x01) else "off")
        elif can_id == ID_M001_TERMINAL_TEMPS:
            self.temp_labels["Dew sensor mV"].setText(f"{u16(data, 2) * 0.1:.1f}")
            self.temp_labels["Terminal neg C"].setText(f"{s16(data, 4) * 0.01:.2f}")
            self.temp_labels["Terminal pos C"].setText(f"{s16(data, 6) * 0.01:.2f}")
        elif can_id == ID_M001_MODULE_STATUS:
            self.temp_labels["Module voltage V"].setText(f"{u16(data, 6) * 0.01:.2f}")
        elif can_id == ID_M001_MAX_MIN_VOLTAGES:
            self.temp_labels["M001 cell min mV"].setText(f"{u16(data, 0) * 0.1:.1f}")
            self.temp_labels["M001 cell max mV"].setText(f"{u16(data, 2) * 0.1:.1f}")
            self.temp_labels["M001 cell avg mV"].setText(f"{u16(data, 4) * 0.1:.1f}")
        elif can_id == ID_BMMP_CELL_VOLTAGES:
            self.temp_labels["BMMp cell max mV"].setText(f"{u16(data, 0)}")
            self.temp_labels["BMMp cell min mV"].setText(f"{u16(data, 2)}")
            self.temp_labels["BMMp cell avg mV"].setText(f"{u16(data, 4)}")
            self.temp_labels["BMMp max cell number"].setText(f"{data[6]}")
            self.temp_labels["BMMp min cell number"].setText(f"{data[7]}")
        elif can_id == ID_M001_IC_CELL_SUM:
            self.temp_labels["IC1 sum V"].setText(f"{u16(data, 0) * 0.01:.2f}")
            self.temp_labels["IC2 sum V"].setText(f"{u16(data, 2) * 0.01:.2f}")
        elif can_id == ID_M001_IC_INTERNAL_TEMP:
            self.temp_labels["IC1 temp C"].setText(f"{s16(data, 0) * 0.01:.2f}")
            self.temp_labels["IC2 temp C"].setText(f"{s16(data, 2) * 0.01:.2f}")
        elif can_id == ID_BMMP_CELL_TEMPS:
            self.temp_labels["Cell max C"].setText(f"{s16(data, 0) * 0.1:.1f}")
            self.temp_labels["Cell min C"].setText(f"{s16(data, 2) * 0.1:.1f}")
            self.temp_labels["Cell avg C"].setText(f"{s16(data, 4) * 0.1:.1f}")
            self.temp_labels["Cell max temp number"].setText(f"{data[6]}")
            self.temp_labels["Cell min temp number"].setText(f"{data[7]}")
        elif can_id == ID_BMMP_TERMINAL_TEMPS:
            self.temp_labels["Terminal max C"].setText(f"{s16(data, 0) * 0.1:.1f}")
            self.temp_labels["Terminal min C"].setText(f"{s16(data, 2) * 0.1:.1f}")
            self.temp_labels["Terminal avg C"].setText(f"{s16(data, 4) * 0.1:.1f}")
            self.temp_labels["Terminal max number"].setText(f"{data[6]}")
            self.temp_labels["Terminal min number"].setText(f"{data[7]}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--channel", type=int, default=0, help="Kvaser channel index")
    parser.add_argument("--bitrate", type=int, default=500000, help="CAN bitrate")
    args = parser.parse_args()

    app = QtWidgets.QApplication(sys.argv)
    worker = CanWorker(args.channel, args.bitrate)
    window = MainWindow(worker)
    window.show()
    worker.start()
    rc = app.exec_()
    worker.stop()
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
