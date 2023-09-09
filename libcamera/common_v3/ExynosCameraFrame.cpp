/*
**
** Copyright 2013, Samsung Electronics Co. LTD
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

/* #define LOG_NDEBUG 0 */
#define LOG_TAG "ExynosCameraFrame"
#include <cutils/log.h>

#include "ExynosCameraFrame.h"

namespace android {

#ifdef DEBUG_FRAME_MEMORY_LEAK
unsigned long long ExynosCameraFrame::m_checkLeakCount;
unsigned long long ExynosCameraFrame::m_checkLeakFrameCount;
Mutex ExynosCameraFrame::m_countLock;
#endif

ExynosCameraFrame::ExynosCameraFrame(
        int cameraId,
        ExynosCameraParameters *obj_param,
        uint32_t frameCount,
        uint32_t frameType)
{
    m_cameraId = cameraId;
    m_parameters = obj_param;
    m_frameCount = frameCount;
    m_frameType = frameType;
    memset(m_name, 0x00, sizeof(m_name));

    CLOGV(" create frame type(%d), frameCount(%d)", frameType, frameCount);

    m_init();
}

ExynosCameraFrame::ExynosCameraFrame(int cameraId)
{
    m_cameraId = cameraId;
    m_parameters = NULL;
    m_frameCount = 0;
    m_frameType = FRAME_TYPE_OTHERS;
    memset(m_name, 0x00, sizeof(m_name));

    m_init();
}

ExynosCameraFrame::~ExynosCameraFrame()
{
    m_deinit();
}

#ifdef DEBUG_FRAME_MEMORY_LEAK
long long int ExynosCameraFrame::getCheckLeakCount()
{
    return m_checkLeakCount;
}
#endif

status_t ExynosCameraFrame::addSiblingEntity(
        __unused ExynosCameraFrameEntity *curEntity,
        ExynosCameraFrameEntity *newEntity)
{
    Mutex::Autolock l(m_linkageLock);
    m_linkageList.push_back(newEntity);

    return NO_ERROR;
}

status_t ExynosCameraFrame::addChildEntity(
        ExynosCameraFrameEntity *parentEntity,
        ExynosCameraFrameEntity *newEntity)
{
    status_t ret = NO_ERROR;

    if (parentEntity == NULL) {
        CLOGE("parentEntity is NULL");
        return BAD_VALUE;
    }

    /* TODO: This is not suit in case of newEntity->next != NULL */
    ExynosCameraFrameEntity *tmpEntity;

    tmpEntity = parentEntity->getNextEntity();
    ret = parentEntity->setNextEntity(newEntity);
    if (ret != NO_ERROR) {
        CLOGE("setNextEntity fail, ret(%d)", ret);
        return ret;
    }
    newEntity->setNextEntity(tmpEntity);

    return ret;
}

status_t ExynosCameraFrame::addChildEntity(
        ExynosCameraFrameEntity *parentEntity,
        ExynosCameraFrameEntity *newEntity,
        int parentPipeId)
{
    status_t ret = NO_ERROR;

    if (parentEntity == NULL) {
        CLOGE("parentEntity is NULL");
        return BAD_VALUE;
    }

    /* TODO: This is not suit in case of newEntity->next != NULL */
    ExynosCameraFrameEntity *tmpEntity;

    tmpEntity = parentEntity->getNextEntity();
    ret = parentEntity->setNextEntity(newEntity);
    if (ret != NO_ERROR) {
        CLOGE("setNextEntity fail, ret(%d)", ret);
        return ret;
    }

    if (0 <= parentPipeId) {
        ret = newEntity->setParentPipeId((enum pipeline)parentPipeId);
        if (ret != NO_ERROR) {
            CLOGE("setParentPipeId(%d) fail, ret(%d)", parentPipeId, ret);
            return ret;
        }
    } else {
        CLOGW("parentPipeId(%d) < 0. you may set parentPipeId which connect between parent(%d) and child(%d)",
             parentPipeId, parentEntity->getPipeId(), newEntity->getPipeId());
    }

    newEntity->setNextEntity(tmpEntity);
    return ret;
}

ExynosCameraFrameEntity *ExynosCameraFrame::getFirstEntity(void)
{
    List<ExynosCameraFrameEntity *>::iterator r;
    ExynosCameraFrameEntity *firstEntity = NULL;

    Mutex::Autolock l(m_linkageLock);
    if (m_linkageList.empty()) {
        CLOGE("m_linkageList is empty");
        firstEntity = NULL;
        return firstEntity;
    }

    r = m_linkageList.begin()++;
    m_currentEntity = r;
    firstEntity = *r;

    return firstEntity;
}

ExynosCameraFrameEntity *ExynosCameraFrame::getNextEntity(void)
{
    ExynosCameraFrameEntity *nextEntity = NULL;

    Mutex::Autolock l(m_linkageLock);
    m_currentEntity++;

    if (m_currentEntity == m_linkageList.end()) {
        return nextEntity;
    }

    nextEntity = *m_currentEntity;

    return nextEntity;
}
/* Unused, but useful */
/*
ExynosCameraFrameEntity *ExynosCameraFrame::getChildEntity(ExynosCameraFrameEntity *parentEntity)
{
    ExynosCameraFrameEntity *childEntity = NULL;

    if (parentEntity == NULL) {
        CLOGE("parentEntity is NULL");
        return childEntity;
    }

    childEntity = parentEntity->getNextEntity();

    return childEntity;
}
*/

ExynosCameraFrameEntity *ExynosCameraFrame::searchEntityByPipeId(uint32_t pipeId)
{
    List<ExynosCameraFrameEntity *>::iterator r;
    ExynosCameraFrameEntity *curEntity = NULL;
    int listSize = 0;

    Mutex::Autolock l(m_linkageLock);
    if (m_linkageList.empty()) {
        CLOGE("m_linkageList is empty");
        return NULL;
    }

    listSize = m_linkageList.size();
    r = m_linkageList.begin();

    for (int i = 0; i < listSize; i++) {
        curEntity = *r;
        if (curEntity == NULL) {
            CLOGE("curEntity is NULL, index(%d), linkageList size(%d)",
                 i, listSize);
            return NULL;
        }

        while (curEntity != NULL) {
            if (curEntity->getPipeId() == pipeId)
                return curEntity;
            curEntity = curEntity->getNextEntity();
        }
        r++;
    }

    CLOGD("Cannot find matched entity, frameCount(%d), pipeId(%d)", getFrameCount(), pipeId);

    return NULL;
}

status_t ExynosCameraFrame::setSrcBuffer(uint32_t pipeId,
                                         ExynosCameraBuffer srcBuf,
                                         uint32_t nodeIndex)
{
    status_t ret = NO_ERROR;

    if (nodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid buffer index, index(%d)", nodeIndex);
        return BAD_VALUE;
    }

    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        CLOGE("Could not find entity, pipeID(%d)", pipeId);
        return BAD_VALUE;
    }

    ret = entity->setSrcBuf(srcBuf, nodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("Could not set src buffer, ret(%d)", ret);
        return ret;
    }

    return ret;
}

status_t ExynosCameraFrame::setDstBuffer(uint32_t pipeId,
                                         ExynosCameraBuffer dstBuf,
                                         uint32_t nodeIndex)
{
    status_t ret = NO_ERROR;

    if (nodeIndex >= DST_BUFFER_COUNT_MAX) {
        CLOGE("Invalid buffer index, index(%d)", nodeIndex);
        return BAD_VALUE;
    }

    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        CLOGE("Could not find entity, pipeID(%d)", pipeId);
        return BAD_VALUE;
    }

    ret = entity->setDstBuf(dstBuf, nodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("Could not set dst buffer, ret(%d)", ret);
        return ret;
    }

    /* TODO: set buffer to child node's source */
    entity = entity->getNextEntity();
    if (entity != NULL) {
        ret = entity->setSrcBuf(dstBuf);
        if (ret != NO_ERROR) {
            CLOGE("Could not set dst buffer, ret(%d)", ret);
            return ret;
        }
    }

    return ret;
}

