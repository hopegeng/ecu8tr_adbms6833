# file:      tle9012.py
# author:    rmilne
# version:   0.1.0
# date:      Sept2020
# brief:     UDS wrappers for TLE9012 API

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

from enum import Enum, IntEnum, unique
from bms.cmds import IOCBI, IocbiType, IoCtlParameter
from functools import wraps
import logging


TLE9012_MAX_DEVICES = 2
TLE9012_MAX_CELLS = 12


@unique
class Cmd(Enum):
    SetCellEnable = 0
    GetCellEnable = 0x1
    SetMaxVoltDropThd = 0x2
    GetMaxVoltDropThd = 0x3
    SetOVoltFltThd = 0x4
    GetOVoltFltThd = 0x5
    SetMinVoltDropThd = 0x6
    GetMinVoltDropThd = 0x7
    SetUVoltFltThd = 0x8
    GetUVoltFltThd = 0x9
    SetExtTempOvertempThd = 0xa
    GetExtTempOvertempThd = 0xb
    SetOtFltCurrSrc = 0xc
    GetOtFltCurrSrc = 0xd
    SetExtTempSensorsUsed = 0xe
    GetExtTempSensorsUsed = 0xf
    SetIntTempOvertempThd = 0x10
    GetIntTempOvertempThd = 0x11
    SetNumConsecErr = 0x12
    GetNumConsecErr = 0x13
    SetExtTempTrigForRR = 0x14
    GetExtTempTrigForRR = 0x15
    SetSleepModeTimingForRR = 0x16
    GetSleepModeTimingForRR = 0x17
    SetRRCounter = 0x18
    GetRRCounter = 0x19
    SetRRSync = 0x1a
    GetRRSync = 0x1b
    SetRRCfgMsk = 0x1c
    GetRRCfgMsk = 0x1d
    SetFltMskCfg = 0x1e
    GetFltMskCfg = 0x1f
    SetGenDiagMsk = 0x20
    GetGenDiagMsk = 0x21
    SetCellUVoltFlg = 0x22
    GetCellUVoltFlg = 0x23
    SetCellOVoltFlg = 0x24
    GetCellOVoltFlg = 0x25
    SetExtTempDiagOpenFlg = 0x26
    GetExtTempDiagOpenFlg = 0x27
    SetExtTempDiagShortFlg = 0x28
    GetExtTempDiagShortFlg = 0x29
    SetExtTempDiagOtFlg = 0x2a
    GetExtTempDiagOtFlg = 0x2b
    SetCellOpenloadFlg = 0x2c
    GetCellOpenloadFlg = 0x2d
    SetCRCRegisterError = 0x2e
    GetCRCRegisterError = 0x2f
    SetExtendWdg = 0x30
    GetExtendWdg = 0x31
    SetActivateSleepMode = 0x32
    GetActivateSleepMode = 0x33
    SetUCurrFltThd = 0x34
    GetUCurrFltThd = 0x35
    SetOCurrFltThd = 0x36
    GetOCurrFltThd = 0x37
    SetBalState = 0x38
    GetBalState = 0x39
    SetAVMExtTempDiagPd = 0x3a
    GetAVMExtTempDiagPd = 0x3b
    SetDiagResMskFlg = 0x3c
    GetDiagResMskFlg = 0x3d
    SetBalDiagOCurr = 0x3e
    GetBalDiagOCurr = 0x3f
    SetBalDiagUCurr = 0x40
    GetBalDiagUCurr = 0x41
    GetCellMeasure = 0x42
    GetBlockMeasure = 0x43
    GetExtTempRes = 0x44
    GetExtTempSrc = 0x45
    GetExtTempPd = 0x46
    GetExtTempValid = 0x47
    GetGPIOInputState = 0x48
    SetGPIOOutputState = 0x49
    GetGPIOOutputState = 0x4a
    SetGPIODir = 0x4b
    GetGPIODir = 0x4c
    GetPWMInputState = 0x4d
    SetPWMEnable = 0x4e
    GetPWMEnable = 0x4f
    SetPWMOutputState = 0x50
    GetPWMOutputState = 0x51
    SetPWMDirection = 0x52
    GetPWMDirection = 0x53
    SetGPIOPwmUVoltErr = 0x54
    GetGPIOPwmUVoltErr = 0x55
    SetPwmPeriodDuty = 0x56
    GetPWMPeriod = 0x57
    GetPWMDutyCycle = 0x58


class CellNum(Enum):
    # cell number used by various registers
    C_0 = 0
    C_1 = 1
    C_2 = 2
    C_3 = 3
    C_4 = 4
    C_5 = 5
    C_6 = 6
    C_7 = 7
    C_8 = 8
    C_9 = 9
    C_10 = 10
    C_11 = 11


class CellEn(Enum):
    # cell enable used by PART_CONFIG
    DISABLE = 0
    ENABLE = 1


class CurrentSrc(Enum):
    # current source used by TEMP_CONFIG
    I0_USED = 0
    I1_USED = 1
    I2_USED = 2
    I3_USED = 3


class ExtTempSensorsUsed(Enum):
    # current source used by TEMP_CONFIG
    NO_EXT_TEMP_SENSE = 0
    TMP0_ACTIVE = 1
    TMP0_1_ACTIVE = 2
    TMP0_1_2_ACTIVE = 3
    TMP0_1_2_3_ACTIVE = 4
    TMP0_1_2_3_4_ACTIVE = 5


class ExtTempTrigForRR(Enum):
    # Ext Temperature triggering used by RR_ERR_CNT
    EVERY_RR = 0
    ONE_MEASURE_1_NO_MEASURE = 1
    ONE_MEASURE_2_NO_MEASURE = 2
    ONE_MEASURE_3_NO_MEASURE = 3
    ONE_MEASURE_4_NO_MEASURE = 4
    ONE_MEASURE_5_NO_MEASURE = 5
    ONE_MEASURE_6_NO_MEASURE = 6
    ONE_MEASURE_7_NO_MEASURE = 7


class RRSync(Enum):
    # Round robin synchronization with watchdog used by PART_CONFIG
    NO_SYNC_WITH_WDG_COUNTER = 0
    SYNC_WITH_WDG_COUNTER = 1


class RRCfg(IntEnum):
    # masks for Round Robin used by RR_CONFIG
    MASK_NR_ERR_COUNTER_ADC_ERROR = 8
    MASK_NR_ERR_COUNTER_OPEN_LOAD_ERROR = 9
    MASK_NR_ERR_COUNTER_EXT_TEMP_ERROR = 10
    MASK_NR_ERR_COUNTER_INT_TEMP_ERROR = 11
    MASK_NR_ERR_COUNTER_UNDERTEMPERATURE_ERROR = 12
    MASK_NR_ERR_COUNTER_OVERTEMPERATURE_ERROR = 13
    MASK_NR_ERR_COUNTER_BALANCING_ERROR_UNDERCURRENT = 14
    MASK_NR_ERR_COUNTER_BALANCING_ERROR_OVERCURRENT = 15


class FltMskCfg(IntEnum):
    ENABLE_ERROR_PIN_FUNCTIONALITY = 5
    EMM_ERR_ADC_ERROR = 6
    EMM_ERR_OPEN_LOAD_ERROR = 7
    EMM_ERR_INTERNAL_IC_ERROR = 8
    EMM_ERR_REGISTER_CRC_ERROR = 9
    EMM_ERR_EXT_TEMP_ERROR = 10
    EMM_ERR_INT_TEMP_ERROR = 11
    EMM_ERR_UNDERTEMPERATURE_ERROR = 12
    EMM_ERR_OVERTEMPERATURE_ERROR = 13
    EMM_ERR_BALANCING_ERROR_UNDERCURRENT = 14
    EMM_ERR_BALANCING_ERROR_OVERCURRENT = 15


class GenDiag(Enum):
   GPIO_WAKEUP_ENABLED = 0
   MASTER_ON_TOP_CONFIG = 1
   BALANCING_ACTIVE = 2
   CVM_BVM_AVM_MEASUREMENT_ACTIVE = 3
   ROUND_ROBIN_ACTIVE = 4
   ENABLE_UNDERTEMPERATURE_INDUCED_SLEEP = 5
   ENABLE_ADC_ERROR = 6
   ENABLE_OPEN_LOAD_ERROR = 7
   ENABLE_INTERNAL_IC_ERROR = 8
   ENABLE_REGISTER_CRC_ERROR = 9
   ENABLE_EXT_TEMP_ERROR = 10
   ENABLE_INT_TEMP_ERROR = 11
   ENABLE_UNDERTEMPERATURE_ERROR = 12
   ENABLE_OVERTEMPERATURE_ERROR = 13
   ENABLE_BALANCING_ERROR_UNDERCURRENT = 14
   ENABLE_BALANCING_ERROR_OVERCURRENT = 15


class UVolt(Enum):
    # undervoltage used by CELL_UV
    NO_UNDERVOLTAGE_IN_CELL = 0
    UNDERVOLTAGE_IN_CELL = 1


class OVolt(Enum):
    # OVolt used by CELL_OV
    NO_OVERVOLTAGE_IN_CELL = 0
    OVERVOLTAGE_IN_CELL = 1


class ExtTemp(Enum):
    # Ext Temperature Measurement used by various registers
    EXT_TMP_IDX_0 = 0
    EXT_TMP_IDX_1 = 1
    EXT_TMP_IDX_2 = 2
    EXT_TMP_IDX_3 = 3
    EXT_TMP_IDX_4 = 4


class Openload(Enum):
    # Openload used by various registers
    NO_OPENLOAD = 0
    OPENLOAD = 1


class ShrtCirc(Enum):
    # TempShortCircuit used by EXT_TEMP_DIAG
    NO_SHORT_CIRCUIT = 0
    SHORT_CIRCUIT = 1


class OTemp(Enum):
    # OTemp used by EXT_TEMP_DIAG
    NO_OVERTEMPERATURE_IN_EXT_TEMP = 0
    OVERTEMPERATURE_IN_EXT_TEMP = 1


class BalSwitchState(Enum):
    # Balancing Driver state used by BAL_SETTINGS #
    DRIVER_OFF = 0
    DRIVER_ON = 1


class AVMExtTempDiag(IntEnum):
    # masks for the auxiliary voltage measurement used by AVM_CONFIG #
    SELECT_EXT_TEMP_DIAGNOSE_FOR_AVM_PULLDOWN_AUX_1_ACTIVE = 2
    SELECT_EXT_TEMP_DIAGNOSE_FOR_AVM_PULLDOWN_AUX_0_ACTIVE = 3
    SELECT_EXT_TEMP_DIAGNOSE_FOR_AVM_PULLDOWN_EXT_TMP_2_ACTIVE = 4
    SELECT_EXT_TEMP_DIAGNOSE_FOR_AVM_PULLDOWN_EXT_TMP_1_ACTIVE = 5
    SELECT_EXT_TEMP_DIAGNOSE_FOR_AVM_PULLDOWN_EXT_TMP_0_ACTIVE = 6
    SELECT_EXT_TEMP_DIAGNOSE_FOR_AVM_NO_PULLDOWN = 7


