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
#include "ExynosCameraUtils.h"

#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
Mutex ExynosCameraFusionWrapper::m_bokehCaptureProcessingLock;
int ExynosCameraFusionWrapper::m_bokehCaptureProcessing;
#endif

ExynosCameraFusionWrapper::ExynosCameraFusionWrapper()
{
    ALOGD("DEBUG(%s[%d]):new ExynosCameraFusionWrapper object allocated", __FUNCTION__, __LINE__);

    for (int i = 0; i < CAMERA_ID_MAX; i++) {
        m_flagCreated[i] = false;

        m_width[i] = 0;
        m_height[i] = 0;
        m_stride[i] = 0;
    }
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    m_cameraParam = new UniPluginDualCameraParam_t;

    m_fallback = false;
    m_wideZoomMargin = 0;
    m_teleZoomMargin = 0;
#endif

    m_scenario = SCENARIO_NORMAL;

#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
    m_vraInputSizeWidth = 0;
    m_vraInputSizeHeight = 0;

    m_bokehBlurStrength = 0;

    m_hwBcropSizeWidth = 0;
    m_hwBcropSizeHeight = 0;

    m_BokehPreviewResultValue = 0;
    m_BokehCaptureResultValue = 0;
    m_BokehPreviewProcessResult = 0;
#ifdef SAMSUNG_DUAL_PORTRAIT_BEAUTY
    m_beautyFaceRetouchLevel = 0;
    m_beautyFaceSkinColorLevel = 0;
#endif
    m_CurrentBokehPreviewResult = 0;
    m_bokehCaptureProcessing = 0;

#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
    m_prevDualCaptureFlag = -1;
    m_dualCaptureFlag = -1;
#endif
#endif /* SAMSUNG_DUAL_PORTRAIT_SOLUTION */
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
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
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

    if (srcWidth == 0 || srcHeight == 0 || dstWidth == 0 || dstHeight == 0) {
        CLOGE("ERR(%s[%d]):srcWidth == %d || srcHeight == %d || dstWidth == %d || dstHeight == %d. so, fail",
            __FUNCTION__, __LINE__, srcWidth, srcHeight, dstWidth, dstHeight);
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
                                            struct camera2_shot_ext shot_ext[],
                                            ExynosCameraBuffer srcBuffer[],
                                            ExynosRect srcRect[],
                                            ExynosCameraBufferManager *srcBufferManager[],
                                            ExynosCameraBuffer dstBuffer,
                                            ExynosRect dstRect,
                                            ExynosCameraBufferManager *dstBufferManager
#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
                                            , int multiShotCount,
                                            int LDCaptureTotalCount
#endif
                                            )
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

    /* just memcpy */
    for (int i = 0; i < srcBuffer[0].planeCount; i++) {
        if (srcBuffer[0].fd[i] > 0) {
            memcpy(dstBuffer.addr[i], srcBuffer[0].addr[i], srcBuffer[0].size[i]);
        }
    }

#if 0
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
#endif

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
                                              struct camera2_shot_ext shot_ext[], bool isCapture
#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
                                              , int multiShotCount,
                                              int LDCaptureTotalCount
#endif
                                              )
{
    // hack for CLOG
    int         m_cameraId = cameraId;
    const char *m_name = "";

    status_t ret = NO_ERROR;

#if defined (SAMSUNG_DUAL_ZOOM_PREVIEW) || defined (SAMSUNG_DUAL_PORTRAIT_SOLUTION)
    if (m_getScenario() == SCENARIO_DUAL_REAR_ZOOM) {
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
        unsigned int bpp = 0;
        unsigned int planeCount  = 1;
#ifdef SAMSUNG_DUAL_SOLUTION_DEBUG
        int framecount = 0;
#endif
        UniPluginBufferData_t pluginData;
        for (int i = OUTPUT_NODE_1; i <= OUTPUT_NODE_2; i++) {
            if (i == OUTPUT_NODE_2 &&
                (m_getFrameType() == FRAME_TYPE_PREVIEW || m_getFrameType() == FRAME_TYPE_PREVIEW_SLAVE))
                break;

            switch (m_getFrameType()) {
            case FRAME_TYPE_PREVIEW:
                pluginData.cameraType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
                break;
            case FRAME_TYPE_PREVIEW_DUAL_MASTER:
            case FRAME_TYPE_REPROCESSING_DUAL_MASTER:
                if (i == OUTPUT_NODE_1)
                    pluginData.cameraType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
                else
                    pluginData.cameraType = UNI_PLUGIN_CAMERA_TYPE_TELE;
                break;
            case FRAME_TYPE_PREVIEW_SLAVE:
                pluginData.cameraType = UNI_PLUGIN_CAMERA_TYPE_TELE;
                break;
            };

            pluginData.inWidth = srcRect[i].fullW;
            pluginData.inHeight = srcRect[i].fullH;
            pluginData.outWidth = dstRect.fullW;
            pluginData.outHeight = dstRect.fullH;

            pluginData.inBuffY = srcBuffer[i].addr[0];
            pluginData.inBuffFd[UNI_PLUGIN_BUFFER_PLANE_Y] = srcBuffer[i].fd[UNI_PLUGIN_BUFFER_PLANE_Y];
            pluginData.outBuffY = dstBuffer.addr[0];
            pluginData.outBuffFd[UNI_PLUGIN_BUFFER_PLANE_Y] = dstBuffer.fd[UNI_PLUGIN_BUFFER_PLANE_Y];

            ret = getV4l2FormatInfo(srcRect[i].colorFormat, &bpp, &planeCount);
            if (ret < 0) {
                CLOGE2("[Fusion] getV4l2FormatInfo (srcRect[%d].colorFormat(%x)) fail",
                            i, srcRect[i].colorFormat);
                /* HACK */
                bpp = 12;
                planeCount = 1;
                srcRect[i].colorFormat = V4L2_PIX_FMT_NV21M;
            }

            pluginData.inFormat = (UNI_PLUGIN_COLORFORMAT)srcRect[i].colorFormat;
            pluginData.outFormat = (UNI_PLUGIN_COLORFORMAT)srcRect[i].colorFormat;

            switch (planeCount) {
            case 1:
                pluginData.inBuffU = srcBuffer[i].addr[0] + srcRect[i].fullW * srcRect[i].fullH;
                pluginData.outBuffU = dstBuffer.addr[0]    + dstRect.fullW    * dstRect.fullH;
                break;
            case 2:
                pluginData.inBuffU = srcBuffer[i].addr[1];
                pluginData.inBuffFd[UNI_PLUGIN_BUFFER_PLANE_U] = srcBuffer[i].fd[UNI_PLUGIN_BUFFER_PLANE_U];
                pluginData.inBuffFd[UNI_PLUGIN_BUFFER_PLANE_V] = 0;
                pluginData.outBuffU = dstBuffer.addr[1];
                pluginData.outBuffFd[UNI_PLUGIN_BUFFER_PLANE_U] = dstBuffer.fd[UNI_PLUGIN_BUFFER_PLANE_U];
                pluginData.outBuffFd[UNI_PLUGIN_BUFFER_PLANE_V] = 0;
                break;
            default:
                android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):Invalid planeCount(%d), assert!!!!",
                        __FUNCTION__, __LINE__, planeCount);
                break;
            }

            pluginData.timestamp = (unsigned long long)shot_ext[i].shot.udm.sensor.timeStampBoot;

            ret = getV4l2FormatInfo(srcRect[i].colorFormat, &bpp, &planeCount);
            if (ret < 0) {
                CLOGE2("[Fusion] getV4l2FormatInfo (srcRect[%d].colorFormat(%x)) fail",
                            i, srcRect[i].colorFormat);
            }

#ifdef SAMSUNG_DUAL_SOLUTION_DEBUG
            framecount = shot_ext[OUTPUT_NODE_1].shot.dm.request.frameCount;
#endif

            if (isCapture) {
#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
                uni_plugin_set(m_getCaptureHandle(), DUAL_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &pluginData);
#ifdef SAMSUNG_DUAL_SOLUTION_DEBUG
                bool bRet;
                char filePath[70];

                memset(filePath, 0, sizeof(filePath));
                snprintf(filePath, sizeof(filePath), "/data/camera/Fusion%d_%d.yuv", framecount, i);

                bRet = dumpToFile((char *)filePath, srcBuffer[i].addr[0], srcBuffer[i].size[0]);
                if (bRet != true)
                    CLOGE("couldn't make a nv21 file");
#endif
#endif
            } else {
                uni_plugin_set(m_getPreviewHandle(), DUAL_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &pluginData);
            }
        }

        if (isCapture) {
#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
            m_emulationProcessTimer.start();
            ret = uni_plugin_process(m_getCaptureHandle());
            m_emulationProcessTimer.stop();
            m_emulationProcessTime = (int)m_emulationProcessTimer.durationUsecs();
            CLOGD("[Fusion] ProcessTime = %d", m_emulationProcessTime);
#endif
        }
        else {
            ret = uni_plugin_process(m_getPreviewHandle());
            uni_plugin_get(m_getPreviewHandle(), DUAL_PREVIEW_PLUGIN_NAME,
                UNI_PLUGIN_INDEX_DUAL_CAMERA_INFO, (UniPluginDualCameraParam_t*)m_cameraParam);
        }

        if (ret < 0) {
            CLOGE2("[Fusion] plugin_process failed!!");
        }
