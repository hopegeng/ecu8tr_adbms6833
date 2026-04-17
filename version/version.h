/*
 * version.h
 *
 *  Created on: Dec 2024
 *      Author: rgeng
 */

#ifndef VERSION_H_
#define VERSION_H_


/*************************** Source File Constants ****************************/


/***************************** Source File Types ******************************/
typedef struct VERSION_s	/** \brief VERSIONS Buffer */
{
	uint32_t ID : 8;		/**< \brief [7:0] Manifest ID */
	uint32_t INC : 10;		/**< \brief [17:8] Application Incremental Build (r) */
	uint32_t MINOR : 10;	/**< \brief [27:18] Application Minor version (r) */
	uint32_t MAJOR : 4;		/**< \brief [31:28] Application Major version (r) */
	const char* pStr;		/**< \brief Version short form string (r) */
} VERSION_t;


/****************************** Application APIs ******************************/
extern int32_t version_Get(VERSION_t* pVer);


#endif /* VERSION_H_ */

