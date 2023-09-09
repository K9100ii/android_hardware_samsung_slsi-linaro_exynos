/*
 * Copyright (C) 2017, Samsung Electronics Co. LTD
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

/* #define LOG_NDEBUG 0 */
#define LOG_TAG "ExynosCameraInterface"
#include <log/log.h>

#include "ExynosCameraInterface.h"
#include "ExynosCameraAutoTimer.h"

#ifdef SAMSUNG_TN_FEATURE
#include "SecCameraVendorTags.h"
#endif

namespace android {

/* Convert Id to the one for HAL. Refer to the enum CAMERA_ID in ExynosCameraSensorInfoBase.h  */
static int HAL_getCameraId(int id)
{
    int cameraId = id;
    switch (cameraId) {
    case CAMERA_REMAP_ID_IRIS:
        cameraId = CAMERA_ID_SECURE;
        break;
#ifdef USE_DUAL_CAMERA
    case CAMERA_REMAP_ID_TELE:
        cameraId = CAMERA_ID_BACK_1;
        break;
    case CAMERA_REMAP_ID_DUAL_BACK:
        cameraId = CAMERA_ID_BACK_0;
        break;
    case CAMERA_REMAP_ID_DUAL_FRONT:
        cameraId = CAMERA_ID_FRONT_0;
        break;
#endif
    default:
        break;
    }

    return cameraId;
}

static int HAL3_camera_device_open(const struct hw_module_t* module,
                                    const char *id,
                                    struct hw_device_t** device)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    int cameraId = atoi(id);
    enum CAMERA_STATE state;
    FILE *fp = NULL;
    int ret = 0;

    CameraMetadata metadata;
    camera_metadata_entry flashAvailable;
    bool hasFlash = false;
    char flashFilePath[100] = {'\0',};
    uint32_t numOfSensors = 1;
#ifdef USE_DUAL_CAMERA
    int slaveCameraId = -1;

    switch (cameraId) {
        case CAMERA_REMAP_ID_DUAL_BACK:
            slaveCameraId = CAMERA_ID_BACK_1;
            numOfSensors = 2;
            break;
        case CAMERA_REMAP_ID_DUAL_FRONT:
            slaveCameraId = CAMERA_ID_FRONT_1;
            numOfSensors = 2;
            break;
        //HACK: always use to dual camera
        case CAMERA_ID_BACK_0:
            slaveCameraId = CAMERA_ID_BACK_1;
            numOfSensors = 2;
            break;
        default:
            break;
    }
#endif

    cameraId = HAL_getCameraId(cameraId);

    /* Validation check */
    ALOGI("INFO(%s[%d]):camera(%d) in ======", __FUNCTION__, __LINE__, cameraId);
    if (cameraId < 0 || cameraId >= HAL_getNumberOfCameras()) {
        if (cameraId < CAMERA_ID_HIDDEN_START || cameraId >= CAMERA_ID_HIDDEN_START + HAL_getNumberOfHiddenCameras()) {
            ALOGE("ERR(%s[%d]):Invalid camera ID %d", __FUNCTION__, __LINE__, cameraId);
            return -EINVAL;
        }
    }

    /* Check init thread state */
    if (g_thread) {
        ret = pthread_join(g_thread, NULL);
        if (ret != 0) {
            ALOGE("ERR(%s[%d]):pthread_join failed with error code %d",  __FUNCTION__, __LINE__, ret);
        }
        g_thread = 0;
    }

    /* Setting status and check current status */
    state = CAMERA_OPENED;
    if (check_camera_state(state, cameraId) == false) {
        ALOGE("ERR(%s[%d]):camera(%d) state(%d) is INVALID", __FUNCTION__, __LINE__, cameraId, state);
        return -EUSERS;
    }

    /* Create camera device */
    if (g_cam_device3[cameraId]) {
        ALOGE("ERR(%s[%d]):returning existing camera ID(%d)", __FUNCTION__, __LINE__, cameraId);
        *device = (hw_device_t *)g_cam_device3[cameraId];
        goto done;
    }

