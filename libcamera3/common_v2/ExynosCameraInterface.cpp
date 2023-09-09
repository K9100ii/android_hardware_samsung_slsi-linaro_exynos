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
#include "ExynosCameraTimeLogger.h"

#ifdef SAMSUNG_TN_FEATURE
#include "SecCameraVendorTags.h"
#endif

namespace android {

/* Convert Id to the one for HAL. Refer to the enum CAMERA_ID in ExynosCameraSensorInfoBase.h  */
static int HAL_getCameraId(int serviceCamId, int *mainCamId, int *subCamId, int *scenario, int *camInfoIndex)
{
    uint32_t numOfSensors = 1;
    *scenario = SCENARIO_NORMAL;

    switch (serviceCamId) {
        /* open ID */
#ifdef CAMERA_OPEN_ID_REAR_0
        case CAMERA_OPEN_ID_REAR_0:
            *mainCamId = CAMERA_ID_BACK;
            *subCamId = -1;
            *camInfoIndex = CAMERA_INDEX_REAR_0;
            numOfSensors = 1;
            break;
#endif
#ifdef CAMERA_OPEN_ID_FRONT_1
        case CAMERA_OPEN_ID_FRONT_1:
            *mainCamId = CAMERA_ID_FRONT;
            *subCamId = -1;
            *camInfoIndex = CAMERA_INDEX_FRONT_1;
            numOfSensors = 1;
            break;
#endif
#ifdef CAMERA_OPEN_ID_REAR_2
        case CAMERA_OPEN_ID_REAR_2:
            *mainCamId = CAMERA_ID_BACK_1;
            *subCamId = -1;
            *camInfoIndex = CAMERA_INDEX_REAR_2;
            numOfSensors = 1;
        break;
#endif
        /* in case of 1 device of dual camera */
#ifdef USES_DUAL_REAR_ZOOM
        case CAMERA_SERVICE_ID_DUAL_REAR_ZOOM:
            *mainCamId = CAMERA_ID_BACK;
            *subCamId = CAMERA_ID_BACK_1;
            *camInfoIndex = CAMERA_INDEX_DUAL_REAR_ZOOM;
            numOfSensors = 2;
            *scenario = SCENARIO_DUAL_REAR_ZOOM;
            break;
#endif
#ifdef USES_DUAL_REAR_PORTRAIT
        case CAMERA_SERVICE_ID_DUAL_REAR_PORTRAIT:
            *mainCamId = CAMERA_ID_BACK_1;
            *subCamId = CAMERA_ID_BACK;
            *camInfoIndex = CAMERA_INDEX_DUAL_REAR_PORTRAIT;
            numOfSensors = 2;
            *scenario = SCENARIO_DUAL_REAR_PORTRAIT;
            break;
#endif
#ifdef USES_DUAL_FRONT_PORTRAIT
        case CAMERA_SERVICE_ID_DUAL_FRONT_PORTRAIT:
            *mainCamId = CAMERA_ID_FRONT;
            *subCamId = CAMERA_ID_FRONT_1;
            *camInfoIndex = CAMERA_INDEX_DUAL_FRONT_PORTRAIT;
            numOfSensors = 2;
            *scenario = SCENARIO_DUAL_FRONT_PORTRAIT;
            break;
#endif
        /* in case of 2 devices of dual camera or 1 camera*/
#ifdef USES_IRIS_SECURE
        case CAMERA_SERVICE_ID_IRIS:
            *mainCamId = CAMERA_ID_SECURE;
            *subCamId = -1;
            *camInfoIndex = CAMERA_INDEX_IRIS_SECURE;
            numOfSensors = 1;
            *scenario = SCENARIO_SECURE;
            break;
#endif
#ifdef USE_DUAL_CAMERA
#if defined(MAIN_1_CAMERA_SENSOR_NAME)
        case CAMERA_SERVICE_ID_REAR_2ND:
            if (MAIN_1_CAMERA_SENSOR_NAME != SENSOR_NAME_NOTHING) {
                *mainCamId = CAMERA_ID_BACK_1;
                *subCamId = -1;
                *camInfoIndex = CAMERA_INDEX_HIDDEN_REAR_2;
                numOfSensors = 1;
            } else {
                *mainCamId = -1;
                *subCamId = -1;
                *camInfoIndex = -1;
                numOfSensors = -1;
            }
            break;
#endif
#if defined(FRONT_1_CAMERA_SENSOR_NAME)
        case CAMERA_SERVICE_ID_FRONT_2ND:
            if (FRONT_1_CAMERA_SENSOR_NAME != SENSOR_NAME_NOTHING) {
                *mainCamId = CAMERA_ID_FRONT_1;
                *subCamId = -1;
                *camInfoIndex = CAMERA_INDEX_HIDDEN_FRONT_2;
                numOfSensors = 1;
            } else {
                *mainCamId = -1;
                *subCamId = -1;
                *camInfoIndex = -1;
                numOfSensors = -1;
            }
            break;
#endif
#endif

        default:
            ALOGV("ERR(%s[%d]:Invaild Camera Id(%d)", __FUNCTION__, __LINE__, serviceCamId);
            *mainCamId = -1;
            *subCamId = -1;
            *camInfoIndex = -1;
            numOfSensors = -1;
            break;
    }

    return numOfSensors;
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
    int numOfSensors = 1;
    int mainCameraId = -1;
    int subCameraId = -1;
    int scenario = -1;
    int camInfoIndex = -1;

    numOfSensors = HAL_getCameraId(cameraId, &mainCameraId, &subCameraId, &scenario, &camInfoIndex);

    /* Validation check */
    ALOGI("INFO(%s[%d]):camera(%d) master(%d), slave(%d) camInfoIndex(%d) in ======",
        __FUNCTION__, __LINE__, cameraId, mainCameraId, subCameraId, camInfoIndex);

    if (numOfSensors < 1 || mainCameraId < 0 || camInfoIndex < 0) {
        ALOGE("ERR(%s[%d]):Invalid camera ID%d and numOf Sensors(%d)",
            __FUNCTION__, __LINE__, mainCameraId, numOfSensors);
        return -EINVAL;
    }

#ifdef TIME_LOGGER_LAUNCH_ENABLE
    TIME_LOGGER_INIT(mainCameraId);
#endif
    TIME_LOGGER_UPDATE(mainCameraId, 0, 0, CUMULATIVE_CNT, OPEN_START, 0);

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
    if (check_camera_state(state, mainCameraId) == false) {
        ALOGE("ERR(%s[%d]):camera(%d) state(%d) is INVALID", __FUNCTION__, __LINE__, mainCameraId, state);
        return -EUSERS;
    }

    /* Create camera device */
    if (g_cam_device3[mainCameraId]) {
        ALOGE("ERR(%s[%d]):returning existing camera ID(%d)", __FUNCTION__, __LINE__, mainCameraId);
        *device = (hw_device_t *)g_cam_device3[mainCameraId];
        goto done;
    }

#ifdef USE_DEBUG_PROPERTY
    {
        bool first = true;
        for (int i = 0; i < MAX_NUM_OF_CAMERA; i++) {
            if (g_cam_device3[i]) first = false;
        }
        if (first) LOGMGR_INIT();
    }
#endif

    g_cam_device3[mainCameraId] = (camera3_device_t *)malloc(sizeof(camera3_device_t));
    if (!g_cam_device3[mainCameraId])
        return -ENOMEM;

    g_cam_openLock[mainCameraId].lock();
    g_cam_device3[mainCameraId]->common.tag     = HARDWARE_DEVICE_TAG;
    g_cam_device3[mainCameraId]->common.version = CAMERA_DEVICE_API_VERSION_3_5;
    g_cam_device3[mainCameraId]->common.module  = const_cast<hw_module_t *>(module);
    g_cam_device3[mainCameraId]->common.close   = HAL3_camera_device_close;
    g_cam_device3[mainCameraId]->ops            = &camera_device3_ops;

    ALOGV("DEBUG(%s[%d]):open camera(%d)", __FUNCTION__, __LINE__, mainCameraId);
    g_cam_device3[mainCameraId]->priv = new ExynosCamera(mainCameraId, subCameraId, scenario, &g_cam_info[camInfoIndex]);
    *device = (hw_device_t *)g_cam_device3[mainCameraId];

    ALOGI("INFO(%s[%d]):camera(%d) out from new g_cam_device3[%d]->priv()",
        __FUNCTION__, __LINE__, mainCameraId, mainCameraId);

    g_cam_openLock[mainCameraId].unlock();

done:
    cam_stateLock[mainCameraId].lock();
    cam_state[mainCameraId] = state;
    cam_stateLock[mainCameraId].unlock();

#ifdef USE_DUAL_CAMERA
    if (numOfSensors >= 2 && subCameraId != -1) {
        camera3_device_t *cam_device = (camera3_device_t *)(*device);
        obj(cam_device)->setDualMode(true);

        cam_stateLock[subCameraId].lock();
        cam_state[subCameraId] = state;
        cam_stateLock[subCameraId].unlock();
    }
#endif

    if (g_cam_info[camInfoIndex]) {
        metadata = g_cam_info[camInfoIndex];
        flashAvailable = metadata.find(ANDROID_FLASH_INFO_AVAILABLE);

        ALOGV("INFO(%s[%d]): cameraId(%d), flashAvailable.count(%zu), flashAvailable.data.u8[0](%d)",
            __FUNCTION__, __LINE__, mainCameraId, flashAvailable.count, flashAvailable.data.u8[0]);

        if (flashAvailable.count == 1 && flashAvailable.data.u8[0] == 1) {
            hasFlash = true;
        } else {
            hasFlash = false;
        }
    }

    /* Turn off torch and update torch status */
    if(hasFlash && g_cam_torchEnabled[mainCameraId]) {
        if (mainCameraId == CAMERA_ID_BACK || mainCameraId == CAMERA_ID_BACK_1) {
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

            g_cam_torchEnabled[mainCameraId] = false;
        }
    }

#ifdef USE_CAMERA_MEMBOOST
    fp = fopen(MEMBOOST_FILE_PATH, "w+");
    if (fp == NULL) {
        ALOGE("ERR(%s[%d]):MEMBOOST_FILE_PATH open fail", __FUNCTION__, __LINE__);
    } else {
        fwrite("2", sizeof(char), 1, fp);
        fflush(fp);
        fclose(fp);
        ALOGI("INFO(%s[%d]):MEMBOOST start", __FUNCTION__, __LINE__);
    }
#endif

    g_callbacks->torch_mode_status_change(g_callbacks, id, TORCH_MODE_STATUS_NOT_AVAILABLE);

    ALOGI("INFO(%s[%d]):camera(%d) out =====", __FUNCTION__, __LINE__, mainCameraId);

    TIME_LOGGER_UPDATE(mainCameraId, 0, 0, CUMULATIVE_CNT, OPEN_END, 0);

    return 0;
}

static int HAL3_camera_device_close(struct hw_device_t* device)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    int mainCameraId = -1;
#ifdef USE_DUAL_CAMERA
    int subCameraId = -1;
#endif
    int ret = OK;
    enum CAMERA_STATE state;
    char camid[10];

