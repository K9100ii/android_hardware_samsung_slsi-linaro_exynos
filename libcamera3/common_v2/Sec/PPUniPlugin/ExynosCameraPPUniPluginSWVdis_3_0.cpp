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
                                            ExynosCameraImage *dstImage,
                                            ExynosCameraParameters *params)
{
    status_t ret = NO_ERROR;
    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;

    Mutex::Autolock l(m_uniPluginLock);

    if (m_uniPluginHandle == NULL) {
        CLOGE("[VDIS] VDIS Uni plugin(%s) is NULL!!", m_uniPluginName);
        return BAD_VALUE;
    }

    switch (m_VDISstatus) {
    case VDIS_IDLE:
        break;
    case VDIS_RUN:
        /* Start processing */
        m_timer.start();
        if (srcImage[0].buf.index >= 0 && dstImage[0].buf.index >= 0) {
            process(srcImage, dstImage, params);
            getPreviewVDISInfo();
            ret = getVideoOutInfo(dstImage);
        }
        m_timer.stop();
        durationTime = m_timer.durationMsecs();
        if (durationTime > 15) {
            CLOGD("[VDIS] duration time(%5d msec) (%d)", (int)durationTime, srcImage[0].buf.index);
        }
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

    if(m_loadThread != NULL) {
        m_loadThread->join();
    }

    if (m_uniPluginHandle != NULL) {
        UniPluginCameraInfo_t cameraInfo;
        cameraInfo.cameraType = getUniCameraType(m_configurations->getScenario(), m_cameraId);
        cameraInfo.sensorType = (UNI_PLUGIN_SENSOR_TYPE)m_getUniSensorId(m_cameraId);
        cameraInfo.APType = UNI_PLUGIN_AP_TYPE_SLSI;
#ifdef SAMSUNG_SW_VDIS_UHD_20MARGIN
        cameraInfo.fov = 0.83f;
#else
        cameraInfo.fov = 0.95f;
#endif

        CLOGD("Set camera info: %d:%d",
                cameraInfo.cameraType, cameraInfo.sensorType);
        ret = uni_plugin_set(m_uniPluginHandle,
                      m_uniPluginName, UNI_PLUGIN_INDEX_CAMERA_INFO, &cameraInfo);
        if (ret < 0) {
            CLOGE("VDIS Plugin set UNI_PLUGIN_INDEX_CAMERA_INFO failed!!");
        }

        ret = uni_plugin_init(m_uniPluginHandle);
        if (ret < 0) {
            CLOGE("VDIS plugin(%s) init failed!!, ret(%d)", m_uniPluginName, ret);
            return INVALID_OPERATION;
        }
    } else {
        CLOGE("VDIS plugin(%s) is NULL!!", m_uniPluginName);
        return BAD_VALUE;
    }

    return NO_ERROR;
}

void ExynosCameraPPUniPluginSWVdis::m_init(void)
{
    CLOGD(" ");

    strncpy(m_uniPluginName, VDIS_PLUGIN_NAME, EXYNOS_CAMERA_NAME_STR_SIZE - 1);

    m_srcImageCapacity.setNumOfImage(1);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21M);

    m_dstImageCapacity.setNumOfImage(1);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21M);

    m_VDISstatus = VDIS_IDLE;

    memset(&m_swVDIS_rectInfo, 0, sizeof(UTrect));
    memset(&m_swVDIS_pluginData, 0, sizeof(UniPluginBufferData_t));
    memset(&m_swVDIS_outBufInfo, 0, sizeof(UniPluginBufferData_t));
    memset(&m_swVDIS_pluginExtraBufferInfo, 0, sizeof(UniPluginExtraBufferInfo_t));
    memset(&m_swVDIS_Fps, 0, sizeof(UniPluginFPS_t));
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    memset(&m_swVDIS_dualInfo, 0, sizeof(UniPluginDualInfo_t));
#endif
#ifdef SAMSUNG_SW_VDIS_USE_OIS
    m_initSWVdisOISParam();
    m_SWVdisExtCtrlInfo.pParams = NULL;
    m_SWVdisExtCtrlInfo.pParams1 = NULL;
#endif

    m_refCount = 0;
}

