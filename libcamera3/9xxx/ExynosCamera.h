/*
 * Copyright (C) 2017, Samsung Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef EXYNOS_CAMERA_HW_IMPLEMENTATION_H
#define EXYNOS_CAMERA_HW_IMPLEMENTATION_H

#include "ExynosCameraDefine.h"

#include "ExynosCameraRequestManager.h"
#include "ExynosCameraStreamManager.h"
#include "ExynosCameraMetadataConverter.h"
#include "ExynosCameraParameters.h"
#include "ExynosCameraFrameManager.h"
#include "ExynosCameraFrameFactory.h"
#include "ExynosCameraFrameFactoryPreview.h"
#include "ExynosCameraFrameFactoryPreviewFrontPIP.h"
#include "ExynosCameraFrameFactoryVision.h"
#include "ExynosCameraFrameReprocessingFactory.h"
#include "ExynosCameraTimeLogger.h"
#include "ExynosCameraFrameJpegReprocessingFactory.h"

#ifdef USE_DUAL_CAMERA
#include "ExynosCameraFrameFactoryPreviewDual.h"
#include "ExynosCameraFrameReprocessingFactoryDual.h"
#endif

#ifdef USES_SW_VDIS
//class ExynosCameraSolutionSWVdis;
#include "ExynosCameraSolutionSWVdis.h"
#endif

namespace android {

#define SET_STREAM_CONFIG_BIT(_BIT,_STREAM_ID) \
    ((_BIT) |= (1 << ((_STREAM_ID) % HAL_STREAM_ID_MAX)))

#define COMPARE_STREAM_CONFIG_BIT(_BIT1,_BIT2) \
    ((_BIT1) & (_BIT2))

#define IS_OUTPUT_NODE(_FACTORY,_PIPE) \
    (((_FACTORY->getNodeType(_PIPE) >= OUTPUT_NODE) \
        && (_FACTORY->getNodeType(_PIPE) < MAX_OUTPUT_NODE)) ? true : false)

typedef ExynosCameraThread<ExynosCamera> mainCameraThread;
typedef ExynosCameraList<ExynosCameraFrameFactory *> framefactory3_queue_t;

class ExynosCameraStreamThread : public ExynosCameraThread <ExynosCameraStreamThread>
{
public:
    ExynosCameraStreamThread(ExynosCamera* cameraObj, const char* name, int pipeId);
    ~ExynosCameraStreamThread();

    frame_queue_t* getFrameDoneQ();
    void setFrameDoneQ(frame_queue_t* frameDoneQ);

private:
    ExynosCamera* m_mainCamera;
    frame_queue_t* m_frameDoneQ;
    int m_pipeId;

private:
    bool m_streamThreadFunc(void);
};

#ifdef ADAPTIVE_RESERVED_MEMORY
enum BUF_TYPE {
    BUF_TYPE_UNDEFINED = 0,
    BUF_TYPE_NORMAL,
    BUF_TYPE_VENDOR,
    BUF_TYPE_REPROCESSING,
    BUF_TYPE_MAX
};

enum BUF_PRIORITY {
    BUF_PRIORITY_FLITE = 0,
    BUF_PRIORITY_ISP,
    BUF_PRIORITY_CAPTURE_CB,
    BUF_PRIORITY_YUV_CAP,
    BUF_PRIORITY_THUMB,
    BUF_PRIORITY_DSCALEYUVSTALL,
    BUF_PRIORITY_FUSION,
    BUF_PRIORITY_VRA,
    BUF_PRIORITY_MCSC_OUT,
    BUF_PRIORITY_SSM,
    BUF_PRIORITY_ISP_RE,
    BUF_PRIORITY_JPEG_INTERNAL,
    BUF_PRIORITY_MAX
};

typedef struct {
    buffer_manager_tag_t bufTag;
    buffer_manager_configuration_t bufConfig;
    int minReservedNum;
    int totalPlaneSize;
    enum BUF_TYPE bufType;
} adaptive_priority_buffer_t;
#endif

typedef struct {
    ExynosCameraParameters *parameters;
    ExynosCameraFrameFactory *previewFactory;
    ExynosCameraFrameFactory *reprocessingFactory;
    ExynosCameraActivityControl *activityControl;
    ExynosCameraFrameSelector *captureSelector;
} frame_handle_components_t;

#ifdef USE_DUAL_CAMERA
typedef enum DUAL_STANDBY_TRIGGER {
    DUAL_STANDBY_TRIGGER_NONE,
    DUAL_STANDBY_TRIGGER_MASTER_ON,
    DUAL_STANDBY_TRIGGER_MASTER_OFF,
    DUAL_STANDBY_TRIGGER_SLAVE_ON,
    DUAL_STANDBY_TRIGGER_SLAVE_OFF,
    DUAL_STANDBY_TRIGGER_MAX,
} dual_standby_trigger_t;
#endif

#ifdef DEBUG_FUSION_CAPTURE_DUMP
static uint32_t captureNum = 0;
#endif

class ExynosCamera {
public:
    ExynosCamera() {};
    ExynosCamera(int mainCameraId, int subCameraId, int scenario, camera_metadata_t **info);
    virtual             ~ExynosCamera();

    /** Startup */
    status_t    initializeDevice(const camera3_callback_ops *callbackOps);

    /** Stream configuration and buffer registration */
    status_t    configureStreams(camera3_stream_configuration_t *stream_list);

    /** Template request settings provision */
    status_t    construct_default_request_settings(camera_metadata_t **request, int type);

    /** Submission of capture requests to HAL */
    status_t    processCaptureRequest(camera3_capture_request *request);

    /** Vendor metadata registration */
    void        get_metadata_vendor_tag_ops(const camera3_device_t *, vendor_tag_query_ops_t *ops);

    /** Flush all currently in-process captures and all buffers */
    status_t    flush(void);

    status_t    setPIPMode(bool enabled);
    status_t    setDualMode(bool enabled);
    bool        getDualMode(void);

    /** Print out debugging state for the camera device */
    void        dump(void);

    /** Stop */
    status_t    releaseDevice(void);

    void        release();

    /* Common functions */
    int         getCameraId() const;
