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

#ifndef __AME_EXPOSURE_H__
#define __AME_EXPOSURE_H__

#include "amcomdef.h"
#include "merror.h"
#include "asvloffscreen.h"

#define AME_ERR_INVALID_LOW_EXPOSURED_IMAGE		(MERR_BASIC_BASE + 20)
#define AME_ERR_INVALID_OVER_EXPOSURED_IMAGE	(MERR_BASIC_BASE + 21)
#define AME_ERR_ALIGN_FAIL						(MERR_BASIC_BASE + 22)
#define AME_ERR_INVALID__EXPOSURE				(MERR_BASIC_BASE + 23)
#define AME_ERR_CONTAIN_GHOST				    (MERR_BASIC_BASE + 24)

#define AME_FRONT_CAMERA						0x100

typedef struct _tag_AME_pParam {
	MLong					lToneLength;		// [in]  Adjustable parameter for tone mapping, the range is 0~100.
	MLong					lBrightness;	    // [in]  Adjustable parameter for brightness, the range is -100~+100.)
	MLong					lToneSaturation;	// [in]  Adjustable parameter for saturation, the range is 0~+100.)
} AME_pParam , *LPAME_pParam ;

typedef struct _tag_AME_Userdata {
	MRECT					lCropRect;	 // [out] the crop rect
	MVoid*                  lreserved1;  // [in]  reserved data 1
	MVoid*					lreserved2;  // [in]  reserved data 2
} AME_Userdata , *LPAME_Userdata ;

#ifdef __cplusplus
extern "C" {
#endif

/************************************************************************
* This function is implemented by the caller, registered with
* any time-consuming processing functions, and will be called
* periodically during processing so the caller application can
* obtain the operation status (i.e., to draw a progress bar),
* as well as determine whether the operation should be canceled or not
************************************************************************/
typedef MRESULT (*AME_FNPROGRESS) (
	MLong					lProgress,			// The percentage of the current operation
	MLong					lStatus,			// The current status at the moment
	MVoid					*pParam				// Caller-defined data
);

MRESULT  AME_EXP_Init(
    MHandle					hMemMgr,			// The handle for memory manager
	MLong					lCameraMode,		// The model name for camera, reserve for camera customizing tuning.
    MHandle                 *phExposureEffect	// The handle for HDR
);

MRESULT  AME_EXP_Uninit(
	MHandle					hMemMgr,			// The handle for memory manager
    MHandle					hExposureEffect		// The handle for HDR
);

MRESULT  AME_GetImageEVShotInfo(
	MHandle					hMemMgr,				// The handle for memory manager
	LPASVLOFFSCREEN			pPreviewImg,			// input Preview data same ev as normal
	MInt32*					pEVArray,				// [out] the image shot by ev value (-6...+6, means -6/3...+6/3)
	MInt32*					pi32ShotImgNumber		// [out]  the number of shot image, it is 1, 2 or 3.
	);

MRESULT  AME_GetImageEVShotInfoWithRange(
	MHandle					hMemMgr,				// The handle for memory manager
	LPASVLOFFSCREEN			pPreviewImg,			// input Preview data same ev as normal
	MInt32					i32EVMin,				// [in] the min ev value, it is < 0
	MInt32					i32EVMax,				// [in] the max ev value, it is > 0
	MInt32					i32EVStep,				// [in] the step ev value, it is >= 1
	MInt32*					pEVArray,				// [out] the image shot by ev value ([i32EVMin,i32EVMax], means [i32EVMin/i32EVStep, i32EVMax/i32EVStep])
	MInt32*					pi32ShotImgNumber		// [out]  the number of shot image, it is 1, 2 or 3.
	);

/************************************************************************
 * The function used to creat hdr.
 ************************************************************************/

MRESULT AME_SetGhostSuppressionLevel(
	MHandle					hMemMgr,	        // [in] The handle for memory manager
	MHandle					hExpHandle,         // [in] The handle for HDR
	MInt32					level				// [in] Ghost detection level, value range: 0~10. 0: no ghost suppression. 10: strictest ghost suppression.
	);

MRESULT AME_ProcessFrames(
	MHandle					hMemMgr,	        // [in] The handle for memory manager
	MHandle					hExpHandle,         // [in] The handle for HDR
	LPASVLOFFSCREEN			ImgArray,           //input image data
	MLong					lIndex,				//input image number
	AME_FNPROGRESS			fnCallback,
	MVoid*					UserData            //user data, use when special case
	);

MRESULT AME_RunAlignHighDynamicRangeImage(
	MHandle					hMemMgr,	        // [in] The handle for memory manager
	MHandle					hExpHandle,         // [in] The handle for HDR
	LPASVLOFFSCREEN			pImgArray,          //input image data
	MLong					lImgNumber,			//input image number
	LPASVLOFFSCREEN			pResultImgArray,	// [out] the image data for hdr
	MLong					lResultImgNumber,	// [in]  the number of result image, it must be 1 or 2.
	LPAME_pParam			pParam,             // parmar of bright and tone
	MLong                   lPerformace,        // case 0 quality mode ;case 1 fast mode; case 2 use quality when normal and use fast when art;
	AME_FNPROGRESS			fnCallback,
	MVoid*					UserData            //user data, use when special case
	);
/************************************************************************
 * The function used to get version information of hdr effect library.
 ************************************************************************/
typedef struct
{
	MLong lCodebase;			// Codebase version number
	MLong lMajor;				// major version number
	MLong lMinor;				// minor version number
	MLong lBuild;				// Build version number, increasable only
	const MTChar *Version;		// version in string form
	const MTChar *BuildDate;	// latest build Date
	const MTChar *CopyRight;	// copyright
} AME_Version;
const AME_Version * AME_GetVersion();


#ifdef __cplusplus
}
#endif

#endif
