import socket
import struct
import zlib
import sys

ADBMS6830 = 0
TLE9012 = 1

CELL_MONITOR_SELECT = ADBMS6830

CELL_MEASUREMENT_FACTOR = 5000.0/65536.0
BLOCK_MEASUREMENT_FACTOR = 60000.0/65536.0
TLE9012_EXT_OT_CONV	= 6250.0/1024.0

ADBMS6830_CELL_V_LSB = 150
ADBMS6830_CELL_CONVERSION_FACTOR = 1500
ADBMS6830_VOLT_SCALING = 1000
ADBMS6830_TEMP_SCALING = 100
ADBMS6830_MVPERDEGREEC = 75
ADBMS6830_TEMP_KELVIN = 2730
ADBMS6830_NUMBER_OF_NODES = 2

INT_TEMP_LSB = 0.66                         # Page 36: formula 7.1 
INT_TEMP_BASE = 547.3

# Constants
IP = "192.168.1.200"
PORT = 8001
MAX_TLE9012_DATA_ELEMENT = 50
MAX_ADBMS6830_DATA_ELEMENT = 100
EXPECTED_DELIMITER = 0x2424  # Assuming "$$" translates to 0x2424
TOTAL_TLE9012_DATA_SIZE = 1+1+(3*MAX_ADBMS6830_DATA_ELEMENT)+4+2   # 1+1+(3*x) + 4 + 2
TOTAL_ADBMS6830_DATA_SIZE = ADBMS6830_NUMBER_OF_NODES+1+(3*MAX_ADBMS6830_DATA_ELEMENT)+4+2   # 1+1+(3*x) + 4 + 2

BIT_WIDTH = 0

DEBUG = 0

def get_ext_temp( regVal ):
    intc = ( regVal >> 10 ) & 0x03
    result = regVal & 0x3FF
    return( TLE9012_EXT_OT_CONV * pow( 4, intc ) * result )

def get_internal_temp( regVal ):
    INTERNAL_TEMP = INT_TEMP_BASE - INT_TEMP_LSB * (float)( regVal & 0x3FF )
    return INTERNAL_TEMP

def find_delimiter(sock):
    buffer = b''
    while len(buffer) < 2:
        buffer += sock.recv(1)
        if buffer[-2:] == struct.pack("H", EXPECTED_DELIMITER):
            return True
    return False

# Data structure to hold received data
class ECU8TRData_ADBMS6830:
    def __init__(self):
        self.dev = []           #List of device numbers
        self.body_cnt = None
        self.data_bodies = []  # List to hold ECU8TR_DATA_BODY_t elements
        self.crc = None
        self.delimiter = None

# Data structure to hold received data
class ECU8TRData_TLE9012:
    def __init__(self):
        self.dev = None
        self.body_cnt = None
        self.data_bodies = []  # List to hold ECU8TR_DATA_BODY_t elements
        self.crc = None
        self.delimiter = None

def connect_to_server(ip, port):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(30)
    try:
        s.connect((ip, port))
        print("Connected to the server.")
    except socket.timeout:
        print("Connection timed out.")
        s = None
    except Exception as e:
        print(f"Failed to connect to the server: {e}")
        s = None
    return s

def receive_adbms6830_data(sock):

    ecu8_data = ECU8TRData_ADBMS6830()

    sock.settimeout( 120 ); # 20 minutes

    while True:
        try:
            # Header
            data = sock.recv(TOTAL_ADBMS6830_DATA_SIZE)
            #print(data)
            if not data:
                # Server has closed the connection
                raise ValueError("Incomplete header received, the server closed the socket")
                
            # Unpack delimiter and compare
            delimiter, = struct.unpack("!H", data[-2:])  # Assuming network (big-endian) byte order
            if delimiter != EXPECTED_DELIMITER:
                find_delimiter(sock)
                continue
            else:
                ecu8_data.delimiter = delimiter  # Now correctly assigned
            
            # Extract CRC and compare
            ecu8_data.crc = int.from_bytes(data[-6:-2], byteorder='big')  # Assuming CRC is before the last 2 bytes (delimiter)
            calculated_crc = zlib.crc32(data[:-6]) & 0xFFFFFFFF  # Excluding CRC and delimiter from calculation
            if ecu8_data.crc != calculated_crc:
                print(f"CRC error: received: {ecu8_data.crc:#x}, calculated: {calculated_crc:#x}")
                continue

            #Extract `dev`
            for i in range(ADBMS6830_NUMBER_OF_NODES):
                ecu8_data.dev.append(data[i])

            #Extract `dev` and `body_cnt`
            ecu8_data.body_cnt = data[ADBMS6830_NUMBER_OF_NODES]

            #Extract data
            data_body_data = data[ADBMS6830_NUMBER_OF_NODES+1:-6]  # Skipping header, CRC, and delimiter
            for i in range(ecu8_data.body_cnt):
                offset = i * 3  # Each ECU8TR_DATA_BODY_t is 3 bytes
                body_segment = data_body_data[offset:offset + 3]
                reg, reg_value = struct.unpack("!BH", body_segment)  # Unpack each data body
                ecu8_data.data_bodies.append((reg, reg_value))           
            
            return ecu8_data
        
        except ValueError as e:
            print(e)
            return None

