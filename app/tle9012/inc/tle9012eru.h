/**
 ******************************************************************************
 * @file    tle9012eru.h
 * @author	RG
 * @version V0.1.0
 * @date    May2020
 * @brief	Supports the enable/disable functions related to the External
 * 			Request unit.
 *
 ******************************************************************************
 * @attention
 *
 * <h2><center> OnzerOS&trade; SYSTEM LEVEL OPERATIONAL SOFTWARE</center></h2>
 * <h2><center>&copy; COPYRIGHT 2020 Neutron Controls.</center></h2>
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS for
 * the sole purpose of a reference design, WITHOUT WARRANTIES OR CONDITIONS OF
 * ANY KIND, either express or implied.
 *
 * !DO NOT DISTRIBUTE!
 * For evaluation only.
 * Extended license agreement required for distribution and use!
 *
 ******************************************************************************
 */

#ifndef TLE_9012_ERU_H_
#define TLE_9012_ERU_H_


/*************************** Source File Constants ****************************/


/***************************** Source File Types ******************************/
typedef void (*TLE9012ERU_HANDLER)( void );

/****************************** Application APIs ******************************/
extern int32_t tle9012eru_enable(boolean bState );
extern void tle9012eru_setHandler( TLE9012ERU_HANDLER pFun );

#endif /* TLE_9012_ERU_H_ */

