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

#ifndef EXYNOS_CAMERA_DUAL_SERVICE_INTERFACE_H
#define EXYNOS_CAMERA_DUAL_SERVICE_INTERFACE_H

#include <utils/RefBase.h>

#include "ExynosCamera.h"
#include "ExynosCameraInterfaceState.h"
#include "ExynosCameraCallbackHooker.h"

#if defined(BOARD_BACK_CAMERA_USES_EXTERNAL_CAMERA) || defined(BOARD_FRONT_CAMERA_USES_EXTERNAL_CAMERA)
#include "SecCameraInterface.h"
#endif

#ifndef CAMERA_MODULE_VERSION
#define CAMERA_MODULE_VERSION   CAMERA_MODULE_API_VERSION_1_0
#endif

#define SET_METHOD(m) m : HAL_camera_device_dual_##m

#define EXYNOS_CAMERA_DUAL_CAMERA_IF_DEBUG

#ifdef EXYNOS_CAMERA_DUAL_CAMERA_IF_DEBUG
#define EXYNOS_CAMERA_DUAL_CAMERA_IF_DEBUG_LOG ALOGI
#else
#define EXYNOS_CAMERA_DUAL_CAMERA_IF_DEBUG_LOG ALOGV
#endif

/* We want service not to know actual num of cameras. */
#define EXYNOS_CAMERA_INTERNAL_NUM 5

namespace android {

static int sCameraInfo[][2] = {
    {
        CAMERA_FACING_BACK,
        BACK_ROTATION  /* orientation */
    },
    {
        CAMERA_FACING_FRONT,
        FRONT_ROTATION  /* orientation */
    },
};

static camera_device_t *g_cam_device[MAX_NUM_OF_CAMERA];

static Mutex            g_cam_openLock[MAX_NUM_OF_CAMERA];
static Mutex            g_cam_previewLock[MAX_NUM_OF_CAMERA];
static Mutex            g_cam_recordingLock[MAX_NUM_OF_CAMERA];
static bool             g_flagPipMode;

static inline ExynosCamera *obj(struct camera_device *dev)
{
    return reinterpret_cast<ExynosCamera *>(dev->priv);
}

static inline int getOpenedCameraHalCnt(void)
{
    int cnt = 0;

    for (int i = 0; i < MAX_NUM_OF_CAMERA; i++) {
       if (g_cam_device[i] != NULL)
        cnt++;
    }

    return cnt;
}

static inline void setPipMode(bool flagPipMode)
{
    g_flagPipMode = flagPipMode;
}

static inline void setAutoDualMode(bool flagAutoDualMode)
{
    g_flagAutoDualMode = flagAutoDualMode;
}

static inline bool getAutoDualMode(void)
{
    return g_flagAutoDualMode;
}

/* this API for checking dual Camera supported by BoardConfig.mk */
static inline bool isDualCameraSupported(int cameraId)
{
    bool ret = false;

    int mainCameraId = -1;
    int subCameraId = -1;

    /*
     * if dualCamera subId is alive && this camera id is dual Camera's main camera,
     * then, it is dual Camera scenario
     */
    getDualCameraId(&mainCameraId, &subCameraId);

    if (0 < subCameraId) {
        if (cameraId == mainCameraId) {
            ret = true;
        }
    }

    return ret;
}

/* this API for checking dual Camera can turn on this scenario */
static inline bool isDualCamera(int cameraId)
{
    bool ret = false;

    if (g_flagAutoDualMode == false)
        return false;

    ret = isDualCameraSupported(cameraId);

    if (g_flagPipMode == true)
        ret = false;

    return ret;
};

static inline int getTeleCameraId(int cameraId)
{
    int mainCameraId = -1;
    int subCameraId = -1;

    /*
     * if dualCamera subId is alive && this camera id is dual Camera's main camera,
     * then, it is dual Camera scenario
     */
    getDualCameraId(&mainCameraId, &subCameraId);

    if (subCameraId < 0) {
        android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):getDualCameraId(%d, %d) return value is weird. (is it dualCamera scenario?), assert!!!!",
            __FUNCTION__, __LINE__, mainCameraId, subCameraId);
    }

    if (cameraId != mainCameraId) {
        android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):cameraId(%d) != mainCameraId(%d). (is it dualCamera scenario?), assert!!!!",
            __FUNCTION__, __LINE__, cameraId, mainCameraId);
    }

    return subCameraId;
}

