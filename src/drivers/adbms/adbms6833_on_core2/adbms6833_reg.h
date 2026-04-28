/**
 * \file adbms6833_reg.h
 * \brief ADBMS6833 register header file
 * \author R. Larocque
 *
 * \brief This is a list of all commands used to access the ADBMS6833
 *
 * Note: The address constants are 9 bit values where
 * the lower 8 bit define the address and the 9th bit
 * defines the page!
 */

#ifndef BMS_DRV_INC_ADBMS6833_REG_H_
#define BMS_DRV_INC_ADBMS6833_REG_H_

/**
 * \name Configuration Registers Commands
 *
 * \{*/
#define ADBMS6833_WRCFGA_REG				0x0001	/**< \brief  Write Configuration Register Group A*/
#define ADBMS6833_WRCFGB_REG				0x0024	/**< \brief  Write Configuration Register Group B*/
#define ADBMS6833_RDCFGA_REG				0x0002	/**< \brief  Read Configuration Register Group A*/
#define ADBMS6833_RDCFGB_REG				0x0026	/**< \brief  Read Configuration Register Group B*/
/**\}*/

/**
 * \name Read Cell Voltage Commands
 *
 * \{*/
#define ADBMS6833_RDCVA_REG					0x0004	/**< \brief Read Cell Voltage Register Group A*/
#define ADBMS6833_RDCVB_REG					0x0006	/**< \brief Read Cell Voltage Register Group B*/
#define ADBMS6833_RDCVC_REG					0x0008	/**< \brief Read Cell Voltage Register Group C*/
#define ADBMS6833_RDCVD_REG					0x000A	/**< \brief Read Cell Voltage Register Group D*/
#define ADBMS6833_RDCVE_REG					0x0009	/**< \brief Read Cell Voltage Register Group E*/
#define ADBMS6833_RDCVF_REG					0x000B	/**< \brief Read Cell Voltage Register Group F*/
#define ADBMS6833_RDCVALL_REG				0x000C	/**< \brief Read All Cell Results*/
/**\}*/

/**
 * \name Read Averaged Cell Voltage Commands
 *
 * \{*/
#define ADBMS6833_RDACA_REG					0x0044	/**< \brief Read Averaged Cell Voltage Register Group A*/
#define ADBMS6833_RDACB_REG					0x0046	/**< \brief Read Averaged Cell Voltage Register Group B*/
#define ADBMS6833_RDACC_REG					0x0048	/**< \brief Read Averaged Cell Voltage Register Group C*/
#define ADBMS6833_RDACD_REG					0x004A	/**< \brief Read Averaged Cell Voltage Register Group D*/
#define ADBMS6833_RDACE_REG					0x0049	/**< \brief Read Averaged Cell Voltage Register Group E*/
#define ADBMS6833_RDACF_REG					0x004B	/**< \brief Read Averaged Cell Voltage Register Group F*/
#define ADBMS6833_RDACALL_REG				0x004C	/**< \brief Read All Averaged Cell Results*/
/**\}*/

/**
 * \name Read S Voltage Commands
 *
 * \{*/
#define ADBMS6833_RDSVA_REG					0x0003	/**< \brief Read S Voltage Register Group A*/
#define ADBMS6833_RDSVB_REG					0x0005	/**< \brief Read S Voltage Register Group B*/
#define ADBMS6833_RDSVC_REG					0x0007	/**< \brief Read S Voltage Register Group C*/
#define ADBMS6833_RDSVD_REG					0x000D	/**< \brief Read S Voltage Register Group D*/
#define ADBMS6833_RDSVE_REG					0x000E	/**< \brief Read S Voltage Register Group E*/
#define ADBMS6833_RDSVF_REG					0x000F	/**< \brief Read S Voltage Register Group F*/
#define ADBMS6833_RDSALL_REG				0x0010	/**< \brief Read All S Results*/
#define ADBMS6833_RDCSALL_REG				0x0011	/**< \brief Read All C and S Results*/
#define ADBMS6833_RDACSALL_REG				0x0051	/**< \brief Read All Averaged C and S Results*/
/**\}*/

