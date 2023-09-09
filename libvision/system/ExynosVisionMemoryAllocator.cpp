/*
 * Copyright 2013, Samsung Electronics Co. LTD
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

#define LOG_TAG "ExynosVisionMemoryAllocator"
#include <cutils/log.h>

#include <cstdio>

#include "ExynosVisionMemoryAllocator.h"

namespace android {

using namespace std;

ExynosVisionAllocator::ExynosVisionAllocator()
{

}

ExynosVisionAllocator::~ExynosVisionAllocator()
{

}

status_t
ExynosVisionAllocator::alloc_mem(
        int size,
        int *fd,
        char **addr,
        bool mapNeeded)
{
    status_t ret = NO_ERROR;
    void *ptr = NULL;

    if (size == 0) {
        VXLOGE("ERR(%s):size equals zero, mapNeeded:%d", mapNeeded);
        ret = BAD_VALUE;
        goto func_exit;
    }

    ptr = malloc(size);
    if (ptr == NULL) {
        VXLOGE("not implemented yet");
        ret = NO_MEMORY;
    }

func_exit:
    *fd   = 0;
    *addr = (char*)ptr;

    return ret;
}

status_t
ExynosVisionAllocator::free_mem(
        int size,
        int *fd,
        char **addr,
        bool mapNeeded)
{
    status_t ret = NO_ERROR;
    void *ptr = *addr;

    if (ptr != NULL) {
        free(ptr);
    } else {
        VXLOGE("address should not be zero, size:%d, mapNeeded:%d", size, mapNeeded);
        ret = INVALID_OPERATION;
    }

    *fd = -1;
    *addr = NULL;

    return ret;
}

status_t
ExynosVisionAllocator::init(bool isCached)
{
    if (isCached) {
        VXLOGE("caching is not supported");
    }

    return NO_ERROR;
}

ExynosVisionIonAllocator::ExynosVisionIonAllocator()
{
    m_ionClient   = 0;
    m_ionAlign    = 0;
    m_ionHeapMask = 0;
    m_ionFlags    = 0;
}

ExynosVisionIonAllocator::~ExynosVisionIonAllocator()
{
    ion_close(m_ionClient);
}

status_t ExynosVisionIonAllocator::init(bool isCached)
{
    status_t ret = NO_ERROR;

    if (m_ionClient == 0) {
        m_ionClient = ion_open();

        if (m_ionClient < 0) {
            VXLOGE("ERR(%s):ion_open failed");
            ret = BAD_VALUE;
            goto func_exit;
        }
    }

    m_ionAlign    = 0;
    m_ionHeapMask = ION_HEAP_SYSTEM_MASK;
    m_ionFlags    = (isCached == true ?
        (ION_FLAG_CACHED | ION_FLAG_CACHED_NEEDS_SYNC ) : 0);

func_exit:

    return ret;
}

status_t ExynosVisionIonAllocator::alloc_mem(
        int size,
        int *fd,
        char **addr,
        bool mapNeeded)
{
    status_t ret = NO_ERROR;
    int ionFd = 0;
    char *ionAddr = NULL;

    if (m_ionClient == 0) {
        VXLOGE("ERR:allocator is not yet created");
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (size == 0) {
        VXLOGE("ERR:size equals zero");
        ret = BAD_VALUE;
        goto func_exit;
    }

    ret = ion_alloc_fd(m_ionClient, size, m_ionAlign, m_ionHeapMask, m_ionFlags, &ionFd);

    if (ret < 0) {
        VXLOGE("ERR:ion_alloc_fd(fd=%d) failed(%s)", ionFd, strerror(errno));
        ionFd = -1;
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (mapNeeded == true) {
        if (map(size, ionFd, &ionAddr) != NO_ERROR) {
            VXLOGE("ERR:map failed");
        }
    }

    VXLOGD2("ion buffer is allocated, fd:%d, addr:0x%Xd, size:%d", ionFd, ionAddr, size);

func_exit:
    *fd   = ionFd;
    *addr = ionAddr;

    return ret;
}

status_t ExynosVisionIonAllocator::alloc_mem(
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

    if (m_ionClient == 0) {
        VXLOGE("ERR:allocator is not yet created");
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (size == 0) {
        VXLOGE("ERR:size equals zero");
        ret = BAD_VALUE;
        goto func_exit;
    }

    ret  = ion_alloc_fd(m_ionClient, size, m_ionAlign, mask, flags, &ionFd);

    if (ret < 0) {
        VXLOGE("ERR:ion_alloc_fd(fd=%d) failed(%s)", ionFd, strerror(errno));
        ionFd = -1;
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (mapNeeded == true) {
        if (map(size, ionFd, &ionAddr) != NO_ERROR) {
            VXLOGE("ERR:map failed");
        }
    }

func_exit:

    *fd   = ionFd;
    *addr = ionAddr;

    return ret;
}

status_t ExynosVisionIonAllocator::free_mem(
        int size,
        int *fd,
        char **addr,
        bool mapNeeded)
{
    status_t ret = NO_ERROR;
    int ionFd = *fd;
    char *ionAddr = *addr;

    if (ionFd < 0) {
        VXLOGE("ERR:ion_fd is lower than zero");
        ret = BAD_VALUE;
        goto func_exit;
    }

    if (mapNeeded == true) {
        if (ionAddr == NULL) {
            VXLOGE("ERR:ion_addr equals NULL");
            ret = BAD_VALUE;
            goto func_exit;
        }

        if (munmap(ionAddr, size) < 0) {
            VXLOGE("ERR:munmap failed");
            ret = INVALID_OPERATION;
            goto func_exit;
        }
    }

    ion_close(ionFd);

    ionFd   = -1;
    ionAddr = NULL;

func_exit:

    *fd   = ionFd;
    *addr = ionAddr;

    return ret;
}

status_t ExynosVisionIonAllocator::map(int size, int fd, char **addr)
{
    status_t ret = NO_ERROR;
    char *ionAddr = NULL;

    if (size == 0) {
        VXLOGE("size equals zero");
        ret = BAD_VALUE;
        goto func_exit;
    }

    if (fd <= 0) {
        VXLOGE("fd=%d failed", size);
        ret = BAD_VALUE;
        goto func_exit;
    }

    ionAddr = (char *)mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

    if (ionAddr == (char *)MAP_FAILED || ionAddr == NULL) {
        VXLOGE("ion_map(size=%d) failed", size);
        close(fd);
        ionAddr = NULL;
        ret = INVALID_OPERATION;
        goto func_exit;
    }

func_exit:

    *addr = ionAddr;

    return ret;
}

void ExynosVisionIonAllocator::setIonHeapMask(int mask)
{
    m_ionHeapMask |= mask;
}

void ExynosVisionIonAllocator::setIonFlags(int flags)
{
    m_ionFlags |= flags;
}

}

