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

status_t ExynosCameraPipeSync::create(int32_t *sensorIds)
{
    status_t ret = NO_ERROR;

    ret = ExynosCameraSWPipe::create(sensorIds);
    if (ret != NO_ERROR) {
        CLOGE("SWPipe creation fail!");
        return ret;
    }

    m_masterFrameQ = new frame_queue_t;
    m_slaveFrameQ = new frame_queue_t;

    m_inputFrameQ->setWaitTime(100000000);

    CLOGI("create() is succeed (%d)", getPipeId());

    return NO_ERROR;

}

status_t ExynosCameraPipeSync::stop(void)
{
    CLOGD("");

    status_t ret = NO_ERROR;

    ret = ExynosCameraSWPipe::stop();
    if (ret != NO_ERROR) {
        CLOGE("SWPipe stop fail!");
        return ret;
    }

    m_masterFrameQ->release();
    m_slaveFrameQ->release();

    return NO_ERROR;
}

status_t ExynosCameraPipeSync::m_destroy(void)
{
    status_t ret = NO_ERROR;

    ret = ExynosCameraSWPipe::m_destroy();
    if (ret != NO_ERROR)
        CLOGE("Destroy fail!");

    if (m_masterFrameQ != NULL) {
        m_masterFrameQ->release();
        delete m_masterFrameQ;
        m_masterFrameQ = NULL;
    }

    if (m_slaveFrameQ != NULL) {
        m_slaveFrameQ->release();
        delete m_slaveFrameQ;
        m_slaveFrameQ = NULL;
    }

    return ret;
}

