#ifndef LTC6812_REG_H
#define LTC6812_REG_H

/* LTC6812-1 command codes, per LTC6812 datasheet command table. */
#define LTC6812_WRCFGA_REG        0x0001u
#define LTC6812_WRCFGB_REG        0x0024u
#define LTC6812_RDCFGA_REG        0x0002u
#define LTC6812_RDCFGB_REG        0x0026u

#define LTC6812_RDCVA_REG         0x0004u
#define LTC6812_RDCVB_REG         0x0006u
#define LTC6812_RDCVC_REG         0x0008u
#define LTC6812_RDCVD_REG         0x000Au
#define LTC6812_RDCVE_REG         0x0009u

#define LTC6812_RDAUXA_REG        0x000Cu
#define LTC6812_RDAUXB_REG        0x000Eu
#define LTC6812_RDAUXC_REG        0x000Du
#define LTC6812_RDAUXD_REG        0x000Fu

#define LTC6812_ADCV_BASE_REG     0x0260u
#define LTC6812_ADAX_BASE_REG     0x0460u

#define LTC6812_CLRCELL_REG       0x0711u
#define LTC6812_CLRAUX_REG        0x0712u
#define LTC6812_MUTE_REG          0x0028u
#define LTC6812_UNMUTE_REG        0x0029u

#endif /* LTC6812_REG_H */
