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

#define LOG_TAG "ExynosCameraBokehMetaDataConverter"

#include "ExynosCameraBokehMetaDataConverter.h"

//#define EXYNOS_CAMERA_BOKEH_META_DATA_CONVERTER_DEBUG

#ifdef EXYNOS_CAMERA_BOKEH_META_DATA_CONVERTER_DEBUG
#define META_CONVERTER_LOG CLOGD
#else
#define META_CONVERTER_LOG CLOGV
#endif

ExynosRect ExynosCameraBokehMetaDataConverter::translateFocusRoi(int cameraId,
                                                                  struct camera2_shot_ext *shot_ext)
{
    // hack for CLOG
    int m_cameraId = cameraId;
    const char *m_name = "";

    if (shot_ext == NULL) {
        android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):shot_ext == NULL on cameraId(%d), assert!!!!",
            __FUNCTION__, __LINE__, m_cameraId);
    }

    ExynosRect rect;

    // Focus ROI
    rect.x = shot_ext->shot.dm.aa.afRegions[0];
    rect.y = shot_ext->shot.dm.aa.afRegions[1];
    rect.w = shot_ext->shot.dm.aa.afRegions[2] - shot_ext->shot.dm.aa.afRegions[0];
    rect.h = shot_ext->shot.dm.aa.afRegions[3] - shot_ext->shot.dm.aa.afRegions[1];
    rect.fullW = rect.w;
    rect.fullH = rect.h;

    META_CONVERTER_LOG("DEBUG(%s[%d]):afRegions:(%d, %d, %d, %d)",
        __FUNCTION__, __LINE__,
        rect.x,
        rect.y,
        rect.w,
        rect.h);

    return rect;
}

bool ExynosCameraBokehMetaDataConverter::translateAfStatus(int cameraId,
                                                            struct camera2_shot_ext *shot_ext)
{
    // hack for CLOG
    int m_cameraId = cameraId;
    const char *m_name = "";

    bool ret = false;

    if (shot_ext == NULL) {
        android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):shot_ext == NULL on cameraId(%d), assert!!!!",
            __FUNCTION__, __LINE__, m_cameraId);
    }

    ExynosCameraActivityAutofocus::AUTOFOCUS_STATE afState =  ExynosCameraActivityAutofocus::afState2AUTOFOCUS_STATE(shot_ext->shot.dm.aa.afState);

    switch (afState) {
    case ExynosCameraActivityAutofocus::AUTOFOCUS_STATE_SUCCEESS:
        ret = true;
        break;
    case ExynosCameraActivityAutofocus::AUTOFOCUS_STATE_FAIL:
    case ExynosCameraActivityAutofocus::AUTOFOCUS_STATE_SCANNING:
    default:
        ret = false;
        break;
    }

    META_CONVERTER_LOG("DEBUG(%s[%d]):afState:(%d) ExynosCameraActivityAutofocus::AUTOFOCUS_STATE(%d), ret(%d)",
        __FUNCTION__, __LINE__,
        shot_ext->shot.dm.aa.afState, afState, ret);

    return ret;
}

float ExynosCameraBokehMetaDataConverter::translateAnalogGain(int cameraId,
                                                               struct camera2_shot_ext *shot_ext)
{
    // hack for CLOG
    int m_cameraId = cameraId;
    const char *m_name = "";

    if (shot_ext == NULL) {
        android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):shot_ext == NULL on cameraId(%d), assert!!!!",
            __FUNCTION__, __LINE__, m_cameraId);
    }

    // AE gain
    META_CONVERTER_LOG("DEBUG(%s[%d]):analogGain:(%d)",
        __FUNCTION__, __LINE__,
        shot_ext->shot.udm.sensor.analogGain);

    return (float)shot_ext->shot.udm.sensor.analogGain;
}

void ExynosCameraBokehMetaDataConverter::translateScalerSetting(int cameraId,
                                                                 struct camera2_shot_ext *shot_ext,
                                                                 int perFramePos,
                                                                 ExynosRect *yRect,
                                                                 ExynosRect *cbcrRect)
{
    // hack for CLOG
    int m_cameraId = cameraId;
    const char *m_name = "";

    if (shot_ext == NULL) {
        android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):shot_ext == NULL on cameraId(%d), assert!!!!",
            __FUNCTION__, __LINE__, m_cameraId);
    }

    yRect->w = shot_ext->node_group.capture[perFramePos].output.cropRegion[2] -
               shot_ext->node_group.capture[perFramePos].output.cropRegion[0];

    yRect->h = shot_ext->node_group.capture[perFramePos].output.cropRegion[3] -
               shot_ext->node_group.capture[perFramePos].output.cropRegion[1];

    yRect->fullW = yRect->w;
    yRect->fullH = yRect->h;

    cbcrRect->w = yRect->w / 2;
    cbcrRect->h = yRect->h;

    cbcrRect->fullW = cbcrRect->w;
    cbcrRect->fullH = cbcrRect->h;

    META_CONVERTER_LOG("DEBUG(%s[%d]):scalerSetting:perFramePos(%d) y(%d x %d) cbcr(%d x %d)",
        __FUNCTION__, __LINE__,
        perFramePos,
        yRect->w,
        yRect->h,
        cbcrRect->w,
        cbcrRect->h);
}

