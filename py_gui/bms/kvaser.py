# file:      kvaser.py
# author:    rmilne
# version:   0.1.0
# date:      Aug2020
# brief:     Class to encapsulate Kvaser pycanlib

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

import threading
import queue
from canlib import canlib, Frame
import logging


class Kvaser(threading.Thread):
    """
    kvaser Module
    Asynchronous RX, Synchronous Tx
    """
    def __init__(self, channel_number=0, is_fd=False, debug=False):
        threading.Thread.__init__(self)
        self.lock = threading.Lock()
        self.is_fd = is_fd
        self.debug = debug
        self.rx_q_buf = queue.Queue(16)
        self.running = False
        self.rx_decode = {}
        # J1939 identifier format (29-bit)
        self.frame_flags = canlib.MessageFlag.EXT

        open_flags = 0

        if self.is_fd:
            open_flags = canlib.Open.CAN_FD
            # BMS controller firmware uses fd long and fast
            self.frame_flags |= (canlib.MessageFlag.FDF |
                                 canlib.MessageFlag.BRS)

        try:
            self.ch = canlib.openChannel(channel_number, flags=open_flags)

            chdata = canlib.ChannelData(channel_number)

            if (self.debug):
                logging.info("{ch}.{name} ({ean} / {serial})".format(
                    ch=channel_number,
                    name=chdata.channel_name,
                    ean=chdata.card_upc_no,
                    serial=chdata.card_serial_no,
                ))

            self.ch.setBusOutputControl(canlib.canDRIVER_NORMAL)
            '''
            AURIX TC3xx NBTPi values calculated via IfxCan_Node_setBitTiming()
            from configured baudrate=500000 and sample point=8000 (80%):
            NBTP1 register (0xf020861c) = 0x00070e03
                NTSEG1 = 0xe (15 - 1)
                NTSEG2 = 0x3 (4 - 1)
                NSJW = 0 (1 - 1)
            '''

            if "Kvaser Leaf Light v2" in chdata.channel_name:
                self.ch.setBusParams( canlib.canBITRATE_500K )
            else:
                self.ch.setBusParams( freq=500000, tseg1=15, tseg2=4, sjw=1 )

            if self.is_fd:
                '''
                AURIX TC3xx DBTPi values calculated via
                IfxCan_Node_setFastBitTiming()
                from configured baudrate=2000000 and sample point=8000 (80%):
                DBTP1 register (0xf020860c) = 0x00010e30
                    DTSEG1 = 0xe (15 - 1)
                    DTSEG2 = 0x3 (4 - 1)
                    DSJW = 0 (1 - 1)
                '''
                self.ch.setBusParamsFd(
                    freq_brs=2000000,
                    tseg1_brs=15,
                    tseg2_brs=4,
                    sjw_brs=1)
            # Set DoCAN extended id filter for N_TAtype #5 (classic) or #7 (fd)
            self.ch.canSetAcceptanceFilter(0x18da0000, 0x1fff0000,
                                           is_extended=True)
            self.ch.iocontrol.error_frames_reporting = True
            self.ch.busOn()
            self.running = True
        except canlib.CanNotFound:
            print("Kvaser hw not connected")
            self.ch = None
            raise

    '''
    Rx thread will call message queuing routine according to identifier TA
    field
    '''
    def rx_register(self, identifier, rx_queue_func):
        if identifier not in self.rx_decode:
            self.rx_decode[identifier] = rx_queue_func
            logging.debug("rx_decode:{}".format(self.rx_decode))

    ''' Kill rx thread '''
    def abort(self):
        logging.debug("kvaser abort")
        self.ch.iocontrol.flush_tx_buffer()
        self.running = False
        self.ch.iocontrol.flush_rx_buffer()

    def run(self):
        timeout = 2
        while self.running:
            try:
                rcv_msg = self.ch.read(timeout)
                if rcv_msg.flags & (canlib.MessageFlag.MSGERR_MASK |
                                    canlib.MessageFlag.ERROR_FRAME):
                    logging.error("rx msg err:0x{:x}".format(rcv_msg.flags))
                    self.abort()
                else:
                    # Check if 8-bit TA of rx identifier is registered
                    ta = (rcv_msg.id & 0x0000ff00) >> 8
                    if ta in self.rx_decode:
                        # Queue message with registered queue write routine
                        self.rx_decode[ta](rcv_msg)
                        timeout = 0
                    else:
                        if len(rcv_msg.data):
                            payloadStr = " ".join("0x{:02X}".format(x)
                                                  for x in rcv_msg.data)
                            logging.error("? rx msg id:{} dlc:{} flags:{} data\
                                          :{}".format(rcv_msg.id, rcv_msg.dlc,
                                          rcv_msg.flags, payloadStr))
                        else:
                            logging.error("? rx msg id:{} dlc:{} flags:{}".
                                          format(rcv_msg.id, rcv_msg.dlc,
                                                 rcv_msg.flags))
                        self.rx_decode[ta](rcv_msg)
            except canlib.canNoMsg:
                timeout = 2
            except Exception as ex:
                logging.error("kvaser rx task exception: roland%s", ex.args[0])
                self.running = False
        if (self.debug):
            logging.debug("kvaser rx thread terminated")

    def make_frame(self, frame_id, frame_data):
        '''
        Build CAN frame with flags set in initializer
        '''
        return Frame(id_=frame_id, data=frame_data, flags=self.frame_flags)

    def write(self, canframe, timeout):
        '''
        Parameters:
            frame (canlib.Frame) – Frame containing the CAN data to be sent
            timeout (int) – The timeout, in milliseconds. 0xFFFFFFFF gives an
            infinite timeout.

        '''
        self.lock.acquire()
        try:
            self.ch.writeWait(canframe, timeout)
        except canlib.CanError as e:
            logging.error("canlib write error:{}".format(e.args[0]))
            raise
        finally:
            self.lock.release()

    def read(self, timeout):
        '''
        Parameters:
            timeout (int) – Timeout in milliseconds, -1 gives an infinite
            timeout.
        Returns:
            canlib.Frame
        '''
        try:
            if not self.running:
                raise
            return self.ch.read(timeout)
        except canlib.canNoMsg as ex:
            logging.error("canlib.canNoMsg ex:{}".format(ex.args[0]))
            return None
        except canlib.CanError as e:
            logging.error("canlib read error:{}".format(e.args[0]))
            raise

    def __del__(self):
        '''
        GC disposal of class resources
        '''
        self.rx_q_buf.queue.clear()
        if self.ch:
            self.ch.close()            
