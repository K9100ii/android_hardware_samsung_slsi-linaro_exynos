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

#ifdef USE_DUAL_CAMERA
/*
 * Dual Parameters(singleton)
 */
void ExynosCamera1DualParameters::init()
{
    m_zoom = 0;
    m_LLSValue = 0;
    m_lockDualCameraSyncType = false;
#ifdef SAMSUNG_DUAL_SOLUTION
    m_forceWide = false;
    m_wideFocusState = -1;
    m_teleFocusState = -1;
    m_wideFocusDone = false;
    m_teleFocusDone = false;
    m_forceWideCond = false;
    m_forceWideOnDelay = 0;
    m_forceWideOffDelay = 0;
    m_checkForceWideCount = 0;
    m_checkForceWideCond = 0;
    m_dualLux = 0;
    m_dualFocusDistance = 0;
    m_isFusionCapture = false;
    m_dualGyroHint = false;
    m_dualHrmHint = false;
    m_dualAccHint = false;
#endif
    m_dualCameraSyncType = SYNC_TYPE_BASE;
    m_dualCameraReprocessingSyncType = SYNC_TYPE_BASE;
    strncpy(m_name, "DualParameter",  EXYNOS_CAMERA_NAME_STR_SIZE - 1);

    m_dualNotifyThread = new DualNotifyThread(this, &ExynosCamera1DualParameters::m_dualNotifyThreadFunc, "dualNotifyThread", PRIORITY_DISPLAY);
    m_dualNotifyQ.setup(m_dualNotifyThread);
    m_flagDualNotifyThreadStop = true;

    for (int i = 0; i < CAMERA_ID_MAX; i++) {
        m_camera[i] = NULL;
        m_params[i] = NULL;
        m_dualNotifyCallbackFunc[i] = NULL;
        m_flagFinishStartPreview[i] = false;
    }
}

void ExynosCamera1DualParameters::setDualCameraSyncType(int32_t cameraId, sync_type_t *syncType, sync_type_t *reprocessingSyncType)
{
    Mutex::Autolock lock(m_lock);

    CLOGV2(CAM_ID "syncType %d -> %d/reprocessingSyncType %d -> %d(lock:%d, switching:%d)",
            cameraId, m_name, m_dualCameraSyncType, *syncType,
            m_dualCameraReprocessingSyncType, *reprocessingSyncType,
            m_lockDualCameraSyncType, m_flagForceSwitchingOnly);

    if (!m_lockDualCameraSyncType) {
        /* forcely settting for switching only mode */
        if (m_flagForceSwitchingOnly) {
            /* for preview */
            if (*syncType == SYNC_TYPE_SYNC) {
                m_dualCameraSyncType = SYNC_TYPE_BYPASS;
            } else {
                m_dualCameraSyncType = *syncType;
            }

            /* for capture */
            if (*reprocessingSyncType == SYNC_TYPE_SYNC) {
                m_dualCameraReprocessingSyncType = SYNC_TYPE_BYPASS;
            } else {
                m_dualCameraReprocessingSyncType = *reprocessingSyncType;
            }
        } else {
#ifdef SAMSUNG_DUAL_SOLUTION
            int32_t masterCameraId = -1;
            int32_t slaveCameraId = -1;

            getDualCameraId(&masterCameraId, &slaveCameraId);

            if (m_forceWide) {
                if (m_checkForceWideCount == 0)
                    m_checkForceWideCount = 30;

                if (cameraId == masterCameraId) {
                    if (!m_checkForceWideCond) {
                        /* not decide forceWide in Bypass mode directly */
                        m_dualCameraSyncType = SYNC_TYPE_BYPASS;
                        m_dualCameraReprocessingSyncType = SYNC_TYPE_BYPASS;

                        /* Check tele meta after some frames */
                        m_checkForceWideCount--;
                        if (m_checkForceWideCount == 0)
                            m_checkForceWideCond = true;
                    } else {
                        /* Enter sync mode for some frames */
                        m_dualCameraSyncType = SYNC_TYPE_SYNC;
                        m_dualCameraReprocessingSyncType = SYNC_TYPE_SYNC;
                        m_checkForceWideCount--;
                        if (m_checkForceWideCount == 0)
                            m_checkForceWideCond = false;

                        /* Check tele meta during some frames */
                        if (!m_forceWideCond)
                            m_forceWideOffDelay++;
                        else
                            m_forceWideOffDelay = 0;

                        /* Decide forcewide */
                        if (m_forceWideOffDelay >= FORCE_WIDE_OFF_DELAY) {
                            m_forceWide = false;
                            CLOGD2(CAM_ID "[ForceWide] disable forceWide", cameraId, m_name);
                        } else {
                            m_forceWide = true;
                        }
                    }
                    m_forceWideOnDelay = 0;
                }
            } else {
                if (cameraId == slaveCameraId) {
                    /* m_forceWideCond check for some frames and enable forceWide */
                    if (m_forceWideCond)
                        m_forceWideOnDelay++;
                    else
                        m_forceWideOnDelay = 0;

                    if (m_forceWideOnDelay >= FORCE_WIDE_ON_DELAY) {
                        m_forceWide = true;
                        CLOGD2(CAM_ID "[ForceWide] enable forceWide", cameraId, m_name);
                    } else {
                        m_forceWide = false;
                    }
                }
                m_forceWideOffDelay = 0;
                m_checkForceWideCount = 0;

                m_dualCameraSyncType = *syncType;
                m_dualCameraReprocessingSyncType = *reprocessingSyncType;
            }

            CLOGV2(CAM_ID "[ForceWide] syncType(%d), forceWide(%d), m_checkForceWideCount(%d), onDelay(%d), offDelay(%d)",
                    cameraId, m_name, m_dualCameraSyncType, m_forceWide,
                    m_checkForceWideCount, m_forceWideOnDelay, m_forceWideOffDelay);
#else
            m_dualCameraSyncType = *syncType;
            m_dualCameraReprocessingSyncType = *reprocessingSyncType;
#endif
        }
    }

    *syncType = m_dualCameraSyncType;
    *reprocessingSyncType = m_dualCameraReprocessingSyncType;
}

sync_type_t ExynosCamera1DualParameters::getDualCameraSyncType(int32_t cameraId)
{
    Mutex::Autolock lock(m_lock);

    return m_dualCameraSyncType;
}

void ExynosCamera1DualParameters::setRecordingHint(int32_t cameraId, bool hint)
{
    Mutex::Autolock lock(m_lock);

    m_recordingHint = hint;
}

bool ExynosCamera1DualParameters::getRecordingHint(int32_t cameraId)
{
    Mutex::Autolock lock(m_lock);

    return m_recordingHint;
}

sync_type_t ExynosCamera1DualParameters::getDualCameraReprocessingSyncType(int32_t cameraId)
{
    Mutex::Autolock lock(m_lock);

    return m_dualCameraReprocessingSyncType;
}

void ExynosCamera1DualParameters::setZoom(int32_t cameraId, int zoom)
{
    Mutex::Autolock lock(m_lock);

    CLOGV2(CAM_ID "zoom %d -> %d", cameraId, m_name, m_zoom, zoom);

    m_zoom = zoom;
}

int ExynosCamera1DualParameters::getZoom(int32_t cameraId)
{
    Mutex::Autolock lock(m_lock);

    return m_zoom;
}

void ExynosCamera1DualParameters::setLLSValue(int32_t cameraId, int LLSValue)
{
    Mutex::Autolock lock(m_lock);

    CLOGV2(CAM_ID "lls %d -> %d", cameraId, m_name, m_LLSValue, LLSValue);

    if (!m_lockDualCameraSyncType)
        m_LLSValue = LLSValue;
}

int ExynosCamera1DualParameters::getLLSValue(int32_t cameraId)
{
    Mutex::Autolock lock(m_lock);

    return m_LLSValue;
}

#ifdef SAMSUNG_DUAL_SOLUTION
void ExynosCamera1DualParameters::setMasterCam(int32_t cameraId, int curDispCamId)
{
    Mutex::Autolock lock(m_lock);

    m_masterCam = curDispCamId;
}

int32_t ExynosCamera1DualParameters::getMasterCam(int32_t cameraId)
{
    Mutex::Autolock lock(m_lock);

    return m_masterCam;
}

void ExynosCamera1DualParameters::setForceWide(int32_t cameraId, bool forceWide)
{
    Mutex::Autolock lock(m_lock);

    if (!m_lockDualCameraSyncType) {
        if (m_forceWide != forceWide) {
            CLOGI2(CAM_ID "[ForceWide] forceWide %d -> %d", cameraId, m_name, m_forceWide, forceWide);
            m_forceWide = forceWide;
        }
    }
}

int ExynosCamera1DualParameters::getForceWide(int32_t cameraId)
{
    Mutex::Autolock lock(m_lock);

    return m_forceWide;
}

int ExynosCamera1DualParameters::checkDualAfState(int cameraId, int focusState)
{
    Mutex::Autolock lock(m_lock);

    int ret;
    int masterCameraId, slaveCameraId;
    getDualCameraId(&masterCameraId, &slaveCameraId);

    if (cameraId == masterCameraId) {
        m_wideFocusState = focusState;
        m_wideFocusDone = true;
    } else if (cameraId == slaveCameraId) {
        m_teleFocusState = focusState;
        m_teleFocusDone = true;
    }

    if (m_wideFocusDone && m_teleFocusDone) {
        ret = m_wideFocusState;
        m_wideFocusDone = false;
        m_teleFocusDone = false;
    } else
        ret = -1;

    return ret;
}

void ExynosCamera1DualParameters::setForceWideCond(int32_t cameraId, bool forceWideCond)
{
    Mutex::Autolock lock(m_lock);

    if (m_forceWideCond != forceWideCond)
        CLOGI2(CAM_ID "[ForceWide] forceWideCond %d -> %d", cameraId, m_name, m_forceWideCond, forceWideCond);

    m_forceWideCond = forceWideCond;
}

bool ExynosCamera1DualParameters::getForceWideCond(int32_t cameraId)
{
    Mutex::Autolock lock(m_lock);

    return m_forceWideCond;
}

void ExynosCamera1DualParameters::checkFusionCaptureMode(int cameraId,
                                        struct camera2_shot_ext *wideMeta, struct camera2_shot_ext *teleMeta)
{
    Mutex::Autolock lock(m_lock);

    if (!m_lockDualCameraSyncType) {
        m_dualLux = (int32_t)wideMeta->shot.udm.ae.vendorSpecific[386];
        m_dualFocusDistance = (int32_t)teleMeta->shot.dm.aa.vendor_objectDistanceCm;

        if (m_dualLux >= DUAL_LOW_LIGHT_CONDITION_NORMAL &&
                m_dualFocusDistance >= DUAL_DISTANCE_CONDITION_NORMAL &&
                m_teleFocusState == FOCUS_RESULT_SUCCESS)
            m_isFusionCapture = true;
        else
            m_isFusionCapture = false;
    }

    return;
}

bool ExynosCamera1DualParameters::getFusionCaptureMode(int cameraId, bool isLogPrint)
{
    Mutex::Autolock lock(m_lock);

    if (isLogPrint)
        CLOGD2(CAM_ID "Lux: %f, Focus distance: %d, Tele focus state: %d",
                cameraId, m_name, m_dualLux/256.0, m_dualFocusDistance, m_teleFocusState);

    return m_isFusionCapture;
}

#ifdef SAMSUNG_SENSOR_LISTENER
#ifdef SAMSUNG_GYRO
void ExynosCamera1DualParameters::setDualGyro(SensorListenerEvent_t data)
{
    Mutex::Autolock lock(m_lock);

    setMetaCtlGyro(&m_dualmetadata, data);
}

void ExynosCamera1DualParameters::setDualGyroHint(bool onOff)
{
    m_dualGyroHint = onOff;
}

bool ExynosCamera1DualParameters::getDualGyroHint(void)
{
    return m_dualGyroHint;
}
#endif
#ifdef SAMSUNG_HRM
void ExynosCamera1DualParameters::setDualHRM(int ir_data, int flicker_data, int status)
{
    Mutex::Autolock lock(m_lock);

    setMetaCtlHRM(&m_dualmetadata, ir_data, flicker_data, status);
}

void ExynosCamera1DualParameters::setDualHRMHint(bool onOff)
{
    m_dualHrmHint = onOff;
}

bool ExynosCamera1DualParameters::getDualHRMHint(void)
{
    return m_dualHrmHint;
}
#endif
#ifdef SAMSUNG_ACCELEROMETER
void ExynosCamera1DualParameters::setDualAccelerometer(SensorListenerEvent_t data)
{
    Mutex::Autolock lock(m_lock);

    setMetaCtlAcceleration(&m_dualmetadata, data);
}

void ExynosCamera1DualParameters::setDualAccelerometerHint(bool onOff)
{
    m_dualAccHint = onOff;
}

bool ExynosCamera1DualParameters::getDualAccelerometerHint(void)
{
    return m_dualAccHint;
}
#endif
#endif /* SAMSUNG_SENSOR_LISTENER */
camera2_shot_ext* ExynosCamera1DualParameters::getDualMetadata(void)
{
    Mutex::Autolock lock(m_lock);

    return (&m_dualmetadata);
}
#endif /* SAMSUNG_DUAL_SOLUTION */

void ExynosCamera1DualParameters::setParamExifInfo(int32_t cameraId, uint32_t iso, uint32_t exposureTime)
{
    Mutex::Autolock lock(m_lock);

    if (m_params[cameraId] == NULL) {
        CLOGE2(CAM_ID "m_params[%d] is NULL!!!", cameraId, m_name, cameraId);
        return;
    }

    m_params[cameraId]->set("exif_iso", iso);
    m_params[cameraId]->set("exif_exptime", exposureTime);

    return;
}

void ExynosCamera1DualParameters::lockDualCameraSyncType(int32_t cameraId, bool lockFlag)
{
    Mutex::Autolock lock(m_lock);

    CLOGI2(CAM_ID "lockDualCameraSyncType %d -> %d", cameraId, m_name, m_lockDualCameraSyncType, lockFlag);

    m_lockDualCameraSyncType = lockFlag;
}

void ExynosCamera1DualParameters::clearDualCameraInformation(int32_t cameraId)
{
    Mutex::Autolock lock(m_lock);

    CLOGI2(CAM_ID "clearAllCameraInformation", cameraId, m_name);

    m_LLSValue = 0;
    m_lockDualCameraSyncType = false;
    m_flagForceSwitchingOnly = false;
#ifdef SAMSUNG_DUAL_SOLUTION
    m_wideFocusState = -1;
    m_teleFocusState = -1;
    m_wideFocusDone = false;
    m_teleFocusDone = false;
    m_dualLux = 0;
    m_dualFocusDistance = 0;
    m_forceWideCond = false;
    m_forceWideOnDelay = 0;
    m_forceWideOffDelay = 0;
    m_checkForceWideCount = 0;
    m_checkForceWideCond = 0;
    m_isFusionCapture = false;
#endif

    for (int i = 0; i < CAMERA_ID_MAX; i++) {
        m_camera[i] = NULL;
        m_dualNotifyCallbackFunc[i] = NULL;
        m_flagFinishStartPreview[i] = false;
    }
}

#ifdef DUAL_SMOOTH_TRANSITION_LAUNCH
void ExynosCamera1DualParameters::finishStartPreview(int32_t cameraId, int32_t targetCameraId)
{
    if (targetCameraId < 0 || targetCameraId >= CAMERA_ID_MAX)
        return;

    Mutex::Autolock lock(m_lock);

    CLOGI2(CAM_ID "targetCameraId %d -> true", cameraId, m_name,
            m_flagFinishStartPreview[targetCameraId]);

    m_flagFinishStartPreview[targetCameraId] = true;
}

bool ExynosCamera1DualParameters::getFlagFinishStartPreview(int32_t cameraId, int32_t targetCameraId)
{
    if (targetCameraId < 0 || targetCameraId >= CAMERA_ID_MAX)
        return false;

    Mutex::Autolock lock(m_lock);

    return m_flagFinishStartPreview[targetCameraId];
}
#endif

void ExynosCamera1DualParameters::registerCameraParam(int32_t cameraId, CameraParameters* param)
{
    Mutex::Autolock lock(m_lock);

    CLOGI2(CAM_ID "m_params[%d]: %p", cameraId, m_name, cameraId, param);

    if (m_params[cameraId] != NULL) {
        CLOGE2(CAM_ID "m_params[%d] is not null!!", cameraId, m_name, cameraId);
    }

    m_params[cameraId] = param;
}

void ExynosCamera1DualParameters::setForceSwitchingOnly(int32_t cameraId, bool forceSwitchingOnly)
{
    Mutex::Autolock lock(m_lock);

    CLOGI2(CAM_ID "forceSwitchingOnly %d -> %d", cameraId, m_name, m_flagForceSwitchingOnly, forceSwitchingOnly);

    m_flagForceSwitchingOnly = forceSwitchingOnly;
}

int ExynosCamera1DualParameters::getFlagForceSwitchingOnly(int32_t cameraId)
{
    Mutex::Autolock lock(m_lock);

    return m_flagForceSwitchingOnly;
}

/* to notify */
void ExynosCamera1DualParameters::registerNotifyCallback(int32_t cameraId, ExynosCamera *camera, dual_notify_func_t notifyFunc)
{
    Mutex::Autolock lock(m_lock);

    CLOGV2(CAM_ID "", cameraId, m_name);

    if (m_camera[cameraId] != NULL) {
        CLOGE2(CAM_ID "m_camera[%d] is not null!!", cameraId, m_name, cameraId);
    }

    if (m_dualNotifyCallbackFunc[cameraId] != NULL) {
        CLOGE2(CAM_ID "m_dualNotifyCallbackFunc[%d] is not null!!", cameraId, m_name, cameraId);
    }

    m_camera[cameraId] = camera;
    m_dualNotifyCallbackFunc[cameraId] = notifyFunc;
    m_flagDualNotifyThreadStop = false;
}

void ExynosCamera1DualParameters::unregisterNotifyCallback(int32_t cameraId)
{
    Mutex::Autolock lock(m_lock);
    dual_camera_notify_parcel_t *parcel = NULL;

    CLOGV2(CAM_ID "", cameraId, m_name);

    if (m_camera[cameraId] == NULL) {
        CLOGW2(CAM_ID "m_camera[%d] is already null!!", cameraId, m_name, cameraId);
    }

    if (m_dualNotifyCallbackFunc[cameraId] == NULL) {
        CLOGW2(CAM_ID "m_dualNotifyCallbackFunc[%d] is already null!!", cameraId, m_name, cameraId);
    }

    m_flagDualNotifyThreadStop = true;
    m_dualNotifyThread->stop();
    m_dualNotifyQ.sendCmd(WAKE_UP);
    m_dualNotifyThread->requestExitAndWait();

    /* flush notifyQ */
    while (m_dualNotifyQ.getSizeOfProcessQ()) {
        if (m_dualNotifyQ.popProcessQ(&parcel) == OK) {
            if (parcel != NULL)
                delete parcel;
        } else {
            break;
        }
    }

    m_camera[cameraId] = NULL;
    m_dualNotifyCallbackFunc[cameraId] = NULL;
}

status_t ExynosCamera1DualParameters::notify(int32_t cameraId, enum dual_camera_notify_type notifyType,
                                             bool useThread, int32_t arg1, int32_t arg2, void* arg3)
{
    CLOGI2(CAM_ID "notifyType(%d), arg1(%d), arg2(%d)", cameraId, m_name, notifyType, arg1, arg2);

    if (m_camera[cameraId] == NULL) {
        CLOGE2(CAM_ID "m_camera[%d] is null!!", cameraId, m_name, cameraId);
        return INVALID_OPERATION;
    }

    if (m_dualNotifyCallbackFunc[cameraId] == NULL) {
        CLOGE2(CAM_ID "m_dualNotifyCallbackFunc[%d] is null!!", cameraId, m_name, cameraId);
        return INVALID_OPERATION;
    }

    if (useThread) {
        Mutex::Autolock lock(m_lock);

        dual_camera_notify_parcel_t *parcel = new dual_camera_notify_parcel_t;
        parcel->notifyType = notifyType;
        parcel->arg1 = arg1;
        parcel->arg2 = arg2;
        parcel->arg3 = arg3;

        m_dualNotifyQ.pushProcessQ(&parcel);
    } else {
        uint32_t targetCameraId = arg1;
        int32_t value = arg2;

        if (targetCameraId >= CAMERA_ID_MAX) {
            CLOGE2("invalid target cameraId(%d) %d", arg1, arg2);
        } else if (m_camera[targetCameraId] == NULL) {
            CLOGW2("not registerd camera cameraId(%d) %d", arg1, arg2);
        } else if (m_dualNotifyCallbackFunc[targetCameraId] == NULL) {
            CLOGW2("not registerd notifyCallback cameraId(%d) %d", arg1, arg2);
        } else {
            /* notify callback */
            dual_notify_func_t notifyFunc = m_dualNotifyCallbackFunc[targetCameraId];;
            (m_camera[targetCameraId]->*notifyFunc)(notifyType, arg1, arg2, arg3);
        }
    }

    return NO_ERROR;
}

/* protected */
bool ExynosCamera1DualParameters::m_dualNotifyThreadFunc(void)
{
    status_t ret;
    dual_camera_notify_parcel_t *parcel = NULL;

    if (m_flagDualNotifyThreadStop)
        return false;

    ret = m_dualNotifyQ.waitAndPopProcessQ(&parcel);
    if (m_flagDualNotifyThreadStop) {
        if (ret == OK && parcel != NULL)
            delete parcel;

        return false;
    } else {
        if (ret < 0) {
            /* TODO: We need to make timeout duration depends on FPS */
            if (ret == TIMED_OUT) {
                CLOGV2("wait timeout");
            } else {
                CLOGE2("wait and pop fail, ret(%d)", ret);
            }
            return true;
        }
    }

    if (parcel != NULL) {
        CLOGI2("notifyType(%d), arg1(%d), arg2(%d)", parcel->notifyType, parcel->arg1, parcel->arg2);

        switch (parcel->notifyType) {
        case DUAL_CAMERA_NOTIFY_WAKE_UP:
        case DUAL_CAMERA_NOTIFY_WAKE_UP_FINISH:
            {
                uint32_t targetCameraId = parcel->arg1;
                int32_t value = parcel->arg2;

                if (targetCameraId >= CAMERA_ID_MAX) {
                    CLOGE2("invalid target cameraId(%d) %d", parcel->arg1, parcel->arg2);
                } else if (m_camera[targetCameraId] == NULL) {
                    CLOGW2("not registerd camera cameraId(%d) %d", parcel->arg1, parcel->arg2);
                } else if (m_dualNotifyCallbackFunc[targetCameraId] == NULL) {
                    CLOGW2("not registerd notifyCallback cameraId(%d) %d", parcel->arg1, parcel->arg2);
                } else {
                    /* notify callback */
                    dual_notify_func_t notifyFunc = m_dualNotifyCallbackFunc[targetCameraId];;
                    (m_camera[targetCameraId]->*notifyFunc)(parcel->notifyType, parcel->arg1, parcel->arg2, parcel->arg3);
                }
            }
            break;
        default:
            break;
        }
        delete parcel;
    }

    if (m_flagDualNotifyThreadStop)
        return false;

    return true;
}
#endif //USE_DUAL_CAMERA

