/*
**
** Copyright 2017, Samsung Electronics Co. LTD
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

#ifndef EXYNOS_CAMERA_FRAME_FACTORY_BASE_H
#define EXYNOS_CAMERA_FRAME_FACTORY_BASE_H

#include "ExynosCameraFrame.h"
#include "ExynosCameraObject.h"
#include "ExynosCameraPipe.h"
#include "ExynosCameraMCPipe.h"
#include "ExynosCameraPipePP.h"
#ifdef USE_DUAL_CAMERA
#include "ExynosCameraPipeSync.h"
#include "ExynosCameraPipeFusion.h"
#endif

#include "ExynosCameraFrameManager.h"
#include "ExynosCameraParameters.h"
#include "ExynosCameraStreamMutex.h"

namespace android {

#define SET_OUTPUT_DEVICE_BASIC_INFO(perframeInfo) \
    pipeInfo[nodeType].rectInfo = tempRect;\
    pipeInfo[nodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;\
    pipeInfo[nodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;\
    pipeInfo[nodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = CAPTURE_NODE_MAX;\
    pipeInfo[nodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perframeInfoIndex = perframeInfo;\
    pipeInfo[nodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_LEADER;\
    pipeInfo[nodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameVideoID = (m_deviceInfo[INDEX(pipeId)].nodeNum[nodeType] - FIMC_IS_VIDEO_BAS_NUM);
#define SET_CAPTURE_DEVICE_BASIC_INFO() \
    pipeInfo[nodeType].rectInfo = tempRect;\
    pipeInfo[nodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;\
    pipeInfo[nodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;\
    pipeInfo[leaderNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameNodeType = PERFRAME_NODE_TYPE_CAPTURE;\
    pipeInfo[leaderNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameVideoID = (m_deviceInfo[INDEX(pipeId)].nodeNum[nodeType] - FIMC_IS_VIDEO_BAS_NUM);
#define SET_OUTPUT_DEVICE_CAPTURE_NODE_INFO() \
    pipeInfo[nodeType].rectInfo = tempRect;\
    pipeInfo[nodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;\
    pipeInfo[nodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;\
    pipeInfo[leaderNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameNodeType = PERFRAME_NODE_TYPE_CAPTURE;\
    pipeInfo[leaderNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameVideoID = (m_deviceInfo[INDEX(pipeId)].nodeNum[nodeType] - FIMC_IS_VIDEO_BAS_NUM);

typedef enum frame_factory_state {
    FRAME_FACTORY_STATE_NONE    = 0,
    FRAME_FACTORY_STATE_CREATE  = 1,
    FRAME_FACTORY_STATE_INIT    = 2,
    FRAME_FACTORY_STATE_RUN     = 3,
    FRAME_FACTORY_STATE_MAX     = 4,
} frame_factory_state_t;

enum FRAME_FACTORY_TYPE {
    FRAME_FACTORY_TYPE_REPROCESSING = 0,
    FRAME_FACTORY_TYPE_CAPTURE_PREVIEW,
#ifdef USE_DUAL_CAMERA
    FRAME_FACTORY_TYPE_PREVIEW_DUAL,
    FRAME_FACTORY_TYPE_REPROCESSING_DUAL,
#endif
    FRAME_FACTORY_TYPE_VISION,
    FRAME_FACTORY_TYPE_MAX,
};

class ExynosCamera;
class ExynosCameraRequest;
class ExynosCameraFrameFactory;
typedef sp<ExynosCameraRequest> ExynosCameraRequestSP_sprt_t;
typedef sp<ExynosCameraRequest>& ExynosCameraRequestSP_dptr_t;
typedef status_t (ExynosCamera::*factory_handler_t)(ExynosCameraRequestSP_sprt_t, ExynosCameraFrameFactory*, frame_type_t);

class ExynosCameraFrameFactoryBase : public ExynosCameraObject {
public:
    ExynosCameraFrameFactoryBase()
    {
        m_init();
    }

    ExynosCameraFrameFactoryBase(int cameraId, ExynosCameraParameters *param)
    {
        m_init();

        m_cameraId = cameraId;
        m_parameters = param;
        m_activityControl = m_parameters->getActivityControl();
    }

public:
    virtual ~ExynosCameraFrameFactoryBase();

    virtual status_t        create(void);
    virtual status_t        precreate(void);
    virtual status_t        postcreate(void);
    virtual status_t        destroy(void);

    virtual status_t        initPipes(void) = 0;
    virtual status_t        mapBuffers(void);
    virtual status_t        preparePipes(void) = 0;
    virtual status_t        startPipes(void) = 0;
    virtual status_t        stopPipes(void) = 0;
    virtual status_t        startInitialThreads(void) = 0;

    virtual status_t        fastenAeStable(int32_t numFrames, ExynosCameraBuffer *buffers);
    virtual status_t        sensorStandby(bool flagStandby);
    virtual bool            isSensorStandby(void);

    virtual status_t        setStopFlag(void);
    virtual status_t        stopPipe(uint32_t pipeId);
    virtual status_t        startThread(uint32_t pipeId);
    virtual status_t        stopThread(uint32_t pipeId);
    virtual status_t        stopThreadAndWait(uint32_t pipeId, int sleep = 5, int times = 40);
    virtual bool            checkPipeThreadRunning(uint32_t pipeId);
    virtual void            setThreadOneShotMode(uint32_t pipeId, bool enable);

    virtual status_t        setFrameManager(ExynosCameraFrameManager *manager);
    virtual status_t        getFrameManager(ExynosCameraFrameManager **manager);

    virtual status_t        setBufferSupplierToPipe(ExynosCameraBufferSupplier *bufferSupplier, uint32_t pipeId);

    virtual status_t        setOutputFrameQToPipe(frame_queue_t *outputQ, uint32_t pipeId);
    virtual status_t        getOutputFrameQToPipe(frame_queue_t **outputQ, uint32_t pipeId);
    virtual status_t        setFrameDoneQToPipe(frame_queue_t *frameDoneQ, uint32_t pipeId);
    virtual status_t        getFrameDoneQToPipe(frame_queue_t **frameDoneQ, uint32_t pipeId);
    virtual status_t        getInputFrameQToPipe(frame_queue_t **inputFrameQ, uint32_t pipeId);

    virtual status_t        pushFrameToPipe(ExynosCameraFrameSP_dptr_t newFrame, uint32_t pipeId);

    virtual status_t        setParam(struct v4l2_streamparm *streamParam, uint32_t pipeId);
    virtual status_t        setControl(int cid, int value, uint32_t pipeId);
    virtual status_t        setControl(int cid, int value, uint32_t pipeId, enum NODE_TYPE nodeType);
    virtual status_t        getControl(int cid, int *value, uint32_t pipeId);

    virtual void            setRequest(int pipeId, bool enable);
    virtual bool            getRequest(int pipeId);

    virtual status_t        getThreadState(int **threadState, uint32_t pipeId);
    virtual status_t        getThreadInterval(uint64_t **threadInterval, uint32_t pipeId);
    virtual status_t        getThreadRenew(int **threadRenew, uint32_t pipeId);
    virtual status_t        incThreadRenew(uint32_t pipeId);

    virtual int             getRunningFrameCount(uint32_t pipeId);

    virtual enum NODE_TYPE  getNodeType(uint32_t pipeId) = 0;

    virtual ExynosCameraFrameSP_sptr_t createNewFrame(uint32_t frameCount, bool useJpegFlag = false) = 0;

    virtual void            dump(void);
    /* only for debugging */
    virtual status_t        dumpFimcIsInfo(uint32_t pipeId, bool bugOn);
