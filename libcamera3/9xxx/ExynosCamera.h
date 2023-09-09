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

#include "ExynosCameraTuningInterface.h"

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

#ifdef SAMSUNG_DUAL_PORTRAIT_SEF
#include "libraries/sec/SEFAPIs.h"
#include "libraries/sec/SEFFileTypes.h"
#include "libraries/sec/SEFKeyName.h"
#endif

#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
#include "ExynosCameraBokehInclude.h"
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
#ifdef SAMSUNG_FACTORY_DRAM_TEST
typedef ExynosCameraList<ExynosCameraBuffer> vcbuffer_queue_t;
#endif

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
    int currentCameraId;
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

#ifdef DEBUG_IQ_OSD
    void        printOSD(char *Y, char *UV, int yuvSizeW, int yuvSizeH, struct camera2_shot_ext *meta_shot_ext);
#endif

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
                                                    int streamId, bool skipBuffer, uint64_t streamTimeStamp,
                                                    ExynosCameraParameters *params
#ifdef SAMSUNG_SSM
                                                    , bool ssmframeFlag = false
#endif
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
    status_t    m_destroyPreviewFrameFactory(void);
    status_t    m_startFrameFactory(ExynosCameraFrameFactory *factory);
    status_t    m_startReprocessingFrameFactory(ExynosCameraFrameFactory *factory);
    status_t    m_stopFrameFactory(ExynosCameraFrameFactory *factory);
    status_t    m_stopReprocessingFrameFactory(ExynosCameraFrameFactory *factory);
    status_t    m_deinitFrameFactory();

    /* frame Generation / Done handler */
    status_t    m_checkMultiCaptureMode(ExynosCameraRequestSP_sprt_t request);
    status_t    m_createInternalFrameFunc(ExynosCameraRequestSP_sprt_t request, bool flagFinishFactoryStart,
                                                        enum Request_Sync_Type syncType,
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

    status_t    m_updateLatestInfoToShot(struct camera2_shot_ext *shot_ext, frame_type_t frameType);
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
    status_t    m_updateYsumValue(ExynosCameraFrameSP_sptr_t frame,
                                  ExynosCameraRequestSP_sprt_t request,
                                  int srcPipeId, NODE_TYPE dstNodeType = OUTPUT_NODE);
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
                                          frame_type_t frameType);
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
                                ExynosCameraBuffer *timestampBuffer, frame_handle_components_t *components);
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
    void        m_checkRequestStreamChanged(char *handlerName, uint32_t currentStreamBit);

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
    mutable Mutex                   m_updateShotInfoLock;

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
    uint32_t                        m_prevStreamBit;

    struct ExynosConfigInfo         *m_exynosconfig;

    struct camera2_shot_ext         *m_currentPreviewShot[CAMERA_ID_MAX];
    struct camera2_shot_ext         *m_currentInternalShot[CAMERA_ID_MAX];
    struct camera2_shot_ext         *m_currentCaptureShot[CAMERA_ID_MAX];
    struct camera2_shot_ext         *m_currentVisionShot[CAMERA_ID_MAX];

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

    enum DUAL_OPERATION_MODE        m_earlyDualOperationMode;

    volatile int32_t                m_needSlaveDynamicBayerCount;
#endif

    List<ExynosCameraRequestSP_sprt_t>  m_latestRequestList;
    mutable Mutex                   m_latestRequestListLock;

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

    sp<mainCameraThread>            m_previewStreamGMVThread;
    bool                            m_previewStreamGMVPipeThreadFunc(void);

    sp<ExynosCameraStreamThread>            m_previewStreamISPThread;

    sp<ExynosCameraStreamThread>            m_previewStreamMCSCThread;

    sp<ExynosCameraStreamThread>            m_previewStreamGDCThread;

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

#ifndef SAMSUNG_UNIPLUGIN
#define PP_SCENARIO_NONE 0
#endif
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

    ExynosCameraDurationTimer       m_previewDurationTimer[2];
    int                             m_previewDurationTime[2];
    uint32_t                        m_captureResultToggle;
    uint32_t                        m_displayPreviewToggle;
    int                             m_burstFps_history[4];

#ifdef SUPPORT_DEPTH_MAP
    bool                            m_flagUseInternalDepthMap;
#endif
#ifdef DEBUG_IQ_OSD
    int                             m_isOSDMode;
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

#ifdef SAMSUNG_TN_FEATURE
    status_t                        m_checkMultiCaptureMode_vendor_captureHint(ExynosCameraRequestSP_sprt_t request, CameraMetadata *meta);
#endif

    status_t                        m_checkMultiCaptureMode_vendor_update(ExynosCameraRequestSP_sprt_t request);
    status_t                        processCaptureRequest_vendor_initDualSolutionZoom(camera3_capture_request *request, status_t& ret);
    status_t						processCaptureRequest_vendor_initDualSolutionPortrait(camera3_capture_request *request, status_t& ret);

