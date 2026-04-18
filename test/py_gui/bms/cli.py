# file:      cli.py
# author:    rmilne
# version:   0.1.0
# date:      Aug2020
# brief:     Click cli main

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
import sys
# import os
import click
import linecache
# import time
from canlib import canlib
import shutil
import logging
from bms.docan import DoCAN
from bms.kvaser import Kvaser
from bms.cmds import IOCBI, IocbiType, RoutineControl, RcType, RC_REQ_SUBFUNC
import bms.tle9012 as Csc
import colorama

# Required for windows coloured text - no effect if using ANSI compliant shell
colorama.init()
# Colour codes
REDCC = '\x1b[31m'
ENDCC = '\x1b[m'

'''
# Useful for output formatting ie: colourful text or machine readable
click.echo('Piped input:%s' % str(not sys.stdin.isatty()))
click.echo('Piped output:%s' % str(not sys.stdout.isatty()))
click.echo('Piped error:%s' % str(not sys.stderr.isatty()))
'''
'''
logging levels - set here for amount of detail in log file
CRITICAL
ERROR
WARNING
INFO
DEBUG
NOTSET
'''
logging.basicConfig(filename='', level=logging.DEBUG)
# Default source address
ID_SA = 0x0a
# Default Target address
ID_TA = 0x0b

# IOCBI data to obtain evadc goup3 channels 0-3
get_evadc_g3ch0 = IocbiType(0xe1, 0)
get_evadc_g3ch1 = IocbiType(0xe1, 1)
get_evadc_g3ch2 = IocbiType(0xe1, 2)
get_evadc_g3ch3 = IocbiType(0xe1, 3)

# Routine Control data (segmented tx test)
routineControlOption = [
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
    0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
    0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f]
rc_test = RcType(RC_REQ_SUBFUNC.STR.value, 0x02, 0x00, routineControlOption)


@click.command()
@click.option('--channel', type=int, default=1, help='Kvaser channel number.')
@click.option('--sa', type=int, default=ID_SA, help='Src address (8-bit)')
@click.option('--ta', type=int, default=ID_TA, help='Target address (8-bit)')
@click.option('--is_fd', type=bool, default=False,
              help='Set to True to obtain evadc CANFD.')
