/*
 * Copyright@ Samsung Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#define LOG_TAG "ArcsoftFusionBokehCapture"
#include <cutils/properties.h>
#include <fcntl.h>
#include <dlfcn.h>

#include "ArcsoftFusionBokehCapture.h"
#include "ArcsoftCaliLib.h"

#define BOKEH_BLUR_INTENSITY 80
#define BOKEH_DUAL_CAM_DEGREE 0

#define __FOCUS_DEBUG__

void  *ArcsoftFusionBokehCapture::m_handle = NULL;
Mutex  ArcsoftFusionBokehCapture::m_handleLock;

ArcsoftFusionBokehCapture::ArcsoftFusionBokehCapture()
{
    m_pBokehDllDes = NULL;
    m_pDisparityData = NULL;
    m_i32DisparityDataSize = 0;
    m_prtFaces = NULL;
    m_pi32FaceAngles = NULL;
    memset(&m_inParams, 0, sizeof(bokeh_frame_params_t));
    m_fMaxFOV = 0.0;

    ALOGD("DEBUG(%s[%d]):new ArcsoftFusionBokehCapture object allocated", __FUNCTION__, __LINE__);
}

ArcsoftFusionBokehCapture::~ArcsoftFusionBokehCapture()
{
    ALOGD("DEBUG(%s[%d]):new ArcsoftFusionBokehCapture object deallocated", __FUNCTION__, __LINE__);
}

status_t ArcsoftFusionBokehCapture::m_create(void)
{
    status_t ret = NO_ERROR;

    loadlibrary();

    ALOGD("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    return ret;
}

status_t ArcsoftFusionBokehCapture::m_destroy(void)
{
    status_t ret = NO_ERROR;

    unloadlibrary();

    ALOGD("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    return ret;
}

status_t ArcsoftFusionBokehCapture::m_execute(Map_t *map)
{
    status_t ret = NO_ERROR;


    doBokehPPInit();
    getFrameParams(map);
#ifdef USES_DUAL_CAMERA_CAL_ARCSOFT
    memcpy(m_inParams.output.ppu8Plane[0], m_inParams.wideimg.ppu8Plane[0], sizeof(MByte)*m_inParams.output.pi32Pitch[0]*m_inParams.output.i32Height);
    memcpy(m_inParams.output.ppu8Plane[1], m_inParams.wideimg.ppu8Plane[1], sizeof(MByte)*m_inParams.output.pi32Pitch[0]*m_inParams.output.i32Height >> 1);
    ArcsoftCaliLib &callib = ArcsoftCaliLib::getInstance();
    callib.execute(m_inParams.teleimg, m_inParams.wideimg, map);
#else
    doBokehPPProcess(m_bokehIntensity);
#endif
    doBokehPPUninit();

    return ret;
}

status_t ArcsoftFusionBokehCapture::m_init(Map_t *map)
{
    status_t ret = NO_ERROR;

    return ret;
}

status_t ArcsoftFusionBokehCapture::m_query(Map_t *map)
{
    status_t ret = NO_ERROR;

    return ret;
}

/*===========================================================================
* FUNCTION   : getFrameParams
*
* DESCRIPTION:
*==========================================================================*/
void ArcsoftFusionBokehCapture::getFrameParams(Map_t* map)
{
    ARC_BOKEHPP_LOGD("getFrameParams <-----");

    /* from map */
#if 1
    int  focusX           = (Data_int32_t)(*map)[PLUGIN_FOCUS_POSTRION_X];
    int  focusY           = (Data_int32_t)(*map)[PLUGIN_FOCUS_POSTRION_Y];
#endif

    for (int i = 0; i < m_mapSrcBufCnt; i++) {
        ARC_BOKEHPP_LOGD("%s(%d): src[%d/%d]::(adr:%p, P%d, S%d, stride%d, [%d, %d, %d, %d, %d, %d] format :%d",
            __FUNCTION__, __LINE__,
            i,
            m_mapSrcBufCnt,
            m_mapSrcBufAddr[i][0],
            m_mapSrcBufPlaneCnt[i],
            m_mapSrcBufSize[i][0],
            m_mapSrcBufWStride[0],
            m_mapSrcRect[i][PLUGIN_ARRAY_RECT_X],
            m_mapSrcRect[i][PLUGIN_ARRAY_RECT_Y],
            m_mapSrcRect[i][PLUGIN_ARRAY_RECT_W],
            m_mapSrcRect[i][PLUGIN_ARRAY_RECT_H],
            m_mapSrcRect[i][PLUGIN_ARRAY_RECT_FULL_W],
            m_mapSrcRect[i][PLUGIN_ARRAY_RECT_FULL_H],
            m_mapSrcV4L2Format[i]);
    }

    ARC_BOKEHPP_LOGD("%s(%d): dst[%d]::(adr:%p, P%d, S%d, stride%d, [%d, %d, %d, %d, %d, %d] format :%d",
        __FUNCTION__, __LINE__,
        m_mapDstBufCnt,
        m_mapDstBufAddr[0],
        m_mapDstBufPlaneCnt[0],
        m_mapDstBufSize[0][0],
        m_mapDstBufWStride[0],
        m_mapDstRect[0][PLUGIN_ARRAY_RECT_X],
        m_mapDstRect[0][PLUGIN_ARRAY_RECT_Y],
        m_mapDstRect[0][PLUGIN_ARRAY_RECT_W],
        m_mapDstRect[0][PLUGIN_ARRAY_RECT_H],
        m_mapDstRect[0][PLUGIN_ARRAY_RECT_FULL_W],
        m_mapDstRect[0][PLUGIN_ARRAY_RECT_FULL_H],
        m_mapDstV4L2Format[0]);

    //feedinput Main frame size
    m_inParams.wideimg.u32PixelArrayFormat = ASVL_PAF_NV21;
    m_inParams.wideimg.i32Width = m_mapSrcRect[0][PLUGIN_ARRAY_RECT_W];
    m_inParams.wideimg.i32Height = m_mapSrcRect[0][PLUGIN_ARRAY_RECT_H];
    m_inParams.wideimg.pi32Pitch[0] = m_mapSrcBufWStride[0];
    m_inParams.wideimg.pi32Pitch[1] = m_mapSrcBufWStride[0];
    m_inParams.wideimg.pi32Pitch[2] = m_mapSrcBufWStride[0];
    m_inParams.wideimg.ppu8Plane[0] = (MUInt8*)m_mapSrcBufAddr[0][0];
    m_inParams.wideimg.ppu8Plane[1] = m_inParams.wideimg.ppu8Plane[0] + m_inParams.wideimg.pi32Pitch[0] * m_inParams.wideimg.i32Height;
    m_inParams.wideimg.ppu8Plane[2] = 0;
    ARC_BOKEHPP_LOGD("main image size %dx%d pitch is %d", m_mapSrcRect[0][PLUGIN_ARRAY_RECT_W], m_mapSrcRect[0][PLUGIN_ARRAY_RECT_H],
        m_mapSrcBufWStride[0]);

    // The output buffer info and output metadata are copied from the main camera input frame.
    m_inParams.output.u32PixelArrayFormat = m_inParams.wideimg.u32PixelArrayFormat;
    m_inParams.output.i32Width = m_mapDstRect[0][PLUGIN_ARRAY_RECT_W];
    m_inParams.output.i32Height = m_mapDstRect[0][PLUGIN_ARRAY_RECT_H];
    m_inParams.output.pi32Pitch[0] = m_mapDstBufWStride[0];
    m_inParams.output.pi32Pitch[1] = m_mapDstBufWStride[0];
    m_inParams.output.pi32Pitch[2] = m_mapDstBufWStride[0];

    m_inParams.output.ppu8Plane[0] = (MUInt8*)m_mapDstBufAddr[0];
    m_inParams.output.ppu8Plane[1] = m_inParams.output.ppu8Plane[0] + m_inParams.output.pi32Pitch[0] * m_inParams.output.i32Height;
    m_inParams.output.ppu8Plane[2] = 0;
    ARC_BOKEHPP_LOGD("output image size %dx%d pitch is %d", m_mapDstRect[0][PLUGIN_ARRAY_RECT_W], m_mapDstRect[0][PLUGIN_ARRAY_RECT_H],
        m_mapDstBufWStride[0]);

    //feedinput Aux frame size
    m_inParams.teleimg.u32PixelArrayFormat = ASVL_PAF_NV21;
    m_inParams.teleimg.i32Width = m_mapSrcRect[1][PLUGIN_ARRAY_RECT_W];
    m_inParams.teleimg.i32Height = m_mapSrcRect[1][PLUGIN_ARRAY_RECT_H];
    m_inParams.teleimg.pi32Pitch[0] = m_mapSrcBufWStride[1];
    m_inParams.teleimg.pi32Pitch[1] = m_mapSrcBufWStride[1];
    m_inParams.teleimg.pi32Pitch[2] = m_mapSrcBufWStride[1];

    m_inParams.teleimg.ppu8Plane[0] = (MUInt8*)m_mapSrcBufAddr[1][0];
    m_inParams.teleimg.ppu8Plane[1] = m_inParams.teleimg.ppu8Plane[0] + m_inParams.teleimg.pi32Pitch[0] * m_inParams.teleimg.i32Height;
    m_inParams.teleimg.ppu8Plane[2] = 0;
    ARC_BOKEHPP_LOGD("aux image size %dx%d pitch is %d", m_mapSrcRect[1][PLUGIN_ARRAY_RECT_W], m_mapSrcRect[1][PLUGIN_ARRAY_RECT_H],
        m_mapSrcBufWStride[1]);

    m_inParams.caminfo.i32MainWidth_CropNoScale = m_wideFullsizeW;
    m_inParams.caminfo.i32MainHeight_CropNoScale = m_wideFullsizeH;

    m_inParams.caminfo.i32AuxWidth_CropNoScale = m_teleFullsizeW;
    m_inParams.caminfo.i32AuxHeight_CropNoScale = m_teleFullsizeH;

    m_inParams.focus.x = focusX * m_inParams.wideimg.i32Width / m_inParams.caminfo.i32MainWidth_CropNoScale;
    m_inParams.focus.y = focusY * m_inParams.wideimg.i32Height / m_inParams.caminfo.i32MainHeight_CropNoScale;

    if (m_mapSrcAfStatus[0] == 1) {
        m_inParams.af_status = _AF_STATUS_VALID;
    } else {
        m_inParams.af_status = _AF_STATUS_INVALID;
    }

    ARC_BOKEHPP_LOGD("focus center is (%d,%d), status is %d", m_inParams.focus.x, m_inParams.focus.y, m_inParams.af_status);

    //get face detection info from main camera, tele camera is the main camera
    m_inParams.faceparam.fMaxFOV = m_fMaxFOV;
    m_inParams.faceparam.i32ImgDegree = BOKEH_DUAL_CAM_DEGREE;

    {
        memset(m_prtFaces, 0, (sizeof(MRECT) * BOKEH_MAX_FACE_NUM));
        memset(m_pi32FaceAngles, 0, (sizeof(MInt32) * BOKEH_MAX_FACE_NUM));

        m_inParams.faceparam.faceParam.i32FacesNum = 0;
        m_inParams.faceparam.faceParam.pi32FaceAngles = m_pi32FaceAngles;
        m_inParams.faceparam.faceParam.prtFaces = m_prtFaces;
    }



    ARC_BOKEHPP_LOGD("getFrameParams ----->");
}

