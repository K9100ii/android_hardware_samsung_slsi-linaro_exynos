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

#define LOG_TAG "ExynosCameraBokehWrapper"

#include "ExynosCameraBokehWrapper.h"
#include "ExynosCameraPipe.h"


/* for Preview Bokeh wrapper */
ExynosCameraBokehPreviewWrapper::ExynosCameraBokehPreviewWrapper()
{
    CLOGD2("new ExynosCameraBokehPreviewWrapper object allocated");
#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
    m_previewHandle = NULL;
#endif
}

ExynosCameraBokehPreviewWrapper::~ExynosCameraBokehPreviewWrapper()
{
}

#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
status_t ExynosCameraBokehPreviewWrapper::execute(int cameraId,
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
        CLOGE2("[Bokeh] flagCreate(%d) == false. so fail", cameraId);
        return INVALID_OPERATION;
    }

    m_emulationProcessTimer.start();
    UNI_PLUGIN_CAMERA_TYPE cameraType = UNI_PLUGIN_CAMERA_TYPE_WIDE;

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
            cameraType = UNI_PLUGIN_CAMERA_TYPE_TELE;
            break;
        case FRAME_TYPE_PREVIEW_DUAL_MASTER:
            if (i == OUTPUT_NODE_1)
                cameraType = UNI_PLUGIN_CAMERA_TYPE_TELE;
            else
                cameraType = UNI_PLUGIN_CAMERA_TYPE_WIDE;

            break;
        case FRAME_TYPE_PREVIEW_SLAVE:
            cameraType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
            break;
        };

        if (cameraType == UNI_PLUGIN_CAMERA_TYPE_TELE) {
            int bokehCaptureProcessing = 0;
            bokehCaptureProcessing = m_getBokehCaptureProcessing();
            uni_plugin_set(m_getPreviewHandle(), LIVE_FOCUS_PREVIEW_PLUGIN_NAME,
                            UNI_PLUGIN_INDEX_CAPTURE_PROCESSING, &bokehCaptureProcessing);
        }

        CLOGV2("[Bokeh] i(%d) FocusState(%d) (%d %d %d %d)", i,
                ExynosCameraActivityAutofocus::afState2AUTOFOCUS_STATE(shot_ext[i].shot.dm.aa.afState),
                shot_ext[i].shot.dm.aa.afRegions[0],
                shot_ext[i].shot.dm.aa.afRegions[1],
                shot_ext[i].shot.dm.aa.afRegions[2],
                shot_ext[i].shot.dm.aa.afRegions[3]);
        CLOGV2("[Bokeh] i(%d) FocusState(%d) (%d %d %d %d)", i,
                ExynosCameraActivityAutofocus::afState2AUTOFOCUS_STATE(shot_ext[i].shot.dm.aa.afState),
                shot_ext[i].shot.ctl.aa.afRegions[0],
                shot_ext[i].shot.ctl.aa.afRegions[1],
                shot_ext[i].shot.ctl.aa.afRegions[2],
                shot_ext[i].shot.ctl.aa.afRegions[3]);

        UniPluginFocusData_t focusData;
        focusData.cameraType = cameraType;

        ExynosRect hwBcropSize;

        ExynosCameraActivityAutofocus::AUTOFOCUS_STATE afState =  ExynosCameraActivityAutofocus::afState2AUTOFOCUS_STATE(shot_ext[i].shot.dm.aa.afState);
        focusData.focusState = afState;
        m_getHwBcropSize(&(hwBcropSize.w), &(hwBcropSize.h));

        focusData.focusROI.left   = calibratePosition(hwBcropSize.w, srcRect[i].fullW, shot_ext[i].shot.dm.aa.afRegions[0]);
        focusData.focusROI.top    = calibratePosition(hwBcropSize.h, srcRect[i].fullH, shot_ext[i].shot.dm.aa.afRegions[1]);
        focusData.focusROI.right  = calibratePosition(hwBcropSize.w, srcRect[i].fullW, shot_ext[i].shot.dm.aa.afRegions[2]);
        focusData.focusROI.bottom = calibratePosition(hwBcropSize.h, srcRect[i].fullH, shot_ext[i].shot.dm.aa.afRegions[3]);

        if (focusData.focusROI.left == 0 && focusData.focusROI.top == 0
            && focusData.focusROI.right == 0 && focusData.focusROI.bottom == 0) {
            focusData.focusROI.left = 0;
            focusData.focusROI.top = 0;
            focusData.focusROI.right = srcRect[i].fullW - 1;
            focusData.focusROI.bottom = srcRect[i].fullH - 1;
        }

        uni_plugin_set(m_getPreviewHandle(), LIVE_FOCUS_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_FOCUS_INFO, &focusData);

        UniPluginExtraBufferInfo_t extraBufferInfo;
        extraBufferInfo.cameraType = cameraType;
        extraBufferInfo.exposureValue.den = 256;
        extraBufferInfo.exposureValue.snum = (int32_t)shot_ext[i].shot.udm.ae.vendorSpecific[4];
        extraBufferInfo.exposureTime.num = (int32_t)shot_ext[i].shot.udm.ae.vendorSpecific[64];
        extraBufferInfo.iso[0] = (int32_t)shot_ext[i].shot.udm.ae.vendorSpecific[391]; /* short ISO */
        extraBufferInfo.iso[1] = (int32_t)shot_ext[i].shot.udm.ae.vendorSpecific[393]; /* long ISO */

        int temp = shot_ext[i].shot.udm.internal.vendorSpecific[2];
        if ((int) shot_ext[i].shot.udm.ae.vendorSpecific[102] < 0)
            temp = -temp;
        extraBufferInfo.brightnessValue.snum = (int32_t) (ROUND_OFF_HALF((double)((temp * EXIF_DEF_APEX_DEN)/256.f), 0));
        if ((int) shot_ext[i].shot.udm.ae.vendorSpecific[102] < 0)
            extraBufferInfo.brightnessValue.snum = -extraBufferInfo.brightnessValue.snum;
        extraBufferInfo.brightnessValue.den = EXIF_DEF_APEX_DEN;

        switch (m_getOrientation()) {
            case 90:
                extraBufferInfo.orientation = UNI_PLUGIN_ORIENTATION_90;
                break;
            case 180:
                extraBufferInfo.orientation = UNI_PLUGIN_ORIENTATION_180;
                break;
            case 270:
                extraBufferInfo.orientation = UNI_PLUGIN_ORIENTATION_270;
                break;
            case 0:
            default:
                extraBufferInfo.orientation = UNI_PLUGIN_ORIENTATION_0;
                break;
        }
        extraBufferInfo.objectDistanceCm = (int32_t)shot_ext[i].shot.dm.aa.vendor_objectDistanceCm;
        uni_plugin_set(m_getPreviewHandle(), LIVE_FOCUS_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO, &extraBufferInfo);
    }

    int bokehBlurStrength = m_getBokehBlurStrength();
    uni_plugin_set(m_getPreviewHandle(), LIVE_FOCUS_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_DUAL_BLUR_LEVEL, &bokehBlurStrength);

