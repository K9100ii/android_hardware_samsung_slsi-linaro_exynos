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

#define LOG_TAG "ExynosCameraFusionWrapper"

#include "ExynosCameraFusionWrapper.h"
#include "ExynosCameraPipe.h"

ExynosCameraFusionWrapper::ExynosCameraFusionWrapper()
{
    ALOGD("DEBUG(%s[%d]):new ExynosCameraFusionWrapper object allocated", __FUNCTION__, __LINE__);

    for (int i = 0; i < CAMERA_ID_MAX; i++) {
        m_flagCreated[i] = false;

        m_width[i] = 0;
        m_height[i] = 0;
        m_stride[i] = 0;
    }
#ifdef SAMSUNG_DUAL_SOLUTION
    m_cameraParam = new UniPlugin_DCVOZ_CAMERAPARAM_t;
#endif
}

ExynosCameraFusionWrapper::~ExynosCameraFusionWrapper()
{
    status_t ret = NO_ERROR;

    for (int i = 0; i < CAMERA_ID_MAX; i++) {
        if (flagCreate(i) == true) {
            ret = destroy(i);
            if (ret != NO_ERROR) {
                ALOGE("ERR(%s[%d]):destroy(%d) fail", __FUNCTION__, __LINE__, i);
            }
        }
    }
#ifdef SAMSUNG_DUAL_SOLUTION
    delete m_cameraParam;
#endif
}

status_t ExynosCameraFusionWrapper::create(int cameraId,
                                           int srcWidth, int srcHeight,
                                           int dstWidth, int dstHeight,
                                           char *calData, int calDataSize)
{
    // hack for CLOG
    int         m_cameraId = cameraId;
    const char *m_name = "";

    Mutex::Autolock lock(m_createLock);

    status_t ret = NO_ERROR;

    if (CAMERA_ID_MAX <= cameraId) {
        CLOGE("ERR(%s[%d]):invalid cameraId(%d). so, fail", __FUNCTION__, __LINE__, cameraId);
        return INVALID_OPERATION;
    }

    if (m_flagCreated[cameraId] == true) {
        CLOGE("ERR(%s[%d]):cameraId(%d) is alread created. so, fail", __FUNCTION__, __LINE__, cameraId);
        return INVALID_OPERATION;
    }

    if (srcWidth == 0 || srcWidth == 0 || dstWidth == 0 || dstHeight == 0) {
        CLOGE("ERR(%s[%d]):srcWidth == %d || srcWidth == %d || dstWidth == %d || dstHeight == %d. so, fail",
            __FUNCTION__, __LINE__, srcWidth, srcWidth, dstWidth, dstHeight);
        return INVALID_OPERATION;
    }

    m_init(cameraId);

    CLOGD("DEBUG(%s[%d]):create(calData(%p), calDataSize(%d), srcWidth(%d), srcHeight(%d), dstWidth(%d), dstHeight(%d)",
        __FUNCTION__, __LINE__,
        calData, calDataSize, srcWidth, srcHeight, dstWidth, dstHeight);

    // set info int width, int height, int stride
    m_width      [cameraId] = srcWidth;
    m_height     [cameraId] = srcHeight;
    m_stride     [cameraId] = srcWidth;

    // declare it created
    m_flagCreated[cameraId] = true;

    return NO_ERROR;
}

status_t ExynosCameraFusionWrapper::destroy(int cameraId)
{
    // hack for CLOG
    int         m_cameraId = cameraId;
    const char *m_name = "";

    Mutex::Autolock lock(m_createLock);

    status_t ret = NO_ERROR;

    if (CAMERA_ID_MAX <= cameraId) {
        CLOGE("ERR(%s[%d]):invalid cameraId(%d). so, fail", __FUNCTION__, __LINE__, cameraId);
        return INVALID_OPERATION;
    }

    if (m_flagCreated[cameraId] == false) {
        CLOGE("ERR(%s[%d]):cameraId(%d) is alread destroyed. so, fail", __FUNCTION__, __LINE__, cameraId);
        return INVALID_OPERATION;
    }

    CLOGD("DEBUG(%s[%d]):destroy()", __FUNCTION__, __LINE__);

    m_flagCreated[cameraId] = false;

    return NO_ERROR;
}

bool ExynosCameraFusionWrapper::flagCreate(int cameraId)
{
    Mutex::Autolock lock(m_createLock);

    if (CAMERA_ID_MAX <= cameraId) {
        android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):invalid cameraId(%d), assert!!!!",
                __FUNCTION__, __LINE__, cameraId);
    }

    return m_flagCreated[cameraId];
}

bool ExynosCameraFusionWrapper::flagReady(int cameraId)
{
    return m_flagCreated[cameraId];
}

status_t ExynosCameraFusionWrapper::execute(int cameraId,
                                            sync_type_t syncType,
                                            struct camera2_shot_ext shot_ext[],
                                            DOF *dof[],
                                            ExynosCameraBuffer srcBuffer[],
                                            ExynosRect srcRect[],
                                            ExynosCameraBufferManager *srcBufferManager[],
                                            ExynosCameraBuffer dstBuffer,
                                            ExynosRect dstRect,
                                            ExynosCameraBufferManager *dstBufferManager)
{
    // hack for CLOG
    int         m_cameraId = cameraId;
    const char *m_name = "";

    status_t ret = NO_ERROR;

    if (this->flagCreate(cameraId) == false) {
        CLOGE("ERR(%s[%d]):flagCreate(%d) == false. so fail", __FUNCTION__, __LINE__, cameraId);
        return INVALID_OPERATION;
    }

    m_emulationProcessTimer.start();

    switch (syncType) {
    case SYNC_TYPE_SYNC:
        ret = m_execute(cameraId,
                srcBuffer, srcRect,
                dstBuffer, dstRect,
                shot_ext);
        break;
    case SYNC_TYPE_BYPASS:
    case SYNC_TYPE_SWITCH:
        /* just memcpy */
        for (int i = 0; i < srcBuffer[0].planeCount; i++) {
            if (srcBuffer[0].fd[i] > 0) {
                memcpy(dstBuffer.addr[i], srcBuffer[0].addr[i], srcBuffer[0].size[i]);
            }
        }
        break;
    default:
        CLOGE("Invalid SyncType(%d)", syncType);
        ret = INVALID_OPERATION;
        break;
    }

    m_emulationProcessTimer.stop();
    m_emulationProcessTime = (int)m_emulationProcessTimer.durationUsecs();

    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_execute() fail", __FUNCTION__, __LINE__);
    }

    return ret;
}

