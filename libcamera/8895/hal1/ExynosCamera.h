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

#ifdef SAMSUNG_TN_FEATURE
#include "SecCameraParameters.h"
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

#ifdef SUPPORT_SW_VDIS
#include "SecCameraSWVdis_3_0.h"
#endif /*SUPPORT_SW_VDIS*/

#ifdef SAMSUNG_HYPER_MOTION
#include "SecCameraHyperMotion.h"
#endif /*SAMSUNG_HYPER_MOTION*/

#ifdef BURST_CAPTURE
#include <sys/resource.h>
#include <private/android_filesystem_config.h>

#include <ctype.h>
#include <dirent.h>
#endif

#ifdef LLS_CAPTURE
#define MULTI_FRAME_SHOT_PARAMETERS   0xF124
#endif

#ifdef LLS_REPROCESSING
#define MULTI_FRAME_SHOT_BV_INFO        0xF127
#endif

#ifdef SR_CAPTURE
#define MULTI_FRAME_SHOT_SR_EXTRA_INFO 0xF126
#endif

#ifdef SUPPORT_DEPTH_MAP
#define OUTFOCUS_DEPTH_MAP_INFO 0xF324
#endif

#ifdef TOUCH_AE
#define AE_RESULT 0xF351
#endif

#define COMMON_SHOT_CANCEL_PICTURE_COMPLETED 0xF411

namespace android {

enum longexposure_capture_status {
    LONG_EXPOSURE_PREVIEW       = 0,
    LONG_EXPOSURE_CAPTURING,
    LONG_EXPOSURE_CANCEL_NOTI
};

enum CAMERA_FACING_DIRECTION {
    CAMERA_FACING_DIRECTION_BACK  = 0,
    CAMERA_FACING_DIRECTION_FRONT = 1,
};

struct CameraBufferFD {
    unsigned int magic;
    int fd;
};

class ExynosCamera {
public:
    ExynosCamera() {};
    ExynosCamera(int cameraId, camera_device_t *dev);
    virtual             ~ExynosCamera();
    void        initialize();
    void        initializeVision();    /* Vendor */
    void        release();
    void        releaseVision();    /* Vendor */

    int         getCameraId() const;
    int         getCameraIdInternal() const;

    /* For preview */
    status_t    setPreviewWindow(preview_stream_ops *w);   /* Vendor */

    status_t    startPreview();         /* Vendor */
    void        stopPreview();          /* Vendor */
    status_t    startPreviewVision();         /* Vendor */
    void        stopPreviewVision();          /* Vendor */
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

#ifdef SAMSUNG_QUICK_SWITCH
    bool        getQuickSwitchFlag();
#endif

#ifdef DEBUG_IQ_OSD
    void        printOSD(char *Y, char *UV, struct camera2_shot_ext *meta_shot_ext);
    int         isOSDMode;
#endif

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
    status_t                        m_handlePreviewFrame(ExynosCameraFrameSP_sptr_t frame); /* Vendor */

    /* handler for faceDetection */
    void                            m_handleFaceDetectionFrame(ExynosCameraFrameSP_sptr_t previewFrame, ExynosCameraBuffer *previewBuffer);

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
    status_t                        m_prepareBeforeSync(int srcPipeId, int dstPipeId,
                                                        ExynosCameraFrameSP_sptr_t frame,
                                                        int dstPos,
                                                        ExynosCameraFrameFactory *factory,
                                                        frame_queue_t *outputQ,
                                                        bool isReprocessing);
    status_t                        m_prepareAfterSync(int srcPipeId, int dstPipeId,
                                                       ExynosCameraFrameSP_sptr_t frame,
                                                       int dstPos,
                                                       ExynosCameraFrameFactory *factory,
                                                       frame_queue_t *outputQ,
                                                       bool isReprocessing);
    status_t                        m_prepareAfterFusion(int srcPipeId,
                                                       ExynosCameraFrameSP_sptr_t frame,
                                                       int dstPos, bool isReprocessing);
#endif

    /* Start threads */
    sp<mainCameraThread>            m_setBuffersThread;
    bool                            m_setBuffersThreadFunc(void);