/**
 * \name Read Filtered Cell Voltage Commands
 *
 * \{*/
#define ADBMS6833_RDFCA_REG					0x0012	/**< \brief Read Filtered Cell Voltage Register Group A*/
#define ADBMS6833_RDFCB_REG					0x0013	/**< \brief Read Filtered Cell Voltage Register Group B*/
#define ADBMS6833_RDFCC_REG					0x0014	/**< \brief Read Filtered Cell Voltage Register Group C*/
#define ADBMS6833_RDFCD_REG					0x0015	/**< \brief Read Filtered Cell Voltage Register Group D*/
#define ADBMS6833_RDFCE_REG					0x0016	/**< \brief Read Filtered Cell Voltage Register Group E*/
#define ADBMS6833_RDFCF_REG					0x0017	/**< \brief Read Filtered Cell Voltage Register Group F*/
#define ADBMS6833_RDFCALL_REG				0x0018	/**< \brief Read All Filtered Cell Results*/
/**\}*/

/**
 * \name Read Auxiliary Commands
 *
 * \{*/
#define ADBMS6833_RDAUXA_REG				0x0019	/**< \brief Read Auxiliary Register Group A*/
#define ADBMS6833_RDAUXB_REG				0x001A	/**< \brief Read Auxiliary Register Group B*/
#define ADBMS6833_RDAUXC_REG				0x001B	/**< \brief Read Auxiliary Register Group C*/
#define ADBMS6833_RDAUXD_REG				0x001F	/**< \brief Read Auxiliary Register Group D*/
#define ADBMS6833_RDAUXE_REG				0x0036	/**< \brief Read Auxiliary Register Group E*/
/**\}*/

/**
 * \name Read Redundant Auxiliary Commands
 *
 * \{*/
#define ADBMS6833_RDRAXA_REG				0x001C	/**< \brief Read Redundant Auxiliary Register Group A*/
#define ADBMS6833_RDRAXB_REG				0x001D	/**< \brief Read Redundant Auxiliary Register Group B*/
#define ADBMS6833_RDRAXC_REG				0x001E	/**< \brief Read Redundant Auxiliary Register Group C*/
#define ADBMS6833_RDRAXD_REG				0x0026	/**< \brief Read Redundant Auxiliary Register Group D*/
/**\}*/

/**
 * \name Read Status Commands
 *
 * \{*/
#define ADBMS6833_RDSTATA_REG				0x0030	/**< \brief Read Status Register Group A*/
#define ADBMS6833_RDSTATB_REG				0x0031	/**< \brief Read Status Register Group B*/
#define ADBMS6833_RDSTATC_REG				0x0032	/**< \brief Read Status Register Group C*/
#define ADBMS6833_RDSTATD_REG				0x0033	/**< \brief Read Status Register Group D*/
#define ADBMS6833_RDSTATE_REG				0x0034	/**< \brief Read Status Register Group E*/
#define ADBMS6833_RDASALL_REG				0x0035	/**< \brief Read all Aux/Status Registers*/
/**\}*/

/**
 * \name PWM Commands
 *
 * \{*/
#define ADBMS6833_WRPWMA_REG				0x0020	/**< \brief Write PWM Register Group A*/
#define ADBMS6833_RDPWMA_REG				0x0022	/**< \brief Read PWM Register Group A*/
#define ADBMS6833_WRPWMB_REG				0x0021	/**< \brief Write PWM Register Group B*/
#define ADBMS6833_RDPWMB_REG				0x0023	/**< \brief Read PWM Register Group B*/
/**\}*/

/**
 * \name Start ADC Conversion Commands
 *
 * \{*/