status_t ExynosCameraFusionWrapper::m_execute(int cameraId,
                                              ExynosCameraBuffer srcBuffer[], ExynosRect srcRect[],
                                              ExynosCameraBuffer dstBuffer,   ExynosRect dstRect,
                                              struct camera2_shot_ext shot_ext[], bool isCapture)
{
    // hack for CLOG
    int         m_cameraId = cameraId;
    const char *m_name = "";

#ifdef SAMSUNG_DUAL_SOLUTION
    status_t ret = NO_ERROR;
    unsigned int bpp = 0;
    unsigned int planeCount  = 1;

    UniPluginBufferData_t pluginData;
    for (int i = OUTPUT_NODE_1; i <= OUTPUT_NODE_2; i++) {
        if (i == OUTPUT_NODE_2 &&
            (m_getSyncType() == SYNC_TYPE_SWITCH || m_getSyncType() == SYNC_TYPE_BYPASS))
            break;

        switch (m_getSyncType()) {
        case SYNC_TYPE_BYPASS:
            pluginData.CameraType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
            break;
        case SYNC_TYPE_SYNC:
            if (i == OUTPUT_NODE_1)
                pluginData.CameraType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
            else
                pluginData.CameraType = UNI_PLUGIN_CAMERA_TYPE_TELE;
            break;
        case SYNC_TYPE_SWITCH:
            pluginData.CameraType = UNI_PLUGIN_CAMERA_TYPE_TELE;
            break;
        };

        pluginData.InWidth = srcRect[i].fullW;
        pluginData.InHeight = srcRect[i].fullH;
        pluginData.OutWidth = dstRect.fullW;
        pluginData.OutHeight = dstRect.fullH;

        pluginData.InBuffY = srcBuffer[i].addr[0];
        pluginData.InBuffFd[UNI_PLUGIN_BUFFER_PLANE_Y] = srcBuffer[i].fd[UNI_PLUGIN_BUFFER_PLANE_Y];
        pluginData.OutBuffY = dstBuffer.addr[0];
        pluginData.OutBuffFd[UNI_PLUGIN_BUFFER_PLANE_Y] = dstBuffer.fd[UNI_PLUGIN_BUFFER_PLANE_Y];

        ret = getYuvFormatInfo(srcRect[i].colorFormat, &bpp, &planeCount);
        if (ret < 0) {
            CLOGE2("[Fusion]getYuvFormatInfo(srcRect[%d].colorFormat(%x)) fail",
                        i, srcRect[i].colorFormat);
        }

        pluginData.InFormat = srcRect[i].colorFormat;
        pluginData.OutFormat = srcRect[i].colorFormat;

        switch (planeCount) {
        case 1:
            pluginData.InBuffU = srcBuffer[i].addr[0] + srcRect[i].fullW * srcRect[i].fullH;
            pluginData.OutBuffU = dstBuffer.addr[0]    + dstRect.fullW    * dstRect.fullH;
            break;
        case 2:
            pluginData.InBuffU = srcBuffer[i].addr[1];
            pluginData.InBuffFd[UNI_PLUGIN_BUFFER_PLANE_U] = srcBuffer[i].fd[UNI_PLUGIN_BUFFER_PLANE_U];
            pluginData.OutBuffU = dstBuffer.addr[1];
            pluginData.OutBuffFd[UNI_PLUGIN_BUFFER_PLANE_U] = dstBuffer.fd[UNI_PLUGIN_BUFFER_PLANE_U];
            break;
        default:
            android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):Invalid planeCount(%d), assert!!!!",
                    __FUNCTION__, __LINE__, planeCount);
            break;
        }

        pluginData.Timestamp = (unsigned long long)shot_ext[i].shot.dm.sensor.timeStamp;

        if (isCapture) {
#ifdef SAMSUNG_DUAL_CAPTURE_SOLUTION
            uni_plugin_set(m_getCaptureHandle(), DUAL_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &pluginData);
#endif
        } else {
            uni_plugin_set(m_getPreviewHandle(), DUAL_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &pluginData);
        }
    }

    if (isCapture) {
#ifdef SAMSUNG_DUAL_CAPTURE_SOLUTION
        ret = uni_plugin_process(m_getCaptureHandle());
#endif
    }
    else {
        ret = uni_plugin_process(m_getPreviewHandle());
        uni_plugin_get(m_getPreviewHandle(), DUAL_PREVIEW_PLUGIN_NAME,
            UNI_PLUGIN_INDEX_ARC_CAMERA_INFO, (UniPlugin_DCVOZ_CAMERAPARAM_t*)m_cameraParam);
    }

    if (ret < 0) {
        CLOGE2("[Fusion] plugin_process failed!!");
    }