status_t ExynosCameraPPUniPluginSWVdis::start(void)
{
    status_t ret = NO_ERROR;

    uint32_t minFps = 0;
    uint32_t maxFps = 0;
    int inWidth = 0;
    int inHeight = 0;
    int outWidth = 0;
    int outHeight = 0;
    int deviceOrientation = 0;
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    int masterCameraId = UNI_PLUGIN_SENSOR_NAME_NONE;
    int slaveCameraId = UNI_PLUGIN_SENSOR_NAME_NONE;
    bool isDualMode = false;
#endif
#ifdef SAMSUNG_SW_VDIS_USE_OIS
    int exposureTime = 0;
    int oisGain = 0;
    UNI_PLUGIN_OIS_MODE oisMode = UNI_PLUGIN_OIS_MODE_VDIS;
    if (getCameraId() == CAMERA_ID_FRONT || getCameraId() == CAMERA_ID_FRONT_1) {
        oisMode = UNI_PLUGIN_OIS_MODE_CENTERING;
    }

    m_initSWVdisOISParam();
#endif

    Mutex::Autolock l(m_uniPluginLock);

    if (m_refCount++ > 0) {
        return ret;
    }

    m_configurations->getSize(CONFIGURATION_VIDEO_SIZE, (uint32_t *)&outWidth, (uint32_t *)&outHeight);
    m_configurations->getPreviewFpsRange(&minFps, &maxFps);

#ifdef SAMSUNG_SW_VDIS
    m_parameters->getSWVdisYuvSize(outWidth, outHeight, (int)maxFps, &inWidth, &inHeight);
#endif

#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    isDualMode = m_configurations->getMode(CONFIGURATION_DUAL_MODE);
    getDualCameraId(&masterCameraId, &slaveCameraId);

    if (isDualMode == true) {
        ExynosRect fusionSrcRect;
        ExynosRect fusionDstRect;
        UNI_PLUGIN_SENSOR_TYPE mainSensorType;
        UNI_PLUGIN_SENSOR_TYPE auxSensorType;

#ifdef USE_DUAL_CAMERA
        if (m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false)
#endif
        {
            m_parameters->getFusionSize(outWidth, outHeight, &fusionSrcRect, &fusionDstRect);
            inWidth = fusionDstRect.w;
            inHeight = fusionDstRect.h;
        }

        mainSensorType = (UNI_PLUGIN_SENSOR_TYPE)m_getUniSensorId(masterCameraId);
        auxSensorType = (UNI_PLUGIN_SENSOR_TYPE)m_getUniAuxSensorId(slaveCameraId);
        m_swVDIS_dualInfo.isDual = isDualMode;
        m_swVDIS_dualInfo.mainSensorType = mainSensorType;
        m_swVDIS_dualInfo.auxSensorType = auxSensorType;
        m_UniPluginSet(UNI_PLUGIN_INDEX_DUAL_INFO, &m_swVDIS_dualInfo);
    }
#endif
    m_configurations->getPreviewFpsRange(&minFps, &maxFps);
    deviceOrientation = m_configurations->getModeValue(CONFIGURATION_DEVICE_ORIENTATION);

    m_swVDIS_pluginData.inWidth = inWidth;
    m_swVDIS_pluginData.inHeight = inHeight;
    m_swVDIS_pluginData.outWidth = outWidth;
    m_swVDIS_pluginData.outHeight = outHeight;

    m_UniPluginSet(UNI_PLUGIN_INDEX_BUFFER_INFO, &m_swVDIS_pluginData);

    memset(&m_swVDIS_pluginExtraBufferInfo, 0, sizeof(UniPluginExtraBufferInfo_t));
    m_swVDIS_pluginExtraBufferInfo.orientation = (UNI_PLUGIN_DEVICE_ORIENTATION)(deviceOrientation / 90);
    m_UniPluginSet(UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO, &m_swVDIS_pluginExtraBufferInfo);

    if (maxFps >= 60 && minFps >= 60) {
        m_swVDIS_Fps.maxFPS.num = 60;
        m_swVDIS_Fps.maxFPS.den = 1;
    } else {
        m_swVDIS_Fps.maxFPS.num = 30;
        m_swVDIS_Fps.maxFPS.den = 1;
    }

    m_UniPluginSet(UNI_PLUGIN_INDEX_FPS_INFO, &m_swVDIS_Fps);

#ifdef SAMSUNG_SW_VDIS_USE_OIS
    //set to master param
    if (m_SWVdisExtCtrlInfo.pParams != NULL) {
        ExynosCameraParameters *pParams = (ExynosCameraParameters *)m_SWVdisExtCtrlInfo.pParams;

        exposureTime = pParams->getSWVdisPreviewFrameExposureTime();
        oisGain = getSWVdisOISGain(exposureTime);
        setSWVdisOISCoef(oisGain);
        pParams->setSWVdisMetaCtlOISCoef(oisGain);

#ifdef USE_DUAL_CAMERA
        if (m_SWVdisExtCtrlInfo.pParams1 != NULL) {
            pParams = (ExynosCameraParameters *)m_SWVdisExtCtrlInfo.pParams1;
            pParams->setSWVdisMetaCtlOISCoef(oisGain);
        }
#endif
    }

    m_UniPluginSet(UNI_PLUGIN_INDEX_OIS_MODE, &oisMode);
    m_UniPluginSet(UNI_PLUGIN_INDEX_OIS_GAIN, &oisGain);
#endif

    ret = m_UniPluginInit();
    if (ret != NO_ERROR) {
        CLOGE("[VDIS] VDIS Plugin init failed!!");
    }

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

    if (m_uniPluginHandle == NULL) {
        CLOGE("[VDIS] VDIS Uni plugin(%s) is NULL!!", m_uniPluginName);
        return BAD_VALUE;
    }

#ifdef SAMSUNG_SW_VDIS_USE_OIS
    /* Stop case */
    if (m_SWVdisExtCtrlInfo.pParams != NULL) {
        ExynosCameraParameters *pParams = (ExynosCameraParameters *)m_SWVdisExtCtrlInfo.pParams;
        pParams->setSWVdisMetaCtlOISCoef(0);
#ifdef USE_DUAL_CAMERA
        if (m_SWVdisExtCtrlInfo.pParams1 != NULL) {
            pParams = (ExynosCameraParameters *)m_SWVdisExtCtrlInfo.pParams1;
            pParams->setSWVdisMetaCtlOISCoef(0);
        }
#endif
    }
#endif

    ret = m_UniPluginDeinit();
    if (ret < 0) {
        CLOGE("[VDIS] VDIS Tracking plugin deinit failed!!");
    }

    m_VDISstatus = VDIS_DEINIT;

    return ret;
}

