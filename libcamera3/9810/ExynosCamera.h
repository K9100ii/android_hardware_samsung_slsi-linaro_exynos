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

#ifdef USE_DUAL_CAMERA
#include "ExynosCameraFrameFactoryPreviewDual.h"
#include "ExynosCameraFrameReprocessingFactoryDual.h"
#endif

#ifdef SAMSUNG_UNIPLUGIN
#include "uni_plugin_wrapper.h"
#endif
#ifdef SAMSUNG_SENSOR_LISTENER
#include "sensor_listener_wrapper.h"
#endif
#ifdef SAMSUNG_UNI_API
#include "uni_api_wrapper.h"
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

class ExynosCamera {
public:
    ExynosCamera() {};
    ExynosCamera(int cameraId, camera_metadata_t **info, uint32_t numOfSensors = 1);
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
    bool        getAvailablePIPMode(void);
    status_t    setDualMode(bool enabled);
    bool        getDualMode(void);

    /** Print out debugging state for the camera device */
    void        dump(void);

    /** Stop */
    status_t    releaseDevice(void);

    void        release();

    /* Common functions */
    int         getCameraId() const;
    int         getCameraIdOrigin() const;

    /* For Vendor */
    status_t    setParameters(const CameraParameters& params);

#ifdef DEBUG_IQ_OSD
    void        printOSD(char *Y, char *UV, struct camera2_shot_ext *meta_shot_ext);
#endif

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
    void        m_vendorSpecificPreConstructor(int cameraId);/* Vendor */
    void        m_vendorSpecificConstructor(void);    /* Vendor */
    void        m_vendorSpecificPreDestructor(void);  /* Vendor */
    void        m_vendorSpecificDestructor(void);     /* Vendor */
    void        m_vendorCreateThreads(void);         /* Vendor */

    /* Helper functions for notification */
    status_t    m_sendJpegStreamResult(ExynosCameraRequestSP_sprt_t request, ExynosCameraBuffer *buffer, int size);
    status_t    m_sendRawStreamResult(ExynosCameraRequestSP_sprt_t request, ExynosCameraBuffer *buffer);
    status_t    m_sendVisionStreamResult(ExynosCameraRequestSP_sprt_t request, ExynosCameraBuffer *buffer);
    status_t    m_sendZslStreamResult(ExynosCameraRequestSP_sprt_t request, ExynosCameraBuffer *buffer);
#ifdef SUPPORT_DEPTH_MAP
    status_t    m_sendDepthStreamResult(ExynosCameraRequestSP_sprt_t request, ExynosCameraBuffer *buffer, int streamId);
#endif
    status_t    m_sendYuvStreamResult(ExynosCameraRequestSP_sprt_t request, ExynosCameraBuffer *buffer, int streamId);
    status_t    m_sendYuvStreamStallResult(ExynosCameraRequestSP_sprt_t request, ExynosCameraBuffer *buffer, int streamId);
    status_t    m_sendThumbnailStreamResult(ExynosCameraRequestSP_sprt_t request, ExynosCameraBuffer *buffer, int streamId);
    status_t    m_sendForceYuvStreamResult(ExynosCameraRequestSP_sprt_t request, ExynosCameraFrameSP_sptr_t frame);
    status_t    m_sendForceStallStreamResult(ExynosCameraRequestSP_sprt_t request);
    status_t    m_sendMeta(ExynosCameraRequestSP_sprt_t request, EXYNOS_REQUEST_RESULT::TYPE type);
    status_t    m_sendPartialMeta(ExynosCameraRequestSP_sprt_t request, EXYNOS_REQUEST_RESULT::TYPE type);
    status_t    m_sendNotifyShutter(ExynosCameraRequestSP_sprt_t request, uint64_t timeStamp = 0);
    status_t    m_sendNotifyError(ExynosCameraRequestSP_sprt_t request, EXYNOS_REQUEST_RESULT::ERROR error, camera3_stream_t *stream = NULL);

