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

/*#define LOG_NDEBUG 0 */
#define LOG_TAG "ExynosCameraPPUniPluginSWVdis"

#include "ExynosCameraPPUniPluginSWVdis_3_0.h"


ExynosCameraPPUniPluginSWVdis::~ExynosCameraPPUniPluginSWVdis()
{
}

status_t ExynosCameraPPUniPluginSWVdis::m_draw(ExynosCameraImage *srcImage,
                                            ExynosCameraImage *dstImage)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    status_t ret = NO_ERROR;
    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;

    Mutex::Autolock l(m_uniPluginLock);

    if (m_UniPluginHandle == NULL) {
        CLOGE("[VDIS] VDIS Uni plugin(%s) is NULL!!", m_UniPluginName);
        return BAD_VALUE;
    }

    switch (m_VDISstatus) {
    case VDIS_IDLE:
        break;
    case VDIS_RUN:
        /* Start processing */
        m_timer.start();
        if (srcImage[0].buf.index >= 0 && dstImage[0].buf.index >= 0) {
            process(srcImage, dstImage);
            getPreviewVDISInfo();
            getVideoOutInfo();
        } else {
#ifdef SAMSUNG_SW_VDIS
            m_parameters->setSWVdisVideoIndex(-1);
            //getVideoOutInfo();
#endif
        }
        m_timer.stop();
        durationTime = m_timer.durationMsecs();

        CLOGD("[VDIS] duration time(%5d msec) (%d)", (int)durationTime, srcImage[0].buf.index);
        break;
    case VDIS_DEINIT:
        /* Start processing */
        CLOGD("[VDIS] VDIS_DEINIT");
        break;
    default:
        break;
    }

    return ret;
}

status_t ExynosCameraPPUniPluginSWVdis::m_UniPluginInit(void)
{
    status_t ret = NO_ERROR;

    if (m_UniPluginHandle != NULL) {
        UniPluginCameraInfo_t cameraInfo;
        cameraInfo.CameraType = (UNI_PLUGIN_CAMERA_TYPE)m_cameraId;
        cameraInfo.SensorType = (UNI_PLUGIN_SENSOR_TYPE)getSensorId(m_cameraId);
        cameraInfo.APType = UNI_PLUGIN_AP_TYPE_SLSI;

        CLOGD("Set camera info: %d:%d",
                cameraInfo.CameraType, cameraInfo.SensorType);
        ret = uni_plugin_set(m_UniPluginHandle,
                      m_UniPluginName, UNI_PLUGIN_INDEX_CAMERA_INFO, &cameraInfo);
        if (ret < 0) {
            CLOGE("VDIS Plugin set UNI_PLUGIN_INDEX_CAMERA_INFO failed!!");
        }

        ret = uni_plugin_init(m_UniPluginHandle);
        if (ret < 0) {
            CLOGE("VDIS plugin(%s) init failed!!, ret(%d)", m_UniPluginName, ret);
            return INVALID_OPERATION;
        }
    } else {
        CLOGE("VDIS plugin(%s) is NULL!!", m_UniPluginName);
        return BAD_VALUE;
    }

    return NO_ERROR;
}

void ExynosCameraPPUniPluginSWVdis::m_init(void)
{
    CLOGD(" ");

    strncpy(m_UniPluginName, VDIS_PLUGIN_NAME, EXYNOS_CAMERA_NAME_STR_SIZE - 1);

    m_srcImageCapacity.setNumOfImage(1);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21M);

    m_dstImageCapacity.setNumOfImage(1);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21M);

    m_VDISstatus = VDIS_IDLE;

    m_refCount = 0;
}

