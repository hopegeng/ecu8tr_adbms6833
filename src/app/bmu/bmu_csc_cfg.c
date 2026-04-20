/*
 * bmu_csc_cfg.c
 *
 *  Created on: Apr 19, 2026
 *      Author: rgeng
 */


#include "bmu_csc_cfg.h"

uint8_t Bmu_Csc_GlobalAfeIndex(uint8_t csc_index, uint8_t afe_on_csc)
{
    return (uint8_t)(csc_index * BMU_AFES_PER_CSC + afe_on_csc);
}

/* Default example:
 * Cell 0..9   -> AFE0 channels 0..9
 * Cell 10..19 -> AFE1 channels 0..9
 *
 * Replace this with your exact CSC schematic mapping.
 */
const Bmu_CscMapType g_bmuCscMap[BMU_CSC_COUNT] =
{
    {
        .cell_map =
        {
            { true, 0u, 0u, 0u, 0u },
            { true, 0u, 1u, 1u, 1u },
            { true, 0u, 2u, 2u, 2u },
            { true, 0u, 3u, 3u, 3u },
            { true, 0u, 4u, 4u, 4u },
            { true, 0u, 5u, 5u, 5u },
            { true, 0u, 6u, 6u, 6u },
            { true, 0u, 7u, 7u, 7u },
            { true, 0u, 8u, 8u, 8u },
            { true, 0u, 9u, 9u, 9u },

            { true, 1u, 0u, 0u, 0u },
            { true, 1u, 1u, 1u, 1u },
            { true, 1u, 2u, 2u, 2u },
            { true, 1u, 3u, 3u, 3u },
            { true, 1u, 4u, 4u, 4u },
            { true, 1u, 5u, 5u, 5u },
            { true, 1u, 6u, 6u, 6u },
            { true, 1u, 7u, 7u, 7u },
            { true, 1u, 8u, 8u, 8u },
            { true, 1u, 9u, 9u, 9u }
        }
    }
};
