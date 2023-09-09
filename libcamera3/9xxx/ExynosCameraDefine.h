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

#ifndef EXYNOS_CAMERA_CLASS_COMMON_DEFINE
#define EXYNOS_CAMERA_CLASS_COMMON_DEFINE

#include <utils/threads.h>
#include <utils/RefBase.h>
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <hardware/camera.h>
#include <hardware/camera3.h>
#include <hardware/gralloc1.h>
#include <CameraParameters.h>
#include <CameraMetadata.h>
#include <media/hardware/MetadataBufferType.h>
#include <system/camera_metadata.h>

#include <fcntl.h>
#include <sys/mman.h>
#include "csc.h"

#include "ExynosCameraParameters.h"
#include "ExynosCameraFrameFactory.h"
#include "ExynosCameraMemory.h"
#include "ExynosCameraBufferSupplier.h"
#include "ExynosCameraActivityControl.h"
#include "ExynosCameraFrameSelector.h"

#ifdef USE_CAMERA_PREVIEW_FRAME_SCHEDULER
#include "SecCameraPreviewFrameSchedulerSimple.h"
#endif

#ifdef SAMSUNG_TN_FEATURE
#include "SecCameraUtil.h"
#endif

#ifdef SAMSUNG_DNG
#include "SecCameraDngCreator.h"
#endif

namespace android {

typedef ExynosCameraList<ExynosCameraFrameSP_sptr_t> frame_queue_t;
typedef ExynosCameraList<ExynosCameraBuffer> buffer_queue_t;
typedef ExynosCameraList<buffer_dump_info_t> buffer_dump_info_queue_t;

typedef ExynosCameraList<uint32_t> worker_queue_t;
#ifdef SAMSUNG_DNG
typedef ExynosCameraList<ExynosCameraBuffer> dng_capture_queue_t;
typedef ExynosCameraList<ExynosCameraBuffer> bayer_release_queue_t;
#endif

typedef sp<ExynosCameraFrame>  ExynosCameraFrameSP_t;
typedef sp<ExynosCameraFrame>  ExynosCameraFrameSP_sptr_t; /* single ptr */
typedef sp<ExynosCameraFrame>& ExynosCameraFrameSP_dptr_t; /* double ptr */
typedef wp<ExynosCameraFrame> ExynosCameraFrameWP_t;
typedef wp<ExynosCameraFrame>& ExynosCameraFrameWP_dptr_t; /* wp without inc refcount */

typedef unsigned char uchar;
typedef unsigned char byte;

#define STREAM_OPTION_PREVIEW_MASK           (1 << 0)
#define STREAM_OPTION_STALL_MASK             (1 << 1)
#define STREAM_OPTION_THUMBNAIL_CB_MASK      (1 << 2)
#define STREAM_OPTION_DEPTH10_MASK           (1 << 3)
#define STREAM_OPTION_IRIS_MASK              (1 << 4)
#define STREAM_OPTION_STITCHING              (1 << 5)

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

enum EXYNOS_CAMERA_STREAM_CHARACTERISTICS_ID {
    HAL_STREAM_ID_RAW                 = 0,
    HAL_STREAM_ID_PREVIEW             = 1,
    HAL_STREAM_ID_VIDEO               = 2,
    HAL_STREAM_ID_JPEG                = 3,
    HAL_STREAM_ID_CALLBACK            = 4,
    HAL_STREAM_ID_ZSL_INPUT           = 5,
    HAL_STREAM_ID_ZSL_OUTPUT          = 6,
    HAL_STREAM_ID_CALLBACK_STALL      = 7,
    HAL_STREAM_ID_THUMBNAIL_CALLBACK  = 8,
    HAL_STREAM_ID_DEPTHMAP            = 9,
    HAL_STREAM_ID_DEPTHMAP_STALL      = 10,
    HAL_STREAM_ID_VISION              = 11,
    HAL_STREAM_ID_PREVIEW_VIDEO       = 12,
    HAL_STREAM_ID_MAX                 = 13,
};

enum Request_Sync_Type {
    REQ_SYNC_NONE,
    REQ_SYNC_WITH_3AA,
    REQ_SYNC_WITH_BUFFER
};

typedef struct fdae_info {
    uint32_t    id[CAMERA2_MAX_FACES];
    uint32_t    score[CAMERA2_MAX_FACES];
    uint32_t    rect[CAMERA2_MAX_FACES][4];
    uint32_t    isRot[CAMERA2_MAX_FACES];
    uint32_t    rot[CAMERA2_MAX_FACES];
    uint32_t    faceNum;
    uint32_t    frameCount;
} fdae_info_t;

}
#endif