#ifdef SAMSUNG_DUAL_PORTRAIT_BEAUTY
    int beautyFaceRetouchLevel = m_getBeautyFaceRetouchLevel();
    uni_plugin_set(m_getPreviewHandle(), LIVE_FOCUS_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_BEAUTY_LEVEL_SOFTEN, &beautyFaceRetouchLevel);
    
    int beautyFaceSkinColorLevel = m_getBeautyFaceSkinColorLevel();
    uni_plugin_set(m_getPreviewHandle(), LIVE_FOCUS_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_BEAUTY_LEVEL_COLOR, &beautyFaceSkinColorLevel);
#endif

    UniPluginFaceInfo_t faceInfo[NUM_OF_DETECTED_FACES];
    int faceNum = 0;
    struct camera2_dm *dm = &(shot_ext[OUTPUT_NODE_1].shot.dm);
    if (dm != NULL) {
        ExynosRect vraInputSize;
        ExynosRect2 detectedFace[NUM_OF_DETECTED_FACES];

        memset(detectedFace, 0x00, sizeof(detectedFace));
        memset(faceInfo, 0x00, sizeof(faceInfo));

        m_getVRAInputSize(&(vraInputSize.w), &(vraInputSize.h));

        if (dm->stats.faceDetectMode != FACEDETECT_MODE_OFF) {
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
                faceInfo[i].faceROI.left   = calibratePosition(vraInputSize.w, srcRect[1].fullW, detectedFace[i].x1);
                faceInfo[i].faceROI.top    = calibratePosition(vraInputSize.h, srcRect[1].fullH, detectedFace[i].y1);
                faceInfo[i].faceROI.right  = calibratePosition(vraInputSize.w, srcRect[1].fullW, detectedFace[i].x2);
                faceInfo[i].faceROI.bottom = calibratePosition(vraInputSize.h, srcRect[1].fullH, detectedFace[i].y2);
                faceNum++;
                CLOGV2("[Bokeh] face:%d(%d %d %d %d %d)", i, dm->stats.faceIds[i],
                    faceInfo[i].faceROI.left,
                    faceInfo[i].faceROI.top,
                    faceInfo[i].faceROI.right,
                    faceInfo[i].faceROI.bottom);
            }
        }

        uni_plugin_set(m_getPreviewHandle(), LIVE_FOCUS_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_FACE_NUM, &faceNum);
        if (ret < 0) {
            CLOGE2("[Bokeh] LLS Plugin set UNI_PLUGIN_INDEX_FACE_NUM failed!! ret(%d)", ret);
        }

        uni_plugin_set(m_getPreviewHandle(), LIVE_FOCUS_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_FACE_INFO, &faceInfo);
        if (ret < 0) {
            CLOGE2("[Bokeh] LLS Plugin set UNI_PLUGIN_INDEX_FACE_INFO failed!! ret(%d)", ret);
        }
    }

    ret = m_execute(cameraId,
                    srcBuffer, srcRect,
                    dstBuffer, dstRect,
                    shot_ext, false);
    m_emulationProcessTimer.stop();
    m_emulationProcessTime = (int)m_emulationProcessTimer.durationUsecs();

    if (ret != NO_ERROR) {
        CLOGE2("[Bokeh] m_execute() fail");
    }
