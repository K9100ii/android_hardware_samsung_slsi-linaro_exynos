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
#define LOG_TAG "ExynosCameraPPUniPluginLD"

#include "ExynosCameraPPUniPluginLD.h"

ExynosCameraPPUniPluginLD::~ExynosCameraPPUniPluginLD()
{
}

status_t ExynosCameraPPUniPluginLD::m_draw(ExynosCameraImage *srcImage,
                                           ExynosCameraImage *dstImage,
                                           ExynosCameraParameters *params)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    status_t ret = NO_ERROR;

#ifdef SAMSUNG_LLS_DEBLUR
    int currentLDCaptureStep = srcImage[0].multiShotCount;
    int LDCaptureLastStep = SCAPTURE_STEP_COUNT_1 + m_configurations->getModeValue(CONFIGURATION_LD_CAPTURE_COUNT) - 1;
    int LDCaptureTotalCount = m_configurations->getModeValue(CONFIGURATION_LD_CAPTURE_COUNT);
    int HiFiLLS = -1;
    int camera_scenario = m_configurations->getScenario();
    camera2_shot_ext *shot_ext = NULL;
    UniPluginPrivInfo_t privInfo;

    Mutex::Autolock l(m_uniPluginLock);

    if (m_uniPluginHandle == NULL) {
        CLOGE("[LLS_MBR] LLS Uni plugin(%s) is NULL. Call ThreadJoin.", m_uniPluginName);
        if (m_UniPluginThreadJoin() != NO_ERROR) {
            return BAD_VALUE;
        }
    }

    if (srcImage[0].buf.index < 0 || dstImage[0].buf.index < 0) {
        return NO_ERROR;
    }

    CLOGD("[LLS_MBR] currentLDCaptureStep(%d)", currentLDCaptureStep);

    shot_ext = (struct camera2_shot_ext *)srcImage[0].buf.addr[srcImage[0].buf.getMetaPlaneIndex()];

    /* First shot */
    if (currentLDCaptureStep == SCAPTURE_STEP_COUNT_1) {
        UNI_PLUGIN_OPERATION_MODE opMode = UNI_PLUGIN_OP_END;

        int llsMode = m_configurations->getModeValue(CONFIGURATION_LD_CAPTURE_LLS_VALUE);

        switch(llsMode) {
        case LLS_LEVEL_FLASH:
            CLOGE("[LLS_MBR]Disable flashed LLS Capture Mode. Please, check lls Mode");
            return BAD_VALUE;
        case LLS_LEVEL_MULTI_MERGE_2:
        case LLS_LEVEL_MULTI_MERGE_INDICATOR_2:
        case LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_2:
#ifdef SUPPORT_ZSL_MULTIFRAME
        case LLS_LEVEL_MULTI_MERGE_LOW_2:
        case LLS_LEVEL_MULTI_MERGE_ZSL_2:
#endif
        case LLS_LEVEL_MULTI_MERGE_3:
        case LLS_LEVEL_MULTI_MERGE_INDICATOR_3:
        case LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_3:
#ifdef SUPPORT_ZSL_MULTIFRAME
        case LLS_LEVEL_MULTI_MERGE_LOW_3:
        case LLS_LEVEL_MULTI_MERGE_ZSL_3:
#endif
        case LLS_LEVEL_MULTI_MERGE_4:
        case LLS_LEVEL_MULTI_MERGE_INDICATOR_4:
        case LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_4:
#ifdef SUPPORT_ZSL_MULTIFRAME
        case LLS_LEVEL_MULTI_MERGE_LOW_4:
        case LLS_LEVEL_MULTI_MERGE_ZSL_4:
#endif
        case LLS_LEVEL_MULTI_MERGE_5:
        case LLS_LEVEL_MULTI_MERGE_INDICATOR_5:
        case LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_5:
#ifdef SUPPORT_ZSL_MULTIFRAME
        case LLS_LEVEL_MULTI_MERGE_LOW_5:
        case LLS_LEVEL_MULTI_MERGE_ZSL_5:
#endif
            opMode = UNI_PLUGIN_OP_COMPOSE_IMAGE;
            break;
        case LLS_LEVEL_MULTI_PICK_2:
        case LLS_LEVEL_MULTI_PICK_3:
        case LLS_LEVEL_MULTI_PICK_4:
        case LLS_LEVEL_MULTI_PICK_5:
            opMode = UNI_PLUGIN_OP_SELECT_IMAGE;
            break;
        case LLS_LEVEL_ZSL:
            opMode = UNI_PLUGIN_OP_SR_IMAGE;
            break;
        default:
            CLOGE("[LLS_MBR] Wrong LLS mode(%d) has been get!!", llsMode);
            return BAD_VALUE;
        }
        CLOGD("[LLS_MBR] llsMode(%d) LastStepCount(%d) opMode(%d) TotalCount(%d)",
                llsMode, LDCaptureLastStep, opMode, LDCaptureTotalCount);

        /* Init Uni plugin */
        ret = m_UniPluginInit();
        if (ret != NO_ERROR) {
            CLOGE("[LLS_MBR] LLS Plugin init failed!!");
        }

        /* Setting LLS total count */
        CLOGD("[LLS_MBR] Set capture num: %d", LDCaptureTotalCount);
        ret = m_UniPluginSet(UNI_PLUGIN_INDEX_TOTAL_BUFFER_NUM, &LDCaptureTotalCount);
        if (ret != NO_ERROR) {
            CLOGE("[LLS_MBR] LLS Plugin set UNI_PLUGIN_INDEX_TOTAL_BUFFER_NUM failed!!");
        }

        /* Setting LLS capture mode */
        CLOGD("[LLS_MBR] Set capture mode: %d", opMode);
        ret = m_UniPluginSet(UNI_PLUGIN_INDEX_OPERATION_MODE, &opMode);
        if (ret != NO_ERROR) {
            CLOGE("[LLS_MBR] LLS Plugin set UNI_PLUGIN_INDEX_OPERATION_MODE failed!!");
        }

#ifdef SAMSUNG_HIFI_CAPTURE
        int hwPictureW = 0, hwPictureH = 0;
        int masterCameraId = UNI_PLUGIN_SENSOR_NAME_NONE, slaveCameraId = UNI_PLUGIN_SENSOR_NAME_NONE;
        UniPluginDualInfo_t dualInfo;

        memset(&dualInfo, 0, sizeof(UniPluginDualInfo_t));

        params->getSize(HW_INFO_HW_PICTURE_SIZE, (uint32_t *)&hwPictureW, (uint32_t *)&hwPictureH);
        dualInfo.fullWidth = hwPictureW;
        dualInfo.fullHeight = hwPictureH;

#ifdef USE_DUAL_CAMERA
        if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true) {
            getDualCameraId(&masterCameraId, &slaveCameraId);

            dualInfo.isDual = true;
            dualInfo.mainSensorType = (UNI_PLUGIN_SENSOR_TYPE)m_getUniSensorId(masterCameraId);
            dualInfo.auxSensorType = (UNI_PLUGIN_SENSOR_TYPE)m_getUniAuxSensorId(slaveCameraId);
        }
