/*
**
** Copyright 2015, Samsung Electronics Co. LTD
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
#define LOG_TAG "ExynosCamera3Parameters"
#include <cutils/log.h>

#include "ExynosCamera3Parameters.h"

namespace android {

ExynosCamera3Parameters::ExynosCamera3Parameters(int cameraId, bool flagCompanion, int halVersion)
{
    m_cameraId = cameraId;
    m_halVersion = halVersion;

    const char *myName = (m_cameraId == CAMERA_ID_BACK) ? "ParametersBack" : "ParametersFront";
    strncpy(m_name, myName,  EXYNOS_CAMERA_NAME_STR_SIZE - 1);

    m_staticInfo = createExynosCamera3SensorInfo(cameraId);
    m_useSizeTable = (m_staticInfo->sizeTableSupport) ? USE_CAMERA_SIZE_TABLE : false;
    m_useAdaptiveCSCRecording = (cameraId == CAMERA_ID_BACK) ? USE_ADAPTIVE_CSC_RECORDING : USE_ADAPTIVE_CSC_RECORDING_FRONT;

    m_exynosconfig = NULL;
    m_activityControl = new ExynosCameraActivityControl(m_cameraId);

    memset(&m_cameraInfo, 0, sizeof(struct exynos_camera_info));
    memset(&m_exifInfo, 0, sizeof(m_exifInfo));

    m_initMetadata();

    m_setExifFixedAttribute();

    m_exynosconfig = new ExynosConfigInfo();

    mDebugInfo.num_of_appmarker = 1; /* Default : APP4 */
    mDebugInfo.idx[0][0] = APP_MARKER_4; /* matching the app marker 4 */

#ifdef SAMSUNG_OIS
    if (cameraId == CAMERA_ID_BACK) {
        mDebugInfo.debugSize[APP_MARKER_4]  = sizeof(struct camera2_udm) + sizeof(struct ois_exif_data);
    } else {
        mDebugInfo.debugSize[APP_MARKER_4] = sizeof(struct camera2_udm);
    }
#else
    mDebugInfo.debugSize[APP_MARKER_4] = sizeof(struct camera2_udm);
#endif
    mDebugInfo.debugData[APP_MARKER_4] = new char[mDebugInfo.debugSize[APP_MARKER_4]];
    memset((void *)mDebugInfo.debugData[APP_MARKER_4], 0, mDebugInfo.debugSize[APP_MARKER_4]);
    memset((void *)m_exynosconfig, 0x00, sizeof(struct ExynosConfigInfo));

    // CAUTION!! : Initial values must be prior to setDefaultParameter() function.
    // Initial Values : START
#if defined(SAMSUNG_COMPANION) || defined(SAMSUNG_EEPROM)
    m_romReadThreadDone = false;
    m_use_companion = flagCompanion;
#endif
    m_fastFpsMode = 0;
    m_previewSizeChanged = false;
    m_flagRestartPreviewChecked = false;
    m_flagRestartPreview = false;
    m_reallocBuffer = false;
    m_flagCheckDualMode = false;
    m_flagVideoStabilization = false;
    m_flag3dnrMode = false;
#ifdef LLS_CAPTURE
    m_flagLLSOn = false;
    m_LLSCaptureOn = false;
#endif
#ifdef SR_CAPTURE
    m_flagSRSOn = false;
#endif
#ifdef OIS_CAPTURE
    m_flagOISCaptureOn = false;
#endif
#ifdef USE_BINNING_MODE
    m_binningProperty = checkProperty(false);
#endif

    m_flagCheckRecordingHint = false;
    m_zoomWithScaler = false;

    m_useDynamicBayer = (cameraId == CAMERA_ID_BACK) ? USE_DYNAMIC_BAYER : USE_DYNAMIC_BAYER_FRONT;
    m_useDynamicBayerVideoSnapShot =
        (cameraId == CAMERA_ID_BACK) ? USE_DYNAMIC_BAYER_VIDEO_SNAP_SHOT : USE_DYNAMIC_BAYER_VIDEO_SNAP_SHOT_FRONT;
    m_useDynamicBayer120FpsVideoSnapShot =
        (cameraId == CAMERA_ID_BACK) ? USE_DYNAMIC_BAYER_120FPS_VIDEO_SNAP_SHOT : USE_DYNAMIC_BAYER_120FPS_VIDEO_SNAP_SHOT_FRONT;
    m_useDynamicBayer240FpsVideoSnapShot =
        (cameraId == CAMERA_ID_BACK) ? USE_DYNAMIC_BAYER_240FPS_VIDEO_SNAP_SHOT : USE_DYNAMIC_BAYER_240FPS_VIDEO_SNAP_SHOT_FRONT;
    m_useDynamicScc = (cameraId == CAMERA_ID_BACK) ? USE_DYNAMIC_SCC_REAR : USE_DYNAMIC_SCC_FRONT;

#if defined(SAMSUNG_COMPANION) || defined(SAMSUNG_EEPROM)
    m_useFastenAeStable = isFastenAeStable(cameraId, m_use_companion);
#else
    m_useFastenAeStable = false;
#endif

    /* we cannot know now, whether recording mode or not */
    /*
    if (getRecordingHint() == true || getDualRecordingHint() == true)
        m_usePureBayerReprocessing = (cameraId == CAMERA_ID_BACK) ? USE_PURE_BAYER_REPROCESSING_ON_RECORDING : USE_PURE_BAYER_REPROCESSING_FRONT_ON_RECORDING;
    else
    */
    m_usePureBayerReprocessing = (cameraId == CAMERA_ID_BACK) ? USE_PURE_BAYER_REPROCESSING : USE_PURE_BAYER_REPROCESSING_FRONT;

#ifdef SAMSUNG_LLV
    m_isLLVOn = true;
#endif

    m_enabledMsgType = 0;

    m_previewBufferCount = NUM_PREVIEW_BUFFERS;

    m_dvfsLock = false;

#ifdef SAMSUNG_DOF
    m_curLensStep = 0;
    m_curLensCount = 0;
#endif

#ifdef SAMSUNG_OIS
    m_oisNode = NULL;
    m_setOISmodeSetting = false;
#ifdef OIS_CAPTURE
    m_llsValue = 0;
#endif
#endif
    m_zoom_activated = false;

#ifdef SUPPORT_DEPTH_MAP
    m_flaguseDepthMap = false;
#endif

    for (int i = 0; i < this->getYuvStreamMaxNum(); i++) {
        m_yuvBufferCount[i] = 1;
#ifdef USE_BDS_2_0_480P_YUV
        m_yuvSizeSetup[i] = false;
#endif
    }

    resetMinYuvSize();

    // Initial Values : END
    setDefaultCameraInfo();
}

ExynosCamera3Parameters::~ExynosCamera3Parameters()
{
    if (m_staticInfo != NULL) {
        delete m_staticInfo;
        m_staticInfo = NULL;
    }

    if (m_activityControl != NULL) {
        delete m_activityControl;
        m_activityControl = NULL;
    }

    for(int i = 0; i < mDebugInfo.num_of_appmarker; i++) {
        if(mDebugInfo.debugData[mDebugInfo.idx[i][0]])
                delete mDebugInfo.debugData[mDebugInfo.idx[i][0]];
        mDebugInfo.debugData[mDebugInfo.idx[i][0]] = NULL;
        mDebugInfo.debugSize[mDebugInfo.idx[i][0]] = 0;
    }

    if (m_exynosconfig != NULL) {
        memset((void *)m_exynosconfig, 0x00, sizeof(struct ExynosConfigInfo));
        delete m_exynosconfig;
        m_exynosconfig = NULL;
    }

    if (m_exifInfo.maker_note) {
        delete m_exifInfo.maker_note;
        m_exifInfo.maker_note = NULL;
    }

    if (m_exifInfo.user_comment) {
        delete m_exifInfo.user_comment;
        m_exifInfo.user_comment = NULL;
    }
}

int ExynosCamera3Parameters::getCameraId(void)
{
    return m_cameraId;
}

void ExynosCamera3Parameters::setDefaultCameraInfo(void)
{
    CLOGI("");

    m_setHwSensorSize(m_staticInfo->maxSensorW, m_staticInfo->maxSensorH);
    for (int i = 0; i < this->getYuvStreamMaxNum(); i++) {
        m_setYuvSize(m_staticInfo->maxPreviewW, m_staticInfo->maxPreviewH, i);
        m_setYuvFormat(V4L2_PIX_FMT_NV21, i);
    }

    m_setHwPictureSize(m_staticInfo->maxPictureW, m_staticInfo->maxPictureH);
    m_setHwPictureFormat(SCC_OUTPUT_COLOR_FMT);
    m_setThumbnailSize(m_staticInfo->maxThumbnailW, m_staticInfo->maxThumbnailH);

    /* Initalize BNS scale ratio, step:500, ex)1500->x1.5 scale down */
    m_setBnsScaleRatio(1000);
    /* Initalize Binning scale ratio */
    m_setBinningScaleRatio(1000);

    m_setRecordingHint(false);
    setDualMode(false);
    m_setEffectHint(0);
}

status_t ExynosCamera3Parameters::m_setIntelligentMode(int intelligentMode)
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
        CLOGE("vision mode can not change before stoppreview");
        visionMode = true;
    }

    m_setVisionMode(visionMode);

    return ret;
 }

int ExynosCamera3Parameters::getIntelligentMode(void)
{
    return m_cameraInfo.intelligentMode;
}

void ExynosCamera3Parameters::m_setVisionMode(bool vision)
{
    m_cameraInfo.visionMode = vision;
}

bool ExynosCamera3Parameters::getVisionMode(void)
{
    return m_cameraInfo.visionMode;
}

void ExynosCamera3Parameters::m_setVisionModeFps(int fps)
{
    m_cameraInfo.visionModeFps = fps;
}

int ExynosCamera3Parameters::getVisionModeFps(void)
{
    return m_cameraInfo.visionModeFps;
}

void ExynosCamera3Parameters::m_setVisionModeAeTarget(int ae)
{
    m_cameraInfo.visionModeAeTarget = ae;
}

int ExynosCamera3Parameters::getVisionModeAeTarget(void)
{
    return m_cameraInfo.visionModeAeTarget;
}

void ExynosCamera3Parameters::m_setRecordingHint(bool hint)
{
    m_cameraInfo.recordingHint = hint;

    if (hint) {
        setMetaVideoMode(&m_metadata, AA_VIDEOMODE_ON);
    } else if (!hint && !getDualRecordingHint() && !getEffectRecordingHint()) {
        setMetaVideoMode(&m_metadata, AA_VIDEOMODE_OFF);
    }

    /* RecordingHint is confirmed */
    m_flagCheckRecordingHint = true;
}

bool ExynosCamera3Parameters::getRecordingHint(void)
{
    /*
     * Before setParameters, we cannot know recordingHint is valid or not
     * So, check and make assert for fast debugging
     */
    if (m_flagCheckRecordingHint == false)
        android_printAssert(NULL, LOG_TAG, "Cannot call getRecordingHint befor setRecordingHint, assert!!!!");

    return m_cameraInfo.recordingHint;
}

void ExynosCamera3Parameters::setDualMode(bool dual)
{
    m_cameraInfo.dualMode = dual;
    /* dualMode is confirmed */
    m_flagCheckDualMode = true;
}

bool ExynosCamera3Parameters::getDualMode(void)
{
    /*
    * Before setParameters, we cannot know dualMode is valid or not
    * So, check and make assert for fast debugging
    */
    if (m_flagCheckDualMode == false)
        return 0;

    return m_cameraInfo.dualMode;
}

void ExynosCamera3Parameters::m_setDualRecordingHint(bool hint)
{
    m_cameraInfo.dualRecordingHint = hint;

    if (hint) {
        setMetaVideoMode(&m_metadata, AA_VIDEOMODE_ON);
    } else if (!hint && !getRecordingHint() && !getEffectRecordingHint()) {
        setMetaVideoMode(&m_metadata, AA_VIDEOMODE_OFF);
    }
}

bool ExynosCamera3Parameters::getDualRecordingHint(void)
{
    return m_cameraInfo.dualRecordingHint;
}

void ExynosCamera3Parameters::m_setEffectHint(bool hint)
{
    m_cameraInfo.effectHint = hint;
}

bool ExynosCamera3Parameters::getEffectHint(void)
{
    return m_cameraInfo.effectHint;
}

bool ExynosCamera3Parameters::getEffectRecordingHint(void)
{
    return m_cameraInfo.effectRecordingHint;
}

status_t ExynosCamera3Parameters::checkPreviewFpsRange(uint32_t minFps, uint32_t maxFps)
{
    status_t ret = NO_ERROR;
    uint32_t curMinFps = 0, curMaxFps = 0;

    getPreviewFpsRange(&curMinFps, &curMaxFps);
    if (curMinFps != minFps || curMaxFps != maxFps)
        m_setPreviewFpsRange(minFps, maxFps);

    return ret;
}

void ExynosCamera3Parameters::m_setPreviewFpsRange(uint32_t min, uint32_t max)
{
    setMetaCtlAeTargetFpsRange(&m_metadata, min, max);
    setMetaCtlSensorFrameDuration(&m_metadata, (uint64_t)((1000 * 1000 * 1000) / (uint64_t)max));

    CLOGI("fps min(%d) max(%d)", min, max);
}

void ExynosCamera3Parameters::getPreviewFpsRange(uint32_t *min, uint32_t *max)
{
    /* ex) min = 15 , max = 30 */
    getMetaCtlAeTargetFpsRange(&m_metadata, min, max);
}

status_t ExynosCamera3Parameters::checkVideoSize(int newVideoW, int newVideoH)
{
    if (0 < newVideoW && 0 < newVideoH &&
        m_isSupportedVideoSize(newVideoW, newVideoH) == false) {
        return BAD_VALUE;
    }

    CLOGI("[setParameters]newVideo Size (%dx%d), ratioId(%d)",
        newVideoW, newVideoH, m_cameraInfo.videoSizeRatioId);
    m_setVideoSize(newVideoW, newVideoH);

    return NO_ERROR;
}

bool ExynosCamera3Parameters::m_isSupportedVideoSize(const int width,
                                                    const int height)
{
    int maxWidth = 0;
    int maxHeight = 0;
    int (*sizeList)[SIZE_OF_RESOLUTION];

    getMaxVideoSize(&maxWidth, &maxHeight);

    if (maxWidth < width || maxHeight < height) {
        CLOGE("invalid video Size(maxSize(%d/%d) size(%d/%d)",
                maxWidth, maxHeight, width, height);
        return false;
    }

    if (getCameraId() == CAMERA_ID_BACK) {
        sizeList = m_staticInfo->rearVideoList;
        for (int i = 0; i < m_staticInfo->rearVideoListMax; i++) {
            if (sizeList[i][0] > maxWidth || sizeList[i][1] > maxHeight)
                continue;
            if (sizeList[i][0] == width && sizeList[i][1] == height) {
                m_cameraInfo.videoSizeRatioId = sizeList[i][2];
                return true;
            }
        }
    } else {
        sizeList = m_staticInfo->frontVideoList;
        for (int i = 0; i <  m_staticInfo->frontVideoListMax; i++) {
            if (sizeList[i][0] > maxWidth || sizeList[i][1] > maxHeight)
                continue;
            if (sizeList[i][0] == width && sizeList[i][1] == height) {
                m_cameraInfo.videoSizeRatioId = sizeList[i][2];
                return true;
            }
        }
    }

    CLOGE("Invalid video size(%dx%d)", width, height);

    return false;
}

bool ExynosCamera3Parameters::m_isUHDRecordingMode(void)
{
    bool isUHDRecording = false;
    int videoW = 0, videoH = 0;
    getVideoSize(&videoW, &videoH);

    if (((videoW == 3840 && videoH == 2160) || (videoW == 2560 && videoH == 1440))
        && getRecordingHint() == true)
        isUHDRecording = true;

    return isUHDRecording;
}

void ExynosCamera3Parameters::m_setVideoSize(int w, int h)
{
    m_cameraInfo.videoW = w;
    m_cameraInfo.videoH = h;
}

bool ExynosCamera3Parameters::getUHDRecordingMode(void)
{
    return m_isUHDRecordingMode();
}

void ExynosCamera3Parameters::getVideoSize(int *w, int *h)
{
    *w = m_cameraInfo.videoW;
    *h = m_cameraInfo.videoH;
}

void ExynosCamera3Parameters::getMaxVideoSize(int *w, int *h)
{
    *w = m_staticInfo->maxVideoW;
    *h = m_staticInfo->maxVideoH;
}

int ExynosCamera3Parameters::getVideoFormat(void)
{
    if (getAdaptiveCSCRecording() == true) {
        return V4L2_PIX_FMT_NV21M;
    } else {
        return V4L2_PIX_FMT_NV12M;
    }
}

status_t ExynosCamera3Parameters::checkCallbackSize(int callbackW, int callbackH)
{
    status_t ret = NO_ERROR;
    int curCallbackW = -1, curCallbackH = -1;

    if (callbackW < 0 || callbackH < 0) {
        CLOGE("Invalid callback size. %dx%d",
                 callbackW, callbackH);
        return INVALID_OPERATION;
    }

    getCallbackSize(&curCallbackW, &curCallbackH);

    if (callbackW != curCallbackW || callbackH != curCallbackH) {
        CLOGI("curCallbackSize %dx%d newCallbackSize %dx%d",
                curCallbackW, curCallbackH, callbackW, callbackH);

        m_setCallbackSize(callbackW, callbackH);
    }

    return ret;
}

void ExynosCamera3Parameters::m_setCallbackSize(int w, int h)
{
    m_cameraInfo.callbackW = w;
    m_cameraInfo.callbackH = h;
}

void ExynosCamera3Parameters::getCallbackSize(int *w, int *h)
{
    *w = m_cameraInfo.callbackW;
    *h = m_cameraInfo.callbackH;
}

status_t ExynosCamera3Parameters::checkCallbackFormat(int callbackFormat)
{
    status_t ret = NO_ERROR;
    int curCallbackFormat = -1;
    int newCallbackFormat = -1;

    if (callbackFormat < 0) {
        CLOGE("Inavlid callback format. %x",
                 callbackFormat);
        return INVALID_OPERATION;
    }

    newCallbackFormat = HAL_PIXEL_FORMAT_2_V4L2_PIX(callbackFormat);
    curCallbackFormat = getCallbackFormat();

    if (curCallbackFormat != newCallbackFormat) {
        char curFormatName[V4L2_FOURCC_LENGTH] = {};
        char newFormatName[V4L2_FOURCC_LENGTH] = {};
        m_getV4l2Name(curFormatName, V4L2_FOURCC_LENGTH, curCallbackFormat);
        m_getV4l2Name(newFormatName, V4L2_FOURCC_LENGTH, newCallbackFormat);
        CLOGI("curCallbackFormat %s newCallbackFormat %s",
                 curFormatName, newFormatName);

        m_setCallbackFormat(newCallbackFormat);
    }

    return ret;
}

void ExynosCamera3Parameters::m_setCallbackFormat(int colorFormat)
{
    m_cameraInfo.callbackFormat = colorFormat;
}

int ExynosCamera3Parameters::getCallbackFormat(void)
{
    return m_cameraInfo.callbackFormat;
}

bool ExynosCamera3Parameters::getReallocBuffer() {
    Mutex::Autolock lock(m_reallocLock);
    return m_reallocBuffer;
}

bool ExynosCamera3Parameters::setReallocBuffer(bool enable) {
    Mutex::Autolock lock(m_reallocLock);
    m_reallocBuffer = enable;
    return m_reallocBuffer;
}

void ExynosCamera3Parameters::setFastFpsMode(int fpsMode)
{
    m_fastFpsMode = fpsMode;
}

int ExynosCamera3Parameters::getFastFpsMode(void)
{
    return m_fastFpsMode;
}

void ExynosCamera3Parameters::m_setHighSpeedRecording(bool highSpeed)
{
    m_cameraInfo.highSpeedRecording = highSpeed;
}

bool ExynosCamera3Parameters::getHighSpeedRecording(void)
{
    return m_cameraInfo.highSpeedRecording;
}

void ExynosCamera3Parameters::m_setRestartPreviewChecked(bool restart)
{
    CLOGD("setRestartPreviewChecked(during SetParameters) %s", restart ? "true" : "false");
    Mutex::Autolock lock(m_parameterLock);

    m_flagRestartPreviewChecked = restart;
}

bool ExynosCamera3Parameters::m_getRestartPreviewChecked(void)
{
    Mutex::Autolock lock(m_parameterLock);

    return m_flagRestartPreviewChecked;
}

void ExynosCamera3Parameters::m_setRestartPreview(bool restart)
{
    CLOGD("setRestartPreview %s", restart ? "true" : "false");
    Mutex::Autolock lock(m_parameterLock);

    m_flagRestartPreview = restart;

}

void ExynosCamera3Parameters::m_setVideoStabilization(bool stabilization)
{
    m_cameraInfo.videoStabilization = stabilization;
}

bool ExynosCamera3Parameters::getVideoStabilization(void)
{
    return m_cameraInfo.videoStabilization;
}

bool ExynosCamera3Parameters::updateTpuParameters(void)
{
    status_t ret = NO_ERROR;

    /* 1. update data video stabilization state to actual*/
    CLOGD("video stabilization old(%d) new(%d)", m_cameraInfo.videoStabilization, m_flagVideoStabilization);
    m_setVideoStabilization(m_flagVideoStabilization);

    bool hwVdisMode  = this->getHWVdisMode();

    if (setDisEnable(hwVdisMode) != NO_ERROR) {
        CLOGE("setDisEnable(%d) fail", hwVdisMode);
    }

    /* 2. update data 3DNR state to actual*/
    CLOGD("3DNR old(%d) new(%d)", m_cameraInfo.is3dnrMode, m_flag3dnrMode);
    m_set3dnrMode(m_flag3dnrMode);
    if (setDnrEnable(m_flag3dnrMode) != NO_ERROR) {
        CLOGE("setDnrEnable(%d) fail", m_flag3dnrMode);
    }

    return true;
}

bool ExynosCamera3Parameters::isSWVdisMode(void)
{
    bool swVDIS_mode = false;
    bool use3DNR_dmaout = false;

    int nPreviewW, nPreviewH;
    getPreviewSize(&nPreviewW, &nPreviewH);

    if ((getRecordingHint() == true) &&
        (getCameraId() == CAMERA_ID_BACK) &&
        (getHighSpeedRecording() == false) &&
        (use3DNR_dmaout == false) &&
        (getSWVdisUIMode() == true) &&
        ((nPreviewW == 1920 && nPreviewH == 1080) || (nPreviewW == 1280 && nPreviewH == 720)))
    {
        swVDIS_mode = true;
    }

    return swVDIS_mode;
}

bool ExynosCamera3Parameters::isSWVdisModeWithParam(int nPreviewW, int nPreviewH)
{
    bool swVDIS_mode = false;
    bool use3DNR_dmaout = false;

    if ((getRecordingHint() == true) &&
        (getCameraId() == CAMERA_ID_BACK) &&
        (getHighSpeedRecording() == false) &&
        (use3DNR_dmaout == false) &&
        (getSWVdisUIMode() == true) &&
        ((nPreviewW == 1920 && nPreviewH == 1080) || (nPreviewW == 1280 && nPreviewH == 720)))
    {
        swVDIS_mode = true;
    }

    return swVDIS_mode;
}

bool ExynosCamera3Parameters::getHWVdisMode(void)
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
        default:
            ret = false;
            break;
        }
    }

#ifdef SUPPORT_SW_VDIS
    if (ret == true &&
        this->isSWVdisMode() == true) {
        ret = false;
    }
#endif

    return ret;
}

int ExynosCamera3Parameters::getHWVdisFormat(void)
{
    return V4L2_PIX_FMT_YUYV;
}

void ExynosCamera3Parameters::m_setSWVdisMode(bool swVdis)
{
    m_cameraInfo.swVdisMode = swVdis;
}

bool ExynosCamera3Parameters::getSWVdisMode(void)
{
    return m_cameraInfo.swVdisMode;
}

void ExynosCamera3Parameters::m_setSWVdisUIMode(bool swVdisUI)
{
    m_cameraInfo.swVdisUIMode = swVdisUI;
}

bool ExynosCamera3Parameters::getSWVdisUIMode(void)
{
    return m_cameraInfo.swVdisUIMode;
}

status_t ExynosCamera3Parameters::checkPreviewSize(int previewW, int previewH)
{
    /* preview size */
    int newPreviewW = 0;
    int newPreviewH = 0;
    int newCalHwPreviewW = 0;
    int newCalHwPreviewH = 0;

    int curPreviewW = 0;
    int curPreviewH = 0;
    int curHwPreviewW = 0;
    int curHwPreviewH = 0;

    getPreviewSize(&curPreviewW, &curPreviewH);
    getHwPreviewSize(&curHwPreviewW, &curHwPreviewH);

    newPreviewW = previewW;
    newPreviewH = previewH;
    if (m_adjustPreviewSize(&newPreviewW, &newPreviewH, &newCalHwPreviewW, &newCalHwPreviewH) != OK) {
        CLOGE("[Parameters] adjustPreviewSize fail, newPreviewSize(%dx%d)", newPreviewW, newPreviewH);
        return BAD_VALUE;
    }

    if (m_isSupportedPreviewSize(newPreviewW, newPreviewH) == false) {
        CLOGE("[Parameters] new preview size is invalid(%dx%d)", newPreviewW, newPreviewH);
        return BAD_VALUE;
    }

    CLOGI("[setParameters]Cur Preview size(%dx%d)", curPreviewW, curPreviewH);
    CLOGI("[setParameters]Cur HwPreview size(%dx%d)", curHwPreviewW, curHwPreviewH);
    CLOGI("[setParameters]param.preview size(%dx%d)", previewW, previewH);
    CLOGI("[setParameters]Adjust Preview size(%dx%d), ratioId(%d)", newPreviewW, newPreviewH, m_cameraInfo.previewSizeRatioId);
    CLOGI("[setParameters]Calibrated HwPreview size(%dx%d)", newCalHwPreviewW, newCalHwPreviewH);

    if (curPreviewW != newPreviewW || curPreviewH != newPreviewH ||
        curHwPreviewW != newCalHwPreviewW || curHwPreviewH != newCalHwPreviewH ||
        getHighResolutionCallbackMode() == true) {
        m_setPreviewSize(newPreviewW, newPreviewH);
        m_setHwPreviewSize(newCalHwPreviewW, newCalHwPreviewH);

        if (getHighResolutionCallbackMode() == true) {
            m_previewSizeChanged = false;
        } else {
            CLOGD("setRestartPreviewChecked true");
            m_setRestartPreviewChecked(true);
            m_previewSizeChanged = true;
        }
    } else {
        m_previewSizeChanged = false;
    }

    updateBinningScaleRatio();
    updateBnsScaleRatio();

    return NO_ERROR;
}