status_t ExynosCameraFrame::setDstBuffer(uint32_t pipeId,
                                         ExynosCameraBuffer dstBuf,
                                         uint32_t nodeIndex,
                                         int      parentPipeId)
{
    status_t ret = NO_ERROR;

    if (nodeIndex >= DST_BUFFER_COUNT_MAX) {
        CLOGE("Invalid buffer index, index(%d)", nodeIndex);
        return BAD_VALUE;
    }

    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        CLOGE("Could not find entity, pipeID(%d)", pipeId);
        return BAD_VALUE;
    }

    ret = entity->setDstBuf(dstBuf, nodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("Could not set dst buffer, ret(%d)", ret);
        return ret;
    }

    /* TODO: set buffer to child node's source */
    entity = entity->getNextEntity();
    if (entity != NULL) {
        bool flagSetChildEntity = false;

        /*
         * it  will set child's source buffer
         * when no specific parent set. (for backward compatibility)
         * when specific parent only. (for MCPipe)
         */
        if (entity->flagSpecficParent() == true) {
            if (parentPipeId == entity->getParentPipeId()) {
                flagSetChildEntity = true;
            } else {
                CLOGV("parentPipeId(%d) != entity->getParentPipeId()(%d). so skip setting child src Buf",
                     parentPipeId, entity->getParentPipeId());
            }
        } else {
            /* this is for backward compatiblity */
            flagSetChildEntity = true;
        }

        /* child mode need to setting next */
        if (flagSetChildEntity == true) {
            ret = entity->setSrcBuf(dstBuf);
            if (ret != NO_ERROR) {
                CLOGE("Could not set dst buffer, ret(%d)", ret);
                return ret;
            }
        }
    }

    return ret;
}

status_t ExynosCameraFrame::getSrcBuffer(uint32_t pipeId,
                                         ExynosCameraBuffer *srcBuf,
                                         uint32_t nodeIndex)
{
    status_t ret = NO_ERROR;

    if (nodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid buffer index, index(%d)", nodeIndex);
        return BAD_VALUE;
    }

    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        CLOGE("Could not find entity, pipeID(%d)", pipeId);
        return BAD_VALUE;
    }

    ret = entity->getSrcBuf(srcBuf, nodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("Could not get src buffer, ret(%d)", ret);
        return ret;
    }

    return ret;
}

status_t ExynosCameraFrame::getDstBuffer(uint32_t pipeId,
                                         ExynosCameraBuffer *dstBuf,
                                         uint32_t nodeIndex)
{
    status_t ret = NO_ERROR;

    if (nodeIndex >= DST_BUFFER_COUNT_MAX) {
        CLOGE("Invalid buffer index, index(%d)", nodeIndex);
        return BAD_VALUE;
    }

    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        CLOGE("Could not find entity, pipeID(%d)", pipeId);
        return BAD_VALUE;
    }

    ret = entity->getDstBuf(dstBuf, nodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("Could not get dst buffer, ret(%d)", ret);
        return ret;
    }

    return ret;
}

status_t ExynosCameraFrame::setSrcRect(uint32_t pipeId, ExynosRect srcRect, uint32_t nodeIndex)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        CLOGE("Could not find entity, pipeID(%d)", pipeId);
        return BAD_VALUE;
    }

    ret = entity->setSrcRect(srcRect, nodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("Could not set src rect, ret(%d)", ret);
        return ret;
    }

    return ret;
}

status_t ExynosCameraFrame::setDstRect(uint32_t pipeId, ExynosRect dstRect, uint32_t nodeIndex)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        CLOGE("Could not find entity, pipeID(%d)", pipeId);
        return BAD_VALUE;
    }

    ret = entity->setDstRect(dstRect);
    if (ret != NO_ERROR) {
        CLOGE("Could not set dst rect, ret(%d)", ret);
        return ret;
    }

    /* TODO: set buffer to child node's source */
    entity = entity->getNextEntity();
    if (entity != NULL) {
        ret = entity->setSrcRect(dstRect, nodeIndex);
        if (ret != NO_ERROR) {
            CLOGE("Could not set dst rect, ret(%d)", ret);
            return ret;
        }
    }

    return ret;
}

status_t ExynosCameraFrame::getSrcRect(uint32_t pipeId, ExynosRect *srcRect, uint32_t nodeIndex)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        CLOGE("Could not find entity, pipeID(%d)", pipeId);
        return BAD_VALUE;
    }

    ret = entity->getSrcRect(srcRect, nodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("Could not get src rect, ret(%d)", ret);
        return ret;
    }

    return ret;
}

status_t ExynosCameraFrame::getDstRect(uint32_t pipeId, ExynosRect *dstRect, uint32_t nodeIndex)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        CLOGE("Could not find entity, pipeID(%d)", pipeId);
        return BAD_VALUE;
    }

    ret = entity->getDstRect(dstRect, nodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("Could not get dst rect, ret(%d)", ret);
        return ret;
    }

    return ret;
}

status_t ExynosCameraFrame::getSrcBufferState(uint32_t pipeId,
                                         entity_buffer_state_t *state,
                                         uint32_t nodeIndex)
{
    if (nodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid buffer index, index(%d)", nodeIndex);
        return BAD_VALUE;
    }

    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        CLOGE("Could not find entity, pipeID(%d)", pipeId);
        return BAD_VALUE;
    }

    *state = entity->getSrcBufState(nodeIndex);

    return NO_ERROR;
}

status_t ExynosCameraFrame::getDstBufferState(uint32_t pipeId,
                                         entity_buffer_state_t *state,
                                         uint32_t nodeIndex)
{
    if (nodeIndex >= DST_BUFFER_COUNT_MAX) {
        CLOGE("Invalid buffer index, index(%d)", nodeIndex);
        return BAD_VALUE;
    }

    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        CLOGE("Could not find entity, pipeID(%d)", pipeId);
        return BAD_VALUE;
    }

    *state = entity->getDstBufState(nodeIndex);

    return NO_ERROR;
}

status_t ExynosCameraFrame::setSrcBufferState(uint32_t pipeId,
                                         entity_buffer_state_t state,
                                         uint32_t nodeIndex)
{
    status_t ret = NO_ERROR;

    if (nodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid buffer index, index(%d)", nodeIndex);
        return BAD_VALUE;
    }

    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        CLOGE("Could not find entity, pipeID(%d)", pipeId);
        return BAD_VALUE;
    }

    ret = entity->setSrcBufState(state, nodeIndex);

    return ret;
}

status_t ExynosCameraFrame::setDstBufferState(uint32_t pipeId,
                                         entity_buffer_state_t state,
                                         uint32_t nodeIndex)
{
    status_t ret = NO_ERROR;

    if (nodeIndex >= DST_BUFFER_COUNT_MAX) {
        CLOGE("Invalid buffer index, index(%d)", nodeIndex);
        return BAD_VALUE;
    }

    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        CLOGE("Could not find entity, pipeID(%d)", pipeId);
        return BAD_VALUE;
    }

    ret = entity->setDstBufState(state, nodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("Could not set dst buffer, ret(%d)", ret);
        return ret;
    }

    /* Set buffer to child node's source */
    entity = entity->getNextEntity();
    if (entity != NULL) {
        ret = entity->setSrcBufState(state);
        if (ret != NO_ERROR) {
            CLOGE("Could not set src buffer, ret(%d)", ret);
            return ret;
        }
    }

    return ret;
}