#else
    status_t ret = NO_ERROR;

    char *srcYAddr    = NULL;
    char *srcCbcrAddr = NULL;

    char *dstYAddr    = NULL;
    char *dstCbcrAddr = NULL;

    unsigned int bpp = 0;
    unsigned int planeCount  = 1;

    int srcPlaneSize = 0;
    int srcHalfPlaneSize = 0;

    int dstPlaneSize = 0;
    int dstHalfPlaneSize = 0;

    int copySize = 0;

    /*
     * if previous emulationProcessTime is slow than 33msec,
     * we need change the next copy time
     *
     * ex :
     * frame 0 :
     * 1.0(copyRatio) = 33333 / 33333(previousFusionProcessTime : init value)
     * 1.0  (copyRatio) = 1.0(copyRatio) * 1.0(m_emulationCopyRatio);
     * m_emulationCopyRatio = 1.0
     * m_emulationProcessTime = 66666

     * frame 1 : because of frame0's low performance, shrink down copyRatio.
     * 0.5(copyRatio) = 33333 / 66666(previousFusionProcessTime)
     * 0.5(copyRatio) = 0.5(copyRatio) * 1.0(m_emulationCopyRatio);
     * m_emulationCopyRatio = 0.5
     * m_emulationProcessTime = 33333

     * frame 2 : acquire the proper copy time
     * 1.0(copyRatio) = 33333 / 33333(previousFusionProcessTime)
     * 0.5(copyRatio) = 1.0(copyRatio) * 0.5(m_emulationCopyRatio);
     * m_emulationCopyRatio = 0.5
     * m_emulationProcessTime = 16666

     * frame 3 : because of frame2's fast performance, increase copyRatio.
     * 2.0(copyRatio) = 33333 / 16666(previousFusionProcessTime)
     * 1.0(copyRatio) = 2.0(copyRatio) * 0.5(m_emulationCopyRatio);
     * m_emulationCopyRatio = 1.0
     * m_emulationProcessTime = 33333
     */
    int previousFusionProcessTime = m_emulationProcessTime;
    if (previousFusionProcessTime <= 0)
        previousFusionProcessTime = 1;

    float copyRatio = (float)FUSION_PROCESSTIME_STANDARD / (float)previousFusionProcessTime;
    copyRatio = copyRatio * m_emulationCopyRatio;

    if (1.0f <= copyRatio) {
        copyRatio = 1.0f;
    } else if (0.1f < copyRatio) {
        copyRatio -= 0.05f; // threshold value : 5%
    } else {
        CLOGW("WARN(%s[%d]):copyRatio(%d) is too smaller than 0.1f. previousFusionProcessTime(%d), m_emulationCopyRatio(%f)",
            __FUNCTION__, __LINE__, copyRatio, previousFusionProcessTime, m_emulationCopyRatio);
    }

    m_emulationCopyRatio = copyRatio;

    for (int i = OUTPUT_NODE_1; i <= OUTPUT_NODE_2; i++) {
        srcPlaneSize = srcRect[i].fullW * srcRect[i].fullH;
        srcHalfPlaneSize = srcPlaneSize / 2;

        dstPlaneSize = dstRect.fullW * dstRect.fullH;
        dstHalfPlaneSize = dstPlaneSize / 2;

        copySize = (srcHalfPlaneSize < dstHalfPlaneSize) ? srcHalfPlaneSize : dstHalfPlaneSize;

        ret = getYuvFormatInfo(srcRect[i].colorFormat, &bpp, &planeCount);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getYuvFormatInfo(srcRect[%d].colorFormat(%x)) fail", __FUNCTION__, __LINE__, i, srcRect[i].colorFormat);
        }

        srcYAddr    = srcBuffer[i].addr[0];
        dstYAddr    = dstBuffer.addr[0];

        switch (planeCount) {
        case 1:
            srcCbcrAddr = srcBuffer[i].addr[0] + srcRect[i].fullW * srcRect[i].fullH;
            dstCbcrAddr = dstBuffer.addr[0]    + dstRect.fullW    * dstRect.fullH;
            break;
        case 2:
            srcCbcrAddr = srcBuffer[i].addr[1];
            dstCbcrAddr = dstBuffer.addr[1];
            break;
        default:
            android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):Invalid planeCount(%d), assert!!!!",
                    __FUNCTION__, __LINE__, planeCount);
            break;
        }

        EXYNOS_CAMERA_FUSION_WRAPPER_DEBUG_LOG("DEBUG(%s[%d]):fusion emulationn running ~~~ memcpy(%d, %d, %d) by src(%d, %d), dst(%d, %d), previousFusionProcessTime(%d) copyRatio(%f)",
            __FUNCTION__, __LINE__,
            dstBuffer.addr[0], srcBuffer[i].addr[0], copySize,
            srcRect[i].fullW, srcRect[i].fullH,
            dstRect.fullW, dstRect.fullH,
            previousFusionProcessTime, copyRatio);

        /* in case of source 2 */
        if (i == OUTPUT_NODE_2) {
            dstYAddr    += dstHalfPlaneSize;
            dstCbcrAddr += dstHalfPlaneSize / 2;
        }

        if (srcRect[i].fullW == dstRect.fullW &&
            srcRect[i].fullH == dstRect.fullH) {
            int oldCopySize = copySize;
            copySize = (int)((float)copySize * copyRatio);

            if (oldCopySize < copySize) {
                CLOGW("WARN(%s[%d]):oldCopySize(%d) < copySize(%d). just adjust oldCopySize",
                    __FUNCTION__, __LINE__, oldCopySize, copySize);

                copySize = oldCopySize;
            }

            memcpy(dstYAddr,    srcYAddr,    copySize);
            memcpy(dstCbcrAddr, srcCbcrAddr, copySize / 2);
        } else {
            int width  = (srcRect[i].fullW < dstRect.fullW) ? srcRect[i].fullW : dstRect.fullW;
            int height = (srcRect[i].fullH < dstRect.fullH) ? srcRect[i].fullH : dstRect.fullH;

            int oldHeight = height;
            height = (int)((float)height * copyRatio);

            if (oldHeight < height) {
                CLOGW("WARN(%s[%d]):oldHeight(%d) < height(%d). just adjust oldHeight",
                    __FUNCTION__, __LINE__, oldHeight, height);

                height = oldHeight;
            }

            for (int h = 0; h < height / 2; h++) {
                memcpy(dstYAddr,    srcYAddr,    width);
                srcYAddr += srcRect[i].fullW;
                dstYAddr += dstRect.fullW;
            }

            for (int h = 0; h < height / 4; h++) {
                memcpy(dstCbcrAddr, srcCbcrAddr, width);
                srcCbcrAddr += srcRect[i].fullW;
                dstCbcrAddr += dstRect.fullW;
            }
        }
    }
