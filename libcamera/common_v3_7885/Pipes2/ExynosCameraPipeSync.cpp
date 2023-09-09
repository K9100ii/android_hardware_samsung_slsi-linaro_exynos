/*
**
** Copyright 2016, Samsung Electronics Co. LTD
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
#define LOG_TAG "ExynosCameraPipeSync"
#include <cutils/log.h>

#include "ExynosCameraPipeSync.h"

namespace android {

ExynosCameraPipeSync::~ExynosCameraPipeSync()
{
    this->destroy();
}

status_t ExynosCameraPipeSync::create(__unused int32_t *sensorIds)
{
    CLOGD("");

    status_t ret;

    ret = ExynosCameraSWPipe::create(sensorIds);
    if (ret < 0) {
        CLOGE("startThread fail");
        return ret;
    }

    char SuperClassName[EXYNOS_CAMERA_NAME_STR_SIZE] = {0,};
    snprintf(SuperClassName, sizeof(SuperClassName), "%s_%s_SWThread", m_name, "Sel");
    m_selectThread = ExynosCameraThreadFactory::createThread(this, &ExynosCameraPipeSync::m_selectThreadFunc, SuperClassName);

    snprintf(SuperClassName, sizeof(SuperClassName), "%s_%s_SWThread", m_name, "Remove");
    m_removeFrameThread = ExynosCameraThreadFactory::createThread(this, &ExynosCameraPipeSync::m_removeFrameThreadFunc, SuperClassName);

    m_lastTimeStamp = 0;

    m_inputFrameQ->setWaitTime(5000000000);

    CLOGI("create() is succeed (%d) M:%d, S:%d", getPipeId(), m_masterCameraId, m_slaveCameraId);

    return ret;
}

status_t ExynosCameraPipeSync::destroy(void)
{
    CLOGD("");

    status_t ret;

    ret = ExynosCameraSWPipe::release();
    if (ret < 0) {
        CLOGE("release fail");
        return ret;
    }

    if (m_dualSelector != NULL) {
        m_dualSelector->deinit(m_cameraId);
        m_dualSelector = NULL;
    }

    if (m_outputFrameQ != NULL) {
        m_outputFrameQ->release();
        m_outputFrameQ = NULL;
    }

    CLOGI("destroy() is succeed (%d)", getPipeId());

    return ret;
}

status_t ExynosCameraPipeSync::start(void)
{
    CLOGD("");

    m_flagTryStop = false;

    return NO_ERROR;
}

status_t ExynosCameraPipeSync::stop(void)
{
    CLOGD("");

    status_t ret = NO_ERROR;

    m_flagTryStop = true;

    m_mainThread->requestExitAndWait();
    m_selectThread->requestExitAndWait();
    m_removeFrameThread->requestExitAndWait();

    m_dualSelector->flush(m_cameraId);

    CLOGD(" thead exited");

    if (m_inputFrameQ != NULL)
        m_inputFrameQ->release();

    if (m_outputFrameQ != NULL)
        m_outputFrameQ->release();

    if (m_notifyQ != NULL)
        (m_notifyQ)->release();

    if (m_removeFrameQ != NULL)
        (m_removeFrameQ)->release();

    m_flagTryStop = false;

    return ret;
}

status_t ExynosCameraPipeSync::setBufferManager(ExynosCameraBufferManager **bufferManager)
{
    CLOGD("");

    status_t ret = NO_ERROR;

    if (m_dualSelector == NULL) {
        CLOGE("dualSelector is NULL!!");
        return INVALID_OPERATION;
    }

    for (int i = 0; i < MAX_NODE; i++) {
        m_bufferManager[i] = bufferManager[i];

        if (i == OUTPUT_NODE &&
                m_bufferManager[i] != NULL) {

            CLOGI("bufferManager(%d) registered(%d)", i, m_bufferManager[i]->getAllocatedBufferCount());

            m_dualSelector->deinit(m_cameraId);

            m_dualSelector->setFlagValidCameraId(m_masterCameraId, m_slaveCameraId);

            ret = m_dualSelector->registerRemoveFrameQ(m_cameraId, &m_removeFrameQ);
            if (ret != NO_ERROR)
                CLOG_ASSERT("m_dualSelector->registerNotifyQ is fail");

            ret = m_dualSelector->setInfo(m_cameraId, m_parameters, m_bufferManager[i]);
            if (ret != NO_ERROR)
                CLOG_ASSERT("m_dualSelector->setInfo is fail");

            /* only master set ther hold count and notifyQ */
            if (m_cameraId == m_masterCameraId) {
                int maxBuffer = m_bufferManager[i]->getNumOfAllowedMaxBuffer();
                maxBuffer = maxBuffer > 1 ? maxBuffer - 1 : maxBuffer;

                ret = m_dualSelector->registerNotifyQ(m_cameraId, &m_notifyQ);
                if (ret != NO_ERROR)
                    CLOG_ASSERT("m_dualSelector->registerNotifyQ is fail");

                ret = m_dualSelector->setFrameHoldCount(m_cameraId, maxBuffer, m_isReprocessing() ? maxBuffer : 0);
                if (ret != NO_ERROR)
                    CLOG_ASSERT("m_dualSelector->setFrameHoldCount is fail", maxBuffer);
            } else if (m_flagSelectedByAllCamera == true) {
                ret = m_dualSelector->registerNotifyQ(m_cameraId, &m_notifyQ);
                if (ret != NO_ERROR)
                    CLOG_ASSERT("m_dualSelector->registerNotifyQ is fail");
            }
        }
    }

    return ret;
}