status_t ExynosCameraFrame::ensureSrcBufferState(uint32_t pipeId,
                                         entity_buffer_state_t state)
{
    status_t ret = NO_ERROR;
    int retry = 0;
    entity_buffer_state_t curState;

    do {
        ret = getSrcBufferState(pipeId, &curState);
        if (ret != NO_ERROR)
            continue;

        if (state == curState) {
            ret = OK;
            break;
        } else {
            ret = BAD_VALUE;
            usleep(100);
        }

        retry++;
        if (retry == 10)
            ret = TIMED_OUT;
    } while (ret != OK && retry < 100);

    CLOGV(" retry count %d", retry);

    return ret;
}

status_t ExynosCameraFrame::ensureDstBufferState(uint32_t pipeId,
                                         entity_buffer_state_t state)
{
    status_t ret = NO_ERROR;
    int retry = 0;
    entity_buffer_state_t curState;

    do {
        ret = getDstBufferState(pipeId, &curState);
        if (ret != NO_ERROR)
            continue;

        if (state == curState) {
            ret = OK;
            break;
        } else {
            ret = BAD_VALUE;
            usleep(100);
        }

        retry++;
        if (retry == 10)
            ret = TIMED_OUT;
    } while (ret != OK && retry < 100);

    CLOGV(" retry count %d", retry);

    return ret;
}

status_t ExynosCameraFrame::setEntityState(uint32_t pipeId,
                                           entity_state_t state)
{
#ifdef USE_FRAME_REFERENCE_COUNT
    Mutex::Autolock lock(m_entityLock);
#endif
    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        CLOGE("Could not find entity, pipeID(%d)", pipeId);
        return BAD_VALUE;
    }

    if (entity->getEntityState() == ENTITY_STATE_COMPLETE &&
        state != ENTITY_STATE_REWORK) {
        return NO_ERROR;
    }

    if (state == ENTITY_STATE_COMPLETE) {
        m_numCompletePipe++;
        if (m_numCompletePipe >= m_numRequestPipe)
            setFrameState(FRAME_STATE_COMPLETE);
    }

    entity->setEntityState(state);

    return NO_ERROR;
}

status_t ExynosCameraFrame::getEntityState(uint32_t pipeId,
                                           entity_state_t *state)
{
#ifdef USE_FRAME_REFERENCE_COUNT
    Mutex::Autolock lock(m_entityLock);
#endif
    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        CLOGE("Could not find entity, pipeID(%d)", pipeId);
        return BAD_VALUE;
    }

    *state = entity->getEntityState();
    return NO_ERROR;
}

status_t ExynosCameraFrame::getEntityBufferType(uint32_t pipeId,
                                                entity_buffer_type_t *type)
{
#ifdef USE_FRAME_REFERENCE_COUNT
    Mutex::Autolock lock(m_entityLock);
#endif
    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        CLOGE("Could not find entity, pipeID(%d)", pipeId);
        return BAD_VALUE;
    }

    *type = entity->getBufType();
    return NO_ERROR;
}

uint32_t ExynosCameraFrame::getFrameCount(void)
{
    return m_frameCount;
}

int32_t ExynosCameraFrame::getMetaFrameCount(uint32_t srcNodeIndex)
{
    return getMetaDmRequestFrameCount(&m_metaData[srcNodeIndex]);
}

status_t ExynosCameraFrame::setNumRequestPipe(uint32_t num)
{
    m_numRequestPipe = num;
    return NO_ERROR;
}

uint32_t ExynosCameraFrame::getNumRequestPipe(void)
{
    return m_numRequestPipe;
}

bool ExynosCameraFrame::isComplete(void)
{
    return checkFrameState(FRAME_STATE_COMPLETE);
}

ExynosCameraFrameEntity *ExynosCameraFrame::getFrameDoneEntity(void)
{
    List<ExynosCameraFrameEntity *>::iterator r;
    ExynosCameraFrameEntity *curEntity = NULL;

    Mutex::Autolock l(m_linkageLock);
    if (m_linkageList.empty()) {
        CLOGE("m_linkageList is empty");
        return NULL;
    }

    r = m_linkageList.begin()++;
    curEntity = *r;

    while (r != m_linkageList.end()) {
        if (curEntity != NULL) {
            switch (curEntity->getEntityState()) {
            case ENTITY_STATE_FRAME_SKIP:
            case ENTITY_STATE_REWORK:
            case ENTITY_STATE_FRAME_DONE:
                if (curEntity->getNextEntity() != NULL) {
                    curEntity = curEntity->getNextEntity();
                    continue;
                }
                return curEntity;
                break;
            default:
                break;
            }
        }
        r++;
        curEntity = *r;
    }

    return NULL;
}

ExynosCameraFrameEntity *ExynosCameraFrame::getFrameDoneEntity(uint32_t pipeID)
{
    List<ExynosCameraFrameEntity *>::iterator r;
    ExynosCameraFrameEntity *curEntity = NULL;

    Mutex::Autolock l(m_linkageLock);
    if (m_linkageList.empty()) {
        CLOGE("m_linkageList is empty");
        return NULL;
    }

    r = m_linkageList.begin()++;
    curEntity = *r;

    while (r != m_linkageList.end()) {
        if (curEntity != NULL && pipeID == curEntity->getPipeId()) {
            switch (curEntity->getEntityState()) {
            case ENTITY_STATE_FRAME_SKIP:
            case ENTITY_STATE_REWORK:
            case ENTITY_STATE_FRAME_DONE:
                if (curEntity->getNextEntity() != NULL) {
                    curEntity = curEntity->getNextEntity();
                    continue;
                }
                return curEntity;
                break;
            default:
                break;
            }
        }
        r++;
        curEntity = *r;
    }

    return NULL;
}

ExynosCameraFrameEntity *ExynosCameraFrame::getFrameDoneFirstEntity(void)
{
    List<ExynosCameraFrameEntity *>::iterator r;
    ExynosCameraFrameEntity *curEntity = NULL;

    Mutex::Autolock l(m_linkageLock);
    if (m_linkageList.empty()) {
        CLOGE("m_linkageList is empty");
        return NULL;
    }

    r = m_linkageList.begin()++;
    curEntity = *r;

    while (r != m_linkageList.end()) {
        if (curEntity != NULL) {
            switch (curEntity->getEntityState()) {
            case ENTITY_STATE_REWORK:
                if (curEntity->getNextEntity() != NULL) {
                    curEntity = curEntity->getNextEntity();
                    continue;
                }
                return curEntity;
                break;
            case ENTITY_STATE_FRAME_DONE:
                return curEntity;
                break;
            case ENTITY_STATE_FRAME_SKIP:
            case ENTITY_STATE_COMPLETE:
                if (curEntity->getNextEntity() != NULL) {
                    curEntity = curEntity->getNextEntity();
                    continue;
                }
                break;
            default:
                break;
            }
        }
        r++;
        curEntity = *r;
    }

    return NULL;
}

ExynosCameraFrameEntity *ExynosCameraFrame::getFrameDoneFirstEntity(uint32_t pipeID)
{
    List<ExynosCameraFrameEntity *>::iterator r;
    ExynosCameraFrameEntity *curEntity = NULL;

    Mutex::Autolock l(m_linkageLock);
    if (m_linkageList.empty()) {
        CLOGE("m_linkageList is empty");
        return NULL;
    }

    r = m_linkageList.begin()++;
    curEntity = *r;

    while (r != m_linkageList.end()) {
        if (curEntity != NULL) {
            switch (curEntity->getEntityState()) {
            case ENTITY_STATE_REWORK:
                if (curEntity->getPipeId() == pipeID)
                    return curEntity;

                if (curEntity->getNextEntity() != NULL) {
                    curEntity = curEntity->getNextEntity();
                    continue;
                }
                break;
            case ENTITY_STATE_FRAME_DONE:
                if (curEntity->getPipeId() == pipeID)
                    return curEntity;

                if (curEntity->getNextEntity() != NULL) {
                    curEntity = curEntity->getNextEntity();
                    continue;
                }
                break;
            case ENTITY_STATE_FRAME_SKIP:
            case ENTITY_STATE_COMPLETE:
                if (curEntity->getNextEntity() != NULL) {
                    curEntity = curEntity->getNextEntity();
                    continue;
                }
                break;
            default:
                break;
            }
        }
        r++;
        curEntity = *r;
    }

    return NULL;
}