status_t ExynosCamera3Parameters::checkYuvSize(const int width, const int height, const int outputPortId)
{
    status_t ret = NO_ERROR;
    int curYuvWidth = 0;
    int curYuvHeight = 0;

    getYuvSize(&curYuvWidth, &curYuvHeight, outputPortId);

    if (m_isSupportedPreviewSize(width, height) == false) {
        CLOGE("Invalid YUV size. %dx%d",
                 width, height);
        return BAD_VALUE;
    }

    CLOGI("curYuvSize %dx%d newYuvSize %dx%d outputPortId %d",
            curYuvWidth, curYuvHeight, width, height, outputPortId);

    if (curYuvWidth != width || curYuvHeight != height) {
        m_setYuvSize(width, height, outputPortId);
#ifdef USE_BDS_2_0_480P_YUV
        m_yuvSizeSetup[outputPortId] = true;
#endif

        CLOGD("setRestartPreviewChecked true");
        m_setRestartPreviewChecked(true);
        m_previewSizeChanged = true;
    } else {
        m_previewSizeChanged = false;
    }

    /* Update minimum YUV size */
    if(m_minYuvW == 0) {
        m_minYuvW = width;
    } else if (width < m_minYuvW) {
        m_minYuvW = width;
    }

    if(m_minYuvH == 0) {
        m_minYuvH = height;
    } else if (height < m_minYuvH) {
        m_minYuvH = height;
    }

#ifdef SUPPORT_DEPTH_MAP
    checkUseDepthMap();
#endif

    return NO_ERROR;
}

status_t ExynosCamera3Parameters::m_adjustPreviewSize(int *newPreviewW, int *newPreviewH,
                                                      int *newCalHwPreviewW, int *newCalHwPreviewH)
{
    /* hack : when app give 1446, we calibrate to 1440 */
    if (*newPreviewW == 1446 && *newPreviewH == 1080) {
        CLOGW("Invalid previewSize(%d/%d). so, calibrate to (1440/%d)", *newPreviewW, *newPreviewH, *newPreviewH);
        *newPreviewW = 1440;
    }

    *newCalHwPreviewW = *newPreviewW;
    *newCalHwPreviewH = *newPreviewH;

    return NO_ERROR;
}

#ifdef HAL3_YUVSIZE_BASED_BDS
/*
   Make the all YUV output size as smallest preview size.
   Format will be set to NV21
*/
status_t ExynosCamera3Parameters::initYuvSizes() {
    int maxWidth, maxHeight = 0;
    int (*sizeList)[SIZE_OF_RESOLUTION];
    size_t lastIdx = 0;

    if (getCameraId() == CAMERA_ID_BACK) {
        lastIdx = m_staticInfo->rearPreviewListMax - 1;
        sizeList = m_staticInfo->rearPreviewList;

    } else {
        lastIdx = m_staticInfo->frontPreviewListMax - 1;
        sizeList = m_staticInfo->frontPreviewList;
    }

    for(int outputPort = 0; outputPort < getYuvStreamMaxNum(); outputPort++) {
        CLOGV("Port[%d] Idx[%d], Size(%d x %d) / true"
            , outputPort, lastIdx, sizeList[lastIdx][0], sizeList[lastIdx][1]);
        m_setYuvSize(sizeList[lastIdx][0], sizeList[lastIdx][1], outputPort);
        m_setYuvFormat(V4L2_PIX_FMT_NV21, outputPort);
    }

    return NO_ERROR;
}
#endif

status_t ExynosCamera3Parameters::resetMinYuvSize() {
    m_minYuvW = 0;
    m_minYuvH = 0;
    return OK;
}

status_t ExynosCamera3Parameters::getMinYuvSize(int* w, int* h) const {
    if(m_minYuvH == 0 || m_minYuvW == 0) {
        CLOGE(" Min YUV size is not initialized (w=%d, h=%d)"
            ,  m_minYuvW, m_minYuvH);
        return INVALID_OPERATION;
    }

    *w = m_minYuvW;
    *h = m_minYuvH;
    return OK;
}


bool ExynosCamera3Parameters::m_isSupportedPreviewSize(const int width,
                                                     const int height)
{
    int maxWidth, maxHeight = 0;
    int (*sizeList)[SIZE_OF_RESOLUTION];

    if (getHighResolutionCallbackMode() == true) {
        CLOGD(" Burst panorama mode start");
        m_cameraInfo.previewSizeRatioId = SIZE_RATIO_16_9;
        return true;
    }

    getMaxPreviewSize(&maxWidth, &maxHeight);

    if (maxWidth*maxHeight < width*height) {
        CLOGE("invalid PreviewSize(maxSize(%d/%d) size(%d/%d)",
            maxWidth, maxHeight, width, height);
        return false;
    }

    if (getCameraId() == CAMERA_ID_BACK) {
        sizeList = m_staticInfo->rearPreviewList;
        for (int i = 0; i < m_staticInfo->rearPreviewListMax; i++) {
            if (sizeList[i][0] > maxWidth || sizeList[i][1] > maxHeight)
                continue;
            if (sizeList[i][0] == width && sizeList[i][1] == height) {
                m_cameraInfo.previewSizeRatioId = sizeList[i][2];
                return true;
            }
        }
    } else {
        sizeList = m_staticInfo->frontPreviewList;
        for (int i = 0; i <  m_staticInfo->frontPreviewListMax; i++) {
            if (sizeList[i][0] > maxWidth || sizeList[i][1] > maxHeight)
                continue;
            if (sizeList[i][0] == width && sizeList[i][1] == height) {
                m_cameraInfo.previewSizeRatioId = sizeList[i][2];
                return true;
            }
        }
    }

    CLOGE("Invalid preview size(%dx%d)", width, height);

    return false;
}

#ifdef USE_BINNING_MODE
int *ExynosCamera3Parameters::getBinningSizeTable(void) {
    int *sizeList = NULL;
    int index = 0;

    if (m_staticInfo->vtcallSizeLut == NULL
        || m_staticInfo->vtcallSizeLutMax == 0) {
        CLOGE("vtcallSizeLut is NULL");
        return sizeList;
    }

    for (index = 0; index < m_staticInfo->vtcallSizeLutMax; index++) {
        if (m_staticInfo->vtcallSizeLut[index][0] == m_cameraInfo.previewSizeRatioId)
        break;
    }

    if (m_staticInfo->vtcallSizeLutMax <= index)
        index = 0;

    sizeList = m_staticInfo->vtcallSizeLut[index];

    return sizeList;
}
#endif

status_t ExynosCamera3Parameters::m_getPreviewSizeList(int *sizeList)
{
    int *tempSizeList = NULL;
    int configMode = -1;
    int videoRatioEnum = SIZE_RATIO_16_9;
    int index = 0;

#ifdef USE_BINNING_MODE
    if (getBinningMode() == true) {
        tempSizeList = getBinningSizeTable();
    } else
#endif
    {
        if (m_staticInfo->previewSizeLut == NULL) {
            CLOGE("previewSizeLut is NULL");
            return INVALID_OPERATION;
        } else if (m_staticInfo->previewSizeLutMax <= m_cameraInfo.previewSizeRatioId) {
            CLOGE("unsupported preview ratioId(%d)",
                     m_cameraInfo.previewSizeRatioId);
            return BAD_VALUE;
        }

#if defined(ENABLE_FULL_FRAME)
        tempSizeList = m_staticInfo->videoSizeLut[m_cameraInfo.previewSizeRatioId];
#else
        configMode = this->getConfigMode();
        switch (configMode) {
        case CONFIG_MODE::NORMAL:
            tempSizeList = m_staticInfo->previewSizeLut[m_cameraInfo.previewSizeRatioId];
            break;
        case CONFIG_MODE::HIGHSPEED_120:
        case CONFIG_MODE::HIGHSPEED_240:
            if (m_staticInfo->videoSizeLutHighSpeed240Max == 0
                || m_staticInfo->videoSizeLutHighSpeed240 == NULL) {
                CLOGE("videoSizeLutHighSpeed240 is NULL");
            } else {
                for (index = 0; index < m_staticInfo->videoSizeLutHighSpeed240Max; index++) {
                    if (m_staticInfo->videoSizeLutHighSpeed240[index][RATIO_ID] == videoRatioEnum) {
                        break;
                    }
                }

                if (index >= m_staticInfo->videoSizeLutHighSpeed240Max)
                    index = 0;

                tempSizeList = m_staticInfo->videoSizeLutHighSpeed240[index];
            }
            break;
        }
#endif
    }

    if (tempSizeList == NULL) {
        CLOGE("fail to get LUT");
        return INVALID_OPERATION;
    }

    for (int i = 0; i < SIZE_LUT_INDEX_END; i++)
        sizeList[i] = tempSizeList[i];

    return NO_ERROR;
}

void ExynosCamera3Parameters::m_getSWVdisPreviewSize(int w, int h, int *newW, int *newH)
{
    if (w < 0 || h < 0) {
        return;
    }

    if (w == 1920 && h == 1080) {
        *newW = 2304;
        *newH = 1296;
    }
    else if (w == 1280 && h == 720) {
        *newW = 1536;
        *newH = 864;
    }
    else {
        *newW = ALIGN_UP((w * 6) / 5, CAMERA_16PX_ALIGN);
        *newH = ALIGN_UP((h * 6) / 5, CAMERA_16PX_ALIGN);
    }
}

bool ExynosCamera3Parameters::m_isHighResolutionCallbackSize(const int width, const int height)
{
    bool highResolutionCallbackMode;

    if (width == m_staticInfo->highResolutionCallbackW && height == m_staticInfo->highResolutionCallbackH)
        highResolutionCallbackMode = true;
    else
        highResolutionCallbackMode = false;

    CLOGD("[setParameters]highResolutionCallSize:%s",
        highResolutionCallbackMode == true? "on":"off");

    m_setHighResolutionCallbackMode(highResolutionCallbackMode);

    return highResolutionCallbackMode;
}

void ExynosCamera3Parameters::m_isHighResolutionMode(const CameraParameters& params)
{
    bool highResolutionCallbackMode;
    int shotmode = params.getInt("shot-mode");

    if ((getRecordingHint() == false) && (shotmode == SHOT_MODE_PANORAMA))
        highResolutionCallbackMode = true;
    else
        highResolutionCallbackMode = false;

    CLOGD("[setParameters]highResolutionMode:%s",
        highResolutionCallbackMode == true? "on":"off");

    m_setHighResolutionCallbackMode(highResolutionCallbackMode);
}

void ExynosCamera3Parameters::m_setHighResolutionCallbackMode(bool enable)
{
    m_cameraInfo.highResolutionCallbackMode = enable;
}

bool ExynosCamera3Parameters::getHighResolutionCallbackMode(void)
{
    return m_cameraInfo.highResolutionCallbackMode;
}

status_t ExynosCamera3Parameters::checkPreviewFormat(const int streamPreviewFormat)
{
    status_t ret = NO_ERROR;
    int curPreviewFormat = -1;
    int newPreviewFormat = -1;

    newPreviewFormat = HAL_PIXEL_FORMAT_2_V4L2_PIX(streamPreviewFormat);
    curPreviewFormat = getPreviewFormat();

    if (curPreviewFormat != newPreviewFormat) {
        char curFormatName[V4L2_FOURCC_LENGTH] = {};
        char newFormatName[V4L2_FOURCC_LENGTH] = {};
        m_getV4l2Name(curFormatName, V4L2_FOURCC_LENGTH, curPreviewFormat);
        m_getV4l2Name(newFormatName, V4L2_FOURCC_LENGTH, newPreviewFormat);
        CLOGI("curPreviewFormat %s newPreviewFormat %s",
                 curFormatName, newFormatName);
        m_setPreviewFormat(newPreviewFormat);
    }

    return ret;
}

status_t ExynosCamera3Parameters::checkYuvFormat(const int format, const int outputPortId)
{
    status_t ret = NO_ERROR;
    int curYuvFormat = -1;
    int newYuvFormat = -1;

    newYuvFormat = HAL_PIXEL_FORMAT_2_V4L2_PIX(format);
    curYuvFormat = getYuvFormat(outputPortId);

    if (newYuvFormat != curYuvFormat) {
        char curFormatName[V4L2_FOURCC_LENGTH] = {};
        char newFormatName[V4L2_FOURCC_LENGTH] = {};
        m_getV4l2Name(curFormatName, V4L2_FOURCC_LENGTH, curYuvFormat);
        m_getV4l2Name(newFormatName, V4L2_FOURCC_LENGTH, newYuvFormat);
        CLOGI("curYuvFormat %s newYuvFormat %s outputPortId %d",
                curFormatName, newFormatName, outputPortId);
        m_setYuvFormat(newYuvFormat, outputPortId);
    }

    return ret;
}

void ExynosCamera3Parameters::m_setPreviewSize(int w, int h)
{
    m_cameraInfo.previewW = w;
    m_cameraInfo.previewH = h;
}

void ExynosCamera3Parameters::getPreviewSize(int *w, int *h)
{
    *w = m_cameraInfo.previewW;
    *h = m_cameraInfo.previewH;
}

void ExynosCamera3Parameters::m_setYuvSize(const int width, const int height, const int index)
{
    m_cameraInfo.yuvWidth[index] = width;
    m_cameraInfo.yuvHeight[index] = height;
}

void ExynosCamera3Parameters::getYuvSize(int *width, int *height, const int index)
{
    *width = m_cameraInfo.yuvWidth[index];
    *height = m_cameraInfo.yuvHeight[index];
}

void ExynosCamera3Parameters::getMaxSensorSize(int *w, int *h)
{
    *w = m_staticInfo->maxSensorW;
    *h = m_staticInfo->maxSensorH;
}

void ExynosCamera3Parameters::getSensorMargin(int *w, int *h)
{
    *w = m_staticInfo->sensorMarginW;
    *h = m_staticInfo->sensorMarginH;
}

void ExynosCamera3Parameters::m_adjustSensorMargin(int *sensorMarginW, int *sensorMarginH)
{
    float bnsRatio = 1.00f;
    float binningRatio = 1.00f;
    float sensorMarginRatio = 1.00f;

    bnsRatio = (float)getBnsScaleRatio() / 1000.00f;
    binningRatio = (float)getBinningScaleRatio() / 1000.00f;
    sensorMarginRatio = bnsRatio * binningRatio;
    if ((int)sensorMarginRatio < 1) {
        CLOGW("Invalid sensor margin ratio(%f), bnsRatio(%f), binningRatio(%f)",
                 sensorMarginRatio, bnsRatio, binningRatio);
        sensorMarginRatio = 1.00f;
    }

    int leftMargin = 0, rightMargin = 0, topMargin = 0, bottomMargin = 0;

    rightMargin = ALIGN_DOWN((int)(m_staticInfo->sensorMarginBase[WIDTH_BASE] / sensorMarginRatio), 2);
    leftMargin = m_staticInfo->sensorMarginBase[LEFT_BASE] + rightMargin;
    bottomMargin = ALIGN_DOWN((int)(m_staticInfo->sensorMarginBase[HEIGHT_BASE] / sensorMarginRatio), 2);
    topMargin = m_staticInfo->sensorMarginBase[TOP_BASE] + bottomMargin;

    *sensorMarginW = leftMargin + rightMargin;
    *sensorMarginH = topMargin + bottomMargin;
}

void ExynosCamera3Parameters::getMaxPreviewSize(int *w, int *h)
{
    *w = m_staticInfo->maxPreviewW;
    *h = m_staticInfo->maxPreviewH;
}

int ExynosCamera3Parameters::getBayerFormat(int pipeId)
{
    int bayerFormat = V4L2_PIX_FMT_SBGGR16;

#ifdef CAMERA_PACKED_BAYER_ENABLE
    switch (pipeId) {
    case PIPE_FLITE:
    case PIPE_VC0:
    case PIPE_3AA_REPROCESSING:
        bayerFormat = V4L2_PIX_FMT_SBGGR16;
        break;
    case PIPE_3AA:
    case PIPE_FLITE_REPROCESSING:
        bayerFormat = CAMERA_FLITE_BAYER_FORMAT;
        break;
    case PIPE_3AC:
    case PIPE_3AP:
    case PIPE_ISP:
    case PIPE_3AC_REPROCESSING:
    case PIPE_3AP_REPROCESSING:
    case PIPE_ISP_REPROCESSING:
        bayerFormat = CAMERA_3APC_BAYER_FORMAT;
        break;
    default:
        CLOGW("Invalid pipeId(%d)", pipeId);
        break;
    }
#endif

    return bayerFormat;
}

void ExynosCamera3Parameters::m_setPreviewFormat(int fmt)
{
    m_cameraInfo.previewFormat = fmt;
}

void ExynosCamera3Parameters::m_setYuvFormat(const int format, const int index)
{
    m_cameraInfo.yuvFormat[index] = format;
}

int ExynosCamera3Parameters::getPreviewFormat(void)
{
    return m_cameraInfo.previewFormat;
}

int ExynosCamera3Parameters::getYuvFormat(const int index)
{
    return m_cameraInfo.yuvFormat[index];
}

void ExynosCamera3Parameters::m_setHwPreviewSize(int w, int h)
{
    m_cameraInfo.hwPreviewW = w;
    m_cameraInfo.hwPreviewH = h;
}

void ExynosCamera3Parameters::getHwPreviewSize(int *w, int *h)
{
    getYuvSize(w, h, 0);

    if (*w <= 0 || *h <= 0)
        getMaxPreviewSize(w, h);
}

void ExynosCamera3Parameters::setHwPreviewStride(int stride)
{
    m_cameraInfo.previewStride = stride;
}

int ExynosCamera3Parameters::getHwPreviewStride(void)
{
    return m_cameraInfo.previewStride;
}

int ExynosCamera3Parameters::getHwPreviewFormat(void)
{
    CLOGV("m_cameraInfo.hwPreviewFormat(%d)", m_cameraInfo.hwPreviewFormat);

    return m_cameraInfo.hwPreviewFormat;
}

void ExynosCamera3Parameters::updateHwSensorSize(void)
{
    int curHwSensorW = 0;
    int curHwSensorH = 0;
    int newHwSensorW = 0;
    int newHwSensorH = 0;
    int maxHwSensorW = 0;
    int maxHwSensorH = 0;

    getHwSensorSize(&newHwSensorW, &newHwSensorH);
    getMaxSensorSize(&maxHwSensorW, &maxHwSensorH);

    if (newHwSensorW > maxHwSensorW || newHwSensorH > maxHwSensorH) {
        CLOGE("Invalid sensor size (maxSize(%d/%d) size(%d/%d)",
        maxHwSensorW, maxHwSensorH, newHwSensorW, newHwSensorH);
    }

    if (getHighSpeedRecording() == true) {
#if 0
        int sizeList[SIZE_LUT_INDEX_END];
        m_getHighSpeedRecordingSize(sizeList);
        newHwSensorW = sizeList[SENSOR_W];
        newHwSensorH = sizeList[SENSOR_H];
#endif
    } else if (getScalableSensorMode() == true) {
        m_getScalableSensorSize(&newHwSensorW, &newHwSensorH);
    } else {
        getBnsSize(&newHwSensorW, &newHwSensorH);
    }

    getHwSensorSize(&curHwSensorW, &curHwSensorH);
    CLOGI("curHwSensor size(%dx%d) newHwSensor size(%dx%d)", curHwSensorW, curHwSensorH, newHwSensorW, newHwSensorH);
    if (curHwSensorW != newHwSensorW || curHwSensorH != newHwSensorH) {
        m_setHwSensorSize(newHwSensorW, newHwSensorH);
        CLOGI("newHwSensor size(%dx%d)", newHwSensorW, newHwSensorH);
    }
}

void ExynosCamera3Parameters::m_setHwSensorSize(int w, int h)
{
    m_cameraInfo.hwSensorW = w;
    m_cameraInfo.hwSensorH = h;
}

void ExynosCamera3Parameters::getHwSensorSize(int *w, int *h)
{
    CLOGV("getScalableSensorMode()(%d)", getScalableSensorMode());
    int width  = 0;
    int height = 0;
    int sizeList[SIZE_LUT_INDEX_END];

    if (m_cameraInfo.scalableSensorMode != true) {
        /* matched ratio LUT is not existed, use equation */
        if (m_useSizeTable == true
            && m_staticInfo->previewSizeLut != NULL
            && m_cameraInfo.previewSizeRatioId < m_staticInfo->previewSizeLutMax
            && m_getPreviewSizeList(sizeList) == NO_ERROR) {

            width = sizeList[SENSOR_W];
            height = sizeList[SENSOR_H];

        } else {
            width  = m_cameraInfo.hwSensorW;
            height = m_cameraInfo.hwSensorH;
        }
    } else {
        m_getScalableSensorSize(&width, &height);
    }

    *w = width;
    *h = height;
}

void ExynosCamera3Parameters::updateBnsScaleRatio(void)
{
    int ret = 0;
    uint32_t bnsRatio = DEFAULT_BNS_RATIO * 1000;
    int curPreviewW = 0, curPreviewH = 0;

    if (m_staticInfo->bnsSupport == false)
        return;

    getPreviewSize(&curPreviewW, &curPreviewH);
    if (getDualMode() == true) {
#if defined(USE_BNS_DUAL_PREVIEW) || defined(USE_BNS_DUAL_RECORDING)
        bnsRatio = 2000;
#endif
    } else if ((getRecordingHint() == true)
/*    || (curPreviewW == curPreviewH)*/) {
#ifdef USE_BNS_RECORDING
        int videoW = 0, videoH = 0;
        getVideoSize(&videoW, &videoH);

        if ((getHighSpeedRecording() == true)) {
            bnsRatio = 1000;
        } else if (videoW == 1920 && videoH == 1080) {
            bnsRatio = 1500;
            CLOGI("bnsRatio(%d), videoSize (%d, %d)",
                 bnsRatio, videoW, videoH);
        } else
#endif
        {
            bnsRatio = 1000;
        }
        if (bnsRatio != getBnsScaleRatio()) {
            CLOGI(" restart set due to changing  bnsRatio(%d/%d)",
                 bnsRatio, getBnsScaleRatio());
            m_setRestartPreview(true);
        }
    }
#ifdef USE_BINNING_MODE
    else if (getBinningMode() == true) {
        bnsRatio = 1000;
    }
#endif

    if (bnsRatio != getBnsScaleRatio())
        ret = m_setBnsScaleRatio(bnsRatio);

    if (ret < 0)
        CLOGE(" Cannot update BNS scale ratio(%d)", bnsRatio);
}

status_t ExynosCamera3Parameters::m_setBnsScaleRatio(int ratio)
{
#define MIN_BNS_RATIO 1000
#define MAX_BNS_RATIO 8000

    if (m_staticInfo->bnsSupport == false) {
        CLOGD(" This camera does not support BNS");
        ratio = MIN_BNS_RATIO;
    }

    if (ratio < MIN_BNS_RATIO || ratio > MAX_BNS_RATIO) {
        CLOGE(" Out of bound, ratio(%d), min:max(%d:%d)", ratio, MAX_BNS_RATIO, MAX_BNS_RATIO);
        return BAD_VALUE;
    }

    CLOGD(" update BNS ratio(%d -> %d)", m_cameraInfo.bnsScaleRatio, ratio);

    m_cameraInfo.bnsScaleRatio = ratio;

    /* When BNS scale ratio is changed, reset BNS size to MAX sensor size */
    getMaxSensorSize(&m_cameraInfo.bnsW, &m_cameraInfo.bnsH);

    return NO_ERROR;
}

uint32_t ExynosCamera3Parameters::getBnsScaleRatio(void)
{
    return m_cameraInfo.bnsScaleRatio;
}

void ExynosCamera3Parameters::setBnsSize(int w, int h)
{
    m_cameraInfo.bnsW = w;
    m_cameraInfo.bnsH = h;

    updateHwSensorSize();

#if 0
    int zoom = getZoomLevel();
    int previewW = 0, previewH = 0;
    getPreviewSize(&previewW, &previewH);
    if (m_setParamCropRegion(zoom, w, h, previewW, previewH) != NO_ERROR)
        CLOGE("m_setParamCropRegion() fail");
#else
    ExynosRect srcRect, dstRect;
    getPreviewBayerCropSize(&srcRect, &dstRect);
#endif
}

void ExynosCamera3Parameters::getBnsSize(int *w, int *h)
{
    *w = m_cameraInfo.bnsW;
    *h = m_cameraInfo.bnsH;
}

void ExynosCamera3Parameters::updateBinningScaleRatio(void)
{
    int ret = 0;
    uint32_t binningRatio = DEFAULT_BINNING_RATIO * 1000;

    if ((getRecordingHint() == true)
        && (getHighSpeedRecording() == true)) {
        int fpsmode = 0;
        fpsmode = getFastFpsMode();
        switch (fpsmode) {
        case 1: /* 60 fps */
            binningRatio = 2000;
            break;
        case 2: /* 120 fps */
        case 3: /* 240 fps */
            binningRatio = 4000;
            break;
        default:
            CLOGE(" Invalide FastFpsMode(%d)", fpsmode);
        }
    }
#ifdef USE_BINNING_MODE
    else if (getBinningMode() == true) {
        binningRatio = 2000;
    }
#endif

    if (binningRatio != getBinningScaleRatio()) {
        CLOGI("New sensor binning ratio(%d)", binningRatio);
        ret = m_setBinningScaleRatio(binningRatio);
    }

    if (ret < 0)
        CLOGE(" Cannot update BNS scale ratio(%d)", binningRatio);
}

status_t ExynosCamera3Parameters::m_setBinningScaleRatio(int ratio)
{
#define MIN_BINNING_RATIO 1000
#define MAX_BINNING_RATIO 6000

    if (ratio < MIN_BINNING_RATIO || ratio > MAX_BINNING_RATIO) {
        CLOGE(" Out of bound, ratio(%d), min:max(%d:%d)",
                 ratio, MAX_BINNING_RATIO, MAX_BINNING_RATIO);
        return BAD_VALUE;
    }

    m_cameraInfo.binningScaleRatio = ratio;

    return NO_ERROR;
}

uint32_t ExynosCamera3Parameters::getBinningScaleRatio(void)
{
    return m_cameraInfo.binningScaleRatio;
}

