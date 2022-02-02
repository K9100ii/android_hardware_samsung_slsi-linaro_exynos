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
 * \file      ExynosCameraMemory.h
 * \brief     header file for ExynosCameraMemory
 * \author    Sunmi Lee(carrotsm.lee@samsung.com)
 * \date      2013/07/22
 *
 */

#ifndef EXYNOS_CAMERA_MEMORY_H__
#define EXYNOS_CAMERA_MEMORY_H__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <binder/MemoryHeapBase.h>
#include <hardware/camera.h>
#include <hardware/camera3.h>
#include <videodev2.h>
#include <videodev2_exynos_camera.h>
#include <hardware/exynos/ion.h>
#include <ui/GraphicBuffer.h>

#include <GrallocWrapper.h>
#include "gralloc1_priv.h"

#include "exynos_format.h"

#include "ExynosCameraCommonInclude.h"

#include "fimc-is-metadata.h"
#include "ExynosCameraAutoTimer.h"

namespace android {
namespace GrallocWrapper {
    class Mapper;
    class Allocator;
}

/* #define EXYNOS_CAMERA_MEMORY_TRACE */

/* EXYNOS_CAMERA_MEMORY_TRACE_GRALLOC_PERFORMANCE define is log of gralloc function(xxxxx) duration.
 * If you want check function duration, enable this define.
 */
/* #define EXYNOS_CAMERA_MEMORY_TRACE_GRALLOC_PERFORMANCE */
#define GRALLOC_WARNING_DURATION_MSEC   (180)     /* 180ms */

class ExynosCameraIonAllocator {
public:
    ExynosCameraIonAllocator();
    virtual ~ExynosCameraIonAllocator();

    status_t init(bool isCached);
    status_t alloc(
            int size,
            int *fd,
            char **addr,
            bool mapNeeded);
    status_t alloc(
            int size,
            int *fd,
            char **addr,
            int  mask,
            int  flags,
            bool mapNeeded);
    status_t free(
            int size,
            int *fd,
            char **addr,
            bool mapNeeded);
    status_t map(int size, int fd, char **addr);
    void     setIonHeapMask(int mask);
    void     setIonFlags(int flags);
    status_t createBufferContainer(
            int *fd,
            int batchSize,
            int *containerFd);

private:
    int             m_ionClient;
    size_t          m_ionAlign;
    unsigned int    m_ionHeapMask;
    unsigned int    m_ionFlags;

    sp<GraphicBuffer>                m_graphicBuffer[VIDEO_MAX_FRAME];
};

class ExynosCameraStreamAllocator {
public:
    ExynosCameraStreamAllocator(int actualFormat);
    virtual ~ExynosCameraStreamAllocator();

    status_t init(camera3_stream_t *allocator);
    int      lock(
                buffer_handle_t **bufHandle,
                int fd[],
                char *addr[],
                bool *isLocked,
                int planeCount);

    int      unlock(buffer_handle_t *bufHandle);

private:
    camera3_stream_t                *m_allocator;
    GrallocWrapper::Mapper          *m_grallocMapper;
    int                             m_actualFormat;
};
}
#endif