def receive_tle9012_data(sock):

    ecu8_data = ECU8TRData_TLE9012()
    sock.settimeout( 120 ); # 20 minutes

    while True:
        try:
            # Header
            data = sock.recv(TOTAL_TLE9012_DATA_SIZE)
            if not data:
                # Server has closed the connection
                raise ValueError("Incomplete header received, the server closed the socket")
                
            # Unpack delimiter and compare
            delimiter, = struct.unpack("!H", data[-2:])  # Assuming network (big-endian) byte order
            if delimiter != EXPECTED_DELIMITER:
                find_delimiter(sock)
                continue
            else:
                ecu8_data.delimiter = delimiter  # Now correctly assigned
            
            # Extract CRC and compare
            ecu8_data.crc = int.from_bytes(data[-6:-2], byteorder='big')  # Assuming CRC is before the last 2 bytes (delimiter)
            calculated_crc = zlib.crc32(data[:-6]) & 0xFFFFFFFF  # Excluding CRC and delimiter from calculation
            if ecu8_data.crc != calculated_crc:
                print(f"CRC error: received: {ecu8_data.crc:#x}, calculated: {calculated_crc:#x}")
                continue

            # Extract `dev` and `body_cnt`
            ecu8_data.dev, ecu8_data.body_cnt = struct.unpack("!BB", data[:2])

            data_body_data = data[2:-6]  # Skipping header, CRC, and delimiter
            for i in range(ecu8_data.body_cnt):
                offset = i * 3  # Each ECU8TR_DATA_BODY_t is 3 bytes
                body_segment = data_body_data[offset:offset + 3]
                reg, reg_value = struct.unpack("!BH", body_segment)  # Unpack each data body
                ecu8_data.data_bodies.append((reg, reg_value))           
            
            return ecu8_data
        
        except ValueError as e:
            print(e)
            return None


class DataReceptionError(Exception):
    pass

        
def get_physical( reg, digital ):
#adbms6830
    if reg >=0x00 and reg<= 0x10:   #Cell voltages
        if digital >= 0 and digital < 55500:
            factor = digital & 0xFFFF
        elif digital >= 55500 and digital <= 65535:
            factor = digital - 0x10000
        return( ((factor) * (ADBMS6830_CELL_V_LSB/ADBMS6830_VOLT_SCALING)) + ADBMS6830_CELL_CONVERSION_FACTOR)
    if reg == 0x11:         #ITMP
        factor = (((digital * (150/1000000)) ) + 1.5)
        return (((factor /(7.5/1000))) - 273)
#tle9012
    if reg >= 0x19 and reg <= 0x24: #CVM
        return( digital * CELL_MEASUREMENT_FACTOR )
    if reg == 0x28: #BVM
        return( digital * BLOCK_MEASUREMENT_FACTOR )
    if reg == 0x30: #Internal temp
        return get_internal_temp( digital )
    if reg >= 0x29 and reg <= 0x2d:
        return get_ext_temp( digital )
    return 0.0

def process_adbms6830_data( ecu8tr_data ):
    # If CRC and delimiter are valid, print the data
    k=0
    print( "" )
    #print(f"Body Count: {ecu8tr_data.body_cnt}")
    for i, body in enumerate(ecu8tr_data.data_bodies, start=ADBMS6830_NUMBER_OF_NODES):
        if(k%(ecu8tr_data.body_cnt/ADBMS6830_NUMBER_OF_NODES)) == 0:
            print(f"ADBMS6830 Device: {ecu8tr_data.dev[int(k/(ecu8tr_data.body_cnt/ADBMS6830_NUMBER_OF_NODES))]}")
        k+=1
        reg = body[0]
        digital = body[1]
        phy = get_physical( reg, digital )
        if reg >=0x01 and reg<= 0x10:
            if digital == 0:
                print(f"Cell#{reg}: Raw Reading= {digital}, Calculated Cell Voltage= N/A")
            else:
                print(f"Cell#{reg}: Raw Reading= {digital}, Calculated Cell Voltage= {phy}mV")
        elif reg == 0x11:
            print(f"ITMP: Raw Reading= {digital}, Calculated ITMP= {phy}C\n")
        else:
            print( "Error: Unidentified register number\r\n" )
    print( "" )

def process_tle9012_data( ecu8tr_data ):
   # If CRC and delimiter are valid, print the data

    print( "" )
    print(f"Device: {ecu8tr_data.dev}")
    print(f"Body Count: {ecu8tr_data.body_cnt}")
    for i, body in enumerate(ecu8tr_data.data_bodies, start=1):
        reg = body[0]
        digital = body[1]
        phy = get_physical( reg, digital )
        print( f"Data Body {i}: Reg = {hex(reg)}, Reg Value = {digital:#06x}, phy = {phy}" )
    print( "" )


if __name__ == "__main__":

    sock = connect_to_server(IP, PORT)
    if sock:
        try:
            while True:
                if CELL_MONITOR_SELECT == ADBMS6830:
                    ecu8_data = receive_adbms6830_data(sock)
                elif CELL_MONITOR_SELECT == TLE9012:
                    ecu8_data = receive_tle9012_data(sock)
                if ecu8_data is None:
                    raise DataReceptionError("Failed to receive data correctly.")
                if CELL_MONITOR_SELECT == ADBMS6830:
                    process_adbms6830_data( ecu8_data )
                elif CELL_MONITOR_SELECT == TLE9012:
                    process_tle9012_data( ecu8_data )
        except DataReceptionError as e:
            if DEBUG:
                raise # re-raise the exception and track back gets printed
            else:
                print("{}: {}".format(type(e).__name__, e))
                
            print(f"Error: {e}")                
        finally:
            sock.close()
            print( "Server socket closed" )
