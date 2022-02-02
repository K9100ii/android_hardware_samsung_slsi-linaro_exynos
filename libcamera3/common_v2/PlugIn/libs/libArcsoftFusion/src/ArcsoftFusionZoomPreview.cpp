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

#define LOG_TAG "ArcsoftFusionZoomPreview"

#include <fcntl.h>
#include <dlfcn.h>
#include <cutils/properties.h>
#include "ArcsoftFusionZoomPreview.h"

#if 0
#ifdef ALOGD
#undef ALOGD
#define ALOGD ALOGV
#endif
#endif

#ifdef ARCSOFT_FUSION_WRAPPER
float g_zoom_list[] = {
    1,1,1,0,0,
    1.021,1,1,0,0,
    1.042,1.021,1,0,0,
    1.065,1.042,1,0,0,
    1.087,1.065,1,0,0,
    1.11,1.087,1,0,0,
    1.134,1.11,1,0,0,
    1.158,1.134,1,0,0,
    1.182,1.158,1,0,0,
    1.208,1.182,1,0,0,
    1.233,1.208,1,0,0,
    1.259,1.233,1,0,0,
    1.286,1.259,1,0,0,
    1.313,1.286,1,0,0,
    1.341,1.313,1,0,0,
    1.37,1.341,1,0,0,
    1.399,1.37,1,0,0,
    1.429,1.158,1,20,0,
    1.459,1.182,1,20,0,
    1.49,1.208,1,20,0,
    1.522,1.233,1,20,0,
    1.554,1.259,1,20,0,
    1.587,1.286,1,20,0,
    1.621,1.313,1,20,0,
    1.655,1.341,1,20,0,
    1.69,1.37,1,20,0,
    1.726,1.399,1,20,0,
    1.763,1.429,1,20,0,
    1.8,1.459,1,20,0,
    1.838,1.49,1,20,0,
    1.877,1.522,1,20,0,
    1.917,1.554,1,20,0,
    1.958,1.587,1,20,0,
    2,1.621,1,20,0,
    2.042,1.655,1.021,20,0,
    2.085,1.69,1.042,20,0,
    2.13,1.726,1.065,20,0,
    2.175,1.763,1.087,20,0,
    2.221,1.8,1.11,20,0,
    2.268,1.838,1.134,20,0,
    2.316,1.877,1.158,20,0,
    2.365,1.917,1.182,20,0,
    2.416,1.958,1,20,20,
    2.467,2,1.021,20,20,
    2.519,2.042,1.042,20,20,
    2.573,2.085,1.065,20,20,
    2.627,2.13,1.087,20,20,
    2.683,2.175,1.11,20,20,
    2.74,2.221,1.134,20,20,
    2.798,2.268,1.158,20,20,
    2.858,2.316,1.182,20,20,
    2.918,2.365,1.208,20,20,
    2.98,2.416,1.233,20,20,
    3.044,2.467,1.259,20,20,
    3.108,2.519,1.286,20,20,
    3.174,2.573,1.313,20,20,
    3.242,2.627,1.341,20,20,
    3.311,2.683,1.37,20,20,
    3.381,2.74,1.399,20,20,
    3.453,2.798,1.429,20,20,
    3.526,2.858,1.459,20,20,
    3.601,2.918,1.49,20,20,
    3.677,2.98,1.522,20,20,
    3.755,3.044,1.554,20,20,
    3.835,3.108,1.587,20,20,
    3.916,3.174,1.621,20,20,
    4,3.311,1.655,20,20,
    4.084,3.381,1.69,20,20,
    4.171,3.453,1.726,20,20,
    4.26,3.526,1.8,20,20,
    4.35,3.601,1.838,20,20,
    4.442,3.677,1.877,20,20,
    4.537,3.755,1.917,20,20,
    4.633,3.835,1.958,20,20,
    4.731,3.916,2,20,20,
    4.832,4,2.042,20,20,
    4.934,4.084,2.085,20,20,
    5.039,4.171,2.13,20,20,
    5.146,4.26,2.175,20,20,
    5.255,4.35,2.221,20,20,
    5.367,4.442,2.268,20,20,
    5.481,4.537,2.316,20,20,
    5.597,4.633,2.365,20,20,
    5.716,4.731,2.416,20,20,
    5.837,4.832,2.467,20,20,
    5.961,4.934,2.519,20,20,
    6.088,4.633,2.365,30,30,
    6.217,4.731,2.416,30,30,
    6.349,4.832,2.467,30,30,
    6.484,4.934,2.519,30,30,
    6.622,5.039,2.573,30,30,
    6.762,5.146,2.627,30,30,
    6.906,5.255,2.683,30,30,
    7.052,5.367,2.74,30,30,
    7.202,5.481,2.798,30,30,
    7.355,5.597,2.858,30,30,
    7.511,5.716,2.918,30,30,
    7.67,5.837,2.98,30,30,
    7.833,5.961,3.044,30,30,
    8,6.088,3.108,30,30};
#endif

//#define _WRITE_LOG_TO_FILE_

extern "C" long long GetCurrentTime()
{
    struct timeval time;
    gettimeofday(&time,NULL);
    long long curtime = ((long long)(time.tv_sec))*1000 + time.tv_usec/1000;
    return curtime;
}
long long g_arc_timestamp = 0;
uint32_t g_testframeID = 0;
FILE* m_LogFile;

MHandle ArcsoftFusionZoomPreview::m_handle = MNull;
Mutex   ArcsoftFusionZoomPreview::m_handleLock;

ArcsoftFusionZoomPreview::ArcsoftFusionZoomPreview()
{
    memset(&m_cameraImageInfo, 0, sizeof(m_cameraImageInfo));
    memset(&m_cameraParam, 0, sizeof(m_cameraParam));

    for (int i = 0; i < 2; i++) {
        m_mapOutImageRatioList[i] = NULL;
        m_mapOutNeedMarginList[i] = NULL;
    }

    m_bInit = false;
    ALOGD("DEBUG(%s[%d]):new ArcsoftFusionZoomPreview object allocated", __FUNCTION__, __LINE__);
}

ArcsoftFusionZoomPreview::~ArcsoftFusionZoomPreview()
{
    ALOGD("DEBUG(%s[%d]):new ArcsoftFusionZoomPreview object deallocated", __FUNCTION__, __LINE__);
}