#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
    else {
        int dualCaptureFlag = -1;
        uni_plugin_get(m_getPreviewHandle(), LIVE_FOCUS_PREVIEW_PLUGIN_NAME,
                UNI_PLUGIN_INDEX_DUAL_CAPTURE_FLAG, &dualCaptureFlag);
        m_setDualCaptureFlag(dualCaptureFlag);
        if (m_prevDualCaptureFlag != dualCaptureFlag) {
            CLOGD2("[Bokeh] dualCaptureFlag is changed(%d -> %d)",
                m_prevDualCaptureFlag, dualCaptureFlag);
            m_prevDualCaptureFlag = dualCaptureFlag;
        }
    }
#endif
#endif

    return ret;
}

status_t ExynosCameraBokehPreviewWrapper::m_initDualSolution(int cameraId,
                                                             int maxSensorW, int maxSensorH,
                                                             int sensorW, int sensorH,
                                                             int previewW, int previewH)
{
    status_t ret = NO_ERROR;

    if (m_previewHandle == NULL) {
        CLOGE2("[Bokeh] m_previewHandle is Null!!");
        ret = NO_INIT;
        goto EXIT;
    }

    CLOGD2("[Bokeh] INIT");

    if (m_previewHandle != NULL
        && (cameraId == CAMERA_ID_BACK_1 || cameraId == CAMERA_ID_FRONT_1)) {
        UniPluginDualInitParam_t initParam;
        initParam.wideFov = WIDE_CAMERA_FOV;
        initParam.teleFov = TELE_CAMERA_FOV;
        initParam.pCalData = m_calData;
        initParam.calDataLen = DUAL_CAL_DATA_SIZE;
        initParam.fnCallback = NULL;
        initParam.pUserData = NULL;
        uni_plugin_set(m_previewHandle, LIVE_FOCUS_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_DUAL_INIT_PARAMS, &initParam);

        ret = uni_plugin_init(m_previewHandle);
        if (ret < 0) {
            CLOGE2("[Bokeh] DUAL_PREVEIEW_SOLUTION Plugin init failed!!");
            goto EXIT;
        }

        m_isInit = true;
        m_wideAfStatus = -1;
        m_teleAfStatus = -1;
    }

    m_setBokehCaptureProcessing(0);

EXIT:
    return ret;
}

