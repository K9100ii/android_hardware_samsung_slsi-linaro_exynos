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

#define LOG_TAG "ExynosCameraMemoryAllocator"
#include "ExynosCameraMemory.h"
#include <hardware/exynos/dmabuf_container.h>

namespace android {

ExynosCameraIonAllocator::ExynosCameraIonAllocator()
{
    m_ionClient   = -1;
    m_ionAlign    = 0;
    m_ionHeapMask = 0;
    m_ionFlags    = 0;
}

ExynosCameraIonAllocator::~ExynosCameraIonAllocator()
{
    exynos_ion_close(m_ionClient);
}

status_t ExynosCameraIonAllocator::init(bool isCached)
{
    status_t ret = NO_ERROR;

    if (m_ionClient < 0) {
        m_ionClient = exynos_ion_open();

        if (m_ionClient < 0) {
            ALOGE("ERR(%s):exynos_ion_open(%d) failed", __FUNCTION__, m_ionClient);
            ret = BAD_VALUE;
            goto func_exit;
        }
    }

    m_ionAlign    = 0;
    m_ionHeapMask = EXYNOS_ION_HEAP_SYSTEM_MASK;
    m_ionFlags    = (isCached == true ?
        (ION_FLAG_CACHED | ION_FLAG_CACHED_NEEDS_SYNC ) : 0);

func_exit:

    return ret;
}

status_t ExynosCameraIonAllocator::alloc(
        int size,
        int *fd,
        char **addr,
        bool mapNeeded)
{
    status_t ret = NO_ERROR;
    int ionFd = 0;
    char *ionAddr = NULL;

    if (m_ionClient < 0) {
        ALOGE("ERR(%s):allocator is not yet created", __FUNCTION__);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (size == 0) {
        ALOGE("ERR(%s):size equals zero", __FUNCTION__);
        ret = BAD_VALUE;
        goto func_exit;
    }

    ionFd = exynos_ion_alloc(m_ionClient, size, m_ionHeapMask, m_ionFlags);
    if (ionFd < 0) {
        ALOGE("ERR(%s):exynos_ion_alloc(fd=%d) failed(%s)", __FUNCTION__, ionFd, strerror(errno));
        ionFd = -1;
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (mapNeeded == true) {
        if (map(size, ionFd, &ionAddr) != NO_ERROR) {
            ALOGE("ERR(%s):map failed", __FUNCTION__);
        }
    }

func_exit:

    *fd   = ionFd;
    *addr = ionAddr;

    return ret;
}

status_t ExynosCameraIonAllocator::alloc(
        int size,
        int *fd,
        char **addr,
        int  mask,
        int  flags,
        bool mapNeeded)
{
    status_t ret = NO_ERROR;
    int ionFd = 0;
    char *ionAddr = NULL;

    if (m_ionClient < 0) {
        ALOGE("ERR(%s):allocator is not yet created", __FUNCTION__);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (size == 0) {
        ALOGE("ERR(%s):size equals zero", __FUNCTION__);
        ret = BAD_VALUE;
        goto func_exit;
    }

    ionFd = exynos_ion_alloc(m_ionClient, size, mask, flags);
    if (ionFd < 0) {
        ALOGE("ERR(%s):exynos_ion_alloc(fd=%d) failed(%s)", __FUNCTION__, ionFd, strerror(errno));
        ionFd = -1;
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (mapNeeded == true) {
        if (map(size, ionFd, &ionAddr) != NO_ERROR) {
            ALOGE("ERR(%s):map failed", __FUNCTION__);
        }
    }

func_exit:

    *fd   = ionFd;
    *addr = ionAddr;

    return ret;
}

status_t ExynosCameraIonAllocator::free(
        __unused int size,
        int *fd,
        char **addr,
        bool mapNeeded)
{
    status_t ret = NO_ERROR;
    int ionFd = *fd;
    char *ionAddr = *addr;

    if (ionFd < 0) {
        ALOGE("ERR(%s):ion_fd is lower than zero", __FUNCTION__);
        ret = BAD_VALUE;
        goto func_exit;
    }

    if (mapNeeded == true) {
        if (ionAddr == NULL) {
            ALOGE("ERR(%s):ion_addr equals NULL", __FUNCTION__);
            ret = BAD_VALUE;
            goto func_close_exit;
        }

        if (munmap(ionAddr, size) < 0) {
            ALOGE("ERR(%s):munmap failed", __FUNCTION__);
            ret = INVALID_OPERATION;
            goto func_close_exit;
        }
    }

func_close_exit:

    exynos_ion_close(ionFd);

    ionFd   = -1;
    ionAddr = NULL;

func_exit:

    *fd   = ionFd;
    *addr = ionAddr;

    return ret;
}

status_t ExynosCameraIonAllocator::map(int size, int fd, char **addr)
{
    status_t ret = NO_ERROR;
    char *ionAddr = NULL;

    if (size == 0) {
        ALOGE("ERR(%s):size equals zero", __FUNCTION__);
        ret = BAD_VALUE;
        goto func_exit;
    }

    if (fd <= 0) {
        ALOGE("ERR(%s):fd=%d failed", __FUNCTION__, fd);
        ret = BAD_VALUE;
        goto func_exit;
    }

    ionAddr = (char *)mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

    if (ionAddr == (char *)MAP_FAILED || ionAddr == NULL) {
        ALOGE("ERR(%s):ion_map(size=%d) failed, (fd=%d), (%s)", __FUNCTION__, size, fd, strerror(errno));
        close(fd);
        ionAddr = NULL;
        ret = INVALID_OPERATION;
        goto func_exit;
    }

func_exit:

    *addr = ionAddr;

    return ret;
}

void ExynosCameraIonAllocator::setIonHeapMask(int mask)
{
    m_ionHeapMask |= mask;
}

void ExynosCameraIonAllocator::setIonFlags(int flags)
{
    m_ionFlags |= flags;
}

status_t ExynosCameraIonAllocator::createBufferContainer(int *fd, int batchSize, int *containerFd)
{
    int bufferContainerFd = -1;
    if (fd == NULL || batchSize < 1 || containerFd == NULL) {
        ALOGE("ERR(%s[%d]):Invalid parameters. fd %p batchSize %d containerFd %p",
                __FUNCTION__, __LINE__,
                fd, batchSize, containerFd);
        return BAD_VALUE;
    }

    bufferContainerFd = dma_buf_merge(*fd, fd + 1, batchSize - 1);
    if (bufferContainerFd < 0) {
        ALOGE("ERR(%s[%d]):Failed to create BufferContainer. batchSize %d containerFd %d",
                __FUNCTION__, __LINE__,
                batchSize, bufferContainerFd);
        return INVALID_OPERATION;
    }

    ALOGV("DEBUG(%s[%d]):Success to create BufferContiner. batchSize %d containerFd %d",
            __FUNCTION__, __LINE__,
            batchSize, bufferContainerFd);

    *containerFd = bufferContainerFd;

    return NO_ERROR;
}

ExynosCameraStreamAllocator::ExynosCameraStreamAllocator(int actualFormat)
{
    m_allocator = NULL;
    m_actualFormat = actualFormat;

    m_grallocMapper = new GrallocWrapper::Mapper();
}

ExynosCameraStreamAllocator::~ExynosCameraStreamAllocator()
{
    if (m_grallocMapper != NULL) {
        delete m_grallocMapper;
        m_grallocMapper = NULL;
    }
}

status_t ExynosCameraStreamAllocator::init(camera3_stream_t *allocator)
{
    status_t ret = NO_ERROR;

    m_allocator = allocator;

    return ret;
}

int ExynosCameraStreamAllocator::lock(
        buffer_handle_t **bufHandle,
        int fd[],
        char *addr[],
        bool *isLocked,
        int planeCount)
{
    int ret = 0;
    uint32_t usage  = 0;
    uint32_t format = 0;
    void  *grallocAddr[3] = {NULL};
    const private_handle_t *priv_handle = NULL;
    int   grallocFd[3] = {0};
    ExynosCameraDurationTimer   lockbufferTimer;
    GrallocWrapper::Error error = GrallocWrapper::Error::NONE;
    GrallocWrapper::YCbCrLayout ycbcrLayout;
    GrallocWrapper::IMapper::Rect rect;

    if (bufHandle == NULL) {
        ALOGE("ERR(%s):bufHandle equals NULL, failed", __FUNCTION__);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (*bufHandle == NULL) {
        ALOGE("ERR(%s):*bufHandle == NULL, failed", __FUNCTION__);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    ycbcrLayout.y = NULL;
    ycbcrLayout.cb = NULL;
    ycbcrLayout.cr = NULL;
    ycbcrLayout.yStride = 0;
    ycbcrLayout.cStride = 0;
    ycbcrLayout.chromaStep = 0;

    rect.left = 0;
    rect.top = 0;
    rect.width  = m_allocator->width;
    rect.height = m_allocator->height;
    usage  = m_allocator->usage;
    format = m_allocator->format;

    switch (m_actualFormat) {
    case HAL_PIXEL_FORMAT_EXYNOS_ARGB_8888:
    case HAL_PIXEL_FORMAT_RGBA_8888:
    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_BGRA_8888:
    case HAL_PIXEL_FORMAT_RGB_888:
    case HAL_PIXEL_FORMAT_RGB_565:
    case HAL_PIXEL_FORMAT_RAW16:
    case HAL_PIXEL_FORMAT_Y16:
    case HAL_PIXEL_FORMAT_Y8:
    case HAL_PIXEL_FORMAT_RAW_OPAQUE:
    case HAL_PIXEL_FORMAT_BLOB:
    case HAL_PIXEL_FORMAT_YCbCr_422_I:
        if (planeCount == 1) {
            lockbufferTimer.start();
            error = m_grallocMapper->lock(
                    **bufHandle,
                    usage,
                    rect,
                    -1, /* acquireFence */
                    grallocAddr);
            lockbufferTimer.stop();
            break;
        }
    default:
        lockbufferTimer.start();
/* HACK: YSUM solution for 64bit usage is not supported */
#ifdef USE_YSUM_RECORDING

        if ((usage & GRALLOC1_CONSUMER_USAGE_VIDEO_ENCODER)
                && (planeCount == 3)) {
            error = m_grallocMapper->lock(**bufHandle,
                    (uint64_t)usage | GRALLOC1_CONSUMER_USAGE_VIDEO_PRIVATE_DATA,
                    rect,
                    -1, /* acquireFence */
                    &ycbcrLayout);
        } else
#endif
        {
            error = m_grallocMapper->lock(**bufHandle,
                    usage,
                    rect,
                    -1, /* acquireFence */
                    &ycbcrLayout);
        }
        lockbufferTimer.stop();
        break;
    }

#if defined (EXYNOS_CAMERA_MEMORY_TRACE_GRALLOC_PERFORMANCE)
    ALOGD("DEBUG(%s[%d]):Check grallocHAL lock performance, duration(%ju usec)",
            __FUNCTION__, __LINE__, lockbufferTimer.durationUsecs());
#else
    if (lockbufferTimer.durationMsecs() > GRALLOC_WARNING_DURATION_MSEC)
        ALOGW("WRN(%s[%d]):grallocHAL->lock() duration(%ju msec)",
                __FUNCTION__, __LINE__, lockbufferTimer.durationMsecs());
#endif

    priv_handle = private_handle_t::dynamicCast(**bufHandle);

    if (error != GrallocWrapper::Error::NONE) {
        ALOGE("ERR(%s):grallocHal->lock failed.. ", __FUNCTION__);
        if (priv_handle == NULL) {
            ALOGE("ERR(%s):[H%p]private_handle is NULL",
                    __FUNCTION__, **bufHandle);
        } else {
            ALOGE("ERR(%s):[H%p]fd %d/%d/%d usage %x(P%jx/C%jx) format %x(I%jx/F%x) rect %d,%d,%d,%d size %d/%d/%d",
                    __FUNCTION__,
                    **bufHandle,
                    priv_handle->fd, priv_handle->fd1, priv_handle->fd2,
                    usage, priv_handle->producer_usage, priv_handle->consumer_usage,
                    format, priv_handle->internal_format, priv_handle->frameworkFormat,
                    rect.left, rect.top, rect.width, rect.height,
                    priv_handle->size, priv_handle->size1, priv_handle->size2);
        }

        ret = INVALID_OPERATION;
        goto func_exit;
    }

    switch (m_actualFormat) {
    case HAL_PIXEL_FORMAT_EXYNOS_YV12_M:
        grallocFd[2] = priv_handle->fd2;
        grallocAddr[2] = ycbcrLayout.cb;
        grallocFd[1] = priv_handle->fd1;
        grallocAddr[1] = ycbcrLayout.cr;
        grallocFd[0] = priv_handle->fd;
        grallocAddr[0] = ycbcrLayout.y;
        break;
    case HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M:
    case HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M_FULL:
        if ( /* (usage & GRALLOC1_CONSUMER_USAGE_VIDEO_PRIVATE_DATA) &&
              * : YSUM solution for 64bit usage is not supported */
                (usage & GRALLOC1_CONSUMER_USAGE_VIDEO_ENCODER) &&
                (m_actualFormat == HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M) &&
                (planeCount == 3)) {
            /* Use VideoMeta data */
            grallocFd[2] = priv_handle->fd2;
            grallocAddr[2] = ycbcrLayout.cb;
        }
        grallocFd[1] = priv_handle->fd1;
        grallocAddr[1] = ycbcrLayout.cr;
        grallocFd[0] = priv_handle->fd;
        grallocAddr[0] = ycbcrLayout.y;
        break;
    case HAL_PIXEL_FORMAT_YCbCr_420_888:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_P_M:
        grallocFd[2] = priv_handle->fd2;
        grallocAddr[2] = ycbcrLayout.cr;
        grallocFd[1] = priv_handle->fd1;
        grallocAddr[1] = ycbcrLayout.cb;
        grallocFd[0] = priv_handle->fd;
        grallocAddr[0] = ycbcrLayout.y;
        break;
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_P010_M:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_PRIV:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_S10B:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_S10B:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_TILED:
        if (usage & GRALLOC1_CONSUMER_USAGE_VIDEO_PRIVATE_DATA) {
            /* Use VideoMeta data */
            grallocFd[2] = priv_handle->fd2;
            grallocAddr[2] = ycbcrLayout.cr;
        }
        grallocFd[1] = priv_handle->fd1;
        grallocAddr[1] = ycbcrLayout.cb;
        grallocFd[0] = priv_handle->fd;
        grallocAddr[0] = ycbcrLayout.y;
        break;
    case HAL_PIXEL_FORMAT_EXYNOS_ARGB_8888:
    case HAL_PIXEL_FORMAT_RGBA_8888:
    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_BGRA_8888:
    case HAL_PIXEL_FORMAT_RGB_888:
    case HAL_PIXEL_FORMAT_RGB_565:
    case HAL_PIXEL_FORMAT_RAW16:
    case HAL_PIXEL_FORMAT_RAW_OPAQUE:
    case HAL_PIXEL_FORMAT_BLOB:
    case HAL_PIXEL_FORMAT_Y8:
    case HAL_PIXEL_FORMAT_Y16:
    case HAL_PIXEL_FORMAT_YCbCr_422_I:
    case HAL_PIXEL_FORMAT_YV12:
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_YCbCr_422_SP:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_P:
    case HAL_PIXEL_FORMAT_EXYNOS_YCrCb_422_I:
    case HAL_PIXEL_FORMAT_EXYNOS_CbYCrY_422_I:
    case HAL_PIXEL_FORMAT_EXYNOS_CrYCbY_422_I:
    case HAL_PIXEL_FORMAT_EXYNOS_YCrCb_422_SP:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_TILED:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_PN:
    default:
        grallocFd[0] = priv_handle->fd;
        if (grallocAddr[0] == NULL) {
            grallocAddr[0] = ycbcrLayout.y;
        }
        break;
    }

    *isLocked    = true;

func_exit:
    switch (planeCount) {
    case 3:
        fd[2]   = grallocFd[2];
        addr[2] = (char *)grallocAddr[2];
    case 2:
        fd[1]   = grallocFd[1];
        addr[1] = (char *)grallocAddr[1];
    case 1:
        fd[0]   = grallocFd[0];
        addr[0] = (char *)grallocAddr[0];
        break;
    default:
        break;
    }

    return ret;
}

int ExynosCameraStreamAllocator::unlock(buffer_handle_t *bufHandle)
{
    int ret = 0;
    int releaseFence = -1;

    if (bufHandle == NULL) {
        ALOGE("ERR(%s):bufHandle equals NULL", __FUNCTION__);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    releaseFence = m_grallocMapper->unlock(*bufHandle);

func_exit:
    return ret;
}
}