    ALOGI("INFO(%s[%d]):in =====", __FUNCTION__, __LINE__);

    if (device) {
        camera3_device_t *cam_device = (camera3_device_t *)device;
        mainCameraId = obj(cam_device)->getCameraId();

        ALOGV("DEBUG(%s[%d]):close camera(%d)", __FUNCTION__, __LINE__, mainCameraId);

#ifdef TIME_LOGGER_CLOSE_ENABLE
        TIME_LOGGER_INIT(mainCameraId);
#endif
        TIME_LOGGER_UPDATE(mainCameraId, 0, 0, CUMULATIVE_CNT, CLOSE_START, 0);

        ret = obj(cam_device)->releaseDevice();
        if (ret) {
            ALOGE("ERR(%s[%d]):initialize error!!", __FUNCTION__, __LINE__);
            ret = BAD_VALUE;
        }

        state = CAMERA_CLOSED;
        if (check_camera_state(state, mainCameraId) == false) {
            ALOGE("ERR(%s[%d]):camera(%d) state(%d) is INVALID",
                __FUNCTION__, __LINE__, mainCameraId, state);
            return -1;
        }

        g_cam_openLock[mainCameraId].lock();
        ALOGV("INFO(%s[%d]):camera(%d) open locked..", __FUNCTION__, __LINE__, mainCameraId);
        g_cam_device3[mainCameraId] = NULL;
        g_cam_openLock[mainCameraId].unlock();
        ALOGV("INFO(%s[%d]):camera(%d) open unlocked..", __FUNCTION__, __LINE__, mainCameraId);

#ifdef USE_DUAL_CAMERA
        subCameraId = obj(cam_device)->getSubCameraId();

        if (obj(cam_device)->getDualMode() == true && (subCameraId >= 0)) {
            cam_stateLock[subCameraId].lock();
            cam_state[subCameraId] = state;
            cam_stateLock[subCameraId].unlock();
        }
#endif

        delete static_cast<ExynosCamera *>(cam_device->priv);
        free(cam_device);

        cam_stateLock[mainCameraId].lock();
        cam_state[mainCameraId] = state;
        cam_stateLock[mainCameraId].unlock();
        ALOGI("INFO(%s[%d]):close camera(%d)", __FUNCTION__, __LINE__, mainCameraId);

        /* Update torch status */
        g_cam_torchEnabled[mainCameraId] = false;
        snprintf(camid, sizeof(camid), "%d\n", mainCameraId);
        g_callbacks->torch_mode_status_change(g_callbacks, camid, TORCH_MODE_STATUS_AVAILABLE_OFF);

#ifdef TIME_LOGGER_CLOSE_ENABLE
        TIME_LOGGER_UPDATE(mainCameraId, 0, 0, CUMULATIVE_CNT, CLOSE_END, 0);
        TIME_LOGGER_SAVE(mainCameraId);
#endif
    }
#ifdef USE_INTERNAL_ALLOC_DEBUG
    alloc_info_print();
#endif
    ALOGI("INFO(%s[%d]):out =====", __FUNCTION__, __LINE__);
    return ret;
}