status_t ExynosCameraFrame::skipFrame(void)
{
#ifdef USE_FRAME_REFERENCE_COUNT
    Mutex::Autolock lock(m_frameStateLock);
#endif
    m_frameState = FRAME_STATE_SKIPPED;

    return NO_ERROR;
}

void ExynosCameraFrame::setFrameState(frame_status_t state)
{
#ifdef USE_FRAME_REFERENCE_COUNT
    Mutex::Autolock lock(m_frameStateLock);
#endif
    /* TODO: We need state machine */
    if (state > FRAME_STATE_INVALID)
        m_frameState = FRAME_STATE_INVALID;
    else
        m_frameState = state;
}

frame_status_t ExynosCameraFrame::getFrameState(void)
{
#ifdef USE_FRAME_REFERENCE_COUNT
    Mutex::Autolock lock(m_frameStateLock);
#endif
    return m_frameState;
}

bool ExynosCameraFrame::checkFrameState(frame_status_t state)
{
#ifdef USE_FRAME_REFERENCE_COUNT
    Mutex::Autolock lock(m_frameStateLock);
#endif
    return (m_frameState == state) ? true : false;
}

void ExynosCameraFrame::printEntity(void)
{
    List<ExynosCameraFrameEntity *>::iterator r;
    ExynosCameraFrameEntity *curEntity = NULL;
    int listSize = 0;

    Mutex::Autolock l(m_linkageLock);
    if (m_linkageList.empty()) {
        CLOGE("m_linkageList is empty");
        return;
    }

    listSize = m_linkageList.size();
    r = m_linkageList.begin();

    CLOGD(" FrameCount(%d), request(%d), complete(%d)", getFrameCount(), m_numRequestPipe, m_numCompletePipe);

    for (int i = 0; i < listSize; i++) {
        curEntity = *r;
        if (curEntity == NULL) {
            CLOGE("curEntity is NULL, index(%d)", i);
            return;
        }

        CLOGD("sibling id(%d), state(%d)",
             curEntity->getPipeId(), curEntity->getEntityState());

        while (curEntity != NULL) {
            CLOGD("----- Child id(%d), state(%d)",
                 curEntity->getPipeId(), curEntity->getEntityState());
            curEntity = curEntity->getNextEntity();
        }
        r++;
    }

    return;
}

void ExynosCameraFrame::printNotDoneEntity(void)
{
    List<ExynosCameraFrameEntity *>::iterator r;
    ExynosCameraFrameEntity *curEntity = NULL;
    int listSize = 0;

    Mutex::Autolock l(m_linkageLock);
    if (m_linkageList.empty()) {
        CLOGE("m_linkageList is empty");
        return;
    }

    listSize = m_linkageList.size();
    r = m_linkageList.begin();

    CLOGD(" FrameCount(%d), request(%d), complete(%d)",
             getFrameCount(), m_numRequestPipe, m_numCompletePipe);

    for (int i = 0; i < listSize; i++) {
        curEntity = *r;
        if (curEntity == NULL) {
            CLOGE("curEntity is NULL, index(%d)", i);
            return;
        }

        if (curEntity->getEntityState() != ENTITY_STATE_COMPLETE) {
            CLOGD("sibling id(%d), state(%d)",
                     curEntity->getPipeId(), curEntity->getEntityState());
        }

        while (curEntity != NULL) {
            if (curEntity->getEntityState() != ENTITY_STATE_COMPLETE) {
                CLOGD("----- Child id(%d), state(%d)",
                         curEntity->getPipeId(), curEntity->getEntityState());
            }
            curEntity = curEntity->getNextEntity();
        }
        r++;
    }

    return;
}

void ExynosCameraFrame::dump(void)
{
    printEntity();

    for (int i = 0; i < MAX_NUM_PIPES; i ++) {
        if (m_request[INDEX(i)] == true)
            CLOGI("pipeId(%d)'s request is ture", i);
    }
}

void ExynosCameraFrame::frameLock(void)
{
#ifndef USE_FRAME_REFERENCE_COUNT
    Mutex::Autolock lock(m_frameLock);
#endif
    m_frameLocked = true;
}

void ExynosCameraFrame::frameUnlock(void)
{
#ifndef USE_FRAME_REFERENCE_COUNT
        Mutex::Autolock lock(m_frameLock);
#endif
    m_frameLocked = false;
}

bool ExynosCameraFrame::getFrameLockState(void)
{
#ifndef USE_FRAME_REFERENCE_COUNT
        Mutex::Autolock lock(m_frameLock);
#endif
    return m_frameLocked;
}

status_t ExynosCameraFrame::initMetaData(struct camera2_shot_ext *shot, uint32_t srcNodeIndex)
{
    status_t ret = NO_ERROR;

    if (shot != NULL) {
        CLOGV(" initialize shot_ext");
        memcpy(&m_metaData[srcNodeIndex], shot, sizeof(struct camera2_shot_ext));
    }

    ret = m_parameters->duplicateCtrlMetadata(&m_metaData[srcNodeIndex]);
    if (ret != NO_ERROR) {
        CLOGE("duplicate Ctrl metadata fail");
        return INVALID_OPERATION;
    }

    return ret;
}

status_t ExynosCameraFrame::getMetaData(struct camera2_shot_ext *shot, uint32_t srcNodeIndex)
{
    if (shot == NULL) {
        CLOGE(" buffer is NULL");
        return BAD_VALUE;
    }

    memcpy(shot, &m_metaData[srcNodeIndex], sizeof(struct camera2_shot_ext));

    return NO_ERROR;
}

status_t ExynosCameraFrame::setMetaData(struct camera2_shot_ext *shot, uint32_t srcNodeIndex)
{
    if (shot == NULL) {
        CLOGE(" buffer is NULL");
        return BAD_VALUE;
    }

    memcpy(&m_metaData[srcNodeIndex], shot, sizeof(struct camera2_shot_ext));

    return NO_ERROR;
}

status_t ExynosCameraFrame::storeDynamicMeta(struct camera2_shot_ext *shot, uint32_t srcNodeIndex)
{
    if (shot == NULL) {
        CLOGE(" buffer is NULL");
        return BAD_VALUE;
    }

    if (getMetaDmRequestFrameCount(shot) == 0)
        CLOGW(" DM Frame count is ZERO");

    memcpy(&m_metaData[srcNodeIndex].shot.dm, &shot->shot.dm, sizeof(struct camera2_dm));

    return NO_ERROR;
}

status_t ExynosCameraFrame::storeDynamicMeta(struct camera2_dm *dm, uint32_t srcNodeIndex)
{
    if (dm == NULL) {
        CLOGE(" buffer is NULL");
        return BAD_VALUE;
    }

    if (getMetaDmRequestFrameCount(dm) == 0)
        CLOGW(" DM Frame count is ZERO");

    memcpy(&m_metaData[srcNodeIndex].shot.dm, dm, sizeof(struct camera2_dm));

    return NO_ERROR;
}

status_t ExynosCameraFrame::storeUserDynamicMeta(struct camera2_shot_ext *shot, uint32_t srcNodeIndex)
{
    if (shot == NULL) {
        CLOGE(" buffer is NULL");
        return BAD_VALUE;
    }

    memcpy(&m_metaData[srcNodeIndex].shot.udm, &shot->shot.udm, sizeof(struct camera2_udm));

    return NO_ERROR;
}