    g_cam_device3[cameraId] = (camera3_device_t *)malloc(sizeof(camera3_device_t));
    if (!g_cam_device3[cameraId])
        return -ENOMEM;

    g_cam_openLock[cameraId].lock();
    g_cam_device3[cameraId]->common.tag     = HARDWARE_DEVICE_TAG;
    g_cam_device3[cameraId]->common.version = CAMERA_DEVICE_API_VERSION_3_5;
    g_cam_device3[cameraId]->common.module  = const_cast<hw_module_t *>(module);
    g_cam_device3[cameraId]->common.close   = HAL3_camera_device_close;
    g_cam_device3[cameraId]->ops            = &camera_device3_ops;

    ALOGV("DEBUG(%s[%d]):open camera(%d)", __FUNCTION__, __LINE__, cameraId);
    g_cam_device3[cameraId]->priv = new ExynosCamera(cameraId, &g_cam_info[cameraId], numOfSensors);
    *device = (hw_device_t *)g_cam_device3[cameraId];

    ALOGI("INFO(%s[%d]):camera(%d) out from new g_cam_device3[%d]->priv()",
        __FUNCTION__, __LINE__, cameraId, cameraId);

    g_cam_openLock[cameraId].unlock();

done:
    cam_stateLock[cameraId].lock();
    cam_state[cameraId] = state;
    cam_stateLock[cameraId].unlock();

#ifdef USE_DUAL_CAMERA
    if (numOfSensors >= 2) {
        camera3_device_t *cam_device = (camera3_device_t *)(*device);
        obj(cam_device)->setDualMode(true);

        cam_stateLock[slaveCameraId].lock();
        cam_state[slaveCameraId] = state;
        cam_stateLock[slaveCameraId].unlock();
    }
#endif

    if (g_cam_info[cameraId]) {
        metadata = g_cam_info[cameraId];
        flashAvailable = metadata.find(ANDROID_FLASH_INFO_AVAILABLE);

        ALOGV("INFO(%s[%d]): cameraId(%d), flashAvailable.count(%d), flashAvailable.data.u8[0](%d)",
            __FUNCTION__, __LINE__, cameraId, (int)flashAvailable.count, flashAvailable.data.u8[0]);

        if (flashAvailable.count == 1 && flashAvailable.data.u8[0] == 1) {
            hasFlash = true;
        } else {
            hasFlash = false;
        }
    }

    /* Turn off torch and update torch status */
    if(hasFlash && g_cam_torchEnabled[cameraId]) {
        if (cameraId == CAMERA_ID_BACK) {
            snprintf(flashFilePath, sizeof(flashFilePath), TORCH_REAR_FILE_PATH);
        } else {
            snprintf(flashFilePath, sizeof(flashFilePath), TORCH_FRONT_FILE_PATH);
        }

        fp = fopen(flashFilePath, "w+");
        if (fp == NULL) {
            ALOGE("ERR(%s[%d]):torch file open fail", __FUNCTION__, __LINE__);
        } else {
            fwrite("0", sizeof(char), 1, fp);
            fflush(fp);
            fclose(fp);

            g_cam_torchEnabled[cameraId] = false;
        }
    }

    g_callbacks->torch_mode_status_change(g_callbacks, id, TORCH_MODE_STATUS_NOT_AVAILABLE);

    ALOGI("INFO(%s[%d]):camera(%d) out =====", __FUNCTION__, __LINE__, cameraId);

    return 0;
}

