#ifndef EXYNOS_CAMERA_TUNING_H
#define EXYNOS_CAMERA_TUNING_H

#include "ExynosCameraTuningTypes.h"

//#include "ExynosCameraThreadFactory.h"
#include "ExynosCameraList.h"

#include "../ExynosCameraTuningCommand.h"

namespace android {
using namespace std;

typedef ExynosCameraList<ExynosCameraTuningCmdSP_sptr_t> command_queue_t;

class ExynosCameraTuneBufferManager;

class ExynosCameraTuningModule
{
public:
    ExynosCameraTuningModule();
    virtual ~ExynosCameraTuningModule();

public:
    status_t create(char* moduleName);
    status_t destroy();

    status_t sendCommand(ExynosCameraTuningCmdSP_dptr_t cmd);
    status_t setSocketFd(int socketFD, TUNE_SOCKET_ID socketId);
    status_t setTuningFd(int tuningFD);

protected:
    bool m_commandHandler(void);
    virtual status_t m_OnCommandHandler(ExynosCameraTuningCmdSP_sptr_t cmd) = 0;

    int m_tuningFD;
    int m_socketCmdFD;
    int m_socketJsonFD;
    int m_socketImageFD;

private:
    sp<Thread> m_thread;
    command_queue_t* m_cmdQ;
    bool m_flagDestroy;
};


class ExynosCameraTuningController : public ExynosCameraTuningModule
{
    typedef struct {
        struct camera2_ae_udm   ae;
        struct camera2_awb_udm  awb;
        struct camera2_af_udm   af;
        struct camera2_ipc_udm  ipc;
    } UDMforExif;

    typedef struct {
        uint32_t    usize;
        UDMforExif  udmForExif;
    } DMforExif;

public:
    ExynosCameraTuningController();
    virtual ~ExynosCameraTuningController();

    static status_t setISPControl(ExynosCameraTuningCommand::t_data* data);
    static status_t setAFControl(ExynosCameraTuningCommand::t_data* data);
    static status_t setVRAControl(ExynosCameraTuningCommand::t_data* data);

protected:
    virtual status_t m_OnCommandHandler(ExynosCameraTuningCmdSP_sptr_t cmd);

private:
    status_t m_SystemControl(ExynosCameraTuningCommand::t_data* data);
    status_t m_ModeChangeControl(ExynosCameraTuningCommand::t_data* data);
    status_t m_AFControl(ExynosCameraTuningCommand::t_data* data);
    status_t m_MCSCControl(ExynosCameraTuningCommand::t_data* data);
    status_t m_VRAControl(ExynosCameraTuningCommand::t_data* data);
    status_t m_getControl(ExynosCameraTuningCommand::t_data* data);
    uint32_t m_getPhyAddr(uint32_t commandID);
    status_t m_setParamToBuffer(uint32_t index, uint32_t data);
    uint32_t m_getParamFromBuffer(uint32_t index);
    status_t m_updateShareBase(void);
    status_t m_updateDebugBase(void);
    status_t m_updateAFLog(void);
    status_t m_updateDdkVersion(char* version);
    status_t m_updateExif(void);

private:
    unsigned long m_cmdBufferAddr;
    unsigned long m_fwBufferAddr;
    unsigned long m_afLogBufferAddr;
    unsigned long m_exifBufferAddr;
    uint32_t *m_shareBase;
    uint32_t *m_debugBase;
    uint32_t m_maxParamIndex;
};


class ExynosCameraTuningImageManager : public ExynosCameraTuningModule
{
enum T_IMAGE_STATE {
    T_IMAGE_STATE_WAIT = 0,
    T_IMAGE_STATE_READY,
    T_IMAGE_STATE_READ_SIZE,
    T_IMAGE_STATE_READ_DONE = T_IMAGE_STATE_WAIT,
    T_IMAGE_STATE_END,
};

public:
    ExynosCameraTuningImageManager();
    virtual ~ExynosCameraTuningImageManager();

protected:
    virtual status_t m_OnCommandHandler(ExynosCameraTuningCmdSP_sptr_t cmd);

private:
    enum T_IMAGE_STATE m_imageState;

};


class ExynosCameraTuningJson : public ExynosCameraTuningModule
{
enum T_JSON_STATE {
    T_JSON_STATE_WAIT = 0,
    T_JSON_STATE_READY,
    T_JSON_STATE_WRITING,
    T_JSON_STATE_WRITE_DONE = T_JSON_STATE_WAIT,
    T_JSON_STATE_READ_SIZE,
    T_JSON_STATE_READ_SIZE_DONE,
    T_JSON_STATE_READING,
    T_JSON_STATE_READ_DONE = T_JSON_STATE_WAIT,
    T_JSON_STATE_END,
};

public:
    ExynosCameraTuningJson();
    virtual ~ExynosCameraTuningJson();

protected:
    virtual status_t m_OnCommandHandler(ExynosCameraTuningCmdSP_sptr_t cmd);

private:
    status_t m_allocBuffer();
    status_t m_deallocBuffer();

    status_t m_writer(ExynosCameraTuningCommand::t_data* data);
    status_t m_reader(ExynosCameraTuningCommand::t_data* data);
    status_t m_readDone(ExynosCameraTuningCommand::t_data* data);

private:
    ExynosCameraBufferManager*       m_jsonReadBufMgr;
    ExynosCameraBufferManager*       m_jsonWriteBufMgr;

    ExynosCameraBuffer               m_jsonReadBuffer;
    ExynosCameraBuffer               m_jsonWriteBuffer;

    ExynosCameraTuneBufferManager*    m_tuneBufferManager;

    enum T_JSON_STATE m_jsonState;
    uint32_t m_jsonReadyCheckCount;
    char* m_jsonRead_mallocBuffer;
    uint32_t m_jsonReadSize;
    uint32_t m_jsonReadRemainingSize;
    char* m_jsonReadAddr;
    uint32_t m_jsonWriteSize;
    uint32_t m_jsonWriteRemainingSize;
    char* m_jsonWriteAddr;
};

}; //namespace android
#endif //#ifndef EXYNOS_CAMERA_TUNING_H