#ifdef USE_DUAL_CAMERA
    int         getSubCameraId() const;
#endif
    /* For Vendor */
    status_t    setParameters(const CameraParameters& params);

    bool        previewStreamFunc(ExynosCameraFrameSP_sptr_t newFrame, int pipeId);

private:
    typedef enum exynos_camera_state {
        EXYNOS_CAMERA_STATE_OPEN        = 0,
        EXYNOS_CAMERA_STATE_INITIALIZE  = 1,
        EXYNOS_CAMERA_STATE_CONFIGURED  = 2,
        EXYNOS_CAMERA_STATE_START       = 3,
        EXYNOS_CAMERA_STATE_RUN         = 4,
        EXYNOS_CAMERA_STATE_FLUSH       = 5,
        EXYNOS_CAMERA_STATE_ERROR       = 6,
        EXYNOS_CAMERA_STATE_MAX         = 7,
    } exynos_camera_state_t;

private:
    /* Helper functions for initialization */
    void        m_createThreads(void);
    void        m_createManagers(void);
    void        m_vendorSpecificPreConstructor(int cameraId, int scenario);/* Vendor */
    void        m_vendorSpecificConstructor(void);    /* Vendor */
    void        m_vendorSpecificPreDestructor(void);  /* Vendor */
    void        m_vendorSpecificDestructor(void);     /* Vendor */
    void        m_vendorCreateThreads(void);         /* Vendor */
    status_t    m_reInit(void);
    status_t    m_vendorReInit(void);

    /* Helper functions for notification */
    status_t    m_sendJpegStreamResult(ExynosCameraRequestSP_sprt_t request, ExynosCameraBuffer *buffer, int size);
    status_t    m_sendRawStreamResult(ExynosCameraRequestSP_sprt_t request, ExynosCameraBuffer *buffer);
    status_t    m_sendVisionStreamResult(ExynosCameraRequestSP_sprt_t request, ExynosCameraBuffer *buffer);
    status_t    m_sendZslStreamResult(ExynosCameraRequestSP_sprt_t request, ExynosCameraBuffer *buffer);
#ifdef SUPPORT_DEPTH_MAP
    status_t    m_sendDepthStreamResult(ExynosCameraRequestSP_sprt_t request, ExynosCameraBuffer *buffer, int streamId);
#endif
    status_t    m_sendYuvStreamResult(ExynosCameraRequestSP_sprt_t request, ExynosCameraBuffer *buffer,
                                                    int streamId, bool skipBuffer, uint64_t streamTimeStamp
                                                    );
    status_t    m_sendYuvStreamStallResult(ExynosCameraRequestSP_sprt_t request, ExynosCameraBuffer *buffer, int streamId);
    status_t    m_sendThumbnailStreamResult(ExynosCameraRequestSP_sprt_t request, ExynosCameraBuffer *buffer, int streamId);
    status_t    m_sendForceYuvStreamResult(ExynosCameraRequestSP_sprt_t request, ExynosCameraFrameSP_sptr_t frame,
                                                ExynosCameraFrameFactory *factory);
    status_t    m_sendForceStallStreamResult(ExynosCameraRequestSP_sprt_t request);
    status_t    m_sendMeta(ExynosCameraRequestSP_sprt_t request, EXYNOS_REQUEST_RESULT::TYPE type);
    status_t    m_sendPartialMeta(ExynosCameraRequestSP_sprt_t request, EXYNOS_REQUEST_RESULT::TYPE type);
    status_t    m_sendNotifyShutter(ExynosCameraRequestSP_sprt_t request, uint64_t timeStamp = 0);
    status_t    m_sendNotifyError(ExynosCameraRequestSP_sprt_t request, EXYNOS_REQUEST_RESULT::ERROR error,
                                            camera3_stream_t *stream = NULL);

    /* Helper functions of Buffer operation */
    status_t    m_createIonAllocator(ExynosCameraIonAllocator **allocator);

    status_t    m_setBuffers(void);
    status_t    m_setVisionBuffers(void);
    status_t    m_setVendorBuffers();

    status_t    m_setupPreviewFactoryBuffers(const ExynosCameraRequestSP_sprt_t request,
                                                ExynosCameraFrameSP_sptr_t frame,
                                                ExynosCameraFrameFactory *factory);
    status_t    m_setupBatchFactoryBuffers(ExynosCameraRequestSP_sprt_t request,
                                                ExynosCameraFrameSP_sptr_t frame,
                                                ExynosCameraFrameFactory *factory);
    status_t    m_setupVisionFactoryBuffers(const ExynosCameraRequestSP_sprt_t request, ExynosCameraFrameSP_sptr_t frame);
    status_t    m_setupCaptureFactoryBuffers(const ExynosCameraRequestSP_sprt_t request, ExynosCameraFrameSP_sptr_t frame);

    status_t    m_allocBuffers(
                    const buffer_manager_tag_t tag,
                    const buffer_manager_configuration_t info);