static int HAL3_camera_device_close(struct hw_device_t* device)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    uint32_t cameraId = 0;
    int ret = OK;
    enum CAMERA_STATE state;
    char camid[10];

    ALOGI("INFO(%s[%d]):in =====", __FUNCTION__, __LINE__);

    if (device) {
        camera3_device_t *cam_device = (camera3_device_t *)device;
        cameraId = obj(cam_device)->getCameraIdOrigin();

        ALOGV("DEBUG(%s[%d]):close camera(%d)", __FUNCTION__, __LINE__, cameraId);

        ret = obj(cam_device)->releaseDevice();
        if (ret) {
            ALOGE("ERR(%s[%d]):initialize error!!", __FUNCTION__, __LINE__);
            ret = BAD_VALUE;
        }

        state = CAMERA_CLOSED;
        if (check_camera_state(state, cameraId) == false) {
            ALOGE("ERR(%s[%d]):camera(%d) state(%d) is INVALID",
                __FUNCTION__, __LINE__, cameraId, state);
            return -1;
        }

        g_cam_openLock[cameraId].lock();
        ALOGV("INFO(%s[%d]):camera(%d) open locked..", __FUNCTION__, __LINE__, cameraId);
        g_cam_device3[cameraId] = NULL;
        g_cam_openLock[cameraId].unlock();
        ALOGV("INFO(%s[%d]):camera(%d) open unlocked..", __FUNCTION__, __LINE__, cameraId);

#ifdef USE_DUAL_CAMERA
        if (obj(cam_device)->getDualMode() == true) {
            int slaveCameraId = -1;
            switch (cameraId) {
            case CAMERA_ID_BACK_0:
                slaveCameraId = CAMERA_ID_BACK_1;
                break;
            case CAMERA_ID_FRONT_0:
                slaveCameraId = CAMERA_ID_FRONT_1;
                break;
            default:
                break;
            }

            cam_stateLock[cameraId].lock();
            cam_state[cameraId] = state;
            cam_stateLock[cameraId].unlock();
        }
#endif

        delete static_cast<ExynosCamera *>(cam_device->priv);
        free(cam_device);

        cam_stateLock[cameraId].lock();
        cam_state[cameraId] = state;
        cam_stateLock[cameraId].unlock();
        ALOGI("INFO(%s[%d]):close camera(%d)", __FUNCTION__, __LINE__, cameraId);
    }

    /* Update torch status */
    g_cam_torchEnabled[cameraId] = false;
    snprintf(camid, sizeof(camid), "%d\n", cameraId);
    g_callbacks->torch_mode_status_change(g_callbacks, camid,
        TORCH_MODE_STATUS_AVAILABLE_OFF);

    ALOGI("INFO(%s[%d]):out =====", __FUNCTION__, __LINE__);
    return ret;
}

static int HAL3_camera_device_initialize(const struct camera3_device *dev,
                                        const camera3_callback_ops_t *callback_ops)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    int ret = OK;
    uint32_t cameraId = obj(dev)->getCameraIdOrigin();
    ALOGI("INFO(%s[%d]):in =====", __FUNCTION__, __LINE__);

    g_cam_configLock[cameraId].lock();

    ALOGE("INFO(%s[%d]): dual cam_state[0](%d)", __FUNCTION__, __LINE__, cam_state[0]);

