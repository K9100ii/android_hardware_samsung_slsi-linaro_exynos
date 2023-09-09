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
#define LOG_TAG "ExynosCameraPPUniPluginHyperlapse"

#include "ExynosCameraPPUniPluginHyperlapse.h"


ExynosCameraPPUniPluginHyperlapse::~ExynosCameraPPUniPluginHyperlapse()
{
}

status_t ExynosCameraPPUniPluginHyperlapse::m_draw(ExynosCameraImage *srcImage,
                                            ExynosCameraImage *dstImage,
                                            ExynosCameraParameters *params)
{
    status_t ret = NO_ERROR;
    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;

    Mutex::Autolock l(m_uniPluginLock);

    if (m_uniPluginHandle == NULL) {
        CLOGE("[Hyperlapse] Hyperlapse Uni plugin(%s) is NULL!!", m_uniPluginName);
        return BAD_VALUE;
    }

    switch (m_hyperlapse_Status) {
    case HYPERLAPSE_IDLE:
        break;
    case HYPERLAPSE_RUN:
        m_timer.start();
        ret = process(srcImage, dstImage, params);
        m_timer.stop();
        durationTime = m_timer.durationMsecs();
        if (durationTime > 15) {
            CLOGD("[HYPERLAPSE] duration time(%5d msec) (%d)", (int)durationTime, srcImage[0].buf.index);
        }
        break;
    default:
        break;
    }

    return ret;
}

status_t ExynosCameraPPUniPluginHyperlapse::m_UniPluginInit(void)
{
    status_t ret = NO_ERROR;

    if(m_loadThread != NULL) {
        m_loadThread->join();
    }

    if (m_uniPluginHandle != NULL) {
        UniPluginCameraInfo_t cameraInfo;
        cameraInfo.cameraType = getUniCameraType(m_configurations->getScenario(), m_cameraId);
        cameraInfo.sensorType = (UNI_PLUGIN_SENSOR_TYPE)getSensorId(m_cameraId);
        cameraInfo.APType = UNI_PLUGIN_AP_TYPE_SLSI;

        CLOGD("Set camera info: %d:%d",
                cameraInfo.cameraType, cameraInfo.sensorType);
        ret = uni_plugin_set(m_uniPluginHandle,
                      m_uniPluginName, UNI_PLUGIN_INDEX_CAMERA_INFO, &cameraInfo);
        if (ret < 0) {
            CLOGE("Hyperlapse Plugin set UNI_PLUGIN_INDEX_CAMERA_INFO failed!!");
        }

        ret = uni_plugin_init(m_uniPluginHandle);
        if (ret < 0) {
            CLOGE("Hyperlapse plugin(%s) init failed!!, ret(%d)", m_uniPluginName, ret);
            return INVALID_OPERATION;
        }
    } else {
        CLOGE("Hyperlapse plugin(%s) is NULL!!", m_uniPluginName);
        return BAD_VALUE;
    }

    return NO_ERROR;
}

void ExynosCameraPPUniPluginHyperlapse::m_init(void)
{
    CLOGD(" ");

    strncpy(m_uniPluginName, HYPER_MOTION_PLUGIN_NAME, EXYNOS_CAMERA_NAME_STR_SIZE - 1);

    m_srcImageCapacity.setNumOfImage(1);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21M);

    m_dstImageCapacity.setNumOfImage(1);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21M);

    m_hyperlapse_Status = HYPERLAPSE_IDLE;

    memset(&m_hyperlapse_pluginData, 0, sizeof(UniPluginBufferData_t));
    memset(&m_hyperlapse_pluginExtraBufferInfo, 0, sizeof(UniPluginExtraBufferInfo_t));

    m_hyperlapseTimeStamp = 0;
    m_hyperlapseTimeBTWFrames = 0;

    m_refCount = 0;
}