class AuxVoltDiag(IntEnum):
    # masks for the auxiliary voltage measurement used by AVM_CONFIG #
    DIAGNOSTIC_MEASUREMENT_MASK_EXT_TEMP_0_FOR_AVM = 3
    DIAGNOSTIC_MEASUREMENT_MASK_EXT_TEMP_1_FOR_AVM = 4
    DIAGNOSTIC_MEASUREMENT_MASK_AUX_MEAS_0_FOR_AVM = 6
    DIAGNOSTIC_MEASUREMENT_MASK_AUX_MEAS_1_FOR_AVM = 7
    DIAGNOSTIC_MEASUREMENT_MASK_RESISTOR_FOR_AVM = 9


class DiagnosisResistorMask(Enum):
    # Mask for diagnosis resistor for AVM used by AVM_CONFIG #
    AVM_MEASUREMENT_MASKED_OUT = 0
    AVM_MEASUREMENT_PERFORMED_ON_AVM_START = 1


class Overcurrent(Enum):
    # OTemp used by EXT_TEMP_DIAG #
    NO_OVERCURRENT_IN_CELL = 0
    OVERCURRENT_IN_CELL = 1


class Undercurrent(Enum):
    NO_UNDERCURRENT_IN_CELL = 0
    UNDERCURRENT_IN_CELL = 1


class GpioPwm(Enum):
    # value used to determine which PWM or GPIO is being set #
    GPIO_PWM_0 = 0
    GPIO_PWM_1 = 1


class GpioPwmDir(Enum):
    # value used to determine GPIO output state
    GPIO_PWM_DIRECTION_IN = 0
    GPIO_PWM_DIRECTION_OUT = 1


