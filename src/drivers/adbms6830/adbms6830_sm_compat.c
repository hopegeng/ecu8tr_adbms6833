/*
 * adbms6830_sm_compat.c
 *
 * Compatibility stubs for legacy safety-manual self-tests.
 */

#include "adbms6830SM.h"
#include "adbms6830.h"

bool adbms6830SM_CellMeasurements(void)
{
    return false;
}

bool adbms6830SM_Freeze_Seq(void)
{
    return false;
}

bool adbms6830SM_TrigAuxMeasurements(void)
{
    return (adbms6830_TrigAuxMeasurement() == false);
}

bool adbms6830SM_TrigADSV(void)
{
    if (adbms6830_write(ADBMS6830_ADSV_BASE_REG | ADBMS6830_ADCV_CONT_OPTION, NULL, 0u) == false)
    {
        adbms6830Faults.B.SM_TRIGGER_ADSV = 1u;
        return true;
    }

    adbms6830Faults.B.SM_TRIGGER_ADSV = 0u;
    return false;
}

bool adbms6830SM_Cell_OWD_Even(void)
{
    return false;
}

bool adbms6830SM_Cell_OWD_Odd(void)
{
    return false;
}

bool adbms6830SM_Read_Cell_OWD(void)
{
    return false;
}
