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

#ifndef EXYNOS_CAMERA_PIPE_H
#define EXYNOS_CAMERA_PIPE_H

#include <array>
#include <log/log.h>

#include "ExynosCameraCommonInclude.h"

#include "ExynosCameraThread.h"

#include "ExynosCameraNode.h"
#include "ExynosCameraNodeJpegHAL.h"
#ifdef USE_CIP_HW
#include "ExynosCameraNodeCIP.h"
#endif
#include "ExynosCameraFrame.h"
#include "ExynosCameraSensorInfo.h"
#include "ExynosCameraParameters.h"
#include "ExynosCameraList.h"
#include "ExynosCameraBufferSupplier.h"

#include "ExynosCameraSizeControl.h"
#include "ExynosCameraTimeLogger.h"

#include "ExynosJpegApi.h"
#ifdef USE_SLSI_PLUGIN
#include "PlugInCommon.h"
#endif
namespace android {

using namespace std;
typedef sp<ExynosCameraFrame>  ExynosCameraFrameSP_t;
typedef sp<ExynosCameraFrame>  ExynosCameraFrameSP_sptr_t; /* single ptr */
typedef sp<ExynosCameraFrame>& ExynosCameraFrameSP_dptr_t; /* double ptr */
typedef wp<ExynosCameraFrame> ExynosCameraFrameWP_t;
typedef wp<ExynosCameraFrame>& ExynosCameraFrameWP_dptr_t; /* wp without inc refcount */
typedef ExynosCameraList<ExynosCameraFrameSP_sptr_t> frame_queue_t;
typedef List<ExynosCameraFrameSP_sptr_t> raw_frame_queue_t;

/* for reprocessing pipe, if timeout happend, prohibit loging until this define */
#define TIME_LOG_COUNT 5

enum PIPE_POSITION {
    SRC_PIPE            = 0,
    DST_PIPE
};

enum SENSOR_STANDBY_STATE {
    SENSOR_STANDBY_OFF,
    SENSOR_STANDBY_ON_READY,
    SENSOR_STANDBY_ON,
};

typedef enum perframe_node_type {
    PERFRAME_NODE_TYPE_NONE        = 0,
    PERFRAME_NODE_TYPE_LEADER      = 1,
    PERFRAME_NODE_TYPE_CAPTURE     = 2,
} perframe_node_type_t;

typedef struct ExynosCameraNodeObjects {
    ExynosCameraNode    *node[MAX_NODE];
    ExynosCameraNode    *secondaryNode[MAX_NODE];
    bool isInitalize;
} camera_node_objects_t;

typedef struct ExynosCameraPerframeNodeInfo {
    perframe_node_type_t perFrameNodeType;
    int perframeInfoIndex;
    int perFrameVideoID;

    ExynosCameraPerframeNodeInfo()
    {
        perFrameNodeType  = PERFRAME_NODE_TYPE_NONE;
        perframeInfoIndex = 0;
        perFrameVideoID   = 0;
    }

    ExynosCameraPerframeNodeInfo& operator =(const ExynosCameraPerframeNodeInfo &other)
    {
        perFrameNodeType  = other.perFrameNodeType;
        perframeInfoIndex = other.perframeInfoIndex;
        perFrameVideoID   = other.perFrameVideoID;

        return *this;
    }
} camera_pipe_perframe_node_info_t;

typedef struct ExynosCameraPerframeNodeGroupInfo {
    int perframeSupportNodeNum;
    camera_pipe_perframe_node_info_t perFrameLeaderInfo;
    camera_pipe_perframe_node_info_t perFrameCaptureInfo[CAPTURE_NODE_MAX];

    ExynosCameraPerframeNodeGroupInfo()
    {
        perframeSupportNodeNum = 0;
    }

    ExynosCameraPerframeNodeGroupInfo& operator =(const ExynosCameraPerframeNodeGroupInfo &other)
    {
        perframeSupportNodeNum  = other.perframeSupportNodeNum;
        perFrameLeaderInfo      = other.perFrameLeaderInfo;

        for (int i = 0; i < CAPTURE_NODE_MAX; i++)
            perFrameCaptureInfo[i] = other.perFrameCaptureInfo[i];

        return *this;
    }
} camera_pipe_perframe_node_group_info_t;

typedef struct ExynosCameraPipeInfo {
    struct ExynosRect rectInfo;
    struct v4l2_requestbuffers bufInfo;
    camera_pipe_perframe_node_group_info_t perFrameNodeGroupInfo;
    unsigned int bytesPerPlane[EXYNOS_CAMERA_BUFFER_MAX_PLANES];

    /* The yuvPixelSize is used to distinguish pixel size of YUV format */
    camera_pixel_size pixelSize;

    ExynosCameraPipeInfo()
    {
        memset(&bufInfo, 0, sizeof(v4l2_requestbuffers));

        for (int i = 0; i < EXYNOS_CAMERA_BUFFER_MAX_PLANES; i++)
            bytesPerPlane[i] = 0;

        pixelSize = CAMERA_PIXEL_SIZE_8BIT;
    }

    ExynosCameraPipeInfo& operator =(const ExynosCameraPipeInfo &other)
    {
        rectInfo = other.rectInfo;
        memcpy(&bufInfo, &(other.bufInfo), sizeof(v4l2_requestbuffers));
        perFrameNodeGroupInfo = other.perFrameNodeGroupInfo;

        for (int i = 0; i < EXYNOS_CAMERA_BUFFER_MAX_PLANES; i++)
            bytesPerPlane[i] = other.bytesPerPlane[i];

        pixelSize = other.pixelSize;

        return *this;
    }
} camera_pipe_info_t;

typedef struct ExynosCameraDeviceInfo {
    int32_t nodeNum[MAX_NODE];
    int32_t secondaryNodeNum[MAX_NODE];
    char    nodeName[MAX_NODE][EXYNOS_CAMERA_NAME_STR_SIZE];
    char    secondaryNodeName[MAX_NODE][EXYNOS_CAMERA_NAME_STR_SIZE];

    int pipeId[MAX_NODE]; /* enum pipeline */
    unsigned int connectionMode[MAX_NODE];
    buffer_manager_type_t bufferManagerType[MAX_NODE];

    ExynosCameraDeviceInfo()
    {
        for (int i = 0; i < MAX_NODE; i++) {
            nodeNum[i] = -1;
            secondaryNodeNum[i] = -1;

            memset(nodeName[i], 0, EXYNOS_CAMERA_NAME_STR_SIZE);
            memset(secondaryNodeName[i], 0, EXYNOS_CAMERA_NAME_STR_SIZE);

            pipeId[i] = -1;
            connectionMode[i] = 0;
            bufferManagerType[i] = BUFFER_MANAGER_INVALID_TYPE;
        }
    }

    ExynosCameraDeviceInfo& operator =(const ExynosCameraDeviceInfo &other)
    {
        for (int i = 0; i < MAX_NODE; i++) {
            nodeNum[i] = other.nodeNum[i];
            secondaryNodeNum[i] = other.secondaryNodeNum[i];

            strncpy(nodeName[i],          other.nodeName[i],          EXYNOS_CAMERA_NAME_STR_SIZE - 1);
            strncpy(secondaryNodeName[i], other.secondaryNodeName[i], EXYNOS_CAMERA_NAME_STR_SIZE - 1);

            pipeId[i] = other.pipeId[i];
            connectionMode[i] = other.connectionMode[i];
            bufferManagerType[i] = other.bufferManagerType[i];
        }

        return *this;
    }
} camera_device_info_t;


namespace BUFFER_POS {
    enum POS {
        SRC = 0x00,
        DST = 0x01
    };
}

namespace BUFFERQ_TYPE {
    enum TYPE {
        INPUT = 0x00,
        OUTPUT = 0x01
    };
};

class ExynosCameraPipe : public ExynosCameraObject {
public:
    ExynosCameraPipe()
    {
        m_init();
    }