status_t ExynosCameraPPUniPluginSWVdis::start(void)
{
    status_t ret = NO_ERROR;

    Mutex::Autolock l(m_uniPluginLock);

    if (m_refCount++ > 0) {
        return ret;
    }

    m_parameters->getVideoSize(&m_swVDIS_OutW, &m_swVDIS_OutH);
#ifdef SAMSUNG_SW_VDIS
    m_parameters->getSWVdisYuvSize(m_swVDIS_OutW, m_swVDIS_OutH, &m_swVDIS_InW, &m_swVDIS_InH);
#endif
    m_parameters->getPreviewFpsRange(&m_swVDIS_MinFps, &m_swVDIS_MaxFps);
    m_swVDIS_DeviceOrientation = m_parameters->getDeviceOrientation();
#ifdef SAMSUNG_OIS_VDIS
    m_swVDIS_OISMode = UNI_PLUGIN_OIS_MODE_VDIS;
    m_swVDIS_OISGain = m_parameters->getOISGain();
    m_OISvdisMode = UNI_PLUGIN_OIS_MODE_END;
#endif

    m_swVDIS_Mode = true;
    m_swVDIS_Delay = 21;
    m_swVDIS_FrameNum = 0;

    m_swVDIS_rectInfo.left = 0;
    m_swVDIS_rectInfo.top = 0;
    m_swVDIS_outBufInfo.Timestamp = 0;

    m_swVDIS_NextOutBufIdx = 0;
    m_swVDIS_OutW = m_swVDIS_InW;
    m_swVDIS_OutH = m_swVDIS_InH;
#ifdef SAMSUNG_SW_VDIS
    m_parameters->getSWVdisAdjustYuvSize(&m_swVDIS_OutW, &m_swVDIS_OutH);
#endif

    memset(&m_swVDIS_pluginData, 0, sizeof(UniPluginBufferData_t));
    m_swVDIS_pluginData.InWidth = m_swVDIS_InW;
    m_swVDIS_pluginData.InHeight = m_swVDIS_InH;
    m_swVDIS_pluginData.OutWidth = m_swVDIS_OutW;
    m_swVDIS_pluginData.OutHeight = m_swVDIS_OutH;

    m_UniPluginSet(UNI_PLUGIN_INDEX_BUFFER_INFO, &m_swVDIS_pluginData);

    memset(&m_swVDIS_pluginExtraBufferInfo, 0, sizeof(UniPluginExtraBufferInfo_t));
    m_swVDIS_pluginExtraBufferInfo.orientation = (UNI_PLUGIN_DEVICE_ORIENTATION)(m_swVDIS_DeviceOrientation / 90);
    m_UniPluginSet(UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO, &m_swVDIS_pluginExtraBufferInfo);

    memset(&m_swVDIS_Fps, 0, sizeof(UniPluginFPS_t));
    if (m_swVDIS_MaxFps >= 60 && m_swVDIS_MinFps >= 60) {
        m_swVDIS_Fps.maxFPS.num = 60;
        m_swVDIS_Fps.maxFPS.den = 1;
    } else {
        m_swVDIS_Fps.maxFPS.num = 30;
        m_swVDIS_Fps.maxFPS.den = 1;
    }

    m_UniPluginSet(UNI_PLUGIN_INDEX_FPS_INFO, &m_swVDIS_Fps);
#ifdef SAMSUNG_OIS_VDIS
    m_UniPluginSet(UNI_PLUGIN_INDEX_OIS_MODE, &m_swVDIS_OISMode);
    m_UniPluginSet(UNI_PLUGIN_INDEX_OIS_GAIN, &m_swVDIS_OISGain);
#endif

    ret = m_UniPluginInit();
    if (ret != NO_ERROR) {
        CLOGE("[VDIS] VDIS Plugin init failed!!");
    }

    //ALOGD("VDIS_HAL_INIT: VS_INIT_DONE: Input: %d x %d  Output: %d x %d Delay: %d NUM_InBuffer: %d",
    //    m_swVDIS_InW, m_swVDIS_InH, m_swVDIS_OutW, m_swVDIS_OutH, m_swVDIS_Delay, NUM_VDIS_BUFFERS);

    m_VDISstatus = VDIS_RUN;

    return ret;
}

status_t ExynosCameraPPUniPluginSWVdis::stop(bool suspendFlag)
{
    status_t ret = NO_ERROR;

    Mutex::Autolock l(m_uniPluginLock);

    if (--m_refCount > 0) {
        return ret;
    }

    CLOGD("[VDIS]");

    if (m_UniPluginHandle == NULL) {
        CLOGE("[VDIS] VDIS Uni plugin(%s) is NULL!!", m_UniPluginName);
        return BAD_VALUE;
    }

    /* Stop case */
    ret = m_UniPluginDeinit();
    if (ret < 0) {
        CLOGE("[VDIS] VDIS Tracking plugin deinit failed!!");
    }

    m_swVDIS_Mode = false;
    m_swVDIS_FrameNum = 0;

#ifdef SAMSUNG_OIS_VDIS
    m_swVDIS_OISGain = 0;
#endif

    m_VDISstatus = VDIS_DEINIT;

    return ret;
}

