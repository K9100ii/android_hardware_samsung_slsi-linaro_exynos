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

#ifndef ARCSOFT_DUALCAM_IMG_REFOCUS_DLL_H_
#define ARCSOFT_DUALCAM_IMG_REFOCUS_DLL_H_

#include "ammem.h"
#include "arcsoft_dualcam_image_refocus.h"
#ifdef __cplusplus
extern "C" {
#endif

//the dll export function
#define ARC_DCIR_GetVersion_OBJ_EXPORT           "ARC_DCIR_GetVersion"
#define ARC_DCIR_GetDefaultParam_OBJ_EXPORT      "ARC_DCIR_GetDefaultParam"
#define ARC_DCIR_Init_OBJ_EXPORT                 "ARC_DCIR_Init"
#define ARC_DCIR_Uninit_OBJ_EXPORT               "ARC_DCIR_Uninit"
#define ARC_DCIR_SetCameraImageInfo_OBJ_EXPORT   "ARC_DCIR_SetCameraImageInfo"
#define ARC_DCIR_SetCaliData_OBJ_EXPORT          "ARC_DCIR_SetCaliData"
#define ARC_DCIR_CalcDisparityData_OBJ_EXPORT    "ARC_DCIR_CalcDisparityData"
#define ARC_DCIR_GetDisparityDataSize_OBJ_EXPORT "ARC_DCIR_GetDisparityDataSize"
#define ARC_DCIR_GetDisparityData_OBJ_EXPORT     "ARC_DCIR_GetDisparityData"
#define ARC_DCIR_Process_OBJ_EXPORT              "ARC_DCIR_Process"
#define ARC_DCIR_BuildDeviceKernelBin_OBJ_EXPORT "ARC_BuildDeviceKernelBin"
#define ARC_DCIR_SetOCLKernel_OBJ_EXPORT         "ARC_DCIR_SetOCLKernel"
#define ARC_DCIR_Reset_OBJ_EXPORT                "ARC_DCIR_Reset"


typedef const MPBASE_Version * (*ARC_DCIR_GetVersion_Func)();

typedef MRESULT (*ARC_DCIR_GetDefaultParam_Func)(  // return MOK if success, otherwise fail
    LPARC_DCIR_PARAM pParam                         // [out] The default parameter of algorithm engine
    );

typedef MRESULT (*ARC_DCIR_Init_Func)(         // return MOK if success, otherwise fail
    MHandle                *phHandle,               // [out] The algorithm engine will be initialized by this API
    MInt32 nMode
    );

typedef MRESULT (*ARC_DCIR_Uninit_Func)(           // return MOK if success, otherwise fail
    MHandle                *phHandle                    // [in/out] The algorithm engine will be un-initialized by this API
    );

typedef MRESULT (*ARC_DCIR_SetCameraImageInfo_Func)(   // return MOK if success, otherwise fail
    MHandle                hHandle ,                            // [in]  The algorithm engine
    LPARC_REFOCUSCAMERAIMAGE_PARAM   pParam                     // [in]  Camera and image information
    );

typedef MRESULT (*ARC_DCIR_SetCaliData_Func)(            // return MOK if success, otherwise fail
    MHandle                hHandle ,                            // [in]  The algorithm engine
    LPARC_DC_CALDATA   pCaliData                                // [in]   Calibration Data
    );

typedef MRESULT (*ARC_DCIR_CalcDisparityData_Func)(    // return MOK if success, otherwise fail
    MHandle                hHandle,                             // [in]  The algorithm engine
    LPASVLOFFSCREEN        pLeftImg,                            // [in]  The off-screen of left image
    LPASVLOFFSCREEN        pRightImg,                           // [in]  The off-screen of right image
    LPARC_DCIR_PARAM    pDCIRParam                              // [in]  The parameters for algorithm engine
    );

typedef MRESULT (*ARC_DCIR_GetDisparityDataSize_Func)(// return MOK if success, otherwise fail
    MHandle                hHandle,                            // [in]  The algorithm engine
    MInt32                *pi32Size                            // [out] The size of disparity map
    );

typedef MRESULT (*ARC_DCIR_GetDisparityData_Func)(      // return MOK if success, otherwise fail
    MHandle                hHandle,                             // [in]  The algorithm engine
    MVoid                *pDisparityData                        // [out] The data of disparity map
    );

typedef MRESULT (*ARC_DCIR_Process_Func)(             // return MOK if success, otherwise fail
    MHandle                       hHandle,                  // [in]  The algorithm engine
    MVoid                        *pDisparityData,           // [in]  The data of disparity data
    MInt32                        i32DisparityDataSize,     // [in]  The size of disparity data
    LPASVLOFFSCREEN                pLeftImg,                // [in]  The off-screen of left image
    LPARC_DCIR_REFOCUS_PARAM       pRFParam,                // [in]  The parameter for refocusing image
    LPASVLOFFSCREEN                pDstImg                  // [out]  The off-screen of result image
    );

typedef MInt32 	(*ARC_DCIR_BuildDeviceKernelBin_Func)(
    const MPChar fileName //[in] the file path for save kernel bin file
    );

typedef MRESULT (*ARC_DCIR_SetOCLKernel_Func)(           // return MOK if success, otherwise fail
	MHandle	                  hEngine,              // [in]  The algorithm engine
	ARC_DC_KERNALBIN          *kernelBin            // [in]  The kernelBin of ocl
    );

typedef MRESULT (*ARC_DCIR_Reset_Func)(                  // return MOK if success, otherwise fail
	MHandle             hEngine                     // [in/out] The algorithm engine will be reset by this API
    );


//for processor dll handle struct
typedef struct _tag_bokeh_image_dll_export
{
    //the dll handle
    void*                               dlPtr;
    ARC_DCIR_GetVersion_Func            dlARC_DCIR_GetVersion_FuncPtr;
    ARC_DCIR_GetDefaultParam_Func       dlARC_DCIR_GetDefaultParam_FuncPtr;
    ARC_DCIR_Init_Func                  dlARC_DCIR_Init_FuncPtr;
    ARC_DCIR_Uninit_Func                dlARC_DCIR_Uninit_FuncPtr;
    ARC_DCIR_SetCameraImageInfo_Func    dlARC_DCIR_SetCameraImageInfo_FuncPtr;
    ARC_DCIR_SetCaliData_Func           dlARC_DCIR_SetCaliData_FuncPtr;
    ARC_DCIR_CalcDisparityData_Func     dlARC_DCIR_CalcDisparityData_FuncPtr;
    ARC_DCIR_GetDisparityDataSize_Func  dlARC_DCIR_GetDisparityDataSize_FuncPtr;
    ARC_DCIR_GetDisparityData_Func      dlARC_DCIR_GetDisparityData_FuncPtr;
    ARC_DCIR_Process_Func               dlARC_DCIR_Process_FuncPtr;
    ARC_DCIR_BuildDeviceKernelBin_Func  dlARC_DCIR_BuildDeviceKernelBin_FuncPtr;
    ARC_DCIR_SetOCLKernel_Func          dlARC_DCIR_SetOCLKernel_FuncPtr;
    ARC_DCIR_Reset_Func                 dlARC_DCIR_Reset_FuncPtr;
    
    _tag_bokeh_image_dll_export()
    {
        dlPtr                                   = NULL;
        dlARC_DCIR_GetVersion_FuncPtr           = NULL;
        dlARC_DCIR_GetDefaultParam_FuncPtr      = NULL;
        dlARC_DCIR_Init_FuncPtr                 = NULL;
        dlARC_DCIR_Uninit_FuncPtr               = NULL;
        dlARC_DCIR_SetCameraImageInfo_FuncPtr   = NULL;
        dlARC_DCIR_SetCaliData_FuncPtr          = NULL;
        dlARC_DCIR_CalcDisparityData_FuncPtr    = NULL;
        dlARC_DCIR_GetDisparityDataSize_FuncPtr = NULL;
        dlARC_DCIR_GetDisparityData_FuncPtr     = NULL;
        dlARC_DCIR_Process_FuncPtr              = NULL;
        dlARC_DCIR_BuildDeviceKernelBin_FuncPtr = NULL;
        dlARC_DCIR_SetOCLKernel_FuncPtr         = NULL;
        dlARC_DCIR_Reset_FuncPtr                = NULL;
    }
 
}bokeh_image_dll_export_t;



#ifdef __cplusplus
}
#endif

#endif /* ARCSOFT_DUALCAM_IMG_REFOCUS_DLL_H_ */
