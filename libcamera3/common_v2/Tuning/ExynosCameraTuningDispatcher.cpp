#define LOG_TAG "ExynosCameraTuniingDispatcher"

#include "ExynosCameraTuningDispatcher.h"
#include "ExynosCameraTuningCommand.h"
#include "modules/ExynosCameraTuningModule.h"

namespace android {

ExynosCameraTuningDispatcher::ExynosCameraTuningDispatcher()
{
    //create();
    m_socketCmdFD = -1;
    m_socketJsonFD = -1;
    m_socketImageFD = -1;
    m_tuningFD = -1;
}

ExynosCameraTuningDispatcher::~ExynosCameraTuningDispatcher()
{
    destroy();
}

status_t ExynosCameraTuningDispatcher::create()
{
    char moduleName[30];

    memcpy(moduleName, "ModuleController", 30);
    m_modules[T_MODULE_CONTROLLER] = new ExynosCameraTuningController();
    m_modules[T_MODULE_CONTROLLER]->create(moduleName);

    memcpy(moduleName, "ModuleImageMgr", 30);
    m_modules[T_MODULE_IMAGE_MANAGER] = new ExynosCameraTuningImageManager();
    m_modules[T_MODULE_IMAGE_MANAGER]->create(moduleName);

    memcpy(moduleName, "ModuleJson", 30);
    m_modules[T_MODULE_JSON] = new ExynosCameraTuningJson();
    m_modules[T_MODULE_JSON]->create(moduleName);

    m_tuningFD = open("/dev/is_tuning_json", O_RDWR);
    if (m_tuningFD < 0) {
        CLOGE2("Failed to open /dev/is_tuning_json");
    }

    CLOGD2("JSON tuning dev opened(fd:%d)", m_tuningFD);

    for(int i = 0; i < T_MODULE_MAX; i++) {
        if (m_modules[i] != NULL) {
            m_modules[i]->setTuningFd(m_tuningFD);
        }
    }

    return NO_ERROR;
}

status_t ExynosCameraTuningDispatcher::destroy()
{
    int i = 0;

    for( i = 0; i < T_MODULE_MAX; i++) {
        if (m_modules[i] != NULL) {
            m_modules[i]->destroy();
            delete m_modules[i];
        }

        m_modules[i] = NULL;
    }

    if (m_tuningFD >= 0) {
        close(m_tuningFD);
    }

    return NO_ERROR;
}

status_t ExynosCameraTuningDispatcher::dispatch(ExynosCameraTuningCmdSP_sptr_t command)
{
    int group = command->getCommandGroup();

    CLOGD2("group(%d)", group);

    switch (group) {
    case ExynosCameraTuningCommand::SET_CONTROL:
    case ExynosCameraTuningCommand::GET_CONTROL:
    case ExynosCameraTuningCommand::READ_DATA:
    case ExynosCameraTuningCommand::UPDATE_EXIF:
        m_modules[T_MODULE_CONTROLLER]->sendCommand(command);
        break;
    case ExynosCameraTuningCommand::WRITE_IMAGE:
    case ExynosCameraTuningCommand::READ_IMAGE:
        m_modules[T_MODULE_IMAGE_MANAGER]->sendCommand(command);
        break;
    case ExynosCameraTuningCommand::WRITE_JSON:
    case ExynosCameraTuningCommand::READ_JSON:
        m_modules[T_MODULE_JSON]->sendCommand(command);
        break;
    default:
        CLOGE2("Unknown command group (%d)", group);
        break;
    }

    return NO_ERROR;
}

status_t ExynosCameraTuningDispatcher::setSocketFd(int socketFD, TUNE_SOCKET_ID socketId)
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

    for(int i = 0; i < T_MODULE_MAX; i++) {
        if (m_modules[i] != NULL) {
            m_modules[i]->setSocketFd(socketFD, socketId);
        }
    }

    return NO_ERROR;
}

}; //namespace android
