# file:      docan.py
# author:    rmilne
# version:   0.1.0
# date:      Aug2020
# brief:     UDS (ISO 14229-1) via CAN (ISO 15765-2) using Kvaser pycanlib

# Attention
# COPYRIGHT 2020 Neutron Controls
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either expressed or implied.
#
#  !DO NOT DISTRIBUTE!
#  For evaluation only.
#  Extended license agreement required for distribution and use!

import os
import sys
import threading
import logging
import queue
import time
from enum import Enum

os.system( "" )

# Upper 16-bits of CAN address with SAE J1939 format
# (ISO 15765-2:2016(E) A.2.1)
# for Normal addressing, physical addressed messages
BASE_ID_EXT = 0x18da0000    #ECU8
#BASE_ID_EXT = 0x12010000    #ECU8TR
# Padding that minimizes stuff-bit insertions
PADDING = 0xcc
# Set logging level
logging.basicConfig(filename='', level=logging.INFO)

class RxState(Enum):
    # Rx thread state
    IDLE = 0
    FF = 1
    CF = 2
    DONE = 3
    OVF = 4


class DoCAN(object):
    TX_DIR = 0x01
    RX_DIR = 0x00

    def __init__(self, source_address, target_address, can, is_async=False):
        if source_address < 0 or source_address > 0xff:
            raise ValueError('0 <= sa <= 0xff')
        if target_address < 0 or target_address > 0xff:
            raise ValueError('0 <= ta <= 0xff')
        self.sa = source_address
        self.ta = target_address
        self.is_async = is_async
        self.candrv = can
        self.cf_index = 0
        self.seg_length = 0
        self.raw_output = False

        # Command dictionary
        self.cmd_dict = {}
        self.rx_q_buf = queue.Queue(16)

        if is_async:
            self.tx_q_buf = queue.Queue(16)
            self.tx_thread = tx_thread(self)
            self.tx_thread.start()

        self.candrv.rx_register(self.sa, self.queue_rx_msg)

    def GenCanId(self, targ_addr):
        return BASE_ID_EXT + (targ_addr << 8) + self.sa

    def AddCmd(self, key, val):
        if key in self.cmd_dict:
            raise ValueError('cmd_dict key assigned')
        else:
            self.cmd_dict[key] = val

    def DelCmd(self):
        self.cmd_dict.clear()

    def ExecuteDict(self, cmd_key):
        if self.is_async:
            try:
                self.tx_q_buf.put(self.cmd_dict[cmd_key])
            except queue.Full:
                return False
            return True
        else:
            return self.ExecuteWait(self.cmd_dict[cmd_key])

    def Exe(self, cmd):
        if self.is_async:
            try:
                self.tx_q_buf.put(cmd)
            except queue.Full:
                return False
            return True
        else:
            return self.ExecuteWait(cmd)

    '''
    Synchronous execution of command implies successive command execution.
    ExecuteWait will not return until completion or timeout of a command.
    '''
    def ExecuteWait(self, cmd_obj):
        # Execute prepare func of command
        if not cmd_obj.prepare():
            logging.error("{} prepare error".format(cmd_obj))
        else:
            # Packetize for CAN protocol
            frames = self.Packetize(cmd_obj.tx_data, self.candrv.is_fd)

            if len(frames):
                for frame in frames:
                    try:
                        id = self.GenCanId(self.ta)
                        canframe = self.candrv.make_frame(id, frame)
                        self.candrv.write(canframe, 100)
                        if frame[0] & 0xf0 == 0x00:
                            # Exit for loop if SF
                            break
                        if frame[0] & 0xf0 == 0x10:
                            # logging.info("Wait for FC")
                            fc_wait = .1
                            while True:
                                # FF write followed by wait for FC from target
                                # fc_wait True|False => timeout|no-timeout
                                # TODO: handle non-zero FC BlockSize?
                                wait_time = self.WaitFlowControl(fc_wait)
                                if wait_time == 0:
                                    # Target will issue another FC when ready
                                    fc_wait = 1
                                else:
                                    break

                    except Exception as e:
                        logging.error("tx_thread send exception:{}"
                                      .format(e.args[0]))
                        break

                if cmd_obj.timeout == 0:
                    logging.debug("cmd has no expected response")
                    # No response expected
                    return True

                # Rx process
                start = time.time()
                rx_state = RxState.IDLE
                while True:
                    try:
                        # Rx signalled or timeout (False = non-blocking)
                        msg = self.rx_q_buf.get(True, cmd_obj.timeout)
                    except queue.Empty:
                        # Rx timeout - quit rx process
                        logging.error("========SA:0x{:02x} rx timeout:{}"
                                      .format(self.sa, cmd_obj.timeout))
                        break

                    # Unpacketize is a state machine for segmented CAN messages
                    if len(msg.data):
                        try:
                            rx_state, rx_data = self.Unpacketize(rx_state,
                                                                 msg.data)
                            dataStr = " ".join("0x{:02X}".format(x) for x in
                                              rx_data)
                            logging.info("{}".format(dataStr))
                            cmd_obj.rx_data += rx_data
                            if rx_state == RxState.DONE:
                                # Complete message received
                                logging.debug("RxState.DONE")
                                break
                            elif rx_state == RxState.FF:
                                # Send FC with ContinueToSend flag and with no
                                # BlockSize restriction
                                logging.debug("RxState.FF")
                                self.SendFlowControl(self.ta)
                            elif rx_state == RxState.OVF:
                                # Overflow on target
                                logging.error("target rx overflow")
                                break
                            else:
                                # CF
                                logging.debug("RxState.CF")
                                end = time.time()
                                if (end - start) > cmd_obj.timeout:
                                    # Timeout
                                    logging.error("SA:0x{:02x} rx timeout CF"
                                                  .format(self.sa,))
                                    break
                        except ValueError as ve:
                            # Protocol formatting problem
                            logging.error("{} unpacketize err:{}"
                                          .format(cmd_obj, ve))
                            break
                        except Exception as unpack_e:
                            logging.error("{} unpacketize exception: {}"
                                          .format(cmd_obj, unpack_e))
                            break
                try:
                    if not cmd_obj.validate():
                        logging.error("{} validate fail".format(cmd_obj))
                    else:
                        if cmd_obj.callback:
                            try:
                                cmd_obj.callback(cmd_obj)
                                return True
                            except ValueError as verr:
                                logging.error("{} callback err:{}"
                                              .format(cmd_obj, verr))
                            except Exception as cb_e:
                                logging.error("{} callback exception: {}"
                                              .format(cmd_obj, cb_e))
                        else:
                            return True
                except Exception as val_e:
                    logging.error("{} validate exception: {}"
                                  .format(cmd_obj, val_e))
        return False

    def udsSendReceive(self, raw_frame, time_out = 1 ):

        try:
            frames = self.Packetize(raw_frame, self.candrv.is_fd)
        except:
            self.candrv.running = False
            return bytearray()

        rxFrame = bytearray()

        if len(frames):
            for frame in frames:
                try:
                    id = self.GenCanId(self.ta)
                    canframe = self.candrv.make_frame(id, frame)
                    self.__debug_print( DoCAN.TX_DIR, canframe )
                    self.candrv.write(canframe, 100)
                    if frame[0] & 0xf0 == 0x00:
                        # Exit for loop if SF
                        break
                    if frame[0] & 0xf0 == 0x10:
                        # logging.info("Wait for FC")
                        fc_wait = 1
                        while True:
                            # FF write followed by wait for FC from target
                            # fc_wait True|False => timeout|no-timeout
                            # TODO: handle non-zero FC BlockSize?
                            wait_time = self.WaitFlowControl(fc_wait)
                            if wait_time == 0:
                                # Target will issue another FC when ready
                                fc_wait = 1
                            else:
                                break

                except Exception as e:
                    logging.error("tx_thread send exception:{}"
                                    .format(e.args[0]))
                    break

            if time_out == 0:
                logging.debug("cmd has no expected response")
                # No response expected
                return bytearray()

            # Rx process
            start = time.time()
            rx_state = RxState.IDLE
            while True:
                try:
                    # Rx signalled or timeout (False = non-blocking)
                    msg = self.rx_q_buf.get(True, time_out )
                except queue.Empty:
                    # Rx timeout - quit rx process
                    logging.error("--------SA:0x{:02x} rx timeout:{}"
                                    .format(self.sa, time_out ))
                    break

                # Unpacketize is a state machine for segmented CAN messages
                if len(msg.data):
                    try:
                        rx_state, rx_data = self.Unpacketize(rx_state,
                                                                msg.data)
                        dataStr = " ".join("0x{:02X}".format(x) for x in
                                          rx_data)
                        # logging.info("{}".format(dataStr))
                        # print("DOCAN: udsSendReceive: if len(msg.data): msg.data: ",msg.data)
                        # print("DOCAN: udsSendReceive: rx_state: ",rx_state)
                        # print("DOCAN: udsSendReceive: rx_data: ",rx_data)
                        # print("DOCAN: udsSendReceive: ",dataStr)
                        rxFrame += rx_data
                        if rx_state == RxState.DONE:
                            # Complete message received
                            logging.debug("RxState.DONE")
                            # print("RxState.DONE")
                            break
                        elif rx_state == RxState.FF:
                            # Send FC with ContinueToSend flag and with no
                            # BlockSize restriction
                            logging.debug("RxState.FF")
                            # print("RxState.FF")
                            self.SendFlowControl(self.ta)
                        elif rx_state == RxState.OVF:
                            # Overflow on target
                            logging.error("target rx overflow")
                            break
                        else:
                            # CF
                            # print("RxState.CF")
                            logging.debug("RxState.CF")
                            end = time.time()
                            if (end - start) > time_out:
                                # Timeout
                                logging.error("SA:0x{:02x} rx timeout CF"
                                                .format(self.sa,))
                                break
                    except ValueError as ve:
                        # Protocol formatting problem
                        #logging.error("{} unpacketize err:{}"
                        #               .format(cmd_obj, ve))
                        print("DOCAN: udsSendReceive: except ValueError")
                        break
                    except Exception as unpack_e:
                       # logging.error("{} unpacketize exception: {}"
                       #                 .format(cmd_obj, unpack_e))
                        break

        return rxFrame

    def SendCANUniversalMessage(self, message_CANID, tx_data):
        '''
        Send a standard universal message        
        '''
        #tx_data = [0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]        
        #ecu8tr_addr = 0x12010a0b
        try:
            #id = self.GenCanId(targ_addr)
            canframe = self.candrv.make_frame(message_CANID, tx_data)

            #if self.debug:
            #    self._print(id, tx_data)
            self.candrv.write(canframe, 100)

        except Exception as e:
            raise ValueError("SendFlowControl exception: {}".format(e.args[0]))

    def PeekCANMessage(self, timeout):
            '''
            Wait for a CAN message from the target
            '''
            try:
                frame = self.rx_q_buf.get(True, timeout)
                self.DebugPrintRXFrame(frame,80)

            except queue.Empty:
                raise ValueError("Peek CAN message wait timeout")

    def DebugPrintRXFrame(frame, width):
        form = '═^' + str(width - 1)
        print(format(" Frame received ", form))
        print("id:", frame.id)
        print("data:", bytes(frame.data))
        print("dlc:", frame.dlc)
        print("flags:", frame.flags)
        print("timestamp:", frame.timestamp)

    def SendFlowControl(self, targ_addr):
        '''
        Send a flow control frame
        flag (lower nibble of first byte) = 0 (ContinueToSend)
        BlockSize = 0 (No further FC frames required)
        STmin = 2 milliseconds
        '''
        tx_data = [0x30, 0, 2, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc]
        try:
            id = self.GenCanId(targ_addr)
            canframe = self.candrv.make_frame(id, tx_data)

            #if self.debug:
            #    self._print(id, tx_data)
            self.candrv.write(canframe, 100)

        except Exception as e:
            raise ValueError("SendFlowControl exception: {}".format(e.args[0]))

    def WaitFlowControl(self, timeout):
        '''
        Wait for a flow control frame from the target
        '''
        try:
            frame = self.rx_q_buf.get(True, timeout)
            if frame.data[0] & 0xf0 != 0x30:
                raise ValueError("Invalid FC PCI")

            flag = frame.data[0] & 0x0f
            if flag == 0:
                # Wait time defined by FC STmin value
                return frame.data[2]
            elif flag == 1:
                # Wait until next FC issued by target
                return 0
            else:
                raise ValueError("FC overflow")

        except queue.Empty:
            raise ValueError("FC wait timeout")

    def WaitExecuteQueueComplete(self):
        if self.is_async:
            self.tx_q_buf.join()

    def GetFdDataLength(self, len):
        if len < 9:
            return len
        elif len < 13:
            return 12
        elif len < 17:
            return 16
        elif len < 21:
            return 20
        elif len < 25:
            return 24
        elif len < 33:
            return 32
        elif len < 49:
            return 48
        else:
            return 64

    def queue_rx_msg(self, msg):
        '''
        Rx CAN frames that pass candrv.rx_register identification arrive here
        '''
        # tx thread dequeues message for unpacketization
        self.__debug_print( DoCAN.RX_DIR, msg )
        self.rx_q_buf.put(msg)

    '''
    Packetization of bytes as a collection of CAN frames as per ISO 15756-2

    Network protocol control information (N_PCI)
    ===========================================================================
    Byte Offset | 0 (bit 7-4) | 0 (bit 3-0)  | 1           | 2      |    ...
    ===========================================================================
    Single      | 0           | size (0-7)   | Data A      | Data B | Data C ..
    Single (FD) | 0           | 0            | size (0-62) | Data A | Data B ..
    First (<4K)	| 1           | size (12 bits)             | Data A | Data B ..
    First (>4K)	| 1           | 0            | 0           | size (bytes 2-6)
    Consecutive | 2           | index (0-15) | Data A      | Data B | Data C ..
    Flow Ctrl   | 3           | flag (0-2)   | Block size  | STmin  | ...

    FC flag:
    0 - Continue to send (BS == 0 -> send all CFs; BS != 0 -> max CFs before
    next FC)
    1 - Wait for next FC with flag == 0 (ignore BS, STmin)
    2 - Overflow on target - abort transmission

    NB: unused data bytes are padded with 0xCC value
    '''
    def Packetize(self, tx_list, is_fd):
        can_frame = []
        can_frame_list = []
        tx_list_size = len(tx_list)

        if not is_fd:
            # CAN Classic
            # Init CAN message data to 0xcc
            can_frame = [PADDING] * 8
            if tx_list_size <= 7:
                # SF (single frame)
                # Set lower nibble of first byte with length of payload
                can_frame[0] = tx_list_size
                # Copy payload to frame starting at second byte
                can_frame[1:tx_list_size + 1] = tx_list[0:tx_list_size]
                can_frame_list.append(can_frame)
                return can_frame_list

            # Segmented frames if data length>7 => 1*FF + n*CF
            frame_idx = 0
            while len(tx_list) > 0:
                if frame_idx == 0:
                    # FF (first frame)
                    if tx_list_size > 4095:
                        # Extended length placed in bytes 2-6
                        can_frame[0] = 0x10
                        can_frame[1] = 0
                        can_frame[2] = (tx_list_size >> 24) & 0xff
                        can_frame[3] = (tx_list_size >> 16) & 0xff
                        can_frame[4] = (tx_list_size >> 8) & 0xff
                        can_frame[5] = tx_list_size & 0xff
                    else:
                        # Put upper 4 bits of 12-bit length into lower nibble
                        # of first byte
                        can_frame[0] = 0x10 + ((tx_list_size >> 8) & 0x0f)
                        can_frame[1] = tx_list_size & 0xff
                        # Fill remainder of first frame with data
                        can_frame[2:8] = tx_list[:6]
                        # Set intial index value for first CF
                        frame_idx = 1
                        # Strip off copied raw data from front of array
                        tx_list = tx_list[6:]
                else:
                    # CF (consecutive frame)
                    can_frame = bytearray( [0xCC] * 8 )
                    can_frame[0] = 0x20 + (frame_idx % 16)
                    # Increment index for next CF
                    frame_idx = frame_idx + 1
                    # Copy data into CF
                    if len(tx_list) >= 7:
                        for idx in range(0, 7):
                            can_frame[idx + 1] = tx_list[idx]
                        # Strip off copied raw data from front of array
                        tx_list = tx_list[7:]
                    else:
                        for idx in range(0, len(tx_list)):
                            can_frame[idx + 1] = tx_list[idx]
                        tx_list.clear()
                # Append frame to collection of can_frame_list
                can_frame_list.append(can_frame[:])

        else:
            # CANFD
            frame_size = 0
            if tx_list_size <= 62:
                # SF (single frame)
                frame_size = self.GetFdDataLength(tx_list_size + 2)
                # Init CAN message data to 0xcc
                can_frame = [PADDING] * frame_size
                # Set first byte to zero to indicate FF using CANFD
                can_frame[0] = 0
                # Place length of payload in second byte
                can_frame[1] = tx_list_size
                # Copy payload to frame starting at second byte
                can_frame[2:tx_list_size + 2] = tx_list[0:tx_list_size]
                can_frame_list.append(can_frame)
                return can_frame_list

            # Segmented frames if data length>7 => 1*FF + n*CF
            frame_idx = 0
            while len(tx_list) > 0:
                if frame_idx == 0:
                    # FF (first frame)
                    if tx_list_size > 4095:
                        # Extended length placed in bytes 2-6
                        can_frame[0] = 0x10
                        can_frame[1] = 0
                        can_frame[2] = (tx_list_size >> 24) & 0xff
                        can_frame[3] = (tx_list_size >> 16) & 0xff
                        can_frame[4] = (tx_list_size >> 8) & 0xff
                        can_frame[5] = tx_list_size & 0xff
                    else:
                        can_frame.append(0x10 + ((tx_list_size >> 8) & 0x0f))
                        can_frame.append(tx_list_size & 0xff)
                        # Fill remainder of first frame with raw data
                        can_frame.extend(tx_list[:62])
                        # Strip off copied raw data from front of array
                        tx_list = tx_list[62:]
                else:
                    # CF (consecutive frame)
                    frame_size = self.GetFdDataLength(len(tx_list) + 1)
                    can_frame = [PADDING] * frame_size
                    can_frame[0] = 0x20 + (frame_idx % 16)
                    # Increment index for next CF
                    frame_idx = frame_idx + 1
                    # Copy data into CF
                    if len(tx_list) >= frame_size - 1:
                        for idx in range(0, frame_size - 1):
                            can_frame[idx+1:] = tx_list[idx]
                        # Strip off copied raw data from front of array
                        tx_list = tx_list[frame_size - 1:]
                    else:
                        for idx in range(0, len(tx_list)):
                            can_frame[idx + 1] = tx_list[idx]
                        tx_list.clear()
                # Append frame to collection of can_frame_list
                can_frame_list.append(can_frame[:])

        return can_frame_list

    def Unpacketize(self, state, rx_data):
        size = 0
        rx_len = len(rx_data)
        if rx_len == 0:
            # Shouldn't happen if caller tests for this condition
            raise ValueError('rx_frame with no data')

        # Get protocol control information
        pci = rx_data[0] & 0xf0

        if state == RxState.IDLE:
            if pci == 0x00:
                # SF (single frame)
                if rx_data[0] == 0:
                    # CANFD SF if lower nibble of PCI byte is zero
                    size = rx_data[1]
                    if size > 62:
                        raise ValueError('SF fd size')
                    if size + 2 > rx_len:
                        raise ValueError('SF fd dlc')
                    return RxState.DONE, rx_data[2:size + 2]
                else:
                    # CAN Classic
                    size = rx_data[0]
                    if size > 7:
                        raise ValueError('SF classic size')
                    if size + 1 > rx_len:
                        raise ValueError('SF classic dlc')
                    return RxState.DONE, rx_data[1:size + 1]

            elif pci == 0x10:
                # FF (first frame) - PCI data is two bytes
                if rx_len < 2:
                    raise ValueError('FF dlc')

                # Prepare index value for expected index value of first CF
                self.cf_index = 1
                if rx_data[0] == 10 and rx_data[1] == 0:
                    # FF >= 4K
                    if rx_len < 6:
                        raise ValueError('FF ext dlc')
                    self.seg_length = rx_data[2] << 24
                    self.seg_length += rx_data[3] << 16
                    self.seg_length += rx_data[4] << 8
                    self.seg_length += rx_data[5]
                    logging.debug("seg_length:{}".format(self.seg_length))
                    # FF state return value prompts caller to send FC frame
                    return RxState.FF, []
                else:
                    # FF < 4K
                    if rx_len < 2:
                        raise ValueError('FF dlc')
                    self.seg_length = ((rx_data[0] & 0x0f) << 8) + rx_data[1]
                    if self.seg_length < (rx_len - 2):
                        raise ValueError('FF invalid length')
                    # Reduce total length of data by amount returned in FF
                    self.seg_length = self.seg_length - (rx_len - 2)
                    return RxState.FF, rx_data[2:rx_len]

            elif pci == 0x30:
                # FC (flow control frame) - ignore if flag == 0
                flag = rx_data[0] & 0x0f
                if flag == 1:
                    return RxState.WAIT, []
                if flag == 2:
                    return RxState.OVF, []
            else:
                raise ValueError('Invalid IDLE state PCI')

        else:
            # state == RxState.FF or state == RxState.CF:
            if pci == 0x20:
                if ((rx_data[0] & 0x0f) == (self.cf_index % 16)):
                    self.cf_index = self.cf_index + 1
                    if self.seg_length > (rx_len - 1):
                        self.seg_length = self.seg_length - (rx_len - 1)
                        return RxState.CF, rx_data[1:rx_len]
                    else:
                        return RxState.DONE, rx_data[1:self.seg_length + 1]
                else:
                    raise ValueError('CF index')
            elif pci == 0x30:
                # FC (flow control frame) - ignore if flag == 0
                flag = rx_data[0] & 0x0f
                if flag == 1:
                    return RxState.WAIT, []
                if flag == 2:
                    return RxState.OVF, []
            else:
                raise ValueError('Invalid IDLE state PCI')

    def dispose(self):
        if self.candrv:
            self.candrv.abort()

        # A dequeue of None will terminate the tx thread
        # flush queues
        # Don't recreate queue since breaks reference to original thread
        # tx_q_buf
        try:
            self.rx_q_buf.queue.clear()
            if self.is_async:
                self.tx_q_buf.queue.clear()
        finally:
            if self.is_async:
                # A dequeue of None will terminate the tx thread
                self.tx_q_buf.put(None)

        logging.debug("disposed BMSLL obj")
        self.cmd_dict.clear()

    def setRawOutput( self, raw_output ):
        self.raw_output = raw_output

    def __debug_print( self, dir, canFrame ):
        '''
        Frame: canFrame
        '''
        if self.raw_output == False:
            return

        if dir == DoCAN.TX_DIR:
            color_code = "\x1b[33m"
        else:
            color_code = "\x1b[32m"

        frame = canFrame.data
        id = canFrame.id
        frameStr = " ".join( "0x{:02X}".format(x) for x in frame )
        print( color_code + "\t\t0x{:04X}\t\t{}\x1b[0m".format( id, frameStr) )

       


