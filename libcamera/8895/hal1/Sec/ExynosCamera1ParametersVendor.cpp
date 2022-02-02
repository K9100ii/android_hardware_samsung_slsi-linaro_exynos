/*
**
** Copyright 2013, Samsung Electronics Co. LTD
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

/* #define LOG_NDEBUG 0 */
#define LOG_TAG "ExynosCamera1Parameters"
#include <cutils/log.h>

#include "ExynosCamera1Parameters.h"

namespace android {

void ExynosCamera1Parameters::vendorSpecificConstructor(int cameraId, bool flagCompanion)
{
    char propertyValue[PROPERTY_VALUE_MAX];

    mDebugInfo.num_of_appmarker = 1; /* Default : APP4 */
    mDebugInfo.idx[0][0] = APP_MARKER_4; /* matching the app marker 4 */

    /* DebugInfo2 */
    mDebugInfo2.num_of_appmarker = 1; /* Default : APP4 */
    mDebugInfo2.idx[0][0] = APP_MARKER_4; /* matching the app marker 4 */

    mDebugInfo.debugSize[APP_MARKER_4] = sizeof(struct camera2_udm);

    // Check debug_attribute_t struct in ExynosExif.h
    mDebugInfo.debugSize[APP_MARKER_4] += sizeof(struct sensor_id_exif_data);

    mDebugInfo.debugData[APP_MARKER_4] = new char[mDebugInfo.debugSize[APP_MARKER_4]];
    memset((void *)mDebugInfo.debugData[APP_MARKER_4], 0, mDebugInfo.debugSize[APP_MARKER_4]);

    /* DebugInfo2 */
    mDebugInfo2.debugSize[APP_MARKER_4] = mDebugInfo.debugSize[APP_MARKER_4];
    mDebugInfo2.debugData[APP_MARKER_4] = new char[mDebugInfo2.debugSize[APP_MARKER_4]];
    memset((void *)mDebugInfo2.debugData[APP_MARKER_4], 0, mDebugInfo2.debugSize[APP_MARKER_4]);

    // Check debug_attribute_t struct in ExynosExif.h
    mDebugInfo.debugSize[APP_MARKER_5] = 0;

    if (mDebugInfo.idx[1][0] == APP_MARKER_5 && mDebugInfo.debugSize[APP_MARKER_5] != 0) {
        mDebugInfo.num_of_appmarker++;
        mDebugInfo.debugData[APP_MARKER_5] = new char[mDebugInfo.debugSize[APP_MARKER_5]];
        memset((void *)mDebugInfo.debugData[APP_MARKER_5], 0, mDebugInfo.debugSize[APP_MARKER_5]);

        /* DebugInfo2 */
        mDebugInfo2.idx[1][0] = APP_MARKER_5;
        mDebugInfo2.num_of_appmarker++;
        mDebugInfo2.debugSize[APP_MARKER_5] = mDebugInfo.debugSize[APP_MARKER_5];
        mDebugInfo2.debugData[APP_MARKER_5] = new char[mDebugInfo2.debugSize[APP_MARKER_5]];
        memset((void *)mDebugInfo2.debugData[APP_MARKER_5], 0, mDebugInfo2.debugSize[APP_MARKER_5]);
    }

    // CAUTION!! : Initial values must be prior to setDefaultParameter() function.
    // Initial Values : START
    m_useFastenAeStable = (cameraId == CAMERA_ID_BACK) ? USE_FASTEN_AE_STABLE : USE_FASTEN_AE_STABLE_FRONT;

#ifdef USE_BINNING_MODE
    m_binningProperty = checkBinningProperty();
#endif
#ifdef FORCE_CAL_RELOAD
    m_calValid = true;
#endif
#ifdef SUPPORT_DEPTH_MAP
    m_flaguseDepthMap = false;
    m_flagDepthCallbackOnPreview = false;
    m_flagDepthCallbackOnCapture = false;
#endif
#if defined(BURST_CAPTURE) && defined(VARIABLE_BURST_FPS)
    m_burstShotDuration = NORMAL_BURST_DURATION;
#endif

#ifdef USE_ODC_CAPTURE
    m_flagODCCaptureMode = false;
    m_flagODCCaptureEnable = false;
#endif

}

void ExynosCamera1Parameters::operator=(const ExynosCamera1Parameters& obj)
{
    (*this).m_params.unflatten(obj.m_params.flatten());
    memcpy(&(*this).m_metadata, &obj.m_metadata, sizeof(struct camera2_shot_ext));

    memcpy(&(*this).m_cameraInfo, &obj.m_cameraInfo, sizeof(struct exynos_camera_info));

    memcpy((*this).m_staticInfo, obj.m_staticInfo, sizeof(*m_staticInfo));

    (*this).m_enabledMsgType =  obj.m_enabledMsgType;

    (*this).m_calculatedHorizontalViewAngle = obj.m_calculatedHorizontalViewAngle;

    (*this).m_previewRunning = obj.m_previewRunning;
    (*this).m_previewSizeChanged = obj.m_previewSizeChanged;
    (*this).m_pictureRunning = obj.m_pictureRunning;
    (*this).m_recordingRunning = obj.m_recordingRunning;
    (*this).m_flagCheckDualMode = obj.m_flagCheckDualMode;

    (*this).m_IsThumbnailCallbackOn = obj.m_IsThumbnailCallbackOn;
    (*this).m_flagRestartPreviewChecked = obj.m_flagRestartPreviewChecked;
    (*this).m_flagRestartPreview = obj.m_flagRestartPreview;
    (*this).m_fastFpsMode = obj.m_fastFpsMode;
    (*this).m_flagFirstStart = obj.m_flagFirstStart;
    (*this).m_flagMeteringRegionChanged = obj.m_flagMeteringRegionChanged;
    (*this).m_flagHWVDisMode = obj.m_flagHWVDisMode;

    (*this).m_flagVideoStabilization = obj.m_flagVideoStabilization;
    (*this).m_flag3dnrMode = obj.m_flag3dnrMode;

    (*this).m_flagCheckRecordingHint = obj.m_flagCheckRecordingHint;

    (*this).m_setfile = obj.m_setfile;
    (*this).m_yuvRange = obj.m_yuvRange;
    (*this).m_setfileReprocessing = obj.m_setfileReprocessing;
    (*this).m_yuvRangeReprocessing = obj.m_yuvRangeReprocessing;

#ifdef  USE_BINNING_MODE
    (*this).m_binningProperty = obj.m_binningProperty;
#endif

    (*this).m_useSizeTable = obj.m_useSizeTable;
    (*this).m_useFastenAeStable = obj.m_useFastenAeStable;

    (*this).m_useAdaptiveCSCRecording = obj.m_useAdaptiveCSCRecording;
    (*this).m_dvfsLock = obj.m_dvfsLock;
    (*this).m_previewBufferCount = obj.m_previewBufferCount;

    (*this).m_reallocBuffer = obj.m_reallocBuffer;

    (*this).m_exynosconfig->mode = obj.m_exynosconfig->mode;
    (*this).m_exynosconfig->current = obj.m_exynosconfig->current;

    (*this).m_setFocusmodeSetting = obj.m_setFocusmodeSetting;

    (*this).m_zoom_activated = obj.m_zoom_activated;
    (*this).m_firing_flash_marking = obj.m_firing_flash_marking;
    (*this).m_halVersion = obj.m_halVersion;

#ifdef  FORCE_CAL_RELOAD
    (*this).m_calValid = obj.m_calValid;
#endif

    (*this).m_exposureTimeCapture = obj.m_exposureTimeCapture;

    (*this).m_zoomWithScaler = obj.m_zoomWithScaler;

    (*this).m_flagMotionPhotoOn = obj.m_flagMotionPhotoOn;
}

status_t ExynosCamera1Parameters::setRestartParameters(const CameraParameters& params)
{
    status_t ret = NO_ERROR;

    /* Return OK means that the vision mode is enabled */
    if (checkVisionMode(params) != NO_ERROR) {
        CLOGE(" checkVisionMode fail");
        return BAD_VALUE;
    }

    if (getVisionMode() == true) {
        CLOGD(" Vision mode enabled");
        return NO_ERROR;
    }

    if (checkRecordingHint(params) != NO_ERROR)
        CLOGE(" checkRecordingHint fail");

    if (checkDualMode(params) != NO_ERROR)
        CLOGE(" checkDualMode fail");

    if (checkDualRecordingHint(params) != NO_ERROR)
        CLOGE(" checkDualRecordingHint fail");

    if (checkEffectHint(params) != NO_ERROR)
        CLOGE(" checkEffectHint fail");

    if (checkEffectRecordingHint(params) != NO_ERROR)
        CLOGE(" checkEffectRecordingHint fail");

    if (checkPreviewFps(params) != NO_ERROR) {
        CLOGE(" checkPreviewFps fail");
        return BAD_VALUE;
    }

    if (getRecordingRunning() == false) {
        if (checkVideoSize(params) != NO_ERROR)
            CLOGE(" checkVideoSize fail");
    }

    if (checkFastFpsMode(params) != NO_ERROR)
        CLOGE(" checkFastFpsMode fail");

    if (checkPreviewSize(params) != NO_ERROR) {
        CLOGE(" checkPreviewSize fail");
        return BAD_VALUE;
    }

    if (checkPreviewFormat(params) != NO_ERROR)
        CLOGE(" checkPreviewFormat fail");

    if (m_getRestartPreviewChecked() == true) {
        CLOGD("Need restart preview");
        m_setRestartPreview(m_flagRestartPreviewChecked);
    }

    return ret;

}

status_t ExynosCamera1Parameters::setParameters(const CameraParameters& params)
{
    status_t ret = NO_ERROR;

#ifdef TEST_GED_HIGH_SPEED_RECORDING
    int minFpsRange = 0, maxFpsRange = 0;
    int frameRate = 0;

    params.getPreviewFpsRange(&minFpsRange, &maxFpsRange);
    frameRate = params.getPreviewFrameRate();
    CLOGD("getFastFpsMode=%d, maxFpsRange=%d, frameRate=%d",
        getFastFpsMode(), maxFpsRange, frameRate);
    if (frameRate == 60) {
        setFastFpsMode(1);
    } else if (frameRate == 120) {
        setFastFpsMode(2);
    } else if (frameRate == 240) {
        setFastFpsMode(3);
    } else {
        setFastFpsMode(0);
    }

    CLOGD("getFastFpsMode=%d", getFastFpsMode());
#endif
    /* Return OK means that the vision mode is enabled */
    if (checkVisionMode(params) != NO_ERROR) {
        CLOGE(" checkVisionMode fail");
        return BAD_VALUE;
    }

    if (getVisionMode() == true) {
        CLOGD(" Vision mode enabled");
    }

    if (checkRecordingHint(params) != NO_ERROR)
        CLOGE(" checkRecordingHint fail");

    if (checkDualMode(params) != NO_ERROR)
        CLOGE(" checkDualMode fail");

    if (checkDualRecordingHint(params) != NO_ERROR)
        CLOGE(" checkDualRecordingHint fail");

    if (checkEffectHint(params) != NO_ERROR)
        CLOGE(" checkEffectHint fail");

    if (checkEffectRecordingHint(params) != NO_ERROR)
        CLOGE(" checkEffectRecordingHint fail");

    if (checkPreviewFps(params) != NO_ERROR) {
        CLOGE(" checkPreviewFps fail");
        return BAD_VALUE;
    }

    if (getRecordingRunning() == false) {
        if (checkVideoSize(params) != NO_ERROR)
            CLOGE(" checkVideoSize fail");
    }

    if (checkFastFpsMode(params) != NO_ERROR)
        CLOGE(" checkFastFpsMode fail");

    if (checkVideoStabilization(params) != NO_ERROR)
        CLOGE(" checkVideoStabilization fail");

    if (checkSWVdisMode(params) != NO_ERROR)
        CLOGE(" checkSWVdisMode fail");

    bool swVdisUIMode = false;
    m_setSWVdisUIMode(swVdisUIMode);

    if (checkVtMode(params) != NO_ERROR)
        CLOGE(" checkVtMode fail");

    if (checkPreviewSize(params) != NO_ERROR) {
        CLOGE(" checkPreviewSize fail");
        return BAD_VALUE;
    }
    if (checkHWVdisMode(params) != NO_ERROR)
        CLOGE(" checkHWVdisMode fail");

    if (check3dnrMode(params) != NO_ERROR)
        CLOGE(" check3dnrMode fail");

    if (checkDrcMode(params) != NO_ERROR)
        CLOGE(" checkDrcMode fail");

    if (checkOdcMode(params) != NO_ERROR)
        CLOGE(" checkOdcMode fail");

    if (checkPreviewFormat(params) != NO_ERROR)
        CLOGE(" checkPreviewFormat fail");

    if (checkPictureSize(params) != NO_ERROR) {
        CLOGE(" checkPictureSize fail");
        return BAD_VALUE;
    }

    if (checkPictureFormat(params) != NO_ERROR) {
        CLOGE(" checkPictureFormat fail");
        return BAD_VALUE;
    }

    if (checkJpegQuality(params) != NO_ERROR)
        CLOGE(" checkJpegQuality fail");

    if (checkThumbnailSize(params) != NO_ERROR)
        CLOGE(" checkThumbnailSize fail");

    if (checkThumbnailQuality(params) != NO_ERROR)
        CLOGE(" checkThumbnailQuality fail");

    if (checkZoomLevel(params) != NO_ERROR) {
        CLOGE(" checkZoomLevel fail");
        return BAD_VALUE;
    }

    if (checkRotation(params) != NO_ERROR)
        CLOGE(" checkRotation fail");

    if (checkAutoExposureLock(params) != NO_ERROR)
        CLOGE(" checkAutoExposureLock fail");

    if (checkExposureCompensation(params) != NO_ERROR) {
        CLOGE(" checkExposureCompensation fail");
        return BAD_VALUE;
    }

    if (checkExposureTime(params) != NO_ERROR) {
        CLOGE(" checkExposureTime fail");
        return BAD_VALUE;
    }

    if (checkMeteringMode(params) != NO_ERROR)
        CLOGE(" checkMeteringMode fail");

    if (checkMeteringAreas(params) != NO_ERROR) {
        CLOGE(" checkMeteringAreas fail");
        return BAD_VALUE;
    }

    if (checkAntibanding(params) != NO_ERROR)
        CLOGE(" checkAntibanding fail");

    if (checkSceneMode(params) != NO_ERROR) {
        CLOGE(" checkSceneMode fail");
        return BAD_VALUE;
    }

    if (checkFocusMode(params) != NO_ERROR) {
        CLOGE(" checkFocusMode fail");
        return BAD_VALUE;
    }

    if (checkFlashMode(params) != NO_ERROR) {
        CLOGE(" checkFlashMode fail");
        return BAD_VALUE;
    }

    if (checkWhiteBalanceMode(params) != NO_ERROR) {
        CLOGE(" checkWhiteBalanceMode fail");
        return BAD_VALUE;
    }

    if (checkAutoWhiteBalanceLock(params) != NO_ERROR)
        CLOGE(" checkAutoWhiteBalanceLock fail");

    if (checkWbKelvin(params) != NO_ERROR) {
        CLOGE(" checkWbKelvin fail");
        return BAD_VALUE;
    }

    if (checkFocusAreas(params) != NO_ERROR) {
        CLOGE(" checkFocusAreas fail");
        return BAD_VALUE;
    }

    if (checkColorEffectMode(params) != NO_ERROR) {
        CLOGE(" checkColorEffectMode fail");
        return BAD_VALUE;
    }

    if (checkGpsAltitude(params) != NO_ERROR)
        CLOGE(" checkGpsAltitude fail");

    if (checkGpsLatitude(params) != NO_ERROR)
        CLOGE(" checkGpsLatitude fail");

    if (checkGpsLongitude(params) != NO_ERROR)
        CLOGE(" checkGpsLongitude fail");

    if (checkGpsProcessingMethod(params) != NO_ERROR)
        CLOGE(" checkGpsProcessingMethod fail");

    if (checkGpsTimeStamp(params) != NO_ERROR)
        CLOGE(" checkGpsTimeStamp fail");

#if 0
    if (checkCityId(params) != NO_ERROR)
        CLOGE(" checkCityId fail");

    if (checkWeatherId(params) != NO_ERROR)
        CLOGE(" checkWeatherId fail");
#endif

    if (checkBrightness(params) != NO_ERROR)
        CLOGE(" checkBrightness fail");

    if (checkSaturation(params) != NO_ERROR)
        CLOGE(" checkSaturation fail");

    if (checkSharpness(params) != NO_ERROR)
        CLOGE(" checkSharpness fail");

    if (checkHue(params) != NO_ERROR)
        CLOGE(" checkHue fail");

    if (checkIso(params) != NO_ERROR)
        CLOGE(" checkIso fail");

    if (checkContrast(params) != NO_ERROR)
        CLOGE(" checkContrast fail");

    if (checkHdrMode(params) != NO_ERROR)
        CLOGE(" checkHdrMode fail");

    if (checkWdrMode(params) != NO_ERROR)
        CLOGE(" checkWdrMode fail");

    if (checkShotMode(params) != NO_ERROR)
        CLOGE(" checkShotMode fail");

    if (checkAntiShake(params) != NO_ERROR)
        CLOGE(" checkAntiShake fail");

    if (checkGamma(params) != NO_ERROR)
        CLOGE(" checkGamma fail");

    if (checkVRMode(params) != NO_ERROR)
        CLOGE(" checkVRMode fail");

    if (checkSlowAe(params) != NO_ERROR)
        CLOGE(" checkSlowAe fail");

    if (checkScalableSensorMode(params) != NO_ERROR)
        CLOGE(" checkScalableSensorMode fail");

    if (checkImageUniqueId(params) != NO_ERROR)
        CLOGE(" checkImageUniqueId fail");

    if (checkSeriesShotMode(params) != NO_ERROR)
        CLOGE(" checkSeriesShotMode fail");

#ifdef BURST_CAPTURE
    if (checkSeriesShotFilePath(params) != NO_ERROR)
        CLOGE(" checkSeriesShotFilePath fail");
#endif

    if (m_getRestartPreviewChecked() == true) {
        CLOGD("Need restart preview");
        m_setRestartPreview(m_flagRestartPreviewChecked);
    }

    if (checkSetfileYuvRange() != NO_ERROR)
        CLOGE(" checkSetfileYuvRange fail");

    checkHorizontalViewAngle();

#ifdef SUPPORT_DEPTH_MAP
    checkUseDepthMap();
#endif

#ifdef USE_ODC_CAPTURE
    if (checkODCCaptureMode() != NO_ERROR)
        CLOGE(" checkODCCaptureMode fail");
#endif
    return ret;
}

void ExynosCamera1Parameters::setDefaultParameter(void)
{
    CLOGI("");

    status_t ret = NO_ERROR;
    CameraParameters p;
    String8 tempStr;
    char strBuf[256];

    m_cameraInfo.autoFocusMacroPosition = ExynosCameraActivityAutofocus::AUTOFOCUS_MACRO_POSITION_BASE;

    /* Preview Size */
    getMaxPreviewSize(&m_cameraInfo.previewW, &m_cameraInfo.previewH);
    m_setHwPreviewSize(m_cameraInfo.previewW, m_cameraInfo.previewH);

    tempStr.setTo("");
    if (getResolutionList(tempStr, m_staticInfo, &m_cameraInfo.previewW, &m_cameraInfo.previewH, MODE_PREVIEW, m_cameraId) != NO_ERROR) {
        CLOGE("getResolutionList(MODE_PREVIEW) fail");

        m_cameraInfo.previewW = 640;
        m_cameraInfo.previewH = 480;
        tempStr = String8::format("%dx%d", m_cameraInfo.previewW, m_cameraInfo.previewH);
    }

    p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES, tempStr.string());
    CLOGD(" Default preview size is %dx%d", m_cameraInfo.previewW, m_cameraInfo.previewH);
    p.setPreviewSize(m_cameraInfo.previewW, m_cameraInfo.previewH);