def cli(channel, sa, ta, is_fd):
    '''
    Run hardcoded UDSonCAN tests
    TODO: FD len>8; cmd object handling (+ protocol);  RK header -> cmd_obj
    '''
    # print_channels()
    if sys.stdout.isatty():
        width, height = shutil.get_terminal_size((80, 20))
        form = '═^' + str(width - 1)
        print(format('BMS CLI', form))

    try:
        can_cls = Kvaser(channel, is_fd, True)
    except canlib.CanNotFound:
        # Kvaser hw not attached - exit immediately
        sys.exit()

    try:
        # Start kvaser Rx task
        can_cls.start()
        # Instantiate UDS over CAN command processor
        bms = DoCAN(sa, ta, can_cls)
        # Instantiate first TLE9012 object in chain
        csc0 = Csc.TLE9012(0)

        # Create EVADC commands
        cmd_get_evadc_0 = IOCBI(get_evadc_g3ch0, evadc_callback, 0.1)
        cmd_get_evadc_1 = IOCBI(get_evadc_g3ch1, evadc_callback, 0.1)
        cmd_get_evadc_2 = IOCBI(get_evadc_g3ch2, evadc_callback, 0.1)
        cmd_get_evadc_3 = IOCBI(get_evadc_g3ch3, evadc_callback, 0.1)
        cmd_rc_test = RoutineControl(rc_test, rc_test_callback, 0.1)

        # Add commands to cmd dictionary
        bms.AddCmd("GET_EVADC_G3CH0", cmd_get_evadc_0)
        bms.AddCmd("GET_EVADC_G3CH1", cmd_get_evadc_1)
        bms.AddCmd("GET_EVADC_G3CH2", cmd_get_evadc_2)
        bms.AddCmd("GET_EVADC_G3CH3", cmd_get_evadc_3)
        bms.AddCmd("RC_test", cmd_rc_test)

        # Test local callbacks
        bms.AddCmd("SetCellEnable", csc0.SetCellEnable(Csc.CellNum.C_0.value,
                   Csc.CellEn.ENABLE.value, SetCellEnableCB))
        bms.AddCmd("GetCellEnable", csc0.GetCellEnable(Csc.CellNum.C_0.value,
                   GetCellEnableCB, 0.1))

        # Exe commands in dictionary
        for key in bms.cmd_dict:
            if not bms.ExecuteDict(key):
                raise ValueError("cmd tx queue OVF")
        bms.WaitExecuteQueueComplete()
        # Clear command dictionary
        bms.DelCmd()

        # Execute commands without dictionary

        # Test Csc.CellEn enum range (1-bit range) over cell range
        bms.Exe(csc0.SetCellEnable(Csc.CellNum.C_0.value,
                                   Csc.CellEn.DISABLE.value))
        bms.Exe(csc0.GetCellEnable(Csc.CellNum.C_0.value))
        bms.Exe(csc0.SetCellEnable(Csc.CellNum.C_0.value,
                                   Csc.CellEn.ENABLE.value))
        bms.Exe(csc0.GetCellEnable(Csc.CellNum.C_0.value))
        bms.Exe(csc0.SetCellEnable(Csc.CellNum.C_11.value,
                                   Csc.CellEn.DISABLE.value))
        bms.Exe(csc0.GetCellEnable(Csc.CellNum.C_11.value))
        bms.Exe(csc0.SetCellEnable(Csc.CellNum.C_11.value,
                                   Csc.CellEn.ENABLE.value))
        bms.Exe(csc0.GetCellEnable(Csc.CellNum.C_11.value))

        # Test arg 6-bit range
        bms.Exe(csc0.SetMaxVoltDropThd(0))
        bms.Exe(csc0.GetMaxVoltDropThd())
        bms.Exe(csc0.SetMaxVoltDropThd(0x3f))
        bms.Exe(csc0.GetMaxVoltDropThd())

        # Test arg 10-bit range
        bms.Exe(csc0.SetOVoltFltThd(0))
        bms.Exe(csc0.GetOVoltFltThd())
        bms.Exe(csc0.SetOVoltFltThd(0x03ff))
        bms.Exe(csc0.GetOVoltFltThd())

        # Test arg 6-bit range
        bms.Exe(csc0.SetMinVoltDropThd(0))
        bms.Exe(csc0.GetMinVoltDropThd())
        bms.Exe(csc0.SetMinVoltDropThd(0x3f))
        bms.Exe(csc0.GetMinVoltDropThd())

        # Test arg 10-bit range
        bms.Exe(csc0.SetUVoltFltThd(0))
        bms.Exe(csc0.GetUVoltFltThd())
        bms.Exe(csc0.SetUVoltFltThd(0x03ff))
        bms.Exe(csc0.GetUVoltFltThd())

        # Test arg 10-bit range
        bms.Exe(csc0.SetExtTempOvertempThd(0))
        bms.Exe(csc0.GetExtTempOvertempThd())
        bms.Exe(csc0.SetExtTempOvertempThd(0x3ff))
        bms.Exe(csc0.GetExtTempOvertempThd())

        # Test Csc.CurrentSrc enum range (2-bit)
        bms.Exe(csc0.SetOtFltCurrSrc(Csc.CurrentSrc.I0_USED.value))
        bms.Exe(csc0.GetOtFltCurrSrc())
        bms.Exe(csc0.SetOtFltCurrSrc(Csc.CurrentSrc.I3_USED.value))
        bms.Exe(csc0.GetOtFltCurrSrc())

        # Test Csc.ExtTempSensorsUsed range
        bms.Exe(csc0.SetExtTempSensorsUsed(Csc.ExtTempSensorsUsed.
                                           NO_EXT_TEMP_SENSE.value))
        bms.Exe(csc0.GetExtTempSensorsUsed())
        bms.Exe(csc0.SetExtTempSensorsUsed(Csc.ExtTempSensorsUsed.
                                           TMP0_1_2_3_4_ACTIVE.value))
        bms.Exe(csc0.GetExtTempSensorsUsed())

        # Test arg 10-bit range
        bms.Exe(csc0.SetIntTempOvertempThd(0))
        bms.Exe(csc0.GetIntTempOvertempThd())
        bms.Exe(csc0.SetIntTempOvertempThd(0x03ff))
        bms.Exe(csc0.GetIntTempOvertempThd())

        # Arg has 3-bit range
        bms.Exe(csc0.SetNumConsecErr(0))
        bms.Exe(csc0.GetNumConsecErr())
        bms.Exe(csc0.SetNumConsecErr(7))
        bms.Exe(csc0.GetNumConsecErr())

        # Test Csc.ExtTempTrigForRR range (3-bit range)
        bms.Exe(csc0.SetExtTempTrigForRR(Csc.ExtTempTrigForRR.EVERY_RR.value))
        bms.Exe(csc0.GetExtTempTrigForRR())
        bms.Exe(csc0.SetExtTempTrigForRR(Csc.ExtTempTrigForRR.
                                         ONE_MEASURE_7_NO_MEASURE.value))
        bms.Exe(csc0.GetExtTempTrigForRR())

        # Arg has 10-bit range
        bms.Exe(csc0.SetSleepModeTimingForRR(0))
        bms.Exe(csc0.GetSleepModeTimingForRR())
        bms.Exe(csc0.SetSleepModeTimingForRR(0x3ff))
        bms.Exe(csc0.GetSleepModeTimingForRR())

        # Arg has 8-bit range
        bms.Exe(csc0.SetRRCounter(0))
        bms.Exe(csc0.GetRRCounter())
        bms.Exe(csc0.SetRRCounter(0x7f))
        bms.Exe(csc0.GetRRCounter())

        # Test Csc.RRSync enum range (1-bit range)
        bms.Exe(csc0.SetRRSync(Csc.RRSync.NO_SYNC_WITH_WDG_COUNTER.value))
        bms.Exe(csc0.GetRRSync())
        bms.Exe(csc0.SetRRSync(Csc.RRSync.SYNC_WITH_WDG_COUNTER.value))
        bms.Exe(csc0.GetRRSync())

        # Test Csc.RRCfg enum range
        bms.Exe(csc0.SetRRCfgMsk(Csc.RRCfg.MASK_NR_ERR_COUNTER_ADC_ERROR.value, False))
        bms.Exe(csc0.GetRRCfgMsk(Csc.RRCfg.MASK_NR_ERR_COUNTER_ADC_ERROR.value))
        bms.Exe(csc0.SetRRCfgMsk(Csc.RRCfg.MASK_NR_ERR_COUNTER_ADC_ERROR.value, True))
        bms.Exe(csc0.GetRRCfgMsk(Csc.RRCfg.MASK_NR_ERR_COUNTER_ADC_ERROR.value))
        bms.Exe(csc0.SetRRCfgMsk(Csc.RRCfg.MASK_NR_ERR_COUNTER_BALANCING_ERROR_OVERCURRENT.value, False))
        bms.Exe(csc0.GetRRCfgMsk(Csc.RRCfg.MASK_NR_ERR_COUNTER_BALANCING_ERROR_OVERCURRENT.value))
        bms.Exe(csc0.SetRRCfgMsk(Csc.RRCfg.MASK_NR_ERR_COUNTER_BALANCING_ERROR_OVERCURRENT.value, True))
        bms.Exe(csc0.GetRRCfgMsk(Csc.RRCfg.MASK_NR_ERR_COUNTER_BALANCING_ERROR_OVERCURRENT.value))

        # Test Csc.FltMskCfg enum range
        bms.Exe(csc0.SetFltMskCfg(Csc.FltMskCfg.ENABLE_ERROR_PIN_FUNCTIONALITY.value, False))
        bms.Exe(csc0.GetFltMskCfg(Csc.FltMskCfg.ENABLE_ERROR_PIN_FUNCTIONALITY.value))
        bms.Exe(csc0.SetFltMskCfg(Csc.FltMskCfg.ENABLE_ERROR_PIN_FUNCTIONALITY.value, True))
        bms.Exe(csc0.GetFltMskCfg(Csc.FltMskCfg.ENABLE_ERROR_PIN_FUNCTIONALITY.value))
        bms.Exe(csc0.SetFltMskCfg(Csc.FltMskCfg.EMM_ERR_BALANCING_ERROR_OVERCURRENT.value, False))
        bms.Exe(csc0.GetFltMskCfg(Csc.FltMskCfg.EMM_ERR_BALANCING_ERROR_OVERCURRENT.value))
        bms.Exe(csc0.SetFltMskCfg(Csc.FltMskCfg.EMM_ERR_BALANCING_ERROR_OVERCURRENT.value, True))
        bms.Exe(csc0.GetFltMskCfg(Csc.FltMskCfg.EMM_ERR_BALANCING_ERROR_OVERCURRENT.value))

        # Test Csc.GenDiag enum range
        bms.Exe(csc0.SetGenDiagMsk(Csc.GenDiag.GPIO_WAKEUP_ENABLED.value, False))
        bms.Exe(csc0.GetGenDiagMsk(Csc.GenDiag.GPIO_WAKEUP_ENABLED.value))
        bms.Exe(csc0.SetGenDiagMsk(Csc.GenDiag.GPIO_WAKEUP_ENABLED.value, True))
        bms.Exe(csc0.GetGenDiagMsk(Csc.GenDiag.GPIO_WAKEUP_ENABLED.value))
        bms.Exe(csc0.SetGenDiagMsk(Csc.GenDiag.ENABLE_BALANCING_ERROR_OVERCURRENT.value, False))
        bms.Exe(csc0.GetGenDiagMsk(Csc.GenDiag.ENABLE_BALANCING_ERROR_OVERCURRENT.value))
        bms.Exe(csc0.SetGenDiagMsk(Csc.GenDiag.ENABLE_BALANCING_ERROR_OVERCURRENT.value, True))
        bms.Exe(csc0.GetGenDiagMsk(Csc.GenDiag.ENABLE_BALANCING_ERROR_OVERCURRENT.value))

        # Test Csc.UVolt enum range (1-bit) over cell range
        bms.Exe(csc0.SetCellUVoltFlg(Csc.CellNum.C_0.value,
                                     Csc.UVolt.NO_UNDERVOLTAGE_IN_CELL.value))
        bms.Exe(csc0.GetCellUVoltFlg(Csc.CellNum.C_0.value))
        bms.Exe(csc0.SetCellUVoltFlg(Csc.CellNum.C_0.value,
                                     Csc.UVolt.UNDERVOLTAGE_IN_CELL.value))
        bms.Exe(csc0.GetCellUVoltFlg(Csc.CellNum.C_0.value))
        bms.Exe(csc0.SetCellUVoltFlg(Csc.CellNum.C_11.value,
                                     Csc.UVolt.NO_UNDERVOLTAGE_IN_CELL.value))
        bms.Exe(csc0.GetCellUVoltFlg(Csc.CellNum.C_11.value))
        bms.Exe(csc0.SetCellUVoltFlg(Csc.CellNum.C_11.value,
                                     Csc.UVolt.UNDERVOLTAGE_IN_CELL.value))
        bms.Exe(csc0.GetCellUVoltFlg(Csc.CellNum.C_11.value))

        # Test Csc.OVolt enum range (1-bit) over cell range
        bms.Exe(csc0.SetCellOVoltFlg(Csc.CellNum.C_0.value,
                                     Csc.OVolt.NO_OVERVOLTAGE_IN_CELL.value))
        bms.Exe(csc0.GetCellOVoltFlg(Csc.CellNum.C_0.value))
        bms.Exe(csc0.SetCellOVoltFlg(Csc.CellNum.C_0.value,
                                     Csc.OVolt.OVERVOLTAGE_IN_CELL.value))
        bms.Exe(csc0.GetCellOVoltFlg(Csc.CellNum.C_0.value))
        bms.Exe(csc0.SetCellOVoltFlg(Csc.CellNum.C_11.value,
                                     Csc.OVolt.NO_OVERVOLTAGE_IN_CELL.value))
        bms.Exe(csc0.GetCellOVoltFlg(Csc.CellNum.C_11.value))
        bms.Exe(csc0.SetCellOVoltFlg(Csc.CellNum.C_11.value,
                                     Csc.OVolt.OVERVOLTAGE_IN_CELL.value))
        bms.Exe(csc0.GetCellOVoltFlg(Csc.CellNum.C_11.value))

        # Test Csc.Openload enum range (1-bit) over Csc.ExtTemp enum range
        bms.Exe(csc0.SetExtTempDiagOpenFlg(Csc.ExtTemp.EXT_TMP_IDX_0.value,
                                           Csc.Openload.NO_OPENLOAD.value))
        bms.Exe(csc0.GetExtTempDiagOpenFlg(Csc.ExtTemp.EXT_TMP_IDX_0.value))
        bms.Exe(csc0.SetExtTempDiagOpenFlg(Csc.ExtTemp.EXT_TMP_IDX_0.value,
                                           Csc.Openload.OPENLOAD.value))
        bms.Exe(csc0.GetExtTempDiagOpenFlg(Csc.ExtTemp.EXT_TMP_IDX_0.value))
        bms.Exe(csc0.SetExtTempDiagOpenFlg(Csc.ExtTemp.EXT_TMP_IDX_4.value,
                                           Csc.Openload.NO_OPENLOAD.value))
        bms.Exe(csc0.GetExtTempDiagOpenFlg(Csc.ExtTemp.EXT_TMP_IDX_4.value))
        bms.Exe(csc0.SetExtTempDiagOpenFlg(Csc.ExtTemp.EXT_TMP_IDX_4.value,
                                           Csc.Openload.OPENLOAD.value))
        bms.Exe(csc0.GetExtTempDiagOpenFlg(Csc.ExtTemp.EXT_TMP_IDX_4.value))

        # Test Csc.ShrtCirc enum range (1-bit) over Csc.ExtTemp enum range
        bms.Exe(csc0.SetExtTempDiagShortFlg(Csc.ExtTemp.EXT_TMP_IDX_0.value,
                                           Csc.ShrtCirc.NO_SHORT_CIRCUIT.value))
        bms.Exe(csc0.GetExtTempDiagShortFlg(Csc.ExtTemp.EXT_TMP_IDX_0.value))
        bms.Exe(csc0.SetExtTempDiagShortFlg(Csc.ExtTemp.EXT_TMP_IDX_0.value,
                                           Csc.ShrtCirc.SHORT_CIRCUIT.value))
        bms.Exe(csc0.GetExtTempDiagShortFlg(Csc.ExtTemp.EXT_TMP_IDX_0.value))
        bms.Exe(csc0.SetExtTempDiagShortFlg(Csc.ExtTemp.EXT_TMP_IDX_4.value,
                                           Csc.ShrtCirc.NO_SHORT_CIRCUIT.value))
        bms.Exe(csc0.GetExtTempDiagShortFlg(Csc.ExtTemp.EXT_TMP_IDX_4.value))
        bms.Exe(csc0.SetExtTempDiagShortFlg(Csc.ExtTemp.EXT_TMP_IDX_4.value,
                                           Csc.ShrtCirc.SHORT_CIRCUIT.value))
        bms.Exe(csc0.GetExtTempDiagShortFlg(Csc.ExtTemp.EXT_TMP_IDX_4.value))

        # Test Csc.OTemp enum range (1-bit) over Csc.ExtTemp enum range
        bms.Exe(csc0.SetExtTempDiagOtFlg(Csc.ExtTemp.EXT_TMP_IDX_0.value,
                                           Csc.OTemp.NO_OVERTEMPERATURE_IN_EXT_TEMP.value))
        bms.Exe(csc0.GetExtTempDiagOtFlg(Csc.ExtTemp.EXT_TMP_IDX_0.value))
        bms.Exe(csc0.SetExtTempDiagOtFlg(Csc.ExtTemp.EXT_TMP_IDX_0.value,
                                           Csc.OTemp.OVERTEMPERATURE_IN_EXT_TEMP.value))
        bms.Exe(csc0.GetExtTempDiagOtFlg(Csc.ExtTemp.EXT_TMP_IDX_0.value))
        bms.Exe(csc0.SetExtTempDiagOtFlg(Csc.ExtTemp.EXT_TMP_IDX_4.value,
                                           Csc.OTemp.NO_OVERTEMPERATURE_IN_EXT_TEMP.value))
        bms.Exe(csc0.GetExtTempDiagOtFlg(Csc.ExtTemp.EXT_TMP_IDX_4.value))
        bms.Exe(csc0.SetExtTempDiagOtFlg(Csc.ExtTemp.EXT_TMP_IDX_4.value,
                                           Csc.OTemp.OVERTEMPERATURE_IN_EXT_TEMP.value))
        bms.Exe(csc0.GetExtTempDiagOtFlg(Csc.ExtTemp.EXT_TMP_IDX_4.value))

        # Test Csc.Openload enum range (1-bit) over cell range
        bms.Exe(csc0.SetCellOpenloadFlg(Csc.CellNum.C_0.value,
                                        Csc.Openload.NO_OPENLOAD.value))
        bms.Exe(csc0.GetCellOpenloadFlg(Csc.CellNum.C_0.value))
        bms.Exe(csc0.SetCellOpenloadFlg(Csc.CellNum.C_0.value,
                                        Csc.Openload.OPENLOAD.value))
        bms.Exe(csc0.GetCellOpenloadFlg(Csc.CellNum.C_0.value))
        bms.Exe(csc0.SetCellOpenloadFlg(Csc.CellNum.C_11.value,
                                        Csc.Openload.NO_OPENLOAD.value))
        bms.Exe(csc0.GetCellOpenloadFlg(Csc.CellNum.C_11.value))
        bms.Exe(csc0.SetCellOpenloadFlg(Csc.CellNum.C_11.value,
                                        Csc.Openload.OPENLOAD.value))
        bms.Exe(csc0.GetCellOpenloadFlg(Csc.CellNum.C_11.value))

        # Test boolean range
        bms.Exe(csc0.SetExtendWdg(False))
        bms.Exe(csc0.GetExtendWdg())
        bms.Exe(csc0.SetExtendWdg(True))
        bms.Exe(csc0.GetExtendWdg())

        # Test boolean range
        bms.Exe(csc0.SetActivateSleepMode(False))
        bms.Exe(csc0.GetActivateSleepMode())
        bms.Exe(csc0.SetActivateSleepMode(True))
        bms.Exe(csc0.GetActivateSleepMode())

        # Test arg 8-bit range
        bms.Exe(csc0.SetUCurrFltThd(0))
        bms.Exe(csc0.GetUCurrFltThd())
        bms.Exe(csc0.SetUCurrFltThd(0xff))
        bms.Exe(csc0.GetUCurrFltThd())

        # Test arg 8-bit range
        bms.Exe(csc0.SetOCurrFltThd(0))
        bms.Exe(csc0.GetOCurrFltThd())
        bms.Exe(csc0.SetOCurrFltThd(0xff))
        bms.Exe(csc0.GetOCurrFltThd())

        # Test Csc.BalSwitchState enum range (1-bit) over cell range
        bms.Exe(csc0.SetBalState(Csc.CellNum.C_0.value,
                                 Csc.BalSwitchState.DRIVER_OFF.value))
        bms.Exe(csc0.GetBalState(Csc.CellNum.C_0.value))
        bms.Exe(csc0.SetBalState(Csc.CellNum.C_0.value,
                                 Csc.BalSwitchState.DRIVER_ON.value))
        bms.Exe(csc0.GetBalState(Csc.CellNum.C_0.value))
        bms.Exe(csc0.SetBalState(Csc.CellNum.C_11.value,
                                 Csc.BalSwitchState.DRIVER_OFF.value))
        bms.Exe(csc0.GetBalState(Csc.CellNum.C_11.value))
        bms.Exe(csc0.SetBalState(Csc.CellNum.C_11.value,
                                 Csc.BalSwitchState.DRIVER_ON.value))
        bms.Exe(csc0.GetBalState(Csc.CellNum.C_11.value))

        # Test Csc.AVMExtTempDiag enum range
        bms.Exe(csc0.SetAVMExtTempDiagPd(Csc.AVMExtTempDiag.SELECT_EXT_TEMP_DIAGNOSE_FOR_AVM_PULLDOWN_AUX_1_ACTIVE.value))
        bms.Exe(csc0.GetAVMExtTempDiagPd())
        bms.Exe(csc0.SetAVMExtTempDiagPd(Csc.AVMExtTempDiag.SELECT_EXT_TEMP_DIAGNOSE_FOR_AVM_NO_PULLDOWN.value))
        bms.Exe(csc0.GetAVMExtTempDiagPd())

        # Test Csc.DiagnosisResistorMask enum (1-bit) over Csc.AuxVoltDiag range
        bms.Exe(csc0.SetDiagResMskFlg(Csc.AuxVoltDiag.DIAGNOSTIC_MEASUREMENT_MASK_EXT_TEMP_0_FOR_AVM.value,
                                      Csc.DiagnosisResistorMask.AVM_MEASUREMENT_MASKED_OUT.value))
        bms.Exe(csc0.GetDiagResMskFlg(Csc.AuxVoltDiag.DIAGNOSTIC_MEASUREMENT_MASK_EXT_TEMP_0_FOR_AVM.value))
        bms.Exe(csc0.SetDiagResMskFlg(Csc.AuxVoltDiag.DIAGNOSTIC_MEASUREMENT_MASK_EXT_TEMP_0_FOR_AVM.value,
                                      Csc.DiagnosisResistorMask.AVM_MEASUREMENT_PERFORMED_ON_AVM_START.value))
        bms.Exe(csc0.GetDiagResMskFlg(Csc.AuxVoltDiag.DIAGNOSTIC_MEASUREMENT_MASK_EXT_TEMP_0_FOR_AVM.value))
        bms.Exe(csc0.SetDiagResMskFlg(Csc.AuxVoltDiag.DIAGNOSTIC_MEASUREMENT_MASK_RESISTOR_FOR_AVM.value,
                                      Csc.DiagnosisResistorMask.AVM_MEASUREMENT_MASKED_OUT.value))
        bms.Exe(csc0.GetDiagResMskFlg(Csc.AuxVoltDiag.DIAGNOSTIC_MEASUREMENT_MASK_RESISTOR_FOR_AVM.value))
        bms.Exe(csc0.SetDiagResMskFlg(Csc.AuxVoltDiag.DIAGNOSTIC_MEASUREMENT_MASK_RESISTOR_FOR_AVM.value,
                                      Csc.DiagnosisResistorMask.AVM_MEASUREMENT_PERFORMED_ON_AVM_START.value))
        bms.Exe(csc0.GetDiagResMskFlg(Csc.AuxVoltDiag.DIAGNOSTIC_MEASUREMENT_MASK_RESISTOR_FOR_AVM.value))

        # Test Csc.Overcurrent enum range (1-bit) over cell range
        bms.Exe(csc0.SetBalDiagOCurr(Csc.CellNum.C_0.value,
                                     Csc.Overcurrent.NO_OVERCURRENT_IN_CELL.value))
        bms.Exe(csc0.GetBalDiagOCurr(Csc.CellNum.C_0.value))
        bms.Exe(csc0.SetBalDiagOCurr(Csc.CellNum.C_0.value,
                                     Csc.Overcurrent.OVERCURRENT_IN_CELL.value))
        bms.Exe(csc0.GetBalDiagOCurr(Csc.CellNum.C_0.value))
        bms.Exe(csc0.SetBalDiagOCurr(Csc.CellNum.C_11.value,
                                     Csc.Overcurrent.NO_OVERCURRENT_IN_CELL.value))
        bms.Exe(csc0.GetBalDiagOCurr(Csc.CellNum.C_11.value))
        bms.Exe(csc0.SetBalDiagOCurr(Csc.CellNum.C_11.value,
                                     Csc.Overcurrent.OVERCURRENT_IN_CELL.value))
        bms.Exe(csc0.GetBalDiagOCurr(Csc.CellNum.C_11.value))

        # Test Csc.Undercurrent enum range (1-bit) over cell range
        bms.Exe(csc0.SetBalDiagUCurr(Csc.CellNum.C_0.value,
                                     Csc.Undercurrent.NO_UNDERCURRENT_IN_CELL.value))
        bms.Exe(csc0.GetBalDiagUCurr(Csc.CellNum.C_0.value))
        bms.Exe(csc0.SetBalDiagUCurr(Csc.CellNum.C_0.value,
                                     Csc.Undercurrent.UNDERCURRENT_IN_CELL.value))
        bms.Exe(csc0.GetBalDiagUCurr(Csc.CellNum.C_0.value))
        bms.Exe(csc0.SetBalDiagUCurr(Csc.CellNum.C_11.value,
                                     Csc.Undercurrent.NO_UNDERCURRENT_IN_CELL.value))
        bms.Exe(csc0.GetBalDiagUCurr(Csc.CellNum.C_11.value))
        bms.Exe(csc0.SetBalDiagUCurr(Csc.CellNum.C_11.value,
                                     Csc.Undercurrent.UNDERCURRENT_IN_CELL.value))
        bms.Exe(csc0.GetBalDiagUCurr(Csc.CellNum.C_11.value))

        bms.Exe(csc0.GetCellMeasure(Csc.CellNum.C_0.value))
        bms.Exe(csc0.GetCellMeasure(Csc.CellNum.C_11.value))

        bms.Exe(csc0.GetBlockMeasure())

        bms.Exe(csc0.GetExtTempRes(Csc.ExtTemp.EXT_TMP_IDX_0.value))
        bms.Exe(csc0.GetExtTempRes(Csc.ExtTemp.EXT_TMP_IDX_4.value))
        bms.Exe(csc0.GetExtTempSrc(Csc.ExtTemp.EXT_TMP_IDX_0.value))
        bms.Exe(csc0.GetExtTempSrc(Csc.ExtTemp.EXT_TMP_IDX_4.value))
        bms.Exe(csc0.GetExtTempPd(Csc.ExtTemp.EXT_TMP_IDX_0.value))
        bms.Exe(csc0.GetExtTempPd(Csc.ExtTemp.EXT_TMP_IDX_4.value))
        bms.Exe(csc0.GetExtTempValid(Csc.ExtTemp.EXT_TMP_IDX_0.value))
        bms.Exe(csc0.GetExtTempValid(Csc.ExtTemp.EXT_TMP_IDX_4.value))

    except KeyboardInterrupt:
        print('KeyboardInterrupt')
    except:
        PrintException()

    finally:
        bms.dispose()