static int HAL3_camera_device_initialize(const struct camera3_device *dev,
                                        const camera3_callback_ops_t *callback_ops)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    int ret = OK;
    int mainCameraId = obj(dev)->getCameraId();
    ALOGI("INFO(%s[%d]):in =====", __FUNCTION__, __LINE__);

    TIME_LOGGER_UPDATE(mainCameraId, 0, 0, CUMULATIVE_CNT, INITIALIZE_START, 0);

    g_cam_configLock[mainCameraId].lock();

    ALOGE("INFO(%s[%d]): dual cam_state[0](%d)", __FUNCTION__, __LINE__, cam_state[0]);

#ifdef PIP_CAMERA_SUPPORTED
    if (mainCameraId != 0 && g_cam_device3[0] != NULL
        && cam_state[0] != CAMERA_NONE && cam_state[0] != CAMERA_CLOSED) {
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
                __FUNCTION__, __LINE__, mainCameraId, ret);
        } else {
            ALOGI("INFO(%s[%d]):camera(%d) set pip mode",
                __FUNCTION__, __LINE__, mainCameraId);
        }
    }
#endif

    ret = obj(dev)->initializeDevice(callback_ops);
    if (ret) {
        ALOGE("ERR(%s[%d]):initialize error!!", __FUNCTION__, __LINE__);
        ret = NO_INIT;
    }
    g_cam_configLock[mainCameraId].unlock();

    ALOGV("DEBUG(%s):set callback ops - %p", __FUNCTION__, callback_ops);
    ALOGI("INFO(%s[%d]):out =====", __FUNCTION__, __LINE__);

    TIME_LOGGER_UPDATE(mainCameraId, 0, 0, CUMULATIVE_CNT, INITIALIZE_END, 0);

    return ret;
}