class tx_thread(threading.Thread):
    '''
    Tx Thread executes queued commands
    NB: This method will behave as expected for groups of commands that do not
        modify commands already in the tx queue.  Use WaitExecuteQueueComplete
        to allow the tx queue to empty if a command is executed/modified/
        executed.
    '''
    def __init__(self, bms_obj):
        threading.Thread.__init__(self)
        self.bms = bms_obj
        self.rx_state = RxState.IDLE

    def run(self):
        cmd_obj = None
        while True:
            try:
                cmd_obj = self.bms.tx_q_buf.get(True, .2)
            except queue.Empty:
                continue

            if cmd_obj is None:
                # dispose method writes None to queue to signal termination
                logging.debug("joined BMSLL.sa:0x{:02x} tx thread"
                              .format(self.bms.sa,))
                self.bms.tx_q_buf.task_done()
                break

            # Execute prepare func of command
            if not cmd_obj.prepare():
                logging.error("{} prepare error".format(cmd_obj))
            else:
                # Packetize for CAN protocol
                frames = self.bms.Packetize(cmd_obj.tx_data,
                                            self.bms.candrv.is_fd)

                if len(frames):
                    for frame in frames:
                        try:
                            id = self.bms.GenCanId(self.bms.ta)
                            canframe = self.bms.candrv.make_frame(id, frame)
                            self.bms.candrv.write(canframe, 100)
                            if frame[0] & 0xf0 == 0x00:
                                # Exit for loop if SF
                                break
                            if frame[0] & 0xf0 == 0x10:
                                # logging.info("Wait for FC")
                                fc_wait = .1
                                while True:
                                    # FF write followed by wait for FC from
                                    # target
                                    # fc_wait True|False => timeout|no-timeout
                                    # TODO: handle non-zero FC BlockSize?
                                    wait_time = \
                                        self.bms.WaitFlowControl(fc_wait)
                                    if wait_time == 0:
                                        # Target will issue another FC when
                                        # ready
                                        fc_wait = 1
                                    else:
                                        break

                        except Exception as e:
                            logging.error("tx_thread send exception:{}"
                                          .format(e.args[0]))
                            break

                    if cmd_obj.timeout == 0:
                        logging.debug("cmd has no expected response")
                        # No response expected - wait for next tx command
                        continue

                    # Rx process
                    start = time.time()
                    rx_state = RxState.IDLE
                    while True:
                        try:
                            # Rx signalled or timeout (False = non-blocking)
                            msg = self.bms.rx_q_buf.get(True, cmd_obj.timeout)
                        except queue.Empty:
                            # Rx timeout - quit rx process
                            logging.error("SA:0x{:02x} rx timeout:{}"
                                          .format(self.bms.sa,
                                                  cmd_obj.timeout))
                            break

                        # Unpacketize is a state machine for segmented CAN
                        # messages
                        if len(msg.data):
                            try:
                                rx_state, rx_data = \
                                    self.bms.Unpacketize(rx_state, msg.data)
                                # dataStr = " ".join("0x{:02X}".format(x)
                                #                    for x in rx_data)
                                # logging.info("{}".format(dataStr))
                                cmd_obj.rx_data += rx_data
                                if rx_state == RxState.DONE:
                                    # Complete message received
                                    logging.debug("RxState.DONE")
                                    break
                                elif rx_state == RxState.FF:
                                    # Send FC with ContinueToSend flag and
                                    # with no BlockSize restriction
                                    logging.debug("RxState.FF")
                                    self.bms.SendFlowControl(self.bms.ta)
                                elif rx_state == RxState.OVF:
                                    # Overflow on target
                                    logging.error("target rx overflow")
                                    break
                                else:
                                    # CF
                                    logging.debug("RxState.CF")
                                    end = time.time()
                                    if (end - start) > cmd_obj.timeout:
                                        # Timeout
                                        logging.error("SA:0x{:02x} rx timeout \
                                                      CF".format(self.bms.sa,))
                                        break
                            except ValueError as ve:
                                # Protocol formatting problem
                                logging.error("{} unpacketize err:{}"
                                              .format(cmd_obj, ve))
                                break
                            except Exception as unpack_e:
                                logging.error("{} unpacketize exception: {}"
                                              .format(cmd_obj, unpack_e))
                                break
                    try:
                        if not cmd_obj.validate():
                            logging.error("{} validate fail".format(cmd_obj))
                        else:
                            if cmd_obj.callback:
                                try:
                                    cmd_obj.callback(cmd_obj)
                                except ValueError as verr:
                                    logging.error("{} callback err:{}"
                                                  .format(cmd_obj, verr))
                                except Exception as cb_e:
                                    logging.error("{} callback exception: {}"
                                                  .format(cmd_obj, cb_e))
                    except Exception as val_e:
                        logging.error("{} validate exception: {}"
                                      .format(cmd_obj, val_e))

            # Required by join operation in WaitExecuteQueueComplete()
            self.bms.tx_q_buf.task_done()