    /* Helper functions of Buffer operation */
    status_t    m_createIonAllocator(ExynosCameraIonAllocator **allocator);

    status_t    m_setBuffers(void);
    status_t    m_setVisionBuffers(void);
    status_t    m_setVendorBuffers();

    status_t    m_setupPreviewFactoryBuffers(const ExynosCameraRequestSP_sprt_t request, ExynosCameraFrameSP_sptr_t frame);
    status_t    m_setupBatchFactoryBuffers(ExynosCameraRequestSP_sprt_t request, ExynosCameraFrameSP_sptr_t frame);
    status_t    m_setupVisionFactoryBuffers(const ExynosCameraRequestSP_sprt_t request, ExynosCameraFrameSP_sptr_t frame);
    status_t    m_setupCaptureFactoryBuffers(const ExynosCameraRequestSP_sprt_t request, ExynosCameraFrameSP_sptr_t frame);

    status_t    m_allocBuffers(
                    const buffer_manager_tag_t tag,
                    const buffer_manager_configuration_t info);

    /* helper functions for set buffer to frame */
    status_t    m_setupEntity(uint32_t pipeId, ExynosCameraFrameSP_sptr_t newFrame,
                                ExynosCameraBuffer *srcBuf = NULL,
                                ExynosCameraBuffer *dstBuf = NULL,
                                int dstNodeIndex = 0,
                                int dstPipeId = -1);
    status_t    m_setSrcBuffer(uint32_t pipeId, ExynosCameraFrameSP_sptr_t newFrame, ExynosCameraBuffer *buffer);
    status_t    m_setDstBuffer(uint32_t pipeId, ExynosCameraFrameSP_sptr_t newFrame, ExynosCameraBuffer *buffer, int nodeIndex = 0, int dstPipeId = -1);

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
    status_t    m_createInternalFrameFunc(ExynosCameraRequestSP_sprt_t request, enum Request_Sync_Type syncType, frame_type_t frameType = FRAME_TYPE_INTERNAL);
    status_t    m_createPreviewFrameFunc(enum Request_Sync_Type syncType);
    status_t    m_createVisionFrameFunc(enum Request_Sync_Type syncType);
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
    void        m_updateFD(struct camera2_shot_ext *shot_ext, enum facedetect_mode fdMode, int dsInputPortId, bool isReprocessing);
    void        m_updateEdgeNoiseMode(struct camera2_shot_ext *shot_ext, bool isCaptureFrame);
    void        m_updateExposureTime(struct camera2_shot_ext *shot_ext);
    void        m_updateSetfile(struct camera2_shot_ext *shot_ext, bool isCaptureFrame);

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
                                        ExynosCameraRequestSP_sprt_t request = NULL);
#ifdef USE_DUAL_CAMERA
    status_t    m_generateTransitionFrame(ExynosCameraFrameFactory *factory,
                                          List<ExynosCameraFrameSP_sptr_t> *list,
                                          Mutex *listLock,
                                          ExynosCameraFrameSP_dptr_t newFrame);
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
                                    ExynosCameraRequestSP_sprt_t request);
    status_t    m_checkStreamBufferStatus(ExynosCameraRequestSP_sprt_t request,
                                          ExynosCameraStream *stream, int *status,
                                          bool bufferSkip = false);
    void        m_checkUpdateResult(ExynosCameraFrameSP_sptr_t frame, uint32_t streamConfigBit);
    status_t    m_updateTimestamp(ExynosCameraFrameSP_sptr_t frame, ExynosCameraBuffer *timestampBuffer);
    status_t    m_storePreviewStatsInfo(struct camera2_shot_ext *src_ext);
    status_t    m_getPreviewFDInfo(struct camera2_shot_ext *dst_ext);
    status_t    m_handlePreviewFrame(ExynosCameraFrameSP_sptr_t frame, int pipeId);
    status_t    m_handleVisionFrame(ExynosCameraFrameSP_sptr_t frame, int pipeId);
    status_t    m_handleInternalFrame(ExynosCameraFrameSP_sptr_t frame, int pipeId);
    status_t    m_handleYuvCaptureFrame(ExynosCameraFrameSP_sptr_t frame);
    status_t    m_handleNV21CaptureFrame(ExynosCameraFrameSP_sptr_t frame, int leaderPipeId);
    status_t    m_resizeToDScaledYuvStall(ExynosCameraFrameSP_sptr_t frame, int leaderPipeId, int nodePipeId);
    status_t    m_handleJpegFrame(ExynosCameraFrameSP_sptr_t frame, int leaderPipeId);
    status_t    m_handleBayerBuffer(ExynosCameraFrameSP_sptr_t frame, int pipeId);
