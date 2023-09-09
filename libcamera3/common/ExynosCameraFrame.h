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

#ifndef EXYNOS_CAMERA_FRAME_H
#define EXYNOS_CAMERA_FRAME_H

#include <utils/List.h>

#include "ExynosCameraParameters.h"
#include "ExynosCameraSensorInfo.h"
#include "ExynosCameraBuffer.h"
#include "ExynosCameraList.h"
#include "ExynosCameraNode.h"

typedef ExynosCameraList<uint32_t> frame_key_queue_t;

/* #define DEBUG_FRAME_MEMORY_LEAK */

#ifndef MAX_NUM_PIPES
#define MAX_NUM_PIPES \
    GET_MAX_NUM((MAX_PIPE_NUM % 100),(MAX_PIPE_NUM_FRONT % 100),(MAX_PIPE_NUM_REPROCESSING % 100))
#endif

#ifndef INDEX
#define INDEX(x)            ((x % 100) % MAX_NUM_PIPES)
#endif

namespace android {

/* Frame type for debugging */
typedef enum FRAME_TYPE {
    FRAME_TYPE_BASE                     = 0,
    FRAME_TYPE_OTHERS                   = 1,
    FRAME_TYPE_PREVIEW                  = 2,
    FRAME_TYPE_PREVIEW_FRONT            = 3,
    FRAME_TYPE_REPROCESSING             = 4,
    FRAME_TYPE_VISION                   = 5,
    FRAME_TYPE_INTERNAL                 = 6,
#ifdef USE_DUAL_CAMERA
    FRAME_TYPE_PREVIEW_SLAVE            = 7,
    FRAME_TYPE_PREVIEW_DUAL_MASTER      = 8,
    FRAME_TYPE_PREVIEW_DUAL_SLAVE       = 9,
    FRAME_TYPE_REPROCESSING_DUAL_MASTER = 10,
    FRAME_TYPE_REPROCESSING_DUAL_SLAVE  = 11,
    /*
     * "Transition" means that
     * Master or Slave camera was ready to enter "INTERNAL" mode
     * (post processing standby).
     * This type is necessary to avoid frame drop
     * by changing from "INTERNAL" to "NORMAL" in opposite camera.
     */
    FRAME_TYPE_TRANSITION               = 12,
#endif
    FRAME_TYPE_MAX,
} frame_type_t;

typedef enum entity_type {
    ENTITY_TYPE_INPUT_ONLY              = 0, /* Need input buffer only */
    ENTITY_TYPE_OUTPUT_ONLY             = 1, /* Need output buffer only */
    ENTITY_TYPE_INPUT_OUTPUT            = 2, /* Need input/output both */
    ENTITY_TYPE_INVALID,
} entity_type_t;

/* Entity state define */
typedef enum entity_state {
    ENTITY_STATE_NOP                    = 0, /* Do not need operation */
    ENTITY_STATE_READY                  = 1, /* Ready to operation */
    ENTITY_STATE_PROCESSING             = 2, /* Processing stage */
    ENTITY_STATE_FRAME_DONE             = 3, /* Pipe has been done HW operation. */
    ENTITY_STATE_COMPLETE               = 4, /* Complete frame, done for all SW work. */
    ENTITY_STATE_REWORK                 = 5, /* After COMPLETE, but need re-work entity */
    ENTITY_STATE_FRAME_SKIP             = 6, /* Entity has been skipped */
} entity_state_t;

typedef enum entity_buffer_type {
    ENTITY_BUFFER_FIXED                 = 0, /* Buffer is never change */
    ENTITY_BUFFER_DELIVERY              = 1, /* Buffer change is possible */
    ENTITY_BUFFER_INVALID
} entity_buffer_type_t;

typedef enum entity_buffer_state {
    ENTITY_BUFFER_STATE_NOREQ           = 0, /* This buffer is not used */
    ENTITY_BUFFER_STATE_REQUESTED       = 1, /* This buffer is not used */
    ENTITY_BUFFER_STATE_READY           = 2, /* Buffer is ready */
    ENTITY_BUFFER_STATE_PROCESSING      = 3, /* Buffer is being prossed */
    ENTITY_BUFFER_STATE_COMPLETE        = 4, /* Buffer is complete */
    ENTITY_BUFFER_STATE_ERROR           = 5, /* Buffer is complete */
    ENTITY_BUFFER_STATE_INVALID,
} entity_buffer_state_t;

enum DST_BUFFER_COUNT
{
    DST_BUFFER_DEFAULT = 0,
    DST_BUFFER_COUNT_MAX = MAX_NODE, /* this must same with MAX_NODE */
};

enum SRC_BUFFER_COUNT
{
    SRC_BUFFER_DEFAULT = 0,
    SRC_BUFFER_COUNT_MAX = MAX_OUTPUT_NODE,
};

/*
 * Master camera can have BYPASS, SYNC types
 * Slave camera can have SYNC, SWITCH types
 *
 * - BYPASS : used master camera's frame only
 * - SYNC :   used master/slave camera's frames
 * - SWITCH : used slave camera's frame only
 */
typedef enum sync_type
{
    SYNC_TYPE_BASE,
    SYNC_TYPE_BYPASS,
    SYNC_TYPE_SYNC,
    SYNC_TYPE_SWITCH,
    SYNC_TYPE_MAX,
} sync_type_t;

typedef enum STREAM_TYPE {
    STREAM_TYPE_CAPTURE				= 0,
    STREAM_TYPE_RAW					= 1,
    STREAM_TYPE_ZSL_INPUT			= 2,
    STREAM_TYPE_ZSL_OUTPUT			= 3,
    STREAM_TYPE_DEPTH				= 4,
    STREAM_TYPE_DEPTH_STALL			= 5,
    STREAM_TYPE_YUVCB_STALL			= 6,
    STREAM_TYPE_THUMBNAIL_CB		= 7,
    STREAM_TYPE_VISION				= 8,
    STREAM_TYPE_MAX,
} STREAM_TYPE_T;

class ExynosCameraFrameEntity {
public:
    ExynosCameraFrameEntity(
        uint32_t pipeId,
        entity_type_t type,
        entity_buffer_type_t bufType);
    uint32_t getPipeId(void);

