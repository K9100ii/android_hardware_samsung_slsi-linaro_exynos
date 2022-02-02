/*----------------------------------------------------------------------------------------------
*
* This file is ArcSoft's property. It contains ArcSoft's trade secret, proprietary and
* confidential information.
*
* The information and code contained in this file is only for authorized ArcSoft employees
* to design, create, modify, or review.
*
* DO NOT DISTRIBUTE, DO NOT DUPLICATE OR TRANSMIT IN ANY FORM WITHOUT PROPER AUTHORIZATION.
*
* If you are not an intended recipient of this file, you must not copy, distribute, modify,
* or take any action in reliance on it.
*
* If you have received this file in error, please immediately notify ArcSoft and
* permanently delete the original and any copy of any file and any printout thereof.
*
*-------------------------------------------------------------------------------------------------*/

#ifndef __ARCSOFT_DUALCAM_CALIBRATION_H__20161110_
#define __ARCSOFT_DUALCAM_CALIBRATION_H__20161110_

#include "amcomdef.h"
#include "asvloffscreen.h"
#include "merror.h"
#include "ammem.h"

#ifdef DUALCAMCALIBRATIONDLL_EXPORTS
#define ARC_DCCAL_API __declspec(dllexport)
#else
#define ARC_DCCAL_API
#endif

#ifdef __cplusplus
extern "C" {
#endif


#define ARC_DCCAL_ERROR_BASE 0xA000
#define ARC_DCCAL_NOT_ENOUGH_CHESSBOARD 			(ARC_DCCAL_ERROR_BASE+1)
#define ARC_DCCAL_CHESSBOARD_OVERLAPED 				(ARC_DCCAL_ERROR_BASE+2)
#define ARC_DCCAL_CAL_WITH_LARGE_ERROR 				(ARC_DCCAL_ERROR_BASE+3)
#define ARC_DCCAL_INPUT_ERROR 						(ARC_DCCAL_ERROR_BASE+5)
#define ARC_DCCAL_UNEQUAL_IMAGE_NUMBERS 			(ARC_DCCAL_ERROR_BASE+7)
#define ARC_DCCAL_LARGE_ERROR 						(ARC_DCCAL_ERROR_BASE+9)
#define ARC_DCCAL_INVALID_IMAGE_TYPE 				(ARC_DCCAL_ERROR_BASE + 11)
#define ARC_DCCAL_INCORRECT_INPUT_ORDER 			(ARC_DCCAL_ERROR_BASE + 12)



#define ARC_DCCAL_RET_BASE							0XB000
#define ARC_DCCAL_RET_ROTATION_X					(ARC_DCCAL_RET_BASE+1)
#define ARC_DCCAL_RET_ROTATION_Y					(ARC_DCCAL_RET_BASE+2)
#define ARC_DCCAL_RET_ROTATION_Z					(ARC_DCCAL_RET_BASE+3)
#define ARC_DCCAL_RET_SHIFT_X					    (ARC_DCCAL_RET_BASE+4)
#define ARC_DCCAL_RET_SHIFT_Y					    (ARC_DCCAL_RET_BASE+5)

typedef struct _tagARC_DCCAL_INITPARAM
{
	MInt32 i32NumberOfImages;
	MInt32 i32ChessboardWidth;		//number of blocks in the direction of x axis
	MInt32 i32ChessboardHeight;		//number of blocks in the direction of y axis.
	MInt32 i32NumberOfChessboards;	//total number of chessboards in this situation.
	MDouble dbBlockSize; 			//physical size of block(mm)
}ARC_DCCAL_INITPARAM, *LPARC_DCCAL_INITPARAM;



/** @brief Get Version information
*
* @return		    The version information
*/
ARC_DCCAL_API const MPBASE_Version *ARC_DCCAL_GetVersion();


/** @brief Initialize the dualcam calibration engine
*
* @param[in/out]	phHandle	pointer of the handle of dualcam calibration engine
* @param[in]		pParam
* @return			MOK: everything is ok
*/
ARC_DCCAL_API MRESULT ARC_DCCAL_Init(
		MHandle* 			phHandle,
		LPARC_DCCAL_INITPARAM pParam
		);

/** @brief Uninitialize the dualcam calibration engine
*
* @param[in/out]    hHandle	pointer of the handle of dualcam calibration engine
* @return			if MOK, everything is ok
*/
ARC_DCCAL_API MRESULT ARC_DCCAL_Uninit(MHandle* phHandle);


/** @brief
* @param[in]	hHandle			handle of dualcam calibration engine
* @param[in]		pSrcImgL
* @param[in]		pSrcImgR
* @param[out]		pCalData
* @return			MOK: everything is ok
*/
#define ARC_DCCAL_CALDATA_LEN 2048
ARC_DCCAL_API MRESULT ARC_DCCAL_Process(
		MHandle					hHandle,
		LPASVLOFFSCREEN			pSrcImgL,
		LPASVLOFFSCREEN			pSrcImgR,
		MByte*					pCalData
		);

/** @brief
* @param[in]	hHandle			handle of dualcam calibration engine
* @param[in]		i32Type
* @param[out]		pElement
* @return			MOK: everything is ok
*/
ARC_DCCAL_API MRESULT  ARC_DCCAL_GetCalDataElement(
		MHandle		hHandle,
		MInt32 		i32Type,
		MDouble* 	pElement
		);

/** @brief
* @param[in]		i32Type
* @param[in]		pCalData        2k Bytes. Calibration Data.
* @param[out]		pElement
* @return			MOK: everything is ok
*/
ARC_DCCAL_API MRESULT  ARC_DCCAL_GetCalDataElementFromCalData(
		MInt32 		i32Type,
		MByte*      pCalData,
		MDouble* 	pElement
		);


#ifdef __cplusplus
}
#endif

#endif //__ARCSOFT_DUALCAM_CALIBRATION_H__20161110_


