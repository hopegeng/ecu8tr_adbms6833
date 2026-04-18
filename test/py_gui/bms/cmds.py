# file:      cmds.py
# author:    rmilne
# version:   0.1.0
# date:      Aug2020
# brief:     Command objects that interface with BMS controller

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

from enum import Enum, IntEnum
import logging

logging.basicConfig(filename='', level=logging.DEBUG)


class RSID(IntEnum):
    """ Request Session Identifiers """
    IOCBI = 0x2f    # InputOutputControlByIdentifier Request
    RC = 0x31       # RoutineControl Request
    POS_RESP = 0x40
    NEG_RESP = 0x7f


class IoCtlParameter(Enum):
    """ IOCBI Request message InputOutputControlParameter values """
    RCTECU = 0          # returnControlToECU
    RTD = 1             # resetToDefault
    FCS = 2             # freezeCurrentState
    STA = 3             # shortTermAdjustment


class RC_REQ_SUBFUNC(Enum):
    """ RoutineControl Request message sub-function values """
    ISOSAERESRVD = 0    # Reserved
    STR = 1             # startRoutine
    STPR = 2            # stopRoutine
    RRR = 3             # requestRoutineResults


class NRC(IntEnum):
    """ Negative Response Code """
    PR = 0x00  # positiveResponse
    GR = 0x10  # generalReject
    SNS = 0x11  # serviceNotSupported
    SFNS = 0x12  # sub-functionNotSupported
    IMLOIF = 0x13  # incorrectMessageLengthOrInvalidFormat
    RTL = 0x14  # responseTooLong
    BRR = 0x21  # busyRepeatRequest
    CNC = 0x22  # conditionsNotCorrect
    RSE = 0x24  # requestSequenceError
    NRFSC = 0x25  # noResponseFromSubnetComponent
    FPEORA = 0x26  # FailurePreventsExecutionOfRequestedAction
    ROOR = 0x31  # requestOutOfRange
    SAD = 0x33  # securityAccessDenied
    IK = 0x35  # invalidKey
    ENOA = 0x36  # exceedNumberOfAttempts
    RTDNE = 0x37  # requiredTimeDelayNotExpired
    UDNA = 0x70  # uploadDownloadNotAccepted
    TDS = 0x71  # transferDataSuspended
    GPF = 0x72  # generalProgrammingFailure
    WBSC = 0x73  # wrongBlockSequenceCounter
    RCRRP = 0x78  # requestCorrectlyReceived-ResponsePending
    SFNSIAS = 0x7E  # sub-functionNotSupportedInActiveSession
    SNSIAS = 0x7F  # serviceNotSupportedInActiveSession
    RPMTH = 0x81  # rpmTooHigh
    RPMTL = 0x82  # rpmTooLow
    EIR = 0x83  # engineIsRunning
    EINR = 0x84  # engineIsNotRunning
    ERTTL = 0x85  # engineRunTimeTooLow
    TEMPTH = 0x86  # temperatureTooHigh
    TEMPTL = 0x87  # temperatureTooLow
    VSTH = 0x88  # vehicleSpeedTooHigh
    VSTL = 0x89  # vehicleSpeedTooLow
    TPTH = 0x8A  # throttle/PedalTooHigh
    TPTL = 0x8B  # throttle/PedalTooLow
    TRNIN = 0x8C  # transmissionRangeNotInNeutral
    TRNIG = 0x8D  # transmissionRangeNotInGear
    BSNC = 0x8F  # brakeSwitchNotClosed
    SLNIP = 0x90  # shifterLeverNotInPark
    TCCL = 0x91  # torqueConverterClutchLocked
    VTH = 0x92  # voltageTooHigh
    VTL = 0x93  # voltageTooLow


class UDS_Mtype(Enum):
    eMtypeDiagnostics = 0
    eMtypeRemoteDiagnostics = 1


class UDS_N_TAtype(Enum):
    e1_classic_11bit_1to1 = 0
    e2_classic_11bit_1toN = 1
    e3_fd_11bit_1to1 = 2
    e4_fd_11bit_1toN = 3
    e5_classic_29bit_1to1 = 4
    e6_classic_29bit_1toN = 5
    e7_fd_29bit_1to1 = 6
    e8_fd_29bit_1toN = 7


class IocbiType:
    """
    Data structure used by IOCBI command
    Default controlStatusRecord[0] = IoCtlParameter = shortTermAdjustment
    """
    def __init__(self, dataIdentifier1, dataIdentifier2,
                 IoCtlParameter=IoCtlParameter.STA.value,
                 ctlOptionRecord=[]):
        self.dataIdentifier1 = dataIdentifier1
        self.dataIdentifier2 = dataIdentifier2
        self.IoCtlParameter = IoCtlParameter
        self.ctlOptionRecord = ctlOptionRecord
        self.ctlStatusRecord = []


class RcType:
    """ Data structure used by RoutineControl command """
    def __init__(self, subfunction, routineIdentifier1, routineIdentifier2,
                 routineStatusRecord=[]):
        self.subfunction = subfunction
        self.routineIdentifier1 = routineIdentifier1
        self.routineIdentifier2 = routineIdentifier2
        self.routineStatusRecord = routineStatusRecord
        self.routineInfo = -1