status_t ExynosCameraFrame::storeUserDynamicMeta(struct camera2_udm *udm, uint32_t srcNodeIndex)
{
    if (udm == NULL) {
        CLOGE(" buffer is NULL");
        return BAD_VALUE;
    }

    memcpy(&m_metaData[srcNodeIndex].shot.udm, udm, sizeof(struct camera2_udm));

    return NO_ERROR;
}

status_t ExynosCameraFrame::getDynamicMeta(struct camera2_shot_ext *shot, uint32_t srcNodeIndex)
{
    if (shot == NULL) {
        CLOGE(" buffer is NULL");
        return BAD_VALUE;
    }

    memcpy(&shot->shot.dm, &m_metaData[srcNodeIndex].shot.dm, sizeof(struct camera2_dm));

    return NO_ERROR;
}

status_t ExynosCameraFrame::getDynamicMeta(struct camera2_dm *dm, uint32_t srcNodeIndex)
{
    if (dm == NULL) {
        CLOGE(" buffer is NULL");
        return BAD_VALUE;
    }

    memcpy(dm, &m_metaData[srcNodeIndex].shot.dm, sizeof(struct camera2_dm));

    return NO_ERROR;
}

status_t ExynosCameraFrame::getUserDynamicMeta(struct camera2_shot_ext *shot, uint32_t srcNodeIndex)
{
    if (shot == NULL) {
        CLOGE(" buffer is NULL");
        return BAD_VALUE;
    }

    memcpy(&shot->shot.udm, &m_metaData[srcNodeIndex].shot.udm, sizeof(struct camera2_udm));

    return NO_ERROR;
}

status_t ExynosCameraFrame::getUserDynamicMeta(struct camera2_udm *udm, uint32_t srcNodeIndex)
{
    if (udm == NULL) {
        CLOGE(" buffer is NULL");
        return BAD_VALUE;
    }

    memcpy(udm, &m_metaData[srcNodeIndex].shot.udm, sizeof(struct camera2_udm));

    return NO_ERROR;
}

status_t ExynosCameraFrame::setMetaDataEnable(bool flag, uint32_t srcNodeIndex)
{
    m_metaDataEnable[srcNodeIndex] = flag;
    return NO_ERROR;
}

bool ExynosCameraFrame::getMetaDataEnable(uint32_t srcNodeIndex)
{
    long count = 0;

    while (count < DM_WAITING_COUNT) {
        if (m_metaDataEnable[srcNodeIndex] == true) {
            if (0 < count)
                CLOGD(" metadata enable count(%ld) ", count);

            break;
        }

        count++;
        usleep(WAITING_TIME);
    }

    return m_metaDataEnable[srcNodeIndex];
}

int ExynosCameraFrame::getZoom(uint32_t srcNodeIndex)
{
    return m_zoom[srcNodeIndex];
}

status_t ExynosCameraFrame::setZoom(int zoom, uint32_t srcNodeIndex)
{
    m_zoom[srcNodeIndex] = zoom;

    return NO_ERROR;
}

status_t ExynosCameraFrame::getNodeGroupInfo(struct camera2_node_group *node_group, int index, uint32_t srcNodeIndex)
{
    if (node_group == NULL) {
        CLOGE(" node_group is NULL");
        return BAD_VALUE;
    }

    if (index >= PERFRAME_NODE_GROUP_MAX) {
        CLOGE(" index is bigger than PERFRAME_NODE_GROUP_MAX");
        return BAD_VALUE;
    }

    memcpy(node_group, &m_node_gorup[srcNodeIndex][index], sizeof(struct camera2_node_group));

    return NO_ERROR;
}

status_t ExynosCameraFrame::storeNodeGroupInfo(struct camera2_node_group *node_group, int index, uint32_t srcNodeIndex)
{
    if (node_group == NULL) {
        CLOGE(" node_group is NULL");
        return BAD_VALUE;
    }

    if (index >= PERFRAME_NODE_GROUP_MAX) {
        CLOGE(" index is bigger than PERFRAME_NODE_GROUP_MAX");
        return BAD_VALUE;
    }

    memcpy(&m_node_gorup[srcNodeIndex][index], node_group, sizeof(struct camera2_node_group));

    return NO_ERROR;
}

void ExynosCameraFrame::dumpNodeGroupInfo(const char *name, uint32_t srcNodeIndex)
{
    if (name != NULL)
        CLOGD("(%s)++++++++++++++++++++ frameCount(%d)", name, m_frameCount);
    else
        CLOGD("()++++++++++++++++++++ frameCount(%d)", m_frameCount);

    for (int i = 0; i < PERFRAME_NODE_GROUP_MAX; i ++) {
        CLOGI("Leader[%d] (%d, %d, %d, %d)(%d, %d, %d, %d)(%d %d)",
            i,
            m_node_gorup[srcNodeIndex][i].leader.input.cropRegion[0],
            m_node_gorup[srcNodeIndex][i].leader.input.cropRegion[1],
            m_node_gorup[srcNodeIndex][i].leader.input.cropRegion[2],
            m_node_gorup[srcNodeIndex][i].leader.input.cropRegion[3],
            m_node_gorup[srcNodeIndex][i].leader.output.cropRegion[0],
            m_node_gorup[srcNodeIndex][i].leader.output.cropRegion[1],
            m_node_gorup[srcNodeIndex][i].leader.output.cropRegion[2],
            m_node_gorup[srcNodeIndex][i].leader.output.cropRegion[3],
            m_node_gorup[srcNodeIndex][i].leader.request,
            m_node_gorup[srcNodeIndex][i].leader.vid);

        for (int j = 0; j < CAPTURE_NODE_MAX; j ++) {
            CLOGI("Capture[%d][%d] (%d, %d, %d, %d)(%d, %d, %d, %d)(%d, %d)",
                i,
                j,
                m_node_gorup[srcNodeIndex][i].capture[j].input.cropRegion[0],
                m_node_gorup[srcNodeIndex][i].capture[j].input.cropRegion[1],
                m_node_gorup[srcNodeIndex][i].capture[j].input.cropRegion[2],
                m_node_gorup[srcNodeIndex][i].capture[j].input.cropRegion[3],
                m_node_gorup[srcNodeIndex][i].capture[j].output.cropRegion[0],
                m_node_gorup[srcNodeIndex][i].capture[j].output.cropRegion[1],
                m_node_gorup[srcNodeIndex][i].capture[j].output.cropRegion[2],
                m_node_gorup[srcNodeIndex][i].capture[j].output.cropRegion[3],
                m_node_gorup[srcNodeIndex][i].capture[j].request,
                m_node_gorup[srcNodeIndex][i].capture[j].vid);
        }

        if (name != NULL)
            CLOGD("(%s)------------------------ ", name);
        else
            CLOGD("()------------------------ ");
    }

    if (name != NULL)
        CLOGD("(%s)++++++++++++++++++++", name);
    else
        CLOGD("()++++++++++++++++++++");

    return;
}

sync_type_t ExynosCameraFrame::getSyncType(void)
{
    return m_syncType;
}

status_t ExynosCameraFrame::setSyncType(sync_type_t type)
{
    m_syncType = type;

    return NO_ERROR;
}

/* backup for reprocessing */
void ExynosCameraFrame::setReprocessingFrameType(frame_type_t frameType)
{
    m_reprocessingFrameType = frameType;
}

uint32_t ExynosCameraFrame::getReprocessingFrameType()
{
    return m_reprocessingFrameType;
}

int ExynosCameraFrame::getReprocessingZoom(void)
{
    return m_reprocessingZoom;
}

status_t ExynosCameraFrame::setReprocessingZoom(int zoom)
{
    m_reprocessingZoom = zoom;

    return NO_ERROR;
}

sync_type_t ExynosCameraFrame::getReprocessingSyncType(void)
{
    return m_reprocessingSyncType;
}

status_t ExynosCameraFrame::setReprocessingSyncType(sync_type_t type)
{
    m_reprocessingSyncType = type;

    return NO_ERROR;
}