#ifdef MONITOR_LOG_SYNC
    virtual status_t        syncLog(uint32_t pipeId, uint32_t syncId);
#endif
    status_t                setFrameCreateHandler(factory_handler_t handler);
    factory_handler_t       getFrameCreateHandler();

    virtual bool            isCreated(void);
    virtual bool            isRunning(void);

    void                    setFactoryType(enum FRAME_FACTORY_TYPE factoryType);
    enum FRAME_FACTORY_TYPE getFactoryType(void);

protected:
    virtual status_t        m_setupConfig(void) = 0;
    virtual status_t        m_constructPipes(void) = 0;

    virtual status_t        m_initPipelines(ExynosCameraFrameSP_sptr_t frame);
    virtual status_t        m_checkPipeInfo(uint32_t srcPipeId, uint32_t dstPipeId);

    virtual void            m_initDeviceInfo(int pipeId);

    virtual status_t        m_setSensorSize(int pipeId, int sensorW, int sensorH);

    virtual status_t        m_transitState(frame_factory_state_t state);

private:
    void                    m_init(void);

protected:
    uint32_t                    m_frameCount;

    int32_t                     m_nodeNums[MAX_NUM_PIPES][MAX_NODE];
    int32_t                     m_sensorIds[MAX_NUM_PIPES][MAX_NODE];
    camera_device_info_t        m_deviceInfo[MAX_NUM_PIPES];

    ExynosCameraPipe           *m_pipes[MAX_NUM_PIPES];
    ExynosCameraParameters     *m_parameters;
    ExynosCameraFrameManager   *m_frameMgr;
    ExynosCameraActivityControl *m_activityControl;

    bool                        m_request[MAX_NUM_PIPES];

    bool                        m_bypassDRC;
    bool                        m_bypassDIS;
    bool                        m_bypassDNR;
    bool                        m_bypassFD;

    enum HW_CONNECTION_MODE     m_flagFlite3aaOTF;
    enum HW_CONNECTION_MODE     m_flag3aaIspOTF;
    enum HW_CONNECTION_MODE     m_flagIspMcscOTF;
    enum HW_CONNECTION_MODE     m_flagMcscVraOTF;
#ifdef USE_DUAL_CAMERA
    enum HW_CONNECTION_MODE     m_flagIspDcpOTF;
    enum HW_CONNECTION_MODE     m_flagDcpMcscOTF;

#endif

    Mutex                       m_sensorStandbyLock;
    bool                        m_sensorStandby;
    bool                        m_supportReprocessing;
    bool                        m_flagReprocessing;
    bool                        m_supportPureBayerReprocessing;

    factory_handler_t           m_frameCreateHandler;

    frame_factory_state_t       m_state;
    Mutex                       m_stateLock;

    enum FRAME_FACTORY_TYPE    m_factoryType;
};

}; /* namespace android */

#endif
