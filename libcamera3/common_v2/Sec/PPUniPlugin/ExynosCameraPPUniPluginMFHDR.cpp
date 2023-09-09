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
#define LOG_TAG "ExynosCameraPPUniPluginMFHDR"

#include "ExynosCameraPPUniPluginMFHDR.h"

ExynosCameraPPUniPluginMFHDR::~ExynosCameraPPUniPluginMFHDR()
{
}

status_t ExynosCameraPPUniPluginMFHDR::m_draw(ExynosCameraImage *srcImage,
                                           ExynosCameraImage *dstImage,
                                           ExynosCameraParameters *params)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    status_t ret = NO_ERROR;

#ifdef SAMSUNG_MFHDR_CAPTURE
    int currentHDRCaptureStep = srcImage[0].multiShotCount;
    int HDRCaptureLastStep = SCAPTURE_STEP_COUNT_1 + m_configurations->getModeValue(CONFIGURATION_LD_CAPTURE_COUNT) - 1;
    int HDRCaptureTotalCount = m_configurations->getModeValue(CONFIGURATION_LD_CAPTURE_COUNT);
    UniPluginFaceInfo_t hdrFaceInfo[NUM_OF_DETECTED_FACES];
    int hdrFaceNum = 0;
    int camera_scenario = m_configurations->getScenario();
    camera2_shot_ext *shot_ext = (struct camera2_shot_ext *)srcImage[0].buf.addr[srcImage[0].buf.getMetaPlaneIndex()];
    struct camera2_udm *udm = &(shot_ext->shot.udm);
    struct camera2_dm *dm = &(shot_ext->shot.dm);

    Mutex::Autolock l(m_uniPluginLock);

    if (m_uniPluginHandle == NULL) {
        CLOGE("[MF_HDR] MFHDR Uni plugin(%s) is NULL. Call ThreadJoin.", m_uniPluginName);
        if (m_UniPluginThreadJoin() != NO_ERROR) {
            return BAD_VALUE;
        }
    }

    if (srcImage[0].buf.index < 0 || dstImage[0].buf.index < 0) {
        return NO_ERROR;
    }

    if (currentHDRCaptureStep == m_configurations->getModeValue(CONFIGURATION_EXIF_CAPTURE_STEP_COUNT)) {
        CLOGD("copy (%d)frame meta for multi frame capture",
                m_configurations->getModeValue(CONFIGURATION_EXIF_CAPTURE_STEP_COUNT));
        m_configurations->setExifMetaData(&shot_ext->shot);
    }

    /* First shot */
    if (currentHDRCaptureStep == SCAPTURE_STEP_COUNT_1) {
        /* Init Uni plugin */
        ret = m_UniPluginInit();
        if (ret != NO_ERROR) {
            CLOGE("[MF_HDR] MFHDR Plugin init failed!!");
        }

        /* Setting HDR total count */
        CLOGD("[MF_HDR] Set capture num: %d", HDRCaptureTotalCount);
        ret = m_UniPluginSet(UNI_PLUGIN_INDEX_TOTAL_BUFFER_NUM, &HDRCaptureTotalCount);
        if (ret != NO_ERROR) {
            CLOGE("[MF_HDR] MFHDR Plugin set UNI_PLUGIN_INDEX_TOTAL_BUFFER_NUM failed!!");
        }

        /* Start case */
        UniPluginCameraInfo_t cameraInfo;
        cameraInfo.cameraType = getUniCameraType(camera_scenario, params->getCameraId());
        cameraInfo.sensorType = (UNI_PLUGIN_SENSOR_TYPE)m_getUniSensorId(params->getCameraId());
        cameraInfo.APType = UNI_PLUGIN_AP_TYPE_SLSI;

        CLOGD("[MF_HDR]Set camera info: %d:%d",
                cameraInfo.cameraType, cameraInfo.sensorType);
        ret = uni_plugin_set(m_uniPluginHandle,
                      m_uniPluginName, UNI_PLUGIN_INDEX_CAMERA_INFO, &cameraInfo);
        if (ret < 0) {
            CLOGE("[MF_HDR]Plugin set UNI_PLUGIN_INDEX_CAMERA_INFO failed!!");
        }

        /* Setting HDR INFO. */
        uint32_t hdrData = 0;
        hdrData = m_getUniHDRMode();
        CLOGD("[MF_HDR] Set hdr mode: %d", hdrData);
        ret = m_UniPluginSet(UNI_PLUGIN_INDEX_HDR_INFO, &hdrData);
        if (ret != NO_ERROR) {
            CLOGE("[MF_HDR] MFHDR Plugin set UNI_PLUGIN_INDEX_HDR_INFO failed!!");
        }

        /* Setting HDR extra info */
        int pictureW = 0, pictureH = 0;
        int cropPictureW = shot_ext->reserved_stream.output_crop_region[2];
        int cropPictureH = shot_ext->reserved_stream.output_crop_region[3];
        params->getSize(HW_INFO_HW_YUV_SIZE, (uint32_t *)&pictureW, (uint32_t *)&pictureH);

        CLOGD("[MF_HDR] Input width: %d, height: %d", cropPictureW, cropPictureH);

        if (udm != NULL) {
            UniPluginExtraBufferInfo_t extraData;
            memset(&extraData, 0, sizeof(UniPluginExtraBufferInfo_t));
            /* EV */
            extraData.exposureValue.den = 256;
            extraData.exposureValue.snum = (int32_t)udm->ae.vendorSpecific[4];

            /* short shutter speed(us) */
            extraData.shutterSpeed[0].num = udm->ae.vendorSpecific[390];
            extraData.shutterSpeed[0].den = 1000000;

            /* long shutter speed(us) */
            extraData.shutterSpeed[1].num = udm->ae.vendorSpecific[392];
            extraData.shutterSpeed[1].den = 1000000;

            /* short ISO */
            extraData.iso[0] = udm->ae.vendorSpecific[391];

            /* long ISO */
            extraData.iso[1] = udm->ae.vendorSpecific[393];

            /* HDR Strength */
            extraData.DRCstrength = udm->ae.vendorSpecific[394];

            /* float zoomRatio */
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
            if (m_configurations->getScenario() == SCENARIO_DUAL_REAR_ZOOM
                && m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW) {
                if (params->getCameraId() == CAMERA_ID_BACK_1) {
                    extraData.zoomRatio = (float)pictureW/(float)cropPictureW;
                } else {
                    extraData.zoomRatio = m_configurations->getZoomRatio();
                }
            } else
#endif
            {
                extraData.zoomRatio = m_configurations->getZoomRatio();
            }
            CLOGD("[MF_HDR] zoomRatio(%f)", extraData.zoomRatio);

            ret = m_UniPluginSet(UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO, &extraData);
            if(ret != NO_ERROR) {
                CLOGE("[MF_HDR] MFHDR Plugin set UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO failed!!");
            }
        }
    }

    /* Setting HDR FD info */
    memset(hdrFaceInfo, 0x00, sizeof(hdrFaceInfo));

    if (dm != NULL && shot_ext->shot.ctl.stats.faceDetectMode > FACEDETECT_MODE_OFF) {
        ExynosRect vraInputSize;
        ExynosRect2 detectedFace[NUM_OF_DETECTED_FACES];

        if (params->getHwConnectionMode(PIPE_MCSC, PIPE_VRA) == HW_CONNECTION_MODE_M2M) {
#ifdef CAPTURE_FD_SYNC_WITH_PREVIEW
            params->getHwVraInputSize(&vraInputSize.w, &vraInputSize.h, params->getDsInputPortId(false));
#else
            params->getHwVraInputSize(&vraInputSize.w, &vraInputSize.h,
                                            shot_ext->shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS]);
#endif
        } else {
            params->getSize(HW_INFO_HW_YUV_SIZE, (uint32_t *)&vraInputSize.w, (uint32_t *)&vraInputSize.h, 0);
        }
        CLOGD("[MF_HDR] VRA Size(%dx%d)", vraInputSize.w, vraInputSize.h);

        if (getCameraId() == CAMERA_ID_FRONT
            && m_configurations->getModeValue(CONFIGURATION_FLIP_HORIZONTAL) == true) {
            /* convert left, right values when flip horizontal(front camera) */
            for (int i = 0; i < NUM_OF_DETECTED_FACES; i++) {
                if (dm->stats.faceIds[i]) {
                    detectedFace[i].x1 = vraInputSize.w - dm->stats.faceRectangles[i][2];
                    detectedFace[i].y1 = dm->stats.faceRectangles[i][1];
                    detectedFace[i].x2 = vraInputSize.w - dm->stats.faceRectangles[i][0];
                    detectedFace[i].y2 = dm->stats.faceRectangles[i][3];
                }
            }
        } else {
            for (int i = 0; i < NUM_OF_DETECTED_FACES; i++) {
                if (dm->stats.faceIds[i]) {
                    detectedFace[i].x1 = dm->stats.faceRectangles[i][0];
                    detectedFace[i].y1 = dm->stats.faceRectangles[i][1];
                    detectedFace[i].x2 = dm->stats.faceRectangles[i][2];
                    detectedFace[i].y2 = dm->stats.faceRectangles[i][3];
                }
            }
        }

        for (int i = 0; i < NUM_OF_DETECTED_FACES; i++) {
            if (dm->stats.faceIds[i]) {
                hdrFaceInfo[i].faceROI.left   = calibratePosition(vraInputSize.w, srcImage[0].rect.w, detectedFace[i].x1);
                hdrFaceInfo[i].faceROI.top    = calibratePosition(vraInputSize.h, srcImage[0].rect.h, detectedFace[i].y1);
                hdrFaceInfo[i].faceROI.right  = calibratePosition(vraInputSize.w, srcImage[0].rect.w, detectedFace[i].x2);
                hdrFaceInfo[i].faceROI.bottom = calibratePosition(vraInputSize.h, srcImage[0].rect.h, detectedFace[i].y2);
                hdrFaceNum++;
            }
        }
    }

    ret = m_UniPluginSet(UNI_PLUGIN_INDEX_FACE_NUM, &hdrFaceNum);
    if (ret < 0) {
        CLOGE("[MF_HDR] MFHDR Plugin set UNI_PLUGIN_INDEX_FACE_NUM failed!! ret(%d)", ret);
        /* To do */
    }

    ret = m_UniPluginSet(UNI_PLUGIN_INDEX_FACE_INFO, &hdrFaceInfo);
    if (ret < 0) {
        CLOGE("[MF_HDR] MFHDR Plugin set UNI_PLUGIN_INDEX_FACE_INFO failed!! ret(%d)", ret);
        /* To do */
    }

    UniPluginMultiHdrData_t hdrData;

    /* WB */
    hdrData.wbGain.data = dm->color.gains;
    hdrData.wbGain.size = 4;

    /* CCM */
    hdrData.ccmVectors.data = &(udm->awb.vendorSpecific[25]);
    hdrData.ccmVectors.size = 9;

    /* GTM */
    hdrData.globalToneMap.data = udm->drc.globalToneMap;
    hdrData.globalToneMap.size = 32;

    /* Gamma */
    hdrData.rgbGammaLutX.data = udm->rgbGamma.xTable;
    hdrData.rgbGammaLutX.size = 32;
    hdrData.rgbGammaLutY.data = udm->rgbGamma.yTable;
    hdrData.rgbGammaLutY.size = 96;

    ret = m_UniPluginSet(UNI_PLUGIN_INDEX_MULTI_HDR_DATA, &hdrData);
    if (ret < 0) {
        CLOGE("[MF_HDR] MFHDR Plugin set UNI_PLUGIN_INDEX_FACE_INFO failed!! ret(%d)", ret);
        /* To do */
    }

    /* Set HDR buffer */
    UniPluginBufferData_t pluginData;
    memset(&pluginData, 0, sizeof(UniPluginBufferData_t));

    pluginData.inBuffY  = srcImage[0].buf.addr[0];
    pluginData.inBuffU  = srcImage[0].buf.addr[0] + (srcImage[0].rect.w * srcImage[0].rect.h);
    pluginData.inWidth  = srcImage[0].rect.w;
    pluginData.inHeight = srcImage[0].rect.h;
    pluginData.inFormat = UNI_PLUGIN_FORMAT_NV21;
    pluginData.fov.left = 0;
    pluginData.fov.top = 0;
    pluginData.fov.right = srcImage[0].rect.w;
    pluginData.fov.bottom = srcImage[0].rect.h;
    CLOGD("[MF_HDR] Set In-buffer info(W: %d, H: %d)",
            pluginData.inWidth, pluginData.inHeight);

    pluginData.inBuffFd[UNI_PLUGIN_BUFFER_PLANE_Y] = srcImage[0].buf.fd[0];
    pluginData.inBuffFd[UNI_PLUGIN_BUFFER_PLANE_U] = -1;
    pluginData.outBuffFd[UNI_PLUGIN_BUFFER_PLANE_Y] = dstImage[0].buf.fd[0];
    pluginData.outBuffFd[UNI_PLUGIN_BUFFER_PLANE_U] = -1;
    pluginData.outBuffY = dstImage[0].buf.addr[0];
    pluginData.outBuffU = dstImage[0].buf.addr[0] + (dstImage[0].rect.w * dstImage[0].rect.h);
    pluginData.outWidth = dstImage[0].rect.w ;
    pluginData.outHeight = dstImage[0].rect.h;
    CLOGD("[MF_HDR] Set Out-buffer info(W: %d, H: %d)",
            pluginData.outWidth, pluginData.outHeight);

    ret = m_UniPluginSet(UNI_PLUGIN_INDEX_BUFFER_INFO, &pluginData);
    if (ret != NO_ERROR) {
        CLOGE("[MF_HDR] MFHDR Plugin set UNI_PLUGIN_INDEX_BUFFER_INFO failed!!");
    }

    /* Last shot */
    if (currentHDRCaptureStep == HDRCaptureLastStep) {
        CLOGD("[MF_HDR] Last Shot!!");
        if (ret == NO_ERROR) {
            ret = m_UniPluginProcess();
            if (ret != NO_ERROR) {
                CLOGE("[MF_HDR] MFHDR Plugin process failed!!");
            }

            UTstr debugData;
            debugData.data = new unsigned char[LLS_EXIF_SIZE];

            if (debugData.data != NULL) {
                ret = m_UniPluginGet(UNI_PLUGIN_INDEX_DEBUG_INFO, &debugData);
                if (ret != NO_ERROR) {
                    CLOGE("[MF_HDR] MFHDR Plugin get UNI_PLUGIN_INDEX_DEBUG_INFO failed!!");
                } else {
                    CLOGD("[MF_HDR] Debug buffer size: %d", debugData.size);

                    params->setMFHDRdebugInfo(debugData.data, debugData.size);
                }

                delete []debugData.data;
            }
        }

        ret = m_UniPluginDeinit();
        if (ret != NO_ERROR) {
            CLOGE("[MF_HDR] MFHDR Plugin deinit failed!!");
        }
    }
