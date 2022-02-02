/*
**
** Copyright 2013, Samsung Electronics Co. LTD
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

#ifndef EXYNOS_CAMERA_HW_IMPLEMENTATION_H
#define EXYNOS_CAMERA_HW_IMPLEMENTATION_H

#include <media/hardware/HardwareAPI.h>

#include "ExynosCameraDefine.h"
#include "ExynosCamera1Parameters.h"
#include "ExynosCameraConfig.h"

#include "ExynosCameraFrameFactoryPreview.h"
#include "ExynosCameraFrameReprocessingFactory.h"
#include "ExynosCameraFrameFactoryVision.h"

#ifdef BURST_CAPTURE
#include <sys/resource.h>
#include <private/android_filesystem_config.h>

#include <ctype.h>
#include <dirent.h>
#endif

#ifdef TOUCH_AE
#define AE_RESULT 0xF351
#endif

namespace android {

class ExynosCamera {
public:
    ExynosCamera() {};
    ExynosCamera(int cameraId, camera_device_t *dev);
    virtual             ~ExynosCamera();
    void        initialize();
    void        release();

    int         getCameraId() const;

    /* For preview */
    status_t    setPreviewWindow(preview_stream_ops *w);   /* Vendor */

    status_t    startPreview();         /* Vendor */
    void        stopPreview();          /* Vendor */
    bool        previewEnabled();

    /* For recording */
    status_t    startRecording();       /* Vendor */
    void        stopRecording();        /* Vendor */
    bool        recordingEnabled();
    void        releaseRecordingFrame(const void *opaque);

    /* For capture */
    status_t    takePicture();          /* Vendor */
    status_t    cancelPicture();

    /* For focusing */
    status_t    autoFocus();
    status_t    cancelAutoFocus();      /* Vendor */

    /* For settings */
    status_t    setParameters(const CameraParameters& params);   /* Vendor */
    CameraParameters  getParameters() const;
    status_t    sendCommand(int32_t command, int32_t arg1, int32_t arg2);   /* Vendor */

    void        setCallbacks(
                    camera_notify_callback notify_cb,
                    camera_data_callback data_cb,
                    camera_data_timestamp_callback data_cb_timestamp,
                    camera_request_memory get_memory,
                    void *user);

    status_t    setDualMode(bool enabled);

    void        enableMsgType(int32_t msgType);
    void        disableMsgType(int32_t msgType);
    bool        msgTypeEnabled(int32_t msgType);

    status_t    storeMetaDataInBuffers(bool enable);

    /* For debugging */
    status_t    dump(int fd);

private:
    /* Internal Threads & Functions */
    camera_device_t                 *m_dev;
    typedef ExynosCameraThread<ExynosCamera> mainCameraThread;

    /* Main handling thread */
    sp<mainCameraThread>            m_mainThread;
    bool                            m_mainThreadFunc(void);
    status_t                        m_handlePreviewFrame(ExynosCameraFrame *frame); /* Vendor */

    sp<mainCameraThread>            m_mainSetupQThread[MAX_NUM_PIPES];
    bool                            m_mainThreadQSetupFLITE(void);
    bool                            m_mainThreadQSetup3AA(void);

    sp<mainCameraThread>            m_previewThread;
    bool                            m_previewThreadFunc(void);                      /* Vendor */

    sp<mainCameraThread>            m_recordingThread;
    bool                            m_recordingThreadFunc(void);                    /* Vendor */

    sp<mainCameraThread>            m_zoomPreviwWithCscThread;
    bool                            m_zoomPreviwWithCscThreadFunc(void);

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    frame_queue_t                  *m_fusionQ;
    sp<mainCameraThread>           m_fusionThread;
    bool                           m_fusionThreadFunc(void);
    status_t                       m_syncPreviewWithFusion(int pipeId, ExynosCameraFrame *frame);