void ExynosCameraPPUniPluginSWVdis::process(ExynosCameraImage *srcImage,
                                                ExynosCameraImage *dstImage,
                                                ExynosCameraParameters *params)
{
    UniPluginFaceInfo_t faceInfo[NUM_OF_DETECTED_FACES];
    int inBufIndex = -1;
    int outBufIndex = -1;
    int skipFlag = 0;
    int lux = 0;
    int exposureTime = 0;
    unsigned int faceNum = 0;
    camera2_shot_ext *shot_ext = NULL;
    float zoomRatio = 0.0;
#ifdef SAMSUNG_SW_VDIS_USE_OIS
    int oisGain = 0;
#endif
#ifdef SAMSUNG_GYRO
    UniPluginSensorData_t sensorData;
    SensorListenerEvent_t *gyroData = NULL;
#endif

    shot_ext = (struct camera2_shot_ext *)srcImage[0].buf.addr[srcImage[0].buf.getMetaPlaneIndex()];
    memset(&faceInfo, 0, sizeof(UniPluginFaceInfo_t) * NUM_OF_DETECTED_FACES);

    /* face detection information */
    if (getCameraId() == CAMERA_ID_FRONT) {
        if (shot_ext->shot.ctl.stats.faceDetectMode > FACEDETECT_MODE_OFF) {
            ExynosRect vraInputSize;

            if (params->getHwConnectionMode(PIPE_MCSC, PIPE_VRA) == HW_CONNECTION_MODE_M2M) {
                params->getHwVraInputSize(&vraInputSize.w, &vraInputSize.h,
                                                shot_ext->shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS]);
            } else {
                params->getSize(HW_INFO_HW_YUV_SIZE, (uint32_t *)&vraInputSize.w, (uint32_t *)&vraInputSize.h, 0);
            }

            for (int i = 0; i < NUM_OF_DETECTED_FACES; i++) {
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
    }

    m_UniPluginSet(UNI_PLUGIN_INDEX_FACE_INFO, &faceInfo);
    m_UniPluginSet(UNI_PLUGIN_INDEX_FACE_NUM, &faceNum);

    /* lux */
    lux = shot_ext->shot.udm.ae.vendorSpecific[5] / 256;
    if (lux > 20) {
        lux = -1 * (16777216 - lux);     //2^24 = 16777216
    }

    /* UNI_PLUGIN_INDEX_BUFFER_INFO */
    m_swVDIS_pluginData.inBuffY = (char *)srcImage[0].buf.addr[0];
    m_swVDIS_pluginData.inBuffU = (char *)srcImage[0].buf.addr[1];
    m_swVDIS_pluginData.inBuffFd[0] = srcImage[0].buf.fd[0];
    m_swVDIS_pluginData.inBuffFd[1] = srcImage[0].buf.fd[1];

    exposureTime = (int32_t)shot_ext->shot.dm.sensor.exposureTime;

#ifdef SAMSUNG_GYRO
    /* Set sensor Data */
    gyroData = params->getGyroData();
    memset(&sensorData, 0, sizeof(UniPluginSensorData_t));

    sensorData.gyroData.x = gyroData->gyro.x;
    sensorData.gyroData.y = gyroData->gyro.y;
    sensorData.gyroData.z = gyroData->gyro.z;
    sensorData.gyroData.timestamp = gyroData->gyro.timestamp;
    m_UniPluginSet(UNI_PLUGIN_INDEX_SENSOR_DATA, &sensorData);
#endif

#ifdef SAMSUNG_SW_VDIS_USE_OIS
    if (exposureTime == 0) {
        exposureTime = params->getSWVdisPreviewFrameExposureTime();
    }

    oisGain = getSWVdisOISGain(exposureTime);
    if (setSWVdisOISCoef(oisGain) != -1 ) {
        if (m_SWVdisExtCtrlInfo.pParams != NULL) {
            ExynosCameraParameters *pParams = (ExynosCameraParameters *)m_SWVdisExtCtrlInfo.pParams;
            pParams->setSWVdisMetaCtlOISCoef(oisGain);

#ifdef USE_DUAL_CAMERA
            if (m_SWVdisExtCtrlInfo.pParams1 != NULL) {
                pParams = (ExynosCameraParameters *)m_SWVdisExtCtrlInfo.pParams1;
                pParams->setSWVdisMetaCtlOISCoef(oisGain);
            }
#endif
        } else {
            params->setSWVdisMetaCtlOISCoef(oisGain);
        }
        m_UniPluginSet(UNI_PLUGIN_INDEX_OIS_GAIN, &oisGain);
    }
#endif

    zoomRatio = m_configurations->getZoomRatio();
    if (zoomRatio < 1.0f) {
        CLOGD("zoomRatio %f is under 1", zoomRatio);
    }

#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    if (m_swVDIS_dualInfo.isDual == true) {
        UNI_PLUGIN_CAMERA_TYPE cameraType = (UNI_PLUGIN_CAMERA_TYPE)m_configurations->getModeValue(CONFIGURATION_DUAL_DISP_CAM_TYPE);

        m_swVDIS_pluginData.cameraType = cameraType;
        if (cameraType == UNI_PLUGIN_CAMERA_TYPE_TELE) {
            zoomRatio = zoomRatio / 2;
        }
        m_UniPluginSet(UNI_PLUGIN_INDEX_DUAL_INFO, &m_swVDIS_dualInfo);
    }
#endif

    inBufIndex = srcImage[0].buf.index;
    if (srcImage[0].buf.fd[0] == dstImage[0].buf.fd[0]
        && srcImage[0].buf.fd[1] == dstImage[0].buf.fd[1]) {
        /* when the none of recording request */
        outBufIndex = 0xFF;
        skipFlag = 1;
    } else {
        outBufIndex = dstImage[0].buf.index;
    }

    if (outBufIndex < 0) {
        m_swVDIS_pluginData.outBuffY = NULL;
        m_swVDIS_pluginData.outBuffU = NULL;
        m_swVDIS_pluginData.outBuffFd[0] = -1;
        m_swVDIS_pluginData.outBuffFd[1] = -1;
    } else {
        m_swVDIS_pluginData.outBuffY = (char *)dstImage[0].buf.addr[0];
        m_swVDIS_pluginData.outBuffU = (char *)dstImage[0].buf.addr[1];
        m_swVDIS_pluginData.outBuffFd[0] = dstImage[0].buf.fd[0];
        m_swVDIS_pluginData.outBuffFd[1] = dstImage[0].buf.fd[1];
    }

    /* [23:16] output buffer index, [15:8] input buffer index, [7:0] vdis processing skip flag */
    m_swVDIS_pluginData.index = (outBufIndex << 16) | (inBufIndex << 8) | (skipFlag);

    /* m_swVDIS_pluginData.timestamp = srcImage[0].buf.timeStamp; */
    m_swVDIS_pluginData.timestamp = shot_ext->shot.udm.sensor.timeStampBoot;
    m_swVDIS_pluginData.timestampBoot = shot_ext->shot.udm.sensor.timeStampBoot;
    m_UniPluginSet(UNI_PLUGIN_INDEX_BUFFER_INFO, &m_swVDIS_pluginData);

    /* UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO (brightnessValue, exposure time, zoom ratio) */
    m_swVDIS_pluginExtraBufferInfo.brightnessValue.snum = lux;
    m_swVDIS_pluginExtraBufferInfo.exposureTime.snum = exposureTime;
    m_swVDIS_pluginExtraBufferInfo.zoomRatio = zoomRatio;

    m_UniPluginSet(UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO, &m_swVDIS_pluginExtraBufferInfo);

    m_UniPluginProcess();
}