int32_t ArcsoftFusionBokehCapture::loadlibrary()
{
    ARC_BOKEHPP_LOGD("E");
    int rc = NO_ERROR;

    void* dlPtr = dlopen(LIB_PATH_BOKEH, RTLD_LAZY);
    if (dlPtr == NULL) {
        ARC_BOKEHPP_LOGD("so load failed reason = %s", dlerror());
        return BAD_VALUE;
    }

    ARC_DCIR_GetVersion_Func dlGetVersionFuncPtr =
        (ARC_DCIR_GetVersion_Func)(dlsym(dlPtr, ARC_DCIR_GetVersion_OBJ_EXPORT));
    if (dlGetVersionFuncPtr == NULL) {
        ARC_BOKEHPP_LOGD("find %s failed", ARC_DCIR_GetVersion_OBJ_EXPORT);
        dlclose(dlPtr);
        return BAD_VALUE;
    }

    ARC_DCIR_GetDefaultParam_Func dlGetParamFuncPtr =
        (ARC_DCIR_GetDefaultParam_Func)(dlsym(dlPtr, ARC_DCIR_GetDefaultParam_OBJ_EXPORT));
    if (dlGetParamFuncPtr == NULL) {
        ARC_BOKEHPP_LOGD("find %s failed", ARC_DCIR_GetDefaultParam_OBJ_EXPORT);
        dlclose(dlPtr);
        return BAD_VALUE;
    }

    ARC_DCIR_Init_Func dlInitFuncPtr =
        (ARC_DCIR_Init_Func)(dlsym(dlPtr, ARC_DCIR_Init_OBJ_EXPORT));
    if (dlInitFuncPtr == NULL) {
        ARC_BOKEHPP_LOGD("find %s failed", ARC_DCIR_Init_OBJ_EXPORT);
        dlclose(dlPtr);
        return BAD_VALUE;
    }

    ARC_DCIR_Uninit_Func dlUnInitFuncPtr =
        (ARC_DCIR_Uninit_Func)(dlsym(dlPtr, ARC_DCIR_Uninit_OBJ_EXPORT));
    if (dlUnInitFuncPtr == NULL) {
        ARC_BOKEHPP_LOGD("find %s failed", ARC_DCIR_Uninit_OBJ_EXPORT);
        dlclose(dlPtr);
        return BAD_VALUE;
    }

    ARC_DCIR_SetCameraImageInfo_Func dlSetInfoFuncPtr =
        (ARC_DCIR_SetCameraImageInfo_Func)(dlsym(dlPtr, ARC_DCIR_SetCameraImageInfo_OBJ_EXPORT));
    if (dlSetInfoFuncPtr == NULL) {
        ARC_BOKEHPP_LOGD("find %s failed", ARC_DCIR_SetCameraImageInfo_OBJ_EXPORT);
        dlclose(dlPtr);
        return BAD_VALUE;
    }

    ARC_DCIR_SetCaliData_Func dlSetCaliDataFuncPtr =
        (ARC_DCIR_SetCaliData_Func)(dlsym(dlPtr, ARC_DCIR_SetCaliData_OBJ_EXPORT));
    if (dlSetCaliDataFuncPtr == NULL) {
        ARC_BOKEHPP_LOGD("find %s failed", ARC_DCIR_SetCaliData_OBJ_EXPORT);
        dlclose(dlPtr);
        return BAD_VALUE;
    }

    ARC_DCIR_CalcDisparityData_Func dlCalcDisparityFuncPtr =
        (ARC_DCIR_CalcDisparityData_Func)(dlsym(dlPtr, ARC_DCIR_CalcDisparityData_OBJ_EXPORT));
    if (dlCalcDisparityFuncPtr == NULL) {
        ARC_BOKEHPP_LOGD("find %s failed", ARC_DCIR_CalcDisparityData_OBJ_EXPORT);
        dlclose(dlPtr);
        return BAD_VALUE;
    }

    ARC_DCIR_GetDisparityDataSize_Func dlGetDisparityDizeFuncPtr =
        (ARC_DCIR_GetDisparityDataSize_Func)(dlsym(dlPtr, ARC_DCIR_GetDisparityDataSize_OBJ_EXPORT));
    if (dlGetDisparityDizeFuncPtr == NULL) {
        ARC_BOKEHPP_LOGD("find %s failed", ARC_DCIR_GetDisparityDataSize_OBJ_EXPORT);
        dlclose(dlPtr);
        return BAD_VALUE;
    }

    ARC_DCIR_GetDisparityData_Func dlGetDisparityFuncPtr =
        (ARC_DCIR_GetDisparityData_Func)(dlsym(dlPtr, ARC_DCIR_GetDisparityData_OBJ_EXPORT));
    if (dlGetDisparityFuncPtr == NULL) {
        ARC_BOKEHPP_LOGD("find %s failed", ARC_DCIR_GetDisparityData_OBJ_EXPORT);
        dlclose(dlPtr);
        return BAD_VALUE;
    }

    ARC_DCIR_Process_Func dlProcessFuncPtr =
        (ARC_DCIR_Process_Func)(dlsym(dlPtr, ARC_DCIR_Process_OBJ_EXPORT));
    if (dlProcessFuncPtr == NULL) {
        ARC_BOKEHPP_LOGD("find %s failed", ARC_DCIR_Process_OBJ_EXPORT);
        dlclose(dlPtr);
        return BAD_VALUE;
    }

    ARC_DCIR_BuildDeviceKernelBin_Func dlBuildDeviceKernelBinFuncPtr =
        (ARC_DCIR_BuildDeviceKernelBin_Func)(dlsym(dlPtr, ARC_DCIR_BuildDeviceKernelBin_OBJ_EXPORT));
    if (dlBuildDeviceKernelBinFuncPtr == NULL) {
        ARC_BOKEHPP_LOGD("find %s failed", ARC_DCIR_BuildDeviceKernelBin_OBJ_EXPORT);
        dlclose(dlPtr);
        return BAD_VALUE;
    }

    ARC_DCIR_SetOCLKernel_Func dlSetOCLKernelFuncPtr =
        (ARC_DCIR_SetOCLKernel_Func)(dlsym(dlPtr, ARC_DCIR_SetOCLKernel_OBJ_EXPORT));
    if (dlSetOCLKernelFuncPtr == NULL) {
        ARC_BOKEHPP_LOGD("find %s failed", ARC_DCIR_SetOCLKernel_OBJ_EXPORT);
        dlclose(dlPtr);
        return BAD_VALUE;
    }

    ARC_DCIR_Reset_Func dlResetFuncPtr =
        (ARC_DCIR_Reset_Func)(dlsym(dlPtr, ARC_DCIR_Reset_OBJ_EXPORT));
    if (dlResetFuncPtr == NULL) {
        ARC_BOKEHPP_LOGD("find %s failed", ARC_DCIR_Reset_OBJ_EXPORT);
        dlclose(dlPtr);
        return BAD_VALUE;
    }

    //Save func ptr
    bokeh_image_dll_export_t* pBokehDllDes = new bokeh_image_dll_export_t;
    pBokehDllDes->dlPtr = dlPtr;
    pBokehDllDes->dlARC_DCIR_GetVersion_FuncPtr = dlGetVersionFuncPtr;
    pBokehDllDes->dlARC_DCIR_GetDefaultParam_FuncPtr = dlGetParamFuncPtr;
    pBokehDllDes->dlARC_DCIR_Init_FuncPtr = dlInitFuncPtr;
    pBokehDllDes->dlARC_DCIR_Uninit_FuncPtr = dlUnInitFuncPtr;
    pBokehDllDes->dlARC_DCIR_SetCameraImageInfo_FuncPtr = dlSetInfoFuncPtr;
    pBokehDllDes->dlARC_DCIR_SetCaliData_FuncPtr = dlSetCaliDataFuncPtr;
    pBokehDllDes->dlARC_DCIR_CalcDisparityData_FuncPtr = dlCalcDisparityFuncPtr;
    pBokehDllDes->dlARC_DCIR_GetDisparityDataSize_FuncPtr = dlGetDisparityDizeFuncPtr;
    pBokehDllDes->dlARC_DCIR_GetDisparityData_FuncPtr = dlGetDisparityFuncPtr;
    pBokehDllDes->dlARC_DCIR_Process_FuncPtr = dlProcessFuncPtr;
    pBokehDllDes->dlARC_DCIR_BuildDeviceKernelBin_FuncPtr = dlBuildDeviceKernelBinFuncPtr;
    pBokehDllDes->dlARC_DCIR_SetOCLKernel_FuncPtr = dlSetOCLKernelFuncPtr;
    pBokehDllDes->dlARC_DCIR_Reset_FuncPtr = dlResetFuncPtr;

    m_pBokehDllDes = pBokehDllDes;

    ARC_BOKEHPP_LOGD("X");

    return rc;
}


