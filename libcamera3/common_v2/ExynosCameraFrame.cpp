/*
**
** Copyright 2017, Samsung Electronics Co. LTD
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
#include <log/log.h>

#include "ExynosCameraFrame.h"

namespace android {

#ifdef DEBUG_FRAME_MEMORY_LEAK
unsigned long long ExynosCameraFrame::m_checkLeakCount;
unsigned long long ExynosCameraFrame::m_checkLeakFrameCount;
Mutex ExynosCameraFrame::m_countLock;
#endif

ExynosCameraFrame::ExynosCameraFrame(
        int cameraId,
        ExynosCameraConfigurations *configurations,
        ExynosCameraParameters *params,
        uint32_t frameCount,
        uint32_t frameType)
{
    m_cameraId = cameraId;
    m_configurations = configurations;
    m_parameters = params;
    m_frameCount = frameCount;
    m_frameIndex = 0;
    m_frameType = frameType;
    memset(m_name, 0x00, sizeof(m_name));

    CLOGV(" create frame type(%d), frameCount(%d)", frameType, frameCount);

    m_init();
}

ExynosCameraFrame::ExynosCameraFrame(int cameraId)
{
    m_cameraId = cameraId;
    m_configurations = NULL;
    m_parameters = NULL;
    m_frameCount = 0;
    m_frameIndex = 0;
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
    return m_privateCheckLeakCount;
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

    CLOGD("Cannot find matched entity, frameCount(%d), pipeId(%d), type(%d)", getFrameCount(), pipeId, getFrameType());

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
    Mutex::Autolock lock(m_entityLock);
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
        m_updateStatusForResultUpdate((enum pipeline)pipeId);

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
    Mutex::Autolock lock(m_entityLock);
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
    Mutex::Autolock lock(m_entityLock);
    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        CLOGE("Could not find entity, pipeID(%d)", pipeId);
        return BAD_VALUE;
    }

    *type = entity->getBufType();
    return NO_ERROR;
}

void ExynosCameraFrame::setParameters(ExynosCameraParameters *params)
{
    m_parameters = params;
}

ExynosCameraParameters * ExynosCameraFrame::getParameters()
{
    return m_parameters;
}

uint32_t ExynosCameraFrame::getFrameCount(void)
{
    return m_frameCount;
}

int32_t ExynosCameraFrame::getMetaFrameCount(uint32_t srcNodeIndex)
{
    if (srcNodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid srcNodeIndex index, max(%d) index(%d)", SRC_BUFFER_COUNT_MAX, srcNodeIndex);
        return 0;
    }

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

uint32_t ExynosCameraFrame::getNumRemainPipe(void)
{
    int numRemainPipe = m_numRequestPipe - m_numCompletePipe;

    return (numRemainPipe > 0) ? numRemainPipe : 0;
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

ExynosCameraFrameEntity *ExynosCameraFrame::getFirstEntityNotComplete(void)
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
            case ENTITY_STATE_COMPLETE:
                if (curEntity->getNextEntity() != NULL) {
                    curEntity = curEntity->getNextEntity();
                    continue;
                }
                break;
            default:
                return curEntity;
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
    Mutex::Autolock lock(m_frameStateLock);
    m_frameState = FRAME_STATE_SKIPPED;

    return NO_ERROR;
}

void ExynosCameraFrame::setFrameState(frame_status_t state)
{
    Mutex::Autolock lock(m_frameStateLock);

#ifdef USE_DEBUG_PROPERTY
    if (state == FRAME_STATE_COMPLETE &&
            state != m_frameState) {
        m_completeTimestamp = systemTime(SYSTEM_TIME_BOOTTIME);

        CLOG_PERFORMANCE(FPS, getCameraId(),
                getFactoryType(), DURATION,
                FRAME_COMPLETE, 0, getRequestKey(), this);
    }
    m_previousFrameState = m_frameState;
#endif

    /* TODO: We need state machine */
    if (state > FRAME_STATE_INVALID)
        m_frameState = FRAME_STATE_INVALID;
    else
        m_frameState = state;
}

frame_status_t ExynosCameraFrame::getFrameState(void)
{
    Mutex::Autolock lock(m_frameStateLock);
    return m_frameState;
}

bool ExynosCameraFrame::checkFrameState(frame_status_t state)
{
    Mutex::Autolock lock(m_frameStateLock);
    return (m_frameState == state) ? true : false;
}

