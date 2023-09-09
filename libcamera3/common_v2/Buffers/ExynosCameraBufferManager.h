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
 * \file      ExynosCameraBufferManager.h
 * \brief     header file for ExynosCameraBufferManager
 * \author    Sunmi Lee(carrotsm.lee@samsung.com)
 * \date      2013/07/17
 *
 */

#ifndef EXYNOS_CAMERA_BUFFER_MANAGER_H__
#define EXYNOS_CAMERA_BUFFER_MANAGER_H__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <utils/List.h>
#include <utils/threads.h>
#include <cutils/properties.h>

#include <ui/Fence.h>
#include <videodev2.h>
#include <videodev2_exynos_camera.h>
#include <hardware/exynos/ion.h>

#include "gralloc1_priv.h"

#include "ExynosCameraCommonInclude.h"
#include "fimc-is-metadata.h"

#include "ExynosCameraObject.h"
#include "ExynosCameraList.h"
#include "ExynosCameraAutoTimer.h"
#include "ExynosCameraBuffer.h"
#include "ExynosCameraMemory.h"
#include "ExynosCameraThread.h"

namespace android {

/* #define DUMP_2_FILE */
/* #define EXYNOS_CAMERA_BUFFER_TRACE */

#ifdef EXYNOS_CAMERA_BUFFER_TRACE
#define EXYNOS_CAMERA_BUFFER_IN()   CLOGD("IN..")
#define EXYNOS_CAMERA_BUFFER_OUT()  CLOGD("OUT..")
#else
#define EXYNOS_CAMERA_BUFFER_IN()   ((void *)0)
#define EXYNOS_CAMERA_BUFFER_OUT()  ((void *)0)
#endif

// Hack: Close Fence FD if the fd is larger than specified number
// Currently, Joon's fence FD is not closed properly
/* #define FORCE_CLOSE_ACQUIRE_FD */
#define FORCE_CLOSE_ACQUIRE_FD_THRESHOLD    700

#define SWBUFFER_MAX_COUNT                  80

typedef enum buffer_manager_type {
    BUFFER_MANAGER_ION_TYPE                 = 0,
    BUFFER_MANAGER_FASTEN_AE_ION_TYPE       = 1,
    BUFFER_MANAGER_SERVICE_GRALLOC_TYPE     = 2,
    BUFFER_MANAGER_ONLY_HAL_USE_ION_TYPE    = 3,
    BUFFER_MANAGER_INVALID_TYPE,
} buffer_manager_type_t;

typedef enum buffer_manager_allocation_mode {
    BUFFER_MANAGER_ALLOCATION_ATONCE   = 0,   /* alloc() : allocation all buffers */
    BUFFER_MANAGER_ALLOCATION_ONDEMAND = 1,   /* alloc() : allocation the number of reqCount buffers, getBuffer() : increase buffers within limits */
    BUFFER_MANAGER_ALLOCATION_SILENT   = 2,   /* alloc() : same as ONDEMAND, increase buffers in background */
    BUFFER_MANAGER_ALLOCATION_INVALID_MODE,
} buffer_manager_allocation_mode_t;

typedef struct buffer_manager_configuration {
    int planeCount;
    unsigned int size[EXYNOS_CAMERA_BUFFER_MAX_PLANES];
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES];
    int startBufIndex;
    int reqBufCount;
    int allowedMaxBufCount;
    int batchSize;
    exynos_camera_buffer_type_t type;
    buffer_manager_allocation_mode_t allocMode;
    bool createMetaPlane;
    bool needMmap;
    int reservedMemoryCount;

    buffer_manager_configuration() {
        planeCount = 0;
        memset(size, 0x00, sizeof(size));
        memset(bytesPerLine, 0x00, sizeof(bytesPerLine));
        startBufIndex = 0;
        reqBufCount = 0;
        allowedMaxBufCount = 0;
        batchSize = 1;
        type = EXYNOS_CAMERA_BUFFER_INVALID_TYPE;
        allocMode = BUFFER_MANAGER_ALLOCATION_INVALID_MODE;
        createMetaPlane = false;
        needMmap = false;
        reservedMemoryCount = 0;
    }

