/*
 * adbms_service.c
 *
 *  Created on: Apr 19, 2026
 *      Author: rgeng
 */

#include "can_if.h"
#include "adbms_service.h"
#include "adbms6833_proto.h"
#include "adbms6822_if.h"
#include "bmu_csc_cfg.h"


static Adbms6833_StackSnapshotType g_stackSnapshot;
static bool g_snapshot_valid = false;

Bmu_ReturnType Adbms_Service_Init(void)
{
    Adbms6822_Init();
    Adbms6833Proto_Init();
    g_snapshot_valid = false;
    return BMU_OK;
}

Bmu_ReturnType Adbms_Service_Wake(void)
{
    return Adbms6822_WakeChain( true );
}

static Bmu_ReturnType Adbms_Service_RefreshStackIfNeeded(void)
{
    Bmu_ReturnType ret;

    if (!Adbms6822_IsAwake())
    {
        ret = Adbms6822_WakeChain( true );
        if (ret != BMU_OK)
        {
            return ret;
        }
    }

    if (!g_snapshot_valid)
    {
        ret = Adbms6833_ReadSnapshot(&g_stackSnapshot);
        if (ret != BMU_OK)
        {
            return ret;
        }
        g_snapshot_valid = true;
    }

    return BMU_OK;
}

Bmu_ReturnType Adbms_Service_ReadCscSnapshot(uint8_t csc_index, Adbms_ServiceCscSnapshotType *snapshot)
{
    uint8_t cell;

    if ((snapshot == 0) || (csc_index >= BMU_CSC_COUNT))
    {
        return BMU_E_PARAM;
    }

    if (Adbms_Service_RefreshStackIfNeeded() != BMU_OK)
    {
        snapshot->valid = false;
        return BMU_E_NOT_OK;
    }

    snapshot->csc_index = csc_index;
    snapshot->valid = true;

    for (cell = 0u; cell < BMU_CELLS_PER_CSC; cell++)
    {
        const Bmu_CscCellMapType *map = &g_bmuCscMap[csc_index].cell_map[cell];
        uint8_t afe_global;

        if (!map->used)
        {
            snapshot->cells[cell].valid = false;
            continue;
        }

        afe_global = Bmu_Csc_GlobalAfeIndex(csc_index, map->afe_on_csc);

        if ((afe_global >= g_stackSnapshot.afe_count) ||
            (!g_stackSnapshot.afe[afe_global].valid))
        {
            snapshot->cells[cell].valid = false;
            continue;
        }

        snapshot->cells[cell].cell_voltage_raw_0p1mV =
            g_stackSnapshot.afe[afe_global].cell_raw_0p1mV[map->afe_cell_channel];

        snapshot->cells[cell].gpio_voltage_raw_0p1mV =
            g_stackSnapshot.afe[afe_global].gpio_raw_0p1mV[map->afe_gpio_channel];

        snapshot->cells[cell].balancing =
            g_stackSnapshot.afe[afe_global].balancing[map->balance_channel];

        snapshot->cells[cell].valid = true;
    }

    /* Optional policy: invalidate cached full stack after one CSC or after full round */
    g_snapshot_valid = false;

    return BMU_OK;
}