#ifdef PIP_CAMERA_SUPPORTED
    if (cameraId != 0 && g_cam_device3[0] != NULL
        && cam_state[0] != CAMERA_NONE && cam_state[0] != CAMERA_CLOSED) {
        ret = obj(g_cam_device3[0])->getAvailablePIPMode();
        if (ret == false) {
            ALOGE("ERR(%s[%d]):camera(%d) PIP scenario can not be supported after Front camera open.",
                __FUNCTION__, __LINE__, cameraId);
            g_cam_configLock[cameraId].unlock();
            return BAD_VALUE;
        }

        /* Set PIP mode into Rear Camera */
        ret = obj(g_cam_device3[0])->setPIPMode(true);
        if (ret != NO_ERROR) {
            ALOGE("ERR(%s[%d]):camera(%d) set pipe mode fail. ret %d",
                    __FUNCTION__, __LINE__, 0, ret);
        } else {
            ALOGI("INFO(%s[%d]):camera(%d) set pip mode. Restart stream.",
                    __FUNCTION__, __LINE__, 0);
        }

        ret = obj(dev)->setPIPMode(true);
        if (ret != NO_ERROR) {
            ALOGE("ERR(%s[%d]):camera(%d) set pip mode fail, ret(%d)",
                __FUNCTION__, __LINE__, cameraId, ret);
        } else {
            ALOGI("INFO(%s[%d]):camera(%d) set pip mode",
                __FUNCTION__, __LINE__, cameraId);
        }
    } else if (cameraId != 1 && g_cam_device3[1] != NULL
        && cam_state[1] != CAMERA_NONE && cam_state[1] != CAMERA_CLOSED) {
        /* When rear camera is opened after front camera configuration or run, PIP scenario is fail.
         * The rear camera must be opened before front camera configuration for PIP scenario*/
        ret = obj(g_cam_device3[1])->getAvailablePIPMode();
        if (ret == false) {
            ALOGE("ERR(%s[%d]):camera(%d) PIP scenario can not be supported after Front camera open.",
                __FUNCTION__, __LINE__, cameraId);
            g_cam_configLock[cameraId].unlock();
            return BAD_VALUE;
        }

        /* Set PIP mode into Front Camera */
        ret = obj(g_cam_device3[1])->setPIPMode(true);
        if (ret != NO_ERROR) {
            ALOGE("ERR(%s[%d]):camera(%d) set pipe mode fail. ret %d",
                    __FUNCTION__, __LINE__, 0, ret);
        } else {
            ALOGI("INFO(%s[%d]):camera(%d) set pip mode. Restart stream.",
                    __FUNCTION__, __LINE__, 0);
        }

        ret = obj(dev)->setPIPMode(true);
        if (ret != NO_ERROR) {
            ALOGE("ERR(%s[%d]):camera(%d) set pip mode fail, ret(%d)",
                __FUNCTION__, __LINE__, cameraId, ret);
        } else {
            ALOGI("INFO(%s[%d]):camera(%d) set pip mode",
                __FUNCTION__, __LINE__, cameraId);
        }
    }
#endif

    ret = obj(dev)->initializeDevice(callback_ops);
    if (ret) {
        ALOGE("ERR(%s[%d]):initialize error!!", __FUNCTION__, __LINE__);
        ret = BAD_VALUE;
    }
    g_cam_configLock[cameraId].unlock();

    ALOGV("DEBUG(%s):set callback ops - %p", __FUNCTION__, callback_ops);
    ALOGI("INFO(%s[%d]):out =====", __FUNCTION__, __LINE__);
    return ret;
}

static int HAL3_camera_device_configure_streams(const struct camera3_device *dev,
                                                camera3_stream_configuration_t *stream_list)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    int ret = OK;
    uint32_t cameraId = obj(dev)->getCameraIdOrigin();
    ALOGI("INFO(%s[%d]):in =====", __FUNCTION__, __LINE__);
    g_cam_configLock[cameraId].lock();
    ret = obj(dev)->configureStreams(stream_list);
    if (ret) {
        ALOGE("ERR(%s[%d]):configure_streams error!!", __FUNCTION__, __LINE__);
        ret = BAD_VALUE;
    }
    g_cam_configLock[cameraId].unlock();
    ALOGI("INFO(%s[%d]):out =====", __FUNCTION__, __LINE__);
    return ret;
}

static const camera_metadata_t* HAL3_camera_device_construct_default_request_settings(
                                                                const struct camera3_device *dev,
                                                                int type)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    camera_metadata_t *request = NULL;
    status_t res;
    uint32_t cameraId = obj(dev)->getCameraIdOrigin();
    ALOGI("INFO(%s[%d]):in =====", __FUNCTION__, __LINE__);
    g_cam_configLock[cameraId].lock();
    res = obj(dev)->construct_default_request_settings(&request, type);
    if (res) {
        ALOGE("ERR(%s[%d]):constructDefaultRequestSettings error!!", __FUNCTION__, __LINE__);
        g_cam_configLock[cameraId].unlock();
        return NULL;
    }
    g_cam_configLock[cameraId].unlock();
    ALOGI("INFO(%s[%d]):out =====", __FUNCTION__, __LINE__);
    return request;
}