    status_t setSrcBuf(ExynosCameraBuffer buf, uint32_t nodeIndex = 0);
    status_t setDstBuf(ExynosCameraBuffer buf, uint32_t nodeIndex = 0);

    status_t getSrcBuf(ExynosCameraBuffer *buf, uint32_t nodeIndex = 0);
    status_t getDstBuf(ExynosCameraBuffer *buf, uint32_t nodeIndex = 0);

    status_t setSrcRect(ExynosRect rect, uint32_t nodeIndex = 0);
    status_t setDstRect(ExynosRect rect, uint32_t nodeIndex = 0);

    status_t getSrcRect(ExynosRect *rect, uint32_t nodeIndex = 0);
    status_t getDstRect(ExynosRect *rect, uint32_t nodeIndex = 0);

    status_t setSrcBufState(entity_buffer_state_t state, uint32_t nodeIndex = 0);
    status_t setDstBufState(entity_buffer_state_t state, uint32_t nodeIndex = 0);

    entity_buffer_state_t getSrcBufState(uint32_t nodeIndex = 0);
    entity_buffer_state_t getDstBufState(uint32_t nodeIndex = 0);

    entity_buffer_type_t getBufType(void);

    status_t        setEntityState(entity_state_t state);
    entity_state_t  getEntityState(void);

    ExynosCameraFrameEntity *getPrevEntity(void);
    ExynosCameraFrameEntity *getNextEntity(void);

    status_t setPrevEntity(ExynosCameraFrameEntity *entity);
    status_t setNextEntity(ExynosCameraFrameEntity *entity);

    bool     flagSpecficParent(void);
    status_t setParentPipeId(enum pipeline parentPipeId);
    int      getParentPipeId(void);

    status_t setRotation(int rotation);
    int      getRotation(void);
    status_t setFlipHorizontal(int flipHorizontal);
    int      getFlipHorizontal(void);
    status_t setFlipVertical(int flipVertical);
    int      getFlipVertical(void);
private:
    status_t m_setEntityType(entity_type_t type);

private:
    uint32_t                m_pipeId;
    ExynosCameraBuffer      m_srcBuf[SRC_BUFFER_COUNT_MAX];
    ExynosCameraBuffer      m_dstBuf[DST_BUFFER_COUNT_MAX];