#ifdef ADAPTIVE_RESERVED_MEMORY
    status_t    m_setupAdaptiveBuffer();
    status_t    m_addAdaptiveBufferInfos(const buffer_manager_tag_t tag,
                                         const buffer_manager_configuration_t info,
                                         enum BUF_PRIORITY buf_prior,
                                         enum BUF_TYPE buf_type);
    status_t    m_allocAdaptiveNormalBuffers();
    status_t    m_allocAdaptiveReprocessingBuffers();
    status_t    m_setAdaptiveReservedBuffers();
#endif

    /* helper functions for set buffer to frame */
    status_t    m_setupEntity(uint32_t pipeId, ExynosCameraFrameSP_sptr_t newFrame,
                                ExynosCameraBuffer *srcBuf = NULL,
                                ExynosCameraBuffer *dstBuf = NULL,
                                int dstNodeIndex = 0,
                                int dstPipeId = -1);
    status_t    m_setSrcBuffer(uint32_t pipeId, ExynosCameraFrameSP_sptr_t newFrame, ExynosCameraBuffer *buffer);
    status_t    m_setDstBuffer(uint32_t pipeId, ExynosCameraFrameSP_sptr_t newFrame, ExynosCameraBuffer *buffer,
                                        int nodeIndex = 0, int dstPipeId = -1);

    status_t    m_setVOTFBuffer(uint32_t pipeId, ExynosCameraFrameSP_sptr_t newFrame, uint32_t srcPipeId, uint32_t dstPipeId);
    status_t    m_setHWFCBuffer(uint32_t pipeId, ExynosCameraFrameSP_sptr_t newFrame, uint32_t srcPipeId, uint32_t dstPipeId);
    /* helper functions for frame factory */
    status_t    m_constructFrameFactory(void);
    status_t    m_startFrameFactory(ExynosCameraFrameFactory *factory);
    status_t    m_startReprocessingFrameFactory(ExynosCameraFrameFactory *factory);
    status_t    m_stopFrameFactory(ExynosCameraFrameFactory *factory);
    status_t    m_stopReprocessingFrameFactory(ExynosCameraFrameFactory *factory);
    status_t    m_deinitFrameFactory();

    /* frame Generation / Done handler */
    status_t    m_checkMultiCaptureMode(ExynosCameraRequestSP_sprt_t request);
    status_t    m_createInternalFrameFunc(ExynosCameraRequestSP_sprt_t request, enum Request_Sync_Type syncType,
                                                        frame_type_t frameType = FRAME_TYPE_INTERNAL);
    status_t    m_createPreviewFrameFunc(enum Request_Sync_Type syncType, bool flagFinishFactoryStart);
    status_t    m_createVisionFrameFunc(enum Request_Sync_Type syncType, bool flagFinishFactoryStart);
    status_t    m_createCaptureFrameFunc(void);

    status_t    m_previewFrameHandler(ExynosCameraRequestSP_sprt_t request,
                                      ExynosCameraFrameFactory *targetfactory,
                                      frame_type_t frameType);
    status_t    m_visionFrameHandler(ExynosCameraRequestSP_sprt_t request,
                                      ExynosCameraFrameFactory *targetfactory,
                                      frame_type_t frameType = FRAME_TYPE_VISION);
    status_t    m_captureFrameHandler(ExynosCameraRequestSP_sprt_t request,
                                      ExynosCameraFrameFactory *targetfactory,
                                      frame_type_t frameType);
    bool        m_previewStreamFunc(ExynosCameraFrameSP_sptr_t newFrame, int pipeId);

    void        m_updateCropRegion(struct camera2_shot_ext *shot_ext);
    status_t    m_updateJpegControlInfo(const struct camera2_shot_ext *shot_ext);
    void        m_updateFD(struct camera2_shot_ext *shot_ext, enum facedetect_mode fdMode,
                                int dsInputPortId, bool isReprocessing, bool isEarlyFd = 0);
    void        m_updateEdgeNoiseMode(struct camera2_shot_ext *shot_ext, bool isCaptureFrame);
    void        m_updateExposureTime(struct camera2_shot_ext *shot_ext);
    void        m_updateSetfile(struct camera2_shot_ext *shot_ext, bool isCaptureFrame);
    void        m_updateMasterCam(struct camera2_shot_ext *shot_ext);
    void        m_setTransientActionInfo(frame_handle_components_t *components);
    void        m_setApertureControl(frame_handle_components_t *components, struct camera2_shot_ext *shot_ext);
    status_t    m_updateYsumValue(ExynosCameraFrameSP_sptr_t frame, ExynosCameraRequestSP_sprt_t request);
    void        m_adjustSlave3AARegion(frame_handle_components_t *components, ExynosCameraFrameSP_sptr_t frame);

    /* helper functions for frame */
    status_t    m_generateFrame(ExynosCameraFrameFactory *factory,
                                List<ExynosCameraFrameSP_sptr_t> *list,
                                Mutex *listLock,
                                ExynosCameraFrameSP_dptr_t newFrame,
                                ExynosCameraRequestSP_sprt_t request);
    status_t    m_generateFrame(ExynosCameraFrameFactory *factory,
                                List<ExynosCameraFrameSP_sptr_t> *list,
                                Mutex *listLock,
                                ExynosCameraFrameSP_dptr_t newFrame,
                                ExynosCameraRequestSP_sprt_t request,
                                bool useJpegFlag);
    status_t    m_generateInternalFrame(ExynosCameraFrameFactory *factory,
                                        List<ExynosCameraFrameSP_sptr_t> *list,
                                        Mutex *listLock,
                                        ExynosCameraFrameSP_dptr_t newFrame,
                                        frame_type_t frameType,
                                        ExynosCameraRequestSP_sprt_t request = NULL);
