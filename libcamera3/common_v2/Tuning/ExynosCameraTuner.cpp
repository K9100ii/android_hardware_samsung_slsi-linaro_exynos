#define LOG_TAG "ExynosCameraTuner"

#include "ExynosCameraTuningCommand.h"
#include "ExynosCameraTuner.h"
#include "ExynosCameraBufferManager.h"
#include "ExynosCameraTuningDefines.h"
#include "modules/ExynosCameraTuningCommandIdList.h"
#include "modules/ExynosCameraTuningModule.h"

#ifdef USE_HAL3_CAMERA
#include "ExynosCamera3FrameFactory.h"
#else
#include "ExynosCameraFrameFactory.h"
#endif


namespace android {

ExynosCameraTuner* ExynosCameraTuner::m_self = NULL;

ExynosCameraTuner::ExynosCameraTuner() {
    m_shotState = T_STATE_SHOT_NONE;
    m_bImageReady = false;
    m_bImageReadyChecked = false;
    m_bStarted = false;
    m_flagResetImage = false;
    m_jsonRuningFlag = 0;

    m_parameters = NULL;
    m_configurations = NULL;

    m_frameFactory[0] = NULL;
    m_frameFactory[1] = NULL;

    for(int i = 0; i < MAX_NUM_PIPES; i++) {
        m_request[i] = false;
    }

    m_cmdQ = new command_queue_t;
    m_cmdQ->setWaitTime(30000000); //30ms

    m_cmdMap.clear();

    dumpCount = 0;
    m_tuneBufferManager = new ExynosCameraTuneBufferManager();
    m_dispatcher = new ExynosCameraTuningDispatcher();
}

ExynosCameraTuner::~ExynosCameraTuner() {
    if (m_tuneBufferManager != NULL) {
        delete m_tuneBufferManager;
        m_tuneBufferManager = NULL;
    }

    if (m_dispatcher != NULL) {
        delete m_dispatcher;
        m_dispatcher = NULL;
    }

    if (m_cmdQ != NULL) {
        m_cmdQ->release();
    }

    m_cmdMap.clear();
}

ExynosCameraTuner* ExynosCameraTuner::getInstance() {
    if (m_self == NULL) {
        m_self = new ExynosCameraTuner();
    }

    return m_self;
}

void ExynosCameraTuner::deleteInstance() {
    delete m_self;
    m_self = NULL;
}

status_t ExynosCameraTuner::start() {
    status_t ret = NO_ERROR;

    if (m_bStarted == false) {
        m_dispatcher->create();
        ret = m_parser.create(m_dispatcher);


        m_bStarted = true;
    } else {
        CLOGW2("ExynosCameraTuner is already started");
    }
    m_flagResetImage = true;
    /* HACK: HAL1 MCSC0 is Default Preveiw */
    m_request[PIPE_MCSC1] = true;

    return ret;
}

status_t ExynosCameraTuner::start(EXYNOSCAMERA_FRAME_FACTORY* previewFrameFactory, EXYNOSCAMERA_FRAME_FACTORY* captureFrameFactory) {
    status_t ret = NO_ERROR;

    m_frameFactory[0] = previewFrameFactory;
    m_frameFactory[1] = captureFrameFactory;

    ret = this->start();

    return ret;
}

status_t ExynosCameraTuner::stop() {
    status_t ret = NO_ERROR;
    uint32_t pipeId = 0;

    if (m_bStarted == false) return ret;

    for(int i = 0; i < MAX_NUM_PIPES; i++) {
        if (ExynosCameraTuner::getInstance()->getRequest(i) == true) {
            pipeId = i;
            break;
        }
    }

    Mutex::Autolock l(m_updateImageLock[pipeId]);

    m_bStarted = false;

    m_frameFactory[0] = NULL;
    m_frameFactory[1] = NULL;

    m_releaseBuffer();

    ret = m_parser.destroy();

    deleteInstance();

    return ret;
}

status_t ExynosCameraTuner::updateShot(struct camera2_shot_ext* shot, int target, int from) {
    status_t ret = NO_ERROR;

    if (m_bStarted == false) return INVALID_OPERATION;

    Mutex::Autolock l(m_updateShotLock);
    CLOGD2("m_shotState(%d), target(%s) ,from(%s)",
            m_shotState, (target == 0?"SENSOR":"ISP"), (from==0?"HAL":"MODULE"));

    switch(m_shotState) {
    case T_STATE_SHOT_NONE:
        if (from == TUNE_META_SET_FROM_HAL) {
            memcpy(&m_shot, shot, sizeof(struct camera2_shot_ext));
            m_shotState = T_STATE_SHOT_INIT;
        }
        break;
    case T_STATE_SHOT_APPLIED:
    case T_STATE_SHOT_TUNED:
        if (from == TUNE_META_SET_FROM_HAL) {
            if (target == TUNE_META_SET_TARGET_SENSOR
                || target == TUNE_META_SET_TARGET_3AA) {
                memcpy(&shot->shot.udm, &m_shot.shot.udm, sizeof(struct camera2_udm));
            }

            if (m_jsonRuningFlag > 0) {
                CLOGD2("TuneID(%d)", m_shot.tuning_info.tune_id);
                shot->tuning_info.tune_id = m_shot.tuning_info.tune_id;
                shot->tuning_info.json_addr = m_shot.tuning_info.json_addr;
                shot->tuning_info.json_size = m_shot.tuning_info.json_size;
            } else {
                shot->tuning_info.tune_id = 0;
                shot->tuning_info.json_addr = 0;
                shot->tuning_info.json_size = 0;
            }

            memcpy(&m_shot, shot, sizeof(struct camera2_shot_ext));
            //m_shotState = T_STATE_SHOT_INIT;

            if (m_cmdMap.empty() == false) {
                map<uint32_t, ExynosCameraTuningCmdSP_sptr_t>::iterator iter;
                ExynosCameraTuningCmdSP_sptr_t cmd;
                ExynosCameraTuningCommand::t_data *data = NULL;

                iter = m_cmdMap.begin();
                do {
                    cmd = iter->second;
                    data = cmd->getData();

                    if ((data->commandID & SIMMYCMD_SET_EMULATOR_START) == SIMMYCMD_SET_EMULATOR_START) {
                        ExynosCameraTuningController::setISPControl(data);

                        if (data->commandID == SIMMYCMD_SET_EMULATOR_START) {
                            ret = BAD_VALUE;
                        } else if (data->commandID == SIMMYCMD_SET_EMULATOR_STOP) {
                            m_cmdMap.clear();
                            break;
                        }
                    } else if ((data->commandID & SIMMYCMD_SET_VRA_START_STOP) == SIMMYCMD_SET_VRA_START_STOP) {
                        ExynosCameraTuningController::setVRAControl(data);
                    } else if ((data->commandID & SIMMYCMD_AF_CHANGE_MODE) == SIMMYCMD_AF_CHANGE_MODE) {
                        if (target == TUNE_META_SET_TARGET_SENSOR
                            || target == TUNE_META_SET_TARGET_3AA) {
                            ret = ExynosCameraTuningController::setAFControl(data);
                        }
                    } else if ((data->commandID & SIMMYCMD_SET_ISP_START_STOP) == SIMMYCMD_SET_ISP_START_STOP) {
                        if (target == TUNE_META_SET_TARGET_ISP) {
                            ret = ExynosCameraTuningController::setISPControl(data);
                        }
                    }

                    if (ret != NO_ERROR) {
                        map<uint32_t, ExynosCameraTuningCmdSP_sptr_t>::iterator tempIter = iter++;
                        m_cmdMap.erase(tempIter);
                        ret = NO_ERROR;
                    } else {
                        iter++;
                    }
                } while (iter != m_cmdMap.end());
            }

            memcpy(shot, &m_shot, sizeof(struct camera2_shot_ext));
            m_shotState = T_STATE_SHOT_APPLIED;
        }
    case T_STATE_SHOT_INIT:
        if (from == TUNE_META_SET_FROM_TUNING_MODULE) {
            memcpy(&m_shot, shot, sizeof(struct camera2_shot_ext));
            m_shotState = T_STATE_SHOT_TUNED;
        }
        break;
#if 0
    case T_STATE_SHOT_TUNED:
        if (from == TUNE_META_SET_FROM_HAL) {
            memcpy(shot, &m_shot, sizeof(struct camera2_shot_ext));
            /*
            shot->tuning_info.tune_id = m_shot.tuning_info.tune_id;
            shot->tuning_info.json_addr = m_shot.tuning_info.json_addr;
            shot->tuning_info.json_size = m_shot.tuning_info.json_size;
            */
            m_shotState = T_STATE_SHOT_APPLIED;

#if 0
            if (m_jsonRuningFlag > 0) {
                CLOGD2("reset json meta");
                m_shot.tuning_info.tune_id = 0x0;
                m_shot.tuning_info.json_addr = 0;
                m_shot.tuning_info.json_size = 0;
                m_shotState = T_STATE_SHOT_TUNED;

                /*
                if (m_jsonRuningFlag == TUNE_JSON_WRITE) {
                    m_jsonRuningFlag = 0;
                }
                */
            }
#endif
        }
        break;
#endif
    }

    return ret;
}

status_t ExynosCameraTuner::forceSetShot(struct camera2_shot_ext* shot) {
    status_t ret = NO_ERROR;

    if (m_bStarted == false) return INVALID_OPERATION;

    CLOGD2("m_shotState(%d)", m_shotState);

    memcpy(&m_shot, shot, sizeof(struct camera2_shot_ext));

    return ret;
}

status_t ExynosCameraTuner::getShot(struct camera2_shot_ext* shot) {
    status_t ret = NO_ERROR;

    if (m_bStarted == false) return INVALID_OPERATION;

    CLOGD2("m_shotState(%d)", m_shotState);

    Mutex::Autolock l(m_updateShotLock);

    if (m_shotState > T_STATE_SHOT_NONE) {
        memcpy(shot, &m_shot, sizeof(struct camera2_shot_ext));
        ret = NO_ERROR;
    } else {
        ret = INVALID_OPERATION;
    }

    return ret;
}

status_t ExynosCameraTuner::forceGetShot(struct camera2_shot_ext* shot) {
    status_t ret = NO_ERROR;

    if (m_bStarted == false) return INVALID_OPERATION;

    CLOGD2("m_shotState(%d)", m_shotState);

    if (m_shotState > T_STATE_SHOT_NONE) {
        memcpy(shot, &m_shot, sizeof(struct camera2_shot_ext));
        ret = NO_ERROR;
    } else {
        ret = INVALID_OPERATION;
    }

    return ret;
}

T_STATE_SHOT ExynosCameraTuner::getShotState(void) {
    return m_shotState;
}

status_t ExynosCameraTuner::updateImage(ExynosCameraFrameSP_sptr_t frame, int pipeId, EXYNOSCAMERA_FRAME_FACTORY* factory) {
    status_t ret = NO_ERROR;

    if (m_bStarted == false) return INVALID_OPERATION;

    CLOGD2("frameCount(%d), pipeId(%d)", frame->getFrameCount(), pipeId);

    if (m_parameters == NULL) {
        CLOGW2("m_paramters is NULL, return");
        return NO_ERROR;
    }

    if (m_flagResetImage == true) {
        m_releaseBuffer();
        m_flagResetImage = false;
    }
    m_setBuffer();

    ExynosCameraBuffer buffer;
    ExynosCameraBuffer targetBuffer;
    int targetBufferIndex = -1;
    int capturePipeId = -1;
    struct camera2_shot_ext *shot;

    switch(pipeId) {
    case PIPE_FLITE:
        capturePipeId = PIPE_VC0;
        if (m_configurations->getModeValue(CONFIGURATION_OBTE_TUNING_MODE) >= 1
            && frame->getRequest(capturePipeId) == true) {
            Mutex::Autolock l(m_updateImageLock[capturePipeId]);

            ret = frame->getDstBuffer(frame->getFrameDoneFirstEntity(pipeId)->getPipeId(), &buffer, factory->getNodeType(capturePipeId));
            if (ret != NO_ERROR || buffer.index < 0) {
                CLOGE2("Failed to get DST buffer. pipeId %d bufferIndex %d frameCount %d ret %d",
                        capturePipeId, buffer.index, frame->getFrameCount(), ret);
                return ret;
            }

            ret = m_bufferMgr[T_BAYER_IN_BUFFER_MGR]->getBuffer(&targetBufferIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &targetBuffer);
            if (ret != NO_ERROR) {
                CLOGE2("Buffer manager getBuffer fail, frameCount(%d), ret(%d)", frame->getFrameCount(), ret);
            }

            for (int i = 0; i < buffer.planeCount - 1; i++) {
                CLOGD2("copy addr[%d], size[%d]", i, targetBuffer.size[i]);
                if (buffer.size[i] > targetBuffer.size[i]) {
                    memcpy(buffer.addr[i], targetBuffer.addr[i], targetBuffer.size[i]);
                } else {
                    memcpy(buffer.addr[i], targetBuffer.addr[i], buffer.size[i]);
                }
            }

            ret = m_bufferMgr[T_BAYER_IN_BUFFER_MGR]->putBuffer(targetBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
            if (ret != NO_ERROR) {
                CLOGE2("Buffer manager release fail, index(%d), ret(%d)", targetBuffer.index, ret);
            }

            CLOGD2("Update Bayer Image");
        }

        break;
    case PIPE_3AA:
        capturePipeId = PIPE_VC0;
        if (frame->getRequest(capturePipeId) == true && getRequest(capturePipeId) == true) {
            Mutex::Autolock l(m_updateImageLock[capturePipeId]);

            ret = frame->getDstBuffer(frame->getFrameDoneFirstEntity(pipeId)->getPipeId(), &buffer, factory->getNodeType(capturePipeId));
            if (ret != NO_ERROR || buffer.index < 0) {
                CLOGE2("Failed to get DST buffer. pipeId %d bufferIndex %d frameCount %d ret %d",
                        capturePipeId, buffer.index, frame->getFrameCount(), ret);
                return ret;
            }

            ret = m_bufferMgr[T_BAYER_OUT_BUFFER_MGR]->getBuffer(&targetBufferIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &targetBuffer);
            if (ret != NO_ERROR) {
                CLOGE2("Buffer manager getBuffer fail, frameCount(%d), ret(%d)", frame->getFrameCount(), ret);
            }

            for (int i = 0; i < buffer.planeCount - 1; i++) {
                CLOGD2("copy addr[%d], size[%d]", i, targetBuffer.size[i]);
                if (buffer.size[i] > targetBuffer.size[i]) {
                    memcpy(targetBuffer.addr[i], buffer.addr[i], targetBuffer.size[i]);
                } else {
                    memcpy(targetBuffer.addr[i], buffer.addr[i], buffer.size[i]);
                }
            }

            ret = m_bufferMgr[T_BAYER_OUT_BUFFER_MGR]->putBuffer(targetBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
            if (ret != NO_ERROR) {
                CLOGE2("Buffer manager release fail, index(%d), ret(%d)", targetBuffer.index, ret);
            }

            m_bImageReady = true;
        }

        capturePipeId = PIPE_3AC;
        if (frame->getRequest(capturePipeId) == true && getRequest(capturePipeId) == true) {
            Mutex::Autolock l(m_updateImageLock[capturePipeId]);

            ret = frame->getDstBuffer(frame->getFrameDoneFirstEntity(pipeId)->getPipeId(), &buffer, factory->getNodeType(capturePipeId));
            if (ret != NO_ERROR || buffer.index < 0) {
                CLOGE2("Failed to get DST buffer. pipeId %d bufferIndex %d frameCount %d ret %d",
                        capturePipeId, buffer.index, frame->getFrameCount(), ret);
                return ret;
            }

            ret = m_bufferMgr[T_3AA0_BUFFER_MGR]->getBuffer(&targetBufferIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &targetBuffer);
            if (ret != NO_ERROR) {
                CLOGE2("Buffer manager getBuffer fail, frameCount(%d), ret(%d)", frame->getFrameCount(), ret);
            }

            for (int i = 0; i < buffer.planeCount - 1; i++) {
                CLOGD2("copy addr[%d], size[%d]", i, targetBuffer.size[i]);
                if (buffer.size[i] > targetBuffer.size[i]) {
                    memcpy(targetBuffer.addr[i], buffer.addr[i], targetBuffer.size[i]);
                } else {
                    memcpy(targetBuffer.addr[i], buffer.addr[i], buffer.size[i]);
                }
            }

            ret = m_bufferMgr[T_3AA0_BUFFER_MGR]->putBuffer(targetBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
            if (ret != NO_ERROR) {
                CLOGE2("Buffer manager release fail, index(%d), ret(%d)", targetBuffer.index, ret);
            }

            m_bImageReady = true;
        }

        capturePipeId = PIPE_3AP;
        if (frame->getRequest(capturePipeId) == true) {
            ret = frame->getDstBuffer(frame->getFrameDoneFirstEntity(pipeId)->getPipeId(), &buffer, factory->getNodeType(capturePipeId));
            if (ret != NO_ERROR || buffer.index < 0) {
                CLOGE2("Failed to get DST buffer. pipeId %d bufferIndex %d frameCount %d ret %d",
                        capturePipeId, buffer.index, frame->getFrameCount(), ret);
                return ret;
            }
            shot = (camera2_shot_ext *)buffer.addr[buffer.getMetaPlaneIndex()];

            updateShot(shot, TUNE_META_SET_TARGET_ISP, TUNE_META_SET_FROM_HAL);
            frame->setMetaData(shot);

            ExynosCameraTuningCmdSP_sptr_t command = getCommand();
            command->setCommandGroup(ExynosCameraTuningCommand::UPDATE_EXIF);
            m_dispatcher->dispatch(command);
        }

        break;
    case PIPE_ISP:
        if (m_jsonRuningFlag == TUNE_JSON_WRITE
            || m_jsonRuningFlag == TUNE_JSON_READ) {
            CLOGD2("json runing flag(%d)", m_jsonRuningFlag);

            //frame->getMetaData(&shot);

            ret = frame->getSrcBuffer(pipeId, &buffer);
            if (ret != NO_ERROR || buffer.index < 0) {
                CLOGE2("Failed to get DST buffer. pipeId %d bufferIndex %d frameCount %d ret %d",
                        capturePipeId, buffer.index, frame->getFrameCount(), ret);
                return ret;
            }

            shot = (camera2_shot_ext *)buffer.addr[buffer.getMetaPlaneIndex()];
            CLOGD2("frame(%d, %d), json read size(%d)", frame->getFrameCount(), shot->shot.dm.request.frameCount, shot->tuning_info.json_size);

            if (shot->tuning_info.json_size > 0) {
                ExynosCameraTuningCmdSP_sptr_t command = getCommand();
                if (m_jsonRuningFlag == TUNE_JSON_WRITE) {
                    command->setCommandGroup(ExynosCameraTuningCommand::WRITE_JSON);
                    command->getData()->commandID = TUNE_JSON_INTERNAL_CMD_WRITE_DONE;
                    command->getData()->moduleCmd.length = shot->tuning_info.json_size;
                } else if (m_jsonRuningFlag == TUNE_JSON_READ) {
                    command->setCommandGroup(ExynosCameraTuningCommand::WRITE_JSON);
                    command->getData()->commandID = TUNE_JSON_INTERNAL_CMD_READ_DONE;
                    command->getData()->moduleCmd.length = shot->tuning_info.json_size;
                }

                m_dispatcher->dispatch(command);

                m_jsonRuningFlag = 0;
            }
        }

        if (m_bImageReadyChecked == false) {
            capturePipeId = PIPE_ISPC;
            if (frame->getRequest(capturePipeId) == true && getRequest(capturePipeId) == true) {
                Mutex::Autolock l(m_updateImageLock[capturePipeId]);
                ret = frame->getDstBuffer(frame->getFrameDoneFirstEntity(pipeId)->getPipeId(), &buffer, factory->getNodeType(capturePipeId));
                if (ret != NO_ERROR || buffer.index < 0) {
                    CLOGE2("Failed to get DST buffer. pipeId %d bufferIndex %d frameCount %d ret %d",
                            capturePipeId, buffer.index, frame->getFrameCount(), ret);
                    return ret;
                }

                ret = m_bufferMgr[T_ISP_BUFFER_MGR]->getBuffer(&targetBufferIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &targetBuffer);
                if (ret != NO_ERROR) {
                    CLOGE2("Buffer manager getBuffer fail, frameCount(%d), ret(%d)", frame->getFrameCount(), ret);
                }

                for (int i = 0; i < buffer.planeCount - 1; i++) {
                    CLOGD2("copy addr[%d], size[%d]", i, targetBuffer.size[i]);
                    if (buffer.size[i] > targetBuffer.size[i]) {
                        memcpy(targetBuffer.addr[i], buffer.addr[i], targetBuffer.size[i]);
                    } else {
                        memcpy(targetBuffer.addr[i], buffer.addr[i], buffer.size[i]);
                    }
                }

                ret = m_bufferMgr[T_ISP_BUFFER_MGR]->putBuffer(targetBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
                if (ret != NO_ERROR) {
                    CLOGE2("Buffer manager release fail, index(%d), ret(%d)", targetBuffer.index, ret);
                }

                m_bImageReady = true;
            }

            capturePipeId = PIPE_MCSC0;
            //if (frame->getRequest(capturePipeId) == true) {
            if (frame->getRequest(capturePipeId) == true && getRequest(capturePipeId) == true) {
                Mutex::Autolock l(m_updateImageLock[capturePipeId]);
                ret = frame->getDstBuffer(frame->getFrameDoneFirstEntity(pipeId)->getPipeId(), &buffer, factory->getNodeType(capturePipeId));
                if (ret != NO_ERROR || buffer.index < 0) {
                    CLOGE2("Failed to get DST buffer. pipeId %d bufferIndex %d frameCount %d ret %d",
                            PIPE_MCSC0, buffer.index, frame->getFrameCount(), ret);
                    return ret;
                }

                ret = m_bufferMgr[T_MCSC0_BUFFER_MGR]->getBuffer(&targetBufferIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &targetBuffer);
                if (ret != NO_ERROR) {
                    CLOGE2("Buffer manager getBuffer fail, frameCount(%d), ret(%d)", frame->getFrameCount(), ret);
                }

                for (int i = 0; i < buffer.planeCount - 1; i++) {
                    CLOGD2("copy addr[%d], size[%d]", i, targetBuffer.size[i]);
                    if (buffer.size[i] > targetBuffer.size[i]) {
                        memcpy(targetBuffer.addr[i], buffer.addr[i], targetBuffer.size[i]);
                    } else {
                        memcpy(targetBuffer.addr[i], buffer.addr[i], buffer.size[i]);
                    }
                }

                ret = m_bufferMgr[T_MCSC0_BUFFER_MGR]->putBuffer(targetBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
                if (ret != NO_ERROR) {
                    CLOGE2("Buffer manager release fail, index(%d), ret(%d)", targetBuffer.index, ret);
                }

                m_bImageReady = true;
            } else {
                CLOGV2("frame->getRequest(%d) == false", capturePipeId);
            }

            capturePipeId = PIPE_MCSC1;
            if (frame->getRequest(capturePipeId) == true  && getRequest(capturePipeId) == true) {
                Mutex::Autolock l(m_updateImageLock[capturePipeId]);
                ret = frame->getDstBuffer(frame->getFrameDoneFirstEntity(pipeId)->getPipeId(), &buffer, factory->getNodeType(capturePipeId));
                if (ret != NO_ERROR || buffer.index < 0) {
                    CLOGE2("Failed to get DST buffer. pipeId %d bufferIndex %d frameCount %d ret %d",
                            PIPE_MCSC0, buffer.index, frame->getFrameCount(), ret);
                    return ret;
                }

                ret = m_bufferMgr[T_MCSC1_BUFFER_MGR]->getBuffer(&targetBufferIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &targetBuffer);
                if (ret != NO_ERROR) {
                    CLOGE2("Buffer manager getBuffer fail, frameCount(%d), ret(%d)", frame->getFrameCount(), ret);
                }

                for (int i = 0; i < buffer.planeCount - 1; i++) {
                    CLOGD2("copy addr[%d], size[%d]", i, targetBuffer.size[i]);
                    if (buffer.size[i] > targetBuffer.size[i]) {
                        memcpy(targetBuffer.addr[i], buffer.addr[i], targetBuffer.size[i]);
                    } else {
                        memcpy(targetBuffer.addr[i], buffer.addr[i], buffer.size[i]);
                    }
                }

                ret = m_bufferMgr[T_MCSC1_BUFFER_MGR]->putBuffer(targetBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
                if (ret != NO_ERROR) {
                    CLOGE2("Buffer manager release fail, index(%d), ret(%d)", targetBuffer.index, ret);
                }

                m_bImageReady = true;
            } else {
                CLOGV2("frame->getRequest(%d) == false", capturePipeId);
            }

            capturePipeId = PIPE_MCSC2;
            if (frame->getRequest(capturePipeId) == true  && getRequest(capturePipeId) == true) {
                Mutex::Autolock l(m_updateImageLock[capturePipeId]);
                ret = frame->getDstBuffer(frame->getFrameDoneFirstEntity(pipeId)->getPipeId(), &buffer, factory->getNodeType(capturePipeId));
                if (ret != NO_ERROR || buffer.index < 0) {
                    CLOGE2("Failed to get DST buffer. pipeId %d bufferIndex %d frameCount %d ret %d",
                            PIPE_MCSC0, buffer.index, frame->getFrameCount(), ret);
                    return ret;
                }

                ret = m_bufferMgr[T_MCSC2_BUFFER_MGR]->getBuffer(&targetBufferIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &targetBuffer);
                if (ret != NO_ERROR) {
                    CLOGE2("Buffer manager getBuffer fail, frameCount(%d), ret(%d)", frame->getFrameCount(), ret);
                }

                for (int i = 0; i < buffer.planeCount - 1; i++) {
                    CLOGD2("copy addr[%d], size[%d]", i, targetBuffer.size[i]);
                    if (buffer.size[i] > targetBuffer.size[i]) {
                        memcpy(targetBuffer.addr[i], buffer.addr[i], targetBuffer.size[i]);
                    } else {
                        memcpy(targetBuffer.addr[i], buffer.addr[i], buffer.size[i]);
                    }
                }

                ret = m_bufferMgr[T_MCSC2_BUFFER_MGR]->putBuffer(targetBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
                if (ret != NO_ERROR) {
                    CLOGE2("Buffer manager release fail, index(%d), ret(%d)", targetBuffer.index, ret);
                }

                m_bImageReady = true;
            } else {
                CLOGV2("frame->getRequest(%d) == false", capturePipeId);
            }
        }

        break;
    }

    return ret;
}

status_t ExynosCameraTuner::getImage(void)
{
    status_t ret = NO_ERROR;

    if (m_bStarted == false) return INVALID_OPERATION;

    int socketFd = m_parser.getSocketFd(TUNE_SOCKET_IMAGE);
    int writeRet = -1;
    int targetBufferIndex = -1;
    uint32_t lockPipeId = 0;
    T_BUFFER_MGR_ID bufferMgrIdx = T_BUFFER_MGR_MAX;
    ExynosCameraBuffer targetBuffer;

    if (socketFd < 0) {
        CLOGD2("Invalid socketFD(%d)", socketFd);
        return NO_ERROR;
    }

    for (uint32_t i = 0; i < MAX_NUM_PIPES; i++) {
        if (m_request[i] == true) {
            switch (i) {
            case PIPE_VC0:
                bufferMgrIdx = T_BAYER_OUT_BUFFER_MGR;
                break;
            case PIPE_3AC:
                bufferMgrIdx = T_3AA0_BUFFER_MGR;
                break;
            case PIPE_ISPC:
                bufferMgrIdx = T_ISP_BUFFER_MGR;
                break;
            case PIPE_MCSC0:
                bufferMgrIdx = T_MCSC0_BUFFER_MGR;
                break;
            case PIPE_MCSC1:
                bufferMgrIdx = T_MCSC1_BUFFER_MGR;
                break;
            default:
                CLOGE2("Invalid request Pipe(%d)", i);
                break;
            }
            lockPipeId = i;
            break;
        }
    }

    if (m_bufferMgr[bufferMgrIdx] == NULL) {
        CLOGD2("Buffer manager is NULL(index:%d)", bufferMgrIdx);
        return NO_ERROR;
    }

    Mutex::Autolock l(m_updateImageLock[lockPipeId]);

    m_request[lockPipeId] = false;
    ret = m_bufferMgr[bufferMgrIdx]->getBuffer(&targetBufferIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &targetBuffer);
    if (ret != NO_ERROR) {
        CLOGE2("Buffer manager getBuffer fail, ret(%d)", ret);
    }

    if (targetBufferIndex < 0) {
        CLOGD2("Invalid Buffer index(%d)", targetBufferIndex);
        return NO_ERROR;
    }


#if 0 /* Dump to file */
    char filePath[70];
    memset(filePath, 0, sizeof(filePath));
    snprintf(filePath, sizeof(filePath), "/data/OBTE/OBTE%d.yuv", dumpCount++);
    dumpToFile2plane((char *)filePath, targetBuffer.addr[0], targetBuffer.addr[1], targetBuffer.size[0], targetBuffer.size[1]);
#endif

    //for (uint32_t i = 0; i < targetBuffer.planeCount; i++) {
    for (uint32_t i = 0; i < (uint32_t)(targetBuffer.planeCount - 1); i++) {
        uint32_t writeSize = (524288);
        char *writeAddr = targetBuffer.addr[i];
        uint32_t remainingSize = targetBuffer.size[i];
        while (remainingSize > 0) {
            if (m_bStarted == false) {
                break;;
            }

            if (writeSize > remainingSize) {
                writeSize = remainingSize;
            }

            CLOGD2("write image size(%d), remainingSize(%d)", writeSize, remainingSize);
            writeRet = write(socketFd, writeAddr, writeSize);
            if (writeRet < 0) {
                CLOGD2("Write fail, size(%d)", writeRet);
                continue;
            }

            writeAddr += writeSize;
            remainingSize -= writeSize;
            //usleep(1000);
        }
    }

    ret = m_bufferMgr[bufferMgrIdx]->putBuffer(targetBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
    if (ret != NO_ERROR) {
        CLOGE2("Buffer manager release fail, index(%d), ret(%d)", targetBuffer.index, ret);
    }

    m_request[lockPipeId] = true;
    m_bImageReady = false;
    m_bImageReadyChecked = false;

    CLOGD2("Image read done");

    return NO_ERROR;
}

status_t ExynosCameraTuner::setImage(void)
{
    status_t ret = NO_ERROR;

    int socketFd = m_parser.getSocketFd(TUNE_SOCKET_IMAGE);
    int readRet = -1;
    int targetBufferIndex = -1;
    T_BUFFER_MGR_ID bufferMgrIdx = T_BAYER_IN_BUFFER_MGR;
    ExynosCameraBuffer targetBuffer;

    if (socketFd < 0) {
        CLOGD2("Invalid socketFD(%d)", socketFd);
        return NO_ERROR;
    }

    if (m_bufferMgr[bufferMgrIdx] == NULL) {
        CLOGD2("Buffer manager is NULL(index:%d)", bufferMgrIdx);
        return NO_ERROR;
    }

    ret = m_bufferMgr[bufferMgrIdx]->getBuffer(&targetBufferIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &targetBuffer);
    if (ret != NO_ERROR) {
        CLOGE2("Buffer manager getBuffer fail, ret(%d)", ret);
    }

    if (targetBufferIndex < 0) {
        CLOGD2("Invalid Buffer index(%d)", targetBufferIndex);
        return NO_ERROR;
    }


#if 0 /* Dump to file */
    char filePath[70];
    memset(filePath, 0, sizeof(filePath));
    snprintf(filePath, sizeof(filePath), "/data/OBTE/OBTE%d.yuv", dumpCount++);
    dumpToFile2plane((char *)filePath, targetBuffer.addr[0], targetBuffer.addr[1], targetBuffer.size[0], targetBuffer.size[1]);
#endif

    //for (uint32_t i = 0; i < targetBuffer.planeCount; i++) {
    for (uint32_t i = 0; i < (uint32_t)(targetBuffer.planeCount - 1); i++) {
        uint32_t readSize = 524288;
        char *readAddr = targetBuffer.addr[i];
        uint32_t remainingSize = targetBuffer.size[i];
        while (remainingSize > 0) {
            if (m_bStarted == false) {
                break;;
            }

            if (readSize > remainingSize) {
                readSize = remainingSize;
            }

            CLOGD2("read image size(%d), remainingSize(%d)", readSize, remainingSize);
            readRet = read(socketFd, readAddr, readSize);
            if (readRet < 0) {
                CLOGD2("Write fail, size(%d)", readRet);
                continue;
            }

            readAddr += readSize;
            remainingSize -= readSize;
            //usleep(1000);
        }
    }

    ret = m_bufferMgr[bufferMgrIdx]->putBuffer(targetBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
    if (ret != NO_ERROR) {
        CLOGE2("Buffer manager release fail, index(%d), ret(%d)", targetBuffer.index, ret);
    }

    CLOGD2("Image write done");

    return NO_ERROR;
}

status_t ExynosCameraTuner::setControl(int value) {
    status_t ret = NO_ERROR;

    CLOGD2("%d", value);
    if (m_bStarted == false) return INVALID_OPERATION;

    /*
    if(m_jsonRuningFlag != 0) {
        CLOGE2("json operation is runing(0x%x)", m_jsonRuningFlag);
        return INVALID_OPERATION;
    }
    */

    if (m_frameFactory[0] != NULL) {
        ret = m_frameFactory[0]->setControl(V4L2_CID_IS_S_TUNING_CONFIG, value, PIPE_ISP);
    }

    m_jsonRuningFlag = value;

    return ret;
}

status_t ExynosCameraTuner::setRequest(int pipeId, bool enable) {
    status_t ret = NO_ERROR;

    //if (m_bStarted == false) return INVALID_OPERATION;

    Mutex::Autolock l(m_updateImageLock[pipeId]);
    CLOGD2("Pipe(%d), request(%d)", pipeId, enable);

#if 0
    if (m_frameFactory[0] != NULL) {
        /* HACK: HAL3 preview port */
        if (pipeId == PIPE_MCSC1) {
        } else {
            m_frameFactory[0]->setRequest(pipeId, enable);
        }
    }
#endif

    m_request[pipeId] = enable;

    if (m_bImageReadyChecked == false) {
        m_bImageReady = false;
    }

    return ret;
}

status_t ExynosCameraTuner::setConfigurations(ExynosCameraConfigurations *configurations)
{
    status_t ret = NO_ERROR;

    if (configurations == NULL) {
        return BAD_VALUE;
    }

    m_configurations = configurations;

    return ret;
}

ExynosCameraConfigurations* ExynosCameraTuner::getConfigurations(void)
{
    return m_configurations;
}

status_t ExynosCameraTuner::setParameters(ExynosCameraParameters *parameters)
{
    status_t ret = NO_ERROR;

    if (parameters == NULL) {
        return BAD_VALUE;
    }

    m_parameters = parameters;

    return ret;
}

ExynosCameraParameters* ExynosCameraTuner::getParameters(void)
{
    return m_parameters;
}

status_t ExynosCameraTuner::sendCommand(ExynosCameraTuningCmdSP_dptr_t cmd)
{
    m_cmdQ->pushProcessQ(&cmd);

    return NO_ERROR;
}

ExynosCameraTuningCmdSP_sptr_t ExynosCameraTuner::getCommand(void)
{
    ExynosCameraTuningCmdSP_sptr_t cmd = NULL;

    if (m_cmdQ->getSizeOfProcessQ() > 0) {
        m_cmdQ->popProcessQ(&cmd);
    } else {
        cmd = new ExynosCameraTuningCommand();
    }

    return cmd;
}

status_t ExynosCameraTuner::storeToMap(ExynosCameraTuningCmdSP_dptr_t cmd)
{
    CLOGD2("cmdID(%d)", cmd->getData()->commandID);
    map<uint32_t, ExynosCameraTuningCmdSP_sptr_t>::iterator iter;
    pair<map<uint32_t, ExynosCameraTuningCmdSP_sptr_t>::iterator, bool> mapRet;

    iter = m_cmdMap.find(cmd->getData()->commandID);
    if (iter != m_cmdMap.end()) {
        m_cmdMap.erase(cmd->getData()->commandID);
    }

    mapRet = m_cmdMap.insert(pair<uint32_t, ExynosCameraTuningCmdSP_sptr_t>(cmd->getData()->commandID, cmd));
    if (mapRet.second == false) {
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

status_t ExynosCameraTuner::eraseToMap(ExynosCameraTuningCmdSP_dptr_t cmd)
{
    CLOGD2("cmdID(%d)", cmd->getData()->commandID);
    map<uint32_t, ExynosCameraTuningCmdSP_sptr_t>::iterator iter;

    iter = m_cmdMap.find(cmd->getData()->commandID);
    if (iter == m_cmdMap.end()) {
        return INVALID_OPERATION;
    }

    m_cmdMap.erase(iter);

    return NO_ERROR;
}

bool ExynosCameraTuner::getRequest(int pipeId) {
    return m_request[pipeId];
}

status_t ExynosCameraTuner::resetImage(void)
{
    status_t ret = NO_ERROR;

    m_flagResetImage = true;

    return ret;
}

bool ExynosCameraTuner::isImageReady(void) {
    if (m_bImageReady == true) {
        m_bImageReadyChecked = true;
    }
    CLOGD2("HSY:%d", m_bImageReady);
    return m_bImageReady;
}

unsigned long ExynosCameraTuner::getBufferAddr(T_BUFFER_MGR_ID bufferMgr) {
    status_t ret = NO_ERROR;

    if (m_bufferMgr[bufferMgr] != NULL) {
        ExynosCameraBuffer buffer;
        int bufferIndex = -1;

        ret = m_bufferMgr[bufferMgr]->getBuffer(&bufferIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &buffer);
        if (ret != NO_ERROR) {
            CLOGE2("Buffer manager getBuffer fail, ret(%d)", ret);
        }

        ret = m_bufferMgr[bufferMgr]->putBuffer(buffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
        if (ret != NO_ERROR) {
            CLOGE2("Buffer manager release fail, index(%d), ret(%d)", buffer.index, ret);
        }

        return (unsigned long)buffer.addr[0];
    } else {
        return 0;
    }
}

status_t ExynosCameraTuner::m_setBuffer() {
    status_t ret = NO_ERROR;
    unsigned int bpp = 0;
    unsigned int planeCount = 1;
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    int hwPreviewFormat = 0;
    int hwSensorW = 0, hwSensorH = 0;
    int sensorMarginW = 0, sensorMarginH = 0;
    int mcscYuvW = 0, mcscYuvH = 0;
    int maxBufferCount = 1, minBufferCount = 1;

    for (int i = T_BAYER_IN_BUFFER_MGR; i < T_BUFFER_MGR_MAX; i++) {
        if(m_bufferMgr[i] == NULL) {
            m_bufferMgr[i] = m_tuneBufferManager->getBufferManager((T_BUFFER_MGR_ID)i);

            switch (i) {
            case T_BAYER_IN_BUFFER_MGR:
            case T_BAYER_OUT_BUFFER_MGR:
                m_parameters->getSize(HW_INFO_HW_SENSOR_SIZE, (uint32_t*)&hwSensorW, (uint32_t*)&hwSensorH);
                planeSize[0] = getBayerPlaneSize(hwSensorW, hwSensorH, m_parameters->getBayerFormat(PIPE_FLITE));
                planeCount = 1;
                break;
            case T_3AA0_BUFFER_MGR:
            case T_3AA1_BUFFER_MGR:
            case T_ISP_BUFFER_MGR:
                m_parameters->getSize(HW_INFO_HW_SENSOR_SIZE, (uint32_t*)&hwSensorW, (uint32_t*)&hwSensorH);
                m_parameters->getSize(HW_INFO_SENSOR_MARGIN_SIZE, (uint32_t*)&sensorMarginW, (uint32_t*)&sensorMarginH);
                hwSensorW -= sensorMarginW;
                hwSensorH -= sensorMarginH;
                planeSize[0] = getBayerPlaneSize(hwSensorW, hwSensorH, m_parameters->getBayerFormat(PIPE_3AP));
                planeCount = 1;
                break;
            case T_MCSC0_BUFFER_MGR:
                //TODO : get from parameters
                hwPreviewFormat = m_configurations->getYuvFormat(0);
                ret = getV4l2FormatInfo(hwPreviewFormat, &bpp, &planeCount);
                if (ret < 0) {
                    CLOGE2("getYuvFormatInfo(hwPreviewFormat(%x)) fail", hwPreviewFormat);
                }

                m_parameters->getSize(HW_INFO_HW_YUV_SIZE, (uint32_t*)&mcscYuvW, (uint32_t*)&mcscYuvH, 0);

                ret = getYuvPlaneSize(hwPreviewFormat, planeSize, mcscYuvW, mcscYuvH);
                if (ret < 0) {
                    CLOGE2("getYuvPlaneSize(hwPreviewFormat(%x)) fail", hwPreviewFormat);
                }

                break;
            case T_MCSC1_BUFFER_MGR:
                //TODO : get from parameters
                hwPreviewFormat = m_configurations->getYuvFormat(1);
                ret = getV4l2FormatInfo(hwPreviewFormat, &bpp, &planeCount);
                if (ret < 0) {
                    CLOGE2("getYuvFormatInfo(hwPreviewFormat(%x)) fail", hwPreviewFormat);
                }

                m_parameters->getSize(HW_INFO_HW_YUV_SIZE, (uint32_t*)&mcscYuvW, (uint32_t*)&mcscYuvH, 1);

                ret = getYuvPlaneSize(hwPreviewFormat, planeSize, mcscYuvW, mcscYuvH);
                if (ret < 0) {
                    CLOGE2("getYuvPlaneSize(hwPreviewFormat(%x)) fail", hwPreviewFormat);
                }

                break;
            case T_MCSC2_BUFFER_MGR:
                //TODO : get from parameters
                hwPreviewFormat = m_configurations->getYuvFormat(2);
                ret = getV4l2FormatInfo(hwPreviewFormat, &bpp, &planeCount);
                if (ret < 0) {
                    CLOGE2("getYuvFormatInfo(hwPreviewFormat(%x)) fail", hwPreviewFormat);
                }

                m_parameters->getSize(HW_INFO_HW_YUV_SIZE, (uint32_t*)&mcscYuvW, (uint32_t*)&mcscYuvH, 2);

                ret = getYuvPlaneSize(hwPreviewFormat, planeSize, mcscYuvW, mcscYuvH);
                if (ret < 0) {
                    CLOGE2("getYuvPlaneSize(hwPreviewFormat(%x)) fail", hwPreviewFormat);
                }

                break;
            case T_JSON_READ_BUFFER_MGR:
            case T_JSON_WRITE_BUFFER_MGR:
            default:
                break;
            }

            /* meta */
            planeCount++;

            m_tuneBufferManager->allocBuffers(m_bufferMgr[i], planeCount, planeSize, bytesPerLine,
                    minBufferCount, maxBufferCount, 1/*batch size*/,
                    EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE, true/*createMetaPlane*/, true/*needMmap*/);

            CLOGD2("BufferMgr(%d) allocation success!", i);
        }
    }

    return ret;
}

status_t ExynosCameraTuner::m_releaseBuffer() {
    for (int i = T_BAYER_IN_BUFFER_MGR; i < T_BUFFER_MGR_MAX; i++) {
        if(m_bufferMgr[i] != NULL) {
            m_bufferMgr[i]->deinit();
            m_bufferMgr[i] = NULL;
        }
    }

    return NO_ERROR;
}

ExynosCameraTuneBufferManager::ExynosCameraTuneBufferManager() {
    status_t ret = NO_ERROR;

    for (int idx = T_BAYER_IN_BUFFER_MGR; idx < T_BUFFER_MGR_MAX; idx++) {
        m_managerList[idx].id = (T_BUFFER_MGR_ID)(idx + 1);
        m_managerList[idx].pManager = NULL;
    }

    //create ion allocator
    int retry = 0;
    do {
        retry++;
        CLOGI2("try(%d) to create IonAllocator", retry);
        m_allocator = new ExynosCameraIonAllocator();
        ret = m_allocator->init(false);
        if (ret < 0)
            CLOGE2("create IonAllocator fail (retryCount=%d)", retry);
        else {
            CLOGD2("m_createIonAllocator success (allocator=%p)", m_allocator);
            break;
        }
    } while (ret < 0 && retry < 3);

    if (ret < 0 && retry >=3) {
        CLOGE2("create IonAllocator fail (retryCount=%d)", retry);
        ret = INVALID_OPERATION;
    }
}

ExynosCameraTuneBufferManager::~ExynosCameraTuneBufferManager() {
    for (int idx = T_BAYER_IN_BUFFER_MGR; idx < T_BUFFER_MGR_MAX; idx++) {
        if( m_managerList[idx].pManager != NULL) {
            m_managerList[idx].pManager->deinit();
            m_managerList[idx].pManager = NULL;
        }
    }

    if (m_allocator != NULL) {
        delete m_allocator;
    }
}

ExynosCameraBufferManager*
ExynosCameraTuneBufferManager::getBufferManager(T_BUFFER_MGR_ID bufferMgrId) {
    if (m_managerList[bufferMgrId].pManager == NULL) {
        m_createBufferManager(bufferMgrId);
    }

    return m_managerList[bufferMgrId].pManager;
}

status_t
ExynosCameraTuneBufferManager::m_createBufferManager(T_BUFFER_MGR_ID bufferMgrId) {
    status_t ret = NO_ERROR;

    m_managerList[bufferMgrId].pManager = ExynosCameraBufferManager::createBufferManager(BUFFER_MANAGER_ION_TYPE);
    if (m_managerList[bufferMgrId].pManager == NULL) {
        ret = INVALID_OPERATION;
        CLOGE2("Fail to create buffer manager Id(%d)", bufferMgrId);
        return ret;
    }

    m_managerList[bufferMgrId].pManager->create("bufferManager", 0, m_allocator);

    return ret;
}

status_t
ExynosCameraTuneBufferManager::allocBuffers(
            ExynosCameraBufferManager *bufManager,
            int  planeCount,
            unsigned int *planeSize,
            __unused unsigned int *bytePerLine,
            int  minBufCount,
            int  maxBufCount,
            int batchSize,
            exynos_camera_buffer_type_t type,
            bool createMetaPlane,
            bool needMmap)
{
    int ret = 0;
    int retryCount = 20; /* 2Sec */
    buffer_manager_configuration_t bufConfig;

    if (bufManager == NULL) {
        CLOGE2("ERR(%s[%d]):BufferManager is NULL", __FUNCTION__, __LINE__);
        ret = BAD_VALUE;
        goto func_exit;
    }

retry_alloc:
    CLOGI2("setInfo(planeCount=%d, minBufCount=%d, maxBufCount=%d, batchSize=%d, type=%d, allocMode=%d, meta=%d, mmap=%d)",
        planeCount, minBufCount, maxBufCount, batchSize, (int)type, (int)BUFFER_MANAGER_ALLOCATION_ONDEMAND, createMetaPlane, needMmap);

    bufConfig.planeCount = planeCount;
    for (uint32_t i = 0; i < (uint32_t)planeCount; i++) {
        bufConfig.size[i] = planeSize[i];
    }
    bufConfig.startBufIndex = 31;
    bufConfig.reqBufCount = minBufCount;
    bufConfig.allowedMaxBufCount = maxBufCount;
    bufConfig.batchSize = batchSize;
    bufConfig.type = type;
    bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ONDEMAND;
    bufConfig.createMetaPlane = createMetaPlane;
    bufConfig.needMmap = needMmap;
    bufConfig.reservedMemoryCount = 0;

    ret = bufManager->setInfo(bufConfig);
    if (ret < 0) {
        CLOGE2("setInfo fail");
        goto func_exit;
    }

    ret = bufManager->alloc();
    if (ret < 0) {
        if (retryCount != 0
            && (type == EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE
            || type == EXYNOS_CAMERA_BUFFER_ION_CACHED_RESERVED_TYPE)) {
            CLOGE2("Alloc fail about reserved memory. retry alloc. (%d)", retryCount);
            usleep(100000); /* 100ms */
            retryCount--;
            goto retry_alloc;
        }

        CLOGE2("alloc fail");
        goto func_exit;
    }

func_exit:

    return ret;

}


};//namespace android

