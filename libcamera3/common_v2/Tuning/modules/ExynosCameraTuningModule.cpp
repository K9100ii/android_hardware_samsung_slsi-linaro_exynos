#define LOG_TAG "ExynosCameraTuningModule"

#include "ExynosCameraTuningModule.h"
#include "../ExynosCameraTuner.h"

#include "ExynosCameraThread.h"

namespace android {

ExynosCameraTuningModule::ExynosCameraTuningModule()
{
    m_tuningFD = -1;
    m_socketCmdFD = -1;
    m_socketJsonFD = -1;
    m_socketImageFD = -1;
    m_flagDestroy = false;
}

ExynosCameraTuningModule::~ExynosCameraTuningModule()
{
    destroy();
}

status_t ExynosCameraTuningModule::create(char* moduleName)
{
    status_t ret = NO_ERROR;

    m_cmdQ = new command_queue_t;
    m_cmdQ->setWaitTime(30000000); //30ms

    m_thread = new ExynosCameraThread<ExynosCameraTuningModule>(this, &ExynosCameraTuningModule::m_commandHandler, moduleName);

    m_thread->run(moduleName);//TODO : module's name

    return ret;
}

status_t ExynosCameraTuningModule::destroy()
{
    status_t ret = NO_ERROR;

    if (m_thread != NULL && m_cmdQ != NULL) {
        m_flagDestroy = true;
        m_thread->requestExit();
        m_cmdQ->sendCmd(WAKE_UP);
        m_thread->requestExitAndWait();
        m_cmdQ->release();
    }

    return ret;
}

status_t ExynosCameraTuningModule::sendCommand(ExynosCameraTuningCmdSP_dptr_t cmd)
{
    m_cmdQ->pushProcessQ(&cmd);

    return NO_ERROR;
}

bool ExynosCameraTuningModule::m_commandHandler()
{
    status_t ret = NO_ERROR;
    bool loop = true;

    int retryCount = 100; // 30ms * 100 = 3sec

    ExynosCameraTuningCmdSP_sptr_t cmd = NULL;
    CLOGD2("waiting command from dispather!!!!!!");

    while (retryCount > 0) {
        if (m_flagDestroy == true) {
            return false;
        }

        ret = m_cmdQ->waitAndPopProcessQ(&cmd);
        if (ret == TIMED_OUT) {
            if (retryCount%30 == 0) {
                CLOGW2("wait and pop timeout, ret(%d)", ret);
            }
        } else if (ret < 0) {
            CLOGE2("wait and pop fail, ret(%d)", ret);
            /* TODO: doing exception handling */
        } else {
            break;
        }

        retryCount--;
    }

    if (cmd == NULL) {
        CLOGE2("received command is NULL!!!");
        return loop;
    }

    CLOGD2("cmd(%d)", cmd->getCommandGroup());

    ret = m_OnCommandHandler(cmd);
    if (ret != NO_ERROR) {
        CLOGE2("m_OnCommandHandler is error(%d)", ret);
    }

    return loop;
}

status_t ExynosCameraTuningModule::setSocketFd(int socketFD, TUNE_SOCKET_ID socketId)
{
    if (socketFD < 0) {
        CLOGE2("Invalid socket FD(%d)", socketFD);
        return BAD_VALUE;
    }

    switch (socketId) {
    case TUNE_SOCKET_CMD:
        m_socketCmdFD = socketFD;
        break;
    case TUNE_SOCKET_JSON:
        m_socketJsonFD = socketFD;
        break;
    case TUNE_SOCKET_IMAGE:
        m_socketImageFD = socketFD;
        break;
    default:
        CLOGE2("Invalid socket ID(%d)", socketId);
        break;
    }

    return NO_ERROR;
}

status_t ExynosCameraTuningModule::setTuningFd(int tuningFD)
{
    if (tuningFD < 0) {
        CLOGE2("Invalid socket FD(%d)", tuningFD);
        return BAD_VALUE;
    }

    m_tuningFD = tuningFD;

    return NO_ERROR;
}

}; //namespace android