#endif

    return ret;
}
void ExynosCameraFusionWrapper::m_init(int cameraId)
{
    // hack for CLOG
    int         m_cameraId = cameraId;
    const char *m_name = "";

    getDualCameraId(&m_mainCameraId, &m_subCameraId);

    CLOGD("DEBUG(%s[%d]):m_mainCameraId(CAMERA_ID_%d), m_subCameraId(CAMERA_ID_%d)",
        __FUNCTION__, __LINE__, m_mainCameraId, m_subCameraId);

    for (int i = 0; i < CAMERA_ID_MAX; i++) {
        m_flagValidCameraId[i] = false;
    }

    m_flagValidCameraId[m_mainCameraId] = true;
    m_flagValidCameraId[m_subCameraId] = true;

    m_emulationProcessTime = FUSION_PROCESSTIME_STANDARD;
    m_emulationCopyRatio = 1.0f;
}


#ifdef SAMSUNG_DUAL_SOLUTION
void ExynosCameraFusionWrapper::m_setCurZoomRatio(int cameraId, float zoomRatio)
{
    if (cameraId == CAMERA_ID_BACK) {
        m_wideZoomRatio = zoomRatio / 1000.f;
    }
    else if (cameraId == CAMERA_ID_BACK_1) {
        m_teleZoomRatio = zoomRatio / 1000.f;
    }
}

void ExynosCameraFusionWrapper::m_setCurViewRatio(int cameraId, float viewRatio)
{
    if (cameraId == CAMERA_ID_BACK) {
        m_wideViewRatio = viewRatio / 1000.f;
    }
    else if (cameraId == CAMERA_ID_BACK_1) {
        m_teleViewRatio = viewRatio / 1000.f;
    }
}

float ExynosCameraFusionWrapper::m_getCurZoomRatio(UNI_PLUGIN_CAMERA_TYPE cameraType)
{
    if (cameraType == UNI_PLUGIN_CAMERA_TYPE_WIDE) {
        return m_wideZoomRatio;
    }
    else if (cameraType == UNI_PLUGIN_CAMERA_TYPE_TELE) {
        return m_teleZoomRatio;
    }

    return 1.0f;
}

float ExynosCameraFusionWrapper::m_getCurViewRatio(UNI_PLUGIN_CAMERA_TYPE cameraType)
{
    if (cameraType == UNI_PLUGIN_CAMERA_TYPE_WIDE) {
        return m_wideViewRatio;
    }
    else if (cameraType == UNI_PLUGIN_CAMERA_TYPE_TELE) {
        return m_teleViewRatio;
    }

    return 1.0f;
}

float ExynosCameraFusionWrapper::m_getZoomRatio(int cameraID, int zoomLevel)
{
    if (cameraID == CAMERA_ID_BACK) {
        if (m_wideImageRatioList != NULL)
            return m_wideImageRatioList[zoomLevel] * 1000.0;
    } else if (cameraID == CAMERA_ID_BACK_1) {
        if (m_teleImageRatioList != NULL)
            return m_teleImageRatioList[zoomLevel] * 1000.0;
    }
    CLOGE2("[Fusion] CameraID: %d, image ratio list is NULL");
    return 1000.0f;
}



void ExynosCameraFusionWrapper::m_setFocusStatus(int cameraId, int AFstatus)
{
    if (cameraId == CAMERA_ID_BACK) {
        m_wideAfStatus = AFstatus;

    } else {
        m_teleAfStatus = AFstatus;
    }
}

int ExynosCameraFusionWrapper::m_getFocusStatus(int cameraId)
{
    if (cameraId == CAMERA_ID_BACK) {
        return m_wideAfStatus;

    } else {
        return m_teleAfStatus;
    }
}

int ExynosCameraFusionWrapper::m_getCurDisplayedCam(void)
{
    if (m_displayedCamera == UNI_PLUGIN_CAMERA_TYPE_WIDE) {
        return CAMERA_ID_BACK;
    } else {
        return CAMERA_ID_BACK_1;
    }
}

void ExynosCameraFusionWrapper::m_setCurDisplayedCam(int cameraId)
{
    if (cameraId == CAMERA_ID_BACK) {
        m_displayedCamera = UNI_PLUGIN_CAMERA_TYPE_WIDE;
    } else {
        m_displayedCamera = UNI_PLUGIN_CAMERA_TYPE_TELE;
    }
}

status_t ExynosCameraFusionWrapper::m_getDualCalDataFromSysfs(void)
{
    FILE *fp = NULL;
    char filePath[80];
    status_t ret = NO_ERROR;
    size_t result = 0;

    memset(filePath, 0, sizeof(filePath));
    snprintf(filePath, sizeof(DUAL_CAL_DATA_PATH), "%s", DUAL_CAL_DATA_PATH);
    fp = fopen(filePath, "r");
    if (fp == NULL) {
        CLOGE2("failed to open sysfs");
        ret = BAD_VALUE;
        goto EXIT;
    }

    result = fread(m_calData, 1, DUAL_CAL_DATA_SIZE, fp);
    if(result < DUAL_CAL_DATA_SIZE)
        ret = BAD_VALUE;

     fclose(fp);

EXIT:
    return ret;
}

void ExynosCameraFusionWrapper::m_setSyncType(sync_type_t syncType)
{
    m_syncType = syncType;
}

sync_type_t ExynosCameraFusionWrapper::m_getSyncType(void)
{
    return m_syncType;
}

int ExynosCameraFusionWrapper::m_getNeedMargin(int cameraID, int zoomLevel)
{
    if (cameraID == CAMERA_ID_BACK) {
        if (m_wideNeedMarginList != NULL)
            return m_wideNeedMarginList[zoomLevel];
    } else if (cameraID == CAMERA_ID_BACK_1) {
        if (m_teleNeedMarginList != NULL)
            return m_teleNeedMarginList[zoomLevel];
    }
    CLOGE2("[Fusion] CameraID: %d, needMargin list is NULL");
    return 1;
}

