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
                                           ExynosCameraImage *dstImage)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    status_t ret = NO_ERROR;

#ifdef SAMSUNG_LLS_DEBLUR
    int currentLDCaptureStep = srcImage[0].multiShotCount;
    int LDCaptureLastStep = SCAPTURE_STEP_COUNT_1 + m_parameters->getLDCaptureCount() - 1;
    int LDCaptureTotalCount = m_parameters->getLDCaptureCount();
    UniPluginFaceInfo_t llsFaceInfo[NUM_OF_DETECTED_FACES];
    int llsFaceNum = 0;
    int HiFiLLS = -1;

    Mutex::Autolock l(m_uniPluginLock);

    if (m_UniPluginHandle == NULL) {
        CLOGE("[LLS_MBR] LLS Uni plugin(%s) is NULL. Call ThreadJoin.", m_UniPluginName);
        if (m_UniPluginThreadJoin() != NO_ERROR) {
            return BAD_VALUE;
        }
    }

    /* First shot */
    if (currentLDCaptureStep == SCAPTURE_STEP_COUNT_1) {
        UNI_PLUGIN_OPERATION_MODE opMode = UNI_PLUGIN_OP_END;

        int llsMode = m_parameters->getLDCaptureLLSValue();

        switch(llsMode) {
        case LLS_LEVEL_FLASH:
            CLOGE("Disable flashed LLS Capture Mode. Please, check lls Mode");
            return BAD_VALUE;
        case LLS_LEVEL_MULTI_MERGE_2:
        case LLS_LEVEL_MULTI_MERGE_INDICATOR_2:
        case LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_2:
        case LLS_LEVEL_MULTI_MERGE_3:
        case LLS_LEVEL_MULTI_MERGE_INDICATOR_3:
        case LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_3:
        case LLS_LEVEL_MULTI_MERGE_4:
        case LLS_LEVEL_MULTI_MERGE_INDICATOR_4:
        case LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_4:
            opMode = UNI_PLUGIN_OP_COMPOSE_IMAGE;
            break;
        case LLS_LEVEL_MULTI_PICK_2:
        case LLS_LEVEL_MULTI_PICK_3:
        case LLS_LEVEL_MULTI_PICK_4:
            opMode = UNI_PLUGIN_OP_SELECT_IMAGE;
            break;
        case LLS_LEVEL_ZSL:
            opMode = UNI_PLUGIN_OP_COMPOSE_IMAGE; /* temp */
            break;
        default:
            CLOGE("[LLS_MBR] Wrong LLS mode(%d) has been get!!", llsMode);
            return BAD_VALUE;
        }
        CLOGD("[LLS_MBR] llsMode(%d) LastStepCount(%d) opMode(%d) TotalCount(%d)",
                llsMode, LDCaptureLastStep, opMode, LDCaptureTotalCount);

        if (m_hifiLLSMode == true) {
            int powerCtrl = 1;
            ret = m_UniPluginSet(UNI_PLUGIN_INDEX_POWER_CONTROL, &powerCtrl);
            if (ret < 0) {
                CLOGE("[LLS_MBR] LLS Plugin set UNI_PLUGIN_INDEX_POWER_CONTROL failed!! ret(%d)", ret);
            }
        }

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

        /* Start case */
        UniPluginCameraInfo_t cameraInfo;
        cameraInfo.CameraType = (UNI_PLUGIN_CAMERA_TYPE)m_cameraId;
        cameraInfo.SensorType = (UNI_PLUGIN_SENSOR_TYPE)getSensorId(m_cameraId);
        cameraInfo.APType = UNI_PLUGIN_AP_TYPE_SLSI;

        CLOGD("Set camera info: %d:%d",
                cameraInfo.CameraType, cameraInfo.SensorType);
        ret = uni_plugin_set(m_UniPluginHandle,
                      m_UniPluginName, UNI_PLUGIN_INDEX_CAMERA_INFO, &cameraInfo);
        if (ret < 0) {
            CLOGE("Plugin set UNI_PLUGIN_INDEX_CAMERA_INFO failed!!");
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
        camera2_shot_ext *shot_ext = (struct camera2_shot_ext *)srcImage[0].buf.addr[srcImage[0].buf.getMetaPlaneIndex()];
        struct camera2_udm *udm = &(shot_ext->shot.udm);

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
            extraData.zoomRatio = m_parameters->getZoomRatio();
            CLOGD("[LLS_MBR] zoomRatio(%f)", extraData.zoomRatio);

            ret = m_UniPluginSet(UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO, &extraData);
            if(ret != NO_ERROR) {
                CLOGE("[LLS_MBR] LLS Plugin set UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO failed!!");
            }
        }

        CLOGD("[LLS_MBR] m_hifiLLSMode: %d", m_hifiLLSMode);
        if (m_hifiLLSMode == true) {
            struct camera2_dm *dm = &(shot_ext->shot.dm);
            if (dm != NULL) {
                ExynosRect vraInputSize;
                ExynosRect2 detectedFace[NUM_OF_DETECTED_FACES];

                memset(detectedFace, 0x00, sizeof(detectedFace));
                memset(llsFaceInfo, 0x00, sizeof(llsFaceInfo));

                if (m_parameters->getHwConnectionMode(PIPE_MCSC, PIPE_VRA) == HW_CONNECTION_MODE_M2M) {
                    m_parameters->getHwVraInputSize(&vraInputSize.w, &vraInputSize.h);
                } else {
                    m_parameters->getHwYuvSize(&vraInputSize.w, &vraInputSize.h, 0);
                }
                CLOGD("[LLS_MBR] VRA Size(%dx%d)", vraInputSize.w, vraInputSize.h);

                if (dm->stats.faceDetectMode != FACEDETECT_MODE_OFF) {
                    if (getCameraId() == CAMERA_ID_FRONT
                        && m_parameters->getFlipHorizontal() == true) {
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
                }

                for (int i = 0; i < NUM_OF_DETECTED_FACES; i++) {
                    if (dm->stats.faceIds[i]) {
                        llsFaceInfo[i].FaceROI.left   = calibratePosition(vraInputSize.w, 2000, detectedFace[i].x1) - 1000;
                        llsFaceInfo[i].FaceROI.top    = calibratePosition(vraInputSize.h, 2000, detectedFace[i].y1) - 1000;
                        llsFaceInfo[i].FaceROI.right  = calibratePosition(vraInputSize.w, 2000, detectedFace[i].x2) - 1000;
                        llsFaceInfo[i].FaceROI.bottom = calibratePosition(vraInputSize.h, 2000, detectedFace[i].y2) - 1000;
                        llsFaceNum++;
                    }
                }
            }
        }
    }

    if (m_hifiLLSMode == true) {
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

    pluginData.InBuffY  = srcImage[0].buf.addr[0];
    pluginData.InBuffU  = srcImage[0].buf.addr[0] + (srcImage[0].rect.w * srcImage[0].rect.h);
    pluginData.InWidth  = srcImage[0].rect.w;
    pluginData.InHeight = srcImage[0].rect.h;
    pluginData.InFormat = UNI_PLUGIN_FORMAT_NV21;
    pluginData.FOV.left = 0;
    pluginData.FOV.top = 0;
    pluginData.FOV.right = srcImage[0].rect.w;
    pluginData.FOV.bottom = srcImage[0].rect.h;
    CLOGD("[LLS_MBR] Set In-buffer info(W: %d, H: %d)",
            pluginData.InWidth, pluginData.InHeight);

    if (m_hifiLLSMode == true) {
        pluginData.InBuffFd[UNI_PLUGIN_BUFFER_PLANE_Y] = srcImage[0].buf.fd[0];
        pluginData.InBuffFd[UNI_PLUGIN_BUFFER_PLANE_U] = -1;
        pluginData.OutBuffFd[UNI_PLUGIN_BUFFER_PLANE_Y] = dstImage[0].buf.fd[0];
        pluginData.OutBuffFd[UNI_PLUGIN_BUFFER_PLANE_U] = -1;
        pluginData.OutBuffY = dstImage[0].buf.addr[0];
        pluginData.OutBuffU = dstImage[0].buf.addr[0] + (dstImage[0].rect.w * dstImage[0].rect.h);
        pluginData.OutWidth = dstImage[0].rect.w ;
        pluginData.OutHeight = dstImage[0].rect.h;
        CLOGD("[LLS_MBR] Set Out-buffer info(W: %d, H: %d)",
                pluginData.OutWidth, pluginData.OutHeight);
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

                    m_parameters->setLLSdebugInfo(debugData.data, debugData.size);
                }

                delete []debugData.data;
            }
        }

        ret = m_UniPluginDeinit();
        if (ret != NO_ERROR) {
            CLOGE("[LLS_MBR] LLS Plugin deinit failed!!");
        }

        if (m_hifiLLSMode == true) {
            int powerCtrl = 0;
            ret = m_UniPluginSet(UNI_PLUGIN_INDEX_POWER_CONTROL, &powerCtrl);
            if (ret < 0) {
                CLOGE("[LLS_MBR] LLS Plugin set UNI_PLUGIN_INDEX_POWER_CONTROL failed!! ret(%d)", ret);
            }
        }
    }
#endif

    return ret;
}

void ExynosCameraPPUniPluginLD::m_init(int scenario)
{
    CLOGD(" ");
    m_hifiLLSMode = false;

    if (scenario == PP_SCENARIO_HIFI_LLS) {
        strncpy(m_UniPluginName, HIFI_LLS_PLUGIN_NAME, EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_hifiLLSMode = true;
    } else {
        strncpy(m_UniPluginName, LLS_PLUGIN_NAME, EXYNOS_CAMERA_NAME_STR_SIZE - 1);
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

    if(m_loadThread != NULL) {
        m_loadThread->join();
    }

    if (m_UniPluginHandle != NULL) {
        ret = uni_plugin_init(m_UniPluginHandle);
        if (ret < 0) {
            CLOGE("Uni plugin(%s) init failed!!, ret(%d)", m_UniPluginName, ret);
            return INVALID_OPERATION;
        }
    } else {
        CLOGE("Uni plugin(%s) is NULL!!", m_UniPluginName);
        return BAD_VALUE;
    }

    return NO_ERROR;
}