    ExynosRect              m_srcRect[SRC_BUFFER_COUNT_MAX];
    ExynosRect              m_dstRect[DST_BUFFER_COUNT_MAX];

    entity_type_t           m_EntityType;
    entity_buffer_type_t    m_bufferType;

    entity_buffer_state_t   m_srcBufState[SRC_BUFFER_COUNT_MAX];
    entity_buffer_state_t   m_dstBufState[DST_BUFFER_COUNT_MAX];
    entity_state_t          m_entityState;

    ExynosCameraFrameEntity *m_prevEntity;
    ExynosCameraFrameEntity *m_nextEntity;

    bool                     m_flagSpecificParent;
    int                      m_parentPipeId;

    int                      m_rotation;
    int                      m_flipHorizontal;
    int                      m_flipVertical;
};

/* Frame state define */
typedef enum frame_status {
    FRAME_STATE_READY      = 0,    /* Ready to operation */
    FRAME_STATE_RUNNING    = 1,    /* Frame is running */
    FRAME_STATE_COMPLETE   = 2,    /* Complete frame. */
    FRAME_STATE_SKIPPED    = 3,    /* This Frame has been skipped. */
    FRAME_STATE_INVALID    = 4,    /* Invalid state */
} frame_status_t;

typedef struct ExynosCameraPerFrameInfo {
    bool perFrameControlNode;
    int perFrameNodeIndex;
    int perFrameNodeVideID;
} camera_per_fream_into_t;

typedef struct ExynosCameraDupBufferInfo {
    int streamID;
    int extScalerPipeID;
} dup_buffer_info_t;

class ExynosCameraFrame : public RefBase {

    friend class FrameWorker;
    friend class CreateWorker;
    friend class DeleteWorker;
    friend class ExynosCameraFrameManager;

private:
    ExynosCameraFrame(
            int cameraId,
            ExynosCameraParameters *obj_param,
            uint32_t frameCount,
            uint32_t frameType = 0);
    ExynosCameraFrame(int cameraId);

    ~ExynosCameraFrame();

public:
    /* If curEntity is NULL, newEntity is added to m_linkageList */
    status_t        addSiblingEntity(
                        ExynosCameraFrameEntity *curEntity,
                        ExynosCameraFrameEntity *newEntity);
    status_t        addChildEntity(
                        ExynosCameraFrameEntity *parentEntity,
                        ExynosCameraFrameEntity *newEntity);
    status_t        addChildEntity(
                        ExynosCameraFrameEntity *parentEntity,
                        ExynosCameraFrameEntity *newEntity,
                        int parentPipeId);

    ExynosCameraFrameEntity *getFirstEntity(void);
    ExynosCameraFrameEntity *getNextEntity(void);
    /* Unused, but useful */
    /* ExynosCameraFrameEntity *getChildEntity(ExynosCameraFrameEntity *parentEntity); */

    ExynosCameraFrameEntity *searchEntityByPipeId(uint32_t pipeId);

    status_t        setSrcBuffer(
                        uint32_t pipeId,
                        ExynosCameraBuffer srcBuf,
                        uint32_t nodeIndex = 0);
    status_t        setDstBuffer(
                        uint32_t pipeId,
                        ExynosCameraBuffer dstBuf,
                        uint32_t nodeIndex = 0);
    status_t        setDstBuffer(
                        uint32_t pipeId,
                        ExynosCameraBuffer dstBuf,
                        uint32_t nodeIndex,
                        int      parentPipeId);

    status_t        getSrcBuffer(
                        uint32_t pipeId,
                        ExynosCameraBuffer *srcBuf,
                        uint32_t nodeIndex = 0);
    status_t        getDstBuffer(
                        uint32_t pipeId,
                        ExynosCameraBuffer *dstBuf,
                        uint32_t nodeIndex = 0);

    status_t        setSrcRect(
                        uint32_t pipeId,
                        ExynosRect srcRect,
                        uint32_t nodeIndex = 0);
    status_t        setDstRect(
                        uint32_t pipeId,
                        ExynosRect dstRect,
                        uint32_t nodeIndex = 0);

