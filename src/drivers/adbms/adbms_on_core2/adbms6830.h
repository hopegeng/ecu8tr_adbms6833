/**
 * \file adbms6830.h
 * \brief ADBMS6830 register header file
 * \author R. Larocque
 *

 *
 */

#ifndef ADBMS6830_H
#define ADBMS6830_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "qspi.h"

/******************************************************************************/
/*-----------------------------Contants--------------------------------*/
/******************************************************************************/
/**
 *  @name Constants used for Fuel Cell
 *
 * \{*/
#define ADBMS6830_CELL_OV_THRESHOLD			1400			/**< \brief Overvoltage threshold in mV*/
#define ADBMS6830_CELL_UV_THRESHOLD			600			   	/**< \brief Undervoltage threshold in mV*/
#define ADBMS6830_CELL_CONVERSION_FACTOR	1500			/**< \brief Conversion factor used to for calculation of voltages*/
#define ADBMS6830_NO_OF_CELLS				16				/**< \brief Number of cells in each node*/
#define ADBMS6830_CELL_LOW_RANGE			500				/**< \brief Cell low range in mV.  If voltage is below this VCELL_OOR fault is issued*/
#define ADBMS6830_CELL_HIGH_RANGE			2000				/**< \brief Cell high range in mV.  If voltage is above this VCELL_OOR fault is issued*/
/**\}*/

/******************************************************************************/
/*-----------------------------Macros--------------------------------*/
/******************************************************************************/
/**
 *  @name Macros used for Fuel Cell
 *
 * \{*/
#define ADBMS6830_NUMBER_OF_NODES 			1u				/**< \brief number of nodes in the ring*/
#define ADBMS6830_CELL_VOLTAGE_CONVERSION	6666			/**< \brief equates to 1/150e-6 x 1000 */
#define ADBMS6830_CELL_V_LSB				150				/**< \brief in micro volts*/
#define ADBMS6830_VOLT_SCALING				1000u			/**< \brief conversion factor to give results in millivolts
																 Vaux = ADC x 150e-6 +1.500 = V
																 Vaux = (ADC x 150 / 1000) + 1500 = mV*/
#define ADBMS6830_TEMP_KELVIN				2730u			/**< \brief Absolute zero x 10*/
#define ADBMS6830_MVPERDEGREEC				75u				/**< \brief represents 7.5mv/C  */
#define ADBMS6830_TEMP_SCALING				100u
/**\}*/


/**
 *  @name Macros used for Fault detection
 *
 * \{*/
#define ADBMS6830_NOFAULTS					0u				/**< \brief Used to check to see if a fault has occured */
#define ADBMS6830_FAULT_OCCURED				1u				/**< \brief Used to check to see if a fault has occured */
#define ADBMS6830_CxOV_FAULT				0xAA			/**< \brief bitmask to determine if Cell over voltage occurred */
#define ADBMS6830_CxUV_FAULT				0x55			/**< \brief bitmask to determine if Cell under voltage occurred */
#define ADBMS6830_STATC_BITMASK_FAULT		0x7FF			/**< \brief botmask to determine if faults were issued in STATC*/
/**\}*/


/******************************************************************************/
/*-----------------------------Data Structures--------------------------------*/
/******************************************************************************/

/**
 * \brief Structure to contain the various Status voltages
 */
typedef struct
{
	uint16_t Vref2;												/**< \brief Second reference voltage */
	uint16_t Itmp;												/**< \brief Internal die temperature*/
	uint16_t Vd;												/**< \brief Digital power supply voltage*/
	uint16_t Va;												/**< \brief Analog power supply voltage*/
	uint16_t Vres;												/**< \brief Vref2 across resistor*/
}STATUSV_t;

/**
 * \brief Structure to contain cell metrics
 */
typedef struct
{
	int16_t packV[ADBMS6830_NUMBER_OF_NODES];									/**< \brief Total voltage of pack based on CV measurements*/
	int16_t cellV[ADBMS6830_NUMBER_OF_NODES][ADBMS6830_NO_OF_CELLS];				/**< \brief Cell voltages for Cells 1 to 16 in each node*/
	int16_t avgCellV[ADBMS6830_NUMBER_OF_NODES][ADBMS6830_NO_OF_CELLS];			/**< \brief Averaged Cell voltages for Cells 1 to 16 in each node*/
	int16_t filtCellV[ADBMS6830_NUMBER_OF_NODES][ADBMS6830_NO_OF_CELLS];			/**< \brief Filtered Cell voltages for Cells 1 to 16 in each node*/
	int16_t sPinV[ADBMS6830_NUMBER_OF_NODES][ADBMS6830_NO_OF_CELLS];				/**< \brief S-pin Cell voltages for Cells 1 to 16 in each node*/
	STATUSV_t statusV[ADBMS6830_NUMBER_OF_NODES];									/**< \brief Status voltages for Nodex*/
}CELL_INFO_t;

/**
 *  @brief Struct for Configuration Group A Register 0
 */
typedef struct
{
	uint32_t		CTH			: 3;	/**< \brief Bits 0-2: C-ADC vs S-ADC comparison voltage threshold\n 000: 5.1mV\n 001: 8.1mV(default)\n 010: 9.1mV\n
	                                   011: 10.05mV\n 100: 15mV\n 101: 19.95mV\n 110: 25.05mV\n 111: 40.05mV*/
	uint32_t		reserved	: 4;	/**< \brief Bits 3-6: reserved*/
	uint32_t 		REFON		: 1;	/**< \brief Bit 7: Reference Powered up\n 1 = reference remains powered up until watchdog timeout\n
	 	 	 	 	 	 	 	 	 	 0 = reference shuts down after conversions (default)*/
}ADBMS6830_CFGA0_s;

/**
 *  @brief Union for Configuration Group A Register 0
 */
typedef union
{
	ADBMS6830_CFGA0_s B;
	uint8_t U8;
}ADBMS6830_CFGA0_t;

/**
 *  @brief Struct for Configuration Group A Register 1 \n
 *
 *  Assets various flags in Status Register C for latent fault detection.\n
 *  Asserting flags in status register for latent fault diagnostic does not cause the ADBMS6830 to behave as if the flag was set by the actual
 *  diagnostic mechanism.
 *
 */
typedef struct
{
	uint32_t	FLAG_D0		: 1;	/**< \brief Bit0 : 1 = forces oscillator counter fast*/
	uint32_t	FLAG_D1		: 1;	/**< \brief Bit1 : 1 = forces oscillator counter slow*/
	uint32_t	FLAG_D2		: 1;	/**< \brief Bit2 : 1 = forces supply error detection (ED)*/
	uint32_t	FLAG_D3		: 1;	/**< \brief Bit3 : 1 = selects supply OV and delta detection.  0 = selects UV*/
	uint32_t	FLAG_D4		: 1;	/**< \brief Bit4 : 1 = selects THSD*/
	uint32_t	FLAG_D5		: 1;	/**< \brief Bit5 : 1 = forces nonvolatile memory (NVM) error detection (ED).  Sets CED and SED*/
	uint32_t	FLAG_D6		: 1;	/**< \brief Bit6 : 1 = forces NVM multiple error detection(MED).  Sets CMED and SMED.*/
	uint32_t	FLAG_D7		: 1;	/**< \brief Bit7 : 1 = forces TMODCHK*/

}ADBMS6830_CFGA1_s;

/**
 *  @brief Union for Configuration Group A Register 1
 */
typedef union
{
	ADBMS6830_CFGA1_s B;			/**< \brief	bitfield access*/
	uint8_t U8;						/**< \brief	unsigned access*/
}ADBMS6830_CFGA1_t;

/**
 *  @brief Struct for Configuration Group A Register 2
 *
 */
typedef struct
{
	uint8_t		reserved			: 3;	/**< \brief Bits 0-2: reserved*/
	uint8_t		OWA					: 3;	/**< \brief Bits 3-5: Open wire Soak times (For AUX commands) \n
	 	 	 	 	 	 	 	 	 	 	 	 	 	 If OWRNG = 0, soak time = 2^(6+OWA[2:0]) clocks (32us to 4.1ms). \n
	 	 	 	 	 	 	 	 	 	 	 	 	 	 If OWRNG = 1, soak time = 2^(13+OWA[2:0]) clocks (4.1ms to 524ms) */
	uint8_t		OWRNG				: 1;	/**< \brief Bit 6: Soak time range \n
	 	 	 	 	 	 	 	 	 	 	 	 	 	 0: short soak time range\n
	 	 	 	 	 	 	 	 	 	 	 	 	 	 1: long soak time range*/
	uint8_t		SOAKON				: 1;	/**< \brief Bit 7: Enables soak on AUX ADCs  \n
	 	 	 	 	 	 	 	 	 	 	 	 	 	 0: disables soak time \n
	 	 	 	 	 	 	 	 	 	 	 	 	 	 1: enables soak time for all commands*/
}ADBMS6830_CFGA2_s;

/**
 *  @brief Union for Configuration Group A Register 2
 */
typedef union
{
	ADBMS6830_CFGA2_s B;
	uint8_t U8;
}ADBMS6830_CFGA2_t;

/**
 *  @brief Struct for Configuration Group A Register 3 \n
 *  Output State Control \n
 *
 *  For all GPOs, \n
 *  0: GPOx - GPOx pin pull-down on. \n
 *  1: GPOx - GPOx pin pull-down off(default)
 *
 */
typedef struct
{

		uint8_t		GPO1		: 1;	/**< \brief Bit 0: GPO1 Output state control*/
		uint8_t		GPO2		: 1;	/**< \brief Bit 1: GPO2 Output state control*/
		uint8_t		GPO3		: 1;	/**< \brief Bit 2: GPO3 Output state control*/
		uint8_t 	GPO4		: 1;	/**< \brief Bit 3: GPO4 Output state control*/
		uint8_t 	GPO5		: 1;	/**< \brief Bit 4: GPO5 Output state control*/
		uint8_t 	GPO6		: 1;	/**< \brief Bit 5: GPO6 Output state control*/
		uint8_t 	GPO7		: 1;	/**< \brief Bit 6: GPO7 Output state control*/
		uint8_t 	GPO8		: 1;	/**< \brief Bit 7: GPO8 Output state control*/

}ADBMS6830_CFGA3_s;

/**
 *  @brief Union for Configuration Group A Register 3
 */
typedef union
{
	ADBMS6830_CFGA3_s B;
	uint8_t U8;
}ADBMS6830_CFGA3_t;

/**
 *  @brief Struct for Configuration Group A Register 4 \n
 *  Output State Control \n
 *
 *  For all GPOs, \n
 *  0: GPOx - GPOx pin pull-down on. \n
 *  1: GPOx - GPOx pin pull-down off(default)
 *
 */
typedef struct
{

		uint8_t		GPO9		: 1;	/**< \brief Bit 0: GPO9 Output state control*/
		uint8_t		GPO10		: 1;	/**< \brief Bit 1: GPO10 Output state control*/
		uint8_t		reserved	: 6;	/**< \brief Bits 2-7: reserved*/


}ADBMS6830_CFGA4_s;

/**
 *  @brief Union for Configuration Group A Register 4
 */
typedef union
{
	ADBMS6830_CFGA4_s B;
	uint8_t U8;
}ADBMS6830_CFGA4_t;

/**
 *  @brief Struct for Configuration Group A Register 5
 *
 */
typedef struct
{
	uint8_t		FC		: 3;	/**< \brief Bits 0-2:  IIR filter parameter.
												 |-3db Corner Freq|FC[2]|FC[1]|FC[0}|Filter Parameter|
												 |:--------------:|:---:|:---:|:---:|:--------------:|
												 | Filter Disabled|0|0|0|N/A|
												 |110|0|0|1|2|
												 |45|0|1|0|4|
												 |21|0|1|1|8|
												 |10|1|0|0|16|
												 |5|1|0|1|32|
												 |1.25|1|1|0|128|
												 |0.625|1|1|1|256|*/
	uint8_t		COMMBK		: 1;	/**< \brief Bit 3: Communication Break \n
												 1 = asserts the communication break feature that prevents the device from propagation communication further through the daisy chain*/
	uint8_t		MUTE_ST		: 1;	/**< \brief Bit 4: Mute Status\n
												 0: mute is deactivated\n
												 1: mute is activated and discharging is disabled*/
	uint8_t 	SNAP_ST		: 1;	/**< \brief Bit 5: Snapshot status\n
												 0: snapshot is deactivated\n
												 1: snapshot is activated. result registers are frozen.*/
	uint8_t 	reserved	: 2;	/**< \brief Bit 6&7: reserved*/
}ADBMS6830_CFGA5_s;

/**
 *  @brief Union for Configuration Group A Register 5
 */
typedef union
{
	ADBMS6830_CFGA5_s B;
	uint8_t U8;
}ADBMS6830_CFGA5_t;

/**
 *  @brief Struct for Configuration Group A Register
 */
typedef struct
{
	ADBMS6830_CFGA0_t	CFGA0 			;		/**< \brief Configuration Group A Register 0 */
	ADBMS6830_CFGA1_t	CFGA1 			;		/**< \brief Configuration Group A Register 1 */
	ADBMS6830_CFGA2_t	CFGA2 			;		/**< \brief Configuration Group A Register 2 */
	ADBMS6830_CFGA3_t	CFGA3 			;		/**< \brief Configuration Group A Register 3 */
	ADBMS6830_CFGA4_t	CFGA4 			;		/**< \brief Configuration Group A Register 4 */
	ADBMS6830_CFGA5_t	CFGA5 			;		/**< \brief Configuration Group A Register 5 */
} ADBMS6830_CFGA_t;

/**
 *  @brief Struct for Configuration Group B Register 0
 */
typedef struct
{
	uint8_t		VUV		: 8;	/**< \brief Bits 0 to 7: UV comparison voltage\n
	 	 	 	 	 	 	 	 	 VUV[7:0] Cell undervoltage threshold = VUV[11:0] x 16 x 150uV + 1.5V*/
}ADBMS6830_CFGB0_s;
/**
 *  @brief Union for Configuration Group B Register 0
 */
typedef union
{
	ADBMS6830_CFGB0_s B;
	uint8_t U8;
}ADBMS6830_CFGB0_t;

/**
 *  @brief Struct for Configuration Group B Register 1
 */
typedef struct
{
	uint8_t		VUV			: 4;	/**< \brief Bits 0 to 3: UV comparison voltage\n
	 	 	 	 	 	 	 	 	 VUV[11:8] Cell undervoltage threshold = VUV[11:0] x 16 x 150uV + 1.5V*/
	uint8_t		VOV			: 4;	/**< \brief Bits 4 to 7: OV comparison voltage\n
	 	 	 	 	 	 	 	 	 VOV[11:8] Cell overvoltage threshold = VOV[11:0] x 16 x 150uV + 1.5V*/
}ADBMS6830_CFGB1_s;

/**
 *  @brief Union for Configuration Group B Register 1
 */
typedef union
{
	ADBMS6830_CFGB1_s B;
	uint8_t U8;
}ADBMS6830_CFGB1_t;

/**
 *  @brief Struct for Configuration Group B Register 2
 */
typedef struct
{
	uint8_t		VOV			: 8;	/**< \brief Bits 4 to 7: OV comparison voltage\n
	 	 	 	 	 	 	 	 	 VOV[11:4] Cell overvoltage threshold = VOV[11:0] x 16 x 150uV + 1.5V*/
}ADBMS6830_CFGB2_s;