status_t ExynosCameraPipeSync::startThread(void)
{
    CLOGD("");

    status_t ret = NO_ERROR;

    if (m_mainThread->isRunning() == false) {
        m_mainThread->run(m_name, PRIORITY_URGENT_DISPLAY);
        CLOGD("mainThread start");
    }

    /* only master camera's sync pipe run the selectThread */
    if (m_flagSelectedByAllCamera == true ||
            m_cameraId == m_masterCameraId) {
        if (m_selectThread->isRunning() == false) {
            CLOGD("selectThread start(M:%d)", m_masterCameraId);
            m_selectThread->run(m_name, PRIORITY_URGENT_DISPLAY);
        }

        if (m_removeFrameThread->isRunning() == false) {
            CLOGD("removeThread start(M:%d)", m_masterCameraId);
            m_removeFrameThread->run(m_name);
        }
    } else {
        CLOGD("don't start selectThread(M:%d)", m_masterCameraId);
    }

    CLOGI("startThread is succeed (%d)", getPipeId());

    return ret;
}

status_t ExynosCameraPipeSync::stopThread(void)
{
    CLOGD("");

    status_t ret = NO_ERROR;

    m_mainThread->requestExit();
    m_selectThread->requestExit();
    m_removeFrameThread->requestExit();

    if (m_inputFrameQ != NULL)
        m_inputFrameQ->sendCmd(WAKE_UP);

    if (m_outputFrameQ != NULL)
        m_outputFrameQ->sendCmd(WAKE_UP);

    if (m_removeFrameQ != NULL)
        m_removeFrameQ->sendCmd(WAKE_UP);

    CLOGI("stopThread is succeed (%d)", getPipeId());

    return ret;
}

void ExynosCameraPipeSync::setDualSelector(ExynosCameraDualFrameSelector *dualSelector)
{
    CLOGD("");

    status_t ret = NO_ERROR;

    m_dualSelector = dualSelector;

    /* register the notifyQ, removeFrameQ from dual selector(only master camera) */
    if (m_cameraId == m_masterCameraId) {
        m_dualSelector->flush(m_cameraId);
        if (ret != NO_ERROR)
            CLOG_ASSERT("im_dualSelector->flush() is fail");
    }
}

status_t ExynosCameraPipeSync::m_run(void)
{
    static int internalLogCount[CAMERA_ID_MAX];
    status_t ret = 0;
    bool isSrc = true;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraFrameEntity *entity = NULL;
    frame_type_t frameType;
    frame_status_t frameState;

    ret = m_inputFrameQ->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
#ifdef USE_DUAL_CAMERA
            if (!(m_parameters->getDualCameraMode() == true &&
                        m_parameters->getDualStandbyMode() == ExynosCameraParameters::DUAL_STANDBY_MODE_ACTIVE_IN_SENSOR))
#endif
            CLOGW("wait timeout");
        } else {
            CLOGE("wait and pop fail, ret(%d)", ret);
            /* TODO: doing exception handling */
        }
        return ret;
    }

    if (newFrame == NULL) {
        CLOGE("new frame is NULL");
        return NO_ERROR;
    }

    if (m_dualSelector == NULL) {
        CLOGE("m_dualSelector is NULL");
        return NO_ERROR;
    }

    frameType = (frame_type_t)newFrame->getFrameType();
    frameState = newFrame->getFrameState();