    sp<mainCameraThread>            m_startPictureInternalThread;
    bool                            m_startPictureInternalThreadFunc(void);

    sp<mainCameraThread>            m_startPictureBufferThread;
    bool                            m_startPictureBufferThreadFunc(void);

#ifdef CAMERA_FAST_ENTRANCE_V1
    sp<mainCameraThread>            m_fastenAeThread;
    bool                            m_fastenAeThreadFunc(void);
    status_t                        m_waitFastenAeThreadEnd(void);
#endif

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

#ifdef SAMSUNG_LBP
    sp<mainCameraThread>            m_LBPThread;
    bool                            m_LBPThreadFunc(void);                          /* Vendor */
#endif

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

#ifdef SAMSUNG_COMPANION
    sp<mainCameraThread>            m_companionThread;
    bool                            m_companionThreadFunc(void);                    /* Vendor */
#endif
#ifdef SAMSUNG_EEPROM
    sp<mainCameraThread>            m_eepromThread;
    bool                            m_eepromThreadFunc(void);                       /* Vendor */
#endif
#ifdef RAWDUMP_CAPTURE
    sp<mainCameraThread>            m_RawCaptureDumpThread;
    bool                            m_RawCaptureDumpThreadFunc(void);               /* Vendor */
#endif
#ifdef SAMSUNG_DNG
    sp<mainCameraThread>            m_DNGCaptureThread;
    bool                            m_DNGCaptureThreadFunc(void);                   /* Vendor */
#endif
#ifdef SAMSUNG_LLS_DEBLUR
    sp<mainCameraThread>            m_LDCaptureThread;
    bool                            m_LDCaptureThreadFunc(void);                    /* Vendor */
#endif
#ifdef SAMSUNG_UNIPLUGIN
    sp<mainCameraThread>            m_uniPluginThread;
    bool                            m_uniPluginThreadFunc(void);                    /* Vendor */
#endif
#ifdef SAMSUNG_QUICK_SWITCH
    sp<mainCameraThread>            m_preStartPictureInternalThread;
    bool                            m_preStartPictureInternalThreadFunc(void);		/* Vendor */
#endif

    /* Construction/Destruction function */
    void        m_createThreads(void);
    status_t    m_setConfigInform();
    status_t    m_setFrameManager();
    status_t    m_initFrameFactory(void);
    status_t    m_deinitFrameFactory(void);
    void        m_vendorSpecificPreConstructor(int cameraId, camera_device_t *dev);/* Vendor */
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
    bool        m_startCurrentSet(void);                                            /* Vendor */
    bool        m_stopCurrentSet(void);                                             /* Vendor */

    /* Buffer function */
    status_t    m_setBuffers(void);
    status_t    m_setVendorBuffers(void);                                           /* Vendor */
#ifdef SAMSUNG_QUICK_SWITCH
    status_t    m_reallocVendorBuffers(void);                                       /* Vendor */
    status_t    m_deallocVendorBuffers(void);                                       /* Vendor */
#endif
    status_t    m_setReprocessingBuffer(void);
    status_t    m_setPreviewCallbackBuffer(void);
    status_t    m_setPictureBuffer(void);
    status_t    m_setVendorPictureBuffer(void);                                     /* Vendor */
    status_t    m_releaseBuffers(void);
    status_t    m_releaseVendorBuffers(void);                                       /* Vendor */