status_t ExynosCameraBokehPreviewWrapper::m_deinitDualSolution(int cameraId)
{
    status_t ret = NO_ERROR;
    CLOGD2("[Bokeh] m_deinitDualSolution (cameraId(%d))", cameraId);

    if (m_previewHandle != NULL
        && (cameraId == CAMERA_ID_BACK_1 || cameraId == CAMERA_ID_FRONT_1)) {
        ret = uni_plugin_deinit(m_previewHandle);
        if(ret < 0) {
            CLOGE2("[Bokeh] Plugin deinit failed!!");
            return INVALID_OPERATION;
        }

        m_isInit = false;
        m_wideAfStatus = -1;
        m_teleAfStatus = -1;
    }

    return ret;
}
#endif

/* for Capture Bokeh wrapper */
ExynosCameraBokehCaptureWrapper::ExynosCameraBokehCaptureWrapper()
{
    CLOGD2("new ExynosCameraBokehCaptureWrapper object allocated");
#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
    m_captureHandle = NULL;
#endif
}

ExynosCameraBokehCaptureWrapper::~ExynosCameraBokehCaptureWrapper()
{
}

#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
status_t ExynosCameraBokehCaptureWrapper::m_getDebugInfo(int cameraId, void *data)
{
    return uni_plugin_get(m_getCaptureHandle(), LIVE_FOCUS_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_DEBUG_INFO, data);
}

status_t ExynosCameraBokehCaptureWrapper::m_getDepthMap(int cameraId, UniPluginBufferData_t *data)
{
    return uni_plugin_get(m_getCaptureHandle(), LIVE_FOCUS_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, data);
}

status_t ExynosCameraBokehCaptureWrapper::m_getArcExtraData(int cameraId, void *data)
{
    return uni_plugin_get(m_getCaptureHandle(), LIVE_FOCUS_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_DEBUG_INFO, data);
}

status_t ExynosCameraBokehCaptureWrapper::m_getDOFSMetaData(int cameraId, void *data)
{
    return uni_plugin_get(m_getCaptureHandle(), LIVE_FOCUS_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_DUAL_SEF_DATA, data);
}

status_t ExynosCameraBokehCaptureWrapper::execute(int cameraId,
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
        CLOGE2("[BokehCapture] flagCreate(%d) == false. so fail", cameraId);
        return INVALID_OPERATION;
    }

    CLOGD2("[BokehCapture] execute() IN m_getFrameType(%d)", m_getFrameType());

    m_emulationProcessTimer.start();

#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
    CLOGD2("[BokehCapture] cameraId(%d), multiShotCount(%d) LDCaptureTotalCount(%d)",
            cameraId, multiShotCount, LDCaptureTotalCount);

    if (multiShotCount == 1 || multiShotCount == 0) 