void ExynosCameraPPUniPluginSWVdis::process(ExynosCameraImage *srcImage, ExynosCameraImage *dstImage)
{
    camera2_shot_ext *shot_ext = NULL;
    shot_ext = (struct camera2_shot_ext *)srcImage[0].buf.addr[srcImage[0].buf.getMetaPlaneIndex()];

    //face detection information
    m_swVDIS_pluginFaceNum = 0;
    memset(&m_swVDIS_pluginFaceInfo, 0, sizeof(UniPluginFaceInfo_t) * NUM_OF_DETECTED_FACES);

    if (shot_ext->shot.dm.stats.faceDetectMode != FACEDETECT_MODE_OFF) {
        for (int i = 0; i < NUM_OF_DETECTED_FACES; i++) {
            if (shot_ext->shot.dm.stats.faceIds[i]) {
                m_swVDIS_pluginFaceInfo[i].index = i;
                m_swVDIS_pluginFaceInfo[i].FaceROI.left    = shot_ext->shot.dm.stats.faceRectangles[i][0];
                m_swVDIS_pluginFaceInfo[i].FaceROI.top     = shot_ext->shot.dm.stats.faceRectangles[i][1];
                m_swVDIS_pluginFaceInfo[i].FaceROI.right   = shot_ext->shot.dm.stats.faceRectangles[i][2];
                m_swVDIS_pluginFaceInfo[i].FaceROI.bottom  = shot_ext->shot.dm.stats.faceRectangles[i][3];
                m_swVDIS_pluginFaceNum++;
            }
        }
    }

    m_UniPluginSet(UNI_PLUGIN_INDEX_FACE_INFO, &m_swVDIS_pluginFaceInfo);
    m_UniPluginSet(UNI_PLUGIN_INDEX_FACE_NUM, &m_swVDIS_pluginFaceNum);

    //lux
    m_swVDIS_Lux = shot_ext->shot.udm.ae.vendorSpecific[5] / 256;
    if (m_swVDIS_Lux > 20) {
        m_swVDIS_Lux = -1 * (16777216 - m_swVDIS_Lux);     //2^24 = 16777216
    }

    //UNI_PLUGIN_INDEX_BUFFER_INFO
    m_swVDIS_pluginData.InBuffY = (char *)srcImage[0].buf.addr[0];
    m_swVDIS_pluginData.InBuffU = (char *)srcImage[0].buf.addr[1];
    m_swVDIS_pluginData.InBuffFd[0] = srcImage[0].buf.fd[0];
    m_swVDIS_pluginData.InBuffFd[1] = srcImage[0].buf.fd[1];

#ifdef SAMSUNG_OIS_VDIS
    m_UniPluginSet(UNI_PLUGIN_INDEX_OIS_GAIN, &oisGain);
#endif

    if (dstImage[0].buf.index < 0) {
        m_swVDIS_pluginData.OutBuffY = NULL;
        m_swVDIS_pluginData.OutBuffU = NULL;
    } else {
        m_swVDIS_pluginData.OutBuffY = (char *)dstImage[0].buf.addr[0];
        m_swVDIS_pluginData.OutBuffU = (char *)dstImage[0].buf.addr[1];
        m_swVDIS_pluginData.OutBuffFd[0] = dstImage[0].buf.fd[0];
        m_swVDIS_pluginData.OutBuffFd[1] = dstImage[0].buf.fd[1];
    }

    int inBufIndex = srcImage[0].buf.index;
    int skipFlag = (dstImage[0].buf.index < 0) ? (1) : (0);

    //[31:16] output buffer index, [15:8] input buffer index, [7:0] vdis processing skip flag
    m_swVDIS_pluginData.Index = (dstImage[0].buf.index << 16) | (inBufIndex << 8) | (skipFlag);

    #if 0   //temp
    if (dstImage[0].index >= 0) {
        m_swVDIS_NextOutBufIdx = (m_swVDIS_NextOutBufIdx + 1) % m_swVDIS_OutBufferNum;
    }
    #endif

    //m_swVDIS_pluginData.Timestamp = srcImage[0].buf.timeStamp;
    m_swVDIS_pluginData.Timestamp = shot_ext->shot.udm.sensor.timeStampBoot;    //temp
    m_swVDIS_pluginData.TimestampBoot = shot_ext->shot.udm.sensor.timeStampBoot;
    m_swVDIS_pluginData.BrightnessLux = m_swVDIS_Lux;
    m_UniPluginSet(UNI_PLUGIN_INDEX_BUFFER_INFO, &m_swVDIS_pluginData);

    //UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO (exposure time, zoom ratio)
    m_swVDIS_pluginExtraBufferInfo.exposureTime.snum = (int32_t)shot_ext->shot.dm.sensor.exposureTime;
    m_swVDIS_pluginExtraBufferInfo.zoomRatio = m_parameters->getZoomRatio();
    m_UniPluginSet(UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO, &m_swVDIS_pluginExtraBufferInfo);

    m_UniPluginProcess();

    m_swVDIS_FrameNum++;
}

void ExynosCameraPPUniPluginSWVdis::getPreviewVDISInfo(void)
{
    m_swVDIS_outBufInfo.bufferType = UNI_PLUGIN_BUFFER_TYPE_PREVIEW;
    m_UniPluginGet(UNI_PLUGIN_INDEX_CROP_INFO, &m_swVDIS_rectInfo);
    m_UniPluginGet(UNI_PLUGIN_INDEX_BUFFER_INFO, &m_swVDIS_outBufInfo);

#ifdef SAMSUNG_SW_VDIS
    if (m_swVDIS_outBufInfo.Timestamp != 0) {
        m_parameters->setSWVdisPreviewOffset(m_swVDIS_rectInfo.left, m_swVDIS_rectInfo.top);
    }
#endif
}

void ExynosCameraPPUniPluginSWVdis::getVideoOutInfo(void)
{
    int outIndex = -1;

    m_swVDIS_outBufInfo.bufferType = UNI_PLUGIN_BUFFER_TYPE_RECORDING;
    m_UniPluginGet(UNI_PLUGIN_INDEX_BUFFER_INFO, &m_swVDIS_outBufInfo);

    if (m_swVDIS_outBufInfo.Index >= 0) {
        //*timeStamp = (nsecs_t)m_swVDIS_outBufInfo.Timestamp;
        outIndex = (m_swVDIS_outBufInfo.Index >> 16) & 0xFF;
    }

#ifdef SAMSUNG_SW_VDIS
    m_parameters->setSWVdisVideoIndex(outIndex);
#endif
}
