#from io import IncrementalNewlineDecoder
#import logging
#from re import A
#import pyvisa

from PyQt5 import QtWidgets as qtw
from PyQt5 import QtCore as qtc
import signal

#from nrc import NRC_2_Str
import zlib
import sys
import getopt
import struct
import time
import socket
from enum import IntEnum

from datetime import datetime

#custom imports
from mainFordSKModule import Ui_MainWindow
from tle9012source import  TLE9012

sys.path.insert(0,'..')
sys.path.insert(0, "../bms")
sys.path.insert(0,'../env/Lib/site-packages')

from bms.kvaser import Kvaser
from bms.docan import DoCAN
from canlib import canlib

# Default source address
ID_SA = 0x0a
# Default Target address
ID_TA = 0x0b
OPTION_CONTROL = 0x03
UDS_IOCBI = 0x2F
ID1 = 0x00

UDP_TX_PORT_NUM = 8889 #default port address for initial connection
UDP_RX_PORT_NUM = 8888 #default port address for initial connection
TCP_CMD_PORT_NUM = 8000 #default port address for initial connection
TCP_DATA_PORT_NUM = 8001 #default port address for initial connection
ECU8TR_IP_ADDRESS = "192.168.1.200" # Default IP address for initial connection

# Length of the byte stream to receive
DATA_LENGTH = 104
# Length of the CRC32 value
CRC_LENGTH = 4

MAX_TLE9012_DATA_ELEMENT = 32
EXPECTED_DELIMITER = 0x2424  # Assuming "$$" translates to 0x2424
TOTAL_DATA_SIZE = 104   # 1+1+(3*32) + 4 + 2

CELL_MEASUREMENT_FACTOR = 5000.0/65536.0
BLOCK_MEASUREMENT_FACTOR = 60000.0/65536.0
TLE9012_EXT_OT_CONV	= 6250.0/1024.0

INT_TEMP_LSB = 0.66                         # Page 36: formula 7.1 
INT_TEMP_BASE = 547.3

RED = '\033[91m'
GREEN = '\033[92m'
RESET = '\033[0m'  # Reset the color

# Data structure to hold received data
class ECU8TRData:
    def __init__(self):
        self.dev = None
        self.body_cnt = None
        self.data_bodies = []  # List to hold ECU8TR_DATA_BODY_t elements
        self.crc = None
        self.delimiter = None

class ECU8TR_CMD_e(IntEnum):
    CMD_NET_SET_MAC = 0
    CMD_NET_SET_IP = 1
    CMD_NET_SET_NETMASK = 2
    CMD_NET_SET_GATEWAY = 3
    CMD_NET_SHOW_NETWORK = 4
    CMD_TLE9012_SLEEP = 5
    CMD_TLE9012_WAKEUP = 6
    CMD_TLE9012_SET_NETWORK = 7
    CMD_ECU8TR_VERSION = 8
    CMD_TLE9012_GET_NETWORK = 9
    CMD_TLE9012_READ_REG = 10
    CMD_TLE9012_WRITE_REG = 11
    CMD_HELP = 12
    CMD_EXIT = 13
    CMD_NONE = 14
cmd_sent = ECU8TR_CMD_e.CMD_NONE

command_map = {
    "set_mac": ECU8TR_CMD_e.CMD_NET_SET_MAC,
    "set_ip": ECU8TR_CMD_e.CMD_NET_SET_IP,
    "set_netmask": ECU8TR_CMD_e.CMD_NET_SET_NETMASK,
    "set_gateway": ECU8TR_CMD_e.CMD_NET_SET_GATEWAY,
    "show_network": ECU8TR_CMD_e.CMD_NET_SHOW_NETWORK,
    "tle9012_sleep": ECU8TR_CMD_e.CMD_TLE9012_SLEEP,
    "tle9012_wakeup": ECU8TR_CMD_e.CMD_TLE9012_WAKEUP,
    "tle9012_read_reg": ECU8TR_CMD_e.CMD_TLE9012_READ_REG,
    "tle9012_write_reg": ECU8TR_CMD_e.CMD_TLE9012_WRITE_REG,
    "tle9012_set_network": ECU8TR_CMD_e.CMD_TLE9012_SET_NETWORK,
    "ecu8tr_version": ECU8TR_CMD_e.CMD_ECU8TR_VERSION,
    "exit": ECU8TR_CMD_e.CMD_EXIT,
    }


