
import socket
import struct
import sys
import threading
import time
from enum import IntEnum

SERVER_IP = "192.168.1.200"
SERVER_PORT = 8000
ISOUART_COMM_FAILURE = 1
ISOUART_COMM_SUCCESS = 0

RED = '\033[91m'
GREEN = '\033[92m'
RESET = '\033[0m'  # Reset the color


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
    CMD_TLE9012_RUNNING_MODE = 10
    CMD_TLE9012_SET_BITWIDTH = 13
    CMD_TLE9012_GET_BITWIDTH = 14
    CMD_RESET = 15
    CMD_HELP = 16
    CMD_EXIT = 17
    CMD_NONE = 18
    CMD_ADBMS6830_WAKEUP = 19
    CMD_ADBMS6830_SLEEP = 20
    CMD_ADBMS6830_RUNNING_MODE = 21

is_exit = False

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
    
    global is_exit

    sock.settimeout(1)  # Non-blocking with timeout
    while is_exit == False:
        try:
            data = sock.recv(1024)
            if data:
                    message = data.decode('utf-8').rstrip('$$')
                    cmd_id = int.from_bytes(data[0:4], byteorder='big')
                    cmd_result = int.from_bytes(data[4:8], byteorder='big')
                    print(f"command ID = {cmd_id}, result = {cmd_result}" )
                    print(f"{message[8:]}")
            else:
                break
        except socket.timeout:
            continue  # Continue listening
        except Exception as e:
            print(f"Error: {e}")
            is_exit = True
            sys.exit( 1 )

def listen_for_commands(sock):

    global is_exit


    command_map = {
        "set_mac": ECU8TR_CMD_e.CMD_NET_SET_MAC,
        "set_ip": ECU8TR_CMD_e.CMD_NET_SET_IP,
        "set_netmask": ECU8TR_CMD_e.CMD_NET_SET_NETMASK,
        "set_gateway": ECU8TR_CMD_e.CMD_NET_SET_GATEWAY,
        "show_network": ECU8TR_CMD_e.CMD_NET_SHOW_NETWORK,
        "tle9012_sleep": ECU8TR_CMD_e.CMD_TLE9012_SLEEP,
        "tle9012_wakeup": ECU8TR_CMD_e.CMD_TLE9012_WAKEUP,
        "tle9012_set_network": ECU8TR_CMD_e.CMD_TLE9012_SET_NETWORK,
        "show_version": ECU8TR_CMD_e.CMD_ECU8TR_VERSION,
        "show_isouart_network": ECU8TR_CMD_e.CMD_TLE9012_GET_NETWORK,
        "show_tle9012_mode": ECU8TR_CMD_e.CMD_TLE9012_RUNNING_MODE,
        "tle9012_set_bitwidth": ECU8TR_CMD_e.CMD_TLE9012_SET_BITWIDTH,
        "show_tle9012_bitwidth": ECU8TR_CMD_e.CMD_TLE9012_GET_BITWIDTH,
        "system_reboot": ECU8TR_CMD_e.CMD_RESET,
        "help": ECU8TR_CMD_e.CMD_HELP,
        "exit": ECU8TR_CMD_e.CMD_EXIT,
        "adbms6830_wakeup": ECU8TR_CMD_e.CMD_ADBMS6830_WAKEUP,
        "adbms6830_sleep": ECU8TR_CMD_e.CMD_ADBMS6830_SLEEP,
        "show_adbms6830_mode": ECU8TR_CMD_e.CMD_ADBMS6830_RUNNING_MODE,
    }

    while is_exit == False:
        try:
            user_input = input(f"Enter command: ")

            """
            if user_input.lower() == 'exit':
                print("Exiting command listener.")
                break
            """

            
            # Split the command and its arguments (if any)
            parts = user_input.split(' ')
            command = parts[0]
            args = ' '.join(parts[1:]) if len(parts) > 1 else ""
            if command in command_map:
                cmd_code = command_map[command]
                # For commands that don't take arguments, ignore any provided args
                if command in ["show_network", "tle9012_sleep", "tle9012_wakeup", "tle9012_mode", "show_tle9012_bitwidth" ]:
                    cmd_body = ""
                elif command in ["exit"]:
                    is_exit = True
                elif command in ["help"]:
                    print( "" )
                    for cmd in command_map:
                        cmd_body = ""
                        print( f"{cmd}" )
                    print( "" )
                else:
                    cmd_body = args
                if is_exit == False:
                    packed_data = pack_ecu8_cmd(cmd_code, cmd_body)
                    sock.sendall(packed_data)
                    print("")
                    time.sleep( 0.4 )

                if command in ["tle9012_set_network"]:  # The response from the setting network is slow
                    time.sleep( 0.6 )

                if command in ["tle9012_set_width"]:  # The response from the setting network is slow
                    time.sleep( 0.6 )

            else:
                print("Invalid command.")

        except Exception as e:
            print(f"Error: {e}")
            is_exit = True
            sys.exit( 1 )

if __name__=="__main__":
    
    # Setup and start threads
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout( 5 )
        sock.connect((SERVER_IP, SERVER_PORT))
    except socket.timeout:
        print("Socket operation timed out")
        sys.exit( 1 )
    except ConnectionRefusedError:
        print("Connection refused by the server")
        sys.exit( 1 )
    except socket.error as err:
        print(f"Socket error: {err}")
        sys.exit( 1 )

    # Start a thread for receiving messages
    recv_thread = threading.Thread(target=receive_message, args=(sock,), daemon=True)
    recv_thread.start()

    # Start a thread for listening to user commands
    cmd_thread = threading.Thread(target=listen_for_commands, args=(sock,), daemon=True)
    cmd_thread.start()

    try:
        while is_exit == False:
            # Keep the main thread alive
            pass
    except KeyboardInterrupt:
        print("Program terminated by user")
        is_exit = True
    finally:
        cmd_thread.join()
        recv_thread.join()        
        sock.close()


