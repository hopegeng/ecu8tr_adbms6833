import time
from canlib import canlib
import cantools
# 1. Load your DBC file
DBC_PATH = 'Modified GEN3_0 DBC.dbc'
db = cantools.database.load_file(DBC_PATH, strict=False)

def setup_kvaser(channel=0):
    # Initialize the Kvaser channel
    ch = canlib.openChannel(channel, canlib.canOPEN_ACCEPT_VIRTUAL)
    ch.setBusParams(canlib.canBITRATE_500K) # Set to match your TC3xx bitrate
    ch.busOn()
    print(f"Kvaser Channel {channel} is now ONLINE.")
    return ch

def start_listening(ch):
    print("Listening for BMS messages... Press Ctrl+C to stop.")
    try:
        while True:
            try:
                frame = ch.read(timeout=100)
                
                try:
                    # Attempt to decode
                    decoded_msg = db.decode_message(frame.id, frame.data)
                    msg_definition = db.get_message_by_frame_id(frame.id)
                    
                    print(f"[{time.strftime('%H:%M:%S')}] {msg_definition.name} (0x{frame.id:X}):")
                    for signal, value in decoded_msg.items():
                        print(f"  -> {signal}: {value}")
                    print("-" * 30)
                    
                except cantools.database.errors.DecodeError as e:
                    # This catches the "got 3" error and prints the raw data instead
                    print(f"[{time.strftime('%H:%M:%S')}] Decode Error on ID 0x{frame.id:X}: {e}")
                    print(f"  Raw Data: {frame.data.hex()}")
                    print("-" * 30)
                except KeyError:
                    print(f"Unknown ID: 0x{frame.id:X} Data: {frame.data.hex()}")

            except canlib.canNoMsg:
                continue
    except KeyboardInterrupt:
        print("\nStopping...")
    finally:
        ch.busOff()
        ch.close()

if __name__ == "__main__":
    channel = setup_kvaser(0)
    start_listening(channel)