#endif

    /* Start threads */
    sp<mainCameraThread>            m_setBuffersThread;
    bool                            m_setBuffersThreadFunc(void);

    sp<mainCameraThread>            m_startPictureInternalThread;
    bool                            m_startPictureInternalThreadFunc(void);

    sp<mainCameraThread>            m_startPictureBufferThread;
    bool                            m_startPictureBufferThreadFunc(void);

    /* Capture Threads */
    sp<mainCameraThread>            m_prePictureThread;
    bool                            m_prePictureThreadFunc(void);

    sp<mainCameraThread>            m_pictureThread;
    bool                            m_pictureThreadFunc(void);                      /* Vendor */

    sp<mainCameraThread>            m_postPictureThread;
    bool                            m_postPictureThreadFunc(void);                  /* Vendor */

    sp<mainCameraThread>            m_jpegCallbackThread;
    bool                            m_jpegCallbackThreadFunc(void);                 /* Vendor */

    sp<mainCameraThread>            m_yuvCallbackThread;
    bool                            m_yuvCallbackThreadFunc(void);

    sp<mainCameraThread>            m_ThumbnailCallbackThread;
    bool                            m_ThumbnailCallbackThreadFunc(void);

    sp<mainCameraThread>            m_jpegSaveThread[JPEG_SAVE_THREAD_MAX_COUNT];
    bool                            m_jpegSaveThreadFunc(void);                     /* Vendor */

    sp<mainCameraThread>            m_shutterCallbackThread;
    bool                            m_shutterCallbackThreadFunc(void);              /* Vendor */

    sp<mainCameraThread>            m_highResolutionCallbackThread;
    bool                            m_highResolutionCallbackThreadFunc(void);

    sp<mainCameraThread>            m_postPictureCallbackThread;
    bool                            m_postPictureCallbackThreadFunc(void);

    /* AutoFocus & FaceDetection Threads */
    sp<mainCameraThread>            m_autoFocusThread;
    bool                            m_autoFocusThreadFunc(void);                    /* Vendor */

    sp<mainCameraThread>            m_autoFocusContinousThread;
    bool                            m_autoFocusContinousThreadFunc(void);           /* Vendor */

    sp<mainCameraThread>            m_facedetectThread;
    bool                            m_facedetectThreadFunc(void);

    /* ETC Threads */
    sp<mainCameraThread>            m_visionThread;
    bool                            m_visionThreadFunc(void);                       /* Vendor */

    sp<mainCameraThread>            m_monitorThread;
    bool                            m_monitorThreadFunc(void);                      /* Vendor */

#ifdef GED_DNG
    sp<mainCameraThread>            m_DNGCaptureThread;
    bool                            m_DNGCaptureThreadFunc(void);                   /* Vendor */