status_t ExynosCameraPipeSync::m_run(void)
{
    static int internalLogCount[CAMERA_ID_MAX];
    status_t ret = 0;
    bool isSrc = true, isSync = false;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraFrameSP_sptr_t tempFrame = NULL;
    ExynosCameraFrameSP_sptr_t compareFrame = NULL;
    ExynosCameraFrameEntity *entity = NULL;
    ExynosCameraBuffer buffer;
    int numOfProcessQ = 0;
    int masterSrcPipeId = -1;
    int slaveSrcPipeId = -1;
    enum NODE_TYPE masterSrcBufferPos = INVALID_NODE;
    enum NODE_TYPE slaveSrcBufferPos = INVALID_NODE;

    if (m_reprocessing == false) {
        switch (m_parameters->getDualPreviewMode()) {
        case DUAL_PREVIEW_MODE_HW:
            masterSrcPipeId = PIPE_3AA;
            masterSrcBufferPos = CAPTURE_NODE_4; //3AP
            slaveSrcPipeId = PIPE_ISP;
            slaveSrcBufferPos = CAPTURE_NODE_5; //ISPC
            break;
        case DUAL_PREVIEW_MODE_SW:
            masterSrcPipeId = PIPE_ISP;
            slaveSrcPipeId = PIPE_ISP;
            masterSrcBufferPos = CAPTURE_NODE_17;
            slaveSrcBufferPos = CAPTURE_NODE_17;
            break;
        default:
            break;
        }
    } else {
        switch (m_parameters->getDualReprocessingMode()) {
        case DUAL_REPROCESSING_MODE_HW:
            masterSrcPipeId = PIPE_3AA_REPROCESSING;
            slaveSrcPipeId = PIPE_ISP_REPROCESSING;
            masterSrcBufferPos = CAPTURE_NODE_4;
            slaveSrcBufferPos = CAPTURE_NODE_5;
            break;
        case DUAL_REPROCESSING_MODE_SW:
            masterSrcPipeId = PIPE_ISP_REPROCESSING;
            slaveSrcPipeId = PIPE_ISP_REPROCESSING;
            masterSrcBufferPos = CAPTURE_NODE_17;
            slaveSrcBufferPos = CAPTURE_NODE_17;
            break;
        default:
            break;
        }
    }

    ret = m_inputFrameQ->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGW("wait timeout");
            numOfProcessQ = m_slaveFrameQ->getSizeOfProcessQ();
            for (int i = 0; i < numOfProcessQ; i++) {
                newFrame = NULL;
                ret = m_slaveFrameQ->popProcessQ(&newFrame);
                if (ret != NO_ERROR) {
                    CLOGE("popProcessQ fail!, ret(%d)", ret);
                } else {
                    newFrame->setFrameState(FRAME_STATE_SKIPPED);
                    m_outputFrameQ->pushProcessQ(&newFrame);
                }
            }

            numOfProcessQ = m_masterFrameQ->getSizeOfProcessQ();
            for (int i = 0; i < numOfProcessQ; i++) {
                newFrame = NULL;
                ret = m_masterFrameQ->popProcessQ(&newFrame);
                if (ret != NO_ERROR) {
                    CLOGE("popProcessQ fail!, ret(%d)", ret);
                } else {
                    newFrame->setFrameState(FRAME_STATE_SKIPPED);
                    m_outputFrameQ->pushProcessQ(&newFrame);
                }
            }
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

    entity_buffer_state_t bufferState;

    CLOGD("input frame (isSrc:%d, Cam:%d, Fcount:%d, Sync:%d, Type:%d, Time:%lld, State:%d)",
            isSrc,
            newFrame->getCameraId(),
            newFrame->getFrameCount(),
            newFrame->getSyncType(),
            newFrame->getFrameType(),
            newFrame->getTimeStamp(),
            newFrame->getFrameState());

    /* check the frame state */
    switch (newFrame->getFrameState()) {
    case FRAME_STATE_SKIPPED:
    case FRAME_STATE_INVALID:
        if (newFrame->getFrameType() == FRAME_TYPE_PREVIEW_DUAL_MASTER) {
            CLOGE("Master frame(%d) state is invalid(%d)",
                newFrame->getFrameCount(), newFrame->getFrameState());
            newFrame->setFrameState(FRAME_STATE_SKIPPED);
            m_outputFrameQ->pushProcessQ(&newFrame);
            goto master_not_sync;
        } else if (newFrame->getFrameType() == FRAME_TYPE_PREVIEW_DUAL_SLAVE) {
            CLOGE("Slave frame(%d) state is invalid(%d)",
                newFrame->getFrameCount(), newFrame->getFrameState());
            newFrame->setFrameState(FRAME_STATE_SKIPPED);
            m_outputFrameQ->pushProcessQ(&newFrame);
            goto slave_not_sync;
        }
        break;
    default:
        break;
    }

    /* check the frame type */
    switch (newFrame->getFrameType()) {
    case FRAME_TYPE_INTERNAL:
        if ((internalLogCount[m_cameraId]++ % 100) == 0) {
            CLOGI("[INTERNAL_FRAME] frame(%d) type(%d), (%d)",
                    newFrame->getFrameCount(), newFrame->getFrameType(), internalLogCount[m_cameraId]);
        }
        newFrame->setFrameState(FRAME_STATE_SKIPPED);
        /* No break: Same as preview */
    case FRAME_TYPE_PREVIEW:
    case FRAME_TYPE_REPROCESSING:
        goto func_exit;
        break;
    case FRAME_TYPE_PREVIEW_DUAL_MASTER:
    case FRAME_TYPE_REPROCESSING_DUAL_MASTER:
    {
        m_masterFrameQ->pushProcessQ(&newFrame);
master_not_sync:
        if (m_masterFrameQ->getSizeOfProcessQ() > 0) {
            numOfProcessQ = m_slaveFrameQ->getSizeOfProcessQ();
            if (numOfProcessQ > 0) {
                for (int i = 0; i < numOfProcessQ; i++) {
                    compareFrame = NULL;
                    ret = m_slaveFrameQ->popProcessQ(&compareFrame);
                    if (ret != NO_ERROR) {
                        CLOGE("popProcessQ fail!, ret(%d)", ret);
                        continue;
                    }

                    newFrame = NULL;
                    ret = m_masterFrameQ->popProcessQ(&newFrame);
                    if (ret != NO_ERROR) {
                        CLOGE("popProcessQ fail!, ret(%d)", ret);
                        continue;
                    }

                    /* ToDo: frame sync
                     * if (newFrame->getFrameCount() == compareFrame->getFrameCount()) { */
                    if (1) {
                        ret = m_syncFrame(newFrame, compareFrame);
                        if (ret != NO_ERROR) {
                            CLOGE("Frame(count:%d) sync fail!!, ret(%d)", newFrame->getFrameCount(), ret);
                        }
                        isSync = true;
                        ret = newFrame->setEntityState(getPipeId(), ENTITY_STATE_FRAME_DONE);
                        if (ret != NO_ERROR) {
                            CLOGE("setEntityState(%d, ENTITY_STATE_FRAME_DONE) fail", getPipeId());
                        }
                        m_outputFrameQ->pushProcessQ(&newFrame);
                        m_outputFrameQ->pushProcessQ(&compareFrame);

                        if (m_masterFrameQ->getSizeOfProcessQ() > 0) {
                            newFrame = NULL;

                            m_masterFrameQ->popProcessQ(&newFrame);
                            if (ret != NO_ERROR) {
                                CLOGE("popProcessQ fail!, ret(%d)", ret);
                            }

                            newFrame->setFrameState(FRAME_STATE_SKIPPED);
                            m_outputFrameQ->pushProcessQ(&newFrame);
                        }

                        if (m_slaveFrameQ->getSizeOfProcessQ() > 0) {
                            newFrame = NULL;
                            m_slaveFrameQ->popProcessQ(&newFrame);
                            if (ret != NO_ERROR) {
                                CLOGE("popProcessQ fail!, ret(%d)", ret);
                            }
                            newFrame->setFrameState(FRAME_STATE_SKIPPED);
                            m_outputFrameQ->pushProcessQ(&newFrame);
                        }
                        break;
                    } else {
                        m_slaveFrameQ->pushProcessQ(&compareFrame);
                        m_masterFrameQ->pushProcessQ(&newFrame);
                    }
                }
            }
        } else {
            if (m_slaveFrameQ->getSizeOfProcessQ() > 1) {
                newFrame = NULL;
                m_slaveFrameQ->popProcessQ(&newFrame);
                if (ret != NO_ERROR) {
                    CLOGE("popProcessQ fail!, ret(%d)", ret);
                }

                newFrame->setFrameState(FRAME_STATE_SKIPPED);
                m_outputFrameQ->pushProcessQ(&newFrame);
            }
        }

        if (m_masterFrameQ->getSizeOfProcessQ() > 1) {
            newFrame = NULL;

            m_masterFrameQ->popProcessQ(&newFrame);
            if (ret != NO_ERROR) {
                CLOGE("popProcessQ fail!, ret(%d)", ret);
            }

            newFrame->setFrameState(FRAME_STATE_SKIPPED);
            m_outputFrameQ->pushProcessQ(&newFrame);
        }

        return NO_ERROR;
    }
    case FRAME_TYPE_PREVIEW_DUAL_SLAVE:
    case FRAME_TYPE_REPROCESSING_DUAL_SLAVE:
    {
        m_slaveFrameQ->pushProcessQ(&newFrame);
slave_not_sync:
        if (m_slaveFrameQ->getSizeOfProcessQ() > 0) {
            numOfProcessQ = m_masterFrameQ->getSizeOfProcessQ();
            if (numOfProcessQ > 0) {
                for (int i = 0; i < numOfProcessQ; i++) {
                    compareFrame = NULL;
                    ret = m_masterFrameQ->popProcessQ(&compareFrame);
                    if (ret != NO_ERROR) {
                        CLOGE("popProcessQ fail!, ret(%d)", ret);
                        continue;
                    }

                    newFrame = NULL;
                    ret = m_slaveFrameQ->popProcessQ(&newFrame);
                    if (ret != NO_ERROR) {
                        CLOGE("popProcessQ fail!, ret(%d)", ret);
                        continue;
                    }

                    /* ToDo: frame sync
                     * if (newFrame->getFrameCount() == compareFrame->getFrameCount()) { */
                    if (1) {
                        ret = m_syncFrame(compareFrame, newFrame);
                        if (ret != NO_ERROR) {
                            CLOGE("Frame(count:%d) sync fail!!, ret(%d)", newFrame->getFrameCount(), ret);
                        }
                        isSync = true;
                        ret = compareFrame->setEntityState(getPipeId(), ENTITY_STATE_FRAME_DONE);
                        if (ret != NO_ERROR) {
                            CLOGE("setEntityState(%d, ENTITY_STATE_FRAME_DONE) fail", getPipeId());
                        }
                        m_outputFrameQ->pushProcessQ(&compareFrame);
                        m_outputFrameQ->pushProcessQ(&newFrame);

                        if (m_masterFrameQ->getSizeOfProcessQ() > 0) {
                            newFrame = NULL;
                            m_masterFrameQ->popProcessQ(&newFrame);
                            if (ret != NO_ERROR) {
                                CLOGE("popProcessQ fail!, ret(%d)", ret);
                            }

                            newFrame->setFrameState(FRAME_STATE_SKIPPED);
                            m_outputFrameQ->pushProcessQ(&newFrame);
                        }

                        if (m_slaveFrameQ->getSizeOfProcessQ() > 0) {
                            newFrame = NULL;
                            m_slaveFrameQ->popProcessQ(&newFrame);
                            if (ret != NO_ERROR) {
                                CLOGE("popProcessQ fail!, ret(%d)", ret);
                            }

                            newFrame->setFrameState(FRAME_STATE_SKIPPED);
                            m_outputFrameQ->pushProcessQ(&newFrame);
                        }
                        break;
                    } else {
                        m_masterFrameQ->pushProcessQ(&compareFrame);
                        m_slaveFrameQ->pushProcessQ(&newFrame);
                    }
                }
            }
        } else {
            if (m_masterFrameQ->getSizeOfProcessQ() > 1) {
                newFrame = NULL;

                m_masterFrameQ->popProcessQ(&newFrame);
                if (ret != NO_ERROR) {
                    CLOGE("popProcessQ fail!, ret(%d)", ret);
                }

                newFrame->setFrameState(FRAME_STATE_SKIPPED);
                m_outputFrameQ->pushProcessQ(&newFrame);
            }
        }

        if (m_slaveFrameQ->getSizeOfProcessQ() > 1) {
            newFrame = NULL;
            m_slaveFrameQ->popProcessQ(&newFrame);
            if (ret != NO_ERROR) {
                CLOGE("popProcessQ fail!, ret(%d)", ret);
            }

            newFrame->setFrameState(FRAME_STATE_SKIPPED);
            m_outputFrameQ->pushProcessQ(&newFrame);
        }

        return NO_ERROR;
    }
    default:
        internalLogCount[m_cameraId] = 0;
        break;
    }

func_exit:

    m_outputFrameQ->pushProcessQ(&newFrame);

    return NO_ERROR;
}