    status_t        getSrcRect(
                        uint32_t pipeId,
                        ExynosRect *srcRect,
                        uint32_t nodeIndex = 0);
    status_t        getDstRect(
                        uint32_t pipeId,
                        ExynosRect *dstRect,
                        uint32_t nodeIndex = 0);

    status_t        getSrcBufferState(
                        uint32_t pipeId,
                        entity_buffer_state_t *state,
                        uint32_t nodeIndex = 0);
    status_t        getDstBufferState(
                        uint32_t pipeId,
                        entity_buffer_state_t *state,
                        uint32_t nodeIndex = 0);

    status_t        setSrcBufferState(
                        uint32_t pipeId,
                        entity_buffer_state_t state,
                        uint32_t nodeIndex = 0);
    status_t        setDstBufferState(
                        uint32_t pipeId,
                        entity_buffer_state_t state,
                        uint32_t nodeIndex = 0);

    status_t        ensureSrcBufferState(
                        uint32_t pipeId,
                        entity_buffer_state_t state);

    status_t        ensureDstBufferState(
                        uint32_t pipeId,
                        entity_buffer_state_t state);

    status_t        setEntityState(
                        uint32_t pipeId,
                        entity_state_t state);
    status_t        getEntityState(
                        uint32_t pipeId,
                        entity_state_t *state);

    status_t        getEntityBufferType(
                        uint32_t pipeId,
                        entity_buffer_type_t *type);

    void            setRequest(bool tap,
                               bool tac,
                               bool isp,
                               bool scc,
                               bool dis,
                               bool scp);

    void            setRequest(bool tap,
                               bool tac,
                               bool isp,
                               bool ispp,
                               bool ispc,
                               bool scc,
                               bool dis,
                               bool scp);

    void            setRequest(uint32_t pipeId, bool val);
    bool            getRequest(uint32_t pipeId);

    uint32_t        getFrameCount(void);
    int32_t         getMetaFrameCount(uint32_t srcNodeIndex = 0);
    status_t        setNumRequestPipe(uint32_t num);
    uint32_t        getNumRequestPipe(void);
    uint32_t        getNumRemainPipe(void);

    bool            isComplete(void);
    ExynosCameraFrameEntity *getFrameDoneEntity(void);
    ExynosCameraFrameEntity *getFrameDoneEntity(uint32_t pipeID);
    ExynosCameraFrameEntity *getFrameDoneFirstEntity(void);
    ExynosCameraFrameEntity *getFrameDoneFirstEntity(uint32_t pipeID);

    status_t        skipFrame(void);

    void            setFrameState(frame_status_t state);
    frame_status_t  getFrameState(void);
    bool            checkFrameState(frame_status_t state);

    void            printEntity(void);
    void            printNotDoneEntity(void);
    void            dump(void);

    void            frameLock(void);
    void            frameUnlock(void);
    bool            getFrameLockState(void);

    status_t        initMetaData(struct camera2_shot_ext *shot, uint32_t srcNodeIndex = 0);
    status_t        getMetaData(struct camera2_shot_ext *shot, uint32_t srcNodeIndex = 0);
    status_t        setMetaData(struct camera2_shot_ext *shot, uint32_t srcNodeIndex = 0);

    status_t        storeDynamicMeta(struct camera2_shot_ext *shot, uint32_t srcNodeIndex = 0);
    status_t        storeDynamicMeta(struct camera2_dm *dm, uint32_t srcNodeIndex = 0);
    status_t        storeUserDynamicMeta(struct camera2_shot_ext *shot, uint32_t srcNodeIndex = 0);
    status_t        storeUserDynamicMeta(struct camera2_udm *udm, uint32_t srcNodeIndex = 0);

    status_t        getDynamicMeta(struct camera2_shot_ext *shot, uint32_t srcNodeIndex = 0);
    status_t        getDynamicMeta(struct camera2_dm *dm, uint32_t srcNodeIndex = 0);
    status_t        getUserDynamicMeta(struct camera2_shot_ext *shot, uint32_t srcNodeIndex = 0);
    status_t        getUserDynamicMeta(struct camera2_udm *udm, uint32_t srcNodeIndex = 0);