status_t ExynosCamera3Parameters::checkPictureSize(int pictureW, int pictureH)
{
    int curPictureW = 0;
    int curPictureH = 0;
    int curHwPictureW = 0;
    int curHwPictureH = 0;
    int newHwPictureW = 0;
    int newHwPictureH = 0;

    if (pictureW < 0 || pictureH < 0) {
        return BAD_VALUE;
    }

    if (m_adjustPictureSize(&pictureW, &pictureH, &newHwPictureW, &newHwPictureH) != NO_ERROR) {
        return BAD_VALUE;
    }

    if (m_isSupportedPictureSize(pictureW, pictureH) == false) {
        int maxHwPictureW =0;
        int maxHwPictureH = 0;

        CLOGE("Invalid picture size(%dx%d)", pictureW, pictureH);

        /* prevent wrong size setting */
        getMaxPictureSize(&maxHwPictureW, &maxHwPictureH);
        m_setPictureSize(maxHwPictureW, maxHwPictureH);
        m_setHwPictureSize(maxHwPictureW, maxHwPictureH);

//        m_params.setPictureSize(maxHwPictureW, maxHwPictureH);

        CLOGE("changed picture size to MAX(%dx%d)", maxHwPictureW, maxHwPictureH);

#ifdef FIXED_SENSOR_SIZE
        updateHwSensorSize();
#endif
        return INVALID_OPERATION;
    }
    CLOGI("[setParameters]newPicture Size (%dx%d), ratioId(%d)",
        pictureW, pictureH, m_cameraInfo.pictureSizeRatioId);

    getPictureSize(&curPictureW, &curPictureH);
    getHwPictureSize(&curHwPictureW, &curHwPictureH);

    if (curPictureW != pictureW || curPictureH != pictureH ||
        curHwPictureW != newHwPictureW || curHwPictureH != newHwPictureH) {

        CLOGI("[setParameters]Picture size changed: cur(%dx%d) -> new(%dx%d)",
                curPictureW, curPictureH, pictureW, pictureH);
        CLOGI("[setParameters]HwPicture size changed: cur(%dx%d) -> new(%dx%d)",
                curHwPictureW, curHwPictureH, newHwPictureW, newHwPictureH);

        m_setPictureSize(pictureW, pictureH);
        m_setHwPictureSize(newHwPictureW, newHwPictureH);

#ifdef FIXED_SENSOR_SIZE
        updateHwSensorSize();
#endif
    }

    return NO_ERROR;
}

status_t ExynosCamera3Parameters::m_adjustPictureSize(int *newPictureW, int *newPictureH,
                                                 int *newHwPictureW, int *newHwPictureH)
{
    int ret = 0;
    int newX = 0, newY = 0, newW = 0, newH = 0;
    float zoomRatio = getZoomRatio(0) / 1000;

    if ((getRecordingHint() == true && getHighSpeedRecording() == true)
#ifdef USE_BINNING_MODE
        || getBinningMode()
#endif
       )
    {
       int sizeList[SIZE_LUT_INDEX_END];
       if (m_getPreviewSizeList(sizeList) == NO_ERROR) {
           *newPictureW = sizeList[TARGET_W];
           *newPictureH = sizeList[TARGET_H];
           *newHwPictureW = *newPictureW;
           *newHwPictureH = *newPictureH;

           return NO_ERROR;
       } else {
           CLOGE("m_getPreviewSizeList() fail");
           return BAD_VALUE;
       }
    }

    getMaxPictureSize(newHwPictureW, newHwPictureH);

    if (getCameraId() == CAMERA_ID_BACK) {
        ret = getCropRectAlign(*newHwPictureW, *newHwPictureH,
                *newPictureW, *newPictureH,
                &newX, &newY, &newW, &newH,
                CAMERA_BCROP_ALIGN, 2, 0, zoomRatio);
        if (ret < 0) {
            CLOGE("getCropRectAlign(%d, %d, %d, %d) fail",
                    *newHwPictureW, *newHwPictureH, *newPictureW, *newPictureH);
            return BAD_VALUE;
        }
        *newHwPictureW = newW;
        *newHwPictureH = newH;

#ifdef FIXED_SENSOR_SIZE
        /*
         * sensor crop size:
         * sensor crop is only used at 16:9 aspect ratio in picture size.
         */
        if (getSamsungCamera() == true) {
            if (((float)*newPictureW / (float)*newPictureH) == ((float)16 / (float)9)) {
                CLOGD("(%s): Use sensor crop (ratio: %f)",
                        ((float)*newPictureW / (float)*newPictureH));
                m_setHwSensorSize(newW, newH);
            }
        }
#endif
    }

    return NO_ERROR;
}

bool ExynosCamera3Parameters::m_isSupportedPictureSize(const int width,
                                                     const int height)
{
    int maxWidth, maxHeight = 0;
    int (*sizeList)[SIZE_OF_RESOLUTION];

    getMaxPictureSize(&maxWidth, &maxHeight);

    if (maxWidth < width || maxHeight < height) {
        CLOGE("invalid picture Size(maxSize(%d/%d) size(%d/%d)",
            maxWidth, maxHeight, width, height);
        return false;
    }

    if (getCameraId() == CAMERA_ID_BACK) {
        sizeList = m_staticInfo->rearPictureList;
        for (int i = 0; i < m_staticInfo->rearPictureListMax; i++) {
            if (sizeList[i][0] > maxWidth || sizeList[i][1] > maxHeight)
                continue;
            if (sizeList[i][0] == width && sizeList[i][1] == height) {
                m_cameraInfo.pictureSizeRatioId = sizeList[i][2];
                return true;
            }
        }
    } else {
        sizeList = m_staticInfo->frontPictureList;
        for (int i = 0; i < m_staticInfo->frontPictureListMax; i++) {
            if (sizeList[i][0] > maxWidth || sizeList[i][1] > maxHeight)
                continue;
            if (sizeList[i][0] == width && sizeList[i][1] == height) {
                m_cameraInfo.pictureSizeRatioId = sizeList[i][2];
                return true;
            }
        }
    }

    CLOGE("Invalid picture size(%dx%d)", width, height);

    return false;
}

void ExynosCamera3Parameters::m_setPictureSize(int w, int h)
{
    m_cameraInfo.pictureW = w;
    m_cameraInfo.pictureH = h;
}

void ExynosCamera3Parameters::getPictureSize(int *w, int *h)
{
    *w = m_cameraInfo.pictureW;
    *h = m_cameraInfo.pictureH;
}

void ExynosCamera3Parameters::getMaxPictureSize(int *w, int *h)
{
    *w = m_staticInfo->maxPictureW;
    *h = m_staticInfo->maxPictureH;
}

void ExynosCamera3Parameters::m_setHwPictureSize(int w, int h)
{
    m_cameraInfo.hwPictureW = w;
    m_cameraInfo.hwPictureH = h;
}

void ExynosCamera3Parameters::getHwPictureSize(int *w, int *h)
{
    *w = m_cameraInfo.hwPictureW;
    *h = m_cameraInfo.hwPictureH;
}

void ExynosCamera3Parameters::m_setHwBayerCropRegion(int w, int h, int x, int y)
{
    Mutex::Autolock lock(m_parameterLock);

    m_cameraInfo.hwBayerCropW = w;
    m_cameraInfo.hwBayerCropH = h;
    m_cameraInfo.hwBayerCropX = x;
    m_cameraInfo.hwBayerCropY = y;
}

void ExynosCamera3Parameters::getHwBayerCropRegion(int *w, int *h, int *x, int *y)
{
    Mutex::Autolock lock(m_parameterLock);

    *w = m_cameraInfo.hwBayerCropW;
    *h = m_cameraInfo.hwBayerCropH;
    *x = m_cameraInfo.hwBayerCropX;
    *y = m_cameraInfo.hwBayerCropY;
}

void ExynosCamera3Parameters::m_setHwPictureFormat(int fmt)
{
    m_cameraInfo.hwPictureFormat = fmt;
}

int ExynosCamera3Parameters::getHwPictureFormat(void)
{
    CLOGE("m_cameraInfo.pictureFormat(%d)", m_cameraInfo.hwPictureFormat);

    return m_cameraInfo.hwPictureFormat;
}

status_t ExynosCamera3Parameters::checkJpegQuality(int quality)
{
    int curJpegQuality = -1;

    if (quality < 0 || quality > 100) {
        CLOGE("Invalid JPEG quality %d.",
                 quality);
        return BAD_VALUE;
    }

    curJpegQuality = getJpegQuality();

    if (curJpegQuality != quality) {
        CLOGI("curJpegQuality %d newJpegQuality %d",
                 curJpegQuality, quality);
        m_setJpegQuality(quality);
    }

    return NO_ERROR;
}

void ExynosCamera3Parameters::m_setJpegQuality(int quality)
{
    m_cameraInfo.jpegQuality = quality;
}

int ExynosCamera3Parameters::getJpegQuality(void)
{
    return m_cameraInfo.jpegQuality;
}

status_t ExynosCamera3Parameters::checkThumbnailSize(int thumbnailW, int thumbnailH)
{
    int curThumbnailW = -1, curThumbnailH = -1;

    if (thumbnailW < 0 || thumbnailH < 0
        || thumbnailW > m_staticInfo->maxThumbnailW
        || thumbnailH > m_staticInfo->maxThumbnailH) {
        CLOGE("Invalide thumbnail size %dx%d",
                 thumbnailW, thumbnailH);
        return BAD_VALUE;
    }

    getThumbnailSize(&curThumbnailW, &curThumbnailH);

    if (curThumbnailW != thumbnailW || curThumbnailH != thumbnailH) {
        CLOGI("curThumbnailSize %dx%d newThumbnailSize %dx%d",
                curThumbnailW, curThumbnailH, thumbnailW, thumbnailH);
        m_setThumbnailSize(thumbnailW, thumbnailH);
    }

    return NO_ERROR;
}

void ExynosCamera3Parameters::m_setThumbnailSize(int w, int h)
{
    m_cameraInfo.thumbnailW = w;
    m_cameraInfo.thumbnailH = h;
}

void ExynosCamera3Parameters::getThumbnailSize(int *w, int *h)
{
    *w = m_cameraInfo.thumbnailW;
    *h = m_cameraInfo.thumbnailH;
}

void ExynosCamera3Parameters::getMaxThumbnailSize(int *w, int *h)
{
    *w = m_staticInfo->maxThumbnailW;
    *h = m_staticInfo->maxThumbnailH;
}

status_t ExynosCamera3Parameters::checkThumbnailQuality(int quality)
{
    int curThumbnailQuality = -1;

    if (quality < 0 || quality > 100) {
        CLOGE("Invalid thumbnail quality %d",
                 quality);
        return BAD_VALUE;
    }
    curThumbnailQuality = getThumbnailQuality();

    if (curThumbnailQuality != quality) {
        CLOGI("curThumbnailQuality %d newThumbnailQuality %d",
                 curThumbnailQuality, quality);
        m_setThumbnailQuality(quality);
    }

    return NO_ERROR;
}

void ExynosCamera3Parameters::m_setThumbnailQuality(int quality)
{
    m_cameraInfo.thumbnailQuality = quality;
}

int ExynosCamera3Parameters::getThumbnailQuality(void)
{
    return m_cameraInfo.thumbnailQuality;
}

void ExynosCamera3Parameters::m_set3dnrMode(bool toggle)
{
    m_cameraInfo.is3dnrMode = toggle;
}

bool ExynosCamera3Parameters::get3dnrMode(void)
{
    return m_cameraInfo.is3dnrMode;
}

void ExynosCamera3Parameters::m_setDrcMode(bool toggle)
{
    m_cameraInfo.isDrcMode = toggle;
    if (setDrcEnable(toggle) < 0) {
        CLOGE(" set DRC fail, toggle(%d)", toggle);
    }
}

bool ExynosCamera3Parameters::getDrcMode(void)
{
    return m_cameraInfo.isDrcMode;
}

void ExynosCamera3Parameters::m_setOdcMode(bool toggle)
{
    m_cameraInfo.isOdcMode = toggle;
}

bool ExynosCamera3Parameters::getOdcMode(void)
{
    return m_cameraInfo.isOdcMode;
}

bool ExynosCamera3Parameters::getTpuEnabledMode(void)
{
    if (getHWVdisMode() == true)
        return true;

    if (get3dnrMode() == true)
        return true;

    if (getOdcMode() == true)
        return true;

    return false;
}

status_t ExynosCamera3Parameters::setZoomLevel(int zoom)
{
    int srcW = 0;
    int srcH = 0;
    int dstW = 0;
    int dstH = 0;

    m_cameraInfo.zoom = zoom;

    getHwSensorSize(&srcW, &srcH);
    getHwPreviewSize(&dstW, &dstH);

#if 0
    if (m_setParamCropRegion(zoom, srcW, srcH, dstW, dstH) != NO_ERROR) {
        return BAD_VALUE;
    }
#else
    ExynosRect srcRect, dstRect;
    getPreviewBayerCropSize(&srcRect, &dstRect);
#endif

    return NO_ERROR;
}

status_t ExynosCamera3Parameters::setCropRegion(int x, int y, int w, int h)
{
    status_t ret = NO_ERROR;

    ret = setMetaCtlCropRegion(&m_metadata, x, y, w, h);
    if (ret != NO_ERROR) {
        CLOGE("Failed to setMetaCtlCropRegion(%d, %d, %d, %d)",
                 x, y, w, h);
    }

    return ret;
}

void ExynosCamera3Parameters::m_getCropRegion(int *x, int *y, int *w, int *h)
{
    getMetaCtlCropRegion(&m_metadata, x, y, w, h);
}

status_t ExynosCamera3Parameters::m_setParamCropRegion(
        int zoom,
        int srcW, int srcH,
        int dstW, int dstH)
{
    int newX = 0, newY = 0, newW = 0, newH = 0;
    float zoomRatio = getZoomRatio(zoom) / 1000;

    if (getCropRectAlign(srcW,  srcH,
                         dstW,  dstH,
                         &newX, &newY,
                         &newW, &newH,
                         CAMERA_BCROP_ALIGN, 2,
                         zoom, zoomRatio) != NO_ERROR) {
        CLOGE("getCropRectAlign(%d, %d, %d, %d) fail",
            srcW,  srcH, dstW,  dstH);
        return BAD_VALUE;
    }

    newX = ALIGN_UP(newX, 2);
    newY = ALIGN_UP(newY, 2);
    newW = srcW - (newX * 2);
    newH = srcH - (newY * 2);

    CLOGI("size0(%d, %d, %d, %d)",
        srcW, srcH, dstW, dstH);
    CLOGI("size(%d, %d, %d, %d), level(%d)",
        newX, newY, newW, newH, zoom);

    m_setHwBayerCropRegion(newW, newH, newX, newY);

    return NO_ERROR;
}

int ExynosCamera3Parameters::getZoomLevel(void)
{
    return m_cameraInfo.zoom;
}

void ExynosCamera3Parameters::m_setRotation(int rotation)
{
    m_cameraInfo.rotation = rotation;
}

int ExynosCamera3Parameters::getRotation(void)
{
    return m_cameraInfo.rotation;
}


int ExynosCamera3Parameters::getFocusMode(void)
{
    return m_cameraInfo.focusMode;
}

void ExynosCamera3Parameters::m_setExifFixedAttribute(void)
{
    char property[PROPERTY_VALUE_MAX];

    memset(&m_exifInfo, 0, sizeof(m_exifInfo));

    /* 2 0th IFD TIFF Tags */
    /* 3 Maker */
    property_get("ro.product.manufacturer", property, EXIF_DEF_MAKER);
    strncpy((char *)m_exifInfo.maker, property,
                sizeof(m_exifInfo.maker) - 1);
    m_exifInfo.maker[sizeof(EXIF_DEF_MAKER) - 1] = '\0';

    /* 3 Model */
    property_get("ro.product.model", property, EXIF_DEF_MODEL);
    strncpy((char *)m_exifInfo.model, property,
                sizeof(m_exifInfo.model) - 1);
    m_exifInfo.model[sizeof(m_exifInfo.model) - 1] = '\0';
    /* 3 Software */
    property_get("ro.build.PDA", property, EXIF_DEF_SOFTWARE);
    strncpy((char *)m_exifInfo.software, property,
                sizeof(m_exifInfo.software) - 1);
    m_exifInfo.software[sizeof(m_exifInfo.software) - 1] = '\0';

    /* 3 YCbCr Positioning */
    m_exifInfo.ycbcr_positioning = EXIF_DEF_YCBCR_POSITIONING;

    /*2 0th IFD Exif Private Tags */
    /* 3 Exposure Program */
    m_exifInfo.exposure_program = EXIF_DEF_EXPOSURE_PROGRAM;
    /* 3 Exif Version */
    memcpy(m_exifInfo.exif_version, EXIF_DEF_EXIF_VERSION, sizeof(m_exifInfo.exif_version));

    /* 3 Aperture */
    m_exifInfo.aperture.num = (uint32_t)(round(m_staticInfo->aperture * COMMON_DENOMINATOR));
    m_exifInfo.aperture.den = COMMON_DENOMINATOR;
    /* 3 F Number */
    m_exifInfo.fnumber.num = (uint32_t)(round(m_staticInfo->fNumber * COMMON_DENOMINATOR));
    m_exifInfo.fnumber.den = COMMON_DENOMINATOR;
    /* 3 Maximum lens aperture */
    m_exifInfo.max_aperture.num = (uint32_t)(round(m_staticInfo->aperture * COMMON_DENOMINATOR));
    m_exifInfo.max_aperture.den = COMMON_DENOMINATOR;
    /* 3 Lens Focal Length */
    m_exifInfo.focal_length.num = (uint32_t)(round(m_staticInfo->focalLength * COMMON_DENOMINATOR));
    m_exifInfo.focal_length.den = COMMON_DENOMINATOR;

    /* 3 Maker note */
    if (m_exifInfo.maker_note)
        delete m_exifInfo.maker_note;

    m_exifInfo.maker_note_size = 98;
    m_exifInfo.maker_note = new unsigned char[m_exifInfo.maker_note_size];
    memset((void *)m_exifInfo.maker_note, 0, m_exifInfo.maker_note_size);
    /* 3 User Comments */
    if (m_exifInfo.user_comment)
        delete m_exifInfo.user_comment;

    m_exifInfo.user_comment_size = sizeof("user comment");
    m_exifInfo.user_comment = new unsigned char[m_exifInfo.user_comment_size + 8];
    memset((void *)m_exifInfo.user_comment, 0, m_exifInfo.user_comment_size + 8);

    /* 3 Color Space information */
    m_exifInfo.color_space = EXIF_DEF_COLOR_SPACE;
    /* 3 interoperability */
    m_exifInfo.interoperability_index = EXIF_DEF_INTEROPERABILITY;
    /* 3 Exposure Mode */
    m_exifInfo.exposure_mode = EXIF_DEF_EXPOSURE_MODE;

    /* 2 0th IFD GPS Info Tags */
    unsigned char gps_version[4] = { 0x02, 0x02, 0x00, 0x00 };
    memcpy(m_exifInfo.gps_version_id, gps_version, sizeof(gps_version));

    /* 2 1th IFD TIFF Tags */
    m_exifInfo.compression_scheme = EXIF_DEF_COMPRESSION;
    m_exifInfo.x_resolution.num = EXIF_DEF_RESOLUTION_NUM;
    m_exifInfo.x_resolution.den = EXIF_DEF_RESOLUTION_DEN;
    m_exifInfo.y_resolution.num = EXIF_DEF_RESOLUTION_NUM;
    m_exifInfo.y_resolution.den = EXIF_DEF_RESOLUTION_DEN;
    m_exifInfo.resolution_unit = EXIF_DEF_RESOLUTION_UNIT;
}

void ExynosCamera3Parameters::setExifChangedAttribute(exif_attribute_t   *exifInfo,
                                                     ExynosRect         *pictureRect,
                                                     ExynosRect         *thumbnailRect,
                                                     camera2_shot_t     *shot)
{
    m_setExifChangedAttribute(exifInfo, pictureRect, thumbnailRect, shot);
}