    ExynosCameraPipe(
        int cameraId,
        ExynosCameraConfigurations *configurations,
        ExynosCameraParameters *obj_param,
        bool isReprocessing,
        int32_t *nodeNums)
    {
        m_init();

        setCameraId(cameraId);
        m_reprocessing = isReprocessing ? 1 : 0;
        m_oneShotMode = isReprocessing;

        if (nodeNums) {
            for (int i = 0; i < MAX_NODE; i++)
                m_nodeNum[i] = nodeNums[i];
        }

        if (obj_param) {
            m_parameters = obj_param;
            m_activityControl = m_parameters->getActivityControl();
        }

        if (configurations) {
            m_configurations = configurations;
            m_exynosconfig = m_configurations->getConfig();
        }

        m_bufferSupplier = NULL;
    }

    virtual ~ExynosCameraPipe();

    virtual status_t        create(int32_t *sensorIds = NULL);
    virtual status_t        precreate(int32_t *sensorIds = NULL);
    virtual status_t        postcreate(int32_t *sensorIds = NULL);
    virtual status_t        destroy(void);

    virtual status_t        setupPipe(camera_pipe_info_t *pipeInfos, int32_t *sensorIds = NULL);
    virtual status_t        setupPipe(camera_pipe_info_t *pipeInfos, int32_t *sensorIds, int32_t *ispSensorIds);
#ifdef USE_SLSI_PLUGIN
    virtual status_t        setupPipe(__unused Map_t *map) {
        android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):Not supported API. use PlugInPipe, assert!!!!", __FUNCTION__, __LINE__);
        return NO_ERROR;
    };
#endif
    virtual status_t        setParameter(__unused int key, __unused void *data) {
        android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):Not supported API. use PlugInPipe, assert!!!!", __FUNCTION__, __LINE__);
        return NO_ERROR;
    };
    virtual status_t        getParameter(__unused int key, __unused void *data) {
        android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):Not supported API. use PlugInPipe, assert!!!!", __FUNCTION__, __LINE__);
        return NO_ERROR;
    };

    virtual status_t        prepare(void);
    virtual status_t        prepare(uint32_t prepareCnt);

    virtual status_t        start(void);
    virtual status_t        stop(void);
    virtual bool            flagStart(void);

    virtual status_t        startThread(void);
    virtual status_t        stopThread(void);
    virtual bool            flagStartThread(void);
    virtual status_t        stopThreadAndWait(int sleep, int times);

    virtual status_t        sensorStream(bool on);
    virtual status_t        sensorStandby(bool on);
    virtual enum SENSOR_STANDBY_STATE getSensorStandbyState(void);
    virtual status_t        forceDone(unsigned int cid, int value);
    virtual status_t        setControl(int cid, int value);
    virtual status_t        setControl(int cid, int value, enum NODE_TYPE nodeType);
    virtual status_t        getControl(int cid, int *value);
    virtual status_t        getControl(int cid, int *value, enum NODE_TYPE nodeType);
    virtual status_t        setExtControl(struct v4l2_ext_controls *ctrl);
    virtual status_t        setParam(struct v4l2_streamparm streamParam);

    virtual status_t        pushFrame(ExynosCameraFrameSP_dptr_t newFrame);

    virtual status_t        instantOn(int32_t numFrames);
    virtual status_t        instantOff(void);
    virtual status_t        instantOnPushFrameQ(BUFFERQ_TYPE::TYPE type, ExynosCameraFrameSP_dptr_t frame);

    virtual status_t        getPipeInfo(int *fullW, int *fullH, int *colorFormat, int pipePosition);
    virtual status_t        setPipeId(uint32_t id);
    virtual uint32_t        getPipeId(void);
    virtual status_t        setPipeId(enum NODE_TYPE nodeType, uint32_t id);
    virtual int             getPipeId(enum NODE_TYPE nodeType);
    virtual status_t        setPipeStreamLeader(bool);
    virtual status_t        setPrevPipeID(int);
    virtual bool            getPipeStreamLeader(void);
    virtual int             getPrevPipeID(void);


    virtual status_t        setPipeName(const char *pipeName);
    virtual char           *getPipeName(void);

    virtual status_t        clearInputFrameQ(void);
    virtual status_t        getInputFrameQ(frame_queue_t **inputQ);
    virtual status_t        setOutputFrameQ(frame_queue_t *outputQ);
    virtual status_t        getOutputFrameQ(frame_queue_t **outputQ);

    virtual status_t        setBoosting(bool isBoosting);

    virtual bool            isThreadRunning(void);

    virtual status_t        getThreadState(int **threadState);
    virtual status_t        getThreadInterval(uint64_t **timeInterval);
    virtual status_t        getThreadRenew(int **timeRenew);
    virtual status_t        incThreadRenew(void);
    virtual status_t        setStopFlag(void);

    virtual int             getRunningFrameCount(void);

    virtual void            dump(void);

    /* only for debugging */
    virtual status_t        dumpFimcIsInfo(bool bugOn);
