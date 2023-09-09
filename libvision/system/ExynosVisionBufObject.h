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

#ifndef EXYNOS_VISION_BUFFER_H
#define EXYNOS_VISION_BUFFER_H

#include <utils/threads.h>
#include <utils/Mutex.h>
#include <utils/List.h>
#include <utils/Vector.h>

#include <VX/vx.h>

#include "ExynosVisionMemoryAllocator.h"
#include "ExynosVisionPool.h"

#define ID_BUF "[VX][%s]"

#define EXYNOS_VISION_BUFFER_TRACE
#ifdef EXYNOS_VISION_BUFFER_TRACE
#define VXLOGBF(fmt, ...) \
    ALOGD(Paste2(ID_BUF, fmt), __FUNCTION__,##__VA_ARGS__)
#else
#define VXLOGBF(fmt, ...)
#endif

namespace android {

#define DEFAULT_SLOT_NUM    2

class ExynosVisionBufMemory {
private:
    int m_fd;
    char *m_userptr;

    vx_bool m_imported_flag;
    vx_uint32   m_buffer_size;

public:

private:

public:
    /* Constructor */
    ExynosVisionBufMemory();

    /* Destructor */
    virtual ~ExynosVisionBufMemory();

    char* getAddr(void)
    {
        return m_userptr;
    }
    int getFd(void)
    {
        return m_fd;
    }

    vx_status alloc(ExynosVisionAllocator *allocator, vx_uint32 buf_size);
    vx_status import(void *ptr, int fd);
    vx_status free(ExynosVisionAllocator *allocator);
};

}; // namespace android
#endif