void ExynosCameraPPUniPluginSWVdis::getPreviewVDISInfo(void)
{
    m_swVDIS_outBufInfo.bufferType = UNI_PLUGIN_BUFFER_TYPE_PREVIEW;
    m_UniPluginGet(UNI_PLUGIN_INDEX_CROP_INFO, &m_swVDIS_rectInfo);
    m_UniPluginGet(UNI_PLUGIN_INDEX_BUFFER_INFO, &m_swVDIS_outBufInfo);

#ifdef SAMSUNG_SW_VDIS
    if (m_swVDIS_outBufInfo.timestamp != 0) {
        m_configurations->setSWVdisPreviewOffset(m_swVDIS_rectInfo.left, m_swVDIS_rectInfo.top);
    }
#endif
}

status_t ExynosCameraPPUniPluginSWVdis::getVideoOutInfo(ExynosCameraImage *image)
{
    status_t ret = NO_ERROR;
    int inIndex = -1;
    int outIndex = -1;
    int recFlag = 0;

    m_swVDIS_outBufInfo.bufferType = UNI_PLUGIN_BUFFER_TYPE_RECORDING;
    m_UniPluginGet(UNI_PLUGIN_INDEX_BUFFER_INFO, &m_swVDIS_outBufInfo);

    if (m_swVDIS_outBufInfo.index == -1) {
        /* does not input buffer release. delayed period for VDIS */
        ret = BAD_VALUE;
    } else {
        inIndex = (m_swVDIS_outBufInfo.index >> 8) & 0xFF;
        outIndex = (m_swVDIS_outBufInfo.index >> 16) & 0xFF;
        recFlag = m_swVDIS_outBufInfo.index & 0xFF;
    
        if (outIndex == 0xFF || recFlag == 0) {
            /* no video stream period, but should be release input buffer */
            ret = BAD_VALUE;
        }
    
        image[0].bufferDondeIndex = inIndex;
        image[0].streamTimeStamp = m_swVDIS_outBufInfo.timestamp;
    }

    return ret;
}