//#ifdef MONITOR_LOG_SYNC
    virtual status_t        syncLog(uint32_t syncId);
//#endif

/* MC Pipe include buffer manager, so FrameFactory(ExynosCamera) must set buffer manager to pipe.
 * Add interface for set buffer manager to pipe.
 */
    virtual status_t        setBufferSupplier(ExynosCameraBufferSupplier *bufferSupplier);
    virtual status_t        setBufferManager(ExynosCameraBufferManager **bufferManager);

/* Set map buffer is makes node operation faster at first start.
 * Thereby map buffer before start, reduce map buffer time at start.
 * It use first Exynos5430/5433 in old pipe.
 */
    virtual status_t        setMapBuffer(ExynosCameraBuffer *srcBuf = NULL, ExynosCameraBuffer *dstBuf = NULL);

/* MC Pipe have two output queue.
 * If you want push frame to FrameDoneQ in ExynosCamera explicitly, use this interface.
 */

    virtual status_t        setFrameDoneQ(frame_queue_t *frameDoneQ);
    virtual status_t        getFrameDoneQ(frame_queue_t **frameDoneQ);

    virtual status_t        setNodeInfos(camera_node_objects_t *nodeObjects, bool flagReset = false);
    virtual status_t        getNodeInfos(camera_node_objects_t *nodeObjects);

    virtual void            setOneShotMode(bool enable);