int32_t ArcsoftFusionBokehCapture::unloadlibrary()
{
    //close so library
    if (m_pBokehDllDes && m_pBokehDllDes->dlPtr != NULL) {
        dlclose(m_pBokehDllDes->dlPtr);
        delete m_pBokehDllDes;
    }
    return NO_ERROR;
};

int32_t ArcsoftFusionBokehCapture::doBokehPPInit()
{
    ARC_BOKEHPP_LOGD("doBokehPPInit <-----");
    int rc = NO_ERROR;
    MHandle hEngine = MNull;
    PMRECT prtFaces = NULL;
    MInt32* pi32FaceAngles = NULL;
    ARC_DC_CALDATA calData;
    MRESULT res = MOK;
    MInt32  lFocusMode = ARC_DCIR_NORMAL_MODE;
    MPBASE_Version *version = NULL;
    memset(&calData, 0, sizeof(ARC_DC_CALDATA));

    /* use default param */
    char prop[PROPERTY_VALUE_MAX];
    memset(prop, 0, sizeof(prop));
    property_get("persist.camera.bokeh.defaultfov", prop, "0");
    int defaultfov = atoi(prop);

    ARC_BOKEHPP_LOGD("Bokeh doBokehPPInit: load bokeh library success");

    if (m_pBokehDllDes == NULL || m_pBokehDllDes->dlPtr == NULL) {
        rc = BAD_VALUE;
        goto FailedInit;
    }

    version = (MPBASE_Version *)m_pBokehDllDes->dlARC_DCIR_GetVersion_FuncPtr();
    if (version) {
        ARC_BOKEHPP_LOGD("arcsoft_refocus : Version = %s , %s ", version->Version, version->BuildDate);
    }

    // init algorithm engine
    res = m_pBokehDllDes->dlARC_DCIR_Init_FuncPtr(&hEngine, lFocusMode);
    if (MOK != res) {
        rc = BAD_VALUE;
        goto FailedInit;
    }
    ARC_BOKEHPP_LOGD("Bokeh doBokehPPInit: init bokeh success");

    calData.i32CalibDataSize = CALIB_DATA_ARCSOFT_SIZE;
    calData.pCalibData = m_calData;
    m_pBokehDllDes->dlARC_DCIR_SetCaliData_FuncPtr(hEngine, &calData);
    ARC_BOKEHPP_LOGD("Bokeh doBokehPPInit: set calibration data success, data size is %d", calData.i32CalibDataSize);

    prtFaces = (PMRECT)malloc(sizeof(MRECT) * BOKEH_MAX_FACE_NUM);
    pi32FaceAngles = (MInt32*)malloc(sizeof(MInt32) * BOKEH_MAX_FACE_NUM);
    if (NULL != prtFaces && NULL != pi32FaceAngles) {
        memset(prtFaces, 0, (sizeof(MRECT) * BOKEH_MAX_FACE_NUM));
        memset(pi32FaceAngles, 0, (sizeof(MInt32) * BOKEH_MAX_FACE_NUM));
    }
    else {
        rc = -ENOMEM;
        goto FailedInit;
    }

    if (1) {
        // get default param form algorithm engine
        ARC_DCIR_PARAM defaultparam_t;
        memset(&defaultparam_t, 0, sizeof(ARC_DCIR_PARAM));
        m_pBokehDllDes->dlARC_DCIR_GetDefaultParam_FuncPtr(&defaultparam_t);
        m_fMaxFOV = defaultparam_t.fMaxFOV;
        ARC_BOKEHPP_LOGD(" default param is MaxFov=%f degree=%d facenumber=%d", m_fMaxFOV, defaultparam_t.i32ImgDegree, defaultparam_t.faceParam.i32FacesNum);
    }
    else {
        // max FOV of Tele camera
        //m_fMaxFOV = MAX(m_pCaps->aux_cam_cap->hor_view_angle, m_pCaps->aux_cam_cap->ver_view_angle);
    }
    m_handle = hEngine;
    m_prtFaces = prtFaces;
    m_pi32FaceAngles = pi32FaceAngles;
    //ARC_BOKEHPP_LOGD(" Bokeh fov main[h:%f v:%f] aux[h:%f v:%f] maxFov[%f]",
    //    m_pCaps->main_cam_cap->hor_view_angle, m_pCaps->main_cam_cap->ver_view_angle,
    //    m_pCaps->aux_cam_cap->hor_view_angle, m_pCaps->aux_cam_cap->ver_view_angle, m_fMaxFOV);

    ARC_BOKEHPP_LOGD("doBokehPPInit ----->");
    return rc;

FailedInit:
    if (NULL != prtFaces) {
        free(prtFaces);
    }
    if (NULL != pi32FaceAngles) {
        free(pi32FaceAngles);
    }

    if (m_pBokehDllDes && hEngine) {
        m_pBokehDllDes->dlARC_DCIR_Uninit_FuncPtr(&hEngine);
    }

    ARC_BOKEHPP_LOGD("doBokehPPInit ----->");

    return rc;
}

