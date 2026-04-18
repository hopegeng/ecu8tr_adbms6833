/*
 * version.c
 *
 *  Created on: Dec 2024
 *      Author: rgeng
 */


/*----- Includes -------------------------------------------------------------*/
/* C includes */
#include <stdint.h>
#include <string.h>
/* IFX includes */
#include "Cpu/Std/Platform_Types.h"
#include "Cpu/Std/Ifx_Types.h"
/* Application Includes */
#include "version.h"

extern const VERSION_t gethDrvVersion;	/**< \brief NC-DoCAN API version  */

/*----- Protected Variables --------------------------------------------------*/


/*----- Externals ------------------------------------------------------------*/


/*----- Macros ---------------------------------------------------------------*/


/*----- Prototypes -----------------------------------------------------------*/


/*----- Member variables -----------------------------------------------------*/


/*----- Constant members -----------------------------------------------------*/

#include <FreeRTOS.h>
#include <task.h>


VERSION_t osVer =
{
	.ID = (0u),
	.MAJOR = tskKERNEL_VERSION_MAJOR,
	.MINOR = tskKERNEL_VERSION_MINOR,
	.INC = tskKERNEL_VERSION_BUILD,
	.pStr = "os"
};

const VERSION_t* versionMainifest[] __align(4) =
{
	//&mainVersion,
	//&gethDrvVersion,
	&osVer,
	NULL		/* Table terminator */
};


/*----- Private Functions ----------------------------------------------------*/


/*----- Protected Functions --------------------------------------------------*/


/*----- Public Functions -----------------------------------------------------*/
/**
 *******************************************************************************
 *
 * @author  JW
 * @date    Jan2002 (created)
 * @brief   version_Get()
 * 			Central table to store version.
 *
 * @param   pVer of type VERSION_t*.
 * 			Pointer to buffer to receive version.
 * @retval  int32_t.  Result:
 * 			0 = Version located and buffer contains version.
 * 			-1 = failure, could not locate version ID
 * 			-2 = failure, input buffer not valid.
 *
 ******************************************************************************/
int32_t version_Get(VERSION_t* pVer)
{
	int32_t i32Rtn = -1;


	if(NULL != pVer)
	{
		VERSION_t** pTable = ((VERSION_t**)versionMainifest);


		/* Initialize to UNKNOWN */
		pVer->MAJOR = (0x4u);
		pVer->MINOR = (0x3FFu);
		pVer->INC = (0x3FFu);

		for(uint16_t u16Index = (0u); NULL != pTable[u16Index]; u16Index++)
		{
			VERSION_t* pEntry = ((VERSION_t*)pTable[u16Index]);
			if((NULL != pVer->pStr) && (NULL != pEntry->pStr))
			{
				size_t len = strlen(pEntry->pStr);


				if(0 == memcmp(pEntry->pStr, pVer->pStr, len))
				{
					pVer->MAJOR = pEntry->MAJOR;
					pVer->MINOR = pEntry->MINOR;
					pVer->INC = pEntry->INC;
					i32Rtn = 0;
					break;
				}
			}
		}
	}
	else
	{
		i32Rtn = -2;
	}

	return(i32Rtn);
}/* END version_Get()  */

