/*******************************************************************************
Copyright(c) ArcSoft, All right reserved.

This file is ArcSoft's property. It contains ArcSoft's trade secret, proprietary 
and confidential information. 

The information and code contained in this file is only for authorized ArcSoft 
employees to design, create, modify, or review.

DO NOT DISTRIBUTE, DO NOT DUPLICATE OR TRANSMIT IN ANY FORM WITHOUT PROPER 
AUTHORIZATION.

If you are not an intended recipient of this file, you must not copy, 
distribute, modify, or take any action in reliance on it. 

If you have received this file in error, please immediately notify ArcSoft and 
permanently delete the original and any copy of any file and any printout 
thereof.
*******************************************************************************/
#ifndef _ARCSOFT_ANTISHAKING_H_
#define _ARCSOFT_ANTISHAKING_H_

#include "asvloffscreen.h"
#include "merror.h"

#ifdef ASSFDLL_EXPORTS
#define ASSF_API __declspec(dllexport)
#else
#define ASSF_API
#endif

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
typedef MRESULT (*ASSF_FNPROGRESS) (
	MInt32		lProgress,				// The percentage of the current operation
	MInt32		lStatus,				// The current status at the moment
	MVoid		*pParam					// Caller-defined data
);

/************************************************************************
* This function is used to get version information of library
************************************************************************/
typedef struct _tag_ASSF_Version {
	MInt32		lCodebase;	/* Codebase version number */
	MInt32		lMajor;		/* Major version number */
	MInt32		lMinor;		/* Minor version number*/
	MInt32		lBuild;		/* Build version number, increasable only */
	const MChar *Version;	/* Version in string form */
	const MChar *BuildDate;	/* Latest build date */
	const MChar *CopyRight;	/* Copyrights */
} ASSF_Version;

ASSF_API MVoid ASSF_GetVersion(ASSF_Version *pVer);

/************************************************************************
* This function is used to get default parameters of library
************************************************************************/
typedef struct _tag_CAMERA_PARAM {
	MFloat           	fISO;
	MFloat          	fExpoTime;
} CAMERA_PARAM, *LPCAMERA_PARAM;

typedef struct _tag_ASSF_PARAM {
	MInt32			lIntensity;
	CAMERA_PARAM	camParam;
} ASSF_PARAM, *LPASSF_PARAM;

ASSF_API MRESULT ASSF_GetDefaultParam(LPASSF_PARAM pParam);

/************************************************************************
* The functions is used to perform algorithm
************************************************************************/
#define MAX_AS_PREVIEW_IMAGES	3

typedef struct _tag_ASSF_INPUTINFO {
	MInt32     			lImgNum;
	LPASVLOFFSCREEN		pImages[MAX_AS_PREVIEW_IMAGES];
} ASSF_INPUTINFO, *LPASSF_INPUTINFO;

ASSF_API MRESULT GetCaptureISO(LPCAMERA_PARAM pAutoCamParam,LPCAMERA_PARAM pCaptureCamParam,MFloat isoThreshold);

ASSF_API MRESULT ASSF_Init(					// return MOK if success, otherwise fail
	MHandle				hMemMgr,			// [in]  The memory manager
	MHandle				*phEnhancer			// [out] The algorithm engine will be initialized by this API
);

ASSF_API MRESULT ASSF_Uninit(				// return MOK if success, otherwise fail
	MHandle				*phEnhancer			// [in/out] The algorithm engine will be un-initialized by this API
);

ASSF_API MRESULT ASSF_Enhancement(			// return MOK if success, otherwise fail
	MHandle				hEnhancer,			// [in]  The algorithm engine
	LPASSF_INPUTINFO	pPreviewImgs,		// [in]  The offscreen of preview images
	LPASVLOFFSCREEN		pSrcImg,			// [in]  The offscreen of source images
	LPASVLOFFSCREEN		pDstImg,			// [out] The offscreen of result image
	LPASSF_PARAM		pASParam,			// [in]  The parameters for algorithm engine
	ASSF_FNPROGRESS		fnCallback,			// [in]  The callback function 
	MVoid				*pParam				// [in]  Caller-specific data that will be passed into the callback function
);

#ifdef __cplusplus
}
#endif

#endif // _ARCSOFT_ANTISHAKING_H_
