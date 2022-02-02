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
#define LOG_TAG "ExynosCameraConfigurations"
#include <log/log.h>

#include "ExynosCameraConfigurations.h"

namespace android {

ExynosCameraConfigurations::ExynosCameraConfigurations(int cameraId, int scenario)
{
    m_cameraId = cameraId;
    m_scenario = scenario;

    switch (m_cameraId) {
    case CAMERA_ID_BACK:
        strncpy(m_name, "Back", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        break;
    case CAMERA_ID_FRONT:
        strncpy(m_name, "Front", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
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
        CLOGE("Invalid camera ID(%d)", m_cameraId);
        break;
    }

    m_staticInfo = createExynosCameraSensorInfo(m_cameraId);

    m_exynosconfig = NULL;
    memset(&m_metaParameters, 0, sizeof(struct CameraMetaParameters));

    m_metaParameters.m_zoomRatio = 1.0f;
    m_metaParameters.m_flashMode = FLASH_MODE_OFF;

    for (int i = 0; i < CAMERA_ID_MAX; i++) {
        m_parameters[i] = NULL;
    }

    m_exynosconfig = new ExynosConfigInfo();
    memset((void *)m_exynosconfig, 0x00, sizeof(struct ExynosConfigInfo));

    // CAUTION!! : Initial values must be prior to setDefaultParameter() function.
    // Initial Values : START
    m_flagRestartStream = false;

#ifdef USE_DUAL_CAMERA
    m_tryDualOperationMode = DUAL_OPERATION_MODE_MASTER;
    m_tryDualOperationModeReprocessing = DUAL_OPERATION_MODE_MASTER;

    m_dualOperationMode = DUAL_OPERATION_MODE_NONE;
    m_dualOperationModeReprocessing = DUAL_OPERATION_MODE_NONE;
    m_dualOperationModeLockCount = 0;
    m_dualHwSyncOn = true;
#endif

    memset(m_width, 0, sizeof(m_width));
    memset(m_height, 0, sizeof(m_width));
    memset(m_mode, 0, sizeof(m_mode));
    memset(m_flagCheck, 0, sizeof(m_flagCheck));
    memset(m_modeValue, 0, sizeof(m_modeValue));
    memset(m_yuvWidth, 0, sizeof(m_yuvWidth));
    memset(m_yuvHeight, 0, sizeof(m_yuvHeight));
    memset(m_yuvFormat, 0, sizeof(m_yuvFormat));
    memset(m_yuvBufferCount, 0, sizeof(m_yuvBufferCount));

    m_mode[CONFIGURATION_DYNAMIC_BAYER_MODE] = (m_cameraId == CAMERA_ID_BACK) ? USE_DYNAMIC_BAYER : USE_DYNAMIC_BAYER_FRONT;
    m_useDynamicBayer[CONFIG_MODE::NORMAL] = m_mode[CONFIGURATION_DYNAMIC_BAYER_MODE];
    m_useDynamicBayer[CONFIG_MODE::HIGHSPEED_60] = (m_cameraId == CAMERA_ID_BACK) ? USE_DYNAMIC_BAYER_60FPS : USE_DYNAMIC_BAYER_60FPS_FRONT;
    m_useDynamicBayer[CONFIG_MODE::HIGHSPEED_120] = (m_cameraId == CAMERA_ID_BACK) ? USE_DYNAMIC_BAYER_120FPS : USE_DYNAMIC_BAYER_120FPS_FRONT;
    m_useDynamicBayer[CONFIG_MODE::HIGHSPEED_240] = (m_cameraId == CAMERA_ID_BACK) ? USE_DYNAMIC_BAYER_240FPS : USE_DYNAMIC_BAYER_240FPS_FRONT;
    m_useDynamicBayer[CONFIG_MODE::HIGHSPEED_480] = (m_cameraId == CAMERA_ID_BACK) ? USE_DYNAMIC_BAYER_480FPS : USE_DYNAMIC_BAYER_480FPS_FRONT;

    m_yuvBufferStat = false;

    for (int i = 0; i < YUV_OUTPUT_PORT_ID_MAX; i++) {
        m_yuvBufferCount[i] = 1; //YUV
    }

    m_minFps = 0;
    m_maxFps = 0;

    resetSize(CONFIGURATION_MIN_YUV_SIZE);
    setModeValue(CONFIGURATION_YUV_STALL_PORT_USAGE, YUV_STALL_USAGE_DSCALED);

    m_exposureTimeCapture = 0L;

#ifdef USES_DUAL_CAMERA_SOLUTION_ARCSOFT
    m_arcSoftCameraIndex = 0;
    m_arcSoftViewRatio = 1.0f;
    m_arcSoftImageShiftX = 0;
    m_arcSoftImageShiftY = 0;
    m_arcSoftMaster3A = DUAL_OPERATION_MODE_MASTER;
#endif

    m_vendorSpecificConstructor();

    // Initial Values : END
    setDefaultCameraInfo();
}

ExynosCameraConfigurations::~ExynosCameraConfigurations()
{
    if (m_exynosconfig != NULL) {
        memset((void *)m_exynosconfig, 0x00, sizeof(struct ExynosConfigInfo));
        delete m_exynosconfig;
        m_exynosconfig = NULL;
    }

    if (m_staticInfo != NULL) {
        delete m_staticInfo;
        m_staticInfo = NULL;
    }

    m_vendorSpecificDestructor();
}

void ExynosCameraConfigurations::setDefaultCameraInfo(void)
{
    CLOGI("");

    for (int i = 0; i < YUV_MAX; i++) {
        /* YUV */
        setYuvFormat(V4L2_PIX_FMT_NV21, i);

        /* YUV_STALL */
        setYuvFormat(V4L2_PIX_FMT_NV21, i + YUV_MAX);
    }

    /* Initalize Binning scale ratio */
    setModeValue(CONFIGURATION_BINNING_RATIO, 1000);

    setMode(CONFIGURATION_RECORDING_MODE, false);
    setMode(CONFIGURATION_PIP_MODE, false);
}

void ExynosCameraConfigurations::setParameters(int cameraId, ExynosCameraParameters *parameters)
{
    if (cameraId < CAMERA_ID_BACK || CAMERA_ID_MAX <= cameraId) {
        android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):Invalid cameraId(%d)", __FUNCTION__, __LINE__, cameraId);
    }

    m_parameters[cameraId] = parameters;
}

ExynosCameraParameters *ExynosCameraConfigurations::getParameters(int cameraId)
{
    if(cameraId < CAMERA_ID_BACK || CAMERA_ID_MAX <= cameraId) {
        android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):Invalid cameraId(%d)", __FUNCTION__, __LINE__, cameraId);
    }

    return m_parameters[cameraId];
}

int ExynosCameraConfigurations::getScenario(void)
{
    return m_scenario;
}

void ExynosCameraConfigurations::setRestartStream(bool restart)
{
    Mutex::Autolock lock(m_modifyLock);
    m_flagRestartStream = restart;
}

bool ExynosCameraConfigurations::getRestartStream(void)
{
    Mutex::Autolock lock(m_modifyLock);
    return m_flagRestartStream;
}

bool ExynosCameraConfigurations::setConfig(struct ExynosConfigInfo* config)
{
    memcpy(m_exynosconfig, config, sizeof(struct ExynosConfigInfo));
    setConfigMode(m_exynosconfig->mode);

    return true;
}
struct ExynosConfigInfo* ExynosCameraConfigurations::getConfig(void)
{
    return m_exynosconfig;
}

bool ExynosCameraConfigurations::setConfigMode(uint32_t mode)
{
    bool ret = false;
    switch(mode){
    case CONFIG_MODE::NORMAL:
    case CONFIG_MODE::HIGHSPEED_60:
    case CONFIG_MODE::HIGHSPEED_120:
    case CONFIG_MODE::HIGHSPEED_240:
    case CONFIG_MODE::HIGHSPEED_480:
        m_exynosconfig->current = &m_exynosconfig->info[mode];
        m_exynosconfig->mode = mode;
        ret = true;
        break;
    default:
        CLOGE(" unknown config mode (%d)", mode);
    }
    return ret;
}

int ExynosCameraConfigurations::getConfigMode(void) const
{
    int ret = -1;
    switch(m_exynosconfig->mode){
    case CONFIG_MODE::NORMAL:
    case CONFIG_MODE::HIGHSPEED_60:
    case CONFIG_MODE::HIGHSPEED_120:
    case CONFIG_MODE::HIGHSPEED_240:
    case CONFIG_MODE::HIGHSPEED_480:
        ret = m_exynosconfig->mode;
        break;
    default:
        CLOGE(" unknown config mode (%d)", m_exynosconfig->mode);
    }

    return ret;
}

void ExynosCameraConfigurations::updateMetaParameter(struct CameraMetaParameters *metaParameters)
{
    memcpy(&this->m_metaParameters, metaParameters, sizeof(struct CameraMetaParameters));
    setModeValue(CONFIGURATION_FLASH_MODE, metaParameters->m_flashMode);
}

status_t ExynosCameraConfigurations::checkYuvFormat(const int format, const int outputPortId)
{
    status_t ret = NO_ERROR;
    int curYuvFormat = -1;
    int newYuvFormat = -1;

    newYuvFormat = HAL_PIXEL_FORMAT_2_V4L2_PIX(format);
    curYuvFormat = getYuvFormat(outputPortId);

    if (newYuvFormat != curYuvFormat) {
        char curFormatName[V4L2_FOURCC_LENGTH] = {};
        char newFormatName[V4L2_FOURCC_LENGTH] = {};
        getV4l2Name(curFormatName, V4L2_FOURCC_LENGTH, curYuvFormat);
        getV4l2Name(newFormatName, V4L2_FOURCC_LENGTH, newYuvFormat);
        CLOGI("curYuvFormat %s newYuvFormat %s outputPortId %d",
                curFormatName, newFormatName, outputPortId);
        setYuvFormat(newYuvFormat, outputPortId);
    }

    return ret;
}

status_t ExynosCameraConfigurations::checkPreviewFpsRange(uint32_t minFps, uint32_t maxFps)
{
    status_t ret = NO_ERROR;
    uint32_t curMinFps = 0, curMaxFps = 0;

    getPreviewFpsRange(&curMinFps, &curMaxFps);

    if (m_adjustPreviewFpsRange(minFps, maxFps) != NO_ERROR) {
        CLOGE("Fails to adjust preview fps range");
        return INVALID_OPERATION;
    }

    if (curMinFps != minFps || curMaxFps != maxFps) {
        {
            if (curMaxFps <= 30 && maxFps == 60) {
                /* 60fps mode */
                setModeValue(CONFIGURATION_HIGHSPEED_MODE, (int)(CONFIG_MODE::HIGHSPEED_60));
                setRestartStream(true);
            } else if (curMaxFps == 60 && maxFps <= 30) {
                /* 30fps mode */
                setModeValue(CONFIGURATION_HIGHSPEED_MODE, (int)(CONFIG_MODE::NORMAL));
                setRestartStream(true);
            }
        }

        setPreviewFpsRange(minFps, maxFps);
    }

    return ret;
}

void ExynosCameraConfigurations::setPreviewFpsRange(uint32_t min, uint32_t max)
{
    m_minFps = min;
    m_maxFps = max;

#ifdef USE_DUAL_CAMERA
    CLOGV("fps min(%d) max(%d)", min, max);
#else
    CLOGI("fps min(%d) max(%d)", min, max);
#endif
}

void ExynosCameraConfigurations::getPreviewFpsRange(uint32_t *min, uint32_t *max)
{
    /* ex) min = 15 , max = 30 */
    *min = m_minFps;
    *max = m_maxFps;
}

status_t ExynosCameraConfigurations::m_adjustPreviewFpsRange(__unused uint32_t &newMinFps, uint32_t &newMaxFps)
{
    bool flagSpecialMode = false;

    if (getMode(CONFIGURATION_PIP_MODE) == true) {
        flagSpecialMode = true;
        CLOGV(" PIPMode(true), newMaxFps=%d", newMaxFps);
    }

    if (getMode(CONFIGURATION_PIP_RECORDING_MODE) == true) {
        flagSpecialMode = true;
        CLOGV("PIPRecordingHint(true), newMaxFps=%d", newMaxFps);
    }

    return NO_ERROR;
}

void ExynosCameraConfigurations::setYuvFormat(const int format, const int index)
{
    int formatArrayNum = sizeof(m_yuvFormat) / sizeof(m_yuvFormat[0]);

    if (formatArrayNum != YUV_OUTPUT_PORT_ID_MAX) {
        android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):Invalid yuvFormat array length %d."\
                            " YUV_OUTPUT_PORT_ID_MAX %d",
                            __FUNCTION__, __LINE__,
                            formatArrayNum,
                            YUV_OUTPUT_PORT_ID_MAX);
        return;
    }

    m_yuvFormat[index] = format;
}