/**
 * Open camera device
 */
static int HAL_camera_device_open(
        const struct hw_module_t* module,
        const char *id,
        struct hw_device_t** device);

/**
 * Close camera device
 */
static int HAL_camera_device_close(struct hw_device_t* device);

/**
 * Set the preview_stream_ops to which preview frames are sent
 */
static int HAL_camera_device_set_preview_window(
        struct camera_device *dev,
        struct preview_stream_ops *buf);

/**
 * Set the notification and data callbacks
 */
static void HAL_camera_device_set_callbacks(
        struct camera_device *dev,
        camera_notify_callback notify_cb,
        camera_data_callback data_cb,
        camera_data_timestamp_callback data_cb_timestamp,
        camera_request_memory get_memory,
        void* user);

/**
 * The following three functions all take a msg_type, which is a bitmask of
 * the messages defined in include/ui/Camera.h
 */

/**
 * Enable a message, or set of messages.
 */
static void HAL_camera_device_enable_msg_type(
        struct camera_device *dev,
        int32_t msg_type);

/**
 * Disable a message, or a set of messages.
 *
 * Once received a call to disableMsgType(CAMERA_MSG_VIDEO_FRAME), camera
 * HAL should not rely on its client to call releaseRecordingFrame() to
 * release video recording frames sent out by the cameral HAL before and
 * after the disableMsgType(CAMERA_MSG_VIDEO_FRAME) call. Camera HAL
 * clients must not modify/access any video recording frame after calling
 * disableMsgType(CAMERA_MSG_VIDEO_FRAME).
 */
static void HAL_camera_device_disable_msg_type(
        struct camera_device *dev,
        int32_t msg_type);

/**
 * Query whether a message, or a set of messages, is enabled.  Note that
 * this is operates as an AND, if any of the messages queried are off, this
 * will return false.
 */
static int HAL_camera_device_msg_type_enabled(
        struct camera_device *dev,
        int32_t msg_type);

/**
 * Start preview mode.
 */
static int HAL_camera_device_start_preview(struct camera_device *dev);

/**
 * Stop a previously started preview.
 */
static void HAL_camera_device_stop_preview(struct camera_device *dev);

/**
 * Returns true if preview is enabled.
 */
static int HAL_camera_device_preview_enabled(struct camera_device *dev);

/**
 * Request the camera HAL to store meta data or real YUV data in the video
 * buffers sent out via CAMERA_MSG_VIDEO_FRAME for a recording session. If
 * it is not called, the default camera HAL behavior is to store real YUV
 * data in the video buffers.
 *
 * This method should be called before startRecording() in order to be
 * effective.
 *
 * If meta data is stored in the video buffers, it is up to the receiver of
 * the video buffers to interpret the contents and to find the actual frame
 * data with the help of the meta data in the buffer. How this is done is
 * outside of the scope of this method.
 *
 * Some camera HALs may not support storing meta data in the video buffers,
 * but all camera HALs should support storing real YUV data in the video
 * buffers. If the camera HAL does not support storing the meta data in the
 * video buffers when it is requested to do do, INVALID_OPERATION must be
 * returned. It is very useful for the camera HAL to pass meta data rather
 * than the actual frame data directly to the video encoder, since the
 * amount of the uncompressed frame data can be very large if video size is
 * large.
 *
 * @param enable if true to instruct the camera HAL to store
 *      meta data in the video buffers; false to instruct
 *      the camera HAL to store real YUV data in the video
 *      buffers.
 *
 * @return OK on success.
 */
static int HAL_camera_device_store_meta_data_in_buffers(
        struct camera_device *dev,
        int enable);

/**
 * Start record mode. When a record image is available, a
 * CAMERA_MSG_VIDEO_FRAME message is sent with the corresponding
 * frame. Every record frame must be released by a camera HAL client via
 * releaseRecordingFrame() before the client calls
 * disableMsgType(CAMERA_MSG_VIDEO_FRAME). After the client calls
 * disableMsgType(CAMERA_MSG_VIDEO_FRAME), it is the camera HAL's
 * responsibility to manage the life-cycle of the video recording frames,
 * and the client must not modify/access any video recording frames.
 */