static int HAL3_camera_device_process_capture_request(const struct camera3_device *dev,
                                                        camera3_capture_request_t *request)
{
    /* ExynosCameraAutoTimer autoTimer(__FUNCTION__); */

    int ret = OK;
    ALOGV("INFO(%s[%d]):in =====", __FUNCTION__, __LINE__);
    ret = obj(dev)->processCaptureRequest(request);
    if (ret) {
        ALOGE("ERR(%s[%d]):process_capture_request error(%d)!!", __FUNCTION__, __LINE__, ret);
        ret = BAD_VALUE;
    }
    ALOGV("INFO(%s[%d]):out =====", __FUNCTION__, __LINE__);
    return ret;
}

static int HAL3_camera_device_flush(const struct camera3_device *dev)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    int ret = 0;
    uint32_t cameraId = obj(dev)->getCameraIdOrigin();

    ALOGI("INFO(%s[%d]):in =====", __FUNCTION__, __LINE__);
    g_cam_configLock[cameraId].lock();
    ret = obj(dev)->flush();
    if (ret) {
        ALOGE("ERR(%s[%d]):flush error(%d)!!", __FUNCTION__, __LINE__, ret);
        ret = BAD_VALUE;
    }
    g_cam_configLock[cameraId].unlock();

    ALOGI("INFO(%s[%d]):out =====", __FUNCTION__, __LINE__);
    return ret;
}

#ifdef SAMSUNG_TN_FEATURE
static int HAL3_camera_device_set_parameters(
        const struct camera3_device *dev,
        const char *parms)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    ALOGD("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    String8 str(parms);
    CameraParameters p(str);

    return obj(dev)->setParameters(p);
}
#endif

static void HAL3_camera_device_get_metadata_vendor_tag_ops(const struct camera3_device *dev,
                                                            vendor_tag_query_ops_t* ops)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    ALOGI("INFO(%s[%d]):in =====", __FUNCTION__, __LINE__);

    if (dev == NULL)
        ALOGE("ERR(%s[%d]):dev is NULL", __FUNCTION__, __LINE__);

    if (ops == NULL)
        ALOGE("ERR(%s[%d]):ops is NULL", __FUNCTION__, __LINE__);

    ALOGI("INFO(%s[%d]):out =====", __FUNCTION__, __LINE__);
}

static void HAL3_camera_device_dump(const struct camera3_device *dev, int fd)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    ALOGI("INFO(%s[%d]):in =====", __FUNCTION__, __LINE__);

    if (dev == NULL)
        ALOGE("ERR(%s[%d]):dev is NULL", __FUNCTION__, __LINE__);

    if (fd < 0)
        ALOGE("ERR(%s[%d]):fd is Negative Value", __FUNCTION__, __LINE__);

    ALOGI("INFO(%s[%d]):out =====", __FUNCTION__, __LINE__);
}

/***************************************************************************
 * FUNCTION   : get_camera_info
 *
 * DESCRIPTION: static function to query the numner of cameras
 *
 * PARAMETERS : none
 *
 * RETURN     : the number of cameras pre-defined
 ***************************************************************************/
static int HAL_getNumberOfCameras()
{
    /* ExynosCameraAutoTimer autoTimer(__FUNCTION__); */
    int getNumOfCamera = sizeof(sCameraInfo) / sizeof(sCameraInfo[0]);
    ALOGV("DEBUG(%s[%d]):Number of cameras(%d)", __FUNCTION__, __LINE__, getNumOfCamera);
    return getNumOfCamera;
}