#endif

    /* Construction/Destruction function */
    void        m_createThreads(void);
    status_t    m_setConfigInform();
    status_t    m_setFrameManager();
    status_t    m_initFrameFactory(void);
    status_t    m_deinitFrameFactory(void);
    void        m_vendorSpecificConstructor(int cameraId, camera_device_t *dev);    /* Vendor */
    void        m_vendorSpecificDestructor(void);                                   /* Vendor */

    /* Start/Stop function */
    status_t    m_startPreviewInternal(void);                                       /* Vendor */
    status_t    m_stopPreviewInternal(void);                                        /* Vendor */

    status_t    m_restartPreviewInternal(bool flagUpdateParam, CameraParameters *params); /* Vendor */

    status_t    m_startPictureInternal(void);
    status_t    m_stopPictureInternal(void);                                        /* Vendor */

    status_t    m_startRecordingInternal(void);                                     /* Vendor */
    status_t    m_stopRecordingInternal(void);                                      /* Vendor */

    status_t    m_startVisionInternal(void);                                        /* Vendor */
    status_t    m_stopVisionInternal(void);                                         /* Vendor */

    status_t    m_startCompanion(void);                                             /* Vendor */
    status_t    m_stopCompanion(void);                                              /* Vendor */
    status_t    m_waitCompanionThreadEnd(void);                                     /* Vendor */

    bool        m_startFaceDetection(void);
    bool        m_stopFaceDetection(void);

    /* Buffer function */
    status_t    m_setBuffers(void);
    status_t    m_setVendorBuffers(void);                                           /* Vendor */
    status_t    m_setReprocessingBuffer(void);
    status_t    m_setPreviewCallbackBuffer(void);
    status_t    m_setPictureBuffer(void);
    status_t    m_setVendorPictureBuffer(void);                                     /* Vendor */
    status_t    m_releaseBuffers(void);
    status_t    m_releaseVendorBuffers(void);                                       /* Vendor */

    status_t    m_putBuffers(ExynosCameraBufferManager *bufManager, int bufIndex);
    status_t    m_allocBuffers(
                    ExynosCameraBufferManager *bufManager,
                    int  planeCount,
                    unsigned int *planeSize,
                    unsigned int *bytePerLine,
                    int  reqBufCount,
                    bool createMetaPlane,
                    bool needMmap = false);
    status_t    m_allocBuffers(
                    ExynosCameraBufferManager *bufManager,
                    int  planeCount,
                    unsigned int *planeSize,
                    unsigned int *bytePerLine,
                    int  minBufCount,
                    int  maxBufCount,
                    exynos_camera_buffer_type_t type,
                    bool createMetaPlane,
                    bool needMmap = false);
    status_t    m_allocBuffers(
                    ExynosCameraBufferManager *bufManager,
                    int  planeCount,
                    unsigned int *planeSize,
                    unsigned int *bytePerLine,
                    int  minBufCount,
                    int  maxBufCount,
                    exynos_camera_buffer_type_t type,
                    buffer_manager_allocation_mode_t allocMode,
                    bool createMetaPlane,
                    bool needMmap = false);
    status_t    m_allocBuffers(
                    ExynosCameraBufferManager *bufManager,
                    int  planeCount,
                    unsigned int *planeSize,
                    unsigned int *bytePerLine,
                    int  reqBufCount,
                    bool createMetaPlane,
                    int width,
                    int height,
                    int stride,
                    int pixelFormat,
                    bool needMmap = false);

    bool        m_enableFaceDetection(bool toggle);
    int         m_calibratePosition(int w, int new_w, int pos);
    status_t    m_doFdCallbackFunc(ExynosCameraFrame *frame);

    int         m_getShotBufferIdex() const;
    status_t    m_generateFrame(int32_t frameCount, ExynosCameraFrame **newFrame);

    status_t    m_generateFrameReprocessing(ExynosCameraFrame **newFrame);

    /* vision */
    status_t    m_generateFrameVision(int32_t frameCount, ExynosCameraFrame **newFrame);

    void        m_dump(void);
    void        m_dumpVendor(void);         /* Vendor */

    status_t    m_searchFrameFromList(List<ExynosCameraFrame *> *list, uint32_t frameCount, ExynosCameraFrame **frame);
    status_t    m_removeFrameFromList(List<ExynosCameraFrame *> *list, ExynosCameraFrame *frame);
    status_t    m_deleteFrame(ExynosCameraFrame **frame);

    status_t    m_clearList(List<ExynosCameraFrame *> *list);
    status_t    m_clearList(frame_queue_t *queue);
    status_t    m_clearFrameQ(frame_queue_t *frameQ, uint32_t pipeId, uint32_t direction);

    status_t    m_printFrameList(List<ExynosCameraFrame *> *list);

    status_t    m_createIonAllocator(ExynosCameraIonAllocator **allocator);
    status_t    m_createInternalBufferManager(ExynosCameraBufferManager **bufferManager, const char *name);
    status_t    m_createBufferManager(
                    ExynosCameraBufferManager **bufferManager,
                    const char *name,
                    buffer_manager_type type = BUFFER_MANAGER_ION_TYPE);

    status_t    m_setupEntity(
                    uint32_t pipeId,
                    ExynosCameraFrame *newFrame,
                    ExynosCameraBuffer *srcBuf = NULL,
                    ExynosCameraBuffer *dstBuf = NULL);
    status_t    m_setSrcBuffer(
                    uint32_t pipeId,
                    ExynosCameraFrame *newFrame,
                    ExynosCameraBuffer *buffer);
    status_t    m_setDstBuffer(
                    uint32_t pipeId,
                    ExynosCameraFrame *newFrame,
                    ExynosCameraBuffer *buffer);

    status_t    m_getBufferManager(uint32_t pipeId, ExynosCameraBufferManager **bufMgr, uint32_t direction);

    status_t    m_calcPreviewGSCRect(ExynosRect *srcRect, ExynosRect *dstRect);
    status_t    m_calcHighResolutionPreviewGSCRect(ExynosRect *srcRect, ExynosRect *dstRect);
    status_t    m_calcRecordingGSCRect(ExynosRect *srcRect, ExynosRect *dstRect);
    status_t    m_calcPictureRect(ExynosRect *srcRect, ExynosRect *dstRect);
    status_t    m_calcPictureRect(int originW, int originH, ExynosRect *srcRect, ExynosRect *dstRect);

    status_t    m_setCallbackBufferInfo(ExynosCameraBuffer *callbackBuf, char *baseAddr);

    status_t    m_doPreviewToCallbackFunc(
                    int32_t pipeId,
                    ExynosCameraFrame *newFrame,
                    ExynosCameraBuffer previewBuf,
                    ExynosCameraBuffer callbackBuf,
                    bool copybuffer = false);
    status_t    m_doCallbackToPreviewFunc(
                    int32_t pipeId,
                    ExynosCameraFrame *newFrame,
                    ExynosCameraBuffer callbackBuf,
                    ExynosCameraBuffer previewBuf);
    status_t    m_doPrviewToRecordingFunc(
                    int32_t pipeId,
                    ExynosCameraBuffer previewBuf,
                    ExynosCameraBuffer recordingBuf,
                    nsecs_t timeStamp);
    status_t    m_syncPrviewWithCSC(int32_t pipeId, int32_t gscPipe, ExynosCameraFrame *frame);
    status_t    m_getAvailableRecordingCallbackHeapIndex(int *index);
    status_t    m_releaseRecordingCallbackHeap(struct VideoNativeHandleMetadata *addr);
    status_t    m_releaseRecordingBuffer(int bufIndex);

    status_t    m_fastenAeStable(void);
    camera_memory_t *m_getJpegCallbackHeap(ExynosCameraBuffer callbackBuf, int seriesShotNumber);

    void        m_debugFpsCheck(uint32_t pipeId);

    uint32_t    m_getBayerPipeId(void);

    status_t    m_convertingStreamToShotExt(ExynosCameraBuffer *buffer, struct camera2_node_output *outputInfo);

    status_t    m_getBayerBuffer(uint32_t pipeId, ExynosCameraBuffer *buffer, camera2_shot_ext *updateDmShot = NULL);
    status_t    m_checkBufferAvailable(uint32_t pipeId, ExynosCameraBufferManager *bufferMgr);

    status_t    m_boostDynamicCapture(void);
    void        m_updateBoostDynamicCaptureSize(camera2_node_group *node_group_info);
    void        m_checkFpsAndUpdatePipeWaitTime(void);
    void        m_printExynosCameraInfo(const char *funcName);

    void        m_checkEntranceLux(struct camera2_shot_ext *meta_shot_ext);
    status_t    m_copyMetaFrameToFrame(ExynosCameraFrame *srcframe, ExynosCameraFrame *dstframe, bool useDm, bool useUdm);
    status_t    m_putFrameBuffer(ExynosCameraFrame *frame, int pipeId, enum buffer_direction_type bufferDirectionType);

    status_t                        m_setVisionBuffers(void);
    status_t                        m_setVisionCallbackBuffer(void);

    bool                            m_getRecordingEnabled(void);
    void                            m_setRecordingEnabled(bool enable);

    /* Pre picture Thread */
    bool                            m_reprocessingPrePictureInternal(void);
    bool                            m_prePictureInternal(bool* pIsProcessed);

    void                            m_terminatePictureThreads(bool callFromJpeg);

    bool                            m_releasebuffersForRealloc(void);
    bool                            m_checkCameraSavingPath(char *dir, char* srcPath, char *dstPath, int dstSize);
    bool                            m_FileSaveFunc(char *filePath, ExynosCameraBuffer *SaveBuf);
    bool                            m_FileSaveFunc(char *filePath, char *saveBuf, unsigned int size);