#ifdef USE_DEBUG_PROPERTY
void ExynosCameraFrame::getEntityInfoStr(String8 &str)
{
    List<ExynosCameraFrameEntity *>::iterator r;
    ExynosCameraFrameEntity *curEntity = NULL;
    int listSize = 0;

    Mutex::Autolock l(m_linkageLock);
    if (m_linkageList.empty()) return;

    str.append("Entity[");
    listSize = m_linkageList.size();
    r = m_linkageList.begin();

    for (int i = 0; i < listSize; i++) {
        curEntity = *r;
        if (curEntity == NULL) return;

        str.appendFormat("(P%d(%d))",
             curEntity->getPipeId(), curEntity->getEntityState());

        curEntity = curEntity->getNextEntity();
        while (curEntity != NULL) {
            str.appendFormat("~>(P%d(%d))",
                    curEntity->getPipeId(), curEntity->getEntityState());
            curEntity = curEntity->getNextEntity();
        }

        if ((i + 1) < listSize) str.append("->");
        r++;
    }
    str.append("]");

    return;
}
#endif

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

    CLOGD(" FrameCount(%d(%d)), type(%d), state(%d), request(%d), complete(%d)",
            getFrameCount(), getMetaFrameCount(), getFrameType(), getFrameState(),
            m_numRequestPipe, m_numCompletePipe);

    for (int i = 0; i < listSize; i++) {
        curEntity = *r;
        if (curEntity == NULL) {
            CLOGE("curEntity is NULL, index(%d)", i);
            return;
        }

        CLOGD("sibling id(%d), state(%d)",
             curEntity->getPipeId(), curEntity->getEntityState());

        curEntity = curEntity->getNextEntity();
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

    CLOGD(" FrameCount(%d(%d)), type(%d), state(%d), request(%d), complete(%d)",
             getFrameCount(), getMetaFrameCount(), getFrameType(), getFrameState(),
             m_numRequestPipe, m_numCompletePipe);

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

        curEntity = curEntity->getNextEntity();
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
    m_frameLocked = true;
}

void ExynosCameraFrame::frameUnlock(void)
{
    m_frameLocked = false;
}

bool ExynosCameraFrame::getFrameLockState(void)
{
    return m_frameLocked;
}

status_t ExynosCameraFrame::initMetaData(struct camera2_shot_ext *shot, uint32_t srcNodeIndex)
{
    status_t ret = NO_ERROR;

    if (srcNodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid srcNodeIndex index, max(%d) index(%d)", SRC_BUFFER_COUNT_MAX, srcNodeIndex);
        return BAD_VALUE;
    }

    if (shot != NULL) {
        CLOGV(" initialize shot_ext");
        memcpy(&m_metaData[srcNodeIndex], shot, sizeof(struct camera2_shot_ext));
    }

    if (m_parameters != NULL) {
        ret = m_parameters->duplicateCtrlMetadata(&m_metaData[srcNodeIndex]);
        if (ret != NO_ERROR) {
            CLOGE("duplicate Ctrl metadata fail");
            return INVALID_OPERATION;
        }
    }

    return ret;
}

/* for const read operation without memcpy */
const struct camera2_shot_ext *ExynosCameraFrame::getConstMeta(uint32_t srcNodeIndex)
{
    if (srcNodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid srcNodeIndex index, max(%d) index(%d)", SRC_BUFFER_COUNT_MAX, srcNodeIndex);
        return &m_metaData[0];
    }

    return &m_metaData[srcNodeIndex];
}

status_t ExynosCameraFrame::getMetaData(struct camera2_shot_ext *shot, uint32_t srcNodeIndex)
{
    char *srcAddr = NULL;
    char *dstAddr = NULL;
    struct camera2_entry_ctl vendorEntry;
    size_t size = 0;

    if (shot == NULL) {
        CLOGE(" buffer is NULL");
        return BAD_VALUE;
    }

    if (srcNodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid srcNodeIndex index, max(%d) index(%d)", SRC_BUFFER_COUNT_MAX, srcNodeIndex);
        return BAD_VALUE;
    }

    /* camera2_entry_ctl back-up */
    memcpy(&vendorEntry, &shot->shot.ctl.vendor_entry, sizeof(camera2_entry_ctl));

    /* camera2_stream region should not be overwritten */
    srcAddr = ((char *) &m_metaData[srcNodeIndex])  + sizeof(struct camera2_stream);
    dstAddr = ((char *) shot)                       + sizeof(struct camera2_stream);
    size    = sizeof(struct camera2_shot_ext)       - sizeof(struct camera2_stream);
    memcpy(dstAddr, srcAddr, size);
    memcpy(&shot->shot.ctl.vendor_entry, &vendorEntry, sizeof(camera2_entry_ctl));

    return NO_ERROR;
}