status_t ExynosCameraPPUniPluginHyperlapse::start(void)
{
    status_t ret = NO_ERROR;

    UNI_PLUGIN_OPERATION_MODE recordingSpeed = UNI_PLUGIN_OP_AUTO_RECORDING;
    int inWidth = 0;
    int inHeight = 0;
    int outWidth = 0;
    int outHeight = 0;
#ifdef SAMSUNG_HYPERLAPSE
    uint32_t minFps = 0;
    uint32_t maxFps = 0;
#endif

    Mutex::Autolock l(m_uniPluginLock);

    if (m_refCount++ > 0) {
        return ret;
    }

    m_configurations->getSize(CONFIGURATION_VIDEO_SIZE, (uint32_t *)&outWidth, (uint32_t *)&outHeight);
#ifdef SAMSUNG_HYPERLAPSE
    m_configurations->getPreviewFpsRange(&minFps, &maxFps);
    m_parameters->getHyperlapseYuvSize(outWidth, outHeight, (int)maxFps, &inWidth, &inHeight);
#endif

    m_hyperlapse_pluginData.inWidth = inWidth;
    m_hyperlapse_pluginData.inHeight = inHeight;
    m_hyperlapse_pluginData.outWidth = outWidth;
    m_hyperlapse_pluginData.outHeight = outHeight;
    m_UniPluginSet(UNI_PLUGIN_INDEX_BUFFER_INFO, &m_hyperlapse_pluginData);

    m_hyperlapseTimeStamp = 0;

    UniPluginFPS_t fps;
    m_UniPluginGet(UNI_PLUGIN_INDEX_FPS_INFO, &fps);
    if (fps.maxFPS.num && fps.maxFPS.den) {
        m_hyperlapseTimeBTWFrames = ((nsecs_t)1000000000U * fps.maxFPS.den) / fps.maxFPS.num;
        CLOGD("[Hyperlapse] m_hyperlapseTimeBTWFrames is %lld", m_hyperlapseTimeBTWFrames);
    } else {
        CLOGE("[Hyperlapse] fps getting is 0!! default set it to 30");
        m_hyperlapseTimeBTWFrames = 33333333;
    }

#ifdef SAMSUNG_HYPERLAPSE
    switch(m_configurations->getModeValue(CONFIGURATION_HYPERLAPSE_SPEED)) {
        case 1:
            recordingSpeed = UNI_PLUGIN_OP_4X_RECORDING;
            break;
        case 2:
            recordingSpeed = UNI_PLUGIN_OP_8X_RECORDING;
            break;
        case 3:
            recordingSpeed = UNI_PLUGIN_OP_16X_RECORDING;
            break;
        case 4:
            recordingSpeed = UNI_PLUGIN_OP_32X_RECORDING;
            break;
        default:
            recordingSpeed = UNI_PLUGIN_OP_AUTO_RECORDING;
            break;
    }
#endif
    m_UniPluginSet(UNI_PLUGIN_INDEX_OPERATION_MODE, &recordingSpeed);

    ret = m_UniPluginInit();
    if (ret != NO_ERROR) {
        CLOGE("[Hyperlapse] Hyperlapse Plugin init failed!!");
    }

    m_hyperlapse_Status = HYPERLAPSE_RUN;

    return ret;
}

status_t ExynosCameraPPUniPluginHyperlapse::stop(bool suspendFlag)
{
    status_t ret = NO_ERROR;

    Mutex::Autolock l(m_uniPluginLock);

    if (--m_refCount > 0) {
        return ret;
    }

    CLOGD("[Hyperlapse]");

    if (m_uniPluginHandle == NULL) {
        CLOGE("[Hyperlapse] Hyperlapse Uni plugin(%s) is NULL!!", m_uniPluginName);
        return BAD_VALUE;
    }

    /* Stop case */
    ret = m_UniPluginDeinit();
    if (ret < 0) {
        CLOGE("[Hyperlapse] Hyperlapse Tracking plugin deinit failed!!");
    }

    m_hyperlapse_Status = HYPERLAPSE_IDLE;

    return ret;
}