public:

private:
    /* Internal Queue */
    /* Main thread Queue */
    frame_queue_t                   *m_pipeFrameDoneQ;
    frame_queue_t                   *m_mainSetupQ[MAX_NUM_PIPES];
    frame_queue_t                   *m_previewQ;
    frame_queue_t                   *m_recordingQ;
    frame_queue_t                   *m_zoomPreviwWithCscQ;
    frame_queue_t                   *m_previewCallbackGscFrameDoneQ;

    /* Reprocessing thread Queue */
    frame_queue_t                   *m_dstIspReprocessingQ;
    frame_queue_t                   *m_dstSccReprocessingQ;
    frame_queue_t                   *m_dstGscReprocessingQ;
    frame_queue_t                   *m_dstPostPictureGscQ;
    frame_queue_t                   *m_dstJpegReprocessingQ;
    frame_queue_t                   *m_postPictureQ;
    jpeg_callback_queue_t           *m_jpegCallbackQ;
    yuv_callback_queue_t            *m_yuvCallbackQ;
    postview_callback_queue_t       *m_postviewCallbackQ;
    thumbnail_callback_queue_t      *m_thumbnailCallbackQ;
    frame_queue_t                   *m_ThumbnailPostCallbackQ;
    frame_queue_t                   *m_highResolutionCallbackQ;

