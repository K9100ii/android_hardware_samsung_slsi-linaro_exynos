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

#define LOG_TAG "ExynosCameraBufferSupplier"
#include "ExynosCameraBufferSupplier.h"

namespace android {

ExynosCameraBufferSupplier::ExynosCameraBufferSupplier()
{
    m_cameraId = 0;
    snprintf(m_name, sizeof(m_name), "BufferSupplier");
}

ExynosCameraBufferSupplier::ExynosCameraBufferSupplier(int cameraId)
{
    setCameraId(cameraId);
    snprintf(m_name, sizeof(m_name), "BufferSupplier");
}

ExynosCameraBufferSupplier::~ExynosCameraBufferSupplier()
{
    m_deinit();
}

void ExynosCameraBufferSupplier::deinit(void)
{
    m_deinit();
}

void ExynosCameraBufferSupplier::deinit(const buffer_manager_tag_t tag)
{
    ExynosCameraBufferManager *bufferMgr = NULL;

    bufferMgr = m_getBufferManager(tag);
    if (bufferMgr == NULL) {
        CLOGE("[P%d T%d]Failed to getBufferManager",
                tag.pipeId[0], tag.managerType);

        return;
    }

    return bufferMgr->deinit();
}

status_t ExynosCameraBufferSupplier::createBufferManager(const char *name,
                                                         void *allocator,
                                                         const buffer_manager_tag_t tag,
                                                         void *stream,
                                                         int actualFormat)
{
    status_t ret = NO_ERROR;
    ExynosCameraBufferManager *newBufferMgr = NULL;

    switch (tag.managerType) {
    case BUFFER_MANAGER_ION_TYPE:
    case BUFFER_MANAGER_FASTEN_AE_ION_TYPE:
        newBufferMgr = (ExynosCameraBufferManager *)new InternalExynosCameraBufferManager();
        break;
    case BUFFER_MANAGER_SERVICE_GRALLOC_TYPE:
        newBufferMgr = (ExynosCameraBufferManager *)new ServiceExynosCameraBufferManager(actualFormat);
        break;
    case BUFFER_MANAGER_INVALID_TYPE:
    default:
        CLOGE("Unknown bufferManager type(%d)",
                tag.managerType);
        break;
    }

    if (newBufferMgr == NULL) {
        CLOGE("[%s][P%d T%d]Failed to new bufferManager.",
                name, tag.pipeId[0], tag.managerType);

        return INVALID_OPERATION;
    }

    ret = newBufferMgr->create(name, m_cameraId, allocator);
    if (ret != NO_ERROR) {
        CLOGE("[%s][P%d T%d]Failed to create bufferManager.",
                name, tag.pipeId[0], tag.managerType);

        return INVALID_OPERATION;
    }

    if (stream != NULL) {
        ret = newBufferMgr->setAllocator(stream);
        if (ret != NO_ERROR) {
            CLOGE("[%s][P%d T%d]Failed to setAllocator",
                    name, tag.pipeId[0], tag.managerType);

            return INVALID_OPERATION;
        }
    }

    {
        Mutex::Autolock lock(m_bufferMgrMapLock);
        m_bufferMgrMap.push_back(buffer_manager_map_item_t(tag, newBufferMgr));
    }

    CLOGI("[%s]Created. P %d,%d,%d,%d,%d, T %d",
            name,
            tag.pipeId[0], tag.pipeId[1], tag.pipeId[2], tag.pipeId[3], tag.pipeId[4],
            tag.managerType);

    return NO_ERROR;
}

status_t ExynosCameraBufferSupplier::resetBuffers(void)
{
    status_t funcRet = NO_ERROR;
    status_t ret = NO_ERROR;
    buffer_manager_tag_t bufTag;
    ExynosCameraBufferManager *bufferMgr = NULL;

    for (buffer_manager_map_iter_t iter = m_bufferMgrMap.begin(); iter != m_bufferMgrMap.end(); iter++) {
        bufferMgr = iter->second;
        ret = bufferMgr->resetBuffers();
        if (ret != NO_ERROR) {
            bufTag = iter->first;
            CLOGE("[P%d T%d]Failed to resetBuffer. ret %d",
                    bufTag.pipeId[0], bufTag.managerType, ret);
            funcRet |= ret;
        }
    }

    return funcRet;
}

status_t ExynosCameraBufferSupplier::resetBuffers(const buffer_manager_tag_t tag)
{
    ExynosCameraBufferManager *bufferMgr = NULL;

    bufferMgr = m_getBufferManager(tag);
    if (bufferMgr == NULL) {
        CLOGE("[P%d T%d]Failed to getBufferManager",
                tag.pipeId[0], tag.managerType);

        return INVALID_OPERATION;
    }

    return bufferMgr->resetBuffers();
}

status_t ExynosCameraBufferSupplier::setInfo(const buffer_manager_tag_t tag,
                                               const buffer_manager_configuration_t info)
{
    ExynosCameraBufferManager *bufferMgr = NULL;

    bufferMgr = m_getBufferManager(tag);
    if (bufferMgr == NULL) {
        CLOGE("[P%d T%d]Failed to getBufferManager",
                tag.pipeId[0], tag.managerType);

        return INVALID_OPERATION;
    }

    return bufferMgr->setInfo(info);
}

status_t ExynosCameraBufferSupplier::alloc(const buffer_manager_tag_t tag)
{
    ExynosCameraBufferManager * bufferMgr = NULL;

    bufferMgr = m_getBufferManager(tag);
    if (bufferMgr == NULL) {
        CLOGE("[P%d T%d]Failed to getBufferManager",
                tag.pipeId[0], tag.managerType);

        return INVALID_OPERATION;
    }

    return bufferMgr->alloc();
}

status_t ExynosCameraBufferSupplier::getBuffer(const buffer_manager_tag_t tag, ExynosCameraBuffer *buffer)
{
    status_t ret = NO_ERROR;
    ExynosCameraBufferManager *bufferMgr = NULL;
    int bufIndex = -1;

    bufferMgr = m_getBufferManager(tag);
    if (bufferMgr == NULL) {
        CLOGE("[P%d T%d]Failed to getBufferManager",
                tag.pipeId[0], tag.managerType);

        return INVALID_OPERATION;
    }

    ret = bufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, buffer);
    if (ret != NO_ERROR
        || bufIndex < 0
        || buffer == NULL) {
        CLOGE("[P%d T%d B%d A%p]Failed to getBuffer. ret %d",
                tag.pipeId[0], tag.managerType, bufIndex, buffer, ret);

        buffer = NULL;
        return INVALID_OPERATION;
    }