    status_t    m_putBuffers(ExynosCameraBufferManager *bufManager, int bufIndex);
#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    status_t    m_putFusionReprocessingBuffers(ExynosCameraFrameSP_sptr_t frame, ExynosCameraBuffer *buf);
#endif
    status_t    m_allocBuffers(
                    ExynosCameraBufferManager *bufManager,
                    int  planeCount,
                    unsigned int *planeSize,
                    unsigned int *bytePerLine,
                    int  reqBufCount,
                    int batchSize,
                    bool createMetaPlane,
                    bool needMmap = false);
    status_t    m_allocBuffers(
                    ExynosCameraBufferManager *bufManager,
                    int  planeCount,
                    unsigned int *planeSize,
                    unsigned int *bytePerLine,
                    int  minBufCount,
                    int  maxBufCount,
                    int batchSize,
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
                    int batchSize,
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
    status_t    m_doFdCallbackFunc(ExynosCameraFrameSP_sptr_t frame);
#ifdef SR_CAPTURE
    status_t    m_doSRCallbackFunc();
#endif

    int         m_getShotBufferIdex() const;
    status_t    m_generateFrame(int32_t frameCount, ExynosCameraFrameSP_dptr_t newFrame);

    status_t    m_generateFrameReprocessing(ExynosCameraFrameSP_dptr_t newFrame, ExynosCameraFrameSP_dptr_t refFrame);

    /* vision */
    status_t    m_generateFrameVision(int32_t frameCount, ExynosCameraFrameSP_dptr_t newFrame);

    void        m_dump(void);
    void        m_dumpVendor(void);         /* Vendor */

    status_t    m_insertFrameToList(List<ExynosCameraFrameSP_sptr_t> *list, ExynosCameraFrameSP_sptr_t frame, Mutex *lock);
    status_t    m_searchFrameFromList(List<ExynosCameraFrameSP_sptr_t> *list, uint32_t frameCount, ExynosCameraFrameSP_dptr_t frame, Mutex *lock);
    status_t    m_removeFrameFromList(List<ExynosCameraFrameSP_sptr_t> *list, ExynosCameraFrameSP_sptr_t frame, Mutex *lock);
    status_t    m_clearList(List<ExynosCameraFrameSP_sptr_t> *list, Mutex *lock);
    status_t    m_printFrameList(List<ExynosCameraFrameSP_sptr_t> *list, Mutex *lock);

    status_t    m_deleteFrame(ExynosCameraFrameSP_dptr_t frame);

    status_t    m_clearList(frame_queue_t *queue);
    status_t    m_clearFrameQ(frame_queue_t *frameQ, uint32_t pipeId, uint32_t direction);

    status_t    m_createIonAllocator(ExynosCameraIonAllocator **allocator);
    status_t    m_createInternalBufferManager(ExynosCameraBufferManager **bufferManager, const char *name);
    status_t    m_createBufferManager(
                    ExynosCameraBufferManager **bufferManager,
                    const char *name,
                    buffer_manager_type type = BUFFER_MANAGER_ION_TYPE);

    status_t    m_setupEntity(
                    uint32_t pipeId,
                    ExynosCameraFrameSP_sptr_t newFrame,
                    ExynosCameraBuffer *srcBuf = NULL,
                    ExynosCameraBuffer *dstBuf = NULL);
    status_t    m_setSrcBuffer(
                    uint32_t pipeId,
                    ExynosCameraFrameSP_sptr_t newFrame,
                    ExynosCameraBuffer *buffer);
    status_t    m_setDstBuffer(
                    uint32_t pipeId,
                    ExynosCameraFrameSP_sptr_t newFrame,
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
                    ExynosCameraFrameSP_sptr_t newFrame,
                    ExynosCameraBuffer previewBuf,
                    ExynosCameraBuffer callbackBuf,
                    bool copybuffer = false);
    status_t    m_doCallbackToPreviewFunc(
                    int32_t pipeId,
                    ExynosCameraFrameSP_sptr_t newFrame,
                    ExynosCameraBuffer callbackBuf,
                    ExynosCameraBuffer previewBuf);
    status_t    m_doPreviewToRecordingFunc(
                    int32_t pipeId,
                    ExynosCameraBuffer previewBuf,
                    ExynosCameraBuffer recordingBuf,
                    nsecs_t timeStamp);
    status_t    m_syncPrviewWithCSC(int32_t pipeId, int32_t gscPipe, ExynosCameraFrameSP_sptr_t frame);
    status_t    m_releaseRecordingBuffer(int bufIndex);
    status_t    m_reprocessingYuvCallbackFunc(ExynosCameraBuffer yuvBuffer);

    status_t    m_fastenAeStable(void);
    camera_memory_t *m_getJpegCallbackHeap(ExynosCameraBuffer callbackBuf, int seriesShotNumber);

    void        m_debugFpsCheck(uint32_t pipeId);

    uint32_t    m_getBayerPipeId(void);

    status_t    m_convertingStreamToShotExt(ExynosCameraBuffer *buffer, struct camera2_node_output *outputInfo);

    status_t    m_getBayerBuffer(uint32_t pipeId, ExynosCameraBuffer *buffer, ExynosCameraFrameSP_dptr_t frame,
                                            camera2_shot_ext *updateDmShot = NULL
#ifdef SUPPORT_DEPTH_MAP
                                    , ExynosCameraBuffer *depthMapbuffer = NULL
#endif
                                    );
    status_t    m_checkBufferAvailable(uint32_t pipeId, ExynosCameraBufferManager *bufferMgr);

    status_t    m_boostDynamicCapture(void);
    void        m_updateBoostDynamicCaptureSize(camera2_node_group *node_group_info);
    void        m_checkFpsAndUpdatePipeWaitTime(void);
    void        m_printExynosCameraInfo(const char *funcName);

#ifdef ONE_SECOND_BURST_CAPTURE
    void        m_clearOneSecondBurst(bool isJpegCallbackThread);
#endif
    void        m_checkEntranceLux(struct camera2_shot_ext *meta_shot_ext);
    status_t    m_copyMetaFrameToFrame(ExynosCameraFrameSP_sptr_t srcframe, ExynosCameraFrameSP_sptr_t dstframe, bool useDm, bool useUdm);
    status_t    m_putFrameBuffer(ExynosCameraFrameSP_sptr_t frame, int pipeId, enum buffer_direction_type bufferDirectionType);

#if defined(SAMSUNG_DOF) || defined(SUPPORT_MULTI_AF)
    void                            m_clearDOFmeta(void);
#endif
#ifdef SAMSUNG_OT
    void                            m_clearOTmeta(void);
#endif
#ifdef SAMSUNG_DNG
    camera_memory_t*                m_getDngCallbackHeap(ExynosCameraBuffer *dngBuf);
    camera_memory_t*                m_getDngCallbackHeap(char *dngBuf, unsigned int bufSize);
#endif

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

    status_t                        m_copyNV21toNV21M(ExynosCameraBuffer srcBuf, ExynosCameraBuffer dstBuf,
                                                      int width, int height,
                                                      bool flagCopyY = true, bool flagCopyCbCr = true);
#ifdef USE_ODC_CAPTURE
    status_t                        m_processODC(ExynosCameraFrameSP_sptr_t newFrame);
    status_t                        m_convertHWPictureFormat(ExynosCameraFrameSP_sptr_t newFrame, int pictureFormat);
#endif
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
    frame_queue_t                   *m_jpegCallbackQ;
    frame_queue_t                   *m_yuvCallbackQ;
    postview_callback_queue_t       *m_postviewCallbackQ;
    thumbnail_callback_queue_t      *m_thumbnailCallbackQ;
    frame_queue_t                   *m_ThumbnailPostCallbackQ;
    frame_queue_t                   *m_highResolutionCallbackQ;
#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    frame_queue_t                   *m_syncReprocessingQ;
    frame_queue_t                   *m_fusionReprocessingQ;
#endif

#ifdef USE_ODC_CAPTURE
    frame_queue_t                   *m_ODCProcessingQ;
    frame_queue_t                   *m_postODCProcessingQ;
#endif

#ifdef SUPPORT_DEPTH_MAP
    depth_callback_queue_t          *m_depthCallbackQ;
#endif

#ifdef RAWDUMP_CAPTURE
    frame_queue_t                   *m_RawCaptureDumpQ;
#endif
#ifdef SAMSUNG_LBP
    lbp_queue_t                     *m_LBPbufferQ;
#endif
#ifdef SAMSUNG_DNG
    dng_capture_queue_t             *m_dngCaptureQ;
    bayer_release_queue_t           *m_dngCaptureDoneQ;
#endif
#ifdef SAMSUNG_BD
    bd_queue_t                      *m_BDbufferQ;
#endif
#ifdef SAMSUNG_LLS_DEBLUR
    frame_queue_t                   *m_LDCaptureQ;
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
    ExynosCameraBufferManager       *m_fusionReprocessingBufferMgr;
#endif
    ExynosCameraBufferManager       *m_recordingBufferMgr;
    ExynosCameraBufferManager       *m_previewCallbackBufferMgr;
#ifdef SUPPORT_DEPTH_MAP
    ExynosCameraBufferManager       *m_depthMapBufferMgr;
#endif
#ifdef SUPPORT_SW_VDIS
    ExynosCameraBufferManager       *m_swVDIS_BufferMgr;
    frame_queue_t                   *m_previewDelayQ;
#endif
#ifdef SAMSUNG_HYPER_MOTION
    ExynosCameraBufferManager       *m_hyperMotion_BufferMgr;
#endif
#ifdef SAMSUNG_LENS_DC
    ExynosCameraBufferManager       *m_lensDCBufferMgr;
#endif

    /* Reprocessing buffer managers */
    ExynosCameraBufferManager       *m_ispReprocessingBufferMgr;
    ExynosCameraBufferManager       *m_sccReprocessingBufferMgr;
    ExynosCameraBufferManager       *m_thumbnailBufferMgr;
    ExynosCameraBufferManager       *m_thumbnailGscBufferMgr;
    ExynosCameraBufferManager       *m_highResolutionCallbackBufferMgr;

    /* TODO: will be removed when SCC scaling for picture size */
    ExynosCameraBufferManager       *m_gscBufferMgr;
    ExynosCameraBufferManager       *m_postPictureBufferMgr;
#ifdef SAMSUNG_LBP
    ExynosCameraBufferManager       *m_lbpBufferMgr;
#endif
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
#ifdef OIS_CAPTURE
    bool                            m_OISCaptureShutterEnabled;
#endif
    bool                            m_use_companion;
    bool                            m_checkFirstFrameLux;
    ExynosCameraActivityControl     *m_exynosCameraActivityControl;

    uint32_t                        m_cameraId;
    uint32_t                        m_cameraOriginId;
    int                             m_scenario;
    uint32_t                        m_cameraSensorId;
    char                            m_name[EXYNOS_CAMERA_NAME_STR_SIZE];
#ifdef SAMSUNG_FRONT_LCD_FLASH
    char                            m_prevHBM[4];
    char                            m_prevAutoHBM[4];
#endif
    camera_notify_callback          m_notifyCb;
    camera_data_callback            m_dataCb;
    camera_data_timestamp_callback  m_dataCbTimestamp;
    camera_request_memory           m_getMemoryCb;
    void                            *m_callbackCookie;

    List<ExynosCameraFrameSP_sptr_t>       m_processList;
    List<ExynosCameraFrameSP_sptr_t>       m_postProcessList;
    List<ExynosCameraFrameSP_sptr_t>       m_recordingProcessList;

#ifdef SAMSUNG_COMPANION
    int                             m_getSensorId(int m_cameraId);
    ExynosCameraNode                *m_companionNode;
#endif
#ifdef SAMSUNG_HLV
    status_t                        m_ProgramAndProcessHLV(ExynosCameraBuffer *FrameBuffer);
#endif
#ifdef SAMSUNG_JQ
    int                             m_processJpegQtable(ExynosCameraBuffer* buffer);
#endif

    bool                            m_autoFocusResetNotify(int focusMode);
    mutable Mutex                   m_autoFocusLock;
    mutable Mutex                   m_captureLock;
    mutable Mutex                   m_processListLock;
    mutable Mutex                   m_postProcessListLock;
    mutable Mutex                   m_recordingProcessListLock;
    mutable Mutex                   m_recordingStopLock;
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

    bool                            m_recordingBufAvailable[MAX_BUFFERS];
    nsecs_t                         m_recordingTimeStamp[MAX_BUFFERS];
    mutable Mutex                   m_recordingStateLock;
    ExynosCameraCounter             m_takePictureCounter;
    ExynosCameraCounter             m_reprocessingCounter;
    ExynosCameraCounter             m_pictureCounter;
    ExynosCameraCounter             m_jpegCounter;
    ExynosCameraCounter             m_yuvcallbackCounter;
#ifdef ONE_SECOND_BURST_CAPTURE
    ExynosCameraCounter             m_jpegCallbackCounter;
#endif

#ifdef SAMSUNG_LBP
    lbp_buffer_t                    m_LBPbuffer[4];
#endif

    bool                            m_flagHoldFaceDetection;
    bool                            m_flagStartFaceDetection;
    bool                            m_flagLLSStart;
    bool                            m_flagLightCondition;
    bool                            m_flagBrightnessValue;
#ifdef SAMSUNG_CAMERA_EXTRA_INFO
    bool                            m_flagFlashCallback;
    bool                            m_flagHdrCallback;
#endif
    camera_face_t                   m_faces[NUM_OF_DETECTED_FACES];
    camera_frame_metadata_t         m_frameMetadata;
    camera_memory_t                 *m_fdCallbackHeap;
    int                             m_fdFrameSkipCount;

#ifdef SR_CAPTURE
    camera_face_t                   m_faces_sr[NUM_OF_DETECTED_FACES];
    camera_memory_t                 *m_srCallbackHeap;
    struct camera2_dm               m_srShotMeta;
    camera_frame_metadata_t         m_sr_frameMetadata;
    bool                            m_isCopySrMdeta;
    int                             m_sr_cropped_width;
    int                             m_sr_cropped_height;
#endif

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

#ifdef SAMSUNG_MAGICSHOT
    int                             m_magicshotMaxCount;
#endif
    frame_queue_t                   *m_jpegSaveQ[JPEG_SAVE_THREAD_MAX_COUNT];

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

#ifdef ONE_SECOND_BURST_CAPTURE
    ExynosCameraDurationTimer       TakepictureDurationTimer;
    uint64_t                        TakepictureDurationTime[ONE_SECOND_BURST_CAPTURE_CHECK_COUNT];
    bool                            m_one_second_burst_capture;
    bool                            m_one_second_burst_first_after_open;
    camera_memory_t                 *m_one_second_jpegCallbackHeap;
    camera_memory_t                 *m_one_second_postviewCallbackHeap;
#endif

    bool                            m_stopBurstShot;
    bool                            m_burst[JPEG_SAVE_THREAD_MAX_COUNT];
    bool                            m_running[JPEG_SAVE_THREAD_MAX_COUNT];

    bool                            m_isZSLCaptureOn;

#ifdef SAMSUNG_INF_BURST
    char                            m_burstTime[20];
#endif
#ifdef LLS_CAPTURE
    int                             m_needLLS_history[LLS_HISTORY_COUNT];
#endif

    bool                            m_highResolutionCallbackRunning;
    bool                            m_skipReprocessing;
    bool                            m_resetPreview;

    uint32_t                        m_displayPreviewToggle;
    uint32_t                        m_fdCallbackToggle;

    bool                            m_hdrEnabled;
    unsigned int                    m_hdrSkipedFcount;
    bool                            m_isFirstStart;
    uint32_t                        m_dynamicSccCount;
    uint32_t                        m_dynamicBayerCount;
    bool                            m_isTryStopFlash;
    uint32_t                        m_curMinFps;

    int                             m_visionFps;
    int                             m_visionAe;
    int                             m_ionClient;


    mutable Mutex                   m_metaCopyLock;
    struct camera2_shot_ext         *m_tempshot;
    struct camera2_shot_ext         *m_fdmeta_shot;
    struct camera2_shot_ext         *m_meta_shot;
    struct camera2_shot             *m_picture_meta_shot;
    int                             m_previewBufferCount;
    struct ExynosConfigInfo         *m_exynosconfig;
#if 1
    uint32_t                        m_hackForAlignment;
#endif
    uint32_t                        m_recordingFrameSkipCount;

    int                             m_longExposureCaptureState;
    unsigned int                    m_longExposureRemainCount;
    bool                            m_stopLongExposure;
    bool                            m_cancelPicture;
    bool                            m_currentSetStart;
    bool                            m_flagMetaDataSet;

#ifdef SAMSUNG_LLV
#ifdef SAMSUNG_LLV_TUNING
    int                            m_LLVpowerLevel;
    status_t                       m_checkLLVtuning(void);
#endif
    int                            m_LLVstatus;
    void                           *m_LLVpluginHandle;
#endif

#ifdef SAMSUNG_LENS_DC
    void                            *m_DCpluginHandle;
    bool                            m_skipLensDC;
    int                             m_LensDCIndex;
    void                            m_setLensDCMap(void);
#endif

#ifdef SAMSUNG_OT
    void                           *m_OTpluginHandle;
    UniPluginFocusData_t            m_OTfocusData;
    UniPluginFocusData_t            m_OTpredictedData;
    int                             m_OTstart;
    int                             m_OTstatus;
    bool                            m_OTisTouchAreaGet;
    Mutex                           m_OT_mutex;
    mutable Condition               m_OT_condition;
    bool                            m_OTisWait;
#endif
#ifdef SAMSUNG_HLV
    void                            *m_HLV;
    int                             m_HLVprocessStep;
#endif
#ifdef SAMSUNG_HYPER_MOTION
    int                            m_hyperMotionStep;
#endif
#ifdef SAMSUNG_LBP
    int                            m_LBPindex;
    int                            m_LBPCount;
    void*                          m_LBPpluginHandle;
    bool                           m_isLBPlux;
    bool                           m_isLBPon;
#endif
#ifdef SAMSUNG_DNG
    SecCameraDngCreator             m_dngCreator;
    camera_memory_t                 *m_dngBayerHeap;
    unsigned int                    m_dngFrameNumber;
    char                            m_dngTime[20];
    char                            m_dngSavePath[CAMERA_FILE_PATH_SIZE];
#endif
#ifdef SAMSUNG_JQ
    unsigned char                  m_qtable[128];
    bool                           m_isJpegQtableOn;
    void                           *m_JQpluginHandle;
    Mutex                          m_JQ_mutex;
    mutable Condition              m_JQ_condition;
    bool                           m_JQisWait;
#endif
#ifdef SAMSUNG_BD
    UTstr                           m_BDbuffer[MAX_BD_BUFF_NUM];
    int                             m_BDbufferIndex;
    void                           *m_BDpluginHandle;
    int                             m_BDstatus;
#endif
#ifdef SAMSUNG_LLS_DEBLUR
    void                           *m_LLSpluginHandle;
    int                             m_LDCaptureCount;
    int                             m_LDBufIndex[MAX_LD_CAPTURE_COUNT];
#endif
#ifdef SAMSUNG_OIS_VDIS
    UNI_PLUGIN_OIS_MODE             m_OISvdisMode;
#endif
#if defined(SAMSUNG_DOF) || defined(SUPPORT_MULTI_AF)
    int                             m_lensmoveCount;
#endif
#ifdef SAMSUNG_SENSOR_LISTENER
    sp<mainCameraThread>            m_sensorListenerThread;
    bool                            m_sensorListenerThreadFunc(void);
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
#endif

#ifdef FIRST_PREVIEW_TIME_CHECK
    ExynosCameraDurationTimer       m_firstPreviewTimer;
    bool                            m_flagFirstPreviewTimerOn;
#endif
#ifdef CAMERA_FAST_ENTRANCE_V1
    bool                            m_isFirstParametersSet;
    bool                            m_fastEntrance;
    int                             m_fastenAeThreadResult;
#endif
#ifdef SAMSUNG_QUICKSHOT
    bool                            m_quickShotStart;
#endif
#ifdef USE_CAMERA_PREVIEW_FRAME_SCHEDULER
    sp<SecCameraPreviewFrameScheduler> m_previewFrameScheduler;
#endif
    bool                            m_flagAFDone;
#ifdef SAMSUNG_QUICK_SWITCH
    int                             m_isQuickSwitchState;
#endif
    int                             m_shutterSpeed;
    int                             m_gain;
    int                             m_irLedWidth;
    int                             m_irLedDelay;
    int                             m_irLedCurrent;
    int                             m_irLedOnTime;
};

}; /* namespace android */
#endif