static int HAL3_camera_device_configure_streams(const struct camera3_device *dev,
                                                camera3_stream_configuration_t *stream_list)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    int ret = OK;
    int mainCameraId = obj(dev)->getCameraId();
    ALOGI("INFO(%s[%d]):in =====", __FUNCTION__, __LINE__);

    TIME_LOGGER_UPDATE(mainCameraId, 0, 0, CUMULATIVE_CNT, CONFIGURE_STREAM_START, 0);

    g_cam_configLock[mainCameraId].lock();
    ret = obj(dev)->configureStreams(stream_list);
    if (ret) {
        ALOGE("ERR(%s[%d]):configure_streams error!!", __FUNCTION__, __LINE__);
        /* if NO_MEMORY is returned, cameraserver makes assert intentionally to recover iris buffer leak */
        if (mainCameraId == CAMERA_ID_SECURE && ret == NO_MEMORY) {
            ret = NO_MEMORY;
        } else if (ret != NO_INIT) { /* NO_INIT = -ENODEV */
            ret = BAD_VALUE;
        }
    }
    g_cam_configLock[mainCameraId].unlock();
    ALOGI("INFO(%s[%d]):out =====", __FUNCTION__, __LINE__);

    TIME_LOGGER_UPDATE(mainCameraId, 0, 0, CUMULATIVE_CNT, CONFIGURE_STREAM_END, 0);

    return ret;
}