void ExynosCamera3Parameters::m_setExifChangedAttribute(exif_attribute_t *exifInfo,
                                                        ExynosRect       *pictureRect,
                                                        ExynosRect       *thumbnailRect,
                                                        camera2_shot_t   *shot)
{
    /* JPEG Picture Size */
    exifInfo->width = pictureRect->w;
    exifInfo->height = pictureRect->h;

    /* Orientation */
    switch (shot->ctl.jpeg.orientation) {
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

    /* Maker Note Size */
    /* back-up udm info for exif's maker note */
#ifdef SAMSUNG_OIS
    if (getCameraId() == CAMERA_ID_BACK) {
        memcpy((void *)mDebugInfo.debugData[APP_MARKER_4], (void *)&shot->udm, sizeof(struct camera2_udm));
        getOisEXIFFromFile(m_staticInfo, (int)m_cameraInfo.oisMode);

        /* Copy ois data to debugData*/
        memcpy((void *)(mDebugInfo.debugData[APP_MARKER_4] + sizeof(struct camera2_udm)),
                (void *)&m_staticInfo->ois_exif_info, sizeof(m_staticInfo->ois_exif_info));
    } else {
        memcpy((void *)mDebugInfo.debugData[APP_MARKER_4], (void *)&shot->udm, mDebugInfo.debugSize[APP_MARKER_4]);
    }
#else
    memcpy((void *)mDebugInfo.debugData[APP_MARKER_4], (void *)&shot->udm, mDebugInfo.debugSize[APP_MARKER_4]);
#endif

    /* TODO */
#if 0
    if (getSeriesShotCount() && getShotMode() != SHOT_MODE_BEST_PHOTO) {
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
        long long int mCityId = getCityId();
        l_makernote[46] = getWeatherId();
        memcpy(l_makernote + 90, &mCityId, 8);
        exifInfo->maker_note_size = 98;
        memcpy(exifInfo->maker_note, l_makernote, sizeof(l_makernote));
    } else {
        exifInfo->maker_note_size = 0;
    }
#else
    exifInfo->maker_note_size = 0;
#endif

    /* Date Time */
    struct timeval rawtime;
    struct tm timeinfo;
    gettimeofday(&rawtime, NULL);
    localtime_r((time_t *)&rawtime.tv_sec, &timeinfo);
    strftime((char *)exifInfo->date_time, 20, "%Y:%m:%d %H:%M:%S", &timeinfo);
    snprintf((char *)exifInfo->sec_time, 5, "%04d", (int)(rawtime.tv_usec/1000));

    /* Exif Private Tags */
    bool flagSLSIAlgorithm = true;
    /*
     * vendorSpecific2[0] : info
     * vendorSpecific2[100] : 0:sirc 1:cml
     * vendorSpecific2[101] : cml exposure
     * vendorSpecific2[102] : cml iso(gain)
     * vendorSpecific2[103] : cml Bv
     */

    /* ISO Speed Rating */
#if 0 /* TODO: Must be same with the sensitivity in Result Metadata */
    exifInfo->iso_speed_rating = shot->udm.internal.vendorSpecific2[101];
#else
    if (shot->dm.sensor.sensitivity < m_staticInfo->sensitivityRange[MIN]) {
        exifInfo->iso_speed_rating = m_staticInfo->sensitivityRange[MIN];
    } else {
        exifInfo->iso_speed_rating = shot->dm.sensor.sensitivity;
    }
#endif

    /* Exposure Program */
    if (m_exposureTimeCapture == 0) {
        exifInfo->exposure_program = EXIF_DEF_EXPOSURE_PROGRAM;
    } else {
        exifInfo->exposure_program = EXIF_DEF_EXPOSURE_MANUAL;
    }

    /* Exposure Time */
    exifInfo->exposure_time.num = 1;
#if 0 /* TODO: Must be same with the exposure time in Result Metadata */
    if (shot->udm.ae.vendorSpecific[0] == 0xAEAEAEAE) {
        exifInfo->exposure_time.den = (uint32_t) shot->udm.ae.vendorSpecific[64];
    } else
#endif
    {
        /* HACK : Sometimes, F/W does NOT send the exposureTime */
        if (shot->dm.sensor.exposureTime != 0)
            exifInfo->exposure_time.den = (uint32_t)round((double)1e9 / shot->dm.sensor.exposureTime);
        else
            exifInfo->exposure_time.num = 0;
    }

    /* Shutter Speed */
    exifInfo->shutter_speed.num = (uint32_t) (ROUND_OFF_HALF(((double) (shot->udm.internal.vendorSpecific2[103] / 256.f) * EXIF_DEF_APEX_DEN), 0));
    exifInfo->shutter_speed.den = EXIF_DEF_APEX_DEN;

    /* Aperture */
    exifInfo->aperture.num = APEX_FNUM_TO_APERTURE((double) (exifInfo->fnumber.num) / (double) (exifInfo->fnumber.den)) * COMMON_DENOMINATOR;
    exifInfo->aperture.den = COMMON_DENOMINATOR;

    /* Max Aperture */
    exifInfo->max_aperture.num = APEX_FNUM_TO_APERTURE((double) (exifInfo->fnumber.num) / (double) (exifInfo->fnumber.den)) * COMMON_DENOMINATOR;
    exifInfo->max_aperture.den = COMMON_DENOMINATOR;

    /* Brightness */
    int temp = shot->udm.internal.vendorSpecific2[102];
    if ((int) shot->udm.ae.vendorSpecific[102] < 0)
        temp = -temp;
    exifInfo->brightness.num = (int32_t) (ROUND_OFF_HALF((double)((temp * EXIF_DEF_APEX_DEN)/256.f), 0));
    if ((int) shot->udm.ae.vendorSpecific[102] < 0)
        exifInfo->brightness.num = -exifInfo->brightness.num;
    exifInfo->brightness.den = EXIF_DEF_APEX_DEN;

    CLOGD("udm->internal.vendorSpecific2[100](%d)", shot->udm.internal.vendorSpecific2[100]);
    CLOGD("udm->internal.vendorSpecific2[101](%d)", shot->udm.internal.vendorSpecific2[101]);
    CLOGD("udm->internal.vendorSpecific2[102](%d)", shot->udm.internal.vendorSpecific2[102]);
    CLOGD("udm->internal.vendorSpecific2[103](%d)", shot->udm.internal.vendorSpecific2[103]);

    CLOGD("iso_speed_rating(%d)", exifInfo->iso_speed_rating);
    CLOGD("exposure_time(%d/%d)", exifInfo->exposure_time.num, exifInfo->exposure_time.den);
    CLOGD("shutter_speed(%d/%d)", exifInfo->shutter_speed.num, exifInfo->shutter_speed.den);
    CLOGD("aperture     (%d/%d)", exifInfo->aperture.num, exifInfo->aperture.den);
    CLOGD("brightness   (%d/%d)", exifInfo->brightness.num, exifInfo->brightness.den);

    /* Exposure Bias */
#if defined(USE_SUBDIVIDED_EV)
    exifInfo->exposure_bias.num = shot->ctl.aa.aeExpCompensation * (m_staticInfo->exposureCompensationStep * 10);
#else
    exifInfo->exposure_bias.num =
        (shot->ctl.aa.aeExpCompensation) * (m_staticInfo->exposureCompensationStep * 10);
#endif
    exifInfo->exposure_bias.den = 10;

    /* Metering Mode */
#ifdef SAMSUNG_COMPANION
    enum companion_wdr_mode wdr_mode;
    wdr_mode = shot->uctl.companionUd.wdr_mode;
    if (wdr_mode == COMPANION_WDR_ON) {
        exifInfo->metering_mode = EXIF_METERING_PATTERN;
    } else
#endif
    {
        switch (shot->ctl.aa.aeMode) {
        case AA_AEMODE_CENTER:
                exifInfo->metering_mode = EXIF_METERING_CENTER;
                break;
        case AA_AEMODE_MATRIX:
                exifInfo->metering_mode = EXIF_METERING_AVERAGE;
                break;
        case AA_AEMODE_SPOT:
                exifInfo->metering_mode = EXIF_METERING_SPOT;
                break;
            default:
                exifInfo->metering_mode = EXIF_METERING_AVERAGE;
                break;
        }

#ifdef SAMSUNG_FOOD_MODE
        if(shot->ctl.aa.sceneMode == AA_SCENE_MODE_FOOD) {
            exifInfo->metering_mode = EXIF_METERING_AVERAGE;
        }
#endif
    }

    /* Flash Mode */
    if (shot->ctl.flash.flashMode == CAM2_FLASH_MODE_TORCH) {
        exifInfo->flash = 1;
    } else {
        exifInfo->flash = getMarkingOfExifFlash();
    }

    /* White Balance */
    if (shot->ctl.aa.awbMode == AA_AWBMODE_WB_AUTO)
        exifInfo->white_balance = EXIF_WB_AUTO;
    else
        exifInfo->white_balance = EXIF_WB_MANUAL;

    /* Focal Length in 35mm length */
    exifInfo->focal_length_in_35mm_length = getFocalLengthIn35mmFilm();

    /* Scene Capture Type */
    switch (shot->ctl.aa.sceneMode) {
        case AA_SCENE_MODE_PORTRAIT:
        case AA_SCENE_MODE_FACE_PRIORITY:
            exifInfo->scene_capture_type = EXIF_SCENE_PORTRAIT;
            break;
        case AA_SCENE_MODE_LANDSCAPE:
            exifInfo->scene_capture_type = EXIF_SCENE_LANDSCAPE;
            break;
        case AA_SCENE_MODE_NIGHT:
            exifInfo->scene_capture_type = EXIF_SCENE_NIGHT;
            break;
        default:
            exifInfo->scene_capture_type = EXIF_SCENE_STANDARD;
            break;
    }

    switch (this->getShotMode()) {
        case SHOT_MODE_BEAUTY_FACE:
        case SHOT_MODE_BEST_FACE:
            exifInfo->scene_capture_type = EXIF_SCENE_PORTRAIT;
            break;
        default:
            break;
    }

    /* Image Unique ID */
#if defined(SAMSUNG_TN_FEATURE) && defined(SENSOR_FW_GET_FROM_FILE)
    char *front_fw = NULL;
    char *savePtr;

    if (getCameraId() == CAMERA_ID_BACK){
        memset(exifInfo->unique_id, 0, sizeof(exifInfo->unique_id));
        strncpy((char *)exifInfo->unique_id,
                getSensorFWFromFile(m_staticInfo, m_cameraId), sizeof(exifInfo->unique_id) - 1);
    } else if (getCameraId() == CAMERA_ID_FRONT) {
        front_fw = strtok_r((char *)getSensorFWFromFile(m_staticInfo, m_cameraId), " ", &savePtr);
        strcpy((char *)exifInfo->unique_id, front_fw);
    }
#endif

    /* GPS Coordinates */
    double gpsLatitude = shot->ctl.jpeg.gpsCoordinates[0];
    double gpsLongitude = shot->ctl.jpeg.gpsCoordinates[1];
    double gpsAltitude = shot->ctl.jpeg.gpsCoordinates[2];
    if (gpsLatitude != 0 && gpsLongitude != 0) {
        if (gpsLatitude > 0)
            strncpy((char *) exifInfo->gps_latitude_ref, "N", 2);
        else
            strncpy((char *) exifInfo->gps_latitude_ref, "S", 2);

        if (gpsLongitude > 0)
            strncpy((char *) exifInfo->gps_longitude_ref, "E", 2);
        else
            strncpy((char *) exifInfo->gps_longitude_ref, "W", 2);

        if (gpsAltitude > 0)
            exifInfo->gps_altitude_ref = 0;
        else
            exifInfo->gps_altitude_ref = 1;

        gpsLatitude = fabs(gpsLatitude);
        gpsLongitude = fabs(gpsLongitude);
        gpsAltitude = fabs(gpsAltitude);

        exifInfo->gps_latitude[0].num = (uint32_t) gpsLatitude;
        exifInfo->gps_latitude[0].den = 1;
        exifInfo->gps_latitude[1].num = (uint32_t)((gpsLatitude - exifInfo->gps_latitude[0].num) * 60);
        exifInfo->gps_latitude[1].den = 1;
        exifInfo->gps_latitude[2].num = (uint32_t)(round((((gpsLatitude - exifInfo->gps_latitude[0].num) * 60)
                        - exifInfo->gps_latitude[1].num) * 60));
        exifInfo->gps_latitude[2].den = 1;

        exifInfo->gps_longitude[0].num = (uint32_t)gpsLongitude;
        exifInfo->gps_longitude[0].den = 1;
        exifInfo->gps_longitude[1].num = (uint32_t)((gpsLongitude - exifInfo->gps_longitude[0].num) * 60);
        exifInfo->gps_longitude[1].den = 1;
        exifInfo->gps_longitude[2].num = (uint32_t)(round((((gpsLongitude - exifInfo->gps_longitude[0].num) * 60)
                        - exifInfo->gps_longitude[1].num) * 60));
        exifInfo->gps_longitude[2].den = 1;

        exifInfo->gps_altitude.num = (uint32_t)gpsAltitude;
        exifInfo->gps_altitude.den = 1;

        struct tm tm_data;
        long gpsTimestamp = (long) shot->ctl.jpeg.gpsTimestamp;
        gmtime_r(&gpsTimestamp, &tm_data);
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

    /* Thumbnail Size */
    exifInfo->widthThumb = thumbnailRect->w;
    exifInfo->heightThumb = thumbnailRect->h;

    setMarkingOfExifFlash(0);
}

debug_attribute_t *ExynosCamera3Parameters::getDebugAttribute(void)
{
    return &mDebugInfo;
}

status_t ExynosCamera3Parameters::getFixedExifInfo(exif_attribute_t *exifInfo)
{
    if (exifInfo == NULL) {
        CLOGE(" buffer is NULL");
        return BAD_VALUE;
    }

    memcpy(exifInfo, &m_exifInfo, sizeof(exif_attribute_t));

    return NO_ERROR;
}

#ifdef SAMSUNG_OIS
void ExynosCamera3Parameters::m_setOIS(enum  optical_stabilization_mode ois)
{
    m_cameraInfo.oisMode = ois;

    if(getZoomActiveOn()) {
        CLOGD("[setParameters]zoom moving..");
        return;
    }

#if 0 // Host controlled OIS Factory Mode
    if(m_oisNode) {
        setOISMode();
    } else {
        CLOGD("Ois node is not prepared yet(%s[%d]) !!!!");
        m_setOISmodeSetting = true;
    }
#else
    setOISMode();
#endif
}

enum optical_stabilization_mode ExynosCamera3Parameters::getOIS(void)
{
    return m_cameraInfo.oisMode;
}

void ExynosCamera3Parameters::setOISNode(ExynosCameraNode *node)
{
    m_oisNode = node;
}

void ExynosCamera3Parameters::setOISModeSetting(bool enable)
{
    m_setOISmodeSetting = enable;
}

int ExynosCamera3Parameters::getOISModeSetting(void)
{
    return m_setOISmodeSetting;
}

void ExynosCamera3Parameters::setOISMode(void)
{
    int ret = 0;

    CLOGD("set OIS Mode = %d", m_cameraInfo.oisMode);

#ifdef SAMSUNG_OIS
    setMetaCtlOIS(&m_metadata, m_cameraInfo.oisMode);
#endif

#if 0 // Host controlled OIS Factory Mode
    if (m_cameraInfo.oisMode == OPTICAL_STABILIZATION_MODE_SINE_X && m_oisNode != NULL) {
        ret = m_oisNode->setControl(V4L2_CID_CAMERA_OIS_SINE_MODE, OPTICAL_STABILIZATION_MODE_SINE_X);
        if (ret < 0) {
            CLOGE("FLITE setControl fail, ret(%d)", ret);
        }
    } else if (m_cameraInfo.oisMode == OPTICAL_STABILIZATION_MODE_SINE_Y && m_oisNode != NULL) {
        ret = m_oisNode->setControl(V4L2_CID_CAMERA_OIS_SINE_MODE, OPTICAL_STABILIZATION_MODE_SINE_Y);
        if (ret < 0) {
            CLOGE("FLITE setControl fail, ret(%d)", ret);
        }
    }
#endif
}

#endif

#ifdef SAMSUNG_COMPANION
enum companion_drc_mode ExynosCamera3Parameters::getRTDrc(void)
{
    enum companion_drc_mode mode = COMPANION_DRC_OFF;

#ifdef CAMERA_GED_FEATURE
    CLOGV("empty operation");
#else
    getMetaCtlRTDrc(&m_metadata, &mode);
#endif
    return mode;
}

enum companion_paf_mode ExynosCamera3Parameters::getPaf(void)
{
    enum companion_paf_mode mode = COMPANION_PAF_OFF;

#ifdef CAMERA_GED_FEATURE
    CLOGV("empty operation");
#else
    getMetaCtlPaf(&m_metadata, &mode);
#endif

    return mode;
}

enum companion_wdr_mode ExynosCamera3Parameters::getRTHdr(void)
{
    enum companion_wdr_mode mode = COMPANION_WDR_OFF;

#ifdef CAMERA_GED_FEATURE
    CLOGV("empty operation");
#else
    if (m_use_companion == true)
        getMetaCtlRTHdr(&m_metadata, &mode);
#endif

    return mode;
}
#endif


void ExynosCamera3Parameters::m_setHdrMode(bool hdr)
{
    m_cameraInfo.hdrMode = hdr;

#ifdef CAMERA_GED_FEATURE
    if (hdr == true)
        m_setShotMode(SHOT_MODE_RICH_TONE);
    else
        m_setShotMode(SHOT_MODE_NORMAL);
#endif

    m_activityControl->setHdrMode(hdr);
}

bool ExynosCamera3Parameters::getHdrMode(void)
{
    return m_cameraInfo.hdrMode;
}

void ExynosCamera3Parameters::m_setWdrMode(bool wdr)
{
    m_cameraInfo.wdrMode = wdr;
}


#ifdef USE_BINNING_MODE
int ExynosCamera3Parameters::getBinningMode(void)
{
    int ret = 0;

    if (m_staticInfo->vtcallSizeLutMax == 0 || m_staticInfo->vtcallSizeLut == NULL) {
       CLOGV("vtCallSizeLut is NULL, can't support the binnig mode");
       return ret;
    }

#ifdef SAMSUNG_COMPANION
    if ((getCameraId() == CAMERA_ID_FRONT) && getUseCompanion()) {
        CLOGV("Companion mode in front can't support the binning mode.(%d,%d)",
        getCameraId(), getUseCompanion());
        return ret;
    }
#endif

    /* For VT Call with DualCamera Scenario */
    if (getDualMode()) {
        CLOGV("DualMode can't support the binnig mode.(%d,%d)", getCameraId(), getDualMode());
        return ret;
    }

    if (getVtMode() != 3 && getVtMode() > 0 && getVtMode() < 5) {
        ret = 1;
    } else {
        if (m_binningProperty == true) {
            ret = 1;
        }
    }
    return ret;
}
#endif

void ExynosCamera3Parameters::m_setShotMode(int shotMode)
{
    enum aa_mode mode = AA_CONTROL_AUTO;
    enum aa_scene_mode sceneMode = AA_SCENE_MODE_FACE_PRIORITY;
    bool changeSceneMode = true;

    switch (shotMode) {
    case SHOT_MODE_DRAMA:
#ifdef SAMSUNG_DOF
    case SHOT_MODE_LIGHT_TRACE:
#endif
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_DRAMA;
        break;
#ifdef SAMSUNG_MAGICSHOT
    case SHOT_MODE_MAGIC:
        if (getCameraId() == CAMERA_ID_BACK) {
            mode = AA_CONTROL_USE_SCENE_MODE;
            sceneMode = AA_SCENE_MODE_DRAMA;
        } else {
            mode = AA_CONTROL_AUTO;
            sceneMode = AA_SCENE_MODE_FACE_PRIORITY;
        }
        break;
#endif
    case SHOT_MODE_3D_PANORAMA:
    case SHOT_MODE_PANORAMA:
    case SHOT_MODE_FRONT_PANORAMA:
    case SHOT_MODE_INTERACTIVE:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_PANORAMA;
        break;
    case SHOT_MODE_NIGHT:
    case SHOT_MODE_NIGHT_SCENE:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_LLS;
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
#ifdef SAMSUNG_TN_FEATURE
    case SHOT_MODE_SEQUENCE:
#endif
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_GOLF;
        break;
    case SHOT_MODE_NORMAL:
    case SHOT_MODE_AUTO:
    case SHOT_MODE_BEAUTY_FACE:
    case SHOT_MODE_BEST_PHOTO:
    case SHOT_MODE_BEST_FACE:
    case SHOT_MODE_ERASER:
    case SHOT_MODE_RICH_TONE:
    case SHOT_MODE_STORY:
    case SHOT_MODE_SELFIE_ALARM:
#ifdef SAMSUNG_DOF
    case SHOT_MODE_3DTOUR:
    case SHOT_MODE_OUTFOCUS:
#endif
    case SHOT_MODE_FASTMOTION:
    case SHOT_MODE_ANTI_FOG:
        mode = AA_CONTROL_AUTO;
        sceneMode = AA_SCENE_MODE_FACE_PRIORITY;
        break;
    case SHOT_MODE_DUAL:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_DUAL;
        break;
    case SHOT_MODE_AQUA:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_AQUA;
        break;
    case SHOT_MODE_AUTO_PORTRAIT:
    case SHOT_MODE_PET:
#ifdef USE_LIMITATION_FOR_THIRD_PARTY
    case THIRD_PARTY_BLACKBOX_MODE:
    case THIRD_PARTY_VTCALL_MODE:
#endif
    default:
        changeSceneMode = false;
        break;
    }

#ifdef LLS_CAPTURE
    if(m_flagLLSOn) {
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_LLS;
    }
#endif

    m_cameraInfo.shotMode = shotMode;
    if (changeSceneMode == true)
        setMetaCtlSceneMode(&m_metadata, mode, sceneMode);
}

int ExynosCamera3Parameters::getPreviewBufferCount(void)
{
    CLOGV("[setParameters]getPreviewBufferCount %d", m_previewBufferCount);

    return m_previewBufferCount;
}

void ExynosCamera3Parameters::setPreviewBufferCount(int previewBufferCount)
{
    m_previewBufferCount = previewBufferCount;

    CLOGV("[setParameters]setPreviewBufferCount %d", m_previewBufferCount);

    return;
}

int ExynosCamera3Parameters::getShotMode(void)
{
    return m_cameraInfo.shotMode;
}

void ExynosCamera3Parameters::m_setVtMode(int vtMode)
{
    m_cameraInfo.vtMode = vtMode;

    setMetaVtMode(&m_metadata, (enum camera_vt_mode)vtMode);
}

int ExynosCamera3Parameters::getVtMode(void)
{
    return m_cameraInfo.vtMode;
}

bool ExynosCamera3Parameters::isScalableSensorSupported(void)
{
    return m_staticInfo->scalableSensorSupport;
}

bool ExynosCamera3Parameters::m_adjustScalableSensorMode(const int scaleMode)
{
    bool adjustedScaleMode = false;
    int pictureW = 0;
    int pictureH = 0;
    float pictureRatio = 0;
    uint32_t minFps = 0;
    uint32_t maxFps = 0;

    /* If scale_mode is 1 or dual camera, scalable sensor turn on */
    if (scaleMode == 1)
        adjustedScaleMode = true;

    if (getDualMode() == true)
        adjustedScaleMode = true;

    /*
     * scalable sensor only support     24     fps for 4:3  - picture size
     * scalable sensor only support 15, 24, 30 fps for 16:9 - picture size
     */
    getPreviewFpsRange(&minFps, &maxFps);
    getPictureSize(&pictureW, &pictureH);

    pictureRatio = ROUND_OFF(((float)pictureW / (float)pictureH), 2);

    if (pictureRatio == 1.33f) { /* 4:3 */
        if (maxFps != 24)
            adjustedScaleMode = false;
    } else if (pictureRatio == 1.77f) { /* 16:9 */
        if ((maxFps != 15) && (maxFps != 24) && (maxFps != 30))
            adjustedScaleMode = false;
    } else {
        adjustedScaleMode = false;
    }

    if (scaleMode == 1 && adjustedScaleMode == false) {
        CLOGW("pictureRatio(%f, %d, %d) fps(%d, %d) is not proper for scalable",
                pictureRatio, pictureW, pictureH, minFps, maxFps);
    }

    return adjustedScaleMode;
}

void ExynosCamera3Parameters::setScalableSensorMode(bool scaleMode)
{
    m_cameraInfo.scalableSensorMode = scaleMode;
}

bool ExynosCamera3Parameters::getScalableSensorMode(void)
{
    return m_cameraInfo.scalableSensorMode;
}

void ExynosCamera3Parameters::m_getScalableSensorSize(int *newSensorW, int *newSensorH)
{
    int previewW = 0;
    int previewH = 0;

    *newSensorW = 1920;
    *newSensorH = 1080;

    /* default scalable sensor size is 1920x1080(16:9) */
    getPreviewSize(&previewW, &previewH);

    /* when preview size is 1440x1080(4:3), return sensor size(1920x1440) */
    /* if (previewW == 1440 && previewH == 1080) { */
    if ((previewW * 3 / 4) == previewH) {
        *newSensorW  = 1920;
        *newSensorH = 1440;
    }
}

status_t ExynosCamera3Parameters::m_setImageUniqueId(const char *uniqueId)
{
    int uniqueIdLength;

    if (uniqueId == NULL) {
        return BAD_VALUE;
    }

    memset(m_cameraInfo.imageUniqueId, 0, sizeof(m_cameraInfo.imageUniqueId));

    uniqueIdLength = strlen(uniqueId);
    memcpy(m_cameraInfo.imageUniqueId, uniqueId, uniqueIdLength);

    return NO_ERROR;
}

const char *ExynosCamera3Parameters::getImageUniqueId(void)
{
#if defined(SAMSUNG_TN_FEATURE) && defined(SENSOR_FW_GET_FROM_FILE)
    char *sensorfw = NULL;
    char *uniqueid = NULL;
#ifdef FORCE_CAL_RELOAD
    char checkcal[50];

    memset(checkcal, 0, sizeof(checkcal));
#endif
    sensorfw = (char *)getSensorFWFromFile(m_staticInfo, m_cameraId);
#ifdef FORCE_CAL_RELOAD
    snprintf(checkcal, sizeof(checkcal), "%s", sensorfw);
    m_calValid = m_checkCalibrationDataValid(checkcal);
#endif

    if (getCameraId() == CAMERA_ID_BACK) {
        uniqueid = sensorfw;
    } else {
#ifdef SAMSUNG_EEPROM_FRONT
        if (SAMSUNG_EEPROM_FRONT) {
            uniqueid = sensorfw;
        } else
#endif
        {
            uniqueid = strtok(sensorfw, " ");
        }
    }
    setImageUniqueId(uniqueid);

    return (const char *)m_cameraInfo.imageUniqueId;
#else
    return m_cameraInfo.imageUniqueId;
#endif
}

#ifdef SAMSUNG_TN_FEATURE
void ExynosCamera3Parameters::setImageUniqueId(char *uniqueId)
{
    memcpy(m_cameraInfo.imageUniqueId, uniqueId, sizeof(m_cameraInfo.imageUniqueId));
}
#endif

#ifdef SAMSUNG_DOF
int ExynosCamera3Parameters::getMoveLensTotal(void)
{
    return m_cameraInfo.lensPosTbl[0];
}

void ExynosCamera3Parameters::setMoveLensTotal(int count)
{
    m_cameraInfo.lensPosTbl[0] = count;
}

void ExynosCamera3Parameters::setMoveLensCount(int count)
{
    m_curLensCount = count;
    CLOGD("[setMoveLensCount][DOF]m_curLensCount : %d",
                m_curLensCount);
}

void ExynosCamera3Parameters::m_setLensPosition(int step)
{
    CLOGD("[m_setLensPosition][DOF]step : %d",
                step);
    setMetaCtlLensPos(&m_metadata, m_cameraInfo.lensPosTbl[step]);
    m_curLensStep = m_cameraInfo.lensPosTbl[step];
}
#endif

#ifdef BURST_CAPTURE
int ExynosCamera3Parameters::getSeriesShotSaveLocation(void)
{
    int seriesShotSaveLocation = m_seriesShotSaveLocation;
    int shotMode = getShotMode();

    /* GED's series shot work as callback */
#ifdef CAMERA_GED_FEATURE
    seriesShotSaveLocation = BURST_SAVE_CALLBACK;
#else
    if (shotMode == SHOT_MODE_BEST_PHOTO) {
        seriesShotSaveLocation = BURST_SAVE_CALLBACK;
    } else {
        if (m_seriesShotSaveLocation == 0)
            seriesShotSaveLocation = BURST_SAVE_PHONE;
        else
            seriesShotSaveLocation = BURST_SAVE_SDCARD;
    }
#endif

    return seriesShotSaveLocation;
}

void ExynosCamera3Parameters::setSeriesShotSaveLocation(int ssaveLocation)
{
    m_seriesShotSaveLocation = ssaveLocation;
}

char *ExynosCamera3Parameters::getSeriesShotFilePath(void)
{
    return m_cameraInfo.seriesShotFilePath;
}
#endif

int ExynosCamera3Parameters::getSeriesShotMode(void)
{
    return m_cameraInfo.seriesShotMode;
}

void ExynosCamera3Parameters::m_setSeriesShotCount(int seriesShotCount)
{
    m_cameraInfo.seriesShotCount = seriesShotCount;
}

int ExynosCamera3Parameters::getSeriesShotCount(void)
{
    return m_cameraInfo.seriesShotCount;
}

void ExynosCamera3Parameters::setSamsungCamera(bool value)
{
    String8 tempStr;
    ExynosCameraActivityAutofocus *autoFocusMgr = m_activityControl->getAutoFocusMgr();
    m_cameraInfo.samsungCamera = value;
}

bool ExynosCamera3Parameters::getSamsungCamera(void)
{
    return m_cameraInfo.samsungCamera;
}

bool ExynosCamera3Parameters::getZoomSupported(void)
{
    return m_staticInfo->zoomSupport;
}

int ExynosCamera3Parameters::getFocalLengthIn35mmFilm(void)
{
    return m_staticInfo->focalLengthIn35mmLength;
}

int ExynosCamera3Parameters::getMaxZoomLevel(void)
{
    return m_staticInfo->maxZoomLevel;
}

float ExynosCamera3Parameters::getMaxZoomRatio(void)
{
    return (float)m_staticInfo->maxZoomRatio;
}

float ExynosCamera3Parameters::getZoomRatio(int zoomLevel)
{
    float zoomRatio = 1.00f;
    if (getZoomSupported() == true)
        zoomRatio = (float)m_staticInfo->zoomRatioList[zoomLevel];
    else
        zoomRatio = 1000.00f;

    return zoomRatio;
}

float ExynosCamera3Parameters::getZoomRatio(void)
{
    android_printAssert(NULL, LOG_TAG, "Not yet supported, assert!!!!");
}

bool ExynosCamera3Parameters::getVideoSnapshotSupported(void)
{
    return m_staticInfo->videoSnapshotSupport;
}

bool ExynosCamera3Parameters::getVideoStabilizationSupported(void)
{
    bool supported = false;
    for (size_t i = 0; i < m_staticInfo->videoStabilizationModesLength; i++) {
        if (m_staticInfo->videoStabilizationModes[i]
            == ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_ON) {
            supported = true;
            break;
        }
    }
    return supported;
}

void ExynosCamera3Parameters::enableMsgType(int32_t msgType)
{
    Mutex::Autolock lock(m_msgLock);
    m_enabledMsgType |= msgType;
}

void ExynosCamera3Parameters::disableMsgType(int32_t msgType)
{
    Mutex::Autolock lock(m_msgLock);
    m_enabledMsgType &= ~msgType;
}

bool ExynosCamera3Parameters::msgTypeEnabled(int32_t msgType)
{
    Mutex::Autolock lock(m_msgLock);
    return (m_enabledMsgType & msgType);
}

void ExynosCamera3Parameters::m_initMetadata(void)
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
    shot->ctl.lens.aperture = m_staticInfo->aperture;
    shot->ctl.lens.focalLength = m_staticInfo->focalLength;
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

    for (size_t i = 0; i < sizeof(colorTransform)/sizeof(colorTransform[0]); i++) {
        shot->ctl.color.transform[i].num = colorTransform[i] * COMMON_DENOMINATOR;
        shot->ctl.color.transform[i].den = COMMON_DENOMINATOR;
    }

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
    /* shot->uctl.companionUd.caf_mode = COMPANION_CAF_ON; */
    /* shot->uctl.companionUd.disparity_mode = COMPANION_DISPARITY_CENSUS_CENTER; */

#ifdef SAMSUNG_COMPANION
    m_metadata.shot.uctl.companionUd.drc_mode = COMPANION_DRC_OFF;
    m_metadata.shot.uctl.companionUd.paf_mode = COMPANION_PAF_OFF;
    m_metadata.shot.uctl.companionUd.wdr_mode = COMPANION_WDR_OFF;
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

status_t ExynosCamera3Parameters::duplicateCtrlMetadata(void *buf)
{
    if (buf == NULL) {
        CLOGE("ERR: buf is NULL");
        return BAD_VALUE;
    }

    struct camera2_shot_ext *meta_shot_ext = (struct camera2_shot_ext *)buf;
    memcpy(&meta_shot_ext->shot.ctl, &m_metadata.shot.ctl, sizeof(struct camera2_ctl));
#ifdef SAMSUNG_COMPANION
    meta_shot_ext->shot.uctl.companionUd.wdr_mode = getRTHdr();
    meta_shot_ext->shot.uctl.companionUd.paf_mode = getPaf();
#endif

#ifdef SAMSUNG_DOF
    if(getShotMode() == SHOT_MODE_OUTFOCUS && m_curLensCount > 0) {
        meta_shot_ext->shot.uctl.lensUd.pos = m_metadata.shot.uctl.lensUd.pos;
        meta_shot_ext->shot.uctl.lensUd.posSize = m_metadata.shot.uctl.lensUd.posSize;
        meta_shot_ext->shot.uctl.lensUd.direction = m_metadata.shot.uctl.lensUd.direction;
        meta_shot_ext->shot.uctl.lensUd.slewRate = m_metadata.shot.uctl.lensUd.slewRate;
    }
#endif

    return NO_ERROR;
}

status_t ExynosCamera3Parameters::setFrameSkipCount(int count)
{
    m_frameSkipCounter.setCount(count);

    return NO_ERROR;
}

status_t ExynosCamera3Parameters::getFrameSkipCount(int *count)
{
    *count = m_frameSkipCounter.getCount();
    m_frameSkipCounter.decCount();

    return NO_ERROR;
}

int ExynosCamera3Parameters::getFrameSkipCount(void)
{
    return m_frameSkipCounter.getCount();
}


ExynosCameraActivityControl *ExynosCamera3Parameters::getActivityControl(void)
{
    return m_activityControl;
}

status_t ExynosCamera3Parameters::setDisEnable(bool enable)
{
    setMetaBypassDis(&m_metadata, enable == true ? 0 : 1);
    return NO_ERROR;
}

bool ExynosCamera3Parameters::getDisEnable(void)
{
    return m_metadata.dis_bypass;
}

status_t ExynosCamera3Parameters::setDrcEnable(bool enable)
{
    setMetaBypassDrc(&m_metadata, enable == true ? 0 : 1);
    return NO_ERROR;
}

bool ExynosCamera3Parameters::getDrcEnable(void)
{
    return m_metadata.drc_bypass;
}

status_t ExynosCamera3Parameters::setDnrEnable(bool enable)
{
    setMetaBypassDnr(&m_metadata, enable == true ? 0 : 1);
    return NO_ERROR;
}

bool ExynosCamera3Parameters::getDnrEnable(void)
{
    return m_metadata.dnr_bypass;
}

status_t ExynosCamera3Parameters::setFdEnable(bool enable)
{
    setMetaBypassFd(&m_metadata, enable == true ? 0 : 1);
    return NO_ERROR;
}

bool ExynosCamera3Parameters::getFdEnable(void)
{
    return m_metadata.fd_bypass;
}

status_t ExynosCamera3Parameters::setFdMode(enum facedetect_mode mode)
{
    setMetaCtlFdMode(&m_metadata, mode);
    return NO_ERROR;
}

status_t ExynosCamera3Parameters::getFdMeta(bool reprocessing, void *buf)
{
    if (buf == NULL) {
        CLOGE("ERR: buf is NULL");
        return BAD_VALUE;
    }

    struct camera2_shot_ext *meta_shot_ext = (struct camera2_shot_ext *)buf;

    /* disable face detection for reprocessing frame */
    if (reprocessing) {
        meta_shot_ext->fd_bypass = 1;
        meta_shot_ext->shot.ctl.stats.faceDetectMode = ::FACEDETECT_MODE_OFF;
    }

    return NO_ERROR;
}

void ExynosCamera3Parameters::setFlipHorizontal(int val)
{
    if (val < 0) {
        CLOGE(" setFlipHorizontal ignored, invalid value(%d)",
                 val);
        return;
    }

    m_cameraInfo.flipHorizontal = val;
}

int ExynosCamera3Parameters::getFlipHorizontal(void)
{
    if (m_cameraInfo.shotMode == SHOT_MODE_FRONT_PANORAMA) {
        return 0;
    } else {
        return m_cameraInfo.flipHorizontal;
    }
}

void ExynosCamera3Parameters::setFlipVertical(int val)
{
    if (val < 0) {
        CLOGE(" setFlipVertical ignored, invalid value(%d)",
                 val);
        return;
    }

    m_cameraInfo.flipVertical = val;
}

int ExynosCamera3Parameters::getFlipVertical(void)
{
    return m_cameraInfo.flipVertical;
}

bool ExynosCamera3Parameters::getCallbackNeedCSC(void)
{
    bool ret = true;

#if 0 /* LSI Code */
    int curShotMode = getShotMode();

    switch (curShotMode) {
    case SHOT_MODE_BEAUTY_FACE:
        ret = false;
        break;
    default:
        break;
    }
#else /* TN Code */
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
#endif
    return ret;
}

bool ExynosCamera3Parameters::getCallbackNeedCopy2Rendering(void)
{
    bool ret = false;

#if 0 /* LSI Code */
    int curShotMode = getShotMode();

    switch (curShotMode) {
    case SHOT_MODE_BEAUTY_FACE:
        ret = true;
        break;
    default:
        break;
    }
#else /* TN Code */
    int previewW = 0, previewH = 0;

    getPreviewSize(&previewW, &previewH);
    if (previewW * previewH <= 1920*1080)
        ret = true;
#endif
    return ret;
}

#ifdef LLS_CAPTURE
int ExynosCamera3Parameters::getLLS(struct camera2_shot_ext *shot)
{
#ifdef OIS_CAPTURE
    m_llsValue = shot->shot.dm.stats.vendor_LowLightMode;
#endif

#ifdef RAWDUMP_CAPTURE
    return LLS_LEVEL_ZSL;
#endif

#if defined(LLS_VALUE_VERSION_3_0)

    int ret = shot->shot.dm.stats.vendor_LowLightMode;

    if (m_cameraId == CAMERA_ID_FRONT) {
        return shot->shot.dm.stats.vendor_LowLightMode;
    }

    if(m_flagLLSOn) {
        switch (getFlashMode()) {
        case FLASH_MODE_AUTO:
            ret = shot->shot.dm.stats.vendor_LowLightMode;
            break;
        case FLASH_MODE_OFF:
            if (shot->shot.dm.stats.vendor_LowLightMode == STATE_LLS_LEVEL_HIGH)
                ret = LLS_LEVEL_LOW;
            else
                ret = shot->shot.dm.stats.vendor_LowLightMode;
            break;
        case FLASH_MODE_ON:
        case FLASH_MODE_TORCH:
        case FLASH_MODE_RED_EYE:
            ret = LLS_LEVEL_HIGH;
            break;
        default:
            ret = shot->shot.dm.stats.vendor_LowLightMode;
            break;
        }
    }else {
        switch (getFlashMode()) {
        case FLASH_MODE_AUTO:
            ret = shot->shot.dm.stats.vendor_LowLightMode;
            break;
        case FLASH_MODE_OFF:
            if (shot->shot.dm.stats.vendor_LowLightMode == STATE_LLS_LEVEL_HIGH)
                ret = LLS_LEVEL_ZSL_LIKE;
            else
                ret = shot->shot.dm.stats.vendor_LowLightMode;
            break;
        case FLASH_MODE_ON:
        case FLASH_MODE_TORCH:
        case FLASH_MODE_RED_EYE:
            ret = LLS_LEVEL_HIGH;
            break;
        default:
            ret = shot->shot.dm.stats.vendor_LowLightMode;
            break;
        }
    }

    CLOGV("m_flagLLSOn(%d) getFlashMode(%d), LowLightMode(%d)",
         m_flagLLSOn, getFlashMode(), shot->shot.dm.stats.vendor_LowLightMode);

    return ret;

#elif defined(LLS_VALUE_VERSION_2_0)

    int ret = shot->shot.dm.stats.vendor_LowLightMode;

    if (m_cameraId == CAMERA_ID_FRONT) {
        return shot->shot.dm.stats.vendor_LowLightMode;
    }

    if(m_flagLLSOn) {
        switch (getFlashMode()) {
        case FLASH_MODE_AUTO:
            ret = shot->shot.dm.stats.vendor_LowLightMode;
            break;
        case FLASH_MODE_OFF:
            if (shot->shot.dm.stats.vendor_LowLightMode == STATE_LLS_LEVEL_FLASH)
                ret = LLS_LEVEL_LOW;
            else
                ret = shot->shot.dm.stats.vendor_LowLightMode;
            break;
        case FLASH_MODE_ON:
        case FLASH_MODE_TORCH:
        case FLASH_MODE_RED_EYE:
            ret = LLS_LEVEL_HIGH;
            break;
        default:
            ret = shot->shot.dm.stats.vendor_LowLightMode;
            break;
        }
    }else {
        switch (getFlashMode()) {
        case FLASH_MODE_AUTO:
            ret = shot->shot.dm.stats.vendor_LowLightMode;
            break;
        case FLASH_MODE_OFF:
            if (shot->shot.dm.stats.vendor_LowLightMode == STATE_LLS_LEVEL_HIGH)
                ret = LLS_LEVEL_ZSL_LIKE;
            else
                ret = shot->shot.dm.stats.vendor_LowLightMode;
            break;
        case FLASH_MODE_ON:
        case FLASH_MODE_TORCH:
        case FLASH_MODE_RED_EYE:
            ret = LLS_LEVEL_HIGH;
            break;
        default:
            ret = shot->shot.dm.stats.vendor_LowLightMode;
            break;
        }
    }

    CLOGV("m_flagLLSOn(%d) getFlashMode(%d), LowLightMode(%d)",
         m_flagLLSOn, getFlashMode(), shot->shot.dm.stats.vendor_LowLightMode);

    return ret;

#else /* not defined FLASHED_LLS_CAPTURE */

    int ret = LLS_LEVEL_ZSL;

    switch (getFlashMode()) {
    case FLASH_MODE_OFF:
        if (shot->shot.dm.stats.vendor_LowLightMode == STATE_LLS_LEVEL_LOW
            || shot->shot.dm.stats.vendor_LowLightMode == STATE_LLS_LEVEL_HIGH)
            ret = LLS_LEVEL_LOW;
        else if (shot->shot.dm.stats.vendor_LowLightMode == STATE_LLS_LEVEL_SIS)
            ret = LLS_LEVEL_SIS;
        break;
    case FLASH_MODE_AUTO:
        if (shot->shot.dm.stats.vendor_LowLightMode == STATE_LLS_LEVEL_LOW)
            ret = LLS_LEVEL_LOW;
        else if (shot->shot.dm.stats.vendor_LowLightMode == STATE_LLS_LEVEL_HIGH)
            ret = LLS_LEVEL_ZSL;
        else if (shot->shot.dm.stats.vendor_LowLightMode == STATE_LLS_LEVEL_SIS)
            ret = LLS_LEVEL_SIS;
        break;
    case FLASH_MODE_ON:
    case FLASH_MODE_TORCH:
    case FLASH_MODE_RED_EYE:
    default:
    ret = LLS_LEVEL_ZSL;
        break;
    }

#if defined(LLS_NOT_USE_SIS_FRONT)
    if ((getCameraId() == CAMERA_ID_FRONT) && (ret == LLS_LEVEL_SIS)) {
        ret = LLS_LEVEL_ZSL;
    }
#endif

    CLOGV("m_flagLLSOn(%d) getFlashMode(%d), LowLightMode(%d), shotmode(%d)",
         m_flagLLSOn, getFlashMode(), shot->shot.dm.stats.vendor_LowLightMode,getShotMode());

    return ret;
#endif
}

void ExynosCamera3Parameters::setLLSOn(uint32_t enable)
{
    m_flagLLSOn = enable;
}

bool ExynosCamera3Parameters::getLLSOn(void)
{
    return m_flagLLSOn;
}

void ExynosCamera3Parameters::m_setLLSShotMode()
{
    enum aa_mode mode = AA_CONTROL_USE_SCENE_MODE;
    enum aa_scene_mode sceneMode = AA_SCENE_MODE_LLS;

    setMetaCtlSceneMode(&m_metadata, mode, sceneMode);
}

#ifdef SET_LLS_CAPTURE_SETFILE
void ExynosCamera3Parameters::setLLSCaptureOn(bool enable)
{
    m_LLSCaptureOn = enable;
}

int ExynosCamera3Parameters::getLLSCaptureOn()
{
    return m_LLSCaptureOn;
}
#endif
#endif

#ifdef SR_CAPTURE
void ExynosCamera3Parameters::setSROn(uint32_t enable)
{
    m_flagSRSOn = (enable > 0) ? true : false;
}

bool ExynosCamera3Parameters::getSROn(void)
{
    return m_flagSRSOn;
}
#endif

#ifdef OIS_CAPTURE
void ExynosCamera3Parameters::setOISCaptureModeOn(bool enable)
{
    m_flagOISCaptureOn = enable;
}

bool ExynosCamera3Parameters::getOISCaptureModeOn(void)
{
    return m_flagOISCaptureOn;
}
#endif

bool ExynosCamera3Parameters::setDeviceOrientation(int orientation)
{
    if (orientation < 0 || orientation % 90 != 0) {
        CLOGE("Invalid orientation (%d)",
                 orientation);
        return false;
    }

    m_cameraInfo.deviceOrientation = orientation;

    /* fd orientation need to be calibrated, according to f/w spec */
    int hwRotation = BACK_ROTATION;

#if 0
    if (this->getCameraId() == CAMERA_ID_FRONT)
        hwRotation = FRONT_ROTATION;
#endif

    int fdOrientation = (orientation + hwRotation) % 360;

    CLOGD("orientation(%d), hwRotation(%d), fdOrientation(%d)",
                 orientation, hwRotation, fdOrientation);

    return true;
}

int ExynosCamera3Parameters::getDeviceOrientation(void)
{
    return m_cameraInfo.deviceOrientation;
}

int ExynosCamera3Parameters::getFdOrientation(void)
{
    return (m_cameraInfo.deviceOrientation + BACK_ROTATION) % 360;;
}

void ExynosCamera3Parameters::getSetfileYuvRange(bool flagReprocessing, int *setfile, int *yuvRange)
{
    if (flagReprocessing == true) {
        *setfile = m_setfileReprocessing;
        *yuvRange = m_yuvRangeReprocessing;
    } else {
        *setfile = m_setfile;
        *yuvRange = m_yuvRange;
    }
}

status_t ExynosCamera3Parameters::checkSetfileYuvRange(void)
{
    int oldSetFile = m_setfile;
    int oldYUVRange = m_yuvRange;

    /* general */
    m_getSetfileYuvRange(false, &m_setfile, &m_yuvRange);

    /* reprocessing */
    m_getSetfileYuvRange(true, &m_setfileReprocessing, &m_yuvRangeReprocessing);

    CLOGD("m_cameraId(%d) : general[setfile(%d) YUV range(%d)] : reprocesing[setfile(%d) YUV range(%d)]",
        m_cameraId,
        m_setfile, m_yuvRange,
        m_setfileReprocessing, m_yuvRangeReprocessing);

    return NO_ERROR;
}

void ExynosCamera3Parameters::setSetfileYuvRange(void)
{
    /* reprocessing */
    m_getSetfileYuvRange(true, &m_setfileReprocessing, &m_yuvRangeReprocessing);

    CLOGD("m_cameraId(%d) : general[setfile(%d) YUV range(%d)] : reprocesing[setfile(%d) YUV range(%d)]",
        m_cameraId,
        m_setfile, m_yuvRange,
        m_setfileReprocessing, m_yuvRangeReprocessing);

}

void ExynosCamera3Parameters::setSetfileYuvRange(bool flagReprocessing, int setfile, int yuvRange)
{

    if (flagReprocessing) {
        m_setfileReprocessing = setfile;
        m_yuvRangeReprocessing = yuvRange;
    } else {
        m_setfile = setfile;
        m_yuvRange = yuvRange;
    }

    CLOGD("m_cameraId(%d) : general[setfile(%d) YUV range(%d)] : reprocesing[setfile(%d) YUV range(%d)]",
        m_cameraId,
        m_setfile, m_yuvRange,
        m_setfileReprocessing, m_yuvRangeReprocessing);

}

void ExynosCamera3Parameters::m_getSetfileYuvRange(bool flagReprocessing, int *setfile, int *yuvRange)
{
    uint32_t currentSetfile = 0;
    uint32_t stateReg = 0;
    int flagYUVRange = YUV_FULL_RANGE;

    unsigned int minFps = 0;
    unsigned int maxFps = 0;
    getPreviewFpsRange(&minFps, &maxFps);

#ifdef SAMSUNG_COMPANION
    if (getRTHdr() == COMPANION_WDR_ON) {
        stateReg |= STATE_REG_RTHDR_ON;
    } else if (getRTHdr() == COMPANION_WDR_AUTO) {
        stateReg |= STATE_REG_RTHDR_AUTO;
    }
#endif

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

    if (flagReprocessing == true)
        stateReg |= STATE_REG_FLAG_REPROCESSING;

#ifdef SET_LLS_CAPTURE_SETFILE
    if (getLLSCaptureOn())
        stateReg |= STATE_REG_NEED_LLS;
#endif

#ifdef USE_BINNING_MODE
    if (m_cameraId == CAMERA_ID_BACK && flagReprocessing == false
        && getBinningMode() == true) {
        stateReg |= STATE_REG_BINNING_MODE;
    }
#endif

#ifdef SR_CAPTURE
    int zoomLevel = getZoomLevel();
    float zoomRatio = getZoomRatio(zoomLevel) / 1000;

    if (getRecordingHint() == false && flagReprocessing == true
#ifdef SET_LLS_CAPTURE_SETFILE
        && !getLLSCaptureOn()
#endif
        ) {
        if (zoomRatio >= 3.0f && zoomRatio < 4.0f) {
            stateReg |= STATE_REG_ZOOM;
            CLOGV("currentSetfile zoom");
        } else if (zoomRatio >= 4.0f) {
            if (getSROn()) {
                stateReg |= STATE_REG_ZOOM_OUTDOOR;
                CLOGV("currentSetfile zoomoutdoor");
            } else {
                stateReg |= STATE_REG_ZOOM_INDOOR;
                CLOGV("currentSetfile zoomindoor");
            }
        }
    }
#endif

    if ((stateReg & STATE_REG_RECORDINGHINT)||
        (stateReg & STATE_REG_UHD_RECORDING)||
        (stateReg & STATE_REG_DUAL_RECORDINGHINT)) {
        if (flagReprocessing == false) {
            flagYUVRange = YUV_LIMITED_RANGE;
        }
    }

    if (m_cameraId == CAMERA_ID_FRONT) {
        int vtMode = getVtMode();

        if (0 < vtMode) {
            switch (vtMode) {
            case 1:
                currentSetfile = ISS_SUB_SCENARIO_FRONT_VT1;
                if(stateReg == STATE_STILL_CAPTURE)
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
        } else if (getShotMode() == SHOT_MODE_FRONT_PANORAMA) {
            currentSetfile = ISS_SUB_SCENARIO_FRONT_PANORAMA;
        } else {
            switch(stateReg) {
                case STATE_STILL_PREVIEW:
#if FRONT_CAMERA_USE_SAMSUNG_COMPANION
                    if (!getUseCompanion())
                        currentSetfile = ISS_SUB_SCENARIO_FRONT_C2_OFF_STILL_PREVIEW;
                    else
#endif
                        currentSetfile = ISS_SUB_SCENARIO_STILL_PREVIEW;
                    break;

                case STATE_STILL_PREVIEW_WDR_ON:
                    currentSetfile = ISS_SUB_SCENARIO_STILL_PREVIEW_WDR_ON;
                    break;

                case STATE_STILL_PREVIEW_WDR_AUTO:
                    currentSetfile = ISS_SUB_SCENARIO_STILL_PREVIEW_WDR_AUTO;
                    break;

                case STATE_VIDEO:
#if FRONT_CAMERA_USE_SAMSUNG_COMPANION
                    if (!getUseCompanion())
                        currentSetfile = ISS_SUB_SCENARIO_FRONT_C2_OFF_VIDEO;
                    else
#endif
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
#if FRONT_CAMERA_USE_SAMSUNG_COMPANION
                    if (!getUseCompanion())
                        currentSetfile = ISS_SUB_SCENARIO_FRONT_C2_OFF_STILL_CAPTURE;
                    else
#endif
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

                default:
                    CLOGD("can't define senario of setfile.(0x%4x)", stateReg);
                    break;
            }
        }
    } else {
        switch(stateReg) {
            case STATE_STILL_PREVIEW:
                if (getHighSpeedRecording() == true)
                    currentSetfile = ISS_SUB_SCENARIO_VIDEO_HIGH_SPEED;
                else
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
                currentSetfile = ISS_SUB_SCENARIO_FHD_60FPS;
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
                currentSetfile = ISS_SUB_SCENARIO_VIDEO_WDR_AUTO;
                break;

            case STATE_DUAL_VIDEO:
                currentSetfile = ISS_SUB_SCENARIO_DUAL_VIDEO;
                break;

            case STATE_DUAL_STILL_PREVIEW:
                currentSetfile = ISS_SUB_SCENARIO_DUAL_STILL;
                break;

            case STATE_UHD_PREVIEW:
            case STATE_UHD_VIDEO:
                currentSetfile = ISS_SUB_SCENARIO_UHD_30FPS;
                break;

            case STATE_UHD_PREVIEW_WDR_ON:
            case STATE_UHD_VIDEO_WDR_ON:
                currentSetfile = ISS_SUB_SCENARIO_UHD_30FPS_WDR_ON;
                break;

#ifdef SR_CAPTURE
            case STATE_STILL_CAPTURE_ZOOM:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_ZOOM;
                break;
            case STATE_STILL_CAPTURE_ZOOM_OUTDOOR:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_ZOOM_OUTDOOR;
                break;
            case STATE_STILL_CAPTURE_ZOOM_INDOOR:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_ZOOM_INDOOR;
                break;
            case STATE_STILL_CAPTURE_WDR_ON_ZOOM:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_WDR_ON_ZOOM;
                break;
            case STATE_STILL_CAPTURE_WDR_ON_ZOOM_OUTDOOR:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_WDR_ON_ZOOM_OUTDOOR;
                break;
            case STATE_STILL_CAPTURE_WDR_ON_ZOOM_INDOOR:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_WDR_ON_ZOOM_INDOOR;
                break;
            case STATE_STILL_CAPTURE_WDR_AUTO_ZOOM:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_WDR_AUTO_ZOOM;
                break;
            case STATE_STILL_CAPTURE_WDR_AUTO_ZOOM_OUTDOOR:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_WDR_AUTO_ZOOM_OUTDOOR;
                break;
            case STATE_STILL_CAPTURE_WDR_AUTO_ZOOM_INDOOR:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_WDR_AUTO_ZOOM_INDOOR;
                break;
#endif
#ifdef SET_LLS_CAPTURE_SETFILE
            case STATE_STILL_CAPTURE_LLS:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_LLS;
#ifdef LLS_REPROCESSING
                switch(getLLSCaptureCount()) {
                    case 1:
                        currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE;
                        break;
                    default:
                        break;
                }
#endif
                break;
            case STATE_VIDEO_CAPTURE_WDR_ON_LLS:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_WDR_ON_LLS;
#ifdef LLS_REPROCESSING
                switch(getLLSCaptureCount()) {
                    case 1:
                        currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_WDR_ON;
                        break;
                    default:
                        break;
                }
#endif
                break;
            case STATE_STILL_CAPTURE_WDR_AUTO_LLS:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_LLS;
#ifdef LLS_REPROCESSING
                switch(getLLSCaptureCount()) {
                    case 1:
                        currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_WDR_AUTO;
                        break;
                    default:
                        break;
                }
#endif
                break;
#endif
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
    *setfile = currentSetfile;
    *yuvRange = flagYUVRange;
}

#ifdef SAMSUNG_COMPANION
void ExynosCamera3Parameters::setUseCompanion(bool use)
{
    m_use_companion = use;
}

bool ExynosCamera3Parameters::getUseCompanion()
{
    if (m_cameraId == CAMERA_ID_FRONT && getDualMode() == true)
        m_use_companion = false;

    return m_use_companion;
}
#endif

void ExynosCamera3Parameters::setUseDynamicBayer(bool enable)
{
    m_useDynamicBayer = enable;
}

bool ExynosCamera3Parameters::getUseDynamicBayer(void)
{
    return m_useDynamicBayer;
}

void ExynosCamera3Parameters::setUseDynamicBayerVideoSnapShot(bool enable)
{
    m_useDynamicBayerVideoSnapShot = enable;
}

bool ExynosCamera3Parameters::getUseDynamicBayerVideoSnapShot(void)
{
    return m_useDynamicBayerVideoSnapShot;
}

void ExynosCamera3Parameters::setUseDynamicBayer120FpsVideoSnapShot(bool enable)
{
    m_useDynamicBayer120FpsVideoSnapShot = enable;
}

bool ExynosCamera3Parameters::getUseDynamicBayer120FpsVideoSnapShot(void)
{
    return m_useDynamicBayer120FpsVideoSnapShot;
}

void ExynosCamera3Parameters::setUseDynamicBayer240FpsVideoSnapShot(bool enable)
{
    m_useDynamicBayer240FpsVideoSnapShot = enable;
}

bool ExynosCamera3Parameters::getUseDynamicBayer240FpsVideoSnapShot(void)
{
    return m_useDynamicBayer240FpsVideoSnapShot;
}

void ExynosCamera3Parameters::setUseDynamicScc(bool enable)
{
    m_useDynamicScc = enable;
}

bool ExynosCamera3Parameters::getUseDynamicScc(void)
{
    bool dynamicScc = m_useDynamicScc;
    bool reprocessing = isReprocessing();

    if (getRecordingHint() == true && reprocessing == false)
        dynamicScc = false;

    return dynamicScc;
}

void ExynosCamera3Parameters::setUseFastenAeStable(bool enable)
{
    m_useFastenAeStable = enable;
}

bool ExynosCamera3Parameters::getUseFastenAeStable(void)
{
    return m_useFastenAeStable;
}

#ifdef SAMSUNG_LLV
void ExynosCamera3Parameters::setLLV(bool enable)
{
    m_isLLVOn = enable;
}

bool ExynosCamera3Parameters::getLLV(void)
{
    bool isSizeSupported = true;
    int videoW, videoH;

    getVideoSize(&videoW, &videoH);
    if(videoW > 1920 || videoH > 1080)
        isSizeSupported = false;
    else {
        uint32_t minFPS = 0;
        uint32_t maxFPS = 0;
        getPreviewFpsRange(&minFPS, &maxFPS);
        if(minFPS > 30 || maxFPS > 30)
            isSizeSupported = false;
    }

    return (m_isLLVOn && isSizeSupported);
}
#endif

status_t ExynosCamera3Parameters::calcHighResolutionPreviewGSCRect(ExynosRect *srcRect, ExynosRect *dstRect)
{
    int ret = 0;

    int cropX = 0, cropY = 0;
    int cropW = 0, cropH = 0;
    int crop_crop_x = 0, crop_crop_y = 0;
    int crop_crop_w = 0, crop_crop_h = 0;

    int previewW = 0, previewH = 0, previewFormat = 0;
    int pictureW = 0, pictureH = 0, pictureFormat = 0;
    previewFormat = getPreviewFormat();
    pictureFormat = getHwPictureFormat();

    getPictureSize(&pictureW, &pictureH);
    getPreviewSize(&previewW, &previewH);

    srcRect->x = 0;
    srcRect->y = 0;
    srcRect->w = pictureW;
    srcRect->h = pictureH;
    srcRect->fullW = pictureW;
    srcRect->fullH = pictureH;
    srcRect->colorFormat = pictureFormat;

    dstRect->x = 0;
    dstRect->y = 0;
    dstRect->w = previewW;
    dstRect->h = previewH;
    dstRect->fullW = previewW;
    dstRect->fullH = previewH;
    dstRect->colorFormat = previewFormat;

    return NO_ERROR;
}

status_t ExynosCamera3Parameters::calcRecordingGSCRect(ExynosRect *srcRect, ExynosRect *dstRect)
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
#ifdef SUPPORT_SW_VDIS
    if(isSWVdisMode())
        m_swVDIS_AdjustPreviewSize(&hwPreviewW, &hwPreviewH);
#endif /*SUPPORT_SW_VDIS*/
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

status_t ExynosCamera3Parameters::calcPictureRect(ExynosRect *srcRect, ExynosRect *dstRect)
{
    int ret = 0;

    int hwSensorW = 0, hwSensorH = 0;
    int hwPictureW = 0, hwPictureH = 0, hwPictureFormat = 0;
    int pictureW = 0, pictureH = 0, pictureFormat = 0;
    int previewW = 0, previewH = 0;

    int cropX = 0, cropY = 0;
    int cropW = 0, cropH = 0;
    int crop_crop_x = 0, crop_crop_y = 0;
    int crop_crop_w = 0, crop_crop_h = 0;

    int zoomLevel = 0;
    float zoomRatio = 1.0f;

    /* TODO: check state ready for start */
    pictureFormat = getHwPictureFormat();
    zoomLevel = getZoomLevel();
    getHwPictureSize(&hwPictureW, &hwPictureH);
    getPictureSize(&pictureW, &pictureH);

    getHwSensorSize(&hwSensorW, &hwSensorH);
    getPreviewSize(&previewW, &previewH);

    zoomRatio = getZoomRatio(zoomLevel) / 1000;
    /* TODO: get crop size from ctlMetadata */
    ret = getCropRectAlign(hwSensorW, hwSensorH,
                     previewW, previewH,
                     &cropX, &cropY,
                     &cropW, &cropH,
                     CAMERA_BCROP_ALIGN, 2,
                     zoomLevel, zoomRatio);

    zoomRatio = getZoomRatio(0) / 1000;
    ret = getCropRectAlign(cropW, cropH,
                     pictureW, pictureH,
                     &crop_crop_x, &crop_crop_y,
                     &crop_crop_w, &crop_crop_h,
                     2, 2,
                     0, zoomRatio);

    ALIGN_UP(crop_crop_x, 2);
    ALIGN_UP(crop_crop_y, 2);

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

    srcRect->x = crop_crop_x;
    srcRect->y = crop_crop_y;
    srcRect->w = crop_crop_w;
    srcRect->h = crop_crop_h;
    srcRect->fullW = cropW;
    srcRect->fullH = cropH;
    srcRect->colorFormat = pictureFormat;

    dstRect->x = 0;
    dstRect->y = 0;
    dstRect->w = pictureW;
    dstRect->h = pictureH;
    dstRect->fullW = pictureW;
    dstRect->fullH = pictureH;
    dstRect->colorFormat = JPEG_INPUT_COLOR_FMT;

    return NO_ERROR;
}

status_t ExynosCamera3Parameters::calcPictureRect(int originW, int originH, ExynosRect *srcRect, ExynosRect *dstRect)
{
    int ret = 0;
    int pictureW = 0, pictureH = 0, pictureFormat = 0;

    int crop_crop_x = 0, crop_crop_y = 0;
    int crop_crop_w = 0, crop_crop_h = 0;
    float zoomRatio = getZoomRatio(0) / 1000;

    /* TODO: check state ready for start */
    pictureFormat = getHwPictureFormat();
    getPictureSize(&pictureW, &pictureH);

    /* TODO: get crop size from ctlMetadata */
    ret = getCropRectAlign(originW, originH,
                     pictureW, pictureH,
                     &crop_crop_x, &crop_crop_y,
                     &crop_crop_w, &crop_crop_h,
                     2, 2,
                     0, zoomRatio);

    ALIGN_UP(crop_crop_x, 2);
    ALIGN_UP(crop_crop_y, 2);

#if 0
    CLOGD("originSize (%dx%d) pictureSize (%dx%d)",
            originW, originH, pictureW, pictureH);
    CLOGD("size2 cropX = %d, cropY = %d, cropW = %d, cropH = %d, zoom = %d",
            crop_crop_x, crop_crop_y, crop_crop_w, crop_crop_h, zoom);
    CLOGD("size pictureFormat = 0x%x, JPEG_INPUT_COLOR_FMT = 0x%x",
            pictureFormat, JPEG_INPUT_COLOR_FMT);
#endif

    srcRect->x = crop_crop_x;
    srcRect->y = crop_crop_y;
    srcRect->w = crop_crop_w;
    srcRect->h = crop_crop_h;
    srcRect->fullW = originW;
    srcRect->fullH = originH;
    srcRect->colorFormat = pictureFormat;

    dstRect->x = 0;
    dstRect->y = 0;
    dstRect->w = pictureW;
    dstRect->h = pictureH;
    dstRect->fullW = pictureW;
    dstRect->fullH = pictureH;
    dstRect->colorFormat = JPEG_INPUT_COLOR_FMT;

    return NO_ERROR;
}

status_t ExynosCamera3Parameters::getPreviewBayerCropSize(ExynosRect *srcRect, ExynosRect *dstRect, bool applyZoom)
{
    int hwBnsW   = 0;
    int hwBnsH   = 0;
    int hwBcropW = 0;
    int hwBcropH = 0;
    int sizeList[SIZE_LUT_INDEX_END];
    int hwSensorMarginW = 0;
    int hwSensorMarginH = 0;

    /* matched ratio LUT is not existed, use equation */
    if (m_useSizeTable == false
        || m_staticInfo->previewSizeLut == NULL
        || m_staticInfo->previewSizeLutMax <= m_cameraInfo.previewSizeRatioId
        || m_getPreviewSizeList(sizeList) != NO_ERROR)
        return calcPreviewBayerCropSize(srcRect, dstRect);

    /* use LUT */
    hwBnsW = sizeList[BNS_W];
    hwBnsH = sizeList[BNS_H];
    hwBcropW = sizeList[BCROP_W];
    hwBcropH = sizeList[BCROP_H];

    if (getRecordingHint() == true) {
        if (m_cameraInfo.previewSizeRatioId != m_cameraInfo.videoSizeRatioId) {
            CLOGV("preview ratioId(%d) != videoRatioId(%d), use previewRatioId",
                m_cameraInfo.previewSizeRatioId, m_cameraInfo.videoSizeRatioId);
        }
    }

    int curBnsW = 0, curBnsH = 0;
    getBnsSize(&curBnsW, &curBnsH);
    if (SIZE_RATIO(curBnsW, curBnsH) != SIZE_RATIO(hwBnsW, hwBnsH))
        CLOGW("current BNS size(%dx%d) is NOT same with Hw BNS size(%dx%d)",
                 curBnsW, curBnsH, hwBnsW, hwBnsH);

    /* Re-calculate the BNS size for removing Sensor Margin */
    getSensorMargin(&hwSensorMarginW, &hwSensorMarginH);
    m_adjustSensorMargin(&hwSensorMarginW, &hwSensorMarginH);

    hwBnsW = hwBnsW - hwSensorMarginW;
    hwBnsH = hwBnsH - hwSensorMarginH;

    /* src */
    srcRect->x = 0;
    srcRect->y = 0;
    srcRect->w = hwBnsW;
    srcRect->h = hwBnsH;

    int cropRegionX = 0, cropRegionY = 0, cropRegionW = 0, cropRegionH = 0;
    int maxSensorW = 0, maxSensorH = 0;
    float scaleRatioX = 0.0f, scaleRatioY = 0.0f;
    status_t ret = NO_ERROR;

    if (applyZoom == true && isUse3aaInputCrop() == true)
        m_getCropRegion(&cropRegionX, &cropRegionY, &cropRegionW, &cropRegionH);

    getMaxSensorSize(&maxSensorW, &maxSensorH);

    /* 1. Scale down the crop region to adjust with the bcrop input size */
    scaleRatioX = (float) hwBnsW / (float) maxSensorW;
    scaleRatioY = (float) hwBnsH / (float) maxSensorH;
    cropRegionX = (int) (cropRegionX * scaleRatioX);
    cropRegionY = (int) (cropRegionY * scaleRatioY);
    cropRegionW = (int) (cropRegionW * scaleRatioX);
    cropRegionH = (int) (cropRegionH * scaleRatioY);

    if (cropRegionW < 1 || cropRegionH < 1) {
        cropRegionW = hwBnsW;
        cropRegionH = hwBnsH;
    }

    /* 2. Calculate the real crop region with considering the target ratio */
    if ((cropRegionW > hwBcropW) && (cropRegionH > hwBcropH)) {
        dstRect->x = ALIGN_DOWN((cropRegionX + ((cropRegionW - hwBcropW) >> 1)), 2);
        dstRect->y = ALIGN_DOWN((cropRegionY + ((cropRegionH - hwBcropH) >> 1)), 2);
        dstRect->w = hwBcropW;
        dstRect->h = hwBcropH;
    } else {
        ret = getCropRectAlign(cropRegionW, cropRegionH,
                hwBcropW, hwBcropH,
                &(dstRect->x), &(dstRect->y),
                &(dstRect->w), &(dstRect->h),
                CAMERA_BCROP_ALIGN, 2,
                0, 0.0f);
        dstRect->x = ALIGN_DOWN((cropRegionX + dstRect->x), 2);
        dstRect->y = ALIGN_DOWN((cropRegionY + dstRect->y), 2);
    }

    /* 3. Compensate the crop size to satisfy Max Scale Up Ratio */
    if (dstRect->w * SCALER_MAX_SCALE_UP_RATIO < hwBnsW
        || dstRect->h * SCALER_MAX_SCALE_UP_RATIO < hwBnsH) {
        dstRect->w = ALIGN_UP((int)ceil((float)hwBnsW / SCALER_MAX_SCALE_UP_RATIO), CAMERA_BCROP_ALIGN);
        dstRect->h = ALIGN_UP((int)ceil((float)hwBnsH / SCALER_MAX_SCALE_UP_RATIO), CAMERA_BCROP_ALIGN);
    }

    /* 4. Compensate the crop size to satisfy Max Scale Up Ratio on reprocessing path */
    if (getUsePureBayerReprocessing() == false
        && m_cameraInfo.pictureSizeRatioId != m_cameraInfo.previewSizeRatioId) {
        status_t ret = NO_ERROR;
        int pictureW = 0;
        int pictureH = 0;
        ExynosRect pictureCrop;

        getPictureSize(&pictureW, &pictureH);
        if (pictureW < 1 || pictureH < 1)
            getMaxPictureSize(&pictureW, &pictureH);

        ret = getCropRectAlign(dstRect->w, dstRect->h,
                pictureW, pictureH,
                &pictureCrop.x, &pictureCrop.y,
                &pictureCrop.w, &pictureCrop.h,
                CAMERA_MCSC_ALIGN, 2,
                0, 0.0f);
        if (ret != NO_ERROR) {
            CLOGE("Failed to getCropRectAlign. previewBcrop %dx%d picture %dx%d",
                    dstRect->w, dstRect->h, pictureW, pictureH);
        }

        if (pictureCrop.w * SCALER_MAX_SCALE_UP_RATIO < pictureW
            || pictureCrop.h * SCALER_MAX_SCALE_UP_RATIO < pictureH) {
            CLOGW("Zoom ratio is upto x%d pictureCrop %dx%d pictureTarget %dx%d",
                    SCALER_MAX_SCALE_UP_RATIO,
                    pictureCrop.w, pictureCrop.h,
                    pictureW, pictureH);

            dstRect->x = cropRegionX;
            dstRect->y = cropRegionY;
            dstRect->w = cropRegionW;
            dstRect->h = cropRegionH;
        }
    }

    m_setHwBayerCropRegion(dstRect->w, dstRect->h, dstRect->x, dstRect->y);
#ifdef DEBUG_PERFRAME
    CLOGD("hwBnsSize %dx%d, hwBcropSize %d,%d %dx%d",
            srcRect->w, srcRect->h,
            dstRect->x, dstRect->y, dstRect->w, dstRect->h);
#endif

    return NO_ERROR;
}

status_t ExynosCamera3Parameters::calcPreviewBayerCropSize(ExynosRect *srcRect, ExynosRect *dstRect)
{
    status_t ret = 0;

    int maxZoomRatio = 0;
    int hwSensorW = 0, hwSensorH = 0;
    int hwPictureW = 0, hwPictureH = 0;
    int pictureW = 0, pictureH = 0;
    int previewW = 0, previewH = 0;
    int hwSensorMarginW = 0, hwSensorMarginH = 0;

    int cropX = 0, cropY = 0;
    int cropW = 0, cropH = 0;
    int bayerFormat = getBayerFormat(PIPE_3AA);

#ifdef DEBUG_RAWDUMP
    if (checkBayerDumpEnable()) {
        bayerFormat = CAMERA_DUMP_BAYER_FORMAT;
    }
#endif

    /* TODO: check state ready for start */
    maxZoomRatio = getMaxZoomRatio() / 1000;
    getHwPictureSize(&hwPictureW, &hwPictureH);
    getPictureSize(&pictureW, &pictureH);

    getHwSensorSize(&hwSensorW, &hwSensorH);
    getPreviewSize(&previewW, &previewH);
    getSensorMargin(&hwSensorMarginW, &hwSensorMarginH);
    m_adjustSensorMargin(&hwSensorMarginW, &hwSensorMarginH);

    hwSensorW -= hwSensorMarginW;
    hwSensorH -= hwSensorMarginH;

    int cropRegionX = 0, cropRegionY = 0, cropRegionW = 0, cropRegionH = 0;
    int maxSensorW = 0, maxSensorH = 0;
    float scaleRatioX = 0.0f, scaleRatioY = 0.0f;

    if (isUse3aaInputCrop() == true)
        m_getCropRegion(&cropRegionX, &cropRegionY, &cropRegionW, &cropRegionH);

    getMaxSensorSize(&maxSensorW, &maxSensorH);

    /* 1. Scale down the crop region to adjust with the bcrop input size */
    scaleRatioX = (float) hwSensorW / (float) maxSensorW;
    scaleRatioY = (float) hwSensorH / (float) maxSensorH;
    cropRegionX = (int) (cropRegionX * scaleRatioX);
    cropRegionY = (int) (cropRegionY * scaleRatioY);
    cropRegionW = (int) (cropRegionW * scaleRatioX);
    cropRegionH = (int) (cropRegionH * scaleRatioY);

    if (cropRegionW < 1 || cropRegionH < 1) {
        cropRegionW = hwSensorW;
        cropRegionH = hwSensorH;
    }

    /* 2. Calculate the real crop region with considering the target ratio */
    ret = getCropRectAlign(cropRegionW, cropRegionH,
            previewW, previewH,
            &cropX, &cropY,
            &cropW, &cropH,
            CAMERA_BCROP_ALIGN, 2,
            0, 0.0f);

    cropX = ALIGN_DOWN((cropRegionX + cropX), 2);
    cropY = ALIGN_DOWN((cropRegionY + cropY), 2);

    if (getUsePureBayerReprocessing() == false) {
        int pictureCropX = 0, pictureCropY = 0;
        int pictureCropW = 0, pictureCropH = 0;

        ret = getCropRectAlign(cropW, cropH,
                pictureW, pictureH,
                &pictureCropX, &pictureCropY,
                &pictureCropW, &pictureCropH,
                CAMERA_BCROP_ALIGN, 2,
                0, 0.0);

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

status_t ExynosCamera3Parameters::calcPreviewDzoomCropSize(ExynosRect *srcRect, ExynosRect *dstRect)
{
    int ret = 0;

    int previewW = 0, previewH = 0;
    int cropX = 0, cropY = 0;
    int cropW = 0, cropH = 0;

    int zoomLevel = 0;
    int maxZoomRatio = 0;
    float zoomRatio = getZoomRatio(0) / 1000;

    /* TODO: check state ready for start */
    zoomLevel = getZoomLevel();
    maxZoomRatio = getMaxZoomRatio() / 1000;
    getHwPreviewSize(&previewW, &previewH);
    zoomRatio = getZoomRatio(zoomLevel) / 1000;

    ret = getCropRectAlign(srcRect->w, srcRect->h,
                     previewW, previewH,
                     &srcRect->x, &srcRect->y,
                     &srcRect->w, &srcRect->h,
                     2, 2,
                     zoomLevel, zoomRatio);
    dstRect->x = 0;
    dstRect->y = 0;
    dstRect->w = previewW;
    dstRect->h = previewH;

    CLOGV("SRC cropX = %d, cropY = %d, cropW = %d, cropH = %d, zoom = %d ratio = %f", srcRect->x, srcRect->y, srcRect->w, srcRect->h, zoomLevel, zoomRatio);
    CLOGV("DST cropX = %d, cropY = %d, cropW = %d, cropH = %d, zoom = %d ratio = %f", dstRect->x, dstRect->y, dstRect->w, dstRect->h, zoomLevel, zoomRatio);

    return NO_ERROR;
}

status_t ExynosCamera3Parameters::getPictureBayerCropSize(ExynosRect *srcRect, ExynosRect *dstRect)
{
    int hwBnsW   = 0;
    int hwBnsH   = 0;
    int hwBcropW = 0;
    int hwBcropH = 0;
    int hwSensorMarginW = 0;
    int hwSensorMarginH = 0;

    /* matched ratio LUT is not existed, use equation */
    if (m_useSizeTable == false
        || m_staticInfo->pictureSizeLut == NULL
        || m_staticInfo->pictureSizeLutMax <= m_cameraInfo.pictureSizeRatioId
        || m_cameraInfo.pictureSizeRatioId != m_cameraInfo.previewSizeRatioId)
        return calcPictureBayerCropSize(srcRect, dstRect);

    /* use LUT */
    hwBnsW   = m_staticInfo->pictureSizeLut[m_cameraInfo.pictureSizeRatioId][BNS_W];
    hwBnsH   = m_staticInfo->pictureSizeLut[m_cameraInfo.pictureSizeRatioId][BNS_H];
    hwBcropW = m_staticInfo->pictureSizeLut[m_cameraInfo.pictureSizeRatioId][BCROP_W];
    hwBcropH = m_staticInfo->pictureSizeLut[m_cameraInfo.pictureSizeRatioId][BCROP_H];

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

    int cropRegionX = 0, cropRegionY = 0, cropRegionW = 0, cropRegionH = 0;
    int maxSensorW = 0, maxSensorH = 0;
    float scaleRatioX = 0.0f, scaleRatioY = 0.0f;
    status_t ret = NO_ERROR;

    if (isUseReprocessing3aaInputCrop() == true)
        m_getCropRegion(&cropRegionX, &cropRegionY, &cropRegionW, &cropRegionH);

    getMaxSensorSize(&maxSensorW, &maxSensorH);

    /* 1. Scale down the crop region to adjust with the bcrop input size */
    scaleRatioX = (float) hwBnsW / (float) maxSensorW;
    scaleRatioY = (float) hwBnsH / (float) maxSensorH;
    cropRegionX = (int) (cropRegionX * scaleRatioX);
    cropRegionY = (int) (cropRegionY * scaleRatioY);
    cropRegionW = (int) (cropRegionW * scaleRatioX);
    cropRegionH = (int) (cropRegionH * scaleRatioY);

    if (cropRegionW < 1 || cropRegionH < 1) {
        cropRegionW = hwBnsW;
        cropRegionH = hwBnsH;
    }

    /* 2. Calculate the real crop region with considering the target ratio */
    if ((cropRegionW > hwBcropW) && (cropRegionH > hwBcropH)) {
        dstRect->x = ALIGN_DOWN((cropRegionX + ((cropRegionW - hwBcropW) >> 1)), 2);
        dstRect->y = ALIGN_DOWN((cropRegionY + ((cropRegionH - hwBcropH) >> 1)), 2);
        dstRect->w = hwBcropW;
        dstRect->h = hwBcropH;
    } else {
        ret = getCropRectAlign(cropRegionW, cropRegionH,
                hwBcropW, hwBcropH,
                &(dstRect->x), &(dstRect->y),
                &(dstRect->w), &(dstRect->h),
                CAMERA_BCROP_ALIGN, 2,
                0, 0.0f);
        dstRect->x = ALIGN_DOWN((cropRegionX + dstRect->x), 2);
        dstRect->y = ALIGN_DOWN((cropRegionY + dstRect->y), 2);
    }

    /* 3. Compensate the crop size to satisfy Max Scale Up Ratio */
    if (dstRect->w * SCALER_MAX_SCALE_UP_RATIO < hwBnsW
        || dstRect->h * SCALER_MAX_SCALE_UP_RATIO < hwBnsH) {
        dstRect->w = ALIGN_UP((int)ceil((float)hwBnsW / SCALER_MAX_SCALE_UP_RATIO), CAMERA_BCROP_ALIGN);
        dstRect->h = ALIGN_UP((int)ceil((float)hwBnsH / SCALER_MAX_SCALE_UP_RATIO), CAMERA_BCROP_ALIGN);
    }

#if DEBUG_PERFRAME
    CLOGD("hwBnsSize %dx%d, hwBcropSize %d,%d %dx%d",
            srcRect->w, srcRect->h,
            dstRect->x, dstRect->y, dstRect->w, dstRect->h);
#endif

    return NO_ERROR;
}

status_t ExynosCamera3Parameters::calcPictureBayerCropSize(ExynosRect *srcRect, ExynosRect *dstRect)
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

    int maxZoomRatio = 0;
    int bayerFormat = getBayerFormat(PIPE_3AA_REPROCESSING);

#ifdef DEBUG_RAWDUMP
    if (checkBayerDumpEnable()) {
        bayerFormat = CAMERA_DUMP_BAYER_FORMAT;
    }
#endif

    /* TODO: check state ready for start */
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

    if (getUsePureBayerReprocessing() == true) {
        int cropRegionX = 0, cropRegionY = 0, cropRegionW = 0, cropRegionH = 0;
        float scaleRatioX = 0.0f, scaleRatioY = 0.0f;

        if (isUseReprocessing3aaInputCrop() == true)
            m_getCropRegion(&cropRegionX, &cropRegionY, &cropRegionW, &cropRegionH);

        /* 1. Scale down the crop region to adjust with the bcrop input size */
        scaleRatioX = (float) hwSensorW / (float) maxSensorW;
        scaleRatioY = (float) hwSensorH / (float) maxSensorH;
        cropRegionX = (int) (cropRegionX * scaleRatioX);
        cropRegionY = (int) (cropRegionY * scaleRatioY);
        cropRegionW = (int) (cropRegionW * scaleRatioX);
        cropRegionH = (int) (cropRegionH * scaleRatioY);

        if (cropRegionW < 1 || cropRegionH < 1) {
            cropRegionW = hwSensorW;
            cropRegionH = hwSensorH;
        }

        ret = getCropRectAlign(cropRegionW, cropRegionH,
                pictureW, pictureH,
                &cropX, &cropY,
                &cropW, &cropH,
                CAMERA_BCROP_ALIGN, 2,
                0, 0.0f);

        cropX = ALIGN_DOWN((cropRegionX + cropX), 2);
        cropY = ALIGN_DOWN((cropRegionY + cropY), 2);

        if (cropW < pictureW / maxZoomRatio || cropH < pictureH / maxZoomRatio) {
            CLOGW(" zoom ratio is upto x%d, crop(%dx%d), picture(%dx%d)", maxZoomRatio, cropW, cropH, pictureW, pictureH);
            cropX = ALIGN_DOWN(((hwSensorW - (pictureW / maxZoomRatio)) >> 1), 2);
            cropY = ALIGN_DOWN(((hwSensorH - (pictureH / maxZoomRatio)) >> 1), 2);
            cropW = hwSensorW - (cropX * 2);
            cropH = hwSensorH - (cropY * 2);
        }
    } else {
        getHwBayerCropRegion(&hwSensorCropW, &hwSensorCropH, &hwSensorCropX, &hwSensorCropY);

        ret = getCropRectAlign(hwSensorCropW, hwSensorCropH,
                     pictureW, pictureH,
                     &cropX, &cropY,
                     &cropW, &cropH,
                     CAMERA_BCROP_ALIGN, 2,
                     0, 0.0f);

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
    CLOGD("maxSensorSize %dx%d, hwSensorSize %dx%d, previewSize %dx%d",
            maxSensorW, maxSensorH, hwSensorW, hwSensorH, previewW, previewH);
    CLOGD("hwPictureSize %dx%d, pictureSize %dx%d",
            hwPictureW, hwPictureH, pictureW, pictureH);
    CLOGD("pictureBayerCropSize %d,%d %dx%d",
            cropX, cropY, cropW, cropH);
    CLOGD("pictureFormat 0x%x, JPEG_INPUT_COLOR_FMT 0x%x",
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

status_t ExynosCamera3Parameters::m_getPreviewBdsSize(ExynosRect *dstRect)
{
    int hwBdsW = 0;
    int hwBdsH = 0;
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

    if (getRecordingHint() == true) {
        int videoW = 0, videoH = 0;
        getVideoSize(&videoW, &videoH);

        if (m_cameraInfo.previewSizeRatioId != m_cameraInfo.videoSizeRatioId)
            CLOGV("preview ratioId(%d) != videoRatioId(%d), use previewRatioId",
                    m_cameraInfo.previewSizeRatioId, m_cameraInfo.videoSizeRatioId);

        if ((videoW == 3840 && videoH == 2160) || (videoW == 2560 && videoH == 1440)) {
            hwBdsW = videoW;
            hwBdsH = videoH;
        }
    }
#ifdef USE_BDS_WIDE_SELFIE
    else if (getShotMode() == SHOT_MODE_FRONT_PANORAMA && !getRecordingHint()) {
        hwBdsW = WIDE_SELFIE_WIDTH;
        hwBdsH = WIDE_SELFIE_HEIGHT;
    }
#endif
#ifdef USE_BDS_2_0_480P_YUV
    /* For the Exynos8890 MC-scaler :
       The image quality from MC-scaler is guaranteed only under 1/8 down scale ratio.
       (Poly : ~1/4, Bilinear : ~1/2)
       To make pass the ITS sensor fusion test,
       YUV stream with the size under 480p must be scaled down firstly at BDS with 1/2 ratio.
     */
    if (this->getCameraId() == CAMERA_ID_BACK) {
        int bdsRatio = 1;
        for (int i = 0; i < this->getYuvStreamMaxNum(); i++) {
            if (m_yuvSizeSetup[i] == true
                    && m_cameraInfo.yuvHeight[i] > 480 ) {
                bdsRatio = 1;
                break;
            } else if (m_yuvSizeSetup[i] == true) {
                bdsRatio = 2;
            }
        }
        hwBdsW = ALIGN_DOWN((hwBdsW / bdsRatio), 2);
        hwBdsH = ALIGN_DOWN((hwBdsH / bdsRatio), 2);
    }
#endif

    dstRect->x = 0;
    dstRect->y = 0;
    dstRect->w = hwBdsW;
    dstRect->h = hwBdsH;

#ifdef DEBUG_PERFRAME
    CLOGD("hwBdsSize (%dx%d)",
             dstRect->w, dstRect->h);
#endif

    return NO_ERROR;
}

status_t ExynosCamera3Parameters::getPreviewBdsSize(ExynosRect *dstRect, bool applyZoom)
{
    status_t ret = NO_ERROR;
    ExynosRect bnsSize;
    ExynosRect bayerCropSize;
    ExynosRect bdsSize;

    ret = m_getPreviewBdsSize(&bdsSize);
    if (ret != NO_ERROR) {
        CLOGE("Failed to m_getPreviewBdsSize()");
        return ret;
    }

    if (this->getHWVdisMode() == true) {
        int disW = ALIGN_UP((int)(bdsSize.w * HW_VDIS_W_RATIO), 2);
        int disH = ALIGN_UP((int)(bdsSize.h * HW_VDIS_H_RATIO), 2);

        CLOGV("HWVdis adjusted BDS Size (%d x %d) -> (%d x %d)",
             dstRect->w, dstRect->h, disW, disH);

        bdsSize.w = disW;
        bdsSize.h = disH;
    }

    /* Check the invalid BDS size compared to Bcrop size */
    ret = getPreviewBayerCropSize(&bnsSize, &bayerCropSize, applyZoom);
    if (ret != NO_ERROR)
        CLOGE("Failed to getPreviewBayerCropSize()");

    if (bayerCropSize.w < bdsSize.w || bayerCropSize.h < bdsSize.h) {
        CLOGV("bayerCropSize %dx%d is smaller than BDSSize %dx%d. Force bayerCropSize",
                bayerCropSize.w, bayerCropSize.h, bdsSize.w, bdsSize.h);

        bdsSize.w = bayerCropSize.w;
        bdsSize.h = bayerCropSize.h;
    }

#ifdef HAL3_YUVSIZE_BASED_BDS
    /*
       Do not use BDS downscaling if one or more YUV ouput size
       is larger than BDS output size
    */
    for (int i = 0; i < getYuvStreamMaxNum(); i++) {
        int yuvWidth, yuvHeight;
        getYuvSize(&yuvWidth, &yuvHeight, i);

        if(yuvWidth > bdsSize.w || yuvHeight > bdsSize.h) {
            CLOGV("Expanding BDS(%d x %d) size to BCrop(%d x %d) to handle YUV stream [%d, (%d x %d)]",
                    bdsSize.w, bdsSize.h, bayerCropSize.w, bayerCropSize.h, i, yuvWidth, yuvHeight);
            bdsSize.w = bayerCropSize.w;
            bdsSize.h = bayerCropSize.h;
            break;
        }
    }
#endif


    dstRect->x = 0;
    dstRect->y = 0;
    dstRect->w = bdsSize.w;
    dstRect->h = bdsSize.h;

#ifdef DEBUG_PERFRAME
    CLOGD("hwBdsSize %dx%d",
             dstRect->w, dstRect->h);
#endif

    return ret;
}

status_t ExynosCamera3Parameters::calcPreviewBDSSize(ExynosRect *srcRect, ExynosRect *dstRect)
{
    status_t ret = NO_ERROR;
    int previewW = 0, previewH = 0;
    int bayerFormat = getBayerFormat(PIPE_3AA);
    ExynosRect bnsSize;
    ExynosRect bayerCropSize;

    /* Get preview size info */
    getPreviewSize(&previewW, &previewH);
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
    dstRect->w = previewW;
    dstRect->h = previewH;
    dstRect->fullW = previewW;
    dstRect->fullH = previewH;
    dstRect->colorFormat = JPEG_INPUT_COLOR_FMT;

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



status_t ExynosCamera3Parameters::getPictureBdsSize(ExynosRect *dstRect)
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

    /* use LUT */
    hwBdsW = m_staticInfo->pictureSizeLut[m_cameraInfo.pictureSizeRatioId][BDS_W];
    hwBdsH = m_staticInfo->pictureSizeLut[m_cameraInfo.pictureSizeRatioId][BDS_H];

    /* Check the invalid BDS size compared to Bcrop size */
    ret = getPictureBayerCropSize(&bnsSize, &bayerCropSize);
    if (ret != NO_ERROR)
        CLOGE("Failed to getPictureBayerCropSize()");

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
    CLOGD("hwBdsSize %dx%d",
             dstRect->w, dstRect->h);
#endif

    return NO_ERROR;
}

status_t ExynosCamera3Parameters::getPreviewYuvCropSize(ExynosRect *yuvCropSize)
{
    status_t ret = NO_ERROR;
    ExynosRect previewBdsSize;
    ExynosRect previewYuvCropSize;
    ExynosRect cropRegion;
    int maxSensorW = 0, maxSensorH = 0;
    float scaleRatioX = 0.0f, scaleRatioY = 0.0f;

    /* 1. Check the invalid parameter */
    if (yuvCropSize == NULL) {
        CLOGE("yuvCropSize is NULL");
        return BAD_VALUE;
    }

    /* 2. Get the BDS info & Zoom info */
    ret = this->getPreviewBdsSize(&previewBdsSize);
    if (ret != NO_ERROR) {
        CLOGE("getPreviewBdsSize failed");
        return ret;
    }

    if (isUseIspInputCrop() == true
        || isUseMcscInputCrop() == true)
        m_getCropRegion(&cropRegion.x, &cropRegion.y, &cropRegion.w, &cropRegion.h);

    getMaxSensorSize(&maxSensorW, &maxSensorH);

    /* 3. Scale down the crop region to adjust with the original yuv size */
    scaleRatioX = (float) previewBdsSize.w / (float) maxSensorW;
    scaleRatioY = (float) previewBdsSize.h / (float) maxSensorH;
    cropRegion.x = (int) (cropRegion.x * scaleRatioX);
    cropRegion.y = (int) (cropRegion.y * scaleRatioY);
    cropRegion.w = (int) (cropRegion.w * scaleRatioX);
    cropRegion.h = (int) (cropRegion.h * scaleRatioY);

    if (cropRegion.w < 1 || cropRegion.h < 1) {
        cropRegion.w = previewBdsSize.w;
        cropRegion.h = previewBdsSize.h;
    }

    /* 4. Calculate the YuvCropSize with ZoomRatio */
#if defined(SCALER_MAX_SCALE_UP_RATIO)
    /*
     * After dividing float & casting int,
     * zoomed size can be smaller too much.
     * so, when zoom until max, ceil up about floating point.
     */
    if (ALIGN_UP(cropRegion.w, CAMERA_BCROP_ALIGN) * SCALER_MAX_SCALE_UP_RATIO < previewBdsSize.w
     || ALIGN_UP(cropRegion.h, 2) * SCALER_MAX_SCALE_UP_RATIO < previewBdsSize.h) {
        previewYuvCropSize.w = ALIGN_UP((int)ceil((float)previewBdsSize.w / SCALER_MAX_SCALE_UP_RATIO), CAMERA_BCROP_ALIGN);
        previewYuvCropSize.h = ALIGN_UP((int)ceil((float)previewBdsSize.h / SCALER_MAX_SCALE_UP_RATIO), 2);
    } else
#endif
    {
        previewYuvCropSize.w = ALIGN_UP(cropRegion.w, CAMERA_BCROP_ALIGN);
        previewYuvCropSize.h = ALIGN_UP(cropRegion.h, 2);
    }

    /* 4. Calculate the YuvCrop X-Y Offset Coordination & Set Result */
    if (previewBdsSize.w > previewYuvCropSize.w) {
        yuvCropSize->x = ALIGN_UP(((previewBdsSize.w - previewYuvCropSize.w) >> 1), 2);
        yuvCropSize->w = previewYuvCropSize.w;
    } else {
        yuvCropSize->x = 0;
        yuvCropSize->w = previewBdsSize.w;
    }
    if (previewBdsSize.h > previewYuvCropSize.h) {
        yuvCropSize->y = ALIGN_UP(((previewBdsSize.h - previewYuvCropSize.h) >> 1), 2);
        yuvCropSize->h = previewYuvCropSize.h;
    } else {
        yuvCropSize->y = 0;
        yuvCropSize->h = previewBdsSize.h;
    }

#ifdef DEBUG_PERFRAME
    CLOGD("BDS %dx%d cropRegion %d,%d %dx%d",
            previewBdsSize.w, previewBdsSize.h,
            yuvCropSize->x, yuvCropSize->y, yuvCropSize->w, yuvCropSize->h);
#endif

    return ret;
}

status_t ExynosCamera3Parameters::getPictureYuvCropSize(ExynosRect *yuvCropSize)
{
    status_t ret = NO_ERROR;
    int zoomLevel = 0;
    float zoomRatio = 1.00f;
    ExynosRect bnsSize;
    ExynosRect pictureBayerCropSize;
    ExynosRect pictureBdsSize;
    ExynosRect ispInputSize;
    ExynosRect pictureYuvCropSize;

    /* 1. Check the invalid parameter */
    if (yuvCropSize == NULL) {
        CLOGE("yuvCropSize is NULL");
        return BAD_VALUE;
    }

    /* 2. Get the ISP input info & Zoom info */
    if (this->getUsePureBayerReprocessing() == true) {
        ret = this->getPictureBdsSize(&pictureBdsSize);
        if (ret != NO_ERROR) {
            CLOGE("getPictureBdsSize failed");
            return ret;
        }

        ispInputSize.x = 0;
        ispInputSize.y = 0;
        ispInputSize.w = pictureBdsSize.w;
        ispInputSize.h = pictureBdsSize.h;
    } else {
        ret = this->getPictureBayerCropSize(&bnsSize, &pictureBayerCropSize);
        if (ret != NO_ERROR) {
            CLOGE("getPictureBdsSize failed");
            return ret;
        }

        ispInputSize.x = 0;
        ispInputSize.y = 0;
        ispInputSize.w = pictureBayerCropSize.w;
        ispInputSize.h = pictureBayerCropSize.h;
    }

    if (isUseReprocessingIspInputCrop() == true
        || isUseReprocessingMcscInputCrop() == true)
        /* TODO: Implement YUV crop for reprocessing */
        CLOGE("Picture YUV crop is NOT supported");

    /* 3. Calculate the YuvCropSize with ZoomRatio */
#if defined(SCALER_MAX_SCALE_UP_RATIO)
    /*
     * After dividing float & casting int,
     * zoomed size can be smaller too much.
     * so, when zoom until max, ceil up about floating point.
     */
    if (ALIGN_UP((int)((float)ispInputSize.w / zoomRatio), CAMERA_BCROP_ALIGN) * SCALER_MAX_SCALE_UP_RATIO < ispInputSize.w
     || ALIGN_UP((int)((float)ispInputSize.h/ zoomRatio), 2) * SCALER_MAX_SCALE_UP_RATIO < ispInputSize.h) {
        pictureYuvCropSize.w = ALIGN_UP((int)ceil((float)ispInputSize.w / zoomRatio), CAMERA_BCROP_ALIGN);
        pictureYuvCropSize.h = ALIGN_UP((int)ceil((float)ispInputSize.h / zoomRatio), 2);
    } else
#endif
    {
        pictureYuvCropSize.w = ALIGN_UP((int)((float)ispInputSize.w / zoomRatio), CAMERA_BCROP_ALIGN);
        pictureYuvCropSize.h = ALIGN_UP((int)((float)ispInputSize.h / zoomRatio), 2);
    }

    /* 4. Calculate the YuvCrop X-Y Offset Coordination & Set Result */
    if (ispInputSize.w > pictureYuvCropSize.w) {
        yuvCropSize->x = ALIGN_UP(((ispInputSize.w - pictureYuvCropSize.w) >> 1), 2);
        yuvCropSize->w = pictureYuvCropSize.w;
    } else {
        yuvCropSize->x = 0;
        yuvCropSize->w = ispInputSize.w;
    }
    if (ispInputSize.h > pictureYuvCropSize.h) {
        yuvCropSize->y = ALIGN_UP(((ispInputSize.h - pictureYuvCropSize.h) >> 1), 2);
        yuvCropSize->h = pictureYuvCropSize.h;
    } else {
        yuvCropSize->y = 0;
        yuvCropSize->h = ispInputSize.h;
    }

#ifdef DEBUG_PERFRAME
    CLOGD("ISPS %dx%d YuvCrop %d,%d %dx%d zoomLevel %d zoomRatio %f",
            ispInputSize.w, ispInputSize.h,
            yuvCropSize->x, yuvCropSize->y, yuvCropSize->w, yuvCropSize->h,
            zoomLevel, zoomRatio);
#endif

    return ret;
}


status_t ExynosCamera3Parameters::getFastenAeStableSensorSize(int *hwSensorW, int *hwSensorH)
{
    *hwSensorW = m_staticInfo->fastAeStableLut[0][SENSOR_W];
    *hwSensorH = m_staticInfo->fastAeStableLut[0][SENSOR_H];

    return NO_ERROR;
}

status_t ExynosCamera3Parameters::getFastenAeStableBcropSize(int *hwBcropW, int *hwBcropH)
{
    *hwBcropW = m_staticInfo->fastAeStableLut[0][BCROP_W];
    *hwBcropH = m_staticInfo->fastAeStableLut[0][BCROP_H];

    return NO_ERROR;
}

status_t ExynosCamera3Parameters::getFastenAeStableBdsSize(int *hwBdsW, int *hwBdsH)
{
    *hwBdsW = m_staticInfo->fastAeStableLut[0][BDS_W];
    *hwBdsH = m_staticInfo->fastAeStableLut[0][BDS_H];

    return NO_ERROR;
}

status_t ExynosCamera3Parameters::getDepthMapSize(int *depthMapW, int *depthMapH)
{
    if (m_staticInfo->depthMapSizeLut!= NULL) {
        *depthMapW = m_staticInfo->depthMapSizeLut[m_cameraInfo.previewSizeRatioId][SENSOR_W];
        *depthMapH = m_staticInfo->depthMapSizeLut[m_cameraInfo.previewSizeRatioId][SENSOR_H];
    } else {
        *depthMapW = 0;
        *depthMapH = 0;
    }

    return NO_ERROR;
}

#ifdef SUPPORT_DEPTH_MAP
bool ExynosCamera3Parameters::getUseDepthMap(void)
{
    CLOGV(" m_flaguseDepthMap(%d)", m_flaguseDepthMap);

    return m_flaguseDepthMap;
}

void ExynosCamera3Parameters::m_setUseDepthMap(bool useDepthMap)
{
    m_flaguseDepthMap = useDepthMap;
}

status_t ExynosCamera3Parameters::checkUseDepthMap(void)
{
    int depthMapW = 0, depthMapH = 0;
    getDepthMapSize(&depthMapW, &depthMapH);

    if (depthMapW != 0 && depthMapH != 0) {
        m_setUseDepthMap(true);
    } else {
        m_setUseDepthMap(false);
    }

    CLOGD(" depthMapW(%d), depthMapH (%d) getUseDepthMap(%d)",
             depthMapW, depthMapH, getUseDepthMap());

    return NO_ERROR;
}
#endif

status_t ExynosCamera3Parameters::calcPictureBDSSize(ExynosRect *srcRect, ExynosRect *dstRect)
{
    status_t ret = NO_ERROR;
    ExynosRect bnsSize;
    ExynosRect bayerCropSize;
    int pictureW = 0, pictureH = 0;
    int bayerFormat = getBayerFormat(PIPE_3AA_REPROCESSING);

#ifdef DEBUG_RAWDUMP
    if (checkBayerDumpEnable()) {
        bayerFormat = CAMERA_DUMP_BAYER_FORMAT;
    }
#endif
    getPictureSize(&pictureW, &pictureH);
    ret = getPictureBayerCropSize(&bnsSize, &bayerCropSize);
    if (ret != NO_ERROR)
        CLOGE("Failed to getPictureBayerCropSize()");

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
    CLOGE("hwBdsSize %dx%d Picture %dx%d",
            dstRect->w, dstRect->h, pictureW, pictureH);
#endif

    return NO_ERROR;
}

status_t ExynosCamera3Parameters::calcNormalToTpuSize(int srcW, int srcH, int *dstW, int *dstH)
{
    status_t ret = NO_ERROR;
    if (srcW < 0 || srcH < 0) {
        CLOGE("src size is invalid(%d x %d)", srcW, srcH);
        return INVALID_OPERATION;
    }

    int disW = ALIGN_UP((int)(srcW * HW_VDIS_W_RATIO), 2);
    int disH = ALIGN_UP((int)(srcH * HW_VDIS_H_RATIO), 2);

    *dstW = disW;
    *dstH = disH;
    CLOGD("HWVdis adjusted BDS Size (%d x %d) -> (%d x %d)",
         srcW, srcH, disW, disH);

    return NO_ERROR;
}

status_t ExynosCamera3Parameters::calcTpuToNormalSize(int srcW, int srcH, int *dstW, int *dstH)
{
    status_t ret = NO_ERROR;
    if (srcW < 0 || srcH < 0) {
        CLOGE("src size is invalid(%d x %d)", srcW, srcH);
        return INVALID_OPERATION;
    }

    int disW = ALIGN_DOWN((int)(srcW / HW_VDIS_W_RATIO), 2);
    int disH = ALIGN_DOWN((int)(srcH / HW_VDIS_H_RATIO), 2);

    *dstW = disW;
    *dstH = disH;
    CLOGD("HWVdis adjusted BDS Size (%d x %d) -> (%d x %d)",
         srcW, srcH, disW, disH);

    return ret;
}

void ExynosCamera3Parameters::setUsePureBayerReprocessing(bool enable)
{
    m_usePureBayerReprocessing = enable;
}

bool ExynosCamera3Parameters::getUsePureBayerReprocessing(void)
{
    int oldMode = m_usePureBayerReprocessing;

    if (getRecordingHint() == true) {
        if (getDualMode() == true)
            m_usePureBayerReprocessing = (getCameraId() == CAMERA_ID_BACK) ? USE_PURE_BAYER_REPROCESSING_ON_DUAL_RECORDING : USE_PURE_BAYER_REPROCESSING_FRONT_ON_DUAL_RECORDING;
        else
            m_usePureBayerReprocessing = (getCameraId() == CAMERA_ID_BACK) ? USE_PURE_BAYER_REPROCESSING_ON_RECORDING : USE_PURE_BAYER_REPROCESSING_FRONT_ON_RECORDING;
    } else {
        if (getDualMode() == true)
            m_usePureBayerReprocessing = (getCameraId() == CAMERA_ID_BACK) ? USE_PURE_BAYER_REPROCESSING_ON_DUAL : USE_PURE_BAYER_REPROCESSING_FRONT_ON_DUAL;
        else
            m_usePureBayerReprocessing = (getCameraId() == CAMERA_ID_BACK) ? USE_PURE_BAYER_REPROCESSING : USE_PURE_BAYER_REPROCESSING_FRONT;
    }

    if (oldMode != m_usePureBayerReprocessing) {
        CLOGD("bayer usage is changed (%d -> %d)", oldMode, m_usePureBayerReprocessing);
    }

    return m_usePureBayerReprocessing;
}

int32_t ExynosCamera3Parameters::getReprocessingBayerMode(void)
{
    int32_t mode = REPROCESSING_BAYER_MODE_NONE;
    bool useDynamicBayer = false;
    int configMode = getConfigMode();

    switch (configMode) {
    case CONFIG_MODE::NORMAL:
        useDynamicBayer = (getRecordingHint() == true || getDualRecordingHint() == true) ?
            getUseDynamicBayerVideoSnapShot() : getUseDynamicBayer();
        break;
    case CONFIG_MODE::HIGHSPEED_120:
        useDynamicBayer = getUseDynamicBayer120FpsVideoSnapShot();
        break;
    case CONFIG_MODE::HIGHSPEED_240:
        useDynamicBayer = getUseDynamicBayer240FpsVideoSnapShot();
        break;
    default:
        CLOGE("configMode is abnormal(%d)",
            configMode);
        break;
    }

    if (isReprocessing() == false)
        return mode;

    if (useDynamicBayer == true) {
        if (getUsePureBayerReprocessing() == true)
            mode = REPROCESSING_BAYER_MODE_PURE_DYNAMIC;
        else
            mode = REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC;
    } else {
        if (getUsePureBayerReprocessing() == true)
            mode = REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON;
        else
            mode = REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON;
    }

    return mode;
}

void ExynosCamera3Parameters::setAdaptiveCSCRecording(bool enable)
{
    m_useAdaptiveCSCRecording = enable;
}

bool ExynosCamera3Parameters::getAdaptiveCSCRecording(void)
{
    return m_useAdaptiveCSCRecording;
}

bool ExynosCamera3Parameters::doCscRecording(void)
{
    bool ret = true;
    int hwPreviewW = 0, hwPreviewH = 0;
    int videoW = 0, videoH = 0;

    getHwPreviewSize(&hwPreviewW, &hwPreviewH);
    getVideoSize(&videoW, &videoH);
    CLOGV("hwPreviewSize = %d x %d",   hwPreviewW, hwPreviewH);
    CLOGV("videoSize     = %d x %d",   videoW, videoH);

    if (((videoW == 3840 && videoH == 2160) || (videoW == 1920 && videoH == 1080) || (videoW == 2560 && videoH == 1440))
        && m_useAdaptiveCSCRecording == true
        && videoW == hwPreviewW
        && videoH == hwPreviewH) {
        ret = false;
    }

    return ret;
}

int ExynosCamera3Parameters::getHalPixelFormat(void)
{
    int setfile = 0;
    int yuvRange = 0;
    int previewFormat = getHwPreviewFormat();
    int halFormat = 0;

    m_getSetfileYuvRange(false, &setfile, &yuvRange);

    halFormat = convertingHalPreviewFormat(previewFormat, yuvRange);

    return halFormat;
}

#if (TARGET_ANDROID_VER_MAJ >= 4 && TARGET_ANDROID_VER_MIN >= 4)
int ExynosCamera3Parameters::convertingHalPreviewFormat(int previewFormat, int yuvRange)
{
    int halFormat = 0;

    switch (previewFormat) {
    case V4L2_PIX_FMT_NV21:
        CLOGD(" preview format NV21");
        halFormat = HAL_PIXEL_FORMAT_YCrCb_420_SP;
        break;
    case V4L2_PIX_FMT_NV21M:
        CLOGD(" preview format NV21M");
        if (yuvRange == YUV_FULL_RANGE) {
            halFormat = HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M_FULL;
        } else if (yuvRange == YUV_LIMITED_RANGE) {
            halFormat = HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M;
        } else {
            CLOGW(" invalid yuvRange, force set to full range");
            halFormat = HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M_FULL;
        }
        break;
    case V4L2_PIX_FMT_YVU420:
        CLOGD(" preview format YVU420");
        halFormat = HAL_PIXEL_FORMAT_YV12;
        break;
    case V4L2_PIX_FMT_YVU420M:
        CLOGD(" preview format YVU420M");
        halFormat = HAL_PIXEL_FORMAT_EXYNOS_YV12_M;
        break;
    default:
        CLOGE(" unknown preview format(%d)", previewFormat);
        break;
    }

    return halFormat;
}
#else
int ExynosCamera3Parameters::convertingHalPreviewFormat(int previewFormat, int yuvRange)
{
    int halFormat = 0;

    switch (previewFormat) {
    case V4L2_PIX_FMT_NV21:
        halFormat = HAL_PIXEL_FORMAT_YCrCb_420_SP;
        break;
    case V4L2_PIX_FMT_NV21M:
        if (yuvRange == YUV_FULL_RANGE) {
            halFormat = HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_FULL;
        } else if (yuvRange == YUV_LIMITED_RANGE) {
            halFormat = HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP;
        } else {
            CLOGW(" invalid yuvRange, force set to full range");
            halFormat = HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_FULL;
        }
        break;
    case V4L2_PIX_FMT_YVU420:
        halFormat = HAL_PIXEL_FORMAT_YV12;
        break;
    case V4L2_PIX_FMT_YVU420M:
        halFormat = HAL_PIXEL_FORMAT_EXYNOS_YV12;
        break;
    default:
        CLOGE(" unknown preview format(%d)", previewFormat);
        break;
    }

    return halFormat;
}
#endif

void ExynosCamera3Parameters::setDvfsLock(bool lock) {
    m_dvfsLock = lock;
}

bool ExynosCamera3Parameters::getDvfsLock(void) {
    return m_dvfsLock;
}

#ifdef DEBUG_RAWDUMP
bool ExynosCamera3Parameters::checkBayerDumpEnable(void)
{
#ifndef RAWDUMP_CAPTURE
    char enableRawDump[PROPERTY_VALUE_MAX];
    property_get("ro.debug.rawdump", enableRawDump, "0");

    if (strcmp(enableRawDump, "1") == 0) {
        /*CLOGD("checkBayerDumpEnable : 1");*/
        return true;
    } else {
        /*CLOGD("checkBayerDumpEnable : 0");*/
        return false;
    }
#endif
    return true;
}
#endif  /* DEBUG_RAWDUMP */

bool ExynosCamera3Parameters::setConfig(struct ExynosConfigInfo* config)
{
    memcpy(m_exynosconfig, config, sizeof(struct ExynosConfigInfo));
    setConfigMode(m_exynosconfig->mode);
    return true;
}
struct ExynosConfigInfo* ExynosCamera3Parameters::getConfig()
{
    return m_exynosconfig;
}

bool ExynosCamera3Parameters::setConfigMode(uint32_t mode)
{
    bool ret = false;
    switch(mode){
    case CONFIG_MODE::NORMAL:
    case CONFIG_MODE::HIGHSPEED_60:
    case CONFIG_MODE::HIGHSPEED_120:
    case CONFIG_MODE::HIGHSPEED_240:
        m_exynosconfig->current = &m_exynosconfig->info[mode];
        m_exynosconfig->mode = mode;
        ret = true;
        break;
    default:
        CLOGE(" unknown config mode (%d)", mode);
    }
    return ret;
}

int ExynosCamera3Parameters::getConfigMode()
{
    int ret = -1;
    switch(m_exynosconfig->mode){
    case CONFIG_MODE::NORMAL:
    case CONFIG_MODE::HIGHSPEED_60:
    case CONFIG_MODE::HIGHSPEED_120:
    case CONFIG_MODE::HIGHSPEED_240:
        ret = m_exynosconfig->mode;
        break;
    default:
        CLOGE(" unknown config mode (%d)", m_exynosconfig->mode);
    }

    return ret;
}

void ExynosCamera3Parameters::setZoomActiveOn(bool enable)
{
    m_zoom_activated = enable;
}

bool ExynosCamera3Parameters::getZoomActiveOn(void)
{
    return m_zoom_activated;
}

status_t ExynosCamera3Parameters::setMarkingOfExifFlash(int flag)
{
    m_firing_flash_marking = flag;

    return NO_ERROR;
}

int ExynosCamera3Parameters::getMarkingOfExifFlash(void)
{
    return m_firing_flash_marking;
}

bool ExynosCamera3Parameters::increaseMaxBufferOfPreview(void)
{
    if((getShotMode() == SHOT_MODE_BEAUTY_FACE)||(getShotMode() == SHOT_MODE_FRONT_PANORAMA)
#ifdef LLS_CAPTURE
        || (getLLSOn() == true && getCameraId() == CAMERA_ID_FRONT)
#endif
        ) {
        return true;
    } else {
        return false;
    }
}

bool ExynosCamera3Parameters::getSensorOTFSupported(void)
{
    return m_staticInfo->flite3aaOtfSupport;
}

bool ExynosCamera3Parameters::isReprocessing(void)
{
    bool reprocessing = false;
    int cameraId = getCameraId();
    bool flagDual = getDualMode();

    if (cameraId == CAMERA_ID_BACK) {
#if defined(MAIN_CAMERA_DUAL_REPROCESSING) && defined(MAIN_CAMERA_SINGLE_REPROCESSING)
        reprocessing = (flagDual == true) ? MAIN_CAMERA_DUAL_REPROCESSING : MAIN_CAMERA_SINGLE_REPROCESSING;
#else
        CLOGW(" MAIN_CAMERA_DUAL_REPROCESSING/MAIN_CAMERA_SINGLE_REPROCESSING is not defined");
#endif
    } else {
#if defined(FRONT_CAMERA_DUAL_REPROCESSING) && defined(FRONT_CAMERA_SINGLE_REPROCESSING)
        reprocessing = (flagDual == true) ? FRONT_CAMERA_DUAL_REPROCESSING : FRONT_CAMERA_SINGLE_REPROCESSING;
#else
        CLOGW(" FRONT_CAMERA_DUAL_REPROCESSING/FRONT_CAMERA_SINGLE_REPROCESSING is not defined");
#endif
    }

    return reprocessing;
}

bool ExynosCamera3Parameters::isSccCapture(void)
{
    bool sccCapture = false;
    int cameraId = getCameraId();
    bool flagDual = getDualMode();

    if (cameraId == CAMERA_ID_BACK) {
#if defined(MAIN_CAMERA_DUAL_SCC_CAPTURE) && defined(MAIN_CAMERA_SINGLE_SCC_CAPTURE)
        sccCapture = (flagDual == true) ? MAIN_CAMERA_DUAL_SCC_CAPTURE : MAIN_CAMERA_SINGLE_SCC_CAPTURE;
#else
        CLOGW(" MAIN_CAMERA_DUAL_SCC_CAPTURE/MAIN_CAMERA_SINGLE_SCC_CAPTUREis not defined");
#endif
    } else {
#if defined(FRONT_CAMERA_DUAL_SCC_CAPTURE) && defined(FRONT_CAMERA_SINGLE_SCC_CAPTURE)
        sccCapture = (flagDual == true) ? FRONT_CAMERA_DUAL_SCC_CAPTURE : FRONT_CAMERA_SINGLE_SCC_CAPTURE;
#else
        CLOGW(" FRONT_CAMERA_DUAL_SCC_CAPTURE/FRONT_CAMERA_SINGLE_SCC_CAPTURE is not defined");
#endif
    }

    return sccCapture;
}


/* True if private reprocessing or YUV reprocessing is supported */
bool ExynosCamera3Parameters::isSupportZSLInput(void) {
    if(m_staticInfo->capabilities != NULL && m_staticInfo->capabilitiesLength > 0) {
        for(size_t i = 0; i < m_staticInfo->capabilitiesLength; i++) {
            if( (m_staticInfo->capabilities[i] == ANDROID_REQUEST_AVAILABLE_CAPABILITIES_PRIVATE_REPROCESSING)
                || (m_staticInfo->capabilities[i] == ANDROID_REQUEST_AVAILABLE_CAPABILITIES_YUV_REPROCESSING) ) {
                return true;
            }
        }
    }

    return false;
}

bool ExynosCamera3Parameters::isFlite3aaOtf(void)
{
    bool flagOtfInput = false;

    int cameraId = getCameraId();
    bool flagDual = getDualMode();
    bool flagSensorOtf = getSensorOTFSupported();

    if (flagSensorOtf == false)
        return flagOtfInput;

    if (cameraId == CAMERA_ID_BACK) {
        /* for 52xx scenario */
        flagOtfInput = true;

        if (flagDual == true) {
#ifdef MAIN_CAMERA_DUAL_FLITE_3AA_OTF
            flagOtfInput = MAIN_CAMERA_DUAL_FLITE_3AA_OTF;
#else
            CLOGW(" MAIN_CAMERA_DUAL_FLITE_3AA_OTF is not defined");
#endif
        } else {
#ifdef MAIN_CAMERA_SINGLE_FLITE_3AA_OTF
            flagOtfInput = MAIN_CAMERA_SINGLE_FLITE_3AA_OTF;
#else
            CLOGW(" MAIN_CAMERA_SINGLE_FLITE_3AA_OTF is not defined");
#endif
        }
    } else {
        if (flagDual == true) {
#ifdef FRONT_CAMERA_DUAL_FLITE_3AA_OTF
            flagOtfInput = FRONT_CAMERA_DUAL_FLITE_3AA_OTF;
#else
            CLOGW(" FRONT_CAMERA_DUAL_FLITE_3AA_OTF is not defined");
#endif
        } else {
#ifdef FRONT_CAMERA_SINGLE_FLITE_3AA_OTF
            flagOtfInput = FRONT_CAMERA_SINGLE_FLITE_3AA_OTF;
#else
            CLOGW(" FRONT_CAMERA_SINGLE_FLITE_3AA_OTF is not defined");
#endif
        }
    }

    return flagOtfInput;
}

bool ExynosCamera3Parameters::is3aaIspOtf(void)
{
    bool ret = false;

    int cameraId = getCameraId();
    bool flagDual = getDualMode();

    if (cameraId == CAMERA_ID_BACK) {
        if (flagDual == true) {
#ifdef MAIN_CAMERA_DUAL_3AA_ISP_OTF
            ret = MAIN_CAMERA_DUAL_3AA_ISP_OTF;
#else
            CLOGW(" MAIN_CAMERA_DUAL_3AA_ISP_OTF is not defined");
#endif
        } else {
#ifdef MAIN_CAMERA_SINGLE_3AA_ISP_OTF
            ret = MAIN_CAMERA_SINGLE_3AA_ISP_OTF;
#else
            CLOGW(" MAIN_CAMERA_SINGLE_3AA_ISP_OTF is not defined");
#endif
        }
    } else {
        if (flagDual == true) {
#ifdef FRONT_CAMERA_DUAL_3AA_ISP_OTF
            ret = FRONT_CAMERA_DUAL_3AA_ISP_OTF;
#else
            CLOGW(" FRONT_CAMERA_DUAL_3AA_ISP_OTF is not defined");
#endif
        } else {
#ifdef FRONT_CAMERA_SINGLE_3AA_ISP_OTF
            ret = FRONT_CAMERA_SINGLE_3AA_ISP_OTF;
#else
            CLOGW(" FRONT_CAMERA_SINGLE_3AA_ISP_OTF is not defined");
#endif
        }
    }

    return ret;
}

bool ExynosCamera3Parameters::isIspTpuOtf(void)
{
    bool ret = false;

    int cameraId = getCameraId();
    bool flagDual = getDualMode();
    bool flagTpu = getTpuEnabledMode();

    if (flagTpu == true) {
        if (cameraId == CAMERA_ID_BACK) {
            if (flagDual == true) {
#ifdef MAIN_CAMERA_DUAL_ISP_TPU_OTF
                ret = MAIN_CAMERA_DUAL_ISP_TPU_OTF;
#else
                CLOGW(" MAIN_CAMERA_DUAL_ISP_TPU_OTF is not defined");
#endif
            } else {
#ifdef MAIN_CAMERA_SINGLE_ISP_TPU_OTF
                ret = MAIN_CAMERA_SINGLE_ISP_TPU_OTF;
#else
                CLOGW(" MAIN_CAMERA_SINGLE_ISP_TPU_OTF is not defined");
#endif
            }
        } else {
            if (flagDual == true) {
#ifdef FRONT_CAMERA_DUAL_ISP_TPU_OTF
                ret = FRONT_CAMERA_DUAL_ISP_TPU_OTF;
#else
                CLOGW(" FRONT_CAMERA_DUAL_ISP_TPU_OTF is not defined");
#endif
            } else {
#ifdef FRONT_CAMERA_SINGLE_ISP_TPU_OTF
                ret = FRONT_CAMERA_SINGLE_ISP_TPU_OTF;
#else
                CLOGW(" FRONT_CAMERA_SINGLE_ISP_TPU_OTF is not defined");
#endif
            }
        }
    }

    return ret;
}

bool ExynosCamera3Parameters::isIspMcscOtf(void)
{
    bool ret = false;

    int cameraId = getCameraId();
    bool flagDual = getDualMode();
    bool flagTpu = getTpuEnabledMode();

    if (flagTpu == false) {
        if (cameraId == CAMERA_ID_BACK) {
            if (flagDual == true) {
#ifdef MAIN_CAMERA_DUAL_ISP_MCSC_OTF
                ret = MAIN_CAMERA_DUAL_ISP_MCSC_OTF;
#else
                CLOGW(" MAIN_CAMERA_DUAL_ISP_MCSC_OTF is not defined");
#endif
            } else {
#ifdef MAIN_CAMERA_SINGLE_ISP_MCSC_OTF
                ret = MAIN_CAMERA_SINGLE_ISP_MCSC_OTF;
#else
                CLOGW(" MAIN_CAMERA_SINGLE_ISP_MCSC_OTF is not defined");
#endif
            }
        } else {
            if (flagDual == true) {
#ifdef FRONT_CAMERA_DUAL_ISP_MCSC_OTF
                ret = FRONT_CAMERA_DUAL_ISP_MCSC_OTF;
#else
                CLOGW(" FRONT_CAMERA_DUAL_ISP_MCSC_OTF is not defined");
#endif
            } else {
#ifdef FRONT_CAMERA_SINGLE_ISP_MCSC_OTF
                ret = FRONT_CAMERA_SINGLE_ISP_MCSC_OTF;
#else
                CLOGW(" FRONT_CAMERA_SINGLE_ISP_MCSC_OTF is not defined");
#endif
            }
        }
    }

    return ret;
}

bool ExynosCamera3Parameters::isTpuMcscOtf(void)
{
    bool ret = false;

    int cameraId = getCameraId();
    bool flagDual = getDualMode();
    bool flagTpu = getTpuEnabledMode();

    if (flagTpu == true) {
        if (cameraId == CAMERA_ID_BACK) {
            if (flagDual == true) {
#ifdef MAIN_CAMERA_DUAL_TPU_MCSC_OTF
                ret = MAIN_CAMERA_DUAL_TPU_MCSC_OTF;
#else
                CLOGW(" MAIN_CAMERA_DUAL_TPU_MCSC_OTF is not defined");
#endif
            } else {
#ifdef MAIN_CAMERA_SINGLE_TPU_MCSC_OTF
                ret = MAIN_CAMERA_SINGLE_TPU_MCSC_OTF;
#else
                CLOGW(" MAIN_CAMERA_SINGLE_TPU_MCSC_OTF is not defined");
#endif
            }
        } else {
            if (flagDual == true) {
#ifdef FRONT_CAMERA_DUAL_TPU_MCSC_OTF
                ret = FRONT_CAMERA_DUAL_TPU_MCSC_OTF;
#else
                CLOGW(" FRONT_CAMERA_DUAL_TPU_MCSC_OTF is not defined");
#endif
            } else {
#ifdef FRONT_CAMERA_SINGLE_TPU_MCSC_OTF
                ret = FRONT_CAMERA_SINGLE_TPU_MCSC_OTF;
#else
                CLOGW(" FRONT_CAMERA_SINGLE_TPU_MCSC_OTF is not defined");
#endif
            }
        }
    }

    return ret;
}

bool ExynosCamera3Parameters::isReprocessing3aaIspOTF(void)
{
    bool otf = false;

    int cameraId = getCameraId();
    bool flagDual = getDualMode();

    if (cameraId == CAMERA_ID_BACK) {
        if (flagDual == true) {
#ifdef MAIN_CAMERA_DUAL_3AA_ISP_OTF_REPROCESSING
            otf = MAIN_CAMERA_DUAL_3AA_ISP_OTF_REPROCESSING;
#else
            CLOGW(" MAIN_CAMERA_DUAL_3AA_ISP_OTF_REPROCESSING is not defined");
#endif
        } else {
#ifdef MAIN_CAMERA_SINGLE_3AA_ISP_OTF_REPROCESSING
            otf = MAIN_CAMERA_SINGLE_3AA_ISP_OTF_REPROCESSING;
#else
            CLOGW(" MAIN_CAMERA_SINGLE_3AA_ISP_OTF_REPROCESSING is not defined");
#endif
        }
    } else {
        if (flagDual == true) {
#ifdef FRONT_CAMERA_DUAL_3AA_ISP_OTF_REPROCESSING
            otf = FRONT_CAMERA_DUAL_3AA_ISP_OTF_REPROCESSING;
#else
            CLOGW(" FRONT_CAMERA_DUAL_3AA_ISP_OTF_REPROCESSING is not defined");
#endif
        } else {
#ifdef FRONT_CAMERA_SINGLE_3AA_ISP_OTF_REPROCESSING
            otf = FRONT_CAMERA_SINGLE_3AA_ISP_OTF_REPROCESSING;
#else
            CLOGW(" FRONT_CAMERA_SINGLE_3AA_ISP_OTF_REPROCESSING is not defined");
#endif
        }
    }

    if (otf == true) {
        bool flagDirtyBayer = false;

        int reprocessingBayerMode = this->getReprocessingBayerMode();
        switch(reprocessingBayerMode) {
        case REPROCESSING_BAYER_MODE_NONE:
        case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON:
        case REPROCESSING_BAYER_MODE_PURE_DYNAMIC:
            flagDirtyBayer = false;
            break;
        case REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON:
        case REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC:
        default:
            flagDirtyBayer = true;
            break;
        }

        if (flagDirtyBayer == true) {
            CLOGW(" otf == true. but, flagDirtyBayer == true. so force false on 3aa_isp otf");

            otf = false;
        }
    }

    return otf;
}

bool ExynosCamera3Parameters::isReprocessingIspTpuOtf(void)
{
    bool otf = false;

    int cameraId = getCameraId();
    bool flagDual = getDualMode();

    if (cameraId == CAMERA_ID_BACK) {
        if (flagDual == true) {
#ifdef MAIN_CAMERA_DUAL_ISP_TPU_OTF_REPROCESSING
            otf = MAIN_CAMERA_DUAL_ISP_TPU_OTF_REPROCESSING;
#else
            CLOGW(" MAIN_CAMERA_DUAL_ISP_TPU_OTF_REPROCESSING is not defined");
#endif
        } else {
#ifdef MAIN_CAMERA_SINGLE_ISP_TPU_OTF_REPROCESSING
            otf = MAIN_CAMERA_SINGLE_ISP_TPU_OTF_REPROCESSING;
#else
            CLOGW(" MAIN_CAMERA_SINGLE_ISP_TPU_OTF_REPROCESSING is not defined");
#endif
        }
    } else {
        if (flagDual == true) {
#ifdef FRONT_CAMERA_DUAL_ISP_TPU_OTF_REPROCESSING
            otf = FRONT_CAMERA_DUAL_ISP_TPU_OTF_REPROCESSING;
#else
            CLOGW(" FRONT_CAMERA_DUAL_ISP_TPU_OTF_REPROCESSING is not defined");
#endif
        } else {
#ifdef FRONT_CAMERA_SINGLE_ISP_TPU_OTF_REPROCESSING
            otf = FRONT_CAMERA_SINGLE_ISP_TPU_OTF_REPROCESSING;
#else
            CLOGW(" FRONT_CAMERA_SINGLE_ISP_TPU_OTF_REPROCESSING is not defined");
#endif
        }
    }

    return otf;
}

bool ExynosCamera3Parameters::isReprocessingIspMcscOtf(void)
{
    bool otf = false;

    int cameraId = getCameraId();
    bool flagDual = getDualMode();

    if (cameraId == CAMERA_ID_BACK) {
        if (flagDual == true) {
#ifdef MAIN_CAMERA_DUAL_ISP_MCSC_OTF_REPROCESSING
            otf = MAIN_CAMERA_DUAL_ISP_MCSC_OTF_REPROCESSING;
#else
            CLOGW(" MAIN_CAMERA_DUAL_ISP_MCSC_OTF_REPROCESSING is not defined");
#endif
        } else {
#ifdef MAIN_CAMERA_SINGLE_ISP_MCSC_OTF_REPROCESSING
            otf = MAIN_CAMERA_SINGLE_ISP_MCSC_OTF_REPROCESSING;
#else
            CLOGW(" MAIN_CAMERA_SINGLE_ISP_MCSC_OTF_REPROCESSING is not defined");
#endif
        }
    } else {
        if (flagDual == true) {
#ifdef FRONT_CAMERA_DUAL_ISP_MCSC_OTF_REPROCESSING
            otf = FRONT_CAMERA_DUAL_ISP_MCSC_OTF_REPROCESSING;
#else
            CLOGW(" FRONT_CAMERA_DUAL_ISP_MCSC_OTF_REPROCESSING is not defined");
#endif
        } else {
#ifdef FRONT_CAMERA_SINGLE_ISP_MCSC_OTF_REPROCESSING
            otf = FRONT_CAMERA_SINGLE_ISP_MCSC_OTF_REPROCESSING;
#else
            CLOGW(" FRONT_CAMERA_SINGLE_ISP_MCSC_OTF_REPROCESSING is not defined");
#endif
        }
    }

    return otf;
}

bool ExynosCamera3Parameters::isReprocessingTpuMcscOtf(void)
{
    bool otf = false;

    int cameraId = getCameraId();
    bool flagDual = getDualMode();

    if (cameraId == CAMERA_ID_BACK) {
        if (flagDual == true) {
#ifdef MAIN_CAMERA_DUAL_TPU_MCSC_OTF_REPROCESSING
            otf = MAIN_CAMERA_DUAL_TPU_MCSC_OTF_REPROCESSING;
#else
            CLOGW(" MAIN_CAMERA_DUAL_TPU_MCSC_OTF_REPROCESSING is not defined");
#endif
        } else {
#ifdef MAIN_CAMERA_SINGLE_TPU_MCSC_OTF_REPROCESSING
            otf = MAIN_CAMERA_SINGLE_TPU_MCSC_OTF_REPROCESSING;
#else
            CLOGW(" MAIN_CAMERA_SINGLE_TPU_MCSC_OTF_REPROCESSING is not defined");
#endif
        }
    } else {
        if (flagDual == true) {
#ifdef FRONT_CAMERA_DUAL_TPU_MCSC_OTF_REPROCESSING
            otf = FRONT_CAMERA_DUAL_TPU_MCSC_OTF_REPROCESSING;
#else
            CLOGW(" FRONT_CAMERA_DUAL_TPU_MCSC_OTF_REPROCESSING is not defined");
#endif
        } else {
#ifdef FRONT_CAMERA_SINGLE_TPU_MCSC_OTF_REPROCESSING
            otf = FRONT_CAMERA_SINGLE_TPU_MCSC_OTF_REPROCESSING;
#else
            CLOGW(" FRONT_CAMERA_SINGLE_TPU_MCSC_OTF_REPROCESSING is not defined");
#endif
        }
    }

    return otf;
}

bool ExynosCamera3Parameters::isSingleChain(void)
{
#ifdef USE_SINGLE_CHAIN
    return true;
#else
    return false;
#endif
}

bool ExynosCamera3Parameters::isUse3aaInputCrop(void)
{
    return true;
}

bool ExynosCamera3Parameters::isUseIspInputCrop(void)
{
    if (isUse3aaInputCrop() == true
        || is3aaIspOtf() == true)
        return false;
    else
        return true;
}

bool ExynosCamera3Parameters::isUseMcscInputCrop(void)
{
    if (isUse3aaInputCrop() == true
        || isUseIspInputCrop() == true
        || isIspMcscOtf() == true
        || isTpuMcscOtf() == true)
        return false;
    else
        return true;
}

bool ExynosCamera3Parameters::isUseReprocessing3aaInputCrop(void)
{
    return true;
}

bool ExynosCamera3Parameters::isUseReprocessingIspInputCrop(void)
{
    if (isUseReprocessing3aaInputCrop() == true
        || isReprocessing3aaIspOTF() == true)
        return false;
    else
        return true;
}

bool ExynosCamera3Parameters::isUseReprocessingMcscInputCrop(void)
{
    if (isUseReprocessing3aaInputCrop() == true
        || isUseReprocessingIspInputCrop() == true
        || isReprocessingIspMcscOtf() == true
        || isReprocessingTpuMcscOtf() == true)
        return false;
    else
        return true;
}

bool ExynosCamera3Parameters::isUseEarlyFrameReturn(void)
{
#if defined(USE_EARLY_FRAME_RETURN)
    return true;
#else
    return false;
#endif
}

bool ExynosCamera3Parameters::isUseHWFC(void)
{
#if defined(USE_JPEG_HWFC)
    return USE_JPEG_HWFC;
#else
    return false;
#endif
}

void ExynosCamera3Parameters::setZoomPreviewWIthScaler(bool enable)
{
	m_zoomWithScaler = enable;
}

bool ExynosCamera3Parameters::getZoomPreviewWIthScaler(void)
{
	return m_zoomWithScaler;
}

bool ExynosCamera3Parameters::isOwnScc(int cameraId)
{
    bool ret = false;

    if (cameraId == CAMERA_ID_BACK) {
#ifdef MAIN_CAMERA_HAS_OWN_SCC
        ret = MAIN_CAMERA_HAS_OWN_SCC;
#else
        CLOGW(" MAIN_CAMERA_HAS_OWN_SCC is not defined");
#endif
    } else {
#ifdef FRONT_CAMERA_HAS_OWN_SCC
        ret = FRONT_CAMERA_HAS_OWN_SCC;
#else
        CLOGW(" FRONT_CAMERA_HAS_OWN_SCC is not defined");
#endif
    }

    return ret;
}

int ExynosCamera3Parameters::getHalVersion(void)
{
    return m_halVersion;
}

void ExynosCamera3Parameters::setHalVersion(int halVersion)
{
    m_halVersion = halVersion;
    m_activityControl->setHalVersion(m_halVersion);

    CLOGI(" m_halVersion(%d)", m_halVersion);

    return;
}

struct ExynosSensorInfoBase *ExynosCamera3Parameters::getSensorStaticInfo()
{
    return m_staticInfo;
}

bool ExynosCamera3Parameters::getSetFileCtlMode(void)
{
#ifdef SET_SETFILE_BY_SET_CTRL
    return true;
#else
    return false;
#endif
}

bool ExynosCamera3Parameters::getSetFileCtl3AA_ISP(void)
{
#ifdef SET_SETFILE_BY_SET_CTRL_3AA_ISP
    return SET_SETFILE_BY_SET_CTRL_3AA_ISP;
#else
    return false;
#endif
}

bool ExynosCamera3Parameters::getSetFileCtl3AA(void)
{
#ifdef SET_SETFILE_BY_SET_CTRL_3AA
    return SET_SETFILE_BY_SET_CTRL_3AA;
#else
    return false;
#endif
}

bool ExynosCamera3Parameters::getSetFileCtlISP(void)
{
#ifdef SET_SETFILE_BY_SET_CTRL_ISP
    return SET_SETFILE_BY_SET_CTRL_ISP;
#else
    return false;
#endif
}

bool ExynosCamera3Parameters::getSetFileCtlSCP(void)
{
#ifdef SET_SETFILE_BY_SET_CTRL_SCP
    return SET_SETFILE_BY_SET_CTRL_SCP;
#else
    return false;
#endif
}

void ExynosCamera3Parameters::m_getV4l2Name(char* colorName, size_t length, int colorFormat)
{
    size_t index = 0;
    if (colorName == NULL) {
        CLOGE("colorName is NULL");
        return;
    }

    for (index = 0; index < length-1; index++) {
        colorName[index] = colorFormat & 0xff;
        colorFormat = colorFormat >> 8;
    }
    colorName[index] = '\0';
}

int32_t ExynosCamera3Parameters::getYuvStreamMaxNum(void)
{
    int32_t yuvStreamMaxNum = -1;

    if (m_staticInfo == NULL) {
        CLOGE("m_staticInfo is NULL");

        return INVALID_OPERATION;
    }

    yuvStreamMaxNum = m_staticInfo->maxNumOutputStreams[PROCESSED];
    if (yuvStreamMaxNum < 0) {
        CLOGE("Invalid MaxNumOutputStreamsProcessed %d",
                 yuvStreamMaxNum);
        return BAD_VALUE;
    }

    return yuvStreamMaxNum;
}

status_t ExynosCamera3Parameters::setYuvBufferCount(const int count, const int index)
{
    if (count < 0 || count > VIDEO_MAX_FRAME
            || index < 0 || index > m_staticInfo->maxNumOutputStreams[PROCESSED]) {
        CLOGE("Invalid argument. count %d index %d",
                 count, index);

        return BAD_VALUE;
    }

    m_yuvBufferCount[index] = count;

    return NO_ERROR;
}

int ExynosCamera3Parameters::getYuvBufferCount(const int index)
{
    if (index < 0 || index > m_staticInfo->maxNumOutputStreams[PROCESSED]) {
        CLOGE("Invalid index %d",
                 index);
        return 0;
    }

    return m_yuvBufferCount[index];
}

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
void ExynosCamera3Parameters::setDualCameraMode(bool toggle)
{
    android_printAssert(NULL, LOG_TAG, "Not yet supported, assert!!!!");
}

bool ExynosCamera3Parameters::getDualCameraMode(void)
{
    android_printAssert(NULL, LOG_TAG, "Not yet supported, assert!!!!");

    return false;
}

bool ExynosCamera3Parameters::isFusionEnabled(void)
{
    android_printAssert(NULL, LOG_TAG, "Not yet supported, assert!!!!");

    return false;
}

status_t ExynosCamera3Parameters::getFusionSize(int w, int h, ExynosRect *srcRect, ExynosRect *dstRect)
{
    android_printAssert(NULL, LOG_TAG, "Not yet supported, assert!!!!");

    return NO_ERROR;
}

status_t ExynosCamera3Parameters::setFusionInfo(camera2_shot_ext *shot_ext)
{
    android_printAssert(NULL, LOG_TAG, "Not yet supported, assert!!!!");

    return NO_ERROR;
}

DOF *ExynosCamera3Parameters::getDOF(void)
{
    android_printAssert(NULL, LOG_TAG, "Not yet supported, assert!!!!");

    return NULL;
}

#ifdef USE_CP_FUSION_LIB
char * ExynosCamera3Parameters::readFusionCalData(int *readSize)
{
    android_printAssert(NULL, LOG_TAG, "Not yet supported, assert!!!!");

    return NULL;
}

void ExynosCamera3Parameters::setFusionCalData(char *addr, int size)
{
    android_printAssert(NULL, LOG_TAG, "Not yet supported, assert!!!!");
}

char *ExynosCamera3Parameters::getFusionCalData(int *size)
{
    android_printAssert(NULL, LOG_TAG, "Not yet supported, assert!!!!");

    return NULL;
}
#endif // USE_CP_FUSION_LIB
#endif // BOARD_CAMERA_USES_DUAL_CAMERA


void ExynosCamera3Parameters::setHighSpeedMode(uint32_t mode)
{
    switch(mode){
    case CONFIG_MODE::HIGHSPEED_120:
        setConfigMode(CONFIG_MODE::HIGHSPEED_120);
        m_setHighSpeedRecording(true);
        break;
    case CONFIG_MODE::HIGHSPEED_240:
        setConfigMode(CONFIG_MODE::HIGHSPEED_240);
        m_setHighSpeedRecording(true);
        break;
    case CONFIG_MODE::NORMAL:
    default:
        setConfigMode(CONFIG_MODE::NORMAL);
        m_setHighSpeedRecording(false);
       break;
    }
}
#ifdef USE_BDS_2_0_480P_YUV
void ExynosCamera3Parameters::clearYuvSizeSetupFlag(void)
{
    memset(&m_yuvSizeSetup, 0x00, sizeof(m_yuvSizeSetup));
}
#endif
}; /* namespace android */
