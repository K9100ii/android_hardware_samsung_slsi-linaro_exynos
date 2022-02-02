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

#ifndef __ARCSOFT_DUALCAM_VERIFICATION_H__20161110_
#define __ARCSOFT_DUALCAM_VERIFICATION_H__20161110_

#include "amcomdef.h"
#include "asvloffscreen.h"
#include "merror.h"
#include "ammem.h"

#ifdef DUALCAMVERIFICATIONDLL_EXPORTS
#define ARC_DCVF_API __declspec(dllexport)
#else
#define ARC_DCVF_API
#endif

#ifdef __cplusplus
extern "C" {
#endif


#define ARC_DCVF_ERROR_BASE 0xA000
#define ARC_DCVF_NOT_ENOUGH_CHESSBOARD 			(ARC_DCVF_ERROR_BASE+1)
#define ARC_DCVF_CHESSBOARD_OVERLAPED 			(ARC_DCVF_ERROR_BASE+2)
#define ARC_DCVF_INPUT_ERROR 					(ARC_DCVF_ERROR_BASE+5)
#define ARC_DCVF_UNEQUAL_IMAGE_NUMBERS 			(ARC_DCVF_ERROR_BASE+7)
#define ARC_DCVF_FAIL_VERIFICATION              (ARC_DCVF_ERROR_BASE+8)
#define ARC_DCVF_LARGE_ERROR 					(ARC_DCVF_ERROR_BASE+9)
#define ARC_DCVF_INVALID_IMAGE_TYPE 			(ARC_DCVF_ERROR_BASE + 11)
#define ARC_DCVF_BLURIMAGE 			            (ARC_DCVF_ERROR_BASE + 12)


typedef struct _tagARC_DCVF_INITPARAM
{
	MInt32 i32ChessboardWidth;		//number of blocks in the direction of x axis
	MInt32 i32ChessboardHeight;		//number of blocks in the direction of y axis.
}ARC_DCVF_INITPARAM, *LPARC_DCVF_INITPARAM;

typedef struct _tagARC_DCVF_RESULT
{
	MDouble  AvgError;		//average error for shift of pixel
	MDouble  MaxError;		//max error for shift of pixel
	MDouble  ErrorRange;    //error range
}ARC_DCVF_RESULT, *LPARC_DCVF_RESULT;

/** @brief Get Version information
*
* @return		    The version information
*/
ARC_DCVF_API const MPBASE_Version *ARC_DCVF_GetVersion();


/** @brief Initialize the dualcam verificaiton engine
*
* @param[in/out]	phHandle	pointer of the handle of dualcam verificaiton engine
* @param[in]		pParam
* @return			MOK: everything is ok
*/
ARC_DCVF_API MRESULT ARC_DCVF_Init(
		MHandle* 			phHandle,
		LPARC_DCVF_INITPARAM pParam
		);

/** @brief Uninitialize the dualcam verificaiton engine
*
* @param[in/out]    hHandle	pointer of the handle of dualcam verificaiton engine
* @return			if MOK, everything is ok
*/
ARC_DCVF_API MRESULT ARC_DCVF_Uninit(MHandle* phHandle);


/** @brief
* @param[in/out]	hHandle			handle of dualcam verificaiton engine
* @param[in]		pSrcImgL
* @param[in]		pSrcImgR
* @param[in]		pCalData
* @param[out]		pVfResult
* @return			MOK: everything is ok
*/
#define ARC_DCVF_CALDATA_LEN 2048
ARC_DCVF_API MRESULT ARC_DCVF_Process(
		MHandle					hHandle,
		LPASVLOFFSCREEN			pSrcImgL,
		LPASVLOFFSCREEN			pSrcImgR,
		MByte*					pCalData,
		LPARC_DCVF_RESULT       pVfResult
		);

#ifdef __cplusplus
}
#endif

#endif //__ARCSOFT_DUALCAM_VERIFICATION_H__20161110_