void ExynosCameraBokehMetaDataConverter::translateCropSetting(int cameraId,
                                                               struct camera2_shot_ext *shot_ext,
                                                               int perFramePos,
                                                               ExynosRect2 *yRect,
                                                               ExynosRect2 *cbcrRect)
{
    // hack for CLOG
    int m_cameraId = cameraId;
    const char *m_name = "";

    if (shot_ext == NULL) {
        android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):shot_ext == NULL on cameraId(%d), assert!!!!",
            __FUNCTION__, __LINE__, m_cameraId);
    }

    ExynosRect scalerYRect;
    ExynosRect scalerCbRect;

    translateScalerSetting(cameraId, shot_ext, perFramePos, &scalerYRect, &scalerCbRect);

    yRect->x1 = 0;
    yRect->x2 = scalerYRect.w;

    yRect->y1 = 0;
    yRect->y2 = scalerYRect.h;

    cbcrRect->x1 = 0;
    cbcrRect->x2 = scalerCbRect.w;

    cbcrRect->y1 = 0;
    cbcrRect->y2 = scalerCbRect.h;

    META_CONVERTER_LOG("DEBUG(%s[%d]):cropSetting:perFramePos(%d) y(%d<->%d x %d<->%d) cbcr(%d<->%d x %d<->%d)",
        __FUNCTION__, __LINE__,
        perFramePos,
        yRect->x1,    yRect->x2,    yRect->y1,    yRect->y2,
        cbcrRect->x1, cbcrRect->x2, cbcrRect->y1, cbcrRect->y2);
}

float ExynosCameraBokehMetaDataConverter::translateZoomRatio(int cameraId,
                                                              struct camera2_shot_ext *shot_ext)
{
    // hack for CLOG
    int m_cameraId = cameraId;
    const char *m_name = "";

    if (shot_ext == NULL) {
        android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):shot_ext == NULL on cameraId(%d), assert!!!!",
            __FUNCTION__, __LINE__, m_cameraId);
    }

    // zoomRatio
    float zoomRatio = shot_ext->shot.udm.zoomRatio;

    //getMetaCtlZoom(shot_ext, &zoomRatio);

    META_CONVERTER_LOG("DEBUG(%s[%d]):zoomRatio:(%f)",
        __FUNCTION__, __LINE__,
        zoomRatio);

    return zoomRatio;
}

void ExynosCameraBokehMetaDataConverter::translate2Parameters(int cameraId,
                                                               CameraParameters *params,
                                                               struct camera2_shot_ext *shot_ext,
                                                               ExynosRect pictureRect)
{
    // hack for CLOG
    int m_cameraId = cameraId;
    const char *m_name = "";

    status_t ret = NO_ERROR;

    if (shot_ext == NULL) {
        android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):shot_ext == NULL on cameraId(%d), assert!!!!",
            __FUNCTION__, __LINE__, m_cameraId);
    }

    ///////////////////////////////////////////////
    // Focus distances
    float nearFieldCm = 0.0f;
    float lensShiftUm = 0.0f;
    float farFieldCm = 0.0f;

#if 0
    translateFocusPos(cameraId, shot_ext, dof, &nearFieldCm, &lensShiftUm, &farFieldCm);
#endif

    CLOGD("DEBUG(%s[%d]):focus-distances: nearFiledCm(%f), lensShiftUm(%f), farFiledCm(%f)",
        __FUNCTION__, __LINE__,
        nearFieldCm,
        lensShiftUm,
        farFieldCm);

    char tempStr[EXYNOS_CAMERA_NAME_STR_SIZE];

    sprintf(tempStr, "%.1f,%.1f,%.1f", nearFieldCm, lensShiftUm, farFieldCm);
    params->set(CameraParameters::KEY_FOCUS_DISTANCES, tempStr);

    ///////////////////////////////////////////////
    // Focus ROI
    ExynosRect bayerRoiRect;
    bayerRoiRect = translateFocusRoi(cameraId, shot_ext);

    // calibrate to picture size.
    ExynosRect pictureRoiRect;
    float wRatio = (float)pictureRect.w / (float)((bayerRoiRect.x * 2) + bayerRoiRect.w);
    float hRatio = (float)pictureRect.h / (float)((bayerRoiRect.y * 2) + bayerRoiRect.h);

    pictureRoiRect.x = (int)((float)bayerRoiRect.x * wRatio);
    pictureRoiRect.y = (int)((float)bayerRoiRect.y * hRatio);
    pictureRoiRect.w = (int)((float)bayerRoiRect.w * wRatio);
    pictureRoiRect.h = (int)((float)bayerRoiRect.h * hRatio);

    params->set("roi_startx", pictureRoiRect.x);
    params->set("roi_starty", pictureRoiRect.y);
    params->set("roi_width",  pictureRoiRect.w);
    params->set("roi_height", pictureRoiRect.h);

    CLOGD("DEBUG(%s[%d]):bayerRoi(%d,%d,%d,%d)->pictureRoi(%d,%d,%d,%d) in pictureSize(%dx%d)",
        __FUNCTION__, __LINE__,
        bayerRoiRect.x, bayerRoiRect.y, bayerRoiRect.w, bayerRoiRect.h,
        pictureRoiRect.x, pictureRoiRect.y, pictureRoiRect.w, pictureRoiRect.h,
        pictureRect.w, pictureRect.h);

    ///////////////////////////////////////////////
    // AE gain
    float analogGain = 0.0f;

    analogGain = translateAnalogGain(cameraId, shot_ext);

    // min : 100
    float analogGainRatio = (float)(shot_ext->shot.udm.sensor.analogGain) / 100.0f;

    CLOGD("DEBUG(%s[%d]):ae_info_gain(analogGain):(%f), analogGainRatio(%f)",
        __FUNCTION__, __LINE__, analogGain, analogGainRatio);

    params->setFloat("ae_info_gain", analogGainRatio);
}