    buffer_manager_configuration& operator =(const buffer_manager_configuration &other) {
        this->planeCount = other.planeCount;
        memcpy(this->size, other.size, sizeof(this->size));
        memcpy(this->bytesPerLine, other.bytesPerLine, sizeof(this->bytesPerLine));
        this->startBufIndex = other.startBufIndex;
        this->reqBufCount = other.reqBufCount;
        this->allowedMaxBufCount = other.allowedMaxBufCount;
        this->batchSize = other.batchSize;
        this->type = other.type;
        this->allocMode = other.allocMode;
        this->createMetaPlane = other.createMetaPlane;
        this->needMmap = other.needMmap;
        this->reservedMemoryCount = other.reservedMemoryCount;

        return *this;
    }
} buffer_manager_configuration_t;

class ExynosCameraBufferManager : public ExynosCameraObject {
protected:
    ExynosCameraBufferManager();

public:
    static ExynosCameraBufferManager *createBufferManager(buffer_manager_type_t type);
    virtual ~ExynosCameraBufferManager();

    status_t         create(const char *name, void *defaultAllocator);
    status_t         create(const char *name, int cameraId, void *defaultAllocator);

    void             init(void);
    virtual void     deinit(void);
    virtual status_t resetBuffers(void) = 0;

    status_t         setAllocator(void *allocator);

    void             setContigBufCount(int reservedMemoryCount);
    int              getContigBufCount(void);

    virtual status_t setInfo(buffer_manager_configuration_t info);
    virtual status_t alloc(void) = 0;

    virtual status_t putBuffer(
                        int bufIndex,
                        enum EXYNOS_CAMERA_BUFFER_POSITION position);
    virtual status_t getBuffer(
                        int    *reqBufIndex,
                        enum   EXYNOS_CAMERA_BUFFER_POSITION position,
                        struct ExynosCameraBuffer *buffer);

    virtual status_t updateStatus(
                        int bufIndex,
                        int driverValue,
                        enum EXYNOS_CAMERA_BUFFER_POSITION   position,
                        enum EXYNOS_CAMERA_BUFFER_PERMISSION permission);
    virtual status_t getStatus(
                        int bufIndex,
                        struct ExynosCameraBufferStatus *bufStatus);

    virtual status_t getIndexByFd(int fd, int *index);

    bool             isAllocated(void);
    virtual bool     isAvaliable(int bufIndex);

    void             dump(void);
    void             dumpBufferInfo(void);
    int              getAllocatedBufferCount(void);
    int              getAvailableIncreaseBufferCount(void);
    virtual int      getNumOfAvailableBuffer(void);
    virtual int      getNumOfAvailableAndNoneBuffer(void);
    int              getNumOfAllowedMaxBuffer(void);
    void             printBufferInfo(
                        const char *funcName,
                        const int lineNum,
                        int bufIndex,
                        int planeIndex);
    void             printBufferQState(void);
    virtual void     printBufferState(void);
    virtual void     printBufferState(int bufIndex, int planeIndex);

    virtual status_t increase(int increaseCount);
    virtual status_t cancelBuffer(int bufIndex);
    virtual status_t setBufferCount(int bufferCount);
    virtual int      getBufferCount(void);
    virtual int      getBufStride(void);

protected:
    virtual bool     m_allocationThreadFunc(void) = 0;
    status_t         m_free(void);

    status_t         m_setDefaultAllocator(void *allocator);
    virtual status_t m_defaultAlloc(int bIndex, int eIndex, bool isMetaPlane);
    virtual status_t m_defaultFree(int bIndex, int eIndex, bool isMetaPlane);
    virtual bool     m_checkInfoForAlloc(void);
    status_t         m_createDefaultAllocator(bool isCached = false);
    int              m_getTotalPlaneCount(int planeCount, int batchSize, bool hasMetaPlane);

    virtual void     m_resetSequenceQ(void);

    virtual status_t m_setAllocator(void *allocator) = 0;
    virtual status_t m_alloc(int bIndex, int eIndex) = 0;
    virtual status_t m_free(int bIndex, int eIndex)  = 0;

    virtual status_t m_increase(int increaseCount) = 0;
    virtual status_t m_decrease(void) = 0;

protected:
    bool                        m_flagAllocated;
    int                         m_reservedMemoryCount;
    int                         m_reqBufCount;
    int                         m_allocatedBufCount;
    int                         m_allowedMaxBufCount;
    bool                        m_flagSkipAllocation;
    bool                        m_isDestructor;
    mutable Mutex               m_lock;
    bool                        m_flagNeedMmap;