static int HAL_camera_device_start_recording(struct camera_device *dev);

/**
 * Stop a previously started recording.
 */
static void HAL_camera_device_stop_recording(struct camera_device *dev);

/**
 * Returns true if recording is enabled.
 */
static int HAL_camera_device_recording_enabled(struct camera_device *dev);

/**
 * Release a record frame previously returned by CAMERA_MSG_VIDEO_FRAME.
 *
 * It is camera HAL client's responsibility to release video recording
 * frames sent out by the camera HAL before the camera HAL receives a call
 * to disableMsgType(CAMERA_MSG_VIDEO_FRAME). After it receives the call to
 * disableMsgType(CAMERA_MSG_VIDEO_FRAME), it is the camera HAL's
 * responsibility to manage the life-cycle of the video recording frames.
 */
static void HAL_camera_device_release_recording_frame(
        struct camera_device *dev,
        const void *opaque);

/**
 * Start auto focus, the notification callback routine is called with
 * CAMERA_MSG_FOCUS once when focusing is complete. autoFocus() will be
 * called again if another auto focus is needed.
 */
static int HAL_camera_device_auto_focus(struct camera_device *dev);

/**
 * Cancels auto-focus function. If the auto-focus is still in progress,
 * this function will cancel it. Whether the auto-focus is in progress or
 * not, this function will return the focus position to the default.  If
 * the camera does not support auto-focus, this is a no-op.
 */
static int HAL_camera_device_cancel_auto_focus(struct camera_device *dev);

/**
 * Take a picture.
 */
static int HAL_camera_device_take_picture(struct camera_device *dev);

/**
 * Cancel a picture that was started with takePicture. Calling this method
 * when no picture is being taken is a no-op.
 */
static int HAL_camera_device_cancel_picture(struct camera_device *dev);

/**
 * Set the camera parameters. This returns BAD_VALUE if any parameter is
 * invalid or not supported.
 */
static int HAL_camera_device_set_parameters(
        struct camera_device *dev,
        const char *parms);

/** 
 * Return the camera parameters.
 */
char *HAL_camera_device_get_parameters(struct camera_device *dev);

/**
 * Release buffer that used by the camera parameters.
 */
static void HAL_camera_device_put_parameters(
        struct camera_device *dev,
        char *parms);

/**
 * Send command to camera driver.
 */
static int HAL_camera_device_send_command(
        struct camera_device *dev,
        int32_t cmd,
        int32_t arg1,
        int32_t arg2);

/**
 * Release the hardware resources owned by this object.  Note that this is
 * *not* done in the destructor.
 */
static void HAL_camera_device_release(struct camera_device *dev);

/**
 * Dump state of the camera hardware
 */
static int HAL_camera_device_dump(struct camera_device *dev, int fd);

/**
 * Callback functions for the camera HAL module to use to inform the framework
 * of changes to the camera subsystem. These are called only by HAL modules
 * implementing version CAMERA_MODULE_API_VERSION_2_1 or higher of the HAL
 * module API interface.
 */
static int HAL_set_callbacks(const camera_module_callbacks_t *callbacks);

/**
 * Retrun the camera hardware info
 */
static int HAL_getCameraInfo(int cameraId, struct camera_info *info);

/**
 * Return number of the camera hardware
 */
static int HAL_getNumberOfCameras();

static int HAL_open_legacy(const struct hw_module_t* module, const char* id, uint32_t halVersion, struct hw_device_t** device);

static void HAL_get_vendor_tag_ops(vendor_tag_ops_t* ops);

/*
 * from here
 * Dual Camera Interface
 */
static int HAL_camera_device_dual_open(
        const struct hw_module_t* module,
        const char *id,
        struct hw_device_t** device);

static int HAL_camera_device_dual_close(struct hw_device_t* device);

static int HAL_camera_device_dual_set_preview_window(
        struct camera_device *dev,
        struct preview_stream_ops *buf);

