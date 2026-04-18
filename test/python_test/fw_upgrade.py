import socket
import zlib
import sys
import os
import time
import argparse  


def wait_for_ok_response( sock, timeout ):
    try:
        sock.settimeout(timeout)  # Timeout after 5 seconds
        response, _ = sock.recvfrom(1024)  
        if response.decode() != "OK":
            print("Unexpected response received. Exiting...")
            exit(1)
    except socket.timeout:
        print("No response received. Exiting...")
        exit(1)
    finally:
        sock.settimeout(None)  # Remove timeout



def calculate_crc(data):
    return zlib.crc32(data) & 0xFFFFFFFF

def send_initial_header(udp_socket, address, total_length, total_blocks):
    header = b'NC'  # Start upgrade command
    total_len_bytes = total_length.to_bytes(4, byteorder='big')
    total_blocks_bytes = total_blocks.to_bytes(4, byteorder='big')
    crc_data = total_len_bytes + total_blocks_bytes
    crc = calculate_crc(crc_data)
    initial_payload = header + crc.to_bytes(4, byteorder='big') + crc_data
    udp_socket.sendto(initial_payload, address)
    wait_for_ok_response( udp_socket, 80 )

def send_firmware_chunk(udp_socket, address, chunk, seq_num):
    crc = calculate_crc(chunk)
    payload = seq_num.to_bytes(4, byteorder='big') + crc.to_bytes(4, byteorder='big') + chunk
    udp_socket.sendto(payload, address)
    wait_for_ok_response(udp_socket, 5)

def send_final_swap_command(udp_socket, address):
    command = b"NC-DO-SWAP"
    # No data is appended after the command, so we just calculate CRC for the command
    #crc = calculate_crc(command)
    #final_payload = command + crc.to_bytes(4, byteorder='big')
    final_payload = command
    udp_socket.sendto(final_payload, address)
    wait_for_ok_response(udp_socket, 3)
    print( "" )
    print( "Upgrading the firmware successfully!" )

def password_protected_file_input():
    print( "You should know what you are doing. Flashing incorrect image will brick the box. Only binary file is accecpted(*.bin)" )
    # Prompt the user for a password
    password = input("Please enter the password: ")
    
    # Check if the password matches the expected value
    if password == "NC_ECU8TR_UPGRADE":
        # If the password matches, ask for the filename
        filename = input("Password correct. Please enter the firmware path and name: ")
        if filename.endswith(".bin"):
            return filename
        else:
            print( "Invalid firmware name, only *.bin is accecpted" )
            exit( 1 )
    else:
        # If the password does not match, exit the function
        print("Incorrect password. Exiting...")
        exit( 1 )



def main():
    
    firmware_path = password_protected_file_input()

    udp_ip = '192.168.1.200'  # Board IP address
    udp_port = 8100  # Port on which the board is listening
    chunk_size = 256  # Adjust based on your bootloader's expected chunk size
    

    udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    address = (udp_ip, udp_port)

    total_length = os.path.getsize(firmware_path)
    total_blocks = (total_length + chunk_size - 1) // chunk_size  # Round up division

    # Send initial header
    print( "Switching to the bootloader, will take some time..." )
    send_initial_header(udp_socket, address, total_length, total_blocks)

    with open(firmware_path, 'rb') as firmware_file:
        seq_num = 1  # Starting sequence number for the firmware chunks
        while True:
            chunk = firmware_file.read(chunk_size)
            if not chunk:
                break  # End of file
            send_firmware_chunk(udp_socket, address, chunk, seq_num)            
            print( f"sending {seq_num} of the {total_blocks}", end="\r" )
            seq_num += 1
    
    send_final_swap_command( udp_socket, address )
    udp_socket.close()


if __name__ == '__main__':
    main()