int32_t ArcsoftFusionBokehCapture::doBokehPPUninit()
{
    ARC_BOKEHPP_LOGD("doBokehPPUninit <-----");

    //free face roi memory
    if (m_prtFaces) {
        free(m_prtFaces);
        m_prtFaces = NULL;
    }
    if (m_pi32FaceAngles) {
        free(m_pi32FaceAngles);
        m_pi32FaceAngles = NULL;
    }

    //free disparity data memory
    if (m_pDisparityData) {
        free(m_pDisparityData);
        m_pDisparityData = NULL;
        m_i32DisparityDataSize = 0;
    }

    //uninit algorithm engine
    if (m_handle) {
        m_pBokehDllDes->dlARC_DCIR_Uninit_FuncPtr(&m_handle);
        m_handle = NULL;
    }

    ARC_BOKEHPP_LOGD("doBokehPPUninit ----->");

    return NO_ERROR;
};

int32_t ArcsoftFusionBokehCapture::doBokehPPProcess(uint32_t bulrlevel)
{
    ARC_BOKEHPP_LOGD("E doBokehPPProcess <-----");

    //set image info(image size before CPP) to algorithm engine
    ARC_REFOCUSCAMERAIMAGE_PARAM camInfo;
    memset(&camInfo, 0, sizeof(ARC_REFOCUSCAMERAIMAGE_PARAM));
    camInfo.i32MainHeight_CropNoScale = m_inParams.caminfo.i32MainHeight_CropNoScale;
    camInfo.i32MainWidth_CropNoScale = m_inParams.caminfo.i32MainWidth_CropNoScale;
    camInfo.i32AuxHeight_CropNoScale = m_inParams.caminfo.i32AuxHeight_CropNoScale;
    camInfo.i32AuxWidth_CropNoScale = m_inParams.caminfo.i32AuxWidth_CropNoScale;

    ARC_BOKEHPP_LOGD("doBokehPPProcess: caminfo value:CropSizeNoScale tele [%dx%d] ,wide [%dx%d]",
        camInfo.i32MainWidth_CropNoScale, camInfo.i32MainHeight_CropNoScale,
        camInfo.i32AuxWidth_CropNoScale, camInfo.i32AuxHeight_CropNoScale);


    m_pBokehDllDes->dlARC_DCIR_SetCameraImageInfo_FuncPtr(m_handle, &camInfo);


    char prop[PROPERTY_VALUE_MAX];
    memset(prop, 0, sizeof(prop));
    property_get("persist.camera.bokehimg.dump", prop, "0");
    int dump = atoi(prop);
    // dump file
    if (dump) {
        nsecs_t end = systemTime();
        dumpYUVtoFile(&m_inParams.teleimg, end, "Tele");
        dumpYUVtoFile(&m_inParams.wideimg, end, "Wide");
        dumpYUVtoFile(&m_inParams.output, end, "out");

    }

    // calculate the disparity map only one time for one pair of images
    MRESULT res = MOK;
    MInt32 lDMSize = 0;
    res = m_pBokehDllDes->dlARC_DCIR_CalcDisparityData_FuncPtr(m_handle,
        &(m_inParams.wideimg),
        &(m_inParams.teleimg),
        &(m_inParams.faceparam));
    if (MOK != res) {
        ARC_BOKEHPP_LOGD("calc disparity data size fail,res=: %d", res);
        memcpy(m_inParams.output.ppu8Plane[0], m_inParams.wideimg.ppu8Plane[0], sizeof(MByte)*m_inParams.output.pi32Pitch[0]*m_inParams.output.i32Height);
        memcpy(m_inParams.output.ppu8Plane[1], m_inParams.wideimg.ppu8Plane[1], sizeof(MByte)*m_inParams.output.pi32Pitch[0]*m_inParams.output.i32Height >> 1);
        return BAD_VALUE;
    }
    ARC_BOKEHPP_LOGD("calc disparity data success");

    // get the size of disparity map
    res = m_pBokehDllDes->dlARC_DCIR_GetDisparityDataSize_FuncPtr(m_handle, &lDMSize);
    if (MOK != res) {
        return BAD_VALUE;
    }
    if ((lDMSize > 0 && NULL == m_pDisparityData)
        || (lDMSize > 0 && lDMSize > m_i32DisparityDataSize)) {
        if (NULL != m_pDisparityData)
            free(m_pDisparityData);

        m_pDisparityData = (uint8_t*)malloc(lDMSize * sizeof(uint8_t));
        m_i32DisparityDataSize = lDMSize;
    }
    if (NULL == m_pDisparityData) {
        return -ENOMEM;
    }
    ARC_BOKEHPP_LOGD("get disparity data size is %d", m_i32DisparityDataSize);

    // get the data of disparity map
    res = m_pBokehDllDes->dlARC_DCIR_GetDisparityData_FuncPtr(m_handle, m_pDisparityData);
    if (MOK != res) {
        ARC_BOKEHPP_LOGD("get disparity data size fail,res=: %d", res);
        memcpy(m_inParams.output.ppu8Plane[0], m_inParams.wideimg.ppu8Plane[0], sizeof(MByte)*m_inParams.output.pi32Pitch[0]*m_inParams.output.i32Height);
        memcpy(m_inParams.output.ppu8Plane[1], m_inParams.wideimg.ppu8Plane[1], sizeof(MByte)*m_inParams.output.pi32Pitch[0]*m_inParams.output.i32Height >> 1);
        return BAD_VALUE;
    }
    ARC_BOKEHPP_LOGD("get disparity data success");

    // do bokeh effect multi-times
    ARC_DCIR_REFOCUS_PARAM rfParam;
    memset(&rfParam, 0, sizeof(ARC_DCIR_REFOCUS_PARAM));
    rfParam.ptFocus.x = m_inParams.focus.x;
    rfParam.ptFocus.y = m_inParams.focus.y;
    if (bulrlevel == 0 || bulrlevel > 100) {
        rfParam.i32BlurIntensity = BOKEH_BLUR_INTENSITY;
    }
    else {
        rfParam.i32BlurIntensity = bulrlevel;
    }

    ARC_BOKEHPP_LOGD("rfParam.ptFocus.x=%d rfParam.ptFocus.y=%d", rfParam.ptFocus.x, rfParam.ptFocus.y);

    res = m_pBokehDllDes->dlARC_DCIR_Process_FuncPtr(m_handle, m_pDisparityData, lDMSize,
        &(m_inParams.wideimg), &rfParam, &(m_inParams.output));
    ARC_BOKEHPP_LOGD("capture bokeh process done res = %d", res);
    if (MOK != res){
        return BAD_VALUE;
    }


    ARC_BOKEHPP_LOGD("X doBokehPPProcess ----->");
    return NO_ERROR;
}