static void HAL_camera_device_dual_set_callbacks(
        struct camera_device *dev,
        camera_notify_callback notify_cb,
        camera_data_callback data_cb,
        camera_data_timestamp_callback data_cb_timestamp,
        camera_request_memory get_memory,
        void* user);

static void HAL_camera_device_dual_enable_msg_type(
        struct camera_device *dev,
        int32_t msg_type);

static void HAL_camera_device_dual_disable_msg_type(
        struct camera_device *dev,
        int32_t msg_type);

static int HAL_camera_device_dual_msg_type_enabled(
        struct camera_device *dev,
        int32_t msg_type);

static int HAL_camera_device_dual_start_preview(struct camera_device *dev);

static void HAL_camera_device_dual_stop_preview(struct camera_device *dev);

static int HAL_camera_device_dual_preview_enabled(struct camera_device *dev);

static int HAL_camera_device_dual_store_meta_data_in_buffers(
        struct camera_device *dev,
        int enable);

static int HAL_camera_device_dual_start_recording(struct camera_device *dev);

static void HAL_camera_device_dual_stop_recording(struct camera_device *dev);

static int HAL_camera_device_dual_recording_enabled(struct camera_device *dev);

static void HAL_camera_device_dual_release_recording_frame(
        struct camera_device *dev,
        const void *opaque);

static int HAL_camera_device_dual_auto_focus(struct camera_device *dev);

static int HAL_camera_device_dual_cancel_auto_focus(struct camera_device *dev);

static int HAL_camera_device_dual_take_picture(struct camera_device *dev);

static int HAL_camera_device_dual_cancel_picture(struct camera_device *dev);

static int HAL_camera_device_dual_set_parameters(
        struct camera_device *dev,
        const char *parms);

char *HAL_camera_device_dual_get_parameters(struct camera_device *dev);

static void HAL_camera_device_dual_put_parameters(
        struct camera_device *dev,
        char *parms);

static int HAL_camera_device_dual_send_command(
        struct camera_device *dev,
        int32_t cmd,
        int32_t arg1,
        int32_t arg2);

static void HAL_camera_device_dual_release(struct camera_device *dev);

static int HAL_camera_device_dual_dump(struct camera_device *dev, int fd);

static camera_device_ops_t camera_device_ops = {
        SET_METHOD(set_preview_window),
        SET_METHOD(set_callbacks),
        SET_METHOD(enable_msg_type),
        SET_METHOD(disable_msg_type),
        SET_METHOD(msg_type_enabled),
        SET_METHOD(start_preview),
        SET_METHOD(stop_preview),
        SET_METHOD(preview_enabled),
        SET_METHOD(store_meta_data_in_buffers),
        SET_METHOD(start_recording),
        SET_METHOD(stop_recording),
        SET_METHOD(recording_enabled),
        SET_METHOD(release_recording_frame),
        SET_METHOD(auto_focus),
        SET_METHOD(cancel_auto_focus),
        SET_METHOD(take_picture),
        SET_METHOD(cancel_picture),
        SET_METHOD(set_parameters),
        SET_METHOD(get_parameters),
        SET_METHOD(put_parameters),
        SET_METHOD(send_command),
        SET_METHOD(release),
        SET_METHOD(dump),
};

static hw_module_methods_t camera_module_methods = {
            open : HAL_camera_device_dual_open
};

extern "C" {
    struct camera_module HAL_MODULE_INFO_SYM = {
      common : {
          tag                : HARDWARE_MODULE_TAG,
          module_api_version : CAMERA_MODULE_VERSION,
          hal_api_version    : HARDWARE_HAL_API_VERSION,
          id                 : CAMERA_HARDWARE_MODULE_ID,
          name               : "Exynos Camera HAL1",
          author             : "Samsung Corporation",
          methods            : &camera_module_methods,
          dso                : NULL,
          reserved           : {0},
      },
      get_number_of_cameras : HAL_getNumberOfCameras,
      get_camera_info       : HAL_getCameraInfo,
      set_callbacks         : HAL_set_callbacks,
#if (TARGET_ANDROID_VER_MAJ >= 4 && TARGET_ANDROID_VER_MIN >= 4)
      get_vendor_tag_ops    : HAL_get_vendor_tag_ops,
      open_legacy           : HAL_open_legacy,
      set_torch_mode        : NULL,
      init                  : NULL,
      reserved              : {0}
#endif
    };
}