def do_parse_read_reg_cmd( data ):
    if data[0] == ISOUART_COMM_SUCCESS:
        if data[1] == 1:
            print( "Ring structure: True" )
        else:
            print( "Ring structure: False" )
        if data[2] == 1:
            val = data[5] << 8 | data[6]
            print( f"Read dev = {hex(data[3])}, reg = {hex(data[4])}, value = {hex(val)}" )
            print( "" )
    else:
        print( "Failed to do a register read on TLE9012" )


def pack_ecu8_cmd(cmd_code, cmd_body):

    cmd_body = cmd_body.encode('utf-8')
    cmd_body = cmd_body[:32]
    cmd_body += b'\x00' * (32 - len(cmd_body))
    delimiter = b'$$'
    packed_data = struct.pack('>I32sh', cmd_code, cmd_body, struct.unpack('>h', delimiter)[0])
    return packed_data

def receive_message(sock):
    
    global cmd_sent
    global is_exit

    sock.settimeout(1)  # Non-blocking with timeout
    while is_exit == False:
        try:
            data = sock.recv(1024)
            if data:
                if cmd_sent == ECU8TR_CMD_e.CMD_TLE9012_READ_REG:
                    cmd_sent = ECU8TR_CMD_e.CMD_NONE
                    do_parse_read_reg_cmd( data )
                else:
                    message = data.decode('utf-8').rstrip('$$')
                    print(f"{message}")
            else:
                break
        except socket.timeout:
            continue  # Continue listening
        except Exception as e:
            print(f"Error: {e}")
            sys.exit( 1 )