#endif

        CLOGD("[LLS_MBR] UNI_PLUGIN_INDEX_DUAL_INFO(isDual:%d, full size:%dx%d, Sensor id:%d,%d)",
                dualInfo.isDual, dualInfo.fullWidth, dualInfo.fullHeight,
                (int)dualInfo.mainSensorType, (int)dualInfo.auxSensorType);

        ret = uni_plugin_set(m_uniPluginHandle,
            m_uniPluginName, UNI_PLUGIN_INDEX_DUAL_INFO, &dualInfo);
        if (ret < 0) {
            CLOGE("[LLS_MBR] LLS Plugin set UNI_PLUGIN_INDEX_DUAL_INFO failed!! ret(%d)", ret);
        }
#endif

        /* Start case */
        UniPluginCameraInfo_t cameraInfo;
        cameraInfo.cameraType = getUniCameraType(camera_scenario, params->getCameraId());
        cameraInfo.sensorType = (UNI_PLUGIN_SENSOR_TYPE)m_getUniSensorId(params->getCameraId());
        cameraInfo.APType = UNI_PLUGIN_AP_TYPE_SLSI;

        CLOGD("[LLS_MBR]Set camera info: %d:%d",
                cameraInfo.cameraType, cameraInfo.sensorType);
        ret = uni_plugin_set(m_uniPluginHandle,
                      m_uniPluginName, UNI_PLUGIN_INDEX_CAMERA_INFO, &cameraInfo);
        if (ret < 0) {
            CLOGE("[LLS_MBR]Plugin set UNI_PLUGIN_INDEX_CAMERA_INFO failed!!");
        }

        /* Setting HDR INFO. */
        if (m_hifiLLSMode == true) {
            uint32_t hdrData = 0;
            hdrData = m_getUniHDRMode();
            CLOGD("[LLS_MBR] Set hdr mode: %d", hdrData);
            ret = m_UniPluginSet(UNI_PLUGIN_INDEX_HDR_INFO, &hdrData);
            if (ret != NO_ERROR) {
                CLOGE("[LLS_MBR] LLS Plugin set UNI_PLUGIN_INDEX_HDR_INFO failed!!");
            }
        }

        /* Setting LLS extra info */
        struct camera2_udm *udm = &(shot_ext->shot.udm);
        int cropPictureW = shot_ext->reserved_stream.output_crop_region[2];
        int cropPictureH = shot_ext->reserved_stream.output_crop_region[3];
        CLOGD("[LLS_MBR] Input width: %d, height: %d", cropPictureW, cropPictureH);

        if (udm != NULL) {
            UniPluginExtraBufferInfo_t extraData;
            memset(&extraData, 0, sizeof(UniPluginExtraBufferInfo_t));

            extraData.brightnessValue.snum = (int)udm->ae.vendorSpecific[5];
            extraData.brightnessValue.den = 256;

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
            extraData.DRCstrength = udm->ae.vendorSpecific[394];

            /* float zoomRatio */
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
            if (m_configurations->getScenario() == SCENARIO_DUAL_REAR_ZOOM
                && m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW) {
                if (params->getCameraId() == CAMERA_ID_BACK_1) {
                    extraData.zoomRatio = (float)hwPictureW/(float)cropPictureW;
                } else {
                    extraData.zoomRatio = m_configurations->getZoomRatio();
                }
            } else
#endif
            {
                extraData.zoomRatio = m_configurations->getZoomRatio();
            }
            CLOGD("[LLS_MBR] zoomRatio(%f)", extraData.zoomRatio);

            ret = m_UniPluginSet(UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO, &extraData);
            if(ret != NO_ERROR) {
                CLOGE("[LLS_MBR] LLS Plugin set UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO failed!!");
            }
        }

        /* Set priv Info */
        memset(&privInfo, 0, sizeof(UniPluginPrivInfo_t));
        privInfo.priv[0] = &(shot_ext->shot.udm.aa.gyroInfo);
        ret = m_UniPluginSet(UNI_PLUGIN_INDEX_PRIV_INFO, &privInfo);
        if (ret < 0) {
            CLOGE("[LLS_MBR](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_PRIV_INFO) failed!!", __FUNCTION__, __LINE__);
        }

        CLOGD("[LLS_MBR] m_hifiLLSMode: %d", m_hifiLLSMode);
    }

    if (m_hifiLLSMode == true) {
        struct camera2_dm *dm = &(shot_ext->shot.dm);
        UniPluginFaceInfo_t llsFaceInfo[NUM_OF_DETECTED_FACES];
        int llsFaceNum = 0;

        memset(llsFaceInfo, 0x00, sizeof(llsFaceInfo));

        if (dm != NULL && shot_ext->shot.ctl.stats.faceDetectMode > FACEDETECT_MODE_OFF) {
            ExynosRect2 detectedFace[NUM_OF_DETECTED_FACES];
            ExynosRect vraInputSize;

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
            CLOGD("[LLS_MBR] VRA Size(%dx%d)", vraInputSize.w, vraInputSize.h);

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
                    llsFaceInfo[i].faceROI.left   = calibratePosition(vraInputSize.w, 2000, detectedFace[i].x1) - 1000;
                    llsFaceInfo[i].faceROI.top    = calibratePosition(vraInputSize.h, 2000, detectedFace[i].y1) - 1000;
                    llsFaceInfo[i].faceROI.right  = calibratePosition(vraInputSize.w, 2000, detectedFace[i].x2) - 1000;
                    llsFaceInfo[i].faceROI.bottom = calibratePosition(vraInputSize.h, 2000, detectedFace[i].y2) - 1000;
                    llsFaceNum++;
                }
            }
        }

        ret = m_UniPluginSet(UNI_PLUGIN_INDEX_FACE_NUM, &llsFaceNum);
        if (ret < 0) {
            CLOGE("[LLS_MBR] LLS Plugin set UNI_PLUGIN_INDEX_FACE_NUM failed!! ret(%d)", ret);
            /* To do */
        }

        ret = m_UniPluginSet(UNI_PLUGIN_INDEX_FACE_INFO, &llsFaceInfo);
        if (ret < 0) {
            CLOGE("[LLS_MBR] LLS Plugin set UNI_PLUGIN_INDEX_FACE_INFO failed!! ret(%d)", ret);
            /* To do */
        }
    }

    /* Set LLS buffer */
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
    CLOGD("[LLS_MBR] Set In-buffer info(W: %d, H: %d)",
            pluginData.inWidth, pluginData.inHeight);

    if (m_hifiLLSMode == true) {
        pluginData.inBuffFd[UNI_PLUGIN_BUFFER_PLANE_Y] = srcImage[0].buf.fd[0];
        pluginData.inBuffFd[UNI_PLUGIN_BUFFER_PLANE_U] = -1;
        pluginData.outBuffFd[UNI_PLUGIN_BUFFER_PLANE_Y] = dstImage[0].buf.fd[0];
        pluginData.outBuffFd[UNI_PLUGIN_BUFFER_PLANE_U] = -1;
        pluginData.outBuffY = dstImage[0].buf.addr[0];
        pluginData.outBuffU = dstImage[0].buf.addr[0] + (dstImage[0].rect.w * dstImage[0].rect.h);
        pluginData.outWidth = dstImage[0].rect.w ;
        pluginData.outHeight = dstImage[0].rect.h;
        CLOGD("[LLS_MBR] Set Out-buffer info(W: %d, H: %d)",
                pluginData.outWidth, pluginData.outHeight);
    }

    ret = m_UniPluginSet(UNI_PLUGIN_INDEX_BUFFER_INFO, &pluginData);
    if (ret != NO_ERROR) {
        CLOGE("[LLS_MBR] LLS Plugin set UNI_PLUGIN_INDEX_BUFFER_INFO failed!!");
    }

    /* Last shot */
    if (currentLDCaptureStep == LDCaptureLastStep) {
        CLOGD("[LLS_MBR] Last Shot!!");
        if (ret == NO_ERROR) {
            ret = m_UniPluginProcess();
            if (ret != NO_ERROR) {
                CLOGE("[LLS_MBR] LLS Plugin process failed!!");
            }

            UTstr debugData;
            debugData.data = new unsigned char[LLS_EXIF_SIZE];

            if (debugData.data != NULL) {
                ret = m_UniPluginGet(UNI_PLUGIN_INDEX_DEBUG_INFO, &debugData);
                if (ret != NO_ERROR) {
                    CLOGE("[LLS_MBR] LLS Plugin get UNI_PLUGIN_INDEX_DEBUG_INFO failed!!");
                } else {
                    CLOGD("[LLS_MBR] Debug buffer size: %d", debugData.size);

#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
                    void *pFusionInfo = m_configurations->getFusionFrameStateInfo();
                    int fusionInfoSize = sizeof(unsigned short);

                    if ((pFusionInfo) && (debugData.size <= (LLS_EXIF_SIZE - fusionInfoSize))) {
                        memcpy(&(debugData.data[debugData.size]), pFusionInfo, fusionInfoSize);
                        params->setLLSdebugInfo(debugData.data, debugData.size + fusionInfoSize);
                    }
                    else
#endif
                    {
                        params->setLLSdebugInfo(debugData.data, debugData.size);
                    }
                }

                delete []debugData.data;
            }
        }

        ret = m_UniPluginDeinit();
        if (ret != NO_ERROR) {
            CLOGE("[LLS_MBR] LLS Plugin deinit failed!!");
        }
    }