#define USE_THREAD_ON_DUAL_CAMERA_INTERFACE

#ifdef USE_THREAD_ON_DUAL_CAMERA_INTERFACE
//#define DEBUG_USE_THREAD_ON_DUAL_CAMERA

#define USE_THREAD_ON_DUAL_CAMERA_OPEN
#define USE_THREAD_ON_DUAL_CAMERA_STOP_PREVIEW
#define USE_THREAD_ON_DUAL_CAMERA_SET_PARAMETERS
#define USE_THREAD_ON_DUAL_CAMERA_TAKE_PICTURE
#define USE_THREAD_ON_DUAL_CAMERA_RELEASE
#define USE_THREAD_ON_DUAL_CAMERA_CLOSE
#endif

#define EXYNOS_CAMERA_THREAD_API_Q_SIZE (32)

class ExynosCameraThreadInterface {
public:
    enum RUM_MODE {
        RUN_MODE_BASE = 0,
        RUN_MODE_IMMEDIATE,     // run thread immediately, when api call.
        RUN_MODE_BATCH,         // save on m_apiQ, when startPreview() is called, run apies in batch
        RUN_MODE_NO_THREAD,     // run api w/o thread
        RUN_MODE_MAX,
    };

public:
    ExynosCameraThreadInterface();

    ExynosCameraThreadInterface(const struct hw_module_t* module, int cameraId);
    virtual ~ExynosCameraThreadInterface();

    void     setRunMode(enum RUM_MODE runMode);
    ExynosCameraThreadInterface::RUM_MODE getRunMode(void);

    status_t open(void);
    status_t setParameters(const char *params);
    status_t startPreview(void);
    status_t stopPreview(void);
    status_t takePicture(void);
    status_t release(void);
    status_t close(void);
    status_t checkApiDone(void);

public:
    /* this is special public.(because of HAL_camera_device_open) */
    struct hw_device_t            *m_hw_device;

private:
    enum API_TYPE {
        API_TYPE_BASE = 0,
        API_TYPE_OPEN,
        API_TYPE_SET_PARAMETERS,
        API_TYPE_START_PREVIEW,
        API_TYPE_STOP_PREVIEW,
        API_TYPE_TAKE_PICTURE,
        API_TYPE_RELEASE,
        API_TYPE_CLOSE,
        API_TYPE_CHECK_API_DONE,
        API_TYPE_MAX,
    };

private:
    typedef ExynosCameraThread<ExynosCameraThreadInterface> cameraThreadInterface;

    const struct hw_module_t*      m_module;
    int                            m_cameraId;
    char                           m_name[EXYNOS_CAMERA_NAME_STR_SIZE];

    camera_device_t               *m_cameraDevice;

    enum RUM_MODE                  m_runMode;
    List<enum API_TYPE>            m_apiQ;
    Mutex                          m_apiQLock;

    sp<cameraThreadInterface>      m_apiThread;
    enum API_TYPE                  m_lastApiType;
    status_t                       m_lastApiRet;

    String8                        m_params;

private:
    status_t m_runThread(enum API_TYPE apiType, const char *params = NULL);
    status_t m_waitThread(enum API_TYPE apiType);
    bool     m_mainThreadFunc(void);

    void     m_pushApiQ(enum API_TYPE apiType);
    ExynosCameraThreadInterface::API_TYPE m_popApiQ(void);

    void     m_skipApiQ(enum API_TYPE apiType);
    void     m_skipApiQ(enum API_TYPE apiType1, enum API_TYPE apiType2);
    void     m_skipWithoutApiQ(enum API_TYPE apiType);

    int      m_sizeOfApiQ(void);
    void     m_clearApiQ(void);

    static char    *m_apiType2Str(enum API_TYPE apiType);
    static char    *m_runMode2Str(enum RUM_MODE runMode);
};

ExynosCameraThreadInterface *g_threadInterface[MAX_NUM_OF_CAMERA];
ExynosCameraCallbackHooker  *g_callbackHooker[MAX_NUM_OF_CAMERA];

}; /* namespace android */
#endif