ExynosCamera1Parameters::ExynosCamera1Parameters(int cameraId, __unused bool flagCompanion, int halVersion, bool flagDummy)
{
    m_cameraId = cameraId;
    m_halVersion = halVersion;
    m_flagDummy = flagDummy;

    if (m_cameraId == CAMERA_ID_SECURE) {
        m_scenario = SCENARIO_SECURE;
        m_cameraId = CAMERA_ID_FRONT;
    } else {
        m_scenario = SCENARIO_NORMAL;
    }

    const char *myName = NULL;
    if (m_cameraId == CAMERA_ID_BACK || m_cameraId == CAMERA_ID_BACK_1) {
        if (m_flagDummy == true)
            myName = "ParametersBackDummy";
        else
            myName = "ParametersBack";
    } else if (m_cameraId == CAMERA_ID_FRONT || m_cameraId == CAMERA_ID_FRONT_1) {
        if (m_flagDummy == true)
            myName = "ParametersFrontDummy";
        else
            myName = "ParametersFront";
    }

    if (myName != NULL) {
        strncpy(m_name, myName,  EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    }

    m_staticInfo = createExynosCamera1SensorInfo(cameraId);
    m_useSizeTable = (m_staticInfo->sizeTableSupport) ? USE_CAMERA_SIZE_TABLE : false;
    m_useAdaptiveCSCRecording = (cameraId == CAMERA_ID_BACK) ? USE_ADAPTIVE_CSC_RECORDING : USE_ADAPTIVE_CSC_RECORDING_FRONT;

    m_dummyParameters = NULL;
    if (m_flagDummy == false)
        m_dummyParameters = new ExynosCamera1Parameters(cameraId, flagCompanion, halVersion, true);

    m_exynosconfig = NULL;
    m_activityControl = new ExynosCameraActivityControl(m_cameraId);

    memset(&m_cameraInfo, 0, sizeof(struct exynos_camera_info));
    memset(&m_exifInfo, 0, sizeof(m_exifInfo));

#ifdef SAMSUNG_DUAL_SOLUTION
    m_fusionPreviewWrapper = ExynosCameraSingleton<ExynosCameraFusionPreviewWrapper>::getInstance();
    m_fusionCaptureWrapper = ExynosCameraSingleton<ExynosCameraFusionCaptureWrapper>::getInstance();
#endif

    m_initMetadata();

    m_setExifFixedAttribute();

    m_exynosconfig = new ExynosConfigInfo();
    memset((void *)m_exynosconfig, 0x00, sizeof(struct ExynosConfigInfo));

#ifdef USE_MCSC_FOR_FD
    memset(&m_fdConfig, 0x00, sizeof(ExynosFDConfig_t));
    m_fdConfig.flagFdEnable = false;
#endif

    // CAUTION!! : Initial values must be prior to setDefaultParameter() function
    // Initial Values : START
    m_use_companion = false;
    m_IsThumbnailCallbackOn = false;
    m_fastFpsMode = 0;
    m_previewRunning = false;
    m_previewSizeChanged = false;
    m_pictureRunning = false;
    m_recordingRunning = false;
    m_flagRestartPreviewChecked = false;
    m_flagRestartPreview = false;
    m_reallocBuffer = false;
    m_setFocusmodeSetting = false;
    m_flagMeteringRegionChanged = false;
    m_flagCheckDualMode = false;
#ifdef USE_DUAL_CAMERA
    m_flagCheckDualCameraMode = false;
    m_flagDualCameraPreviewFusionMode = false;
    m_flagDualCameraCaptureFusionMode = false;
    m_dualStandbyMode = DUAL_STANDBY_MODE_INACTIVE;
    m_backupDualPostStandbyMinFps = 0;
    m_backupDualPostStandbyMaxFps = 0;
    m_transitionCount = 0;
    m_smoothTransitionCount = 0;
    m_wakeupFinishCount = 0;
    m_dualParameters = ExynosCameraSingleton<ExynosCamera1DualParameters>::getInstance();
    m_dualStableFromStandby = false;
#endif
    m_flagHWVDisMode = false;
    m_flagVideoStabilization = false;
    m_flag3dnrMode = false;
    m_reprocessingBayerMode = REPROCESSING_BAYER_MODE_NONE;
    m_flagCheckRecordingHint = false;
    m_zoomWithScaler = false;
    m_currentZoomRatio = 0.0f;
    m_ionClient = -1;

    m_useDynamicBayer = (cameraId == CAMERA_ID_BACK) ? USE_DYNAMIC_BAYER : USE_DYNAMIC_BAYER_FRONT;
    m_useDynamicBayerVideoSnapShot =
        (cameraId == CAMERA_ID_BACK) ? USE_DYNAMIC_BAYER_VIDEO_SNAP_SHOT : USE_DYNAMIC_BAYER_VIDEO_SNAP_SHOT_FRONT;
    m_useDynamicScc = (cameraId == CAMERA_ID_BACK) ? USE_DYNAMIC_SCC_REAR : USE_DYNAMIC_SCC_FRONT;
    m_useFastenAeStable = false;
    m_fastenAeStableOn = false;
    m_usePureBayerReprocessing =
        (cameraId == CAMERA_ID_BACK) ? USE_PURE_BAYER_REPROCESSING : USE_PURE_BAYER_REPROCESSING_FRONT;
    m_enabledMsgType = 0;
    m_previewBufferCount = NUM_PREVIEW_BUFFERS;
    m_dvfsLock = false;
    m_zoom_activated = false;
    m_flagOutPutFormatNV21Enable = false;
    m_exposureTimeCapture = 0;
    m_flagHWFCEnable = isUseHWFC();

    m_isSwitchSensor = false;

    vendorSpecificConstructor(cameraId, flagCompanion);
    // Initial Values : END

    setDefaultCameraInfo();

    if (m_flagDummy == false)
        setDefaultParameter();
}

ExynosCamera1Parameters::~ExynosCamera1Parameters()
{
    if (m_staticInfo != NULL) {
        delete m_staticInfo;
        m_staticInfo = NULL;
    }

    if (m_activityControl != NULL) {
        delete m_activityControl;
        m_activityControl = NULL;
    }

    for (int i = 0; i < mDebugInfo.num_of_appmarker; i++) {
        if (mDebugInfo.debugData[mDebugInfo.idx[i][0]])
            delete[] mDebugInfo.debugData[mDebugInfo.idx[i][0]];
        mDebugInfo.debugData[mDebugInfo.idx[i][0]] = NULL;
        mDebugInfo.debugSize[mDebugInfo.idx[i][0]] = 0;
    }

    for (int i = 0; i < mDebugInfo2.num_of_appmarker; i++) {
        if (mDebugInfo2.debugData[mDebugInfo2.idx[i][0]])
            delete[] mDebugInfo2.debugData[mDebugInfo2.idx[i][0]];
        mDebugInfo2.debugData[mDebugInfo2.idx[i][0]] = NULL;
        mDebugInfo2.debugSize[mDebugInfo2.idx[i][0]] = 0;
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
#ifdef SAMSUNG_OT
    if (m_objectTrackingArea != NULL)
        delete[] m_objectTrackingArea;
    if (m_objectTrackingWeight != NULL)
        delete[] m_objectTrackingWeight;
#endif

    if (m_flagDummy == false && m_dummyParameters != NULL) {
        delete m_dummyParameters;
        m_dummyParameters = NULL;
    }

#ifdef USE_CP_FUSION_LIB
    if (m_fusionCalData) {
        delete m_fusionCalData;
        m_fusionCalData = NULL;
    }
#endif
}

int ExynosCamera1Parameters::getCameraId(void)
{
    return m_cameraId;
}

CameraParameters ExynosCamera1Parameters::getParameters() const
{
    CLOGI();

    return m_params;
}

void ExynosCamera1Parameters::setDefaultCameraInfo(void)
{
    CLOGV();
    m_setHwSensorSize(m_staticInfo->maxSensorW, m_staticInfo->maxSensorH);
    m_setHwPreviewSize(m_staticInfo->maxPreviewW, m_staticInfo->maxPreviewH);
    m_setHwPictureSize(m_staticInfo->maxPictureW, m_staticInfo->maxPictureH);

    /* Initalize BNS scale ratio, step:500, ex)1500->x1.5 scale down */
    m_setBnsScaleRatio(1000);
    /* Initalize Binning scale ratio */
    m_setBinningScaleRatio(1000);
    /* Set Default VideoSize to FHD */
    m_setVideoSize(1920,1080);
}

bool ExynosCamera1Parameters::checkRestartPreview(const CameraParameters& params)
{
    bool restartFlag = false;

    *m_dummyParameters = *this;
    m_dummyParameters->setRestartParameters(params);
    restartFlag = m_dummyParameters->getRestartPreview();
    return restartFlag;
}

status_t ExynosCamera1Parameters::checkRecordingHint(const CameraParameters& params)
{
    /* recording hint */
    bool recordingHint = false;
    const char *newRecordingHint = params.get(CameraParameters::KEY_RECORDING_HINT);
    int newEffectRecordingHint = params.getInt("effectrecording-hint");
    bool effectRecordingHint = (newEffectRecordingHint == 1) ? true : false;

    if (newRecordingHint != NULL) {
        CLOGD("DEBUG(%s):newRecordingHint : %s, newEffectRecordingHint : %d",
                "setParameters", newRecordingHint, newEffectRecordingHint);

        recordingHint = (strcmp(newRecordingHint, "true") == 0) ? true : false;

        m_setRecordingHint(recordingHint | effectRecordingHint);

        m_params.set(CameraParameters::KEY_RECORDING_HINT, newRecordingHint);
    } else {
        /* to confirm that recordingHint value is checked up (whatever value is) */
        m_setRecordingHint(m_cameraInfo.recordingHint | effectRecordingHint);

        recordingHint = getRecordingHint();
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setRecordingHint(bool hint)
{
    ExynosCameraActivityAutofocus *autoFocusMgr = m_activityControl->getAutoFocusMgr();
    //ExynosCameraActivityFlash *flashMgr = m_activityControl->getFlashMgr();

    m_cameraInfo.recordingHint = hint;

#ifdef SAMSUNG_DUAL_SOLUTION
    m_dualParameters->setRecordingHint(m_cameraId, hint);
#endif

    if (hint) {
        setMetaVideoMode(&m_metadata, AA_VIDEOMODE_ON);
    } else if (!hint && !getDualRecordingHint() && !getEffectRecordingHint()) {
        setMetaVideoMode(&m_metadata, AA_VIDEOMODE_OFF);
    }

    if (m_flagDummy == false) {
        autoFocusMgr->setRecordingHint(hint);
        /* recordinghint flag use to control m_isNeedCaptureFlash in ExynosCameraActivityFlash
        So, flag only set for recording running
        */
        //flashMgr->setRecordingHint(hint);
    }

    /* RecordingHint is confirmed */
    m_flagCheckRecordingHint = true;
}

bool ExynosCamera1Parameters::getRecordingHint(void)
{
    /*
     * Before setParameters, we cannot know recordingHint is valid or not
     * So, check and make assert for fast debugging
     */
    if (m_flagCheckRecordingHint == false)
        android_printAssert(NULL, LOG_TAG, "Cannot call getRecordingHint befor setRecordingHint, assert!!!!");

    return m_cameraInfo.recordingHint;
}

void ExynosCamera1Parameters::setDualMode(bool dual)
{
    m_cameraInfo.dualMode = dual;
    /* dualMode is confirmed */
    m_flagCheckDualMode = true;
}

bool ExynosCamera1Parameters::getDualMode(void)
{
    /*
    * Before setParameters, we cannot know dualMode is valid or not
    * So, check and make assert for fast debugging
    */
    if (m_flagCheckDualMode == false)
        android_printAssert(NULL, LOG_TAG, "Cannot call getDualMode befor checkDualMode, assert!!!!");

    return m_cameraInfo.dualMode;
}

status_t ExynosCamera1Parameters::checkDualRecordingHint(const CameraParameters& params)
{
    /* dual recording hint */
    bool flagDualRecordingHint = false;
    int newDualRecordingHint = params.getInt("dualrecording-hint");

    if (newDualRecordingHint == 1) {
        CLOGD("[setParameters]newDualRecordingHint : %d", newDualRecordingHint);
        flagDualRecordingHint = true;
    }

    m_setDualRecordingHint(flagDualRecordingHint);
    m_params.set("dualrecording-hint", newDualRecordingHint);

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setDualRecordingHint(bool hint)
{
    m_cameraInfo.dualRecordingHint = hint;

    if (hint) {
        setMetaVideoMode(&m_metadata, AA_VIDEOMODE_ON);
    } else if (!hint && !getRecordingHint() && !getEffectRecordingHint()) {
        setMetaVideoMode(&m_metadata, AA_VIDEOMODE_OFF);
    }
}

bool ExynosCamera1Parameters::getDualRecordingHint(void)
{
    return m_cameraInfo.dualRecordingHint;
}

status_t ExynosCamera1Parameters::checkEffectHint(const CameraParameters& params)
{
    /* effect hint */
    bool flagEffectHint = false;
    int newEffectHint = params.getInt("effect_hint");

    if (newEffectHint < 0)
        return NO_ERROR;

    if (newEffectHint == 1) {
        CLOGD("[setParameters]newEffectHint : %d", newEffectHint);
        flagEffectHint = true;
    }

    m_setEffectHint(newEffectHint);
    m_params.set("effect_hint", newEffectHint);

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setEffectHint(bool hint)
{
    m_cameraInfo.effectHint = hint;
}

bool ExynosCamera1Parameters::getEffectHint(void)
{
    return m_cameraInfo.effectHint;
}

status_t ExynosCamera1Parameters::checkEffectRecordingHint(const CameraParameters& params)
{
    /* Effect recording hint */
    bool flagEffectRecordingHint = false;
    int newEffectRecordingHint = params.getInt("effectrecording-hint");
    int curEffectRecordingHint = m_params.getInt("effectrecording-hint");

    if (newEffectRecordingHint < 0) {
        CLOGV("[setParameters]Invalid newEffectRecordingHint");
        return NO_ERROR;
    }

    if (newEffectRecordingHint != curEffectRecordingHint) {
        CLOGD("[setParameters]newEffectRecordingHint : %d", newEffectRecordingHint);
        if (newEffectRecordingHint == 1)
            flagEffectRecordingHint = true;
        m_setEffectRecordingHint(flagEffectRecordingHint);
        m_params.set("effectrecording-hint", newEffectRecordingHint);
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setEffectRecordingHint(bool hint)
{
    ExynosCameraActivityAutofocus *autoFocusMgr = m_activityControl->getAutoFocusMgr();
    ExynosCameraActivityFlash *flashMgr = m_activityControl->getFlashMgr();

    m_cameraInfo.effectRecordingHint = hint;

    if (hint) {
        setMetaVideoMode(&m_metadata, AA_VIDEOMODE_ON);
    } else if (!hint && !getRecordingHint() && !getDualRecordingHint()) {
        setMetaVideoMode(&m_metadata, AA_VIDEOMODE_OFF);
    }

    if (m_flagDummy == false) {
        autoFocusMgr->setRecordingHint(hint);
        flashMgr->setRecordingHint(hint);
    }
}

bool ExynosCamera1Parameters::getEffectRecordingHint(void)
{
    return m_cameraInfo.effectRecordingHint;
}

status_t ExynosCamera1Parameters::checkPreviewFps(const CameraParameters& params)
{
    int ret = 0;

    ret = checkPreviewFpsRange(params);
    if (ret == BAD_VALUE) {
        CLOGE("[setParameters]Inavalid value");
        return ret;
    } else if (ret != NO_ERROR) {
        ret = checkPreviewFrameRate(params);
    }

    return ret;
}

status_t ExynosCamera1Parameters::checkPreviewFpsRange(const CameraParameters& params)
{
    int newMinFps = 0;
    int newMaxFps = 0;
    int newHwMinFps = 0;
    int newHwMaxFps = 0;
    int newFrameRate = params.getPreviewFrameRate();
    uint32_t curMinFps = 0;
    uint32_t curMaxFps = 0;

    params.getPreviewFpsRange(&newMinFps, &newMaxFps);
    if (newMinFps <= 0 || newMaxFps <= 0 || newMinFps > newMaxFps) {
        CLOGE("PreviewFpsRange is invalid, newMin(%d), newMax(%d)", newMinFps, newMaxFps);
        return BAD_VALUE;
    }

    newHwMinFps = newMinFps;
    newHwMaxFps = newMaxFps;

    if (m_adjustPreviewFpsRange(newHwMinFps, newHwMaxFps) != NO_ERROR) {
        CLOGE("Fail to adjust preview fps range");
        return INVALID_OPERATION;
    }

    newMinFps = newMinFps / 1000;
    newMaxFps = newMaxFps / 1000;
    newHwMinFps = newHwMinFps / 1000;
    newHwMaxFps = newHwMaxFps / 1000;
    if (FRAME_RATE_MAX < newHwMaxFps || newHwMaxFps < newHwMinFps) {
        CLOGE("PreviewFpsRange is out of bound");
        return INVALID_OPERATION;
    }

    getPreviewFpsRange(&curMinFps, &curMaxFps);
    CLOGI("[checkPreviewFpsRange]curFpsRange[Min=%d, Max=%d], newHWFpsRange[Min=%d, Max=%d], newFpsRange[Min=%d, Max=%d], [curFrameRate=%d]",
        curMinFps, curMaxFps, newHwMinFps, newHwMaxFps,
        newMinFps, newMaxFps, m_params.getPreviewFrameRate());

    if (curMinFps != (uint32_t)newMinFps || curMaxFps != (uint32_t)newMaxFps
        || curMinFps != (uint32_t)newHwMinFps || curMaxFps != (uint32_t)newHwMaxFps) {
        m_setPreviewFpsRange((uint32_t)newHwMinFps, (uint32_t)newHwMaxFps);

        char newFpsRange[256];
        memset (newFpsRange, 0, 256);
        snprintf(newFpsRange, 256, "%d,%d", newMinFps * 1000, newMaxFps * 1000);

        CLOGI("set PreviewFpsRange(%s). PreviewFrameRate is (curFps=%d->newFps=%d)", newFpsRange, m_params.getPreviewFrameRate(), newHwMaxFps);
        m_params.set(CameraParameters::KEY_PREVIEW_FPS_RANGE, newFpsRange);
        m_params.setPreviewFrameRate(newMaxFps);
    }

    getPreviewFpsRange(&curMinFps, &curMaxFps);

    if (m_flagDummy == false) {
        m_activityControl->setFpsValue(curMinFps);
    }

    /* For backword competivity */
    m_params.setPreviewFrameRate(newFrameRate);

    return NO_ERROR;
}

status_t ExynosCamera1Parameters::m_adjustPreviewFpsRange(int &newMinFps, int &newMaxFps)
{
    bool flagSpecialMode = false;
    int curSceneMode = 0;
    int curShotMode = 0;
    /* only used for printing log */
    int orgMinFps = newMinFps;
    int orgMaxFps = newMaxFps;

    if (getDualMode() == true
#ifdef USE_DUAL_CAMERA
            && (getDualCameraMode() == false)
#endif
       ) {
        flagSpecialMode = true;

        /* when dual mode, fps is limited by 24fps */
        if (24000 < newMaxFps)
            newMaxFps = 24000;

        /* set fixed fps. */
        newMinFps = newMaxFps;
        if (orgMinFps != newMinFps ||
                orgMaxFps != newMaxFps) {
            CLOGD("dualMode(true), newMaxFps=%d->%d", orgMaxFps, newMaxFps);
        }
    }

    if (getDualRecordingHint() == true) {
        flagSpecialMode = true;

        /* when dual recording mode, fps is limited by 24fps */
        if (24000 < newMaxFps)
            newMaxFps = 24000;

        /* set fixed fps. */
        newMinFps = newMaxFps;
        if (orgMinFps != newMinFps ||
                orgMaxFps != newMaxFps) {
            CLOGD("dualRecordingHint(true), newMinFps=%d->%d,newMaxFps=%d->%d",
                    orgMinFps, newMinFps, orgMaxFps, newMaxFps);
        }
    }

    if (getEffectHint() == true) {
        flagSpecialMode = true;
#if 0   /* Don't use to set fixed fps in the hal side. */
        /* when effect mode, fps is limited by 24fps */
        if (24000 < newMaxFps)
            newMaxFps = 24000;

        /* set fixed fps due to GPU preformance. */
        newMinFps = newMaxFps;
#endif
        if (orgMinFps != newMinFps ||
                orgMaxFps != newMaxFps) {
            CLOGD("effectHint(true), newMaxFps=%d->%d", orgMaxFps, newMaxFps);
        }
    }

    if (getIntelligentMode() == 1) {
        flagSpecialMode = true;
        CLOGD("DEBUG(%s[%d]):intelligent_mode(true), newMaxFps=%d", __FUNCTION__, __LINE__, newMaxFps);
    }

    if (getRecordingHint() == true) {
        flagSpecialMode = true;
        if (orgMinFps != newMinFps ||
                orgMaxFps != newMaxFps) {
            CLOGD("RecordingHint(true),newMinFps=%d->%d,newMaxFps=%d->%d",
                    orgMinFps, newMinFps, orgMaxFps, newMaxFps);
        }
    }

#ifdef USE_LIMITATION_FOR_THIRD_PARTY
    if (getSamsungCamera() == false && flagSpecialMode != true) {
        switch (m_fpsProperty) {
        case 30000:
        case 15000:
            CLOGI("INFO(%s): set to %d fps depends on fps property", __FUNCTION__, m_fpsProperty/1000);
            newMinFps = m_fpsProperty;
            newMaxFps = m_fpsProperty;
            flagSpecialMode = true;
            break;
        default:
            /* Don't use to set fixed fps in the hal side. */
            break;
        }
    }
#endif

    if (flagSpecialMode == true) {
        if (orgMinFps != newMinFps ||
                orgMaxFps != newMaxFps) {
            CLOGD("special mode enabled, newMaxFps=%d->%d", orgMaxFps, newMaxFps);
        }
        goto done;
    }

    curSceneMode = getSceneMode();
    switch (curSceneMode) {
    case SCENE_MODE_ACTION:
        if (getHighSpeedRecording() == true){
            newMinFps = newMaxFps;
        } else {
            newMinFps = 30000;
            newMaxFps = 30000;
        }
        break;
    case SCENE_MODE_PORTRAIT:
    case SCENE_MODE_LANDSCAPE:
        if (getHighSpeedRecording() == true){
            newMinFps = newMaxFps / 2;
        } else {
            newMinFps = 15000;
            newMaxFps = 30000;
        }
        break;
    case SCENE_MODE_NIGHT:
        /* for Front MMS mode FPS */
        if (getCameraId() == CAMERA_ID_FRONT && getRecordingHint() == true)
            break;

        if (getHighSpeedRecording() == true){
            newMinFps = newMaxFps / 4;
        } else {
            newMinFps = 8000;
            newMaxFps = 30000;
        }
        break;
    case SCENE_MODE_NIGHT_PORTRAIT:
    case SCENE_MODE_THEATRE:
    case SCENE_MODE_BEACH:
    case SCENE_MODE_SNOW:
    case SCENE_MODE_SUNSET:
    case SCENE_MODE_STEADYPHOTO:
    case SCENE_MODE_FIREWORKS:
    case SCENE_MODE_SPORTS:
    case SCENE_MODE_PARTY:
    case SCENE_MODE_CANDLELIGHT:
        if (getHighSpeedRecording() == true){
            newMinFps = newMaxFps / 2;
        } else {
            newMinFps = 15000;
            newMaxFps = 30000;
        }
        break;
    default:
        break;
    }

    curShotMode = getShotMode();
    switch (curShotMode) {
    case SHOT_MODE_DRAMA:
    case SHOT_MODE_3DTOUR:
    case SHOT_MODE_3D_PANORAMA:
    case SHOT_MODE_LIGHT_TRACE:
        newMinFps = 30000;
        newMaxFps = 30000;
        break;
    default:
        break;
    }

done:
    if (newMinFps != newMaxFps) {
        if (m_getSupportedVariableFpsList(newMinFps, newMaxFps, &newMinFps, &newMaxFps) == false)
            newMinFps = newMaxFps / 2;
    }

#ifdef USE_DUAL_CAMERA
    if (getDualCameraMode() == true) {
        flagSpecialMode = true;
#ifdef DUAL_ENABLE_ASYNC_FPS
        /* post standby mode */
        m_backupDualPostStandbyMinFps = newMinFps;
        m_backupDualPostStandbyMaxFps = newMaxFps;

        if (DUAL_POSTSTANDBY_FPS > 0) {
            /* post standby mode */
            if (getDualStandbyMode() == DUAL_STANDBY_MODE_ACTIVE_IN_POST) {
                newMinFps = DUAL_POSTSTANDBY_FPS;
                newMaxFps = DUAL_POSTSTANDBY_FPS;
            }
        }
#endif
        if (m_backupDualPostStandbyMinFps != newMinFps ||
                m_backupDualPostStandbyMaxFps != newMaxFps) {
            CLOGD("dualCameraMode(true), newMinFps=%d->%d, newMaxFps=%d->%d, backupFps=%d/%d, PostStandby=%d",
                    orgMinFps, newMinFps, orgMaxFps, newMaxFps,
                    m_backupDualPostStandbyMinFps,
                    m_backupDualPostStandbyMaxFps,
                    getDualStandbyMode());
        }
    }
#endif

    return NO_ERROR;
}

void ExynosCamera1Parameters::updatePreviewFpsRange(void)
{
    uint32_t curMinFps = 0;
    uint32_t curMaxFps = 0;
    int newMinFps = 0;
    int newMaxFps = 0;

    getPreviewFpsRange(&curMinFps, &curMaxFps);
    newMinFps = curMinFps * 1000;
    newMaxFps = curMaxFps * 1000;

    if (m_adjustPreviewFpsRange(newMinFps, newMaxFps) != NO_ERROR) {
        CLOGE("Fils to adjust preview fps range");
        return;
    }

    newMinFps = newMinFps / 1000;
    newMaxFps = newMaxFps / 1000;

    if (curMinFps != (uint32_t)newMinFps || curMaxFps != (uint32_t)newMaxFps) {
        m_setPreviewFpsRange((uint32_t)newMinFps, (uint32_t)newMaxFps);
    }
}

status_t ExynosCamera1Parameters::checkPreviewFrameRate(const CameraParameters& params)
{
    int newFrameRate = params.getPreviewFrameRate();
    int curFrameRate = m_params.getPreviewFrameRate();
    int newMinFps = 0;
    int newMaxFps = 0;
    int tempFps = 0;

    if (newFrameRate < 0) {
        return BAD_VALUE;
    }
    CLOGD("curFrameRate=%d, newFrameRate=%d", curFrameRate, newFrameRate);
    if (newFrameRate != curFrameRate) {
        tempFps = newFrameRate * 1000;

        if (m_getSupportedVariableFpsList(tempFps / 2, tempFps, &newMinFps, &newMaxFps) == false) {
            newMinFps = tempFps / 2;
            newMaxFps = tempFps;
        }

        char newFpsRange[256];
        memset (newFpsRange, 0, 256);
        snprintf(newFpsRange, 256, "%d,%d", newMinFps, newMaxFps);
        m_params.set(CameraParameters::KEY_PREVIEW_FPS_RANGE, newFpsRange);

        if (checkPreviewFpsRange(m_params) == true) {
            m_params.setPreviewFrameRate(newFrameRate);
            CLOGD("setPreviewFrameRate(newFrameRate=%d)", newFrameRate);
        }
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setPreviewFpsRange(uint32_t min, uint32_t max)
{
    setMetaCtlAeTargetFpsRange(&m_metadata, min, max);
    setMetaCtlSensorFrameDuration(&m_metadata, (uint64_t)((1000 * 1000 * 1000) / (uint64_t)max));

    CLOGI("fps min(%d) max(%d)", min, max);
}

void ExynosCamera1Parameters::getPreviewFpsRange(uint32_t *min, uint32_t *max)
{
    /* ex) min = 15 , max = 30 */
    getMetaCtlAeTargetFpsRange(&m_metadata, min, max);
}

bool ExynosCamera1Parameters::m_getSupportedVariableFpsList(int min, int max, int *newMin, int *newMax)
{
    int (*sizeList)[2];

    if (getCameraId() == CAMERA_ID_BACK) {
        /* Try to find exactly same in REAR LIST*/
        sizeList = m_staticInfo->rearFPSList;
        for (int i = 0; i < m_staticInfo->rearFPSListMax; i++) {
            if (sizeList[i][1] == max && sizeList[i][0] == min) {
                *newMin = sizeList[i][0];
                *newMax = sizeList[i][1];

                return true;
            }
        }
        /* Try to find exactly same in HIDDEN REAR LIST*/
        sizeList = m_staticInfo->hiddenRearFPSList;
        for (int i = 0; i < m_staticInfo->hiddenRearFPSListMax; i++) {
            if (sizeList[i][1] == max && sizeList[i][0] == min) {
                *newMin = sizeList[i][0];
                *newMax = sizeList[i][1];

                return true;
            }
        }
        /* Try to find similar fps in REAR LIST*/
        sizeList = m_staticInfo->rearFPSList;
        for (int i = 0; i < m_staticInfo->rearFPSListMax; i++) {
            if (max <= sizeList[i][1] && sizeList[i][0] <= min) {
                if(sizeList[i][1] == sizeList[i][0])
                    continue;

                *newMin = sizeList[i][0];
                *newMax = sizeList[i][1];

                CLOGW("calibrate new fps(%d/%d -> %d/%d)", min, max, *newMin, *newMax);

                return true;
            }
        }
        /* Try to find similar fps in HIDDEN REAR LIST*/
        sizeList = m_staticInfo->hiddenRearFPSList;
        for (int i = 0; i < m_staticInfo->hiddenRearFPSListMax; i++) {
            if (max <= sizeList[i][1] && sizeList[i][0] <= min) {
                if(sizeList[i][1] == sizeList[i][0])
                    continue;

                *newMin = sizeList[i][0];
                *newMax = sizeList[i][1];

                CLOGW("calibrate new fps(%d/%d -> %d/%d)", min, max, *newMin, *newMax);

                return true;
            }
        }
    } else {
        /* Try to find exactly same in FRONT LIST*/
        sizeList = m_staticInfo->frontFPSList;
        for (int i = 0; i < m_staticInfo->frontFPSListMax; i++) {
            if (sizeList[i][1] == max && sizeList[i][0] == min) {
                *newMin = sizeList[i][0];
                *newMax = sizeList[i][1];

                return true;
            }
        }
        /* Try to find exactly same in HIDDEN FRONT LIST*/
        sizeList = m_staticInfo->hiddenFrontFPSList;
        for (int i = 0; i < m_staticInfo->hiddenFrontFPSListMax; i++) {
            if (sizeList[i][1] == max && sizeList[i][0] == min) {
                *newMin = sizeList[i][0];
                *newMax = sizeList[i][1];

                return true;
            }
        }
        /* Try to find similar fps in FRONT LIST*/
        sizeList = m_staticInfo->frontFPSList;
        for (int i = 0; i < m_staticInfo->frontFPSListMax; i++) {
            if (max <= sizeList[i][1] && sizeList[i][0] <= min) {
                if(sizeList[i][1] == sizeList[i][0])
                    continue;

                *newMin = sizeList[i][0];
                *newMax = sizeList[i][1];

                CLOGW("calibrate new fps(%d/%d -> %d/%d)", min, max, *newMin, *newMax);

                return true;
            }
        }
        /* Try to find similar fps in HIDDEN FRONT LIST*/
        sizeList = m_staticInfo->hiddenFrontFPSList;
        for (int i = 0; i < m_staticInfo->hiddenFrontFPSListMax; i++) {
            if (max <= sizeList[i][1] && sizeList[i][0] <= min) {
                if(sizeList[i][1] == sizeList[i][0])
                    continue;

                *newMin = sizeList[i][0];
                *newMax = sizeList[i][1];

                CLOGW("calibrate new fps(%d/%d -> %d/%d)", min, max, *newMin, *newMax);

                return true;
            }
        }
    }

    return false;
}

bool ExynosCamera1Parameters::m_isSupportedVideoSize(const int width,
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

    if (getCameraId() == CAMERA_ID_BACK) {
        sizeList = m_staticInfo->hiddenRearVideoList;
        for (int i = 0; i < m_staticInfo->hiddenRearVideoListMax; i++) {
            if (sizeList[i][0] > maxWidth || sizeList[i][1] > maxHeight)
                continue;
            if (sizeList[i][0] == width && sizeList[i][1] == height) {
                m_cameraInfo.videoSizeRatioId = sizeList[i][2];
                return true;
            }
        }
    } else {
        sizeList = m_staticInfo->hiddenFrontVideoList;
        for (int i = 0; i < m_staticInfo->hiddenFrontVideoListMax; i++) {
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

bool ExynosCamera1Parameters::m_isUHDRecordingMode(void)
{
    bool isUHDRecording = false;
    int videoW = 0, videoH = 0;
    getVideoSize(&videoW, &videoH);

    if (((videoW == 3840 && videoH == 2160) || (videoW == 2560 && videoH == 1440))
        && getRecordingHint() == true) {
        isUHDRecording = true;
    }
#if 0
    /* we need to make WQHD SCP(LCD size), when FHD recording for clear rendering */
    int hwPreviewW = 0, hwPreviewH = 0;
    getHwPreviewSize(&hwPreviewW, &hwPreviewH);

#ifdef SUPPORT_SW_VDIS
    if(m_swVDIS_Mode) {
        m_swVDIS_AdjustPreviewSize(&hwPreviewW, &hwPreviewH);
    }
#endif /*SUPPORT_SW_VDIS*/

    /* regard align margin(ex:1920x1088), check size more than 1920x1088 */
    /* if (1920 < hwPreviewW && 1080 < hwPreviewH) */
    if ((ALIGN_UP(1920, CAMERA_16PX_ALIGN) < hwPreviewW) &&
        (ALIGN_UP(1080, CAMERA_16PX_ALIGN) < hwPreviewH) &&
        (getRecordingHint() == true)) {
        isUHDRecording = true;
    }
#endif

    return isUHDRecording;
}

void ExynosCamera1Parameters::m_setHwVideoSize(int w, int h)
{
    m_cameraInfo.hwVideoW = w;
    m_cameraInfo.hwVideoH = h;
}

bool ExynosCamera1Parameters::getUHDRecordingMode(void)
{
    return m_isUHDRecordingMode();
}

void ExynosCamera1Parameters::getHwVideoSize(int *w, int *h)
{
    if (m_cameraInfo.hwVideoW == 0 || m_cameraInfo.hwVideoH == 0) {
        *w = m_cameraInfo.videoW;
        *h = m_cameraInfo.videoH;
    } else {
        *w = m_cameraInfo.hwVideoW;
        *h = m_cameraInfo.hwVideoH;
    }

}

void ExynosCamera1Parameters::getVideoSize(int *w, int *h)
{
    *w = m_cameraInfo.videoW;
    *h = m_cameraInfo.videoH;
}

void ExynosCamera1Parameters::getMaxVideoSize(int *w, int *h)
{
    *w = m_staticInfo->maxVideoW;
    *h = m_staticInfo->maxVideoH;
}

int ExynosCamera1Parameters::getVideoFormat(void)
{

    int colorFormat = 0;

/* use only NV21 for Vodeo, because OMX read this format before we determine Adaptive CSC or not */
#if 0
    if (doCscRecording() == false) {
        colorFormat = V4L2_PIX_FMT_NV21M;
        m_params.set(CameraParameters::KEY_VIDEO_FRAME_FORMAT, CameraParameters::PIXEL_FORMAT_YUV420SP_NV21);
        CLOGI("video_frame colorFormat == YUV420SP_NV21");
    } else {
        colorFormat = V4L2_PIX_FMT_NV12M;
        m_params.set(CameraParameters::KEY_VIDEO_FRAME_FORMAT, CameraParameters::PIXEL_FORMAT_YUV420SP);
        CLOGI("video_frame colorFormat == YUV420SP");
    }
#else
    if (getAdaptiveCSCRecording() == true) {
        colorFormat = V4L2_PIX_FMT_NV21M;
        CLOGV("video_frame colorFormat == YUV420SP_NV21");
    } else {
        colorFormat = V4L2_PIX_FMT_NV12M;
        CLOGV("video_frame colorFormat == YUV420SP_NV12");
    }

#endif

    CLOGV("[setParameters]colorFormat(0x%x):", colorFormat);

    return colorFormat;
}

bool ExynosCamera1Parameters::getReallocBuffer() {
    Mutex::Autolock lock(m_reallocLock);
    return m_reallocBuffer;
}

bool ExynosCamera1Parameters::setReallocBuffer(bool enable) {
    Mutex::Autolock lock(m_reallocLock);
    m_reallocBuffer = enable;
    return m_reallocBuffer;
}

status_t ExynosCamera1Parameters::checkFastFpsMode(const CameraParameters& params)
{
    int fastFpsMode  = params.getInt("fast-fps-mode");
    int tempShotMode = params.getInt("shot-mode");
    int prevFpsMode = getFastFpsMode();

    uint32_t curMinFps = 0;
    uint32_t curMaxFps = 0;
    uint32_t newMinFps = curMinFps;
    uint32_t newMaxFps = curMaxFps;

    bool recordingHint = getRecordingHint();
    bool isShotModeAnimated = false;
    bool flagHighSpeed = false;
    int newVideoW = 0;
    int newVideoH = 0;

    params.getVideoSize(&newVideoW, &newVideoH);
    getPreviewFpsRange(&curMinFps, &curMaxFps);

    // Workaround : Should be removed later once application fixes this.
    if( (curMinFps == 60 && curMaxFps == 60) && (newVideoW == 1920 && newVideoH == 1080) ) {
        fastFpsMode = 1;
    }

#if (!USE_HIGHSPEED_RECORDING)
    fastFpsMode = -1;
    CLOGD("[setParameters]fast-fps-mode not supported. set to (%d).", fastFpsMode);
#endif

    CLOGI("[setParameters]fast-fps-mode: %d. curFpsRange[Min=%d, Max=%d], [curFrameRate=%d]",
        fastFpsMode, curMinFps, curMaxFps, m_params.getPreviewFrameRate());


    if (fastFpsMode <= 0 || fastFpsMode > 3) {
        m_setHighSpeedRecording(false);
        setConfigMode(CONFIG_MODE::NORMAL);
        if( fastFpsMode != prevFpsMode) {
            setFastFpsMode(fastFpsMode);
            m_params.set("fast-fps-mode", fastFpsMode);
            setReallocBuffer(true);
            m_setRestartPreviewChecked(true);
        }
        return NO_ERROR;
    } else {
        if( fastFpsMode == prevFpsMode ) {
            CLOGE("[checkFastFpsMode]mode is not changed fastFpsMode(%d) prevFpsMode(%d)", fastFpsMode, prevFpsMode);
            return NO_ERROR;
        }
    }

    if (tempShotMode == SHOT_MODE_ANIMATED_SCENE) {
            isShotModeAnimated = true;
    }

    if ((recordingHint == true) && !(isShotModeAnimated)) {

        CLOGD("[setParameters]Set High Speed Recording");

        switch(fastFpsMode) {
            case 1:
                newMinFps = 60;
                newMaxFps = 60;
                setConfigMode(CONFIG_MODE::HIGHSPEED_60);
                break;
            case 2:
                newMinFps = 120;
                newMaxFps = 120;
                setConfigMode(CONFIG_MODE::HIGHSPEED_120);
                break;
            case 3:
                newMinFps = 240;
                newMaxFps = 240;
                setConfigMode(CONFIG_MODE::HIGHSPEED_240);
                break;
        }

        setFastFpsMode(fastFpsMode);
        m_params.set("fast-fps-mode", fastFpsMode);

        CLOGI("fastFpsMode(%d) prevFpsMode(%d)", fastFpsMode, prevFpsMode);
        setReallocBuffer(true);
        m_setRestartPreviewChecked(true);

        flagHighSpeed = m_adjustHighSpeedRecording(curMinFps, curMaxFps, newMinFps, newMaxFps);
        m_setHighSpeedRecording(flagHighSpeed);
        m_setPreviewFpsRange(newMinFps, newMaxFps);

        CLOGI("[checkFastFpsMode]m_setPreviewFpsRange(newFpsRange[Min=%d, Max=%d])", newMinFps, newMaxFps);
#ifdef TEST_GED_HIGH_SPEED_RECORDING
        m_params.setPreviewFrameRate(newMaxFps);
        CLOGD("[checkFastFpsMode]setPreviewFrameRate (newMaxFps=%d)", newMaxFps);
#endif
        updateHwSensorSize();
    }

    return NO_ERROR;
};

void ExynosCamera1Parameters::setFastFpsMode(int fpsMode)
{
    m_fastFpsMode = fpsMode;
}

int ExynosCamera1Parameters::getFastFpsMode(void)
{
    return m_fastFpsMode;
}

void ExynosCamera1Parameters::m_setHighSpeedRecording(bool highSpeed)
{
    m_cameraInfo.highSpeedRecording = highSpeed;
}

bool ExynosCamera1Parameters::getHighSpeedRecording(void)
{
    return m_cameraInfo.highSpeedRecording;
}

int *ExynosCamera1Parameters::getHighSpeedSizeTable(int fpsMode) {
    int index = 0;
    int *sizeList = NULL;
    int videoW = 0, videoH = 0;
    int videoRatioEnum = SIZE_RATIO_16_9;
    float videoRatio = 0.00f;

    /*
       SIZE_RATIO_16_9 = 0,
       SIZE_RATIO_4_3,
       SIZE_RATIO_1_1,
       SIZE_RATIO_3_2,
       SIZE_RATIO_5_4,
       SIZE_RATIO_5_3,
       SIZE_RATIO_11_9,
     */
    getVideoSize(&videoW, &videoH);
    videoRatio = ROUND_OFF(((float)videoW / (float)videoH), 2);
    if (videoRatio == 1.33f)        /* 4:3 */
        videoRatioEnum = SIZE_RATIO_4_3;
    else if (videoRatio == 1.77f) /* 16:9 */
        videoRatioEnum = SIZE_RATIO_16_9;
    else if (videoRatio == 1.00f) /* 1:1 */
        videoRatioEnum = SIZE_RATIO_1_1;
    else if (videoRatio == 1.50f) /* 3:2 */
        videoRatioEnum = SIZE_RATIO_3_2;
    else if (videoRatio == 1.25f) /* 5:4 */
        videoRatioEnum = SIZE_RATIO_5_4;
    else if (videoRatio == 1.66f) /* 5:3*/
        videoRatioEnum = SIZE_RATIO_5_3;
    else if (videoRatio == 1.22f) /* 11:9 */
        videoRatioEnum = SIZE_RATIO_11_9;
    else
        videoRatioEnum = SIZE_RATIO_16_9;

    switch (fpsMode) {
    case CONFIG_MODE::HIGHSPEED_60:
        if (m_staticInfo->videoSizeLutHighSpeed60Max == 0
            || m_staticInfo->videoSizeLutHighSpeed60 == NULL) {
            CLOGE("videoSizeLutHighSpeed is NULL.fpsMode(%d)", fpsMode);
            sizeList = NULL;
            break;
        }

        for (index = 0; index < m_staticInfo->videoSizeLutHighSpeed60Max; index++) {
            if (m_staticInfo->videoSizeLutHighSpeed60[index][RATIO_ID] == videoRatioEnum)
                break;
        }

        if (index >= m_staticInfo->videoSizeLutHighSpeed60Max)
            index = 0;

        sizeList = m_staticInfo->videoSizeLutHighSpeed60[index];
        break;
    case CONFIG_MODE::HIGHSPEED_120:
        if (m_staticInfo->videoSizeLutHighSpeed120Max == 0
                || m_staticInfo->videoSizeLutHighSpeed120 == NULL) {
            CLOGE("videoSizeLutHighSpeed is NULL.fpsMode(%d)", fpsMode);
            sizeList = NULL;
            break;
        }

        for (index = 0; index < m_staticInfo->videoSizeLutHighSpeed120Max; index++) {
            if (m_staticInfo->videoSizeLutHighSpeed120[index][RATIO_ID] == videoRatioEnum)
                break;
        }

        if (index >= m_staticInfo->videoSizeLutHighSpeed120Max)
            index = 0;

        sizeList = m_staticInfo->videoSizeLutHighSpeed120[index];
        break;
    case CONFIG_MODE::HIGHSPEED_240:
        if (m_staticInfo->videoSizeLutHighSpeed240Max == 0
                || m_staticInfo->videoSizeLutHighSpeed240 == NULL) {
            CLOGE("videoSizeLutHighSpeed is NULL.fpsMode(%d)", fpsMode);
            sizeList = NULL;
            break;
        }

        for (index = 0; index < m_staticInfo->videoSizeLutHighSpeed240Max; index++) {
            if (m_staticInfo->videoSizeLutHighSpeed240[index][RATIO_ID] == videoRatioEnum)
                break;
        }

        if (index >= m_staticInfo->videoSizeLutHighSpeed240Max)
            index = 0;

        sizeList = m_staticInfo->videoSizeLutHighSpeed240[index];
        break;
    default :
        CLOGE("getConfigMode:fpsmode(%d) fail", fpsMode);
        break;
    }

    return sizeList;
}

int *ExynosCamera1Parameters::getDualModeSizeTable(void) {
    int *sizeList = NULL;
#ifdef USE_LIVE_BROADCAST_DUAL
    bool plb_mode = getPLBMode();

    CLOGV("plb_mode(%d), BinningMode(%d)",
            plb_mode, getBinningMode());

    if (plb_mode == true && getBinningMode()) {
        int index = 0;

        if (m_staticInfo->liveBroadcastSizeLut == NULL
                || m_staticInfo->liveBroadcastSizeLutMax == 0) {
            CLOGE("liveBroadcastSizeLut is NULL");
            return sizeList;
        }

        for (index = 0; index < m_staticInfo->liveBroadcastSizeLutMax; index++) {
            if (m_staticInfo->liveBroadcastSizeLut[index][0] == m_cameraInfo.previewSizeRatioId)
                break;
        }

        if (m_staticInfo->liveBroadcastSizeLutMax <= index) {
            CLOGE("unsupported video ratioId(%d)",
                    m_cameraInfo.previewSizeRatioId);
            sizeList = NULL;
        } else {
            sizeList = m_staticInfo->liveBroadcastSizeLut[index];
        }
    } else
#endif
    {
#ifdef USE_DUAL_CAMERA
#if 0
        //TODO: add new dual LUT?
        if (getDualCameraMode() == true
                && getRecordingHint() == true
                && m_staticInfo->dualCameraVideoSizeLut != NULL
                && m_cameraInfo.previewSizeRatioId < m_staticInfo->videoSizeLutMax) {
            sizeList = m_staticInfo->dualCameraVideoSizeLut[m_cameraInfo.previewSizeRatioId];
        } else if (getDualCameraMode() == true
                && m_staticInfo->dualCameraPreviewSizeLut != NULL
                && m_cameraInfo.previewSizeRatioId < m_staticInfo->previewSizeLutMax) {
            sizeList = m_staticInfo->dualCameraPreviewSizeLut[m_cameraInfo.previewSizeRatioId];
        } else
#else
        if (getDualCameraMode() == true
                && getRecordingHint() == true
                && m_staticInfo->videoSizeLut != NULL
                && m_cameraInfo.previewSizeRatioId < m_staticInfo->videoSizeLutMax) {
            sizeList = m_staticInfo->videoSizeLut[m_cameraInfo.previewSizeRatioId];
        } else if (getDualCameraMode() == true
                && m_staticInfo->previewSizeLut != NULL
                && m_cameraInfo.previewSizeRatioId < m_staticInfo->previewSizeLutMax) {
            sizeList = m_staticInfo->previewSizeLut[m_cameraInfo.previewSizeRatioId];
        } else
#endif
#endif
        if (getDualRecordingHint() == true
                && m_staticInfo->dualVideoSizeLut != NULL
                && m_cameraInfo.previewSizeRatioId < m_staticInfo->videoSizeLutMax) {
            sizeList = m_staticInfo->dualVideoSizeLut[m_cameraInfo.previewSizeRatioId];
        } else if (m_staticInfo->dualPreviewSizeLut != NULL
                && m_cameraInfo.previewSizeRatioId < m_staticInfo->previewSizeLutMax) {
            sizeList = m_staticInfo->dualPreviewSizeLut[m_cameraInfo.previewSizeRatioId];
        } else { /* Use Preview LUT as a default */
            if (m_staticInfo->previewSizeLut == NULL) {
                CLOGE("previewSizeLut is NULL");
                sizeList = NULL;
                return sizeList;
            } else if (m_staticInfo->previewSizeLutMax <= m_cameraInfo.previewSizeRatioId) {
                CLOGE("unsupported preview ratioId(%d)",
                        m_cameraInfo.previewSizeRatioId);
                sizeList = NULL;
                return sizeList;
            }
            sizeList = m_staticInfo->previewSizeLut[m_cameraInfo.previewSizeRatioId];
        }
    }

    return sizeList;
}

int *ExynosCamera1Parameters::getBnsRecordingSizeTable(void) {
    int *sizeList = NULL;
#ifdef USE_LIVE_BROADCAST
    bool plb_mode = getPLBMode();

    CLOGV("plb_mode(%d), BinningMode(%d)",
            plb_mode, getBinningMode());

    if (plb_mode == true && getBinningMode()) {
        int index = 0;

        if (m_staticInfo->liveBroadcastSizeLut == NULL
                || m_staticInfo->liveBroadcastSizeLutMax == 0) {
            CLOGE("liveBroadcastSizeLut is NULL");
            return sizeList;
        }

        for (index = 0; index < m_staticInfo->liveBroadcastSizeLutMax; index++) {
            if (m_staticInfo->liveBroadcastSizeLut[index][0] == m_cameraInfo.previewSizeRatioId)
                break;
        }

        if (m_staticInfo->liveBroadcastSizeLutMax <= index) {
            CLOGE("unsupported video ratioId(%d)",
                    m_cameraInfo.previewSizeRatioId);
            sizeList = NULL;
        } else {
            sizeList = m_staticInfo->liveBroadcastSizeLut[index];
        }
    } else
#endif
    {
        if (m_staticInfo->videoSizeLutMax <= m_cameraInfo.previewSizeRatioId) {
            CLOGE("unsupported video ratioId(%d)",
                    m_cameraInfo.previewSizeRatioId);
            return sizeList;
        }

        sizeList = m_staticInfo->videoSizeBnsLut[m_cameraInfo.previewSizeRatioId];
    }

    return sizeList;
}

int *ExynosCamera1Parameters::getNormalRecordingSizeTable(void) {
    int *sizeList = NULL;
#ifdef USE_LIVE_BROADCAST
    bool plb_mode = getPLBMode();

    CLOGV("plb_mode(%d), BinningMode(%d)",
            plb_mode, getBinningMode());

    if (plb_mode == true && getBinningMode()) {
        int index = 0;

        if (m_staticInfo->liveBroadcastSizeLut == NULL
                || m_staticInfo->liveBroadcastSizeLutMax == 0) {
            CLOGE("liveBroadcastSizeLut is NULL");
            return sizeList;
        }

        for (index = 0; index < m_staticInfo->liveBroadcastSizeLutMax; index++) {
            if (m_staticInfo->liveBroadcastSizeLut[index][0] == m_cameraInfo.previewSizeRatioId)
                break;
        }

        if (m_staticInfo->liveBroadcastSizeLutMax <= index) {
            CLOGE("unsupported video ratioId(%d)",
                    m_cameraInfo.previewSizeRatioId);
            sizeList = NULL;
        } else {
            sizeList = m_staticInfo->liveBroadcastSizeLut[index];
        }
    } else
#endif
    {
        if (m_staticInfo->videoSizeLut == NULL) {
            CLOGE("videoSizeLut is NULL");
            return sizeList;
        } else if (m_staticInfo->videoSizeLutMax <= m_cameraInfo.previewSizeRatioId) {
            CLOGE("unsupported video ratioId(%d)",
                    m_cameraInfo.previewSizeRatioId);
            return sizeList;
        }

        sizeList = m_staticInfo->videoSizeLut[m_cameraInfo.previewSizeRatioId];
    }

    return sizeList;
}

int *ExynosCamera1Parameters::getBinningSizeTable(void) {
    int *sizeList = NULL;
#ifdef USE_LIVE_BROADCAST
    bool plb_mode = getPLBMode();

    CLOGV("plb_mode(%d), BinningMode(%d)",
            plb_mode, getBinningMode());

    if (plb_mode == true) {
        int index = 0;

        if (m_staticInfo->liveBroadcastSizeLut == NULL
                || m_staticInfo->liveBroadcastSizeLutMax == 0) {
            CLOGE("liveBroadcastSizeLut is NULL");
            return sizeList;
        }

        for (index = 0; index < m_staticInfo->liveBroadcastSizeLutMax; index++) {
            if (m_staticInfo->liveBroadcastSizeLut[index][0] == m_cameraInfo.previewSizeRatioId)
                break;
        }

        if (m_staticInfo->liveBroadcastSizeLutMax <= index) {
            CLOGE("unsupported binningSize ratioId(%d)",
                    m_cameraInfo.previewSizeRatioId);
            sizeList = NULL;
        } else {
            sizeList = m_staticInfo->liveBroadcastSizeLut[index];
        }
    } else
#endif
    {
        /*
             VT mode
             1: 3G vtmode (176x144, Fixed 7fps)
             2: LTE or WIFI vtmode (640x480, Fixed 15fps)
             3: LSI chip not used this mode
             4: CHN vtmode (1920X1080, Fixed 30fps)
        */
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
    }

    return sizeList;
}

bool ExynosCamera1Parameters::getSwitchSensor(void)
{
    return m_isSwitchSensor;
}

bool ExynosCamera1Parameters::m_adjustHighSpeedRecording(int curMinFps, int curMaxFps, __unused int newMinFps, int newMaxFps)
{
    bool flagHighSpeedRecording = false;
    bool restartPreview = false;

    /* setting high speed */
    if (30 < newMaxFps) {
        flagHighSpeedRecording = true;
        /* 30 -> 60/120 */
        if (curMaxFps <= 30)
            restartPreview = true;
        /* 60 -> 120 */
        else if (curMaxFps <= 60 && 120 <= newMaxFps)
            restartPreview = true;
        /* 120 -> 60 */
        else if (curMaxFps <= 120 && newMaxFps <= 60)
            restartPreview = true;
        /* variable 60 -> fixed 60 */
        else if (curMinFps < 60 && newMaxFps <= 60)
            restartPreview = true;
        /* variable 120 -> fixed 120 */
        else if (curMinFps < 120 && newMaxFps <= 120)
            restartPreview = true;
    } else if (newMaxFps <= 30) {
        flagHighSpeedRecording = false;
        if (30 < curMaxFps)
            restartPreview = true;
    }

    if (restartPreview == true &&
        getPreviewRunning() == true) {
        CLOGD("setRestartPreviewChecked true");
        m_setRestartPreviewChecked(true);
    }

    return flagHighSpeedRecording;
}

void ExynosCamera1Parameters::m_setRestartPreviewChecked(bool restart)
{
    CLOGD("setRestartPreviewChecked(during SetParameters) %s", restart ? "true" : "false");
    Mutex::Autolock lock(m_parameterLock);

    m_flagRestartPreviewChecked = restart;
}

bool ExynosCamera1Parameters::m_getRestartPreviewChecked(void)
{
    Mutex::Autolock lock(m_parameterLock);

    return m_flagRestartPreviewChecked;
}

void ExynosCamera1Parameters::m_setRestartPreview(bool restart)
{
    CLOGD("setRestartPreview %s", restart ? "true" : "false");
    Mutex::Autolock lock(m_parameterLock);

    m_flagRestartPreview = restart;

}

void ExynosCamera1Parameters::setPreviewRunning(bool enable)
{
    Mutex::Autolock lock(m_parameterLock);

    m_previewRunning = enable;
    m_flagRestartPreviewChecked = false;
    m_flagRestartPreview = false;
}

void ExynosCamera1Parameters::setPictureRunning(bool enable)
{
    Mutex::Autolock lock(m_parameterLock);

    m_pictureRunning = enable;
}

void ExynosCamera1Parameters::setRecordingRunning(bool enable)
{
    Mutex::Autolock lock(m_parameterLock);

    m_recordingRunning = enable;
}

bool ExynosCamera1Parameters::getPreviewRunning(void)
{
    Mutex::Autolock lock(m_parameterLock);

    return m_previewRunning;
}

bool ExynosCamera1Parameters::getPictureRunning(void)
{
    Mutex::Autolock lock(m_parameterLock);

    return m_pictureRunning;
}

bool ExynosCamera1Parameters::getRecordingRunning(void)
{
    Mutex::Autolock lock(m_parameterLock);

    return m_recordingRunning;
}

bool ExynosCamera1Parameters::getRestartPreview(void)
{
    Mutex::Autolock lock(m_parameterLock);

    return m_flagRestartPreview;
}

status_t ExynosCamera1Parameters::checkVideoStabilization(const CameraParameters& params)
{
    /* video stablization */
    const char *newVideoStabilization = params.get(CameraParameters::KEY_VIDEO_STABILIZATION);
    bool currVideoStabilization = m_flagVideoStabilization;
    bool isVideoStabilization = false;

    if (newVideoStabilization != NULL) {
        CLOGD("[setParameters]newVideoStabilization %s", newVideoStabilization);

        if (!strcmp(newVideoStabilization, "true"))
            isVideoStabilization = true;

        if (currVideoStabilization != isVideoStabilization) {
            m_flagVideoStabilization = isVideoStabilization;
            m_setVideoStabilization(m_flagVideoStabilization);
            m_params.set(CameraParameters::KEY_VIDEO_STABILIZATION, newVideoStabilization);
        }
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setVideoStabilization(bool stabilization)
{
    m_cameraInfo.videoStabilization = stabilization;
}

bool ExynosCamera1Parameters::getVideoStabilization(void)
{
    return m_cameraInfo.videoStabilization;
}

bool ExynosCamera1Parameters::updateParameters(void)
{
#ifdef USE_DUAL_CAMERA
    /* 1. update TPU parameter */
    m_updateTpuParameters();
#endif

#ifdef USE_MCSC_FOR_FD
    /* 2. update VRA parameter */
	m_updateVRAParameters();
#endif

    return true;
}

status_t ExynosCamera1Parameters::checkPreviewSize(const CameraParameters& params)
{
    /* preview size */
    int previewW = 0;
    int previewH = 0;
    int newPreviewW = 0;
    int newPreviewH = 0;
    int newCalHwPreviewW = 0;
    int newCalHwPreviewH = 0;

    int curPreviewW = 0;
    int curPreviewH = 0;
    int curHwPreviewW = 0;
    int curHwPreviewH = 0;

    int videoW = 0, videoH = 0;

    params.getPreviewSize(&previewW, &previewH);
    getPreviewSize(&curPreviewW, &curPreviewH);
    getHwPreviewSize(&curHwPreviewW, &curHwPreviewH);
    getVideoSize(&videoW, &videoH);
    m_isHighResolutionMode(params);

    newPreviewW = previewW;
    newPreviewH = previewH;
    if (m_adjustPreviewSize(params, previewW, previewH, &newPreviewW, &newPreviewH, &newCalHwPreviewW, &newCalHwPreviewH) != OK) {
        CLOGE("[Parameters]adjustPreviewSize fail, newPreviewSize(%dx%d)", newPreviewW, newPreviewH);
        return BAD_VALUE;
    }

    if (m_isSupportedPreviewSize(newPreviewW, newPreviewH) == false) {
        CLOGE("[Parameters]new preview size is invalid(%dx%d)", newPreviewW, newPreviewH);
        return BAD_VALUE;
    }

    CLOGI("[setParameters]Cur Preview size(%dx%d)", curPreviewW, curPreviewH);
    CLOGI("[setParameters]Cur HwPreview size(%dx%d)", curHwPreviewW, curHwPreviewH);
    CLOGI("[setParameters]param.preview size(%dx%d)", previewW, previewH);
    CLOGI("[setParameters]Adjust Preview size(%dx%d), ratioId(%d)", newPreviewW, newPreviewH, m_cameraInfo.previewSizeRatioId);
    CLOGI("[setParameters]Calibrated HwPreview size(%dx%d)", newCalHwPreviewW, newCalHwPreviewH);

    if (curPreviewW != newPreviewW
       || curPreviewH != newPreviewH
       || curHwPreviewW != newCalHwPreviewW
       || curHwPreviewH != newCalHwPreviewH
       || getHighResolutionCallbackMode() == true) {
        m_setPreviewSize(newPreviewW, newPreviewH);
        m_setHwPreviewSize(newCalHwPreviewW, newCalHwPreviewH);

        if (getHighResolutionCallbackMode() == true) {
            m_previewSizeChanged = false;
        } else {
            CLOGD("setRestartPreviewChecked true");
#ifdef USE_ISP_BUFFER_SIZE_TO_BDS
            setReallocBuffer(true);
#endif
            m_setRestartPreviewChecked(true);
            m_previewSizeChanged = true;
        }
    } else {
        m_previewSizeChanged = false;
    }

    updateBinningScaleRatio();
    updateBnsScaleRatio();

    m_params.setPreviewSize(newPreviewW, newPreviewH);

    return NO_ERROR;
}

bool ExynosCamera1Parameters::m_isSupportedPreviewSize(const int width,
                                                     const int height)
{
    int maxWidth, maxHeight = 0;
    int (*sizeList)[SIZE_OF_RESOLUTION];
    int n_ListMax;

     if (getCameraId() == CAMERA_ID_BACK){
        sizeList = m_staticInfo->rearPictureList;
        n_ListMax = m_staticInfo->rearPictureListMax;
     } else {
        sizeList = m_staticInfo->frontPictureList;
        n_ListMax = m_staticInfo->frontPictureListMax;
     }

     if (getHighResolutionCallbackMode() == true) {
        for (int i = 0; i < n_ListMax; i++) {
            if (sizeList[i][0] == width && sizeList[i][1] == height) {
                m_cameraInfo.previewSizeRatioId = sizeList[i][2];
                return true;
            }
        }
        return false;
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

    if (getCameraId() == CAMERA_ID_BACK) {
        sizeList = m_staticInfo->hiddenRearPreviewList;
        for (int i = 0; i < m_staticInfo->hiddenRearPreviewListMax; i++) {
            if (sizeList[i][0] > maxWidth || sizeList[i][1] > maxHeight)
                continue;
            if (sizeList[i][0] == width && sizeList[i][1] == height) {
                m_cameraInfo.previewSizeRatioId = sizeList[i][2];
                return true;
            }
        }
    } else {
        sizeList = m_staticInfo->hiddenFrontPreviewList;
        for (int i = 0; i < m_staticInfo->hiddenFrontPreviewListMax; i++) {
            if (sizeList[i][0] > maxWidth || sizeList[i][1] > maxHeight)
                continue;
            if (sizeList[i][0] == width && sizeList[i][1] == height) {
                m_cameraInfo.previewSizeRatioId = sizeList[i][2];
                return true;
            }
        }
    }

    return false;
}

status_t ExynosCamera1Parameters::m_getPreviewSizeList(int *sizeList)
{
    int *tempSizeList = NULL;

    if (getDualMode() == true) {
        tempSizeList = getDualModeSizeTable();
    } else { /* getDualMode() == false */
#ifdef USE_BINNING_MODE
        if (getBinningMode() == true) {
            tempSizeList = getBinningSizeTable();
        } else
#endif
#ifdef USE_REMOSAIC_CAPTURE
        if (getSwitchSensor() == true) {
            tempSizeList = m_staticInfo->hrPreviewSizeLut[m_cameraInfo.previewSizeRatioId];
        } else
#endif
        {
            if (getRecordingHint() == true) {
                int videoW = 0, videoH = 0;
                getVideoSize(&videoW, &videoH);
                if (getHighSpeedRecording() == true) {
                    int fpsmode = 0;
                    fpsmode = getConfigMode();
                    tempSizeList = getHighSpeedSizeTable(fpsmode);

                    if (tempSizeList == NULL) {
                        fpsmode = getFastFpsMode();

                        if (fpsmode <= 0) {
                            CLOGE("getFastFpsMode fpsmode(%d) fail", fpsmode);
                            return BAD_VALUE;
                        } else if (m_staticInfo->videoSizeLutHighSpeed == NULL) {
                            CLOGE("videoSizeLutHighSpeed is NULL");
                            return INVALID_OPERATION;
                        }

                        fpsmode--;
                        tempSizeList = m_staticInfo->videoSizeLutHighSpeed[fpsmode];
                    }
                }
#ifdef USE_BNS_RECORDING
                else if (m_staticInfo->videoSizeBnsLut != NULL
                         && videoW == 1920 && videoH == 1080) /* Use BNS Recording only for FHD(16:9) */
                    tempSizeList = getBnsRecordingSizeTable();
#endif
                else /* Normal Recording Mode */
                    tempSizeList = getNormalRecordingSizeTable();
            } else { /* Use Preview LUT */
                if (m_staticInfo->previewSizeLut == NULL) {
                    CLOGE("previewSizeLut is NULL");
                    return INVALID_OPERATION;
                } else if (m_staticInfo->previewSizeLutMax <= m_cameraInfo.previewSizeRatioId) {
                    CLOGE("unsupported preview ratioId(%d)",
                            m_cameraInfo.previewSizeRatioId);
                    return BAD_VALUE;
                }

                tempSizeList = m_staticInfo->previewSizeLut[m_cameraInfo.previewSizeRatioId];
            }
        }
    }

    if (tempSizeList == NULL) {
        CLOGE("fail to get LUT");
        return INVALID_OPERATION;
    }

    for (int i = 0; i < SIZE_LUT_INDEX_END; i++)
        sizeList[i] = tempSizeList[i];

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_getSWVdisPreviewSize(int w, int h, int *newW, int *newH)
{
    if (w < 0 || h < 0) {
        return;
    }

    if (w == 3840 && h == 2160) {
        *newW = 4032;
        *newH = 2268;
    } else if (w == 1920 && h == 1080) {
        *newW = 2304;
        *newH = 1296;
    } else if (w == 1280 && h == 720) {
        *newW = 1536;
        *newH = 864;
    } else {
        *newW = ALIGN_UP((w * 6) / 5, CAMERA_16PX_ALIGN);
        *newH = ALIGN_UP((h * 6) / 5, CAMERA_16PX_ALIGN);
    }
}

void ExynosCamera1Parameters::m_getSWVdisVideoSize(int w, int h, int *newW, int *newH)
{
    if (w < 0 || h < 0) {
        return;
    }

    if (w == 3840 && h == 2160) {
        *newW = 4032;
        *newH = 2268;
    } else if (w == 1920 && h == 1080) {
        *newW = 2304;
        *newH = 1296;
    } else if (w == 1280 && h == 720) {
        *newW = 1536;
        *newH = 864;
    } else {
        *newW = ALIGN_UP((w * 6) / 5, CAMERA_16PX_ALIGN);
        *newH = ALIGN_UP((h * 6) / 5, CAMERA_16PX_ALIGN);
    }
}

bool ExynosCamera1Parameters::m_isHighResolutionCallbackSize(const int width, const int height)
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

void ExynosCamera1Parameters::m_isHighResolutionMode(const CameraParameters& params)
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

void ExynosCamera1Parameters::m_setHighResolutionCallbackMode(bool enable)
{
    m_cameraInfo.highResolutionCallbackMode = enable;
}

bool ExynosCamera1Parameters::getHighResolutionCallbackMode(void)
{
    return m_cameraInfo.highResolutionCallbackMode;
}

status_t ExynosCamera1Parameters::checkPreviewFormat(const CameraParameters& params)
{
    const char *strNewPreviewFormat = params.getPreviewFormat();
    const char *strCurPreviewFormat = m_params.getPreviewFormat();
    int curHwPreviewFormat = getHwPreviewFormat();
#ifdef USE_MCSC1_FOR_PREVIEWCALLBACK
    int curPreviewFormat = getPreviewFormat();
#endif
    int newPreviewFormat = 0;
    int hwPreviewFormat = 0;

    CLOGD("[setParameters]newPreviewFormat: %s", strNewPreviewFormat);

    if (!strcmp(strNewPreviewFormat, CameraParameters::PIXEL_FORMAT_RGB565))
        newPreviewFormat = V4L2_PIX_FMT_RGB565;
    else if (!strcmp(strNewPreviewFormat, CameraParameters::PIXEL_FORMAT_RGBA8888))
        newPreviewFormat = V4L2_PIX_FMT_RGB32;
    else if (!strcmp(strNewPreviewFormat, CameraParameters::PIXEL_FORMAT_YUV420SP))
        newPreviewFormat = V4L2_PIX_FMT_NV21;
    else if (!strcmp(strNewPreviewFormat, CameraParameters::PIXEL_FORMAT_YUV420P))
        newPreviewFormat = V4L2_PIX_FMT_YVU420;
    else if (!strcmp(strNewPreviewFormat, "yuv420sp_custom"))
        newPreviewFormat = V4L2_PIX_FMT_NV12T;
    else if (!strcmp(strNewPreviewFormat, "yuv422i"))
        newPreviewFormat = V4L2_PIX_FMT_YUYV;
    else if (!strcmp(strNewPreviewFormat, "yuv422p"))
        newPreviewFormat = V4L2_PIX_FMT_YUV422P;
    else
        newPreviewFormat = V4L2_PIX_FMT_NV21; /* for 3rd party */

    if (m_adjustPreviewFormat(newPreviewFormat, hwPreviewFormat) != NO_ERROR) {
        return BAD_VALUE;
    }

    m_setPreviewFormat(newPreviewFormat);
    m_params.setPreviewFormat(strNewPreviewFormat);

#ifdef USE_MCSC1_FOR_PREVIEWCALLBACK
    if (curPreviewFormat != newPreviewFormat) {
        CLOGI("[setParameters]: preview format changed cur(%s) -> new(%s)",
            strCurPreviewFormat, strNewPreviewFormat);

        if (getPreviewRunning() == true) {
            CLOGD("[setParameters]: setRestartPreviewChecked true");
            m_setRestartPreviewChecked(true);
        }
    }
#endif

    if (curHwPreviewFormat != hwPreviewFormat) {
        m_setHwPreviewFormat(hwPreviewFormat);
        CLOGI("[setParameters]preview format changed cur(%s) -> new(%s)",
            strCurPreviewFormat, strNewPreviewFormat);

        if (getPreviewRunning() == true) {
            CLOGD("setRestartPreviewChecked true");
            m_setRestartPreviewChecked(true);
        }
    }

    return NO_ERROR;
}

status_t ExynosCamera1Parameters::m_adjustPreviewFormat(__unused int &previewFormat, int &hwPreviewFormat)
{
#if 1
    /* HACK : V4L2_PIX_FMT_NV21M is set to FIMC-IS  *
     * and Gralloc. V4L2_PIX_FMT_YVU420 is just     *
     * color format for callback frame.             */
    hwPreviewFormat = V4L2_PIX_FMT_NV21M;
#else
    if (previewFormat == V4L2_PIX_FMT_NV21)
        hwPreviewFormat = V4L2_PIX_FMT_NV21M;
    else if (previewFormat == V4L2_PIX_FMT_YVU420)
        hwPreviewFormat = V4L2_PIX_FMT_YVU420M;
#endif

#ifdef USE_CP_FUSION_LIB
    // we need to fix NV21 for fusion
    if (isFusionEnabled() == true) {
        hwPreviewFormat = V4L2_PIX_FMT_NV21;
    }
#endif

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setPreviewSize(int w, int h)
{
    m_cameraInfo.previewW = w;
    m_cameraInfo.previewH = h;
}

void ExynosCamera1Parameters::getPreviewSize(int *w, int *h)
{
    *w = m_cameraInfo.previewW;
    *h = m_cameraInfo.previewH;
}

void ExynosCamera1Parameters::getMaxSensorSize(int *w, int *h)
{
    *w = m_staticInfo->maxSensorW;
    *h = m_staticInfo->maxSensorH;
}

void ExynosCamera1Parameters::getSensorMargin(int *w, int *h)
{
    *w = m_staticInfo->sensorMarginW;
    *h = m_staticInfo->sensorMarginH;
}

void ExynosCamera1Parameters::m_adjustSensorMargin(int *sensorMarginW, int *sensorMarginH)
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

    *sensorMarginW = ALIGN_DOWN((int)(*sensorMarginW / sensorMarginRatio), 2);
    *sensorMarginH = ALIGN_DOWN((int)(*sensorMarginH / sensorMarginRatio), 2);
}

void ExynosCamera1Parameters::getMaxPreviewSize(int *w, int *h)
{
    *w = m_staticInfo->maxPreviewW;
    *h = m_staticInfo->maxPreviewH;
}

void ExynosCamera1Parameters::m_setPreviewFormat(int fmt)
{
    m_cameraInfo.previewFormat = fmt;
}

int ExynosCamera1Parameters::getPreviewFormat(void)
{
    return m_cameraInfo.previewFormat;
}

void ExynosCamera1Parameters::m_setHwPreviewSize(int w, int h)
{
    m_cameraInfo.hwPreviewW = w;
    m_cameraInfo.hwPreviewH = h;
}

void ExynosCamera1Parameters::getHwPreviewSize(int *w, int *h)
{
    if (m_cameraInfo.scalableSensorMode != true) {
        *w = m_cameraInfo.hwPreviewW;
        *h = m_cameraInfo.hwPreviewH;
    } else {
        int newSensorW = 0;
        int newSensorH = 0;
        m_getScalableSensorSize(&newSensorW, &newSensorH);

        *w = newSensorW;
        *h = newSensorH;
/*
 *    Should not use those value
 *    *w = 1024;
 *    *h = 768;
 *    *w = 1440;
 *    *h = 1080;
 */
        *w = m_cameraInfo.hwPreviewW;
        *h = m_cameraInfo.hwPreviewH;
    }
}

void ExynosCamera1Parameters::setHwPreviewStride(int stride)
{
    m_cameraInfo.previewStride = stride;
}

int ExynosCamera1Parameters::getHwPreviewStride(void)
{
    return m_cameraInfo.previewStride;
}

void ExynosCamera1Parameters::m_setHwPreviewFormat(int fmt)
{
    m_cameraInfo.hwPreviewFormat = fmt;
}

int ExynosCamera1Parameters::getHwPreviewFormat(void)
{
    return m_cameraInfo.hwPreviewFormat;
}

void ExynosCamera1Parameters::updateHwSensorSize(void)
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

void ExynosCamera1Parameters::m_setHwSensorSize(int w, int h)
{
    m_cameraInfo.hwSensorW = w;
    m_cameraInfo.hwSensorH = h;
}

void ExynosCamera1Parameters::getHwSensorSize(int *w, int *h)
{
    CLOGV(" getScalableSensorMode()(%d)", getScalableSensorMode());
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

void ExynosCamera1Parameters::updateBnsScaleRatio(void)
{
    int ret = 0;
    uint32_t bnsRatio = DEFAULT_BNS_RATIO * 1000;
    int curPreviewW = 0, curPreviewH = 0;

    if (m_staticInfo->bnsSupport == false)
        return;

    getPreviewSize(&curPreviewW, &curPreviewH);
#ifdef USE_DUAL_CAMERA
    if (getDualCameraMode() == true) {
        bnsRatio = 1000; // Dual Camera use BDS, not BNS.
    } else
#endif
    if (getDualMode() == true
#ifdef USE_LIVE_BROADCAST_DUAL
        && !getPLBMode()
#endif
       ) {
#if defined(USE_BNS_DUAL_PREVIEW) || defined(USE_BNS_DUAL_RECORDING)
#ifdef DUAL_BNS_RATIO
        bnsRatio = DUAL_BNS_RATIO;
#else
        bnsRatio = 2000;
#endif
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

status_t ExynosCamera1Parameters::m_setBnsScaleRatio(uint32_t ratio)
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

    if (ratio != m_cameraInfo.bnsScaleRatio)
        CLOGD(" update BNS ratio(%d -> %d)", m_cameraInfo.bnsScaleRatio, ratio);

    m_cameraInfo.bnsScaleRatio = ratio;

    /* When BNS scale ratio is changed, reset BNS size to MAX sensor size */
    getMaxSensorSize(&m_cameraInfo.bnsW, &m_cameraInfo.bnsH);

    return NO_ERROR;
}

status_t ExynosCamera1Parameters::m_addHiddenResolutionList(String8 &string8Buf,
                                                            __unused struct ExynosSensorInfoBase *sensorInfo,
                                                            int w, int h, enum MODE mode, int cameraId)

{
    status_t ret = NO_ERROR;
    bool found = false;

    int (*sizeList)[SIZE_OF_RESOLUTION];
    int listSize = 0;

    switch (mode) {
    case MODE_PREVIEW:
        if (cameraId == CAMERA_ID_BACK) {
            sizeList = m_staticInfo->hiddenRearPreviewList;
            listSize = m_staticInfo->hiddenRearPreviewListMax;
        } else {
            sizeList = m_staticInfo->hiddenFrontPreviewList;
            listSize = m_staticInfo->hiddenFrontPreviewListMax;
        }
        break;
    case MODE_PICTURE:
        if (cameraId == CAMERA_ID_BACK) {
            sizeList = m_staticInfo->hiddenRearPictureList;
            listSize = m_staticInfo->hiddenRearPictureListMax;
        } else {
            sizeList = m_staticInfo->hiddenFrontPictureList;
            listSize = m_staticInfo->hiddenFrontPictureListMax;
        }
        break;
    case MODE_VIDEO:
        if (cameraId == CAMERA_ID_BACK) {
            sizeList = m_staticInfo->hiddenRearVideoList;
            listSize = m_staticInfo->hiddenRearVideoListMax;
        } else {
            sizeList = m_staticInfo->hiddenFrontVideoList;
            listSize = m_staticInfo->hiddenFrontVideoListMax;
        }
        break;
    default:
        CLOGE(" invalid mode(%d)", mode);
        return BAD_VALUE;
        break;
    }

    for (int i = 0; i < listSize; i++) {
        if (w == sizeList[i][0] && h == sizeList[i][1]) {
            found = true;
            break;
        }
    }

    if (found == true) {
        String8 uhdTempStr;
        char strBuf[32];

        snprintf(strBuf, sizeof(strBuf), "%dx%d,", w, h);

        /* append on head of string8Buf */
        uhdTempStr.setTo(strBuf);
        uhdTempStr.append(string8Buf);
        string8Buf.setTo(uhdTempStr);
    } else {
        ret = INVALID_OPERATION;
    }

    return ret;
}

uint32_t ExynosCamera1Parameters::getBnsScaleRatio(void)
{
    return m_cameraInfo.bnsScaleRatio;
}

void ExynosCamera1Parameters::setBnsSize(int w, int h)
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

void ExynosCamera1Parameters::getBnsSize(int *w, int *h)
{
    *w = m_cameraInfo.bnsW;
    *h = m_cameraInfo.bnsH;
}

void ExynosCamera1Parameters::updateBinningScaleRatio(void)
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

status_t ExynosCamera1Parameters::m_setBinningScaleRatio(int ratio)
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

uint32_t ExynosCamera1Parameters::getBinningScaleRatio(void)
{
    return m_cameraInfo.binningScaleRatio;
}

status_t ExynosCamera1Parameters::checkPictureSize(const CameraParameters& params)
{
    int curPictureW = 0;
    int curPictureH = 0;
    int newPictureW = 0;
    int newPictureH = 0;
    int curHwPictureW = 0;
    int curHwPictureH = 0;
    int newHwPictureW = 0;
    int newHwPictureH = 0;
    int right_ratio = 177;

    params.getPictureSize(&newPictureW, &newPictureH);

    if (newPictureW < 0 || newPictureH < 0) {
        return BAD_VALUE;
    }

    if (m_adjustPictureSize(&newPictureW, &newPictureH, &newHwPictureW, &newHwPictureH) != NO_ERROR) {
        return BAD_VALUE;
    }

    if (m_scenario == SCENARIO_SECURE) {
        int maxHwPictureW =0;
        int maxHwPictureH = 0;

        /* prevent wrong size setting */
        getMaxPictureSize(&maxHwPictureW, &maxHwPictureH);
        m_setPictureSize(maxHwPictureW, maxHwPictureH);
        m_setHwPictureSize(maxHwPictureW, maxHwPictureH);
        m_params.setPictureSize(maxHwPictureW, maxHwPictureH);

        CLOGE("SECURE picture size(%dx%d)", maxHwPictureW, maxHwPictureH);
        return NO_ERROR;
    }

    if (m_isSupportedPictureSize(newPictureW, newPictureH) == false) {
        int maxHwPictureW =0;
        int maxHwPictureH = 0;

        CLOGE("Invalid picture size(%dx%d)", newPictureW, newPictureH);

        /* prevent wrong size setting */
        getMaxPictureSize(&maxHwPictureW, &maxHwPictureH);
        m_setPictureSize(maxHwPictureW, maxHwPictureH);
        m_setHwPictureSize(maxHwPictureW, maxHwPictureH);
        m_params.setPictureSize(maxHwPictureW, maxHwPictureH);
        CLOGE("changed picture size to MAX(%dx%d)", maxHwPictureW, maxHwPictureH);

#ifdef FIXED_SENSOR_SIZE
        updateHwSensorSize();
#endif
        return INVALID_OPERATION;
    }
    CLOGI("[setParameters]newPicture Size (%dx%d), ratioId(%d)",
        newPictureW, newPictureH, m_cameraInfo.pictureSizeRatioId);

    if ((int)(m_staticInfo->maxSensorW * 100 / m_staticInfo->maxSensorH) == right_ratio) {
        setHorizontalViewAngle(newPictureW, newPictureH);
    }
    m_params.setFloat(CameraParameters::KEY_HORIZONTAL_VIEW_ANGLE, getHorizontalViewAngle());

    getPictureSize(&curPictureW, &curPictureH);
    getHwPictureSize(&curHwPictureW, &curHwPictureH);

    if (curPictureW != newPictureW || curPictureH != newPictureH ||
        curHwPictureW != newHwPictureW || curHwPictureH != newHwPictureH) {

        CLOGI("[setParameters]Picture size changed: cur(%dx%d) -> new(%dx%d)",
                curPictureW, curPictureH, newPictureW, newPictureH);
        CLOGI("[setParameters]HwPicture size changed: cur(%dx%d) -> new(%dx%d)",
                curHwPictureW, curHwPictureH, newHwPictureW, newHwPictureH);

        m_setPictureSize(newPictureW, newPictureH);
        m_setHwPictureSize(newHwPictureW, newHwPictureH);
        m_params.setPictureSize(newPictureW, newPictureH);

#ifdef FIXED_SENSOR_SIZE
        updateHwSensorSize();
#endif
    }

    return NO_ERROR;
}

status_t ExynosCamera1Parameters::m_adjustPictureSize(int *newPictureW, int *newPictureH,
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

    ret = getCropRectAlign(*newHwPictureW, *newHwPictureH,
            *newPictureW, *newPictureH,
            &newX, &newY, &newW, &newH,
            CAMERA_16PX_ALIGN, 2, 0, zoomRatio);
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
            CLOGD("Use sensor crop (ratio: %f)",
                    ((float)*newPictureW / (float)*newPictureH));
            m_setHwSensorSize(newW, newH);
        }
    }
#endif

    return NO_ERROR;
}

bool ExynosCamera1Parameters::m_isSupportedPictureSize(const int width,
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

    if (getCameraId() == CAMERA_ID_BACK) {
        sizeList = m_staticInfo->hiddenRearPictureList;
        for (int i = 0; i < m_staticInfo->hiddenRearPictureListMax; i++) {
            if (sizeList[i][0] > maxWidth || sizeList[i][1] > maxHeight)
                continue;
            if (sizeList[i][0] == width && sizeList[i][1] == height) {
                m_cameraInfo.pictureSizeRatioId = sizeList[i][2];
                return true;
            }
        }
    } else {
        sizeList = m_staticInfo->hiddenFrontPictureList;
        for (int i = 0; i < m_staticInfo->hiddenFrontPictureListMax; i++) {
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

void ExynosCamera1Parameters::m_setPictureSize(int w, int h)
{
    m_cameraInfo.pictureW = w;
    m_cameraInfo.pictureH = h;
}

void ExynosCamera1Parameters::getPictureSize(int *w, int *h)
{
    *w = m_cameraInfo.pictureW;
    *h = m_cameraInfo.pictureH;
}

void ExynosCamera1Parameters::getMaxPictureSize(int *w, int *h)
{
    *w = m_staticInfo->maxPictureW;
    *h = m_staticInfo->maxPictureH;
}

void ExynosCamera1Parameters::m_setHwPictureSize(int w, int h)
{
    m_cameraInfo.hwPictureW = w;
    m_cameraInfo.hwPictureH = h;
}

void ExynosCamera1Parameters::getHwPictureSize(int *w, int *h)
{
    *w = m_cameraInfo.hwPictureW;
    *h = m_cameraInfo.hwPictureH;
}

void ExynosCamera1Parameters::m_setHwBayerCropRegion(int w, int h, int x, int y)
{
    Mutex::Autolock lock(m_parameterLock);

    m_cameraInfo.hwBayerCropW = w;
    m_cameraInfo.hwBayerCropH = h;
    m_cameraInfo.hwBayerCropX = x;
    m_cameraInfo.hwBayerCropY = y;
}

void ExynosCamera1Parameters::getHwBayerCropRegion(int *w, int *h, int *x, int *y)
{
    Mutex::Autolock lock(m_parameterLock);

    *w = m_cameraInfo.hwBayerCropW;
    *h = m_cameraInfo.hwBayerCropH;
    *x = m_cameraInfo.hwBayerCropX;
    *y = m_cameraInfo.hwBayerCropY;
}

void ExynosCamera1Parameters::m_setPictureFormat(int fmt)
{
    m_cameraInfo.pictureFormat = fmt;
}

int ExynosCamera1Parameters::getPictureFormat(void)
{
    return m_cameraInfo.pictureFormat;
}

void ExynosCamera1Parameters::m_setHwPictureFormat(int fmt)
{
    m_cameraInfo.hwPictureFormat = fmt;
}

int ExynosCamera1Parameters::getHwPictureFormat(void)
{
    return m_cameraInfo.hwPictureFormat;
}

status_t ExynosCamera1Parameters::checkJpegQuality(const CameraParameters& params)
{
    int newJpegQuality = params.getInt(CameraParameters::KEY_JPEG_QUALITY);
    int curJpegQuality = getJpegQuality();

    CLOGD("[setParameters]newJpegQuality %d", newJpegQuality);

    if (newJpegQuality < 1 || newJpegQuality > 100) {
        CLOGE(" Invalid Jpeg Quality (Min: %d, Max: %d, Value: %d)", 1, 100, newJpegQuality);
        return BAD_VALUE;
    }

    if (curJpegQuality != newJpegQuality) {
        m_setJpegQuality(newJpegQuality);
        m_params.set(CameraParameters::KEY_JPEG_QUALITY, newJpegQuality);
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setJpegQuality(int quality)
{
    m_cameraInfo.jpegQuality = quality;
}

int ExynosCamera1Parameters::getJpegQuality(void)
{
    return m_cameraInfo.jpegQuality;
}

status_t ExynosCamera1Parameters::checkThumbnailSize(const CameraParameters& params)
{
    int curThumbnailW = 0;
    int curThumbnailH = 0;
    int maxThumbnailW = 0;
    int maxThumbnailH = 0;
    int newJpegThumbnailW = params.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH);
    int newJpegThumbnailH = params.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT);

    CLOGD("[setParameters]newJpegThumbnailW X newJpegThumbnailH: %d X %d",
            newJpegThumbnailW, newJpegThumbnailH);

    getMaxThumbnailSize(&maxThumbnailW, &maxThumbnailH);

    if (newJpegThumbnailW < 0 || newJpegThumbnailH < 0 ||
        newJpegThumbnailW > maxThumbnailW || newJpegThumbnailH > maxThumbnailH) {
        CLOGE(" Invalid Thumbnail Size (maxSize(%d/%d) size(%d/%d)",
                maxThumbnailW, maxThumbnailH, newJpegThumbnailW, newJpegThumbnailH);
        return BAD_VALUE;
    }

    getThumbnailSize(&curThumbnailW, &curThumbnailH);

    if (curThumbnailW != newJpegThumbnailW || curThumbnailH != newJpegThumbnailH) {
        m_setThumbnailSize(newJpegThumbnailW, newJpegThumbnailH);
        m_params.set(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH,  newJpegThumbnailW);
        m_params.set(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT, newJpegThumbnailH);
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setThumbnailSize(int w, int h)
{
    m_cameraInfo.thumbnailW = w;
    m_cameraInfo.thumbnailH = h;
}

void ExynosCamera1Parameters::getThumbnailSize(int *w, int *h)
{
    *w = m_cameraInfo.thumbnailW;
    *h = m_cameraInfo.thumbnailH;
}

void ExynosCamera1Parameters::getMaxThumbnailSize(int *w, int *h)
{
    *w = m_staticInfo->maxThumbnailW;
    *h = m_staticInfo->maxThumbnailH;
}

status_t ExynosCamera1Parameters::checkThumbnailQuality(const CameraParameters& params)
{
    int newJpegThumbnailQuality = params.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY);
    int curThumbnailQuality = getThumbnailQuality();

    if (newJpegThumbnailQuality < 0 || newJpegThumbnailQuality > 100) {
        CLOGE(" Invalid Thumbnail Quality (Min: %d, Max: %d, Value: %d)",
                0, 100, newJpegThumbnailQuality);
        return BAD_VALUE;
    }

    CLOGD("[setParameters]newJpegThumbnailQuality %d", newJpegThumbnailQuality);

    if (curThumbnailQuality != newJpegThumbnailQuality) {
        m_setThumbnailQuality(newJpegThumbnailQuality);
        m_params.set(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY, newJpegThumbnailQuality);
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setThumbnailQuality(int quality)
{
    m_cameraInfo.thumbnailQuality = quality;
}

int ExynosCamera1Parameters::getThumbnailQuality(void)
{
    return m_cameraInfo.thumbnailQuality;
}

status_t ExynosCamera1Parameters::check3dnrMode(const CameraParameters& params)
{
    bool new3dnrMode = false;
    const char *str3dnrMode = params.get("3dnr");

    if (str3dnrMode == NULL) {
        return NO_ERROR;
    }

    CLOGD("[setParameters]new3dnrMode %s", str3dnrMode);

#ifdef TPU_HW_DEFAULT_OPEN
#if !defined(USE_3DNR_ALWAYS)
    if (!strcmp(str3dnrMode, "true"))
#endif
        new3dnrMode = true;
#else
    if (!strcmp(str3dnrMode, "true"))
        CLOGW("[setParameters]TPU disabled, change 3DNR mode to false!");

    new3dnrMode = false;
#endif

    if (m_flag3dnrMode != new3dnrMode) {
        m_flag3dnrMode = new3dnrMode;
        m_params.set("3dnr", str3dnrMode);
    }

    m_set3dnrMode(m_flag3dnrMode);
    if (setDnrEnable(m_flag3dnrMode) != NO_ERROR) {
        CLOGE("setDnrEnable(%d) fail", m_flag3dnrMode);
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_set3dnrMode(bool toggle)
{
    m_cameraInfo.is3dnrMode = toggle;
}

bool ExynosCamera1Parameters::get3dnrMode(void)
{
    return m_cameraInfo.is3dnrMode;
}

status_t ExynosCamera1Parameters::checkDrcMode(const CameraParameters& params)
{
    bool newDrcMode = false;
    bool curDrcMode = false;
    const char *strDrcMode = params.get("drc");

    if (strDrcMode == NULL) {
#ifdef USE_FRONT_PREVIEW_DRC
        if (getCameraId() == CAMERA_ID_FRONT && m_staticInfo->drcSupport == true) {
            newDrcMode = !getRecordingHint();
            m_setDrcMode(newDrcMode);
            CLOGD("[setParameters]Force DRC %s for front",
                    (newDrcMode == true)? "ON" : "OFF");
        }
#endif
        return NO_ERROR;
    }

    CLOGD("[setParameters]newDrcMode %s", strDrcMode);

    if (!strcmp(strDrcMode, "true"))
        newDrcMode = true;

    curDrcMode = getDrcMode();

    if (curDrcMode != newDrcMode) {
        m_setDrcMode(newDrcMode);
        m_params.set("drc", strDrcMode);
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setDrcMode(bool toggle)
{
    m_cameraInfo.isDrcMode = toggle;
    if (setDrcEnable(toggle) < 0) {
        CLOGE(" set DRC fail, toggle(%d)", toggle);
    }
}

bool ExynosCamera1Parameters::getDrcMode(void)
{
    return m_cameraInfo.isDrcMode;
}

status_t ExynosCamera1Parameters::checkOdcMode(const CameraParameters& params)
{
    bool newOdcMode = false;
    bool curOdcMode = false;
    const char *strOdcMode = params.get("odc");

    if (strOdcMode == NULL) {
        return NO_ERROR;
    }

    CLOGD("[setParameters]newOdcMode %s", strOdcMode);

#ifdef TPU_HW_DEFAULT_OPEN
    if (!strcmp(strOdcMode, "true"))
        newOdcMode = true;

#ifdef USE_ODC_VIDEO
    if (getRecordingHint() && (getFastFpsMode() <= 1))
        newOdcMode = true;
#endif

#else
    if (!strcmp(strOdcMode, "true"))
        CLOGW("[setParameters]TPU disabled, change ODC mode to false!");

    newOdcMode = false;
#endif

    curOdcMode = getOdcMode();

    if (curOdcMode != newOdcMode) {
        m_setOdcMode(newOdcMode);
        m_params.set("odc", strOdcMode);
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setOdcMode(bool toggle)
{
    m_cameraInfo.isOdcMode = toggle;
}

bool ExynosCamera1Parameters::getOdcMode(void)
{
    return m_cameraInfo.isOdcMode;
}

bool ExynosCamera1Parameters::getTpuEnabledMode(void)
{
#if !defined(USE_DUAL_CAMERA)
    if (getDualMode() == true)
        return false;
#endif

    if (getHWVdisMode() == true)
        return true;

    if (get3dnrMode() == true)
        return true;

    if (getOdcMode() == true)
        return true;

    return false;
}

void ExynosCamera1Parameters::getHwVraInputSize(int *w, int *h)
{
#if defined(MAX_VRA_INPUT_SIZE_WIDTH) && defined(MAX_VRA_INPUT_SIZE_HEIGHT)
    int vraWidth = MAX_VRA_INPUT_WIDTH;
    int vraHeight = MAX_VRA_INPUT_HEIGHT;
#else
    int vraWidth = 640;
    int vraHeight = 480;
#endif
    float vraRatio = ROUND_OFF(((float)vraWidth / (float)vraHeight), 2);

    switch (m_cameraInfo.previewSizeRatioId) {
    case SIZE_RATIO_16_9:
        *w = vraWidth;
        *h = ALIGN_UP((vraWidth / 16) * 9, 2);
        break;
    case SIZE_RATIO_4_3:
        *w = ALIGN_UP((vraHeight / 3) * 4, CAMERA_16PX_ALIGN);
        *h = vraHeight;
        break;
    case SIZE_RATIO_1_1:
        *w = vraHeight;
        *h = vraHeight;
        break;
    case SIZE_RATIO_3_2:
        if (vraRatio == 1.33f) { /* 4:3 */
            *w = vraWidth;
            *h = ALIGN_UP((vraWidth / 3) * 2, 2);
        } else if (vraRatio == 1.77f) { /* 16:9 */
            *w = ALIGN_UP((vraHeight / 2) * 3, CAMERA_16PX_ALIGN);
            *h = vraHeight;
        } else {
            *w = vraWidth;
            *h = vraHeight;
        }
        break;
    case SIZE_RATIO_5_4:
        *w = ALIGN_UP((vraHeight / 4) * 5, CAMERA_16PX_ALIGN);
        *h = vraHeight;
        break;
    case SIZE_RATIO_5_3:
        if (vraRatio == 1.33f) { /* 4:3 */
            *w = vraWidth;
            *h = ALIGN_UP((vraWidth / 5) * 3, 2);
        } else if (vraRatio == 1.77f) { /* 16:9 */
            *w = ALIGN_UP((vraHeight / 3) * 5, CAMERA_16PX_ALIGN);
            *h = vraHeight;
        } else {
            *w = vraWidth;
            *h = vraHeight;
        }
        break;
    case SIZE_RATIO_11_9:
        *w = ALIGN_UP((vraHeight / 9) * 11, CAMERA_16PX_ALIGN);
        *h = vraHeight;
        break;
    default:
        CLOGW("WARN(%s[%d]):Invalid size ratio(%d)",
                __FUNCTION__, __LINE__, m_cameraInfo.previewSizeRatioId);

        *w = vraWidth;
        *h = vraHeight;
        break;
    }
}

int ExynosCamera1Parameters::getHwVraInputFormat(void)
{
#if defined(CAMERA_VRA_INPUT_FORMAT)
    return CAMERA_VRA_INPUT_FORMAT;
#else
    return V4L2_PIX_FMT_NV16;
#endif
}

#ifdef USE_VRA_GROUP
void ExynosCamera1Parameters::setDsInputPortId(int dsInputPortId)
{
    m_dsInputPortId = dsInputPortId;
}

int ExynosCamera1Parameters::getDsInputPortId(void)
{
    return m_dsInputPortId;
}
#endif

status_t ExynosCamera1Parameters::checkZoomLevel(const CameraParameters& params)
{
    int newZoom = params.getInt(CameraParameters::KEY_ZOOM);

    CLOGD("[setParameters]newZoom %d", newZoom);

    /* cannot support DZoom -> set Zoom Level 0 */
    if (getZoomSupported() == false) {
        if (newZoom != ZOOM_LEVEL_0) {
            CLOGE("Invalid value (Zoom Should be %d, Value: %d)", ZOOM_LEVEL_0, newZoom);
            return BAD_VALUE;
        }

        if (setZoomLevel(ZOOM_LEVEL_0) != NO_ERROR)
            return BAD_VALUE;

        return NO_ERROR;
    } else {
        if (newZoom < ZOOM_LEVEL_0 || getMaxZoomLevel() <= newZoom) {
            CLOGE("Invalid value (Min: %d, Max: %d, Value: %d)", ZOOM_LEVEL_0, getMaxZoomLevel(), newZoom);
            return BAD_VALUE;
        }

        if (setZoomLevel(newZoom) != NO_ERROR) {
            return BAD_VALUE;
        }
        m_params.set(CameraParameters::KEY_ZOOM, newZoom);

        m_flagMeteringRegionChanged = true;

        return NO_ERROR;
    }
    return NO_ERROR;
}

status_t ExynosCamera1Parameters::setZoomLevel(int zoom)
{
    int srcW = 0;
    int srcH = 0;
    int dstW = 0;
    int dstH = 0;
    float zoomRatio = 1.00f;

#ifdef USE_DUAL_CAMERA
    if (getDualCameraMode() == true) {
        sync_type_t syncType = SYNC_TYPE_BASE;
        sync_type_t reprocessingSyncType = SYNC_TYPE_BASE;
        updateDualCameraSyncType(zoom, &syncType, &reprocessingSyncType);
        m_dualParameters->setZoom(m_cameraId, zoom);
    }
#endif
#ifdef USE_CP_FUSION_LIB
#ifdef USE_CP_FUSION_LIB_CROP_IN_LIBRARY
    /*
     * fusion will zoom internally.
     * HAL only give zoomRatio to fusion
     */
    if (isFusionEnabled() == false)
#endif
#endif
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

    zoomRatio = getZoomRatio(zoom) / 1000;
    m_currentZoomRatio = zoomRatio;

#ifdef USE_FW_ZOOMRATIO
#ifdef SAMSUNG_DUAL_SOLUTION
    if (getDualCameraMode() == true)
        setMetaCtlZoom(&m_metadata, getZoomOrgRatio(zoom) / 1000);
    else
#endif
    {
        setMetaCtlZoom(&m_metadata, zoomRatio);
    }
#endif

#ifdef SAMSUNG_DUAL_SOLUTION
    if (getDualCameraMode() == true) {
        ExynosRect statsRoi;
        getPreviewStatsRoi(&dstRect, &statsRoi);
        setMetaCtlStatsRoi(&m_metadata, statsRoi.x, statsRoi.y, statsRoi.w, statsRoi.h);

        CLOGD("zoomRatio = %.03f, orgZoomRatio = %.03f, cropRect(%d, %d, %d, %d), statsRoi(%d, %d, %d, %d)",
                zoomRatio, getZoomOrgRatio(zoom) / 1000,
                dstRect.x, dstRect.y, dstRect.w, dstRect.h,
                statsRoi.x, statsRoi.y, statsRoi.w, statsRoi.h);
    } else
#endif
    {
        setMetaCtlStatsRoi(&m_metadata, dstRect.x, dstRect.y, dstRect.w, dstRect.h);
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::setMasterCamera(int curDispCamId)
{
    enum aa_cameratype cameraType;

    if (getDualCameraMode() == true) {
        switch(curDispCamId) {
#ifdef SAMSUNG_DUAL_SOLUTION
        case UNI_PLUGIN_CAMERA_TYPE_WIDE:
            cameraType = AA_CAMERATYPE_WIDE;
            break;
        case UNI_PLUGIN_CAMERA_TYPE_TELE:
            cameraType = AA_CAMERATYPE_TELE;
            break;
#endif
        default:
            cameraType = AA_CAMERATYPE_SINGLE;
            break;
        }
    } else {
        cameraType = AA_CAMERATYPE_SINGLE;
    }

#ifdef SAMSUNG_DUAL_SOLUTION
    m_dualParameters->setMasterCam(m_cameraId, cameraType);
#endif

    setMetaCtlMasterCam(&m_metadata, cameraType);
    CLOGV("cameraId = %d, masterCamera = %d", m_cameraId, cameraType);
}

status_t ExynosCamera1Parameters::m_setParamCropRegion(
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

    CLOGV("size0(%d, %d, %d, %d)",
        srcW, srcH, dstW, dstH);
    CLOGV("size(%d, %d, %d, %d), level(%d)",
        newX, newY, newW, newH, zoom);

    m_setHwBayerCropRegion(newW, newH, newX, newY);

    return NO_ERROR;
}

int ExynosCamera1Parameters::getZoomLevel(void)
{
#ifdef USE_DUAL_CAMERA
    if (getDualCameraMode() == true)
        return m_dualParameters->getZoom(m_cameraId);
    else
#endif
        return m_cameraInfo.zoom;
}

status_t ExynosCamera1Parameters::checkRotation(const CameraParameters& params)
{
    int newRotation = params.getInt(CameraParameters::KEY_ROTATION);
    int curRotation = 0;

    if (newRotation < 0) {
        CLOGE(" Invalide Rotation value(%d)", newRotation);
        return NO_ERROR;
    }

    CLOGD("[setParameters]set orientation:%d", newRotation);

    curRotation = getRotation();

    if (curRotation != newRotation) {
        m_setRotation(newRotation);
        m_params.set(CameraParameters::KEY_ROTATION, newRotation);
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setRotation(int rotation)
{
    m_cameraInfo.rotation = rotation;
}

int ExynosCamera1Parameters::getRotation(void)
{
    return m_cameraInfo.rotation;
}

status_t ExynosCamera1Parameters::checkAutoExposureLock(const CameraParameters& params)
{
    bool newAutoExposureLock = false;
    bool curAutoExposureLock = false;
    const char *strAutoExposureLock = params.get(CameraParameters::KEY_AUTO_EXPOSURE_LOCK);
    if (strAutoExposureLock == NULL) {
        return NO_ERROR;
    }

    CLOGD("[setParameters]newAutoExposureLock %s", strAutoExposureLock);

    if (!strcmp(strAutoExposureLock, "true"))
        newAutoExposureLock = true;

    curAutoExposureLock = getAutoExposureLock();

    if (curAutoExposureLock != newAutoExposureLock) {
        ExynosCameraActivityFlash *m_flashMgr = m_activityControl->getFlashMgr();
        m_flashMgr->setAeLock(newAutoExposureLock);
        m_setAutoExposureLock(newAutoExposureLock);
        m_params.set(CameraParameters::KEY_AUTO_EXPOSURE_LOCK, strAutoExposureLock);
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setAutoExposureLock(bool lock)
{
    m_cameraInfo.autoExposureLock = lock;
    setMetaCtlAeLock(&m_metadata, lock);
}

bool ExynosCamera1Parameters::getAutoExposureLock(void)
{
    return m_cameraInfo.autoExposureLock;
}

void ExynosCamera1Parameters::m_adjustAeMode(enum aa_aemode curAeMode, enum aa_aemode *newAeMode)
{
    int curMeteringMode = getMeteringMode();
    if (curAeMode == AA_AEMODE_OFF) {
        switch(curMeteringMode){
        case METERING_MODE_AVERAGE:
            *newAeMode = AA_AEMODE_AVERAGE;
            break;
        case METERING_MODE_CENTER:
            *newAeMode = AA_AEMODE_CENTER;
            break;
        case METERING_MODE_MATRIX:
            *newAeMode = AA_AEMODE_MATRIX;
            break;
        case METERING_MODE_SPOT:
            *newAeMode = AA_AEMODE_SPOT;
            break;
        default:
            *newAeMode = curAeMode;
            break;
        }
    }
}

status_t ExynosCamera1Parameters::checkExposureCompensation(const CameraParameters& params)
{
    if (getHalVersion() == IS_HAL_VER_3_2)
        return NO_ERROR;

    int minExposureCompensation = params.getInt(CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION);
    int maxExposureCompensation = params.getInt(CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION);
    int newExposureCompensation = params.getInt(CameraParameters::KEY_EXPOSURE_COMPENSATION);
    int curExposureCompensation = getExposureCompensation();

    CLOGD("[setParameters]newExposureCompensation %d", newExposureCompensation);

    if ((newExposureCompensation < minExposureCompensation) ||
        (newExposureCompensation > maxExposureCompensation)) {
        CLOGE(" Invalide Exposurecompensation (Min: %d, Max: %d, Value: %d)",
            minExposureCompensation, maxExposureCompensation, newExposureCompensation);
        return BAD_VALUE;
    }

    if (curExposureCompensation != newExposureCompensation) {
        m_setExposureCompensation(newExposureCompensation);
        m_params.set(CameraParameters::KEY_EXPOSURE_COMPENSATION, newExposureCompensation);
    }

    return NO_ERROR;
}

status_t ExynosCamera1Parameters::checkExposureTime(const CameraParameters& params)
{
    int newExposureTime = params.getInt("exposure-time");
    int curExposureTime = m_params.getInt("exposure-time");
    const char *strNewSceneMode = params.get(CameraParameters::KEY_SCENE_MODE);

    if (strNewSceneMode == NULL) {
        return INVALID_OPERATION;
    }

    if (newExposureTime > 0
        && strcmp(strNewSceneMode, CameraParameters::SCENE_MODE_AUTO)) {
        CLOGD("[setParameters]ExposureTime can set only auto scene mode");
        return INVALID_OPERATION;
    }

    CLOGD("[setParameters]newExposureTime : %d", newExposureTime);

    if (newExposureTime < 0)
        return BAD_VALUE;

    if (newExposureTime == curExposureTime)
        return NO_ERROR;

    if (newExposureTime != 0
        && (newExposureTime > getMaxExposureTime() || newExposureTime < getMinExposureTime())) {
        CLOGE("[setParameters]Invalid Exposure Time(%d). minExposureTime(%d), maxExposureTime(%d)",
                newExposureTime, getMaxExposureTime(), getMinExposureTime());

        return BAD_VALUE;
    }

    m_setExposureTime((uint64_t) newExposureTime);
    m_params.set("exposure-time", newExposureTime);

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setExposureTime(uint64_t exposureTime)
{
    m_exposureTimeCapture = exposureTime;

    /* make capture exposuretime micro second */
    setMetaCtlCaptureExposureTime(&m_metadata, (uint32_t)exposureTime);

    /* For showing faster Preview */
    if (exposureTime > CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT)
        exposureTime = CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT;

    /* make exposuretime nano second */
    setMetaCtlExposureTime(&m_metadata, exposureTime * 1000);
}

void ExynosCamera1Parameters::setExposureTime(void)
{
    /* make exposuretime nano second */
    setMetaCtlExposureTime(&m_metadata, getLongExposureTime() * 1000);
}

void ExynosCamera1Parameters::setPreviewExposureTime(void)
{
    ExynosCameraActivityFlash *flashMgr = m_activityControl->getFlashMgr();

    m_setExposureTime(m_exposureTimeCapture);
    flashMgr->setManualExposureTime(0);
}

uint64_t ExynosCamera1Parameters::getPreviewExposureTime(void)
{
    uint64_t exposureTime = 0;

    getMetaCtlExposureTime(&m_metadata, &exposureTime);
    /* make exposuretime milli second */
    exposureTime = exposureTime / 1000;

    return exposureTime;
}

uint64_t ExynosCamera1Parameters::getCaptureExposureTime(void)
{
    return m_exposureTimeCapture;
}

uint64_t ExynosCamera1Parameters::getLongExposureTime(void)
{
#ifndef CAMERA_ADD_BAYER_ENABLE
    if (m_exposureTimeCapture >= CAMERA_EXPOSURE_TIME_MAX) {
        return CAMERA_EXPOSURE_TIME_MAX;
    } else
#endif
    {
        return m_exposureTimeCapture / (getLongExposureShotCount() + 1);
    }
}

int32_t ExynosCamera1Parameters::getLongExposureShotCount(void)
{
    bool getResult;
    int32_t count = 0;
#ifdef CAMERA_ADD_BAYER_ENABLE
    if (m_exposureTimeCapture <= CAMERA_EXPOSURE_TIME_MAX)
#endif
    {
        return 0;
    }

    if (m_exposureTimeCapture % CAMERA_EXPOSURE_TIME_MAX) {
        count = 2;
        getResult = false;
        while (!getResult) {
            if (m_exposureTimeCapture % count) {
                count++;
                continue;
            }
            if (CAMERA_EXPOSURE_TIME_MAX < (m_exposureTimeCapture / count)) {
                count++;
                continue;
            }
            getResult = true;
        }
        return count - 1;
    } else {
        return m_exposureTimeCapture / CAMERA_EXPOSURE_TIME_MAX - 1;
    }
}

int ExynosCamera1Parameters::getBatchSize(enum pipeline pipeId)
{
    int batchSize = 1;
#ifdef HFR_ENABLE
    uint32_t minFps = 0, maxFps = 0;

    getPreviewFpsRange(&minFps, &maxFps);

    /*
     * Default batchSize is MAX(1, maxFps / MULTI_BUFFER_BASE_FPS).
     * If specific pipe has different batchSize,
     * add case with pipeId.
     */
    switch (pipeId) {
    default:
        batchSize = MAX(1, maxFps / MULTI_BUFFER_BASE_FPS);
        break;
    }

    if (pipeId >= PIPE_FLITE_REPROCESSING) {
        /* Reprocessing stream always uses single buffer scheme */
        batchSize = 1;
    }
#endif
    CLOGV("pipeId(%d) batch size(%d)", pipeId, batchSize); 
    return batchSize;
}

status_t ExynosCamera1Parameters::checkMeteringAreas(const CameraParameters& params)
{
    int ret = NO_ERROR;
    const char *newMeteringAreas = params.get(CameraParameters::KEY_METERING_AREAS);
    const char *curMeteringAreas = m_params.get(CameraParameters::KEY_METERING_AREAS);
    const char meteringAreas[20] = "(0,0,0,0,0)";
    bool nullCheckflag = false;

    int newMeteringAreasSize = 0;
    bool isMeteringAreasSame = false;
    uint32_t maxNumMeteringAreas = getMaxNumMeteringAreas();

    if (m_exposureTimeCapture > 0) {
        CLOGD("[setParameters] manual exposure mode. MeteringAreas will not set.");
        return NO_ERROR;
    }

    if (newMeteringAreas == NULL || (newMeteringAreas != NULL && !strcmp(newMeteringAreas, "(0,0,0,0,0)"))) {
        if(getMeteringMode() == METERING_MODE_SPOT) {
            newMeteringAreas = meteringAreas;
            nullCheckflag = true;
        } else {
            setMetaCtlAeRegion(&m_metadata, 0, 0, 0, 0, 0);
            return NO_ERROR;
        }
    }

    if(getSamsungCamera()) {
        maxNumMeteringAreas = 1;
    }

    if (maxNumMeteringAreas <= 0) {
        CLOGD("[Parameters] meterin area is not supported");
        return NO_ERROR;
    }

    CLOGD("[setParameters]newMeteringAreas: %s ,maxNumMeteringAreas(%d)", newMeteringAreas, maxNumMeteringAreas);

    newMeteringAreasSize = strlen(newMeteringAreas);
    if (curMeteringAreas != NULL) {
        isMeteringAreasSame = !strncmp(newMeteringAreas, curMeteringAreas, newMeteringAreasSize);
    }

    if (curMeteringAreas == NULL || isMeteringAreasSame == false || m_flagMeteringRegionChanged == true) {
        /* ex : (-10,-10,0,0,300),(0,0,10,10,700) */
        ExynosRect2 *rect2s  = new ExynosRect2[maxNumMeteringAreas];
        int         *weights = new int[maxNumMeteringAreas];
        uint32_t validMeteringAreas = bracketsStr2Ints((char *)newMeteringAreas, maxNumMeteringAreas, rect2s, weights, 1);

        if (0 < validMeteringAreas && validMeteringAreas <= maxNumMeteringAreas) {
            m_setMeteringAreas((uint32_t)validMeteringAreas, rect2s, weights);
            if(!nullCheckflag) {
                m_params.set(CameraParameters::KEY_METERING_AREAS, newMeteringAreas);
            }
        } else {
            CLOGE("MeteringAreas value is invalid");
            ret = UNKNOWN_ERROR;
        }

        m_flagMeteringRegionChanged = false;
        delete [] rect2s;
        delete [] weights;
    }

    return ret;
}

void ExynosCamera1Parameters::m_setMeteringAreas(uint32_t num, ExynosRect  *rects, int *weights)
{
    ExynosRect2 *rect2s = new ExynosRect2[num];

    for (uint32_t i = 0; i < num; i++)
        convertingRectToRect2(&rects[i], &rect2s[i]);

    m_setMeteringAreas(num, rect2s, weights);

    delete [] rect2s;
}

void ExynosCamera1Parameters::getMeteringAreas(__unused ExynosRect *rects)
{
    /* TODO */
}

void ExynosCamera1Parameters::getMeteringAreas(__unused ExynosRect2 *rect2s)
{
    /* TODO */
}

void ExynosCamera1Parameters::m_setMeteringMode(int meteringMode)
{
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t w = 0;
    uint32_t h = 0;
    uint32_t weight = 0;
    int hwSensorW = 0;
    int hwSensorH = 0;
    enum aa_aemode aeMode;

    if (getAutoExposureLock() == true) {
        CLOGD("autoExposure is Locked");
        return;
    }

    m_cameraInfo.meteringMode = meteringMode;

    getHwSensorSize(&hwSensorW, &hwSensorH);

    switch (meteringMode) {
    case METERING_MODE_AVERAGE:
        aeMode = AA_AEMODE_AVERAGE;
        x = 0;
        y = 0;
        w = hwSensorW;
        h = hwSensorH;
        weight = 1000;
        break;
    case METERING_MODE_MATRIX:
        aeMode = AA_AEMODE_MATRIX;
        x = 0;
        y = 0;
        w = hwSensorW;
        h = hwSensorH;
        weight = 1000;
        break;
    case METERING_MODE_SPOT:
        /* In spot mode, default region setting is 100x100 rectangle on center */
        aeMode = AA_AEMODE_SPOT;
        x = hwSensorW / 2 - 50;
        y = hwSensorH / 2 - 50;
        w = hwSensorW / 2 + 50;
        h = hwSensorH / 2 + 50;
        weight = 50;
        break;
#ifdef TOUCH_AE
    case METERING_MODE_MATRIX_TOUCH:
        aeMode = AA_AEMODE_MATRIX_TOUCH;
        break;
    case METERING_MODE_SPOT_TOUCH:
        aeMode = AA_AEMODE_SPOT_TOUCH;
        break;
    case METERING_MODE_CENTER_TOUCH:
        aeMode = AA_AEMODE_CENTER_TOUCH;
        break;
    case METERING_MODE_AVERAGE_TOUCH:
        aeMode = AA_AEMODE_AVERAGE_TOUCH;
        break;
#endif
    case METERING_MODE_CENTER:
    default:
        aeMode = AA_AEMODE_CENTER;
        x = 0;
        y = 0;
        w = 0;
        h = 0;
        weight = 1000;
        break;
    }

    setMetaCtlAeMode(&m_metadata, aeMode);

    if (getCaptureExposureTime() > 0)
        aeMode = AA_AEMODE_OFF;

    ExynosCameraActivityFlash *m_flashMgr = m_activityControl->getFlashMgr();
    m_flashMgr->setFlashExposure(aeMode);
}

/* Set IonClient */
void ExynosCamera1Parameters::setIonClient(int ionClient)
{
    m_ionClient = ionClient;
}

int ExynosCamera1Parameters::getIonClient(void)
{
    return m_ionClient;
}

int ExynosCamera1Parameters::getMeteringMode(void)
{
    return m_cameraInfo.meteringMode;
}

int ExynosCamera1Parameters::getSupportedMeteringMode(void)
{
    return m_staticInfo->meteringList;
}

status_t ExynosCamera1Parameters::checkAntibanding(const CameraParameters& params)
{
    int newAntibanding = -1;
    int curAntibanding = -1;

    const char *strKeyAntibanding = params.get(CameraParameters::KEY_ANTIBANDING);
#ifdef USE_CSC_FEATURE
    CLOGD("[setParameters]m_antiBanding %s", m_antiBanding);
    const char *strNewAntibanding = m_adjustAntibanding(m_antiBanding);
#else
    const char *strNewAntibanding = m_adjustAntibanding(strKeyAntibanding);
#endif

    if (strNewAntibanding == NULL) {
        return NO_ERROR;
    }
    CLOGD("[setParameters]strNewAntibanding %s", strNewAntibanding);

    if (!strcmp(strNewAntibanding, CameraParameters::ANTIBANDING_AUTO))
        newAntibanding = AA_AE_ANTIBANDING_AUTO;
    else if (!strcmp(strNewAntibanding, CameraParameters::ANTIBANDING_50HZ))
        newAntibanding = AA_AE_ANTIBANDING_AUTO_50HZ;
    else if (!strcmp(strNewAntibanding, CameraParameters::ANTIBANDING_60HZ))
        newAntibanding = AA_AE_ANTIBANDING_AUTO_60HZ;
    else if (!strcmp(strNewAntibanding, CameraParameters::ANTIBANDING_OFF))
        newAntibanding = AA_AE_ANTIBANDING_OFF;
    else {
        CLOGE("Invalid antibanding value(%s)", strNewAntibanding);
        return BAD_VALUE;
    }

    curAntibanding = getAntibanding();

    if (curAntibanding != newAntibanding)
        m_setAntibanding(newAntibanding);

    if (strKeyAntibanding != NULL)
        m_params.set(CameraParameters::KEY_ANTIBANDING, strKeyAntibanding); // HAL test (Ext_SecCameraParametersTest_testSetAntibanding_P01)

    return NO_ERROR;
}

const char *ExynosCamera1Parameters::m_adjustAntibanding(const char *strAntibanding)
{
    const char *strAdjustedAntibanding = NULL;

    strAdjustedAntibanding = strAntibanding;

#if 0 /* fixed the flicker issue when highspeed recording(60fps or 120fps) */
    /* when high speed recording mode, off thre antibanding */
    if (getHighSpeedRecording())
        strAdjustedAntibanding = CameraParameters::ANTIBANDING_OFF;
#endif
    return strAdjustedAntibanding;
}


void ExynosCamera1Parameters::m_setAntibanding(int value)
{
    setMetaCtlAntibandingMode(&m_metadata, (enum aa_ae_antibanding_mode)value);
}

int ExynosCamera1Parameters::getAntibanding(void)
{
    enum aa_ae_antibanding_mode antibanding;
    getMetaCtlAntibandingMode(&m_metadata, &antibanding);
    return (int)antibanding;
}

int ExynosCamera1Parameters::getSupportedAntibanding(void)
{
    return m_staticInfo->antiBandingList;
}

#ifdef USE_CSC_FEATURE
void ExynosCamera1Parameters::m_getAntiBandingFromLatinMCC(char *temp_str)
{
    char value[PROPERTY_VALUE_MAX];
    char country_value[10];

    memset(value, 0x00, sizeof(value));
    memset(country_value, 0x00, sizeof(country_value));
    if (!property_get("gsm.operator.numeric", value,"")) {
        strcpy(temp_str, CameraParameters::ANTIBANDING_60HZ);
        return ;
    }
    memcpy(country_value, value, 3);

    /** MCC Info. Jamaica : 338 / Argentina : 722 / Chile : 730 / Paraguay : 744 / Uruguay : 748  **/
    if (strstr(country_value,"338") || strstr(country_value,"722") || strstr(country_value,"730") || strstr(country_value,"744") || strstr(country_value,"748"))
        strcpy(temp_str, CameraParameters::ANTIBANDING_50HZ);
    else
        strcpy(temp_str, CameraParameters::ANTIBANDING_60HZ);
}

int ExynosCamera1Parameters::m_IsLatinOpenCSC()
{
    char sales_code[PROPERTY_VALUE_MAX] = {0};
    property_get("ro.csc.sales_code", sales_code, "");
    if (strstr(sales_code,"TFG") || strstr(sales_code,"TPA") || strstr(sales_code,"TTT") || strstr(sales_code,"JDI") || strstr(sales_code,"PCI") )
        return 1;
    else
        return 0;
}

void ExynosCamera1Parameters::m_chooseAntiBandingFrequency()
{
    status_t ret = NO_ERROR;
    int LatinOpenCSClength = 5;
    char *LatinOpenCSCstr = NULL;
    char *CSCstr = NULL;
    const char *defaultStr = "50hz";

    if (m_IsLatinOpenCSC()) {
        LatinOpenCSCstr = (char *)malloc(LatinOpenCSClength);
        if (LatinOpenCSCstr == NULL) {
            CLOGE("LatinOpenCSCstr is NULL");
            CSCstr = (char *)defaultStr;
            memset(m_antiBanding, 0, sizeof(m_antiBanding));
            strcpy(m_antiBanding, CSCstr);
            return;
        }
        memset(LatinOpenCSCstr, 0, LatinOpenCSClength);

        m_getAntiBandingFromLatinMCC(LatinOpenCSCstr);
        CSCstr = LatinOpenCSCstr;
    } else {
        CSCstr = (char *)SecNativeFeature::getInstance()->getString("CscFeature_Camera_CameraFlicker");
    }

    if (CSCstr == NULL || strlen(CSCstr) == 0) {
        CSCstr = (char *)defaultStr;
    }

    memset(m_antiBanding, 0, sizeof(m_antiBanding));
    strcpy(m_antiBanding, CSCstr);
    CLOGD("m_antiBanding = %s",m_antiBanding);

    if (LatinOpenCSCstr != NULL) {
        free(LatinOpenCSCstr);
        LatinOpenCSCstr = NULL;
    }
}
#endif

int ExynosCamera1Parameters::getSceneMode(void)
{
    return m_cameraInfo.sceneMode;
}

int ExynosCamera1Parameters::getSupportedSceneModes(void)
{
    return m_staticInfo->sceneModeList;
}

void ExynosCamera1Parameters::setFocusModeLock(bool enable) {
    int curFocusMode = getFocusMode();

    CLOGD("FocusModeLock (%s)", enable? "true" : "false");

    if(enable) {
        m_activityControl->stopAutoFocus();
    } else {
        m_setFocusMode(curFocusMode);
    }
}

void ExynosCamera1Parameters::setFocusModeSetting(bool enable)
{
    m_setFocusmodeSetting = enable;
}

int ExynosCamera1Parameters::getFocusModeSetting(void)
{
    return m_setFocusmodeSetting;
}

int ExynosCamera1Parameters::getFocusMode(void)
{
    return m_cameraInfo.focusMode;
}

int ExynosCamera1Parameters::getSupportedFocusModes(void)
{
    return m_staticInfo->focusModeList;
}

const char *ExynosCamera1Parameters::m_adjustFlashMode(const char *flashMode)
{
    const char *newFlashMode = NULL;

    /* TODO: vendor specific adjust */

    newFlashMode = flashMode;

    return newFlashMode;
}

int ExynosCamera1Parameters::getFlashMode(void)
{
    return m_cameraInfo.flashMode;
}

int ExynosCamera1Parameters::getSupportedFlashModes(void)
{
    return m_staticInfo->flashModeList;
}

status_t ExynosCamera1Parameters::checkWhiteBalanceMode(const CameraParameters& params)
{
    int newWhiteBalance = -1;
    int curWhiteBalance = -1;
    const char *strWhiteBalance = params.get(CameraParameters::KEY_WHITE_BALANCE);
    const char *strNewWhiteBalance = m_adjustWhiteBalanceMode(strWhiteBalance);

    if (strNewWhiteBalance == NULL) {
        return NO_ERROR;
    }

    CLOGD("[setParameters]newWhiteBalance %s", strNewWhiteBalance);

    if (!strcmp(strNewWhiteBalance, CameraParameters::WHITE_BALANCE_AUTO))
        newWhiteBalance = WHITE_BALANCE_AUTO;
    else if (!strcmp(strNewWhiteBalance, CameraParameters::WHITE_BALANCE_INCANDESCENT))
        newWhiteBalance = WHITE_BALANCE_INCANDESCENT;
    else if (!strcmp(strNewWhiteBalance, CameraParameters::WHITE_BALANCE_FLUORESCENT))
        newWhiteBalance = WHITE_BALANCE_FLUORESCENT;
    else if (!strcmp(strNewWhiteBalance, CameraParameters::WHITE_BALANCE_WARM_FLUORESCENT))
        newWhiteBalance = WHITE_BALANCE_WARM_FLUORESCENT;
    else if (!strcmp(strNewWhiteBalance, CameraParameters::WHITE_BALANCE_DAYLIGHT))
        newWhiteBalance = WHITE_BALANCE_DAYLIGHT;
    else if (!strcmp(strNewWhiteBalance, CameraParameters::WHITE_BALANCE_CLOUDY_DAYLIGHT))
        newWhiteBalance = WHITE_BALANCE_CLOUDY_DAYLIGHT;
    else if (!strcmp(strNewWhiteBalance, CameraParameters::WHITE_BALANCE_TWILIGHT))
        newWhiteBalance = WHITE_BALANCE_TWILIGHT;
    else if (!strcmp(strNewWhiteBalance, CameraParameters::WHITE_BALANCE_SHADE))
        newWhiteBalance = WHITE_BALANCE_SHADE;
    else if (!strcmp(strNewWhiteBalance, "temperature"))
        newWhiteBalance = WHITE_BALANCE_CUSTOM_K;
    else {
        CLOGE("Invalid white balance(%s)", strNewWhiteBalance);
        return BAD_VALUE;
    }

    if (!(newWhiteBalance & getSupportedWhiteBalance())) {
        CLOGE(" white balance mode(%s) is not supported", strNewWhiteBalance);
        return BAD_VALUE;
    }

    curWhiteBalance = getWhiteBalanceMode();

    if (getSceneMode() == SCENE_MODE_AUTO) {
        enum aa_awbmode cur_awbMode;
        getMetaCtlAwbMode(&m_metadata, &cur_awbMode);

        if (m_setWhiteBalanceMode(newWhiteBalance) != NO_ERROR)
            return BAD_VALUE;

        m_params.set(CameraParameters::KEY_WHITE_BALANCE, strNewWhiteBalance);
    }
    return NO_ERROR;
}

const char *ExynosCamera1Parameters::m_adjustWhiteBalanceMode(const char *whiteBalance)
{
    const char *newWhiteBalance = NULL;

    /* TODO: vendor specific adjust */

    /* TN' feautre can change whiteBalance even if Non SCENE_MODE_AUTO */

    newWhiteBalance = whiteBalance;

    return newWhiteBalance;
}

status_t ExynosCamera1Parameters::m_setWhiteBalanceMode(int whiteBalance)
{
    enum aa_awbmode awbMode;

    switch (whiteBalance) {
    case WHITE_BALANCE_AUTO:
        awbMode = AA_AWBMODE_WB_AUTO;
        break;
    case WHITE_BALANCE_INCANDESCENT:
        awbMode = AA_AWBMODE_WB_INCANDESCENT;
        break;
    case WHITE_BALANCE_FLUORESCENT:
        awbMode = AA_AWBMODE_WB_FLUORESCENT;
        break;
    case WHITE_BALANCE_DAYLIGHT:
        awbMode = AA_AWBMODE_WB_DAYLIGHT;
        break;
    case WHITE_BALANCE_CLOUDY_DAYLIGHT:
        awbMode = AA_AWBMODE_WB_CLOUDY_DAYLIGHT;
        break;
    case WHITE_BALANCE_WARM_FLUORESCENT:
        awbMode = AA_AWBMODE_WB_WARM_FLUORESCENT;
        break;
    case WHITE_BALANCE_TWILIGHT:
        awbMode = AA_AWBMODE_WB_TWILIGHT;
        break;
    case WHITE_BALANCE_SHADE:
        awbMode = AA_AWBMODE_WB_SHADE;
        break;
    case WHITE_BALANCE_CUSTOM_K:
        awbMode = AA_AWBMODE_WB_CUSTOM_K;
        break;
    default:
        CLOGE("Unsupported value(%d)", whiteBalance);
        return BAD_VALUE;
    }

    m_cameraInfo.whiteBalanceMode = whiteBalance;
    setMetaCtlAwbMode(&m_metadata, awbMode);

    ExynosCameraActivityFlash *m_flashMgr = m_activityControl->getFlashMgr();
    m_flashMgr->setFlashWhiteBalance(awbMode);

    return NO_ERROR;
}

int ExynosCamera1Parameters::m_convertMetaCtlAwbMode(struct camera2_shot_ext *shot_ext)
{
    int awbMode = WHITE_BALANCE_AUTO;

    switch (shot_ext->shot.ctl.aa.awbMode) {
        case AA_AWBMODE_WB_AUTO:
            awbMode = WHITE_BALANCE_AUTO;
            break;
        case AA_AWBMODE_WB_INCANDESCENT:
            awbMode = WHITE_BALANCE_INCANDESCENT;
            break;
        case AA_AWBMODE_WB_FLUORESCENT:
            awbMode = WHITE_BALANCE_FLUORESCENT;
            break;
        case AA_AWBMODE_WB_DAYLIGHT:
            awbMode = WHITE_BALANCE_DAYLIGHT;
            break;
        case AA_AWBMODE_WB_CLOUDY_DAYLIGHT:
            awbMode = WHITE_BALANCE_CLOUDY_DAYLIGHT;
            break;
        case AA_AWBMODE_WB_WARM_FLUORESCENT:
            awbMode = WHITE_BALANCE_WARM_FLUORESCENT;
            break;
        case AA_AWBMODE_WB_TWILIGHT:
            awbMode = WHITE_BALANCE_TWILIGHT;
            break;
        case AA_AWBMODE_WB_SHADE:
            awbMode = WHITE_BALANCE_SHADE;
            break;
        default:
            CLOGE("Unsupported awbMode(%d)", shot_ext->shot.ctl.aa.awbMode);
            return BAD_VALUE;
    }

    return awbMode;
}

int ExynosCamera1Parameters::getWhiteBalanceMode(void)
{
    return m_cameraInfo.whiteBalanceMode;
}

int ExynosCamera1Parameters::getSupportedWhiteBalance(void)
{
    return m_staticInfo->whiteBalanceList;
}

int ExynosCamera1Parameters::getSupportedISO(void)
{
    return m_staticInfo->isoValues;
}

status_t ExynosCamera1Parameters::checkAutoWhiteBalanceLock(const CameraParameters& params)
{
    bool newAutoWhiteBalanceLock = false;
    bool curAutoWhiteBalanceLock = false;
    const char *strNewAutoWhiteBalanceLock = params.get(CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK);

    if (strNewAutoWhiteBalanceLock == NULL) {
        return NO_ERROR;
    }

    CLOGD("[setParameters]strNewAutoWhiteBalanceLock %s", strNewAutoWhiteBalanceLock);

    if (!strcmp(strNewAutoWhiteBalanceLock, "true"))
        newAutoWhiteBalanceLock = true;

    curAutoWhiteBalanceLock = getAutoWhiteBalanceLock();

    if (curAutoWhiteBalanceLock != newAutoWhiteBalanceLock) {
        ExynosCameraActivityFlash *m_flashMgr = m_activityControl->getFlashMgr();
        m_flashMgr->setAwbLock(newAutoWhiteBalanceLock);
        m_setAutoWhiteBalanceLock(newAutoWhiteBalanceLock);
        m_params.set(CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK, strNewAutoWhiteBalanceLock);
    }
    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setAutoWhiteBalanceLock(bool value)
{
    m_cameraInfo.autoWhiteBalanceLock = value;
    setMetaCtlAwbLock(&m_metadata, value);
}

bool ExynosCamera1Parameters::getAutoWhiteBalanceLock(void)
{
    return m_cameraInfo.autoWhiteBalanceLock;
}

#ifdef SAMSUNG_FOOD_MODE
status_t ExynosCamera1Parameters::checkWbLevel(const CameraParameters& params)
{
    int32_t newWbLevel = params.getInt("wb-level");
    int32_t curWbLevel = getWbLevel();
    const char *strNewSceneMode = params.get(CameraParameters::KEY_SCENE_MODE);
    int minWbLevel = -4, maxWbLevel = 4;

    if (strNewSceneMode == NULL) {
        CLOGE("strNewSceneMode is NULL.");
        return NO_ERROR;
    }

    if (!strcmp(strNewSceneMode, "food")) {
        if ((newWbLevel < minWbLevel) || (newWbLevel > maxWbLevel)) {
            CLOGE("Invalid WbLevel (Min: %d, Max: %d, Value: %d)",
                    minWbLevel, maxWbLevel, newWbLevel);
            return BAD_VALUE;
        }

        CLOGD("[setParameters]newWbLevel %d", newWbLevel);

        if (curWbLevel != newWbLevel) {
            m_setWbLevel(newWbLevel);
            m_params.set("wb-level", newWbLevel);
        }
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setWbLevel(int32_t value)
{
    setMetaCtlWbLevel(&m_metadata, value + IS_WBLEVEL_DEFAULT + FW_CUSTOM_OFFSET);
}

int32_t ExynosCamera1Parameters::getWbLevel(void)
{
    int32_t wbLevel;

    getMetaCtlWbLevel(&m_metadata, &wbLevel);

    return wbLevel - IS_WBLEVEL_DEFAULT - FW_CUSTOM_OFFSET;
}
#endif

status_t ExynosCamera1Parameters::checkWbKelvin(const CameraParameters& params)
{
    int32_t newWbKelvin = params.getInt("wb-k");
    int32_t curWbKelvin = getWbKelvin();
    const char *strNewSceneMode = params.get(CameraParameters::KEY_SCENE_MODE);
    const char *strWhiteBalance = params.get(CameraParameters::KEY_WHITE_BALANCE);
    const char *strNewWhiteBalance = m_adjustWhiteBalanceMode(strWhiteBalance);

    if (strNewSceneMode == NULL || strNewWhiteBalance == NULL) {
        CLOGE("strNewSceneMode or strNewWhiteBalance is NULL.");
        return NO_ERROR;
    }

    if (strcmp(strNewSceneMode, "food") && !strcmp(strNewWhiteBalance, "temperature")) {
        if ((newWbKelvin < getMinWBK()) || (newWbKelvin > getMaxWBK())) {
            CLOGE("Invalid WbKelvin (Min: %d, Max: %d, Value: %d)",
                    getMinWBK(), getMaxWBK(), newWbKelvin);
            return BAD_VALUE;
        }

        CLOGD("[setParameters]newWbKelvin %d", newWbKelvin);

        if (curWbKelvin != newWbKelvin) {
            m_setWbKelvin(newWbKelvin);
            m_params.set("wb-k", newWbKelvin);
        }
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setWbKelvin(int32_t value)
{
    setMetaCtlWbLevel(&m_metadata, value);
}

int32_t ExynosCamera1Parameters::getWbKelvin(void)
{
    int32_t wbKelvin = 0;

    getMetaCtlWbLevel(&m_metadata, &wbKelvin);

    return wbKelvin;
}

void ExynosCamera1Parameters::m_setFocusAreas(uint32_t numValid, ExynosRect *rects, int *weights)
{
    ExynosRect2 *rect2s = new ExynosRect2[numValid];

    for (uint32_t i = 0; i < numValid; i++)
        convertingRectToRect2(&rects[i], &rect2s[i]);

    m_setFocusAreas(numValid, rect2s, weights);

    delete [] rect2s;
}

void ExynosCamera1Parameters::getFocusAreas(int *validFocusArea, ExynosRect2 *rect2s, int *weights)
{
    *validFocusArea = m_cameraInfo.numValidFocusArea;

    if (*validFocusArea != 0) {
        /* Currently only supported 1 region */
        getMetaCtlAfRegion(&m_metadata, &rect2s->x1, &rect2s->y1,
                            &rect2s->x2, &rect2s->y2, weights);
    }
}

status_t ExynosCamera1Parameters::checkColorEffectMode(const CameraParameters& params)
{
    int newEffectMode = EFFECT_NONE;
    int curEffectMode = EFFECT_NONE;
    const char *strNewEffectMode = params.get(CameraParameters::KEY_EFFECT);

    if (strNewEffectMode == NULL) {
        return NO_ERROR;
    }

    CLOGD("[setParameters]strNewEffectMode %s", strNewEffectMode);

    if (!strcmp(strNewEffectMode, CameraParameters::EFFECT_NONE)) {
        newEffectMode = EFFECT_NONE;
    } else if (!strcmp(strNewEffectMode, CameraParameters::EFFECT_MONO)) {
        newEffectMode = EFFECT_MONO;
    } else if (!strcmp(strNewEffectMode, CameraParameters::EFFECT_NEGATIVE)) {
        newEffectMode = EFFECT_NEGATIVE;
    } else if (!strcmp(strNewEffectMode, CameraParameters::EFFECT_SOLARIZE)) {
        newEffectMode = EFFECT_SOLARIZE;
    } else if (!strcmp(strNewEffectMode, CameraParameters::EFFECT_SEPIA)) {
        newEffectMode = EFFECT_SEPIA;
    } else if (!strcmp(strNewEffectMode, CameraParameters::EFFECT_POSTERIZE)) {
        newEffectMode = EFFECT_POSTERIZE;
    } else if (!strcmp(strNewEffectMode, CameraParameters::EFFECT_WHITEBOARD)) {
        newEffectMode = EFFECT_WHITEBOARD;
    } else if (!strcmp(strNewEffectMode, CameraParameters::EFFECT_BLACKBOARD)) {
        newEffectMode = EFFECT_BLACKBOARD;
    } else if (!strcmp(strNewEffectMode, CameraParameters::EFFECT_AQUA)) {
        newEffectMode = EFFECT_AQUA;
#ifdef SAMSUNG_TN_FEATURE
    } else if (!strcmp(strNewEffectMode, CameraParameters::EFFECT_POINT_BLUE)) {
        newEffectMode = EFFECT_BLUE;
#endif
    } else if (!strcmp(strNewEffectMode, "point-red-yellow")) {
        newEffectMode = EFFECT_RED_YELLOW;
    } else if (!strcmp(strNewEffectMode, "vintage-cold")) {
        newEffectMode = EFFECT_COLD_VINTAGE;
    } else if (!strcmp(strNewEffectMode, "beauty" )) {
        newEffectMode = EFFECT_BEAUTY_FACE;
    } else {
        CLOGE("Invalid effect(%s)", strNewEffectMode);
        return BAD_VALUE;
    }

    if (!isSupportedColorEffects(newEffectMode)) {
        CLOGE(" Effect mode(%s) is not supported!", strNewEffectMode);
        return BAD_VALUE;
    }

    curEffectMode = getColorEffectMode();

    if (curEffectMode != newEffectMode) {
        m_setColorEffectMode(newEffectMode);
        m_params.set(CameraParameters::KEY_EFFECT, strNewEffectMode);

        m_frameSkipCounter.setCount(EFFECT_SKIP_FRAME);
    }
    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setColorEffectMode(int effect)
{
    aa_effect_mode_t newEffect;

    switch(effect) {
    case EFFECT_NONE:
        newEffect = AA_EFFECT_OFF;
        break;
    case EFFECT_MONO:
        newEffect = AA_EFFECT_MONO;
        break;
    case EFFECT_NEGATIVE:
        newEffect = AA_EFFECT_NEGATIVE;
        break;
    case EFFECT_SOLARIZE:
        newEffect = AA_EFFECT_SOLARIZE;
        break;
    case EFFECT_SEPIA:
        newEffect = AA_EFFECT_SEPIA;
        break;
    case EFFECT_POSTERIZE:
        newEffect = AA_EFFECT_POSTERIZE;
        break;
    case EFFECT_WHITEBOARD:
        newEffect = AA_EFFECT_WHITEBOARD;
        break;
    case EFFECT_BLACKBOARD:
        newEffect = AA_EFFECT_BLACKBOARD;
        break;
    case EFFECT_AQUA:
        newEffect = AA_EFFECT_AQUA;
        break;
    case EFFECT_RED_YELLOW:
        newEffect = AA_EFFECT_RED_YELLOW_POINT;
        break;
    case EFFECT_BLUE:
        newEffect = AA_EFFECT_BLUE_POINT;
        break;
    case EFFECT_WARM_VINTAGE:
        newEffect = AA_EFFECT_WARM_VINTAGE;
        break;
    case EFFECT_COLD_VINTAGE:
        newEffect = AA_EFFECT_COLD_VINTAGE;
        break;
    case EFFECT_BEAUTY_FACE:
        newEffect = AA_EFFECT_BEAUTY_FACE;
        break;
    default:
        newEffect = AA_EFFECT_OFF;
        CLOGE("Color Effect mode(%d) is not supported", effect);
        break;
    }
    setMetaCtlAaEffect(&m_metadata, newEffect);
}

int ExynosCamera1Parameters::getColorEffectMode(void)
{
    aa_effect_mode_t curEffect;
    int effect;

    getMetaCtlAaEffect(&m_metadata, &curEffect);

    switch(curEffect) {
    case AA_EFFECT_OFF:
        effect = EFFECT_NONE;
        break;
    case AA_EFFECT_MONO:
        effect = EFFECT_MONO;
        break;
    case AA_EFFECT_NEGATIVE:
        effect = EFFECT_NEGATIVE;
        break;
    case AA_EFFECT_SOLARIZE:
        effect = EFFECT_SOLARIZE;
        break;
    case AA_EFFECT_SEPIA:
        effect = EFFECT_SEPIA;
        break;
    case AA_EFFECT_POSTERIZE:
        effect = EFFECT_POSTERIZE;
        break;
    case AA_EFFECT_WHITEBOARD:
        effect = EFFECT_WHITEBOARD;
        break;
    case AA_EFFECT_BLACKBOARD:
        effect = EFFECT_BLACKBOARD;
        break;
    case AA_EFFECT_AQUA:
        effect = EFFECT_AQUA;
        break;
    case AA_EFFECT_RED_YELLOW_POINT:
        effect = EFFECT_RED_YELLOW;
        break;
    case AA_EFFECT_BLUE_POINT:
        effect = EFFECT_BLUE;
        break;
    case AA_EFFECT_WARM_VINTAGE:
        effect = EFFECT_WARM_VINTAGE;
        break;
    case AA_EFFECT_COLD_VINTAGE:
        effect = EFFECT_COLD_VINTAGE;
        break;
    case AA_EFFECT_BEAUTY_FACE:
        effect = EFFECT_BEAUTY_FACE;
        break;
    default:
        effect = 0;
        CLOGE("Color Effect mode(%d) is invalid value", curEffect);
        break;
    }

    return effect;
}

int ExynosCamera1Parameters::getSupportedColorEffects(void)
{
    return m_staticInfo->effectList;
}

bool ExynosCamera1Parameters::isSupportedColorEffects(int effectMode)
{
    int ret = false;

    if (effectMode & getSupportedColorEffects()) {
        return true;
    }

    if (effectMode & m_staticInfo->hiddenEffectList) {
        return true;
    }

    return ret;
}

status_t ExynosCamera1Parameters::checkGpsAltitude(const CameraParameters& params)
{
    double newAltitude = 0;
    double curAltitude = 0;
    const char *strNewGpsAltitude = params.get(CameraParameters::KEY_GPS_ALTITUDE);

    if (strNewGpsAltitude == NULL) {
        m_params.remove(CameraParameters::KEY_GPS_ALTITUDE);
        m_setGpsAltitude(0);
        return NO_ERROR;
    }

    CLOGV("[setParameters]strNewGpsAltitude %s", strNewGpsAltitude);

    newAltitude = atof(strNewGpsAltitude);
    curAltitude = getGpsAltitude();

    if (curAltitude != newAltitude) {
        m_setGpsAltitude(newAltitude);
        m_params.set(CameraParameters::KEY_GPS_ALTITUDE, strNewGpsAltitude);
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setGpsAltitude(double altitude)
{
    m_cameraInfo.gpsAltitude = altitude;
}

double ExynosCamera1Parameters::getGpsAltitude(void)
{
    return m_cameraInfo.gpsAltitude;
}

status_t ExynosCamera1Parameters::checkGpsLatitude(const CameraParameters& params)
{
    double newLatitude = 0;
    double curLatitude = 0;
    const char *strNewGpsLatitude = params.get(CameraParameters::KEY_GPS_LATITUDE);

    if (strNewGpsLatitude == NULL) {
        m_params.remove(CameraParameters::KEY_GPS_LATITUDE);
        m_setGpsLatitude(0);
        return NO_ERROR;
    }

    CLOGV("[setParameters]strNewGpsLatitude %s", strNewGpsLatitude);

    newLatitude = atof(strNewGpsLatitude);
    curLatitude = getGpsLatitude();

    if (curLatitude != newLatitude) {
        m_setGpsLatitude(newLatitude);
        m_params.set(CameraParameters::KEY_GPS_LATITUDE, strNewGpsLatitude);
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setGpsLatitude(double latitude)
{
    m_cameraInfo.gpsLatitude = latitude;
}

double ExynosCamera1Parameters::getGpsLatitude(void)
{
    return m_cameraInfo.gpsLatitude;
}

status_t ExynosCamera1Parameters::checkGpsLongitude(const CameraParameters& params)
{
    double newLongitude = 0;
    double curLongitude = 0;
    const char *strNewGpsLongitude = params.get(CameraParameters::KEY_GPS_LONGITUDE);

    if (strNewGpsLongitude == NULL) {
        m_params.remove(CameraParameters::KEY_GPS_LONGITUDE);
        m_setGpsLongitude(0);
        return NO_ERROR;
    }

    CLOGV("[setParameters]strNewGpsLongitude %s", strNewGpsLongitude);

    newLongitude = atof(strNewGpsLongitude);
    curLongitude = getGpsLongitude();

    if (curLongitude != newLongitude) {
        m_setGpsLongitude(newLongitude);
        m_params.set(CameraParameters::KEY_GPS_LONGITUDE, strNewGpsLongitude);
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setGpsLongitude(double longitude)
{
    m_cameraInfo.gpsLongitude = longitude;
}

double ExynosCamera1Parameters::getGpsLongitude(void)
{
    return m_cameraInfo.gpsLongitude;
}

status_t ExynosCamera1Parameters::checkGpsProcessingMethod(const CameraParameters& params)
{
    // gps processing method
    const char *strNewGpsProcessingMethod = params.get(CameraParameters::KEY_GPS_PROCESSING_METHOD);
    const char *strCurGpsProcessingMethod = NULL;
    bool changeMethod = false;

    if (strNewGpsProcessingMethod == NULL) {
        m_params.remove(CameraParameters::KEY_GPS_PROCESSING_METHOD);
        m_setGpsProcessingMethod(NULL);
        return NO_ERROR;
    }

    CLOGD("[setParameters]strNewGpsProcessingMethod %s", strNewGpsProcessingMethod);

    strCurGpsProcessingMethod = getGpsProcessingMethod();

    if (strCurGpsProcessingMethod != NULL) {
        int newLen = strlen(strNewGpsProcessingMethod);
        int curLen = strlen(strCurGpsProcessingMethod);

        if (newLen != curLen)
            changeMethod = true;
        else
            changeMethod = strncmp(strNewGpsProcessingMethod, strCurGpsProcessingMethod, newLen);
    }

    if (changeMethod == true) {
        m_setGpsProcessingMethod(strNewGpsProcessingMethod);
        m_params.set(CameraParameters::KEY_GPS_PROCESSING_METHOD, strNewGpsProcessingMethod);
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setGpsProcessingMethod(const char *gpsProcessingMethod)
{
    memset(m_exifInfo.gps_processing_method, 0, sizeof(m_exifInfo.gps_processing_method));
    if (gpsProcessingMethod == NULL)
        return;

    size_t len = strlen(gpsProcessingMethod);

    if (len > sizeof(m_exifInfo.gps_processing_method)) {
        len = sizeof(m_exifInfo.gps_processing_method);
    }
    memcpy(m_exifInfo.gps_processing_method, gpsProcessingMethod, len);
}

const char *ExynosCamera1Parameters::getGpsProcessingMethod(void)
{
    return (const char *)m_exifInfo.gps_processing_method;
}

void ExynosCamera1Parameters::m_setExifFixedAttribute(void)
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

    /* 3 Exif Version */
    memcpy(m_exifInfo.exif_version, EXIF_DEF_EXIF_VERSION, sizeof(m_exifInfo.exif_version));
    /* 3 Aperture */
    m_exifInfo.aperture.num = m_staticInfo->apertureNum;
    m_exifInfo.aperture.den = m_staticInfo->apertureDen;
    /* 3 F Number */
    m_exifInfo.fnumber.num = m_staticInfo->fNumberNum;
    m_exifInfo.fnumber.den = m_staticInfo->fNumberDen;
    /* 3 Maximum lens aperture */
    m_exifInfo.max_aperture.num = m_staticInfo->apertureNum;
    m_exifInfo.max_aperture.den = m_staticInfo->apertureDen;
    /* 3 Lens Focal Length */
    m_exifInfo.focal_length.num = m_staticInfo->focalLengthNum;
    m_exifInfo.focal_length.den = m_staticInfo->focalLengthDen;
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

void ExynosCamera1Parameters::setExifChangedAttribute(exif_attribute_t   *exifInfo,
                                                     ExynosRect         *pictureRect,
                                                     ExynosRect         *thumbnailRect,
                                                     camera2_shot_t     *shot,
                                                     bool               useDebugInfo2
#ifdef SAMSUNG_DUAL_CAPTURE_SOLUTION
                                                     , camera2_shot_t     *shot2
#endif
                                                     )
{
#ifdef SAMSUNG_DUAL_CAPTURE_SOLUTION
    if (shot2)
        m_setExifChangedAttribute(exifInfo, pictureRect, thumbnailRect, &(shot->dm), &(shot->udm),
                                            &(shot2->dm), &(shot2->udm), useDebugInfo2);
    else
        m_setExifChangedAttribute(exifInfo, pictureRect, thumbnailRect, &(shot->dm), &(shot->udm),
                                            NULL, NULL, useDebugInfo2);
#else
    m_setExifChangedAttribute(exifInfo, pictureRect, thumbnailRect, &(shot->dm), &(shot->udm), useDebugInfo2);
#endif
}

debug_attribute_t *ExynosCamera1Parameters::getDebugAttribute(void)
{
    return &mDebugInfo;
}

status_t ExynosCamera1Parameters::getFixedExifInfo(exif_attribute_t *exifInfo)
{
    if (exifInfo == NULL) {
        CLOGE(" buffer is NULL");
        return BAD_VALUE;
    }

    memcpy(exifInfo, &m_exifInfo, sizeof(exif_attribute_t));

    return NO_ERROR;
}

status_t ExynosCamera1Parameters::checkGpsTimeStamp(const CameraParameters& params)
{
    long newGpsTimeStamp = -1;
    long curGpsTimeStamp = -1;
    const char *strNewGpsTimeStamp = params.get(CameraParameters::KEY_GPS_TIMESTAMP);

    if (strNewGpsTimeStamp == NULL) {
        m_params.remove(CameraParameters::KEY_GPS_TIMESTAMP);
        m_setGpsTimeStamp(0);
        return NO_ERROR;
    }

    CLOGD("[setParameters]strNewGpsTimeStamp %s", strNewGpsTimeStamp);

    newGpsTimeStamp = atol(strNewGpsTimeStamp);

    curGpsTimeStamp = getGpsTimeStamp();

    if (curGpsTimeStamp != newGpsTimeStamp) {
        m_setGpsTimeStamp(newGpsTimeStamp);
        m_params.set(CameraParameters::KEY_GPS_TIMESTAMP, strNewGpsTimeStamp);
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setGpsTimeStamp(long timeStamp)
{
    m_cameraInfo.gpsTimeStamp = timeStamp;
}

long ExynosCamera1Parameters::getGpsTimeStamp(void)
{
    return m_cameraInfo.gpsTimeStamp;
}

/*
 * Additional API.
 */

status_t ExynosCamera1Parameters::checkBrightness(const CameraParameters& params)
{
    int maxBrightness = params.getInt("brightness-max");
    int minBrightness = params.getInt("brightness-min");
    int newBrightness = params.getInt("brightness");
    int curBrightness = -1;
    int curEffectMode = EFFECT_NONE;

    CLOGD("[setParameters]newBrightness %d", newBrightness);

    if (newBrightness < minBrightness || newBrightness > maxBrightness) {
        CLOGE(" Invalid Brightness (Min: %d, Max: %d, Value: %d)", minBrightness, maxBrightness, newBrightness);
        return BAD_VALUE;
    }

    curEffectMode = getColorEffectMode();
    if(curEffectMode == EFFECT_BEAUTY_FACE) {
        return NO_ERROR;
    }

    curBrightness = getBrightness();

    if (curBrightness != newBrightness) {
        m_setBrightness(newBrightness);
        m_params.set("brightness", newBrightness);
    }
    return NO_ERROR;
}

/* F/W's middle value is 3, and step is -2, -1, 0, 1, 2 */
void ExynosCamera1Parameters::m_setBrightness(int brightness)
{
    setMetaCtlBrightness(&m_metadata, brightness + IS_BRIGHTNESS_DEFAULT + FW_CUSTOM_OFFSET);
}

int ExynosCamera1Parameters::getBrightness(void)
{
    int32_t brightness = 0;

    getMetaCtlBrightness(&m_metadata, &brightness);
    return brightness - IS_BRIGHTNESS_DEFAULT - FW_CUSTOM_OFFSET;
}

status_t ExynosCamera1Parameters::checkSaturation(const CameraParameters& params)
{
    int maxSaturation = params.getInt("saturation-max");
    int minSaturation = params.getInt("saturation-min");
    int newSaturation = params.getInt("saturation");
    int curSaturation = -1;

    CLOGD("[setParameters]newSaturation %d", newSaturation);

    if (newSaturation < minSaturation || newSaturation > maxSaturation) {
        CLOGE(" Invalid Saturation (Min: %d, Max: %d, Value: %d)", minSaturation, maxSaturation, newSaturation);
        return BAD_VALUE;
    }

    curSaturation = getSaturation();

    if (curSaturation != newSaturation) {
        m_setSaturation(newSaturation);
        m_params.set("saturation", newSaturation);
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setSaturation(int saturation)
{
    setMetaCtlSaturation(&m_metadata, saturation + IS_SATURATION_DEFAULT + FW_CUSTOM_OFFSET);
}

int ExynosCamera1Parameters::getSaturation(void)
{
    int32_t saturation = 0;

    getMetaCtlSaturation(&m_metadata, &saturation);
    return saturation - IS_SATURATION_DEFAULT - FW_CUSTOM_OFFSET;
}

status_t ExynosCamera1Parameters::checkSharpness(const CameraParameters& params)
{
    int maxSharpness = params.getInt("sharpness-max");
    int minSharpness = params.getInt("sharpness-min");
    int newSharpness = params.getInt("sharpness");
    int curSharpness = -1;
    int curEffectMode = EFFECT_NONE;

    CLOGD("[setParameters]newSharpness %d", newSharpness);

    if (newSharpness < minSharpness || newSharpness > maxSharpness) {
        CLOGE(" Invalid Sharpness (Min: %d, Max: %d, Value: %d)", minSharpness, maxSharpness, newSharpness);
        return BAD_VALUE;
    }

    curEffectMode = getColorEffectMode();
    if(curEffectMode == EFFECT_BEAUTY_FACE
#if 0
//#ifdef SAMSUNG_FOOD_MODE
        || (getSceneMode() == SCENE_MODE_FOOD)
#endif
        ) {
        return NO_ERROR;
    }

    curSharpness = getSharpness();

    if (curSharpness != newSharpness) {
        m_setSharpness(newSharpness);
        m_params.set("sharpness", newSharpness);
    }
    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setSharpness(int sharpness)
{
    enum processing_mode default_edge_mode = PROCESSING_MODE_FAST;
    enum processing_mode default_noise_mode = PROCESSING_MODE_FAST;
    int default_edge_strength = 5;
    int default_noise_strength = 5;

    int newSharpness = sharpness + IS_SHARPNESS_DEFAULT;
    enum processing_mode edge_mode = default_edge_mode;
    enum processing_mode noise_mode = default_noise_mode;
    int edge_strength = default_edge_strength;
    int noise_strength = default_noise_strength;

    switch (newSharpness) {
        case IS_SHARPNESS_MINUS_2:
            edge_mode = PROCESSING_MODE_MANUAL;
            noise_mode = PROCESSING_MODE_MANUAL;
            edge_strength = default_edge_strength;
            noise_strength = 10;
            break;
        case IS_SHARPNESS_MINUS_1:
            edge_mode = PROCESSING_MODE_MANUAL;
            noise_mode = PROCESSING_MODE_MANUAL;
            edge_strength = default_edge_strength;
            noise_strength = (10 + default_noise_strength + 1) / 2;
            break;
        case IS_SHARPNESS_DEFAULT:
            edge_mode = default_edge_mode;
            noise_mode = default_noise_mode;
            edge_strength = default_edge_strength;
            noise_strength = default_noise_strength;
            break;
        case IS_SHARPNESS_PLUS_1:
            edge_mode = PROCESSING_MODE_MANUAL;
            noise_mode = PROCESSING_MODE_MANUAL;
            edge_strength = (10 + default_edge_strength + 1) / 2;
            noise_strength = default_noise_strength;
            break;
        case IS_SHARPNESS_PLUS_2:
            edge_mode = PROCESSING_MODE_MANUAL;
            noise_mode = PROCESSING_MODE_MANUAL;
            edge_strength = 10;
            noise_strength = default_noise_strength;
            break;
        default:
            break;
    }

    CLOGD("newSharpness %d edge_mode(%d),st(%d), noise(%d),st(%d)",
            newSharpness, edge_mode, edge_strength, noise_mode, noise_strength);

    setMetaCtlSharpness(&m_metadata, edge_mode, edge_strength, noise_mode, noise_strength);
}

int ExynosCamera1Parameters::getSharpness(void)
{
    enum processing_mode default_edge_mode = PROCESSING_MODE_FAST;
    enum processing_mode default_noise_mode = PROCESSING_MODE_FAST;
    int default_edge_sharpness = 5;
    int default_noise_sharpness = 5;

    int32_t edge_sharpness = default_edge_sharpness;
    int32_t noise_sharpness = default_noise_sharpness;
    int32_t sharpness = 0;
    enum processing_mode edge_mode = default_edge_mode;
    enum processing_mode noise_mode = default_noise_mode;

    getMetaCtlSharpness(&m_metadata, &edge_mode, &edge_sharpness, &noise_mode, &noise_sharpness);

    if(noise_sharpness == 10 && edge_sharpness == default_edge_sharpness) {
        sharpness = IS_SHARPNESS_MINUS_2;
    } else if(noise_sharpness == (10 + default_noise_sharpness + 1) / 2
            && edge_sharpness == default_edge_sharpness) {
        sharpness = IS_SHARPNESS_MINUS_1;
    } else if(noise_sharpness == default_noise_sharpness
            && edge_sharpness == default_edge_sharpness) {
        sharpness = IS_SHARPNESS_DEFAULT;
    } else if(noise_sharpness == default_noise_sharpness
            && edge_sharpness == (10 + default_edge_sharpness + 1) / 2) {
        sharpness = IS_SHARPNESS_PLUS_1;
    } else if(noise_sharpness == default_noise_sharpness
            && edge_sharpness == 10) {
        sharpness = IS_SHARPNESS_PLUS_2;
    } else {
        sharpness = IS_SHARPNESS_DEFAULT;
    }

    return sharpness - IS_SHARPNESS_DEFAULT;
}

status_t ExynosCamera1Parameters::checkHue(const CameraParameters& params)
{
    int maxHue = params.getInt("hue-max");
    int minHue = params.getInt("hue-min");
    int newHue = params.getInt("hue");
    int curHue = -1;

    CLOGD("[setParameters]newHue %d", newHue);

    if (newHue < minHue || newHue > maxHue) {
        CLOGE(" Invalid Hue (Min: %d, Max: %d, Value: %d)", minHue, maxHue, newHue);
        return BAD_VALUE;
    }

    curHue = getHue();

    if (curHue != newHue) {
        m_setHue(newHue);
        m_params.set("hue", newHue);
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setHue(int hue)
{
    setMetaCtlHue(&m_metadata, hue + IS_HUE_DEFAULT + FW_CUSTOM_OFFSET);
}

int ExynosCamera1Parameters::getHue(void)
{
    int32_t hue = 0;

    getMetaCtlHue(&m_metadata, &hue);
    return hue - IS_HUE_DEFAULT - FW_CUSTOM_OFFSET;
}

status_t ExynosCamera1Parameters::checkIso(const CameraParameters& params)
{
    uint32_t newISO = 0;
    uint32_t curISO = 0;
    const char *strNewISO = params.get("iso");

    if (strNewISO == NULL) {
        return NO_ERROR;
    }

    CLOGD("[setParameters]strNewISO %s", strNewISO);

    if (!strcmp(strNewISO, "auto")) {
        newISO = 0;
    } else {
        newISO = (uint32_t)atoi(strNewISO);
        if (newISO == 0) {
            CLOGE("Invalid iso value(%s)", strNewISO);
            return BAD_VALUE;
        }
    }

    curISO = getIso();

    if (curISO != newISO) {
        m_setIso(newISO);
        m_params.set("iso", strNewISO);
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setIso(uint32_t iso)
{
    enum aa_isomode mode = AA_ISOMODE_AUTO;

    if (iso == 0)
        mode = AA_ISOMODE_AUTO;
    else
        mode = AA_ISOMODE_MANUAL;

    setMetaCtlIso(&m_metadata, mode, iso);
}

uint32_t ExynosCamera1Parameters::getIso(void)
{
    enum aa_isomode mode = AA_ISOMODE_AUTO;
    uint32_t iso = 0;

    getMetaCtlIso(&m_metadata, &mode, &iso);

    return iso;
}

status_t ExynosCamera1Parameters::checkContrast(const CameraParameters& params)
{
    uint32_t newContrast = 0;
    uint32_t curContrast = 0;
    const char *strNewContrast = params.get("contrast");

    if (strNewContrast == NULL) {
        return NO_ERROR;
    }

    CLOGD("[setParameters]strNewContrast %s", strNewContrast);

    if (!strcmp(strNewContrast, "auto"))
        newContrast = IS_CONTRAST_DEFAULT;
    else if (!strcmp(strNewContrast, "-2"))
        newContrast = IS_CONTRAST_MINUS_2;
    else if (!strcmp(strNewContrast, "-1"))
        newContrast = IS_CONTRAST_MINUS_1;
    else if (!strcmp(strNewContrast, "0"))
        newContrast = IS_CONTRAST_DEFAULT;
    else if (!strcmp(strNewContrast, "1"))
        newContrast = IS_CONTRAST_PLUS_1;
    else if (!strcmp(strNewContrast, "2"))
        newContrast = IS_CONTRAST_PLUS_2;
    else {
        CLOGE("Invalid contrast value(%s)", strNewContrast);
        return BAD_VALUE;
    }

    curContrast = getContrast();

    if (curContrast != newContrast) {
        m_setContrast(newContrast);
        m_params.set("contrast", strNewContrast);
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setContrast(uint32_t contrast)
{
    setMetaCtlContrast(&m_metadata, contrast);
}

uint32_t ExynosCamera1Parameters::getContrast(void)
{
    uint32_t contrast = 0;
    getMetaCtlContrast(&m_metadata, &contrast);
    return contrast;
}

status_t ExynosCamera1Parameters::checkHdrMode(const CameraParameters& params)
{
    int newHDR = params.getInt("hdr-mode");
    bool curHDR = -1;

    if (newHDR < 0) {
        return NO_ERROR;
    }

    CLOGD("[setParameters]newHDR %d", newHDR);

    curHDR = getHdrMode();

    if (curHDR != (bool)newHDR) {
        m_setHdrMode((bool)newHDR);
        m_params.set("hdr-mode", newHDR);
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setHdrMode(bool hdr)
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

bool ExynosCamera1Parameters::getHdrMode(void)
{
    return m_cameraInfo.hdrMode;
}

status_t ExynosCamera1Parameters::checkWdrMode(const CameraParameters& params)
{
    int newWDR = params.getInt("wdr");
    bool curWDR = -1;

    if (newWDR < 0) {
        return NO_ERROR;
    }

    CLOGD("[setParameters]newWDR %d", newWDR);

    curWDR = getWdrMode();

    if (curWDR != (bool)newWDR) {
        m_setWdrMode((bool)newWDR);
        m_params.set("wdr", newWDR);
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setWdrMode(bool wdr)
{
    m_cameraInfo.wdrMode = wdr;
}

bool ExynosCamera1Parameters::getWdrMode(void)
{
    return m_cameraInfo.wdrMode;
}

int ExynosCamera1Parameters::getPreviewBufferCount(void)
{
    CLOGV("[setParameters]getPreviewBufferCount %d", m_previewBufferCount);

    return m_previewBufferCount;
}

void ExynosCamera1Parameters::setPreviewBufferCount(int previewBufferCount)
{
    m_previewBufferCount = previewBufferCount;

    CLOGV("[setParameters]setPreviewBufferCount %d", m_previewBufferCount);

    return;
}

status_t ExynosCamera1Parameters::checkAntiShake(const CameraParameters& params)
{
    int newAntiShake = params.getInt("anti-shake");
    bool curAntiShake = false;
    bool toggle = false;
    int curShotMode = getShotMode();

    if (curShotMode != SHOT_MODE_AUTO)
        return NO_ERROR;

    if (newAntiShake < 0) {
        return NO_ERROR;
    }

    CLOGD("[setParameters]newAntiShake %d", newAntiShake);

    if (newAntiShake == 1)
        toggle = true;

    curAntiShake = getAntiShake();

    if (curAntiShake != toggle) {
        m_setAntiShake(toggle);
        m_params.set("anti-shake", newAntiShake);
    }

    return NO_ERROR;
}

void ExynosCamera1Parameters::m_setAntiShake(bool toggle)
{
    enum aa_mode mode = AA_CONTROL_AUTO;
    enum aa_scene_mode sceneMode = AA_SCENE_MODE_FACE_PRIORITY;

    if (toggle == true) {
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_ANTISHAKE;
    }

    setMetaCtlSceneMode(&m_metadata, mode, sceneMode);
    m_cameraInfo.antiShake = toggle;
}

bool ExynosCamera1Parameters::getAntiShake(void)
{
    return m_cameraInfo.antiShake;
}

status_t ExynosCamera1Parameters::checkScalableSensorMode(const CameraParameters& params)
{
    bool needScaleMode = false;
    bool curScaleMode = false;
    int newScaleMode = params.getInt("scale_mode");

    if (newScaleMode < 0) {
        return NO_ERROR;
    }

    CLOGD("[setParameters]newScaleMode %d", newScaleMode);

    if (isScalableSensorSupported() == true) {
        needScaleMode = m_adjustScalableSensorMode(newScaleMode);
        curScaleMode = getScalableSensorMode();

        if (curScaleMode != needScaleMode) {
            setScalableSensorMode(needScaleMode);
            m_params.set("scale_mode", newScaleMode);
        }

//        updateHwSensorSize();
    }

    return NO_ERROR;
}

bool ExynosCamera1Parameters::isScalableSensorSupported(void)
{
    return m_staticInfo->scalableSensorSupport;
}

bool ExynosCamera1Parameters::m_adjustScalableSensorMode(const int scaleMode)
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


    if (getDualMode() == true
#ifdef USE_LIVE_BROADCAST_DUAL
        && (plb_mode != true || (plb_mode == true && camera_id == CAMERA_ID_FRONT))
#endif
       ) {
        adjustedScaleMode = true;
    }

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

void ExynosCamera1Parameters::setScalableSensorMode(bool scaleMode)
{
    m_cameraInfo.scalableSensorMode = scaleMode;
}

bool ExynosCamera1Parameters::getScalableSensorMode(void)
{
    return m_cameraInfo.scalableSensorMode;
}

void ExynosCamera1Parameters::m_getScalableSensorSize(int *newSensorW, int *newSensorH)
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

bool ExynosCamera1Parameters::getZoomSupported(void)
{
    return m_staticInfo->zoomSupport;
}

bool ExynosCamera1Parameters::getSmoothZoomSupported(void)
{
    return m_staticInfo->smoothZoomSupport;
}

void ExynosCamera1Parameters::checkHorizontalViewAngle(void)
{
    m_params.setFloat(CameraParameters::KEY_HORIZONTAL_VIEW_ANGLE, getHorizontalViewAngle());
}

void ExynosCamera1Parameters::setHorizontalViewAngle(int pictureW, int pictureH)
{
    double pi_camera = 3.1415926f;
    double distance;
    double ratio;
    double hViewAngle_half_rad = pi_camera / 180 * (double)m_staticInfo->horizontalViewAngle[SIZE_RATIO_16_9] / 2;

    distance = ((double)m_staticInfo->maxSensorW / (double)m_staticInfo->maxSensorH * 9 / 2)
                / tan(hViewAngle_half_rad);
    ratio = (double)pictureW / (double)pictureH;

    m_calculatedHorizontalViewAngle = (float)(atan(ratio * 9 / 2 / distance) * 2 * 180 / pi_camera);
}

float ExynosCamera1Parameters::getHorizontalViewAngle(void)
{
    int right_ratio = 177;

    if ((int)(m_staticInfo->maxSensorW * 100 / m_staticInfo->maxSensorH) == right_ratio) {
        return m_calculatedHorizontalViewAngle;
    } else {
        return m_staticInfo->horizontalViewAngle[m_cameraInfo.pictureSizeRatioId];
    }
}

float ExynosCamera1Parameters::getVerticalViewAngle(void)
{
    return m_staticInfo->verticalViewAngle;
}

void ExynosCamera1Parameters::getFnumber(int *num, int *den)
{
    *num = m_staticInfo->fNumberNum;
    *den = m_staticInfo->fNumberDen;
}

void ExynosCamera1Parameters::getApertureValue(int *num, int *den)
{
    *num = m_staticInfo->apertureNum;
    *den = m_staticInfo->apertureDen;
}

int ExynosCamera1Parameters::getFocalLengthIn35mmFilm(void)
{
    return m_staticInfo->focalLengthIn35mmLength;
}

void ExynosCamera1Parameters::getFocalLength(int *num, int *den)
{
    *num = m_staticInfo->focalLengthNum;
    *den = m_staticInfo->focalLengthDen;
}

void ExynosCamera1Parameters::getFocusDistances(int *num, int *den)
{
    *num = m_staticInfo->focusDistanceNum;
    *den = m_staticInfo->focusDistanceDen;
}

int ExynosCamera1Parameters::getMinExposureCompensation(void)
{
    return m_staticInfo->minExposureCompensation;
}

int ExynosCamera1Parameters::getMaxExposureCompensation(void)
{
    return m_staticInfo->maxExposureCompensation;
}

float ExynosCamera1Parameters::getExposureCompensationStep(void)
{
    return m_staticInfo->exposureCompensationStep;
}

int ExynosCamera1Parameters::getMinExposureTime(void)
{
    return m_staticInfo->minExposureTime;
}

int ExynosCamera1Parameters::getMaxExposureTime(void)
{
    return m_staticInfo->maxExposureTime;
}

int64_t ExynosCamera1Parameters::getSensorMinExposureTime(void)
{
    return m_staticInfo->exposureTimeRange[MIN];
}

int64_t ExynosCamera1Parameters::getSensorMaxExposureTime(void)
{
    return m_staticInfo->exposureTimeRange[MAX];
}

int ExynosCamera1Parameters::getMinWBK(void)
{
    return m_staticInfo->minWBK;
}

int ExynosCamera1Parameters::getMaxWBK(void)
{
    return m_staticInfo->maxWBK;
}

int ExynosCamera1Parameters::getMaxNumDetectedFaces(void)
{
    return m_staticInfo->maxNumDetectedFaces;
}

uint32_t ExynosCamera1Parameters::getMaxNumFocusAreas(void)
{
    return m_staticInfo->maxNumFocusAreas;
}

uint32_t ExynosCamera1Parameters::getMaxNumMeteringAreas(void)
{
    return m_staticInfo->maxNumMeteringAreas;
}

int ExynosCamera1Parameters::getMaxZoomLevel(void)
{
    int zoomLevel = 0;
    int samsungCamera = getSamsungCamera();

    if (samsungCamera) {
        zoomLevel = m_staticInfo->maxZoomLevel;
    } else {
        zoomLevel = m_staticInfo->maxBasicZoomLevel;
    }
    return zoomLevel;
}

float ExynosCamera1Parameters::getMaxZoomRatio(void)
{
    return (float)m_staticInfo->maxZoomRatio;
}

float ExynosCamera1Parameters::getZoomRatio(int zoomLevel)
{
    float zoomRatio = 1.00f;
    if (getZoomSupported() == true) {
        zoomRatio = (float)m_staticInfo->zoomRatioList[zoomLevel];
#ifdef SAMSUNG_DUAL_SOLUTION
        if (m_fusionPreviewWrapper->m_getIsInit() == true && getDualCameraMode() == true) {
            zoomRatio = m_fusionPreviewWrapper->m_getZoomRatio(getCameraId(), zoomLevel);
        }
#endif
    } else
        zoomRatio = 1000.00f;

    return zoomRatio;
}

float ExynosCamera1Parameters::getZoomRatio(void)
{
    return m_currentZoomRatio;
}

bool ExynosCamera1Parameters::getVideoSnapshotSupported(void)
{
    return m_staticInfo->videoSnapshotSupport;
}

bool ExynosCamera1Parameters::getVideoStabilizationSupported(void)
{
    return m_staticInfo->videoStabilizationSupport;
}

bool ExynosCamera1Parameters::getAutoWhiteBalanceLockSupported(void)
{
    return m_staticInfo->autoWhiteBalanceLockSupport;
}

bool ExynosCamera1Parameters::getAutoExposureLockSupported(void)
{
    return m_staticInfo->autoExposureLockSupport;
}

void ExynosCamera1Parameters::enableMsgType(int32_t msgType)
{
    Mutex::Autolock lock(m_msgLock);
    m_enabledMsgType |= msgType;
}

void ExynosCamera1Parameters::disableMsgType(int32_t msgType)
{
    Mutex::Autolock lock(m_msgLock);
    m_enabledMsgType &= ~msgType;
}

bool ExynosCamera1Parameters::msgTypeEnabled(int32_t msgType)
{
    Mutex::Autolock lock(m_msgLock);
    return (m_enabledMsgType & msgType);
}

status_t ExynosCamera1Parameters::setFrameSkipCount(int count)
{
    m_frameSkipCounter.setCount(count);

    return NO_ERROR;
}

status_t ExynosCamera1Parameters::getFrameSkipCount(int *count)
{
    *count = m_frameSkipCounter.getCount();
    m_frameSkipCounter.decCount();

    return NO_ERROR;
}

int ExynosCamera1Parameters::getFrameSkipCount(void)
{
    return m_frameSkipCounter.getCount();
}

void ExynosCamera1Parameters::setIsFirstStartFlag(bool flag)
{
    m_flagFirstStart = flag;
}

int ExynosCamera1Parameters::getIsFirstStartFlag(void)
{
    return m_flagFirstStart;
}

ExynosCameraActivityControl *ExynosCamera1Parameters::getActivityControl(void)
{
    return m_activityControl;
}

status_t ExynosCamera1Parameters::setAutoFocusMacroPosition(int autoFocusMacroPosition)
{
    int oldAutoFocusMacroPosition = m_cameraInfo.autoFocusMacroPosition;
    m_cameraInfo.autoFocusMacroPosition = autoFocusMacroPosition;

    m_activityControl->setAutoFocusMacroPosition(oldAutoFocusMacroPosition, autoFocusMacroPosition);

    return NO_ERROR;
}

status_t ExynosCamera1Parameters::setDisEnable(bool enable)
{
    setMetaBypassDis(&m_metadata, enable == true ? 0 : 1);
    return NO_ERROR;
}

bool ExynosCamera1Parameters::getDisEnable(void)
{
    return m_metadata.dis_bypass;
}

status_t ExynosCamera1Parameters::setDrcEnable(bool enable)
{
    setMetaBypassDrc(&m_metadata, enable == true ? 0 : 1);
    return NO_ERROR;
}

bool ExynosCamera1Parameters::getDrcEnable(void)
{
    return m_metadata.drc_bypass;
}

status_t ExynosCamera1Parameters::setDnrEnable(bool enable)
{
    status_t ret = NO_ERROR;
    bool flagBypass = false;

    ret = check3dnrBypass(enable, &flagBypass);
    if (ret != NO_ERROR)
        CLOGE("check3dnrBypass fail!");

    setMetaBypassDnr(&m_metadata, flagBypass);

    return NO_ERROR;
}

bool ExynosCamera1Parameters::getDnrEnable(void)
{
    return m_metadata.dnr_bypass;
}

status_t ExynosCamera1Parameters::setFdEnable(bool enable)
{
    setMetaBypassFd(&m_metadata, enable == true ? 0 : 1);
    return NO_ERROR;
}

bool ExynosCamera1Parameters::getFdEnable(void)
{
    return m_metadata.fd_bypass;
}

status_t ExynosCamera1Parameters::setFdMode(enum facedetect_mode mode)
{
    setMetaCtlFdMode(&m_metadata, mode);
    return NO_ERROR;
}

status_t ExynosCamera1Parameters::getFdMeta(bool reprocessing, void *buf)
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

void ExynosCamera1Parameters::setFlipHorizontal(int val)
{
    if (val < 0) {
        CLOGE(" setFlipHorizontal ignored, invalid value(%d)",
                val);
        return;
    }

    m_cameraInfo.flipHorizontal = val;
}

int ExynosCamera1Parameters::getFlipHorizontal(void)
{
    if (m_cameraInfo.shotMode == SHOT_MODE_FRONT_PANORAMA) {
        return 0;
    } else {
        return m_cameraInfo.flipHorizontal;
    }
}

void ExynosCamera1Parameters::setFlipVertical(int val)
{
    if (val < 0) {
        CLOGE(" setFlipVertical ignored, invalid value(%d)",
                val);
        return;
    }

    m_cameraInfo.flipVertical = val;
}

int ExynosCamera1Parameters::getFlipVertical(void)
{
    return m_cameraInfo.flipVertical;
}

bool ExynosCamera1Parameters::setDeviceOrientation(int orientation)
{
    if (orientation < 0 || orientation % 90 != 0) {
        CLOGE("Invalid orientation (%d)",
                orientation);
        return false;
    }

    m_cameraInfo.deviceOrientation = orientation;

    /* fd orientation need to be calibrated, according to f/w spec */
    int hwRotation = BACK_ROTATION;

#if 1
    if (this->getCameraId() == CAMERA_ID_FRONT)
        hwRotation = FRONT_ROTATION;
#endif

    int fdOrientation = (orientation + hwRotation) % 360;

    CLOGD("orientation(%d), hwRotation(%d), fdOrientation(%d)",
                orientation, hwRotation, fdOrientation);

    return true;
}

int ExynosCamera1Parameters::getDeviceOrientation(void)
{
    return m_cameraInfo.deviceOrientation;
}

int ExynosCamera1Parameters::getFdOrientation(void)
{
    return (m_cameraInfo.deviceOrientation + BACK_ROTATION) % 360;
/*
   // Orientation will be modified at driver.
    int cameraId = getCameraId();
    int rotation = 0;
    switch(cameraId) {
    case CAMERA_ID_BACK:
    case CAMERA_ID_BACK_1:
        rotation = (m_cameraInfo.deviceOrientation + BACK_ROTATION) % 360;
        break;
    case CAMERA_ID_FRONT:
    case CAMERA_ID_FRONT_1:
        rotation = (m_cameraInfo.deviceOrientation + FRONT_ROTATION) % 360;
        break;
    default:
        CLOGE("invalid cameraId");
        break;
    }
    return rotation;
*/
}

void ExynosCamera1Parameters::getSetfileYuvRange(bool flagReprocessing, int *setfile, int *yuvRange)
{
    if (flagReprocessing == true) {
        *setfile = m_setfileReprocessing;
        *yuvRange = m_yuvRangeReprocessing;
    } else {
        if (getFastenAeStableOn()) {
            *setfile = ISS_SUB_SCENARIO_STILL_PREVIEW;
        } else {
            *setfile = m_setfile;
        }
        *yuvRange = m_yuvRange;
    }
}

status_t ExynosCamera1Parameters::checkSetfileYuvRange(void)
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

void ExynosCamera1Parameters::setSetfileYuvRange(void)
{
    /* reprocessing */
    m_getSetfileYuvRange(true, &m_setfileReprocessing, &m_yuvRangeReprocessing);

    CLOGD("m_cameraId(%d) : general[setfile(%d) YUV range(%d)] : reprocesing[setfile(%d) YUV range(%d)]",
        m_cameraId,
        m_setfile, m_yuvRange,
        m_setfileReprocessing, m_yuvRangeReprocessing);

}

void ExynosCamera1Parameters::setUseDynamicBayer(bool enable)
{
    m_useDynamicBayer = enable;
}

bool ExynosCamera1Parameters::getUseDynamicBayer(void)
{
    return m_useDynamicBayer;
}

void ExynosCamera1Parameters::setUseDynamicBayerVideoSnapShot(bool enable)
{
    m_useDynamicBayerVideoSnapShot = enable;
}

bool ExynosCamera1Parameters::getUseDynamicBayerVideoSnapShot(void)
{
    return m_useDynamicBayerVideoSnapShot;
}

void ExynosCamera1Parameters::setUseDynamicScc(bool enable)
{
    m_useDynamicScc = enable;
}

bool ExynosCamera1Parameters::getUseDynamicScc(void)
{
    bool dynamicScc = m_useDynamicScc;
    bool reprocessing = isReprocessing();

    if (getRecordingHint() == true && reprocessing == false)
        dynamicScc = false;

    return dynamicScc;
}

void ExynosCamera1Parameters::setUseFastenAeStable(bool enable)
{
    m_useFastenAeStable = enable;
}

bool ExynosCamera1Parameters::getUseFastenAeStable(void)
{
    return m_useFastenAeStable;
}

void ExynosCamera1Parameters::setFastenAeStableOn(bool enable)
{
    m_fastenAeStableOn = enable;
    CLOGD("m_cameraId(%d) : m_fastenAeStableOn(%d)",
        m_cameraId, m_fastenAeStableOn);
}

bool ExynosCamera1Parameters::getFastenAeStableOn(void)
{
    return m_fastenAeStableOn;
}

status_t ExynosCamera1Parameters::calcHighResolutionPreviewGSCRect(ExynosRect *srcRect, ExynosRect *dstRect)
{
    int previewW = 0, previewH = 0, previewFormat = 0;
    int pictureW = 0, pictureH = 0, pictureFormat = 0;
    previewFormat = getPreviewFormat();
    pictureFormat = getHwPictureFormat();

    if (isOwnScc(m_cameraId) == true)
        getPictureSize(&pictureW, &pictureH);
    else
        getHwPictureSize(&pictureW, &pictureH);
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

status_t ExynosCamera1Parameters::calcPictureRect(ExynosRect *srcRect, ExynosRect *dstRect)
{
    int ret = 0;

    int hwSensorW = 0, hwSensorH = 0;
    int hwPictureW = 0, hwPictureH = 0;
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
                     CAMERA_BCROP_ALIGN, 2,
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

status_t ExynosCamera1Parameters::calcPictureRect(int originW, int originH, ExynosRect *srcRect, ExynosRect *dstRect)
{
    int ret = 0;
    int pictureW = 0, pictureH = 0, pictureFormat = 0;

    int crop_crop_x = 0, crop_crop_y = 0;
    int crop_crop_w = 0, crop_crop_h = 0;
    float zoomRatio = getZoomRatio(0) / 1000;
#if 0
    int zoom = 0;
#endif
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

#ifdef SAMSUNG_DUAL_SOLUTION
void ExynosCamera1Parameters::getPreviewStatsRoi(ExynosRect *dstRect, ExynosRect *statsRoi)
{
    /*
        dstRect    : bcrop image with margin
        cropRect3A : real image size for 3A
    */
    if (m_needPreviewMargin) {
        statsRoi->w = dstRect->w / DUAL_SOLUTION_MARGIN_RATIO;
        statsRoi->h = dstRect->h / DUAL_SOLUTION_MARGIN_RATIO;
        statsRoi->x = ALIGN_UP((dstRect->x + (dstRect->w - statsRoi->w) >> 1), 2);
        statsRoi->y = ALIGN_UP((dstRect->y + (dstRect->h - statsRoi->h) >> 1), 2);
    } else {
        statsRoi->w = dstRect->w;
        statsRoi->h = dstRect->h;
        statsRoi->x = dstRect->x;
        statsRoi->y = dstRect->y;
    }
}
#endif

status_t ExynosCamera1Parameters::getPreviewBayerCropSize(ExynosRect *srcRect, ExynosRect *dstRect, bool applyZoom)
{
    int hwBnsW   = 0;
    int hwBnsH   = 0;
    int hwBcropW = 0;
    int hwBcropH = 0;
    int zoomLevel = 0;
    float zoomRatio = 1.00f;
    int sizeList[SIZE_LUT_INDEX_END];
    int hwSensorMarginW = 0;
    int hwSensorMarginH = 0;

    /* matched ratio LUT is not existed, use equation */
    if (m_useSizeTable == false
        || m_staticInfo->previewSizeLut == NULL
        || m_staticInfo->previewSizeLutMax <= m_cameraInfo.previewSizeRatioId
        || m_getPreviewSizeList(sizeList) != NO_ERROR) {
        return calcPreviewBayerCropSize(srcRect, dstRect);
    }

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
    if (SIZE_RATIO(curBnsW, curBnsH) != SIZE_RATIO(hwBnsW, hwBnsH)) {
        CLOGW("current BNS size(%dx%d) is NOT same with Hw BNS size(%dx%d)",
                curBnsW, curBnsH, hwBnsW, hwBnsH);
    }

    if (applyZoom == true && isUse3aaInputCrop() == true) {
        zoomLevel = getZoomLevel();
        zoomRatio = getZoomRatio(zoomLevel) / 1000;
    }

    int fastFpsMode = getFastFpsMode();
    if (fastFpsMode > CONFIG_MODE::HIGHSPEED_60 &&
        fastFpsMode < CONFIG_MODE::MAX &&
        getZoomPreviewWIthScaler() == true) {
        CLOGV("hwBnsSize (%dx%d), hwBcropSize(%dx%d), fastFpsMode(%d)",
                hwBnsW, hwBnsH,
                hwBcropW, hwBcropH,
                fastFpsMode);
    } else {
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
    }

    if ((getUsePureBayerReprocessing() == false && m_cameraInfo.pictureSizeRatioId != m_cameraInfo.previewSizeRatioId)
#ifdef USE_REMOSAIC_CAPTURE
        || (getUsePureBayerReprocessing() == false && getCameraId() == CAMERA_ID_FRONT)
#endif
        )
    {
        int ret = 0;
        int hwBcropX = 0;
        int hwBcropY = 0;
        int pictureW = 0;
        int pictureH = 0;
        int hwSensorW = 0;
        int hwSensorH = 0;
        int maxZoomRatio = 0;
        int pictureCropX = 0, pictureCropY = 0;
        int pictureCropW = 0, pictureCropH = 0;

        zoomLevel = 0;
        zoomRatio = getZoomRatio(zoomLevel) / 1000;

        maxZoomRatio = getMaxZoomRatio() / 1000;

        getPictureSize(&pictureW, &pictureH);
        getHwSensorSize(&hwSensorW, &hwSensorH);

        ret = getCropRectAlign(hwBcropW, hwBcropH,
                pictureW, pictureH,
                &pictureCropX, &pictureCropY,
                &pictureCropW, &pictureCropH,
                CAMERA_BCROP_ALIGN, 2,
                zoomLevel, zoomRatio);

        pictureCropX = ALIGN_DOWN(pictureCropX, 2);
        pictureCropY = ALIGN_DOWN(pictureCropY, 2);
        pictureCropW = hwBcropW - (pictureCropX * 2);
        pictureCropH = hwBcropH - (pictureCropY * 2);

        if (pictureCropW < pictureW / maxZoomRatio || pictureCropH < pictureH / maxZoomRatio) {
            CLOGW(" zoom ratio is upto x%d, crop(%dx%d), picture(%dx%d)",
                    maxZoomRatio, hwBcropW, hwBcropH, pictureW, pictureH);
            float src_ratio = 1.0f;
            float dst_ratio = 1.0f;
            /* ex : 1024 / 768 */
            src_ratio = ROUND_OFF_HALF(((float)hwBcropW / (float)hwBcropH), 2);
            /* ex : 352  / 288 */
            dst_ratio = ROUND_OFF_HALF(((float)pictureW / (float)pictureH), 2);

            if (dst_ratio <= src_ratio) {
                /* shrink w */
                hwBcropX = ALIGN_DOWN(((int)(hwSensorW - ((pictureH / maxZoomRatio) * src_ratio)) >> 1), 2);
                hwBcropY = ALIGN_DOWN(((hwSensorH - (pictureH / maxZoomRatio)) >> 1), 2);
            } else {
                /* shrink h */
                hwBcropX = ALIGN_DOWN(((hwSensorW - (pictureW / maxZoomRatio)) >> 1), 2);
                hwBcropY = ALIGN_DOWN(((int)(hwSensorH - ((pictureW / maxZoomRatio) / src_ratio)) >> 1), 2);
            }
            hwBcropW = hwSensorW - (hwBcropX * 2);
            hwBcropH = hwSensorH - (hwBcropY * 2);
        }
    }else {
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

            if (hwBcropW < fusionSrcRect.w) {
                CLOGV("calibrate bcrop width size (%d -> %d)",  hwBcropW, fusionSrcRect.w);
                hwBcropW = fusionSrcRect.w;
            }

            if (hwBcropH < fusionSrcRect.h) {
                CLOGV("calibrate bcrop height size (%d -> %d)",  hwBcropH, fusionSrcRect.h);
                hwBcropH = fusionSrcRect.h;
            }
        }
#endif // USE_CP_FUSION_LIB_LIMIT_CROP_ON_ISP
#endif // USE_CP_FUSION_LIB_CROP_IN_LIBRARY
#endif // USE_CP_FUSION_LIB
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

    m_setHwBayerCropRegion(dstRect->w, dstRect->h, dstRect->x, dstRect->y);
#ifdef DEBUG_PERFRAME
    CLOGD("hwBnsSize %dx%d, hwBcropSize %d,%d %dx%d zoomLevel %d zoomRatio %f",
            srcRect->w, srcRect->h,
            dstRect->x, dstRect->y, dstRect->w, dstRect->h,
            zoomLevel, zoomRatio);
#endif

    return NO_ERROR;
}

status_t ExynosCamera1Parameters::calcPreviewDzoomCropSize(ExynosRect *srcRect, ExynosRect *dstRect)
{
    int ret = 0;

    int previewW = 0, previewH = 0;

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

status_t ExynosCamera1Parameters::getPreviewBdsSize(ExynosRect *dstRect, bool applyZoom)
{
    status_t ret = NO_ERROR;
    ExynosRect bnsSize;
    ExynosRect bayerCropSize;
    ExynosRect bdsSize;

    ret = m_getPreviewBdsSize(&bdsSize);
    if (ret != NO_ERROR) {
        CLOGE("m_getPreviewBdsSize() fail");
        return ret;
    }

    if (this->getHWVdisMode() == true) {
        int disW = ALIGN_UP((int)(bdsSize.w * HW_VDIS_W_RATIO), 2);
        int disH = ALIGN_UP((int)(bdsSize.h * HW_VDIS_H_RATIO), 2);

        CLOGV("HWVdis adjusted BDS Size (%d x %d) -> (%d x %d)",
            bdsSize.w, bdsSize.h, disW, disH);

        bdsSize.w = disW;
        bdsSize.h = disH;
    }

    /* Check the invalid BDS size compared to Bcrop size */
    ret = getPreviewBayerCropSize(&bnsSize, &bayerCropSize, applyZoom);
    if (ret != NO_ERROR)
        CLOGE("getPreviewBayerCropSize() failed");

    if (bayerCropSize.w < bdsSize.w || bayerCropSize.h < bdsSize.h) {
        CLOGV("bayerCropSize %dx%d is smaller than BDSSize %dx%d. Force bayerCropSize",
                bayerCropSize.w, bayerCropSize.h, bdsSize.w, bdsSize.h);

        bdsSize.w = bayerCropSize.w;
        bdsSize.h = bayerCropSize.h;
    }

    dstRect->x = 0;
    dstRect->y = 0;
    dstRect->w = bdsSize.w;
    dstRect->h = bdsSize.h;

#ifdef DEBUG_PERFRAME
    CLOGD("hwBdsSize %dx%d", dstRect->w, dstRect->h);
#endif

    return ret;
}

status_t ExynosCamera1Parameters::getPreviewYuvCropSize(ExynosRect *yuvCropSize)
{
    status_t ret = NO_ERROR;
    int zoomLevel = 0;
    float zoomRatio = 1.00f;
    ExynosRect previewBdsSize;
    ExynosRect previewYuvCropSize;

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
        || isUseMcscInputCrop() == true) {
        zoomLevel = this->getZoomLevel();
        zoomRatio = getZoomRatio(zoomLevel) / 1000;
    }

    /* 3. Calculate the YuvCropSize with ZoomRatio */
#if defined(SCALER_MAX_SCALE_UP_RATIO)
    /*
     * After dividing float & casting int,
     * zoomed size can be smaller too much.
     * so, when zoom until max, ceil up about floating point.
     */
    if (ALIGN_UP((int)((float)previewBdsSize.w / zoomRatio), CAMERA_BCROP_ALIGN) * SCALER_MAX_SCALE_UP_RATIO < previewBdsSize.w
     || ALIGN_UP((int)((float)previewBdsSize.h/ zoomRatio), 2) * SCALER_MAX_SCALE_UP_RATIO < previewBdsSize.h) {
        previewYuvCropSize.w = ALIGN_UP((int)ceil((float)previewBdsSize.w / zoomRatio), CAMERA_BCROP_ALIGN);
        previewYuvCropSize.h = ALIGN_UP((int)ceil((float)previewBdsSize.h / zoomRatio), 2);
    } else
#endif
    {
        previewYuvCropSize.w = ALIGN_UP((int)((float)previewBdsSize.w / zoomRatio), CAMERA_BCROP_ALIGN);
        previewYuvCropSize.h = ALIGN_UP((int)((float)previewBdsSize.h / zoomRatio), 2);
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

    if (isIspMcscOtf() == true
        || (isIspTpuOtf() == true && isTpuMcscOtf() == true)) {
        yuvCropSize->x = 0;
        yuvCropSize->y = 0;
        yuvCropSize->w = previewBdsSize.w;
        yuvCropSize->h = previewBdsSize.h;
    }

#ifdef DEBUG_PERFRAME
    CLOGD("BDS %dx%d YuvCrop %d,%d %dx%d zoomLevel %d zoomRatio %f",
            previewBdsSize.w, previewBdsSize.h,
            yuvCropSize->x, yuvCropSize->y, yuvCropSize->w, yuvCropSize->h,
            zoomLevel, zoomRatio);
#endif

    return ret;
}

status_t ExynosCamera1Parameters::getPictureYuvCropSize(ExynosRect *yuvCropSize)
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
        || isUseReprocessingMcscInputCrop() == true) {
        zoomLevel = this->getZoomLevel();
        zoomRatio = getZoomRatio(zoomLevel) / 1000;
    }

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

    if (isReprocessingIspMcscOtf() == true
        || (isReprocessingIspTpuOtf() == true && isReprocessingTpuMcscOtf() == true)) {
        yuvCropSize->x = 0;
        yuvCropSize->y = 0;
        yuvCropSize->w = ispInputSize.w;
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

status_t ExynosCamera1Parameters::getFastenAeStableSensorSize(int *hwSensorW, int *hwSensorH, int index)
{
    *hwSensorW = m_staticInfo->fastAeStableLut[index][SENSOR_W];
    *hwSensorH = m_staticInfo->fastAeStableLut[index][SENSOR_H];

    return NO_ERROR;
}

status_t ExynosCamera1Parameters::getFastenAeStableBcropSize(int *hwBcropW, int *hwBcropH, int index)
{
    int zoomLevel = 0;
    float zoomRatio = 1.00f;
    int BcropW = 0;
    int BcropH = 0;

    BcropW = m_staticInfo->fastAeStableLut[index][BCROP_W];
    BcropH = m_staticInfo->fastAeStableLut[index][BCROP_H];

    zoomLevel = getZoomLevel();
    zoomRatio = getZoomRatio(zoomLevel) / 1000;

    if (zoomLevel != 0) {
#if defined(SCALER_MAX_SCALE_UP_RATIO)
        /*
         * After dividing float & casting int,
         * zoomed size can be smaller too much.
         * so, when zoom until max, ceil up about floating point.
         */
        if (ALIGN_UP((int)((float)BcropW / zoomRatio), CAMERA_BCROP_ALIGN) * SCALER_MAX_SCALE_UP_RATIO < BcropW ||
            ALIGN_UP((int)((float)BcropH / zoomRatio), 2) * SCALER_MAX_SCALE_UP_RATIO < BcropH) {
            BcropW = ALIGN_UP((int)ceil((float)BcropW / zoomRatio), CAMERA_BCROP_ALIGN);
            BcropH = ALIGN_UP((int)ceil((float)BcropH / zoomRatio), 2);
        } else
#endif
        {
            BcropW = ALIGN_UP((int)((float)BcropW / zoomRatio), CAMERA_BCROP_ALIGN);
            BcropH = ALIGN_UP((int)((float)BcropH / zoomRatio), 2);
        }
    }

    if(BcropW < 320 || BcropH < 240) {
        *hwBcropW = 320;
        *hwBcropH = 240;
    } else {
        *hwBcropW = BcropW;
        *hwBcropH = BcropH;
    }

    return NO_ERROR;
}

status_t ExynosCamera1Parameters::getFastenAeStableBdsSize(int *hwBdsW, int *hwBdsH, int index)
{
    *hwBdsW = m_staticInfo->fastAeStableLut[index][BDS_W];
    *hwBdsH = m_staticInfo->fastAeStableLut[index][BDS_H];

    return NO_ERROR;
}

status_t ExynosCamera1Parameters::getDepthMapSize(int *depthMapW, int *depthMapH)
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

int ExynosCamera1Parameters::getActualPictureRatioIdx(int pictureRatioId)
{
    int actualRatioIdx = 0;
    for (int index = 0; index < m_staticInfo->pictureSizeLutMax; index++) {
        if (m_staticInfo->pictureSizeLut[index][0] == pictureRatioId) {
            actualRatioIdx = index;
            break;
        }
    }
    return actualRatioIdx;
}

#ifdef SUPPORT_DEPTH_MAP
bool ExynosCamera1Parameters::getUseDepthMap(void)
{
    return m_flaguseDepthMap;
}

void ExynosCamera1Parameters::m_setUseDepthMap(bool useDepthMap)
{
    m_flaguseDepthMap = useDepthMap;
}

status_t ExynosCamera1Parameters::checkUseDepthMap(void)
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

bool ExynosCamera1Parameters::checkFaceDetectMeta(struct camera2_shot_ext *shot_ext)
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

                for (int j = 0; j < 6; j++) {
                    m_metadata.shot.dm.stats.faceLandmarks[i][j] = shot_ext->shot.dm.stats.faceLandmarks[i][j];
                }
                for (int j = 0; j < 4; j++) {
                    m_metadata.shot.dm.stats.faceRectangles[i][j] = shot_ext->shot.dm.stats.faceRectangles[i][j];
                }
            }

            for (int i = 0; i < CAMERA2_MAX_FACES - 2; i++) {
                m_metadata.shot.dm.stats.faces[i] = shot_ext->shot.dm.stats.faces[i];
            }
        } else if (shot_ext->shot.dm.stats.faceDetectMode <= FACEDETECT_MODE_OFF) {
            shot_ext->shot.dm.stats.faceDetectMode = m_metadata.shot.dm.stats.faceDetectMode;

            for (int i = 0; i < CAMERA2_MAX_FACES; i++) {
                shot_ext->shot.dm.stats.faceIds[i] = m_metadata.shot.dm.stats.faceIds[i];
                shot_ext->shot.dm.stats.faceScores[i] = m_metadata.shot.dm.stats.faceScores[i];

                for (int j = 0; j < 6; j++) {
                    shot_ext->shot.dm.stats.faceLandmarks[i][j] = m_metadata.shot.dm.stats.faceLandmarks[i][j];
                }
                for (int j = 0; j < 4; j++) {
                    shot_ext->shot.dm.stats.faceRectangles[i][j] = m_metadata.shot.dm.stats.faceRectangles[i][j];
                }
            }

            for (int i = 0; i < CAMERA2_MAX_FACES - 2; i++) {
                shot_ext->shot.dm.stats.faces[i] = m_metadata.shot.dm.stats.faces[i];
            }

            ret = true;
        }
    }

    return ret;
}

status_t ExynosCamera1Parameters::calcNormalToTpuSize(int srcW, int srcH, int *dstW, int *dstH)
{
    if (srcW < 0 || srcH < 0) {
        CLOGE("src size is invalid(%d x %d)", srcW, srcH);
        return INVALID_OPERATION;
    }

    int disW = ALIGN_UP((int)(srcW * HW_VDIS_W_RATIO), 2);
    int disH = ALIGN_UP((int)(srcH * HW_VDIS_H_RATIO), 2);

    *dstW = disW;
    *dstH = disH;
    CLOGD("HWVdis adjusted BDS Size (%d x %d) -> (%d x %d)", srcW, srcH, disW, disH);

    return NO_ERROR;
}

status_t ExynosCamera1Parameters::calcTpuToNormalSize(int srcW, int srcH, int *dstW, int *dstH)
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
    CLOGD("HWVdis adjusted BDS Size (%d x %d) -> (%d x %d)", srcW, srcH, disW, disH);

    return ret;
}

void ExynosCamera1Parameters::setUsePureBayerReprocessing(bool enable)
{
    m_usePureBayerReprocessing = enable;
}

bool ExynosCamera1Parameters::getUsePureBayerReprocessing(void)
{
    int oldMode = m_usePureBayerReprocessing;

    if (getDualMode() == true)
        m_usePureBayerReprocessing = (getCameraId() == CAMERA_ID_BACK) ? USE_PURE_BAYER_REPROCESSING_ON_DUAL : USE_PURE_BAYER_REPROCESSING_FRONT_ON_DUAL;
    else {
        m_usePureBayerReprocessing = (getCameraId() == CAMERA_ID_BACK) ? USE_PURE_BAYER_REPROCESSING : USE_PURE_BAYER_REPROCESSING_FRONT;

#ifdef USE_REMOSAIC_CAPTURE
        if (getRemosaicCaptureMode() == REMOSAIC_CAPTURE_MODE_PURE_BAYER) {
            m_usePureBayerReprocessing = (getCameraId() == CAMERA_ID_BACK) ? USE_PURE_BAYER_REPROCESSING_REMOSAIC : USE_PURE_BAYER_REPROCESSING_FRONT_REMOSAIC;
        }
#endif
    }

    if (oldMode != m_usePureBayerReprocessing) {
        CLOGD("bayer usage is changed (%d -> %d)", oldMode, m_usePureBayerReprocessing);
    }

    return m_usePureBayerReprocessing;
}

int32_t ExynosCamera1Parameters::updateReprocessingBayerMode(void)
{
    int32_t mode = REPROCESSING_BAYER_MODE_NONE;
    bool useDynamicBayer = (getRecordingHint() == true || getDualRecordingHint() == true) ?
        getUseDynamicBayerVideoSnapShot() : getUseDynamicBayer();

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

    m_reprocessingBayerMode = mode;

    return m_reprocessingBayerMode;
}

int32_t ExynosCamera1Parameters::getReprocessingBayerMode(void)
{
    return m_reprocessingBayerMode;
}

void ExynosCamera1Parameters::setAdaptiveCSCRecording(bool enable)
{
    m_useAdaptiveCSCRecording = enable;
}

bool ExynosCamera1Parameters::getAdaptiveCSCRecording(void)
{
    return m_useAdaptiveCSCRecording;
}

int ExynosCamera1Parameters::getHalPixelFormat(void)
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
int ExynosCamera1Parameters::convertingHalPreviewFormat(int previewFormat, int yuvRange)
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
int ExynosCamera1Parameters::convertingHalPreviewFormat(int previewFormat, int yuvRange)
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

void ExynosCamera1Parameters::setDvfsLock(bool lock) {
    m_dvfsLock = lock;
}

bool ExynosCamera1Parameters::getDvfsLock(void) {
    return m_dvfsLock;
}

bool ExynosCamera1Parameters::setConfig(struct ExynosConfigInfo* config)
{
    memcpy(m_exynosconfig, config, sizeof(struct ExynosConfigInfo));
    setConfigMode(m_exynosconfig->mode);
    return true;
}
struct ExynosConfigInfo* ExynosCamera1Parameters::getConfig()
{
    return m_exynosconfig;
}

bool ExynosCamera1Parameters::setConfigMode(uint32_t mode)
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

int ExynosCamera1Parameters::getConfigMode()
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

void ExynosCamera1Parameters::setZoomActiveOn(bool enable)
{
    m_zoom_activated = enable;
}

bool ExynosCamera1Parameters::getZoomActiveOn(void)
{
    return m_zoom_activated;
}

status_t ExynosCamera1Parameters::setMarkingOfExifFlash(int flag)
{
    m_firing_flash_marking = flag;

    return NO_ERROR;
}

#ifdef USE_LIVE_BROADCAST
void ExynosCamera1Parameters::setPLBMode(bool plbMode)
{
    m_cameraInfo.plbMode = plbMode;
}

bool ExynosCamera1Parameters::getPLBMode(void)
{
    return m_cameraInfo.plbMode;
}
#endif

int ExynosCamera1Parameters::getMarkingOfExifFlash(void)
{
    return m_firing_flash_marking;
}

bool ExynosCamera1Parameters::getSensorOTFSupported(void)
{
    return m_staticInfo->flite3aaOtfSupport;
}

bool ExynosCamera1Parameters::isReprocessing(void)
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

bool ExynosCamera1Parameters::isSccCapture(void)
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

bool ExynosCamera1Parameters::isFlite3aaOtf(void)
{
    bool flagOtfInput = false;

    int cameraId = getCameraId();
    bool flagDual = getDualMode();
    bool flagSensorOtf = getSensorOTFSupported();

    if (flagSensorOtf == false) {
        return flagOtfInput;
    }
    if (m_scenario == SCENARIO_SECURE || getVisionMode() == true) {
        flagOtfInput = false;
    } else if (cameraId == CAMERA_ID_BACK) {
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

bool ExynosCamera1Parameters::is3aaIspOtf(void)
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

bool ExynosCamera1Parameters::isIspTpuOtf(void)
{
    bool ret = false;

    int cameraId = getCameraId();
    bool flagDual = getDualMode();
    bool flagTpu = getTpuEnabledMode();

    if (cameraId == CAMERA_ID_BACK) {
        if (flagDual == true) {
#ifdef MAIN_CAMERA_DUAL_ISP_TPU_OTF
            ret = MAIN_CAMERA_DUAL_ISP_TPU_OTF;
#else
            CLOGW(" MAIN_CAMERA_DUAL_ISP_TPU_OTF is not defined");
#endif
        } else {
#ifdef MAIN_CAMERA_SINGLE_ISP_TPU_OTF
            ret = (getOdcMode() && flagTpu)? MAIN_CAMERA_SINGLE_ISP_TPU_GDC_OTF : MAIN_CAMERA_SINGLE_ISP_TPU_OTF;
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
            ret = (getOdcMode() && flagTpu)? FRONT_CAMERA_SINGLE_ISP_TPU_GDC_OTF : FRONT_CAMERA_SINGLE_ISP_TPU_OTF;
#else
            CLOGW(" FRONT_CAMERA_SINGLE_ISP_TPU_OTF is not defined");
#endif
        }
    }

    return ret;
}

bool ExynosCamera1Parameters::isIspMcscOtf(void)
{
    bool ret = false;

    int cameraId = getCameraId();
    bool flagDual = getDualMode();

    if (cameraId == CAMERA_ID_BACK) {
        if (flagDual == true) {
#ifdef MAIN_CAMERA_DUAL_ISP_MCSC_OTF
            ret = MAIN_CAMERA_DUAL_ISP_MCSC_OTF;
#else
            CLOGW(" MAIN_CAMERA_DUAL_ISP_MCSC_OTF is not defined");
#endif
        } else {
#ifdef MAIN_CAMERA_SINGLE_ISP_MCSC_OTF
            ret = (isIspTpuOtf() ? MAIN_CAMERA_SINGLE_ISP_MCSC_OTF : false);;
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
            ret = (isIspTpuOtf() ? FRONT_CAMERA_SINGLE_ISP_MCSC_OTF : false);
#else
            CLOGW(" FRONT_CAMERA_SINGLE_ISP_MCSC_OTF is not defined");
#endif
        }
    }

    return ret;
}

bool ExynosCamera1Parameters::isTpuMcscOtf(void)
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

bool ExynosCamera1Parameters::isMcscVraOtf(void)
{
    bool ret = true;
#ifdef USE_VRA_GROUP
    int cameraId = getCameraId();
    bool flagDual = getDualMode();

    switch(cameraId) {
    case CAMERA_ID_BACK:
    case CAMERA_ID_BACK_1:
        if (flagDual == true) {
#ifdef MAIN_CAMERA_DUAL_MCSC_VRA_OTF
            ret = MAIN_CAMERA_DUAL_MCSC_VRA_OTF;
#endif
        } else {
#ifdef MAIN_CAMERA_SINGLE_MCSC_VRA_OTF
            ret = MAIN_CAMERA_SINGLE_MCSC_VRA_OTF;
#endif
        }
        break;
    case CAMERA_ID_FRONT:
    case CAMERA_ID_FRONT_1:
        if (flagDual == true) {
#ifdef FRONT_CAMERA_DUAL_MCSC_VRA_OTF
            ret = FRONT_CAMERA_DUAL_MCSC_VRA_OTF;
#endif
        } else {
#ifdef FRONT_CAMERA_SINGLE_MCSC_VRA_OTF
            ret = FRONT_CAMERA_SINGLE_MCSC_VRA_OTF;
#endif
        }
        break;
    }
#endif
    return ret;
}

bool ExynosCamera1Parameters::isReprocessing3aaIspOTF(void)
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

#ifdef USE_REMOSAIC_CAPTURE
    bool flagRemosaicPureBayer = otf;
    switch(this->getRemosaicCaptureMode()) {
    case REMOSAIC_CAPTURE_MODE_PURE_BAYER:
        flagRemosaicPureBayer = true;
        break;
    case REMOSAIC_CAPTURE_MODE_PROCESSED_BAYER:
    case REMOSAIC_CAPTURE_MODE_NONE:
    default:
        break;
    }

    if (otf != flagRemosaicPureBayer) {
        CLOGW(" otf == %d. but, flagRemosaicPureBayer == %d. so force false on 3aa_isp otf", otf, flagRemosaicPureBayer);
        otf = flagRemosaicPureBayer;
    }
#endif

    return otf;
}

bool ExynosCamera1Parameters::isReprocessingIspTpuOtf(void)
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

bool ExynosCamera1Parameters::isReprocessingIspMcscOtf(void)
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

bool ExynosCamera1Parameters::isReprocessingTpuMcscOtf(void)
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

bool ExynosCamera1Parameters::isSingleChain(void)
{
#ifdef USE_SINGLE_CHAIN
    return true;
#else
    return false;
#endif
}

bool ExynosCamera1Parameters::isUse3aaInputCrop(void)
{
    return true;
}

bool ExynosCamera1Parameters::isUseIspInputCrop(void)
{
    if (isUse3aaInputCrop() == true
        || is3aaIspOtf() == true)
        return false;
    else
        return true;
}

bool ExynosCamera1Parameters::isUseMcscInputCrop(void)
{
    if (isUse3aaInputCrop() == true
        || isUseIspInputCrop() == true
        || isIspMcscOtf() == true
        || isTpuMcscOtf() == true)
        return false;
    else
        return true;
}

bool ExynosCamera1Parameters::isUseReprocessing3aaInputCrop(void)
{
#ifdef USE_CP_FUSION_LIB
    /*
     * in case of cp fusion capture,
     * crop and scale up is happen on app.
     * so, hal does not crop and scale.
     */
    return false;
#else
    return true;
#endif
}

bool ExynosCamera1Parameters::isUseReprocessingIspInputCrop(void)
{
    if (isUseReprocessing3aaInputCrop() == true
        || isReprocessing3aaIspOTF() == true)
        return false;
    else
        return true;
}

bool ExynosCamera1Parameters::isUseReprocessingMcscInputCrop(void)
{
    if (isUseReprocessing3aaInputCrop() == true
        || isUseReprocessingIspInputCrop() == true
        || isReprocessingIspMcscOtf() == true
        || isReprocessingTpuMcscOtf() == true)
        return false;
    else
        return true;
}

bool ExynosCamera1Parameters::isUseEarlyFrameReturn(void)
{
#if defined(USE_EARLY_FRAME_RETURN)
    return true;
#else
    return false;
#endif
}

bool ExynosCamera1Parameters::isUseHWFC(void)
{
#if defined(USE_JPEG_HWFC)
    return USE_JPEG_HWFC;
#else
    return false;
#endif
}

bool ExynosCamera1Parameters::isHWFCOnDemand(void)
{
#if defined(USE_JPEG_HWFC_ONDEMAND)
    return USE_JPEG_HWFC_ONDEMAND;
#else
    return false;
#endif
}

void ExynosCamera1Parameters::setZoomPreviewWIthScaler(bool enable)
{
    m_zoomWithScaler = enable;
}

bool ExynosCamera1Parameters::getZoomPreviewWIthScaler(void)
{
    return m_zoomWithScaler;
}

bool ExynosCamera1Parameters::isOwnScc(int cameraId)
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

int ExynosCamera1Parameters::getHalVersion(void)
{
    return m_halVersion;
}

void ExynosCamera1Parameters::setHalVersion(int halVersion)
{
    m_halVersion = halVersion;
    m_activityControl->setHalVersion(m_halVersion);

    CLOGI(" m_halVersion(%d)", m_halVersion);

    return;
}

struct ExynosSensorInfoBase *ExynosCamera1Parameters::getSensorStaticInfo()
{
    CLOGE(" halVersion(%d) does not support this function!!!!", m_halVersion);
    return NULL;
}

bool ExynosCamera1Parameters::getSetFileCtlMode(void)
{
#ifdef SET_SETFILE_BY_SET_CTRL
    return true;
#else
    return false;
#endif
}

bool ExynosCamera1Parameters::getSetFileCtl3AA_ISP(void)
{
#ifdef SET_SETFILE_BY_SET_CTRL_3AA_ISP
    return SET_SETFILE_BY_SET_CTRL_3AA_ISP;
#else
    return false;
#endif
}

bool ExynosCamera1Parameters::getSetFileCtl3AA(void)
{
#ifdef SET_SETFILE_BY_SET_CTRL_3AA
    return SET_SETFILE_BY_SET_CTRL_3AA;
#else
    return false;
#endif
}

bool ExynosCamera1Parameters::getSetFileCtlISP(void)
{
#ifdef SET_SETFILE_BY_SET_CTRL_ISP
    return SET_SETFILE_BY_SET_CTRL_ISP;
#else
    return false;
#endif
}

bool ExynosCamera1Parameters::getSetFileCtlSCP(void)
{
#ifdef SET_SETFILE_BY_SET_CTRL_SCP
    return SET_SETFILE_BY_SET_CTRL_SCP;
#else
    return false;
#endif
}

bool ExynosCamera1Parameters::getDualCameraMode(void)
{

#ifndef SAMSUNG_DUAL_SOLUTION
    /*
    * Before setParameters, we cannot know dualCameraMode is valid or not
    * So, check and make assert for fast debugging
    */
#ifdef USE_DUAL_CAMERA
    if (m_flagCheckDualCameraMode == false)
        android_printAssert(NULL, LOG_TAG, "Cannot call getDualCameraMode befor checkDualCameraMode, assert!!!!");
#endif
#endif
    return m_cameraInfo.dualCameraMode;
}

#ifdef USE_DUAL_CAMERA
status_t ExynosCamera1Parameters::checkDualCameraMode(const CameraParameters& params)
{
    bool flagDualCameraMode = false;
    int newDualCameraMode = params.getInt("dual_camera_mode");

    CLOGD("DEBUG(%s):newDualCameraMode : %d", "setParameters", newDualCameraMode);

    if (newDualCameraMode == 1) {
        /* forcely set the dual mode, it makes exynoscamera run like PIP path */
        setDualMode(true);

        /* dual_camera_mode */
        CLOGD("DEBUG(%s):newDualCameraMode : %d", "setParameters", newDualCameraMode);
        flagDualCameraMode = true;

        m_params.set("dual_camera_mode", newDualCameraMode);
    }

    setDualCameraMode(flagDualCameraMode);

    return NO_ERROR;
}

void ExynosCamera1Parameters::setDualCameraMode(bool toggle)
{
    m_cameraInfo.dualCameraMode = toggle;
    /* dualMode is confirmed */
    m_flagCheckDualCameraMode = true;
}

bool ExynosCamera1Parameters::isFusionEnabled(void)
{
    bool ret = false;

#ifdef USE_DUAL_CAMERA_PREVIEW_FUSION
    if (getDualCameraMode() == true) {
        ret = true;
    }
#endif

    return ret;
}

void ExynosCamera1Parameters::updateDualCameraFusionMode()
{
    if (getDualCameraMode() == true) {
        /* fusion disabled case */
        /* 1. UHD recording */
        if (getUHDRecordingMode() == true) {
            m_flagDualCameraPreviewFusionMode = true;
            m_flagDualCameraCaptureFusionMode = true;
        } else {
            m_flagDualCameraPreviewFusionMode = true;
            m_flagDualCameraCaptureFusionMode = true;
        }
    } else {
        m_flagDualCameraPreviewFusionMode = false;
        m_flagDualCameraCaptureFusionMode = false;
        CLOGI("Dual camera fusion mode is not supported");
    }
}

bool ExynosCamera1Parameters::getDualCameraPreviewFusionMode(void)
{
    return m_flagDualCameraPreviewFusionMode;
}

bool ExynosCamera1Parameters::getDualCameraCaptureFusionMode(void)
{
    return m_flagDualCameraCaptureFusionMode;
}

status_t ExynosCamera1Parameters::getFusionSize(int w, int h, ExynosRect *srcRect, ExynosRect *dstRect)
{
    status_t ret = NO_ERROR;

    ret = m_getFusionSize(w, h, srcRect, true);
    if (ret != NO_ERROR) {
        CLOGE("m_getFusionSize(%d, %d, true) fail", w, h);
        return ret;
    }

    ret = m_getFusionSize(w, h, dstRect, false);
    if (ret != NO_ERROR) {
        CLOGE("m_getFusionSize(%d, %d, false) fail", w, h);
        return ret;
    }

    return ret;
}

status_t ExynosCamera1Parameters::m_getFusionSize(int w, int h, ExynosRect *rect, bool flagSrc)
{
    status_t ret = NO_ERROR;

    if (w <= 0 || h <= 0) {
        CLOGE("w(%d) <= 0 || h(%d) <= 0", w, h);
        return INVALID_OPERATION;
    }

    rect->x = 0;
    rect->y = 0;

    rect->w = w;
    rect->h = h;

    rect->fullW = rect->w;
    rect->fullH = rect->h;

    rect->colorFormat = getHwPreviewFormat();

#ifdef SAMSUNG_DUAL_SOLUTION
    int wideW, wideH, teleW, teleH, dstW, dstH;
    getDualSolutionSize(&dstW, &dstH, &wideW, &wideH, &teleW, &teleH);
    if (flagSrc == true) {
        if (getCameraId() == CAMERA_ID_BACK) {
            rect->w = wideW;
            rect->h = wideH;
        } else if (getCameraId() == CAMERA_ID_BACK_1) {
            rect->w = teleW;
            rect->h = teleH;
        }
    } else {
        rect->w = dstW;
        rect->h = dstH;
    }

    rect->fullW = rect->w;
    rect->fullH = rect->h;
#endif

#ifdef USE_CP_FUSION_LIB
    /*
     * wide's input size = ALIGN_UP(output size * 1.3, 64);
     * tele's input size = ALIGN_UP(output size * 1.5, 64);

     * ex :
     * 1440 x 1080(output size) : 1920 x 1408(wide's input), 2176 x 1664(tele's input)
     * 1920 x 1080(output size) : 2496 x 1408(wide's input), 2880 x 1664(tele's input)
     */

    bool findLut = false;

    float CPInputConstant = 1.0f;

    float CpWideInputConstant = 1.3f;
    float CpTeleInputConstant = 1.5f;

    int   CpWidthAlignConstant = 64;
    int   CpHeightAlignConstant = 2;

    switch (getCameraId()) {
    case CAMERA_ID_BACK:
        CPInputConstant = CpWideInputConstant;
        break;
    case CAMERA_ID_FRONT:
    case CAMERA_ID_BACK_1:
    case CAMERA_ID_FRONT_1:
        CPInputConstant = CpTeleInputConstant;
        break;
    default:
       CLOGE("no matched cameraId(%d)", getCameraId());
       ret = INVALID_OPERATION;
       goto err;
       break;
    }

//#define LIMIT_FUSION_SIZE_BY_OUTPUTSIZE

    if (flagSrc == true) {
#ifdef LIMIT_FUSION_SIZE_BY_OUTPUTSIZE
            rect->w = w;
            rect->h = h;
#else
        int sizeList[SIZE_LUT_INDEX_END];

        if (3840 <= w || 2160 <= h) {
            rect->w = w;
            rect->h = h;
        } else if (m_getPreviewSizeList(sizeList) == NO_ERROR) {
            rect->w = (sizeList[BDS_W] < sizeList[TARGET_W])? sizeList[BDS_W] : sizeList[TARGET_W];
            rect->h = (sizeList[BDS_H] < sizeList[TARGET_H])? sizeList[BDS_H] : sizeList[TARGET_H];
        } else {
            rect->w = ALIGN_UP((int)(w * CPInputConstant), CpWidthAlignConstant);
            rect->h = ALIGN_UP((int)(h * CPInputConstant), CpHeightAlignConstant);
        }
#endif
    } else {
        int previewW = 0, previewH = 0;
        getPreviewSize(&previewW, &previewH);
        rect->w = previewW;
        rect->h = previewH;
    }

    findLut = true;

    if (findLut == true) {
#if 0
        /* if crop size is bigger than bcrop, just adjust bcrop size */
        /*
        ExynosRect srcBayerCropRect, dstBayerCropRect;
        getPreviewBayerCropSize(&srcBayerCropRect, &dstBayerCropRect);

        if (dstBayerCropRect.w < rect->w ||
            dstBayerCropRect.h < rect->h) {
            CLOGW("calibrate Fusion LUT size(%d x %d -> %d x %d) by bcrop size(%d x %d)",
                rect->w, rect->h, w, h, dstBayerCropRect.w, dstBayerCropRect.h);

            rect->w = w;
            rect->h = h;
        }
        */

        int bcropW, bcropH, bcropX, bcropY = 0;
        getHwBayerCropRegion(&bcropW, &bcropH, &bcropX, &bcropY);

        if (bcropW < rect->w ||
            bcropH < rect->h) {
            CLOGW("calibrate Fusion LUT size(%d x %d -> %d x %d) by bcrop size(%d x %d)",
                rect->w, rect->h, bcropW, bcropH, bcropW, bcropH);

            rect->w = bcropW;
            rect->h = bcropH;
        }
#endif

        rect->fullW = rect->w;
        rect->fullH = rect->h;
        rect->colorFormat = getHwPreviewFormat();

        CLOGV("fusion %s size (%d x %d) according to(%d x %d)",
            (flagSrc == true ? "src" : "dst"), rect->w, rect->h, w, h);
        return ret;
    }

err:
    CLOGE("just set input size(%d, %d) == output size(%d, %d) for Fusion lib", w, h, w, h);
#endif

    return ret;
}

bool ExynosCamera1Parameters::m_updateTpuParameters(void)
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

#ifdef USE_MCSC_FOR_FD
bool ExynosCamera1Parameters::m_setFDConfig(CONFIG_FD::TAG tag, ExynosFDConfig_t *config)
{
    Mutex::Autolock lock(m_fdConfigLock);

    bool ret = true;

    switch (tag) {
    case CONFIG_FD::ENABLE:
        if (m_fdConfig.flagFdEnable != config->flagFdEnable) {
            CLOGD("setFDConfig tag(%d) prev(%d) cur(%d)", tag, m_fdConfig.flagFdEnable, config->flagFdEnable);
            m_fdConfig.flagFdEnable = config->flagFdEnable;
        }
        break;
    case CONFIG_FD::SIZE:
        if (m_fdConfig.width != config->width
            || m_fdConfig.height != config->height) {
            CLOGD("setFDConfig tag(%d) prev(%d x %d) cur(%d x %d)", tag, m_fdConfig.width, m_fdConfig.height, config->width, config->height);
            m_fdConfig.width = config->width;
            m_fdConfig.height = config->height;
        }
        break;
    case CONFIG_FD::PIPE_INFO:
        if (m_fdConfig.pipeId != config->pipeId
            || m_fdConfig.portId != config->portId) {
            CLOGD("setFDConfig tag(%d) prev(pipeId %d portId %d) cur(pipeId %d portId %d)", tag, m_fdConfig.pipeId, m_fdConfig.portId, config->pipeId, config->portId);
            m_fdConfig.pipeId = config->pipeId;
            m_fdConfig.portId = config->portId;
        }
        break;
    case CONFIG_FD::BASE:
    case CONFIG_FD::MAX:
    default:
        ret = false;
        CLOGE("setFDConfig valid tag(%d)", tag);
        break;
    }

    return ret;
}

ExynosFDConfig_t *ExynosCamera1Parameters::m_getFDConfig()
{
    Mutex::Autolock lock(m_fdConfigLock);
    return &m_fdConfig;
}

bool ExynosCamera1Parameters::m_updateVRAParameters(void)
{
    status_t ret = NO_ERROR;
    /* 1. check VRA condition. */
    bool flag = false;
    int width = 0;
    int height = 0;

    if (getDualCameraMode() == true
        && getRecordingHint() == true) {
        getVideoSize(&width, &height);
#ifdef USE_MCSC_FOR_FD_WQHDRECORDING
        if (width == 2560 && height == 1440) {
            flag = USE_MCSC_FOR_FD_WQHDRECORDING;
        }
#endif

#ifdef USE_MCSC_FOR_FD_UHDRECORDING
        if (width == 3840 && height == 2160) {
            flag = USE_MCSC_FOR_FD_UHDRECORDING;
        }
#endif
    }

    ExynosFDConfig_t config;

    config.flagFdEnable = flag;
    config.width = USE_MCSC_FOR_FD_INFO_WIDTH;
    config.height = USE_MCSC_FOR_FD_INFO_HEIGHT;
    config.pipeId = PIPE_ISP;
    if (flag) {
        config.portId = PIPE_MCSC2;
    } else {
        config.portId = PIPE_MCSC0;
    }

    /* 1. update VRA flag. */
    m_setFDConfig(CONFIG_FD::ENABLE, &config);
    /* update VRA size. */
    m_setFDConfig(CONFIG_FD::SIZE, &config);
    /* 3. update VRA port. */
    m_setFDConfig(CONFIG_FD::PIPE_INFO, &config);

    return true;
}
#endif

status_t ExynosCamera1Parameters::setFusionInfo(camera2_shot_ext *shot_ext)
{
    status_t ret = NO_ERROR;

    if (shot_ext == NULL) {
        CLOGE("shot_ext == NULL. so, fail");
        return INVALID_OPERATION;
    }

    ExynosRect pictureRect;
    getPictureSize(&pictureRect.w, &pictureRect.h);
    pictureRect.fullW = pictureRect.w;
    pictureRect.fullH = pictureRect.h;

    ExynosCameraFusionMetaDataConverter::translate2Parameters(m_cameraId, &m_params, shot_ext, (DOF *)m_staticInfo->dof, pictureRect);

    return ret;
}

DOF *ExynosCamera1Parameters::getDOF(void)
{
    return (DOF *)m_staticInfo->dof;
}

ExynosCameraParameters::dual_standby_mode_t  ExynosCamera1Parameters::getDualStandbyMode(void)
{
    Mutex::Autolock lock(m_parameterLock);

    return m_dualStandbyMode;
}

void ExynosCamera1Parameters::setDualStandbyMode(ExynosCameraParameters::dual_standby_mode_t dualStandbyMode)
{
    Mutex::Autolock lock(m_parameterLock);

    if (m_dualStandbyMode != dualStandbyMode)
        CLOGI("standbyMode %d -> %d");

    m_dualStandbyMode = dualStandbyMode;

    switch (dualStandbyMode) {
    case ExynosCameraParameters::DUAL_STANDBY_MODE_ACTIVE_IN_POST:
        m_wakeupFinishCount = 1; /* notify at first frame */
        setDualStableFromStandby(false);
        break;
    case ExynosCameraParameters::DUAL_STANDBY_MODE_ACTIVE_IN_SENSOR:
        m_wakeupFinishCount = DUAL_WAKEUP_FINISH_COUNT;
        setDualStableFromStandby(false);
        break;
    default:
        break;
    }
}

void ExynosCamera1Parameters::updateDualCameraSyncType(int zoom, sync_type_t *syncType, sync_type_t *reprocessingSyncType)
{
    int masterCameraId = -1;
    int slaveCameraId = -1;
    *syncType = SYNC_TYPE_BASE;
    *reprocessingSyncType = SYNC_TYPE_BASE;

    Mutex::Autolock lock(m_parameterLock);

    getDualCameraId(&masterCameraId, &slaveCameraId);

    if (getFlagForceSwitchingOnly() == true) {
        if (zoom > DUAL_SWITCHING_SYNC_MODE_MAX_ZOOM_LEVEL) {
            /* SWITCH MODE */
            *syncType = SYNC_TYPE_SWITCH;
            *reprocessingSyncType = SYNC_TYPE_SWITCH;
        } else {
            /* BYPASS MODE */
            *syncType = SYNC_TYPE_BYPASS;
            *reprocessingSyncType = SYNC_TYPE_BYPASS;
        }
    } else {
        /*
         *  BYPASS : x0 ~ DUAL_SYNC_MODE_MIN_ZOOM_LEVEL
         *  FUSION : DUAL_SYNC_MODE_MIN_ZOOM_LEVEL ~
         *           DUAL_SYNC_MODE_MAX_ZOOM_LEVEL
         *  SWITCH : ~ DUAL_SYNC_MODE_MAX_ZOOM_LEVEL
         */
        if (zoom > DUAL_PREVIEW_SYNC_MODE_MAX_ZOOM_LEVEL) {
            /* SWITCH MODE */
            *syncType = SYNC_TYPE_SWITCH;
        } else if (zoom < DUAL_PREVIEW_SYNC_MODE_MIN_ZOOM_LEVEL) {
            /* BYPASS MODE */
            *syncType = SYNC_TYPE_BYPASS;
#ifdef SAMSUNG_DUAL_SOLUTION
            m_dualParameters->setForceWide(m_cameraId, false);
#endif
        } else {
            /* SYNC MODE */
            *syncType = SYNC_TYPE_SYNC;
#ifdef SAMSUNG_DUAL_SOLUTION
            m_dualParameters->setForceWide(m_cameraId, false);
#endif
        }

        /* reprocessingSyncType */
        if (zoom > DUAL_CAPTURE_SYNC_MODE_MAX_ZOOM_LEVEL) {
            /* SWITCH MODE */
            *reprocessingSyncType = SYNC_TYPE_SWITCH;
        } else if (zoom < DUAL_CAPTURE_SYNC_MODE_MIN_ZOOM_LEVEL) {
            /* BYPASS MODE */
            *reprocessingSyncType = SYNC_TYPE_BYPASS;
        } else {
            /* SYNC MODE */
            *reprocessingSyncType = SYNC_TYPE_SYNC;
        }
    }

    m_dualParameters->setDualCameraSyncType(m_cameraId, syncType, reprocessingSyncType);
}

sync_type_t ExynosCamera1Parameters::getDualCameraSyncType(void)
{
    return m_dualParameters->getDualCameraSyncType(m_cameraId);
}

sync_type_t ExynosCamera1Parameters::getDualCameraReprocessingSyncType(void)
{
    return m_dualParameters->getDualCameraReprocessingSyncType(m_cameraId);
}

void ExynosCamera1Parameters::lockDualCameraSyncType(bool lockFlag)
{
    m_dualParameters->lockDualCameraSyncType(m_cameraId, lockFlag);
}

int ExynosCamera1Parameters::setDualTransitionCount(int count)
{
    Mutex::Autolock lock(m_transitionLock);

    if (count <= 0) {
        CLOGI("count(%d -> %d)", m_transitionCount, count);
        m_transitionCount = count;
    } else {
        if (m_smoothTransitionCount <= 0) {
            if ((m_transitionCount <= 0 && getFlagForceSwitchingOnly() == false) ||
                    (getFlagForceSwitchingOnly() == true)) {
                CLOGI("count(%d -> %d)", m_transitionCount, count);
                m_transitionCount = count;
            }
        }
    }

    return m_transitionCount;
}

int ExynosCamera1Parameters::getDualTransitionCount(void)
{
    Mutex::Autolock lock(m_transitionLock);

    return m_transitionCount;
}

int ExynosCamera1Parameters::decreaseDualTransitionCount(void)
{
    Mutex::Autolock lock(m_transitionLock);

    CLOGV("count(%d -> %d)", m_transitionCount, m_transitionCount - 1);

    if (m_transitionCount >=0)
        m_transitionCount--;

    return m_transitionCount;
}

int ExynosCamera1Parameters::setDualSmoothTransitionCount(int count)
{
    Mutex::Autolock lock(m_transitionLock);

    if (getFlagForceSwitchingOnly()) {
        m_smoothTransitionCount = 0;
        return 0;
    }

    if (count <= 0)
        CLOGI("count(%d -> %d)", m_smoothTransitionCount, count);
    else
        CLOGV("count(%d -> %d)", m_smoothTransitionCount, count);

    m_smoothTransitionCount = count;

    return m_smoothTransitionCount;
}

int ExynosCamera1Parameters::getDualSmoothTransitionCount(void)
{
    Mutex::Autolock lock(m_transitionLock);

    if (getFlagForceSwitchingOnly())
        return 0;

    return m_smoothTransitionCount;
}

int ExynosCamera1Parameters::decreaseDualSmoothTransitionCount(void)
{
    Mutex::Autolock lock(m_transitionLock);

    CLOGV("count(%d -> %d)", m_smoothTransitionCount, m_smoothTransitionCount - 1);

    if (m_transitionCount <= 0 && m_smoothTransitionCount >=0)
        m_smoothTransitionCount--;

    return m_smoothTransitionCount;
}

int ExynosCamera1Parameters::setDualWakeupFinishCount(int count)
{
    Mutex::Autolock lock(m_transitionLock);

    m_wakeupFinishCount = count;

    return count;
}

int ExynosCamera1Parameters::getDualWakeupFinishCount(void)
{
    Mutex::Autolock lock(m_transitionLock);

    return m_wakeupFinishCount;
}

int ExynosCamera1Parameters::decreaseDualWakeupFinishCount(void)
{
    Mutex::Autolock lock(m_transitionLock);

    CLOGV("count(%d -> %d)", m_wakeupFinishCount, m_wakeupFinishCount - 1);

    if (m_wakeupFinishCount > 0)
        m_wakeupFinishCount--;

    return m_wakeupFinishCount;
}

void ExynosCamera1Parameters::clearDualCameraInformation(void)
{
    int masterCameraId, slaveCameraId;
    getDualCameraId(&masterCameraId, &slaveCameraId);

    Mutex::Autolock lock(m_transitionLock);

    m_dualStandbyMode = DUAL_STANDBY_MODE_INACTIVE;
    m_transitionCount = 0;
    m_smoothTransitionCount = 0;
    m_wakeupFinishCount = 0;

    if (m_cameraId == masterCameraId)
        m_dualParameters->clearDualCameraInformation(m_cameraId);
}

void ExynosCamera1Parameters::getDualBackupPostStandbyFps(uint32_t *min, uint32_t *max)
{
    if (min != NULL && max != NULL) {
        *min = m_backupDualPostStandbyMinFps / 1000;
        *max = m_backupDualPostStandbyMaxFps / 1000;
    }
}

#ifdef DUAL_SMOOTH_TRANSITION_LAUNCH
void ExynosCamera1Parameters::finishStartPreview(int32_t targetCameraId)
{
    m_dualParameters->finishStartPreview(m_cameraId, targetCameraId);
}

bool ExynosCamera1Parameters::getFlagFinishStartPreview(int32_t targetCameraId)
{
    return m_dualParameters->getFlagFinishStartPreview(m_cameraId, targetCameraId);
}
#endif

void ExynosCamera1Parameters::setForceSwitchingOnly(bool forceSwitchingOnly)
{
    m_dualParameters->setForceSwitchingOnly(m_cameraId, forceSwitchingOnly);
}

bool ExynosCamera1Parameters::getFlagForceSwitchingOnly(void)
{
    return m_dualParameters->getFlagForceSwitchingOnly(m_cameraId);
}

void ExynosCamera1Parameters::setDualStableFromStandby(bool stable)
{
    Mutex::Autolock lock(m_transitionLock);

    if (m_dualStableFromStandby != stable) {
        CLOGI("dualStable (%d -> %d)", m_dualStableFromStandby, stable);
        m_dualStableFromStandby = stable;
    }
}

bool ExynosCamera1Parameters::getDualStableFromStandby(void)
{
    Mutex::Autolock lock(m_transitionLock);

    return m_dualStableFromStandby;
}

void ExynosCamera1Parameters::registerDualNotifyCallback(ExynosCamera *camera, dual_notify_func_t notifyFunc)
{
    m_dualParameters->registerNotifyCallback(m_cameraId, camera, notifyFunc);
}

void ExynosCamera1Parameters::unregisterDualNotifyCallback(void)
{
    m_dualParameters->unregisterNotifyCallback(m_cameraId);
}

status_t ExynosCamera1Parameters::dualNotify(enum dual_camera_notify_type notifyType,
                                             bool useThread,
                                             int32_t arg1,
                                             int32_t arg2,
                                             void* arg3)
{
    return m_dualParameters->notify(m_cameraId, notifyType, useThread, arg1, arg2, arg3);
}

#ifdef USE_CP_FUSION_LIB
char * ExynosCamera1Parameters::readFusionCalData(int *readSize)
{
    FILE *fp = NULL;
    char *calData = NULL;
    int calDataSize = 0;

    fp = fopen("/vendor/firmware/FusionCalibrationParams.bin", "rb");
    if (fp == NULL) {
        CLOGE("fopen(/vendor/firmware/FusionCalibrationParams.bin) fail");
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    calDataSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    calDataSize + 1;
    calData = new char[calDataSize];

    // Read complete data
    size_t result = fread(calData, 1, calDataSize, fp);
    if(result != calDataSize) {
        CLOGE("failed to read caldata. result(%d), calDataSize(%d)", result, calDataSize);

        delete calData;
        calData = NULL;
    } else {
        *readSize = calDataSize;
    }

    fclose(fp);

    if (calData) {
        CLOGD("calDataSize(%d) : calData(%s)", calDataSize, calData);
    } else {
        CLOGD("calDataSize(%d)", calDataSize);
    }

    return calData;
}

void ExynosCamera1Parameters::setFusionCalData(char *addr, int size)
{
    char *calData = new char[size + 1];

    memcpy(calData, addr, size);
    calData[size] = '\0';

    CLOGD("calib_data is addr(%p) size(%d) str(%s)", addr, size, calData);

    m_params.set("calib_data", calData);

    delete calData;
}

char *ExynosCamera1Parameters::getFusionCalData(int *size)
{
    *size = m_fusionCalDataSize;

    return m_fusionCalData;
}
#endif // USE_CP_FUSION_LIB

bool ExynosCamera1Parameters::isFastenAeStableEnable(void)
{
    bool ret = false;
    /*
     *              index  LIVE_OUTFOCUS getDualCameraMode getDualMode W(fastenAe) T(fastenAe)  Front(other)
     *    single      1       FALSE           FALSE           FALSE      ENABLE       ENABLE      ENABLE
     *    PIP         2       FALSE           FALSE           TRUE       DISABLE      DISABLE      NONE
     *    1-device    3       FALSE           TRUE            TRUE       ENABLE       DISABLE      NONE
     *    2-device    4       TRUE            FALSE           FALSE      DISABLE      ENABLE       NONE
     */

    bool dualCameraMode = false;
    bool dualMode = false;
    int shotMode = 0;
    int cameraId = getCameraId();

    dualCameraMode = getDualCameraMode();
    dualMode = getDualMode();
    shotMode = getShotMode();

    switch(cameraId) {
        case CAMERA_ID_BACK_0:
            /* index :1 / 3 */
            if (shotMode != SHOT_MODE_LIVE_OUTFOCUS
                && ((dualCameraMode == false && dualMode == false) || (dualCameraMode == true && dualMode == true))) {
                ret = true;
            } else {
                ret = false;
            }
            break;
        case CAMERA_ID_BACK_1:
            /* index :2 / 3 */
            if (shotMode != SHOT_MODE_LIVE_OUTFOCUS
                && ((dualCameraMode == false && dualMode == true) || (dualCameraMode == true && dualMode == true))) {
                ret = false;
            } else {
                ret = true;
            }
            break;
        default:
            /* index :1 */
            if (shotMode != SHOT_MODE_LIVE_OUTFOCUS && dualCameraMode == false && dualMode == false) {
                ret = true;
            } else {
                ret = false;
            }
            break;
    }

    return ret;
}

#ifdef USE_MCSC_FOR_FD
ExynosFDConfig_t *ExynosCamera1Parameters::getFDConfig()
{
    return m_getFDConfig();
}
#endif // USE_MCSC_FOR_FD

#endif // USE_DUAL_CAMERA

void ExynosCamera1Parameters::switchSensorMode()
{
    CLOGD("%d -> %d", m_isSwitchSensor, !m_isSwitchSensor);
    m_isSwitchSensor = (m_isSwitchSensor == true)? false : true;
}

}; /* namespace android */