#ifdef GED_DNG
    dng_capture_queue_t             *m_dngCaptureQ;
    bayer_release_queue_t           *m_dngCaptureDoneQ;
#endif

    /* AutoFocus & FaceDetection thread Queue */
    worker_queue_t                  m_autoFocusContinousQ;
    frame_queue_t                   *m_facedetectQ;

    /* vision */
    frame_queue_t                   *m_pipeFrameVisionDoneQ;

    /* Buffer managers */
    /* Main buffer managers */
    ExynosCameraBufferManager       *m_bayerBufferMgr;
#ifdef DEBUG_RAWDUMP
    ExynosCameraBufferManager       *m_fliteBufferMgr;
#endif
    ExynosCameraBufferManager       *m_3aaBufferMgr;
    ExynosCameraBufferManager       *m_ispBufferMgr;
    ExynosCameraBufferManager       *m_hwDisBufferMgr;
    ExynosCameraBufferManager       *m_sccBufferMgr;
    ExynosCameraBufferManager       *m_scpBufferMgr;
    ExynosCameraBufferManager       *m_zoomScalerBufferMgr;
#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    ExynosCameraBufferManager       *m_fusionBufferMgr;
#endif
    ExynosCameraBufferManager       *m_recordingBufferMgr;
    ExynosCameraBufferManager       *m_previewCallbackBufferMgr;

    /* Reprocessing buffer managers */
    ExynosCameraBufferManager       *m_ispReprocessingBufferMgr;
    ExynosCameraBufferManager       *m_sccReprocessingBufferMgr;
    ExynosCameraBufferManager       *m_thumbnailBufferMgr;
    ExynosCameraBufferManager       *m_thumbnailGscBufferMgr;
    ExynosCameraBufferManager       *m_highResolutionCallbackBufferMgr;

    /* TODO: will be removed when SCC scaling for picture size */
    ExynosCameraBufferManager       *m_gscBufferMgr;
    ExynosCameraBufferManager       *m_postPictureBufferMgr;
    ExynosCameraBufferManager       *m_jpegBufferMgr;

    ExynosCamera1Parameters         *m_parameters;
    preview_stream_ops              *m_previewWindow;

    ExynosCameraFrameFactory        *m_previewFrameFactory;
    ExynosCameraFrameFactory        *m_pictureFrameFactory;
    ExynosCameraFrameFactory        *m_reprocessingFrameFactory;
    ExynosCameraFrameFactory        *m_visionFrameFactory;

    ExynosCameraGrallocAllocator    *m_grAllocator;
    ExynosCameraIonAllocator        *m_ionAllocator;
    ExynosCameraMHBAllocator        *m_mhbAllocator;

    mutable Mutex                   m_frameLock;
    mutable Mutex                   m_searchframeLock;

    bool                            m_previewEnabled;
    bool                            m_pictureEnabled;
    bool                            m_recordingEnabled;
    bool                            m_zslPictureEnabled;
    bool                            m_use_companion;
    bool                            m_checkFirstFrameLux;
    ExynosCameraActivityControl     *m_exynosCameraActivityControl;

    uint32_t                        m_cameraId;
    uint32_t                        m_cameraSensorId;
    char                            m_name[EXYNOS_CAMERA_NAME_STR_SIZE];

    camera_notify_callback          m_notifyCb;
    camera_data_callback            m_dataCb;
    camera_data_timestamp_callback  m_dataCbTimestamp;
    camera_request_memory           m_getMemoryCb;
    void                            *m_callbackCookie;

    List<ExynosCameraFrame *>       m_processList;
    List<ExynosCameraFrame *>       m_postProcessList;
    List<ExynosCameraFrame *>       m_recordingProcessList;

    bool                            m_autoFocusResetNotify(int focusMode);
    mutable Mutex                   m_autoFocusLock;
    mutable Mutex                   m_captureLock;
    mutable Mutex                   m_recordingListLock;
    bool                            m_exitAutoFocusThread;
    bool                            m_autoFocusRunning;
    int                             m_autoFocusType;

    uint32_t                        m_fliteFrameCount;
    uint32_t                        m_3aa_ispFrameCount;
    uint32_t                        m_ispFrameCount;
    uint32_t                        m_sccFrameCount;
    uint32_t                        m_scpFrameCount;

    int                             m_frameSkipCount;

    ExynosCameraFrameManager        *m_frameMgr;

    bool                            m_isSuccessedBufferAllocation;

    /* for Recording */
    bool                            m_doCscRecording;
    int                             m_recordingBufferCount;
    nsecs_t                         m_lastRecordingTimeStamp;
    nsecs_t                         m_recordingStartTimeStamp;

    camera_memory_t                 *m_recordingCallbackHeap;
    bool                            m_recordingCallbackHeapAvailable[MAX_BUFFERS];
    mutable Mutex                   m_recordingCallbackHeapAvailableLock;

    bool                            m_recordingBufAvailable[MAX_BUFFERS];
    nsecs_t                         m_recordingTimeStamp[MAX_BUFFERS];
    mutable Mutex                   m_recordingStateLock;
    ExynosCameraCounter             m_takePictureCounter;
    ExynosCameraCounter             m_reprocessingCounter;
    ExynosCameraCounter             m_pictureCounter;
    ExynosCameraCounter             m_jpegCounter;
    ExynosCameraCounter             m_yuvcallbackCounter;

    bool                            m_flagStartFaceDetection;
    bool                            m_flagLLSStart;
    bool                            m_flagLightCondition;
    camera_face_t                   m_faces[NUM_OF_DETECTED_FACES];
    camera_frame_metadata_t         m_frameMetadata;
    camera_memory_t                 *m_fdCallbackHeap;

    bool                            m_faceDetected;
    int                             m_fdThreshold;

    ExynosCameraScalableSensor      m_scalableSensorMgr;

    /* Watch Dog Thread */