status_t ExynosCameraPPUniPluginHyperlapse::process(ExynosCameraImage *srcImage,
                                                        ExynosCameraImage *dstImage,
                                                        ExynosCameraParameters *params)
{
    status_t ret = NO_ERROR;

    UniPluginFaceInfo_t faceInfo[ABLE_FACECNT];
    UNI_PLUGIN_OPERATION_MODE recordingSpeed = UNI_PLUGIN_OP_AUTO_RECORDING;
    int inBufIndex = -1;
    int outBufIndex = -1;
    int outQueueIndex = -1;
    unsigned int faceNum = 0;
    int lux = 0;
    camera2_shot_ext *shot_ext = NULL;

    if (srcImage[0].buf.fd[0] == dstImage[0].buf.fd[0]
        && srcImage[0].buf.fd[1] == dstImage[0].buf.fd[1]) {
        return BAD_VALUE;
    }

    shot_ext = (struct camera2_shot_ext *)srcImage[0].buf.addr[srcImage[0].buf.getMetaPlaneIndex()];

    /* hyperlapse timestamp init */
    if (m_hyperlapseTimeStamp == 0) {
        m_hyperlapseTimeStamp = shot_ext->shot.udm.sensor.timeStampBoot;
    }

    /* face detection information */
    memset(&faceInfo, 0, sizeof(UniPluginFaceInfo_t) * ABLE_FACECNT);

    if (shot_ext->shot.ctl.stats.faceDetectMode > FACEDETECT_MODE_OFF) {
        ExynosRect vraInputSize;

        if (params->getHwConnectionMode(PIPE_MCSC, PIPE_VRA) == HW_CONNECTION_MODE_M2M) {
            params->getHwVraInputSize(&vraInputSize.w, &vraInputSize.h,
                                            shot_ext->shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS]);
        } else {
            params->getSize(HW_INFO_HW_YUV_SIZE, (uint32_t *)&vraInputSize.w, (uint32_t *)&vraInputSize.h, 0);
        }

        for (int i = 0; i < ABLE_FACECNT; i++) {
            if (shot_ext->shot.dm.stats.faceIds[i]) {
                faceInfo[i].index = i;
                faceInfo[i].faceROI.left    = calibratePosition(vraInputSize.w, srcImage[0].rect.w, shot_ext->shot.dm.stats.faceRectangles[i][0]);
                faceInfo[i].faceROI.top     = calibratePosition(vraInputSize.h, srcImage[0].rect.h, shot_ext->shot.dm.stats.faceRectangles[i][1]);
                faceInfo[i].faceROI.right   = calibratePosition(vraInputSize.w, srcImage[0].rect.w, shot_ext->shot.dm.stats.faceRectangles[i][2]);
                faceInfo[i].faceROI.bottom  = calibratePosition(vraInputSize.h, srcImage[0].rect.h, shot_ext->shot.dm.stats.faceRectangles[i][3]);
                faceNum++;
            }
        }
    }

    m_UniPluginSet(UNI_PLUGIN_INDEX_FACE_INFO, &faceInfo);
    m_UniPluginSet(UNI_PLUGIN_INDEX_FACE_NUM, &faceNum);

    /* lux */
    lux = shot_ext->shot.udm.ae.vendorSpecific[5] / 256;
    if (lux > 20) {
        lux = -1 * (16777216 - lux);     //2^24 = 16777216
    }

    /* UNI_PLUGIN_INDEX_BUFFER_INFO */
    m_hyperlapse_pluginData.inBuffY = (char *)srcImage[0].buf.addr[0];
    m_hyperlapse_pluginData.inBuffU = (char *)srcImage[0].buf.addr[1];

    inBufIndex = srcImage[0].buf.index;
    outBufIndex = dstImage[0].buf.index;

    if (outBufIndex < 0) {
        m_hyperlapse_pluginData.outBuffY = NULL;
        m_hyperlapse_pluginData.outBuffU = NULL;
    } else {
        m_hyperlapse_pluginData.outBuffY = (char *)dstImage[0].buf.addr[0];
        m_hyperlapse_pluginData.outBuffU = (char *)dstImage[0].buf.addr[1];
    }

    m_hyperlapse_pluginData.timestamp = shot_ext->shot.udm.sensor.timeStampBoot;
    m_UniPluginSet(UNI_PLUGIN_INDEX_BUFFER_INFO, &m_hyperlapse_pluginData);

    m_hyperlapse_pluginExtraBufferInfo.exposureTime.snum = (int32_t)shot_ext->shot.dm.sensor.exposureTime;
    m_hyperlapse_pluginExtraBufferInfo.zoomRatio = m_configurations->getZoomRatio();
    m_hyperlapse_pluginExtraBufferInfo.orientation = (UNI_PLUGIN_DEVICE_ORIENTATION)m_configurations->getModeValue(CONFIGURATION_DEVICE_ORIENTATION);
    m_hyperlapse_pluginExtraBufferInfo.brightnessValue.snum = lux;
    m_hyperlapse_pluginExtraBufferInfo.exposureValue.num = shot_ext->shot.udm.ae.vendorSpecific[4];
    m_hyperlapse_pluginExtraBufferInfo.exposureValue.den = 256;
    m_UniPluginSet(UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO, &m_hyperlapse_pluginExtraBufferInfo);

#ifdef SAMSUNG_HYPERLAPSE
    switch(m_configurations->getModeValue(CONFIGURATION_HYPERLAPSE_SPEED)) {
        case 1:
            recordingSpeed = UNI_PLUGIN_OP_4X_RECORDING;
            break;
        case 2:
            recordingSpeed = UNI_PLUGIN_OP_8X_RECORDING;
            break;
        case 3:
            recordingSpeed = UNI_PLUGIN_OP_16X_RECORDING;
            break;
        case 4:
            recordingSpeed = UNI_PLUGIN_OP_32X_RECORDING;
            break;
        default:
            recordingSpeed = UNI_PLUGIN_OP_AUTO_RECORDING;
            break;
    }
#endif
    m_UniPluginSet(UNI_PLUGIN_INDEX_OPERATION_MODE, &recordingSpeed);

    m_UniPluginProcess();

    UTpoll hyperlapse_FrameToEncodeStatus = UTprocessing;
    outQueueIndex = dstImage[0].buf.index;
    m_UniPluginSet(UNI_PLUGIN_INDEX_BUFFER_INDEX, &outQueueIndex);
    m_UniPluginGet(UNI_PLUGIN_INDEX_POLLING, &hyperlapse_FrameToEncodeStatus);

    if (hyperlapse_FrameToEncodeStatus == UTsuccess) {
        m_hyperlapseTimeStamp += m_hyperlapseTimeBTWFrames;
        dstImage[0].streamTimeStamp = m_hyperlapseTimeStamp;
    } else {
        ret = BAD_VALUE;
    }

    return ret;
}