int32_t ArcsoftFusionBokehCapture::loadKernelBin(ARC_DC_KERNALBIN *pKernelBin)
{
    FILE *fd = NULL;
    int32_t fileaccess = -1;
    int32_t filesize = 0;
    int32_t readsize = 0;

    if (pKernelBin == NULL)
        return UNEXPECTED_NULL;

    ARC_BOKEHPP_LOGD("E");
    fileaccess = access(LIB_PATH_BOKEH_KERNELBIN, 0);
    if (fileaccess != 0) {
        m_pBokehDllDes->dlARC_DCIR_BuildDeviceKernelBin_FuncPtr((char*)LIB_PATH_BOKEH_KERNELBIN);
    }
    fd = fopen(LIB_PATH_BOKEH_KERNELBIN, "rb");
    if (fd == NULL) {
        goto error;
    }
    fseek(fd, 0L, SEEK_END);
    filesize = ftell(fd);
    if (filesize <= 0) {
        goto error;
    }
    pKernelBin->i32KernelBindataSize = filesize;
    pKernelBin->pKernelBindata = (MByte*)malloc(sizeof(MByte) * filesize);

    fseek(fd, 0L, SEEK_SET);
    while (!feof(fd) && readsize < filesize) {
        readsize += fread((pKernelBin->pKernelBindata + readsize), sizeof(MByte), BOKEH_KERNELBIN_BLOCKSIZE, fd);
    }
    fclose(fd);

    ARC_BOKEHPP_LOGD("X");
    return NO_ERROR;

error:
    unloadKernelBin(pKernelBin);
    ARC_BOKEHPP_LOGD("X");
    return INVALID_OPERATION;
}

