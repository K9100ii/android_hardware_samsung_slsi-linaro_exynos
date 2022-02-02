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

#define LOG_TAG "ArcsoftFusionBokehPreview"

#include "ArcsoftFusionBokehPreview.h"

#include <fcntl.h>
#include <cutils/properties.h>
#include <stdio.h>

#define __FOCUS_DEBUG__
bool g_arc_dump_reset = true;
int64_t g_arc_dump_timestamp = 0;
uint32_t g_arc_dump_frame_idx = 0;

void *ArcsoftFusionBokehPreview::m_handle = NULL;
Mutex ArcsoftFusionBokehPreview::m_handleLock;

ArcsoftFusionBokehPreview::ArcsoftFusionBokehPreview()
{
    m_pCaps = NULL;
    m_framelength = 0;
    g_arc_dump_frame_idx = 0;
    g_arc_dump_gap_frame_idx = 0;
    m_bokehIntensity = 0;
    memset(&m_inParams, 0, sizeof(rtb_frame_params_t));

    ALOGD("DEBUG(%s[%d]):new ArcsoftFusionBokehPreview object allocated", __FUNCTION__, __LINE__);
}

ArcsoftFusionBokehPreview::~ArcsoftFusionBokehPreview()
{
    ALOGD("DEBUG(%s[%d]):new ArcsoftFusionBokehPreview object deallocated", __FUNCTION__, __LINE__);
}

