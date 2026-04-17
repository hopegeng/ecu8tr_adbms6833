/**
 ******************************************************************************
 * @file    tle9012eru.c
 * @author	RG
 * @version V0.1.1
 * @date    May2021
 * @brief	External request unit.
 *
 ******************************************************************************
 * @attention
 *
 * <h2><center> OnzerOS&trade; SYSTEM LEVEL OPERATIONAL SOFTWARE</center></h2>
 * <h2><center>&copy; COPYRIGHT 2020 Neutron Controls</center></h2>
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * !DO NOT DISTRIBUTE!
 * For evaluation only.
 * Extended license agreement required for distribution and use!
 *
 ******************************************************************************
 */


/*----- Includes -------------------------------------------------------------*/
/* C includes */
#include <stdint.h>
#include <string.h>
/* IFX includes */
#include "Ifx_Types.h"
#include "Port/Io/IfxPort_Io.h"
#include "_PinMap/IfxPort_PinMap.h"
#include "_Reg/IfxPort_Reg.h"
#include "Scu/Std/IfxScuEru.h"
#include "_Reg/IfxSrc_reg.h"
/* Application includes */
#include <tle9012eru.h>


/*----- Externals ------------------------------------------------------------*/


/*----- Macros ---------------------------------------------------------------*/


#define TLE9015_ERRQ_LOC_OUT_ISR_PRIORITY				40

/*----- Declarations ---------------------------------------------------------*/


/*----- Prototypes -----------------------------------------------------------*/


/*----- Static members -------------------------------------------------------*/
__private0 __far static volatile boolean eruESR7IsActive = FALSE;

TLE9012ERU_HANDLER eruHandler = NULL;



/*----- Constant members -----------------------------------------------------*/


/*----- Private Functions ----------------------------------------------------*/
/**
 *******************************************************************************
 *
 * @author  RG
 * @date    May2021 (created)
 * @brief   tle9012eru_esr7ISR()
 *
 * @param   (void)
 * @retval  (void)
 *
 ******************************************************************************/
void __interrupt(TLE9015_ERRQ_LOC_OUT_ISR_PRIORITY) __vector_table(0) tle9012eru_esr7ISR(void)
{
	if( eruHandler != NULL )
	{
		eruHandler();
	}
}/* eru_vESR5isr() */


/*----- Protected Functions --------------------------------------------------*/


/*----- Public Functions -----------------------------------------------------*/
/**
 *******************************************************************************
 *
 * @author	RG
 * @date 	May2021 (created)
 * @brief	tle9012eru_enable()
 * 			Enables ESR7 to generates an interrupt upon a RE as reported by
 * 			the LS2.
 *
 * @param	NONE.
 * @retval	NONE.
 *
 ******************************************************************************/
int32_t tle9012eru_enable( boolean bState )
{
	int32_t i32Rtn = -1;
	uint32_t u32EruReg;
	void* pvEruReg = &u32EruReg;
	Ifx_SRC_SRCR isr;
	IfxCpu_ResourceCpu coreId = __mfcr(CPU_CORE_ID);

	if((TRUE == bState) && (FALSE == eruESR7IsActive))
	{
	    uint16 password = IfxScuWdt_getSafetyWatchdogPasswordInline();


	    IfxScuWdt_clearSafetyEndinitInline(password);
	    /* ERRQ_LOT_OUT: P20.09 = INPUT (tristate) */
		P20_IOCR8.B.PC9 = (0x00u);

		/* Configure the input ERU channel */
		u32EruReg = SCU_EICR3.U;
		((Ifx_SCU_EICR*)pvEruReg)->B.EXIS1 = (0u); 			/* ERS7: REQ1 (p20.09) --> IN70 */
		((Ifx_SCU_EICR*)pvEruReg)->B.LDEN1 = (1u);			/* Auto clear Level detection disabled */
		((Ifx_SCU_EICR*)pvEruReg)->B.FEN1 = (1u); 			/* Falling Edge Detect enabled */
		((Ifx_SCU_EICR*)pvEruReg)->B.REN1 = (0u);			/* Rising Edge Detect disabled */
		((Ifx_SCU_EICR*)pvEruReg)->B.EIEN1 = (1u);			/* Trigger event enabled */
		((Ifx_SCU_EICR*)pvEruReg)->B.INP1 = (0x00u);		/* route via OGU0 */
		SCU_EICR3.U = u32EruReg;



		isr.U = 0x0u;
		isr.B.SRPN = TLE9015_ERRQ_LOC_OUT_ISR_PRIORITY;
		isr.B.SRE	= 0x1u;
		isr.B.TOS = 0;
		SRC_SCUERU0.U = isr.U;

		/* Enable interrupt */
		u32EruReg = SCU_IGCR0.U;
		u32EruReg &= (0x0000FFFF);
		((Ifx_SCU_IGCR*)pvEruReg)->B.IGP0 = (1u);			/* IOUT2 is activated in response to a trigger event. The pattern is not considered. */
		SCU_IGCR0.U = u32EruReg;

		IfxScuWdt_setSafetyEndinitInline(password);

		eruESR7IsActive = TRUE;
		i32Rtn = 0;
	}
	else if((FALSE == bState) && (TRUE == eruESR7IsActive))
	{
		/* Disable the interrupt */
		SCU_IGCR0.U &= (0x0000FFFF);

		/* Disable up interrupt service node */
		MODULE_SRC.SCU.SCUERU[0].U = 0x0u;

		/* Reset the channel */
		SCU_EICR3.U &= (0x0000FFFFu);

		eruESR7IsActive = FALSE;
		i32Rtn = 0;
	}
	else
	{
		i32Rtn = 1;
	}

	return(i32Rtn);
}/* END tle9012eru_enable() */


void tle9012eru_setHandler( TLE9012ERU_HANDLER pFun )
{
	eruHandler = pFun;
}