#ifdef USE_DUAL_CAMERA
    status_t    m_generateTransitionFrame(ExynosCameraFrameFactory *factory,
                                          List<ExynosCameraFrameSP_sptr_t> *list,
                                          Mutex *listLock,
                                          ExynosCameraFrameSP_dptr_t newFrame,
                                          frame_type_t frameType,
                                          ExynosCameraRequestSP_sprt_t request);
#endif
    status_t    m_searchFrameFromList(List<ExynosCameraFrameSP_sptr_t> *list,
                                      Mutex *listLock,
                                      uint32_t frameCount,
                                      ExynosCameraFrameSP_dptr_t frame);
    status_t    m_removeFrameFromList(List<ExynosCameraFrameSP_sptr_t> *list,
                                      Mutex *listLock,
                                      ExynosCameraFrameSP_sptr_t frame);

    uint32_t    m_getSizeFromFrameList(List<ExynosCameraFrameSP_sptr_t> *list, Mutex *listLock);
    status_t    m_clearList(List<ExynosCameraFrameSP_sptr_t> *list, Mutex *listLock);

    uint32_t    m_getSizeFromRequestList(List<ExynosCameraRequestSP_sprt_t> *list, Mutex *listLock);
    status_t    m_clearRequestList(List<ExynosCameraRequestSP_sprt_t> *list, Mutex *listLock);

    status_t    m_removeInternalFrames(List<ExynosCameraFrameSP_sptr_t> *list, Mutex *listLock);
    status_t    m_releaseInternalFrame(ExynosCameraFrameSP_sptr_t frame);
    status_t    m_checkStreamBuffer(ExynosCameraFrameSP_sptr_t frame,
                                    ExynosCameraStream *stream,
                                    ExynosCameraBuffer *buffer,
                                    ExynosCameraRequestSP_sprt_t request,
                                    ExynosCameraFrameFactory *factory);
    status_t    m_checkStreamBufferStatus(ExynosCameraRequestSP_sprt_t request,
                                          ExynosCameraStream *stream, int *status,
                                          bool bufferSkip = false);
    void        m_checkUpdateResult(ExynosCameraFrameSP_sptr_t frame, uint32_t streamConfigBit);
    status_t    m_updateTimestamp(ExynosCameraRequestSP_sprt_t request, ExynosCameraFrameSP_sptr_t frame,
                                ExynosCameraBuffer *timestampBuffer);
    status_t    m_handlePreviewFrame(ExynosCameraFrameSP_sptr_t frame, int pipeId, ExynosCameraFrameFactory *factory);
    status_t    m_handleVisionFrame(ExynosCameraFrameSP_sptr_t frame, int pipeId, ExynosCameraFrameFactory *factory);
    status_t    m_handleInternalFrame(ExynosCameraFrameSP_sptr_t frame, int pipeId, ExynosCameraFrameFactory *factory);
    status_t    m_handleYuvCaptureFrame(ExynosCameraFrameSP_sptr_t frame);
    status_t    m_handleNV21CaptureFrame(ExynosCameraFrameSP_sptr_t frame, int leaderPipeId);
    status_t    m_resizeToDScaledYuvStall(ExynosCameraFrameSP_sptr_t frame, int leaderPipeId, int nodePipeId);
    status_t    m_handleJpegFrame(ExynosCameraFrameSP_sptr_t frame, int leaderPipeId);
    status_t    m_handleBayerBuffer(ExynosCameraFrameSP_sptr_t frame, ExynosCameraRequestSP_sprt_t request, int pipeId);
#ifdef SUPPORT_DEPTH_MAP
    status_t    m_handleDepthBuffer(ExynosCameraFrameSP_sptr_t frame, ExynosCameraRequestSP_sprt_t request);
#endif
#ifdef USES_SW_VDIS
    status_t    m_handleVdisFrame(ExynosCameraFrameSP_sptr_t frame, ExynosCameraRequestSP_sprt_t request, int pipeId, ExynosCameraFrameFactory * factory);
