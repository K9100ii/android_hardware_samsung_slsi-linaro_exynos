#ifndef EXYNOS_CAMERA_TUNING_DISPATCHER_H__
#define EXYNOS_CAMERA_TUNING_DISPATCHER_H__

#include "ExynosCameraTuningTypes.h"
#include "ExynosCameraTuningDefines.h"

namespace android {

class ExynosCameraTuningCommand;
class ExynosCameraTuningModule;

class ExynosCameraTuningDispatcher
{
private:
    typedef enum {
        T_MODULE_CONTROLLER = 0,
        T_MODULE_IMAGE_MANAGER = 1,
        T_MODULE_JSON = 2,
        T_MODULE_MAX = 3
    }T_MODULES;

public:
    ExynosCameraTuningDispatcher();
    ~ExynosCameraTuningDispatcher();

    status_t create();
    status_t destroy();
    status_t dispatch(ExynosCameraTuningCmdSP_sptr_t command);
    status_t setSocketFd(int socketFD, TUNE_SOCKET_ID socketId);

private:
    ExynosCameraTuningModule* m_modules[T_MODULE_MAX];
    int m_tuningFD;
    int m_socketCmdFD;
    int m_socketJsonFD;
    int m_socketImageFD;
};

}; //namespace android

#endif //EXYNOS_CAMERA_TUNING_DISPATCHER_H__