#ifdef MONITOR_LOG_SYNC
    static uint32_t                 cameraSyncLogId;
    int                             m_syncLogDuration;
    uint32_t                        m_getSyncLogId(void);
#endif
    bool                            m_disablePreviewCB;
    bool                            m_flagThreadStop;
    status_t                        m_checkThreadState(int *threadState, int *countRenew);
    status_t                        m_checkThreadInterval(uint32_t pipeId, uint32_t pipeInterval, int *threadState);
    unsigned int                    m_callbackState;
    unsigned int                    m_callbackStateOld;
    int                             m_callbackMonitorCount;
    bool                            m_isNeedAllocPictureBuffer;

#ifdef FPS_CHECK
    /* TODO: */
#define DEBUG_MAX_PIPE_NUM 10
    int32_t                         m_debugFpsCount[DEBUG_MAX_PIPE_NUM];
    ExynosCameraDurationTimer       m_debugFpsTimer[DEBUG_MAX_PIPE_NUM];
#endif

    ExynosCameraFrameSelector       *m_captureSelector;
    ExynosCameraFrameSelector       *m_sccCaptureSelector;

    jpeg_callback_queue_t          *m_jpegSaveQ[JPEG_SAVE_THREAD_MAX_COUNT];

#ifdef BURST_CAPTURE
    bool                            m_isCancelBurstCapture;
    int                             m_burstCaptureCallbackCount;
    mutable Mutex                   m_burstCaptureCallbackCountLock;
    mutable Mutex                   m_burstCaptureSaveLock;
    ExynosCameraDurationTimer       m_burstSaveTimer;
    long long                       m_burstSaveTimerTime;
    int                             m_burstDuration;
    bool                            m_burstInitFirst;
    bool                            m_burstRealloc;
    char                            m_burstSavePath[CAMERA_FILE_PATH_SIZE];
    int                             m_burstShutterLocation;