#endif
#ifdef USE_DUAL_CAMERA
    status_t    m_checkDualOperationMode(ExynosCameraRequestSP_sprt_t request,
                                         bool isNeedModeChange, bool isReprocessing, bool flagFinishFactoryStart);
    status_t    m_setStandbyModeTrigger(bool flagFinishFactoryStart,
                                        enum DUAL_OPERATION_MODE newDualOperationMode,
                                        enum DUAL_OPERATION_MODE oldDualOperationMode,
                                        bool isNeedModeChange,
                                        bool *isTriggered);
#endif

    /* helper functions for request */
    status_t    m_pushServiceRequest(camera3_capture_request *request, ExynosCameraRequestSP_dptr_t req,
                                                bool skipRequest = false);
    status_t    m_popServiceRequest(ExynosCameraRequestSP_dptr_t request);
    status_t    m_pushRunningRequest(ExynosCameraRequestSP_dptr_t request_in);
    status_t    m_updateResultShot(ExynosCameraRequestSP_sprt_t request, struct camera2_shot_ext *src_ext,
                                   enum metadata_type metaType = PARTIAL_NONE, frame_type_t frameType = FRAME_TYPE_BASE);
    status_t    m_updateJpegPartialResultShot(ExynosCameraRequestSP_sprt_t request, struct camera2_shot_ext *dst_ext);
    status_t    m_setFactoryAddr(ExynosCameraRequestSP_dptr_t request);
    status_t    m_updateFDAEMetadata(ExynosCameraFrameSP_sptr_t frame);

    /* helper functions for configuration options */
    uint32_t    m_getBayerPipeId(void);
    status_t    m_setFrameManager();
    status_t    m_setupFrameFactoryToRequest();
    status_t    m_setConfigInform();
    status_t    m_setStreamInfo(camera3_stream_configuration *streamList);
    status_t    m_enumStreamInfo(camera3_stream_t *stream);
    status_t    m_checkStreamInfo(void);

    status_t    m_getBayerServiceBuffer(ExynosCameraFrameSP_sptr_t frame,
                                        ExynosCameraBuffer *buffer,
                                        ExynosCameraRequestSP_sprt_t request);
    status_t    m_releaseSelectorTagBuffers(ExynosCameraFrameSP_sptr_t bayerFrame);
    status_t    m_getBayerBuffer(uint32_t pipeId,
                                 uint32_t frameCount,
                                 ExynosCameraBuffer *buffer,
                                 ExynosCameraFrameSelector *selector,
                                 frame_type_t frameType,
                                 ExynosCameraFrameSP_dptr_t frame
#ifdef SUPPORT_DEPTH_MAP
                                 , ExynosCameraBuffer *depthMapBuffer = NULL
#endif
                                            );
    status_t    m_checkBufferAvailable(uint32_t pipeId);
    bool        m_checkValidBayerBufferSize(struct camera2_stream *stream, ExynosCameraFrameSP_sptr_t frame, bool flagForceRecovery);
    bool        m_isRequestEssential(ExynosCameraRequestSP_dptr_t request);

    status_t                m_transitState(exynos_camera_state_t state);
    exynos_camera_state_t   m_getState(void);
    bool                    m_isSkipBurstCaptureBuffer(frame_type_t frameType);

    /* s/w processing functions */
#if defined(USE_RAW_REVERSE_PROCESSING) && defined(USE_SW_RAW_REVERSE_PROCESSING)
    void        m_reverseProcessingApplyCalcRGBgain(int32_t *inRGBgain, int32_t *invRGBgain, int32_t *pedestal);
    int         m_reverseProcessingApplyGammaProc(int pixelIn, int32_t *lutX, int32_t *lutY);
    status_t    m_reverseProcessingBayer(ExynosCameraFrameSP_sptr_t frame, ExynosCameraBuffer *inBuf, ExynosCameraBuffer *outBuf);
#endif

    /* debuger functions */
#ifdef FPS_CHECK
    void        m_debugFpsCheck(enum pipeline pipeId);
#endif
    void        getFrameHandleComponents(frame_type_t frameType, frame_handle_components_t *components);

#ifdef SUPPORT_ME
    status_t    m_handleMeBuffer(ExynosCameraFrameSP_sptr_t frame, int mePos, const int meLeaderPipeId = -1);
#endif

private:
    ExynosCameraRequestManager      *m_requestMgr;
    ExynosCameraMetadataConverter   *m_metadataConverter;
    ExynosCameraConfigurations      *m_configurations;
    ExynosCameraParameters          *m_parameters[CAMERA_ID_MAX];
    ExynosCameraStreamManager       *m_streamManager;
    ExynosCameraFrameManager        *m_frameMgr;
    ExynosCameraIonAllocator        *m_ionAllocator;
    ExynosCameraBufferSupplier      *m_bufferSupplier;
    ExynosCameraActivityControl     *m_activityControl[CAMERA_ID_MAX];

    ExynosCameraFrameFactory        *m_frameFactory[FRAME_FACTORY_TYPE_MAX];
    framefactory3_queue_t           *m_frameFactoryQ;

    ExynosCameraFrameSelector       *m_captureSelector[CAMERA_ID_MAX];
    ExynosCameraFrameSelector       *m_captureZslSelector;