void ArcsoftFusionBokehCapture::unloadKernelBin(ARC_DC_KERNALBIN *pKernelBin)
{
    if (pKernelBin != NULL) {
        if (pKernelBin->pKernelBindata != NULL)
            free(pKernelBin->pKernelBindata);
        pKernelBin->i32KernelBindataSize = 0;
        pKernelBin->pKernelBindata = NULL;
    }
}

void ArcsoftFusionBokehCapture::dumpFrameParams(bokeh_frame_params_t& p)
{
    ARC_BOKEHPP_LOGD("E");

    LPASVLOFFSCREEN s = NULL;

    s = &p.teleimg;
    ARC_BOKEHPP_LOGD("tele frame size: %d, %d, stride:%d",
        s->i32Width, s->i32Height, s->pi32Pitch[0]);

    s = &p.wideimg;
    ARC_BOKEHPP_LOGD("wide frame size: %d, %d, stride:%d",
        s->i32Width, s->i32Height, s->pi32Pitch[0]);

    s = &p.output;
    ARC_BOKEHPP_LOGD("output frame size: %d, %d, stride:%d",
        s->i32Width, s->i32Height, s->pi32Pitch[0]);

    ARC_BOKEHPP_LOGD("focus point x,y: %d,%d",
        p.focus.x, p.focus.y);

    ARC_BOKEHPP_LOGD("cam info left[%d, %d] right:[%d, %d]",
        p.caminfo.i32MainWidth_CropNoScale, p.caminfo.i32MainHeight_CropNoScale,
        p.caminfo.i32AuxWidth_CropNoScale, p.caminfo.i32AuxHeight_CropNoScale);


    ARC_BOKEHPP_LOGD("X");
}