status_t ExynosCameraPipeSync::m_syncFrame(ExynosCameraFrameSP_sptr_t masterFrame,
                                           ExynosCameraFrameSP_sptr_t slaveFrame)
{
    status_t ret = NO_ERROR;

    ExynosCameraBuffer buffer;
    int masterSrcPipeId = -1;
    int slaveSrcPipeId = -1;
    int dstPipeId = -1;
    enum NODE_TYPE masterSrcBufferPos = INVALID_NODE;
    enum NODE_TYPE slaveSrcBufferPos = INVALID_NODE;
    enum NODE_TYPE slaveDstBufferPos = INVALID_NODE;

    CLOGD("masterFrameCount(%d), slaveFrameCount(%d)",
            masterFrame->getFrameCount(), slaveFrame->getFrameCount());

    if (m_reprocessing == false) {
        switch (m_parameters->getDualPreviewMode()) {
        case DUAL_PREVIEW_MODE_HW:
            masterSrcPipeId = PIPE_3AA;
            slaveSrcPipeId = PIPE_ISP;
            if (m_parameters->getHwConnectionMode(PIPE_ISP, PIPE_DCP) == HW_CONNECTION_MODE_M2M) {
                dstPipeId = PIPE_DCP;
            } else {
                dstPipeId = PIPE_ISP;
            }
            masterSrcBufferPos = CAPTURE_NODE_4; //3AP
            slaveSrcBufferPos = CAPTURE_NODE_5; //ISPC
            slaveDstBufferPos = CAPTURE_NODE_5; //ISPC
            break;
        case DUAL_PREVIEW_MODE_SW:
            masterSrcPipeId = PIPE_ISP;
            slaveSrcPipeId = PIPE_ISP;
            dstPipeId = PIPE_FUSION;
            masterSrcBufferPos = CAPTURE_NODE_17;
            slaveSrcBufferPos = CAPTURE_NODE_17;
            break;
        default:
            break;
        }
    } else {
        switch (m_parameters->getDualReprocessingMode()) {
        case DUAL_REPROCESSING_MODE_HW:
            masterSrcPipeId = PIPE_3AA_REPROCESSING;
            slaveSrcPipeId = PIPE_ISP_REPROCESSING;
            if (m_parameters->getHwConnectionMode(PIPE_ISP_REPROCESSING, PIPE_DCP_REPROCESSING) == HW_CONNECTION_MODE_M2M) {
                dstPipeId = PIPE_DCP_REPROCESSING;
            } else {
                dstPipeId = PIPE_ISP_REPROCESSING;
            }
            masterSrcBufferPos = CAPTURE_NODE_4;
            slaveSrcBufferPos = CAPTURE_NODE_5;
            slaveDstBufferPos = CAPTURE_NODE_5;
            break;
        case DUAL_REPROCESSING_MODE_SW:
            masterSrcPipeId = PIPE_ISP_REPROCESSING;
            slaveSrcPipeId = PIPE_ISP_REPROCESSING;
            dstPipeId = PIPE_FUSION_REPROCESSING;
            masterSrcBufferPos = CAPTURE_NODE_17;
            slaveSrcBufferPos = CAPTURE_NODE_17;
            break;
        default:
            break;
        }
    }

    ret = slaveFrame->getDstBuffer(slaveSrcPipeId, &buffer, slaveSrcBufferPos);
    if (ret != NO_ERROR) {
        CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                slaveSrcPipeId, ret);
    }
    if (buffer.index < 0) {
        CLOGE("Invalid buffer index(%d), framecount(%d), pipeId(%d)",
                buffer.index, slaveFrame->getFrameCount(), slaveSrcPipeId);
    }

    if (m_reprocessing == false) {
        switch (m_parameters->getDualPreviewMode()) {
        case DUAL_PREVIEW_MODE_HW:
            ret = masterFrame->setDstBufferState(dstPipeId, ENTITY_BUFFER_STATE_REQUESTED, CAPTURE_NODE_12);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to setDstBufferState. ret %d",
                        masterFrame->getFrameCount(), buffer.index, ret);
            } else {
                ret = masterFrame->setDstBuffer(dstPipeId, buffer, CAPTURE_NODE_12);
                if (ret != NO_ERROR) {
                    CLOGE("setDstBuffer fail, pipeId(%d), ret(%d)",
                            dstPipeId, ret);
                }
            }
            break;
        case DUAL_PREVIEW_MODE_SW:
            ret = masterFrame->setSrcBuffer(dstPipeId, buffer, OUTPUT_NODE_2);
            if (ret != NO_ERROR) {
                CLOGE("setSrcBuffer fail, pipeId(%d), ret(%d)",
                        dstPipeId, ret);
            }
            break;
        default:
            break;
        }
    } else {
        switch (m_parameters->getDualReprocessingMode()) {
        case DUAL_REPROCESSING_MODE_HW:
            ret = masterFrame->setDstBufferState(dstPipeId, ENTITY_BUFFER_STATE_REQUESTED, CAPTURE_NODE_12);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to setDstBufferState. ret %d",
                        masterFrame->getFrameCount(), buffer.index, ret);
            } else {
                ret = masterFrame->setDstBuffer(dstPipeId, buffer, CAPTURE_NODE_12);
                if (ret != NO_ERROR) {
                    CLOGE("setDstBuffer fail, pipeId(%d), ret(%d)",
                            dstPipeId, ret);
                }
            }
            break;
        case DUAL_PREVIEW_MODE_SW:
            ret = masterFrame->setSrcBuffer(dstPipeId, buffer, OUTPUT_NODE_2);
            if (ret != NO_ERROR) {
                CLOGE("setSrcBuffer fail, pipeId(%d), ret(%d)",
                        dstPipeId, ret);
            }
            break;
        default:
            break;
        }
    }

    return NO_ERROR;
}

void ExynosCameraPipeSync::m_init(void)
{
    m_lastTimeStamp = 0;
}
}; /* namespace android */
