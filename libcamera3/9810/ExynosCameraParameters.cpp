/*
**
** Copyright 2017, Samsung Electronics Co. LTD
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
#define LOG_TAG "ExynosCameraParameters"
#include <log/log.h>

#include "ExynosCameraParameters.h"

namespace android {

ExynosCameraParameters::ExynosCameraParameters(int cameraId)
{
    if (cameraId == CAMERA_ID_SECURE) {
        m_scenario = SCENARIO_SECURE;
        m_cameraId = CAMERA_ID_FRONT;
    } else {
        m_scenario = SCENARIO_NORMAL;
        m_cameraId = cameraId;
    }

    switch (cameraId) {
    case CAMERA_ID_BACK:
        strncpy(m_name, "Back", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        break;
    case CAMERA_ID_FRONT:
        strncpy(m_name, "Front", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        break;
    case CAMERA_ID_SECURE:
        strncpy(m_name, "Secure", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        break;
#ifdef USE_DUAL_CAMERA
    case CAMERA_ID_BACK_1:
        strncpy(m_name, "BackSlave", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        break;
    case CAMERA_ID_FRONT_1:
        strncpy(m_name, "FrontSlave", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        break;
#endif
    default:
        memset(m_name, 0x00, sizeof(m_name));
        CLOGE("Invalid camera ID(%d)", cameraId);
        break;
    }

    m_staticInfo = createExynosCameraSensorInfo(cameraId);
    m_useSizeTable = (m_staticInfo->sizeTableSupport) ? USE_CAMERA_SIZE_TABLE : false;

    m_exynosconfig = NULL;
    m_activityControl = new ExynosCameraActivityControl(m_cameraId);
    ExynosCameraActivityUCTL* uctlMgr = m_activityControl->getUCTLMgr();
    uctlMgr->setMetadata(&m_metadata);

    memset(&m_cameraInfo, 0, sizeof(struct exynos_camera_info));
    memset(&m_exifInfo, 0, sizeof(m_exifInfo));
    memset(&m_metaParameters, 0, sizeof(struct CameraMetaParameters));

    m_metaParameters.m_zoomRatio = 1.0f;

    m_initMetadata();

    m_setExifFixedAttribute();

    m_exynosconfig = new ExynosConfigInfo();
    memset((void *)m_exynosconfig, 0x00, sizeof(struct ExynosConfigInfo));

    // CAUTION!! : Initial values must be prior to setDefaultParameter() function.
    // Initial Values : START
    m_flagCheckPIPMode = false;
    m_flagCheckProMode = false;
#ifdef USE_DUAL_CAMERA
    m_flagCheckDualMode = false;
    m_dualOperationMode = DUAL_OPERATION_MODE_SYNC;
#endif

    m_flagCheckRecordingHint = false;
    m_flagRestartStream = false;

    m_useSensorPackedBayer = false;
    m_useDynamicBayer = (m_cameraId == CAMERA_ID_BACK) ? USE_DYNAMIC_BAYER : USE_DYNAMIC_BAYER_FRONT;
    m_useDynamicBayer60Fps = (m_cameraId == CAMERA_ID_BACK) ? USE_DYNAMIC_BAYER_60FPS : USE_DYNAMIC_BAYER_60FPS_FRONT;
    m_useDynamicBayer120Fps = (m_cameraId == CAMERA_ID_BACK) ? USE_DYNAMIC_BAYER_120FPS : USE_DYNAMIC_BAYER_120FPS_FRONT;
    m_useDynamicBayer240Fps = (m_cameraId == CAMERA_ID_BACK) ? USE_DYNAMIC_BAYER_240FPS : USE_DYNAMIC_BAYER_240FPS_FRONT;

    m_useFastenAeStable = false;

    m_usePureBayerReprocessing = (m_cameraId == CAMERA_ID_BACK) ? USE_PURE_BAYER_REPROCESSING : USE_PURE_BAYER_REPROCESSING_FRONT;

    m_dvfsLock = false;

    for (int i = 0; i < this->getYuvStreamMaxNum(); i++) {
        m_yuvBufferCount[i] = 1; //YUV
        m_yuvBufferCount[i + YUV_MAX] = 1; //YUV_STALL
    }

    m_isUniqueIdRead = false;

    resetMinYuvSize();

    m_previewDsInputPortId = MCSC_PORT_NONE;
    m_captureDsInputPortId = MCSC_PORT_NONE;

    m_flagYuvStallPortUsage = YUV_STALL_USAGE_DSCALED;
    m_exposureTimeCapture = 0L;

    m_isManualAeControl = false;
    m_metaParameters.m_flashMode = FLASH_MODE_OFF;

    m_isFactoryBin = false;
    if (m_scenario == SCENARIO_SECURE) {
        char propertyValue[PROPERTY_VALUE_MAX];

        m_exposureTime = m_staticInfo->exposureTime;
        m_gain = m_staticInfo->gain;
        m_ledPulseWidth = m_staticInfo->ledPulseWidth;
        m_ledPulseDelay = m_staticInfo->ledPulseDelay;
        m_ledCurrent = m_staticInfo->ledCurrent;

        property_get("ro.factory.factory_binary", propertyValue, "0");
        if (strncmp(propertyValue, "factory", 7)) {
            m_isFactoryBin = true;
            m_ledMaxTime = 0L;
        } else {
            m_ledMaxTime = m_staticInfo->ledMaxTime;
        }

        m_setVisionMode(true);
    }

    m_isFullSizeLut = false;

    m_thumbnailCbW = 0;
    m_thumbnailCbH = 0;

    m_setfile = 0;
    m_yuvRange = 0;
    m_setfileReprocessing = 0;
    m_yuvRangeReprocessing = 0;
    m_firing_flash_marking = 0;

    m_previewPortId = -1;
    m_recordingPortId = -1;
    m_yuvStallPort = -1;

    vendorSpecificConstructor(cameraId);

    // Initial Values : END
    setDefaultCameraInfo();
}

ExynosCameraParameters::~ExynosCameraParameters()
{
    if (m_staticInfo != NULL) {
        delete m_staticInfo;
        m_staticInfo = NULL;
    }

    if (m_activityControl != NULL) {
        delete m_activityControl;
        m_activityControl = NULL;
    }

    if (m_exynosconfig != NULL) {
        memset((void *)m_exynosconfig, 0x00, sizeof(struct ExynosConfigInfo));
        delete m_exynosconfig;
        m_exynosconfig = NULL;
    }

    if (m_exifInfo.maker_note) {
        delete[] m_exifInfo.maker_note;
        m_exifInfo.maker_note = NULL;
    }

    if (m_exifInfo.user_comment) {
        delete[] m_exifInfo.user_comment;
        m_exifInfo.user_comment = NULL;
    }

    m_vendorSpecificDestructor();
}

void ExynosCameraParameters::setDefaultCameraInfo(void)
{
    CLOGI("");

    m_setHwSensorSize(m_staticInfo->maxSensorW, m_staticInfo->maxSensorH);
    for (int i = 0; i < this->getYuvStreamMaxNum(); i++) {
        /* YUV */
        m_setYuvSize(m_staticInfo->maxPreviewW, m_staticInfo->maxPreviewH, i);
        m_setYuvFormat(V4L2_PIX_FMT_NV21, i);

        /* YUV_STALL */
        m_setYuvSize(m_staticInfo->maxPreviewW, m_staticInfo->maxPreviewH, i + YUV_MAX);
        m_setYuvFormat(V4L2_PIX_FMT_NV21, i + YUV_MAX);
    }

    m_setHwPictureSize(m_staticInfo->maxPictureW, m_staticInfo->maxPictureH);
    m_setPictureSize(m_staticInfo->maxPictureW, m_staticInfo->maxPictureH);
    m_setHwPictureFormat(SCC_OUTPUT_COLOR_FMT);
    m_setHwPicturePixelSize(CAMERA_PIXEL_SIZE_8BIT);
    m_setThumbnailSize(m_staticInfo->maxThumbnailW, m_staticInfo->maxThumbnailH);

    /* Initalize Binning scale ratio */
    m_setBinningScaleRatio(1000);

    setRecordingHint(false);
    setVideoSize(0, 0);
    setPIPMode(false);
}

void ExynosCameraParameters::m_setVisionMode(bool vision)
{
    m_cameraInfo.visionMode = vision;
}

bool ExynosCameraParameters::getVisionMode(void)
{
    return m_cameraInfo.visionMode;
}

void ExynosCameraParameters::m_setVisionModeFps(int fps)
{
    m_cameraInfo.visionModeFps = fps;
}

int ExynosCameraParameters::getVisionModeFps(void)
{
    return m_cameraInfo.visionModeFps;
}

void ExynosCameraParameters::m_setVisionModeAeTarget(int ae)
{
    m_cameraInfo.visionModeAeTarget = ae;
}

int ExynosCameraParameters::getVisionModeAeTarget(void)
{
    return m_cameraInfo.visionModeAeTarget;
}

void ExynosCameraParameters::setRecordingHint(bool hint)
{
    m_cameraInfo.recordingHint = hint;

    /* RecordingHint is confirmed */
    m_flagCheckRecordingHint = true;
}

bool ExynosCameraParameters::getRecordingHint(void)
{
    /*
     * Before setParameters, we cannot know recordingHint is valid or not
     * So, check and make assert for fast debugging
     */
    if (m_flagCheckRecordingHint == false)
        android_printAssert(NULL, LOG_TAG, "Cannot call getRecordingHint befor setRecordingHint, assert!!!!");

    return m_cameraInfo.recordingHint;
}

void ExynosCameraParameters::setProMode(bool proMode)
{
    /* proMode is confirmed */
    m_flagCheckProMode = proMode;
}

void ExynosCameraParameters::setPIPMode(bool pip)
{
    m_cameraInfo.pipMode = pip;
    /* PIP Mode is confirmed */
    m_flagCheckPIPMode = true;
}

bool ExynosCameraParameters::getPIPMode(void)
{
    /*
    * Before setParameters, we cannot know PIP Mode is valid or not
    * So, check and make assert for fast debugging
    */
    if (m_flagCheckPIPMode == false) {
        return false;
    }

    return m_cameraInfo.pipMode;
}

#ifdef USE_DUAL_CAMERA
void ExynosCameraParameters::setDualMode(bool enabled)
{
    m_cameraInfo.dualMode = enabled;
    /* Dual Mode is confirmed */
    m_flagCheckDualMode = true;
}

bool ExynosCameraParameters::getDualMode(void)
{
    /*
    * Before setParameters, we cannot know Dual Mode is valid or not
    * So, check and make assert for fast debugging
    */
    if (m_flagCheckDualMode == false) {
        return false;
    }

    return m_cameraInfo.dualMode;
}

enum DUAL_PREVIEW_MODE ExynosCameraParameters::getDualPreviewMode(void)
{
    /*
    * Before setParameters, we cannot know dualMode is valid or not
    * So, check and make assert for fast debugging
    */
    if (m_flagCheckDualMode == false) {
        return DUAL_PREVIEW_MODE_OFF;
    }

    if (getDualMode() == false) {
        return DUAL_PREVIEW_MODE_OFF;
    }

#ifdef USE_DUAL_PREVIEW_HW
    if (USE_DUAL_PREVIEW_HW == true) {
        return DUAL_PREVIEW_MODE_HW;
    } else
#endif
#ifdef USE_DUAL_PREVIEW_SW
    if (USE_DUAL_PREVIEW_SW == true) {
        return DUAL_PREVIEW_MODE_SW;
    } else
#endif
    {
        return DUAL_PREVIEW_MODE_OFF;
    }
}

enum DUAL_REPROCESSING_MODE ExynosCameraParameters::getDualReprocessingMode(void)
{
    /*
    * Before setParameters, we cannot know dualMode is valid or not
    * So, check and make assert for fast debugging
    */
    if (m_flagCheckDualMode == false) {
        return DUAL_REPROCESSING_MODE_OFF;
    }

    if (getDualMode() == false) {
        return DUAL_REPROCESSING_MODE_OFF;
    }

#ifdef USE_DUAL_REPROCESSING_HW
    if (USE_DUAL_REPROCESSING_HW == true) {
        return DUAL_REPROCESSING_MODE_HW;
    } else
#endif
#ifdef USE_DUAL_REPROCESSING_SW
    if (USE_DUAL_REPROCESSING_SW == true) {
        return DUAL_REPROCESSING_MODE_SW;
    } else
#endif
    {
        return DUAL_REPROCESSING_MODE_OFF;
    }
}

void ExynosCameraParameters::setDualOperationMode(enum DUAL_OPERATION_MODE mode)
{
    m_dualOperationMode = mode;
}

enum DUAL_OPERATION_MODE ExynosCameraParameters::getDualOperationMode(void)
{
    return m_dualOperationMode;
}

bool ExynosCameraParameters::isSupportMasterSensorStandby(void)
{
#ifdef SUPPORT_MASTER_SENSOR_STANDBY
    return SUPPORT_MASTER_SENSOR_STANDBY;
#else
    return true;
#endif
}

bool ExynosCameraParameters::isSupportSlaveSensorStandby(void)
{
#ifdef SUPPORT_SLAVE_SENSOR_STANDBY
    return SUPPORT_SLAVE_SENSOR_STANDBY;
#else
    return true;
#endif
}
#endif

uint32_t ExynosCameraParameters::getSensorStandbyDelay(void)
{
#ifdef SENSOR_STANDBY_DELAY
    return SENSOR_STANDBY_DELAY;
#else
    return 2;
#endif
}

bool ExynosCameraParameters::getPIPRecordingHint(void)
{
    return m_cameraInfo.pipRecordingHint;
}

void ExynosCameraParameters::m_setPreviewFpsRange(uint32_t min, uint32_t max)
{
    setMetaCtlAeTargetFpsRange(&m_metadata, min, max);
    setMetaCtlSensorFrameDuration(&m_metadata, (uint64_t)((1000 * 1000 * 1000) / (uint64_t)max));

    CLOGI("fps min(%d) max(%d)", min, max);
}

void ExynosCameraParameters::getPreviewFpsRange(uint32_t *min, uint32_t *max)
{
    /* ex) min = 15 , max = 30 */
    getMetaCtlAeTargetFpsRange(&m_metadata, min, max);
}

bool ExynosCameraParameters::m_isUHDRecordingMode(void)
{
    bool isUHDRecording = false;
    int videoW = 0, videoH = 0;
    getVideoSize(&videoW, &videoH);

    if (((videoW == 3840 && videoH == 2160) || (videoW == 2560 && videoH == 1440))
        && getRecordingHint() == true)
        isUHDRecording = true;

    return isUHDRecording;
}

void ExynosCameraParameters::setVideoSize(int w, int h)
{
    m_cameraInfo.videoW = w;
    m_cameraInfo.videoH = h;
}

bool ExynosCameraParameters::getUHDRecordingMode(void)
{
    return m_isUHDRecordingMode();
}

void ExynosCameraParameters::getVideoSize(int *w, int *h)
{
    *w = m_cameraInfo.videoW;
    *h = m_cameraInfo.videoH;
}

int ExynosCameraParameters::getVideoFormat(void)
{
    return V4L2_PIX_FMT_NV12M;
}

void ExynosCameraParameters::m_setHighSpeedRecording(bool highSpeed)
{
    m_cameraInfo.highSpeedRecording = highSpeed;
}

bool ExynosCameraParameters::getHighSpeedRecording(void)
{
    return m_cameraInfo.highSpeedRecording;
}

void ExynosCameraParameters::setRestartStream(bool restart)
{
    Mutex::Autolock lock(m_parameterLock);
    m_flagRestartStream = restart;
}

bool ExynosCameraParameters::getRestartStream(void)
{
    Mutex::Autolock lock(m_parameterLock);
    return m_flagRestartStream;
}

bool ExynosCameraParameters::getVideoStabilization(void)
{
    return m_cameraInfo.videoStabilization;
}

bool ExynosCameraParameters::getHWVdisMode(void)
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

#ifdef SAMSUNG_SW_VDIS
    if (ret == true && this->getSWVdisMode() == true) {
        ret = false;
    }
#endif

    return ret;
}

int ExynosCameraParameters::getHWVdisFormat(void)
{
    return V4L2_PIX_FMT_YUYV;
}

void ExynosCameraParameters::getHwVraInputSize(int *w, int *h, int dsInputPortId)
{
    status_t ret = NO_ERROR;
    int vraWidth = MAX_VRA_INPUT_WIDTH;
    int vraHeight = MAX_VRA_INPUT_HEIGHT;
    int dsInputWidth = 0;
    int dsInputHeight = 0;
    ExynosRect dummyIspSize;
    ExynosRect dsOutputSize;

    if (dsInputPortId >= MCSC_PORT_MAX
        || dsInputPortId <= MCSC_PORT_NONE) {
        *w = vraWidth;
        *h = vraHeight;
        return;
    }

    switch (dsInputPortId) {
    case MCSC_PORT_0:
    case MCSC_PORT_1:
    case MCSC_PORT_2:
        getYuvVendorSize(&dsInputWidth, &dsInputHeight, dsInputPortId, dummyIspSize);
        break;
    case MCSC_PORT_3:
        getPictureSize(&dsInputWidth, &dsInputHeight);
        break;
    case MCSC_PORT_4:
        getThumbnailSize(&dsInputWidth, &dsInputHeight);
        break;
    default:
        CLOGE("Unsupported dsInputPortId %d", dsInputPortId);
        *w = vraWidth;
        *h = vraHeight;
        return;
    }

    if (dsInputWidth < vraWidth || dsInputHeight < vraHeight) {
        dsOutputSize.w = dsInputWidth;
        dsOutputSize.h = dsInputHeight;
    } else {
        ret = getCropRectAlign(
                vraWidth, vraHeight, dsInputWidth, dsInputHeight,
                &dsOutputSize.x, &dsOutputSize.y, &dsOutputSize.w, &dsOutputSize.h,
                2, 2, 1.0f);
        if (ret != NO_ERROR) {
            CLOGE("Failed to getCropRectAlign. %dx%d -> %dx%d ret %d",
                    vraWidth, vraHeight, dsInputWidth, dsInputHeight, ret);

            *w = vraWidth;
            *h = vraHeight;
            return;
        }
    }

    *w = dsOutputSize.w;
    *h = dsOutputSize.h;

    CLOGV("dsInputPortId %d DSsize %dx%d->%dx%d",
            dsInputPortId, dsInputWidth, dsInputHeight, *w, *h);
    return;
}

int ExynosCameraParameters::getHwVraInputFormat(void)
{
#if defined(CAMERA_VRA_INPUT_FORMAT)
    return CAMERA_VRA_INPUT_FORMAT;
#else
    return V4L2_PIX_FMT_NV16;
#endif
}

void ExynosCameraParameters::setDsInputPortId(int dsInputPortId, bool isReprocessing)
{
    if (isReprocessing == false) {
        m_previewDsInputPortId = dsInputPortId;
    } else {
        m_captureDsInputPortId = dsInputPortId;
    }
}

int ExynosCameraParameters::getDsInputPortId(bool isReprocessing)
{
    int dsInputPortId = MCSC_PORT_NONE;

    if (isReprocessing == false) {
        dsInputPortId = m_previewDsInputPortId;
    } else {
        dsInputPortId = m_captureDsInputPortId;
    }

    return dsInputPortId;
}

void ExynosCameraParameters::updateYsumPordId(struct camera2_shot_ext *shot_ext)
{
    if (getYsumRecordingMode() == true) {
        setMetaUctlYsumPort(shot_ext, (enum mcsc_port)getRecordingPortId());
        CLOGV("YSUM recording port ID(%d)", getRecordingPortId());
    }
}

status_t ExynosCameraParameters::updateYsumBuffer(struct ysum_data *ysumdata, ExynosCameraBuffer *dstBuf)
{
    ExynosVideoMeta *videoMeta = NULL;
    uint64_t ysumValue = ysumdata->higher_ysum_value;
    ysumValue = ((ysumValue << 32) | ysumdata->lower_ysum_value);

    if (dstBuf->batchSize > 1) {
        CLOGD("YSUM recording does not support batch mode. only works at 30, 60 fps.");
        return NO_ERROR;
    }

    int ysumPlaneIndex = dstBuf->getMetaPlaneIndex() - 1;
    if (dstBuf->size[ysumPlaneIndex] != EXYNOS_CAMERA_YSUM_PLANE_SIZE) {
        android_printAssert(NULL, LOG_TAG,
                        "ASSERT(%s):Invalid access to ysum plane. planeIndex %d size %d",
                        __FUNCTION__, ysumPlaneIndex, dstBuf->size[ysumPlaneIndex]);
        return -BAD_VALUE;
    }

    videoMeta = (ExynosVideoMeta *)dstBuf->addr[ysumPlaneIndex];

    videoMeta->eType = VIDEO_INFO_TYPE_YSUM_DATA;
    videoMeta->data.enc.sYsumData.high = ysumdata->higher_ysum_value;
    videoMeta->data.enc.sYsumData.low = ysumdata->lower_ysum_value;

    CLOGV("ysumValue(%lld), higher_ysum_value(%d), lower_ysum_value(%d)",
                   (long long)ysumValue, ysumdata->higher_ysum_value, ysumdata->lower_ysum_value);

    return NO_ERROR;
}