#ifdef SUPPORT_DEPTH_MAP
    status_t    m_handleDepthBuffer(ExynosCameraFrameSP_sptr_t frame, ExynosCameraRequestSP_sprt_t request);
#endif
#ifdef USE_DUAL_CAMERA
    status_t    m_checkDualOperationMode(ExynosCameraRequestSP_sprt_t request);
    status_t    m_setStandbyMode(bool standby);
#endif

    /* helper functions for request */
    status_t    m_pushServiceRequest(camera3_capture_request *request, ExynosCameraRequestSP_dptr_t req);
    status_t    m_popServiceRequest(ExynosCameraRequestSP_dptr_t request);
    status_t    m_pushRunningRequest(ExynosCameraRequestSP_dptr_t request_in);
    status_t    m_updateResultShot(ExynosCameraRequestSP_sprt_t request, struct camera2_shot_ext *src_ext,
                                   enum metadata_type metaType = PARTIAL_NONE);
    status_t    m_updateJpegPartialResultShot(ExynosCameraRequestSP_sprt_t request);
    status_t    m_setFactoryAddr(ExynosCameraRequestSP_dptr_t request);

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
    status_t    m_getBayerBuffer(uint32_t pipeId,
                                 uint32_t frameCount,
                                 ExynosCameraBuffer *buffer,
                                 ExynosCameraFrameSelector *selector
#ifdef SUPPORT_DEPTH_MAP
                                 , ExynosCameraBuffer *depthMapBuffer = NULL
#endif
                                            );
    status_t    m_checkBufferAvailable(uint32_t pipeId);

    status_t                m_transitState(exynos_camera_state_t state);
    exynos_camera_state_t   m_getState(void);
    bool                    m_isSkipBurstCaptureBuffer();
	/* debuger functions */
#ifdef FPS_CHECK
	void		m_debugFpsCheck(enum pipeline pipeId);
#endif

private:
    ExynosCameraRequestManager      *m_requestMgr;
    ExynosCameraMetadataConverter   *m_metadataConverter;
    ExynosCameraParameters          *m_parameters[CAMERA_ID_MAX];
    ExynosCameraStreamManager       *m_streamManager;
    ExynosCameraFrameManager        *m_frameMgr;
    ExynosCameraIonAllocator        *m_ionAllocator;
    ExynosCameraBufferSupplier      *m_bufferSupplier;
    ExynosCameraActivityControl     *m_activityControl;

    ExynosCameraFrameFactory        *m_frameFactory[FRAME_FACTORY_TYPE_MAX];
    framefactory3_queue_t           *m_frameFactoryQ;

    ExynosCameraFrameSelector       *m_captureSelector[CAMERA_ID_MAX];
    ExynosCameraFrameSelector       *m_captureZslSelector;

private:
#ifdef USE_DUAL_CAMERA
    uint32_t                        m_cameraIds[2];
#else
    uint32_t                        m_cameraIds[1];
