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

#define LOG_TAG "ExynosCameraBufferManager"
#include "ExynosCameraBufferManager.h"

namespace android {

ExynosCameraBufferManager::ExynosCameraBufferManager()
{
    m_isDestructor = false;
    m_cameraId = 0;

    init();

    m_allocationThread = new allocThread(this, &ExynosCameraBufferManager::m_allocationThreadFunc, "allocationThreadFunc");
}

ExynosCameraBufferManager::~ExynosCameraBufferManager()
{
    m_isDestructor = true;
}

ExynosCameraBufferManager *ExynosCameraBufferManager::createBufferManager(buffer_manager_type_t type)
{
    switch (type) {
    case BUFFER_MANAGER_ION_TYPE:
        return (ExynosCameraBufferManager *)new InternalExynosCameraBufferManager();
        break;
    case BUFFER_MANAGER_SERVICE_GRALLOC_TYPE:
        return (ExynosCameraBufferManager *)new ServiceExynosCameraBufferManager(-1);
        break;
    case BUFFER_MANAGER_INVALID_TYPE:
        CLOGE2("Unknown bufferManager type(%d)", (int)type);
    default:
        break;
    }

    return NULL;
}

status_t ExynosCameraBufferManager::create(const char *name, int cameraId, void *defaultAllocator)
{
    Mutex::Autolock lock(m_lock);

    status_t ret = NO_ERROR;

    setCameraId(cameraId);
    strncpy(m_name, name, EXYNOS_CAMERA_NAME_STR_SIZE - 1);

    if (defaultAllocator == NULL) {
        if (m_createDefaultAllocator(false) != NO_ERROR) {
            CLOGE("m_createDefaultAllocator failed");
            return INVALID_OPERATION;
        }
    } else {
        if (m_setDefaultAllocator(defaultAllocator) != NO_ERROR) {
            CLOGE("m_setDefaultAllocator failed");
            return INVALID_OPERATION;
        }
    }

    return ret;
}

status_t ExynosCameraBufferManager::create(const char *name, void *defaultAllocator)
{
    return create(name, 0, defaultAllocator);
}

void ExynosCameraBufferManager::init(void)
{
    EXYNOS_CAMERA_BUFFER_IN();

    m_flagAllocated = false;
    m_reservedMemoryCount = 0;
    m_reqBufCount  = 0;
    m_allocatedBufCount  = 0;
    m_allowedMaxBufCount = 0;
    m_defaultAllocator = NULL;
    m_isCreateDefaultAllocator = false;
    for (int bufIndex = 0; bufIndex < VIDEO_MAX_FRAME; bufIndex++) {
        for (int planeIndex = 0; planeIndex < EXYNOS_CAMERA_BUFFER_MAX_PLANES; planeIndex++) {
            m_buffer[bufIndex].fd[planeIndex] = -1;
        }
    }
    m_hasMetaPlane = false;
    memset(m_name, 0x00, sizeof(m_name));
    strncpy(m_name, "none", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_flagSkipAllocation = false;
    m_flagNeedMmap = false;
    m_allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
    m_indexOffset = 0;

    EXYNOS_CAMERA_BUFFER_OUT();
}

void ExynosCameraBufferManager::deinit(void)
{
    if (m_flagAllocated == false) {
        CLOGI("OUT.. Buffer is not allocated");
        return;
    }

    CLOGD("IN..");

    if (m_allocMode == BUFFER_MANAGER_ALLOCATION_SILENT) {
        m_allocationThread->join();
        CLOGI("allocationThread is finished");
    }

    for (int bufIndex = 0; bufIndex < m_allocatedBufCount; bufIndex++)
        cancelBuffer(bufIndex);

    if (m_free() != NO_ERROR)
        CLOGE("free failed");

    if (m_defaultAllocator != NULL && m_isCreateDefaultAllocator == true) {
        delete m_defaultAllocator;
        m_defaultAllocator = NULL;
    }

    m_reservedMemoryCount = 0;
    m_flagSkipAllocation = false;
    CLOGD("OUT..");
}

status_t ExynosCameraBufferManager::resetBuffers(void)
{
    /* same as deinit */
    /* clear buffers except releasing the memory */
    status_t ret = NO_ERROR;

    if (m_flagAllocated == false) {
        CLOGI("OUT.. Buffer is not allocated");
        return ret;
    }

    CLOGD("IN..");

    if (m_allocMode == BUFFER_MANAGER_ALLOCATION_SILENT) {
        m_allocationThread->join();
        CLOGI("allocationThread is finished");
    }

    for (int bufIndex = m_indexOffset; bufIndex < m_allocatedBufCount + m_indexOffset; bufIndex++)
        cancelBuffer(bufIndex);

    m_resetSequenceQ();
    m_flagSkipAllocation = true;

    return ret;
}

status_t ExynosCameraBufferManager::setAllocator(void *allocator)
{
    Mutex::Autolock lock(m_lock);

    if (allocator == NULL) {
        CLOGE("m_allocator equals NULL");
        return INVALID_OPERATION;
    }

    return m_setAllocator(allocator);
}

status_t ExynosCameraBufferManager::alloc(void)
{
    EXYNOS_CAMERA_BUFFER_IN();
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    Mutex::Autolock lock(m_lock);

    status_t ret = NO_ERROR;

    if (m_flagSkipAllocation == true) {
        CLOGI("skip to allocate memory (m_flagSkipAllocation=%d)", (int)m_flagSkipAllocation);
        goto func_exit;
    }

    if (m_checkInfoForAlloc() == false) {
        CLOGE("m_checkInfoForAlloc failed");
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (m_hasMetaPlane == true) {
        if (m_defaultAlloc(m_indexOffset, m_reqBufCount + m_indexOffset, m_hasMetaPlane) != NO_ERROR) {
            CLOGE("m_defaultAlloc failed");
            ret = INVALID_OPERATION;
            goto func_exit;
        }
    }

    /* allocate image buffer */
    if (m_alloc(m_indexOffset, m_reqBufCount + m_indexOffset) != NO_ERROR) {
        CLOGE("m_alloc failed");

        if (m_hasMetaPlane == true) {
            CLOGD("Free metadata plane. bufferCount %d", m_reqBufCount);
            if (m_defaultFree(m_indexOffset, m_reqBufCount + m_indexOffset, m_hasMetaPlane) != NO_ERROR) {
                CLOGE("m_defaultFree failed");
            }
        }

        ret = INVALID_OPERATION;
        goto func_exit;
    }

    m_allocatedBufCount = m_reqBufCount;
    m_resetSequenceQ();
    m_flagAllocated = true;

    CLOGD("Allocate the buffer succeeded "
          "(m_allocatedBufCount=%d, m_reqBufCount=%d, m_allowedMaxBufCount=%d) --- dumpBufferInfo ---",
        m_allocatedBufCount, m_reqBufCount, m_allowedMaxBufCount);
    dumpBufferInfo();
    CLOGD("------------------------------------------------------------------");

    if (m_allocMode == BUFFER_MANAGER_ALLOCATION_SILENT) {
        /* run the allocationThread */
        m_allocationThread->run(PRIORITY_DEFAULT);
        CLOGI("allocationThread is started");
    }

func_exit:

    m_flagSkipAllocation = false;
    EXYNOS_CAMERA_BUFFER_OUT();

    return ret;
}

status_t ExynosCameraBufferManager::m_free(void)
{
    EXYNOS_CAMERA_BUFFER_IN();
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    Mutex::Autolock lock(m_lock);

    CLOGD("Free the buffer (m_allocatedBufCount=%d) --- dumpBufferInfo ---", m_allocatedBufCount);
    dumpBufferInfo();
    CLOGD("------------------------------------------------------");

    status_t ret = NO_ERROR;

    if (m_flagAllocated != false) {
        if (m_free(m_indexOffset, m_allocatedBufCount + m_indexOffset) != NO_ERROR) {
            CLOGE("m_free failed");
            ret = INVALID_OPERATION;
            goto func_exit;
        }

        if (m_hasMetaPlane == true) {
            if (m_defaultFree(m_indexOffset, m_allocatedBufCount + m_indexOffset, m_hasMetaPlane) != NO_ERROR) {
                CLOGE("m_defaultFree failed");
                ret = INVALID_OPERATION;
                goto func_exit;
            }
        }
        m_availableBufferIndexQLock.lock();
        m_availableBufferIndexQ.clear();
        m_availableBufferIndexQLock.unlock();
        m_allocatedBufCount  = 0;
        m_allowedMaxBufCount = 0;
        m_flagAllocated = false;
    }

    CLOGD("Free the buffer succeeded (m_allocatedBufCount=%d)", m_allocatedBufCount);

func_exit:

    EXYNOS_CAMERA_BUFFER_OUT();

    return ret;
}

void ExynosCameraBufferManager::m_resetSequenceQ()
{
    Mutex::Autolock lock(m_availableBufferIndexQLock);
    m_availableBufferIndexQ.clear();

    for (int bufIndex = m_indexOffset; bufIndex < m_allocatedBufCount + m_indexOffset; bufIndex++)
        m_availableBufferIndexQ.push_back(m_buffer[bufIndex].index);

    return;
}

void ExynosCameraBufferManager::setContigBufCount(int reservedMemoryCount)
{
    CLOGI("reservedMemoryCount(%d)", reservedMemoryCount);
    m_reservedMemoryCount = reservedMemoryCount;
    return;
}

int ExynosCameraBufferManager::getContigBufCount(void)
{
    return m_reservedMemoryCount;
}

/*  If Image buffer color format equals YV12, and buffer has MetaDataPlane..

    planeCount = 4      (set by user)
    size[0] : Image buffer plane Y size
    size[1] : Image buffer plane Cr size
    size[2] : Image buffer plane Cb size

    if (createMetaPlane == true)
        size[3] = EXYNOS_CAMERA_META_PLANE_SIZE;    (set by BufferManager, internally)
*/
status_t ExynosCameraBufferManager::setInfo(buffer_manager_configuration_t info)
{
    EXYNOS_CAMERA_BUFFER_IN();
    Mutex::Autolock lock(m_lock);

    status_t ret = NO_ERROR;
    int totalPlaneCount = 0;

    if (m_indexOffset > 0) {
        CLOGD("buffer indexOffset(%d), Index[0 - %d] not used", m_indexOffset, m_indexOffset);
    }
    m_indexOffset = info.startBufIndex;

    if (info.allowedMaxBufCount < info.reqBufCount) {
        CLOGW("abnormal value [reqBufCount=%d, allowedMaxBufCount=%d]",
                info.reqBufCount, info.allowedMaxBufCount);
        info.allowedMaxBufCount = info.reqBufCount;
    }

    if (info.reqBufCount < 0 || VIDEO_MAX_FRAME < info.reqBufCount) {
        CLOGE("abnormal value [reqBufCount=%d]", info.reqBufCount);
        ret = BAD_VALUE;
        goto func_exit;
    }

    if (info.planeCount < 0 || EXYNOS_CAMERA_BUFFER_MAX_PLANES <= info.planeCount) {
        CLOGE("abnormal value [planeCount=%d]", info.planeCount);
        ret = BAD_VALUE;
        goto func_exit;
    }

    totalPlaneCount = m_getTotalPlaneCount(info.planeCount, info.batchSize, info.createMetaPlane);
    if (totalPlaneCount < 1 || EXYNOS_CAMERA_BUFFER_MAX_PLANES < totalPlaneCount) {
        CLOGE("Failed to getTotalPlaneCount." \
                "totalPlaneCount %d planeCount %d batchSize %d hasMetaPlane %d",
                totalPlaneCount, info.planeCount, info.batchSize, info.createMetaPlane);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (info.createMetaPlane == true) {
        info.size[info.planeCount - 1] = EXYNOS_CAMERA_META_PLANE_SIZE;
        m_hasMetaPlane = true;
    }

    for (int bufIndex = m_indexOffset; bufIndex < info.allowedMaxBufCount + m_indexOffset; bufIndex++) {
        for (int planeIndex = 0; planeIndex < info.planeCount; planeIndex++) {
            if (info.size[planeIndex] <= 0) {
                CLOGE("abnormal value [size=%d, planeIndex %d]",
                        info.size[planeIndex], planeIndex);
                ret = BAD_VALUE;
                goto func_exit;
            }

            int curPlaneIndex = ((info.createMetaPlane == true && planeIndex == info.planeCount -1) ?
                                (totalPlaneCount - 1) : planeIndex);

            m_buffer[bufIndex].size[curPlaneIndex]         = info.size[planeIndex];
            m_buffer[bufIndex].bytesPerLine[curPlaneIndex] = info.bytesPerLine[planeIndex];
        }

        /* Copy image plane imformation into other planes in batch buffer */
        if (info.batchSize > 1) {
            int imagePlaneCount = (m_hasMetaPlane == true) ? (info.planeCount - 1) : info.planeCount;
            for (int batchIndex = 1; batchIndex < info.batchSize; batchIndex++) {
                for (int planeIndex = 0; planeIndex < imagePlaneCount; planeIndex++) {
                    int curPlaneIndex = (batchIndex * imagePlaneCount) + planeIndex;
                    m_buffer[bufIndex].size[curPlaneIndex]          = info.size[planeIndex];
                    m_buffer[bufIndex].bytesPerLine[curPlaneIndex]  = info.bytesPerLine[planeIndex];
                }
            }
        }

        m_buffer[bufIndex].planeCount = info.planeCount;
        m_buffer[bufIndex].type       = info.type;
        m_buffer[bufIndex].batchSize  = info.batchSize;
    }

    m_allowedMaxBufCount    = info.allowedMaxBufCount + info.startBufIndex;
    m_reqBufCount           = info.reqBufCount;
    m_flagNeedMmap          = info.needMmap;
    m_allocMode             = info.allocMode;
    m_reservedMemoryCount   = info.reservedMemoryCount;
func_exit:

    EXYNOS_CAMERA_BUFFER_OUT();

    return ret;
}

bool ExynosCameraBufferManager::m_allocationThreadFunc(void)
{
    status_t ret = NO_ERROR;
    int increaseCount = 1;

    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    CLOGI("increase buffer silently - start - "
          "(m_allowedMaxBufCount=%d, m_allocatedBufCount=%d, m_reqBufCount=%d)",
        m_allowedMaxBufCount, m_allocatedBufCount, m_reqBufCount);

    increaseCount = m_allowedMaxBufCount - m_reqBufCount;

    /* increase buffer*/
    for (int count = 0; count < increaseCount; count++) {
        ret = m_increase(1);
        if (ret < 0) {
            CLOGE("increase the buffer failed");
        } else {
            m_lock.lock();
            m_availableBufferIndexQ.push_back(m_buffer[m_allocatedBufCount + m_indexOffset].index);
            m_allocatedBufCount++;
            m_lock.unlock();
        }

    }
    dumpBufferInfo();
    CLOGI("increase buffer silently - end - (increaseCount=%d)"
          "(m_allowedMaxBufCount=%d, m_allocatedBufCount=%d, m_reqBufCount=%d)",
         increaseCount, m_allowedMaxBufCount, m_allocatedBufCount, m_reqBufCount);

    /* false : Thread run once */
    return false;
}

status_t ExynosCameraBufferManager::putBuffer(
        int bufIndex,
        enum EXYNOS_CAMERA_BUFFER_POSITION position)
{
    EXYNOS_CAMERA_BUFFER_IN();
    Mutex::Autolock lock(m_lock);

    status_t ret = NO_ERROR;
    List<int>::iterator r;
    bool found = false;
    enum EXYNOS_CAMERA_BUFFER_PERMISSION permission;

    permission = EXYNOS_CAMERA_BUFFER_PERMISSION_AVAILABLE;

    if (bufIndex < 0 || m_allocatedBufCount + m_indexOffset <= bufIndex) {
        CLOGE("buffer Index in out of bound [bufIndex=%d], allocatedBufCount(%d)",
             bufIndex, m_allocatedBufCount);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    m_availableBufferIndexQLock.lock();
    for (r = m_availableBufferIndexQ.begin(); r != m_availableBufferIndexQ.end(); r++) {
        if (bufIndex == *r) {
            found = true;
            break;
        }
    }
    m_availableBufferIndexQLock.unlock();

    if (found == true) {
        CLOGV("bufIndex=%d is already in (available state)", bufIndex);
        goto func_exit;
    }

    if (updateStatus(bufIndex, 0, position, permission) != NO_ERROR) {
        CLOGE("setStatus failed [bufIndex=%d, position=%d, permission=%d]",
             bufIndex, (int)position, (int)permission);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    m_availableBufferIndexQLock.lock();
    m_availableBufferIndexQ.push_back(m_buffer[bufIndex].index);
    m_availableBufferIndexQLock.unlock();

func_exit:

    EXYNOS_CAMERA_BUFFER_OUT();

    return ret;
}

/* User Process need to check the index of buffer returned from "getBuffer()" */
status_t ExynosCameraBufferManager::getBuffer(
        int  *reqBufIndex,
        enum EXYNOS_CAMERA_BUFFER_POSITION position,
        struct ExynosCameraBuffer *buffer)
{
    EXYNOS_CAMERA_BUFFER_IN();
    Mutex::Autolock lock(m_lock);

    status_t ret = NO_ERROR;
    List<int>::iterator r;

    int  bufferIndex;
    enum EXYNOS_CAMERA_BUFFER_PERMISSION permission;

    bufferIndex = *reqBufIndex;
    permission = EXYNOS_CAMERA_BUFFER_PERMISSION_NONE;

    if (m_allocatedBufCount == 0) {
        CLOGE("m_allocatedBufCount equals zero");
        ret = INVALID_OPERATION;
        goto func_exit;
    }

reDo:
    if (bufferIndex < 0 || m_allocatedBufCount + m_indexOffset <= bufferIndex) {
        /* find availableBuffer */
        m_availableBufferIndexQLock.lock();
        if (m_availableBufferIndexQ.empty() == false) {
            r = m_availableBufferIndexQ.begin();
            bufferIndex = *r;
            m_availableBufferIndexQ.erase(r);
#ifdef EXYNOS_CAMERA_BUFFER_TRACE
            CLOGI("available buffer [index=%d]...", bufferIndex);
#endif
        }
        m_availableBufferIndexQLock.unlock();
    } else {
        m_availableBufferIndexQLock.lock();
        /* get the Buffer of requested */
        for (r = m_availableBufferIndexQ.begin(); r != m_availableBufferIndexQ.end(); r++) {
            if (bufferIndex == *r) {
                m_availableBufferIndexQ.erase(r);
                break;
            }
        }
        m_availableBufferIndexQLock.unlock();
    }

    if (0 <= bufferIndex && bufferIndex < m_allocatedBufCount + m_indexOffset) {
        /* found buffer */
        if (isAvaliable(bufferIndex) == false) {
            CLOGE("isAvaliable failed [bufferIndex=%d]", bufferIndex);
            ret = BAD_VALUE;
            goto func_exit;
        }

        permission = EXYNOS_CAMERA_BUFFER_PERMISSION_IN_PROCESS;

        if (updateStatus(bufferIndex, 0, position, permission) != NO_ERROR) {
            CLOGE("setStatus failed [bIndex=%d, position=%d, permission=%d]",
                 bufferIndex, (int)position, (int)permission);
            ret = INVALID_OPERATION;
            goto func_exit;
        }
    } else {
        /* do not find buffer */
        if (m_allocMode == BUFFER_MANAGER_ALLOCATION_ONDEMAND) {
            /* increase buffer*/
            ret = m_increase(1);
            if (ret < 0) {
                CLOGE("increase the buffer failed, m_allocatedBufCount %d, bufferIndex %d",
                      m_allocatedBufCount, bufferIndex);
            } else {
                m_availableBufferIndexQLock.lock();
                m_availableBufferIndexQ.push_back(m_allocatedBufCount + m_indexOffset);
                m_availableBufferIndexQLock.unlock();
                bufferIndex = m_allocatedBufCount + m_indexOffset;
                m_allocatedBufCount++;

                dumpBufferInfo();
                CLOGI("increase the buffer succeeded (bufferIndex=%d)", bufferIndex);
                goto reDo;
            }
        } else {
            if (m_allocatedBufCount == 1)
                bufferIndex = 0;
            else
                ret = INVALID_OPERATION;
        }

        if (ret < 0) {
#ifdef EXYNOS_CAMERA_BUFFER_TRACE
            CLOGD("find free buffer... failed --- dump ---");
            dump();
            CLOGD("----------------------------------------");
            CLOGD("buffer Index in out of bound [bufferIndex=%d]", bufferIndex);
#endif
            ret = BAD_VALUE;
            goto func_exit;
        }
    }

    m_buffer[bufferIndex].index = bufferIndex;
    *reqBufIndex = bufferIndex;
    *buffer      = m_buffer[bufferIndex];

func_exit:

    EXYNOS_CAMERA_BUFFER_OUT();

    return ret;
}

status_t ExynosCameraBufferManager::increase(int increaseCount)
{
    CLOGD("increaseCount(%d) function invalid. Do nothing", increaseCount);
    return NO_ERROR;
}

status_t ExynosCameraBufferManager::cancelBuffer(int bufIndex)
{
    return putBuffer(bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
}

int ExynosCameraBufferManager::getBufStride(void)
{
    return 0;
}

status_t ExynosCameraBufferManager::updateStatus(
        int bufIndex,
        int driverValue,
        enum EXYNOS_CAMERA_BUFFER_POSITION   position,
        enum EXYNOS_CAMERA_BUFFER_PERMISSION permission)
{
    if (bufIndex < 0) {
        CLOGE("Invalid buffer index %d", bufIndex);
        return BAD_VALUE;
    }

    m_buffer[bufIndex].index = bufIndex;
    m_buffer[bufIndex].status.driverReturnValue = driverValue;
    m_buffer[bufIndex].status.position          = position;
    m_buffer[bufIndex].status.permission        = permission;

    return NO_ERROR;
}

status_t ExynosCameraBufferManager::getStatus(
        int bufIndex,
        struct ExynosCameraBufferStatus *bufStatus)
{
    *bufStatus = m_buffer[bufIndex].status;

    return NO_ERROR;
}

status_t ExynosCameraBufferManager::getIndexByFd(int fd, int *index)
{
    if (fd < 0) {
        CLOGE("Invalid FD %d", fd);
        return BAD_VALUE;
    }

    *index = -1;
    for (int bufIndex = m_indexOffset; bufIndex < m_reqBufCount + m_indexOffset; bufIndex++) {
        if (m_buffer[bufIndex].fd[0] == fd) {
            *index = bufIndex;
            break;
        }
    }

    if (*index < 0 || *index > m_allowedMaxBufCount + m_indexOffset) {
        CLOGE("Invalid buffer index %d. fd %d", *index, fd);

        *index = -1;
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

bool ExynosCameraBufferManager::isAllocated(void)
{
    return m_flagAllocated;
}

bool ExynosCameraBufferManager::isAvaliable(int bufIndex)
{
    bool ret = false;

    switch (m_buffer[bufIndex].status.permission) {
    case EXYNOS_CAMERA_BUFFER_PERMISSION_NONE:
    case EXYNOS_CAMERA_BUFFER_PERMISSION_AVAILABLE:
        ret = true;
        break;

    case EXYNOS_CAMERA_BUFFER_PERMISSION_IN_PROCESS:
    default:
#ifdef EXYNOS_CAMERA_BUFFER_TRACE
        CLOGD("buffer is not available");
        dump();
#endif
        ret = false;
        break;
    }

    return ret;
}

status_t ExynosCameraBufferManager::m_setDefaultAllocator(void *allocator)
{
    m_defaultAllocator = (ExynosCameraIonAllocator *)allocator;

    return NO_ERROR;
}

status_t ExynosCameraBufferManager::m_defaultAlloc(int bIndex, int eIndex, bool isMetaPlane)
{
    EXYNOS_CAMERA_BUFFER_IN();

    status_t ret = NO_ERROR;
    int planeIndexStart = 0;
    int planeIndexEnd   = 0;
    bool mapNeeded      = false;
#ifdef DEBUG_RAWDUMP
    char enableRawDump[PROP_VALUE_MAX];
#endif /* DEBUG_RAWDUMP */

    int mask  = EXYNOS_CAMERA_BUFFER_ION_MASK_NONCACHED;
    int flags = EXYNOS_CAMERA_BUFFER_ION_FLAG_NONCACHED;

    ExynosCameraDurationTimer timer;
    long long    durationTime = 0;
    long long    durationTimeSum = 0;
    unsigned int estimatedBase = EXYNOS_CAMERA_BUFFER_ION_WARNING_TIME_NONCACHED;
    unsigned int estimatedTime = 0;
    unsigned int bufferSize = 0;
    int          reservedMaxCount = 0;
    int          bufIndex = 0;

    if (m_defaultAllocator == NULL) {
        CLOGE("m_defaultAllocator equals NULL");
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (bIndex < 0 || eIndex < 0) {
        CLOGE("Invalid index parameters. bIndex %d eIndex %d", bIndex, eIndex);
        ret = BAD_VALUE;
        goto func_exit;
    }

    if (isMetaPlane == true) {
        mapNeeded = true;
    } else {
#ifdef DEBUG_RAWDUMP
        mapNeeded = true;
#else
        mapNeeded = m_flagNeedMmap;
#endif
    }

    for (bufIndex = bIndex; bufIndex < eIndex; bufIndex++) {
        if (isMetaPlane == true) {
            planeIndexStart = m_buffer[bufIndex].getMetaPlaneIndex();
            planeIndexEnd   = planeIndexStart + 1;
            mask  = EXYNOS_CAMERA_BUFFER_ION_MASK_NONCACHED;
            flags = EXYNOS_CAMERA_BUFFER_ION_FLAG_NONCACHED;
            estimatedBase = EXYNOS_CAMERA_BUFFER_ION_WARNING_TIME_NONCACHED;
        } else {
            planeIndexStart = 0;
            planeIndexEnd   = m_buffer[bufIndex].getMetaPlaneIndex();
            switch (m_buffer[bufIndex].type) {
            case EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE:
                mask  = EXYNOS_CAMERA_BUFFER_ION_MASK_NONCACHED;
                flags = EXYNOS_CAMERA_BUFFER_ION_FLAG_NONCACHED;
                estimatedBase = EXYNOS_CAMERA_BUFFER_ION_WARNING_TIME_NONCACHED;
                break;
            case EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE:
                mask  = EXYNOS_CAMERA_BUFFER_ION_MASK_CACHED;
                flags = EXYNOS_CAMERA_BUFFER_ION_FLAG_CACHED;
                estimatedBase = EXYNOS_CAMERA_BUFFER_ION_WARNING_TIME_CACHED;
                break;
            case EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE:
            /* case EXYNOS_CAMERA_BUFFER_ION_NONCACHED_RESERVED_TYPE: */
#ifdef RESERVED_MEMORY_ENABLE
                reservedMaxCount = (m_reservedMemoryCount > 0 ? m_reservedMemoryCount : RESERVED_BUFFER_COUNT_MAX);
#else
                reservedMaxCount = 0;
#endif
                if (bufIndex < reservedMaxCount) {
                    CLOGI("bufIndex(%d) < reservedMaxCount(%d) , m_reservedMemoryCount(%d), non-cached",
                        bufIndex, reservedMaxCount, m_reservedMemoryCount);
                    mask  = EXYNOS_CAMERA_BUFFER_ION_MASK_RESERVED;
                    flags = EXYNOS_CAMERA_BUFFER_ION_FLAG_RESERVED;
                    estimatedBase = EXYNOS_CAMERA_BUFFER_ION_WARNING_TIME_RESERVED;
                } else {
                    CLOGI("bufIndex(%d) >= reservedMaxCount(%d) , m_reservedMemoryCount(%d),"
                        "non-cached. so, alloc ion memory instead of reserved memory",
                        bufIndex, reservedMaxCount, m_reservedMemoryCount);
                    mask  = EXYNOS_CAMERA_BUFFER_ION_MASK_NONCACHED;
                    flags = EXYNOS_CAMERA_BUFFER_ION_FLAG_NONCACHED;
                    estimatedBase = EXYNOS_CAMERA_BUFFER_ION_WARNING_TIME_NONCACHED;
                }
                break;
            case EXYNOS_CAMERA_BUFFER_ION_CACHED_RESERVED_TYPE:
#ifdef RESERVED_MEMORY_ENABLE
                reservedMaxCount = (m_reservedMemoryCount > 0 ? m_reservedMemoryCount : RESERVED_BUFFER_COUNT_MAX);
#else
                reservedMaxCount = 0;
#endif
                if (bufIndex < reservedMaxCount) {
                    CLOGI("bufIndex(%d) < reservedMaxCount(%d) , m_reservedMemoryCount(%d), cached",
                        bufIndex, reservedMaxCount, m_reservedMemoryCount);

                    mask  = EXYNOS_CAMERA_BUFFER_ION_MASK_RESERVED;
                    flags = EXYNOS_CAMERA_BUFFER_ION_FLAG_RESERVED | EXYNOS_CAMERA_BUFFER_ION_FLAG_CACHED;
                    estimatedBase = EXYNOS_CAMERA_BUFFER_ION_WARNING_TIME_RESERVED;
                } else {
                    CLOGI("bufIndex(%d) >= reservedMaxCount(%d) , m_reservedMemoryCount(%d),"
                        "cached. so, alloc ion memory instead of reserved memory",
                        bufIndex, reservedMaxCount, m_reservedMemoryCount);

                    mask  = EXYNOS_CAMERA_BUFFER_ION_MASK_CACHED;
                    flags = EXYNOS_CAMERA_BUFFER_ION_FLAG_CACHED;
                    estimatedBase = EXYNOS_CAMERA_BUFFER_ION_WARNING_TIME_CACHED;
                }
                break;
            case EXYNOS_CAMERA_BUFFER_ION_CACHED_SYNC_FORCE_TYPE:
                ALOGD("SYNC_FORCE_CACHED");
                mask  = EXYNOS_CAMERA_BUFFER_ION_MASK_CACHED;
                flags = EXYNOS_CAMERA_BUFFER_ION_FLAG_CACHED_SYNC_FORCE;
                estimatedBase = EXYNOS_CAMERA_BUFFER_ION_WARNING_TIME_CACHED;
                break;
            case EXYNOS_CAMERA_BUFFER_ION_RESERVED_SECURE_TYPE:
#ifdef RESERVED_MEMORY_ENABLE
                reservedMaxCount = (m_reservedMemoryCount > 0 ? m_reservedMemoryCount : RESERVED_BUFFER_COUNT_MAX);
#else
                reservedMaxCount = 0;
#endif
                if (bufIndex < reservedMaxCount) {
                    CLOGI("bufIndex(%d) < reservedMaxCount(%d) , m_reservedMemoryCount(%d), non-cached",
                        bufIndex, reservedMaxCount, m_reservedMemoryCount);
                    mask  = EXYNOS_CAMERA_BUFFER_ION_MASK_SECURE;
                    flags = EXYNOS_CAMERA_BUFFER_ION_FLAG_SECURE;
                    estimatedBase = EXYNOS_CAMERA_BUFFER_ION_WARNING_TIME_RESERVED;
                } else {
                    CLOGI("bufIndex(%d) >= reservedMaxCount(%d) , m_reservedMemoryCount(%d),"
                        "non-cached. so, alloc ion memory instead of reserved memory",
                        bufIndex, reservedMaxCount, m_reservedMemoryCount);
                    mask  = EXYNOS_CAMERA_BUFFER_ION_MASK_NONCACHED;
                    flags = EXYNOS_CAMERA_BUFFER_ION_MASK_NONCACHED;
                    estimatedBase = EXYNOS_CAMERA_BUFFER_ION_WARNING_TIME_NONCACHED;
                }
                break;
            case EXYNOS_CAMERA_BUFFER_ION_CACHED_RESERVED_SECURE_TYPE:
#ifdef RESERVED_MEMORY_ENABLE
                reservedMaxCount = (m_reservedMemoryCount > 0 ? m_reservedMemoryCount : RESERVED_BUFFER_COUNT_MAX);
#else
                reservedMaxCount = 0;
#endif
                if (bufIndex < reservedMaxCount) {
                    CLOGI("bufIndex(%d) < reservedMaxCount(%d) , m_reservedMemoryCount(%d), cached",
                        bufIndex, reservedMaxCount, m_reservedMemoryCount);

                    mask  = EXYNOS_CAMERA_BUFFER_ION_MASK_SECURE;
                    flags = EXYNOS_CAMERA_BUFFER_ION_FLAG_SECURE | EXYNOS_CAMERA_BUFFER_ION_FLAG_CACHED;
                    estimatedBase = EXYNOS_CAMERA_BUFFER_ION_WARNING_TIME_RESERVED;
                } else {
                    CLOGI("bufIndex(%d) >= reservedMaxCount(%d) , m_reservedMemoryCount(%d),"
                        "cached. so, alloc ion memory instead of reserved memory",
                        bufIndex, reservedMaxCount, m_reservedMemoryCount);

                    mask  = EXYNOS_CAMERA_BUFFER_ION_MASK_CACHED;
                    flags = EXYNOS_CAMERA_BUFFER_ION_FLAG_CACHED;
                    estimatedBase = EXYNOS_CAMERA_BUFFER_ION_WARNING_TIME_CACHED;
                }
                break;
            case EXYNOS_CAMERA_BUFFER_INVALID_TYPE:
            default:
                CLOGE("buffer type is invaild (%d)", (int)m_buffer[bufIndex].type);
                break;
            }
        }

        if (isMetaPlane == false) {
            timer.start();
            bufferSize = 0;
        }

        for (int planeIndex = planeIndexStart; planeIndex < planeIndexEnd; planeIndex++) {
            if (m_buffer[bufIndex].fd[planeIndex] >= 0) {
                CLOGE("buffer[%d].fd[%d] = %d already allocated",
                         bufIndex, planeIndex, m_buffer[bufIndex].fd[planeIndex]);
                continue;
            }

            if (m_defaultAllocator->alloc(
                    m_buffer[bufIndex].size[planeIndex],
                    &(m_buffer[bufIndex].fd[planeIndex]),
                    &(m_buffer[bufIndex].addr[planeIndex]),
                    mask,
                    flags,
                    mapNeeded) != NO_ERROR) {
#if defined(RESERVED_MEMORY_ENABLE) && defined(RESERVED_MEMORY_REALLOC_WITH_ION)
                if (m_buffer[bufIndex].type == EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE) {
                    CLOGE("Realloc with ion:bufIndex(%d), m_reservedMemoryCount(%d),"
                        " non-cached. so, alloc ion memory instead of reserved memory",
                        bufIndex, m_reservedMemoryCount);

                    mask  = EXYNOS_CAMERA_BUFFER_ION_MASK_NONCACHED;
                    flags = EXYNOS_CAMERA_BUFFER_ION_FLAG_NONCACHED;
                    estimatedBase = EXYNOS_CAMERA_BUFFER_ION_WARNING_TIME_NONCACHED;
                } else if (m_buffer[bufIndex].type == EXYNOS_CAMERA_BUFFER_ION_CACHED_RESERVED_TYPE) {
                    CLOGE("Realloc with ion:bufIndex(%d), m_reservedMemoryCount(%d),"
                        " cached. so, alloc ion memory instead of reserved memory",
                        bufIndex, m_reservedMemoryCount);

                    mask  = EXYNOS_CAMERA_BUFFER_ION_MASK_CACHED;
                    flags = EXYNOS_CAMERA_BUFFER_ION_FLAG_CACHED;
                    estimatedBase = EXYNOS_CAMERA_BUFFER_ION_WARNING_TIME_CACHED;
                } else {
                    CLOGE("m_defaultAllocator->alloc(bufIndex=%d, planeIndex=%d, size=%d) failed",
                        bufIndex, planeIndex, m_buffer[bufIndex].size[planeIndex]);

                    ret = INVALID_OPERATION;
                    goto func_exit;                   
                }

                if (m_defaultAllocator->alloc(
                        m_buffer[bufIndex].size[planeIndex],
                        &(m_buffer[bufIndex].fd[planeIndex]),
                        &(m_buffer[bufIndex].addr[planeIndex]),
                        mask,
                        flags,
                        mapNeeded) != NO_ERROR) {
                    CLOGE("m_defaultAllocator->alloc(bufIndex=%d, planeIndex=%d, size=%d) failed",
                        bufIndex, planeIndex, m_buffer[bufIndex].size[planeIndex]);
                    ret = INVALID_OPERATION;
                    goto func_exit;
                }

#else
                CLOGE("m_defaultAllocator->alloc(bufIndex=%d, planeIndex=%d, size=%d) failed",
                    bufIndex, planeIndex, m_buffer[bufIndex].size[planeIndex]);

                ret = INVALID_OPERATION;
                goto func_exit;
#endif
            }

#ifdef EXYNOS_CAMERA_BUFFER_TRACE
            printBufferInfo(__FUNCTION__, __LINE__, bufIndex, planeIndex);
#endif
            if (isMetaPlane == false) {
                bufferSize = bufferSize + m_buffer[bufIndex].size[planeIndex];
            }
        }

        if (isMetaPlane == false) {
            timer.stop();
            durationTime = timer.durationMsecs();
            durationTimeSum += durationTime;
            CLOGD("duration time(%5d msec):(type=%d, bufIndex=%d, size=%.2f, batchSize=%d)",
                 (int)durationTime, m_buffer[bufIndex].type, bufIndex,
                 (float)bufferSize / (float)(1024 * 1024), m_buffer[bufIndex].batchSize);

            estimatedTime = estimatedBase * bufferSize / EXYNOS_CAMERA_BUFFER_1MB;
            if (estimatedTime < durationTime) {
                CLOGW("estimated time(%5d msec):(type=%d, bufIndex=%d, size=%d)",
                     (int)estimatedTime, m_buffer[bufIndex].type, bufIndex, (int)bufferSize);
            }
        }

        if (updateStatus(
                bufIndex,
                0,
                EXYNOS_CAMERA_BUFFER_POSITION_NONE,
                EXYNOS_CAMERA_BUFFER_PERMISSION_AVAILABLE) != NO_ERROR) {
            CLOGE("setStatus failed [bIndex=%d, position=NONE, permission=NONE]",
                 bufIndex);
            ret = INVALID_OPERATION;
            goto func_exit;
        }
    }

    if ((planeIndexEnd - planeIndexStart) == 1) {
        CLOGD("Duration time of buffer(Plane:%d) allocation(%5d msec)", planeIndexStart, (int)durationTimeSum);
    } else if ((planeIndexEnd - planeIndexStart) > 1) {
        CLOGD("Duration time of buffer(Plane:%d~%d) allocation(%5d msec)",planeIndexStart, (planeIndexEnd - 1), (int)durationTimeSum);
    }

    EXYNOS_CAMERA_BUFFER_OUT();

    return ret;

func_exit:
    EXYNOS_CAMERA_BUFFER_OUT();

    if (bufIndex < eIndex) {
        if (m_defaultFree(0, bufIndex, isMetaPlane) != NO_ERROR) {
            CLOGE("m_defaultFree failed");
        }
    }
    return ret;
}

status_t ExynosCameraBufferManager::m_defaultFree(int bIndex, int eIndex, bool isMetaPlane)
{
    EXYNOS_CAMERA_BUFFER_IN();

    status_t ret = NO_ERROR;
    int planeIndexStart = 0;
    int planeIndexEnd   = 0;
    bool mapNeeded      = false;
#ifdef DEBUG_RAWDUMP
    char enableRawDump[PROP_VALUE_MAX];
#endif /* DEBUG_RAWDUMP */

    if (isMetaPlane == true) {
        mapNeeded = true;
    } else {
#ifdef DEBUG_RAWDUMP
        mapNeeded = true;
#else
        mapNeeded = m_flagNeedMmap;
#endif
    }

    for (int bufIndex = bIndex; bufIndex < eIndex; bufIndex++) {
        if (isAvaliable(bufIndex) == false) {
            CLOGE("buffer [bufIndex=%d] in InProcess state", bufIndex);
            if (m_isDestructor == false) {
                ret = BAD_VALUE;
                continue;
            } else {
                CLOGE("buffer [bufIndex=%d] in InProcess state, but try to forcedly free", bufIndex);
            }
        }

        if (isMetaPlane == true) {
            planeIndexStart = m_buffer[bufIndex].getMetaPlaneIndex();
            planeIndexEnd   = planeIndexStart + 1;
        } else {
            planeIndexStart = 0;
            planeIndexEnd   = m_buffer[bufIndex].getMetaPlaneIndex();
        }

        for (int planeIndex = planeIndexStart; planeIndex < planeIndexEnd; planeIndex++) {
            if (m_defaultAllocator->free(
                    m_buffer[bufIndex].size[planeIndex],
                    &(m_buffer[bufIndex].fd[planeIndex]),
                    &(m_buffer[bufIndex].addr[planeIndex]),
                    mapNeeded) != NO_ERROR) {
                CLOGE("m_defaultAllocator->free for Imagedata Plane failed." \
                        "bufIndex %d planeIndex %d fd %d addr %p",
                    bufIndex, planeIndex,
                    m_buffer[bufIndex].fd[planeIndex],
                    m_buffer[bufIndex].addr[planeIndex]);
                ret = INVALID_OPERATION;
                goto func_exit;
            }
        }

        if (updateStatus(
                bufIndex,
                0,
                EXYNOS_CAMERA_BUFFER_POSITION_NONE,
                EXYNOS_CAMERA_BUFFER_PERMISSION_NONE) != NO_ERROR) {
            CLOGE("setStatus failed [bIndex=%d, position=NONE, permission=NONE]", bufIndex);
            ret = INVALID_OPERATION;
            goto func_exit;
        }
    }

func_exit:

    EXYNOS_CAMERA_BUFFER_OUT();

    return ret;
}

bool ExynosCameraBufferManager::m_checkInfoForAlloc(void)
{
    EXYNOS_CAMERA_BUFFER_IN();

    bool ret = true;

    if (m_reqBufCount < 0 || VIDEO_MAX_FRAME < m_reqBufCount) {
        CLOGE("buffer Count in out of bound [m_reqBufCount=%d]", m_reqBufCount);
        ret = false;
        goto func_exit;
    }

    for (int bufIndex = m_indexOffset; bufIndex < m_reqBufCount + m_indexOffset; bufIndex++) {
        if (m_buffer[bufIndex].planeCount < 0
         || VIDEO_MAX_PLANES <= m_buffer[bufIndex].planeCount) {
            CLOGE("plane Count in out of bound [m_buffer[bIndex].planeCount=%d]",
                 m_buffer[bufIndex].planeCount);
            ret = false;
            goto func_exit;
        }

        for (int planeIndex = 0; planeIndex < m_buffer[bufIndex].planeCount; planeIndex++) {
            int curPlaneIndex = ((m_hasMetaPlane == true && planeIndex == m_buffer[bufIndex].planeCount -1) ?
                                m_buffer[bufIndex].getMetaPlaneIndex() : planeIndex);

            if (m_buffer[bufIndex].size[curPlaneIndex] == 0) {
                CLOGE("size is empty [m_buffer[%d].size[%d]=%d]",
                    bufIndex, curPlaneIndex, m_buffer[bufIndex].size[curPlaneIndex]);
                ret = false;
                goto func_exit;
            }
        }
    }

func_exit:

    EXYNOS_CAMERA_BUFFER_OUT();

    return ret;
}

status_t ExynosCameraBufferManager::m_createDefaultAllocator(bool isCached)
{
    EXYNOS_CAMERA_BUFFER_IN();

    status_t ret = NO_ERROR;

    m_defaultAllocator = new ExynosCameraIonAllocator();
    m_isCreateDefaultAllocator = true;
    if (m_defaultAllocator->init(isCached) != NO_ERROR) {
        CLOGE("m_defaultAllocator->init failed");
        ret = INVALID_OPERATION;
        goto func_exit;
    }

func_exit:

    EXYNOS_CAMERA_BUFFER_OUT();

    return ret;
}

int ExynosCameraBufferManager::m_getTotalPlaneCount(int planeCount, int batchSize, bool hasMetaPlane)
{
    if (planeCount < 1 || batchSize < 1) {
        CLOGE("Invalid values. planeCount %d batchSize %d", planeCount, batchSize);
        return 0;
    }

    int totalPlaneCount = 0;
    if (hasMetaPlane == true) {
        int imagePlaneCount = planeCount - 1;
        totalPlaneCount = (imagePlaneCount * batchSize) + 1;
    } else {
        totalPlaneCount = planeCount * batchSize;
    }

    return totalPlaneCount;
}

int ExynosCameraBufferManager::getAllocatedBufferCount(void)
{
    return m_allocatedBufCount;
}

int ExynosCameraBufferManager::getAvailableIncreaseBufferCount(void)
{
    CLOGI("this function only applied to ONDEMAND mode (%d)", m_allocMode);
    int numAvailable = 0;

    if (m_allocMode == BUFFER_MANAGER_ALLOCATION_ONDEMAND)
        numAvailable += (m_allowedMaxBufCount - m_allocatedBufCount);

    CLOGI("m_allowedMaxBufCount(%d), m_allocatedBufCount(%d), ret(%d)",
         m_allowedMaxBufCount, m_allocatedBufCount, numAvailable);
    return numAvailable;
}

int ExynosCameraBufferManager::getNumOfAvailableBuffer(void)
{
    int numAvailable = 0;

    for (int i = m_indexOffset; i < m_allocatedBufCount + m_indexOffset; i++) {
        if (m_buffer[i].status.permission == EXYNOS_CAMERA_BUFFER_PERMISSION_AVAILABLE)
            numAvailable++;
    }

    if (m_allocMode == BUFFER_MANAGER_ALLOCATION_ONDEMAND)
        numAvailable += (m_allowedMaxBufCount - m_allocatedBufCount);

    return numAvailable;
}

int ExynosCameraBufferManager::getNumOfAvailableAndNoneBuffer(void)
{
    int numAvailable = 0;

    for (int i = m_indexOffset; i < m_allocatedBufCount + m_indexOffset; i++) {
        if (m_buffer[i].status.permission == EXYNOS_CAMERA_BUFFER_PERMISSION_AVAILABLE ||
            m_buffer[i].status.permission == EXYNOS_CAMERA_BUFFER_PERMISSION_NONE)
            numAvailable++;
    }

    return numAvailable;
}

void ExynosCameraBufferManager::printBufferState(void)
{
    for (int i = m_indexOffset; i < m_allocatedBufCount + m_indexOffset; i++) {
        CLOGV("m_buffer[%d].fd[0]=%d, position=%d, permission=%d]",
            i, m_buffer[i].fd[0],
            m_buffer[i].status.position, m_buffer[i].status.permission);
    }

    return;
}

void ExynosCameraBufferManager::printBufferState(int bufIndex, int planeIndex)
{
    CLOGI("m_buffer[%d].fd[%d]=%d, .status.permission=%d]",
        bufIndex, planeIndex, m_buffer[bufIndex].fd[planeIndex],
        m_buffer[bufIndex].status.permission);

    return;
}

void ExynosCameraBufferManager::printBufferQState()
{
    List<int>::iterator r;
    int  bufferIndex;

    Mutex::Autolock lock(m_availableBufferIndexQLock);

    for (r = m_availableBufferIndexQ.begin(); r != m_availableBufferIndexQ.end(); r++) {
        bufferIndex = *r;
        CLOGV("bufferIndex=%d", bufferIndex);
    }

    return;
}

void ExynosCameraBufferManager::printBufferInfo(
        __unused const char *funcName,
        __unused const int lineNum,
        int bufIndex,
        int planeIndex)
{
    CLOGI("[m_buffer[%d].fd[%d]=%d] .addr=%p .size=%d]",
        bufIndex, planeIndex,
        m_buffer[bufIndex].fd[planeIndex],
        m_buffer[bufIndex].addr[planeIndex],
        m_buffer[bufIndex].size[planeIndex]);

    return;
}

void ExynosCameraBufferManager::dump(void)
{
    CLOGD("----- dump buffer status -----");
    printBufferState();
    printBufferQState();

    return;
}

void ExynosCameraBufferManager::dumpBufferInfo(void)
{
#ifdef EXYNOS_CAMERA_DUMP_BUFFER_INFO
    for (int bufIndex = m_indexOffset; bufIndex < m_allocatedBufCount + m_indexOffset; bufIndex++) {
        int totalPlaneCount = m_getTotalPlaneCount(m_buffer[bufIndex].planeCount, m_buffer[bufIndex].batchSize, m_hasMetaPlane);
        for (int planeIndex = 0; planeIndex < totalPlaneCount; planeIndex++) {
            CLOGI("[m_buffer[%d].fd[%d]=%d] .addr=%p .size=%d .position=%d .permission=%d]",
                    m_buffer[bufIndex].index, planeIndex,
                    m_buffer[bufIndex].fd[planeIndex],
                    m_buffer[bufIndex].addr[planeIndex],
                    m_buffer[bufIndex].size[planeIndex],
                    m_buffer[bufIndex].status.position,
                    m_buffer[bufIndex].status.permission);
        }
    }
    printBufferQState();
#endif

    return;
}

status_t ExynosCameraBufferManager::setBufferCount(__unused int bufferCount)
{
    CLOGD("");

    return NO_ERROR;
}

int ExynosCameraBufferManager::getBufferCount(void)
{
    CLOGV("");

    return 0;
}

InternalExynosCameraBufferManager::InternalExynosCameraBufferManager()
{
    ExynosCameraBufferManager::init();
}

InternalExynosCameraBufferManager::~InternalExynosCameraBufferManager()
{
    ExynosCameraBufferManager::deinit();
}

status_t InternalExynosCameraBufferManager::m_setAllocator(void *allocator)
{
    return m_setDefaultAllocator(allocator);
}

status_t InternalExynosCameraBufferManager::m_alloc(int bIndex, int eIndex)
{
    status_t ret = NO_ERROR;

    ret = m_defaultAlloc(bIndex, eIndex, false);
    if (ret != NO_ERROR) {
        CLOGE("Failed to alloc. bIndex %d eIndex %d ret %d",
                bIndex, eIndex, ret);
        return ret;
    }

    if (m_buffer[0].batchSize > 1) {
        ret = m_constructBufferContainer(bIndex, eIndex);
        if (ret != NO_ERROR) {
            CLOGE("Failed to constructBufferContainer. bIndex %d eIndex %d ret %d",
                    bIndex, eIndex, ret);
            return ret;
        }
    }

    return NO_ERROR;
}

status_t InternalExynosCameraBufferManager::m_free(int bIndex, int eIndex)
{
    status_t ret = NO_ERROR;

    if (m_buffer[0].batchSize > 1) {
        ret = m_destructBufferContainer(bIndex, eIndex);
        if (ret != NO_ERROR) {
            CLOGE("Failed to destructBufferContainer. bIndex %d eIndex %d ret %d",
                    bIndex, eIndex, ret);
            return ret;
        }
    } else {
        ret = m_defaultFree(bIndex, eIndex, false);
        if (ret != NO_ERROR) {
            CLOGE("Failed to free. bIndex %d eIndex %d ret %d",
                    bIndex, eIndex, ret);
            return ret;
        }
    }

    return NO_ERROR;
}

status_t InternalExynosCameraBufferManager::m_increase(int increaseCount)
{
    CLOGD("IN..");
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = NO_ERROR;

    if (m_allowedMaxBufCount <= m_allocatedBufCount) {
        CLOGD("BufferManager can't increase the buffer "
              "(m_reqBufCount=%d, m_allowedMaxBufCount=%d <= m_allocatedBufCount=%d)",
            m_reqBufCount, m_allowedMaxBufCount, m_allocatedBufCount);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (m_allowedMaxBufCount < m_allocatedBufCount + increaseCount) {
        CLOGI("change the increaseCount (%d->%d) --- "
              "(m_reqBufCount=%d, m_allowedMaxBufCount=%d <= m_allocatedBufCount=%d + increaseCount=%d)",
             increaseCount, m_allowedMaxBufCount - m_allocatedBufCount,
            m_reqBufCount, m_allowedMaxBufCount, m_allocatedBufCount, increaseCount);
        increaseCount = m_allowedMaxBufCount - m_allocatedBufCount;
    }

    /* set the buffer information */
    for (int bufIndex = m_allocatedBufCount + m_indexOffset; bufIndex < m_allocatedBufCount + m_indexOffset + increaseCount; bufIndex++) {
        for (int planeIndex = 0; planeIndex < m_buffer[0].planeCount; planeIndex++) {
            if (m_buffer[0].size[planeIndex] == 0) {
                CLOGE("abnormal value [size=%d]",
                     m_buffer[0].size[planeIndex]);
                ret = BAD_VALUE;
                goto func_exit;
            }
            m_buffer[bufIndex].size[planeIndex]         = m_buffer[0].size[planeIndex];
            m_buffer[bufIndex].bytesPerLine[planeIndex] = m_buffer[0].bytesPerLine[planeIndex];
        }
        m_buffer[bufIndex].planeCount = m_buffer[0].planeCount;
        m_buffer[bufIndex].type       = m_buffer[0].type;
    }

    if (m_alloc(m_allocatedBufCount + m_indexOffset, m_allocatedBufCount + m_indexOffset + increaseCount) != NO_ERROR) {
        CLOGE("m_alloc failed");
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (m_hasMetaPlane == true) {
        if (m_defaultAlloc(m_allocatedBufCount + m_indexOffset, m_allocatedBufCount + m_indexOffset + increaseCount, m_hasMetaPlane) != NO_ERROR) {
            CLOGE("m_defaultAlloc failed");
            ret = INVALID_OPERATION;
            goto func_exit;
        }
    }

    CLOGD("Increase the buffer succeeded (m_allocatedBufCount=%d, increaseCount=%d)",
         m_allocatedBufCount + m_indexOffset, increaseCount);

func_exit:

    CLOGD("OUT..");

    return ret;
}

status_t InternalExynosCameraBufferManager::m_decrease(void)
{
    CLOGD("IN..");
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = true;
    List<int>::iterator r;

    int  bufferIndex = -1;

    if (m_allocatedBufCount <= m_reqBufCount) {
        CLOGD("BufferManager can't decrease the buffer "
              "(m_allowedMaxBufCount=%d, m_allocatedBufCount=%d <= m_reqBufCount=%d)",
            m_allowedMaxBufCount, m_allocatedBufCount, m_reqBufCount);
        ret = INVALID_OPERATION;
        goto func_exit;
    }
    bufferIndex = m_allocatedBufCount;

    if (m_free(bufferIndex-1 + m_indexOffset, bufferIndex + m_indexOffset) != NO_ERROR) {
        CLOGE("m_free failed");
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (m_hasMetaPlane == true) {
        if (m_defaultFree(bufferIndex-1 + m_indexOffset, bufferIndex + m_indexOffset, m_hasMetaPlane) != NO_ERROR) {
            CLOGE("m_defaultFree failed");
            ret = INVALID_OPERATION;
            goto func_exit;
        }
    }

    m_availableBufferIndexQLock.lock();
    for (r = m_availableBufferIndexQ.begin(); r != m_availableBufferIndexQ.end(); r++) {
        if ((bufferIndex + m_indexOffset) == *r) {
            m_availableBufferIndexQ.erase(r);
            break;
        }
    }
    m_availableBufferIndexQLock.unlock();
    m_allocatedBufCount--;

    CLOGD("Decrease the buffer succeeded (m_allocatedBufCount=%d)" ,
         m_allocatedBufCount);

func_exit:

    CLOGD("OUT..");

    return ret;
}

status_t InternalExynosCameraBufferManager::m_constructBufferContainer(int bIndex, int eIndex)
{
    EXYNOS_CAMERA_BUFFER_IN();

    status_t ret = NO_ERROR;
    ExynosCameraDurationTimer timer;

    if (m_defaultAllocator == NULL) {
        CLOGE("defaultAllocator is NULL");
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (bIndex < 0 || eIndex < 0) {
        CLOGE("Invalid parameters. bIndex %d eIndex %d",
                bIndex, eIndex);
        ret = BAD_VALUE;
        goto func_exit;
    }

    for (int bufIndex = bIndex; bufIndex < eIndex; bufIndex++) {
        /* Get buffer container FD for same size planes */
        int imagePlaneCount = (m_hasMetaPlane == true) ?
                            (m_buffer[bufIndex].planeCount - 1)
                            : m_buffer[bufIndex].planeCount;

        timer.start();
        for (int planeIndex = 0; planeIndex < imagePlaneCount; planeIndex++) {
            /* Gather single FDs */
            int fds[EXYNOS_CAMERA_BUFFER_MAX_PLANES];
            for (int batchIndex = 0; batchIndex < m_buffer[bufIndex].batchSize; batchIndex++) {
                fds[batchIndex] = m_buffer[bufIndex].fd[(batchIndex * imagePlaneCount) + planeIndex];
            }

            ret = m_defaultAllocator->createBufferContainer(fds,
                                                            m_buffer[bufIndex].batchSize,
                                                            &m_buffer[bufIndex].containerFd[planeIndex]);
            if (ret != NO_ERROR) {
                CLOGE("[B%d P%d]Failed to createBufferContainer. batchSize %d ret %d",
                        bufIndex, planeIndex, m_buffer[bufIndex].batchSize, ret);
                ret = INVALID_OPERATION;
                goto func_exit;
            }

            /* Release single buffer FD.
             * Buffer container will have reference for each single buffer.
             */
            for (int batchIndex = 0; batchIndex < m_buffer[bufIndex].batchSize; batchIndex++) {
                int curPlaneIndex = (batchIndex * imagePlaneCount) + planeIndex;
                ret = m_defaultAllocator->free(m_buffer[bufIndex].size[curPlaneIndex],
                                            &(m_buffer[bufIndex].fd[curPlaneIndex]),
                                            &(m_buffer[bufIndex].addr[curPlaneIndex]),
                                            m_flagNeedMmap);
                if (ret != NO_ERROR) {
                    CLOGE("[B%d P%d FD%d]Failed to free. ret %d",
                            bufIndex, planeIndex, m_buffer[bufIndex].fd[curPlaneIndex], ret);
                    /* continue */
                }
            }
        }

        timer.stop();
        CLOGD("duration time(%5d msec):(bufIndex=%d, batchSize=%d, fd=%d/%d/%d)",
                (int)timer.durationMsecs(), bufIndex, m_buffer[bufIndex].batchSize,
                m_buffer[bufIndex].containerFd[0], m_buffer[bufIndex].containerFd[1], m_buffer[bufIndex].containerFd[2]);
    }

func_exit:
    EXYNOS_CAMERA_BUFFER_OUT();

    return ret;
}

status_t InternalExynosCameraBufferManager::m_destructBufferContainer(int bIndex, int eIndex)
{
    EXYNOS_CAMERA_BUFFER_IN();

    status_t ret = NO_ERROR;
    char *dummyAddr = NULL;

    if (m_defaultAllocator == NULL) {
        CLOGE("defaultAllocator is NULL");
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (bIndex < 0 || eIndex < 0) {
        CLOGE("Invalid parameters. bIndex %d eIndex %d",
                bIndex, eIndex);
        ret = BAD_VALUE;
        goto func_exit;
    }

    for (int bufIndex = bIndex; bufIndex < eIndex; bufIndex++) {
        int imagePlaneCount = (m_hasMetaPlane == true) ?
                              (m_buffer[bufIndex].planeCount - 1)
                              : m_buffer[bufIndex].planeCount;

        for (int planeIndex = 0; planeIndex < imagePlaneCount; planeIndex++) {
            ret = m_defaultAllocator->free(m_buffer[bufIndex].size[planeIndex],
                                           &(m_buffer[bufIndex].containerFd[planeIndex]),
                                           &dummyAddr, /*mapNeeded */false);
            if (ret != NO_ERROR) {
                CLOGE("[B%d P%d FD%d]Failed to free containerFd. ret %d",
                        bufIndex, planeIndex, m_buffer[bufIndex].containerFd[planeIndex], ret);
                /* continue */
            }
        }
    }

func_exit:
    EXYNOS_CAMERA_BUFFER_OUT();

    return ret;
}

status_t InternalExynosCameraBufferManager::increase(int increaseCount)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    Mutex::Autolock lock(m_lock);
    status_t ret = NO_ERROR;

    CLOGI("m_allocatedBufCount(%d), m_allowedMaxBufCount(%d), increaseCount(%d)",
         m_allocatedBufCount, m_allowedMaxBufCount, increaseCount);

    /* increase buffer*/
    ret = m_increase(increaseCount);
    if (ret < 0) {
        CLOGE("increase the buffer failed, m_allocatedBufCount(%d), m_allowedMaxBufCount(%d), increaseCount(%d)",
              m_allocatedBufCount, m_allowedMaxBufCount, increaseCount);
    } else {
        for (int bufferIndex = m_allocatedBufCount + m_indexOffset; bufferIndex < m_allocatedBufCount + m_indexOffset + increaseCount; bufferIndex++) {
            m_availableBufferIndexQLock.lock();
            m_availableBufferIndexQ.push_back(bufferIndex);
            m_availableBufferIndexQLock.unlock();
        }
        m_allocatedBufCount += increaseCount;

        dumpBufferInfo();
        CLOGI("increase the buffer succeeded (increaseCount(%d))",
             increaseCount);
    }

    return ret;
}

ExynosCameraFence::ExynosCameraFence()
{
    m_fenceType = EXYNOS_CAMERA_FENCE_TYPE_BASE;

    m_acquireFence = -1;
    m_releaseFence = -1;

    m_fence = 0;

    m_flagSwfence = false;
}

ExynosCameraFence::ExynosCameraFence(
        enum EXYNOS_CAMERA_FENCE_TYPE fenceType,
        int acquireFence,
        int releaseFence)
{
    /* default setting */
    m_fenceType = EXYNOS_CAMERA_FENCE_TYPE_BASE;
    m_acquireFence = -1;
    m_releaseFence = -1;
    m_fence = 0;
    m_flagSwfence = false;

    /* we will set from here */
    if (fenceType <= EXYNOS_CAMERA_FENCE_TYPE_BASE ||
            EXYNOS_CAMERA_FENCE_TYPE_MAX <= fenceType) {
        ALOGE("ERR(%s[%d]):Invalid fenceType(%d)",
                __FUNCTION__, __LINE__, fenceType);
        return;
    }

    m_fenceType = fenceType;
    m_acquireFence = acquireFence;
    m_releaseFence = releaseFence;

    if (0 <= m_acquireFence || 0 <= m_releaseFence) {
        ALOGV("DEBUG(%s[%d]):m_acquireFence(%d), m_releaseFence(%d)",
                __FUNCTION__, __LINE__, m_acquireFence, m_releaseFence);
    }

#ifdef USE_SW_FENCE
    m_flagSwfence = true;
#endif
    if (m_flagSwfence == true) {
        switch (m_fenceType) {
        case EXYNOS_CAMERA_FENCE_TYPE_ACQUIRE:
            m_fence = new Fence(acquireFence);
            break;
        case EXYNOS_CAMERA_FENCE_TYPE_RELEASE:
            m_fence = new Fence(releaseFence);
            break;
        default:
            ALOGE("ERR(%s[%d]):invalid m_fenceType(%d)",
                    __FUNCTION__, __LINE__, m_fenceType);
            break;
        }
    }
}

ExynosCameraFence::~ExynosCameraFence()
{
    /* delete sp<Fence> addr */
    m_fence = 0;
    static uint64_t closeCnt = 0;
    if(m_acquireFence >= ACQUIRE_FD_THRESHOLD) {
        if (closeCnt++ % 1000 == 0) {
            CLOGW2("Attempt to close acquireFence[%d], %ld th close.",
                     m_acquireFence, (long)closeCnt);
        }
    }
}

int ExynosCameraFence::getFenceType(void)
{
    return m_fenceType;
}

int ExynosCameraFence::getAcquireFence(void)
{
    return m_acquireFence;
}

int ExynosCameraFence::getReleaseFence(void)
{
    return m_releaseFence;
}

bool ExynosCameraFence::isValid(void)
{
    bool ret = false;

    if (m_flagSwfence == true) {
        if (m_fence == NULL) {
            CLOGE2("m_fence == NULL. so, fail");
            ret = false;
        } else {
            ret = m_fence->isValid();
        }
    } else {
        switch (m_fenceType) {
        case EXYNOS_CAMERA_FENCE_TYPE_ACQUIRE:
            if (0 <= m_acquireFence)
              ret = true;
            break;
        case EXYNOS_CAMERA_FENCE_TYPE_RELEASE:
            if (0 <= m_releaseFence)
              ret = true;
            break;
        default:
            CLOGE2("invalid m_fenceType(%d)",
                 m_fenceType);
            break;
        }
    }

    return ret;
}

status_t ExynosCameraFence::wait(int time)
{
    status_t ret = NO_ERROR;

    if (this->isValid() == false) {
        CLOGE2("this->isValid() == false. so, fail!! fencType(%d)",
                m_fenceType);
        return INVALID_OPERATION;
    }

    if (m_flagSwfence == false) {
        CLOGW2("m_flagSwfence == false. so, fail!! fencType(%d)",
                m_fenceType);

        return INVALID_OPERATION;
    }

    int waitTime = time;
    if (waitTime < 0)
        waitTime = 1000; /* wait 1 sec */

    int fenceFd = -1;

    switch (m_fenceType) {
    case EXYNOS_CAMERA_FENCE_TYPE_ACQUIRE:
        fenceFd = m_acquireFence;
        break;
    case EXYNOS_CAMERA_FENCE_TYPE_RELEASE:
        fenceFd = m_releaseFence;
        break;
    default:
        CLOGE2("invalid m_fenceType(%d)",
             m_fenceType);
        break;
    }

    ret = m_fence->wait(waitTime);
    if (ret == TIMED_OUT) {
        CLOGE2("Fence timeout. so, fail!! fenceFd(%d), fencType(%d)",
             fenceFd, m_fenceType);

        return INVALID_OPERATION;
    } else if (ret != OK) {
        CLOGE2("Fence wait error. so, fail!! fenceFd(%d), fencType(%d)",
             fenceFd, m_fenceType);

        return INVALID_OPERATION;
    }

    return ret;
}

ServiceExynosCameraBufferManager::ServiceExynosCameraBufferManager(int actualFormat)
{
    ExynosCameraBufferManager::init();

    m_allocator = new ExynosCameraStreamAllocator(actualFormat);

    CLOGD("");
    for (int bufIndex = 0; bufIndex < VIDEO_MAX_FRAME; bufIndex++) {
        m_handle[bufIndex] = NULL;
        m_handleIsLocked[bufIndex] = false;
    }
}

ServiceExynosCameraBufferManager::~ServiceExynosCameraBufferManager()
{
    if (m_allocator != NULL) {
        delete m_allocator;
        m_allocator = NULL;
    }

    ExynosCameraBufferManager::deinit();
}

status_t ServiceExynosCameraBufferManager::putBuffer(
        int bufIndex,
        enum EXYNOS_CAMERA_BUFFER_POSITION position)
{
    EXYNOS_CAMERA_BUFFER_IN();
    Mutex::Autolock lock(m_lock);

    status_t ret = NO_ERROR;
    List<int>::iterator r;
    bool found = false;
    int totalPlaneCount = 0;
    enum EXYNOS_CAMERA_BUFFER_PERMISSION permission;

    permission = EXYNOS_CAMERA_BUFFER_PERMISSION_AVAILABLE;

    if (bufIndex < 0 || m_allocatedBufCount + m_indexOffset <= bufIndex) {
        CLOGE("buffer Index in out of bound [bufIndex=%d], allocatedBufCount(%d)",
             bufIndex, m_allocatedBufCount);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    m_availableBufferIndexQLock.lock();
    for (r = m_availableBufferIndexQ.begin(); r != m_availableBufferIndexQ.end(); r++) {
        if (bufIndex == *r) {
            found = true;
            break;
        }
    }
    m_availableBufferIndexQLock.unlock();

    if (found == true) {
        CLOGV("bufIndex=%d is already in (available state)", bufIndex);
        goto func_exit;
    }

    /* Clear Image Plane Information */
    totalPlaneCount = m_getTotalPlaneCount(m_buffer[bufIndex].planeCount,
                                           m_buffer[bufIndex].batchSize,
                                           m_hasMetaPlane);
    totalPlaneCount -= (m_hasMetaPlane == true)? 1 : 0;

    for (int i = 0; i < totalPlaneCount; i++) {
        m_buffer[bufIndex].fd[i] = -1;
        m_buffer[bufIndex].addr[i] = NULL;
        m_buffer[bufIndex].handle[i] = NULL;
        m_buffer[bufIndex].acquireFence[i] = -1;
        m_buffer[bufIndex].releaseFence[i] = -1;
    }

    if (m_buffer[bufIndex].batchSize > 1) {
        ret = m_destructBufferContainer(bufIndex);
        if (ret != NO_ERROR) {
            CLOGE("[B%d]Failed to destructBufferContainer. ret %d", bufIndex, ret);
            /* continue */
        }
    }

    if (updateStatus(bufIndex, 0, position, permission) != NO_ERROR) {
        CLOGE("setStatus failed [bufIndex=%d, position=%d, permission=%d]",
             bufIndex, (int)position, (int)permission);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    m_availableBufferIndexQLock.lock();
    m_availableBufferIndexQ.push_back(m_buffer[bufIndex].index);
    m_availableBufferIndexQLock.unlock();

func_exit:

    EXYNOS_CAMERA_BUFFER_OUT();

    return ret;
}

status_t ServiceExynosCameraBufferManager::getBuffer(
        int  *reqBufIndex,
        enum EXYNOS_CAMERA_BUFFER_POSITION position,
        struct ExynosCameraBuffer *buffer)
{
    EXYNOS_CAMERA_BUFFER_IN();
    Mutex::Autolock lock(m_lock);

    status_t ret = NO_ERROR;
    List<int>::iterator r;

    int  bufferIndex;
    int planeCount;
    enum EXYNOS_CAMERA_BUFFER_PERMISSION permission;
    ExynosCameraFence* fence = NULL;

    bufferIndex = *reqBufIndex;
    permission = EXYNOS_CAMERA_BUFFER_PERMISSION_NONE;

    if (m_allocatedBufCount == 0) {
        CLOGE("m_allocatedBufCount equals zero");
        ret = INVALID_OPERATION;
        goto func_exit;
    }

reDo:
    if (bufferIndex < 0 || m_allocatedBufCount + m_indexOffset <= bufferIndex) {
        /* find availableBuffer */
        m_availableBufferIndexQLock.lock();
        if (m_availableBufferIndexQ.empty() == false) {
            r = m_availableBufferIndexQ.begin();
            bufferIndex = *r;
            m_availableBufferIndexQ.erase(r);
#ifdef EXYNOS_CAMERA_BUFFER_TRACE
            CLOGI("available buffer [index=%d]...", bufferIndex);
#endif
        }
        m_availableBufferIndexQLock.unlock();
    } else {
        m_availableBufferIndexQLock.lock();
        /* get the Buffer of requested */
        for (r = m_availableBufferIndexQ.begin(); r != m_availableBufferIndexQ.end(); r++) {
            if (bufferIndex == *r) {
                m_availableBufferIndexQ.erase(r);
                break;
            }
        }
        m_availableBufferIndexQLock.unlock();
    }

    if (0 <= bufferIndex && bufferIndex < m_allocatedBufCount + m_indexOffset) {
        /* found buffer */
        if (isAvaliable(bufferIndex) == false) {
            CLOGE("isAvaliable failed [bufferIndex=%d]", bufferIndex);
            ret = BAD_VALUE;
            goto func_exit;
        }

        permission = EXYNOS_CAMERA_BUFFER_PERMISSION_IN_PROCESS;

        if (updateStatus(bufferIndex, 0, position, permission) != NO_ERROR) {
            CLOGE("setStatus failed [bIndex=%d, position=%d, permission=%d]",
                 bufferIndex, (int)position, (int)permission);
            ret = INVALID_OPERATION;
            goto func_exit;
        }
    } else {
        /* do not find buffer */
        if (m_allocMode == BUFFER_MANAGER_ALLOCATION_ONDEMAND) {
            /* increase buffer*/
            ret = m_increase(1);
            if (ret < 0) {
                CLOGE("increase the buffer failed, m_allocatedBufCount %d, bufferIndex %d",
                      m_allocatedBufCount, bufferIndex);
            } else {
                m_availableBufferIndexQLock.lock();
                m_availableBufferIndexQ.push_back(m_allocatedBufCount + m_indexOffset);
                m_availableBufferIndexQLock.unlock();
                bufferIndex = m_allocatedBufCount + m_indexOffset;
                m_allocatedBufCount++;

                dumpBufferInfo();
                CLOGI("increase the buffer succeeded (bufferIndex=%d)", bufferIndex);
                goto reDo;
            }
        } else {
            ret = INVALID_OPERATION;
        }

        if (ret < 0) {
#ifdef EXYNOS_CAMERA_BUFFER_TRACE
            CLOGD("find free buffer... failed --- dump ---");
            dump();
            CLOGD("----------------------------------------");
            CLOGD("buffer Index in out of bound [bufferIndex=%d]", bufferIndex);
#endif
            ret = BAD_VALUE;
            goto func_exit;
        }
    }

    m_buffer[bufferIndex].index = bufferIndex;

    for (int batchIndex = 0; batchIndex < m_buffer[bufferIndex].batchSize; batchIndex++) {
        if (buffer->handle[batchIndex] == NULL) {
            CLOGW("[B%d-%d]Handle is NULL. Skip it", bufferIndex, batchIndex);
            continue;
        }

        fence = new ExynosCameraFence(
                ExynosCameraFence::EXYNOS_CAMERA_FENCE_TYPE_ACQUIRE,
                buffer->acquireFence[batchIndex],
                buffer->releaseFence[batchIndex]);

        /* Wait fence */
#ifdef USE_CAMERA2_USE_FENCE
        if (fence != NULL) {
#ifdef USE_SW_FENCE
            /* wait before give buffer to hardware */
            if (fence->isValid() == true) {
                ret = m_waitFence(fence);
                if (ret != NO_ERROR) {
                    CLOGE("[B%d-%d]m_waitFence() fail", bufferIndex, batchIndex);
                    goto loop_continue;
                }
            } else {
                CLOGV("[B%d-%d]Fence is invalid", bufferIndex, batchIndex);
            }
            /* Initialize fence */
            buffer->acquireFence[batchIndex] = -1;
            buffer->releaseFence[batchIndex] = -1;
#else
            /* give fence to H/W */
            buffer->acquireFence[batchIndex] = ptrFence->getAcquireFence();
            buffer->releaseFence[batchIndex] = ptrFence->getReleaseFence();
#endif
        }
#endif

        planeCount = (m_hasMetaPlane == true) ? m_buffer[bufferIndex].planeCount - 1
                                              : m_buffer[bufferIndex].planeCount;

        if (m_flagNeedMmap == true) {
            /* Get FD/VA from handle */
            ret = m_allocator->lock(
                    &(buffer->handle[batchIndex]),
                    &(m_buffer[bufferIndex].fd[batchIndex * planeCount]),
                    &(m_buffer[bufferIndex].addr[batchIndex * planeCount]),
                    &m_handleIsLocked[bufferIndex],
                    planeCount);
            if (ret != NO_ERROR) {
                CLOGE("[B%d]Failed to grallocHal->lock buffer_handle", bufferIndex);
                goto loop_continue;
            }

            /* Sync cache operation */
            ret = m_allocator->unlock(buffer->handle[batchIndex]);
            if (ret != NO_ERROR) {
                CLOGE("[B%d]Failed to grallocHal->unlock buffer_handle", bufferIndex);
                goto loop_continue;
            }
        } else {
            /* Get FD from handle */
            ret = m_getBufferInfoFromHandle(*(buffer->handle[batchIndex]),
                                            planeCount,
                                            &(m_buffer[bufferIndex].fd[batchIndex * planeCount]));
            if (ret != NO_ERROR) {
                CLOGE("[B%d]Failed to getBufferInfoFromHandle. ret %d", bufferIndex, ret);
                goto loop_continue;
            }
        }

        m_buffer[bufferIndex].handle[batchIndex] = buffer->handle[batchIndex];
        m_handleIsLocked[bufferIndex] = false;

loop_continue:
        if (fence != NULL) {
            delete fence;
            fence = NULL;
        }
    }

    if (m_buffer[bufferIndex].batchSize > 1) {
        ret = m_constructBufferContainer(bufferIndex);
        if (ret != NO_ERROR) {
            CLOGE("[B%d]Failed to constructBufferContainer. ret %d", bufferIndex, ret);
            /* continue */
        }
    }

    *reqBufIndex = bufferIndex;
    *buffer      = m_buffer[bufferIndex];

func_exit:

    EXYNOS_CAMERA_BUFFER_OUT();

    return ret;
}

status_t ServiceExynosCameraBufferManager::m_setAllocator(void *allocator)
{
    if (m_allocator == NULL) {
        CLOGE("m_allocator equals NULL");
        goto func_exit;
    }

    m_allocator->init((camera3_stream_t *)allocator);

func_exit:
    return NO_ERROR;
}

status_t ServiceExynosCameraBufferManager::m_alloc(int bIndex, int eIndex)
{
    EXYNOS_CAMERA_BUFFER_IN();

    status_t ret = OK;
    CLOGD("");

    for (int bufIndex = bIndex; bufIndex < eIndex; bufIndex++) {
        if (updateStatus(
                bufIndex,
                0,
                EXYNOS_CAMERA_BUFFER_POSITION_IN_SERVICE,
                EXYNOS_CAMERA_BUFFER_PERMISSION_NONE) != NO_ERROR) {
            CLOGE("setStatus failed [bufIndex=%d, position=SERVICE, permission=NONE]", bufIndex);
            ret = INVALID_OPERATION;
            break;
        }
    }

    /*
     * service buffer is given by service.
     * so, initial state is all un-available.
     */
    m_availableBufferIndexQLock.lock();
    m_availableBufferIndexQ.clear();
    m_availableBufferIndexQLock.unlock();

    EXYNOS_CAMERA_BUFFER_OUT();

    return ret;
}

status_t ServiceExynosCameraBufferManager::m_free(int bIndex, int eIndex)
{
    EXYNOS_CAMERA_BUFFER_IN();

    status_t ret = NO_ERROR;

    CLOGD("IN");
    dump();

    for (int bufIndex = bIndex; bufIndex < eIndex; bufIndex++) {
        if (m_handleIsLocked[bufIndex] == false) {
            CLOGV("buffer [bufIndex=%d] already free", bufIndex);
            continue;
        }

        m_handleIsLocked[bufIndex] = false;

        if (updateStatus(
                bufIndex,
                0,
                EXYNOS_CAMERA_BUFFER_POSITION_NONE,
                EXYNOS_CAMERA_BUFFER_PERMISSION_NONE) != NO_ERROR) {
            CLOGE("setStatus failed [bIndex=%d, position=NONE, permission=NONE]", bufIndex);
            ret = INVALID_OPERATION;
            goto func_exit;
        }
    }

func_exit:

    EXYNOS_CAMERA_BUFFER_OUT();

    return ret;
}

status_t ServiceExynosCameraBufferManager::m_increase(__unused int increaseCount)
{
    CLOGD("allocMode(%d) is invalid. Do nothing", m_allocMode);
    return INVALID_OPERATION;
}

status_t ServiceExynosCameraBufferManager::m_decrease(void)
{
    return INVALID_OPERATION;
}

status_t ServiceExynosCameraBufferManager::m_getBufferInfoFromHandle(buffer_handle_t handle,
                                                                     int planeCount,
                                                                     int fd[])
{
    int curPlaneCount = planeCount;

    if (handle->version != sizeof(native_handle_t)) {
        android_printAssert(NULL, LOG_TAG,
                            "ASSERT(%s):native_handle_t size mismatch. local %d != framework %d",
                            __FUNCTION__, (int)sizeof(native_handle_t), handle->version);

        return BAD_VALUE;
    }

    if (handle->numFds != planeCount) {
        curPlaneCount = (handle->numFds < planeCount)? handle->numFds : planeCount;
        CLOGW("planeCount mismatch. local %d != handle %d. Use %d.",
                planeCount, handle->numFds, curPlaneCount);
    }

    for (int i = 0; i < curPlaneCount; i++) {
        if (handle->data[i] < 0) {
            CLOGE("Invalid FD %d for plane %d",
                    handle->data[i], i);
            continue;
        }

        fd[i] = handle->data[i];
    }

    return NO_ERROR;
}

status_t ServiceExynosCameraBufferManager::m_waitFence(ExynosCameraFence *fence)
{
    status_t ret = NO_ERROR;

#if 0
    /* reference code */
    sp<Fence> bufferAcquireFence = new Fence(buffer->acquire_fence);
    ret = bufferAcquireFence->wait(1000); /* 1 sec */
    if (ret == TIMED_OUT) {
        CLOGE("Fence timeout(%d)!!", request->frame_number);
        return INVALID_OPERATION;
    } else if (ret != OK) {
        CLOGE("Waiting on Fence error(%d)!!", request->frame_number);
        return INVALID_OPERATION;
    }
#endif

    if (fence == NULL) {
        CLOGE("fence == NULL. so, fail");
        return INVALID_OPERATION;
    }

    if (fence->isValid() == false) {
        CLOGE("fence->isValid() == false. so, fail");
        return INVALID_OPERATION;
    }

    CLOGV("Valid fence");


    ret = fence->wait();
    if (ret != NO_ERROR) {
        CLOGE("fence->wait() fail");
        return INVALID_OPERATION;
    } else {
        CLOGV("fence->wait() succeed");
    }

    return ret;
}

status_t ServiceExynosCameraBufferManager::m_constructBufferContainer(int bufIndex)
{
    EXYNOS_CAMERA_BUFFER_IN();

    status_t ret = NO_ERROR;
    ExynosCameraDurationTimer timer;
    int imagePlaneCount = 0;

    if (m_defaultAllocator == NULL) {
        CLOGE("defaultAllocator is NULL");
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (bufIndex < 0) {
        CLOGE("Invalid parameters. bufIndex %d", bufIndex);
        ret = BAD_VALUE;
        goto func_exit;
    }

    /* Get buffer container FD for same size planes */
    imagePlaneCount = (m_hasMetaPlane == true) ? (m_buffer[bufIndex].planeCount - 1)
                                               : m_buffer[bufIndex].planeCount;

    timer.start();
    for (int planeIndex = 0; planeIndex < imagePlaneCount; planeIndex++) {
        /* Gather single FDs */
        int fds[EXYNOS_CAMERA_BUFFER_MAX_PLANES];
        for (int batchIndex = 0; batchIndex < m_buffer[bufIndex].batchSize; batchIndex++) {
            fds[batchIndex] = m_buffer[bufIndex].fd[(batchIndex * imagePlaneCount) + planeIndex];
        }

        ret = m_defaultAllocator->createBufferContainer(fds,
                m_buffer[bufIndex].batchSize,
                &m_buffer[bufIndex].containerFd[planeIndex]);
        if (ret != NO_ERROR) {
            CLOGE("[B%d P%d]Failed to createBufferContainer. batchSize %d ret %d",
                    bufIndex, planeIndex, m_buffer[bufIndex].batchSize, ret);
            ret = INVALID_OPERATION;
            goto func_exit;
        }
    }

    timer.stop();
    CLOGV("duration time(%5d msec):(bufIndex=%d, batchSize=%d, fd=%d/%d/%d)",
            (int)timer.durationMsecs(), bufIndex, m_buffer[bufIndex].batchSize,
            m_buffer[bufIndex].containerFd[0], m_buffer[bufIndex].containerFd[1], m_buffer[bufIndex].containerFd[2]);

func_exit:
    EXYNOS_CAMERA_BUFFER_OUT();

    return ret;
}

status_t ServiceExynosCameraBufferManager::m_destructBufferContainer(int bufIndex)
{
    EXYNOS_CAMERA_BUFFER_IN();

    status_t ret = NO_ERROR;
    int imagePlaneCount = 0;
    char* dummyAddr = NULL;

    if (m_defaultAllocator == NULL) {
        CLOGE("defaultAllocator is NULL");
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (bufIndex < 0) {
        CLOGE("Invalid parameters. bufIndex %d", bufIndex);
        ret = BAD_VALUE;
        goto func_exit;
    }

    imagePlaneCount = (m_hasMetaPlane == true) ? (m_buffer[bufIndex].planeCount - 1)
                                               : m_buffer[bufIndex].planeCount;

    for (int planeIndex = 0; planeIndex < imagePlaneCount; planeIndex++) {
        ret = m_defaultAllocator->free(m_buffer[bufIndex].size[planeIndex],
                                       &(m_buffer[bufIndex].containerFd[planeIndex]),
                                       &dummyAddr, /*mapNeeded */false);
        if (ret != NO_ERROR) {
            CLOGE("[B%d P%d FD%d]Failed to free containerFd. ret %d",
                    bufIndex, planeIndex, m_buffer[bufIndex].containerFd[planeIndex], ret);
            /* continue */
        }
    }

func_exit:
    EXYNOS_CAMERA_BUFFER_OUT();

    return ret;
}

} // namespace android
