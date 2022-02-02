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
#ifndef _ARCSOFT_NIGHT_SHOT_H_
#define _ARCSOFT_NIGHT_SHOT_H_

#include "asvloffscreen.h"
#include "merror.h"

#ifdef NIGHTSHOTDLL_EXPORTS
#define NIGHTSHOT_API __declspec(dllexport)
#else
#define NIGHTSHOT_API
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
typedef MRESULT (*ANS_FNPROGRESS) (
	MLong		lProgress,				// The percentage of the current operation
	MLong		lStatus,				// The current status at the moment
	MVoid		*pParam					// Caller-defined data
);

/************************************************************************
* This function is used to get version information of library
************************************************************************/
typedef struct _tag_ANS_Version {
	MLong		lCodebase;	/* Codebase version number */
	MLong		lMajor;		/* Major version number */
	MLong		lMinor;		/* Minor version number*/
	MLong		lBuild;		/* Build version number, increasable only */
	const MChar *Version;	/* Version in string form */
	const MChar *BuildDate;	/* Latest build date */
	const MChar *CopyRight;	/* Copyrights */
} ANS_Version;

NIGHTSHOT_API MVoid ANS_GetVersion(ANS_Version *pVer);

/************************************************************************
* This function is used to get default parameters of library
************************************************************************/
typedef struct _tag_ANS_PARAM {
	MLong		lRefNum;
	MLong       lIntensity;
	MLong       lLightIntensity;
	MLong       lSaturation;
	MBool		bNeedSharpen;
} ANS_PARAM, *LPANS_PARAM;

NIGHTSHOT_API MRESULT ANS_GetDefaultParam(LPANS_PARAM pParam);

/************************************************************************
* The functions is used to perform image night shot
************************************************************************/
#define MAX_INPUT_IMAGES	8
typedef struct _tag_ANS_INPUTINFO {
	MLong				lImgNum;
	LPASVLOFFSCREEN		pImages[MAX_INPUT_IMAGES];
} ANS_INPUTINFO, *LPANS_INPUTINFO;

NIGHTSHOT_API MRESULT ANS_Init(				// return MOK if success, otherwise fail
	MHandle				hMemMgr,			// [in]  The memory manager
	MHandle				*phEnhancer			// [out] The algorithm engine will be initialized by this API
);

NIGHTSHOT_API MRESULT ANS_Uninit(			// return MOK if success, otherwise fail
	MHandle				*phEnhancer			// [in/out] The algorithm engine will be un-initialized by this API
);

NIGHTSHOT_API MRESULT ANS_Enhancement(		// return MOK if success, otherwise fail
	MHandle				hEnhancer,			// [in]  The algorithm engine
	LPANS_INPUTINFO		pSrcImgs,			// [in]  The offscreen of source images
	LPASVLOFFSCREEN		pDstImg,			// [out] The offscreen of result image
	LPANS_PARAM			pNSParam,			// [in]  The parameters for algorithm engine
	ANS_FNPROGRESS		fnCallback,			// [in]  The callback function 
	MVoid				*pParam				// [in]  Caller-specific data that will be passed into the callback function
);



#ifdef __cplusplus
}
#endif

#endif // _ARCSOFT_NIGHT_SHOT_H_