#endif

#ifdef USE_PREVIEW_DURATION_CONTROL
    ExynosCameraDurationTimer       PreviewDurationTimer;
    uint64_t                        PreviewDurationTime;
#endif

#ifdef PREVIEW_DURATION_DEBUG
    ExynosCameraDurationTimer       PreviewDurationDebugTimer;
#endif

    bool                            m_stopBurstShot;
    bool                            m_burst[JPEG_SAVE_THREAD_MAX_COUNT];
    bool                            m_running[JPEG_SAVE_THREAD_MAX_COUNT];

    bool                            m_isZSLCaptureOn;

    bool                            m_highResolutionCallbackRunning;
    bool                            m_skipReprocessing;
    int                             m_skipCount;
    bool                            m_resetPreview;

    uint32_t                        m_displayPreviewToggle;

    bool                            m_hdrEnabled;
    unsigned int                    m_hdrSkipedFcount;
    bool                            m_isFirstStart;
    uint32_t                        m_dynamicSccCount;
    bool                            m_isTryStopFlash;
    uint32_t                        m_curMinFps;

    int                             m_visionFps;
    int                             m_visionAe;
    int                             m_ionClient;


    mutable Mutex                   m_metaCopyLock;
    struct camera2_shot_ext         *m_tempshot;
    struct camera2_shot_ext         *m_fdmeta_shot;
    struct camera2_shot_ext         *m_meta_shot;
    int                             m_previewBufferCount;
    struct ExynosConfigInfo         *m_exynosconfig;
#if 1
    uint32_t                        m_hackForAlignment;
#endif
    uint32_t                        m_recordingFrameSkipCount;

    unsigned int                    m_longExposureRemainCount;
    bool                            m_longExposurePreview;
    bool                            m_stopLongExposure;
    bool                            m_cancelPicture;

#ifdef GED_DNG
    camera_memory_t                 *m_dngBayerHeap;
#endif

#ifdef FIRST_PREVIEW_TIME_CHECK
    ExynosCameraDurationTimer       m_firstPreviewTimer;
    bool                            m_flagFirstPreviewTimerOn;
#endif

#ifdef CAMERA_VENDOR_TURNKEY_FEATURE
    sp<ExynosCameraSFLMgr>          m_sflMgr;
    bool                            m_multiCaptureThreadFunc(void);
    sp<mainCameraThread>            m_multiCaptureThread;
    frame_queue_t                   *m_multiCaptureQ;
    status_t                        m_vendorFeatureUpdateMetaInfo(ExynosCameraFrame* frame, int pipeId);
    status_t                        m_vendorFeaturePreviewInfo(ExynosCameraFrame *frame, int pipeId, bool isSrc, int dstPos);
    status_t                        m_vendorFeatureTakePicture(int *pictureCount, int *seriesShotCount);

    /* vendor library queue */
    frame_queue_t                   *m_vendorLibraryQ;

    status_t                        m_vendorFeatureHandlePreviewFrame(ExynosCameraFrame *frame, ExynosCameraFrameEntity *entity);

    /* vendor library thread */
    sp<mainCameraThread>            m_vendorLibraryThread;
    bool                            m_vendorLibraryThreadFunc(void);

#endif

};

}; /* namespace android */
#endif