'''
Callback for IOCBI commands
NB: invoked inside bms.tx_thread context
'''
def evadc_callback(cmd_obj):
    '''
    Returned data structure in IOCBI.rx_data:

    typedef struct BMS_IOCB_GETAVOL_s
    {
        uint8_t CHANNEL[2];
        VOLTAGE_VAL_t VOLTAGE;
        uint8_t SIGN;
    } BMS_IOCB_GETAVOL_t;
    '''
    index = cmd_obj.RX_OVERHEAD
    if (cmd_obj.iocbi_type.dataIdentifier1 == 0xe1 and
            len(cmd_obj.rx_data) == (index + 7)):
        group = cmd_obj.rx_data[index]
        channel = cmd_obj.rx_data[index + 1]
        evadc_res = cmd_obj.rx_data[index + 2] << 24
        evadc_res += cmd_obj.rx_data[index + 3] << 16
        evadc_res += cmd_obj.rx_data[index + 4] << 8
        evadc_res += cmd_obj.rx_data[index + 5]
        # sign = cmd_obj.rx_data[index + 6]
        logging.info("{} G[{}]CH[{}] voltage:{}"
                     .format(cmd_obj, group, channel, evadc_res))
    else:
        logging.error("IOCBI callback error")


def rc_test_callback(cmd_obj):
    if cmd_obj.rc_type.routineInfo != -1 and \
            len(cmd_obj.rx_data) > cmd_obj.RX_OVERHEAD:
        logging.info("{} Info:{} StatusRecord:{}"
                     .format(cmd_obj, cmd_obj.rc_type.routineInfo,
                             cmd_obj.rx_data[cmd_obj.RX_OVERHEAD:]))