/**
*  @brief Union for Configuration Group B Register 2
*/
typedef union
{
	ADBMS6830_CFGB2_s B;
	uint8_t U8;
}ADBMS6830_CFGB2_t;

/**
 *  @brief Struct for Configuration Group B Register 3
 */
typedef struct
{
	uint8_t		DCTO		: 6;	/**< \brief Bits 0-5 : Discharge timeout value\n
	 	 	 	 	 	 	 	 	 	 	 	 	 Write = Set new value\n
	 	 	 	 	 	 	 	 	 	 	 	 	 Read = remaining value\n
	 	 	 	 	 	 	 	 	 	 	 	 	 1 = less than or equal to 1 increment remaining\n
	 	 	 	 	 	 	 	 	 	 	 	 	 0 = timeout has occurred or DCTO no set.*/
	uint8_t		DTRNG		: 1;	/**< \brief Bit6 : Discharge timer range\n
	 	 	 	 	 	 	 	 	 	 	 	 	 1 = 0 to 16.8 hours in 16 minute increments\n
	 	 	 	 	 	 	 	 	 	 	 	 	 0 = 0 to 63 minutes in 1 minute increments.*/
	uint8_t		DTMEN		: 1;	/**< \brief Bit7 : Enable discharge timer monitor\n
	 	 	 	 	 	 	 	 	 	 	 	 	 1 = Enables the discharge timer monitor function.\n
	 	 	 	 	 	 	 	 	 	 	 	 	 0 = Disables the discharge timer monitor function.*/
}ADBMS6830_CFGB3_s;

/**
 *  @brief Union for Configuration Group B Register 3
 */
typedef union
{
	ADBMS6830_CFGB3_s B;
	uint8_t U8;
}ADBMS6830_CFGB3_t;

/**
 *  @brief Struct for Configuration Group B Register 4
 *
 *  For all DCCx: \n
 *  	1: continuously turns on shorting switch for Cellx\n
 *  	0: continuously turns off shorting switch for Cellx
 */
typedef struct
{
	uint8_t		DCC1	: 1;	/**< \brief Bit0 : Discharge Cell 1*/
	uint8_t		DCC2	: 1;	/**< \brief Bit1 : Discharge Cell 2*/
	uint8_t		DCC3	: 1;	/**< \brief Bit2 : Discharge Cell 3*/
	uint8_t		DCC4	: 1;	/**< \brief Bit3 : Discharge Cell 4*/
	uint8_t		DCC5	: 1;	/**< \brief Bit4 : Discharge Cell 5*/
	uint8_t		DCC6	: 1;	/**< \brief Bit5 : Discharge Cell 6*/
	uint8_t		DCC7	: 1;	/**< \brief Bit6 : Discharge Cell 7*/
	uint8_t		DCC8	: 1;	/**< \brief Bit7 : Discharge Cell 8*/
}ADBMS6830_CFGB4_s;

/**
 *  @brief Union for Configuration Group B Register 4
 */
typedef union
{
	ADBMS6830_CFGB4_s B;
	uint8_t U8;
}ADBMS6830_CFGB4_t;

/**
 *  @brief Struct for Configuration Group B Register 5
 *
 *   For all DCCx: \n
 *  	1: continuously turns on shorting switch for Cellx\n
 *  	0: continuously turns off shorting switch for Cellx
 */
typedef struct
{
	uint8_t		DCC9	: 1;	/**< \brief Bit0 : Discharge Cell 9*/
	uint8_t		DCC10	: 1;	/**< \brief Bit1 : Discharge Cell 10*/
	uint8_t		DCC11	: 1;	/**< \brief Bit2 : Discharge Cell 11*/
	uint8_t		DCC12	: 1;	/**< \brief Bit3 : Discharge Cell 12*/
	uint8_t		DCC13	: 1;	/**< \brief Bit4 : Discharge Cell 13*/
	uint8_t		DCC14	: 1;	/**< \brief Bit5 : Discharge Cell 14*/
	uint8_t		DCC15	: 1;	/**< \brief Bit6 : Discharge Cell 15*/
	uint8_t		DCC16	: 1;	/**< \brief Bit7 : Discharge Cell 16*/
}ADBMS6830_CFGB5_s;

/**
 *  @brief Union for Configuration Group B Register 5
 */
typedef union
{
	ADBMS6830_CFGB5_s B;
	uint8_t U8;
}ADBMS6830_CFGB5_t;

/**
 *  @brief Struct for Configuration Group B Register
 */
typedef struct
{
	ADBMS6830_CFGB0_t	CFGB0 			;		/**< \brief Configuration Group A Register 0 */
	ADBMS6830_CFGB1_t	CFGB1 			;		/**< \brief Configuration Group A Register 1 */
	ADBMS6830_CFGB2_t	CFGB2 			;		/**< \brief Configuration Group A Register 2 */
	ADBMS6830_CFGB3_t	CFGB3 			;		/**< \brief Configuration Group A Register 3 */
	ADBMS6830_CFGB4_t	CFGB4 			;		/**< \brief Configuration Group A Register 4 */
	ADBMS6830_CFGB5_t	CFGB5 			;		/**< \brief Configuration Group A Register 5 */
} ADBMS6830_CFGB_t;

/**
 *  @brief Struct for Cell Voltage Register Group A
 *
 *  Cell Voltage 16 bit ADC measurement for Cell x.\n
 *  Voltage = CxV * 150uV + 1.5V\n\n
 *  CxV is reset to 0x800 on power-up and after clear command.
 */
typedef struct
{
	uint8_t	CVAR0 			;		/**< \brief C1V bits[7:0] */
	uint8_t	CVAR1 			;		/**< \brief C1V bits[15:8] */
	uint8_t	CVAR2 			;		/**< \brief C2V bits[7:0]  */
	uint8_t	CVAR3 			;		/**< \brief C2V bits[15:8] */
	uint8_t	CVAR4 			;		/**< \brief C3V bits[7:0]  */
	uint8_t	CVAR5 			;		/**< \brief C3V bits[15:8]  */
} ADBMS6830_RDCVA_t;

/**
 *  @brief Struct for Cell Voltage Register Group B
 *
 *  Cell Voltage 16 bit ADC measurement for Cell x.\n
 *  Voltage = CxV * 150uV + 1.5V\n\n
 *  CxV is reset to 0x800 on power-up and after clear command.
 */
typedef struct
{
	uint8_t	CVBR0 			;		/**< \brief C4V bits[7:0] */
	uint8_t	CVBR1 			;		/**< \brief C4V bits[15:8] */
	uint8_t	CVBR2 			;		/**< \brief C5V bits[7:0]  */
	uint8_t	CVBR3 			;		/**< \brief C5V bits[15:8] */
	uint8_t	CVBR4 			;		/**< \brief C6V bits[7:0]  */
	uint8_t	CVBR5 			;		/**< \brief C6V bits[15:8]  */
} ADBMS6830_RDCVB_t;

/**
 *  @brief Struct for Cell Voltage Register Group C
 *
 *  Cell Voltage 16 bit ADC measurement for Cell x.\n
 *  Voltage = CxV * 150uV + 1.5V\n\n
 *  CxV is reset to 0x800 on power-up and after clear command.
 */
typedef struct
{
	uint8_t	CVCR0 			;		/**< \brief C7V bits[7:0] */
	uint8_t	CVCR1 			;		/**< \brief C7V bits[15:8] */
	uint8_t	CVCR2 			;		/**< \brief C8V bits[7:0]  */
	uint8_t	CVCR3 			;		/**< \brief C8V bits[15:8] */
	uint8_t	CVCR4 			;		/**< \brief C9V bits[7:0]  */
	uint8_t	CVCR5 			;		/**< \brief C9V bits[15:8]  */
} ADBMS6830_RDCVC_t;

/**
 *  @brief Struct for Cell Voltage Register Group D
 *
 *  Cell Voltage 16 bit ADC measurement for Cell x.\n
 *  Voltage = CxV * 150uV + 1.5V\n\n
 *  CxV is reset to 0x800 on power-up and after clear command.
 */
typedef struct
{
	uint8_t	CVDR0 			;		/**< \brief C10V bits[7:0] */
	uint8_t	CVDR1 			;		/**< \brief C10V bits[15:8] */
	uint8_t	CVDR2 			;		/**< \brief C11V bits[7:0]  */
	uint8_t	CVDR3 			;		/**< \brief C11V bits[15:8] */
	uint8_t	CVDR4 			;		/**< \brief C12V bits[7:0]  */
	uint8_t	CVDR5 			;		/**< \brief C12V bits[15:8]  */
} ADBMS6830_RDCVD_t;

/**
 *  @brief Struct for Cell Voltage Register Group E
 *
 *  Cell Voltage 16 bit ADC measurement for Cell x.\n
 *  Voltage = CxV * 150uV + 1.5V\n\n
 *  CxV is reset to 0x800 on power-up and after clear command.
 */
typedef struct
{
	uint8_t	CVER0 			;		/**< \brief C13V bits[7:0] */
	uint8_t	CVER1 			;		/**< \brief C13V bits[15:8] */
	uint8_t	CVER2 			;		/**< \brief C14V bits[7:0]  */
	uint8_t	CVER3 			;		/**< \brief C14V bits[15:8] */
	uint8_t	CVER4 			;		/**< \brief C15V bits[7:0]  */
	uint8_t	CVER5 			;		/**< \brief C15V bits[15:8]  */
} ADBMS6830_RDCVE_t;

/**
 *  @brief Struct for Cell Voltage Register Group F
 *
 *  Cell Voltage 16 bit ADC measurement for Cell x.\n
 *  Voltage = CxV * 150uV + 1.5V\n\n
 *  CxV is reset to 0x800 on power-up and after clear command.
 */
typedef struct
{
	uint8_t	CVFR0 			;		/**< \brief C16V bits[7:0] */
	uint8_t	CVFR1 			;		/**< \brief C16V bits[15:8] */
	uint8_t	CVFR2 			;		/**< \brief reserved (0xff) */
	uint8_t	CVFR3 			;		/**< \brief reserved (0xff) */
	uint8_t	CVFR4 			;		/**< \brief reserved (0xff) */
	uint8_t	CVFR5 			;		/**< \brief reserved (0xff) */
} ADBMS6830_RDCVF_t;

/**
 *  @brief Struct for Cell Voltage Registers

 */
typedef struct
{
	ADBMS6830_RDCVA_t	RDCVA		;	/**< \brief Cell Voltage Group A */
	ADBMS6830_RDCVB_t	RDCVB  		;	/**< \brief Cell Voltage Group B */
	ADBMS6830_RDCVC_t	RDCVC		;	/**< \brief Cell Voltage Group C */
	ADBMS6830_RDCVD_t	RDCVD		;	/**< \brief Cell Voltage Group D */
	ADBMS6830_RDCVE_t	RDCVE		;	/**< \brief Cell Voltage Group E */
	ADBMS6830_RDCVF_t	RDCVF		;	/**< \brief Cell Voltage Group F */
} ADBMS6830_RDCV_t;

/**
 *  @brief Struct for Averaged Cell Voltage Register Group A
 *
 *  Cell Voltage 16 bit average of 8 conversion results for Cell x.\n
 *  Averaged Voltage = ACxV * 150uV + 1.5V\n\n
 *  ACxV is reset to 0x800 on power-up and after clear command.
 */
typedef struct
{
	uint8_t	ACVAR0 			;		/**< \brief AC1V bits[7:0] */
	uint8_t	ACVAR1 			;		/**< \brief AC1V bits[15:8] */
	uint8_t	ACVAR2 			;		/**< \brief AC2V bits[7:0]  */
	uint8_t	ACVAR3 			;		/**< \brief AC2V bits[15:8] */
	uint8_t	ACVAR4 			;		/**< \brief AC3V bits[7:0]  */
	uint8_t	ACVAR5 			;		/**< \brief AC3V bits[15:8]  */
} ADBMS6830_RDACVA_t;

/**
 *  @brief Struct for Averaged Cell Voltage Register Group B
 *
 *  Cell Voltage 16 bit average of 8 conversion results for Cell x.\n
 *  Averaged Voltage = ACxV * 150uV + 1.5V\n\n
 *  ACxV is reset to 0x800 on power-up and after clear command.
 */
typedef struct
{
	uint8_t	ACVBR0 			;		/**< \brief AC4V bits[7:0] */
	uint8_t	ACVBR1 			;		/**< \brief AC4V bits[15:8] */
	uint8_t	ACVBR2 			;		/**< \brief AC5V bits[7:0]  */
	uint8_t	ACVBR3 			;		/**< \brief AC5V bits[15:8] */
	uint8_t	ACVBR4 			;		/**< \brief AC6V bits[7:0]  */
	uint8_t	ACVBR5 			;		/**< \brief AC6V bits[15:8]  */
} ADBMS6830_RDACVB_t;

/**
 *  @brief Struct for Averaged Cell Voltage Register Group C
 *
 *  Cell Voltage 16 bit average of 8 conversion results for Cell x.\n
 *  Averaged Voltage = ACxV * 150uV + 1.5V\n\n
 *  ACxV is reset to 0x800 on power-up and after clear command.
 */
typedef struct
{
	uint8_t	ACVCR0 			;		/**< \brief AC7V bits[7:0] */
	uint8_t	ACVCR1 			;		/**< \brief AC7V bits[15:8] */
	uint8_t	ACVCR2 			;		/**< \brief AC8V bits[7:0]  */
	uint8_t	ACVCR3 			;		/**< \brief AC8V bits[15:8] */
	uint8_t	ACVCR4 			;		/**< \brief AC9V bits[7:0]  */
	uint8_t	ACVCR5 			;		/**< \brief AC9V bits[15:8]  */
} ADBMS6830_RDACVC_t;

/**
 *  @brief Struct for Averaged Cell Voltage Register Group D
 *
 *  Cell Voltage 16 bit average of 8 conversion results for Cell x.\n
 *  Averaged Voltage = ACxV * 150uV + 1.5V\n\n
 *  ACxV is reset to 0x800 on power-up and after clear command.
 */
typedef struct
{
	uint8_t	ACVDR0 			;		/**< \brief AC10V bits[7:0] */
	uint8_t	ACVDR1 			;		/**< \brief AC10V bits[15:8] */
	uint8_t	ACVDR2 			;		/**< \brief AC11V bits[7:0]  */
	uint8_t	ACVDR3 			;		/**< \brief AC11V bits[15:8] */
	uint8_t	ACVDR4 			;		/**< \brief AC12V bits[7:0]  */
	uint8_t	ACVDR5 			;		/**< \brief AC12V bits[15:8]  */
} ADBMS6830_RDACVD_t;

/**
 *  @brief Struct for Averaged Cell Voltage Register Group E
 *
 *  Cell Voltage 16 bit average of 8 conversion results for Cell x.\n
 *  Averaged Voltage = ACxV * 150uV + 1.5V\n\n
 *  ACxV is reset to 0x800 on power-up and after clear command.
 */
typedef struct
{
	uint8_t	ACVER0 			;		/**< \brief AC13V bits[7:0] */
	uint8_t	ACVER1 			;		/**< \brief AC13V bits[15:8] */
	uint8_t	ACVER2 			;		/**< \brief AC14V bits[7:0]  */
	uint8_t	ACVER3 			;		/**< \brief AC14V bits[15:8] */
	uint8_t	ACVER4 			;		/**< \brief AC15V bits[7:0]  */
	uint8_t	ACVER5 			;		/**< \brief AC15V bits[15:8]  */
} ADBMS6830_RDACVE_t;