    bool                        m_hasMetaPlane;
    /* using internal allocator (ION) for MetaData plane */
    ExynosCameraIonAllocator    *m_defaultAllocator;
    bool                        m_isCreateDefaultAllocator;
    struct ExynosCameraBuffer   m_buffer[VIDEO_MAX_FRAME];
    List<int>                   m_availableBufferIndexQ;
    mutable Mutex               m_availableBufferIndexQLock;

    buffer_manager_allocation_mode_t m_allocMode;
    int                         m_indexOffset;
};

class InternalExynosCameraBufferManager : public ExynosCameraBufferManager {
public:
    InternalExynosCameraBufferManager();
    virtual ~InternalExynosCameraBufferManager();

    status_t increase(int increaseCount);
    bool     m_allocationThreadFunc(void);
    status_t alloc(void);
    status_t resetBuffers(void);

protected:
    status_t m_setAllocator(void *allocator);

    status_t m_alloc(int bIndex, int eIndex);
    status_t m_free(int bIndex, int eIndex);

    status_t m_increase(int increaseCount);
    status_t m_decrease(void);

    status_t m_constructBufferContainer(int bIndex, int eIndex);
    status_t m_destructBufferContainer(int bIndex, int eIndex);

private:
    typedef ExynosCameraThread<InternalExynosCameraBufferManager> allocThread;
    sp<allocThread>             m_allocationThread;
};

class ExynosCameraFence {
public:
    enum EXYNOS_CAMERA_FENCE_TYPE {
        EXYNOS_CAMERA_FENCE_TYPE_BASE = 0,
        EXYNOS_CAMERA_FENCE_TYPE_ACQUIRE,
        EXYNOS_CAMERA_FENCE_TYPE_RELEASE,
        EXYNOS_CAMERA_FENCE_TYPE_MAX,
    };

private:
    ExynosCameraFence();

public:
    ExynosCameraFence(
                enum EXYNOS_CAMERA_FENCE_TYPE fenceType,
                int acquireFence,
                int releaseFence);

    virtual ~ExynosCameraFence();

    int  getFenceType(void);
    int  getAcquireFence(void);
    int  getReleaseFence(void);

    bool isValid(void);
    status_t wait(int time = -1);

private:
    enum EXYNOS_CAMERA_FENCE_TYPE m_fenceType;

    int       m_acquireFence;
    int       m_releaseFence;

    sp<Fence> m_fence;

    bool      m_flagSwfence;
};

class ServiceExynosCameraBufferManager : public ExynosCameraBufferManager {
public:
    ServiceExynosCameraBufferManager(int actualFormat);
    virtual ~ServiceExynosCameraBufferManager();

    bool     m_allocationThreadFunc(void);
    status_t resetBuffers(void);
    status_t alloc(void);
    status_t putBuffer(int bufIndex,
                       enum EXYNOS_CAMERA_BUFFER_POSITION position);
    status_t getBuffer(int    *reqBufIndex,
                       enum   EXYNOS_CAMERA_BUFFER_POSITION position,
                       struct ExynosCameraBuffer *buffer);

    /*
     * The H/W fence sequence is
     * 1. On putBuffer (call by processCaptureRequest()),
     *    And, save acquire_fence, release_fence value.
     *    S/W fence : Make Fence class with acquire_fence on output_buffer.
     *
     * 2. when getBuffer,
     *    H/W fence : save acquire_fence, release_fence to ExynosCameraBuffer.
     *    S/W fence : wait Fence class that allocated at step 1.
     *
     *    (step 3 ~ step 4 is on ExynosCameraNode::m_qBuf())
     * 3. During H/W qBuf,
     *    give ExynosCameraBuffer.acquireFence (gotten step2) to v4l2
     *    v4l2_buffer.flags = V4L2_BUF_FLAG_USE_SYNC;
     *    v4l2_buffer.reserved = ExynosCameraBuffer.acquireFence;
     * 4. after H/W qBuf
     *    v4l2_buffer.reserved is changed to release_fence value.
     *    So,
     *    ExynosCameraBuffer.releaseFence = static_cast<int>(v4l2_buffer.reserved)
     *
     * 5. (step5 is on ExynosCamera::m_setResultBufferInfo())
     *    after H/W dqBuf, we meet handlePreview().
     *    we can set final value.
     *    result_buffer_info_t.acquire_fence = -1. (NOT original ExynosCameraBuffer.acquireFence)
     *    result_buffer_info_t.release_fence = ExynosCameraBuffer.releaseFence. (gotten by driver at step3)
     *    (service will look this release_fence.)
     *
     * 6  skip bufferManger::putBuffer().
     *     (we don't need to call putBuffer. because, buffer came from service.)
     *
     * 7. repeat from 1.
     */

protected:
    status_t m_setAllocator(void *allocator);
    status_t m_alloc(int bIndex, int eIndex);
    status_t m_free(int bIndex, int eIndex);