#endif

    return ret;
}

void ExynosCameraPPUniPluginMFHDR::m_init(int scenario)
{
    CLOGD("[MF_HDR]");

    strncpy(m_uniPluginName, MF_HDR_PLUGIN_NAME, EXYNOS_CAMERA_NAME_STR_SIZE - 1);

    m_srcImageCapacity.setNumOfImage(1);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21M);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21);

    m_dstImageCapacity.setNumOfImage(1);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21M);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21);
}

status_t ExynosCameraPPUniPluginMFHDR::m_UniPluginInit(void)
{
    status_t ret = NO_ERROR;

    m_sensorId = UNI_PLUGIN_SENSOR_NAME_NONE;
    m_auxSensorId = UNI_PLUGIN_SENSOR_NAME_NONE;

    if(m_loadThread != NULL) {
        m_loadThread->join();
    }

    if (m_uniPluginHandle != NULL) {
        ret = uni_plugin_init(m_uniPluginHandle);
        if (ret < 0) {
            CLOGE("[MF_HDR]Uni plugin(%s) init failed!!, ret(%d)", m_uniPluginName, ret);
            return INVALID_OPERATION;
        }
    } else {
        CLOGE("[MF_HDR]Uni plugin(%s) is NULL!!", m_uniPluginName);
        return BAD_VALUE;
    }

    return NO_ERROR;
}