static const camera_metadata_t* HAL3_camera_device_construct_default_request_settings(
                                                                const struct camera3_device *dev,
                                                                int type)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    camera_metadata_t *request = NULL;
    status_t res;
    int mainCameraId = obj(dev)->getCameraId();
    ALOGI("INFO(%s[%d]):in =====", __FUNCTION__, __LINE__);
    g_cam_configLock[mainCameraId].lock();
    res = obj(dev)->construct_default_request_settings(&request, type);
    if (res) {
        ALOGE("ERR(%s[%d]):constructDefaultRequestSettings error!!", __FUNCTION__, __LINE__);
        g_cam_configLock[mainCameraId].unlock();
        return NULL;
    }
    g_cam_configLock[mainCameraId].unlock();
    ALOGI("INFO(%s[%d]):out =====", __FUNCTION__, __LINE__);
    return request;
}

static int HAL3_camera_device_process_capture_request(const struct camera3_device *dev,
                                                        camera3_capture_request_t *request)
{
    /* ExynosCameraAutoTimer autoTimer(__FUNCTION__); */

    int ret = OK;
    __unused int mainCameraId = obj(dev)->getCameraId();

    ALOGV("INFO(%s[%d]):in =====", __FUNCTION__, __LINE__);

    TIME_LOGGER_UPDATE(mainCameraId, 0, 0, CUMULATIVE_CNT, PROCESS_CAPTURE_REQUEST_START, 0);
#ifdef TIME_LOGGER_LAUNCH_ENABLE
    if (request->frame_number == 0) {
        TIME_LOGGER_SAVE(mainCameraId);
    }
#endif

    ret = obj(dev)->processCaptureRequest(request);
    if (ret) {
        ALOGE("ERR(%s[%d]):process_capture_request error(%d)!!", __FUNCTION__, __LINE__, ret);
        if (ret != NO_INIT) /* NO_INIT = -ENODEV */
            ret = BAD_VALUE;
    }
    ALOGV("INFO(%s[%d]):out =====", __FUNCTION__, __LINE__);
    return ret;
}