static int HAL_getNumberOfHiddenCameras()
{
    /* ExynosCameraAutoTimer autoTimer(__FUNCTION__); */
    int getNumOfHiddenCamera = sizeof(sCameraHiddenInfo) / sizeof(sCameraHiddenInfo[0]);
    ALOGV("DEBUG(%s[%d]):Number of Hidden Camera(%d)", __FUNCTION__, __LINE__, getNumOfHiddenCamera);
    return getNumOfHiddenCamera;
}

static int HAL_getCameraInfo(int camera_id, struct camera_info *info)
{
    /* ExynosCameraAutoTimer autoTimer(__FUNCTION__); */
    status_t ret = NO_ERROR;

    int cameraId = HAL_getCameraId(camera_id);

    ALOGI("INFO(%s[%d]):in =====", __FUNCTION__, __LINE__);
    if (cameraId < 0 || cameraId >= HAL_getNumberOfCameras()) {
        if (cameraId < CAMERA_ID_HIDDEN_START || cameraId >= CAMERA_ID_HIDDEN_START + HAL_getNumberOfHiddenCameras()) {
            ALOGE("ERR(%s[%d]):Invalid camera ID %d", __FUNCTION__, __LINE__, cameraId);
            return -ENODEV;
        }
    }

    /* set facing and orientation */
    if (cameraId >= CAMERA_ID_HIDDEN_START) {
        if ((cameraId - CAMERA_ID_HIDDEN_START) < (int) (sizeof(sCameraHiddenInfo) / sizeof(sCameraHiddenInfo[0]))) {
            memcpy(info, &sCameraHiddenInfo[cameraId - CAMERA_ID_HIDDEN_START], sizeof(sCameraHiddenInfo[0]));
        } else {
            ALOGE("ERR(%s[%d]):Invalid camera ID %d", __FUNCTION__, __LINE__, cameraId);
            return -ENODEV;
        }
    } else {
        memcpy(info, &sCameraInfo[cameraId], sizeof(sCameraInfo[0]));
    }

    /* set device API version */
    info->device_version = CAMERA_DEVICE_API_VERSION_3_5;

    /* set camera_metadata_t if needed */
    if (info->device_version >= HARDWARE_DEVICE_API_VERSION(2, 0)) {
        if (g_cam_info[cameraId] == NULL) {
            ALOGV("DEBUG(%s[%d]):Return static information (%d)", __FUNCTION__, __LINE__, cameraId);
            ret = ExynosCameraMetadataConverter::constructStaticInfo(cameraId, &g_cam_info[cameraId]);
            if (ret != 0) {
                ALOGE("ERR(%s[%d]): static information is NULL", __FUNCTION__, __LINE__);
                return -EINVAL;
            }
            info->static_camera_characteristics = g_cam_info[cameraId];
        } else {
            ALOGV("DEBUG(%s[%d]):Reuse Return static information (%d)", __FUNCTION__, __LINE__, cameraId);
            info->static_camera_characteristics = g_cam_info[cameraId];
        }
    }

    /* set service arbitration (resource_cost, conflicting_devices, conflicting_devices_length */
    info->resource_cost = sCameraConfigInfo[cameraId].resource_cost;
    info->conflicting_devices = sCameraConfigInfo[cameraId].conflicting_devices;
    info->conflicting_devices_length = sCameraConfigInfo[cameraId].conflicting_devices_length;
    ALOGV("INFO(%s info->resource_cost = %d ", __FUNCTION__, info->resource_cost);
    if (info->conflicting_devices_length) {
        for (size_t i = 0; i < info->conflicting_devices_length; i++) {
            ALOGV("INFO(%s info->conflicting_devices = %s ", __FUNCTION__, info->conflicting_devices[i]);
        }
    } else {
        ALOGV("INFO(%s info->conflicting_devices_length is zero ", __FUNCTION__);
    }

    return NO_ERROR;
}

