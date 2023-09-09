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

#ifndef EXYNOS_CAMERA3_SERVICE_INTERFACE_H
#define EXYNOS_CAMERA3_SERVICE_INTERFACE_H

#include <utils/RefBase.h>

#include "hardware/camera3.h"
#include "system/camera_metadata.h"

#include "ExynosCamera.h"
#include "ExynosCameraInterfaceState.h"
#include "pthread.h"

#define SET_METHOD3(m) .m = HAL3_camera_device_##m
/* flashlight control */
/* init camera module */
#define INIT_MODULE_PATH "/sys/class/camera/rear/fw_update"

#ifdef USE_CAMERA_MEMBOOST
#define MEMBOOST_FILE_PATH "/sys/kernel/mm/vmscan/mem_boost_mode"
#endif

namespace android {

typedef enum {
#if defined(CAMERA_OPEN_ID_REAR_0)
    CAMERA_INDEX_REAR_0,
#endif
#if defined(CAMERA_OPEN_ID_FRONT_1)
    CAMERA_INDEX_FRONT_1,
#endif
#if defined(CAMERA_OPEN_ID_REAR_2)
    CAMERA_INDEX_REAR_2,
#endif
    CAMERA_INDEX_DUAL_REAR_ZOOM,
    CAMERA_INDEX_DUAL_REAR_PORTRAIT,
    CAMERA_INDEX_DUAL_FRONT_PORTRAIT,
    CAMERA_INDEX_HIDDEN_REAR_2,
    CAMERA_INDEX_HIDDEN_FRONT_2,
    CAMERA_INDEX_IRIS_SECURE,
    CAMERA_INDEX_ID_MAX,
} camera_index_t;

static HAL_CameraInfo_t *g_HAL_CameraInfo[MAX_NUM_OF_CAMERA];
static camera3_device_t *g_cam_device3[MAX_NUM_OF_CAMERA];
static camera_metadata_t *g_cam_info[CAMERA_INDEX_ID_MAX] = {NULL, NULL};
static const camera_module_callbacks_t *g_callbacks = NULL;

//ExynosCameraRequestManager *m_exynosCameraRequestManager[MAX_NUM_OF_CAMERA];
//const   camera3_callback_ops_t *callbackOps;

static Mutex            g_cam_openLock[MAX_NUM_OF_CAMERA];
static Mutex            g_cam_configLock[MAX_NUM_OF_CAMERA];
static bool             g_cam_torchEnabled[MAX_NUM_OF_CAMERA] = {false, false};
pthread_t		g_thread;

static inline ExynosCamera *obj(const struct camera3_device *dev)
{
    return reinterpret_cast<ExynosCamera *>(dev->priv);
};

static int HAL_getCameraId(int serviceCamId, int *mainCamId, int *subCamId, int *scenario, int *camInfoIndex);

/**
 * Open camera device
 */
static int HAL3_camera_device_open(const struct hw_module_t* module,
                                    const char *id,
                                    struct hw_device_t** device);

/**
 * Close camera device
 */
static int HAL3_camera_device_close(struct hw_device_t* device);

/**
 * initialize
 * One-time initialization to pass framework callback function pointers to the HAL.
 */
static int HAL3_camera_device_initialize(const struct camera3_device *dev,
                                        const camera3_callback_ops_t *callback_ops);

/**
 * configure_streams
 * Reset the HAL camera device processing pipeline and set up new input and output streams.
 */
static int HAL3_camera_device_configure_streams(const struct camera3_device *dev,
                                                camera3_stream_configuration_t *stream_list);

/**
 * construct_default_request_settings
 * Create capture settings for standard camera use cases.
 */
static const camera_metadata_t* HAL3_camera_device_construct_default_request_settings(
                                                                const struct camera3_device *dev,
                                                                int type);

/**
 * process_capture_request
 * Send a new capture request to the HAL.
 */
static int HAL3_camera_device_process_capture_request(const struct camera3_device *dev,
                                                        camera3_capture_request_t *request);

/**
 * flush
 * Flush all currently in-process captures and all buffers in the pipeline on the given device.
 */
static int HAL3_camera_device_flush(const struct camera3_device *dev);

/**
 * get_metadata_vendor_tag_ops
 * Get methods to query for vendor extension metadata tag information.
 */
static void HAL3_camera_device_get_metadata_vendor_tag_ops(const struct camera3_device *dev,
                                                            vendor_tag_query_ops_t* ops);

/**
 * dump
 * Print out debugging state for the camera device.
 */
static void HAL3_camera_device_dump(const struct camera3_device *dev, int fd);

/**
 * Retrun the camera hardware info
 */
static int HAL_getCameraInfo(int camera_id, struct camera_info *info);

/**
 * Provide callback function pointers to the HAL module to inform framework
 * of asynchronous camera module events.
 */
static int HAL_set_callbacks(const camera_module_callbacks_t *callbacks);

/**
 * Return number of the camera hardware
 */
static int HAL_getNumberOfCameras();
#ifdef SAMSUNG_TN_FEATURE
static int HAL_getNumberOfHiddenCameras();
#endif

static void HAL_get_vendor_tag_ops(vendor_tag_ops_t* ops);

static int HAL_set_torch_mode(const char* camera_id, bool enabled);

static int HAL_init(void);

static camera3_device_ops_t camera_device3_ops = {
        SET_METHOD3(initialize),
        SET_METHOD3(configure_streams),
        NULL,
        SET_METHOD3(construct_default_request_settings),
        SET_METHOD3(process_capture_request),
        SET_METHOD3(get_metadata_vendor_tag_ops),
        SET_METHOD3(dump),
        SET_METHOD3(flush),
        {0} /* reserved for future use */
};

static hw_module_methods_t mCameraHwModuleMethods = {
            .open = HAL3_camera_device_open
};

/*
 * Required HAL header.
 */
extern "C" {
    camera_module_t HAL_MODULE_INFO_SYM = {
        .common = {
            .tag                = HARDWARE_MODULE_TAG,
            .module_api_version = CAMERA_MODULE_API_VERSION_2_4,
            .hal_api_version    = HARDWARE_HAL_API_VERSION,
            .id                 = CAMERA_HARDWARE_MODULE_ID,
            .name               = "Exynos Camera HAL3",
            .author             = "Samsung Electronics Inc",
            .methods            = &mCameraHwModuleMethods,
            .dso                = NULL,
            .reserved           = {0},
        },
        .get_number_of_cameras = HAL_getNumberOfCameras,
        .get_camera_info       = HAL_getCameraInfo,
        .set_callbacks         = HAL_set_callbacks,
        .get_vendor_tag_ops    = HAL_get_vendor_tag_ops,
        .open_legacy           = NULL,
        .set_torch_mode        = HAL_set_torch_mode,
        .init                  = HAL_init,
        .reserved              = {0},
    };
}

}; // namespace android
#endif