void ExynosCameraFrame::setJpegSize(int size)
{
    m_jpegSize = size;
}

int ExynosCameraFrame::getJpegSize(void)
{
    return m_jpegSize;
}

int64_t ExynosCameraFrame::getTimeStamp(uint32_t srcNodeIndex)
{
    return (int64_t)getMetaDmSensorTimeStamp(&m_metaData[srcNodeIndex]);
}

#ifdef SAMSUNG_TIMESTAMP_BOOT
int64_t ExynosCameraFrame::getTimeStampBoot(uint32_t srcNodeIndex)
{
    return (int64_t)getMetaUdmSensorTimeStampBoot(&m_metaData[srcNodeIndex]);
}
#endif

void ExynosCameraFrame::getFpsRange(uint32_t *min, uint32_t *max, uint32_t srcNodeIndex)
{
    getMetaCtlAeTargetFpsRange(&m_metaData[srcNodeIndex], min, max);
}

void ExynosCameraFrame::setRequest(bool tap,
                                   bool tac,
                                   bool isp,
                                   bool scc,
                                   bool dis,
                                   bool scp)
{
    m_request[PIPE_3AP] = tap;
    m_request[PIPE_3AC] = tac;
    m_request[PIPE_ISP] = isp;
    m_request[PIPE_SCC] = scc;
    m_request[PIPE_DIS] = dis;
    m_request[PIPE_SCP] = scp;
}

void ExynosCameraFrame::setRequest(bool tap,
                                   bool tac,
                                   bool isp,
                                   bool ispp,
                                   bool ispc,
                                   bool scc,
                                   bool dis,
                                   bool scp)
{
    setRequest(tap,
               tac,
               isp,
               scc,
               dis,
               scp);

    m_request[PIPE_ISPP] = ispp;
    m_request[PIPE_ISPC] = ispc;
}

void ExynosCameraFrame::setRequest(uint32_t pipeId, bool val)
{
    switch (pipeId) {
    case PIPE_FLITE_FRONT:
    case PIPE_FLITE_REPROCESSING:
        pipeId = PIPE_FLITE;
        break;
    case PIPE_VC0_REPROCESSING:
        pipeId = PIPE_VC0;
        break;
    case PIPE_3AC_FRONT:
    case PIPE_3AC_REPROCESSING:
        pipeId = PIPE_3AC;
        break;
    case PIPE_3AP_FRONT:
    case PIPE_3AP_REPROCESSING:
        pipeId = PIPE_3AP;
        break;
    case PIPE_ISP_FRONT:
        pipeId = PIPE_ISP;
        break;
    case PIPE_ISPC_FRONT:
    case PIPE_ISPC_REPROCESSING:
        pipeId = PIPE_ISPC;
        break;
    case PIPE_SCC_FRONT:
    case PIPE_SCC_REPROCESSING:
        pipeId = PIPE_SCC;
        break;
    case PIPE_SCP_FRONT:
    case PIPE_SCP_REPROCESSING:
        pipeId = PIPE_SCP;
        break;
    default:
        break;
    }

    if ((pipeId % 100) >= MAX_NUM_PIPES)
        CLOGW("Invalid pipeId(%d)", pipeId);
    else
        m_request[INDEX(pipeId)] = val;
}

bool ExynosCameraFrame::getRequest(uint32_t pipeId)
{
    bool request = false;

    switch (pipeId) {
    case PIPE_FLITE_FRONT:
    case PIPE_FLITE_REPROCESSING:
        pipeId = PIPE_FLITE;
        break;
    case PIPE_VC0_REPROCESSING:
        pipeId = PIPE_VC0;
        break;
    case PIPE_3AC_FRONT:
    case PIPE_3AC_REPROCESSING:
        pipeId = PIPE_3AC;
        break;
    case PIPE_3AP_FRONT:
    case PIPE_3AP_REPROCESSING:
        pipeId = PIPE_3AP;
        break;
    case PIPE_ISP_FRONT:
        pipeId = PIPE_ISP;
        break;
    case PIPE_ISPC_FRONT:
    case PIPE_ISPC_REPROCESSING:
        pipeId = PIPE_ISPC;
        break;
    case PIPE_SCC_FRONT:
    case PIPE_SCC_REPROCESSING:
        pipeId = PIPE_SCC;
        break;
    case PIPE_SCP_FRONT:
    case PIPE_SCP_REPROCESSING:
        pipeId = PIPE_SCP;
        break;
    default:
        break;
    }

    if ((pipeId % 100) >= MAX_NUM_PIPES)
        CLOGW("Invalid pipeId(%d)", pipeId);
    else
        request = m_request[INDEX(pipeId)];

    return request;
}

bool ExynosCameraFrame::getIspDone(void)
{
    return m_ispDoneFlag;
}

void ExynosCameraFrame::setIspDone(bool done)
{
    m_ispDoneFlag = done;
}

bool ExynosCameraFrame::get3aaDrop()
{
    return m_3aaDropFlag;
}

void ExynosCameraFrame::set3aaDrop(bool flag)
{
    m_3aaDropFlag = flag;
}

void ExynosCameraFrame::setIspcDrop(bool flag)
{
    m_ispcDropFlag = flag;
}

bool ExynosCameraFrame::getIspcDrop(void)
{
    return m_ispcDropFlag;
}

void ExynosCameraFrame::setDisDrop(bool flag)
{
    m_disDropFlag = flag;
}

bool ExynosCameraFrame::getDisDrop(void)
{
    return m_disDropFlag;
}

bool ExynosCameraFrame::getScpDrop()
{
    return m_scpDropFlag;
}

void ExynosCameraFrame::setScpDrop(bool flag)
{
    m_scpDropFlag = flag;
}

bool ExynosCameraFrame::getSccDrop()
{
    return m_sccDropFlag;
}

void ExynosCameraFrame::setSccDrop(bool flag)
{
    m_sccDropFlag = flag;
}

uint32_t ExynosCameraFrame::getUniqueKey(void)
{
    return m_uniqueKey;
}

status_t ExynosCameraFrame::setUniqueKey(uint32_t uniqueKey)
{
    m_uniqueKey = uniqueKey;
    return NO_ERROR;
}

status_t ExynosCameraFrame::setFrameInfo(ExynosCameraParameters *obj_param, uint32_t frameCount, uint32_t frameType)
{
    status_t ret = NO_ERROR;

    m_parameters = obj_param;
    m_frameCount = frameCount;
    m_frameType = frameType;
    return ret;
}

status_t ExynosCameraFrame::setFrameMgrInfo(frame_key_queue_t *queue)
{
    status_t ret = NO_ERROR;

    m_frameQueue = queue;

    return ret;
}

void ExynosCameraFrame::setFrameType(frame_type_t frameType)
{
    m_frameType = frameType;
}

uint32_t ExynosCameraFrame::getFrameType()
{
    return m_frameType;
}