#endif
    {
        UNI_PLUGIN_CAMERA_TYPE cameraType = UNI_PLUGIN_CAMERA_TYPE_WIDE;

        for (int i = OUTPUT_NODE_1; i <= OUTPUT_NODE_2; i++) {
            if (i == OUTPUT_NODE_2 &&
                (m_getFrameType() == FRAME_TYPE_REPROCESSING || m_getFrameType() == FRAME_TYPE_REPROCESSING_SLAVE)) {
                break;
            }

            switch (m_getFrameType()) {
            case FRAME_TYPE_REPROCESSING:
                cameraType = UNI_PLUGIN_CAMERA_TYPE_TELE;
                break;
            case FRAME_TYPE_REPROCESSING_DUAL_MASTER:
                if (i == OUTPUT_NODE_1)
                    cameraType = UNI_PLUGIN_CAMERA_TYPE_TELE;
                else
                    cameraType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
                break;
            case FRAME_TYPE_REPROCESSING_SLAVE:
                cameraType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
                break;
            };

            CLOGV2("[Bokeh] i(%d) cameraType(%d) FocusState(%d) (%d %d %d %d)", i, cameraType,
                    ExynosCameraActivityAutofocus::afState2AUTOFOCUS_STATE(shot_ext[i].shot.dm.aa.afState),
                    shot_ext[i].shot.dm.aa.afRegions[0],
                    shot_ext[i].shot.dm.aa.afRegions[1],
                    shot_ext[i].shot.dm.aa.afRegions[2],
                    shot_ext[i].shot.dm.aa.afRegions[3]);
            CLOGV2("[Bokeh] i(%d) cameraType(%d) FocusState(%d) (%d %d %d %d)", i, cameraType,
                    ExynosCameraActivityAutofocus::afState2AUTOFOCUS_STATE(shot_ext[i].shot.dm.aa.afState),
                    shot_ext[i].shot.ctl.aa.afRegions[0],
                    shot_ext[i].shot.ctl.aa.afRegions[1],
                    shot_ext[i].shot.ctl.aa.afRegions[2],
                    shot_ext[i].shot.ctl.aa.afRegions[3]);

            if (cameraType == UNI_PLUGIN_CAMERA_TYPE_TELE) {
                UniPluginFocusData_t focusData;
                focusData.cameraType = cameraType;

                ExynosRect hwBcropSize;

                ExynosCameraActivityAutofocus::AUTOFOCUS_STATE afState =  ExynosCameraActivityAutofocus::afState2AUTOFOCUS_STATE(shot_ext[i].shot.dm.aa.afState);
                focusData.focusState = afState;
                m_getHwBcropSize(&(hwBcropSize.w), &(hwBcropSize.h));

                focusData.focusROI.left   = calibratePosition(hwBcropSize.w, srcRect[i].fullW, shot_ext[i].shot.dm.aa.afRegions[0]);
                focusData.focusROI.top    = calibratePosition(hwBcropSize.h, srcRect[i].fullH, shot_ext[i].shot.dm.aa.afRegions[1]);
                focusData.focusROI.right  = calibratePosition(hwBcropSize.w, srcRect[i].fullW, shot_ext[i].shot.dm.aa.afRegions[2]);
                focusData.focusROI.bottom = calibratePosition(hwBcropSize.h, srcRect[i].fullH, shot_ext[i].shot.dm.aa.afRegions[3]);

                if (focusData.focusState == ExynosCameraActivityAutofocus::AUTOFOCUS_STATE_SUCCEESS
                    && focusData.focusROI.left == 0 && focusData.focusROI.top == 0
                    && focusData.focusROI.right == 0 && focusData.focusROI.bottom == 0) {
                    focusData.focusROI.left = 0;
                    focusData.focusROI.top = 0;
                    focusData.focusROI.right = srcRect[i].fullW - 1;
                    focusData.focusROI.bottom = srcRect[i].fullH - 1;
                }

                uni_plugin_set(m_getCaptureHandle(), LIVE_FOCUS_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_FOCUS_INFO, &focusData);
            }

            UniPluginExtraBufferInfo_t extraBufferInfo;
            extraBufferInfo.cameraType = cameraType;
            extraBufferInfo.exposureValue.den = 256;
            extraBufferInfo.exposureValue.snum = (int32_t)shot_ext[i].shot.udm.ae.vendorSpecific[4];
            extraBufferInfo.exposureTime.num = (int32_t)shot_ext[i].shot.udm.ae.vendorSpecific[64];
            extraBufferInfo.iso[0] = (int32_t)shot_ext[i].shot.udm.ae.vendorSpecific[391]; /* short ISO */
            extraBufferInfo.iso[1] = (int32_t)shot_ext[i].shot.udm.ae.vendorSpecific[393]; /* long ISO */

            int temp = shot_ext[i].shot.udm.internal.vendorSpecific[2];
            if ((int) shot_ext[i].shot.udm.ae.vendorSpecific[102] < 0)
                temp = -temp;
            extraBufferInfo.brightnessValue.snum = (int32_t) (ROUND_OFF_HALF((double)((temp * EXIF_DEF_APEX_DEN)/256.f), 0));
            if ((int) shot_ext[i].shot.udm.ae.vendorSpecific[102] < 0)
                extraBufferInfo.brightnessValue.snum = -extraBufferInfo.brightnessValue.snum;
            extraBufferInfo.brightnessValue.den = EXIF_DEF_APEX_DEN;

            switch (m_getOrientation()) {
                case 90:
                    extraBufferInfo.orientation = UNI_PLUGIN_ORIENTATION_90;
                    break;
                case 180:
                    extraBufferInfo.orientation = UNI_PLUGIN_ORIENTATION_180;
                    break;
                case 270:
                    extraBufferInfo.orientation = UNI_PLUGIN_ORIENTATION_270;
                    break;
                case 0:
                default:
                    extraBufferInfo.orientation = UNI_PLUGIN_ORIENTATION_0;
                    break;
            }
            extraBufferInfo.objectDistanceCm = (int32_t)shot_ext[i].shot.dm.aa.vendor_objectDistanceCm;
            uni_plugin_set(m_getCaptureHandle(), LIVE_FOCUS_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO, &extraBufferInfo);
        }

        int bokehBlurStrength = m_getBokehBlurStrength();
        CLOGD2("[BokehCapture] bokehBlurStrength(%d)", bokehBlurStrength);
        uni_plugin_set(m_getCaptureHandle(), LIVE_FOCUS_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_DUAL_BLUR_LEVEL, &bokehBlurStrength);

#ifdef SAMSUNG_DUAL_PORTRAIT_BEAUTY
        int beautyFaceRetouchLevel = m_getBeautyFaceRetouchLevel();
        CLOGD2("[BokehCapture] beautyFaceRetouchLevel(%d)", beautyFaceRetouchLevel);
        uni_plugin_set(m_getCaptureHandle(), LIVE_FOCUS_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_BEAUTY_LEVEL_SOFTEN, &beautyFaceRetouchLevel);

        int beautyFaceSkinColorLevel = m_getBeautyFaceSkinColorLevel();
        CLOGD2("[BokehCapture] beautyFaceSkinColorLevel(%d)", beautyFaceSkinColorLevel);
        uni_plugin_set(m_getCaptureHandle(), LIVE_FOCUS_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_BEAUTY_LEVEL_COLOR, &beautyFaceSkinColorLevel);
#endif

        UniPluginFaceInfo_t faceInfo[NUM_OF_DETECTED_FACES];
        int faceNum = 0;
        struct camera2_dm *dm = &(shot_ext[OUTPUT_NODE_1].shot.dm);
        if (dm != NULL) {
            ExynosRect vraInputSize;
            ExynosRect2 detectedFace[NUM_OF_DETECTED_FACES];

            memset(detectedFace, 0x00, sizeof(detectedFace));
            memset(faceInfo, 0x00, sizeof(faceInfo));

            m_getVRAInputSize(&(vraInputSize.w), &(vraInputSize.h));

            for (int i = 0; i < NUM_OF_DETECTED_FACES; i++) {
                if (dm->stats.faceIds[i]) {
                    detectedFace[i].x1 = dm->stats.faceRectangles[i][0];
                    detectedFace[i].y1 = dm->stats.faceRectangles[i][1];
                    detectedFace[i].x2 = dm->stats.faceRectangles[i][2];
                    detectedFace[i].y2 = dm->stats.faceRectangles[i][3];
                }
            }

            for (int i = 0; i < NUM_OF_DETECTED_FACES; i++) {
                if (dm->stats.faceIds[i]) {
                    faceInfo[i].faceROI.left   = calibratePosition(vraInputSize.w, srcRect[1].fullW, detectedFace[i].x1);
                    faceInfo[i].faceROI.top    = calibratePosition(vraInputSize.h, srcRect[1].fullH, detectedFace[i].y1);
                    faceInfo[i].faceROI.right  = calibratePosition(vraInputSize.w, srcRect[1].fullW, detectedFace[i].x2);
                    faceInfo[i].faceROI.bottom = calibratePosition(vraInputSize.h, srcRect[1].fullH, detectedFace[i].y2);
                    faceNum++;
                }
            }

            uni_plugin_set(m_getCaptureHandle(), LIVE_FOCUS_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_FACE_NUM, &faceNum);
            if (ret < 0) {
                CLOGE2("[Bokeh] LLS Plugin set UNI_PLUGIN_INDEX_FACE_NUM failed!! ret(%d)", ret);
            }

            uni_plugin_set(m_getCaptureHandle(), LIVE_FOCUS_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_FACE_INFO, &faceInfo);
            if (ret < 0) {
                CLOGE2("[Bokeh] LLS Plugin set UNI_PLUGIN_INDEX_FACE_INFO failed!! ret(%d)", ret);
            }
        }

        int currentBokehPreviewResult = m_getCurrentBokehPreviewResult();
        CLOGD2("[BokehCapture] currentBokehPreviewResult(%d)", currentBokehPreviewResult);
        uni_plugin_set(m_getCaptureHandle(), LIVE_FOCUS_CAPTURE_PLUGIN_NAME,
                        UNI_PLUGIN_INDEX_DUAL_RESULT_VALUE, &currentBokehPreviewResult);
    }

    ret = m_execute(cameraId,
                    srcBuffer, srcRect,
                    dstBuffer, dstRect,
                    shot_ext, true
#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
                    , multiShotCount, LDCaptureTotalCount
#endif
                    );
    m_emulationProcessTimer.stop();
    m_emulationProcessTime = (int)m_emulationProcessTimer.durationUsecs();

    if (ret != NO_ERROR) {
        CLOGE2("[Bokeh] m_execute() fail");
    }

    return NO_ERROR; //return ret;
}