class MainWindow(qtw.QMainWindow):
    
    readTimer = qtc.QTimer()
    expectedNumOfAFENodes = 1   #defaults to expect 1 node for min connection

    
    def __init__(self,parent = None):
        super(MainWindow,self).__init__(parent)        
        self.ui = Ui_MainWindow()
        self.ui.setupUi(self)
        
        self.ui.pushButtonSleep.clicked.connect(self.buttonSleepClicked)
        self.ui.pushButtonWake.clicked.connect(self.buttonWakeClicked)
        self.ui.pushButtonTest.clicked.connect(self.buttonTestClicked)
        
        #init CAN/BMS comms
        self.ui.radioButtonCANRX.setChecked(True)
        self.ui.radioButtonCANTX.setChecked(True)
        self.ui.radioButtonCANRX.setChecked(False)
        self.ui.radioButtonCANTX.setChecked(False)

        self.ui.listWidgetCANMessages.addItem("Peek BPSM messages")

        self.getVersion()

        # Create a TCP/IP socket
        self.stream_cell_data_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        # Set socket timeout to 5 seconds
        self.stream_cell_data_sock.settimeout(5)        
        # Connect the data socket to the server for streaming reception
        self.stream_cell_data_sock.connect((ECU8TR_IP_ADDRESS, TCP_DATA_PORT_NUM))
        print("Connected to the server.")
            
        
        
    
    def getVersion(self):
        #connect to ECU8TR and get version        
        print("Check ECU8TR version")
        
        # Setup networking sockets
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout( 5 )
            sock.connect((ECU8TR_IP_ADDRESS, TCP_CMD_PORT_NUM))
        except socket.timeout:
            print("Socket operation timed out")
            sys.exit( 1 )
        except ConnectionRefusedError:
            print("Connection refused by the server")
            sys.exit( 1 )
        except socket.error as err:
            print(f"Socket error: {err}")
            sys.exit( 1 )
        
        cmd_code = command_map["ecu8tr_version"]
        cmd_body = ""
        packed_data = pack_ecu8_cmd(cmd_code, cmd_body)
        sock.sendall(packed_data)

        cmd_sent = ECU8TR_CMD_e.CMD_ECU8TR_VERSION

        try:
            data = sock.recv(1024)
            if data:
                if cmd_sent == ECU8TR_CMD_e.CMD_TLE9012_READ_REG:
                    cmd_sent = ECU8TR_CMD_e.CMD_NONE
                    do_parse_read_reg_cmd( data )
                else:
                    message = data.decode('utf-8').rstrip('$$')
                    self.ui.labelFirmwareVer.setText(message)
                    print(f"{message}")
        except socket.timeout:
            print("socket timeout")

        sock.close()
        

    def buttonTestClicked(self):
        #Commands are sent automatically to connect, disconnect, stream and stop streaming data
        if (self.ui.pushButtonWake.text() == "CONNECT"):     #don't allow automated commands if using manual mode

            if (self.ui.pushButtonTest.text() == "TEST"):
                #RED blinking LED goes to GREEN blinking LED
                if (self.ui.comboBoxComms.currentText() =="CAN"):
                    print("Starting CAN Mode")
                    self.readTimer.timeout.connect(self.peekECU8TRStream)
                    self.connectModuleCAN()
                    self.ui.radioButtonCANRX.setChecked(False)
                elif (self.ui.comboBoxComms.currentText() =="ETHERNET"):
                    print("Starting Ethernet Mode")
                    self.readTimer.timeout.connect(self.peekECU8TRStreamIP)
                    self.connectModuleIP()
                    time.sleep(1)
                    self.streamModuleIP()
                    
                self.readTimer.start(200)
                self.ui.pushButtonTest.setText("END TEST")
                self.ui.labelActiveNodes.setText("1")
                
            else:
                #goes from GREEN blinking LED to RED blinking LED
                self.readTimer.stop()

                if (self.ui.comboBoxComms.currentText() =="CAN"):
                    self.readTimer.timeout.disconnect(self.peekECU8TRStream)                
                    self.disconnectModuleCAN()
                elif (self.ui.comboBoxComms.currentText() =="ETHERNET"):                
                    self.disconnectModuleIP()
                    time.sleep(1)
                    self.stopStreamModuleIP()

                self.ui.pushButtonTest.setText("TEST")
                self.ui.labelActiveNodes.setText("0")

    def buttonWakeClicked(self):
    #sends wake command and puts system into stream response mode
        #RED blinking LED goes to GREEN solid LED and blinking if data port 8001 open
        self.wakeModuleIP()
        #print("Waking up BPSM and Streaming in Ethernet Mode")
        self.readTimer.timeout.connect(self.peekECU8TRStreamIP)
        self.readTimer.start(200)


    def buttonSleepClicked(self):
    #sends stop streaming and sleep commands
        
        #GREEN LED goes to blinking LED
        self.readTimer.stop()
        self.sleepModuleIP()

        
    def processFrameIntoFullMetrics(self,results_frame):
        #populates full metrics frame from individuals for ease of UI populating
   
        #print(int.from_bytes(results_frame[7:8],"little"))
        frame_id = int.from_bytes(results_frame[7:8],"little")

        if (frame_id == 1):
            print("frame 1")
            conversion_val = int.from_bytes(results_frame[0:2], "big")
            self.ui.labelHSModuleVal.setText(str(float(conversion_val)/1000))
            self.ui.labelModuleVal.setText(str(float(conversion_val)/1000))            
            conversion_val = int.from_bytes(results_frame[2:4], "big")
            self.ui.labelCellValHS1.setText(str(float(conversion_val)/1000))
            self.ui.progressBarCellValHS1.setValue(100/42*(conversion_val/100))
            conversion_val = int.from_bytes(results_frame[4:6], "big")
            self.ui.labelCellValHS2.setText(str(float(conversion_val)/1000))
            self.ui.progressBarCellValHS2.setValue(100/42*(conversion_val/100))

        elif (frame_id == 2):
            print("frame 2")
            conversion_val = int.from_bytes(results_frame[0:2], "big")
            self.ui.labelCellValHS3.setText(str(float(conversion_val)/1000))
            self.ui.progressBarCellValHS3.setValue(100/42*(conversion_val/100))
            conversion_val = int.from_bytes(results_frame[2:4], "big")
            self.ui.labelCellValHS4.setText(str(float(conversion_val)/1000))
            self.ui.progressBarCellValHS4.setValue(100/42*(conversion_val/100))
            conversion_val = int.from_bytes(results_frame[4:6], "big")
            self.ui.labelCellValHS5.setText(str(float(conversion_val)/1000))
            self.ui.progressBarCellValHS5.setValue(100/42*(conversion_val/100))

        elif (frame_id == 3):
            print("frame 3")
            conversion_val = int.from_bytes(results_frame[0:2], "big")
            self.ui.labelCellValHS6.setText(str(float(conversion_val)/1000))
            self.ui.progressBarCellValHS6.setValue(100/42*(conversion_val/100))
            conversion_val = int.from_bytes(results_frame[2:4], "big")
            self.ui.labelCellValHS7.setText(str(float(conversion_val)/1000))
            self.ui.progressBarCellValHS7.setValue(100/42*(conversion_val/100))
            conversion_val = int.from_bytes(results_frame[4:6], "big")
            self.ui.labelCellValHS8.setText(str(float(conversion_val)/1000))
            self.ui.progressBarCellValHS8.setValue(100/42*(conversion_val/100))

        elif (frame_id == 4):
            print("frame 4")
            conversion_val = int.from_bytes(results_frame[0:2], "big")
            self.ui.labelCellValHS9.setText(str(float(conversion_val)/1000))
            self.ui.progressBarCellValHS9.setValue(100/42*(conversion_val/100))
            conversion_val = int.from_bytes(results_frame[2:4], "big")
            self.ui.labelCellValHS10.setText(str(float(conversion_val)/1000))
            self.ui.progressBarCellValHS10.setValue(100/42*(conversion_val/100))
            conversion_val = int.from_bytes(results_frame[4:6], "big")
            self.ui.labelCellValHS11.setText(str(float(conversion_val)/1000))
            self.ui.progressBarCellValHS11.setValue(100/42*(conversion_val/100))
        elif (frame_id == 5):
            print("frame 5")
            conversion_val = int.from_bytes(results_frame[0:2], "big")
            self.ui.labelCellValHS12.setText(str(float(conversion_val)/1000))
            self.ui.progressBarCellValHS12.setValue(100/42*(conversion_val/100))
            #conversion_val = int.from_bytes(results_frame[2:4], "big")
            #self.ui.labelCellValHS4.setText(str(float(conversion_val)/1000))
            #conversion_val = int.from_bytes(results_frame[4:6], "big")
            #self.ui.labelCellValHS5.setText(str(float(conversion_val)/1000))            
        elif (frame_id == 6):
            print("frame 6")
            conversion_val = int.from_bytes(results_frame[0:2], "big")
            self.ui.labelTempDieVal.setText(str(float(conversion_val)/10))
            conversion_val = int.from_bytes(results_frame[2:4], "big")
            self.ui.labelTemp1Val.setText(str(float(conversion_val-10000)/1000))
            conversion_val = int.from_bytes(results_frame[4:6], "big")
            self.ui.labelTemp2Val.setText(str(float(conversion_val-10000)/1000))
        elif (frame_id == 7):
            print("frame 7")
            conversion_val = int.from_bytes(results_frame[0:2], "big")
            self.ui.labelTemp3Val.setText(str(float(conversion_val-10000)/1000))
            conversion_val = int.from_bytes(results_frame[2:4], "big")
            self.ui.labelTemp4Val.setText(str(float(conversion_val-10000)/1000))
            conversion_val = int.from_bytes(results_frame[4:6], "big")
            self.ui.labelTemp5Val.setText(str(float(conversion_val-10000)/1000))
        else:
            print("error in parsing CAN message number D7")


    def peekECU8TRStreamIP(self):
        #check for IP messages
        
        print("\nPeeking IP messages...")

        correct_count = 0
        error_count = 0

        # Create a TCP/IP socket
        '''sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        # Set socket timeout to 5 seconds
        sock.settimeout(1)
        
        # Connect the socket to the server
        try:
            sock.connect((ECU8TR_IP_ADDRESS, TCP_DATA_PORT_NUM))
            print("Connected to the server.")
            
            sock.settimeout(1)
            
                # Receive the byte stream
            data = sock.recv(DATA_LENGTH)
            if not data:
                # Server has closed the connection
                print("\nServer has closed the connection.")            
            elif len(data) < DATA_LENGTH:
                #print("\nIncomplete data received.")
                error_count += 1
                

            # Receive the CRC32 value
            received_crc = sock.recv(CRC_LENGTH)
            if not received_crc:
                # Server has closed the connection
                print("\nServer has closed the connection.")
            elif len(received_crc) < CRC_LENGTH:
                #print("\nIncomplete CRC received.")
                #break
                error_count += 1
            

            # Calculate the CRC32 of the received data
            calculated_crc = zlib.crc32(data) & 0xFFFFFFFF
            received_crc_value = int.from_bytes(received_crc, byteorder='big')

            # Compare the calculated CRC with the received one
            if calculated_crc == received_crc_value:
                correct_count += 1
            else:
                error_count += 1
                
            sys.stdout.write(f"\rCorrect: {correct_count}, Errors: {error_count}")
            sys.stdout.flush()

            sock.close()

        except socket.timeout:
            print("\nSocket operation timed out.")
        except socket.error as e:
            print(f"\nSocket error: {e}")
        except KeyboardInterrupt:
            print("\nProgram terminated by user.")
        finally:
            sock.close()
        
        sock.close()'''
        
        '''self.stream_cell_data_sock.settimeout(20)
        data = self.stream_cell_data_sock.recv(DATA_LENGTH)
        print (data)

        if not data:
            # Server has closed the connection
            print("\nServer has closed the connection.")            
        elif len(data) < DATA_LENGTH:
            print("\nIncomplete data received.")
            error_count += 1
                

        # Receive the CRC32 value
        received_crc = self.stream_cell_data_sock.recv(CRC_LENGTH)
        print(received_crc)
        if not received_crc:
                # Server has closed the connection
            print("\nServer has closed the connection.")
        elif len(received_crc) < CRC_LENGTH:
            print("\nIncomplete CRC received.")
                #break
            error_count += 1
            

        # Calculate the CRC32 of the received data
        calculated_crc = zlib.crc32(data) & 0xFFFFFFFF
        received_crc_value = int.from_bytes(received_crc, byteorder='big')

        # Compare the calculated CRC with the received one
        if calculated_crc == received_crc_value:
            correct_count += 1
        else:
            error_count += 1
                
        sys.stdout.write(f"\rCorrect: {correct_count}, Errors: {error_count}")
        sys.stdout.flush()'''


        ecu8_data = ECU8TRData()
        data = self.stream_cell_data_sock.recv(TOTAL_DATA_SIZE)
        #print(data)

                
        # Unpack delimiter and compare
        delimiter, = struct.unpack("!H", data[-2:])  # Assuming network (big-endian) byte order
        if delimiter != EXPECTED_DELIMITER:
            self.find_delimiter(self.stream_cell_data_sock)
        else:
            ecu8_data.delimiter = delimiter  # Now correctly assigned
            
        # Extract CRC and compare
        ecu8_data.crc = int.from_bytes(data[-6:-2], byteorder='big')  # Assuming CRC is before the last 2 bytes (delimiter)
        calculated_crc = zlib.crc32(data[:-6]) & 0xFFFFFFFF  # Excluding CRC and delimiter from calculation
        if ecu8_data.crc != calculated_crc:
            print(f"CRC error: received: {ecu8_data.crc:#x}, calculated: {calculated_crc:#x}")
            

        # Extract `dev` and `body_cnt`
        ecu8_data.dev, ecu8_data.body_cnt = struct.unpack("!BB", data[:2])

        data_body_data = data[2:-6]  # Skipping header, CRC, and delimiter
        for i in range(ecu8_data.body_cnt):
            offset = i * 3  # Each ECU8TR_DATA_BODY_t is 3 bytes
            body_segment = data_body_data[offset:offset + 3]
            reg, reg_value = struct.unpack("!BH", body_segment)  # Unpack each data body
            ecu8_data.data_bodies.append((reg, reg_value))
        
        print(ecu8_data.data_bodies)

        self.process_data(ecu8_data)

        #            self.processFrameIntoFullMetrics(data)
        #           self.ui.listWidgetCANMessages.addItem(str(data))
                    #print(f"Received {len(data)} bytes from {address}: {data.decode()}")    

    def wakeModuleIP(self):
        #connect to module AFEs based on config
        print("waking up battery module via IP")
        # Setup networking sockets
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout( 5 )
            sock.connect((ECU8TR_IP_ADDRESS, TCP_CMD_PORT_NUM))
        except socket.timeout:
            print("Socket operation timed out")
            sys.exit( 1 )
        except ConnectionRefusedError:
            print("Connection refused by the server")
            sys.exit( 1 )
        except socket.error as err:
            print(f"Socket error: {err}")
            sys.exit( 1 )
        
        
        cmd_code = command_map["tle9012_wakeup"]
        cmd_body = ""
        packed_data = pack_ecu8_cmd(cmd_code, cmd_body)
        sock.sendall(packed_data)

        cmd_sent = ECU8TR_CMD_e.CMD_TLE9012_WAKEUP

        try:
            data = sock.recv(1024)
            if data:
                if cmd_sent == ECU8TR_CMD_e.CMD_TLE9012_READ_REG:
                    cmd_sent = ECU8TR_CMD_e.CMD_NONE
                    do_parse_read_reg_cmd( data )
                else:
                    message = data.decode('utf-8').rstrip('$$')
                    #self.ui.labelFirmwareVer.setText(message)
                    print(f"{message}")
        except socket.timeout:
            print("socket timeout")

        sock.close()

        
    def sleepModuleIP(self):
        #connect to module AFEs based on config
        print("putting to sleep battery module via IP")
        
        # Setup networking sockets
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout( 5 )
            sock.connect((ECU8TR_IP_ADDRESS, TCP_CMD_PORT_NUM))
        except socket.timeout:
            print("Socket operation timed out")
            sys.exit( 1 )
        except ConnectionRefusedError:
            print("Connection refused by the server")
            sys.exit( 1 )
        except socket.error as err:
            print(f"Socket error: {err}")
            sys.exit( 1 )

        cmd_code = command_map["tle9012_sleep"]
        cmd_body = ""
        packed_data = pack_ecu8_cmd(cmd_code, cmd_body)
        sock.sendall(packed_data)

        cmd_sent = ECU8TR_CMD_e.CMD_TLE9012_SLEEP

        try:
            data = sock.recv(1024)
            if data:
                if cmd_sent == ECU8TR_CMD_e.CMD_TLE9012_READ_REG:
                    cmd_sent = ECU8TR_CMD_e.CMD_NONE
                    do_parse_read_reg_cmd( data )
                else:
                    message = data.decode('utf-8').rstrip('$$')
                    #self.ui.labelFirmwareVer.setText(message)
                    print(f"{message}")
        except socket.timeout:
            print("socket timeout")

        sock.close()
    
    def get_ext_temp(self, regVal ):
        intc = ( regVal >> 10 ) & 0x03
        result = regVal & 0x3FF
        return( TLE9012_EXT_OT_CONV * pow( 4, intc ) * result )

    def get_internal_temp(self, regVal ):
        INTERNAL_TEMP = INT_TEMP_BASE - INT_TEMP_LSB * (float)( regVal & 0x3FF )
        return INTERNAL_TEMP

    def find_delimiter(sock):
        buffer = b''
        while len(buffer) < 2:
            buffer += sock.recv(1)
            if buffer[-2:] == struct.pack("H", EXPECTED_DELIMITER):
                return True
        return False
    
    def get_physical( self, reg, digital ):
        if reg >= 0x19 and reg <= 0x24: #CVM
            return( digital * CELL_MEASUREMENT_FACTOR )
        if reg == 0x28: #BVM
            return( digital * BLOCK_MEASUREMENT_FACTOR )
        if reg == 0x30: #Internal temp
            return self.get_internal_temp( digital )
        if reg >= 0x29 and reg <= 0x2d:
            return self.get_ext_temp( digital )
        return 0.0

    def process_data(self, ecu8tr_data ):
        # If CRC and delimiter are valid, print the data

        print( "" )
        print(f"Device: {ecu8tr_data.dev}")
        print(f"Body Count: {ecu8tr_data.body_cnt}")
        for i, body in enumerate(ecu8tr_data.data_bodies, start=1):
            reg = body[0]
            digital = body[1]
            phy = self.get_physical( reg, digital )
            print( f"Data Body {i}: Reg = {hex(reg)}, Reg Value = {digital:#06x}, phy = {phy}" )

            conversion_val = phy

            if (hex(reg) == '0x19'):
                print("C1")                
                #self.ui.labelHSModuleVal.setText(str(float(conversion_val)/1000))
                #self.ui.labelModuleVal.setText(str(float(conversion_val)/1000))
                self.ui.labelCellValHS1.setText(str(round(conversion_val/1000,3)))
                self.ui.progressBarCellValHS1.setValue(100/42*(conversion_val/100))
                
            elif (hex(reg) == '0x1a'):
                print("C2")                
                self.ui.labelCellValHS2.setText(str(round(conversion_val/1000,3)))
                self.ui.progressBarCellValHS2.setValue(100/42*(conversion_val/100))

            elif (hex(reg) == '0x1b'):
                print("C3")                
                self.ui.labelCellValHS3.setText(str(round(conversion_val/1000,3)))
                self.ui.progressBarCellValHS3.setValue(100/42*(conversion_val/100))

            elif (hex(reg) == '0x1c'):
                print("C4")
                self.ui.labelCellValHS4.setText(str(round(conversion_val/1000,3)))
                self.ui.progressBarCellValHS4.setValue(100/42*(conversion_val/100))
            
            elif (hex(reg) == '0x1d'):
                print("C5")
                self.ui.labelCellValHS5.setText(str(round(conversion_val/1000,3)))
                self.ui.progressBarCellValHS5.setValue(100/42*(conversion_val/100))

            elif (hex(reg) == '0x1e'):
                print("C6")
                self.ui.labelCellValHS6.setText(str(round(conversion_val/1000,3)))
                self.ui.progressBarCellValHS6.setValue(100/42*(conversion_val/100))
            
            elif (hex(reg) == '0x1f'):
                print("C7")
                self.ui.labelCellValHS7.setText(str(round(conversion_val/1000,3)))
                self.ui.progressBarCellValHS7.setValue(100/42*(conversion_val/100))
            
            elif (hex(reg) == '0x20'):
                print("C8")
                self.ui.labelCellValHS8.setText(str(round(conversion_val/1000,3)))
                self.ui.progressBarCellValHS8.setValue(100/42*(conversion_val/100))

            elif (hex(reg) == '0x21'):
                print("C9")
                self.ui.labelCellValHS9.setText(str(round(conversion_val/1000,3)))
                self.ui.progressBarCellValHS9.setValue(100/42*(conversion_val/100))
            elif (hex(reg) == '0x22'):
                print("C10")
                self.ui.labelCellValHS10.setText(str(round(conversion_val/1000,3)))
                self.ui.progressBarCellValHS10.setValue(100/42*(conversion_val/100))
            elif (hex(reg) == '0x23'):
                print("C11")
                self.ui.labelCellValHS11.setText(str(round(conversion_val/1000,3)))
                self.ui.progressBarCellValHS11.setValue(100/42*(conversion_val/100))
            elif (hex(reg) == '0x24'):
                print("C12")
                self.ui.labelCellValHS12.setText(str(round(conversion_val/1000,3)))
                self.ui.progressBarCellValHS12.setValue(100/42*(conversion_val/100))
            elif (hex(reg) == '0x28'):
                print("Module V")
                self.ui.labelHSModuleVal.setText(str(round(conversion_val/1000,3)))
                self.ui.labelModuleVal.setText(str(round(conversion_val/1000,3)))
            elif (hex(reg) == '0x30'):
                print("Temp Die")
                self.ui.labelTempDieVal.setText(str(round(conversion_val,2)))
            elif (hex(reg) == '0x29'):
                print("Temp 1")
                self.ui.labelTemp1Val.setText(str(round((conversion_val-10000)/1000,2)))
            elif (hex(reg) == '0x2a'):
                print("Temp 2")
                self.ui.labelTemp2Val.setText(str(round((conversion_val-10000)/1000,2)))
            elif (hex(reg) == '0x2b'):
                print("Temp 3")
                self.ui.labelTemp3Val.setText(str(round((conversion_val-10000)/1000,2)))
            elif (hex(reg) == '0x2c'):
                print("Temp 4")
                self.ui.labelTemp4Val.setText(str(round((conversion_val-10000)/1000,2)))
            elif (hex(reg) == '0x2d'):
                print("Temp 5")
                self.ui.labelTemp5Val.setText(str(round((conversion_val-10000)/1000,2)))


        print( "" )
        
    def __del__(self):
        self.stream_cell_data_sock.close()



    
    
def main():       
    print("Starting ECU8TR Application V0.0.3")
    # Handle high resolution displays:
    if hasattr(qtc.Qt, 'AA_EnableHighDpiScaling'):
        qtw.QApplication.setAttribute(qtc.Qt.AA_EnableHighDpiScaling, True)
    if hasattr(qtc.Qt, 'AA_UseHighDpiPixmaps'):
        qtw.QApplication.setAttribute(qtc.Qt.AA_UseHighDpiPixmaps, True)
               

    app =qtw.QApplication([])
    mainWin = MainWindow()
    mainWin.show()
    sys.exit(app.exec_())

if __name__ == '__main__':
    main()