static int HAL_set_callbacks(const camera_module_callbacks_t *callbacks)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    if (callbacks == NULL)
        ALOGE("ERR(%s[%d]):dev is NULL", __FUNCTION__, __LINE__);

    g_callbacks = callbacks;

    return OK;
}

static int HAL_set_torch_mode(const char* camera_id, bool enabled)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    int cameraId = atoi(camera_id);
    FILE *fp = NULL;
    CameraMetadata metadata;
    camera_metadata_entry flashAvailable;
    int ret = 0;
    char flashFilePath[100] = {'\0',};

    cameraId = HAL_getCameraId(cameraId);

    ALOGI("INFO(%s[%d]):in =====", __FUNCTION__, __LINE__);
    if (cameraId < 0 || cameraId >= HAL_getNumberOfCameras()) {
        if (cameraId < CAMERA_ID_HIDDEN_START || cameraId >= CAMERA_ID_HIDDEN_START + HAL_getNumberOfHiddenCameras()) {
            ALOGE("ERR(%s[%d]):Invalid camera ID %d", __FUNCTION__, __LINE__, cameraId);
            return -EINVAL;
        }
    }

    /* Check the android.flash.info.available */
    /* If this camera device does not support flash, It have to return -ENOSYS */
    metadata = g_cam_info[cameraId];
    flashAvailable = metadata.find(ANDROID_FLASH_INFO_AVAILABLE);

    if (flashAvailable.count == 1 && flashAvailable.data.u8[0] == 1) {
        ALOGV("DEBUG(%s[%d]): Flash metadata exist", __FUNCTION__, __LINE__);
    } else {
        ALOGE("ERR(%s[%d]): Can not find flash metadata", __FUNCTION__, __LINE__);
        return -ENOSYS;
    }

    ALOGI("INFO(%s[%d]): Current Camera State (state = %d)", __FUNCTION__, __LINE__, cam_state[cameraId]);

    /* Add the check the camera state that camera in use or not */
    if (cam_state[cameraId] > CAMERA_CLOSED) {
        ALOGE("ERR(%s[%d]): Camera Device is busy (state = %d)", __FUNCTION__, __LINE__, cam_state[cameraId]);
        g_callbacks->torch_mode_status_change(g_callbacks, camera_id, TORCH_MODE_STATUS_AVAILABLE_OFF);
        return -EBUSY;
    }

    /* Add the sysfs file read (sys/class/camera/flash/torch_flash) then set 0 or 1 */
    if (cameraId == CAMERA_ID_BACK) {
        snprintf(flashFilePath, sizeof(flashFilePath), TORCH_REAR_FILE_PATH);
    } else {
        snprintf(flashFilePath, sizeof(flashFilePath), TORCH_FRONT_FILE_PATH);
    }

    fp = fopen(flashFilePath, "w+");
    if (fp == NULL) {
        ALOGE("ERR(%s[%d]):torch file open(%s) fail",
            __FUNCTION__, __LINE__, flashFilePath);
        return -ENOSYS;
    }

    if (enabled) {
        fwrite("1", sizeof(char), 1, fp);
    } else {
        fwrite("0", sizeof(char), 1, fp);
    }

    fflush(fp);

    ret = fclose(fp);
    if (ret != 0) {
        ALOGE("ERR(%s[%d]): file close failed(%d)", __FUNCTION__, __LINE__, ret);
    }

    if (enabled) {
        g_cam_torchEnabled[cameraId] = true;
        g_callbacks->torch_mode_status_change(g_callbacks,
            camera_id, TORCH_MODE_STATUS_AVAILABLE_ON);
    } else {
        g_cam_torchEnabled[cameraId] = false;
        g_callbacks->torch_mode_status_change(g_callbacks,
            camera_id, TORCH_MODE_STATUS_AVAILABLE_OFF);
    }

    ALOGI("INFO(%s[%d]):out =====", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

void *init_func(__unused void *data)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    FILE *fp = NULL;
    char name[64];

    ALOGI("INFO(%s[%d]):in =====", __FUNCTION__, __LINE__);

    fp = fopen(INIT_MODULE_PATH, "r");
    if (fp == NULL) {
        ALOGI("INFO(%s[%d]):module init file open fail", __FUNCTION__, __LINE__);
        return NULL;
    }

    if (fgets(name, sizeof(name), fp) == NULL) {
        ALOGI("INFO(%s[%d]):failed to read init sysfs", __FUNCTION__, __LINE__);
    }

    fclose(fp);

    ALOGI("INFO(%s[%d]):out =====", __FUNCTION__, __LINE__);

    return NULL;
}