#ifdef SAMSUNG_TN_FEATURE
    bool                            m_isSkipBurstCaptureBuffer_vendor(frame_type_t frameType, int& isSkipBuffer, int& ratio, int& runtime_fps, int& burstshotTargetFps);
#endif

	status_t                        m_captureFrameHandler_vendor_updateConfigMode(ExynosCameraRequestSP_sprt_t request, ExynosCameraFrameFactory *targetfactory, frame_type_t& frameType);

#ifdef SAMSUNG_TN_FEATURE
	status_t                        m_captureFrameHandler_vendor_updateCaptureMode(ExynosCameraRequestSP_sprt_t request, frame_handle_components_t& components, ExynosCameraActivitySpecialCapture *sCaptureMgr);
#endif

    status_t                        m_captureFrameHandler_vendor_updateDualROI(ExynosCameraRequestSP_sprt_t request, frame_handle_components_t& components, frame_type_t frameType);

#ifdef OIS_CAPTURE
    status_t                        m_captureFrameHandler_vendor_updateIntent(ExynosCameraRequestSP_sprt_t request, frame_handle_components_t& components);
#endif

#ifdef SAMSUNG_SW_VDIS
    status_t                        configureStreams_vendor_updateVDIS();
#endif

	status_t                        m_previewFrameHandler_vendor_updateRequest(ExynosCameraFrameFactory *targetfactory);

	status_t                        m_handlePreviewFrame_vendor_handleBuffer(ExynosCameraFrameSP_sptr_t frame, int pipeId, ExynosCameraFrameFactory *factory, frame_handle_components_t& components, status_t& ret);


    status_t                        m_checkStreamInfo_vendor(status_t &ret);

#ifdef SAMSUNG_TN_FEATURE
    void                            m_updateTemperatureInfo(struct camera2_shot_ext *shot_ext);
#endif

#ifdef SAMSUNG_RTHDR
    void                            m_updateWdrMode(struct camera2_shot_ext *shot_ext, bool isCaptureFrame);
#endif

#if defined (SAMSUNG_DUAL_ZOOM_PREVIEW) || defined (SAMSUNG_DUAL_PORTRAIT_SOLUTION)
    status_t                        m_updateBeforeForceSwitchSolution(ExynosCameraFrameSP_sptr_t frame, int pipeId);
    status_t                        m_updateAfterForceSwitchSolution(ExynosCameraFrameSP_sptr_t frame);
    status_t                        m_updateBeforeDualSolution(ExynosCameraFrameSP_sptr_t frame, int pipeId);
    status_t                        m_updateAfterDualSolution(ExynosCameraFrameSP_sptr_t frame);
    status_t                        m_readDualCalData(ExynosCameraFusionWrapper *fusionWrapper);
    bool                            m_check2XButtonPressed(float curZoomRatio, float prevZoomRatio);
#endif /* SAMSUNG_DUAL_ZOOM_PREVIEW || SAMSUNG_DUAL_PORTRAIT_SOLUTION */

#ifdef SAMSUNG_DUAL_PORTRAIT_SEF
    status_t                        m_sendSefStreamResult(ExynosCameraRequestSP_sprt_t request,
                                                ExynosCameraFrameSP_sptr_t frame, ExynosCameraBuffer *bokehOuputBuffer);
#endif

#if defined(USE_DUAL_CAMERA) && defined(SAMSUNG_DUAL_ZOOM_PREVIEW)
    void                            m_updateCropRegion_vendor(struct camera2_shot_ext *shot_ext,
                                                int &sensorMaxW, int &sensorMaxH,
                                                ExynosRect &targetActiveZoomRect,
                                                ExynosRect &subActiveZoomRect,
                                                int &subSolutionMargin);
#endif

#ifdef SAMSUNG_HIFI_CAPTURE
void                                m_setupReprocessingPipeline_HifiCapture(ExynosCameraFrameFactory *factory);
#endif

#ifdef SAMSUNG_DOF
void                                m_selectBayer_dof(int shotMode,
                                                    ExynosCameraActivityAutofocus *autoFocusMgr,
                                                    frame_handle_components_t components);
#endif

#ifdef SAMSUNG_TN_FEATURE
bool                                m_selectBayer_tn(ExynosCameraFrameSP_sptr_t frame,
                                                    uint64_t captureExposureTime,
                                                    camera2_shot_ext *shot_ext,
                                                    ExynosCameraBuffer &bayerBuffer,
                                                    ExynosCameraBuffer &depthMapBuffer);
#endif

#ifdef SAMSUNG_DUAL_PORTRAIT_SEF
void                                m_captureStreamThreadFunc_dualPortraitSef(ExynosCameraFrameSP_sptr_t frame,
                                                            frame_handle_components_t &components,
                                                            int &dstPipeId);
