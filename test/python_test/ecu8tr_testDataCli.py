import socket
import zlib
import sys



# Server address and port
SERVER_IP = '192.168.1.200'
SERVER_PORT = 8001

# Length of the byte stream to receive
DATA_LENGTH = 252
# Length of the CRC32 value
CRC_LENGTH = 4

def main():
    correct_count = 0
    error_count = 0

    # Create a TCP/IP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # Set socket timeout to 5 seconds
    sock.settimeout(5)
    
    # Connect the socket to the server
    try:
        sock.connect((SERVER_IP, SERVER_PORT))
        print("Connected to the server.")
        
        sock.settimeout(20)
        while True:
            # Receive the byte stream
            data = sock.recv(DATA_LENGTH)
            if not data:
                # Server has closed the connection
                print("\nServer has closed the connection.")
                break
            elif len(data) < DATA_LENGTH:
                #print("\nIncomplete data received.")
                error_count += 1
                continue

            # Receive the CRC32 value
            received_crc = sock.recv(CRC_LENGTH)
            if not received_crc:
                # Server has closed the connection
                print("\nServer has closed the connection.")
                break
            elif len(received_crc) < CRC_LENGTH:
                #print("\nIncomplete CRC received.")
                #break
                error_count += 1
                continue

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

    except socket.timeout:
        print("\nSocket operation timed out.")
    except socket.error as e:
        print(f"\nSocket error: {e}")
    except KeyboardInterrupt:
        print("\nProgram terminated by user.")
    finally:
        sock.close()

if __name__ == "__main__":
    main()