status_t ExynosCameraBokehCaptureWrapper::m_initDualSolution(int cameraId,
                                                             int maxSensorW, int maxSensorH,
                                                             int sensorW, int sensorH,
                                                             int captureW, int captureH)
{
    status_t ret = NO_ERROR;
    CLOGI2("[BokehCapture] m_initDualSolution (cameraId(%d))", cameraId);

    if (m_captureHandle == NULL) {
        ret = NO_INIT;
        goto EXIT;
    }

    if (m_captureHandle != NULL
        && (cameraId == CAMERA_ID_BACK_1 || cameraId == CAMERA_ID_FRONT_1)) {
        UniPluginDualInitParam_t initParam;
        initParam.wideFov = WIDE_CAMERA_FOV;
        initParam.teleFov = TELE_CAMERA_FOV;
        initParam.pCalData = m_calData;
        initParam.calDataLen = DUAL_CAL_DATA_SIZE;
        initParam.fnCallback = NULL;
        initParam.pUserData = NULL;
        uni_plugin_set(m_captureHandle, LIVE_FOCUS_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_DUAL_INIT_PARAMS, &initParam);

        ret = uni_plugin_init(m_captureHandle);
        if (ret < 0) {
            CLOGE2("[BokehCapture] DUAL_CAPTURE_SOLUTION Plugin init failed!!");
            goto EXIT;
        }

        m_isInit = true;
        m_wideAfStatus = -1;
        m_teleAfStatus = -1;
    }

EXIT:
    return ret;
}

status_t ExynosCameraBokehCaptureWrapper::m_deinitDualSolution(int cameraId)
{
    status_t ret = NO_ERROR;
    CLOGI2("[BokehCapture] m_deinitDualSolution (cameraId(%d))", cameraId);

    if (m_captureHandle != NULL
        && (cameraId == CAMERA_ID_BACK_1 || cameraId == CAMERA_ID_FRONT_1)) {
        ret = uni_plugin_deinit(m_captureHandle);
        if (ret < 0) {
            CLOGE2("[BokehCapture] Plugin deinit failed!!");
            return INVALID_OPERATION;
        }

        m_isInit = false;
        m_wideAfStatus = -1;
        m_teleAfStatus = -1;
    }

    return ret;
}
#endif