void ExynosCameraFusionWrapper::m_setSolutionHandle(void* previewHandle, void* captureHandle)
{
    m_previewHandle = previewHandle;
    m_captureHandle = captureHandle;
}
void* ExynosCameraFusionWrapper::m_getPreviewHandle()
{
    return m_previewHandle;
}
void* ExynosCameraFusionWrapper::m_getCaptureHandle()
{
    return m_captureHandle;
}

void ExynosCameraFusionWrapper::m_setOrientation(int orientation)
{
    m_orientation = orientation;
}

int ExynosCameraFusionWrapper::m_getOrientation()
{
    return m_orientation;
}

#ifdef SAMSUNG_DUAL_CAPTURE_SOLUTION
void ExynosCameraFusionWrapper::m_setCropROI(UNI_PLUGIN_CAMERA_TYPE cameraType, UTrect cropROI)
{
    if (cameraType == UNI_PLUGIN_CAMERA_TYPE_WIDE) {
        m_WideCropROI = cropROI;
    } else if (cameraType == UNI_PLUGIN_CAMERA_TYPE_TELE) {
        m_TeleCropROI = cropROI;
    }
}
UTrect* ExynosCameraFusionWrapper::m_getCropROI(UNI_PLUGIN_CAMERA_TYPE cameraType)
{
    UTrect rect;
    if (cameraType == UNI_PLUGIN_CAMERA_TYPE_WIDE) {
        return (&m_WideCropROI);
    } else if (cameraType == UNI_PLUGIN_CAMERA_TYPE_TELE) {
        return (&m_TeleCropROI);
    }
    return (&rect);
}
void ExynosCameraFusionWrapper::m_setCropROIRatio(UNI_PLUGIN_CAMERA_TYPE cameraType, float cropROIRatio)
{
    if (cameraType == UNI_PLUGIN_CAMERA_TYPE_WIDE) {
        m_WideCropROIRatio = cropROIRatio;
    } else if (cameraType == UNI_PLUGIN_CAMERA_TYPE_TELE) {
        m_TeleCropROIRatio = cropROIRatio;
    }
}
float ExynosCameraFusionWrapper::m_getCropROIRatio(UNI_PLUGIN_CAMERA_TYPE cameraType)
{
    if (cameraType == UNI_PLUGIN_CAMERA_TYPE_WIDE) {
        return m_WideCropROIRatio;
    } else if (cameraType == UNI_PLUGIN_CAMERA_TYPE_TELE) {
        return m_TeleCropROIRatio;
    }
    return 0.f;
}
#endif //SAMSUNG_DUAL_CAPTURE_SOLUTION
void ExynosCameraFusionWrapper::m_setCameraParam(UniPlugin_DCVOZ_CAMERAPARAM_t* param)
{
    m_cameraParam = param;
}

void ExynosCameraFusionWrapper::m_setForceWide(void)
{
    UNI_PLUGIN_CAMERA_TYPE cameraType = UNI_PLUGIN_CAMERA_TYPE_TELE;

    /* 'TELE' means disabling force wide mode */
    if (m_forceWide)
        cameraType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
    else
        cameraType = UNI_PLUGIN_CAMERA_TYPE_TELE;

    uni_plugin_set(m_getPreviewHandle(), DUAL_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_ARC_FALLBACK_SELECT, &cameraType);
}

int ExynosCameraFusionWrapper::m_getCameraParamIndex(void)
{
    if (m_cameraParam->CameraIndex == UNI_PLUGIN_CAMERA_TYPE_WIDE)
        return CAMERA_ID_BACK;
    else if (m_cameraParam->CameraIndex == UNI_PLUGIN_CAMERA_TYPE_TELE)
        return CAMERA_ID_BACK_1;

    return CAMERA_ID_BACK;
}

UniPlugin_DCVOZ_CAMERAPARAM_t* ExynosCameraFusionWrapper::m_getCameraParam(void)
{
    return (m_cameraParam);
}
#endif

/* for Preview fusion wrapper */
ExynosCameraFusionPreviewWrapper::ExynosCameraFusionPreviewWrapper()
{
    CLOGD2("new ExynosCameraFusionPreviewWrapper object allocated");
#ifdef SAMSUNG_DUAL_SOLUTION
    m_previewHandle = NULL;
#endif
}

ExynosCameraFusionPreviewWrapper::~ExynosCameraFusionPreviewWrapper()
{
}