class Command(object):
    """ Base command class """
    def __init__(self, sid, callback, timeout):
        self.sid = sid
        self.tx_data = bytearray()
        self.rx_data = []
        self.callback = callback
        self.timeout = timeout
        self.retval = 0

    def prepare(self):
        """ Base class prepare """
        del self.tx_data[:]
        del self.rx_data[:]
        return True

    def validate(self):
        """ Base class validate """
        if len(self.rx_data) > 0:
            if self.rx_data[0] == RSID.NEG_RESP.value:
                # logging.info("NEG_RESP:{}".format(self.rx_data))
                if len(self.rx_data) > 2:
                    try:
                        # Try casting rx_data[2] to NRC enum
                        nrc_from_int = NRC(self.rx_data[2])
                        logging.error("NRC:{} SID:0x{:02x} ".
                                      format(nrc_from_int.name,
                                             self.rx_data[1]))
                    except Exception as ex:
                        # Unknown value of rx_data[1]
                        logging.error("Validate ex:{} NRC:unknown SID:0x{:02x}"
                                      .format(ex.args[0], self.rx_data[1]))
                else:
                    logging.error("NRC")
                return False

            # Positive response always increments SID by 0x40
            if self.rx_data[0] == (self.sid + RSID.POS_RESP.value):
                return True
        return False


class IOCBI(Command):
    """ IOCBI class derived from Command class """
    def __init__(self, iocbi_type, callback=None, timeout=1.0):
        if timeout < 0:
            raise ValueError("Invalid cmd timeout")
        # Invoke base class constructor
        super(IOCBI, self).__init__(RSID.IOCBI.value, callback, timeout)
        self.iocbi_type = iocbi_type
        # IOCBI overhead: SID, dataIdentifier1, dataIdentifier2, IoCtlParameter
        self.RX_OVERHEAD = 4

    # __str__ used for object identification
    def __str__(self):
        return "IOCBI id1[0x{:02x}]id2[0x{:02x}]".format(
            self.iocbi_type.dataIdentifier1,
            self.iocbi_type.dataIdentifier2)

    def prepare(self):
        if super(IOCBI, self).prepare():
            self.tx_data.append(self.sid)
            self.tx_data.append(self.iocbi_type.dataIdentifier1)
            self.tx_data.append(self.iocbi_type.dataIdentifier2)
            self.tx_data.append(self.iocbi_type.IoCtlParameter)
            if len(self.iocbi_type.ctlOptionRecord):
                self.tx_data.extend(self.iocbi_type.ctlOptionRecord)
            self.iocbi_type.ctlStatusRecord.clear()
            return True
        return False

    def validate(self):
        if super(IOCBI, self).validate():
            # Validate length of response data and SID
            if len(self.rx_data) > 3:
                # validate echo of UDS_IOCBIPR_t (SID validated in base class)
                for i in range(1, self.RX_OVERHEAD):
                    if self.rx_data[i] != self.tx_data[i]:
                        logging.error("Invalid IOCBI header")
                        return False
                if len(self.rx_data) > self.RX_OVERHEAD:
                    self.iocbi_type.ctlStatusRecord = self.rx_data[4:]
                return True
            else:
                logging.error("iocbi invalid rx len:{}".
                              format(len(self.rx_data)))
        return False


class RoutineControl(Command):
    """ RoutineControl class derived from Command class """
    def __init__(self, rc_type, callback=None, timeout=1.0):
        if timeout < 0:
            raise ValueError("Invalid cmd timeout")
        # Invoke base class constructor
        super(RoutineControl, self).__init__(RSID.RC.value, callback, timeout)
        self.rc_type = rc_type
        # Value used to index the data of the returned message
        self.RX_OVERHEAD = 5

    # __str__ used for object identification
    def __str__(self):
        return "RC subfunc[{}]rid1[0x{:02x}]rid2[0x{:02x}]".format(
            RC_REQ_SUBFUNC(self.rc_type.subfunction).name,
            self.rc_type.routineIdentifier1,
            self.rc_type.routineIdentifier2)

    def prepare(self):
        if super(RoutineControl, self).prepare():
            self.tx_data.append(self.sid)
            self.tx_data.append(self.rc_type.subfunction)
            self.tx_data.append(self.rc_type.routineIdentifier1)
            self.tx_data.append(self.rc_type.routineIdentifier2)
            if len(self.rc_type.routineStatusRecord):
                self.tx_data.extend(self.rc_type.routineStatusRecord)
            return True
        return False

    def validate(self):
        if super(RoutineControl, self).validate():
            if len(self.rx_data) > 4:
                # validate echo of UDS_RCPR_t (SID validated in base class)
                for i in range(1, 4):
                    if self.rx_data[i] != self.tx_data[i]:
                        logging.error("Invalid RC response")
                        return False
                self.rc_type.routineInfo = self.rx_data[4]
                # if len(self.rx_data) > 5:
                #    self.rc_type.routineStatusRecord = self.rx_data[5:]
                return True
        return False