status_t ExynosCameraFrame::m_init()
{
    m_numRequestPipe = 0;
    m_numCompletePipe = 0;
    m_frameState = FRAME_STATE_READY;
    m_frameLocked = false;

    for (int i = 0; i < SRC_BUFFER_COUNT_MAX; i++) {
        m_metaDataEnable[i] = false;
        m_zoom[i] = 0;
        memset(&m_metaData[i], 0x0, sizeof(struct camera2_shot_ext));

        for (int j = 0; j < PERFRAME_NODE_GROUP_MAX; j++)
            memset(&m_node_gorup[i][j], 0x0, sizeof(struct camera2_node_group));
    }

    m_jpegSize = 0;
    m_ispDoneFlag = false;
    m_3aaDropFlag = false;
    m_ispcDropFlag = false;
    m_disDropFlag = false;
    m_scpDropFlag = false;
    m_sccDropFlag = false;

    for (int i = 0; i < MAX_NUM_PIPES; i++)
        m_request[i] = false;

    m_uniqueKey = 0;
    m_capture = 0;
    m_recording = false;
    m_preview = false;
    m_previewCb = false;
    m_serviceBayer = false;
    m_zsl = false;

#ifdef USE_FRAME_REFERENCE_COUNT
    m_refCount = 1;
#endif
    CLOGV(" Generate frame type(%d), frameCount(%d)", m_frameType, m_frameCount);

#ifdef DEBUG_FRAME_MEMORY_LEAK
    m_countLock.lock();
    m_checkLeakCount++;
    m_checkLeakFrameCount++;
    m_privateCheckLeakFrameCount = m_checkLeakFrameCount;
    CLOGI("[HalFrmCnt:F%d][LeakFrmCnt:F%lld] CONSTRUCTOR (%lld)",
           m_frameCount, m_privateCheckLeakFrameCount, m_checkLeakCount);
    m_countLock.unlock();
#endif

    m_dupBufferInfo.streamID = 0;
    m_dupBufferInfo.extScalerPipeID = 0;

    m_frameQueue = NULL;
    m_syncType = SYNC_TYPE_BASE;

    m_reprocessingZoom = 0;
    m_reprocessingFrameType = m_frameType;
    m_reprocessingSyncType = m_syncType;

    return NO_ERROR;
}

status_t ExynosCameraFrame::m_deinit()
{
    CLOGV(" Delete frame type(%d), frameCount(%d)", m_frameType, m_frameCount);
#ifdef DEBUG_FRAME_MEMORY_LEAK
    m_countLock.lock();
    m_checkLeakCount --;
    CLOGI("[HalFrmCnt:F%d][LeakFrmCnt:F%lld] DESTRUCTOR (%lld)",
           m_frameCount, m_privateCheckLeakFrameCount, m_checkLeakCount);
    m_countLock.unlock();
#endif

    if (m_frameQueue != NULL)
        m_frameQueue->pushProcessQ(&m_uniqueKey);

    List<ExynosCameraFrameEntity *>::iterator r;
    ExynosCameraFrameEntity *curEntity = NULL;
    ExynosCameraFrameEntity *tmpEntity = NULL;

    Mutex::Autolock l(m_linkageLock);
    while (!m_linkageList.empty()) {
        r = m_linkageList.begin()++;
        if (*r) {
            curEntity = *r;

            while (curEntity != NULL) {
                tmpEntity = curEntity->getNextEntity();
                CLOGV("PipeId:%d", curEntity->getPipeId());

                delete curEntity;
                curEntity = tmpEntity;
            }

        }
        m_linkageList.erase(r);
    }

    return NO_ERROR;
}

status_t ExynosCameraFrame::setRotation(uint32_t pipeId, int rotation)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        CLOGE("Could not find entity, pipeID(%d)", pipeId);
        return BAD_VALUE;
    }

    ret = entity->setRotation(rotation);
    if (ret != NO_ERROR) {
        CLOGE("pipeId(%d)->setRotation(%d) fail", pipeId, rotation);
        return ret;
    }

    return ret;
}

status_t ExynosCameraFrame::getRotation(uint32_t pipeId)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        CLOGE("Could not find entity, pipeID(%d)", pipeId);
        return BAD_VALUE;
    }

    return entity->getRotation();
}

#ifdef PERFRAME_CONTROL_FOR_FLIP
status_t ExynosCameraFrame::setFlipHorizontal(uint32_t pipeId, int flipHorizontal)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        CLOGE("Could not find entity, pipeID(%d)", pipeId);
        return BAD_VALUE;
    }

    ret = entity->setFlipHorizontal(flipHorizontal);
    if (ret != NO_ERROR) {
        CLOGE("pipeId(%d)->setRotation(%d) fail", pipeId, flipHorizontal);
        return ret;
    }

    return ret;
}

status_t ExynosCameraFrame::getFlipHorizontal(uint32_t pipeId)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        CLOGE("Could not find entity, pipeID(%d)", pipeId);
        return BAD_VALUE;
    }

    return entity->getFlipHorizontal();
}

status_t ExynosCameraFrame::setFlipVertical(uint32_t pipeId, int flipVertical)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        CLOGE("Could not find entity, pipeID(%d)", pipeId);
        return BAD_VALUE;
    }

    ret = entity->setFlipVertical(flipVertical);
    if (ret != NO_ERROR) {
        CLOGE("pipeId(%d)->setRotation(%d) fail", pipeId, flipVertical);
        return ret;
    }

    return ret;
}

status_t ExynosCameraFrame::getFlipVertical(uint32_t pipeId)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        CLOGE("Could not find entity, pipeID(%d)", pipeId);
        return BAD_VALUE;
    }

    return entity->getFlipVertical();
}
#endif

/*
 * ExynosCameraFrameEntity class
 */

ExynosCameraFrameEntity::ExynosCameraFrameEntity(
        uint32_t pipeId,
        entity_type_t type,
        entity_buffer_type_t bufType)
{
    m_pipeId = pipeId;

    if (m_setEntityType(type) != NO_ERROR)
        CLOGE2("setEntityType fail, pipeId(%d), type(%d)", pipeId, type);

    m_bufferType = bufType;
    m_entityState = ENTITY_STATE_READY;

    m_prevEntity = NULL;
    m_nextEntity = NULL;

    m_flagSpecificParent = false;
    m_parentPipeId = -1;

    m_rotation = 0;

#ifdef PERFRAME_CONTROL_FOR_FLIP
    m_flipHorizontal = 0;
    m_flipVertical = 0;
#endif
}

status_t ExynosCameraFrameEntity::m_setEntityType(entity_type_t type)
{
    status_t ret = NO_ERROR;

    m_EntityType = type;

    /* for src */
    for(int i = SRC_BUFFER_DEFAULT; i < SRC_BUFFER_COUNT_MAX; i++) {
        switch (type) {
        case ENTITY_TYPE_INPUT_ONLY:
            m_srcBufState[i] = ENTITY_BUFFER_STATE_REQUESTED;
            break;
        case ENTITY_TYPE_OUTPUT_ONLY:
            m_srcBufState[i] = ENTITY_BUFFER_STATE_NOREQ;
            break;
        case ENTITY_TYPE_INPUT_OUTPUT:
            m_srcBufState[i] = ENTITY_BUFFER_STATE_REQUESTED;
            break;
        default:
            m_srcBufState[i] = ENTITY_BUFFER_STATE_NOREQ;
            m_EntityType = ENTITY_TYPE_INVALID;
            ret = BAD_VALUE;
            break;
        }
    }

    /* for dst */
    for(int i = DST_BUFFER_DEFAULT; i < DST_BUFFER_COUNT_MAX; i++) {
        switch (type) {
        case ENTITY_TYPE_INPUT_ONLY:
            m_dstBufState[i] = ENTITY_BUFFER_STATE_NOREQ;
            break;
        case ENTITY_TYPE_OUTPUT_ONLY:
            m_dstBufState[i] = ENTITY_BUFFER_STATE_REQUESTED;
            break;
        case ENTITY_TYPE_INPUT_OUTPUT:
            m_dstBufState[i] = ENTITY_BUFFER_STATE_REQUESTED;
            break;
        default:
            m_dstBufState[i] = ENTITY_BUFFER_STATE_NOREQ;
            m_EntityType = ENTITY_TYPE_INVALID;
            ret = BAD_VALUE;
            break;
        }
    }

    return ret;
}

uint32_t ExynosCameraFrameEntity::getPipeId(void)
{
    return m_pipeId;
}