#ifdef USE_MCPIPE_SERIALIZATION_MODE
    virtual void            needSerialization(bool enable);
#endif

    virtual status_t        setDeviceInfo(__unused camera_device_info_t *deviceInfo) { return NO_ERROR; }
    virtual status_t        setUseLatestFrame(__unused bool flag) { return NO_ERROR; }

protected:
    virtual bool            m_mainThreadFunc(void);

    virtual status_t        m_putBuffer(void);
    virtual status_t        m_getBuffer(void);

    virtual status_t        m_updateMetadataToFrame(void *metadata, int index);
    virtual status_t        m_getFrameByIndex(ExynosCameraFrameSP_dptr_t frame, int index);
    virtual status_t        m_completeFrame(
                                ExynosCameraFrameSP_dptr_t frame,
                                ExynosCameraBuffer buffer,
                                bool isValid = true);

    virtual status_t        m_setInput(ExynosCameraNode *nodes[], int32_t *nodeNums, int32_t *sensorIds);
    virtual status_t        m_setPipeInfo(camera_pipe_info_t *pipeInfos);
    virtual status_t        m_setNodeInfo(ExynosCameraNode *node, camera_pipe_info_t *pipeInfos,
                                          int planeCount, enum YUV_RANGE yuvRange,
                                          bool flagBayer = false);
    virtual status_t        m_forceDone(ExynosCameraNode *node, unsigned int cid, int value);

    virtual status_t        m_startNode(void);
    virtual status_t        m_stopNode(void);
    virtual status_t        m_clearNode(void);

    virtual status_t        m_checkNodeGroupInfo(char *name, camera2_node *oldNode, camera2_node *newNode);
    virtual status_t        m_checkNodeGroupInfo(int index, camera2_node *oldNode, camera2_node *newNode);

    virtual void            m_dumpRunningFrameList(void);

    virtual void            m_dumpPerframeNodeGroupInfo(const char *name, camera_pipe_perframe_node_group_info_t nodeInfo);

    virtual void            m_configDvfs(void);
    virtual bool            m_flagValidInt(int num);
            bool            m_isOtf(int sensorId);
            bool            m_checkValidFrameCount(struct camera2_shot_ext *shot_ext);
            bool            m_checkValidFrameCount(struct camera2_stream *stream);
    virtual status_t        m_handleInvalidFrame(int index, ExynosCameraFrameSP_sptr_t newFrame, ExynosCameraBuffer *buffer);
    virtual bool            m_checkLeaderNode(int sensorId);
    virtual bool            m_isReprocessing(void);
    virtual bool            m_checkThreadLoop(void);