#endif

    return ret;
}

void ExynosCameraPPUniPluginLD::m_init(int scenario)
{
    CLOGD("[LLS_MBR]");
    m_hifiLLSMode = false;

    if (scenario == PP_SCENARIO_HIFI_LLS) {
        strncpy(m_uniPluginName, HIFI_LLS_PLUGIN_NAME, EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_hifiLLSMode = true;
    } else {
        strncpy(m_uniPluginName, LLS_PLUGIN_NAME, EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    }

    m_srcImageCapacity.setNumOfImage(1);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21M);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21);

    m_dstImageCapacity.setNumOfImage(1);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21M);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21);
}

status_t ExynosCameraPPUniPluginLD::m_UniPluginInit(void)
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
            CLOGE("[LLS_MBR]Uni plugin(%s) init failed!!, ret(%d)", m_uniPluginName, ret);
            return INVALID_OPERATION;
        }
    } else {
        CLOGE("[LLS_MBR]Uni plugin(%s) is NULL!!", m_uniPluginName);
        return BAD_VALUE;
    }

    return NO_ERROR;
}

status_t ExynosCameraPPUniPluginLD::m_extControl(int controlType, void *data)
{
    status_t ret = NO_ERROR;
    enum UNI_PLUGIN_INDEX pluging_index = UNI_PLUGIN_INDEX_END;

    switch(controlType) {
    case PP_EXT_CONTROL_POWER_CONTROL:
        pluging_index = UNI_PLUGIN_INDEX_POWER_CONTROL;
        break;
    case PP_EXT_CONTROL_SET_UNI_PLUGIN_HANDLE:
        CLOGD("[LLS_MBR] PP_EXT_CONTROL_SET_UNI_PLUGIN_HANDLE");
        m_uniPluginHandle = data;
        break;
    default:
        break;
    }

    if (m_uniPluginHandle == NULL) {
        CLOGW("[LLS_MBR] LLS Uni plugin(%s) is NULL. Call ThreadJoin.", m_uniPluginName);
        if (m_UniPluginThreadJoin() != NO_ERROR) {
            return BAD_VALUE;
        }
    }

    if (m_uniPluginHandle != NULL) {
        if (pluging_index < UNI_PLUGIN_INDEX_END) {
            CLOGD("[LLS_MBR] pluging_index(%d)", pluging_index);
            ret = m_UniPluginSet(pluging_index, data);
            if (ret < 0) {
                CLOGE("[LLS_MBR] LLS Plugin set controlType(%d) failed!! ret(%d)", controlType, ret);
            }
        }
    } else {
        CLOGE("[LLS_MBR]Uni plugin(%s) is NULL!!", m_uniPluginName);
        return BAD_VALUE;
    }

    return NO_ERROR;
}