static int HAL3_camera_device_flush(const struct camera3_device *dev)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    int ret = 0;
    int mainCameraId = obj(dev)->getCameraId();

    ALOGI("INFO(%s[%d]):in =====", __FUNCTION__, __LINE__);
    g_cam_configLock[mainCameraId].lock();
    ret = obj(dev)->flush();
    if (ret) {
        ALOGE("ERR(%s[%d]):flush error(%d)!!", __FUNCTION__, __LINE__, ret);
        if (ret != NO_INIT) /* NO_INIT = -ENODEV */
            ret = BAD_VALUE;
    }
    g_cam_configLock[mainCameraId].unlock();

    ALOGI("INFO(%s[%d]):out =====", __FUNCTION__, __LINE__);
    return ret;
}

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
    int i = 0;
    int getNumOfCamera = 0;
    int mainCamId = -1;
    int subCamId = -1;
    int numOfSensor = -1;
    int scenario = -1;
    int camInfoIndex = -1;
    int size = ExynosCameraMetadataConverter::getExynosCameraDeviceInfoSize();

    for (i = 0; i < size; i++) {
        if (g_HAL_CameraInfo[i] == NULL)
            g_HAL_CameraInfo[i] = ExynosCameraMetadataConverter::getExynosCameraDeviceInfoByCamIndex(i);

        int serviceCamId = g_HAL_CameraInfo[i]->cameraId;
        if (serviceCamId < CAMERA_SERVICE_ID_OPEN_CAMERA_MAX) {
            numOfSensor = HAL_getCameraId(serviceCamId, &mainCamId, &subCamId, &scenario, &camInfoIndex);
            if (numOfSensor > 0 && mainCamId != -1) {
                getNumOfCamera++;
            }
        }
    }
    ALOGV("DEBUG(%s[%d]):Number of cameras(%d)", __FUNCTION__, __LINE__, getNumOfCamera);

    return getNumOfCamera;
}

