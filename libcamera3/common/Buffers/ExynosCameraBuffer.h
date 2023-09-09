/*
 * Copyright 2017, Samsung Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed toggle an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*!
 * \file      ExynosCameraBuffer.h
 * \brief     hearder file for ExynosCameraBuffer
 * \author    Sunmi Lee(carrotsm.lee@samsung.com)
 * \date      2013/07/17
 *
 */

#ifndef EXYNOS_CAMERA_BUFFER_H__
#define EXYNOS_CAMERA_BUFFER_H__


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <binder/MemoryHeapBase.h>
#include <hardware/camera.h>
#include <videodev2.h>
#include <videodev2_exynos_camera.h>

#include "gralloc1_priv.h"

#include "ExynosCameraMemory.h"
#include "fimc-is-metadata.h"

namespace android {

/* metadata plane : non-cached buffer */
/* image plane (default) : non-cached buffer */
#define EXYNOS_CAMERA_BUFFER_1MB                        (1024*1024)
#define EXYNOS_CAMERA_BUFFER_WARNING_TIME_MARGIN        (100)  /* 0.1ms per 1MB */

#define EXYNOS_CAMERA_BUFFER_ION_MASK_NONCACHED         (EXYNOS_ION_HEAP_SYSTEM_MASK)
#define EXYNOS_CAMERA_BUFFER_ION_FLAG_NONCACHED         (0)
#define EXYNOS_CAMERA_BUFFER_ION_WARNING_TIME_NONCACHED (1600 + EXYNOS_CAMERA_BUFFER_WARNING_TIME_MARGIN)  /* 1.6ms per 1MB */

#define EXYNOS_CAMERA_BUFFER_ION_MASK_CACHED            (EXYNOS_ION_HEAP_SYSTEM_MASK)
#define EXYNOS_CAMERA_BUFFER_ION_FLAG_CACHED            (ION_FLAG_CACHED | ION_FLAG_CACHED_NEEDS_SYNC)
#define EXYNOS_CAMERA_BUFFER_ION_FLAG_CACHED_SYNC_FORCE (ION_FLAG_CACHED | ION_FLAG_CACHED_NEEDS_SYNC | ION_FLAG_SYNC_FORCE)
#define EXYNOS_CAMERA_BUFFER_ION_WARNING_TIME_CACHED    (670 + EXYNOS_CAMERA_BUFFER_WARNING_TIME_MARGIN)  /* 0.67ms per 1MB */

#define EXYNOS_CAMERA_BUFFER_ION_MASK_RESERVED          (EXYNOS_ION_HEAP_VIDEO_FRAME_MASK)
#define EXYNOS_CAMERA_BUFFER_ION_FLAG_RESERVED          (0)
#define EXYNOS_CAMERA_BUFFER_ION_WARNING_TIME_RESERVED  (50)  /* 0.05ms */

#define EXYNOS_CAMERA_BUFFER_GRALLOC_WARNING_TIME       (3300 + EXYNOS_CAMERA_BUFFER_WARNING_TIME_MARGIN)  /* 3.3ms per 1MB */

#define EXYNOS_CAMERA_BUFFER_ION_MASK_SECURE            (EXYNOS_ION_HEAP_SECURE_CAMERA_MASK)
#define EXYNOS_CAMERA_BUFFER_ION_FLAG_SECURE            (ION_FLAG_PROTECTED)

typedef enum exynos_camera_buffer_type {
    EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE = 0,
    EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE    = 1,
    EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE  = 2,
    EXYNOS_CAMERA_BUFFER_ION_NONCACHED_RESERVED_TYPE = EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE,
    EXYNOS_CAMERA_BUFFER_ION_CACHED_RESERVED_TYPE = 3,
    EXYNOS_CAMERA_BUFFER_ION_CACHED_SYNC_FORCE_TYPE = 4,
    EXYNOS_CAMERA_BUFFER_ION_RESERVED_SECURE_TYPE = 5,
    EXYNOS_CAMERA_BUFFER_ION_CACHED_RESERVED_SECURE_TYPE = 6,
    EXYNOS_CAMERA_BUFFER_INVALID_TYPE,
} exynos_camera_buffer_type_t;

enum EXYNOS_CAMERA_BUFFER_POSITION {
    EXYNOS_CAMERA_BUFFER_POSITION_NONE = 0,
    EXYNOS_CAMERA_BUFFER_POSITION_IN_DRIVER,
    EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL,
    EXYNOS_CAMERA_BUFFER_POSITION_IN_SERVICE,
    EXYNOS_CAMERA_BUFFER_POSITION_MAX
};

enum EXYNOS_CAMERA_BUFFER_PERMISSION {
    EXYNOS_CAMERA_BUFFER_PERMISSION_NONE = 0,
    EXYNOS_CAMERA_BUFFER_PERMISSION_AVAILABLE,
    EXYNOS_CAMERA_BUFFER_PERMISSION_IN_PROCESS,
    EXYNOS_CAMERA_BUFFER_PERMISSION_MAX
};

struct ExynosCameraBufferStatus {
    int  driverReturnValue;
    enum EXYNOS_CAMERA_BUFFER_POSITION   position;
    enum EXYNOS_CAMERA_BUFFER_PERMISSION permission;
#ifdef __cplusplus
    ExynosCameraBufferStatus() {
        driverReturnValue = 0;
        position   = EXYNOS_CAMERA_BUFFER_POSITION_NONE;
        permission = EXYNOS_CAMERA_BUFFER_PERMISSION_NONE;
    }

    ExynosCameraBufferStatus& operator =(const ExynosCameraBufferStatus &other) {
        driverReturnValue = other.driverReturnValue;
        position   = other.position;
        permission = other.permission;

        return *this;
    }