#ifdef USE_DUAL_CAMERA_LOG_TRACE
    CLOGI("input frame (isSrc:%d, Cam:%d, Fcount:%d, Sync:%d, Type:%d, Time:%lld, State:%d)",
            isSrc,
            newFrame->getCameraId(),
            newFrame->getFrameCount(),
            newFrame->getSyncType(),
            newFrame->getFrameType(),
            newFrame->getTimeStamp(),
            newFrame->getFrameState());
#endif

    /* push the frame to dual selector */
    switch (newFrame->getSyncType()) {
    case SYNC_TYPE_BYPASS:
        if (m_cameraId == m_slaveCameraId)
            CLOG_ASSERT("invalid cameraId(%d != %d) frame(%d)", m_cameraId, m_slaveCameraId, newFrame->getFrameCount());
        break;
    case SYNC_TYPE_SYNC:
        break;
    case SYNC_TYPE_SWITCH:
        if (m_cameraId == m_masterCameraId)
            CLOG_ASSERT("invalid cameraId(%d != %d) frame(%d)", m_cameraId, m_masterCameraId, newFrame->getFrameCount());
        break;
    default:
        CLOG_ASSERT("invalid sync type(%d) frame(%d)", m_cameraId, newFrame->getSyncType(), newFrame->getFrameCount());
        break;
    }

    /* set the entity state */
    ret = newFrame->setEntityState(getPipeId(), ENTITY_STATE_FRAME_DONE);
    if (ret != NO_ERROR) {
        CLOGE("setEntityState(%d, ENTITY_STATE_FRAME_DONE) fail", getPipeId());
    }

    /* push the frame to dual selector */
    ret = m_dualSelector->manageNormalFrameHoldList(m_cameraId, newFrame, getPipeId(), isSrc, OUTPUT_NODE_1);
    if (ret != NO_ERROR) {
        /* in case of error, release frame with buffer */
        if (newFrame->getFrameType() != FRAME_TYPE_INTERNAL) {
            ExynosCameraBuffer buffer;
            buffer.index = -2;
            CLOGE("manageNormalFrameHoldList fail(%d, F%d)", isSrc, newFrame->getFrameCount());
            newFrame->getSrcBuffer(getPipeId(), &buffer, OUTPUT_NODE_1);
            m_dualSelector->releaseBuffer(m_cameraId, &buffer);
            goto func_exit;
        }
    }

    /* check the frame type */
    switch (frameType) {
    case FRAME_TYPE_INTERNAL:
        if ((internalLogCount[m_cameraId]++ % 100) == 0) {
            CLOGI("[INTERNAL_FRAME] frame(%d) type(%d), (%d)",
                    newFrame->getFrameCount(), frameType, internalLogCount[m_cameraId]);
        }
        goto func_exit;
        break;
    default:
        internalLogCount[m_cameraId] = 0;
        break;
    }

    /* check the frame state */
    switch (frameState) {
    case FRAME_STATE_SKIPPED:
    case FRAME_STATE_INVALID:
        CLOGE("frame(%d) state is invalid(%d)",
                newFrame->getFrameCount(), frameState);
        goto func_exit;
        break;
    default:
        break;
    }

    return NO_ERROR;

func_exit:
    /* comptle the frame to remove the processList */
    newFrame->setFrameState(FRAME_STATE_SKIPPED);

    ret = newFrame->setEntityState(getPipeId(), ENTITY_STATE_FRAME_DONE);
    if (ret != NO_ERROR) {
        CLOGE("setEntityState(%d, ENTITY_STATE_FRAME_DONE) fail", getPipeId());
    }
    m_outputFrameQ->pushProcessQ(&newFrame);

    return NO_ERROR;
}

bool ExynosCameraPipeSync::m_selectThreadFunc(void)
{
    int ret = 0;

    ret = m_select();
    if (ret < 0) {
        if (ret != TIMED_OUT)
            CLOGE("m_select fail");
    }

    if (m_flagTryStop == true)
        return false;
    else
        return true;
}