private:
#ifdef USE_DUAL_CAMERA
    int                             m_cameraIds[2];
#else
    int                             m_cameraIds[1];
#endif
    int                             m_cameraId;

#ifdef ADAPTIVE_RESERVED_MEMORY
    adaptive_priority_buffer_t      m_adaptiveBufferInfos[BUF_PRIORITY_MAX];
#endif

    uint32_t                        m_scenario;
    char                            m_name[EXYNOS_CAMERA_NAME_STR_SIZE];
    mutable Condition               m_captureResultDoneCondition;
    mutable Mutex                   m_captureResultDoneLock;
    uint64_t                        m_lastFrametime;
    int                             m_prepareFrameCount;

    mutable Mutex                   m_frameCompleteLock;

    int                             m_shutterSpeed;
    int                             m_gain;
    int                             m_irLedWidth;
    int                             m_irLedDelay;
    int                             m_irLedCurrent;
    int                             m_irLedOnTime;

    int                             m_visionFps;

    exynos_camera_state_t           m_state;
    Mutex                           m_stateLock;

    bool                            m_captureStreamExist;
    bool                            m_rawStreamExist;
    bool                            m_videoStreamExist;

    bool                            m_recordingEnabled;

    struct ExynosConfigInfo         *m_exynosconfig;

    mutable Mutex                   m_currentPreviewShotLock;
    struct camera2_shot_ext         *m_currentPreviewShot;
    struct camera2_shot_ext         *m_currentCaptureShot;

#ifdef MONITOR_LOG_SYNC
    static uint32_t                 cameraSyncLogId;
    int                             m_syncLogDuration;
    uint32_t                        m_getSyncLogId(void);
#endif
    /* process queue */
    List<ExynosCameraFrameSP_sptr_t >   m_processList;
    mutable Mutex                       m_processLock;
    List<ExynosCameraFrameSP_sptr_t >   m_captureProcessList;
    mutable Mutex                       m_captureProcessLock;
    frame_queue_t                       *m_pipeFrameDoneQ[MAX_PIPE_NUM];

    /* capture Queue */
    frame_queue_t                   *m_selectBayerQ;
    frame_queue_t                   *m_bayerStreamQ;
    frame_queue_t                   *m_captureQ;
    frame_queue_t                   *m_yuvCaptureDoneQ;
    frame_queue_t                   *m_reprocessingDoneQ;

    ExynosCameraList<ExynosCameraFrameSP_sptr_t> *m_shotDoneQ;
    status_t                        m_waitShotDone(enum Request_Sync_Type syncType, ExynosCameraFrameSP_dptr_t reqSyncFrame);
#ifdef USE_DUAL_CAMERA
    frame_queue_t                   *m_selectDualSlaveBayerQ;
    ExynosCameraList<ExynosCameraFrameSP_sptr_t> *m_slaveShotDoneQ;
    ExynosCameraList<dual_standby_trigger_t> *m_dualStandbyTriggerQ;
    uint32_t                        m_dualTransitionCount;
    uint32_t                        m_dualCaptureLockCount;
    mutable Mutex                   m_dualOperationModeLock;
    bool                            m_dualMultiCaptureLockflag;
    uint32_t                        m_earlyTriggerRequestKey;

    List<ExynosCameraRequestSP_sprt_t>  m_essentialRequestList;
    List<ExynosCameraRequestSP_sprt_t>  m_latestRequestList;
    mutable Mutex                   m_latestRequestListLock;

    enum DUAL_OPERATION_MODE        m_earlyDualOperationMode;

    volatile int32_t                m_needSlaveDynamicBayerCount;
#endif
    List<ExynosCameraRequestSP_sprt_t>  m_requestPreviewWaitingList;
    List<ExynosCameraRequestSP_sprt_t>  m_requestCaptureWaitingList;
    mutable Mutex                   m_requestPreviewWaitingLock;
    mutable Mutex                   m_requestCaptureWaitingLock;

    uint32_t                        m_firstRequestFrameNumber;
    int                             m_internalFrameCount;
    bool                            m_isNeedRequestFrame;

    // TODO: Temporal. Efficient implementation is required.
    mutable Mutex                   m_updateMetaLock;

    mutable Mutex                   m_flushLock;
    bool                            m_flushLockWait;

    /* Thread */
    sp<mainCameraThread>            m_mainPreviewThread;
    bool                            m_mainPreviewThreadFunc(void);

    sp<mainCameraThread>            m_mainCaptureThread;
    bool                            m_mainCaptureThreadFunc(void);

#ifdef USE_DUAL_CAMERA
    bool                            m_flagFinishDualFrameFactoryStartThread;
    sp<mainCameraThread>            m_dualFrameFactoryStartThread; /* master or slave frameFactoryStartThread */
    bool                            m_dualFrameFactoryStartThreadFunc(void);
    sp<mainCameraThread>            m_slaveMainThread;
    bool                            m_slaveMainThreadFunc(void);
    sp<mainCameraThread>            m_dualStandbyThread;
    bool                            m_dualStandbyThreadFunc(void);