status_t ArcsoftFusionBokehPreview::m_create(void)
{
    status_t ret = NO_ERROR;

    ALOGD("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    return ret;
}

status_t ArcsoftFusionBokehPreview::m_destroy(void)
{
    status_t ret = NO_ERROR;

    doArcRTBUninit();

    ALOGD("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    return ret;
}

status_t ArcsoftFusionBokehPreview::m_execute(Map_t *map)
{
    status_t ret = NO_ERROR;

    getFrameParams(map);

    doArcRTBProcess();

    return ret;
}

status_t ArcsoftFusionBokehPreview::m_init(Map_t *map)
{
    status_t ret = NO_ERROR;

    doArcRTBInit();

    return ret;
}

status_t ArcsoftFusionBokehPreview::m_uninit(Map_t *map)
{
    status_t ret = NO_ERROR;

    doArcRTBUninit();

    return ret;
}

status_t ArcsoftFusionBokehPreview::m_query(Map_t *map)
{
    status_t ret = NO_ERROR;

    if (map != NULL) {
        (*map)[PLUGIN_VERSION] = (Map_data_t)MAKE_VERSION(1, 0);
        (*map)[PLUGIN_LIB_NAME] = (Map_data_t) "ArcsoftFusionBokehPreviewLib";
        (*map)[PLUGIN_BUILD_DATE] = (Map_data_t)__DATE__;
        (*map)[PLUGIN_BUILD_TIME] = (Map_data_t)__TIME__;
        (*map)[PLUGIN_PLUGIN_CUR_SRC_BUF_CNT] = (Map_data_t)2;
        (*map)[PLUGIN_PLUGIN_CUR_DST_BUF_CNT] = (Map_data_t)1;
        (*map)[PLUGIN_PLUGIN_MAX_SRC_BUF_CNT] = (Map_data_t)2;
        (*map)[PLUGIN_PLUGIN_MAX_DST_BUF_CNT] = (Map_data_t)1;
    }

    return ret;
}


/*===========================================================================
* FUNCTION   : getFrameParams
*
* DESCRIPTION: Helper function to get input params from input metadata
*==========================================================================*/
void ArcsoftFusionBokehPreview::getFrameParams(Map_t *map)
{

    ARC_BOKEH_LOGD("E");

#if 1
    int  focusX           = (Data_int32_t)(*map)[PLUGIN_FOCUS_POSTRION_X];
    int  focusY           = (Data_int32_t)(*map)[PLUGIN_FOCUS_POSTRION_Y];
#endif

    for (int i = 0; i < m_mapSrcBufCnt; i++) {
        ARC_BOKEH_LOGD("%s(%d): src[%d/%d]::(adr:%p, P%d, S%d, stride%d, [%d, %d, %d, %d, %d, %d] format :%d",
            __FUNCTION__, __LINE__,
            i,
            m_mapSrcBufCnt,
            m_mapSrcBufAddr[i][0],
            m_mapSrcBufPlaneCnt[i],
            m_mapSrcBufSize[i][0],
            m_mapDstBufWStride[i],
            m_mapSrcRect[i][PLUGIN_ARRAY_RECT_X],
            m_mapSrcRect[i][PLUGIN_ARRAY_RECT_Y],
            m_mapSrcRect[i][PLUGIN_ARRAY_RECT_W],
            m_mapSrcRect[i][PLUGIN_ARRAY_RECT_H],
            m_mapSrcRect[i][PLUGIN_ARRAY_RECT_FULL_W],
            m_mapSrcRect[i][PLUGIN_ARRAY_RECT_FULL_H],
            m_mapSrcV4L2Format[i]);
    }
#if 1
    ARC_BOKEH_LOGD("focus point:(%d).(%d), AF status:%d,wide fullsize:[%d, %d], tele fullsize:[%d, %d], intensity: %d]",
        focusX,
        focusY,
        m_mapSrcAfStatus[0],
        m_wideFullsizeW,
        m_wideFullsizeH,
        m_teleFullsizeW,
        m_teleFullsizeH,
        m_bokehIntensity);
#endif
    ARC_BOKEH_LOGD("%s(%d): dst[%d]::(adr:%p, P%d, S%d, stride%d, [%d, %d, %d, %d, %d, %d] format :%d",
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
    m_inParams.mainimg.u32PixelArrayFormat = ASVL_PAF_NV21;
    m_inParams.mainimg.i32Width = m_mapSrcRect[0][PLUGIN_ARRAY_RECT_W];
    m_inParams.mainimg.i32Height = m_mapSrcRect[0][PLUGIN_ARRAY_RECT_H];
    m_inParams.mainimg.pi32Pitch[0] = m_mapSrcBufWStride[0];
    m_inParams.mainimg.pi32Pitch[1] = m_mapSrcBufWStride[0];
    m_inParams.mainimg.pi32Pitch[2] = m_mapSrcBufWStride[0];
    m_inParams.mainimg.ppu8Plane[0] = (MUInt8*)m_mapSrcBufAddr[0][0];
    m_inParams.mainimg.ppu8Plane[1] = (MUInt8*)m_mapSrcBufAddr[0][1];
    m_inParams.mainimg.ppu8Plane[2] = 0;
    ARC_BOKEH_LOGD("main image size %dx%d pitch is %d", m_mapSrcRect[0][PLUGIN_ARRAY_RECT_W], m_mapSrcRect[0][PLUGIN_ARRAY_RECT_H],
        m_mapSrcBufWStride[0]);

    // The output buffer info and output metadata are copied from the main camera input frame.
    m_inParams.output.u32PixelArrayFormat = m_inParams.mainimg.u32PixelArrayFormat;
    m_inParams.output.i32Width = m_mapDstRect[0][PLUGIN_ARRAY_RECT_W];
    m_inParams.output.i32Height = m_mapDstRect[0][PLUGIN_ARRAY_RECT_H];
    m_inParams.output.pi32Pitch[0] = m_mapDstBufWStride[0];
    m_inParams.output.pi32Pitch[1] = m_mapDstBufWStride[0];
    m_inParams.output.pi32Pitch[2] = m_mapDstBufWStride[0];

    m_inParams.output.ppu8Plane[0] = (MUInt8*)m_mapDstBufAddr[0];
    m_inParams.output.ppu8Plane[1] = (MUInt8*)m_mapDstBufAddr[1];
    m_inParams.output.ppu8Plane[2] = 0;
    ARC_BOKEH_LOGD("output image size %dx%d pitch is %d", m_mapDstRect[0][PLUGIN_ARRAY_RECT_W], m_mapDstRect[0][PLUGIN_ARRAY_RECT_H],
        m_mapDstBufWStride[0]);

    //feedinput Aux frame size
    m_inParams.auximg.u32PixelArrayFormat = ASVL_PAF_NV21;
    m_inParams.auximg.i32Width = m_mapSrcRect[1][PLUGIN_ARRAY_RECT_W];
    m_inParams.auximg.i32Height = m_mapSrcRect[1][PLUGIN_ARRAY_RECT_H];
    m_inParams.auximg.pi32Pitch[0] = m_mapSrcBufWStride[1];
    m_inParams.auximg.pi32Pitch[1] = m_mapSrcBufWStride[1];
    m_inParams.auximg.pi32Pitch[2] = m_mapSrcBufWStride[1];

    m_inParams.auximg.ppu8Plane[0] = (MUInt8*)m_mapSrcBufAddr[1][0];
    m_inParams.auximg.ppu8Plane[1] = (MUInt8*)m_mapSrcBufAddr[1][1];
    m_inParams.auximg.ppu8Plane[2] = 0;
    ARC_BOKEH_LOGD("aux image size %dx%d pitch is %d", m_mapSrcRect[1][PLUGIN_ARRAY_RECT_W], m_mapSrcRect[1][PLUGIN_ARRAY_RECT_H],
        m_mapSrcBufWStride[1]);

    m_inParams.caminfo.i32MainWidth_CropNoScale = m_wideFullsizeW;
    m_inParams.caminfo.i32MainHeight_CropNoScale = m_wideFullsizeH;

    m_inParams.caminfo.i32AuxWidth_CropNoScale = m_teleFullsizeW;
    m_inParams.caminfo.i32AuxHeight_CropNoScale = m_teleFullsizeH;

    m_inParams.focus.x = focusX * m_inParams.mainimg.i32Width / m_inParams.caminfo.i32MainWidth_CropNoScale;
    m_inParams.focus.y = focusY * m_inParams.mainimg.i32Height / m_inParams.caminfo.i32MainHeight_CropNoScale;

    if (m_mapSrcAfStatus[0] == 1) {
        m_inParams.af_status = _AF_RTB_STATUS_VALID;
    } else {
        m_inParams.af_status = _AF_RTB_STATUS_INVALID;
    }

    ARC_BOKEH_LOGD("focus center is (%d,%d), focus status is %d", m_inParams.focus.x, m_inParams.focus.y, m_inParams.af_status);

    ARC_BOKEH_LOGD("X");
}


int32_t ArcsoftFusionBokehPreview::doArcRTBInit()
{
    Mutex::Autolock lock(m_handleLock);

    if(m_handle != NULL) {
        return NO_ERROR;
    }

    ARC_BOKEH_LOGD("doArcRTBInit E");
    int rc = NO_ERROR;
    ARC_DC_CALDATA calData;
    MRESULT res = MOK;
    MPBASE_Version *version = NULL;
    memset(&calData, 0, sizeof(ARC_DC_CALDATA));

    version = (MPBASE_Version *)ARC_DCVR_GetVersion();
    if (version) {
        ALOGD("arcsoft_refocus : Version = %s , %s ", version->Version, version->BuildDate);
    }

    // init algorithm engine
    res = ARC_DCVR_Init(&m_handle);
    if (MOK != res) {
        rc = BAD_VALUE;
    }
    ALOGD("ARC_DCVR_Init res = %d", res);
    calData.i32CalibDataSize = CALIB_DATA_ARCSOFT_SIZE;
    calData.pCalibData = m_calData;

    ARC_BOKEH_LOGD("ARC_DCVR_SetCaliData calData.i32CalibDataSize = %d, calData.pCalibData = %p", calData.i32CalibDataSize, calData.pCalibData);
    res = ARC_DCVR_SetCaliData(m_handle, &calData);
    ALOGD("ARC_DCVR_SetCaliData res = %d", res);

    char prop[PROPERTY_VALUE_MAX];
    memset(prop, 0, sizeof(prop));
    property_get("persist.camera.arccal.dump", prop, "0");
    int dump = atoi(prop);
    // dump file
    if (dump) {
        dumpDatatoFile(m_calData, ARC_DCOZ_CALIBRATIONDATA_LEN, "cali.bin");
    }

    res = ARC_DCVR_SetImageDegree(m_handle, RTB_DUAL_CAM_DEGREE);
    ARC_BOKEH_LOGD("ARC_DCVR_SetImageDegree res = %d", res);
    if (MOK != res) {
        rc = BAD_VALUE;
    }

    ARC_BOKEH_LOGD("doArcRTBInit X");
    return rc;
}

int32_t ArcsoftFusionBokehPreview::doArcRTBUninit()
{
    Mutex::Autolock lock(m_handleLock);
    int rc = NO_ERROR;
    //uninit algorithm engine
    if (m_handle != NULL) {
        ARC_BOKEH_LOGD("doArcRTBUninit E");
        rc = ARC_DCVR_Uninit(&m_handle);
        ALOGD("ARC_DCVR_Uninit res = %d", rc);
        m_handle = NULL;
    }


    ARC_BOKEH_LOGD("doArcRTBUninit X");

    return NO_ERROR;
};

int32_t ArcsoftFusionBokehPreview::doArcRTBProcess()
{
    ARC_BOKEH_LOGD("E");
    MRESULT res = MOK;

    //set image info(image size before CPP) to algorithm engine
    ARC_REFOCUSCAMERAIMAGE_PARAM camInfo;
    memset(&camInfo, 0, sizeof(ARC_REFOCUSCAMERAIMAGE_PARAM));
    camInfo.i32MainHeight_CropNoScale = m_inParams.caminfo.i32MainHeight_CropNoScale;
    camInfo.i32MainWidth_CropNoScale = m_inParams.caminfo.i32MainWidth_CropNoScale;
    camInfo.i32AuxHeight_CropNoScale = m_inParams.caminfo.i32AuxHeight_CropNoScale;
    camInfo.i32AuxWidth_CropNoScale = m_inParams.caminfo.i32AuxWidth_CropNoScale;
    ARC_BOKEH_LOGD("caminfo value:fullsize main [%dx%d] ,aux [%dx%d]",
        camInfo.i32MainWidth_CropNoScale, camInfo.i32MainHeight_CropNoScale,
        camInfo.i32AuxWidth_CropNoScale, camInfo.i32AuxHeight_CropNoScale);

    if (m_handle == NULL) {
        return BAD_VALUE;
    }
    res = ARC_DCVR_SetCameraImageInfo(m_handle, &camInfo);
    ALOGD("ARC_DCVR_SetCameraImageInfo res = %d", res);

    // do bokeh effect multi-times
    ARC_DCVR_PARAM rfParam;
    memset(&rfParam, 0, sizeof(ARC_DCVR_PARAM));
    rfParam.ptFocus.x = m_inParams.focus.x;
    rfParam.ptFocus.y = m_inParams.focus.y;
    rfParam.i32BlurLevel = m_bokehIntensity;
    rfParam.bRefocusOn = m_inParams.af_status;

    ARC_BOKEH_LOGD("rfParam.ptFocus.x=%d rfParam.ptFocus.y=%d rfParam.bRefocusOn=%d, rfParam.i32BlurLevel=%d", rfParam.ptFocus.x, rfParam.ptFocus.y, rfParam.bRefocusOn, rfParam.i32BlurLevel);

    if (m_handle == NULL) {
        return BAD_VALUE;
    }

    if (m_inParams.mainimg.ppu8Plane[0] == NULL || m_inParams.auximg.ppu8Plane[0] == NULL || m_inParams.output.ppu8Plane[0] == NULL) {
        ARC_BOKEH_LOGD("buffer is NULL");
    }

    res = ARC_DCVR_Process(m_handle, &(m_inParams.mainimg), &(m_inParams.auximg), &(m_inParams.output), &rfParam);
    ALOGD("ARC_DCVR_Process res = %d", res);

    char prop[PROPERTY_VALUE_MAX];
    memset(prop, 0, sizeof(prop));
    property_get("persist.camera.arcrtb.dump", prop, "0");
    int dump = atoi(prop);
    // dump file
    if (dump) {
        int64_t timestamp = 0;
        timeval now;
        gettimeofday(&now, NULL);
        timestamp = now.tv_sec*1000+(now.tv_usec+500)/1000;
        if (rfParam.bRefocusOn)
        {
            if (g_arc_dump_reset) {
                g_arc_dump_timestamp = timestamp;
                g_arc_dump_reset = false;
            }
            dumpYUVtoFile(&m_inParams.mainimg, "bokeh_wide", g_arc_dump_timestamp);
            dumpYUVtoFile(&m_inParams.auximg, "bokeh_tele", g_arc_dump_timestamp);
            dumpYUVtoFile(&m_inParams.output, "bokeh_out", g_arc_dump_timestamp);
        }
        else {
            g_arc_dump_reset = true;
        }
    }

    char propex[PROPERTY_VALUE_MAX];
    memset(propex, 0, sizeof(propex));
    property_get("persist.camera.arcrtbframe.dump", propex, "0");

    int dumpex = atoi(propex);
    // dump file
    if (dumpex) {
        int64_t timestamp = 0;
        timeval now;
        gettimeofday(&now, NULL);
        timestamp = now.tv_sec*1000+(now.tv_usec+500)/1000;

        if (g_arc_dump_gap_frame_idx % 30 == 0)
        {
            dumpYUVtoFile(&m_inParams.mainimg, "bokeh_wide_frame", timestamp);
            dumpYUVtoFile(&m_inParams.auximg, "bokeh_tele_frame", timestamp);
            dumpYUVtoFile(&m_inParams.output, "bokeh_out_frame", timestamp);
        }
        g_arc_dump_gap_frame_idx++;
    }

    if (MOK != res){
        memcpy(m_inParams.output.ppu8Plane[0], m_inParams.mainimg.ppu8Plane[0], sizeof(MByte)*m_inParams.output.pi32Pitch[0]*m_inParams.output.i32Height);
        memcpy(m_inParams.output.ppu8Plane[1], m_inParams.mainimg.ppu8Plane[1], sizeof(MByte)*m_inParams.output.pi32Pitch[0]*m_inParams.output.i32Height >> 1);
        return res;
    }




    ARC_BOKEH_LOGD("X");
    return res;
}

void ArcsoftFusionBokehPreview::dumpDatatoFile(MVoid *pData, int size, const char* name)
{
    ARC_BOKEH_LOGD("E");
    char filename[256];
    memset(filename, 0, sizeof(char) * 256);

    snprintf(filename, sizeof(filename), ARC_DEBUG_DUMP_NAME"%s", name);

    int file_fd = open(filename, O_RDWR | O_CREAT, 0777);
    if (file_fd > 0)
    {
        //fchmod(file_fd, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        write(file_fd, pData, size);
        ARC_BOKEH_LOGD("DEBUG(%s[%d]):fd_close FD(%d)", __FUNCTION__, __LINE__, file_fd);
        close(file_fd);
    }
    ARC_BOKEH_LOGD("X");
}

void ArcsoftFusionBokehPreview::dumpYUVtoFile(ASVLOFFSCREEN *pAsvl, const char* name_prefix, const int64_t name_timestamp)
{
  ARC_BOKEH_LOGD("QCameraArcRTB----dumpYUVtoFile E");
  char filename[256];
  memset(filename, 0, sizeof(char)*256);

  snprintf(filename, sizeof(filename), ARC_DEBUG_DUMP_NAME"%s_%lld_%dx%d.nv21",
                name_prefix, name_timestamp, pAsvl->pi32Pitch[0], pAsvl->i32Height);

  int file_fd = open(filename, O_RDWR | O_CREAT | O_APPEND, 0777);
  if (file_fd > 0)
  {
    //fchmod(file_fd, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    ssize_t writen_bytes = 0;
    writen_bytes = write(file_fd, pAsvl->ppu8Plane[0], pAsvl->pi32Pitch[0] * pAsvl->i32Height);//only for NV21 or NV12
    writen_bytes = write(file_fd, pAsvl->ppu8Plane[1], pAsvl->pi32Pitch[1] * (pAsvl->i32Height >> 1));//only for NV21 or NV12
    close(file_fd);
  }

  ARC_BOKEH_LOGD("QCameraArcRTB----dump output frame to file: %s", filename);
}

void ArcsoftFusionBokehPreview::dumpFrameParams(rtb_frame_params_t& p)
{
    ARC_BOKEH_LOGD("E");

    LPASVLOFFSCREEN s = NULL;

    s = &p.mainimg;
    ARC_BOKEH_LOGD("main frame size: %d, %d, stride:%d",
        s->i32Width, s->i32Height, s->pi32Pitch[0]);

    s = &p.auximg;
    ARC_BOKEH_LOGD("aux frame size: %d, %d, stride:%d",
        s->i32Width, s->i32Height, s->pi32Pitch[0]);

    s = &p.output;
    ARC_BOKEH_LOGD("output frame size: %d, %d, stride:%d",
        s->i32Width, s->i32Height, s->pi32Pitch[0]);

    ARC_BOKEH_LOGD("focus point x,y: %d,%d",
        p.focus.x, p.focus.y);

    ARC_BOKEH_LOGD("cam info left[%d, %d] right:[%d, %d]",
        p.caminfo.i32MainWidth_CropNoScale, p.caminfo.i32MainHeight_CropNoScale,
        p.caminfo.i32AuxWidth_CropNoScale, p.caminfo.i32AuxHeight_CropNoScale);


    ARC_BOKEH_LOGD("X");
}
nsecs_t ArcsoftFusionBokehPreview::getFrameTimeStamp(void *preview_frame) {
    ARC_BOKEH_LOGD("E");

    nsecs_t frameTime = 0;
    //frameTime = nsecs_t(preview_frame->ts.tv_sec) * 1000000000LL + preview_frame->ts.tv_nsec;

    ARC_BOKEH_LOGD("X");

    return frameTime;
}

#ifdef ARCSOFT_FUSION_WRAPPER
ARC_DCVR_API const MPBASE_Version *ARC_DCVR_GetVersion()
{
    return MOK;
}

ARC_DCVR_API MRESULT ARC_DCVR_Init(                    // return MOK if success, otherwise fail
    MHandle                *phHandle                    // [out] The algorithm engine will be initialized by this API
    )
{
    return MOK;
}

ARC_DCVR_API MRESULT ARC_DCVR_SetCameraImageInfo(                    // return MOK if success, otherwise fail
    MHandle                hHandle,                               // [in]  The algorithm engine
    LPARC_REFOCUSCAMERAIMAGE_PARAM   pParam                     // [in]  Camera and image information
    )
{
    return MOK;
}

ARC_DCVR_API MRESULT ARC_DCVR_SetCaliData(                    // return MOK if success, otherwise fail
    MHandle                hHandle,                               // [in]  The algorithm engine
    LPARC_DC_CALDATA   pCaliData                          // [in]   Calibration Data
    )
{
    return MOK;
}

ARC_DCVR_API MRESULT ARC_DCVR_SetImageDegree(                    // return MOK if success, otherwise fail
    MHandle                hHandle,                               // [in]  The algorithm engine
    MInt32    i32ImgDegree                               // [in]  The degree of input images
    )
{
    return MOK;
}

ARC_DCVR_API MRESULT ARC_DCVR_Uninit(                // return MOK if success, otherwise fail
    MHandle                *phHandle                // [in/out] The algorithm engine will be un-initialized by this API
    )
{
    return MOK;
}

ARC_DCVR_API MRESULT ARC_DCVR_Process(                // return MOK if success, otherwise fail
    MHandle                hHandle,                    // [in]  The algorithm engine
    LPASVLOFFSCREEN        pMainImg,                    // [in]  The offscreen of main image input. Generally, Left or Wide as Main.
    LPASVLOFFSCREEN        pAuxImg,                    // [in]  The offscreen of auxiliary image input.
    LPASVLOFFSCREEN        pDstImg,                    // [out] The offscreen of result image
    LPARC_DCVR_PARAM        pParam                    // [in]  The parameters for algorithm engine
    )
{
    return MOK;
}

#endif // ARCSOFT_FUSION_WRAPPER