#endif
    uint32_t                        m_cameraId;
    uint32_t                        m_cameraOriginId;
    uint32_t                        m_scenario;
    char                            m_name[EXYNOS_CAMERA_NAME_STR_SIZE];
    mutable Condition               m_captureResultDoneCondition;
    mutable Mutex                   m_captureResultDoneLock;
    uint64_t                        m_lastFrametime;

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
    frame_queue_t                   *m_captureQ;
    frame_queue_t                   *m_yuvCaptureDoneQ;
    frame_queue_t                   *m_reprocessingDoneQ;

    ExynosCameraList<uint32_t>      *m_shotDoneQ;
    status_t                        m_waitShotDone(enum Request_Sync_Type syncType);
#ifdef USE_DUAL_CAMERA
    ExynosCameraList<uint32_t>      *m_dualShotDoneQ;
    uint32_t                        m_dualTransitionCount;
#endif
    List<ExynosCameraRequestSP_sprt_t>  m_requestPreviewWaitingList;
    List<ExynosCameraRequestSP_sprt_t>  m_requestCaptureWaitingList;
    mutable Mutex                   m_requestPreviewWaitingLock;
    mutable Mutex                   m_requestCaptureWaitingLock;

    uint32_t                        m_firstRequestFrameNumber;
    int                             m_internalFrameCount;
    bool                            m_isNeedRequestFrame;

    // TODO: Temporal. Efficient implementation is required.
    mutable Mutex               m_updateMetaLock;

    mutable Mutex                   m_flushLock;
    bool                            m_flushLockWait;

    /* Thread */
    sp<mainCameraThread>            m_mainPreviewThread;
    bool                            m_mainPreviewThreadFunc(void);

    sp<mainCameraThread>            m_mainCaptureThread;
    bool                            m_mainCaptureThreadFunc(void);

#ifdef USE_DUAL_CAMERA
    sp<mainCameraThread>            m_dualMainThread;
    bool                            m_dualMainThreadFunc(void);
#endif

    sp<mainCameraThread>            m_previewStreamBayerThread;
    bool                            m_previewStreamBayerPipeThreadFunc(void);

    sp<mainCameraThread>            m_previewStream3AAThread;
    bool                            m_previewStream3AAPipeThreadFunc(void);

    sp<mainCameraThread>            m_previewStreamISPThread;
    bool                            m_previewStreamISPPipeThreadFunc(void);

    sp<mainCameraThread>            m_previewStreamMCSCThread;
    bool                            m_previewStreamMCSCPipeThreadFunc(void);

    sp<mainCameraThread>            m_previewStreamGDCThread;
    bool                            m_previewStreamGDCPipeThreadFunc(void);

    sp<mainCameraThread>            m_previewStreamVRAThread;
    bool                            m_previewStreamVRAPipeThreadFunc(void);

#ifdef SUPPORT_HFD
    sp<mainCameraThread>            m_previewStreamHFDThread;
    bool                            m_previewStreamHFDPipeThreadFunc(void);
#endif

    sp<mainCameraThread>            m_previewStreamPPPipeThread;
    bool                            m_previewStreamPPPipeThreadFunc(void);

#ifdef USE_DUAL_CAMERA
    sp<mainCameraThread>            m_previewStreamDCPThread;
    bool                            m_previewStreamDCPPipeThreadFunc(void);

    sp<mainCameraThread>            m_previewStreamSyncThread;
    bool                            m_previewStreamSyncPipeThreadFunc(void);

    sp<mainCameraThread>            m_previewStreamFusionThread;
    bool                            m_previewStreamFusionPipeThreadFunc(void);
#endif

#ifdef SAMSUNG_TN_FEATURE
    sp<mainCameraThread>            m_previewStreamPPPipe2Thread;
    bool                            m_previewStreamPPPipe2ThreadFunc(void);
#ifdef SAMSUNG_SW_VDIS
    sp<mainCameraThread>            m_SWVdisPreviewCbThread;
    bool                            m_SWVdisPreviewCbThreadFunc();