status_t ArcsoftFusionZoomPreview::m_create(void)
{
    status_t ret = NO_ERROR;

    ALOGD("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    return ret;
}

status_t ArcsoftFusionZoomPreview::m_destroy(void)
{
    Mutex::Autolock lock(m_handleLock);

    status_t ret = NO_ERROR;
    MRESULT retVal = MOK;

    for (int i = 0; i < 2; i++) {
        if (m_zoomRatioList[i] != NULL) {
            delete [] m_zoomRatioList[i];
            m_zoomRatioList[i] = NULL;
        }

        if (m_mapOutImageRatioList[i] != NULL) {
            delete [] m_mapOutImageRatioList[i];
            m_mapOutImageRatioList[i] = NULL;
        }

        if (m_mapOutNeedMarginList[i] != NULL) {
            delete []m_mapOutNeedMarginList[i];
            m_mapOutNeedMarginList[i] = NULL;
        }
    }

    ret = m_uninitLib();
    if(ret != NO_ERROR) {
        ALOGE("ERR(%s[%d]):m_uninitLib() fail", __FUNCTION__, __LINE__);
    }

#ifdef _WRITE_LOG_TO_FILE_
    CloseFileLog();
#endif

    ALOGD("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    return ret;
}

status_t ArcsoftFusionZoomPreview::m_execute(Map_t *map)
{
    status_t ret = NO_ERROR;

    ALOGD("DEBUG(%s[%d]) E:", __FUNCTION__, __LINE__);

    MRESULT retVal = MOK;

    //for dump
    char prop[100];
    memset(prop, 0, sizeof(prop));
    property_get("persist.camera.arcst.dumpimg", prop, "0");
    int dumpimg = atoi(prop);

    ret = getFrameParams(map);

    ret = m_setCameraControlFlag(map);
    if (ret != NO_ERROR){
        ALOGE("ERR m_setCameraControlFlag%d", ret);
    }

    if(m_syncType == 1 || m_syncType == 2) {
    ALOGD("DEBUG(%s[%d]) : zoom preview parameters wide image size:(%d, %d), pitch(%d, %d), plane(%p, %p), fd(%d, %d)",
        __FUNCTION__, __LINE__,
        m_pWideImg.i32Width, m_pWideImg.i32Height,
        m_pWideImg.pi32Pitch[0], m_pWideImg.pi32Pitch[1],
        m_pWideImg.ppu8Plane[0], m_pWideImg.ppu8Plane[1],
        m_pFdWideImg.i32FdPlane1, m_pFdWideImg.i32FdPlane2);
    }

    if(m_syncType == 3 || m_syncType == 2) {
    ALOGD("DEBUG(%s[%d]) : zoom preview parameters tele image size:(%d, %d), pitch(%d, %d), plane(%p, %p), fd(%d, %d)",
        __FUNCTION__, __LINE__,
        m_pTeleImg.i32Width, m_pTeleImg.i32Height,
        m_pTeleImg.pi32Pitch[0], m_pTeleImg.pi32Pitch[1],
        m_pTeleImg.ppu8Plane[0], m_pTeleImg.ppu8Plane[1],
        m_pFdTeleImg.i32FdPlane1, m_pFdTeleImg.i32FdPlane2);
    }

    ALOGD("DEBUG(%s[%d]) : zoom preview parameters dst image size:(%d, %d), pitch(%d, %d), plane(%p, %p), fd(%d, %d)",
        __FUNCTION__, __LINE__,
        m_pDstImg.i32Width, m_pDstImg.i32Height,
        m_pDstImg.pi32Pitch[0], m_pDstImg.pi32Pitch[1],
        m_pDstImg.ppu8Plane[0], m_pDstImg.ppu8Plane[1],
        m_pFdResultImg.i32FdPlane1, m_pFdResultImg.i32FdPlane2);

    ALOGD("DEBUG(%s[%d]) : zoom preview parameters ratio:(%f, %f, %f) margin:(%d, %d)",
        __FUNCTION__, __LINE__,
        m_pRatioParam.fWideViewRatio, m_pRatioParam.fWideImageRatio, m_pRatioParam.fTeleImageRatio,
        m_pRatioParam.bNeedWideFullBuffer, m_pRatioParam.bNeedTeleFullBuffer);

    ALOGD("DEBUG(%s[%d]) : zoom preview parameters camera control flag before process = %d",
        __FUNCTION__, __LINE__,
        m_cameraParam.CameraControlFlag);

    if(dumpimg == 1)
    {
        char szRatio[256];
        memset(szRatio, 0x0, sizeof(char)*256);
        sprintf(szRatio,"%lld_%d_wide_%.2f_%.2f",
            g_arc_timestamp, g_testframeID, m_pRatioParam.fWideViewRatio, m_pRatioParam.fWideImageRatio);
        dumpYUVtoFile(&m_pWideImg, szRatio);
        memset(szRatio, 0x0, sizeof(char)*256);
        sprintf(szRatio,"%lld_%d_tele_%.2f_%.2f",
            g_arc_timestamp, g_testframeID, m_pRatioParam.fTeleViewRatio, m_pRatioParam.fTeleImageRatio);
        dumpYUVtoFile(&m_pTeleImg, szRatio);

#ifdef _WRITE_LOG_TO_FILE_
        {
            WriteFileLog("frameIndex=%d", g_testframeID);
            WriteFileLog("timestamp=%lld", GetCurrentTime());
            WriteFileLog("ratioParam.wideViewRatio=%.2f", m_pRatioParam.fWideViewRatio);
            WriteFileLog("ratioParam.wideImageRatio=%.2f", m_pRatioParam.fWideImageRatio);
            WriteFileLog("ratioParam.teleViewRatio=%.2f", m_pRatioParam.fTeleViewRatio);
            WriteFileLog("ratioParam.teleImageRatio=%.2f", m_pRatioParam.fTeleImageRatio);

            WriteFileLog("cameraParam.CameraControlFlag=%d", m_cameraParam.CameraControlFlag);
            if (m_cameraParam.CameraControlFlag & ARC_DCOZ_WIDECAMERAOPENED)
                WriteFileLog("cameraParam.CameraControlFlag WIDE OPENDED");
            else
                WriteFileLog("cameraParam.CameraControlFlag WIDE CLOSED");
            if (m_cameraParam.CameraControlFlag & ARC_DCOZ_TELECAMERAOPENED)
                WriteFileLog("cameraParam.CameraControlFlag TELE OPENDED");
            else
                WriteFileLog("cameraParam.CameraControlFlag TELE CLOSED");
            int masterCamera = (m_cameraParam.CameraControlFlag & ARC_DCOZ_MASTERSESSIONMASK);
            if (masterCamera == ARC_DCOZ_WIDESESSION)
                WriteFileLog("cameraParam.CameraControlFlag WIDE MASTER");
            else if (masterCamera == ARC_DCOZ_TELESESSION)
                WriteFileLog("cameraParam.CameraControlFlag TELE MASTER");
            int fallback = (m_cameraParam.CameraControlFlag & ARC_DCOZ_SWITCHMASK);
            if (fallback == ARC_DCOZ_SWITCHTOWIDE)
                WriteFileLog("cameraParam.CameraControlFlag FALLBACK");
            else
                WriteFileLog("cameraParam.CameraControlFlag NO FALLBACK");

            WriteFileLog("cameraParam.exposureTimeWide.den=%u", m_cameraParam.exposureTimeWide.den);
            WriteFileLog("cameraParam.exposureTimeWide.num=%u", m_cameraParam.exposureTimeWide.num);

            WriteFileLog("cameraParam.exposureTimeTele.den=%u", m_cameraParam.exposureTimeTele.den);
            WriteFileLog("cameraParam.exposureTimeTele.num=%u", m_cameraParam.exposureTimeTele.num);

            WriteFileLog("cameraParam.ISOWide=%d", m_cameraParam.ISOWide);
            WriteFileLog("cameraParam.ISOTele=%d", m_cameraParam.ISOTele);

            WriteFileLog("cameraParam.WideFocusFlag=%d", m_cameraParam.WideFocusFlag);
            WriteFileLog("cameraParam.TeleFocusFlag=%d", m_cameraParam.TeleFocusFlag);
        }
#endif
    }

    {
        retVal = ARC_DCVOZ_Process(
            m_handle,
            &m_pWideImg,
            &m_pTeleImg,
            &m_pRatioParam,
            &m_cameraParam,
            &m_pDstImg,
            &m_pFdWideImg,
            &m_pFdTeleImg,
            &m_pFdResultImg);
    }

    m_getFaceResultRect(map);

    if (dumpimg == 1) {
        char szRatio[256];
        memset(szRatio, 0x0, sizeof(char)*256);
        sprintf(szRatio,"%lld_%d_dst_%.2f_%.2f",
            g_arc_timestamp, g_testframeID, m_pRatioParam.fWideViewRatio, m_pRatioParam.fWideImageRatio);
        dumpYUVtoFile(&m_pDstImg, szRatio);

#ifdef _WRITE_LOG_TO_FILE_
        {
            WriteFileLog("after process ------");
            WriteFileLog("cameraParam.CameraControlFlag=%d", m_cameraParam.CameraControlFlag);
            int whichCamera = (m_cameraParam.CameraControlFlag & ARC_DCOZ_CAMERANEEDFLAGMASK);
            switch(whichCamera){
                case ARC_DCOZ_NEEDWIDEFRAMEONLY:
                    WriteFileLog("cameraParam.CameraControlFlag WIDE NEED OPEN");
                    break;
                case ARC_DCOZ_NEEDTELEFRAMEONLY:
                    WriteFileLog("cameraParam.CameraControlFlag TELE NEED OPEN");
                    break;
                case ARC_DCOZ_NEEDBOTHFRAMES:
                    WriteFileLog("cameraParam.CameraControlFlag BOTH NEED OPEN");
                    break;
            }

            int recommendSwitch = (m_cameraParam.CameraControlFlag & ARC_DCOZ_RECOMMENDMASK);
            switch(recommendSwitch){
                case ARC_DCOZ_RECOMMENDTOTELE:
                    WriteFileLog("cameraParam.CameraControlFlag TELE NEED MASTER");
                    break;
                case ARC_DCOZ_RECOMMENDTOWIDE:
                    WriteFileLog("cameraParam.CameraControlFlag WIDE NEED MASTER");
                    break;
                case ARC_DCOZ_RECOMMENDFB:
                    WriteFileLog("cameraParam.CameraControlFlag FALLBACK NEED MASTER");
                    break;
            }
            WriteFileLog("cameraParam.CameraIndex=%d", m_cameraParam.CameraIndex);
            WriteFileLog("cameraParam.ImageShiftX=%d", m_cameraParam.ImageShiftX);
            WriteFileLog("cameraParam.ImageShiftY=%d", m_cameraParam.ImageShiftY);
        }
#endif
        ++g_testframeID;
    }

    if (retVal != MOK) {
        ALOGE("ERR(%s[%d]):ARC_DCVOZ_Process() fail", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    ALOGD("DEBUG(%s[%d]) : zoom preview parameters camera control flag after process = %d",
        __FUNCTION__, __LINE__,
        m_cameraParam.CameraControlFlag);

    ALOGD("DEBUG(%s[%d]) : zoom preview parameters cameraParam CameraIndex:(%d), ImageShift:(%d, %d)",
        __FUNCTION__, __LINE__,
        m_cameraParam.CameraIndex, m_cameraParam.ImageShiftX, m_cameraParam.ImageShiftY);

    ret = m_getCameraControlFlag(map);
    if (ret != NO_ERROR){
        ALOGE("ERR m_getCameraControlFlag%d", ret);
    }

    m_saveValue(map);

    ALOGD("DEBUG(%s[%d]) X:", __FUNCTION__, __LINE__);

    return ret;
}

status_t ArcsoftFusionZoomPreview::m_init(Map_t *map)
{
    m_handleLock.lock();

    status_t ret = NO_ERROR;

    if (m_bInit == true) {
        ALOGD("DEBUG(%s[%d]) Already init", __FUNCTION__, __LINE__);
        ALOGD("");
        m_handleLock.unlock();
        return ret;
    }

    ALOGD("DEBUG(%s[%d]) E", __FUNCTION__, __LINE__);

    MRESULT retVal;

    g_arc_timestamp = GetCurrentTime();
    g_testframeID = 0;
#ifdef _WRITE_LOG_TO_FILE_
    m_LogFile = NULL;
    OpenFileLog();
#endif

    memset(&m_pRatioParam, 0, sizeof(ARC_DCVOZ_RATIOPARAM));
    memset(&m_cameraParam, 0, sizeof(ARC_DCVOZ_CAMERAPARAM));

    ret = m_initLib(map);
    if(ret != NO_ERROR) {
        ALOGE("ERR(%s[%d]):m_initLib() fail", __FUNCTION__, __LINE__);
        goto ERROR;
    }

    ret = m_setPreviewSize(map);
    if (ret != NO_ERROR) {
        ALOGE("ERR(%s[%d]):m_setPreviewSize() fail", __FUNCTION__, __LINE__);
        goto ERROR;
    }

    ret = m_setZoomList(map);
    if (ret != NO_ERROR) {
        ALOGE("ERR(%s[%d]):m_setZoomList() fail", __FUNCTION__, __LINE__);
        goto ERROR;
    }

    ret = m_InitZoomValue(map);
    if (ret != NO_ERROR) {
        ALOGE("ERR(%s[%d]):m_InitZoomValue() fail", __FUNCTION__, __LINE__);
        goto ERROR;
    }

    m_bInit = true;

    ALOGD("DEBUG(%s[%d]) X", __FUNCTION__, __LINE__);

    m_handleLock.unlock();

    return ret;

ERROR:
    m_handleLock.unlock();

    return m_uninit(map);
}

status_t ArcsoftFusionZoomPreview::m_uninit(Map_t *map)
{
    Mutex::Autolock lock(m_handleLock);

    status_t ret = NO_ERROR;

    ALOGD("DEBUG(%s[%d]) E", __FUNCTION__, __LINE__);

    for (int i = 0; i < 2; i++) {
        if (m_zoomRatioList[i] != NULL) {
            delete(m_zoomRatioList[i]);
            m_zoomRatioList[i] = NULL;
        }

        if (m_mapOutImageRatioList[i] != NULL) {
            delete(m_mapOutImageRatioList[i]);
            m_mapOutImageRatioList[i] = NULL;
        }

        if (m_mapOutNeedMarginList[i] != NULL) {
            delete(m_mapOutNeedMarginList[i]);
            m_mapOutNeedMarginList[i] = NULL;
        }
    }

    ret = m_uninitLib();
    if(ret != NO_ERROR) {
        ALOGE("ERR(%s[%d]):m_uninitLib() fail", __FUNCTION__, __LINE__);
    }

    m_bInit = false;

#ifdef _WRITE_LOG_TO_FILE_
    CloseFileLog();
#endif

    ALOGD("DEBUG(%s[%d]) X", __FUNCTION__, __LINE__);

    return ret;
}

status_t ArcsoftFusionZoomPreview::m_query(Map_t *map)
{
    status_t ret = NO_ERROR;

    return ret;
}

status_t ArcsoftFusionZoomPreview::m_initLib(Map_t *map)
{
    status_t ret = NO_ERROR;
    MRESULT retVal = MOK;

    ARC_DCOZ_INITPARAM initParam;
    m_init_ARC_DCOZ_INITPARAM(&initParam);

    ALOGD("DEBUG(%s[%d]):ARC_DCVOZ_Init b m_handle:%p", __FUNCTION__, __LINE__, m_handle);
    if(m_handle == NULL) {
        ALOGD("DEBUG(%s[%d]):ARC_DCVOZ_Init", __FUNCTION__, __LINE__);

        retVal = ARC_DCVOZ_Init(&m_handle, &initParam);
        if(retVal != MOK) {
            ALOGE("ERR(%s[%d]):ARC_DCVOZ_Init() fail ret = %d", __FUNCTION__, __LINE__, retVal);
            ret = INVALID_OPERATION;
        }
        ALOGD("DEBUG(%s[%d]):ARC_DCVOZ_Init a m_handle:%p", __FUNCTION__, __LINE__, m_handle);
    }

    return ret;
}

status_t ArcsoftFusionZoomPreview::m_uninitLib(void)
{
    status_t ret = NO_ERROR;
    MRESULT retVal = MOK;

    if(m_handle != NULL) {
        ALOGD("DEBUG(%s[%d]):ARC_DCVOZ_Uninit b m_handle:%p", __FUNCTION__, __LINE__, m_handle);

        retVal = ARC_DCVOZ_Uninit(&m_handle);
        if(retVal != MOK) {
            ALOGE("ERR(%s[%d]):ARC_DCVOZ_Uninit() fail ret = %d", __FUNCTION__, __LINE__, retVal);
            ret = INVALID_OPERATION;
        } else {
            m_handle = NULL;
        }
    }

    return ret;
}

status_t ArcsoftFusionZoomPreview::m_setPreviewSize(Map_t *map)
{
    status_t ret = NO_ERROR;
    MRESULT retVal;

    ARC_DCVOZ_CAMERAIMAGEINFO cameraImageInfo;

    MInt32 i32Mode = ARC_DCOZ_MODE_F;

    m_mapSrcRect = (Array_buf_rect_t) (*map)[PLUGIN_SRC_BUF_RECT];

    m_wideFullsizeW =  m_mapSrcRect[0][PLUGIN_ARRAY_RECT_FULL_W];
    m_wideFullsizeH =  m_mapSrcRect[0][PLUGIN_ARRAY_RECT_FULL_H];
    m_teleFullsizeW =  m_mapSrcRect[1][PLUGIN_ARRAY_RECT_FULL_W];
    m_teleFullsizeH =  m_mapSrcRect[1][PLUGIN_ARRAY_RECT_FULL_H];

    ALOGD("wide : w(%d), h(%d), fullW(%d), fullH(%d) / tele : w(%d), h(%d), fullW(%d), fullH(%d)",
        m_mapSrcRect[0][PLUGIN_ARRAY_RECT_W], m_mapSrcRect[0][PLUGIN_ARRAY_RECT_H], m_wideFullsizeW, m_wideFullsizeH,
        m_mapSrcRect[1][PLUGIN_ARRAY_RECT_W], m_mapSrcRect[1][PLUGIN_ARRAY_RECT_H], m_teleFullsizeW, m_teleFullsizeH);
    // ArcSoft: hard code here because we can't get these values when init, pls check
#if 0
    // wide
    //feed previewsize(e.g. 1440x1080)
    cameraImageInfo.i32WideWidth      = m_mapSrcRect[0][PLUGIN_ARRAY_RECT_W];
    cameraImageInfo.i32WideHeight     = m_mapSrcRect[0][PLUGIN_ARRAY_RECT_H];

    cameraImageInfo.i32WideFullWidth  = m_wideFullsizeW;    //[in] Width of wide full image by ISP.
    cameraImageInfo.i32WideFullHeight = m_wideFullsizeH;    //[in] Height of wide full image by ISP.

    // tele
    //feed previewsize(e.g. 1440x1080)
    cameraImageInfo.i32TeleWidth      = m_mapSrcRect[0][PLUGIN_ARRAY_RECT_W];
    cameraImageInfo.i32TeleHeight     = m_mapSrcRect[0][PLUGIN_ARRAY_RECT_H];

    cameraImageInfo.i32TeleFullWidth  = m_teleFullsizeW;    //[in] Width of tele full image by ISP.
    cameraImageInfo.i32TeleFullHeight = m_teleFullsizeH;    //[in] Height of tele full image by ISP.

    // dst
    //feed previewsize(e.g. 1440x1080)
    cameraImageInfo.i32DstWidth       = m_mapDstRect[0][PLUGIN_ARRAY_RECT_W];
    cameraImageInfo.i32DstHeight      = m_mapDstRect[0][PLUGIN_ARRAY_RECT_H];
#else
    if ((m_mapSrcRect[0][PLUGIN_ARRAY_RECT_W] * 9) == (m_mapSrcRect[0][PLUGIN_ARRAY_RECT_H] * 16)) {
        // wide
        //feed previewsize(e.g. 1440x1080)
        cameraImageInfo.i32WideWidth      = 1920;
        cameraImageInfo.i32WideHeight      = 1080;

        cameraImageInfo.i32WideFullWidth  = 4032;    //[in] Width of wide full image by ISP.
        cameraImageInfo.i32WideFullHeight = 2268;    //[in] Height of wide full image by ISP.

        // tele
        //feed previewsize(e.g. 1440x1080)
        cameraImageInfo.i32TeleWidth      = 1920;
        cameraImageInfo.i32TeleHeight      = 1080;

        cameraImageInfo.i32TeleFullWidth  = 4032;    //[in] Width of tele full image by ISP.
        cameraImageInfo.i32TeleFullHeight = 2268;    //[in] Height of tele full image by ISP.

        // dst
        //feed previewsize(e.g. 1440x1080)
        cameraImageInfo.i32DstWidth       = 1920;
        cameraImageInfo.i32DstHeight      = 1080;
    } else {
        // wide
        //feed previewsize(e.g. 1440x1080)
        cameraImageInfo.i32WideWidth      = 1440;
        cameraImageInfo.i32WideHeight      = 1080;

        cameraImageInfo.i32WideFullWidth  = 4032;    //[in] Width of wide full image by ISP.
        cameraImageInfo.i32WideFullHeight = 3024;    //[in] Height of wide full image by ISP.

        // tele
        //feed previewsize(e.g. 1440x1080)
        cameraImageInfo.i32TeleWidth      = 1440;
        cameraImageInfo.i32TeleHeight      = 1080;

        cameraImageInfo.i32TeleFullWidth  = 4032;    //[in] Width of tele full image by ISP.
        cameraImageInfo.i32TeleFullHeight = 3024;    //[in] Height of tele full image by ISP.

        // dst
        //feed previewsize(e.g. 1440x1080)
        cameraImageInfo.i32DstWidth       = 1440;
        cameraImageInfo.i32DstHeight      = 1080;
    }
#endif

//please check this API(Assertion is occurred)
#if 1
    retVal = ARC_DCVOZ_SetPreviewSize(
        m_handle,
        &cameraImageInfo,
        i32Mode);
    if (retVal != MOK) {
        ALOGE("ERR(%s[%d]):ARC_DCVOZ_SetPreviewSize(wide(%d x %d, in %d x %d) / wide(%d x %d, in %d x %d) / dst(%d x %d)) fail",
            __FUNCTION__, __LINE__,
            cameraImageInfo.i32WideWidth,
            cameraImageInfo.i32WideHeight,
            cameraImageInfo.i32WideFullWidth,
            cameraImageInfo.i32WideFullHeight,
            cameraImageInfo.i32TeleWidth,
            cameraImageInfo.i32TeleHeight,
            cameraImageInfo.i32TeleFullWidth,
            cameraImageInfo.i32TeleFullHeight,
            cameraImageInfo.i32DstWidth,
            cameraImageInfo.i32DstHeight);
        return INVALID_OPERATION;
    }
    ALOGD("(%s[%d]):ARC_DCVOZ_SetPreviewSize(wide(%d x %d, in %d x %d) / wide(%d x %d, in %d x %d) / dst(%d x %d))",
        __FUNCTION__, __LINE__,
        cameraImageInfo.i32WideWidth,
        cameraImageInfo.i32WideHeight,
        cameraImageInfo.i32WideFullWidth,
        cameraImageInfo.i32WideFullHeight,
        cameraImageInfo.i32TeleWidth,
        cameraImageInfo.i32TeleHeight,
        cameraImageInfo.i32TeleFullWidth,
        cameraImageInfo.i32TeleFullHeight,
        cameraImageInfo.i32DstWidth,
        cameraImageInfo.i32DstHeight);

    //get the max size from the algo (aka prviewsize(1440x1080)*130%),
    //and inform system to output this resize preview data
    m_cameraImageInfo.i32WideFullHeight = cameraImageInfo.i32WideFullHeight;
    m_cameraImageInfo.i32WideFullWidth = cameraImageInfo.i32WideFullWidth;
    m_cameraImageInfo.i32WideHeight = cameraImageInfo.i32WideHeight;
    m_cameraImageInfo.i32WideWidth = cameraImageInfo.i32WideWidth;

    m_cameraImageInfo.i32TeleFullHeight = cameraImageInfo.i32TeleFullHeight;
    m_cameraImageInfo.i32TeleFullWidth = cameraImageInfo.i32TeleFullWidth;
    m_cameraImageInfo.i32TeleHeight = cameraImageInfo.i32TeleHeight;
    m_cameraImageInfo.i32TeleWidth = cameraImageInfo.i32TeleWidth;

    m_cameraImageInfo.i32DstHeight = cameraImageInfo.i32DstHeight;
    m_cameraImageInfo.i32DstWidth = cameraImageInfo.i32DstWidth;
#endif

    return ret;

}

status_t ArcsoftFusionZoomPreview::m_setZoomList(Map_t *map)
{
    status_t ret = NO_ERROR;

    m_mapSrcZoomRatioListSize = (Array_buf_t)(*map)[PLUGIN_ZOOM_RATIO_LIST_SIZE];
    m_mapSrcZoomRatioList = (Array_float_addr_t)(*map)[PLUGIN_ZOOM_RATIO_LIST];

#if 0
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < m_mapSrcZoomRatioListSize[i]; j++) {
            ALOGD("m_mapSrcZoomRatioList[%d][%d] : %f", i,j, m_mapSrcZoomRatioList[i][j]);
        }
    }
#endif

    //get zoom list from camera system, aka all support zoom ratio
    Plugin_DCOZ_ZoomlistParam_t pCameraZoomListParam;
    pCameraZoomListParam.pfWideZoomList = (MFloat *)m_mapSrcZoomRatioList[0];
    pCameraZoomListParam.i32WideZoomListSize = (MInt32)m_mapSrcZoomRatioListSize[0];
    pCameraZoomListParam.pfTeleZoomList = (MFloat *)m_mapSrcZoomRatioList[0];
    pCameraZoomListParam.i32TeleZoomListSize = (MInt32)m_mapSrcZoomRatioListSize[0];

    if ((pCameraZoomListParam.pfWideZoomList == NULL) ||
        (pCameraZoomListParam.pfTeleZoomList == NULL) ||
        (pCameraZoomListParam.i32WideZoomListSize <= 0) ||
        (pCameraZoomListParam.i32TeleZoomListSize <= 0)) {

        ret = INVALID_OPERATION;
        ALOGE("ERR setZoomList(%p, %p, %d, %d) fail",
            pCameraZoomListParam.pfWideZoomList,
            pCameraZoomListParam.pfTeleZoomList,
            pCameraZoomListParam.i32WideZoomListSize,
            pCameraZoomListParam.i32TeleZoomListSize);
    }

    ret = ARC_DCVOZ_SetZoomList(m_handle,
                                pCameraZoomListParam.pfWideZoomList,
                                pCameraZoomListParam.i32WideZoomListSize,
                                pCameraZoomListParam.pfTeleZoomList,
                                pCameraZoomListParam.i32TeleZoomListSize);
    if(MOK != ret){
        ret = INVALID_OPERATION;
        ALOGE("ERR ARC_DCVOZ_SetZoomList ret = %d", ret);
    }

    return ret;
}

status_t ArcsoftFusionZoomPreview::m_InitZoomValue(Map_t *map)
{
    status_t ret = NO_ERROR;

    m_mapSrcZoomRatioListSize = (Array_buf_t)(*map)[PLUGIN_ZOOM_RATIO_LIST_SIZE];
    m_mapSrcZoomRatioList = (Array_float_addr_t)(*map)[PLUGIN_ZOOM_RATIO_LIST];

    m_zoomMaxSize = m_mapSrcZoomRatioListSize[0];
    MFloat fViewRatio;
    ALOGD("sizeOfList(%d)", m_zoomMaxSize);

    for (int i = 0; i < 2; i++) {
        if (m_zoomRatioList[i] == NULL) {
            m_zoomRatioList[i] = new float[m_zoomMaxSize];
        }
        if (m_mapSrcZoomRatioList[i] != NULL) {
            memcpy(m_zoomRatioList[i], m_mapSrcZoomRatioList[i], sizeof(float)*m_zoomMaxSize);
        }

        if (m_mapOutImageRatioList[i] == NULL) {
            m_mapOutImageRatioList[i] = new float[m_zoomMaxSize];
        }
        if (m_mapOutNeedMarginList[i] == NULL) {
            m_mapOutNeedMarginList[i] = new int[m_zoomMaxSize];
        }
    }



// ArcSoft: just for zoom list debug
#ifdef ARCSOFT_FUSION_WRAPPER
    for (int i = 0; i < m_zoomMaxSize; ++i) {
        m_mapOutImageRatioList[0][i] = g_zoom_list[i*5 + 1];
        m_mapOutImageRatioList[1][i] = g_zoom_list[i*5 + 2];
        m_mapOutNeedMarginList[0][i] = (int)g_zoom_list[i*5 + 3];
        m_mapOutNeedMarginList[1][i] = (int)g_zoom_list[i*5 + 4];
#if 0
        ALOGD("OutImageRatioList[%d](%f)(%f), OutNeedMarginList[%d](%d)(%d)",
            i, m_mapOutImageRatioList[0][i], m_mapOutImageRatioList[1][i],
            i, m_mapOutNeedMarginList[0][i], m_mapOutNeedMarginList[1][i]);

        ALOGD("zoomRatioList[%d](%f)", i, m_zoomRatioList[0][i]);
#endif
    }
#else
    for (int i = 0; i < m_zoomMaxSize; i++) {
        fViewRatio = m_mapSrcZoomRatioList[0][i];

        ret = ARC_DCVOZ_GetZoomVal(m_handle, fViewRatio,
                                m_mapOutImageRatioList[0] + i,
                                m_mapOutImageRatioList[1] + i,
                                m_mapOutNeedMarginList[0] + i,
                                m_mapOutNeedMarginList[1] + i);
        if(MOK != ret){
            ret = INVALID_OPERATION;
            ALOGE("ERR ARC_DCVOZ_GetZoomVal ret = %d", ret);
        }
#if 0
        ALOGD("viewRatio(%f), OutImageRatioList[%d](%f)(%f), OutNeedMarginList[%d](%d)(%d)",
            fViewRatio, i, m_mapOutImageRatioList[0][i], m_mapOutImageRatioList[1][i],
            i, m_mapOutNeedMarginList[0][i], m_mapOutNeedMarginList[1][i]);
#endif
    }
#endif

    (*map)[PLUGIN_IMAGE_RATIO_LIST] = (Map_data_t)m_mapOutImageRatioList;
    (*map)[PLUGIN_NEED_MARGIN_LIST] = (Map_data_t)m_mapOutNeedMarginList;

    return ret;
}

status_t ArcsoftFusionZoomPreview::m_setCameraControlFlag(Map_t * map)
{
    status_t ret = NO_ERROR;

    int openFlag = 0;
    int openFlag2 = 0;
    int masterCameraHAL2Arc = 0;
    bool bfallback = false;

    masterCameraHAL2Arc = (*map)[PLUGIN_MASTER_CAMERA_HAL2ARC];
    ALOGD("DEBUG(%s[%d]):switchCamera(%d) masterCameraHAL2Arc(0x%x) m_buttonZoomMode(%d)", __FUNCTION__, __LINE__,
        m_syncType, masterCameraHAL2Arc, m_buttonZoomMode);

    switch (m_syncType) {
    case 1:
        openFlag = ARCSOFT_FUSION_EMULATOR_WIDE_OPEN;
        //openFlag2 = ARCSOFT_FUSION_EMULATOR_TELE_OPEN;
        break;
    case 2:
        openFlag = ARCSOFT_FUSION_EMULATOR_WIDE_OPEN;
        openFlag2 = ARCSOFT_FUSION_EMULATOR_TELE_OPEN;
        break;
    case 3:
        //openFlag = ARCSOFT_FUSION_EMULATOR_WIDE_OPEN;
        openFlag2 = ARCSOFT_FUSION_EMULATOR_TELE_OPEN;
        break;
    default:
        ALOGW("Invalid m_syncType(%d). so, just set WIDE", (int)m_syncType);

        openFlag = ARCSOFT_FUSION_EMULATOR_WIDE_OPEN;
        //openFlag2 = ARCSOFT_FUSION_EMULATOR_TELE_OPEN;

        break;
    };

    m_cameraParam.CameraControlFlag &=(~ARC_DCOZ_WIDECAMERAOPENED);
    if (openFlag){
        m_cameraParam.CameraControlFlag |= ARC_DCOZ_WIDECAMERAOPENED; // Wide camera should opened.
        ALOGD("DEBUG(%s[%d]) : zoom preview parameters camera control flag WIDE OPENDED",
            __FUNCTION__, __LINE__);
    } else {
        ALOGD("DEBUG(%s[%d]) : zoom preview parameters camera control flag WIDE CLOSED",
            __FUNCTION__, __LINE__);
    }

    m_cameraParam.CameraControlFlag &=(~ARC_DCOZ_TELECAMERAOPENED);
    if (openFlag2){
        m_cameraParam.CameraControlFlag |= ARC_DCOZ_TELECAMERAOPENED; // Tele camera should opened.
        ALOGD("DEBUG(%s[%d]) : zoom preview parameters camera control flag TELE OPENDED",
            __FUNCTION__, __LINE__);
    } else {
        ALOGD("DEBUG(%s[%d]) : zoom preview parameters camera control flag TELE CLOSED",
            __FUNCTION__, __LINE__);
    }

    if (openFlag == 0 && openFlag2 == 0) {
        ALOGE("DEBUG(%s[%d]):Invalid openFlag and openFlag2", __FUNCTION__, __LINE__);
    }

    bfallback = (*map)[PLUGIN_FALLBACK];
    if(bfallback){//to wide
        m_cameraParam.CameraControlFlag |= ARC_DCOZ_SWITCHTOWIDE;
        ALOGD("DEBUG(%s[%d]) : zoom preview parameters camera control flag FALLBACK",
            __FUNCTION__, __LINE__);
    } else {//to tele
        m_cameraParam.CameraControlFlag &= (~ARC_DCOZ_SWITCHTOWIDE);
        ALOGD("DEBUG(%s[%d]) : zoom preview parameters camera control flag NO FALLBACK",
            __FUNCTION__, __LINE__);
    }

    m_cameraParam.CameraControlFlag &= (~ARC_DCOZ_MASTERSESSIONMASK);
    if (0x30 == masterCameraHAL2Arc){
        m_cameraParam.CameraControlFlag |= ARC_DCOZ_WIDESESSION;
        ALOGD("DEBUG(%s[%d]) : zoom preview parameters camera control flag WIDE MASTER",
            __FUNCTION__, __LINE__);
    } else if (0x10 == masterCameraHAL2Arc){
        m_cameraParam.CameraControlFlag |= ARC_DCOZ_TELESESSION;
        ALOGD("DEBUG(%s[%d]) : zoom preview parameters camera control flag TELE MASTER",
            __FUNCTION__, __LINE__);
    } else{
        ALOGE("DEBUG(%s[%d]):Invalid masterCameraHAL2Arc(0x%x)", __FUNCTION__, __LINE__, masterCameraHAL2Arc);
    }

    if (m_buttonZoomMode > 0) {
        m_cameraParam.CameraControlFlag |= ARC_DCOZ_BUTTON2XFLAG;
        ALOGD("DEBUG(%s[%d]) : zoom preview parameters camera control flag BUTTON2XFLAG",
            __FUNCTION__, __LINE__);
    } else {
        m_cameraParam.CameraControlFlag &= (~ARC_DCOZ_BUTTON2XFLAG);
    }
    return ret;
}

status_t ArcsoftFusionZoomPreview::m_getCameraControlFlag(Map_t * map)
{
    status_t ret = NO_ERROR;

    Plugin_DCOZ_CameraParam_t pCameraParam;

    pCameraParam.CameraIndex = m_cameraParam.CameraIndex;
    pCameraParam.ImageShiftX = m_cameraParam.ImageShiftX;
    pCameraParam.ImageShiftY = m_cameraParam.ImageShiftY;

    int whichCamera = (m_cameraParam.CameraControlFlag & ( ARC_DCOZ_WIDECAMERASHOULDOPEN | ARC_DCOZ_TELECAMERASHOULDOPEN ));
    switch(whichCamera){
        case ARC_DCOZ_WIDECAMERASHOULDOPEN:
            pCameraParam.NeedSwitchCamera = PLUGIN_CAMERA_TYPE_WIDE;
            ALOGD("DEBUG(%s[%d]) : zoom preview parameters camera control flag WIDE NEED OPEN",
                __FUNCTION__, __LINE__);
            break;
        case ARC_DCOZ_TELECAMERASHOULDOPEN:
            pCameraParam.NeedSwitchCamera = PLUGIN_CAMERA_TYPE_TELE;
            ALOGD("DEBUG(%s[%d]) : zoom preview parameters camera control flag TELE NEED OPEN",
                __FUNCTION__, __LINE__);
            break;
        case  ARC_DCOZ_WIDECAMERASHOULDOPEN | ARC_DCOZ_TELECAMERASHOULDOPEN :
            pCameraParam.NeedSwitchCamera = PLUGIN_CAMERA_TYPE_BOTH_WIDE_TELE;
            ALOGD("DEBUG(%s[%d]) : zoom preview parameters camera control flag BOTH NEED OPEN",
                __FUNCTION__, __LINE__);
            break;
        default:
            ALOGW("Invalid whichCamera(%d). so, just set PLUGIN_CAMERA_TYPE_WIDE, recommendSwitch");
            pCameraParam.NeedSwitchCamera = PLUGIN_CAMERA_TYPE_WIDE;
            break;
    }

    int recommendSwitch = (m_cameraParam.CameraControlFlag & ARC_DCOZ_RECOMMENDMASK);
    switch(recommendSwitch){
        case ARC_DCOZ_RECOMMENDTOTELE:
            pCameraParam.SwitchTo = RECOMMEND_TO_TELE;
            ALOGD("DEBUG(%s[%d]) : zoom preview parameters camera control flag TELE NEED MASTER",
                __FUNCTION__, __LINE__);
            break;
        case ARC_DCOZ_RECOMMENDTOWIDE:
            pCameraParam.SwitchTo = RECOMMEND_TO_WIDE;
            ALOGD("DEBUG(%s[%d]) : zoom preview parameters camera control flag WIDE NEED MASTER",
                __FUNCTION__, __LINE__);
            break;
        case ARC_DCOZ_RECOMMENDFB:
            pCameraParam.SwitchTo = RECOMMEND_TO_FB;
            ALOGD("DEBUG(%s[%d]) : zoom preview parameters camera control flag FALLBACK NEED MASTER",
                __FUNCTION__, __LINE__);
            pCameraParam.NeedSwitchCamera = PLUGIN_CAMERA_TYPE_WIDE;
            ALOGD("DEBUG(%s[%d]) : zoom preview parameters camera control flag FALLBACK WIDE NEED OPEN",
                __FUNCTION__, __LINE__);
            break;
        default:
            ALOGW("Invalid recommendSwitch(%d). so, just set RECOMMEND_TO_TELE, recommendSwitch");
            pCameraParam.SwitchTo = RECOMMEND_TO_TELE;
            break;
    }

#ifdef ARCSOFT_FUSION_WRAPPER
    //pCameraParam.CameraIndex = ARC_DCOZ_BASEWIDE;

    pCameraParam.ImageShiftX = 8;
    pCameraParam.ImageShiftY = 10;

    int sizeOfList = m_mapSrcZoomRatioListSize[0];

    float SrcZoomRatio = m_mapSrcZoomRatio[0];

    // hack : assume m_mapSrcZoomRatio[0] is 1.69
    SrcZoomRatio = 1.69f;
    m_imageRatio[0] = 1.37f;
    m_imageRatio[1] = 1.0f;

    /*
    for (int i = 0; i < sizeOfList - 1; ++i) {
        if (SrcZoomRatio == g_zoom_list[i * 5 + 0]) {
            m_fWideImageRatio = m_mapOutImageRatioList[0][i];
            m_fTeleImageRatio = m_mapOutImageRatioList[1][i];
            break;
        }
    }
    */
#else
#endif

    m_cameraIndex = pCameraParam.CameraIndex;
    m_needSwitchCamera = pCameraParam.NeedSwitchCamera;
    m_masterCameraArc2HAL = pCameraParam.SwitchTo;

    m_imageShiftX = pCameraParam.ImageShiftX;
    m_imageShiftY = pCameraParam.ImageShiftY;
    m_imageRatio[0] = m_pRatioParam.fWideImageRatio;
    m_imageRatio[1] = m_pRatioParam.fTeleImageRatio;

    return ret;
}

status_t ArcsoftFusionZoomPreview::getFrameParams(Map_t *map)
{
    status_t ret = NO_ERROR;

    MUInt32 format = ASVL_PAF_NV21;

    ALOGD("getFrameParameters");

    if(m_syncType == 1) {
        m_pWideImg.u32PixelArrayFormat = format;
        m_pWideImg.i32Width     = m_mapSrcRect[0][PLUGIN_ARRAY_RECT_W];
        m_pWideImg.i32Height    = m_mapSrcRect[0][PLUGIN_ARRAY_RECT_H];
        m_pWideImg.ppu8Plane[0] = (MUInt8 *)m_mapSrcBufAddr[0][0];
        m_pWideImg.ppu8Plane[1] = (MUInt8 *)m_mapSrcBufAddr[0][1];
        m_pWideImg.pi32Pitch[0] = m_pWideImg.i32Width; // pitch is stride
        m_pWideImg.pi32Pitch[1] = m_pWideImg.pi32Pitch[0];
    }

    if(m_syncType == 3) {
        m_pTeleImg.u32PixelArrayFormat = format;
        m_pTeleImg.i32Width     = m_mapSrcRect[0][PLUGIN_ARRAY_RECT_W];
        m_pTeleImg.i32Height    = m_mapSrcRect[0][PLUGIN_ARRAY_RECT_H];
        m_pTeleImg.ppu8Plane[0] = (MUInt8 *)m_mapSrcBufAddr[0][0];
        m_pTeleImg.ppu8Plane[1] = (MUInt8 *)m_mapSrcBufAddr[0][1];
        m_pTeleImg.pi32Pitch[0] = m_pTeleImg.i32Width; // pitch is stride
        m_pTeleImg.pi32Pitch[1] = m_pTeleImg.pi32Pitch[0];
    }

    if(m_syncType == 2) {
        m_pWideImg.u32PixelArrayFormat = format;
        m_pWideImg.i32Width     = m_mapSrcRect[0][PLUGIN_ARRAY_RECT_W];
        m_pWideImg.i32Height    = m_mapSrcRect[0][PLUGIN_ARRAY_RECT_H];
        m_pWideImg.ppu8Plane[0] = (MUInt8 *)m_mapSrcBufAddr[0][0];
        m_pWideImg.ppu8Plane[1] = (MUInt8 *)m_mapSrcBufAddr[0][1];
        m_pWideImg.pi32Pitch[0] = m_pWideImg.i32Width; // pitch is stride
        m_pWideImg.pi32Pitch[1] = m_pWideImg.pi32Pitch[0];

        m_pTeleImg.u32PixelArrayFormat = format;
        m_pTeleImg.i32Width     = m_mapSrcRect[1][PLUGIN_ARRAY_RECT_W];
        m_pTeleImg.i32Height    = m_mapSrcRect[1][PLUGIN_ARRAY_RECT_H];
        m_pTeleImg.ppu8Plane[0] = (MUInt8 *)m_mapSrcBufAddr[1][0];
        m_pTeleImg.ppu8Plane[1] = (MUInt8 *)m_mapSrcBufAddr[1][1];
        m_pTeleImg.pi32Pitch[0] = m_pTeleImg.i32Width; // pitch is stride
        m_pTeleImg.pi32Pitch[1] = m_pTeleImg.pi32Pitch[0];
    }

    memset(&m_pDstImg, 0, sizeof(ASVLOFFSCREEN));

    m_pDstImg.i32Width     = m_mapDstRect[0][PLUGIN_ARRAY_RECT_W];
    m_pDstImg.i32Height    = m_mapDstRect[0][PLUGIN_ARRAY_RECT_H];
    m_pDstImg.u32PixelArrayFormat = format;
    m_pDstImg.ppu8Plane[0] = (MUInt8*)m_mapDstBufAddr[0];
    m_pDstImg.ppu8Plane[1] = (MUInt8*)m_mapDstBufAddr[1];
    m_pDstImg.pi32Pitch[0] = m_pDstImg.i32Width;
    m_pDstImg.pi32Pitch[1] = m_pDstImg.i32Width; // need to check

    m_iso = (Array_buf_t)(*map)[PLUGIN_ISO_LIST];
    m_cameraParam.ISOWide = m_iso[0];
    m_cameraParam.ISOTele = m_iso[1];

    m_cameraParam.WideFocusFlag = m_mapSrcAfStatus[0];
    m_cameraParam.TeleFocusFlag = m_mapSrcAfStatus[1];

    //platform's file description from map
    m_pFdWideImg.i32FdPlane1 = m_mapSrcBufFd[0][0];
    m_pFdWideImg.i32FdPlane2 = m_mapSrcBufFd[0][1];

    m_pFdTeleImg.i32FdPlane1 = m_mapSrcBufFd[1][0];
    m_pFdTeleImg.i32FdPlane2 = m_mapSrcBufFd[1][1];

    m_pFdResultImg.i32FdPlane1 = m_mapDstBufFd[0][0];
    m_pFdResultImg.i32FdPlane2 = m_mapDstBufFd[0][1];

    int32_t zoomLevel = (Data_int32_t)(*map)[PLUGIN_ZOOM_LEVEL];
    if ((0 <= zoomLevel) && ( zoomLevel < m_zoomMaxSize)) {
        // HAL1
        //m_pRatioParam.fWideViewRatio = m_zoomRatioList[0][zoomLevel];
        //m_pRatioParam.fTeleViewRatio = m_zoomRatioList[0][zoomLevel];
        // HAL3
        m_pRatioParam.fWideViewRatio = m_mapSrcZoomRatio[0];
        m_pRatioParam.fTeleViewRatio = m_mapSrcZoomRatio[0];

        // HAL1
        m_pRatioParam.fWideImageRatio = m_mapOutImageRatioList[0][zoomLevel];
        m_pRatioParam.fTeleImageRatio = m_mapOutImageRatioList[1][zoomLevel];
        // HAL3
        //?

        m_pRatioParam.bNeedWideFullBuffer = m_mapOutNeedMarginList[0][zoomLevel];
        m_pRatioParam.bNeedTeleFullBuffer = m_mapOutNeedMarginList[1][zoomLevel];
    } else {
        ALOGE("Invalid zoomLevel(%d)", zoomLevel);
    }

    m_viewRatio = m_pRatioParam.fWideViewRatio;

#if 0
    ret = ARC_DCVOZ_GetZoomVal(
        m_handle,
        (m_mapSrcZoomRatio[0] / 1000.0f),
        &m_fWideImageRatio,
        &m_fTeleImageRatio,
        &m_i32NeedWideResize,
        &m_i32NeedTeleResize);
    if (MOK != ret){
        ALOGE("ERR ARC_DCVOZ_GetZoomVal ret = %d", ret);
        ret = INVALID_OPERATION;
    }

    m_pRatioParam.fWideViewRatio      = m_mapSrcZoomRatio[0] / 1000.0f;         //[in] The zoom level of user image while getting wide image.
    m_pRatioParam.fWideImageRatio     = m_fWideImageRatio;            //[in] The zoom level of wide image.
    m_pRatioParam.fTeleViewRatio      = m_mapSrcZoomRatio[1] / 1000.0f;         //[in] The zoom level of user image while getting tele image.
    m_pRatioParam.fTeleImageRatio     = m_fTeleImageRatio;            //[in] The zoom level of tele image.
    m_pRatioParam.bNeedWideFullBuffer = m_i32NeedWideResize;          //over margin size percent, e.g. 0(means 100%*previewsize) , 20(means 120%*previewsize)
    m_pRatioParam.bNeedTeleFullBuffer = m_i32NeedTeleResize;
#endif

    return ret;
}

status_t ArcsoftFusionZoomPreview::m_getDualFocusRect(Map_t *map)
{
    status_t ret = NO_ERROR;

    // ArcSoft add for debug, should send to DDK
    MRECT * pTeleRect = (MRECT *)malloc(sizeof(MRECT));
    MRECT * pWideRect = (MRECT *)malloc(sizeof(MRECT));
    // ArcSoft add for debug, should received from app
    MRECT  UserTouchRect;
    UserTouchRect.left = 10;
    UserTouchRect.top = 10;
    UserTouchRect.bottom = 200;
    UserTouchRect.right = 200;
    //ArcSoft add end

    //ArcSoft Note:  m_pRatioParam should be realtime ratio value.
    ALOGD("UserTouchRect :left(%d),top(%d),bottom(%d),right(%d)", UserTouchRect.left, UserTouchRect.top,UserTouchRect.bottom, UserTouchRect.right);

    ret = ARC_DCVOZ_GetTeleRectBaseOnResult(m_handle, &m_pRatioParam,UserTouchRect, pTeleRect);
    if (ret != MOK) {
        ALOGE("ERR(%s[%d]):ARC_DCVOZ_GetTeleRectBaseOnResult() fail", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }


    ret = ARC_DCVOZ_GetWideRectBaseOnResult( m_handle, &m_pRatioParam, UserTouchRect, pWideRect);
    if (ret != MOK) {
        ALOGE("ERR(%s[%d]):ARC_DCVOZ_GetWideRectBaseOnResult() fail", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    ALOGD("m_getDualFocusRect: pWideRect :left(%d),top(%d),bottom(%d),right(%d)", pWideRect->left, pWideRect->top,pWideRect->bottom, pWideRect->right);
    ALOGD("m_getDualFocusRect: pTeleRect :left(%d),top(%d),bottom(%d),right(%d)", pTeleRect->left, pTeleRect->top,pTeleRect->bottom, pTeleRect->right);


    return ret;
}

status_t ArcsoftFusionZoomPreview::m_getFaceResultRect(Map_t *map)
{
    status_t ret = NO_ERROR;
    unsigned int (*Array_fd_master_rect_t)[4];
    unsigned int (*Array_fd_slave_rect_t)[4];
    int numOfDetectedFaces = 0;
    int i = 0;
    MRECT  WideRect;
    MRECT  TeleRect;
    MRECT * pResultRect;
    // ArcSoft add for debug, should send to app

    Array_fd_master_rect_t = (unsigned int (*)[4])(*map)[PLUGIN_MASTER_FACE_RECT];
    Array_fd_slave_rect_t = (unsigned int (*)[4])(*map)[PLUGIN_SLAVE_FACE_RECT];
    numOfDetectedFaces = (int)(*map)[PLUGIN_FACE_NUM];

    ALOGD("m_cameraIndex %d", m_cameraIndex);

    for (i = 0; i < numOfDetectedFaces; i++) {
        WideRect.left = Array_fd_master_rect_t[i][0];
        WideRect.top = Array_fd_master_rect_t[i][1];
        WideRect.right = Array_fd_master_rect_t[i][2];
        WideRect.bottom = Array_fd_master_rect_t[i][3];

        TeleRect.left = Array_fd_slave_rect_t[i][0];
        TeleRect.top = Array_fd_slave_rect_t[i][1];
        TeleRect.right = Array_fd_slave_rect_t[i][2];
        TeleRect.bottom = Array_fd_slave_rect_t[i][3];

        pResultRect = &m_resultRect[i];
        (*pResultRect).left = 0;
        (*pResultRect).top = 0;
        (*pResultRect).right = 0;
        (*pResultRect).bottom = 0;

        if (TeleRect.left == 0 && TeleRect.top == 0 && TeleRect.right == 0 && TeleRect.bottom == 0) {
            TeleRect = WideRect;
        }

        ALOGD("Wide source frame[%d %d][%d %d]", WideRect.left,　WideRect.top, WideRect.right, WideRect.bottom);
        ALOGD("Tele source frame[%d %d][%d %d]", TeleRect.left, TeleRect.top, TeleRect.right, TeleRect.bottom);

        switch (m_cameraIndex) {
            case 2:
                ALOGD("WideRect :left(%d),top(%d),right(%d),bottom(%d)", WideRect.left, WideRect.top,WideRect.right, WideRect.bottom);
                ret = ARC_DCVOZ_GetResultRectBaseOnWide(m_handle, &m_pRatioParam, WideRect, m_pRatioParam.bNeedWideFullBuffer, pResultRect);
                if (ret != MOK) {
                    ALOGE("ERR(%s[%d]):ARC_DCVOZ_GetResultRectBaseOnWide() fail", __FUNCTION__, __LINE__);
                    return INVALID_OPERATION;
                }
                break;
            case 3:
                ALOGD("TeleRect :left(%d),top(%d),right(%d),bottom(%d)", TeleRect.left, TeleRect.top,TeleRect.right, TeleRect.bottom);
                ret = ARC_DCVOZ_GetResultRectBaseOnTele(m_handle, &m_pRatioParam, TeleRect, m_pRatioParam.bNeedTeleFullBuffer, pResultRect);
                if (ret != MOK) {
                    ALOGE("ERR(%s[%d]):ARC_DCVOZ_GetResultRectBaseOnTele() fail", __FUNCTION__, __LINE__);
                    return INVALID_OPERATION;
                }
                break;
            default:
                ALOGW("Invalid m_cameraIndex(%d).", (int)m_cameraIndex);
                break;
        }

        ALOGD("pResultRect :left(%d),top(%d),bottom(%d),right(%d)", pResultRect->left, pResultRect->top, pResultRect->bottom, pResultRect->right);
    }

    (*map)[PLUGIN_RESULT_FACE_RECT] = (Map_data_t)(m_resultRect);

    return ret;
}

void ArcsoftFusionZoomPreview::OpenFileLog()
{
    char szFilename[256] = {0};
    long long curTime = GetCurrentTime();
    snprintf(szFilename, sizeof(szFilename), "/data/misc/cameraserver/%lld_inputValues.txt", g_arc_timestamp);

    remove(szFilename);

    if(NULL != m_LogFile)
    {
        fclose(m_LogFile);
        m_LogFile = NULL;
    }
    m_LogFile = fopen(szFilename, "at+");

}

void ArcsoftFusionZoomPreview::CloseFileLog()
{
    if(NULL != m_LogFile)
    {
        fclose(m_LogFile);
        m_LogFile = NULL;
    }
}

void ArcsoftFusionZoomPreview::WriteFileLog(const char *fmt, ...)
{

    va_list ap;
    char buf[256] = {0};
    va_start(ap, fmt);
    vsnprintf(buf, 256, fmt, ap);
    va_end(ap);

    fwrite(buf, strlen(buf) * sizeof(char), sizeof(char), m_LogFile);
    fwrite("\r\n", 2, 1, m_LogFile);

}

void ArcsoftFusionZoomPreview::dumpYUVtoFile(ASVLOFFSCREEN *pAsvl, char* name_prefix)
{
    ALOGD("ArcsoftFusionZoomPreview----dumpYUVtoFile E");
    char filename[256];
    memset(filename, 0, sizeof(char)*256);

    snprintf(filename, sizeof(filename), "/data/misc/cameraserver/%s_%dx%d.nv21",
    name_prefix, pAsvl->pi32Pitch[0], pAsvl->i32Height);

    int file_fd = open(filename, O_RDWR | O_CREAT, 0777);
    if (file_fd >= 0)
    {
        //fchmod(file_fd, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        ssize_t writen_bytes = 0;
        writen_bytes = write(file_fd, pAsvl->ppu8Plane[0], pAsvl->pi32Pitch[0] * pAsvl->i32Height);//only for NV21 or NV12
        writen_bytes = write(file_fd, pAsvl->ppu8Plane[1], pAsvl->pi32Pitch[1] * (pAsvl->i32Height >> 1));//only for NV21 or NV12
        ALOGD("DEBUG(%s[%d]):fd_close FD(%d)", __FUNCTION__, __LINE__, file_fd);
        close(file_fd);
    }

    ALOGD("ArcsoftFusionZoomPreview----dump frame to file: %s", filename);
}

#ifdef ARCSOFT_FUSION_WRAPPER

ARC_DCVOZ_API MRESULT ARC_DCVOZ_Init(
    MHandle*             phHandle,
    LPARC_DCOZ_INITPARAM pInitParam
    )
{
    return MOK;
}

ARC_DCVOZ_API MRESULT ARC_DCVOZ_Uninit(MHandle* phHandle)
{
    return MOK;
}

ARC_DCVOZ_API MRESULT ARC_DCVOZ_SetZoomList(
    MHandle        hHandle,
    MFloat*        pfWideZoomList,
    MInt32         i32WideZoomListSize,
    MFloat*        pfTeleZoomList,
    MInt32         i32TeleZoomListSize
    )
{
    return MOK;
}

ARC_DCVOZ_API MRESULT ARC_DCVOZ_SetPreviewSize(
    MHandle        hHandle,
    LPARC_DCVOZ_CAMERAIMAGEINFO    pCameraImageInfo,
    MInt32        i32Mode)
{
    return MOK;
}

ARC_DCVOZ_API MRESULT ARC_DCVOZ_GetZoomVal(
    MHandle        hHandle,
    MFloat         fViewRatio,
    MFloat*        pfWideImageRatio,
    MFloat*        pfTeleImageRatio,
    MInt32*        pbNeedWideFullBuffer,
    MInt32*        pbNeedTeleFullBuffer
    )
{
    return MOK;
}

ARC_DCVOZ_API MRESULT ARC_DCVOZ_Process(
    MHandle                    hHandle,
    LPASVLOFFSCREEN            pWideImg,
    LPASVLOFFSCREEN            pTeleImg,
    LPARC_DCVOZ_RATIOPARAM     pRatioParam,
    LPARC_DCVOZ_CAMERAPARAM    pCameraParam,
    LPASVLOFFSCREEN            pDstImg,
    LPARC_DCOZ_IMAGEFD           pFdWideImg,
    LPARC_DCOZ_IMAGEFD           pFdTeleImg,
    LPARC_DCOZ_IMAGEFD           pFdResultImg
    )
{
    return MOK;
}

ARC_DCVOZ_API MRESULT ARC_DCVOZ_GetTeleRectBaseOnResult(
        MHandle hHandle,
        LPARC_DCVOZ_RATIOPARAM pRatioParam,
        MRECT ResultRect,
        MRECT *pTeleRect
        )
{
    return MOK;
}

ARC_DCVOZ_API MRESULT ARC_DCVOZ_GetWideRectBaseOnResult(
        MHandle hHandle,
        LPARC_DCVOZ_RATIOPARAM pRatioParam,
        MRECT ResultRect,
        MRECT *pWideRect
        )
{
    return MOK;
}

ARC_DCVOZ_API MRESULT ARC_DCVOZ_GetResultRectBaseOnWide(
        MHandle hHandle,
        LPARC_DCVOZ_RATIOPARAM pRatioParam,
        MRECT WideRect,
        MInt32 WideNeedMarginInfo,
        MRECT *pResultRect
        )
{
    return MOK;
}

ARC_DCVOZ_API MRESULT ARC_DCVOZ_GetResultRectBaseOnTele(
        MHandle hHandle,
        LPARC_DCVOZ_RATIOPARAM pRatioParam,
        MRECT TeleRect,
        MInt32 TeleNeedMarginInfo,
        MRECT *pResultRect
        )
{
    return MOK;
}

ARC_DCVOZ_API  MRESULT ARC_DCVOZ_GetSwitchModePassParam(
        MHandle hHandle,
        LPARC_DCVOZ_RECORDING_PARAM pPassParam
        )
{
    return MOK;
}

ARC_DCVOZ_API  MRESULT ARC_DCVOZ_SetSwitchModePassParam(
        MHandle hHandle,
        LPARC_DCVOZ_RECORDING_PARAM pPassParam
        )
{
    return MOK;
}

#endif // ARCSOFT_FUSION_WRAPPER