int ExynosCameraConfigurations::getYuvFormat(const int index)
{
    int formatArrayNum = sizeof(m_yuvFormat) / sizeof(m_yuvFormat[0]);

    if (formatArrayNum != YUV_OUTPUT_PORT_ID_MAX) {
        android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):Invalid yuvFormat array length %d."\
                " YUV_OUTPUT_PORT_ID_MAX %d",
                __FUNCTION__, __LINE__,
                formatArrayNum,
                YUV_OUTPUT_PORT_ID_MAX);
        return - 1;
    }

    return m_yuvFormat[index];
}

status_t ExynosCameraConfigurations::setYuvBufferCount(const int count, const int outputPortId)
{
    if (count < 0 || count > VIDEO_MAX_FRAME
            || outputPortId < 0 || outputPortId >= YUV_OUTPUT_PORT_ID_MAX) {
        CLOGE("Invalid argument. count %d outputPortId %d", count, outputPortId);
        return BAD_VALUE;
    }

    m_yuvBufferCount[outputPortId] = count;

    return NO_ERROR;
}

int ExynosCameraConfigurations::getYuvBufferCount(const int outputPortId)
{
    if (outputPortId < 0 || outputPortId >= YUV_OUTPUT_PORT_ID_MAX) {
        CLOGE("Invalid index %d", outputPortId);
        return 0;
    }

    return m_yuvBufferCount[outputPortId];
}

