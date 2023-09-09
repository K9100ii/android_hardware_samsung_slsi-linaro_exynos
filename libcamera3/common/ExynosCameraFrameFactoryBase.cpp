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
#define LOG_TAG "ExynosCameraFrameFactoryBase"
#include <log/log.h>

#include "ExynosCameraFrameFactoryBase.h"

namespace android {

ExynosCameraFrameFactoryBase::~ExynosCameraFrameFactoryBase()
{
    status_t ret = NO_ERROR;

    ret = destroy();
    if (ret != NO_ERROR)
        CLOGE("destroy fail");
}

status_t ExynosCameraFrameFactoryBase::create(void)
{
    CLOGI("");

    status_t ret = NO_ERROR;

    m_setupConfig();
    m_constructPipes();

    for (int i = 0; i < MAX_NUM_PIPES; i++) {
        if (m_pipes[i] != NULL) {
            /* HACK: FLITE must set sensorIds */
            if (i == PIPE_3AA
                || (m_cameraId == CAMERA_ID_FRONT && i == PIPE_FLITE)) {
                ret = m_pipes[i]->create(m_sensorIds[i]);
            } else {
                ret = m_pipes[i]->create();
            }
            if (ret != NO_ERROR) {
                CLOGE("%s(%d)->create() fail, ret(%d)",
                        m_pipes[i]->getName(), i, ret);
                return ret;
            }

            CLOGD("%s(%d) created", m_pipes[i]->getName(), i);
        }
    }

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryBase::precreate(void)
{
    CLOGI("");

    status_t ret = NO_ERROR;

    m_setupConfig();
    m_constructPipes();

    for (int i = 0; i < MAX_NUM_PIPES; i++) {
        if (m_pipes[i] != NULL) {
            /* HACK: FLITE must set sensorIds */
            if (i == PIPE_3AA
                || (m_cameraId == CAMERA_ID_FRONT && i == PIPE_FLITE)) {
                ret = m_pipes[i]->precreate(m_sensorIds[i]);
            } else {
                ret = m_pipes[i]->precreate();
            }
            if (ret != NO_ERROR) {
                CLOGE("%s(%d)->create() fail, ret(%d)",
                        m_pipes[i]->getName(), i, ret);
                return ret;
            }

            CLOGD("%s(%d) created", m_pipes[i]->getName(), i);
        }
    }

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryBase::postcreate(void)
{
    CLOGI("");

    status_t ret = NO_ERROR;

    for (int i = 0; i < MAX_NUM_PIPES; i++) {
        if (m_pipes[i] != NULL) {
            /* HACK: FLITE must skip */
            if (i == PIPE_FLITE) {
                continue;
            } else {
                ret = m_pipes[i]->postcreate();
            }
            if (ret != NO_ERROR) {
                CLOGE("%s(%d)->create() fail, ret(%d)",
                        m_pipes[i]->getName(), i, ret);
                return ret;
            }

            CLOGD("%s(%d) created", m_pipes[i]->getName(), i);
        }
    }

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryBase::destroy(void)
{
    CLOGI("");
    status_t ret = NO_ERROR;

    for (int i = 0; i < MAX_NUM_PIPES; i++) {
        if (m_pipes[i] != NULL) {
            ret = m_pipes[i]->destroy();
            if (ret != NO_ERROR) {
                CLOGE("m_pipes[%d]->destroy() fail", i);
                return ret;
            }

            SAFE_DELETE(m_pipes[i]);

            CLOGD("Pipe(%d) destroyed", i);
        }
    }

    ret = m_transitState(FRAME_FACTORY_STATE_NONE);

    return ret;
}

status_t ExynosCameraFrameFactoryBase::mapBuffers(void)
{
    CLOGE("Must use the concreate class, don't use superclass");
    return INVALID_OPERATION;
}

status_t ExynosCameraFrameFactoryBase::fastenAeStable(__unused int32_t numFrames, __unused ExynosCameraBuffer *buffers)
{
    CLOGE("Must use the concreate class, don't use superclass");
    return INVALID_OPERATION;
}

status_t ExynosCameraFrameFactoryBase::sensorStandby(__unused bool flagStandby)
{
    CLOGE("Must use the concreate class, don't use superclass");
    return INVALID_OPERATION;
}

bool ExynosCameraFrameFactoryBase::isSensorStandby(void)
{
    return m_sensorStandby;
}

status_t ExynosCameraFrameFactoryBase::setStopFlag(void)
{
    CLOGE("Must use the concreate class, don't use superclass");
    return INVALID_OPERATION;
}

status_t ExynosCameraFrameFactoryBase::stopPipe(uint32_t pipeId)
{
    status_t ret = NO_ERROR;

    ret = m_pipes[INDEX(pipeId)]->stopThread();
    if (ret != NO_ERROR) {
        CLOGE("Pipe:%d stopThread fail, ret(%d)", pipeId, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ret = m_pipes[INDEX(pipeId)]->stop();
    if (ret != NO_ERROR) {
        CLOGE("Pipe:%d stop fail, ret(%d)", pipeId, ret);
        /* TODO: exception handling */
        /* return INVALID_OPERATION; */
    }

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryBase::startThread(uint32_t pipeId)
{
    status_t ret = NO_ERROR;

    CLOGI("pipeId=%d", pipeId);

    ret = m_pipes[INDEX(pipeId)]->startThread();
    if (ret != NO_ERROR) {
        CLOGE("start thread fail, pipeId(%d), ret(%d)", pipeId, ret);
        /* TODO: exception handling */
    }
    return ret;
}

status_t ExynosCameraFrameFactoryBase::stopThread(uint32_t pipeId)
{
    status_t ret = NO_ERROR;

    CLOGI("pipeId=%d", pipeId);

    if (m_pipes[INDEX(pipeId)] == NULL) {
        CLOGE("m_pipes[INDEX(%d)] == NULL. so, fail", pipeId);
        return INVALID_OPERATION;
    }

    ret = m_pipes[INDEX(pipeId)]->stopThread();
    if (ret != NO_ERROR) {
        CLOGE("stop thread fail, pipeId(%d), ret(%d)", pipeId, ret);
        /* TODO: exception handling */
    }
    return ret;
}

status_t ExynosCameraFrameFactoryBase::stopThreadAndWait(uint32_t pipeId, int sleep, int times)
{
    status_t ret = NO_ERROR;

    CLOGI("pipeId=%d", pipeId);
    ret = m_pipes[INDEX(pipeId)]->stopThreadAndWait(sleep, times);
    if (ret != NO_ERROR) {
        CLOGE("pipe(%d) stopThreadAndWait fail, ret(%d)", pipeId, ret);
        /* TODO: exception handling */
        ret = INVALID_OPERATION;
    }
    return ret;
}

bool ExynosCameraFrameFactoryBase::checkPipeThreadRunning(uint32_t pipeId)
{
    status_t ret = NO_ERROR;

    ret = m_pipes[INDEX(pipeId)]->isThreadRunning();

    return ret;
}

void ExynosCameraFrameFactoryBase::setThreadOneShotMode(uint32_t pipeId, bool enable)
{
    m_pipes[INDEX(pipeId)]->setOneShotMode(enable);
}

status_t ExynosCameraFrameFactoryBase::setFrameManager(ExynosCameraFrameManager *manager)
{
    m_frameMgr = manager;
    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryBase::getFrameManager(ExynosCameraFrameManager **manager)
{
    *manager = m_frameMgr;
    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryBase::setBufferSupplierToPipe(ExynosCameraBufferSupplier *bufferSupplier, uint32_t pipeId)
{
    if (m_pipes[INDEX(pipeId)] == NULL) {
        CLOGE("m_pipes[INDEX(%d)] == NULL. pipeId(%d)", INDEX(pipeId), pipeId);
        return INVALID_OPERATION;
    }

    return m_pipes[INDEX(pipeId)]->setBufferSupplier(bufferSupplier);
}

status_t ExynosCameraFrameFactoryBase::setOutputFrameQToPipe(frame_queue_t *outputQ, uint32_t pipeId)
{
    m_pipes[INDEX(pipeId)]->setOutputFrameQ(outputQ);
    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryBase::getOutputFrameQToPipe(frame_queue_t **outputQ, uint32_t pipeId)
{
    CLOGV("pipeId=%d", pipeId);
    m_pipes[INDEX(pipeId)]->getOutputFrameQ(outputQ);

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryBase::setFrameDoneQToPipe(frame_queue_t *frameDoneQ, uint32_t pipeId)
{
    m_pipes[INDEX(pipeId)]->setFrameDoneQ(frameDoneQ);
    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryBase::getFrameDoneQToPipe(frame_queue_t **frameDoneQ, uint32_t pipeId)
{
    CLOGV("pipeId=%d", pipeId);
    m_pipes[INDEX(pipeId)]->getFrameDoneQ(frameDoneQ);

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryBase::getInputFrameQToPipe(frame_queue_t **inputFrameQ, uint32_t pipeId)
{
    CLOGV("pipeId=%d", pipeId);

    m_pipes[INDEX(pipeId)]->getInputFrameQ(inputFrameQ);

    if (inputFrameQ == NULL)
        CLOGE("inputFrameQ is NULL");

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryBase::pushFrameToPipe(ExynosCameraFrameSP_dptr_t newFrame, uint32_t pipeId)
{
    m_pipes[INDEX(pipeId)]->pushFrame(newFrame);
    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryBase::setParam(struct v4l2_streamparm *streamParam, uint32_t pipeId)
{
    status_t ret = NO_ERROR;

    ret = m_pipes[INDEX(pipeId)]->setParam(*streamParam);

    return ret;
}

status_t ExynosCameraFrameFactoryBase::setControl(int cid, int value, uint32_t pipeId)
{
    status_t ret = NO_ERROR;

    ret = m_pipes[INDEX(pipeId)]->setControl(cid, value);

    return ret;
}

status_t ExynosCameraFrameFactoryBase::setControl(int cid, int value, uint32_t pipeId, enum NODE_TYPE nodeType)
{
    status_t ret = NO_ERROR;

    ret = m_pipes[INDEX(pipeId)]->setControl(cid, value, nodeType);

    return ret;
}

status_t ExynosCameraFrameFactoryBase::getControl(int cid, int *value, uint32_t pipeId)
{
    status_t ret = NO_ERROR;

    ret = m_pipes[INDEX(pipeId)]->getControl(cid, value);

    return ret;
}

void ExynosCameraFrameFactoryBase::setRequest(int pipeId, bool enable)
{
    if ((pipeId % 100) >= MAX_NUM_PIPES) {
        CLOGE("Invalid pipeId(%d)", pipeId);
    } else {
        m_request[INDEX(pipeId)] = enable;

        if (pipeId == PIPE_HWFC_JPEG_SRC_REPROCESSING
            || pipeId == PIPE_HWFC_JPEG_DST_REPROCESSING) {
            m_request[INDEX(PIPE_HWFC_JPEG_SRC_REPROCESSING)] = enable;
            m_request[INDEX(PIPE_HWFC_JPEG_DST_REPROCESSING)] = enable;
        }

        if (pipeId == PIPE_HWFC_THUMB_SRC_REPROCESSING
            || pipeId == PIPE_HWFC_THUMB_DST_REPROCESSING) {
            m_request[INDEX(PIPE_HWFC_THUMB_SRC_REPROCESSING)] = enable;
            m_request[INDEX(PIPE_HWFC_THUMB_DST_REPROCESSING)] = enable;
        }
    }
}

bool ExynosCameraFrameFactoryBase::getRequest(int pipeId)
{
    bool ret = false;
    if ((pipeId % 100) >= MAX_NUM_PIPES) {
        CLOGE("Invalid pipeId(%d)", pipeId);
    } else {
        ret = m_request[INDEX(pipeId)];
    }

    return ret;
}

status_t ExynosCameraFrameFactoryBase::getThreadState(int **threadState, uint32_t pipeId)
{
    m_pipes[INDEX(pipeId)]->getThreadState(threadState);

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryBase::getThreadInterval(uint64_t **threadInterval, uint32_t pipeId)
{
    m_pipes[INDEX(pipeId)]->getThreadInterval(threadInterval);

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryBase::getThreadRenew(int **threadRenew, uint32_t pipeId)
{
    m_pipes[INDEX(pipeId)]->getThreadRenew(threadRenew);

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryBase::incThreadRenew(uint32_t pipeId)
{
    m_pipes[INDEX(pipeId)]->incThreadRenew();

    return NO_ERROR;
}

int ExynosCameraFrameFactoryBase::getRunningFrameCount(uint32_t pipeId)
{
    int runningFrameCount = 0;
    CLOGV("");

    runningFrameCount = m_pipes[INDEX(pipeId)]->getRunningFrameCount();

    return runningFrameCount;
}

void ExynosCameraFrameFactoryBase::dump()
{
    CLOGI("");

    for (int i = 0; i < MAX_NUM_PIPES; i++) {
        if (m_pipes[i] != NULL) {
            m_pipes[i]->dump();
        }
    }

    return;
}

status_t ExynosCameraFrameFactoryBase::dumpFimcIsInfo(uint32_t pipeId, bool bugOn)
{
    status_t ret = NO_ERROR;

    if (m_pipes[INDEX(pipeId)] != NULL)
        ret = m_pipes[INDEX(pipeId)]->dumpFimcIsInfo(bugOn);
    else
        CLOGE("pipe is not ready (%d/%d)", pipeId, bugOn);

    return ret;
}

#ifdef MONITOR_LOG_SYNC
status_t ExynosCameraFrameFactoryBase::syncLog(uint32_t pipeId, uint32_t syncId)
{
    status_t ret = NO_ERROR;

    if (m_pipes[INDEX(pipeId)] != NULL)
        ret = m_pipes[INDEX(pipeId)]->syncLog(syncId);
    else
        ALOGE("ERR(%s): pipe is not ready (%d/%d)", __FUNCTION__, pipeId, syncId);

    return ret;
}
#endif

status_t ExynosCameraFrameFactoryBase::setFrameCreateHandler(factory_handler_t handler)
{
    status_t ret = NO_ERROR;
    m_frameCreateHandler = handler;
    return ret;
}

factory_handler_t ExynosCameraFrameFactoryBase::getFrameCreateHandler()
{
    return m_frameCreateHandler;
}

void ExynosCameraFrameFactoryBase::setFactoryType(enum FRAME_FACTORY_TYPE factoryType)
{
    m_factoryType = factoryType;
}

enum FRAME_FACTORY_TYPE ExynosCameraFrameFactoryBase::getFactoryType(void)
{
    return m_factoryType;
}

bool ExynosCameraFrameFactoryBase::isCreated(void)
{
    Mutex::Autolock lock(m_stateLock);

    bool isCreated = false;

    switch (m_state) {
    case FRAME_FACTORY_STATE_INIT:
    case FRAME_FACTORY_STATE_RUN:
        CLOGW("invalid state(%d)", m_state);
    case FRAME_FACTORY_STATE_CREATE:
        isCreated = true;
        break;
    default:
        break;
    }

    return isCreated;
}

bool ExynosCameraFrameFactoryBase::isRunning(void)
{
    Mutex::Autolock lock(m_stateLock);

    return (m_state == FRAME_FACTORY_STATE_RUN)? true : false;
}

status_t ExynosCameraFrameFactoryBase::m_initPipelines(ExynosCameraFrameSP_sptr_t frame)
{
    ExynosCameraFrameEntity *curEntity = NULL;
    ExynosCameraFrameEntity *childEntity = NULL;
    frame_queue_t *frameQ = NULL;
    status_t ret = NO_ERROR;

    curEntity = frame->getFirstEntity();

    while (curEntity != NULL) {
        childEntity = curEntity->getNextEntity();
        if (childEntity != NULL) {
            ret = getInputFrameQToPipe(&frameQ, childEntity->getPipeId());
            if (ret != NO_ERROR || frameQ == NULL) {
                CLOGE("getInputFrameQToPipe fail, ret(%d), frameQ(%p)", ret, frameQ);
                return ret;
            }

            ret = setOutputFrameQToPipe(frameQ, curEntity->getPipeId());
            if (ret != NO_ERROR) {
                CLOGE("setOutputFrameQToPipe fail, ret(%d)", ret);
                return ret;
            }

            /* check Image Configuration Equality */
            ret = m_checkPipeInfo(curEntity->getPipeId(), childEntity->getPipeId());
            if (ret != NO_ERROR) {
                CLOGE("checkPipeInfo fail, Pipe[%d], Pipe[%d]", curEntity->getPipeId(), childEntity->getPipeId());
                return ret;
            }

            curEntity = childEntity;
        } else {
            curEntity = frame->getNextEntity();
        }
    }

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryBase::m_checkPipeInfo(uint32_t srcPipeId, uint32_t dstPipeId)
{
    int srcFullW, srcFullH, srcColorFormat;
    int dstFullW, dstFullH, dstColorFormat;
    status_t ret = NO_ERROR;

    ret = m_pipes[INDEX(srcPipeId)]->getPipeInfo(&srcFullW, &srcFullH, &srcColorFormat, SRC_PIPE);
    if (ret != NO_ERROR) {
        CLOGE("Source getPipeInfo fail");
        return ret;
    }
    ret = m_pipes[INDEX(dstPipeId)]->getPipeInfo(&dstFullW, &dstFullH, &dstColorFormat, DST_PIPE);
    if (ret != NO_ERROR) {
        CLOGE("Destination getPipeInfo fail");
        return ret;
    }

    if (srcFullW != dstFullW || srcFullH != dstFullH || srcColorFormat != dstColorFormat) {
        CLOGE("Video Node Image Configuration is NOT matching. so, fail");

        CLOGE("fail info : srcPipeId(%d), srcFullW(%d), srcFullH(%d), srcColorFormat(%d)",
                srcPipeId, srcFullW, srcFullH, srcColorFormat);

        CLOGE("fail info : dstPipeId(%d), dstFullW(%d), dstFullH(%d), dstColorFormat(%d)",
                dstPipeId, dstFullW, dstFullH, dstColorFormat);

        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

void ExynosCameraFrameFactoryBase::m_initDeviceInfo(int pipeId)
{
    camera_device_info_t nullDeviceInfo;

    m_deviceInfo[pipeId] = nullDeviceInfo;

    for (int i = 0; i < MAX_NODE; i++) {
        // set nodeNum
        m_nodeNums[pipeId][i] = m_deviceInfo[pipeId].nodeNum[i];

        // set default sensorId
        m_sensorIds[pipeId][i]  = -1;
    }
}

status_t ExynosCameraFrameFactoryBase::m_setSensorSize(int pipeId, int sensorW, int sensorH)
{
    status_t ret = NO_ERROR;

    /* set sensor size */
    int sensorSizeBit = (sensorW << SENSOR_SIZE_WIDTH_SHIFT) | (sensorH << SENSOR_SIZE_HEIGHT_SHIFT);

    ret = m_pipes[pipeId]->setControl(V4L2_CID_IS_S_SENSOR_SIZE, sensorSizeBit);
    if (ret != NO_ERROR) {
        CLOGE("setControl(V4L2_CID_IS_S_SENSOR_SIZE, (%d x %d)) fail", sensorW, sensorH);
        return ret;
    }

    CLOGD("setControl(V4L2_CID_IS_S_SENSOR_SIZE, (%d x %d)) succeed", sensorW, sensorH);

    return ret;
}

status_t ExynosCameraFrameFactoryBase::m_transitState(frame_factory_state_t state)
{
    Mutex::Autolock lock(m_stateLock);

    CLOGV("State transition. curState %d newState %d",
            m_state, state);

    if (m_state == state) {
        CLOGI("Skip state transition. curState %d",
                m_state);
        return NO_ERROR;
    }

    switch (m_state) {
    case FRAME_FACTORY_STATE_NONE:
        if (state != FRAME_FACTORY_STATE_CREATE)
            goto ERR_EXIT;

        m_state = state;
        break;
    case FRAME_FACTORY_STATE_CREATE:
        if (state > FRAME_FACTORY_STATE_INIT)
            goto ERR_EXIT;

        m_state = state;
        break;
    case FRAME_FACTORY_STATE_INIT:
        if (state != FRAME_FACTORY_STATE_RUN)
            goto ERR_EXIT;

        m_state = state;
        break;
    case FRAME_FACTORY_STATE_RUN:
        if (state != FRAME_FACTORY_STATE_CREATE)
            goto ERR_EXIT;

        m_state = state;
        break;
    default:
        CLOGW("Invalid curState %d maxValue %d",
                state, FRAME_FACTORY_STATE_MAX);
        goto ERR_EXIT;
    }

    return NO_ERROR;

ERR_EXIT:
    CLOGE("Invalid state transition. curState %d newState %d",
            m_state, state);
    return INVALID_OPERATION;
}

void ExynosCameraFrameFactoryBase::m_init(void)
{
    m_cameraId = 0;
    m_frameCount = 0;

    memset(m_name, 0x00, sizeof(m_name));
    memset(m_nodeNums, -1, sizeof(m_nodeNums));
    memset(m_sensorIds, -1, sizeof(m_sensorIds));

    for (int i = 0; i < MAX_NUM_PIPES; i++) {
        m_pipes[i] = NULL;
        m_request[i] = false;
    }

    m_frameMgr = NULL;
    m_frameCreateHandler = NULL;

    /* setting about bypass */
    m_bypassDRC = true;
    m_bypassDIS = true;
    m_bypassDNR = true;
    m_bypassFD = true;

    /* setting about H/W OTF mode */
    m_flagFlite3aaOTF = HW_CONNECTION_MODE_NONE;
    m_flag3aaIspOTF = HW_CONNECTION_MODE_NONE;
    m_flagIspMcscOTF = HW_CONNECTION_MODE_NONE;
    m_flagMcscVraOTF = HW_CONNECTION_MODE_NONE;
#ifdef USE_DUAL_CAMERA
    m_flagIspDcpOTF = HW_CONNECTION_MODE_NONE;
    m_flagDcpMcscOTF = HW_CONNECTION_MODE_NONE;
#endif

    /* setting about reprocessing */
    m_supportReprocessing = false;
    m_flagReprocessing = false;
    m_supportPureBayerReprocessing = false;
    m_state = FRAME_FACTORY_STATE_NONE;
    m_factoryType = FRAME_FACTORY_TYPE_MAX;
    m_sensorStandby = false;
}

}; /* namespace android */
