/*
 * Copyright (C) 2015, Samsung Electronics Co. LTD
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

#define LOG_TAG "ExynosVisionBufObject"
#include <cutils/log.h>

#include "ExynosVisionBufObject.h"

namespace android {

ExynosVisionBufMemory::ExynosVisionBufMemory()
{
    m_fd = 0;
    m_userptr = NULL;

    m_imported_flag = vx_false_e;
    m_buffer_size = 0;
}

ExynosVisionBufMemory::~ExynosVisionBufMemory()
{

}

vx_status
ExynosVisionBufMemory::alloc(ExynosVisionAllocator *allocator, vx_uint32 buf_size)
{
    vx_status status = VX_SUCCESS;
    status_t ret;

    int fd;
    char *addr;

    ret = allocator->alloc_mem(buf_size, &fd, &addr, vx_true_e);
    if (ret != NO_ERROR) {
        VXLOGE("allocation fail, ret:%d", ret);
        status = VX_FAILURE;
    }

    m_fd = fd;
    m_userptr = addr;
    m_buffer_size = buf_size;

    return status;
}

vx_status
ExynosVisionBufMemory::import(void *ptr, int fd)
{
    vx_status status = VX_SUCCESS;

    m_imported_flag = vx_true_e;
    m_fd = fd;
    m_userptr = (char*)ptr;

    return status;
}

vx_status
ExynosVisionBufMemory::free(ExynosVisionAllocator *allocator)
{
    if (m_imported_flag == vx_true_e)
        return VX_SUCCESS;

    vx_status status = VX_SUCCESS;
    status_t ret;

    ret = allocator->free_mem(m_buffer_size, &m_fd, &m_userptr, vx_true_e);
    if (ret != NO_ERROR) {
        VXLOGE("free fail, ret:%d", ret);
        status = VX_FAILURE;
    }

    return status;
}

}; /* namespace android */