#endif
#endif

    sp<mainCameraThread>            m_selectBayerThread;
    bool                            m_selectBayerThreadFunc(void);

    sp<mainCameraThread>            m_captureThread;
    bool                            m_captureThreadFunc(void);

    sp<mainCameraThread>            m_captureStreamThread;
    bool                            m_captureStreamThreadFunc(void);

    sp<mainCameraThread>            m_setBuffersThread;
    bool                            m_setBuffersThreadFunc(void);

    sp<mainCameraThread>            m_framefactoryCreateThread;
    bool                            m_frameFactoryCreateThreadFunc(void);

    sp<mainCameraThread>            m_reprocessingFrameFactoryStartThread;
    bool                            m_reprocessingFrameFactoryStartThreadFunc(void);

    sp<mainCameraThread>            m_startPictureBufferThread;
    bool                            m_startPictureBufferThreadFunc(void);

    sp<mainCameraThread>            m_frameFactoryStartThread;
    bool                            m_frameFactoryStartThreadFunc(void);

#ifdef SUPPORT_HW_GDC
    frame_queue_t                   *m_gdcQ;
    sp<mainCameraThread>            m_gdcThread;
    bool                            m_gdcThreadFunc(void);
#endif

#ifdef CAMERA_FAST_ENTRANCE_V1
    sp<mainCameraThread>            m_fastenAeThread;
    bool                            m_fastenAeThreadFunc(void);
    status_t                        m_waitFastenAeThreadEnd(void);
#endif

    sp<mainCameraThread>            m_thumbnailCbThread;
    bool                            m_thumbnailCbThreadFunc(void);
    frame_queue_t                   *m_thumbnailCbQ;
    frame_queue_t                   *m_thumbnailPostCbQ;

    frame_queue_t                   *m_resizeDoneQ;

#ifdef SAMSUNG_TN_FEATURE
    sp<mainCameraThread>            m_dscaledYuvStallPostProcessingThread;
    bool                            m_dscaledYuvStallPostProcessingThreadFunc(void);
    frame_queue_t                   *m_dscaledYuvStallPPCbQ;
    frame_queue_t                   *m_dscaledYuvStallPPPostCbQ;
#endif

    status_t                        m_setSetfile(void);
    status_t                        m_setupPipeline(ExynosCameraFrameFactory *factory);
    status_t                        m_setupVisionPipeline(void);
    status_t                        m_setupReprocessingPipeline(ExynosCameraFrameFactory *factory);

    sp<mainCameraThread>            m_monitorThread;
    bool                            m_monitorThreadFunc(void);

#ifdef YUV_DUMP
    frame_queue_t                   *m_dumpFrameQ;
    sp<mainCameraThread>            m_dumpThread;
    bool                            m_dumpThreadFunc(void);
#endif
#ifdef LLS_CAPTURE
    int                             m_needLLS_history[LLS_HISTORY_COUNT];
#endif
    status_t                        m_setReprocessingBuffer(void);

#ifdef SAMSUNG_READ_ROM
    sp<mainCameraThread>            m_readRomThread;
    bool                            m_readRomThreadFunc(void);
    status_t                        m_waitReadRomThreadEnd(void);
#endif

#ifdef SAMSUNG_LENS_DC
    void                            *m_DCpluginHandle;
    bool                            m_skipLensDC;
    int                             m_LensDCIndex;
    void                            m_setLensDCMap(void);
#endif

#ifdef SAMSUNG_TN_FEATURE
    bool                            m_pipePPPreviewStart[MAX_PIPE_UNI_NUM];
    bool                            m_pipePPReprocessingStart[MAX_PIPE_UNI_NUM];
#endif

#ifdef SAMSUNG_HLV
    void                            *m_HLV;
    int                             m_HLVprocessStep;