#ifdef SAMSUNG_DUAL_SOLUTION
status_t ExynosCameraFusionPreviewWrapper::execute(int cameraId,
                                            sync_type_t syncType,
                                            struct camera2_shot_ext shot_ext[],
                                            DOF *dof[],
                                            ExynosCameraBuffer srcBuffer[],
                                            ExynosRect srcRect[],
                                            ExynosCameraBufferManager *srcBufferManager[],
                                            ExynosCameraBuffer dstBuffer,
                                            ExynosRect dstRect,
                                            ExynosCameraBufferManager *dstBufferManager)
{
    status_t ret = NO_ERROR;

    if (this->flagCreate(cameraId) == false) {
        CLOGE2("[Fusion] flagCreate(%d) == false. so fail", cameraId);
        return INVALID_OPERATION;
    }

    m_emulationProcessTimer.start();
    UniPlugin_DCVOZ_RatioParam_t zoom;
    int isWideOpened, isTeleOpened;
    UNI_PLUGIN_CAMERA_TYPE cameraType;
    memset(&zoom, 0, sizeof(zoom));

    for (int i = OUTPUT_NODE_1; i <= OUTPUT_NODE_2; i++) {
        if (i == OUTPUT_NODE_2 &&
            (m_getSyncType() == SYNC_TYPE_SWITCH || m_getSyncType() == SYNC_TYPE_BYPASS))
            break;

        switch (m_getSyncType()) {
        case SYNC_TYPE_BYPASS:
            cameraType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
            isWideOpened = 1;
            isTeleOpened = 0;
            break;
        case SYNC_TYPE_SYNC:
            if (i == OUTPUT_NODE_1)
                cameraType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
            else
                cameraType = UNI_PLUGIN_CAMERA_TYPE_TELE;

            isWideOpened = 1;
            isTeleOpened = 1;
            break;
        case SYNC_TYPE_SWITCH:
            cameraType = UNI_PLUGIN_CAMERA_TYPE_TELE;
            isWideOpened = 0;
            isTeleOpened = 1;
            break;
        };

        if (i == OUTPUT_NODE_1){
            uni_plugin_set(m_getPreviewHandle(), DUAL_PREVIEW_PLUGIN_NAME,
                            UNI_PLUGIN_INDEX_ARC_WIDE_CAM_OPEN_FLAG, &isWideOpened);
            uni_plugin_set(m_getPreviewHandle(), DUAL_PREVIEW_PLUGIN_NAME,
                            UNI_PLUGIN_INDEX_ARC_TELE_CAM_OPEN_FLAG, &isTeleOpened);
        }

        UniPluginExtraBufferInfo_t extraBufferInfo;
        extraBufferInfo.CameraType = cameraType;
        extraBufferInfo.exposureValue.den = 256;
        extraBufferInfo.exposureValue.snum = (int32_t)shot_ext[i].shot.udm.ae.vendorSpecific[4];
        extraBufferInfo.exposureTime.num = (int32_t)shot_ext[i].shot.udm.ae.vendorSpecific[64];
        extraBufferInfo.iso[0] = (int32_t)shot_ext[i].shot.udm.ae.vendorSpecific[63];
        extraBufferInfo.orientation = (UNI_PLUGIN_DEVICE_ORIENTATION)m_getOrientation();
        extraBufferInfo.objectDistanDistanceCm = (int32_t)shot_ext[i].shot.dm.aa.vendor_objectDistanceCm;
        uni_plugin_set(m_getPreviewHandle(), DUAL_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO, &extraBufferInfo);

        UniPluginFocusData_t focusData;
        focusData.CameraType = cameraType;
        ExynosCameraActivityAutofocus::AUTOFOCUS_STATE afState =  ExynosCameraActivityAutofocus::afState2AUTOFOCUS_STATE(shot_ext[i].shot.dm.aa.afState);
        focusData.FocusState = afState;
        uni_plugin_set(m_getPreviewHandle(), DUAL_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_FOCUS_INFO, &focusData);

        if (cameraType == UNI_PLUGIN_CAMERA_TYPE_WIDE) {
            zoom.fWideImageRatio = m_getCurZoomRatio(cameraType);
            zoom.fWideViewRatio = m_getCurViewRatio(cameraType);
        } else if (cameraType == UNI_PLUGIN_CAMERA_TYPE_TELE) {
            zoom.fTeleImageRatio = m_getCurZoomRatio(cameraType);
            zoom.fTeleViewRatio = m_getCurViewRatio(cameraType);
        }
    }

    uni_plugin_set(m_getPreviewHandle(), DUAL_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_ARC_ZOOM_RATIO, &zoom);

    ret = m_execute(cameraId,
                    srcBuffer, srcRect,
                    dstBuffer, dstRect,
                    shot_ext, false);
    m_emulationProcessTimer.stop();
    m_emulationProcessTime = (int)m_emulationProcessTimer.durationUsecs();

    if (ret != NO_ERROR) {
        CLOGE2("[Fusion] m_execute() fail");
    }

    return ret;
}

status_t ExynosCameraFusionPreviewWrapper::m_initDualSolution(int cameraId, int sensorW, int sensorH, int previewW, int previewH,
                                                                    int * zoomList, int zoomListSize)
{
    status_t ret = NO_ERROR;

    if (m_previewHandle != NULL && cameraId == CAMERA_ID_BACK) {
        ret = m_getDualCalDataFromSysfs();
        if (ret != NO_ERROR) {
            CLOGE2("[Fusion] Calibration data read failed!!");
            goto EXIT;
        }

        UniPlugin_DCOZ_InitParam_t initParam;
        initParam.fWideFov = WIDE_CAMERA_FOV;
        initParam.fTeleFov = TELE_CAMERA_FOV;
        initParam.pCalData = m_calData;
        initParam.i32CalDataLen = DUAL_CAL_DATA_SIZE;
        initParam.fnCallback = NULL;
        initParam.pUserData = NULL;
        uni_plugin_set(m_previewHandle, DUAL_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_ARC_INIT_PARAMS, &initParam);

        uni_plugin_init(m_previewHandle);

        UniPlugin_DCVOZ_CameraImageInfo_t imageInfo;
        imageInfo.i32WideFullWidth = sensorW;
       	imageInfo.i32WideFullHeight = sensorH;
       	imageInfo.i32TeleFullWidth = sensorW;
        imageInfo.i32TeleFullHeight = sensorH;
       	imageInfo.i32WideWidth = previewW;
        imageInfo.i32WideHeight = previewH;
       	imageInfo.i32TeleWidth = previewW;
       	imageInfo.i32TeleHeight = previewH;
       	imageInfo.i32DstWidth = previewW;
       	imageInfo.i32DstHeight = previewH;
        uni_plugin_set(m_previewHandle, DUAL_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_ARC_VFE_OUTPUT_DIMENSION, &imageInfo);

        m_viewRatioList = new float[zoomListSize];
        for(int i = 0; i < zoomListSize; i++) {
            m_viewRatioList[i] = (float)(zoomList[i] / 1000.f);
        }

        UniPlugin_DCOZ_ZoomListParam_t zoomInfo;
        zoomInfo.pfWideZoomList = m_viewRatioList;
        zoomInfo.i32WideZoomListSize = zoomListSize;
        zoomInfo.pfTeleZoomList = m_viewRatioList;
        zoomInfo.i32TeleZoomListSize = zoomListSize;
        uni_plugin_set(m_previewHandle, DUAL_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_ARC_ZOOM_RATIO_LIST, &zoomInfo);
        uni_plugin_get(m_previewHandle, DUAL_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_ARC_ZOOM_RATIO_LIST, &zoomInfo);
        m_wideImageRatioList = zoomInfo.outWideImageRatioList;
        m_teleImageRatioList = zoomInfo.outTeleImageRatioList;
        m_wideNeedMarginList = zoomInfo.outWideNeedMarginList;
        m_teleNeedMarginList = zoomInfo.outTeleNeedMarginList;

        m_isInit = true;
        m_wideAfStatus = -1;
        m_teleAfStatus = -1;
        m_displayedCamera = -1;
        m_forceWide = false;
    }

EXIT:
    return ret;
}