/**
 *  @brief Struct for Averaged Cell Voltage Register Group F
 *
 *  Cell Voltage 16 bit average of 8 conversion results for Cell x.\n
 *  Averaged Voltage = ACxV * 150uV + 1.5V\n\n
 *  ACxV is reset to 0x800 on power-up and after clear command.
 */
typedef struct
{
	uint8_t	ACVFR0 			;		/**< \brief AC16V bits[7:0] */
	uint8_t	ACVFR1 			;		/**< \brief AC16V bits[15:8] */
	uint8_t	ACVFR2 			;		/**< \brief reserved (0xff) */
	uint8_t	ACVFR3 			;		/**< \brief reserved (0xff) */
	uint8_t	ACVFR4 			;		/**< \brief reserved (0xff) */
	uint8_t	ACVFR5 			;		/**< \brief reserved (0xff) */
} ADBMS6830_RDACVF_t;

/**
 *  @brief Struct for Averaged Cell Voltage Registers

 */
typedef struct
{
	ADBMS6830_RDACVA_t	RDACVA		;	/**< \brief Averaged Cell Voltage Group A */
	ADBMS6830_RDACVB_t	RDACVB  	;	/**< \brief Averaged Cell Voltage Group B */
	ADBMS6830_RDACVC_t	RDACVC		;	/**< \brief Averaged Cell Voltage Group C */
	ADBMS6830_RDACVD_t	RDACVD		;	/**< \brief Averaged Cell Voltage Group D */
	ADBMS6830_RDACVE_t	RDACVE		;	/**< \brief Averaged Cell Voltage Group E */
	ADBMS6830_RDACVF_t	RDACVF		;	/**< \brief Averaged Cell Voltage Group F */
} ADBMS6830_RDACV_t;

/**
 *  @brief Struct for Filtered Cell Voltage Register Group A
 *
 *  Cell Voltage 16 bit IIR filtered measurement for Cell x.\n
 *  Filtered Voltage = ACxV * 150uV + 1.5V\n\n
 *  FCxV is reset to 0x800 on power-up and after clear command.
 */
typedef struct
{
	uint8_t	FCVAR0 			;		/**< \brief FC1V bits[7:0] */
	uint8_t	FCVAR1 			;		/**< \brief FC1V bits[15:8] */
	uint8_t	FCVAR2 			;		/**< \brief FC2V bits[7:0]  */
	uint8_t	FCVAR3 			;		/**< \brief FC2V bits[15:8] */
	uint8_t	FCVAR4 			;		/**< \brief FC3V bits[7:0]  */
	uint8_t	FCVAR5 			;		/**< \brief FC3V bits[15:8]  */
} ADBMS6830_RDFCVA_t;

/**
 *  @brief Struct for Filtered Cell Voltage Register Group B
 *
 *  Cell Voltage 16 bit IIR filtered measurement for Cell x.\n
 *  Filtered Voltage = ACxV * 150uV + 1.5V\n\n
 *  FCxV is reset to 0x800 on power-up and after clear command.
 */
typedef struct
{
	uint8_t	FCVBR0 			;		/**< \brief FC4V bits[7:0] */
	uint8_t	FCVBR1 			;		/**< \brief FC4V bits[15:8] */
	uint8_t	FCVBR2 			;		/**< \brief FC5V bits[7:0]  */
	uint8_t	FCVBR3 			;		/**< \brief FC5V bits[15:8] */
	uint8_t	FCVBR4 			;		/**< \brief FC6V bits[7:0]  */
	uint8_t	FCVBR5 			;		/**< \brief FC6V bits[15:8]  */
} ADBMS6830_RDFCVB_t;

/**
 *  @brief Struct for Filtered Cell Voltage Register Group C
 *
 *  Cell Voltage 16 bit IIR filtered measurement for Cell x.\n
 *  Filtered Voltage = ACxV * 150uV + 1.5V\n\n
 *  FCxV is reset to 0x800 on power-up and after clear command.
 */
typedef struct
{
	uint8_t	FCVCR0 			;		/**< \brief FC7V bits[7:0] */
	uint8_t	FCVCR1 			;		/**< \brief FC7V bits[15:8] */
	uint8_t	FCVCR2 			;		/**< \brief FC8V bits[7:0]  */
	uint8_t	FCVCR3 			;		/**< \brief FC8V bits[15:8] */
	uint8_t	FCVCR4 			;		/**< \brief FC9V bits[7:0]  */
	uint8_t	FCVCR5 			;		/**< \brief FC9V bits[15:8]  */
} ADBMS6830_RDFCVC_t;

/**
 *  @brief Struct for Filtered Cell Voltage Register Group D
 *
 *  Cell Voltage 16 bit IIR filtered measurement for Cell x.\n
 *  Filtered Voltage = ACxV * 150uV + 1.5V\n\n
 *  FCxV is reset to 0x800 on power-up and after clear command.
 */
typedef struct
{
	uint8_t	FCVDR0 			;		/**< \brief FC10V bits[7:0] */
	uint8_t	FCVDR1 			;		/**< \brief FC10V bits[15:8] */
	uint8_t	FCVDR2 			;		/**< \brief FC11V bits[7:0]  */
	uint8_t	FCVDR3 			;		/**< \brief FC11V bits[15:8] */
	uint8_t	FCVDR4 			;		/**< \brief FC12V bits[7:0]  */
	uint8_t	FCVDR5 			;		/**< \brief FC12V bits[15:8]  */
} ADBMS6830_RDFCVD_t;

/**
 *  @brief Struct for Filtered Cell Voltage Register Group E
 *
 *  Cell Voltage 16 bit IIR filtered measurement for Cell x.\n
 *  Filtered Voltage = ACxV * 150uV + 1.5V\n\n
 *  FCxV is reset to 0x800 on power-up and after clear command.
 */
typedef struct
{
	uint8_t	FCVER0 			;		/**< \brief FC13V bits[7:0] */
	uint8_t	FCVER1 			;		/**< \brief FC13V bits[15:8] */
	uint8_t	FCVER2 			;		/**< \brief FC14V bits[7:0]  */
	uint8_t	FCVER3 			;		/**< \brief FC14V bits[15:8] */
	uint8_t	FCVER4 			;		/**< \brief FC15V bits[7:0]  */
	uint8_t	FCVER5 			;		/**< \brief FC15V bits[15:8]  */
} ADBMS6830_RDFCVE_t;

/**
 *  @brief Struct for Filtered Cell Voltage Register Group F
 *
 *  Cell Voltage 16 bit IIR filtered measurement for Cell x.\n
 *  Filtered Voltage = ACxV * 150uV + 1.5V\n\n
 *  FCxV is reset to 0x800 on power-up and after clear command.
 */
typedef struct
{
	uint8_t	FCVFR0 			;		/**< \brief FC16V bits[7:0] */
	uint8_t	FCVFR1 			;		/**< \brief FC16V bits[15:8] */
	uint8_t	FCVFR2 			;		/**< \brief reserved (0xff) */
	uint8_t	FCVFR3 			;		/**< \brief reserved (0xff) */
	uint8_t	FCVFR4 			;		/**< \brief reserved (0xff) */
	uint8_t	FCVFR5 			;		/**< \brief reserved (0xff) */
} ADBMS6830_RDFCVF_t;

/**
 *  @brief Struct for Filtered Cell Voltage Registers

 */
typedef struct
{
	ADBMS6830_RDFCVA_t	RDFCVA		;	/**< \brief Filtered Cell Voltage Group A */
	ADBMS6830_RDFCVB_t	RDFCVB  	;	/**< \brief Filtered Cell Voltage Group B */
	ADBMS6830_RDFCVC_t	RDFCVC		;	/**< \brief Filtered Cell Voltage Group C */
	ADBMS6830_RDFCVD_t	RDFCVD		;	/**< \brief Filtered Cell Voltage Group D */
	ADBMS6830_RDFCVE_t	RDFCVE		;	/**< \brief Filtered Cell Voltage Group E */
	ADBMS6830_RDFCVF_t	RDFCVF		;	/**< \brief Filtered Cell Voltage Group F */
} ADBMS6830_RDFCV_t;

/**
 *  @brief Struct for S-Voltage Register Group A
 *
 *  Sx Pin voltage:  16 bit ADC measurement value for Sx pin from ADSV or ADCV commands Cell x.\n
 *  Spin Voltage = SxV * 150uV + 1.5V\n\n
 *  SxV is reset to 0x800 on power-up and after clear command.
 */
typedef struct
{
	uint8_t	SVAR0 			;		/**< \brief S1V bits[7:0] */
	uint8_t	SVAR1 			;		/**< \brief S1V bits[15:8] */
	uint8_t	SVAR2 			;		/**< \brief S2V bits[7:0]  */
	uint8_t	SVAR3 			;		/**< \brief S2V bits[15:8] */
	uint8_t	SVAR4 			;		/**< \brief S3V bits[7:0]  */
	uint8_t	SVAR5 			;		/**< \brief S3V bits[15:8]  */
} ADBMS6830_RDSVA_t;

/**
 *  @brief Struct for S-Voltage Register Group B
 *
 *  Sx Pin voltage:  16 bit ADC measurement value for Sx pin from ADSV or ADCV commands Cell x.\n
 *  Spin Voltage = SxV * 150uV + 1.5V\n\n
 *  SxV is reset to 0x800 on power-up and after clear command.
 */
typedef struct
{
	uint8_t	SVBR0 			;		/**< \brief S4V bits[7:0] */
	uint8_t	SVBR1 			;		/**< \brief S4V bits[15:8] */
	uint8_t	SVBR2 			;		/**< \brief S5V bits[7:0]  */
	uint8_t	SVBR3 			;		/**< \brief S5V bits[15:8] */
	uint8_t	SVBR4 			;		/**< \brief S6V bits[7:0]  */
	uint8_t	SVBR5 			;		/**< \brief S6V bits[15:8]  */
} ADBMS6830_RDSVB_t;

/**
 *  @brief Struct for S-Voltage Register Group C
 *
 *  Sx Pin voltage:  16 bit ADC measurement value for Sx pin from ADSV or ADCV commands Cell x.\n
 *  Spin Voltage = SxV * 150uV + 1.5V\n\n
 *  SxV is reset to 0x800 on power-up and after clear command.
 */
typedef struct
{
	uint8_t	SVCR0 			;		/**< \brief S7V bits[7:0] */
	uint8_t	SVCR1 			;		/**< \brief S7V bits[15:8] */
	uint8_t	SVCR2 			;		/**< \brief S8V bits[7:0]  */
	uint8_t	SVCR3 			;		/**< \brief S8V bits[15:8] */
	uint8_t	SVCR4 			;		/**< \brief S9V bits[7:0]  */
	uint8_t	SVCR5 			;		/**< \brief S9V bits[15:8]  */
} ADBMS6830_RDSVC_t;

/**
 *  @brief Struct for S-Voltage Register Group D
 *
 *  Sx Pin voltage:  16 bit ADC measurement value for Sx pin from ADSV or ADCV commands Cell x.\n
 *  Spin Voltage = SxV * 150uV + 1.5V\n\n
 *  SxV is reset to 0x800 on power-up and after clear command.
 */
typedef struct
{
	uint8_t	SVDR0 			;		/**< \brief S10V bits[7:0] */
	uint8_t	SVDR1 			;		/**< \brief S10V bits[15:8] */
	uint8_t	SVDR2 			;		/**< \brief S11V bits[7:0]  */
	uint8_t	SVDR3 			;		/**< \brief S11V bits[15:8] */
	uint8_t	SVDR4 			;		/**< \brief S12V bits[7:0]  */
	uint8_t	SVDR5 			;		/**< \brief S12V bits[15:8]  */
} ADBMS6830_RDSVD_t;

/**
 *  @brief Struct for S-Voltage Register Group E
 *
 *  Sx Pin voltage:  16 bit ADC measurement value for Sx pin from ADSV or ADCV commands Cell x.\n
 *  Spin Voltage = SxV * 150uV + 1.5V\n\n
 *  SxV is reset to 0x800 on power-up and after clear command.
 */
typedef struct
{
	uint8_t	SVER0 			;		/**< \brief S13V bits[7:0] */
	uint8_t	SVER1 			;		/**< \brief S13V bits[15:8] */
	uint8_t	SVER2 			;		/**< \brief S14V bits[7:0]  */
	uint8_t	SVER3 			;		/**< \brief S14V bits[15:8] */
	uint8_t	SVER4 			;		/**< \brief S15V bits[7:0]  */
	uint8_t	SVER5 			;		/**< \brief S15V bits[15:8]  */
} ADBMS6830_RDSVE_t;

/**
 *  @brief Struct for S-Voltage Register Group F
 *
 *  Sx Pin voltage:  16 bit ADC measurement value for Sx pin from ADSV or ADCV commands Cell x.\n
 *  Spin Voltage = SxV * 150uV + 1.5V\n\n
 *  SxV is reset to 0x800 on power-up and after clear command.
 */
typedef struct
{
	uint8_t	SVFR0 			;		/**< \brief S16V bits[7:0] */
	uint8_t	SVFR1 			;		/**< \brief S16V bits[15:8] */
	uint8_t	SVFR2 			;		/**< \brief reserved (0xff) */
	uint8_t	SVFR3 			;		/**< \brief reserved (0xff) */
	uint8_t	SVFR4 			;		/**< \brief reserved (0xff) */
	uint8_t	SVFR5 			;		/**< \brief reserved (0xff) */
} ADBMS6830_RDSVF_t;

/**
 *  @brief Struct for S-Voltage Registers

 */
typedef struct
{
	ADBMS6830_RDSVA_t	RDSVA		;	/**< \brief S-Voltage Group A */
	ADBMS6830_RDSVB_t	RDSVB  		;	/**< \brief S-Voltage Group B */
	ADBMS6830_RDSVC_t	RDSVC		;	/**< \brief S-Voltage Group C */
	ADBMS6830_RDSVD_t	RDSVD		;	/**< \brief S-Voltage Group D */
	ADBMS6830_RDSVE_t	RDSVE		;	/**< \brief S-Voltage Group E */
	ADBMS6830_RDSVF_t	RDSVF		;	/**< \brief S-Voltage Group F */
} ADBMS6830_RDSV_t;

/**
 *  @brief Struct for Auxiliary Register Group A
 *
 *  Gx GPIOx voltage:  16 bit ADC measurement value for GPIOx voltage.\n
 *  GPIOx Voltage = GxV * 150uV + 1.5V\n\n
 *  GxV is reset to 0x800 on power-up and after clear command.
 */
typedef struct
{
	uint8_t	GPAR0 			;		/**< \brief G1V bits[7:0] */
	uint8_t	GPAR1 			;		/**< \brief G1V bits[15:8] */
	uint8_t	GPAR2 			;		/**< \brief G2V bits[7:0]  */
	uint8_t	GPAR3 			;		/**< \brief G2V bits[15:8] */
	uint8_t	GPAR4 			;		/**< \brief G3V bits[7:0]  */
	uint8_t	GPAR5 			;		/**< \brief G3V bits[15:8]  */
} ADBMS6830_RDGPA_t;