class TLE9012(object):
    def __init__(self, DevNum):
        if not isinstance(DevNum, int) or \
                (DevNum < 0) or \
                (DevNum > TLE9012_MAX_DEVICES):
            raise ValueError("Invalid device number")
        # Identifier for TLE9012 - Infineon board has eight chained via TLE9015
        self.DevNum = DevNum
        # Separate maps for command and data
        self.CmdData = dict()
        self.CmdObj = dict()

    '''
    Validate Enum value.
    Contained within try-except of docan.tx_thread.
    '''
    def ValidateEnum(self, enum_type, val):
        try:
            return enum_type(val).value
        except Exception as ex:
            raise ValueError('ValidateEnum ex:{} Invalid {} value:{}'.
                             format(ex.args[0], enum_type, val))

    '''
    Validation of value used for an arbitrary sized bitfield.
    Contained within try-except of docan.tx_thread.
    '''
    def ValidateBitfield(self, val, num_bits):
        if isinstance(val, int) and isinstance(num_bits, int) and \
                val >= 0 and num_bits > 0 and val < (1 << num_bits):
            return val
        raise ValueError('Invalid {}-bit value: 0x{:x}'.format(num_bits, val))

    def ValidateBool(self, val):
        if val == True or val == False:
            return val
        if isinstance(val[0], int):
            return val
        raise ValueError('Invalid boolean: {}'.format(val))

    ''' Default IOCBI response callback that displays cmd id + data payload '''
    def DefaultIOCBICallback(self, cmd_obj):
        cmd = IOCBI(cmd_obj)
        if len(cmd.iocbi_type.ctlStatusRecord):
            data_str = ''.join("%02x " % i for i in
                               cmd.iocbi_type.ctlStatusRecord)
            logging.info('DefaultIOCBICallback {} [ '.format(cmd) +
                         data_str + ']')
        else:
            logging.info('DefaultIOCBICallback {}'.format(cmd))

    '''
    Factory method for creating/fetching cmd + data objects.
    Contained within try-except of cli main.
    '''
    def IOCBICmdFactory(self, cmd_enum, data_list, callback, timeout):
        if not isinstance(cmd_enum, Cmd):
            raise ValueError('Invalid cmd_enum value')
        cmd_obj = None
        if cmd_enum.name not in self.CmdObj:
            # Lazy instantiation of command and data objects
            self.CmdData[cmd_enum.name] = \
                IocbiType(0x0, cmd_enum.value, IoCtlParameter.STA.value,
                          data_list)
            cmd_obj = IOCBI(self.CmdData[cmd_enum.name], callback, timeout)
            self.CmdObj[cmd_enum.name] = cmd_obj
        else:
            cmd_obj = self.CmdObj[cmd_enum.name]
            cmd_obj.iocbi_type.ctlOptionRecord = data_list
            cmd_obj.timeout = timeout
            cmd_obj.callback = callback
        if callback is None:
            # Default callback
            cmd_obj.callback = self.DefaultIOCBICallback
        return cmd_obj

    '''
    Validate response data length.
    Contained within try-except of docan.tx_thread.
    '''
    def ValidateResponseDataLength(self, cmd_enum, length):
        if not isinstance(cmd_enum, Cmd) or \
                cmd_enum.name not in self.CmdObj:
            raise ValueError('Invalid command')
        if len(self.CmdObj[cmd_enum.name].iocbi_type.ctlStatusRecord) != \
                length:
            raise ValueError('Invalid {} rx_data length'.format(cmd_enum.name))

    '''
    Decorator for callback functions to check response DevNum.
    Contained within try-except of docan.tx_thread.
    '''
    def IOCBISetCallBackDecorator(func):
        @wraps(func)
        def wrapped(self, cmd_obj):
            # All TLE9012 responses have a min data payload of self.DevNum
            if len(cmd_obj.iocbi_type.ctlStatusRecord) < 1 or \
                    self.DevNum != cmd_obj.iocbi_type.ctlStatusRecord[0]:
                raise ValueError('Response DevNum error')
            return func(self, cmd_obj)
        return wrapped

    '''
    Decorator for callback functions to check response DevNum.
    Contained within try-except of docan.tx_thread.
    '''
    def IOCBICallBackDecorator(func):
        @wraps(func)
        def wrapped(self, cmd_obj):
            if self.DevNum != cmd_obj.iocbi_type.ctlStatusRecord[0]:
                raise ValueError('Response DevNum mismatch')
            return func(self, cmd_obj)
        return wrapped

    # #########################################################################
    # #########################################################################
    # ###################### TLE9012 API WRAPPER FUNCTIONS ####################
    # #########################################################################
    # #########################################################################
    '''
    tle9012->Reg.PART_CONFIG.B.EN_CELLx
    '''
    # Command factory for SetCellEnable
    def SetCellEnable(self, inCell=0, inEnable=CellEn.DISABLE.value,
                      callback=None, timeout=1.0):
        data_list = [self.DevNum,
                     self.ValidateEnum(CellNum, inCell),
                     self.ValidateEnum(CellEn, inEnable)]
        if callback is None:
            callback = self.SetCellEnableCB
        return self.IOCBICmdFactory(Cmd.SetCellEnable, data_list,
                                    callback, timeout)

    @IOCBICallBackDecorator
    def SetCellEnableCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.SetCellEnable, 3)
        if type(cmd_obj) is IOCBI:
            cell = self.ValidateEnum(CellNum,
                                     cmd_obj.iocbi_type.ctlStatusRecord[1])
            enable = self.ValidateEnum(CellEn,
                                       cmd_obj.iocbi_type.ctlStatusRecord[2])
            logging.info('SetCellEnable Dev:{} Cell:{} {}'.
                         format(self.DevNum, CellNum(cell).name,
                                CellEn(enable).name))

    # Command factory for GetCellEnable
    def GetCellEnable(self, inCell=0, callback=None, timeout=1.0):
        data_list = [self.DevNum, self.ValidateEnum(CellNum, inCell)]
        if callback is None:
            callback = self.GetCellEnableCB
        return self.IOCBICmdFactory(Cmd.GetCellEnable, data_list,
                                    callback, timeout)

    @IOCBICallBackDecorator
    def GetCellEnableCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.GetCellEnable, 3)
        cell = self.ValidateEnum(CellNum,
                                 cmd_obj.iocbi_type.ctlStatusRecord[1])
        enable = self.ValidateEnum(CellEn,
                                   cmd_obj.iocbi_type.ctlStatusRecord[2])
        logging.info('GetCellEnable Dev:{} Cell:{} {}'.
                     format(self.DevNum, CellNum(cell).name,
                            CellEn(enable).name))

    '''
    tle9012->Reg.OL_OV_THR_Reg.B.OL_THR_MAX
    '''
    # Command factory for SetMaxVoltDropThd
    def SetMaxVoltDropThd(self, inThd, callback=None, timeout=1.0):
        threshold = self.ValidateBitfield(inThd, 6)
        data_list = [self.DevNum, threshold]
        if callback is None:
            callback = self.SetMaximumVoltDropThdCB
        return self.IOCBICmdFactory(Cmd.SetMaxVoltDropThd,
                                    data_list, callback, timeout)

    @IOCBICallBackDecorator
    def SetMaximumVoltDropThdCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.
                                        SetMaxVoltDropThd, 2)
        val = cmd_obj.iocbi_type.ctlStatusRecord[1]
        logging.info("SetMaxVoltDropThd Dev:{} value:0x{:04x}".
                     format(self.DevNum, self.ValidateBitfield(val, 6)))

    # Command factory for GetMaxVoltDropThd
    def GetMaxVoltDropThd(self, callback=None, timeout=1.0):
        data_list = [self.DevNum]
        if callback is None:
            callback = self.GetMaxVoltDropThdCB
        return self.IOCBICmdFactory(Cmd.GetMaxVoltDropThd,
                                    data_list, callback, timeout)

    @IOCBICallBackDecorator
    def GetMaxVoltDropThdCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.GetMaxVoltDropThd, 2)
        val = cmd_obj.iocbi_type.ctlStatusRecord[1]
        logging.info('GetMaxVoltDropThd Dev:{} value:0x{:04x}'.
                     format(self.DevNum, self.ValidateBitfield(val, 6)))

    '''
    tle9012->Reg.OL_OV_THR_Reg.B.OV_THR
    '''
    # Command factory for SetOVoltFltThd
    def SetOVoltFltThd(self, inThd, callback=None, timeout=1.0):
        threshold = self.ValidateBitfield(inThd, 10)
        data_list = [self.DevNum, threshold >> 8, threshold & 0xff]
        if callback is None:
            callback = self.SetOVoltFltThdCB
        return self.IOCBICmdFactory(Cmd.SetOVoltFltThd,
                                    data_list, callback, timeout)

    @IOCBICallBackDecorator
    def SetOVoltFltThdCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.
                                        SetOVoltFltThd, 3)
        val = (cmd_obj.iocbi_type.ctlStatusRecord[1] << 8) + \
            cmd_obj.iocbi_type.ctlStatusRecord[2]
        logging.info("SetOVoltFltThd Dev:{} value:0x{:04x}".
                     format(self.DevNum, self.ValidateBitfield(val, 10)))

    # Command factory for GetOVoltFltThd
    def GetOVoltFltThd(self, callback=None, timeout=1.0):
        data_list = [self.DevNum]
        if callback is None:
            callback = self.GetOVoltFltThdCB
        return self.IOCBICmdFactory(Cmd.GetOVoltFltThd,
                                    data_list, callback, timeout)

    @IOCBICallBackDecorator
    def GetOVoltFltThdCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.
                                        GetOVoltFltThd, 3)
        val = (cmd_obj.iocbi_type.ctlStatusRecord[1] << 8) + \
            cmd_obj.iocbi_type.ctlStatusRecord[2]
        logging.info('GetOVoltFltThd Dev:{} value:0x{:04x}'.
                     format(self.DevNum, self.ValidateBitfield(val, 10)))

    '''
    tle9012->Reg.OL_UV_THR_Reg.B.OL_THR_MIN
    '''
    # Command factory for SetMinVoltDropThd
    def SetMinVoltDropThd(self, inThd, callback=None, timeout=1.0):
        threshold = self.ValidateBitfield(inThd, 6)
        data_list = [self.DevNum, threshold]
        if callback is None:
            callback = self.SetMinimumVoltDropThdCB
        return self.IOCBICmdFactory(Cmd.SetMinVoltDropThd,
                                    data_list, callback, timeout)

    @IOCBICallBackDecorator
    def SetMinimumVoltDropThdCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.
                                        SetMinVoltDropThd, 2)
        val = cmd_obj.iocbi_type.ctlStatusRecord[1]
        logging.info("SetMinVoltDropThd Dev:{} value:0x{:04x}".
                     format(self.DevNum, self.ValidateBitfield(val, 6)))

    # Command factory for GetMinVoltDropThd
    def GetMinVoltDropThd(self, callback=None, timeout=1.0):
        data_list = [self.DevNum]
        if callback is None:
            callback = self.GetMinimumVoltDropThdCB
        return self.IOCBICmdFactory(Cmd.GetMinVoltDropThd,
                                    data_list, callback, timeout)

    @IOCBICallBackDecorator
    def GetMinimumVoltDropThdCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.
                                        GetMinVoltDropThd, 2)
        val = cmd_obj.iocbi_type.ctlStatusRecord[1]
        logging.info('GetMinVoltDropThd Dev:{} value:0x{:04x}'.
                     format(self.DevNum, self.ValidateBitfield(val, 6)))

    '''
    tle9012->Reg.OL_UV_THR_Reg.B.UV_THR
    '''
    # Command factory for SetUVoltFltThd
    def SetUVoltFltThd(self, inThd, callback=None, timeout=1.0):
        threshold = self.ValidateBitfield(inThd, 10)
        data_list = [self.DevNum, threshold >> 8, threshold & 0xff]
        if callback is None:
            callback = self.SetUVoltFltThdCB
        return self.IOCBICmdFactory(Cmd.SetUVoltFltThd,
                                    data_list, callback, timeout)

    @IOCBICallBackDecorator
    def SetUVoltFltThdCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.
                                        SetUVoltFltThd, 3)
        val = (cmd_obj.iocbi_type.ctlStatusRecord[1] << 8) + \
            cmd_obj.iocbi_type.ctlStatusRecord[2]
        logging.info("SetUVoltFltThd Dev:{} value:0x{:04x}".
                     format(self.DevNum, self.ValidateBitfield(val, 10)))

    # Command factory for GetUVoltFltThd
    def GetUVoltFltThd(self, callback=None, timeout=1.0):
        data_list = [self.DevNum]
        if callback is None:
            callback = self.GetUVoltFltThdCB
        return self.IOCBICmdFactory(Cmd.GetUVoltFltThd,
                                    data_list, callback, timeout)

    @IOCBICallBackDecorator
    def GetUVoltFltThdCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.
                                        GetUVoltFltThd, 3)
        val = (cmd_obj.iocbi_type.ctlStatusRecord[1] << 8) + \
            cmd_obj.iocbi_type.ctlStatusRecord[2]
        logging.info('GetUVoltFltThd Dev:{} value:0x{:04x}'.
                     format(self.DevNum, self.ValidateBitfield(val, 10)))

    '''
    tle9012->Reg.TEMP_CONF_Reg.B.EXT_OT_THR
    '''
    # Command factory for SetExtTempOvertempThd
    def SetExtTempOvertempThd(self, inThd, callback=None, timeout=1.0):
        threshold = self.ValidateBitfield(inThd, 10)
        data_list = [self.DevNum, threshold >> 8, threshold & 0xff]
        if callback is None:
            callback = self.SetExtTempOvertempThdCB
        return self.IOCBICmdFactory(Cmd.SetExtTempOvertempThd,
                                    data_list, callback, timeout)

    @IOCBICallBackDecorator
    def SetExtTempOvertempThdCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.SetExtTempOvertempThd, 3)
        val = (cmd_obj.iocbi_type.ctlStatusRecord[1] << 8) + \
            cmd_obj.iocbi_type.ctlStatusRecord[2]
        logging.info("SetExtTempOvertempThd Dev:{} value:0x{:04x}".
                     format(self.DevNum, self.ValidateBitfield(val, 10)))

    # Command factory for GetExtTempOvertempThd
    def GetExtTempOvertempThd(self, callback=None, timeout=1.0):
        data_list = [self.DevNum]
        if callback is None:
            callback = self.GetExtTempOvertempThdCB
        return self.IOCBICmdFactory(Cmd.GetExtTempOvertempThd,
                                    data_list, callback, timeout)

    @IOCBICallBackDecorator
    def GetExtTempOvertempThdCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.GetExtTempOvertempThd, 3)
        val = (cmd_obj.iocbi_type.ctlStatusRecord[1] << 8) + \
            cmd_obj.iocbi_type.ctlStatusRecord[2]
        logging.info('GetExtTempOvertempThd Dev:{} value:0x{:04x}'
                     .format(self.DevNum, self.ValidateBitfield(val, 10)))

    '''
    tle9012->Reg.TEMP_CONF_Reg.B.I_NTC
    '''
    # Command factory for SetOtFltCurrSrc
    def SetOtFltCurrSrc(self, inCurrentSrc=CurrentSrc.I0_USED.value,
                        callback=None, timeout=1.0):
        data_list = [self.DevNum,
                     self.ValidateEnum(CurrentSrc, inCurrentSrc)]
        if callback is None:
            callback = self.SetOtFltCurrSrcCB
        return self.IOCBICmdFactory(Cmd.SetOtFltCurrSrc,
                                    data_list, callback, timeout)

    @IOCBICallBackDecorator
    def SetOtFltCurrSrcCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.SetOtFltCurrSrc, 2)
        current_src = self.ValidateEnum(CurrentSrc,
                                        cmd_obj.iocbi_type.ctlStatusRecord[1])
        logging.info('SetOtFltCurrSrc Dev:{} current_src:{}'
                     .format(self.DevNum, CurrentSrc(current_src).name))

    # Command factory for GetOtFltCurrSrc
    def GetOtFltCurrSrc(self, callback=None, timeout=1.0):
        data_list = [self.DevNum]
        if callback is None:
            callback = self.GetOtFltCurrSrcCB
        return self.IOCBICmdFactory(Cmd.GetOtFltCurrSrc, data_list, 
                                    callback, timeout)

    @IOCBICallBackDecorator
    def GetOtFltCurrSrcCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.GetOtFltCurrSrc, 2)
        current_src = self.ValidateEnum(CurrentSrc,
                                        cmd_obj.iocbi_type.ctlStatusRecord[1])
        logging.info('GetOtFltCurrSrc Dev:{} current source{}'
                     .format(self.DevNum, CurrentSrc(current_src).name))

    '''
    tle9012->Reg.TEMP_CONF_Reg.B.NR_TEMP_SENSE
    '''
    # Command factory for SetExtTempSensorsUsed
    def SetExtTempSensorsUsed(self, inSensor=ExtTempSensorsUsed.
                              NO_EXT_TEMP_SENSE.value, callback=None,
                              timeout=1.0):
        data_list = [self.DevNum,
                     self.ValidateEnum(ExtTempSensorsUsed, inSensor)]
        if callback is None:
            callback = self.SetExtTempSensorsUsedCB
        return self.IOCBICmdFactory(Cmd.SetExtTempSensorsUsed, data_list, 
                                    callback, timeout)

    @IOCBICallBackDecorator
    def SetExtTempSensorsUsedCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.SetExtTempSensorsUsed, 2)
        sensor = self.ValidateEnum(ExtTempSensorsUsed,
                                   cmd_obj.iocbi_type.ctlStatusRecord[1])
        logging.info('SetExtTempSensorsUsed Dev:{} current_src: {}'
                     .format(self.DevNum, ExtTempSensorsUsed(sensor).name))

    # Command factory for GetExtTempSensorsUsed
    def GetExtTempSensorsUsed(self, callback=None, timeout=1.0):
        data_list = [self.DevNum]
        if callback is None:
            callback = self.GetExtTempSensorsUsedCB
        return self.IOCBICmdFactory(Cmd.GetExtTempSensorsUsed, data_list,
                                    callback, timeout)

    @IOCBICallBackDecorator
    def GetExtTempSensorsUsedCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.GetExtTempSensorsUsed, 2)
        sensor = self.ValidateEnum(ExtTempSensorsUsed,
                                   cmd_obj.iocbi_type.ctlStatusRecord[1])
        logging.info('GetExtTempSensorsUsed Dev:{} sensor: {}'
                     .format(self.DevNum, ExtTempSensorsUsed(sensor).name))

    '''
    tle9012->Reg.INT_OT_WARN_CONF_Reg.B.INT_OT_THR
    '''
    # Command factory for SetIntTempOvertempThd
    def SetIntTempOvertempThd(self, inThd, callback=None, timeout=1.0):
        threshold = self.ValidateBitfield(inThd, 10)
        data_list = [self.DevNum, threshold >> 8, threshold & 0xff]
        if callback is None:
            callback = self.SetIntTempOvertempThdCB
        return self.IOCBICmdFactory(Cmd.SetIntTempOvertempThd, data_list, 
                                    callback, timeout)

    @IOCBICallBackDecorator
    def SetIntTempOvertempThdCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.
                                        SetIntTempOvertempThd, 3)
        val = (cmd_obj.iocbi_type.ctlStatusRecord[1] << 8) + \
            cmd_obj.iocbi_type.ctlStatusRecord[2]
        logging.info("SetIntTempOvertempThd Dev:{} value:0x{:04x}".
                     format(self.DevNum, self.ValidateBitfield(val, 10)))

    # Command factory for GetIntTempOvertempThd
    def GetIntTempOvertempThd(self, callback=None, timeout=1.0):
        data_list = [self.DevNum]
        if callback is None:
            callback = self.GetIntTempOvertempThdCB
        return self.IOCBICmdFactory(Cmd.
                                    GetIntTempOvertempThd,
                                    data_list, callback, timeout)

    @IOCBICallBackDecorator
    def GetIntTempOvertempThdCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.GetIntTempOvertempThd, 3)
        val = (cmd_obj.iocbi_type.ctlStatusRecord[1] << 8) + \
            cmd_obj.iocbi_type.ctlStatusRecord[2]
        logging.info('GetIntTempOvertempThd Dev:{} value:0x{:04x}'.
                     format(self.DevNum, self.ValidateBitfield(val, 10)))

    '''
    tle9012->Reg.RR_ERR_CNT_Reg.B.NR_ERR
    '''
    # Command factory for SetNumConsecErr
    def SetNumConsecErr(self, inErrors, callback=None, timeout=1.0):
        errors = self.ValidateBitfield(inErrors, 3)
        data_list = [self.DevNum, errors]
        if callback is None:
            callback = self.SetNumConsecErrCB
        return self.IOCBICmdFactory(Cmd.SetNumConsecErr,
                                    data_list, callback, timeout)

    @IOCBICallBackDecorator
    def SetNumConsecErrCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.SetNumConsecErr, 2)
        val = cmd_obj.iocbi_type.ctlStatusRecord[1]
        logging.info("SetNumConsecErr Dev:{} value:0x{:04x}".
                     format(self.DevNum, self.ValidateBitfield(val, 3)))

    # Command factory for GetNumConsecErr
    def GetNumConsecErr(self, callback=None, timeout=1.0):
        data_list = [self.DevNum]
        if callback is None:
            callback = self.GetNumConsecErrCB
        return self.IOCBICmdFactory(Cmd.GetNumConsecErr,
                                    data_list, callback, timeout)

    @IOCBICallBackDecorator
    def GetNumConsecErrCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.GetNumConsecErr, 2)
        val = cmd_obj.iocbi_type.ctlStatusRecord[1]
        logging.info('GetNumConsecErr Dev:{} value:0x{:04x}'.
                     format(self.DevNum, self.ValidateBitfield(val, 3)))

    '''
    tle9012->Reg.RR_ERR_CNT_Reg.B.NR_EXT_TEMP_START
    '''
    # Command factory for SetExtTempTrigForRR
    def SetExtTempTrigForRR(self, inTrig, callback=None, timeout=1.0):
        data_list = [self.DevNum,
                     self.ValidateEnum(ExtTempTrigForRR, inTrig)]
        if callback is None:
            callback = self.SetExtTempTrigForRRCB
        return self.IOCBICmdFactory(Cmd.SetExtTempTrigForRR,
                                    data_list, callback, timeout)

    @IOCBICallBackDecorator
    def SetExtTempTrigForRRCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.SetExtTempTrigForRR, 2)
        trigger = self.ValidateEnum(ExtTempTrigForRR,
                                    cmd_obj.iocbi_type.ctlStatusRecord[1])
        logging.info("SetExtTempTrigForRR Dev:{} trigger:{}"
                     .format(self.DevNum, ExtTempTrigForRR(trigger).name))

    # Command factory for GetExtTempTrigForRR
    def GetExtTempTrigForRR(self, callback=None, timeout=1.0):
        data_list = [self.DevNum]
        if callback is None:
            callback = self.GetExtTempTrigForRRCB
        return self.IOCBICmdFactory(Cmd.GetExtTempTrigForRR, data_list,
                                    callback, timeout)

    @IOCBICallBackDecorator
    def GetExtTempTrigForRRCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.GetExtTempTrigForRR, 2)
        trigger = self.ValidateEnum(ExtTempTrigForRR,
                                    cmd_obj.iocbi_type.ctlStatusRecord[1])
        logging.info('GetExtTempTrigForRR Dev:{} trigger:{}'
                     .format(self.DevNum, ExtTempTrigForRR(trigger).name))

    '''
    tle9012->Reg.RR_ERR_CNT_Reg.B.RR_SLEEP_CNT
    '''
    # Command factory for SetSleepModeTimingForRR
    def SetSleepModeTimingForRR(self, inSleepModeTiming, callback=None,
                                timeout=1.0):
        sleep_mode_timing = self.ValidateBitfield(inSleepModeTiming, 10)
        data_list = [self.DevNum, sleep_mode_timing >> 8,
                     sleep_mode_timing & 0xff]
        if callback is None:
            callback = self.SetSleepModeTimingForRRCB
        return self.IOCBICmdFactory(Cmd.SetSleepModeTimingForRR, data_list,
                                    callback, timeout)

    @IOCBICallBackDecorator
    def SetSleepModeTimingForRRCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.SetSleepModeTimingForRR, 3)
        val = (cmd_obj.iocbi_type.ctlStatusRecord[1] << 8) + \
            cmd_obj.iocbi_type.ctlStatusRecord[2]
        logging.info("SetSleepModeTimingForRR Dev:{} SleepModeTiming:0x{:04x}"
                     .format(self.DevNum, self.ValidateBitfield(val, 10)))

    # Command factory for GetSleepModeTimingForRR
    def GetSleepModeTimingForRR(self, callback=None, timeout=1.0):
        data_list = [self.DevNum]
        if callback is None:
            callback = self.GetSleepModeTimingForRRCB
        return self.IOCBICmdFactory(Cmd.GetSleepModeTimingForRR, data_list,
                                    callback, timeout)

    @IOCBICallBackDecorator
    def GetSleepModeTimingForRRCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.GetSleepModeTimingForRR, 3)
        val = (cmd_obj.iocbi_type.ctlStatusRecord[1] << 8) + \
            cmd_obj.iocbi_type.ctlStatusRecord[2]
        logging.info('GetSleepModeTimingForRR Dev:{} SleepModeTiming:0x{:04x}'
                     .format(self.DevNum, self.ValidateBitfield(val, 10)))

    '''
    tle9012->Reg.RR_CONFIG_Reg.B.RR_CNT
    '''
    # Command factory for SetRRCounter
    def SetRRCounter(self, inRRCounter, callback=None, timeout=1.0):
        rr_counter = self.ValidateBitfield(inRRCounter, 7)
        data_list = [self.DevNum, rr_counter]
        if callback is None:
            callback = self.SetRRCounterCB
        return self.IOCBICmdFactory(Cmd.SetRRCounter,
                                    data_list, callback, timeout)

    @IOCBICallBackDecorator
    def SetRRCounterCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.SetRRCounter, 2)
        logging.info("SetRRCounter Dev:{} RRCounter:{}".
                     format(self.DevNum,
                            cmd_obj.iocbi_type.ctlStatusRecord[1]))

    # Command factory for GetRRCounter
    def GetRRCounter(self, callback=None, timeout=1.0):
        data_list = [self.DevNum]
        if callback is None:
            callback = self.GetRRCounterCB
        return self.IOCBICmdFactory(Cmd.GetRRCounter, data_list,
                                    callback, timeout)

    @IOCBICallBackDecorator
    def GetRRCounterCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.GetRRCounter, 2)
        logging.info('GetRRCounter Dev:{} RRCounter:{}'.
                     format(self.DevNum,
                            cmd_obj.iocbi_type.ctlStatusRecord[1]))

    '''
    tle9012->Reg.RR_CONFIG_Reg.B.RR_SYNC
    '''
    # Command factory for SetRRSync
    def SetRRSync(self, inSync, callback=None, timeout=1.0):
        data_list = [self.DevNum, self.ValidateEnum(RRSync, inSync)]
        if callback is None:
            callback = self.SetRRSyncCB
        return self.IOCBICmdFactory(Cmd.SetRRSync, data_list,
                                    callback, timeout)

    @IOCBICallBackDecorator
    def SetRRSyncCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.SetRRSync, 2)
        sync = self.ValidateEnum(RRSync, cmd_obj.iocbi_type.ctlStatusRecord[1])
        logging.info("SetRRSync Dev:{} trigger:{}"
                     .format(self.DevNum, RRSync(sync).name))

    # Command factory for GetRRSync
    def GetRRSync(self, callback=None, timeout=1.0):
        data_list = [self.DevNum]
        if callback is None:
            callback = self.GetRRSyncCB
        return self.IOCBICmdFactory(Cmd.GetRRSync, data_list, callback,
                                    timeout)

    @IOCBICallBackDecorator
    def GetRRSyncCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.GetRRSync, 2)
        sync = self.ValidateEnum(RRSync, cmd_obj.iocbi_type.ctlStatusRecord[1])
        logging.info('GetRRSync Dev:{} trigger:{}'
                     .format(self.DevNum, RRSync(sync).name))

    '''
    tle9012->RR_CONFIG.B.M_NR_ERR_x
    '''
    # Command factory for SetRRCfgMsk
    def SetRRCfgMsk(self, inCfg, inMask, callback=None, timeout=1.0):
        data_list = [self.DevNum,
                     self.ValidateEnum(RRCfg, inCfg),
                     self.ValidateBool(inMask)]
        if callback is None:
            callback = self.SetRRCfgMskCB
        return self.IOCBICmdFactory(Cmd.SetRRCfgMsk, data_list, callback,
                                    timeout)

    @IOCBICallBackDecorator
    def SetRRCfgMskCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.SetRRCfgMsk, 3)
        config = self.ValidateEnum(RRCfg,
                                   cmd_obj.iocbi_type.ctlStatusRecord[1])
        mask = self.ValidateBool(cmd_obj.iocbi_type.ctlStatusRecord[2])
        logging.info("SetRRCfgMsk Dev:{} config:{} mask:{}"
                     .format(self.DevNum, RRCfg(config).name, mask))

    # Command factory for GetRRCfgMsk
    def GetRRCfgMsk(self, inCfg, callback=None, timeout=1.0):
        data_list = [self.DevNum,
                     self.ValidateEnum(RRCfg, inCfg)]
        if callback is None:
            callback = self.GetRRCfgMskCB
        return self.IOCBICmdFactory(Cmd.GetRRCfgMsk, data_list, callback,
                                    timeout)

    @IOCBICallBackDecorator
    def GetRRCfgMskCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.GetRRCfgMsk, 3)
        config = self.ValidateEnum(RRCfg,
                                   cmd_obj.iocbi_type.ctlStatusRecord[1])
        mask = self.ValidateBool(cmd_obj.iocbi_type.ctlStatusRecord[2])
        logging.info('GetRRCfgMsk Dev:{} config:{} mask:{}'
                     .format(self.DevNum, RRCfg(config).name, mask))

    '''
    void TLE9012_SetFltMskCfg(uint8_t inDevice, TLE9012_FltMskCfg_t inConfig, bool inMask);
    bool TLE9012_GetFltMskCfg(uint8_t inDevice, TLE9012_FltMskCfg_t inConfig);
    '''
    # Command factory for SetFltMskCfg
    def SetFltMskCfg(self, inCfg, inMask, callback=None, timeout=1.0):
        data_list = [self.DevNum,
                     self.ValidateEnum(FltMskCfg, inCfg),
                     self.ValidateBool(inMask)]
        if callback is None:
            callback = self.SetFltMskCfgCB
        return self.IOCBICmdFactory(Cmd.SetFltMskCfg, data_list, callback,
                                    timeout)

    @IOCBICallBackDecorator
    def SetFltMskCfgCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.SetFltMskCfg, 3)
        config = self.ValidateEnum(FltMskCfg,
                                   cmd_obj.iocbi_type.ctlStatusRecord[1])
        mask = self.ValidateBool(cmd_obj.iocbi_type.ctlStatusRecord[2])
        logging.info("SetFltMskCfg Dev:{} config:{} mask:{}"
                     .format(self.DevNum, FltMskCfg(config).name, mask))

    # Command factory for GetFltMskCfg
    def GetFltMskCfg(self, inCfg, callback=None, timeout=1.0):
        data_list = [self.DevNum,
                     self.ValidateEnum(FltMskCfg, inCfg)]
        if callback is None:
            callback = self.GetFltMskCfgCB
        return self.IOCBICmdFactory(Cmd.GetFltMskCfg, data_list, callback,
                                    timeout)

    @IOCBICallBackDecorator
    def GetFltMskCfgCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.GetFltMskCfg, 3)
        config = self.ValidateEnum(FltMskCfg,
                                   cmd_obj.iocbi_type.ctlStatusRecord[1])
        mask = self.ValidateBool(cmd_obj.iocbi_type.ctlStatusRecord[2])
        logging.info('GetFltMskCfg Dev:{} config:{} mask:{}'
                     .format(self.DevNum, FltMskCfg(config).name, mask))

    '''
    void TLE9012_SetGenDiagMsk(uint8_t inDevice, TLE9012_GenDiag_t inConfig, bool inMask);
    bool TLE9012_GetGenDiagMsk(uint8_t inDevice, TLE9012_GenDiag_t inConfig);
    '''
    # Command factory for SetGenDiagMsk
    def SetGenDiagMsk(self, inCfg, inMask, callback=None, timeout=1.0):
        data_list = [self.DevNum,
                     self.ValidateEnum(GenDiag, inCfg),
                     self.ValidateBool(inMask)]
        if callback is None:
            callback = self.SetGenDiagMskCB
        return self.IOCBICmdFactory(Cmd.SetGenDiagMsk, data_list, callback,
                                    timeout)

    @IOCBICallBackDecorator
    def SetGenDiagMskCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.SetGenDiagMsk, 3)
        config = self.ValidateEnum(GenDiag,
                                   cmd_obj.iocbi_type.ctlStatusRecord[1])
        mask = self.ValidateBool(cmd_obj.iocbi_type.ctlStatusRecord[2])
        logging.info("SetGenDiagMsk Dev:{} config:{} mask:{}"
                     .format(self.DevNum, GenDiag(config).name, mask))

    # Command factory for GetGenDiagMsk
    def GetGenDiagMsk(self, inCfg, callback=None, timeout=1.0):
        data_list = [self.DevNum,
                     self.ValidateEnum(GenDiag, inCfg)]
        if callback is None:
            callback = self.GetGenDiagMskCB
        return self.IOCBICmdFactory(Cmd.GetGenDiagMsk, data_list, callback,
                                    timeout)

    @IOCBICallBackDecorator
    def GetGenDiagMskCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.GetGenDiagMsk, 3)
        config = self.ValidateEnum(GenDiag,
                                   cmd_obj.iocbi_type.ctlStatusRecord[1])
        mask = self.ValidateBool(cmd_obj.iocbi_type.ctlStatusRecord[2])
        logging.info('GetGenDiagMsk Dev:{} config:{} mask:{}'
                     .format(self.DevNum, GenDiag(config).name, mask))

    '''
    void TLE9012_SetCellUndervoltageFlg(uint8_t inDevice, TLE9012_CellNumber_t inCell, TLE9012_Undervoltage_t inUV);
    TLE9012_Undervoltage_t TLE9012_GetCellUndervoltageFlg(uint8_t inDevice, TLE9012_CellNumber_t inCell);
    '''
    # Command factory for SetCellUVoltFlg
    def SetCellUVoltFlg(self, inCell=0,
                        inUV=UVolt.NO_UNDERVOLTAGE_IN_CELL.value,
                        callback=None, timeout=1.0):
        data_list = [self.DevNum,
                     self.ValidateEnum(CellNum, inCell),
                     self.ValidateEnum(UVolt, inUV)]
        if callback is None:
            callback = self.SetCellUVoltFlgCB
        return self.IOCBICmdFactory(Cmd.SetCellUVoltFlg,
                                    data_list, callback, timeout)

    @IOCBICallBackDecorator
    def SetCellUVoltFlgCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.SetCellUVoltFlg, 3)
        cell = self.ValidateEnum(CellNum,
                                 cmd_obj.iocbi_type.ctlStatusRecord[1])
        uv = self.ValidateEnum(UVolt,
                               cmd_obj.iocbi_type.ctlStatusRecord[2])
        logging.info('SetCellUVoltFlg Dev:{} {} {}'
                     .format(self.DevNum, CellNum(cell).name, UVolt(uv).name))

    # Command factory for GetCellUVoltFlg
    def GetCellUVoltFlg(self, inCell=0, callback=None, timeout=1.0):
        data_list = [self.DevNum,
                     self.ValidateEnum(CellNum, inCell)]
        if callback is None:
            callback = self.GetCellUVoltFlgCB
        return self.IOCBICmdFactory(Cmd.GetCellUVoltFlg, data_list, callback,
                                    timeout)

    @IOCBICallBackDecorator
    def GetCellUVoltFlgCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.GetCellUVoltFlg, 3)
        cell = self.ValidateEnum(CellNum,
                                 cmd_obj.iocbi_type.ctlStatusRecord[1])
        uv = self.ValidateEnum(UVolt,
                               cmd_obj.iocbi_type.ctlStatusRecord[2])
        logging.info('GetCellUVoltFlg Dev:{} {} {}'
                     .format(self.DevNum, CellNum(cell).name, UVolt(uv).name))

    '''
    void TLE9012_SetCellOvervoltageFlg(uint8_t inDevice, TLE9012_CellNumber_t inCell, TLE9012_Overvoltage_t inUV);
    TLE9012_Overvoltage_t TLE9012_GetCellOvervoltageFlg(uint8_t inDevice, TLE9012_CellNumber_t inCell);
    '''
    # Command factory for SetCellOVoltFlg
    def SetCellOVoltFlg(self, inCell=0,
                        inOV=OVolt.NO_OVERVOLTAGE_IN_CELL.value,
                        callback=None, timeout=1.0):
        data_list = [self.DevNum,
                     self.ValidateEnum(CellNum, inCell),
                     self.ValidateEnum(OVolt, inOV)]
        if callback is None:
            callback = self.SetCellOVoltFlgCB
        return self.IOCBICmdFactory(Cmd.SetCellOVoltFlg, data_list, callback,
                                    timeout)

    @IOCBICallBackDecorator
    def SetCellOVoltFlgCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.SetCellOVoltFlg, 3)
        cell = self.ValidateEnum(CellNum,
                                 cmd_obj.iocbi_type.ctlStatusRecord[1])
        ov = self.ValidateEnum(OVolt,
                               cmd_obj.iocbi_type.ctlStatusRecord[2])
        logging.info('SetCellOVoltFlg Dev:{} {} {}'
                     .format(self.DevNum, CellNum(cell).name, OVolt(ov).name))

    # Command factory for GetCellOVoltFlg
    def GetCellOVoltFlg(self, inCell=0, callback=None, timeout=1.0):
        data_list = [self.DevNum,
                     self.ValidateEnum(CellNum, inCell)]
        if callback is None:
            callback = self.GetCellOVoltFlgCB
        return self.IOCBICmdFactory(Cmd.GetCellOVoltFlg, data_list, callback,
                                    timeout)

    @IOCBICallBackDecorator
    def GetCellOVoltFlgCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.GetCellOVoltFlg, 3)
        cell = self.ValidateEnum(CellNum,
                                 cmd_obj.iocbi_type.ctlStatusRecord[1])
        ov = self.ValidateEnum(OVolt,
                               cmd_obj.iocbi_type.ctlStatusRecord[2])
        logging.info('GetCellOVoltFlg Dev:{} {} {}'
                     .format(self.DevNum, CellNum(cell).name, OVolt(ov).name))

    '''
    void TLE9012_SetExtTempDiagOpenFlg(uint8_t inDevice, TLE9012_ExtTemp_t inExtTemp, TLE9012_Openload_t inOL);
    TLE9012_Openload_t TLE9012_GetExtTempDiagOpenFlg(uint8_t inDevice, TLE9012_ExtTemp_t inExtTemp);
    '''
    # Command factory for SetExtTempDiagOpenFlg
    def SetExtTempDiagOpenFlg(self, inExtTemp=ExtTemp.EXT_TMP_IDX_0.value,
                              inOL=Openload.NO_OPENLOAD.value,
                              callback=None, timeout=1.0):
        data_list = [self.DevNum,
                     self.ValidateEnum(ExtTemp, inExtTemp),
                     self.ValidateEnum(Openload, inOL)]
        if callback is None:
            callback = self.SetExtTempDiagOpenFlgCB
        return self.IOCBICmdFactory(Cmd.SetExtTempDiagOpenFlg, data_list,
                                    callback, timeout)

    @IOCBICallBackDecorator
    def SetExtTempDiagOpenFlgCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.SetExtTempDiagOpenFlg, 3)
        ext_temp = self.ValidateEnum(ExtTemp,
                                     cmd_obj.iocbi_type.ctlStatusRecord[1])
        ol = self.ValidateEnum(Openload,
                               cmd_obj.iocbi_type.ctlStatusRecord[2])
        logging.info('SetExtTempDiagOpenFlg Dev:{} {} {}'
                     .format(self.DevNum, ExtTemp(ext_temp).name,
                             Openload(ol).name))

    # Command factory for GetExtTempDiagOpenFlg
    def GetExtTempDiagOpenFlg(self, inExtTemp=ExtTemp.EXT_TMP_IDX_0.value,
                              callback=None, timeout=1.0):
        data_list = [self.DevNum,
                     self.ValidateEnum(ExtTemp, inExtTemp)]
        if callback is None:
            callback = self.GetExtTempDiagOpenFlgCB
        return self.IOCBICmdFactory(Cmd.GetExtTempDiagOpenFlg, data_list,
                                    callback, timeout)

    @IOCBICallBackDecorator
    def GetExtTempDiagOpenFlgCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.GetExtTempDiagOpenFlg, 3)
        ext_temp = self.ValidateEnum(ExtTemp,
                                     cmd_obj.iocbi_type.ctlStatusRecord[1])
        ol = self.ValidateEnum(Openload,
                               cmd_obj.iocbi_type.ctlStatusRecord[2])
        logging.info('GetExtTempDiagOpenFlg Dev:{} {} {}'
                     .format(self.DevNum, ExtTemp(ext_temp).name,
                             Openload(ol).name))

    '''
    void TLE9012_SetExtTempDiagShortFlg(uint8_t inDevice, TLE9012_ExtTemp_t inExtTemp, TLE9012_ShortCircuit_t inSC);
    TLE9012_ShortCircuit_t TLE9012_GetExtTempDiagShortFlg(uint8_t inDevice, TLE9012_ExtTemp_t inExtTemp);
    '''
    # Command factory for SetExtTempDiagShortFlg
    def SetExtTempDiagShortFlg(self, inExtTemp=ExtTemp.EXT_TMP_IDX_0.value,
                               inSC=ShrtCirc.NO_SHORT_CIRCUIT.value,
                               callback=None, timeout=1.0):
        data_list = [self.DevNum,
                     self.ValidateEnum(ExtTemp, inExtTemp),
                     self.ValidateEnum(ShrtCirc, inSC)]
        if callback is None:
            callback = self.SetExtTempDiagShortFlgCB
        return self.IOCBICmdFactory(Cmd.SetExtTempDiagShortFlg,
                                    data_list, callback, timeout)

    @IOCBICallBackDecorator
    def SetExtTempDiagShortFlgCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.SetExtTempDiagShortFlg, 3)
        ext_temp = self.ValidateEnum(ExtTemp,
                                     cmd_obj.iocbi_type.ctlStatusRecord[1])
        sc = self.ValidateEnum(ShrtCirc,
                               cmd_obj.iocbi_type.ctlStatusRecord[2])
        logging.info('SetExtTempDiagShortFlg Dev:{} {} {}'
                     .format(self.DevNum, ExtTemp(ext_temp).name,
                             ShrtCirc(sc).name))

    # Command factory for GetExtTempDiagShortFlg
    def GetExtTempDiagShortFlg(self, inExtTemp=ExtTemp.EXT_TMP_IDX_0.value,
                               callback=None, timeout=1.0):
        data_list = [self.DevNum,
                     self.ValidateEnum(ExtTemp, inExtTemp)]
        if callback is None:
            callback = self.GetExtTempDiagShortFlgCB
        return self.IOCBICmdFactory(Cmd.GetExtTempDiagShortFlg, data_list,
                                    callback, timeout)

    @IOCBICallBackDecorator
    def GetExtTempDiagShortFlgCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.GetExtTempDiagShortFlg, 3)
        ext_temp = self.ValidateEnum(ExtTemp,
                                     cmd_obj.iocbi_type.ctlStatusRecord[1])
        sc = self.ValidateEnum(ShrtCirc,
                               cmd_obj.iocbi_type.ctlStatusRecord[2])
        logging.info('GetExtTempDiagShortFlg Dev:{} {} {}'
                     .format(self.DevNum, ExtTemp(ext_temp).name,
                             ShrtCirc(sc).name))

    '''
    void TLE9012_SetExtTempOTempFlg(uint8_t inDevice, TLE9012_ExtTemp_t inExtTemp, TLE9012_OTemp_t inOT);
    TLE9012_OTemp_t TLE9012_GetExtTempDiagOtFlg(uint8_t inDevice, TLE9012_ExtTemp_t inExtTemp);
    '''
    # Command factory for SetExtTempDiagOtFlg
    def SetExtTempDiagOtFlg(self, inExtTemp=ExtTemp.EXT_TMP_IDX_0.value,
                            inSC=OTemp.NO_OVERTEMPERATURE_IN_EXT_TEMP.value,
                            callback=None, timeout=1.0):
        data_list = [self.DevNum,
                     self.ValidateEnum(ExtTemp, inExtTemp),
                     self.ValidateEnum(OTemp, inSC)]
        if callback is None:
            callback = self.SetExtTempDiagOtFlgCB
        return self.IOCBICmdFactory(Cmd.SetExtTempDiagOtFlg, data_list,
                                    callback, timeout)

    @IOCBICallBackDecorator
    def SetExtTempDiagOtFlgCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.
                                        SetExtTempDiagOtFlg, 3)
        ext_temp = self.ValidateEnum(ExtTemp,
                                     cmd_obj.iocbi_type.ctlStatusRecord[1])
        ot = self.ValidateEnum(OTemp,
                               cmd_obj.iocbi_type.ctlStatusRecord[2])
        logging.info('SetExtTempDiagOtFlg Dev:{} {} {}'
                     .format(self.DevNum, ExtTemp(ext_temp).name,
                             OTemp(ot).name))

    # Command factory for GetExtTempDiagOtFlg
    def GetExtTempDiagOtFlg(self, inExtTemp=ExtTemp.EXT_TMP_IDX_0.value,
                            callback=None, timeout=1.0):
        data_list = [self.DevNum,
                     self.ValidateEnum(ExtTemp, inExtTemp)]
        if callback is None:
            callback = self.GetExtTempDiagOtFlgCB
        return self.IOCBICmdFactory(Cmd.GetExtTempDiagOtFlg, data_list,
                                    callback, timeout)

    @IOCBICallBackDecorator
    def GetExtTempDiagOtFlgCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.GetExtTempDiagOtFlg, 3)
        ext_temp = self.ValidateEnum(ExtTemp,
                                     cmd_obj.iocbi_type.ctlStatusRecord[1])
        ot = self.ValidateEnum(OTemp,
                               cmd_obj.iocbi_type.ctlStatusRecord[2])
        logging.info('GetExtTempDiagOtFlg Dev:{} {} {}'
                     .format(self.DevNum, ExtTemp(ext_temp).name,
                             OTemp(ot).name))

    '''
    void TLE9012_SetCellOpenloadFlg(uint8_t inDevice, TLE9012_CellNumber_t inCell, TLE9012_Openload_t inOL);
    TLE9012_Openload_t TLE9012_GetCellOpenloadFlg(uint8_t inDevice, TLE9012_CellNumber_t inCell);
    '''
    # Command factory for SetCellOpenloadFlg
    def SetCellOpenloadFlg(self, inCell=0, inOL=Openload.NO_OPENLOAD.value,
                           callback=None, timeout=1.0):
        data_list = [self.DevNum,
                     self.ValidateEnum(CellNum, inCell),
                     self.ValidateEnum(Openload, inOL)]
        if callback is None:
            callback = self.SetCellOpenloadFlgCB
        return self.IOCBICmdFactory(Cmd.SetCellOpenloadFlg, data_list,
                                    callback, timeout)

    @IOCBICallBackDecorator
    def SetCellOpenloadFlgCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.SetCellOpenloadFlg, 3)
        cell = self.ValidateEnum(CellNum,
                                 cmd_obj.iocbi_type.ctlStatusRecord[1])
        ol = self.ValidateEnum(Openload,
                               cmd_obj.iocbi_type.ctlStatusRecord[2])
        logging.info('SetCellOpenloadFlg Dev:{} {} {}'
                     .format(self.DevNum, CellNum(cell).name,
                             Openload(ol).name))

    # Command factory for GetCellOpenloadFlg
    def GetCellOpenloadFlg(self, inCell=0, callback=None, timeout=1.0):
        data_list = [self.DevNum, self.ValidateEnum(CellNum,
                     inCell)]
        if callback is None:
            callback = self.GetCellOpenloadFlgCB
        return self.IOCBICmdFactory(Cmd.GetCellOpenloadFlg, data_list,
                                    callback, timeout)

    @IOCBICallBackDecorator
    def GetCellOpenloadFlgCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.GetCellOpenloadFlg, 3)
        cell = self.ValidateEnum(CellNum,
                                 cmd_obj.iocbi_type.ctlStatusRecord[1])
        ol = self.ValidateEnum(Openload,
                               cmd_obj.iocbi_type.ctlStatusRecord[2])
        logging.info('GetCellOpenloadFlg Dev:{} {} {}'
                     .format(self.DevNum, CellNum(cell).name,
                             Openload(ol).name))

    '''
    void TLE9012_SetCRCRegisterError(uint8_t inDevice, TLE9012_Register_Index_t inRegister);
    TLE9012_Register_Index_t TLE9012_GetCRCRegisterError(uint8_t inDevice);
    '''

    '''
    void TLE9012_SetExtendedWatchdog(uint8_t inDevice, bool inEnable);
    bool TLE9012_GetExtendedWatchdog(uint8_t inDevice);
    '''
    # Command factory for SetExtendWdg
    def SetExtendWdg(self, inEnable, callback=None, timeout=1.0):
        data_list = [self.DevNum, self.ValidateBool(inEnable)]
        if callback is None:
            callback = self.SetExtendWdgCB
        return self.IOCBICmdFactory(Cmd.SetExtendWdg, data_list,
                                    callback, timeout)

    @IOCBICallBackDecorator
    def SetExtendWdgCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.SetExtendWdg, 2)
        logging.info("SetExtendWdg Dev:{} enable:{}"
                     .format(self.DevNum,
                             self.ValidateBool(cmd_obj.iocbi_type.
                                               ctlStatusRecord[1])))

    # Command factory for GetExtendWdg
    def GetExtendWdg(self, callback=None, timeout=1.0):
        data_list = [self.DevNum]
        if callback is None:
            callback = self.GetExtendWdgCB
        return self.IOCBICmdFactory(Cmd.GetExtendWdg, data_list,
                                    callback, timeout)

    @IOCBICallBackDecorator
    def GetExtendWdgCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.GetExtendWdg, 2)
        logging.info('GetExtendWdg Dev:{} enable:{}'
                     .format(self.DevNum,
                             self.ValidateBool(cmd_obj.iocbi_type.
                                               ctlStatusRecord[1])))

    '''
    void TLE9012_SetActivateSleepMode(uint8_t inDevice, bool inEnable);
    bool TLE9012_GetActivateSleepMode(uint8_t inDevice);
    '''
    # Command factory for SetActivateSleepMode
    def SetActivateSleepMode(self, inEnable, callback=None, timeout=1.0):
        data_list = [self.DevNum, self.ValidateBool(inEnable)]
        if callback is None:
            callback = self.SetActivateSleepModeCB
        return self.IOCBICmdFactory(Cmd.SetActivateSleepMode, data_list,
                                    callback, timeout)

    @IOCBICallBackDecorator
    def SetActivateSleepModeCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.SetActivateSleepMode, 2)
        logging.info("SetActivateSleepMode Dev:{} enable:{}"
                     .format(self.DevNum,
                             self.ValidateBool(cmd_obj.iocbi_type.
                                               ctlStatusRecord[1])))

    # Command factory for GetActivateSleepMode
    def GetActivateSleepMode(self, callback=None, timeout=1.0):
        data_list = [self.DevNum]
        if callback is None:
            callback = self.GetActivateSleepModeCB
        return self.IOCBICmdFactory(Cmd.GetActivateSleepMode, data_list,
                                    callback, timeout)

    @IOCBICallBackDecorator
    def GetActivateSleepModeCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.GetActivateSleepMode, 2)
        logging.info('GetActivateSleepMode Dev:{} enable:{}'
                     .format(self.DevNum,
                             self.ValidateBool(cmd_obj.iocbi_type.
                                               ctlStatusRecord[1])))

    '''
    void TLE9012_SetUnderCurrentFaultThd(uint8_t inDevice, uint8_t inThd);
    uint8_t TLE9012_GetUnderCurrentFaultThd(uint8_t inDevice);
    '''
    # Command factory for SetUCurrFltThd
    def SetUCurrFltThd(self, inThd, callback=None, timeout=1.0):
        threshold = self.ValidateBitfield(inThd, 8)
        data_list = [self.DevNum, threshold]
        if callback is None:
            callback = self.SetUCurrFltThdCB
        return self.IOCBICmdFactory(Cmd.SetUCurrFltThd,
                                    data_list, callback, timeout)

    @IOCBICallBackDecorator
    def SetUCurrFltThdCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.SetUCurrFltThd, 2)
        logging.info("SetUCurrFltThd Dev:{} threshold:{}"
                     .format(self.DevNum,
                             cmd_obj.iocbi_type.ctlStatusRecord[1]))

    # Command factory for GetUCurrFltThd
    def GetUCurrFltThd(self, callback=None, timeout=1.0):
        data_list = [self.DevNum]
        if callback is None:
            callback = self.GetUCurrFltThdCB
        return self.IOCBICmdFactory(Cmd.GetUCurrFltThd,
                                    data_list, callback, timeout)

    @IOCBICallBackDecorator
    def GetUCurrFltThdCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.GetUCurrFltThd, 2)
        logging.info('GetUCurrFltThd Dev:{} threshold:{}'
                     .format(self.DevNum,
                             cmd_obj.iocbi_type.ctlStatusRecord[1]))

    '''
    void TLE9012_SetOverCurrentFaultThd(uint8_t inDevice, uint8_t inThd);
    uint8_t TLE9012_GetOverCurrentFaultThd(uint8_t inDevice);
    '''
    # Command factory for SetOCurrFltThd
    def SetOCurrFltThd(self, inThd, callback=None, timeout=1.0):
        threshold = self.ValidateBitfield(inThd, 8)
        data_list = [self.DevNum, threshold]
        if callback is None:
            callback = self.SetOCurrFltCB
        return self.IOCBICmdFactory(Cmd.SetOCurrFltThd,
                                    data_list, callback, timeout)

    @IOCBICallBackDecorator
    def SetOCurrFltCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.SetOCurrFltThd, 2)
        logging.info("SetOCurrFltThd Dev:{} threshold:{}"
                     .format(self.DevNum,
                             cmd_obj.iocbi_type.ctlStatusRecord[1]))

    # Command factory for GetOCurrFltThd
    def GetOCurrFltThd(self, callback=None, timeout=1.0):
        data_list = [self.DevNum]
        if callback is None:
            callback = self.GetOCurrFltCB
        return self.IOCBICmdFactory(Cmd.GetOCurrFltThd,
                                    data_list, callback, timeout)

    @IOCBICallBackDecorator
    def GetOCurrFltCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.GetOCurrFltThd, 2)
        logging.info('GetOCurrFltThd Dev:{} threshold:{}'
                     .format(self.DevNum,
                             cmd_obj.iocbi_type.ctlStatusRecord[1]))

    '''
    void TLE9012_SetBalState(uint8_t inDevice, TLE9012_CellNumber_t inCell, TLE9012_BalSwitchState_t inON);
    TLE9012_BalSwitchState_t TLE9012_GetBalState(uint8_t inDevice, TLE9012_CellNumber_t inCell);
    '''
    # Command factory for SetBalState
    def SetBalState(self, inCell=0, inON=BalSwitchState.DRIVER_OFF.value,
                    callback=None, timeout=1.0):
        data_list = [self.DevNum,
                     self.ValidateEnum(CellNum, inCell),
                     self.ValidateEnum(BalSwitchState,
                                       inON)]
        if callback is None:
            callback = self.SetBalStateCB
        return self.IOCBICmdFactory(Cmd.SetBalState,
                                    data_list, callback, timeout)

    @IOCBICallBackDecorator
    def SetBalStateCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.SetBalState, 3)
        cell = self.ValidateEnum(CellNum,
                                 cmd_obj.iocbi_type.ctlStatusRecord[1])
        on = self.ValidateEnum(BalSwitchState,
                               cmd_obj.iocbi_type.ctlStatusRecord[2])
        logging.info('SetBalState Dev:{} {} {}'
                     .format(self.DevNum, CellNum(cell).name,
                             BalSwitchState(on).name))

    # Command factory for GetBalState
    def GetBalState(self, inCell=0, callback=None, timeout=1.0):
        data_list = [self.DevNum,
                     self.ValidateEnum(CellNum, inCell)]
        if callback is None:
            callback = self.GetBalStateCB
        return self.IOCBICmdFactory(Cmd.GetBalState,
                                    data_list, callback, timeout)

    @IOCBICallBackDecorator
    def GetBalStateCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.GetBalState, 3)
        cell = self.ValidateEnum(CellNum,
                                 cmd_obj.iocbi_type.ctlStatusRecord[1])
        on = self.ValidateEnum(BalSwitchState,
                               cmd_obj.iocbi_type.ctlStatusRecord[2])
        logging.info('GetBalState Dev:{} {} {}'
                     .format(self.DevNum, CellNum(cell).name,
                             BalSwitchState(on).name))

    '''
    void TLE9012_SetAVMExtTempDiagPulldown(uint8_t inDevice, TLE9012_AVMExtTempDiag_t in_pulldown);
    TLE9012_AVMExtTempDiag_t TLE9012_GetAVMExtTempDiagPulldown(uint8_t inDevice);
    '''
    # Command factory for SetAVMExtTempDiagPd
    def SetAVMExtTempDiagPd(self, inPulldown, callback=None, timeout=1.0):
        data_list = [self.DevNum,
                     self.ValidateEnum(AVMExtTempDiag, inPulldown)]
        if callback is None:
            callback = self.SetAVMExtTempDiagPdCB
        return self.IOCBICmdFactory(Cmd.SetAVMExtTempDiagPd, data_list,
                                    callback, timeout)

    @IOCBICallBackDecorator
    def SetAVMExtTempDiagPdCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.SetAVMExtTempDiagPd, 2)
        in_pulldown = self.ValidateEnum(AVMExtTempDiag,
                                        cmd_obj.iocbi_type.ctlStatusRecord[1])
        logging.info("SetAVMExtTempDiagPd Dev:{} pulldown:{}".
                     format(self.DevNum, AVMExtTempDiag(in_pulldown).name))

    # Command factory for GetAVMExtTempDiagPd
    def GetAVMExtTempDiagPd(self, callback=None, timeout=1.0):
        data_list = [self.DevNum]
        if callback is None:
            callback = self.GetAVMExtTempDiagPdCB
        return self.IOCBICmdFactory(Cmd.GetAVMExtTempDiagPd, data_list,
                                    callback, timeout)

    @IOCBICallBackDecorator
    def GetAVMExtTempDiagPdCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.GetAVMExtTempDiagPd, 2)
        in_pulldown = self.ValidateEnum(AVMExtTempDiag,
                                        cmd_obj.iocbi_type.ctlStatusRecord[1])
        logging.info('GetAVMExtTempDiagPd Dev:{} pulldown:{}'.
                     format(self.DevNum, AVMExtTempDiag(in_pulldown).name))

    '''
    void TLE9012_SetDiagnosisResistorMaskFlg(uint8_t inDevice, TLE9012_AuxVoltDiag_t in_AVMMask, TLE9012_DiagnosisResistorMask_t inUV);
    TLE9012_DiagnosisResistorMask_t TLE9012_GetDiagnosisResistorMaskFlg(uint8_t inDevice, TLE9012_AuxVoltDiag_t in_AVMMask);
    '''
    # Command factory for SetDiagResMskFlg
    def SetDiagResMskFlg(self, in_AVMMask, inRM, callback=None, timeout=1.0):
        data_list = [self.DevNum,
                     self.ValidateEnum(AuxVoltDiag, in_AVMMask),
                     self.ValidateEnum(DiagnosisResistorMask, inRM)]
        if callback is None:
            callback = self.SetDiagResMskFlgCB
        return self.IOCBICmdFactory(Cmd.SetDiagResMskFlg,
                                    data_list, callback, timeout)

    @IOCBICallBackDecorator
    def SetDiagResMskFlgCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.
                                        SetDiagResMskFlg, 3)
        config = self.ValidateEnum(AuxVoltDiag,
                                   cmd_obj.iocbi_type.ctlStatusRecord[1])
        mask = self.ValidateEnum(DiagnosisResistorMask,
                                 cmd_obj.iocbi_type.ctlStatusRecord[2])
        logging.info("SetDiagResMskFlg Dev:{} {} {}"
                     .format(self.DevNum, AuxVoltDiag(config).name,
                             DiagnosisResistorMask(mask).name))

    # Command factory for GetDiagResMskFlg
    def GetDiagResMskFlg(self, inCfg, callback=None, timeout=1.0):
        data_list = [self.DevNum,
                     self.ValidateEnum(AuxVoltDiag, inCfg)]
        if callback is None:
            callback = self.GetDiagResMskFlgCB
        return self.IOCBICmdFactory(Cmd.GetDiagResMskFlg,
                                    data_list, callback, timeout)

    @IOCBICallBackDecorator
    def GetDiagResMskFlgCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.GetDiagResMskFlg, 3)
        avm_mask = self.ValidateEnum(AuxVoltDiag,
                                     cmd_obj.iocbi_type.ctlStatusRecord[1])
        mask = self.ValidateEnum(DiagnosisResistorMask,
                                 cmd_obj.iocbi_type.ctlStatusRecord[2])
        logging.info('GetDiagResMskFlg Dev:{} {} {}'.
                     format(self.DevNum, AuxVoltDiag(avm_mask).name,
                            DiagnosisResistorMask(mask).name))

    '''
    void TLE9012_SetBalanceDiagOvercurrent(uint8_t inDevice, TLE9012_CellNumber_t inCell, TLE9012_Overcurrent_t inOC);
    TLE9012_Overcurrent_t TLE9012_GetBalanceDiagOvercurrent(uint8_t inDevice, TLE9012_CellNumber_t inCell);
    '''
    # Command factory for SetBalDiagOCurr
    def SetBalDiagOCurr(self, inCell=0,
                        inOC=Overcurrent.NO_OVERCURRENT_IN_CELL.value,
                        callback=None, timeout=1.0):
        data_list = [self.DevNum,
                     self.ValidateEnum(CellNum, inCell),
                     self.ValidateEnum(Overcurrent, inOC)]
        if callback is None:
            callback = self.SetBalDiagOCurrCB
        return self.IOCBICmdFactory(Cmd.SetBalDiagOCurr, data_list, callback,
                                    timeout)

    @IOCBICallBackDecorator
    def SetBalDiagOCurrCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.SetBalDiagOCurr, 3)
        cell = self.ValidateEnum(CellNum,
                                 cmd_obj.iocbi_type.ctlStatusRecord[1])
        overcurrent = self.ValidateEnum(Overcurrent,
                                        cmd_obj.iocbi_type.ctlStatusRecord[2])
        logging.info('SetBalDiagOCurr Dev:{} {} {}'
                     .format(self.DevNum, CellNum(cell).name,
                             Overcurrent(overcurrent).name))

    # Command factory for GetBalDiagOCurr
    def GetBalDiagOCurr(self, inCell=0, callback=None, timeout=1.0):
        data_list = [self.DevNum,
                     self.ValidateEnum(CellNum, inCell)]
        if callback is None:
            callback = self.GetBalDiagOCurrCB
        return self.IOCBICmdFactory(Cmd.GetBalDiagOCurr,
                                    data_list, callback, timeout)

    @IOCBICallBackDecorator
    def GetBalDiagOCurrCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.GetBalDiagOCurr, 3)
        cell = self.ValidateEnum(CellNum,
                                 cmd_obj.iocbi_type.ctlStatusRecord[1])
        overcurrent = self.ValidateEnum(Overcurrent,
                                        cmd_obj.iocbi_type.ctlStatusRecord[2])
        logging.info('GetBalDiagOCurr Dev:{} {} {}'
                     .format(self.DevNum, CellNum(cell).name,
                             Overcurrent(overcurrent).name))

    '''
    void TLE9012_SetBalanceDiagUndercurrent(uint8_t inDevice, TLE9012_CellNumber_t inCell, TLE9012_Undercurrent_t inUC);
    TLE9012_Undercurrent_t TLE9012_GetBalanceDiagUndercurrent(uint8_t inDevice, TLE9012_CellNumber_t inCell);
    '''
    # Command factory for SetBalDiagUCurr
    def SetBalDiagUCurr(self, inCell=0,
                        inOC=Undercurrent.NO_UNDERCURRENT_IN_CELL.value,
                        callback=None, timeout=1.0):
        data_list = [self.DevNum,
                     self.ValidateEnum(CellNum, inCell),
                     self.ValidateEnum(Undercurrent, inOC)]
        if callback is None:
            callback = self.SetBalDiagUCurrCB
        return self.IOCBICmdFactory(Cmd.SetBalDiagUCurr, data_list, callback,
                                    timeout)

    @IOCBICallBackDecorator
    def SetBalDiagUCurrCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.SetBalDiagUCurr, 3)
        cell = self.ValidateEnum(CellNum,
                                 cmd_obj.iocbi_type.ctlStatusRecord[1])
        overcurrent = self.ValidateEnum(Undercurrent,
                                        cmd_obj.iocbi_type.ctlStatusRecord[2])
        logging.info('SetBalDiagUCurr Dev:{} {} {}'
                     .format(self.DevNum, CellNum(cell).name,
                             Undercurrent(overcurrent).name))

    # Command factory for GetBalDiagUCurr
    def GetBalDiagUCurr(self, inCell=0, callback=None, timeout=1.0):
        data_list = [self.DevNum,
                     self.ValidateEnum(CellNum, inCell)]
        if callback is None:
            callback = self.GetBalDiagUCurrCB
        return self.IOCBICmdFactory(Cmd.GetBalDiagUCurr,
                                    data_list, callback, timeout)

    @IOCBICallBackDecorator
    def GetBalDiagUCurrCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.GetBalDiagUCurr, 3)
        cell = self.ValidateEnum(CellNum,
                                 cmd_obj.iocbi_type.ctlStatusRecord[1])
        undercurrent = self.ValidateEnum(Undercurrent,
                                        cmd_obj.iocbi_type.ctlStatusRecord[2])
        logging.info('GetBalDiagUCurr Dev:{} {} {}'
                     .format(self.DevNum, CellNum(cell).name,
                             Undercurrent(undercurrent).name))

    '''
    uint16_t TLE9012_GetCellMeasurement(uint8_t inDevice, TLE9012_CellNumber_t inCell);
    '''
    # Command factory for GetCellMeasure
    def GetCellMeasure(self, inCell=0, callback=None, timeout=1.0):
        data_list = [self.DevNum,
                     self.ValidateEnum(CellNum, inCell)]
        if callback is None:
            callback = self.GetCellMeasureCB
        return self.IOCBICmdFactory(Cmd.GetCellMeasure, data_list,
                                    callback, timeout)

    @IOCBICallBackDecorator
    def GetCellMeasureCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.GetCellMeasure, 4)
        cell = self.ValidateEnum(CellNum,
                                 cmd_obj.iocbi_type.ctlStatusRecord[1])
        val = (cmd_obj.iocbi_type.ctlStatusRecord[2] << 8) + \
            cmd_obj.iocbi_type.ctlStatusRecord[3]
        logging.info('GetCellMeasure Dev:{} {} cellMeasurement:{}'
                     .format(self.DevNum, CellNum(cell).name, val))

    '''
    uint16_t TLE9012_GetBlockVoltMeasurement(uint8_t inDevice);
    '''
    # Command factory for GetBlockMeasure
    def GetBlockMeasure(self, callback=None, timeout=1.0):
        data_list = [self.DevNum]
        if callback is None:
            callback = self.GetBlockMeasureCB
        return self.IOCBICmdFactory(Cmd.GetBlockMeasure,
                                    data_list, callback, timeout)

    @IOCBICallBackDecorator
    def GetBlockMeasureCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.
                                        GetBlockMeasure, 3)
        val = (cmd_obj.iocbi_type.ctlStatusRecord[1] << 8) + \
            cmd_obj.iocbi_type.ctlStatusRecord[2]
        logging.info('GetBlockMeasure Dev:{} value:{}'
                     .format(self.DevNum, val))

    '''
    uint16_t TLE9012_GetResultForExtTemp(uint8_t inDevice, TLE9012_ExtTemp_t inExtTemp);
    TLE9012_CurrentSrc_t TLE9012_GetCurrentSrcForExtTemp(uint8_t inDevice, TLE9012_ExtTemp_t inExtTemp);
    bool TLE9012_GetPulldownEnabledForExtTemp(uint8_t inDevice, TLE9012_ExtTemp_t inExtTemp);
    bool TLE9012_GetValidStateResultForExtTemp(uint8_t inDevice, TLE9012_ExtTemp_t inExtTemp);
    '''
    # Command factory for GetExtTempRes
    def GetExtTempRes(self, inExtTemp=ExtTemp.EXT_TMP_IDX_0.value,
                      callback=None, timeout=1.0):
        data_list = [self.DevNum,
                     self.ValidateEnum(ExtTemp, inExtTemp)]
        if callback is None:
            callback = self.GetExtTempResCB
        return self.IOCBICmdFactory(Cmd.GetExtTempRes, data_list,
                                    callback, timeout)

    @IOCBICallBackDecorator
    def GetExtTempResCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.GetExtTempRes, 4)
        ext_temp = self.ValidateEnum(ExtTemp,
                                     cmd_obj.iocbi_type.ctlStatusRecord[1])
        val = (cmd_obj.iocbi_type.ctlStatusRecord[2] << 8) + \
            cmd_obj.iocbi_type.ctlStatusRecord[3]
        logging.info('GetExtTempRes Dev:{} {} external_temp_result:{}'
                     .format(self.DevNum,
                             ExtTemp(ext_temp).name, val))

    # Command factory for GetExtTempSrc
    def GetExtTempSrc(self, inExtTemp=ExtTemp.EXT_TMP_IDX_0.value,
                      callback=None, timeout=1.0):
        data_list = [self.DevNum,
                     self.ValidateEnum(ExtTemp, inExtTemp)]
        if callback is None:
            callback = self.GetExtTempSrcCB
        return self.IOCBICmdFactory(Cmd.GetExtTempSrc,
                                    data_list, callback, timeout)

    @IOCBICallBackDecorator
    def GetExtTempSrcCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.GetExtTempSrc, 3)
        ext_temp = self.ValidateEnum(ExtTemp,
                                     cmd_obj.iocbi_type.ctlStatusRecord[1])
        current_src = self.ValidateEnum(OTemp,
                                        cmd_obj.iocbi_type.ctlStatusRecord[2])
        logging.info('GetExtTempSrc Dev:{} {} {}'
                     .format(self.DevNum, ExtTemp(ext_temp).name,
                             CurrentSrc(current_src).name))

    # Command factory for GetExtTempPd
    def GetExtTempPd(self, inExtTemp=ExtTemp.EXT_TMP_IDX_0.value,
                     callback=None, timeout=1.0):
        data_list = [self.DevNum,
                     self.ValidateEnum(ExtTemp, inExtTemp)]
        if callback is None:
            callback = self.GetExtTempPdCB
        return self.IOCBICmdFactory(Cmd.GetExtTempPd,
                                    data_list, callback, timeout)

    @IOCBICallBackDecorator
    def GetExtTempPdCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.GetExtTempPd, 3)
        config = self.ValidateEnum(ExtTemp,
                                   cmd_obj.iocbi_type.ctlStatusRecord[1])
        pulldown_enabled = self.ValidateBool(cmd_obj.iocbi_type.
                                             ctlStatusRecord[2])
        logging.info('GetExtTempPd Dev:{} {} enabled:{}'
                     .format(self.DevNum, ExtTemp(config).name,
                             pulldown_enabled))

    # Command factory for GetExtTempValid
    def GetExtTempValid(self, inExtTemp=ExtTemp.EXT_TMP_IDX_0.value,
                        callback=None, timeout=1.0):
        data_list = [self.DevNum, self.ValidateEnum(ExtTemp, inExtTemp)]
        if callback is None:
            callback = self.GetExtTempValidCB
        return self.IOCBICmdFactory(Cmd.GetExtTempValid,
                                    data_list, callback, timeout)

    @IOCBICallBackDecorator
    def GetExtTempValidCB(self, cmd_obj):
        self.ValidateResponseDataLength(Cmd.GetExtTempValid, 3)
        config = self.ValidateEnum(ExtTemp,
                                   cmd_obj.iocbi_type.ctlStatusRecord[1])
        valid_state = self.ValidateBool(cmd_obj.iocbi_type.ctlStatusRecord[2])
        logging.info('GetExtTempValid Dev:{} {} valid_state:{}'
                     .format(self.DevNum, ExtTemp(config).name, valid_state))

    # The following are stubs in tle9012_app.c
    '''
    bool TLE9012_GetGPIOInputState(uint8_t inDevice, TLE9012_GpioPwm_t inPwm);

    void TLE9012_SetGPIOOutputState(uint8_t inDevice, TLE9012_GpioPwm_t inPwm, bool inOutput);
    bool TLE9012_GetGPIOOutputState(uint8_t inDevice, TLE9012_GpioPwm_t inPwm);

    void TLE9012_SetGPIODirection(uint8_t inDevice, TLE9012_GpioPwm_t inPwm, TLE9012_GpioPwmDir_t inDirection);
    TLE9012_GpioPwmDir_t TLE9012_GetGPIODirection(uint8_t inDevice, TLE9012_GpioPwm_t inPwm);

    bool TLE9012_GetPWMInputState(uint8_t inDevice, TLE9012_GpioPwm_t inPwm);

    void TLE9012_SetPWMEnable(uint8_t inDevice, TLE9012_GpioPwm_t inPwm, bool inEnable);
    bool TLE9012_GetPWMEnable(uint8_t inDevice, TLE9012_GpioPwm_t inPwm);

    void TLE9012_SetPWMOutputState(uint8_t inDevice, TLE9012_GpioPwm_t inPwm, bool inOutput);
    bool TLE9012_GetPWMOutputState(uint8_t inDevice, TLE9012_GpioPwm_t inPwm);

    void TLE9012_SetPWMDirection(uint8_t inDevice, TLE9012_GpioPwm_t inPwm, TLE9012_GpioPwmDir_t inDirection);
    TLE9012_GpioPwmDir_t TLE9012_GetPWMDirection(uint8_t inDevice, TLE9012_GpioPwm_t inPwm);

    void TLE9012_SetGPIOPwmUVoltErr(uint8_t inDevice, bool inUndervoltageError);
    bool TLE9012_GetGPIOPwmUVoltErr(uint8_t inDevice);

    void TLE9012_SetPwmPeriodDuty(uint8_t inDevice, TLE9012_GpioPwm_t inPwm, uint8_t inPeriod, uint8_t inDutyCycle);
    uint8_t TLE9012_GetPWMPeriod(uint8_t inDevice, TLE9012_GpioPwm_t inPwm);
    uint8_t TLE9012_GetPWMDutyCycle(uint8_t inDevice, TLE9012_GpioPwm_t inPwm);
    '''