status_t ExynosCameraFusionPreviewWrapper::m_deinitDualSolution(int cameraId)
{
    status_t ret = NO_ERROR;

    if (cameraId == CAMERA_ID_BACK && m_previewHandle != NULL) {
        ret = uni_plugin_deinit(m_previewHandle);
        if(ret < 0) {
            CLOGE2("[Fusion] Plugin deinit failed!!");
            return INVALID_OPERATION;
        }

        if(m_viewRatioList != NULL)
            delete []m_viewRatioList;

        m_isInit = false;
        m_wideAfStatus = -1;
        m_teleAfStatus = -1;
        m_displayedCamera = -1;
    }

    return ret;
}
#endif

/* for Capture fusion wrapper */
ExynosCameraFusionCaptureWrapper::ExynosCameraFusionCaptureWrapper()
{
    CLOGD2("new ExynosCameraFusionCaptureWrapper object allocated");
#ifdef SAMSUNG_DUAL_SOLUTION
    m_captureHandle = NULL;
#endif
}

ExynosCameraFusionCaptureWrapper::~ExynosCameraFusionCaptureWrapper()
{
}

#ifdef SAMSUNG_DUAL_CAPTURE_SOLUTION
status_t ExynosCameraFusionCaptureWrapper::m_calRoiRect(int cameraId, ExynosRect mainRoiRect, ExynosRect subRoiRect)
{
    UniPluginBufferData_t pluginMainData, pluginSubData;
    UniPlugin_DCIOZ_RatioParam_t zoomInfo;
    UniPlugin_DCIOZ_CropROI_Info_t cropRoiInfo;
    UNI_PLUGIN_CAMERA_TYPE cameraType, otherCameraType;

    if (cameraId == CAMERA_ID_BACK_0) {
        cameraType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
        otherCameraType = UNI_PLUGIN_CAMERA_TYPE_TELE;
    } else if (cameraId == CAMERA_ID_BACK_1) {
        cameraType = UNI_PLUGIN_CAMERA_TYPE_TELE;
        otherCameraType = UNI_PLUGIN_CAMERA_TYPE_TELE;
    }
    pluginMainData.CameraType = cameraType;
    pluginMainData.InWidth = mainRoiRect.w;
    pluginMainData.InHeight = mainRoiRect.h;

    zoomInfo.fViewRatio = m_getCurViewRatio(cameraType);
    zoomInfo.fWideImageRatio = m_getCurZoomRatio(cameraType);
    if (m_getSyncType() == SYNC_TYPE_SYNC)
        zoomInfo.fTeleImageRatio = 1.0f;
    else
        zoomInfo.fTeleImageRatio = m_getCurZoomRatio(otherCameraType);

    if (m_getSyncType() == SYNC_TYPE_SYNC) {
        pluginSubData.CameraType = otherCameraType;
        pluginSubData.InWidth = subRoiRect.w;
        pluginSubData.InHeight = subRoiRect.h;
        uni_plugin_set(m_getCaptureHandle(), DUAL_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &pluginSubData);
    }
    uni_plugin_set(m_getCaptureHandle(), DUAL_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &pluginMainData);
    uni_plugin_set(m_getCaptureHandle(), DUAL_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_ARC_ZOOM_RATIO, &zoomInfo);
    uni_plugin_set(m_getCaptureHandle(), DUAL_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_ARC_CROP_ROI, m_getCameraParam());
    uni_plugin_get(m_getCaptureHandle(), DUAL_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_ARC_CROP_ROI, &cropRoiInfo);

    if (cameraType == UNI_PLUGIN_CAMERA_TYPE_WIDE) {
        m_setCropROI(cameraType, cropRoiInfo.outWideCropROI);
        m_setCropROIRatio(cameraType, cropRoiInfo.outWideCropROIRatio);
        if (m_getSyncType() == SYNC_TYPE_SYNC) {
            m_setCropROI(otherCameraType, cropRoiInfo.outTeleCropROI);
            m_setCropROIRatio(otherCameraType, cropRoiInfo.outTeleCropROIRatio);
        }
    } else if (cameraType == UNI_PLUGIN_CAMERA_TYPE_TELE) {
        m_setCropROI(cameraType, cropRoiInfo.outTeleCropROI);
        m_setCropROIRatio(cameraType, cropRoiInfo.outTeleCropROIRatio);
    }
    return NO_ERROR;
}
#endif //SAMSUNG_DUAL_CAPTURE_SOLUTION