/**
 *  @brief Struct for Auxiliary Register Group B
 *
 *  Gx GPIOx voltage:  16 bit ADC measurement value for GPIOx voltage.\n
 *  GPIOx Voltage = GxV * 150uV + 1.5V\n\n
 *  GxV is reset to 0x800 on power-up and after clear command.
 */
typedef struct
{
	uint8_t	GPBR0 			;		/**< \brief G4V bits[7:0] */
	uint8_t	GPBR1 			;		/**< \brief G4V bits[15:8] */
	uint8_t	GPBR2 			;		/**< \brief G5V bits[7:0]  */
	uint8_t	GPBR3 			;		/**< \brief G5V bits[15:8] */
	uint8_t	GPBR4 			;		/**< \brief G6V bits[7:0]  */
	uint8_t	GPBR5 			;		/**< \brief G6V bits[15:8]  */
} ADBMS6830_RDGPB_t;

/**
 *  @brief Struct for Auxiliary Register Group C
 *
 *  Gx GPIOx voltage:  16 bit ADC measurement value for GPIOx voltage.\n
 *  GPIOx Voltage = GxV * 150uV + 1.5V\n\n
 *  GxV is reset to 0x800 on power-up and after clear command.
 */
typedef struct
{
	uint8_t	GPCR0 			;		/**< \brief G7V bits[7:0] */
	uint8_t	GPCR1 			;		/**< \brief G7V bits[15:8] */
	uint8_t	GPCR2 			;		/**< \brief G8V bits[7:0]  */
	uint8_t	GPCR3 			;		/**< \brief G8V bits[15:8] */
	uint8_t	GPCR4 			;		/**< \brief G9V bits[7:0]  */
	uint8_t	GPCR5 			;		/**< \brief G9V bits[15:8]  */
} ADBMS6830_RDGPC_t;

/**
 *  @brief Struct for Auxiliary Register Group D
 *
 *  Gx GPIOx voltage:  16 bit ADC measurement value for GPIOx voltage.\n
 *  GPIOx Voltage = GxV * 150uV + 1.5V\n\n
 *  GxV is reset to 0x800 on power-up and after clear command.\n
 *  \n
 *  VPV - 16bit ADC measurement of V+ to V- = 25 * (VPV*150uV+1.5V)\n
 *  VMV - 16bit ADC measurement of V- to V- = VMV*150uV+1.5V
 */
typedef struct
{
	uint8_t	GPDR0 			;		/**< \brief G10V bits[7:0] */
	uint8_t	GPDR1 			;		/**< \brief G10V bits[15:8] */
	uint8_t	GPDR2 			;		/**< \brief VMV bits[7:0]  */
	uint8_t	GPDR3 			;		/**< \brief VMV bits[15:8] */
	uint8_t	GPDR4 			;		/**< \brief VPV bits[7:0]  */
	uint8_t	GPDR5 			;		/**< \brief VPV bits[15:8]  */
} ADBMS6830_RDGPD_t;

/**
 *  @brief Struct for Auxiliary Registers

 */
typedef struct
{
	ADBMS6830_RDGPA_t	RDAUXA		;	/**< \brief Auxiliary Register Group A */
	ADBMS6830_RDGPB_t	RDAUXB  	;	/**< \brief Auxiliary Register Group B */
	ADBMS6830_RDGPC_t	RDAUXC		;	/**< \brief Auxiliary Register Group C */
	ADBMS6830_RDGPD_t	RDAUXD		;	/**< \brief Auxiliary Register Group D */
} ADBMS6830_RDAUX_t;

/**
 *  @brief Struct for Redundant Auxiliary Register Group A
 *
 *  R_Gx GPIOx voltage:  16 bit ADC measurement value for GPIOx voltage.\n
 *  GPIOx Voltage = R_GxV * 150uV + 1.5V\n\n
 *  R_GxV is reset to 0x800 on power-up and after clear command.
 */
typedef struct
{
	uint8_t	RGPAR0 			;		/**< \brief R_G1V bits[7:0] */
	uint8_t	RGPAR1 			;		/**< \brief R_G1V bits[15:8] */
	uint8_t	RGPAR2 			;		/**< \brief R_G2V bits[7:0]  */
	uint8_t	RGPAR3 			;		/**< \brief R_G2V bits[15:8] */
	uint8_t	RGPAR4 			;		/**< \brief R_G3V bits[7:0]  */
	uint8_t	RGPAR5 			;		/**< \brief R_G3V bits[15:8]  */
} ADBMS6830_RDRGPA_t;

/**
 *  @brief Struct for Redundant Auxiliary Register Group B
 *
 *  R_Gx GPIOx voltage:  16 bit ADC measurement value for GPIOx voltage.\n
 *  GPIOx Voltage = R_GxV * 150uV + 1.5V\n\n
 *  R_GxV is reset to 0x800 on power-up and after clear command.
 */
typedef struct
{
	uint8_t	RGPBR0 			;		/**< \brief R_G4V bits[7:0] */
	uint8_t	RGPBR1 			;		/**< \brief R_G4V bits[15:8] */
	uint8_t	RGPBR2 			;		/**< \brief R_G5V bits[7:0]  */
	uint8_t	RGPBR3 			;		/**< \brief R_G5V bits[15:8] */
	uint8_t	RGPBR4 			;		/**< \brief R_G6V bits[7:0]  */
	uint8_t	RGPBR5 			;		/**< \brief R_G6V bits[15:8]  */
} ADBMS6830_RDRGPB_t;

/**
 *  @brief Struct for Redundant Auxiliary Register Group C
 *
 *  R_Gx GPIOx voltage:  16 bit ADC measurement value for GPIOx voltage.\n
 *  GPIOx Voltage = R_GxV * 150uV + 1.5V\n\n
 *  R_GxV is reset to 0x800 on power-up and after clear command.
 */
typedef struct
{
	uint8_t	RGPCR0 			;		/**< \brief R_G7V bits[7:0] */
	uint8_t	RGPCR1 			;		/**< \brief R_G7V bits[15:8] */
	uint8_t	RGPCR2 			;		/**< \brief R_G8V bits[7:0]  */
	uint8_t	RGPCR3 			;		/**< \brief R_G8V bits[15:8] */
	uint8_t	RGPCR4 			;		/**< \brief R_G9V bits[7:0]  */
	uint8_t	RGPCR5 			;		/**< \brief R_G9V bits[15:8]  */
} ADBMS6830_RDRGPC_t;

/**
 *  @brief Struct for Redundant Auxiliary Register Group D
 *
 *  R_Gx GPIOx voltage:  16 bit ADC measurement value for GPIOx voltage.\n
 *  GPIOx Voltage = R_GxV * 150uV + 1.5V\n\n
 *  R_GxV is reset to 0x800 on power-up and after clear command.\n
 */
typedef struct
{
	uint8_t	RGPDR0 			;		/**< \brief R_G10V bits[7:0] */
	uint8_t	RGPDR1 			;		/**< \brief R_G10V bits[15:8] */
	uint8_t	RGPDR2 			;		/**< \brief reserved (0xff) */
	uint8_t	RGPDR3 			;		/**< \brief reserved (0xff) */
	uint8_t	RGPDR4 			;		/**< \brief reserved (0xff) */
	uint8_t	RGPDR5 			;		/**< \brief reserved (0xff) */
} ADBMS6830_RDRGPD_t;

/**
 *  @brief Struct for Redundant Auxiliary Registers

 */
typedef struct
{
	ADBMS6830_RDRGPA_t	RDRAXA		;	/**< \brief Redundant Auxiliary Register Group A */
	ADBMS6830_RDRGPB_t	RDRAXB  	;	/**< \brief Redundant Auxiliary Register Group B */
	ADBMS6830_RDRGPC_t	RDRAXC		;	/**< \brief Redundant Auxiliary Register Group C */
	ADBMS6830_RDRGPD_t	RDRAXD		;	/**< \brief Redundant Auxiliary Register Group D */
} ADBMS6830_RDRAX_t;

/**
 *  @brief Struct for Status Register Group A
 *
 */
typedef struct
{
	uint8_t	STAR0 			;		/**< \brief VREF2 bits[7:0] Secondary Reference voltage\n
												VREF2 = VREF2[15:0]x150uV + 1.5V\n
												Normal range is within 2.988 to 3.012V*/
	uint8_t	STAR1 			;		/**< \brief VREF2 bits[15:8] */
	uint8_t	STAR2 			;		/**< \brief ITMP bits[7:0] Internal Die Temperature\n
	 	 	 	 	 	 	 	 	 	 	 	ITMP = ((ITMP[15:0] x 150uV +1.5V)/.0075)-273*/
	uint8_t	STAR3 			;		/**< \brief ITMP bits[7:0] */
	uint8_t	STAR4 			;		/**< \brief reserved  */
	uint8_t	STAR5 			;		/**< \brief reserved  */
} ADBMS6830_STATA_t;

/**
 *  @brief Struct for Status Register Group B
 *
 */
typedef struct
{
	uint8_t	STBR0 			;		/**< \brief VD bits[7:0] Digital power supply voltage\n
												VD = VD[15:0]x150uV + 1.5V\n
												Normal range is within 2.7 to 3.6V*/
	uint8_t	STBR1 			;		/**< \brief VD bits[15:8] */
	uint8_t	STBR2 			;		/**< \brief VA bits[7:0] Analog power supply voltage\n
	 	 	 	 	 	 	 	 	 	 	 	VA = VA[15:0]x150uV + 1.5V\n
	 	 	 	 	 	 	 	 	 	 	 	VA is set by external components and must be in the range of 4.5 to 5.5V*/
	uint8_t	STBR3 			;		/**< \brief VA bits[7:0] */
	uint8_t	STBR4 			;		/**< \brief VRES bits[7:0]  VREF2 across series resistor for open wire check\n
	 	 	 	 	 	 	 	 	 	 	 	VRES = VRES[15:0]x150uV + 1.5V\n*/
	uint8_t	STBR5 			;		/**< \brief VRES bits[15:8]   */
} ADBMS6830_STATB_t;

/**
 *  @brief Struct for Status Group C Register 0
 */
typedef struct
{
	uint8_t		CS1FLT		: 1;	/**< \brief Bit 0: C-ADC vs S-ADC fault of channel 1\n
	 	 	 	 	 	 	 	 	 	 	 	 	   Read: 1 = a mismatch between C-ADC and S-ADC measurement on Channel 1 occurred.\n
	 	 	 	 	 	 	 	 	 	 	 	 	   Read: 0 = mismatch between C-ADC and S-ADC measurement on Channel 1 occurred.no */
	uint8_t		CS2FLT		: 1;	/**< \brief Bit 1: C-ADC vs S-ADC fault of channel 2*/
	uint8_t		CS3FLT		: 1;	/**< \brief Bit 2: C-ADC vs S-ADC fault of channel 3*/
	uint8_t		CS4FLT		: 1;	/**< \brief Bit 3: C-ADC vs S-ADC fault of channel 4*/
	uint8_t		CS5FLT		: 1;	/**< \brief Bit 4: C-ADC vs S-ADC fault of channel 5*/
	uint8_t		CS6FLT		: 1;	/**< \brief Bit 5: C-ADC vs S-ADC fault of channel 6*/
	uint8_t		CS7FLT		: 1;	/**< \brief Bit 6: C-ADC vs S-ADC fault of channel 7*/
	uint8_t		CS8FLT		: 1;	/**< \brief Bit 7: C-ADC vs S-ADC fault of channel 8*/
}ADBMS6830_STCR0_s;

/**
 *  @brief Union for Configuration Group A Register 0
 */
typedef union
{
	ADBMS6830_STCR0_s B;
	uint8_t U8;
}ADBMS6830_STCR0_t;

/**
 *  @brief Struct for Status Group C Register 1
 */
typedef struct
{
	uint8_t		CS9FLT		: 1;	/**< \brief Bit 0: C-ADC vs S-ADC fault of channel 9\n
	 	 	 	 	 	 	 	 	 	 	 	 	   Read: 1 = a mismatch between C-ADC and S-ADC measurement on Channel 1 occurred.\n
	 	 	 	 	 	 	 	 	 	 	 	 	   Read: 0 = mismatch between C-ADC and S-ADC measurement on Channel 1 occurred.no */
	uint8_t		CS10FLT		: 1;	/**< \brief Bit 1: C-ADC vs S-ADC fault of channel 10*/
	uint8_t		CS11FLT		: 1;	/**< \brief Bit 2: C-ADC vs S-ADC fault of channel 11*/
	uint8_t		CS12FLT		: 1;	/**< \brief Bit 3: C-ADC vs S-ADC fault of channel 12*/
	uint8_t		CS13FLT		: 1;	/**< \brief Bit 4: C-ADC vs S-ADC fault of channel 13*/
	uint8_t		CS14FLT		: 1;	/**< \brief Bit 5: C-ADC vs S-ADC fault of channel 14*/
	uint8_t		CS15FLT		: 1;	/**< \brief Bit 6: C-ADC vs S-ADC fault of channel 15*/
	uint8_t		CS16FLT		: 1;	/**< \brief Bit 7: C-ADC vs S-ADC fault of channel 16*/
}ADBMS6830_STCR1_s;

/**
 *  @brief Union for Configuration Group A Register 1
 */
typedef union
{
	ADBMS6830_STCR1_s B;
	uint8_t U8;
}ADBMS6830_STCR1_t;

/**
 *  @brief Struct for Status Group C Register 2
 */
typedef struct
{
	uint8_t		CT_HI		: 5;	/**< \brief Bits 0-4: CT[10:6]Free running C-ADC Conversion counter\n Resets with every ADCV command.  Rolls over maximum value.  */
	uint8_t		reserved	: 3;	/**< \brief Bits 5-7: reserved*/
}ADBMS6830_STCR2_s;

/**
 *  @brief Union for Configuration Group A Register 2
 */
typedef union
{
	ADBMS6830_STCR2_s B;
	uint8_t U8;
}ADBMS6830_STCR2_t;

/**
 *  @brief Struct for Status Group C Register 3
 */
typedef struct
{
	uint8_t 	CTS			: 2;	/**< \brief Bits 0&1: Free running C-ADC subsample conversion counter.\n 4 increments per sample*/
	uint8_t		CT_LO		: 6;	/**< \brief Bits 2-7: CT[5:0]Free running C-ADC Conversion counter\n Resets with every ADCV command.  Rolls over maximum value.  */
}ADBMS6830_STCR3_s;

/**
 *  @brief Union for Configuration Group A Register 3
 */
typedef union
{
	ADBMS6830_STCR3_s B;
	uint8_t U8;
}ADBMS6830_STCR3_t;

/**
 *  @brief Struct for Status Group C Register 4
 */