void ExynosCameraConfigurations::resetYuvBufferCount(void)
{
    memset(m_yuvBufferCount, 0, sizeof(m_yuvBufferCount));
}

status_t ExynosCameraConfigurations::setFrameSkipCount(int count)
{
    m_frameSkipCounter.setCount(count);

    return NO_ERROR;
}

status_t ExynosCameraConfigurations::getFrameSkipCount(int *count)
{
    *count = m_frameSkipCounter.getCount();
    m_frameSkipCounter.decCount();

    return NO_ERROR;
}

float ExynosCameraConfigurations::getZoomRatio(void)
{
    return m_metaParameters.m_zoomRatio;
}

float ExynosCameraConfigurations::getPrevZoomRatio(void)
{
    return m_metaParameters.m_prevZoomRatio;
}

#ifdef USE_DUAL_CAMERA
enum DUAL_PREVIEW_MODE ExynosCameraConfigurations::getDualPreviewMode(void)
{
    /*
    * Before setParameters, we cannot know dualMode is valid or not
    * So, check and make assert for fast debugging
    */
    if (m_flagCheck[CONFIGURATION_DUAL_MODE] == false) {
        return DUAL_PREVIEW_MODE_OFF;
    }

    if (getMode(CONFIGURATION_DUAL_MODE) == false) {
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

enum DUAL_REPROCESSING_MODE ExynosCameraConfigurations::getDualReprocessingMode(void)
{
    /*
    * Before setParameters, we cannot know dualMode is valid or not
    * So, check and make assert for fast debugging
    */
    if (m_flagCheck[CONFIGURATION_DUAL_MODE] == false) {
        return DUAL_REPROCESSING_MODE_OFF;
    }

    if (getMode(CONFIGURATION_DUAL_MODE) == false) {
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

void ExynosCameraConfigurations::setTryDualOperationMode(enum DUAL_OPERATION_MODE mode)
{
    m_tryDualOperationMode = mode;
}

enum DUAL_OPERATION_MODE ExynosCameraConfigurations::getTryDualOperationMode(void)
{
    return m_tryDualOperationMode;
}

void ExynosCameraConfigurations::setDualOperationMode(enum DUAL_OPERATION_MODE mode)
{
    m_dualOperationMode = mode;
}

enum DUAL_OPERATION_MODE ExynosCameraConfigurations::getDualOperationMode(void)
{
    return m_dualOperationMode;
}

void ExynosCameraConfigurations::setTryDualOperationModeReprocessing(enum DUAL_OPERATION_MODE mode)
{
    m_tryDualOperationModeReprocessing = mode;
}

enum DUAL_OPERATION_MODE ExynosCameraConfigurations::getTryDualOperationModeReprocessing(void)
{
    return m_tryDualOperationModeReprocessing;
}

void ExynosCameraConfigurations::setDualOperationModeReprocessing(enum DUAL_OPERATION_MODE mode)
{
    m_dualOperationModeReprocessing = mode;
}

enum DUAL_OPERATION_MODE ExynosCameraConfigurations::getDualOperationModeReprocessing(void)
{
    return m_dualOperationModeReprocessing;
}

void ExynosCameraConfigurations::setDualOperationModeLockCount(int32_t count)
{
    Mutex::Autolock lock(m_lockDualOperationModeLock);

    CLOGV("%d -> %d", m_dualOperationModeLockCount, count);

    m_dualOperationModeLockCount = count;
}

int32_t ExynosCameraConfigurations::getDualOperationModeLockCount(void)
{
    Mutex::Autolock lock(m_lockDualOperationModeLock);

    return m_dualOperationModeLockCount;
}

void ExynosCameraConfigurations::decreaseDualOperationModeLockCount(void)
{
    Mutex::Autolock lock(m_lockDualOperationModeLock);

    if (m_dualOperationModeLockCount > 0)
        m_dualOperationModeLockCount--;
}

void ExynosCameraConfigurations::setDualHwSyncOn(bool hwSyncOn)
{
    m_dualHwSyncOn = hwSyncOn;
}

bool ExynosCameraConfigurations::getDualHwSyncOn(void) const
{
    return m_dualHwSyncOn;
}
#endif

bool ExynosCameraConfigurations::m_getGmvMode(void) const
{
    bool useGmv = false;

#ifdef SUPPORT_GMV
        useGmv = true;
#endif
    return useGmv;
}

}; /* namespace android */
