#define LOG_TAG "ExynosCameraTuningJson"

#include "../ExynosCameraTuner.h"

#include "ExynosCameraTuningModule.h"
#include "ExynosCameraTuningCommandIdList.h"

namespace android {

ExynosCameraTuningJson::ExynosCameraTuningJson()
{
    CLOGD2("");

    m_tuneBufferManager = new ExynosCameraTuneBufferManager();

    m_jsonReadBufMgr = NULL;
    m_jsonWriteBufMgr = NULL;

    m_jsonState = T_JSON_STATE_WAIT;
    m_jsonReadyCheckCount = 0;
    m_jsonReadSize = 0;
    m_jsonReadRemainingSize = 0;
    m_jsonReadAddr = 0x0;

    m_jsonWriteSize = 0;
    m_jsonWriteRemainingSize = 0;
    m_jsonWriteAddr = 0x0;

    m_allocBuffer();

}

ExynosCameraTuningJson::~ExynosCameraTuningJson()
{
    if (m_tuneBufferManager != NULL) {
        delete m_tuneBufferManager;
        m_tuneBufferManager = NULL;
    }

    m_deallocBuffer();
}

status_t ExynosCameraTuningJson::m_allocBuffer() {
    CLOGD2("");

    status_t ret = NO_ERROR;
    int jsonBufferIndex = -1;
    unsigned int planeSize = 1024*1024;
    unsigned int bytesPerLine = 0;

    /* write buffer manager */
    if(m_jsonWriteBufMgr == NULL) {
        m_jsonWriteBufMgr = m_tuneBufferManager->getBufferManager(T_JSON_WRITE_BUFFER_MGR);

        m_tuneBufferManager->allocBuffers(m_jsonWriteBufMgr, 2, &planeSize, &bytesPerLine,
            1/*minBufferCount*/, 1/*maxBufferCount*/, 1/*batch size*/,
            EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE, true/*createMetaPlane*/, true/*needMmap*/);
    }

    ret = m_jsonWriteBufMgr->getBuffer(&jsonBufferIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &m_jsonWriteBuffer);
    if (jsonBufferIndex < 0 || ret != NO_ERROR) {
        CLOGE2("Failed to get WRITE buffer index(%d), ret(%d)", jsonBufferIndex, ret);
    }

    /* read buffer manager */
    if(m_jsonReadBufMgr == NULL) {
        m_jsonReadBufMgr = m_tuneBufferManager->getBufferManager(T_JSON_READ_BUFFER_MGR);

        m_tuneBufferManager->allocBuffers(m_jsonReadBufMgr, 2, &planeSize, &bytesPerLine,
            1/*minBufferCount*/, 1/*maxBufferCount*/, 1/*batch size*/,
            EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE, true/*createMetaPlane*/, true/*needMmap*/);
    }

    ret = m_jsonReadBufMgr->getBuffer(&jsonBufferIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &m_jsonReadBuffer);
    if (jsonBufferIndex < 0 || ret != NO_ERROR) {
        CLOGE2("Failed to get READ buffer index(%d), ret(%d)", jsonBufferIndex, ret);
    }

    m_jsonRead_mallocBuffer = (char*)malloc(planeSize);

    return ret;
}

status_t ExynosCameraTuningJson::m_deallocBuffer() {
    status_t ret = NO_ERROR;

    if(m_jsonWriteBufMgr) {
        m_jsonWriteBufMgr->deinit();
    }

    if(m_jsonReadBufMgr) {
        m_jsonReadBufMgr->deinit();
    }

    delete m_jsonRead_mallocBuffer;

    return ret;
}


status_t ExynosCameraTuningJson::m_OnCommandHandler(ExynosCameraTuningCmdSP_sptr_t cmd)
{
    status_t ret = NO_ERROR;
    CLOGD2("group(%d), command(%x), subCommand(%d)", cmd->getCommandGroup(), cmd->getData()->moduleCmd.command, cmd->getData()->moduleCmd.subCommand);

    uint8_t writeData[SIMMIAN_CTRL_MAXPACKET] = {0,};
    uint32_t writeSize = 0;
    int writeRet = -1;
    int readRet = -1;
    uint32_t value = cmd->getData()->moduleCmd.command;
    uint32_t index = cmd->getData()->moduleCmd.subCommand;
    uint32_t length = cmd->getData()->moduleCmd.length;

    if (cmd->getData()->commandID == TUNE_JSON_INTERNAL_CMD_WRITE_DONE) {
        //struct camera2_shot_ext shot_ext;
        //ret = ExynosCameraTuner::getInstance()->updateShot(&shot_ext, TUNE_META_SET_FROM_HAL);
        m_jsonState = T_JSON_STATE_WRITE_DONE;
    } else if (cmd->getData()->commandID == TUNE_JSON_INTERNAL_CMD_READ_DONE) {
        m_readDone(cmd->getData());
    } else if (cmd->getCommandGroup() == ExynosCameraTuningCommand::READ_JSON) {
        switch (value) {
        case 0xFFFF:
            CLOGD2("TUNE_JSON_INTERNAL_CMD_READY");
            /*
               if (m_jsonState == T_JSON_STATE_WRITE_READY
               || m_jsonState == T_JSON_STATE_READ_READY) {
             */
            if (m_jsonState == T_JSON_STATE_READY) {
                *(uint32_t *)writeData = 0x5A5AA5A5;
            } else {
                *writeData = 0;
            }

            CLOGD2("Write(%d)", length);
            CLOGD2("write(%d), data(%x,%x,%x,%x)", writeSize, writeData[0], writeData[1], writeData[2], writeData[3]);
            writeSize = length;
            writeRet = write(m_socketCmdFD, writeData, writeSize);
            if (writeRet < 0) {
                CLOGD2("Write fail, size(%d)", writeRet);
            }

            break;
        case 0xFFFD:
            CLOGD2("TUNE_JSON_INTERNAL_CMD_READ(index:%x)", index);
            if (index == 0x0101) {
                CLOGD2("TUNE_JSON_INTERNAL_CMD_READING(%x)", index);
                if (m_jsonState == T_JSON_STATE_READING) {
                    writeSize = length;
                    if (writeSize > m_jsonReadRemainingSize) {
                        writeSize = m_jsonReadRemainingSize;
                    }

                    do {
                        writeRet = write(m_socketJsonFD, m_jsonReadAddr, writeSize);
                        if (writeRet < 0) {
                            CLOGD2("Write fail, size(%d)", writeRet);
                            usleep(5000);
                        }
                    } while (writeRet <= 0);

                    m_jsonReadAddr += writeSize;
                    m_jsonReadRemainingSize -= writeSize;
                }
                CLOGD2("Write(%d), Remaining(%d)", length, m_jsonReadRemainingSize);
            }
            break;
        case 0xFFFC:
            CLOGD2("TUNE_JSON_INTERNAL_CMD_GET_READ_SIZE(index:%x)", index);
            if (index == 0x0100) {
                if (m_jsonState == T_JSON_STATE_READ_SIZE_DONE) {
                    if (m_jsonReadSize % 0x1000 == 0) {
                        m_jsonReadSize += 8;
                        m_jsonReadRemainingSize += 8;
                    }

                    *(uint32_t *)writeData = m_jsonReadSize;
                    *(char **)(writeData + 4) = m_jsonReadAddr;
                    CLOGD2("%x, %x, %x, %x", writeData[0], writeData[1], writeData[2], writeData[3]);
                } else {
                    *writeData = 0;
                }
            } else if (index == 0x0101) {
                if (m_jsonState == T_JSON_STATE_READ_SIZE_DONE) {
                    *(uint32_t *)writeData = 0xA5A5A5A5;
                } else {
                    *writeData = 0;
                }
            }

            writeSize = length;
            CLOGD2("write(%d), data(%x,%x,%x,%x)", writeSize, writeData[0], writeData[1], writeData[2], writeData[3]);
            if (writeSize > 0) {
                writeRet = write(m_socketCmdFD, writeData, writeSize);
                if (writeRet < 0) {
                    CLOGD2("Write fail, size(%d)", writeRet);
                }
            }

            break;
        default:
            CLOGD2("SY:Invalid value(%x)", value);
            break;
        }
    } else if (cmd->getCommandGroup() == ExynosCameraTuningCommand::WRITE_JSON) {
        switch(value) {
        case 0xFFFF:
            CLOGD2("TUNE_JSON_INTERNAL_CMD_READY(state:%d)", m_jsonState);
            if (m_jsonState == T_JSON_STATE_WAIT) {
                m_jsonState = T_JSON_STATE_READY;
            } else {
                m_jsonReadyCheckCount++;
                if (m_jsonReadyCheckCount >= 50) {
                    m_jsonState = T_JSON_STATE_WAIT;
                    m_jsonReadyCheckCount = 0;
                }
            }
            break;
        case 0xFFFE:
            CLOGD2("TUNE_JSON_INTERNAL_CMD_WRITE(index:%x)", index);
            if (index == 0x0100) {
                CLOGD2("TUNE_JSON_INTERNAL_CMD_WRITE_START(%x)", index);
                if (m_jsonState == T_JSON_STATE_READY) {
                    m_jsonWriteSize = cmd->getData()->commandAddr;
                    m_jsonWriteRemainingSize = m_jsonWriteSize;
                    m_jsonWriteAddr = m_jsonWriteBuffer.addr[0];

                    m_jsonState = T_JSON_STATE_WRITING;
                    length = 0;
                }
            } else if (index == 0x0101) {
                CLOGD2("TUNE_JSON_INTERNAL_CMD_WRITING(%x)", index);
                if (m_jsonState == T_JSON_STATE_WRITING) {
                    do {
                        readRet = read(m_socketJsonFD, m_jsonWriteAddr, length);
                        if (readRet <= 0) {
                            CLOGE2("file read fail, ret(%d)", readRet);
                            usleep(5000);
                        }
                        CLOGD2("ret(%d)", readRet);
                    } while (readRet <= 0);

                    m_jsonWriteAddr += length;
                    m_jsonWriteRemainingSize -= length;

                    length = 0;
                }
            } else if (index == 0x0102) {
                CLOGD2("TUNE_JSON_INTERNAL_CMD_WRITE_END(%x)", index);
                CLOGD2("remain:%d", m_jsonWriteRemainingSize);
                if (m_jsonState == T_JSON_STATE_WRITING
                    && m_jsonWriteRemainingSize == 0) {
#if 0 /* Dump to file */
                    char filePath[70];
                    memset(filePath, 0, sizeof(filePath));
                    snprintf(filePath, sizeof(filePath), "/data/OBTE/OBTE.json");
                    dumpToFile((char *)filePath, m_jsonWriteBuffer.addr[0], m_jsonWriteSize);
#endif

                    writeRet = write(m_tuningFD, (char *)m_jsonWriteBuffer.addr[0], m_jsonWriteSize);
                    if (writeRet < 0) {
                        CLOGD2("Write fail, size(%d)", writeRet);
                    }
                    CLOGD2("JSON write done size(%d)", writeRet);

                    m_writer(cmd->getData());
                    //struct camera2_shot_ext shot_ext;

                    //ret = ExynosCameraTuner::getInstance()->updateShot(&shot_ext, TUNE_META_SET_FROM_HAL);

                    //m_jsonState = T_JSON_STATE_WRITE_DONE;
                }
            }
            break;
        case 0xFFFD:
            CLOGD2("TUNE_JSON_INTERNAL_CMD_READ(index:%x)", index);
            if (index == 0x0100) {
                CLOGD2("TUNE_JSON_INTERNAL_CMD_READ_START(%x)", index);
                if (m_jsonState == T_JSON_STATE_READY) {
                    m_jsonState = T_JSON_STATE_READING;
                }
            } else if (index == 0x0102) {
                CLOGD2("TUNE_JSON_INTERNAL_CMD_READ_END(%x)", index);
                if (m_jsonState == T_JSON_STATE_READING) {
                    m_jsonState = T_JSON_STATE_READ_DONE;
                }
            }
            break;
        case 0xFFFC:
            CLOGD2("TUNE_JSON_INTERNAL_CMD_GET_READ_SIZE(index:%x)", index);
            if (index == 0x0100) {
                if (m_jsonState == T_JSON_STATE_READY) {
                    m_jsonReadSize = length;
                    m_reader(cmd->getData());
                    m_jsonState = T_JSON_STATE_READ_SIZE;
                    length = 0;
                    usleep(50000);
                } else {
                    *writeData = 0;
                }
            }

            break;
        default:
            CLOGD2("SY:Invalid value(%x)", value);
            break;
        }

        CLOGD2("length:%d", length);
        if (length > 0) {
            writeSize = length;
            CLOGD2("write(%d), data(%x,%x,%x,%x)", writeSize, writeData[0], writeData[1], writeData[2], writeData[3]);
            writeRet = write(m_socketCmdFD, writeData, writeSize);
            if (writeRet < 0) {
                CLOGD2("Write fail, size(%d)", writeRet);
            }
        }
    }

    if (cmd != NULL) {
        ret = ExynosCameraTuner::getInstance()->sendCommand(cmd);
    }

    return ret;
}

status_t ExynosCameraTuningJson::m_writer(__unused ExynosCameraTuningCommand::t_data* data)
{
    CLOGD2("");

    status_t ret = NO_ERROR;

    struct camera2_shot_ext shot_ext;

    do {
        ret = ExynosCameraTuner::getInstance()->getShot(&shot_ext);
        //usleep(100000);
    } while(ret != NO_ERROR);

    ExynosCameraTuner::getInstance()->setControl(TUNE_JSON_WRITE);
    if (ret != NO_ERROR) {
        CLOGE2("Failed to set control(TUNE_JSON_WRITE)");
    }

    //test
    //memcpy(m_jsonWriteBuffer.addr[0], m_jsonReadBuffer.addr[0], m_jsonReadBuffer.size[0]);

    shot_ext.tuning_info.tune_id = 0x102;
    shot_ext.tuning_info.json_addr = (uint64_t)m_jsonWriteBuffer.addr[0];
    shot_ext.tuning_info.json_size = m_jsonWriteSize;//(uint64_t)m_jsonWriteBuffer.size[0];

    CLOGD2("json write size(%d)", m_jsonWriteSize);

    ret = ExynosCameraTuner::getInstance()->updateShot(&shot_ext, TUNE_META_SET_TARGET_ISP, TUNE_META_SET_FROM_TUNING_MODULE);

    return ret;
}

status_t ExynosCameraTuningJson::m_reader(ExynosCameraTuningCommand::t_data* data)
{
    CLOGD2("");

    status_t ret = NO_ERROR;

    struct camera2_shot_ext shot_ext;

    do {
        ret = ExynosCameraTuner::getInstance()->getShot(&shot_ext);
        //usleep(100000);
    } while(ret != NO_ERROR);

    ret = ExynosCameraTuner::getInstance()->setControl(TUNE_JSON_READ);
    if (ret != NO_ERROR) {
        CLOGE2("Failed to set control(TUNE_JSON_READ)");
    }

    shot_ext.tuning_info.tune_id = data->commandAddr;
    //shot_ext.tuning_info.json_addr = (uint64_t)m_jsonReadBuffer.addr[0];
    //shot_ext.tuning_info.json_size = (uint32_t)m_jsonReadBuffer.size[0];

    /* Temp */
    shot_ext.tuning_info.json_addr = (uint64_t)m_jsonRead_mallocBuffer;
    shot_ext.tuning_info.json_size = 0;//m_jsonReadSize;//(uint31_t)(1024*1024);

    CLOGD2("json read size(%d)", m_jsonReadSize);
    ret = ExynosCameraTuner::getInstance()->updateShot(&shot_ext, TUNE_META_SET_TARGET_ISP, TUNE_META_SET_FROM_TUNING_MODULE);

    return ret;
}

status_t ExynosCameraTuningJson::m_readDone(ExynosCameraTuningCommand::t_data* data)
{
    CLOGD2("");

    status_t ret = NO_ERROR;

#if 0
    char filePath[70];
    memset(filePath, 0, sizeof(filePath));
    snprintf(filePath, sizeof(filePath), "/data/media/0/json_read.json");
#endif

    m_jsonReadSize = data->moduleCmd.length;

    ret = read(m_tuningFD, (char *)m_jsonReadBuffer.addr[0], m_jsonReadSize);
    if (ret < 0) {
        CLOGE2("Failed to read json(%d), ret(%d)", m_jsonReadSize, ret);
        return INVALID_OPERATION;
    } else {
        CLOGD2("json read size(%d), ret(%d)", m_jsonReadSize, ret);
    }

    m_jsonReadRemainingSize = m_jsonReadSize;
    m_jsonReadAddr = m_jsonReadBuffer.addr[0];
    m_jsonState = T_JSON_STATE_READ_SIZE_DONE;

#if 0
    int bRet = dumpToFile((char *)filePath, (char*)m_jsonReadBuffer.addr[0], m_jsonReadBuffer.size[0]);
    if (bRet != true) {
        CLOGE2("couldn't make a json file");
    }
#endif

    return ret;
}

}; //namespace android
