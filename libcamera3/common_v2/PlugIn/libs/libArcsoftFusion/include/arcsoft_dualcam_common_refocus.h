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

#ifndef DUALCAMREFOCUS_INC_ARCSOFT_DUALCAM_COMMON_REFOCUS_H_
#define DUALCAMREFOCUS_INC_ARCSOFT_DUALCAM_COMMON_REFOCUS_H_

#ifdef ARC_DCIRDLL_EXPORTS
#ifdef PLATFORM_LINUX
#define ARCDCOOM_API __attribute__((visibility ("default")))
#else
#define ARCDCOOM_API __declspec(dllexport)
#endif
#else
#define ARCDCOOM_API
#endif
#ifdef __cplusplus
extern "C" {
#endif

// The face orientation
#define ARC_DCR_FOC_0				0x1		// 0 degree
#define ARC_DCR_FOC_90				0x2		// 90 degree
#define ARC_DCR_FOC_270				0x3		// 270 degree
#define ARC_DCR_FOC_180				0x4		// 180 degree
#define ARC_DCR_FOC_30				0x5		// 30 degree
#define ARC_DCR_FOC_60				0x6		// 60 degree
#define ARC_DCR_FOC_120				0x7		// 120 degree
#define ARC_DCR_FOC_150				0x8		// 150 degree
#define ARC_DCR_FOC_210				0x9		// 210 degree
#define ARC_DCR_FOC_240				0xa		// 240 degree
#define ARC_DCR_FOC_300				0xb		// 300 degree
#define ARC_DCR_FOC_330				0xc		// 330 degree

typedef struct _tagARC_DC_CALDATA
{
	MVoid		*pCalibData;					// [in]  The data of camera's calibration
	MInt32		i32CalibDataSize;				// [in]  The size of camera's calibration data

}ARC_DC_CALDATA, *LPARC_DC_CALDATA;


typedef struct _tagARC_REFOCUSCAMERAIMAGE_PARAM
{
	MInt32				i32MainWidth_CropNoScale;		// [in]  Width of main image without scale.
	MInt32				i32MainHeight_CropNoScale;	    // [in]  Height of main image without scale.
	MInt32				i32AuxWidth_CropNoScale;	    // [in]  Width of auxiliary image without scale.
	MInt32				i32AuxHeight_CropNoScale;  	    // [in]  Height of auxiliary image without scale.
}ARC_REFOCUSCAMERAIMAGE_PARAM, *LPARC_REFOCUSCAMERAIMAGE_PARAM;


typedef struct _tagARC_DC_KERNALBIN
{
	MByte		*pKernelBindata;					// [in]  The data of Kernel bin
	MInt32		i32KernelBindataSize;				// [in]  The size of Kernel bin

}ARC_DC_KERNALBIN, *LPARC_DC_KERNALBIN;

ARCDCOOM_API MInt32 	ARC_BuildDeviceKernelBin(
		const MPChar fileName //[in] the file path for save kernel bin file
		);
#ifdef __cplusplus
}
#endif
#endif /* DUALCAMREFOCUS_INC_ARCSOFT_DUALCAM_COMMON_REFOCUS_H_ */