typedef struct
{
	uint8_t		SMED		: 1;	/**< \brief Bit 0: S-Trim Multiple Error Detection */
	uint8_t		SED			: 1;	/**< \brief Bit 1: S-Trim Error Detection*/
	uint8_t		CMED		: 1;	/**< \brief Bit 2: C-Trim Multiple Error Detection*/
	uint8_t		CED			: 1;	/**< \brief Bit 3: C-Trim Error Detection*/
	uint8_t		VD_UV		: 1;	/**< \brief Bit 4: 3V digital rail under voltage\n  This bit can be cleared using CLRFLAG command with CL_VDUV=1*/
	uint8_t		VD_OV		: 1;	/**< \brief Bit 5: 3V digital rail over voltage\n  This bit can be cleared using CLRFLAG command with CL_VDOV=1*/
	uint8_t		VA_UV		: 1;	/**< \brief Bit 6: 5V digital rail under voltage\n  This bit can be cleared using CLRFLAG command with CL_VAUV=1*/
	uint8_t		VA_OV		: 1;	/**< \brief Bit 7: 5V digital rail over voltage\n  This bit can be cleared using CLRFLAG command with CL_VAOV=1*/
}ADBMS6830_STCR4_s;

/**
 *  @brief Union for Configuration Group C Register 4
 */
typedef union
{
	ADBMS6830_STCR4_s B;
	uint8_t U8;
}ADBMS6830_STCR4_t;

/**
 *  @brief Struct for Status Group C Register 5
 */
typedef struct
{
	uint8_t		OSCCHK		: 1;	/**< \brief Bit 0: Oscillator Check\n 1 = an out of range oscillator count detected during an ADC operation */
	uint8_t		TMODCHK		: 1;	/**< \brief Bit 1: Test mode detection\n 1 = the device has previously activated a test mode*/
	uint8_t		THSD		: 1;	/**< \brief Bit 2: Thermal shutdown status*/
	uint8_t		SLEEP		: 1;	/**< \brief Bit 3: Sleep mode detection\n 1 = this device has previously powered cycled or entered sleep mode*/
	uint8_t		SPIFLT		: 1;	/**< \brief Bit 4: SPI fault*/
	uint8_t		COMP		: 1;	/**< \brief Bit 5: Comparison between C-ADC and S-ADC results is active\n  This allows latent fault detection.  Please refer to the safety manual*/
	uint8_t		VDE			: 1;	/**< \brief Bit 6: Supply rail delta\n 1 = all the 5V supplies differed from Vreg by more than 0.5V*/
	uint8_t		VDEL		: 1;	/**< \brief Bit 7: Supply rail delta latent.  Allows to check supply rail monitors for latent fault.*/
}ADBMS6830_STCR5_s;

/**
 *  @brief Union for Configuration Group C Register 5
 */
typedef union
{
	ADBMS6830_STCR5_s B;
	uint8_t U8;
}ADBMS6830_STCR5_t;

/**
 *  @brief Struct for Status Group C Register
 */
typedef struct
{
	ADBMS6830_STCR0_t	STCR0 			;		/**< \brief Status Group C Register 0 */
	ADBMS6830_STCR1_t	STCR1 			;		/**< \brief Status Group C Register 1 */
	ADBMS6830_STCR2_t	STCR2 			;		/**< \brief Status Group C Register 2 */
	ADBMS6830_STCR3_t	STCR3 			;		/**< \brief Status Group C Register 3 */
	ADBMS6830_STCR4_t	STCR4 			;		/**< \brief Status Group C Register 4 */
	ADBMS6830_STCR5_t	STCR5 			;		/**< \brief Status Group C Register 5 */
} ADBMS6830_STATC_t;


/**
 *  @brief Struct for Status Group D Register 0
 */
typedef struct
{
	uint8_t		C1UV		: 1;	/**< \brief Bit 0: Cell 1 Under voltage flag*/
	uint8_t		C1OV		: 1;	/**< \brief Bit 1: Cell 1 Over voltage flag*/
	uint8_t		C2UV		: 1;	/**< \brief Bit 2: Cell 2 Under voltage flag*/
	uint8_t		C2OV		: 1;	/**< \brief Bit 3: Cell 2 Over voltage flag*/
	uint8_t		C3UV		: 1;	/**< \brief Bit 4: Cell 3 Under voltage flag*/
	uint8_t		C3OV		: 1;	/**< \brief Bit 5: Cell 3 Over voltage flag*/
	uint8_t		C4UV		: 1;	/**< \brief Bit 6: Cell 4 Under voltage flag*/
	uint8_t		C4OV		: 1;	/**< \brief Bit 7: Cell 4 Over voltage flag*/
}ADBMS6830_STDR0_s;

/**
 *  @brief Union for Configuration Group D Register 0
 */
typedef union
{
	ADBMS6830_STDR0_s B;
	uint8_t U8;
}ADBMS6830_STDR0_t;

/**
 *  @brief Struct for Status Group D Register 1
 */
typedef struct
{
	uint8_t		C5UV		: 1;	/**< \brief Bit 0: Cell 5 Under voltage flag*/
	uint8_t		C5OV		: 1;	/**< \brief Bit 1: Cell 5 Over voltage flag*/
	uint8_t		C6UV		: 1;	/**< \brief Bit 2: Cell 6 Under voltage flag*/
	uint8_t		C6OV		: 1;	/**< \brief Bit 3: Cell 6 Over voltage flag*/
	uint8_t		C7UV		: 1;	/**< \brief Bit 4: Cell 7 Under voltage flag*/
	uint8_t		C7OV		: 1;	/**< \brief Bit 5: Cell 7 Over voltage flag*/
	uint8_t		C8UV		: 1;	/**< \brief Bit 6: Cell 8 Under voltage flag*/
	uint8_t		C8OV		: 1;	/**< \brief Bit 7: Cell 8 Over voltage flag*/
}ADBMS6830_STDR1_s;

/**
 *  @brief Union for Configuration Group D Register 1
 */
typedef union
{
	ADBMS6830_STDR1_s B;
	uint8_t U8;
}ADBMS6830_STDR1_t;

/**
 *  @brief Struct for Status Group D Register 2
 */
typedef struct
{
	uint8_t		C9UV		: 1;	/**< \brief Bit 0: Cell 9 Under voltage flag*/
	uint8_t		C9OV		: 1;	/**< \brief Bit 1: Cell 9 Over voltage flag*/
	uint8_t		C10UV		: 1;	/**< \brief Bit 2: Cell 10 Under voltage flag*/
	uint8_t		C10OV		: 1;	/**< \brief Bit 3: Cell 10 Over voltage flag*/
	uint8_t		C11UV		: 1;	/**< \brief Bit 4: Cell 11 Under voltage flag*/
	uint8_t		C11OV		: 1;	/**< \brief Bit 5: Cell 11 Over voltage flag*/
	uint8_t		C12UV		: 1;	/**< \brief Bit 6: Cell 12 Under voltage flag*/
	uint8_t		C12OV		: 1;	/**< \brief Bit 7: Cell 12 Over voltage flag*/
}ADBMS6830_STDR2_s;

/**
 *  @brief Union for Configuration Group D Register 2
 */
typedef union
{
	ADBMS6830_STDR2_s B;
	uint8_t U8;
}ADBMS6830_STDR2_t;

/**
 *  @brief Struct for Status Group D Register 3
 */
typedef struct
{
	uint8_t		C13UV		: 1;	/**< \brief Bit 0: Cell 13 Under voltage flag*/
	uint8_t		C13OV		: 1;	/**< \brief Bit 1: Cell 13 Over voltage flag*/
	uint8_t		C14UV		: 1;	/**< \brief Bit 2: Cell 14 Under voltage flag*/
	uint8_t		C14OV		: 1;	/**< \brief Bit 3: Cell 14 Over voltage flag*/
	uint8_t		C15UV		: 1;	/**< \brief Bit 4: Cell 15 Under voltage flag*/
	uint8_t		C15OV		: 1;	/**< \brief Bit 5: Cell 15 Over voltage flag*/
	uint8_t		C16UV		: 1;	/**< \brief Bit 6: Cell 16 Under voltage flag*/
	uint8_t		C16OV		: 1;	/**< \brief Bit 7: Cell 16 Over voltage flag*/
}ADBMS6830_STDR3_s;

/**
 *  @brief Union for Configuration Group D Register 3
 */
typedef union
{
	ADBMS6830_STDR3_s B;
	uint8_t U8;
}ADBMS6830_STDR3_t;

/**
 *  @brief Struct for Status Group D Register 4
 */
typedef struct
{
	uint8_t		reserved	: 8;	/**< \brief Bits 0-7: reserved*/
}ADBMS6830_STDR4_t;


/**
 *  @brief Struct for Status Group D Register 5
 */
typedef struct
{
	uint8_t		OC_CNTR		: 8;	/**< \brief Bits 0-7: Oscillator check counter.\n Stores the results of the oscillator counter check.*/
}ADBMS6830_STDR5_s;

/**
 *  @brief Union for Configuration Group D Register 5
 */
typedef union
{
	ADBMS6830_STDR5_s B;
	uint8_t U8;
}ADBMS6830_STDR5_t;

/**
 *  @brief Struct for Status Group D Register
 */
typedef struct
{
	ADBMS6830_STDR0_t	STDR0 			;		/**< \brief Status Group D Register 0 */
	ADBMS6830_STDR1_t	STDR1 			;		/**< \brief Status Group D Register 1 */
	ADBMS6830_STDR2_t	STDR2 			;		/**< \brief Status Group D Register 2 */
	ADBMS6830_STDR3_t	STDR3 			;		/**< \brief Status Group D Register 3 */
	ADBMS6830_STDR4_t	STDR4 			;		/**< \brief Status Group D Register 4 */
	ADBMS6830_STDR5_t	STDR5 			;		/**< \brief Status Group D Register 5 */
} ADBMS6830_STATD_t;

/**
 *  @brief Struct for Status Group E Register 0
 */
typedef struct
{
	uint8_t		reserved	: 8;	/**< \brief Bits 0-7: reserved*/
}ADBMS6830_STER0_t;

/**
 *  @brief Struct for Status Group E Register 1
 */
typedef struct
{
	uint8_t		reserved	: 8;	/**< \brief Bits 0-7: reserved*/
}ADBMS6830_STER1_t;

/**
 *  @brief Struct for Status Group E Register 2
 */
typedef struct
{
	uint8_t		reserved	: 8;	/**< \brief Bits 0-7: reserved*/
}ADBMS6830_STER2_t;

/**
 *  @brief Struct for Status Group E Register 3
 */
typedef struct
{
	uint8_t		reserved	: 8;	/**< \brief Bits 0-7: reserved*/
}ADBMS6830_STER3_t;

/**
 *  @brief Struct for Status Group E Register 4
 */
typedef struct
{
	uint8_t		GPI1		: 1;	/**< \brief Bit 0: GPIO 1 Pin state*/
	uint8_t		GPI2		: 1;	/**< \brief Bit 1: GPIO 2 Pin state*/
	uint8_t		GPI3		: 1;	/**< \brief Bit 2: GPIO 3 Pin state*/
	uint8_t		GPI4		: 1;	/**< \brief Bit 3: GPIO 4 Pin state*/
	uint8_t		GPI5		: 1;	/**< \brief Bit 4: GPIO 5 Pin state*/
	uint8_t		GPI6		: 1;	/**< \brief Bit 5: GPIO 6 Pin state*/
	uint8_t		GPI7		: 1;	/**< \brief Bit 6: GPIO 7 Pin state*/
	uint8_t		GPI8		: 1;	/**< \brief Bit 7: GPIO 8 Pin state*/
}ADBMS6830_STER4_s;

/**
 *  @brief Union for Configuration Group E Register 4
 */
typedef union
{
	ADBMS6830_STER4_s B;
	uint8_t U8;
}ADBMS6830_STER4_t;

/**
 *  @brief Struct for Status Group E Register 5
 */
typedef struct
{
	uint8_t		GPI9		: 1;	/**< \brief Bit 0: GPIO 1 Pin state*/
	uint8_t		GPI10		: 1;	/**< \brief Bit 1: GPIO 2 Pin state*/
	uint8_t		reserved	: 2;	/**< \brief Bits 2&3: reserved*/
	uint8_t		REV			: 4;	/**< \brief Bits 4-7: Device Revision Code*/
}ADBMS6830_STER5_s;

/**
 *  @brief Union for Configuration Group E Register 5
 */
typedef union
{
	ADBMS6830_STER5_s B;
	uint8_t U8;
}ADBMS6830_STER5_t;

/**
 *  @brief Struct for Status Group E Register
 */
typedef struct
{
	ADBMS6830_STER0_t	STER0 			;		/**< \brief Status Group E Register 0 */
	ADBMS6830_STER1_t	STER1 			;		/**< \brief Status Group E Register 1 */
	ADBMS6830_STER2_t	STER2 			;		/**< \brief Status Group E Register 2 */
	ADBMS6830_STER3_t	STER3 			;		/**< \brief Status Group E Register 3 */
	ADBMS6830_STER4_t	STER4 			;		/**< \brief Status Group E Register 4 */
	ADBMS6830_STER5_t	STER5 			;		/**< \brief Status Group E Register 5 */
} ADBMS6830_STATE_t;

/**
 *  @brief Struct for Status Group RegisterS
 */
typedef struct
{
	ADBMS6830_STATA_t	STATA 			;		/**< \brief Status Group A Register */
	ADBMS6830_STATB_t	STATB 			;		/**< \brief Status Group B Register */
	ADBMS6830_STATC_t	STATC 			;		/**< \brief Status Group C Register */
	ADBMS6830_STATD_t	STATD 			;		/**< \brief Status Group D Register */
	ADBMS6830_STATE_t	STATE 			;		/**< \brief Status Group E Register */
} ADBMS6830_STAT_t;

/**
 *  @brief Struct for COMM group register 0
 */
typedef struct
{
	uint8_t		FCOM0		: 4;	/**< \brief Bit 0: Final Communication Control Bits\n
										Write for I2C communication\n
										0000 = master ACK\n
										1000 = master NACK\n
										1001 = master NACK+stop\n\n
										Read for SPI Communication\n
										x000 = CSB low\n
										1001 = CSB high\n\n
										Read for I2C communication\n
										0000 = ACK from master\n
										0111 = ACK from slave\n
										1111 = NACK from slave\n
										0001 = ACK from slave + stop from master\n
										1001 = NACK from slave + stop from master*/
	uint8_t		ICOM0		: 4;	/**< \brief Bit 1: Initial Communication Control Bits\n
	 	 	 	 	 	 	 	 	 	 Write for I2C Communication\n
	 	 	 	 	 	 	 	 	 	 0110 = start\n
	 	 	 	 	 	 	 	 	 	 0001 = stop\n
	 	 	 	 	 	 	 	 	 	 0000 = blank\n
	 	 	 	 	 	 	 	 	 	 0111 = no transmit\n\n
	 	 	 	 	 	 	 	 	 	 Read for SPI communication\n
	 	 	 	 	 	 	 	 	 	 1000 = CSB low\n
	 	 	 	 	 	 	 	 	 	 1010 = CSB falling edge\n
	 	 	 	 	 	 	 	 	 	 1001 = CSB high\n
	 	 	 	 	 	 	 	 	 	 1111 = no transmit*/
}ADBMS6830_COMM0_s;

/**
 *  @brief Union for COMM group register 0
 */
typedef union
{
	ADBMS6830_COMM0_s B;
	uint8_t U8;
}ADBMS6830_COMM0_t;

/**
 *  @brief Struct for COMM group register 1
 */
typedef struct
{
	uint8_t		D0			: 8;	/**< \brief Bits 0-7: I2C/SPI communication data byte*/
}ADBMS6830_COMM1_t;


/**
 *  @brief Struct for COMM group register 2
 */