void ArcsoftFusionBokehCapture::dumpDatatoFile(MVoid *pData, int size, const char* name)
{
    ARC_BOKEHPP_LOGD("E");
    char filename[256];
    memset(filename, 0, sizeof(char) * 256);

    snprintf(filename, sizeof(filename), ARC_DEBUG_DUMP_NAME"%s", name);

    int file_fd = open(filename, O_RDWR | O_CREAT, 0777);
    if (file_fd > 0)
    {
        //fchmod(file_fd, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        write(file_fd, pData, size);
        ALOGD("DEBUG(%s[%d]):fd_close FD(%d)", __FUNCTION__, __LINE__, file_fd);
        close(file_fd);
    }
    ARC_BOKEHPP_LOGD("X");
}

void ArcsoftFusionBokehCapture::dumpYUVtoFile(ASVLOFFSCREEN *pAsvl, uint64_t idx, const char* name_prefix)
{
    ARC_BOKEHPP_LOGD("dumpYUVtoFile :E");
    char filename[256];
    memset(filename, 0, sizeof(char) * 256);

    snprintf(filename, sizeof(filename), ARC_DEBUG_DUMP_NAME"%s_%dx%d_%lld.nv21",
        name_prefix, pAsvl->pi32Pitch[0], pAsvl->i32Height, idx);

    int file_fd = open(filename, O_RDWR | O_CREAT, 0777);

    if (file_fd > 0)
    {
        //fchmod(file_fd, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        ssize_t writen_bytes = 0;
        writen_bytes = write(file_fd, pAsvl->ppu8Plane[0], pAsvl->pi32Pitch[0] * pAsvl->i32Height);//only for NV21 or NV12
        writen_bytes = write(file_fd, pAsvl->ppu8Plane[1], pAsvl->pi32Pitch[1] * (pAsvl->i32Height >> 1));//only for NV21 or NV12
        ALOGD("DEBUG(%s[%d]):fd_close FD(%d)", __FUNCTION__, __LINE__, file_fd);
        close(file_fd);
    }
    else {
        ARC_BOKEHPP_LOGD("file_fd: %d   %s", file_fd, strerror(errno));
    }
    ARC_BOKEHPP_LOGD("dumpYUVtoFile :X");
}