status_t ExynosCameraPPUniPluginMFHDR::m_extControl(int controlType, void *data)
{
    status_t ret = NO_ERROR;
    enum UNI_PLUGIN_INDEX pluging_index = UNI_PLUGIN_INDEX_END;

    switch(controlType) {
    case PP_EXT_CONTROL_POWER_CONTROL:
        pluging_index = UNI_PLUGIN_INDEX_POWER_CONTROL;
        break;
    case PP_EXT_CONTROL_SET_UNI_PLUGIN_HANDLE:
        CLOGD("[MF_HDR] PP_EXT_CONTROL_SET_UNI_PLUGIN_HANDLE");
        m_uniPluginHandle = data;
        break;
    default:
        break;
    }

    if (m_uniPluginHandle == NULL) {
        CLOGW("[MF_HDR] HDR Uni plugin(%s) is NULL. Call ThreadJoin.", m_uniPluginName);
        if (m_UniPluginThreadJoin() != NO_ERROR) {
            return BAD_VALUE;
        }
    }

    if (m_uniPluginHandle != NULL) {
        if (pluging_index < UNI_PLUGIN_INDEX_END) {
            CLOGD("[MF_HDR] pluging_index(%d)", pluging_index);
            ret = m_UniPluginSet(pluging_index, data);
            if (ret < 0) {
                CLOGE("[MF_HDR] HDR Plugin set controlType(%d) failed!! ret(%d)", controlType, ret);
            }
        }
    } else {
        CLOGE("[MF_HDR]Uni plugin(%s) is NULL!!", m_uniPluginName);
        return BAD_VALUE;
    }

    return NO_ERROR;
}