typedef struct
{
	uint8_t		FCOM1		: 4;	/**< \brief Bit 0: Final Communication Control Bits\n
										Write for I2C communication\n
										0000 = master ACK\n
										1000 = master NACK\n
										1001 = master NACK+stop\n\n
										Read for SPI Communication\n
										x000 = CSB low\n
										1001 = CSB high\n\n
										Read for I2C communication\n
										0000 = ACK from master\n
										0111 = ACK from slave\n
										1111 = NACK from slave\n
										0001 = ACK from slave + stop from master\n
										1001 = NACK from slave + stop from master*/
	uint8_t		ICOM1		: 4;	/**< \brief Bit 1: Initial Communication Control Bits\n
	 	 	 	 	 	 	 	 	 	 Write for I2C Communication\n
	 	 	 	 	 	 	 	 	 	 0110 = start\n
	 	 	 	 	 	 	 	 	 	 0001 = stop\n
	 	 	 	 	 	 	 	 	 	 0000 = blank\n
	 	 	 	 	 	 	 	 	 	 0111 = no transmit\n\n
	 	 	 	 	 	 	 	 	 	 Read for SPI communication\n
	 	 	 	 	 	 	 	 	 	 1000 = CSB low\n
	 	 	 	 	 	 	 	 	 	 1010 = CSB falling edge\n
	 	 	 	 	 	 	 	 	 	 1001 = CSB high\n
	 	 	 	 	 	 	 	 	 	 1111 = no transmit*/
}ADBMS6830_COMM2_s;

/**
 *  @brief Union for COMM group register 2
 */
typedef union
{
	ADBMS6830_COMM2_s B;
	uint8_t U8;
}ADBMS6830_COMM2_t;

/**
 *  @brief Struct for COMM group register 3
 */
typedef struct
{
	uint8_t		D1			: 8;	/**< \brief Bits 0-7: I2C/SPI communication data byte*/
}ADBMS6830_COMM3_t;

/**
 *  @brief Struct for COMM group register 4
 */
typedef struct
{
	uint8_t		FCOM2		: 4;	/**< \brief Bit 0: Final Communication Control Bits\n
										Write for I2C communication\n
										0000 = master ACK\n
										1000 = master NACK\n
										1001 = master NACK+stop\n\n
										Read for SPI Communication\n
										x000 = CSB low\n
										1001 = CSB high\n\n
										Read for I2C communication\n
										0000 = ACK from master\n
										0111 = ACK from slave\n
										1111 = NACK from slave\n
										0001 = ACK from slave + stop from master\n
										1001 = NACK from slave + stop from master*/
	uint8_t		ICOM2		: 4;	/**< \brief Bit 1: Initial Communication Control Bits\n
	 	 	 	 	 	 	 	 	 	 Write for I2C Communication\n
	 	 	 	 	 	 	 	 	 	 0110 = start\n
	 	 	 	 	 	 	 	 	 	 0001 = stop\n
	 	 	 	 	 	 	 	 	 	 0000 = blank\n
	 	 	 	 	 	 	 	 	 	 0111 = no transmit\n\n
	 	 	 	 	 	 	 	 	 	 Read for SPI communication\n
	 	 	 	 	 	 	 	 	 	 1000 = CSB low\n
	 	 	 	 	 	 	 	 	 	 1010 = CSB falling edge\n
	 	 	 	 	 	 	 	 	 	 1001 = CSB high\n
	 	 	 	 	 	 	 	 	 	 1111 = no transmit*/
}ADBMS6830_COMM4_s;

/**
 *  @brief Union for COMM group register 4
 */
typedef union
{
	ADBMS6830_COMM4_s B;
	uint8_t U8;
}ADBMS6830_COMM4_t;

/**
 *  @brief Struct for COMM group register 5
 */
typedef struct
{
	uint8_t		D2			: 8;	/**< \brief Bits 0-7: I2C/SPI communication data byte*/
}ADBMS6830_COMM5_t;

/**
 *  @brief Struct for COMM register group
 */
typedef struct
{
	ADBMS6830_COMM0_t	COMM0 			;		/**< \brief COMM Group Register 0 */
	ADBMS6830_COMM1_t	COMM1 			;		/**< \brief COMM Group Register 1 */
	ADBMS6830_COMM2_t	COMM2 			;		/**< \brief COMM Group Register 2 */
	ADBMS6830_COMM3_t	COMM3 			;		/**< \brief COMM Group Register 3 */
	ADBMS6830_COMM4_t	COMM4 			;		/**< \brief COMM Group Register 4 */
	ADBMS6830_COMM5_t	COMM5 			;		/**< \brief COMM Group Register 5 */
} ADBMS6830_COMM_t;

/**
 *  @brief Struct for PWM group A register 0
 */
typedef struct
{
	uint8_t		PWM1		: 4;	/**< \brief Bits 0-3: PWM Configuration\n
	 	 	 	 	 	 	 	 	 	 	 1111 - 100% duty cycle\n
	 	 	 	 	 	 	 	 	 	 	 0001 - 6.6% duty cycle\n
	 	 	 	 	 	 	 	 	 	 	 0000 - disabled (default)*/
	uint8_t		PWM2		: 4;	/**< \brief Bits 4-7: PWM Configuration\n
	 	 	 	 	 	 	 	 	 	 	 1111 - 100% duty cycle\n
	 	 	 	 	 	 	 	 	 	 	 0001 - 6.6% duty cycle\n
	 	 	 	 	 	 	 	 	 	 	 0000 - disabled (default)*/
}ADBMS6830_PWMR0_s;

/**
 *  @brief Union for PWM group A register 0
 */
typedef union
{
	ADBMS6830_PWMR0_s B;
	uint8_t U8;
}ADBMS6830_PWMR0_t;

/**
 *  @brief Struct for PWM group A register 1
 */
typedef struct
{
	uint8_t		PWM3		: 4;	/**< \brief Bits 0-3: PWM Configuration\n
	 	 	 	 	 	 	 	 	 	 	 1111 - 100% duty cycle\n
	 	 	 	 	 	 	 	 	 	 	 0001 - 6.6% duty cycle\n
	 	 	 	 	 	 	 	 	 	 	 0000 - disabled (default)*/
	uint8_t		PWM4		: 4;	/**< \brief Bits 4-7: PWM Configuration\n
	 	 	 	 	 	 	 	 	 	 	 1111 - 100% duty cycle\n
	 	 	 	 	 	 	 	 	 	 	 0001 - 6.6% duty cycle\n
	 	 	 	 	 	 	 	 	 	 	 0000 - disabled (default)*/
}ADBMS6830_PWMR1_s;

/**
 *  @brief Union for PWM group A register 1
 */
typedef union
{
	ADBMS6830_PWMR1_s B;
	uint8_t U8;
}ADBMS6830_PWMR1_t;

/**
 *  @brief Struct for PWM group A register 2
 */
typedef struct
{
	uint8_t		PWM5		: 4;	/**< \brief Bits 0-3: PWM Configuration\n
	 	 	 	 	 	 	 	 	 	 	 1111 - 100% duty cycle\n
	 	 	 	 	 	 	 	 	 	 	 0001 - 6.6% duty cycle\n
	 	 	 	 	 	 	 	 	 	 	 0000 - disabled (default)*/
	uint8_t		PWM6		: 4;	/**< \brief Bits 4-7: PWM Configuration\n
	 	 	 	 	 	 	 	 	 	 	 1111 - 100% duty cycle\n
	 	 	 	 	 	 	 	 	 	 	 0001 - 6.6% duty cycle\n
	 	 	 	 	 	 	 	 	 	 	 0000 - disabled (default)*/
}ADBMS6830_PWMR2_s;

/**
 *  @brief Union for PWM group A register 2
 */
typedef union
{
	ADBMS6830_PWMR2_s B;
	uint8_t U8;
}ADBMS6830_PWMR2_t;

/**
 *  @brief Struct for PWM group A register 3
 */
typedef struct
{
	uint8_t		PWM7		: 4;	/**< \brief Bits 0-3: PWM Configuration\n
	 	 	 	 	 	 	 	 	 	 	 1111 - 100% duty cycle\n
	 	 	 	 	 	 	 	 	 	 	 0001 - 6.6% duty cycle\n
	 	 	 	 	 	 	 	 	 	 	 0000 - disabled (default)*/
	uint8_t		PWM8		: 4;	/**< \brief Bits 4-7: PWM Configuration\n
	 	 	 	 	 	 	 	 	 	 	 1111 - 100% duty cycle\n
	 	 	 	 	 	 	 	 	 	 	 0001 - 6.6% duty cycle\n
	 	 	 	 	 	 	 	 	 	 	 0000 - disabled (default)*/
}ADBMS6830_PWMR3_s;

/**
 *  @brief Union for PWM group A register 3
 */
typedef union
{
	ADBMS6830_PWMR3_s B;
	uint8_t U8;
}ADBMS6830_PWMR3_t;

/**
 *  @brief Struct for PWM group A register 4
 */
typedef struct
{
	uint8_t		PWM9		: 4;	/**< \brief Bits 0-3: PWM Configuration\n
	 	 	 	 	 	 	 	 	 	 	 1111 - 100% duty cycle\n
	 	 	 	 	 	 	 	 	 	 	 0001 - 6.6% duty cycle\n
	 	 	 	 	 	 	 	 	 	 	 0000 - disabled (default)*/
	uint8_t		PWM10		: 4;	/**< \brief Bits 4-7: PWM Configuration\n
	 	 	 	 	 	 	 	 	 	 	 1111 - 100% duty cycle\n
	 	 	 	 	 	 	 	 	 	 	 0001 - 6.6% duty cycle\n
	 	 	 	 	 	 	 	 	 	 	 0000 - disabled (default)*/
}ADBMS6830_PWMR4_s;

/**
 *  @brief Union for PWM group A register 4
 */
typedef union
{
	ADBMS6830_PWMR4_s B;
	uint8_t U8;
}ADBMS6830_PWMR4_t;

/**
 *  @brief Struct for PWM group A register 5
 */
typedef struct
{
	uint8_t		PWM11		: 4;	/**< \brief Bits 0-3: PWM Configuration\n
	 	 	 	 	 	 	 	 	 	 	 1111 - 100% duty cycle\n
	 	 	 	 	 	 	 	 	 	 	 0001 - 6.6% duty cycle\n
	 	 	 	 	 	 	 	 	 	 	 0000 - disabled (default)*/
	uint8_t		PWM12		: 4;	/**< \brief Bits 4-7: PWM Configuration\n
	 	 	 	 	 	 	 	 	 	 	 1111 - 100% duty cycle\n
	 	 	 	 	 	 	 	 	 	 	 0001 - 6.6% duty cycle\n
	 	 	 	 	 	 	 	 	 	 	 0000 - disabled (default)*/
}ADBMS6830_PWMR5_s;

/**
 *  @brief Union for PWM group A register 5
 */
typedef union
{
	ADBMS6830_PWMR5_s B;
	uint8_t U8;
}ADBMS6830_PWMR5_t;

/**
 *  @brief Struct for PWMA Register group
 */
typedef struct
{
	ADBMS6830_PWMR0_t 	PWMR0;			/**< \brief PWMA Group Register 0 */
	ADBMS6830_PWMR1_t 	PWMR1;			/**< \brief PWMA Group Register 1 */
	ADBMS6830_PWMR2_t 	PWMR2;			/**< \brief PWMA Group Register 2 */
	ADBMS6830_PWMR3_t 	PWMR3;			/**< \brief PWMA Group Register 3 */
	ADBMS6830_PWMR4_t 	PWMR4;			/**< \brief PWMA Group Register 4 */
	ADBMS6830_PWMR5_t 	PWMR5;			/**< \brief PWMA Group Register 5 */
}ADBMS6830_PWMA_t;

/**
 *  @brief Struct for PWM group B register 0
 */
typedef struct
{
	uint8_t		PWM13		: 4;	/**< \brief Bits 0-3: PWM Configuration\n
	 	 	 	 	 	 	 	 	 	 	 1111 - 100% duty cycle\n
	 	 	 	 	 	 	 	 	 	 	 0001 - 6.6% duty cycle\n
	 	 	 	 	 	 	 	 	 	 	 0000 - disabled (default)*/
	uint8_t		PWM14		: 4;	/**< \brief Bits 4-7: PWM Configuration\n
	 	 	 	 	 	 	 	 	 	 	 1111 - 100% duty cycle\n
	 	 	 	 	 	 	 	 	 	 	 0001 - 6.6% duty cycle\n
	 	 	 	 	 	 	 	 	 	 	 0000 - disabled (default)*/
}ADBMS6830_PSR0_s;

/**
 *  @brief Union for PWM group B register 0
 */
typedef union
{
	ADBMS6830_PSR0_s B;
	uint8_t U8;
}ADBMS6830_PSR0_t;

/**
 *  @brief Struct for PWM group B register 1
 */
typedef struct
{
	uint8_t		PWM15		: 4;	/**< \brief Bits 0-3: PWM Configuration\n
	 	 	 	 	 	 	 	 	 	 	 1111 - 100% duty cycle\n
	 	 	 	 	 	 	 	 	 	 	 0001 - 6.6% duty cycle\n
	 	 	 	 	 	 	 	 	 	 	 0000 - disabled (default)*/
	uint8_t		PWM16		: 4;	/**< \brief Bits 4-7: PWM Configuration\n
	 	 	 	 	 	 	 	 	 	 	 1111 - 100% duty cycle\n
	 	 	 	 	 	 	 	 	 	 	 0001 - 6.6% duty cycle\n
	 	 	 	 	 	 	 	 	 	 	 0000 - disabled (default)*/
}ADBMS6830_PSR1_s;

/**
 *  @brief Union for PWM group B register 1
 */
typedef union
{
	ADBMS6830_PSR1_s B;
	uint8_t U8;
}ADBMS6830_PSR1_t;

/**
 *  @brief Struct for PWM group B register 2
 */
typedef struct
{
	uint8_t		reserved		: 4;	/**< \brief Bits 0-7: reserved*/

}ADBMS6830_PSR2_t;

/**
 *  @brief Struct for PWM group B register 3
 */
typedef struct
{
	uint8_t		reserved		: 4;	/**< \brief Bits 0-7: reserved*/

}ADBMS6830_PSR3_t;

/**
 *  @brief Struct for PWM group B register 4
 */
typedef struct
{
	uint8_t		reserved		: 4;	/**< \brief Bits 0-7: reserved*/

}ADBMS6830_PSR4_t;

/**
 *  @brief Struct for PWM group B register 5
 */
typedef struct
{
	uint8_t		reserved		: 4;	/**< \brief Bits 0-7: reserved*/

}ADBMS6830_PSR5_t;

/**
 *  @brief Struct for PWMB Register group
 */
typedef struct
{
	ADBMS6830_PSR0_t 	PSR0;			/**< \brief PWMB Group Register 0 */
	ADBMS6830_PSR1_t 	PSR1;			/**< \brief PWMB Group Register 1 */
	ADBMS6830_PSR2_t 	PSR2;			/**< \brief PWMB Group Register 2 */
	ADBMS6830_PSR3_t 	PSR3;			/**< \brief PWMB Group Register 3 */
	ADBMS6830_PSR4_t 	PSR4;			/**< \brief PWMB Group Register 4 */
	ADBMS6830_PSR5_t 	PSR5;			/**< \brief PWMB Group Register 5 */
}ADBMS6830_PWMB_t;