    /* Preview Format */
    tempStr.setTo("");
    tempStr = String8::format("%s,%s", CameraParameters::PIXEL_FORMAT_YUV420SP, CameraParameters::PIXEL_FORMAT_YUV420P);
    p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FORMATS, tempStr);
    p.setPreviewFormat(CameraParameters::PIXEL_FORMAT_YUV420SP);


    /* Video Size */
    getMaxVideoSize(&m_cameraInfo.maxVideoW, &m_cameraInfo.maxVideoH);

    tempStr.setTo("");
    if (getResolutionList(tempStr, m_staticInfo, &m_cameraInfo.maxVideoW, &m_cameraInfo.maxVideoH, MODE_VIDEO, m_cameraId) != NO_ERROR) {
        CLOGE("getResolutionList(MODE_VIDEO) fail");

        m_cameraInfo.videoW = 640;
        m_cameraInfo.videoH = 480;
        tempStr = String8::format("%dx%d", m_cameraInfo.maxVideoW, m_cameraInfo.maxVideoH);
    }
    else {
#ifdef USE_WQHD_RECORDING
        if (m_addHiddenResolutionList(tempStr, m_staticInfo, 2560, 1440, MODE_VIDEO, m_cameraId) != NO_ERROR) {
            CLOGW("getResolutionList(MODE_VIDEO) fail");
        }
#endif
#ifdef USE_UHD_RECORDING
        if (m_addHiddenResolutionList(tempStr, m_staticInfo, 3840, 2160, MODE_VIDEO, m_cameraId) != NO_ERROR) {
            CLOGW("getResolutionList(MODE_VIDEO) fail");
        }
#endif
    }

    CLOGD(" KEY_SUPPORTED_VIDEO_SIZES %s", tempStr.string());

    p.set(CameraParameters::KEY_SUPPORTED_VIDEO_SIZES, tempStr.string());

    CLOGD(" Max video size is %dx%d", m_cameraInfo.maxVideoW, m_cameraInfo.maxVideoH);
    CLOGD(" Default video size is %dx%d", m_cameraInfo.videoW, m_cameraInfo.videoH);
    p.setVideoSize(m_cameraInfo.videoW, m_cameraInfo.videoH);

    /* Video Format */
    if (getAdaptiveCSCRecording() == true) {
        CLOGI("video_frame_foramt == YUV420SP_NV21");
        p.set(CameraParameters::KEY_VIDEO_FRAME_FORMAT, CameraParameters::PIXEL_FORMAT_YUV420SP_NV21);
    } else {
        CLOGI("video_frame_foramt == YUV420SP");
        p.set(CameraParameters::KEY_VIDEO_FRAME_FORMAT, CameraParameters::PIXEL_FORMAT_YUV420SP);
    }

    /* Preferred preview size for Video */
    tempStr.setTo("");
    tempStr = String8::format("%dx%d", m_cameraInfo.previewW, m_cameraInfo.previewH);
    p.set(CameraParameters::KEY_PREFERRED_PREVIEW_SIZE_FOR_VIDEO, tempStr.string());

    /* Picture Size */
    getMaxPictureSize(&m_cameraInfo.pictureW, &m_cameraInfo.pictureH);

    tempStr.setTo("");
    if (getResolutionList(tempStr, m_staticInfo, &m_cameraInfo.pictureW, &m_cameraInfo.pictureH, MODE_PICTURE, m_cameraId) != NO_ERROR) {
        CLOGE("m_getResolutionList(MODE_PICTURE) fail");

        m_cameraInfo.pictureW = 640;
        m_cameraInfo.pictureH = 480;
        tempStr = String8::format("%dx%d", m_cameraInfo.pictureW, m_cameraInfo.pictureH);
    }

    p.set(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES, tempStr.string());
    CLOGD(" Default picture size is %dx%d", m_cameraInfo.pictureW, m_cameraInfo.pictureH);
    p.setPictureSize(m_cameraInfo.pictureW, m_cameraInfo.pictureH);

    tempStr = String8::format("%s,%s,%s", CameraParameters::PIXEL_FORMAT_JPEG,
                                          CameraParameters::PIXEL_FORMAT_YUV420SP,
                                          CameraParameters::PIXEL_FORMAT_YUV420SP_NV21);

    /* Picture Format */
    p.set(CameraParameters::KEY_SUPPORTED_PICTURE_FORMATS, tempStr);
    p.setPictureFormat(CameraParameters::PIXEL_FORMAT_JPEG);

    /* Jpeg Quality */
    p.set(CameraParameters::KEY_JPEG_QUALITY, "96"); /* maximum quality */

    /* Thumbnail Size */
    getMaxThumbnailSize(&m_cameraInfo.thumbnailW, &m_cameraInfo.thumbnailH);

    tempStr.setTo("");
    if (getResolutionList(tempStr, m_staticInfo, &m_cameraInfo.thumbnailW, &m_cameraInfo.thumbnailH, MODE_THUMBNAIL, m_cameraId) != NO_ERROR) {
        tempStr = String8::format("%dx%d", m_cameraInfo.thumbnailW, m_cameraInfo.thumbnailH);
    }
    p.set(CameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES, tempStr.string());
    p.set(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH,  m_cameraInfo.thumbnailW);
    p.set(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT, m_cameraInfo.thumbnailH);

    /* Thumbnail Quality */
    p.set(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY, "100");

    /* Exposure */
    p.set(CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION, getMinExposureCompensation());
    p.set(CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION, getMaxExposureCompensation());
    p.set(CameraParameters::KEY_EXPOSURE_COMPENSATION, 0);
    p.setFloat(CameraParameters::KEY_EXPOSURE_COMPENSATION_STEP, getExposureCompensationStep());

    /* Auto Exposure Lock supported */
    if (getAutoExposureLockSupported() == true)
        p.set(CameraParameters::KEY_AUTO_EXPOSURE_LOCK_SUPPORTED, "true");
    else
        p.set(CameraParameters::KEY_AUTO_EXPOSURE_LOCK_SUPPORTED, "false");

    /* Face Detection */
    p.set(CameraParameters::KEY_MAX_NUM_DETECTED_FACES_HW, getMaxNumDetectedFaces());
    p.set(CameraParameters::KEY_MAX_NUM_DETECTED_FACES_SW, 0);

    /* Video Sanptshot Supported */
    if (getVideoSnapshotSupported() == true)
        p.set(CameraParameters::KEY_VIDEO_SNAPSHOT_SUPPORTED, "true");
    else
        p.set(CameraParameters::KEY_VIDEO_SNAPSHOT_SUPPORTED, "false");

    /* Video Stabilization Supported */
    if (getVideoStabilizationSupported() == true)
        p.set(CameraParameters::KEY_VIDEO_STABILIZATION_SUPPORTED, "true");
    else
        p.set(CameraParameters::KEY_VIDEO_STABILIZATION_SUPPORTED, "false");

    /* Focus Mode */
    int focusMode = getSupportedFocusModes();
    tempStr.setTo("");
    if (focusMode & FOCUS_MODE_AUTO) {
        tempStr.append(CameraParameters::FOCUS_MODE_AUTO);
    } else if (focusMode & FOCUS_MODE_FIXED){
        tempStr.append(CameraParameters::FOCUS_MODE_FIXED);
    }
    if (focusMode & FOCUS_MODE_INFINITY) {
        tempStr.append(",");
        tempStr.append(CameraParameters::FOCUS_MODE_INFINITY);
    }
    if (focusMode & FOCUS_MODE_MACRO) {
        tempStr.append(",");
        tempStr.append(CameraParameters::FOCUS_MODE_MACRO);
    }
    if (focusMode & FOCUS_MODE_EDOF) {
        tempStr.append(",");
        tempStr.append(CameraParameters::FOCUS_MODE_EDOF);
    }
    if (focusMode & FOCUS_MODE_CONTINUOUS_VIDEO) {
        tempStr.append(",");
        tempStr.append(CameraParameters::FOCUS_MODE_CONTINUOUS_VIDEO);
    }
    if (focusMode & FOCUS_MODE_CONTINUOUS_PICTURE) {
        tempStr.append(",");
        tempStr.append(CameraParameters::FOCUS_MODE_CONTINUOUS_PICTURE);
    }
    if (focusMode & FOCUS_MODE_CONTINUOUS_PICTURE_MACRO) {
        tempStr.append(",");
        tempStr.append("continuous-picture-macro");
    }

    p.set(CameraParameters::KEY_SUPPORTED_FOCUS_MODES,
          tempStr.string());

    if (focusMode & FOCUS_MODE_AUTO)
        p.set(CameraParameters::KEY_FOCUS_MODE,
              CameraParameters::FOCUS_MODE_AUTO);
    else if (focusMode & FOCUS_MODE_CONTINUOUS_PICTURE)
        p.set(CameraParameters::KEY_FOCUS_MODE,
              CameraParameters::FOCUS_MODE_CONTINUOUS_PICTURE);
    else if (focusMode & FOCUS_MODE_CONTINUOUS_VIDEO)
        p.set(CameraParameters::KEY_FOCUS_MODE,
              CameraParameters::FOCUS_MODE_CONTINUOUS_VIDEO);
    else if (focusMode & FOCUS_MODE_FIXED)
        p.set(CameraParameters::KEY_FOCUS_MODE,
          CameraParameters::FOCUS_MODE_FIXED);
    else
        p.set(CameraParameters::KEY_FOCUS_MODE,
          CameraParameters::FOCUS_MODE_INFINITY);

/*TODO: This values will be changed */
#define BACK_CAMERA_AUTO_FOCUS_DISTANCES_STR       "0.10,1.20,Infinity"
#define FRONT_CAMERA_FOCUS_DISTANCES_STR           "0.20,0.25,Infinity"

#define BACK_CAMERA_MACRO_FOCUS_DISTANCES_STR      "0.10,0.20,Infinity"
#define BACK_CAMERA_INFINITY_FOCUS_DISTANCES_STR   "0.10,1.20,Infinity"

    /* Focus Distances */
    if (getCameraId() == CAMERA_ID_BACK)
        p.set(CameraParameters::KEY_FOCUS_DISTANCES,
              BACK_CAMERA_AUTO_FOCUS_DISTANCES_STR);
    else
        p.set(CameraParameters::KEY_FOCUS_DISTANCES,
              FRONT_CAMERA_FOCUS_DISTANCES_STR);

    p.set(CameraParameters::FOCUS_DISTANCE_INFINITY, "Infinity");

    /* Max number of Focus Areas */
    p.set(CameraParameters::KEY_MAX_NUM_FOCUS_AREAS, 0);
    if (focusMode & FOCUS_MODE_TOUCH) {
        p.set(CameraParameters::KEY_MAX_NUM_FOCUS_AREAS, 1);
        p.set(CameraParameters::KEY_FOCUS_AREAS, "(0,0,0,0,0)");
    }

    /* Flash */
    int flashMode = getSupportedFlashModes();
    tempStr.setTo("");
    if (flashMode & FLASH_MODE_OFF) {
        tempStr.append(CameraParameters::FLASH_MODE_OFF);
    }
    if (flashMode & FLASH_MODE_AUTO) {
        tempStr.append(",");
        tempStr.append(CameraParameters::FLASH_MODE_AUTO);
    }
    if (flashMode & FLASH_MODE_ON) {
        tempStr.append(",");
        tempStr.append(CameraParameters::FLASH_MODE_ON);
    }
    if (flashMode & FLASH_MODE_RED_EYE) {
        tempStr.append(",");
        tempStr.append(CameraParameters::FLASH_MODE_RED_EYE);
    }
    if (flashMode & FLASH_MODE_TORCH) {
        tempStr.append(",");
        tempStr.append(CameraParameters::FLASH_MODE_TORCH);
    }

    p.set(CameraParameters::KEY_SUPPORTED_FLASH_MODES, tempStr.string());
    p.set(CameraParameters::KEY_FLASH_MODE, CameraParameters::FLASH_MODE_OFF);

    /* shot-mode */
    p.set("shot-mode", SHOT_MODE_NORMAL);

    /* scene mode */
    int sceneMode = getSupportedSceneModes();
    tempStr.setTo("");
    if (sceneMode & SCENE_MODE_AUTO) {
        tempStr.append(CameraParameters::SCENE_MODE_AUTO);
    }
    if (sceneMode & SCENE_MODE_ACTION) {
        tempStr.append(",");
        tempStr.append(CameraParameters::SCENE_MODE_ACTION);
    }
    if (sceneMode & SCENE_MODE_PORTRAIT) {
        tempStr.append(",");
        tempStr.append(CameraParameters::SCENE_MODE_PORTRAIT);
    }
    if (sceneMode & SCENE_MODE_LANDSCAPE) {
        tempStr.append(",");
        tempStr.append(CameraParameters::SCENE_MODE_LANDSCAPE);
    }
    if (sceneMode & SCENE_MODE_NIGHT) {
        tempStr.append(",");
        tempStr.append(CameraParameters::SCENE_MODE_NIGHT);
    }
    if (sceneMode & SCENE_MODE_NIGHT_PORTRAIT) {
        tempStr.append(",");
        tempStr.append(CameraParameters::SCENE_MODE_NIGHT_PORTRAIT);
    }
    if (sceneMode & SCENE_MODE_THEATRE) {
        tempStr.append(",");
        tempStr.append(CameraParameters::SCENE_MODE_THEATRE);
    }
    if (sceneMode & SCENE_MODE_BEACH) {
        tempStr.append(",");
        tempStr.append(CameraParameters::SCENE_MODE_BEACH);
    }
    if (sceneMode & SCENE_MODE_SNOW) {
        tempStr.append(",");
        tempStr.append(CameraParameters::SCENE_MODE_SNOW);
    }
    if (sceneMode & SCENE_MODE_SUNSET) {
        tempStr.append(",");
        tempStr.append(CameraParameters::SCENE_MODE_SUNSET);
    }
    if (sceneMode & SCENE_MODE_STEADYPHOTO) {
        tempStr.append(",");
        tempStr.append(CameraParameters::SCENE_MODE_STEADYPHOTO);
    }
    if (sceneMode & SCENE_MODE_FIREWORKS) {
        tempStr.append(",");
        tempStr.append(CameraParameters::SCENE_MODE_FIREWORKS);
    }
    if (sceneMode & SCENE_MODE_SPORTS) {
        tempStr.append(",");
        tempStr.append(CameraParameters::SCENE_MODE_SPORTS);
    }
    if (sceneMode & SCENE_MODE_PARTY) {
        tempStr.append(",");
        tempStr.append(CameraParameters::SCENE_MODE_PARTY);
    }
    if (sceneMode & SCENE_MODE_CANDLELIGHT) {
        tempStr.append(",");
        tempStr.append(CameraParameters::SCENE_MODE_CANDLELIGHT);
    }

    p.set(CameraParameters::KEY_SUPPORTED_SCENE_MODES,
          tempStr.string());
    p.set(CameraParameters::KEY_SCENE_MODE,
          CameraParameters::SCENE_MODE_AUTO);

    /* effect */
    int effect = getSupportedColorEffects();
    tempStr.setTo("");
    if (effect & EFFECT_NONE) {
        tempStr.append(CameraParameters::EFFECT_NONE);
    }
    if (effect & EFFECT_MONO) {
        tempStr.append(",");
        tempStr.append(CameraParameters::EFFECT_MONO);
    }
    if (effect & EFFECT_NEGATIVE) {
        tempStr.append(",");
        tempStr.append(CameraParameters::EFFECT_NEGATIVE);
    }
    if (effect & EFFECT_SOLARIZE) {
        tempStr.append(",");
        tempStr.append(CameraParameters::EFFECT_SOLARIZE);
    }
    if (effect & EFFECT_SEPIA) {
        tempStr.append(",");
        tempStr.append(CameraParameters::EFFECT_SEPIA);
    }
    if (effect & EFFECT_POSTERIZE) {
        tempStr.append(",");
        tempStr.append(CameraParameters::EFFECT_POSTERIZE);
    }
    if (effect & EFFECT_WHITEBOARD) {
        tempStr.append(",");
        tempStr.append(CameraParameters::EFFECT_WHITEBOARD);
    }
    if (effect & EFFECT_BLACKBOARD) {
        tempStr.append(",");
        tempStr.append(CameraParameters::EFFECT_BLACKBOARD);
    }
    if (effect & EFFECT_AQUA) {
        tempStr.append(",");
        tempStr.append(CameraParameters::EFFECT_AQUA);
    }

    p.set(CameraParameters::KEY_SUPPORTED_EFFECTS, tempStr.string());
    p.set(CameraParameters::KEY_EFFECT, CameraParameters::EFFECT_NONE);

    /* white balance */
    int whiteBalance = getSupportedWhiteBalance();
    tempStr.setTo("");
    if (whiteBalance & WHITE_BALANCE_AUTO) {
        tempStr.append(CameraParameters::WHITE_BALANCE_AUTO);
    }
    if (whiteBalance & WHITE_BALANCE_INCANDESCENT) {
        tempStr.append(",");
        tempStr.append(CameraParameters::WHITE_BALANCE_INCANDESCENT);
    }
    if (whiteBalance & WHITE_BALANCE_FLUORESCENT) {
        tempStr.append(",");
        tempStr.append(CameraParameters::WHITE_BALANCE_FLUORESCENT);
    }
    if (whiteBalance & WHITE_BALANCE_WARM_FLUORESCENT) {
        tempStr.append(",");
        tempStr.append(CameraParameters::WHITE_BALANCE_WARM_FLUORESCENT);
    }
    if (whiteBalance & WHITE_BALANCE_DAYLIGHT) {
        tempStr.append(",");
        tempStr.append(CameraParameters::WHITE_BALANCE_DAYLIGHT);
    }
    if (whiteBalance & WHITE_BALANCE_CLOUDY_DAYLIGHT) {
        tempStr.append(",");
        tempStr.append(CameraParameters::WHITE_BALANCE_CLOUDY_DAYLIGHT);
    }
    if (whiteBalance & WHITE_BALANCE_TWILIGHT) {
        tempStr.append(",");
        tempStr.append(CameraParameters::WHITE_BALANCE_TWILIGHT);
    }
    if (whiteBalance & WHITE_BALANCE_SHADE) {
        tempStr.append(",");
        tempStr.append(CameraParameters::WHITE_BALANCE_SHADE);
    }

    p.set(CameraParameters::KEY_SUPPORTED_WHITE_BALANCE,
          tempStr.string());
    p.set(CameraParameters::KEY_WHITE_BALANCE, CameraParameters::WHITE_BALANCE_AUTO);

    /* Auto Whitebalance Lock supported */
    if (getAutoWhiteBalanceLockSupported() == true)
        p.set(CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK_SUPPORTED, "true");
    else
        p.set(CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK_SUPPORTED, "false");

    /*  anti banding */
    tempStr.setTo("");
    int antiBanding = getSupportedAntibanding();
    if (antiBanding & ANTIBANDING_AUTO) {
        tempStr.append(CameraParameters::ANTIBANDING_AUTO);
    }
    if (antiBanding & ANTIBANDING_50HZ) {
        tempStr.append(",");
        tempStr.append(CameraParameters::ANTIBANDING_50HZ);
    }
    if (antiBanding & ANTIBANDING_60HZ) {
        tempStr.append(",");
        tempStr.append(CameraParameters::ANTIBANDING_60HZ);
    }
    if (antiBanding & ANTIBANDING_OFF) {
        tempStr.append(",");
        tempStr.append(CameraParameters::ANTIBANDING_OFF);
    }

    p.set(CameraParameters::KEY_SUPPORTED_ANTIBANDING,
          tempStr.string());

    p.set(CameraParameters::KEY_ANTIBANDING, CameraParameters::ANTIBANDING_AUTO);

    /* rotation */
    p.set(CameraParameters::KEY_ROTATION, 0);

    /* view angle */
    setHorizontalViewAngle(640, 480);
    p.setFloat(CameraParameters::KEY_HORIZONTAL_VIEW_ANGLE, getHorizontalViewAngle());
    p.setFloat(CameraParameters::KEY_VERTICAL_VIEW_ANGLE, getVerticalViewAngle());

    /* metering */
    p.set(CameraParameters::KEY_MAX_NUM_METERING_AREAS, getMaxNumMeteringAreas());
    p.set(CameraParameters::KEY_METERING_AREAS, 0);

    /* zoom */
    if (getZoomSupported() == true) {
        int maxZoom = getMaxZoomLevel();
        if (0 < maxZoom) {
            p.set(CameraParameters::KEY_ZOOM_SUPPORTED, "true");

            if (getSmoothZoomSupported() == true)
                p.set(CameraParameters::KEY_SMOOTH_ZOOM_SUPPORTED, "true");
            else
                p.set(CameraParameters::KEY_SMOOTH_ZOOM_SUPPORTED, "false");

            p.set(CameraParameters::KEY_MAX_ZOOM, maxZoom - 1);
            p.set(CameraParameters::KEY_ZOOM, ZOOM_LEVEL_0);

            int max_zoom_ratio = (int)getMaxZoomRatio();
            tempStr.setTo("");
            if (getZoomRatioList(tempStr, maxZoom, max_zoom_ratio, m_staticInfo->zoomRatioList) == NO_ERROR)
                p.set(CameraParameters::KEY_ZOOM_RATIOS, tempStr.string());
            else
                p.set(CameraParameters::KEY_ZOOM_RATIOS, "100");

            p.set("constant-growth-rate-zoom-supported", "true");

            CLOGI("[setDefaultParameter]maxZoom=%d, max_zoom_ratio= %d, zoomRatioList=%s", maxZoom, max_zoom_ratio, tempStr.string());
        } else {
            p.set(CameraParameters::KEY_ZOOM_SUPPORTED, "false");
            p.set(CameraParameters::KEY_SMOOTH_ZOOM_SUPPORTED, "false");
            p.set(CameraParameters::KEY_MAX_ZOOM, ZOOM_LEVEL_0);
            p.set(CameraParameters::KEY_ZOOM, ZOOM_LEVEL_0);
        }
    } else {
        p.set(CameraParameters::KEY_ZOOM_SUPPORTED, "false");
        p.set(CameraParameters::KEY_SMOOTH_ZOOM_SUPPORTED, "false");
        p.set(CameraParameters::KEY_MAX_ZOOM, ZOOM_LEVEL_0);
        p.set(CameraParameters::KEY_ZOOM, ZOOM_LEVEL_0);
    }

    /* fps */
    uint32_t minFpsRange = 15;
    uint32_t maxFpsRange = 30;

    getPreviewFpsRange(&minFpsRange, &maxFpsRange);
#ifdef TEST_GED_HIGH_SPEED_RECORDING
    maxFpsRange = 120;
#endif
    CLOGI("[getPreviewFpsRange]minFpsRange=%d, maxFpsRange=%d", (int)minFpsRange, (int)maxFpsRange);
    int minFps = (minFpsRange == 0) ? 0 : (int)minFpsRange;
    int maxFps = (maxFpsRange == 0) ? 0 : (int)maxFpsRange;

    tempStr.setTo("");
    snprintf(strBuf, 256, "%d", minFps);
    tempStr.append(strBuf);

    for (int i = minFps + 1; i <= maxFps; i++) {
        snprintf(strBuf, 256, ",%d", i);
        tempStr.append(strBuf);
    }
    p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES,  tempStr.string());

    minFpsRange = minFpsRange * 1000;
    maxFpsRange = maxFpsRange * 1000;

    tempStr.setTo("");
    getSupportedFpsList(tempStr, minFpsRange, maxFpsRange, m_cameraId, m_staticInfo);
    CLOGI("[setDefaultParameter]supportedFpsList=%s", tempStr.string());
    p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE, tempStr.string());
    /* p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE, "(15000,30000),(30000,30000)"); */

    /* limit 30 fps on default setting. */
    if (30 < maxFps)
        maxFps = 30;
    p.setPreviewFrameRate(maxFps);

    if (30000 < maxFpsRange)
        maxFpsRange = 30000;
    snprintf(strBuf, 256, "%d,%d", maxFpsRange/2, maxFpsRange);
    p.set(CameraParameters::KEY_PREVIEW_FPS_RANGE, strBuf);

    /* focal length */
    int num = 0;
    int den = 0;
    int precision = 0;
    getFocalLength(&num, &den);

    switch (den) {
    default:
    case 1000:
        precision = 3;
        break;
    case 100:
        precision = 2;
        break;
    case 10:
        precision = 1;
        break;
    case 1:
        precision = 0;
        break;
    }
    snprintf(strBuf, 256, "%.*f", precision, ((float)num / (float)den));
    p.set(CameraParameters::KEY_FOCAL_LENGTH, strBuf);

    /* Additional params. */
    p.set("contrast", "auto");
    p.set("iso", "auto");

    // Set supported ISO values
    int isoValues = getSupportedISO();
    tempStr.setTo("");
    if (isoValues & ISO_AUTO) {
        tempStr.append(CameraParameters::ISO_AUTO);
    }
    if (isoValues & ISO_100) {
        tempStr.append(",");
        tempStr.append(CameraParameters::ISO_100);
    }
    if (isoValues & ISO_200) {
        tempStr.append(",");
        tempStr.append(CameraParameters::ISO_200);
    }
    if (isoValues & ISO_400) {
        tempStr.append(",");
        tempStr.append(CameraParameters::ISO_400);
    }
    if (isoValues & ISO_800) {
        tempStr.append(",");
        tempStr.append(CameraParameters::ISO_800);
    }

    p.set("iso-values",
          tempStr.string());

    p.set("wdr", 0);
    p.set("hdr-mode", 0);
    p.set("metering", "center");    /* For Samsung SDK */

    // Set Supported Metering Mode
    int meteringModes = getSupportedMeteringMode();
    tempStr.setTo("");
    if (meteringModes & METERING_MODE_MATRIX) {
        tempStr.append(CameraParameters::METERING_MATRIX);
    }
    if (meteringModes & METERING_MODE_CENTER) {
        tempStr.append(",");
        tempStr.append(CameraParameters::METERING_CENTER);
    }
    if (meteringModes & METERING_MODE_SPOT) {
        tempStr.append(",");
        tempStr.append(CameraParameters::METERING_SPOT);
    }

    p.set("brightness", 0);
    p.set("brightness-max", 2);
    p.set("brightness-min", -2);

    p.set("saturation", 0);
    p.set("saturation-max", 2);
    p.set("saturation-min", -2);

    p.set("sharpness", 0);
    p.set("sharpness-max", 2);
    p.set("sharpness-min", -2);

    p.set("hue", 0);
    p.set("hue-max", 2);
    p.set("hue-min", -2);

    /* For Series shot */
    p.set("burst-capture", 0);
    p.set("best-capture", 0);

    /* fnumber */
    getFnumber(&num, &den);
    p.set("fnumber-value-numerator", num);
    p.set("fnumber-value-denominator", den);

    /* max aperture value */
    getApertureValue(&num, &den);
    p.set("maxaperture-value-numerator", num);
    p.set("maxaperture-value-denominator", den);

    /* focal length */
    getFocalLength(&num, &den);
    p.set("focallength-value-numerator", num);
    p.set("focallength-value-denominator", den);

    /* focal length in 35mm film */
    int focalLengthIn35mmFilm = 0;
    focalLengthIn35mmFilm = getFocalLengthIn35mmFilm();
    p.set("focallength-35mm-value", focalLengthIn35mmFilm);

#if defined(TEST_APP_HIGH_SPEED_RECORDING)
    p.set("fast-fps-mode", 0);
#endif

    p.set(CameraParameters::KEY_DYNAMIC_RANGE_CONTROL, "off");

    /* PHASE_AF */
    p.set(CameraParameters::KEY_PHASE_AF, "off");

    /* RT_HDR */
    p.set(CameraParameters::KEY_RT_HDR, "off");
    p.set("imageuniqueid-value", 0);