status_t ExynosCameraFrame::setMetaData(struct camera2_shot_ext *shot, uint32_t srcNodeIndex)
{
    if (shot == NULL) {
        CLOGE(" buffer is NULL");
        return BAD_VALUE;
    }

    if (srcNodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid srcNodeIndex index, max(%d) index(%d)", SRC_BUFFER_COUNT_MAX, srcNodeIndex);
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

    if (srcNodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid srcNodeIndex index, max(%d) index(%d)", SRC_BUFFER_COUNT_MAX, srcNodeIndex);
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

    if (srcNodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid srcNodeIndex index, max(%d) index(%d)", SRC_BUFFER_COUNT_MAX, srcNodeIndex);
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

    if (srcNodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid srcNodeIndex index, max(%d) index(%d)", SRC_BUFFER_COUNT_MAX, srcNodeIndex);
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

    if (srcNodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid srcNodeIndex index, max(%d) index(%d)", SRC_BUFFER_COUNT_MAX, srcNodeIndex);
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

    if (srcNodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid srcNodeIndex index, max(%d) index(%d)", SRC_BUFFER_COUNT_MAX, srcNodeIndex);
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

    if (srcNodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid srcNodeIndex index, max(%d) index(%d)", SRC_BUFFER_COUNT_MAX, srcNodeIndex);
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

    if (srcNodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid srcNodeIndex index, max(%d) index(%d)", SRC_BUFFER_COUNT_MAX, srcNodeIndex);
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

    if (srcNodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid srcNodeIndex index, max(%d) index(%d)", SRC_BUFFER_COUNT_MAX, srcNodeIndex);
        return BAD_VALUE;
    }

    memcpy(udm, &m_metaData[srcNodeIndex].shot.udm, sizeof(struct camera2_udm));

    return NO_ERROR;
}

status_t ExynosCameraFrame::setMetaDataEnable(bool flag, uint32_t srcNodeIndex)
{
    if (srcNodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid srcNodeIndex index, max(%d) index(%d)", SRC_BUFFER_COUNT_MAX, srcNodeIndex);
        return BAD_VALUE;
    }

    m_metaDataEnable[srcNodeIndex] = flag;
    return NO_ERROR;
}

bool ExynosCameraFrame::getMetaDataEnable(uint32_t srcNodeIndex)
{
    if (srcNodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid srcNodeIndex index, max(%d) index(%d)", SRC_BUFFER_COUNT_MAX, srcNodeIndex);
        return false;
    }

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

status_t ExynosCameraFrame::getZoomRect(ExynosRect *zoomRect, uint32_t srcNodeIndex)
{
    if (srcNodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid srcNodeIndex index, max(%d) index(%d)", SRC_BUFFER_COUNT_MAX, srcNodeIndex);
        return BAD_VALUE;
    }

    zoomRect->x = m_zoomRect[srcNodeIndex].x;
    zoomRect->y = m_zoomRect[srcNodeIndex].y;
    zoomRect->w = m_zoomRect[srcNodeIndex].w;
    zoomRect->h = m_zoomRect[srcNodeIndex].h;

    return NO_ERROR;
}

status_t ExynosCameraFrame::setZoomRect(ExynosRect zoomRect, uint32_t srcNodeIndex)
{
    if (srcNodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid srcNodeIndex index, max(%d) index(%d)", SRC_BUFFER_COUNT_MAX, srcNodeIndex);
        return BAD_VALUE;
    }

    m_zoomRect[srcNodeIndex].x = zoomRect.x;
    m_zoomRect[srcNodeIndex].y = zoomRect.y;
    m_zoomRect[srcNodeIndex].w = zoomRect.w;
    m_zoomRect[srcNodeIndex].h = zoomRect.h;

    return NO_ERROR;
}

status_t ExynosCameraFrame::getActiveZoomRect(ExynosRect *zoomRect, uint32_t srcNodeIndex)
{
    if (srcNodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid srcNodeIndex index, max(%d) index(%d)", SRC_BUFFER_COUNT_MAX, srcNodeIndex);
        return BAD_VALUE;
    }

    zoomRect->x = m_activeZoomRect[srcNodeIndex].x;
    zoomRect->y = m_activeZoomRect[srcNodeIndex].y;
    zoomRect->w = m_activeZoomRect[srcNodeIndex].w;
    zoomRect->h = m_activeZoomRect[srcNodeIndex].h;

    return NO_ERROR;
}

status_t ExynosCameraFrame::setActiveZoomRect(ExynosRect zoomRect, uint32_t srcNodeIndex)
{
    if (srcNodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid srcNodeIndex index, max(%d) index(%d)", SRC_BUFFER_COUNT_MAX, srcNodeIndex);
        return BAD_VALUE;
    }

    m_activeZoomRect[srcNodeIndex].x = zoomRect.x;
    m_activeZoomRect[srcNodeIndex].y = zoomRect.y;
    m_activeZoomRect[srcNodeIndex].w = zoomRect.w;
    m_activeZoomRect[srcNodeIndex].h = zoomRect.h;

    return NO_ERROR;
}

float ExynosCameraFrame::getZoomRatio(uint32_t srcNodeIndex)
{
    if (srcNodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid srcNodeIndex index, max(%d) index(%d)", SRC_BUFFER_COUNT_MAX, srcNodeIndex);
        return BAD_VALUE;
    }

    return m_zoomRatio[srcNodeIndex];
}

status_t ExynosCameraFrame::setZoomRatio(float zoomRatio, uint32_t srcNodeIndex)
{
    if (srcNodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid srcNodeIndex index, max(%d) index(%d)", SRC_BUFFER_COUNT_MAX, srcNodeIndex);
        return BAD_VALUE;
    }

    m_zoomRatio[srcNodeIndex] = zoomRatio;

    return NO_ERROR;
}

float ExynosCameraFrame::getActiveZoomRatio(uint32_t srcNodeIndex)
{
    if (srcNodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid srcNodeIndex index, max(%d) index(%d)", SRC_BUFFER_COUNT_MAX, srcNodeIndex);
        return BAD_VALUE;
    }

    return m_activeZoomRatio[srcNodeIndex];
}

status_t ExynosCameraFrame::setActiveZoomRatio(float zoomRatio, uint32_t srcNodeIndex)
{
    if (srcNodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid srcNodeIndex index, max(%d) index(%d)", SRC_BUFFER_COUNT_MAX, srcNodeIndex);
        return BAD_VALUE;
    }

    m_activeZoomRatio[srcNodeIndex] = zoomRatio;

    return NO_ERROR;
}

float ExynosCameraFrame::getAppliedZoomRatio(uint32_t srcNodeIndex)
{
    if (srcNodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid srcNodeIndex index, max(%d) index(%d)", SRC_BUFFER_COUNT_MAX, srcNodeIndex);
        return BAD_VALUE;
    }

    return m_appliedZoomRatio[srcNodeIndex];
}

status_t ExynosCameraFrame::setAppliedZoomRatio(float zoomRatio, uint32_t srcNodeIndex)
{
    if (srcNodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid srcNodeIndex index, max(%d) index(%d)", SRC_BUFFER_COUNT_MAX, srcNodeIndex);
        return BAD_VALUE;
    }

    m_appliedZoomRatio[srcNodeIndex] = zoomRatio;

    return NO_ERROR;
}

int ExynosCameraFrame::getActiveZoomMargin(uint32_t srcNodeIndex)
{
    if (srcNodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid srcNodeIndex index, max(%d) index(%d)", SRC_BUFFER_COUNT_MAX, srcNodeIndex);
        return BAD_VALUE;
    }

    return m_activeZoomMargin[srcNodeIndex];
}

status_t ExynosCameraFrame::setActiveZoomMargin(int zoomMargin, uint32_t srcNodeIndex)
{
    if (srcNodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid srcNodeIndex index, max(%d) index(%d)", SRC_BUFFER_COUNT_MAX, srcNodeIndex);
        return BAD_VALUE;
    }

    m_activeZoomMargin[srcNodeIndex] = zoomMargin;

    return NO_ERROR;
}

uint32_t ExynosCameraFrame::getSetfile(uint32_t srcNodeIndex)
{
    if (srcNodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid srcNodeIndex index, max(%d) index(%d)", SRC_BUFFER_COUNT_MAX, srcNodeIndex);
        return BAD_VALUE;
    }

    return m_metaData[srcNodeIndex].setfile;
}

status_t ExynosCameraFrame::setSetfile(uint32_t setfile, uint32_t srcNodeIndex)
{
    if (srcNodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid srcNodeIndex index, max(%d) index(%d)", SRC_BUFFER_COUNT_MAX, srcNodeIndex);
        return BAD_VALUE;
    }

    m_metaData[srcNodeIndex].setfile = setfile;

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

    if (srcNodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid srcNodeIndex index, max(%d) index(%d)", SRC_BUFFER_COUNT_MAX, srcNodeIndex);
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

    if (srcNodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid srcNodeIndex index, max(%d) index(%d)", SRC_BUFFER_COUNT_MAX, srcNodeIndex);
        return BAD_VALUE;
    }

    memcpy(&m_node_gorup[srcNodeIndex][index], node_group, sizeof(struct camera2_node_group));

    return NO_ERROR;
}

void ExynosCameraFrame::dumpNodeGroupInfo(const char *name, uint32_t srcNodeIndex)
{
    if (srcNodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid srcNodeIndex index, max(%d) index(%d)", SRC_BUFFER_COUNT_MAX, srcNodeIndex);
        return;
    }

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

void ExynosCameraFrame::setCameraId(int cameraId)
{
    m_cameraId = cameraId;
#ifdef USE_DEBUG_PROPERTY
    m_previousFrameState = FRAME_STATE_READY;
    m_createTimestamp = systemTime(SYSTEM_TIME_BOOTTIME);
    m_completeTimestamp = 0LL;
#endif
}

int ExynosCameraFrame::getCameraId(void)
{
    return m_cameraId;
}

void ExynosCameraFrame::setFactoryType(enum FRAME_FACTORY_TYPE factoryType)
{
    m_factoryType = factoryType;
}

enum FRAME_FACTORY_TYPE ExynosCameraFrame::getFactoryType(void) const
{
    return m_factoryType;
}

void ExynosCameraFrame::setRequestKey(uint32_t requestKey)
{
    m_requestKey = requestKey;
}

uint32_t ExynosCameraFrame::getRequestKey(void) const
{
    return m_requestKey;
}

void ExynosCameraFrame::setFrameIndex(int index)
{
    m_frameIndex = index;
}

int ExynosCameraFrame::getFrameIndex(void)
{
    return m_frameIndex;
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
    if (srcNodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid srcNodeIndex index, max(%d) index(%d)", SRC_BUFFER_COUNT_MAX, srcNodeIndex);
        return BAD_VALUE;
    }

    return (int64_t)getMetaDmSensorTimeStamp(&m_metaData[srcNodeIndex]);
}

int64_t ExynosCameraFrame::getTimeStampBoot(uint32_t srcNodeIndex)
{
    if (srcNodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid srcNodeIndex index, max(%d) index(%d)", SRC_BUFFER_COUNT_MAX, srcNodeIndex);
        return BAD_VALUE;
    }

    return (int64_t)getMetaUdmSensorTimeStampBoot(&m_metaData[srcNodeIndex]);
}

void ExynosCameraFrame::getFpsRange(uint32_t *min, uint32_t *max, uint32_t srcNodeIndex)
{
    if (srcNodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid srcNodeIndex index, max(%d) index(%d)", SRC_BUFFER_COUNT_MAX, srcNodeIndex);
        return;
    }

    getMetaCtlAeTargetFpsRange(&m_metaData[srcNodeIndex], min, max);
}

void ExynosCameraFrame::setFpsRange(uint32_t min, uint32_t max, uint32_t srcNodeIndex)
{
    if (srcNodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE("Invalid srcNodeIndex index, max(%d) index(%d)", SRC_BUFFER_COUNT_MAX, srcNodeIndex);
        return;
    }

    setMetaCtlAeTargetFpsRange(&m_metaData[srcNodeIndex], min, max);
    setMetaCtlSensorFrameDuration(&m_metaData[srcNodeIndex], (uint64_t)((1000 * 1000 * 1000) / (uint64_t)max));
}

bool ExynosCameraFrame::getBackupRequest(uint32_t pipeId)
{
    bool request = false;

    if ((pipeId % 100) >= MAX_NUM_PIPES) {
        CLOGE("[F%d]Invalid pipeId(%d)", m_frameCount, pipeId);
    } else {
        request = m_backupRequest[INDEX(pipeId)];
    }

    return request;
}

void ExynosCameraFrame::backupRequest(void)
{
    for (int i = 0; i < MAX_NUM_PIPES; i++)
        m_backupRequest[i] = m_request[i];
}

void ExynosCameraFrame::restoreRequest(void)
{
    for (int i = 0; i < MAX_NUM_PIPES; i++)
        m_request[i] = m_backupRequest[i];
}

/*
 * Based on registered request values, this API will control specific request to set.
 * and also reverse another request(true) to set by !val.
 *  - This function must be called after backupRequest().
 *  - This function returns the flag that whether another request changed or not.
 *
 * ex. existed settting.
 *    PIPE_3AC_REPROCESSING = true
 *    PIPE_JPEG_REPROCESSING = true
 *    PIPE_THUMB_REPROCESSING = true
 *
 *   If it happened calling reverseSpecificRequest(PIPE_3AC_REPROCESSING, true),
 *    PIPE_3AC_REPROCESSING = true
 *    PIPE_JPEG_REPROCESSING = false  (!true)
 *    PIPE_THUMB_REPROCESSING = false (!true)
 *
 *   If it happened calling reverseSpecificRequest(PIPE_3AC_REPROCESSING, false),
 *    PIPE_3AC_REPROCESSING = false
 *    PIPE_JPEG_REPROCESSING = true  (!false)
 *    PIPE_THUMB_REPROCESSING = true (!false)
 */
bool ExynosCameraFrame::reverseExceptForSpecificReq(uint32_t pipeId, bool val)
{
    bool changed = false;

    for (int i = 0; i < MAX_NUM_PIPES; i++) {
        if (INDEX(pipeId) == INDEX(i)) {
            m_request[i] = val;
        } else if (m_backupRequest[i]) {
            m_request[i] = !val;
            changed = true;
        }
    }

    return changed;
}

void ExynosCameraFrame::setRequest(uint32_t pipeId, bool val)
{
    if ((pipeId % 100) >= MAX_NUM_PIPES) {
        CLOGE("[F%d]Invalid pipeId(%d)", m_frameCount, pipeId);
    } else {
        m_request[INDEX(pipeId)] = val;
    }
}

bool ExynosCameraFrame::getRequest(uint32_t pipeId)
{
    bool request = false;

    if ((pipeId % 100) >= MAX_NUM_PIPES) {
        CLOGE("[F%d]Invalid pipeId(%d)", m_frameCount, pipeId);
    } else {
        request = m_request[INDEX(pipeId)];
    }

    return request;
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

status_t ExynosCameraFrame::setFrameInfo(ExynosCameraConfigurations *configurations, ExynosCameraParameters *params,
                                         uint32_t frameCount, uint32_t frameType)
{
    status_t ret = NO_ERROR;

    m_configurations = configurations;
    m_parameters = params;
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
        m_zoomRatio[i] = 1.0f;
        m_activeZoomRatio[i] = 1.0f;
        m_appliedZoomRatio[i]= 1.0f;
        m_activeZoomMargin[i]= 0;

        memset(&m_zoomRect[i], 0x0, sizeof(ExynosRect));
        memset(&m_activeZoomRect[i], 0x0, sizeof(ExynosRect));
        memset(&m_metaData[i], 0x0, sizeof(struct camera2_shot_ext));

        for (int j = 0; j < PERFRAME_NODE_GROUP_MAX; j++)
            memset(&m_node_gorup[i][j], 0x0, sizeof(struct camera2_node_group));
    }

    m_jpegSize = 0;

    for (int i = 0; i < MAX_NUM_PIPES; i++) {
        m_request[i] = false;
        m_backupRequest[i] = false;
    }

    m_uniqueKey = 0;
    m_yuvStallPortEnable = false;

    for (int i = 0; i < STREAM_TYPE_MAX; i++) {
        m_stream[i] = false;
    }

    m_refCount = 1;
    CLOGV(" Generate frame type(%d), frameCount(%d)", m_frameType, m_frameCount);

#ifdef DEBUG_FRAME_MEMORY_LEAK
    m_privateCheckLeakCount = 0;
    m_countLock.lock();
    m_checkLeakCount++;
    m_checkLeakFrameCount++;
    m_privateCheckLeakCount = m_checkLeakCount;
    CLOGI("[HalFrmCnt:F%d][LeakFrmCnt:F%lld] CONSTRUCTOR (%lld)",
           m_frameCount, m_privateCheckLeakFrameCount, m_checkLeakCount);
    m_countLock.unlock();
#endif

    m_dupBufferInfo.streamID = 0;
    m_dupBufferInfo.extScalerPipeID = 0;

    m_frameQueue = NULL;

#ifdef CORRECT_TIMESTAMP_FOR_SENSORFUSION
    m_adjustedTimestampFlag = false;
#endif

    m_specialCaptureStep = 0;
    m_hasRequest = false;
    m_updateResult = false;

#ifdef SAMSUNG_TN_FEATURE
    for (int i = 0; i < MAX_PIPE_UNI_NUM; i++) {
        m_scenario[i] = -1;
    }
#endif

    for (int i = 0; i < RESULT_UPDATE_TYPE_MAX; i++) {
        m_resultUpdatePipeId[i] = MAX_PIPE_NUM;
        m_resultUpdateStatus[i] = RESULT_UPDATE_STATUS_NONE;
    }

    m_stateInSelector = FRAME_STATE_IN_SELECTOR_BASE;

    m_needDynamicBayer = false;
    m_releaseDepthBufferFlag = true;
    m_streamTimeStamp = 0;
    m_bufferDondeIndex = -1;

#ifdef USE_DEBUG_PROPERTY
    m_createTimestamp = 0LL;
    m_completeTimestamp = 0LL;
    m_previousFrameState = FRAME_STATE_INVALID;
#endif

    m_bvOffset = 0;
    m_factoryType = FRAME_FACTORY_TYPE_MAX;
    m_requestKey = 0;

    return NO_ERROR;
}

status_t ExynosCameraFrame::m_deinit()
{
    CLOGV(" Delete frame type(%d), frameCount(%d)", m_frameType, m_frameCount);
#ifdef DEBUG_FRAME_MEMORY_LEAK
    m_countLock.lock();
    m_checkLeakCount --;
    CLOGI("[HalFrmCnt:F%d][LeakFrmCnt:F%lld] DESTRUCTOR (%lld)",
           m_frameCount, m_privateCheckLeakCount, m_checkLeakCount);
    m_countLock.unlock();
#endif

    if (isComplete() == true && isCompleteForResultUpdate() == false) {
        m_dumpStatusForResultUpdate();
    }

    if (m_frameQueue != NULL)
        m_frameQueue->pushProcessQ(&m_uniqueKey);

    List<ExynosCameraFrameEntity *>::iterator r;
    ExynosCameraFrameEntity *curEntity = NULL;
    ExynosCameraFrameEntity *tmpEntity = NULL;

    {
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
    }

    m_selectorTagQueueLock.lock();
    m_selectorTagQueue.clear();
    m_selectorTagQueueLock.unlock();

    return NO_ERROR;
}

void ExynosCameraFrame::m_updateStatusForResultUpdate(enum pipeline pipeId)
{
    for (int i = RESULT_UPDATE_TYPE_PARTIAL; i < RESULT_UPDATE_TYPE_MAX; i++) {
        if (m_resultUpdatePipeId[i] == pipeId
            && m_resultUpdateStatus[i] == RESULT_UPDATE_STATUS_REQUIRED) {
            switch (i) {
            case RESULT_UPDATE_TYPE_PARTIAL:
            case RESULT_UPDATE_TYPE_ALL:
                m_resultUpdateStatus[i] = RESULT_UPDATE_STATUS_READY;
                break;
            case RESULT_UPDATE_TYPE_BUFFER:
                m_resultUpdateStatus[i] = RESULT_UPDATE_STATUS_DONE;
                break;
            default:
                /* No operation */
                break;
            }
        }
    }
}

void ExynosCameraFrame::m_dumpStatusForResultUpdate(void)
{
    for (int i = RESULT_UPDATE_TYPE_PARTIAL; i < RESULT_UPDATE_TYPE_MAX; i++) {
        CLOGD("[F%d T%d]ResultType %2d PipeId %3d ResultUpdateStatus %2d",
                m_frameCount, m_frameType,
                i, m_resultUpdatePipeId[i], m_resultUpdateStatus[i]);
    }

    printNotDoneEntity();
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
    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        CLOGE("Could not find entity, pipeID(%d)", pipeId);
        return BAD_VALUE;
    }

    return entity->getRotation();
}

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
    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        CLOGE("Could not find entity, pipeID(%d)", pipeId);
        return BAD_VALUE;
    }

    return entity->getFlipVertical();
}

#ifdef SAMSUNG_TN_FEATURE
void ExynosCameraFrame::setPPScenario(int index, int scenario)
{
    if (index < MAX_PIPE_UNI_NUM) {
        m_scenario[index] = scenario;
    }
}

int32_t ExynosCameraFrame::getPPScenario(int pipeId)
{
    switch (pipeId) {
    case PIPE_PP_UNI_REPROCESSING:
    case PIPE_PP_UNI_REPROCESSING2:
        return m_scenario[pipeId - PIPE_PP_UNI_REPROCESSING];
        break;
    case PIPE_PP_UNI:
    case PIPE_PP_UNI2:
    case PIPE_PP_UNI3:
        return m_scenario[pipeId - PIPE_PP_UNI];
        break;
    default:
        return -1;
        break;
    }
}

int32_t ExynosCameraFrame::getPPScenarioIndex(int scenario)
{
    int index = -1;
    for (int i = 0; i < MAX_PIPE_UNI_NUM; i++) {
        if (m_scenario[i] == scenario) {
            index = i;
            break;
        }
    }

    return index;
}

bool ExynosCameraFrame::hasPPScenario(int scenario)
{
    int ret = false;
    for (int i = 0; i < MAX_PIPE_UNI_NUM; i++) {
        if (m_scenario[i] == scenario) {
            ret = true;
            break;
        }
    }

    return ret;
}

bool ExynosCameraFrame::isLastPPScenarioPipe(int pipeId)
{
    int ret = false;
    int index = pipeId - PIPE_PP_UNI;

    if (index < 0 && index >= MAX_PIPE_UNI_NUM) {
        return false;
    }

    if (index == MAX_PIPE_UNI_NUM - 1) {
        ret = true;
    } else if (m_scenario[index + 1] == -1) {
        ret = true;
    }

    return ret;
}
#endif

void ExynosCameraFrame::setStreamRequested(int stream, bool flag)
{
    if (stream < STREAM_TYPE_MAX) {
        m_stream[stream] = flag;
    }
}

bool ExynosCameraFrame::getStreamRequested(int stream)
{
    if (stream < STREAM_TYPE_MAX) {
        return m_stream[stream];
    } else {
        return false;
    }
}

void ExynosCameraFrame::setPipeIdForResultUpdate(result_update_type_t resultType, enum pipeline pipeId)
{
    m_resultUpdatePipeId[resultType] = pipeId;
}

int ExynosCameraFrame::getPipeIdForResultUpdate(result_update_type_t resultType)
{
    return m_resultUpdatePipeId[resultType];
}

void ExynosCameraFrame::setStatusForResultUpdate(result_update_type_t resultType, result_update_status_t resultStatus)
{
    m_resultUpdateStatus[resultType] = resultStatus;
}

int ExynosCameraFrame::getStatusForResultUpdate(result_update_type_t resultType)
{
    return m_resultUpdateStatus[resultType];
}

bool ExynosCameraFrame::isReadyForResultUpdate(result_update_type_t resultType)
{
    bool isNeedResultUpdate = false;

    isNeedResultUpdate = (m_resultUpdateStatus[resultType] == RESULT_UPDATE_STATUS_READY);

    CLOGV("[F%d]ResultType %d ResultStatus %d Update %d",
            m_frameCount, resultType, m_resultUpdateStatus[resultType], isNeedResultUpdate);

    return isNeedResultUpdate;
}

bool ExynosCameraFrame::isCompleteForResultUpdate(void)
{
    bool isPartialResultDone = false;
    bool isBufferDone = false;
    bool isAllResultDone = false;

    isPartialResultDone = (m_resultUpdateStatus[RESULT_UPDATE_TYPE_PARTIAL] == RESULT_UPDATE_STATUS_NONE)
                          || (m_resultUpdateStatus[RESULT_UPDATE_TYPE_PARTIAL] == RESULT_UPDATE_STATUS_DONE);
    isBufferDone        = (m_resultUpdateStatus[RESULT_UPDATE_TYPE_BUFFER] == RESULT_UPDATE_STATUS_NONE)
                          || (m_resultUpdateStatus[RESULT_UPDATE_TYPE_BUFFER] == RESULT_UPDATE_STATUS_DONE);
    isAllResultDone     = (m_resultUpdateStatus[RESULT_UPDATE_TYPE_ALL] == RESULT_UPDATE_STATUS_NONE)
                          || (m_resultUpdateStatus[RESULT_UPDATE_TYPE_ALL] == RESULT_UPDATE_STATUS_DONE);

    return (isPartialResultDone && isBufferDone && isAllResultDone);
}

#ifdef USE_DUAL_CAMERA
bool ExynosCameraFrame::isSlaveFrame(void)
{
    switch (m_frameType) {
    case FRAME_TYPE_PREVIEW_SLAVE:
    case FRAME_TYPE_PREVIEW_DUAL_SLAVE:
    case FRAME_TYPE_REPROCESSING_SLAVE:
    case FRAME_TYPE_REPROCESSING_DUAL_SLAVE:
    case FRAME_TYPE_INTERNAL_SLAVE:
    case FRAME_TYPE_TRANSITION_SLAVE:
        return true;
    default:
        return false;
    }
}
#endif

/* selector tag helper functions */
void ExynosCameraFrame::lockSelectorTagList(void)
{
    m_selectorTagQueueLock.lock();
}

void ExynosCameraFrame::unlockSelectorTagList(void)
{
    m_selectorTagQueueLock.unlock();
}

status_t ExynosCameraFrame::addSelectorTag(int selectorId, int pipeId, int bufPos, bool isSrc)
{
    Mutex::Autolock l(m_selectorTagQueueLock);

    return addRawSelectorTag(selectorId, pipeId, bufPos, isSrc);
}

bool ExynosCameraFrame::findSelectorTag(ExynosCameraFrameSelectorTag *compareTag)
{
    Mutex::Autolock l(m_selectorTagQueueLock);

    return findRawSelectorTag(compareTag);
}

bool ExynosCameraFrame::removeSelectorTag(ExynosCameraFrameSelectorTag *removeTag)
{
    Mutex::Autolock l(m_selectorTagQueueLock);

    return removeRawSelectorTag(removeTag);
}

bool ExynosCameraFrame::getFirstSelectorTag(ExynosCameraFrameSelectorTag *tag)
{
    Mutex::Autolock l(m_selectorTagQueueLock);

    return getFirstRawSelectorTag(tag);
}

void ExynosCameraFrame::setStateInSelector(FRAME_STATE_IN_SELECTOR_T state)
{
    Mutex::Autolock l(m_selectorTagQueueLock);

    setRawStateInSelector(state);
}

FRAME_STATE_IN_SELECTOR_T ExynosCameraFrame::getStateInSelector(void)
{
    Mutex::Autolock l(m_selectorTagQueueLock);

    return getRawStateInSelector();
}

/*
 * It needs lock.
 * It appends the new tag to this frame.
 */
status_t ExynosCameraFrame::addRawSelectorTag(int selectorId, int pipeId, int bufPos, bool isSrc)
{
    ExynosCameraFrameSelectorTag tempTag;

    tempTag.selectorId = selectorId;
    tempTag.pipeId = pipeId;
    tempTag.bufPos = bufPos;
    tempTag.isSrc = isSrc;

    m_selectorTagQueue.push_back(tempTag);

    return NO_ERROR;
}

/*
 * It needs lock.
 * It returns first tag to match with compareTag.
 */
bool ExynosCameraFrame::findRawSelectorTag(ExynosCameraFrameSelectorTag *compareTag)
{
    selector_tag_queue_t::iterator r;
    bool found = false;
    selector_tag_queue_t *list = getSelectorTagList();

    if (list->empty())
        return false;

    r = list->begin()++;

    do {
        if ((r->selectorId == compareTag->selectorId || compareTag->selectorId < 0)
                && (r->pipeId == compareTag->pipeId || compareTag->pipeId < 0)
                && (r->bufPos == compareTag->bufPos || compareTag->bufPos < 0)
                && (r->isSrc == compareTag->isSrc || compareTag->isSrc < 0)) {
            *compareTag = *r;
            found = true;
            break;
        }

        r++;
    } while (r != list->end());

    return found;
}

/*
 * It needs lock.
 * It removes the existing tag from this frame.
 */
bool ExynosCameraFrame::removeRawSelectorTag(ExynosCameraFrameSelectorTag *removeTag)
{
    selector_tag_queue_t::iterator r;
    bool removed = false;
    selector_tag_queue_t *list = getSelectorTagList();

    if (list->empty())
        return false;

    r = list->begin()++;

    do {
        if (*r == *removeTag) {
            r = list->erase(r);
            removed = true;
            break;
        }

        r++;
    } while (r != list->end());

    return removed;
}

/*
 * It needs lock.
 * It returns the first tag from this frame.
 */
bool ExynosCameraFrame::getFirstRawSelectorTag(ExynosCameraFrameSelectorTag *tag)
{
    selector_tag_queue_t::iterator r;
    selector_tag_queue_t *list = getSelectorTagList();

    if (list->empty())
        return false;

    r = list->begin()++;
    *tag = *r;

    return true;
}

/*
 * It needs lock.
 * It updates the state which this frame is processing on the selector.
 */
void ExynosCameraFrame::setRawStateInSelector(FRAME_STATE_IN_SELECTOR_T state)
{
    m_stateInSelector = state;
}

/*
 * It needs lock.
 * It returns the state which this frame is processing on the selector.
 */
FRAME_STATE_IN_SELECTOR_T ExynosCameraFrame::getRawStateInSelector(void)
{
    return m_stateInSelector;
}

selector_tag_queue_t *ExynosCameraFrame::getSelectorTagList(void)
{
    return &m_selectorTagQueue;
}

void ExynosCameraFrame::setNeedDynamicBayer(bool set)
{
    m_needDynamicBayer = set;
}

bool ExynosCameraFrame::getNeedDynamicBayer(void)
{
    return m_needDynamicBayer;
}

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

    m_flipHorizontal = 0;
    m_flipVertical = 0;

#ifdef USE_DEBUG_PROPERTY
    m_processTimestamp = 0LL;
    m_doneTimestamp = 0LL;
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
    if (nodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE2("Invalid buffer index, index(%d)", nodeIndex);
        return BAD_VALUE;
    }

    this->m_srcRect[nodeIndex] = rect;

    return NO_ERROR;
}

status_t ExynosCameraFrameEntity::setDstRect(ExynosRect rect, uint32_t nodeIndex)
{
    if (nodeIndex >= DST_BUFFER_COUNT_MAX) {
        CLOGE2("Invalid buffer index, index(%d)", nodeIndex);
        return BAD_VALUE;
    }

    this->m_dstRect[nodeIndex] = rect;

    return NO_ERROR;
}

status_t ExynosCameraFrameEntity::getSrcRect(ExynosRect *rect, uint32_t nodeIndex)
{
    if (nodeIndex >= SRC_BUFFER_COUNT_MAX) {
        CLOGE2("Invalid buffer index, index(%d)", nodeIndex);
        return BAD_VALUE;
    }

    *rect = this->m_srcRect[nodeIndex];

    return NO_ERROR;
}

status_t ExynosCameraFrameEntity::getDstRect(ExynosRect *rect, uint32_t nodeIndex)
{
    if (nodeIndex >= DST_BUFFER_COUNT_MAX) {
        CLOGE2("Invalid buffer index, index(%d)", nodeIndex);
        return BAD_VALUE;
    }

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
#ifdef USE_DEBUG_PROPERTY
    if (m_entityState != state) {
        switch (state) {
        case ENTITY_STATE_PROCESSING:
            m_processTimestamp = systemTime(SYSTEM_TIME_BOOTTIME);
            break;
        case ENTITY_STATE_FRAME_DONE:
            m_doneTimestamp = systemTime(SYSTEM_TIME_BOOTTIME);
            break;
        default:
            break;
        }
    }
#endif
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

}; /* namespace android */