#ifdef SAMSUNG_SW_VDIS_USE_OIS
void ExynosCameraPPUniPluginSWVdis::m_initSWVdisOISParam(void)
{
    m_SWVdisOisFadeVal = -1;
    m_SWVdisOisGainFrameCount[0] = 0;
    m_SWVdisOisGainFrameCount[1] = 0;
}

bool ExynosCameraPPUniPluginSWVdis::isSWVdisUHD(void)
{
    bool swVdisResUHD = false;
    int videoWidth = 0;
    int videoHeight = 0;

    m_configurations->getSize(CONFIGURATION_VIDEO_SIZE, (uint32_t *)&videoWidth, (uint32_t *)&videoHeight);
    if (videoWidth == 3840 && videoHeight == 2160) {
        swVdisResUHD = true;
    }

    return swVdisResUHD;
}

int ExynosCameraPPUniPluginSWVdis::setSWVdisOISCoef(uint32_t coef)
{
    int res = -1;

    if (m_configurations->getModeValue(CONFIGURATION_TRANSIENT_ACTION_MODE) == SAMSUNG_ANDROID_CONTROL_TRANSIENT_ACTION_ZOOMING) {
        return res;
    }

    if (m_SWVdisOisFadeVal != coef) {
        m_SWVdisOisFadeVal = coef;
        res = coef;
    }

    return res;
}