#endif

#ifdef SAMSUNG_TN_FEATURE
int                                 m_captureStreamThreadFunc_tn(ExynosCameraFrameSP_sptr_t frame,
                                                            frame_handle_components_t &components,
                                                            ExynosCameraFrameEntity *entity,
                                                            int yuvStallPipeId);

status_t                            m_captureStreamThreadFunc_caseUniProcessing(ExynosCameraFrameSP_sptr_t frame,
                                                            ExynosCameraFrameEntity *entity,
                                                            int currentLDCaptureStep,
                                                            int LDCaptureLastStep);

status_t                            m_captureStreamThreadFunc_caseUniProcessing2(ExynosCameraFrameSP_sptr_t frame,
                                                            ExynosCameraFrameEntity *entity);
#endif

#ifdef SAMSUNG_DUAL_PORTRAIT_SEF
status_t                            m_captureStreamThreadFunc_caseSyncReprocessingDualPortraitSef(ExynosCameraFrameSP_sptr_t frame,
                                                            ExynosCameraFrameEntity *entity,
                                                            frame_handle_components_t &components,
                                                            ExynosCameraRequestSP_sprt_t request);
#endif

#ifdef SAMSUNG_TN_FEATURE
void                                m_handleNV21CaptureFrame_tn(ExynosCameraFrameSP_sptr_t frame,
                                                            int leaderPipeId,
                                                            int &nodePipeId);

bool                                m_handleNV21CaptureFrame_useInternalYuvStall(ExynosCameraFrameSP_sptr_t frame,
                                                            int leaderPipeId,
                                                            ExynosCameraRequestSP_sprt_t request,
                                                            ExynosCameraBuffer &dstBuffer,
                                                            int nodePipeId,
                                                            int streamId,
                                                            status_t &error);

status_t                            m_handleNV21CaptureFrame_unipipe(ExynosCameraFrameSP_sptr_t frame,
                                                            int leaderPipeId,
                                                            int &pipeId_next,
                                                            int &nodePipeId);
#endif

#if defined(SAMSUNG_DUAL_ZOOM_PREVIEW)
void                                m_setBuffers_dualZoomPreview(ExynosRect &previewRect);
#endif

#ifdef SAMSUNG_SW_VDIS
status_t                            m_setupBatchFactoryBuffers_swVdis(ExynosCameraFrameSP_sptr_t frame,
                                                  ExynosCameraFrameFactory *factory,
                                                  int streamId,
                                                  uint32_t &leaderPipeId,
                                                  buffer_manager_tag_t &bufTag);
#endif

#ifdef SAMSUNG_TN_FEATURE
status_t                            m_setReprocessingBuffer_tn(int maxPictureW, int maxPictureH);
#endif

private:
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    bool                            m_2x_btn;
    int                             m_prevRecomCamType;
    float                           m_holdZoomRatio;
#endif

#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    ExynosCameraFusionWrapper       *m_fusionZoomPreviewWrapper;
#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
    ExynosCameraFusionWrapper       *m_fusionZoomCaptureWrapper;
#endif
    void                            *m_previewSolutionHandle;
    void                            *m_captureSolutionHandle;
#endif

#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
    ExynosCameraFusionWrapper       *m_bokehPreviewWrapper;
    ExynosCameraFusionWrapper       *m_bokehCaptureWrapper;
    void                            *m_bokehPreviewSolutionHandle;
    void                            *m_bokehCaptureSolutionHandle;
#endif

#ifdef SAMSUNG_UNIPLUGIN
    void                            *m_uniPlugInHandle[PP_SCENARIO_MAX];
#endif

#ifdef SAMSUNG_TN_FEATURE
    sp<mainCameraThread>            m_previewStreamPPPipe2Thread;
    bool                            m_previewStreamPPPipe2ThreadFunc(void);

    sp<mainCameraThread>            m_previewStreamPPPipe3Thread;
    bool                            m_previewStreamPPPipe3ThreadFunc(void);

    sp<mainCameraThread>            m_gscPreviewCbThread;
    bool                            m_gscPreviewCbThreadFunc();
#endif

#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
    frame_queue_t                   *m_slaveJpegDoneQ;
#endif

#ifdef SAMSUNG_TN_FEATURE
    sp<mainCameraThread>            m_dscaledYuvStallPostProcessingThread;
    bool                            m_dscaledYuvStallPostProcessingThreadFunc(void);
    frame_queue_t                   *m_dscaledYuvStallPPCbQ;
    frame_queue_t                   *m_dscaledYuvStallPPPostCbQ;
#endif

#ifdef SAMSUNG_FACTORY_DRAM_TEST
    int                             m_dramTestQCount;
    int                             m_dramTestDoneCount;
    vcbuffer_queue_t                *m_postVC0Q;
    sp<mainCameraThread>            m_postVC0Thread;
    bool                            m_postVC0ThreadFunc(void);