def SetCellEnableCB(cmd_obj):
    if len(cmd_obj.rx_data) > cmd_obj.RX_OVERHEAD:
        data_str = ''.join("%02x " % i for i in
                           cmd_obj.rx_data[cmd_obj.RX_OVERHEAD:])
        logging.info('{} [ '.format(cmd_obj) + data_str + ']')
    else:
        logging.info('{}'.format(cmd_obj))


def GetCellEnableCB(cmd_obj):
    if len(cmd_obj.rx_data) == cmd_obj.RX_OVERHEAD + 3:
        logging.info("{} GetCellEnable value:{}"
                     .format(cmd_obj,
                             cmd_obj.rx_data[cmd_obj.RX_OVERHEAD + 2]))


# https://stackoverflow.com/questions/14519177/python-exception-handling-line-number
def PrintException():
    exc_type, exc_obj, tb = sys.exc_info()
    f = tb.tb_frame
    lineno = tb.tb_lineno
    filename = f.f_code.co_filename
    linecache.checkcache(filename)
    line = linecache.getline(filename, lineno, f.f_globals)
    if not sys.stdout.isatty():
        sys.stderr.write('EXCEPTION IN ({}, LINE {} "{}"): {}'
                         .format(filename, lineno, line.strip(), exc_obj))
    else:
        # Red text if stdout to tty
        print(REDCC + 'EXCEPTION IN ({}, LINE {} "{}"): {}'
              .format(filename, lineno, line.strip(), exc_obj)
              + ENDCC, flush=True)

# Unused routines:
'''
# Print Kvaser channels
def print_channels():
    for ch in range(canlib.getNumberOfChannels()):
        chdata = canlib.ChannelData(ch)
        print("{ch}. {name} ({ean} / {serial})".format(
            ch=ch,
            name=chdata.channel_name,
            ean=chdata.card_upc_no,
            serial=chdata.card_serial_no,
        ))

# Generator for spinning cursor while waiting for user input
spinner = spinning_cursor()

def spinning_cursor():
    while True:
        for cursor in '|/-\\':
            yield cursor
'''