#else
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

        ret = getV4l2FormatInfo(srcRect[i].colorFormat, &bpp, &planeCount);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]): getV4l2FormatInfo (srcRect[%d].colorFormat(%x)) fail", __FUNCTION__, __LINE__, i, srcRect[i].colorFormat);
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

        EXYNOS_CAMERA_FUSION_WRAPPER_DEBUG_LOG(
            "DEBUG(%s[%d]):fusion emul running, memcpy(%d,%d,%d) from(%d,%d) to(%d,%d), ProcessTime(%d) Ratio(%f)",
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
#endif /* SAMSUNG_DUAL_ZOOM_PREVIEW */
    } else if (m_getScenario() == SCENARIO_DUAL_REAR_PORTRAIT) {
#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
        unsigned int bpp = 0;
        unsigned int planeCount  = 1;

        UniPluginBufferData_t pluginData;
        for (int i = OUTPUT_NODE_1; i <= OUTPUT_NODE_2; i++) {
#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
            if (isCapture && m_getFrameType() == FRAME_TYPE_REPROCESSING_DUAL_MASTER
                && i == OUTPUT_NODE_2 && multiShotCount > 1) {
                CLOGD2("[BokehCapture] Not need to set wideBuffer for LiveFocusLLS, multiShotCount(%d)", multiShotCount);
                continue;
            }
#endif
            if (i == OUTPUT_NODE_2 &&
                (m_getFrameType() == FRAME_TYPE_PREVIEW || m_getFrameType() == FRAME_TYPE_PREVIEW_SLAVE)) {
                break;
            }

            switch (m_getFrameType()) {
            case FRAME_TYPE_PREVIEW:
                pluginData.cameraType = UNI_PLUGIN_CAMERA_TYPE_TELE;
                break;
            case FRAME_TYPE_PREVIEW_DUAL_MASTER:
            case FRAME_TYPE_REPROCESSING_DUAL_MASTER:
                if (i == OUTPUT_NODE_1)
                    pluginData.cameraType = UNI_PLUGIN_CAMERA_TYPE_TELE;
                else
                    pluginData.cameraType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
                break;
            case FRAME_TYPE_PREVIEW_SLAVE:
                pluginData.cameraType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
                break;
            };

            pluginData.inWidth = srcRect[i].fullW;
            pluginData.inHeight = srcRect[i].fullH;
            pluginData.outWidth = dstRect.fullW;
            pluginData.outHeight = dstRect.fullH;
            pluginData.stride = srcRect[i].fullW;

            pluginData.inBuffY = srcBuffer[i].addr[0];
            pluginData.inBuffFd[UNI_PLUGIN_BUFFER_PLANE_Y] = srcBuffer[i].fd[UNI_PLUGIN_BUFFER_PLANE_Y];
            pluginData.outBuffY = dstBuffer.addr[0];
            pluginData.outBuffFd[UNI_PLUGIN_BUFFER_PLANE_Y] = dstBuffer.fd[UNI_PLUGIN_BUFFER_PLANE_Y];

            ret = getV4l2FormatInfo(srcRect[i].colorFormat, &bpp, &planeCount);
            if (ret < 0) {
                CLOGE2("[Bokeh] getV4l2FormatInfo (srcRect[%d].colorFormat(%x)) fail",
                            i, srcRect[i].colorFormat);
                /* HACK */
                bpp = 12;
                planeCount = 1;
                srcRect[i].colorFormat = V4L2_PIX_FMT_NV21M;
            }

            pluginData.inFormat = (UNI_PLUGIN_COLORFORMAT)srcRect[i].colorFormat;
            pluginData.outFormat = (UNI_PLUGIN_COLORFORMAT)srcRect[i].colorFormat;

            switch (planeCount) {
            case 1:
                pluginData.inBuffU = srcBuffer[i].addr[0] + srcRect[i].fullW * srcRect[i].fullH;
                pluginData.outBuffU = dstBuffer.addr[0] + dstRect.fullW * dstRect.fullH;
                break;
            case 2:
                pluginData.inBuffU = srcBuffer[i].addr[1];
                pluginData.inBuffFd[UNI_PLUGIN_BUFFER_PLANE_U] = srcBuffer[i].fd[UNI_PLUGIN_BUFFER_PLANE_U];
                pluginData.inBuffFd[UNI_PLUGIN_BUFFER_PLANE_V] = 0;
                pluginData.outBuffU = dstBuffer.addr[1];
                pluginData.outBuffFd[UNI_PLUGIN_BUFFER_PLANE_U] = dstBuffer.fd[UNI_PLUGIN_BUFFER_PLANE_U];
                pluginData.outBuffFd[UNI_PLUGIN_BUFFER_PLANE_V] = 0;
                break;
            default:
                android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):Invalid planeCount(%d), assert!!!!",
                        __FUNCTION__, __LINE__, planeCount);
                break;
            }

            pluginData.timestamp = (unsigned long long)shot_ext[i].shot.udm.sensor.timeStampBoot;

            if (isCapture) {
                uni_plugin_set(m_getCaptureHandle(), LIVE_FOCUS_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &pluginData);
            } else {
                uni_plugin_set(m_getPreviewHandle(), LIVE_FOCUS_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &pluginData);
            }
        }

#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
        if (isCapture && LDCaptureTotalCount != 0 && multiShotCount == LDCaptureTotalCount) {
            // set Wide InputBuffer(srcBuffer[1]) to Tele LLS Output BufferAddr from LiveFocusLib
            UniPluginBufferData_t pluginData;
            pluginData.cameraType = UNI_PLUGIN_CAMERA_TYPE_TELE;

            pluginData.outWidth = srcRect[1].fullW;
            pluginData.outHeight = srcRect[1].fullH;
            pluginData.stride = srcRect[1].fullW;

            pluginData.outBuffY = srcBuffer[1].addr[0];
            pluginData.outBuffFd[UNI_PLUGIN_BUFFER_PLANE_Y] = srcBuffer[1].fd[UNI_PLUGIN_BUFFER_PLANE_Y];

            ret = getV4l2FormatInfo(srcRect[1].colorFormat, &bpp, &planeCount);
            if (ret < 0) {
                CLOGE2("[Bokeh] getV4l2FormatInfo (srcRect[%d].colorFormat(%x)) fail",
                            1, srcRect[1].colorFormat);
                /* HACK */
                bpp = 12;
                planeCount = 1;
                srcRect[1].colorFormat = V4L2_PIX_FMT_NV21M;
            }

            pluginData.outFormat = (UNI_PLUGIN_COLORFORMAT)srcRect[1].colorFormat;

            switch (planeCount) {
            case 1:
                pluginData.outBuffU = srcBuffer[1].addr[0] + srcRect[1].fullW * srcRect[1].fullH;
                break;
            case 2:
                pluginData.outBuffU = srcBuffer[1].addr[1];
                pluginData.outBuffFd[UNI_PLUGIN_BUFFER_PLANE_U] = srcBuffer[1].fd[UNI_PLUGIN_BUFFER_PLANE_U];
                pluginData.outBuffFd[UNI_PLUGIN_BUFFER_PLANE_V] = 0;
                break;
            default:
                android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):Invalid planeCount(%d), assert!!!!",
                        __FUNCTION__, __LINE__, planeCount);
                break;
            }

            CLOGD2("[BokehCapture] set Wide InputBuffer(srcBuffer[1]:%p, %d) to Tele LLS Output BufferAddr from LiveFocusLib",
                srcBuffer[1].addr[0], planeCount);

            uni_plugin_set(m_getCaptureHandle(), LIVE_FOCUS_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &pluginData);
        }