#define ADBMS6833_ADCV_BASE_REG				0x0260u	/**< \brief Starts Cell Voltage ADC Conversion and Poll Status base address*/
#define ADBMS6833_ADCV_CONT_OPTION			0x0080u	/**< \brief Continuous measurement option*/
#define ADBMS6833_ADSV_BASE_REG				0x0168	/**< \brief Start S-ADC Conversion and Poll Status base address*/
#define ADBMS6833_ADSV_OW_EVEN				0x0001	/**< \brief Option to start open wire detection for even channels*/
#define ADBMS6833_ADSV_OW_ODD				0x0002	/**< \brief Option to start open wire detection for odd channels*/
#define ADBMS6833_ADAX_BASE_REG				0x0410	/**< \brief Start AUX ADC Conversion and Poll Status base address*/
#define ADBMS6833_ADAX2_BASE_REG			0x0400	/**< \brief Start AUX2 ADC Conversion and Poll Status base address*/
/**\}*/

/**
 * \name Clear Commands
 *
 * \{*/
#define ADBMS6833_CLRCELL_REG				0x0711	/**< \brief Clear Cell Voltage Register Groups*/
#define ADBMS6833_CLRFC_REG					0x0714	/**< \brief Clear Filtered Cell Voltage Register Groups*/
#define ADBMS6833_CLRAUX_REG				0x0712	/**< \brief Clear Auxiliary Register Groups*/
#define ADBMS6833_CLRSPIN_REG				0x0716	/**< \brief Clear S-Voltage Register Groups*/
#define ADBMS6833_CLRFLAG_REG				0x0717	/**< \brief Clear Flags*/
#define ADBMS6833_CLOVUV_REG				0x0715	/**< \brief Clear OV/UV*/
/**\}*/

/**
 * \name Polling Commands
 *
 * \{*/
#define ADBMS6833_PLADC_REG					0x0718	/**< \brief Poll any ADC Status*/
#define ADBMS6833_PLCADC_REG				0x071C	/**< \brief Poll C-ADC*/
#define ADBMS6833_PLSADC_REG				0x071D	/**< \brief Poll S-ADC*/
#define ADBMS6833_PLAUX_REG					0x071E	/**< \brief Poll AUX ADC*/
#define ADBMS6833_PLAUX2_REG				0x071F	/**< \brief POLL AUX2 ADC*/
/**\}*/

/**
 * \name Communication Commands
 *
 * \{*/
#define ADBMS6833_WRCOMM_REG				0x0721	/**< \brief Write COMM register group*/
#define ADBMS6833_RDCOMM_REG				0x0722	/**< \brief Read COMM register group*/
#define ADBMS6833_STCOMM_REG				0x0723	/**< \brief Start I2C/SPI Communication*/
/**\}*/

/**
 * \name Miscellaneous Commands
 *
 * \{*/
#define ADBMS6833_MUTE_REG					0x0028	/**< \brief Mute Discharge*/
#define ADBMS6833_UNMUTE_REG				0x0029	/**< \brief Unmute Discharge*/
#define ADBMS6833_RDSID_REG					0x002C	/**< \brief Read Serial ID Register Group*/
#define ADBMS6833_RSTCC_REG					0x002E	/**< \brief Reset counter command*/
#define ADBMS6833_SNAP_REG					0x002D	/**< \brief Snapshot */
#define ADBMS6833_UNSNAP_REG				0x002F	/**< \brief Release Snapshot*/
#define ADBMS6833_SRST_REG					0x0027	/**< \brief Soft Reset*/
#define ADBMS6833_ULRR_REG					0x0038	/**< \brief Unlock Retention Register*/
#define ADBMS6833_WRRR_REG					0x0039  /**< \brief Write Retention Registers*/
#define ADBMS6833_RDRR_REG					0x003A  /**< \brief Read Retnetion Register*/
/**\}*/

#endif /* BMS_DRV_INC_ADBMS6833_REG_H_ */