#ifdef HAL3_YUVSIZE_BASED_BDS
/*
   Make the all YUV output size as smallest preview size.
   Format will be set to NV21
*/
status_t ExynosCameraParameters::initYuvSizes() {
    int maxWidth, maxHeight = 0;
    int (*sizeList)[SIZE_OF_RESOLUTION];
    size_t lastIdx = 0;

    lastIdx = m_staticInfo->yuvListMax - 1;
    sizeList = m_staticInfo->yuvList;

    for(int outputPort = 0; outputPort < getYuvStreamMaxNum(); outputPort++) {
        CLOGV("Port[%d] Idx[%d], Size(%d x %d) / true"
            , outputPort, lastIdx, sizeList[lastIdx][0], sizeList[lastIdx][1]);
        /* YUV */
        m_setYuvSize(sizeList[lastIdx][0], sizeList[lastIdx][1], outputPort);
        m_setYuvFormat(V4L2_PIX_FMT_NV21, outputPort);

        /* YUV_STALL */
        m_setYuvSize(sizeList[lastIdx][0], sizeList[lastIdx][1], outputPort + YUV_MAX);
        m_setYuvFormat(V4L2_PIX_FMT_NV21, outputPort + YUV_MAX);
    }

    return NO_ERROR;
}
#endif

void ExynosCameraParameters::resetMinYuvSize() {
    m_cameraInfo.minYuvW = 0;
    m_cameraInfo.minYuvH = 0;
}

status_t ExynosCameraParameters::getMinYuvSize(int* w, int* h) const {
    if (m_cameraInfo.minYuvH == 0 || m_cameraInfo.minYuvW == 0) {
        CLOGE(" Min YUV size is not initialized (w=%d, h=%d)",
            m_cameraInfo.minYuvW, m_cameraInfo.minYuvH);
        return INVALID_OPERATION;
    }

    *w = m_cameraInfo.minYuvW;
    *h = m_cameraInfo.minYuvH;
    return OK;
}

void ExynosCameraParameters::resetMaxYuvSize() {
    m_cameraInfo.maxYuvW = 0;
    m_cameraInfo.maxYuvH = 0;
}

status_t ExynosCameraParameters::getMaxYuvSize(int* w, int* h) const {
    if(m_cameraInfo.maxYuvH == 0 || m_cameraInfo.maxYuvW == 0) {
        CLOGE("Max YUV size is not initialized (w=%d, h=%d)", m_cameraInfo.maxYuvW, m_cameraInfo.maxYuvH);
        return INVALID_OPERATION;
    }

    *w = m_cameraInfo.maxYuvW;
    *h = m_cameraInfo.maxYuvH;
    return OK;
}

void ExynosCameraParameters::resetMaxHwYuvSize() {
    m_cameraInfo.maxHwYuvW = 0;
    m_cameraInfo.maxHwYuvH = 0;
}

status_t ExynosCameraParameters::getMaxHwYuvSize(int* w, int* h) const {
    if(m_cameraInfo.maxHwYuvH == 0 || m_cameraInfo.maxHwYuvW == 0) {
        CLOGE("Max YUV size is not initialized (w=%d, h=%d)", m_cameraInfo.maxYuvW, m_cameraInfo.maxYuvH);
        return INVALID_OPERATION;
    }

    *w = m_cameraInfo.maxHwYuvW;
    *h = m_cameraInfo.maxHwYuvH;
    return OK;
}

status_t ExynosCameraParameters::resetYuvSizeRatioId(void)
{
    m_cameraInfo.yuvSizeRatioId = m_staticInfo->sensorArrayRatio;

    return NO_ERROR;
}

bool ExynosCameraParameters::m_isSupportedYuvSize(const int width,
                                                   const int height,
                                                   __unused const int outputPortId,
                                                   int *ratio)
{
    int maxWidth, maxHeight = 0;
    int (*sizeList)[SIZE_OF_RESOLUTION];

    getMaxPreviewSize(&maxWidth, &maxHeight);

    if (maxWidth*maxHeight < width*height) {
        CLOGE("invalid PreviewSize(maxSize(%d/%d) size(%d/%d)", maxWidth, maxHeight, width, height);
        return false;
    }

    sizeList = m_staticInfo->yuvList;
    for (int i = 0; i < m_staticInfo->yuvListMax; i++) {
        if (sizeList[i][0] > maxWidth || sizeList[i][1] > maxHeight)
            continue;

        if (sizeList[i][0] == width && sizeList[i][1] == height) {
            *ratio = sizeList[i][3];
            return true;
        }
    }

    CLOGE("Invalid preview size(%dx%d)", width, height);

    return false;
}

#ifdef USE_BINNING_MODE
int *ExynosCameraParameters::getBinningSizeTable(void) {
    int *sizeList = NULL;
    int index = 0;

    if (m_staticInfo->vtcallSizeLut == NULL
        || m_staticInfo->vtcallSizeLutMax == 0) {
        CLOGE("vtcallSizeLut is NULL");
        return sizeList;
    }

    for (index = 0; index < m_staticInfo->vtcallSizeLutMax; index++) {
        if (m_staticInfo->vtcallSizeLut[index][0] == m_cameraInfo.yuvSizeRatioId)
        break;
    }

    if (m_staticInfo->vtcallSizeLutMax <= index)
        index = 0;

    sizeList = m_staticInfo->vtcallSizeLut[index];

    return sizeList;
}
#endif

void ExynosCameraParameters::m_getSWVdisPreviewSize(int w, int h, int *newW, int *newH)
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

status_t ExynosCameraParameters::checkYuvFormat(const int format, const int outputPortId)
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

void ExynosCameraParameters::getPreviewSize(int *w, int *h)
{
    *w = m_cameraInfo.previewW;
    *h = m_cameraInfo.previewH;
}

void ExynosCameraParameters::m_setYuvSize(const int width, const int height, const int index)
{
    int widthArrayNum = sizeof(m_cameraInfo.yuvWidth) / sizeof(m_cameraInfo.yuvWidth[0]);
    int heightArrayNum = sizeof(m_cameraInfo.yuvHeight) / sizeof(m_cameraInfo.yuvHeight[0]);

    if (widthArrayNum != YUV_OUTPUT_PORT_ID_MAX
        || heightArrayNum != YUV_OUTPUT_PORT_ID_MAX) {
        android_printAssert(NULL, LOG_TAG, "ASSERT:Invalid yuvSize array length %dx%d."\
                            " YUV_OUTPUT_PORT_ID_MAX %d",
                            widthArrayNum, heightArrayNum,
                            YUV_OUTPUT_PORT_ID_MAX);
        return;
    }

    m_cameraInfo.yuvWidth[index] = width;
    m_cameraInfo.yuvHeight[index] = height;
}

void ExynosCameraParameters::getYuvSize(int *width, int *height, int index)
{
    int widthArrayNum = sizeof(m_cameraInfo.yuvWidth)/sizeof(m_cameraInfo.yuvWidth[0]);
    int heightArrayNum = sizeof(m_cameraInfo.yuvHeight)/sizeof(m_cameraInfo.yuvHeight[0]);

    if (widthArrayNum != YUV_OUTPUT_PORT_ID_MAX
            || heightArrayNum != YUV_OUTPUT_PORT_ID_MAX) {
        android_printAssert(NULL, LOG_TAG, "ASSERT:Invalid yuvSize array length %dx%d."\
                " YUV_OUTPUT_PORT_ID_MAX %d",
                widthArrayNum, heightArrayNum,
                YUV_OUTPUT_PORT_ID_MAX);
        return;
    }

    *width = m_cameraInfo.yuvWidth[index];
    *height = m_cameraInfo.yuvHeight[index];
}

void ExynosCameraParameters::resetYuvSize(void)
{
    memset(m_cameraInfo.yuvWidth, 0, sizeof(m_cameraInfo.yuvWidth));
    memset(m_cameraInfo.yuvHeight, 0, sizeof(m_cameraInfo.yuvHeight));
}

void ExynosCameraParameters::m_setHwYuvSize(const int width, const int height, const int index)
{
    int widthArrayNum = sizeof(m_cameraInfo.hwYuvWidth) / sizeof(m_cameraInfo.hwYuvWidth[0]);
    int heightArrayNum = sizeof(m_cameraInfo.hwYuvHeight) / sizeof(m_cameraInfo.hwYuvHeight[0]);

    if (widthArrayNum != YUV_OUTPUT_PORT_ID_MAX
        || heightArrayNum != YUV_OUTPUT_PORT_ID_MAX) {
        android_printAssert(NULL, LOG_TAG, "ASSERT:Invalid yuvSize array length %dx%d."\
                            " YUV_OUTPUT_PORT_ID_MAX %d",
                            widthArrayNum, heightArrayNum,
                            YUV_OUTPUT_PORT_ID_MAX);
        return;
    }

    m_cameraInfo.hwYuvWidth[index] = width;
    m_cameraInfo.hwYuvHeight[index] = height;
}

void ExynosCameraParameters::getHwYuvSize(int *width, int *height, int index)
{
    int widthArrayNum = sizeof(m_cameraInfo.hwYuvWidth)/sizeof(m_cameraInfo.hwYuvWidth[0]);
    int heightArrayNum = sizeof(m_cameraInfo.hwYuvHeight)/sizeof(m_cameraInfo.hwYuvHeight[0]);

    if (widthArrayNum != YUV_OUTPUT_PORT_ID_MAX
            || heightArrayNum != YUV_OUTPUT_PORT_ID_MAX) {
        android_printAssert(NULL, LOG_TAG, "ASSERT:Invalid yuvSize array length %dx%d."\
                " YUV_OUTPUT_PORT_ID_MAX %d",
                widthArrayNum, heightArrayNum,
                YUV_OUTPUT_PORT_ID_MAX);
        return;
    }

    *width = m_cameraInfo.hwYuvWidth[index];
    *height = m_cameraInfo.hwYuvHeight[index];
}

void ExynosCameraParameters::resetHwYuvSize(void)
{
    memset(m_cameraInfo.hwYuvWidth, 0, sizeof(m_cameraInfo.hwYuvWidth));
    memset(m_cameraInfo.hwYuvHeight, 0, sizeof(m_cameraInfo.hwYuvHeight));
}

void ExynosCameraParameters::setPreviewPortId(int outputPortId)
{
    m_previewPortId = outputPortId;
}

bool ExynosCameraParameters::isPreviewPortId(int outputPortId)
{
    bool result = false;

    if (m_previewPortId >= YUV_0 && m_previewPortId < YUV_MAX
        && outputPortId == m_previewPortId)
        result = true;
    else
        result = false;

    return result;
}

int ExynosCameraParameters::getPreviewPortId(void)
{
    return m_previewPortId;
}

void ExynosCameraParameters::setRecordingPortId(int outputPortId)
{
    m_recordingPortId = outputPortId;

    setRecordingHint(true);
}

bool ExynosCameraParameters::isRecordingPortId(int outputPortId)
{
    bool result = false;

    if (outputPortId == m_recordingPortId)
        result = true;
    else
        result = false;

    return result;
}

int ExynosCameraParameters::getRecordingPortId(void)
{
    return m_recordingPortId;
}

void ExynosCameraParameters::setYuvOutPortId(enum pipeline pipeId, int outputPortId)
{
    if (pipeId >= sizeof(m_yuvOutPortId) / sizeof(m_yuvOutPortId[0])) {
        CLOGE("Invalid pipeId %d", pipeId);
        return;
    }

    m_yuvOutPortId[pipeId] = outputPortId;
}

int ExynosCameraParameters::getYuvOutPortId(enum pipeline pipeId)
{
    if (pipeId >= sizeof(m_yuvOutPortId) / sizeof(m_yuvOutPortId[0])) {
        CLOGE("Invalid pipeId %d", pipeId);
        return -1;
    }

    return m_yuvOutPortId[pipeId];
}

void ExynosCameraParameters::getMaxSensorSize(int *w, int *h)
{
    *w = m_staticInfo->maxSensorW;
    *h = m_staticInfo->maxSensorH;
}

void ExynosCameraParameters::getSensorMargin(int *w, int *h)
{
    *w = m_staticInfo->sensorMarginW;
    *h = m_staticInfo->sensorMarginH;
}

void ExynosCameraParameters::m_adjustSensorMargin(int *sensorMarginW, int *sensorMarginH)
{
    float bnsRatio = 1.00f;
    float binningRatio = 1.00f;
    float sensorMarginRatio = 1.00f;

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

void ExynosCameraParameters::getMaxPreviewSize(int *w, int *h)
{
    *w = m_staticInfo->maxPreviewW;
    *h = m_staticInfo->maxPreviewH;
}

int ExynosCameraParameters::getBayerFormat(int pipeId)
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
    case PIPE_3AP:
    case PIPE_ISP:
    case PIPE_3AC:
    case PIPE_3AP_REPROCESSING:
    case PIPE_ISP_REPROCESSING:
        bayerFormat = CAMERA_3AP_BAYER_FORMAT;
        break;
    case PIPE_3AC_REPROCESSING:
        /* only RAW(DNG) format */
        bayerFormat = (m_useSensorPackedBayer == true) ? CAMERA_3AC_REPROCESSING_BAYER_FORMAT : V4L2_PIX_FMT_SBGGR16;
        break;
    default:
        CLOGW("Invalid pipeId(%d)", pipeId);
        break;
    }

    return bayerFormat;
}

void ExynosCameraParameters::m_setYuvFormat(const int format, const int index)
{
    int formatArrayNum = sizeof(m_cameraInfo.yuvFormat) / sizeof(m_cameraInfo.yuvFormat[0]);

    if (formatArrayNum != YUV_OUTPUT_PORT_ID_MAX) {
        android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):Invalid yuvFormat array length %d."\
                            " YUV_OUTPUT_PORT_ID_MAX %d",
                            __FUNCTION__, __LINE__,
                            formatArrayNum,
                            YUV_OUTPUT_PORT_ID_MAX);
        return;
    }

    m_cameraInfo.yuvFormat[index] = format;
}

int ExynosCameraParameters::getYuvFormat(const int index)
{
    int formatArrayNum = sizeof(m_cameraInfo.yuvFormat) / sizeof(m_cameraInfo.yuvFormat[0]);

    if (formatArrayNum != YUV_OUTPUT_PORT_ID_MAX) {
        android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):Invalid yuvFormat array length %d."\
                " YUV_OUTPUT_PORT_ID_MAX %d",
                __FUNCTION__, __LINE__,
                formatArrayNum,
                YUV_OUTPUT_PORT_ID_MAX);
        return - 1;
    }

    return m_cameraInfo.yuvFormat[index];
}

void ExynosCameraParameters::getHwPreviewSize(int *w, int *h)
{
    int previewPort = -1;

    previewPort = getPreviewPortId();

    if (previewPort >= YUV_0 && previewPort < YUV_MAX) {
        getHwYuvSize(w, h, previewPort);
    } else {
        CLOGE("Invalid port Id. Set to default yuv size.");
        getMaxPreviewSize(w, h);
    }
}

int ExynosCameraParameters::getHwPreviewFormat(void)
{
    CLOGV("m_cameraInfo.hwPreviewFormat(%d)", m_cameraInfo.hwPreviewFormat);

    return m_cameraInfo.hwPreviewFormat;
}

void ExynosCameraParameters::updateHwSensorSize(void)
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

void ExynosCameraParameters::m_setHwSensorSize(int w, int h)
{
    m_cameraInfo.hwSensorW = w;
    m_cameraInfo.hwSensorH = h;
}

void ExynosCameraParameters::getHwSensorSize(int *w, int *h)
{
    int width  = 0;
    int height = 0;
    int sizeList[SIZE_LUT_INDEX_END];

    /* matched ratio LUT is not existed, use equation */
    if (m_useSizeTable == true
        && m_getPreviewSizeList(sizeList) == NO_ERROR) {

        width = sizeList[SENSOR_W];
        height = sizeList[SENSOR_H];

    } else {
        width  = m_cameraInfo.hwSensorW;
        height = m_cameraInfo.hwSensorH;
    }

    *w = width;
    *h = height;
}

void ExynosCameraParameters::setBnsSize(int w, int h)
{
    m_cameraInfo.bnsW = w;
    m_cameraInfo.bnsH = h;

    updateHwSensorSize();
}

void ExynosCameraParameters::getBnsSize(int *w, int *h)
{
    *w = m_cameraInfo.bnsW;
    *h = m_cameraInfo.bnsH;
}

