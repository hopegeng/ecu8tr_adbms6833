#!/usr/bin/env python3
"""GEN 3.0 CSC EEPROM Tester -- PyQt5 / Kvaser.

Tests ID ODP/ID Backup write, Static Area 1 & 2 read/write,
CRC16/CRC32 calculation and write, and hardware ODP lock via
M001_WriteStartLTCTransmission.

Usage:
    python eeprom_tester.py [--channel 0] [--bitrate 500000]
"""

import argparse
import struct
import sys
import threading
import time
import zlib

import can
from PyQt5 import QtCore, QtGui, QtWidgets

# -- CAN IDs ------------------------------------------------------------------
ID_EEPROM_DATA  = 0x10007801  # M001_EEPROMData            BMU -> PC
ID_WRITE_EEPROM = 0x10087801  # M001_WriteEEPROMData        PC -> BMU
ID_START_LTC    = 0x10088801  # M001_StartLTCTransmission   PC -> BMU
ID_WRITE_START  = 0x10088800  # M001_WriteStartLTCTransmission (start + lock ODP)
ID_CONFIG_MSG   = 0x1808D801  # M001_ConfigurationMessage   PC -> BMU
ID_SAFETY_MECH  = 0x10010801  # M001_ModuleSafetyMechanisms BMU -> PC

# -- Mux constants for M001_ConfigurationMessage ------------------------------
MUX_CELL_TYPE   = 0xCE11AA55  # -> writes ODP 0x02-0x03 + ID Backup
MUX_MODULE_TYPE = 0xADADAA55  # -> writes ODP 0x04-0x05 + ID Backup
MUX_PCB_TYPE    = 0x11CEAA55  # -> writes ODP 0x0E-0x0F + ID Backup
MUX_IC_TYPE     = 0xCEA1AA55  # -> writes Static Area 1 (0x001E) AND Static Area 2 (0x00BE)
MUX_SERIAL_LO   = 0xD0D0AA55  # -> writes ODP 0x06-0x09 + ID Backup
MUX_SERIAL_HI   = 0xDADAAA55  # -> writes ODP 0x0A-0x0D + ID Backup
MUX_READ        = 0xEE5200    # -> triggers EEPROM read-back (M001_EEPROMData response)

# -- M001_WriteStartLTCTransmission magic -------------------------------------
START_MAGIC = 0x953CA8

# -- Named EEPROM read indices (byte address = index x 4) --------------------
# ID Backup area 0x0000-0x000F (same field layout as ODP page)
IDX_ID_0 = 0   # 0x0000: [CRC16_lo, CRC16_hi, CellType_maj, CellType_min]
IDX_ID_1 = 1   # 0x0004: [ModType_maj, ModType_min, Serial[0], Serial[1]]
IDX_ID_2 = 2   # 0x0008: [Serial[2],   Serial[3],   Serial[4], Serial[5]]
IDX_ID_3 = 3   # 0x000C: [Serial[6],   Serial[7],   PcbType_maj, PcbType_min]
# Static Area 1  0x0010-0x00AF
IDX_S1_0 = 4   # 0x0010: [CRC32 byte 0..3]
IDX_S1_1 = 5   # 0x0014: [SerCells, ParCells, PcbSer[0], PcbSer[1]]
IDX_S1_2 = 6   # 0x0018: [PcbSer[2], PcbSer[3], PcbSer[4], PcbSer[5]]
IDX_S1_3 = 7   # 0x001C: [PcbSer[6], PcbSer[7], IcType_maj, IcType_min]
# Static Area 2  0x00B0-0x014F  (mirror)
IDX_S2_0 = 44  # 0x00B0
IDX_S2_1 = 45  # 0x00B4
IDX_S2_2 = 46  # 0x00B8
IDX_S2_3 = 47  # 0x00BC

# -- Quick-reference rows shown in the Raw tab --------------------------------
ADDR_REFERENCE = [
    (0,  "0x0000", "ID Backup: [CRC16_lo, CRC16_hi, CellType_maj, CellType_min]"),
    (1,  "0x0004", "ID Backup: [ModType_maj, ModType_min, Serial[0], Serial[1]]"),
    (2,  "0x0008", "ID Backup: [Serial[2..5]]"),
    (3,  "0x000C", "ID Backup: [Serial[6], Serial[7], PcbType_maj, PcbType_min]"),
    (4,  "0x0010", "Static1: CRC32 [0..3]"),
    (5,  "0x0014", "Static1: [SerialCells, ParallelCells, PcbSerial[0], PcbSerial[1]]"),
    (6,  "0x0018", "Static1: [PcbSerial[2..5]]"),
    (7,  "0x001C", "Static1: [PcbSerial[6], PcbSerial[7], IcType_maj, IcType_min]"),
    (10, "0x0028", "Static1: locking_recognition (legacy byte, not HW OTP)"),
    (44, "0x00B0", "Static2 mirror: CRC32 [0..3]"),
    (45, "0x00B4", "Static2 mirror: [SerialCells, ParallelCells, PcbSerial[0], PcbSerial[1]]"),
    (46, "0x00B8", "Static2 mirror: [PcbSerial[2..5]]"),
    (47, "0x00BC", "Static2 mirror: [PcbSerial[6], PcbSerial[7], IcType_maj, IcType_min]"),
    (84, "0x0150", "Dynamic Area 1: start"),
    (168,"0x02A0", "Dynamic Area 2: start"),
]

# -- CRC functions ------------------------------------------------------------

def _crc16_ac9a(data: bytes, init: int = 0x0000) -> int:
    """CRC-16 with polynomial 0xAC9A (non-reflected / MSB-first left-shift).

    Polynomial source: BorgWarner requirement AK.UHE.SW.0053
    (HD=5 at 241 bits, per GEN3.0 EEPROM layout requirements document).

    Coverage: 14 bytes at ODP offsets 0x02-0x0F (CellType through PcbType).
    Stored  : ODP 0x00-0x01 and ID Backup idx 0 bytes [0]=lo [1]=hi.

    The init value and XOR-out are NOT documented in the spec; the two most
    likely variants are init=0x0000 (simple) and init=0xFFFF (seeded).
    Verify against a known-good device to confirm which is correct.
    """
    crc = init & 0xFFFF
    for byte in data:
        crc ^= (byte & 0xFF) << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0xAC9A) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def _crc32_ecu8tr(data: bytes) -> int:
    """CRC-32/ISO-HDLC: poly=0xEDB88320, init=0xFFFFFFFF, reflected, XOR=0xFFFFFFFF.

    Confirmed from firmware src/tools/tools.h (crc32() function) and from
    zlib.crc32 usage in test scripts (fw_upgrade.py, ecu8trMain.py).

    Coverage: 156 bytes at main-array offsets 0x0014-0x00AF (Static Area 1 data).
    Stored  : first 4 bytes of each area -- idx 4 (0x0010) for Static1,
              idx 44 (0x00B0) for Static2 -- little-endian.
    """
    return zlib.crc32(data) & 0xFFFFFFFF