#endif

#ifdef SAMSUNG_DUAL_SOLUTION_DEBUG
        framecount = shot_ext[OUTPUT_NODE_1].shot.dm.request.frameCount;
#endif

        if (isCapture) {
            int bokehResult = 110;
#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
            CLOGD2("[BokehCapture] cameraId(%d), multiShotCount(%d) LDCaptureTotalCount(%d)",
                                cameraId, multiShotCount, LDCaptureTotalCount);

            if (LDCaptureTotalCount == 0 || (multiShotCount == LDCaptureTotalCount))
#endif
            {
                m_setBokehCaptureProcessing(1);
                m_emulationProcessTimer.start();
                ret = uni_plugin_process(m_getCaptureHandle());
                m_emulationProcessTimer.stop();
                m_emulationProcessTime = (int)m_emulationProcessTimer.durationUsecs();
                m_setBokehCaptureProcessing(0);

                uni_plugin_get(m_getCaptureHandle(), LIVE_FOCUS_CAPTURE_PLUGIN_NAME,
                    UNI_PLUGIN_INDEX_DUAL_RESULT_VALUE, (int*)&bokehResult);
                m_setBokehCaptureResultValue(bokehResult);

                CLOGD("[BokehCapture] ProcessTime = %d Resutl = %d", m_emulationProcessTime, bokehResult);
            }
        } else {
            int bokehResult = 110;
            m_emulationProcessTimer.start();
            ret = uni_plugin_process(m_getPreviewHandle());
            m_emulationProcessTimer.stop();
            m_emulationProcessTime = (int)m_emulationProcessTimer.durationUsecs();

            uni_plugin_get(m_getPreviewHandle(), LIVE_FOCUS_PREVIEW_PLUGIN_NAME,
                UNI_PLUGIN_INDEX_DUAL_RESULT_VALUE, (int*)&bokehResult);
            m_setBokehPreviewResultValue(bokehResult);

            m_setBokehPreviewProcessResult(ret);
            if (m_emulationProcessTime > 42000) {
                CLOGD("[BokehPreview] ProcessTime = %d Resutl = %d", m_emulationProcessTime, bokehResult);
            }
        }

        if (ret < 0) {
            CLOGE2("[Bokeh] plugin_process failed!!");
        }
#else
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

        ret = getV4l2FormatInfo(srcRect[i].colorFormat, &bpp, &planeCount);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]): getV4l2FormatInfo (srcRect[%d].colorFormat(%x)) fail",
                __FUNCTION__, __LINE__, i, srcRect[i].colorFormat);
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

        EXYNOS_CAMERA_FUSION_WRAPPER_DEBUG_LOG(
            "DEBUG(%s[%d]):fusion emul running, memcpy(%d,%d,%d) from(%d,%d) to(%d,%d), ProcessTime(%d) Ratio(%f)",
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
#endif /* SAMSUNG_DUAL_PORTRAIT_SOLUTION */
    }
#else
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

        ret = getV4l2FormatInfo(srcRect[i].colorFormat, &bpp, &planeCount);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]): getV4l2FormatInfo (srcRect[%d].colorFormat(%x)) fail", __FUNCTION__, __LINE__, i, srcRect[i].colorFormat);
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
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    m_wideZoomMargin = 0;
    m_teleZoomMargin = 0;
#endif
}

#if defined(SAMSUNG_DUAL_ZOOM_PREVIEW) || defined(SAMSUNG_DUAL_PORTRAIT_SOLUTION)
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
void ExynosCameraFusionWrapper::m_setFallback(void)
{
    UNI_PLUGIN_CAMERA_TYPE cameraType = UNI_PLUGIN_CAMERA_TYPE_TELE;

    /* 'TELE' means disabling force wide mode */
    if (m_fallback) {
        cameraType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
    } else {
        cameraType = UNI_PLUGIN_CAMERA_TYPE_TELE;
    }
    uni_plugin_set(m_getPreviewHandle(), DUAL_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_DUAL_FALLBACK_SELECT, &cameraType);
}

void ExynosCameraFusionWrapper::m_setFallbackMode(bool fallback)
{
    m_fallback = fallback;
}

bool ExynosCameraFusionWrapper::m_getFallbackMode(void)
{
    return m_fallback;
}

void ExynosCameraFusionWrapper::m_setCurZoomRect(int cameraId, ExynosRect zoomRect)
{
    if (cameraId == CAMERA_ID_BACK) {
        m_wideZoomRect.x = zoomRect.x;
        m_wideZoomRect.y = zoomRect.y;
        m_wideZoomRect.w = zoomRect.w;
        m_wideZoomRect.h = zoomRect.h;

        m_wideZoomRectUT.left = zoomRect.x;
        m_wideZoomRectUT.top = zoomRect.y;
        m_wideZoomRectUT.right = zoomRect.x + zoomRect.w - 1;
        m_wideZoomRectUT.bottom = zoomRect.y + zoomRect.h - 1;
    }
    else if (cameraId == CAMERA_ID_BACK_1) {
        m_teleZoomRect.x = zoomRect.x;
        m_teleZoomRect.y = zoomRect.y;
        m_teleZoomRect.w = zoomRect.w;
        m_teleZoomRect.h = zoomRect.h;

        m_teleZoomRectUT.left = zoomRect.x;
        m_teleZoomRectUT.top = zoomRect.y;
        m_teleZoomRectUT.right = zoomRect.x + zoomRect.w - 1;
        m_teleZoomRectUT.bottom = zoomRect.y + zoomRect.h - 1;
    }
}

void ExynosCameraFusionWrapper::m_setCurViewRect(int cameraId, ExynosRect viewRect)
{
    if (cameraId == CAMERA_ID_BACK) {
        m_wideViewRect.x = viewRect.x;
        m_wideViewRect.y = viewRect.y;
        m_wideViewRect.w = viewRect.w;
        m_wideViewRect.h = viewRect.h;

        m_wideViewRectUT.left = viewRect.x;
        m_wideViewRectUT.top = viewRect.y;
        m_wideViewRectUT.right = viewRect.x + viewRect.w - 1;
        m_wideViewRectUT.bottom = viewRect.y + viewRect.h - 1;
    }
    else if (cameraId == CAMERA_ID_BACK_1) {
        m_teleViewRect.x = viewRect.x;
        m_teleViewRect.y = viewRect.y;
        m_teleViewRect.w = viewRect.w;
        m_teleViewRect.h = viewRect.h;

        m_teleViewRectUT.left = viewRect.x;
        m_teleViewRectUT.top = viewRect.y;
        m_teleViewRectUT.right = viewRect.x + viewRect.w - 1;
        m_teleViewRectUT.bottom = viewRect.y + viewRect.h - 1;

    }
}

void ExynosCameraFusionWrapper::m_setCurZoomMargin(int cameraId, int zoomMargin)
{
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    if (cameraId == CAMERA_ID_BACK) {
        m_wideZoomMargin = zoomMargin;
    } else {
        m_teleZoomMargin = zoomMargin;
    }
#endif
}

int ExynosCameraFusionWrapper::m_getCurZoomMargin(UNI_PLUGIN_CAMERA_TYPE cameraType)
{
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    if (cameraType == UNI_PLUGIN_CAMERA_TYPE_TELE) {
        return m_teleZoomMargin;
    } else {
        return m_wideZoomMargin;
    }
#else
    return 1;
#endif
}

void ExynosCameraFusionWrapper::m_getCurZoomRect(UNI_PLUGIN_CAMERA_TYPE cameraType, ExynosRect *zoomRect)
{
    if (cameraType == UNI_PLUGIN_CAMERA_TYPE_TELE) {
        zoomRect->x = m_teleZoomRect.x;
        zoomRect->y = m_teleZoomRect.y;
        zoomRect->w = m_teleZoomRect.w;
        zoomRect->h = m_teleZoomRect.h;
    } else {
        zoomRect->x = m_wideZoomRect.x;
        zoomRect->y = m_wideZoomRect.y;
        zoomRect->w = m_wideZoomRect.w;
        zoomRect->h = m_wideZoomRect.h;
    }
}