/**
 *  @brief Struct for Retention Register
 *
 *  The ADBMS6830 retains 6 bytes of data in the retention register even during sleep mode.\n
 *  To write to this register, send an unlock retention register command (ULRR) followed by a write retention\n
 *  command (WRRR) with 6 bytes of RR data followed by the data PEC.  Any other commands sent after a ULRR command\n
 *  locks the writing to the retention group.
 */
typedef struct
{
	uint8_t	RRR0 			;		/**< \brief RR0 bits[7:0] */
	uint8_t	RRR1 			;		/**< \brief RR1 bits[15:8] */
	uint8_t	RRR2 			;		/**< \brief RR2 bits[7:0]  */
	uint8_t	RRR3 			;		/**< \brief RR3 bits[15:8] */
	uint8_t	RRR4 			;		/**< \brief RR4 bits[7:0]  */
	uint8_t	RRR5 			;		/**< \brief RR5 bits[15:8]  */
} ADBMS6830_RDRR_t;

/**
 *  @brief Struct for Serial ID Register Group
 *
 *	48bit factory programmed serial ID code.
 */
typedef struct
{
	uint8_t	SIDR0 			;		/**< \brief SID bits[7:0] */
	uint8_t	SIDR1 			;		/**< \brief SID bits[15:8] */
	uint8_t	SIDR2 			;		/**< \brief SID bits[23:16]  */
	uint8_t	SIDR3 			;		/**< \brief SID bits[31:24] */
	uint8_t	SIDR4 			;		/**< \brief SID bits[39:32]  */
	uint8_t	SIDR5 			;		/**< \brief SID bits[47:40]  */
} ADBMS6830_RDSID_t;

/**
 * @brief Structure that contains the raw cell voltage combined measurements from all the voltages
 * */
typedef struct
{
	uint16_t	C1V;								/**< \brief C1V raw*/
	uint16_t	C2V;								/**< \brief C1V raw*/
	uint16_t	C3V;								/**< \brief C1V raw*/
	uint16_t	C4V;								/**< \brief C1V raw*/
	uint16_t	C5V;								/**< \brief C1V raw*/
	uint16_t	C6V;								/**< \brief C1V raw*/
	uint16_t	C7V;								/**< \brief C1V raw*/
	uint16_t	C8V;								/**< \brief C1V raw*/
	uint16_t	C9V;								/**< \brief C1V raw*/
	uint16_t	C10V;								/**< \brief C1V raw*/
	uint16_t	C11V;								/**< \brief C1V raw*/
	uint16_t	C12V;								/**< \brief C1V raw*/
	uint16_t	C13V;								/**< \brief C1V raw*/
	uint16_t	C14V;								/**< \brief C1V raw*/
	uint16_t	C15V;								/**< \brief C1V raw*/
	uint16_t	C16V;								/**< \brief C1V raw*/
}RAW_CELL_t;

/**
 * @brief Structure that contains the raw cell voltage combined measurements from all the voltages
 * */
typedef struct
{
	uint16_t	AC1V;								/**< \brief AC1V raw*/
	uint16_t	AC2V;								/**< \brief AC2V raw*/
	uint16_t	AC3V;								/**< \brief AC3V raw*/
	uint16_t	AC4V;								/**< \brief AC4V raw*/
	uint16_t	AC5V;								/**< \brief AC5V raw*/
	uint16_t	AC6V;								/**< \brief AC6V raw*/
	uint16_t	AC7V;								/**< \brief AC7V raw*/
	uint16_t	AC8V;								/**< \brief AC8V raw*/
	uint16_t	AC9V;								/**< \brief AC9V raw*/
	uint16_t	AC10V;								/**< \brief AC10V raw*/
	uint16_t	AC11V;								/**< \brief AC11V raw*/
	uint16_t	AC12V;								/**< \brief AC12V raw*/
	uint16_t	AC13V;								/**< \brief AC13V raw*/
	uint16_t	AC14V;								/**< \brief AC14V raw*/
	uint16_t	AC15V;								/**< \brief AC15V raw*/
	uint16_t	AC16V;								/**< \brief AC16V raw*/
}RAW_AVERAGED_CELL_t;

/**
 * @brief Structure that contains the raw filtered cell voltage combined measurements from all the voltages
 * */
typedef struct
{
	uint16_t	FC1V;								/**< \brief FC1V raw*/
	uint16_t	FC2V;								/**< \brief FC2V raw*/
	uint16_t	FC3V;								/**< \brief FC3V raw*/
	uint16_t	FC4V;								/**< \brief FC4V raw*/
	uint16_t	FC5V;								/**< \brief FC5V raw*/
	uint16_t	FC6V;								/**< \brief FC6V raw*/
	uint16_t	FC7V;								/**< \brief FC7V raw*/
	uint16_t	FC8V;								/**< \brief FC8V raw*/
	uint16_t	FC9V;								/**< \brief FC9V raw*/
	uint16_t	FC10V;								/**< \brief FC10V raw*/
	uint16_t	FC11V;								/**< \brief FC11V raw*/
	uint16_t	FC12V;								/**< \brief FC12V raw*/
	uint16_t	FC13V;								/**< \brief FC13V raw*/
	uint16_t	FC14V;								/**< \brief FC14V raw*/
	uint16_t	FC15V;								/**< \brief FC15V raw*/
	uint16_t	FC16V;								/**< \brief FC16V raw*/
}RAW_FILTERED_CELL_t;

/**
 * @brief Structure that contains the raw s-voltage combined measurements from all the voltages
 * */
typedef struct
{
	uint16_t	S1V;								/**< \brief S1V raw*/
	uint16_t	S2V;								/**< \brief S2V raw*/
	uint16_t	S3V;								/**< \brief S3V raw*/
	uint16_t	S4V;								/**< \brief S4V raw*/
	uint16_t	S5V;								/**< \brief S5V raw*/
	uint16_t	S6V;								/**< \brief S6V raw*/
	uint16_t	S7V;								/**< \brief S7V raw*/
	uint16_t	S8V;								/**< \brief S8V raw*/
	uint16_t	S9V;								/**< \brief S9V raw*/
	uint16_t	S10V;								/**< \brief S10V raw*/
	uint16_t	S11V;								/**< \brief S11V raw*/
	uint16_t	S12V;								/**< \brief S12V raw*/
	uint16_t	S13V;								/**< \brief S13V raw*/
	uint16_t	S14V;								/**< \brief S14V raw*/
	uint16_t	S15V;								/**< \brief S15V raw*/
	uint16_t	S16V;								/**< \brief S16V raw*/
}RAW_S_VOLTAGE_t;

/**
 * @brief Structure that contains the raw AUX combined measurements from all the GPIOs
 * */
typedef struct
{
	uint16_t	G1V;								/**< \brief G1V raw*/
	uint16_t	G2V;								/**< \brief G2V raw*/
	uint16_t	G3V;								/**< \brief G3V raw*/
	uint16_t	G4V;								/**< \brief G4V raw*/
	uint16_t	G5V;								/**< \brief G5V raw*/
	uint16_t	G6V;								/**< \brief G6V raw*/
	uint16_t	G7V;								/**< \brief G7V raw*/
	uint16_t	G8V;								/**< \brief G8V raw*/
	uint16_t	G9V;								/**< \brief G9V raw*/
	uint16_t	G10V;								/**< \brief G10V raw*/
	uint16_t	VMV;								/**< \brief V- to V- raw measurement*/
	uint16_t	VPV;								/**< \brief V+ to V- raw measurement*/
}RAW_AUX_t;

/**
 * @brief Structure that contains the raw redundant AUX combined measurements from all the GPIOs
 * */
typedef struct
{
	uint16_t	R_G1V;								/**< \brief redundant G1V raw*/
	uint16_t	R_G2V;								/**< \brief redundant G2V raw*/
	uint16_t	R_G3V;								/**< \brief redundant G3V raw*/
	uint16_t	R_G4V;								/**< \brief redundant G4V raw*/
	uint16_t	R_G5V;								/**< \brief redundant G5V raw*/
	uint16_t	R_G6V;								/**< \brief redundant G6V raw*/
	uint16_t	R_G7V;								/**< \brief redundant G7V raw*/
	uint16_t	R_G8V;								/**< \brief redundant G8V raw*/
	uint16_t	R_G9V;								/**< \brief redundant G9V raw*/
	uint16_t	R_G10V;								/**< \brief redundant G10V raw*/
}RAW_RAXA_t;

/**
 * @brief Structure that contains the raw Status Voltage combined measurements
 * */
typedef struct
{
	uint16_t	VREF2;								/**< \brief Second reference voltage raw*/
	uint16_t	ITMP;								/**< \brief Internal die temperature raw*/
	uint16_t	VD;									/**< \brief Digital power supply voltage raw*/
	uint16_t	VA;									/**< \brief Analog power supply voltage raw*/
	uint16_t	VRES;								/**< \brief Vref2 across resistor raw*/
}RAW_STATV_t;

/**
 * @brief Structure that contains the raw cell voltage combined measurements from all the voltages
 * */
typedef struct
{
	RAW_CELL_t	Cell;								/***< \brief Structure that contains the raw cell voltages*/
	RAW_AVERAGED_CELL_t	AvCell;						/***< \brief Structure that contains the raw averaged cell voltages*/
	RAW_FILTERED_CELL_t FiltCell;					/***< \brief Structure that contains the raw filtered cell voltages*/
	RAW_S_VOLTAGE_t	S_Volt;							/***< \brief Structure that contains the raw s pin voltages*/
	RAW_AUX_t Aux;									/***< \brief Structure that contains the GPIOs voltages*/
	RAW_RAXA_t R_Aux;								/***< \brief Structure that contains the redundant GPIOs voltages*/
	RAW_STATV_t Status_Voltages;					/***< \brief Structure that contains the Status voltages*/
}RAW_DATA_t;

/**
 * @brief Structure that is used to clear the CS-mismatch faults from Status C register
 * */
typedef struct
{
	uint8_t	CL_CS1FLT 			:1;					/**< \brief Bit 0: Clear CS1FLT in STCR0*/
	uint8_t	CL_CS2FLT			:1;					/**< \brief Bit 1: Clear CS2FLT in STCR0*/
	uint8_t	CL_CS3FLT			:1;					/**< \brief Bit 2: Clear CS3FLT in STCR0*/
	uint8_t	CL_CS4FLT			:1;					/**< \brief Bit 3: Clear CS4FLT in STCR0*/
	uint8_t	CL_CS5FLT			:1;					/**< \brief Bit 4: Clear CS5FLT in STCR0*/
	uint8_t	CL_CS6FLT			:1;					/**< \brief Bit 5: Clear CS6FLT in STCR0*/
	uint8_t	CL_CS7FLT			:1;					/**< \brief Bit 6: Clear CS7FLT in STCR0*/
	uint8_t	CL_CS8FLT			:1;					/**< \brief Bit 7: Clear CS8FLT in STCR0*/
}ADBMS6830_CFD0_s;

/**
 *  @brief Union for CFD0
 */
typedef union
{
	ADBMS6830_CFD0_s B;
	uint8_t U8;
}ADBMS6830_CFD0_t;

/**
 * @brief Structure that is used to clear the the CS-mismatch faults from Status C register
 * */
typedef struct
{
	uint8_t	CL_CS9FLT 			:1;					/**< \brief Bit 0: Clear CS9FLT in STCR1*/
	uint8_t	CL_CS10FLT			:1;					/**< \brief Bit 1: Clear CS10FLT in STCR1*/
	uint8_t	CL_CS11FLT			:1;					/**< \brief Bit 2: Clear CS11FLT in STCR1*/
	uint8_t	CL_CS12FLT			:1;					/**< \brief Bit 3: Clear CS12FLT in STCR1*/
	uint8_t	CL_CS13FLT			:1;					/**< \brief Bit 4: Clear CS13FLT in STCR1*/
	uint8_t	CL_CS14FLT			:1;					/**< \brief Bit 5: Clear CS14FLT in STCR1*/
	uint8_t	CL_CS15FLT			:1;					/**< \brief Bit 6: Clear CS15FLT in STCR1*/
	uint8_t	CL_CS16FLT			:1;					/**< \brief Bit 7: Clear CS16FLT in STCR1*/
}ADBMS6830_CFD1_s;

/**
 *  @brief Union for CFD1
 */
typedef union
{
	ADBMS6830_CFD1_s B;
	uint8_t U8;
}ADBMS6830_CFD1_t;

/**
 * @brief Structure that is used to clear the faults from Status C register STCR4\n
 *
 * write 1 to clear
 *
 * */
typedef struct
{
	uint8_t	CL_CLSMED 			:1;					/**< \brief Bit 0: Clear SMED in STCR4*/
	uint8_t	CL_SED				:1;					/**< \brief Bit 1: Clear SED in STCR4*/
	uint8_t	CL_CMED				:1;					/**< \brief Bit 2: Clear CMED in STCR4*/
	uint8_t	CL_CED				:1;					/**< \brief Bit 3: Clear CED in STCR4*/
	uint8_t	CL_VDUV				:1;					/**< \brief Bit 4: Clear VDUV in STCR4*/
	uint8_t	CL_VDOV				:1;					/**< \brief Bit 5: Clear VDOV in STCR4*/
	uint8_t	CL_VAUV				:1;					/**< \brief Bit 6: Clear VAUV in STCR4*/
	uint8_t	CL_VAOV				:1;					/**< \brief Bit 7: Clear VAOV in STCR4*/
}ADBMS6830_CFD4_s;

/**
 *  @brief Union for CFD4
 */
typedef union
{
	ADBMS6830_CFD4_s B;
	uint8_t U8;
}ADBMS6830_CFD4_t;

/**
 * @brief Structure that is used to clear the faults from Status C register STCR5\n
 *
 * write 1 to clear
 *
 * */
typedef struct
{
	uint8_t	CL_OSCCHK 			:1;					/**< \brief Bit 0: Clear OSCCHK in STCR5*/
	uint8_t	CL_TMODE			:1;					/**< \brief Bit 1: Clear TMODE in STCR5*/
	uint8_t	CL_THSD				:1;					/**< \brief Bit 2: Clear THSD in STCR5*/
	uint8_t	CL_SLEEP			:1;					/**< \brief Bit 3: Clear SLEEP in STCR5*/
	uint8_t	CL_SPIFLT			:1;					/**< \brief Bit 4: Clear SPIFLT in STCR5*/
	uint8_t	reserved			:1;					/**< \brief Bit 5: reserved*/
	uint8_t	CL_VDE				:1;					/**< \brief Bit 6: Clear VDE in STCR5*/
	uint8_t	CL_VDEL				:1;					/**< \brief Bit 7: Clear VDEL in STCR5*/
}ADBMS6830_CFD5_s;

/**
 *  @brief Union for CFD5
 */
typedef union
{
	ADBMS6830_CFD5_s B;
	uint8_t U8;
}ADBMS6830_CFD5_t;

/**
 *
 * @brief Structure that is used to clear the faults from Status C register
 *
 * */
typedef struct
{
	ADBMS6830_CFD0_t CFDO;							/**< \brief Clear fault flags for STCR0*/
	ADBMS6830_CFD1_t CFD1;							/**< \brief Clear fault flags for STCR1*/
	uint8_t		 CFD2;							/**< \brief reserved*/
	uint8_t		 CFD3;							/**< \brief reserved*/
	ADBMS6830_CFD4_t CFD4;							/**< \brief Clear fault flags for STCR4*/
	ADBMS6830_CFD5_t CFD5;							/**< \brief Clear fault flags for STCR5*/
} ADBMS6830_CL_FLT_t;

