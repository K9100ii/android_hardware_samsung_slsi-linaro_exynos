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

#ifndef EXYNOS_OPENVX_MEMORY_ALLOCATION_H
#define EXYNOS_OPENVX_MEMORY_ALLOCATION_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <utils/Errors.h>
#include <ion/ion.h>
#include <sys/mman.h>

#include "ExynosVisionCommonConfig.h"

namespace android {

class ExynosVisionAllocator {
private:

public:

private:

public:
    ExynosVisionAllocator();
    virtual ~ExynosVisionAllocator();

    virtual status_t alloc_mem(
            int size,
            int *fd,
            char **addr,
            bool mapNeeded);
    virtual status_t free_mem(
            int size,
            int *fd,
            char **addr,
            bool mapNeeded);

    virtual status_t init(bool isCached);
};

class ExynosVisionIonAllocator : public ExynosVisionAllocator {
public:
    ExynosVisionIonAllocator();
    virtual ~ExynosVisionIonAllocator();

    virtual status_t init(bool isCached);
    virtual status_t alloc_mem(
            int size,
            int *fd,
            char **addr,
            bool mapNeeded);
    virtual status_t alloc_mem(
            int size,
            int *fd,
            char **addr,
            int  mask,
            int  flags,
            bool mapNeeded);
    virtual status_t free_mem(
            int size,
            int *fd,
            char **addr,
            bool mapNeeded);
    status_t map(int size, int fd, char **addr);
    void     setIonHeapMask(int mask);
    void     setIonFlags(int flags);

private:
    int             m_ionClient;
    size_t          m_ionAlign;
    unsigned int    m_ionHeapMask;
    unsigned int    m_ionFlags;
};

}; // namespace android
#endif