private:
    void                    m_init(void);

protected:
    ExynosCameraConfigurations  *m_configurations;
    ExynosCameraParameters      *m_parameters;
    ExynosCameraBufferSupplier  *m_bufferSupplier;
    ExynosCameraBufferManager   *m_bufferManager[MAX_NODE];
    ExynosCameraActivityControl *m_activityControl;

    sp<Thread>                  m_mainThread;

    struct ExynosConfigInfo     *m_exynosconfig;

    ExynosCameraNode           *m_node[MAX_NODE];
    int32_t                     m_nodeNum[MAX_NODE];
    int32_t                     m_sensorIds[MAX_NODE];
    ExynosCameraNode           *m_mainNode;
    int32_t                     m_mainNodeNum;

    frame_queue_t              *m_inputFrameQ;
    frame_queue_t              *m_outputFrameQ;
    frame_queue_t              *m_frameDoneQ;

    array<ExynosCameraFrameSP_sptr_t, MAX_BUFFERS>                  m_runningFrameList;
    array<array<ExynosCameraFrameSP_sptr_t, MAX_BUFFERS>, MAX_NODE> m_nodeRunningFrameList;
    uint32_t                    m_numOfRunningFrame;

protected:
    uint32_t                    m_pipeId;

    uint32_t                    m_prepareBufferCount;
    uint32_t                    m_numBuffers;
    /* Node for capture Interface : destination port */
    int                         m_numCaptureBuf;

    uint32_t                    m_reprocessing;
    bool                        m_oneShotMode;
    bool                        m_flagStartPipe;
    bool                        m_flagTryStop;
    bool                        m_dvfsLocked;
    bool                        m_isBoosting;
    bool                        m_metadataTypeShot;
    bool                        m_flagFrameDoneQ;

    ExynosCameraDurationTimer   m_timer;
    int                         m_threadCommand;
    uint64_t                    m_timeInterval;
    int                         m_threadState;
    int                         m_threadRenew;

    Mutex                       m_pipeframeLock;

    camera2_node_group          m_curNodeGroupInfo[PERFRAME_INFO_INDEX_MAX];

    camera_pipe_perframe_node_group_info_t m_perframeMainNodeGroupInfo;

    int                         m_lastSrcFrameCount;
    int                         m_lastDstFrameCount;

    int                         m_timeLogCount;
    int                         m_prevPipeId;
    bool                        m_flagStreamLeader;


protected:
    typedef enum pipe_state {
        PIPE_STATE_NONE     = 0,
        PIPE_STATE_CREATE   = 1,
        PIPE_STATE_INIT     = 2,
        PIPE_STATE_RUN      = 3,
        PIPE_STATE_MAX
    } pipe_state_t;

    pipe_state_t        m_state;
    Mutex               m_stateLock;

    virtual status_t    m_transitState(pipe_state_t state);
    virtual bool        m_checkState(pipe_state_t state);
};

}; /* namespace android */

#endif