#if defined(BURST_CAPTURE) && defined(VARIABLE_BURST_FPS)
    tempStr.setTo("");
    tempStr = String8::format("(%d,%d)", BURSTSHOT_MIN_FPS, BURSTSHOT_MAX_FPS);
    p.set("burstshot-fps-values", tempStr.string());
#else
    p.set("burstshot-fps-values", "(0,0)");
#endif

    p.set("drc", "false");
    p.set("3dnr", "false");
    p.set("odc", "false");

    p.set("effectrecording-hint", 0);

    /* Exposure Time */
    if (getMinExposureTime() != 0 && getMaxExposureTime() != 0) {
        p.set("min-exposure-time", getMinExposureTime());
        p.set("max-exposure-time", getMaxExposureTime());
    }
    p.set("exposure-time", 0);

    /* WB Kelvin */
    if (getMinWBK() != 0 && getMaxWBK() != 0) {
        p.set("min-wb-k", getMinWBK());
        p.set("max-wb-k", getMaxWBK());
    }
    p.set("wb-k", 0);

#if 0
    /* Limitation time of recording
    * 0 or Not declared : No limit
    * seconds           : Limit for seconds (ex. 300 : 5 minute)
    */
    p.set("max-limit-recording-time-fhd60", 300);
    p.set("max-limit-recording-time-slowmotion", 300);
    p.set("max-limit-recording-time-uhd", 300);
    p.set("max-limit-recording-time-wqhd", 300);
#endif

#ifdef USE_CP_FUSION_LIB
    m_fusionCalData = readFusionCalData(&m_fusionCalDataSize);
    if (m_fusionCalData == NULL) {
        CLOGE("readFusionCalData() fail");
    } else {
        setFusionCalData(m_fusionCalData, m_fusionCalDataSize);
    }
#endif

    m_params = p;

    /* make sure m_secCamera has all the settings we do.  applications
     * aren't required to call setParameters themselves (only if they
     * want to change something.
     */
    ret = setParameters(p);
    if (ret < 0)
        CLOGE("setParameters is fail");
}

status_t ExynosCamera1Parameters::checkVisionMode(const CameraParameters& params)
{
    status_t ret;

    /* Check vision mode */
    int intelligent_mode = params.getInt("intelligent-mode");

    ret = m_setIntelligentMode(intelligent_mode);
    if (ret != NO_ERROR) {
        CLOGE("[setParameters]Inavalid Intelligent mode");
        return ret;
    }
    m_params.set("intelligent-mode", intelligent_mode);

    CLOGD("[setParameters]intelligent_mode(%d) getVisionMode(%d)", intelligent_mode, getVisionMode());

    /* Smart stay need to skip more frames */
    int skipCompensation = m_frameSkipCounter.getCompensation();
    if (intelligent_mode == 1) {
        m_frameSkipCounter.setCompensation(skipCompensation + SMART_STAY_SKIP_COMPENSATION);
    } else {
        m_frameSkipCounter.setCompensation(skipCompensation);
    }

    if (getVisionMode() == true) {
        /* preset for each vision mode */
        switch (intelligent_mode) {
        case 2:
            m_setVisionModeFps(10);
            break;
        case 3:
            m_setVisionModeFps(5);
            break;
        default:
            m_setVisionModeFps(10);
            break;
        }

/* Vision mode custom frame rate will be enabled when application ready */
#if 0
        /* If user wants to set custom fps, vision mode set max fps to frame rate */
        int minFps = -1;
        int maxFps = -1;
        params.getPreviewFpsRange(&minFps, &maxFps);

        if (minFps > 0 && maxFps > 0) {
            CLOGD(" set user frame rate (%d)", maxFps / 1000);
            m_setVisionModeFps(maxFps / 1000);
        }
#endif

        /* smart-screen-exposure */
        int newVisionAeTarget = params.getInt("smart-screen-exposure");
        if (0 < newVisionAeTarget) {
            CLOGD("[setParameters]newVisionAeTarget : %d", newVisionAeTarget);
            m_setVisionModeAeTarget(newVisionAeTarget);
            m_params.set("smart-screen-exposure", newVisionAeTarget);
        }

        return OK;
    } else {
        return NO_ERROR;
    }
}

status_t ExynosCamera1Parameters::m_setIntelligentMode(int intelligentMode)
{
    status_t ret = NO_ERROR;
    bool visionMode = false;

    m_cameraInfo.intelligentMode = intelligentMode;

    if (intelligentMode > 1) {
        if (m_staticInfo->visionModeSupport == true) {
            visionMode = true;
        } else {
            CLOGE("[setParameters]tried to set vision mode(not supported)");
            ret = BAD_VALUE;
        }
    } else if (getVisionMode()) {
        CLOGW("vision mode can not change before stoppreview");
        visionMode = true;
    }

    m_setVisionMode(visionMode);

    return ret;
 }

int ExynosCamera1Parameters::getIntelligentMode(void)
{
    return m_cameraInfo.intelligentMode;
}

void ExynosCamera1Parameters::m_setVisionMode(bool vision)
{
    m_cameraInfo.visionMode = vision;
}

bool ExynosCamera1Parameters::getVisionMode(void)
{
    return m_cameraInfo.visionMode;
}

void ExynosCamera1Parameters::m_setVisionModeFps(int fps)
{
    m_cameraInfo.visionModeFps = fps;
}

int ExynosCamera1Parameters::getVisionModeFps(void)
{
    return m_cameraInfo.visionModeFps;
}

void ExynosCamera1Parameters::m_setVisionModeAeTarget(int ae)
{
    m_cameraInfo.visionModeAeTarget = ae;
}

int ExynosCamera1Parameters::getVisionModeAeTarget(void)
{
    return m_cameraInfo.visionModeAeTarget;
}

status_t ExynosCamera1Parameters::checkSWVdisMode(const CameraParameters& params)
{
    const char *newSwVdis = params.get("sw-vdis");
    bool currSwVdis = getSWVdisMode();
    if (newSwVdis != NULL) {
        CLOGD("[setParameters]newSwVdis %s", newSwVdis);
        bool swVdisMode = true;

        if (!strcmp(newSwVdis, "off"))
            swVdisMode = false;

        m_setSWVdisMode(swVdisMode);
        m_params.set("sw-vdis", newSwVdis);
    }

    return NO_ERROR;
}

status_t ExynosCamera1Parameters::checkVideoSize(const CameraParameters& params)
{
    /*  Video size */
    int newVideoW = 0;
    int newVideoH = 0;
    int newHwVideoW = 0;
    int newHwVideoH = 0;

    params.getVideoSize(&newVideoW, &newVideoH);

    /* size is negative, set default size */
    if ((newVideoW < 0) || (newVideoH < 0)) {
        CLOGW("size(%d x %d) is invalid", newVideoW, newVideoH);

        params.dump();

        newVideoW = 1920;
        newVideoH = 1080;
        CLOGW("set default size as (%d x %d)", newVideoW, newVideoH);
    }

    if (0 < newVideoW && 0 < newVideoH &&
        m_isSupportedVideoSize(newVideoW, newVideoH) == false) {
        return BAD_VALUE;
    }

    /* restart when video size is changed to control MCSC2 */
    int oldVideoW = 0;
    int oldVideoH = 0;

    getVideoSize(&oldVideoW, &oldVideoH);

    if ((oldVideoW != newVideoW) || (oldVideoH != newVideoH)) {
        m_setRestartPreviewChecked(true);
        m_previewSizeChanged = true;
        CLOGE("setRestartPreviewChecked true");
    }

    CLOGI("[setParameters]newVideo Size (%dx%d), ratioId(%d)",
        newVideoW, newVideoH, m_cameraInfo.videoSizeRatioId);
    m_setVideoSize(newVideoW, newVideoH);
    m_params.setVideoSize(newVideoW, newVideoH);

    newHwVideoW = newVideoW;
    newHwVideoH = newVideoH;
    m_setHwVideoSize(newHwVideoW, newHwVideoH);

    return NO_ERROR;
}

status_t ExynosCamera1Parameters::checkHWVdisMode(__unused const CameraParameters& params)
{
    status_t ret = NO_ERROR;

    bool hwVdisMode = this->getHWVdisMode();

    CLOGD("[setParameters]newHwVdis %d", hwVdisMode);

    ret = setDisEnable(hwVdisMode);
    if (ret != NO_ERROR) {
        CLOGE("setDisEnable(%d) fail", hwVdisMode);
    } else {
        if (m_flagHWVDisMode != hwVdisMode) {
            m_flagHWVDisMode = hwVdisMode;
        }
    }

    return ret;
}

status_t ExynosCamera1Parameters::check3dnrBypass(bool is3dnrMode, bool *flagBypass)
{
    int hwVideoW = 0, hwVideoH = 0;
    uint32_t minFpsRange = 0, maxFpsRange = 0;
    bool new3dnrMode = false;

    getHwVideoSize(&hwVideoW, &hwVideoH);
    getPreviewFpsRange(&minFpsRange, &maxFpsRange);

    if (is3dnrMode == true) {
        if (getDualMode() == true) {
            /* 1. TDNR : dual always disabled. */
#if defined(USE_3DNR_DUAL)
            new3dnrMode = USE_3DNR_DUAL;
#else
            new3dnrMode = false;
#endif
        } else if (hwVideoW >= 3840 && hwVideoH >= 2160) {
            /* 2. TDNR : UHD Recording Featuring */
#if defined(USE_3DNR_UHDRECORDING)
            if (minFpsRange < 60 && maxFpsRange < 60)
                new3dnrMode = USE_3DNR_UHDRECORDING;
            else
#endif
                new3dnrMode = false;
        } else if (minFpsRange > 60 || maxFpsRange > 60) {
            /* 3. TDNR : HighSpeed Recording Featuring */
#if defined(USE_3DNR_HIGHSPEED_RECORDING)
            new3dnrMode = USE_3DNR_HIGHSPEED_RECORDING;
#else
            new3dnrMode = false;
#endif
        } else if (getVtMode() > 0) {
            /* 4. TDNR : Vt call Featuring */
#if defined(USE_3DNR_VTCALL)
            new3dnrMode = USE_3DNR_VTCALL;
#else
            new3dnrMode = false;
#endif
        } else if (getIntelligentMode() == 1) {
            /* 5. TDNR : smart stay */
#if defined(USE_3DNR_SMARTSTAY)
            new3dnrMode = USE_3DNR_SMARTSTAY;
#else
            new3dnrMode = false;
#endif
        } else {
                new3dnrMode = true;
        }
    } else {
        new3dnrMode = false;
    }

    *flagBypass = !(new3dnrMode);

    return NO_ERROR;
}

bool ExynosCamera1Parameters::isSWVdisMode(void)
{
    bool swVDIS_mode = false;
    bool use3DNR_dmaout = false;
    int configMode = getConfigMode();

    int nVideoW, nVideoH;
    getVideoSize(&nVideoW, &nVideoH);

    if ((getRecordingHint() == true) &&
        (configMode == CONFIG_MODE::NORMAL
         || configMode == CONFIG_MODE::HIGHSPEED_60) &&
        (use3DNR_dmaout == false) &&
        (getSWVdisUIMode() == true) &&
        ((nVideoW == 2560 && nVideoH == 1440)
         || (nVideoW == 1920 && nVideoH == 1080)
         || (nVideoW == 1280 && nVideoH == 720)))
    {
        swVDIS_mode = true;
    }

    return swVDIS_mode;
}

bool ExynosCamera1Parameters::isSWVdisModeWithParam(int nWidth, int nHeight)
{
    bool swVDIS_mode = false;
    bool use3DNR_dmaout = false;
    int configMode = getConfigMode();

    if ((getRecordingHint() == true) &&
        (configMode == CONFIG_MODE::NORMAL
         || configMode == CONFIG_MODE::HIGHSPEED_60) &&
        (use3DNR_dmaout == false) &&
        (getSWVdisUIMode() == true) &&
        ((nWidth == 2560 && nHeight == 1440)
         || (nWidth == 1920 && nHeight == 1080)
         || (nWidth == 1280 && nHeight == 720)))
    {
        swVDIS_mode = true;
    }

    return swVDIS_mode;
}

bool ExynosCamera1Parameters::isSWVdisOnPreview(void)
{
    bool swVDIS_OnPreview = false;

    if (this->isSWVdisMode() == true) {
        if (getHighSpeedRecording() == false && getCameraId() == CAMERA_ID_BACK) {
            swVDIS_OnPreview = true;
        }
    }

    return swVDIS_OnPreview;
}

bool ExynosCamera1Parameters::getHWVdisMode(void)
{
    bool ret = this->getVideoStabilization();

    /*
     * Only true case,
     * we will test whether support or not.
     */
    if (ret == true) {
        switch (getCameraId()) {
        case CAMERA_ID_BACK:
#ifdef SUPPORT_BACK_HW_VDIS
            ret = SUPPORT_BACK_HW_VDIS;
#else
            ret = false;
#endif
            break;
        case CAMERA_ID_FRONT:
#ifdef SUPPORT_FRONT_HW_VDIS
            ret = SUPPORT_FRONT_HW_VDIS;
#else
            ret = false;
#endif
            break;
        case CAMERA_ID_BACK_1:
#ifdef SUPPORT_BACK_1_HW_VDIS
            ret = SUPPORT_BACK_1_HW_VDIS;
#else
            ret = false;
#endif
            break;
        default:
            ret = false;
            break;
        }
    }

    return ret;
}

int ExynosCamera1Parameters::getHWVdisFormat(void)
{
    return V4L2_PIX_FMT_YUYV;
}

void ExynosCamera1Parameters::m_setSWVdisMode(bool swVdis)
{
    m_cameraInfo.swVdisMode = swVdis;
}

bool ExynosCamera1Parameters::getSWVdisMode(void)
{
    return m_cameraInfo.swVdisMode;
}

void ExynosCamera1Parameters::m_setSWVdisUIMode(bool swVdisUI)
{
    m_cameraInfo.swVdisUIMode = swVdisUI;
}

bool ExynosCamera1Parameters::getSWVdisUIMode(void)
{
    return m_cameraInfo.swVdisUIMode;
}

status_t ExynosCamera1Parameters::m_adjustPreviewSize(const CameraParameters &params,
                                                     __unused int previewW, __unused int previewH,
                                                     int *newPreviewW, int *newPreviewH,
                                                     int *newCalHwPreviewW, int *newCalHwPreviewH)
{
    int ShotMode = params.getInt("shot-mode");
    int videoW = 0, videoH = 0;
    getVideoSize(&videoW, &videoH);

    /* hack : when app give 1446, we calibrate to 1440 */
    if (*newPreviewW == 1446 && *newPreviewH == 1080) {
        CLOGW("Invalid previewSize(%d/%d). so, calibrate to (1440/%d)", *newPreviewW, *newPreviewH, *newPreviewH);
        *newPreviewW = 1440;
    }

    /* calibrate H/W aligned size*/
    if (getNumOfMcscOutputPorts() < 3
        && getRecordingHint() == true
        && (videoW > *newPreviewW || videoH > *newPreviewH)) {
        /* Sharing one port for Preview and Recording */
        ExynosRect bdsRect;

        /* If the video ratio is differ with preview ratio,
           the default ratio is set into preview ratio */
        if (SIZE_RATIO(*newPreviewW, *newPreviewH) != SIZE_RATIO(videoW, videoH))
            CLOGW("preview ratio(%dx%d) is not matched with video ratio(%dx%d)",
                    *newPreviewW, *newPreviewH, videoW, videoH);

        if (this->isUseBDS() == true) {
            m_getPreviewBdsSize(&bdsRect);
        } else {
            /*
             * m_getPreviewBdsSize give only the size adjusted zoom.
             * without BDS, I want to get original bcrop size.
             * so, I get the hwSensorSize.
             */
            int hwSensorW = 0, hwSensorH = 0;
            getHwSensorSize(&hwSensorW, &hwSensorH);

            bdsRect.w = hwSensorW;
            bdsRect.h = hwSensorH;
            bdsRect.fullW = bdsRect.w;
            bdsRect.fullH = bdsRect.h;
        }

        if (bdsRect.w <= videoW && bdsRect.h <= videoH) {
            *newCalHwPreviewW = bdsRect.w;
            *newCalHwPreviewH = bdsRect.h;
        } else if (SIZE_RATIO(*newPreviewW, *newPreviewH) > SIZE_RATIO(videoW, videoH)) {
            *newCalHwPreviewW = ALIGN_UP((*newPreviewW) * videoH / (*newPreviewH), CAMERA_MCSC_ALIGN);
            *newCalHwPreviewH = videoH;
        } else if (SIZE_RATIO(*newPreviewW, *newPreviewH) < SIZE_RATIO(videoW, videoH)) {
            *newCalHwPreviewW = videoW;
            *newCalHwPreviewH = ALIGN_UP((*newPreviewH) * videoW / (*newPreviewW), CAMERA_MCSC_ALIGN);
        } else {
            *newCalHwPreviewW = videoW;
            *newCalHwPreviewH = videoH;
        }

        CLOGD("preview(%dx%d) is smaller than video(%dx%d). info : bdsRect(%dx%d), newCalHwPreview(%dx%d)",
                *newPreviewW, *newPreviewH, videoW, videoH, bdsRect.w, bdsRect.h, *newCalHwPreviewW, *newCalHwPreviewH);

        if (*newCalHwPreviewW > bdsRect.w || *newCalHwPreviewH > bdsRect.h) {
            *newCalHwPreviewW = bdsRect.w;
            *newCalHwPreviewH = bdsRect.h;
        }
    } else if (ShotMode == SHOT_MODE_PANORAMA) {
        if(SIZE_RATIO(*newPreviewW, *newPreviewH) == SIZE_RATIO(4, 3)) {
            *newCalHwPreviewW = PANORAMA_PREVIEW_WIDTH_RATIO_4_3;
            *newCalHwPreviewH = PANORAMA_PREVIEW_HEIGHT_RATIO_4_3;
        } else {
            *newCalHwPreviewW = PANORAMA_PREVIEW_WIDTH_RATIO_16_9;
            *newCalHwPreviewH = PANORAMA_PREVIEW_HEIGHT_RATIO_16_9;
        }
    } else {
        *newCalHwPreviewW = *newPreviewW;
        *newCalHwPreviewH = *newPreviewH;
    }

    return NO_ERROR;
}

/* TODO: Who explane this offset value? */
/* F/W's middle value is 5, and step is -4, -3, -2, -1, 0, 1, 2, 3, 4 */
void ExynosCamera1Parameters::m_setExposureCompensation(int32_t value)
{
    setMetaCtlExposureCompensation(&m_metadata, value);
#ifdef USE_SUBDIVIDED_EV
    setMetaCtlExposureCompensationStep(&m_metadata, m_staticInfo->exposureCompensationStep);
#endif
}

int ExynosCamera1Parameters::getBayerFormat(int pipeId)
{
    int bayerFormat = V4L2_PIX_FMT_SBGGR16;

    switch (pipeId) {
    case PIPE_FLITE:
    case PIPE_VC0:
    case PIPE_3AA:
    case PIPE_FLITE_REPROCESSING:
    case PIPE_3AA_REPROCESSING:
        bayerFormat = CAMERA_FLITE_BAYER_FORMAT;
        break;
    case PIPE_3AC:
        bayerFormat = CAMERA_3AC_BAYER_FORMAT;
        break;
    case PIPE_3AP:
    case PIPE_ISP:
        bayerFormat = CAMERA_3AP_BAYER_FORMAT;
        break;
    case PIPE_3AC_REPROCESSING:
    case PIPE_3AP_REPROCESSING:
    case PIPE_ISP_REPROCESSING:
        bayerFormat = CAMERA_3AP_REPROCESSING_BAYER_FORMAT;
        break;
    default:
        CLOGW("Invalid pipeId(%d)", pipeId);
        break;
    }

    return bayerFormat;
}

int32_t ExynosCamera1Parameters::getExposureCompensation(void)
{
    int32_t expCompensation;
    getMetaCtlExposureCompensation(&m_metadata, &expCompensation);

    return expCompensation;
}

void ExynosCamera1Parameters::m_setMeteringAreas(uint32_t num, ExynosRect2 *rect2s, int *weights)
{
    uint32_t maxNumMeteringAreas = getMaxNumMeteringAreas();

    if (maxNumMeteringAreas == 0) {
        CLOGV("maxNumMeteringAreas is 0. so, ignored");
        return;
    }

    if (maxNumMeteringAreas < num)
        num = maxNumMeteringAreas;

    if (getAutoExposureLock() == true) {
        CLOGD("autoExposure is Locked");
        return;
    }

    if (num == 1) {
        int meteringMode = getMeteringMode();

        if (isRectNull(&rect2s[0]) == true) {
            switch (meteringMode) {
                case METERING_MODE_SPOT:
                    /*
                     * Even if SPOT metering mode, area must set valid values,
                     * but areas was invalid values, we change mode to CENTER.
                     */
                    m_setMeteringMode(METERING_MODE_CENTER);
                    m_cameraInfo.isTouchMetering = false;
                    break;
                case METERING_MODE_AVERAGE:
                case METERING_MODE_CENTER:
                case METERING_MODE_MATRIX:
                default:
                    /* adjust metering setting */
                    break;
            }
        } else {
            switch (meteringMode) {
                case METERING_MODE_CENTER:
                    /*
                     * SPOT metering mode in GED camera App was not set METERING_MODE_SPOT,
                     * but set metering areas only.
                     */
                    m_setMeteringMode(METERING_MODE_SPOT);
                    m_cameraInfo.isTouchMetering = true;
                    break;
                case METERING_MODE_AVERAGE:
                case METERING_MODE_MATRIX:
                case METERING_MODE_SPOT:
                default:
                    /* adjust metering setting */
                    break;
            }
        }
    } else {
        if (num > 1 && isRectEqual(&rect2s[0], &rect2s[1]) == false) {
            /* if MATRIX mode support, mode set METERING_MODE_MATRIX */
            m_setMeteringMode(METERING_MODE_AVERAGE);
            m_cameraInfo.isTouchMetering = false;
        } else {
            m_setMeteringMode(METERING_MODE_AVERAGE);
            m_cameraInfo.isTouchMetering = false;
        }
    }

    ExynosRect bayerCropRegionRect;
    ExynosRect cropRegionRect;
    ExynosRect2 newRect2;

    getHwBayerCropRegion(&bayerCropRegionRect.w, &bayerCropRegionRect.h, &bayerCropRegionRect.x, &bayerCropRegionRect.y);

    if (isUseIspInputCrop() == true
        || isUseMcscInputCrop() == true) {
        status_t ret = NO_ERROR;
        int zoomLevel = 0;
        float zoomRatio = getZoomRatio(0) / 1000;

        zoomLevel = getZoomLevel();
        zoomRatio = getZoomRatio(zoomLevel) / 1000;
        ret = getCropRectAlign(bayerCropRegionRect.w, bayerCropRegionRect.h,
                bayerCropRegionRect.w, bayerCropRegionRect.h,
                &cropRegionRect.x, &cropRegionRect.y,
                &cropRegionRect.w, &cropRegionRect.h,
                2, 2,
                zoomLevel, zoomRatio);
    } else {
        cropRegionRect = bayerCropRegionRect;
    }

    for (uint32_t i = 0; i < num; i++) {
        bool isChangeMeteringArea = false;
        if (isRectNull(&rect2s[i]) == false)
            isChangeMeteringArea = true;
        else
            isChangeMeteringArea = false;

        if (isChangeMeteringArea == true) {
            CLOGD("from Service (%d %d %d %d) %d", rect2s->x1, rect2s->y1, rect2s->x2, rect2s->y2, getMeteringMode());
            newRect2 = convertingAndroidArea2HWAreaBcropOut(&rect2s[i], &cropRegionRect);
            CLOGD("to FW (%d %d %d %d) %d", newRect2.x1, newRect2.y1, newRect2.x2, newRect2.y2, weights[i]);
            setMetaCtlAeRegion(&m_metadata, newRect2.x1, newRect2.y1, newRect2.x2, newRect2.y2, weights[i]);
        }
    }
}