void ExynosCameraFusionWrapper::m_getCurViewRect(UNI_PLUGIN_CAMERA_TYPE cameraType, ExynosRect *viewZoomRect)
{
    if (cameraType == UNI_PLUGIN_CAMERA_TYPE_TELE) {
        viewZoomRect->x = m_teleViewRect.x;
        viewZoomRect->y = m_teleViewRect.y;
        viewZoomRect->w = m_teleViewRect.w;
        viewZoomRect->h = m_teleViewRect.h;
    } else {
        viewZoomRect->x = m_wideViewRect.x;
        viewZoomRect->y = m_wideViewRect.y;
        viewZoomRect->w = m_wideViewRect.w;
        viewZoomRect->h = m_wideViewRect.h;
    }
}

void ExynosCameraFusionWrapper::m_getCurZoomRectUT(UNI_PLUGIN_CAMERA_TYPE cameraType, UTrect *zoomRect)
{
    if (cameraType == UNI_PLUGIN_CAMERA_TYPE_TELE) {
        zoomRect->left = m_teleZoomRectUT.left;
        zoomRect->top = m_teleZoomRectUT.top;
        zoomRect->right = m_teleZoomRectUT.right;
        zoomRect->bottom = m_teleZoomRectUT.bottom;
    } else {
        zoomRect->left = m_wideZoomRectUT.left;
        zoomRect->top = m_wideZoomRectUT.top;
        zoomRect->right = m_wideZoomRectUT.right;
        zoomRect->bottom = m_wideZoomRectUT.bottom;
    }
}

void ExynosCameraFusionWrapper::m_getCurViewRectUT(UNI_PLUGIN_CAMERA_TYPE cameraType, UTrect *viewZoomRect)
{
    if (cameraType == UNI_PLUGIN_CAMERA_TYPE_TELE) {
        viewZoomRect->left = m_teleViewRectUT.left;
        viewZoomRect->top = m_teleViewRectUT.top;
        viewZoomRect->right= m_teleViewRectUT.right;
        viewZoomRect->bottom= m_teleViewRectUT.bottom;
    } else {
        viewZoomRect->left = m_wideViewRectUT.left;
        viewZoomRect->top = m_wideViewRectUT.top;
        viewZoomRect->right= m_wideViewRectUT.right;
        viewZoomRect->bottom= m_wideViewRectUT.bottom;
    }
}