static int HAL_init()
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    int ret = 0;

    ALOGI("INFO(%s[%d]):in =====", __FUNCTION__, __LINE__);

    ret = pthread_create(&g_thread, NULL, init_func, NULL);
    if (ret) {
        ALOGE("ERR(%s[%d]):pthread_create failed with error code %d", __FUNCTION__, __LINE__, ret);
    }

    ALOGI("INFO(%s[%d]):out =====", __FUNCTION__, __LINE__);

    return OK;
}

/**
 * HAL_getPhysicalCameraInfo
 * Returns the static metadata for a physical camera in a logical camera device.
 */
static int HAL_getPhysicalCameraInfo(int physical_camera_id,
           camera_metadata_t **static_metadata)
{
    status_t ret = NO_ERROR;

    int cameraId = HAL_getCameraId(physical_camera_id);

    /*
     *  This function is only called for those physical camera
     * ID(s) that are not exposed independently.
     */
    if (cameraId < 0 || physical_camera_id < HAL_getNumberOfCameras()) {
        ALOGE("ERR(%s[%d]):Invalid camera ID %d , (%d / %d)", __FUNCTION__, __LINE__, cameraId,
             physical_camera_id, HAL_getNumberOfCameras());
        goto FUNC_ERR;
    }

    if (static_metadata == NULL)
	goto FUNC_ERR;

    if (g_cam_info[cameraId] == NULL) {
        ret = ExynosCameraMetadataConverter::constructStaticInfo(cameraId, &g_cam_info[cameraId]);
        if (ret != 0) {
            ALOGE("ERR(%s[%d]): static information is NULL", __FUNCTION__, __LINE__);
            goto FUNC_ERR;
        }
    }

    *static_metadata = g_cam_info[cameraId];
    return NO_ERROR;

FUNC_ERR:
    if (static_metadata)
        *static_metadata = NULL;

    return -EINVAL;
}

/*
 * HAL_isStreamCombinationSupported
 * Check for device support of specific camera stream combination
 */
static int HAL_isStreamCombinationSupported(int camera_id,
           const camera_stream_combination_t *streams)
{
    /* TODO: stream combination query need to be supported */

    ALOGD("(%s[%d]): (%d , %p)", __FUNCTION__, __LINE__, camera_id, streams);

    return -ENOSYS;
}

static void HAL_get_vendor_tag_ops(__unused vendor_tag_ops_t* ops)
{
    ALOGV("INFO(%s):", __FUNCTION__);

#ifdef SAMSUNG_TN_FEATURE
    SecCameraVendorTags::Ops = ops;

    ops->get_all_tags = SecCameraVendorTags::get_ext_all_tags;
    ops->get_tag_count = SecCameraVendorTags::get_ext_tag_count;
    ops->get_tag_type = SecCameraVendorTags::get_ext_tag_type;
    ops->get_tag_name = SecCameraVendorTags::get_ext_tag_name;
    ops->get_section_name = SecCameraVendorTags::get_ext_section_name;
    ops->reserved[0] = NULL;
#else
    ALOGW("WARN(%s[%d]):empty operation", __FUNCTION__, __LINE__);
#endif
}

}; /* namespace android */
