#ifndef EXYNOS_CAMERA_TUNING_COMMAND_PARSER__
#define EXYNOS_CAMERA_TUNING_COMMAND_PARSER__

#include "ExynosCameraTuningTypes.h"
#include "ExynosCameraThread.h"

#include "ExynosCameraTuningCommand.h"
#include "ExynosCameraTuningDispatcher.h"

namespace android {

class ExynosCameraTuningCommand;

class ExynosCameraTuningCommandParser
{
private:
    typedef struct {
        uint8_t bRequestType;
        uint8_t bRequest;
        uint16_t value;
        uint16_t index;
        uint16_t length;
    }SIMMIAN_CTRL_S;

public:
    ExynosCameraTuningCommandParser();
    ~ExynosCameraTuningCommandParser();

public:
    status_t create(ExynosCameraTuningDispatcher* dispatcher);
    status_t destroy();
    int     getSocketFd(TUNE_SOCKET_ID socketId);

private:
    status_t m_readCommand(uint8_t* request, uint32_t readLength);
    bool m_socketOpen(void);
    bool m_parse(void);

    status_t m_OnSetRequest(int vendorRequest, ExynosCameraTuningCmdSP_sptr_t tuneCmd);
    status_t m_OnGetRequest(int vendorRequest, ExynosCameraTuningCmdSP_sptr_t tuneCmd);

    ExynosCameraTuningCmdSP_sptr_t m_generateTuneCommand();

private:
    sp<Thread> m_socketOpenThread;
    sp<Thread> m_mainThread;
    ExynosCameraTuningDispatcher* m_dispatcher;

    int m_socketCmdFD;
    int m_socketJsonFD;
    int m_socketImageFD;
    bool m_togleTest;
    int m_skipCount;
    bool m_socketOpened;
    bool m_socketCmdOpened;
    bool m_socketJsonOpened;
    bool m_socketImageOpened;
};

}; //namespace android

#endif //EXYNOS_CAMERA_TUNING_COMMAND_PARSER__