#endif

    sp<ExynosCameraStreamThread>            m_previewStreamBayerThread;

    sp<ExynosCameraStreamThread>    m_previewStream3AAThread;

    sp<ExynosCameraStreamThread>            m_previewStreamGMVThread;
    bool                            m_previewStreamGMVPipeThreadFunc(void);

    sp<ExynosCameraStreamThread>            m_previewStreamISPThread;

    sp<ExynosCameraStreamThread>            m_previewStreamMCSCThread;

    sp<ExynosCameraStreamThread>            m_previewStreamGDCThread;

    sp<ExynosCameraStreamThread>    m_previewStreamVDISThread;

    sp<mainCameraThread>            m_previewStreamVRAThread;
    bool                            m_previewStreamVRAPipeThreadFunc(void);

#ifdef SUPPORT_HFD
    sp<mainCameraThread>            m_previewStreamHFDThread;
    bool                            m_previewStreamHFDPipeThreadFunc(void);
#endif

    sp<mainCameraThread>            m_previewStreamPPPipeThread;
    bool                            m_previewStreamPPPipeThreadFunc(void);

#ifdef USE_DUAL_CAMERA
    sp<mainCameraThread>            m_previewStreamSyncThread;
    bool                            m_previewStreamSyncPipeThreadFunc(void);

    sp<mainCameraThread>            m_previewStreamFusionThread;
    bool                            m_previewStreamFusionPipeThreadFunc(void);

    sp<mainCameraThread>            m_selectDualSlaveBayerThread;
    bool                            m_selectDualSlaveBayerThreadFunc(void);
#endif

    sp<mainCameraThread>            m_selectBayerThread;
    status_t                        m_selectBayerHandler(uint32_t pipeID, ExynosCameraFrameSP_sptr_t frame, ExynosCameraBuffer *bayerBuffer,ExynosCameraFrameSP_sptr_t bayerFrame, ExynosCameraFrameFactory *factory);
    bool                            m_selectBayerThreadFunc(void);
    bool                            m_selectBayer(ExynosCameraFrameSP_sptr_t frame);

    sp<mainCameraThread>            m_bayerStreamThread;
    bool                            m_bayerStreamThreadFunc(void);

    sp<mainCameraThread>            m_captureThread;
    bool                            m_captureThreadFunc(void);

    sp<mainCameraThread>            m_captureStreamThread;
    bool                            m_captureStreamThreadFunc(void);

    sp<mainCameraThread>            m_setBuffersThread;
    bool                            m_setBuffersThreadFunc(void);

    sp<mainCameraThread>            m_deinitBufferSupplierThread;
    bool                            m_deinitBufferSupplierThreadFunc(void);

    sp<mainCameraThread>            m_framefactoryCreateThread;
    bool                            m_frameFactoryCreateThreadFunc(void);
    int                             m_framefactoryCreateResult;

    sp<mainCameraThread>            m_reprocessingFrameFactoryStartThread;
    bool                            m_reprocessingFrameFactoryStartThreadFunc(void);

    sp<mainCameraThread>            m_startPictureBufferThread;
    bool                            m_startPictureBufferThreadFunc(void);

    sp<mainCameraThread>            m_frameFactoryStartThread;
    bool                            m_frameFactoryStartThreadFunc(void);

#if defined(USE_RAW_REVERSE_PROCESSING) && defined(USE_SW_RAW_REVERSE_PROCESSING)
    frame_queue_t                   *m_reverseProcessingBayerQ;
    sp<mainCameraThread>            m_reverseProcessingBayerThread;
    bool                            m_reverseProcessingBayerThreadFunc(void);
#endif

#ifdef SUPPORT_HW_GDC
    frame_queue_t                   *m_gdcQ;
    sp<mainCameraThread>            m_gdcThread;
    bool                            m_gdcThreadFunc(void);
#endif

    sp<mainCameraThread>            m_thumbnailCbThread;
    bool                            m_thumbnailCbThreadFunc(void);
    frame_queue_t                   *m_thumbnailCbQ;
    frame_queue_t                   *m_thumbnailPostCbQ;

    frame_queue_t                   *m_resizeDoneQ;

    status_t                        m_setSetfile(void);
    status_t                        m_setupPipeline(ExynosCameraFrameFactory *factory);
    status_t                        m_setupVisionPipeline(void);
    status_t                        m_setupReprocessingPipeline(ExynosCameraFrameFactory *factory);

    sp<mainCameraThread>            m_monitorThread;
    bool                            m_monitorThreadFunc(void);

#ifdef BUFFER_DUMP
    buffer_dump_info_queue_t        *m_dumpBufferQ;
    sp<mainCameraThread>            m_dumpThread;
    bool                            m_dumpThreadFunc(void);
#endif
    status_t                        m_setReprocessingBuffer(void);

    unsigned int                    m_longExposureRemainCount;
    bool                            m_stopLongExposure;
    uint64_t                        m_preLongExposureTime;

    ExynosCameraBuffer              m_newLongExposureCaptureBuffer;

    int                             m_ionClient;

    bool                            m_checkFirstFrameLux;
    void                            m_checkEntranceLux(struct camera2_shot_ext *meta_shot_ext);
    status_t                        m_fastenAeStable(ExynosCameraFrameFactory *factory);
    status_t                        m_checkRestartStream(ExynosCameraRequestSP_sprt_t request);
    status_t                        m_restartStreamInternal();
    int                             m_getMcscLeaderPipeId(ExynosCameraFrameFactory *factory);