    status_t m_increase(int increaseCount);
    status_t m_decrease(void);

    status_t m_getBufferInfoFromHandle(buffer_handle_t handle, int planeCount, /* out */ int fd[]);
    status_t m_checkBufferInfo(const buffer_handle_t handle, const ExynosCameraBuffer* buffer);

    virtual status_t m_waitFence(ExynosCameraFence *fence);

    status_t m_constructBufferContainer(int bufIndex);
    status_t m_destructBufferContainer(int bufIndex);

private:
    ExynosCameraStreamAllocator     *m_allocator;
    bool                            m_handleIsLocked[VIDEO_MAX_FRAME];

    List<ExynosCameraFence *>       m_fenceList;
    mutable Mutex                   m_fenceListLock;

    typedef ExynosCameraThread<ServiceExynosCameraBufferManager> allocThread;
    sp<allocThread>             m_allocationThread;

};

class SWExynosCameraBufferManager : public ExynosCameraBufferManager {
public:
    SWExynosCameraBufferManager();
    virtual ~SWExynosCameraBufferManager();

    status_t         resetBuffers(void);
    status_t         alloc(void);
    status_t         setInfo(buffer_manager_configuration_t info);
    status_t         increase(int increaseCount);
    status_t         putBuffer(int bufIndex,
                                    enum EXYNOS_CAMERA_BUFFER_POSITION position);
    status_t         getBuffer(int    *reqBufIndex,
                                    enum   EXYNOS_CAMERA_BUFFER_POSITION position,
                                    struct ExynosCameraBuffer *buffer);
    status_t         updateStatus(int bufIndex,
                                        int driverValue,
                                        enum EXYNOS_CAMERA_BUFFER_POSITION   position,
                                        enum EXYNOS_CAMERA_BUFFER_PERMISSION permission);
    status_t         getStatus(int bufIndex,
                                    struct ExynosCameraBufferStatus *bufStatus);
    status_t         getIndexByFd(int fd, int *index);
    bool             isAvaliable(int bufIndex);
    int              getNumOfAvailableBuffer(void);
    int              getNumOfAvailableAndNoneBuffer(void);
    void             dumpBufferInfo(void);
    void             printBufferInfo(const char *funcName,
                                            const int lineNum,
                                            int bufIndex,
                                            int planeIndex);
    void             printBufferState(void);
    void             printBufferState(int bufIndex, int planeIndex);

protected:
    bool             m_allocationThreadFunc(void);

    void             m_resetSequenceQ(void);
    status_t         m_setAllocator(void *allocator);

    status_t         m_defaultAlloc(int bIndex, int eIndex, bool isMetaPlane);
    status_t         m_defaultFree(int bIndex, int eIndex, bool isMetaPlane);
    bool             m_checkInfoForAlloc(void);

    status_t         m_alloc(int bIndex, int eIndex);
    status_t         m_free(int bIndex, int eIndex);

    status_t         m_increase(int increaseCount);
    status_t         m_decrease(void);

    status_t         m_constructBufferContainer(int bIndex, int eIndex);
    status_t         m_destructBufferContainer(int bIndex, int eIndex);

private:
    struct ExynosCameraBuffer   m_swBuffer[SWBUFFER_MAX_COUNT];
    typedef ExynosCameraThread<SWExynosCameraBufferManager> allocThread;
    sp<allocThread>             m_allocationThread;
};

}
#endif