#ifdef ARCSOFT_FUSION_WRAPPER

ARCDCIR_API MRESULT ARC_DCIR_Init(                // return MOK if success, otherwise fail
    MHandle                *phHandle,                // [in/out] The algorithm engine will be initialized by this API
    MInt32              i32Mode
)
{
    return MOK;
}

ARCDCIR_API MRESULT ARC_DCIR_Uninit(                // return MOK if success, otherwise fail
    MHandle                *phHandle                    // [in/out] The algorithm engine will be un-initialized by this API
)
{
    return MOK;
}


ARCDCIR_API MRESULT ARC_DCIR_SetCameraImageInfo(    // return MOK if success, otherwise fail
    MHandle                hHandle ,                   // [in]  The algorithm engine
    LPARC_REFOCUSCAMERAIMAGE_PARAM   pParam         // [in]  Camera and image information
    )
{
    return MOK;
}


ARCDCIR_API MRESULT ARC_DCIR_SetCaliData(            // return MOK if success, otherwise fail
    MHandle                hHandle ,                   // [in]  The algorithm engine
    LPARC_DC_CALDATA   pCaliData                    // [in]   Calibration Data
    )
{
    return MOK;
}


ARCDCIR_API MRESULT ARC_DCIR_SetDistortionCoef(
    MHandle                hHandle ,
    MFloat                *pLeftDistortionCoef,        // [in]  The left image distortion coefficient,size is float[11]
    MFloat                *pRightDistortionCoef        // [in]  The right image distortion coefficient,size is float[11]
)
{
    return MOK;
}


ARCDCIR_API MRESULT ARC_DCIR_CalcDisparityData(        // return MOK if success, otherwise fail
    MHandle                hHandle,                    // [in]  The algorithm engine
    LPASVLOFFSCREEN        pMainImg,                    // [in]  The offscreen of main image input. Generally, Left image or Wide image as Main.
    LPASVLOFFSCREEN        pAuxImg,                    // [in]  The offscreen of auxiliary image input.
    LPARC_DCIR_PARAM    pDCIRParam                     // [in]  The parameters for algorithm engine
)
{
    return MOK;
}


ARCDCIR_API MRESULT ARC_DCIR_GetDisparityDataSize(        // return MOK if success, otherwise fail
    MHandle                hHandle,                        // [in]  The algorithm engine
    MInt32                *pi32Size                        // [out] The size of disparity map
)
{
    return MOK;
}


ARCDCIR_API MRESULT ARC_DCIR_GetDisparityData(            // return MOK if success, otherwise fail
    MHandle                hHandle,                        // [in]  The algorithm engine
    MVoid                *pDisparityData                        // [out] The data of disparity map
)
{
    return MOK;
}


ARCDCIR_API MRESULT ARC_DCIR_Process(                    // return MOK if success, otherwise fail
    MHandle                        hHandle,                // [in]  The algorithm engine
    MVoid                        *pDisparityData,        // [in]  The data of disparity data
    MInt32                        i32DisparityDataSize,    // [in]  The size of disparity data
    LPASVLOFFSCREEN                pMainImg,                // [in]  The off-screen of main image
    LPARC_DCIR_REFOCUS_PARAM    pRFParam,                // [in]  The parameter for refocusing image
    LPASVLOFFSCREEN                pDstImg                    // [out]  The off-screen of result image
)
{
    return MOK;
}


ARCDCIR_API MRESULT ARC_DCIR_SetOCLKernel(            // return MOK if success, otherwise fail
    MHandle                hEngine,                // [in]  The algorithm engine
    ARC_DC_KERNALBIN    *kernelBin                // [in]  The kernelBin of ocl

)
{
    return MOK;
}

ARCDCIR_API MRESULT ARC_DCIR_Reset(                // return MOK if success, otherwise fail
    MHandle             hEngine                 // [in/out] The algorithm engine will be reset by this API
)
{
    return MOK;
}

#endif // ARCSOFT_FUSION_WRAPPER