status_t ExynosCameraPipeSync::m_select(void)
{
    int ret = 0;
    int timeStamp = 0;
    ExynosCameraDualFrameSelector::Message msg;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraBuffer srcBuffer;
    ExynosCameraFrameEntity *entity = NULL;

    /* for DROP_FRAME */
    bool releaseMasterBuffer = false;
    bool releaseSlaveBuffer = false;
    int masterSrcIndex = 0;
    int slaveSrcIndex = 0;

    CLOGV("");

    ret = (m_notifyQ)->waitAndPopProcessQ(&msg);
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGW("wait timeout");
        } else {
            CLOGE("wait and pop fail, ret(%d)", ret);
            /* TODO: doing exception handling */
        }
        return ret;
    }

    /* if both cameras(master / slave) can select the frame, check the cameraId from message */
    if (m_flagSelectedByAllCamera == true) {
        if (msg.getCameraId() != m_cameraId) {
            CLOGV("wrong cameraId(%d), syncType(%d)", msg.getCameraId(), msg.getSyncType());
            return ret;
        }
    }

    /* 1. get the Frame from dual selector */
    newFrame = m_dualSelector->selectFrame(m_cameraId);
    if (newFrame == NULL) {
        CLOGE("new frame is NULL");
        return NO_ERROR;
    }

    if (m_flagSelectedByAllCamera == true) {
        if (m_cameraId != newFrame->getCameraId()) {
            CLOG_ASSERT("wrong cameraId(%d), syncType(%d)",
                    newFrame->getCameraId(),
                    newFrame->getSyncType());
        }
    }

#ifdef USE_DUAL_CAMERA_LOG_TRACE
    CLOGI("pipeId:%d, selected frame(Cam:%d, HF:%d, DF:%d, SyncType:%d, Type:%d, State:%d, TimeStamp:%d",
           getPipeId(),
           newFrame->getCameraId(),
           newFrame->getFrameCount(),
           newFrame->getMetaFrameCount(),
           newFrame->getSyncType(),
           newFrame->getFrameType(),
           newFrame->getFrameState(),
           (int)(newFrame->getTimeStamp() / 1000000));
#endif

    /* 2. check the timeStamp(ms) */
    timeStamp = newFrame->getTimeStamp() / 1000000;
    if (abs(timeStamp - m_lastTimeStamp) < 5) {
        CLOGW("forcely drop the frame due to timeStamp(%d vs %d)", timeStamp, m_lastTimeStamp);
        goto DROP_FRAME;
    }

    /* 3. set the entity state to done */
    if (newFrame->getSyncType() == SYNC_TYPE_SWITCH) {
        /* set the entity state to done */
        ret = newFrame->setEntityState(getPipeId(), ENTITY_STATE_REWORK);
        if (ret < 0) {
            CLOGE("frame(%d)->setEntityState(pipeId(%d), ENTITY_STATE_REWORK), ret(%d)",
                    newFrame->getFrameCount(), getPipeId(), ret);
        }

        /* set the entity state to done */
        ret = newFrame->setEntityState(getPipeId(), ENTITY_STATE_FRAME_DONE);
        if (ret < 0) {
            CLOGE("frame(%d)->setEntityState(pipeId(%d), ENTITY_STATE_FRAME_DONE), ret(%d)",
                    newFrame->getFrameCount(), getPipeId(), ret);
        }
    } else {
        ret = newFrame->setEntityState(getPipeId(), ENTITY_STATE_FRAME_DONE);
        if (ret < 0) {
            CLOGE("newFrame->setEntityState(pipeId(%d), ENTITY_STATE_FRAME_DONE), ret(%d)",
                   getPipeId(), ret);
            /* TODO: doing exception handling */
            return OK;
        }
    }

    /* 4. update last timestamp and push the frame to outputQ */
    m_lastTimeStamp = newFrame->getTimeStamp() / 1000000;
    m_outputFrameQ->pushProcessQ(&newFrame);

    return NO_ERROR;

