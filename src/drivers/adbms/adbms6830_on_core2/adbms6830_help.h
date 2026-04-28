#ifndef SRC_DRIVERS_ADBMS_ADBMS6830_ON_CORE2_ADBMS6830_HELP_H_
#define SRC_DRIVERS_ADBMS_ADBMS6830_ON_CORE2_ADBMS6830_HELP_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Minimal ADBMS6830 helper definitions used by the modern Core2 driver.
 *
 * The old app/adbms6830 driver carried many more raw-data and state-machine
 * types, but the current src/drivers/adbms/adbms6830_on_core2 implementation
 * only needs:
 * - threshold / conversion constants
 * - CFGA register packing
 * - CFGB register packing
 */

#define ADBMS6830_CORE2_CELL_OV_THRESHOLD         (1400)
#define ADBMS6830_CORE2_CELL_UV_THRESHOLD         (600)
#define ADBMS6830_CORE2_CELL_CONVERSION_FACTOR    (1500)
#define ADBMS6830_CORE2_NO_OF_CELLS               (16)
#define ADBMS6830_CORE2_CELL_VOLTAGE_CONVERSION   (6666)

typedef struct
{
    uint32_t CTH      : 3;
    uint32_t reserved : 4;
    uint32_t REFON    : 1;
} ADBMS6830_CORE2_CFGA0_s;

typedef union
{
    ADBMS6830_CORE2_CFGA0_s B;
    uint8_t U8;
} ADBMS6830_CORE2_CFGA0_t;

typedef struct
{
    uint32_t FLAG_D0 : 1;
    uint32_t FLAG_D1 : 1;
    uint32_t FLAG_D2 : 1;
    uint32_t FLAG_D3 : 1;
    uint32_t FLAG_D4 : 1;
    uint32_t FLAG_D5 : 1;
    uint32_t FLAG_D6 : 1;
    uint32_t FLAG_D7 : 1;
} ADBMS6830_CORE2_CFGA1_s;

typedef union
{
    ADBMS6830_CORE2_CFGA1_s B;
    uint8_t U8;
} ADBMS6830_CORE2_CFGA1_t;

typedef struct
{
    uint8_t reserved : 3;
    uint8_t OWA      : 3;
    uint8_t OWRNG    : 1;
    uint8_t SOAKON   : 1;
} ADBMS6830_CORE2_CFGA2_s;

typedef union
{
    ADBMS6830_CORE2_CFGA2_s B;
    uint8_t U8;
} ADBMS6830_CORE2_CFGA2_t;

typedef struct
{
    uint8_t GPO1 : 1;
    uint8_t GPO2 : 1;
    uint8_t GPO3 : 1;
    uint8_t GPO4 : 1;
    uint8_t GPO5 : 1;
    uint8_t GPO6 : 1;
    uint8_t GPO7 : 1;
    uint8_t GPO8 : 1;
} ADBMS6830_CORE2_CFGA3_s;

typedef union
{
    ADBMS6830_CORE2_CFGA3_s B;
    uint8_t U8;
} ADBMS6830_CORE2_CFGA3_t;

typedef struct
{
    uint8_t GPO9     : 1;
    uint8_t GPO10    : 1;
    uint8_t reserved : 6;
} ADBMS6830_CORE2_CFGA4_s;

typedef union
{
    ADBMS6830_CORE2_CFGA4_s B;
    uint8_t U8;
} ADBMS6830_CORE2_CFGA4_t;

typedef struct
{
    uint8_t FC       : 3;
    uint8_t COMMBK   : 1;
    uint8_t MUTE_ST  : 1;
    uint8_t SNAP_ST  : 1;
    uint8_t reserved : 2;
} ADBMS6830_CORE2_CFGA5_s;

typedef union
{
    ADBMS6830_CORE2_CFGA5_s B;
    uint8_t U8;
} ADBMS6830_CORE2_CFGA5_t;

typedef struct
{
    ADBMS6830_CORE2_CFGA0_t CFGA0;
    ADBMS6830_CORE2_CFGA1_t CFGA1;
    ADBMS6830_CORE2_CFGA2_t CFGA2;
    ADBMS6830_CORE2_CFGA3_t CFGA3;
    ADBMS6830_CORE2_CFGA4_t CFGA4;
    ADBMS6830_CORE2_CFGA5_t CFGA5;
} ADBMS6830_CORE2_CFGA_t;

typedef struct
{
    uint8_t VUV : 8;
} ADBMS6830_CORE2_CFGB0_s;

typedef union
{
    ADBMS6830_CORE2_CFGB0_s B;
    uint8_t U8;
} ADBMS6830_CORE2_CFGB0_t;

typedef struct
{
    uint8_t VUV : 4;
    uint8_t VOV : 4;
} ADBMS6830_CORE2_CFGB1_s;

typedef union
{
    ADBMS6830_CORE2_CFGB1_s B;
    uint8_t U8;
} ADBMS6830_CORE2_CFGB1_t;

typedef struct
{
    uint8_t VOV : 8;
} ADBMS6830_CORE2_CFGB2_s;

typedef union
{
    ADBMS6830_CORE2_CFGB2_s B;
    uint8_t U8;
} ADBMS6830_CORE2_CFGB2_t;

typedef struct
{
    uint8_t DCTO  : 6;
    uint8_t DTRNG : 1;
    uint8_t DTMEN : 1;
} ADBMS6830_CORE2_CFGB3_s;

typedef union
{
    ADBMS6830_CORE2_CFGB3_s B;
    uint8_t U8;
} ADBMS6830_CORE2_CFGB3_t;

typedef struct
{
    uint8_t DCC1 : 1;
    uint8_t DCC2 : 1;
    uint8_t DCC3 : 1;
    uint8_t DCC4 : 1;
    uint8_t DCC5 : 1;
    uint8_t DCC6 : 1;
    uint8_t DCC7 : 1;
    uint8_t DCC8 : 1;
} ADBMS6830_CORE2_CFGB4_s;

typedef union
{
    ADBMS6830_CORE2_CFGB4_s B;
    uint8_t U8;
} ADBMS6830_CORE2_CFGB4_t;

typedef struct
{
    uint8_t DCC9  : 1;
    uint8_t DCC10 : 1;
    uint8_t DCC11 : 1;
    uint8_t DCC12 : 1;
    uint8_t DCC13 : 1;
    uint8_t DCC14 : 1;
    uint8_t DCC15 : 1;
    uint8_t DCC16 : 1;
} ADBMS6830_CORE2_CFGB5_s;

typedef union
{
    ADBMS6830_CORE2_CFGB5_s B;
    uint8_t U8;
} ADBMS6830_CORE2_CFGB5_t;

typedef struct
{
    ADBMS6830_CORE2_CFGB0_t CFGB0;
    ADBMS6830_CORE2_CFGB1_t CFGB1;
    ADBMS6830_CORE2_CFGB2_t CFGB2;
    ADBMS6830_CORE2_CFGB3_t CFGB3;
    ADBMS6830_CORE2_CFGB4_t CFGB4;
    ADBMS6830_CORE2_CFGB5_t CFGB5;
} ADBMS6830_CORE2_CFGB_t;

#ifdef __cplusplus
}
#endif

#endif /* SRC_DRIVERS_ADBMS_ADBMS6830_ON_CORE2_ADBMS6830_HELP_H_ */