    status_t        setMetaDataEnable(bool flag, uint32_t srcNodeIndex = 0);
    bool            getMetaDataEnable(uint32_t srcNodeIndex = 0);

    int             getZoom(uint32_t srcNodeIndex = 0);
    status_t        setZoom(int zoom, uint32_t srcNodeIndex = 0);

    float           getZoomRatio(uint32_t srcNodeIndex = 0);
    status_t        setZoomRatio(float zoomRatio, uint32_t srcNodeIndex = 0);

    status_t        getNodeGroupInfo(struct camera2_node_group *node_group, int index, uint32_t srcNodeIndex = 0);
    status_t        getNodeGroupInfo(struct camera2_node_group *node_group, int index, int *zoom, uint32_t srcNodeIndex = 0);
    status_t        getNodeGroupInfo(struct camera2_node_group *node_group, int index, float *zoomRatio, uint32_t srcNodeIndex = 0);
    status_t        storeNodeGroupInfo(struct camera2_node_group *node_group, int index, uint32_t srcNodeIndex = 0);
    status_t        storeNodeGroupInfo(struct camera2_node_group *node_group, int index, int zoom, uint32_t srcNodeIndex = 0);
    status_t        storeNodeGroupInfo(struct camera2_node_group *node_group, int index, float zoomRatio, uint32_t srcNodeIndex = 0);
    void            dumpNodeGroupInfo(const char *name, uint32_t srcNodeIndex = 0);

    sync_type_t     getSyncType(void);
    status_t        setSyncType(sync_type_t type);

    int             getCameraId(void);

    /* backup for reprocessing */
    void            setReprocessingFrameType(frame_type_t frameType);
    uint32_t        getReprocessingFrameType(void);
    int             getReprocessingZoom(void);
    status_t        setReprocessingZoom(int zoom);
    sync_type_t     getReprocessingSyncType(void);
    status_t        setReprocessingSyncType(sync_type_t type);

    void            setJpegSize(int size);
    int             getJpegSize(void);

    int64_t         getTimeStamp(uint32_t srcNodeIndex = 0);
#ifdef  SAMSUNG_TIMESTAMP_BOOT
    int64_t         getTimeStampBoot(uint32_t srcNodeIndex = 0);
#endif
    void            getFpsRange(uint32_t *min, uint32_t *max, uint32_t srcNodeIndex = 0);
    void            setFpsRange(uint32_t min, uint32_t max, uint32_t srcNodeIndex = 0);

    void            setIspDone(bool done);
    void            set3aaDrop(bool flag);
    void            setIspcDrop(bool flag);
    void            setDisDrop(bool flag);
    void            setScpDrop(bool flag);
    void            setSccDrop(bool flag);
    bool            getIspDone(void);
    bool            get3aaDrop(void);
    bool            getIspcDrop(void);
    bool            getScpDrop(void);
    bool            getSccDrop(void);
    bool            getDisDrop(void);

    uint32_t        getUniqueKey(void);
    status_t        setUniqueKey(uint32_t uniqueKey);
    status_t        setFrameInfo(ExynosCameraParameters *obj_param, uint32_t frameCount, uint32_t frameType);
    void            setFrameType(frame_type_t frameType);
    uint32_t        getFrameType(void);

    void setFrameYuvStallPortUsage(int usage) {
        m_yuvStallPortEnable = usage;
    }

    int getFrameYuvStallPortUsage(void) {
        return m_yuvStallPortEnable;
    }

    dup_buffer_info_t       getDupBufferInfo() {
        return m_dupBufferInfo;
    }

    void setDupBufferInfo(dup_buffer_info_t dupBufferInfo) {
        m_dupBufferInfo = dupBufferInfo;
        return;
    }

#ifdef DEBUG_FRAME_MEMORY_LEAK
    long long int   getCheckLeakCount();
#endif

    status_t setRotation(uint32_t pipeId, int rotation);
    int      getRotation(uint32_t pipeId);
    status_t setFlipHorizontal(uint32_t pipeId, int flipHorizontal);
    int      getFlipHorizontal(uint32_t pipeId);
    status_t setFlipVertical(uint32_t pipeId, int flipVertical);
    int      getFlipVertical(uint32_t pipeId);

#ifdef CORRECT_TIMESTAMP_FOR_SENSORFUSION
    void setAdjustedTimestampFlag(bool flag) {m_adjustedTimestampFlag = flag;};
    bool getAdjustedTimestampFlag(void) {return m_adjustedTimestampFlag;};
#endif