#endif /* SAMSUNG_DUAL_ZOOM_PREVIEW */
void ExynosCameraFusionWrapper::m_getActiveZoomInfo(ExynosRect viewZoomRect,
                                                    ExynosRect *wideZoomRect,
                                                    ExynosRect *teleZoomRect,
                                                    int *wideZoomMargin,
                                                    int *teleZoomMargin
) {
    UTrect viewZoomRectUT = {0, };
    UniPluginDualRatioParam_t activeZoomRectUT = {0, };

    Mutex::Autolock lock(m_Lock);

    /* HACK : convert ExynosRect to MRECT */
    viewZoomRectUT.left = viewZoomRect.x;
    viewZoomRectUT.top = viewZoomRect.y;
    viewZoomRectUT.right = viewZoomRect.x + viewZoomRect.w - 1;
    viewZoomRectUT.bottom = viewZoomRect.y + viewZoomRect.h - 1;

    uni_plugin_set(m_getPreviewHandle(), DUAL_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_USER_VIEW_RATIO, &viewZoomRectUT);
    uni_plugin_get(m_getPreviewHandle(), DUAL_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_DUAL_ZOOM_RATIO, &activeZoomRectUT);

    /* HACK : convert MRECT to ExynosRect */
    wideZoomRect->x = activeZoomRectUT.wideImageRect.left;
    wideZoomRect->y = activeZoomRectUT.wideImageRect.top;
    wideZoomRect->w = activeZoomRectUT.wideImageRect.right - activeZoomRectUT.wideImageRect.left + 1;
    wideZoomRect->h = activeZoomRectUT.wideImageRect.bottom - activeZoomRectUT.wideImageRect.top + 1;

    teleZoomRect->x = activeZoomRectUT.teleImageRect.left;
    teleZoomRect->y = activeZoomRectUT.teleImageRect.top;
    teleZoomRect->w = activeZoomRectUT.teleImageRect.right - activeZoomRectUT.teleImageRect.left + 1;
    teleZoomRect->h = activeZoomRectUT.teleImageRect.bottom - activeZoomRectUT.teleImageRect.top + 1;

    *wideZoomMargin = activeZoomRectUT.needWideFullBuffer;
    *teleZoomMargin = activeZoomRectUT.needTeleFullBuffer;
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

#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
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
#endif

void ExynosCameraFusionWrapper::m_setFrameType(uint32_t frameType)
{
    m_frameType = frameType;
}

uint32_t ExynosCameraFusionWrapper::m_getFrameType(void)
{
    return m_frameType;
}

void ExynosCameraFusionWrapper::m_setScenario(uint32_t scenario)
{
    m_scenario = scenario;
}

uint32_t ExynosCameraFusionWrapper::m_getScenario(void)
{
    return m_scenario;
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

void ExynosCameraFusionWrapper::m_getCalBuf(unsigned char **buf)
{
    *buf = &m_calData[0];
}

void ExynosCameraFusionWrapper::m_setOrientation(int orientation)
{
    m_orientation = orientation;
}

int ExynosCameraFusionWrapper::m_getOrientation()
{
    return m_orientation;
}

void ExynosCameraFusionWrapper::m_setVRAInputSize(int width, int height)
{
    m_vraInputSizeWidth = width;
    m_vraInputSizeHeight = height;
}

void ExynosCameraFusionWrapper::m_getVRAInputSize(int *width, int *height)
{
    *width = m_vraInputSizeWidth;
    *height = m_vraInputSizeHeight;
}

#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
void ExynosCameraFusionWrapper::m_setBokehBlurStrength(int bokehBlurStrength)
{
    m_bokehBlurStrength = bokehBlurStrength;
}

int ExynosCameraFusionWrapper::m_getBokehBlurStrength(void)
{
    return m_bokehBlurStrength;
}

#ifdef SAMSUNG_DUAL_PORTRAIT_BEAUTY
void ExynosCameraFusionWrapper::m_setBeautyFaceRetouchLevel(int beautyFaceRetouchLevel)
{
    m_beautyFaceRetouchLevel = beautyFaceRetouchLevel;
}

int ExynosCameraFusionWrapper::m_getBeautyFaceRetouchLevel(void)
{
    return m_beautyFaceRetouchLevel;
}

void ExynosCameraFusionWrapper::m_setBeautyFaceSkinColorLevel(int beautyFaceSkinColorLevel)
{
    m_beautyFaceSkinColorLevel = beautyFaceSkinColorLevel;
}

int ExynosCameraFusionWrapper::m_getBeautyFaceSkinColorLevel(void)
{
    return m_beautyFaceSkinColorLevel;
}
#endif

void ExynosCameraFusionWrapper::m_setHwBcropSize(int width, int height)
{
    m_hwBcropSizeWidth = width;
    m_hwBcropSizeHeight = height;
}

void ExynosCameraFusionWrapper::m_getHwBcropSize(int *width, int *height)
{
    *width = m_hwBcropSizeWidth;
    *height = m_hwBcropSizeHeight;
}

void ExynosCameraFusionWrapper::m_setBokehPreviewResultValue(int result)
{
    m_BokehPreviewResultValue = result;
}

int ExynosCameraFusionWrapper::m_getBokehPreviewResultValue(void)
{
    return m_BokehPreviewResultValue;
}

void ExynosCameraFusionWrapper::m_setBokehCaptureResultValue(int result)
{
    m_BokehCaptureResultValue = result;
}

int ExynosCameraFusionWrapper::m_getBokehCaptureResultValue(void)
{
    return m_BokehCaptureResultValue;
}

void ExynosCameraFusionWrapper::m_setBokehPreviewProcessResult(int result)
{
    m_BokehPreviewProcessResult = result;
}

int ExynosCameraFusionWrapper::m_getBokehPreviewProcessResult(void)
{
    return m_BokehPreviewProcessResult;
}

void ExynosCameraFusionWrapper::m_setCurrentBokehPreviewResult(int result)
{
    m_CurrentBokehPreviewResult = result;
}

int ExynosCameraFusionWrapper::m_getCurrentBokehPreviewResult(void)
{
    return m_CurrentBokehPreviewResult;
}

void ExynosCameraFusionWrapper::m_setBokehCaptureProcessing(int bokehCaptureProcessing)
{
    Mutex::Autolock l(m_bokehCaptureProcessingLock);

    m_bokehCaptureProcessing = bokehCaptureProcessing;
}

int ExynosCameraFusionWrapper::m_getBokehCaptureProcessing(void)
{
    Mutex::Autolock l(m_bokehCaptureProcessingLock);

    return m_bokehCaptureProcessing;
}

#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
void ExynosCameraFusionWrapper::m_setDualCaptureFlag(int dualCaptureFlag)
{
    m_dualCaptureFlag = dualCaptureFlag;
}

int ExynosCameraFusionWrapper::m_getDualCaptureFlag(void)
{
    return m_dualCaptureFlag;
}
#endif
#endif /* SAMSUNG_DUAL_PORTRAIT_SOLUTION */

#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
void ExynosCameraFusionWrapper::m_setCropROI(UNI_PLUGIN_CAMERA_TYPE cameraType, UTrect cropROI)
{
    if (cameraType == UNI_PLUGIN_CAMERA_TYPE_WIDE) {
        m_WideCropROI.left = cropROI.left;
        m_WideCropROI.top = cropROI.top;
        m_WideCropROI.right = cropROI.right;
        m_WideCropROI.bottom = cropROI.bottom;
    } else if (cameraType == UNI_PLUGIN_CAMERA_TYPE_TELE) {
        m_TeleCropROI.left = cropROI.left;
        m_TeleCropROI.top = cropROI.top;
        m_TeleCropROI.right = cropROI.right;
        m_TeleCropROI.bottom = cropROI.bottom;
    } else {
        CLOGE2("invalid cameraType(%d)", cameraType);
    }
}

void ExynosCameraFusionWrapper::m_getCropROI(UNI_PLUGIN_CAMERA_TYPE cameraType, UTrect *cropROI)
{
    if (cameraType == UNI_PLUGIN_CAMERA_TYPE_WIDE) {
        cropROI->left = m_WideCropROI.left;
        cropROI->top = m_WideCropROI.top;
        cropROI->right = m_WideCropROI.right;
        cropROI->bottom = m_WideCropROI.bottom;
    } else if (cameraType == UNI_PLUGIN_CAMERA_TYPE_TELE) {
        cropROI->left = m_TeleCropROI.left;
        cropROI->top = m_TeleCropROI.top;
        cropROI->right = m_TeleCropROI.right;
        cropROI->bottom = m_TeleCropROI.bottom;
    } else {
        CLOGE2("invalid cameraType(%d)", cameraType);
    }
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

void ExynosCameraFusionWrapper::m_setBcropROI(int cameraId, int bCropX, int bCropY)
{
    if (cameraId == CAMERA_ID_BACK) {
        m_bCropROI.left = bCropX;
        m_bCropROI.top  = bCropY;
        uni_plugin_set(m_getCaptureHandle(), DUAL_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_CROP_INFO, &m_bCropROI);
    }
}

void ExynosCameraFusionWrapper::m_setWideFaceInfo(void *faceROIs, int faceNums)
{
    memcpy(m_WidefaceROIs, faceROIs, sizeof(ExynosRect2)*faceNums);
    m_WidefaceNums = faceNums;
}

void ExynosCameraFusionWrapper::m_setTeleFaceInfo(void *faceROIs, int faceNums)
{
    memcpy(m_TelefaceROIs, faceROIs, sizeof(ExynosRect2)*faceNums);
    m_TelefaceNums = faceNums;
}

void ExynosCameraFusionWrapper::m_resetFaceInfo(void)
{
    if (m_WidefaceNums > 0) {
        memset(m_WidefaceROIs, 0x00, sizeof(ExynosRect2) *NUM_OF_DETECTED_FACES);
        m_WidefaceNums = 0;
    }

    if (m_TelefaceNums > 0) {
        memset(m_TelefaceROIs, 0x00, sizeof(ExynosRect2) *NUM_OF_DETECTED_FACES);
        m_TelefaceNums = 0;
    }
}
#endif //SAMSUNG_DUAL_ZOOM_CAPTURE

#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
void ExynosCameraFusionWrapper::m_setCameraParam(UniPluginDualCameraParam_t* param)
{
    m_cameraParam = param;
}

void ExynosCameraFusionWrapper::m_setDualSelectedCam(int selectedCam)
{
    UniPluginDualCameraParam_t param;

    if (selectedCam != -1) {
        param.confirmSession = (UNI_PLUGIN_CAMERA_TYPE)selectedCam;
        uni_plugin_set(m_getPreviewHandle(), DUAL_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_DUAL_CAMERA_INFO, &param);
    } else {
        CLOGD2("Skip set selectedCam for first frame");
    }
}

UniPluginDualCameraParam_t* ExynosCameraFusionWrapper::m_getCameraParam(void)
{
    return (m_cameraParam);
}

int ExynosCameraFusionWrapper::m_getConvertRect2Origin(UNI_PLUGIN_CAMERA_TYPE cameraType, ExynosRect2 *srcRect, ExynosRect2 *destRect)
{
    UniPluginRectConvertInfo_t CovertInfo;
    int ret = 0;

    CovertInfo.convertInROI.left = srcRect->x1;
    CovertInfo.convertInROI.top = srcRect->y1;
    CovertInfo.convertInROI.right = srcRect->x2;
    CovertInfo.convertInROI.bottom = srcRect->y2;
    CovertInfo.cameraType = cameraType;
    CovertInfo.convertType = UNI_PLUGIN_RECT_CONVERT_SCREEN_TO_ORIGIN;

    ret = uni_plugin_get(m_getPreviewHandle(), DUAL_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_RECT_CONVERT, &CovertInfo);
    if (ret == 0) {
        destRect->x1 = CovertInfo.convertOutROI.left;
        destRect->y1 = CovertInfo.convertOutROI.top;
        destRect->x2 = CovertInfo.convertOutROI.right;
        destRect->y2 = CovertInfo.convertOutROI.bottom;
    } else {
        CLOGI2("[Fusion] m_getConvertRect2Origin src %d %d %d %d ret %d",
            srcRect->x1, srcRect->y1, srcRect->x2, srcRect->y2, ret);
    }

    return ret;
}

int ExynosCameraFusionWrapper::m_getConvertRect2Screen(UNI_PLUGIN_CAMERA_TYPE cameraType, ExynosRect2 *srcRect, ExynosRect2 *destRect, int margin)
{
    UniPluginRectConvertInfo_t CovertInfo;
    int ret = 0;

    CovertInfo.convertInROI.left = srcRect->x1;
    CovertInfo.convertInROI.top = srcRect->y1;
    CovertInfo.convertInROI.right = srcRect->x2;
    CovertInfo.convertInROI.bottom = srcRect->y2;
    CovertInfo.cameraType = cameraType;
    CovertInfo.convertType = UNI_PLUGIN_RECT_CONVERT_ORIGIN_TO_SCREEN;
    CovertInfo.needMarginBuffer = margin;

    ret = uni_plugin_get(m_getPreviewHandle(), DUAL_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_RECT_CONVERT, &CovertInfo);
    if (ret == 0) {
        destRect->x1 = CovertInfo.convertOutROI.left;
        destRect->y1 = CovertInfo.convertOutROI.top;
        destRect->x2 = CovertInfo.convertOutROI.right;
        destRect->y2 = CovertInfo.convertOutROI.bottom;
    } else {
        CLOGI2("[Fusion] m_getConvertRect2Screen src %d %d %d %d ret %d",
            srcRect->x1, srcRect->y1, srcRect->x2, srcRect->y2, ret);
    }

    return ret;
}
#endif
#endif /* defined(SAMSUNG_DUAL_ZOOM_PREVIEW) || defined(SAMSUNG_DUAL_PORTRAIT_SOLUTION) */

/* for Preview fusion Zoom wrapper */
ExynosCameraFusionZoomPreviewWrapper::ExynosCameraFusionZoomPreviewWrapper()
{
    CLOGD2("new ExynosCameraFusionZoomPreviewWrapper object allocated");
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    m_previewHandle = NULL;
#endif
}

ExynosCameraFusionZoomPreviewWrapper::~ExynosCameraFusionZoomPreviewWrapper()
{
}

#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
status_t ExynosCameraFusionZoomPreviewWrapper::execute(int cameraId,
                                            struct camera2_shot_ext shot_ext[],
                                            ExynosCameraBuffer srcBuffer[],
                                            ExynosRect srcRect[],
                                            ExynosCameraBufferManager *srcBufferManager[],
                                            ExynosCameraBuffer dstBuffer,
                                            ExynosRect dstRect,
                                            ExynosCameraBufferManager *dstBufferManager
#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
                                            , int multiShotCount,
                                            int LDCaptureTotalCount
#endif
                                            )
{
    status_t ret = NO_ERROR;

    if (this->flagCreate(cameraId) == false) {
        CLOGE2("[Fusion] flagCreate(%d) == false. so fail", cameraId);
        return INVALID_OPERATION;
    }

    m_emulationProcessTimer.start();
    UniPluginDualRatioParam_t zoom;
    int isWideOpened, isTeleOpened;
    UNI_PLUGIN_CAMERA_TYPE cameraType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
    memset(&zoom, 0, sizeof(zoom));

    Mutex::Autolock lock(m_Lock);

#if 0 /* test code */
    /* just memcpy */
    for (int i = 0; i < srcBuffer[0].planeCount; i++) {
        if (srcBuffer[0].fd[i] > 0) {
            memcpy(dstBuffer.addr[i], srcBuffer[0].addr[i], srcBuffer[0].size[i]);
        }
    }
#else
    for (int i = OUTPUT_NODE_1; i <= OUTPUT_NODE_2; i++) {
        if (i == OUTPUT_NODE_2 &&
            (m_getFrameType() == FRAME_TYPE_PREVIEW || m_getFrameType() == FRAME_TYPE_PREVIEW_SLAVE))
            break;

        switch (m_getFrameType()) {
        case FRAME_TYPE_PREVIEW:
            cameraType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
            isWideOpened = 1;
            isTeleOpened = 0;
            break;
        case FRAME_TYPE_PREVIEW_DUAL_MASTER:
            if (i == OUTPUT_NODE_1)
                cameraType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
            else
                cameraType = UNI_PLUGIN_CAMERA_TYPE_TELE;

            isWideOpened = 1;
            isTeleOpened = 1;
            break;
        case FRAME_TYPE_PREVIEW_SLAVE:
            cameraType = UNI_PLUGIN_CAMERA_TYPE_TELE;
            isWideOpened = 0;
            isTeleOpened = 1;
            break;
        };

        if (i == OUTPUT_NODE_1) {
            uni_plugin_set(m_getPreviewHandle(), DUAL_PREVIEW_PLUGIN_NAME,
                            UNI_PLUGIN_INDEX_DUAL_WIDE_CAM_OPEN_FLAG, &isWideOpened);
            uni_plugin_set(m_getPreviewHandle(), DUAL_PREVIEW_PLUGIN_NAME,
                            UNI_PLUGIN_INDEX_DUAL_TELE_CAM_OPEN_FLAG, &isTeleOpened);
        }

        UniPluginExtraBufferInfo_t extraBufferInfo;
        extraBufferInfo.cameraType = cameraType;
        extraBufferInfo.exposureValue.den = 256;
        extraBufferInfo.exposureValue.snum = (int32_t)shot_ext[i].shot.udm.ae.vendorSpecific[4];
        extraBufferInfo.exposureTime.num = (int32_t)shot_ext[i].shot.udm.ae.vendorSpecific[64];
        extraBufferInfo.iso[0] = (int32_t)shot_ext[i].shot.udm.ae.vendorSpecific[63];
        extraBufferInfo.orientation = (UNI_PLUGIN_DEVICE_ORIENTATION)m_getOrientation();
        extraBufferInfo.objectDistanceCm = (int32_t)shot_ext[i].shot.dm.aa.vendor_objectDistanceCm;
        uni_plugin_set(m_getPreviewHandle(), DUAL_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO, &extraBufferInfo);

#if 0
        int m_zoomBtnFlag = 0;
        uni_plugin_set(m_getPreviewHandle(), DUAL_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_OPERATION_MODE, &m_zoomBtnFlag);
#endif

        UniPluginFocusData_t focusData;
        focusData.cameraType = cameraType;
        ExynosCameraActivityAutofocus::AUTOFOCUS_STATE afState
                    = ExynosCameraActivityAutofocus::afState2AUTOFOCUS_STATE(shot_ext[i].shot.dm.aa.afState);
        focusData.focusState = afState;
        uni_plugin_set(m_getPreviewHandle(), DUAL_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_FOCUS_INFO, &focusData);

        if (cameraType == UNI_PLUGIN_CAMERA_TYPE_WIDE) {
            m_getCurZoomRectUT(cameraType, &(zoom.wideImageRect));
            m_getCurViewRectUT(cameraType, &(zoom.wideViewRect));
            zoom.needWideFullBuffer = m_getCurZoomMargin(cameraType);
        } else if (cameraType == UNI_PLUGIN_CAMERA_TYPE_TELE) {
            m_getCurZoomRectUT(cameraType, &(zoom.teleImageRect));
            m_getCurViewRectUT(cameraType, &(zoom.teleViewRect));
            zoom.needTeleFullBuffer = m_getCurZoomMargin(cameraType);
        }
    }

    CLOGV2("WideviewRect(%d,%d,%d,%d) WideImageRect(%d,%d,%d,%d) TeleViweRect(%d,%d,%d,%d) TeleImageRect(%d,%d,%d,%d) Margine(%d,%d)",
        zoom.wideViewRect.left,
        zoom.wideViewRect.top,
        zoom.wideViewRect.right,
        zoom.wideViewRect.bottom,
        zoom.wideImageRect.left,
        zoom.wideImageRect.top,
        zoom.wideImageRect.right,
        zoom.wideImageRect.bottom,
        zoom.teleViewRect.left,
        zoom.teleViewRect.top,
        zoom.teleViewRect.right,
        zoom.teleViewRect.bottom,
        zoom.teleImageRect.left,
        zoom.teleImageRect.top,
        zoom.teleImageRect.right,
        zoom.teleImageRect.bottom,
        zoom.needWideFullBuffer,
        zoom.needTeleFullBuffer);


    uni_plugin_set(m_getPreviewHandle(), DUAL_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_DUAL_ZOOM_RATIO, &zoom);

    ret = m_execute(cameraId,
                    srcBuffer, srcRect,
                    dstBuffer, dstRect,
                    shot_ext, false);
#endif
    m_emulationProcessTimer.stop();
    m_emulationProcessTime = (int)m_emulationProcessTimer.durationUsecs();

    if (ret != NO_ERROR) {
        CLOGE2("[Fusion] m_execute() fail");
    }

    return ret;
}

status_t ExynosCameraFusionZoomPreviewWrapper::m_initDualSolution(int cameraId,
                                                                  int maxSensorW, int maxSensorH,
                                                                  int sensorW, int sensorH,
                                                                  int previewW, int previewH)
{
    status_t ret = NO_ERROR;
    UniPluginDualImageInfo_t imageInfo;

    if (m_previewHandle == NULL) {
        ret = NO_INIT;
        goto EXIT;
    }

    if (m_previewHandle != NULL && cameraId == CAMERA_ID_BACK) {
        UniPluginDualInitParam_t initParam;
        initParam.wideFov = WIDE_CAMERA_FOV;
        initParam.teleFov = TELE_CAMERA_FOV;
        initParam.pCalData = m_calData;
        initParam.calDataLen = DUAL_CAL_DATA_SIZE;
        initParam.fnCallback = NULL;
        initParam.pUserData = NULL;
        uni_plugin_set(m_previewHandle, DUAL_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_DUAL_INIT_PARAMS, &initParam);

        ret = uni_plugin_init(m_previewHandle);
        if (ret < 0) {
            CLOGE2("[Fusion] DUAL_PREVEIEW_SOLUTION Plugin init failed!!");
            goto EXIT;
        }

        imageInfo.wideActiveRangeWidth = maxSensorW;
        imageInfo.wideActiveRangeHeight = maxSensorH;
        imageInfo.teleActiveRangeWidth = maxSensorW;
        imageInfo.teleActiveRangeHeight = maxSensorH;

        imageInfo.wideFetchWindowBasedOnCalWidth = sensorW;
        imageInfo.wideFetchWindowBasedOnCalHeight = sensorH;
        imageInfo.teleFetchWindowBasedOnCalWidth = sensorW;
        imageInfo.teleFetchWindowBasedOnCalHeight = sensorH;

        imageInfo.wideWidth = previewW;
        imageInfo.wideHeight = previewH;
       	imageInfo.teleWidth = previewW;
       	imageInfo.teleHeight = previewH;
       	imageInfo.dstWidth = previewW;
       	imageInfo.dstHeight = previewH;
        uni_plugin_set(m_previewHandle, DUAL_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_DUAL_VFE_OUTPUT_DIMENSION, &imageInfo);

        m_isInit = true;
        m_fallback = false;
        m_wideAfStatus = -1;
        m_teleAfStatus = -1;
        m_displayedCamera = -1;
    }

EXIT:
    return ret;
}

status_t ExynosCameraFusionZoomPreviewWrapper::m_deinitDualSolution(int cameraId)
{
    status_t ret = NO_ERROR;

    if (cameraId == CAMERA_ID_BACK && m_previewHandle != NULL) {
        ret = uni_plugin_deinit(m_previewHandle);
        if(ret < 0) {
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

/* for Capture fusion zoom wrapper */
ExynosCameraFusionZoomCaptureWrapper::ExynosCameraFusionZoomCaptureWrapper()
{
    CLOGD2("new ExynosCameraFusionZoomCaptureWrapper object allocated");
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    m_captureHandle = NULL;
#endif
}

ExynosCameraFusionZoomCaptureWrapper::~ExynosCameraFusionZoomCaptureWrapper()
{
}

#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
status_t ExynosCameraFusionZoomCaptureWrapper::m_getDebugInfo(int cameraId, void *data)
{
    uni_plugin_get(m_getCaptureHandle(), DUAL_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_DEBUG_INFO, data);
    return NO_ERROR;
}

status_t ExynosCameraFusionZoomCaptureWrapper::m_calRoiRect(int cameraId, ExynosRect mainRoiRect, ExynosRect subRoiRect)
{
    UniPluginBufferData_t pluginMainData, pluginSubData;
    UniPluginDualRatioParam_t zoomInfo;
    UniPluginDualCropROIInfo_t cropRoiInfo;
    UNI_PLUGIN_CAMERA_TYPE cameraType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
    UNI_PLUGIN_CAMERA_TYPE otherCameraType = UNI_PLUGIN_CAMERA_TYPE_TELE;
    UTrect viewZoomRectUT = {0, };
    UniPluginDualRatioParam_t captureActiveZoomRectUT ={0, };

    Mutex::Autolock lock(m_captureLock);

    if (cameraId == CAMERA_ID_BACK) {
        cameraType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
        otherCameraType = UNI_PLUGIN_CAMERA_TYPE_TELE;
    } else if (cameraId == CAMERA_ID_BACK_1) {
        cameraType = UNI_PLUGIN_CAMERA_TYPE_TELE;
        otherCameraType = UNI_PLUGIN_CAMERA_TYPE_TELE;
    }
    pluginMainData.cameraType = cameraType;
    pluginMainData.inWidth = mainRoiRect.w;
    pluginMainData.inHeight = mainRoiRect.h;

    m_getCurViewRectUT(cameraType, &zoomInfo.outViewRect);
    m_getCurZoomRectUT(cameraType, &zoomInfo.wideImageRect);

    if (m_getFrameType() == FRAME_TYPE_REPROCESSING_DUAL_MASTER) {
        /* default Rect about 1.0x : need to check Rect policy */
        zoomInfo.teleImageRect.left = 0;
        zoomInfo.teleImageRect.right = subRoiRect.w - 1;
        zoomInfo.teleImageRect.top = 0;
        zoomInfo.teleImageRect.bottom = subRoiRect.h - 1;
    } else {
        m_getCurZoomRectUT(cameraType, &(zoomInfo.teleImageRect));
    }

    m_getCurViewRectUT(cameraType, &viewZoomRectUT);
    uni_plugin_set(m_getCaptureHandle(), DUAL_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_USER_VIEW_RATIO, &viewZoomRectUT);
    uni_plugin_get(m_getCaptureHandle(), DUAL_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_DUAL_ZOOM_RATIO, &captureActiveZoomRectUT);

    if (m_getFrameType() == FRAME_TYPE_REPROCESSING_DUAL_MASTER) {
        pluginSubData.cameraType = otherCameraType;
        pluginSubData.inWidth = subRoiRect.w;
        pluginSubData.inHeight = subRoiRect.h;
        uni_plugin_set(m_getCaptureHandle(), DUAL_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &pluginSubData);
    }
    uni_plugin_set(m_getCaptureHandle(), DUAL_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &pluginMainData);
    uni_plugin_set(m_getCaptureHandle(), DUAL_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_DUAL_ZOOM_RATIO, &captureActiveZoomRectUT);
    uni_plugin_set(m_getCaptureHandle(), DUAL_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_DUAL_CROP_ROI, m_getCameraParam());
    uni_plugin_get(m_getCaptureHandle(), DUAL_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_DUAL_CROP_ROI, &cropRoiInfo);

    if (cameraType == UNI_PLUGIN_CAMERA_TYPE_WIDE) {
        m_setCropROI(cameraType, cropRoiInfo.outWideCropROI);
        m_setCropROIRatio(cameraType, cropRoiInfo.outWideCropROIRatio);
        if (m_getFrameType() == FRAME_TYPE_REPROCESSING_DUAL_MASTER) {
            m_setCropROI(otherCameraType, cropRoiInfo.outTeleCropROI);
            m_setCropROIRatio(otherCameraType, cropRoiInfo.outTeleCropROIRatio);
        }
    } else if (cameraType == UNI_PLUGIN_CAMERA_TYPE_TELE) {
        m_setCropROI(cameraType, cropRoiInfo.outTeleCropROI);
        m_setCropROIRatio(cameraType, cropRoiInfo.outTeleCropROIRatio);
    } else {
        CLOGE2("invalid cameraType(%d)", cameraType);
    }

    return NO_ERROR;
}
#endif //SAMSUNG_DUAL_ZOOM_CAPTURE

#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
status_t ExynosCameraFusionZoomCaptureWrapper::execute(int cameraId,
                                            struct camera2_shot_ext shot_ext[],
                                            ExynosCameraBuffer srcBuffer[],
                                            ExynosRect srcRect[],
                                            ExynosCameraBufferManager *srcBufferManager[],
                                            ExynosCameraBuffer dstBuffer,
                                            ExynosRect dstRect,
                                            ExynosCameraBufferManager *dstBufferManager
#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
                                            , int multiShotCount,
                                            int LDCaptureTotalCount
#endif
                                            )
{
    status_t ret = NO_ERROR;

    if (this->flagCreate(cameraId) == false) {
        CLOGE2("[Fusion] flagCreate(%d) == false. so fail", cameraId);
        return INVALID_OPERATION;
    }

    m_emulationProcessTimer.start();

    Mutex::Autolock lock(m_captureLock);

#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
    UniPluginDualRatioParam_t zoomInfo;
    UniPluginExtraBufferInfo_t extraBufferInfo;
    UniPluginFocusData_t focusData;
    UniPluginFaceInfo_t faceROIs[NUM_OF_DETECTED_FACES];
    UniPluginDualRatioParam_t activeZoomRectUT;
    UniPluginDualImageInfo_t imageInfo;

    memset(&zoomInfo, 0, sizeof(UniPluginDualRatioParam_t));
    memset(&extraBufferInfo, 0, sizeof(UniPluginExtraBufferInfo_t));
    memset(&focusData, 0, sizeof(UniPluginFocusData_t));
    memset(&faceROIs, 0, sizeof(UniPluginFaceInfo_t) * NUM_OF_DETECTED_FACES);

    /* get previewSize for FaceROI */
    uni_plugin_get(m_previewHandle, DUAL_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_DUAL_VFE_OUTPUT_DIMENSION, &imageInfo);
#endif

    for (int i = OUTPUT_NODE_1; i <= OUTPUT_NODE_2; i++) {
        if (i == OUTPUT_NODE_2 &&
            (m_getFrameType() == FRAME_TYPE_PREVIEW || m_getFrameType() == FRAME_TYPE_PREVIEW_SLAVE))
            break;
#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
        if (i == OUTPUT_NODE_1) {
            UTrect wideImageRect;

            extraBufferInfo.cameraType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
            focusData.cameraType = UNI_PLUGIN_CAMERA_TYPE_WIDE;

            m_getCurViewRectUT(UNI_PLUGIN_CAMERA_TYPE_WIDE, &(zoomInfo.outViewRect));

            m_getCropROI(UNI_PLUGIN_CAMERA_TYPE_WIDE, &wideImageRect);
            zoomInfo.wideImageRect.left = wideImageRect.left;
            zoomInfo.wideImageRect.top = wideImageRect.top;
            zoomInfo.wideImageRect.right = wideImageRect.right;
            zoomInfo.wideImageRect.bottom = wideImageRect.bottom;
        } else if (i == OUTPUT_NODE_2) {
            UTrect teleImageRect;

            extraBufferInfo.cameraType = UNI_PLUGIN_CAMERA_TYPE_TELE;
            focusData.cameraType = UNI_PLUGIN_CAMERA_TYPE_TELE;
            m_getCropROI(UNI_PLUGIN_CAMERA_TYPE_TELE, &teleImageRect);
            zoomInfo.teleImageRect.left = teleImageRect.left;
            zoomInfo.teleImageRect.top = teleImageRect.top;
            zoomInfo.teleImageRect.right = teleImageRect.right;
            zoomInfo.teleImageRect.bottom = teleImageRect.bottom;
        }

        //focus state & roi
        ExynosRect hwBcropSize;
        ExynosCameraActivityAutofocus::AUTOFOCUS_STATE afState =
                    ExynosCameraActivityAutofocus::afState2AUTOFOCUS_STATE(shot_ext[i].shot.dm.aa.afState);
        focusData.focusState = (afState == ExynosCameraActivityAutofocus::AUTOFOCUS_STATE_SUCCEESS) ? 1 : 0;

        m_getHwBcropSize(&(hwBcropSize.w), &(hwBcropSize.h));

        focusData.focusROI.left   = calibratePosition(hwBcropSize.w, imageInfo.dstWidth, shot_ext[i].shot.dm.aa.afRegions[0]);
        focusData.focusROI.top    = calibratePosition(hwBcropSize.h, imageInfo.dstHeight, shot_ext[i].shot.dm.aa.afRegions[1]);
        focusData.focusROI.right  = calibratePosition(hwBcropSize.w, imageInfo.dstWidth, shot_ext[i].shot.dm.aa.afRegions[2]);
        focusData.focusROI.bottom = calibratePosition(hwBcropSize.h, imageInfo.dstHeight, shot_ext[i].shot.dm.aa.afRegions[3]);

        CLOGV2("[Fusion] cameraType(%d) FocusState(%d) dm(%d %d %d %d) focusROI(%d %d %d %d)",
                focusData.cameraType,
                focusData.focusState,
                shot_ext[i].shot.dm.aa.afRegions[0],
                shot_ext[i].shot.dm.aa.afRegions[1],
                shot_ext[i].shot.dm.aa.afRegions[2],
                shot_ext[i].shot.dm.aa.afRegions[3],
                focusData.focusROI.left,
                focusData.focusROI.top,
                focusData.focusROI.right,
                focusData.focusROI.bottom);

        //ISO & object distance % EV & exposureTime & Orientation
        extraBufferInfo.iso[0] = (int32_t)shot_ext[i].shot.udm.ae.vendorSpecific[63];
        extraBufferInfo.objectDistanceCm = (int32_t)shot_ext[i].shot.dm.aa.vendor_objectDistanceCm;
        extraBufferInfo.exposureValue.den = 256;
        extraBufferInfo.exposureValue.snum = (int32_t)shot_ext[i].shot.udm.ae.vendorSpecific[4];
        extraBufferInfo.exposureTime.den = 1000;
        extraBufferInfo.exposureTime.snum = (int32_t)shot_ext[i].shot.udm.ae.vendorSpecific[64];
        extraBufferInfo.orientation = (UNI_PLUGIN_DEVICE_ORIENTATION)m_getOrientation();

        //face num & face info
        int faceNums = 0;
        ExynosRect vraInputSize = {0, 0, 0, 0};
        m_getVRAInputSize(&(vraInputSize.w), &(vraInputSize.h));

        for (int num = 0; num < CAMERA2_MAX_FACES; num++) {
            if (shot_ext[i].shot.dm.stats.faceIds[num] > 0) {
                faceROIs[num].index = num;

                faceROIs[num].faceROI.left = calibratePosition(vraInputSize.w,
                                                               imageInfo.dstWidth,
                                                               shot_ext[i].shot.dm.stats.faceRectangles[num][0]);

                faceROIs[num].faceROI.top = calibratePosition(vraInputSize.h,
                                                              imageInfo.dstHeight,
                                                              shot_ext[i].shot.dm.stats.faceRectangles[num][1]);

                faceROIs[num].faceROI.right = calibratePosition(vraInputSize.w,
                                                                imageInfo.dstWidth,
                                                                shot_ext[i].shot.dm.stats.faceRectangles[num][2]);

                faceROIs[num].faceROI.bottom = calibratePosition(vraInputSize.h,
                                                                 imageInfo.dstHeight,
                                                                 shot_ext[i].shot.dm.stats.faceRectangles[num][3]);

                faceNums++;

                CLOGV2("VRA(%dx%d), preview(%dx%d), FaceRect(%d,%d,%d,%d->%d,%d,%d,%d)",
                    vraInputSize.w, vraInputSize.h,
                    imageInfo.dstWidth, imageInfo.dstHeight,
                    shot_ext[i].shot.dm.stats.faceRectangles[num][0],
                    shot_ext[i].shot.dm.stats.faceRectangles[num][1],
                    shot_ext[i].shot.dm.stats.faceRectangles[num][2],
                    shot_ext[i].shot.dm.stats.faceRectangles[num][3],
                    faceROIs[num].faceROI.left,
                    faceROIs[num].faceROI.top,
                    faceROIs[num].faceROI.right,
                    faceROIs[num].faceROI.bottom);
            }
        }

        //There must be FACE_NUM & FACE_INFO after EXTRA_BUFFER_INFO.
        uni_plugin_set(m_getCaptureHandle(), DUAL_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO, &extraBufferInfo);
        uni_plugin_set(m_getCaptureHandle(), DUAL_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_FOCUS_INFO, &focusData);
        uni_plugin_set(m_getCaptureHandle(), DUAL_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_FACE_NUM, &faceNums);
        uni_plugin_set(m_getCaptureHandle(), DUAL_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_FACE_INFO, &faceROIs);
#endif
    }

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

status_t ExynosCameraFusionZoomCaptureWrapper::m_initDualSolution(int cameraId,
                                                                  int maxSensorW, int maxSensorH,
                                                                  int sensorW, int sensorH,
                                                                  int captureW, int captureH)
{
    status_t ret = NO_ERROR;
    UniPluginDualImageInfo_t imageInfo;

    CLOGI2("[Fusion] m_initDualSolution (cameraId(%d))", cameraId);

    if (m_captureHandle == NULL) {
        ret = NO_INIT;
        goto EXIT;
    }

    if (m_captureHandle != NULL && cameraId == CAMERA_ID_BACK) {
        UniPluginDualInitParam_t initParam;
        initParam.wideFov = WIDE_CAMERA_FOV;
        initParam.teleFov = TELE_CAMERA_FOV;
        initParam.pCalData = m_calData;
        initParam.calDataLen = DUAL_CAL_DATA_SIZE;
        initParam.fnCallback = NULL;
        initParam.pUserData = NULL;
        uni_plugin_set(m_captureHandle, DUAL_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_DUAL_INIT_PARAMS, &initParam);

        ret = uni_plugin_init(m_captureHandle);
        if (ret < 0) {
            CLOGE2("[Fusion] DUAL_CAPTURE_SOLUTION Plugin init failed!!");
            goto EXIT;
        }

        imageInfo.wideActiveRangeWidth = maxSensorW;
        imageInfo.wideActiveRangeHeight = maxSensorH;
        imageInfo.teleActiveRangeWidth = maxSensorW;
        imageInfo.teleActiveRangeHeight = maxSensorH;

        imageInfo.wideFetchWindowBasedOnCalWidth = sensorW;
        imageInfo.wideFetchWindowBasedOnCalHeight = sensorH;
        imageInfo.teleFetchWindowBasedOnCalWidth = sensorW;
        imageInfo.teleFetchWindowBasedOnCalHeight = sensorH;

        uni_plugin_set(m_captureHandle, DUAL_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_DUAL_VFE_OUTPUT_DIMENSION, &imageInfo);

        m_isInit = true;
        m_wideAfStatus = -1;
        m_teleAfStatus = -1;
        m_displayedCamera = -1;
    }

EXIT:
    return ret;
}

status_t ExynosCameraFusionZoomCaptureWrapper::m_deinitDualSolution(int cameraId)
{
    status_t ret = NO_ERROR;
    CLOGI2("[Fusion] m_deinitDualSolution (cameraId(%d))", cameraId);

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