# -- Annotation map for M001_EEPROMData responses ----------------------------
def _annotate(addr, d):
    if addr == 0x0000:
        return f"CRC16=0x{d[1]:02X}{d[0]:02X}  CellType={d[2]:02X}.{d[3]:02X}"
    if addr == 0x0004:
        return f"ModType={d[0]:02X}.{d[1]:02X}  Serial[0]={d[2]:02X} Serial[1]={d[3]:02X}"
    if addr == 0x0008:
        return f"Serial[2..5]={d[0]:02X}{d[1]:02X}{d[2]:02X}{d[3]:02X}"
    if addr == 0x000C:
        return f"Serial[6]={d[0]:02X} Serial[7]={d[1]:02X}  PcbType={d[2]:02X}.{d[3]:02X}"
    if addr in (0x0010, 0x00B0):
        area = "Static1" if addr == 0x0010 else "Static2"
        return f"{area} CRC32=0x{d[3]:02X}{d[2]:02X}{d[1]:02X}{d[0]:02X}"
    if addr in (0x0014, 0x00B4):
        return f"SerCells={d[0]}  ParCells={d[1]}  PcbSer[0]={d[2]:02X}  PcbSer[1]={d[3]:02X}"
    if addr in (0x0018, 0x00B8):
        return f"PcbSer[2..5]={d[0]:02X}{d[1]:02X}{d[2]:02X}{d[3]:02X}"
    if addr in (0x001C, 0x00BC):
        return f"PcbSer[6]={d[0]:02X}  PcbSer[7]={d[1]:02X}  IcType={d[2]:02X}.{d[3]:02X}"
    return ""


# ---------------------------------------------------------------------------
# CAN worker (runs RX on a daemon thread, TX inline)
# ---------------------------------------------------------------------------
class CanWorker(QtCore.QObject):
    message_received = QtCore.pyqtSignal(object)
    status_changed   = QtCore.pyqtSignal(str)

    def __init__(self, channel, bitrate):
        super().__init__()
        self.channel  = channel
        self.bitrate  = bitrate
        self.bus      = None
        self._thread  = None
        self._running = False

    def start(self):
        try:
            self.bus = can.Bus(interface="kvaser", channel=self.channel,
                               bitrate=self.bitrate)
        except Exception as exc:
            self.status_changed.emit(f"CAN open failed: {exc}")
            return
        self._running = True
        self._thread = threading.Thread(target=self._rx_loop, daemon=True)
        self._thread.start()
        self.status_changed.emit(
            f"Kvaser ch{self.channel}  {self.bitrate} bit/s  connected")

    def stop(self):
        self._running = False
        if self._thread:
            self._thread.join(timeout=1.0)
        if self.bus:
            self.bus.shutdown()
            self.bus = None

    # -- TX -------------------------------------------------------------------

    def _send(self, can_id, payload):
        if self.bus is None:
            self.status_changed.emit("CAN not open")
            return
        data = (bytes(payload) + b"\x00" * 8)[:8]
        msg  = can.Message(arbitration_id=can_id, is_extended_id=True, data=data)
        try:
            self.bus.send(msg, timeout=0.2)
        except Exception as exc:
            self.status_changed.emit(f"TX error 0x{can_id:08X}: {exc}")

    def send_config(self, mux, major, minor):
        """M001_ConfigurationMessage -- major/minor in bytes 4-5."""
        payload = struct.pack("<IBB", mux, major & 0xFF, minor & 0xFF)
        self._send(ID_CONFIG_MSG, payload)

    def send_config_4bytes(self, mux, data4):
        """M001_ConfigurationMessage -- raw 4-byte payload at bytes 4-7."""
        payload = struct.pack("<I", mux) + bytes(data4[:4])
        self._send(ID_CONFIG_MSG, payload)

    def send_eeprom_write(self, index, data32):
        """M001_WriteEEPROMData -- write 4 bytes at address = index x 4."""
        payload = bytes([0x00, 0x00, 0x00, index & 0xFF]) + \
                  struct.pack("<I", data32 & 0xFFFFFFFF)
        self._send(ID_WRITE_EEPROM, payload)

    def send_eeprom_read(self, index):
        """M001_WriteEEPROMData with read-mux -- triggers M001_EEPROMData response."""
        m = MUX_READ
        payload = bytes([m & 0xFF, (m >> 8) & 0xFF, (m >> 16) & 0xFF,
                         index & 0xFF, 0x00, 0x00, 0x00, 0x00])
        self._send(ID_WRITE_EEPROM, payload)

    def send_start_ltc(self):
        """M001_StartLTCTransmission -- start measurement, no lock."""
        m = START_MAGIC
        payload = bytes([m & 0xFF, (m >> 8) & 0xFF, (m >> 16) & 0xFF, 0x04])
        self._send(ID_START_LTC, payload)

    def send_lock_odp(self):
        """M001_WriteStartLTCTransmission -- start + IRREVERSIBLY lock ODP page."""
        m = START_MAGIC
        payload = bytes([m & 0xFF, (m >> 8) & 0xFF, (m >> 16) & 0xFF])
        self._send(ID_WRITE_START, payload)

    # -- RX -------------------------------------------------------------------

    def _rx_loop(self):
        while self._running:
            try:
                msg = self.bus.recv(timeout=0.1)
            except Exception as exc:
                self.status_changed.emit(f"RX error: {exc}")
                time.sleep(0.5)
                continue
            if msg is not None:
                self.message_received.emit(msg)


# ---------------------------------------------------------------------------
# Shared style
# ---------------------------------------------------------------------------
STYLE = """
QWidget { background:#f5f7fb; color:#1f2937;
          font-family:"Segoe UI",Arial; font-size:10pt; }
QGroupBox { background:#ffffff; border:1px solid #d8dee9;
            border-radius:6px; margin-top:14px; padding:10px;
            font-weight:600; }
QGroupBox::title { subcontrol-origin:margin; left:10px;
                   padding:0 4px; color:#334155; }
QGroupBox#crc { background:#f0fdf4; border:1px solid #86efac; }
QGroupBox#crc::title { color:#166534; }
QTabWidget::pane { border:1px solid #d8dee9; background:#ffffff;
                   border-radius:0 6px 6px 6px; }
QTabBar::tab { background:#e5e7eb; color:#374151;
               padding:6px 16px; margin-right:2px;
               border-radius:5px 5px 0 0; }
QTabBar::tab:selected { background:#2563eb; color:#ffffff; }
QPushButton { background:#2563eb; color:white; border:0;
              border-radius:5px; padding:6px 14px; font-weight:600; }
QPushButton:hover { background:#1d4ed8; }
QPushButton:disabled { background:#9ca3af; }
QPushButton#sec { background:#e5e7eb; color:#111827; }
QPushButton#sec:hover { background:#d1d5db; }
QPushButton#warn { background:#dc2626; }
QPushButton#warn:hover { background:#b91c1c; }
QPushButton#green { background:#16a34a; }
QPushButton#green:hover { background:#15803d; }
QSpinBox, QLineEdit { border:1px solid #d8dee9; border-radius:4px;
                      padding:4px 6px; background:#ffffff; }
QLabel#mono { font-family:Consolas,"Courier New"; font-size:9pt;
              background:#f1f5f9; border:1px solid #e2e8f0;
              border-radius:4px; padding:4px 8px; }
QLabel#crc_result { font-family:Consolas,"Courier New"; font-size:10pt;
                    color:#166534; font-weight:700;
                    background:#dcfce7; border:1px solid #86efac;
                    border-radius:4px; padding:4px 10px; }
QLabel#crc_pending { font-family:Consolas,"Courier New"; font-size:10pt;
                     color:#6b7280; background:#f1f5f9;
                     border:1px solid #e2e8f0; border-radius:4px;
                     padding:4px 10px; }
QLabel#locked   { color:#dc2626; font-weight:700; font-size:11pt; }
QLabel#unlocked { color:#16a34a; font-weight:700; font-size:11pt; }
QLabel#unknown  { color:#6b7280; font-size:11pt; }
QTableWidget { background:#ffffff; alternate-background-color:#f8fafc;
               gridline-color:#e5e7eb; border:1px solid #d8dee9;
               border-radius:6px; }
QHeaderView::section { background:#eef2f7; color:#334155; border:0;
                       border-right:1px solid #d8dee9;
                       padding:5px; font-weight:600; }
"""