    void setFrameSpecialCaptureStep(uint32_t state) {m_specialCaptureStep = state;};
    uint32_t getFrameSpecialCaptureStep(void) {return m_specialCaptureStep;};

#ifdef SAMSUNG_TN_FEATURE
    void    setScenario(int index, int scenario);
    int32_t getScenario(int pipeId);
    int32_t getScenarioIndex(int scenario);
    bool    hasScenario(int scenario);
#endif

    void setStreamRequested(int stream, bool flag);
    bool getStreamRequested(int stream);

    void setHasRequest(bool hasReques) {m_hasRequest = hasReques;};
    bool getHasRequest(void) {return m_hasRequest;};

    void setUpdateResult(bool flag);
    bool getUpdateResult(void);

private:
    status_t        m_init();
    status_t        m_deinit();

    /* ACCESS allowed only frameManager */
    status_t        setFrameMgrInfo(frame_key_queue_t *queue);

private:
    int                         m_cameraId;
    char                        m_name[EXYNOS_CAMERA_NAME_STR_SIZE];

    mutable Mutex                        m_linkageLock;
    List<ExynosCameraFrameEntity *>      m_linkageList;
    List<ExynosCameraFrameEntity *>::iterator m_currentEntity;

    ExynosCameraParameters      *m_parameters;
    uint32_t                    m_frameCount;
    uint32_t                    m_frameType;

    frame_status_t              m_frameState;
    mutable Mutex               m_frameStateLock;
    mutable Mutex               m_frameLock;

    uint32_t                    m_numRequestPipe;
    uint32_t                    m_numCompletePipe;

    bool                        m_frameLocked;

    bool                        m_metaDataEnable[SRC_BUFFER_COUNT_MAX];
    struct camera2_shot_ext     m_metaData[SRC_BUFFER_COUNT_MAX];
    struct camera2_node_group   m_node_gorup[SRC_BUFFER_COUNT_MAX][PERFRAME_NODE_GROUP_MAX];
    int                         m_zoom[SRC_BUFFER_COUNT_MAX];
    float                       m_zoomRatio[SRC_BUFFER_COUNT_MAX];
    sync_type_t                 m_syncType;

    /* backup field for reprocessing */
    uint32_t                    m_reprocessingFrameType;
    int                         m_reprocessingZoom;
    sync_type_t                 m_reprocessingSyncType;

    int                         m_jpegSize;

    bool                        m_request[MAX_NUM_PIPES];

    bool                        m_ispDoneFlag;
    bool                        m_3aaDropFlag;
    bool                        m_ispcDropFlag;
    bool                        m_disDropFlag;
    bool                        m_sccDropFlag;
    bool                        m_scpDropFlag;

    uint32_t                    m_uniqueKey;
    bool                        m_yuvStallPortEnable;

    mutable Mutex               m_refCountLock;
    int32_t                     m_refCount;
    mutable Mutex               m_entityLock;

#ifdef DEBUG_FRAME_MEMORY_LEAK
    static Mutex                m_countLock;
    /* total remained frame which not deleted */
    static unsigned long long   m_checkLeakCount;
    /* unique frame count to trace */
    static unsigned long long   m_checkLeakFrameCount;
    /* m_checkLeakFrameCount */
    unsigned long long          m_privateCheckLeakCount;
#endif
    dup_buffer_info_t           m_dupBufferInfo;
#ifdef CORRECT_TIMESTAMP_FOR_SENSORFUSION
    bool m_adjustedTimestampFlag;
#endif
    uint32_t                    m_specialCaptureStep;

    frame_key_queue_t          *m_frameQueue;
    bool                        m_hasRequest;
    bool                        m_updateResult;

#ifdef SAMSUNG_TN_FEATURE
    int32_t                     m_scenario[MAX_PIPE_UNI_NUM];
#endif
    bool                        m_stream[STREAM_TYPE_MAX];
};

}; /* namespace android */

#endif