    bool operator ==(const ExynosCameraBufferStatus &other) const {
        bool ret = true;

        if (driverReturnValue != other.driverReturnValue
        || position   != other.position
        || permission != other.permission) {
            ret = false;
        }

        return ret;
    }

    bool operator !=(const ExynosCameraBufferStatus &other) const {
        return !(*this == other);
    }
#endif
};

typedef struct buffer_manager_tag{
    int pipeId[5];
    int managerType;

    buffer_manager_tag() {
        int length = sizeof(pipeId) / sizeof(pipeId[0]);
        for (int i = 0; i < length; i++) {
            pipeId[i] = -1;
        }
        managerType = -1;
    }

    buffer_manager_tag& operator =(const buffer_manager_tag &other) {
        int length = sizeof(pipeId) / sizeof(pipeId[0]);
        for (int i = 0; i < length; i++) {
            this->pipeId[i] = other.pipeId[i];
        }
        this->managerType = other.managerType;

        return *this;
    }

    bool operator ==(const buffer_manager_tag &other) const {
        if (this->managerType != other.managerType) {
            return false;
        }

        for (int i = 0; i < 5 && this->pipeId[i] > -1; i++) {
            for (int j = 0; j < 5 && other.pipeId[j] > -1; j++) {
                if (this->pipeId[i] == other.pipeId[j]) {
                    return true;
                }
            }
        }

        return false;
    }

    bool operator !=(const buffer_manager_tag &other) const {
        return !(*this == other);
    }
} buffer_manager_tag_t;

struct ExynosCameraBuffer {
    int                             index;
    int                             planeCount;
    int                             batchSize;
    int                             fd[EXYNOS_CAMERA_BUFFER_MAX_PLANES];
    int                             containerFd[EXYNOS_CAMERA_BUFFER_MAX_PLANES];
    unsigned int                    size[EXYNOS_CAMERA_BUFFER_MAX_PLANES];
    unsigned int                    bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES];
    char                            *addr[EXYNOS_CAMERA_BUFFER_MAX_PLANES];
    struct ExynosCameraBufferStatus status;
    exynos_camera_buffer_type_t     type;   /* this value in effect exclude metadataPlane*/
    buffer_manager_tag_t            tag;

    buffer_handle_t                 *handle[EXYNOS_CAMERA_BUFFER_MAX_PLANES];
    int                             acquireFence[EXYNOS_CAMERA_BUFFER_MAX_PLANES];
    int                             releaseFence[EXYNOS_CAMERA_BUFFER_MAX_PLANES];

    void            *manager;
#ifdef __cplusplus
    ExynosCameraBuffer() {
        index = -1;
        planeCount = 0;
        batchSize = 1;
        for (int planeIndex = 0; planeIndex < EXYNOS_CAMERA_BUFFER_MAX_PLANES; planeIndex++) {
            fd[planeIndex]   = -1;
            containerFd[planeIndex] = -1;
            size[planeIndex] = 0;
            bytesPerLine[planeIndex] = 0;
            addr[planeIndex] = NULL;
            handle[planeIndex] = NULL;
            acquireFence[planeIndex] = -1;
            releaseFence[planeIndex] = -1;
        }
        status.driverReturnValue = 0;
        status.position          = EXYNOS_CAMERA_BUFFER_POSITION_NONE;
        status.permission        = EXYNOS_CAMERA_BUFFER_PERMISSION_NONE;
        type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;

        manager = NULL;
    }

    int getMetaPlaneIndex() {
        int metadataPlaneIndex = -1;
        metadataPlaneIndex = (planeCount - 1) * batchSize;
        if (size[metadataPlaneIndex] != EXYNOS_CAMERA_META_PLANE_SIZE) {
            android_printAssert(NULL, LOG_TAG,
                                "ASSERT(%s):Invalid access to metadata plane. planeIndex %d size %d",
                                __FUNCTION__, metadataPlaneIndex, size[metadataPlaneIndex]);
        }

        return metadataPlaneIndex;
    }

    ExynosCameraBuffer& operator =(const ExynosCameraBuffer &other) {
        index      = other.index;
        planeCount = other.planeCount;
        batchSize  = other.batchSize;
        for (int i = 0; i < EXYNOS_CAMERA_BUFFER_MAX_PLANES; i++) {
            fd[i]           = other.fd[i];
            containerFd[i]  = other.containerFd[i];
            size[i]         = other.size[i];
            bytesPerLine[i] = other.bytesPerLine[i];
            addr[i]         = other.addr[i];
            handle[i]       = other.handle[i];
            acquireFence[i] = other.acquireFence[i];
            releaseFence[i] = other.releaseFence[i];
        }
        status     = other.status;
        type       = other.type;
        tag        = other.tag;

        manager = other.manager;

        return *this;
    }

    bool operator ==(const ExynosCameraBuffer &other) const {
        bool ret = true;

        if (index != other.index
        || planeCount != other.planeCount
        || batchSize != other.batchSize
        || status != other.status
        || type   != other.type
        || manager != other.manager) {
            ret = false;
        }

        for (int i = 0; i < EXYNOS_CAMERA_BUFFER_MAX_PLANES; i++) {
            if (fd[i]  != other.fd[i]
            || containerFd[i] != other.containerFd[i]
            || size[i] != other.size[i]
            || bytesPerLine[i] != other.bytesPerLine[i]
            || addr[i] != other.addr[i]
            || handle[i] != other.handle[i]
            || acquireFence[i] != other.acquireFence[i]
            || releaseFence[i] != other.releaseFence[i]) {
                ret = false;
                break;
            }
        }

        return ret;
    }

    bool operator !=(const ExynosCameraBuffer &other) const {
        return !(*this == other);
    }
#endif
};
}
#endif