/**
 * @brief Structure that is used to clear the OV/UV faults from Status D register STDR0
 * */
typedef struct
{
	uint8_t	CL_C1UV				:1;					/**< \brief Bit 0: Clear C1UV in STDR0*/
	uint8_t	CL_C1OV				:1;					/**< \brief Bit 1: Clear C1OV in STDR0*/
	uint8_t	CL_C2UV				:1;					/**< \brief Bit 2: Clear C2UV in STDR0*/
	uint8_t	CL_C2OV				:1;					/**< \brief Bit 3: Clear C2OV in STDR0*/
	uint8_t	CL_C3UV				:1;					/**< \brief Bit 4: Clear C3UV in STDR0*/
	uint8_t	CL_C3OV				:1;					/**< \brief Bit 5: Clear C3OV in STDR0*/
	uint8_t	CL_C4UV				:1;					/**< \brief Bit 6: Clear C4UV in STDR0*/
	uint8_t	CL_C4OV				:1;					/**< \brief Bit 7: Clear C4OV in STDR0*/
}ADBMS6830_CL_STDR0_s;

/**
 *  @brief Union for CL_STDR0
 */
typedef union
{
	ADBMS6830_CL_STDR0_s B;
	uint8_t U8;
}ADBMS6830_CL_STDR0_T;

/**
 * @brief Structure that is used to clear the OV/UV faults from Status D register STDR1
 * */
typedef struct
{
	uint8_t	CL_C5UV				:1;					/**< \brief Bit 0: Clear C5UV in STDR1*/
	uint8_t	CL_C5OV				:1;					/**< \brief Bit 1: Clear C5OV in STDR1*/
	uint8_t	CL_C6UV				:1;					/**< \brief Bit 2: Clear C6UV in STDR1*/
	uint8_t	CL_C6OV				:1;					/**< \brief Bit 3: Clear C6OV in STDR1*/
	uint8_t	CL_C7UV				:1;					/**< \brief Bit 4: Clear C7UV in STDR1*/
	uint8_t	CL_C7OV				:1;					/**< \brief Bit 5: Clear C7OV in STDR1*/
	uint8_t	CL_C8UV				:1;					/**< \brief Bit 6: Clear C8UV in STDR1*/
	uint8_t	CL_C8OV				:1;					/**< \brief Bit 7: Clear C8OV in STDR1*/
}ADBMS6830_CL_STDR1_s;

/**
 *  @brief Union for CL_STDR1
 */
typedef union
{
	ADBMS6830_CL_STDR1_s B;
	uint8_t U8;
}ADBMS6830_CL_STDR1_T;

/**
 * @brief Structure that is used to clear the OV/UV faults from Status D register STDR2
 * */
typedef struct
{
	uint8_t	CL_C9UV				:1;					/**< \brief Bit 0: Clear C9UV in STDR2*/
	uint8_t	CL_C9OV				:1;					/**< \brief Bit 1: Clear C9OV in STDR2*/
	uint8_t	CL_C10UV			:1;					/**< \brief Bit 2: Clear C10UV in STDR2*/
	uint8_t	CL_C10OV			:1;					/**< \brief Bit 3: Clear C10OV in STDR2*/
	uint8_t	CL_C11UV			:1;					/**< \brief Bit 4: Clear C11UV in STDR2*/
	uint8_t	CL_C11OV			:1;					/**< \brief Bit 5: Clear C11OV in STDR2*/
	uint8_t	CL_C12UV			:1;					/**< \brief Bit 6: Clear C12UV in STDR2*/
	uint8_t	CL_C12OV			:1;					/**< \brief Bit 7: Clear C12OV in STDR2*/
}ADBMS6830_CL_STDR2_s;

/**
 *  @brief Union for CL_STDR2
 */
typedef union
{
	ADBMS6830_CL_STDR2_s B;
	uint8_t U8;
}ADBMS6830_CL_STDR2_T;

/**
 * @brief Structure that is used to clear the OV/UV faults from Status D register STDR3
 * */
typedef struct
{
	uint8_t	CL_C13UV			:1;					/**< \brief Bit 0: Clear C13UV in STDR3*/
	uint8_t	CL_C13OV			:1;					/**< \brief Bit 1: Clear C13OV in STDR3*/
	uint8_t	CL_C14UV			:1;					/**< \brief Bit 2: Clear C14UV in STDR3*/
	uint8_t	CL_C14OV			:1;					/**< \brief Bit 3: Clear C14OV in STDR3*/
	uint8_t	CL_C15UV			:1;					/**< \brief Bit 4: Clear C15UV in STDR3*/
	uint8_t	CL_C15OV			:1;					/**< \brief Bit 5: Clear C15OV in STDR3*/
	uint8_t	CL_C16UV			:1;					/**< \brief Bit 6: Clear C16UV in STDR3*/
	uint8_t	CL_C16OV			:1;					/**< \brief Bit 7: Clear C16OV in STDR3*/
}ADBMS6830_CL_STDR3_s;

/**
 *  @brief Union for CL_STDR3
 */
typedef union
{
	ADBMS6830_CL_STDR3_s B;
	uint8_t U8;
}ADBMS6830_CL_STDR3_T;

/**
 *
 * @brief Structure that is used to clear the ov/uv faults from Status d register
 *
 * */
typedef struct
{
	ADBMS6830_CL_STDR0_T CL_STDR0;						/**< \brief Clear fault flags for STDR0*/
	ADBMS6830_CL_STDR1_T CL_STDR1;						/**< \brief Clear fault flags for STDR1*/
	ADBMS6830_CL_STDR2_T CL_STDR2;						/**< \brief Clear fault flags for STDR2*/
	ADBMS6830_CL_STDR3_T CL_STDR3;						/**< \brief Clear fault flags for STDR3*/
	uint8_t 			 CL_STDR4;						/**< \brief reserved*/
	uint8_t				 CL_STDR5;						/**< \brief reserved*/
} ADBMS6830_CL_OVUV_t;

/**
 * @brief Structure that contains all the data registers
 * */
typedef struct
{
	uint8_t				command_Counter;			/**< \brief upper bits from PEC0 on a read.*/
	ADBMS6830_CFGA_t 	CFGA;						/**< \brief Configuration Group A Registers */
	ADBMS6830_CFGB_t	CFGB;						/**< \brief Configuration Group B Registers */
	ADBMS6830_RDCV_t	RDCV;						/**< \brief Cell Voltage Registers */
	ADBMS6830_RDACV_t 	RDACV;						/**< \brief Averaged Cell Voltage Registers */
	ADBMS6830_RDFCV_t	RDFCV;						/**< \brief Filtered Cell Voltage Registers */
	ADBMS6830_RDSV_t	RDSV;						/**< \brief S-Voltage Registers */
	ADBMS6830_RDAUX_t	RDAUX;						/**< \brief Auxiliary Registers */
	ADBMS6830_RDRAX_t   RDRAX;						/**< \brief Redundant Auxiliary Registers */
	ADBMS6830_STAT_t    RDSTAT;						/**< \brief Status Registers */
	ADBMS6830_COMM_t    RDCOMM;						/**< \brief COMM registers */
	ADBMS6830_PWMA_t	PWMA;						/**< \brief PWMA registers */
	ADBMS6830_PWMB_t	PWMB;						/**< \brief PWMB registers */
	ADBMS6830_RDRR_t	RDRR;						/**< \brief Retention registers*/
	ADBMS6830_RDSID_t	RDSID;						/**< \brief Serial ID registers*/
	ADBMS6830_CL_FLT_t	CLR_FLT;					/**< \brief Clear fault flags*/
	ADBMS6830_CL_OVUV_t CLR_OVUV;					/**< \brief Clear OV/UV fault*/
}ADBMS6830_REGISTERS_t;

/**
 * @brief Structure that contains all the fault bits
 * */
typedef struct
{
	uint32_t	CvsS_Mismatch		: 1;			/**< \brief Bit  0: C-ADC vs S-ADC miss match error has occurred */
	uint32_t  	VAOV				: 1;			/**< \brief Bit  1: 5V Analog rail over voltage  */
	uint32_t  	VAUV				: 1;			/**< \brief Bit  2: 5V Analog rail under voltage  */
	uint32_t  	VDOV				: 1;			/**< \brief Bit  3: 3V Digital rail over voltage  */
	uint32_t  	VDUV				: 1;			/**< \brief Bit  4: 3V Digital rail under voltage  */
	uint32_t  	CED					: 1;			/**< \brief Bit  5: C trim error detection  */
	uint32_t  	CMED				: 1;			/**< \brief Bit  6: C trim multiple error detection  */
	uint32_t  	SED					: 1;			/**< \brief Bit  7: S trim error detection  */
	uint32_t  	SMED				: 1;			/**< \brief Bit  8: S trim multiple error detection  */
	uint32_t  	VDEL				: 1;			/**< \brief Bit  9: Supply rail latent detection  */
	uint32_t  	VDE					: 1;			/**< \brief Bit 10: Supply Rail delta > 0.5V  */
	uint32_t  	COMP				: 1;			/**< \brief Bit 11: CvsS comparison Latent Fault  */
	uint32_t  	SPIFLT				: 1;			/**< \brief Bit 12: mismatch between redundant SPI slave outputs  */
	uint32_t  	THSD				: 1;			/**< \brief Bit 13: Thermal shutdown status  */
	uint32_t  	OSSCHK				: 1;			/**< \brief Bit 14: Out of range oscillator count detected during ADC operation  */
	uint32_t  	CxOV				: 1;			/**< \brief Bit 15: Cell overvoltage detected  */
	uint32_t  	CxUV				: 1;			/**< \brief Bit 16: Cell undervoltage detected  */
	uint32_t	reserved			:15;			/**< \brief Bit 17-31: reserved*/
}ADBMS6830_FAULTS_s;

/**
 * @brief Union that contains all the fault bits
 * */
typedef union
{
	ADBMS6830_FAULTS_s B;
	uint32_t 	U32;
}ADBMS6830_FAULTS_t;

typedef struct
{
	bool	 				isInitialized;
	ADBMS6830_FAULTS_t 		faults[ADBMS6830_NUMBER_OF_NODES];
	RAW_DATA_t  			rawData[ADBMS6830_NUMBER_OF_NODES];
	ADBMS6830_REGISTERS_t 	reg[ADBMS6830_NUMBER_OF_NODES];
}ADBMS6830_FUELCELL_INFO_t;

typedef struct
{
	uint32_t SM_STATC_FAULT 	:1;				/**< \brief StatC fault detected*/
	uint32_t SM_VCELL_OOR		:1;				/**< \brief Bit 1: Cell voltage out of range*/
	uint32_t SM_FREEZE_SEQ		:1;				/**< \brief Bit 2: No clock and freeze detector*/
	uint32_t SM_TRIGGER_ADX		:1;				/**< \brief Bit 3: Failed to trigger ADX and ADX2*/

	uint32_t SM_TRIGGER_ADSV	:1;				/**< \brief Bit 4: Failed to trigger ADSV*/
	uint32_t SM_CELL_OWD		:1; 			/**< \brief Bit 5: Failed open wire detection*/

}SM_FAULT_s;

typedef union
{
	SM_FAULT_s 	B;
	uint32_t	U32;
}ADBMS6830_SM_FAULT_t;
/*----- Private Functions Prototypes ----------------------------------------------------*/
static uint64_t adbms6830_Micros(void);
static void adbms6830_wakeup(bool force);
static bool adbms6830_ReadSID(void);
static bool adbms6830_read( uint16_t addr, uint8_t *pRxBuff, uint16_t rd_len);
static bool adbms6830_muteDischarge(void);
extern bool adbms6830_SNAP(void);
extern bool adbms6830_UNSNAP(void);
static bool adbms6830_VerifyFaultStatC(void);
extern ADBMS6830_FUELCELL_INFO_t adbms6830Info;
extern ADBMS6830_SM_FAULT_t adbms6830Faults;

/***************************** Source File Types ******************************/
extern bool adbms6830_write(uint16_t addr, uint8_t* pData, uint16_t len );
extern bool adbms6830_Config(void);
extern bool adbms6830_ReadCFGA(void);
extern bool adbms6830_ReadCFGB(void);
extern uint8_t adbms6830_ReadStatA(void);
extern uint8_t adbms6830_ReadStatB(void);
extern uint8_t adbms6830_ReadStatC(void);
extern uint8_t adbms6830_ReadStatD(void);
extern uint8_t adbms6830_ReadStatE(void);
extern uint8_t adbms6830_ReadCVA(void);
extern uint8_t adbms6830_ReadCVB(void);
extern uint8_t adbms6830_ReadCVC(void);
extern uint8_t adbms6830_ReadCVD(void);
extern uint8_t adbms6830_ReadCVE(void);
extern uint8_t adbms6830_ReadCVF(void);
extern uint8_t adbms6830_ReadACVA(void);
extern uint8_t adbms6830_ReadACVB(void);
extern uint8_t adbms6830_ReadACVC(void);
extern uint8_t adbms6830_ReadACVD(void);
extern uint8_t adbms6830_ReadACVE(void);
extern uint8_t adbms6830_ReadACVF(void);
extern uint8_t adbms6830_ReadFCA(void);
extern uint8_t adbms6830_ReadFCB(void);
extern uint8_t adbms6830_ReadFCC(void);
extern uint8_t adbms6830_ReadFCD(void);
extern uint8_t adbms6830_ReadFCE(void);
extern uint8_t adbms6830_ReadFCF(void);
extern uint8_t adbms6830_ReadSVA(void);
extern uint8_t adbms6830_ReadSVB(void);
extern uint8_t adbms6830_ReadSVC(void);
extern uint8_t adbms6830_ReadSVD(void);
extern uint8_t adbms6830_ReadSVE(void);
extern uint8_t adbms6830_ReadSVF(void);
extern bool adbms6830_ReadAUXA(void);
extern bool adbms6830_ReadAUXB(void);
extern bool adbms6830_ReadAUXC(void);
extern bool adbms6830_ReadAUXD(void);
extern bool adbms6830_ReadRAXA(void);
extern bool adbms6830_ReadRAXB(void);
extern bool adbms6830_ReadRAXC(void);
extern bool adbms6830_ReadRAXD(void);
extern bool adbms6830_WriteCFGA(void);
extern bool adbms6830_WriteCFGB(void);
extern bool adbms6830_ReadCOMM(void);
extern bool adbms6830_ReadPWMA(void);
extern bool adbms6830_ReadPWMB(void);
extern bool adbms6830_ReadRR(void);
extern bool adbms6830_WriteCOMM(void);
extern bool adbms6830_WritePWMA(void);
extern bool adbms6830_WritePWMB(void);
extern bool adbms6830_WriteRR(void);
extern bool adbms6830_clearFaultFlags(void);
extern bool adbms6830_clearOVUV(void);
extern bool adbms6830_ConfigureOVUVthresholds(void);
extern bool adbms6830_TrigAuxMeasurement(void);
extern bool adbms6830_checkStatC_Faults(void);
extern bool adbms6830_checkStatD_Faults(void);
#endif /* ADBMS6830_H */