#endif

#ifdef SAMSUNG_READ_ROM
    sp<mainCameraThread>            m_readRomThread;
    bool                            m_readRomThreadFunc(void);
    status_t                        m_waitReadRomThreadEnd(void);
#endif

#ifdef SAMSUNG_UNIPLUGIN
    sp<mainCameraThread>            m_uniPluginThread;
    bool                            m_uniPluginThreadFunc(void);
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

#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
    ExynosCameraBuffer              m_liveFocusLLSBuf[CAMERA_ID_MAX][MAX_LD_CAPTURE_COUNT];
    ExynosCameraBuffer              m_liveFocusFirstSlaveJpegBuf;
#endif

#ifdef CAMERA_FAST_ENTRANCE_V1
    bool                            m_isFirstParametersSet;
    bool                            m_fastEntrance;
    int                             m_fastenAeThreadResult;
    mutable Mutex                   m_previewFactoryLock;
#endif

#ifdef SAMSUNG_SENSOR_LISTENER
    sp<mainCameraThread>            m_sensorListenerThread;
    bool                            m_sensorListenerThreadFunc(void);
    bool                            m_getSensorListenerData(frame_handle_components_t *components);
    sp<mainCameraThread>            m_sensorListenerUnloadThread;
    bool                            m_sensorListenerUnloadThreadFunc(void);
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
#ifdef SAMSUNG_PROX_FLICKER
    void*                           m_proximityHandle;
    SensorListenerEvent_t           m_proximityListenerData;
#endif
#endif

#ifdef SAMSUNG_TN_FEATURE
    int                             m_getPortPreviewUniPP(ExynosCameraRequestSP_sprt_t request, int pp_scenario);
    status_t                        m_setupPreviewUniPP(ExynosCameraFrameSP_sptr_t frame,
                                                        ExynosCameraRequestSP_sprt_t request,
                                                        int pipeId, int subPipeId, int pipeId_next);
    status_t                        m_setupCaptureUniPP(ExynosCameraFrameSP_sptr_t frame,
                                                        int pipeId, int subPipeId, int pipeId_next);
    int                             m_connectPreviewUniPP(ExynosCameraRequestSP_sprt_t request,
                                                        ExynosCameraFrameFactory *targetfactory);
    int                             m_connectCaptureUniPP(ExynosCameraFrameFactory *targetfactory);
    status_t                        m_initUniPP(void);
    status_t                        m_deinitUniPP(void);
#endif

#if defined(SAMSUNG_HIFI_VIDEO) && defined(HIFIVIDEO_INPUTCOPY_DISABLE)
    buffer_queue_t                  *m_hifiVideoBufferQ;
    uint32_t                        m_hifiVideoBufferCount;
#endif

#ifdef SAMSUNG_SSM
    void                            m_setupSSMMode(ExynosCameraRequestSP_sprt_t request,
                                                            frame_handle_components_t components);
    void                            m_checkSSMState(ExynosCameraRequestSP_sprt_t request,
                                                            struct camera2_shot_ext *shot_ext);
    status_t                        m_SSMProcessing(ExynosCameraRequestSP_sprt_t request,
                                                            ExynosCameraFrameSP_sptr_t frame, int pipeId,
                                                            ExynosCameraBuffer *buffer, int streamId);
    status_t                        m_sendForceSSMResult(ExynosCameraRequestSP_sprt_t request);
    enum SSM_STATE                  m_SSMState;
    int                             m_SSMAutoDelay;
    frame_queue_t                   *m_SSMAutoDelayQ;
    uint64_t                        m_SSMRecordingtime;
    uint64_t                        m_SSMOrgRecordingtime;
    int                             m_SSMSkipToggle;
    int                             m_SSMRecordingToggle;
    buffer_queue_t                  *m_SSMSaveBufferQ;
    ExynosRect2                     m_SSMRegion;
    int                             m_SSMMode;
    int                             m_SSMCommand;
    bool                            m_SSMUseAutoDelayQ;
    int                             m_SSMFirstRecordingRequest;
    ExynosCameraDurationTimer       m_SSMDetectDurationTimer;
    int                             m_SSMDetectDurationTime;
    bool                            m_checkRegister;
#endif

#ifdef SAMSUNG_DOF
    bool                            m_isFocusSet;
#endif

#ifdef SAMSUNG_EVENT_DRIVEN
    int                             m_eventDrivenToggle;
#endif

#ifdef SAMSUNG_HYPERLAPSE_DEBUGLOG
    uint32_t                        m_recordingCallbackCount;
#endif

#ifdef SAMSUNG_HIFI_CAPTURE
    bool                            m_hifiLLSPowerOn;
#endif
};

}; /* namespace android */
#endif