#ifdef SAMSUNG_DUAL_SOLUTION
status_t ExynosCameraFusionCaptureWrapper::execute(int cameraId,
                                            sync_type_t syncType,
                                            struct camera2_shot_ext shot_ext[],
                                            DOF *dof[],
                                            ExynosCameraBuffer srcBuffer[],
                                            ExynosRect srcRect[],
                                            ExynosCameraBufferManager *srcBufferManager[],
                                            ExynosCameraBuffer dstBuffer,
                                            ExynosRect dstRect,
                                            ExynosCameraBufferManager *dstBufferManager)
{
    status_t ret = NO_ERROR;

    if (this->flagCreate(cameraId) == false) {
        CLOGE2("[Fusion] flagCreate(%d) == false. so fail", cameraId);
        return INVALID_OPERATION;
    }

    m_emulationProcessTimer.start();

#ifdef SAMSUNG_DUAL_CAPTURE_SOLUTION
    UniPlugin_DCIOZ_RatioParam_t zoomInfo;
    UniPluginExtraBufferInfo_t extraBufferInfo;
    UniPluginFocusData_t focusData;

    memset(&zoomInfo, 0, sizeof(UniPlugin_DCIOZ_RatioParam_t));
    memset(&extraBufferInfo, 0, sizeof(UniPluginExtraBufferInfo_t));
    memset(&focusData, 0, sizeof(UniPluginFocusData_t));
#endif
    for (int i = OUTPUT_NODE_1; i <= OUTPUT_NODE_2; i++) {
        if (i == OUTPUT_NODE_2 &&
            (m_getSyncType() == SYNC_TYPE_SWITCH || m_getSyncType() == SYNC_TYPE_BYPASS))
            break;
#ifdef SAMSUNG_DUAL_CAPTURE_SOLUTION
        if (i == OUTPUT_NODE_1) {
            extraBufferInfo.CameraType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
            focusData.CameraType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
            zoomInfo.fViewRatio = m_getCurViewRatio(UNI_PLUGIN_CAMERA_TYPE_WIDE);
            zoomInfo.fWideImageRatio = m_getCropROIRatio(UNI_PLUGIN_CAMERA_TYPE_WIDE);
        } else if (i == OUTPUT_NODE_2) {
            extraBufferInfo.CameraType = UNI_PLUGIN_CAMERA_TYPE_TELE;
            focusData.CameraType = UNI_PLUGIN_CAMERA_TYPE_TELE;
            zoomInfo.fTeleImageRatio = m_getCropROIRatio(UNI_PLUGIN_CAMERA_TYPE_TELE);
        }

        ExynosCameraActivityAutofocus::AUTOFOCUS_STATE afState = ExynosCameraActivityAutofocus::afState2AUTOFOCUS_STATE(shot_ext[i].shot.dm.aa.afState);
        focusData.FocusState = afState;

        extraBufferInfo.iso[0] = (int32_t)shot_ext[i].shot.udm.ae.vendorSpecific[63];
        extraBufferInfo.objectDistanDistanceCm = (int32_t)shot_ext[i].shot.dm.aa.vendor_objectDistanceCm;

        CLOGD2("[Fusion] iso(%d)", extraBufferInfo.iso[0]);
        CLOGD2("[Fusion] focus(%d)", focusData.FocusState);

        uni_plugin_set(m_getCaptureHandle(), DUAL_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO, &extraBufferInfo);
        if (ret < 0) {
            CLOGE2("[Fusion](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO) failed!!", __FUNCTION__, __LINE__);
        }
        uni_plugin_set(m_getCaptureHandle(), DUAL_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_FOCUS_INFO, &focusData);
        if (ret < 0) {
            CLOGE2("[Fusion](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_FOCUS_INFO) failed!!", __FUNCTION__, __LINE__);
        }
#endif
    }
#ifdef SAMSUNG_DUAL_CAPTURE_SOLUTION
    uni_plugin_set(m_getCaptureHandle(), DUAL_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_ARC_ZOOM_RATIO, &zoomInfo);
    if (ret < 0) {
        CLOGE2("[Fusion](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_ARC_ZOOM_RATIO) failed!!", __FUNCTION__, __LINE__);
    }
#endif

    ret = m_execute(cameraId,
                    srcBuffer, srcRect,
                    dstBuffer, dstRect,
                    shot_ext, true);
    m_emulationProcessTimer.stop();
    m_emulationProcessTime = (int)m_emulationProcessTimer.durationUsecs();

    if (ret != NO_ERROR) {
        CLOGE2("[Fusion] m_execute() fail");
    }

    return ret;
}

status_t ExynosCameraFusionCaptureWrapper::m_initDualSolution(int cameraId, int sensorW, int sensorH, int captureW, int captureH,
                                                                    int * zoomList, int zoomListSize)
{
    status_t ret = NO_ERROR;

    if (m_captureHandle != NULL && cameraId == CAMERA_ID_BACK) {
        ret = m_getDualCalDataFromSysfs();
        if (ret != NO_ERROR) {
            CLOGE2("[Fusion] Calibration data read failed!!");
            goto EXIT;
        }

        UniPlugin_DCOZ_InitParam_t initParam;
        initParam.fWideFov = WIDE_CAMERA_FOV;
        initParam.fTeleFov = TELE_CAMERA_FOV;
        initParam.pCalData = m_calData;
        initParam.i32CalDataLen = DUAL_CAL_DATA_SIZE;
        initParam.fnCallback = NULL;
        initParam.pUserData = NULL;
        uni_plugin_set(m_captureHandle, DUAL_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_ARC_INIT_PARAMS, &initParam);

        uni_plugin_init(m_captureHandle);

        UniPlugin_DCVOZ_CameraImageInfo_t imageInfo;
        imageInfo.i32WideFullWidth = sensorW;
        imageInfo.i32WideFullHeight = sensorH;
        imageInfo.i32TeleFullWidth = sensorW;
        imageInfo.i32TeleFullHeight = sensorH;
        imageInfo.i32WideWidth = captureW;
        imageInfo.i32WideHeight = captureH;
        imageInfo.i32TeleWidth = captureW;
        imageInfo.i32TeleHeight = captureH;
        imageInfo.i32DstWidth = captureW;
        imageInfo.i32DstHeight = captureH;
        uni_plugin_set(m_captureHandle, DUAL_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_ARC_VFE_OUTPUT_DIMENSION, &imageInfo);

        m_isInit = true;
        m_wideAfStatus = -1;
        m_teleAfStatus = -1;
        m_displayedCamera = -1;
    }

EXIT:
    return ret;
}

status_t ExynosCameraFusionCaptureWrapper::m_deinitDualSolution(int cameraId)
{
    status_t ret = NO_ERROR;

    if (m_captureHandle != NULL && cameraId == CAMERA_ID_BACK) {
        ret = uni_plugin_deinit(m_captureHandle);
        if (ret < 0) {
            CLOGE2("[Fusion] Plugin deinit failed!!");
            return INVALID_OPERATION;
        }

        m_isInit = false;
        m_wideAfStatus = -1;
        m_teleAfStatus = -1;
        m_displayedCamera = -1;
    }

    return ret;
}
#endif

