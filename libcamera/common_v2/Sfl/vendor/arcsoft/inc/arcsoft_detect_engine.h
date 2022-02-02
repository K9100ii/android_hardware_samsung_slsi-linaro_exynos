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

#ifndef _ARCSOFT_DETECT_ENGINE_H_
#define _ARCSOFT_DETECT_ENGINE_H_
#include "amcomdef.h"
#include "asvloffscreen.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_PATH               256
#define MAX_FACE_NUM           10
#define MAX_PRIVIEW_FACE_NUM   10
#define MAX_CAPTURE_FACE_NUM   10



	/* Defines for result*/
#define MERR_FACE_BEAUTIFY_SUCCEED  0
#define MERR_INVALID_INPUT		   -2
#define MERR_USER_ABORT			   -3
#define MERR_IMAGE_FORMAT		  -101
#define MERR_IMAGE_SIZE_UNMATCH	  -102
#define MERR_ALLOC_MEM_FAIL		  -201
#define MERR_AHR_NULLPTR			  -301
#define MERR_AHR_RECREATE		      -302
#define MERR_AHR_SIZEOUTRANGE         -303
#define MERR_AHR_EXPIRED			  -304
#define MERR_NOFACE_INPUT		  -903 


#define MERR_FACEROLL			  -1412 
#define MERR_MODEL_FORMAT		  -1502
#define MERR_NO_MODEL			  -1506
#define MERR_MODEL_ALL_SKIN		  -1509	
#define MERR_PROCESS_MODEL		  -1600
#define MERR_NO_DATA			  -1601
#define MERR_ENABLE_FEATURES	  -1602

#define	MERR_CALLBACK_USER_ABORT	0x1
#define MERR_FACE_TOOSMALL          0x7000
#define MERR_CONFIG_MISMATCH	    0x8000
#define MERR_ENGINE_NONZERO			0x9000


#define ARCSOFT_FEATURE_FACEBEAUTIFY 0x1
#define ARCSOFT_FEATURE_SLIM_FACE    0x10
#define ARCSOFT_FEATURE_SKIN_BRIGHT  0x20
#define ARCSOFT_FEATURE_EYE_ENLARGE  0x80

#define ARCSOFT_SKIN_BASE_MODE   0x0
#define ARCSOFT_FACE_BASE_MODE   0x1

typedef struct
{
    MInt32  iFeatureNeeded;
    MInt32  iPreviewMaxFaceSupport; /*value 1-3 */
    MInt32  iCaptureMaxFaceSupport; /*value 1-10 */
    MInt32  iSkinSoftenLevel;/*value 0-100 */
    MInt32  iSkinBrightLevel;/*value 0-100 */
    MInt32  iSlenderFaceLevel;/*value 0-100 */
    MInt32  iEyeEnlargmentLevel;/*value 0-100 */
    MInt32	iMode;
    MUInt32	u32PixelArrayFormat;
    MInt32	i32Width;
    MInt32	i32Height;
}ADE_PARAMETERS, *LPADE_PARAMETERS;


typedef struct
{
    /*****FACE BEAUTY*******/
	ASVLOFFSCREEN			stBeautyImg; 	        /* The image data that after beautify*/
	MInt32        			lBeautyRes; 	        /* The beautify return value,0 mean success, otherwise failed */

	/*****FACE INFO*******/
    MRECT       prtFaces[MAX_FACE_NUM];             /* The bounding box of face*/
    MInt32      lFaceNum;                           /* Number of faces detected*/
	
}ADE_PROCESS_RESULT, *LPADE_PROCESS_RESULT;


typedef struct
{
    MInt32       iSkinSoftenLevel;/* The beautify lever between 0-100 */
    MInt32       iSkinBrightLevel;/*value 0-100 */
    MInt32       iSlenderFaceLevel;/*value 0-100 */
    MInt32  	 iEyeEnlargmentLevel;/*value 0-100 */
    MInt32       iFeatureNeeded;  /* Dynamic Open/Close feature     */
}ADE_PROPERTY, *LPADE_PROPERTY;

typedef struct
{
    MTChar szBuildDate[MAX_PATH];
    MTChar szVersion[MAX_PATH];
    MTChar szCopyRight[MAX_PATH];
}ADE_Version, *LPADE_Version;

MRESULT ArcsoftDetectEngine_Init(				                /* return MOK if success, otherwise fail*/
                                 MHandle			*pEngine,	/* [out]  Pointer pointing to a  detection engine*/
                                 LPADE_PARAMETERS	pParameter	/* [in]  init parameters*/
							 );

MRESULT ArcsoftDetectEngine_Process(				            /* return MOK if success, otherwise fail*/
                                MHandle			pEngine,		/* [in]  Pointer pointing to an ENGINE structure*/
                                LPASVLOFFSCREEN	pImgSrc,		/* [in]  The original image data*/
                                LPADE_PROCESS_RESULT    pProcessResult   /* [in & out] */
							 );

MRESULT ArcsoftDetectEngine_GetProperty(                            /* return MOK if success, otherwise fail*/
                                MHandle         pEngine,        /* [in]  Pointer pointing to an ENGINE structure*/
                                LPADE_PROPERTY  pProperty       /* [in & out]  Get current engine property */
                             );

MRESULT ArcsoftDetectEngine_SetProperty(				            /* return MOK if success, otherwise fail*/
                                MHandle			pEngine,		/* [in]  Pointer pointing to an ENGINE structure*/
                                LPADE_PROPERTY  pProperty       /* [in]  Dynamic change property */
							 );

MRESULT ArcsoftDetectEngine_BeautyForCapture(				    /* return MOK if success, otherwise fail*/
                               MHandle			pEngine,		/* [in]  Pointer pointing to an ENGINE structure*/
                               LPASVLOFFSCREEN	pImgSrc,		/* [in]  The original image data*/
                               LPASVLOFFSCREEN  pImgDst         /* [out] The image data after beauty*/
							 );

MRESULT ArcsoftDetectEngine_UnInit(				        /* return MOK if success, otherwise fail*/
								 MHandle	pEngine		/* [in]  Pointer pointing to an ENGINE structure*/
							 );

const ADE_Version* ArcsoftDetectEngine_GetVersion(MHandle pEngine);

#ifdef __cplusplus
}
#endif

#endif//_ARCSOFT_DETECT_ENGINE_H_