static int HAL_getCameraInfo(int serviceCameraId, struct camera_info *info)
{
    /* ExynosCameraAutoTimer autoTimer(__FUNCTION__); */
    status_t ret = NO_ERROR;
    int numOfSensors = 1;
    int mainCameraId = -1;
    int subCameraId = -1;
    int scenario = -1;
    int camInfoIndex = -1;

    numOfSensors = HAL_getCameraId(serviceCameraId, &mainCameraId, &subCameraId, &scenario, &camInfoIndex);

    ALOGI("INFO(%s[%d]):in =====", __FUNCTION__, __LINE__);

    /* set facing and orientation */
    /*  set service arbitration (resource_cost, conflicting_devices, conflicting_devices_length) */
    if (numOfSensors < 1 || mainCameraId < 0 || camInfoIndex < 0) {
            ALOGE("ERR(%s[%d]):Invalid camera ID %d/%d camInfoIndex(%d)",
                __FUNCTION__, __LINE__, serviceCameraId, mainCameraId, camInfoIndex);
            return NO_INIT; /* NO_INIT = -ENODEV */
    } 

    /* set device API version */
    info->device_version = CAMERA_DEVICE_API_VERSION_3_5;

    /* set camera_metadata_t if needed */
    if (info->device_version >= HARDWARE_DEVICE_API_VERSION(2, 0)) {
        if (g_cam_info[camInfoIndex] == NULL) {
            ALOGV("DEBUG(%s[%d]):Return static information (%d)", __FUNCTION__, __LINE__, mainCameraId);
            ret = ExynosCameraMetadataConverter::constructStaticInfo(serviceCameraId, mainCameraId, scenario, 
                                                                     &g_cam_info[camInfoIndex], &g_HAL_CameraInfo[camInfoIndex]);
            if (ret != 0) {
                ALOGE("ERR(%s[%d]): static information is NULL", __FUNCTION__, __LINE__);
                return BAD_VALUE;
            }
            info->static_camera_characteristics = g_cam_info[camInfoIndex];
            
            if (g_HAL_CameraInfo[camInfoIndex] != NULL) {
                info->facing = g_HAL_CameraInfo[camInfoIndex]->facing_info;
                info->orientation = g_HAL_CameraInfo[camInfoIndex]->orientation;
                info->resource_cost = g_HAL_CameraInfo[camInfoIndex]->resource_cost;
                info->conflicting_devices = g_HAL_CameraInfo[camInfoIndex]->conflicting_devices;
                info->conflicting_devices_length = g_HAL_CameraInfo[camInfoIndex]->conflicting_devices_length;
            } else {
                ALOGE("ERR(%s[%d]): HAL_CameraInfo[%d] is NULL!", __FUNCTION__, __LINE__, camInfoIndex);
                return BAD_VALUE;
            }
        } else {
            ALOGV("DEBUG(%s[%d]):Reuse Return static information ( serviceCameraId (%d) mainCameraId (%d) camInfoIndex(%d) )",
                    __FUNCTION__, __LINE__, serviceCameraId, mainCameraId, camInfoIndex);
            info->static_camera_characteristics = g_cam_info[camInfoIndex];
            info->facing = g_HAL_CameraInfo[camInfoIndex]->facing_info;
            info->orientation = g_HAL_CameraInfo[camInfoIndex]->orientation;
            info->resource_cost = g_HAL_CameraInfo[camInfoIndex]->resource_cost;
            info->conflicting_devices = g_HAL_CameraInfo[camInfoIndex]->conflicting_devices;
            info->conflicting_devices_length = g_HAL_CameraInfo[camInfoIndex]->conflicting_devices_length;
        }
    }

    ALOGD("DEBUG(%s info->resource_cost = %d ", __FUNCTION__, info->resource_cost);

    if (info->conflicting_devices_length) {
        for (size_t i = 0; i < info->conflicting_devices_length; i++) {
            ALOGD("DEBUG(%s info->conflicting_devices = %s ", __FUNCTION__, info->conflicting_devices[i]);
        }
    } else {
        ALOGV("DEBUG(%s info->conflicting_devices_length is zero ", __FUNCTION__);
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

static int HAL_set_torch_mode(const char* serviceCameraId, bool enabled)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    int cameraId = atoi(serviceCameraId);
    FILE *fp = NULL;
    CameraMetadata metadata;
    camera_metadata_entry flashAvailable;
    int ret = 0;
    int numOfSensors = 1;
    int mainCameraId = -1;
    int subCameraId = -1;
    int scenario = -1;
    int camInfoIndex = -1;
    char flashFilePath[100] = {'\0',};

    numOfSensors = HAL_getCameraId(cameraId, &mainCameraId, &subCameraId, &scenario, &camInfoIndex);

    ALOGI("INFO(%s[%d]):in =====", __FUNCTION__, __LINE__);
    if (numOfSensors < 1 || mainCameraId < 0 || camInfoIndex < 0) {
        ALOGE("ERR(%s[%d]):Invalid camera ID%d and numOf Sensors(%d)",
            __FUNCTION__, __LINE__, mainCameraId, numOfSensors);
        return -EINVAL;
    }

    /* Check the android.flash.info.available */
    /* If this camera device does not support flash, It have to return -ENOSYS */
    metadata = g_cam_info[camInfoIndex];
    flashAvailable = metadata.find(ANDROID_FLASH_INFO_AVAILABLE);

    if (flashAvailable.count == 1 && flashAvailable.data.u8[0] == 1) {
        ALOGV("DEBUG(%s[%d]): Flash metadata exist", __FUNCTION__, __LINE__);
    } else {
        ALOGE("ERR(%s[%d]): Can not find flash metadata", __FUNCTION__, __LINE__);
        return -ENOSYS;
    }

    ALOGI("INFO(%s[%d]): Current Camera State (state = %d)", __FUNCTION__, __LINE__, cam_state[mainCameraId]);

    /* Add the check the camera state that camera in use or not */
    if (cam_state[mainCameraId] > CAMERA_CLOSED) {
        ALOGE("ERR(%s[%d]): Camera Device is busy (state = %d)", __FUNCTION__, __LINE__, cam_state[mainCameraId]);
        g_callbacks->torch_mode_status_change(g_callbacks, serviceCameraId, TORCH_MODE_STATUS_AVAILABLE_OFF);
        return -EBUSY;
    }

    /* Add the sysfs file read (sys/class/camera/flash/torch_flash) then set 0 or 1 */
    if (mainCameraId == CAMERA_ID_BACK || mainCameraId == CAMERA_ID_BACK_1) {
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
        g_cam_torchEnabled[mainCameraId] = true;
        g_callbacks->torch_mode_status_change(g_callbacks,
            serviceCameraId, TORCH_MODE_STATUS_AVAILABLE_ON);
    } else {
        g_cam_torchEnabled[mainCameraId] = false;
        g_callbacks->torch_mode_status_change(g_callbacks,
            serviceCameraId, TORCH_MODE_STATUS_AVAILABLE_OFF);
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