int ExynosCameraPPUniPluginSWVdis::getSWVdisOISGain(int exposureTime)
{
    int oisGain = 0;
    uint32_t nMinFps, nMaxFps;
    int nOISGainCountThreshold;
    float fZoomRatio = m_configurations->getZoomRatio();
    UNI_PLUGIN_CAMERA_TYPE cameraType = UNI_PLUGIN_CAMERA_TYPE_WIDE;

#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    cameraType = (UNI_PLUGIN_CAMERA_TYPE)m_configurations->getModeValue(CONFIGURATION_DUAL_DISP_CAM_TYPE);

    if (cameraType == UNI_PLUGIN_CAMERA_TYPE_WIDE || cameraType == UNI_PLUGIN_CAMERA_TYPE_REAR_0) {
        oisGain = (fZoomRatio >= 3.0) ? 100 : 0;
    } else if (cameraType == UNI_PLUGIN_CAMERA_TYPE_TELE) {
        fZoomRatio = fZoomRatio / 2;
        oisGain = (fZoomRatio >= 2.0) ? 100 : 0;
    }
#else
    oisGain = (fZoomRatio >= 3.0) ? 100 : 0;
#endif

    m_configurations->getPreviewFpsRange(&nMinFps, &nMaxFps);

    if (nMinFps >= 60 && nMaxFps >= 60) {
        nOISGainCountThreshold = 120;    // 2 seconds (60fps * 2 = 120frames)
    } else {
        nOISGainCountThreshold = 60;     // 2 seconds (30fps * 2 = 60 frames)
    }

#ifndef SAMSUNG_SW_VDIS_UHD_20MARGIN
    if (isSWVdisUHD() == true) {
        oisGain = 100;
    } else
#endif
    {
        if (oisGain == 0) {
            int gainIdx = 0;
            int lowLightExpTimeThresold = 0;

#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
            if (cameraType == UNI_PLUGIN_CAMERA_TYPE_TELE) {
                lowLightExpTimeThresold = LOW_LIGHT_EXPOSURE_TIME_THRES_TELE;
            } else
#endif
            {
                lowLightExpTimeThresold = LOW_LIGHT_EXPOSURE_TIME_THRES_WIDE;
            }

            if (exposureTime >= lowLightExpTimeThresold) {
                m_SWVdisOisGainFrameCount[0] = 0;
                gainIdx = 1;
            } else {
                m_SWVdisOisGainFrameCount[1] = 0;
                gainIdx = 0;
            }

            if (m_SWVdisOisFadeVal == -1) {
                m_SWVdisOisGainFrameCount[gainIdx] = nOISGainCountThreshold;
            }

            if (++m_SWVdisOisGainFrameCount[gainIdx] >= nOISGainCountThreshold) {
                m_SWVdisOisGainFrameCount[gainIdx] = nOISGainCountThreshold;
            }

            if (m_SWVdisOisGainFrameCount[0] == nOISGainCountThreshold) {
                oisGain = 0;
            } else if (m_SWVdisOisGainFrameCount[1] == nOISGainCountThreshold) {
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
                oisGain = 60;
#else
                oisGain = 50;
#endif
            } else {
                oisGain = m_SWVdisOisFadeVal;
            }
        } else {
            m_SWVdisOisGainFrameCount[0] = nOISGainCountThreshold;
            m_SWVdisOisGainFrameCount[1] = nOISGainCountThreshold;
        }
    }

    return oisGain;
}

status_t ExynosCameraPPUniPluginSWVdis::m_extControl(int controlType, void *data)
{
    switch(controlType) {
    case PP_EXT_CONTROL_VDIS:
        memcpy((void *)(&m_SWVdisExtCtrlInfo), data, sizeof(struct swVdisExtCtrlInfo));
        break;
    default:
        break;
    }

    return NO_ERROR;
}
#endif