status_t ExynosCamera1Parameters::checkPictureFormat(const CameraParameters& params)
{
    int curPictureFormat = 0;
    int newPictureFormat = 0;
    int newHwPictureFormat = 0;
    const char *strNewPictureFormat = params.getPictureFormat();
    const char *strCurPictureFormat = m_params.getPictureFormat();
    if (strNewPictureFormat == NULL) {
        return NO_ERROR;
    }

    CLOGD("[setParameters]newPictureFormat %s", strNewPictureFormat);

    if (!strcmp(strNewPictureFormat, CameraParameters::PIXEL_FORMAT_JPEG)) {
        newPictureFormat = V4L2_PIX_FMT_JPEG;
        newHwPictureFormat = SCC_OUTPUT_COLOR_FMT;
    }
    else if (!strcmp(strNewPictureFormat, CameraParameters::PIXEL_FORMAT_YUV420SP) ||
             !strcmp(strNewPictureFormat, CameraParameters::PIXEL_FORMAT_YUV420SP_NV21)) {
        newPictureFormat = V4L2_PIX_FMT_NV21;
        newHwPictureFormat = SCC_OUTPUT_COLOR_FMT;
    } else {
        CLOGE(" Picture format(%s) is not supported!", strNewPictureFormat);
        return BAD_VALUE;
    }

    curPictureFormat = getPictureFormat();

    if (newPictureFormat != curPictureFormat) {
        CLOGI("[Parameters]Picture format changed, cur(%s) -> new(%s)",
                strCurPictureFormat, strNewPictureFormat);
        m_setPictureFormat(newPictureFormat);
        m_setHwPictureFormat(newHwPictureFormat);
        m_params.setPictureFormat(strNewPictureFormat);
    }

    return NO_ERROR;
}

status_t ExynosCamera1Parameters::checkMeteringMode(const CameraParameters& params)
{
    const char *strNewMeteringMode = params.get("metering");
    int newMeteringMode = -1;
    int curMeteringMode = -1;
    if (strNewMeteringMode == NULL) {
        return NO_ERROR;
    }

    CLOGD("[setParameters]strNewMeteringMode %s", strNewMeteringMode);

    {
        if (!strcmp(strNewMeteringMode, "average"))
            newMeteringMode = METERING_MODE_AVERAGE;
        else if (!strcmp(strNewMeteringMode, "center"))
            newMeteringMode = METERING_MODE_CENTER;
        else if (!strcmp(strNewMeteringMode, "matrix"))
            newMeteringMode = METERING_MODE_MATRIX;
        else if (!strcmp(strNewMeteringMode, "spot"))
            newMeteringMode = METERING_MODE_SPOT;
#ifdef TOUCH_AE
        else if (!strcmp(strNewMeteringMode, "weighted-center"))
            newMeteringMode = METERING_MODE_CENTER_TOUCH;
        else if (!strcmp(strNewMeteringMode, "weighted-matrix"))
            newMeteringMode = METERING_MODE_MATRIX_TOUCH;
        else if (!strcmp(strNewMeteringMode, "weighted-spot"))
            newMeteringMode = METERING_MODE_SPOT_TOUCH;
        else if (!strcmp(strNewMeteringMode, "weighted-average"))
            newMeteringMode = METERING_MODE_AVERAGE_TOUCH;
#endif
        else {
            CLOGE("Invalid metering newMetering(%s)", strNewMeteringMode);
            return UNKNOWN_ERROR;
        }
    }

    curMeteringMode = getMeteringMode();

    m_setMeteringMode(newMeteringMode);
    m_params.set("metering", strNewMeteringMode);

    if (curMeteringMode != newMeteringMode) {
        CLOGI(" Metering Area is changed (%d -> %d)", curMeteringMode, newMeteringMode);
        m_flagMeteringRegionChanged = true;
    }

    return NO_ERROR;
}