DROP_FRAME:
    switch (newFrame->getSyncType()) {
    case SYNC_TYPE_SYNC:
        releaseMasterBuffer = true;
        releaseSlaveBuffer = true;

        masterSrcIndex = OUTPUT_NODE_1;
        slaveSrcIndex = OUTPUT_NODE_2;
        break;
    case SYNC_TYPE_BYPASS:
        releaseMasterBuffer = true;

        masterSrcIndex = OUTPUT_NODE_1;
        break;
    case SYNC_TYPE_SWITCH:
        releaseSlaveBuffer = true;

        slaveSrcIndex = OUTPUT_NODE_1;
        break;
    default:
        break;
    }

    /* release master source buffer */
    if (releaseMasterBuffer == true) {
        ret = newFrame->getSrcBuffer(getPipeId(), &srcBuffer, masterSrcIndex);
        if (ret < 0) {
            CLOGE("newFrame(%d)->getSrcBuffer(%d, %d) fail(%d)",
                    newFrame->getFrameCount(), getPipeId(), masterSrcIndex, ret);
        } else {
            ret = m_dualSelector->releaseBuffer(m_masterCameraId, &srcBuffer);
            if (ret != NO_ERROR) {
                CLOGE("m_dualSelector->releaseBuffer(%d, %d) pipeId:%d, frame:%d fail(%d)",
                        masterSrcIndex, srcBuffer.index, getPipeId(), newFrame->getFrameCount(), ret);
            }
        }
    }

    /* release slave source buffer */
    if (releaseSlaveBuffer == true) {
        ret = newFrame->getSrcBuffer(getPipeId(), &srcBuffer, slaveSrcIndex);
        if (ret < 0) {
            CLOGE("newFrame(%d)->getSrcBuffer(%d, %d) fail(%d)",
                    newFrame->getFrameCount(), getPipeId(), slaveSrcIndex, ret);
        } else {
            ret = m_dualSelector->releaseBuffer(m_slaveCameraId, &srcBuffer);
            if (ret != NO_ERROR) {
                CLOGE("m_dualSelector->releaseBuffer(%d, %d) pipeId:%d, frame:%d fail(%d)",
                        slaveSrcIndex, srcBuffer.index, getPipeId(), newFrame->getFrameCount(), ret);
            }
        }
    }

    /* comptle the frame to remove the processList */
    newFrame->setFrameState(FRAME_STATE_SKIPPED);

    ret = newFrame->setEntityState(getPipeId(), ENTITY_STATE_FRAME_DONE);
    if (ret < 0) {
        CLOGE("setEntityState(%d, ENTITY_STATE_FRAME_DONE) frame:%d, fail(%d)",
                getPipeId(), newFrame->getFrameCount(), ret);
        /* TODO: doing exception handling */
    }

    m_outputFrameQ->pushProcessQ(&newFrame);

    return NO_ERROR;
}

bool ExynosCameraPipeSync::m_removeFrameThreadFunc(void)
{
    int ret = 0;

    ret = m_removeFrame();
    if (ret < 0) {
        if (ret != TIMED_OUT)
            CLOGE("m_putBuffer fail");
    }

    return m_checkThreadLoop();
}

status_t ExynosCameraPipeSync::m_removeFrame(void)
{
    int ret = 0;
    ExynosCameraFrameSP_sptr_t frame = NULL;

    CLOGV("");

    ret = (m_removeFrameQ)->waitAndPopProcessQ(&frame);
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            /* CLOGW("wait timeout"); */
        } else {
            CLOGE("wait and pop fail, ret(%d)", ret);
            /* TODO: doing exception handling */
        }
        return ret;
    }

    CLOGV("remove frame(HF:%d, DF:%d, TimeStamp:%lld, SyncType:%d, State:%d, Type:%d",
           frame->getFrameCount(),
           frame->getMetaFrameCount(),
           frame->getTimeStamp(),
           frame->getSyncType(),
           frame->getFrameState(),
           frame->getFrameType());

    frame->setFrameState(FRAME_STATE_SKIPPED);

    ret = frame->setEntityState(getPipeId(), ENTITY_STATE_FRAME_DONE);
    if (ret < 0) {
        CLOGE("setEntityState(%d, ENTITY_STATE_FRAME_DONE) frame:%d, fail(%d)",
               getPipeId(), frame->getFrameCount(), ret);
        /* TODO: doing exception handling */
    }

    m_outputFrameQ->pushProcessQ(&frame);

    return NO_ERROR;
}

void ExynosCameraPipeSync::m_init(void)
{
    m_masterCameraId = -1;
    m_slaveCameraId = -1;
    m_dualSelector = NULL;
    m_notifyQ = NULL;
    m_lastTimeStamp = 0;
}
}; /* namespace android */