void ExynosCameraParameters::updateBinningScaleRatio(void)
{
    int ret = 0;
    uint32_t binningRatio = DEFAULT_BINNING_RATIO * 1000;

    if ((getRecordingHint() == true)
        && (getHighSpeedRecording() == true)) {
        int configMode = getConfigMode();
        switch (configMode) {
        case CONFIG_MODE::HIGHSPEED_60:
            binningRatio = 2000;
            break;
        case CONFIG_MODE::HIGHSPEED_120:
        case CONFIG_MODE::HIGHSPEED_240:
            binningRatio = 4000;
            break;
        default:
            CLOGE("Invalide configMode(%d)", configMode);
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

status_t ExynosCameraParameters::m_setBinningScaleRatio(int ratio)
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

uint32_t ExynosCameraParameters::getBinningScaleRatio(void)
{
    return m_cameraInfo.binningScaleRatio;
}

int ExynosCameraParameters::getBatchSize(enum pipeline pipeId)
{
    int batchSize = 1;
#ifdef SUPPORT_HFR_BATCH_MODE
    uint32_t minFps = 0, maxFps = 0;
    int yuvPortId = -1;
    getPreviewFpsRange(&minFps, &maxFps);

    /*
     * Default batchSize is MAX(1, maxFps/MULTI_BUFFER_BASE_FPS).
     * If specific pipe has different batchSize,
     * add case with pipeId.
     */
    switch (pipeId) {
    case PIPE_MCSC0:
    case PIPE_MCSC1:
    case PIPE_MCSC2:
        yuvPortId = pipeId - PIPE_MCSC0;
        if (this->useServiceBatchMode() == true) {
            batchSize = 1;
            break;
        } else if (this->isPreviewPortId(yuvPortId) == true) {
            /* Preview stream buffer is not delivered through every request */
            batchSize = 1;
            break;
        }
    default:
        batchSize = m_exynosconfig->current->bufInfo.num_batch_buffers;
        break;
    }
#endif

    if (pipeId >= PIPE_FLITE_REPROCESSING) {
        /* Reprocessing stream always uses single buffer scheme */
        batchSize = 1;
    }
    return batchSize;
}

bool ExynosCameraParameters::useServiceBatchMode(void)
{
#ifdef USE_SERVICE_BATCH_MODE
    return true;
#else
    return false;
#endif
}

bool ExynosCameraParameters::isCriticalSection(enum pipeline pipeId, enum critical_section_type type)
{
    bool flag = false;

    switch (type) {
    case CRITICAL_SECTION_TYPE_HWFC:
        if (m_getReprocessing3aaIspOtf() != HW_CONNECTION_MODE_M2M
            && pipeId == PIPE_3AA_REPROCESSING) {
            flag = true;
        } else if (pipeId == PIPE_ISP_REPROCESSING) {
            flag = true;
        } else if (pipeId == PIPE_HWFC_JPEG_DST_REPROCESSING) {
            flag = true;
        }
        break;
    case CRITICAL_SECTION_TYPE_VOTF:
#if 0 // For Lhotse Dual Scenario
        if (this->isDual() == false) {
            /* This critical section is only requred for Dual camera operation */
            flag = false;
            break;
        }

        if (pipeId == PIPE_ISP_MASTER
            || pipeId == PIPE_ISP_MASTER_REPROCESSING) {
            || pipeId == PIPE_DCPS0
            || pipeId == PIPE_DCPS1
            flag = true;
        }
#endif
        break;
    default:
        break;
    }

    return flag;
}

status_t ExynosCameraParameters::checkPictureSize(int pictureW, int pictureH)
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

status_t ExynosCameraParameters::m_adjustPictureSize(int *newPictureW, int *newPictureH,
                                                 int *newHwPictureW, int *newHwPictureH)
{
    int ret = 0;
    int newX = 0, newY = 0, newW = 0, newH = 0;
    float zoomRatio = 1.0f;

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

    ret = getCropRectAlign(*newHwPictureW, *newHwPictureH,
            *newPictureW, *newPictureH,
            &newX, &newY, &newW, &newH,
            CAMERA_BCROP_ALIGN, 2, zoomRatio);
    if (ret < 0) {
        CLOGE("getCropRectAlign(%d, %d, %d, %d) fail",
                *newHwPictureW, *newHwPictureH, *newPictureW, *newPictureH);
        return BAD_VALUE;
    }

    *newHwPictureW = newW;
    *newHwPictureH = newH;

    return NO_ERROR;
}

bool ExynosCameraParameters::m_isSupportedPictureSize(const int width,
                                                     const int height)
{
    int maxWidth, maxHeight = 0;
    int (*sizeList)[SIZE_OF_RESOLUTION];

#ifdef USE_BINNING_MODE
    if (m_binningProperty) {
        CLOGD("Do not check supported picture size at binning mode");
        return true;
    };
#endif

    getMaxPictureSize(&maxWidth, &maxHeight);

    if (maxWidth < width || maxHeight < height) {
        CLOGE("invalid picture Size(maxSize(%d/%d) size(%d/%d)",
            maxWidth, maxHeight, width, height);
        return false;
    }

    sizeList = m_staticInfo->jpegList;
    for (int i = 0; i < m_staticInfo->jpegListMax; i++) {
        if (sizeList[i][0] > maxWidth || sizeList[i][1] > maxHeight)
            continue;
        if (sizeList[i][0] == width && sizeList[i][1] == height) {
            m_cameraInfo.pictureSizeRatioId = sizeList[i][3];
            return true;
        }
    }

    CLOGE("Invalid picture size(%dx%d)", width, height);

    return false;
}

void ExynosCameraParameters::m_setPictureSize(int w, int h)
{
    m_cameraInfo.pictureW = w;
    m_cameraInfo.pictureH = h;
}

void ExynosCameraParameters::getPictureSize(int *w, int *h)
{
    *w = m_cameraInfo.pictureW;
    *h = m_cameraInfo.pictureH;
}

void ExynosCameraParameters::getMaxPictureSize(int *w, int *h)
{
    *w = m_staticInfo->maxPictureW;
    *h = m_staticInfo->maxPictureH;
}

void ExynosCameraParameters::m_setHwPictureSize(int w, int h)
{
    m_cameraInfo.hwPictureW = w;
    m_cameraInfo.hwPictureH = h;
}

void ExynosCameraParameters::getHwPictureSize(int *w, int *h)
{
    *w = m_cameraInfo.hwPictureW;
    *h = m_cameraInfo.hwPictureH;
}

void ExynosCameraParameters::m_setHwBayerCropRegion(int w, int h, int x, int y)
{
    Mutex::Autolock lock(m_parameterLock);

    m_cameraInfo.hwBayerCropW = w;
    m_cameraInfo.hwBayerCropH = h;
    m_cameraInfo.hwBayerCropX = x;
    m_cameraInfo.hwBayerCropY = y;
}

void ExynosCameraParameters::getHwBayerCropRegion(int *w, int *h, int *x, int *y)
{
    Mutex::Autolock lock(m_parameterLock);

    *w = m_cameraInfo.hwBayerCropW;
    *h = m_cameraInfo.hwBayerCropH;
    *x = m_cameraInfo.hwBayerCropX;
    *y = m_cameraInfo.hwBayerCropY;
}

void ExynosCameraParameters::m_setHwPictureFormat(int fmt)
{
    m_cameraInfo.hwPictureFormat = fmt;
}

int ExynosCameraParameters::getHwPictureFormat(void)
{
    return m_cameraInfo.hwPictureFormat;
}

void ExynosCameraParameters::m_setHwPicturePixelSize(camera_pixel_size pixelSize)
{
    m_cameraInfo.hwPicturePixelSize = pixelSize;
}

camera_pixel_size ExynosCameraParameters::getHwPicturePixelSize(void)
{
    return m_cameraInfo.hwPicturePixelSize;
}

status_t ExynosCameraParameters::checkJpegQuality(int quality)
{
    int curJpegQuality = -1;

    if (quality < 0 || quality > 100) {
        CLOGE("Invalid JPEG quality %d.", quality);
        return BAD_VALUE;
    }

    curJpegQuality = getJpegQuality();

    if (curJpegQuality != quality) {
        CLOGI("curJpegQuality %d newJpegQuality %d", curJpegQuality, quality);
        m_setJpegQuality(quality);
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setJpegQuality(int quality)
{
    m_cameraInfo.jpegQuality = quality;
}

int ExynosCameraParameters::getJpegQuality(void)
{
    return m_cameraInfo.jpegQuality;
}

status_t ExynosCameraParameters::checkThumbnailSize(int thumbnailW, int thumbnailH)
{
    int curThumbnailW = -1, curThumbnailH = -1;

    if (thumbnailW < 0 || thumbnailH < 0
        || thumbnailW > m_staticInfo->maxThumbnailW
        || thumbnailH > m_staticInfo->maxThumbnailH) {
        CLOGE("Invalide thumbnail size %dx%d", thumbnailW, thumbnailH);
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

void ExynosCameraParameters::m_setThumbnailSize(int w, int h)
{
    m_cameraInfo.thumbnailW = w;
    m_cameraInfo.thumbnailH = h;
}

void ExynosCameraParameters::getThumbnailSize(int *w, int *h)
{
    *w = m_cameraInfo.thumbnailW;
    *h = m_cameraInfo.thumbnailH;
}

void ExynosCameraParameters::getMaxThumbnailSize(int *w, int *h)
{
    *w = m_staticInfo->maxThumbnailW;
    *h = m_staticInfo->maxThumbnailH;
}

status_t ExynosCameraParameters::checkThumbnailQuality(int quality)
{
    int curThumbnailQuality = -1;

    if (quality < 0 || quality > 100) {
        CLOGE("Invalid thumbnail quality %d", quality);
        return BAD_VALUE;
    }
    curThumbnailQuality = getThumbnailQuality();

    if (curThumbnailQuality != quality) {
        CLOGI("curThumbnailQuality %d newThumbnailQuality %d", curThumbnailQuality, quality);
        m_setThumbnailQuality(quality);
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setThumbnailQuality(int quality)
{
    m_cameraInfo.thumbnailQuality = quality;
}

int ExynosCameraParameters::getThumbnailQuality(void)
{
    return m_cameraInfo.thumbnailQuality;
}

void ExynosCameraParameters::m_setOdcMode(bool toggle)
{
    m_cameraInfo.isOdcMode = toggle;
}

bool ExynosCameraParameters::getOdcMode(void)
{
    return m_cameraInfo.isOdcMode;
}

bool ExynosCameraParameters::getGDCEnabledMode(void)
{
    bool enabled = false;

#ifdef SUPPORT_HW_GDC
    enabled =  SUPPORT_HW_GDC;
#endif

    /* TODO: GDC scenario must be defined */

    return enabled;
}

status_t ExynosCameraParameters::setCropRegion(int x, int y, int w, int h)
{
    status_t ret = NO_ERROR;

    ret = setMetaCtlCropRegion(&m_metadata, x, y, w, h);
    if (ret != NO_ERROR) {
        CLOGE("Failed to setMetaCtlCropRegion(%d, %d, %d, %d)", x, y, w, h);
    }

    return ret;
}

void ExynosCameraParameters::m_getCropRegion(int *x, int *y, int *w, int *h)
{
    getMetaCtlCropRegion(&m_metadata, x, y, w, h);
}

status_t ExynosCameraParameters::m_setParamCropRegion(
        int srcW, int srcH,
        int dstW, int dstH)
{
    int newX = 0, newY = 0, newW = 0, newH = 0;
    float zoomRatio = getZoomRatio();

    if (getCropRectAlign(srcW,  srcH,
                         dstW,  dstH,
                         &newX, &newY,
                         &newW, &newH,
                         CAMERA_BCROP_ALIGN, 2,
                         zoomRatio) != NO_ERROR) {
        CLOGE("getCropRectAlign(%d, %d, %d, %d) zoomRatio(%f) fail",
            srcW,  srcH, dstW,  dstH, zoomRatio);
        return BAD_VALUE;
    }

    newX = ALIGN_UP(newX, 2);
    newY = ALIGN_UP(newY, 2);
    newW = srcW - (newX * 2);
    newH = srcH - (newY * 2);

    CLOGI("size0(%d, %d, %d, %d) zoomRatio(%f)",
        srcW, srcH, dstW, dstH, zoomRatio);
    CLOGI("size(%d, %d, %d, %d) zoomRatio(%f)",
        newX, newY, newW, newH, zoomRatio);

    m_setHwBayerCropRegion(newW, newH, newX, newY);

    return NO_ERROR;
}

void ExynosCameraParameters::m_setExifFixedAttribute(void)
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
        delete[] m_exifInfo.maker_note;

    m_exifInfo.maker_note_size = 98;
    m_exifInfo.maker_note = new unsigned char[m_exifInfo.maker_note_size];
    memset((void *)m_exifInfo.maker_note, 0, m_exifInfo.maker_note_size);
    /* 3 User Comments */
    if (m_exifInfo.user_comment)
        delete[] m_exifInfo.user_comment;

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

void ExynosCameraParameters::setExifChangedAttribute(exif_attribute_t   *exifInfo,
                                                     ExynosRect         *pictureRect,
                                                     ExynosRect         *thumbnailRect,
                                                     camera2_shot_t     *shot,
                                                     bool               useDebugInfo2)
{
    m_setExifChangedAttribute(exifInfo, pictureRect, thumbnailRect, shot, useDebugInfo2);
}

debug_attribute_t *ExynosCameraParameters::getDebugAttribute(void)
{
    return &mDebugInfo;
}

debug_attribute_t *ExynosCameraParameters::getDebug2Attribute(void)
{
    return &mDebugInfo2;
}

status_t ExynosCameraParameters::getFixedExifInfo(exif_attribute_t *exifInfo)
{
    if (exifInfo == NULL) {
        CLOGE(" buffer is NULL");
        return BAD_VALUE;
    }

    memcpy(exifInfo, &m_exifInfo, sizeof(exif_attribute_t));

    return NO_ERROR;
}

bool ExynosCameraParameters::getHdrMode(void)
{
    bool hdrMode = false;

    return hdrMode;
}

#ifdef USE_BINNING_MODE
int ExynosCameraParameters::getBinningMode(void)
{
    int ret = 0;

    if (m_staticInfo->vtcallSizeLutMax == 0 || m_staticInfo->vtcallSizeLut == NULL) {
       CLOGV("vtCallSizeLut is NULL, can't support the binnig mode");
       return ret;
    }

    /* For VT Call with DualCamera Scenario */
    if (getPIPMode() == true) {
        CLOGV("PIP Mode can't support the binnig mode.(%d,%d)", getCameraId(), getPIPMode());
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

int ExynosCameraParameters::getShotMode(void)
{
    return m_cameraInfo.shotMode;
}

void ExynosCameraParameters::m_setVtMode(int vtMode)
{
    m_cameraInfo.vtMode = vtMode;

    setMetaVtMode(&m_metadata, (enum camera_vt_mode)vtMode);
}

int ExynosCameraParameters::getVtMode(void)
{
    return m_cameraInfo.vtMode;
}

status_t ExynosCameraParameters::m_setImageUniqueId(const char *uniqueId)
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

const char *ExynosCameraParameters::getImageUniqueId(void)
{
#if defined(SAMSUNG_TN_FEATURE) && defined(SENSOR_FW_GET_FROM_FILE)
    char *sensorfw = NULL;
    char *uniqueid = NULL;
#ifdef FORCE_CAL_RELOAD
    char checkcal[50];

    memset(checkcal, 0, sizeof(checkcal));
#endif

    if (m_isUniqueIdRead == false) {
        sensorfw = (char *)getSensorFWFromFile(m_staticInfo, m_cameraId);
#ifdef FORCE_CAL_RELOAD
        snprintf(checkcal, sizeof(checkcal), "%s", sensorfw);
        m_calValid = m_checkCalibrationDataValid(checkcal);
#endif

        if (getCameraId() == CAMERA_ID_BACK) {
            uniqueid = sensorfw;
        } else {
#ifdef SAMSUNG_READ_ROM_FRONT
            if (SAMSUNG_READ_ROM_FRONT) {
                uniqueid = sensorfw;
            } else
#endif
            {
                uniqueid = strtok(sensorfw, " ");
            }
        }
        setImageUniqueId(uniqueid);
        m_isUniqueIdRead = true;
    }

    return (const char *)m_cameraInfo.imageUniqueId;
#else
    return m_cameraInfo.imageUniqueId;
#endif
}

#ifdef SAMSUNG_TN_FEATURE
void ExynosCameraParameters::setImageUniqueId(char *uniqueId)
{
    memcpy(m_cameraInfo.imageUniqueId, uniqueId, sizeof(m_cameraInfo.imageUniqueId));
}
#endif

#ifdef SAMSUNG_DOF
void ExynosCameraParameters::m_setLensPos(int pos)
{
    CLOGD("[DOF]position: %d", pos);
    setMetaCtlLensPos(&m_metadata, pos);
}
#endif

int ExynosCameraParameters::getSeriesShotMode(void)
{
    return m_cameraInfo.seriesShotMode;
}

int ExynosCameraParameters::getSeriesShotCount(void)
{
    return m_cameraInfo.seriesShotCount;
}

int ExynosCameraParameters::getMultiCaptureMode(void)
{
    return m_cameraInfo.multiCaptureMode;
}

void ExynosCameraParameters::setMultiCaptureMode(int captureMode)
{
    m_cameraInfo.multiCaptureMode = captureMode;
}

int ExynosCameraParameters::getFocalLengthIn35mmFilm(void)
{
    return m_staticInfo->focalLengthIn35mmLength;
}

float ExynosCameraParameters::getMaxZoomRatio(void)
{
    return (float)m_staticInfo->maxZoomRatio;
}

void ExynosCameraParameters::setZoomRatio(float zoomRatio)
{
    m_metaParameters.m_zoomRatio = zoomRatio;
}

float ExynosCameraParameters::getZoomRatio(void)
{
    return m_metaParameters.m_zoomRatio;
}

void ExynosCameraParameters::m_initMetadata(void)
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

    int minFps = m_staticInfo->minFps;
    int maxFps = m_staticInfo->maxFps;

    /* The min fps can not be '0'. Therefore it is set up default value '15'. */
    if (minFps == 0) {
        CLOGW(" Invalid min fps value(%d)", minFps);
        minFps = 7;
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
    if (m_setParamCropRegion(m_staticInfo->maxPictureW, m_staticInfo->maxPictureH,
                             m_staticInfo->maxPreviewW, m_staticInfo->maxPreviewH) != NO_ERROR) {
        CLOGE("m_setParamCropRegion() fail");
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
    m_metadata.shot.uctl.companionUd.drc_mode = COMPANION_DRC_OFF;
    m_metadata.shot.uctl.companionUd.paf_mode = COMPANION_PAF_OFF;
    m_metadata.shot.uctl.companionUd.wdr_mode = COMPANION_WDR_OFF;

    /* 4. udm */

    /* magicNumber */
    shot->magicNumber = SHOT_MAGIC_NUMBER;

    for (int i = 0; i < INTERFACE_TYPE_MAX; i++) {
        shot->uctl.scalerUd.mcsc_sub_blk_port[i] = MCSC_PORT_NONE;
    }

    setMetaSetfile(&m_metadata, 0x0);

    /* user request */
    m_metadata.drc_bypass = 1;
    m_metadata.dis_bypass = 1;
    m_metadata.dnr_bypass = 1;
    m_metadata.fd_bypass  = 1;
}

status_t ExynosCameraParameters::init(void)
{
#ifdef HAL3_YUVSIZE_BASED_BDS
    status_t ret;

    /* Reset all the YUV output sizes to smallest one
    To make sure the prior output setting do not affect current session.
    */
    ret = initYuvSizes();
    if (ret < 0) {
        CLOGE("clearYuvSizes() failed!! status(%d)", ret);
        return ret;
    }
    /* To find the minimum sized YUV stream in stream configuration, reset the old data */
#endif

    m_cameraInfo.yuvSizeLutIndex = -1;
    m_cameraInfo.pictureSizeLutIndex = -1;

    m_previewPortId = -1;
    m_recordingPortId = -1;

    for (int i = 0; i < MAX_PIPE_NUM; i++) {
        m_yuvOutPortId[i] = -1;
    }

    resetMinYuvSize();
    resetMaxYuvSize();
    resetYuvSize();
    resetMaxHwYuvSize();
    resetHwYuvSize();
    resetYuvBufferCount();
    resetYuvSizeRatioId();
    setUseFullSizeLUT(false);
    setUseSensorPackedBayer(true);
    setRecordingHint(false);

    m_vendorInit();

    m_initMetadata();

    return OK;
}

status_t ExynosCameraParameters::duplicateCtrlMetadata(void *buf)
{
    if (buf == NULL) {
        CLOGE("ERR: buf is NULL");
        return BAD_VALUE;
    }

    struct camera2_shot_ext *meta_shot_ext = (struct camera2_shot_ext *)buf;
    memcpy(&meta_shot_ext->shot.ctl, &m_metadata.shot.ctl, sizeof(struct camera2_ctl));

#ifdef SAMSUNG_RTHDR
    meta_shot_ext->shot.uctl.companionUd.wdr_mode = getRTHdr();
#endif
#ifdef SAMSUNG_PAF
    meta_shot_ext->shot.uctl.companionUd.paf_mode = getPaf();
#endif

#ifdef SUPPORT_DEPTH_MAP
    meta_shot_ext->shot.uctl.companionUd.disparity_mode = m_metadata.shot.uctl.companionUd.disparity_mode;
#endif

    return NO_ERROR;
}

ExynosCameraActivityControl *ExynosCameraParameters::getActivityControl(void)
{
    return m_activityControl;
}

void ExynosCameraParameters::setFlipHorizontal(int val)
{
    if (val < 0) {
        CLOGE(" setFlipHorizontal ignored, invalid value(%d)", val);
        return;
    }

    m_cameraInfo.flipHorizontal = val;
}

int ExynosCameraParameters::getFlipHorizontal(void)
{
    return m_cameraInfo.flipHorizontal;
}

void ExynosCameraParameters::setFlipVertical(int val)
{
    if (val < 0) {
        CLOGE(" setFlipVertical ignored, invalid value(%d)", val);
        return;
    }

    m_cameraInfo.flipVertical = val;
}

int ExynosCameraParameters::getFlipVertical(void)
{
    return m_cameraInfo.flipVertical;
}

#ifdef LLS_CAPTURE
void ExynosCameraParameters::setLLSValue(int value)
{
    m_LLSValue = value;
}

int ExynosCameraParameters::getLLSValue(void)
{
    return m_LLSValue;
}

void ExynosCameraParameters::setLLSOn(uint32_t enable)
{
    m_flagLLSOn = enable;
}

bool ExynosCameraParameters::getLLSOn(void)
{
    return m_flagLLSOn;
}

#ifdef SET_LLS_CAPTURE_SETFILE
void ExynosCameraParameters::setLLSCaptureOn(bool enable)
{
    m_LLSCaptureOn = enable;
}

int ExynosCameraParameters::getLLSCaptureOn()
{
    return m_LLSCaptureOn;
}
#endif
#endif

#ifdef OIS_CAPTURE
void ExynosCameraParameters::setOISCaptureModeOn(bool enable)
{
    m_flagOISCaptureOn = enable;
}

bool ExynosCameraParameters::getOISCaptureModeOn(void)
{
    return m_flagOISCaptureOn;
}
#endif

bool ExynosCameraParameters::setDeviceOrientation(int orientation)
{
    if (orientation < 0 || orientation % 90 != 0) {
        CLOGE("Invalid orientation (%d)", orientation);
        return false;
    }

    m_cameraInfo.deviceOrientation = orientation;

    /* fd orientation need to be calibrated, according to f/w spec */
    int hwRotation = BACK_ROTATION;

    if (this->getCameraId() == CAMERA_ID_FRONT)
        hwRotation = FRONT_ROTATION;

    int fdOrientation = (orientation + hwRotation) % 360;

    CLOGD("orientation(%d), hwRotation(%d), fdOrientation(%d)", orientation, hwRotation, fdOrientation);

    return true;
}

int ExynosCameraParameters::getDeviceOrientation(void)
{
    return m_cameraInfo.deviceOrientation;
}

int ExynosCameraParameters::getFdOrientation(void)
{
    /* Calibrate FRONT FD orientation */
    if (getCameraId() == CAMERA_ID_FRONT) {
        return (m_cameraInfo.deviceOrientation + FRONT_ROTATION + 180) % 360;
    } else {
        return (m_cameraInfo.deviceOrientation + BACK_ROTATION) % 360;
    }
}

void ExynosCameraParameters::getSetfileYuvRange(bool flagReprocessing, int *setfile, int *yuvRange)
{
    if (flagReprocessing == true) {
        *setfile = m_setfileReprocessing;
        *yuvRange = m_yuvRangeReprocessing;
    } else {
        *setfile = m_setfile;
        *yuvRange = m_yuvRange;
    }
}

status_t ExynosCameraParameters::checkSetfileYuvRange(void)
{
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

void ExynosCameraParameters::setSetfileYuvRange(void)
{
    /* reprocessing */
    m_getSetfileYuvRange(true, &m_setfileReprocessing, &m_yuvRangeReprocessing);

    CLOGD("m_cameraId(%d) : general[setfile(%d) YUV range(%d)] : reprocesing[setfile(%d) YUV range(%d)]",
        m_cameraId,
        m_setfile, m_yuvRange,
        m_setfileReprocessing, m_yuvRangeReprocessing);

}

void ExynosCameraParameters::setSetfileYuvRange(bool flagReprocessing, int setfile, int yuvRange)
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

void ExynosCameraParameters::m_getSetfileYuvRange(bool flagReprocessing, int *setfile, int *yuvRange)
{
    uint32_t currentSetfile = 0;
    uint32_t currentScenario = 0;
    uint32_t stateReg = 0;
    int flagYUVRange = YUV_FULL_RANGE;

    unsigned int minFps = 0;
    unsigned int maxFps = 0;
    getPreviewFpsRange(&minFps, &maxFps);

#ifdef SAMSUNG_SW_VDIS
    if (getSWVdisMode()) {
        currentScenario = FIMC_IS_SCENARIO_SWVDIS;
    }
#endif

#ifdef SAMSUNG_COLOR_IRIS
    if (getShotMode() == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_COLOR_IRIS) {
        currentScenario = FIMC_IS_SCENARIO_COLOR_IRIS;
    }
#endif

    if (m_getUseFullSizeLUT() == true) {
        currentScenario = FIMC_IS_SCENARIO_FULL_SIZE;
    }

#ifdef SAMSUNG_RTHDR
    if (getRTHdr() == COMPANION_WDR_ON)
        stateReg |= STATE_REG_RTHDR_ON;
    else if (getRTHdr() == COMPANION_WDR_AUTO
#ifdef SUPPORT_WDR_AUTO_LIKE
            || getRTHdr() == COMPANION_WDR_AUTO_LIKE
#endif
        )
        stateReg |= STATE_REG_RTHDR_AUTO;
#endif

    if (m_isUHDRecordingMode() == true)
        stateReg |= STATE_REG_UHD_RECORDING;

    if (getPIPMode() == true) {
        stateReg |= STATE_REG_DUAL_MODE;
        if (getPIPRecordingHint() == true)
            stateReg |= STATE_REG_DUAL_RECORDINGHINT;
    } else {
        if (getRecordingHint() == true)
            stateReg |= STATE_REG_RECORDINGHINT;
    }

    if (flagReprocessing == true) {
        stateReg |= STATE_REG_FLAG_REPROCESSING;

#ifdef SET_LLS_CAPTURE_SETFILE
        if (getLLSCaptureOn() == true)
            stateReg |= STATE_REG_NEED_LLS;
#endif

        if (getLongExposureShotCount() > 0)
            stateReg |= STATE_STILL_CAPTURE_LONG;
#ifdef LLS_CAPTURE
        else if (getLongExposureShotCount() == 0
                 && getLLSValue() == LLS_LEVEL_MANUAL_ISO)
            stateReg |= STATE_REG_MANUAL_ISO;
#ifdef SAMSUNG_RTHDR
        if (getRTHdr() == COMPANION_WDR_AUTO
            && getLLSValue() == LLS_LEVEL_SHARPEN_SINGLE)
            stateReg |= STATE_REG_SHARPEN_SINGLE;
#endif
#endif

#ifdef SAMSUNG_HIFI_CAPTURE
        if (getRecordingHint() == false
#ifdef SET_LLS_CAPTURE_SETFILE
            && getLLSCaptureOn() == false
#endif
            && getLLSValue() != LLS_LEVEL_SHARPEN_SINGLE
            && getLLSValue() != LLS_LEVEL_MANUAL_ISO
            && getLongExposureShotCount() == 0) {
            float zoomRatio = getZoomRatio();

            if (zoomRatio >= 3.0f && zoomRatio < 4.0f) {
                stateReg |= STATE_REG_ZOOM;
                CLOGV("currentSetfile zoom");
            } else if (zoomRatio >= 4.0f) {
                if (getLDCaptureMode() == MULTI_SHOT_MODE_SR) {
                    stateReg |= STATE_REG_ZOOM_OUTDOOR;
                    CLOGV("currentSetfile zoomoutdoor");
                } else {
                    stateReg |= STATE_REG_ZOOM_INDOOR;
                    CLOGV("currentSetfile zoomindoor");
                }
            }
        }
#endif
    } else if (flagReprocessing == false) {
        if (stateReg & STATE_REG_RECORDINGHINT
            || stateReg & STATE_REG_UHD_RECORDING
            || stateReg & STATE_REG_DUAL_RECORDINGHINT
#ifdef SAMSUNG_COLOR_IRIS
            || getShotMode() == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_COLOR_IRIS
#endif
            ) {
            flagYUVRange = YUV_LIMITED_RANGE;
        }

#ifdef USE_BINNING_MODE
        if (getBinningMode() == true
#ifndef SET_PREVIEW_BINNING_SETFILE
            && m_cameraId == CAMERA_ID_BACK
#endif
        ) {
            stateReg |= STATE_REG_BINNING_MODE;
        }
#endif
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
#ifdef SAMSUNG_TN_FEATURE
        } else if (getIntelligentMode() == INTELLIGENT_MODE_SMART_STAY) {
            currentSetfile = ISS_SUB_SCENARIO_FRONT_SMART_STAY;
        } else if (getShotMode() == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_WIDE_SELFIE) {
            currentSetfile = ISS_SUB_SCENARIO_FRONT_PANORAMA;
#endif
#ifdef SAMSUNG_COLOR_IRIS
        } else if (getShotMode() == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_COLOR_IRIS) {
            // It is temporary code for preview of color iris.
            // Need to include subscenario id "ISS_SUB_SCENARIO_FRONT_COLOR_IRIS_PREVIEW" in RTA.
            // If include, currentSetfile will change.
            currentSetfile = ISS_SUB_SCENARIO_STILL_PREVIEW;
#endif
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
                currentSetfile = BINNING_SETFILE_INDEX;
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

#ifdef SAMSUNG_HIFI_CAPTURE
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
#if (LLS_SETFILE_VERSION == 2)
            case STATE_STILL_CAPTURE_LLS:
                currentSetfile = ISS_SUB_SCENARIO_MERGED_STILL_CAPTURE;
                break;
            case STATE_VIDEO_CAPTURE_WDR_ON_LLS:
                currentSetfile = ISS_SUB_SCENARIO_MERGED_STILL_CAPTURE_WDR_ON;
                break;
            case STATE_STILL_CAPTURE_WDR_AUTO_LLS:
                currentSetfile = ISS_SUB_SCENARIO_MERGED_STILL_CAPTURE_WDR_AUTO;
                break;
#else
            case STATE_STILL_CAPTURE_LLS:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_LLS;
                break;
            case STATE_VIDEO_CAPTURE_WDR_ON_LLS:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_WDR_ON_LLS;
                break;
            case STATE_STILL_CAPTURE_WDR_AUTO_LLS:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_LLS;
                break;
#endif /* LLS_SETFILE_VERSION == 2 */
#endif /* SET_LLS_CAPTURE_SETFILE */
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
    CLOGD("getPIPMode()(%d)", getPIPMode());
    CLOGD("getPIPRecordingHint()(%d)", getPIPRecordingHint());
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

    *setfile = currentSetfile | (currentScenario << 16);
    *yuvRange = flagYUVRange;
}

void ExynosCameraParameters::setUseSensorPackedBayer(bool enable)
{
#ifdef CAMERA_PACKED_BAYER_ENABLE
    m_useSensorPackedBayer = enable;
#else
    m_useSensorPackedBayer = false;
#endif
    CLOGD("PackedBayer %s", (m_useSensorPackedBayer == true) ? "ENABLE" : "DISABLE");
}

void ExynosCameraParameters::setUseDynamicBayer(bool enable)
{
    m_useDynamicBayer = enable;
}

bool ExynosCameraParameters::getUseDynamicBayer(void)
{
    int configMode = getConfigMode();

    switch (configMode) {
    case CONFIG_MODE::NORMAL:
        return m_useDynamicBayer;
    case CONFIG_MODE::HIGHSPEED_60:
        return m_useDynamicBayer60Fps;
    case CONFIG_MODE::HIGHSPEED_120:
        return m_useDynamicBayer120Fps;
    case CONFIG_MODE::HIGHSPEED_240:
        return m_useDynamicBayer240Fps;
    default:
        CLOGE("configMode is abnormal(%d)", configMode);
        break;
    }

    return m_useDynamicBayer;
}

void ExynosCameraParameters::setUseFastenAeStable(bool enable)
{
    m_useFastenAeStable = enable;
}

bool ExynosCameraParameters::getUseFastenAeStable(void)
{
    return m_useFastenAeStable;
}

bool ExynosCameraParameters::isFastenAeStableEnable(void)
{
    bool ret = false;

    if (getUseFastenAeStable() == true
#ifndef USE_DUAL_CAMERA
        && getPIPMode() == false
#endif
        && getHighSpeedRecording() == false
        && getVisionMode() == false) {
        ret = true;
    } else {
        return false;
    }

#ifdef USE_DUAL_CAMERA
    /*
     *              index  LIVE_OUTFOCUS getDualCameraMode getPIPMode W(fastenAe) T(fastenAe)  Front(other)
     *    single      1       FALSE           FALSE           FALSE      ENABLE       ENABLE      ENABLE
     *    PIP         2       FALSE           FALSE           TRUE       DISABLE      DISABLE      NONE
     *    1-device    3       FALSE           TRUE            TRUE       ENABLE       DISABLE      NONE
     *    2-device    4       TRUE            FALSE           FALSE      DISABLE      ENABLE       NONE
     */

    bool dualMode = getPIPMode();
    int shotMode = getShotMode();

    /* ToDo: get Camera ID from dual instance (getCameraId()) */
    int cameraId = CAMERA_ID_BACK_0;

    /* ToDo: get dual Ccamera mode from application (getDualCameraMode();) */
    bool dualCameraMode = false;

    switch(cameraId) {
        case CAMERA_ID_BACK_0:
            /* index :1 / 3 */
            if (((dualCameraMode == false && dualMode == false) || (dualCameraMode == true && dualMode == true))
#ifdef SAMSUNG_TN_FEATURE
                && shotMode != SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_SELECTIVE_FOCUS
#endif
                ) {
                ret = true;
            } else {
                ret = false;
            }
            break;
        case CAMERA_ID_BACK_1:
            /* index :2 / 3 */
            if (((dualCameraMode == false && dualMode == true) || (dualCameraMode == true && dualMode == true))
#ifdef SAMSUNG_TN_FEATURE
                && shotMode != SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_SELECTIVE_FOCUS
#endif
                ) {
                ret = false;
            } else {
                ret = true;
            }
            break;
        default:
            /* index :1 */
            if (dualCameraMode == false && dualMode == false
#ifdef SAMSUNG_TN_FEATURE
                && shotMode != SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_SELECTIVE_FOCUS
#endif
                ) {
                ret = true;
            } else {
                ret = false;
            }
            break;
    }
#endif

    return ret;
}
status_t ExynosCameraParameters::m_getSizeListIndex(int (*sizelist)[SIZE_OF_LUT], int listMaxSize, int ratio, int *index)
{
    if (*index == -1) {
        for (int i = 0; i < listMaxSize; i++) {
            if (sizelist[i][RATIO_ID] == ratio) {
                *index = i;
                break;
            }
        }
    }

    if (*index == -1) {
        return BAD_VALUE;
    }

    return NO_ERROR;
}

status_t ExynosCameraParameters::getPreviewBayerCropSize(ExynosRect *srcRect, ExynosRect *dstRect, bool applyZoom)
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
        || m_getPreviewSizeList(sizeList) != NO_ERROR)
        return calcPreviewBayerCropSize(srcRect, dstRect);

    /* use LUT */
    hwBnsW = sizeList[BNS_W];
    hwBnsH = sizeList[BNS_H];
    hwBcropW = sizeList[BCROP_W];
    hwBcropH = sizeList[BCROP_H];

    int curBnsW = 0, curBnsH = 0;
    getBnsSize(&curBnsW, &curBnsH);
    if (SIZE_RATIO(curBnsW, curBnsH) != SIZE_RATIO(hwBnsW, hwBnsH)) {
        CLOGW("current BNS size(%dx%d) is NOT same with Hw BNS size(%dx%d)",
                 curBnsW, curBnsH, hwBnsW, hwBnsH);
    }

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

    ExynosRect activeArraySize;
    ExynosRect cropRegion;
    ExynosRect hwActiveArraySize;
    float scaleRatioW = 0.0f, scaleRatioH = 0.0f;
    status_t ret = NO_ERROR;

    if (applyZoom == true && isUse3aaInputCrop() == true) {
        getMaxSensorSize(&activeArraySize.w, &activeArraySize.h);
        m_getCropRegion(&cropRegion.x, &cropRegion.y, &cropRegion.w, &cropRegion.h);

        CLOGV("ActiveArraySize %dx%d(%d) CropRegion %d,%d %dx%d(%d) HWSensorSize %dx%d(%d) BcropSize %dx%d(%d)",
                activeArraySize.w, activeArraySize.h, SIZE_RATIO(activeArraySize.w, activeArraySize.h),
                cropRegion.x, cropRegion.y, cropRegion.w, cropRegion.h, SIZE_RATIO(cropRegion.w, cropRegion.h),
                hwBnsW, hwBnsH, SIZE_RATIO(hwBnsW, hwBnsH),
                hwBcropW, hwBcropH, SIZE_RATIO(hwBcropW, hwBcropH));

        /* Calculate H/W active array size for current sensor aspect ratio
           based on active array size
         */
        ret = getCropRectAlign(activeArraySize.w, activeArraySize.h,
                               hwBnsW, hwBnsH,
                               &hwActiveArraySize.x, &hwActiveArraySize.y,
                               &hwActiveArraySize.w, &hwActiveArraySize.h,
                               2, 2, 1.0f);
        if (ret != NO_ERROR) {
            CLOGE("Failed to getCropRectAlign. Src %dx%d, Dst %dx%d",
                    activeArraySize.w, activeArraySize.h,
                    hwBnsW, hwBnsH);
            return INVALID_OPERATION;
        }

        /* Scale down the crop region & HW active array size
           to adjust them to the 3AA input size without sensor margin
         */
        scaleRatioW = (float) hwBnsW / (float) hwActiveArraySize.w;
        scaleRatioH = (float) hwBnsH / (float) hwActiveArraySize.h;

        cropRegion.x = ALIGN_DOWN((int) (((float) cropRegion.x) * scaleRatioW), 2);
        cropRegion.y = ALIGN_DOWN((int) (((float) cropRegion.y) * scaleRatioH), 2);
        cropRegion.w = (int) ((float) cropRegion.w) * scaleRatioW;
        cropRegion.h = (int) ((float) cropRegion.h) * scaleRatioH;

        hwActiveArraySize.x = ALIGN_DOWN((int) (((float) hwActiveArraySize.x) * scaleRatioW), 2);
        hwActiveArraySize.y = ALIGN_DOWN((int) (((float) hwActiveArraySize.y) * scaleRatioH), 2);
        hwActiveArraySize.w = (int) (((float) hwActiveArraySize.w) * scaleRatioW);
        hwActiveArraySize.h = (int) (((float) hwActiveArraySize.h) * scaleRatioH);

        if (cropRegion.w < 1 || cropRegion.h < 1) {
            CLOGW("Invalid scaleRatio %fx%f, cropRegion' %d,%d %dx%d.",
                    scaleRatioW, scaleRatioH,
                    cropRegion.x, cropRegion.y, cropRegion.w, cropRegion.h);
            cropRegion.x = 0;
            cropRegion.y = 0;
            cropRegion.w = hwBnsW;
            cropRegion.h = hwBnsH;
        }

        /* Calculate HW bcrop size inside crop region' */
        if ((cropRegion.w > hwBcropW) && (cropRegion.h > hwBcropH)) {
            dstRect->x = ALIGN_DOWN(((cropRegion.w - hwBcropW) >> 1), 2);
            dstRect->y = ALIGN_DOWN(((cropRegion.h - hwBcropH) >> 1), 2);
            dstRect->w = hwBcropW;
            dstRect->h = hwBcropH;
        } else {
            ret = getCropRectAlign(cropRegion.w, cropRegion.h,
                    hwBcropW, hwBcropH,
                    &(dstRect->x), &(dstRect->y),
                    &(dstRect->w), &(dstRect->h),
                    CAMERA_BCROP_ALIGN, 2, 1.0f);
            if (ret != NO_ERROR) {
                CLOGE("Failed to getCropRectAlign. Src %dx%d, Dst %dx%d",
                        cropRegion.w, cropRegion.h,
                        hwBcropW, hwBcropH);
                return INVALID_OPERATION;
            }

            dstRect->x = ALIGN_DOWN(dstRect->x, 2);
            dstRect->y = ALIGN_DOWN(dstRect->y, 2);
        }

        /* Add crop region offset to HW bcrop offset */
        dstRect->x += cropRegion.x;
        dstRect->y += cropRegion.y;

        /* Check the boundary of HW active array size for HW bcrop offset & size */
        if (dstRect->x < hwActiveArraySize.x) dstRect->x = hwActiveArraySize.x;
        if (dstRect->y < hwActiveArraySize.y) dstRect->y = hwActiveArraySize.y;
        if (dstRect->x + dstRect->w > hwActiveArraySize.x + hwBnsW)
            dstRect->x = hwActiveArraySize.x + hwBnsW - dstRect->w;
        if (dstRect->y + dstRect->h > hwActiveArraySize.y + hwBnsH)
            dstRect->y = hwActiveArraySize.y + hwBnsH - dstRect->h;

        /* Remove HW active array size offset */
        dstRect->x -= hwActiveArraySize.x;
        dstRect->y -= hwActiveArraySize.y;
    } else {
        /* Calculate HW bcrop size inside HW sensor output size */
        if ((hwBnsW > hwBcropW) && (hwBnsH > hwBcropH)) {
            dstRect->x = ALIGN_DOWN(((hwBnsW - hwBcropW) >> 1), 2);
            dstRect->y = ALIGN_DOWN(((hwBnsH - hwBcropH) >> 1), 2);
            dstRect->w = hwBcropW;
            dstRect->h = hwBcropH;
        } else {
            ret = getCropRectAlign(hwBnsW, hwBnsH,
                                   hwBcropW, hwBcropH,
                                   &(dstRect->x), &(dstRect->y),
                                   &(dstRect->w), &(dstRect->h),
                                   CAMERA_BCROP_ALIGN, 2, 1.0f);
            if (ret != NO_ERROR) {
                CLOGE("Failed to getCropRectAlign. Src %dx%d, Dst %dx%d",
                        hwBnsW, hwBnsH,
                        hwBcropW, hwBcropH);
                return INVALID_OPERATION;
            }
        }
    }

    CLOGV("HWBcropSize %d,%d %dx%d(%d)",
            dstRect->x, dstRect->y, dstRect->w, dstRect->h, SIZE_RATIO(dstRect->w, dstRect->h));

    /* Compensate the crop size to satisfy Max Scale Up Ratio */
    if (dstRect->w * SCALER_MAX_SCALE_UP_RATIO < hwBnsW
        || dstRect->h * SCALER_MAX_SCALE_UP_RATIO < hwBnsH) {
        dstRect->w = ALIGN_UP((int)ceil((float)hwBnsW / SCALER_MAX_SCALE_UP_RATIO), CAMERA_BCROP_ALIGN);
        dstRect->h = ALIGN_UP((int)ceil((float)hwBnsH / SCALER_MAX_SCALE_UP_RATIO), CAMERA_BCROP_ALIGN);
    }

    /* Compensate the crop size to satisfy Max Scale Up Ratio on reprocessing path */
    if (getUsePureBayerReprocessing() == false
        && m_cameraInfo.pictureSizeRatioId != m_cameraInfo.yuvSizeRatioId) {
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
                1.0f);
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
            /* TODO: Error handling must be implemented */
            dstRect->x = cropRegion.x;
            dstRect->y = cropRegion.y;
            dstRect->w = cropRegion.w;
            dstRect->h = cropRegion.h;
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

status_t ExynosCameraParameters::calcPreviewBayerCropSize(ExynosRect *srcRect, ExynosRect *dstRect)
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
            1.0f);

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
                1.0f);

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

status_t ExynosCameraParameters::calcPictureBayerCropSize(ExynosRect *srcRect, ExynosRect *dstRect)
{
    int ret = 0;

    int maxSensorW = 0, maxSensorH = 0;
    int hwSensorW = 0, hwSensorH = 0;
    int hwPictureW = 0, hwPictureH = 0;
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
                1.0f);

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
                     1.0f);

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

status_t ExynosCameraParameters::getPreviewBdsSize(ExynosRect *dstRect, bool applyZoom)
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
        getHwYuvSize(&yuvWidth, &yuvHeight, i);

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
    CLOGD("hwBdsSize %dx%d", dstRect->w, dstRect->h);
#endif

    return ret;
}

status_t ExynosCameraParameters::calcPreviewBDSSize(ExynosRect *srcRect, ExynosRect *dstRect)
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
    CLOGE("BDS %dx%d Preview %dx%d", dstRect->w, dstRect->h, previewW, previewH);
#endif

    return NO_ERROR;
}

status_t ExynosCameraParameters::getPictureBdsSize(ExynosRect *dstRect)
{
    status_t ret = NO_ERROR;
    ExynosRect bnsSize;
    ExynosRect bayerCropSize;
    int hwBdsW = 0;
    int hwBdsH = 0;
    int sizeList[SIZE_LUT_INDEX_END];

    /* matched ratio LUT is not existed, use equation */
    if (m_useSizeTable == false
        || m_getPictureSizeList(sizeList) != NO_ERROR) {
        ExynosRect rect;
        return calcPictureBDSSize(&rect, dstRect);
    }

    /* use LUT */
    hwBdsW = sizeList[BDS_W];
    hwBdsH = sizeList[BDS_H];

    /* Check the invalid BDS size compared to Bcrop size */
    ret = getPictureBayerCropSize(&bnsSize, &bayerCropSize);
    if (ret != NO_ERROR)
        CLOGE("Failed to getPictureBayerCropSize()");

    if (bayerCropSize.w < hwBdsW || bayerCropSize.h < hwBdsH) {
        CLOGV("bayerCropSize %dx%d is smaller than BDSSize %dx%d. Force bayerCropSize",
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

status_t ExynosCameraParameters::getPreviewYuvCropSize(ExynosRect *yuvCropSize)
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

status_t ExynosCameraParameters::getPictureYuvCropSize(ExynosRect *yuvCropSize)
{
    status_t ret = NO_ERROR;
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
    CLOGD("ISPS %dx%d YuvCrop %d,%d %dx%d zoomRatio %f",
            ispInputSize.w, ispInputSize.h,
            yuvCropSize->x, yuvCropSize->y, yuvCropSize->w, yuvCropSize->h,
            zoomRatio);
#endif

    return ret;
}


status_t ExynosCameraParameters::getFastenAeStableSensorSize(int *hwSensorW, int *hwSensorH, int index)
{
    *hwSensorW = m_staticInfo->fastAeStableLut[index][SENSOR_W];
    *hwSensorH = m_staticInfo->fastAeStableLut[index][SENSOR_H];

    return NO_ERROR;
}

status_t ExynosCameraParameters::getFastenAeStableBcropSize(int *hwBcropW, int *hwBcropH, int index)
{
    *hwBcropW = m_staticInfo->fastAeStableLut[index][BCROP_W];
    *hwBcropH = m_staticInfo->fastAeStableLut[index][BCROP_H];

    return NO_ERROR;
}

status_t ExynosCameraParameters::getFastenAeStableBdsSize(int *hwBdsW, int *hwBdsH, int index)
{
    *hwBdsW = m_staticInfo->fastAeStableLut[index][BDS_W];
    *hwBdsH = m_staticInfo->fastAeStableLut[index][BDS_H];

    return NO_ERROR;
}

status_t ExynosCameraParameters::calcPictureBDSSize(ExynosRect *srcRect, ExynosRect *dstRect)
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
    CLOGE("hwBdsSize %dx%d Picture %dx%d", dstRect->w, dstRect->h, pictureW, pictureH);
#endif

    return NO_ERROR;
}

bool ExynosCameraParameters::getUsePureBayerReprocessing(void)
{
    int oldMode = m_usePureBayerReprocessing;
    int cameraId = getCameraId();

#ifdef USE_DUAL_CAMERA
    switch (cameraId) {
    case CAMERA_ID_BACK_0:
    case CAMERA_ID_BACK_1:
        cameraId = CAMERA_ID_BACK;
        break;
    case CAMERA_ID_FRONT_0:
    case CAMERA_ID_FRONT_1:
        cameraId = CAMERA_ID_FRONT;
        break;
    default:
        break;
    }
#endif

    /*
     * If SamsungCamera flag is set to TRUE,
     * the PRO_MODE is operated using pure bayer reprocessing,
     * and the other MODE is operated using processed bayer reprocessing.
     *
     * If SamsungCamera flag is set to FALSE (GED or 3rd party),
     * the reprocessing bayer mode is defined in CONFIG files.
     *
     * If the DEBUG_RAWDUMP define is enabled,
     * the reprocessing bayer mode is set pure bayer
     *
     */

#ifndef DEBUG_RAWDUMP
    if (getSamsungCamera() == true) {
        if (m_flagCheckProMode == true || m_scenario == SCENARIO_SECURE) {
            m_usePureBayerReprocessing = true;
        } else {
            m_usePureBayerReprocessing = false;
        }
    } else {
        if (getRecordingHint() == true) {
#ifdef USE_DUAL_CAMERA
            if (getDualMode() == true) {
                m_usePureBayerReprocessing = (cameraId == CAMERA_ID_BACK) ? USE_PURE_BAYER_REPROCESSING_ON_DUAL_RECORDING : USE_PURE_BAYER_REPROCESSING_FRONT_ON_DUAL_RECORDING;
            } else
#endif
            if (getPIPMode() == true) {
                m_usePureBayerReprocessing = (cameraId == CAMERA_ID_BACK) ? USE_PURE_BAYER_REPROCESSING_ON_PIP_RECORDING : USE_PURE_BAYER_REPROCESSING_FRONT_ON_PIP_RECORDING;
            } else {
                m_usePureBayerReprocessing = (cameraId == CAMERA_ID_BACK) ? USE_PURE_BAYER_REPROCESSING_ON_RECORDING : USE_PURE_BAYER_REPROCESSING_FRONT_ON_RECORDING;
            }
        } else {
#ifdef USE_DUAL_CAMERA
            if (getDualMode() == true) {
                m_usePureBayerReprocessing = (cameraId == CAMERA_ID_BACK) ? USE_PURE_BAYER_REPROCESSING_ON_DUAL : USE_PURE_BAYER_REPROCESSING_FRONT_ON_DUAL;
            } else
#endif
            if (getPIPMode() == true) {
                m_usePureBayerReprocessing = (cameraId == CAMERA_ID_BACK) ? USE_PURE_BAYER_REPROCESSING_ON_PIP : USE_PURE_BAYER_REPROCESSING_FRONT_ON_PIP;
            } else {
                m_usePureBayerReprocessing = (cameraId == CAMERA_ID_BACK) ? USE_PURE_BAYER_REPROCESSING : USE_PURE_BAYER_REPROCESSING_FRONT;
            }
        }
    }
#else
    m_usePureBayerReprocessing = true;
#endif

    if (oldMode != m_usePureBayerReprocessing) {
        CLOGD("bayer usage is changed (%d -> %d)", oldMode, m_usePureBayerReprocessing);
    }

    return m_usePureBayerReprocessing;
}

int32_t ExynosCameraParameters::getReprocessingBayerMode(void)
{
    int32_t mode = REPROCESSING_BAYER_MODE_NONE;
    bool useDynamicBayer = getUseDynamicBayer();

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

void ExynosCameraParameters::setDvfsLock(bool lock) {
    m_dvfsLock = lock;
}

bool ExynosCameraParameters::getDvfsLock(void) {
    return m_dvfsLock;
}

#ifdef DEBUG_RAWDUMP
bool ExynosCameraParameters::checkBayerDumpEnable(void)
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

bool ExynosCameraParameters::setConfig(struct ExynosConfigInfo* config)
{
    memcpy(m_exynosconfig, config, sizeof(struct ExynosConfigInfo));
    setConfigMode(m_exynosconfig->mode);
    return true;
}
struct ExynosConfigInfo* ExynosCameraParameters::getConfig()
{
    return m_exynosconfig;
}

bool ExynosCameraParameters::setConfigMode(uint32_t mode)
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

int ExynosCameraParameters::getConfigMode()
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

status_t ExynosCameraParameters::setMarkingOfExifFlash(int flag)
{
    m_firing_flash_marking = flag;

    return NO_ERROR;
}

int ExynosCameraParameters::getMarkingOfExifFlash(void)
{
    return m_firing_flash_marking;
}

bool ExynosCameraParameters::getSensorOTFSupported(void)
{
    return m_staticInfo->flite3aaOtfSupport;
}

bool ExynosCameraParameters::isReprocessing(void)
{
    bool reprocessing = false;
    int cameraId = getCameraId();
    bool flagPIP = getPIPMode();

    if (m_scenario == SCENARIO_SECURE) {
        return false;
    }

    if (cameraId == CAMERA_ID_BACK) {
#if defined(MAIN_CAMERA_DUAL_REPROCESSING) && defined(MAIN_CAMERA_SINGLE_REPROCESSING)
        reprocessing = (flagPIP == true) ? MAIN_CAMERA_DUAL_REPROCESSING : MAIN_CAMERA_SINGLE_REPROCESSING;
#else
        CLOGW(" MAIN_CAMERA_DUAL_REPROCESSING/MAIN_CAMERA_SINGLE_REPROCESSING is not defined");
#endif
    } else {
#if defined(FRONT_CAMERA_DUAL_REPROCESSING) && defined(FRONT_CAMERA_SINGLE_REPROCESSING)
        reprocessing = (flagPIP == true) ? FRONT_CAMERA_DUAL_REPROCESSING : FRONT_CAMERA_SINGLE_REPROCESSING;
#else
        CLOGW(" FRONT_CAMERA_DUAL_REPROCESSING/FRONT_CAMERA_SINGLE_REPROCESSING is not defined");
#endif
    }

    return reprocessing;
}

bool ExynosCameraParameters::isSccCapture(void)
{
    bool sccCapture = false;
    int cameraId = getCameraId();
    bool flagPIP = getPIPMode();

    if (cameraId == CAMERA_ID_BACK) {
#if defined(MAIN_CAMERA_DUAL_SCC_CAPTURE) && defined(MAIN_CAMERA_SINGLE_SCC_CAPTURE)
        sccCapture = (flagPIP == true) ? MAIN_CAMERA_DUAL_SCC_CAPTURE : MAIN_CAMERA_SINGLE_SCC_CAPTURE;
#else
        CLOGW(" MAIN_CAMERA_DUAL_SCC_CAPTURE/MAIN_CAMERA_SINGLE_SCC_CAPTUREis not defined");
#endif
    } else {
#if defined(FRONT_CAMERA_DUAL_SCC_CAPTURE) && defined(FRONT_CAMERA_SINGLE_SCC_CAPTURE)
        sccCapture = (flagPIP == true) ? FRONT_CAMERA_DUAL_SCC_CAPTURE : FRONT_CAMERA_SINGLE_SCC_CAPTURE;
#else
        CLOGW(" FRONT_CAMERA_DUAL_SCC_CAPTURE/FRONT_CAMERA_SINGLE_SCC_CAPTURE is not defined");
#endif
    }

    return sccCapture;
}


/* True if private reprocessing or YUV reprocessing is supported */
bool ExynosCameraParameters::isSupportZSLInput(void) {
    if (m_staticInfo->supportedCapabilities & CAPABILITIES_PRIVATE_REPROCESSING
        || m_staticInfo->supportedCapabilities & CAPABILITIES_YUV_REPROCESSING) {
        return true;
    }

    return false;
}

enum HW_CONNECTION_MODE ExynosCameraParameters::getHwConnectionMode(
        enum pipeline srcPipeId,
        enum pipeline dstPipeId)
{
    enum HW_CONNECTION_MODE hwConnectionMode = HW_CONNECTION_MODE_NONE;

        switch (srcPipeId) {
        case PIPE_FLITE:
            if (dstPipeId != PIPE_3AA) {
                goto INVALID_PIPE_ID;
            }

            hwConnectionMode = m_getFlite3aaOtf();
            break;
        case PIPE_3AA:
            if (dstPipeId != PIPE_ISP) {
                goto INVALID_PIPE_ID;
            }

            hwConnectionMode = m_get3aaIspOtf();
            break;
        case PIPE_ISP:
            if (dstPipeId != PIPE_DCP
                && dstPipeId != PIPE_MCSC) {
                goto INVALID_PIPE_ID;
            }

#ifdef USE_DUAL_CAMERA
            if (dstPipeId == PIPE_DCP) {
                hwConnectionMode = m_getIspDcpOtf();
            } else
#endif
            if (dstPipeId ==  PIPE_MCSC) {
                hwConnectionMode = m_getIspMcscOtf();
            }
            break;
        case PIPE_MCSC:
            if (dstPipeId != PIPE_VRA) {
                goto INVALID_PIPE_ID;
            }

            hwConnectionMode = m_getMcscVraOtf();
            break;
        case PIPE_3AA_REPROCESSING:
            if (dstPipeId != PIPE_ISP_REPROCESSING) {
                goto INVALID_PIPE_ID;
            }

            hwConnectionMode = m_getReprocessing3aaIspOtf();
            break;
        case PIPE_ISP_REPROCESSING:
            if (dstPipeId != PIPE_DCP_REPROCESSING
                && dstPipeId != PIPE_MCSC_REPROCESSING) {
                goto INVALID_PIPE_ID;
            }

#ifdef USE_DUAL_CAMERA
            if (dstPipeId == PIPE_DCP_REPROCESSING) {
                hwConnectionMode = m_getReprocessingIspDcpOtf();
            } else
#endif
            if (dstPipeId ==  PIPE_MCSC_REPROCESSING) {
                hwConnectionMode = m_getReprocessingIspMcscOtf();
            }
            break;
        case PIPE_MCSC_REPROCESSING:
            if (dstPipeId != PIPE_VRA_REPROCESSING) {
                goto INVALID_PIPE_ID;
            }

            hwConnectionMode = m_getReprocessingMcscVraOtf();
            break;
#ifdef USE_DUAL_CAMERA
        case PIPE_DCP:
            if (dstPipeId != PIPE_MCSC) {
                goto INVALID_PIPE_ID;
            }

            hwConnectionMode = m_getDcpMcscOtf();
            break;
        case PIPE_DCP_REPROCESSING:
            if (dstPipeId != PIPE_MCSC_REPROCESSING) {
                goto INVALID_PIPE_ID;
            }

            hwConnectionMode = m_getReprocessingDcpMcscOtf();
            break;
#endif
        default:
            goto INVALID_PIPE_ID;
            break;
        }

    return hwConnectionMode;

INVALID_PIPE_ID:
    CLOGE("Invalid pipe ID src(%d), dst(%d)", srcPipeId, dstPipeId);
    hwConnectionMode = HW_CONNECTION_MODE_NONE;
    return hwConnectionMode;
}

enum HW_CONNECTION_MODE ExynosCameraParameters::m_getFlite3aaOtf(void)
{
    enum HW_CONNECTION_MODE hwConnectionMode = HW_CONNECTION_MODE_NONE;

    bool flagPIP = getPIPMode();
#ifdef USE_DUAL_CAMERA
    bool flagDual = getDualMode();
    enum DUAL_PREVIEW_MODE dualPreviewMode = getDualPreviewMode();
#endif

    if (m_scenario == SCENARIO_SECURE) {
        hwConnectionMode = HW_CONNECTION_MODE_M2M;
        return hwConnectionMode;
    }

    switch (getCameraId()) {
    case CAMERA_ID_BACK:
#ifdef USE_DUAL_CAMERA
        if (flagDual == true
            && dualPreviewMode != DUAL_PREVIEW_MODE_OFF) {
#ifdef MAIN_CAMERA_DUAL_FLITE_3AA_OTF
            hwConnectionMode = MAIN_CAMERA_DUAL_FLITE_3AA_OTF;
#else
            CLOGW("MAIN_CAMERA_DUAL_FLITE_3AA_OTF is not defined");
#endif
        } else
#endif /* USE_DUAL_CAMERA */
        if (flagPIP == true) {
#ifdef MAIN_CAMERA_PIP_FLITE_3AA_OTF
            hwConnectionMode = MAIN_CAMERA_PIP_FLITE_3AA_OTF;
#else
            CLOGW("MAIN_CAMERA_PIP_FLITE_3AA_OTF is not defined");
#endif
        } else {
#ifdef MAIN_CAMERA_SINGLE_FLITE_3AA_OTF
            hwConnectionMode = MAIN_CAMERA_SINGLE_FLITE_3AA_OTF;
#else
            CLOGW("MAIN_CAMERA_SINGLE_FLITE_3AA_OTF is not defined");
#endif
        }
        break;
    case CAMERA_ID_FRONT:
#ifdef USE_DUAL_CAMERA
        if (flagDual == true
            && dualPreviewMode != DUAL_PREVIEW_MODE_OFF) {
#ifdef FRONT_CAMERA_DUAL_FLITE_3AA_OTF
            hwConnectionMode = FRONT_CAMERA_DUAL_FLITE_3AA_OTF;
#else
            CLOGW("FRONT_CAMERA_DUAL_FLITE_3AA_OTF is not defined");
#endif
        } else
#endif /* USE_DUAL_CAMERA */
        if (flagPIP == true) {
#ifdef FRONT_CAMERA_PIP_FLITE_3AA_OTF
            hwConnectionMode = FRONT_CAMERA_PIP_FLITE_3AA_OTF;
#else
            CLOGW("FRONT_CAMERA_PIP_FLITE_3AA_OTF is not defined");
#endif
        } else {
#ifdef FRONT_CAMERA_SINGLE_FLITE_3AA_OTF
            hwConnectionMode = FRONT_CAMERA_SINGLE_FLITE_3AA_OTF;
#else
            CLOGW("FRONT_CAMERA_SINGLE_FLITE_3AA_OTF is not defined");
#endif
        }
        break;
#ifdef USE_DUAL_CAMERA
    case CAMERA_ID_BACK_1:
        if (flagDual == true
            && dualPreviewMode != DUAL_PREVIEW_MODE_OFF) {
#ifdef SUB_CAMERA_DUAL_FLITE_3AA_OTF
            hwConnectionMode = SUB_CAMERA_DUAL_FLITE_3AA_OTF;
#else
            CLOGW("SUB_CAMERA_DUAL_FLITE_3AA_OTF is not defined");
#endif
        } else if (flagPIP == true) {
#ifdef MAIN_CAMERA_PIP_FLITE_3AA_OTF
            hwConnectionMode = MAIN_CAMERA_PIP_FLITE_3AA_OTF;
#else
            CLOGW("MAIN_CAMERA_PIP_FLITE_3AA_OTF is not defined");
#endif
        } else {
#ifdef MAIN_CAMERA_SINGLE_FLITE_3AA_OTF
            hwConnectionMode = MAIN_CAMERA_SINGLE_FLITE_3AA_OTF;
#else
            CLOGW("MAIN_CAMERA_SINGLE_FLITE_3AA_OTF is not defined");
#endif
        }
        break;
    case CAMERA_ID_FRONT_1:
        if (flagDual == true
            && dualPreviewMode != DUAL_PREVIEW_MODE_OFF) {
#ifdef SUB_CAMERA_DUAL_FLITE_3AA_OTF
            hwConnectionMode = SUB_CAMERA_DUAL_FLITE_3AA_OTF;
#else
            CLOGW("SUB_CAMERA_DUAL_FLITE_3AA_OTF is not defined");
#endif
        } else if (flagPIP == true) {
#ifdef FRONT_CAMERA_PIP_FLITE_3AA_OTF
            hwConnectionMode = FRONT_CAMERA_PIP_FLITE_3AA_OTF;
#else
            CLOGW("FRONT_CAMERA_PIP_FLITE_3AA_OTF is not defined");
#endif
        } else {
#ifdef FRONT_CAMERA_SINGLE_FLITE_3AA_OTF
            hwConnectionMode = FRONT_CAMERA_SINGLE_FLITE_3AA_OTF;
#else
            CLOGW("FRONT_CAMERA_SINGLE_FLITE_3AA_OTF is not defined");
#endif
        }
        break;
#endif /* USE_DUAL_CAMERA */
    default:
        CLOGE("Invalid camera ID(%d)", getCameraId());
        hwConnectionMode = HW_CONNECTION_MODE_NONE;
        break;
    }

    return hwConnectionMode;
}

enum HW_CONNECTION_MODE ExynosCameraParameters::m_get3aaIspOtf(void)
{
    enum HW_CONNECTION_MODE hwConnectionMode = HW_CONNECTION_MODE_NONE;

    bool flagPIP = getPIPMode();
#ifdef USE_DUAL_CAMERA
    bool flagDual = getDualMode();
    enum DUAL_PREVIEW_MODE dualPreviewMode = getDualPreviewMode();
#endif

    switch (getCameraId()) {
    case CAMERA_ID_BACK:
#ifdef USE_DUAL_CAMERA
        if (flagDual == true
            && dualPreviewMode != DUAL_PREVIEW_MODE_OFF) {
#ifdef MAIN_CAMERA_DUAL_3AA_ISP_OTF
            hwConnectionMode = MAIN_CAMERA_DUAL_3AA_ISP_OTF;
#else
            CLOGW("MAIN_CAMERA_DUAL_3AA_ISP_OTF is not defined");
#endif
        } else
#endif /* USE_DUAL_CAMERA */
        if (flagPIP == true) {
#ifdef MAIN_CAMERA_PIP_3AA_ISP_OTF
            hwConnectionMode = MAIN_CAMERA_PIP_3AA_ISP_OTF;
#else
            CLOGW("MAIN_CAMERA_PIP_3AA_ISP_OTF is not defined");
#endif
        } else {
#ifdef MAIN_CAMERA_SINGLE_3AA_ISP_OTF
            hwConnectionMode = MAIN_CAMERA_SINGLE_3AA_ISP_OTF;
#else
            CLOGW("MAIN_CAMERA_SINGLE_3AA_ISP_OTF is not defined");
#endif
        }
        break;
    case CAMERA_ID_FRONT:
#ifdef USE_DUAL_CAMERA
        if (flagDual == true
            && dualPreviewMode != DUAL_PREVIEW_MODE_OFF) {
#ifdef FRONT_CAMERA_DUAL_3AA_ISP_OTF
            hwConnectionMode = FRONT_CAMERA_DUAL_3AA_ISP_OTF;
#else
            CLOGW("FRONT_CAMERA_DUAL_3AA_ISP_OTF is not defined");
#endif
        } else
#endif /* USE_DUAL_CAMERA */
        if (flagPIP == true) {
#ifdef FRONT_CAMERA_PIP_3AA_ISP_OTF
            hwConnectionMode = FRONT_CAMERA_PIP_3AA_ISP_OTF;
#else
            CLOGW("FRONT_CAMERA_PIP_3AA_ISP_OTF is not defined");
#endif
        } else {
#ifdef FRONT_CAMERA_SINGLE_3AA_ISP_OTF
            hwConnectionMode = FRONT_CAMERA_SINGLE_3AA_ISP_OTF;
#else
            CLOGW(" FRONT_CAMERA_SINGLE_3AA_ISP_OTF is not defined");
#endif
        }
        break;
#ifdef USE_DUAL_CAMERA
    case CAMERA_ID_BACK_1:
        if (flagDual == true
            && dualPreviewMode != DUAL_PREVIEW_MODE_OFF) {
#ifdef SUB_CAMERA_DUAL_3AA_ISP_OTF
            hwConnectionMode = SUB_CAMERA_DUAL_3AA_ISP_OTF;
#else
            CLOGW("SUB_CAMERA_DUAL_3AA_ISP_OTF is not defined");
#endif
        } else if (flagPIP == true) {
#ifdef MAIN_CAMERA_PIP_3AA_ISP_OTF
            hwConnectionMode = MAIN_CAMERA_PIP_3AA_ISP_OTF;
#else
            CLOGW("MAIN_CAMERA_PIP_3AA_ISP_OTF is not defined");
#endif
        } else {
#ifdef MAIN_CAMERA_SINGLE_3AA_ISP_OTF
            hwConnectionMode = MAIN_CAMERA_SINGLE_3AA_ISP_OTF;
#else
            CLOGW("MAIN_CAMERA_SINGLE_3AA_ISP_OTF is not defined");
#endif
        }
        break;
    case CAMERA_ID_FRONT_1:
        if (flagDual == true
            && dualPreviewMode != DUAL_PREVIEW_MODE_OFF) {
#ifdef SUB_CAMERA_DUAL_3AA_ISP_OTF
            hwConnectionMode = SUB_CAMERA_DUAL_3AA_ISP_OTF;
#else
            CLOGW("SUB_CAMERA_DUAL_3AA_ISP_OTF is not defined");
#endif
        } else if (flagPIP == true) {
#ifdef FRONT_CAMERA_PIP_3AA_ISP_OTF
            hwConnectionMode = FRONT_CAMERA_PIP_3AA_ISP_OTF;
#else
            CLOGW("FRONT_CAMERA_PIP_3AA_ISP_OTF is not defined");
#endif
        } else {
#ifdef FRONT_CAMERA_SINGLE_3AA_ISP_OTF
            hwConnectionMode = FRONT_CAMERA_SINGLE_3AA_ISP_OTF;
#else
            CLOGW(" FRONT_CAMERA_SINGLE_3AA_ISP_OTF is not defined");
#endif
        }
        break;
#endif /* USE_DUAL_CAMERA */
    default:
        CLOGE("Invalid camera ID(%d)", getCameraId());
        hwConnectionMode = HW_CONNECTION_MODE_NONE;
        break;
    }

    return hwConnectionMode;
}

enum HW_CONNECTION_MODE ExynosCameraParameters::m_getIspMcscOtf(void)
{
    enum HW_CONNECTION_MODE hwConnectionMode = HW_CONNECTION_MODE_NONE;

    bool flagPIP = getPIPMode();
#ifdef USE_DUAL_CAMERA
    bool flagDual = getDualMode();
    enum DUAL_PREVIEW_MODE dualPreviewMode = getDualPreviewMode();
#endif

    switch (getCameraId()) {
    case CAMERA_ID_BACK:
#ifdef USE_DUAL_CAMERA
        if (flagDual == true
            && dualPreviewMode != DUAL_PREVIEW_MODE_OFF) {
#ifdef MAIN_CAMERA_DUAL_ISP_MCSC_OTF
            hwConnectionMode = MAIN_CAMERA_DUAL_ISP_MCSC_OTF;
#else
            CLOGW("MAIN_CAMERA_DUAL_ISP_MCSC_OTF is not defined");
#endif
        } else
#endif /* USE_DUAL_CAMERA */
        if (flagPIP == true) {
#ifdef MAIN_CAMERA_PIP_ISP_MCSC_OTF
            hwConnectionMode = MAIN_CAMERA_PIP_ISP_MCSC_OTF;
#else
            CLOGW("MAIN_CAMERA_PIP_ISP_MCSC_OTF is not defined");
#endif
        } else {
#ifdef MAIN_CAMERA_SINGLE_ISP_MCSC_OTF
            hwConnectionMode = MAIN_CAMERA_SINGLE_ISP_MCSC_OTF;
#else
            CLOGW("MAIN_CAMERA_SINGLE_ISP_MCSC_OTF is not defined");
#endif
        }
        break;
    case CAMERA_ID_FRONT:
#ifdef USE_DUAL_CAMERA
        if (flagDual == true
            && dualPreviewMode != DUAL_PREVIEW_MODE_OFF) {
#ifdef FRONT_CAMERA_DUAL_ISP_MCSC_OTF
            hwConnectionMode = FRONT_CAMERA_DUAL_ISP_MCSC_OTF;
#else
            CLOGW("FRONT_CAMERA_DUAL_ISP_MCSC_OTF is not defined");
#endif
        } else
#endif /* USE_DUAL_CAMERA */
        if (flagPIP == true) {
#ifdef FRONT_CAMERA_PIP_ISP_MCSC_OTF
            hwConnectionMode = FRONT_CAMERA_PIP_ISP_MCSC_OTF;
#else
            CLOGW("FRONT_CAMERA_PIP_ISP_MCSC_OTF is not defined");
#endif
        } else {
#ifdef FRONT_CAMERA_SINGLE_ISP_MCSC_OTF
            hwConnectionMode = FRONT_CAMERA_SINGLE_ISP_MCSC_OTF;
#else
            CLOGW("FRONT_CAMERA_SINGLE_ISP_MCSC_OTF is not defined");
#endif
        }
        break;
#ifdef USE_DUAL_CAMERA
    case CAMERA_ID_BACK_1:
        if (flagDual == true
            && dualPreviewMode != DUAL_PREVIEW_MODE_OFF) {
#ifdef SUB_CAMERA_DUAL_ISP_MCSC_OTF
            hwConnectionMode = SUB_CAMERA_DUAL_ISP_MCSC_OTF;
#else
            CLOGW("SUB_CAMERA_DUAL_ISP_MCSC_OTF is not defined");
#endif
        } else if (flagPIP == true) {
#ifdef MAIN_CAMERA_PIP_ISP_MCSC_OTF
            hwConnectionMode = MAIN_CAMERA_PIP_ISP_MCSC_OTF;
#else
            CLOGW("MAIN_CAMERA_PIP_ISP_MCSC_OTF is not defined");
#endif
        } else {
#ifdef MAIN_CAMERA_SINGLE_ISP_MCSC_OTF
            hwConnectionMode = MAIN_CAMERA_SINGLE_ISP_MCSC_OTF;
#else
            CLOGW("MAIN_CAMERA_SINGLE_ISP_MCSC_OTF is not defined");
#endif
        }
        break;
    case CAMERA_ID_FRONT_1:
        if (flagDual == true
            && dualPreviewMode != DUAL_PREVIEW_MODE_OFF) {
#ifdef SUB_CAMERA_DUAL_ISP_MCSC_OTF
            hwConnectionMode = SUB_CAMERA_DUAL_ISP_MCSC_OTF;
#else
            CLOGW("SUB_CAMERA_DUAL_ISP_MCSC_OTF is not defined");
#endif
        } else if (flagPIP == true) {
#ifdef FRONT_CAMERA_PIP_ISP_MCSC_OTF
            hwConnectionMode = FRONT_CAMERA_PIP_ISP_MCSC_OTF;
#else
            CLOGW("FRONT_CAMERA_PIP_ISP_MCSC_OTF is not defined");
#endif
        } else {
#ifdef FRONT_CAMERA_SINGLE_ISP_MCSC_OTF
            hwConnectionMode = FRONT_CAMERA_SINGLE_ISP_MCSC_OTF;
#else
            CLOGW("FRONT_CAMERA_SINGLE_ISP_MCSC_OTF is not defined");
#endif
        }
        break;
#endif /* USE_DUAL_CAMERA */
    default:
        CLOGE("Invalid camera ID(%d)", getCameraId());
        hwConnectionMode = HW_CONNECTION_MODE_NONE;
        break;
    }

    return hwConnectionMode;
}

#ifdef USE_DUAL_CAMERA
enum HW_CONNECTION_MODE ExynosCameraParameters::m_getIspDcpOtf(void)
{
    enum HW_CONNECTION_MODE hwConnectionMode = HW_CONNECTION_MODE_NONE;

    bool flagPIP = getPIPMode();
    bool flagDual = getDualMode();
    enum DUAL_PREVIEW_MODE dualPreviewMode = getDualPreviewMode();

    switch (getCameraId()) {
    case CAMERA_ID_BACK:
        if (flagDual == true
            && dualPreviewMode != DUAL_PREVIEW_MODE_OFF) {
#ifdef MAIN_CAMERA_DUAL_ISP_DCP_OTF
            hwConnectionMode = MAIN_CAMERA_DUAL_ISP_DCP_OTF;
#else
            CLOGW("MAIN_CAMERA_DUAL_ISP_DCP_OTF is not defined");
#endif
        } else if (flagPIP == true) {
#ifdef MAIN_CAMERA_PIP_ISP_DCP_OTF
            hwConnectionMode = MAIN_CAMERA_PIP_ISP_DCP_OTF;
#else
            CLOGW("MAIN_CAMERA_PIP_ISP_DCP_OTF is not defined");
#endif
        } else {
#ifdef MAIN_CAMERA_SINGLE_ISP_DCP_OTF
            hwConnectionMode = MAIN_CAMERA_SINGLE_ISP_DCP_OTF;
#else
            CLOGW("MAIN_CAMERA_SINGLE_ISP_DCP_OTF is not defined");
#endif
        }
        break;
    case CAMERA_ID_FRONT:
        if (flagDual == true
            && dualPreviewMode != DUAL_PREVIEW_MODE_OFF) {
#ifdef FRONT_CAMERA_DUAL_ISP_DCP_OTF
            hwConnectionMode = FRONT_CAMERA_DUAL_ISP_DCP_OTF;
#else
            CLOGW("FRONT_CAMERA_DUAL_ISP_DCP_OTF is not defined");
#endif
        } else if (flagPIP == true) {
#ifdef FRONT_CAMERA_PIP_ISP_DCP_OTF
            hwConnectionMode = FRONT_CAMERA_PIP_ISP_DCP_OTF;
#else
            CLOGW("FRONT_CAMERA_PIP_ISP_DCP_OTF is not defined");
#endif
        } else {
#ifdef FRONT_CAMERA_SINGLE_ISP_DCP_OTF
            hwConnectionMode = FRONT_CAMERA_SINGLE_ISP_DCP_OTF;
#else
            CLOGW("FRONT_CAMERA_SINGLE_ISP_DCP_OTF is not defined");
#endif
        }
        break;
    case CAMERA_ID_BACK_1:
        if (flagDual == true
            && dualPreviewMode != DUAL_PREVIEW_MODE_OFF) {
#ifdef SUB_CAMERA_DUAL_ISP_DCP_OTF
            hwConnectionMode = SUB_CAMERA_DUAL_ISP_DCP_OTF;
#else
            CLOGW("SUB_CAMERA_DUAL_ISP_DCP_OTF is not defined");
#endif
        } else if (flagPIP == true) {
#ifdef MAIN_CAMERA_PIP_ISP_DCP_OTF
            hwConnectionMode = MAIN_CAMERA_PIP_ISP_DCP_OTF;
#else
            CLOGW("MAIN_CAMERA_PIP_ISP_DCP_OTF is not defined");
#endif
        } else {
#ifdef MAIN_CAMERA_SINGLE_ISP_DCP_OTF
            hwConnectionMode = MAIN_CAMERA_SINGLE_ISP_DCP_OTF;
#else
            CLOGW("MAIN_CAMERA_SINGLE_ISP_DCP_OTF is not defined");
#endif
        }
        break;
    case CAMERA_ID_FRONT_1:
        if (flagDual == true
            && dualPreviewMode != DUAL_PREVIEW_MODE_OFF) {
#ifdef SUB_CAMERA_DUAL_ISP_DCP_OTF
            hwConnectionMode = SUB_CAMERA_DUAL_ISP_DCP_OTF;
#else
            CLOGW("SUB_CAMERA_DUAL_ISP_DCP_OTF is not defined");
#endif
        } else if (flagPIP == true) {
#ifdef FRONT_CAMERA_PIP_ISP_DCP_OTF
            hwConnectionMode = FRONT_CAMERA_PIP_ISP_DCP_OTF;
#else
            CLOGW("FRONT_CAMERA_PIP_ISP_DCP_OTF is not defined");
#endif
        } else {
#ifdef FRONT_CAMERA_SINGLE_ISP_DCP_OTF
            hwConnectionMode = FRONT_CAMERA_SINGLE_ISP_DCP_OTF;
#else
            CLOGW("FRONT_CAMERA_SINGLE_ISP_DCP_OTF is not defined");
#endif
        }
        break;
    default:
        CLOGE("Invalid camera ID(%d)", getCameraId());
        hwConnectionMode = HW_CONNECTION_MODE_NONE;
        break;
    }

    return hwConnectionMode;
}

enum HW_CONNECTION_MODE ExynosCameraParameters::m_getDcpMcscOtf(void)
{
    enum HW_CONNECTION_MODE hwConnectionMode = HW_CONNECTION_MODE_NONE;

    bool flagPIP = getPIPMode();
    bool flagDual = getDualMode();
    enum DUAL_PREVIEW_MODE dualPreviewMode = getDualPreviewMode();

    switch (getCameraId()) {
    case CAMERA_ID_BACK:
        if (flagDual == true
            && dualPreviewMode != DUAL_PREVIEW_MODE_OFF) {
#ifdef MAIN_CAMERA_DUAL_DCP_MCSC_OTF
            hwConnectionMode = MAIN_CAMERA_DUAL_DCP_MCSC_OTF;
#else
            CLOGW("MAIN_CAMERA_DUAL_DCP_MCSC_OTF is not defined");
#endif
        } else
        if (flagPIP == true) {
#ifdef MAIN_CAMERA_PIP_DCP_MCSC_OTF
            hwConnectionMode = MAIN_CAMERA_PIP_DCP_MCSC_OTF;
#else
            CLOGW("MAIN_CAMERA_PIP_DCP_MCSC_OTF is not defined");
#endif
        } else {
#ifdef MAIN_CAMERA_SINGLE_DCP_MCSC_OTF
            hwConnectionMode = MAIN_CAMERA_SINGLE_DCP_MCSC_OTF;
#else
            CLOGW("MAIN_CAMERA_SINGLE_DCP_MCSC_OTF is not defined");
#endif
        }
        break;
    case CAMERA_ID_FRONT:
        if (flagDual == true
            && dualPreviewMode != DUAL_PREVIEW_MODE_OFF) {
#ifdef FRONT_CAMERA_DUAL_DCP_MCSC_OTF
            hwConnectionMode = FRONT_CAMERA_DUAL_DCP_MCSC_OTF;
#else
            CLOGW("FRONT_CAMERA_DUAL_DCP_MCSC_OTF is not defined");
#endif
        } else
        if (flagPIP == true) {
#ifdef FRONT_CAMERA_PIP_DCP_MCSC_OTF
            hwConnectionMode = FRONT_CAMERA_PIP_DCP_MCSC_OTF;
#else
            CLOGW("FRONT_CAMERA_PIP_DCP_MCSC_OTF is not defined");
#endif
        } else {
#ifdef FRONT_CAMERA_SINGLE_DCP_MCSC_OTF
            hwConnectionMode = FRONT_CAMERA_SINGLE_DCP_MCSC_OTF;
#else
            CLOGW("FRONT_CAMERA_SINGLE_DCP_MCSC_OTF is not defined");
#endif
        }
        break;
    case CAMERA_ID_BACK_1:
        if (flagDual == true
            && dualPreviewMode != DUAL_PREVIEW_MODE_OFF) {
#ifdef SUB_CAMERA_DUAL_DCP_MCSC_OTF
            hwConnectionMode = SUB_CAMERA_DUAL_DCP_MCSC_OTF;
#else
            CLOGW("SUB_CAMERA_DUAL_DCP_MCSC_OTF is not defined");
#endif
        } else if (flagPIP == true) {
#ifdef MAIN_CAMERA_PIP_DCP_MCSC_OTF
            hwConnectionMode = MAIN_CAMERA_PIP_DCP_MCSC_OTF;
#else
            CLOGW("MAIN_CAMERA_PIP_DCP_MCSC_OTF is not defined");
#endif
        } else {
#ifdef MAIN_CAMERA_SINGLE_DCP_MCSC_OTF
            hwConnectionMode = MAIN_CAMERA_SINGLE_DCP_MCSC_OTF;
#else
            CLOGW("MAIN_CAMERA_SINGLE_DCP_MCSC_OTF is not defined");
#endif
        }
        break;
    case CAMERA_ID_FRONT_1:
        if (flagDual == true
            && dualPreviewMode != DUAL_PREVIEW_MODE_OFF) {
#ifdef SUB_CAMERA_DUAL_DCP_MCSC_OTF
            hwConnectionMode = SUB_CAMERA_DUAL_DCP_MCSC_OTF;
#else
            CLOGW("SUB_CAMERA_DUAL_DCP_MCSC_OTF is not defined");
#endif
        } else if (flagPIP == true) {
#ifdef FRONT_CAMERA_PIP_DCP_MCSC_OTF
            hwConnectionMode = FRONT_CAMERA_PIP_DCP_MCSC_OTF;
#else
            CLOGW("FRONT_CAMERA_PIP_DCP_MCSC_OTF is not defined");
#endif
        } else {
#ifdef FRONT_CAMERA_SINGLE_DCP_MCSC_OTF
            hwConnectionMode = FRONT_CAMERA_SINGLE_DCP_MCSC_OTF;
#else
            CLOGW("FRONT_CAMERA_SINGLE_DCP_MCSC_OTF is not defined");
#endif
        }
        break;
    default:
        CLOGE("Invalid camera ID(%d)", getCameraId());
        hwConnectionMode = HW_CONNECTION_MODE_NONE;
        break;
    }

    return hwConnectionMode;
}
#endif /* USE_DUAL_CAMERA */

enum HW_CONNECTION_MODE ExynosCameraParameters::m_getMcscVraOtf(void)
{
    enum HW_CONNECTION_MODE hwConnectionMode = HW_CONNECTION_MODE_NONE;

    bool flagPIP = getPIPMode();
#ifdef USE_DUAL_CAMERA
    bool flagDual = getDualMode();
    enum DUAL_PREVIEW_MODE dualPreviewMode = getDualPreviewMode();
#endif

    switch (getCameraId()) {
    case CAMERA_ID_BACK:
#ifdef USE_DUAL_CAMERA
        if (flagDual == true
            && dualPreviewMode != DUAL_PREVIEW_MODE_OFF) {
#ifdef MAIN_CAMERA_DUAL_MCSC_VRA_OTF
            hwConnectionMode = MAIN_CAMERA_DUAL_MCSC_VRA_OTF;
#else
            CLOGW("MAIN_CAMERA_DUAL_MCSC_VRA_OTF is not defined");
#endif
        } else
#endif /* USE_DUAL_CAMERA */
        if (flagPIP == true) {
#ifdef MAIN_CAMERA_PIP_MCSC_VRA_OTF
            hwConnectionMode = MAIN_CAMERA_PIP_MCSC_VRA_OTF;
#else
            CLOGW("MAIN_CAMERA_PIP_MCSC_VRA_OTF is not defined");
#endif
        } else {
#ifdef MAIN_CAMERA_SINGLE_MCSC_VRA_OTF
            hwConnectionMode = MAIN_CAMERA_SINGLE_MCSC_VRA_OTF;
#else
            CLOGW("MAIN_CAMERA_SINGLE_TPU_MCSC_OTF is not defined");
#endif
        }
        break;
    case CAMERA_ID_FRONT:
#ifdef USE_DUAL_CAMERA
        if (flagDual == true
            && dualPreviewMode != DUAL_PREVIEW_MODE_OFF) {
#ifdef FRONT_CAMERA_DUAL_MCSC_VRA_OTF
            hwConnectionMode = FRONT_CAMERA_DUAL_MCSC_VRA_OTF;
#else
            CLOGW("FRONT_CAMERA_DUAL_MCSC_VRA_OTF is not defined");
#endif
        } else
#endif /* USE_DUAL_CAMERA */
        if (flagPIP == true) {
#ifdef FRONT_CAMERA_PIP_MCSC_VRA_OTF
            hwConnectionMode = FRONT_CAMERA_PIP_MCSC_VRA_OTF;
#else
            CLOGW("FRONT_CAMERA_PIP_MCSC_VRA_OTF is not defined");
#endif
        } else {
#ifdef FRONT_CAMERA_SINGLE_MCSC_VRA_OTF
            hwConnectionMode = FRONT_CAMERA_SINGLE_MCSC_VRA_OTF;
#else
            CLOGW(" FRONT_CAMERA_SINGLE_MCSC_PIP_OTF is not defined");
#endif
        }
        break;
#ifdef USE_DUAL_CAMERA
    case CAMERA_ID_BACK_1:
        if (flagDual == true
            && dualPreviewMode != DUAL_PREVIEW_MODE_OFF) {
#ifdef SUB_CAMERA_DUAL_MCSC_VRA_OTF
            hwConnectionMode = SUB_CAMERA_DUAL_MCSC_VRA_OTF;
#else
            CLOGW("SUB_CAMERA_DUAL_MCSC_VRA_OTF is not defined");
#endif
        } else if (flagPIP == true) {
#ifdef MAIN_CAMERA_PIP_MCSC_VRA_OTF
            hwConnectionMode = MAIN_CAMERA_PIP_MCSC_VRA_OTF;
#else
            CLOGW("MAIN_CAMERA_PIP_MCSC_VRA_OTF is not defined");
#endif
        } else {
#ifdef MAIN_CAMERA_SINGLE_MCSC_VRA_OTF
            hwConnectionMode = MAIN_CAMERA_SINGLE_MCSC_VRA_OTF;
#else
            CLOGW("MAIN_CAMERA_SINGLE_TPU_MCSC_OTF is not defined");
#endif
        }
        break;
    case CAMERA_ID_FRONT_1:
        if (flagDual == true
            && dualPreviewMode != DUAL_PREVIEW_MODE_OFF) {
#ifdef SUB_CAMERA_DUAL_MCSC_VRA_OTF
            hwConnectionMode = SUB_CAMERA_DUAL_MCSC_VRA_OTF;
#else
            CLOGW("SUB_CAMERA_DUAL_MCSC_VRA_OTF is not defined");
#endif
        } else if (flagPIP == true) {
#ifdef FRONT_CAMERA_PIP_MCSC_VRA_OTF
            hwConnectionMode = FRONT_CAMERA_PIP_MCSC_VRA_OTF;
#else
            CLOGW("FRONT_CAMERA_PIP_MCSC_VRA_OTF is not defined");
#endif
        } else {
#ifdef FRONT_CAMERA_SINGLE_MCSC_VRA_OTF
            hwConnectionMode = FRONT_CAMERA_SINGLE_MCSC_VRA_OTF;
#else
            CLOGW(" FRONT_CAMERA_SINGLE_MCSC_PIP_OTF is not defined");
#endif
        }
        break;
#endif /* USE_DUAL_CAMERA */
    default:
        CLOGE("Invalid camera ID(%d)", getCameraId());
        hwConnectionMode = HW_CONNECTION_MODE_NONE;
        break;
    }

    return hwConnectionMode;
}

enum HW_CONNECTION_MODE ExynosCameraParameters::m_getReprocessing3aaIspOtf(void)
{
    enum HW_CONNECTION_MODE hwConnectionMode = HW_CONNECTION_MODE_NONE;

    bool flagPIP = getPIPMode();
#ifdef USE_DUAL_CAMERA
    bool flagDual = getDualMode();
    enum DUAL_REPROCESSING_MODE dualReprocessingMode = getDualReprocessingMode();
#endif

    switch (getCameraId()) {
    case CAMERA_ID_BACK:
#ifdef USE_DUAL_CAMERA
        if (flagDual == true
            && dualReprocessingMode != DUAL_REPROCESSING_MODE_OFF) {
#ifdef MAIN_CAMERA_DUAL_3AA_ISP_OTF_REPROCESSING
            hwConnectionMode = MAIN_CAMERA_DUAL_3AA_ISP_OTF_REPROCESSING;
#else
            CLOGW("MAIN_CAMERA_DUAL_3AA_ISP_OTF_REPROCESSING is not defined");
#endif
        } else
#endif /* USE_DUAL_CAMERA */
        if (flagPIP == true) {
#ifdef MAIN_CAMERA_PIP_3AA_ISP_OTF_REPROCESSING
            hwConnectionMode = MAIN_CAMERA_PIP_3AA_ISP_OTF_REPROCESSING;
#else
            CLOGW("MAIN_CAMERA_PIP_3AA_ISP_OTF_REPROCESSING is not defined");
#endif
        } else {
#ifdef MAIN_CAMERA_SINGLE_3AA_ISP_OTF_REPROCESSING
            hwConnectionMode = MAIN_CAMERA_SINGLE_3AA_ISP_OTF_REPROCESSING;
#else
            CLOGW("MAIN_CAMERA_SINGLE_3AA_ISP_OTF_REPROCESSING is not defined");
#endif
        }
        break;
    case CAMERA_ID_FRONT:
#ifdef USE_DUAL_CAMERA
        if (flagDual == true
            && dualReprocessingMode != DUAL_REPROCESSING_MODE_OFF) {
#ifdef FRONT_CAMERA_DUAL_3AA_ISP_OTF_REPROCESSING
            hwConnectionMode = FRONT_CAMERA_DUAL_3AA_ISP_OTF_REPROCESSING;
#else
            CLOGW("FRONT_CAMERA_DUAL_3AA_ISP_OTF_REPROCESSING is not defined");
#endif
        } else
#endif /* USE_DUAL_CAMERA */
        if (flagPIP == true) {
#ifdef FRONT_CAMERA_PIP_3AA_ISP_OTF_REPROCESSING
            hwConnectionMode = FRONT_CAMERA_PIP_3AA_ISP_OTF_REPROCESSING;
#else
            CLOGW("FRONT_CAMERA_PIP_3AA_ISP_OTF_REPROCESSING is not defined");
#endif
        } else {
#ifdef FRONT_CAMERA_SINGLE_3AA_ISP_OTF_REPROCESSING
            hwConnectionMode = FRONT_CAMERA_SINGLE_3AA_ISP_OTF_REPROCESSING;
#else
            CLOGW("FRONT_CAMERA_SINGLE_3AA_ISP_OTF_REPROCESSING is not defined");
#endif
        }
        break;
#ifdef USE_DUAL_CAMERA
    case CAMERA_ID_BACK_1:
        if (flagDual == true
            && dualReprocessingMode != DUAL_REPROCESSING_MODE_OFF) {
#ifdef SUB_CAMERA_DUAL_3AA_ISP_OTF_REPROCESSING
            hwConnectionMode = SUB_CAMERA_DUAL_3AA_ISP_OTF_REPROCESSING;
#else
            CLOGW("SUB_CAMERA_DUAL_3AA_ISP_OTF_REPROCESSING is not defined");
#endif
        } else if (flagPIP == true) {
#ifdef MAIN_CAMERA_PIP_3AA_ISP_OTF_REPROCESSING
            hwConnectionMode = MAIN_CAMERA_PIP_3AA_ISP_OTF_REPROCESSING;
#else
            CLOGW("MAIN_CAMERA_PIP_3AA_ISP_OTF_REPROCESSING is not defined");
#endif
        } else {
#ifdef MAIN_CAMERA_SINGLE_3AA_ISP_OTF_REPROCESSING
            hwConnectionMode = MAIN_CAMERA_SINGLE_3AA_ISP_OTF_REPROCESSING;
#else
            CLOGW("MAIN_CAMERA_SINGLE_3AA_ISP_OTF_REPROCESSING is not defined");
#endif
        }
        break;
    case CAMERA_ID_FRONT_1:
        if (flagDual == true
            && dualReprocessingMode != DUAL_REPROCESSING_MODE_OFF) {
#ifdef SUB_CAMERA_DUAL_3AA_ISP_OTF_REPROCESSING
            hwConnectionMode = SUB_CAMERA_DUAL_3AA_ISP_OTF_REPROCESSING;
#else
            CLOGW("SUB_CAMERA_DUAL_3AA_ISP_OTF_REPROCESSING is not defined");
#endif
        } else if (flagPIP == true) {
#ifdef FRONT_CAMERA_PIP_3AA_ISP_OTF_REPROCESSING
            hwConnectionMode = FRONT_CAMERA_PIP_3AA_ISP_OTF_REPROCESSING;
#else
            CLOGW("FRONT_CAMERA_PIP_3AA_ISP_OTF_REPROCESSING is not defined");
#endif
        } else {
#ifdef FRONT_CAMERA_SINGLE_3AA_ISP_OTF_REPROCESSING
            hwConnectionMode = FRONT_CAMERA_SINGLE_3AA_ISP_OTF_REPROCESSING;
#else
            CLOGW("FRONT_CAMERA_SINGLE_3AA_ISP_OTF_REPROCESSING is not defined");
#endif
        }
        break;
#endif /* USE_DUAL_CAMERA */
    default:
        CLOGE("Invalid camera ID(%d)", getCameraId());
        hwConnectionMode = HW_CONNECTION_MODE_NONE;
        break;
    }

    if (hwConnectionMode != HW_CONNECTION_MODE_M2M
        && getUsePureBayerReprocessing() == false) {
        CLOGW("Processed bayer must using 3AA-ISP M2M. but, Current mode(%d)",
                (int)hwConnectionMode);

        hwConnectionMode = HW_CONNECTION_MODE_M2M;
    }

    return hwConnectionMode;
}

enum HW_CONNECTION_MODE ExynosCameraParameters::m_getReprocessingIspMcscOtf(void)
{
    enum HW_CONNECTION_MODE hwConnectionMode = HW_CONNECTION_MODE_NONE;

    bool flagPIP = getPIPMode();
#ifdef USE_DUAL_CAMERA
    bool flagDual = getDualMode();
    enum DUAL_REPROCESSING_MODE dualReprocessingMode = getDualReprocessingMode();
#endif

    switch (getCameraId()) {
    case CAMERA_ID_BACK:
#ifdef USE_DUAL_CAMERA
        if (flagDual == true
            && dualReprocessingMode != DUAL_REPROCESSING_MODE_OFF) {
#ifdef MAIN_CAMERA_DUAL_ISP_MCSC_OTF_REPROCESSING
            hwConnectionMode = MAIN_CAMERA_DUAL_ISP_MCSC_OTF_REPROCESSING;
#else
            CLOGW("MAIN_CAMERA_DUAL_ISP_MCSC_OTF_REPROCESSING is not defined");
#endif
        } else
#endif /* USE_DUAL_CAMERA */
        if (flagPIP == true) {
#ifdef MAIN_CAMERA_PIP_ISP_MCSC_OTF_REPROCESSING
            hwConnectionMode = MAIN_CAMERA_PIP_ISP_MCSC_OTF_REPROCESSING;
#else
            CLOGW("MAIN_CAMERA_PIP_ISP_MCSC_OTF_REPROCESSING is not defined");
#endif
        } else {
#ifdef MAIN_CAMERA_SINGLE_ISP_MCSC_OTF_REPROCESSING
            hwConnectionMode = MAIN_CAMERA_SINGLE_ISP_MCSC_OTF_REPROCESSING;
#else
            CLOGW("MAIN_CAMERA_SINGLE_ISP_MCSC_OTF_REPROCESSING is not defined");
#endif
        }
        break;
    case CAMERA_ID_FRONT:
#ifdef USE_DUAL_CAMERA
        if (flagDual == true
            && dualReprocessingMode != DUAL_REPROCESSING_MODE_OFF) {
#ifdef FRONT_CAMERA_DUAL_ISP_MCSC_OTF_REPROCESSING
            hwConnectionMode = FRONT_CAMERA_DUAL_ISP_MCSC_OTF_REPROCESSING;
#else
            CLOGW("FRONT_CAMERA_DUAL_ISP_MCSC_OTF_REPROCESSING is not defined");
#endif
        } else
#endif /* USE_DUAL_CAMERA */
        if (flagPIP == true) {
#ifdef FRONT_CAMERA_PIP_ISP_MCSC_OTF_REPROCESSING
            hwConnectionMode = FRONT_CAMERA_PIP_ISP_MCSC_OTF_REPROCESSING;
#else
            CLOGW("FRONT_CAMERA_PIP_ISP_MCSC_OTF_REPROCESSING is not defined");
#endif
        } else {
#ifdef FRONT_CAMERA_SINGLE_ISP_MCSC_OTF_REPROCESSING
            hwConnectionMode = FRONT_CAMERA_SINGLE_ISP_MCSC_OTF_REPROCESSING;
#else
            CLOGW("FRONT_CAMERA_SINGLE_ISP_MCSC_OTF_REPROCESSING is not defined");
#endif
        }
        break;
#ifdef USE_DUAL_CAMERA
    case CAMERA_ID_BACK_1:
        if (flagDual == true
            && dualReprocessingMode != DUAL_REPROCESSING_MODE_OFF) {
#ifdef SUB_CAMERA_DUAL_ISP_MCSC_OTF_REPROCESSING
            hwConnectionMode = SUB_CAMERA_DUAL_ISP_MCSC_OTF_REPROCESSING;
#else
            CLOGW("SUB_CAMERA_DUAL_ISP_MCSC_OTF_REPROCESSING is not defined");
#endif
        } else if (flagPIP == true) {
#ifdef MAIN_CAMERA_PIP_ISP_MCSC_OTF_REPROCESSING
            hwConnectionMode = MAIN_CAMERA_PIP_ISP_MCSC_OTF_REPROCESSING;
#else
            CLOGW("MAIN_CAMERA_PIP_ISP_MCSC_OTF_REPROCESSING is not defined");
#endif
        } else {
#ifdef MAIN_CAMERA_SINGLE_ISP_MCSC_OTF_REPROCESSING
            hwConnectionMode = MAIN_CAMERA_SINGLE_ISP_MCSC_OTF_REPROCESSING;
#else
            CLOGW("MAIN_CAMERA_SINGLE_ISP_MCSC_OTF_REPROCESSING is not defined");
#endif
        }
        break;
    case CAMERA_ID_FRONT_1:
        if (flagDual == true
            && dualReprocessingMode != DUAL_REPROCESSING_MODE_OFF) {
#ifdef SUB_CAMERA_DUAL_ISP_MCSC_OTF_REPROCESSING
            hwConnectionMode = SUB_CAMERA_DUAL_ISP_MCSC_OTF_REPROCESSING;
#else
            CLOGW("SUB_CAMERA_DUAL_ISP_MCSC_OTF_REPROCESSING is not defined");
#endif
        } else if (flagPIP == true) {
#ifdef FRONT_CAMERA_PIP_ISP_MCSC_OTF_REPROCESSING
            hwConnectionMode = FRONT_CAMERA_PIP_ISP_MCSC_OTF_REPROCESSING;
#else
            CLOGW("FRONT_CAMERA_PIP_ISP_MCSC_OTF_REPROCESSING is not defined");
#endif
        } else {
#ifdef FRONT_CAMERA_SINGLE_ISP_MCSC_OTF_REPROCESSING
            hwConnectionMode = FRONT_CAMERA_SINGLE_ISP_MCSC_OTF_REPROCESSING;
#else
            CLOGW("FRONT_CAMERA_SINGLE_ISP_MCSC_OTF_REPROCESSING is not defined");
#endif
        }
        break;
#endif /* USE_DUAL_CAMERA */
    default:
        CLOGE("Invalid camera ID(%d)", getCameraId());
        hwConnectionMode = HW_CONNECTION_MODE_NONE;
        break;
    }

    return hwConnectionMode;
}

#ifdef USE_DUAL_CAMERA
enum HW_CONNECTION_MODE ExynosCameraParameters::m_getReprocessingIspDcpOtf(void)
{
    enum HW_CONNECTION_MODE hwConnectionMode = HW_CONNECTION_MODE_NONE;

    bool flagPIP = getPIPMode();
    bool flagDual = getDualMode();
    enum DUAL_REPROCESSING_MODE dualReprocessingMode = getDualReprocessingMode();

    switch (getCameraId()) {
    case CAMERA_ID_BACK:
        if (flagDual == true
            && dualReprocessingMode != DUAL_REPROCESSING_MODE_OFF) {
#ifdef MAIN_CAMERA_DUAL_ISP_DCP_OTF_REPROCESSING
            hwConnectionMode = MAIN_CAMERA_DUAL_ISP_DCP_OTF_REPROCESSING;
#else
            CLOGW("MAIN_CAMERA_DUAL_ISP_DCP_OTF_REPROCESSING is not defined");
#endif
        } else if (flagPIP == true) {
#ifdef MAIN_CAMERA_PIP_ISP_DCP_OTF_REPROCESSING
            hwConnectionMode = MAIN_CAMERA_PIP_ISP_DCP_OTF_REPROCESSING;
#else
            CLOGW("MAIN_CAMERA_PIP_ISP_DCP_OTF_REPROCESSING is not defined");
#endif
        } else {
#ifdef MAIN_CAMERA_SINGLE_ISP_DCP_OTF_REPROCESSING
            hwConnectionMode = MAIN_CAMERA_SINGLE_ISP_DCP_OTF_REPROCESSING;
#else
            CLOGW("MAIN_CAMERA_SINGLE_ISP_DCP_OTF_REPROCESSING is not defined");
#endif
        }
        break;
    case CAMERA_ID_FRONT:
        if (flagDual == true
            && dualReprocessingMode != DUAL_REPROCESSING_MODE_OFF) {
#ifdef FRONT_CAMERA_DUAL_ISP_DCP_OTF_REPROCESSING
            hwConnectionMode = FRONT_CAMERA_DUAL_ISP_DCP_OTF_REPROCESSING;
#else
            CLOGW("FRONT_CAMERA_DUAL_ISP_DCP_OTF_REPROCESSING is not defined");
#endif
        } else if (flagPIP == true) {
#ifdef FRONT_CAMERA_PIP_ISP_DCP_OTF_REPROCESSING
            hwConnectionMode = FRONT_CAMERA_PIP_ISP_DCP_OTF_REPROCESSING;
#else
            CLOGW("FRONT_CAMERA_PIP_ISP_DCP_OTF_REPROCESSING is not defined");
#endif
        } else {
#ifdef FRONT_CAMERA_SINGLE_ISP_DCP_OTF_REPROCESSING
            hwConnectionMode = FRONT_CAMERA_SINGLE_ISP_DCP_OTF_REPROCESSING;
#else
            CLOGW("FRONT_CAMERA_SINGLE_ISP_DCP_OTF_REPROCESSING is not defined");
#endif
        }
        break;
    case CAMERA_ID_BACK_1:
        if (flagDual == true
            && dualReprocessingMode != DUAL_REPROCESSING_MODE_OFF) {
#ifdef SUB_CAMERA_DUAL_ISP_DCP_OTF_REPROCESSING
            hwConnectionMode = SUB_CAMERA_DUAL_ISP_DCP_OTF_REPROCESSING;
#else
            CLOGW("SUB_CAMERA_DUAL_ISP_DCP_OTF_REPROCESSING is not defined");
#endif
        } else if (flagPIP == true) {
#ifdef MAIN_CAMERA_PIP_ISP_DCP_OTF_REPROCESSING
            hwConnectionMode = MAIN_CAMERA_PIP_ISP_DCP_OTF_REPROCESSING;
#else
            CLOGW("MAIN_CAMERA_PIP_ISP_DCP_OTF_REPROCESSING is not defined");
#endif
        } else {
#ifdef MAIN_CAMERA_SINGLE_ISP_DCP_OTF_REPROCESSING
            hwConnectionMode = MAIN_CAMERA_SINGLE_ISP_DCP_OTF_REPROCESSING;
#else
            CLOGW("MAIN_CAMERA_SINGLE_ISP_DCP_OTF_REPROCESSING is not defined");
#endif
        }
        break;
    case CAMERA_ID_FRONT_1:
        if (flagDual == true
            && dualReprocessingMode != DUAL_REPROCESSING_MODE_OFF) {
#ifdef SUB_CAMERA_DUAL_ISP_DCP_OTF_REPROCESSING
            hwConnectionMode = SUB_CAMERA_DUAL_ISP_DCP_OTF_REPROCESSING;
#else
            CLOGW("SUB_CAMERA_DUAL_ISP_DCP_OTF_REPROCESSING is not defined");
#endif
        } else if (flagPIP == true) {
#ifdef FRONT_CAMERA_PIP_ISP_DCP_OTF_REPROCESSING
            hwConnectionMode = FRONT_CAMERA_PIP_ISP_DCP_OTF_REPROCESSING;
#else
            CLOGW("FRONT_CAMERA_PIP_ISP_DCP_OTF_REPROCESSING is not defined");
#endif
        } else {
#ifdef FRONT_CAMERA_SINGLE_ISP_DCP_OTF_REPROCESSING
            hwConnectionMode = FRONT_CAMERA_SINGLE_ISP_DCP_OTF_REPROCESSING;
#else
            CLOGW("FRONT_CAMERA_SINGLE_ISP_DCP_OTF_REPROCESSING is not defined");
#endif
        }
        break;
    default:
        CLOGE("Invalid camera ID(%d)", getCameraId());
        hwConnectionMode = HW_CONNECTION_MODE_NONE;
        break;
    }

    return hwConnectionMode;
}

enum HW_CONNECTION_MODE ExynosCameraParameters::m_getReprocessingDcpMcscOtf(void)
{
    enum HW_CONNECTION_MODE hwConnectionMode = HW_CONNECTION_MODE_NONE;

    bool flagPIP = getPIPMode();
    bool flagDual = getDualMode();
    enum DUAL_REPROCESSING_MODE dualReprocessingMode = getDualReprocessingMode();

    switch (getCameraId()) {
    case CAMERA_ID_BACK:
        if (flagDual == true
            && dualReprocessingMode != DUAL_REPROCESSING_MODE_OFF) {
#ifdef MAIN_CAMERA_DUAL_DCP_MCSC_OTF_REPROCESSING
            hwConnectionMode = MAIN_CAMERA_DUAL_DCP_MCSC_OTF_REPROCESSING;
#else
            CLOGW("MAIN_CAMERA_DUAL_DCP_MCSC_OTF_REPROCESSING is not defined");
#endif
        } else if (flagPIP == true) {
#ifdef MAIN_CAMERA_PIP_DCP_MCSC_OTF_REPROCESSING
            hwConnectionMode = MAIN_CAMERA_PIP_DCP_MCSC_OTF_REPROCESSING;
#else
            CLOGW("MAIN_CAMERA_PIP_DCP_MCSC_OTF_REPROCESSING is not defined");
#endif
        } else {
#ifdef MAIN_CAMERA_SINGLE_DCP_MCSC_OTF_REPROCESSING
            hwConnectionMode = MAIN_CAMERA_SINGLE_DCP_MCSC_OTF_REPROCESSING;
#else
            CLOGW("MAIN_CAMERA_SINGLE_DCP_MCSC_OTF_REPROCESSING is not defined");
#endif
        }
        break;
    case CAMERA_ID_FRONT:
        if (flagDual == true
            && dualReprocessingMode != DUAL_REPROCESSING_MODE_OFF) {
#ifdef FRONT_CAMERA_DUAL_DCP_MCSC_OTF_REPROCESSING
            hwConnectionMode = FRONT_CAMERA_DUAL_DCP_MCSC_OTF_REPROCESSING;
#else
            CLOGW("FRONT_CAMERA_DUAL_DCP_MCSC_OTF_REPROCESSING is not defined");
#endif
        } else if (flagPIP == true) {
#ifdef FRONT_CAMERA_PIP_DCP_MCSC_OTF_REPROCESSING
            hwConnectionMode = FRONT_CAMERA_PIP_DCP_MCSC_OTF_REPROCESSING;
#else
            CLOGW("FRONT_CAMERA_PIP_DCP_MCSC_OTF_REPROCESSING is not defined");
#endif
        } else {
#ifdef FRONT_CAMERA_SINGLE_DCP_MCSC_OTF_REPROCESSING
            hwConnectionMode = FRONT_CAMERA_SINGLE_DCP_MCSC_OTF_REPROCESSING;
#else
            CLOGW("FRONT_CAMERA_SINGLE_DCP_MCSC_OTF_REPROCESSING is not defined");
#endif
        }
        break;
    case CAMERA_ID_BACK_1:
        if (flagDual == true
            && dualReprocessingMode != DUAL_REPROCESSING_MODE_OFF) {
#ifdef SUB_CAMERA_DUAL_DCP_MCSC_OTF_REPROCESSING
            hwConnectionMode = SUB_CAMERA_DUAL_DCP_MCSC_OTF_REPROCESSING;
#else
            CLOGW("SUB_CAMERA_DUAL_DCP_MCSC_OTF_REPROCESSING is not defined");
#endif
        } else if (flagPIP == true) {
#ifdef MAIN_CAMERA_PIP_DCP_MCSC_OTF_REPROCESSING
            hwConnectionMode = MAIN_CAMERA_PIP_DCP_MCSC_OTF_REPROCESSING;
#else
            CLOGW("MAIN_CAMERA_PIP_DCP_MCSC_OTF_REPROCESSING is not defined");
#endif
        } else {
#ifdef MAIN_CAMERA_SINGLE_DCP_MCSC_OTF_REPROCESSING
            hwConnectionMode = MAIN_CAMERA_SINGLE_DCP_MCSC_OTF_REPROCESSING;
#else
            CLOGW("MAIN_CAMERA_SINGLE_DCP_MCSC_OTF_REPROCESSING is not defined");
#endif
        }
        break;
    case CAMERA_ID_FRONT_1:
        if (flagDual == true
            && dualReprocessingMode != DUAL_REPROCESSING_MODE_OFF) {
#ifdef SUB_CAMERA_DUAL_DCP_MCSC_OTF_REPROCESSING
            hwConnectionMode = SUB_CAMERA_DUAL_DCP_MCSC_OTF_REPROCESSING;
#else
            CLOGW("SUB_CAMERA_DUAL_DCP_MCSC_OTF_REPROCESSING is not defined");
#endif
        } else if (flagPIP == true) {
#ifdef FRONT_CAMERA_PIP_DCP_MCSC_OTF_REPROCESSING
            hwConnectionMode = FRONT_CAMERA_PIP_DCP_MCSC_OTF_REPROCESSING;
#else
            CLOGW("FRONT_CAMERA_PIP_DCP_MCSC_OTF_REPROCESSING is not defined");
#endif
        } else {
#ifdef FRONT_CAMERA_SINGLE_DCP_MCSC_OTF_REPROCESSING
            hwConnectionMode = FRONT_CAMERA_SINGLE_DCP_MCSC_OTF_REPROCESSING;
#else
            CLOGW("FRONT_CAMERA_SINGLE_DCP_MCSC_OTF_REPROCESSING is not defined");
#endif
        }
        break;
    default:
        CLOGE("Invalid camera ID(%d)", getCameraId());
        hwConnectionMode = HW_CONNECTION_MODE_NONE;
        break;
    }

    return hwConnectionMode;
}
#endif /* USE_DUAL_CAMERA */

enum HW_CONNECTION_MODE ExynosCameraParameters::m_getReprocessingMcscVraOtf(void)
{
    enum HW_CONNECTION_MODE hwConnectionMode = HW_CONNECTION_MODE_NONE;

    bool flagPIP = getPIPMode();
#ifdef USE_DUAL_CAMERA
    bool flagDual = getDualMode();
    enum DUAL_REPROCESSING_MODE dualReprocessingMode = getDualReprocessingMode();
#endif

    switch (getCameraId()) {
    case CAMERA_ID_BACK:
#ifdef USE_DUAL_CAMERA
        if (flagDual == true
            && dualReprocessingMode != DUAL_REPROCESSING_MODE_OFF) {
#ifdef MAIN_CAMERA_DUAL_MCSC_VRA_OTF_REPROCESSING
            hwConnectionMode = MAIN_CAMERA_DUAL_MCSC_VRA_OTF_REPROCESSING;
#else
            CLOGW("MAIN_CAMERA_DUAL_MCSC_VRA_OTF_REPROCESSING is not defined");
#endif
        } else
#endif /* USE_DUAL_CAMERA */
        if (flagPIP == true) {
#ifdef MAIN_CAMERA_PIP_MCSC_VRA_OTF_REPROCESSING
            hwConnectionMode = MAIN_CAMERA_PIP_MCSC_VRA_OTF_REPROCESSING;
#else
            CLOGW("MAIN_CAMERA_PIP_MCSC_VRA_OTF_REPROCESSING is not defined");
#endif
        } else {
#ifdef MAIN_CAMERA_SINGLE_MCSC_VRA_OTF_REPROCESSING
            hwConnectionMode = MAIN_CAMERA_SINGLE_MCSC_VRA_OTF_REPROCESSING;
#else
            CLOGW("MAIN_CAMERA_SINGLE_MCSC_VRA_OTF_REPROCESSING is not defined");
#endif
        }
        break;
    case CAMERA_ID_FRONT:
#ifdef USE_DUAL_CAMERA
        if (flagDual == true
            && dualReprocessingMode != DUAL_REPROCESSING_MODE_OFF) {
#ifdef FRONT_CAMERA_DUAL_MCSC_VRA_OTF_REPROCESSING
            hwConnectionMode = FRONT_CAMERA_DUAL_MCSC_VRA_OTF_REPROCESSING;
#else
            CLOGW("FRONT_CAMERA_DUAL_MCSC_VRA_OTF_REPROCESSING is not defined");
#endif
        } else
#endif /* USE_DUAL_CAMERA */
        if (flagPIP == true) {
#ifdef FRONT_CAMERA_PIP_MCSC_VRA_OTF_REPROCESSING
            hwConnectionMode = FRONT_CAMERA_PIP_MCSC_VRA_OTF_REPROCESSING;
#else
            CLOGW("FRONT_CAMERA_PIP_MCSC_VRA_OTF_REPROCESSING is not defined");
#endif
        } else {
#ifdef FRONT_CAMERA_SINGLE_MCSC_VRA_OTF_REPROCESSING
            hwConnectionMode = FRONT_CAMERA_SINGLE_MCSC_VRA_OTF_REPROCESSING;
#else
            CLOGW("FRONT_CAMERA_SINGLE_MCSC_VRA_OTF_REPROCESSING is not defined");
#endif
        }
        break;
#ifdef USE_DUAL_CAMERA
    case CAMERA_ID_BACK_1:
        if (flagDual == true
            && dualReprocessingMode != DUAL_REPROCESSING_MODE_OFF) {
#ifdef SUB_CAMERA_DUAL_MCSC_VRA_OTF_REPROCESSING
            hwConnectionMode = SUB_CAMERA_DUAL_MCSC_VRA_OTF_REPROCESSING;
#else
            CLOGW("SUB_CAMERA_DUAL_MCSC_VRA_OTF_REPROCESSING is not defined");
#endif
        } else if (flagPIP == true) {
#ifdef MAIN_CAMERA_PIP_MCSC_VRA_OTF_REPROCESSING
            hwConnectionMode = MAIN_CAMERA_PIP_MCSC_VRA_OTF_REPROCESSING;
#else
            CLOGW("MAIN_CAMERA_PIP_MCSC_VRA_OTF_REPROCESSING is not defined");
#endif
        } else {
#ifdef MAIN_CAMERA_SINGLE_MCSC_VRA_OTF_REPROCESSING
            hwConnectionMode = MAIN_CAMERA_SINGLE_MCSC_VRA_OTF_REPROCESSING;
#else
            CLOGW("MAIN_CAMERA_SINGLE_MCSC_VRA_OTF_REPROCESSING is not defined");
#endif
        }
        break;
    case CAMERA_ID_FRONT_1:
        if (flagDual == true
            && dualReprocessingMode != DUAL_REPROCESSING_MODE_OFF) {
#ifdef SUB_CAMERA_DUAL_MCSC_VRA_OTF_REPROCESSING
            hwConnectionMode = SUB_CAMERA_DUAL_MCSC_VRA_OTF_REPROCESSING;
#else
            CLOGW("SUB_CAMERA_DUAL_MCSC_VRA_OTF_REPROCESSING is not defined");
#endif
        } else if (flagPIP == true) {
#ifdef FRONT_CAMERA_PIP_MCSC_VRA_OTF_REPROCESSING
            hwConnectionMode = FRONT_CAMERA_PIP_MCSC_VRA_OTF_REPROCESSING;
#else
            CLOGW("FRONT_CAMERA_PIP_MCSC_VRA_OTF_REPROCESSING is not defined");
#endif
        } else {
#ifdef FRONT_CAMERA_SINGLE_MCSC_VRA_OTF_REPROCESSING
            hwConnectionMode = FRONT_CAMERA_SINGLE_MCSC_VRA_OTF_REPROCESSING;
#else
            CLOGW("FRONT_CAMERA_SINGLE_MCSC_VRA_OTF_REPROCESSING is not defined");
#endif
        }
        break;
#endif /* USE_DUAL_CAMERA */
    default:
        CLOGE("Invalid camera ID(%d)", getCameraId());
        hwConnectionMode = HW_CONNECTION_MODE_NONE;
        break;
    }

    return hwConnectionMode;
}

bool ExynosCameraParameters::isUse3aaInputCrop(void)
{
    return true;
}

bool ExynosCameraParameters::isUseIspInputCrop(void)
{
    if (isUse3aaInputCrop() == true
        || m_get3aaIspOtf() != HW_CONNECTION_MODE_M2M)
        return false;
    else
        return true;
}

bool ExynosCameraParameters::isUseMcscInputCrop(void)
{
    if (isUse3aaInputCrop() == true
        || isUseIspInputCrop() == true
        || m_getIspMcscOtf() != HW_CONNECTION_MODE_M2M)
        return false;
    else
        return true;
}

bool ExynosCameraParameters::isUseReprocessing3aaInputCrop(void)
{
    return true;
}

bool ExynosCameraParameters::isUseReprocessingIspInputCrop(void)
{
    if (isUseReprocessing3aaInputCrop() == true
        || m_getReprocessing3aaIspOtf() != HW_CONNECTION_MODE_M2M)
        return false;
    else
        return true;
}

bool ExynosCameraParameters::isUseReprocessingMcscInputCrop(void)
{
    if (isUseReprocessing3aaInputCrop() == true
        || isUseReprocessingIspInputCrop() == true
        || m_getReprocessingIspMcscOtf() != HW_CONNECTION_MODE_M2M)
        return false;
    else
        return true;
}

bool ExynosCameraParameters::isUseEarlyFrameReturn(void)
{
#if defined(USE_EARLY_FRAME_RETURN)
    return true;
#else
    return false;
#endif
}

bool ExynosCameraParameters::isUseHWFC(void)
{
#if defined(USE_JPEG_HWFC)
    return USE_JPEG_HWFC;
#else
    return false;
#endif
}

bool ExynosCameraParameters::isHWFCOnDemand(void)
{
#if defined(USE_JPEG_HWFC_ONDEMAND)
    return USE_JPEG_HWFC_ONDEMAND;
#else
    return false;
#endif
}

struct ExynosCameraSensorInfoBase *ExynosCameraParameters::getSensorStaticInfo()
{
    return m_staticInfo;
}

bool ExynosCameraParameters::getSetFileCtlMode(void)
{
#ifdef SET_SETFILE_BY_SET_CTRL
    return true;
#else
    return false;
#endif
}

bool ExynosCameraParameters::getSetFileCtl3AA(void)
{
#ifdef SET_SETFILE_BY_SET_CTRL_3AA
    return SET_SETFILE_BY_SET_CTRL_3AA;
#else
    return false;
#endif
}

bool ExynosCameraParameters::getSetFileCtlISP(void)
{
#ifdef SET_SETFILE_BY_SET_CTRL_ISP
    return SET_SETFILE_BY_SET_CTRL_ISP;
#else
    return false;
#endif
}

void ExynosCameraParameters::m_getV4l2Name(char* colorName, size_t length, int colorFormat)
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

int32_t ExynosCameraParameters::getYuvStreamMaxNum(void)
{
    int32_t yuvStreamMaxNum = -1;

    if (m_staticInfo == NULL) {
        CLOGE("m_staticInfo is NULL");

        return INVALID_OPERATION;
    }

    yuvStreamMaxNum = m_staticInfo->maxNumOutputStreams[PROCESSED];
    if (yuvStreamMaxNum < 0) {
        CLOGE("Invalid MaxNumOutputStreamsProcessed %d", yuvStreamMaxNum);
        return BAD_VALUE;
    }

    return yuvStreamMaxNum;
}

int32_t ExynosCameraParameters::getInputStreamMaxNum(void)
{
    int32_t inputStreamMaxNum = -1;

    if (m_staticInfo == NULL) {
        CLOGE("m_staticInfo is NULL");

        return INVALID_OPERATION;
    }

    inputStreamMaxNum = m_staticInfo->maxNumInputStreams;
    if (inputStreamMaxNum < 0) {
        CLOGE("Invalid MaxNumInputStreams %d", inputStreamMaxNum);
        return BAD_VALUE;
    }

    return inputStreamMaxNum;
}

status_t ExynosCameraParameters::setYuvBufferCount(const int count, const int outputPortId)
{
    if (count < 0 || count > VIDEO_MAX_FRAME
            || outputPortId < 0 || outputPortId >= YUV_OUTPUT_PORT_ID_MAX) {
        CLOGE("Invalid argument. count %d outputPortId %d", count, outputPortId);
        return BAD_VALUE;
    }

    m_yuvBufferCount[outputPortId] = count;

    return NO_ERROR;
}

int ExynosCameraParameters::getYuvBufferCount(const int outputPortId)
{
    if (outputPortId < 0 || outputPortId >= YUV_OUTPUT_PORT_ID_MAX) {
        CLOGE("Invalid index %d", outputPortId);
        return 0;
    }

    return m_yuvBufferCount[outputPortId];
}

void ExynosCameraParameters::resetYuvBufferCount(void)
{
    memset(m_yuvBufferCount, 0, sizeof(m_yuvBufferCount));
}

void ExynosCameraParameters::setHighSpeedMode(uint32_t mode)
{
    switch(mode){
     case CONFIG_MODE::HIGHSPEED_60:
        setConfigMode(CONFIG_MODE::HIGHSPEED_60);
        m_setHighSpeedRecording(true);
        break;
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

int ExynosCameraParameters::getMaxHighSpeedFps(void)
{
   int maxFps = 0;
   int (*sizeList)[2];

   if (m_staticInfo->highSpeedVideoFPSList != NULL) {
        sizeList = m_staticInfo->highSpeedVideoFPSList;
        for (int i = 0; i < m_staticInfo->highSpeedVideoFPSListMax; i++) {
            if ((sizeList[i][0] == sizeList[i][1])
                && (sizeList[i][1]/1000 > maxFps)) {
                maxFps = sizeList[i][1]/1000;
            }
       }
   }

   return maxFps;
}

bool ExynosCameraParameters::checkFaceDetectMeta(struct camera2_shot_ext *shot_ext)
{
    Mutex::Autolock lock(m_faceDetectMetaLock);
    bool ret = false;

    if (shot_ext->shot.ctl.stats.faceDetectMode > FACEDETECT_MODE_OFF) {
        if (shot_ext->shot.dm.stats.faceDetectMode > FACEDETECT_MODE_OFF
            && m_metadata.shot.dm.request.frameCount < shot_ext->shot.dm.request.frameCount) {

            m_metadata.shot.dm.request.frameCount = shot_ext->shot.dm.request.frameCount;
            m_metadata.shot.dm.stats.faceDetectMode = shot_ext->shot.dm.stats.faceDetectMode;

            for (int i = 0; i < CAMERA2_MAX_FACES; i++) {
                m_metadata.shot.dm.stats.faceIds[i] = shot_ext->shot.dm.stats.faceIds[i];
                m_metadata.shot.dm.stats.faceScores[i] = shot_ext->shot.dm.stats.faceScores[i];
                m_metadata.shot.dm.stats.faces[i] = shot_ext->shot.dm.stats.faces[i];
                for (int j = 0; j < 6; j++) {
                    m_metadata.shot.dm.stats.faceLandmarks[i][j] = shot_ext->shot.dm.stats.faceLandmarks[i][j];
                }
                for (int j = 0; j < 4; j++) {
                    m_metadata.shot.dm.stats.faceRectangles[i][j] = shot_ext->shot.dm.stats.faceRectangles[i][j];
                }
            }
        } else if (shot_ext->shot.dm.stats.faceDetectMode <= FACEDETECT_MODE_OFF) {
            shot_ext->shot.dm.stats.faceDetectMode = m_metadata.shot.dm.stats.faceDetectMode;

            for (int i = 0; i < CAMERA2_MAX_FACES; i++) {
                shot_ext->shot.dm.stats.faceIds[i] = m_metadata.shot.dm.stats.faceIds[i];
                shot_ext->shot.dm.stats.faceScores[i] = m_metadata.shot.dm.stats.faceScores[i];
                shot_ext->shot.dm.stats.faces[i] = m_metadata.shot.dm.stats.faces[i];
                for (int j = 0; j < 6; j++) {
                    shot_ext->shot.dm.stats.faceLandmarks[i][j] = m_metadata.shot.dm.stats.faceLandmarks[i][j];
                }
                for (int j = 0; j < 4; j++) {
                    shot_ext->shot.dm.stats.faceRectangles[i][j] = m_metadata.shot.dm.stats.faceRectangles[i][j];
                }
            }

            ret = true;
        }
    }

    return ret;
}

void ExynosCameraParameters::setUseFullSizeLUT(bool enable)
{
    m_isFullSizeLut = enable;
}

bool ExynosCameraParameters::m_getUseFullSizeLUT(void)
{
    return m_isFullSizeLut;
}

void ExynosCameraParameters::setYsumRecordingMode(bool enable)
{
    m_cameraInfo.ysumRecordingMode = enable;
}

bool ExynosCameraParameters::getYsumRecordingMode(void)
{
#ifdef USE_YSUM_RECORDING
    if (getPIPMode() == true)
        return false;

    return m_cameraInfo.ysumRecordingMode;
#endif
    return false;
}

void ExynosCameraParameters::setShotMode(int shotMode)
{
    m_cameraInfo.shotMode = shotMode;
}

void ExynosCameraParameters::setManualAeControl(bool isManualAeControl)
{
    m_isManualAeControl = isManualAeControl;
}

bool ExynosCameraParameters::getManualAeControl(void)
{
    return m_isManualAeControl;
}

void ExynosCameraParameters::setFlashMode(int flashMode)
{
    m_metaParameters.m_flashMode = flashMode;
}

int ExynosCameraParameters::getFlashMode(void)
{
    return m_metaParameters.m_flashMode;
}

void ExynosCameraParameters::setCaptureExposureTime(uint64_t exposureTime)
{
    m_exposureTimeCapture = exposureTime;
}

uint64_t ExynosCameraParameters::getCaptureExposureTime(void)
{
    return m_exposureTimeCapture;
}

bool ExynosCameraParameters::getHfdMode(void)
{
    bool useHfd = false;

    /*
       This is Static value,
       If you want off to HFD operation,
       turn off HFD pipe request
     */
#ifdef SUPPORT_HFD
    if (m_cameraId == CAMERA_ID_FRONT
        && getPIPMode() == false) {
        useHfd = true;
    }
#endif

    return useHfd;
}

void ExynosCameraParameters::updateMetaParameter(struct CameraMetaParameters *metaParameters)
{
    memcpy(&this->m_metaParameters, metaParameters, sizeof(struct CameraMetaParameters));
}
}; /* namespace android */
