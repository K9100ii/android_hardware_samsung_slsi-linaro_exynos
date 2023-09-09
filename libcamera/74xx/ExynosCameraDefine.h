
#ifndef EXYNOS_CAMERA_CLASS_COMMON_DEFINE
#define EXYNOS_CAMERA_CLASS_COMMON_DEFINE

#include <utils/threads.h>
#include <utils/RefBase.h>
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <hardware/camera.h>
#include <hardware/camera3.h>
#include <hardware/gralloc.h>
#include <camera/Camera.h>
#include <camera/CameraParameters.h>
#include <camera/CameraMetadata.h>
#include <media/hardware/MetadataBufferType.h>
#include <system/camera_metadata.h>

#include <fcntl.h>
#include <sys/mman.h>
#include "csc.h"

#include "ExynosCameraParameters.h"

#include "ExynosCameraFrameFactory.h"
#include "ExynosCameraFrameFactoryPreview.h"
#include "ExynosCameraFrameFactory3aaIspM2M.h"
#include "ExynosCameraFrameFactory3aaIspM2MTpu.h"
#include "ExynosCameraFrameFactory3aaIspOtf.h"
#include "ExynosCameraFrameFactory3aaIspTpu.h"
#include "ExynosCameraFrameFactory3aaIspOtfTpu.h"
#include "ExynosCameraFrameReprocessingFactory.h"
#include "ExynosCameraFrameFactoryVision.h"
#include "ExynosCameraFrameFactoryFront.h"

#include "ExynosCameraMemory.h"
#include "ExynosCameraBufferManager.h"
#include "ExynosCameraBufferLocker.h"
#include "ExynosCameraActivityControl.h"
#include "ExynosCameraScalableSensor.h"
#include "ExynosCameraFrameSelector.h"

#ifdef USE_CAMERA_PREVIEW_FRAME_SCHEDULER
#include "SecCameraPreviewFrameSchedulerSimple.h"
#endif

#ifdef SAMSUNG_TN_FEATURE
#include "SecCameraParameters.h"
#endif

#ifdef SAMSUNG_DNG
#include "SecCameraDngCreator.h"
#endif

namespace android {

typedef struct ExynosCameraJpegCallbackBuffer {
    ExynosCameraBuffer buffer;
    int callbackNumber;
} jpeg_callback_buffer_t;

typedef ExynosCameraList<ExynosCameraFrame *> frame_queue_t;
typedef ExynosCameraList<ExynosCameraBuffer*> buffer_queue_t;
typedef ExynosCameraList<ExynosCameraFrameFactory *> framefactory_queue_t;

typedef ExynosCameraList<jpeg_callback_buffer_t> jpeg_callback_queue_t;
typedef ExynosCameraList<ExynosCameraBuffer> postview_callback_queue_t;
typedef ExynosCameraList<ExynosCameraBuffer> thumbnail_callback_queue_t;
#ifdef SAMSUNG_DNG
typedef ExynosCameraList<ExynosCameraBuffer> dng_capture_queue_t;
#endif
#ifdef SAMSUNG_DEBLUR
typedef ExynosCameraList<ExynosCameraBuffer> deblur_capture_queue_t;
#endif

#ifdef SAMSUNG_LBP
typedef struct ExynosCameraLBPbuffer {
    ExynosCameraBuffer buffer;
    uint32_t frameNumber;
} lbp_buffer_t;

typedef ExynosCameraList<lbp_buffer_t> lbp_queue_t;
#endif

#ifdef SAMSUNG_BD
typedef ExynosCameraList<UTstr> bd_queue_t;
#endif

typedef ExynosCameraList<ExynosCameraFrame *> capture_queue_t;


typedef enum buffer_direction_type {
    SRC_BUFFER_DIRECTION        = 0,
    DST_BUFFER_DIRECTION        = 1,
    INVALID_BUFFER_DIRECTION,
} buffer_direction_type_t;

enum jpeg_save_thread {
    JPEG_SAVE_THREAD0       = 0,
    JPEG_SAVE_THREAD1       = 1,
    JPEG_SAVE_THREAD2,
    JPEG_SAVE_THREAD_MAX_COUNT,
};
/*
typedef struct {
    uint32_t frameNumber;
    camera3_stream_buffer streamBuffer;
} result_buffer_info_t;
*/
/*
enum FRAME_FACTORY_TYPE {
    FRAME_FACTORY_TYPE_CAPTURE_PREVIEW = 0,
    FRAME_FACTORY_TYPE_RECORDING_PREVIEW,
    FRAME_FACTORY_TYPE_DUAL_PREVIEW,
    FRAME_FACTORY_TYPE_REPROCESSING,
    FRAME_FACTORY_TYPE_VISION,
    FRAME_FACTORY_TYPE_MAX,
};
*/
enum FRAME_FACTORY_TYPE {
    FRAME_FACTORY_TYPE_3AA_ISP_M2M = 0,
    FRAME_FACTORY_TYPE_3AA_ISP_M2M_TPU,
    FRAME_FACTORY_TYPE_3AA_ISP_OTF,
    FRAME_FACTORY_TYPE_3AA_ISP_OTF_TPU,
    FRAME_FACTORY_TYPE_CAPTURE_PREVIEW,
    FRAME_FACTORY_TYPE_RECORDING_PREVIEW,
    FRAME_FACTORY_TYPE_DUAL_PREVIEW,
    FRAME_FACTORY_TYPE_REPROCESSING,
    FRAME_FACTORY_TYPE_VISION,
    FRAME_FACTORY_TYPE_MAX,
};

enum EXYNOS_CAMERA_STREAM_CHARACTERISTICS_ID {
    HAL_STREAM_ID_RAW           = 0,
    HAL_STREAM_ID_PREVIEW       = 1,
    HAL_STREAM_ID_VIDEO         = 2,
    HAL_STREAM_ID_JPEG          = 3,
    HAL_STREAM_ID_CALLBACK      = 4,
    HAL_STREAM_ID_ZSL           = 5,
    HAL_STREAM_ID_DUMMY         = 6,
    HAL_STREAM_ID_MAX           = 7,
};


//typedef ExynosCameraList<ExynosCameraFrameFactory *> framefactory_queue_t;

#ifdef SAMSUNG_LLV
enum LLV_status {
    LLV_UNINIT              = 0,
    LLV_INIT                = 1,
    LLV_STOPPED,
};
#endif

#ifdef SAMSUNG_HLV
enum HLV_process_step {
    HLV_PROCESS_DONE = 0,
    HLV_PROCESS_PRE = 1,
    HLV_PROCESS_SET,
    HLV_PROCESS_START,
};
#endif

#ifdef SAMSUNG_OT
enum objet_tracking_status {
    OBJECT_TRACKING_DEINIT              = 0,
    OBJECT_TRACKING_INIT                = 1,
    OBJECT_TRACKING_IDLE,
};
#endif

#ifdef SAMSUNG_BD
enum BD_status {
    BLUR_DETECTION_DEINIT              = 0,
    BLUR_DETECTION_INIT                = 1,
    BLUR_DETECTION_IDLE,
};
#endif
}

#endif
