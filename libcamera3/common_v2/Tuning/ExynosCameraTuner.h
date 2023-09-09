#ifndef EXYNOS_CAMERA_TUNER_H
#define EXYNOS_CAMERA_TUNER_H

#include <string.h>
#include "utils/Mutex.h"

#include "fimc-is-metadata.h"
#include "ExynosCameraTuningTypes.h"
#include "ExynosCameraTuningCommandParser.h"

#include "ExynosCameraBufferManager.h"
#include "ExynosCameraParameters.h"
#include "ExynosCameraConfigurations.h"

namespace android {

typedef ExynosCameraList<ExynosCameraTuningCmdSP_sptr_t> command_queue_t;

class ExynosCameraTuneBufferManager;
class EXYNOSCAMERA_FRAME_FACTORY;

typedef enum {
    T_BAYER_IN_BUFFER_MGR = 0,
    T_BAYER_OUT_BUFFER_MGR,
    T_3AA0_BUFFER_MGR,
    T_3AA1_BUFFER_MGR,
    T_ISP_BUFFER_MGR,
    T_MCSC0_BUFFER_MGR,
    T_MCSC1_BUFFER_MGR,
    T_MCSC2_BUFFER_MGR,
    T_JSON_READ_BUFFER_MGR,
    T_JSON_WRITE_BUFFER_MGR,
    T_BUFFER_MGR_MAX,
} T_BUFFER_MGR_ID;

typedef enum {
    T_STATE_SHOT_NONE = 0,
    T_STATE_SHOT_INIT,
    T_STATE_SHOT_TUNED,
    T_STATE_SHOT_APPLIED,
} T_STATE_SHOT;

class ExynosCameraTuner {

public:

private:
    ExynosCameraTuner();
    ~ExynosCameraTuner();

public:
    static ExynosCameraTuner* getInstance();
    static void deleteInstance();

    status_t start();
    status_t start(EXYNOSCAMERA_FRAME_FACTORY* previewFrameFactory, EXYNOSCAMERA_FRAME_FACTORY* captureFrameFactory);
    status_t stop();

    status_t updateShot(struct camera2_shot_ext* shot, int target, int from);
    status_t forceSetShot(struct camera2_shot_ext* shot);
    status_t getShot(struct camera2_shot_ext* shot);
    status_t forceGetShot(struct camera2_shot_ext* shot);
    T_STATE_SHOT getShotState(void);

    status_t updateImage(ExynosCameraFrameSP_sptr_t frame, int pipeId, EXYNOSCAMERA_FRAME_FACTORY* factory);
    status_t getImage(void);
    status_t setImage(void);

    status_t setControl(int value);
    status_t setRequest(int pipeId, bool enable);
    bool     getRequest(int pipeId);

    status_t setConfigurations(ExynosCameraConfigurations *configurations);
    ExynosCameraConfigurations* getConfigurations(void);
    status_t setParameters(ExynosCameraParameters *parameters);
    ExynosCameraParameters* getParameters(void);

    status_t sendCommand(ExynosCameraTuningCmdSP_dptr_t cmd);
    ExynosCameraTuningCmdSP_sptr_t getCommand(void);

    status_t storeToMap(ExynosCameraTuningCmdSP_dptr_t cmd);
    status_t eraseToMap(ExynosCameraTuningCmdSP_dptr_t cmd);

    status_t resetImage(void);
    bool isImageReady(void);

    unsigned long getBufferAddr(T_BUFFER_MGR_ID bufferMgr);
private:
    status_t m_setBuffer();
    status_t m_releaseBuffer();

public:
    mutable Mutex   readSocketLock;

private:
    bool                             m_request[MAX_NUM_PIPES];
    ExynosCameraTuningCommandParser  m_parser;
    ExynosCameraTuningDispatcher*     m_dispatcher;
    ExynosCameraTuneBufferManager*    m_tuneBufferManager;
    ExynosCameraParameters           *m_parameters;
    ExynosCameraConfigurations       *m_configurations;

    command_queue_t                  *m_cmdQ;

    map<uint32_t, ExynosCameraTuningCmdSP_sptr_t> m_cmdMap;
    struct camera2_shot_ext          m_shot;

    mutable Mutex                    m_updateShotLock;
    mutable Mutex                    m_updateImageLock[MAX_NUM_PIPES];

    T_STATE_SHOT                     m_shotState;

    bool                             m_bImageReady;
    bool                             m_bImageReadyChecked;

    ExynosCameraBufferManager*       m_bufferMgr[T_BUFFER_MGR_MAX];

    bool                             m_bStarted;

    bool                             m_flagResetImage;

    EXYNOSCAMERA_FRAME_FACTORY*      m_frameFactory[2];

    int                              m_jsonRuningFlag;

    static ExynosCameraTuner*        m_self;
    uint32_t dumpCount;
};

class ExynosCameraTuneBufferManager {
public:
    struct managerRegister {
        T_BUFFER_MGR_ID id;
        char name[256];
        ExynosCameraBufferManager* pManager;
    };

public:
    ExynosCameraTuneBufferManager();
    ~ExynosCameraTuneBufferManager();

    ExynosCameraBufferManager* getBufferManager(T_BUFFER_MGR_ID bufferMgr);

    status_t allocBuffers(
        ExynosCameraBufferManager *bufManager,
        int  planeCount,
        unsigned int *planeSize,
        unsigned int *bytePerLine,
        int  minBufCount,
        int  maxBufCount,
        int batchSize,
        exynos_camera_buffer_type_t type,
        bool createMetaPlane,
        bool needMmap);

private:
    status_t m_createBufferManager(T_BUFFER_MGR_ID bufferMgr);

private:
    ExynosCameraIonAllocator* m_allocator;
    struct managerRegister m_managerList[T_BUFFER_MGR_MAX];
};

};

#endif //EXYNOS_CAMERA_TUNER_H