status_t ExynosCameraFrameEntity::setSrcBuf(ExynosCameraBuffer buf, uint32_t nodeIndex)
{
    status_t ret = NO_ERROR;

    if (nodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE2("Invalid buffer index, index(%d)", nodeIndex);
        return BAD_VALUE;
    }

    if (m_srcBufState[nodeIndex] == ENTITY_BUFFER_STATE_COMPLETE) {
        CLOGV2("Buffer completed, state(%d)", m_srcBufState[nodeIndex]);
        return NO_ERROR;
    }

    if (m_bufferType != ENTITY_BUFFER_DELIVERY &&
        m_srcBufState[nodeIndex] != ENTITY_BUFFER_STATE_REQUESTED) {
        CLOGE2("Invalid buffer state(%d)", m_srcBufState[nodeIndex]);
        return INVALID_OPERATION;
    }

    this->m_srcBuf[nodeIndex] = buf;

    ret = setSrcBufState(ENTITY_BUFFER_STATE_READY, nodeIndex);

    return ret;
}

status_t ExynosCameraFrameEntity::setDstBuf(ExynosCameraBuffer buf, uint32_t nodeIndex)
{
    status_t ret = NO_ERROR;

    if (nodeIndex >= DST_BUFFER_COUNT_MAX) {
        CLOGE2("Invalid buffer index, index(%d)", nodeIndex);
        return BAD_VALUE;
    }

    if (m_bufferType != ENTITY_BUFFER_DELIVERY &&
        m_dstBufState[nodeIndex] != ENTITY_BUFFER_STATE_REQUESTED) {
        CLOGE2("Invalid buffer state(%d)", m_dstBufState[nodeIndex]);
        return INVALID_OPERATION;
    }

    this->m_dstBuf[nodeIndex] = buf;
    ret = setDstBufState(ENTITY_BUFFER_STATE_READY, nodeIndex);

    return ret;
}

status_t ExynosCameraFrameEntity::getSrcBuf(ExynosCameraBuffer *buf, uint32_t nodeIndex)
{
    if (nodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE2("Invalid buffer index, index(%d)", nodeIndex);
        return BAD_VALUE;
    }

    *buf = this->m_srcBuf[nodeIndex];

    return NO_ERROR;
}

status_t ExynosCameraFrameEntity::getDstBuf(ExynosCameraBuffer *buf, uint32_t nodeIndex)
{
    if (nodeIndex >= DST_BUFFER_COUNT_MAX) {
        CLOGE2("Invalid buffer index, index(%d)", nodeIndex);
        return BAD_VALUE;
    }

    /* Comment out: It was collide with ExynosCamera's dirty dynamic bayer handling routine.
     * (make error log, but no side effect)
     * This code added for block human error.
     * I will add this code after check the side effect closely.
     */
    /*
    if (this->m_dstBuf[nodeIndex].index == -1) {
        CLOGE2("Invalid buffer index(%d)", nodeIndex);
        return BAD_VALUE;
    }
    */

    *buf = this->m_dstBuf[nodeIndex];

    return NO_ERROR;
}

status_t ExynosCameraFrameEntity::setSrcRect(ExynosRect rect, uint32_t nodeIndex)
{
    this->m_srcRect[nodeIndex] = rect;

    return NO_ERROR;
}

status_t ExynosCameraFrameEntity::setDstRect(ExynosRect rect, uint32_t nodeIndex)
{
    this->m_dstRect[nodeIndex] = rect;

    return NO_ERROR;
}

status_t ExynosCameraFrameEntity::getSrcRect(ExynosRect *rect, uint32_t nodeIndex)
{
    *rect = this->m_srcRect[nodeIndex];

    return NO_ERROR;
}

status_t ExynosCameraFrameEntity::getDstRect(ExynosRect *rect, uint32_t nodeIndex)
{
    *rect = this->m_dstRect[nodeIndex];

    return NO_ERROR;
}

status_t ExynosCameraFrameEntity::setSrcBufState(entity_buffer_state_t state, uint32_t nodeIndex)
{
    if (nodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE2("Invalid buffer index, index(%d)", nodeIndex);
        return BAD_VALUE;
    }

    if (m_srcBufState[nodeIndex] == ENTITY_BUFFER_STATE_COMPLETE) {
        CLOGV2("Buffer completed, state(%d)", m_srcBufState[nodeIndex]);
        return NO_ERROR;
    }

    m_srcBufState[nodeIndex] = state;
    return NO_ERROR;
}

status_t ExynosCameraFrameEntity::setDstBufState(entity_buffer_state_t state, uint32_t nodeIndex)
{
    if (nodeIndex >= DST_BUFFER_COUNT_MAX) {
        CLOGE2("Invalid buffer index, index(%d)", nodeIndex);
        return BAD_VALUE;
    }

    m_dstBufState[nodeIndex] = state;

    return NO_ERROR;
}

entity_buffer_state_t ExynosCameraFrameEntity::getSrcBufState(uint32_t nodeIndex)
{
    if (nodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE2("Invalid buffer index, index(%d)", nodeIndex);
        return ENTITY_BUFFER_STATE_INVALID;
    }

    return m_srcBufState[nodeIndex];
}

entity_buffer_state_t ExynosCameraFrameEntity::getDstBufState(uint32_t nodeIndex)
{
    if (nodeIndex >= DST_BUFFER_COUNT_MAX) {
        CLOGE2("Invalid buffer index, index(%d)", nodeIndex);
        return ENTITY_BUFFER_STATE_INVALID;
    }

    return m_dstBufState[nodeIndex];
}

entity_buffer_type_t ExynosCameraFrameEntity::getBufType(void)
{
    return m_bufferType;
}

status_t ExynosCameraFrameEntity::setEntityState(entity_state_t state)
{
    this->m_entityState = state;

    return NO_ERROR;
}

entity_state_t ExynosCameraFrameEntity::getEntityState(void)
{
    return this->m_entityState;
}

ExynosCameraFrameEntity *ExynosCameraFrameEntity::getPrevEntity(void)
{
    return this->m_prevEntity;
}

ExynosCameraFrameEntity *ExynosCameraFrameEntity::getNextEntity(void)
{
    return this->m_nextEntity;
}

status_t ExynosCameraFrameEntity::setPrevEntity(ExynosCameraFrameEntity *entity)
{
    this->m_prevEntity = entity;

    return NO_ERROR;
}

status_t ExynosCameraFrameEntity::setNextEntity(ExynosCameraFrameEntity *entity)
{
    this->m_nextEntity = entity;

    return NO_ERROR;
}

bool ExynosCameraFrameEntity::flagSpecficParent(void)
{
    return m_flagSpecificParent;
}

status_t ExynosCameraFrameEntity::setParentPipeId(enum pipeline parentPipeId)
{
    if (0 <= m_parentPipeId) {
        CLOGE2("m_parentPipeId(%d) is already set. parentPipeId(%d)",
             m_parentPipeId, parentPipeId);
        return BAD_VALUE;
    }

    m_flagSpecificParent = true;
    m_parentPipeId = parentPipeId;

    return NO_ERROR;
}

int ExynosCameraFrameEntity::getParentPipeId(void)
{
    return m_parentPipeId;
}

status_t ExynosCameraFrameEntity::setRotation(int rotation)
{
    m_rotation = rotation;

    return NO_ERROR;
}

int ExynosCameraFrameEntity::getRotation(void)
{
    return m_rotation;
}

#ifdef PERFRAME_CONTROL_FOR_FLIP
status_t ExynosCameraFrameEntity::setFlipHorizontal(int flipHorizontal)
{
    m_flipHorizontal = flipHorizontal;

    return NO_ERROR;
}

int ExynosCameraFrameEntity::getFlipHorizontal(void)
{
    return m_flipHorizontal;
}

status_t ExynosCameraFrameEntity::setFlipVertical(int flipVertical)
{
    m_flipVertical = flipVertical;

    return NO_ERROR;
}

int ExynosCameraFrameEntity::getFlipVertical(void)
{
    return m_flipVertical;
}
#endif

}; /* namespace android */