status_t ExynosCamera1Parameters::checkSceneMode(const CameraParameters& params)
{
    int  newSceneMode = -1;
    int  curSceneMode = -1;
    const char *strNewSceneMode = params.get(CameraParameters::KEY_SCENE_MODE);

    if (strNewSceneMode == NULL) {
        return NO_ERROR;
    }

    CLOGD("[setParameters]strNewSceneMode %s", strNewSceneMode);

    if (!strcmp(strNewSceneMode, CameraParameters::SCENE_MODE_AUTO)) {
        newSceneMode = SCENE_MODE_AUTO;
    } else if (!strcmp(strNewSceneMode, CameraParameters::SCENE_MODE_ACTION)) {
        newSceneMode = SCENE_MODE_ACTION;
    } else if (!strcmp(strNewSceneMode, CameraParameters::SCENE_MODE_PORTRAIT)) {
        newSceneMode = SCENE_MODE_PORTRAIT;
    } else if (!strcmp(strNewSceneMode, CameraParameters::SCENE_MODE_LANDSCAPE)) {
        newSceneMode = SCENE_MODE_LANDSCAPE;
    } else if (!strcmp(strNewSceneMode, CameraParameters::SCENE_MODE_NIGHT)) {
        newSceneMode = SCENE_MODE_NIGHT;
    } else if (!strcmp(strNewSceneMode, CameraParameters::SCENE_MODE_NIGHT_PORTRAIT)) {
        newSceneMode = SCENE_MODE_NIGHT_PORTRAIT;
    } else if (!strcmp(strNewSceneMode, CameraParameters::SCENE_MODE_THEATRE)) {
        newSceneMode = SCENE_MODE_THEATRE;
    } else if (!strcmp(strNewSceneMode, CameraParameters::SCENE_MODE_BEACH)) {
        newSceneMode = SCENE_MODE_BEACH;
    } else if (!strcmp(strNewSceneMode, CameraParameters::SCENE_MODE_SNOW)) {
        newSceneMode = SCENE_MODE_SNOW;
    } else if (!strcmp(strNewSceneMode, CameraParameters::SCENE_MODE_SUNSET)) {
        newSceneMode = SCENE_MODE_SUNSET;
    } else if (!strcmp(strNewSceneMode, CameraParameters::SCENE_MODE_STEADYPHOTO)) {
        newSceneMode = SCENE_MODE_STEADYPHOTO;
    } else if (!strcmp(strNewSceneMode, CameraParameters::SCENE_MODE_FIREWORKS)) {
        newSceneMode = SCENE_MODE_FIREWORKS;
    } else if (!strcmp(strNewSceneMode, CameraParameters::SCENE_MODE_SPORTS)) {
        newSceneMode = SCENE_MODE_SPORTS;
    } else if (!strcmp(strNewSceneMode, CameraParameters::SCENE_MODE_PARTY)) {
        newSceneMode = SCENE_MODE_PARTY;
    } else if (!strcmp(strNewSceneMode, CameraParameters::SCENE_MODE_CANDLELIGHT)) {
        newSceneMode = SCENE_MODE_CANDLELIGHT;
    } else {
        CLOGE("[Parameters]unmatched scene_mode(%s)", strNewSceneMode);
        return BAD_VALUE;
    }

    curSceneMode = getSceneMode();

    if (curSceneMode != newSceneMode) {
        m_setSceneMode(newSceneMode);
        m_params.set(CameraParameters::KEY_SCENE_MODE, strNewSceneMode);
        updatePreviewFpsRange();
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setSceneMode(int value)
{
    enum aa_mode mode = AA_CONTROL_AUTO;
    enum aa_scene_mode sceneMode = AA_SCENE_MODE_FACE_PRIORITY;

    switch (value) {
    case SCENE_MODE_PORTRAIT:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_PORTRAIT;
        break;
    case SCENE_MODE_LANDSCAPE:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_LANDSCAPE;
        break;
    case SCENE_MODE_NIGHT:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_NIGHT;
        break;
    case SCENE_MODE_BEACH:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_BEACH;
        break;
    case SCENE_MODE_SNOW:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_SNOW;
        break;
    case SCENE_MODE_SUNSET:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_SUNSET;
        break;
    case SCENE_MODE_FIREWORKS:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_FIREWORKS;
        break;
    case SCENE_MODE_SPORTS:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_SPORTS;
        break;
    case SCENE_MODE_PARTY:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_PARTY;
        break;
    case SCENE_MODE_CANDLELIGHT:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_CANDLELIGHT;
        break;
    case SCENE_MODE_STEADYPHOTO:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_STEADYPHOTO;
        break;
    case SCENE_MODE_ACTION:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_ACTION;
        break;
    case SCENE_MODE_NIGHT_PORTRAIT:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_NIGHT_PORTRAIT;
        break;
    case SCENE_MODE_THEATRE:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_THEATRE;
        break;
    case SCENE_MODE_AUTO:
    default:
        mode = AA_CONTROL_AUTO;
        sceneMode = AA_SCENE_MODE_FACE_PRIORITY;
        break;
    }

    m_cameraInfo.sceneMode = value;
    setMetaCtlSceneMode(&m_metadata, mode, sceneMode);
    m_cameraInfo.whiteBalanceMode = m_convertMetaCtlAwbMode(&m_metadata);
}

status_t ExynosCamera1Parameters::checkFocusMode(const CameraParameters& params)
{
    int  newFocusMode = -1;
    int  curFocusMode = -1;
    const char *strFocusMode = params.get(CameraParameters::KEY_FOCUS_MODE);
    const char *strNewFocusMode = m_adjustFocusMode(strFocusMode);

    if (strNewFocusMode == NULL) {
        return NO_ERROR;
    }

    CLOGD("[setParameters]strNewFocusMode %s", strNewFocusMode);

    if (!strcmp(strNewFocusMode, CameraParameters::FOCUS_MODE_AUTO)) {
        newFocusMode = FOCUS_MODE_AUTO;
        m_params.set(CameraParameters::KEY_FOCUS_DISTANCES,
                BACK_CAMERA_AUTO_FOCUS_DISTANCES_STR);
    } else if (!strcmp(strNewFocusMode, CameraParameters::FOCUS_MODE_INFINITY)) {
        newFocusMode = FOCUS_MODE_INFINITY;
        m_params.set(CameraParameters::KEY_FOCUS_DISTANCES,
                BACK_CAMERA_INFINITY_FOCUS_DISTANCES_STR);
    } else if (!strcmp(strNewFocusMode, CameraParameters::FOCUS_MODE_MACRO)) {
        newFocusMode = FOCUS_MODE_MACRO;
        m_params.set(CameraParameters::KEY_FOCUS_DISTANCES,
                BACK_CAMERA_MACRO_FOCUS_DISTANCES_STR);
    } else if (!strcmp(strNewFocusMode, CameraParameters::FOCUS_MODE_FIXED)) {
        newFocusMode = FOCUS_MODE_FIXED;
    } else if (!strcmp(strNewFocusMode, CameraParameters::FOCUS_MODE_EDOF)) {
        newFocusMode = FOCUS_MODE_EDOF;
    } else if (!strcmp(strNewFocusMode, CameraParameters::FOCUS_MODE_CONTINUOUS_VIDEO)) {
        newFocusMode = FOCUS_MODE_CONTINUOUS_VIDEO;
    } else if (!strcmp(strNewFocusMode, CameraParameters::FOCUS_MODE_CONTINUOUS_PICTURE)) {
        newFocusMode = FOCUS_MODE_CONTINUOUS_PICTURE;
    } else if (!strcmp(strNewFocusMode, "face-priority")) {
        newFocusMode = FOCUS_MODE_CONTINUOUS_PICTURE;
    } else if (!strcmp(strNewFocusMode, "continuous-picture-macro")) {
        newFocusMode = FOCUS_MODE_CONTINUOUS_PICTURE_MACRO;
    } else {
        CLOGE("unmatched focus_mode(%s)", strNewFocusMode);
        return BAD_VALUE;
    }

    if (!(newFocusMode & getSupportedFocusModes())){
        CLOGE(" Focus mode(%s) is not supported!", strNewFocusMode);
        return BAD_VALUE;
    }

    m_setFocusMode(newFocusMode);
    m_params.set(CameraParameters::KEY_FOCUS_MODE, strNewFocusMode);

    return NO_ERROR;
}

const char *ExynosCamera1Parameters::m_adjustFocusMode(const char *focusMode)
{
    int sceneMode = getSceneMode();
    const char *newFocusMode = NULL;

    /* TODO: vendor specific adjust */

    newFocusMode = focusMode;

    return newFocusMode;
}

void ExynosCamera1Parameters::m_setFocusMode(int focusMode)
{
    m_cameraInfo.focusMode = focusMode;

    if(getZoomActiveOn()) {
        CLOGD("[setParameters]zoom moving..");
        return;
    }

    /* TODO: Notify auto focus activity */
    if(getPreviewRunning() == true) {
        CLOGD("set Focus Mode!!!!");
        m_activityControl->setAutoFocusMode(focusMode);
    } else {
        m_setFocusmodeSetting = true;
    }
}

status_t ExynosCamera1Parameters::checkFocusAreas(const CameraParameters& params)
{
    int ret = NO_ERROR;
    const char *newFocusAreas = params.get(CameraParameters::KEY_FOCUS_AREAS);
    int newNumFocusAreas = params.getInt(CameraParameters::KEY_MAX_NUM_FOCUS_AREAS);
    int curFocusMode = getFocusMode();
    uint32_t maxNumFocusAreas = getMaxNumFocusAreas();
    int focusAreasCount = 0;

    if (newFocusAreas == NULL) {
        ExynosRect2 nullRect2[1];
        nullRect2[0].x1 = 0;
        nullRect2[0].y1 = 0;
        nullRect2[0].x2 = 0;
        nullRect2[0].y2 = 0;

        if (m_cameraInfo.numValidFocusArea != 0)
            m_setFocusAreas(0, nullRect2, NULL);

        return NO_ERROR;
    }

    if (newNumFocusAreas <= 0)
        focusAreasCount = maxNumFocusAreas;
    else
        focusAreasCount = newNumFocusAreas;

    CLOGD("[setParameters]newFocusAreas %s, numFocusAreas(%d)", newFocusAreas, focusAreasCount);

    /* In CameraParameters.h */
    /*
     * Focus area only has effect if the cur focus mode is FOCUS_MODE_AUTO,
     * FOCUS_MODE_MACRO, FOCUS_MODE_CONTINUOUS_VIDEO, or
     * FOCUS_MODE_CONTINUOUS_PICTURE.
     */
    if (curFocusMode & FOCUS_MODE_AUTO
         || curFocusMode & FOCUS_MODE_MACRO
         || curFocusMode & FOCUS_MODE_CONTINUOUS_VIDEO
         || curFocusMode & FOCUS_MODE_CONTINUOUS_PICTURE
         || curFocusMode & FOCUS_MODE_CONTINUOUS_PICTURE_MACRO
    ) {

        /* ex : (-10,-10,0,0,300),(0,0,10,10,700) */
        ExynosRect2 *rect2s = new ExynosRect2[focusAreasCount];
        int         *weights = new int[focusAreasCount];

        uint32_t validFocusedAreas = bracketsStr2Ints((char *)newFocusAreas, focusAreasCount, rect2s, weights, 1);

        /* Check duplicate area */
        if (validFocusedAreas > 1) {
            for (uint32_t k = 0; k < validFocusedAreas; k++) {
                /* check the bound of array */
                if (k >= (focusAreasCount - 1))
                    break;

                if (rect2s[k].x1 == rect2s[k+1].x1 &&
                        rect2s[k].y1 == rect2s[k+1].y1 &&
                        rect2s[k].x2 == rect2s[k+1].x2 &&
                        rect2s[k].y2 == rect2s[k+1].y2)
                    validFocusedAreas = 0;
            }
        }

        if (0 < validFocusedAreas) {
            /* CameraParameters.h */
            /*
              * A special case of single focus area (0,0,0,0,0) means driver to decide
              * the focus area. For example, the driver may use more signals to decide
              * focus areas and change them dynamically. Apps can set (0,0,0,0,0) if they
              * want the driver to decide focus areas.
              */
            m_setFocusAreas(validFocusedAreas, rect2s, weights);
            m_params.set(CameraParameters::KEY_FOCUS_AREAS, newFocusAreas);
        } else {
            CLOGE("FocusAreas value is invalid");
            ret = UNKNOWN_ERROR;
        }

        delete [] rect2s;
        delete [] weights;
    }

    return ret;
}

void ExynosCamera1Parameters::m_setFocusAreas(uint32_t numValid, ExynosRect2 *rect2s, int *weights)
{
    uint32_t maxNumFocusAreas = getMaxNumFocusAreas();
    if (maxNumFocusAreas < numValid)
        numValid = maxNumFocusAreas;

    ExynosRect2 newRect2(0,0,0,0);
    int defaultWeight = 1000;

#ifdef USE_CP_FUSION_LIB
    if (1) {
        /* m_setFocusMode(FOCUS_MODE_AUTO); */

        ExynosRect cropRegionRect;

        // This ROI is set by according to F/W guideLine.
        int androidFocusRoiW = 2000 / 10;
        int androidFocusRoiH = 2000 / 5;

        // we put ROI in center. so, divide by 2.
        newRect2.x1 = -androidFocusRoiW / 2;
        newRect2.y1 = -androidFocusRoiH / 2;
        newRect2.x2 =  androidFocusRoiW / 2;
        newRect2.y2 =  androidFocusRoiH / 2;

        getHwBayerCropRegion(&cropRegionRect.w, &cropRegionRect.h, &cropRegionRect.x, &cropRegionRect.y);

        newRect2 = convertingAndroidArea2HWAreaBcropOut(&newRect2, &cropRegionRect);
#else

    if ((numValid == 1 || numValid == 0) && (isRectNull(&rect2s[0]) == true)) {
        /* m_setFocusMode(FOCUS_MODE_AUTO); */
#endif
        m_activityControl->setAutoFcousArea(newRect2, defaultWeight);

        m_activityControl->touchAFMode = false;
        m_activityControl->touchAFModeForFlash = false;
    } else {
        ExynosRect bayerCropRegionRect;
        ExynosRect cropRegionRect;

        getHwBayerCropRegion(&bayerCropRegionRect.w, &bayerCropRegionRect.h, &bayerCropRegionRect.x, &bayerCropRegionRect.y);

        if (isUseIspInputCrop() == true
            || isUseMcscInputCrop() == true) {
            status_t ret = NO_ERROR;
            int zoomLevel = 0;
            float zoomRatio = getZoomRatio(0) / 1000;

            zoomLevel = getZoomLevel();
            zoomRatio = getZoomRatio(zoomLevel) / 1000;
            ret = getCropRectAlign(bayerCropRegionRect.w, bayerCropRegionRect.h,
                    bayerCropRegionRect.w, bayerCropRegionRect.h,
                    &cropRegionRect.x, &cropRegionRect.y,
                    &cropRegionRect.w, &cropRegionRect.h,
                    2, 2,
                    zoomLevel, zoomRatio);
        } else {
            cropRegionRect = bayerCropRegionRect;
        }

        for (uint32_t i = 0; i < numValid; i++) {
            newRect2 = convertingAndroidArea2HWAreaBcropOut(&rect2s[i], &cropRegionRect);
            /*setMetaCtlAfRegion(&m_metadata, rect2s[i].x1, rect2s[i].y1,
                                    rect2s[i].x2, rect2s[i].y2, weights[i]);*/
            m_activityControl->setAutoFcousArea(newRect2, weights[i]);

            defaultWeight = weights[i];
        }
        m_activityControl->touchAFMode = true;
        m_activityControl->touchAFModeForFlash = true;
    }

    // f/w support only one region now.
    setMetaCtlAfRegion(&m_metadata, newRect2.x1, newRect2.y1, newRect2.x2, newRect2.y2, defaultWeight);

    m_cameraInfo.numValidFocusArea = numValid;
}

debug_attribute_t *ExynosCamera1Parameters::getDebug2Attribute(void)
{
    return &mDebugInfo2;
}

void ExynosCamera1Parameters::m_setExifChangedAttribute(exif_attribute_t    *exifInfo,
                                                        ExynosRect          *pictureRect,
                                                        ExynosRect          *thumbnailRect,
                                                        __unused camera2_dm *dm,
                                                        camera2_udm         *udm,
                                                        bool                useDebugInfo2)
{
    debug_attribute_t &debugInfo = ((useDebugInfo2 == false) ? mDebugInfo : mDebugInfo2);

    /* 2 0th IFD TIFF Tags */
    /* 3 Width */
    exifInfo->width = pictureRect->w;
    /* 3 Height */
    exifInfo->height = pictureRect->h;

    /* 3 Orientation */
    switch (m_cameraInfo.rotation) {
    case 90:
        exifInfo->orientation = EXIF_ORIENTATION_90;
        break;
    case 180:
        exifInfo->orientation = EXIF_ORIENTATION_180;
        break;
    case 270:
        exifInfo->orientation = EXIF_ORIENTATION_270;
        break;
    case 0:
    default:
        exifInfo->orientation = EXIF_ORIENTATION_UP;
        break;
    }

    /* 3 Maker note */
    /* Clear previous debugInfo data */
    memset((void *)debugInfo.debugData[APP_MARKER_4], 0, debugInfo.debugSize[APP_MARKER_4]);
    /* back-up udm info for exif's maker note */
    memcpy((void *)debugInfo.debugData[APP_MARKER_4], (void *)udm, sizeof(struct camera2_udm));
    size_t offset = sizeof(struct camera2_udm);

// Check debug_attribute_t struct in ExynosExif.h
    int realDataSize = 0;
    m_staticInfoExifLock.lock();
    /* Copy sensorId data to debugData*/
    memcpy((void *)(debugInfo.debugData[APP_MARKER_4] + offset),
           (void *)&m_staticInfo->sensor_id_exif_info, realDataSize);
    offset += realDataSize;
    m_staticInfoExifLock.unlock();

#ifdef BURST_CAPTURE
    if (getSeriesShotCount() && getSeriesShotMode() == SERIES_SHOT_MODE_BURST) {
        unsigned char l_makernote[98] = { 0x07, 0x00, 0x01, 0x00, 0x07, 0x00, 0x04, 0x00, 0x00, 0x00,
                                          0x30, 0x31, 0x30, 0x30, 0x02, 0x00, 0x04, 0x00, 0x01, 0x00,
                                          0x00, 0x00, 0x00, 0x20, 0x01, 0x00, 0x40, 0x00, 0x04, 0x00,
                                          0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x00,
                                          0x04, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                          0x10, 0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x5A, 0x00,
                                          0x00, 0x00, 0x50, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00,
                                          0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x00, 0x01, 0x00,
                                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        exifInfo->maker_note_size = 98;
        memcpy(exifInfo->maker_note, l_makernote, sizeof(l_makernote));
    } else
#endif
    {
        exifInfo->maker_note_size = 0;
    }

    /* 3 Date time */
    struct timeval rawtime;
    struct tm timeinfo;
    gettimeofday(&rawtime, NULL);
    localtime_r((time_t *)&rawtime.tv_sec, &timeinfo);
    strftime((char *)exifInfo->date_time, 20, "%Y:%m:%d %H:%M:%S", &timeinfo);
    snprintf((char *)exifInfo->sec_time, 5, "%04d", (int)(rawtime.tv_usec/1000));

    /* 2 0th IFD Exif Private Tags */
    bool flagSLSIAlgorithm = true;
    /*
     * vendorSpecific2[100]      : exposure
     * vendorSpecific2[101]      : iso(gain)
     * vendorSpecific2[102] /256 : Bv
     * vendorSpecific2[103]      : Tv
     */

    /* 3 ISO Speed Rating */
    exifInfo->iso_speed_rating = udm->internal.vendorSpecific2[101];

    /* 3 Exposure Program */
    if (m_exposureTimeCapture == 0) {
        exifInfo->exposure_program = EXIF_DEF_EXPOSURE_PROGRAM;
    } else {
        exifInfo->exposure_program = EXIF_DEF_EXPOSURE_MANUAL;
    }

    /* 3 Exposure Time */
    if (m_exposureTimeCapture == 0) {
        exifInfo->exposure_time.num = 1;

        if (udm->ae.vendorSpecific[0] == 0xAEAEAEAE)
            exifInfo->exposure_time.den = (uint32_t)udm->ae.vendorSpecific[64];
        else
            exifInfo->exposure_time.den = (uint32_t)udm->internal.vendorSpecific2[100];
    } else if (m_exposureTimeCapture > 0 && m_exposureTimeCapture <= CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT) {
        exifInfo->exposure_time.num = 1;
        if ((1000000 % m_exposureTimeCapture) != 0) {
            uint32_t dig;
            if (m_exposureTimeCapture < 100) {
                dig = 1000;
            } else if (m_exposureTimeCapture < 1000) {
                dig = 100;
            } else if (m_exposureTimeCapture < 10000) {
                dig = 10;
            } else {
                dig = 5;
            }
            exifInfo->exposure_time.den = ROUND_OFF_DIGIT((int)(1000000 / m_exposureTimeCapture), dig);
        } else {
            exifInfo->exposure_time.den = (int)(1000000 / m_exposureTimeCapture);
        }
    } else {
        exifInfo->exposure_time.num = (uint32_t)m_exposureTimeCapture / 1000;
        exifInfo->exposure_time.den = 1000;
    }

    /* 3 Shutter Speed */
    if (m_exposureTimeCapture <= CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT) {
        if (udm->ae.vendorSpecific[0] == 0xAEAEAEAE)
            exifInfo->shutter_speed.num = (uint32_t)(ROUND_OFF_HALF(((double)(udm->ae.vendorSpecific[69] / 256.f) * EXIF_DEF_APEX_DEN), 0));
        else
            exifInfo->shutter_speed.num = (uint32_t)(ROUND_OFF_HALF(((double)(udm->internal.vendorSpecific2[103] / 256.f) * EXIF_DEF_APEX_DEN), 0));

        exifInfo->shutter_speed.den = EXIF_DEF_APEX_DEN;
    } else {
        exifInfo->shutter_speed.num = (int32_t)(log2((double)m_exposureTimeCapture / 1000000) * -100);
        exifInfo->shutter_speed.den = 100;
    }

    /* 3 Aperture */
    exifInfo->aperture.num = APEX_FNUM_TO_APERTURE((double)(exifInfo->fnumber.num) / (double)(exifInfo->fnumber.den)) * m_staticInfo->apertureDen;
    exifInfo->aperture.den = m_staticInfo->apertureDen;

    /* 3 Max Aperture */
    exifInfo->max_aperture.num = APEX_FNUM_TO_APERTURE((double)(exifInfo->fnumber.num) / (double)(exifInfo->fnumber.den)) * m_staticInfo->apertureDen;
    exifInfo->max_aperture.den = m_staticInfo->apertureDen;

    /* 3 Brightness */
    int temp = udm->internal.vendorSpecific2[102];
    if ((int)udm->ae.vendorSpecific[102] < 0)
        temp = -temp;

    exifInfo->brightness.num = (int32_t)(ROUND_OFF_HALF((double)((temp * EXIF_DEF_APEX_DEN) / 256.f), 0));
    if ((int)udm->ae.vendorSpecific[102] < 0)
        exifInfo->brightness.num = -exifInfo->brightness.num;

    exifInfo->brightness.den = EXIF_DEF_APEX_DEN;

    CLOGD("udm->internal.vendorSpecific2[100](%d)", udm->internal.vendorSpecific2[100]);
    CLOGD("udm->internal.vendorSpecific2[101](%d)", udm->internal.vendorSpecific2[101]);
    CLOGD("udm->internal.vendorSpecific2[102](%d)", udm->internal.vendorSpecific2[102]);
    CLOGD("udm->internal.vendorSpecific2[103](%d)", udm->internal.vendorSpecific2[103]);

    CLOGD("iso_speed_rating(%d)", exifInfo->iso_speed_rating);
    CLOGD("exposure_time(%d/%d)", exifInfo->exposure_time.num, exifInfo->exposure_time.den);
    CLOGD("shutter_speed(%d/%d)", exifInfo->shutter_speed.num, exifInfo->shutter_speed.den);
    CLOGD("aperture     (%d/%d)", exifInfo->aperture.num, exifInfo->aperture.den);
    CLOGD("brightness   (%d/%d)", exifInfo->brightness.num, exifInfo->brightness.den);

    /* 3 Exposure Bias */
    if (m_exposureTimeCapture == 0) {
        exifInfo->exposure_bias.num = (int32_t)getExposureCompensation() * (m_staticInfo->exposureCompensationStep * 10);
        exifInfo->exposure_bias.den = 10;
    } else {
        exifInfo->exposure_bias.num = 0;
        exifInfo->exposure_bias.den = 1;
    }

    /* 3 Metering Mode */
    {
        switch (m_cameraInfo.meteringMode) {
        case METERING_MODE_CENTER:
            exifInfo->metering_mode = EXIF_METERING_CENTER;
            break;
        case METERING_MODE_MATRIX:
            exifInfo->metering_mode = EXIF_METERING_AVERAGE;
            break;
        case METERING_MODE_SPOT:
#ifdef TOUCH_AE
        case METERING_MODE_CENTER_TOUCH:
        case METERING_MODE_SPOT_TOUCH:
        case METERING_MODE_AVERAGE_TOUCH:
        case METERING_MODE_MATRIX_TOUCH:
#endif
            exifInfo->metering_mode = EXIF_METERING_SPOT;
            break;
        case METERING_MODE_AVERAGE:
        default:
            exifInfo->metering_mode = EXIF_METERING_AVERAGE;
            break;
        }
    }

    /* 3 Flash */
    if (m_cameraInfo.flashMode == FLASH_MODE_OFF) {
        exifInfo->flash = 0;
    } else if (m_cameraInfo.flashMode == FLASH_MODE_TORCH) {
        exifInfo->flash = 1;
    } else {
        exifInfo->flash = getMarkingOfExifFlash();
    }

    /* 3 White Balance */
    if (m_cameraInfo.whiteBalanceMode == WHITE_BALANCE_AUTO)
        exifInfo->white_balance = EXIF_WB_AUTO;
    else
        exifInfo->white_balance = EXIF_WB_MANUAL;

    /* 3 Focal Length in 35mm length */
    exifInfo->focal_length_in_35mm_length = m_staticInfo->focalLengthIn35mmLength;

    /* 3 Scene Capture Type */
    switch (m_cameraInfo.sceneMode) {
    case SCENE_MODE_PORTRAIT:
        exifInfo->scene_capture_type = EXIF_SCENE_PORTRAIT;
        break;
    case SCENE_MODE_LANDSCAPE:
        exifInfo->scene_capture_type = EXIF_SCENE_LANDSCAPE;
        break;
    case SCENE_MODE_NIGHT:
        exifInfo->scene_capture_type = EXIF_SCENE_NIGHT;
        break;
    default:
        exifInfo->scene_capture_type = EXIF_SCENE_STANDARD;
        break;
    }

    /* 3 Image Unique ID */
    /* 2 0th IFD GPS Info Tags */
    if (m_cameraInfo.gpsLatitude != 0 && m_cameraInfo.gpsLongitude != 0) {
        if (m_cameraInfo.gpsLatitude > 0)
            strncpy((char *)exifInfo->gps_latitude_ref, "N", 2);
        else
            strncpy((char *)exifInfo->gps_latitude_ref, "S", 2);

        if (m_cameraInfo.gpsLongitude > 0)
            strncpy((char *)exifInfo->gps_longitude_ref, "E", 2);
        else
            strncpy((char *)exifInfo->gps_longitude_ref, "W", 2);

        if (m_cameraInfo.gpsAltitude > 0)
            exifInfo->gps_altitude_ref = 0;
        else
            exifInfo->gps_altitude_ref = 1;

        double latitude = fabs(m_cameraInfo.gpsLatitude);
        double longitude = fabs(m_cameraInfo.gpsLongitude);
        double altitude = fabs(m_cameraInfo.gpsAltitude);

        exifInfo->gps_latitude[0].num = (uint32_t)latitude;
        exifInfo->gps_latitude[0].den = 1;
        exifInfo->gps_latitude[1].num = (uint32_t)((latitude - exifInfo->gps_latitude[0].num) * 60);
        exifInfo->gps_latitude[1].den = 1;
        exifInfo->gps_latitude[2].num = (uint32_t)(round((((latitude - exifInfo->gps_latitude[0].num) * 60)
                                        - exifInfo->gps_latitude[1].num) * 60));
        exifInfo->gps_latitude[2].den = 1;

        exifInfo->gps_longitude[0].num = (uint32_t)longitude;
        exifInfo->gps_longitude[0].den = 1;
        exifInfo->gps_longitude[1].num = (uint32_t)((longitude - exifInfo->gps_longitude[0].num) * 60);
        exifInfo->gps_longitude[1].den = 1;
        exifInfo->gps_longitude[2].num = (uint32_t)(round((((longitude - exifInfo->gps_longitude[0].num) * 60)
                                        - exifInfo->gps_longitude[1].num) * 60));
        exifInfo->gps_longitude[2].den = 1;

        exifInfo->gps_altitude.num = (uint32_t)altitude;
        exifInfo->gps_altitude.den = 1;

        struct tm tm_data;
        gmtime_r(&m_cameraInfo.gpsTimeStamp, &tm_data);
        exifInfo->gps_timestamp[0].num = tm_data.tm_hour;
        exifInfo->gps_timestamp[0].den = 1;
        exifInfo->gps_timestamp[1].num = tm_data.tm_min;
        exifInfo->gps_timestamp[1].den = 1;
        exifInfo->gps_timestamp[2].num = tm_data.tm_sec;
        exifInfo->gps_timestamp[2].den = 1;
        snprintf((char*)exifInfo->gps_datestamp, sizeof(exifInfo->gps_datestamp),
                "%04d:%02d:%02d", tm_data.tm_year + 1900, tm_data.tm_mon + 1, tm_data.tm_mday);

        exifInfo->enableGps = true;
    } else {
        exifInfo->enableGps = false;
    }

    /* 2 1th IFD TIFF Tags */
    exifInfo->widthThumb = thumbnailRect->w;
    exifInfo->heightThumb = thumbnailRect->h;
}

#ifdef USE_BINNING_MODE
int ExynosCamera1Parameters::getBinningMode(void)
{
    int ret = 0;
    int vt_mode = getVtMode();

    if (m_binningProperty == true)
        CLOGI("binning mode = %d", m_binningProperty);

    if (m_staticInfo->vtcallSizeLutMax == 0 || m_staticInfo->vtcallSizeLut == NULL) {
       CLOGV("vtCallSizeLut is NULL, can't support the binnig mode");
       return ret;
    }

    /* For VT Call with DualCamera Scenario */
    if (getDualMode()
       ) {
        CLOGV("DualMode can't support the binnig mode.(%d,%d)", getCameraId(), getDualMode());
        return ret;
    }

    if (vt_mode != 3 && vt_mode > 0 && vt_mode < 5) {
        ret = 1;
    }
    else {
        if (m_binningProperty == true) {
            ret = 1;
        }
    }
    return ret;
}
#endif

status_t ExynosCamera1Parameters::checkFlashMode(const CameraParameters& params)
{
    int newFlashMode = -1;
    int curFlashMode = -1;
    const char *strFlashMode = params.get(CameraParameters::KEY_FLASH_MODE);
    const char *strNewFlashMode = m_adjustFlashMode(strFlashMode);

    if (strNewFlashMode == NULL) {
        return NO_ERROR;
    }

    CLOGD("[setParameters]strNewFlashMode %s", strNewFlashMode);

    if (!strcmp(strNewFlashMode, CameraParameters::FLASH_MODE_OFF)) {
        newFlashMode = FLASH_MODE_OFF;
    } else if (!strcmp(strNewFlashMode, CameraParameters::FLASH_MODE_AUTO)) {
        newFlashMode = FLASH_MODE_AUTO;
    } else if (!strcmp(strNewFlashMode, CameraParameters::FLASH_MODE_ON)) {
        newFlashMode = FLASH_MODE_ON;
    } else if (!strcmp(strNewFlashMode, CameraParameters::FLASH_MODE_RED_EYE)) {
        newFlashMode = FLASH_MODE_RED_EYE;
    } else if (!strcmp(strNewFlashMode, CameraParameters::FLASH_MODE_TORCH)) {
        newFlashMode = FLASH_MODE_TORCH;
    } else {
        CLOGE("unmatched flash_mode(%s), turn off flash", strNewFlashMode);
        newFlashMode = FLASH_MODE_OFF;
        strNewFlashMode = CameraParameters::FLASH_MODE_OFF;
        return BAD_VALUE;
    }

#ifndef UNSUPPORT_FLASH
    if (!(newFlashMode & getSupportedFlashModes())) {
        {
            CLOGE(" Flash mode(%s) is not supported!", strNewFlashMode);
            return BAD_VALUE;
        }
    }
#endif

    curFlashMode = getFlashMode();

    if (curFlashMode != newFlashMode) {
        m_setFlashMode(newFlashMode);
        m_params.set(CameraParameters::KEY_FLASH_MODE, strNewFlashMode);
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setFlashMode(int flashMode)
{
    m_cameraInfo.flashMode = flashMode;

    /* TODO: Notity flash activity */
    m_activityControl->setFlashMode(flashMode);

#ifdef USE_FW_FLASHMODE
    enum camera_flash_mode newFlashMode;

    switch(flashMode) {
    case FLASH_MODE_OFF:
        newFlashMode = CAMERA_FLASH_MODE_OFF;
        break;
    case FLASH_MODE_AUTO:
        newFlashMode = CAMERA_FLASH_MODE_AUTO;
        break;
    case FLASH_MODE_ON:
        newFlashMode = CAMERA_FLASH_MODE_ON;
        break;
    case FLASH_MODE_RED_EYE:
        newFlashMode = CAMERA_FLASH_MODE_RED_EYE;
        break;
    case FLASH_MODE_TORCH:
        newFlashMode = CAMERA_FLASH_MODE_TORCH;
        break;
    default:
        newFlashMode = CAMERA_FLASH_MODE_OFF;
        CLOGE("Flash mode(%d) is not supported", flashMode);
        break;
    }
    setMetaUctlFlashMode(&m_metadata, newFlashMode);
#endif
}

status_t ExynosCamera1Parameters::checkDualMode(const CameraParameters& params)
{
    /* dual_mode */
    bool flagDualMode = false;
    int newDualMode = params.getInt("dual_mode");

    if (newDualMode == 1) {
        CLOGD("[setParameters]newDualMode : %d", newDualMode);
        flagDualMode = true;
    }

    setDualMode(flagDualMode);
    m_params.set("dual_mode", newDualMode);

    if (newDualMode == 1) {
        m_setDualShotMode(params);
    }

    return NO_ERROR;
}

status_t ExynosCamera1Parameters::checkShotMode(const CameraParameters& params)
{
    int newShotMode = params.getInt("shot-mode");
    int orgShotMode = params.getInt("shot-mode");
    int curShotMode = -1;

    if (newShotMode < 0) {
        return NO_ERROR;
    }

    CLOGD("[setParameters]newShotMode %d", newShotMode);

    curShotMode = getShotMode();

    if ((getRecordingHint() == true)
        ) {
        m_setShotMode(SHOT_MODE_NORMAL);
        m_params.set("shot-mode", SHOT_MODE_NORMAL);
    } else if (curShotMode != newShotMode) {
        m_setShotMode(newShotMode);
        m_params.set("shot-mode", orgShotMode);

        updatePreviewFpsRange();
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setShotMode(int shotMode)
{
    enum aa_mode mode = AA_CONTROL_AUTO;
    enum aa_scene_mode sceneMode = AA_SCENE_MODE_FACE_PRIORITY;
    bool changeSceneMode = true;

    switch (shotMode) {
    case SHOT_MODE_DRAMA:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_DRAMA;
        break;
    case SHOT_MODE_3D_PANORAMA:
    case SHOT_MODE_PANORAMA:
    case SHOT_MODE_FRONT_PANORAMA:
    case SHOT_MODE_INTERACTIVE:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_PANORAMA;
        break;
    case SHOT_MODE_NIGHT:
    case SHOT_MODE_NIGHT_SCENE:
        {
            mode = AA_CONTROL_USE_SCENE_MODE;
            sceneMode = AA_SCENE_MODE_LLS;
        }
        break;
    case SHOT_MODE_ANIMATED_SCENE:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_ANIMATED;
        break;
    case SHOT_MODE_SPORTS:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_SPORTS;
        break;
    case SHOT_MODE_GOLF:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_GOLF;
        break;
    case SHOT_MODE_NORMAL:
        if (getDualMode() == true) {
            mode = AA_CONTROL_USE_SCENE_MODE;
            sceneMode = AA_SCENE_MODE_DUAL;
        } else {
            mode = AA_CONTROL_AUTO;
            sceneMode = AA_SCENE_MODE_FACE_PRIORITY;
        }
        break;
    case SHOT_MODE_AUTO:
    case SHOT_MODE_BEAUTY_FACE:
    case SHOT_MODE_BEST_PHOTO:
    case SHOT_MODE_BEST_FACE:
    case SHOT_MODE_ERASER:
    case SHOT_MODE_RICH_TONE:
    case SHOT_MODE_STORY:
    case SHOT_MODE_SELFIE_ALARM:
    case SHOT_MODE_FASTMOTION:
    case SHOT_MODE_ANTI_FOG:
        mode = AA_CONTROL_AUTO;
        sceneMode = AA_SCENE_MODE_FACE_PRIORITY;
        break;
    case SHOT_MODE_PRO_MODE:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_PRO_MODE;
        break;
    case SHOT_MODE_DUAL:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_DUAL;
        break;
    case SHOT_MODE_VIDEO_COLLAGE:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_VIDEO_COLLAGE;
        break;
    case SHOT_MODE_AUTO_PORTRAIT:
    case SHOT_MODE_PET:
    default:
        changeSceneMode = false;
        break;
    }

    m_cameraInfo.shotMode = shotMode;
    if (changeSceneMode == true)
        setMetaCtlSceneMode(&m_metadata, mode, sceneMode);
}

int ExynosCamera1Parameters::getShotMode(void)
{
    return m_cameraInfo.shotMode;
}

status_t ExynosCamera1Parameters::checkVtMode(const CameraParameters& params)
{
    int newVTMode = params.getInt("vtmode");
    int curVTMode = -1;

    CLOGD("[setParameters]newVTMode %d", newVTMode);

    /*
     * VT mode
     *   1: 3G vtmode (176x144, Fixed 7fps)
     *   2: LTE or WIFI vtmode (640x480, Fixed 15fps)
     *   3: Reserved : Smart Stay
     *   4: CHINA vtmode (1280x720, Fixed 30fps)
     */
    if (newVTMode == 3 || (newVTMode < 0 || newVTMode > 4)) {
        newVTMode = 0;
    }

    curVTMode = getVtMode();

    if (curVTMode != newVTMode) {
        m_setVtMode(newVTMode);
        m_params.set("vtmode", newVTMode);
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setVtMode(int vtMode)
{
    m_cameraInfo.vtMode = vtMode;

    setMetaVtMode(&m_metadata, (enum camera_vt_mode)vtMode);
}

int ExynosCamera1Parameters::getVtMode(void)
{
    return m_cameraInfo.vtMode;
}

status_t ExynosCamera1Parameters::checkVRMode(const CameraParameters& params)
{
    int newVRMode =  params.getInt("vrmode");
    int curVRMode = getVRMode();

    CLOGD("[setParameters]newVRMode %d", newVRMode);

    if (curVRMode != newVRMode) {
        m_setVRMode(newVRMode);
        m_params.set("vrmode", newVRMode);
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setVRMode(int vrMode)
{
    m_cameraInfo.vrMode = vrMode;
}

int ExynosCamera1Parameters::getVRMode(void)
{
    return m_cameraInfo.vrMode;
}

status_t ExynosCamera1Parameters::checkGamma(const CameraParameters& params)
{
    bool newGamma = false;
    bool curGamma = false;
    const char *strNewGamma = params.get("video_recording_gamma");

    if (strNewGamma == NULL) {
        return NO_ERROR;
    }

    CLOGD("[setParameters]strNewGamma %s", strNewGamma);

    if (!strcmp(strNewGamma, "off")) {
        newGamma = false;
    } else if (!strcmp(strNewGamma, "on")) {
        newGamma = true;
    } else {
        CLOGE("unmatched gamma(%s)", strNewGamma);
        return BAD_VALUE;
    }

    curGamma = getGamma();

    if (curGamma != newGamma) {
        m_setGamma(newGamma);
        m_params.set("video_recording_gamma", strNewGamma);
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setGamma(bool gamma)
{
    m_cameraInfo.gamma = gamma;
}

bool ExynosCamera1Parameters::getGamma(void)
{
    return m_cameraInfo.gamma;
}

status_t ExynosCamera1Parameters::checkSlowAe(const CameraParameters& params)
{
    bool newSlowAe = false;
    bool curSlowAe = false;
    const char *strNewSlowAe = params.get("slow_ae");

    if (strNewSlowAe == NULL) {
        return NO_ERROR;
    }

    CLOGD("[setParameters]strNewSlowAe %s", strNewSlowAe);

    if (!strcmp(strNewSlowAe, "off"))
        newSlowAe = false;
    else if (!strcmp(strNewSlowAe, "on"))
        newSlowAe = true;
    else {
        CLOGE("unmatched slow_ae(%s)", strNewSlowAe);
        return BAD_VALUE;
    }

    curSlowAe = getSlowAe();

    if (curSlowAe != newSlowAe) {
        m_setSlowAe(newSlowAe);
        m_params.set("slow_ae", strNewSlowAe);
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setSlowAe(bool slowAe)
{
    m_cameraInfo.slowAe = slowAe;
}

bool ExynosCamera1Parameters::getSlowAe(void)
{
    return m_cameraInfo.slowAe;
}

status_t ExynosCamera1Parameters::checkImageUniqueId(__unused const CameraParameters& params)
{
    const char *strCurImageUniqueId = m_params.get("imageuniqueid-value");
    const char *strNewImageUniqueId = NULL;

    if(strCurImageUniqueId == NULL || strcmp(strCurImageUniqueId, "") == 0 || strcmp(strCurImageUniqueId, "0") == 0) {
        strNewImageUniqueId = getImageUniqueId();

        if (strNewImageUniqueId != NULL && strcmp(strNewImageUniqueId, "") != 0) {
            CLOGD("[setParameters]newImageUniqueId %s ", strNewImageUniqueId );
            m_params.set("imageuniqueid-value", strNewImageUniqueId);
        }
    }
    return NO_ERROR;
}

const char *ExynosCamera1Parameters::getImageUniqueId(void)
{
    return m_cameraInfo.imageUniqueId;
}

#ifdef BURST_CAPTURE
status_t ExynosCamera1Parameters::checkSeriesShotFilePath(const CameraParameters& params)
{
    const char *seriesShotFilePath = params.get("capture-burst-filepath");

    if (seriesShotFilePath != NULL) {
        snprintf(m_seriesShotFilePath, sizeof(m_seriesShotFilePath), "%s", seriesShotFilePath);
        CLOGD("[setParameters]seriesShotFilePath %s", seriesShotFilePath);
        m_params.set("capture-burst-filepath", seriesShotFilePath);
    } else {
        CLOGD("[setParameters]seriesShotFilePath NULL");
        memset(m_seriesShotFilePath, 0, CAMERA_FILE_PATH_SIZE);
    }

    return NO_ERROR;
}

#ifdef VARIABLE_BURST_FPS
void ExynosCamera1Parameters::setBurstShotDuration(int fps)
{
    if (fps == BURSTSHOT_MIN_FPS)
        m_burstShotDuration = MIN_BURST_DURATION;
    else
        m_burstShotDuration = NORMAL_BURST_DURATION;
}

int ExynosCamera1Parameters::getBurstShotDuration(void)
{
    return m_burstShotDuration;
}
#endif /* VARIABLE_BURST_FPS */
#endif /* BURST_CAPTURE */

status_t ExynosCamera1Parameters::checkSeriesShotMode(const CameraParameters& params)
{
#ifdef BURST_CAPTURE
    int burstCount = params.getInt("burst-capture");
    int bestCount = params.getInt("best-capture");

    CLOGD("DEBUG(%s): burstCount(%d), bestCount(%d)", "setParameters", burstCount, bestCount);

    if (burstCount < 0 || bestCount < 0) {
        CLOGE("ERR(%s[%d]): Invalid burst-capture count(%d), best-capture count(%d)", __FUNCTION__, __LINE__, burstCount, bestCount);
        return BAD_VALUE;
    }

    // use following when app uses burst-capture and best-capture
    /* TODO: select shot count */
    if (bestCount > burstCount) {
        m_setSeriesShotCount(bestCount);
        m_params.set("burst-capture", 0);
        m_params.set("best-capture", bestCount);
    } else {
        m_setSeriesShotCount(burstCount);
        m_params.set("burst-capture", burstCount);
        m_params.set("best-capture", 0);
    }
#endif

    return NO_ERROR;
}

#ifdef BURST_CAPTURE
int ExynosCamera1Parameters::getSeriesShotSaveLocation(void)
{
    int seriesShotSaveLocation = m_seriesShotSaveLocation;
    int shotMode = getShotMode();

    seriesShotSaveLocation = BURST_SAVE_CALLBACK;

    return seriesShotSaveLocation;
}

void ExynosCamera1Parameters::setSeriesShotSaveLocation(int ssaveLocation)
{
    m_seriesShotSaveLocation = ssaveLocation;
}

char *ExynosCamera1Parameters::getSeriesShotFilePath(void)
{
    return m_seriesShotFilePath;
}
#endif

int ExynosCamera1Parameters::getSeriesShotDuration(void)
{
    return NORMAL_BURST_DURATION;
}

int ExynosCamera1Parameters::getSeriesShotMode(void)
{
    return m_cameraInfo.seriesShotMode;
}

void ExynosCamera1Parameters::setSeriesShotMode(int sshotMode, int count)
{
    int sshotCount = 0;
    int shotMode = getShotMode();
    if (sshotMode == SERIES_SHOT_MODE_BURST) {
        if (shotMode == SHOT_MODE_BEST_PHOTO) {
            sshotMode = SERIES_SHOT_MODE_BEST_PHOTO;
            sshotCount = 8;
        } else if (shotMode == SHOT_MODE_BEST_FACE) {
            sshotMode = SERIES_SHOT_MODE_BEST_FACE;
            sshotCount = 5;
        } else if (shotMode == SHOT_MODE_ERASER) {
            sshotMode = SERIES_SHOT_MODE_ERASER;
            sshotCount = 5;
        }
        else if (shotMode == SHOT_MODE_SELFIE_ALARM) {
            sshotMode = SERIES_SHOT_MODE_SELFIE_ALARM;
            sshotCount = 3;
        } else {
            sshotMode = SERIES_SHOT_MODE_BURST;
            sshotCount = MAX_SERIES_SHOT_COUNT;
        }
    } else if (sshotMode == SERIES_SHOT_MODE_LLS ||
            sshotMode == SERIES_SHOT_MODE_SIS) {
            if(count > 0) {
                sshotCount = count;
            } else {
                sshotCount = 5;
            }
    }

    CLOGD("set shotmode(%d), shotCount(%d)", sshotMode, sshotCount);

    m_cameraInfo.seriesShotMode = sshotMode;
    m_setSeriesShotCount(sshotCount);
}

void ExynosCamera1Parameters::m_setSeriesShotCount(int seriesShotCount)
{
    m_cameraInfo.seriesShotCount = seriesShotCount;
}

int ExynosCamera1Parameters::getSeriesShotCount(void)
{
    return m_cameraInfo.seriesShotCount;
}

void ExynosCamera1Parameters::m_initMetadata(void)
{
    memset(&m_metadata, 0x00, sizeof(struct camera2_shot_ext));
    struct camera2_shot *shot = &m_metadata.shot;

    // 1. ctl
    // request
    shot->ctl.request.id = 0;
    shot->ctl.request.metadataMode = METADATA_MODE_FULL;
    shot->ctl.request.frameCount = 0;

    // lens
    shot->ctl.lens.focusDistance = -1.0f;
    shot->ctl.lens.aperture = (float)m_staticInfo->apertureNum / (float)m_staticInfo->apertureDen;
    shot->ctl.lens.focalLength = (float)m_staticInfo->focalLengthNum / (float)m_staticInfo->focalLengthDen;

    shot->ctl.lens.filterDensity = 0.0f;
    shot->ctl.lens.opticalStabilizationMode = ::OPTICAL_STABILIZATION_MODE_OFF;

    int minFps = (m_staticInfo->minFps == 0) ? 0 : (m_staticInfo->maxFps / 2);
    int maxFps = (m_staticInfo->maxFps == 0) ? 0 : m_staticInfo->maxFps;

    /* The min fps can not be '0'. Therefore it is set up default value '15'. */
    if (minFps == 0) {
        CLOGW(" Invalid min fps value(%d)", minFps);
        minFps = 15;
    }

    /*  The initial fps can not be '0' and bigger than '30'. Therefore it is set up default value '30'. */
    if (maxFps == 0 || 30 < maxFps) {
        CLOGW(" Invalid max fps value(%d)", maxFps);
        maxFps = 30;
    }

    /* sensor */
    shot->ctl.sensor.exposureTime = 0;
    shot->ctl.sensor.frameDuration = (1000 * 1000 * 1000) / maxFps;
    shot->ctl.sensor.sensitivity = 0;

    /* flash */
    shot->ctl.flash.flashMode = ::CAM2_FLASH_MODE_OFF;
    shot->ctl.flash.firingPower = 0;
    shot->ctl.flash.firingTime = 0;

    /* hotpixel */
    shot->ctl.hotpixel.mode = (enum processing_mode)0;

    /* demosaic */
    shot->ctl.demosaic.mode = (enum demosaic_processing_mode)0;

    /* noise */
    shot->ctl.noise.mode = ::PROCESSING_MODE_OFF;
    shot->ctl.noise.strength = 5;

    /* shading */
    shot->ctl.shading.mode = (enum processing_mode)0;

    /* color */
    shot->ctl.color.mode = ::COLORCORRECTION_MODE_FAST;
    static const float colorTransform[9] = {
        1.0f, 0.f, 0.f,
        0.f, 1.f, 0.f,
        0.f, 0.f, 1.f
    };

    memcpy(shot->ctl.color.transform, colorTransform, sizeof(colorTransform));

    /* tonemap */
    shot->ctl.tonemap.mode = ::TONEMAP_MODE_FAST;
    static const float tonemapCurve[4] = {
        0.f, 0.f,
        1.f, 1.f
    };

    int tonemapCurveSize = sizeof(tonemapCurve);
    int sizeOfCurve = sizeof(shot->ctl.tonemap.curveRed) / sizeof(shot->ctl.tonemap.curveRed[0]);

    for (int i = 0; i < sizeOfCurve; i += 4) {
        memcpy(&(shot->ctl.tonemap.curveRed[i]),   tonemapCurve, tonemapCurveSize);
        memcpy(&(shot->ctl.tonemap.curveGreen[i]), tonemapCurve, tonemapCurveSize);
        memcpy(&(shot->ctl.tonemap.curveBlue[i]),  tonemapCurve, tonemapCurveSize);
    }

    /* edge */
    shot->ctl.edge.mode = ::PROCESSING_MODE_OFF;
    shot->ctl.edge.strength = 5;

    /* scaler
     * Max Picture Size == Max Sensor Size - Sensor Margin
     */
    if (m_setParamCropRegion(0,
                             m_staticInfo->maxPictureW, m_staticInfo->maxPictureH,
                             m_staticInfo->maxPreviewW, m_staticInfo->maxPreviewH
                             ) != NO_ERROR) {
        CLOGE("m_setZoom() fail");
    }

    /* jpeg */
    shot->ctl.jpeg.quality = 96;
    shot->ctl.jpeg.thumbnailSize[0] = m_staticInfo->maxThumbnailW;
    shot->ctl.jpeg.thumbnailSize[1] = m_staticInfo->maxThumbnailH;
    shot->ctl.jpeg.thumbnailQuality = 100;
    shot->ctl.jpeg.gpsCoordinates[0] = 0;
    shot->ctl.jpeg.gpsCoordinates[1] = 0;
    shot->ctl.jpeg.gpsCoordinates[2] = 0;
    memset(&shot->ctl.jpeg.gpsProcessingMethod, 0x0,
        sizeof(shot->ctl.jpeg.gpsProcessingMethod));
    shot->ctl.jpeg.gpsTimestamp = 0L;
    shot->ctl.jpeg.orientation = 0L;

    /* stats */
    shot->ctl.stats.faceDetectMode = ::FACEDETECT_MODE_OFF;
    shot->ctl.stats.histogramMode = ::STATS_MODE_OFF;
    shot->ctl.stats.sharpnessMapMode = ::STATS_MODE_OFF;

    /* aa */
    shot->ctl.aa.captureIntent = ::AA_CAPTURE_INTENT_CUSTOM;
    shot->ctl.aa.mode = ::AA_CONTROL_AUTO;
    shot->ctl.aa.effectMode = ::AA_EFFECT_OFF;
    shot->ctl.aa.sceneMode = ::AA_SCENE_MODE_FACE_PRIORITY;
    shot->ctl.aa.videoStabilizationMode = (enum aa_videostabilization_mode)0;

    /* default metering is center */
    shot->ctl.aa.aeMode = ::AA_AEMODE_CENTER;
    shot->ctl.aa.aeRegions[0] = 0;
    shot->ctl.aa.aeRegions[1] = 0;
    shot->ctl.aa.aeRegions[2] = 0;
    shot->ctl.aa.aeRegions[3] = 0;
    shot->ctl.aa.aeRegions[4] = 1000;
    shot->ctl.aa.aeLock = ::AA_AE_LOCK_OFF;
#if defined(USE_SUBDIVIDED_EV)
    shot->ctl.aa.aeExpCompensation = 0; /* 21 is middle */
#else
    shot->ctl.aa.aeExpCompensation = 5; /* 5 is middle */
#endif
    shot->ctl.aa.aeTargetFpsRange[0] = minFps;
    shot->ctl.aa.aeTargetFpsRange[1] = maxFps;

    shot->ctl.aa.aeAntibandingMode = ::AA_AE_ANTIBANDING_AUTO;
    shot->ctl.aa.vendor_aeflashMode = ::AA_FLASHMODE_OFF;
    shot->ctl.aa.awbMode = ::AA_AWBMODE_WB_AUTO;
    shot->ctl.aa.awbLock = ::AA_AWB_LOCK_OFF;
    shot->ctl.aa.afMode = ::AA_AFMODE_OFF;
    shot->ctl.aa.afRegions[0] = 0;
    shot->ctl.aa.afRegions[1] = 0;
    shot->ctl.aa.afRegions[2] = 0;
    shot->ctl.aa.afRegions[3] = 0;
    shot->ctl.aa.afRegions[4] = 1000;
    shot->ctl.aa.afTrigger = (enum aa_af_trigger)0;
    shot->ctl.aa.vendor_isoMode = AA_ISOMODE_AUTO;
    shot->ctl.aa.vendor_isoValue = 0;

    /* 2. dm */

    /* 3. uctl */
#ifdef USE_FW_ZOOMRATIO
    shot->uctl.zoomRatio = 1.00f;
#else
    shot->uctl.zoomRatio = 0;
#endif

    /* 4. udm */

    /* 5. magicNumber */
    shot->magicNumber = SHOT_MAGIC_NUMBER;

    setMetaSetfile(&m_metadata, 0x0);

    /* user request */
    m_metadata.drc_bypass = 1;
    m_metadata.dis_bypass = 1;
    m_metadata.dnr_bypass = 1;
    m_metadata.fd_bypass  = 1;
}

status_t ExynosCamera1Parameters::duplicateCtrlMetadata(void *buf)
{
    if (buf == NULL) {
        CLOGE("ERR: buf is NULL");
        return BAD_VALUE;
    }

    struct camera2_shot_ext *meta_shot_ext = (struct camera2_shot_ext *)buf;
    memcpy(&meta_shot_ext->shot.ctl, &m_metadata.shot.ctl, sizeof(struct camera2_ctl));

#ifdef SUPPORT_DEPTH_MAP
    meta_shot_ext->shot.uctl.companionUd.disparity_mode = m_metadata.shot.uctl.companionUd.disparity_mode;
#endif

#ifdef USE_FW_ZOOMRATIO
    if(getCameraId() == CAMERA_ID_BACK) {
        meta_shot_ext->shot.uctl.zoomRatio = m_metadata.shot.uctl.zoomRatio;
    }
#endif // USE_FW_ZOOMRATIO
    meta_shot_ext->shot.uctl.vtMode = m_metadata.shot.uctl.vtMode;

#ifdef USE_FW_ZOOMRATIO
    if(getCameraId() == CAMERA_ID_BACK) {
        meta_shot_ext->shot.uctl.zoomRatio = m_metadata.shot.uctl.zoomRatio;
    }
#endif // USE_FW_ZOOMRATIO

#ifdef USE_FW_FLASHMODE
    meta_shot_ext->shot.uctl.flashMode = m_metadata.shot.uctl.flashMode;
#endif
#ifdef USE_FW_OPMODE
    meta_shot_ext->shot.uctl.opMode = m_metadata.shot.uctl.opMode;
#endif

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setDualShotMode(const CameraParameters& params)
{
    const char *strParamSceneMode = params.get(CameraParameters::KEY_SCENE_MODE);
    int newShotMode = params.getInt("shot-mode");

    if (strParamSceneMode == NULL) {
        CLOGD("strParamSceneMode is NULL.");
        return;
    }

    if (!strcmp(strParamSceneMode, CameraParameters::SCENE_MODE_AUTO) &&
        newShotMode ==  SHOT_MODE_NORMAL) {
        enum aa_mode mode = AA_CONTROL_USE_SCENE_MODE;
        enum aa_scene_mode sceneMode = AA_SCENE_MODE_DUAL;

        setMetaCtlSceneMode(&m_metadata, mode, sceneMode);
    } else {
        CLOGW("must to check shot mode(%d) and scene modee(%s).",
             newShotMode, strParamSceneMode);
    }
}

void ExynosCamera1Parameters::setMotionPhotoOn(bool enable)
{
    m_flagMotionPhotoOn = enable;
}

bool ExynosCamera1Parameters::getMotionPhotoOn()
{
    return m_flagMotionPhotoOn;
}

#ifdef USE_ODC_CAPTURE
status_t ExynosCamera1Parameters::checkODCCaptureMode()
{
    if(getDualMode() == true) {//PIP mode
        setODCCaptureMode(USE_ODC_CAPTURE_DUAL);
    }
    else {//Single camera mode
#ifdef USE_ODC_CAPTURE_VIDEO_SNAPSHOT
        if (getRecordingHint() == true) {
            setODCCaptureMode(USE_ODC_CAPTURE_VIDEO_SNAPSHOT);
        } else
#endif
        {
            setODCCaptureMode(USE_ODC_CAPTURE_SINGLE);
        }
    }

    setODCCaptureEnable(getODCCaptureMode());

    return NO_ERROR;
}

void ExynosCamera1Parameters::setODCCaptureMode(bool enable)
{
    m_flagODCCaptureMode = enable;
}

bool ExynosCamera1Parameters::getODCCaptureMode(void)
{
    return m_flagODCCaptureMode;
}

void ExynosCamera1Parameters::setODCCaptureEnable(bool enable)
{
    m_flagODCCaptureEnable = enable;
}

bool ExynosCamera1Parameters::getODCCaptureEnable(void)
{
    return m_flagODCCaptureEnable;
}
#endif

void ExynosCamera1Parameters::setOutPutFormatNV21Enable(bool enable)
{
    m_flagOutPutFormatNV21Enable = enable;
}

bool ExynosCamera1Parameters::getOutPutFormatNV21Enable(void)
{
    return m_flagOutPutFormatNV21Enable;
}

void ExynosCamera1Parameters::m_getSetfileYuvRange(bool flagReprocessing, int *setfile, int *yuvRange)
{
    uint32_t currentSetfile = 0;
    uint32_t currentScenario = 0;
    uint32_t stateReg = 0;
    int flagYUVRange = YUV_FULL_RANGE;

    unsigned int minFps = 0;
    unsigned int maxFps = 0;
    getPreviewFpsRange(&minFps, &maxFps);

/* This code is for FHD 60fps performance issue in Grace */
    if (m_isUHDRecordingMode() == true)
        stateReg |= STATE_REG_UHD_RECORDING;

    if (getDualMode() == true) {
        stateReg |= STATE_REG_DUAL_MODE;
        if (getDualRecordingHint() == true)
            stateReg |= STATE_REG_DUAL_RECORDINGHINT;
    } else {
        if (getRecordingHint() == true)
            stateReg |= STATE_REG_RECORDINGHINT;
    }

    if (flagReprocessing == true) {
        stateReg |= STATE_REG_FLAG_REPROCESSING;

        if (getLongExposureShotCount() > 0)
            stateReg |= STATE_STILL_CAPTURE_LONG;

#ifdef USE_BINNING_MODE
    if (flagReprocessing == false && getBinningMode() == true
    ) {
        stateReg |= STATE_REG_BINNING_MODE;
    }
#endif

    } else if (flagReprocessing == false) {
        if (stateReg & STATE_REG_RECORDINGHINT
            || stateReg & STATE_REG_UHD_RECORDING
            || stateReg & STATE_REG_DUAL_RECORDINGHINT)
            flagYUVRange = YUV_LIMITED_RANGE;
    }

    if (m_cameraId == CAMERA_ID_FRONT) {
        int vtMode = getVtMode();

        if (vtMode > 0) {
            switch (vtMode) {
            case 1:
                currentSetfile = ISS_SUB_SCENARIO_FRONT_VT1;
                if (stateReg == STATE_STILL_CAPTURE)
                    currentSetfile = ISS_SUB_SCENARIO_FRONT_VT1_STILL_CAPTURE;
                break;
            case 2:
                currentSetfile = ISS_SUB_SCENARIO_FRONT_VT2;
                break;
            case 4:
                currentSetfile = ISS_SUB_SCENARIO_FRONT_VT4;
                break;
            default:
                currentSetfile = ISS_SUB_SCENARIO_FRONT_VT2;
                break;
            }
        } else if (getIntelligentMode() == 1) {
            currentSetfile = ISS_SUB_SCENARIO_FRONT_SMART_STAY;
        } else if (getShotMode() == SHOT_MODE_FRONT_PANORAMA || getShotMode() == SHOT_MODE_INTERACTIVE) {
            currentSetfile = ISS_SUB_SCENARIO_FRONT_PANORAMA;
        } else {
            switch(stateReg) {
                case STATE_STILL_PREVIEW:
                        currentSetfile = ISS_SUB_SCENARIO_STILL_PREVIEW;
                    break;

                case STATE_STILL_PREVIEW_WDR_ON:
                    currentSetfile = ISS_SUB_SCENARIO_STILL_PREVIEW_WDR_ON;
                    break;

                case STATE_STILL_PREVIEW_WDR_AUTO:
                    currentSetfile = ISS_SUB_SCENARIO_STILL_PREVIEW_WDR_AUTO;
                    break;

                case STATE_VIDEO:
                        currentSetfile = ISS_SUB_SCENARIO_VIDEO;
                    break;

                case STATE_VIDEO_WDR_ON:
                case STATE_UHD_VIDEO_WDR_ON:
                    currentSetfile = ISS_SUB_SCENARIO_VIDEO_WDR_ON;
                    break;

                case STATE_VIDEO_WDR_AUTO:
                case STATE_UHD_VIDEO_WDR_AUTO:
                    currentSetfile = ISS_SUB_SCENARIO_VIDEO_WDR_AUTO;
                    break;

                case STATE_STILL_CAPTURE:
                case STATE_VIDEO_CAPTURE:
                case STATE_UHD_PREVIEW_CAPTURE:
                case STATE_UHD_VIDEO_CAPTURE:
                        currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE;
                    break;

                case STATE_STILL_CAPTURE_WDR_ON:
                case STATE_VIDEO_CAPTURE_WDR_ON:
                case STATE_UHD_PREVIEW_CAPTURE_WDR_ON:
                case STATE_UHD_VIDEO_CAPTURE_WDR_ON:
                    currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_WDR_ON;
                    break;

                case STATE_STILL_CAPTURE_WDR_AUTO:
                case STATE_VIDEO_CAPTURE_WDR_AUTO:
                case STATE_UHD_PREVIEW_CAPTURE_WDR_AUTO:
                case STATE_UHD_VIDEO_CAPTURE_WDR_AUTO:
                    currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_WDR_AUTO;
                    break;

                case STATE_DUAL_STILL_PREVIEW:
                case STATE_DUAL_STILL_CAPTURE:
                case STATE_DUAL_VIDEO_CAPTURE:
                    currentSetfile = ISS_SUB_SCENARIO_DUAL_STILL;
                    break;

                case STATE_DUAL_VIDEO:
                    currentSetfile = ISS_SUB_SCENARIO_DUAL_VIDEO;
                    break;

                case STATE_UHD_PREVIEW:
                case STATE_UHD_VIDEO:
                    currentSetfile = ISS_SUB_SCENARIO_UHD_30FPS;
                    break;

#if defined(USE_BINNING_MODE) && defined(SET_PREVIEW_BINNING_SETFILE)
                case STATE_STILL_BINNING_PREVIEW:
                case STATE_VIDEO_BINNING:
                case STATE_DUAL_VIDEO_BINNING:
                case STATE_DUAL_STILL_BINING_PREVIEW:
                    currentSetfile = ISS_SUB_SCENARIO_FRONT_STILL_PREVIEW_BINNING;
                    break;
#endif /* USE_BINNING_MODE && SET_PREVIEW_BINNING_SETFILE */
                default:
                    CLOGD("can't define senario of setfile.(0x%4x)", stateReg);
                    break;
            }
        }
    } else {
        switch(stateReg) {
            case STATE_STILL_PREVIEW:
                currentSetfile = ISS_SUB_SCENARIO_STILL_PREVIEW;
                break;

            case STATE_STILL_PREVIEW_WDR_ON:
                currentSetfile = ISS_SUB_SCENARIO_STILL_PREVIEW_WDR_ON;
                break;

            case STATE_STILL_PREVIEW_WDR_AUTO:
                currentSetfile = ISS_SUB_SCENARIO_STILL_PREVIEW_WDR_AUTO;
                break;

            case STATE_STILL_CAPTURE:
            case STATE_VIDEO_CAPTURE:
            case STATE_DUAL_STILL_CAPTURE:
            case STATE_DUAL_VIDEO_CAPTURE:
            case STATE_UHD_PREVIEW_CAPTURE:
            case STATE_UHD_VIDEO_CAPTURE:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE;
                break;

            case STATE_STILL_CAPTURE_WDR_ON:
            case STATE_VIDEO_CAPTURE_WDR_ON:
            case STATE_UHD_PREVIEW_CAPTURE_WDR_ON:
            case STATE_UHD_VIDEO_CAPTURE_WDR_ON:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_WDR_ON;
                break;

            case STATE_STILL_CAPTURE_WDR_AUTO:
            case STATE_VIDEO_CAPTURE_WDR_AUTO:
            case STATE_UHD_PREVIEW_CAPTURE_WDR_AUTO:
            case STATE_UHD_VIDEO_CAPTURE_WDR_AUTO:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_WDR_AUTO;
                break;

            case STATE_STILL_BINNING_PREVIEW:
            case STATE_VIDEO_BINNING:
            case STATE_DUAL_VIDEO_BINNING:
            case STATE_DUAL_STILL_BINING_PREVIEW:
#if defined(USE_BINNING_MODE) && defined(SET_PREVIEW_BINNING_SETFILE)
                currentSetfile = ISS_SUB_SCENARIO_STILL_PREVIEW_BINNING;
#else
                currentSetfile = ISS_SUB_SCENARIO_FHD_60FPS;
#endif
                break;

            case STATE_VIDEO:
                if (30 < minFps && 30 < maxFps) {
                    if (300 == minFps && 300 == maxFps) {
                        currentSetfile = ISS_SUB_SCENARIO_WVGA_300FPS;
                    } else if (60 == minFps && 60 == maxFps) {
                        currentSetfile = ISS_SUB_SCENARIO_FHD_60FPS;
                    } else if (240 == minFps && 240 == maxFps) {
                        currentSetfile = ISS_SUB_SCENARIO_FHD_240FPS;
                    } else {
                        currentSetfile = ISS_SUB_SCENARIO_VIDEO_HIGH_SPEED;
                    }
                } else {
                    currentSetfile = ISS_SUB_SCENARIO_VIDEO;
                }
                break;

            case STATE_VIDEO_WDR_ON:
                currentSetfile = ISS_SUB_SCENARIO_VIDEO_WDR_ON;
                break;

            case STATE_VIDEO_WDR_AUTO:
                if (30 < minFps && 30 < maxFps) {
                    if (300 == minFps && 300 == maxFps) {
                        currentSetfile = ISS_SUB_SCENARIO_WVGA_300FPS;
                    } else if (60 == minFps && 60 == maxFps) {
                        currentSetfile = ISS_SUB_SCENARIO_FHD_60FPS;
                    } else if (240 == minFps && 240 == maxFps) {
                        currentSetfile = ISS_SUB_SCENARIO_FHD_240FPS;
                    } else {
                        currentSetfile = ISS_SUB_SCENARIO_VIDEO_HIGH_SPEED;
                    }
                } else {
                    currentSetfile = ISS_SUB_SCENARIO_VIDEO_WDR_AUTO;
                }
                break;

            case STATE_DUAL_VIDEO:
                currentSetfile = ISS_SUB_SCENARIO_DUAL_VIDEO;
                break;

            case STATE_DUAL_STILL_PREVIEW:
                currentSetfile = ISS_SUB_SCENARIO_DUAL_STILL;
                break;

            case STATE_UHD_PREVIEW:
            case STATE_UHD_VIDEO:
            case STATE_UHD_VIDEO_WDR_AUTO:
                currentSetfile = ISS_SUB_SCENARIO_UHD_30FPS;
                break;

            case STATE_UHD_PREVIEW_WDR_ON:
            case STATE_UHD_VIDEO_WDR_ON:
                currentSetfile = ISS_SUB_SCENARIO_UHD_30FPS_WDR_ON;
                break;

            case STATE_STILL_CAPTURE_WDR_AUTO_SHARPEN:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_SHARPEN;
                break;
            case STATE_STILL_CAPTURE_LONG:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_LONG;
                break;
            case STATE_STILL_CAPTURE_MANUAL_ISO:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_MANUAL_ISO;
                break;
            default:
                CLOGD("can't define senario of setfile.(0x%4x)", stateReg);
                break;
        }
    }
#if 0
    CLOGD("===============================================================================");
    CLOGD("CurrentState(0x%4x)", stateReg);
    CLOGD("getRTHdr()(%d)", getRTHdr());
    CLOGD("getRecordingHint()(%d)", getRecordingHint());
    CLOGD("m_isUHDRecordingMode()(%d)", m_isUHDRecordingMode());
    CLOGD("getDualMode()(%d)", getDualMode());
    CLOGD("getDualRecordingHint()(%d)", getDualRecordingHint());
#ifdef USE_BINNING_MODE
    CLOGD("getBinningMode(%d)", getBinningMode());
#endif
    CLOGD("flagReprocessing(%d)", flagReprocessing);
    CLOGD("===============================================================================");
    CLOGD("currentSetfile(%d)", currentSetfile);
    CLOGD("flagYUVRange(%d)", flagYUVRange);
    CLOGD("===============================================================================");
#else
    CLOGD("CurrentState (0x%4x), currentSetfile(%d)", stateReg, currentSetfile);
#endif

done:
    *setfile = currentSetfile | (currentScenario << 16);
    *yuvRange = flagYUVRange;
}

status_t ExynosCamera1Parameters::calcPreviewGSCRect(ExynosRect *srcRect, ExynosRect *dstRect)
{
    int ret = 0;

    int cropX = 0, cropY = 0;
    int cropW = 0, cropH = 0;
    int crop_crop_x = 0, crop_crop_y = 0;
    int crop_crop_w = 0, crop_crop_h = 0;

    int previewW = 0, previewH = 0, previewFormat = 0;
    int hwPreviewW = 0, hwPreviewH = 0, hwPreviewFormat = 0;
    previewFormat = getPreviewFormat();
    hwPreviewFormat = getHwPreviewFormat();

    getHwPreviewSize(&hwPreviewW, &hwPreviewH);
    getPreviewSize(&previewW, &previewH);

    ret = getCropRectAlign(hwPreviewW, hwPreviewH,
                           previewW, previewH,
                           &cropX, &cropY,
                           &cropW, &cropH,
                           GSCALER_IMG_ALIGN, 2,
                           0, 1);

    srcRect->x = cropX;
    srcRect->y = cropY;
    srcRect->w = cropW;
    srcRect->h = cropH;

#ifdef GRALLOC_CAMERA_64BYTE_ALIGN
    srcRect->fullW = ALIGN_UP(cropW, CAMERA_64PX_ALIGN);
#else
    srcRect->fullW = cropW;
#endif
    srcRect->fullH = cropH;
    srcRect->colorFormat = hwPreviewFormat;

    dstRect->x = 0;
    dstRect->y = 0;
    dstRect->w = previewW;
    dstRect->h = previewH;
    dstRect->fullW = previewW;
    dstRect->fullH = previewH;
    dstRect->colorFormat = previewFormat;

    return NO_ERROR;
}

status_t ExynosCamera1Parameters::calcRecordingGSCRect(ExynosRect *srcRect, ExynosRect *dstRect)
{
    int ret = 0;

    int cropX = 0, cropY = 0;
    int cropW = 0, cropH = 0;

    int hwPreviewW = 0, hwPreviewH = 0, hwPreviewFormat = 0;
    int videoW = 0, videoH = 0, videoFormat = 0;
    float zoomRatio = getZoomRatio(0) / 1000;

    hwPreviewFormat = getHwPreviewFormat();
    videoFormat     = getVideoFormat();

    getHwPreviewSize(&hwPreviewW, &hwPreviewH);

    getVideoSize(&videoW, &videoH);

    if (hwPreviewW < videoW || hwPreviewH < videoH) {
        cropW = hwPreviewW;
        cropH = hwPreviewH;
    } else  {
        ret = getCropRectAlign(hwPreviewW, hwPreviewH,
                         videoW, videoH,
                         &cropX, &cropY,
                         &cropW, &cropH,
                         2, 2,
                         0, zoomRatio);
    }

    srcRect->x = cropX;
    srcRect->y = cropY;
    srcRect->w = cropW;
    srcRect->h = cropH;

    /*
     * buffer is alloc as hwPreviewStride size.
     * so, we must set fullW as buffer size.
     * without this, it can have zagging image.
     */
    int hwPreviewStride = getHwPreviewStride();
    if (hwPreviewW < hwPreviewStride) {
        CLOGW("hwPreviewW(%d) < hwPreviewStride(%d). so, adjust hwPreviewStride to hwPreviewW",
            hwPreviewW, hwPreviewStride);
        hwPreviewW = hwPreviewStride;
    }

    srcRect->fullW = hwPreviewW;
    srcRect->fullH = hwPreviewH;
    srcRect->colorFormat = hwPreviewFormat;

    dstRect->x = 0;
    dstRect->y = 0;
    dstRect->w = videoW;
    dstRect->h = videoH;
    dstRect->fullW = videoW;
    dstRect->fullH = videoH;
    dstRect->colorFormat = videoFormat;

    return NO_ERROR;
}

status_t ExynosCamera1Parameters::calcPreviewBayerCropSize(ExynosRect *srcRect, ExynosRect *dstRect)
{
    int ret = 0;

    int hwSensorW = 0, hwSensorH = 0;
    int hwPictureW = 0, hwPictureH = 0;
    int pictureW = 0, pictureH = 0;
    int previewW = 0, previewH = 0;
    int hwSensorMarginW = 0, hwSensorMarginH = 0;

    int cropX = 0, cropY = 0;
    int cropW = 0, cropH = 0;
#if 0
    int crop_crop_x = 0, crop_crop_y = 0;
    int crop_crop_w = 0, crop_crop_h = 0;
    int pictureFormat = 0, hwPictureFormat = 0;
#endif
    int zoomLevel = 0;
    int maxZoomRatio = 0;
    int bayerFormat = getBayerFormat(PIPE_3AA);
    float zoomRatio = getZoomRatio(0) / 1000;

#ifdef DEBUG_RAWDUMP
    if (checkBayerDumpEnable()) {
        bayerFormat = CAMERA_DUMP_BAYER_FORMAT;
    }
#endif

    /* TODO: check state ready for start */
#if 0
    pictureFormat = getHwPictureFormat();
#endif

    if (isUse3aaInputCrop() == true) {
        zoomLevel = getZoomLevel();
        zoomRatio = getZoomRatio(zoomLevel) / 1000;
    }

    maxZoomRatio = getMaxZoomRatio() / 1000;
    getHwPictureSize(&hwPictureW, &hwPictureH);
    getPictureSize(&pictureW, &pictureH);

    getHwSensorSize(&hwSensorW, &hwSensorH);
    getPreviewSize(&previewW, &previewH);
    getSensorMargin(&hwSensorMarginW, &hwSensorMarginH);

    hwSensorW -= hwSensorMarginW;
    hwSensorH -= hwSensorMarginH;

    ret = getCropRectAlign(hwSensorW, hwSensorH,
                     previewW, previewH,
                     &cropX, &cropY,
                     &cropW, &cropH,
                     CAMERA_BCROP_ALIGN, 2,
                     zoomLevel, zoomRatio);

    cropX = ALIGN_DOWN(cropX, 2);
    cropY = ALIGN_DOWN(cropY, 2);
    cropW = hwSensorW - (cropX * 2);
    cropH = hwSensorH - (cropY * 2);

    if (getReprocessingMode() != PROCESSING_MODE_REPROCESSING_PURE_BAYER) {
        int pictureCropX = 0, pictureCropY = 0;
        int pictureCropW = 0, pictureCropH = 0;

        zoomLevel = 0;
        zoomRatio = getZoomRatio(zoomLevel) / 1000;

        ret = getCropRectAlign(cropW, cropH,
                pictureW, pictureH,
                &pictureCropX, &pictureCropY,
                &pictureCropW, &pictureCropH,
                CAMERA_BCROP_ALIGN, 2,
                zoomLevel, zoomRatio);

        pictureCropX = ALIGN_DOWN(pictureCropX, 2);
        pictureCropY = ALIGN_DOWN(pictureCropY, 2);
        pictureCropW = cropW - (pictureCropX * 2);
        pictureCropH = cropH - (pictureCropY * 2);

        if (pictureCropW < pictureW / maxZoomRatio || pictureCropH < pictureH / maxZoomRatio) {
            CLOGW(" zoom ratio is upto x%d, crop(%dx%d), picture(%dx%d)", maxZoomRatio, cropW, cropH, pictureW, pictureH);
            float src_ratio = 1.0f;
            float dst_ratio = 1.0f;
            /* ex : 1024 / 768 */
            src_ratio = ROUND_OFF_HALF(((float)cropW / (float)cropH), 2);
            /* ex : 352  / 288 */
            dst_ratio = ROUND_OFF_HALF(((float)pictureW / (float)pictureH), 2);

            if (dst_ratio <= src_ratio) {
                /* shrink w */
                cropX = ALIGN_DOWN(((int)(hwSensorW - ((pictureH / maxZoomRatio) * src_ratio)) >> 1), 2);
                cropY = ALIGN_DOWN(((hwSensorH - (pictureH / maxZoomRatio)) >> 1), 2);
            } else {
                /* shrink h */
                cropX = ALIGN_DOWN(((hwSensorW - (pictureW / maxZoomRatio)) >> 1), 2);
                cropY = ALIGN_DOWN(((int)(hwSensorH - ((pictureW / maxZoomRatio) / src_ratio)) >> 1), 2);
            }
            cropW = hwSensorW - (cropX * 2);
            cropH = hwSensorH - (cropY * 2);
        }
    } else {
#ifdef USE_CP_FUSION_LIB
#ifdef USE_CP_FUSION_LIB_CROP_IN_LIBRARY
#else
#ifdef USE_CP_FUSION_LIB_LIMIT_CROP_ON_ISP
        if (this->isFusionEnabled() == true) {
            /*
             * when preview fusion,
             * bayer crop size need to be bigger than fusion input size.
             * for warping between wide/tele
             */
            int previewW = 0;
            int previewH = 0;

            this->getPreviewSize(&previewW, &previewH);

            ExynosRect fusionSrcRect;
            ExynosRect fusionDstRect;

            if (this->getFusionSize(previewW, previewH, &fusionSrcRect, &fusionDstRect) != NO_ERROR) {
                CLOGE("getFusionSize(%d, %d) fail", previewW, previewH);
                return INVALID_OPERATION;
            }

            if (cropW < fusionSrcRect.w) {
                CLOGV("calibrate bcrop width size (%d -> %d)",  cropW, fusionSrcRect.w);
                cropW = fusionSrcRect.w;
                cropX = ALIGN_DOWN(((hwSensorW - cropW) >> 1), 2);
            }

            if (cropH < fusionSrcRect.h) {
                CLOGV("calibrate bcrop height size (%d -> %d)", cropH, fusionSrcRect.h);
                cropH = fusionSrcRect.h;
                cropY = ALIGN_DOWN(((hwSensorH - cropH) >> 1), 2);
            }
        }
#endif // USE_CP_FUSION_LIB_LIMIT_CROP_ON_ISP
#endif // USE_CP_FUSION_LIB_CROP_IN_LIBRARY
#endif // USE_CP_FUSION_LIB
    }

#if 0
    CLOGD("hwSensorSize (%dx%d), previewSize (%dx%d)",
            hwSensorW, hwSensorH, previewW, previewH);
    CLOGD("hwPictureSize (%dx%d), pictureSize (%dx%d)",
            hwPictureW, hwPictureH, pictureW, pictureH);
    CLOGD("size cropX = %d, cropY = %d, cropW = %d, cropH = %d, zoom = %d",
            cropX, cropY, cropW, cropH, zoomLevel);
    CLOGD("size2 cropX = %d, cropY = %d, cropW = %d, cropH = %d, zoom = %d",
            crop_crop_x, crop_crop_y, crop_crop_w, crop_crop_h, zoomLevel);
    CLOGD("size pictureFormat = 0x%x, JPEG_INPUT_COLOR_FMT = 0x%x",
            pictureFormat, JPEG_INPUT_COLOR_FMT);
#endif

    srcRect->x = 0;
    srcRect->y = 0;
    srcRect->w = hwSensorW;
    srcRect->h = hwSensorH;
    srcRect->fullW = hwSensorW;
    srcRect->fullH = hwSensorH;
    srcRect->colorFormat = bayerFormat;

    dstRect->x = cropX;
    dstRect->y = cropY;
    dstRect->w = cropW;
    dstRect->h = cropH;
    dstRect->fullW = cropW;
    dstRect->fullH = cropH;
    dstRect->colorFormat = bayerFormat;

    m_setHwBayerCropRegion(dstRect->w, dstRect->h, dstRect->x, dstRect->y);
    return NO_ERROR;
}

status_t ExynosCamera1Parameters::getPictureBdsSize(ExynosRect *dstRect)
{
    status_t ret = NO_ERROR;
    ExynosRect bnsSize;
    ExynosRect bayerCropSize;
    int hwBdsW = 0;
    int hwBdsH = 0;

    /* matched ratio LUT is not existed, use equation */
    if (m_useSizeTable == false
        || m_staticInfo->pictureSizeLut == NULL
        || m_staticInfo->pictureSizeLutMax <= m_cameraInfo.pictureSizeRatioId) {
        ExynosRect rect;
        return calcPictureBDSSize(&rect, dstRect);
    }

#ifdef USE_BINNING_MODE
    if (getBinningMode() == true) {
        /* use LUT */
        hwBdsW = m_staticInfo->vtcallSizeLut[m_cameraInfo.pictureSizeRatioId][BDS_W];
        hwBdsH = m_staticInfo->vtcallSizeLut[m_cameraInfo.pictureSizeRatioId][BDS_H];
    } else
#endif
    {
        /* use LUT */
        hwBdsW = m_staticInfo->pictureSizeLut[m_cameraInfo.pictureSizeRatioId][BDS_W];
        hwBdsH = m_staticInfo->pictureSizeLut[m_cameraInfo.pictureSizeRatioId][BDS_H];
    }
    /* Check the invalid BDS size compared to Bcrop size */
    ret = getPictureBayerCropSize(&bnsSize, &bayerCropSize);
    if (ret != NO_ERROR)
        CLOGE("getPictureBayerCropSize() failed");

    if (bayerCropSize.w < hwBdsW || bayerCropSize.h < hwBdsH) {
        CLOGD("bayerCropSize %dx%d is smaller than BDSSize %dx%d. Force bayerCropSize",
                bayerCropSize.w, bayerCropSize.h, hwBdsW, hwBdsH);

        hwBdsW = bayerCropSize.w;
        hwBdsH = bayerCropSize.h;
    }

    dstRect->x = 0;
    dstRect->y = 0;
    dstRect->w = hwBdsW;
    dstRect->h = hwBdsH;

#ifdef DEBUG_PERFRAME
    CLOGD("hwBdsSize %dx%d", dstRect->w, dstRect->h);
#endif

    return NO_ERROR;
}

status_t ExynosCamera1Parameters::getPictureBayerCropSize(ExynosRect *srcRect, ExynosRect *dstRect)
{
    int hwBnsW   = 0;
    int hwBnsH   = 0;
    int hwBcropW = 0;
    int hwBcropH = 0;
    int zoomLevel = 0;
    float zoomRatio = 1.00f;
    int hwSensorMarginW = 0;
    int hwSensorMarginH = 0;

    /* matched ratio LUT is not existed, use equation */
    if (m_useSizeTable == false
        || m_staticInfo->pictureSizeLut == NULL
        || m_staticInfo->pictureSizeLutMax <= m_cameraInfo.pictureSizeRatioId
        || m_cameraInfo.pictureSizeRatioId != m_cameraInfo.previewSizeRatioId)
        return calcPictureBayerCropSize(srcRect, dstRect);

#ifdef USE_BINNING_MODE
    if(getBinningMode() == true) {
        /* use LUT */
        hwBnsW   = m_staticInfo->vtcallSizeLut[m_cameraInfo.pictureSizeRatioId][BNS_W];
        hwBnsH   = m_staticInfo->vtcallSizeLut[m_cameraInfo.pictureSizeRatioId][BNS_H];
        hwBcropW = m_staticInfo->vtcallSizeLut[m_cameraInfo.pictureSizeRatioId][BCROP_W];
        hwBcropH = m_staticInfo->vtcallSizeLut[m_cameraInfo.pictureSizeRatioId][BCROP_H];
    } else
#endif
    {
        /* use LUT */
        hwBnsW   = m_staticInfo->pictureSizeLut[m_cameraInfo.pictureSizeRatioId][BNS_W];
        hwBnsH   = m_staticInfo->pictureSizeLut[m_cameraInfo.pictureSizeRatioId][BNS_H];
        hwBcropW = m_staticInfo->pictureSizeLut[m_cameraInfo.pictureSizeRatioId][BCROP_W];
        hwBcropH = m_staticInfo->pictureSizeLut[m_cameraInfo.pictureSizeRatioId][BCROP_H];
    }
    if (isUseReprocessing3aaInputCrop() == true) {
        {
            zoomLevel = getZoomLevel();
        }
        zoomRatio = getZoomRatio(zoomLevel) / 1000;
    }

#if defined(SCALER_MAX_SCALE_UP_RATIO)
    /*
     * After dividing float & casting int,
     * zoomed size can be smaller too much.
     * so, when zoom until max, ceil up about floating point.
     */
    if (ALIGN_UP((int)((float)hwBcropW / zoomRatio), CAMERA_BCROP_ALIGN) * SCALER_MAX_SCALE_UP_RATIO < hwBcropW ||
        ALIGN_UP((int)((float)hwBcropH / zoomRatio), 2) * SCALER_MAX_SCALE_UP_RATIO < hwBcropH) {
        hwBcropW = ALIGN_UP((int)ceil((float)hwBcropW / zoomRatio), CAMERA_BCROP_ALIGN);
        hwBcropH = ALIGN_UP((int)ceil((float)hwBcropH / zoomRatio), 2);
    } else
#endif
    {
        hwBcropW = ALIGN_UP((int)((float)hwBcropW / zoomRatio), CAMERA_BCROP_ALIGN);
        hwBcropH = ALIGN_UP((int)((float)hwBcropH / zoomRatio), 2);
    }

    /* Re-calculate the BNS size for removing Sensor Margin.
       On Capture Stream(3AA_M2M_Input), the BNS is not used.
       So, the BNS ratio is not needed to be considered for sensor margin */
    getSensorMargin(&hwSensorMarginW, &hwSensorMarginH);
    hwBnsW = hwBnsW - hwSensorMarginW;
    hwBnsH = hwBnsH - hwSensorMarginH;

    /* src */
    srcRect->x = 0;
    srcRect->y = 0;
    srcRect->w = hwBnsW;
    srcRect->h = hwBnsH;

    /* dst */
    if (hwBnsW > hwBcropW) {
        dstRect->x = ALIGN_UP(((hwBnsW - hwBcropW) >> 1), 2);
        dstRect->w = hwBcropW;
    } else {
        dstRect->x = 0;
        dstRect->w = hwBnsW;
    }

    if (hwBnsH > hwBcropH) {
        dstRect->y = ALIGN_UP(((hwBnsH - hwBcropH) >> 1), 2);
        dstRect->h = hwBcropH;
    } else {
        dstRect->y = 0;
        dstRect->h = hwBnsH;
    }


#if DEBUG
    CLOGE("hwBnsSize %dx%d, hwBcropSize %d,%d %dx%d zoomLevel %d zoomRatio %d",
            srcRect->w, srcRect->h,
            dstRect->x, dstRect->y, dstRect->w, dstRect->h,
            zoomLevel, zoomRatio);
#endif

    return NO_ERROR;
}

status_t ExynosCamera1Parameters::calcPictureBayerCropSize(ExynosRect *srcRect, ExynosRect *dstRect)
{
    int ret = 0;

    int maxSensorW = 0, maxSensorH = 0;
    int hwSensorW = 0, hwSensorH = 0;
    int hwPictureW = 0, hwPictureH = 0, hwPictureFormat = 0;
    int hwSensorCropX = 0, hwSensorCropY = 0;
    int hwSensorCropW = 0, hwSensorCropH = 0;
    int pictureW = 0, pictureH = 0, pictureFormat = 0;
    int previewW = 0, previewH = 0;
    int hwSensorMarginW = 0, hwSensorMarginH = 0;

    int cropX = 0, cropY = 0;
    int cropW = 0, cropH = 0;
    int crop_crop_x = 0, crop_crop_y = 0;
    int crop_crop_w = 0, crop_crop_h = 0;

    int zoomLevel = 0;
    float zoomRatio = 1.0f;
    int maxZoomRatio = 0;
    int bayerFormat = getBayerFormat(PIPE_3AA_REPROCESSING);

#ifdef DEBUG_RAWDUMP
    if (checkBayerDumpEnable()) {
        bayerFormat = CAMERA_DUMP_BAYER_FORMAT;
    }
#endif

    /* TODO: check state ready for start */
    if (isUseReprocessing3aaInputCrop() == true) {
        zoomLevel = getZoomLevel();
        zoomRatio = getZoomRatio(zoomLevel) / 1000;
    }

    pictureFormat = getHwPictureFormat();
    maxZoomRatio = getMaxZoomRatio() / 1000;
    getHwPictureSize(&hwPictureW, &hwPictureH);
    getPictureSize(&pictureW, &pictureH);

    getMaxSensorSize(&maxSensorW, &maxSensorH);
    getHwSensorSize(&hwSensorW, &hwSensorH);
    getPreviewSize(&previewW, &previewH);
    getSensorMargin(&hwSensorMarginW, &hwSensorMarginH);


    hwSensorW -= hwSensorMarginW;
    hwSensorH -= hwSensorMarginH;

    if (getReprocessingMode() == PROCESSING_MODE_REPROCESSING_PURE_BAYER) {
        ret = getCropRectAlign(hwSensorW, hwSensorH,
                     pictureW, pictureH,
                     &cropX, &cropY,
                     &cropW, &cropH,
                     CAMERA_BCROP_ALIGN, 2,
                     zoomLevel, zoomRatio);

        cropX = ALIGN_DOWN(cropX, 2);
        cropY = ALIGN_DOWN(cropY, 2);
        cropW = hwSensorW - (cropX * 2);
        cropH = hwSensorH - (cropY * 2);

        if (cropW < pictureW / maxZoomRatio || cropH < pictureH / maxZoomRatio) {
            CLOGW(" zoom ratio is upto x%d, crop(%dx%d), picture(%dx%d)", maxZoomRatio, cropW, cropH, pictureW, pictureH);
            cropX = ALIGN_DOWN(((hwSensorW - (pictureW / maxZoomRatio)) >> 1), 2);
            cropY = ALIGN_DOWN(((hwSensorH - (pictureH / maxZoomRatio)) >> 1), 2);
            cropW = hwSensorW - (cropX * 2);
            cropH = hwSensorH - (cropY * 2);
        }
    } else {
        zoomLevel = 0;
        zoomRatio = getZoomRatio(zoomLevel) / 1000;
        getHwBayerCropRegion(&hwSensorCropW, &hwSensorCropH, &hwSensorCropX, &hwSensorCropY);

        ret = getCropRectAlign(hwSensorCropW, hwSensorCropH,
                     pictureW, pictureH,
                     &cropX, &cropY,
                     &cropW, &cropH,
                     CAMERA_BCROP_ALIGN, 2,
                     zoomLevel, zoomRatio);

        cropX = ALIGN_DOWN(cropX, 2);
        cropY = ALIGN_DOWN(cropY, 2);
        cropW = hwSensorCropW - (cropX * 2);
        cropH = hwSensorCropH - (cropY * 2);

        if (cropW < pictureW / maxZoomRatio || cropH < pictureH / maxZoomRatio) {
            CLOGW(" zoom ratio is upto x%d, crop(%dx%d), picture(%dx%d)", maxZoomRatio, cropW, cropH, pictureW, pictureH);
            cropX = ALIGN_DOWN(((hwSensorCropW - (pictureW / maxZoomRatio)) >> 1), 2);
            cropY = ALIGN_DOWN(((hwSensorCropH - (pictureH / maxZoomRatio)) >> 1), 2);
            cropW = hwSensorCropW - (cropX * 2);
            cropH = hwSensorCropH - (cropY * 2);
        }
    }

#if 1
    CLOGD("maxSensorSize (%dx%d), hwSensorSize (%dx%d), previewSize (%dx%d)",
            maxSensorW, maxSensorH, hwSensorW, hwSensorH, previewW, previewH);
    CLOGD("hwPictureSize (%dx%d), pictureSize (%dx%d)",
            hwPictureW, hwPictureH, pictureW, pictureH);
    CLOGD("size cropX = %d, cropY = %d, cropW = %d, cropH = %d, zoom = %d",
            cropX, cropY, cropW, cropH, zoomLevel);
    CLOGD("size2 cropX = %d, cropY = %d, cropW = %d, cropH = %d, zoom = %d",
            crop_crop_x, crop_crop_y, crop_crop_w, crop_crop_h, zoomLevel);
    CLOGD("size pictureFormat = 0x%x, JPEG_INPUT_COLOR_FMT = 0x%x",
            pictureFormat, JPEG_INPUT_COLOR_FMT);
#endif

    srcRect->x = 0;
    srcRect->y = 0;
    srcRect->w = maxSensorW;
    srcRect->h = maxSensorH;
    srcRect->fullW = maxSensorW;
    srcRect->fullH = maxSensorH;
    srcRect->colorFormat = bayerFormat;

    dstRect->x = cropX;
    dstRect->y = cropY;
    dstRect->w = cropW;
    dstRect->h = cropH;
    dstRect->fullW = cropW;
    dstRect->fullH = cropH;
    dstRect->colorFormat = bayerFormat;
    return NO_ERROR;
}

status_t ExynosCamera1Parameters::m_getPreviewBdsSize(ExynosRect *dstRect)
{
    int hwBdsW = 0;
    int hwBdsH = 0;
    int videoW = 0, videoH = 0;
    int sizeList[SIZE_LUT_INDEX_END];

    /* matched ratio LUT is not existed, use equation */
    if (m_useSizeTable == false
        || m_staticInfo->previewSizeLut == NULL
        || m_staticInfo->previewSizeLutMax <= m_cameraInfo.previewSizeRatioId
        || m_getPreviewSizeList(sizeList) != NO_ERROR) {
        ExynosRect rect;
        return calcPreviewBDSSize(&rect, dstRect);
    }

    /* use LUT */
    hwBdsW = sizeList[BDS_W];
    hwBdsH = sizeList[BDS_H];

    {
        getHwVideoSize(&videoW, &videoH);
    }

    /* Change bds size to video size when large size recording scenario */
    if (getRecordingHint() == true
        && (videoW > hwBdsW || videoH > hwBdsH)) {
        /* Re-calculate bds height when preview ratio != video ratio */
        if (m_cameraInfo.previewSizeRatioId != m_cameraInfo.videoSizeRatioId) {
            CLOGV("preview ratioId(%d) != videoRatioId(%d), use previewRatioId",
                    m_cameraInfo.previewSizeRatioId, m_cameraInfo.videoSizeRatioId);

            /* This function is not use, cause of TPU internal buffer */
            /*
            ExynosRect bnsSize, bcropSize;
            getPreviewBayerCropSize(&bnsSize, &bcropSize);
            hwBdsH = ALIGN_UP(bcropSize.h * videoW / bcropSize.w, 2);

            if (hwBdsH > bcropSize.h) {
                hwBdsW = bcropSize.w;
                hwBdsH = bcropSize.h;
            }
            */
        } else {
            hwBdsW = videoW;
            hwBdsH = videoH;
        }
    }

    dstRect->x = 0;
    dstRect->y = 0;
    dstRect->w = hwBdsW;
    dstRect->h = hwBdsH;

#ifdef DEBUG_PERFRAME
    CLOGD("hwBdsSize (%dx%d)", dstRect->w, dstRect->h);
#endif

    return NO_ERROR;
}

status_t ExynosCamera1Parameters::calcPreviewBDSSize(ExynosRect *srcRect, ExynosRect *dstRect)
{
    status_t ret = NO_ERROR;
    int previewW = 0, previewH = 0;
    int bayerFormat = getBayerFormat(PIPE_3AA);
    ExynosRect bnsSize;
    ExynosRect bayerCropSize;

    /* Get preview size info */
    getHwPreviewSize(&previewW, &previewH);
    ret = getPreviewBayerCropSize(&bnsSize, &bayerCropSize);
    if (ret != NO_ERROR)
        CLOGE("getPreviewBayerCropSize() failed");

    srcRect->x = bayerCropSize.x;
    srcRect->y = bayerCropSize.y;
    srcRect->w = bayerCropSize.w;
    srcRect->h = bayerCropSize.h;
    srcRect->fullW = bnsSize.w;
    srcRect->fullH = bnsSize.h;
    srcRect->colorFormat = bayerFormat;

    dstRect->x = 0;
    dstRect->y = 0;

    if (this->isUseBDS() == true) {
        dstRect->w = previewW;
        dstRect->h = previewH;
        dstRect->fullW = previewW;
        dstRect->fullH = previewH;
    } else {
        dstRect->w = bayerCropSize.w;
        dstRect->h = bayerCropSize.h;
        dstRect->fullW = bayerCropSize.w;
        dstRect->fullH = bayerCropSize.h;
    }

    dstRect->colorFormat = bayerFormat;

    /* Check the invalid BDS size compared to Bcrop size */
    if (dstRect->w > srcRect->w)
        dstRect->w = srcRect->w;
    if (dstRect->h > srcRect->h)
        dstRect->h = srcRect->h;

#ifdef DEBUG_PERFRAME
    CLOGE("BDS %dx%d Preview %dx%d",
            dstRect->w, dstRect->h, previewW, previewH);
#endif

    return NO_ERROR;
}

status_t ExynosCamera1Parameters::calcPictureBDSSize(ExynosRect *srcRect, ExynosRect *dstRect)
{
    status_t ret = NO_ERROR;
    int pictureW = 0, pictureH = 0;
    int bayerFormat = getBayerFormat(PIPE_3AA_REPROCESSING);
    ExynosRect bnsSize;
    ExynosRect bayerCropSize;

    /* Get picture size info */
    getPictureSize(&pictureW, &pictureH);
    ret = getPictureBayerCropSize(&bnsSize, &bayerCropSize);
    if (ret != NO_ERROR)
        CLOGE("getPictureBayerCropSize() failed");

    srcRect->x = bayerCropSize.x;
    srcRect->y = bayerCropSize.y;
    srcRect->w = bayerCropSize.w;
    srcRect->h = bayerCropSize.h;
    srcRect->fullW = bnsSize.w;
    srcRect->fullH = bnsSize.h;
    srcRect->colorFormat = bayerFormat;

    dstRect->x = 0;
    dstRect->y = 0;
    dstRect->w = pictureW;
    dstRect->h = pictureH;
    dstRect->fullW = pictureW;
    dstRect->fullH = pictureH;
    dstRect->colorFormat = JPEG_INPUT_COLOR_FMT;

    /* Check the invalid BDS size compared to Bcrop size */
    if (dstRect->w > srcRect->w)
        dstRect->w = srcRect->w;
    if (dstRect->h > srcRect->h)
        dstRect->h = srcRect->h;

#ifdef DEBUG_PERFRAME
    CLOGE("BDS %dx%d Picture %dx%d",
            dstRect->w, dstRect->h, pictureW, pictureH);
#endif

    return NO_ERROR;
}

bool ExynosCamera1Parameters::doCscRecording(void)
{
    bool ret = true;
    int previewW = 0, previewH = 0;
    int videoW = 0, videoH = 0;
    {
        getPreviewSize(&previewW, &previewH);
        CLOGV("PreviewSize = %d x %d",   previewW, previewH);
    }

    getVideoSize(&videoW, &videoH);
    CLOGV("videoSize     = %d x %d",   videoW, videoH);

    if (((videoW == 3840 && videoH == 2160) || (videoW == 1920 && videoH == 1080) || (videoW == 2560 && videoH == 1440))
        && m_useAdaptiveCSCRecording == true
        && videoW == previewW
        && videoH == previewH
        && !getFlipHorizontal()
    ) {
        ret = false;
    }

    return ret;
}

#ifdef DEBUG_RAWDUMP
bool ExynosCamera1Parameters::checkBayerDumpEnable(void)
{
    return true;
}
#endif  /* DEBUG_RAWDUMP */

bool ExynosCamera1Parameters::increaseMaxBufferOfPreview(void)
{
    if((getShotMode() == SHOT_MODE_BEAUTY_FACE)||(getShotMode() == SHOT_MODE_FRONT_PANORAMA)
        || (getVtMode() == 4)
        ) {
        return true;
    } else {
        return false;
    }
}

void ExynosCamera1Parameters::m_setVideoSize(int w, int h)
{
    m_cameraInfo.videoW = w;
    m_cameraInfo.videoH = h;
}

#ifdef FORCE_CAL_RELOAD
bool ExynosCamera1Parameters::m_checkCalibrationDataValid(char *sensor_fw)
{
    bool ret = true;
    char *tok = NULL;
    char *calcheck = NULL;
    char *last;
    char readdata[50];
    FILE *fp = NULL;

    fp = fopen("/sys/class/camera/rear/rear_force_cal_load", "r");
    if (fp == NULL) {
        return ret;
    } else {
        if (fgets(readdata, sizeof(readdata), fp) == NULL) {
            CLOGE("failed to read sysfs entry");
        }
        CLOGD("%s", readdata);

        fclose(fp);
    }

    tok = strtok_r(sensor_fw, " ", &last);
    if (tok != NULL) {
        calcheck = strtok_r(NULL, " ", &last);
    }

    if (calcheck != NULL && strlen(calcheck) >= 2 && !strncmp(calcheck, "NG", 2)) {
        CLOGE("Cal data has error");
        ret = false;
    }

    return ret;
}
#endif

void ExynosCamera1Parameters::setUseCompanion(bool use)
{
    m_use_companion = use;
}

bool ExynosCamera1Parameters::getUseCompanion()
{
    if ((m_cameraId == CAMERA_ID_FRONT && getDualMode() == true) ||
        m_cameraId == CAMERA_ID_BACK_1)
        m_use_companion = false;

    return m_use_companion;
}

#ifdef SUPPORT_DEPTH_MAP
bool ExynosCamera1Parameters::getDepthCallbackOnPreview(void)
{
    return m_flagDepthCallbackOnPreview;
}

void ExynosCamera1Parameters::setDepthCallbackOnPreview(bool enable)
{
    m_flagDepthCallbackOnPreview = enable;
}

bool ExynosCamera1Parameters::getDepthCallbackOnCapture(void)
{
    return m_flagDepthCallbackOnCapture;
}

void ExynosCamera1Parameters::setDepthCallbackOnCapture(bool enable)
{
    m_flagDepthCallbackOnCapture = enable;
}

void ExynosCamera1Parameters::setDisparityMode(enum companion_disparity_mode disparity_mode)
{
    setMetaCtlDisparityMode(&m_metadata, disparity_mode);
}
#endif

void ExynosCamera1Parameters::setIsThumbnailCallbackOn(bool enable)
{
    m_IsThumbnailCallbackOn = enable;
}

bool ExynosCamera1Parameters::getIsThumbnailCallbackOn()
{
    return m_IsThumbnailCallbackOn;
}

void ExynosCamera1Parameters::setHWFCEnable(bool enable)
{
    m_flagHWFCEnable = enable;
}

bool ExynosCamera1Parameters::getHWFCEnable(void)
{
    if (m_flagHWFCEnable && !getOutPutFormatNV21Enable()
#ifdef USE_ODC_CAPTURE
        && !getODCCaptureEnable()
#endif
    ) {
        return true;
    } else {
        return false;
    }
}

bool ExynosCamera1Parameters::getCallbackNeedCSC(void)
{
#ifdef USE_GSC_FOR_PREVIEW_CALLBACK
    bool ret = true;

    int previewW = 0, previewH = 0;
    int hwPreviewW = 0, hwPreviewH = 0;
    int previewFormat = getPreviewFormat();

    getPreviewSize(&previewW, &previewH);
    getHwPreviewSize(&hwPreviewW, &hwPreviewH);
    if ((previewW == hwPreviewW)&&
        (previewH == hwPreviewH)&&
        (previewFormat == V4L2_PIX_FMT_NV21)) {
        ret = false;
    }
#ifdef GRALLOC_CAMERA_64BYTE_ALIGN
    if (hwPreviewW % CAMERA_64PX_ALIGN) {
        ret = true;
    }
#endif

#else
    bool ret = false;
#endif

    return ret;
}

bool ExynosCamera1Parameters::getCallbackNeedCopy2Rendering(void)
{
    bool ret = false;
    int previewW = 0, previewH = 0;

    {
        getPreviewSize(&previewW, &previewH);
        if (previewW * previewH <= 1920*1080) {
            ret = true;
        }

        // copy need, according to scenario.
        // ret = false;
    }

    return ret;
}

bool ExynosCamera1Parameters::getFaceDetectionMode(bool flagCheckingRecording)
{
    bool ret = true;

    /* turn off when dual mode without dual camera(fusion) */
    if (getDualMode() == true)
        ret = false;

    /* turn off when vt mode */
    if (getVtMode() != 0)
        ret = false;

    if (getHighSpeedRecording() == true)
        ret = false;

    /* when stopRecording, ignore recording hint */
    if (flagCheckingRecording == true) {
        /* when recording mode, turn off back camera */
        if (getRecordingHint() == true) {
            if (getCameraId() == CAMERA_ID_BACK)
                ret = false;
        }
    }

    return ret;
}

bool ExynosCamera1Parameters::checkEnablePicture(void)
{
    bool ret = true;
    int fastFpsMode = getFastFpsMode();

    if (getIntelligentMode() == 1
#ifndef USE_SNAPSHOT_ON_UHD_RECORDING
        || getUHDRecordingMode()
#endif
#ifndef USE_SNAPSHOT_ON_DUAL_RECORDING
        || getEffectRecordingHint()
        || getDualRecordingHint()
#endif
        || (CONFIG_MODE::HIGHSPEED_60 < fastFpsMode && fastFpsMode < CONFIG_MODE::MAX)
    ) {
        ret = false;
    }

    return ret;
}

status_t ExynosCamera1Parameters::checkShutterSpeed(const CameraParameters& params)
{
    int newShutterSpeed = -1;
    int curShutterSpeed = -1;
    int maxShutterSpeed = params.getInt("max-shutterspeed");
    int minShutterSpeed = params.getInt("min-shutterspeed");

    newShutterSpeed = params.getInt("shutterspeed");

    if (newShutterSpeed < minShutterSpeed || newShutterSpeed > maxShutterSpeed) {
        return NO_ERROR;
    }

    CLOGD("[setParameters]newShutterSpeed %d", newShutterSpeed);

    curShutterSpeed = getShutterSpeed();

    if (curShutterSpeed != newShutterSpeed) {
        m_setShutterSpeed(newShutterSpeed);
        m_params.set("shutterspeed", newShutterSpeed);
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setShutterSpeed(int shutterSpeed)
{
    m_shutterSpeed = shutterSpeed;
}

int ExynosCamera1Parameters::getShutterSpeed(void)
{
    return m_shutterSpeed;
}

status_t ExynosCamera1Parameters::checkGain(const CameraParameters& params)
{
    int newGain = -1;
    int curGain = -1;
    int maxGain = params.getInt("max-gain");
    int minGain = params.getInt("min-gain");

    newGain = params.getInt("gain");

    if (newGain < minGain || newGain > maxGain) {
        return NO_ERROR;
    }

    CLOGD("[setParameters]newGain %d", newGain);

    curGain = getGain();

    if (curGain != newGain) {
        m_setGain(newGain);
        m_params.set("gain", newGain);
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setGain(int gain)
{
    m_gain = gain;
}

int ExynosCamera1Parameters::getGain(void)
{
    return m_gain;
}

status_t ExynosCamera1Parameters::checkIrLedWidth(const CameraParameters& params)
{
    int newIrLedWidth = -1;
    int curIrLedWidth = -1;
    int maxIrLedWidth = params.getInt("max-pulse-width");
    int minIrLedWidth = params.getInt("min-pulse-width");

    newIrLedWidth = params.getInt("pulse-width");

    if (newIrLedWidth < minIrLedWidth || newIrLedWidth > maxIrLedWidth) {
        return NO_ERROR;
    }

    CLOGD("[setParameters]newIrLedWidth %d", newIrLedWidth);

    curIrLedWidth = getIrLedWidth();

    if (curIrLedWidth != newIrLedWidth) {
        m_setIrLedWidth(newIrLedWidth);
        m_params.set("pulse-width", newIrLedWidth);
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setIrLedWidth(int irLedWidth)
{
    m_irLedWidth = irLedWidth;
}

int ExynosCamera1Parameters::getIrLedWidth(void)
{
    return m_irLedWidth;
}

status_t ExynosCamera1Parameters::checkIrLedDelay(const CameraParameters& params)
{
    int newIrLedDelay = -1;
    int curIrLedDelay = -1;
    int maxIrLedDelay = params.getInt("max-pulse-delay");
    int minIrLedDelay = params.getInt("min-pulse-delay");

    newIrLedDelay = params.getInt("pulse-delay");

    if (newIrLedDelay < minIrLedDelay || newIrLedDelay > maxIrLedDelay) {
        return NO_ERROR;
    }

    CLOGD("[setParameters]newIrLedDelay %d", newIrLedDelay);

    curIrLedDelay = getIrLedDelay();

    if (curIrLedDelay != newIrLedDelay) {
        m_setIrLedDelay(newIrLedDelay);
        m_params.set("pulse-delay", newIrLedDelay);
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setIrLedDelay(int irLedDelay)
{
    m_irLedDelay = irLedDelay;
}

int ExynosCamera1Parameters::getIrLedDelay(void)
{
    return m_irLedDelay;
}

status_t ExynosCamera1Parameters::checkIrLedCurrent(const CameraParameters& params)
{
    int newIrLedCurrent = -1;
    int curIrLedCurrent = -1;
    int maxIrLedCurrent = params.getInt("max-dac");
    int minIrLedCurrent = params.getInt("min-dac");

    newIrLedCurrent = params.getInt("dac");

    if (newIrLedCurrent < minIrLedCurrent || newIrLedCurrent > maxIrLedCurrent) {
        return NO_ERROR;
    }

    CLOGD("[setParameters]newIrLedCurrent %d", newIrLedCurrent);

    curIrLedCurrent = getIrLedCurrent();

    if (curIrLedCurrent != newIrLedCurrent) {
        m_setIrLedCurrent(newIrLedCurrent);
        m_params.set("dac", newIrLedCurrent);
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setIrLedCurrent(int irLedCurrent)
{
    m_irLedCurrent = irLedCurrent;
}

int ExynosCamera1Parameters::getIrLedCurrent(void)
{
    return m_irLedCurrent;
}

status_t ExynosCamera1Parameters::checkIrLedOnTime(const CameraParameters& params)
{
    int newIrLedOnTime = -1;
    int curIrLedOnTime = -1;
    int maxIrLedOnTime = params.getInt("max-led-max-time");
    int minIrLedOnTime = params.getInt("min-led-max-time");
    char propertyValue[PROPERTY_VALUE_MAX];

    newIrLedOnTime = params.getInt("led-max-time");

    property_get("ro.factory.factory_binary", propertyValue, "0");
    if (strncmp(propertyValue, "factory", 7)) {
        return NO_ERROR;
    }

    if (newIrLedOnTime < minIrLedOnTime || newIrLedOnTime > maxIrLedOnTime) {
        return NO_ERROR;
    }

    CLOGD("[setParameters]newIrLedOnTime %d", newIrLedOnTime);

    curIrLedOnTime = getIrLedOnTime();

    if (curIrLedOnTime != newIrLedOnTime) {
        m_setIrLedOnTime(newIrLedOnTime);
        m_params.set("led-max-time", newIrLedOnTime);
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setIrLedOnTime(int irLedOnTime)
{
    m_irLedOnTime = irLedOnTime;
}

int ExynosCamera1Parameters::getIrLedOnTime(void)
{
    return m_irLedOnTime;
}

}; /* namespace android */