    buffer->tag = tag;

    return NO_ERROR;
}

status_t ExynosCameraBufferSupplier::putBuffer(ExynosCameraBuffer buffer)
{
    ExynosCameraBufferManager *bufferMgr = NULL;

    bufferMgr = m_getBufferManager(buffer.tag);
    if (bufferMgr == NULL) {
        CLOGE("[P%d T%d]Failed to getBufferManager",
                buffer.tag.pipeId[0], buffer.tag.managerType);

        return INVALID_OPERATION;
    }

    return bufferMgr->putBuffer(buffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
}

int ExynosCameraBufferSupplier::getNumOfAvailableBuffer(const buffer_manager_tag_t tag)
{
    ExynosCameraBufferManager *bufferMgr = NULL;

    bufferMgr = m_getBufferManager(tag);
    if (bufferMgr == NULL) {
        CLOGE("[P%d T%d]Failed to getBufferManager",
                tag.pipeId[0], tag.managerType);

        return INVALID_OPERATION;
    }

    return bufferMgr->getNumOfAvailableBuffer();
}

void ExynosCameraBufferSupplier::dump(void)
{
    ExynosCameraBufferManager *bufferMgr = NULL;

    for (buffer_manager_map_iter_t iter = m_bufferMgrMap.begin(); iter != m_bufferMgrMap.end(); iter++) {
        bufferMgr = (ExynosCameraBufferManager *)iter->second;
        bufferMgr->dump();
    }
}

void ExynosCameraBufferSupplier::m_deinit(void)
{
    Mutex::Autolock lock(m_bufferMgrMapLock);

    ExynosCameraBufferManager *bufferMgr = NULL;
    buffer_manager_map_iter_t iter = m_bufferMgrMap.begin();

    while (iter != m_bufferMgrMap.end()) {
        bufferMgr = iter->second;
        bufferMgr->deinit();
        delete bufferMgr;
        bufferMgr = NULL;
        iter = m_bufferMgrMap.erase(iter);
    }

    m_bufferMgrMap.clear();
}

ExynosCameraBufferManager* ExynosCameraBufferSupplier::m_getBufferManager(const buffer_manager_tag_t tag)
{
    for (buffer_manager_map_iter_t iter = m_bufferMgrMap.begin(); iter != m_bufferMgrMap.end(); iter++) {
        if (tag == iter->first) {
            return iter->second;
        }
    }

    CLOGE("[P%d T%d]Failed to find bufferManager",
            tag.pipeId[0], tag.managerType);

    return NULL;
}
} // namespace android