#endif
#ifdef SAMSUNG_LLS_DEBLUR
    ExynosCameraBuffer              m_LDBuf[MAX_LD_CAPTURE_COUNT];
#endif

    unsigned int                    m_longExposureRemainCount;
    bool                            m_stopLongExposure;
    uint64_t                        m_preLongExposureTime;

    ExynosCameraBuffer              m_newLongExposureCaptureBuffer;

    int                             m_ionClient;

#ifdef CAMERA_FAST_ENTRANCE_V1
    bool                            m_isFirstParametersSet;
    bool                            m_fastEntrance;
    int                             m_fastenAeThreadResult;
#endif
#ifdef SAMSUNG_SENSOR_LISTENER
    sp<mainCameraThread>            m_sensorListenerThread;
    bool                            m_sensorListenerThreadFunc(void);
    bool                            m_getSensorListenerData();
#ifdef SAMSUNG_HRM
    void*                           m_uv_rayHandle;
    SensorListenerEvent_t           m_uv_rayListenerData;
#endif
#ifdef SAMSUNG_LIGHT_IR
    void*                           m_light_irHandle;
    SensorListenerEvent_t           m_light_irListenerData;
#endif
#ifdef SAMSUNG_GYRO
    void*                           m_gyroHandle;
    SensorListenerEvent_t           m_gyroListenerData;
#endif
#ifdef SAMSUNG_ACCELEROMETER
    void*                           m_accelerometerHandle;
    SensorListenerEvent_t           m_accelerometerListenerData;
#endif
#ifdef SAMSUNG_ROTATION
    void*                           m_rotationHandle;
    SensorListenerEvent_t           m_rotationListenerData;
#endif
#endif
    status_t                        m_fastenAeStable(ExynosCameraFrameFactory *factory);
    status_t                        m_checkRestartStream(ExynosCameraRequestSP_sprt_t request);
    status_t                        m_restartStreamInternal();
#ifdef SAMSUNG_TN_FEATURE
    int                             m_getPortPreviewUniPP(ExynosCameraRequestSP_sprt_t request, int pp_scenario);
    status_t                        m_setupPreviewUniPP(ExynosCameraFrameSP_sptr_t frame,
                                                        ExynosCameraRequestSP_sprt_t request,
                                                        int pipeId, int subPipeId, int pipeId_next);
    status_t                        m_setupCaptureUniPP(ExynosCameraFrameSP_sptr_t frame,
                                                        int pipeId, int subPipeId, int pipeId_next);
    int                             m_connectPreviewUniPP(ExynosCameraFrameFactory *targetfactory,
                                                          ExynosCameraRequestSP_sprt_t request);
    int                             m_connectCaptureUniPP(ExynosCameraFrameFactory *targetfactory);
#endif
#ifdef SAMSUNG_FOCUS_PEAKING
    status_t                        m_setFocusPeakingBuffer();
#endif
#ifdef SAMSUNG_SW_VDIS
    status_t                        m_setupSWVdisPreviewGSC(ExynosCameraFrameSP_sptr_t frame,
                                                                            ExynosCameraRequestSP_sprt_t request,
                                                                            int pipeId, int subPipeId);
#endif

    /* HACK : To prevent newly added member variable corruption
       (May caused by compiler bug??) */
    int                             m_currentMultiCaptureMode;
    int                             m_lastMultiCaptureRequestCount;
    int                             m_doneMultiCaptureRequestCount;
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
#ifdef SAMSUNG_DOF
    bool                            m_isFocusSet;
#endif
#ifdef DEBUG_IQ_OSD
    int                             m_isOSDMode;
#endif
#ifdef FPS_CHECK
    int32_t                         m_debugFpsCount[MAX_PIPE_NUM];
    ExynosCameraDurationTimer       m_debugFpsTimer[MAX_PIPE_NUM];
#endif
    struct camera2_stats_dm        m_stats;
};

}; /* namespace android */
#endif