# ---------------------------------------------------------------------------
# Helper widgets
# ---------------------------------------------------------------------------
def _hex_spinbox(lo=0, hi=0xFF):
    sb = QtWidgets.QSpinBox()
    sb.setRange(lo, hi)
    sb.setFixedWidth(78)
    sb.setDisplayIntegerBase(16)
    sb.setPrefix("0x")
    return sb


def _dec_spinbox(lo=0, hi=255):
    sb = QtWidgets.QSpinBox()
    sb.setRange(lo, hi)
    sb.setFixedWidth(72)
    return sb


def _hexline(placeholder="00000000", width=110):
    le = QtWidgets.QLineEdit(placeholder)
    le.setFixedWidth(width)
    le.setMaxLength(8)
    le.setPlaceholderText("XXXXXXXX")
    return le


def _parse_hex32(le):
    """Return int from a hex QLineEdit, or None and colour the border red."""
    try:
        val = int(le.text().strip() or "0", 16) & 0xFFFFFFFF
        le.setStyleSheet("")
        return val
    except ValueError:
        le.setStyleSheet("border:1px solid #ef4444;")
        return None


# ---------------------------------------------------------------------------
# Main window
# ---------------------------------------------------------------------------
class EepromTesterWindow(QtWidgets.QMainWindow):

    def __init__(self, worker):
        super().__init__()
        self.worker    = worker
        self._rx_count = 0

        # EEPROM read-back cache: index (int) -> bytes(4)
        self._eeprom_cache: dict = {}

        # CRC computation state
        self._crc16_computed: int = -1   # -1 = not yet calculated
        self._crc32_computed: int = -1

        # Widget refs set in _build_id_tab / _build_static_tab
        self._crc16_lbl       = None
        self._crc16_cache_lbl = None
        self._crc16_write_btn = None
        self._crc32_lbl       = None
        self._crc32_cache_lbl = None
        self._crc32_write_btn = None

        self.setWindowTitle("GEN 3.0 CSC EEPROM Tester")
        self.setStyleSheet(STYLE)
        self._build_ui()

        worker.message_received.connect(self._handle_can)
        worker.status_changed.connect(self.statusBar().showMessage)

    # -- top-level layout -----------------------------------------------------

    def _build_ui(self):
        root = QtWidgets.QWidget()
        vlay = QtWidgets.QVBoxLayout(root)
        vlay.setContentsMargins(12, 12, 12, 12)
        vlay.setSpacing(8)

        top = QtWidgets.QHBoxLayout()
        btn_start = QtWidgets.QPushButton("Start measurement")
        btn_start.clicked.connect(self.worker.send_start_ltc)
        self._rx_lbl = QtWidgets.QLabel("RX: 0")
        top.addWidget(btn_start)
        top.addStretch()
        top.addWidget(self._rx_lbl)
        vlay.addLayout(top)

        tabs = QtWidgets.QTabWidget()
        tabs.addTab(self._build_id_tab(),     "ID ODP / Backup")
        tabs.addTab(self._build_static_tab(), "Static Areas")
        tabs.addTab(self._build_lock_tab(),   "ODP Lock")
        tabs.addTab(self._build_raw_tab(),    "Raw EEPROM")
        vlay.addWidget(tabs, 1)

        resp_grp = QtWidgets.QGroupBox(
            "Last M001_EEPROMData response  (0x10007801)")
        rl = QtWidgets.QHBoxLayout(resp_grp)
        self._resp_lbl = QtWidgets.QLabel("--")
        self._resp_lbl.setObjectName("mono")
        self._resp_lbl.setWordWrap(True)
        rl.addWidget(self._resp_lbl)
        vlay.addWidget(resp_grp)

        self.setCentralWidget(root)
        self.resize(980, 760)

    # -- Tab 1: ID ODP / Backup -----------------------------------------------

    def _build_id_tab(self):
        w   = QtWidgets.QWidget()
        lay = QtWidgets.QVBoxLayout(w)
        lay.setSpacing(8)

        info = QtWidgets.QLabel(
            "Each <b>Write</b> sends <code>M001_ConfigurationMessage (0x1808D801)</code>. "
            "Firmware writes to the M24C08 ODP ID page (I²C 0xB0) <em>and</em> "
            "the ID Backup region at 0x0000–0x000F in the main array simultaneously."
        )
        info.setWordWrap(True)
        lay.addWidget(info)

        # -- write form -------------------------------------------------------
        form = QtWidgets.QGroupBox(
            "Write ID fields  (ODP page + ID Backup in main array at 0x0000–0x000F)")
        g = QtWidgets.QGridLayout(form)
        g.setColumnStretch(3, 1)

        def hdr(col, text):
            l = QtWidgets.QLabel(f"<b>{text}</b>")
            g.addWidget(l, 0, col)

        hdr(0, "Field")
        hdr(1, "Major / Bytes [0:3]")
        hdr(2, "Minor")
        hdr(3, "")

        row = 1

        # Cell Type
        self._ct_maj = _hex_spinbox()
        self._ct_min = _hex_spinbox()
        g.addWidget(QtWidgets.QLabel("Cell Type  (ODP offset 0x02–0x03)"), row, 0)
        g.addWidget(self._ct_maj, row, 1)
        g.addWidget(self._ct_min, row, 2)
        b = QtWidgets.QPushButton("Write Cell Type")
        b.clicked.connect(lambda: self._write_config(
            MUX_CELL_TYPE, self._ct_maj.value(), self._ct_min.value(), "Cell Type"))
        g.addWidget(b, row, 3)
        row += 1

        # Module Type
        self._mt_maj = _hex_spinbox()
        self._mt_min = _hex_spinbox()
        g.addWidget(QtWidgets.QLabel("Module Type  (ODP offset 0x04–0x05)"), row, 0)
        g.addWidget(self._mt_maj, row, 1)
        g.addWidget(self._mt_min, row, 2)
        b2 = QtWidgets.QPushButton("Write Module Type")
        b2.clicked.connect(lambda: self._write_config(
            MUX_MODULE_TYPE, self._mt_maj.value(), self._mt_min.value(), "Module Type"))
        g.addWidget(b2, row, 3)
        row += 1

        # PCB Type
        self._pbt_maj = _hex_spinbox()
        self._pbt_min = _hex_spinbox()
        g.addWidget(QtWidgets.QLabel("PCB Type  (ODP offset 0x0E–0x0F)"), row, 0)
        g.addWidget(self._pbt_maj, row, 1)
        g.addWidget(self._pbt_min, row, 2)
        b3 = QtWidgets.QPushButton("Write PCB Type")
        b3.clicked.connect(lambda: self._write_config(
            MUX_PCB_TYPE, self._pbt_maj.value(), self._pbt_min.value(), "PCB Type"))
        g.addWidget(b3, row, 3)
        row += 1

        # Module Serial LO
        self._ser_lo = _hexline()
        g.addWidget(QtWidgets.QLabel("Module Serial LO  (ODP 0x06–0x09, hex LE32)"), row, 0)
        g.addWidget(self._ser_lo, row, 1)
        g.addWidget(QtWidgets.QLabel("bytes [0..3]"), row, 2)
        b4 = QtWidgets.QPushButton("Write Serial LO")
        b4.clicked.connect(self._write_serial_lo)
        g.addWidget(b4, row, 3)
        row += 1

        # Module Serial HI
        self._ser_hi = _hexline()
        g.addWidget(QtWidgets.QLabel("Module Serial HI  (ODP 0x0A–0x0D, hex LE32)"), row, 0)
        g.addWidget(self._ser_hi, row, 1)
        g.addWidget(QtWidgets.QLabel("bytes [4..7]"), row, 2)
        b5 = QtWidgets.QPushButton("Write Serial HI")
        b5.clicked.connect(self._write_serial_hi)
        g.addWidget(b5, row, 3)

        lay.addWidget(form)

        # -- read back --------------------------------------------------------
        read_grp = QtWidgets.QGroupBox(
            "Read back ID Backup from main array  (M001_WriteEEPROMData read-mux)")
        rl = QtWidgets.QHBoxLayout(read_grp)
        read_defs = [
            ("CRC16 + Cell Type  (idx 0)", IDX_ID_0),
            ("ModType + Serial[0:2]  (idx 1)", IDX_ID_1),
            ("Serial[2:6]  (idx 2)", IDX_ID_2),
            ("Serial[6:8] + PCB Type  (idx 3)", IDX_ID_3),
        ]
        for label, idx in read_defs:
            b = QtWidgets.QPushButton(label)
            b.setObjectName("sec")
            b.clicked.connect(lambda _=False, i=idx: self.worker.send_eeprom_read(i))
            rl.addWidget(b)

        lay.addWidget(read_grp)

        # -- CRC16 group ------------------------------------------------------
        crc_grp = QtWidgets.QGroupBox(
            "CRC16 -- ID/Safety Area  (poly 0xAC9A, BorgWarner req. AK.UHE.SW.0053)")
        crc_grp.setObjectName("crc")
        cl = QtWidgets.QGridLayout(crc_grp)
        cl.setColumnStretch(3, 1)

        note = QtWidgets.QLabel(
            "<b>Covers:</b> ODP offsets 0x02–0x0F (14 bytes) "
            "= CellType, ModType, Serial[8], PcbType.<br>"
            "<b>Stored at:</b> ODP 0x00–0x01 and ID Backup idx 0 bytes [0]=lo [1]=hi.<br>"
            "<b>Note:</b> This button writes to <u>ID Backup</u> (main array) only. "
            "Writing to ODP requires a dedicated firmware command (not yet implemented). "
            "Ensure all ID fields are written and read back before calculating."
        )
        note.setWordWrap(True)
        cl.addWidget(note, 0, 0, 1, 4)

        # Step 1: read
        cl.addWidget(QtWidgets.QLabel("<b>Step 1</b> -- Read all ID indices:"), 1, 0)
        btn_read_id = QtWidgets.QPushButton("Read ID Fields (idx 0-3)")
        btn_read_id.setObjectName("sec")
        btn_read_id.clicked.connect(self._read_id_fields)
        cl.addWidget(btn_read_id, 1, 1)
        self._crc16_cache_lbl = QtWidgets.QLabel("Cached: 0/4 ID indices")
        self._crc16_cache_lbl.setStyleSheet("color:#64748b;")
        cl.addWidget(self._crc16_cache_lbl, 1, 2, 1, 2)

        # Step 2: calculate
        cl.addWidget(QtWidgets.QLabel("<b>Step 2</b> -- Calculate CRC16:"), 2, 0)

        init_box = QtWidgets.QHBoxLayout()
        self._crc16_init_0    = QtWidgets.QRadioButton("init=0x0000  (most likely)")
        self._crc16_init_ffff = QtWidgets.QRadioButton("init=0xFFFF  (try if 0x0000 fails)")
        self._crc16_init_0.setChecked(True)
        init_box.addWidget(self._crc16_init_0)
        init_box.addWidget(self._crc16_init_ffff)
        init_box.addStretch()
        cl.addLayout(init_box, 2, 1, 1, 2)

        btn_calc_crc16 = QtWidgets.QPushButton("Calculate CRC16")
        btn_calc_crc16.clicked.connect(self._calc_crc16)
        cl.addWidget(btn_calc_crc16, 2, 3)

        # Step 3: result + write
        cl.addWidget(QtWidgets.QLabel("<b>Step 3</b> -- Write to ID Backup idx 0:"), 3, 0)
        self._crc16_lbl = QtWidgets.QLabel("Not calculated yet")
        self._crc16_lbl.setObjectName("crc_pending")
        cl.addWidget(self._crc16_lbl, 3, 1, 1, 2)

        self._crc16_write_btn = QtWidgets.QPushButton("Write CRC16 to idx 0 (0x0000)")
        self._crc16_write_btn.setObjectName("green")
        self._crc16_write_btn.setEnabled(False)
        self._crc16_write_btn.clicked.connect(self._write_crc16)
        cl.addWidget(self._crc16_write_btn, 3, 3)

        lay.addWidget(crc_grp)
        lay.addStretch()
        return w

    # -- Tab 2: Static Areas --------------------------------------------------

    def _build_static_tab(self):
        w   = QtWidgets.QWidget()
        lay = QtWidgets.QVBoxLayout(w)
        lay.setSpacing(8)

        info = QtWidgets.QLabel(
            "<b>IC Type</b> uses <code>M001_ConfigurationMessage</code> -- firmware mirrors it to "
            "both Static Area 1 (0x001E) and Static Area 2 (0x00BE).<br>"
            "All other fields use <code>M001_WriteEEPROMData</code> (index x 4 = byte address). "
            "Each write is exactly 4 bytes; the byte layout is shown per row.<br>"
            "<b>Note:</b> index 7 (0x001C) packs PcbSerial[6:8] and IC Type together -- "
            "fill both sets of fields and write once to avoid overwriting either."
        )
        info.setWordWrap(True)
        lay.addWidget(info)

        # IC Type via ConfigurationMessage
        ict_grp = QtWidgets.QGroupBox(
            "IC Type -- written to Static Area 1 (0x001E) and Static Area 2 (0x00BE) simultaneously")
        il = QtWidgets.QHBoxLayout(ict_grp)
        il.addWidget(QtWidgets.QLabel("Major:"))
        self._ict_maj = _hex_spinbox()
        il.addWidget(self._ict_maj)
        il.addWidget(QtWidgets.QLabel("Minor:"))
        self._ict_min = _hex_spinbox()
        il.addWidget(self._ict_min)
        b_ict = QtWidgets.QPushButton("Write IC Type (both areas)")
        b_ict.clicked.connect(lambda: self._write_config(
            MUX_IC_TYPE, self._ict_maj.value(), self._ict_min.value(), "IC Type"))
        il.addWidget(b_ict)
        il.addStretch()
        lay.addWidget(ict_grp)

        # Topology -- index 5 (0x0014)
        topo_grp = QtWidgets.QGroupBox(
            "Cell Topology -- index 5 (addr 0x0014):  [SerCells | ParCells | PcbSer[0] | PcbSer[1]]")
        tl = QtWidgets.QHBoxLayout(topo_grp)
        tl.addWidget(QtWidgets.QLabel("Serial Cells:"))
        self._ser_cells = _dec_spinbox()
        tl.addWidget(self._ser_cells)
        tl.addWidget(QtWidgets.QLabel("Parallel Cells:"))
        self._par_cells = _dec_spinbox()
        tl.addWidget(self._par_cells)
        tl.addWidget(QtWidgets.QLabel("PcbSerial[0]:"))
        self._pcb_s0 = _hex_spinbox()
        tl.addWidget(self._pcb_s0)
        tl.addWidget(QtWidgets.QLabel("[1]:"))
        self._pcb_s1 = _hex_spinbox()
        tl.addWidget(self._pcb_s1)
        b_topo = QtWidgets.QPushButton("Write (idx 5 = 0x0014)")
        b_topo.clicked.connect(self._write_static_idx5)
        tl.addWidget(b_topo)
        tl.addStretch()
        lay.addWidget(topo_grp)

        # PCB Serial [2..5] -- index 6 (0x0018)
        pcb6_grp = QtWidgets.QGroupBox(
            "PCB Serial bytes [2..5] -- index 6 (addr 0x0018):  [PcbSer[2] | [3] | [4] | [5]]")
        pl = QtWidgets.QHBoxLayout(pcb6_grp)
        self._pcb_s = [_hex_spinbox() for _ in range(4)]
        for i, sb in enumerate(self._pcb_s):
            pl.addWidget(QtWidgets.QLabel(f"[{i+2}]:"))
            pl.addWidget(sb)
        b_pcb6 = QtWidgets.QPushButton("Write (idx 6 = 0x0018)")
        b_pcb6.clicked.connect(self._write_static_idx6)
        pl.addWidget(b_pcb6)
        pl.addStretch()
        lay.addWidget(pcb6_grp)

        # PCB Serial [6..7] + IC Type -- index 7 (0x001C)
        pcb7_grp = QtWidgets.QGroupBox(
            "PCB Serial [6..7] + IC Type -- index 7 (addr 0x001C):  "
            "[PcbSer[6] | PcbSer[7] | IcType_maj | IcType_min]")
        pl7 = QtWidgets.QHBoxLayout(pcb7_grp)
        self._pcb_s6 = _hex_spinbox()
        self._pcb_s7 = _hex_spinbox()
        self._ict7_maj = _hex_spinbox()
        self._ict7_min = _hex_spinbox()
        for lbl, sb in [("PcbSer[6]:", self._pcb_s6), ("PcbSer[7]:", self._pcb_s7),
                         ("IcType_maj:", self._ict7_maj), ("IcType_min:", self._ict7_min)]:
            pl7.addWidget(QtWidgets.QLabel(lbl))
            pl7.addWidget(sb)
        b_pcb7 = QtWidgets.QPushButton("Write (idx 7 = 0x001C)")
        b_pcb7.clicked.connect(self._write_static_idx7)
        pl7.addWidget(b_pcb7)
        pl7.addStretch()
        lay.addWidget(pcb7_grp)

        # Read buttons
        read_grp = QtWidgets.QGroupBox(
            "Read back -- verify Static Area 2 mirrors Static Area 1")
        rl = QtWidgets.QHBoxLayout(read_grp)

        for title, pairs in [
            ("Static Area 1", [
                ("CRC32  (idx 4 = 0x0010)", IDX_S1_0),
                ("Cells+PcbSer[0:2]  (idx 5 = 0x0014)", IDX_S1_1),
                ("PcbSer[2:6]  (idx 6 = 0x0018)", IDX_S1_2),
                ("PcbSer[6:8]+IcT  (idx 7 = 0x001C)", IDX_S1_3),
            ]),
            ("Static Area 2  (mirror)", [
                ("CRC32  (idx 44 = 0x00B0)", IDX_S2_0),
                ("Cells+PcbSer[0:2]  (idx 45 = 0x00B4)", IDX_S2_1),
                ("PcbSer[2:6]  (idx 46 = 0x00B8)", IDX_S2_2),
                ("PcbSer[6:8]+IcT  (idx 47 = 0x00BC)", IDX_S2_3),
            ]),
        ]:
            col = QtWidgets.QVBoxLayout()
            col.addWidget(QtWidgets.QLabel(f"<b>{title}</b>"))
            for label, idx in pairs:
                b = QtWidgets.QPushButton(label)
                b.setObjectName("sec")
                b.clicked.connect(lambda _=False, i=idx: self.worker.send_eeprom_read(i))
                col.addWidget(b)
            rl.addLayout(col)

        lay.addWidget(read_grp)

        # -- CRC32 group ------------------------------------------------------
        crc_grp = QtWidgets.QGroupBox(
            "CRC32 -- Static Areas 1 & 2  (zlib/ISO-HDLC, confirmed from firmware tools.h)")
        crc_grp.setObjectName("crc")
        cl = QtWidgets.QGridLayout(crc_grp)
        cl.setColumnStretch(3, 1)

        note = QtWidgets.QLabel(
            "<b>Covers:</b> 156 bytes at main-array offsets 0x0014–0x00AF "
            "(indices 5–43, Static Area 1 data fields).<br>"
            "<b>Stored at:</b> idx 4 (0x0010, Static1) and idx 44 (0x00B0, Static2) "
            "as little-endian uint32.<br>"
            "<b>Algorithm:</b> CRC-32/ISO-HDLC = Python zlib.crc32() &amp; 0xFFFFFFFF."
        )
        note.setWordWrap(True)
        cl.addWidget(note, 0, 0, 1, 4)

        # Step 1: read all static indices
        cl.addWidget(QtWidgets.QLabel("<b>Step 1</b> -- Read all static data indices:"), 1, 0)
        btn_read_static = QtWidgets.QPushButton("Read Static Data (idx 5-43, ~3 s)")
        btn_read_static.setObjectName("sec")
        btn_read_static.clicked.connect(self._read_static_fields)
        cl.addWidget(btn_read_static, 1, 1)
        self._crc32_cache_lbl = QtWidgets.QLabel("Cached: 0/39 static indices")
        self._crc32_cache_lbl.setStyleSheet("color:#64748b;")
        cl.addWidget(self._crc32_cache_lbl, 1, 2, 1, 2)

        # Step 2: calculate
        cl.addWidget(QtWidgets.QLabel("<b>Step 2</b> -- Calculate CRC32:"), 2, 0)
        btn_calc_crc32 = QtWidgets.QPushButton("Calculate CRC32")
        btn_calc_crc32.clicked.connect(self._calc_crc32)
        cl.addWidget(btn_calc_crc32, 2, 1)

        # Step 3: result + write
        cl.addWidget(QtWidgets.QLabel("<b>Step 3</b> -- Write to Static1 + Static2:"), 3, 0)
        self._crc32_lbl = QtWidgets.QLabel("Not calculated yet")
        self._crc32_lbl.setObjectName("crc_pending")
        cl.addWidget(self._crc32_lbl, 3, 1, 1, 2)

        self._crc32_write_btn = QtWidgets.QPushButton(
            "Write CRC32 to Static1 (idx 4) + Static2 (idx 44)")
        self._crc32_write_btn.setObjectName("green")
        self._crc32_write_btn.setEnabled(False)
        self._crc32_write_btn.clicked.connect(self._write_crc32)
        cl.addWidget(self._crc32_write_btn, 3, 3)

        lay.addWidget(crc_grp)
        lay.addStretch()
        return w

    # -- Tab 3: ODP Lock ------------------------------------------------------

    def _build_lock_tab(self):
        w   = QtWidgets.QWidget()
        lay = QtWidgets.QVBoxLayout(w)
        lay.setSpacing(12)

        warn = QtWidgets.QLabel(
            "⚠  <b>WARNING: Locking the M24C08 ODP Identification Page is a "
            "one-time, IRREVERSIBLE hardware operation.</b><br><br>"
            "The OTP fuse inside the M24C08 is blown permanently -- it cannot be undone.<br>"
            "The firmware sends the lock sequence "
            "<code>START + 0xB0 + 0x80 + 0x02 + STOP</code> via the LTC6812 COMM register.<br><br>"
            "The CAN command is <code>M001_WriteStartLTCTransmission (0x10088800)</code> "
            "with magic <code>0x953CA8</code>. This also triggers a fresh cell measurement.<br><br>"
            "<b>Recommended EOL sequence before locking:</b><br>"
            "1. Write all ID fields (Cell Type, Module Type, PCB Type, Serial LO/HI)<br>"
            "2. Read back all 4 ID indices and verify<br>"
            "3. Calculate CRC16 in the ID ODP/Backup tab<br>"
            "4. Write CRC16 to ID Backup (idx 0)<br>"
            "5. Confirm all values are correct -- <em>then</em> lock ODP below"
        )
        warn.setWordWrap(True)
        warn.setStyleSheet(
            "background:#fef2f2; border:1px solid #fca5a5; "
            "border-radius:6px; padding:12px; color:#991b1b;")
        lay.addWidget(warn)

        status_grp = QtWidgets.QGroupBox(
            "Live ODP Lock Status  --  M001_ModuleSafetyMechanisms (0x10010801)  bit 46")
        sl = QtWidgets.QHBoxLayout(status_grp)
        self._lock_lbl = QtWidgets.QLabel("Unknown -- awaiting safety message")
        self._lock_lbl.setObjectName("unknown")
        sl.addWidget(self._lock_lbl)
        sl.addStretch()
        lay.addWidget(status_grp)

        btn_lock = QtWidgets.QPushButton(
            "\U0001f512   Lock ID ODP Page  (IRREVERSIBLE -- sends 0x10088800)")
        btn_lock.setObjectName("warn")
        btn_lock.setFixedHeight(48)
        btn_lock.clicked.connect(self._confirm_and_lock)
        lay.addWidget(btn_lock)

        lay.addStretch()
        return w

    # -- Tab 4: Raw EEPROM ----------------------------------------------------

    def _build_raw_tab(self):
        w   = QtWidgets.QWidget()
        lay = QtWidgets.QVBoxLayout(w)
        lay.setSpacing(8)

        ctrl_grp = QtWidgets.QGroupBox(
            "M001_WriteEEPROMData (0x10087801) -- index x 4 = byte address in main array")
        cl = QtWidgets.QHBoxLayout(ctrl_grp)

        cl.addWidget(QtWidgets.QLabel("Index (0-255):"))
        self._raw_idx = QtWidgets.QSpinBox()
        self._raw_idx.setRange(0, 255)
        self._raw_idx.setFixedWidth(72)
        self._raw_idx.valueChanged.connect(
            lambda v: self._raw_addr_lbl.setText(f"-> addr 0x{v * 4:04X}"))
        cl.addWidget(self._raw_idx)

        self._raw_addr_lbl = QtWidgets.QLabel("-> addr 0x0000")
        self._raw_addr_lbl.setStyleSheet("color:#64748b; font-family:Consolas;")
        cl.addWidget(self._raw_addr_lbl)

        cl.addWidget(QtWidgets.QLabel("  Data (hex LE32):"))
        self._raw_data = _hexline()
        cl.addWidget(self._raw_data)

        b_w = QtWidgets.QPushButton("Write")
        b_w.clicked.connect(self._raw_write)
        b_r = QtWidgets.QPushButton("Read")
        b_r.setObjectName("sec")
        b_r.clicked.connect(self._raw_read)
        cl.addWidget(b_w)
        cl.addWidget(b_r)
        cl.addStretch()
        lay.addWidget(ctrl_grp)

        ref_grp = QtWidgets.QGroupBox(
            "EEPROM address reference  (click a row to load its index)")
        rfl = QtWidgets.QVBoxLayout(ref_grp)

        tbl = QtWidgets.QTableWidget(0, 3)
        tbl.setHorizontalHeaderLabels(["Index", "Address", "Content"])
        tbl.horizontalHeader().setSectionResizeMode(2, QtWidgets.QHeaderView.Stretch)
        tbl.verticalHeader().setVisible(False)
        tbl.setEditTriggers(QtWidgets.QAbstractItemView.NoEditTriggers)
        tbl.setSelectionBehavior(QtWidgets.QAbstractItemView.SelectRows)
        tbl.setAlternatingRowColors(True)
        tbl.verticalHeader().setDefaultSectionSize(22)

        for idx, addr, content in ADDR_REFERENCE:
            r = tbl.rowCount()
            tbl.insertRow(r)
            for col, val in enumerate([str(idx), addr, content]):
                item = QtWidgets.QTableWidgetItem(val)
                item.setTextAlignment(QtCore.Qt.AlignLeft | QtCore.Qt.AlignVCenter)
                tbl.setItem(r, col, item)

        tbl.resizeColumnToContents(0)
        tbl.resizeColumnToContents(1)
        tbl.cellClicked.connect(
            lambda row, _: self._raw_idx.setValue(
                int(tbl.item(row, 0).text())))
        tbl.setFixedHeight(340)
        rfl.addWidget(tbl)
        lay.addWidget(ref_grp)
        lay.addStretch()
        return w

    # -- Write actions --------------------------------------------------------

    def _write_config(self, mux, major, minor, name):
        self.worker.send_config(mux, major, minor)
        self.statusBar().showMessage(
            f"Sent: {name} -> mux=0x{mux:08X}  major=0x{major:02X}  minor=0x{minor:02X}")

    def _write_serial_lo(self):
        val = _parse_hex32(self._ser_lo)
        if val is None:
            return
        self.worker.send_config_4bytes(MUX_SERIAL_LO, struct.pack("<I", val))
        self.statusBar().showMessage(
            f"Sent: Module Serial LO -> 0x{val:08X}  (ODP 0x06-0x09)")

    def _write_serial_hi(self):
        val = _parse_hex32(self._ser_hi)
        if val is None:
            return
        self.worker.send_config_4bytes(MUX_SERIAL_HI, struct.pack("<I", val))
        self.statusBar().showMessage(
            f"Sent: Module Serial HI -> 0x{val:08X}  (ODP 0x0A-0x0D)")

    def _write_static_idx5(self):
        """Index 5 = 0x0014: [SerCells, ParCells, PcbSer[0], PcbSer[1]]."""
        val = (self._ser_cells.value()        |
               (self._par_cells.value() << 8) |
               (self._pcb_s0.value()    << 16) |
               (self._pcb_s1.value()    << 24))
        self.worker.send_eeprom_write(IDX_S1_1, val)
        self.statusBar().showMessage(
            f"Sent: idx 5 (0x0014) = 0x{val:08X}  "
            f"[SerCells={self._ser_cells.value()} "
            f"ParCells={self._par_cells.value()} "
            f"PcbSer[0]=0x{self._pcb_s0.value():02X} "
            f"PcbSer[1]=0x{self._pcb_s1.value():02X}]")

    def _write_static_idx6(self):
        """Index 6 = 0x0018: [PcbSer[2], PcbSer[3], PcbSer[4], PcbSer[5]]."""
        val = 0
        for i, sb in enumerate(self._pcb_s):
            val |= (sb.value() & 0xFF) << (i * 8)
        self.worker.send_eeprom_write(IDX_S1_2, val)
        self.statusBar().showMessage(
            f"Sent: idx 6 (0x0018) = 0x{val:08X}  [PcbSer[2..5]]")

    def _write_static_idx7(self):
        """Index 7 = 0x001C: [PcbSer[6], PcbSer[7], IcType_maj, IcType_min]."""
        val = (self._pcb_s6.value()        |
               (self._pcb_s7.value()   << 8) |
               (self._ict7_maj.value() << 16) |
               (self._ict7_min.value() << 24))
        self.worker.send_eeprom_write(IDX_S1_3, val)
        self.statusBar().showMessage(
            f"Sent: idx 7 (0x001C) = 0x{val:08X}  "
            f"[PcbSer[6]=0x{self._pcb_s6.value():02X} "
            f"PcbSer[7]=0x{self._pcb_s7.value():02X} "
            f"IcType=0x{self._ict7_maj.value():02X}.0x{self._ict7_min.value():02X}]")

    def _raw_write(self):
        idx = self._raw_idx.value()
        val = _parse_hex32(self._raw_data)
        if val is None:
            return
        self.worker.send_eeprom_write(idx, val)
        self.statusBar().showMessage(
            f"Sent: EEPROM write idx={idx} (addr 0x{idx * 4:04X}) "
            f"data=0x{val:08X}")

    def _raw_read(self):
        idx = self._raw_idx.value()
        self.worker.send_eeprom_read(idx)
        self.statusBar().showMessage(
            f"Sent: EEPROM read request idx={idx} (addr 0x{idx * 4:04X})")

    def _confirm_and_lock(self):
        ret = QtWidgets.QMessageBox.warning(
            self,
            "Lock ODP Page -- IRREVERSIBLE",
            "This will permanently blow the M24C08 OTP fuse and lock "
            "the ID Identification Page.\n\n"
            "This CANNOT be undone.\n\n"
            "Are all ID fields correctly written, verified, and CRC16 written?\n\n"
            "Click Yes to send the lock command.",
            QtWidgets.QMessageBox.Yes | QtWidgets.QMessageBox.No,
            QtWidgets.QMessageBox.No,
        )
        if ret == QtWidgets.QMessageBox.Yes:
            self.worker.send_lock_odp()
            self.statusBar().showMessage(
                "Lock command sent (0x10088800 magic=0x953CA8) -- "
                "watch bit 46 in M001_ModuleSafetyMechanisms...")

    # -- CRC16 actions --------------------------------------------------------

    def _read_id_fields(self):
        """Stagger 4 EEPROM reads for ID field indices 0-3."""
        self._stagger_reads(list(range(4)), interval_ms=60)
        self.statusBar().showMessage("Reading ID fields idx 0-3 (240 ms)...")

    def _calc_crc16(self):
        """Calculate CRC16 over the 14 ID data bytes from cache."""
        missing = {0, 1, 2, 3} - set(self._eeprom_cache)
        if missing:
            QtWidgets.QMessageBox.warning(
                self, "Cache incomplete",
                f"Missing EEPROM cache for indices: {sorted(missing)}\n"
                "Click 'Read ID Fields (idx 0-3)' first and wait for all responses.")
            return

        # 14 input bytes: idx0[2:4] + idx1[0:4] + idx2[0:4] + idx3[0:4]
        data = (bytes([self._eeprom_cache[0][2], self._eeprom_cache[0][3]])
                + bytes(self._eeprom_cache[1])
                + bytes(self._eeprom_cache[2])
                + bytes(self._eeprom_cache[3]))
        assert len(data) == 14

        init_val = 0x0000 if self._crc16_init_0.isChecked() else 0xFFFF
        self._crc16_computed = _crc16_ac9a(data, init_val)

        self._crc16_lbl.setObjectName("crc_result")
        self._crc16_lbl.style().unpolish(self._crc16_lbl)
        self._crc16_lbl.style().polish(self._crc16_lbl)
        self._crc16_lbl.setText(
            f"0x{self._crc16_computed:04X}  "
            f"(init=0x{init_val:04X}, poly=0xAC9A, "
            f"input: {data.hex(' ')})")
        self._crc16_write_btn.setEnabled(True)
        self.statusBar().showMessage(
            f"CRC16 = 0x{self._crc16_computed:04X}  -- click Write to commit to idx 0")

    def _write_crc16(self):
        """Write computed CRC16 to ID Backup idx 0 (main array addr 0x0000)."""
        if self._crc16_computed < 0:
            return
        crc_lo = self._crc16_computed & 0xFF
        crc_hi = (self._crc16_computed >> 8) & 0xFF
        # Preserve CellType bytes from cache (idx0 bytes [2] and [3])
        cached0 = self._eeprom_cache.get(0, b'\x00\x00\x00\x00')
        ct_maj  = cached0[2]
        ct_min  = cached0[3]
        val32   = crc_lo | (crc_hi << 8) | (ct_maj << 16) | (ct_min << 24)
        self.worker.send_eeprom_write(0, val32)
        self.statusBar().showMessage(
            f"Wrote CRC16=0x{self._crc16_computed:04X} to ID Backup idx 0 (addr 0x0000)  "
            f"[CellType preserved: maj=0x{ct_maj:02X} min=0x{ct_min:02X}].  "
            "Note: ODP page (I2C 0xB0) is NOT updated -- requires dedicated firmware command.")

    # -- CRC32 actions --------------------------------------------------------

    def _read_static_fields(self):
        """Stagger 39 EEPROM reads for static data indices 5-43 (~3 s total)."""
        self._stagger_reads(list(range(5, 44)), interval_ms=80)
        self.statusBar().showMessage(
            "Reading static indices 5-43 (39 reads, ~3 s)...")

    def _calc_crc32(self):
        """Calculate CRC32 over 156 bytes from cache entries idx 5-43."""
        missing = set(range(5, 44)) - set(self._eeprom_cache)
        if missing:
            QtWidgets.QMessageBox.warning(
                self, "Cache incomplete",
                f"Missing {len(missing)} static indices from cache "
                f"(e.g. {sorted(missing)[:5]}...).\n"
                "Click 'Read Static Data (idx 5-43)' first and wait ~3 s.")
            return

        data = b"".join(self._eeprom_cache[i] for i in range(5, 44))
        assert len(data) == 156

        self._crc32_computed = _crc32_ecu8tr(data)

        self._crc32_lbl.setObjectName("crc_result")
        self._crc32_lbl.style().unpolish(self._crc32_lbl)
        self._crc32_lbl.style().polish(self._crc32_lbl)
        self._crc32_lbl.setText(
            f"0x{self._crc32_computed:08X}  "
            f"(zlib/ISO-HDLC over 156 bytes, stored LE)")
        self._crc32_write_btn.setEnabled(True)
        self.statusBar().showMessage(
            f"CRC32 = 0x{self._crc32_computed:08X}  -- click Write to commit to idx 4 and 44")

    def _write_crc32(self):
        """Write computed CRC32 to Static1 (idx 4) and Static2 (idx 44)."""
        if self._crc32_computed < 0:
            return
        self.worker.send_eeprom_write(4, self._crc32_computed)
        QtCore.QTimer.singleShot(
            120, lambda: self.worker.send_eeprom_write(44, self._crc32_computed))
        self.statusBar().showMessage(
            f"Wrote CRC32=0x{self._crc32_computed:08X} to "
            f"Static1 idx 4 (0x0010) and Static2 idx 44 (0x00B0)")

    # -- Helpers --------------------------------------------------------------

    def _stagger_reads(self, indices: list, interval_ms: int = 80):
        """Send EEPROM read requests for a list of indices, interval_ms apart."""
        def fire(i):
            if i >= len(indices):
                return
            self.worker.send_eeprom_read(indices[i])
            QtCore.QTimer.singleShot(interval_ms, lambda: fire(i + 1))
        fire(0)

    def _refresh_crc_status(self):
        """Update cache progress labels after a new EEPROM index is cached."""
        if self._crc16_cache_lbl is not None:
            n = sum(1 for i in range(4) if i in self._eeprom_cache)
            if n == 4:
                self._crc16_cache_lbl.setText(
                    "All 4 ID indices cached -- ready to calculate")
                self._crc16_cache_lbl.setStyleSheet("color:#16a34a; font-weight:600;")
            else:
                self._crc16_cache_lbl.setText(f"Cached: {n}/4 ID indices")
                self._crc16_cache_lbl.setStyleSheet("color:#64748b;")

        if self._crc32_cache_lbl is not None:
            n = sum(1 for i in range(5, 44) if i in self._eeprom_cache)
            if n == 39:
                self._crc32_cache_lbl.setText(
                    "All 39 static indices cached -- ready to calculate")
                self._crc32_cache_lbl.setStyleSheet("color:#16a34a; font-weight:600;")
            else:
                self._crc32_cache_lbl.setText(f"Cached: {n}/39 static indices")
                self._crc32_cache_lbl.setStyleSheet("color:#64748b;")

    # -- CAN RX ---------------------------------------------------------------

    def _handle_can(self, msg):
        data   = bytes(msg.data).ljust(8, b"\x00")
        can_id = msg.arbitration_id
        self._rx_count += 1
        self._rx_lbl.setText(f"RX: {self._rx_count}")

        if can_id == ID_EEPROM_DATA:
            self._on_eeprom_data(data)
        elif can_id == ID_SAFETY_MECH:
            self._on_safety_mechanisms(data)

    def _on_eeprom_data(self, data):
        """M001_EEPROMData: [index][dynAreaA|...][0][0][d0][d1][d2][d3]."""
        index  = data[0]
        dyn_a  = bool(data[1] & 0x01)
        d      = data[4:8]
        val32  = struct.unpack_from("<I", d)[0]
        addr   = index * 4
        ann    = _annotate(addr, d)

        # Update read-back cache and CRC status labels
        self._eeprom_cache[index] = bytes(d)
        self._refresh_crc_status()

        text = (f"idx={index}  addr=0x{addr:04X}  dynAreaA={dyn_a}  "
                f"raw=0x{val32:08X}  [{d[0]:02X} {d[1]:02X} {d[2]:02X} {d[3]:02X}]"
                + (f"    ->  {ann}" if ann else ""))
        self._resp_lbl.setText(text)

    def _on_safety_mechanisms(self, data):
        """M001_ModuleSafetyMechanisms: bit 46 = IDPageNotLocked (1=unlocked, 0=locked)."""
        not_locked = bool(data[5] & (1 << 6))
        if not_locked:
            self._lock_lbl.setText("OK  UNLOCKED  (IDPageNotLocked bit = 1)")
            self._lock_lbl.setObjectName("unlocked")
        else:
            self._lock_lbl.setText("LOCKED  (IDPageNotLocked bit = 0)")
            self._lock_lbl.setObjectName("locked")
        self._lock_lbl.style().unpolish(self._lock_lbl)
        self._lock_lbl.style().polish(self._lock_lbl)


# ---------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(
        description="GEN 3.0 CSC EEPROM Tester (Kvaser / PyQt5)")
    parser.add_argument("--channel", type=int, default=0,
                        help="Kvaser channel index (default 0)")
    parser.add_argument("--bitrate", type=int, default=500000,
                        help="CAN bitrate in bit/s (default 500000)")
    args = parser.parse_args()

    app    = QtWidgets.QApplication(sys.argv)
    worker = CanWorker(args.channel, args.bitrate)
    window = EepromTesterWindow(worker)
    window.show()
    worker.start()
    rc = app.exec_()
    worker.stop()
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