#ifdef SUPPORT_DEPTH_MAP
    status_t                        m_setDepthInternalBuffer();
#endif

#define PP_SCENARIO_NONE 0
    status_t                        m_setupPreviewGSC(ExynosCameraFrameSP_sptr_t frame,
                                                                            ExynosCameraRequestSP_sprt_t request,
                                                                            int pipeId, int subPipeId,
                                                                            int pp_scenario = PP_SCENARIO_NONE);

void                                m_copyPreviewCbThreadFunc(ExynosCameraRequestSP_sprt_t request,
                                                            ExynosCameraFrameSP_sptr_t frame,
                                                            ExynosCameraBuffer *buffer);

    ExynosCameraPP                  *m_previewCallbackPP;
    status_t                        m_copyPreview2Callback(ExynosCameraFrameSP_sptr_t frame, ExynosCameraBuffer *srcBuffer, ExynosCameraBuffer *dstBuffer);

#ifdef USE_DUAL_CAMERA
    status_t                        m_updateSizeBeforeDualFusion(ExynosCameraFrameSP_sptr_t frame, int pipeId);
#endif

    mutable Mutex                   m_frameCountLock;

    /* HACK : To prevent newly added member variable corruption
       (May caused by compiler bug??) */
    int                             m_currentMultiCaptureMode;
    int                             m_lastMultiCaptureServiceRequest;
    int                             m_lastMultiCaptureSkipRequest;
    int                             m_lastMultiCaptureNormalRequest;
    int                             m_doneMultiCaptureRequest;
    ExynosCameraDurationTimer       m_firstPreviewTimer;
    bool                            m_flagFirstPreviewTimerOn;

    ExynosCameraDurationTimer       m_previewDurationTimer;
    int                             m_previewDurationTime;
    uint32_t                        m_captureResultToggle;
    uint32_t                        m_displayPreviewToggle;
    int                             m_burstFps_history[4];

    uint32_t                        m_needDynamicBayerCount;

#ifdef SUPPORT_DEPTH_MAP
    bool                            m_flagUseInternalDepthMap;
#endif
#ifdef FPS_CHECK
    int32_t                         m_debugFpsCount[MAX_PIPE_NUM];
    ExynosCameraDurationTimer       m_debugFpsTimer[MAX_PIPE_NUM];
#endif
    struct camera2_stats_dm         m_stats;
    bool                            m_flagUseInternalyuvStall;
    bool                            m_flagVideoStreamPriority;
/********************************************************************************/
/**                          VENDOR                                            **/
/********************************************************************************/

private:
    void                            m_vendorSpecificPreConstructorInitalize(int cameraId, int scenario);
    void                            m_vendorSpecificConstructorInitalize(void);
    void                            m_vendorSpecificPreDestructorDeinitalize(void);
    void                            m_vendorSpecificDestructorDeinitalize(void);
    void                            m_vendorUpdateExposureTime(struct camera2_shot_ext *shot_ext);
	status_t                        setParameters_vendor(const CameraParameters& params);
    status_t                        m_checkMultiCaptureMode_vendor_update(ExynosCameraRequestSP_sprt_t request);
    status_t                        processCaptureRequest_vendor_initDualSolutionZoom(camera3_capture_request *request, status_t& ret);
    status_t						processCaptureRequest_vendor_initDualSolutionPortrait(camera3_capture_request *request, status_t& ret);
	status_t                        m_captureFrameHandler_vendor_updateConfigMode(ExynosCameraRequestSP_sprt_t request, ExynosCameraFrameFactory *targetfactory, frame_type_t& frameType);
    status_t                        m_captureFrameHandler_vendor_updateDualROI(ExynosCameraRequestSP_sprt_t request, frame_handle_components_t& components, frame_type_t frameType);
	status_t                        m_previewFrameHandler_vendor_updateRequest(ExynosCameraFrameFactory *targetfactory);

	status_t                        m_handlePreviewFrame_vendor_handleBuffer(ExynosCameraFrameSP_sptr_t frame, int pipeId, ExynosCameraFrameFactory *factory, frame_handle_components_t& components, status_t& ret);

    status_t                        m_checkStreamInfo_vendor(status_t &ret);

#ifdef USE_SLSI_PLUGIN
    status_t                        m_configurePlugInMode(bool &dynamicBayer);
    status_t                        m_prepareCaptureMode(ExynosCameraRequestSP_sprt_t request, frame_handle_components_t& components);
    status_t                        m_prepareCapturePlugin(ExynosCameraFrameFactory *targetfactory, list<int> *scenarioList);
    status_t                        m_setupCapturePlugIn(ExynosCameraFrameSP_sptr_t frame,
                                                         int pipeId, int subPipeId, int pipeId_next);
    status_t                        m_handleCaptureFramePlugin(ExynosCameraFrameSP_sptr_t frame,
                                                            int leaderPipeId,
                                                            int &pipeId_next,
                                                            int &nodePipeId);
#endif


private:
#ifdef USES_SW_VDIS
    ExynosCameraSolutionSWVdis*      m_exCameraSolutionSWVdis;
#endif
};

}; /* namespace android */
#endif
