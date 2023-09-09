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

/* #define LOG_NDEBUG 0 */
#define LOG_TAG "ExynosCamera"
#include <cutils/log.h>

#include "ExynosCamera.h"

#ifdef BURST_CAPTURE
#include <sys/resource.h>
#include <private/android_filesystem_config.h>

#include <ctype.h>
#include <dirent.h>
#endif

#ifdef LLS_CAPTURE
#define MULTI_FRAME_SHOT_PARAMETERS   0xF124
#endif

#ifdef FLASHED_LLS_CAPTURE
#define MULTI_FRAME_SHOT_FLASHED_INFO   0xF125
#endif
#ifdef LLS_REPROCESSING
#define MULTI_FRAME_SHOT_BV_INFO        0xF127
#endif

#ifdef SR_CAPTURE
#define MULTI_FRAME_SHOT_SR_EXTRA_INFO 0xF126
#endif

#ifdef TOUCH_AE
#define AE_RESULT 0xF351
#endif

namespace android {

/* vision */
#ifdef VISION_DUMP
int dumpIndex = 0;
#endif

#ifdef MONITOR_LOG_SYNC
uint32_t ExynosCamera::cameraSyncLogId = 0;
#endif

ExynosCamera::ExynosCamera(int cameraId,  __unused camera_device_t *dev)
{
    ExynosCameraActivityUCTL *uctlMgr = NULL;

    BUILD_DATE();

    checkAndroidVersion();

    m_cameraId = cameraId;
    m_use_companion = isCompanion(cameraId);
    memset(m_name, 0x00, sizeof(m_name));

#ifdef SAMSUNG_COMPANION
    m_companionNode = NULL;
    m_companionThread = NULL;
    CLOGI("INFO(%s):cameraId(%d), use_companion(%d)", __FUNCTION__, cameraId, m_use_companion);

    if(m_use_companion == true) {
        m_companionThread = new mainCameraThread(this, &ExynosCamera::m_companionThreadFunc,
                                                  "companionshotThread", PRIORITY_URGENT_DISPLAY);
        if (m_companionThread != NULL) {
            m_companionThread->run();
            CLOGD("DEBUG(%s): companionThread starts", __FUNCTION__);
        } else {
            CLOGE("(%s):failed the m_companionThread.", __FUNCTION__);
        }
    }
#endif

#if defined(SAMSUNG_EEPROM)
    m_eepromThread = NULL;
    if ((m_use_companion == false) && (isEEprom(getCameraId()) == true)) {
        m_eepromThread = new mainCameraThread(this, &ExynosCamera::m_eepromThreadFunc,
                                                  "cameraeepromThread", PRIORITY_URGENT_DISPLAY);
        if (m_eepromThread != NULL) {
            m_eepromThread->run();
            CLOGD("DEBUG(%s): eepromThread starts", __FUNCTION__);
        } else {
            CLOGE("(%s): failed the m_eepromThread", __FUNCTION__);
        }
    }
#endif

#ifdef SAMSUNG_COMPANION
    m_exynosCameraParameters = new ExynosCameraParameters(m_cameraId, m_use_companion);
#else
    m_exynosCameraParameters = new ExynosCameraParameters(m_cameraId);
#endif
    m_cameraSensorId = getSensorId(m_cameraId);
    CLOGD("DEBUG(%s):Parameters(CameraId=%d / m_cameraSensorId=%d) created", __FUNCTION__, m_cameraId, m_cameraSensorId);

    m_exynosCameraActivityControl = m_exynosCameraParameters->getActivityControl();

    m_previewFrameFactory      = NULL;
    m_reprocessingFrameFactory = NULL;
    /* vision */
    m_visionFrameFactory= NULL;

    m_ionAllocator = NULL;
    m_grAllocator  = NULL;
    m_mhbAllocator = NULL;

    m_frameMgr = NULL;

    mIonClient = ion_open();
    if (mIonClient < 0) {
        ALOGE("ERR(%s):mIonClient ion_open() fail", __func__);
        mIonClient = -1;
    }

    m_createInternalBufferManager(&m_bayerBufferMgr, "BAYER_BUF");
    m_createInternalBufferManager(&m_3aaBufferMgr, "3A1_BUF");
    m_createInternalBufferManager(&m_ispBufferMgr, "ISP_BUF");
    m_createInternalBufferManager(&m_hwDisBufferMgr, "HW_DIS_BUF");
#ifdef SAMSUNG_DNG
    m_createInternalBufferManager(&m_fliteBufferMgr, "FLITE_BUF");
#endif

    /* reprocessing Buffer */
    m_createInternalBufferManager(&m_ispReprocessingBufferMgr, "ISP_RE_BUF");
    m_createInternalBufferManager(&m_sccReprocessingBufferMgr, "SCC_RE_BUF");

    m_createInternalBufferManager(&m_sccBufferMgr, "SCC_BUF");
    m_createInternalBufferManager(&m_gscBufferMgr, "GSC_BUF");
    m_createInternalBufferManager(&m_jpegBufferMgr, "JPEG_BUF");
#ifdef SUPPORT_SW_VDIS
    m_createInternalBufferManager(&m_swVDIS_BufferMgr, "VDIS_BUF");
#endif /*SUPPORT_SW_VDIS*/

    m_createInternalBufferManager(&m_postPictureGscBufferMgr, "POSTPICTURE_GSC_BUF");
#ifdef SAMSUNG_LBP
    m_createInternalBufferManager(&m_lbpBufferMgr, "LBP_BUF");
#endif
    /* preview Buffer */
    m_scpBufferMgr = NULL;
    m_createInternalBufferManager(&m_previewCallbackBufferMgr, "PREVIEW_CB_BUF");
    m_createInternalBufferManager(&m_highResolutionCallbackBufferMgr, "HIGH_RESOLUTION_CB_BUF");
    m_fdCallbackHeap = NULL;
#ifdef SR_CAPTURE
    m_srCallbackHeap = NULL;
    memset(m_faces_sr, 0, sizeof(camera_face_t) * NUM_OF_DETECTED_FACES);
#endif

    /* recording Buffer */
    m_recordingCallbackHeap = NULL;
    m_createInternalBufferManager(&m_recordingBufferMgr, "REC_BUF");

    /* highspeed dzoom Buffer Mgr */
    m_createInternalBufferManager(&m_zoomScalerBufferMgr, "DZOOM_SCALER_BUF");

    m_createThreads();

    m_createInternalBufferManager(&m_thumbnailGscBufferMgr, "THUMBNAIL_GSC_BUF");
    m_ThumbnailPostCallbackQ = new frame_queue_t;
    m_ThumbnailPostCallbackQ->setWaitTime(2000000000);

    m_pipeFrameDoneQ     = new frame_queue_t;
    dstIspReprocessingQ  = new frame_queue_t;
    dstSccReprocessingQ  = new frame_queue_t;
    dstGscReprocessingQ  = new frame_queue_t;
    m_zoomPreviwWithCscQ = new frame_queue_t;

    dstPostPictureGscQ = new frame_queue_t;
    dstPostPictureGscQ->setWaitTime(2000000000);

    dstJpegReprocessingQ = new frame_queue_t;
    /* vision */
    m_pipeFrameVisionDoneQ = new frame_queue_t;

    m_frameFactoryQ = new framefactory_queue_t;
    m_facedetectQ = new frame_queue_t;
    m_facedetectQ->setWaitTime(500000000);

    m_autoFocusContinousQ.setWaitTime(550000000);
    m_pipeFrameDoneQ->setWaitTime(550000000);

    m_previewQ = new frame_queue_t;
    m_previewQ->setWaitTime(500000000);
    m_previewCallbackGscFrameDoneQ = new frame_queue_t;
    m_recordingQ = new frame_queue_t;
    m_recordingQ->setWaitTime(500000000);
    m_postPictureQ = new frame_queue_t(m_postPictureThread);
#ifdef SAMSUNG_DEBLUR
    m_deblurCaptureQ = new frame_queue_t(m_deblurCaptureThread);
#endif
    for(int i = 0 ; i < MAX_NUM_PIPES ; i++ ) {
        m_mainSetupQ[i] = new frame_queue_t;
        m_mainSetupQ[i]->setWaitTime(550000000);
    }
    m_jpegCallbackQ = new jpeg_callback_queue_t;
    m_postviewCallbackQ = new postview_callback_queue_t;
    m_thumbnailCallbackQ = new thumbnail_callback_queue_t;
#ifdef SAMSUNG_DEBLUR
    m_detectDeblurCaptureQ = new deblur_capture_queue_t;
#endif
#ifdef SAMSUNG_LLS_DEBLUR
    m_LDCaptureQ = new frame_queue_t(m_LDCaptureThread);
#endif
#ifdef SAMSUNG_LBP
    m_LBPbufferQ = new lbp_queue_t;
#endif
#ifdef SAMSUNG_DNG
    m_dngCaptureQ = new dng_capture_queue_t;
#endif
#ifdef SAMSUNG_BD
    m_BDbufferQ = new bd_queue_t;
#endif
    for (int threadNum = JPEG_SAVE_THREAD0; threadNum < JPEG_SAVE_THREAD_MAX_COUNT; threadNum++) {
        m_jpegSaveQ[threadNum] = new jpeg_callback_queue_t;
        m_jpegSaveQ[threadNum]->setWaitTime(2000000000);
        m_burst[threadNum] = false;
        m_running[threadNum] = false;
    }

    dstIspReprocessingQ->setWaitTime(20000000);
    dstSccReprocessingQ->setWaitTime(50000000);
    dstGscReprocessingQ->setWaitTime(500000000);
    dstJpegReprocessingQ->setWaitTime(500000000);
    /* vision */
    m_pipeFrameVisionDoneQ->setWaitTime(2000000000);

    m_jpegCallbackQ->setWaitTime(1000000000);
#ifdef SAMSUNG_LLS_DEBLUR
    m_postviewCallbackQ->setWaitTime(2000000000);
#else
    m_postviewCallbackQ->setWaitTime(1000000000);
#endif
    m_thumbnailCallbackQ->setWaitTime(1000000000);
#ifdef SAMSUNG_DEBLUR
    m_detectDeblurCaptureQ->setWaitTime(1000000000);
#endif
#ifdef SAMSUNG_LBP
    m_LBPbufferQ->setWaitTime(1000000000); //1sec
#endif
#ifdef SAMSUNG_DNG
    m_dngCaptureQ->setWaitTime(1000000000);
#endif
#ifdef SAMSUNG_BD
    m_BDbufferQ->setWaitTime(1000000000); //1sec
#endif
    memset(&m_frameMetadata, 0, sizeof(camera_frame_metadata_t));
    memset(m_faces, 0, sizeof(camera_face_t) * NUM_OF_DETECTED_FACES);

    m_exitAutoFocusThread = false;
    m_autoFocusRunning    = false;
    m_previewEnabled   = false;
    m_pictureEnabled   = false;
    m_recordingEnabled = false;
    m_zslPictureEnabled   = false;
    m_flagStartFaceDetection = false;
    m_flagLLSStart = false;
    m_flagLightCondition = false;
#ifdef SAMSUNG_CAMERA_EXTRA_INFO
    m_flagFlashCallback = false;
    m_flagHdrCallback = false;
#endif
    m_fdThreshold = 0;
    m_captureSelector = NULL;
    m_sccCaptureSelector = NULL;
    m_autoFocusType = 0;
    m_hdrEnabled = false;
    m_doCscRecording = true;
    m_recordingBufferCount = NUM_RECORDING_BUFFERS;
    m_frameSkipCount = 0;
    m_isZSLCaptureOn = false;
    m_isSuccessedBufferAllocation = false;
#ifdef BURST_CAPTURE
    m_burstCaptureCallbackCount = 0;
    m_burstShutterLocation = BURST_SHUTTER_PREPICTURE;
#endif
    m_skipCount = 0;

#ifdef SAMSUNG_LLV
#ifdef SAMSUNG_LLV_TUNING
    m_LLVpowerLevel = UNI_PLUGIN_POWER_CONTROL_OFF;
    m_checkLLVtuning();
#endif
    m_LLVstatus = LLV_UNINIT;
    m_LLVpluginHandle = uni_plugin_load(LLV_PLUGIN_NAME);
    if(m_LLVpluginHandle == NULL) {
        CLOGE("[LLV](%s[%d]): LLV plugin load failed!!", __FUNCTION__, __LINE__);
    }
#endif

#ifdef SAMSUNG_OT
    m_OTstatus = UNI_PLUGIN_OBJECT_TRACKING_IDLE;
    m_OTstart = OBJECT_TRACKING_IDLE;
    m_OTisTouchAreaGet = false;
    m_OTisWait = false;
    m_OTpluginHandle = uni_plugin_load(OBJECT_TRACKING_PLUGIN_NAME);
    if(m_OTpluginHandle == NULL) {
        CLOGE("[OBTR](%s[%d]): Object Tracking plugin load failed!!", __FUNCTION__, __LINE__);
    }
#endif

#ifdef SAMSUNG_LBP
    m_LBPindex = 0;
    m_LBPCount = 0;
    m_isLBPlux = false;
    m_isLBPon = false;
    m_LBPpluginHandle = uni_plugin_load(BESTPHOTO_PLUGIN_NAME);
    if(m_LBPpluginHandle == NULL) {
        CLOGE("[LBP](%s[%d]):Bestphoto plugin load failed!!", __FUNCTION__, __LINE__);
    }
#endif

#ifdef SAMSUNG_JQ
    m_isJpegQtableOn = false;
    m_JQpluginHandle = uni_plugin_load(JPEG_QTABLE_PLUGIN_NAME);
    if(m_JQpluginHandle == NULL) {
        CLOGE("[JQ](%s[%d]):JpegQtable plugin load failed!!", __FUNCTION__, __LINE__);
    }
#endif

#ifdef SAMSUNG_BD
    m_BDpluginHandle = uni_plugin_load(BLUR_DETECTION_PLUGIN_NAME);
    if(m_BDpluginHandle == NULL) {
        CLOGE("[BD](%s[%d]):BlurDetection plugin load failed!!", __FUNCTION__, __LINE__);
    }
    m_BDstatus = BLUR_DETECTION_IDLE;
#endif

#ifdef SAMSUNG_DEBLUR
    m_DeblurpluginHandle = uni_plugin_load(DEBLUR_PLUGIN_NAME);
    if(m_DeblurpluginHandle == NULL) {
        CLOGE("[Deblur](%s[%d]):Deblur plugin load failed!!", __FUNCTION__, __LINE__);
    }
#endif

#ifdef SAMSUNG_LLS_DEBLUR
    m_LLSpluginHandle = uni_plugin_load(LLS_PLUGIN_NAME);
    if(m_LLSpluginHandle == NULL) {
        CLOGE("[LDC](%s[%d]):LLS plugin load failed!!", __FUNCTION__, __LINE__);
    }
#endif

#ifdef SAMSUNG_HLV
    m_HLV = NULL;
    m_HLVprocessStep = HLV_PROCESS_DONE;
#endif

#ifdef SAMSUNG_DOF
    m_lensmoveCount = 0;
    m_AFstatus = ExynosCameraActivityAutofocus::AUTOFOCUS_STATE_NONE;
    m_currentSetStart = false;
    m_flagMetaDataSet = false;
#endif
#ifdef FPS_CHECK
    for (int i = 0; i < DEBUG_MAX_PIPE_NUM; i++)
        m_debugFpsCount[i] = 0;
#endif

    m_stopBurstShot = false;
    m_disablePreviewCB = false;
    m_checkFirstFrameLux = false;

#ifdef LLS_CAPTURE
    for (int i = 0; i < LLS_HISTORY_COUNT; i++)
        m_needLLS_history[i] = 0;
#endif
    m_callbackState = 0;
    m_callbackStateOld = 0;
    m_callbackMonitorCount = 0;

    m_highResolutionCallbackRunning = false;
    m_highResolutionCallbackQ = new frame_queue_t(m_highResolutionCallbackThread);
    m_highResolutionCallbackQ->setWaitTime(500000000);
    m_skipReprocessing = false;
    m_isFirstStart = true;
    m_exynosCameraParameters->setIsFirstStartFlag(m_isFirstStart);

#ifdef RAWDUMP_CAPTURE
    m_RawCaptureDumpQ = new frame_queue_t(m_RawCaptureDumpThread);
#endif

#ifdef SAMSUNG_MAGICSHOT
    if(getCameraId() == CAMERA_ID_BACK) {
        m_magicshotMaxCount = MAGICSHOT_COUNT_REAR;
    } else {
        m_magicshotMaxCount = MAGICSHOT_COUNT_FRONT;
    }
#endif

#ifdef OIS_CAPTURE
    ExynosCameraActivitySpecialCapture *m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();
    m_sCaptureMgr->resetOISCaptureFcount();

    m_OISCaptureShutterEnabled = false;
#endif

#ifdef RAWDUMP_CAPTURE
    ExynosCameraActivitySpecialCapture *m_sCapture = m_exynosCameraActivityControl->getSpecialCaptureMgr();
    m_sCapture->resetRawCaptureFcount();
#endif

#ifdef SAMSUNG_LLS_DEBLUR
    m_LDCaptureCount = 0;

    for (int i = 0; i < MAX_LD_CAPTURE_COUNT; i++) {
        m_LDBufIndex[i] = 0;
    }
#endif

    m_exynosconfig = NULL;
    m_setConfigInform();

    m_setFrameManager();


    /* HACK Reset Preview Flag*/
    m_resetPreview = false;

    m_dynamicSccCount = 0;
    m_previewBufferCount = NUM_PREVIEW_BUFFERS;

#ifdef FIRST_PREVIEW_TIME_CHECK
    m_flagFirstPreviewTimerOn = false;
#endif

    /* init infomation of fd orientation*/
    m_exynosCameraParameters->setDeviceOrientation(0);
    uctlMgr = m_exynosCameraActivityControl->getUCTLMgr();
    if (uctlMgr != NULL)
        uctlMgr->setDeviceRotation(m_exynosCameraParameters->getFdOrientation());
#ifdef MONITOR_LOG_SYNC
    m_syncLogDuration = 0;
#endif
#ifdef SAMSUNG_LBP
    if(getCameraId() == CAMERA_ID_FRONT) {
        m_exynosCameraParameters->resetNormalBestFrameCount();
        m_exynosCameraParameters->resetSCPFrameCount();
        m_exynosCameraParameters->resetBayerFrameCount();
    }
#endif
#ifdef SAMSUNG_HRM
    m_uv_rayHandle = NULL;
#endif
#ifdef SAMSUNG_LIGHT_IR
    m_light_irHandle = NULL;
#endif
#ifdef SAMSUNG_GYRO
    m_gyroHandle = NULL;
#endif

#ifdef ONE_SECOND_BURST_CAPTURE
    /* init one second burst capture parameters. */
    for (int i = 0; i < ONE_SECOND_BURST_CAPTURE_CHECK_COUNT; i++)
        TakepictureDurationTime[i] = 0;
    m_one_second_burst_capture = false;
    m_one_second_burst_first_after_open = false;
    m_one_second_jpegCallbackHeap = NULL;
    m_one_second_postviewCallbackHeap = NULL;
#endif
    m_initFrameFactory();
#ifdef SAMSUNG_QUICKSHOT
    m_quickShotStart = true;
#endif

    m_tempshot = new struct camera2_shot_ext;
    m_fdmeta_shot = new struct camera2_shot_ext;
    m_meta_shot  = new struct camera2_shot_ext;

#ifdef SUPPORT_SW_VDIS
    m_swVDIS_fd_dm = new struct camera2_dm;
    m_swVDIS_FaceData = new struct swVDIS_FaceData;
#endif /*SUPPORT_SW_VDIS*/

#ifdef USE_CAMERA_PREVIEW_FRAME_SCHEDULER
    m_previewFrameScheduler = new SecCameraPreviewFrameScheduler();
#endif

#ifdef CAMERA_FAST_ENTRANCE_V1
    m_fastEntrance = false;
    m_isFirstParametersSet = false;
#endif

#ifdef SAMSUNG_DNG
    m_searchDNGFrame = false;
    m_dngFrameNumber = 0;
    m_longExposureEnds = true;
    m_dngLongExposureRemainCount = 0;
    m_dngFrameNumberForManualExposure = 0;
#endif
    m_longExposureRemainCount = 0;
    m_longExposurePreview = false;
    m_stopLongExposure = false;
    m_cancelPicture = false;

#ifdef SAMSUNG_DEBLUR
    m_useDeblurCaptureOn = false;
    m_deblurCaptureCount = 0;
#endif
    m_previewWindow = NULL;
}

status_t  ExynosCamera::m_setConfigInform() {
    struct ExynosConfigInfo exynosConfig;
    memset((void *)&exynosConfig, 0x00, sizeof(exynosConfig));

    exynosConfig.mode = CONFIG_MODE::NORMAL;

    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_bayer_buffers = (getCameraId() == CAMERA_ID_BACK) ? NUM_BAYER_BUFFERS : FRONT_NUM_BAYER_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.init_bayer_buffers = INIT_BAYER_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_3aa_buffers = (getCameraId() == CAMERA_ID_BACK) ? NUM_3AA_BUFFERS : FRONT_NUM_3AA_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_hwdis_buffers = NUM_HW_DIS_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_preview_buffers = NUM_PREVIEW_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_picture_buffers = (getCameraId() == CAMERA_ID_BACK) ? NUM_PICTURE_BUFFERS : FRONT_NUM_PICTURE_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_reprocessing_buffers = NUM_REPROCESSING_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_recording_buffers = NUM_RECORDING_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_fastaestable_buffer = INITIAL_SKIP_FRAME;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.reprocessing_bayer_hold_count = REPROCESSING_BAYER_HOLD_COUNT;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.preview_buffer_margin = NUM_PREVIEW_BUFFERS_MARGIN;

    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_FLITE] = (getCameraId() == CAMERA_ID_BACK) ? PIPE_FLITE_PREPARE_COUNT : PIPE_FLITE_FRONT_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_3AC] = PIPE_3AC_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_3AA] = PIPE_3AA_ISP_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_3AA_ISP] = PIPE_3AA_ISP_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_ISP] = PIPE_ISP_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_ISPC] = (getCameraId() == CAMERA_ID_BACK) ? PIPE_3AA_ISP_PREPARE_COUNT : PIPE_FLITE_FRONT_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_SCC] = (getCameraId() == CAMERA_ID_BACK) ? PIPE_3AA_ISP_PREPARE_COUNT : PIPE_FLITE_FRONT_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_SCP] = (getCameraId() == CAMERA_ID_BACK) ? PIPE_SCP_PREPARE_COUNT : PIPE_SCP_FRONT_PREPARE_COUNT;

    /* reprocessing */
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_SCP_REPROCESSING] = PIPE_SCP_REPROCESSING_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_ISPC_REPROCESSING] = PIPE_SCC_REPROCESSING_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_SCC_REPROCESSING] = PIPE_SCC_REPROCESSING_PREPARE_COUNT;

#if (USE_HIGHSPEED_RECORDING)
    /* Config HIGH_SPEED 60 buffer & pipe info */
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_bayer_buffers = (getCameraId() == CAMERA_ID_BACK) ? FPS60_NUM_NUM_BAYER_BUFFERS : FPS60_FRONT_NUM_BAYER_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.init_bayer_buffers = FPS60_NUM_NUM_BAYER_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_3aa_buffers = FPS60_NUM_NUM_BAYER_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_hwdis_buffers = FPS60_NUM_HW_DIS_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_preview_buffers = FPS60_NUM_PREVIEW_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_picture_buffers = (getCameraId() == CAMERA_ID_BACK) ? FPS60_NUM_PICTURE_BUFFERS : FPS60_FRONT_NUM_PICTURE_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_reprocessing_buffers = FPS60_NUM_REPROCESSING_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_recording_buffers = FPS60_NUM_RECORDING_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_fastaestable_buffer = FPS60_INITIAL_SKIP_FRAME;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.reprocessing_bayer_hold_count = FPS60_REPROCESSING_BAYER_HOLD_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.preview_buffer_margin = FPS60_NUM_PREVIEW_BUFFERS_MARGIN;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_FLITE] = FPS60_PIPE_FLITE_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_3AC] = FPS60_PIPE_3AC_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_3AA] = FPS60_PIPE_3AA_ISP_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_3AA_ISP] = FPS60_PIPE_3AA_ISP_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_SCP] = FPS60_PIPE_SCP_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_SCP_REPROCESSING] = FPS60_PIPE_SCP_REPROCESSING_PREPARE_COUNT;

    /* Config HIGH_SPEED 120 buffer & pipe info */
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_bayer_buffers = (getCameraId() == CAMERA_ID_BACK) ? FPS120_NUM_NUM_BAYER_BUFFERS : FPS120_FRONT_NUM_BAYER_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.init_bayer_buffers = FPS120_NUM_NUM_BAYER_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_3aa_buffers = FPS120_NUM_NUM_BAYER_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_hwdis_buffers = FPS120_NUM_HW_DIS_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_preview_buffers = FPS120_NUM_PREVIEW_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_picture_buffers = (getCameraId() == CAMERA_ID_BACK) ? FPS120_NUM_PICTURE_BUFFERS : FPS120_FRONT_NUM_PICTURE_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_reprocessing_buffers = FPS120_NUM_REPROCESSING_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_recording_buffers = FPS120_NUM_RECORDING_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_fastaestable_buffer = FPS120_INITIAL_SKIP_FRAME;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.reprocessing_bayer_hold_count = FPS120_REPROCESSING_BAYER_HOLD_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.preview_buffer_margin = FPS120_NUM_PREVIEW_BUFFERS_MARGIN;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].pipeInfo.prepare[PIPE_FLITE] = FPS120_PIPE_FLITE_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].pipeInfo.prepare[PIPE_3AC] = FPS120_PIPE_3AC_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].pipeInfo.prepare[PIPE_3AA] = FPS120_PIPE_3AA_ISP_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].pipeInfo.prepare[PIPE_3AA_ISP] = FPS120_PIPE_3AA_ISP_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].pipeInfo.prepare[PIPE_SCP] = FPS120_PIPE_SCP_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].pipeInfo.prepare[PIPE_SCP_REPROCESSING] = FPS120_PIPE_SCP_REPROCESSING_PREPARE_COUNT;

    /* Config HIGH_SPEED 240 buffer & pipe info */
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_bayer_buffers = (getCameraId() == CAMERA_ID_BACK) ? FPS240_NUM_NUM_BAYER_BUFFERS : FPS240_FRONT_NUM_BAYER_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.init_bayer_buffers = FPS240_INIT_BAYER_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_3aa_buffers = FPS240_NUM_NUM_BAYER_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_hwdis_buffers = FPS240_NUM_HW_DIS_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_preview_buffers = FPS240_NUM_PREVIEW_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_picture_buffers = (getCameraId() == CAMERA_ID_BACK) ? FPS240_NUM_PICTURE_BUFFERS : FPS240_FRONT_NUM_PICTURE_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_reprocessing_buffers = FPS240_NUM_REPROCESSING_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_recording_buffers = FPS240_NUM_RECORDING_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_fastaestable_buffer = FPS240_INITIAL_SKIP_FRAME;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.reprocessing_bayer_hold_count = FPS240_REPROCESSING_BAYER_HOLD_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.preview_buffer_margin = FPS240_NUM_PREVIEW_BUFFERS_MARGIN;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].pipeInfo.prepare[PIPE_FLITE] = FPS240_PIPE_FLITE_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].pipeInfo.prepare[PIPE_3AC] = FPS240_PIPE_3AC_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].pipeInfo.prepare[PIPE_3AA] = FPS240_PIPE_3AA_ISP_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].pipeInfo.prepare[PIPE_3AA_ISP] = FPS240_PIPE_3AA_ISP_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].pipeInfo.prepare[PIPE_SCP] = FPS240_PIPE_SCP_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].pipeInfo.prepare[PIPE_SCP_REPROCESSING] = FPS240_PIPE_SCP_REPROCESSING_PREPARE_COUNT;
#endif

    m_exynosCameraParameters->setConfig(&exynosConfig);

    m_exynosconfig = m_exynosCameraParameters->getConfig();

    return NO_ERROR;
}

void ExynosCamera::m_createThreads(void)
{
    m_mainThread = new mainCameraThread(this, &ExynosCamera::m_mainThreadFunc, "ExynosCameraThread", PRIORITY_URGENT_DISPLAY);
    CLOGD("DEBUG(%s):mainThread created", __FUNCTION__);

    m_previewThread = new mainCameraThread(this, &ExynosCamera::m_previewThreadFunc, "previewThread", PRIORITY_DISPLAY);
    CLOGD("DEBUG(%s):previewThread created", __FUNCTION__);

    /*
     * In here, we cannot know single, dual scenario.
     * So, make all threads.
     */
    /* if (m_exynosCameraParameters->isFlite3aaOtf() == true) { */
    if (1) {
        m_mainSetupQThread[INDEX(PIPE_FLITE)] = new mainCameraThread(this, &ExynosCamera::m_mainThreadQSetupFLITE, "mainThreadQSetupFLITE", PRIORITY_URGENT_DISPLAY);
        CLOGD("DEBUG(%s):mainThreadQSetupFLITEThread created", __FUNCTION__);

/* Change 3AA_ISP, 3AC, SCP to ISP */
/*
        m_mainSetupQThread[INDEX(PIPE_3AC)] = new mainCameraThread(this, &ExynosCamera::m_mainThreadQSetup3AC, "mainThreadQSetup3AC", PRIORITY_URGENT_DISPLAY);
        CLOGD("DEBUG(%s):mainThreadQSetup3ACThread created", __FUNCTION__);

        m_mainSetupQThread[INDEX(PIPE_3AA_ISP)] = new mainCameraThread(this, &ExynosCamera::m_mainThreadQSetup3AA_ISP, "mainThreadQSetup3AA_ISP", PRIORITY_URGENT_DISPLAY);
        CLOGD("DEBUG(%s):mainThreadQSetup3AA_ISPThread created", __FUNCTION__);

        m_mainSetupQThread[INDEX(PIPE_ISP)] = new mainCameraThread(this, &ExynosCamera::m_mainThreadQSetupISP, "mainThreadQSetupISP", PRIORITY_URGENT_DISPLAY);
        CLOGD("DEBUG(%s):mainThreadQSetupISPThread created", __FUNCTION__);

        m_mainSetupQThread[INDEX(PIPE_SCP)] = new mainCameraThread(this, &ExynosCamera::m_mainThreadQSetupSCP, "mainThreadQSetupSCP", PRIORITY_URGENT_DISPLAY);
        CLOGD("DEBUG(%s):mainThreadQSetupSCPThread created", __FUNCTION__);
*/

        m_mainSetupQThread[INDEX(PIPE_3AA)] = new mainCameraThread(this, &ExynosCamera::m_mainThreadQSetup3AA, "mainThreadQSetup3AA", PRIORITY_URGENT_DISPLAY);
        CLOGD("DEBUG(%s):mainThreadQSetup3AAThread created", __FUNCTION__);

    }
    m_setBuffersThread = new mainCameraThread(this, &ExynosCamera::m_setBuffersThreadFunc, "setBuffersThread");
    CLOGD("DEBUG(%s):setBuffersThread created", __FUNCTION__);

    m_startPictureInternalThread = new mainCameraThread(this, &ExynosCamera::m_startPictureInternalThreadFunc, "startPictureInternalThread");
    CLOGD("DEBUG(%s):startPictureInternalThread created", __FUNCTION__);

    m_startPictureBufferThread = new mainCameraThread(this, &ExynosCamera::m_startPictureBufferThreadFunc, "startPictureBufferThread");
    CLOGD("DEBUG(%s):startPictureBufferThread created", __FUNCTION__);

    m_prePictureThread = new mainCameraThread(this, &ExynosCamera::m_prePictureThreadFunc, "prePictureThread");
    CLOGD("DEBUG(%s):prePictureThread created", __FUNCTION__);

    m_pictureThread = new mainCameraThread(this, &ExynosCamera::m_pictureThreadFunc, "PictureThread");
    CLOGD("DEBUG(%s):pictureThread created", __FUNCTION__);

    m_postPictureThread = new mainCameraThread(this, &ExynosCamera::m_postPictureThreadFunc, "postPictureThread");
    CLOGD("DEBUG(%s):postPictureThread created", __FUNCTION__);

    m_recordingThread = new mainCameraThread(this, &ExynosCamera::m_recordingThreadFunc, "recordingThread");
    CLOGD("DEBUG(%s):recordingThread created", __FUNCTION__);

    m_autoFocusThread = new mainCameraThread(this, &ExynosCamera::m_autoFocusThreadFunc, "AutoFocusThread");
    CLOGD("DEBUG(%s):autoFocusThread created", __FUNCTION__);

    m_autoFocusContinousThread = new mainCameraThread(this, &ExynosCamera::m_autoFocusContinousThreadFunc, "AutoFocusContinousThread");
    CLOGD("DEBUG(%s):autoFocusContinousThread created", __FUNCTION__);

    m_facedetectThread = new mainCameraThread(this, &ExynosCamera::m_facedetectThreadFunc, "FaceDetectThread");
    CLOGD("DEBUG(%s):FaceDetectThread created", __FUNCTION__);

    m_monitorThread = new mainCameraThread(this, &ExynosCamera::m_monitorThreadFunc, "monitorThread");
    CLOGD("DEBUG(%s):monitorThread created", __FUNCTION__);

    m_framefactoryThread = new mainCameraThread(this, &ExynosCamera::m_frameFactoryInitThreadFunc, "FrameFactoryInitThread");
    CLOGD("DEBUG(%s):FrameFactoryInitThread created", __FUNCTION__);

    m_jpegCallbackThread = new mainCameraThread(this, &ExynosCamera::m_jpegCallbackThreadFunc, "jpegCallbackThread");
    CLOGD("DEBUG(%s):jpegCallbackThread created", __FUNCTION__);

    m_zoomPreviwWithCscThread = new mainCameraThread(this, &ExynosCamera::m_zoomPreviwWithCscThreadFunc, "zoomPreviwWithCscQThread");
    CLOGD("DEBUG(%s):zoomPreviwWithCscQThread created", __FUNCTION__);

    m_postPictureGscThread = new mainCameraThread(this, &ExynosCamera::m_postPictureGscThreadFunc, "m_postPictureGscThread");
    CLOGD("DEBUG(%s): m_postPictureGscThread created", __FUNCTION__);

#ifdef SAMSUNG_LBP
    m_LBPThread = new mainCameraThread(this, &ExynosCamera::m_LBPThreadFunc, "LBPThread");
    CLOGD("DEBUG(%s): LBPThread created", __FUNCTION__);
#endif
#ifdef SAMSUNG_DNG
    /* m_DNGCaptureThread */
    m_DNGCaptureThread = new mainCameraThread(this, &ExynosCamera::m_DNGCaptureThreadFunc, "m_DNGCaptureThread");
    CLOGD("DEBUG(%s):m_DNGCaptureThread created", __FUNCTION__);
#endif

#ifdef SAMSUNG_SENSOR_LISTENER
    m_sensorListenerThread = new mainCameraThread(this, &ExynosCamera::m_sensorListenerThreadFunc, "sensorListenerThread");
    CLOGD("DEBUG(%s):m_sensorListenerThread created", __FUNCTION__);
#endif

#ifdef SAMSUNG_LLS_DEBLUR
    /* m_LDCapture Thread */
    m_LDCaptureThread = new mainCameraThread(this, &ExynosCamera::m_LDCaptureThreadFunc, "m_LDCaptureThread");
    CLOGD("DEBUG(%s):m_LDCaptureThread created", __FUNCTION__);
#endif

    /* saveThread */
    char threadName[20];
    for (int threadNum = JPEG_SAVE_THREAD0; threadNum < JPEG_SAVE_THREAD_MAX_COUNT; threadNum++) {
        snprintf(threadName, sizeof(threadName), "jpegSaveThread%d", threadNum);
        m_jpegSaveThread[threadNum] = new mainCameraThread(this, &ExynosCamera::m_jpegSaveThreadFunc, threadName);
        CLOGD("DEBUG(%s):%s created", __FUNCTION__, threadName);
    }

    /* high resolution preview callback Thread */
    m_highResolutionCallbackThread = new mainCameraThread(this, &ExynosCamera::m_highResolutionCallbackThreadFunc, "m_highResolutionCallbackThread");
    CLOGD("DEBUG(%s):highResolutionCallbackThread created", __FUNCTION__);

    /* vision */
    m_visionThread = new mainCameraThread(this, &ExynosCamera::m_visionThreadFunc, "VisionThread", PRIORITY_URGENT_DISPLAY);
    CLOGD("DEBUG(%s):visionThread created", __FUNCTION__);

    /* Shutter callback */
    m_shutterCallbackThread = new mainCameraThread(this, &ExynosCamera::m_shutterCallbackThreadFunc, "shutterCallbackThread");
    CLOGD("DEBUG(%s):shutterCallbackThread created", __FUNCTION__);

    /* m_ThumbnailCallback Thread */
    m_ThumbnailCallbackThread = new mainCameraThread(this, &ExynosCamera::m_ThumbnailCallbackThreadFunc, "m_ThumbnailCallbackThread");
    CLOGD("DEBUG(%s):m_ThumbnailCallbackThread created", __FUNCTION__);

#ifdef SAMSUNG_DEBLUR
    /* m_deblurCapture Thread */
    m_deblurCaptureThread = new mainCameraThread(this, &ExynosCamera::m_deblurCaptureThreadFunc, "m_deblurCaptureThread");
    CLOGD("DEBUG(%s):m_deblurCaptureThread created", __FUNCTION__);

    /* m_detectDeblurCapture Thread */
    m_detectDeblurCaptureThread = new mainCameraThread(this, &ExynosCamera::m_detectDeblurCaptureThreadFunc, "m_detectDeblurCaptureThread");
    CLOGD("DEBUG(%s):m_detectDeblurCaptureThread created", __FUNCTION__);
#endif

#ifdef RAWDUMP_CAPTURE
    /* RawCaptureDump Thread */
    m_RawCaptureDumpThread = new mainCameraThread(this, &ExynosCamera::m_RawCaptureDumpThreadFunc, "m_RawCaptureDumpThread");
    CLOGD("DEBUG(%s):RawCaptureDumpThread created", __FUNCTION__);
#endif

#ifdef CAMERA_FAST_ENTRANCE_V1
    m_fastenAeThread = new mainCameraThread(this, &ExynosCamera::m_fastenAeThreadFunc, "fastenAeThread");
    CLOGD("DEBUG(%s):m_fastenAeThread created", __FUNCTION__);
#endif
}

status_t ExynosCamera::m_setupFrameFactory(void)
{
    CLOGI("INFO(%s[%d]): -IN-", __FUNCTION__, __LINE__);

    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = NO_ERROR;

    if (m_exynosCameraParameters->getVisionMode() == true) {
        /* about vision */
        if( m_frameFactory[FRAME_FACTORY_TYPE_VISION] == NULL) {
            m_frameFactory[FRAME_FACTORY_TYPE_VISION] = new ExynosCameraFrameFactoryVision(m_cameraId, m_exynosCameraParameters);
            m_frameFactory[FRAME_FACTORY_TYPE_VISION]->setFrameManager(m_frameMgr);
        }
        m_visionFrameFactory = m_frameFactory[FRAME_FACTORY_TYPE_VISION];

        if (m_frameFactory[FRAME_FACTORY_TYPE_VISION] != NULL && m_frameFactory[FRAME_FACTORY_TYPE_VISION]->isCreated() == false) {
            CLOGD("DEBUG(%s[%d]):setupFrameFactory pushProcessQ(%d)", __FUNCTION__, __LINE__, FRAME_FACTORY_TYPE_VISION);
            m_frameFactoryQ->pushProcessQ(&m_frameFactory[FRAME_FACTORY_TYPE_VISION]);
        }
    } else {
        ExynosCameraFrameFactory *curPreviewFrameFactory = m_previewFrameFactory;

        /* about preview */
        if (m_exynosCameraParameters->getDualMode() == true) {
            m_previewFrameFactory      = m_frameFactory[FRAME_FACTORY_TYPE_DUAL_PREVIEW];
        } else if (m_exynosCameraParameters->getTpuEnabledMode() == true) {
            if (m_exynosCameraParameters->is3aaIspOtf() == true)
                m_previewFrameFactory  = m_frameFactory[FRAME_FACTORY_TYPE_3AA_ISP_OTF_TPU];
            else
                m_previewFrameFactory  = m_frameFactory[FRAME_FACTORY_TYPE_3AA_ISP_M2M_TPU];
        } else {
            if (m_exynosCameraParameters->is3aaIspOtf() == true)
                m_previewFrameFactory  = m_frameFactory[FRAME_FACTORY_TYPE_3AA_ISP_OTF];
            else
                m_previewFrameFactory  = m_frameFactory[FRAME_FACTORY_TYPE_3AA_ISP_M2M];
        }

        /*
         * (by h/w path changing scenario) frameFactory will be changed
         * we must destroy these.(to close device)
         * 1. reprocessing frameFactory
         * 2. preview framFactory
         * 3. companion
         */
        if (curPreviewFrameFactory != NULL &&
            curPreviewFrameFactory != m_previewFrameFactory) {
            CLOGD("DEBUG(%s[%d]):previewFrameFactory is changed. so, destroy all frameFactory", __FUNCTION__, __LINE__);

            if (m_reprocessingFrameFactory != NULL &&
                m_reprocessingFrameFactory->isCreated() == true) {
                CLOGD("DEBUG(%s[%d]):destory old reprocessing FrameFactory", __FUNCTION__, __LINE__);
                m_reprocessingFrameFactory->destroy();
            }

            if (curPreviewFrameFactory->isCreated() == true) {
                CLOGD("DEBUG(%s[%d]):destory old frameFactory", __FUNCTION__, __LINE__);
                curPreviewFrameFactory->destroy();

                m_stopCompanion();

                m_startCompanion();
                /* start CompanionThread. startPreview()'s m_waitCompanionThreadEnd() will wait */
                //m_waitCompanionThreadEnd();
            }
        }

        /* find previewFrameFactory and push */
        for (int i = 0; i < FRAME_FACTORY_TYPE_MAX; i++) {
            if (m_previewFrameFactory == m_frameFactory[i]) {
                if (m_frameFactory[i] != NULL && m_frameFactory[i]->isCreated() == false) {
                    CLOGD("DEBUG(%s[%d]):setupFrameFactory pushProcessQ(%d)", __FUNCTION__, __LINE__, i);
                    m_frameFactoryQ->pushProcessQ(&m_frameFactory[i]);
                }
                break;
            }
        }

        /* about reprocessing */
        if (m_exynosCameraParameters->isReprocessing() == true) {
            m_reprocessingFrameFactory = m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING];

            if (m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING] != NULL && m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING]->isCreated() == false) {
                CLOGD("DEBUG(%s[%d]):setupFrameFactory pushProcessQ(%d)", __FUNCTION__, __LINE__, FRAME_FACTORY_TYPE_REPROCESSING);
                m_frameFactoryQ->pushProcessQ(&m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING]);
            }
        }
    }

    /*
     * disable until multi-instace is possible.
     */
    /*
    for (int i = 0; i < FRAME_FACTORY_TYPE_MAX; i++) {
        if (m_frameFactory[i] != NULL && m_frameFactory[i]->isCreated() == false) {
            CLOGD("DEBUG(%s[%d]):setupFrameFactory pushProcessQ(%d)", __FUNCTION__, __LINE__, i);
            m_frameFactoryQ->pushProcessQ(&m_frameFactory[i]);
        } else {
            CLOGD("DEBUG(%s[%d]):setupFrameFactory no Push(%d)", __FUNCTION__, __LINE__, i);
        }
    }
    */

    CLOGI("INFO(%s[%d]): -OUT-", __FUNCTION__, __LINE__);

    return ret;
}

status_t ExynosCamera::m_initFrameFactory(void)
{
    CLOGI("INFO(%s[%d]): -IN-", __FUNCTION__, __LINE__);

    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    status_t ret = NO_ERROR;
    ExynosCameraFrameFactory *factory = NULL;

    m_previewFrameFactory = NULL;
    m_pictureFrameFactory = NULL;
    m_reprocessingFrameFactory = NULL;
    m_visionFrameFactory = NULL;

    for(int i = 0; i < FRAME_FACTORY_TYPE_MAX; i++)
        m_frameFactory[i] = NULL;

    /*
     * new all FrameFactories.
     * because this called on open(). so we don't know current scenario
     */

    factory = new ExynosCameraFrameFactory3aaIspTpu(m_cameraId, m_exynosCameraParameters);
    m_frameFactory[FRAME_FACTORY_TYPE_3AA_ISP_M2M] = factory;
    /* hack : for dual */
    if (getCameraId() == CAMERA_ID_FRONT) {
        factory = new ExynosCameraFrameFactoryFront(m_cameraId, m_exynosCameraParameters);
        m_frameFactory[FRAME_FACTORY_TYPE_DUAL_PREVIEW] = factory;
    } else {
        factory =  m_frameFactory[FRAME_FACTORY_TYPE_3AA_ISP_M2M];
        m_frameFactory[FRAME_FACTORY_TYPE_DUAL_PREVIEW] = factory;
    }

    factory =  m_frameFactory[FRAME_FACTORY_TYPE_3AA_ISP_M2M];
    m_frameFactory[FRAME_FACTORY_TYPE_3AA_ISP_M2M_TPU] = factory;

    factory = new ExynosCameraFrameFactory3aaIspOtf(m_cameraId, m_exynosCameraParameters);
    m_frameFactory[FRAME_FACTORY_TYPE_3AA_ISP_OTF] = factory;

    factory = new ExynosCameraFrameFactory3aaIspOtfTpu(m_cameraId, m_exynosCameraParameters);
    m_frameFactory[FRAME_FACTORY_TYPE_3AA_ISP_OTF_TPU] = factory;

    factory = new ExynosCameraFrameReprocessingFactory(m_cameraId, m_exynosCameraParameters);
    m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING] = factory;

    for (int i = 0 ; i < FRAME_FACTORY_TYPE_MAX ; i++) {
        factory = m_frameFactory[i];
        if( factory != NULL )
            factory->setFrameManager(m_frameMgr);
    }

    CLOGI("INFO(%s[%d]): -OUT-", __FUNCTION__, __LINE__);

    return ret;
}

status_t ExynosCamera::m_deinitFrameFactory(void)
{
    CLOGI("INFO(%s[%d]): -IN-", __FUNCTION__, __LINE__);

    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = NO_ERROR;
    ExynosCameraFrameFactory *frameFactory = NULL;

    for (int i = 0; i < FRAME_FACTORY_TYPE_MAX; i++) {
        if (m_frameFactory[i] != NULL) {
            frameFactory = m_frameFactory[i];

            for (int k = i + 1; k < FRAME_FACTORY_TYPE_MAX; k++) {
               if (frameFactory == m_frameFactory[k]) {
                   CLOGD("DEBUG(%s[%d]): m_frameFactory index(%d) and index(%d) are same instance, set index(%d) = NULL",
                       __FUNCTION__, __LINE__, i, k, k);
                   m_frameFactory[k] = NULL;
               }
            }

            if (m_frameFactory[i]->isCreated() == true) {
                ret = m_frameFactory[i]->destroy();
                if (ret < 0)
                    CLOGE("ERR(%s[%d]):m_frameFactory[%d] destroy fail", __FUNCTION__, __LINE__, i);
            }

            SAFE_DELETE(m_frameFactory[i]);

            CLOGD("DEBUG(%s[%d]):m_frameFactory[%d] destroyed", __FUNCTION__, __LINE__, i);
        }
    }

    m_previewFrameFactory = NULL;
    m_pictureFrameFactory = NULL;
    m_reprocessingFrameFactory = NULL;
    m_visionFrameFactory = NULL;

    CLOGI("INFO(%s[%d]): -OUT-", __FUNCTION__, __LINE__);

    return ret;
}

status_t ExynosCamera::m_setFrameManager()
{
    sp<FrameWorker> worker;
    m_frameMgr = new ExynosCameraFrameManager("FRAME MANAGER", m_cameraId, FRAMEMGR_OPER::SLIENT, 100, 150);

    worker = new CreateWorker("CREATE FRAME WORKER", m_cameraId, FRAMEMGR_OPER::SLIENT, 200);
    m_frameMgr->setWorker(FRAMEMGR_WORKER::CREATE, worker);

    worker = new DeleteWorker("DELETE FRAME WORKER", m_cameraId, FRAMEMGR_OPER::SLIENT);
    m_frameMgr->setWorker(FRAMEMGR_WORKER::DELETE, worker);

    sp<KeyBox> key = new KeyBox("FRAME KEYBOX", m_cameraId);

    m_frameMgr->setKeybox(key);

    return NO_ERROR;
}


ExynosCamera::~ExynosCamera()
{
    this->release();
}

void ExynosCamera::release()
{
    CLOGI("INFO(%s[%d]): -IN-", __FUNCTION__, __LINE__);
    int ret = 0;

#ifdef CAMERA_FAST_ENTRANCE_V1
    if (m_fastenAeThread != NULL) {
        CLOGI("INFO(%s[%d]): wait m_fastenAeThread", __FUNCTION__, __LINE__);
        m_fastenAeThread->requestExitAndWait();
        CLOGI("INFO(%s[%d]): wait m_fastenAeThread end", __FUNCTION__, __LINE__);
    } else {
        CLOGI("INFO(%s[%d]):m_fastenAeThread is NULL", __FUNCTION__, __LINE__);
    }
#endif

    m_stopCompanion();

    if (m_frameMgr != NULL) {
        m_frameMgr->stop();
    }

    /* release all framefactory */
    m_deinitFrameFactory();

    if (m_exynosCameraParameters != NULL) {
        delete m_exynosCameraParameters;
        m_exynosCameraParameters = NULL;
        CLOGD("DEBUG(%s):Parameters(Id=%d) destroyed", __FUNCTION__, m_cameraId);
    }

    /* free all buffers */
    m_releaseBuffers();

    if (mIonClient >= 0) {
        ion_close(mIonClient);
        mIonClient = -1;
    }

    if (m_ionAllocator != NULL) {
        delete m_ionAllocator;
        m_ionAllocator = NULL;
    }

    if (m_grAllocator != NULL) {
        delete m_grAllocator;
        m_grAllocator = NULL;
    }

    if (m_mhbAllocator != NULL) {
        delete m_mhbAllocator;
        m_mhbAllocator = NULL;
    }

    if (m_pipeFrameDoneQ != NULL) {
        delete m_pipeFrameDoneQ;
        m_pipeFrameDoneQ = NULL;
    }

    if (m_zoomPreviwWithCscQ != NULL) {
        delete m_zoomPreviwWithCscQ;
        m_zoomPreviwWithCscQ = NULL;
    }

    /* vision */
    if (m_pipeFrameVisionDoneQ != NULL) {
        delete m_pipeFrameVisionDoneQ;
        m_pipeFrameVisionDoneQ = NULL;
    }

    if (dstIspReprocessingQ != NULL) {
        delete dstIspReprocessingQ;
        dstIspReprocessingQ = NULL;
    }

    if (dstSccReprocessingQ != NULL) {
        delete dstSccReprocessingQ;
        dstSccReprocessingQ = NULL;
    }

    if (dstGscReprocessingQ != NULL) {
        delete dstGscReprocessingQ;
        dstGscReprocessingQ = NULL;
    }

    if (dstPostPictureGscQ != NULL) {
        delete dstPostPictureGscQ;
        dstPostPictureGscQ = NULL;
    }

    if (m_ThumbnailPostCallbackQ != NULL) {
        delete m_ThumbnailPostCallbackQ;
        m_ThumbnailPostCallbackQ = NULL;
    }

    if (dstJpegReprocessingQ != NULL) {
        delete dstJpegReprocessingQ;
        dstJpegReprocessingQ = NULL;
    }

    if (m_postPictureQ != NULL) {
        delete m_postPictureQ;
        m_postPictureQ = NULL;
    }

#ifdef SAMSUNG_DEBLUR
    if (m_deblurCaptureQ != NULL) {
        delete m_deblurCaptureQ;
        m_deblurCaptureQ = NULL;
    }

    if (m_detectDeblurCaptureQ != NULL) {
        delete m_detectDeblurCaptureQ;
        m_detectDeblurCaptureQ = NULL;
    }
#endif

    if (m_jpegCallbackQ != NULL) {
        delete m_jpegCallbackQ;
        m_jpegCallbackQ = NULL;
    }

    if (m_postviewCallbackQ != NULL) {
        delete m_postviewCallbackQ;
        m_postviewCallbackQ = NULL;
    }

    if (m_thumbnailCallbackQ != NULL) {
        delete m_thumbnailCallbackQ;
        m_thumbnailCallbackQ = NULL;
    }

#ifdef SAMSUNG_LBP
    if (m_LBPbufferQ != NULL) {
        delete m_LBPbufferQ;
        m_LBPbufferQ = NULL;
    }
#endif

#ifdef SAMSUNG_DNG
    if (m_dngCaptureQ != NULL) {
        delete m_dngCaptureQ;
        m_dngCaptureQ = NULL;
    }
#endif

#ifdef SAMSUNG_BD
    if (m_BDbufferQ != NULL) {
        delete m_BDbufferQ;
        m_BDbufferQ = NULL;
    }
#endif

#ifdef SAMSUNG_LLS_DEBLUR
    if (m_LDCaptureQ != NULL) {
        delete m_LDCaptureQ;
        m_LDCaptureQ = NULL;
    }
#endif

    if (m_facedetectQ != NULL) {
        delete m_facedetectQ;
        m_facedetectQ = NULL;
    }

    if (m_previewQ != NULL) {
        delete m_previewQ;
        m_previewQ = NULL;
    }

    for(int i = 0 ; i < MAX_NUM_PIPES ; i++ ) {
        if (m_mainSetupQ[i] != NULL) {
            delete m_mainSetupQ[i];
            m_mainSetupQ[i] = NULL;
        }
    }

    if (m_previewCallbackGscFrameDoneQ != NULL) {
        delete m_previewCallbackGscFrameDoneQ;
        m_previewCallbackGscFrameDoneQ = NULL;
    }

    if (m_recordingQ != NULL) {
        delete m_recordingQ;
        m_recordingQ = NULL;
    }

    for (int threadNum = JPEG_SAVE_THREAD0; threadNum < JPEG_SAVE_THREAD_MAX_COUNT; threadNum++) {
        if (m_jpegSaveQ[threadNum] != NULL) {
            delete m_jpegSaveQ[threadNum];
            m_jpegSaveQ[threadNum] = NULL;
        }
    }

    if (m_highResolutionCallbackQ != NULL) {
        delete m_highResolutionCallbackQ;
        m_highResolutionCallbackQ = NULL;
    }

    if (m_frameFactoryQ != NULL) {
        delete m_frameFactoryQ;
        m_frameFactoryQ = NULL;
        CLOGD("DEBUG(%s):FrameFactoryQ destroyed", __FUNCTION__);
    }

    if (m_bayerBufferMgr != NULL) {
        delete m_bayerBufferMgr;
        m_bayerBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(bayerBufferMgr) destroyed", __FUNCTION__);
    }

#ifdef SAMSUNG_DNG
    if (m_fliteBufferMgr != NULL) {
        delete m_fliteBufferMgr;
        m_fliteBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(m_fliteBufferMgr) destroyed", __FUNCTION__);
    }
#endif

    if (m_3aaBufferMgr != NULL) {
        delete m_3aaBufferMgr;
        m_3aaBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(3aaBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_ispBufferMgr != NULL) {
        delete m_ispBufferMgr;
        m_ispBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(ispBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_hwDisBufferMgr != NULL) {
        delete m_hwDisBufferMgr;
        m_hwDisBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(m_hwDisBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_scpBufferMgr != NULL) {
        delete m_scpBufferMgr;
        m_scpBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(scpBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_zoomScalerBufferMgr != NULL) {
        delete m_zoomScalerBufferMgr;
        m_zoomScalerBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(m_zoomScalerBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_ispReprocessingBufferMgr != NULL) {
        delete m_ispReprocessingBufferMgr;
        m_ispReprocessingBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(ispReprocessingBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_sccReprocessingBufferMgr != NULL) {
        delete m_sccReprocessingBufferMgr;
        m_sccReprocessingBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(sccReprocessingBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_sccBufferMgr != NULL) {
        delete m_sccBufferMgr;
        m_sccBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(sccBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_gscBufferMgr != NULL) {
        delete m_gscBufferMgr;
        m_gscBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(gscBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_postPictureGscBufferMgr != NULL) {
        delete m_postPictureGscBufferMgr;
        m_postPictureGscBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(m_postPictureGscBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_thumbnailGscBufferMgr != NULL) {
        delete m_thumbnailGscBufferMgr;
        m_thumbnailGscBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(m_thumbnailGscBufferMgr) destroyed", __FUNCTION__);
    }

#ifdef SAMSUNG_LBP
    if (m_lbpBufferMgr != NULL) {
        delete m_lbpBufferMgr;
        m_lbpBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(lbpBufferMgr) destroyed", __FUNCTION__);
    }
#endif

    if (m_jpegBufferMgr != NULL) {
        delete m_jpegBufferMgr;
        m_jpegBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(jpegBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_previewCallbackBufferMgr != NULL) {
        delete m_previewCallbackBufferMgr;
        m_previewCallbackBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(previewCallbackBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_highResolutionCallbackBufferMgr != NULL) {
        delete m_highResolutionCallbackBufferMgr;
        m_highResolutionCallbackBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(m_highResolutionCallbackBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_recordingBufferMgr != NULL) {
        delete m_recordingBufferMgr;
        m_recordingBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(recordingBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_captureSelector != NULL) {
        delete m_captureSelector;
        m_captureSelector = NULL;
    }

    if (m_sccCaptureSelector != NULL) {
        delete m_sccCaptureSelector;
        m_sccCaptureSelector = NULL;
    }

    if (m_recordingCallbackHeap != NULL) {
        m_recordingCallbackHeap->release(m_recordingCallbackHeap);
        delete m_recordingCallbackHeap;
        m_recordingCallbackHeap = NULL;
        CLOGD("DEBUG(%s):BufferManager(recordingCallbackHeap) destroyed", __FUNCTION__);
    }
#ifdef SUPPORT_SW_VDIS
    if (m_swVDIS_BufferMgr != NULL) {
        delete m_swVDIS_BufferMgr;
        m_swVDIS_BufferMgr = NULL;
        VDIS_LOG("VDIS_HAL(%s):BufferManager(m_swVDIS_BufferMgr) destroyed", __FUNCTION__);
    }
#endif /*SUPPORT_SW_VDIS*/

#ifdef SAMSUNG_LLV
    if(m_LLVpluginHandle != NULL) {
        ret = uni_plugin_unload(&m_LLVpluginHandle);
        if(ret < 0) {
            CLOGE("[LLV](%s[%d]):LLV plugin unload failed!!", __FUNCTION__, __LINE__);
        }
    }
#endif

#ifdef SAMSUNG_OT
    if(m_OTpluginHandle != NULL) {
        ret = uni_plugin_unload(&m_OTpluginHandle);
        if(ret < 0) {
            CLOGE("[OBTR](%s[%d]):Object Tracking plugin unload failed!!", __FUNCTION__, __LINE__);
        }
    }
#endif

#ifdef SAMSUNG_LBP
    if(m_LBPpluginHandle != NULL) {
        ret = uni_plugin_unload(&m_LBPpluginHandle);
        if(ret < 0) {
            CLOGE("[LBP](%s[%d]):Bestphoto plugin unload failed!!", __FUNCTION__, __LINE__);
        }
    }
#endif

#ifdef SAMSUNG_JQ
    if(m_JQpluginHandle != NULL) {
        ret = uni_plugin_unload(&m_JQpluginHandle);
        if(ret < 0) {
            CLOGE("[JQ](%s[%d]):JPEG Qtable plugin unload failed!!", __FUNCTION__, __LINE__);
        }
    }
#endif

#ifdef SAMSUNG_BD
    if(m_BDpluginHandle != NULL) {
        ret = uni_plugin_unload(&m_BDpluginHandle);
        if(ret < 0) {
            CLOGE("[BD](%s[%d]):Blur Detection plugin unload failed!!", __FUNCTION__, __LINE__);
        }
    }
#endif

#ifdef SAMSUNG_DEBLUR
    if(m_DeblurpluginHandle != NULL) {
        ret = uni_plugin_unload(&m_DeblurpluginHandle);
        if(ret < 0) {
            CLOGE("[Deblur](%s[%d]):Deblur plugin unload failed!!", __FUNCTION__, __LINE__);
        }
    }
#if 0
    if(m_ImgScreenerpluginHandle != NULL) {
        ret = uni_plugin_unload(&m_ImgScreenerpluginHandle);
        if(ret < 0) {
            CLOGE("[Deblur](%s[%d]):Image Screener plugin unload failed!!", __FUNCTION__, __LINE__);
        }
    }
#endif
#endif

#ifdef SAMSUNG_LLS_DEBLUR
    if(m_LLSpluginHandle != NULL) {
        ret = uni_plugin_unload(&m_LLSpluginHandle);
        if(ret < 0) {
            CLOGE("[LDC](%s[%d]):LLS plugin unload failed!!", __FUNCTION__, __LINE__);
        }
    }
#endif

    m_isFirstStart = true;
    m_previewBufferCount = NUM_PREVIEW_BUFFERS;

    if (m_frameMgr != NULL) {
        delete m_frameMgr;
        m_frameMgr = NULL;
    }

    if (m_tempshot != NULL) {
        delete m_tempshot;
        m_tempshot = NULL;
    }

    if (m_fdmeta_shot != NULL) {
        delete m_fdmeta_shot;
        m_fdmeta_shot = NULL;
    }

    if (m_meta_shot != NULL) {
        delete m_meta_shot;
        m_meta_shot = NULL;
    }

#ifdef SUPPORT_SW_VDIS
    if (m_swVDIS_fd_dm != NULL) {
        delete m_swVDIS_fd_dm;
        m_swVDIS_fd_dm = NULL;
    }

    if (m_swVDIS_FaceData != NULL) {
        delete m_swVDIS_FaceData;
        m_swVDIS_FaceData = NULL;
    }
#endif /*SUPPORT_SW_VDIS*/

#ifdef SAMSUNG_HRM
    if(m_uv_rayHandle != NULL) {
        sensor_listener_disable_sensor(m_uv_rayHandle, ST_UV_RAY);
        sensor_listener_unload(&m_uv_rayHandle);
        m_uv_rayHandle = NULL;
    }
#endif

#ifdef SAMSUNG_LIGHT_IR
    if(m_light_irHandle != NULL) {
        sensor_listener_disable_sensor(m_light_irHandle, ST_LIGHT_IR);
        sensor_listener_unload(&m_light_irHandle);
        m_light_irHandle = NULL;
    }
#endif

#ifdef SAMSUNG_GYRO
    if(m_gyroHandle != NULL) {
        sensor_listener_disable_sensor(m_gyroHandle, ST_GYROSCOPE);
        sensor_listener_unload(&m_gyroHandle);
        m_gyroHandle = NULL;
    }
#endif

    CLOGI("INFO(%s[%d]): -OUT-", __FUNCTION__, __LINE__);
}

int ExynosCamera::getCameraId() const
{
    return m_cameraId;
}

int ExynosCamera::getShotBufferIdex() const
{
    return NUM_PLANES(V4L2_PIX_2_HAL_PIXEL_FORMAT(SCC_OUTPUT_COLOR_FMT));
}

status_t ExynosCamera::setPreviewWindow(preview_stream_ops *w)
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    status_t ret = NO_ERROR;
    int width, height;
    int halPreviewFmt = 0;
    bool flagRestart = false;
    buffer_manager_type bufferType = BUFFER_MANAGER_ION_TYPE;

    if (m_exynosCameraParameters != NULL) {
        if (m_exynosCameraParameters->getVisionMode() == true) {
            CLOGW("WRN(%s[%d]): Vision mode does not support", __FUNCTION__, __LINE__);
            /* android_printAssert(NULL, LOG_TAG, "Cannot support this operation"); */

            return NO_ERROR;
        }
    } else {
        CLOGW("(%s):m_exynosCameraParameters is NULL. Skipped", __FUNCTION__);
        return INVALID_OPERATION;
    }

    if (previewEnabled() == true) {
        CLOGW("WRN(%s[%d]): preview is started, we forcely re-start preview", __FUNCTION__, __LINE__);
        flagRestart = true;
        m_disablePreviewCB = true;
        stopPreview();
    }

    m_previewWindow = w;

    if (m_scpBufferMgr != NULL) {
        CLOGD("DEBUG(%s[%d]): scp buffer manager need recreate", __FUNCTION__, __LINE__);
        m_scpBufferMgr->deinit();

        delete m_scpBufferMgr;
        m_scpBufferMgr = NULL;
    }

    if (w == NULL) {
        bufferType = BUFFER_MANAGER_ION_TYPE;
        CLOGW("WARN(%s[%d]):window NULL, create internal buffer for preview", __FUNCTION__, __LINE__);
    } else {
        halPreviewFmt = m_exynosCameraParameters->getHalPixelFormat();
        bufferType = BUFFER_MANAGER_GRALLOC_TYPE;
        m_exynosCameraParameters->getHwPreviewSize(&width, &height);

        if (m_grAllocator == NULL)
            m_grAllocator = new ExynosCameraGrallocAllocator();

#ifdef RESERVED_MEMORY_FOR_GRALLOC_ENABLE
        if (!(((m_exynosCameraParameters->getShotMode() == SHOT_MODE_BEAUTY_FACE) && (getCameraId() == CAMERA_ID_BACK))
            || m_exynosCameraParameters->getRecordingHint() == true)) {
            ret = m_grAllocator->init(m_previewWindow, m_exynosconfig->current->bufInfo.num_preview_buffers,
                                    m_exynosconfig->current->bufInfo.preview_buffer_margin, (GRALLOC_SET_USAGE_FOR_CAMERA | GRALLOC_USAGE_CAMERA_RESERVED));
        } else
#endif
        {
            ret = m_grAllocator->init(m_previewWindow, m_exynosconfig->current->bufInfo.num_preview_buffers, m_exynosconfig->current->bufInfo.preview_buffer_margin);
        }

        if (ret < 0) {
            CLOGE("ERR(%s[%d]):gralloc init fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            goto func_exit;
        }

        ret = m_grAllocator->setBuffersGeometry(width, height, halPreviewFmt);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):gralloc setBufferGeomety fail, size(%dx%d), fmt(%d), ret(%d)",
                __FUNCTION__, __LINE__, width, height, halPreviewFmt, ret);
            goto func_exit;
        }
    }

    m_createBufferManager(&m_scpBufferMgr, "SCP_BUF", bufferType);

    if (bufferType == BUFFER_MANAGER_GRALLOC_TYPE)
        m_scpBufferMgr->setAllocator(m_grAllocator);

    if (flagRestart == true) {
        startPreview();
    }

func_exit:
    m_disablePreviewCB = false;

    return ret;
}

void ExynosCamera::setCallbacks(
        camera_notify_callback notify_cb,
        camera_data_callback data_cb,
        camera_data_timestamp_callback data_cb_timestamp,
        camera_request_memory get_memory,
        void *user)
{
    CLOGI("INFO(%s[%d]): -IN-", __FUNCTION__, __LINE__);

    int ret = 0;

    m_notifyCb        = notify_cb;
    m_dataCb          = data_cb;
    m_dataCbTimestamp = data_cb_timestamp;
    m_getMemoryCb     = get_memory;
    m_callbackCookie  = user;

    if (m_mhbAllocator == NULL)
        m_mhbAllocator = new ExynosCameraMHBAllocator();

    ret = m_mhbAllocator->init(get_memory);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]:m_mhbAllocator init failed", __FUNCTION__, __LINE__);
    }
}

void ExynosCamera::enableMsgType(int32_t msgType)
{
    if (m_exynosCameraParameters) {
        CLOGV("INFO(%s[%d]): enable Msg (%x)", __FUNCTION__, __LINE__, msgType);
        m_exynosCameraParameters->enableMsgType(msgType);
    }
}

void ExynosCamera::disableMsgType(int32_t msgType)
{
    if (m_exynosCameraParameters) {
        CLOGV("INFO(%s[%d]): disable Msg (%x)", __FUNCTION__, __LINE__, msgType);
        m_exynosCameraParameters->disableMsgType(msgType);
    }
}

bool ExynosCamera::msgTypeEnabled(int32_t msgType)
{
    bool IsEnabled = false;

    if (m_exynosCameraParameters) {
        CLOGV("INFO(%s[%d]): Msg type enabled (%x)", __FUNCTION__, __LINE__, msgType);
        IsEnabled = m_exynosCameraParameters->msgTypeEnabled(msgType);
    }

    return IsEnabled;
}

status_t ExynosCamera::startPreview()
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    int ret = 0;
    int32_t skipFrameCount = INITIAL_SKIP_FRAME;
    unsigned int fdCallbackSize = 0;
#ifdef SR_CAPTURE
    unsigned int srCallbackSize = 0;
#endif
#ifdef CAMERA_FAST_ENTRANCE_V1
    int wait_cnt = 0;
#endif

    m_hdrSkipedFcount = 0;
    m_isTryStopFlash= false;
    m_exitAutoFocusThread = false;
    m_curMinFps = 0;
    m_isNeedAllocPictureBuffer = false;
    m_flagThreadStop= false;
    m_frameSkipCount = 0;
#ifndef CAMERA_FAST_ENTRANCE_V1
    m_checkFirstFrameLux = false;
#endif
    m_exynosCameraParameters->setIsThumbnailCallbackOn(false);
    m_stopLongExposure = false;

#ifdef FIRST_PREVIEW_TIME_CHECK
    if (m_flagFirstPreviewTimerOn == false) {
        m_firstPreviewTimer.start();
        m_flagFirstPreviewTimerOn = true;

        CLOGD("DEBUG(%s[%d]):m_firstPreviewTimer start", __FUNCTION__, __LINE__);
    }
#endif

    if (m_previewEnabled == true) {
        return INVALID_OPERATION;
    }

#ifdef SAMSUNG_SENSOR_LISTENER
    if(m_sensorListenerThread != NULL)
        m_sensorListenerThread->run();
#endif

#ifdef FORCE_RESET_MULTI_FRAME_FACTORY
    /* HACK
     * stopPreview() close companion
     * so. start here again
     * The reason why we did't start compaion on stopPreview() is..
     * release() can be come just after stopPreview().
     * If it is, release can be delay by companion open and. then app make exception
     */
    if (m_startCompanion() != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_startCompanion fail", __FUNCTION__, __LINE__);
    }
#endif

#ifdef CAMERA_FAST_ENTRANCE_V1
    if (m_fastEntrance == false)
#endif
    {
        /* frame manager start */
        m_frameMgr->start();
    }

#ifdef SAMSUNG_DOF
    fdCallbackSize = sizeof(camera_frame_metadata_t) * NUM_OF_DETECTED_FACES +
            sizeof(camera2_pdaf_multi_result)*m_frameMetadata.dof_row*m_frameMetadata.dof_column;
#else
    fdCallbackSize = sizeof(camera_frame_metadata_t) * NUM_OF_DETECTED_FACES;
#endif

    m_fdCallbackHeap = m_getMemoryCb(-1, fdCallbackSize, 1, m_callbackCookie);
    if (!m_fdCallbackHeap || m_fdCallbackHeap->data == MAP_FAILED) {
        CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, fdCallbackSize);
        m_fdCallbackHeap = NULL;
        goto err;
    }

#ifdef SR_CAPTURE
    srCallbackSize = sizeof(camera_frame_metadata_t) * NUM_OF_DETECTED_FACES;

    m_srCallbackHeap = m_getMemoryCb(-1, srCallbackSize, 1, m_callbackCookie);
    if (!m_srCallbackHeap || m_srCallbackHeap->data == MAP_FAILED) {
        CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, srCallbackSize);
        m_srCallbackHeap = NULL;
        goto err;
    }
#endif

#ifdef SAMSUNG_LBP
    if(getCameraId() == CAMERA_ID_FRONT)
        m_exynosCameraParameters->resetNormalBestFrameCount();
#endif

#ifdef CAMERA_FAST_ENTRANCE_V1
    if (m_fastEntrance == true) {
        /* In case m_previewFrameFactory is not created, wait for 1 sec */
        while (m_previewFrameFactory == NULL) {
            if (wait_cnt > 200) {
                CLOGE("ERR(%s[%d]): m_previewFrameFactory is not created for 1 SEC", __FUNCTION__, __LINE__);
                break;
            }
            usleep(5000);
            wait_cnt++;

            if (wait_cnt % 20 == 0)
                CLOGW("WARN(%s[%d]): Waiting until m_previewFrameFactory create (wait_cnt %d)",
                        __FUNCTION__, __LINE__, wait_cnt);
        }
    }
    else
#endif
    {
         /*
          * This is for updating parameter value at once.
          * This must be just before making factory
          */
         m_exynosCameraParameters->updateTpuParameters();

         /* setup frameFactory with scenario */
         m_setupFrameFactory();
    }

    /* vision */
    CLOGI("INFO(%s[%d]): getVisionMode(%d)", __FUNCTION__, __LINE__, m_exynosCameraParameters->getVisionMode());
    if (m_exynosCameraParameters->getVisionMode() == true) {
        ret = m_setVisionBuffers();
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_setVisionCallbackBuffer() fail", __FUNCTION__, __LINE__);
            return ret;
        }

        ret = m_setVisionCallbackBuffer();
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_setVisionCallbackBuffer() fail", __FUNCTION__, __LINE__);
            return INVALID_OPERATION;
        }

        if (m_visionFrameFactory->isCreated() == false) {
            ret = m_visionFrameFactory->create();
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):m_visionFrameFactory->create() failed", __FUNCTION__, __LINE__);
                goto err;
            }
            CLOGD("DEBUG(%s):FrameFactory(VisionFrameFactory) created", __FUNCTION__);
        }

        m_exynosCameraParameters->setFrameSkipCount(INITIAL_SKIP_FRAME);

        ret = m_startVisionInternal();
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_startVisionInternal() failed", __FUNCTION__, __LINE__);
            goto err;
        }

        m_visionThread->run(PRIORITY_DEFAULT);
        return NO_ERROR;
    } else {
        m_exynosCameraParameters->setSeriesShotMode(SERIES_SHOT_MODE_NONE);

        if(m_exynosCameraParameters->increaseMaxBufferOfPreview()) {
            m_exynosCameraParameters->setPreviewBufferCount(NUM_PREVIEW_BUFFERS + NUM_PREVIEW_SPARE_BUFFERS);
        } else {
            m_exynosCameraParameters->setPreviewBufferCount(NUM_PREVIEW_BUFFERS);
        }

#ifdef SUPPORT_SW_VDIS
        m_swVDIS_Mode = false;
        if(m_exynosCameraParameters->isSWVdisMode()) {
            m_swVDIS_InW = m_swVDIS_InH = 0;
            m_exynosCameraParameters->getHwPreviewSize(&m_swVDIS_InW, &m_swVDIS_InH);
            m_swVDIS_CameraID = getCameraId();
            m_swVDIS_SensorType = getSensorId(m_swVDIS_CameraID);
#ifdef SAMSUNG_OIS_VDIS
            m_swVDIS_OISMode = UNI_PLUGIN_OIS_MODE_VDIS;
            m_OISvdisMode = UNI_PLUGIN_OIS_MODE_END;
#endif
            m_swVDIS_init();

            m_exynosCameraParameters->setPreviewBufferCount(NUM_VDIS_BUFFERS);
            m_exynosconfig->current->bufInfo.num_preview_buffers = NUM_VDIS_BUFFERS;
            VDIS_LOG("VDIS_HAL: Preview Buffer Count Change to %d", NUM_VDIS_BUFFERS);
        }
#endif /*SUPPORT_SW_VDIS*/

        if ((m_exynosCameraParameters->getRestartPreview() == true) ||
            m_previewBufferCount != m_exynosCameraParameters->getPreviewBufferCount()) {
            ret = setPreviewWindow(m_previewWindow);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):setPreviewWindow fail", __FUNCTION__, __LINE__);
                return INVALID_OPERATION;
            }

            m_previewBufferCount = m_exynosCameraParameters->getPreviewBufferCount();
        }

#ifdef CAMERA_FAST_ENTRANCE_V1
        if (m_fastEntrance == true) {
            ret = m_setBuffers();
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):m_setBuffers failed, releaseBuffer", __FUNCTION__, __LINE__);
                m_isSuccessedBufferAllocation = false;
                goto err;
            }

            m_isSuccessedBufferAllocation = true;
        } else
#endif
        {
            CLOGI("INFO(%s[%d]):setBuffersThread is run", __FUNCTION__, __LINE__);
            m_setBuffersThread->run(PRIORITY_DEFAULT);
        }

        if (m_captureSelector == NULL) {
            ExynosCameraBufferManager *bufMgr = NULL;

            if (m_exynosCameraParameters->isReprocessing() == true)
                bufMgr = m_bayerBufferMgr;

            m_captureSelector = new ExynosCameraFrameSelector(m_exynosCameraParameters, bufMgr, m_frameMgr);

            if (m_exynosCameraParameters->isReprocessing() == true) {
                ret = m_captureSelector->setFrameHoldCount(REPROCESSING_BAYER_HOLD_COUNT);
                if (ret < 0)
                    CLOGE("ERR(%s[%d]): setFrameHoldCount(%d) is fail", __FUNCTION__, __LINE__, REPROCESSING_BAYER_HOLD_COUNT);
            }
        }

        if (m_sccCaptureSelector == NULL) {
            ExynosCameraBufferManager *bufMgr = NULL;
            if (m_exynosCameraParameters->isSccCapture() == true) {
                /* TODO: Dynamic select buffer manager for capture */
                bufMgr = m_sccBufferMgr;
            }
            m_sccCaptureSelector = new ExynosCameraFrameSelector(m_exynosCameraParameters, bufMgr, m_frameMgr);
        }

        if (m_captureSelector != NULL)
            m_captureSelector->release();

        if (m_sccCaptureSelector != NULL)
            m_sccCaptureSelector->release();

#ifdef OIS_CAPTURE
        ExynosCameraActivitySpecialCapture *m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();
        m_sCaptureMgr->resetOISCaptureFcount();
#endif

#ifdef RAWDUMP_CAPTURE
        ExynosCameraActivitySpecialCapture *m_sCapture = m_exynosCameraActivityControl->getSpecialCaptureMgr();
        m_sCapture->resetRawCaptureFcount();
#endif

        if ((m_previewFrameFactory->isCreated() == false)
#ifdef CAMERA_FAST_ENTRANCE_V1
          && (m_fastEntrance == false)
#endif
        ) {
#if defined(SAMSUNG_EEPROM)
            if ((m_use_companion == false) && (isEEprom(getCameraId()) == true)) {
                if (m_eepromThread != NULL) {
                    CLOGD("DEBUG(%s): eepromThread join.....", __FUNCTION__);
                    m_eepromThread->join();
                } else {
                    CLOGD("DEBUG(%s): eepromThread is NULL.", __FUNCTION__);
                }
                m_exynosCameraParameters->setRomReadThreadDone(true);
                CLOGD("DEBUG(%s): eepromThread joined", __FUNCTION__);
            }
#endif /* SAMSUNG_EEPROM */

#ifdef SAMSUNG_COMPANION
            if(m_use_companion == true) {
                ret = m_previewFrameFactory->precreate();
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):m_previewFrameFactory->precreate() failed", __FUNCTION__, __LINE__);
                    goto err;
                }

                m_waitCompanionThreadEnd();

                m_exynosCameraParameters->setRomReadThreadDone(true);

                ret = m_previewFrameFactory->postcreate();
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):m_previewFrameFactory->postcreate() failed", __FUNCTION__, __LINE__);
                    goto err;
                }
            } else {
                ret = m_previewFrameFactory->create();
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):m_previewFrameFactory->create() failed", __FUNCTION__, __LINE__);
                    goto err;
                }
            }
#else
            ret = m_previewFrameFactory->create();
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):m_previewFrameFactory->create() failed", __FUNCTION__, __LINE__);
                goto err;
            }
#endif
            CLOGD("DEBUG(%s):FrameFactory(previewFrameFactory) created", __FUNCTION__);
        }

#ifdef CAMERA_FAST_ENTRANCE_V1
        if (m_fastEntrance == false)
#endif /* CAMERA_FAST_ENTRANCE_V1 */
        {
#ifdef USE_QOS_SETTING
            ret = m_previewFrameFactory->setControl(V4L2_CID_IS_DVFS_CLUSTER1, BIG_CORE_MAX_LOCK, PIPE_3AA);
            if (ret < 0)
                CLOGE("ERR(%s[%d]):V4L2_CID_IS_DVFS_CLUSTER1 setControl fail, ret(%d)", __FUNCTION__, __LINE__, ret);
#endif

            if (m_exynosCameraParameters->getUseFastenAeStable() == true &&
                m_exynosCameraParameters->getDualMode() == false &&
                m_exynosCameraParameters->getRecordingHint() == false &&
                m_isFirstStart == true) {

                ret = m_fastenAeStable();
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):m_fastenAeStable() failed", __FUNCTION__, __LINE__);
                    ret = INVALID_OPERATION;
                    goto err;
                } else {
                    skipFrameCount = 0;
                    m_exynosCameraParameters->setUseFastenAeStable(false);
                }
            } else if(m_exynosCameraParameters->getDualMode() == true) {
                skipFrameCount = INITIAL_SKIP_FRAME + 2;
            }
        }

#ifdef CAMERA_FAST_ENTRANCE_V1
        if (m_fastEntrance == true) {
            m_waitFastenAeThreadEnd();
            if (m_fastenAeThreadResult < 0) {
                CLOGE("ERR(%s[%d]):fastenAeThread exit with error", __FUNCTION__, __LINE__);
                ret = m_fastenAeThreadResult;
                goto err;
            }
        }
#endif

#ifdef SET_FPS_SCENE /* This codes for 5260, Do not need other project */
        struct camera2_shot_ext *initMetaData = new struct camera2_shot_ext;
        if (initMetaData != NULL) {
            m_exynosCameraParameters->duplicateCtrlMetadata(initMetaData);

            ret = m_previewFrameFactory->setControl(V4L2_CID_IS_MIN_TARGET_FPS, initMetaData->shot.ctl.aa.aeTargetFpsRange[0], PIPE_FLITE);
            if (ret < 0)
                CLOGE("ERR(%s[%d]):FLITE setControl fail, ret(%d)", __FUNCTION__, __LINE__, ret);

            ret = m_previewFrameFactory->setControl(V4L2_CID_IS_MAX_TARGET_FPS, initMetaData->shot.ctl.aa.aeTargetFpsRange[1], PIPE_FLITE);
            if (ret < 0)
                CLOGE("ERR(%s[%d]):FLITE setControl fail, ret(%d)", __FUNCTION__, __LINE__, ret);

            ret = m_previewFrameFactory->setControl(V4L2_CID_IS_SCENE_MODE, initMetaData->shot.ctl.aa.sceneMode, PIPE_FLITE);
            if (ret < 0)
                CLOGE("ERR(%s[%d]):FLITE setControl fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            delete initMetaData;
            initMetaData = NULL;
        } else {
            CLOGE("ERR(%s[%d]):initMetaData is NULL", __FUNCTION__, __LINE__);
        }
#elif SET_FPS_FRONTCAM
        if (m_exynosCameraParameters->getCameraId() == CAMERA_ID_FRONT) {
            struct camera2_shot_ext *initMetaData = new struct camera2_shot_ext;
            if (initMetaData != NULL) {
                m_exynosCameraParameters->duplicateCtrlMetadata(initMetaData);
                CLOGD("(%s:[%d]) : setControl for Frame Range.", __FUNCTION__, __LINE__);

                ret = m_previewFrameFactory->setControl(V4L2_CID_IS_MIN_TARGET_FPS, initMetaData->shot.ctl.aa.aeTargetFpsRange[0], PIPE_FLITE_FRONT);
                if (ret < 0)
                    CLOGE("ERR(%s[%d]):FLITE setControl fail, ret(%d)", __FUNCTION__, __LINE__, ret);

                ret = m_previewFrameFactory->setControl(V4L2_CID_IS_MAX_TARGET_FPS, initMetaData->shot.ctl.aa.aeTargetFpsRange[1], PIPE_FLITE_FRONT);
                if (ret < 0)
                    CLOGE("ERR(%s[%d]):FLITE setControl fail, ret(%d)", __FUNCTION__, __LINE__, ret);

                delete initMetaData;
                initMetaData = NULL;
            } else {
                CLOGE("ERR(%s[%d]):initMetaData is NULL", __FUNCTION__, __LINE__);
            }
        }
#endif
#ifdef USE_FADE_IN_ENTRANCE
        if (m_exynosCameraParameters->getFirstEntrance() == true) {
            /* Fade In/Out */
            m_exynosCameraParameters->setFrameSkipCount(0);
            ret = m_previewFrameFactory->setControl(V4L2_CID_CAMERA_FADE_IN, 1, PIPE_FLITE);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):FLITE setControl fail(V4L2_CID_CAMERA_FADE_IN), ret(%d)", __FUNCTION__, __LINE__, ret);
            }
        } else {
            /* Skip Frame */
            m_exynosCameraParameters->setFrameSkipCount(skipFrameCount);
            ret = m_previewFrameFactory->setControl(V4L2_CID_CAMERA_FADE_IN, 0, PIPE_FLITE);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):FLITE setControl fail(V4L2_CID_CAMERA_FADE_IN), ret(%d)", __FUNCTION__, __LINE__, ret);
            }
        }
#else
#ifdef CAMERA_FAST_ENTRANCE_V1
        if (m_fastEntrance == false)
#endif
        {
            m_exynosCameraParameters->setFrameSkipCount(skipFrameCount);
        }
#endif

#ifdef CAMERA_FAST_ENTRANCE_V1
        if (m_fastEntrance == false)
#endif
        {
            m_setBuffersThread->join();
        }

        if (m_isSuccessedBufferAllocation == false) {
            CLOGE("ERR(%s[%d]):m_setBuffersThread() failed", __FUNCTION__, __LINE__);
            goto err;
        }

        ret = m_startPreviewInternal();
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_startPreviewInternal() failed", __FUNCTION__, __LINE__);
            goto err;
        }

        if (m_exynosCameraParameters->isReprocessing() == true) {
#ifdef START_PICTURE_THREAD
#if !defined(USE_SNAPSHOT_ON_UHD_RECORDING)
            if (!m_exynosCameraParameters->getEffectRecordingHint() &&
                !m_exynosCameraParameters->getDualRecordingHint() &&
                !m_exynosCameraParameters->getUHDRecordingMode())
#endif
            {
                m_startPictureInternalThread->run(PRIORITY_DEFAULT);
            }
#endif
        } else {
            m_pictureFrameFactory = m_previewFrameFactory;
            CLOGD("DEBUG(%s[%d]):FrameFactory(pictureFrameFactory) created", __FUNCTION__, __LINE__);

            /*
             * Make remained frameFactory here.
             * in case of SCC capture, make here.
             */
            m_framefactoryThread->run();
        }

#if !defined(USE_SNAPSHOT_ON_UHD_RECORDING)
        if (!m_exynosCameraParameters->getEffectRecordingHint() &&
            !m_exynosCameraParameters->getDualRecordingHint() &&
            !m_exynosCameraParameters->getUHDRecordingMode())
#endif
        {
            m_startPictureBufferThread->run(PRIORITY_DEFAULT);
        }

        if (m_previewWindow != NULL)
            m_previewWindow->set_timestamp(m_previewWindow, systemTime(SYSTEM_TIME_MONOTONIC));

#ifdef RAWDUMP_CAPTURE
        m_mainSetupQThread[INDEX(PIPE_FLITE)]->run(PRIORITY_URGENT_DISPLAY);
        m_mainSetupQThread[INDEX(PIPE_3AA)]->run(PRIORITY_URGENT_DISPLAY);
#else
        /* setup frame thread */
        if (m_exynosCameraParameters->getDualMode() == true && getCameraId() == CAMERA_ID_FRONT) {
            CLOGD("DEBUG(%s[%d]):setupThread Thread start pipeId(%d)", __FUNCTION__, __LINE__, PIPE_FLITE);
            m_mainSetupQThread[INDEX(PIPE_FLITE)]->run(PRIORITY_URGENT_DISPLAY);
        } else {
            switch (m_exynosCameraParameters->getReprocessingBayerMode()) {
            case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON:
                m_mainSetupQ[INDEX(PIPE_FLITE)]->setup(NULL);
                CLOGD("DEBUG(%s[%d]):setupThread Thread start pipeId(%d)", __FUNCTION__, __LINE__, PIPE_FLITE);
                m_mainSetupQThread[INDEX(PIPE_FLITE)]->run(PRIORITY_URGENT_DISPLAY);
                break;
            case REPROCESSING_BAYER_MODE_PURE_DYNAMIC:
                CLOGD("DEBUG(%s[%d]):setupThread with List pipeId(%d)", __FUNCTION__, __LINE__, PIPE_FLITE);
                m_mainSetupQ[INDEX(PIPE_FLITE)]->setup(m_mainSetupQThread[INDEX(PIPE_FLITE)]);
                break;
            default:
                CLOGI("INFO(%s[%d]):setupThread not started pipeID(%d)", __FUNCTION__, __LINE__, PIPE_FLITE);
                break;
            }
            CLOGD("DEBUG(%s[%d]):setupThread Thread start pipeId(%d)", __FUNCTION__, __LINE__, PIPE_3AA);
            m_mainSetupQThread[INDEX(PIPE_3AA)]->run(PRIORITY_URGENT_DISPLAY);
        }
#endif

#ifdef SAMSUNG_DNG
        if (m_exynosCameraParameters->getDNGCaptureModeOn()) {
            CLOGD("[DNG](%s[%d]):setupThread with List pipeId(%d)", __FUNCTION__, __LINE__, PIPE_FLITE);
            m_mainSetupQ[INDEX(PIPE_FLITE)]->setup(m_mainSetupQThread[INDEX(PIPE_FLITE)]);
        }
#endif

        if (m_facedetectThread->isRunning() == false)
            m_facedetectThread->run();

        m_previewThread->run(PRIORITY_DISPLAY);
        m_mainThread->run(PRIORITY_DEFAULT);
        if(m_exynosCameraParameters->getCameraId() == CAMERA_ID_BACK) {
            m_autoFocusContinousThread->run(PRIORITY_DEFAULT);
        }
        m_monitorThread->run(PRIORITY_DEFAULT);

        if (m_exynosCameraParameters->getZoomPreviewWIthScaler() == true) {
            CLOGD("DEBUG(%s[%d]):ZoomPreview with Scaler Thread start", __FUNCTION__, __LINE__);
            m_zoomPreviwWithCscThread->run(PRIORITY_DEFAULT);
        }

        if ((m_exynosCameraParameters->getHighResolutionCallbackMode() == true) &&
            (m_highResolutionCallbackRunning == false)) {
            CLOGD("DEBUG(%s[%d]):High resolution preview callback start", __FUNCTION__, __LINE__);
            if (skipFrameCount > 0)
                m_skipReprocessing = true;
            m_highResolutionCallbackRunning = true;
            if (m_exynosCameraParameters->isReprocessing() == true) {
                m_startPictureInternalThread->run(PRIORITY_DEFAULT);
                m_startPictureInternalThread->join();
            }
            m_prePictureThread->run(PRIORITY_DEFAULT);
        }

        /* FD-AE is always on */
#ifdef USE_FD_AE
        m_startFaceDetection(m_exynosCameraParameters->getFaceDetectionMode());
#endif

        if (m_exynosCameraParameters->getUseFastenAeStable() == true &&
            m_exynosCameraParameters->getCameraId() == CAMERA_ID_BACK &&
            m_exynosCameraParameters->getDualMode() == false &&
            m_exynosCameraParameters->getRecordingHint() == false &&
            m_isFirstStart == true) {
            /* AF mode is setted as INFINITY in fastenAE, and we should update that mode */
            m_exynosCameraActivityControl->setAutoFocusMode(FOCUS_MODE_INFINITY);

            m_exynosCameraParameters->setUseFastenAeStable(false);
            m_exynosCameraActivityControl->setAutoFocusMode(m_exynosCameraParameters->getFocusMode());
            m_isFirstStart = false;
            m_exynosCameraParameters->setIsFirstStartFlag(m_isFirstStart);
        }
    }

#ifdef BURST_CAPTURE
    m_burstInitFirst = true;
#endif

#ifdef SAMSUNG_JQ
    {
        int HwPreviewW = 0, HwPreviewH = 0;
        UniPluginBufferData_t pluginData;

        m_exynosCameraParameters->getHwPreviewSize(&HwPreviewW, &HwPreviewH);
        memset(&pluginData, 0, sizeof(UniPluginBufferData_t));
        pluginData.InWidth = HwPreviewW;
        pluginData.InHeight = HwPreviewH;
        pluginData.InFormat = UNI_PLUGIN_FORMAT_NV21;

        if(m_JQpluginHandle != NULL) {
            ret = uni_plugin_set(m_JQpluginHandle,
                    JPEG_QTABLE_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &pluginData);
            if(ret < 0) {
                CLOGE("[JQ](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_BUFFER_INFO) failed!!", __FUNCTION__, __LINE__);
            }
            ret = uni_plugin_init(m_JQpluginHandle);
            if(ret < 0) {
                CLOGE("[JQ](%s[%d]): Plugin init failed!!", __FUNCTION__, __LINE__);
            }
        }
    }
#endif

#ifdef SAMSUNG_DEBLUR
    if(m_DeblurpluginHandle != NULL) {
        ret = uni_plugin_init(m_DeblurpluginHandle);
        if(ret < 0) {
            CLOGE("[Deblur](%s[%d]): Deblur plugin init failed!!", __FUNCTION__, __LINE__);
        }
    }
#endif

#ifdef CAMERA_FAST_ENTRANCE_V1
    m_fastEntrance = false;
#endif

    return NO_ERROR;

err:

#ifdef SAMSUNG_COMPANION
    if(m_use_companion == true) {
        m_waitCompanionThreadEnd();
    }
#endif
#if defined(SAMSUNG_EEPROM)
    if ((m_use_companion == false) && (isEEprom(getCameraId()) == true)) {
        if (m_eepromThread != NULL) {
            m_eepromThread->join();
        } else {
            CLOGD("DEBUG(%s): eepromThread is NULL.", __FUNCTION__);
        }
    }
#endif

#ifdef CAMERA_FAST_ENTRANCE_V1
    if (m_fastEntrance == true) {
        m_waitFastenAeThreadEnd();
    }

    m_fastEntrance = false;
#endif

    /* frame manager stop */
    m_frameMgr->stop();

    m_setBuffersThread->join();

    m_releaseBuffers();

#ifdef SAMSUNG_SENSOR_LISTENER
    if (m_sensorListenerThread != NULL) {
        m_sensorListenerThread->requestExitAndWait();
    }

#ifdef SAMSUNG_HRM
    if(m_uv_rayHandle != NULL) {
        m_exynosCameraParameters->m_setHRM_Hint(false);
        sensor_listener_disable_sensor(m_uv_rayHandle, ST_UV_RAY);
        sensor_listener_unload(&m_uv_rayHandle);
        m_uv_rayHandle = NULL;
    }
#endif

#ifdef SAMSUNG_LIGHT_IR
    if(m_light_irHandle != NULL) {
        m_exynosCameraParameters->m_setLight_IR_Hint(false);
        sensor_listener_disable_sensor(m_light_irHandle, ST_LIGHT_IR);
        sensor_listener_unload(&m_light_irHandle);
        m_light_irHandle = NULL;
    }
#endif

#ifdef SAMSUNG_GYRO
    if(m_gyroHandle != NULL) {
        m_exynosCameraParameters->m_setGyroHint(false);
        sensor_listener_disable_sensor(m_gyroHandle, ST_GYROSCOPE);
        sensor_listener_unload(&m_gyroHandle);
        m_gyroHandle = NULL;
    }
#endif
#endif /* SAMSUNG_SENSOR_LISTENER */

    return ret;
}

void ExynosCamera::stopPreview()
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);
    int ret = 0;

    ExynosCameraActivityFlash *m_flashMgr = m_exynosCameraActivityControl->getFlashMgr();
    ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
    ExynosCameraFrame *frame = NULL;

#ifdef ONE_SECOND_BURST_CAPTURE
    m_clearOneSecondBurst(false);
#endif

#ifdef LLS_CAPTURE
    m_exynosCameraParameters->setLLSOn(false);
#endif

    m_flagLLSStart = false;

#ifdef CAMERA_FAST_ENTRANCE_V1
    if (m_fastenAeThread != NULL) {
        CLOGI("INFO(%s[%d]): wait m_fastenAeThread", __FUNCTION__, __LINE__);
        m_fastenAeThread->requestExitAndWait();
        CLOGI("INFO(%s[%d]): wait m_fastenAeThread end", __FUNCTION__, __LINE__);
    } else {
        CLOGI("INFO(%s[%d]):m_fastenAeThread is NULL", __FUNCTION__, __LINE__);
    }
#endif

#ifdef SAMSUNG_COMPANION
    if(m_use_companion == true) {
        if (m_companionThread != NULL) {
            CLOGI("INFO(%s[%d]): wait m_companionThread", __FUNCTION__, __LINE__);
            m_companionThread->requestExitAndWait();
            CLOGI("INFO(%s[%d]): wait m_companionThread end", __FUNCTION__, __LINE__);
        } else {
            CLOGI("INFO(%s[%d]): m_companionThread is NULL", __FUNCTION__, __LINE__);
        }
    }
#endif

    if (m_previewEnabled == false) {
        CLOGD("DEBUG(%s[%d]): preview is not enabled", __FUNCTION__, __LINE__);
        return;
    }

    m_stopLongExposure = true;

    if (m_pictureEnabled == true) {
        CLOGW("WARN(%s[%d]):m_pictureEnabled == true (picture is not finished)", __FUNCTION__, __LINE__);
        int retry = 0;
        do {
            usleep(WAITING_TIME);
            retry++;
        } while(m_pictureEnabled == true && retry < (TOTAL_WAITING_TIME/WAITING_TIME));
        CLOGW("WARN(%s[%d]):wait (%d)msec (because, picture is not finished)", __FUNCTION__, __LINE__, WAITING_TIME * retry / 1000);
    }

    m_exynosCameraParameters->setIsThumbnailCallbackOn(false);

    if (m_exynosCameraParameters->getVisionMode() == true) {
        m_frameFactoryQ->release();
        m_visionThread->requestExitAndWait();
        ret = m_stopVisionInternal();
        if (ret < 0)
            CLOGE("ERR(%s[%d]):m_stopVisionInternal fail", __FUNCTION__, __LINE__);
    } else {
#ifdef USE_QOS_SETTING
        if (m_previewFrameFactory == NULL) {
            CLOGW("WARN(%s[%d]):m_previewFrameFactory is NULL. so, skip setControl(V4L2_CID_IS_DVFS_CLUSTER1)", __FUNCTION__, __LINE__);
        } else {
            ret = m_previewFrameFactory->setControl(V4L2_CID_IS_DVFS_CLUSTER1, 0, PIPE_3AA);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):V4L2_CID_IS_DVFS_CLUSTER1 setControl fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            }
        }
#endif
        m_startPictureInternalThread->join();

        /* release about frameFactory */
        m_framefactoryThread->stop();
        m_frameFactoryQ->sendCmd(WAKE_UP);
        m_framefactoryThread->requestExitAndWait();
        m_frameFactoryQ->release();

        m_startPictureBufferThread->join();

        m_autoFocusRunning = false;
        m_exynosCameraActivityControl->cancelAutoFocus();

        CLOGD("DEBUG(%s[%d]): (%d, %d)", __FUNCTION__, __LINE__, m_flashMgr->getNeedCaptureFlash(), m_pictureEnabled);
        if (m_flashMgr->getNeedCaptureFlash() == true && m_pictureEnabled == true) {
            CLOGD("DEBUG(%s[%d]): force flash off", __FUNCTION__, __LINE__);
            m_exynosCameraActivityControl->cancelFlash();
            autoFocusMgr->stopAutofocus();
            m_isTryStopFlash = true;
            /* m_exitAutoFocusThread = true; */
        }

        /* Wait the end of the autoFocus Thread in order to the autofocus and the pre-flash is completed.*/
        m_autoFocusLock.lock();
        m_exitAutoFocusThread = true;
        m_autoFocusLock.unlock();

        int flashMode = AA_FLASHMODE_OFF;
        int waitingTime = FLASH_OFF_MAX_WATING_TIME / TOTAL_FLASH_WATING_COUNT; /* Max waiting time: 500ms, Count:10, Waiting time: 50ms */

        flashMode = m_flashMgr->getFlashStatus();
        if ((flashMode == AA_FLASHMODE_ON_ALWAYS) || (m_flashMgr->getNeedFlashOffDelay() == true)) {
            int i = 0;
            CLOGD("DEBUG(%s[%d]): flash torch was enabled", __FUNCTION__, __LINE__);

            m_exynosCameraParameters->setFrameSkipCount(100);
            do {
                if (m_flashMgr->checkFlashOff() == false) {
                    usleep(waitingTime);
                } else {
                    CLOGD("DEBUG(%s[%d]):turn off the flash torch.(%d)", __FUNCTION__, __LINE__, i);

                    flashMode = m_flashMgr->getFlashStatus();
                    if (flashMode == AA_FLASHMODE_OFF || flashMode == AA_FLASHMODE_CANCEL)
                    {
                        m_flashMgr->setFlashStep(ExynosCameraActivityFlash::FLASH_STEP_OFF);
                    }
                    usleep(waitingTime);
                    break;
                }
            } while(++i < TOTAL_FLASH_WATING_COUNT);

            if (i >= TOTAL_FLASH_WATING_COUNT) {
                CLOGD("DEBUG(%s[%d]):timeOut-flashMode(%d),checkFlashOff(%d)",
                    __FUNCTION__, __LINE__, flashMode, m_flashMgr->checkFlashOff());
            }
        } else if (m_isTryStopFlash == true) {
            usleep(waitingTime*3);  /* 150ms */
            m_flashMgr->setFlashStep(ExynosCameraActivityFlash::FLASH_STEP_OFF);
        }

        m_flashMgr->setNeedFlashOffDelay(false);

        if (m_previewFrameFactory == NULL) {
            CLOGW("WARN(%s[%d]):m_previewFrameFactory is NULL.", __FUNCTION__, __LINE__);
        } else {
            m_previewFrameFactory->setStopFlag();
        }
        if (m_exynosCameraParameters->isReprocessing() == true && m_reprocessingFrameFactory->isCreated() == true)
            m_reprocessingFrameFactory->setStopFlag();
        m_flagThreadStop = true;

        m_takePictureCounter.clearCount();
        m_reprocessingCounter.clearCount();
        m_pictureCounter.clearCount();
        m_jpegCounter.clearCount();
        m_captureSelector->cancelPicture();

        if ((m_exynosCameraParameters->getHighResolutionCallbackMode() == true) &&
            (m_highResolutionCallbackRunning == true)) {
            m_skipReprocessing = false;
            m_highResolutionCallbackRunning = false;
            CLOGD("DEBUG(%s[%d]):High resolution preview callback stop", __FUNCTION__, __LINE__);
            if(getCameraId() == CAMERA_ID_FRONT) {
                m_sccCaptureSelector->cancelPicture();
                m_sccCaptureSelector->wakeupQ();
                CLOGD("DEBUG(%s[%d]):High resolution m_sccCaptureSelector cancel", __FUNCTION__, __LINE__);
            }

            m_prePictureThread->requestExitAndWait();
            m_highResolutionCallbackQ->release();
        }

        ret = m_stopPictureInternal();
        if (ret < 0)
            CLOGE("ERR(%s[%d]):m_stopPictureInternal fail", __FUNCTION__, __LINE__);

        m_exynosCameraActivityControl->stopAutoFocus();
        m_autoFocusThread->requestExitAndWait();

        if (m_previewQ != NULL) {
            m_previewQ->sendCmd(WAKE_UP);
        } else {
            CLOGI("INFO(%s[%d]): m_previewQ is NULL", __FUNCTION__, __LINE__);
        }

        m_zoomPreviwWithCscThread->stop();
        m_zoomPreviwWithCscQ->sendCmd(WAKE_UP);
        m_zoomPreviwWithCscThread->requestExitAndWait();


        m_pipeFrameDoneQ->sendCmd(WAKE_UP);
        m_mainThread->requestExitAndWait();
        m_monitorThread->requestExitAndWait();

        m_shutterCallbackThread->requestExitAndWait();

        m_previewThread->stop();
        if (m_previewQ != NULL) {
            m_previewQ->sendCmd(WAKE_UP);
        }
        m_previewThread->requestExitAndWait();

        if (m_exynosCameraParameters->isFlite3aaOtf() == true) {
            m_mainSetupQThread[INDEX(PIPE_FLITE)]->stop();
            m_mainSetupQ[INDEX(PIPE_FLITE)]->sendCmd(WAKE_UP);
            m_mainSetupQThread[INDEX(PIPE_FLITE)]->requestExitAndWait();

            if (m_mainSetupQThread[INDEX(PIPE_3AC)] != NULL) {
                m_mainSetupQThread[INDEX(PIPE_3AC)]->stop();
                m_mainSetupQ[INDEX(PIPE_3AC)]->sendCmd(WAKE_UP);
                m_mainSetupQThread[INDEX(PIPE_3AC)]->requestExitAndWait();
            }

            m_mainSetupQThread[INDEX(PIPE_3AA)]->stop();
            m_mainSetupQ[INDEX(PIPE_3AA)]->sendCmd(WAKE_UP);
            m_mainSetupQThread[INDEX(PIPE_3AA)]->requestExitAndWait();

            if (m_mainSetupQThread[INDEX(PIPE_ISP)] != NULL) {
                m_mainSetupQThread[INDEX(PIPE_ISP)]->stop();
                m_mainSetupQ[INDEX(PIPE_ISP)]->sendCmd(WAKE_UP);
                m_mainSetupQThread[INDEX(PIPE_ISP)]->requestExitAndWait();
            }

            /* Comment out, because it included ISP */
            /* m_mainSetupQThread[INDEX(PIPE_SCP)]->requestExitAndWait(); */
        } else {
            if (m_mainSetupQThread[INDEX(PIPE_FLITE)] != NULL) {
                m_mainSetupQThread[INDEX(PIPE_FLITE)]->stop();
                m_mainSetupQ[INDEX(PIPE_FLITE)]->sendCmd(WAKE_UP);
                m_mainSetupQThread[INDEX(PIPE_FLITE)]->requestExitAndWait();
            }
        }

        m_autoFocusContinousThread->stop();
        m_autoFocusContinousQ.sendCmd(WAKE_UP);
        m_autoFocusContinousThread->requestExitAndWait();
        m_autoFocusContinousQ.release();

        m_facedetectThread->stop();
        m_facedetectQ->sendCmd(WAKE_UP);
        m_facedetectThread->requestExitAndWait();
        while (m_facedetectQ->getSizeOfProcessQ()) {
            m_facedetectQ->popProcessQ(&frame);
            frame->decRef();
            m_frameMgr->deleteFrame(frame);
            frame = NULL;
        }

        if (m_previewQ != NULL) {
             m_clearList(m_previewQ);
        }

        if (m_zoomPreviwWithCscQ != NULL) {
             m_zoomPreviwWithCscQ->release();
        }

        if (m_previewCallbackGscFrameDoneQ != NULL) {
            m_clearList(m_previewCallbackGscFrameDoneQ);
        }

        for(int i = 0 ; i < MAX_NUM_PIPES ; i++ ) {
            m_clearList(m_mainSetupQ[i]);
        }

        ret = m_stopPreviewInternal();
        if (ret < 0)
            CLOGE("ERR(%s[%d]):m_stopPreviewInternal fail", __FUNCTION__, __LINE__);

#ifdef SUPPORT_SW_VDIS
        if(m_swVDIS_Mode) {
            m_swVDIS_deinit();
            for (int bufIndex = 0; bufIndex < NUM_VDIS_BUFFERS; bufIndex++) {
                m_putBuffers(m_swVDIS_BufferMgr, bufIndex);
            }
            if (m_swVDIS_BufferMgr != NULL) {
                m_swVDIS_BufferMgr->deinit();
            }

            if (m_previewWindow != NULL)
                m_previewWindow->set_crop(m_previewWindow, 0, 0, 0, 0);

            if(m_exynosCameraParameters->increaseMaxBufferOfPreview()) {
                m_exynosCameraParameters->setPreviewBufferCount(NUM_PREVIEW_BUFFERS + NUM_PREVIEW_SPARE_BUFFERS);
                m_exynosconfig->current->bufInfo.num_preview_buffers = NUM_PREVIEW_BUFFERS + NUM_PREVIEW_SPARE_BUFFERS;
                VDIS_LOG("VDIS_HAL: Preview Buffer Count Change to %d", NUM_PREVIEW_BUFFERS + NUM_PREVIEW_SPARE_BUFFERS);
            } else {
                m_exynosCameraParameters->setPreviewBufferCount(NUM_PREVIEW_BUFFERS);
                m_exynosconfig->current->bufInfo.num_preview_buffers = NUM_PREVIEW_BUFFERS;
                VDIS_LOG("VDIS_HAL: Preview Buffer Count Change to %d", NUM_PREVIEW_BUFFERS);
            }
        }
#endif /*SUPPORT_SW_VDIS*/
#ifdef USE_FASTMOTION_CROP
       if(m_exynosCameraParameters->getShotMode() == SHOT_MODE_FASTMOTION) {
            if (m_previewWindow != NULL) {
                m_previewWindow->set_crop(m_previewWindow, 0, 0, 0, 0);
            }
        }
#endif /* USE_FASTMOTION_CROP */
    }

    /* skip to free and reallocate buffers : flite / 3aa / isp / ispReprocessing */
    if (m_bayerBufferMgr != NULL) {
        m_bayerBufferMgr->resetBuffers();
    }
#ifdef SAMSUNG_DNG
    if (m_fliteBufferMgr != NULL) {
        m_fliteBufferMgr->resetBuffers();
    }
#endif
    if (m_3aaBufferMgr != NULL) {
        m_3aaBufferMgr->resetBuffers();
    }
    if (m_ispBufferMgr != NULL) {
#if defined (USE_ISP_BUFFER_SIZE_TO_BDS)
        m_ispBufferMgr->deinit();
#else
        m_ispBufferMgr->resetBuffers();
#endif
    }
    if (m_hwDisBufferMgr != NULL) {
        m_hwDisBufferMgr->resetBuffers();
    }

    /* realloc reprocessing buffer for change burst panorama <-> normal mode */
    if (m_ispReprocessingBufferMgr != NULL) {
        m_ispReprocessingBufferMgr->resetBuffers();
    }
    if (m_sccReprocessingBufferMgr != NULL) {
        m_sccReprocessingBufferMgr->resetBuffers();
    }

    /* realloc callback buffers */
    if (m_scpBufferMgr != NULL) {
        m_scpBufferMgr->deinit();
        m_scpBufferMgr->setBufferCount(0);
    }
    if (m_zoomScalerBufferMgr != NULL) {
        m_zoomScalerBufferMgr->deinit();
    }
    if (m_sccBufferMgr != NULL) {
        m_sccBufferMgr->resetBuffers();
    }
    if (m_gscBufferMgr != NULL) {
        m_gscBufferMgr->resetBuffers();
    }

    if (m_postPictureGscBufferMgr != NULL) {
        m_postPictureGscBufferMgr->deinit();
    }

    if (m_thumbnailGscBufferMgr != NULL) {
        m_thumbnailGscBufferMgr->deinit();
    }

#ifdef SAMSUNG_LBP
    if (m_lbpBufferMgr != NULL) {
        m_lbpBufferMgr->deinit();
    }
#endif

    if (m_jpegBufferMgr != NULL) {
        m_jpegBufferMgr->deinit();
    }

    if (m_hwDisBufferMgr != NULL) {
        m_hwDisBufferMgr->deinit();
    }

    if (m_recordingBufferMgr != NULL) {
        m_recordingBufferMgr->deinit();
    }
    if (m_previewCallbackBufferMgr != NULL) {
        m_previewCallbackBufferMgr->deinit();
    }
    if (m_highResolutionCallbackBufferMgr != NULL) {
        m_highResolutionCallbackBufferMgr->deinit();
    }
    if (m_captureSelector != NULL) {
        m_captureSelector->release();
    }
    if (m_sccCaptureSelector != NULL) {
        m_sccCaptureSelector->release();
    }

#if 0
    /* skip to free and reallocate buffers : flite / 3aa / isp / ispReprocessing */
    CLOGE(" m_setBuffers free all buffers");
    if (m_bayerBufferMgr != NULL) {
        m_bayerBufferMgr->deinit();
    }
    if (m_3aaBufferMgr != NULL) {
        m_3aaBufferMgr->deinit();
    }
    if (m_ispBufferMgr != NULL) {
        m_ispBufferMgr->deinit();
    }
    if (m_hwDisBufferMgr != NULL) {
        m_hwDisBufferMgr->deinit();
    }
#endif
    /* frame manager stop */
    m_frameMgr->stop();
    m_frameMgr->deleteAllFrame();


    m_reprocessingCounter.clearCount();
    m_pictureCounter.clearCount();

    m_hdrSkipedFcount = 0;
    m_dynamicSccCount = 0;

    /* HACK Reset Preview Flag*/
    m_resetPreview = false;

    m_isTryStopFlash= false;
    m_exitAutoFocusThread = false;
    m_isNeedAllocPictureBuffer = false;

    if (m_fdCallbackHeap != NULL) {
        m_fdCallbackHeap->release(m_fdCallbackHeap);
        m_fdCallbackHeap = NULL;
    }

#ifdef SR_CAPTURE
    if (m_srCallbackHeap != NULL) {
        m_srCallbackHeap->release(m_srCallbackHeap);
        m_srCallbackHeap = NULL;
    }
#endif

    m_burstInitFirst = false;

#ifdef SAMSUNG_JQ
    if(m_JQpluginHandle != NULL) {
        ret = uni_plugin_deinit(m_JQpluginHandle);
        if(ret < 0) {
            CLOGE("[JQ](%s[%d]): Plugin deinit failed!!", __FUNCTION__, __LINE__);
        }
    }
#endif

#ifdef SAMSUNG_DEBLUR
    if(m_DeblurpluginHandle != NULL) {
        ret = uni_plugin_deinit(m_DeblurpluginHandle);
        if(ret < 0) {
            CLOGE("[Deblur](%s[%d]): Deblur Plugin deinit failed!!", __FUNCTION__, __LINE__);
        }
    }
#endif

#ifdef SAMSUNG_DOF
    if(m_lensmoveCount) {
        CLOGW("[DOF][%s][%d] Out-focus shot parameter is not reset. Reset here forcely!!: %d",
            __FUNCTION__, __LINE__, m_lensmoveCount);
        ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
        autoFocusMgr->setStartLensMove(false);
        m_lensmoveCount = 0;
        m_exynosCameraParameters->setMoveLensCount(m_lensmoveCount);
        m_exynosCameraParameters->setMoveLensTotal(m_lensmoveCount);
    }
#endif

#ifdef SAMSUNG_SENSOR_LISTENER
    if (m_sensorListenerThread != NULL) {
        m_sensorListenerThread->requestExitAndWait();
    }

#ifdef SAMSUNG_HRM
    if(m_uv_rayHandle != NULL) {
        m_exynosCameraParameters->m_setHRM_Hint(false);
    }
#endif

#ifdef SAMSUNG_LIGHT_IR
    if(m_light_irHandle != NULL) {
        m_exynosCameraParameters->m_setLight_IR_Hint(false);
    }
#endif

#ifdef SAMSUNG_GYRO
    if(m_gyroHandle != NULL) {
        m_exynosCameraParameters->m_setGyroHint(false);
    }
#endif
#endif /* SAMSUNG_SENSOR_LISTENER */

#ifdef FORCE_RESET_MULTI_FRAME_FACTORY
    /*
     * HACK
     * This is force-reset frameFactory adn companion
     */
    m_deinitFrameFactory();

    /*
     * close companion : other node all closed in m_deinitFrameFactory()
     * so, we close also companion
     */
    if (m_stopCompanion() != NO_ERROR)
        CLOGE("ERR(%s[%d]):m_stopCompanion() fail", __FUNCTION__, __LINE__);

    m_initFrameFactory();
#endif

#ifdef CAMERA_FAST_ENTRANCE_V1
    m_fastEntrance = false;
#endif
}

bool ExynosCamera::previewEnabled()
{
    CLOGI("INFO(%s[%d]):m_previewEnabled=%d",
        __FUNCTION__, __LINE__, (int)m_previewEnabled);

    /* in scalable mode, we should controll out state */
    if (m_exynosCameraParameters != NULL &&
        (m_exynosCameraParameters->getScalableSensorMode() == true) &&
        (m_scalableSensorMgr.getMode() == EXYNOS_CAMERA_SCALABLE_CHANGING))
        return true;
    else
        return m_previewEnabled;
}

status_t ExynosCamera::storeMetaDataInBuffers( __unused bool enable)
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    return OK;
}

status_t ExynosCamera::startRecording()
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);
#ifdef SAMSUNG_HLV
    int curVideoW = 0, curVideoH = 0;
    uint32_t curMinFps = 0, curMaxFps = 0;
#endif
    if (m_exynosCameraParameters != NULL) {
        if (m_exynosCameraParameters->getVisionMode() == true) {
            CLOGW("WRN(%s[%d]): Vision mode does not support", __FUNCTION__, __LINE__);
            android_printAssert(NULL, LOG_TAG, "Cannot support this operation");

            return INVALID_OPERATION;
        }
    }

    int ret = 0;
    ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
    ExynosCameraActivityFlash *flashMgr = m_exynosCameraActivityControl->getFlashMgr();
    flashMgr->setCaptureStatus(false);

    if (m_getRecordingEnabled() == true) {
        CLOGW("WARN(%s[%d]):m_recordingEnabled equals true", __FUNCTION__, __LINE__);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

#ifdef USE_FD_AE
    if (m_exynosCameraParameters != NULL) {
        if (m_exynosCameraParameters->getFaceDetectionMode() == false) {
            m_startFaceDetection(false);
        } else {
            /* stay current fd mode */
        }
    } else {
        CLOGW("(%s[%d]):m_exynosCameraParameters is NULL", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }
#endif


    /* Do start recording process */
    ret = m_startRecordingInternal();
    if (ret < 0) {
        CLOGE("ERR");
        return ret;
    }

#ifdef SAMSUNG_HLV
    m_exynosCameraParameters->getVideoSize(&curVideoW, &curVideoH);
    m_exynosCameraParameters->getPreviewFpsRange(&curMinFps, &curMaxFps);

    if (curVideoW <= 1920 && curVideoH <= 1080
        && m_HLV == NULL
        && curMinFps <= 60 && curMaxFps <= 60
        && !m_exynosCameraParameters->isSWVdisMode()
#ifdef USE_LIVE_BROADCAST
        && m_exynosCameraParameters->getPLBMode() == false
#endif
    ) {
        ALOGD("HLV : uni_plugin_load !");
        m_HLV = uni_plugin_load(HIGHLIGHT_VIDEO_PLUGIN_NAME);
        if (m_HLV != NULL) {
            ALOGD("HLV : uni_plugin_load success!");

            ret = uni_plugin_init(m_HLV);
            if (ret < 0) {
                ALOGE("HLV : uni_plugin_init fail . Unload %s", HIGHLIGHT_VIDEO_PLUGIN_NAME);

                ret = uni_plugin_unload(&m_HLV);
                if (ret)
                    ALOGE("HLV : uni_plug_unload failed !!");

                m_HLV = NULL;
            }
            else
            {
                ALOGD("HLV : uni_plugin_init success!");
                UNI_PLUGIN_CAPTURE_STATUS value = UNI_PLUGIN_CAPTURE_STATUS_VID_REC_START;
                ALOGD("HLV: set UNI_PLUGIN_CAPTURE_STATUS_VID_REC_START");

                uni_plugin_set(m_HLV,
                        HIGHLIGHT_VIDEO_PLUGIN_NAME,
                        UNI_PLUGIN_INDEX_CAPTURE_STATUS,
                        &value);
            }
        }
        else {
            ALOGD("HLV : uni_plugin_load fail!");
        }
    }
    else {
        ALOGD("HLV : Not Supported for %dx%d(%d fps) video record !",
            curVideoW, curVideoH, curMinFps);
    }
#endif

    m_lastRecordingTimeStamp  = 0;
    m_recordingStartTimeStamp = 0;
    m_recordingFrameSkipCount = 0;

    m_setRecordingEnabled(true);

    autoFocusMgr->setRecordingHint(true);
    flashMgr->setRecordingHint(true);

func_exit:
    /* wait for initial preview skip */
    if (m_exynosCameraParameters != NULL) {
        int retry = 0;
        while (m_exynosCameraParameters->getFrameSkipCount() > 0 && retry < 3) {
            retry++;
            usleep(33000);
            CLOGI("INFO(%s[%d]): -OUT- (frameSkipCount:%d) (retry:%d)", __FUNCTION__, __LINE__, m_frameSkipCount, retry);
        }
    }

    return NO_ERROR;
}

void ExynosCamera::stopRecording()
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    if (m_exynosCameraParameters != NULL) {
        if (m_exynosCameraParameters->getVisionMode() == true) {
            CLOGW("WRN(%s[%d]): Vision mode does not support", __FUNCTION__, __LINE__);
            android_printAssert(NULL, LOG_TAG, "Cannot support this operation");

            return;
        }
    }

    int ret = 0;
    ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
    ExynosCameraActivityFlash *flashMgr = m_exynosCameraActivityControl->getFlashMgr();
    m_skipCount = 0;

    if (m_getRecordingEnabled() == false) {
        return;
    }
    m_setRecordingEnabled(false);

#ifdef SAMSUNG_HLV
    if (m_HLV != NULL)
    {
        UNI_PLUGIN_CAPTURE_STATUS value = UNI_PLUGIN_CAPTURE_STATUS_VID_REC_STOP;
        ALOGD("HLV: set UNI_PLUGIN_CAPTURE_STATUS_VID_REC_STOP");

        uni_plugin_set(m_HLV,
                HIGHLIGHT_VIDEO_PLUGIN_NAME,
                UNI_PLUGIN_INDEX_CAPTURE_STATUS,
                &value);

        /* De-Init should be called before m_stopRecordingInternal to avoid race-condition in libHighLightVideo */
        ALOGD("HLV : uni_plugin_deinit  !!!");
        ret = uni_plugin_deinit(m_HLV);
        if (ret) {
            ALOGE("HLV : uni_plugin_deinit failed !! Possible Memory Leak !!!");
        }

        ret = uni_plugin_unload(&m_HLV);
        if (ret)
            ALOGE("HLV : uni_plug_unload failed !!  Possible Memory Leak !!!");

        m_HLV = NULL;
        ALOGD("HLV : process_step : %d", m_HLVprocessStep);
    }
#endif

    /* Do stop recording process */
    ret = m_stopRecordingInternal();
    if (ret < 0)
        CLOGE("ERR(%s[%d]):m_stopRecordingInternal fail", __FUNCTION__, __LINE__);

#ifdef USE_FD_AE
    if (m_exynosCameraParameters != NULL) {
        m_startFaceDetection(m_exynosCameraParameters->getFaceDetectionMode(false));
    }
#endif

    autoFocusMgr->setRecordingHint(false);
    flashMgr->setRecordingHint(false);
    flashMgr->setNeedFlashOffDelay(false);
}

bool ExynosCamera::recordingEnabled()
{
    bool ret = m_getRecordingEnabled();
    CLOGI("INFO(%s[%d]):m_recordingEnabled=%d",
        __FUNCTION__, __LINE__, (int)ret);

    return ret;
}

void ExynosCamera::releaseRecordingFrame(const void *opaque)
{
    if (m_exynosCameraParameters != NULL) {
        if (m_exynosCameraParameters->getVisionMode() == true) {
            CLOGW("WRN(%s[%d]): Vision mode does not support", __FUNCTION__, __LINE__);
            android_printAssert(NULL, LOG_TAG, "Cannot support this operation");

            return;
        }
    }

    if (m_getRecordingEnabled() == false) {
        CLOGW("WARN(%s[%d]):m_recordingEnabled equals false", __FUNCTION__, __LINE__);
        /* m_stopRecordingInternal() will wait for recording frame release */
        /* return; */
    }

    if (m_recordingCallbackHeap == NULL) {
        CLOGW("WARN(%s[%d]):recordingCallbackHeap equals NULL", __FUNCTION__, __LINE__);
        return;
    }

    struct VideoNativeHandleMetadata *releaseMetadata = (struct VideoNativeHandleMetadata *) opaque;
    native_handle_t *releaseHandle = NULL;
    int releaseBufferIndex = -1;

    if (releaseMetadata->eType != kMetadataBufferTypeNativeHandleSource) {
        CLOGW("WARN(%s[%d]):Inavlid VideoNativeHandleMetadata Type %d",
                __FUNCTION__, __LINE__,
                releaseMetadata->eType);
        return;
    }

    /*
     * Support only NV21M,
     * data[0]: Y
     * data[1]: CbCr
     * data[2]: bufferIndex
     */
    releaseHandle = releaseMetadata->pHandle;
    releaseBufferIndex = releaseHandle->data[2];
    if (releaseBufferIndex >= m_recordingBufferCount) {
        CLOGW("WARN(%s[%d]):Invalid VideoBufferIndex %d",
                __FUNCTION__, __LINE__,
                releaseBufferIndex);

        goto CLEAN;
    } else if (m_recordingBufAvailable[releaseBufferIndex] == true) {
        CLOGW("WARN(%s[%d]):Already released VideoBufferIndex %d",
                __FUNCTION__, __LINE__,
                releaseBufferIndex);

        goto CLEAN;
    }

    if (m_doCscRecording == true) {
        m_releaseRecordingBuffer(releaseBufferIndex);
    }

    m_recordingTimeStamp[releaseBufferIndex] = 0L;
    m_recordingBufAvailable[releaseBufferIndex] = true;

    m_isFirstStart = false;
    if (m_exynosCameraParameters != NULL) {
        m_exynosCameraParameters->setIsFirstStartFlag(m_isFirstStart);
    }

CLEAN:
    if (releaseHandle != NULL) {
        native_handle_close(releaseHandle);
        native_handle_delete(releaseHandle);
    }

    return;
}

status_t ExynosCamera::autoFocus()
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    if (m_exynosCameraParameters != NULL) {
        if (m_exynosCameraParameters->getVisionMode() == true) {
            CLOGW("WRN(%s[%d]): Vision mode does not support", __FUNCTION__, __LINE__);
            android_printAssert(NULL, LOG_TAG, "Cannot support this operation");

            return INVALID_OPERATION;
        }
    }

    if (m_previewEnabled == false) {
        CLOGE("ERR(%s[%d]): preview is not enabled", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    if (m_autoFocusRunning == false) {
        m_autoFocusType = AUTO_FOCUS_SERVICE;
        m_autoFocusThread->requestExitAndWait();
        m_autoFocusThread->run(PRIORITY_DEFAULT);
    } else {
        CLOGW("WRN(%s[%d]): auto focus is inprogressing", __FUNCTION__, __LINE__);
    }

#if 0 // not used.
    if (m_exynosCameraParameters != NULL) {
        if (m_exynosCameraParameters->getFocusMode() == FOCUS_MODE_AUTO) {
            CLOGI("INFO(%s[%d]) ae awb lock", __FUNCTION__, __LINE__);
            m_exynosCameraParameters->m_setAutoExposureLock(true);
            m_exynosCameraParameters->m_setAutoWhiteBalanceLock(true);
        }
    }
#endif

    return NO_ERROR;
}

status_t ExynosCamera::cancelAutoFocus()
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    if (m_exynosCameraParameters != NULL) {
        if (m_exynosCameraParameters->getVisionMode() == true) {
            CLOGW("WRN(%s[%d]): Vision mode does not support", __FUNCTION__, __LINE__);
            android_printAssert(NULL, LOG_TAG, "Cannot support this operation");

            return INVALID_OPERATION;
        }
    }

    m_autoFocusRunning = false;

#ifdef SAMSUNG_DOF
    if(m_lensmoveCount) {
        CLOGW("[DOF][%s][%d] Out-focus shot parameter is not reset. Reset here forcely!!: %d",
            __FUNCTION__, __LINE__, m_lensmoveCount);
        ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
        autoFocusMgr->setStartLensMove(false);
        m_lensmoveCount = 0;
        m_exynosCameraParameters->setMoveLensCount(m_lensmoveCount);
        m_exynosCameraParameters->setMoveLensTotal(m_lensmoveCount);
    }
#endif

#if 0 // not used.
    if (m_exynosCameraParameters != NULL) {
        if (m_exynosCameraParameters->getFocusMode() == FOCUS_MODE_AUTO) {
            CLOGI("INFO(%s[%d]) ae awb unlock", __FUNCTION__, __LINE__);
            m_exynosCameraParameters->m_setAutoExposureLock(false);
            m_exynosCameraParameters->m_setAutoWhiteBalanceLock(false);
        }
    }
#endif

    if (m_exynosCameraActivityControl->cancelAutoFocus() == false) {
        CLOGE("ERR(%s):Fail on m_secCamera->cancelAutoFocus()", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    /* if autofocusThread is running, we should be wait to receive the AF reseult. */
    m_autoFocusLock.lock();
    m_autoFocusLock.unlock();

    return NO_ERROR;
}

status_t ExynosCamera::takePicture()
{
    int ret = 0;
    int takePictureCount = m_takePictureCounter.getCount();
    int seriesShotCount = 0;
    int currentSeriesShotMode = 0;
    ExynosCameraFrame *newFrame = NULL;
    int32_t reprocessingBayerMode = 0;
    int retryCount = 0;

    if (m_previewEnabled == false) {
        CLOGE("DEBUG(%s):preview is stopped, return error", __FUNCTION__);
        return INVALID_OPERATION;
    }

    if (m_exynosCameraParameters == NULL) {
        CLOGE("ERR(%s):m_exynosCameraParameters is NULL", __FUNCTION__);
        return INVALID_OPERATION;
    }

    while (m_longExposurePreview) {
        m_exynosCameraParameters->setPreviewExposureTime();

        uint64_t delay = 0;
        if (m_exynosCameraParameters->getCaptureExposureTime() <= 33333) {
            delay = 30000;
        } else if (m_exynosCameraParameters->getCaptureExposureTime() > CAMERA_EXPOSURE_TIME_MAX) {
            delay = CAMERA_EXPOSURE_TIME_MAX;
        } else {
            delay = m_exynosCameraParameters->getCaptureExposureTime();
        }

        usleep(delay);

        if (++retryCount > 7) {
            CLOGE("DEBUG(%s):HAL can not set preview exposure time. Returns error.", __FUNCTION__);
            return INVALID_OPERATION;
        }
    }

    retryCount = 0;
#ifdef SAMSUNG_DNG
    m_dngFrameNumberForManualExposure = 0;
#endif

    /* wait autoFocus is over for turning on preFlash */
    m_autoFocusThread->join();

#ifdef ONE_SECOND_BURST_CAPTURE
    /* For Sync with jpegCallbackThread */
    m_captureLock.lock();
#endif

    seriesShotCount = m_exynosCameraParameters->getSeriesShotCount();
    currentSeriesShotMode = m_exynosCameraParameters->getSeriesShotMode();
    reprocessingBayerMode = m_exynosCameraParameters->getReprocessingBayerMode();
    if (m_exynosCameraParameters->getVisionMode() == true) {
        CLOGW("WRN(%s[%d]): Vision mode does not support", __FUNCTION__, __LINE__);
        android_printAssert(NULL, LOG_TAG, "Cannot support this operation");

#ifdef ONE_SECOND_BURST_CAPTURE
        m_captureLock.unlock();
#endif
        return INVALID_OPERATION;
    }

    if (m_exynosCameraParameters->getShotMode() == SHOT_MODE_RICH_TONE) {
        m_hdrEnabled = true;
    } else {
        m_hdrEnabled = false;
    }

#ifdef ONE_SECOND_BURST_CAPTURE
    TakepictureDurationTimer.stop();
    if (m_exynosCameraParameters->getSamsungCamera() && getCameraId() == CAMERA_ID_BACK
        && !m_exynosCameraParameters->getRecordingHint()
        && !m_getRecordingEnabled()
        && !m_exynosCameraParameters->getDualMode()
        && !m_exynosCameraParameters->getEffectHint()
#ifdef SAMSUNG_DNG
        && !m_exynosCameraParameters->getDNGCaptureModeOn()
#endif
        && (currentSeriesShotMode == SERIES_SHOT_MODE_NONE || currentSeriesShotMode == SERIES_SHOT_MODE_ONE_SECOND_BURST)) {
        uint64_t delay;
        uint64_t sum;
        delay = TakepictureDurationTimer.durationUsecs();
        ExynosCameraActivityFlash *m_flashMgr = m_exynosCameraActivityControl->getFlashMgr();
        ExynosCameraActivitySpecialCapture *m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();

        /* set smallest delay */
        if (ONE_SECOND_BURST_CAPTURE_TAKEPICTURE_COUNT == 1
            && m_one_second_burst_first_after_open == true)
            delay = 1;

        /* Check other shot(OIS, HDR without companion, FLASH) launching */
#ifdef OIS_CAPTURE
        if(getCameraId() == CAMERA_ID_BACK && currentSeriesShotMode != SERIES_SHOT_MODE_BURST) {
            m_sCaptureMgr->resetOISCaptureFcount();
            m_exynosCameraParameters->checkOISCaptureMode();
#ifdef SAMSUNG_LLS_DEBLUR
            m_exynosCameraParameters->checkLDCaptureMode();
#endif
        }
#endif
        /* SR, Burst, LLS is already disable on sendCommand() */
        if (m_hdrEnabled
            || (m_exynosCameraParameters->getShotMode() == SHOT_MODE_OUTFOCUS)
            || (m_exynosCameraParameters->getFlashMode() == FLASH_MODE_ON)
            || (getCameraId() == CAMERA_ID_BACK && m_exynosCameraParameters->getOISCaptureModeOn() == true)
#ifdef SAMSUNG_LLS_DEBLUR
            || (m_exynosCameraParameters->getLDCaptureMode() > 0)
#endif
            || (m_flashMgr->getNeedCaptureFlash() == true && currentSeriesShotMode == SERIES_SHOT_MODE_NONE)
            || (m_exynosCameraParameters->getCaptureExposureTime() != 0)
            || m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_RAW_IMAGE)) { // HAL test (Ext_SecCameraActivityTest_testTakePicture_P01)
            /* Check other shot only. reset Capturemode */
            m_exynosCameraParameters->setOISCaptureModeOn(false);
#ifdef SAMSUNG_LLS_DEBLUR
            m_exynosCameraParameters->setLDCaptureMode(MULTI_SHOT_MODE_NONE);
            m_exynosCameraParameters->setLDCaptureCount(0);
            m_exynosCameraParameters->setLDCaptureLLSValue(LLS_LEVEL_ZSL);
#endif
            /* For jpegCallbackThreadFunc() exit */
            m_captureLock.unlock();
            m_clearOneSecondBurst(false);
            m_captureLock.lock();
            /* Reset value */
            currentSeriesShotMode = m_exynosCameraParameters->getSeriesShotMode();
            seriesShotCount = m_exynosCameraParameters->getSeriesShotCount();
            takePictureCount = m_takePictureCounter.getCount();
            /* clear delay */
            delay = 0;
        }

        if (currentSeriesShotMode == SERIES_SHOT_MODE_NONE) {
            if (delay <= ONE_SECOND_BURST_CAPTURE_CHECK_TIME * ONE_MSECOND) {
                sum = 0;
                for (int i = 0; i < ONE_SECOND_BURST_CAPTURE_CHECK_COUNT; i++) {
                    if (TakepictureDurationTime[i] != 0)
                        sum += TakepictureDurationTime[i];

                    if (TakepictureDurationTime[i] == 0) {
                        TakepictureDurationTime[i] = delay;
                        sum += TakepictureDurationTime[i];
                        break;
                    }
                }
                if (sum >= ONE_SECOND_BURST_CAPTURE_CHECK_TIME * ONE_MSECOND) {
                    /* Remove unnecessary time */
                    for (int i = 0; i < ONE_SECOND_BURST_CAPTURE_CHECK_COUNT - 1; i++) {
                        TakepictureDurationTime[i] = TakepictureDurationTime[i + 1];
                    }
                    TakepictureDurationTime[ONE_SECOND_BURST_CAPTURE_CHECK_COUNT - 1] = 0;
                } else {
                    if (TakepictureDurationTime[ONE_SECOND_BURST_CAPTURE_CHECK_COUNT - 1] != 0) {
                        /* do one second burst */
                        CLOGD("DEBUG(%s[%d]): One second burst start - TimeCheck", __FUNCTION__, __LINE__);
                        m_one_second_burst_first_after_open = true;
                        for (int i = 0; i < ONE_SECOND_BURST_CAPTURE_CHECK_COUNT; i++) {
                            TakepictureDurationTime[i] = 0;
                        }
                        /* Set one second burst parameters */
                        m_one_second_burst_capture = true;

                        if (m_burstInitFirst) {
                            m_burstRealloc = true;
                            m_burstInitFirst = false;
                        }
                        m_exynosCameraParameters->setSeriesShotMode(SERIES_SHOT_MODE_ONE_SECOND_BURST);
                        /* Set value */
                        currentSeriesShotMode = m_exynosCameraParameters->getSeriesShotMode();
                        seriesShotCount = m_exynosCameraParameters->getSeriesShotCount();
                        m_takePictureCounter.setCount(0);
                        takePictureCount = m_takePictureCounter.getCount();

                        m_burstCaptureCallbackCount = 0;
                        m_exynosCameraParameters->setSeriesShotSaveLocation(BURST_SAVE_CALLBACK);
                        m_jpegCallbackCounter.setCount(seriesShotCount);
#ifdef USE_DVFS_LOCK
                        m_exynosCameraParameters->setDvfsLock(true);
#endif
                        /* Clear saved CallbackHeap */
                        if (m_one_second_jpegCallbackHeap != NULL) {
                            m_one_second_jpegCallbackHeap->release(m_one_second_jpegCallbackHeap);
                            m_one_second_jpegCallbackHeap = NULL;
                        }
                        if (m_one_second_postviewCallbackHeap != NULL) {
                            m_one_second_postviewCallbackHeap->release(m_one_second_postviewCallbackHeap);
                            m_one_second_postviewCallbackHeap = NULL;
                        }
                    }
                }
            } else {
                /* Init TakepictureDurationTime. Delay is too big */
                for (int i = 0; i < ONE_SECOND_BURST_CAPTURE_CHECK_COUNT; i++)
                    TakepictureDurationTime[i] = 0;
            }
        } else if (currentSeriesShotMode == SERIES_SHOT_MODE_ONE_SECOND_BURST) {
            /* More one second burst */
            /* We add 1 to seriesShotCount because we can't know thread state.
	       We will check m_jpegCallbackCounter is smaller than 1 */
            CLOGD("DEBUG(%s[%d]): Continue One second burst - TimeCheck", __FUNCTION__, __LINE__);
            m_reprocessingCounter.setCount(seriesShotCount + 1);
            m_jpegCounter.setCount(seriesShotCount + 1);
            m_pictureCounter.setCount(seriesShotCount + 1);
            m_jpegCallbackCounter.setCount(seriesShotCount);
            /* Make difference m_takePictureCounter value with seriesShotCount */
            m_takePictureCounter.setCount(seriesShotCount - 1);
            takePictureCount = m_takePictureCounter.getCount();
            m_burstCaptureCallbackCount = 0;
            m_one_second_burst_capture = true;

            if (m_prePictureThread->isRunning() == false) {
                if (m_prePictureThread->run(PRIORITY_DEFAULT) != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):couldn't run pre-picture thread", __FUNCTION__, __LINE__);
                    m_captureLock.unlock();
                    return INVALID_OPERATION;
                }
            }
            if (m_pictureThread->isRunning() == false) {
                if (m_pictureThread->run() != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):couldn't run picture thread, ret(%d)", __FUNCTION__, __LINE__, ret);
                    m_captureLock.unlock();
                    return INVALID_OPERATION;
                }
            }
        }
    }
    TakepictureDurationTimer.start();
#endif

#ifdef ONE_SECOND_BURST_CAPTURE
    m_captureLock.unlock();
#endif

    m_exynosCameraParameters->setMarkingOfExifFlash(0);

#ifdef SAMSUNG_LBP
    if (getCameraId() == CAMERA_ID_FRONT)
        m_exynosCameraParameters->resetNormalBestFrameCount();
#endif

    /* HACK Reset Preview Flag*/
    while ((m_resetPreview == true) && (retryCount < 10)) {
        usleep(200000);
        retryCount ++;
        CLOGI("INFO(%s[%d]) retryCount(%d) m_resetPreview(%d)", __FUNCTION__, __LINE__, retryCount, m_resetPreview);
    }

    if (takePictureCount < 0) {
        CLOGE("ERR(%s[%d]): takePicture is called too much. takePictureCount(%d) / seriesShotCount(%d) . so, fail",
            __FUNCTION__, __LINE__, takePictureCount, seriesShotCount);
        return INVALID_OPERATION;
    } else if (takePictureCount == 0) {
        if (seriesShotCount == 0) {
            m_captureLock.lock();
            if (m_pictureEnabled == true) {
                CLOGE("ERR(%s[%d]): take picture is inprogressing", __FUNCTION__, __LINE__);
                /* return NO_ERROR; */
                m_captureLock.unlock();
                return INVALID_OPERATION;
            }
            m_captureLock.unlock();

            /* general shot */
            seriesShotCount = 1;
        }
        m_takePictureCounter.setCount(seriesShotCount);
    }

    CLOGI("INFO(%s[%d]): takePicture start m_takePictureCounter(%d), currentSeriesShotMode(%d) seriesShotCount(%d)",
        __FUNCTION__, __LINE__, m_takePictureCounter.getCount(), currentSeriesShotMode, seriesShotCount);

    m_printExynosCameraInfo(__FUNCTION__);

    /* TODO: Dynamic bayer capture, currently support only single shot */
    if (reprocessingBayerMode == REPROCESSING_BAYER_MODE_PURE_DYNAMIC) {
        int pipeId = m_getBayerPipeId();

        if (m_bayerBufferMgr->getNumOfAvailableBuffer() > 0) {
            m_previewFrameFactory->setRequestFLITE(true);
            ret = generateFrame(-1, &newFrame);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
                return ret;
            }
            m_previewFrameFactory->setRequestFLITE(false);

            ret = m_setupEntity(pipeId, newFrame);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):setupEntity fail", __FUNCTION__, __LINE__);
                return ret;
            }

            m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId);
            m_previewFrameFactory->pushFrameToPipe(&newFrame, pipeId);
        }

        m_previewFrameFactory->startThread(pipeId);
    } else if (reprocessingBayerMode == REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC) {
        /* Comment out, because it included 3AA, it always running */
        /*
        int pipeId = m_getBayerPipeId();

        if (m_bayerBufferMgr->getNumOfAvailableBuffer() > 0) {
            m_previewFrameFactory->setRequest3AC(true);
            ret = generateFrame(-1, &newFrame);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
                return ret;
            }
            m_previewFrameFactory->setRequest3AC(false);

            ret = m_setupEntity(pipeId, newFrame);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):setupEntity fail", __FUNCTION__, __LINE__);
                return ret;
            }

            m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId);
            m_previewFrameFactory->pushFrameToPipe(&newFrame, pipeId);
        }

        m_previewFrameFactory->startThread(pipeId);
        */
        if (m_bayerBufferMgr->getNumOfAvailableBuffer() > 0)
            m_previewFrameFactory->setRequest3AC(true);
    }

    if (m_takePictureCounter.getCount() == seriesShotCount) {
        ExynosCameraActivitySpecialCapture *m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();
        ExynosCameraActivityFlash *m_flashMgr = m_exynosCameraActivityControl->getFlashMgr();

        m_stopBurstShot = false;
#ifdef BURST_CAPTURE
        m_burstShutterLocation = BURST_SHUTTER_PREPICTURE;
#endif

        if (m_exynosCameraParameters->isReprocessing() == true)
            m_captureSelector->setIsFirstFrame(true);
        else
            m_sccCaptureSelector->setIsFirstFrame(true);

#if 0
        if (m_exynosCameraParameters->isReprocessing() == false || m_exynosCameraParameters->getSeriesShotCount() > 0 ||
                m_hdrEnabled == true) {
            m_pictureFrameFactory = m_previewFrameFactory;
            if (m_exynosCameraParameters->getUseDynamicScc() == true) {
                if (isOwnScc(getCameraId()) == true)
                    m_previewFrameFactory->setRequestSCC(true);
                else
                    m_previewFrameFactory->setRequestISPC(true);

                /* boosting dynamic SCC */
                if (m_hdrEnabled == false &&
                    currentSeriesShotMode == SERIES_SHOT_MODE_NONE) {
                    ret = m_boostDynamicCapture();
                    if (ret < 0)
                        CLOGW("WRN(%s[%d]): fail to boosting dynamic capture", __FUNCTION__, __LINE__);
                }

            }
        } else {
            m_pictureFrameFactory = m_reprocessingFrameFactory;
        }
#endif

        if (m_exynosCameraParameters->getScalableSensorMode()) {
            m_exynosCameraParameters->setScalableSensorMode(false);
            stopPreview();
            setPreviewWindow(m_previewWindow);
            startPreview();
            m_exynosCameraParameters->setScalableSensorMode(true);
        }

        CLOGI("INFO(%s[%d]): takePicture enabled, takePictureCount(%d)",
                __FUNCTION__, __LINE__, m_takePictureCounter.getCount());
        m_pictureEnabled = true;
        m_takePictureCounter.decCount();
        m_isNeedAllocPictureBuffer = true;

        m_startPictureBufferThread->join();

        if (m_exynosCameraParameters->isReprocessing() == true) {
            m_startPictureInternalThread->join();

#ifdef BURST_CAPTURE
#ifdef CAMERA_GED_FEATURE
            if (seriesShotCount > 1)
#else
#ifdef ONE_SECOND_BURST_CAPTURE
            if ((m_exynosCameraParameters->getSeriesShotMode() == SERIES_SHOT_MODE_BURST
                 || m_exynosCameraParameters->getSeriesShotMode() == SERIES_SHOT_MODE_ONE_SECOND_BURST)
                 && m_burstRealloc == true)
#else
            if (m_exynosCameraParameters->getSeriesShotMode() == SERIES_SHOT_MODE_BURST && m_burstRealloc == true)
#endif
#endif
            {
                int allocCount = 0;
                int addCount = 0;
                CLOGD("DEBUG(%s[%d]): realloc buffer for burst shot", __FUNCTION__, __LINE__);
                m_burstRealloc = false;

                allocCount = m_sccReprocessingBufferMgr->getAllocatedBufferCount();
                addCount = (allocCount <= NUM_BURST_GSC_JPEG_INIT_BUFFER)?(NUM_BURST_GSC_JPEG_INIT_BUFFER-allocCount):0;
                if( addCount > 0 ){
                    m_sccReprocessingBufferMgr->increase(addCount);
                }

                allocCount = m_jpegBufferMgr->getAllocatedBufferCount();
                addCount = (allocCount <= NUM_BURST_GSC_JPEG_INIT_BUFFER)?(NUM_BURST_GSC_JPEG_INIT_BUFFER-allocCount):0;
                if( addCount > 0 ){
                    m_jpegBufferMgr->increase(addCount);
                }
                m_isNeedAllocPictureBuffer = true;
            }
#endif
        }

        CLOGD("DEBUG(%s[%d]): currentSeriesShotMode(%d), m_flashMgr->getNeedCaptureFlash(%d)",
            __FUNCTION__, __LINE__, currentSeriesShotMode, m_flashMgr->getNeedCaptureFlash());

#ifdef RAWDUMP_CAPTURE
        if(m_use_companion == true) {
            CLOGD("DEBUG(%s[%d]): start set Raw Capture mode", __FUNCTION__, __LINE__);
            m_sCaptureMgr->resetRawCaptureFcount();
            m_sCaptureMgr->setCaptureMode(ExynosCameraActivitySpecialCapture::SCAPTURE_MODE_RAW);

            m_exynosCameraParameters->setRawCaptureModeOn(true);

            enum aa_capture_intent captureIntent = AA_CAPTRUE_INTENT_STILL_CAPTURE_COMP_BYPASS;

            ret = m_previewFrameFactory->setControl(V4L2_CID_IS_INTENT, captureIntent, PIPE_3AA);
            if (ret < 0)
                CLOGE("ERR(%s[%d]):setControl(STILL_CAPTURE_RAW) fail. ret(%d) intent(%d)",
                __FUNCTION__, __LINE__, ret, captureIntent);
            m_sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_START);
        }
#else
#ifdef OIS_CAPTURE
        if (getCameraId() == CAMERA_ID_BACK
#ifdef ONE_SECOND_BURST_CAPTURE
            && currentSeriesShotMode != SERIES_SHOT_MODE_ONE_SECOND_BURST
#endif
            && !m_getRecordingEnabled()
        ) {
            m_sCaptureMgr->resetOISCaptureFcount();
            m_exynosCameraParameters->checkOISCaptureMode();
#if defined(SAMSUNG_DEBLUR) || defined(SAMSUNG_LLS_DEBLUR)
            if (currentSeriesShotMode == SERIES_SHOT_MODE_NONE) {
                m_exynosCameraParameters->checkLDCaptureMode();
                CLOGD("LDC(%s[%d]): currentSeriesShotMode(%d), getLDCaptureMode(%d)",
                        __FUNCTION__, __LINE__, currentSeriesShotMode, m_exynosCameraParameters->getLDCaptureMode());
            }
#ifdef SAMSUNG_DEBLUR
            if (m_exynosCameraParameters->getLDCaptureMode() == MULTI_SHOT_MODE_DEBLUR) {
                m_captureSelector->setFrameHoldCount(MAX_DEBLUR_CAPTURE_COUNT);
            }
#endif
#endif
        }
#endif

#if defined(SAMSUNG_DEBLUR) || defined(SAMSUNG_LLS_DEBLUR)
        if (getCameraId() == CAMERA_ID_FRONT && currentSeriesShotMode == SERIES_SHOT_MODE_NONE
            && isLDCapture(getCameraId()) && !m_getRecordingEnabled()) {
            m_exynosCameraParameters->checkLDCaptureMode();
            CLOGD("LDC(%s[%d]): currentSeriesShotMode(%d), getLDCaptureMode(%d)",
                    __FUNCTION__, __LINE__, currentSeriesShotMode, m_exynosCameraParameters->getLDCaptureMode());
        }
#endif

        if (m_exynosCameraParameters->getCaptureExposureTime() != 0) {
            m_longExposureRemainCount = m_exynosCameraParameters->getLongExposureShotCount();
        }

#ifdef SAMSUNG_DNG
        if (getCameraId() == CAMERA_ID_BACK
            && (currentSeriesShotMode == SERIES_SHOT_MODE_NONE || currentSeriesShotMode == SERIES_SHOT_MODE_LLS)
            && m_exynosCameraParameters->getDNGCaptureModeOn()
            && !m_getRecordingEnabled()) {
            int buffercount = 1;
            m_longExposureEnds = false;
            m_dngLongExposureRemainCount = 0;

            m_captureSelector->resetDNGFrameCount();
            m_exynosCameraParameters->setCheckMultiFrame(false);
            m_exynosCameraParameters->setUseDNGCapture(true);
            m_exynosCameraParameters->setDngSaveLocation(BURST_SAVE_PHONE);

            if (m_exynosCameraParameters->getOISCaptureModeOn() ||
                m_flashMgr->getNeedCaptureFlash() == true ||
                m_exynosCameraParameters->getCaptureExposureTime() > CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT) {
                m_exynosCameraParameters->setCheckMultiFrame(true);
                m_searchDNGFrame = true;
                buffercount = 3;
                m_flashMgr->resetShotFcount();
                if (m_exynosCameraParameters->getLongExposureShotCount() > 0) {
                    m_dngLongExposureRemainCount = m_exynosCameraParameters->getLongExposureShotCount();
                }
            }
            CLOGD("[DNG](%s[%d]): start set DNG Capture mode m_use_DNGCapture(%d) m_flagMultiFrame(%d)",
                __FUNCTION__, __LINE__, m_exynosCameraParameters->getUseDNGCapture(),
                m_exynosCameraParameters->getCheckMultiFrame());

            int pipeId = PIPE_FLITE;
            ExynosCameraBuffer newBuffer;
            int dstbufferIndex = 0;

            if (m_fliteBufferMgr->getNumOfAvailableBuffer() > 0) {
                m_previewFrameFactory->setRequestFLITE(true);

                for (int i = 0; i < buffercount; i++) {
                    ret = generateFrame(-1, &newFrame);
                    if (ret < 0) {
                        CLOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
                        return ret;
                    }

                    newBuffer.index = -2;
                    dstbufferIndex = -1;

                    m_fliteFrameCount++;
                    m_fliteBufferMgr->getBuffer(&dstbufferIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &newBuffer);

                    CLOGD("[DNG](%s[%d]):create DNG capture frame(%d) index(%d)",
                        __FUNCTION__, __LINE__, newFrame->getFrameCount(), newBuffer.index);

                    ret = m_setupEntity(pipeId, newFrame, NULL, &newBuffer);
                    if (ret < 0) {
                        CLOGE("ERR(%s[%d]):setupEntity fail", __FUNCTION__, __LINE__);
                        return ret;
                    }

                    m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId);
                    m_previewFrameFactory->pushFrameToPipe(&newFrame, pipeId);
                }

                m_fliteFrameCount = newFrame->getFrameCount() + 1;

                if (!m_exynosCameraParameters->getCheckMultiFrame()) {
                    m_previewFrameFactory->setRequestFLITE(false);
                }
            }

            m_previewFrameFactory->startThread(pipeId);
        }
#endif

#ifdef SAMSUNG_LBP
        if (currentSeriesShotMode == SERIES_SHOT_MODE_NONE) {
            uint32_t refBestPicCount = 0;
            if(getCameraId() == CAMERA_ID_FRONT) {
                if (m_exynosCameraParameters->getSCPFrameCount() >= m_exynosCameraParameters->getBayerFrameCount()) {
                    refBestPicCount = m_exynosCameraParameters->getSCPFrameCount() + LBP_CAPTURE_DELAY;
                } else {
                    refBestPicCount = m_exynosCameraParameters->getBayerFrameCount() + LBP_CAPTURE_DELAY;
                }
            }

            if(getCameraId() == CAMERA_ID_FRONT) {
                if(m_isLBPlux == true && m_getRecordingEnabled() == false) {
                    m_exynosCameraParameters->setNormalBestFrameCount(refBestPicCount);
                    m_captureSelector->setFrameIndex(refBestPicCount);

                    ret = m_captureSelector->setFrameHoldCount(m_exynosCameraParameters->getHoldFrameCount());
                    m_isLBPon = true;
                } else {
                    m_isLBPon = false;
                }
            } else {
#if !defined(SAMSUNG_DEBLUR) && !defined(SAMSUNG_LLS_DEBLUR)
                if(m_isLBPlux == true && m_exynosCameraParameters->getOISCaptureModeOn() == true
#ifdef SAMSUNG_DNG
                    && m_exynosCameraParameters->getDNGCaptureModeOn() == false
#endif
                    && m_getRecordingEnabled() == false) {
                    ret = m_captureSelector->setFrameHoldCount(m_exynosCameraParameters->getHoldFrameCount());
                    m_isLBPon = true;
                }
                else
#endif
                {
                    m_isLBPon = false;
                }
            }
        }
#endif
#ifdef SAMSUNG_JQ
        if (currentSeriesShotMode == SERIES_SHOT_MODE_NONE) {
            CLOGD("[JQ](%s[%d]): ON!!", __FUNCTION__, __LINE__);
            m_isJpegQtableOn = true;
            m_exynosCameraParameters->setJpegQtableOn(true);
        }
        else {
            CLOGD("[JQ](%s[%d]): OFF!!", __FUNCTION__, __LINE__);
            m_isJpegQtableOn = false;
            m_exynosCameraParameters->setJpegQtableOn(false);
        }
#endif
        if (m_hdrEnabled == true) {
            seriesShotCount = HDR_REPROCESSING_COUNT;
            m_sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_START);
            m_sCaptureMgr->resetHdrStartFcount();
            m_exynosCameraParameters->setFrameSkipCount(13);
        } else if ((m_flashMgr->getNeedCaptureFlash() == true && currentSeriesShotMode == SERIES_SHOT_MODE_NONE)
#ifdef FLASHED_LLS_CAPTURE
                || (m_flashMgr->getNeedCaptureFlash() == true && currentSeriesShotMode == SERIES_SHOT_MODE_LLS
#ifdef SR_CAPTURE
                    && m_exynosCameraParameters->getSROn() == false
#endif
                    )
#endif
        ) {
            if (m_exynosCameraParameters->getCaptureExposureTime() != 0) {
                m_stopLongExposure = false;
                m_flashMgr->setManualExposureTime(m_exynosCameraParameters->getLongExposureTime() * 1000);
            }

            m_exynosCameraParameters->setMarkingOfExifFlash(1);

#ifdef FLASHED_LLS_CAPTURE
            if (currentSeriesShotMode == SERIES_SHOT_MODE_LLS) {
                m_captureSelector->setFlashedLLSCaptureStatus(true);
#ifdef LLS_REPROCESSING
                m_sCaptureMgr->setIsFlashLLSCapture(true);
#endif
            }
#endif
            if (m_flashMgr->checkPreFlash() == false && m_isTryStopFlash == false) {
                m_flashMgr->setCaptureStatus(true);
                CLOGD("DEBUG(%s[%d]): checkPreFlash(false), Start auto focus internally", __FUNCTION__, __LINE__);
                m_autoFocusType = AUTO_FOCUS_HAL;
                m_flashMgr->setFlashTrigerPath(ExynosCameraActivityFlash::FLASH_TRIGGER_SHORT_BUTTON);
                m_flashMgr->setFlashWaitCancel(false);

                /* execute autoFocus for preFlash */
                m_autoFocusThread->requestExitAndWait();
                m_autoFocusThread->run(PRIORITY_DEFAULT);
            }
        }
#ifdef OIS_CAPTURE
        else if (m_exynosCameraParameters->getOISCaptureModeOn() == true
                && getCameraId() == CAMERA_ID_BACK) {
            CLOGD("DEBUG(%s[%d]): start set zsl-like mode", __FUNCTION__, __LINE__);

            int captureIntent;
#ifdef SAMSUNG_LBP
            m_sCaptureMgr->setBestMultiCaptureMode(false);
#endif

            if (m_exynosCameraParameters->getSeriesShotCount() > 0
#ifdef LLS_REPROCESSING
                && currentSeriesShotMode != SERIES_SHOT_MODE_LLS
#endif
            ) {
                /* BURST */
                m_sCaptureMgr->setMultiCaptureMode(true);
                captureIntent = AA_CAPTURE_INTENT_STILL_CAPTURE_OIS_MULTI;
            }
#ifdef SAMSUNG_LBP
            else if (currentSeriesShotMode == SERIES_SHOT_MODE_NONE && m_isLBPon == true
#ifdef LLS_REPROCESSING
                && currentSeriesShotMode != SERIES_SHOT_MODE_LLS)
#endif
            {
                /* BEST PICK */
                m_sCaptureMgr->setMultiCaptureMode(true);
                m_sCaptureMgr->setBestMultiCaptureMode(true);
                captureIntent = AA_CAPTURE_INTENT_STILL_CAPTURE_OIS_BEST;
            }
#endif
            else {
                /* SINGLE */
#ifdef SAMSUNG_DEBLUR
                if(m_exynosCameraParameters->getLDCaptureMode() == MULTI_SHOT_MODE_DEBLUR) {
                    captureIntent = AA_CAPTRUE_INTENT_STILL_CAPTURE_OIS_DEBLUR;
                    CLOGD("DEBUG(%s[%d]): start set Deblur mode captureIntent(%d)", __FUNCTION__, __LINE__,captureIntent);
                } else
#endif
                {
#ifdef SAMSUNG_LLS_DEBLUR
                    if (m_exynosCameraParameters->getLDCaptureMode() > 0
                        && m_exynosCameraParameters->getLDCaptureMode() != MULTI_SHOT_MODE_DEBLUR) {
                        unsigned int captureintent = AA_CAPTRUE_INTENT_STILL_CAPTURE_DYNAMIC_SHOT;
                        unsigned int capturecount = m_exynosCameraParameters->getLDCaptureCount();
                        unsigned int mask = 0;

                        mask = (((captureintent << 16) & 0xFFFF0000) | ((capturecount << 0) & 0x0000FFFF));
                        captureIntent = mask;
                        CLOGD("DEBUG(%s[%d]): start set multi mode captureIntent(%d)", __FUNCTION__, __LINE__,captureIntent);
                    } else
#endif
                    {
                        captureIntent = AA_CAPTURE_INTENT_STILL_CAPTURE_OIS_SINGLE;
                    }
                }
            }

            /* HACK: For the fast OIS-Capture Response time */
            ret = m_previewFrameFactory->setControl(V4L2_CID_IS_INTENT, captureIntent, PIPE_3AA);
            if (ret < 0)
                CLOGE("ERR(%s[%d]):setControl(STILL_CAPTURE_OIS) fail. ret(%d) intent(%d)",
                __FUNCTION__, __LINE__, ret, captureIntent);

            if(m_exynosCameraParameters->getSeriesShotCount() == 0) {
                m_OISCaptureShutterEnabled = true;
            }

            m_sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_START);
            m_exynosCameraActivityControl->setOISCaptureMode(true);
        }
#endif
        else {
            if (m_exynosCameraParameters->getCaptureExposureTime() != 0) {
                m_stopLongExposure = false;
                m_exynosCameraParameters->setExposureTime();
            }
        }
#ifdef SET_LLS_CAPTURE_SETFILE
        if(getCameraId() == CAMERA_ID_BACK && currentSeriesShotMode == SERIES_SHOT_MODE_LLS
#ifdef SR_CAPTURE
            && !m_exynosCameraParameters->getSROn()
#endif
#ifdef FLASHED_LLS_CAPTURE
            && !m_captureSelector->getFlashedLLSCaptureStatus()
#endif
        ) {
            CLOGD("DEBUG(%s[%d]): set LLS Capture mode on", __FUNCTION__, __LINE__);
            m_exynosCameraParameters->setLLSCaptureOn(true);
        }
#endif
#endif /* RAWDUMP_CAPTURE */
#ifdef SAMSUNG_LBP
        if(currentSeriesShotMode == SERIES_SHOT_MODE_NONE && m_isLBPon == true) {
            int TotalBufferNum = (int)m_exynosCameraParameters->getHoldFrameCount();
            ret = uni_plugin_set(m_LBPpluginHandle, BESTPHOTO_PLUGIN_NAME, UNI_PLUGIN_INDEX_TOTAL_BUFFER_NUM, &TotalBufferNum);
            if(ret < 0) {
                CLOGE("[LBP](%s[%d]):Bestphoto plugin set failed!!", __FUNCTION__, __LINE__);
            }
            ret = uni_plugin_init(m_LBPpluginHandle);
            if(ret < 0) {
                CLOGE("[LBP](%s[%d]):Bestphoto plugin init failed!!", __FUNCTION__, __LINE__);
            }

            m_LBPThread->run(PRIORITY_DEFAULT);
        }
#endif

#ifndef RAWDUMP_CAPTURE
        if (currentSeriesShotMode == SERIES_SHOT_MODE_NONE && m_flashMgr->getNeedCaptureFlash() == false
            && m_exynosCameraParameters->getCaptureExposureTime() == 0
#ifdef OIS_CAPTURE
            && m_exynosCameraParameters->getOISCaptureModeOn() == false
#endif
#ifdef SAMSUNG_LBP
            && !m_isLBPon
#endif
        ) {
            m_isZSLCaptureOn = true;
        }
#endif

#ifndef RAWDUMP_CAPTURE
        if (m_exynosCameraParameters->getSamsungCamera()
#if defined(SAMSUNG_DEBLUR) || defined(SAMSUNG_LLS_DEBLUR)
            && !m_exynosCameraParameters->getLDCaptureMode()
#endif
            ) {
            int thumbnailW = 0, thumbnailH = 0;
            m_exynosCameraParameters->getThumbnailSize(&thumbnailW, &thumbnailH);

            if ((thumbnailW > 0 && thumbnailH > 0)
                && !m_exynosCameraParameters->getRecordingHint()
                && !m_exynosCameraParameters->getDualMode()
                && currentSeriesShotMode != SERIES_SHOT_MODE_LLS
#ifdef SAMSUNG_MAGICSHOT
                && m_exynosCameraParameters->getShotMode() != SHOT_MODE_MAGIC
#endif
                && m_exynosCameraParameters->getShotMode() != SHOT_MODE_OUTFOCUS
                && m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_POSTVIEW_FRAME)) {
                m_exynosCameraParameters->setIsThumbnailCallbackOn(true);
                CLOGI("INFO(%s[%d]): m_isThumbnailCallbackOn(%d)", __FUNCTION__, __LINE__,
                    m_exynosCameraParameters->getIsThumbnailCallbackOn());
            }
        }
#endif

        m_exynosCameraParameters->setSetfileYuvRange();

#if defined(SAMSUNG_DEBLUR) || defined(SAMSUNG_LLS_DEBLUR)
        int pictureCount = m_exynosCameraParameters->getLDCaptureCount();

        if(m_exynosCameraParameters->getLDCaptureMode()) {
            m_reprocessingCounter.setCount(pictureCount);
        } else
#endif
        {
            m_reprocessingCounter.setCount(seriesShotCount);
        }

        if (m_prePictureThread->isRunning() == false) {
            if (m_prePictureThread->run(PRIORITY_DEFAULT) != NO_ERROR) {
                CLOGE("ERR(%s[%d]):couldn't run pre-picture thread", __FUNCTION__, __LINE__);
                return INVALID_OPERATION;
            }
        }

#if defined(SAMSUNG_DEBLUR) || defined(SAMSUNG_LLS_DEBLUR)
        if(m_exynosCameraParameters->getLDCaptureMode()) {
            m_jpegCounter.setCount(seriesShotCount);
            m_pictureCounter.setCount(pictureCount);
            CLOGD("LDC(%s[%d]): pictureCount(%d), getLDCaptureMode(%d)",
                    __FUNCTION__, __LINE__, pictureCount, m_exynosCameraParameters->getLDCaptureMode());
        } else
#endif
        {
            m_jpegCounter.setCount(seriesShotCount);
            m_pictureCounter.setCount(seriesShotCount);
        }

        if (m_exynosCameraParameters->getCaptureExposureTime() <= CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT) {
            if (m_pictureThread->isRunning() == false)
                ret = m_pictureThread->run();
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):couldn't run picture thread, ret(%d)", __FUNCTION__, __LINE__, ret);
                return INVALID_OPERATION;
            }
        }

        /* HDR, LLS, SIS should make YUV callback data. so don't use jpeg thread */
        if (!(m_hdrEnabled == true ||
                currentSeriesShotMode == SERIES_SHOT_MODE_LLS ||
                currentSeriesShotMode == SERIES_SHOT_MODE_SIS ||
                m_exynosCameraParameters->getShotMode() == SHOT_MODE_FRONT_PANORAMA)) {
            m_jpegCallbackThread->join();
            if (m_exynosCameraParameters->getCaptureExposureTime() <= CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT) {
                ret = m_jpegCallbackThread->run();
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):couldn't run jpeg callback thread, ret(%d)", __FUNCTION__, __LINE__, ret);
                    return INVALID_OPERATION;
                }
            }
        }
    } else {
        /* HDR, LLS, SIS should make YUV callback data. so don't use jpeg thread */
        /* One second burst capture launching jpegCallbackThread automatically */
        if (!(m_hdrEnabled == true ||
                currentSeriesShotMode == SERIES_SHOT_MODE_LLS ||
                currentSeriesShotMode == SERIES_SHOT_MODE_SIS ||
#ifdef ONE_SECOND_BURST_CAPTURE
                currentSeriesShotMode == SERIES_SHOT_MODE_ONE_SECOND_BURST ||
#endif
                m_exynosCameraParameters->getShotMode() == SHOT_MODE_FRONT_PANORAMA)) {
            /* series shot : push buffer to callback thread. */
            m_jpegCallbackThread->join();
            ret = m_jpegCallbackThread->run();
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):couldn't run jpeg callback thread, ret(%d)", __FUNCTION__, __LINE__, ret);
                return INVALID_OPERATION;
            }
        }
        CLOGI("INFO(%s[%d]): series shot takePicture, takePictureCount(%d)",
                __FUNCTION__, __LINE__, m_takePictureCounter.getCount());
        m_takePictureCounter.decCount();

        /* TODO : in case of no reprocesssing, we make zsl scheme or increse buf */
        if (m_exynosCameraParameters->isReprocessing() == false)
            m_pictureEnabled = true;
    }

    return NO_ERROR;
}

status_t ExynosCamera::cancelPicture()
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    if (m_exynosCameraParameters == NULL) {
        CLOGE("ERR(%s):m_exynosCameraParameters is NULL", __FUNCTION__);
        return NO_ERROR;
    }

    if (m_exynosCameraParameters->getVisionMode() == true) {
        CLOGW("WRN(%s[%d]): Vision mode does not support", __FUNCTION__, __LINE__);
        /* android_printAssert(NULL, LOG_TAG, "Cannot support this operation"); */

        return NO_ERROR;
    }

    if (m_exynosCameraParameters->getLongExposureShotCount() > 0) {
        CLOGD("DEBUG(%s[%d]):emergency stop for manual exposure shot", __FUNCTION__, __LINE__);
        m_cancelPicture = true;
        m_exynosCameraParameters->setPreviewExposureTime();
        m_pictureEnabled = false;
        m_stopLongExposure = true;
        m_reprocessingCounter.clearCount();
        m_clearJpegCallbackThread(false);
        m_cancelPicture = false;
    }

/*
    m_takePictureCounter.clearCount();
    m_reprocessingCounter.clearCount();
    m_pictureCounter.clearCount();
    m_jpegCounter.clearCount();
*/

    return NO_ERROR;
}

status_t ExynosCamera::setParameters(const CameraParameters& params)
{
    status_t ret = NO_ERROR;
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

#ifdef SCALABLE_ON
    m_exynosCameraParameters->setScalableSensorMode(true);
#else
    m_exynosCameraParameters->setScalableSensorMode(false);
#endif

    ret = m_exynosCameraParameters->setParameters(params);
#if 1
    /* HACK Reset Preview Flag*/
    if (m_exynosCameraParameters->getRestartPreview() == true && m_previewEnabled == true) {
        m_resetPreview = true;
#ifdef SUPPORT_SW_VDIS
        if(!m_swVDIS_Mode)
#endif /*SUPPORT_SW_VDIS*/
            ret = m_restartPreviewInternal();
        m_resetPreview = false;
        CLOGI("INFO(%s[%d]) m_resetPreview(%d)", __FUNCTION__, __LINE__, m_resetPreview);

        if (ret < 0)
            CLOGE("(%s[%d]): restart preview internal fail", __FUNCTION__, __LINE__);
    }
#endif

#ifdef CAMERA_FAST_ENTRANCE_V1
    if ((m_isFirstParametersSet == false) &&
      (m_exynosCameraParameters->getSamsungCamera() == true) &&
      (m_exynosCameraParameters->getDualMode() == false) &&
      (m_exynosCameraParameters->getVisionMode() == false)) {
        CLOGD("DEBUG(%s[%d]):1st setParameters", __FUNCTION__, __LINE__);
        m_fastEntrance = true;
        m_isFirstParametersSet = true;
        m_fastenAeThread->run();
    }
#endif

    return ret;

}

CameraParameters ExynosCamera::getParameters() const
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    return m_exynosCameraParameters->getParameters();
}

int ExynosCamera::getMaxNumDetectedFaces(void)
{
    return m_exynosCameraParameters->getMaxNumDetectedFaces();
}

bool ExynosCamera::m_startFaceDetection(bool toggle)
{
    CLOGD("DEBUG(%s[%d]) toggle : %d", __FUNCTION__, __LINE__, toggle);

    if (toggle == true) {
        m_exynosCameraParameters->setFdEnable(true);
        m_exynosCameraParameters->setFdMode(FACEDETECT_MODE_FULL);
    } else {
        m_exynosCameraParameters->setFdEnable(false);
        m_exynosCameraParameters->setFdMode(FACEDETECT_MODE_OFF);
    }

    memset(&m_frameMetadata, 0, sizeof(camera_frame_metadata_t));

    return true;
}

bool ExynosCamera::startFaceDetection(void)
{
    if (m_flagStartFaceDetection == true) {
        CLOGD("DEBUG(%s):Face detection already started..", __FUNCTION__);
        return true;
    }

    /* FD-AE is always on */
#ifdef USE_FD_AE
#else
    m_startFaceDetection(true);
#endif

    /* Block FD-AF except for special shot modes */
    if(m_exynosCameraParameters->getShotMode() == SHOT_MODE_BEAUTY_FACE ||
        m_exynosCameraParameters->getShotMode() == SHOT_MODE_SELFIE_ALARM) {
        ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();

        if (autoFocusMgr->setFaceDetection(true) == false) {
            CLOGE("ERR(%s[%d]):setFaceDetection(%d)", __FUNCTION__, __LINE__, true);
        } else {
            /* restart CAF when FD mode changed */
            switch (autoFocusMgr->getAutofocusMode()) {
            case ExynosCameraActivityAutofocus::AUTOFOCUS_MODE_CONTINUOUS_PICTURE:
            case ExynosCameraActivityAutofocus::AUTOFOCUS_MODE_CONTINUOUS_PICTURE_MACRO:
                if (autoFocusMgr->flagAutofocusStart() == true &&
                    autoFocusMgr->flagLockAutofocus() == false) {
                    autoFocusMgr->stopAutofocus();
                    autoFocusMgr->startAutofocus();
                }
                break;
            default:
                break;
            }
        }
    }

    if (m_facedetectQ->getSizeOfProcessQ() > 0) {
        CLOGE("ERR(%s[%d]):startFaceDetection recordingQ(%d)", __FUNCTION__, __LINE__, m_facedetectQ->getSizeOfProcessQ());
        /*
         * just empty q on m_facedetectQ.
         * m_clearList() can make deadlock by accessing frame
         * deleted on stopPreview()
         */
        /* m_clearList(m_facedetectQ); */
        m_facedetectQ->release();
    }

    m_flagStartFaceDetection = true;

    if (m_facedetectThread->isRunning() == false)
        m_facedetectThread->run();

    return true;
}

bool ExynosCamera::stopFaceDetection(void)
{
    if (m_flagStartFaceDetection == false) {
        CLOGD("DEBUG(%s [%d]):Face detection already stopped..", __FUNCTION__, __LINE__);
        return true;
    }

    ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();

    if (autoFocusMgr->setFaceDetection(false) == false) {
        CLOGE("ERR(%s[%d]):setFaceDetection(%d)", __FUNCTION__, __LINE__, false);
    } else {
        /* restart CAF when FD mode changed */
        switch (autoFocusMgr->getAutofocusMode()) {
        case ExynosCameraActivityAutofocus::AUTOFOCUS_MODE_CONTINUOUS_PICTURE:
        case ExynosCameraActivityAutofocus::AUTOFOCUS_MODE_CONTINUOUS_PICTURE_MACRO:
            if (autoFocusMgr->flagAutofocusStart() == true &&
                autoFocusMgr->flagLockAutofocus() == false) {
                autoFocusMgr->stopAutofocus();
                autoFocusMgr->startAutofocus();
            }
            break;
        default:
            break;
        }
    }

    /* FD-AE is always on */
#ifdef USE_FD_AE
#else
    m_startFaceDetection(false);
#endif
    m_flagStartFaceDetection = false;

    return true;
}

#ifdef SAMSUNG_DOF
bool ExynosCamera::startCurrentSet(void)
{
    m_currentSetStart = true;

    return true;
}

bool ExynosCamera::stopCurrentSet(void)
{
    m_currentSetStart = false;

    return true;
}
#endif


bool ExynosCamera::m_getRecordingEnabled(void)
{
    Mutex::Autolock lock(m_recordingStateLock);
    return m_recordingEnabled;
}

void ExynosCamera::m_setRecordingEnabled(bool enable)
{
    Mutex::Autolock lock(m_recordingStateLock);
    m_recordingEnabled = enable;
    return;
}

int ExynosCamera::m_calibratePosition(int w, int new_w, int pos)
{
    return (float)(pos * new_w) / (float)w;
}

#ifdef SAMSUNG_DOF
void ExynosCamera::m_clearDOFmeta(void)
{
    if(m_frameMetadata.dof_data != NULL) {
        delete []m_frameMetadata.dof_data;
        m_frameMetadata.dof_data = NULL;
    }
    m_frameMetadata.dof_column = 0;
    m_frameMetadata.dof_row = 0;

    memset(&m_frameMetadata.current_set_data, 0, sizeof(camera_current_set_t));
    memset(&m_frameMetadata.single_paf_data, 0, sizeof(st_AF_PafWinResult));

    m_flagMetaDataSet = false;
}
#endif

#ifdef SAMSUNG_OT
void ExynosCamera::m_clearOTmeta(void)
{
    memset(&m_frameMetadata.object_data, 0, sizeof(camera_object_t));
}
#endif

bool ExynosCamera::m_facedetectThreadFunc(void)
{
    int32_t status = 0;
    bool ret = true;

    int index = 0;
    int count = 0;

    ExynosCameraFrame *newFrame = NULL;
    uint32_t frameCnt = 0;

    if (m_previewEnabled == false) {
        CLOGD("DEBUG(%s):preview is stopped, thread stop", __FUNCTION__);
        ret = false;
        goto func_exit;
    }

    status = m_facedetectQ->waitAndPopProcessQ(&newFrame);
    if (m_flagThreadStop == true) {
        CLOGI("INFO(%s[%d]):m_flagThreadStop(%d) m_flagStartFaceDetection(%d)", __FUNCTION__, __LINE__, m_flagThreadStop, m_flagStartFaceDetection);
        ret = false;
        goto func_exit;
    }

    if (status < 0) {
        if (status == TIMED_OUT) {
            /* Face Detection time out is not meaningful */
        } else {
            /* TODO: doing exception handling */
            CLOGE("ERR(%s):wait and pop fail, ret(%d)", __FUNCTION__, ret);
        }
        goto func_exit;
    }

    count = m_facedetectQ->getSizeOfProcessQ();
    if (count >= MAX_FACEDETECT_THREADQ_SIZE) {
        if (newFrame != NULL) {
            CLOGE("ERR(%s[%d]):m_facedetectQ  skipped QSize(%d) frame(%d)", __FUNCTION__, __LINE__,  count, newFrame->getFrameCount());
            newFrame->decRef();
            m_frameMgr->deleteFrame(newFrame);
            newFrame = NULL;
        }
        for (int i = 0 ; i < count-1 ; i++) {
            m_facedetectQ->popProcessQ(&newFrame);
            if (newFrame != NULL) {
                newFrame->decRef();
                m_frameMgr->deleteFrame(newFrame);
                newFrame = NULL;
            }
        }
        m_facedetectQ->popProcessQ(&newFrame);
    }

    if (newFrame != NULL) {
        status = m_doFdCallbackFunc(newFrame);
        if (status < 0) {
            CLOGE("ERR(%s[%d]) m_doFdCallbackFunc failed(%d).", __FUNCTION__, __LINE__, status);
        }
    }

    if (m_facedetectQ->getSizeOfProcessQ() > 0) {
        ret = true;
    }

    if (newFrame != NULL) {
        newFrame->decRef();
        m_frameMgr->deleteFrame(newFrame);
        newFrame = NULL;
    }

func_exit:

    return ret;
}

#ifdef SR_CAPTURE
status_t ExynosCamera::m_doSRCallbackFunc(ExynosCameraFrame *frame)
{
    ExynosCameraDurationTimer       m_srcallbackTimer;
    long long                       m_srcallbackTimerTime;
    int pictureW, pictureH;

    CLOGI("INFO(%s[%d]) -IN-", __FUNCTION__, __LINE__);

    int id[NUM_OF_DETECTED_FACES];
    int score[NUM_OF_DETECTED_FACES];
    ExynosRect2 detectedFace[NUM_OF_DETECTED_FACES];
    ExynosRect2 detectedLeftEye[NUM_OF_DETECTED_FACES];
    ExynosRect2 detectedRightEye[NUM_OF_DETECTED_FACES];
    ExynosRect2 detectedMouth[NUM_OF_DETECTED_FACES];
    int numOfDetectedFaces = 0;
    int num = 0;
    int previewW, previewH;

    memset(&id, 0x00, sizeof(int) * NUM_OF_DETECTED_FACES);
    memset(&score, 0x00, sizeof(int) * NUM_OF_DETECTED_FACES);

    m_exynosCameraParameters->getHwPreviewSize(&previewW, &previewH);

    int waitCount = 5;
    while (m_isCopySrMdeta && 0 < waitCount) {
        usleep(10000);
        waitCount--;
    }

    CLOGD("DEBUG(%s[%d]) faceDetectMode(%d)", __FUNCTION__, __LINE__, m_srShotMeta.stats.faceDetectMode);
    CLOGV("[%d %d]", m_srShotMeta.stats.faceRectangles[0][0], m_srShotMeta.stats.faceRectangles[0][1]);
    CLOGV("[%d %d]", m_srShotMeta.stats.faceRectangles[0][2], m_srShotMeta.stats.faceRectangles[0][3]);

    num = NUM_OF_DETECTED_FACES;
    if (getMaxNumDetectedFaces() < num)
        num = getMaxNumDetectedFaces();

    switch (m_srShotMeta.stats.faceDetectMode) {
    case FACEDETECT_MODE_SIMPLE:
    case FACEDETECT_MODE_FULL:
        break;
    case FACEDETECT_MODE_OFF:
    default:
        num = 0;
        break;
    }

    for (int i = 0; i < num; i++) {
        if (m_srShotMeta.stats.faceIds[i]) {
            switch (m_srShotMeta.stats.faceDetectMode) {
            case FACEDETECT_MODE_OFF:
                break;
            case FACEDETECT_MODE_SIMPLE:
                detectedFace[i].x1 = m_srShotMeta.stats.faceRectangles[i][0];
                detectedFace[i].y1 = m_srShotMeta.stats.faceRectangles[i][1];
                detectedFace[i].x2 = m_srShotMeta.stats.faceRectangles[i][2];
                detectedFace[i].y2 = m_srShotMeta.stats.faceRectangles[i][3];
                numOfDetectedFaces++;
                break;
            case FACEDETECT_MODE_FULL:
                id[i] = m_srShotMeta.stats.faceIds[i];
                score[i] = m_srShotMeta.stats.faceScores[i];

                detectedFace[i].x1 = m_srShotMeta.stats.faceRectangles[i][0];
                detectedFace[i].y1 = m_srShotMeta.stats.faceRectangles[i][1];
                detectedFace[i].x2 = m_srShotMeta.stats.faceRectangles[i][2];
                detectedFace[i].y2 = m_srShotMeta.stats.faceRectangles[i][3];

                detectedLeftEye[i].x1
                    = detectedLeftEye[i].y1
                    = detectedLeftEye[i].x2
                    = detectedLeftEye[i].y2 = -1;

                detectedRightEye[i].x1
                    = detectedRightEye[i].y1
                    = detectedRightEye[i].x2
                    = detectedRightEye[i].y2 = -1;

                detectedMouth[i].x1
                    = detectedMouth[i].y1
                    = detectedMouth[i].x2
                    = detectedMouth[i].y2 = -1;

                numOfDetectedFaces++;
                break;
            default:
                break;
            }
        }
    }

    if (0 < numOfDetectedFaces) {
        /*
         * camera.h
         * width   : -1000~1000
         * height  : -1000~1000
         * if eye, mouth is not detectable : -2000, -2000.
         */
        memset(m_faces_sr, 0, sizeof(camera_face_t) * NUM_OF_DETECTED_FACES);

        int realNumOfDetectedFaces = 0;

        for (int i = 0; i < numOfDetectedFaces; i++) {
            /*
             * over 50s, we will catch
             * if (score[i] < 50)
             *    continue;
            */
            m_faces_sr[realNumOfDetectedFaces].rect[0] = m_calibratePosition(previewW, 2000, detectedFace[i].x1) - 1000;
            m_faces_sr[realNumOfDetectedFaces].rect[1] = m_calibratePosition(previewH, 2000, detectedFace[i].y1) - 1000;
            m_faces_sr[realNumOfDetectedFaces].rect[2] = m_calibratePosition(previewW, 2000, detectedFace[i].x2) - 1000;
            m_faces_sr[realNumOfDetectedFaces].rect[3] = m_calibratePosition(previewH, 2000, detectedFace[i].y2) - 1000;

            m_faces_sr[realNumOfDetectedFaces].id = id[i];
            m_faces_sr[realNumOfDetectedFaces].score = score[i] > 100 ? 100 : score[i];

            m_faces_sr[realNumOfDetectedFaces].left_eye[0] = (detectedLeftEye[i].x1 < 0) ? -2000 : m_calibratePosition(previewW, 2000, detectedLeftEye[i].x1) - 1000;
            m_faces_sr[realNumOfDetectedFaces].left_eye[1] = (detectedLeftEye[i].y1 < 0) ? -2000 : m_calibratePosition(previewH, 2000, detectedLeftEye[i].y1) - 1000;

            m_faces_sr[realNumOfDetectedFaces].right_eye[0] = (detectedRightEye[i].x1 < 0) ? -2000 : m_calibratePosition(previewW, 2000, detectedRightEye[i].x1) - 1000;
            m_faces_sr[realNumOfDetectedFaces].right_eye[1] = (detectedRightEye[i].y1 < 0) ? -2000 : m_calibratePosition(previewH, 2000, detectedRightEye[i].y1) - 1000;

            m_faces_sr[realNumOfDetectedFaces].mouth[0] = (detectedMouth[i].x1 < 0) ? -2000 : m_calibratePosition(previewW, 2000, detectedMouth[i].x1) - 1000;
            m_faces_sr[realNumOfDetectedFaces].mouth[1] = (detectedMouth[i].y1 < 0) ? -2000 : m_calibratePosition(previewH, 2000, detectedMouth[i].y1) - 1000;

            CLOGV("face posision(cal:%d,%d %dx%d)(org:%d,%d %dx%d), id(%d), score(%d)",
                m_faces_sr[realNumOfDetectedFaces].rect[0], m_faces_sr[realNumOfDetectedFaces].rect[1],
                m_faces_sr[realNumOfDetectedFaces].rect[2], m_faces_sr[realNumOfDetectedFaces].rect[3],
                detectedFace[i].x1, detectedFace[i].y1,
                detectedFace[i].x2, detectedFace[i].y2,
                m_faces_sr[realNumOfDetectedFaces].id,
                m_faces_sr[realNumOfDetectedFaces].score);

            CLOGV("DEBUG(%s[%d]): left eye(%d,%d), right eye(%d,%d), mouth(%dx%d), num of facese(%d)",
                    __FUNCTION__, __LINE__,
                    m_faces_sr[realNumOfDetectedFaces].left_eye[0],
                    m_faces_sr[realNumOfDetectedFaces].left_eye[1],
                    m_faces_sr[realNumOfDetectedFaces].right_eye[0],
                    m_faces_sr[realNumOfDetectedFaces].right_eye[1],
                    m_faces_sr[realNumOfDetectedFaces].mouth[0],
                    m_faces_sr[realNumOfDetectedFaces].mouth[1],
                    realNumOfDetectedFaces
                 );

            realNumOfDetectedFaces++;
        }

        m_sr_frameMetadata.number_of_faces = realNumOfDetectedFaces;
        m_sr_frameMetadata.faces = m_faces_sr;

        m_faceDetected = true;
        m_fdThreshold = 0;
    } else {
        if (m_faceDetected == true && m_fdThreshold < NUM_OF_DETECTED_FACES_THRESHOLD) {
            /* waiting the unexpected fail about face detected */
            m_fdThreshold++;
        } else {
            if (0 < m_sr_frameMetadata.number_of_faces)
                memset(m_faces_sr, 0, sizeof(camera_face_t) * NUM_OF_DETECTED_FACES);

            m_sr_frameMetadata.number_of_faces = 0;
            m_sr_frameMetadata.faces = m_faces_sr;
            m_fdThreshold = 0;
            m_faceDetected = false;
        }
    }

    m_exynosCameraParameters->getPictureSize(&pictureW, &pictureH);

    m_sr_frameMetadata.crop_info.org_width = pictureW;
    m_sr_frameMetadata.crop_info.org_height = pictureH;
    m_sr_frameMetadata.crop_info.cropped_width = m_sr_cropped_width;
    m_sr_frameMetadata.crop_info.cropped_height = m_sr_cropped_height;

    CLOGI("INFO(%s[%d]):sieze (%d,%d,%d,%d)", __FUNCTION__, __LINE__
     , pictureW, pictureH, m_sr_cropped_width, m_sr_cropped_height);

    m_srcallbackTimer.start();

    if (m_exynosCameraParameters->msgTypeEnabled(MULTI_FRAME_SHOT_SR_EXTRA_INFO))
    {
        setBit(&m_callbackState, MULTI_FRAME_SHOT_SR_EXTRA_INFO, false);
        m_dataCb(MULTI_FRAME_SHOT_SR_EXTRA_INFO, m_srCallbackHeap, 0, &m_sr_frameMetadata, m_callbackCookie);
        clearBit(&m_callbackState, MULTI_FRAME_SHOT_SR_EXTRA_INFO, false);
    }
    m_srcallbackTimer.stop();
    m_srcallbackTimerTime = m_srcallbackTimer.durationUsecs();

    if((int)m_srcallbackTimerTime / 1000 > 50) {
        CLOGD("INFO(%s[%d]): SR callback duration time : (%d)mec", __FUNCTION__, __LINE__, (int)m_srcallbackTimerTime / 1000);
    }

    return NO_ERROR;
}
#endif

status_t ExynosCamera::m_doFdCallbackFunc(ExynosCameraFrame *frame)
{
#ifdef SAMSUNG_DOF
    ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
#endif
#ifdef LLS_CAPTURE
    int LLS_value = 0;
#endif
    ExynosCameraDurationTimer       m_fdcallbackTimer;
    long long                       m_fdcallbackTimerTime;

    if (m_fdmeta_shot == NULL) {
        CLOGE("ERR(%s[%d]) meta_shot_ext is null", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    memset(m_fdmeta_shot, 0x00, sizeof(struct camera2_shot_ext));
    if (frame->getDynamicMeta(m_fdmeta_shot) != NO_ERROR) {
        CLOGE("ERR(%s[%d]) meta_shot_ext is null", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    frame->getUserDynamicMeta(m_fdmeta_shot);

    if (m_flagStartFaceDetection == true) {
        int id[NUM_OF_DETECTED_FACES];
        int score[NUM_OF_DETECTED_FACES];
        ExynosRect2 detectedFace[NUM_OF_DETECTED_FACES];
        ExynosRect2 detectedLeftEye[NUM_OF_DETECTED_FACES];
        ExynosRect2 detectedRightEye[NUM_OF_DETECTED_FACES];
        ExynosRect2 detectedMouth[NUM_OF_DETECTED_FACES];
        int numOfDetectedFaces = 0;
        int num = 0;
        struct camera2_dm *dm = NULL;
        int previewW, previewH;

        memset(&id, 0x00, sizeof(int) * NUM_OF_DETECTED_FACES);
        memset(&score, 0x00, sizeof(int) * NUM_OF_DETECTED_FACES);

        m_exynosCameraParameters->getHwPreviewSize(&previewW, &previewH);
#ifdef SUPPORT_SW_VDIS
        if(m_exynosCameraParameters->isSWVdisMode()) {
            m_swVDIS_AdjustPreviewSize(&previewW, &previewH);
        }
#endif /*SUPPORT_SW_VDIS*/

        dm = &(m_fdmeta_shot->shot.dm);
        if (dm == NULL) {
            CLOGE("ERR(%s[%d]) dm data is null", __FUNCTION__, __LINE__);
            return INVALID_OPERATION;
        }

        CLOGV("DEBUG(%s[%d]) faceDetectMode(%d)", __FUNCTION__, __LINE__, dm->stats.faceDetectMode);
        CLOGV("[%d %d]", dm->stats.faceRectangles[0][0], dm->stats.faceRectangles[0][1]);
        CLOGV("[%d %d]", dm->stats.faceRectangles[0][2], dm->stats.faceRectangles[0][3]);

       num = NUM_OF_DETECTED_FACES;
        if (getMaxNumDetectedFaces() < num)
            num = getMaxNumDetectedFaces();

        switch (dm->stats.faceDetectMode) {
        case FACEDETECT_MODE_SIMPLE:
        case FACEDETECT_MODE_FULL:
            break;
        case FACEDETECT_MODE_OFF:
        default:
            num = 0;
            break;
        }

        for (int i = 0; i < num; i++) {
            if (dm->stats.faceIds[i]) {
                switch (dm->stats.faceDetectMode) {
                case FACEDETECT_MODE_OFF:
                    break;
                case FACEDETECT_MODE_SIMPLE:
                    detectedFace[i].x1 = dm->stats.faceRectangles[i][0];
                    detectedFace[i].y1 = dm->stats.faceRectangles[i][1];
                    detectedFace[i].x2 = dm->stats.faceRectangles[i][2];
                    detectedFace[i].y2 = dm->stats.faceRectangles[i][3];
                    numOfDetectedFaces++;
                    break;
                case FACEDETECT_MODE_FULL:
                    id[i] = dm->stats.faceIds[i];
                    score[i] = dm->stats.faceScores[i];

                    detectedFace[i].x1 = dm->stats.faceRectangles[i][0];
                    detectedFace[i].y1 = dm->stats.faceRectangles[i][1];
                    detectedFace[i].x2 = dm->stats.faceRectangles[i][2];
                    detectedFace[i].y2 = dm->stats.faceRectangles[i][3];

                    detectedLeftEye[i].x1
                        = detectedLeftEye[i].y1
                        = detectedLeftEye[i].x2
                        = detectedLeftEye[i].y2 = -1;

                    detectedRightEye[i].x1
                        = detectedRightEye[i].y1
                        = detectedRightEye[i].x2
                        = detectedRightEye[i].y2 = -1;

                    detectedMouth[i].x1
                        = detectedMouth[i].y1
                        = detectedMouth[i].x2
                        = detectedMouth[i].y2 = -1;

                    numOfDetectedFaces++;
                    break;
                default:
                    break;
                }
            }
        }

        if (0 < numOfDetectedFaces) {
            /*
             * camera.h
             * width   : -1000~1000
             * height  : -1000~1000
             * if eye, mouth is not detectable : -2000, -2000.
             */
            memset(m_faces, 0, sizeof(camera_face_t) * NUM_OF_DETECTED_FACES);

            int realNumOfDetectedFaces = 0;

            for (int i = 0; i < numOfDetectedFaces; i++) {
                /*
                 * over 50s, we will catch
                 * if (score[i] < 50)
                 *    continue;
                */
                m_faces[realNumOfDetectedFaces].rect[0] = m_calibratePosition(previewW, 2000, detectedFace[i].x1) - 1000;
                m_faces[realNumOfDetectedFaces].rect[1] = m_calibratePosition(previewH, 2000, detectedFace[i].y1) - 1000;
                m_faces[realNumOfDetectedFaces].rect[2] = m_calibratePosition(previewW, 2000, detectedFace[i].x2) - 1000;
                m_faces[realNumOfDetectedFaces].rect[3] = m_calibratePosition(previewH, 2000, detectedFace[i].y2) - 1000;

                m_faces[realNumOfDetectedFaces].id = id[i];
                m_faces[realNumOfDetectedFaces].score = score[i] > 100 ? 100 : score[i];

                m_faces[realNumOfDetectedFaces].left_eye[0] = (detectedLeftEye[i].x1 < 0) ? -2000 : m_calibratePosition(previewW, 2000, detectedLeftEye[i].x1) - 1000;
                m_faces[realNumOfDetectedFaces].left_eye[1] = (detectedLeftEye[i].y1 < 0) ? -2000 : m_calibratePosition(previewH, 2000, detectedLeftEye[i].y1) - 1000;

                m_faces[realNumOfDetectedFaces].right_eye[0] = (detectedRightEye[i].x1 < 0) ? -2000 : m_calibratePosition(previewW, 2000, detectedRightEye[i].x1) - 1000;
                m_faces[realNumOfDetectedFaces].right_eye[1] = (detectedRightEye[i].y1 < 0) ? -2000 : m_calibratePosition(previewH, 2000, detectedRightEye[i].y1) - 1000;

                m_faces[realNumOfDetectedFaces].mouth[0] = (detectedMouth[i].x1 < 0) ? -2000 : m_calibratePosition(previewW, 2000, detectedMouth[i].x1) - 1000;
                m_faces[realNumOfDetectedFaces].mouth[1] = (detectedMouth[i].y1 < 0) ? -2000 : m_calibratePosition(previewH, 2000, detectedMouth[i].y1) - 1000;

                CLOGV("face posision(cal:%d,%d %dx%d)(org:%d,%d %dx%d), id(%d), score(%d)",
                    m_faces[realNumOfDetectedFaces].rect[0], m_faces[realNumOfDetectedFaces].rect[1],
                    m_faces[realNumOfDetectedFaces].rect[2], m_faces[realNumOfDetectedFaces].rect[3],
                    detectedFace[i].x1, detectedFace[i].y1,
                    detectedFace[i].x2, detectedFace[i].y2,
                    m_faces[realNumOfDetectedFaces].id,
                    m_faces[realNumOfDetectedFaces].score);

                CLOGV("DEBUG(%s[%d]): left eye(%d,%d), right eye(%d,%d), mouth(%dx%d), num of facese(%d)",
                        __FUNCTION__, __LINE__,
                        m_faces[realNumOfDetectedFaces].left_eye[0],
                        m_faces[realNumOfDetectedFaces].left_eye[1],
                        m_faces[realNumOfDetectedFaces].right_eye[0],
                        m_faces[realNumOfDetectedFaces].right_eye[1],
                        m_faces[realNumOfDetectedFaces].mouth[0],
                        m_faces[realNumOfDetectedFaces].mouth[1],
                        realNumOfDetectedFaces
                     );

                realNumOfDetectedFaces++;
            }

            m_frameMetadata.number_of_faces = realNumOfDetectedFaces;
            m_frameMetadata.faces = m_faces;

            m_faceDetected = true;
            m_fdThreshold = 0;
        } else {
            if (m_faceDetected == true && m_fdThreshold < NUM_OF_DETECTED_FACES_THRESHOLD) {
                /* waiting the unexpected fail about face detected */
                m_fdThreshold++;
            } else {
                if (0 < m_frameMetadata.number_of_faces)
                    memset(m_faces, 0, sizeof(camera_face_t) * NUM_OF_DETECTED_FACES);

                m_frameMetadata.number_of_faces = 0;
                m_frameMetadata.faces = m_faces;
                m_fdThreshold = 0;
                m_faceDetected = false;
            }
        }
    } else {
        if (0 < m_frameMetadata.number_of_faces)
            memset(m_faces, 0, sizeof(camera_face_t) * NUM_OF_DETECTED_FACES);

        m_frameMetadata.number_of_faces = 0;
        m_frameMetadata.faces = m_faces;

        m_fdThreshold = 0;

        m_faceDetected = false;
    }

#ifdef LLS_CAPTURE
    m_needLLS_history[3] = m_needLLS_history[2];
    m_needLLS_history[2] = m_needLLS_history[1];
    m_needLLS_history[1] = m_exynosCameraParameters->getLLS(m_fdmeta_shot);

    if ((m_needLLS_history[1] == m_needLLS_history[2])&&
        (m_needLLS_history[1] == m_needLLS_history[3])) {
        LLS_value = m_needLLS_history[0] = m_needLLS_history[1];
    } else {
        LLS_value = m_needLLS_history[0];
    }

    m_exynosCameraParameters->setLLSValue(LLS_value);

    if (m_exynosCameraParameters->getShotMode() == SHOT_MODE_PRO_MODE) {
        LLS_value = LLS_LEVEL_ZSL_LIKE; /* disable SR Mode */
    } else if (LLS_value >= LLS_LEVEL_MULTI_MERGE_INDICATOR_2 && LLS_value <= LLS_LEVEL_MULTI_MERGE_INDICATOR_4) {
        LLS_value = LLS_LEVEL_LLS_INDICATOR_ONLY;
    }

    m_frameMetadata.needLLS = LLS_value;
    m_frameMetadata.light_condition = LLS_value;

    CLOGV("[%d[%d][%d][%d] LLS_value(%d) light_condition(%d)",
        m_needLLS_history[0], m_needLLS_history[1], m_needLLS_history[2], m_needLLS_history[3],
        m_exynosCameraParameters->getLLSValue(), m_frameMetadata.light_condition);
#endif

#ifdef SAMSUNG_LBP
    uint32_t shutterSpeed = 0;

    if(m_fdmeta_shot->shot.udm.ae.vendorSpecific[0] == 0xAEAEAEAE)
        shutterSpeed = m_fdmeta_shot->shot.udm.ae.vendorSpecific[64];
    else
        shutterSpeed = m_fdmeta_shot->shot.udm.internal.vendorSpecific2[100];

    CLOGV("[LBP]: ShutterSpeed:%d", shutterSpeed);
    if(shutterSpeed <= 24)
        m_isLBPlux = true;
    else
        m_isLBPlux = false;
#endif

#ifdef TOUCH_AE
    if(((m_exynosCameraParameters->getMeteringMode() >= METERING_MODE_CENTER_TOUCH)
        && (m_exynosCameraParameters->getMeteringMode() <= METERING_MODE_AVERAGE_TOUCH))
        && (m_fdmeta_shot->shot.udm.ae.vendorSpecific[395] == 1) /* touch ae State flag  - 0:search 1:done */
        && (m_fdmeta_shot->shot.udm.ae.vendorSpecific[398] == 1) /* touch ae enable flag  - 0:disable 1:enable */) {
        m_notifyCb(AE_RESULT, 0, 0, m_callbackCookie);
        CLOGV("INFO(%s[%d]): AE_RESULT(%d)", __FUNCTION__, __LINE__, m_fdmeta_shot->shot.dm.aa.aeState);
    }

    m_frameMetadata.ae_bv_changed = m_fdmeta_shot->shot.udm.ae.vendorSpecific[396];
#endif

#ifdef SAMSUNG_DOF
    if(autoFocusMgr->getCurrentState() == ExynosCameraActivityAutofocus::AUTOFOCUS_STATE_SCANNING ||
            m_currentSetStart == true) {
        /* Single Window */
        m_frameMetadata.current_set_data.lens_position_min = (uint16_t)m_fdmeta_shot->shot.udm.af.lensPositionMacro;
        m_frameMetadata.current_set_data.lens_position_max = (uint16_t)m_fdmeta_shot->shot.udm.af.lensPositionInfinity;
        m_frameMetadata.current_set_data.lens_position_current = (uint16_t)m_fdmeta_shot->shot.udm.af.lensPositionCurrent;
        m_frameMetadata.current_set_data.driver_resolution = m_fdmeta_shot->shot.udm.companion.pdaf.lensPosResolution;
        CLOGV("current_set_data.exposure_time = %d", m_frameMetadata.current_set_data.exposure_time);
        CLOGV("current_set_data.iso = %d", m_frameMetadata.current_set_data.iso);
        CLOGV("current_set_data.lens_position_min = %d", m_frameMetadata.current_set_data.lens_position_min);
        CLOGV("current_set_data.lens_position_max = %d", m_frameMetadata.current_set_data.lens_position_max);
        CLOGV("current_set_data.lens_position_current = %d", m_frameMetadata.current_set_data.lens_position_current);
        CLOGV("current_set_data.driver_resolution = %d", m_frameMetadata.current_set_data.driver_resolution);

        m_flagMetaDataSet = true;
    }

    if(autoFocusMgr->getCurrentState() == ExynosCameraActivityAutofocus::AUTOFOCUS_STATE_SCANNING) {
        m_frameMetadata.single_paf_data.mode = m_fdmeta_shot->shot.udm.companion.pdaf.singleResult.mode;
        m_frameMetadata.single_paf_data.goal_pos = m_fdmeta_shot->shot.udm.companion.pdaf.singleResult.goalPos;
        m_frameMetadata.single_paf_data.reliability = m_fdmeta_shot->shot.udm.companion.pdaf.singleResult.reliability;
        CLOGV("single_paf_data.mode = %d", m_fdmeta_shot->shot.udm.companion.pdaf.singleResult.mode);
        CLOGV("single_paf_data.goal_pos = %d", m_fdmeta_shot->shot.udm.companion.pdaf.singleResult.goalPos);
        CLOGV("single_paf_data.reliability = %d", m_fdmeta_shot->shot.udm.companion.pdaf.singleResult.reliability);
    }

    if (m_exynosCameraParameters->getShotMode() == SHOT_MODE_OUTFOCUS) {
        if (m_fdmeta_shot->shot.udm.companion.pdaf.numCol > 0 &&
                    m_fdmeta_shot->shot.udm.companion.pdaf.numRow > 0) {
            if((autoFocusMgr->getCurrentState() == ExynosCameraActivityAutofocus::AUTOFOCUS_STATE_SUCCEESS ||
                    autoFocusMgr->getCurrentState() == ExynosCameraActivityAutofocus::AUTOFOCUS_STATE_FAIL) &&
                        m_AFstatus == ExynosCameraActivityAutofocus::AUTOFOCUS_STATE_SCANNING) {
                /* Multi window */
                m_frameMetadata.dof_column = m_fdmeta_shot->shot.udm.companion.pdaf.numCol;
                m_frameMetadata.dof_row = m_fdmeta_shot->shot.udm.companion.pdaf.numRow;

                if(m_frameMetadata.dof_data == NULL)
                     m_frameMetadata.dof_data = new st_AF_PafWinResult[m_frameMetadata.dof_column * m_frameMetadata.dof_row];
                for(int i = 0 ; i < m_frameMetadata.dof_column ; i++) {
                    memcpy(m_frameMetadata.dof_data + (i * m_frameMetadata.dof_row), m_fdmeta_shot->shot.udm.companion.pdaf.multiResult[i],
                            sizeof(camera2_pdaf_multi_result) * m_frameMetadata.dof_row);
                }

                for(int i = 0; i < m_fdmeta_shot->shot.udm.companion.pdaf.numCol; i++){
                    for(int j = 0; j < m_fdmeta_shot->shot.udm.companion.pdaf.numRow; j++){
                        CLOGD("dof_data[%d][%d] mode = %d", i, j, (m_frameMetadata.dof_data+(i*m_frameMetadata.dof_row +j))->mode);
                        CLOGD("dof_data[%d][%d] goalPos = %d", i, j, (m_frameMetadata.dof_data+(i*m_frameMetadata.dof_row +j))->goal_pos);
                        CLOGD("dof_data[%d][%d] reliability = %d", i, j, (m_frameMetadata.dof_data+(i*m_frameMetadata.dof_row +j))->reliability);
                    }
                }

                if (m_fdmeta_shot->shot.udm.ae.vendorSpecific[0] == 0xAEAEAEAE)
                    m_frameMetadata.current_set_data.exposure_time = (uint32_t)m_fdmeta_shot->shot.udm.ae.vendorSpecific[64];
                else
                    m_frameMetadata.current_set_data.exposure_time = (uint32_t)m_fdmeta_shot->shot.udm.internal.vendorSpecific2[100];

                m_frameMetadata.current_set_data.iso = (uint16_t)m_fdmeta_shot->shot.udm.internal.vendorSpecific2[101];
                m_frameMetadata.current_set_data.lens_position_min = (uint16_t)m_fdmeta_shot->shot.udm.af.lensPositionMacro;
                m_frameMetadata.current_set_data.lens_position_max = (uint16_t)m_fdmeta_shot->shot.udm.af.lensPositionInfinity;
                m_frameMetadata.current_set_data.lens_position_current = (uint16_t)m_fdmeta_shot->shot.udm.af.lensPositionCurrent;
                m_frameMetadata.current_set_data.driver_resolution = m_fdmeta_shot->shot.udm.companion.pdaf.lensPosResolution;
                CLOGD("current_set_data.exposure_time = %d", m_frameMetadata.current_set_data.exposure_time);
                CLOGD("current_set_data.iso = %d", m_frameMetadata.current_set_data.iso);
                CLOGD("current_set_data.lens_position_min = %d", m_frameMetadata.current_set_data.lens_position_min);
                CLOGD("current_set_data.lens_position_max = %d", m_frameMetadata.current_set_data.lens_position_max);
                CLOGD("current_set_data.lens_position_current = %d", m_frameMetadata.current_set_data.lens_position_current);
                CLOGD("current_set_data.driver_resolution = %d", m_frameMetadata.current_set_data.driver_resolution);

                m_flagMetaDataSet = true;
            }
        }
    } else {
        m_frameMetadata.current_set_data.exposure_time = (uint32_t)m_fdmeta_shot->shot.dm.sensor.exposureTime;
        m_frameMetadata.current_set_data.exposure_value = (int16_t)m_fdmeta_shot->shot.dm.aa.vendor_exposureValue;
        m_frameMetadata.current_set_data.iso = (uint16_t)m_fdmeta_shot->shot.dm.sensor.sensitivity;
        CLOGV("current_set_data.exposure_time = %d", m_frameMetadata.current_set_data.exposure_time);
        CLOGV("current_set_data.exposure_value = %d", m_frameMetadata.current_set_data.exposure_value);
        CLOGV("current_set_data.iso = %d", m_frameMetadata.current_set_data.iso);
    }

    m_AFstatus = autoFocusMgr->getCurrentState();
#endif

#ifdef SAMSUNG_OT
    if (m_exynosCameraParameters->getObjectTrackingGet() == true){
        m_frameMetadata.object_data.focusState = m_OTfocusData.FocusState;
        m_frameMetadata.object_data.focusROI[0] = m_OTfocusData.FocusROILeft;
        m_frameMetadata.object_data.focusROI[1] = m_OTfocusData.FocusROITop;
        m_frameMetadata.object_data.focusROI[2] = m_OTfocusData.FocusROIRight;
        m_frameMetadata.object_data.focusROI[3] = m_OTfocusData.FocusROIBottom;
        CLOGV("[OBTR](%s[%d])focusState: %d, focusROI[0]: %d, focusROI[1]: %d, focusROI[2]: %d, focusROI[3]: %d",
            __FUNCTION__, __LINE__, m_OTfocusData.FocusState,
            m_OTfocusData.FocusROILeft, m_OTfocusData.FocusROITop,
            m_OTfocusData.FocusROIRight, m_OTfocusData.FocusROIBottom);
    }
#endif

#ifdef SAMSUNG_CAMERA_EXTRA_INFO
    if (m_exynosCameraParameters->getFlashMode() == FLASH_MODE_AUTO) {
        ExynosCameraActivityFlash *m_flashMgr = m_exynosCameraActivityControl->getFlashMgr();
        m_frameMetadata.flash_enabled = (int32_t) m_flashMgr->getNeedCaptureFlash();
    } else {
        m_frameMetadata.flash_enabled = 0;
    }

    if (m_exynosCameraParameters->getRTHdr() == COMPANION_WDR_AUTO) {
        if (m_fdmeta_shot->shot.dm.stats.wdrAutoState == STATE_WDR_AUTO_REQUIRED)
            m_frameMetadata.hdr_enabled = 1;
        else
            m_frameMetadata.hdr_enabled = 0;
    } else {
        m_frameMetadata.hdr_enabled = 0;
    }

    m_frameMetadata.awb_exposure.brightness = m_fdmeta_shot->shot.udm.awb.vendorSpecific[11];
    m_frameMetadata.awb_exposure.colorTemp = m_fdmeta_shot->shot.udm.awb.vendorSpecific[21];
    m_frameMetadata.awb_exposure.awbRgbGain[0] = m_fdmeta_shot->shot.udm.awb.vendorSpecific[22];
    m_frameMetadata.awb_exposure.awbRgbGain[1] = m_fdmeta_shot->shot.udm.awb.vendorSpecific[23];
    m_frameMetadata.awb_exposure.awbRgbGain[2] = m_fdmeta_shot->shot.udm.awb.vendorSpecific[24];
#endif

    m_fdcallbackTimer.start();

    if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_METADATA) &&
            (m_flagStartFaceDetection || m_flagLLSStart || m_flagLightCondition
#ifdef SAMSUNG_CAMERA_EXTRA_INFO
            || m_flagFlashCallback || m_flagHdrCallback
#endif
#ifdef SAMSUNG_DOF
            || m_flagMetaDataSet
#endif
#ifdef SAMSUNG_OT
            || m_exynosCameraParameters->getObjectTrackingGet()
#endif
    ))
    {
        setBit(&m_callbackState, CALLBACK_STATE_PREVIEW_META, false);
        m_dataCb(CAMERA_MSG_PREVIEW_METADATA, m_fdCallbackHeap, 0, &m_frameMetadata, m_callbackCookie);
        clearBit(&m_callbackState, CALLBACK_STATE_PREVIEW_META, false);
    }
    m_fdcallbackTimer.stop();
    m_fdcallbackTimerTime = m_fdcallbackTimer.durationUsecs();

#ifdef SAMSUNG_DOF
    m_clearDOFmeta();
#endif

    if((int)m_fdcallbackTimerTime / 1000 > 50) {
        CLOGD("INFO(%s[%d]): FD callback duration time : (%d)mec", __FUNCTION__, __LINE__, (int)m_fdcallbackTimerTime / 1000);
    }

    return NO_ERROR;
}

#ifdef ONE_SECOND_BURST_CAPTURE
void ExynosCamera::m_clearOneSecondBurst(bool isJpegCallbackThread)
{
    if (m_exynosCameraParameters->getSeriesShotMode() == SERIES_SHOT_MODE_ONE_SECOND_BURST) {
        CLOGD("DEBUG(%s[%d]): DISABLE ONE SECOND BURST %d - TimeCheck", __FUNCTION__, __LINE__, isJpegCallbackThread);

        m_takePictureCounter.setCount(0);
#ifdef OIS_CAPTURE
        if (getCameraId() == CAMERA_ID_BACK) {
            ExynosCameraActivitySpecialCapture *m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();
            m_sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_WAIT_CAPTURE);
        }
#endif

        m_pictureEnabled = false;
        m_one_second_burst_capture = false;
        m_jpegCallbackCounter.clearCount();
        m_reprocessingCounter.clearCount();
        m_jpegCounter.clearCount();
        m_pictureCounter.clearCount();
        m_stopBurstShot = true;
        if (isJpegCallbackThread) {
            m_clearJpegCallbackThread(isJpegCallbackThread);
            m_exynosCameraParameters->setSeriesShotMode(SERIES_SHOT_MODE_NONE);
#ifdef USE_DVFS_LOCK
            if (m_exynosCameraParameters->getDvfsLock() == true)
                m_exynosCameraParameters->setDvfsLock(false);
#endif
        } else {
            /* waiting jpegCallbackThread first. jpegCallbackThread will launching m_clearJpegCallbackThread() */
            CLOGI("INFO(%s[%d]): wait m_jpegCallbackThread", __FUNCTION__, __LINE__);
            m_jpegCallbackThread->requestExit();
            m_jpegCallbackThread->requestExitAndWait();
            m_exynosCameraParameters->setSeriesShotMode(SERIES_SHOT_MODE_NONE);
        }

        CLOGD("DEBUG(%s[%d]): DISABLE ONE SECOND BURST DONE %d - TimeCheck", __FUNCTION__, __LINE__, isJpegCallbackThread);
    }
    if (m_one_second_jpegCallbackHeap != NULL) {
        m_one_second_jpegCallbackHeap->release(m_one_second_jpegCallbackHeap);
        m_one_second_jpegCallbackHeap = NULL;
    }
    if (m_one_second_postviewCallbackHeap != NULL) {
        m_one_second_postviewCallbackHeap->release(m_one_second_postviewCallbackHeap);
        m_one_second_postviewCallbackHeap = NULL;
    }
    for (int i = 0; i < ONE_SECOND_BURST_CAPTURE_CHECK_COUNT; i++)
        TakepictureDurationTime[i] = 0;
}
#endif

status_t ExynosCamera::sendCommand(int32_t command, int32_t arg1, __unused int32_t arg2)
{
    ExynosCameraActivityUCTL *uctlMgr = NULL;
#if defined(SAMSUNG_DOF) || defined(SAMSUNG_OT)
    ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
#endif
#ifdef SAMSUNG_OIS
    int zoomLevel;
    int zoomRatio;
#endif

    CLOGV("INFO(%s[%d])", __FUNCTION__, __LINE__);

    if (m_exynosCameraParameters != NULL) {
        if (m_exynosCameraParameters->getVisionMode() == true) {
            CLOGW("WRN(%s[%d]): Vision mode does not support", __FUNCTION__, __LINE__);
            android_printAssert(NULL, LOG_TAG, "Cannot support this operation");

            return INVALID_OPERATION;
        }

#ifdef SAMSUNG_OIS
        zoomLevel = m_exynosCameraParameters->getZoomLevel();
        zoomRatio = m_exynosCameraParameters->getZoomRatio(zoomLevel);
#endif
    } else {
        CLOGE("ERR(%s):m_exynosCameraParameters is NULL", __FUNCTION__);
        return INVALID_OPERATION;
    }

    /* TO DO : implemented based on the command */
    switch(command) {
    case CAMERA_CMD_START_FACE_DETECTION:
    case CAMERA_CMD_STOP_FACE_DETECTION:
        if (getMaxNumDetectedFaces() == 0) {
            CLOGE("ERR(%s):getMaxNumDetectedFaces == 0", __FUNCTION__);
            return BAD_VALUE;
        }

        if (arg1 == CAMERA_FACE_DETECTION_SW) {
            CLOGD("DEBUG(%s):only support HW face dectection", __FUNCTION__);
            return BAD_VALUE;
        }

        if (command == CAMERA_CMD_START_FACE_DETECTION) {
            CLOGD("DEBUG(%s):CAMERA_CMD_START_FACE_DETECTION is called!", __FUNCTION__);
            if (m_flagStartFaceDetection == false
                && startFaceDetection() == false) {
                CLOGE("ERR(%s):startFaceDetection() fail", __FUNCTION__);
                return BAD_VALUE;
            }
        } else {
            CLOGD("DEBUG(%s):CAMERA_CMD_STOP_FACE_DETECTION is called!", __FUNCTION__);
            if ( m_flagStartFaceDetection == true
                && stopFaceDetection() == false) {
                CLOGE("ERR(%s):stopFaceDetection() fail", __FUNCTION__);
                return BAD_VALUE;
            }
        }
        break;
#ifdef SAMSUNG_OT
    case 1504: /* HAL_OBJECT_TRACKING_STARTSTOP */
        CLOGD("DEBUG(%s):HAL_OBJECT_TRACKING_STARTSTOP(%d) is called!", __FUNCTION__, arg1);
        if(arg1) {
            /* Start case */
            int ret = uni_plugin_init(m_OTpluginHandle);
            if(ret < 0) {
                CLOGE("[OBTR](%s[%d]):Object Tracking plugin init failed!!", __FUNCTION__, __LINE__);
            }
            autoFocusMgr->setObjectTrackingStart(true);
            m_OTstart = OBJECT_TRACKING_INIT;
        }
        else {
            /* Stop case */
            autoFocusMgr->setObjectTrackingStart(false);
            m_OTstart = OBJECT_TRACKING_DEINIT;
            m_OTstatus = UNI_PLUGIN_OBJECT_TRACKING_IDLE;
            m_OTisTouchAreaGet = false;
            m_exynosCameraParameters->setObjectTrackingGet(false);
            m_clearOTmeta();
            /* Waiting here in the recording mode can make preview thread stop */
#if 0
            if(m_OTstart != OBJECT_TRACKING_IDLE) {
                m_OT_mutex.lock();
                m_OTisWait = true;
                m_OT_condition.waitRelative(m_OT_mutex, 1000000000); /* 1sec */
                m_OTisWait = false;
                m_OT_mutex.unlock();
            }
            CLOGD("DEBUG(%s):HAL_OBJECT_TRACKING_STARTSTOP(%d) Out is called!", __FUNCTION__, arg1);
#endif
        }
        break;
#endif
#ifdef BURST_CAPTURE
    case 1571: /* HAL_CMD_RUN_BURST_TAKE */
        CLOGD("DEBUG(%s):HAL_CMD_RUN_BURST_TAKE is called!", __FUNCTION__);
#ifdef ONE_SECOND_BURST_CAPTURE
        m_clearOneSecondBurst(false);
#endif

        if( m_burstInitFirst ) {
            m_burstRealloc = true;
            m_burstInitFirst = false;
        }
        m_exynosCameraParameters->setSeriesShotMode(SERIES_SHOT_MODE_BURST);
        m_exynosCameraParameters->setSeriesShotSaveLocation(arg1);
        m_takePictureCounter.setCount(0);

        m_burstCaptureCallbackCount = 0;

        {
            // Check jpeg save path
            char default_path[20];
            int SeriesSaveLocation = m_exynosCameraParameters->getSeriesShotSaveLocation();

            memset(default_path, 0, sizeof(default_path));

            if (SeriesSaveLocation == BURST_SAVE_PHONE)
                strncpy(default_path, CAMERA_SAVE_PATH_PHONE, sizeof(default_path)-1);
            else if (SeriesSaveLocation == BURST_SAVE_SDCARD)
                strncpy(default_path, CAMERA_SAVE_PATH_EXT, sizeof(default_path)-1);

            m_CheckCameraSavingPath(default_path,
                m_exynosCameraParameters->getSeriesShotFilePath(),
                m_burstSavePath, sizeof(m_burstSavePath));
        }
#ifdef SAMSUNG_BD
        m_BDbufferIndex = 0;
        if(m_BDpluginHandle != NULL && m_BDstatus != BLUR_DETECTION_INIT) {
            int ret = uni_plugin_init(m_BDpluginHandle);
            if(ret < 0) {
                CLOGE("[BD](%s[%d]): Plugin init failed!!", __FUNCTION__, __LINE__);
            }
            for(int i = 0; i < MAX_BD_BUFF_NUM; i++) {
                m_BDbuffer[i].data = new unsigned char[BD_EXIF_SIZE];
            }
        }
        m_BDstatus = BLUR_DETECTION_INIT;
#endif
#ifdef USE_DVFS_LOCK
        m_exynosCameraParameters->setDvfsLock(true);
#endif
        break;
    case 1572: /* HAL_CMD_STOP_BURST_TAKE */
        CLOGD("DEBUG(%s):HAL_CMD_STOP_BURST_TAKE is called!", __FUNCTION__);
        m_takePictureCounter.setCount(0);

#ifdef OIS_CAPTURE
        if (getCameraId() == CAMERA_ID_BACK) {
            ExynosCameraActivitySpecialCapture *m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();
            if (!m_exynosCameraParameters->getOISCaptureModeOn()) {
                m_sCaptureMgr->setMultiCaptureMode(false);
            }
            /* setMultiCaptureMode flag will be disabled in SCAPTURE_STEP_END in case of OIS capture */
            m_sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_WAIT_CAPTURE);
            m_exynosCameraParameters->setOISCaptureModeOn(false);
        }
#endif

        if (m_exynosCameraParameters->getSeriesShotCount() == MAX_SERIES_SHOT_COUNT) {
             m_reprocessingCounter.clearCount();
             m_pictureCounter.clearCount();
             m_jpegCounter.clearCount();
        }

        m_stopBurstShot = true;

        m_clearJpegCallbackThread(false);

        m_exynosCameraParameters->setSeriesShotMode(SERIES_SHOT_MODE_NONE);
#ifdef SAMSUNG_BD
        m_BDbufferQ->release();

        if(m_BDpluginHandle != NULL && m_BDstatus == BLUR_DETECTION_INIT) {
            int ret = uni_plugin_deinit(m_BDpluginHandle);
            if(ret < 0) {
                CLOGE("[BD](%s[%d]): Plugin deinit failed!!", __FUNCTION__, __LINE__);
            }
            for(int i = 0; i < MAX_BD_BUFF_NUM; i++) {
                if(m_BDbuffer[i].data != NULL){
                    delete []m_BDbuffer[i].data;
                }
            }
        }
        m_BDstatus = BLUR_DETECTION_DEINIT;
#endif

#ifdef USE_DVFS_LOCK
        if (m_exynosCameraParameters->getDvfsLock() == true)
            m_exynosCameraParameters->setDvfsLock(false);
#endif
        break;
    case 1573: /* HAL_CMD_DELETE_BURST_TAKE */
        CLOGD("DEBUG(%s):HAL_CMD_DELETE_BURST_TAKE is called!", __FUNCTION__);
        m_takePictureCounter.setCount(0);
        break;
#endif
#ifdef LLS_CAPTURE
    case 1600: /* CAMERA_CMD_START_BURST_PANORAMA */
        CLOGD("DEBUG(%s):CAMERA_CMD_START_BURST_PANORAMA is called!", __FUNCTION__);
        m_takePictureCounter.setCount(0);
        break;
    case 1601: /*CAMERA_CMD_STOP_BURST_PANORAMA */
        CLOGD("DEBUG(%s):CAMERA_CMD_STOP_BURST_PANORAMA is called!", __FUNCTION__);
        m_takePictureCounter.setCount(0);
        break;
    case 1516: /*CAMERA_CMD_START_SERIES_CAPTURE */
        CLOGD("DEBUG(%s):CAMERA_CMD_START_SERIES_CAPTURE is called!%d", __FUNCTION__, arg1);
#ifdef ONE_SECOND_BURST_CAPTURE
        m_clearOneSecondBurst(false);
#endif
        m_exynosCameraParameters->setSeriesShotMode(SERIES_SHOT_MODE_LLS, arg1);
        m_takePictureCounter.setCount(0);
        break;
    case 1517: /* CAMERA_CMD_STOP_SERIES_CAPTURE */
        CLOGD("DEBUG(%s):CAMERA_CMD_STOP_SERIES_CAPTURE is called!", __FUNCTION__);
        m_exynosCameraParameters->setSeriesShotMode(SERIES_SHOT_MODE_NONE);
        m_takePictureCounter.setCount(0);
#ifdef SET_LLS_CAPTURE_SETFILE
        m_exynosCameraParameters->setLLSCaptureOn(false);
#endif
#ifdef FLASHED_LLS_CAPTURE
        if (getCameraId() == CAMERA_ID_BACK && m_captureSelector->getFlashedLLSCaptureStatus() == true) {
            m_captureSelector->setFlashedLLSCaptureStatus(false);
#ifdef LLS_REPROCESSING
            ExynosCameraActivitySpecialCapture *m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();
            m_sCaptureMgr->setIsFlashLLSCapture(false);
#endif
        }
#endif

#ifdef OIS_CAPTURE
        if (getCameraId() == CAMERA_ID_BACK && !m_exynosCameraParameters->getOISCaptureModeOn()) {
            ExynosCameraActivitySpecialCapture *m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();
            m_sCaptureMgr->setMultiCaptureMode(false);
        }
#endif
        break;
#endif
    case 1351: /*CAMERA_CMD_AUTO_LOW_LIGHT_SET */
        CLOGD("DEBUG(%s):CAMERA_CMD_AUTO_LOW_LIGHT_SET is called!%d", __FUNCTION__, arg1);
        if(arg1) {
            if( m_flagLLSStart != true) {
                m_flagLLSStart = true;
#ifdef LLS_CAPTURE
                m_exynosCameraParameters->setLLSOn(m_flagLLSStart);
                m_exynosCameraParameters->m_setLLSShotMode();
#endif
            }
        } else {
            m_flagLLSStart = false;
#ifdef LLS_CAPTURE
            m_exynosCameraParameters->setLLSOn(m_flagLLSStart);
            m_exynosCameraParameters->m_setShotMode(m_exynosCameraParameters->getShotMode());
#endif
        }
        break;
    case 1801: /* HAL_ENABLE_LIGHT_CONDITION*/
        CLOGD("DEBUG(%s):HAL_ENABLE_LIGHT_CONDITION is called!%d", __FUNCTION__, arg1);
        if(arg1) {
            m_flagLightCondition = true;
        } else {
            m_flagLightCondition = false;
        }
        break;
#ifdef SAMSUNG_CAMERA_EXTRA_INFO
    case 1802: /* HAL_ENABLE_FLASH_AUTO_CALLBACK*/
        CLOGD("DEBUG(%s):HAL_ENABLE_FLASH_AUTO_CALLBACK is called!%d", __FUNCTION__, arg1);
        if(arg1) {
            m_flagFlashCallback = true;
        } else {
            m_flagFlashCallback = false;
        }
        break;
    case 1803: /* HAL_ENABLE_HDR_AUTO_CALLBACK*/
        CLOGD("DEBUG(%s):HAL_ENABLE_HDR_AUTO_CALLBACK is called!%d", __FUNCTION__, arg1);
        if(arg1) {
            m_flagHdrCallback = true;
        } else {
            m_flagHdrCallback = false;
        }
        break;
#endif
#ifdef SR_CAPTURE
    case 1519: /* CAMERA_CMD_SUPER_RESOLUTION_MODE */
        CLOGD("DEBUG(%s):CAMERA_CMD_SUPER_RESOLUTION_MODE is called!%d", __FUNCTION__, arg1);
        m_exynosCameraParameters->setSROn(arg1);
        if (arg1) {
            m_sr_cropped_width = 0;
            m_sr_cropped_height = 0;
            m_isCopySrMdeta = true;
            memset(m_faces_sr, 0, sizeof(camera_face_t) * NUM_OF_DETECTED_FACES);
            memset(&m_sr_frameMetadata, 0, sizeof(camera_frame_metadata_t));
        }
        break;
#endif
    /* 1508: HAL_SET_SAMSUNG_CAMERA */
    case 1508 :
        CLOGD("DEBUG(%s):HAL_SET_SAMSUNG_CAMERA is called!%d", __FUNCTION__, arg1);
        m_exynosCameraParameters->setSamsungCamera(true);
        break;
    /* 1510: CAMERA_CMD_SET_FLIP */
    case 1510 :
        CLOGD("DEBUG(%s):CAMERA_CMD_SET_FLIP is called!%d", __FUNCTION__, arg1);
        m_exynosCameraParameters->setFlipHorizontal(arg1);
        break;
    /* 1521: CAMERA_CMD_DEVICE_ORIENTATION */
    case 1521:
        CLOGD("DEBUG(%s):CAMERA_CMD_DEVICE_ORIENTATION is called!%d", __FUNCTION__, arg1);
        m_exynosCameraParameters->setDeviceOrientation(arg1);
        uctlMgr = m_exynosCameraActivityControl->getUCTLMgr();
        if (uctlMgr != NULL)
            uctlMgr->setDeviceRotation(m_exynosCameraParameters->getFdOrientation());
        break;
    /*1641: CAMERA_CMD_ADVANCED_MACRO_FOCUS*/
    case 1641:
        CLOGD("DEBUG(%s):CAMERA_CMD_ADVANCED_MACRO_FOCUS is called!%d", __FUNCTION__, arg1);
        m_exynosCameraParameters->setAutoFocusMacroPosition(ExynosCameraActivityAutofocus::AUTOFOCUS_MACRO_POSITION_CENTER);
        break;
    /*1642: CAMERA_CMD_FOCUS_LOCATION*/
    case 1642:
        CLOGD("DEBUG(%s):CAMERA_CMD_FOCUS_LOCATION is called!%d", __FUNCTION__, arg1);
        m_exynosCameraParameters->setAutoFocusMacroPosition(ExynosCameraActivityAutofocus::AUTOFOCUS_MACRO_POSITION_CENTER_UP);
        break;
#ifdef SAMSUNG_DOF
    /*1643: HAL_ENABLE_CURRENT_SET */
    case 1643:
        CLOGD("DEBUG(%s):HAL_ENABLE_CURRENT_SET is called!%d", __FUNCTION__, arg1);
        if(arg1)
            startCurrentSet();
        else
            stopCurrentSet();
        break;
    /*1644: HAL_ENABLE_PAF_RESULT */
    case 1644:
        CLOGD("DEBUG(%s):HAL_ENABLE_PAF_RESULT is called!%d", __FUNCTION__, arg1);
        break;
    /*1650: CAMERA_CMD_START_PAF_CALLBACK */
    case 1650:
        CLOGD("DEBUG(%s):CAMERA_CMD_START_PAF_CALLBACK is called!%d", __FUNCTION__, arg1);
        autoFocusMgr->setPDAF(true);
        break;
    /*1651: CAMERA_CMD_STOP_PAF_CALLBACK */
    case 1651:
        CLOGD("DEBUG(%s):CAMERA_CMD_STOP_PAF_CALLBACK is called!%d", __FUNCTION__, arg1);
        autoFocusMgr->setPDAF(false);
        break;
#endif
    /*1661: CAMERA_CMD_START_ZOOM */
    case 1661:
        CLOGD("DEBUG(%s):CAMERA_CMD_START_ZOOM is called!", __FUNCTION__);
        m_exynosCameraParameters->setZoomActiveOn(true);
        m_exynosCameraParameters->setFocusModeLock(true);
        break;
    /*1662: CAMERA_CMD_STOP_ZOOM */
    case 1662:
        CLOGD("DEBUG(%s):CAMERA_CMD_STOP_ZOOM is called!", __FUNCTION__);
        m_exynosCameraParameters->setZoomActiveOn(false);
        m_exynosCameraParameters->setFocusModeLock(false);
#ifdef SAMSUNG_OIS
        m_exynosCameraParameters->setOISMode();
#endif
        break;
#ifdef SAMSUNG_HLV
    case 1721:  /* HAL_RECORDING_RESUME */
        {
            UNI_PLUGIN_CAPTURE_STATUS value = UNI_PLUGIN_CAPTURE_STATUS_VID_REC_RESUME;
            ALOGD("HLV: set UNI_PLUGIN_CAPTURE_STATUS_VID_REC_RESUME");

            if (m_HLV) {
                uni_plugin_set(m_HLV,
                        HIGHLIGHT_VIDEO_PLUGIN_NAME,
                        UNI_PLUGIN_INDEX_CAPTURE_STATUS,
                        &value);
            }
            else {
                ALOGE("HLV: %s not yet loaded", HIGHLIGHT_VIDEO_PLUGIN_NAME);
            }
        }
        break;

    case 1722: /* HAL_RECORDING_PAUSE */
        {
            ALOGD("HLV: set UNI_PLUGIN_CAPTURE_STATUS_VID_REC_PAUSE");
            UNI_PLUGIN_CAPTURE_STATUS value = UNI_PLUGIN_CAPTURE_STATUS_VID_REC_PAUSE;

            if (m_HLV) {
                uni_plugin_set(m_HLV,
                        HIGHLIGHT_VIDEO_PLUGIN_NAME,
                        UNI_PLUGIN_INDEX_CAPTURE_STATUS,
                        &value);
            }
            else {
                ALOGE("HLV: %s not yet loaded", HIGHLIGHT_VIDEO_PLUGIN_NAME);
            }
        }
        break;
#endif

#ifdef USE_LIVE_BROADCAST
    case 1730: /* SET PLB mode */
        CLOGD("DEBUG(%s):SET PLB mode is called!%d", __FUNCTION__, arg1);
        m_exynosCameraParameters->setPLBMode((bool)arg1);
        break;
#endif

    default:
        CLOGV("DEBUG(%s):unexpectect command(%d)", __FUNCTION__, command);
        break;
    }

    return NO_ERROR;
}

status_t ExynosCamera::generateFrame(int32_t frameCount, ExynosCameraFrame **newFrame)
{
    Mutex::Autolock lock(m_frameLock);

    int ret = 0;
    *newFrame = NULL;

    if (frameCount >= 0) {
        ret = m_searchFrameFromList(&m_processList, frameCount, newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):searchFrameFromList fail", __FUNCTION__, __LINE__);
            return INVALID_OPERATION;
        }
    }

    if (*newFrame == NULL) {
        *newFrame = m_previewFrameFactory->createNewFrame();

        if (*newFrame == NULL) {
            CLOGE("ERR(%s):newFrame is NULL", __FUNCTION__);
            return UNKNOWN_ERROR;
        }

        bool flagRequested = false;

        if (isOwnScc(getCameraId()) == true)
            flagRequested = (*newFrame)->getRequest(PIPE_SCC);
        else
            flagRequested = (*newFrame)->getRequest(PIPE_ISPC);

        if (flagRequested == true) {
            m_dynamicSccCount++;
            CLOGV("DEBUG(%s[%d]):dynamicSccCount inc(%d) frameCount(%d)", __FUNCTION__, __LINE__, m_dynamicSccCount, (*newFrame)->getFrameCount());
        }

        m_processList.push_back(*newFrame);
    }

    return ret;
}

status_t ExynosCamera::generateFrameSccScp(uint32_t pipeId, uint32_t *frameCount, ExynosCameraFrame **frame)
{
    int  ret = 0;
    int  regenerateFrame = 0;
    int  count = *frameCount;
    ExynosCameraFrame *newframe = NULL;

    do {
        *frame = NULL;
        ret = generateFrame(count, frame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):generateFrame fail, pipeID: %d", __FUNCTION__, __LINE__, pipeId);
            return ret;
        }

        newframe = *frame;
        if (((PIPE_SCP == pipeId) && newframe->getScpDrop()) ||
            ((m_cameraId == CAMERA_ID_FRONT) && (PIPE_SCC == pipeId) && (newframe->getSccDrop() == true)) ||
            ((m_cameraId == CAMERA_ID_FRONT) && (PIPE_ISPC == pipeId) && (newframe->getIspcDrop() == true))) {
            count++;

            ret = newframe->setEntityState(pipeId, ENTITY_STATE_FRAME_SKIP);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):setEntityState fail, pipeId(%d), state(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipeId, ENTITY_STATE_FRAME_SKIP, ret);
                *frameCount = count;
                return ret;
            }

            /* let m_mainThreadFunc handle the processlist cleaning part */
            m_pipeFrameDoneQ->pushProcessQ(&newframe);

            regenerateFrame = 1;
            continue;
        }
        regenerateFrame = 0;
    } while (regenerateFrame);

    *frameCount = count;
    return NO_ERROR;
}

status_t ExynosCamera::m_setupEntity(
        uint32_t pipeId,
        ExynosCameraFrame *newFrame,
        ExynosCameraBuffer *srcBuf,
        ExynosCameraBuffer *dstBuf)
{
    int ret = 0;
    entity_buffer_state_t entityBufferState;

    /* set SRC buffer */
    ret = newFrame->getSrcBufferState(pipeId, &entityBufferState);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getSrcBufferState fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
        return ret;
    }

    if (entityBufferState == ENTITY_BUFFER_STATE_REQUESTED) {
        ret = m_setSrcBuffer(pipeId, newFrame, srcBuf);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_setSrcBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
            return ret;
        }
    }

    /* set DST buffer */
    ret = newFrame->getDstBufferState(pipeId, &entityBufferState);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getDstBufferState fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
        return ret;
    }

    if (entityBufferState == ENTITY_BUFFER_STATE_REQUESTED) {
        ret = m_setDstBuffer(pipeId, newFrame, dstBuf);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_setDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
            return ret;
        }
    }

    ret = newFrame->setEntityState(pipeId, ENTITY_STATE_PROCESSING);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setEntityState(ENTITY_STATE_PROCESSING) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
        return ret;
    }

    return NO_ERROR;
}

status_t ExynosCamera::m_setSrcBuffer(
        uint32_t pipeId,
        ExynosCameraFrame *newFrame,
        ExynosCameraBuffer *buffer)
{
    int ret = 0;
    int bufIndex = -1;
    ExynosCameraBufferManager *bufferMgr = NULL;
    ExynosCameraBuffer srcBuf;

    if (buffer == NULL) {
        buffer = &srcBuf;

        ret = m_getBufferManager(pipeId, &bufferMgr, SRC_BUFFER_DIRECTION);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getBufferManager(SRC) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
            return ret;
        }

        if (bufferMgr == NULL) {
            CLOGE("ERR(%s[%d]):buffer manager is NULL, pipeId(%d)", __FUNCTION__, __LINE__, pipeId);
            return BAD_VALUE;
        }

        /* get buffers */
        ret = bufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, buffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getBuffer fail, pipeId(%d), frameCount(%d), ret(%d)",
                __FUNCTION__, __LINE__, pipeId, newFrame->getFrameCount(), ret);
            bufferMgr->dump();
            return ret;
        }
    }

    /* set buffers */
    ret = newFrame->setSrcBuffer(pipeId, *buffer);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setSrcBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
        return ret;
    }

    return NO_ERROR;
}

status_t ExynosCamera::m_setDstBuffer(
        uint32_t pipeId,
        ExynosCameraFrame *newFrame,
        ExynosCameraBuffer *buffer)
{
    int ret = 0;
    int bufIndex = -1;
    ExynosCameraBufferManager *bufferMgr = NULL;
    ExynosCameraBuffer dstBuf;

    if (buffer == NULL) {
        buffer = &dstBuf;

        ret = m_getBufferManager(pipeId, &bufferMgr, DST_BUFFER_DIRECTION);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getBufferManager(DST) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
            return ret;
        }

        if (bufferMgr == NULL) {
            CLOGE("ERR(%s[%d]):buffer manager is NULL, pipeId(%d)", __FUNCTION__, __LINE__, pipeId);
            return BAD_VALUE;
        }

        /* get buffers */
        ret = bufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, buffer);
        if (ret < 0) {
            ExynosCameraFrameEntity *curEntity = newFrame->searchEntityByPipeId(pipeId);
            if (curEntity != NULL) {
                if (curEntity->getBufType() == ENTITY_BUFFER_DELIVERY) {
                    CLOGV("DEBUG(%s[%d]): pipe(%d) buffer is empty for delivery", __FUNCTION__, __LINE__, pipeId);
                    buffer->index = -1;
                } else {
                    CLOGE("ERR(%s[%d]):getBuffer fail, pipeId(%d), frameCount(%d), ret(%d)",
                            __FUNCTION__, __LINE__, pipeId, newFrame->getFrameCount(), ret);
                    return ret;
                }
            } else {
                CLOGE("ERR(%s[%d]):curEntity is NULL, pipeId(%d)", __FUNCTION__, __LINE__, pipeId);
                return ret;
            }
        }
    }

    /* set buffers */
    ret = newFrame->setDstBuffer(pipeId, *buffer);

    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
        return ret;
    }

    return NO_ERROR;
}

status_t ExynosCamera::generateFrameReprocessing(ExynosCameraFrame **newFrame)
{
    Mutex::Autolock lock(m_frameLock);

    int ret = 0;
    struct ExynosCameraBuffer tempBuffer;
    int bufIndex = -1;

     /* 1. Make Frame */
    *newFrame = m_reprocessingFrameFactory->createNewFrame();
    if (*newFrame == NULL) {
        CLOGE("ERR(%s):newFrame is NULL", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    return NO_ERROR;
}

status_t ExynosCamera::m_fastenAeStable(void)
{
    int ret = 0;
    ExynosCameraBuffer fastenAeBuffer[NUM_FASTAESTABLE_BUFFER];
    ExynosCameraBufferManager *skipBufferMgr = NULL;
    m_createInternalBufferManager(&skipBufferMgr, "SKIP_BUF");

    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    planeSize[0] = 32 * 64 * 2;
    int planeCount  = 2;
#ifdef SAMSUNG_COMPANION
    int bufferCount = (m_exynosCameraParameters->getRTHdr() == COMPANION_WDR_ON) ?
                        NUM_FASTAESTABLE_BUFFER : NUM_FASTAESTABLE_BUFFER - 4;
#else
    int bufferCount = NUM_FASTAESTABLE_BUFFER;
#endif

    if (skipBufferMgr == NULL) {
        CLOGE("ERR(%s[%d]):createBufferManager failed", __FUNCTION__, __LINE__);
        goto func_exit;
    }

    ret = m_allocBuffers(skipBufferMgr, planeCount, planeSize, bytesPerLine, bufferCount, true, false);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_3aaBufferMgr m_allocBuffers(bufferCount=%d) fail",
            __FUNCTION__, __LINE__, bufferCount);
        return ret;
    }

    for (int i = 0; i < bufferCount; i++) {
        int index = i;
        ret = skipBufferMgr->getBuffer(&index, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &fastenAeBuffer[i]);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getBuffer fail", __FUNCTION__, __LINE__);
            goto done;
        }
    }

    ret = m_previewFrameFactory->fastenAeStable(bufferCount, fastenAeBuffer);
    if (ret < 0) {
        ret = INVALID_OPERATION;
        // doing some error handling
    }

    m_checkFirstFrameLux = true;
done:
    skipBufferMgr->deinit();
    delete skipBufferMgr;
    skipBufferMgr = NULL;

func_exit:
    return ret;
}

#ifdef CAMERA_FAST_ENTRANCE_V1
bool ExynosCamera::m_fastenAeThreadFunc(void)
{
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    int32_t skipFrameCount = INITIAL_SKIP_FRAME;
    int ret = NO_ERROR;
    m_fastenAeThreadResult = 0;

    /* frame manager start */
    m_frameMgr->start();

    /*
     * This is for updating parameter value at once.
     * This must be just before making factory
     */
    m_exynosCameraParameters->updateTpuParameters();

    /* setup frameFactory with scenario */
    m_setupFrameFactory();

    if (m_previewFrameFactory->isCreated() == false) {
#if defined(SAMSUNG_EEPROM)
        if ((m_use_companion == false) && (isEEprom(getCameraId()) == true)) {
            if (m_eepromThread != NULL) {
                CLOGD("DEBUG(%s): eepromThread join.....", __FUNCTION__);
                m_eepromThread->join();
            } else {
                CLOGD("DEBUG(%s): eepromThread is NULL.", __FUNCTION__);
            }
            m_exynosCameraParameters->setRomReadThreadDone(true);
            CLOGD("DEBUG(%s): eepromThread joined", __FUNCTION__);
        }
#endif /* SAMSUNG_EEPROM */

#ifdef SAMSUNG_COMPANION
        if(m_use_companion == true) {
            ret = m_previewFrameFactory->precreate();
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):m_previewFrameFactory->precreate() failed", __FUNCTION__, __LINE__);
                goto err;
            }

            m_waitCompanionThreadEnd();

            m_exynosCameraParameters->setRomReadThreadDone(true);

            ret = m_previewFrameFactory->postcreate();
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):m_previewFrameFactory->postcreate() failed", __FUNCTION__, __LINE__);
                goto err;
            }
        } else {
            ret = m_previewFrameFactory->create();
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):m_previewFrameFactory->create() failed", __FUNCTION__, __LINE__);
                goto err;
            }
        }
#else
        ret = m_previewFrameFactory->create();
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_previewFrameFactory->create() failed", __FUNCTION__, __LINE__);
            goto err;
        }
#endif /* SAMSUNG_COMPANION */
        CLOGD("DEBUG(%s):FrameFactory(previewFrameFactory) created", __FUNCTION__);
    }

#ifdef USE_QOS_SETTING
    ret = m_previewFrameFactory->setControl(V4L2_CID_IS_DVFS_CLUSTER1, BIG_CORE_MAX_LOCK, PIPE_3AA);
    if (ret < 0)
        CLOGE("ERR(%s[%d]):V4L2_CID_IS_DVFS_CLUSTER1 setControl fail, ret(%d)", __FUNCTION__, __LINE__, ret);
#endif

    if (m_exynosCameraParameters->getUseFastenAeStable() == true &&
        m_exynosCameraParameters->getDualMode() == false &&
        m_exynosCameraParameters->getRecordingHint() == false &&
        m_isFirstStart == true) {

        ret = m_fastenAeStable();
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_fastenAeStable() failed", __FUNCTION__, __LINE__);
            goto err;
        } else {
            skipFrameCount = 0;
            m_exynosCameraParameters->setUseFastenAeStable(false);
        }
    }

#ifdef USE_FADE_IN_ENTRANCE
    if (m_exynosCameraParameters->getUseFastenAeStable() == true) {
        CLOGE("ERR(%s[%d]):consistency in skipFrameCount might be broken by FASTEN_AE and FADE_IN", __FUNCTION__, __LINE__);
    }
#endif

    m_exynosCameraParameters->setFrameSkipCount(skipFrameCount);

    /* one shot */
    return false;

err:
    m_fastenAeThreadResult = ret;

#ifdef SAMSUNG_COMPANION
    if(m_use_companion == true) {
        m_waitCompanionThreadEnd();
    }
#endif
#if defined(SAMSUNG_EEPROM)
    if ((m_use_companion == false) && (isEEprom(getCameraId()) == true)) {
        if (m_eepromThread != NULL) {
            m_eepromThread->join();
        } else {
            CLOGD("DEBUG(%s): eepromThread is NULL.", __FUNCTION__);
        }
    }
#endif

     return false;
}

status_t ExynosCamera::m_waitFastenAeThreadEnd(void)
{
    ExynosCameraDurationTimer timer;

    timer.start();

    if(m_fastEntrance == true) {
        if (m_fastenAeThread != NULL) {
            m_fastenAeThread->join();
        } else {
            CLOGI("INFO(%s[%d]):m_fastenAeThread is NULL", __FUNCTION__, __LINE__);
        }
    }

    timer.stop();

    CLOGD("DEBUG(%s[%d]):fastenAeThread waiting time : duration time(%5d msec)", __FUNCTION__, __LINE__, (int)timer.durationMsecs());
    CLOGD("DEBUG(%s[%d]):fastenAeThread join", __FUNCTION__, __LINE__);

    return NO_ERROR;
}
#endif

status_t ExynosCamera::m_startPreviewInternal(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    CLOGI("DEBUG(%s[%d]):IN", __FUNCTION__, __LINE__);

    uint32_t minBayerFrameNum = 0;
    uint32_t min3AAFrameNum = 0;
    int ret = 0;
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraBuffer dstBuf;
    ExynosCameraBufferManager *scpBufferMgr = NULL;
    int32_t reprocessingBayerMode = m_exynosCameraParameters->getReprocessingBayerMode();
    enum pipeline pipe;
    bool zoomWithScaler = false;

    m_fliteFrameCount = 0;
    m_3aa_ispFrameCount = 0;
    m_ispFrameCount = 0;
    m_sccFrameCount = 0;
    m_scpFrameCount = 0;
    m_displayPreviewToggle = 0;

    zoomWithScaler = m_exynosCameraParameters->getSupportedZoomPreviewWIthScaler();

    if (m_exynosCameraParameters->isFlite3aaOtf() == true)
        minBayerFrameNum = m_exynosconfig->current->bufInfo.init_bayer_buffers;
    else
        minBayerFrameNum = m_exynosconfig->current->bufInfo.num_bayer_buffers
                    - m_exynosconfig->current->bufInfo.reprocessing_bayer_hold_count;

    /*
     * with MCPipe, we need to putBuffer 3 buf.
     */
    /*
    if (reprocessingBayerMode == REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON)
        min3AAFrameNum = minBayerFrameNum;
    else
        min3AAFrameNum = m_exynosconfig->current->pipeInfo.prepare[PIPE_3AA];
    */

    min3AAFrameNum = m_exynosconfig->current->pipeInfo.prepare[PIPE_3AA];

    ExynosCameraBufferManager *taaBufferManager[MAX_NODE];
    ExynosCameraBufferManager *ispBufferManager[MAX_NODE];
    ExynosCameraBufferManager *disBufferManager[MAX_NODE];

    for (int i = 0; i < MAX_NODE; i++) {
        taaBufferManager[i] = NULL;
        ispBufferManager[i] = NULL;
        disBufferManager[i] = NULL;
    }

    m_exynosCameraParameters->setZoomPreviewWIthScaler(zoomWithScaler);

#ifdef FPS_CHECK
    for (int i = 0; i < DEBUG_MAX_PIPE_NUM; i++)
        m_debugFpsCount[i] = 0;
#endif

    switch (reprocessingBayerMode) {
        case REPROCESSING_BAYER_MODE_NONE : /* Not using reprocessing */
            CLOGD("DEBUG(%s[%d]): Use REPROCESSING_BAYER_MODE_NONE", __FUNCTION__, __LINE__);
            m_previewFrameFactory->setRequestFLITE(false);
            m_previewFrameFactory->setRequest3AC(false);
            break;
        case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON :
            CLOGD("DEBUG(%s[%d]): Use REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON", __FUNCTION__, __LINE__);
            m_previewFrameFactory->setRequestFLITE(true);
            m_previewFrameFactory->setRequest3AC(false);
            break;
        case REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON :
            CLOGD("DEBUG(%s[%d]): Use REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON", __FUNCTION__, __LINE__);
            m_previewFrameFactory->setRequestFLITE(false);
            m_previewFrameFactory->setRequest3AC(true);
            break;
        case REPROCESSING_BAYER_MODE_PURE_DYNAMIC :
            CLOGD("DEBUG(%s[%d]): Use REPROCESSING_BAYER_MODE_PURE_DYNAMIC", __FUNCTION__, __LINE__);
            m_previewFrameFactory->setRequestFLITE(false);
            m_previewFrameFactory->setRequest3AC(false);
            break;
        case REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC :
            CLOGD("DEBUG(%s[%d]): Use REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC", __FUNCTION__, __LINE__);
            m_previewFrameFactory->setRequestFLITE(false);
            m_previewFrameFactory->setRequest3AC(false);
            break;
        default :
            CLOGE("ERR(%s[%d]): Unknown dynamic bayer mode", __FUNCTION__, __LINE__);
            m_previewFrameFactory->setRequest3AC(false);
            break;
    }

    if (m_exynosCameraParameters->getZoomPreviewWIthScaler() == true) {
        scpBufferMgr = m_zoomScalerBufferMgr;
    } else {
        scpBufferMgr = m_scpBufferMgr;
    }

    if (m_exynosCameraParameters->getTpuEnabledMode() == true) {
        m_previewFrameFactory->setRequestISPP(true);
        m_previewFrameFactory->setRequestDIS(true);

        if (m_exynosCameraParameters->is3aaIspOtf() == true) {
            taaBufferManager[m_previewFrameFactory->getNodeType(PIPE_3AA)] = m_3aaBufferMgr;
            taaBufferManager[m_previewFrameFactory->getNodeType(PIPE_3AC)] = m_bayerBufferMgr;
            taaBufferManager[m_previewFrameFactory->getNodeType(PIPE_ISPP)] = m_hwDisBufferMgr;

            disBufferManager[m_previewFrameFactory->getNodeType(PIPE_DIS)] = m_hwDisBufferMgr;
            disBufferManager[m_previewFrameFactory->getNodeType(PIPE_SCP)] = scpBufferMgr;
        } else {
            taaBufferManager[m_previewFrameFactory->getNodeType(PIPE_3AA)] = m_3aaBufferMgr;
            taaBufferManager[m_previewFrameFactory->getNodeType(PIPE_3AC)] = m_bayerBufferMgr;
            taaBufferManager[m_previewFrameFactory->getNodeType(PIPE_3AP)] = m_ispBufferMgr;

            ispBufferManager[m_previewFrameFactory->getNodeType(PIPE_ISP)] = m_ispBufferMgr;
            ispBufferManager[m_previewFrameFactory->getNodeType(PIPE_ISPP)] = m_hwDisBufferMgr;

            disBufferManager[m_previewFrameFactory->getNodeType(PIPE_DIS)] = m_hwDisBufferMgr;
            disBufferManager[m_previewFrameFactory->getNodeType(PIPE_SCP)] = scpBufferMgr;
        }
     } else {
        m_previewFrameFactory->setRequestISPP(false);
        m_previewFrameFactory->setRequestDIS(false);

        if (m_exynosCameraParameters->is3aaIspOtf() == true) {
            if (m_exynosCameraParameters->getDualMode() == true && getCameraId() == CAMERA_ID_FRONT) {
                m_previewFrameFactory->setRequestFLITE(true);
                m_previewFrameFactory->setRequestISPC(true);

                taaBufferManager[m_previewFrameFactory->getNodeType(PIPE_3AA)] = m_bayerBufferMgr;
                taaBufferManager[m_previewFrameFactory->getNodeType(PIPE_ISPC)] = m_sccBufferMgr;
            } else {
                taaBufferManager[m_previewFrameFactory->getNodeType(PIPE_3AA)] = m_3aaBufferMgr;
#ifndef RAWDUMP_CAPTURE
                taaBufferManager[m_previewFrameFactory->getNodeType(PIPE_3AC)] = m_bayerBufferMgr;
#endif
                taaBufferManager[m_previewFrameFactory->getNodeType(PIPE_SCP)] = scpBufferMgr;
            }
        } else {
            taaBufferManager[m_previewFrameFactory->getNodeType(PIPE_3AA)] = m_3aaBufferMgr;
            taaBufferManager[m_previewFrameFactory->getNodeType(PIPE_3AC)] = m_bayerBufferMgr;
            taaBufferManager[m_previewFrameFactory->getNodeType(PIPE_3AP)] = m_ispBufferMgr;

            ispBufferManager[m_previewFrameFactory->getNodeType(PIPE_ISP)] = m_ispBufferMgr;
            ispBufferManager[m_previewFrameFactory->getNodeType(PIPE_SCP)] = scpBufferMgr;
        }
    }

    for (int i = 0; i < MAX_NODE; i++) {
        /* If even one buffer slot is valid. call setBufferManagerToPipe() */
        if (taaBufferManager[i] != NULL) {
            ret = m_previewFrameFactory->setBufferManagerToPipe(taaBufferManager, PIPE_3AA);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):m_previewFrameFactory->setBufferManagerToPipe(taaBufferManager, %d) failed",
                    __FUNCTION__, __LINE__, PIPE_3AA);
                return ret;
            }
            break;
        }
    }

    for (int i = 0; i < MAX_NODE; i++) {
        /* If even one buffer slot is valid. call setBufferManagerToPipe() */
        if (ispBufferManager[i] != NULL) {
            ret = m_previewFrameFactory->setBufferManagerToPipe(ispBufferManager, PIPE_ISP);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):m_previewFrameFactory->setBufferManagerToPipe(ispBufferManager, %d) failed",
                    __FUNCTION__, __LINE__, PIPE_ISP);
                return ret;
            }
            break;
        }
    }

    for (int i = 0; i < MAX_NODE; i++) {
        /* If even one buffer slot is valid. call setBufferManagerToPipe() */
        if (disBufferManager[i] != NULL) {
            ret = m_previewFrameFactory->setBufferManagerToPipe(disBufferManager, PIPE_DIS);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):m_previewFrameFactory->setBufferManagerToPipe(disBufferManager, %d) failed",
                    __FUNCTION__, __LINE__, PIPE_DIS);
                return ret;
            }
            break;
        }
    }

    ret = m_previewFrameFactory->initPipes();
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_previewFrameFactory->initPipes() failed", __FUNCTION__, __LINE__);
        return ret;
    }

    m_printExynosCameraInfo(__FUNCTION__);

    for (uint32_t i = 0; i < minBayerFrameNum; i++) {
        ret = generateFrame(i, &newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
            return ret;
        }
        if (newFrame == NULL) {
            CLOGE("ERR(%s[%d]):new faame is NULL", __FUNCTION__, __LINE__);
            return ret;
        }

        m_fliteFrameCount++;

        if (m_exynosCameraParameters->isFlite3aaOtf() == true) {
            if (reprocessingBayerMode == REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON) {
                m_setupEntity(m_getBayerPipeId(), newFrame);
                m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, m_getBayerPipeId());
                m_previewFrameFactory->pushFrameToPipe(&newFrame, m_getBayerPipeId());
            }

            if (i < min3AAFrameNum) {
                m_setupEntity(PIPE_3AA, newFrame);

                if (m_exynosCameraParameters->is3aaIspOtf() == true) {
                    m_previewFrameFactory->setFrameDoneQToPipe(m_pipeFrameDoneQ, PIPE_3AA);

                    if (m_exynosCameraParameters->getTpuEnabledMode() == true) {
                        m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, PIPE_DIS);
                    }
                } else {
                    m_previewFrameFactory->setFrameDoneQToPipe(m_pipeFrameDoneQ, PIPE_3AA);

                    if (m_exynosCameraParameters->getTpuEnabledMode() == true) {
                        m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, PIPE_DIS);
                        m_previewFrameFactory->setFrameDoneQToPipe(m_pipeFrameDoneQ, PIPE_ISP);
                    } else {
                        m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, PIPE_ISP);
                    }
                }

                m_previewFrameFactory->pushFrameToPipe(&newFrame, PIPE_3AA);
                m_3aa_ispFrameCount++;
            }
        } else {
            m_setupEntity(m_getBayerPipeId(), newFrame);
            m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, m_getBayerPipeId());
            m_previewFrameFactory->pushFrameToPipe(&newFrame, m_getBayerPipeId());

            if (m_exynosCameraParameters->is3aaIspOtf() == true) {
                m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, PIPE_3AA);
            } else {
                m_setupEntity(PIPE_3AA, newFrame);
                m_previewFrameFactory->setFrameDoneQToPipe(m_pipeFrameDoneQ, PIPE_3AA);
                m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, PIPE_ISP);
                m_previewFrameFactory->pushFrameToPipe(&newFrame, PIPE_3AA);
                m_3aa_ispFrameCount++;
            }

        }
#if 0
        /* SCC */
        if(m_exynosCameraParameters->isSccCapture() == true) {
            m_sccFrameCount++;

            if (isOwnScc(getCameraId()) == true) {
                pipe = PIPE_SCC;
            } else {
                pipe = PIPE_ISPC;
            }

            if(newFrame->getRequest(pipe)) {
                m_setupEntity(pipe, newFrame);
                m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipe);
                m_previewFrameFactory->pushFrameToPipe(&newFrame, pipe);
            }
        }
#endif
        /* SCP */
/* Comment out, because it included ISP */
/*
        m_scpFrameCount++;

        m_setupEntity(PIPE_SCP, newFrame);
        m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, PIPE_SCP);
        m_previewFrameFactory->pushFrameToPipe(&newFrame, PIPE_SCP);
*/
    }

/* Comment out, because it included ISP */
/*
    for (uint32_t i = minFrameNum; i < INIT_SCP_BUFFERS; i++) {
        ret = generateFrame(i, &newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
            return ret;
        }
        if (newFrame == NULL) {
            CLOGE("ERR(%s[%d]):new faame is NULL", __FUNCTION__, __LINE__);
            return ret;
        }

        m_scpFrameCount++;

        m_setupEntity(PIPE_SCP, newFrame);
        m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, PIPE_SCP);
        m_previewFrameFactory->pushFrameToPipe(&newFrame, PIPE_SCP);
    }
*/

    /* prepare pipes */
    ret = m_previewFrameFactory->preparePipes();
    if (ret < 0) {
        CLOGE("ERR(%s):preparePipe fail", __FUNCTION__);
        return ret;
    }

#ifndef START_PICTURE_THREAD
    if (m_exynosCameraParameters->isReprocessing() == true) {
        m_startPictureInternal();
    }
#endif

#ifdef RAWDUMP_CAPTURE
    /* s_ctrl HAL version for selecting dvfs table */
    ret = m_previewFrameFactory->setControl(V4L2_CID_IS_HAL_VERSION, IS_HAL_VER_3_2, PIPE_3AA);
    ALOGD("WARN(%s): V4L2_CID_IS_HAL_VERSION_%d pipe(%d)", __FUNCTION__, IS_HAL_VER_3_2, PIPE_3AA);
    if (ret < 0)
        ALOGW("WARN(%s): V4L2_CID_IS_HAL_VERSION is fail", __FUNCTION__);
#endif

    /* stream on pipes */
    ret = m_previewFrameFactory->startPipes();
    if (ret < 0) {
        m_previewFrameFactory->stopPipes();
        CLOGE("ERR(%s):startPipe fail", __FUNCTION__);
        return ret;
    }

    /* start all thread */
    ret = m_previewFrameFactory->startInitialThreads();
    if (ret < 0) {
        CLOGE("ERR(%s):startInitialThreads fail", __FUNCTION__);
        return ret;
    }

    m_previewEnabled = true;
    m_exynosCameraParameters->setPreviewRunning(m_previewEnabled);

    if (m_exynosCameraParameters->getFocusModeSetting() == true) {
        CLOGD("set Focus Mode(%s[%d])", __FUNCTION__, __LINE__);
        int focusmode = m_exynosCameraParameters->getFocusMode();
        m_exynosCameraActivityControl->setAutoFocusMode(focusmode);
        m_exynosCameraParameters->setFocusModeSetting(false);
    }

#ifdef SAMSUNG_OIS
    if (m_exynosCameraParameters->getOISModeSetting() == true) {
        CLOGD("set OIS Mode(%s[%d])", __FUNCTION__, __LINE__);
        m_exynosCameraParameters->setOISMode();
        m_exynosCameraParameters->setOISModeSetting(false);
    }
#endif

    CLOGI("DEBUG(%s[%d]):OUT", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCamera::m_stopPreviewInternal(void)
{
    int ret = 0;

    CLOGI("DEBUG(%s[%d]):IN", __FUNCTION__, __LINE__);

    if (m_previewFrameFactory != NULL) {
        ret = m_previewFrameFactory->stopPipes();
        if (ret < 0) {
            CLOGE("ERR(%s):stopPipe fail", __FUNCTION__);
            return ret;
        }
    }

    CLOGD("DEBUG(%s[%d]):clear process Frame list", __FUNCTION__, __LINE__);
    ret = m_clearList(&m_processList);
    if (ret < 0) {
        CLOGE("ERR(%s):m_clearList fail", __FUNCTION__);
        return ret;
    }

    /* clear previous recording frame */
    CLOGD("DEBUG(%s[%d]):Recording m_recordingProcessList(%d) IN",
        __FUNCTION__, __LINE__, (int)m_recordingProcessList.size());
    m_recordingListLock.lock();
    ret = m_clearList(&m_recordingProcessList);
    if (ret < 0) {
        CLOGE("ERR(%s):m_clearList fail", __FUNCTION__);
    }
    m_recordingListLock.unlock();
    CLOGD("DEBUG(%s[%d]):Recording m_recordingProcessList(%d) OUT",
        __FUNCTION__, __LINE__, (int)m_recordingProcessList.size());

    m_pipeFrameDoneQ->release();

    m_fliteFrameCount = 0;
    m_3aa_ispFrameCount = 0;
    m_ispFrameCount = 0;
    m_sccFrameCount = 0;
    m_scpFrameCount = 0;
    m_isZSLCaptureOn = false;

    m_previewEnabled = false;
    m_exynosCameraParameters->setPreviewRunning(m_previewEnabled);

    CLOGI("DEBUG(%s[%d]):OUT", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCamera::m_startPictureInternal(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    int ret = 0;
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES]    = {0};
    int hwPictureW, hwPictureH;
    int planeCount  = 1;
    int minBufferCount = 1;
    int maxBufferCount = 1;
    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;
    buffer_manager_allocation_mode_t allocMode = BUFFER_MANAGER_ALLOCATION_ONDEMAND;
    ExynosCameraBufferManager *taaBufferManager[MAX_NODE];
    ExynosCameraBufferManager *ispBufferManager[MAX_NODE];
    for (int i = 0; i < MAX_NODE; i++) {
        taaBufferManager[i] = NULL;
        ispBufferManager[i] = NULL;
    }

    if (m_zslPictureEnabled == true) {
        CLOGD("DEBUG(%s[%d]): zsl picture is already initialized", __FUNCTION__, __LINE__);
        return NO_ERROR;
    }

    if (m_exynosCameraParameters->isReprocessing() == true) {
        if( m_exynosCameraParameters->getHighSpeedRecording() ) {
            m_exynosCameraParameters->getHwSensorSize(&hwPictureW, &hwPictureH);
            CLOGI("(%s):HW Picture(HighSpeed) width x height = %dx%d", __FUNCTION__, hwPictureW, hwPictureH);
        } else {
            m_exynosCameraParameters->getMaxPictureSize(&hwPictureW, &hwPictureH);
            CLOGI("(%s):HW Picture width x height = %dx%d", __FUNCTION__, hwPictureW, hwPictureH);
        }

        if (SCC_OUTPUT_COLOR_FMT == V4L2_PIX_FMT_NV21M) {
            planeSize[0] = ALIGN_UP(hwPictureW, GSCALER_IMG_ALIGN) * ALIGN_UP(hwPictureH, GSCALER_IMG_ALIGN);
            planeSize[1] = ALIGN_UP(hwPictureW, GSCALER_IMG_ALIGN) * ALIGN_UP(hwPictureH, GSCALER_IMG_ALIGN) / 2;
            planeCount  = 3;
        } else if (SCC_OUTPUT_COLOR_FMT == V4L2_PIX_FMT_NV21) {
            planeSize[0] = ALIGN_UP(hwPictureW, GSCALER_IMG_ALIGN) * ALIGN_UP(hwPictureH, GSCALER_IMG_ALIGN) * 3 / 2;
            planeCount  = 2;
        } else {
            planeSize[0] = ALIGN_UP(hwPictureW, GSCALER_IMG_ALIGN) * ALIGN_UP(hwPictureH, GSCALER_IMG_ALIGN) * 2;
            planeCount  = 2;
        }
        minBufferCount = 1;
        maxBufferCount = NUM_PICTURE_BUFFERS;

        if (m_exynosCameraParameters->getHighResolutionCallbackMode() == true) {
            /* SCC Reprocessing Buffer realloc for high resolution callback */
            minBufferCount = 2;
        }

        ret = m_allocBuffers(m_sccReprocessingBufferMgr, planeCount, planeSize, bytesPerLine, minBufferCount, maxBufferCount, type, allocMode, true, false);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_sccReprocessingBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
                __FUNCTION__, __LINE__, minBufferCount, maxBufferCount);
            return ret;
        }

        if (m_exynosCameraParameters->getUsePureBayerReprocessing() == true) {
            ret = m_setReprocessingBuffer();
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):m_setReprocessing Buffer fail", __FUNCTION__, __LINE__);
                return ret;
            }
        }
    }

    if (m_reprocessingFrameFactory->isCreated() == false) {
        ret = m_reprocessingFrameFactory->create();
        if (ret < 0) {
            CLOGE("ERR(%s):m_reprocessingFrameFactory->create() failed", __FUNCTION__);
            return ret;
        }

        m_pictureFrameFactory = m_reprocessingFrameFactory;
        CLOGD("DEBUG(%s[%d]):FrameFactory(pictureFrameFactory) created", __FUNCTION__, __LINE__);
    }

    /* If we want set buffer namanger from m_getBufferManager, use this */
#if 0
    ret = m_getBufferManager(PIPE_3AA_REPROCESSING, bufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_3AA_REPROCESSING)], SRC_BUFFER_DIRECTION);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getBufferManager() fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, PIPE_3AA_REPROCESSING, ret);
        return ret;
    }

    ret = m_getBufferManager(PIPE_3AA_REPROCESSING, bufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_3AP_REPROCESSING)], DST_BUFFER_DIRECTION);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getBufferManager() fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, PIPE_3AP_REPROCESSING, ret);
        return ret;
    }
#else
    if (m_exynosCameraParameters->isReprocessing3aaIspOTF() == false) {
        taaBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_3AA_REPROCESSING)] = m_bayerBufferMgr;
        taaBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_3AP_REPROCESSING)] = m_ispReprocessingBufferMgr;
    } else {
        taaBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_3AA_REPROCESSING)] = m_bayerBufferMgr;
        taaBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_ISPC_REPROCESSING)] = m_sccReprocessingBufferMgr;

        taaBufferManager[OUTPUT_NODE] = m_bayerBufferMgr;
        taaBufferManager[CAPTURE_NODE] = m_sccReprocessingBufferMgr;
    }
#endif

    ret = m_reprocessingFrameFactory->setBufferManagerToPipe(taaBufferManager, PIPE_3AA_REPROCESSING);
    if (ret < 0) {
        CLOGE("ERR(%s):m_reprocessingFrameFactory->setBufferManagerToPipe() failed", __FUNCTION__);
        return ret;
    }

    ispBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_ISP_REPROCESSING)] = m_ispReprocessingBufferMgr;
    ispBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_ISPC_REPROCESSING)] = m_sccReprocessingBufferMgr;

    ret = m_reprocessingFrameFactory->setBufferManagerToPipe(ispBufferManager, PIPE_ISP_REPROCESSING);
    if (ret < 0) {
        CLOGE("ERR(%s):m_reprocessingFrameFactory->setBufferManagerToPipe() failed", __FUNCTION__);
        return ret;
    }

    ret = m_reprocessingFrameFactory->initPipes();
    if (ret < 0) {
        CLOGE("ERR(%s):m_reprocessingFrameFactory->initPipes() failed", __FUNCTION__);
        return ret;
    }

    ret = m_reprocessingFrameFactory->preparePipes();
    if (ret < 0) {
        CLOGE("ERR(%s):m_reprocessingFrameFactory preparePipe fail", __FUNCTION__);
        return ret;
    }

    /* stream on pipes */
    ret = m_reprocessingFrameFactory->startPipes();
    if (ret < 0) {
        CLOGE("ERR(%s):m_reprocessingFrameFactory startPipe fail", __FUNCTION__);
        return ret;
    }

    m_zslPictureEnabled = true;

    /*
     * Make remained frameFactory here.
     * in case of reprocessing capture, make here.
     */
    m_framefactoryThread->run();

    return NO_ERROR;
}

status_t ExynosCamera::m_stopPictureInternal(void)
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);
    int ret = 0;

    m_prePictureThread->join();
    m_pictureThread->join();

#ifdef SAMSUNG_LLS_DEBLUR
    m_LDCaptureThread->join();
#endif

#ifdef SAMSUNG_DEBLUR
    m_detectDeblurCaptureThread->join();
    m_deblurCaptureThread->join();
#endif

    m_postPictureGscThread->join();

    m_ThumbnailCallbackThread->join();

    m_postPictureThread->join();
    m_jpegCallbackThread->join();

    for (int threadNum = JPEG_SAVE_THREAD0; threadNum < JPEG_SAVE_THREAD_MAX_COUNT; threadNum++) {
        m_jpegSaveThread[threadNum]->join();
    }

    if (m_zslPictureEnabled == true) {
        ret = m_reprocessingFrameFactory->stopPipes();
        if (ret < 0) {
            CLOGE("ERR(%s):m_reprocessingFrameFactory0>stopPipe() fail", __FUNCTION__);
        }
    }

    if (m_exynosCameraParameters->getHighResolutionCallbackMode() == true) {
        m_highResolutionCallbackThread->stop();
        if (m_highResolutionCallbackQ != NULL)
            m_highResolutionCallbackQ->sendCmd(WAKE_UP);
        m_highResolutionCallbackThread->requestExitAndWait();
    }

    /* Clear frames & buffers which remain in capture processingQ */
    m_clearFrameQ(dstSccReprocessingQ, PIPE_SCC, DST_BUFFER_DIRECTION);
    m_clearFrameQ(m_postPictureQ, PIPE_SCC, DST_BUFFER_DIRECTION);
    m_clearFrameQ(dstJpegReprocessingQ, PIPE_JPEG, SRC_BUFFER_DIRECTION);

    dstIspReprocessingQ->release();
    dstGscReprocessingQ->release();

    dstPostPictureGscQ->release();

    m_ThumbnailPostCallbackQ->release();

    dstJpegReprocessingQ->release();

    m_jpegCallbackQ->release();

    m_postviewCallbackQ->release();

    m_thumbnailCallbackQ->release();

#ifdef SAMSUNG_DNG
    m_dngCaptureQ->release();
#endif

#ifdef SAMSUNG_BD
    m_BDbufferQ->release();

    if(m_BDpluginHandle != NULL && m_BDstatus == BLUR_DETECTION_INIT) {
        int ret = uni_plugin_deinit(m_BDpluginHandle);
        if(ret < 0) {
            CLOGE("[BD](%s[%d]): Plugin deinit failed!!", __FUNCTION__, __LINE__);
        }
        for(int i = 0; i < MAX_BD_BUFF_NUM; i++) {
            if(m_BDbuffer[i].data != NULL){
                delete []m_BDbuffer[i].data;
            }
        }
    }
    m_BDstatus = BLUR_DETECTION_DEINIT;
#endif
#ifdef SAMSUNG_DEBLUR
    m_detectDeblurCaptureQ->release();
#endif

    for (int threadNum = JPEG_SAVE_THREAD0; threadNum < JPEG_SAVE_THREAD_MAX_COUNT; threadNum++) {
        m_jpegSaveQ[threadNum]->release();
    }

    if (m_highResolutionCallbackQ->getSizeOfProcessQ() != 0){
        CLOGD("DEBUG(%s[%d]):m_highResolutionCallbackQ->getSizeOfProcessQ(%d). release the highResolutionCallbackQ.",
            __FUNCTION__, __LINE__, m_highResolutionCallbackQ->getSizeOfProcessQ());
        m_highResolutionCallbackQ->release();
    }

    /*
     * HACK :
     * Just sleep for
     * all picture-related thread(having m_postProcessList) is over.
     *  if not :
     *    m_clearList will delete frames.
     *    and then, the internal mutex of other thead's deleted frame
     *    will sleep forever (pThread's tech report)
     *  to remove this hack :
     *    stopPreview()'s burstPanorama-related sequence.
     *    stop all pipe -> wait all thread. -> clear all frameQ.
     */
    usleep(5000);

    CLOGD("DEBUG(%s[%d]):clear postProcess(Picture) Frame list", __FUNCTION__, __LINE__);

    ret = m_clearList(&m_postProcessList);
    if (ret < 0) {
        CLOGE("ERR(%s):m_clearList fail", __FUNCTION__);
        return ret;
    }

    m_zslPictureEnabled = false;

    /* TODO: need timeout */
    return NO_ERROR;
}

status_t ExynosCamera::m_startRecordingInternal(void)
{
    int ret = 0;

    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES]    = {0};
    int videoW = 0, videoH = 0;
    int planeCount  = 1;
    int minBufferCount = 1;
    int maxBufferCount = 1;
    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
    buffer_manager_allocation_mode_t allocMode = BUFFER_MANAGER_ALLOCATION_SILENT;
    int heapFd = 0;

#ifdef SAMSUNG_LLV
    if (m_exynosCameraParameters->getLLV() == true
#ifdef USE_LIVE_BROADCAST
        && m_exynosCameraParameters->getPLBMode() == false
#endif
    ) {
        if(m_LLVpluginHandle != NULL){
            int HwPreviewW = 0, HwPreviewH = 0;
            m_exynosCameraParameters->getHwPreviewSize(&HwPreviewW, &HwPreviewH);

            UniPluginBufferData_t pluginData;
            UniPluginCameraInfo_t pluginCameraInfo;
            pluginCameraInfo.CameraType = (UNI_PLUGIN_CAMERA_TYPE)getCameraId();
            pluginCameraInfo.SensorType = (UNI_PLUGIN_SENSOR_TYPE)m_cameraSensorId;
            memset(&pluginData, 0, sizeof(UniPluginBufferData_t));
            pluginData.InWidth= HwPreviewW;
            pluginData.InHeight= HwPreviewH;
            if(m_LLVpluginHandle != NULL) {
                uni_plugin_set(m_LLVpluginHandle,
                    LLV_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &pluginData);
            }
            int powerCtrl = UNI_PLUGIN_POWER_CONTROL_4;
            if(m_LLVpluginHandle != NULL) {
                uni_plugin_set(m_LLVpluginHandle,
                    LLV_PLUGIN_NAME, UNI_PLUGIN_INDEX_POWER_CONTROL, &powerCtrl);
                uni_plugin_set(m_LLVpluginHandle,
                    LLV_PLUGIN_NAME, UNI_PLUGIN_INDEX_CAMERA_INFO, &pluginCameraInfo);
            }
            uni_plugin_init(m_LLVpluginHandle);

            m_LLVstatus = LLV_INIT;
        }
    }

#endif

    m_exynosCameraParameters->getVideoSize(&videoW, &videoH);
    CLOGD("DEBUG(%s[%d]):videoSize = %d x %d",  __FUNCTION__, __LINE__, videoW, videoH);

    m_doCscRecording = true;
    if (m_exynosCameraParameters->doCscRecording() == true) {
        m_recordingBufferCount = m_exynosconfig->current->bufInfo.num_recording_buffers;
        CLOGI("INFO(%s[%d]):do Recording CSC !!! m_recordingBufferCount(%d)", __FUNCTION__, __LINE__, m_recordingBufferCount);
    } else {
        m_doCscRecording = false;
        m_recordingBufferCount = m_exynosconfig->current->bufInfo.num_preview_buffers;
        CLOGI("INFO(%s[%d]):skip Recording CSC !!! m_recordingBufferCount(%d->%d)",
            __FUNCTION__, __LINE__, m_exynosconfig->current->bufInfo.num_recording_buffers, m_recordingBufferCount);
    }

    /* clear previous recording frame */
    CLOGD("DEBUG(%s[%d]):Recording m_recordingProcessList(%d) IN",
        __FUNCTION__, __LINE__, (int)m_recordingProcessList.size());
    m_recordingListLock.lock();
    ret = m_clearList(&m_recordingProcessList);
    if (ret < 0) {
        CLOGE("ERR(%s):m_clearList fail", __FUNCTION__);
    }
    m_recordingListLock.unlock();
    CLOGD("DEBUG(%s[%d]):Recording m_recordingProcessList(%d) OUT",
        __FUNCTION__, __LINE__, (int)m_recordingProcessList.size());

    for (int32_t i = 0; i < m_recordingBufferCount; i++) {
        m_recordingTimeStamp[i] = 0L;
        m_recordingBufAvailable[i] = true;
    }

    /* alloc recording Callback Heap */
    m_recordingCallbackHeap = m_getMemoryCb(-1, sizeof(VideoNativeHandleMetadata), m_recordingBufferCount, &heapFd);
    if (!m_recordingCallbackHeap || m_recordingCallbackHeap->data == MAP_FAILED) {
        CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, (int)sizeof(struct addrs));
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (m_doCscRecording == true) {
        /* alloc recording Image buffer */
        planeSize[0] = ROUND_UP(videoW, CAMERA_MAGIC_ALIGN) * ROUND_UP(videoH, CAMERA_MAGIC_ALIGN) + MFC_7X_BUFFER_OFFSET;
        planeSize[1] = ROUND_UP(videoW, CAMERA_MAGIC_ALIGN) * ROUND_UP(videoH / 2, CAMERA_MAGIC_ALIGN) + MFC_7X_BUFFER_OFFSET;
        planeCount   = 2;
        if( m_exynosCameraParameters->getHighSpeedRecording() == true)
            minBufferCount = m_recordingBufferCount;
        else
            minBufferCount = 1;

        maxBufferCount = m_recordingBufferCount;

        ret = m_allocBuffers(m_recordingBufferMgr, planeCount, planeSize, bytesPerLine, minBufferCount, maxBufferCount, type, allocMode, false, true);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_recordingBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
                __FUNCTION__, __LINE__, minBufferCount, maxBufferCount);
        }

    }

    if (m_doCscRecording == true) {
        int recPipeId = PIPE_GSC_VIDEO;

        m_previewFrameFactory->startThread(recPipeId);

        if (m_recordingQ->getSizeOfProcessQ() > 0) {
            CLOGE("ERR(%s[%d]):m_startRecordingInternal recordingQ(%d)", __FUNCTION__, __LINE__, m_recordingQ->getSizeOfProcessQ());
            /*
             * just empty q on m_recordingQ.
             * m_clearList() can make deadlock by accessing frame
             * deleted on m_stopRecordingInternal()
             */
            /* m_clearList(m_recordingQ); */
            m_recordingQ->release();
        }

        m_recordingThread->run();
    }

func_exit:

    return ret;
}

status_t ExynosCamera::m_stopRecordingInternal(void)
{
    int ret = 0;
    if (m_doCscRecording == true) {
        int recPipeId = PIPE_GSC_VIDEO;

        {
            Mutex::Autolock lock(m_recordingStopLock);
            m_previewFrameFactory->stopPipe(recPipeId);
        }

        m_recordingQ->sendCmd(WAKE_UP);

        m_recordingThread->requestExitAndWait();
        m_recordingQ->release();
        m_recordingBufferMgr->deinit();
    } else {
        CLOGI("INFO(%s[%d]):reset m_recordingBufferCount(%d->%d)",
            __FUNCTION__, __LINE__, m_recordingBufferCount, m_exynosconfig->current->bufInfo.num_recording_buffers);
        m_recordingBufferCount = m_exynosconfig->current->bufInfo.num_recording_buffers;
    }

    /* Checking all frame(buffer) released from Media recorder */
    int sleepUsecs = 33300;
    int retryCount = 3;     /* 33.3ms*3 = 100ms */
    bool allBufferReleased = true;

    for (int i = 0; i < retryCount; i++) {
        allBufferReleased = true;

        for (int bufferIndex = 0; bufferIndex < m_recordingBufferCount; bufferIndex++) {
            if (m_recordingBufAvailable[bufferIndex] == false) {
                CLOGW("WRN(%s[%d]):Media recorder doesn't release frame(buffer), index(%d)",
                        __FUNCTION__, __LINE__, bufferIndex);
                allBufferReleased = false;
            }
        }

        if(allBufferReleased == true) {
            break;
        }

        usleep(sleepUsecs);
    }

    if (allBufferReleased == false) {
        CLOGE("ERR(%s[%d]):Media recorder doesn't release frame(buffer) all!!", __FUNCTION__, __LINE__);
    }

    if (m_recordingCallbackHeap != NULL) {
        m_recordingCallbackHeap->release(m_recordingCallbackHeap);
        m_recordingCallbackHeap = NULL;
    }

#ifdef SAMSUNG_LLV
    if (m_LLVstatus == LLV_INIT) {
        m_LLVstatus = LLV_STOPPED;
    }
#endif

    return NO_ERROR;
}

status_t ExynosCamera::m_restartPreviewInternal(void)
{
    CLOGI("INFO(%s[%d]): Internal restart preview", __FUNCTION__, __LINE__);
    int ret = 0;
    int err = 0;

    m_flagThreadStop = true;
    m_disablePreviewCB = true;

    m_startPictureInternalThread->join();

    /* release about frameFactory */
    m_framefactoryThread->stop();
    m_frameFactoryQ->sendCmd(WAKE_UP);
    m_framefactoryThread->requestExitAndWait();
    m_frameFactoryQ->release();

    m_startPictureBufferThread->join();

    if (m_previewFrameFactory != NULL)
        m_previewFrameFactory->setStopFlag();

    m_mainThread->requestExitAndWait();

    ret = m_stopPictureInternal();
    if (ret < 0)
        CLOGE("ERR(%s[%d]):m_stopPictureInternal fail", __FUNCTION__, __LINE__);

    m_zoomPreviwWithCscThread->stop();
    m_zoomPreviwWithCscQ->sendCmd(WAKE_UP);
    m_zoomPreviwWithCscThread->requestExitAndWait();

    m_previewThread->stop();
    if(m_previewQ != NULL)
        m_previewQ->sendCmd(WAKE_UP);
    m_previewThread->requestExitAndWait();

    if (m_exynosCameraParameters->isFlite3aaOtf() == true) {
        m_mainSetupQThread[INDEX(PIPE_FLITE)]->stop();
        m_mainSetupQ[INDEX(PIPE_FLITE)]->sendCmd(WAKE_UP);
        m_mainSetupQThread[INDEX(PIPE_FLITE)]->requestExitAndWait();

        if (m_mainSetupQThread[INDEX(PIPE_3AC)] != NULL) {
            m_mainSetupQThread[INDEX(PIPE_3AC)]->stop();
            m_mainSetupQ[INDEX(PIPE_3AC)]->sendCmd(WAKE_UP);
            m_mainSetupQThread[INDEX(PIPE_3AC)]->requestExitAndWait();
        }

        m_mainSetupQThread[INDEX(PIPE_3AA)]->stop();
        m_mainSetupQ[INDEX(PIPE_3AA)]->sendCmd(WAKE_UP);
        m_mainSetupQThread[INDEX(PIPE_3AA)]->requestExitAndWait();

       if (m_mainSetupQThread[INDEX(PIPE_ISP)] != NULL) {
            m_mainSetupQThread[INDEX(PIPE_ISP)]->stop();
            m_mainSetupQ[INDEX(PIPE_ISP)]->sendCmd(WAKE_UP);
            m_mainSetupQThread[INDEX(PIPE_ISP)]->requestExitAndWait();
        }

        /* Comment out, because it included ISP */
        /*
        m_mainSetupQThread[INDEX(PIPE_SCP)]->stop();
        m_mainSetupQ[INDEX(PIPE_SCP)]->sendCmd(WAKE_UP);
        m_mainSetupQThread[INDEX(PIPE_SCP)]->requestExitAndWait();
        */

        m_clearList(m_mainSetupQ[INDEX(PIPE_FLITE)]);
        m_clearList(m_mainSetupQ[INDEX(PIPE_3AA)]);
        m_clearList(m_mainSetupQ[INDEX(PIPE_ISP)]);
        /* Comment out, because it included ISP */
        /* m_clearList(m_mainSetupQ[INDEX(PIPE_SCP)]); */

        m_mainSetupQ[INDEX(PIPE_FLITE)]->release();
        m_mainSetupQ[INDEX(PIPE_3AA)]->release();
        m_mainSetupQ[INDEX(PIPE_ISP)]->release();
        /* Comment out, because it included ISP */
        /* m_mainSetupQ[INDEX(PIPE_SCP)]->release(); */
    } else {
        if (m_mainSetupQThread[INDEX(PIPE_FLITE)] != NULL) {
            m_mainSetupQThread[INDEX(PIPE_FLITE)]->stop();
            m_mainSetupQ[INDEX(PIPE_FLITE)]->sendCmd(WAKE_UP);
            m_mainSetupQThread[INDEX(PIPE_FLITE)]->requestExitAndWait();
            m_clearList(m_mainSetupQ[INDEX(PIPE_FLITE)]);
            m_mainSetupQ[INDEX(PIPE_FLITE)]->release();
        }
    }

    ret = m_stopPreviewInternal();
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_stopPreviewInternal fail", __FUNCTION__, __LINE__);
        err = ret;
    }

#ifdef SUPPORT_SW_VDIS
    if(m_swVDIS_Mode) {
        if (m_swVDIS_BufferMgr != NULL)
            m_swVDIS_BufferMgr->deinit();

        if(m_exynosCameraParameters->increaseMaxBufferOfPreview()) {
            m_exynosCameraParameters->setPreviewBufferCount(NUM_PREVIEW_BUFFERS + NUM_PREVIEW_SPARE_BUFFERS);
            m_exynosconfig->current->bufInfo.num_preview_buffers = NUM_PREVIEW_BUFFERS + NUM_PREVIEW_SPARE_BUFFERS;
            VDIS_LOG("VDIS_HAL: Preview Buffer Count Change to %d", NUM_PREVIEW_BUFFERS + NUM_PREVIEW_SPARE_BUFFERS);
        } else {
            m_exynosCameraParameters->setPreviewBufferCount(NUM_PREVIEW_BUFFERS);
            m_exynosconfig->current->bufInfo.num_preview_buffers = NUM_PREVIEW_BUFFERS;
            VDIS_LOG("VDIS_HAL: Preview Buffer Count Change to %d", NUM_PREVIEW_BUFFERS);
        }
    }
#endif /*SUPPORT_SW_VDIS*/

    /* skip to free and reallocate buffers */
    if (m_bayerBufferMgr != NULL) {
        m_bayerBufferMgr->resetBuffers();
    }
#ifdef SAMSUNG_DNG
    if (m_fliteBufferMgr != NULL) {
        m_fliteBufferMgr->resetBuffers();
    }
#endif
    if (m_3aaBufferMgr != NULL) {
        m_3aaBufferMgr->resetBuffers();
    }
    if (m_ispBufferMgr != NULL) {
        m_ispBufferMgr->resetBuffers();
    }
    if (m_hwDisBufferMgr != NULL) {
        m_hwDisBufferMgr->resetBuffers();
    }
    if (m_sccBufferMgr != NULL) {
        m_sccBufferMgr->resetBuffers();
    }

    if (m_highResolutionCallbackBufferMgr != NULL) {
        m_highResolutionCallbackBufferMgr->resetBuffers();
    }

    /* skip to free and reallocate buffers */
    if (m_ispReprocessingBufferMgr != NULL) {
        m_ispReprocessingBufferMgr->resetBuffers();
    }
    if (m_sccReprocessingBufferMgr != NULL) {
        m_sccReprocessingBufferMgr->resetBuffers();
    }

    if (m_gscBufferMgr != NULL) {
        m_gscBufferMgr->resetBuffers();
    }
    if (m_jpegBufferMgr != NULL) {
        m_jpegBufferMgr->resetBuffers();
    }
    if (m_recordingBufferMgr != NULL) {
        m_recordingBufferMgr->resetBuffers();
    }

    /* realloc callback buffers */
    if (m_scpBufferMgr != NULL) {
        m_scpBufferMgr->deinit();
        m_scpBufferMgr->setBufferCount(0);
    }
    if (m_previewCallbackBufferMgr != NULL) {
        m_previewCallbackBufferMgr->deinit();
    }
    if (m_zoomScalerBufferMgr != NULL) {
        m_zoomScalerBufferMgr->deinit();
    }

    if (m_captureSelector != NULL) {
        m_captureSelector->release();
    }
    if (m_sccCaptureSelector != NULL) {
        m_sccCaptureSelector->release();
    }

    if( m_exynosCameraParameters->getHighSpeedRecording() && m_exynosCameraParameters->getReallocBuffer() ) {
        CLOGD("DEBUG(%s): realloc buffer all buffer deinit ", __FUNCTION__);
        if (m_bayerBufferMgr != NULL) {
            m_bayerBufferMgr->deinit();
        }
        if (m_3aaBufferMgr != NULL) {
            m_3aaBufferMgr->deinit();
        }
        if (m_ispBufferMgr != NULL) {
            m_ispBufferMgr->deinit();
        }

        if (m_sccBufferMgr != NULL) {
            m_sccBufferMgr->deinit();
        }
/*
        if (m_highResolutionCallbackBufferMgr != NULL) {
            m_highResolutionCallbackBufferMgr->deinit();
        }
*/
        if (m_gscBufferMgr != NULL) {
            m_gscBufferMgr->deinit();
        }
        if (m_jpegBufferMgr != NULL) {
            m_jpegBufferMgr->deinit();
        }
        if (m_recordingBufferMgr != NULL) {
            m_recordingBufferMgr->deinit();
        }
    }

    if (m_exynosCameraParameters->getTpuEnabledMode() == true) {
        if (m_hwDisBufferMgr != NULL) {
            m_hwDisBufferMgr->deinit();
        }
    }

    m_flagThreadStop = false;

    ret = setPreviewWindow(m_previewWindow);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setPreviewWindow fail", __FUNCTION__, __LINE__);
        err = ret;
    }

    CLOGI("INFO(%s[%d]):setBuffersThread is run", __FUNCTION__, __LINE__);
    m_setBuffersThread->run(PRIORITY_DEFAULT);
    m_setBuffersThread->join();

    if (m_isSuccessedBufferAllocation == false) {
        CLOGE("ERR(%s[%d]):m_setBuffersThread() failed", __FUNCTION__, __LINE__);
        err = INVALID_OPERATION;
    }

#ifdef START_PICTURE_THREAD
    m_startPictureInternalThread->join();
#endif
    m_startPictureBufferThread->join();

    if (m_exynosCameraParameters->isReprocessing() == true) {
#ifdef START_PICTURE_THREAD
        m_startPictureInternalThread->run(PRIORITY_DEFAULT);
#endif
    } else {
        m_pictureFrameFactory = m_previewFrameFactory;
        CLOGD("DEBUG(%s[%d]):FrameFactory(pictureFrameFactory) created", __FUNCTION__, __LINE__);

        /*
         * Make remained frameFactory here.
         * in case of SCC capture, make here.
         */
        m_framefactoryThread->run();
    }

    ret = m_startPreviewInternal();
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_startPreviewInternal fail", __FUNCTION__, __LINE__);
        err = ret;
    }

    /* setup frame thread */
    if (m_exynosCameraParameters->getDualMode() == true && getCameraId() == CAMERA_ID_FRONT) {
        CLOGD("DEBUG(%s[%d]):setupThread Thread start pipeId(%d)", __FUNCTION__, __LINE__, PIPE_FLITE);
        m_mainSetupQThread[INDEX(PIPE_FLITE)]->run(PRIORITY_URGENT_DISPLAY);
    } else {
        switch (m_exynosCameraParameters->getReprocessingBayerMode()) {
        case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON:
            m_mainSetupQ[INDEX(PIPE_FLITE)]->setup(NULL);
            CLOGD("DEBUG(%s[%d]):setupThread Thread start pipeId(%d)", __FUNCTION__, __LINE__, PIPE_FLITE);
            m_mainSetupQThread[INDEX(PIPE_FLITE)]->run(PRIORITY_URGENT_DISPLAY);
            break;
        case REPROCESSING_BAYER_MODE_PURE_DYNAMIC:
            CLOGD("DEBUG(%s[%d]):setupThread with List pipeId(%d)", __FUNCTION__, __LINE__, PIPE_FLITE);
            m_mainSetupQ[INDEX(PIPE_FLITE)]->setup(m_mainSetupQThread[INDEX(PIPE_FLITE)]);
            break;
        default:
            CLOGI("INFO(%s[%d]):setupThread not started pipeID(%d)", __FUNCTION__, __LINE__, PIPE_FLITE);
            break;
        }
        CLOGD("DEBUG(%s[%d]):setupThread Thread start pipeId(%d)", __FUNCTION__, __LINE__, PIPE_3AA);
        m_mainSetupQThread[INDEX(PIPE_3AA)]->run(PRIORITY_URGENT_DISPLAY);
    }

#ifdef SAMSUNG_DNG
    if (m_exynosCameraParameters->getDNGCaptureModeOn()) {
        CLOGD("[DNG](%s[%d]):setupThread with List pipeId(%d)", __FUNCTION__, __LINE__, PIPE_FLITE);
        m_mainSetupQ[INDEX(PIPE_FLITE)]->setup(m_mainSetupQThread[INDEX(PIPE_FLITE)]);
    }
#endif

    if (m_facedetectThread->isRunning() == false)
        m_facedetectThread->run();

    m_disablePreviewCB = false;
    m_previewThread->run(PRIORITY_DISPLAY);

    m_mainThread->run(PRIORITY_DEFAULT);
    m_startPictureInternalThread->join();

    if (m_exynosCameraParameters->getZoomPreviewWIthScaler() == true) {
        CLOGD("DEBUG(%s[%d]):ZoomPreview with Scaler Thread start", __FUNCTION__, __LINE__);
        m_zoomPreviwWithCscThread->run(PRIORITY_DEFAULT);
    }


    return err;
}

bool ExynosCamera::m_mainThreadQSetup3AA()
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif

    int  ret  = 0;
    bool loop = true;
    int  pipeId_3AA    = PIPE_3AA;
    int  pipeId_3AC    = PIPE_3AC;
    int  pipeId_ISP    = PIPE_ISP;
    int  pipeId_DIS    = PIPE_DIS;
    int  pipeIdCsc = 0;
    int  maxbuffers   = 0;

    ExynosCameraBuffer buffer;
    ExynosCameraFrame  *frame = NULL;
    ExynosCameraFrame  *newframe = NULL;
    nsecs_t timeStamp = 0;
    int frameCount = -1;

    CLOGV("INFO(%s[%d]):wait previewCancelQ", __FUNCTION__, __LINE__);
    ret = m_mainSetupQ[INDEX(pipeId_3AA)]->waitAndPopProcessQ(&frame);
    if (m_flagThreadStop == true) {
        CLOGI("INFO(%s[%d]):m_flagThreadStop(%d)", __FUNCTION__, __LINE__, m_flagThreadStop);
        goto func_exit;
    }

    if (ret < 0) {
        if (ret == TIMED_OUT) {
            CLOGW("WARN(%s):wait timeout", __FUNCTION__);
        } else {
            CLOGE("ERR(%s):wait and pop fail, ret(%d)", __FUNCTION__, ret);
            /* TODO: doing exception handling */
        }
        goto func_exit;
    }

    if (frame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        goto func_exit;
    }

    ret = generateFrame(m_3aa_ispFrameCount, &newframe);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
        goto func_exit;
    }

    if (newframe == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        goto func_exit;
    }

    ret = m_setupEntity(pipeId_3AA, newframe);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setupEntity fail", __FUNCTION__, __LINE__);
    }

    if (m_exynosCameraParameters->is3aaIspOtf() == true) {
        m_previewFrameFactory->setFrameDoneQToPipe(m_pipeFrameDoneQ, pipeId_3AA);

        if (m_exynosCameraParameters->getTpuEnabledMode() == true)
            m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId_DIS);
    } else {
        m_previewFrameFactory->setFrameDoneQToPipe(m_pipeFrameDoneQ, pipeId_3AA);

        if (m_exynosCameraParameters->getTpuEnabledMode() == true) {
            m_previewFrameFactory->setFrameDoneQToPipe(m_pipeFrameDoneQ, pipeId_ISP);
            m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId_DIS);
        } else {
            m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId_ISP);
        }
    }

    m_previewFrameFactory->pushFrameToPipe(&newframe, pipeId_3AA);

    m_3aa_ispFrameCount++;

func_exit:
    if( frame != NULL ) {
        frame->decRef();
        m_frameMgr->deleteFrame(frame);;
        frame = NULL;
    }

    return loop;
}

bool ExynosCamera::m_mainThreadQSetup3AA_ISP()
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif

    int  ret  = 0;
    bool loop = true;
    int  pipeId    = PIPE_3AA_ISP;
    int  pipeIdCsc = 0;
    int  maxbuffers   = 0;

    ExynosCameraBuffer buffer;
    ExynosCameraFrame  *frame = NULL;
    ExynosCameraFrame  *newframe = NULL;
    nsecs_t timeStamp = 0;
    int frameCount = -1;

    CLOGV("INFO(%s[%d]):wait previewCancelQ", __FUNCTION__, __LINE__);
    ret = m_mainSetupQ[INDEX(pipeId)]->waitAndPopProcessQ(&frame);
    if (m_flagThreadStop == true) {
        CLOGI("INFO(%s[%d]):m_flagThreadStop(%d)", __FUNCTION__, __LINE__, m_flagThreadStop);
        goto func_exit;
    }
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        goto func_exit;
    }

    if (frame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        goto func_exit;
    }

    while (m_3aaBufferMgr->getNumOfAvailableBuffer() > 0 &&
            m_ispBufferMgr->getNumOfAvailableBuffer() > 0) {
        ret = generateFrame(m_3aa_ispFrameCount, &newframe);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
            goto func_exit;
        }

        if (newframe == NULL) {
            CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
            goto func_exit;
        }

        ret = m_setupEntity(pipeId, newframe);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):setupEntity fail", __FUNCTION__, __LINE__);
        }

        m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId);
        m_previewFrameFactory->pushFrameToPipe(&newframe, pipeId);

        m_3aa_ispFrameCount++;

    }

func_exit:
    if( frame != NULL ) {
        frame->decRef();
        m_frameMgr->deleteFrame(frame);;
        frame = NULL;
    }

    /*
    if (m_mainSetupQ[INDEX(pipeId)]->getSizeOfProcessQ() <= 0)
        loop = false;
    */

    return loop;
}

bool ExynosCamera::m_mainThreadQSetupISP()
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif

    int  ret  = 0;
    bool loop = false;
    int  pipeId = PIPE_ISP;

    ExynosCameraBuffer buffer;
    ExynosCameraFrame  *frame = NULL;
    nsecs_t timeStamp = 0;
    int frameCount = -1;

    CLOGV("INFO(%s[%d]):wait previewCancelQ", __FUNCTION__, __LINE__);
    ret = m_mainSetupQ[INDEX(pipeId)]->waitAndPopProcessQ(&frame);
    if (m_flagThreadStop == true) {
        CLOGI("INFO(%s[%d]):m_flagThreadStop(%d)", __FUNCTION__, __LINE__, m_flagThreadStop);
        goto func_exit;
    }
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        goto func_exit;
    }

    if (frame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        goto func_exit;
    }

func_exit:
    if( frame != NULL ) {
        frame->decRef();
        m_frameMgr->deleteFrame(frame);
        frame = NULL;
    }

    if (m_mainSetupQ[INDEX(pipeId)]->getSizeOfProcessQ() > 0)
        loop = true;

    return loop;
}

bool ExynosCamera::m_mainThreadQSetupFLITE()
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif

    int  ret  = 0;
    bool loop = true;
    int  pipeId    = PIPE_FLITE;
    int  pipeIdCsc = 0;
    int  maxbuffers   = 0;

    ExynosCameraBuffer buffer;
    ExynosCameraFrame  *frame = NULL;
    ExynosCameraFrame  *newframe = NULL;
    nsecs_t timeStamp = 0;
    int frameCount = -1;
    ExynosCameraBufferManager *bufMgr;
#ifdef SAMSUNG_DNG
    int dstbufferIndex = 0;

    buffer.index = -2;
    dstbufferIndex = -1;

    if (m_exynosCameraParameters->getDNGCaptureModeOn()) {
        loop = false;
    }
#endif

    CLOGV("INFO(%s[%d]):wait previewCancelQ", __FUNCTION__, __LINE__);
    ret = m_mainSetupQ[INDEX(pipeId)]->waitAndPopProcessQ(&frame);
    if (m_flagThreadStop == true) {
        CLOGI("INFO(%s[%d]):m_flagThreadStop(%d)", __FUNCTION__, __LINE__, m_flagThreadStop);
        goto func_exit;
    }
    if (ret < 0) {
        if (ret == TIMED_OUT) {
            CLOGW("WARN(%s):wait timeout", __FUNCTION__);
        } else {
            CLOGE("ERR(%s):wait and pop fail, ret(%d)", __FUNCTION__, ret);
            /* TODO: doing exception handling */
        }
        goto func_exit;
    }

    if (frame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        goto func_exit;
    }

#ifdef SAMSUNG_DNG
    if (m_exynosCameraParameters->getDNGCaptureModeOn()) {
        bufMgr = m_fliteBufferMgr;
    } else
#endif
    {
        bufMgr = m_bayerBufferMgr;
    }

    if (bufMgr->getNumOfAvailableBuffer() > 0) {
        ret = generateFrame(m_fliteFrameCount, &newframe);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
            goto func_exit;
        }

#ifdef SAMSUNG_DNG
        if (m_exynosCameraParameters->getDNGCaptureModeOn()) {
            m_fliteBufferMgr->getBuffer(&dstbufferIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &buffer);
            ret = m_setupEntity(pipeId, newframe, NULL, &buffer);
            CLOGV("[DNG](%s[%d]): m_fliteFrameCount(%d) index(%d)", __FUNCTION__, __LINE__, m_fliteFrameCount,buffer.index);
        } else
#endif
        {
            ret = m_setupEntity(pipeId, newframe);
        }

        if (ret < 0) {
            CLOGE("ERR(%s[%d]):setupEntity fail", __FUNCTION__, __LINE__);
        }
        ret = newframe->getDstBuffer(pipeId, &buffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getDstBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }

        m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId);
        m_previewFrameFactory->pushFrameToPipe(&newframe, pipeId);

        m_fliteFrameCount++;
    }

func_exit:
    if( frame != NULL ) {
        frame->decRef();
        m_frameMgr->deleteFrame(frame);;
        frame = NULL;
    }

    /*
    if (m_mainSetupQ[INDEX(pipeId)]->getSizeOfProcessQ() <= 0)
        loop = false;
    */

    return loop;
}

bool ExynosCamera::m_mainThreadQSetup3AC()
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif

    int  ret  = 0;
    bool loop = true;
    int  pipeId    = PIPE_3AC;
    int  pipeIdCsc = 0;
    int  maxbuffers   = 0;

    ExynosCameraBuffer buffer;
    ExynosCameraFrame  *frame = NULL;
    ExynosCameraFrame  *newframe = NULL;
    nsecs_t timeStamp = 0;
    int frameCount = -1;

    CLOGV("INFO(%s[%d]):wait previewCancelQ", __FUNCTION__, __LINE__);
    ret = m_mainSetupQ[INDEX(pipeId)]->waitAndPopProcessQ(&frame);
    if (m_flagThreadStop == true) {
        CLOGI("INFO(%s[%d]):m_flagThreadStop(%d)", __FUNCTION__, __LINE__, m_flagThreadStop);
        goto func_exit;
    }
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        goto func_exit;
    }

    if (frame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        goto func_exit;
    }
    if (m_bayerBufferMgr->getNumOfAvailableBuffer() > 0) {
        do {
            ret = generateFrame(m_fliteFrameCount, &newframe);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
                goto func_exit;
            }

            ret = m_setupEntity(pipeId, newframe);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):setupEntity fail", __FUNCTION__, __LINE__);
            }
            ret = newframe->getDstBuffer(pipeId, &buffer);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):getDstBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            }

            m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId);
            m_previewFrameFactory->pushFrameToPipe(&newframe, pipeId);

            m_fliteFrameCount++;
        } while (0 < m_bayerBufferMgr->getNumOfAvailableBuffer());
    }

func_exit:
    if( frame != NULL ) {
        frame->decRef();
        m_frameMgr->deleteFrame(frame);;
    }

    return loop;
}

bool ExynosCamera::m_mainThreadQSetupSCP()
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif

    int  ret  = 0;
    bool loop = true;
    int  pipeId    = PIPE_SCP;
    int  pipeIdCsc = 0;
    int  maxbuffers   = 0;

    camera2_node_group node_group_info_isp;
    ExynosCameraBuffer resultBuffer;
    ExynosCameraFrame  *frame = NULL;
    ExynosCameraFrame  *newframe = NULL;
    nsecs_t timeStamp = 0;
    int frameCount = -1;
    int frameGen = 1;

    CLOGV("INFO(%s[%d]):wait previewCancelQ", __FUNCTION__, __LINE__);
    ret = m_mainSetupQ[INDEX(pipeId)]->waitAndPopProcessQ(&frame);
    if (m_flagThreadStop == true) {
        CLOGI("INFO(%s[%d]):m_flagThreadStop(%d)", __FUNCTION__, __LINE__, m_flagThreadStop);
        goto func_exit;
    }
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        goto func_exit;
    }

    if (frame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        goto func_exit;
    }

    ret = generateFrameSccScp(pipeId, &m_scpFrameCount, &newframe);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):generateFrameSccScp fail", __FUNCTION__, __LINE__);
        goto func_exit;
    }

    if( frame->getFrameState() == FRAME_STATE_SKIPPED ) {
        frame->getDstBuffer(pipeId, &resultBuffer);
        m_setupEntity(pipeId, newframe, NULL, &resultBuffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):setupEntity fail", __FUNCTION__, __LINE__);
        }
        newframe->getDstBuffer(pipeId, &resultBuffer);
    } else {
        ret = m_setupEntity(pipeId, newframe);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):setupEntity fail", __FUNCTION__, __LINE__);
        }
        newframe->getDstBuffer(pipeId, &resultBuffer);
    }

    /*check preview drop...*/
    newframe->getDstBuffer(pipeId, &resultBuffer);
    if (resultBuffer.index < 0) {
        newframe->setRequest(pipeId, false);
        newframe->getNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_ISP);
        node_group_info_isp.capture[PERFRAME_BACK_SCP_POS].request = 0;
        newframe->storeNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_ISP);
        m_previewFrameFactory->dump();

        if (m_getRecordingEnabled() == true
            && m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)
            && m_exynosCameraParameters->getHighSpeedRecording() == false) {
            CLOGW("WARN(%s[%d]):Recording preview drop. Failed to get preview buffer. FrameSkipCount(%d)",
                    __FUNCTION__, __LINE__, FRAME_SKIP_COUNT_RECORDING);
            /* when preview buffer is not ready, we should drop preview to make preview buffer ready */
            m_exynosCameraParameters->setFrameSkipCount(FRAME_SKIP_COUNT_RECORDING);
        } else {
           CLOGW("WARN(%s[%d]):Preview drop. Failed to get preview buffer. FrameSkipCount (%d)",
                   __FUNCTION__, __LINE__, FRAME_SKIP_COUNT_PREVIEW);
            /* when preview buffer is not ready, we should drop preview to make preview buffer ready */
            m_exynosCameraParameters->setFrameSkipCount(FRAME_SKIP_COUNT_PREVIEW);
        }
    }

    m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId);
    m_previewFrameFactory->pushFrameToPipe(&newframe, pipeId);

    m_scpFrameCount++;

func_exit:
    if( frame != NULL ) {
        frame->decRef();
        m_frameMgr->deleteFrame(frame);
        frame = NULL;
    }

    /*
    if (m_mainSetupQ[INDEX(pipeId)]->getSizeOfProcessQ() <= 0)
        loop = false;
    */

    return loop;
}

bool ExynosCamera::m_mainThreadFunc(void)
{
    int ret = 0;
    int index = 0;
    ExynosCameraFrame *newFrame = NULL;
    uint32_t frameCnt = 0;

    if (m_previewEnabled == false) {
        CLOGD("DEBUG(%s):preview is stopped, thread stop", __FUNCTION__);
        return false;
    }

    ret = m_pipeFrameDoneQ->waitAndPopProcessQ(&newFrame);
    if (m_flagThreadStop == true) {
        CLOGI("INFO(%s[%d]):m_flagThreadStop(%d)", __FUNCTION__, __LINE__, m_flagThreadStop);
        return false;
    }
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGW("WARN(%s):wait timeout", __FUNCTION__);
        } else {
            CLOGE("ERR(%s):wait and pop fail, ret(%d)", __FUNCTION__, ret);
            /* TODO: doing exception handling */
        }
        return true;
    }

    if (newFrame == NULL) {
        CLOGE("ERR(%s[%d]):newFrame is NULL", __FUNCTION__, __LINE__);
        return true;
    }

    frameCnt = newFrame->getFrameCount();

/* HACK Reset Preview Flag*/
#if 0
    if (m_exynosCameraParameters->getRestartPreview() == true) {
        m_resetPreview = true;
        ret = m_restartPreviewInternal();
        m_resetPreview = false;
        CLOGE("INFO(%s[%d]) m_resetPreview(%d)", __FUNCTION__, __LINE__, m_resetPreview);
        if (ret < 0)
            CLOGE("(%s[%d]): restart preview internal fail", __FUNCTION__, __LINE__);

        return true;
    }
#endif

    if (m_exynosCameraParameters->isFlite3aaOtf() == true) {
        ret = m_handlePreviewFrame(newFrame);
    } else {
        if (m_exynosCameraParameters->getDualMode())
            ret = m_handlePreviewFrameFrontDual(newFrame);
        else
            ret = m_handlePreviewFrameFront(newFrame);
    }
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):handle preview frame fail", __FUNCTION__, __LINE__);
        return ret;
    }

    /* Below code block is moved to m_handlePreviewFrame() and m_handlePreviewFrameFront() functions
     * because we want to delete the frame as soon as the setFrameState(FRAME_STATE_COMPLETE) is called.
     * Otherwise, some other thread might be executed between "setFrameState(FRAME_STATE_COMPLETE)" and
     * "delete frame" statements and might delete the same frame. This would cause the second "delete frame"
     * (trying to delete the same frame) to behave abnormally since that frame is already deleted.
     */
#if 0
    /* Don't use this lock */
    m_frameFliteDeleteBetweenPreviewReprocessing.lock();
    if (newFrame->isComplete() == true) {
        ret = m_removeFrameFromList(&m_processList, newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):remove frame from processList fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }

        if (newFrame->getFrameLockState() == false)
        {
            delete newFrame;
            newFrame = NULL;
        }
    }
    m_frameFliteDeleteBetweenPreviewReprocessing.unlock();
#endif


    /*
     * HACK
     * By using MCpipe. we don't use seperated pipe_scc.
     * so, we will not meet inputFrameQ's fail issue.
     */
    /* m_checkFpsAndUpdatePipeWaitTime(); */

    if(getCameraId() == CAMERA_ID_BACK) {
        m_autoFocusContinousQ.pushProcessQ(&frameCnt);
    }

    return true;
}

status_t ExynosCamera::m_handlePreviewFrame(ExynosCameraFrame *frame)
{
    int ret = 0;
    ExynosCameraFrameEntity *entity = NULL;
    ExynosCameraFrameEntity *curentity = NULL;
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraFrame *fdFrame = NULL;

    ExynosCameraBuffer buffer;

    ExynosCameraBuffer t3acBuffer;
    int pipeID = 0;
    int dummyPipeId = -1;
    /* to handle the high speed frame rate */
    bool skipPreview = false;
    int ratio = 1;
    uint32_t minFps = 0, maxFps = 0;
    uint32_t dispFps = EXYNOS_CAMERA_PREVIEW_FPS_REFERENCE;
    uint32_t fvalid = 0;
    uint32_t fcount = 0;
    uint32_t skipCount = 0;
    struct camera2_stream *shot_stream = NULL;
    ExynosCameraBuffer resultBuffer;
    camera2_node_group node_group_info_isp;
    int32_t reprocessingBayerMode = m_exynosCameraParameters->getReprocessingBayerMode();
    int ispDstBufferIndex = -1;
#ifdef SAMSUNG_LBP
    unsigned int LBPframeCount = 0;
#endif

    entity = frame->getFrameDoneFirstEntity();
    if (entity == NULL) {
        CLOGE("ERR(%s[%d]):current entity is NULL", __FUNCTION__, __LINE__);
        /* TODO: doing exception handling */
        return true;
    }

    curentity = entity;
    pipeID = entity->getPipeId();

    /* TODO: remove hard coding */
    switch(INDEX(entity->getPipeId())) {
    case PIPE_3AA_ISP:
        m_debugFpsCheck(entity->getPipeId());
        ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getSrcBuffer fail, pipeId(%d), ret(%d)",
                __FUNCTION__, __LINE__, entity->getPipeId(), ret);
            return ret;
        }
        ret = m_putBuffers(m_3aaBufferMgr, buffer.index);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
        }

        CLOGV("DEBUG(%s[%d]):3AA_ISP frameCount(%d) frame.Count(%d)",
                __FUNCTION__, __LINE__,
                getMetaDmRequestFrameCount((struct camera2_shot_ext *)buffer.addr[1]),
                frame->getFrameCount());

        ret = frame->getDstBuffer(entity->getPipeId(), &buffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                __FUNCTION__, __LINE__, entity->getPipeId(), ret);
            return ret;
        }

        ret = m_putBuffers(m_ispBufferMgr, buffer.index);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
            break;
        }

        frame->setMetaDataEnable(true);

        /* Face detection */
        if(!m_exynosCameraParameters->getHighSpeedRecording()) {
            skipCount = m_exynosCameraParameters->getFrameSkipCount();
            if( m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_METADATA) &&
                skipCount <= 0) {
                fdFrame = m_frameMgr->createFrame(m_exynosCameraParameters, frame->getFrameCount());
                m_copyMetaFrameToFrame(frame, fdFrame, true, true);
                m_facedetectQ->pushProcessQ(&fdFrame);
            }
        }

        /* ISP capture mode q/dq for vdis */
        if (m_exynosCameraParameters->getTpuEnabledMode() == true) {
#if 0
            /* case 1 : directly push on isp, tpu. */
            ret = m_pushFrameToPipeIspDIS();
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):m_pushFrameToPipeIspDIS() fail", __FUNCTION__, __LINE__);
            }
#else
            /* case 2 : indirectly push on isp, tpu. */
            newFrame = m_frameMgr->createFrame(m_exynosCameraParameters, 0);
            m_mainSetupQ[INDEX(PIPE_ISP)]->pushProcessQ(&newFrame);
#endif
        }

        newFrame = m_frameMgr->createFrame(m_exynosCameraParameters, 0);
        m_mainSetupQ[INDEX(entity->getPipeId())]->pushProcessQ(&newFrame);
        break;
    case PIPE_3AA:
        m_debugFpsCheck(entity->getPipeId());

        /*
        if (entity->getSrcBufState() == ENTITY_BUFFER_STATE_ERROR)
            m_previewFrameFactory->dump();
        */

        /* 3AP buffer handling */
        ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getSrcBuffer fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, entity->getPipeId(), ret);
            return ret;
        }

        if (buffer.index >= 0) {
            ret = m_putBuffers(m_3aaBufferMgr, buffer.index);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
            }
        }

        frame->setMetaDataEnable(true);

        /* 3AC buffer handling */
        do {
            t3acBuffer.index = -1;

            if (frame->getRequest(PIPE_3AC) == true) {
                ret = frame->getDstBuffer(entity->getPipeId(), &t3acBuffer, m_previewFrameFactory->getNodeType(PIPE_3AC));
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):getSrcBuffer fail, pipeId(%d), ret(%d)",
                            __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                }
            }

            if (0 <= t3acBuffer.index) {
                if (frame->getRequest(PIPE_3AC) == true) {
                    if (m_exynosCameraParameters->getHighSpeedRecording() == true) {
                        ret = m_putBuffers(m_bayerBufferMgr, t3acBuffer.index);
                        if (ret < 0) {
                            CLOGE("ERR(%s[%d]):m_putBuffers(m_bayerBufferMgr, %d) fail", __FUNCTION__, __LINE__, t3acBuffer.index);
                            break;
                        }
                    } else {
                        entity_buffer_state_t bufferstate = ENTITY_BUFFER_STATE_NOREQ;
                        ret = frame->getDstBufferState(entity->getPipeId(), &bufferstate, m_previewFrameFactory->getNodeType(PIPE_3AC));
                        if (ret == NO_ERROR && bufferstate != ENTITY_BUFFER_STATE_ERROR) {
                            ret = m_captureSelector->manageFrameHoldList(frame, entity->getPipeId(), false);
                            if (ret < 0) {
                                CLOGE("ERR(%s[%d]):manageFrameHoldList fail", __FUNCTION__, __LINE__);
                                return ret;
                            }
                        } else {
                            ret = m_putBuffers(m_bayerBufferMgr, t3acBuffer.index);
                            if (ret < 0) {
                                CLOGE("ERR(%s[%d]):m_putBuffers(m_bayerBufferMgr, %d) fail", __FUNCTION__, __LINE__, t3acBuffer.index);
                                break;
                            }
                        }
                    }
                } else {
                    if (reprocessingBayerMode == REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON) {
                        CLOGW("WARN(%s[%d]):frame->getRequest(PIPE_3AC) == false. so, just m_putBuffers(t3acBuffer.index(%d)..., pipeId(%d), ret(%d)",
                            __FUNCTION__, __LINE__, t3acBuffer.index, entity->getPipeId(), ret);
                    }

                    ret = m_putBuffers(m_bayerBufferMgr, t3acBuffer.index);
                    if (ret < 0) {
                        CLOGE("ERR(%s[%d]):m_putBuffers(m_bayerBufferMgr, %d) fail", __FUNCTION__, __LINE__, t3acBuffer.index);
                        break;
                    }
                }
            }
        } while (0);

        CLOGV("DEBUG(%s[%d]):3AA_ISP frameCount(%d) frame.Count(%d)",
                __FUNCTION__, __LINE__,
                getMetaDmRequestFrameCount((struct camera2_shot_ext *)buffer.addr[1]),
                frame->getFrameCount());

        /* make the next (n + 1) frame */
        newFrame = m_frameMgr->createFrame(m_exynosCameraParameters, 0);
        m_mainSetupQ[INDEX(entity->getPipeId())]->pushProcessQ(&newFrame);

        break;
    case PIPE_3AC:
    case PIPE_FLITE:
        m_debugFpsCheck(entity->getPipeId());

#ifdef SAMSUNG_DNG
        if (m_exynosCameraParameters->getUseDNGCapture()) {
            CLOGV("[DNG](%s[%d]):PIPE_FLITE (%d)", __FUNCTION__, __LINE__, entity->getPipeId());

            ret = frame->getDstBuffer(entity->getPipeId(), &buffer);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                return ret;
            }

            unsigned int fliteFcount = 0;

            /* Rawdump capture is available in pure bayer only */
            camera2_shot_ext *shot_ext = NULL;
            shot_ext = (camera2_shot_ext *)(buffer.addr[1]);
            if (shot_ext != NULL)
                fliteFcount = shot_ext->shot.dm.request.frameCount;
            else
                ALOGE("ERR(%s[%d]):fliteReprocessingBuffer is null", __FUNCTION__, __LINE__);

            CLOGV("[DNG](%s[%d]):frame count (%d)", __FUNCTION__, __LINE__, fliteFcount);

            if (m_exynosCameraParameters->getCheckMultiFrame()) {
                if (m_searchDNGFrame) {
                    unsigned int DNGFcount = 0;
                    bool DNGCapture_activated = false;

                    if (getDNGFcount() != 0) {
                        m_dngFrameNumber = DNGFcount = getDNGFcount();
                    }

                    CLOGD("[DNG](%s[%d]):DNGFcount(%d) ois mode(%d) frame count(%d)",
                        __FUNCTION__, __LINE__,DNGFcount, m_exynosCameraParameters->getOISCaptureModeOn(), fliteFcount);

                    if (DNGFcount > 0) {
                        DNGCapture_activated = true;
                    }

                    if (m_exynosCameraParameters->getCaptureExposureTime() == 0 &&
                        DNGCapture_activated && fliteFcount >= DNGFcount) {
                        DNGCapture_activated = false;
                        m_searchDNGFrame = false;
                        m_previewFrameFactory->setRequestFLITE(false);

                        CLOGD("[DNG](%s[%d]):Raw frame count (%d)", __FUNCTION__, __LINE__, fliteFcount);

                        m_DNGCaptureThread->run();
                        m_dngCaptureQ->pushProcessQ(&buffer);
                    } else if (m_exynosCameraParameters->getCaptureExposureTime() != 0 &&
                        getDNGFcount() == 0) {
                        if (!m_DNGCaptureThread->isRunning()) {
                            m_DNGCaptureThread->run();
                        }
                        m_dngCaptureQ->pushProcessQ(&buffer);
                        newFrame = m_frameMgr->createFrame(m_exynosCameraParameters, 0);
                        m_mainSetupQ[INDEX(entity->getPipeId())]->pushProcessQ(&newFrame);
                    } else if (m_exynosCameraParameters->getCaptureExposureTime() != 0 &&
                        DNGCapture_activated && fliteFcount >= DNGFcount) {
                        if (m_longExposureEnds) {
                            DNGCapture_activated = false;
                            m_searchDNGFrame = false;
                            m_previewFrameFactory->setRequestFLITE(false);
                            CLOGE("[DNG](%s[%d]):flite qbuffer m_searchDNGFrame(%d)",
                                __FUNCTION__, __LINE__, m_searchDNGFrame ,fliteFcount);
                            ret = m_putBuffers(m_fliteBufferMgr, buffer.index);
                            if (ret < 0) {
                                CLOGE("ERR(%s[%d]):m_putBuffers(%d) fail", __FUNCTION__, __LINE__, buffer.index);
                                break;
                            }
                        } else {
                            newFrame = m_frameMgr->createFrame(m_exynosCameraParameters, 0);
                            m_mainSetupQ[INDEX(entity->getPipeId())]->pushProcessQ(&newFrame);
                            m_dngCaptureQ->pushProcessQ(&buffer);
                        }
                        if (!m_DNGCaptureThread->isRunning() && !m_longExposureEnds) {
                            m_DNGCaptureThread->run();
                        }
                    } else {
                        CLOGE("[DNG](%s[%d]):flite qbuffer m_searchDNGFrame(%d)",
                            __FUNCTION__, __LINE__, m_searchDNGFrame ,fliteFcount);
                        ret = m_putBuffers(m_fliteBufferMgr, buffer.index);
                        if (ret < 0) {
                            CLOGE("ERR(%s[%d]):m_putBuffers(%d) fail", __FUNCTION__, __LINE__, buffer.index);
                            break;
                        }

                        newFrame = m_frameMgr->createFrame(m_exynosCameraParameters, 0);
                        m_mainSetupQ[INDEX(entity->getPipeId())]->pushProcessQ(&newFrame);
                    }
                } else {
                    CLOGE("[DNG](%s[%d]):flite dqbuffer m_searchDNGFrame(%d)",
                        __FUNCTION__, __LINE__, m_searchDNGFrame ,fliteFcount);
                    ret = m_putBuffers(m_fliteBufferMgr, buffer.index);
                    if (ret < 0) {
                        CLOGE("ERR(%s[%d]):m_putBuffers(%d) fail", __FUNCTION__, __LINE__, buffer.index);
                        break;
                    }
                }
            } else {
                CLOGD("[DNG](%s[%d]):Raw frame count (%d)", __FUNCTION__, __LINE__, fliteFcount);
                m_dngFrameNumber = fliteFcount;
                m_captureSelector->setDNGFrame(fliteFcount);
                m_previewFrameFactory->stopThread(entity->getPipeId());
                m_DNGCaptureThread->run();
                m_dngCaptureQ->pushProcessQ(&buffer);
            }
        } else
#endif
        {
            if (m_exynosCameraParameters->getHighSpeedRecording()) {
                ret = frame->getDstBuffer(entity->getPipeId(), &buffer);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                    return ret;
                }
                ret = m_putBuffers(m_bayerBufferMgr, buffer.index);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
                    break;
                }
            } else {
                if (frame->getSccDrop() == true || frame->getIspcDrop() == true) {
                    CLOGE("ERR(%s[%d]):getSccDrop() == %d || getIspcDrop()== %d. so drop this frame(frameCount : %d)",
                        __FUNCTION__, __LINE__, frame->getSccDrop(), frame->getIspcDrop(), frame->getFrameCount());

                    ret = frame->getDstBuffer(entity->getPipeId(), &buffer);
                    if (ret < 0) {
                        CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                            __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                        return ret;
                    }

                    ret = m_putBuffers(m_bayerBufferMgr, buffer.index);
                    if (ret < 0) {
                        CLOGE("ERR(%s[%d]):m_putBuffers(%d) fail", __FUNCTION__, __LINE__, buffer.index);
                        break;
                    }
                } else {
                    ret = m_captureSelector->manageFrameHoldList(frame, entity->getPipeId(), false);
                    if (ret < 0) {
                        CLOGE("ERR(%s[%d]):manageFrameHoldList fail", __FUNCTION__, __LINE__);
                        return ret;
                    }
                }
            }

            /* TODO: Dynamic bayer capture, currently support only single shot */
            if (reprocessingBayerMode == REPROCESSING_BAYER_MODE_PURE_DYNAMIC
                || reprocessingBayerMode == REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC)
                m_previewFrameFactory->stopThread(entity->getPipeId());

            if (reprocessingBayerMode == REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON
                || reprocessingBayerMode == REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON) {
                newFrame = m_frameMgr->createFrame(m_exynosCameraParameters, 0);
                m_mainSetupQ[INDEX(entity->getPipeId())]->pushProcessQ(&newFrame);
            }
        }
        break;
    case PIPE_ISP:
        /*
        if (entity->getSrcBufState() == ENTITY_BUFFER_STATE_ERROR)
            m_previewFrameFactory->dump();
        */

        ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getSrcBuffer fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, entity->getPipeId(), ret);
            return ret;
        }

        if (buffer.index >= 0) {
            ret = m_putBuffers(m_ispBufferMgr, buffer.index);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
                break;
            }
        }

        /* ISP capture mode q/dq for vdis */
        if (m_exynosCameraParameters->getTpuEnabledMode() == true) {
            break;
        }
    case PIPE_DIS:
        /* this check twice on PIPE_SCP. so, disable */
        /* m_debugFpsCheck(pipeID); */
        if (m_exynosCameraParameters->getTpuEnabledMode() == true) {
            ret = frame->getSrcBuffer(PIPE_DIS, &buffer);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):getSrcBuffer fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, PIPE_ISP, ret);
                return ret;
            }

            if (buffer.index >= 0) {
                ret = m_putBuffers(m_hwDisBufferMgr, buffer.index);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):m_putBuffers(m_hwDisBufferMgr, %d) fail",
                        __FUNCTION__, __LINE__, buffer.index);
                }
            }
        }

        CLOGV("DEBUG(%s[%d]):DIS done HAL-frameCount(%d)",
            __FUNCTION__, __LINE__, frame->getFrameCount());

        /*
         * dis capture is scp.
         * so, skip break;
         */

    case PIPE_SCP:
        /* Face detection */
        if (!m_exynosCameraParameters->getHighSpeedRecording()
            && frame->getFrameState() != FRAME_STATE_SKIPPED) {
            skipCount = m_exynosCameraParameters->getFrameSkipCount();
#ifdef SR_CAPTURE
            if(m_isCopySrMdeta) {
                frame->getDynamicMeta(&m_srShotMeta);
                m_isCopySrMdeta = false;
                CLOGI("INFO(%s[%d]) copy SR FdMeta", __FUNCTION__, __LINE__);
            }
#endif
            if( m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_METADATA) &&
                skipCount <= 0) {
                fdFrame = m_frameMgr->createFrame(m_exynosCameraParameters, frame->getFrameCount());
                m_copyMetaFrameToFrame(frame, fdFrame, true, true);
                m_facedetectQ->pushProcessQ(&fdFrame);
            }
        }

        if (m_exynosCameraParameters->getZoomPreviewWIthScaler()== true) {
            CLOGV("INFO(%s[%d]): zoom preview with csc pipeId(%d) frame(%d)",
                __FUNCTION__, __LINE__, entity->getPipeId(), frame->getFrameCount());

            m_zoomPreviwWithCscQ->pushProcessQ(&frame);
            break;
        } else {
            CLOGV("INFO(%s[%d]): zoom preview without csc pipeId(%d) frame(%d)",
                __FUNCTION__, __LINE__, entity->getPipeId(), frame->getFrameCount());
        }

    case PIPE_GSC:
        if (m_exynosCameraParameters->getZoomPreviewWIthScaler()== true) {
            CLOGV("INFO(%s[%d]): zoom preview with csc pipeId(%d) frame(%d)",
                __FUNCTION__, __LINE__, entity->getPipeId(), frame->getFrameCount());

            dummyPipeId = (m_exynosCameraParameters->getTpuEnabledMode())? PIPE_DIS : PIPE_ISP;
            ret = m_syncPrviewWithCSC(dummyPipeId, PIPE_GSC, frame);
            if (ret < 0) {
                CLOGW("WARN(%s[%d]):m_syncPrviewWithCSC failed frame(%d)",
                    __FUNCTION__, __LINE__, frame->getFrameCount());
            }

            entity = frame->searchEntityByPipeId(dummyPipeId);
            if (entity == NULL) {
                CLOGE("ERR(%s[%d]):searchEntityByPipeId failed pipe(%d) frame(%d)",
                    __FUNCTION__, __LINE__, dummyPipeId, frame->getFrameCount());
            }
        } else {
            CLOGV("INFO(%s[%d]): zoom preview without csc pipeId(%d) frame(%d)",
                __FUNCTION__, __LINE__, entity->getPipeId(), frame->getFrameCount());
        }

        if (entity->getDstBufState() == ENTITY_BUFFER_STATE_ERROR) {
            if (frame->getFrameState() != FRAME_STATE_SKIPPED) {
                ret = frame->getDstBuffer(entity->getPipeId(), &buffer, m_previewFrameFactory->getNodeType(PIPE_SCP));
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                            __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                    return ret;
                }

                if (buffer.index >= 0) {
                    ret = m_scpBufferMgr->cancelBuffer(buffer.index, true);
                    if (ret < 0)
                        CLOGE("ERR(%s[%d]):SCP buffer return fail", __FUNCTION__, __LINE__);
                }
                /* For debug */
                /* m_previewFrameFactory->dump(); */

                /* Comment out, because it included ISP */
                /*
                   newFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(PIPE_SCP, frame->getFrameCount());
                   newFrame->setDstBuffer(PIPE_SCP, buffer);
                   newFrame->setFrameState(FRAME_STATE_SKIPPED);

                   m_mainSetupQ[INDEX(entity->getPipeId())]->pushProcessQ(&newFrame);
                 */
            }
            CLOGV("DEBUG(%s[%d]):SCP done HAL-frameCount(%d)", __FUNCTION__, __LINE__, frame->getFrameCount());
        } else if (entity->getDstBufState() == ENTITY_BUFFER_STATE_COMPLETE) {
            m_debugFpsCheck(entity->getPipeId());

            ret = frame->getDstBuffer(entity->getPipeId(), &buffer, m_previewFrameFactory->getNodeType(PIPE_SCP));
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                return ret;
            }

            memset(m_meta_shot, 0x00, sizeof(struct camera2_shot_ext));
            frame->getDynamicMeta(m_meta_shot);
            frame->getUserDynamicMeta(m_meta_shot);

            m_checkEntranceLux(m_meta_shot);
#ifdef SAMSUNG_LBP
            if (m_exynosCameraParameters->getUsePureBayerReprocessing() == true) {
                camera2_shot_ext *shot_ext = NULL;
                shot_ext = (camera2_shot_ext *)(buffer.addr[1]);

                if (shot_ext != NULL)
                    LBPframeCount = shot_ext->shot.dm.request.frameCount;
                else
                    ALOGE("ERR(%s[%d]):Buffer is null", __FUNCTION__, __LINE__);
            } else {
                camera2_stream *lbp_shot_stream = NULL;
                lbp_shot_stream = (struct camera2_stream *)buffer.addr[2];
                getStreamFrameCount(lbp_shot_stream, &LBPframeCount);
            }
            if(getCameraId() == CAMERA_ID_FRONT)
                m_exynosCameraParameters->setSCPFrameCount(LBPframeCount);
#endif
#ifdef SAMSUNG_JQ
            if(m_isJpegQtableOn == true
#ifdef SAMSUNG_LBP
                && m_isLBPon == false
#endif
            ) {
                m_isJpegQtableOn = false;
                ret = m_processJpegQtable(&buffer);
                if (ret < 0) {
                    CLOGE("[JQ](%s[%d]): m_processJpegQtable() failed!!", __FUNCTION__, __LINE__);
                }
            }
#endif
#ifdef SAMSUNG_LLV
            if (m_LLVstatus == LLV_INIT) {
                int HwPreviewW = 0, HwPreviewH = 0;
                m_exynosCameraParameters->getHwPreviewSize(&HwPreviewW, &HwPreviewH);
                UniPluginBufferData_t pluginData;
                memset(&pluginData, 0, sizeof(UniPluginBufferData_t));
                int powerCtrl = UNI_PLUGIN_POWER_CONTROL_OFF;
                float Low_Lux = -5.5;
                float High_Lux = -3.5;
                float Low_Lux_Front = -5.2;
                float High_Lux_Front = -3.2;
                float LLV_Lux = (int32_t)m_meta_shot->shot.udm.ae.vendorSpecific[5] / 256.0;

                pluginData.InBuffY = buffer.addr[0];
                pluginData.InBuffU = buffer.addr[1];

                pluginData.InWidth = HwPreviewW;
                pluginData.InHeight = HwPreviewH;
                pluginData.BrightnessLux = (int32_t)m_meta_shot->shot.udm.ae.vendorSpecific[5];

                if (getCameraId() == CAMERA_ID_FRONT) {
                     if ((LLV_Lux > Low_Lux_Front) && (LLV_Lux <= High_Lux_Front))
                        powerCtrl = UNI_PLUGIN_POWER_CONTROL_4;
                }
                else if (getCameraId() == CAMERA_ID_BACK){
                    if ((LLV_Lux > Low_Lux) && (LLV_Lux <= High_Lux))
                        powerCtrl = UNI_PLUGIN_POWER_CONTROL_4;
                }

#ifdef SAMSUNG_LLV_TUNING
                powerCtrl = m_LLVpowerLevel;
                CLOGD("[LLV_TEST] powerCtrl: %d", powerCtrl);
#endif
                if(m_LLVpluginHandle != NULL) {
                    uni_plugin_set(m_LLVpluginHandle,
                        LLV_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &pluginData);
                    uni_plugin_set(m_LLVpluginHandle,
                        LLV_PLUGIN_NAME, UNI_PLUGIN_INDEX_POWER_CONTROL, &powerCtrl);
                    uni_plugin_process(m_LLVpluginHandle);
                }
            }
            else if (m_LLVstatus == LLV_STOPPED) {
                if(m_LLVpluginHandle != NULL) {
                    uni_plugin_deinit(m_LLVpluginHandle);
                }
                m_LLVstatus = LLV_UNINIT;
            }
#endif

#ifdef SAMSUNG_OT
            if (m_exynosCameraParameters->getObjectTrackingChanged() == true
                && m_OTstatus == UNI_PLUGIN_OBJECT_TRACKING_IDLE && m_OTstart == OBJECT_TRACKING_INIT) {
                int maxNumFocusAreas = m_exynosCameraParameters->getMaxNumFocusAreas();
                ExynosRect2* objectTrackingArea = new ExynosRect2[maxNumFocusAreas];
                int* objectTrackingWeight = new int[maxNumFocusAreas];
                int validNumber;
                UniPluginFocusData_t focusData;
                memset(&focusData, 0, sizeof(UniPluginFocusData_t));

                m_exynosCameraParameters->getObjectTrackingAreas(&validNumber, objectTrackingArea, objectTrackingWeight);

                for (int i = 0; i < validNumber; i++) {
                    focusData.FocusROILeft = objectTrackingArea[i].x1;
                    focusData.FocusROIRight = objectTrackingArea[i].x2;
                    focusData.FocusROITop = objectTrackingArea[i].y1;
                    focusData.FocusROIBottom = objectTrackingArea[i].y2;
                    focusData.FocusWeight = objectTrackingWeight[i];
                    ret = uni_plugin_set(m_OTpluginHandle,
                              OBJECT_TRACKING_PLUGIN_NAME, UNI_PLUGIN_INDEX_FOCUS_INFO, &focusData);
                    if(ret < 0) {
                        CLOGE("[OBTR](%s[%d]):Object Tracking plugin set focus info failed!!", __FUNCTION__, __LINE__);
                    }
                }

                delete[] objectTrackingArea;
                delete[] objectTrackingWeight;

                m_exynosCameraParameters->setObjectTrackingChanged(false);
                m_OTisTouchAreaGet = true;
            }

            if ((m_exynosCameraParameters->getObjectTrackingEnable() == true
                || m_exynosCameraParameters->getObjectTrackingGet() == true)
                && m_OTstart == OBJECT_TRACKING_INIT
                && m_OTisTouchAreaGet == true) {
                int HwPreviewW = 0, HwPreviewH = 0;
                m_exynosCameraParameters->getHwPreviewSize(&HwPreviewW, &HwPreviewH);

                UniPluginBufferData_t bufferData;
                memset(&bufferData, 0, sizeof(UniPluginBufferData_t));

                bufferData.InBuffY = buffer.addr[0];
                bufferData.InBuffU = buffer.addr[1];

                bufferData.InWidth = HwPreviewW;
                bufferData.InHeight = HwPreviewH;
                bufferData.InFormat = UNI_PLUGIN_FORMAT_NV21;
                ret = uni_plugin_set(m_OTpluginHandle,
                          OBJECT_TRACKING_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &bufferData);
                if(ret < 0) {
                    CLOGE("[OBTR](%s[%d]):Object Tracking plugin set buffer info failed!!", __FUNCTION__, __LINE__);
                }

                ret = uni_plugin_process(m_OTpluginHandle);
                if(ret < 0) {
                    CLOGE("[OBTR](%s[%d]):Object Tracking plugin process failed!!", __FUNCTION__, __LINE__);
                }

                uni_plugin_get(m_OTpluginHandle,
                    OBJECT_TRACKING_PLUGIN_NAME, UNI_PLUGIN_INDEX_FOCUS_INFO, &m_OTfocusData);
                CLOGV("[OBTR](%s[%d])Focus state: %d, x1: %d, x2: %d, y1: %d, y2: %d, weight: %d",
                        __FUNCTION__, __LINE__, m_OTfocusData.FocusState,
                        m_OTfocusData.FocusROILeft, m_OTfocusData.FocusROIRight,
                        m_OTfocusData.FocusROITop, m_OTfocusData.FocusROIBottom, m_OTfocusData.FocusWeight);
                CLOGV("[OBTR](%s[%d])Wmove: %d, Hmove: %d, Wvel: %d, Hvel: %d",
                        __FUNCTION__, __LINE__, m_OTfocusData.W_Movement, m_OTfocusData.H_Movement,
                        m_OTfocusData.W_Velocity, m_OTfocusData.H_Velocity);

                if (m_exynosCameraParameters->getObjectTrackingEnable() == true && m_OTstart == OBJECT_TRACKING_INIT) {
                    m_exynosCameraParameters->setObjectTrackingGet(true);
                }

                m_OTstatus = m_OTfocusData.FocusState;

                uni_plugin_get(m_OTpluginHandle,
                    OBJECT_TRACKING_PLUGIN_NAME, UNI_PLUGIN_INDEX_FOCUS_PREDICTED, &m_OTpredictedData);
                CLOGV("[OBTR](%s[%d])Predicted state: %d, x1: %d, x2: %d, y1: %d, y2: %d, weight: %d",
                        __FUNCTION__, __LINE__, m_OTpredictedData.FocusState,
                        m_OTpredictedData.FocusROILeft, m_OTpredictedData.FocusROIRight,
                        m_OTpredictedData.FocusROITop, m_OTpredictedData.FocusROIBottom, m_OTpredictedData.FocusWeight);
                CLOGV("[OBTR](%s[%d])Wmove: %d, Hmove: %d, Wvel: %d, Hvel: %d",
                        __FUNCTION__, __LINE__, m_OTpredictedData.W_Movement, m_OTpredictedData.H_Movement,
                        m_OTpredictedData.W_Velocity, m_OTpredictedData.H_Velocity);

                ExynosRect cropRegionRect;
                ExynosRect2 oldrect, newRect;
                oldrect.x1 = m_OTpredictedData.FocusROILeft;
                oldrect.x2 = m_OTpredictedData.FocusROIRight;
                oldrect.y1 = m_OTpredictedData.FocusROITop;
                oldrect.y2 = m_OTpredictedData.FocusROIBottom;
                m_exynosCameraParameters->getHwBayerCropRegion(&cropRegionRect.w, &cropRegionRect.h, &cropRegionRect.x, &cropRegionRect.y);
                newRect = convertingAndroidArea2HWAreaBcropOut(&oldrect, &cropRegionRect);
                m_OTpredictedData.FocusROILeft = newRect.x1;
                m_OTpredictedData.FocusROIRight = newRect.x2;
                m_OTpredictedData.FocusROITop = newRect.y1;
                m_OTpredictedData.FocusROIBottom = newRect.y2;

                ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
                ret = autoFocusMgr->setObjectTrackingAreas(&m_OTpredictedData);
                if(ret == false) {
                    CLOGE("[OBTR](%s[%d]):setObjectTrackingAreas failed!!", __FUNCTION__, __LINE__);
                }
            }
            if(m_OTstart == OBJECT_TRACKING_DEINIT) {
                int ret = uni_plugin_deinit(m_OTpluginHandle);
                if(ret < 0) {
                    CLOGE("[OBTR](%s[%d]):Object Tracking plugin deinit failed!!", __FUNCTION__, __LINE__);
                }
                m_OTstart = OBJECT_TRACKING_IDLE;
                /* Waiting in the recording mode can make preview thread stop */
#if 0
                m_OT_mutex.lock();
                if(m_OTisWait == true)
                    m_OT_condition.signal();
                m_OT_mutex.unlock();
#endif
            }
#endif
#ifdef SAMSUNG_LBP
            if(((m_captureSelector->getFrameIndex() == LBPframeCount && m_captureSelector->getFrameIndex())
                || (m_LBPindex > 0 && m_LBPindex < m_exynosCameraParameters->getHoldFrameCount()))
                    && m_exynosCameraParameters->getSeriesShotMode() == SERIES_SHOT_MODE_NONE
                    && m_isLBPon == true){
                CLOGD("[LBP](%s[%d]):Start BestPic(%d/%d)", __FUNCTION__, __LINE__, LBPframeCount, m_LBPindex);
                char *srcAddr = NULL;
                char *dstAddr = NULL;
                int hwPreviewFormat = m_exynosCameraParameters->getHwPreviewFormat();
                int planeCount = getYuvPlaneCount(hwPreviewFormat);
                if (planeCount <= 0) {
                    CLOGE("[LBP](%s[%d]):getYuvPlaneCount(%d) fail", __FUNCTION__, __LINE__, hwPreviewFormat);
                }

                ret = m_lbpBufferMgr->getBuffer(&m_LBPindex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &m_LBPbuffer[m_LBPindex].buffer);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):m_lbpBufferMgr->getBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
                    return ret;
                }

                /* TODO : have to consider all fmt(planes) and stride */
                for (int plane = 0; plane < planeCount; plane++) {
                    srcAddr = buffer.addr[plane];
                    dstAddr = m_LBPbuffer[m_LBPindex].buffer.addr[plane];
                    memcpy(dstAddr, srcAddr, m_LBPbuffer[m_LBPindex].buffer.size[plane]);
                }
                m_LBPbuffer[m_LBPindex].frameNumber = LBPframeCount;
                /* There is a case that LBP thread ends first, we need to increase the index before that. */
                m_LBPindex++;
                m_LBPbufferQ->pushProcessQ(&m_LBPbuffer[m_LBPindex-1]);
            }
#endif
#ifdef SUPPORT_SW_VDIS
            if(m_swVDIS_Mode) {
                memset(m_swVDIS_fd_dm, 0x00, sizeof(struct camera2_dm));
                if (frame->getDynamicMeta(m_swVDIS_fd_dm) != NO_ERROR) {
                    CLOGE("ERR(%s[%d]) meta_shot_ext is null", __FUNCTION__, __LINE__);
                }

                int swVDIS_BufIndex = -1;
                int swVDIS_IndexCount = (m_swVDIS_FrameNum % NUM_VDIS_BUFFERS);
                ret = m_swVDIS_BufferMgr->getBuffer(&swVDIS_BufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL,
                            &m_swVDIS_OutBuf[swVDIS_IndexCount]);

                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):m_swVDIS_BufferMgr->getBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
                    return ret;
                }

                nsecs_t swVDIS_timeStamp = (nsecs_t)frame->getTimeStamp();
                nsecs_t swVDIS_timeStampBoot = (nsecs_t)frame->getTimeStampBoot();
                int swVDIS_Lux = m_meta_shot->shot.udm.ae.vendorSpecific[5] / 256;
                int swVDIS_ZoomLevel = m_exynosCameraParameters->getZoomLevel();
                float swVDIS_ExposureValue = (int32_t)m_meta_shot->shot.udm.ae.vendorSpecific[4] / 256.0;

                int swVDIS_num = 0;
                swVDIS_num = NUM_OF_DETECTED_FACES;
                memset(m_swVDIS_FaceData, 0x00, sizeof(swVDIS_FaceData_t));

                for (int i = 0; i < swVDIS_num; i++) {
                    if (m_swVDIS_fd_dm->stats.faceIds[i]) {
                        m_swVDIS_FaceData->FaceRect[i].left = m_swVDIS_fd_dm->stats.faceRectangles[i][0];
                        m_swVDIS_FaceData->FaceRect[i].top = m_swVDIS_fd_dm->stats.faceRectangles[i][1];
                        m_swVDIS_FaceData->FaceRect[i].right = m_swVDIS_fd_dm->stats.faceRectangles[i][2];
                        m_swVDIS_FaceData->FaceRect[i].bottom= m_swVDIS_fd_dm->stats.faceRectangles[i][3];
                        m_swVDIS_FaceData->FaceNum++;
                    }
                }

                m_swVDIS_process(&buffer, swVDIS_IndexCount, swVDIS_timeStamp, swVDIS_timeStampBoot,
                    swVDIS_Lux, swVDIS_ZoomLevel, swVDIS_ExposureValue, m_swVDIS_FaceData);

#ifdef SAMSUNG_OIS_VDIS
                if(m_exynosCameraParameters->getOIS() == OPTICAL_STABILIZATION_MODE_VDIS) {
                    UNI_PLUGIN_OIS_MODE mode = m_swVDIS_getOISmode();
                    uint32_t coef = 0;
                    if(mode == UNI_PLUGIN_OIS_MODE_CENTERING)
                        coef = 0xFFFF;
                    else
                        coef = 0x1;

                    if(m_OISvdisMode != mode) {
                        CLOGD("DEBUG(%s[%d]):OIS VDIS coef: %d", __FUNCTION__, __LINE__, coef);
                        m_OISvdisMode = mode;
                    }
                    m_exynosCameraParameters->setOISCoef(coef);
                }
#endif
            }
#endif /*SUPPORT_SW_VDIS*/

            /* TO DO : skip frame for HDR */
            shot_stream = (struct camera2_stream *)buffer.addr[2];

            if (shot_stream != NULL) {
                getStreamFrameValid(shot_stream, &fvalid);
                getStreamFrameCount(shot_stream, &fcount);
            } else {
                CLOGE("ERR(%s[%d]):shot_stream is NULL", __FUNCTION__, __LINE__);
                fvalid = false;
                fcount = 0;
            }

            /* drop preview frame if lcd supported frame rate < scp frame rate */
            frame->getFpsRange(&minFps, &maxFps);
            if (dispFps < maxFps) {
                ratio = (int)((maxFps * 10 / dispFps) / 10);
                m_displayPreviewToggle = (m_displayPreviewToggle + 1) % ratio;
                skipPreview = (m_displayPreviewToggle == 0) ? true : false;
#ifdef DEBUG
                CLOGE("DEBUG(%s[%d]):preview frame skip! frameCount(%d) (m_displayPreviewToggle=%d,\
                    maxFps=%d, dispFps=%d, ratio=%d, skipPreview=%d)",
                    __FUNCTION__, __LINE__, frame->getFrameCount(),
                    m_displayPreviewToggle, maxFps, dispFps, ratio, (int)skipPreview);
#endif
            }

            newFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(PIPE_SCP, frame->getFrameCount());

            newFrame->setDstBuffer(PIPE_SCP, buffer);

            m_exynosCameraParameters->getFrameSkipCount(&m_frameSkipCount);
            if (m_frameSkipCount > 0) {
                CLOGD("INFO(%s[%d]):Skip frame for frameSkipCount(%d) buffer.index(%d)",
                        __FUNCTION__, __LINE__, m_frameSkipCount, buffer.index);
#ifdef SUPPORT_SW_VDIS
                if(m_swVDIS_Mode) {
                    if(m_frameSkipCount == 1) {
                        for(int i = 0; i < NUM_VDIS_BUFFERS; i++) {
                            ret = m_swVDIS_BufferMgr->cancelBuffer(i);
                            if (ret < 0) {
                                CLOGE("ERR(%s[%d]):swVDIS buffer return fail", __FUNCTION__, __LINE__);
                            }
                        }
                    }
                }
#endif /*SUPPORT_SW_VDIS*/
                newFrame->setFrameState(FRAME_STATE_SKIPPED);
                if (buffer.index >= 0) {
                    ret = m_scpBufferMgr->cancelBuffer(buffer.index, true);
                    if (ret < 0)
                        CLOGE("ERR(%s[%d]):SCP buffer return fail", __FUNCTION__, __LINE__);
                }

            } else {
                if (m_skipReprocessing == true)
                    m_skipReprocessing = false;
#ifdef SUPPORT_SW_VDIS
                if(m_swVDIS_Mode) {
                    nsecs_t timeStamp = m_swVDIS_timeStamp[m_swVDIS_OutBuf[m_swVDIS_OutQIndex].index];

                    ret = m_putBuffers(m_swVDIS_BufferMgr, m_swVDIS_OutBuf[m_swVDIS_OutQIndex].index);
                    if (ret < 0) {
                        CLOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
                        break;
                    }

                    if (m_getRecordingEnabled() == true
                        && m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)) {
                        if (timeStamp <= 0L) {
                            CLOGE("WARN(%s[%d]):timeStamp(%lld) Skip", __FUNCTION__, __LINE__, timeStamp);
                        } else {
                            if (m_exynosCameraParameters->doCscRecording() == true) {
                                /* get Recording Image buffer */
                                int bufIndex = -2;
                                ExynosCameraBuffer recordingBuffer;
                                ret = m_recordingBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &recordingBuffer);
                                if (ret < 0 || bufIndex < 0) {
                                    if ((++m_recordingFrameSkipCount % 100) == 0) {
                                        CLOGE("ERR(%s[%d]): Recording buffer is not available!! Recording Frames are Skipping(%d frames) (bufIndex=%d)",
                                            __FUNCTION__, __LINE__, m_recordingFrameSkipCount, bufIndex);
                                        m_recordingBufferMgr->printBufferQState();
                                    }
                                } else {
                                    if (m_recordingFrameSkipCount != 0) {
                                        CLOGE("ERR(%s[%d]): Recording buffer is not available!! Recording Frames are Skipped(%d frames) (bufIndex=%d) (recordingQ=%d)",
                                               __FUNCTION__, __LINE__, m_recordingFrameSkipCount, bufIndex, m_recordingQ->getSizeOfProcessQ());
                                        m_recordingFrameSkipCount = 0;
                                        m_recordingBufferMgr->printBufferQState();
                                    }

                                    ret = m_doPrviewToRecordingFunc(PIPE_GSC_VIDEO, m_swVDIS_OutBuf[m_swVDIS_OutQIndex], recordingBuffer, timeStamp);
                                    if (ret < 0) {
                                        CLOGW("WARN(%s[%d]):recordingCallback Skip", __FUNCTION__, __LINE__);
                                    }
                                }
                            } else {
                                m_recordingTimeStamp[m_swVDIS_OutBuf[m_swVDIS_OutQIndex].index] = timeStamp;

                                if (m_recordingStartTimeStamp == 0) {
                                    m_recordingStartTimeStamp = timeStamp;
                                    CLOGI("INFO(%s[%d]):m_recordingStartTimeStamp=%lld",
                                        __FUNCTION__, __LINE__, m_recordingStartTimeStamp);
                                }

                                if ((0L < timeStamp)
                                    && (m_lastRecordingTimeStamp < timeStamp)
                                    && (m_recordingStartTimeStamp <= timeStamp)) {
                                    if (m_getRecordingEnabled() == true
                                        && m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)) {
#ifdef CHECK_MONOTONIC_TIMESTAMP
                                        CLOGD("DEBUG(%s[%d]):m_dataCbTimestamp::recordingFrameIndex=%d, recordingTimeStamp=%lld, fd[0]=%d",
                                            __FUNCTION__, __LINE__, m_swVDIS_OutBuf[m_swVDIS_OutQIndex].index, timeStamp, m_swVDIS_OutBuf[m_swVDIS_OutQIndex].fd[0]);
#endif
#ifdef DEBUG
                                        CLOGD("DEBUG(%s[%d]): - lastTimeStamp(%lld), systemTime(%lld), recordingStart(%lld)",
                                            __FUNCTION__, __LINE__,
                                            m_lastRecordingTimeStamp,
                                            systemTime(SYSTEM_TIME_MONOTONIC),
                                            m_recordingStartTimeStamp);
#endif

                                        if (m_recordingBufAvailable[m_swVDIS_OutBuf[m_swVDIS_OutQIndex].index] == false) {
                                             CLOGW("WARN(%s[%d]):recordingFrameIndex(%d) didn't release yet !!! drop the frame !!! "
                                                 " timeStamp(%lld) m_recordingBufAvailable(%d)",
                                                 __FUNCTION__,
                                                 __LINE__,
                                                 m_swVDIS_OutBuf[m_swVDIS_OutQIndex].index,
                                                 timeStamp,
                                                 (int)m_recordingBufAvailable[m_swVDIS_OutBuf[m_swVDIS_OutQIndex].index]);
                                        } else {
                                            if (m_recordingCallbackHeap != NULL) {

                                                struct VideoNativeHandleMetadata *recordAddrs = NULL;

                                                recordAddrs = (struct VideoNativeHandleMetadata *)m_recordingCallbackHeap->data;
                                                recordAddrs[m_swVDIS_OutBuf[m_swVDIS_OutQIndex].index].eType = kMetadataBufferTypeNativeHandleSource;

                                                native_handle* handle = native_handle_create(2, 1);
                                                handle->data[0] = (int32_t) m_swVDIS_OutBuf[m_swVDIS_OutQIndex].fd[0];
                                                handle->data[1] = (int32_t) m_swVDIS_OutBuf[m_swVDIS_OutQIndex].fd[1];
                                                handle->data[2] = (int32_t) m_swVDIS_OutBuf[m_swVDIS_OutQIndex].index;

                                                recordAddrs[m_swVDIS_OutBuf[m_swVDIS_OutQIndex].index].pHandle = handle;

                                                m_recordingBufAvailable[m_swVDIS_OutBuf[m_swVDIS_OutQIndex].index] = false;
                                                m_lastRecordingTimeStamp = timeStamp;
                                            } else {
                                                CLOGD("INFO(%s[%d]) : m_recordingCallbackHeap is NULL.", __FUNCTION__, __LINE__);
                                            }

                                            if (m_getRecordingEnabled() == true
                                                && m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)) {
#ifdef SAMSUNG_HLV
                                                if (m_HLV) {
                                                    /* Ignore the ERROR .. HLV solution is smart */
                                                    ret = m_ProgramAndProcessHLV(&m_swVDIS_OutBuf[m_swVDIS_OutQIndex]);
                                                }
#endif
                                                m_dataCbTimestamp(
                                                    timeStamp,
                                                    CAMERA_MSG_VIDEO_FRAME,
                                                    m_recordingCallbackHeap,
                                                    m_swVDIS_OutBuf[m_swVDIS_OutQIndex].index,
                                                    m_callbackCookie);
                                            }
                                        }
                                    }
                                } else {
                                    CLOGW("WARN(%s[%d]):recordingFrameIndex=%d, timeStamp(%lld) invalid -"
                                        " lastTimeStamp(%lld), systemTime(%lld), recordingStart(%lld), m_recordingBufAvailable(%d)",
                                        __FUNCTION__, __LINE__, m_swVDIS_OutBuf[m_swVDIS_OutQIndex].index, timeStamp,
                                        m_lastRecordingTimeStamp,
                                        systemTime(SYSTEM_TIME_MONOTONIC),
                                        m_recordingStartTimeStamp,
                                        (int)m_recordingBufAvailable[m_swVDIS_OutBuf[m_swVDIS_OutQIndex].index]);
                                        m_recordingTimeStamp[m_swVDIS_OutBuf[m_swVDIS_OutQIndex].index] = 0L;
                                }
                            }
                        }
                    }
                }
                else
#endif /*SUPPORT_SW_VDIS*/
                {
                    nsecs_t timeStamp = (nsecs_t)frame->getTimeStamp();

                    if (m_getRecordingEnabled() == true
                        && m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)) {
                        if (timeStamp <= 0L) {
                            CLOGE("WARN(%s[%d]):timeStamp(%lld) Skip", __FUNCTION__, __LINE__, (long long)timeStamp);
                        } else {
                            if (m_exynosCameraParameters->doCscRecording() == true) {
                                /* get Recording Image buffer */
                                int bufIndex = -2;
                                ExynosCameraBuffer recordingBuffer;
                                ret = m_recordingBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &recordingBuffer);
                                if (ret < 0 || bufIndex < 0) {
                                    if ((++m_recordingFrameSkipCount % 100) == 0) {
                                        CLOGE("ERR(%s[%d]): Recording buffer is not available!! Recording Frames are Skipping(%d frames) (bufIndex=%d)",
                                            __FUNCTION__, __LINE__, m_recordingFrameSkipCount, bufIndex);
                                        m_recordingBufferMgr->printBufferQState();
                                    }
                                } else {
                                    if (m_recordingFrameSkipCount != 0) {
                                        CLOGE("ERR(%s[%d]): Recording buffer is not available!! Recording Frames are Skipped(%d frames) (bufIndex=%d) (recordingQ=%d)",
                                               __FUNCTION__, __LINE__, m_recordingFrameSkipCount, bufIndex, m_recordingQ->getSizeOfProcessQ());
                                        m_recordingFrameSkipCount = 0;
                                        m_recordingBufferMgr->printBufferQState();
                                    }

                                    ret = m_doPrviewToRecordingFunc(PIPE_GSC_VIDEO, buffer, recordingBuffer, timeStamp);
                                    if (ret < 0) {
                                        CLOGW("WARN(%s[%d]):recordingCallback Skip", __FUNCTION__, __LINE__);
                                    }
                                }
                            } else {
                                m_recordingTimeStamp[buffer.index] = timeStamp;

                                if (m_recordingStartTimeStamp == 0) {
                                    m_recordingStartTimeStamp = timeStamp;
                                    CLOGI("INFO(%s[%d]):m_recordingStartTimeStamp=%lld",
                                        __FUNCTION__, __LINE__, (long long)m_recordingStartTimeStamp);
                                }

                                if ((0L < timeStamp)
                                    && (m_lastRecordingTimeStamp < timeStamp)
                                    && (m_recordingStartTimeStamp <= timeStamp)) {
                                    if (m_getRecordingEnabled() == true
                                        && m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)) {
#ifdef CHECK_MONOTONIC_TIMESTAMP
                                        CLOGD("DEBUG(%s[%d]):m_dataCbTimestamp::recordingFrameIndex=%d, recordingTimeStamp=%lld, fd[0]=%d",
                                                __FUNCTION__, __LINE__, buffer.index, (long long)timeStamp, buffer.fd[0]);
#endif
#ifdef DEBUG
                                        CLOGD("DEBUG(%s[%d]): - lastTimeStamp(%lld), systemTime(%lld), recordingStart(%lld)",
                                                __FUNCTION__, __LINE__,
                                                (long long)m_lastRecordingTimeStamp,
                                                (long long)systemTime(SYSTEM_TIME_MONOTONIC),
                                                (long long)m_recordingStartTimeStamp);
#endif

                                        if (m_recordingBufAvailable[buffer.index] == false) {
                                             CLOGW("WARN(%s[%d]):recordingFrameIndex(%d) didn't release yet !!! drop the frame !!! "
                                                 " timeStamp(%lld) m_recordingBufAvailable(%d)",
                                                 __FUNCTION__, __LINE__, buffer.index, (long long)timeStamp, (int)m_recordingBufAvailable[buffer.index]);
                                        } else {
                                            if (m_recordingCallbackHeap != NULL) {
                                                struct VideoNativeHandleMetadata *recordAddrs = NULL;

                                                recordAddrs = (struct VideoNativeHandleMetadata *)m_recordingCallbackHeap->data;
                                                recordAddrs[buffer.index].eType = kMetadataBufferTypeNativeHandleSource;

                                                native_handle* handle = native_handle_create(2, 1);
                                                handle->data[0] = (int32_t) buffer.fd[0];
                                                handle->data[1] = (int32_t) buffer.fd[1];
                                                handle->data[2] = (int32_t) buffer.index;

                                                recordAddrs[buffer.index].pHandle = handle;

                                                m_recordingBufAvailable[buffer.index] = false;
                                                m_lastRecordingTimeStamp = timeStamp;
                                            } else {
                                                CLOGD("INFO(%s[%d]) : m_recordingCallbackHeap is NULL.", __FUNCTION__, __LINE__);
                                            }

                                            if (m_getRecordingEnabled() == true
                                                && m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)) {
#ifdef SAMSUNG_HLV
                                                if (m_HLV) {
                                                    /* Ignore the ERROR .. HLV solution is smart */
                                                    ret = m_ProgramAndProcessHLV(&buffer);
                                                }
#endif
                                                m_dataCbTimestamp(
                                                    timeStamp,
                                                    CAMERA_MSG_VIDEO_FRAME,
                                                    m_recordingCallbackHeap,
                                                    buffer.index,
                                                    m_callbackCookie);
                                            }
                                        }
                                    }
                                } else {
                                    CLOGW("WARN(%s[%d]):recordingFrameIndex=%d, timeStamp(%lld) invalid -"
                                        " lastTimeStamp(%lld), systemTime(%lld), recordingStart(%lld), m_recordingBufAvailable(%d)",
                                        __FUNCTION__, __LINE__, buffer.index, (long long)timeStamp,
                                        (long long)m_lastRecordingTimeStamp,
                                        (long long)systemTime(SYSTEM_TIME_MONOTONIC),
                                        (long long)m_recordingStartTimeStamp,
                                        (int)m_recordingBufAvailable[buffer.index]);
                                        m_recordingTimeStamp[buffer.index] = 0L;
                                }
                            }
                        }
                    }
                }

                ExynosCameraBuffer callbackBuffer;
                ExynosCameraFrame *callbackFrame = NULL;

                callbackFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(PIPE_SCP, frame->getFrameCount());
                frame->getDstBuffer(entity->getPipeId(), &callbackBuffer, m_previewFrameFactory->getNodeType(PIPE_SCP));
                m_copyMetaFrameToFrame(frame, callbackFrame, true, true);

                callbackFrame->setDstBuffer(PIPE_SCP, callbackBuffer);

                if (((m_exynosCameraParameters->getPreviewBufferCount() == NUM_PREVIEW_BUFFERS + NUM_PREVIEW_SPARE_BUFFERS &&
                    m_previewQ->getSizeOfProcessQ() >= 2) ||
                    (m_exynosCameraParameters->getPreviewBufferCount() == NUM_PREVIEW_BUFFERS &&
                    m_previewQ->getSizeOfProcessQ() >= 1)) &&
                    (m_previewThread->isRunning() == true)) {

                    if ((m_getRecordingEnabled() == true) && (m_exynosCameraParameters->doCscRecording() == false)) {
                        CLOGW("WARN(%s[%d]):push frame to previewQ. PreviewQ(%d), PreviewBufferCount(%d)",
                                __FUNCTION__,
                                __LINE__,
                                 m_previewQ->getSizeOfProcessQ(),
                                 m_exynosCameraParameters->getPreviewBufferCount());
                        m_previewQ->pushProcessQ(&callbackFrame);
                    } else {
                        CLOGW("WARN(%s[%d]):Frames are stacked in previewQ. Skip frame. PreviewQ(%d), PreviewBufferCount(%d)",
                            __FUNCTION__, __LINE__,
                            m_previewQ->getSizeOfProcessQ(),
                            m_exynosCameraParameters->getPreviewBufferCount());
                        newFrame->setFrameState(FRAME_STATE_SKIPPED);
                        if (buffer.index >= 0) {
                            /* only apply in the Full OTF of Exynos74xx. */
                            ret = m_scpBufferMgr->cancelBuffer(buffer.index, true);
                            if (ret < 0)
                                CLOGE("ERR(%s[%d]):SCP buffer return fail", __FUNCTION__, __LINE__);
                        }

                        callbackFrame->decRef();
                        m_frameMgr->deleteFrame(callbackFrame);
                        callbackFrame = NULL;
                    }

                } else if((m_exynosCameraParameters->getFastFpsMode() > 1) && (m_exynosCameraParameters->getRecordingHint() == 1)) {
                    m_skipCount++;
#ifdef SAMSUNG_TN_FEATURE
                    short skipInterval = (m_exynosCameraParameters->getFastFpsMode() - 1) * 2;

                    if((m_exynosCameraParameters->getShotMode() == SHOT_MODE_SEQUENCE && (m_skipCount%4 != 0))
                        || (m_exynosCameraParameters->getShotMode() != SHOT_MODE_SEQUENCE && (m_skipCount % skipInterval) > 0)) {
                        CLOGV("INFO(%s[%d]):fast fps mode skip frame(%d) ", __FUNCTION__, __LINE__,m_skipCount);
                        newFrame->setFrameState(FRAME_STATE_SKIPPED);
                        if (buffer.index >= 0) {
                            /* only apply in the Full OTF of Exynos74xx. */
                            ret = m_scpBufferMgr->cancelBuffer(buffer.index, true);
                            if (ret < 0)
                                CLOGE("ERR(%s[%d]):SCP buffer return fail", __FUNCTION__, __LINE__);
                        }

                        callbackFrame->decRef();
                        m_frameMgr->deleteFrame(callbackFrame);
                        callbackFrame = NULL;
                    } else
#endif
                    {
                        CLOGV("INFO(%s[%d]):push frame to previewQ", __FUNCTION__, __LINE__);
                        m_previewQ->pushProcessQ(&callbackFrame);
                    }
                } else {
                    if (m_getRecordingEnabled() == true) {
                        CLOGV("INFO(%s[%d]):push frame to previewQ", __FUNCTION__, __LINE__);
                        m_previewQ->pushProcessQ(&callbackFrame);
                        m_longExposurePreview = false;
                    } else if (m_exynosCameraParameters->getCaptureExposureTime() > 0 &&
                            (m_meta_shot->shot.dm.sensor.exposureTime / 1000) > CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT) {
                        CLOGD("INFO(%s[%d]):manual exposure capture mode. %lld",
                            __FUNCTION__, __LINE__, (long long)m_meta_shot->shot.dm.sensor.exposureTime);
                        newFrame->setFrameState(FRAME_STATE_SKIPPED);
                        if (buffer.index >= 0) {
                            /* only apply in the Full OTF of Exynos74xx. */
                            ret = m_scpBufferMgr->cancelBuffer(buffer.index, true);
                            if (ret < 0)
                                CLOGE("ERR(%s[%d]):SCP buffer return fail", __FUNCTION__, __LINE__);
                        }

                        callbackFrame->decRef();
                        m_frameMgr->deleteFrame(callbackFrame);
                        callbackFrame = NULL;
                        m_longExposurePreview = true;
                    } else {
#ifdef OIS_CAPTURE
                        ExynosCameraActivitySpecialCapture *m_sCaptureMgr = NULL;
                        unsigned int OISFcount = 0;
                        unsigned int fliteFcount = 0;
                        bool OISCapture_activated = false;

                        m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();
                        OISFcount = m_sCaptureMgr->getOISCaptureFcount();
                        OISFcount += OISCAPTURE_DELAY;
                        fliteFcount = m_meta_shot->shot.dm.request.frameCount;

                        if(OISFcount > OISCAPTURE_DELAY) {
                            if (OISFcount == fliteFcount
#ifdef SAMSUNG_LLS_DEBLUR
                                && m_exynosCameraParameters->getLDCaptureMode() == MULTI_SHOT_MODE_NONE
#endif
                            ) {
                                OISCapture_activated = true;
                            }
#ifdef SAMSUNG_LLS_DEBLUR
                            else if (m_exynosCameraParameters->getLDCaptureMode() > MULTI_SHOT_MODE_NONE
                                && OISFcount <= fliteFcount
                                && fliteFcount < OISFcount + m_exynosCameraParameters->getLDCaptureCount()) {
                                OISCapture_activated = true;
                            }
#endif
                        }

                        if((m_exynosCameraParameters->getSeriesShotCount() == 0
#ifdef LLS_REPROCESSING
                            || m_exynosCameraParameters->getSeriesShotMode() == SERIES_SHOT_MODE_LLS
#endif
                            ) && OISCapture_activated) {
                            CLOGD("INFO(%s[%d]):OIS capture mode. Skip frame. FliteFrameCount(%d) ", __FUNCTION__, __LINE__,fliteFcount);
                            newFrame->setFrameState(FRAME_STATE_SKIPPED);
                            if (buffer.index >= 0) {
                                /* only apply in the Full OTF of Exynos74xx. */
                                ret = m_scpBufferMgr->cancelBuffer(buffer.index, true);
                                if (ret < 0)
                                    CLOGE("ERR(%s[%d]):SCP buffer return fail", __FUNCTION__, __LINE__);
                            }

                            callbackFrame->decRef();
                            m_frameMgr->deleteFrame(callbackFrame);
                            callbackFrame = NULL;
                        } else
#endif
                        {
                            CLOGV("INFO(%s[%d]):push frame to previewQ", __FUNCTION__, __LINE__);
                            m_previewQ->pushProcessQ(&callbackFrame);
                        }
                        m_longExposurePreview = false;
                    }
                }
            }
            newFrame->decRef();
            m_frameMgr->deleteFrame(newFrame);
            //m_mainSetupQ[INDEX(entity->getPipeId())]->pushProcessQ(&newFrame);
            CLOGV("DEBUG(%s[%d]):SCP done HAL-frameCount(%d)", __FUNCTION__, __LINE__, frame->getFrameCount());
        } else {
            CLOGV("DEBUG(%s[%d]):SCP droped - SCP buffer is not ready HAL-frameCount(%d)", __FUNCTION__, __LINE__, frame->getFrameCount());

            ret = frame->getDstBuffer(entity->getPipeId(), &buffer, m_previewFrameFactory->getNodeType(PIPE_SCP));
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                return ret;
            }

            if (buffer.index >= 0) {
                ret = m_scpBufferMgr->cancelBuffer(buffer.index, true);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):SCP buffer return fail", __FUNCTION__, __LINE__);
                }
            }

            /* For debug */
            /* m_previewFrameFactory->dump(); */

            /* Comment out, because it included ISP */
            /*
            newFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(PIPE_SCP, frame->getFrameCount());
            newFrame->setDstBuffer(PIPE_SCP, buffer);
            newFrame->setFrameState(FRAME_STATE_SKIPPED);

            m_mainSetupQ[INDEX(entity->getPipeId())]->pushProcessQ(&newFrame);
            */
        }
        break;
    default:
        break;
    }

entity_state_complete:

    entity = curentity;

    ret = frame->setEntityState(entity->getPipeId(), ENTITY_STATE_COMPLETE);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setEntityState fail, pipeId(%d), state(%d), ret(%d)",
            __FUNCTION__, __LINE__, entity->getPipeId(), ENTITY_STATE_COMPLETE, ret);
        return ret;
    }

    if (frame->isComplete() == true) {
        ret = m_removeFrameFromList(&m_processList, frame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):remove frame from processList fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }

        frame->decRef();
        m_frameMgr->deleteFrame(frame);
    }


    return NO_ERROR;
}

status_t ExynosCamera::m_handlePreviewFrameFrontDual(ExynosCameraFrame *frame)
{
    int ret = 0;
    ExynosCameraFrameEntity *entity = NULL;
    ExynosCameraFrameEntity *curentity = NULL;
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraFrame *fdFrame = NULL;

    ExynosCameraBuffer buffer;
    ExynosCameraBuffer ispcBuffer;
    int pipeID = 0;
    int dummyPipeId = -1;
    /* to handle the high speed frame rate */
    bool skipPreview = false;
    int ratio = 1;
    uint32_t minFps = 0, maxFps = 0;
    uint32_t dispFps = EXYNOS_CAMERA_PREVIEW_FPS_REFERENCE;
    uint32_t fvalid = 0;
    uint32_t fcount = 0;
    uint32_t skipCount = 0;
    struct camera2_stream *shot_stream = NULL;
    ExynosCameraBuffer resultBuffer;
    camera2_node_group node_group_info_isp;
    int32_t reprocessingBayerMode = m_exynosCameraParameters->getReprocessingBayerMode();
    int ispDstBufferIndex = -1;

    entity = frame->getFrameDoneFirstEntity();
    if (entity == NULL) {
        CLOGE("ERR(%s[%d]):current entity is NULL frame(%d)", __FUNCTION__, __LINE__, frame->getFrameCount());
        /* TODO: doing exception handling */
        return true;
    }

    curentity = entity;

    pipeID = entity->getPipeId();


    /* TODO: remove hard coding */
    switch(INDEX(entity->getPipeId())) {
    case PIPE_3AA_ISP:
        break;
    case PIPE_3AC:
    case PIPE_FLITE:
        m_debugFpsCheck(entity->getPipeId());

        ret = frame->getDstBuffer(entity->getPipeId(), &buffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                __FUNCTION__, __LINE__, entity->getPipeId(), ret);
            return ret;
        }

        m_setupEntity(PIPE_3AA, frame, &buffer, NULL);
        m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, PIPE_3AA);
        m_previewFrameFactory->pushFrameToPipe(&frame, PIPE_3AA);

        break;
    case PIPE_3AA:
        m_debugFpsCheck(entity->getPipeId());

        ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getSrcBuffer fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, entity->getPipeId(), ret);
            return ret;
        }

        if (buffer.index >= 0) {
            ret = m_putBuffers(m_bayerBufferMgr, buffer.index);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
            }
        }

        newFrame = m_frameMgr->createFrame(m_exynosCameraParameters, 0);
        m_mainSetupQ[INDEX(m_getBayerPipeId())]->pushProcessQ(&newFrame);

        CLOGV("DEBUG(%s[%d]):3AA_ISP frameCount(%d) frame.Count(%d)",
                __FUNCTION__, __LINE__,
                getMetaDmRequestFrameCount((struct camera2_shot_ext *)buffer.addr[1]),
                frame->getFrameCount());

    case PIPE_SCP:
        if (m_exynosCameraParameters->getZoomPreviewWIthScaler()== true) {
            CLOGV("INFO(%s[%d]): zoom preview with csc pipeId(%d) frame(%d)", __FUNCTION__, __LINE__, entity->getPipeId(), frame->getFrameCount());
            m_zoomPreviwWithCscQ->pushProcessQ(&frame);
            break;
        } else {
            CLOGV("INFO(%s[%d]): zoom preview without csc pipeId(%d)", __FUNCTION__, __LINE__, entity->getPipeId());
        }

    case PIPE_GSC:
        if (m_exynosCameraParameters->getZoomPreviewWIthScaler()== true) {
            CLOGV("INFO(%s[%d]): zoom preview with csc pipeId(%d) frame(%d)",
                __FUNCTION__, __LINE__, entity->getPipeId(), frame->getFrameCount());

            dummyPipeId = (m_exynosCameraParameters->getTpuEnabledMode())? PIPE_DIS : PIPE_3AA;
            ret = m_syncPrviewWithCSC(dummyPipeId, PIPE_GSC, frame);
            if (ret < 0) {
                CLOGW("WARN(%s[%d]):m_syncPrviewWithCSC failed frame(%d)", __FUNCTION__, __LINE__, frame->getFrameCount());
            }

            entity = frame->searchEntityByPipeId(dummyPipeId);
            if (entity == NULL) {
                CLOGE("ERR(%s[%d]):searchEntityByPipeId failed pipe(%d) frame(%d)",
                    __FUNCTION__, __LINE__, dummyPipeId, frame->getFrameCount());
                goto entity_state_complete;
            }
        } else {
            CLOGV("INFO(%s[%d]): zoom preview with without csc pipeId(%d) frame(%d)",
                __FUNCTION__, __LINE__, entity->getPipeId(), frame->getFrameCount());
        }

        buffer.index = -1;

        if (frame->getRequest(PIPE_ISPC) == true) {
            ret = frame->getDstBuffer(entity->getPipeId(), &buffer, m_previewFrameFactory->getNodeType(PIPE_ISPC));
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):getSrcBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, entity->getPipeId(), ret);
            }
        }

        if (buffer.index >= 0) {
            if (frame->getRequest(PIPE_ISPC) == true) {
                entity_buffer_state_t bufferstate = ENTITY_BUFFER_STATE_NOREQ;
                ret = frame->getDstBufferState(entity->getPipeId(), &bufferstate, m_previewFrameFactory->getNodeType(PIPE_ISPC));
                if (ret == NO_ERROR && bufferstate != ENTITY_BUFFER_STATE_ERROR) {
                    ret = m_sccCaptureSelector->manageFrameHoldList(frame, entity->getPipeId(), false, m_previewFrameFactory->getNodeType(PIPE_ISPC));
                    if (ret < 0) {
                        CLOGE("ERR(%s[%d]):manageFrameHoldList fail", __FUNCTION__, __LINE__);
                    }
                } else {
                    ret = m_putBuffers(m_sccBufferMgr, buffer.index);
                    if (ret < 0) {
                        CLOGE("ERR(%s[%d]):m_putBuffers(m_bayerBufferMgr, %d) fail", __FUNCTION__, __LINE__, buffer.index);
                    }
                }
            }
        }

        frame->setMetaDataEnable(true);

        if (entity->getDstBufState() == ENTITY_BUFFER_STATE_ERROR) {
            if (frame->getFrameState() != FRAME_STATE_SKIPPED) {
                ret = frame->getDstBuffer(entity->getPipeId(), &buffer, m_previewFrameFactory->getNodeType(PIPE_SCP));
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                            __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                    return ret;
                }

                if (buffer.index >= 0) {
                    ret = m_scpBufferMgr->cancelBuffer(buffer.index);
                    if (ret < 0)
                        CLOGE("ERR(%s[%d]):SCP buffer return fail", __FUNCTION__, __LINE__);
                }
                /* For debug */
                /* m_previewFrameFactory->dump(); */

                /* Comment out, because it included ISP */
                /*
                   newFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(PIPE_SCP, frame->getFrameCount());
                   newFrame->setDstBuffer(PIPE_SCP, buffer);
                   newFrame->setFrameState(FRAME_STATE_SKIPPED);

                   m_mainSetupQ[INDEX(entity->getPipeId())]->pushProcessQ(&newFrame);
                 */
            }
            CLOGV("DEBUG(%s[%d]):SCP done HAL-frameCount(%d)", __FUNCTION__, __LINE__, frame->getFrameCount());
        } else if (entity->getDstBufState() == ENTITY_BUFFER_STATE_COMPLETE) {
            m_debugFpsCheck(entity->getPipeId());

            ret = frame->getDstBuffer(entity->getPipeId(), &buffer, m_previewFrameFactory->getNodeType(PIPE_SCP));

            if (ret < 0) {
                CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                return ret;
            }

            /* TO DO : skip frame for HDR */
            shot_stream = (struct camera2_stream *)buffer.addr[2];

            if (shot_stream != NULL) {
                getStreamFrameValid(shot_stream, &fvalid);
                getStreamFrameCount(shot_stream, &fcount);
            } else {
                CLOGE("ERR(%s[%d]):shot_stream is NULL", __FUNCTION__, __LINE__);
                fvalid = false;
                fcount = 0;
            }

            /* drop preview frame if lcd supported frame rate < scp frame rate */
            frame->getFpsRange(&minFps, &maxFps);
            if (dispFps < maxFps) {
                ratio = (int)((maxFps * 10 / dispFps) / 10);
                m_displayPreviewToggle = (m_displayPreviewToggle + 1) % ratio;
                skipPreview = (m_displayPreviewToggle == 0) ? true : false;
#ifdef DEBUG
                CLOGE("DEBUG(%s[%d]):preview frame skip! frameCount(%d) (m_displayPreviewToggle=%d, maxFps=%d, dispFps=%d, ratio=%d, skipPreview=%d)",
                        __FUNCTION__, __LINE__, frame->getFrameCount(), m_displayPreviewToggle, maxFps, dispFps, ratio, (int)skipPreview);
#endif
            }

            newFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(PIPE_SCP, frame->getFrameCount());

            newFrame->setDstBuffer(PIPE_SCP, buffer);

            m_exynosCameraParameters->getFrameSkipCount(&m_frameSkipCount);
            if (m_frameSkipCount > 0) {
                CLOGD("INFO(%s[%d]):Skip frame for frameSkipCount(%d) buffer.index(%d)",
                        __FUNCTION__, __LINE__, m_frameSkipCount, buffer.index);

                newFrame->setFrameState(FRAME_STATE_SKIPPED);
                if (buffer.index >= 0) {
                    ret = m_scpBufferMgr->cancelBuffer(buffer.index);
                    if (ret < 0)
                        CLOGE("ERR(%s[%d]):SCP buffer return fail", __FUNCTION__, __LINE__);
                }
            } else {
                if (m_skipReprocessing == true)
                    m_skipReprocessing = false;
                nsecs_t timeStamp = (nsecs_t)frame->getTimeStamp();
                if (m_getRecordingEnabled() == true
                    && m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)) {
                    if (timeStamp <= 0L) {
                        CLOGE("WARN(%s[%d]):timeStamp(%lld) Skip", __FUNCTION__, __LINE__, (long long)timeStamp);
                    } else {
                        if (m_exynosCameraParameters->doCscRecording() == true) {
                            /* get Recording Image buffer */
                            int bufIndex = -2;
                            ExynosCameraBuffer recordingBuffer;
                            ret = m_recordingBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &recordingBuffer);
                            if (ret < 0 || bufIndex < 0) {
                                if ((++m_recordingFrameSkipCount % 100) == 0) {
                                    CLOGE("ERR(%s[%d]): Recording buffer is not available!! Recording Frames are Skipping(%d frames) (bufIndex=%d)",
                                            __FUNCTION__, __LINE__, m_recordingFrameSkipCount, bufIndex);
                                    m_recordingBufferMgr->printBufferQState();
                                }
                            } else {
                                if (m_recordingFrameSkipCount != 0) {
                                    CLOGE("ERR(%s[%d]): Recording buffer is not available!! Recording Frames are Skipped(%d frames) (bufIndex=%d) (recordingQ=%d)",
                                            __FUNCTION__, __LINE__, m_recordingFrameSkipCount, bufIndex, m_recordingQ->getSizeOfProcessQ());
                                    m_recordingFrameSkipCount = 0;
                                    m_recordingBufferMgr->printBufferQState();
                                }

                                ret = m_doPrviewToRecordingFunc(PIPE_GSC_VIDEO, buffer, recordingBuffer, timeStamp);
                                if (ret < 0) {
                                    CLOGW("WARN(%s[%d]):recordingCallback Skip", __FUNCTION__, __LINE__);
                                }
                            }
                        } else {
                            m_recordingTimeStamp[buffer.index] = timeStamp;

                            if (m_recordingStartTimeStamp == 0) {
                                m_recordingStartTimeStamp = timeStamp;
                                CLOGI("INFO(%s[%d]):m_recordingStartTimeStamp=%lld",
                                        __FUNCTION__, __LINE__, (long long)m_recordingStartTimeStamp);
                            }

                            if ((0L < timeStamp)
                                && (m_lastRecordingTimeStamp < timeStamp)
                                && (m_recordingStartTimeStamp <= timeStamp)) {
                                if (m_getRecordingEnabled() == true
                                        && m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)) {
#ifdef CHECK_MONOTONIC_TIMESTAMP
                                    CLOGD("DEBUG(%s[%d]):m_dataCbTimestamp::recordingFrameIndex=%d, recordingTimeStamp=%lld, fd[0]=%d",
                                            __FUNCTION__, __LINE__, buffer.index, (long long)timeStamp, buffer.fd[0]);
#endif
#ifdef DEBUG
                                    CLOGD("DEBUG(%s[%d]): - lastTimeStamp(%lld), systemTime(%lld), recordingStart(%lld)",
                                            __FUNCTION__, __LINE__,
                                            (long long)m_lastRecordingTimeStamp,
                                            (long long)systemTime(SYSTEM_TIME_MONOTONIC),
                                            (long long)m_recordingStartTimeStamp);
#endif

                                    if (m_recordingBufAvailable[buffer.index] == false) {
                                        CLOGW("WARN(%s[%d]):recordingFrameIndex(%d) didn't release yet !!! drop the frame !!! "
                                                " timeStamp(%lld) m_recordingBufAvailable(%d)",
                                                __FUNCTION__, __LINE__, buffer.index, (long long)timeStamp, (int)m_recordingBufAvailable[buffer.index]);
                                    } else {
                                        struct VideoNativeHandleMetadata *recordAddrs = NULL;

                                        recordAddrs = (struct VideoNativeHandleMetadata *)m_recordingCallbackHeap->data;
                                        recordAddrs[buffer.index].eType = kMetadataBufferTypeNativeHandleSource;

                                        native_handle* handle = native_handle_create(2, 1);
                                        handle->data[0] = (int32_t) buffer.fd[0];
                                        handle->data[1] = (int32_t) buffer.fd[1];
                                        handle->data[2] = (int32_t) buffer.index;

                                        recordAddrs[buffer.index].pHandle = handle;

                                        m_recordingBufAvailable[buffer.index] = false;
                                        m_lastRecordingTimeStamp = timeStamp;

                                        m_dataCbTimestamp(
                                                timeStamp,
                                                CAMERA_MSG_VIDEO_FRAME,
                                                m_recordingCallbackHeap,
                                                buffer.index,
                                                m_callbackCookie);
                                    }
                                }
                            } else {
                                CLOGW("WARN(%s[%d]):recordingFrameIndex=%d, timeStamp(%lld) invalid -"
                                    " lastTimeStamp(%lld), systemTime(%lld), recordingStart(%lld), m_recordingBufAvailable(%d)",
                                    __FUNCTION__, __LINE__, buffer.index, (long long)timeStamp,
                                    (long long)m_lastRecordingTimeStamp,
                                    (long long)systemTime(SYSTEM_TIME_MONOTONIC),
                                    (long long)m_recordingStartTimeStamp,
                                    (int)m_recordingBufAvailable[buffer.index]);
                                m_recordingTimeStamp[buffer.index] = 0L;
                            }
                        }
                    }
                }
                ExynosCameraBuffer callbackBuffer;
                ExynosCameraFrame *callbackFrame = NULL;
                callbackFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(PIPE_SCP, frame->getFrameCount());
                frame->getDstBuffer(entity->getPipeId(), &callbackBuffer, m_previewFrameFactory->getNodeType(PIPE_SCP));
                m_copyMetaFrameToFrame(frame, callbackFrame, true, true);

                callbackFrame->setDstBuffer(PIPE_SCP, callbackBuffer);

                if (((m_exynosCameraParameters->getPreviewBufferCount() == NUM_PREVIEW_BUFFERS + NUM_PREVIEW_SPARE_BUFFERS &&
                    m_previewQ->getSizeOfProcessQ() >= 2) ||
                    (m_exynosCameraParameters->getPreviewBufferCount() == NUM_PREVIEW_BUFFERS &&
                    m_previewQ->getSizeOfProcessQ() >= 1)) &&
                    (m_previewThread->isRunning() == true)) {
                    CLOGW("WARN(%s[%d]):Frames are stacked in previewQ. Skip frame. PreviewQ(%d), PreviewBufferCount(%d)",
                            __FUNCTION__, __LINE__,
                            m_previewQ->getSizeOfProcessQ(),
                            m_exynosCameraParameters->getPreviewBufferCount());

                    newFrame->setFrameState(FRAME_STATE_SKIPPED);
                    if (buffer.index >= 0) {
                        ret = m_scpBufferMgr->cancelBuffer(buffer.index);
                        if (ret < 0)
                            CLOGE("ERR(%s[%d]):SCP buffer return fail", __FUNCTION__, __LINE__);
                    }

                    callbackFrame->decRef();
                    m_frameMgr->deleteFrame(callbackFrame);
                    callbackFrame = NULL;
                } else if((m_exynosCameraParameters->getFastFpsMode() == 2) && (m_exynosCameraParameters->getRecordingHint() == 1)) {
                    m_skipCount++;
#ifdef SAMSUNG_DOF
                    if((m_exynosCameraParameters->getShotMode() == SHOT_MODE_SEQUENCE && m_skipCount%4 != 0)
                        || (m_exynosCameraParameters->getShotMode() != SHOT_MODE_SEQUENCE && m_skipCount%2 == 1)) {
                        CLOGV("INFO(%s[%d]):fast fps mode skip frame(%d) ", __FUNCTION__, __LINE__,m_skipCount);
                        newFrame->setFrameState(FRAME_STATE_SKIPPED);
                        if (buffer.index >= 0) {
                            ret = m_scpBufferMgr->cancelBuffer(buffer.index);
                            if (ret < 0)
                                CLOGE("ERR(%s[%d]):SCP buffer return fail", __FUNCTION__, __LINE__);
                        }
                        callbackFrame->decRef();
                        m_frameMgr->deleteFrame(callbackFrame);
                        callbackFrame = NULL;
                    } else
#endif
                    {
                        CLOGV("INFO(%s[%d]):push frame to previewQ", __FUNCTION__, __LINE__);
                        m_previewQ->pushProcessQ(&callbackFrame);
                    }
                } else {
                    CLOGV("INFO(%s[%d]):push frame to previewQ", __FUNCTION__, __LINE__);
                    m_previewQ->pushProcessQ(&callbackFrame);
                }
            }
            newFrame->decRef();
            m_frameMgr->deleteFrame(newFrame);
            //m_mainSetupQ[INDEX(entity->getPipeId())]->pushProcessQ(&newFrame);
            CLOGV("DEBUG(%s[%d]):SCP done HAL-frameCount(%d)", __FUNCTION__, __LINE__, frame->getFrameCount());
        } else {
            CLOGV("DEBUG(%s[%d]):SCP droped - SCP buffer is not ready HAL-frameCount(%d)", __FUNCTION__, __LINE__, frame->getFrameCount());

            ret = frame->getDstBuffer(entity->getPipeId(), &buffer, m_previewFrameFactory->getNodeType(PIPE_SCP));
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                return ret;
            }

            if (buffer.index >= 0) {
                ret = m_scpBufferMgr->cancelBuffer(buffer.index);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):SCP buffer return fail", __FUNCTION__, __LINE__);
                }
            }
        }
        break;
    default:
        break;
    }

entity_state_complete:

    entity = curentity;

    ret = frame->setEntityState(entity->getPipeId(), ENTITY_STATE_COMPLETE);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setEntityState fail, pipeId(%d), state(%d), ret(%d)",
            __FUNCTION__, __LINE__, entity->getPipeId(), ENTITY_STATE_COMPLETE, ret);
        return ret;
    }

    if (frame->isComplete() == true) {
        ret = m_removeFrameFromList(&m_processList, frame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):remove frame from processList fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }

        frame->decRef();
        m_frameMgr->deleteFrame(frame);
    }

    return NO_ERROR;
}

bool ExynosCamera::m_frameFactoryInitThreadFunc(void)
{
    CLOGI("INFO(%s[%d]): -IN-", __FUNCTION__, __LINE__);

#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif
    bool loop = false;
    status_t ret = NO_ERROR;

    ExynosCameraFrameFactory *framefactory = NULL;

    ret = m_frameFactoryQ->waitAndPopProcessQ(&framefactory);
    if (m_flagThreadStop == true) {
        CLOGI("INFO(%s[%d]):m_flagThreadStop(%d)", __FUNCTION__, __LINE__, m_flagThreadStop);
        return false;
    }

    if (ret < 0) {
        CLOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        goto func_exit;
    }

    if (framefactory == NULL) {
        CLOGE("ERR(%s[%d]):framefactory is NULL", __FUNCTION__, __LINE__);
        goto func_exit;
    }

    if (framefactory->isCreated() == false) {
        CLOGD("DEBUG(%s[%d]):framefactory create", __FUNCTION__, __LINE__);
        framefactory->create(false);
    } else {
        CLOGD("DEBUG(%s[%d]):framefactory already create", __FUNCTION__, __LINE__);
    }

func_exit:
    if (0 < m_frameFactoryQ->getSizeOfProcessQ()) {
        if (m_previewEnabled == true) {
            loop = true;
        } else {
            CLOGW("WARN(%s[%d]):Sudden stopPreview. so, stop making frameFactory (m_frameFactoryQ->getSizeOfProcessQ() : %d)",
                __FUNCTION__, __LINE__, m_frameFactoryQ->getSizeOfProcessQ());
            loop = false;
        }
    }

    CLOGI("INFO(%s[%d]):m_framefactoryThread end loop(%d) m_frameFactoryQ(%d), m_previewEnabled(%d)",
        __FUNCTION__, __LINE__, loop, m_frameFactoryQ->getSizeOfProcessQ(), m_previewEnabled);

    CLOGI("INFO(%s[%d]): -OUT-", __FUNCTION__, __LINE__);

    return loop;
}

bool ExynosCamera::m_previewThreadFunc(void)
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif

    int  ret  = 0;
    bool loop = true;
    int  pipeId    = 0;
    int  pipeIdCsc = 0;
    int  maxbuffers   = 0;
#ifdef USE_CAMERA_PREVIEW_FRAME_SCHEDULER
    uint32_t curMinFps = 0;
    uint32_t curMaxFps = 0;
#endif
    ExynosCameraBuffer buffer;
    ExynosCameraFrame  *frame = NULL;
    nsecs_t timeStamp = 0;
    int frameCount = -1;
    frame_queue_t *previewQ;

#ifdef USE_CAMERA_PREVIEW_FRAME_SCHEDULER
    m_exynosCameraParameters->getPreviewFpsRange(&curMinFps, &curMaxFps);

    /* Check the Slow/Fast Motion Scenario - sensor : 120fps, preview : 60fps */
    if(((curMinFps == 120) && (curMaxFps == 120))
         || ((curMinFps == 240) && (curMaxFps == 240))) {
        CLOGV("(%s[%d]) : Change PreviewDuration from (%d,%d) to (60000, 60000)", __FUNCTION__, __LINE__, curMinFps, curMaxFps);
        curMinFps = 60;
        curMaxFps = 60;
    }
#endif

    pipeId    = PIPE_SCP;
    pipeIdCsc = PIPE_GSC;

    previewQ = m_previewQ;

    CLOGV("INFO(%s[%d]):wait previewQ", __FUNCTION__, __LINE__);
    ret = previewQ->waitAndPopProcessQ(&frame);
    if (m_flagThreadStop == true) {
        CLOGI("INFO(%s[%d]):m_flagThreadStop(%d)", __FUNCTION__, __LINE__, m_flagThreadStop);

        if (frame != NULL) {
            frame->decRef();
            m_frameMgr->deleteFrame(frame);
            frame = NULL;
        }

        return false;
    }
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (frame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    CLOGV("INFO(%s[%d]):get frame from previewQ", __FUNCTION__, __LINE__);
    timeStamp = (nsecs_t)frame->getTimeStamp();
    frameCount = frame->getFrameCount();
    ret = frame->getDstBuffer(pipeId, &buffer);

    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getDstBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        goto func_exit;
    }

    /* ------------- frome here "frame" cannot use ------------- */
    CLOGV("INFO(%s[%d]):push frame to previewReturnQ", __FUNCTION__, __LINE__);
    if(m_exynosCameraParameters->increaseMaxBufferOfPreview()) {
        maxbuffers = m_exynosCameraParameters->getPreviewBufferCount();
    } else {
        maxbuffers = (int)m_exynosconfig->current->bufInfo.num_preview_buffers;
    }

    if (buffer.index < 0 || buffer.index >= maxbuffers ) {
        CLOGE("ERR(%s[%d]):Out of Index! (Max: %d, Index: %d)", __FUNCTION__, __LINE__, maxbuffers, buffer.index);
        goto func_exit;
    }

    CLOGV("INFO(%s[%d]):m_previewQ->getSizeOfProcessQ(%d) m_scpBufferMgr->getNumOfAvailableBuffer(%d)", __FUNCTION__, __LINE__,
        previewQ->getSizeOfProcessQ(), m_scpBufferMgr->getNumOfAvailableBuffer());

    /* Prevent displaying unprocessed beauty images in beauty shot. */
    if ((m_exynosCameraParameters->getShotMode() == SHOT_MODE_BEAUTY_FACE)
#ifdef LLS_CAPTURE
        || (m_exynosCameraParameters->getLLSOn() == true && getCameraId() == CAMERA_ID_FRONT)
#endif
        ) {
        if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME) &&
            checkBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE)) {
            CLOGV("INFO(%s[%d]):skip the preview callback and the preview display while compressed callback.",
                    __FUNCTION__, __LINE__);
            ret = m_scpBufferMgr->cancelBuffer(buffer.index);
            goto func_exit;
        }
    }

    if ((m_previewWindow == NULL) ||
        (m_getRecordingEnabled() == true) ||
        (m_exynosCameraParameters->getPreviewBufferCount() == NUM_PREVIEW_BUFFERS + NUM_PREVIEW_SPARE_BUFFERS &&
        m_scpBufferMgr->getNumOfAvailableAndNoneBuffer() > 4 &&
        previewQ->getSizeOfProcessQ() < 2) ||
#ifdef SUPPORT_SW_VDIS
        (m_exynosCameraParameters->getPreviewBufferCount() == NUM_VDIS_BUFFERS &&
        m_scpBufferMgr->getNumOfAvailableAndNoneBuffer() > 2 &&
        previewQ->getSizeOfProcessQ() < 1) ||
#endif /*SUPPORT_SW_VDIS*/
        (m_exynosCameraParameters->getPreviewBufferCount() == NUM_PREVIEW_BUFFERS &&
        m_scpBufferMgr->getNumOfAvailableAndNoneBuffer() > 2 &&
        previewQ->getSizeOfProcessQ() < 1)) {
        if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME) &&
            m_highResolutionCallbackRunning == false) {
            ExynosCameraBuffer previewCbBuffer;

            ret = m_setPreviewCallbackBuffer();
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):m_setPreviewCallback Buffer fail", __FUNCTION__, __LINE__);
                return ret;
            }

            int bufIndex = -2;
            m_previewCallbackBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &previewCbBuffer);

            ExynosCameraFrame *newFrame = NULL;

            newFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(pipeIdCsc);
            if (newFrame == NULL) {
                CLOGE("ERR(%s):newFrame is NULL", __FUNCTION__);
                return UNKNOWN_ERROR;
            }

            m_copyMetaFrameToFrame(frame, newFrame, true, true);

            ret = m_doPreviewToCallbackFunc(pipeIdCsc, newFrame, buffer, previewCbBuffer);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):m_doPreviewToCallbackFunc fail", __FUNCTION__, __LINE__);
                m_previewCallbackBufferMgr->putBuffer(bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
                m_scpBufferMgr->cancelBuffer(buffer.index);
                goto func_exit;
            } else {
                if (m_exynosCameraParameters->getCallbackNeedCopy2Rendering() == true) {
                    ret = m_doCallbackToPreviewFunc(pipeIdCsc, frame, previewCbBuffer, buffer);
                    if (ret < 0) {
                        CLOGE("ERR(%s[%d]):m_doCallbackToPreviewFunc fail", __FUNCTION__, __LINE__);
                        m_previewCallbackBufferMgr->putBuffer(bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
                        m_scpBufferMgr->cancelBuffer(buffer.index);
                        goto func_exit;
                    }
                }
            }

            if (newFrame != NULL) {
                newFrame->decRef();
                m_frameMgr->deleteFrame(newFrame);
                newFrame = NULL;
            }

            m_previewCallbackBufferMgr->putBuffer(bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
        }

        if (m_previewWindow != NULL) {
            if (timeStamp > 0L) {
                m_previewWindow->set_timestamp(m_previewWindow, (int64_t)timeStamp);
            } else {
                uint32_t fcount = 0;
                getStreamFrameCount((struct camera2_stream *)buffer.addr[2], &fcount);
                CLOGW("WRN(%s[%d]): frameCount(%d)(%d), Invalid timeStamp(%lld)",
                        __FUNCTION__, __LINE__,
                        frameCount,
                        fcount,
                        (long long)timeStamp);
            }
        }

#ifdef FIRST_PREVIEW_TIME_CHECK
        if (m_flagFirstPreviewTimerOn == true) {
            ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
            m_firstPreviewTimer.stop();
            m_flagFirstPreviewTimerOn = false;

            CLOGD("DEBUG(%s[%d]):m_firstPreviewTimer stop", __FUNCTION__, __LINE__);

            CLOGD("DEBUG(%s[%d]):============= First Preview time ==================", "m_printExynosCameraInfo", __LINE__);
            CLOGD("DEBUG(%s[%d]):= startPreview ~ first frame  : %d msec", "m_printExynosCameraInfo", __LINE__, (int)m_firstPreviewTimer.durationMsecs());
            CLOGD("DEBUG(%s[%d]):===================================================", "m_printExynosCameraInfo", __LINE__);
            autoFocusMgr->displayAFInfo();
        }
#endif

#ifdef SAMSUNG_SENSOR_LISTENER
#ifdef SAMSUNG_HRM
        if(m_uv_rayHandle != NULL) {
            ret = sensor_listener_get_data(m_uv_rayHandle, ST_UV_RAY, &m_uv_rayListenerData, false);
            if(ret < 0) {
                CLOGW("WRN(%s[%d]:%d):skip the HRM data", __FUNCTION__, __LINE__, ret);
            } else {
                m_exynosCameraParameters->m_setHRM(m_uv_rayListenerData.uv_ray.ir_data, m_uv_rayListenerData.uv_ray.status);
            }
        }
#endif

#ifdef SAMSUNG_LIGHT_IR
        if(m_light_irHandle != NULL) {
            ret = sensor_listener_get_data(m_light_irHandle, ST_LIGHT_IR, &m_light_irListenerData, false);
            if(ret < 0) {
                CLOGW("WRN(%s[%d]:%d):skip the Light IR data", __FUNCTION__, __LINE__, ret);
            } else {
                m_exynosCameraParameters->m_setLight_IR(m_light_irListenerData);
            }
        }
#endif

#ifdef SAMSUNG_GYRO
        if(m_gyroHandle != NULL) {
            ret = sensor_listener_get_data(m_gyroHandle, ST_GYROSCOPE, &m_gyroListenerData, false);
            if(ret < 0) {
                CLOGW("WRN(%s[%d]:%d):skip the Gyro data", __FUNCTION__, __LINE__, ret);
            } else {
                m_exynosCameraParameters->m_setGyro(m_gyroListenerData);
            }
        }
#endif
#endif /* SAMSUNG_SENSOR_LISTENER */

#ifdef SUPPORT_SW_VDIS
        if(m_swVDIS_Mode) {
            int swVDIS_InW, swVDIS_InH, swVDIS_OutW, swVDIS_OutH;
            int swVDIS_StartX, swVDIS_StartY;

            m_exynosCameraParameters->getHwPreviewSize(&swVDIS_OutW, &swVDIS_OutH);
            swVDIS_InW = swVDIS_OutW;
            swVDIS_InH = swVDIS_OutH;
            m_swVDIS_AdjustPreviewSize(&swVDIS_OutW, &swVDIS_OutH);

            swVDIS_StartX = (swVDIS_InW-swVDIS_OutW)/2;
            swVDIS_StartY = (swVDIS_InH-swVDIS_OutH)/2;

            if (m_previewWindow != NULL) {
                m_previewWindow->set_crop(m_previewWindow, swVDIS_StartX, swVDIS_StartY,
                    swVDIS_OutW + swVDIS_StartX, swVDIS_OutH + swVDIS_StartY);
            }
        }
#endif /*SUPPORT_SW_VDIS*/
#ifdef USE_FASTMOTION_CROP
        if(m_exynosCameraParameters->getShotMode() == SHOT_MODE_FASTMOTION) {
            int inW, inH, outW, outH, startX, startY;

            m_exynosCameraParameters->getPreviewSize(&inW, &inH);
            outW = ALIGN_DOWN((int)(inW * FASTMOTION_CROP_RATIO), PREVIEW_OPS_CROP_ALIGN);
            outH = ALIGN_DOWN((int)(inH * FASTMOTION_CROP_RATIO), PREVIEW_OPS_CROP_ALIGN);
            startX = ALIGN_DOWN((inW - outW) / 2, PREVIEW_OPS_CROP_ALIGN);
            startY = ALIGN_DOWN((inH - outH) / 2, PREVIEW_OPS_CROP_ALIGN);

            if (m_previewWindow != NULL) {
                m_previewWindow->set_crop(m_previewWindow, startX, startY, startX + outW, startY + outH);
            }
        }
#endif /* USE_FASTMOTION_CROP */

        /* Prevent displaying unprocessed beauty images in beauty shot. */
        if ((m_exynosCameraParameters->getShotMode() == SHOT_MODE_BEAUTY_FACE)
#ifdef LLS_CAPTURE
            || (m_exynosCameraParameters->getLLSOn() == true && getCameraId() == CAMERA_ID_FRONT)
#endif
            ) {
            if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME) &&
                checkBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE)) {
                CLOGV("INFO(%s[%d]):skip the preview callback and the preview display while compressed callback.",
                        __FUNCTION__, __LINE__);
                ret = m_scpBufferMgr->cancelBuffer(buffer.index);
                goto func_exit;
            }
        }

#ifdef USE_CAMERA_PREVIEW_FRAME_SCHEDULER
        if (((m_getRecordingEnabled() == true) && (curMinFps == curMaxFps)
            && (m_exynosCameraParameters->getShotMode() != SHOT_MODE_SEQUENCE))
            || (m_exynosCameraParameters->getVRMode() == 1)) {
            if(previewQ->getSizeOfProcessQ() < 1) {
                m_previewFrameScheduler->m_schedulePreviewFrame(curMaxFps);
            }
        }
#endif

        /* display the frame */
        {
            ret = m_putBuffers(m_scpBufferMgr, buffer.index);
        }

        if (ret < 0) {
            /* TODO: error handling */
            CLOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
        }
    } else {
        ALOGW("WARN(%s[%d]):Preview frame buffer is canceled."
                "PreviewThread is blocked or too many buffers are in Service."
                "PreviewBufferCount(%d), ScpBufferMgr(%d), PreviewQ(%d)",
                __FUNCTION__, __LINE__,
                m_exynosCameraParameters->getPreviewBufferCount(),
                m_scpBufferMgr->getNumOfAvailableAndNoneBuffer(),
                previewQ->getSizeOfProcessQ());
        ret = m_scpBufferMgr->cancelBuffer(buffer.index);
    }

func_exit:

    if (frame != NULL) {
        frame->decRef();
        m_frameMgr->deleteFrame(frame);
        frame = NULL;
    }

    return loop;
}

status_t ExynosCamera::m_setCallbackBufferInfo(ExynosCameraBuffer *callbackBuf, char *baseAddr)
{
    /*
     * If it is not 16-aligend, shrink down it as 16 align. ex) 1080 -> 1072
     * But, memory is set on Android format. so, not aligned area will be black.
     */
    int dst_width = 0, dst_height = 0, dst_crop_width = 0, dst_crop_height = 0;
    int dst_format = m_exynosCameraParameters->getPreviewFormat();

    m_exynosCameraParameters->getPreviewSize(&dst_width, &dst_height);
    dst_crop_width = dst_width;
    dst_crop_height = dst_height;

    if (dst_format == V4L2_PIX_FMT_NV21 ||
        dst_format == V4L2_PIX_FMT_NV21M) {

        callbackBuf->size[0] = (dst_width * dst_height);
        callbackBuf->size[1] = (dst_width * dst_height) / 2;

        callbackBuf->addr[0] = baseAddr;
        callbackBuf->addr[1] = callbackBuf->addr[0] + callbackBuf->size[0];
    } else if (dst_format == V4L2_PIX_FMT_YVU420 ||
               dst_format == V4L2_PIX_FMT_YVU420M) {
        callbackBuf->size[0] = dst_width * dst_height;
        callbackBuf->size[1] = dst_width / 2 * dst_height / 2;
        callbackBuf->size[2] = callbackBuf->size[1];

        callbackBuf->addr[0] = baseAddr;
        callbackBuf->addr[1] = callbackBuf->addr[0] + callbackBuf->size[0];
        callbackBuf->addr[2] = callbackBuf->addr[1] + callbackBuf->size[1];
    }

    CLOGV("DEBUG(%s): dst_size(%dx%d), dst_crop_size(%dx%d)", __FUNCTION__, dst_width, dst_height, dst_crop_width, dst_crop_height);

    return NO_ERROR;
}

status_t ExynosCamera::m_doPreviewToCallbackFunc(
        int32_t pipeId,
        ExynosCameraFrame *newFrame,
        ExynosCameraBuffer previewBuf,
        ExynosCameraBuffer callbackBuf)
{
    CLOGV("DEBUG(%s): converting preview to callback buffer", __FUNCTION__);

    int ret = 0;
    status_t statusRet = NO_ERROR;

    int hwPreviewW = 0, hwPreviewH = 0;
    int hwPreviewFormat = m_exynosCameraParameters->getHwPreviewFormat();
    bool useCSC = m_exynosCameraParameters->getCallbackNeedCSC();

    ExynosCameraDurationTimer probeTimer;
    int probeTimeMSEC;
    uint32_t fcount = 0;
    camera_frame_metadata_t m_Metadata;

    m_exynosCameraParameters->getHwPreviewSize(&hwPreviewW, &hwPreviewH);
#ifdef SUPPORT_SW_VDIS
    if(m_swVDIS_Mode)
        m_swVDIS_AdjustPreviewSize(&hwPreviewW, &hwPreviewH);
#endif /*SUPPORT_SW_VDIS*/

    ExynosRect srcRect, dstRect;

    camera_memory_t *previewCallbackHeap = NULL;
    previewCallbackHeap = m_getMemoryCb(callbackBuf.fd[0], callbackBuf.size[0], 1, m_callbackCookie);
    if (!previewCallbackHeap || previewCallbackHeap->data == MAP_FAILED) {
        CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, callbackBuf.size[0]);
        statusRet = INVALID_OPERATION;
        goto done;
    }

    ret = m_setCallbackBufferInfo(&callbackBuf, (char *)previewCallbackHeap->data);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]): setCallbackBufferInfo fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        statusRet = INVALID_OPERATION;
        goto done;
    }

    if (m_flagThreadStop == true || m_previewEnabled == false) {
        CLOGE("ERR(%s[%d]): preview was stopped!", __FUNCTION__, __LINE__);
        statusRet = INVALID_OPERATION;
        goto done;
    }

    memset(&m_Metadata, 0, sizeof(camera_frame_metadata_t));
#ifdef SAMSUNG_TIMESTAMP_BOOT
    m_Metadata.timestamp = newFrame->getTimeStampBoot();
    CLOGV("INFO(%s[%d]): timestamp:%lldms!", __FUNCTION__, __LINE__, newFrame->getTimeStampBoot());
#endif
    ret = m_calcPreviewGSCRect(&srcRect, &dstRect);

    if (useCSC) {
        ret = newFrame->setSrcRect(pipeId, &srcRect);
        ret = newFrame->setDstRect(pipeId, &dstRect);

        ret = m_setupEntity(pipeId, newFrame, &previewBuf, &callbackBuf);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):setupEntity fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipeId, ret);
            statusRet = INVALID_OPERATION;
            goto done;
        }
        m_previewFrameFactory->setOutputFrameQToPipe(m_previewCallbackGscFrameDoneQ, pipeId);
        m_previewFrameFactory->pushFrameToPipe(&newFrame, pipeId);

        CLOGV("INFO(%s[%d]):wait preview callback output", __FUNCTION__, __LINE__);
        ret = m_previewCallbackGscFrameDoneQ->waitAndPopProcessQ(&newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: doing exception handling */
            statusRet = INVALID_OPERATION;
            goto done;
        }
        if (newFrame == NULL) {
            CLOGE("ERR(%s[%d]):newFrame is NULL", __FUNCTION__, __LINE__);
            statusRet = INVALID_OPERATION;
            goto done;
        }
        CLOGV("INFO(%s[%d]):preview callback done", __FUNCTION__, __LINE__);

#if 0
        int remainedH = m_orgPreviewRect.h - dst_height;

        if (remainedH != 0) {
            char *srcAddr = NULL;
            char *dstAddr = NULL;
            int planeDiver = 1;

            for (int plane = 0; plane < 2; plane++) {
                planeDiver = (plane + 1) * 2 / 2;

                srcAddr = previewBuf.virt.extP[plane] + (ALIGN_UP(hwPreviewW, CAMERA_ISP_ALIGN) * dst_crop_height / planeDiver);
                dstAddr = callbackBuf->virt.extP[plane] + (m_orgPreviewRect.w * dst_crop_height / planeDiver);

                for (int i = 0; i < remainedH; i++) {
                    memcpy(dstAddr, srcAddr, (m_orgPreviewRect.w / planeDiver));

                    srcAddr += (ALIGN_UP(hwPreviewW, CAMERA_ISP_ALIGN) / planeDiver);
                    dstAddr += (m_orgPreviewRect.w                   / planeDiver);
                }
            }
        }
#endif
    } else { /* neon memcpy */
        char *srcAddr = NULL;
        char *dstAddr = NULL;
        int planeCount = getYuvPlaneCount(hwPreviewFormat);
        if (planeCount <= 0) {
            CLOGE("ERR(%s[%d]):getYuvPlaneCount(%d) fail", __FUNCTION__, __LINE__, hwPreviewFormat);
            statusRet = INVALID_OPERATION;
            goto done;
        }

        /* TODO : have to consider all fmt(planes) and stride */
        for (int plane = 0; plane < planeCount; plane++) {
            srcAddr = previewBuf.addr[plane];
            dstAddr = callbackBuf.addr[plane];
            memcpy(dstAddr, srcAddr, callbackBuf.size[plane]);
        }
    }

    probeTimer.start();
    if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME) &&
        !checkBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE) &&
        m_disablePreviewCB == false) {
        if(m_exynosCameraParameters->getVRMode() == 1) {
#ifdef SAMSUNG_TIMESTAMP_BOOT
            setBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
            m_dataCb(CAMERA_MSG_PREVIEW_FRAME|CAMERA_MSG_PREVIEW_METADATA, previewCallbackHeap, 0, &m_Metadata, m_callbackCookie);
            clearBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
#endif
        } else {
            setBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
            m_dataCb(CAMERA_MSG_PREVIEW_FRAME, previewCallbackHeap, 0, NULL, m_callbackCookie);
            clearBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
        }
    }
    probeTimer.stop();
    getStreamFrameCount((struct camera2_stream *)previewBuf.addr[2], &fcount);
    probeTimeMSEC = (int)probeTimer.durationMsecs();

    if (probeTimeMSEC > 33 && probeTimeMSEC <= 66)
        CLOGV("(%s[%d]):(%d) duration time(%5d msec)", __FUNCTION__, __LINE__, fcount, (int)probeTimer.durationMsecs());
    else if(probeTimeMSEC > 66)
        CLOGD("(%s[%d]):(%d) duration time(%5d msec)", __FUNCTION__, __LINE__, fcount, (int)probeTimer.durationMsecs());
    else
        CLOGV("(%s[%d]):(%d) duration time(%5d msec)", __FUNCTION__, __LINE__, fcount, (int)probeTimer.durationMsecs());

done:
    if (previewCallbackHeap != NULL) {
        previewCallbackHeap->release(previewCallbackHeap);
    }

    return statusRet;
}

status_t ExynosCamera::m_doCallbackToPreviewFunc(
        __unused int32_t pipeId,
        __unused ExynosCameraFrame *newFrame,
        ExynosCameraBuffer callbackBuf,
        ExynosCameraBuffer previewBuf)
{
    CLOGV("DEBUG(%s): converting callback to preview buffer", __FUNCTION__);

    int ret = 0;
    status_t statusRet = NO_ERROR;

    int hwPreviewW = 0, hwPreviewH = 0;
    int hwPreviewFormat = m_exynosCameraParameters->getHwPreviewFormat();
    bool useCSC = m_exynosCameraParameters->getCallbackNeedCSC();

    m_exynosCameraParameters->getHwPreviewSize(&hwPreviewW, &hwPreviewH);

#ifdef SUPPORT_SW_VDIS
    if(m_swVDIS_Mode)
        m_swVDIS_AdjustPreviewSize(&hwPreviewW, &hwPreviewH);
#endif /*SUPPORT_SW_VDIS*/

    camera_memory_t *previewCallbackHeap = NULL;
    previewCallbackHeap = m_getMemoryCb(callbackBuf.fd[0], callbackBuf.size[0], 1, m_callbackCookie);
    if (!previewCallbackHeap || previewCallbackHeap->data == MAP_FAILED) {
        CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, callbackBuf.size[0]);
        statusRet = INVALID_OPERATION;
        goto done;
    }

    ret = m_setCallbackBufferInfo(&callbackBuf, (char *)previewCallbackHeap->data);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]): setCallbackBufferInfo fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        statusRet = INVALID_OPERATION;
        goto done;
    }

    if (m_flagThreadStop == true || m_previewEnabled == false) {
        CLOGE("ERR(%s[%d]): preview was stopped!", __FUNCTION__, __LINE__);
        statusRet = INVALID_OPERATION;
        goto done;
    }

    if (useCSC) {
#if 0
        if (m_exynosPreviewCSC) {
            csc_set_src_format(m_exynosPreviewCSC,
                    ALIGN_DOWN(m_orgPreviewRect.w, CAMERA_MAGIC_ALIGN), ALIGN_DOWN(m_orgPreviewRect.h, CAMERA_MAGIC_ALIGN),
                    0, 0, ALIGN_DOWN(m_orgPreviewRect.w, CAMERA_MAGIC_ALIGN), ALIGN_DOWN(m_orgPreviewRect.h, CAMERA_MAGIC_ALIGN),
                    V4L2_PIX_2_HAL_PIXEL_FORMAT(m_orgPreviewRect.colorFormat),
                    1);

            csc_set_dst_format(m_exynosPreviewCSC,
                    previewW, previewH,
                    0, 0, previewW, previewH,
                    V4L2_PIX_2_HAL_PIXEL_FORMAT(previewFormat),
                    0);

            csc_set_src_buffer(m_exynosPreviewCSC,
                    (void **)callbackBuf->virt.extP, CSC_MEMORY_USERPTR);

            csc_set_dst_buffer(m_exynosPreviewCSC,
                    (void **)previewBuf.fd.extFd, CSC_MEMORY_TYPE);

            if (csc_convert_with_rotation(m_exynosPreviewCSC, 0, m_flip_horizontal, 0) != 0)
                CLOGE("ERR(%s):csc_convert() from callback to lcd fail", __FUNCTION__);
        } else {
            CLOGE("ERR(%s):m_exynosPreviewCSC == NULL", __FUNCTION__);
        }
#else
        CLOGW("WRN(%s[%d]): doCallbackToPreview use CSC is not yet possible", __FUNCTION__, __LINE__);
#endif
    } else { /* neon memcpy */
        char *srcAddr = NULL;
        char *dstAddr = NULL;
        int planeCount = getYuvPlaneCount(hwPreviewFormat);
        if (planeCount <= 0) {
            CLOGE("ERR(%s[%d]):getYuvPlaneCount(%d) fail", __FUNCTION__, __LINE__, hwPreviewFormat);
            statusRet = INVALID_OPERATION;
            goto done;
        }

        /* TODO : have to consider all fmt(planes) and stride */
        for (int plane = 0; plane < planeCount; plane++) {
            srcAddr = callbackBuf.addr[plane];
            dstAddr = previewBuf.addr[plane];
            memcpy(dstAddr, srcAddr, callbackBuf.size[plane]);

            if (mIonClient >= 0)
                ion_sync_fd(mIonClient, previewBuf.fd[plane]);
        }
    }

done:
    if (previewCallbackHeap != NULL) {
        previewCallbackHeap->release(previewCallbackHeap);
    }

    return statusRet;
}

status_t ExynosCamera::m_handlePreviewFrameFront(ExynosCameraFrame *frame)
{
    int ret = 0;
    uint32_t skipCount = 0;
    ExynosCameraFrameEntity *entity = NULL;
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraFrame *fdFrame = NULL;
    ExynosCameraBuffer buffer;
    int pipeID = 0;
    ExynosCameraBuffer resultBuffer;
    camera2_node_group node_group_info_isp;
    enum pipeline pipe;

    entity = frame->getFrameDoneEntity();
    if (entity == NULL) {
        CLOGE("ERR(%s[%d]):current entity is NULL, frameCount(%d)",
            __FUNCTION__, __LINE__, frame->getFrameCount());
        /* TODO: doing exception handling */
        return true;
    }

    if (entity->getEntityState() == ENTITY_STATE_FRAME_SKIP)
            goto entity_state_complete;

    pipeID = entity->getPipeId();

    switch(entity->getPipeId()) {
    case PIPE_ISP:
        m_debugFpsCheck(entity->getPipeId());
        ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getSrcBuffer fail, pipeId(%d), ret(%d)",
                __FUNCTION__, __LINE__, entity->getPipeId(), ret);
            return ret;
        }
        frame->setMetaDataEnable(true);

        ret = m_putBuffers(m_ispBufferMgr, buffer.index);

        ret = frame->getSrcBuffer(PIPE_3AA, &buffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getSrcBuffer fail, pipeId(%d), ret(%d)",
                __FUNCTION__, __LINE__, entity->getPipeId(), ret);
            return ret;
        }

        if (m_exynosCameraParameters->isReprocessing() == true) {
            ret = m_captureSelector->manageFrameHoldList(frame, pipeID, true);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):manageFrameHoldList fail", __FUNCTION__, __LINE__);
                return ret;
            }
        } else {
            /* TODO: This is unusual case, flite buffer and isp buffer */
            ret = m_putBuffers(m_bayerBufferMgr, buffer.index);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
            }
        }

        skipCount = m_exynosCameraParameters->getFrameSkipCount();
        if (skipCount <= 0) {
            /* Face detection */
            fdFrame = m_frameMgr->createFrame(m_exynosCameraParameters, frame->getFrameCount());
            m_copyMetaFrameToFrame(frame, fdFrame, true, false);
            m_facedetectQ->pushProcessQ(&fdFrame);
        }

        ret = generateFrame(m_3aa_ispFrameCount, &newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
            return ret;
        }

        m_setupEntity(PIPE_FLITE, newFrame);
        m_setupEntity(PIPE_3AA, newFrame);
        m_setupEntity(PIPE_ISP, newFrame);
        m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, PIPE_ISP);
        m_previewFrameFactory->pushFrameToPipe(&newFrame, PIPE_FLITE);

        m_3aa_ispFrameCount++;

        /* HACK: When SCC pipe is stopped, generate frame for SCC */
        if ((m_sccBufferMgr->getNumOfAvailableBuffer() > 2)) {
            CLOGW("WRN(%s[%d]): Too many available SCC buffers, generating frame for SCC", __FUNCTION__, __LINE__);

            pipe = (isOwnScc(getCameraId()) == true) ? PIPE_SCC : PIPE_ISPC;

            while (m_sccBufferMgr->getNumOfAvailableBuffer() > 0) {
                ret = generateFrame(m_sccFrameCount, &newFrame);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
                    return ret;
                }

                m_setupEntity(pipe, newFrame);
                m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipe);
                m_previewFrameFactory->pushFrameToPipe(&newFrame, pipe);

                m_sccFrameCount++;
            }
        }

        break;
    case PIPE_ISPC:
    case PIPE_SCC:
        m_debugFpsCheck(entity->getPipeId());
        if (entity->getDstBufState() == ENTITY_BUFFER_STATE_COMPLETE) {
            ret = m_sccCaptureSelector->manageFrameHoldList(frame, entity->getPipeId(), false);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):manageFrameHoldList fail", __FUNCTION__, __LINE__);
                return ret;
            }
        }

        pipe = (isOwnScc(getCameraId()) == true) ? PIPE_SCC : PIPE_ISPC;

        while (m_sccBufferMgr->getNumOfAvailableBuffer() > 0) {
            ret = generateFrameSccScp(pipe, &m_sccFrameCount, &newFrame);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):generateFrameSccScp fail", __FUNCTION__, __LINE__);
                return ret;
            }

            m_setupEntity(pipe, newFrame);
            m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipe);
            m_previewFrameFactory->pushFrameToPipe(&newFrame, pipe);

            m_sccFrameCount++;
        }
        break;
    case PIPE_SCP:
        if (entity->getDstBufState() == ENTITY_BUFFER_STATE_ERROR) {
            ret = frame->getDstBuffer(entity->getPipeId(), &buffer, m_previewFrameFactory->getNodeType(PIPE_SCP));
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                return ret;
            }
            ret = m_scpBufferMgr->cancelBuffer(buffer.index);

        } else if (entity->getDstBufState() == ENTITY_BUFFER_STATE_COMPLETE) {
            m_debugFpsCheck(entity->getPipeId());
            ret = frame->getDstBuffer(entity->getPipeId(), &buffer, m_previewFrameFactory->getNodeType(PIPE_SCP));
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                return ret;
            }

            m_exynosCameraParameters->getFrameSkipCount(&m_frameSkipCount);
            if (m_frameSkipCount > 0) {
                CLOGD("INFO(%s[%d]):frameSkipCount=%d", __FUNCTION__, __LINE__, m_frameSkipCount);
                ret = m_scpBufferMgr->cancelBuffer(buffer.index);
            } else {
                nsecs_t timeStamp = (nsecs_t)frame->getTimeStamp();
                if (m_getRecordingEnabled() == true
                    && m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)) {
                    if (timeStamp <= 0L) {
                        CLOGE("WARN(%s[%d]):timeStamp(%lld) Skip", __FUNCTION__, __LINE__, (long long)timeStamp);
                    } else {
                        /* get Recording Image buffer */
                        int bufIndex = -2;
                        ExynosCameraBuffer recordingBuffer;
                        ret = m_recordingBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &recordingBuffer);
                        if (ret < 0 || bufIndex < 0) {
                            if ((++m_recordingFrameSkipCount % 100) == 0) {
                                CLOGE("ERR(%s[%d]): Recording buffer is not available!! Recording Frames are Skipping(%d frames) (bufIndex=%d)",
                                        __FUNCTION__, __LINE__, m_recordingFrameSkipCount, bufIndex);
                            }
                        } else {
                            if (m_recordingFrameSkipCount != 0) {
                                CLOGE("ERR(%s[%d]): Recording buffer is not available!! Recording Frames are Skipped(%d frames) (bufIndex=%d)",
                                        __FUNCTION__, __LINE__, m_recordingFrameSkipCount, bufIndex);
                                m_recordingFrameSkipCount = 0;
                            }

                            ret = m_doPrviewToRecordingFunc(PIPE_GSC_VIDEO, buffer, recordingBuffer, timeStamp);
                            if (ret < 0) {
                                CLOGW("WARN(%s[%d]):recordingCallback Skip", __FUNCTION__, __LINE__);
                            }
                        }
                    }
                }

                ExynosCameraBuffer callbackBuffer;
                ExynosCameraFrame *callbackFrame = NULL;

                callbackFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(PIPE_SCP);
                frame->getDstBuffer(PIPE_SCP, &callbackBuffer, m_previewFrameFactory->getNodeType(PIPE_SCP));
                m_copyMetaFrameToFrame(frame, callbackFrame, true, true);
                callbackFrame->setDstBuffer(PIPE_SCP, callbackBuffer);

                CLOGV("INFO(%s[%d]):push frame to front previewQ", __FUNCTION__, __LINE__);
                m_previewQ->pushProcessQ(&callbackFrame);

                CLOGV("DEBUG(%s[%d]):SCP done HAL-frameCount(%d)", __FUNCTION__, __LINE__, frame->getFrameCount());
            }
        } else {
            CLOGV("DEBUG(%s[%d]):SCP droped - SCP buffer is not ready HAL-frameCount(%d)", __FUNCTION__, __LINE__, frame->getFrameCount());

            ret = frame->getDstBuffer(entity->getPipeId(), &buffer, m_previewFrameFactory->getNodeType(PIPE_SCP));
            if (buffer.index >= 0) {
                ret = m_scpBufferMgr->cancelBuffer(buffer.index);
                if (ret < 0)
                    CLOGE("ERR(%s[%d]):SCP buffer return fail", __FUNCTION__, __LINE__);
            }
            m_previewFrameFactory->dump();
        }

        ret = generateFrameSccScp(PIPE_SCP, &m_scpFrameCount, &newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):generateFrameSccScp fail", __FUNCTION__, __LINE__);
            return ret;
        }

        m_setupEntity(PIPE_SCP, newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):setupEntity fail", __FUNCTION__, __LINE__);
            break;
        }

        /*check preview drop...*/
        newFrame->getDstBuffer(PIPE_SCP, &resultBuffer, m_previewFrameFactory->getNodeType(PIPE_SCP));
        if (resultBuffer.index < 0) {
            newFrame->setRequest(PIPE_SCP, false);
            newFrame->getNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_ISP);
            node_group_info_isp.capture[PERFRAME_FRONT_SCP_POS].request = 0;
            newFrame->storeNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_ISP);

            m_previewFrameFactory->dump();

            /* when preview buffer is not ready, we should drop preview to make preview buffer ready */
            CLOGW("WARN(%s[%d]):Front preview drop. Failed to get preview buffer. FrameSkipcount(%d)",
                    __FUNCTION__, __LINE__, FRAME_SKIP_COUNT_PREVIEW_FRONT);
            m_exynosCameraParameters->setFrameSkipCount(FRAME_SKIP_COUNT_PREVIEW_FRONT);
        }

        m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, PIPE_SCP);
        m_previewFrameFactory->pushFrameToPipe(&newFrame, PIPE_SCP);

        m_scpFrameCount++;
        break;
    default:
        break;
    }

    if (ret < 0) {
        CLOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
        return ret;
    }

entity_state_complete:

    ret = frame->setEntityState(entity->getPipeId(), ENTITY_STATE_COMPLETE);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setEntityState fail, pipeId(%d), state(%d), ret(%d)",
            __FUNCTION__, __LINE__, entity->getPipeId(), ENTITY_STATE_COMPLETE, ret);
        return ret;
    }

    if (frame->isComplete() == true) {
        ret = m_removeFrameFromList(&m_processList, frame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):remove frame from processList fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }

        frame->decRef();
        m_frameMgr->deleteFrame(frame);
    }

    return NO_ERROR;
}

bool ExynosCamera::m_zoomPreviwWithCscThreadFunc(void)
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif
    int  ret  = 0;
    bool loop = true;
    int32_t previewFormat = 0;
    ExynosRect srcRect, dstRect;
    ExynosCameraBuffer srcBuf;
    ExynosCameraBuffer dstBuf;
    uint32_t *output = NULL;
    struct camera2_stream *meta = NULL;
    ExynosCameraBufferManager *srcBufMgr = NULL;
    ExynosCameraBufferManager *dstBufMgr = NULL;
    ExynosCameraFrameEntity *entity = NULL;
    int previewH, previewW;
    int bufIndex = -1;
    int waitCount = 0;
    int scpNodeIndex = -1;
    int srcNodeIndex = -1;
    int srcFmt = -1;
    int pipeId = -1;
    int gscPipe = -1;
    ExynosCameraFrame  *frame = NULL;
    int32_t status = 0;
    frame_queue_t *pipeFrameDoneQ = NULL;
    entity_buffer_state_t state = ENTITY_BUFFER_STATE_NOREQ;

    CLOGV("INFO(%s[%d]):wait zoomPreviwWithCscQThreadFunc", __FUNCTION__, __LINE__);
    status = m_zoomPreviwWithCscQ->waitAndPopProcessQ(&frame);
    if (m_flagThreadStop == true) {
        CLOGI("INFO(%s[%d]):m_flagThreadStop(%d)", __FUNCTION__, __LINE__, m_flagThreadStop);
        return false;
    }
    if (status < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (status == TIMED_OUT) {
            CLOGW("WARN(%s):wait timeout", __FUNCTION__);
        } else {
            CLOGE("ERR(%s):wait and pop fail, ret(%d)", __FUNCTION__, ret);
            /* TODO: doing exception handling */
        }
        return true;
    }

    if (frame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL, retry", __FUNCTION__, __LINE__);
        return true;
    }

    pipeFrameDoneQ = m_pipeFrameDoneQ;

    previewFormat = m_exynosCameraParameters->getHwPreviewFormat();
    m_exynosCameraParameters->getHwPreviewSize(&previewW, &previewH);

    /* get Scaler src Buffer Node Index*/
    if (m_exynosCameraParameters->getDualMode() == true && getCameraId() == CAMERA_ID_FRONT) {
        pipeId = PIPE_3AA;
        gscPipe = PIPE_GSC;
        srcNodeIndex = m_previewFrameFactory->getNodeType(PIPE_ISPC);
        srcFmt = m_exynosCameraParameters->getHWVdisFormat();
        scpNodeIndex = m_previewFrameFactory->getNodeType(PIPE_SCP);
        srcBufMgr = m_sccBufferMgr;
        dstBufMgr = m_scpBufferMgr;
    } else {
        if (m_exynosCameraParameters->getTpuEnabledMode() == true) {
            pipeId = PIPE_DIS;
        } else {
            pipeId = PIPE_ISP;
        }
        gscPipe = PIPE_GSC;
        srcNodeIndex = m_previewFrameFactory->getNodeType(PIPE_SCP);
        srcFmt = m_exynosCameraParameters->getHwPreviewFormat();
        scpNodeIndex = m_previewFrameFactory->getNodeType(PIPE_SCP);
        srcBufMgr = m_zoomScalerBufferMgr;
        dstBufMgr = m_scpBufferMgr;
    }

    ret = frame->getDstBufferState(pipeId, &state, srcNodeIndex);
    if (ret < 0 || state != ENTITY_BUFFER_STATE_COMPLETE) {
        CLOGE("ERR(%s[%d]):getDstBufferState fail, pipeId(%d), state(%d) frame(%d)",__FUNCTION__, __LINE__, pipeId, state, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    /* get scaler source buffer */
    srcBuf.index = -1;

    ret = frame->getDstBuffer(pipeId, &srcBuf, srcNodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d) frame(%d)",__FUNCTION__, __LINE__, pipeId, ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    /* getMetadata to get buffer size */
    meta = (struct camera2_stream*)srcBuf.addr[srcBuf.planeCount-1];

    if (meta == NULL) {
        CLOGE("ERR(%s[%d]):meta is NULL, pipeId(%d), ret(%d) frame(%d)", __FUNCTION__, __LINE__, gscPipe, ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    output = meta->output_crop_region;

    dstBuf.index = -2;
    bufIndex = -1;
    ret = dstBufMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &dstBuf);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):Buffer manager getBuffer fail, frame(%d), ret(%d)",
                __FUNCTION__, __LINE__, frame->getFrameCount(), ret);
        goto func_exit;
    }

    /* csc and scaling */
    srcRect.x = 0;
    srcRect.y = 0;
    srcRect.w = output[2];
    srcRect.h = output[3];
    srcRect.fullW = output[2];
    srcRect.fullH = output[3];
    srcRect.colorFormat = srcFmt;

    dstRect.x = 0;
    dstRect.y = 0;
    dstRect.w = previewW;
    dstRect.h = previewH;
    dstRect.fullW = previewW;
    dstRect.fullH = previewH;
    dstRect.colorFormat = previewFormat;

    if (m_exynosCameraParameters->getDualMode() == true && getCameraId() == CAMERA_ID_FRONT) {
        /* dual front scenario use 3aa bayer crop */
    } else {
        m_exynosCameraParameters->calcPreviewDzoomCropSize(&srcRect, &dstRect);
    }


    ret = frame->setSrcRect(gscPipe, srcRect);
    ret = frame->setDstRect(gscPipe, dstRect);

    CLOGV("DEBUG(%s[%d]):do zoomPreviw with CSC : srcBuf(%d) dstBuf(%d) (%d, %d, %d, %d) format(%d) actual(%x) -> (%d, %d, %d, %d) format(%d)  actual(%x)", __FUNCTION__, __LINE__, srcBuf.index, dstBuf.index, srcRect.x, srcRect.y, srcRect.w, srcRect.h, srcFmt, V4L2_PIX_2_HAL_PIXEL_FORMAT(srcFmt), dstRect.x, dstRect.y, dstRect.w, dstRect.h, previewFormat, V4L2_PIX_2_HAL_PIXEL_FORMAT(previewFormat));

    ret = m_setupEntity(gscPipe, frame, &srcBuf, &dstBuf);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setupEntity fail, pipeId(%d), ret(%d) frame(%d)", __FUNCTION__, __LINE__, gscPipe, ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    m_previewFrameFactory->setOutputFrameQToPipe(pipeFrameDoneQ, gscPipe);
    m_previewFrameFactory->pushFrameToPipe(&frame, gscPipe);

    CLOGV("DEBUG(%s[%d]):--OUT--", __FUNCTION__, __LINE__);

    return loop;

func_exit:

    CLOGE("ERR(%s[%d]):do zoomPreviw with CSC failed frame(%d) ret(%d) src(%d) dst(%d)", __FUNCTION__, __LINE__, frame->getFrameCount(), ret, srcBuf.index, dstBuf.index);

    dstBuf.index = -1;

    ret = frame->getDstBuffer(gscPipe, &dstBuf);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d) frame(%d)",__FUNCTION__, __LINE__, gscPipe, ret, frame->getFrameCount());
    }

    if (dstBuf.index >= 0)
        dstBufMgr->cancelBuffer(dstBuf.index);

    /* msc with ndone frame, in order to frame order*/
    CLOGE("ERR(%s[%d]): zoom with scaler getbuffer failed , use MSC for NDONE frame(%d)", __FUNCTION__, __LINE__, frame->getFrameCount());

    frame->setSrcBufferState(gscPipe, ENTITY_BUFFER_STATE_ERROR);
    m_previewFrameFactory->setOutputFrameQToPipe(pipeFrameDoneQ, gscPipe);
    m_previewFrameFactory->pushFrameToPipe(&frame, gscPipe);

    CLOGV("DEBUG(%s[%d]):--OUT--", __FUNCTION__, __LINE__);

    return INVALID_OPERATION;
}

status_t ExynosCamera::m_syncPrviewWithCSC(int32_t pipeId, int32_t gscPipe, ExynosCameraFrame *frame)
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif
    int32_t previewFormat = 0;
    status_t ret = NO_ERROR;
    ExynosRect srcRect, dstRect;
    ExynosCameraBuffer srcBuf;
    ExynosCameraBuffer dstBuf;
    uint32_t *output = NULL;
    struct camera2_stream *meta = NULL;
    ExynosCameraBufferManager *srcBufMgr = NULL;
    ExynosCameraBufferManager *dstBufMgr = NULL;
    int previewH, previewW;
    int bufIndex = -1;
    int waitCount = 0;
    int srcNodeIndex = -1;
    int dstNodeIndex = -1;
    int scpNodeIndex = -1;
    int srcFmt = -1;
    entity_buffer_state_t state = ENTITY_BUFFER_STATE_NOREQ;

    /* copy metadata src to dst*/
    if (m_exynosCameraParameters->getDualMode() == true && getCameraId() == CAMERA_ID_FRONT) {
        srcNodeIndex = m_previewFrameFactory->getNodeType(PIPE_ISPC);
        dstNodeIndex = 0;
        scpNodeIndex = m_previewFrameFactory->getNodeType(PIPE_SCP);
        srcBufMgr = m_sccBufferMgr;
        dstBufMgr = m_scpBufferMgr;
    } else {
        srcNodeIndex = m_previewFrameFactory->getNodeType(PIPE_SCP);
        dstNodeIndex = 0;
        scpNodeIndex = m_previewFrameFactory->getNodeType(PIPE_SCP);
        srcBufMgr = m_zoomScalerBufferMgr;
        dstBufMgr = m_scpBufferMgr;
    }

    state = ENTITY_BUFFER_STATE_NOREQ;
    ret = frame->getSrcBufferState(gscPipe, &state);
    if( ret < 0 || state == ENTITY_BUFFER_STATE_ERROR) {
        CLOGE("ERR(%s[%d]):getSrcBufferState fail, pipeId(%d), entityState(%d) frame(%d)",__FUNCTION__, __LINE__, gscPipe, state, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    srcBuf.index = -1;
    dstBuf.index = -1;

    ret = frame->getSrcBuffer(gscPipe, &srcBuf);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):getSrcBuffer fail, pipeId(%d), ret(%d) frame(%d)",__FUNCTION__, __LINE__, pipeId, ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    ret = frame->getDstBuffer(gscPipe, &dstBuf, dstNodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d) frame(%d)",__FUNCTION__, __LINE__, pipeId, ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    memcpy(dstBuf.addr[dstBuf.planeCount-1],srcBuf.addr[srcBuf.planeCount-1], sizeof(struct camera2_stream));

    if (m_exynosCameraParameters->getDualMode() == true && getCameraId() == CAMERA_ID_FRONT) {
        /* dual front scenario use ispc buffer for capture, frameSelector ownership for buffer */
    } else {
        if (srcBuf.index >= 0 )
            srcBufMgr->cancelBuffer(srcBuf.index);
    }

    ret = frame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_REQUESTED, scpNodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):setDstBufferState fail, pipeId(%d), ret(%d) frame(%d)",
		__FUNCTION__, __LINE__, pipeId, ret, frame->getFrameCount());
    }
    ret = frame->setDstBuffer(pipeId, dstBuf, scpNodeIndex);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setdst Buffer failed(%d) frame(%d)", __FUNCTION__, __LINE__, ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }
    ret = frame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_COMPLETE, scpNodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):setDstBufferState fail, pipeId(%d), ret(%d) frame(%d)",
		__FUNCTION__, __LINE__, pipeId, ret, frame->getFrameCount());
    }
    ret = frame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_COMPLETE);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):setDstBufferState fail, pipeId(%d), ret(%d) frame(%d)",
		__FUNCTION__, __LINE__, pipeId, ret, frame->getFrameCount());
    }

    CLOGV("DEBUG(%s[%d]): syncPrviewWithCSC done(%d)", __FUNCTION__, __LINE__, ret);

    return ret;

func_exit:

    CLOGE("ERR(%s[%d]): sync with csc failed frame(%d) ret(%d) src(%d) dst(%d)",
		__FUNCTION__, __LINE__, frame->getFrameCount(), ret, srcBuf.index, dstBuf.index);

    srcBuf.index = -1;
    dstBuf.index = -1;

    /* 1. return buffer pipe done. */
    ret = frame->getDstBuffer(pipeId, &srcBuf, srcNodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):getSrcBuffer fail, pipeId(%d), ret(%d) frame(%d)",
		__FUNCTION__, __LINE__, pipeId, ret, frame->getFrameCount());
    }

    /* 2. do not return buffer dual front case, frameselector ownershipt for the buffer */
    if (m_exynosCameraParameters->getDualMode() == true && getCameraId() == CAMERA_ID_FRONT) {
        /* dual front scenario use ispc buffer for capture, frameSelector ownership for buffer */
    } else {
        if (srcBuf.index >= 0)
            srcBufMgr->cancelBuffer(srcBuf.index);
    }

    /* 3. if the gsc dst buffer available, return the buffer. */
    ret = frame->getDstBuffer(gscPipe, &dstBuf, dstNodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d) frame(%d)",
		__FUNCTION__, __LINE__, pipeId, ret, frame->getFrameCount());
    }

    if (dstBuf.index >= 0)
        dstBufMgr->cancelBuffer(dstBuf.index);

    /* 4. change buffer state error for error handlering */
    /*  1)dst buffer state : 0( putbuffer ndone for mcpipe ) */
    /*  2)dst buffer state : scp node index ( getbuffer ndone for mcpipe ) */
    ret = frame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_ERROR, scpNodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):setDstBufferState fail, pipeId(%d), ret(%d) frame(%d)",
		__FUNCTION__, __LINE__, pipeId, ret, frame->getFrameCount());
    }
    ret = frame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_ERROR);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):setDstBufferState fail, pipeId(%d), ret(%d) frame(%d)",
		__FUNCTION__, __LINE__, pipeId, ret, frame->getFrameCount());
    }

    return INVALID_OPERATION;
}

bool ExynosCamera::m_setBuffersThreadFunc(void)
{
    int ret = 0;

    ret = m_setBuffers();
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_setBuffers failed, releaseBuffer", __FUNCTION__, __LINE__);

        /* TODO: Need release buffers and error exit */

        m_releaseBuffers();
        m_isSuccessedBufferAllocation = false;
        return false;
    }

    m_isSuccessedBufferAllocation = true;
    return false;
}

bool ExynosCamera::m_startPictureInternalThreadFunc(void)
{
    int ret = 0;

    ret = m_startPictureInternal();
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_setBuffers failed", __FUNCTION__, __LINE__);

        /* TODO: Need release buffers and error exit */

        return false;
    }

    return false;
}

bool ExynosCamera::m_prePictureThreadFunc(void)
{
    bool loop = false;
    bool isProcessed = true;

    ExynosCameraDurationTimer m_burstPrePictureTimer;
    uint64_t m_burstPrePictureTimerTime;
    uint64_t burstWaitingTime;

    status_t ret = NO_ERROR;

    uint64_t seriesShotDuration = m_exynosCameraParameters->getSeriesShotDuration();

    m_burstPrePictureTimer.start();

    if (m_exynosCameraParameters->isReprocessing())
        loop = m_reprocessingPrePictureInternal();
    else
        loop = m_prePictureInternal(&isProcessed);

    m_burstPrePictureTimer.stop();
    m_burstPrePictureTimerTime = m_burstPrePictureTimer.durationUsecs();

    if(isProcessed && loop && seriesShotDuration > 0 && m_burstPrePictureTimerTime < seriesShotDuration) {
        CLOGD("DEBUG(%s[%d]): The time between shots is too short(%lld)us. Extended to (%lld)us"
            , __FUNCTION__, __LINE__, (long long)m_burstPrePictureTimerTime, (long long)seriesShotDuration);

        burstWaitingTime = seriesShotDuration - m_burstPrePictureTimerTime;
        usleep(burstWaitingTime);
    }

    return loop;
}

/*
 * pIsProcessed : out parameter
 *                true if the frame is properly handled.
 *                false if frame processing is failed or there is no frame to process
 */
bool ExynosCamera::m_prePictureInternal(bool* pIsProcessed)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    CLOGI("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    int ret = 0;
    bool loop = false;
    ExynosCameraFrame *newFrame = NULL;
    camera2_shot_ext *shot_ext = NULL;

    ExynosCameraBuffer fliteReprocessingBuffer;
    ExynosCameraBuffer ispReprocessingBuffer;

    int pipeId = 0;
    int bufPipeId = 0;
    bool isSrc = false;
    int retryCount = 3;

    if (m_hdrEnabled)
        retryCount = 15;

    if (isOwnScc(getCameraId()) == true) {
        bufPipeId = PIPE_SCC;
    } else {
        bufPipeId = PIPE_ISPC;
    }

    if (m_exynosCameraParameters->is3aaIspOtf() == true) {
        pipeId = PIPE_3AA;
    } else {
        pipeId = PIPE_ISP;
    }

    int postProcessQSize = m_postPictureQ->getSizeOfProcessQ();
    if (postProcessQSize > 2) {
        CLOGW("DEBUG(%s[%d]): post picture is delayed(stacked %d frames), skip", __FUNCTION__, __LINE__, postProcessQSize);
        usleep(WAITING_TIME);
        goto CLEAN;
    }

    newFrame = m_sccCaptureSelector->selectFrames(m_reprocessingCounter.getCount(), pipeId, isSrc, retryCount, m_pictureFrameFactory->getNodeType(bufPipeId));
    if (newFrame == NULL) {
        CLOGE("ERR(%s[%d]):newFrame is NULL", __FUNCTION__, __LINE__);
        goto CLEAN;
    }
    newFrame->frameLock();

    CLOGI("DEBUG(%s[%d]):Frame Count (%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());

    m_postProcessList.push_back(newFrame);

    if ((m_exynosCameraParameters->getHighResolutionCallbackMode() == true) &&
        (m_highResolutionCallbackRunning == true)) {
        m_highResolutionCallbackQ->pushProcessQ(&newFrame);
    } else {
        dstSccReprocessingQ->pushProcessQ(&newFrame);
    }

    m_reprocessingCounter.decCount();

    CLOGI("INFO(%s[%d]):prePicture complete, remaining count(%d)", __FUNCTION__, __LINE__, m_reprocessingCounter.getCount());

    if (m_hdrEnabled) {
        ExynosCameraActivitySpecialCapture *m_sCaptureMgr;

        m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();

        if (m_reprocessingCounter.getCount() == 0)
            m_sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_OFF);
    }

    if ((m_exynosCameraParameters->getHighResolutionCallbackMode() == true) &&
        (m_highResolutionCallbackRunning == true))
        loop = true;

    if (m_reprocessingCounter.getCount() > 0) {
        loop = true;

    }

    *pIsProcessed = true;
    return loop;

CLEAN:
    if (newFrame != NULL) {
        newFrame->printEntity();
        CLOGD("DEBUG(%s[%d]): Picture frame delete(%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());
        newFrame->decRef();
        m_frameMgr->deleteFrame(newFrame);
        newFrame = NULL;
    }

    if (m_hdrEnabled) {
        ExynosCameraActivitySpecialCapture *m_sCaptureMgr;

        m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();

        if (m_reprocessingCounter.getCount() == 0)
            m_sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_OFF);
    }

    if (m_reprocessingCounter.getCount() > 0)
        loop = true;

    CLOGI("INFO(%s[%d]): prePicture fail, remaining count(%d)", __FUNCTION__, __LINE__, m_reprocessingCounter.getCount());
    *pIsProcessed = false;   // Notify failure
    return loop;

}

bool ExynosCamera::m_shutterCallbackThreadFunc(void)
{
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);
    int loop = false;

#ifdef OIS_CAPTURE
    if(m_OISCaptureShutterEnabled) {
        CLOGD("INFO(%s[%d]):shutter callback wait start", __FUNCTION__, __LINE__);
        ExynosCameraActivitySpecialCapture *m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();
        m_sCaptureMgr->waitShutterCallback();
        CLOGD("INFO(%s[%d]):fast shutter callback wait end(%d)", __FUNCTION__, __LINE__, m_sCaptureMgr->getOISCaptureFcount());
    }
#endif

    if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_SHUTTER)) {
        CLOGI("INFO(%s[%d]): CAMERA_MSG_SHUTTER callback S", __FUNCTION__, __LINE__);
#ifdef BURST_CAPTURE
        m_notifyCb(CAMERA_MSG_SHUTTER, m_exynosCameraParameters->getSeriesShotDuration(), 0, m_callbackCookie);
#else
        m_notifyCb(CAMERA_MSG_SHUTTER, 0, 0, m_callbackCookie);
#endif
        CLOGI("INFO(%s[%d]): CAMERA_MSG_SHUTTER callback E", __FUNCTION__, __LINE__);
    }

#ifdef FLASHED_LLS_CAPTURE
    if(m_exynosCameraParameters->getSamsungCamera() && m_exynosCameraParameters->getSeriesShotMode() == SERIES_SHOT_MODE_LLS) {
        if (m_exynosCameraParameters->msgTypeEnabled(MULTI_FRAME_SHOT_FLASHED_INFO)) {
            m_notifyCb(MULTI_FRAME_SHOT_FLASHED_INFO, m_captureSelector->getFlashedLLSCaptureStatus(), 0, m_callbackCookie);
            CLOGI("INFO(%s[%d]): MULTI_FRAME_SHOT_FLASHED_INFO callback flash(%s)",
                __FUNCTION__, __LINE__, m_captureSelector->getFlashedLLSCaptureStatus()? "on" : "off");
        }
    }
#endif

    /* one shot */
    return loop;
}

bool ExynosCamera::m_reprocessingPrePictureInternal(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    CLOGI("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    int ret = 0;
    bool loop = false;
    int retry = 0;
    int retryIsp = 0;
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraFrameEntity *entity = NULL;
    camera2_shot_ext *shot_ext = NULL;
    camera2_stream *shot_stream = NULL;
    uint32_t bayerFrameCount = 0;
    struct camera2_node_output output_crop_info;

    ExynosCameraBufferManager *bufferMgr = NULL;

    int bayerPipeId = 0;
    int prePictureDonePipeId = 0;
    enum pipeline pipe;
    ExynosCameraBuffer bayerBuffer;
    ExynosCameraBuffer ispReprocessingBuffer;

    camera2_shot_ext *updateDmShot = new struct camera2_shot_ext;
    memset(updateDmShot, 0x0, sizeof(struct camera2_shot_ext));

    bayerBuffer.index = -2;
    ispReprocessingBuffer.index = -2;

    /*
     * in case of pureBayer and 3aa_isp OTF, buffer will go isp directly
     */
    if (m_exynosCameraParameters->getUsePureBayerReprocessing() == true) {
        if (m_exynosCameraParameters->isReprocessing3aaIspOTF() == true)
            prePictureDonePipeId = PIPE_3AA_REPROCESSING;
        else
            prePictureDonePipeId = PIPE_ISP_REPROCESSING;
    } else {
        prePictureDonePipeId = PIPE_ISP_REPROCESSING;
    }

    if (m_exynosCameraParameters->getHighResolutionCallbackMode() == true) {
        if (m_highResolutionCallbackRunning == true) {
            /* will be removed */
            while (m_skipReprocessing == true) {
                usleep(WAITING_TIME);
                if (m_skipReprocessing == false) {
                    CLOGD("DEBUG(%s[%d]:stop skip frame for high resolution preview callback", __FUNCTION__, __LINE__);
                    break;
                }
            }
        } else if (m_highResolutionCallbackRunning == false) {
            CLOGW("WRN(%s[%d]): m_reprocessingThreadfunc stop for high resolution preview callback", __FUNCTION__, __LINE__);
            loop = false;
            goto CLEAN_FRAME;
        }
    }

#if defined(SAMSUNG_JQ) && defined(ONE_SECOND_BURST_CAPTURE)
    if (m_exynosCameraParameters->getSeriesShotMode() == SERIES_SHOT_MODE_ONE_SECOND_BURST) {
        CLOGD("[JQ](%s[%d]): ON!!", __FUNCTION__, __LINE__);
        m_isJpegQtableOn = true;
        m_exynosCameraParameters->setJpegQtableOn(true);
    }
#endif

    if (m_isZSLCaptureOn
#ifdef OIS_CAPTURE
        || m_OISCaptureShutterEnabled
#endif
    ) {
        CLOGD("INFO(%s[%d]):fast shutter callback!!", __FUNCTION__, __LINE__);
        m_shutterCallbackThread->join();
        m_shutterCallbackThread->run();
    }

    if (m_exynosCameraParameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON ||
        m_exynosCameraParameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_PURE_DYNAMIC) {
        /* Get Bayer buffer for reprocessing */
        ret = m_getBayerBuffer(m_getBayerPipeId(), &bayerBuffer);
    } else if (m_exynosCameraParameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON ||
        m_exynosCameraParameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC) {
        /* Get Bayer buffer for reprocessing */
        ret = m_getBayerBuffer(m_getBayerPipeId(), &bayerBuffer, updateDmShot);
    } else {
        CLOGE("ERR(%s[%d]): bayer mode is not valid (%d)", __FUNCTION__, __LINE__,
            m_exynosCameraParameters->getReprocessingBayerMode());
        goto CLEAN_FRAME;
    }

    if (ret < 0) {
        CLOGE("ERR(%s[%d]): getBayerBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        goto CLEAN_FRAME;
    }

    if (bayerBuffer.index == -2) {
        CLOGE("ERR(%s[%d]): getBayerBuffer fail. buffer index is -2", __FUNCTION__, __LINE__);
        goto CLEAN_FRAME;
    }

    if (m_exynosCameraParameters->getCaptureExposureTime() != 0 && m_stopLongExposure) {
        CLOGD("DEBUG(%s[%d]): Emergency stop", __FUNCTION__, __LINE__);
        goto CLEAN_FRAME;
    }

#ifdef SAMSUNG_LBP
    if (m_exynosCameraParameters->getSeriesShotMode() == SERIES_SHOT_MODE_NONE && m_isLBPon == true) {
        ret = m_captureSelector->setFrameHoldCount(REPROCESSING_BAYER_HOLD_COUNT);
        m_LBPThread->requestExitAndWait();
        m_lbpBufferMgr->resetBuffers();
        if(m_LBPpluginHandle != NULL) {
            ret = uni_plugin_deinit(m_LBPpluginHandle);
            if(ret < 0)
                CLOGE("[LBP](%s[%d]):Bestphoto plugin deinit failed!!", __FUNCTION__, __LINE__);
        }
        m_LBPindex = 0;
        m_isLBPon = false;
    }
#endif

#ifdef SAMSUNG_DEBLUR
        if(m_exynosCameraParameters->getLDCaptureMode() == MULTI_SHOT_MODE_DEBLUR
            && m_reprocessingCounter.getCount() == 1) {
            CLOGD("DEBUG(%s[%d]):m_reprocessingCounter.getCount()(%d)", __FUNCTION__, __LINE__, m_reprocessingCounter.getCount());
            m_exynosCameraParameters->setOISCaptureModeOn(false);
            ret = m_captureSelector->setFrameHoldCount(1);
        }
#endif

    CLOGD("DEBUG(%s[%d]):bayerBuffer index %d", __FUNCTION__, __LINE__, bayerBuffer.index);

    if (m_exynosCameraParameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC) {
        m_captureSelector->clearList(m_getBayerPipeId(), false, m_previewFrameFactory->getNodeType(PIPE_3AC));
    }

    if (!m_isZSLCaptureOn
#ifdef OIS_CAPTURE
        && !m_OISCaptureShutterEnabled
#endif
#ifdef BURST_CAPTURE
        && (m_exynosCameraParameters->getSeriesShotMode() != SERIES_SHOT_MODE_BURST
            || m_burstShutterLocation == BURST_SHUTTER_PREPICTURE)
#endif
#ifdef ONE_SECOND_BURST_CAPTURE
        && (m_exynosCameraParameters->getSeriesShotMode() != SERIES_SHOT_MODE_ONE_SECOND_BURST
            || m_burstShutterLocation == BURST_SHUTTER_PREPICTURE)
#endif
    ) {
        m_shutterCallbackThread->join();
        m_shutterCallbackThread->run();
#ifdef BURST_CAPTURE
        if (m_exynosCameraParameters->getSeriesShotMode() == SERIES_SHOT_MODE_BURST)
            m_burstShutterLocation = BURST_SHUTTER_PREPICTURE_DONE;
#endif
#ifdef ONE_SECOND_BURST_CAPTURE
        if (m_exynosCameraParameters->getSeriesShotMode() == SERIES_SHOT_MODE_ONE_SECOND_BURST)
            m_burstShutterLocation = BURST_SHUTTER_PREPICTURE_DONE;
#endif
    }

#ifdef OIS_CAPTURE
    m_OISCaptureShutterEnabled = false;
#endif

    m_isZSLCaptureOn = false;

#ifdef OIS_CAPTURE
if ((m_exynosCameraParameters->getSeriesShotMode() == SERIES_SHOT_MODE_LLS ||
        m_exynosCameraParameters->getSeriesShotMode() == SERIES_SHOT_MODE_SIS) &&
        m_reprocessingCounter.getCount() == 1) {
            CLOGD("DEBUG(%s[%d]):LLS last capture frame", __FUNCTION__, __LINE__);
#ifndef LLS_REPROCESSING
            ExynosCameraActivitySpecialCapture *m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();
            m_sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_WAIT_CAPTURE);
#else
            m_exynosCameraParameters->setOISCaptureModeOn(false);
#endif
    }
#endif

    /* Generate reprocessing Frame */
    ret = generateFrameReprocessing(&newFrame);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):generateFrameReprocessing fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        goto CLEAN_FRAME;
    }

#ifndef RAWDUMP_CAPTURE
#ifdef DEBUG_RAWDUMP
    if (m_exynosCameraParameters->checkBayerDumpEnable()) {
        int sensorMaxW, sensorMaxH;
        int sensorMarginW, sensorMarginH;
        bool bRet;
        char filePath[70];

        memset(filePath, 0, sizeof(filePath));
        snprintf(filePath, sizeof(filePath), "/data/media/0/RawCapture%d_%d.raw",m_cameraId, m_fliteFrameCount);
        if (m_exynosCameraParameters->getUsePureBayerReprocessing() == true) {
            /* Pure Bayer Buffer Size == MaxPictureSize + Sensor Margin == Max Sensor Size */
            m_exynosCameraParameters->getMaxSensorSize(&sensorMaxW, &sensorMaxH);
        } else {
            m_exynosCameraParameters->getMaxPictureSize(&sensorMaxW, &sensorMaxH);
        }

        bRet = dumpToFile((char *)filePath,
            bayerBuffer.addr[0],
            sensorMaxW * sensorMaxH * 2);
        if (bRet != true)
            CLOGE("couldn't make a raw file");
    }
#endif /* DEBUG_RAWDUMP */
#endif

    if (m_exynosCameraParameters->getUsePureBayerReprocessing() == false
#ifdef LLS_REPROCESSING
        && m_captureSelector->getIsConvertingMeta() == true
#endif
        ) {
        /* TODO: HACK: Will be removed, this is driver's job */
        ret = m_convertingStreamToShotExt(&bayerBuffer, &output_crop_info);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]): shot_stream to shot_ext converting fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            goto CLEAN_FRAME;
        }

        camera2_node_group node_group_info;
        ExynosRect srcRect , dstRect;
        int pictureW = 0, pictureH = 0;

        memset(&node_group_info, 0x0, sizeof(camera2_node_group));
        m_exynosCameraParameters->getPictureSize(&pictureW, &pictureH);

        newFrame->getNodeGroupInfo(&node_group_info, PERFRAME_INFO_DIRTY_REPROCESSING_ISP);
        node_group_info.leader.input.cropRegion[0] = output_crop_info.cropRegion[0];
        node_group_info.leader.input.cropRegion[1] = output_crop_info.cropRegion[1];
        node_group_info.leader.input.cropRegion[2] = output_crop_info.cropRegion[2];
        node_group_info.leader.input.cropRegion[3] = output_crop_info.cropRegion[3];
        node_group_info.leader.output.cropRegion[0] = 0;
        node_group_info.leader.output.cropRegion[1] = 0;
        node_group_info.leader.output.cropRegion[2] = node_group_info.leader.input.cropRegion[2];
        node_group_info.leader.output.cropRegion[3] = node_group_info.leader.input.cropRegion[3];

        node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[0] = node_group_info.leader.output.cropRegion[0];
        node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[1] = node_group_info.leader.output.cropRegion[1];
        node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[2] = node_group_info.leader.output.cropRegion[2];
        node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[3] = node_group_info.leader.output.cropRegion[3];
        node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].output.cropRegion[0] = 0;
        node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].output.cropRegion[1] = 0;
        if (isOwnScc(getCameraId()) == true) {
            node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].output.cropRegion[2] = pictureW;
            node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].output.cropRegion[3] = pictureH;
        } else {
            node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].output.cropRegion[2] = node_group_info.leader.output.cropRegion[2];
            node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].output.cropRegion[3] = node_group_info.leader.output.cropRegion[3];
        }

        CLOGV("DEBUG(%s[%d]): isp capture input(%d %d %d %d), output(%d %d %d %d)", __FUNCTION__, __LINE__,
                                                                                    node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[0],
                                                                                    node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[1],
                                                                                    node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[2],
                                                                                    node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[3],
                                                                                    node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].output.cropRegion[0],
                                                                                    node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].output.cropRegion[1],
                                                                                    node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].output.cropRegion[2],
                                                                                    node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].output.cropRegion[3]);

        if (node_group_info.leader.output.cropRegion[2] < node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[2]) {
            CLOGI("INFO(%s[%d]:(%d -> %d))", __FUNCTION__, __LINE__,
                node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[2],
                node_group_info.leader.output.cropRegion[2]);

            node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[2] = node_group_info.leader.output.cropRegion[2];
        }
        if (node_group_info.leader.output.cropRegion[3] < node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[3]) {
            CLOGI("INFO(%s[%d]:(%d -> %d))", __FUNCTION__, __LINE__,
                node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[3],
                node_group_info.leader.output.cropRegion[3]);

            node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[3] = node_group_info.leader.output.cropRegion[3];
        }

        newFrame->storeNodeGroupInfo(&node_group_info, PERFRAME_INFO_DIRTY_REPROCESSING_ISP);
    }

    shot_ext = (struct camera2_shot_ext *)(bayerBuffer.addr[1]);

    /* Meta setting */
    if (shot_ext != NULL) {
        if (m_exynosCameraParameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON ||
            m_exynosCameraParameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_PURE_DYNAMIC) {
            ret = newFrame->storeDynamicMeta(shot_ext);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]): storeDynamicMeta fail ret(%d)", __FUNCTION__, __LINE__, ret);
                goto CLEAN_FRAME;
            }

            ret = newFrame->storeUserDynamicMeta(shot_ext);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]): storeUserDynamicMeta fail ret(%d)", __FUNCTION__, __LINE__, ret);
                goto CLEAN_FRAME;
            }
        } else if (m_exynosCameraParameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON ||
            m_exynosCameraParameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC) {
            ret = newFrame->storeDynamicMeta(updateDmShot);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]): storeDynamicMeta fail ret(%d)", __FUNCTION__, __LINE__, ret);
                goto CLEAN_FRAME;
            }

            ret = newFrame->storeUserDynamicMeta(updateDmShot);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]): storeUserDynamicMeta fail ret(%d)", __FUNCTION__, __LINE__, ret);
                goto CLEAN_FRAME;
            }
        }

        newFrame->getMetaData(shot_ext);
        m_exynosCameraParameters->duplicateCtrlMetadata((void *)shot_ext);

        CLOGD("DEBUG(%s[%d]):meta_shot_ext->shot.dm.request.frameCount : %d",
                __FUNCTION__, __LINE__,
                getMetaDmRequestFrameCount(shot_ext));
    } else {
        CLOGE("DEBUG(%s[%d]):shot_ext is NULL", __FUNCTION__, __LINE__);
    }

    /* SCC */
    if (isOwnScc(getCameraId()) == true) {
        pipe = PIPE_SCC_REPROCESSING;

        m_getBufferManager(pipe, &bufferMgr, DST_BUFFER_DIRECTION);

        ret = m_checkBufferAvailable(pipe, bufferMgr);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]): Waiting buffer timeout, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipe, ret);
            goto CLEAN_FRAME;
        }

        ret = m_setupEntity(pipe, newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]:setupEntity fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipe, ret);
            goto CLEAN_FRAME;
        }

        if ((m_exynosCameraParameters->getHighResolutionCallbackMode() == true) &&
                (m_highResolutionCallbackRunning == true)) {
            m_reprocessingFrameFactory->setOutputFrameQToPipe(m_highResolutionCallbackQ, pipe);
        } else {
            m_reprocessingFrameFactory->setOutputFrameQToPipe(dstSccReprocessingQ, pipe);
        }

        /* push frame to SCC pipe */
        m_reprocessingFrameFactory->pushFrameToPipe(&newFrame, pipe);
    } else {
        if (m_exynosCameraParameters->isReprocessing3aaIspOTF() == false) {
            pipe = PIPE_ISP_REPROCESSING;

            m_getBufferManager(pipe, &bufferMgr, DST_BUFFER_DIRECTION);

            ret = m_checkBufferAvailable(pipe, bufferMgr);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]): Waiting buffer timeout, pipeId(%d), ret(%d)",
                           __FUNCTION__, __LINE__, pipe, ret);
                goto CLEAN_FRAME;
            }

        } else {
            pipe = PIPE_3AA_REPROCESSING;
        }

        if ((m_exynosCameraParameters->getHighResolutionCallbackMode() == true) &&
                (m_highResolutionCallbackRunning == true)) {
            m_reprocessingFrameFactory->setFrameDoneQToPipe(m_highResolutionCallbackQ, pipe);
        } else {
            m_reprocessingFrameFactory->setFrameDoneQToPipe(dstSccReprocessingQ, pipe);
        }

    }

    /* Add frame to post processing list */
    CLOGD("DEBUG(%s[%d]): push frame(%d) to postPictureList",
        __FUNCTION__, __LINE__, newFrame->getFrameCount());
    newFrame->frameLock();
    m_postProcessList.push_back(newFrame);

    ret = m_reprocessingFrameFactory->startInitialThreads();
    if (ret < 0) {
        CLOGE("ERR(%s):startInitialThreads fail", __FUNCTION__);
        goto CLEAN;
    }

    if (isOwnScc(getCameraId()) == true) {
        retry = 0;
        do {
            if( m_reprocessingFrameFactory->checkPipeThreadRunning(pipe) == false ) {
                ret = m_reprocessingFrameFactory->startInitialThreads();
                if (ret < 0) {
                    CLOGE("ERR(%s):startInitialThreads fail", __FUNCTION__);
                }
            }

            ret = newFrame->ensureDstBufferState(pipe, ENTITY_BUFFER_STATE_PROCESSING);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]): ensure buffer state(ENTITY_BUFFER_STATE_PROCESSING) fail(retry), ret(%d)", __FUNCTION__, __LINE__, ret);
                usleep(1000);
                retry++;
            }
        } while (ret < 0 && retry < 100);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]): ensure buffer state(ENTITY_BUFFER_STATE_PROCESSING) fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            goto CLEAN;
        }
    }

    /* Get bayerPipeId at first entity */
    bayerPipeId = newFrame->getFirstEntity()->getPipeId();
    CLOGD("DEBUG(%s[%d]): bayer Pipe ID(%d)", __FUNCTION__, __LINE__, bayerPipeId);

    /* Check available buffer */
    ret = m_getBufferManager(bayerPipeId, &bufferMgr, DST_BUFFER_DIRECTION);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]): getBufferManager fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        goto CLEAN;
    }
    if (bufferMgr != NULL) {
        ret = m_checkBufferAvailable(bayerPipeId, bufferMgr);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]): Waiting buffer timeout, bayerPipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, bayerPipeId, ret);
            goto CLEAN;
        }
    }

    ret = m_setupEntity(bayerPipeId, newFrame, &bayerBuffer, NULL);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]:setupEntity fail, bayerPipeId(%d), ret(%d)",
            __FUNCTION__, __LINE__, bayerPipeId, ret);
        goto CLEAN;
    }

    m_reprocessingFrameFactory->setOutputFrameQToPipe(dstIspReprocessingQ, prePictureDonePipeId);

        newFrame->incRef();

    /* push the newFrameReprocessing to pipe */
    m_reprocessingFrameFactory->pushFrameToPipe(&newFrame, bayerPipeId);

    /* When enabled SCC capture or pureBayerReprocessing, we need to start bayer pipe thread */
    if (m_exynosCameraParameters->getUsePureBayerReprocessing() == true ||
        m_exynosCameraParameters->isSccCapture() == true)
        m_reprocessingFrameFactory->startThread(bayerPipeId);

    /* wait ISP done */
    CLOGI("INFO(%s[%d]):wait ISP output", __FUNCTION__, __LINE__);
    newFrame = NULL;
    do {
        ret = dstIspReprocessingQ->waitAndPopProcessQ(&newFrame);
        retryIsp++;
    } while (ret == TIMED_OUT && retryIsp < 200 && m_flagThreadStop != true);

    if (ret < 0) {
        CLOGW("WARN(%s[%d]):ISP wait and pop return, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        /* goto CLEAN; */
    }
    if (newFrame == NULL) {
        CLOGE("ERR(%s[%d]):newFrame is NULL", __FUNCTION__, __LINE__);
        goto CLEAN;
    }

    bayerBuffer.index = -2;
    ret = newFrame->getSrcBuffer(bayerPipeId, &bayerBuffer);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getSrcBuffer fail, bayerPipeId(%d), ret(%d)",
                __FUNCTION__, __LINE__, bayerPipeId, ret);
    }

    shot_ext = (struct camera2_shot_ext *)(bayerBuffer.addr[1]);

    ret = newFrame->setEntityState(bayerPipeId, ENTITY_STATE_COMPLETE);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setEntityState(ENTITY_STATE_PROCESSING) fail, pipeId(%d), ret(%d)",
                __FUNCTION__, __LINE__, bayerPipeId, ret);

        if (updateDmShot != NULL) {
            delete updateDmShot;
            updateDmShot = NULL;
        }

        return ret;
    }

    CLOGI("INFO(%s[%d]):ISP output done", __FUNCTION__, __LINE__);

    newFrame->setMetaDataEnable(true);

#ifdef LLS_CAPTURE
    if (shot_ext != NULL) {
        if ((m_exynosCameraParameters->getSeriesShotMode() == SERIES_SHOT_MODE_LLS ||
                m_exynosCameraParameters->getSeriesShotMode() == SERIES_SHOT_MODE_SIS) &&
                    m_reprocessingCounter.getCount() == m_exynosCameraParameters->getSeriesShotCount()) {
            CLOGD("LLS(%s[%d]): lls_tuning_set_index(%d)", __FUNCTION__, __LINE__, shot_ext->shot.dm.stats.vendor_lls_tuning_set_index);
            CLOGD("LLS(%s[%d]): lls_brightness_index(%d)", __FUNCTION__, __LINE__, shot_ext->shot.dm.stats.vendor_lls_brightness_index);
            m_notifyCb(MULTI_FRAME_SHOT_PARAMETERS, shot_ext->shot.dm.stats.vendor_lls_tuning_set_index,
                            shot_ext->shot.dm.stats.vendor_lls_brightness_index, m_callbackCookie);
        }
    }
#endif

#ifdef LLS_REPROCESSING
    if (m_captureSelector->getIsLastFrame() == true)
#endif
    {
        /* put bayer buffer */
        ret = m_putBuffers(m_bayerBufferMgr, bayerBuffer.index);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]): 3AA src putBuffer fail, index(%d), ret(%d)", __FUNCTION__, __LINE__, bayerBuffer.index, ret);
            goto CLEAN;
        }
#ifdef LLS_REPROCESSING
        m_captureSelector->setIsConvertingMeta(true);
#endif
    }

    /* put isp buffer */
    if (m_exynosCameraParameters->getUsePureBayerReprocessing() == true) {
        ret = m_getBufferManager(bayerPipeId, &bufferMgr, DST_BUFFER_DIRECTION);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]): getBufferManager fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            goto CLEAN;
        }
        if (bufferMgr != NULL) {
            ret = newFrame->getDstBuffer(bayerPipeId, &ispReprocessingBuffer, m_previewFrameFactory->getNodeType(PIPE_FLITE));
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):getDstBuffer fail, bayerPipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, bayerPipeId, ret);
                goto CLEAN;
            }
            ret = m_putBuffers(m_ispReprocessingBufferMgr, ispReprocessingBuffer.index);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]): ISP src putBuffer fail, index(%d), ret(%d)", __FUNCTION__, __LINE__, bayerBuffer.index, ret);
                goto CLEAN;
            }
        }
    }

    m_reprocessingCounter.decCount();

    CLOGI("INFO(%s[%d]):reprocessing complete, remaining count(%d)", __FUNCTION__, __LINE__, m_reprocessingCounter.getCount());

    if (m_hdrEnabled) {
        ExynosCameraActivitySpecialCapture *m_sCaptureMgr;

        m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();

        if (m_reprocessingCounter.getCount() == 0)
            m_sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_OFF);
    }

#ifdef OIS_CAPTURE
    if (m_exynosCameraParameters->getOISCaptureModeOn() == true && m_reprocessingCounter.getCount() == 0) {
        m_exynosCameraParameters->setOISCaptureModeOn(false);
    }
#endif

        if (newFrame != NULL) {
        CLOGD("DEBUG(%s[%d]): Reprocessing frame delete(%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());
            newFrame->decRef();
            m_frameMgr->deleteFrame(newFrame);
            newFrame = NULL;
        }
    if ((m_exynosCameraParameters->getHighResolutionCallbackMode() == true) &&
        (m_highResolutionCallbackRunning == true)) {
        loop = true;
    }

    if (m_reprocessingCounter.getCount() > 0)
        loop = true;

    if (updateDmShot != NULL) {
        delete updateDmShot;
        updateDmShot = NULL;
    }

    /* one shot */
    return loop;

CLEAN_FRAME:
    /* newFrame is not pushed any pipes, we can delete newFrame */
    if (newFrame != NULL) {
        newFrame->printEntity();
        CLOGD("DEBUG(%s[%d]): Reprocessing frame delete(%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());
        newFrame->decRef();
        m_frameMgr->deleteFrame(newFrame);
        newFrame = NULL;
    }

CLEAN:
    if (newFrame != NULL) {
        bayerBuffer.index = -2;
        ret = m_getBufferManager(bayerPipeId, &bufferMgr, SRC_BUFFER_DIRECTION);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]): getBufferManager fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }
        if (bufferMgr != NULL) {
            ret = newFrame->getSrcBuffer(bayerPipeId, &bayerBuffer);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):getDstBuffer fail, bayerPipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, bayerPipeId, ret);
            }
            if (bayerBuffer.index >= 0)
                m_putBuffers(bufferMgr, bayerBuffer.index);
        }

        if (m_exynosCameraParameters->getUsePureBayerReprocessing() == true) {
            ret = m_getBufferManager(bayerPipeId, &bufferMgr, DST_BUFFER_DIRECTION);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]): getBufferManager fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            }
            if (bufferMgr != NULL) {
                ret = newFrame->getDstBuffer(bayerPipeId, &ispReprocessingBuffer,
                                                m_pictureFrameFactory->getNodeType(PIPE_3AC));
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):getDstBuffer fail, bayerPipeId(%d), ret(%d)",
                            __FUNCTION__, __LINE__, bayerPipeId, ret);
                }
                if (ispReprocessingBuffer.index >= 0)
                    m_putBuffers(bufferMgr, ispReprocessingBuffer.index);
            }
        }
    }

    /* newFrame is already pushed some pipes, we can not delete newFrame until frame is complete */
    if (newFrame != NULL) {
        newFrame->frameUnlock();
        ret = m_removeFrameFromList(&m_postProcessList, newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):remove frame from processList fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }

        newFrame->printEntity();
        CLOGD("DEBUG(%s[%d]): Reprocessing frame delete(%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());
#ifdef LLS_REPROCESSING
        if (m_captureSelector->getIsLastFrame() == true)
#endif
        {
            newFrame->decRef();
            m_frameMgr->deleteFrame(newFrame);
            newFrame = NULL;
        }
    }

    if (updateDmShot != NULL) {
        delete updateDmShot;
        updateDmShot = NULL;
    }

    if (m_hdrEnabled) {
        ExynosCameraActivitySpecialCapture *m_sCaptureMgr;

        m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();

        if (m_reprocessingCounter.getCount() == 0)
            m_sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_OFF);
    }

#ifdef OIS_CAPTURE
    if (m_exynosCameraParameters->getOISCaptureModeOn() == true && m_reprocessingCounter.getCount() == 0) {
        m_exynosCameraParameters->setOISCaptureModeOn(false);
    }
#endif

        if (newFrame != NULL) {
            newFrame->decRef();
            m_frameMgr->deleteFrame(newFrame);
            newFrame = NULL;
        }
    if ((m_exynosCameraParameters->getHighResolutionCallbackMode() == true) &&
        (m_highResolutionCallbackRunning == true)) {
        loop = true;
    }

    if (m_reprocessingCounter.getCount() > 0)
        loop = true;

    CLOGI("INFO(%s[%d]): reprocessing fail, remaining count(%d)", __FUNCTION__, __LINE__, m_reprocessingCounter.getCount());

    return loop;
}

#if defined(SAMSUNG_COMPANION)
int ExynosCamera::m_getSensorId(int m_cameraId)
{
    unsigned int scenario = 0;
    unsigned int scenarioBit = 0;
    unsigned int nodeNumBit = 0;
    unsigned int sensorIdBit = 0;
    unsigned int sensorId = getSensorId(m_cameraId);

    scenarioBit = (scenario << SCENARIO_SHIFT);

    nodeNumBit = ((FIMC_IS_VIDEO_SS0_NUM - FIMC_IS_VIDEO_SS0_NUM) << SSX_VINDEX_SHIFT);

    sensorIdBit = (sensorId << 0);

    return (scenarioBit) | (nodeNumBit) | (sensorIdBit);
}

bool ExynosCamera::m_companionThreadFunc(void)
{
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);
    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;
    int loop = false;
    int ret = 0;

    m_timer.start();

    m_companionNode = new ExynosCameraNode();

    ret = m_companionNode->create("companion", m_cameraId);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]): Companion Node create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
    }

    ret = m_companionNode->open(MAIN_CAMERA_COMPANION_NUM);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]): Companion Node open fail, ret(%d)", __FUNCTION__, __LINE__, ret);
    }
    CLOGD("DEBUG(%s):Companion Node(%d) opened running)", __FUNCTION__, MAIN_CAMERA_COMPANION_NUM);

    ret = m_companionNode->setInput(m_getSensorId(m_cameraId));
    if (ret < 0) {
        CLOGE("ERR(%s[%d]): Companion Node s_input fail, ret(%d)", __FUNCTION__, __LINE__, ret);
    }
    CLOGD("DEBUG(%s):Companion Node(%d) s_input)", __FUNCTION__, MAIN_CAMERA_COMPANION_NUM);

    m_timer.stop();
    durationTime = m_timer.durationMsecs();
    CLOGD("DEBUG(%s):duration time(%5d msec)", __FUNCTION__, (int)durationTime);

    /* one shot */
    return loop;
}
#endif

#if defined(SAMSUNG_EEPROM)
bool ExynosCamera::m_eepromThreadFunc(void)
{
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);
    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;
    char sensorFW[50] = {0,};
    int ret = 0;
    FILE *fp = NULL;

    m_timer.start();

    if(m_cameraId == CAMERA_ID_BACK) {
        fp = fopen(SENSOR_FW_PATH_BACK, "r");
    } else {
        fp = fopen(SENSOR_FW_PATH_FRONT, "r");
    }
    if (fp == NULL) {
        CLOGE("ERR(%s[%d]):failed to open sysfs. camera id = %d", __FUNCTION__, __LINE__, m_cameraId);
        goto err;
    }

    if (fgets(sensorFW, sizeof(sensorFW), fp) == NULL) {
        CLOGE("ERR(%s[%d]):failed to read sysfs entry", __FUNCTION__, __LINE__);
	    goto err;
    }

    /* int numread = strlen(sensorFW); */
    CLOGI("INFO(%s[%d]): eeprom read complete. Sensor FW ver: %s", __FUNCTION__, __LINE__, sensorFW);

err:
    if (fp != NULL)
        fclose(fp);

    m_timer.stop();
    durationTime = m_timer.durationMsecs();
    CLOGD("DEBUG(%s):duration time(%5d msec)", __FUNCTION__, (int)durationTime);

    /* one shot */
    return false;
}
#endif

bool ExynosCamera::m_ThumbnailCallbackThreadFunc(void)
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif
    int32_t previewFormat = 0;
    status_t ret = NO_ERROR;
    ExynosRect srcRect, dstRect;
    int thumbnailH, thumbnailW;
    int previewH, previewW;
    int waitCount = 0;
    int dstbufferIndex = -1;
    ExynosCameraFrame *callbackFrame = NULL;
    ExynosCameraBuffer postgscReprocessingSrcBuffer;
    ExynosCameraBuffer postgscReprocessingDstBuffer;
    ExynosCameraBufferManager *srcbufferMgr = NULL;
    ExynosCameraBufferManager *dstbufferMgr = NULL;
    int gscPipe = PIPE_GSC_REPROCESSING3;
    int retry = 0;
    int framecount = 0;
    int loop = false;

    postgscReprocessingSrcBuffer.index = -2;
    postgscReprocessingDstBuffer.index = -2;

    CLOGD("DEBUG(%s[%d]):-- IN --", __FUNCTION__, __LINE__);

    /* wait GSC */
    CLOGV("INFO(%s[%d]):wait GSC output pipe(%d)", __FUNCTION__, __LINE__, gscPipe);
    ret = m_thumbnailCallbackQ->waitAndPopProcessQ(&postgscReprocessingSrcBuffer);
    CLOGD("INFO(%s[%d]):GSC output done pipe(%d)", __FUNCTION__, __LINE__, gscPipe);
    if (ret < 0) {
        CLOGE("ERR(%s)(%d):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
       goto CLEAN;
    }

    m_getBufferManager(gscPipe, &srcbufferMgr, SRC_BUFFER_DIRECTION);
    m_getBufferManager(gscPipe, &dstbufferMgr, DST_BUFFER_DIRECTION);

    callbackFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(gscPipe, framecount);

    previewFormat = m_exynosCameraParameters->getHwPreviewFormat();
    m_exynosCameraParameters->getThumbnailSize(&thumbnailW, &thumbnailH);
    m_exynosCameraParameters->getPreviewSize(&previewW, &previewH);

    do {
        dstbufferIndex = -2;
        retry++;

        if (dstbufferMgr->getNumOfAvailableBuffer() > 0)
            dstbufferMgr->getBuffer(&dstbufferIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &postgscReprocessingDstBuffer);
        if (dstbufferIndex < 0) {
            usleep(WAITING_TIME);
            if (retry % 20 == 0) {
                CLOGW("WRN(%s[%d]):retry Post View Thumbnail getBuffer)", __FUNCTION__, __LINE__);
                dstbufferMgr->dump();
            }
        }
    } while (dstbufferIndex < 0 && retry < (TOTAL_WAITING_TIME / WAITING_TIME) && m_flagThreadStop != true && m_stopBurstShot == false);

    srcRect.x = 0;
    srcRect.y = 0;
    srcRect.w = previewW;
    srcRect.h = previewH;
    srcRect.fullW = previewW;
    srcRect.fullH = previewH;
    srcRect.colorFormat = HAL_PIXEL_FORMAT_2_V4L2_PIX(HAL_PIXEL_FORMAT_YCrCb_420_SP);

    dstRect.x = 0;
    dstRect.y = 0;
    dstRect.w = thumbnailW;
    dstRect.h = thumbnailH;
    dstRect.fullW = thumbnailW;
    dstRect.fullH = thumbnailH;
    dstRect.colorFormat = HAL_PIXEL_FORMAT_2_V4L2_PIX(HAL_PIXEL_FORMAT_RGBA_8888);

    CLOGD("DEBUG(%s[%d]):srcBuf(%d) dstBuf(%d) (%d, %d, %d, %d) format(%d) actual(%x) -> (%d, %d, %d, %d) format(%d)  actual(%x)",
        __FUNCTION__, __LINE__, postgscReprocessingSrcBuffer.index, postgscReprocessingDstBuffer.index,
        srcRect.x, srcRect.y, srcRect.w, srcRect.h, srcRect.colorFormat, V4L2_PIX_2_HAL_PIXEL_FORMAT(srcRect.colorFormat),
        dstRect.x, dstRect.y, dstRect.w, dstRect.h, dstRect.colorFormat, V4L2_PIX_2_HAL_PIXEL_FORMAT(previewFormat));

    ret = callbackFrame->setSrcRect(gscPipe, srcRect);
    ret = callbackFrame->setDstRect(gscPipe, dstRect);

    ret = m_setupEntity(gscPipe, callbackFrame, &postgscReprocessingSrcBuffer, &postgscReprocessingDstBuffer);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setupEntity fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, gscPipe, ret);
        goto CLEAN;
    }

#ifdef SAMSUNG_LLS_DEBLUR
    if (m_exynosCameraParameters->getLDCaptureMode()) {
       m_pictureFrameFactory->setFlagFlipIgnore(gscPipe, true);
    }
#endif

    m_pictureFrameFactory->setOutputFrameQToPipe(m_ThumbnailPostCallbackQ, gscPipe);
    m_pictureFrameFactory->pushFrameToPipe(&callbackFrame, gscPipe);

    /* wait GSC done */
    CLOGV("INFO(%s[%d]):wait GSC output", __FUNCTION__, __LINE__);
    waitCount = 0;

    do {
        ret = m_ThumbnailPostCallbackQ->waitAndPopProcessQ(&callbackFrame);
        waitCount++;
    } while (ret == TIMED_OUT && waitCount < 100 && m_flagThreadStop != true);

    if (ret < 0) {
        CLOGW("WARN(%s[%d]):GSC wait and pop return, ret(%d)", __FUNCTION__, __LINE__, ret);
    }

    if (callbackFrame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        goto CLEAN;
    }


    ret = callbackFrame->getDstBuffer(gscPipe, &postgscReprocessingDstBuffer);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, gscPipe, ret);
        goto CLEAN;
    }

#if 0 /* dump code */
    char filePath[70];
    memset(filePath, 0, sizeof(filePath));
    snprintf(filePath, sizeof(filePath), "/data/media/0/RawCapture%d_%d.rgb",
            m_exynosCameraParameters->getCameraId(), newFrame->getFrameCount());

    ret = dumpToFile((char *)filePath,
        postgscReprocessingDstBuffer.addr[0],
        FRAME_SIZE(HAL_PIXEL_FORMAT_RGBA_8888, thumbnailW, thumbnailH));
#endif

    m_postviewCallbackQ->pushProcessQ(&postgscReprocessingDstBuffer);

    CLOGD("DEBUG(%s[%d]):--OUT--", __FUNCTION__, __LINE__);

CLEAN:

    m_pictureFrameFactory->setFlagFlipIgnore(gscPipe, false);

    if (postgscReprocessingDstBuffer.index != -2 && ret < 0)
        m_putBuffers(dstbufferMgr, postgscReprocessingDstBuffer.index);

    if (postgscReprocessingSrcBuffer.index != -2)
        m_putBuffers(srcbufferMgr, postgscReprocessingSrcBuffer.index);

    if (callbackFrame != NULL) {
        callbackFrame->printEntity();
        CLOGD("DEBUG(%s[%d]): Reprocessing frame delete(%d)", __FUNCTION__, __LINE__, callbackFrame->getFrameCount());
        callbackFrame->decRef();
        m_frameMgr->deleteFrame(callbackFrame);
        callbackFrame = NULL;
    }

    return loop;
}

bool ExynosCamera::m_postPictureGscThreadFunc(void)
{
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);
    int loop = false;
    ExynosCameraFrame *newFrame = NULL;
    int ret = 0;
    int pipeId_postPictureGsc = PIPE_GSC_VIDEO;
    ExynosCameraBuffer postgscReprocessingBuffer;
    ExynosCameraBufferManager *bufferMgr = NULL;
    camera_memory_t *postviewCallbackHeap = NULL;
#ifdef SAMSUNG_BD
    int previewW = 0, previewH = 0;
    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;
    bool isThumbnailCallbackOn = false;
#if 0
    char filePath[70] = "/data/media/0/BD_input.yuv";
#endif
#endif

    postgscReprocessingBuffer.index = -2;

    if(m_exynosCameraParameters->isReprocessing() == true) {
        pipeId_postPictureGsc = PIPE_GSC_REPROCESSING2;
    } else {
        pipeId_postPictureGsc = PIPE_GSC_VIDEO;
    }

    m_getBufferManager(pipeId_postPictureGsc, &bufferMgr, DST_BUFFER_DIRECTION);

    /* wait GSC */
    CLOGI("INFO(%s[%d]):wait GSC output pipe(%d)", __FUNCTION__, __LINE__, pipeId_postPictureGsc);
    ret = dstPostPictureGscQ->waitAndPopProcessQ(&newFrame);
    CLOGI("INFO(%s[%d]):GSC output done pipe(%d)", __FUNCTION__, __LINE__, pipeId_postPictureGsc);
    if (ret < 0) {
        CLOGE("ERR(%s)(%d):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
       goto CLEAN;
    }

    if (newFrame == NULL) {
        CLOGE("ERR(%s):newFrame is NULL", __FUNCTION__);
        goto CLEAN;
    }

    ret = newFrame->setEntityState(pipeId_postPictureGsc, ENTITY_STATE_COMPLETE);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setEntityState(ENTITY_STATE_PROCESSING) fail, pipeId(%d), ret(%d)",
            __FUNCTION__, __LINE__, pipeId_postPictureGsc, ret);
        return ret;
    }

    /* put GCC buffer */
    ret = newFrame->getDstBuffer(pipeId_postPictureGsc, &postgscReprocessingBuffer);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
            __FUNCTION__, __LINE__, pipeId_postPictureGsc, ret);
        goto CLEAN;
    }

    if (m_exynosCameraParameters->getIsThumbnailCallbackOn()) {
        m_ThumbnailCallbackThread->run();
        m_thumbnailCallbackQ->pushProcessQ(&postgscReprocessingBuffer);
        CLOGD("DEBUG(%s[%d]): m_ThumbnailCallbackThread run", __FUNCTION__, __LINE__);
#ifdef SAMSUNG_BD
        isThumbnailCallbackOn = true;
#endif
    }
#ifdef SAMSUNG_MAGICSHOT
    else if(m_exynosCameraParameters->getShotMode() == SHOT_MODE_MAGIC) {
        if(m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_POSTVIEW_FRAME)) {
            if( m_burstCaptureCallbackCount < m_magicshotMaxCount ) {
                postviewCallbackHeap = m_getMemoryCb(postgscReprocessingBuffer.fd[0],
                                        postgscReprocessingBuffer.size[0], 1, m_callbackCookie);
                if (!postviewCallbackHeap || postviewCallbackHeap->data == MAP_FAILED) {
                    CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, postgscReprocessingBuffer.size[0]);
                    goto CLEAN;
                }
                setBit(&m_callbackState, CALLBACK_STATE_POSTVIEW_FRAME, true);
                m_dataCb(CAMERA_MSG_POSTVIEW_FRAME, postviewCallbackHeap, 0, NULL, m_callbackCookie);
                clearBit(&m_callbackState, CALLBACK_STATE_POSTVIEW_FRAME, true);
                postviewCallbackHeap->release(postviewCallbackHeap);
                CLOGD("magic shot POSTVIEW callback end(%s)(%d), pipe(%d), index(%d), count(%d), max(%d)",
                    __FUNCTION__, __LINE__, pipeId_postPictureGsc, postgscReprocessingBuffer.index,
                    m_burstCaptureCallbackCount,m_magicshotMaxCount);
            }
            ret = m_putBuffers(bufferMgr, postgscReprocessingBuffer.index);
        }
        return loop;
    }
#endif
#ifdef SAMSUNG_DEBLUR
    else if (m_deblurCaptureCount == 0
        && m_exynosCameraParameters->getLDCaptureMode() == MULTI_SHOT_MODE_DEBLUR) {
        m_detectDeblurCaptureThread->run();
        m_detectDeblurCaptureQ->pushProcessQ(&postgscReprocessingBuffer);
        CLOGD("DEBUG(%s[%d]): m_detectDeblurCaptureThread run", __FUNCTION__, __LINE__);
        return loop;
    }
#endif

#ifdef SAMSUNG_BD
        if(m_exynosCameraParameters->getSeriesShotMode() == SERIES_SHOT_MODE_BURST) {
            m_timer.start();

            UniPluginBufferData_t pluginData;
            m_exynosCameraParameters->getPreviewSize(&previewW, &previewH);
            memset(&pluginData, 0, sizeof(UniPluginBufferData_t));
            pluginData.InWidth = previewW;
            pluginData.InHeight = previewH;
            pluginData.InFormat = UNI_PLUGIN_FORMAT_NV21;
            pluginData.InBuffY = postgscReprocessingBuffer.addr[0];
            pluginData.InBuffU = postgscReprocessingBuffer.addr[1];
#if 0
            dumpToFile((char *)filePath,
                postgscReprocessingSrcBuffer.addr[0],
                (previewW * previewH * 3) / 2);
#endif
            if(m_BDpluginHandle != NULL) {
                ret = uni_plugin_set(m_BDpluginHandle,
                        BLUR_DETECTION_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &pluginData);
                if(ret < 0) {
                    CLOGE("[BD](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_BUFFER_INFO) failed!!", __FUNCTION__, __LINE__);
                }
                ret = uni_plugin_process(m_BDpluginHandle);
                if(ret < 0) {
                    CLOGE("[BD](%s[%d]): Plugin process failed!!", __FUNCTION__, __LINE__);
                }
                ret = uni_plugin_get(m_BDpluginHandle,
                    BLUR_DETECTION_PLUGIN_NAME, UNI_PLUGIN_INDEX_DEBUG_INFO, &m_BDbuffer[m_BDbufferIndex]);
                if(ret < 0) {
                    CLOGE("[BD](%s[%d]): Plugin get failed!!", __FUNCTION__, __LINE__);
                }
                CLOGD("[BD](%s[%d]): Push the buffer(Qsize:%d, Index: %d)",
                            __FUNCTION__, __LINE__, m_BDbufferQ->getSizeOfProcessQ(), m_BDbufferIndex);
                if(m_BDbufferQ->getSizeOfProcessQ() == MAX_BD_BUFF_NUM)
                    CLOGE("[BD](%s[%d]): All buffers are queued!! Skip the buffer!!", __FUNCTION__, __LINE__);
                else
                    m_BDbufferQ->pushProcessQ(&m_BDbuffer[m_BDbufferIndex]);

                if(++m_BDbufferIndex == MAX_BD_BUFF_NUM)
                    m_BDbufferIndex = 0;
            }

            m_timer.stop();
            durationTime = m_timer.durationMsecs();
            CLOGD("[BD](%s[%d]):Plugin duration time(%5d msec)", __FUNCTION__, __LINE__, (int)durationTime);
        }

    if (isThumbnailCallbackOn == false) {
        if (postgscReprocessingBuffer.index != -2)
            m_putBuffers(bufferMgr, postgscReprocessingBuffer.index);
        }
#endif

        return loop;

CLEAN:
    if (postgscReprocessingBuffer.index != -2)
        m_putBuffers(bufferMgr, postgscReprocessingBuffer.index);

    if (newFrame != NULL) {
        newFrame->printEntity();
        CLOGD("DEBUG(%s[%d]): Reprocessing frame delete(%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());
        newFrame->decRef();
        m_frameMgr->deleteFrame(newFrame);
        newFrame = NULL;
    }

    /* one shot */
    return loop;
}

#ifdef SAMSUNG_LBP
bool ExynosCamera::m_LBPThreadFunc(void)
{
    lbp_buffer_t LBPbuff;
    int ret;
    bool loop = true;

    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;
    int HwPreviewW = 0, HwPreviewH = 0;

    ret = m_LBPbufferQ->waitAndPopProcessQ(&LBPbuff);

    if (m_flagThreadStop == true) {
        CLOGI("INFO(%s[%d]):m_flagThreadStop(%d)", __FUNCTION__, __LINE__, m_flagThreadStop);
        m_LBPCount = 0;
        return false;
    }

    if(ret == TIMED_OUT || ret < 0) {
        CLOGE("ERR(%s)(%d):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        m_captureSelector->setBestFrameNum(0);
        m_LBPCount = 0;
        return false;
    }

    CLOGD("[LBP]: Buffer pushed(%d):%p, %p!!", LBPbuff.buffer.index, LBPbuff.buffer.addr[0], LBPbuff.buffer.addr[1]);
    m_LBPCount ++;

    if(m_LBPpluginHandle != NULL) {
        m_exynosCameraParameters->getHwPreviewSize(&HwPreviewW, &HwPreviewH);
        UniPluginBufferData_t pluginData;
        memset(&pluginData, 0, sizeof(UniPluginBufferData_t));
        pluginData.InBuffY= LBPbuff.buffer.addr[0];
        pluginData.InBuffU = LBPbuff.buffer.addr[1];
        pluginData.InWidth = HwPreviewW;
        pluginData.InHeight = HwPreviewH;

        m_timer.start();
        ret = uni_plugin_set(m_LBPpluginHandle, BESTPHOTO_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &pluginData);
        if(ret < 0)
            CLOGE("[LBP](%s[%d]):Bestphoto plugin set failed!!", __FUNCTION__, __LINE__);
        ret = uni_plugin_process(m_LBPpluginHandle);
        if(ret < 0)
            CLOGE("[LBP](%s[%d]):Bestphoto plugin process failed!!", __FUNCTION__, __LINE__);
        m_timer.stop();
        durationTime = m_timer.durationMsecs();
        CLOGD("[LBP](%s[%d]):duration time(%5d msec)", __FUNCTION__, __LINE__, (int)durationTime);
    }

    if(m_LBPCount == m_exynosCameraParameters->getHoldFrameCount()) {
        int bestPhotoIndex;
        ret = uni_plugin_get(m_LBPpluginHandle, BESTPHOTO_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INDEX, &bestPhotoIndex);
        if(ret < 0)
            CLOGE("[LBP](%s[%d]):Bestphoto plugin get failed!!", __FUNCTION__, __LINE__);

        CLOGD("[LBP]: Best photo index(%d), frame(%d)", bestPhotoIndex, m_LBPbuffer[bestPhotoIndex].frameNumber);
#if 0
        /* To check the performance */
        bool bRet;
        char filePath[70];
        for(int i = 0; i < m_LBPCount; i++) {
            memset(filePath, 0, sizeof(filePath));
            if(i == bestPhotoIndex)
                snprintf(filePath, sizeof(filePath), "/data/media/0/DCIM/Bestphoto_%d_Pick.yuv", m_LBPbuffer[i].frameNumber);
            else
                snprintf(filePath, sizeof(filePath), "/data/media/0/DCIM/Bestphoto_%d.yuv", m_LBPbuffer[i].frameNumber);

            bRet = dumpToFileYUV((char *)filePath,
                m_LBPbuffer[i].buffer.addr[0], m_LBPbuffer[i].buffer.addr[1],
                HwPreviewW, HwPreviewH);
            if (bRet != true)
                CLOGE("couldn't make a YUV file");
        }
#endif
        m_captureSelector->setBestFrameNum(m_LBPbuffer[bestPhotoIndex].frameNumber);
#ifdef SAMSUNG_JQ
        if(m_isJpegQtableOn == true) {
            m_isJpegQtableOn = false;
            ret = m_processJpegQtable(&m_LBPbuffer[bestPhotoIndex].buffer);
            if (ret < 0) {
                CLOGE("[JQ](%s[%d]): m_processJpegQtable() failed!!", __FUNCTION__, __LINE__);
            }
        }
#endif
    }

    if(m_LBPCount == m_exynosCameraParameters->getHoldFrameCount()) {
        m_LBPCount = 0;
        loop = false;
    }
    else
        loop = true;

    return loop;
}
#endif
#ifdef SAMSUNG_JQ
int ExynosCamera::m_processJpegQtable(ExynosCameraBuffer* buffer)
{
    int ret = 0;
    int HwPreviewW = 0, HwPreviewH = 0;
    UniPluginBufferData_t pluginData;

    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;

    m_timer.start();

    m_exynosCameraParameters->getHwPreviewSize(&HwPreviewW, &HwPreviewH);

    memset(&pluginData, 0, sizeof(UniPluginBufferData_t));
    pluginData.InBuffY = buffer->addr[0];
    pluginData.InBuffU = buffer->addr[1];
    pluginData.InWidth = HwPreviewW;
    pluginData.InHeight = HwPreviewH;
    pluginData.InFormat = UNI_PLUGIN_FORMAT_NV21;

    if(m_JQpluginHandle != NULL) {
        ret = uni_plugin_set(m_JQpluginHandle,
                JPEG_QTABLE_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &pluginData);
        if(ret < 0) {
            CLOGE("[JQ](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_BUFFER_INFO) failed!!", __FUNCTION__, __LINE__);
        }
        ret = uni_plugin_process(m_JQpluginHandle);
        if(ret < 0) {
            CLOGE("[JQ](%s[%d]): Plugin process failed!!", __FUNCTION__, __LINE__);
        }
        ret = uni_plugin_get(m_JQpluginHandle,
                JPEG_QTABLE_PLUGIN_NAME, UNI_PLUGIN_INDEX_JPEG_QTABLE, m_qtable);
        if(ret < 0) {
            CLOGE("[JQ](%s[%d]): Plugin get(UNI_PLUGIN_INDEX_JPEG_QTABLE) failed!!", __FUNCTION__, __LINE__);
        }

        CLOGD("[JQ](%s[%d]): Qtable Set Done!!", __FUNCTION__, __LINE__);
        m_exynosCameraParameters->setJpegQtable(m_qtable);
        m_exynosCameraParameters->setJpegQtableStatus(JPEG_QTABLE_UPDATED);
    }

    m_timer.stop();
    durationTime = m_timer.durationMsecs();
    CLOGD("[JQ](%s[%d]):duration time(%5d msec)", __FUNCTION__, __LINE__, (int)durationTime);

    return ret;
}
#endif

bool ExynosCamera::m_CheckCameraSavingPath(char *dir, char* srcPath, char *dstPath, int dstSize)
{
    int ret = false;

    struct dirent **items = NULL;
    struct stat fstat;
    int item_count, i;
    char ChangeDirPath[CAMERA_FILE_PATH_SIZE] = {'\0',};

    memset(dstPath, 0, dstSize);

    // Check access path
    if (srcPath && dstSize > (int)strlen(srcPath)) {
        strncpy(dstPath, srcPath, dstSize - 1);
    } else {
        CLOGW("WARN(%s[%d]) Parameter srcPath is NULL. Change to Default Path", __FUNCTION__, __LINE__);
        snprintf(dstPath, dstSize, "%s/DCIM/Camera/", dir);
    }

    if (access(dstPath, 0)==0) {
        CLOGW("WARN(%s[%d]) success access dir = %s", __FUNCTION__, __LINE__, dstPath);
        return true;
    }

    CLOGW("WARN(%s[%d]) can't access dir = %s, root dir = %s", __FUNCTION__, __LINE__, m_burstSavePath, dir);

    // If directory cant't access, then search "DCIM/Camera" folder in current directory
    item_count = scandir(dir, &items, NULL, alphasort);
    for (i = 0 ; i < item_count ; i++) {
        // Search only dcim directory
        lstat(items[i]->d_name, &fstat);
        if ((fstat.st_mode & S_IFDIR) == S_IFDIR) {
            if (!strcmp(items[i]->d_name, ".") || !strcmp(items[i]->d_name, ".."))
                continue;
            if (strcasecmp(items[i]->d_name, "DCIM")==0) {
                sprintf(ChangeDirPath, "%s/%s", dir, items[i]->d_name);
                break;
            }
        }
    }

    if (items != NULL) {
        for (i = 0; i < item_count; i++) {
             if (items[i] != NULL) {
                free(items[i]);
             }
        }
        free(items);
        items = NULL;
    }

    // If "DCIM" directory exists, search "CAMERA" directory
    if (ChangeDirPath[0] != '\0') {
        item_count = scandir(ChangeDirPath, &items, NULL, alphasort);
        for (i = 0; i < item_count; i++) {
            // Search only camera directory
            lstat(items[i]->d_name, &fstat);
            if ((fstat.st_mode & S_IFDIR) == S_IFDIR) {
                if (!strcmp(items[i]->d_name, ".") || !strcmp(items[i]->d_name, ".."))
                    continue;
                if (strcasecmp(items[i]->d_name, "CAMERA")==0) {
                    sprintf(dstPath, "%s/%s/", ChangeDirPath, items[i]->d_name);
                    CLOGW("WARN(%s[%d]) change save path = %s", __FUNCTION__, __LINE__, dstPath);
                    ret = true;
                    break;
                }
            }
        }

        if (items != NULL) {
            for (i = 0; i < item_count; i++) {
                if (items[i] != NULL) {
                    free(items[i]);
                }
            }
            free(items);
        }
    }

    return ret;
}

bool ExynosCamera::m_FileSaveFunc(char *filePath, ExynosCameraBuffer *SaveBuf)
{
    int fd = -1;
    int nw, cnt = 0;
    uint32_t written = 0;
    char *data;

    fd = open(filePath, O_RDWR | O_CREAT, 0664);
    if (fd < 0) {
        CLOGD("DEBUG(%s[%d]):failed to create file [%s]: %s",
            __FUNCTION__, __LINE__, filePath, strerror(errno));
        goto SAVE_ERR;
    }

    data = SaveBuf->addr[0];

    CLOGD("DEBUG(%s[%d]):(%s)file write start)", __FUNCTION__, __LINE__, filePath);
    while (written < SaveBuf->size[0]) {
        nw = ::write(fd, (const char *)(data) + written, SaveBuf->size[0] - written);

        if (nw < 0) {
            CLOGD("DEBUG(%s[%d]):failed to write file [%s]: %s",
                __FUNCTION__, __LINE__, filePath, strerror(errno));
            break;
        }

        written += nw;
        cnt++;
    }
    CLOGD("DEBUG(%s[%d]):(%s)file write end)", __FUNCTION__, __LINE__, filePath);

    if (fd > 0)
        ::close(fd);

    if (chmod(filePath, 0664) < 0) {
        CLOGE("failed chmod [%s]", filePath);
    }
    if (chown(filePath, AID_MEDIA, AID_MEDIA_RW) < 0) {
        CLOGE("failed chown [%s] user(%d), group(%d)", filePath, AID_MEDIA, AID_MEDIA_RW);
    }

    return true;

SAVE_ERR:

    return false;
}

bool ExynosCamera::m_pictureThreadFunc(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    int ret = 0;
    int loop = false;
    int bufIndex = -2;
    int retryCountGSC = 4;

    ExynosCameraFrame *newFrame = NULL;

    ExynosCameraBuffer sccReprocessingBuffer;
    ExynosCameraBufferManager *bufferMgr = NULL;
    struct camera2_stream *shot_stream = NULL;
    ExynosRect srcRect, dstRect;
    int pictureW = 0, pictureH = 0, pictureFormat = 0;
    int hwPictureW = 0, hwPictureH = 0, hwPictureFormat = 0;
    int buffer_idx = getShotBufferIdex();
    float zoomRatio = m_exynosCameraParameters->getZoomRatio(0) / 1000;

    sccReprocessingBuffer.index = -2;

    int pipeId_scc = 0;
    int pipeId_gsc = 0;
    bool isSrc = false;

    if (m_exynosCameraParameters->isReprocessing() == true) {
        if (m_exynosCameraParameters->isReprocessing3aaIspOTF() == false)
            pipeId_scc = (isOwnScc(getCameraId()) == true) ? PIPE_SCC_REPROCESSING : PIPE_ISP_REPROCESSING;
        else
            pipeId_scc = PIPE_3AA_REPROCESSING;

        pipeId_gsc = PIPE_GSC_REPROCESSING;
        isSrc = true;
    } else {
        switch (getCameraId()) {
            case CAMERA_ID_FRONT:
                if (m_exynosCameraParameters->getDualMode() == true) {
                    pipeId_scc = PIPE_3AA;
                } else {
                    pipeId_scc = (isOwnScc(getCameraId()) == true) ? PIPE_SCC : PIPE_ISPC;
                }
                pipeId_gsc = PIPE_GSC_PICTURE;
                break;
            default:
                CLOGE("ERR(%s[%d]):Current picture mode is not yet supported, CameraId(%d), reprocessing(%d)",
                    __FUNCTION__, __LINE__, getCameraId(), m_exynosCameraParameters->isReprocessing());
                break;
        }
    }

    /* wait SCC */
    CLOGI("INFO(%s[%d]):wait SCC output", __FUNCTION__, __LINE__);
    int retry = 0;
    do {
        ret = dstSccReprocessingQ->waitAndPopProcessQ(&newFrame);
        retry++;
    } while (ret == TIMED_OUT && retry < 40 &&
             (m_takePictureCounter.getCount() > 0 || m_exynosCameraParameters->getSeriesShotCount() == 0));

    if (ret < 0) {
        CLOGW("WARN(%s[%d]):wait and pop fail, ret(%d), retry(%d), takePictuerCount(%d), seriesShotCount(%d)",
                __FUNCTION__, __LINE__, ret, retry, m_takePictureCounter.getCount(), m_exynosCameraParameters->getSeriesShotCount());
        // TODO: doing exception handling
        goto CLEAN;
    }

    if (newFrame == NULL) {
        CLOGE("ERR(%s[%d]):newFrame is NULL", __FUNCTION__, __LINE__);
        goto CLEAN;
    }

    if (m_postProcessList.empty()) {
        CLOGE("ERR(%s[%d]): postPictureList is empty", __FUNCTION__, __LINE__);
        usleep(5000);
        if(m_postProcessList.empty()) {
            CLOGE("ERR(%s[%d]):Retry but postPictureList is empty", __FUNCTION__, __LINE__);
            goto CLEAN;
        }
    }

    /*
     * When Non-reprocessing scenario does not setEntityState,
     * because Non-reprocessing scenario share preview and capture frames
     */
    if (m_exynosCameraParameters->isReprocessing() == true) {
        ret = newFrame->setEntityState(pipeId_scc, ENTITY_STATE_COMPLETE);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):setEntityState(ENTITY_STATE_COMPLETE) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_scc, ret);
            return ret;
        }
    }

    CLOGI("INFO(%s[%d]):SCC output done, frame Count(%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());

    /* Shutter Callback */
    if (pipeId_scc != PIPE_SCC_REPROCESSING &&
        pipeId_scc != PIPE_ISP_REPROCESSING) {
        if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_SHUTTER)) {
            CLOGI("INFO(%s[%d]): CAMERA_MSG_SHUTTER callback S", __FUNCTION__, __LINE__);
#ifdef BURST_CAPTURE
            m_notifyCb(CAMERA_MSG_SHUTTER, m_exynosCameraParameters->getSeriesShotDuration(), 0, m_callbackCookie);
#else
            m_notifyCb(CAMERA_MSG_SHUTTER, 0, 0, m_callbackCookie);
#endif
            CLOGI("INFO(%s[%d]): CAMERA_MSG_SHUTTER callback E", __FUNCTION__, __LINE__);
        }
    }

#ifdef SAMSUNG_DOF
    if (m_exynosCameraParameters->getShotMode() == SHOT_MODE_OUTFOCUS &&
            m_exynosCameraParameters->getMoveLensTotal() > 0) {
        CLOGD("[DOF][%s][%d] Lens moved count: %d",
                "m_pictureThreadFunc", __LINE__, m_lensmoveCount);
        CLOGD("[DOF][%s][%d] Total count : %d",
                "m_pictureThreadFunc", __LINE__, m_exynosCameraParameters->getMoveLensTotal());

        ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();

        if(m_lensmoveCount < m_exynosCameraParameters->getMoveLensTotal()) {
            CLOGD("[DOF][%s][%d] callback done!!: %d",
                "m_pictureThreadFunc", __LINE__, m_lensmoveCount);
            autoFocusMgr->setStartLensMove(true);
            m_lensmoveCount++;
            m_exynosCameraParameters->setMoveLensCount(m_lensmoveCount);
            m_exynosCameraParameters->m_setLensPosition(m_lensmoveCount);
        }
        else if(m_lensmoveCount == m_exynosCameraParameters->getMoveLensTotal() ){
            CLOGD("[DOF][%s][%d] Last callback done!!: %d",
                "m_pictureThreadFunc", __LINE__, m_lensmoveCount);
            autoFocusMgr->setStartLensMove(false);

            m_lensmoveCount = 0;
            m_exynosCameraParameters->setMoveLensCount(m_lensmoveCount);
            m_exynosCameraParameters->setMoveLensTotal(m_lensmoveCount);
        }else {
            CLOGE("[DOF][%s][%d] unexpected error!!: %d",
                "m_pictureThreadFunc", __LINE__, m_lensmoveCount);
            autoFocusMgr->setStartLensMove(false);
            m_lensmoveCount = 0;
            m_exynosCameraParameters->setMoveLensCount(m_lensmoveCount);
            m_exynosCameraParameters->setMoveLensTotal(m_lensmoveCount);
        }
    }
#endif

#ifdef SAMSUNG_DNG
    struct camera2_dm *dm;
    struct camera2_udm *udm;

    dm = new struct camera2_dm;
    udm = new struct camera2_udm;

    newFrame->getDynamicMeta(dm);
    newFrame->getUserDynamicMeta(udm);

    if(m_exynosCameraParameters->getDNGCaptureModeOn() &&
        (m_dngFrameNumber == dm->request.frameCount ||
        m_exynosCameraParameters->getCaptureExposureTime() > CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT)) {
        m_exynosCameraParameters->setDngChangedAttribute(dm, udm);
        m_exynosCameraParameters->setIsUsefulDngInfo(true);
    }

    delete dm;
    delete udm;
#endif

#ifdef SAMSUNG_DEBLUR
    if (m_deblurCaptureCount == MAX_DEBLUR_CAPTURE_COUNT - 1) {
        if (getUseDeblurCaptureOn()) {
            m_pictureCounter.decCount();
            CLOGE("DEBUG(%s[%d]):use De-blur Capture m_pictureCounter(%d)",
                __FUNCTION__, __LINE__, m_pictureCounter.getCount());
            goto CLEAN;
        } else {
            m_exynosCameraParameters->setIsThumbnailCallbackOn(true);
            CLOGE("DEBUG(%s[%d]):Didn't use De-blur Capture m_pictureCounter(%d)",
                __FUNCTION__, __LINE__, m_pictureCounter.getCount());
        }
    }
#endif

    if (needGSCForCapture(getCameraId()) == true) {
        /* set GSC buffer */
        if (m_exynosCameraParameters->isReprocessing() == true)
            ret = newFrame->getDstBuffer(pipeId_scc, &sccReprocessingBuffer, m_reprocessingFrameFactory->getNodeType(PIPE_ISPC_REPROCESSING));
        else
            ret = newFrame->getDstBuffer(pipeId_scc, &sccReprocessingBuffer, m_previewFrameFactory->getNodeType(PIPE_ISPC));

        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipeId_scc, ret);
            goto CLEAN;
        }

        if (m_exynosCameraParameters->getIsThumbnailCallbackOn()) {
            if (m_ThumbnailCallbackThread->isRunning()) {
                m_ThumbnailCallbackThread->join();
                CLOGD("DEBUG(%s[%d]): m_ThumbnailCallbackThread join", __FUNCTION__, __LINE__);
            }
        }

        if (m_exynosCameraParameters->getIsThumbnailCallbackOn()
#ifdef SAMSUNG_MAGICSHOT
            || m_exynosCameraParameters->getShotMode() == SHOT_MODE_MAGIC
#endif
#ifdef SAMSUNG_DEBLUR
            || (m_deblurCaptureCount == 0 && m_exynosCameraParameters->getLDCaptureMode() == MULTI_SHOT_MODE_DEBLUR)
#endif
#ifdef SAMSUNG_BD
            || m_exynosCameraParameters->getSeriesShotMode() == SERIES_SHOT_MODE_BURST
#endif
        ) {
            int pipeId_postPictureGsc = PIPE_GSC_REPROCESSING2;
            ExynosCameraBuffer postgscReprocessingBuffer;
            int previewW = 0, previewH = 0, previewFormat = 0;
            ExynosRect srcRect_postPicturegGsc, dstRect_postPicturegGsc;

            postgscReprocessingBuffer.index = -2;
            CLOGD("POSTVIEW callback start, m_postPictureGscThread run (%s)(%d) pipe(%d)",
                __FUNCTION__, __LINE__, pipeId_postPictureGsc);
            shot_stream = (struct camera2_stream *)(sccReprocessingBuffer.addr[buffer_idx]);

            if (shot_stream != NULL) {
                CLOGD("DEBUG(%s[%d]):(%d %d %d %d)", __FUNCTION__, __LINE__,
                    shot_stream->fcount,
                    shot_stream->rcount,
                    shot_stream->findex,
                    shot_stream->fvalid);
                CLOGD("DEBUG(%s[%d]):(%d %d %d %d)(%d %d %d %d)", __FUNCTION__, __LINE__,
                    shot_stream->input_crop_region[0],
                    shot_stream->input_crop_region[1],
                    shot_stream->input_crop_region[2],
                    shot_stream->input_crop_region[3],
                    shot_stream->output_crop_region[0],
                    shot_stream->output_crop_region[1],
                    shot_stream->output_crop_region[2],
                    shot_stream->output_crop_region[3]);
            } else {
                CLOGE("DEBUG(%s[%d]):shot_stream is NULL", __FUNCTION__, __LINE__);
            }

            int retry1 = 0;
            do {
                ret = -1;
                retry1++;
                if (m_postPictureGscBufferMgr->getNumOfAvailableBuffer() > 0) {
                    ret = m_setupEntity(pipeId_postPictureGsc, newFrame, &sccReprocessingBuffer, NULL);
                } else {
                    /* wait available SCC buffer */
                    usleep(WAITING_TIME);
                }
                if (retry1 % 10 == 0)
                    CLOGW("WRAN(%s[%d]):retry setupEntity for GSC", __FUNCTION__, __LINE__);
            } while(ret < 0 && retry1 < (TOTAL_WAITING_TIME/WAITING_TIME));

            if (ret < 0) {
                CLOGE("ERR(%s[%d]):setupEntity fail, pipeId(%d), retry(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipeId_postPictureGsc, retry1, ret);
                goto CLEAN;
            }

            m_exynosCameraParameters->getPreviewSize(&previewW, &previewH);
            m_exynosCameraParameters->getPictureSize(&pictureW, &pictureH);
            pictureFormat = m_exynosCameraParameters->getPictureFormat();
            previewFormat = m_exynosCameraParameters->getPreviewFormat();

            CLOGD("DEBUG(%s):size preview(%d, %d,format:%d)picture(%d, %d,format:%d)", __FUNCTION__,
                previewW, previewH, previewFormat, pictureW, pictureH, pictureFormat);

            srcRect_postPicturegGsc.x = shot_stream->input_crop_region[0];
            srcRect_postPicturegGsc.y = shot_stream->input_crop_region[1];
            srcRect_postPicturegGsc.w = shot_stream->input_crop_region[2];
            srcRect_postPicturegGsc.h = shot_stream->input_crop_region[3];
            srcRect_postPicturegGsc.fullW = shot_stream->input_crop_region[2];
            srcRect_postPicturegGsc.fullH = shot_stream->input_crop_region[3];
            srcRect_postPicturegGsc.colorFormat = pictureFormat;
            dstRect_postPicturegGsc.x = 0;
            dstRect_postPicturegGsc.y = 0;
            dstRect_postPicturegGsc.w = previewW;
            dstRect_postPicturegGsc.h = previewH;
            dstRect_postPicturegGsc.fullW = previewW;
            dstRect_postPicturegGsc.fullH = previewH;
            dstRect_postPicturegGsc.colorFormat = HAL_PIXEL_FORMAT_2_V4L2_PIX(HAL_PIXEL_FORMAT_YCrCb_420_SP);

            ret = getCropRectAlign(srcRect_postPicturegGsc.w, srcRect_postPicturegGsc.h,
                previewW,   previewH,
                &srcRect_postPicturegGsc.x, &srcRect_postPicturegGsc.y,
                &srcRect_postPicturegGsc.w, &srcRect_postPicturegGsc.h,
                2, 2, 0, zoomRatio);

            ret = newFrame->setSrcRect(pipeId_postPictureGsc, &srcRect_postPicturegGsc);
            ret = newFrame->setDstRect(pipeId_postPictureGsc, &dstRect_postPicturegGsc);

            CLOGD("DEBUG(%s):size (%d, %d, %d, %d %d %d)", __FUNCTION__,
                srcRect_postPicturegGsc.x, srcRect_postPicturegGsc.y, srcRect_postPicturegGsc.w,
                srcRect_postPicturegGsc.h, srcRect_postPicturegGsc.fullW, srcRect_postPicturegGsc.fullH);
            CLOGD("DEBUG(%s):size (%d, %d, %d, %d %d %d)", __FUNCTION__,
                dstRect_postPicturegGsc.x, dstRect_postPicturegGsc.y, dstRect_postPicturegGsc.w,
                dstRect_postPicturegGsc.h, dstRect_postPicturegGsc.fullW, dstRect_postPicturegGsc.fullH);

            /* push frame to GSC pipe */
            m_pictureFrameFactory->setOutputFrameQToPipe(dstPostPictureGscQ, pipeId_postPictureGsc);
            m_pictureFrameFactory->pushFrameToPipe(&newFrame, pipeId_postPictureGsc);

            ret = m_postPictureGscThread->run();

            if (ret < 0) {
                CLOGE("ERR(%s[%d]):couldn't run m_postPictureGscThread, ret(%d)", __FUNCTION__, __LINE__, ret);
                return INVALID_OPERATION;
            }
        }

        shot_stream = (struct camera2_stream *)(sccReprocessingBuffer.addr[buffer_idx]);
        if (shot_stream != NULL) {
            CLOGD("DEBUG(%s[%d]):(%d %d %d %d)", __FUNCTION__, __LINE__,
                shot_stream->fcount,
                shot_stream->rcount,
                shot_stream->findex,
                shot_stream->fvalid);
            CLOGD("DEBUG(%s[%d]):(%d %d %d %d)(%d %d %d %d)", __FUNCTION__, __LINE__,
                shot_stream->input_crop_region[0],
                shot_stream->input_crop_region[1],
                shot_stream->input_crop_region[2],
                shot_stream->input_crop_region[3],
                shot_stream->output_crop_region[0],
                shot_stream->output_crop_region[1],
                shot_stream->output_crop_region[2],
                shot_stream->output_crop_region[3]);
        } else {
            CLOGE("DEBUG(%s[%d]):shot_stream is NULL", __FUNCTION__, __LINE__);
            goto CLEAN;
        }

        int retry = 0;
        m_getBufferManager(pipeId_gsc, &bufferMgr, DST_BUFFER_DIRECTION);
        do {
            ret = -1;
            retry++;
            if (bufferMgr->getNumOfAvailableBuffer() > 0) {
                ret = m_setupEntity(pipeId_gsc, newFrame, &sccReprocessingBuffer, NULL);
            } else {
                /* wait available SCC buffer */
                usleep(WAITING_TIME);
            }

            if (retry % 10 == 0) {
                CLOGW("WRAN(%s[%d]):retry setupEntity for GSC postPictureQ(%d), saveQ0(%d), saveQ1(%d), saveQ2(%d)",
                        __FUNCTION__, __LINE__,
                        m_postPictureQ->getSizeOfProcessQ(),
                        m_jpegSaveQ[JPEG_SAVE_THREAD0]->getSizeOfProcessQ(),
                        m_jpegSaveQ[JPEG_SAVE_THREAD1]->getSizeOfProcessQ(),
                        m_jpegSaveQ[JPEG_SAVE_THREAD2]->getSizeOfProcessQ());
            }
        } while(ret < 0 && retry < (TOTAL_WAITING_TIME/WAITING_TIME) && m_stopBurstShot == false);

        if (ret < 0) {
            if (retry >= (TOTAL_WAITING_TIME/WAITING_TIME)) {
                CLOGE("ERR(%s[%d]):setupEntity fail, pipeId(%d), retry(%d), ret(%d), m_stopBurstShot(%d)",
                        __FUNCTION__, __LINE__, pipeId_gsc, retry, ret, m_stopBurstShot);
		/* HACK for debugging P150108-08143 */
		bufferMgr->printBufferState();
                android_printAssert(NULL, LOG_TAG, "BURST_SHOT_TIME_ASSERT(%s[%d]):\
                    unexpected error, get GSC buffer failed, assert!!!!", __FUNCTION__, __LINE__);
            } else {
                CLOGD("DEBUG(%s[%d]):setupEntity stopped, pipeId(%d), retry(%d), ret(%d), m_stopBurstShot(%d)",
                        __FUNCTION__, __LINE__, pipeId_gsc, retry, ret, m_stopBurstShot);
            }
            goto CLEAN;
        }
/* should change size calculation code in pure bayer */
#if 0
        if (shot_stream != NULL) {
            ret = m_calcPictureRect(&srcRect, &dstRect);
            ret = newFrame->setSrcRect(pipeId_gsc, &srcRect);
            ret = newFrame->setDstRect(pipeId_gsc, &dstRect);
        }
#else
        m_exynosCameraParameters->getPictureSize(&pictureW, &pictureH);
        pictureFormat = m_exynosCameraParameters->getPictureFormat();
#if 1   /* HACK in case of 3AA-OTF-ISP input_cropRegion always 0, use output crop region, check the driver */
        srcRect.x = shot_stream->output_crop_region[0];
        srcRect.y = shot_stream->output_crop_region[1];
        srcRect.w = shot_stream->output_crop_region[2];
        srcRect.h = shot_stream->output_crop_region[3];
#endif
        srcRect.fullW = shot_stream->output_crop_region[2];
        srcRect.fullH = shot_stream->output_crop_region[3];
        srcRect.colorFormat = pictureFormat;

        if(m_exynosCameraParameters->getSeriesShotMode() == SERIES_SHOT_MODE_LLS
            || m_exynosCameraParameters->getShotMode() == SHOT_MODE_OUTFOCUS
#ifdef SAMSUNG_LLS_DEBLUR
            || (m_exynosCameraParameters->getLDCaptureMode()
#ifdef SAMSUNG_DEBLUR
                && (m_deblurCaptureCount < m_exynosCameraParameters->getLDCaptureCount() - 1)
#endif
                )
#endif
        ) {
            pictureFormat = V4L2_PIX_FMT_NV21;
        } else {
            pictureFormat = JPEG_INPUT_COLOR_FMT;
        }

#ifdef SR_CAPTURE
        if(m_exynosCameraParameters->getSROn() &&
            (m_exynosCameraParameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON ||
            m_exynosCameraParameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC)) {
            dstRect.x = 0;
            dstRect.y = 0;
            dstRect.w = shot_stream->output_crop_region[2];
            dstRect.h = shot_stream->output_crop_region[3];
            dstRect.fullW = shot_stream->output_crop_region[2];
            dstRect.fullH = shot_stream->output_crop_region[3];
            dstRect.colorFormat = pictureFormat;
        } else
#endif
        {
            dstRect.x = 0;
            dstRect.y = 0;
            dstRect.w = pictureW;
            dstRect.h = pictureH;
            dstRect.fullW = pictureW;
            dstRect.fullH = pictureH;
            dstRect.colorFormat = pictureFormat;
        }
        ret = getCropRectAlign(srcRect.w,  srcRect.h,
                               pictureW,   pictureH,
                               &srcRect.x, &srcRect.y,
                               &srcRect.w, &srcRect.h,
                               2, 2, 0, zoomRatio);

        ret = newFrame->setSrcRect(pipeId_gsc, &srcRect);
        ret = newFrame->setDstRect(pipeId_gsc, &dstRect);
#endif

        CLOGD("DEBUG(%s):size (%d, %d, %d, %d %d %d)", __FUNCTION__,
            srcRect.x, srcRect.y, srcRect.w, srcRect.h, srcRect.fullW, srcRect.fullH);
        CLOGD("DEBUG(%s):size (%d, %d, %d, %d %d %d)", __FUNCTION__,
            dstRect.x, dstRect.y, dstRect.w, dstRect.h, dstRect.fullW, dstRect.fullH);

        /* push frame to GSC pipe */
        m_pictureFrameFactory->setOutputFrameQToPipe(dstGscReprocessingQ, pipeId_gsc);
        m_pictureFrameFactory->pushFrameToPipe(&newFrame, pipeId_gsc);

        /* wait GSC */
        newFrame = NULL;
        CLOGI("INFO(%s[%d]):wait GSC output", __FUNCTION__, __LINE__);
        while (retryCountGSC > 0) {
            ret = dstGscReprocessingQ->waitAndPopProcessQ(&newFrame);
            if (ret == TIMED_OUT) {
                CLOGW("WRN(%s)(%d):wait and pop timeout, ret(%d)", __FUNCTION__, __LINE__, ret);
                m_pictureFrameFactory->startThread(pipeId_gsc);
            } else if (ret < 0) {
                CLOGE("ERR(%s)(%d):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
                /* TODO: doing exception handling */
                goto CLEAN;
            } else if (m_stopBurstShot == true) {
                CLOGD("DEBUG(%s)(%d):stopburst! ret(%d)", __FUNCTION__, __LINE__, ret);
                goto CLEAN;
            } else {
                break;
            }
            retryCountGSC--;
        }

        if (newFrame == NULL) {
            CLOGE("ERR(%s):newFrame is NULL", __FUNCTION__);
            goto CLEAN;
        }
        CLOGI("INFO(%s[%d]):GSC output done", __FUNCTION__, __LINE__);

        if (m_exynosCameraParameters->getIsThumbnailCallbackOn()
#ifdef SAMSUNG_MAGICSHOT
            || m_exynosCameraParameters->getShotMode() == SHOT_MODE_MAGIC
#endif
#ifdef SAMSUNG_DEBLUR
            || (m_deblurCaptureCount == 0 && m_exynosCameraParameters->getLDCaptureMode() == MULTI_SHOT_MODE_DEBLUR)
#endif
#ifdef SAMSUNG_BD
            || m_exynosCameraParameters->getSeriesShotMode() == SERIES_SHOT_MODE_BURST
#endif
        ) {
            m_postPictureGscThread->join();
        }

#ifdef SR_CAPTURE
        if(m_exynosCameraParameters->getSROn()) {
            if (m_exynosCameraParameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON ||
                m_exynosCameraParameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC) {
                m_sr_cropped_width = shot_stream->output_crop_region[2];
                m_sr_cropped_height = shot_stream->output_crop_region[3];
            } else {
                m_sr_cropped_width = pictureW;
                m_sr_cropped_height = pictureH;
            }
            CLOGD("DEBUG(%s):size (%d, %d)", __FUNCTION__, m_sr_cropped_width, m_sr_cropped_height);
        }
#endif

        /* put SCC buffer */
        if (m_exynosCameraParameters->isReprocessing() == true)
            ret = newFrame->getDstBuffer(pipeId_scc, &sccReprocessingBuffer, m_reprocessingFrameFactory->getNodeType(PIPE_ISPC_REPROCESSING));
        else
            ret = newFrame->getDstBuffer(pipeId_scc, &sccReprocessingBuffer, m_previewFrameFactory->getNodeType(PIPE_ISPC));

        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_scc, ret);
            goto CLEAN;
        }

        m_getBufferManager(pipeId_scc, &bufferMgr, DST_BUFFER_DIRECTION);
        ret = m_putBuffers(bufferMgr, sccReprocessingBuffer.index);
        if (ret < 0) {
            CLOGE("ERR(%s)(%d):m_putBuffers fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: doing exception handling */
            goto CLEAN;
        }
    }

#ifdef SAMSUNG_DEBLUR
    if(m_exynosCameraParameters->getLDCaptureMode() == MULTI_SHOT_MODE_DEBLUR) {
        m_deblurCaptureCount++;
        CLOGD("DEBUG(%s[%d]): m_deblurCaptureCoun(%d)", __FUNCTION__, __LINE__, m_deblurCaptureCount);
    }

    if (m_exynosCameraParameters->getLDCaptureMode() == MULTI_SHOT_MODE_DEBLUR
        && m_deblurCaptureCount < m_exynosCameraParameters->getLDCaptureCount()) {
        m_deblurCaptureQ->pushProcessQ(&newFrame);
    } else
#endif
    {
#ifdef SAMSUNG_LLS_DEBLUR
        if (m_exynosCameraParameters->getLDCaptureMode() > 0
            && m_exynosCameraParameters->getLDCaptureMode() != MULTI_SHOT_MODE_DEBLUR) {
            m_LDCaptureQ->pushProcessQ(&newFrame);
        } else
#endif
        {
            /* push postProcess */
            m_postPictureQ->pushProcessQ(&newFrame);
        }
    }
    m_pictureCounter.decCount();

    CLOGI("INFO(%s[%d]):picture thread complete, remaining count(%d)", __FUNCTION__, __LINE__, m_pictureCounter.getCount());

    if (m_pictureCounter.getCount() > 0) {
        loop = true;
    } else {
        if (m_exynosCameraParameters->isReprocessing() == true)
            CLOGD("DEBUG(%s[%d]): ", __FUNCTION__, __LINE__);
        else {
            if (m_exynosCameraParameters->getUseDynamicScc() == true) {
                CLOGD("DEBUG(%s[%d]): Use dynamic bayer", __FUNCTION__, __LINE__);

                if (isOwnScc(getCameraId()) == true)
                    m_previewFrameFactory->setRequestSCC(false);
                else
                    m_previewFrameFactory->setRequestISPC(false);
            }

            m_sccCaptureSelector->clearList(pipeId_scc, isSrc, m_pictureFrameFactory->getNodeType(PIPE_ISPC));
        }

        dstSccReprocessingQ->release();
    }

    /* one shot */
    return loop;

CLEAN:
    if (sccReprocessingBuffer.index != -2) {
        CLOGD("DEBUG(%s[%d]): putBuffer sccReprocessingBuffer(index:%d) in error state",
                __FUNCTION__, __LINE__, sccReprocessingBuffer.index);
        m_getBufferManager(pipeId_scc, &bufferMgr, DST_BUFFER_DIRECTION);
        m_putBuffers(bufferMgr, sccReprocessingBuffer.index);
    }

#ifdef SAMSUNG_DEBLUR
    if(!getUseDeblurCaptureOn()) {
        CLOGI("INFO(%s[%d]):take picture fail, remaining count(%d)", __FUNCTION__, __LINE__, m_pictureCounter.getCount());
    }
#endif

    if (m_pictureCounter.getCount() > 0)
        loop = true;

    /* one shot */
    return loop;
}

camera_memory_t *ExynosCamera::m_getJpegCallbackHeap(ExynosCameraBuffer jpegBuf, int seriesShotNumber)
{
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    camera_memory_t *jpegCallbackHeap = NULL;

#ifdef BURST_CAPTURE
    if (1 < m_exynosCameraParameters->getSeriesShotCount()) {
        int seriesShotSaveLocation = m_exynosCameraParameters->getSeriesShotSaveLocation();

        if (seriesShotNumber < 0 || seriesShotNumber > m_exynosCameraParameters->getSeriesShotCount()) {
             CLOGE("ERR(%s[%d]): Invalid shot number (%d)", __FUNCTION__, __LINE__, seriesShotNumber);
             goto done;
        }

        if (seriesShotSaveLocation == BURST_SAVE_CALLBACK) {
            CLOGD("DEBUG(%s[%d]):burst callback : size (%d), count(%d)", __FUNCTION__, __LINE__, jpegBuf.size[0], seriesShotNumber);

            jpegCallbackHeap = m_getMemoryCb(jpegBuf.fd[0], jpegBuf.size[0], 1, m_callbackCookie);
            if (!jpegCallbackHeap || jpegCallbackHeap->data == MAP_FAILED) {
                CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, jpegBuf.size[0]);
                goto done;
            }
            if (jpegBuf.fd[0] < 0)
                memcpy(jpegCallbackHeap->data, jpegBuf.addr[0], jpegBuf.size[0]);
        } else {
            char filePath[CAMERA_FILE_PATH_SIZE];
            int nw, cnt = 0;
            uint32_t written = 0;
            camera_memory_t *tempJpegCallbackHeap = NULL;

            memset(filePath, 0, sizeof(filePath));

#if defined(SAMSUNG_INF_BURST)
            snprintf(filePath, sizeof(filePath), "%s%s_%03d.jpg",
                    m_burstSavePath, m_burstTime, seriesShotNumber);
#else
            snprintf(filePath, sizeof(filePath), "%sBurst%02d.jpg", m_burstSavePath, seriesShotNumber);
#endif
            CLOGD("DEBUG(%s[%d]):burst callback : size (%d), filePath(%s)", __FUNCTION__, __LINE__, jpegBuf.size[0], filePath);

            jpegCallbackHeap = m_getMemoryCb(-1, sizeof(filePath), 1, m_callbackCookie);
            if (!jpegCallbackHeap || jpegCallbackHeap->data == MAP_FAILED) {
                CLOGE("ERR(%s[%d]):m_getMemoryCb(%s) fail", __FUNCTION__, __LINE__, filePath);
                goto done;
            }

            memcpy(jpegCallbackHeap->data, filePath, sizeof(filePath));
        }
    } else
#endif
    {
        CLOGD("DEBUG(%s[%d]):general callback : size (%d)", __FUNCTION__, __LINE__, jpegBuf.size[0]);

        jpegCallbackHeap = m_getMemoryCb(jpegBuf.fd[0], jpegBuf.size[0], 1, m_callbackCookie);
        if (!jpegCallbackHeap || jpegCallbackHeap->data == MAP_FAILED) {
            CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, jpegBuf.size[0]);
            goto done;
        }

        if (jpegBuf.fd[0] < 0)
            memcpy(jpegCallbackHeap->data, jpegBuf.addr[0], jpegBuf.size[0]);
    }

done:
    if (jpegCallbackHeap == NULL ||
        jpegCallbackHeap->data == MAP_FAILED) {

        if (jpegCallbackHeap) {
            jpegCallbackHeap->release(jpegCallbackHeap);
            jpegCallbackHeap = NULL;
        }

        m_notifyCb(CAMERA_MSG_ERROR, -1, 0, m_callbackCookie);
    }

    CLOGD("INFO(%s[%d]):making callback buffer done", __FUNCTION__, __LINE__);

    return jpegCallbackHeap;
}

bool ExynosCamera::m_postPictureThreadFunc(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    int ret = 0;
    int loop = false;
    int bufIndex = -2;
    int buffer_idx = getShotBufferIdex();
    int retryCountJPEG = 4;

    ExynosCameraFrame *newFrame = NULL;

    ExynosCameraBuffer gscReprocessingBuffer;
    ExynosCameraBuffer jpegReprocessingBuffer;

    gscReprocessingBuffer.index = -2;
    jpegReprocessingBuffer.index = -2;

    int pipeId_gsc = 0;
    int pipeId_jpeg = 0;

    int currentSeriesShotMode = 0;

    if (m_exynosCameraParameters->isReprocessing() == true) {
        if (needGSCForCapture(getCameraId()) == true) {
            pipeId_gsc = PIPE_GSC_REPROCESSING;
        } else {
            pipeId_gsc = (isOwnScc(getCameraId()) == true) ? PIPE_SCC_REPROCESSING : PIPE_ISP_REPROCESSING;
        }
        pipeId_jpeg = PIPE_JPEG_REPROCESSING;
    } else {
        if (needGSCForCapture(getCameraId()) == true) {
            pipeId_gsc = PIPE_GSC_PICTURE;
        } else {
            if (isOwnScc(getCameraId()) == true) {
                pipeId_gsc = PIPE_SCC;
            } else {
                pipeId_gsc = PIPE_ISPC;
            }
        }

        pipeId_jpeg = PIPE_JPEG;
    }

    ExynosCameraBufferManager *bufferMgr = NULL;
    ret = m_getBufferManager(pipeId_gsc, &bufferMgr, DST_BUFFER_DIRECTION);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getBufferManager(SRC) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_gsc, ret);
        return ret;
    }

    CLOGI("INFO(%s[%d]):wait postPictureQ output", __FUNCTION__, __LINE__);
    ret = m_postPictureQ->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        CLOGW("WARN(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        goto CLEAN;
    }
    if (newFrame == NULL) {
        CLOGE("ERR(%s[%d]):newFrame is NULL", __FUNCTION__, __LINE__);
        goto CLEAN;
    }

    if (m_jpegCounter.getCount() <= 0) {
        CLOGD("DEBUG(%s[%d]): Picture canceled", __FUNCTION__, __LINE__);
        goto CLEAN;
    }

    CLOGI("INFO(%s[%d]):postPictureQ output done", __FUNCTION__, __LINE__);

#ifdef SAMSUNG_TN_FEATURE
    /* Save the iso of capture frame to camera parameters for providing App/framework iso info. */
    struct camera2_udm *udm;
    udm = new struct camera2_udm;
    memset(udm, 0x00, sizeof(struct camera2_udm));

    newFrame->getUserDynamicMeta(udm);
#ifdef ONE_SECOND_BURST_CAPTURE
    if (m_exynosCameraParameters->getSeriesShotMode() != SERIES_SHOT_MODE_ONE_SECOND_BURST)
#endif
    {
        m_exynosCameraParameters->setParamIso(udm);
    }

#ifdef LLS_REPROCESSING
    if(m_exynosCameraParameters->getSamsungCamera() && m_exynosCameraParameters->getSeriesShotMode() == SERIES_SHOT_MODE_LLS) {
        if (m_exynosCameraParameters->msgTypeEnabled(MULTI_FRAME_SHOT_BV_INFO)) {
            CLOGD("DEBUG(%s[%d]): MULTI_FRAME_SHOT_BV_INFO BV value %u", __FUNCTION__, __LINE__, udm->awb.vendorSpecific[11]);
            m_notifyCb(MULTI_FRAME_SHOT_BV_INFO, udm->awb.vendorSpecific[11], 0, m_callbackCookie);
        }
    }
#endif /* LLS_REPROCESSING */

    if (udm != NULL) {
        delete udm;
        udm = NULL;
    }
#endif

    /* put picture callback buffer */
    /* get gsc dst buffers */
    ret = newFrame->getDstBuffer(pipeId_gsc, &gscReprocessingBuffer);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_gsc, ret);
        goto CLEAN;
    }

    /* callback */
    if (m_hdrEnabled == false && m_exynosCameraParameters->getSeriesShotCount() <= 0
#ifdef SAMSUNG_DNG
        && !m_exynosCameraParameters->getDNGCaptureModeOn()
#endif
        ) {
        if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_RAW_IMAGE)) {
            CLOGD("DEBUG(%s[%d]): RAW callback", __FUNCTION__, __LINE__);
            camera_memory_t *rawCallbackHeap = NULL;
            rawCallbackHeap = m_getMemoryCb(gscReprocessingBuffer.fd[0], gscReprocessingBuffer.size[0], 1, m_callbackCookie);
            if (!rawCallbackHeap || rawCallbackHeap->data == MAP_FAILED) {
                CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, gscReprocessingBuffer.size[0]);
                goto CLEAN;
            }

            setBit(&m_callbackState, CALLBACK_STATE_RAW_IMAGE, true);
            m_dataCb(CAMERA_MSG_RAW_IMAGE, rawCallbackHeap, 0, NULL, m_callbackCookie);
            clearBit(&m_callbackState, CALLBACK_STATE_RAW_IMAGE, true);
            rawCallbackHeap->release(rawCallbackHeap);
        }

        if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_RAW_IMAGE_NOTIFY)) {
            CLOGD("DEBUG(%s[%d]): RAW_IMAGE_NOTIFY callback", __FUNCTION__, __LINE__);

            m_notifyCb(CAMERA_MSG_RAW_IMAGE_NOTIFY, 0, 0, m_callbackCookie);
        }

        if ((m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_POSTVIEW_FRAME))
            && !m_exynosCameraParameters->getIsThumbnailCallbackOn()
#ifdef SAMSUNG_MAGICSHOT
            && (m_exynosCameraParameters->getShotMode() != SHOT_MODE_MAGIC)
#endif
           ) {
            CLOGD("DEBUG(%s[%d]): POSTVIEW callback", __FUNCTION__, __LINE__);

            camera_memory_t *postviewCallbackHeap = NULL;
            postviewCallbackHeap = m_getMemoryCb(gscReprocessingBuffer.fd[0], gscReprocessingBuffer.size[0], 1, m_callbackCookie);
            if (!postviewCallbackHeap || postviewCallbackHeap->data == MAP_FAILED) {
                CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, gscReprocessingBuffer.size[0]);
                goto CLEAN;
            }

            setBit(&m_callbackState, CALLBACK_STATE_POSTVIEW_FRAME, true);
            m_dataCb(CAMERA_MSG_POSTVIEW_FRAME, postviewCallbackHeap, 0, NULL, m_callbackCookie);
            clearBit(&m_callbackState, CALLBACK_STATE_POSTVIEW_FRAME, true);
            postviewCallbackHeap->release(postviewCallbackHeap);
        }
    }

#if 0 // SAMSUNG_MAGICSHOT
    if ((m_exynosCameraParameters->getShotMode() == SHOT_MODE_MAGIC) &&
        (m_exynosCameraParameters->isReprocessing() == true)) {
        CLOGD("magic shot rear postview callback star(%s)(%d)", __FUNCTION__, __LINE__);
        int pipeId_postPictureGsc = 0;
        ExynosCameraBuffer postgscReprocessingBuffer;
        ExynosCameraBufferManager *bufferMgr1 = NULL;
        struct camera2_stream *shot_stream = NULL;
        ExynosRect srcRect, dstRect;
        int previewW = 0, previewH = 0, previewFormat = 0;
        int pictureW = 0, pictureH = 0, pictureFormat = 0;
        float zoomRatio = m_exynosCameraParameters->getZoomRatio(0) / 1000;

        postgscReprocessingBuffer.index = -2;
        pipeId_postPictureGsc = PIPE_GSC_REPROCESSING;

        shot_stream = (struct camera2_stream *)(gscReprocessingBuffer.addr[buffer_idx]);
        if (shot_stream == NULL) {
            CLOGE("ERR(%s[%d]):shot_stream is NULL. buffer(%d)",
                    __FUNCTION__, __LINE__, gscReprocessingBuffer.index);
            goto CLEAN;
        }

        int retry = 0;
        ret = m_getBufferManager(pipeId_postPictureGsc, &bufferMgr1, DST_BUFFER_DIRECTION);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getBufferManager(GSC) fail, pipeId(%d), ret(%d)",
                __FUNCTION__, __LINE__, pipeId_postPictureGsc, ret);
            return ret;
        }

        do {
            ret = -1;
            retry++;
            if (bufferMgr1->getNumOfAvailableBuffer() > 0) {
                ret = m_setupEntity(pipeId_postPictureGsc, newFrame, &gscReprocessingBuffer, NULL);
            } else {
                /* wait available SCC buffer */
                usleep(WAITING_TIME);
            }

            if (retry % 10 == 0)
                CLOGW("WRAN(%s[%d]):retry setupEntity for GSC", __FUNCTION__, __LINE__);
        } while(ret < 0 && retry < (TOTAL_WAITING_TIME/WAITING_TIME));

        if (ret < 0) {
            CLOGE("ERR(%s[%d]):setupEntity fail, pipeId(%d), retry(%d), ret(%d)",
                __FUNCTION__, __LINE__, pipeId_postPictureGsc, retry, ret);
            goto CLEAN;
        }

        m_exynosCameraParameters->getPreviewSize(&previewW, &previewH);
        m_exynosCameraParameters->getPictureSize(&pictureW, &pictureH);
        pictureFormat = m_exynosCameraParameters->getPictureFormat();
        previewFormat = m_exynosCameraParameters->getPreviewFormat();

        CLOGD("DEBUG(%s):size preview(%d, %d,format:%d)picture(%d, %d,format:%d)", __FUNCTION__,
            previewW, previewH,previewFormat,pictureW, pictureH,pictureFormat);

        srcRect.x = shot_stream->output_crop_region[0];
        srcRect.y = shot_stream->output_crop_region[1];
        srcRect.w = shot_stream->output_crop_region[2];
        srcRect.h = shot_stream->output_crop_region[3];
        srcRect.fullW = shot_stream->output_crop_region[2];
        srcRect.fullH = shot_stream->output_crop_region[3];
        srcRect.colorFormat = SCC_OUTPUT_COLOR_FMT;
        // srcRect.colorFormat = pictureFormat;
        dstRect.x = 0;
        dstRect.y = 0;
        dstRect.w = previewW;
        dstRect.h = previewH;
        dstRect.fullW = previewW;
        dstRect.fullH = previewH;
        dstRect.colorFormat = previewFormat;

        ret = getCropRectAlign(srcRect.w,  srcRect.h,
                previewW,   previewH,
                &srcRect.x, &srcRect.y,
                &srcRect.w, &srcRect.h,
                2, 2, 0, zoomRatio);

        ret = newFrame->setSrcRect(pipeId_postPictureGsc, &srcRect);
        ret = newFrame->setDstRect(pipeId_postPictureGsc, &dstRect);

        CLOGD("DEBUG(%s):size (%d, %d, %d, %d %d %d)", __FUNCTION__,
            srcRect.x, srcRect.y, srcRect.w, srcRect.h, srcRect.fullW, srcRect.fullH);
        CLOGD("DEBUG(%s):size (%d, %d, %d, %d %d %d)", __FUNCTION__,
            dstRect.x, dstRect.y, dstRect.w, dstRect.h, dstRect.fullW, dstRect.fullH);

        /* push frame to GSC pipe */
        m_pictureFrameFactory->setOutputFrameQToPipe(dstPostPictureGscQ, pipeId_postPictureGsc);
        m_pictureFrameFactory->pushFrameToPipe(&newFrame, pipeId_postPictureGsc);

        ret = m_postPictureGscThread->run();
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):couldn't run m_postPictureGscThread, ret(%d)", __FUNCTION__, __LINE__, ret);
            return INVALID_OPERATION;
        }
    }
#endif

    currentSeriesShotMode = m_exynosCameraParameters->getSeriesShotMode();

#ifdef SAMSUNG_DEBLUR
    if (m_exynosCameraParameters->getLDCaptureMode() == MULTI_SHOT_MODE_DEBLUR
        && !getUseDeblurCaptureOn()) {
        m_deblurCaptureCount = 0;
        m_deblur_firstFrame = NULL;
        m_deblur_secondFrame = NULL;
        setUseDeblurCaptureOn(false);
        m_exynosCameraParameters->setLDCaptureMode(MULTI_SHOT_MODE_NONE);
        m_exynosCameraParameters->setLDCaptureCount(0);
        m_exynosCameraParameters->setLDCaptureLLSValue(LLS_LEVEL_ZSL);
        CLOGI("INFO(%s[%d]): deblur capture clear!", __FUNCTION__, __LINE__);
    }
#endif

    /* Make compressed image */
    if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_COMPRESSED_IMAGE) ||
        m_exynosCameraParameters->getSeriesShotCount() > 0 ||
        m_hdrEnabled == true) {

        /* HDR callback */
        if (m_hdrEnabled == true ||
                currentSeriesShotMode == SERIES_SHOT_MODE_LLS ||
                currentSeriesShotMode == SERIES_SHOT_MODE_SIS ||
                m_exynosCameraParameters->getShotMode() == SHOT_MODE_FRONT_PANORAMA) {
#ifdef SR_CAPTURE
            if(m_exynosCameraParameters->getSROn()) {
                m_doSRCallbackFunc(newFrame);
            }
#endif
            CLOGD("DEBUG(%s[%d]): HDR callback", __FUNCTION__, __LINE__);

            /* send yuv image with jpeg callback */
            camera_memory_t    *jpegCallbackHeap = NULL;
            jpegCallbackHeap = m_getMemoryCb(gscReprocessingBuffer.fd[0], gscReprocessingBuffer.size[0], 1, m_callbackCookie);
            if (!jpegCallbackHeap || jpegCallbackHeap->data == MAP_FAILED) {
                CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, gscReprocessingBuffer.size[0]);
                goto CLEAN;
            }

            setBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE, true);
            m_dataCb(CAMERA_MSG_COMPRESSED_IMAGE, jpegCallbackHeap, 0, NULL, m_callbackCookie);
            clearBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE, true);

            jpegCallbackHeap->release(jpegCallbackHeap);

            /* put GSC buffer */
            ret = m_putBuffers(bufferMgr, gscReprocessingBuffer.index);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):bufferMgr->putBuffers() fail, pipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, pipeId_gsc, ret);
                goto CLEAN;
            }

            m_jpegCounter.decCount();
        } else {
            int retry = 0;

            /* 1. get wait available JPEG src buffer */
            do {
                bufIndex = -2;
                retry++;

                if (m_pictureEnabled == false) {
                    CLOGI("INFO(%s[%d]):m_pictureEnable is false", __FUNCTION__, __LINE__);
                    goto CLEAN;
                }
                if (m_jpegBufferMgr->getNumOfAvailableBuffer() > 0)
                    m_jpegBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &jpegReprocessingBuffer);

                if (bufIndex < 0) {
                    usleep(WAITING_TIME);

                    if (retry % 20 == 0) {
                        CLOGW("WRN(%s[%d]):retry JPEG getBuffer(%d) postPictureQ(%d), saveQ0(%d), saveQ1(%d), saveQ2(%d)",
                                __FUNCTION__, __LINE__, bufIndex,
                                m_postPictureQ->getSizeOfProcessQ(),
                                m_jpegSaveQ[JPEG_SAVE_THREAD0]->getSizeOfProcessQ(),
                                m_jpegSaveQ[JPEG_SAVE_THREAD1]->getSizeOfProcessQ(),
                                m_jpegSaveQ[JPEG_SAVE_THREAD2]->getSizeOfProcessQ());
                        m_jpegBufferMgr->dump();
                    }
                }
                /* this will retry until 300msec */
            } while (bufIndex < 0 && retry < (TOTAL_WAITING_TIME / WAITING_TIME) && m_stopBurstShot == false);

            if (bufIndex < 0) {
                if (retry >= (TOTAL_WAITING_TIME / WAITING_TIME)) {
                    CLOGE("ERR(%s[%d]):getBuffer totally fail, retry(%d), m_stopBurstShot(%d)",
                            __FUNCTION__, __LINE__, retry, m_stopBurstShot);
		    /* HACK for debugging P150108-08143 */
		    bufferMgr->printBufferState();
		    android_printAssert(NULL, LOG_TAG, "BURST_SHOT_TIME_ASSERT(%s[%d]): unexpected error, get jpeg buffer failed, assert!!!!", __FUNCTION__, __LINE__);
                } else {
                    CLOGD("DEBUG(%s[%d]):getBuffer stopped, retry(%d), m_stopBurstShot(%d)",
                            __FUNCTION__, __LINE__, retry, m_stopBurstShot);
                }
                ret = m_putBuffers(bufferMgr, gscReprocessingBuffer.index);
                goto CLEAN;
            }

            /* 2. setup Frame Entity */
            ret = m_setupEntity(pipeId_jpeg, newFrame, &gscReprocessingBuffer, &jpegReprocessingBuffer);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):setupEntity fail, pipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, pipeId_jpeg, ret);
                goto CLEAN;
            }

            /* 3. Q Set-up */
            m_pictureFrameFactory->setOutputFrameQToPipe(dstJpegReprocessingQ, pipeId_jpeg);

#ifdef SAMSUNG_BD
            if(m_exynosCameraParameters->getSeriesShotMode() == SERIES_SHOT_MODE_BURST) {
                UTstr bdInfo = {NULL, 0};
                ret = m_BDbufferQ->waitAndPopProcessQ(&bdInfo);
                if(ret == TIMED_OUT || ret < 0) {
                    CLOGE("[BD](%s)(%d):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
                }
                else {
                    CLOGD("[BD](%s[%d]): Pop the buffer(size: %d)", __FUNCTION__, __LINE__, bdInfo.size);
                    m_exynosCameraParameters->setBlurInfo(bdInfo.data, bdInfo.size);
                }
            }
#endif

            /* 4. push the newFrame to pipe */
            m_pictureFrameFactory->pushFrameToPipe(&newFrame, pipeId_jpeg);

            /* 5. wait outputQ */
            CLOGI("INFO(%s[%d]):wait Jpeg output", __FUNCTION__, __LINE__);
            while (retryCountJPEG > 0) {
                ret = dstJpegReprocessingQ->waitAndPopProcessQ(&newFrame);
                if (ret == TIMED_OUT) {
                    CLOGW("WRN(%s)(%d):wait and pop timeout, ret(%d)", __FUNCTION__, __LINE__, ret);
                    m_pictureFrameFactory->startThread(pipeId_jpeg);
                } else if (ret < 0) {
                    CLOGE("ERR(%s)(%d):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
                    /* TODO: doing exception handling */
                    goto CLEAN;
                } else {
                    break;
                }
                retryCountJPEG--;
            }

            if (newFrame == NULL) {
                CLOGE("ERR(%s[%d]):newFrame is NULL", __FUNCTION__, __LINE__);
                goto CLEAN;
            }
            /*
             * When Non-reprocessing scenario does not setEntityState,
             * because Non-reprocessing scenario share preview and capture frames
             */
            if (m_exynosCameraParameters->isReprocessing() == true) {
                ret = newFrame->setEntityState(pipeId_jpeg, ENTITY_STATE_COMPLETE);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):setEntityState(ENTITY_STATE_COMPLETE) fail, pipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, pipeId_jpeg, ret);
                    return ret;
                }
            }

#if defined(SAMSUNG_DEBLUR) || defined(SAMSUNG_LLS_DEBLUR)
            if (m_exynosCameraParameters->getLDCaptureMode()
                && m_exynosCameraParameters->getIsThumbnailCallbackOn()) {
                if (m_postPictureGscThread->isRunning()) {
                    m_postPictureGscThread->join();
                    CLOGD("DEBUG(%s[%d]): m_postPictureGscThread join", __FUNCTION__, __LINE__);
                }
            }
#endif

            /* put GSC buffer */
            ret = m_putBuffers(bufferMgr, gscReprocessingBuffer.index);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):bufferMgr->putBuffers() fail, pipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, pipeId_gsc, ret);
                goto CLEAN;
            }

            int jpegOutputSize = newFrame->getJpegSize();
            CLOGI("INFO(%s[%d]):Jpeg output done, jpeg size(%d)", __FUNCTION__, __LINE__, jpegOutputSize);

            if (jpegOutputSize <= 0) {
                CLOGW("WRN(%s[%d]): jpegOutput size(%d) is invalid", __FUNCTION__, __LINE__, jpegOutputSize);
                jpegOutputSize = jpegReprocessingBuffer.size[0];
            }

            jpegReprocessingBuffer.size[0] = jpegOutputSize;

#if 0 // SAMSUNG_MAGICSHOT
            if ((m_exynosCameraParameters->getShotMode() == SHOT_MODE_MAGIC) &&
                (m_exynosCameraParameters->isReprocessing() == true)) {
                m_postPictureGscThread->join();
            }
#endif

            /* push postProcess to call CAMERA_MSG_COMPRESSED_IMAGE */
            jpeg_callback_buffer_t jpegCallbackBuf;
            jpegCallbackBuf.buffer = jpegReprocessingBuffer;
#ifdef BURST_CAPTURE
            m_burstCaptureCallbackCount++;
            CLOGI("INFO(%s[%d]): burstCaptureCallbackCount(%d)", __FUNCTION__, __LINE__, m_burstCaptureCallbackCount);
#endif
retry:
            if ((m_exynosCameraParameters->getSeriesShotCount() > 0)
#ifdef SAMSUNG_MAGICSHOT
                && !((m_exynosCameraParameters->getShotMode() == SHOT_MODE_MAGIC) && (m_burstCaptureCallbackCount > m_magicshotMaxCount))
#endif
#ifdef ONE_SECOND_BURST_CAPTURE
                && (m_exynosCameraParameters->getSeriesShotMode() != SERIES_SHOT_MODE_ONE_SECOND_BURST)
#endif
                ) {
                int threadNum = 0;

                if (m_burst[JPEG_SAVE_THREAD0] == false && m_jpegSaveThread[JPEG_SAVE_THREAD0]->isRunning() == false) {
                    threadNum = JPEG_SAVE_THREAD0;
                } else if (m_burst[JPEG_SAVE_THREAD1] == false && m_jpegSaveThread[JPEG_SAVE_THREAD1]->isRunning() == false) {
                    threadNum = JPEG_SAVE_THREAD1;
                } else if (m_burst[JPEG_SAVE_THREAD2] == false && m_jpegSaveThread[JPEG_SAVE_THREAD2]->isRunning() == false) {
                    threadNum = JPEG_SAVE_THREAD2;
                } else {
                    CLOGW("WARN(%s[%d]): wait for available save thread, thread running(%d, %d, %d,)",
                            __FUNCTION__, __LINE__,
                            m_jpegSaveThread[JPEG_SAVE_THREAD0]->isRunning(),
                            m_jpegSaveThread[JPEG_SAVE_THREAD1]->isRunning(),
                            m_jpegSaveThread[JPEG_SAVE_THREAD2]->isRunning());
                    usleep(WAITING_TIME * 10);
                    goto retry;
                }

                m_burst[threadNum] = true;
                ret = m_jpegSaveThread[threadNum]->run();
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]): m_jpegSaveThread%d run fail, ret(%d)", __FUNCTION__, __LINE__, threadNum, ret);
                    m_burst[threadNum] = false;
                    m_running[threadNum] = false;
                    goto retry;
                }

                jpegCallbackBuf.callbackNumber = m_burstCaptureCallbackCount;
                m_jpegSaveQ[threadNum]->pushProcessQ(&jpegCallbackBuf);
            } else {
                jpegCallbackBuf.callbackNumber = 0;
                m_jpegCallbackQ->pushProcessQ(&jpegCallbackBuf);
            }

            m_jpegCounter.decCount();
        }
    } else {
        CLOGD("DEBUG(%s[%d]): Disabled compressed image", __FUNCTION__, __LINE__);

#if defined(SAMSUNG_DEBLUR) || defined(SAMSUNG_LLS_DEBLUR)
        if (m_exynosCameraParameters->getLDCaptureMode() && m_exynosCameraParameters->getIsThumbnailCallbackOn()) {
            if (m_postPictureGscThread->isRunning()) {
                m_postPictureGscThread->join();
                CLOGD("DEBUG(%s[%d]): m_postPictureGscThread join", __FUNCTION__, __LINE__);
            }
        }
#endif

        /* put GSC buffer */
        ret = m_putBuffers(bufferMgr, gscReprocessingBuffer.index);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):bufferMgr->putBuffers() fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipeId_gsc, ret);
            goto CLEAN;
        }

        m_jpegCounter.decCount();
    }

    if (newFrame != NULL) {
        newFrame->printEntity();
        newFrame->frameUnlock();
        ret = m_removeFrameFromList(&m_postProcessList, newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):remove frame from processList fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }

#ifdef LLS_REPROCESSING
        if (m_captureSelector->getIsLastFrame() == true)
#endif
        {
            CLOGD("DEBUG(%s[%d]): Picture frame delete(%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());
            newFrame->decRef();
            m_frameMgr->deleteFrame(newFrame);
            newFrame = NULL;
        }
    }

    CLOGI("INFO(%s[%d]):postPicture thread complete, remaining count(%d)", __FUNCTION__, __LINE__, m_jpegCounter.getCount());

    if (m_jpegCounter.getCount() <= 0) {
        if (m_hdrEnabled == true) {
            CLOGI("INFO(%s[%d]): End of HDR capture!", __FUNCTION__, __LINE__);
            m_hdrEnabled = false;
            m_pictureEnabled = false;
        }
        if (currentSeriesShotMode == SERIES_SHOT_MODE_LLS ||
            currentSeriesShotMode == SERIES_SHOT_MODE_SIS) {
            CLOGI("INFO(%s[%d]): End of LLS/SIS capture!", __FUNCTION__, __LINE__);
            m_pictureEnabled = false;

#ifdef SAMSUNG_DNG
            if (m_exynosCameraParameters->getDNGCaptureModeOn()) {
                int waitCount = 5;
                while (m_previewFrameFactory->getRunningFrameCount(PIPE_FLITE) != 0 && 0 < waitCount) {
                    CLOGD("DNG(%s[%d]): wait flite dqbuffer count(%d)",
                        __FUNCTION__, __LINE__, m_previewFrameFactory->getRunningFrameCount(PIPE_FLITE));
                    usleep(30000);
                    waitCount--;
                }

                if (m_exynosCameraParameters->getCheckMultiFrame()) {
                    CLOGE("[DNG](%s[%d]):PIPE_FLITE stop", __FUNCTION__, __LINE__);
                    m_exynosCameraParameters->setUseDNGCapture(false);
                    m_exynosCameraParameters->setCheckMultiFrame(false);
                    m_previewFrameFactory->stopThread(PIPE_FLITE);
                }

                CLOGD("DNG(%s[%d]): reset m_fliteBuffer buffers", __FUNCTION__, __LINE__);
                m_fliteBufferMgr->resetBuffers();
            }
#endif
        }

        if(m_exynosCameraParameters->getShotMode() == SHOT_MODE_FRONT_PANORAMA) {
            CLOGI("INFO(%s[%d]): End of wideselfie capture!", __FUNCTION__, __LINE__);
            m_pictureEnabled = false;
        }

        CLOGD("DEBUG(%s[%d]): free gsc buffers", __FUNCTION__, __LINE__);
        m_gscBufferMgr->resetBuffers();

#ifdef ONE_SECOND_BURST_CAPTURE
        if (!(currentSeriesShotMode == SERIES_SHOT_MODE_ONE_SECOND_BURST ||
            currentSeriesShotMode == SERIES_SHOT_MODE_BURST))
#else
        if (currentSeriesShotMode != SERIES_SHOT_MODE_BURST)
#endif
        {
            CLOGD("DEBUG(%s[%d]): clearList postProcessList, series shot mode(%d)", __FUNCTION__, __LINE__, currentSeriesShotMode);
            if (m_clearList(&m_postProcessList) < 0) {
                CLOGE("ERR(%s):m_clearList fail", __FUNCTION__);
            }
        }
    }

    if (m_exynosCameraParameters->getScalableSensorMode()) {
        m_scalableSensorMgr.setMode(EXYNOS_CAMERA_SCALABLE_CHANGING);
        ret = m_restartPreviewInternal();
        if (ret < 0)
            CLOGE("(%s[%d]): restart preview internal fail", __FUNCTION__, __LINE__);
        m_scalableSensorMgr.setMode(EXYNOS_CAMERA_SCALABLE_NONE);
    }

CLEAN:
    if (newFrame != NULL) {
        CLOGD("DEBUG(%s[%d]): Picture frame delete(%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());
        newFrame->decRef();
        m_frameMgr->deleteFrame(newFrame);
        newFrame = NULL;
    }

    /* HACK: Sometimes, m_postPictureThread is finished without waiting the last picture */
    int waitCount = 5;
    while (m_postPictureQ->getSizeOfProcessQ() == 0 && 0 < waitCount) {
        usleep(10000);
        waitCount--;
    }

    if (m_postPictureQ->getSizeOfProcessQ() > 0 ||
            currentSeriesShotMode != SERIES_SHOT_MODE_NONE) {
        CLOGD("DEBUG(%s[%d]):postPicture thread will run again.  currentSeriesShotMode(%d), postPictureQ size(%d)",
            __func__, __LINE__, currentSeriesShotMode, m_postPictureQ->getSizeOfProcessQ());
        loop = true;
    }

    return loop;
}

bool ExynosCamera::m_jpegSaveThreadFunc(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    CLOGI("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    int ret = 0;
    int loop = false;
    int curThreadNum = -1;
    char burstFilePath[CAMERA_FILE_PATH_SIZE];
#ifdef BURST_CAPTURE
    int fd = -1;
#endif

    jpeg_callback_buffer_t jpegCallbackBuf;
    ExynosCameraBuffer jpegSaveBuffer;
    int seriesShotNumber = -1;
//    camera_memory_t *jpegCallbackHeap = NULL;

    for (int threadNum = JPEG_SAVE_THREAD0; threadNum < JPEG_SAVE_THREAD_MAX_COUNT; threadNum++) {
        if (m_burst[threadNum] == true && m_running[threadNum] == false) {
            m_running[threadNum] = true;
            curThreadNum = threadNum;
            if (m_jpegSaveQ[curThreadNum]->waitAndPopProcessQ(&jpegCallbackBuf) < 0) {
                CLOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
                goto done;
            }
            break;
        }
    }

    if (curThreadNum < 0 || curThreadNum > JPEG_SAVE_THREAD2) {
        CLOGE("ERR(%s[%d]): invalid thrad num (%d)", __FUNCTION__, __LINE__, curThreadNum);
        goto done;
    }

    jpegSaveBuffer = jpegCallbackBuf.buffer;
    seriesShotNumber = jpegCallbackBuf.callbackNumber;

#ifdef BURST_CAPTURE
    if (m_exynosCameraParameters->getSeriesShotCount() > 0) {

        int seriesShotSaveLocation = m_exynosCameraParameters->getSeriesShotSaveLocation();

        if (seriesShotSaveLocation == BURST_SAVE_CALLBACK) {
            jpegCallbackBuf.buffer = jpegSaveBuffer;
            jpegCallbackBuf.callbackNumber = 0;
            m_jpegCallbackQ->pushProcessQ(&jpegCallbackBuf);
            goto done;
        } else {
            memset(burstFilePath, 0, sizeof(burstFilePath));

            m_burstCaptureCallbackCountLock.lock();
#if defined(SAMSUNG_INF_BURST)
            {
                time_t rawtime;
                struct tm *timeinfo;

                time(&rawtime);
                timeinfo = localtime(&rawtime);
                if(seriesShotNumber == 1) {
                    strftime(m_burstTime, 20, "%Y%m%d_%H%M%S", timeinfo);
                }
                else {
                /* We don't do anything in this time */
                }

                snprintf(burstFilePath, sizeof(burstFilePath), "%s%s_%03d.jpg",
                        m_burstSavePath, m_burstTime, seriesShotNumber);
            }
#else
            snprintf(burstFilePath, sizeof(burstFilePath), "%sBurst%02d.jpg", m_burstSavePath, seriesShotNumber);
#endif
            m_burstCaptureCallbackCountLock.unlock();

            CLOGD("DEBUG(%s[%d]):%s fd:%d jpegSize : %d", __FUNCTION__, __LINE__, burstFilePath, jpegSaveBuffer.fd[0], jpegSaveBuffer.size[0]);

            m_burstCaptureSaveLock.lock();

            m_burstSaveTimer.start();

            if(m_FileSaveFunc(burstFilePath, &jpegSaveBuffer) == false) {
                m_burstCaptureSaveLock.unlock();
                CLOGE("ERR(%s[%d]): FILE save ERROR", __FUNCTION__, __LINE__);
                goto done;
            }

            m_burstCaptureSaveLock.unlock();

            m_burstSaveTimer.stop();
            m_burstSaveTimerTime = m_burstSaveTimer.durationUsecs();
            if (m_burstSaveTimerTime > (m_burstDuration - 33000)) {
                m_burstDuration += (int)((m_burstSaveTimerTime - m_burstDuration + 33000) / 33000) * 33000;
                CLOGD("Increase burst duration = %d", m_burstDuration);
            }

            CLOGD("DEBUG(%s[%d]):m_burstSaveTimerTime : %d msec, path(%s)", __FUNCTION__, __LINE__, (int)m_burstSaveTimerTime / 1000, burstFilePath);
        }
        jpegCallbackBuf.buffer = jpegSaveBuffer;
        jpegCallbackBuf.callbackNumber = seriesShotNumber;
        m_jpegCallbackQ->pushProcessQ(&jpegCallbackBuf);
    } else
#endif
    {
        jpegCallbackBuf.buffer = jpegSaveBuffer;
        jpegCallbackBuf.callbackNumber = 0;
        m_jpegCallbackQ->pushProcessQ(&jpegCallbackBuf);
    }

done:
/*
    if (jpegCallbackHeap == NULL ||
        jpegCallbackHeap->data == MAP_FAILED) {

        if (jpegCallbackHeap) {
            jpegCallbackHeap->release(jpegCallbackHeap);
            jpegCallbackHeap = NULL;
        }

        m_notifyCb(CAMERA_MSG_ERROR, -1, 0, m_callbackCookie);
    }
*/
    if (JPEG_SAVE_THREAD0 <= curThreadNum && curThreadNum < JPEG_SAVE_THREAD_MAX_COUNT) {
        m_burst[curThreadNum] = false;
        m_running[curThreadNum] = false;
    }

    CLOGI("INFO(%s[%d]):saving jpeg buffer done", __FUNCTION__, __LINE__);

    /* one shot */
    return false;
}

bool ExynosCamera::m_jpegCallbackThreadFunc(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    CLOGI("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    int ret = 0;
    int retry = 0, maxRetry = 0;
    int loop = false;
    int seriesShotNumber = -1;
    int currentSeriesShotMode = 0;

    jpeg_callback_buffer_t jpegCallbackBuf;
    ExynosCameraBuffer jpegCallbackBuffer;
    ExynosCameraBuffer postviewCallbackBuffer;
    camera_memory_t *jpegCallbackHeap = NULL;
    camera_memory_t *postviewCallbackHeap = NULL;
    ExynosCameraBufferManager *postviewBufferMgr = NULL;

    jpegCallbackBuffer.index = -2;
    jpegCallbackBuf.callbackNumber = -1;

    postviewCallbackBuffer.index = -2;

    ExynosCameraActivityFlash *m_flashMgr = m_exynosCameraActivityControl->getFlashMgr();
    currentSeriesShotMode = m_exynosCameraParameters->getSeriesShotMode();

    if (m_flashMgr->getNeedFlash() == true) {
        maxRetry = TOTAL_FLASH_WATING_COUNT;
#ifdef ONE_SECOND_BURST_CAPTURE
    } else if (currentSeriesShotMode == SERIES_SHOT_MODE_ONE_SECOND_BURST) {
        maxRetry = ONE_SECOND_BURST_CAPTURE_MAX_RETRY;
#endif
    } else {
        maxRetry = TOTAL_WAITING_COUNT;
    }

#ifdef BURST_CAPTURE
    if (currentSeriesShotMode == SERIES_SHOT_MODE_BURST) {
        if (m_burstShutterLocation == BURST_SHUTTER_JPEGCB) {
            m_shutterCallbackThread->join();
            CLOGD("Burst Shutter callback start(%s)(%d)", __FUNCTION__, __LINE__);
            m_shutterCallbackThreadFunc();
            CLOGD("Burst Shutter callback end(%s)(%d)", __FUNCTION__, __LINE__);
        } else if (m_burstShutterLocation == BURST_SHUTTER_PREPICTURE_DONE) {
            m_burstShutterLocation = BURST_SHUTTER_JPEGCB;
        }
    }
#endif

    m_getBufferManager(PIPE_GSC_REPROCESSING3, &postviewBufferMgr, DST_BUFFER_DIRECTION);
    if ((m_exynosCameraParameters->getIsThumbnailCallbackOn()
#if defined(SAMSUNG_DEBLUR) || defined(SAMSUNG_LLS_DEBLUR)
        || m_exynosCameraParameters->getLDCaptureMode()
#endif
        )
#ifdef ONE_SECOND_BURST_CAPTURE
        && currentSeriesShotMode != SERIES_SHOT_MODE_ONE_SECOND_BURST
#endif
        && m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_POSTVIEW_FRAME)) {
        /* One second burst will get thumbnail RGB after get jpegcallbackheap */
        if (m_cancelPicture) {
            loop = false;
            goto CLEAN;
        }

        do {
            ret = m_postviewCallbackQ->waitAndPopProcessQ(&postviewCallbackBuffer);
            if (ret < 0) {
                retry++;
                CLOGW("WARN(%s[%d]):postviewCallbackQ pop fail, retry(%d)", __FUNCTION__, __LINE__, retry);
            }
        } while (ret < 0 && retry < maxRetry &&
                 m_flagThreadStop != true &&
                 m_stopBurstShot == false &&
                 !m_cancelPicture);

        postviewCallbackHeap = m_getMemoryCb(postviewCallbackBuffer.fd[0],
                                             postviewCallbackBuffer.size[0],
                                             1, m_callbackCookie);

        if (!postviewCallbackHeap || postviewCallbackHeap->data == MAP_FAILED) {
            CLOGE("ERR(%s[%d]):get postviewCallbackHeap(fd(%d), size(%d)) fail",
                __FUNCTION__, __LINE__, postviewCallbackBuffer.fd[0], postviewCallbackBuffer.size[0]);
            loop = true;
            goto CLEAN;
        }

        if (!m_cancelPicture) {
            CLOGD("thumbnail POSTVIEW callback start(%s)(%d)", __FUNCTION__, __LINE__);
            setBit(&m_callbackState, CALLBACK_STATE_POSTVIEW_FRAME, true);
            m_dataCb(CAMERA_MSG_POSTVIEW_FRAME, postviewCallbackHeap, 0, NULL, m_callbackCookie);
            clearBit(&m_callbackState, CALLBACK_STATE_POSTVIEW_FRAME, true);
            CLOGD("thumbnail POSTVIEW callback end(%s)(%d)", __FUNCTION__, __LINE__);
        }
        postviewCallbackHeap->release(postviewCallbackHeap);
    }

#ifdef ONE_SECOND_BURST_CAPTURE
    if (currentSeriesShotMode == SERIES_SHOT_MODE_ONE_SECOND_BURST) {
        retry = 0;
        /* For launch COMPRESSED m_dataCb() faster*/
        do {
            if (m_one_second_burst_capture && m_one_second_jpegCallbackHeap != NULL) {
                /* If there's saved jpegCallbackHeap */
                CLOGD("DEBUG(%s[%d]):ONE SECOND CAPTURE coming.", __FUNCTION__, __LINE__);
		        ret = 0;
                break;
            }
            ret = m_jpegCallbackQ->popProcessQ(&jpegCallbackBuf);
            if (ret < 0) {
                /* If there's no saved jpegCallbackHeap, ret value will smaller than 0
                   This will launch "goto CLEAN" */
                retry++;
                CLOGW("WARN(%s[%d]):jpegCallbackQ pop fail, retry(%d)", __FUNCTION__, __LINE__, retry);
                usleep(ONE_SECOND_BURST_CAPTURE_RETRY_DELAY);
            }
        } while(ret < 0 && retry < maxRetry && m_jpegCounter.getCount() > 0 && m_stopBurstShot == false);

        if (m_stopBurstShot)
            goto CLEAN;
    } else
#endif
    {
        if (m_cancelPicture) {
            loop = false;
            goto CLEAN;
        }

        do {
            ret = m_jpegCallbackQ->waitAndPopProcessQ(&jpegCallbackBuf);
            if (ret < 0) {
                retry++;
                CLOGW("WARN(%s[%d]):jpegCallbackQ pop fail, retry(%d)", __FUNCTION__, __LINE__, retry);
            }
        } while(ret < 0 && retry < maxRetry && m_jpegCounter.getCount() > 0 && !m_cancelPicture);
    }

#ifdef ONE_SECOND_BURST_CAPTURE
    if (currentSeriesShotMode == SERIES_SHOT_MODE_ONE_SECOND_BURST
        && m_exynosCameraParameters->getIsThumbnailCallbackOn()
        && jpegCallbackBuf.callbackNumber != -1) {
        do {
            ret = m_postviewCallbackQ->waitAndPopProcessQ(&postviewCallbackBuffer);
            if (ret < 0) {
                retry++;
                CLOGW("WARN(%s[%d]):postviewCallbackQ pop fail, retry(%d)", __FUNCTION__, __LINE__, retry);
            }
        } while (ret < 0 && retry < maxRetry && m_flagThreadStop != true && m_stopBurstShot == false);

        postviewCallbackHeap = m_getMemoryCb(postviewCallbackBuffer.fd[0], postviewCallbackBuffer.size[0], 1, m_callbackCookie);

        if (!postviewCallbackHeap || postviewCallbackHeap->data == MAP_FAILED) {
            CLOGE("ERR(%s[%d]): get postviewCallbackHeap(%d) fail", __FUNCTION__, __LINE__);
            loop = true;
            goto CLEAN;
        }
    }
#endif

    m_captureLock.lock();
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        loop = true;
        goto CLEAN;
    }

    if (jpegCallbackBuf.callbackNumber != -1) {
        jpegCallbackBuffer = jpegCallbackBuf.buffer;
        seriesShotNumber = jpegCallbackBuf.callbackNumber;
    }

    CLOGD("DEBUG(%s[%d]):jpeg calllback is start", __FUNCTION__, __LINE__);

    /* Make compressed image */
    if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_COMPRESSED_IMAGE) ||
        m_exynosCameraParameters->getSeriesShotCount() > 0) {
            if (jpegCallbackBuf.callbackNumber != -1) {
                jpegCallbackHeap = m_getJpegCallbackHeap(jpegCallbackBuffer, seriesShotNumber);
            }
#ifdef ONE_SECOND_BURST_CAPTURE
            if (currentSeriesShotMode == SERIES_SHOT_MODE_ONE_SECOND_BURST) {
                if (jpegCallbackHeap != NULL) {
                    if (m_one_second_jpegCallbackHeap != NULL) {
                        /* if last jpegCallbackHeap is remained */
                        m_one_second_jpegCallbackHeap->release(m_one_second_jpegCallbackHeap);
                    }
                    /* Save last jpegCallbackHeap */
                    m_one_second_jpegCallbackHeap = jpegCallbackHeap;
                } else {
                    /* Ready for COMPRESSED m_dataCb() */
                    jpegCallbackHeap = m_one_second_jpegCallbackHeap;
                }
                if (m_exynosCameraParameters->getIsThumbnailCallbackOn()) {
                    if (postviewCallbackHeap != NULL) {
                        if (m_one_second_postviewCallbackHeap != NULL) {
                            m_one_second_postviewCallbackHeap->release(m_one_second_postviewCallbackHeap);
                        }
                        m_one_second_postviewCallbackHeap = postviewCallbackHeap;
                    } else {
                        postviewCallbackHeap = m_one_second_postviewCallbackHeap;
                    }
                }
            }
#endif
            if (jpegCallbackHeap == NULL) {
                CLOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
                /* TODO: doing exception handling */
                android_printAssert(NULL, LOG_TAG, "Cannot recoverable, assert!!!!");
            }

#ifdef ONE_SECOND_BURST_CAPTURE
            if (currentSeriesShotMode != SERIES_SHOT_MODE_ONE_SECOND_BURST ||
                (currentSeriesShotMode == SERIES_SHOT_MODE_ONE_SECOND_BURST && m_one_second_burst_capture)) {
                if (currentSeriesShotMode == SERIES_SHOT_MODE_ONE_SECOND_BURST) {
                    m_one_second_burst_capture = false;
                    if (m_burstShutterLocation == BURST_SHUTTER_JPEGCB) {
                        m_shutterCallbackThread->join();
                        CLOGD("One Second Shutter callback start(%s)(%d)", __FUNCTION__, __LINE__);
                        m_shutterCallbackThreadFunc();
                        CLOGD("One Second Shutter callback end(%s)(%d)", __FUNCTION__, __LINE__);
                    } else if (m_burstShutterLocation == BURST_SHUTTER_PREPICTURE_DONE) {
                        m_burstShutterLocation = BURST_SHUTTER_JPEGCB;
                    }
                    if (m_exynosCameraParameters->getIsThumbnailCallbackOn()) {
                        if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_POSTVIEW_FRAME)) {
                            CLOGD("thumbnail POSTVIEW callback start(%s)(%d)", __FUNCTION__, __LINE__);
                            setBit(&m_callbackState, CALLBACK_STATE_POSTVIEW_FRAME, true);
                            m_dataCb(CAMERA_MSG_POSTVIEW_FRAME, postviewCallbackHeap, 0, NULL, m_callbackCookie);
                            clearBit(&m_callbackState, CALLBACK_STATE_POSTVIEW_FRAME, true);
                            CLOGD("thumbnail POSTVIEW callback end(%s)(%d)", __FUNCTION__, __LINE__);
                        }
                        if (m_one_second_postviewCallbackHeap != NULL) {
                            /* release saved buffer when COMPRESSED m_dataCb() called. */
                            m_one_second_postviewCallbackHeap->release(m_one_second_postviewCallbackHeap);
                            m_one_second_postviewCallbackHeap = NULL;
                        }
                    }
                    CLOGD("DEBUG(%s[%d]): before compressed - TimeCheck", __FUNCTION__, __LINE__);
                }
#endif
                if (!m_cancelPicture) {
                    setBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE, true);
                    m_dataCb(CAMERA_MSG_COMPRESSED_IMAGE, jpegCallbackHeap, 0, NULL, m_callbackCookie);
                    clearBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE, true);
                    CLOGD("DEBUG(%s[%d]): CAMERA_MSG_COMPRESSED_IMAGE callback (%d)",
                            __FUNCTION__, __LINE__, m_burstCaptureCallbackCount);
                }

#ifdef ONE_SECOND_BURST_CAPTURE
                if (currentSeriesShotMode == SERIES_SHOT_MODE_ONE_SECOND_BURST) {
                    CLOGD("DEBUG(%s[%d]): After compressed - TimeCheck", __FUNCTION__, __LINE__);
                    if (m_one_second_jpegCallbackHeap != NULL) {
                        /* release saved buffer when COMPRESSED m_dataCb() called. */
                        m_one_second_jpegCallbackHeap->release(m_one_second_jpegCallbackHeap);
                        m_one_second_jpegCallbackHeap = NULL;
                    }
                }
            }
#endif
            if (jpegCallbackBuf.callbackNumber != -1) {
                /* put JPEG callback buffer */
                if (m_jpegBufferMgr->putBuffer(jpegCallbackBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE) != NO_ERROR)
                    CLOGE("ERR(%s[%d]):putBuffer(%d) fail", __FUNCTION__, __LINE__, jpegCallbackBuffer.index);
            }

#ifdef ONE_SECOND_BURST_CAPTURE
            /* One second burst operation will release m_one_second_jpegCallbackHeap */
            if (currentSeriesShotMode != SERIES_SHOT_MODE_ONE_SECOND_BURST)
#endif
            jpegCallbackHeap->release(jpegCallbackHeap);
    } else {
        CLOGD("DEBUG(%s[%d]): Disabled compressed image", __FUNCTION__, __LINE__);
    }

CLEAN:
    CLOGI("INFO(%s[%d]):jpeg callback thread complete, remaining count(%d)", __FUNCTION__, __LINE__, m_takePictureCounter.getCount());

    if (postviewCallbackBuffer.index != -2) {
        CLOGD("DEBUG(%s[%d]): put dst(%d)", __FUNCTION__, __LINE__, postviewCallbackBuffer.index);
        m_putBuffers(postviewBufferMgr, postviewCallbackBuffer.index);
    }

#ifdef ONE_SECOND_BURST_CAPTURE
    if (currentSeriesShotMode == SERIES_SHOT_MODE_ONE_SECOND_BURST) {
        m_jpegCallbackCounter.decCount();
        CLOGD("DEBUG(%s[%d]):ONE SECOND BURST remaining count(%d)", __FUNCTION__, __LINE__, m_jpegCallbackCounter.getCount());
        if (m_jpegCallbackCounter.getCount() <= 1) {
            if (m_one_second_burst_capture) {
                loop = true;
                m_jpegCallbackCounter.setCount(2);
                m_reprocessingCounter.setCount(2);
                m_jpegCounter.setCount(2);
                m_pictureCounter.setCount(2);
                CLOGD("DEBUG(%s[%d]):Waiting once more...", __FUNCTION__, __LINE__);
            } else {
                loop = false;
                m_clearOneSecondBurst(true);
            }
        } else {
            loop = true;
        }
    }
#endif

    if (m_takePictureCounter.getCount() == 0) {
        m_pictureEnabled = false;
        loop = false;
#ifdef ONE_SECOND_BURST_CAPTURE
        /* m_clearJpegCallbackThread() already called in m_clearOneSecondBurst() */
        if (currentSeriesShotMode != SERIES_SHOT_MODE_ONE_SECOND_BURST)
#endif
        m_clearJpegCallbackThread(true);
        m_captureLock.unlock();
    } else {
        m_captureLock.unlock();
    }

    return loop;
}

void ExynosCamera::m_clearJpegCallbackThread(bool callFromJpeg)
{
    jpeg_callback_buffer_t jpegCallbackBuf;
    ExynosCameraBuffer postviewCallbackBuffer;
    ExynosCameraBuffer thumbnailCallbackBuffer;
    ExynosCameraBuffer jpegCallbackBuffer;
    ExynosCameraBufferManager *postviewBufferMgr = NULL;
    ExynosCameraBufferManager *thumbnailBufferMgr = NULL;

    CLOGI("INFO(%s[%d]): takePicture disabled, takePicture callback done takePictureCounter(%d)",
            __FUNCTION__, __LINE__, m_takePictureCounter.getCount());
    m_pictureEnabled = false;

    if (m_exynosCameraParameters->getUseDynamicScc() == true) {
        CLOGD("DEBUG(%s[%d]): Use dynamic bayer", __FUNCTION__, __LINE__);
        if (isOwnScc(getCameraId()) == true)
            m_previewFrameFactory->setRequestSCC(false);
        else
            m_previewFrameFactory->setRequestISPC(false);
    }

#ifdef SAMSUNG_DEBLUR
    if (m_exynosCameraParameters->getLDCaptureMode() == MULTI_SHOT_MODE_DEBLUR
        && getUseDeblurCaptureOn()) {
        m_deblurCaptureCount = 0;
        m_deblur_firstFrame = NULL;
        m_deblur_secondFrame = NULL;
        setUseDeblurCaptureOn(false);
        m_exynosCameraParameters->setLDCaptureMode(MULTI_SHOT_MODE_NONE);
        m_exynosCameraParameters->setLDCaptureCount(0);
        m_exynosCameraParameters->setLDCaptureLLSValue(LLS_LEVEL_ZSL);
        CLOGI("INFO(%s[%d]): deblur capture clear!", __FUNCTION__, __LINE__);
    }
#endif

#ifdef SAMSUNG_LLS_DEBLUR
    if(m_exynosCameraParameters->getLDCaptureMode()) {
        m_LDCaptureCount = 0;

        for (int i = 0; i < MAX_LD_CAPTURE_COUNT; i++) {
            m_LDBufIndex[i] = 0;
        }

        m_exynosCameraParameters->setLDCaptureMode(MULTI_SHOT_MODE_NONE);
        m_exynosCameraParameters->setLDCaptureCount(0);
        m_exynosCameraParameters->setLDCaptureLLSValue(LLS_LEVEL_ZSL);
#ifdef SET_LLS_CAPTURE_SETFILE
        m_exynosCameraParameters->setLLSCaptureOn(false);
#endif
    }
#endif

    m_prePictureThread->requestExit();
    m_pictureThread->requestExit();
#ifdef SAMSUNG_LLS_DEBLUR
    m_LDCaptureThread->requestExit();
#endif
    m_postPictureGscThread->requestExit();
#ifdef SAMSUNG_DEBLUR
    m_detectDeblurCaptureThread->requestExit();
    m_deblurCaptureThread->requestExit();
#endif
    m_ThumbnailCallbackThread->requestExit();
#ifdef SAMSUNG_DNG
    m_DNGCaptureThread->requestExit();
#endif
    m_postPictureThread->requestExit();
    m_jpegCallbackThread->requestExit();

    CLOGI("INFO(%s[%d]): wait m_prePictureThrad", __FUNCTION__, __LINE__);
    m_prePictureThread->requestExitAndWait();
    CLOGI("INFO(%s[%d]): wait m_pictureThrad", __FUNCTION__, __LINE__);
    m_pictureThread->requestExitAndWait();
#ifdef SAMSUNG_LLS_DEBLUR
    CLOGI("INFO(%s[%d]): wait m_LDCaptureThread", __FUNCTION__, __LINE__);
    m_LDCaptureThread->requestExitAndWait();
#endif
    CLOGI("INFO(%s[%d]): wait m_postPictureGscThread", __FUNCTION__, __LINE__);
    m_postPictureGscThread->requestExitAndWait();
#ifdef SAMSUNG_DEBLUR
    CLOGI("INFO(%s[%d]): wait m_detectDeblurCaptureThread", __FUNCTION__, __LINE__);
    m_detectDeblurCaptureThread->requestExitAndWait();
    CLOGI("INFO(%s[%d]): wait m_deblurCaptureThread", __FUNCTION__, __LINE__);
    m_deblurCaptureThread->requestExitAndWait();
#endif
    CLOGI("INFO(%s[%d]): wait m_ThumbnailCallbackThread", __FUNCTION__, __LINE__);
    m_ThumbnailCallbackThread->requestExitAndWait();
#ifdef SAMSUNG_DNG
    CLOGI("INFO(%s[%d]): wait m_DNGCaptureThread", __FUNCTION__, __LINE__);
    m_DNGCaptureThread->requestExitAndWait();
#endif
    CLOGI("INFO(%s[%d]): wait m_postPictureThrad", __FUNCTION__, __LINE__);
    m_postPictureThread->requestExitAndWait();

    m_exynosCameraParameters->setIsThumbnailCallbackOn(false);

    CLOGI("INFO(%s[%d]): wait m_jpegCallbackThrad", __FUNCTION__, __LINE__);
    if (!callFromJpeg)
        m_jpegCallbackThread->requestExitAndWait();

    for (int threadNum = JPEG_SAVE_THREAD0; threadNum < JPEG_SAVE_THREAD_MAX_COUNT; threadNum++) {
         CLOGI("INFO(%s[%d]): wait m_jpegSaveThrad%d", __FUNCTION__, __LINE__, threadNum);
         m_jpegSaveThread[threadNum]->requestExitAndWait();
    }

    CLOGI("INFO(%s[%d]): All picture threads done", __FUNCTION__, __LINE__);

    if (m_exynosCameraParameters->isReprocessing() == true) {
        enum pipeline pipe = (isOwnScc(getCameraId()) == true) ? PIPE_SCC_REPROCESSING : PIPE_ISP_REPROCESSING;

        m_reprocessingFrameFactory->stopThread(pipe);
    }

#ifdef SAMSUNG_MAGICSHOT
    if (m_exynosCameraParameters->getShotMode() == SHOT_MODE_MAGIC) {
        if(m_exynosCameraParameters->isReprocessing() == true) {
            m_reprocessingFrameFactory->stopThread(PIPE_GSC_REPROCESSING2);
            CLOGI("INFO(%s[%d]):rear gsc , pipe stop(%d)",__FUNCTION__, __LINE__, PIPE_GSC_REPROCESSING2);
        } else {
            enum pipeline pipe = PIPE_GSC_VIDEO;

            m_previewFrameFactory->stopThread(pipe);
            CLOGI("INFO(%s[%d]):front gsc , pipe stop(%d)",__FUNCTION__, __LINE__, pipe);
        }
    }
#endif

    if(m_exynosCameraParameters->isReprocessing() == true) {
            m_reprocessingFrameFactory->stopThread(PIPE_GSC_REPROCESSING3);
            CLOGI("INFO(%s[%d]):rear gsc , pipe stop(%d)",__FUNCTION__, __LINE__, PIPE_GSC_REPROCESSING3);
    }

    while (m_jpegCallbackQ->getSizeOfProcessQ() > 0) {
        m_jpegCallbackQ->popProcessQ(&jpegCallbackBuf);
        jpegCallbackBuffer = jpegCallbackBuf.buffer;

        CLOGD("DEBUG(%s[%d]):put remaining jpeg buffer(index: %d)", __FUNCTION__, __LINE__, jpegCallbackBuffer.index);
        if (m_jpegBufferMgr->putBuffer(jpegCallbackBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE) != NO_ERROR) {
            CLOGE("ERR(%s[%d]):putBuffer(%d) fail", __FUNCTION__, __LINE__, jpegCallbackBuffer.index);
        }

#ifdef ONE_SECOND_BURST_CAPTURE
        if (m_exynosCameraParameters->getSeriesShotMode() != SERIES_SHOT_MODE_ONE_SECOND_BURST) {
#endif
            int seriesShotSaveLocation = m_exynosCameraParameters->getSeriesShotSaveLocation();
            char command[CAMERA_FILE_PATH_SIZE];
            memset(command, 0, sizeof(command));

#ifdef SAMSUNG_INF_BURST
            snprintf(command, sizeof(command), "rm %s%s_%03d.jpg", m_burstSavePath, m_burstTime, jpegCallbackBuf.callbackNumber);
#else
            snprintf(command, sizeof(command), "rm %sBurst%02d.jpg", m_burstSavePath, jpegCallbackBuf.callbackNumber);
#endif

            CLOGD("DEBUG(%s[%d]):run %s - start", __FUNCTION__, __LINE__, command);
            system(command);
            CLOGD("DEBUG(%s[%d]):run %s - end", __FUNCTION__, __LINE__, command);
#ifdef ONE_SECOND_BURST_CAPTURE
        }
#endif
    }

    m_getBufferManager(PIPE_GSC_REPROCESSING3, &thumbnailBufferMgr, SRC_BUFFER_DIRECTION);
    while (m_thumbnailCallbackQ->getSizeOfProcessQ() > 0) {
        m_thumbnailCallbackQ->popProcessQ(&thumbnailCallbackBuffer);

        CLOGD("DEBUG(%s[%d]):put remaining thumbnailCallbackBuffer buffer(index: %d)", __FUNCTION__, __LINE__, postviewCallbackBuffer.index);
        m_putBuffers(thumbnailBufferMgr, thumbnailCallbackBuffer.index);
    }

    m_getBufferManager(PIPE_GSC_REPROCESSING3, &postviewBufferMgr, DST_BUFFER_DIRECTION);
    while (m_postviewCallbackQ->getSizeOfProcessQ() > 0) {
        m_postviewCallbackQ->popProcessQ(&postviewCallbackBuffer);

        CLOGD("DEBUG(%s[%d]):put remaining postview buffer(index: %d)", __FUNCTION__, __LINE__, postviewCallbackBuffer.index);
        m_putBuffers(postviewBufferMgr, postviewCallbackBuffer.index);
    }

#ifdef SAMSUNG_DNG
    ExynosCameraBuffer dngBuffer;
    while (m_dngCaptureQ->getSizeOfProcessQ() > 0) {
        m_dngCaptureQ->popProcessQ(&dngBuffer);

        CLOGD("DEBUG(%s[%d]):put remaining dngBuffer buffer(index: %d)", __FUNCTION__, __LINE__, dngBuffer.index);
        m_putBuffers(m_fliteBufferMgr, dngBuffer.index);
    }
#endif

    for (int threadNum = JPEG_SAVE_THREAD0; threadNum < JPEG_SAVE_THREAD_MAX_COUNT; threadNum++) {
        while (m_jpegSaveQ[threadNum]->getSizeOfProcessQ() > 0) {
            m_jpegSaveQ[threadNum]->popProcessQ(&jpegCallbackBuf);
            jpegCallbackBuffer = jpegCallbackBuf.buffer;

            CLOGD("DEBUG(%s[%d]):put remaining SaveQ%d jpeg buffer(index: %d)",
                    __FUNCTION__, __LINE__, threadNum, jpegCallbackBuffer.index);
            if (m_jpegBufferMgr->putBuffer(jpegCallbackBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE) != NO_ERROR) {
                CLOGE("ERR(%s[%d]):putBuffer(%d) fail", __FUNCTION__, __LINE__, jpegCallbackBuffer.index);
            }

        }

            m_burst[threadNum] = false;
    }

    if (m_exynosCameraParameters->isReprocessing() == true) {
        int ret = 0;
        enum pipeline pipe = (isOwnScc(getCameraId()) == true) ? PIPE_SCC_REPROCESSING : PIPE_ISP_REPROCESSING;
        CLOGD("DEBUG(%s[%d]): Wait thread exit Pipe(%d) ", __FUNCTION__, __LINE__, pipe);
        ret = m_reprocessingFrameFactory->stopThreadAndWait(pipe);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):stopThread fail, pipe(%d) ret(%d)", __FUNCTION__, __LINE__, pipe,  ret);
        }
    }

    if (m_exynosCameraParameters->isReprocessing() == true) {
        int ret = 0;
        enum pipeline pipes[] = {PIPE_GSC_REPROCESSING, PIPE_GSC_REPROCESSING2, PIPE_GSC_REPROCESSING3};
        for(uint32_t i = 0; i < sizeof(pipes) / sizeof(enum pipeline); i++) {
            ret = m_reprocessingFrameFactory->stopThread(INDEX(pipes[i]));
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):GSC stopThread fail, pipe(%d) ret(%d)", __FUNCTION__, __LINE__, pipes[i],  ret);
            }
            ret = m_reprocessingFrameFactory->stopThreadAndWait(INDEX(pipes[i]));
            if (ret < 0) {
                CLOGE("ERR(%s[%d]): stopThreadAndWait fail, pipeId(%d) ret(%d)", __FUNCTION__, __LINE__, pipes[i], ret);
            }
        }
    }

    CLOGD("DEBUG(%s[%d]): clear postProcessList", __FUNCTION__, __LINE__);
    if (m_clearList(&m_postProcessList) < 0) {
        CLOGE("ERR(%s):m_clearList fail", __FUNCTION__);
    }

#if 1
    CLOGD("DEBUG(%s[%d]): clear dstPostPictureGscQ", __FUNCTION__, __LINE__);
    dstPostPictureGscQ->release();

    CLOGD("DEBUG(%s[%d]): clear postPictureQ", __FUNCTION__, __LINE__);
    m_postPictureQ->release();

    CLOGD("DEBUG(%s[%d]): clear dstSccReprocessingQ", __FUNCTION__, __LINE__);
    dstSccReprocessingQ->release();

    CLOGD("DEBUG(%s[%d]): clear dstJpegReprocessingQ", __FUNCTION__, __LINE__);
    dstJpegReprocessingQ->release();
#else
    ExynosCameraFrame *frame = NULL;

    CLOGD("DEBUG(%s[%d]): clear postPictureQ", __FUNCTION__, __LINE__);
    while(m_postPictureQ->getSizeOfProcessQ()) {
        m_postPictureQ->popProcessQ(&frame);
        if (frame != NULL) {
            delete frame;
            frame = NULL;
        }
    }

    CLOGD("DEBUG(%s[%d]): clear dstSccReprocessingQ", __FUNCTION__, __LINE__);
    while(dstSccReprocessingQ->getSizeOfProcessQ()) {
        dstSccReprocessingQ->popProcessQ(&frame);
        if (frame != NULL) {
            delete frame;
            frame = NULL;
        }
    }
#endif

#ifdef SAMSUNG_DEBLUR
    CLOGD("DEBUG(%s[%d]): clear m_deblurCaptureQ", __FUNCTION__, __LINE__);
    m_deblurCaptureQ->release();
    CLOGD("DEBUG(%s[%d]): clear m_detectDeblurCaptureQ", __FUNCTION__, __LINE__);
    m_detectDeblurCaptureQ->release();
#endif

#ifdef SAMSUNG_LLS_DEBLUR
    CLOGD("DEBUG(%s[%d]): clear m_LDCaptureQ", __FUNCTION__, __LINE__);
    m_LDCaptureQ->release();
#endif

    if (needGSCForCapture(getCameraId()) == true) {
        CLOGD("DEBUG(%s[%d]): reset postPictureGsc buffers", __FUNCTION__, __LINE__);
        m_postPictureGscBufferMgr->resetBuffers();
    }

    CLOGD("DEBUG(%s[%d]): reset thumbnail gsc buffers", __FUNCTION__, __LINE__);
    m_thumbnailGscBufferMgr->resetBuffers();

    CLOGD("DEBUG(%s[%d]): reset buffer gsc buffers", __FUNCTION__, __LINE__);
    m_gscBufferMgr->resetBuffers();
    CLOGD("DEBUG(%s[%d]): reset buffer jpeg buffers", __FUNCTION__, __LINE__);
    m_jpegBufferMgr->resetBuffers();
    CLOGD("DEBUG(%s[%d]): reset buffer sccReprocessing buffers", __FUNCTION__, __LINE__);
    m_sccReprocessingBufferMgr->resetBuffers();

#ifdef SAMSUNG_DNG
    if (m_exynosCameraParameters->getDNGCaptureModeOn()) {
        int waitCount = 100;
        while (m_previewFrameFactory->getRunningFrameCount(PIPE_FLITE) != 0 && 0 < waitCount) {
            CLOGD("DNG(%s[%d]): wait flite dqbuffer count(%d)",
                __FUNCTION__, __LINE__, m_previewFrameFactory->getRunningFrameCount(PIPE_FLITE));
            usleep(30000);
            waitCount--;
        }

        if (m_exynosCameraParameters->getCheckMultiFrame()) {
            CLOGE("[DNG](%s[%d]):PIPE_FLITE stop", __FUNCTION__, __LINE__);
            m_exynosCameraParameters->setUseDNGCapture(false);
            m_exynosCameraParameters->setCheckMultiFrame(false);
            m_previewFrameFactory->stopThread(PIPE_FLITE);
            m_previewFrameFactory->stopThreadAndWait(PIPE_FLITE);
        }

        CLOGD("DNG(%s[%d]): reset m_fliteBuffer buffers", __FUNCTION__, __LINE__);
        m_fliteBufferMgr->resetBuffers();
    }
#endif

#ifdef BURST_CAPTURE
    m_burstShutterLocation = BURST_SHUTTER_PREPICTURE;
#endif
}

bool ExynosCamera::m_highResolutionCallbackThreadFunc(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    int ret = 0;
    int loop = false;
    int retryCountGSC = 4;

    ExynosCameraFrame *newFrame = NULL;
    camera2_stream *shot_stream = NULL;

    ExynosCameraBuffer sccReprocessingBuffer;
    ExynosCameraBuffer highResolutionCbBuffer;

    int cbPreviewW = 0, cbPreviewH = 0;
    int previewFormat = 0;
    ExynosRect srcRect, dstRect;
    m_exynosCameraParameters->getPreviewSize(&cbPreviewW, &cbPreviewH);
    previewFormat = m_exynosCameraParameters->getPreviewFormat();

    int pipeId_scc = 0;
    int pipeId_gsc = 0;

    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    int planeCount = getYuvPlaneCount(previewFormat);
    int minBufferCount = 1;
    int maxBufferCount = 1;
    int buffer_idx = getShotBufferIdex();

    sccReprocessingBuffer.index = -2;
    highResolutionCbBuffer.index = -2;

    pipeId_scc = (isOwnScc(getCameraId()) == true) ? PIPE_SCC_REPROCESSING : PIPE_ISP_REPROCESSING;
    pipeId_gsc = PIPE_GSC_REPROCESSING;

    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;
    buffer_manager_allocation_mode_t allocMode = BUFFER_MANAGER_ALLOCATION_ONDEMAND;

    if (m_exynosCameraParameters->getHighResolutionCallbackMode() == false &&
        m_highResolutionCallbackRunning == false) {
        CLOGD("DEBUG(%s[%d]): High Resolution Callback Stop", __FUNCTION__, __LINE__);
        goto CLEAN;
    }

    ret = getYuvPlaneSize(previewFormat, planeSize, cbPreviewW, cbPreviewH);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]): BAD value, format(%x), size(%dx%d)",
            __FUNCTION__, __LINE__, previewFormat, cbPreviewW, cbPreviewH);
        return ret;
    }

    /* wait SCC */
    CLOGV("INFO(%s[%d]):wait SCC output", __FUNCTION__, __LINE__);
    ret = m_highResolutionCallbackQ->waitAndPopProcessQ(&newFrame);
    if (m_flagThreadStop == true) {
        CLOGI("INFO(%s[%d]):m_flagThreadStop(%d)", __FUNCTION__, __LINE__, m_flagThreadStop);
        goto CLEAN;
    }
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        // TODO: doing exception handling
        goto CLEAN;
    }
    if (newFrame == NULL) {
        CLOGE("ERR(%s[%d]):newFrame is NULL", __FUNCTION__, __LINE__);
        goto CLEAN;
    }

    ret = newFrame->setEntityState(pipeId_scc, ENTITY_STATE_COMPLETE);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setEntityState(ENTITY_STATE_PROCESSING) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_scc, ret);
        return ret;
    }
    CLOGV("INFO(%s[%d]):SCC output done", __FUNCTION__, __LINE__);

    if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME)) {
        /* check dst buffer state for NDONE */
        sccReprocessingBuffer.index = -1;

        if (newFrame->getRequest(PIPE_ISPC_REPROCESSING) == true) {
            /* get GSC src buffer */
            ret = newFrame->getDstBuffer(pipeId_scc, &sccReprocessingBuffer,
                                            m_reprocessingFrameFactory->getNodeType(PIPE_ISPC_REPROCESSING));
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):getSrcBuffer fail, pipeId(%d), ret(%d) frame(%d)",
                    __FUNCTION__, __LINE__, pipeId_scc, ret, newFrame->getFrameCount());
                goto CLEAN;
            }
        } else {
            CLOGE("ERR(%s[%d]):getRequest fail, pipeId(%d), ret(%d), frame(%d), dstBufPos(%d)",
                __FUNCTION__, __LINE__, pipeId_scc, ret, newFrame->getFrameCount(),
                m_reprocessingFrameFactory->getNodeType(PIPE_ISPC_REPROCESSING));
            goto CLEAN;
        }

        if (sccReprocessingBuffer.index >= 0) {
            entity_buffer_state_t bufferstate = ENTITY_BUFFER_STATE_NOREQ;
            ret = newFrame->getDstBufferState(pipeId_scc, &bufferstate,
                                                m_reprocessingFrameFactory->getNodeType(PIPE_ISPC_REPROCESSING));
            if (ret != NO_ERROR || bufferstate == ENTITY_BUFFER_STATE_ERROR) {
                CLOGE("ERR(%s[%d]):getDstBufferState fail, pipeId(%d), ret(%d) bufferState(%d) frame(%d)",
                    __FUNCTION__, __LINE__, pipeId_scc, ret, bufferstate, newFrame->getFrameCount());
                goto CLEAN;
            }
        }

        shot_stream = (struct camera2_stream *)(sccReprocessingBuffer.addr[buffer_idx]);
        if (shot_stream == NULL) {
            CLOGE("ERR(%s[%d]):shot_stream is NULL. buffer(%d)",
                    __FUNCTION__, __LINE__, sccReprocessingBuffer.index);
            goto CLEAN;
        }

        /* alloc GSC buffer */
        if (m_highResolutionCallbackBufferMgr->isAllocated() == false) {
            ret = m_allocBuffers(m_highResolutionCallbackBufferMgr, planeCount, planeSize,
                                    bytesPerLine, minBufferCount, maxBufferCount, type, allocMode, false, false);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):m_highResolutionCallbackBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
                    __FUNCTION__, __LINE__, minBufferCount, maxBufferCount);
                return ret;
            }
        }

        /* get GSC dst buffer */
        int bufIndex = -2;
        m_highResolutionCallbackBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &highResolutionCbBuffer);

        /* get preview callback heap */
        camera_memory_t *previewCallbackHeap = NULL;
        previewCallbackHeap = m_getMemoryCb(highResolutionCbBuffer.fd[0], highResolutionCbBuffer.size[0], 1, m_callbackCookie);
        if (!previewCallbackHeap || previewCallbackHeap->data == MAP_FAILED) {
            CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, highResolutionCbBuffer.size[0]);
            goto CLEAN;
        }

        ret = m_setCallbackBufferInfo(&highResolutionCbBuffer, (char *)previewCallbackHeap->data);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]): setCallbackBufferInfo fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            goto CLEAN;
        }

        /* set src/dst rect */
        srcRect.x = shot_stream->output_crop_region[0];
        srcRect.y = shot_stream->output_crop_region[1];
        srcRect.w = shot_stream->output_crop_region[2];
        srcRect.h = shot_stream->output_crop_region[3];

        ret = m_calcHighResolutionPreviewGSCRect(&srcRect, &dstRect);
        ret = newFrame->setSrcRect(pipeId_gsc, &srcRect);
        ret = newFrame->setDstRect(pipeId_gsc, &dstRect);

        CLOGV("DEBUG(%s[%d]):srcRect x : %d, y : %d, w : %d, h : %d", __FUNCTION__, __LINE__, srcRect.x, srcRect.y, srcRect.w, srcRect.h);
        CLOGV("DEBUG(%s[%d]):dstRect x : %d, y : %d, w : %d, h : %d", __FUNCTION__, __LINE__, dstRect.x, dstRect.y, dstRect.w, dstRect.h);

        ret = m_setupEntity(pipeId_gsc, newFrame, &sccReprocessingBuffer, &highResolutionCbBuffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):setupEntity fail, pipeId(%d), ret(%d)",
                __FUNCTION__, __LINE__, pipeId_gsc, ret);
            goto CLEAN;
        }

        /* push frame to GSC pipe */
        m_pictureFrameFactory->setOutputFrameQToPipe(dstGscReprocessingQ, pipeId_gsc);
        m_pictureFrameFactory->pushFrameToPipe(&newFrame, pipeId_gsc);

        /* wait GSC for high resolution preview callback */
        CLOGI("INFO(%s[%d]):wait GSC output", __FUNCTION__, __LINE__);
        newFrame = NULL;
        while (retryCountGSC > 0) {
            ret = dstGscReprocessingQ->waitAndPopProcessQ(&newFrame);
            if (ret == TIMED_OUT) {
                CLOGW("WRN(%s)(%d):wait and pop timeout, ret(%d)", __FUNCTION__, __LINE__, ret);
                m_pictureFrameFactory->startThread(pipeId_gsc);
            } else if (ret < 0) {
                CLOGE("ERR(%s)(%d):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
                /* TODO: doing exception handling */
                goto CLEAN;
            } else {
                break;
            }
            retryCountGSC--;
        }

        if (newFrame == NULL) {
            CLOGE("ERR(%s):newFrame is NULL", __FUNCTION__);
            goto CLEAN;
        }
        CLOGI("INFO(%s[%d]):GSC output done", __FUNCTION__, __LINE__);

        /* put SCC buffer */
        ret = newFrame->getDstBuffer(pipeId_scc, &sccReprocessingBuffer,
                                        m_reprocessingFrameFactory->getNodeType(PIPE_ISPC_REPROCESSING));
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_scc, ret);
            goto CLEAN;
        }
        ret = m_putBuffers(m_sccReprocessingBufferMgr, sccReprocessingBuffer.index);

        CLOGV("DEBUG(%s[%d]):high resolution preview callback", __FUNCTION__, __LINE__);
        if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME)) {
            setBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
            m_dataCb(CAMERA_MSG_PREVIEW_FRAME, previewCallbackHeap, 0, NULL, m_callbackCookie);
            clearBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
        }

        previewCallbackHeap->release(previewCallbackHeap);

        highResolutionCbBuffer.index = -2;
        ret = newFrame->getDstBuffer(pipeId_gsc, &highResolutionCbBuffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_gsc, ret);
        }

        /* put high resolution callback buffer */
        if (highResolutionCbBuffer.index >=0)
            m_putBuffers(m_highResolutionCallbackBufferMgr, highResolutionCbBuffer.index);

        if (newFrame != NULL) {
            newFrame->frameUnlock();
            ret = m_removeFrameFromList(&m_postProcessList, newFrame);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):remove frame from processList fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            }

            CLOGD("DEBUG(%s[%d]): Reprocessing frame delete(%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());
            newFrame->decRef();
            m_frameMgr->deleteFrame(newFrame);
            newFrame = NULL;
        }
    } else {
        CLOGD("DEBUG(%s[%d]): Preview callback message disabled, skip callback", __FUNCTION__, __LINE__);
        /* put SCC buffer */
        ret = newFrame->getDstBuffer(pipeId_scc, &sccReprocessingBuffer,
                                        m_reprocessingFrameFactory->getNodeType(PIPE_ISPC_REPROCESSING));
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_scc, ret);
            goto CLEAN;
        }
        ret = m_putBuffers(m_sccReprocessingBufferMgr, sccReprocessingBuffer.index);
    }

    if(m_flagThreadStop != true) {
        if (m_highResolutionCallbackQ->getSizeOfProcessQ() > 0 ||
            m_exynosCameraParameters->getHighResolutionCallbackMode() == true) {
            CLOGD("DEBUG(%s[%d]):highResolutionCallbackQ size(%d), highResolutionCallbackMode(%s), start again",
                    __FUNCTION__, __LINE__,
                    m_highResolutionCallbackQ->getSizeOfProcessQ(),
                    (m_exynosCameraParameters->getHighResolutionCallbackMode() == true)? "TRUE" : "FALSE");
            loop = true;
        }
    }

    CLOGI("INFO(%s[%d]):high resolution callback thread complete, loop(%d)", __FUNCTION__, __LINE__, loop);

    /* one shot */
    return loop;

CLEAN:
    if (newFrame != NULL) {
        sccReprocessingBuffer.index = -2;
        ret = newFrame->getDstBuffer(pipeId_scc, &sccReprocessingBuffer,
                                        m_reprocessingFrameFactory->getNodeType(PIPE_ISPC_REPROCESSING));
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_scc, ret);
        }
        if (sccReprocessingBuffer.index >= 0)
            m_putBuffers(m_sccReprocessingBufferMgr, sccReprocessingBuffer.index);

        highResolutionCbBuffer.index = -2;
        ret = newFrame->getDstBuffer(pipeId_gsc, &highResolutionCbBuffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_gsc, ret);
        }

        if (highResolutionCbBuffer.index >= 0)
            m_putBuffers(m_highResolutionCallbackBufferMgr, highResolutionCbBuffer.index);
    }

    if (newFrame != NULL) {
        newFrame->printEntity();

        newFrame->frameUnlock();
        ret = m_removeFrameFromList(&m_postProcessList, newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):remove frame from processList fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }

        newFrame->decRef();
        m_frameMgr->deleteFrame(newFrame);
        newFrame = NULL;
    }

    if(m_flagThreadStop != true) {
        if (m_highResolutionCallbackQ->getSizeOfProcessQ() > 0 ||
                m_exynosCameraParameters->getHighResolutionCallbackMode() == true) {
            CLOGD("DEBUG(%s[%d]):highResolutionCallbackQ size(%d), highResolutionCallbackMode(%s), start again",
                    __FUNCTION__, __LINE__,
                    m_highResolutionCallbackQ->getSizeOfProcessQ(),
                    (m_exynosCameraParameters->getHighResolutionCallbackMode() == true)? "TRUE" : "FALSE");
            loop = true;
        }
    }

    CLOGI("INFO(%s[%d]):high resolution callback thread fail, loop(%d)", __FUNCTION__, __LINE__, loop);

    /* one shot */
    return loop;
}

status_t ExynosCamera::m_doPrviewToRecordingFunc(
        int32_t pipeId,
        ExynosCameraBuffer previewBuf,
        ExynosCameraBuffer recordingBuf,
        nsecs_t timeStamp)
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif

    CLOGV("DEBUG(%s[%d]):--IN-- (previewBuf.index=%d, recordingBuf.index=%d)",
        __FUNCTION__, __LINE__, previewBuf.index, recordingBuf.index);

    status_t ret = NO_ERROR;
    ExynosRect srcRect, dstRect;
    ExynosCameraFrame  *newFrame = NULL;
    struct camera2_node_output node;

    newFrame = m_previewFrameFactory->createNewFrameVideoOnly();
    if (newFrame == NULL) {
        CLOGE("ERR(%s):newFrame is NULL", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    /* TODO: HACK: Will be removed, this is driver's job */
    m_convertingStreamToShotExt(&previewBuf, &node);
    setMetaDmSensorTimeStamp((struct camera2_shot_ext*)previewBuf.addr[previewBuf.planeCount-1], timeStamp);

    /* csc and scaling */
    ret = m_calcRecordingGSCRect(&srcRect, &dstRect);
    ret = newFrame->setSrcRect(pipeId, srcRect);
    ret = newFrame->setDstRect(pipeId, dstRect);

    ret = m_setupEntity(pipeId, newFrame, &previewBuf, &recordingBuf);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setupEntity fail, pipeId(%d), ret(%d)",
            __FUNCTION__, __LINE__, pipeId, ret);
        ret = INVALID_OPERATION;
        if (newFrame != NULL) {
            newFrame->decRef();
            m_frameMgr->deleteFrame(newFrame);
            newFrame = NULL;
        }
        goto func_exit;
    }
    m_recordingListLock.lock();
    m_recordingProcessList.push_back(newFrame);
    m_recordingListLock.unlock();
    m_previewFrameFactory->setOutputFrameQToPipe(m_recordingQ, pipeId);

    m_recordingStopLock.lock();
    if (m_getRecordingEnabled() == false) {
        m_recordingStopLock.unlock();
        CLOGD("DEBUG(%s[%d]): m_getRecordingEnabled is false, skip frame(%d) previewBuf(%d) recordingBuf(%d)",
            __FUNCTION__, __LINE__, newFrame->getFrameCount(), previewBuf.index, recordingBuf.index);
        if (newFrame != NULL) {
            newFrame->decRef();
            m_frameMgr->deleteFrame(newFrame);
            newFrame = NULL;
        }

        if (recordingBuf.index >= 0){
            m_putBuffers(m_recordingBufferMgr, recordingBuf.index);
        }
        goto func_exit;
    }
    m_previewFrameFactory->pushFrameToPipe(&newFrame, pipeId);
    m_recordingStopLock.unlock();

func_exit:

    CLOGV("DEBUG(%s[%d]):--OUT--", __FUNCTION__, __LINE__);
    return ret;

}

bool ExynosCamera::m_recordingThreadFunc(void)
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif

    int  ret  = 0;
    int  pipeId = PIPE_GSC_VIDEO;
    nsecs_t timeStamp = 0;

    ExynosCameraBuffer buffer;
    ExynosCameraFrame  *frame = NULL;

    CLOGV("INFO(%s[%d]):wait gsc done output", __FUNCTION__, __LINE__);
    ret = m_recordingQ->waitAndPopProcessQ(&frame);
    if (m_getRecordingEnabled() == false) {
        CLOGI("INFO(%s[%d]):recording stopped", __FUNCTION__, __LINE__);
        goto func_exit;
    }

    if (ret < 0) {
        if (ret == TIMED_OUT) {
            CLOGW("WARN(%s):wait timeout", __FUNCTION__);
        } else {
            CLOGE("ERR(%s):wait and pop fail, ret(%d)", __FUNCTION__, ret);
            /* TODO: doing exception handling */
        }
        goto func_exit;
    }

    if (frame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        goto func_exit;
    }
    CLOGV("INFO(%s[%d]):gsc done for recording callback", __FUNCTION__, __LINE__);

    ret = frame->getSrcBuffer(pipeId, &buffer);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getDstBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        goto func_exit;
    }

    timeStamp = getMetaDmSensorTimeStamp((struct camera2_shot_ext*)buffer.addr[buffer.planeCount-1]);

    ret = frame->getDstBuffer(pipeId, &buffer);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getDstBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        goto func_exit;
    }

    if (buffer.index < 0 || buffer.index >= (int)m_recordingBufferCount) {
        CLOGE("ERR(%s[%d]):Out of Index! (Max: %d, Index: %d)", __FUNCTION__, __LINE__, m_recordingBufferCount, buffer.index);
        goto func_exit;
    }

    if (m_recordingStartTimeStamp == 0) {
        m_recordingStartTimeStamp = timeStamp;
        CLOGI("INFO(%s[%d]):m_recordingStartTimeStamp=%lld",
                __FUNCTION__, __LINE__, (long long)m_recordingStartTimeStamp);
    }

    if ((0L < timeStamp)
        && (m_lastRecordingTimeStamp < timeStamp)
        && (m_recordingStartTimeStamp <= timeStamp)) {
        if (m_getRecordingEnabled() == true
            && m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)) {
#ifdef CHECK_MONOTONIC_TIMESTAMP
            CLOGD("DEBUG(%s[%d]):m_dataCbTimestamp::recordingFrameIndex=%d, recordingTimeStamp=%lld",
                __FUNCTION__, __LINE__, buffer.index, timeStamp);
#endif
#ifdef DEBUG
            CLOGD("DEBUG(%s[%d]): - lastTimeStamp(%lld), systemTime(%lld), recordingStart(%lld)",
                __FUNCTION__, __LINE__,
                m_lastRecordingTimeStamp,
                systemTime(SYSTEM_TIME_MONOTONIC),
                m_recordingStartTimeStamp);
#endif
            struct VideoNativeHandleMetadata *recordAddrs = NULL;

            recordAddrs = (struct VideoNativeHandleMetadata *)m_recordingCallbackHeap->data;
            recordAddrs[buffer.index].eType = kMetadataBufferTypeNativeHandleSource;

            native_handle* handle = native_handle_create(2, 1);
            handle->data[0] = (int32_t) buffer.fd[0];
            handle->data[1] = (int32_t) buffer.fd[1];
            handle->data[2] = (int32_t) buffer.index;

            recordAddrs[buffer.index].pHandle = handle;

            m_recordingBufAvailable[buffer.index] = false;

            m_lastRecordingTimeStamp = timeStamp;

#ifdef SAMSUNG_HLV
            if (m_HLV) {
                /* Ignore the ERROR .. HLV solution is smart */
                m_ProgramAndProcessHLV(&buffer);
            }
#endif

            m_dataCbTimestamp(
                    timeStamp,
                    CAMERA_MSG_VIDEO_FRAME,
                    m_recordingCallbackHeap,
                    buffer.index,
                    m_callbackCookie);
        }
    } else {
        CLOGW("WARN(%s[%d]):recordingFrameIndex=%d, timeStamp(%lld) invalid -"
            " lastTimeStamp(%lld), systemTime(%lld), recordingStart(%lld)",
            __FUNCTION__, __LINE__, buffer.index, (long long)timeStamp,
            (long long)m_lastRecordingTimeStamp,
            (long long)systemTime(SYSTEM_TIME_MONOTONIC),
            (long long)m_recordingStartTimeStamp);
        m_releaseRecordingBuffer(buffer.index);
    }

func_exit:

    m_recordingListLock.lock();
    if (frame != NULL) {
        ret = m_removeFrameFromList(&m_recordingProcessList, frame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):remove frame from processList fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }
        frame->decRef();
        m_frameMgr->deleteFrame(frame);;
        frame = NULL;
    }
    m_recordingListLock.unlock();

    return m_recordingEnabled;
}

status_t ExynosCamera::m_releaseRecordingBuffer(int bufIndex)
{
    status_t ret = NO_ERROR;

    if (bufIndex < 0 || bufIndex >= (int)m_recordingBufferCount) {
        CLOGE("ERR(%s):Out of Index! (Max: %d, Index: %d)", __FUNCTION__, m_recordingBufferCount, bufIndex);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    ret = m_putBuffers(m_recordingBufferMgr, bufIndex);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
    }

func_exit:

    return ret;
}

status_t ExynosCamera::m_calcPreviewGSCRect(ExynosRect *srcRect, ExynosRect *dstRect)
{
    return m_exynosCameraParameters->calcPreviewGSCRect(srcRect, dstRect);
}

status_t ExynosCamera::m_calcHighResolutionPreviewGSCRect(ExynosRect *srcRect, ExynosRect *dstRect)
{
    return m_exynosCameraParameters->calcHighResolutionPreviewGSCRect(srcRect, dstRect);
}

status_t ExynosCamera::m_calcRecordingGSCRect(ExynosRect *srcRect, ExynosRect *dstRect)
{
    return m_exynosCameraParameters->calcRecordingGSCRect(srcRect, dstRect);
}

status_t ExynosCamera::m_calcPictureRect(ExynosRect *srcRect, ExynosRect *dstRect)
{
    return m_exynosCameraParameters->calcPictureRect(srcRect, dstRect);
}

status_t ExynosCamera::m_calcPictureRect(int originW, int originH, ExynosRect *srcRect, ExynosRect *dstRect)
{
    return m_exynosCameraParameters->calcPictureRect(originW, originH, srcRect, dstRect);
}

status_t ExynosCamera::m_searchFrameFromList(List<ExynosCameraFrame *> *list, uint32_t frameCount, ExynosCameraFrame **frame)
{
    Mutex::Autolock lock(m_searchframeLock);
    int ret = 0;
    ExynosCameraFrame *curFrame = NULL;
    List<ExynosCameraFrame *>::iterator r;

    if (list->empty()) {
        CLOGD("DEBUG(%s[%d]):list is empty", __FUNCTION__, __LINE__);
        return NO_ERROR;
    }

    r = list->begin()++;

    do {
        curFrame = *r;
        if (curFrame == NULL) {
            CLOGE("ERR(%s):curFrame is empty", __FUNCTION__);
            return INVALID_OPERATION;
        }

        if (frameCount == curFrame->getFrameCount()) {
            CLOGV("DEBUG(%s):frame count match: expected(%d)", __FUNCTION__, frameCount);
            *frame = curFrame;
            return NO_ERROR;
        }
        r++;
    } while (r != list->end());

    CLOGV("DEBUG(%s[%d]):Cannot find match frame, frameCount(%d)", __FUNCTION__, __LINE__, frameCount);

    return NO_ERROR;
}

status_t ExynosCamera::m_removeFrameFromList(List<ExynosCameraFrame *> *list, ExynosCameraFrame *frame)
{
    Mutex::Autolock lock(m_searchframeLock);
    int ret = 0;
    ExynosCameraFrame *curFrame = NULL;
    int frameCount = 0;
    int curFrameCount = 0;
    List<ExynosCameraFrame *>::iterator r;

    if (frame == NULL) {
        CLOGE("ERR(%s):frame is NULL", __FUNCTION__);
        return BAD_VALUE;
    }

    if (list->empty()) {
        CLOGD("DEBUG(%s):list is empty", __FUNCTION__);
        return NO_ERROR;
    }

    frameCount = frame->getFrameCount();
    r = list->begin()++;

    do {
        curFrame = *r;
        if (curFrame == NULL) {
            CLOGE("ERR(%s):curFrame is empty", __FUNCTION__);
            return INVALID_OPERATION;
        }

        curFrameCount = curFrame->getFrameCount();
        if (frameCount == curFrameCount) {
            CLOGV("DEBUG(%s):frame count match: expected(%d), current(%d)", __FUNCTION__, frameCount, curFrameCount);
            list->erase(r);
            return NO_ERROR;
        }
        CLOGW("WARN(%s):frame count mismatch: expected(%d), current(%d)", __FUNCTION__, frameCount, curFrameCount);
        /* removed message */
        /* curFrame->printEntity(); */
        r++;
    } while (r != list->end());

    CLOGE("ERR(%s):Cannot find match frame!!!", __FUNCTION__);

    return INVALID_OPERATION;
}

status_t ExynosCamera::m_deleteFrame(ExynosCameraFrame **frame)
{
    status_t ret = NO_ERROR;

    /* put lock using this frame */
    Mutex::Autolock lock(m_searchframeLock);

    if (*frame == NULL) {
        CLOGE("ERR(%s[%d]):frame == NULL. so, fail", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    if ((*frame)->getFrameLockState() == false) {
        if ((*frame)->isComplete() == true) {
            CLOGD("DEBUG(%s[%d]): Reprocessing frame delete(%d)", __FUNCTION__, __LINE__, (*frame)->getFrameCount());

            (*frame)->decRef();
            m_frameMgr->deleteFrame(*frame);
        }
    }

    return NO_ERROR;
}

status_t ExynosCamera::m_clearList(List<ExynosCameraFrame *> *list)
{
    Mutex::Autolock lock(m_searchframeLock);
    int ret = 0;
    ExynosCameraFrame *curFrame = NULL;
    List<ExynosCameraFrame *>::iterator r;

    CLOGD("DEBUG(%s):remaining frame(%d), we remove them all", __FUNCTION__, (int)list->size());

    while (!list->empty()) {
        r = list->begin()++;
        curFrame = *r;
        if (curFrame != NULL) {
            CLOGV("DEBUG(%s):remove frame count %d", __FUNCTION__, curFrame->getFrameCount() );
            curFrame->decRef();
            m_frameMgr->deleteFrame(curFrame);
            curFrame = NULL;
        }
        list->erase(r);
    }
    CLOGD("DEBUG(%s):EXIT ", __FUNCTION__);

    return NO_ERROR;
}

status_t ExynosCamera::m_clearList(frame_queue_t *queue)
{
    Mutex::Autolock lock(m_searchframeLock);
    int ret = 0;
    ExynosCameraFrame *curFrame = NULL;

    CLOGD("DEBUG(%s):remaining frame(%d), we remove them all", __FUNCTION__, queue->getSizeOfProcessQ());

    while (0 < queue->getSizeOfProcessQ()) {
        queue->popProcessQ(&curFrame);
        if (curFrame != NULL) {
            CLOGV("DEBUG(%s):remove frame count %d", __FUNCTION__, curFrame->getFrameCount() );
            curFrame->decRef();
            m_frameMgr->deleteFrame(curFrame);
            curFrame = NULL;
        }
    }
    CLOGD("DEBUG(%s):EXIT ", __FUNCTION__);

    return NO_ERROR;
}

status_t ExynosCamera::m_clearFrameQ(frame_queue_t *frameQ, uint32_t pipeId, uint32_t direction) {
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraFrameEntity *entity = NULL;
    ExynosCameraBuffer deleteSccBuffer;
    ExynosCameraBufferManager *bufferMgr = NULL;
    int ret = NO_ERROR;

    if (frameQ == NULL) {
        CLOGE("ERR(%s[%d]):frameQ is NULL.", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    CLOGI("INFO(%s[%d]): IN... frameQSize(%d)", __FUNCTION__, __LINE__, frameQ->getSizeOfProcessQ());

    while (0 < frameQ->getSizeOfProcessQ()) {
        ret = frameQ->popProcessQ(&newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            continue;
        }

        if (newFrame == NULL) {
            CLOGE("ERR(%s[%d]):newFrame is NULL", __FUNCTION__, __LINE__);
            ret = INVALID_OPERATION;
            continue;
        }

        if (direction == SRC_BUFFER_DIRECTION) {
            ret = newFrame->getSrcBuffer(pipeId, &deleteSccBuffer);
        } else {
            if(m_previewFrameFactory == NULL) {
                return INVALID_OPERATION;
            }
            ret = newFrame->getDstBuffer(pipeId, &deleteSccBuffer, m_previewFrameFactory->getNodeType(pipeId));
        }
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getSrcBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
            continue;
        }

        ret = m_getBufferManager(pipeId, &bufferMgr, direction);
        if (ret < 0)
            CLOGE("ERR(%s[%d]):getBufferManager(SRC) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);

        /* put SCC buffer */
        CLOGD("DEBUG(%s)(%d):m_putBuffer by clearjpegthread(dstSccRe), index(%d)", __FUNCTION__, __LINE__, deleteSccBuffer.index);
        ret = m_putBuffers(bufferMgr, deleteSccBuffer.index);
        if (ret < 0)
            CLOGE("ERR(%s[%d]):bufferMgr->putBuffers() fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
    }

    return ret;
}

status_t ExynosCamera::m_printFrameList(List<ExynosCameraFrame *> *list)
{
    int ret = 0;
    ExynosCameraFrame *curFrame = NULL;
    List<ExynosCameraFrame *>::iterator r;

    CLOGD("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    CLOGD("\t remaining frame count(%d)", (int)list->size());

    r = list->begin()++;

    do {
        curFrame = *r;
        if (curFrame != NULL) {
            CLOGI("\t hal frame count %d", curFrame->getFrameCount() );
            curFrame->printEntity();
        }

        r++;
    } while (r != list->end());
    CLOGD("----------------------------------------------------------------------------");

    return NO_ERROR;
}

status_t ExynosCamera::m_createIonAllocator(ExynosCameraIonAllocator **allocator)
{
    status_t ret = NO_ERROR;
    int retry = 0;
    do {
        retry++;
        CLOGI("INFO(%s[%d]):try(%d) to create IonAllocator", __FUNCTION__, __LINE__, retry);
        *allocator = new ExynosCameraIonAllocator();
        ret = (*allocator)->init(false);
        if (ret < 0)
            CLOGE("ERR(%s[%d]):create IonAllocator fail (retryCount=%d)", __FUNCTION__, __LINE__, retry);
        else {
            CLOGD("DEBUG(%s[%d]):m_createIonAllocator success (allocator=%p)", __FUNCTION__, __LINE__, *allocator);
            break;
        }
    } while (ret < 0 && retry < 3);

    if (ret < 0 && retry >=3) {
        CLOGE("ERR(%s[%d]):create IonAllocator fail (retryCount=%d)", __FUNCTION__, __LINE__, retry);
        ret = INVALID_OPERATION;
    }

    return ret;
}

status_t ExynosCamera::m_createInternalBufferManager(ExynosCameraBufferManager **bufferManager, const char *name)
{
    return m_createBufferManager(bufferManager, name, BUFFER_MANAGER_ION_TYPE);
}

status_t ExynosCamera::m_createBufferManager(
        ExynosCameraBufferManager **bufferManager,
        const char *name,
        buffer_manager_type type)
{
    status_t ret = NO_ERROR;

    if (m_ionAllocator == NULL) {
        ret = m_createIonAllocator(&m_ionAllocator);
        if (ret < 0)
            CLOGE("ERR(%s[%d]):m_createIonAllocator fail", __FUNCTION__, __LINE__);
        else
            CLOGD("DEBUG(%s[%d]):m_createIonAllocator success", __FUNCTION__, __LINE__);
    }

    *bufferManager = ExynosCameraBufferManager::createBufferManager(type);
    (*bufferManager)->create(name, m_cameraId, m_ionAllocator);

    CLOGD("DEBUG(%s):BufferManager(%s) created", __FUNCTION__, name);

    return ret;
}

bool ExynosCamera::m_releasebuffersForRealloc()
{
    status_t ret = NO_ERROR;
    /* skip to free and reallocate buffers : flite / 3aa / isp / ispReprocessing */
    CLOGD("DEBUG(%s[%d]):m_setBuffers free all buffers", __FUNCTION__, __LINE__);
    if (m_bayerBufferMgr != NULL) {
        m_bayerBufferMgr->deinit();
    }
#ifdef SAMSUNG_DNG
    if (m_fliteBufferMgr != NULL) {
        m_fliteBufferMgr->deinit();
    }
#endif
    if (m_3aaBufferMgr != NULL) {
        m_3aaBufferMgr->deinit();
    }
    if (m_ispBufferMgr != NULL) {
        m_ispBufferMgr->deinit();
    }
    if (m_hwDisBufferMgr != NULL) {
        m_hwDisBufferMgr->deinit();
    }

    /* realloc callback buffers */
    if (m_scpBufferMgr != NULL) {
        m_scpBufferMgr->deinit();
        m_scpBufferMgr->setBufferCount(0);
    }

    if (m_sccBufferMgr != NULL) {
        m_sccBufferMgr->deinit();
    }

    if (m_postPictureGscBufferMgr != NULL) {
        m_postPictureGscBufferMgr->deinit();
    }

    if (m_thumbnailGscBufferMgr != NULL) {
        m_thumbnailGscBufferMgr->deinit();
    }

#ifdef SAMSUNG_LBP
    if (m_lbpBufferMgr != NULL) {
        m_lbpBufferMgr->deinit();
    }
#endif

    if (m_previewCallbackBufferMgr != NULL) {
        m_previewCallbackBufferMgr->deinit();
    }
    if (m_highResolutionCallbackBufferMgr != NULL) {
        m_highResolutionCallbackBufferMgr->deinit();
    }

    m_exynosCameraParameters->setReallocBuffer(false);

    if (m_exynosCameraParameters->getRestartPreview() == true) {
        ret = setPreviewWindow(m_previewWindow);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):setPreviewWindow fail", __FUNCTION__, __LINE__);
            return INVALID_OPERATION;
        }
    }

   return true;
}


status_t ExynosCamera::m_setBuffers(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    CLOGI("INFO(%s[%d]):alloc buffer - camera ID: %d",
        __FUNCTION__, __LINE__, m_cameraId);
    int ret = 0;
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES]    = {0};
    int hwPreviewW, hwPreviewH;
    int hwPictureW, hwPictureH;

    int ispBufferW, ispBufferH;
    int previewMaxW, previewMaxH;
    int pictureMaxW, pictureMaxH;
    int sensorMaxW, sensorMaxH;
    ExynosRect bdsRect;

    int planeCount  = 1;
    int maxBufferCount = 1;
    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
    buffer_manager_allocation_mode_t allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;

    if( m_exynosCameraParameters->getReallocBuffer() ) {
        /* skip to free and reallocate buffers : flite / 3aa / isp / ispReprocessing */
        m_releasebuffersForRealloc();
    }

    m_exynosCameraParameters->getHwPreviewSize(&hwPreviewW, &hwPreviewH);
    CLOGI("(%s):HW Preview width x height = %dx%d", __FUNCTION__, hwPreviewW, hwPreviewH);
    m_exynosCameraParameters->getHwPictureSize(&hwPictureW, &hwPictureH);
    CLOGI("(%s):HW Picture width x height = %dx%d", __FUNCTION__, hwPictureW, hwPictureH);
    m_exynosCameraParameters->getMaxPictureSize(&pictureMaxW, &pictureMaxH);
    CLOGI("(%s):Picture MAX width x height = %dx%d", __FUNCTION__, pictureMaxW, pictureMaxH);
    if( m_exynosCameraParameters->getHighSpeedRecording() ) {
        m_exynosCameraParameters->getHwSensorSize(&sensorMaxW, &sensorMaxH);
        CLOGI("(%s):HW Sensor(HighSpeed) MAX width x height = %dx%d", __FUNCTION__, sensorMaxW, sensorMaxH);
        m_exynosCameraParameters->getHwPreviewSize(&previewMaxW, &previewMaxH);
        CLOGI("(%s):HW Preview(HighSpeed) MAX width x height = %dx%d", __FUNCTION__, previewMaxW, previewMaxH);
    } else {
        m_exynosCameraParameters->getMaxSensorSize(&sensorMaxW, &sensorMaxH);
        CLOGI("(%s):Sensor MAX width x height = %dx%d", __FUNCTION__, sensorMaxW, sensorMaxH);
        m_exynosCameraParameters->getMaxPreviewSize(&previewMaxW, &previewMaxH);
        CLOGI("(%s):Preview MAX width x height = %dx%d", __FUNCTION__, previewMaxW, previewMaxH);
    }

#if (SUPPORT_BACK_HW_VDIS || SUPPORT_FRONT_HW_VDIS)
    /*
     * we cannot expect TPU on or not, when open() api.
     * so extract memory TPU size
     */
    int w = 0, h = 0;
    m_exynosCameraParameters->calcNormalToTpuSize(previewMaxW, previewMaxH, &w, &h);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):Hw vdis buffer calulation fail src(%d x %d) dst(%d x %d)",__FUNCTION__, __LINE__, previewMaxW, previewMaxH, w, h);
    }
    previewMaxW = w;
    previewMaxH = h;
    CLOGI("(%s): TPU based Preview MAX width x height = %dx%d", __FUNCTION__, previewMaxW, previewMaxH);
#endif

    m_exynosCameraParameters->getPreviewBdsSize(&bdsRect);

    /* FLITE */
#ifdef CAMERA_PACKED_BAYER_ENABLE
#ifdef DEBUG_RAWDUMP
    if (m_exynosCameraParameters->checkBayerDumpEnable()) {
        bytesPerLine[0] = sensorMaxW * 2;
        planeSize[0] = sensorMaxW * sensorMaxH * 2;
    } else
#endif /* DEBUG_RAWDUMP */
    {
        bytesPerLine[0] = ROUND_UP(sensorMaxW , 10) * 8 / 5;
        planeSize[0] = bytesPerLine[0] * sensorMaxH;
    }
#else
    if (m_exynosCameraParameters->getBayerFormat() == V4L2_PIX_FMT_SBGGR12) {
        bytesPerLine[0] = ROUND_UP(sensorMaxW , 10) * 8 / 5;
        planeSize[0] = bytesPerLine[0] * sensorMaxH;
    } else {
        bytesPerLine[0] = sensorMaxW * 2;
        planeSize[0] = sensorMaxW * sensorMaxH * 2;
    }
#endif
    planeCount  = 2;

    /* TO DO : make num of buffers samely */
    maxBufferCount = m_exynosconfig->current->bufInfo.num_bayer_buffers;
#ifdef RESERVED_MEMORY_ENABLE
    if (getCameraId() == CAMERA_ID_BACK) {
        m_bayerBufferMgr->setContigBufCount(RESERVED_NUM_BAYER_BUFFERS);

#ifdef CAMERA_ADD_BAYER_ENABLE
        type = EXYNOS_CAMERA_BUFFER_ION_CACHED_RESERVED_TYPE;

        ret = m_allocBuffers(m_bayerBufferMgr, planeCount, planeSize, bytesPerLine,
                         maxBufferCount, maxBufferCount, type, true, true);
#else
        type = EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE;

        ret = m_allocBuffers(m_bayerBufferMgr, planeCount, planeSize, bytesPerLine,
                     maxBufferCount, maxBufferCount, type, true, false);
#endif
    } else {
        if (m_exynosCameraParameters->getDualMode() == false) {
            type = EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE;
            m_bayerBufferMgr->setContigBufCount(FRONT_RESERVED_NUM_BAYER_BUFFERS);
        }
        ret = m_allocBuffers(m_bayerBufferMgr, planeCount, planeSize, bytesPerLine,
                 maxBufferCount, maxBufferCount, type, true, false);
    }
#else
    ret = m_allocBuffers(m_bayerBufferMgr, planeCount, planeSize, bytesPerLine,
                 maxBufferCount, maxBufferCount, type, true, false);
#endif

    if (ret < 0) {
        if (getCameraId() == CAMERA_ID_FRONT &&
                type == EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE){
            CLOGD("DEBUG(%s[%d]):can't alloc reserved memory of BayerBuffers(%d). so, change memory type",
                __FUNCTION__, __LINE__, type);

            ret = m_allocBuffers(m_bayerBufferMgr, planeCount, planeSize, bytesPerLine,
                                 maxBufferCount, maxBufferCount, EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE, true, false);

            if (ret < 0) {
                CLOGE("ERR(%s[%d]):bayerBuffer m_allocBuffers(bufferCount=%d) fail",
                    __FUNCTION__, __LINE__, maxBufferCount);
                return ret;
            }
        } else {
            CLOGE("ERR(%s[%d]):bayerBuffer m_allocBuffers(bufferCount=%d) fail",
                __FUNCTION__, __LINE__, maxBufferCount);
            return ret;
        }
    }

    type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;

#ifdef CAMERA_PACKED_BAYER_ENABLE
    memset(&bytesPerLine, 0, sizeof(unsigned int) * EXYNOS_CAMERA_BUFFER_MAX_PLANES);
#else
    if (m_exynosCameraParameters->getBayerFormat() == V4L2_PIX_FMT_SBGGR12)
        memset(&bytesPerLine, 0, sizeof(unsigned int) * EXYNOS_CAMERA_BUFFER_MAX_PLANES);
#endif
    /* for preview */
    planeSize[0] = 32 * 64 * 2;
    planeCount  = 2;
    /* TO DO : make num of buffers samely */
    if (m_exynosCameraParameters->isFlite3aaOtf() == true)
        maxBufferCount = m_exynosconfig->current->bufInfo.num_3aa_buffers;
    else
        maxBufferCount = m_exynosconfig->current->bufInfo.num_bayer_buffers;

    ret = m_allocBuffers(m_3aaBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, true, false);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_3aaBufferMgr m_allocBuffers(bufferCount=%d) fail",
            __FUNCTION__, __LINE__, maxBufferCount);
        return ret;
    }

#if defined (USE_ISP_BUFFER_SIZE_TO_BDS)
    ispBufferW = bdsRect.w;
    ispBufferH = bdsRect.h;
#else
    ispBufferW = previewMaxW;
    ispBufferH = previewMaxH;
#endif

#ifdef CAMERA_PACKED_BAYER_ENABLE
    bytesPerLine[0] = ROUND_UP((ispBufferW * 3 / 2), 16);
    planeSize[0] = bytesPerLine[0] * ispBufferH;
#else
    if (m_exynosCameraParameters->getBayerFormat() == V4L2_PIX_FMT_SBGGR12) {
        bytesPerLine[0] = ROUND_UP((ispBufferW * 3 / 2), 16);
        planeSize[0] = bytesPerLine[0] * ispBufferH;
    } else {
        bytesPerLine[0] = ispBufferW * 2;
        planeSize[0] = ispBufferW * ispBufferH * 2;
    }
#endif

    planeCount  = 2;
    /* TO DO : make num of buffers samely */
    if (m_exynosCameraParameters->is3aaIspOtf() == false) {
        if (m_exynosCameraParameters->isFlite3aaOtf() == true) {
            maxBufferCount = m_exynosconfig->current->bufInfo.num_3aa_buffers;
#ifdef RESERVED_MEMORY_ENABLE
            if (getCameraId() == CAMERA_ID_BACK) {
                type = EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE;
                if(m_exynosCameraParameters->getUHDRecordingMode() == true) {
                    m_ispBufferMgr->setContigBufCount(RESERVED_NUM_ISP_BUFFERS_ON_UHD);
                } else {
                    m_ispBufferMgr->setContigBufCount(RESERVED_NUM_ISP_BUFFERS);
                }
            } else {
                if (m_exynosCameraParameters->getDualMode() == false) {
                    type = EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE;
                    m_ispBufferMgr->setContigBufCount(FRONT_RESERVED_NUM_ISP_BUFFERS);
                }
            }
#endif
        } else {
            maxBufferCount = m_exynosconfig->current->bufInfo.num_bayer_buffers;
#ifdef RESERVED_MEMORY_ENABLE
            if (m_exynosCameraParameters->getDualMode() == false) {
                type = EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE;
                m_ispBufferMgr->setContigBufCount(FRONT_RESERVED_NUM_ISP_BUFFERS);
            }
#endif
        }

        ret = m_allocBuffers(m_ispBufferMgr, planeCount, planeSize, bytesPerLine,
                maxBufferCount, maxBufferCount, type, true, false);
        if (ret < 0) {
            if (getCameraId() == CAMERA_ID_FRONT && type == EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE) {
                CLOGD("DEBUG(%s[%d]):can't alloc reserved memory of IspBuffers(%d). so, change memory type",
                    __FUNCTION__, __LINE__, type);
                ret = m_allocBuffers(m_ispBufferMgr,
                                    planeCount,
                                    planeSize,
                                    bytesPerLine,
                                    maxBufferCount,
                                    maxBufferCount,
                                    EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE,
                                    true,
                                    false);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):m_ispBufferMgr m_allocBuffers(bufferCount=%d) fail",
                        __FUNCTION__, __LINE__, maxBufferCount);
                    return ret;
                }
            } else {
                CLOGE("ERR(%s[%d]):m_ispBufferMgr m_allocBuffers(bufferCount=%d) fail",
                    __FUNCTION__, __LINE__, maxBufferCount);
                return ret;
            }
        }
    } else {
        CLOGD("DEBUG(%s[%d]):SKIP m_ispBufferMgr m_allocBuffers(bufferCount=%d) by 3aaIspOtf",
            __FUNCTION__, __LINE__, maxBufferCount);
    }

    /* HW VDIS memory */
    if (m_exynosCameraParameters->getTpuEnabledMode() == true) {
        maxBufferCount = m_exynosconfig->current->bufInfo.num_hwdis_buffers;

        /* DIS MEMORY */
        int disFormat = m_exynosCameraParameters->getHWVdisFormat();
        unsigned int bpp = 0;
        unsigned int disPlanes = 0;

        getYuvFormatInfo(disFormat, &bpp, &disPlanes);

        switch (disFormat) {
        case V4L2_PIX_FMT_YUYV:
            planeSize[0] = ALIGN_UP(bdsRect.w, TDNR_WIDTH_ALIGN) * ALIGN_UP(bdsRect.h, TDNR_HEIGHT_ALIGN) * 2;
            break;
        default:
            CLOGE("ERR(%s[%d]):unexpected VDIS format(%d). so, fail", __FUNCTION__, __LINE__, disFormat);
            return INVALID_OPERATION;
            break;
        }

        exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;

        ret = m_allocBuffers(m_hwDisBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, maxBufferCount, type, true, false);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_hwDisBufferMgr m_allocBuffers(bufferCount=%d) fail",
                __FUNCTION__, __LINE__, maxBufferCount);
            return ret;
        }

        CLOGD("DEBUG(%s[%d]):m_allocBuffers(m_hwDisBufferMgr): %d x %d, planeCount(%d), maxBufferCount(%d)",
            __FUNCTION__, __LINE__, bdsRect.w, bdsRect.h, planeCount, maxBufferCount);
    }

#ifdef SAMSUNG_LBP
    planeSize[0] = hwPreviewW * hwPreviewH;
    planeSize[1] = hwPreviewW * hwPreviewH / 2;
    planeCount  = 3;

    maxBufferCount = m_exynosCameraParameters->getHoldFrameCount();

    type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;

    ret = m_allocBuffers(m_lbpBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, maxBufferCount, type, true, true);
    if (ret < 0) {
        CLOGE("[LBP]ERR(%s[%d]):m_lbpBufferMgr m_allocBuffers(bufferCount=%d) fail",
            __FUNCTION__, __LINE__, maxBufferCount);
        return ret;
    }
#endif

    planeSize[0] = hwPreviewW * hwPreviewH;
    planeSize[1] = hwPreviewW * hwPreviewH / 2;
    planeCount  = 3;
    if(m_exynosCameraParameters->increaseMaxBufferOfPreview()){
        maxBufferCount = m_exynosCameraParameters->getPreviewBufferCount();
    } else {
        maxBufferCount = m_exynosconfig->current->bufInfo.num_preview_buffers;
    }

    bool needMmap = false;
    if (m_previewWindow == NULL)
        needMmap = true;

    ret = m_allocBuffers(m_scpBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, true, needMmap);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_scpBufferMgr m_allocBuffers(bufferCount=%d) fail",
            __FUNCTION__, __LINE__, maxBufferCount);
        return ret;
    }

    if (m_exynosCameraParameters->getSupportedZoomPreviewWIthScaler()) {
        planeSize[0] = ALIGN_UP(hwPreviewW, GSCALER_IMG_ALIGN) * ALIGN_UP(hwPreviewH, GSCALER_IMG_ALIGN);
        planeSize[1] = ALIGN_UP(hwPreviewW, GSCALER_IMG_ALIGN) * ALIGN_UP(hwPreviewH, GSCALER_IMG_ALIGN) / 2;
        planeCount  = 3;
        if(m_exynosCameraParameters->increaseMaxBufferOfPreview()){
            maxBufferCount = m_exynosCameraParameters->getPreviewBufferCount();
        } else {
            maxBufferCount = m_exynosconfig->current->bufInfo.num_preview_buffers;
        }

        bool needMmap = false;
        if (m_previewWindow == NULL)
            needMmap = true;

        ret = m_allocBuffers(m_zoomScalerBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, true, needMmap);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_scpBufferMgr m_allocBuffers(bufferCount=%d) fail",
                __FUNCTION__, __LINE__, maxBufferCount);
            return ret;
        }
    }
#ifdef SUPPORT_SW_VDIS
    if(m_swVDIS_Mode) {
        VDIS_LOG("VDIS_HAL: m_allocBuffers(m_scpBufferMgr): %d x %d", hwPreviewW, hwPreviewH);

        m_swVDIS_AdjustPreviewSize(&hwPreviewW, &hwPreviewH);

        planeSize[0] = ROUND_UP(hwPreviewW, CAMERA_MAGIC_ALIGN) * ROUND_UP(hwPreviewH, CAMERA_MAGIC_ALIGN) + MFC_7X_BUFFER_OFFSET;
        planeSize[1] = ROUND_UP(hwPreviewW, CAMERA_MAGIC_ALIGN) * ROUND_UP(hwPreviewH / 2, CAMERA_MAGIC_ALIGN) +  + MFC_7X_BUFFER_OFFSET;
        planeCount = 3;
        maxBufferCount = NUM_VDIS_BUFFERS;
        exynos_camera_buffer_type_t swVDIS_type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
        buffer_manager_allocation_mode_t swVDIS_allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;

        VDIS_LOG("VDIS_HAL: m_allocBuffers(m_swVDIS_BufferMgr): %d x %d", hwPreviewW, hwPreviewH);

        ret = m_allocBuffers(m_swVDIS_BufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, maxBufferCount, swVDIS_type, swVDIS_allocMode, true, true);

        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_swVDIS_BufferMgr m_allocBuffers(bufferCount=%d) fail",
                __FUNCTION__, __LINE__, maxBufferCount);
            return ret;
        }

        m_exynosCameraParameters->getHwPreviewSize(&hwPreviewW, &hwPreviewH);
    }
#endif /*SUPPORT_SW_VDIS*/

#ifdef USE_BUFFER_WITH_STRIDE
    int stride = m_scpBufferMgr->getBufStride();
    if (stride != hwPreviewW) {
        CLOGI("INFO(%s[%d]):hwPreviewW(%d), stride(%d)", __FUNCTION__, __LINE__, hwPreviewW, stride);
        if (stride == 0) {
            /* If the SCP buffer manager is not instance of GrallocExynosCameraBufferManager
               (In case of setPreviewWindow(null) is called), return value of setHwPreviewStride()
               will be zero. If this value is passed as SCP width to firmware, firmware will
               generate PABORT error. */
            CLOGW("WARN(%s[%d]):HACK: Invalid stride(%d). It will be replaced as hwPreviewW(%d) value.",
                __FUNCTION__, __LINE__, stride, hwPreviewW);
            stride = hwPreviewW;
        }
    }
#endif

    m_exynosCameraParameters->setHwPreviewStride(stride);

    if (m_exynosCameraParameters->isSccCapture() == true) {
        m_exynosCameraParameters->getHwPictureSize(&hwPictureW, &hwPictureH);
        CLOGI("(%s):HW Picture width x height = %dx%d", __FUNCTION__, hwPictureW, hwPictureH);
        if (SCC_OUTPUT_COLOR_FMT == V4L2_PIX_FMT_NV21M) {
            planeSize[0] = ALIGN_UP(hwPictureW, GSCALER_IMG_ALIGN) * ALIGN_UP(hwPictureH, GSCALER_IMG_ALIGN);
            planeSize[1] = ALIGN_UP(hwPictureW, GSCALER_IMG_ALIGN) * ALIGN_UP(hwPictureH, GSCALER_IMG_ALIGN) / 2;
            planeCount  = 3;
        } else if (SCC_OUTPUT_COLOR_FMT == V4L2_PIX_FMT_NV21) {
            planeSize[0] = ALIGN_UP(hwPictureW, GSCALER_IMG_ALIGN) * ALIGN_UP(hwPictureH, GSCALER_IMG_ALIGN) * 3 / 2;
            planeCount  = 2;
        } else {
        planeSize[0] = ALIGN_UP(hwPictureW, GSCALER_IMG_ALIGN) * ALIGN_UP(hwPictureH, GSCALER_IMG_ALIGN) * 2;
        planeCount  = 2;
        }
        /* TO DO : make same num of buffers */
        if (m_exynosCameraParameters->isFlite3aaOtf() == true)
            maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;
        else
            maxBufferCount = m_exynosconfig->current->bufInfo.num_bayer_buffers;

        ret = m_allocBuffers(m_sccBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, true, false);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_sccBufferMgr m_allocBuffers(bufferCount=%d) fail",
                __FUNCTION__, __LINE__, maxBufferCount);
            return ret;
        }
    }

    CLOGI("INFO(%s[%d]):alloc buffer done - camera ID: %d",
        __FUNCTION__, __LINE__, m_cameraId);

    return NO_ERROR;
}

status_t ExynosCamera::m_setReprocessingBuffer(void)
{
    int ret = 0;
    int pictureMaxW, pictureMaxH;
    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    int planeCount  = 0;
    int bufferCount = 0;
    int minBufferCount = NUM_REPROCESSING_BUFFERS;
    int maxBufferCount = NUM_PICTURE_BUFFERS;
    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
    buffer_manager_allocation_mode_t allocMode = BUFFER_MANAGER_ALLOCATION_ONDEMAND;

    m_exynosCameraParameters->getMaxPictureSize(&pictureMaxW, &pictureMaxH);
    CLOGI("(%s):HW Picture MAX width x height = %dx%d", __FUNCTION__, pictureMaxW, pictureMaxH);

    /* for reprocessing */
#ifdef CAMERA_PACKED_BAYER_ENABLE
#ifdef DEBUG_RAWDUMP
    if (m_exynosCameraParameters->checkBayerDumpEnable()) {
        bytesPerLine[0] = pictureMaxW * 2;
        planeSize[0] = pictureMaxW * pictureMaxH * 2;
    } else
#endif /* DEBUG_RAWDUMP */
    {
        bytesPerLine[0] = ROUND_UP((pictureMaxW * 3 / 2), 16);
        planeSize[0] = bytesPerLine[0] * pictureMaxH;
    }
#else
    if (m_exynosCameraParameters->getBayerFormat() == V4L2_PIX_FMT_SBGGR12) {
        bytesPerLine[0] = ROUND_UP((pictureMaxW * 3 / 2), 16);
        planeSize[0] = bytesPerLine[0] * pictureMaxH;
    } else {
        bytesPerLine[0] = pictureMaxW * 2;
        planeSize[0] = pictureMaxW * pictureMaxH * 2;
    }
#endif
    planeCount  = 2;
    bufferCount = NUM_REPROCESSING_BUFFERS;

    if (m_exynosCameraParameters->getHighResolutionCallbackMode() == true) {
        /* ISP Reprocessing Buffer realloc for high resolution callback */
        minBufferCount = 2;
    }

    ret = m_allocBuffers(m_ispReprocessingBufferMgr, planeCount, planeSize, bytesPerLine, minBufferCount, maxBufferCount, type, allocMode, true, false);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_ispReprocessingBufferMgr m_allocBuffers(minBufferCount=%d/maxBufferCount=%d) fail",
            __FUNCTION__, __LINE__, minBufferCount, maxBufferCount);
        return ret;
    }

    return NO_ERROR;
}

status_t ExynosCamera::m_setPreviewCallbackBuffer(void)
{
    int ret = 0;
    int previewW = 0, previewH = 0;
    int previewFormat = 0;
    m_exynosCameraParameters->getPreviewSize(&previewW, &previewH);
    previewFormat = m_exynosCameraParameters->getPreviewFormat();

    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};

    int planeCount  = getYuvPlaneCount(previewFormat);
    int bufferCount = 1;
    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;

    if (m_previewCallbackBufferMgr == NULL) {
        CLOGE("ERR(%s[%d]): m_previewCallbackBufferMgr is NULL", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    if (m_previewCallbackBufferMgr->isAllocated() == true) {
        if (m_exynosCameraParameters->getRestartPreview() == true) {
            CLOGD("DEBUG(%s[%d]): preview size is changed, realloc buffer", __FUNCTION__, __LINE__);
            m_previewCallbackBufferMgr->deinit();
        } else {
            return NO_ERROR;
        }
    }

    ret = getYuvPlaneSize(previewFormat, planeSize, previewW, previewH);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]): BAD value, format(%x), size(%dx%d)",
            __FUNCTION__, __LINE__, previewFormat, previewW, previewH);
        return ret;
    }

    ret = m_allocBuffers(m_previewCallbackBufferMgr, planeCount, planeSize, bytesPerLine, bufferCount, bufferCount, type, false, false);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_previewCallbackBufferMgr m_allocBuffers(bufferCount=%d) fail",
            __FUNCTION__, __LINE__, bufferCount);
        return ret;
    }

    return NO_ERROR;
}

bool ExynosCamera::m_startPictureBufferThreadFunc(void)
{
    int ret = 0;

    ret = m_setPictureBuffer();
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_setPictureBuffer failed", __FUNCTION__, __LINE__);

        /* TODO: Need release buffers and error exit */

        return false;
    }

    return false;
}

status_t ExynosCamera::m_setPictureBuffer(void)
{
    int ret = 0;
    unsigned int planeSize[3] = {0};
    unsigned int bytesPerLine[3] = {0};
    int pictureW = 0, pictureH = 0, pictureFormat = 0;
    int planeCount = 0;
    int minBufferCount = 1;
    int maxBufferCount = 1;
    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
    buffer_manager_allocation_mode_t allocMode = BUFFER_MANAGER_ALLOCATION_ONDEMAND;
    int sensorMaxW, sensorMaxH;

    m_exynosCameraParameters->getMaxPictureSize(&pictureW, &pictureH);
    pictureFormat = m_exynosCameraParameters->getPictureFormat();

    m_exynosCameraParameters->getMaxSensorSize(&sensorMaxW, &sensorMaxH);
    CLOGI("(%s):Sensor MAX width x height = %dx%d", __FUNCTION__, sensorMaxW, sensorMaxH);

#ifdef SAMSUNG_DNG
    if(m_exynosCameraParameters->getDNGCaptureModeOn()) {
        type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;
        bytesPerLine[0] = sensorMaxW * 2;
        planeSize[0] = sensorMaxW * sensorMaxH * 2 + DNG_HEADER_LIMIT_SIZE;
        planeCount  = 2;
        maxBufferCount = 4;

        ret = m_allocBuffers(m_fliteBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, maxBufferCount, type, true, true);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):bayerBuffer m_allocBuffers(bufferCount=%d) fail",
                __FUNCTION__, __LINE__, maxBufferCount);
            return ret;
        }
    }
#endif

    if ((needGSCForCapture(getCameraId()) == true)
#if 0 // SAMSUNG_MAGICSHOT
        || ((m_exynosCameraParameters->isReprocessing() == true) && (m_exynosCameraParameters->getShotMode() == SHOT_MODE_MAGIC))
#endif
        ) {
#if 0 // SAMSUNG_MAGICSHOT_OLD
        if ((m_exynosCameraParameters->isReprocessing() == true) && (m_exynosCameraParameters->getShotMode() == SHOT_MODE_MAGIC)) {
            int previewW = 0, previewH = 0;
            m_exynosCameraParameters->getPreviewSize(&previewW, &previewH);
            planeSize[0] = previewW * previewH * 1.5;
            planeCount = 1;
        } else
#endif
        {
            if (JPEG_INPUT_COLOR_FMT == V4L2_PIX_FMT_NV21M) {
                planeSize[0] = ALIGN_UP(pictureW, GSCALER_IMG_ALIGN) * ALIGN_UP(pictureH, GSCALER_IMG_ALIGN);
                planeSize[1] = ALIGN_UP(pictureW, GSCALER_IMG_ALIGN) * ALIGN_UP(pictureH, GSCALER_IMG_ALIGN) / 2;
                planeCount = 2;
            } else if (JPEG_INPUT_COLOR_FMT == V4L2_PIX_FMT_NV21) {
                planeSize[0] = ALIGN_UP(pictureW, GSCALER_IMG_ALIGN) * ALIGN_UP(pictureH, GSCALER_IMG_ALIGN) * 3 / 2;
                planeCount = 1;
            } else {
                planeSize[0] = ALIGN_UP(pictureW, GSCALER_IMG_ALIGN) * ALIGN_UP(pictureH, GSCALER_IMG_ALIGN) * 2;
                planeCount = 1;
            }
        }
        minBufferCount = 1;
#ifdef SAMSUNG_LLS_DEBLUR
        maxBufferCount = MAX_LD_CAPTURE_COUNT;
#else
        maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;
#endif

        // Pre-allocate certain amount of buffers enough to fed into 3 JPEG save threads.
        if (m_exynosCameraParameters->getSeriesShotCount() > 0)
            minBufferCount = NUM_BURST_GSC_JPEG_INIT_BUFFER;

#if !defined(SAMSUNG_DEBLUR) && !defined(SAMSUNG_LLS_DEBLUR)
        type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
        ret = m_allocBuffers(m_gscBufferMgr, planeCount, planeSize,
                    bytesPerLine, minBufferCount, maxBufferCount, type, allocMode, false, false);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_gscBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
                __FUNCTION__, __LINE__, minBufferCount, maxBufferCount);
            return ret;
        }
#else
        type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;
        ret = m_allocBuffers(m_gscBufferMgr, planeCount, planeSize,
                    bytesPerLine, minBufferCount, maxBufferCount, type, allocMode, false, true);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_gscBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
                __FUNCTION__, __LINE__, minBufferCount, maxBufferCount);
            return ret;
        }
#endif
    }

    if (needGSCForCapture(getCameraId()) == true) {
        int f_previewW = 0, f_previewH = 0;
        m_exynosCameraParameters->getPreviewSize(&f_previewW, &f_previewH);
        planeSize[0] = f_previewW * f_previewH * 1.5;
        planeCount = 1;
#if defined (SAMSUNG_BD) || defined (SAMSUNG_DEBLUR)
        /* Change the MagicGSCBuffer type to mmap & cached for the performance enhancement*/
        type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;
        ret = m_allocBuffers(m_postPictureGscBufferMgr, planeCount, planeSize,
                    bytesPerLine, minBufferCount, maxBufferCount, type, allocMode, false, true);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_gscBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
                __FUNCTION__, __LINE__, minBufferCount, maxBufferCount);
            return ret;
        }
#else
        type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
        ret = m_allocBuffers(m_postPictureGscBufferMgr, planeCount, planeSize,
                    bytesPerLine, minBufferCount, maxBufferCount, type, allocMode, false, false);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_gscBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
                __FUNCTION__, __LINE__, minBufferCount, maxBufferCount);
            return ret;
        }
#endif
    }

    if(m_exynosCameraParameters->getSamsungCamera()) {
        int thumbnailW = 0, thumbnailH = 0;
        m_exynosCameraParameters->getThumbnailSize(&thumbnailW, &thumbnailH);
        if (thumbnailW > 0 && thumbnailH > 0) {
            planeSize[0] = FRAME_SIZE(HAL_PIXEL_FORMAT_RGBA_8888, thumbnailW, thumbnailH);
            planeCount  = 1;
            maxBufferCount = NUM_THUMBNAIL_POSTVIEW_BUFFERS;
            type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;

            ret = m_allocBuffers(m_thumbnailGscBufferMgr, planeCount, planeSize,
                        bytesPerLine, maxBufferCount, maxBufferCount, type, allocMode, false, true);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):m_gscBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
                    __FUNCTION__, __LINE__, maxBufferCount, maxBufferCount);
                return ret;
            }
        }
    }

    if( m_hdrEnabled == false ) {
        if (JPEG_INPUT_COLOR_FMT == V4L2_PIX_FMT_NV21M) {
            planeSize[0] = pictureW * pictureH * 3 / 2;
            planeCount = 2;
        } else if (JPEG_INPUT_COLOR_FMT == V4L2_PIX_FMT_NV21) {
            planeSize[0] = pictureW * pictureH * 3 / 2;
            planeCount = 1;
        } else {
            planeSize[0] = FRAME_SIZE(V4L2_PIX_2_HAL_PIXEL_FORMAT(pictureFormat), pictureW, pictureH);
            planeCount = 1;
        }
        minBufferCount = 1;
        maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;

        type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;
        CLOGD("DEBUG(%s[%d]): jpegBuffer picture(%dx%d) size(%d)", __FUNCTION__, __LINE__, pictureW, pictureH, planeSize[0]);

        // Same with above GSC buffers
        if (m_exynosCameraParameters->getSeriesShotCount() > 0)
            minBufferCount = NUM_BURST_GSC_JPEG_INIT_BUFFER;

#ifdef RESERVED_MEMORY_ENABLE
        if (getCameraId() == CAMERA_ID_BACK
#ifdef RESERVED_MEMORY_20M_WORKAROUND
        && m_cameraSensorId != SENSOR_NAME_S5K2T2
#endif
         ) {
#ifdef CAMERA_PACKED_BAYER_ENABLE
            type = EXYNOS_CAMERA_BUFFER_ION_CACHED_RESERVED_TYPE;
#else
            /* More reserved memory is needed when using unpack bayer */
            type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;
#endif
            if (m_exynosCameraParameters->getUHDRecordingMode() == true) {
                m_jpegBufferMgr->setContigBufCount(RESERVED_NUM_JPEG_BUFFERS_ON_UHD);
            } else {
                m_jpegBufferMgr->setContigBufCount(RESERVED_NUM_JPEG_BUFFERS);

                /* alloc at once */
                minBufferCount = NUM_BURST_GSC_JPEG_INIT_BUFFER;
            }
        }
#endif

        ret = m_allocBuffers(m_jpegBufferMgr, planeCount, planeSize, bytesPerLine,
                minBufferCount, maxBufferCount, type, allocMode, false, true);
        if (ret < 0) {
            if (getCameraId() == CAMERA_ID_FRONT && type == EXYNOS_CAMERA_BUFFER_ION_CACHED_RESERVED_TYPE) {
                CLOGD("DEBUG(%s[%d]):can't alloc reserved memory of JpegBuffers(%d). so, change memory type",
                                __FUNCTION__, __LINE__, type);

                ret = m_allocBuffers(m_jpegBufferMgr,
                                    planeCount,
                                    planeSize,
                                    bytesPerLine,
                                    minBufferCount,
                                    maxBufferCount,
                                    EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE,
                                    allocMode,
                                    false,
                                    true);
                if (ret < 0) {
                    CLOGE("ERR(%s:%d):jpegSrcHeapBuffer m_allocBuffers(bufferCount=%d) fail",
                            __FUNCTION__, __LINE__, NUM_REPROCESSING_BUFFERS);
                }
            } else {
                CLOGE("ERR(%s:%d):jpegSrcHeapBuffer m_allocBuffers(bufferCount=%d) fail",
                        __FUNCTION__, __LINE__, NUM_REPROCESSING_BUFFERS);
            }
        }
    }

    return ret;
}

status_t ExynosCamera::m_releaseBuffers(void)
{
    CLOGI("INFO(%s[%d]):release buffer", __FUNCTION__, __LINE__);
    int ret = 0;

    if (m_bayerBufferMgr != NULL) {
        m_bayerBufferMgr->deinit();
    }
#ifdef SAMSUNG_DNG
    if (m_fliteBufferMgr != NULL) {
        m_fliteBufferMgr->deinit();
    }
#endif
    if (m_3aaBufferMgr != NULL) {
        m_3aaBufferMgr->deinit();
    }
    if (m_ispBufferMgr != NULL) {
        m_ispBufferMgr->deinit();
    }
    if (m_hwDisBufferMgr != NULL) {
        m_hwDisBufferMgr->deinit();
    }
    if (m_scpBufferMgr != NULL) {
        m_scpBufferMgr->deinit();
    }
    if (m_ispReprocessingBufferMgr != NULL) {
        m_ispReprocessingBufferMgr->deinit();
    }
    if (m_sccReprocessingBufferMgr != NULL) {
        m_sccReprocessingBufferMgr->deinit();
    }
    if (m_sccBufferMgr != NULL) {
        m_sccBufferMgr->deinit();
    }
    if (m_gscBufferMgr != NULL) {
        m_gscBufferMgr->deinit();
    }

    if (m_postPictureGscBufferMgr != NULL) {
        m_postPictureGscBufferMgr->deinit();
    }

    if (m_thumbnailGscBufferMgr != NULL) {
        m_thumbnailGscBufferMgr->deinit();
    }

#ifdef SAMSUNG_LBP
    if (m_lbpBufferMgr != NULL) {
        m_lbpBufferMgr->deinit();
    }
#endif

    if (m_jpegBufferMgr != NULL) {
        m_jpegBufferMgr->deinit();
    }
    if (m_recordingBufferMgr != NULL) {
        m_recordingBufferMgr->deinit();
    }
    if (m_previewCallbackBufferMgr != NULL) {
        m_previewCallbackBufferMgr->deinit();
    }
    if (m_highResolutionCallbackBufferMgr != NULL) {
        m_highResolutionCallbackBufferMgr->deinit();
    }
#ifdef SUPPORT_SW_VDIS
    if (m_swVDIS_BufferMgr != NULL) {
        m_swVDIS_BufferMgr->deinit();
    }
#endif /*SUPPORT_SW_VDIS*/

    CLOGI("INFO(%s[%d]):free buffer done", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCamera::m_putBuffers(ExynosCameraBufferManager *bufManager, int bufIndex)
{
    if (bufManager != NULL)
        bufManager->putBuffer(bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_NONE);

    return NO_ERROR;
}

status_t ExynosCamera::m_allocBuffers(
        ExynosCameraBufferManager *bufManager,
        int  planeCount,
        unsigned int *planeSize,
        unsigned int *bytePerLine,
        int  reqBufCount,
        bool createMetaPlane,
        bool needMmap)
{
    int ret = 0;

    ret = m_allocBuffers(
                bufManager,
                planeCount,
                planeSize,
                bytePerLine,
                reqBufCount,
                reqBufCount,
                EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE,
                BUFFER_MANAGER_ALLOCATION_ATONCE,
                createMetaPlane,
                needMmap);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_allocBuffers(reqBufCount=%d) fail",
            __FUNCTION__, __LINE__, reqBufCount);
    }

    return ret;
}

status_t ExynosCamera::m_allocBuffers(
        ExynosCameraBufferManager *bufManager,
        int  planeCount,
        unsigned int *planeSize,
        unsigned int *bytePerLine,
        int  minBufCount,
        int  maxBufCount,
        exynos_camera_buffer_type_t type,
        bool createMetaPlane,
        bool needMmap)
{
    int ret = 0;

    ret = m_allocBuffers(
                bufManager,
                planeCount,
                planeSize,
                bytePerLine,
                minBufCount,
                maxBufCount,
                type,
                BUFFER_MANAGER_ALLOCATION_ONDEMAND,
                createMetaPlane,
                needMmap);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_allocBuffers(minBufCount=%d, maxBufCount=%d, type=%d) fail",
            __FUNCTION__, __LINE__, minBufCount, maxBufCount, type);
    }

    return ret;
}

status_t ExynosCamera::m_allocBuffers(
        ExynosCameraBufferManager *bufManager,
        int  planeCount,
        unsigned int *planeSize,
        unsigned int *bytePerLine,
        int  minBufCount,
        int  maxBufCount,
        exynos_camera_buffer_type_t type,
        buffer_manager_allocation_mode_t allocMode,
        bool createMetaPlane,
        bool needMmap)
{
    int ret = 0;

    CLOGI("INFO(%s[%d]):setInfo(planeCount=%d, minBufCount=%d, maxBufCount=%d, type=%d, allocMode=%d)",
        __FUNCTION__, __LINE__, planeCount, minBufCount, maxBufCount, (int)type, (int)allocMode);

    ret = bufManager->setInfo(
                        planeCount,
                        planeSize,
                        bytePerLine,
                        0,
                        minBufCount,
                        maxBufCount,
                        type,
                        allocMode,
                        createMetaPlane,
                        needMmap);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setInfo fail", __FUNCTION__, __LINE__);
        goto func_exit;
    }

    ret = bufManager->alloc();
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):alloc fail", __FUNCTION__, __LINE__);
        goto func_exit;
    }

func_exit:

    return ret;
}

status_t ExynosCamera::m_checkThreadState(int *threadState, int *countRenew)
{
    int ret = NO_ERROR;

    if ((*threadState == ERROR_POLLING_DETECTED) || (*countRenew > ERROR_DQ_BLOCKED_COUNT)) {
        CLOGW("WRN(%s[%d]:SCP DQ Timeout! State:[%d], Duration:%d msec", __FUNCTION__, __LINE__, *threadState, (*countRenew)*(MONITOR_THREAD_INTERVAL/1000));
        ret = false;
    } else {
        CLOGV("[%s] (%d) (%d)", __FUNCTION__, __LINE__, *threadState);
        ret = NO_ERROR;
    }

    return ret;
}

status_t ExynosCamera::m_checkThreadInterval(uint32_t pipeId, uint32_t pipeInterval, int *threadState)
{
    uint64_t *threadInterval;
    int ret = NO_ERROR;

    m_previewFrameFactory->getThreadInterval(&threadInterval, pipeId);
    if (*threadInterval > pipeInterval) {
        CLOGW("WRN(%s[%d]:Pipe(%d) Thread Interval [%llu msec], State:[%d]", __FUNCTION__, __LINE__, pipeId, (unsigned long long)(*threadInterval)/1000, *threadState);
        ret = false;
    } else {
        CLOGV("Thread IntervalTime [%lld]", (unsigned long long)*threadInterval);
        CLOGV("Thread Renew state [%d]", *threadState);
        ret = NO_ERROR;
    }

    return ret;
}

bool ExynosCamera::m_monitorThreadFunc(void)
{
    CLOGV("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    int *threadState;
    struct timeval dqTime;
    uint64_t *timeInterval;
    int *countRenew;
    int camId = getCameraId();
    int ret = NO_ERROR;
    int loopCount = 0;

    int dtpStatus = 0;
    int pipeIdFlite = 0;
    int pipeIdErrorCheck = 0;

    for (loopCount = 0; loopCount < MONITOR_THREAD_INTERVAL; loopCount += (MONITOR_THREAD_INTERVAL/20)) {
        if (m_flagThreadStop == true) {
            CLOGI("INFO(%s[%d]): m_flagThreadStop(%d)", __FUNCTION__, __LINE__, m_flagThreadStop);

            return false;
        }

        usleep(MONITOR_THREAD_INTERVAL/20);
    }

    if (m_previewFrameFactory == NULL) {
        CLOGW("WARN(%s[%d]): m_previewFrameFactory is NULL. Skip monitoring.", __FUNCTION__, __LINE__);

        return false;
    }

    pipeIdFlite = PIPE_FLITE;

    if (m_exynosCameraParameters->is3aaIspOtf() == true) {
        if (m_exynosCameraParameters->getTpuEnabledMode() == true)
            pipeIdErrorCheck = PIPE_DIS;
        else
            pipeIdErrorCheck = PIPE_3AA;
    } else {
        pipeIdErrorCheck = PIPE_ISP;
    }

#ifdef MONITOR_LOG_SYNC
    uint32_t pipeIdIsp = 0;

    /* define pipe for isp node cause of sync log sctrl */
    if (m_exynosCameraParameters->isFlite3aaOtf() == true) {
        if (m_exynosCameraParameters->is3aaIspOtf() == true) {
            pipeIdIsp = PIPE_3AA;
        } else {
            pipeIdIsp = PIPE_ISP;
        }
    } else {
        pipeIdIsp = PIPE_ISP;
    }

    /* If it is not front camera in dual and sensor pipe is running, do sync log */
    if (m_previewFrameFactory->checkPipeThreadRunning(pipeIdIsp) &&
        !(getCameraId() == CAMERA_ID_FRONT && m_exynosCameraParameters->getDualMode())){
        if (!(m_syncLogDuration % MONITOR_LOG_SYNC_INTERVAL)) {
            uint32_t syncLogId = m_getSyncLogId();
            CLOGI("INFO(%s[%d]): @FIMC_IS_SYNC %d", __FUNCTION__, __LINE__, syncLogId);
            m_previewFrameFactory->syncLog(pipeIdIsp, syncLogId);
        }
        m_syncLogDuration++;
    }
#endif

    m_previewFrameFactory->getControl(V4L2_CID_IS_G_DTPSTATUS, &dtpStatus, pipeIdFlite);
    if (dtpStatus == 1) {
        CLOGD("DEBUG(%s[%d]):DTP Detected. dtpStatus(%d)", __FUNCTION__, __LINE__, dtpStatus);
        dump();

#ifdef CAMERA_GED_FEATURE
        /* in GED */
        m_notifyCb(CAMERA_MSG_ERROR, 100, 0, m_callbackCookie);
#else
        /* specifically defined */
        m_notifyCb(CAMERA_MSG_ERROR, 1002, 0, m_callbackCookie);
        /* or */
        /* android_printAssert(NULL, LOG_TAG, "killed by itself"); */
#endif

        return false;
    }

#ifdef SENSOR_OVERFLOW_CHECK
    m_previewFrameFactory->getControl(V4L2_CID_IS_G_DTPSTATUS, &dtpStatus, pipeIdFlite);
    if (dtpStatus == 1) {
        CLOGD("DEBUG(%s[%d]):DTP Detected. dtpStatus(%d)", __FUNCTION__, __LINE__, dtpStatus);
        dump();

        /* in GED */
        /* m_notifyCb(CAMERA_MSG_ERROR, 100, 0, m_callbackCookie); */
        /* specifically defined */
        /* m_notifyCb(CAMERA_MSG_ERROR, 1002, 0, m_callbackCookie); */
        /* or */
        android_printAssert(NULL, LOG_TAG, "killed by itself");

        return false;
    }
#endif

    m_previewFrameFactory->getThreadState(&threadState, pipeIdErrorCheck);
    m_previewFrameFactory->getThreadRenew(&countRenew, pipeIdErrorCheck);

    if (m_exynosCameraParameters->getSamsungCamera() && ((*threadState == ERROR_POLLING_DETECTED) || (*countRenew > ERROR_DQ_BLOCKED_COUNT))) {
        CLOGD("DEBUG(%s[%d]):ESD Detected. threadState(%d) *countRenew(%d)", __FUNCTION__, __LINE__, *threadState, *countRenew);
        dump();
#ifdef SAMSUNG_TN_FEATURE
        if (m_recordingEnabled == true
          && m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)
          && m_recordingCallbackHeap != NULL
          && m_callbackCookie != NULL) {

          CLOGD("DEBUG(%s[%d]):Timestamp callback with CAMERA_MSG_ERROR start", __FUNCTION__, __LINE__);

          m_dataCbTimestamp(
              0,
              CAMERA_MSG_ERROR | CAMERA_MSG_VIDEO_FRAME,
              m_recordingCallbackHeap,
              0,
              m_callbackCookie);

          CLOGD("DEBUG(%s[%d]):Timestamp callback with CAMERA_MSG_ERROR end", __FUNCTION__, __LINE__);
        }
#endif

#ifdef CAMERA_GED_FEATURE
        /* in GED */
        /* skip error callback */
        /* m_notifyCb(CAMERA_MSG_ERROR, 100, 0, m_callbackCookie); */
#else
        /* specifically defined */
        m_notifyCb(CAMERA_MSG_ERROR, 1001, 0, m_callbackCookie);
        /* or */
        /* android_printAssert(NULL, LOG_TAG, "killed by itself"); */
#endif

        ret = m_previewFrameFactory->setControl(V4L2_CID_IS_CAMERA_TYPE, IS_COLD_RESET, PIPE_3AA);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):PIPE_%d V4L2_CID_IS_CAMERA_TYPE fail, ret(%d)", __FUNCTION__, __LINE__, PIPE_3AA, ret);
        }

        return false;
    } else {
        CLOGV("[%s] (%d) (%d)", __FUNCTION__, __LINE__, *threadState);
    }

#if 0
    m_checkThreadState(threadState, countRenew)?:ret = false;
    m_checkThreadInterval(PIPE_SCP, WARNING_SCP_THREAD_INTERVAL, threadState)?:ret = false;

    enum pipeline pipe;

    /* check PIPE_3AA thread state & interval */
    if (m_exynosCameraParameters->isFlite3aaOtf() == true) {
        pipe = PIPE_3AA_ISP;

        m_previewFrameFactory->getThreadRenew(&countRenew, pipe);
        m_checkThreadState(threadState, countRenew)?:ret = false;

        if (ret == false) {
            dump();

            /* in GED */
            m_notifyCb(CAMERA_MSG_ERROR, 100, 0, m_callbackCookie);
            /* specifically defined */
            /* m_notifyCb(CAMERA_MSG_ERROR, 1001, 0, m_callbackCookie); */
            /* or */
            android_printAssert(NULL, LOG_TAG, "killed by itself");
        }
    } else {
        pipe = PIPE_3AA;

        m_previewFrameFactory->getThreadRenew(&countRenew, pipe);
        m_checkThreadState(threadState, countRenew)?:ret = false;

        if (ret == false) {
            dump();

            /* in GED */
            m_notifyCb(CAMERA_MSG_ERROR, 100, 0, m_callbackCookie);
            /* specifically defined */
            /* m_notifyCb(CAMERA_MSG_ERROR, 1001, 0, m_callbackCookie); */
            /* or */
            android_printAssert(NULL, LOG_TAG, "killed by itself");
        }
    }

    m_checkThreadInterval(pipe, WARNING_3AA_THREAD_INTERVAL, threadState)?:ret = false;

    if (m_callbackState == 0) {
        m_callbackStateOld = 0;
        m_callbackState = 0;
        m_callbackMonitorCount = 0;
    } else {
        if (m_callbackStateOld != m_callbackState) {
            m_callbackStateOld = m_callbackState;
            CLOGD("INFO(%s[%d]):callback state is updated (0x%x)", __FUNCTION__, __LINE__, m_callbackStateOld);
        } else {
            if ((m_callbackStateOld & m_callbackState) != 0)
                CLOGE("ERR(%s[%d]):callback is blocked (0x%x), Duration:%d msec", __FUNCTION__, __LINE__, m_callbackState, m_callbackMonitorCount*(MONITOR_THREAD_INTERVAL/1000));
        }
    }
#endif

    gettimeofday(&dqTime, NULL);
    m_previewFrameFactory->getThreadInterval(&timeInterval, pipeIdErrorCheck);

    CLOGV("Thread IntervalTime [%lld]", (long long)*timeInterval);
    CLOGV("Thread Renew Count [%d]", *countRenew);

    m_previewFrameFactory->incThreadRenew(pipeIdErrorCheck);

    return true;
}

#ifdef MONITOR_LOG_SYNC
uint32_t ExynosCamera::m_getSyncLogId(void)
{
    return ++cameraSyncLogId;
}
#endif

bool ExynosCamera::m_autoFocusResetNotify(__unused int focusMode)
{
#ifdef SAMSUNG_TN_FEATURE
    /* show restart */
    CLOGD("DEBUG(%s):CAMERA_MSG_FOCUS(%d) mode(%d)", __func__, FOCUS_RESULT_RESTART, focusMode);
    m_notifyCb(CAMERA_MSG_FOCUS, FOCUS_RESULT_RESTART, 0, m_callbackCookie);

    /* show focusing */
    CLOGD("DEBUG(%s):CAMERA_MSG_FOCUS(%d) mode(%d)", __func__, FOCUS_RESULT_FOCUSING, focusMode);
    m_notifyCb(CAMERA_MSG_FOCUS, FOCUS_RESULT_FOCUSING, 0, m_callbackCookie);
#endif
    return true;
}

bool ExynosCamera::m_autoFocusThreadFunc(void)
{
    CLOGI("INFO(%s[%d]): -IN-", __FUNCTION__, __LINE__);

    bool afResult = false;
    int focusMode = 0;
    ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();

    /* block until we're told to start.  we don't want to use
     * a restartable thread and requestExitAndWait() in cancelAutoFocus()
     * because it would cause deadlock between our callbacks and the
     * caller of cancelAutoFocus() which both want to grab the same lock
     * in CameraServices layer.
     */

    if (getCameraId() == CAMERA_ID_FRONT) {
        if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_FOCUS)) {
            if (m_notifyCb != NULL) {
                CLOGD("DEBUG(%s):Do not support autoFocus in front camera.", __FUNCTION__);
                m_notifyCb(CAMERA_MSG_FOCUS, true, 0, m_callbackCookie);
            } else {
                CLOGD("DEBUG(%s):m_notifyCb is NULL!", __FUNCTION__);
            }
        } else {
            CLOGD("DEBUG(%s):autoFocus msg disabled !!", __FUNCTION__);
        }
        return false;
    }

    if (m_autoFocusType == AUTO_FOCUS_SERVICE) {
        focusMode = m_exynosCameraParameters->getFocusMode();
    } else if (m_autoFocusType == AUTO_FOCUS_HAL) {
        focusMode = FOCUS_MODE_AUTO;

        if (m_notifyCb != NULL) {
            m_autoFocusResetNotify(focusMode);
        }
    }

    m_autoFocusLock.lock();
    /* check early exit request */
    if (m_exitAutoFocusThread == true) {
        CLOGD("DEBUG(%s):exiting on request", __FUNCTION__);
        goto done;
    }

    m_autoFocusRunning = true;

    if (m_autoFocusRunning == true) {
        afResult = m_exynosCameraActivityControl->autoFocus(focusMode, m_autoFocusType,
                            m_flagStartFaceDetection, m_frameMetadata.number_of_faces);
        if (afResult == true)
            CLOGV("DEBUG(%s):autoFocus Success!!", __FUNCTION__);
        else
            CLOGV("DEBUG(%s):autoFocus Fail !!", __FUNCTION__);
    } else {
        CLOGV("DEBUG(%s):autoFocus canceled !!", __FUNCTION__);
    }

    /*
     * CAMERA_MSG_FOCUS only takes a bool.  true for
     * finished and false for failure.
     * If cancelAutofocus() called, no callback.
     */
    if ((m_autoFocusRunning == true) &&
        m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_FOCUS)) {

        if (m_notifyCb != NULL) {
            int afFinalResult = (int)afResult;
#ifdef SAMSUNG_TN_FEATURE
            /* if inactive detected, tell it */
            if (m_exynosCameraParameters->getSamsungCamera() == true && focusMode == FOCUS_MODE_CONTINUOUS_PICTURE) {
                if (m_exynosCameraActivityControl->getCAFResult() == FOCUS_RESULT_CANCEL) {
                    afFinalResult = FOCUS_RESULT_CANCEL;
                }
            }
#endif
            CLOGD("DEBUG(%s):CAMERA_MSG_FOCUS(%d) mode(%d)", __FUNCTION__, afFinalResult, focusMode);
            m_autoFocusLock.unlock();
            m_notifyCb(CAMERA_MSG_FOCUS, afFinalResult, 0, m_callbackCookie);
            m_autoFocusLock.lock();
        } else {
            CLOGD("DEBUG(%s):m_notifyCb is NULL mode(%d)", __FUNCTION__, focusMode);
        }
    } else {
        CLOGV("DEBUG(%s):autoFocus canceled, no callback !!", __FUNCTION__);
    }

    autoFocusMgr->displayAFStatus();

    m_autoFocusRunning = false;

    CLOGV("DEBUG(%s):exiting with no error", __FUNCTION__);

#ifdef SAMSUNG_TN_FEATURE
    if (focusMode == FOCUS_MODE_CONTINUOUS_PICTURE || focusMode == FOCUS_MODE_CONTINUOUS_VIDEO) {
        bool prev_afInMotion = autoFocusMgr->getAfInMotionResult();
        bool afInMotion = false;

        autoFocusMgr->setAfInMotionResult(afInMotion);

        if (m_notifyCb != NULL && m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_FOCUS_MOVE)
            && (prev_afInMotion != afInMotion)) {
            CLOGD("DEBUG(%s):CAMERA_MSG_FOCUS_MOVE(%d) mode(%d)",
                __FUNCTION__, afInMotion, m_exynosCameraParameters->getFocusMode());
            m_autoFocusLock.unlock();
            m_notifyCb(CAMERA_MSG_FOCUS_MOVE, afInMotion, 0, m_callbackCookie);
            m_autoFocusLock.lock();
        }
    }
#endif
done:
    m_autoFocusLock.unlock();

    CLOGI("DEBUG(%s):end", __FUNCTION__);

    return false;
}

bool ExynosCamera::m_autoFocusContinousThreadFunc(void)
{
    int ret = 0;
    int index = 0;
    uint32_t frameCnt = 0;
    uint32_t count = 0;

    ret = m_autoFocusContinousQ.waitAndPopProcessQ(&frameCnt);
    if (m_flagThreadStop == true) {
        CLOGI("INFO(%s[%d]):m_flagThreadStop(%d)", __FUNCTION__, __LINE__, m_flagThreadStop);
        return false;
    }
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGW("WARN(%s):wait timeout", __FUNCTION__);
        } else {
            CLOGE("ERR(%s):wait and pop fail, ret(%d)", __FUNCTION__, ret);
            /* TODO: doing exception handling */
        }
        return true;
    }

    count = m_autoFocusContinousQ.getSizeOfProcessQ();
    if( count >= MAX_FOCUSCONTINUS_THREADQ_SIZE ) {
        for( uint32_t i = 0 ; i < count ; i++) {
            m_autoFocusContinousQ.popProcessQ(&frameCnt);
        }
        CLOGD("DEBUG(%s[%d]):m_autoFocusContinousQ  skipped QSize(%d) frame(%d)", __FUNCTION__, __LINE__,  count, frameCnt);
    }

    if(m_autoFocusRunning) {
        return true;
    }

#ifdef SAMSUNG_TN_FEATURE
    /* Continuous Auto-focus */
    if(m_exynosCameraParameters->getSamsungCamera() == true) {
        if (m_exynosCameraParameters->getFocusMode() == FOCUS_MODE_CONTINUOUS_PICTURE) {
            ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
            int afstatus = FOCUS_RESULT_FAIL;
            static int afResult = FOCUS_RESULT_SUCCESS;
            int prev_afstatus = afResult;
            afstatus = m_exynosCameraActivityControl->getCAFResult();
            afResult = afstatus;

            if (afstatus == FOCUS_RESULT_FOCUSING &&
                (prev_afstatus == FOCUS_RESULT_FAIL || prev_afstatus == FOCUS_RESULT_SUCCESS)) {
                afResult = FOCUS_RESULT_RESTART;
            }

            if (m_notifyCb != NULL && m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_FOCUS)
                && (prev_afstatus != afstatus)) {
                CLOGD("DEBUG(%s):CAMERA_MSG_FOCUS(%d) mode(%d)",
                    __FUNCTION__, afResult, m_exynosCameraParameters->getFocusMode());
                m_notifyCb(CAMERA_MSG_FOCUS, afResult, 0, m_callbackCookie);
                autoFocusMgr->displayAFStatus();
            }
        }
    } else {
        if (m_exynosCameraParameters->getFocusMode() == FOCUS_MODE_CONTINUOUS_PICTURE
            || m_exynosCameraParameters->getFocusMode() == FOCUS_MODE_CONTINUOUS_VIDEO) {
            ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
            bool prev_afInMotion = autoFocusMgr->getAfInMotionResult();
            bool afInMotion = false;
            switch (m_exynosCameraActivityControl->getCAFResult()) {
                case FOCUS_RESULT_FAIL: // AA_AFSTATE_PASSIVE_UNFOCUSED
                case FOCUS_RESULT_SUCCESS: // AA_AFSTATE_PASSIVE_FOCUSED
                    afInMotion = false;
                    break;
                case FOCUS_RESULT_FOCUSING: // AA_AFSTATE_PASSIVE_SCAN, AA_AFSTATE_ACTIVE_SCAN
                    afInMotion = true;
                    break;
            }
            autoFocusMgr->setAfInMotionResult(afInMotion);

            if (m_notifyCb != NULL && m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_FOCUS_MOVE)
                && (prev_afInMotion != afInMotion)) {
                m_notifyCb(CAMERA_MSG_FOCUS_MOVE, afInMotion, 0, m_callbackCookie);
            }
        }
    }
#endif
    return true;
}

status_t ExynosCamera::dump(__unused int fd) const
{
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

void ExynosCamera::dump()
{
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    m_printExynosCameraInfo(__FUNCTION__);

    if (m_previewFrameFactory != NULL)
        m_previewFrameFactory->dump();

    if (m_bayerBufferMgr != NULL)
        m_bayerBufferMgr->dump();
#ifdef SAMSUNG_DNG
    if (m_fliteBufferMgr != NULL)
        m_fliteBufferMgr->dump();
#endif
    if (m_3aaBufferMgr != NULL)
        m_3aaBufferMgr->dump();
    if (m_ispBufferMgr != NULL)
        m_ispBufferMgr->dump();
    if (m_hwDisBufferMgr != NULL)
        m_hwDisBufferMgr->dump();
    if (m_scpBufferMgr != NULL)
        m_scpBufferMgr->dump();

    if (m_ispReprocessingBufferMgr != NULL)
        m_ispReprocessingBufferMgr->dump();
    if (m_sccReprocessingBufferMgr != NULL)
        m_sccReprocessingBufferMgr->dump();
    if (m_sccBufferMgr != NULL)
        m_sccBufferMgr->dump();
    if (m_gscBufferMgr != NULL)
        m_gscBufferMgr->dump();

#ifdef SUPPORT_SW_VDIS
    if (m_swVDIS_BufferMgr != NULL)
        m_swVDIS_BufferMgr->dump();
#endif /*SUPPORT_SW_VDIS*/
    return;
}

status_t ExynosCamera::m_getBufferManager(uint32_t pipeId, ExynosCameraBufferManager **bufMgr, uint32_t direction)
{
    status_t ret = NO_ERROR;
    ExynosCameraBufferManager **bufMgrList[2] = {NULL};
    *bufMgr = NULL;
    int internalPipeId  = pipeId;

    /*
     * front / back is different up to scenario(3AA OTF/M2M, etc)
     * so, we don't need to distinguish front / back camera.
     * but. reprocessing must handle the separated operation
     */
    if (pipeId < PIPE_FLITE_REPROCESSING)
        internalPipeId = INDEX(pipeId);

    switch (internalPipeId) {
    case PIPE_FLITE:
#ifdef SAMSUNG_DNG
        if(m_exynosCameraParameters->getDNGCaptureModeOn()) {
            bufMgrList[0] = NULL;
            bufMgrList[1] = &m_fliteBufferMgr;
        } else
#endif
        {
            bufMgrList[0] = NULL;
            bufMgrList[1] = &m_bayerBufferMgr;
        }
        break;
    case PIPE_3AA_ISP:
        bufMgrList[0] = &m_3aaBufferMgr;
        bufMgrList[1] = &m_ispBufferMgr;
        break;
    case PIPE_3AC:
        bufMgrList[0] = NULL;
        bufMgrList[1] = &m_bayerBufferMgr;
        break;
    case PIPE_3AA:
        if (m_exynosCameraParameters->getDualMode() == true && getCameraId() == CAMERA_ID_FRONT) {
            bufMgrList[0] = &m_bayerBufferMgr;
            bufMgrList[1] = &m_sccBufferMgr;
        } else {
            bufMgrList[0] = &m_3aaBufferMgr;

            if (m_exynosCameraParameters->is3aaIspOtf() == true) {
                bufMgrList[1] = &m_hwDisBufferMgr;
            } else {
                bufMgrList[1] = &m_ispBufferMgr;
            }
        }
        break;
    case PIPE_ISP:
        bufMgrList[0] = &m_ispBufferMgr;

        if (m_exynosCameraParameters->getTpuEnabledMode() == true)
            bufMgrList[1] = &m_hwDisBufferMgr;
        else
            bufMgrList[1] = &m_scpBufferMgr;
        break;
    case PIPE_DIS:
        bufMgrList[0] = &m_hwDisBufferMgr;
        bufMgrList[1] = &m_scpBufferMgr;
        break;
    case PIPE_ISPC:
    case PIPE_SCC:
        bufMgrList[0] = NULL;
        bufMgrList[1] = &m_sccBufferMgr;
        break;
    case PIPE_SCP:
        bufMgrList[0] = NULL;
        bufMgrList[1] = &m_scpBufferMgr;
        break;
    case PIPE_GSC:
        if (m_exynosCameraParameters->getDualMode() == true && getCameraId() == CAMERA_ID_FRONT)
            bufMgrList[0] = &m_sccBufferMgr;
        else
            bufMgrList[0] = &m_scpBufferMgr;
       bufMgrList[1] = &m_scpBufferMgr;
       break;
    case PIPE_GSC_VIDEO:
        bufMgrList[0] = &m_sccBufferMgr;
        bufMgrList[1] = &m_postPictureGscBufferMgr;
        break;
    case PIPE_GSC_PICTURE:
        bufMgrList[0] = &m_sccBufferMgr;
        bufMgrList[1] = &m_gscBufferMgr;
        break;
    case PIPE_3AA_REPROCESSING:
        bufMgrList[0] = &m_bayerBufferMgr;
        if (m_exynosCameraParameters->getDualMode() == false)
            bufMgrList[1] = &m_ispReprocessingBufferMgr;
        else
            bufMgrList[1] = &m_sccReprocessingBufferMgr;
        break;
    case PIPE_ISP_REPROCESSING:
        bufMgrList[0] = &m_ispReprocessingBufferMgr;
        bufMgrList[1] = &m_sccReprocessingBufferMgr;
        break;
    case PIPE_ISPC_REPROCESSING:
    case PIPE_SCC_REPROCESSING:
        bufMgrList[0] = NULL;
        bufMgrList[1] = &m_sccReprocessingBufferMgr;
        break;
    case PIPE_GSC_REPROCESSING:
        bufMgrList[0] = &m_sccReprocessingBufferMgr;
        bufMgrList[1] = &m_gscBufferMgr;
        break;
    case PIPE_GSC_REPROCESSING2:
#ifdef SAMSUNG_DEBLUR
        if (getUseDeblurCaptureOn()) {
            bufMgrList[0] = &m_gscBufferMgr;
        } else
#endif
        {
            bufMgrList[0] = &m_sccReprocessingBufferMgr;
        }
        bufMgrList[1] = &m_postPictureGscBufferMgr;
        break;
    case PIPE_GSC_REPROCESSING3:
        bufMgrList[0] = &m_postPictureGscBufferMgr;
        bufMgrList[1] = &m_thumbnailGscBufferMgr;
        break;
    default:
        CLOGE("ERR(%s[%d]): Unknown pipeId(%d)", __FUNCTION__, __LINE__, pipeId);
        bufMgrList[0] = NULL;
        bufMgrList[1] = NULL;
        ret = BAD_VALUE;
        break;
    }

    if (bufMgrList[direction] != NULL)
        *bufMgr = *bufMgrList[direction];

    return ret;
}

uint32_t ExynosCamera::m_getBayerPipeId(void)
{
    uint32_t pipeId = 0;

    if (m_exynosCameraParameters->getUsePureBayerReprocessing() == true) {
        pipeId = PIPE_FLITE;
    } else {
        pipeId = PIPE_3AA;
    }

    return pipeId;
}

void ExynosCamera::m_debugFpsCheck(__unused uint32_t pipeId)
{
#ifdef FPS_CHECK
    uint32_t id = pipeId % DEBUG_MAX_PIPE_NUM;

    m_debugFpsCount[id]++;
    if (m_debugFpsCount[id] == 1) {
        m_debugFpsTimer[id].start();
    }
    if (m_debugFpsCount[id] == 31) {
        m_debugFpsTimer[id].stop();
        long long durationTime = m_debugFpsTimer[id].durationMsecs();
        CLOGI("INFO(%s[%d]): FPS_CHECK(id:%d), duration %lld / 30 = %lld ms. %lld fps",
        __FUNCTION__, __LINE__, pipeId, durationTime, durationTime / 30, 1000 / (durationTime / 30));
        m_debugFpsCount[id] = 0;
    }
#endif
}

status_t ExynosCamera::m_convertingStreamToShotExt(ExynosCameraBuffer *buffer, struct camera2_node_output *outputInfo)
{
/* TODO: HACK: Will be removed, this is driver's job */
    status_t ret = NO_ERROR;
    int bayerFrameCount = 0;
    camera2_shot_ext *shot_ext = NULL;
    camera2_stream *shot_stream = NULL;

    shot_stream = (struct camera2_stream *)buffer->addr[buffer->planeCount-1];
    bayerFrameCount = shot_stream->fcount;
    outputInfo->cropRegion[0] = shot_stream->output_crop_region[0];
    outputInfo->cropRegion[1] = shot_stream->output_crop_region[1];
    outputInfo->cropRegion[2] = shot_stream->output_crop_region[2];
    outputInfo->cropRegion[3] = shot_stream->output_crop_region[3];

    memset(buffer->addr[buffer->planeCount-1], 0x0, sizeof(struct camera2_shot_ext));

    shot_ext = (struct camera2_shot_ext *)buffer->addr[buffer->planeCount-1];
    shot_ext->shot.dm.request.frameCount = bayerFrameCount;

    return ret;
}

#ifdef SAMSUNG_DNG
unsigned int ExynosCamera::getDNGFcount()
{
    unsigned int shotFcount = 0;
    ExynosCameraActivitySpecialCapture *m_sCaptureMgr = NULL;
    ExynosCameraActivityFlash *m_flashMgr = m_exynosCameraActivityControl->getFlashMgr();
    m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();

    CLOGV("[DNG](%s[%d]):getOISCaptureModeOn(%d) getOISCaptureFcount(%d)",
        __FUNCTION__, __LINE__, m_exynosCameraParameters->getOISCaptureModeOn(), m_sCaptureMgr->getOISCaptureFcount());

    if (m_sCaptureMgr->getOISCaptureFcount() > 0) {
        shotFcount = m_sCaptureMgr->getOISCaptureFcount() + OISCAPTURE_DELAY;
    } else if (m_flashMgr->getNeedCaptureFlash() == true && m_flashMgr->getShotFcount()) {
        shotFcount = m_flashMgr->getShotFcount() + 1;
    } else if (m_exynosCameraParameters->getCaptureExposureTime() > CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT) {
        shotFcount = m_dngFrameNumberForManualExposure;
    }

    return shotFcount;
}

bool ExynosCamera::m_DNGCaptureThreadFunc(void)
{
    ExynosCameraBuffer dngBuffer[3];
    ExynosCameraBuffer *saveDngBuffer[3];
    ExynosCameraBuffer *tempDngBuffer;
    int loop = false;
    int ret = 0;
    int count;
    int saveCount;
    unsigned int fliteFcount = 0;
    camera2_shot_ext *shot_ext = NULL;
    ExynosRect srcRect, dstRect;

    m_exynosCameraParameters->getPictureBayerCropSize(&srcRect, &dstRect);

    for (int i = 0; i < 3; i++) {
        dngBuffer[i].index = -2;
        saveDngBuffer[i] = &dngBuffer[i];
    }
    saveCount = 0;
    tempDngBuffer = NULL;

    CLOGD("[DNG](%s[%d]):-- IN --", __FUNCTION__, __LINE__);

DNG_RETRY:
    if (m_exynosCameraParameters->getCaptureExposureTime() != 0 && m_stopLongExposure) {
        CLOGD("[DNG](%s[%d]):flite emergency stop", __FUNCTION__, __LINE__);
        m_longExposureEnds = true;
        m_searchDNGFrame = false;
        m_pictureEnabled = false;
        CLOGD("[DNG](%s[%d]):request flite stop", __FUNCTION__, __LINE__);
        m_previewFrameFactory->setRequestFLITE(false);
        goto CLEAN;
    }
    tempDngBuffer = saveDngBuffer[saveCount];

    /* wait flite */
    CLOGV("INFO(%s[%d]):wait flite output", __FUNCTION__, __LINE__);
    ret = m_dngCaptureQ->waitAndPopProcessQ(tempDngBuffer);
    CLOGD("[DNG](%s[%d]):flite output done pipe", __FUNCTION__, __LINE__);
    if (ret < 0) {
        CLOGE("ERR(%s)(%d):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
       goto CLEAN;
    }

    saveCount++;

    /* Rawdump capture is available in pure bayer only */
    shot_ext = (camera2_shot_ext *)(tempDngBuffer->addr[1]);
    if (shot_ext != NULL)
        fliteFcount = shot_ext->shot.dm.request.frameCount;
    else
        ALOGE("ERR(%s[%d]):fliteReprocessingBuffer is null", __FUNCTION__, __LINE__);

    if (fliteFcount == 0) {
        ALOGE("ERR(%s[%d]):fliteFcount is null", __FUNCTION__, __LINE__);
        goto DNG_RETRY;
    }

    if (m_exynosCameraParameters->getCaptureExposureTime() != 0 &&
        m_exynosCameraParameters->getCheckMultiFrame() &&
        getDNGFcount() == 0) {
        if (saveCount >= 3) {
            shot_ext = (camera2_shot_ext *)(saveDngBuffer[0]->addr[1]);
            if (shot_ext->shot.dm.request.frameCount != 0) {
                ret = m_putBuffers(m_fliteBufferMgr, saveDngBuffer[0]->index);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):m_putBuffers(%d) fail", __FUNCTION__, __LINE__, saveDngBuffer[0]->index);
                    goto CLEAN;
                }
            }
            tempDngBuffer = saveDngBuffer[0];
            saveDngBuffer[0] = saveDngBuffer[1];
            saveDngBuffer[1] = saveDngBuffer[2];
            saveDngBuffer[2] = tempDngBuffer;
            saveDngBuffer[2]->index = -2;
            saveCount--;
        }
        goto DNG_RETRY;
    } else if (m_exynosCameraParameters->getCaptureExposureTime() != 0 &&
        m_exynosCameraParameters->getCheckMultiFrame() &&
        getDNGFcount() != 0 && fliteFcount < getDNGFcount()) {
        for (count = 0; count < saveCount; count++) {
            shot_ext = (camera2_shot_ext *)(saveDngBuffer[count]->addr[1]);
            if (shot_ext->shot.dm.request.frameCount != 0) {
                ret = m_putBuffers(m_fliteBufferMgr, saveDngBuffer[count]->index);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):m_putBuffers(%d) fail", __FUNCTION__, __LINE__, saveDngBuffer[count]->index);
                    goto CLEAN;
                }
            }
            saveDngBuffer[count]->index = -2;
        }
        saveCount = 0;
        goto DNG_RETRY;
    } else if (m_exynosCameraParameters->getCaptureExposureTime() != 0 &&
        m_exynosCameraParameters->getCheckMultiFrame() &&
        getDNGFcount() != 0 && fliteFcount >= getDNGFcount()) {
        for (count = 0; count < saveCount; count++) {
            shot_ext = (camera2_shot_ext *)(saveDngBuffer[count]->addr[1]);
            if (shot_ext->shot.dm.request.frameCount >= getDNGFcount()) {
                break;
            } else {
                if (shot_ext->shot.dm.request.frameCount != 0) {
                    ret = m_putBuffers(m_fliteBufferMgr, saveDngBuffer[count]->index);
                    if (ret < 0) {
                        CLOGE("ERR(%s[%d]):m_putBuffers(%d) fail", __FUNCTION__, __LINE__, saveDngBuffer[count]->index);
                        goto CLEAN;
                    }
                }
                saveDngBuffer[count]->index = -2;
            }
        }
        saveCount = 0;
        for (int i = 0; i < 3; i++) {
            if (saveDngBuffer[i]->index != -2) {
                saveCount++;
            }
        }
        if (saveCount == 2 && saveDngBuffer[0]->index == -2) {
            tempDngBuffer = saveDngBuffer[0];
            saveDngBuffer[0] = saveDngBuffer[1];
            saveDngBuffer[1] = saveDngBuffer[2];
            saveDngBuffer[2] = tempDngBuffer;
        } else if (saveCount == 1 && saveDngBuffer[0]->index == -2) {
            if (saveDngBuffer[1]->index == -2) {
                tempDngBuffer = saveDngBuffer[0];
                saveDngBuffer[0] = saveDngBuffer[2];
                saveDngBuffer[2] = tempDngBuffer;
            } else {
                tempDngBuffer = saveDngBuffer[0];
                saveDngBuffer[0] = saveDngBuffer[1];
                saveDngBuffer[1] = tempDngBuffer;
            }
        }
        if (m_dngLongExposureRemainCount <= 0) {
            for (count = 1; count < saveCount; count++) {
                if (saveDngBuffer[count]->index != -2) {
                    shot_ext = (camera2_shot_ext *)(saveDngBuffer[count]->addr[1]);
                    if (shot_ext->shot.dm.request.frameCount != 0) {
                        ret = m_putBuffers(m_fliteBufferMgr, saveDngBuffer[count]->index);
                        if (ret < 0) {
                            CLOGE("ERR(%s[%d]):m_putBuffers(%d) fail", __FUNCTION__, __LINE__, saveDngBuffer[count]->index);
                            goto CLEAN;
                        }
                    }
                    saveDngBuffer[count]->index = -2;
                }
            }
            saveCount = 1;
        }
    }

    if (m_dngLongExposureRemainCount > 0 && saveDngBuffer[2]->index != -2) {
        ret = addBayerBuffer(saveDngBuffer[2], saveDngBuffer[0], &dstRect);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):addBayerBuffer() fail", __FUNCTION__, __LINE__);
        }

        ret = m_putBuffers(m_fliteBufferMgr, saveDngBuffer[2]->index);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_putBuffers(%d) fail", __FUNCTION__, __LINE__, saveDngBuffer[2]->index);
            goto CLEAN;
        }
        saveDngBuffer[2]->index = -2;
        saveCount--;
        m_dngLongExposureRemainCount--;
    }

    if (m_dngLongExposureRemainCount > 0 && saveDngBuffer[1]->index != -2) {
        ret = addBayerBuffer(saveDngBuffer[1], saveDngBuffer[0], &dstRect);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):addBayerBuffer() fail", __FUNCTION__, __LINE__);
        }

        ret = m_putBuffers(m_fliteBufferMgr, saveDngBuffer[1]->index);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_putBuffers(%d) fail", __FUNCTION__, __LINE__, saveDngBuffer[1]->index);
            goto CLEAN;
        }
        saveDngBuffer[1]->index = -2;
        saveCount--;
        m_dngLongExposureRemainCount--;
        if (m_dngLongExposureRemainCount > 0) {
            goto DNG_RETRY;
        }
    } else if (m_dngLongExposureRemainCount > 0 && saveDngBuffer[1]->index == -2) {
        goto DNG_RETRY;
    }

#ifdef CAMERA_ADD_BAYER_ENABLE
    if (mIonClient >= 0)
        ion_sync_fd(mIonClient, saveDngBuffer[0]->fd[0]);
#endif

    m_longExposureEnds = true;

    CLOGD("[DNG](%s[%d]):frame count (%d)", __FUNCTION__, __LINE__, fliteFcount);

#if 0
    int sensorMaxW, sensorMaxH;
    int sensorMarginW, sensorMarginH;
    bool bRet;
    char filePath[70];

    memset(filePath, 0, sizeof(filePath));
    snprintf(filePath, sizeof(filePath), "/data/media/0/fliteCapture%d_%d.dng",
        m_exynosCameraParameters->getCameraId(), fliteFcount);
    /* Pure Bayer Buffer Size == MaxPictureSize + Sensor Margin == Max Sensor Size */
    m_exynosCameraParameters->getMaxSensorSize(&sensorMaxW, &sensorMaxH);

    CLOGD("[DNG](%s[%d]):Raw frame count (%d)", __FUNCTION__, __LINE__, fliteFcount);

    CLOGD("[DNG](%s[%d]):Raw Dump start (%s)", __FUNCTION__, __LINE__, filePath);

    bRet = dumpToFile((char *)filePath,
        dngBuffer.addr[0],
        dngBuffer.size[0]);
    if (bRet != true)
        ALOGE("couldn't make a raw file", __FUNCTION__, __LINE__);
    CLOGD("[DNG](%s[%d]):Raw Dump end (%s)", __FUNCTION__, __LINE__, filePath);
#endif

    if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_RAW_IMAGE)) {
        // Make Dng format
        m_dngCreator.makeDng(m_exynosCameraParameters, saveDngBuffer[0]->addr[0], &saveDngBuffer[0]->size[0]);

        // Check DNG save path
        char default_path[20];
        memset(default_path, 0, sizeof(default_path));
        strncpy(default_path, CAMERA_SAVE_PATH_PHONE, sizeof(default_path)-1);
        m_CheckCameraSavingPath(default_path,
            m_exynosCameraParameters->getDngFilePath(),
            m_dngSavePath, sizeof(m_dngSavePath));

        CLOGD("DNG(%s[%d]): RAW callback", __FUNCTION__, __LINE__);
        // Make Callback Buffer
        camera_memory_t *rawCallbackHeap = NULL;
        rawCallbackHeap = m_getDngCallbackHeap(saveDngBuffer[0]);
        if (!rawCallbackHeap || rawCallbackHeap->data == MAP_FAILED) {
            CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, saveDngBuffer[0]->size[0]);
            goto CLEAN;
        }

        if (!m_cancelPicture) {
            setBit(&m_callbackState, CALLBACK_STATE_RAW_IMAGE, true);
            m_dataCb(CAMERA_MSG_RAW_IMAGE, rawCallbackHeap, 0, NULL, m_callbackCookie);
            clearBit(&m_callbackState, CALLBACK_STATE_RAW_IMAGE, true);
        }
        rawCallbackHeap->release(rawCallbackHeap);
    }

CLEAN:
    for (count = 0; count < 3; count++) {
        if (saveDngBuffer[count]->index != -2) {
            ret = m_putBuffers(m_fliteBufferMgr, saveDngBuffer[count]->index);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):m_putBuffers(%d) fail", __FUNCTION__, __LINE__, saveDngBuffer[count]->index);
            }
        }
    }

    return loop;
}

camera_memory_t *ExynosCamera::m_getDngCallbackHeap(ExynosCameraBuffer *dngBuf)
{
    camera_memory_t *dngCallbackHeap = NULL;
    int dngSaveLocation = m_exynosCameraParameters->getDngSaveLocation();

    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    if (dngSaveLocation == BURST_SAVE_CALLBACK) {
        CLOGD("DEBUG(%s[%d]):burst callback : size (%d)", __FUNCTION__, __LINE__, dngBuf->size[0]);

        dngCallbackHeap = m_getMemoryCb(dngBuf->fd[0], dngBuf->size[0], 1, m_callbackCookie);
        if (!dngCallbackHeap || dngCallbackHeap->data == MAP_FAILED) {
            CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, dngBuf->size[0]);
            goto done;
        }
        if (dngBuf->fd[0] < 0)
            memcpy(dngCallbackHeap->data, dngBuf->addr[0], dngBuf->size[0]);
    } else {
        char filePath[CAMERA_FILE_PATH_SIZE];
        time_t rawtime;
        struct tm *timeinfo;

        // Get current local time
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        strftime(m_dngTime, 20, "%Y%m%d_%H%M%S", timeinfo);

        // Get Full Path Name (Path + FileName)
        memset(filePath, 0, sizeof(filePath));
        snprintf(filePath, sizeof(filePath), "%s%s_hal.dng", m_dngSavePath, m_dngTime);
        CLOGD("DEBUG(%s[%d]):burst callback : size (%d), filePath(%s)",
            __FUNCTION__, __LINE__, dngBuf->size[0], filePath);

        if(m_FileSaveFunc(filePath, dngBuf) == false) {
            CLOGE("ERR(%s[%d]): FILE save ERROR", __FUNCTION__, __LINE__);
            goto done;
        }

        dngCallbackHeap = m_getMemoryCb(-1, sizeof(filePath), 1, m_callbackCookie);
        if (!dngCallbackHeap || dngCallbackHeap->data == MAP_FAILED) {
            CLOGE("ERR(%s[%d]):m_getMemoryCb(%s) fail", __FUNCTION__, __LINE__, filePath);
            goto done;
        }

        memcpy(dngCallbackHeap->data, filePath, sizeof(filePath));
    }

done:

    if (dngCallbackHeap == NULL || dngCallbackHeap->data == MAP_FAILED) {
        if (dngCallbackHeap) {
            dngCallbackHeap->release(dngCallbackHeap);
            dngCallbackHeap = NULL;
        }

        m_notifyCb(CAMERA_MSG_ERROR, -1, 0, m_callbackCookie);
    }

    CLOGD("INFO(%s[%d]):making callback buffer done", __FUNCTION__, __LINE__);

    return dngCallbackHeap;
}
#endif

#ifdef RAWDUMP_CAPTURE
bool ExynosCamera::m_RawCaptureDumpThreadFunc(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    CLOGI("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    int ret = 0;
    int sensorMaxW, sensorMaxH;
    int sensorMarginW, sensorMarginH;
    bool bRet;
    char filePath[70];
    ExynosCameraBufferManager *bufferMgr = NULL;
    int bayerPipeId = 0;
    ExynosCameraBuffer bayerBuffer;
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraFrame *inListFrame = NULL;
    unsigned int fliteFcount = 0;

    ret = m_RawCaptureDumpQ->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        // TODO: doing exception handling
        goto CLEAN;
    }
    if (newFrame == NULL) {
        CLOGE("ERR(%s[%d]):newFrame is NULL", __FUNCTION__, __LINE__);
        goto CLEAN;
    }

    bayerPipeId = newFrame->getFirstEntity()->getPipeId();
    ret = newFrame->getDstBuffer(bayerPipeId, &bayerBuffer);
    ret = m_getBufferManager(bayerPipeId, &bufferMgr, DST_BUFFER_DIRECTION);

    /* Rawdump capture is available in pure bayer only */
    if (m_exynosCameraParameters->getUsePureBayerReprocessing() == true) {
        camera2_shot_ext *shot_ext = NULL;
        shot_ext = (camera2_shot_ext *)(bayerBuffer.addr[1]);
        if (shot_ext != NULL)
            fliteFcount = shot_ext->shot.dm.request.frameCount;
        else
            ALOGE("ERR(%s[%d]):fliteReprocessingBuffer is null", __FUNCTION__, __LINE__);
    } else {
        camera2_stream *shot_stream = NULL;
        shot_stream = (camera2_stream *)(bayerBuffer.addr[1]);
        if (shot_stream != NULL)
            fliteFcount = shot_stream->fcount;
        else
            ALOGE("ERR(%s[%d]):fliteReprocessingBuffer is null", __FUNCTION__, __LINE__);
    }

    memset(filePath, 0, sizeof(filePath));
    snprintf(filePath, sizeof(filePath), "/data/media/0/RawCapture%d_%d.raw",
        m_exynosCameraParameters->getCameraId(), fliteFcount);
    /* Pure Bayer Buffer Size == MaxPictureSize + Sensor Margin == Max Sensor Size */
    m_exynosCameraParameters->getMaxSensorSize(&sensorMaxW, &sensorMaxH);

    CLOGD("INFO(%s[%d]):Raw Dump start (%s)", __FUNCTION__, __LINE__, filePath);

    bRet = dumpToFile((char *)filePath,
        bayerBuffer.addr[0],
        sensorMaxW * sensorMaxH * 2);
    if (bRet != true)
        ALOGE("couldn't make a raw file", __FUNCTION__, __LINE__);

    ret = bufferMgr->putBuffer(bayerBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):putIndex is %d", __FUNCTION__, __LINE__, bayerBuffer.index);
        bufferMgr->printBufferState();
        bufferMgr->printBufferQState();
    }

CLEAN:

    if (newFrame != NULL) {
        newFrame->frameUnlock();

        ret = m_searchFrameFromList(&m_processList, newFrame->getFrameCount(), &inListFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):searchFrameFromList fail", __FUNCTION__, __LINE__);
        } else {
            if (inListFrame == NULL) {
                CLOGD("DEBUG(%s[%d]): Selected frame(%d) complete, Delete",
                    __FUNCTION__, __LINE__, newFrame->getFrameCount());
                newFrame->decRef();
                m_frameMgr->deleteFrame(newFrame);
            }
            newFrame = NULL;
        }
    }

    return ret;
}
#endif

status_t ExynosCamera::m_getBayerBuffer(uint32_t pipeId, ExynosCameraBuffer *buffer, camera2_shot_ext *updateDmShot)
{
    status_t ret = NO_ERROR;
    bool isSrc = false;
    int retryCount = 30; /* 200ms x 30 */
    camera2_shot_ext *shot_ext = NULL;
    camera2_stream *shot_stream = NULL;
    ExynosCameraFrame *inListFrame = NULL;
    ExynosCameraFrame *bayerFrame = NULL;
    ExynosCameraBuffer *saveBuffer = NULL;
    ExynosCameraBuffer tempBuffer;
    ExynosRect srcRect, dstRect;

#ifdef LLS_REPROCESSING
    unsigned int fliteFcount = 0;
#endif
    int trycount = 0;
    m_exynosCameraParameters->getPictureBayerCropSize(&srcRect, &dstRect);

BAYER_RETRY:
#ifdef ONE_SECOND_BURST_CAPTURE
    if (m_exynosCameraParameters->getCaptureExposureTime() != 0 &&
        m_exynosCameraParameters->getSeriesShotMode() == SERIES_SHOT_MODE_ONE_SECOND_BURST) {
        CLOGD("DEBUG(%s[%d]): manual exposure mode. Stop one second burst.", __FUNCTION__, __LINE__);
        m_stopLongExposure = true;
        m_jpegCallbackCounter.clearCount();
        m_stopBurstShot = true;
    }
#endif

    if (m_exynosCameraParameters->getCaptureExposureTime() != 0 && m_stopLongExposure) {
        CLOGD("DEBUG(%s[%d]): Emergency stop", __FUNCTION__, __LINE__);
        m_reprocessingCounter.clearCount();
        m_pictureEnabled = false;
        goto CLEAN;
    }

#ifdef OIS_CAPTURE
    if (m_exynosCameraParameters->getOISCaptureModeOn() == true) {
        retryCount = 7;
#ifdef SAMSUNG_LLS_DEBLUR
        if (m_exynosCameraParameters->getLDCaptureMode() > 0
            && m_exynosCameraParameters->getLDCaptureMode() < MULTI_SHOT_MODE_MULTI3) {
            m_captureSelector->setWaitTimeOISCapture(70000000);
        } else
#endif
        {
            m_captureSelector->setWaitTimeOISCapture(130000000);
        }
    }
#endif

    m_captureSelector->setWaitTime(200000000);
#ifdef RAWDUMP_CAPTURE
    for (int i = 0; i < 2; i++) {
        bayerFrame = m_captureSelector->selectFrames(m_reprocessingCounter.getCount(), pipeId, isSrc, retryCount);

        if(i == 0) {
            m_RawCaptureDumpQ->pushProcessQ(&bayerFrame);
        } else if (i == 1) {
            m_exynosCameraParameters->setRawCaptureModeOn(false);
        }
    }
#else
#ifdef SAMSUNG_QUICKSHOT
    if (m_exynosCameraParameters->getQuickShot() && m_quickShotStart) {
        for (int i = 0; i < FRAME_SKIP_COUNT_QUICK_SHOT; i++) {
            bayerFrame = m_captureSelector->selectFrames(m_reprocessingCounter.getCount(), pipeId, isSrc, retryCount);
        }
    } else
#endif
    {
        bayerFrame = m_captureSelector->selectFrames(m_reprocessingCounter.getCount(), pipeId, isSrc, retryCount);
    }
#ifdef SAMSUNG_QUICKSHOT
    m_quickShotStart = false;
#endif
#endif
    if (bayerFrame == NULL) {
        CLOGE("ERR(%s[%d]):bayerFrame is NULL", __FUNCTION__, __LINE__);
        ret = INVALID_OPERATION;
        goto CLEAN;
    }

#ifdef LLS_REPROCESSING
    m_exynosCameraParameters->setSetfileYuvRange();
#endif

    if (pipeId == PIPE_3AA) {
        ret = bayerFrame->getDstBuffer(pipeId, buffer, m_previewFrameFactory->getNodeType(pipeId));
        if (ret < 0) {
            CLOGE("ERR(%s[%d]): getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
            goto CLEAN;
        }
    } else if (pipeId == PIPE_FLITE) {
        ret = bayerFrame->getDstBuffer(pipeId, buffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]): getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
            goto CLEAN;
        }
    }

    if (m_exynosCameraParameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON ||
        m_exynosCameraParameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_PURE_DYNAMIC) {
        shot_ext = (struct camera2_shot_ext *)buffer->addr[1];
        CLOGD("DEBUG(%s[%d]): Selected frame count(hal : %d / driver : %d)", __FUNCTION__, __LINE__,
            bayerFrame->getFrameCount(), shot_ext->shot.dm.request.frameCount);
    } else if (m_exynosCameraParameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON ||
        m_exynosCameraParameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC) {
        if (updateDmShot == NULL) {
            CLOGE("ERR(%s[%d]): updateDmShot is NULL", __FUNCTION__, __LINE__);
            goto CLEAN;
        }

        retryCount = 12; /* 80ms * 12 */
        while (retryCount > 0) {
            if (bayerFrame->getMetaDataEnable() == false) {
                CLOGD("DEBUG(%s[%d]): Waiting for update jpeg metadata failed (%d), retryCount(%d)", __FUNCTION__, __LINE__, ret, retryCount);
            } else {
                break;
            }
            retryCount--;
            usleep(DM_WAITING_TIME);
        }

        /* update meta like pure bayer */
        bayerFrame->getUserDynamicMeta(updateDmShot);
        bayerFrame->getDynamicMeta(updateDmShot);
        shot_ext = updateDmShot;

        shot_stream = (struct camera2_stream *)buffer->addr[1];
        CLOGD("DEBUG(%s[%d]): Selected fcount(hal : %d / driver : %d)", __FUNCTION__, __LINE__,
            bayerFrame->getFrameCount(), shot_stream->fcount);
    } else {
        CLOGE("ERR(%s[%d]): reprocessing is not valid pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
        goto CLEAN;
    }

    if (m_exynosCameraParameters->getCaptureExposureTime() != 0 &&
        (shot_ext->shot.dm.sensor.exposureTime / 1000) != m_exynosCameraParameters->getLongExposureTime()) {
        trycount++;

        if (((trycount - (int)m_longExposureRemainCount) >= 3) && m_longExposureRemainCount < 3) {
            m_exynosCameraParameters->setPreviewExposureTime();
        }

        if (buffer->index != -2 && m_bayerBufferMgr != NULL)
            m_putBuffers(m_bayerBufferMgr, buffer->index);

        if (bayerFrame != NULL) {
            bayerFrame->frameUnlock();
            ret = m_searchFrameFromList(&m_processList, bayerFrame->getFrameCount(), &inListFrame);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):searchFrameFromList fail", __FUNCTION__, __LINE__);
            } else {
                CLOGD("DEBUG(%s[%d]): Selected frame(%d, %d msec) is not same with set from user.",
                        __FUNCTION__, __LINE__, bayerFrame->getFrameCount(),
                        (int) shot_ext->shot.dm.sensor.exposureTime);
                bayerFrame->decRef();
                m_frameMgr->deleteFrame(bayerFrame);
                bayerFrame = NULL;
            }
        }
        goto BAYER_RETRY;
    }

#ifdef SAMSUNG_DNG
    if (m_dngFrameNumberForManualExposure == 0)
        m_dngFrameNumberForManualExposure = shot_ext->shot.dm.request.frameCount;
#endif
    if (m_exynosCameraParameters->getCaptureExposureTime() != 0 &&
        m_longExposureRemainCount > 0) {
        if (saveBuffer == NULL) {
            saveBuffer = buffer;
            buffer = &tempBuffer;
            tempBuffer.index = -2;

            if (bayerFrame != NULL) {
                bayerFrame->frameUnlock();
                ret = m_searchFrameFromList(&m_processList, bayerFrame->getFrameCount(), &inListFrame);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):searchFrameFromList fail", __FUNCTION__, __LINE__);
                } else {
                    bayerFrame->decRef();
                    m_frameMgr->deleteFrame(bayerFrame);
                    bayerFrame = NULL;
                }
            }
            goto BAYER_RETRY;
        }

#ifdef CAMERA_PACKED_BAYER_ENABLE
        ret = addBayerBuffer(buffer, saveBuffer, &dstRect, true);
#else
        ret = addBayerBuffer(buffer, saveBuffer, &dstRect);
#endif
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):addBayerBufferPacked() fail", __FUNCTION__, __LINE__);
        }

        if (buffer->index != -2 && m_bayerBufferMgr != NULL)
            m_putBuffers(m_bayerBufferMgr, buffer->index);

        if (bayerFrame != NULL) {
            bayerFrame->frameUnlock();

            ret = m_searchFrameFromList(&m_processList, bayerFrame->getFrameCount(), &inListFrame);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):searchFrameFromList fail", __FUNCTION__, __LINE__);
            } else {
                CLOGD("DEBUG(%s[%d]): Selected frame(%d, %d msec) delete.",
                        __FUNCTION__, __LINE__, bayerFrame->getFrameCount(),
                        (int) shot_ext->shot.dm.sensor.exposureTime);
                bayerFrame->decRef();
                m_frameMgr->deleteFrame(bayerFrame);
                bayerFrame = NULL;
            }
        }
        m_longExposureRemainCount--;
        if (m_longExposureRemainCount > 0) {
            if (m_longExposureRemainCount < 3) {
                m_exynosCameraParameters->setPreviewExposureTime();
            }
            goto BAYER_RETRY;
        }
        buffer = saveBuffer;
        saveBuffer = NULL;

#ifdef CAMERA_ADD_BAYER_ENABLE
        if (mIonClient >= 0)
            ion_sync_fd(mIonClient, buffer->fd[0]);
#endif
    }

    if (m_exynosCameraParameters->getCaptureExposureTime() > CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT) {
        if (m_pictureThread->isRunning() == false) {
            ret = m_pictureThread->run();
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):couldn't run picture thread, ret(%d)", __FUNCTION__, __LINE__, ret);
                return INVALID_OPERATION;

            }
        }

        if (m_jpegCallbackThread->isRunning() == false) {
            ret = m_jpegCallbackThread->run();
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):couldn't run jpeg callback thread, ret(%d)", __FUNCTION__, __LINE__, ret);
                return INVALID_OPERATION;
            }
        }
    }

    m_exynosCameraParameters->setPreviewExposureTime();

#ifdef SAMSUNG_DNG
    if (m_exynosCameraParameters->getDNGCaptureModeOn() && !m_exynosCameraParameters->getCheckMultiFrame()) {
        m_exynosCameraParameters->setUseDNGCapture(false);
        m_captureSelector->resetDNGFrameCount();
        CLOGD("DNG(%s[%d]): getUseDNGCapture(%d) flitecount(%d)", __FUNCTION__, __LINE__,
                m_exynosCameraParameters->getUseDNGCapture(), shot_stream->fcount);
    }
#endif

#ifdef LLS_REPROCESSING
    if(m_captureSelector->getIsLastFrame() == false) {
        if (m_captureSelector->getIsConvertingMeta() == false) {
            camera2_shot_ext *shot_ext = NULL;
            shot_ext = (camera2_shot_ext *)(buffer->addr[1]);
            if (shot_ext != NULL)
                fliteFcount = shot_ext->shot.dm.request.frameCount;
        } else {
            camera2_stream *shot_stream = NULL;
            shot_stream = (camera2_stream *)(buffer->addr[1]);
            if (shot_stream != NULL)
                fliteFcount = shot_stream->fcount;
        }

        CLOGD("DEBUG(%s[%d]): LLS Capture ...ing(hal : %d / driver : %d)", __FUNCTION__, __LINE__,
            bayerFrame->getFrameCount(), fliteFcount);
        return ret;
    }
#endif

CLEAN:
    if (saveBuffer != NULL && saveBuffer->index != -2 && m_bayerBufferMgr != NULL) {
        m_putBuffers(m_bayerBufferMgr, saveBuffer->index);
        saveBuffer->index = -2;
    }

#ifdef LLS_REPROCESSING
    if (m_exynosCameraParameters->getSeriesShotMode() == SERIES_SHOT_MODE_LLS && m_captureSelector->getIsLastFrame() == true) {
        m_captureSelector->resetFrameCount();
        m_exynosCameraParameters->setLLSCaptureCount(0);
    }
#endif

    if (bayerFrame != NULL) {
        bayerFrame->frameUnlock();

        ret = m_searchFrameFromList(&m_processList, bayerFrame->getFrameCount(), &inListFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):searchFrameFromList fail", __FUNCTION__, __LINE__);
        } else {
            CLOGD("DEBUG(%s[%d]): Selected frame(%d) complete, Delete", __FUNCTION__, __LINE__, bayerFrame->getFrameCount());
            bayerFrame->decRef();
            m_frameMgr->deleteFrame(bayerFrame);
            bayerFrame = NULL;
        }
    }

    return ret;
}

status_t ExynosCamera::m_checkBufferAvailable(uint32_t pipeId, ExynosCameraBufferManager *bufferMgr)
{
    status_t ret = TIMED_OUT;
    int retry = 0;

    do {
        ret = -1;
        retry++;
        if (bufferMgr->getNumOfAvailableBuffer() > 0) {
            ret = OK;
        } else {
            /* wait available ISP buffer */
            usleep(WAITING_TIME);
        }
        if (retry % 10 == 0)
            CLOGW("WRAN(%s[%d]):retry(%d) setupEntity for pipeId(%d)", __FUNCTION__, __LINE__, retry, pipeId);
    } while(ret < 0 && retry < (TOTAL_WAITING_TIME/WAITING_TIME) && m_stopBurstShot == false);

    return ret;
}

status_t ExynosCamera::m_boostDynamicCapture(void)
{
    status_t ret = NO_ERROR;
#if 0 /* TODO: need to implementation for bayer */
    uint32_t pipeId = (isOwnScc(getCameraId()) == true) ? PIPE_SCC : PIPE_ISPC;
    uint32_t size = m_processList.size();

    ExynosCameraFrame *curFrame = NULL;
    List<ExynosCameraFrame *>::iterator r;
    camera2_node_group node_group_info_isp;

    if (m_processList.empty()) {
        CLOGD("DEBUG(%s[%d]):m_processList is empty", __FUNCTION__, __LINE__);
        return NO_ERROR;
    }
    CLOGD("DEBUG(%s[%d]):m_processList size(%d)", __FUNCTION__, __LINE__, m_processList.size());
    r = m_processList.end();

    for (unsigned int i = 0; i < 3; i++) {
        r--;
        if (r == m_processList.begin())
            break;

    }

    curFrame = *r;
    if (curFrame == NULL) {
        CLOGE("ERR(%s):curFrame is empty", __FUNCTION__);
        return INVALID_OPERATION;
    }

    if (curFrame->getRequest(pipeId) == true) {
        CLOGD("DEBUG(%s[%d]): Boosting dynamic capture is not need", __FUNCTION__, __LINE__);
        return NO_ERROR;
    }

    CLOGI("INFO(%s[%d]): boosting dynamic capture (frameCount: %d)", __FUNCTION__, __LINE__, curFrame->getFrameCount());
    /* For ISP */
    curFrame->getNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_ISP);
    m_updateBoostDynamicCaptureSize(&node_group_info_isp);
    curFrame->storeNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_ISP);

    curFrame->setRequest(pipeId, true);
    curFrame->setNumRequestPipe(curFrame->getNumRequestPipe() + 1);

    ret = curFrame->setEntityState(pipeId, ENTITY_STATE_REWORK);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setEntityState fail, pipeId(%d), state(%d), ret(%d)",
                __FUNCTION__, __LINE__, pipeId, ENTITY_STATE_REWORK, ret);
        return ret;
    }

    m_previewFrameFactory->pushFrameToPipe(&curFrame, pipeId);
    m_dynamicSccCount++;
    CLOGV("DEBUG(%s[%d]): dynamicSccCount inc(%d) frameCount(%d)", __FUNCTION__, __LINE__, m_dynamicSccCount, curFrame->getFrameCount());
#endif

    return ret;
}

void ExynosCamera::m_updateBoostDynamicCaptureSize(__unused camera2_node_group *node_group_info)
{
#if 0 /* TODO: need to implementation for bayer */
    ExynosRect sensorSize;
    ExynosRect bayerCropSize;

    node_group_info->capture[PERFRAME_BACK_SCC_POS].request = 1;

    m_exynosCameraParameters->getPreviewBayerCropSize(&sensorSize, &bayerCropSize);

    node_group_info->leader.input.cropRegion[0] = bayerCropSize.x;
    node_group_info->leader.input.cropRegion[1] = bayerCropSize.y;
    node_group_info->leader.input.cropRegion[2] = bayerCropSize.w;
    node_group_info->leader.input.cropRegion[3] = bayerCropSize.h;
    node_group_info->leader.output.cropRegion[0] = 0;
    node_group_info->leader.output.cropRegion[1] = 0;
    node_group_info->leader.output.cropRegion[2] = node_group_info->leader.input.cropRegion[2];
    node_group_info->leader.output.cropRegion[3] = node_group_info->leader.input.cropRegion[3];

    /* Capture 0 : SCC - [scaling] */
    node_group_info->capture[PERFRAME_BACK_SCC_POS].input.cropRegion[0] = node_group_info->leader.output.cropRegion[0];
    node_group_info->capture[PERFRAME_BACK_SCC_POS].input.cropRegion[1] = node_group_info->leader.output.cropRegion[1];
    node_group_info->capture[PERFRAME_BACK_SCC_POS].input.cropRegion[2] = node_group_info->leader.output.cropRegion[2];
    node_group_info->capture[PERFRAME_BACK_SCC_POS].input.cropRegion[3] = node_group_info->leader.output.cropRegion[3];

    node_group_info->capture[PERFRAME_BACK_SCC_POS].output.cropRegion[0] = node_group_info->capture[PERFRAME_BACK_SCC_POS].input.cropRegion[0];
    node_group_info->capture[PERFRAME_BACK_SCC_POS].output.cropRegion[1] = node_group_info->capture[PERFRAME_BACK_SCC_POS].input.cropRegion[1];
    node_group_info->capture[PERFRAME_BACK_SCC_POS].output.cropRegion[2] = node_group_info->capture[PERFRAME_BACK_SCC_POS].input.cropRegion[2];
    node_group_info->capture[PERFRAME_BACK_SCC_POS].output.cropRegion[3] = node_group_info->capture[PERFRAME_BACK_SCC_POS].input.cropRegion[3];

    /* Capture 1 : SCP - [scaling] */
    node_group_info->capture[PERFRAME_BACK_SCP_POS].input.cropRegion[0] = node_group_info->leader.output.cropRegion[0];
    node_group_info->capture[PERFRAME_BACK_SCP_POS].input.cropRegion[1] = node_group_info->leader.output.cropRegion[1];
    node_group_info->capture[PERFRAME_BACK_SCP_POS].input.cropRegion[2] = node_group_info->leader.output.cropRegion[2];
    node_group_info->capture[PERFRAME_BACK_SCP_POS].input.cropRegion[3] = node_group_info->leader.output.cropRegion[3];

#endif
    return;
}

void ExynosCamera::m_checkFpsAndUpdatePipeWaitTime(void)
{
    uint32_t curMinFps = 0;
    uint32_t curMaxFps = 0;
    frame_queue_t *inputFrameQ = NULL;

    m_exynosCameraParameters->getPreviewFpsRange(&curMinFps, &curMaxFps);

    if (m_curMinFps != curMinFps) {
        CLOGD("DEBUG(%s[%d]):(%d)(%d)", __FUNCTION__, __LINE__, curMinFps, curMaxFps);

        enum pipeline pipe = (isOwnScc(getCameraId()) == true) ? PIPE_SCC : PIPE_ISPC;

        m_previewFrameFactory->getInputFrameQToPipe(&inputFrameQ, pipe);

        /* 100ms * (30 / 15 fps) = 200ms */
        /* 100ms * (30 / 30 fps) = 100ms */
        /* 100ms * (30 / 10 fps) = 300ms */
        if (inputFrameQ != NULL && curMinFps != 0)
            inputFrameQ->setWaitTime(((100000000 / curMinFps) * 30));
    }

    m_curMinFps = curMinFps;

    return;
}

/* vision */
status_t ExynosCamera::m_startVisionInternal(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    CLOGI("DEBUG(%s[%d]):IN", __FUNCTION__, __LINE__);

    uint32_t minFrameNum = FRONT_NUM_BAYER_BUFFERS;
    int ret = 0;
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraBuffer dstBuf;

    m_fliteFrameCount = 0;
    m_3aa_ispFrameCount = 0;
    m_ispFrameCount = 0;
    m_sccFrameCount = 0;
    m_scpFrameCount = 0;
    m_displayPreviewToggle = 0;

    ret = m_visionFrameFactory->initPipes();
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_visionFrameFactory->initPipes() failed", __FUNCTION__, __LINE__);
        return ret;
    }

    m_visionFps = 10;
    m_visionAe = 0x2A;

    ret = m_visionFrameFactory->setControl(V4L2_CID_SENSOR_SET_FRAME_RATE, m_visionFps, PIPE_FLITE);
    if (ret < 0)
        CLOGE("ERR(%s[%d]):FLITE setControl fail, ret(%d)", __FUNCTION__, __LINE__, ret);

    CLOGD("DEBUG(%s[%d]):m_visionFps(%d)", __FUNCTION__, __LINE__, m_visionFps);

    ret = m_visionFrameFactory->setControl(V4L2_CID_SENSOR_SET_AE_TARGET, m_visionAe, PIPE_FLITE);
    if (ret < 0)
        CLOGE("ERR(%s[%d]):FLITE setControl fail, ret(%d)", __FUNCTION__, __LINE__, ret);

    CLOGD("DEBUG(%s[%d]):m_visionAe(%d)", __FUNCTION__, __LINE__, m_visionAe);

    for (uint32_t i = 0; i < minFrameNum; i++) {
        ret = generateFrameVision(i, &newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
            return ret;
        }
        if (newFrame == NULL) {
            CLOGE("ERR(%s[%d]):new faame is NULL", __FUNCTION__, __LINE__);
            return ret;
        }

        m_fliteFrameCount++;

        m_setupEntity(PIPE_FLITE, newFrame);
        m_visionFrameFactory->setOutputFrameQToPipe(m_pipeFrameVisionDoneQ, PIPE_FLITE);
        m_visionFrameFactory->pushFrameToPipe(&newFrame, PIPE_FLITE);
    }

    /* prepare pipes */
    ret = m_visionFrameFactory->preparePipes();
    if (ret < 0) {
        CLOGE("ERR(%s):preparePipe fail", __FUNCTION__);
        return ret;
    }

    /* stream on pipes */
    ret = m_visionFrameFactory->startPipes();
    if (ret < 0) {
        CLOGE("ERR(%s):startPipe fail", __FUNCTION__);
        return ret;
    }

    /* start all thread */
    ret = m_visionFrameFactory->startInitialThreads();
    if (ret < 0) {
        CLOGE("ERR(%s):startInitialThreads fail", __FUNCTION__);
        return ret;
    }

    m_previewEnabled = true;
    m_exynosCameraParameters->setPreviewRunning(m_previewEnabled);

    CLOGI("DEBUG(%s[%d]):OUT", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCamera::m_stopVisionInternal(void)
{
    int ret = 0;

    CLOGI("DEBUG(%s[%d]):IN", __FUNCTION__, __LINE__);

    ret = m_visionFrameFactory->stopPipes();
    if (ret < 0) {
        CLOGE("ERR(%s):stopPipe fail", __FUNCTION__);
        return ret;
    }

    CLOGD("DEBUG(%s[%d]):clear process Frame list", __FUNCTION__, __LINE__);
    ret = m_clearList(&m_processList);
    if (ret < 0) {
        CLOGE("ERR(%s):m_clearList fail", __FUNCTION__);
        return ret;
    }

    m_clearList(m_pipeFrameVisionDoneQ);

    m_fliteFrameCount = 0;
    m_3aa_ispFrameCount = 0;
    m_ispFrameCount = 0;
    m_sccFrameCount = 0;
    m_scpFrameCount = 0;

    m_previewEnabled = false;
    m_exynosCameraParameters->setPreviewRunning(m_previewEnabled);

    CLOGI("DEBUG(%s[%d]):OUT", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCamera::generateFrameVision(int32_t frameCount, ExynosCameraFrame **newFrame)
{
    Mutex::Autolock lock(m_frameLock);

    int ret = 0;
    *newFrame = NULL;

    if (frameCount >= 0) {
        ret = m_searchFrameFromList(&m_processList, frameCount, newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):searchFrameFromList fail", __FUNCTION__, __LINE__);
            return INVALID_OPERATION;
        }
    }

    if (*newFrame == NULL) {
        *newFrame = m_visionFrameFactory->createNewFrame();

        if (*newFrame == NULL) {
            CLOGE("ERR(%s):newFrame is NULL", __FUNCTION__);
            return UNKNOWN_ERROR;
        }

        m_processList.push_back(*newFrame);
    }

    return ret;
}

status_t ExynosCamera::m_setVisionBuffers(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    CLOGI("INFO(%s[%d]):alloc buffer - camera ID: %d",
        __FUNCTION__, __LINE__, m_cameraId);
    int ret = 0;
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES]    = {0};
    int hwPreviewW, hwPreviewH;
    int hwSensorW, hwSensorH;
    int hwPictureW, hwPictureH;

    int previewMaxW, previewMaxH;
    int sensorMaxW, sensorMaxH;

    int planeCount  = 1;
    int minBufferCount = 1;
    int maxBufferCount = 1;

    maxBufferCount = FRONT_NUM_BAYER_BUFFERS;
    planeSize[0] = VISION_WIDTH * VISION_HEIGHT;
    planeCount  = 2;
    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;

    /* ret = m_allocBuffers(m_bayerBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, maxBufferCount, type, true, false); */
    ret = m_allocBuffers(m_bayerBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, maxBufferCount, type, true, true);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):bayerBuffer m_allocBuffers(bufferCount=%d) fail",
            __FUNCTION__, __LINE__, maxBufferCount);
        return ret;
    }

    CLOGI("INFO(%s[%d]):alloc buffer done - camera ID: %d",
        __FUNCTION__, __LINE__, m_cameraId);

    return NO_ERROR;
}

status_t ExynosCamera::m_setVisionCallbackBuffer(void)
{
    int ret = 0;
    int previewW = 0, previewH = 0;
    int previewFormat = 0;
    m_exynosCameraParameters->getPreviewSize(&previewW, &previewH);
    previewFormat = m_exynosCameraParameters->getPreviewFormat();

    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};

    int planeCount  = getYuvPlaneCount(previewFormat);
    int bufferCount = FRONT_NUM_BAYER_BUFFERS;
    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;

    planeSize[0] = VISION_WIDTH * VISION_HEIGHT;
    planeCount = 1;
    ret = m_allocBuffers(m_previewCallbackBufferMgr, planeCount, planeSize, bytesPerLine, bufferCount, bufferCount, type, false, true);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_previewCallbackBufferMgr m_allocBuffers(bufferCount=%d) fail",
            __FUNCTION__, __LINE__, bufferCount);
        return ret;
    }

    return NO_ERROR;
}


bool ExynosCamera::m_visionThreadFunc(void)
{
    int ret = 0;
    int index = 0;

    int frameSkipCount = 0;
    ExynosCameraFrameEntity *entity = NULL;
    ExynosCameraFrame *handleFrame = NULL;
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraBuffer bayerBuffer;
    int pipeID = 0;
    /* to handle the high speed frame rate */
    bool skipPreview = false;
    int ratio = 1;
    uint32_t minFps = 0, maxFps = 0;
    uint32_t dispFps = EXYNOS_CAMERA_PREVIEW_FPS_REFERENCE;
    uint32_t fvalid = 0;
    uint32_t fcount = 0;
    struct camera2_stream *shot_stream = NULL;
    size_t callbackBufSize;
    status_t statusRet = NO_ERROR;
    int fps = 0;
    int ae = 0;
    int internalValue = 0;

    if (m_previewEnabled == false) {
        CLOGD("DEBUG(%s):preview is stopped, thread stop", __FUNCTION__);
        return false;
    }

    ret = m_pipeFrameVisionDoneQ->waitAndPopProcessQ(&handleFrame);
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGW("WARN(%s):wait timeout", __FUNCTION__);
        } else {
            CLOGE("ERR(%s):wait and pop fail, ret(%d)", __FUNCTION__, ret);
            /* TODO: doing exception handling */
        }
        return true;
    }

    if (handleFrame == NULL) {
        CLOGE("ERR(%s[%d]):handleFrame is NULL", __FUNCTION__, __LINE__);
        return true;
    }

    /* handle vision frame */
    entity = handleFrame->getFrameDoneEntity();
    if (entity == NULL) {
        CLOGE("ERR(%s[%d]):current entity is NULL", __FUNCTION__, __LINE__);
        /* TODO: doing exception handling */
        return true;
    }

    pipeID = entity->getPipeId();

    switch(entity->getPipeId()) {
    case PIPE_FLITE:
        ret = handleFrame->getDstBuffer(entity->getPipeId(), &bayerBuffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                __FUNCTION__, __LINE__, entity->getPipeId(), ret);
            return ret;
        }

#ifdef VISION_DUMP
    char filePath[50];
    snprintf(filePath, sizeof(filePath), "/data/media/0/DCIM/Camera/vision%02d.raw", dumpIndex);
    CLOGD("vision dump %s", filePath);
    dumpToFile(filePath, (char *)bayerBuffer.addr[0], VISION_WIDTH * VISION_HEIGHT);

    dumpIndex ++;
    if (dumpIndex > 4)
        dumpIndex = 0;
#endif

        m_exynosCameraParameters->getFrameSkipCount(&frameSkipCount);

        if (frameSkipCount > 0) {
            CLOGD("INFO(%s[%d]):frameSkipCount(%d)", __FUNCTION__, __LINE__, frameSkipCount);
        } else {
            /* callback */
            if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME)) {
                ExynosCameraBuffer previewCbBuffer;
                camera_memory_t *previewCallbackHeap = NULL;
                char *srcAddr = NULL;
                char *dstAddr = NULL;
                int bufIndex = -2;

                m_previewCallbackBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &previewCbBuffer);
                previewCallbackHeap = m_getMemoryCb(previewCbBuffer.fd[0], previewCbBuffer.size[0], 1, m_callbackCookie);
                if (!previewCallbackHeap || previewCallbackHeap->data == MAP_FAILED) {
                    CLOGE("ERR(%s[%d]):m_getMemoryCb(fd:%d, size:%d) fail", __FUNCTION__, __LINE__, previewCbBuffer.fd[0], previewCbBuffer.size[0]);

                    return INVALID_OPERATION;
                }

                for (int plane = 0; plane < 1; plane++) {
                    srcAddr = bayerBuffer.addr[plane];
                    dstAddr = (char *)previewCallbackHeap->data;
                    memcpy(dstAddr, srcAddr, previewCbBuffer.size[plane]);
                }
#if 0
                // Getting the buffer size for current preview size.
                callbackBufSize = m_getCurrentPreviewSizeBytes();
                if (callbackBufSize <= 0) {
                    CLOGE("ERR(%s[%d]): m_getCurrentPreviewSizeBytes fail, ret(%d)", __FUNCTION__, __LINE__, callbackBufSize);
                    statusRet = INVALID_OPERATION;
//                    goto done;
                }

                // Comparing newely updated preview buffer size against previewCallbackHeap.
                // If size is different, it means that the preview size has changed
                // during the memory transfer or GSC operations. So we have to drop this
                // preview callback buffer to prevent malfunctioning of user application
                if (previewCallbackHeap->size != callbackBufSize) {
                    CLOGW("WARN(%s[%d]): Preview size changed during operation. " \
                          "Initial=[%d], Current=[%d]. Current preview frame[%d] will be dropped."
                          , __FUNCTION__, __LINE__, previewCallbackHeap->size, callbackBufSize, handleFrame->getFrameCount());
//                    goto done;
                }
#endif
                setBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, true);
                m_dataCb(CAMERA_MSG_PREVIEW_FRAME, previewCallbackHeap, 0, NULL, m_callbackCookie);
                clearBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, true);

                previewCallbackHeap->release(previewCallbackHeap);

                m_previewCallbackBufferMgr->putBuffer(bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
            }
        }

        ret = m_putBuffers(m_bayerBufferMgr, bayerBuffer.index);

        ret = generateFrameVision(m_fliteFrameCount, &newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
            return ret;
        }

        ret = m_setupEntity(PIPE_FLITE, newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):setupEntity fail", __FUNCTION__, __LINE__);
            break;
        }

        m_visionFrameFactory->setOutputFrameQToPipe(m_pipeFrameVisionDoneQ, PIPE_FLITE);
        m_visionFrameFactory->pushFrameToPipe(&newFrame, PIPE_FLITE);
        m_fliteFrameCount++;

        break;
    default:
        break;
    }

    ret = handleFrame->setEntityState(entity->getPipeId(), ENTITY_STATE_COMPLETE);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setEntityState fail, pipeId(%d), state(%d), ret(%d)",
            __FUNCTION__, __LINE__, entity->getPipeId(), ENTITY_STATE_COMPLETE, ret);
        return ret;
    }

    CLOGV("DEBUG(%s[%d]):frame complete, count(%d)", __FUNCTION__, __LINE__, handleFrame->getFrameCount());
    ret = m_removeFrameFromList(&m_processList, handleFrame);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):remove frame from processList fail, ret(%d)", __FUNCTION__, __LINE__, ret);
    }

    handleFrame->decRef();
    m_frameMgr->deleteFrame(handleFrame);
    handleFrame = NULL;

#if 1
        fps = m_exynosCameraParameters->getVisionModeFps();
        if (m_visionFps != fps) {
            ret = m_visionFrameFactory->setControl(V4L2_CID_SENSOR_SET_FRAME_RATE, fps, PIPE_FLITE);
            if (ret < 0)
                CLOGE("ERR(%s[%d]):FLITE setControl fail, ret(%d)", __FUNCTION__, __LINE__, ret);

            m_visionFps = fps;
            CLOGD("DEBUG(%s[%d]):(%d)(%d)", __FUNCTION__, __LINE__, m_visionFps, fps);
        }
#if 0
        if (0 < m_visionAeTarget) {
            if (m_visionAeTarget != m_exynosVision->getAeTarget()) {
                if (m_exynosVision->setAeTarget(m_visionAeTarget) == false) {
                    CLOGE("ERR(%s): Fail to setAeTarget(%d)", __func__, m_visionAeTarget);

                    goto err;
                }
            }
        }
#endif

        ae = m_exynosCameraParameters->getVisionModeAeTarget();
        if (ae != m_visionAe) {
            switch(ae) {
            case 1:
                internalValue = 0x2A;
                break;
            case 2:
                internalValue = 0x8A;
                break;
            default:
                internalValue = 0x2A;
                break;
            }

            ret = m_visionFrameFactory->setControl(V4L2_CID_SENSOR_SET_AE_TARGET, internalValue, PIPE_FLITE);
            if (ret < 0)
                CLOGE("ERR(%s[%d]):FLITE setControl fail, ret(%d)", __FUNCTION__, __LINE__, ret);

            m_visionAe = ae;
            CLOGD("DEBUG(%s[%d]):(%d)(%d)", __FUNCTION__, __LINE__, m_visionAe, internalValue);
        }
#endif



    return true;
}

void ExynosCamera::m_printExynosCameraInfo(const char *funcName)
{
    int w = 0;
    int h = 0;
    ExynosRect srcRect, dstRect;

    CLOGD("DEBUG(%s[%d]):===================================================", __FUNCTION__, __LINE__);
    CLOGD("DEBUG(%s[%d]):============= ExynosCameraInfo call by %s", __FUNCTION__, __LINE__, funcName);
    CLOGD("DEBUG(%s[%d]):===================================================", __FUNCTION__, __LINE__);

    CLOGD("DEBUG(%s[%d]):============= Scenario ============================", __FUNCTION__, __LINE__);
    CLOGD("DEBUG(%s[%d]):= getCameraId                 : %d", __FUNCTION__, __LINE__, m_exynosCameraParameters->getCameraId());
    CLOGD("DEBUG(%s[%d]):= getDualMode                 : %d", __FUNCTION__, __LINE__, m_exynosCameraParameters->getDualMode());
    CLOGD("DEBUG(%s[%d]):= getScalableSensorMode       : %d", __FUNCTION__, __LINE__, m_exynosCameraParameters->getScalableSensorMode());
    CLOGD("DEBUG(%s[%d]):= getRecordingHint            : %d", __FUNCTION__, __LINE__, m_exynosCameraParameters->getRecordingHint());
    CLOGD("DEBUG(%s[%d]):= getEffectRecordingHint      : %d", __FUNCTION__, __LINE__, m_exynosCameraParameters->getEffectRecordingHint());
    CLOGD("DEBUG(%s[%d]):= getDualRecordingHint        : %d", __FUNCTION__, __LINE__, m_exynosCameraParameters->getDualRecordingHint());
    CLOGD("DEBUG(%s[%d]):= getAdaptiveCSCRecording     : %d", __FUNCTION__, __LINE__, m_exynosCameraParameters->getAdaptiveCSCRecording());
    CLOGD("DEBUG(%s[%d]):= doCscRecording              : %d", __FUNCTION__, __LINE__, m_exynosCameraParameters->doCscRecording());
    CLOGD("DEBUG(%s[%d]):= needGSCForCapture           : %d", __FUNCTION__, __LINE__, needGSCForCapture(getCameraId()));
    CLOGD("DEBUG(%s[%d]):= getShotMode                 : %d", __FUNCTION__, __LINE__, m_exynosCameraParameters->getShotMode());
    CLOGD("DEBUG(%s[%d]):= getTpuEnabledMode           : %d", __FUNCTION__, __LINE__, m_exynosCameraParameters->getTpuEnabledMode());
    CLOGD("DEBUG(%s[%d]):= getHWVdisMode               : %d", __FUNCTION__, __LINE__, m_exynosCameraParameters->getHWVdisMode());
    CLOGD("DEBUG(%s[%d]):= get3dnrMode                 : %d", __FUNCTION__, __LINE__, m_exynosCameraParameters->get3dnrMode());

    CLOGD("DEBUG(%s[%d]):============= Internal setting ====================", __FUNCTION__, __LINE__);
    CLOGD("DEBUG(%s[%d]):= isFlite3aaOtf               : %d", __FUNCTION__, __LINE__, m_exynosCameraParameters->isFlite3aaOtf());
    CLOGD("DEBUG(%s[%d]):= is3aaIspOtf                 : %d", __FUNCTION__, __LINE__, m_exynosCameraParameters->is3aaIspOtf());
    CLOGD("DEBUG(%s[%d]):= isIspTpuOtf                 : %d", __FUNCTION__, __LINE__, m_exynosCameraParameters->isIspTpuOtf());
    CLOGD("DEBUG(%s[%d]):= isReprocessing              : %d", __FUNCTION__, __LINE__, m_exynosCameraParameters->isReprocessing());
    CLOGD("DEBUG(%s[%d]):= isReprocessing3aaIspOTF     : %d", __FUNCTION__, __LINE__, m_exynosCameraParameters->isReprocessing3aaIspOTF());
    CLOGD("DEBUG(%s[%d]):= getUsePureBayerReprocessing : %d", __FUNCTION__, __LINE__, m_exynosCameraParameters->getUsePureBayerReprocessing());

    int reprocessingBayerMode = m_exynosCameraParameters->getReprocessingBayerMode();
    switch(reprocessingBayerMode) {
    case REPROCESSING_BAYER_MODE_NONE:
        CLOGD("DEBUG(%s[%d]):= getReprocessingBayerMode    : REPROCESSING_BAYER_MODE_NONE", __FUNCTION__, __LINE__);
        break;
    case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON:
        CLOGD("DEBUG(%s[%d]):= getReprocessingBayerMode    : REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON", __FUNCTION__, __LINE__);
        break;
    case REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON:
        CLOGD("DEBUG(%s[%d]):= getReprocessingBayerMode    : REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON", __FUNCTION__, __LINE__);
        break;
    case REPROCESSING_BAYER_MODE_PURE_DYNAMIC:
        CLOGD("DEBUG(%s[%d]):= getReprocessingBayerMode    : REPROCESSING_BAYER_MODE_PURE_DYNAMIC", __FUNCTION__, __LINE__);
        break;
    case REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC:
        CLOGD("DEBUG(%s[%d]):= getReprocessingBayerMode    : REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC", __FUNCTION__, __LINE__);
        break;
    default:
        CLOGD("DEBUG(%s[%d]):= getReprocessingBayerMode    : unexpected mode %d", __FUNCTION__, __LINE__, reprocessingBayerMode);
        break;
    }

    CLOGD("DEBUG(%s[%d]):= isSccCapture                : %d", __FUNCTION__, __LINE__, m_exynosCameraParameters->isSccCapture());

    CLOGD("DEBUG(%s[%d]):============= size setting =======================", __FUNCTION__, __LINE__);
    m_exynosCameraParameters->getMaxSensorSize(&w, &h);
    CLOGD("DEBUG(%s[%d]):= getMaxSensorSize            : %d x %d", __FUNCTION__, __LINE__, w, h);

    m_exynosCameraParameters->getHwSensorSize(&w, &h);
    CLOGD("DEBUG(%s[%d]):= getHwSensorSize             : %d x %d", __FUNCTION__, __LINE__, w, h);

    m_exynosCameraParameters->getBnsSize(&w, &h);
    CLOGD("DEBUG(%s[%d]):= getBnsSize                  : %d x %d", __FUNCTION__, __LINE__, w, h);

    m_exynosCameraParameters->getPreviewBayerCropSize(&srcRect, &dstRect);
    CLOGD("DEBUG(%s[%d]):= getPreviewBayerCropSize     : (%d, %d, %d, %d) -> (%d, %d, %d, %d)", __FUNCTION__, __LINE__,
        srcRect.x, srcRect.y, srcRect.w, srcRect.h,
        dstRect.x, dstRect.y, dstRect.w, dstRect.h);

    m_exynosCameraParameters->getPreviewBdsSize(&dstRect);
    CLOGD("DEBUG(%s[%d]):= getPreviewBdsSize           : (%d, %d, %d, %d)", __FUNCTION__, __LINE__,
        dstRect.x, dstRect.y, dstRect.w, dstRect.h);

    m_exynosCameraParameters->getHwPreviewSize(&w, &h);
    CLOGD("DEBUG(%s[%d]):= getHwPreviewSize            : %d x %d", __FUNCTION__, __LINE__, w, h);

    m_exynosCameraParameters->getPreviewSize(&w, &h);
    CLOGD("DEBUG(%s[%d]):= getPreviewSize              : %d x %d", __FUNCTION__, __LINE__, w, h);

    m_exynosCameraParameters->getPictureBayerCropSize(&srcRect, &dstRect);
    CLOGD("DEBUG(%s[%d]):= getPictureBayerCropSize     : (%d, %d, %d, %d) -> (%d, %d, %d, %d)", __FUNCTION__, __LINE__,
        srcRect.x, srcRect.y, srcRect.w, srcRect.h,
        dstRect.x, dstRect.y, dstRect.w, dstRect.h);

    m_exynosCameraParameters->getPictureBdsSize(&dstRect);
    CLOGD("DEBUG(%s[%d]):= getPictureBdsSize           : (%d, %d, %d, %d)", __FUNCTION__, __LINE__,
        dstRect.x, dstRect.y, dstRect.w, dstRect.h);

    m_exynosCameraParameters->getHwPictureSize(&w, &h);
    CLOGD("DEBUG(%s[%d]):= getHwPictureSize            : %d x %d", __FUNCTION__, __LINE__, w, h);

    m_exynosCameraParameters->getPictureSize(&w, &h);
    CLOGD("DEBUG(%s[%d]):= getPictureSize              : %d x %d", __FUNCTION__, __LINE__, w, h);

    CLOGD("DEBUG(%s[%d]):===================================================", __FUNCTION__, __LINE__);
}

status_t ExynosCamera::m_startCompanion(void)
{
#if defined(SAMSUNG_COMPANION)
    if(m_use_companion == true) {
        if (m_companionNode == NULL) {
            m_companionThread = new mainCameraThread(this, &ExynosCamera::m_companionThreadFunc,
                                                      "companionshotThread", PRIORITY_URGENT_DISPLAY);
            if (m_companionThread != NULL) {
                m_companionThread->run();
                CLOGD("DEBUG(%s): companionThread starts", __FUNCTION__);
            } else {
                CLOGE("(%s):failed the m_companionThread.", __FUNCTION__);
            }
        } else {
            CLOGW("WRN(%s[%d]):m_companionNode != NULL. so, it already running", __FUNCTION__, __LINE__);
        }
    }
#endif

#if defined(SAMSUNG_EEPROM)
    if ((m_use_companion == false) && (isEEprom(getCameraId()) == true)) {
        m_eepromThread = new mainCameraThread(this, &ExynosCamera::m_eepromThreadFunc,
                                                  "cameraeepromThread", PRIORITY_URGENT_DISPLAY);
        if (m_eepromThread != NULL) {
            m_eepromThread->run();
            CLOGD("DEBUG(%s): eepromThread starts", __FUNCTION__);
        } else {
            CLOGE("(%s): failed the m_eepromThread", __FUNCTION__);
        }
    }
#endif

    return NO_ERROR;
}

status_t ExynosCamera::m_stopCompanion(void)
{
#if defined(SAMSUNG_COMPANION)
    if(m_use_companion == true) {
        if (m_companionThread != NULL) {
            CLOGI("INFO(%s[%d]): wait m_companionThread", __FUNCTION__, __LINE__);
            m_companionThread->requestExitAndWait();
            CLOGI("INFO(%s[%d]): wait m_companionThread end", __FUNCTION__, __LINE__);
        } else {
            CLOGI("INFO(%s[%d]): m_companionThread is NULL", __FUNCTION__, __LINE__);
        }

        if (m_companionNode != NULL) {
            ExynosCameraDurationTimer timer;

            timer.start();

            if (m_companionNode->close() != NO_ERROR) {
                CLOGE("ERR(%s):close fail", __FUNCTION__);
            }
            delete m_companionNode;
            m_companionNode = NULL;

            CLOGD("DEBUG(%s):Companion Node(%d) closed", __FUNCTION__, MAIN_CAMERA_COMPANION_NUM);

            timer.stop();
            CLOGD("DEBUG(%s[%d]):duration time(%5d msec)", __FUNCTION__, __LINE__, (int)timer.durationMsecs());

        }
    }
#endif

#if defined(SAMSUNG_EEPROM)
    if ((m_use_companion == false) && (isEEprom(getCameraId()) == true)) {
        if (m_eepromThread != NULL) {
            CLOGI("INFO(%s[%d]): wait m_eepromThread", __FUNCTION__, __LINE__);
            m_eepromThread->requestExitAndWait();
        } else {
            CLOGI("INFO(%s[%d]): m_eepromThread is NULL", __FUNCTION__, __LINE__);
        }
    }
#endif

    return NO_ERROR;
}

status_t ExynosCamera::m_waitCompanionThreadEnd(void)
{
    ExynosCameraDurationTimer timer;

    timer.start();

#ifdef SAMSUNG_COMPANION
    if(m_use_companion == true) {
        if (m_companionThread != NULL) {
            m_companionThread->join();
        } else {
            CLOGI("INFO(%s[%d]):m_companionThread is NULL", __FUNCTION__, __LINE__);
        }
    }
#endif

    timer.stop();
    CLOGD("DEBUG(%s[%d]):companion waiting time : duration time(%5d msec)", __FUNCTION__, __LINE__, (int)timer.durationMsecs());

    CLOGD("DEBUG(%s[%d]):companionThread join", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

#ifdef SAMSUNG_SENSOR_LISTENER
bool ExynosCamera::m_sensorListenerThreadFunc(void)
{
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);
    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;

    m_timer.start();

    uint32_t curMinFps = 0;
    uint32_t curMaxFps = 0;

    m_exynosCameraParameters->getPreviewFpsRange(&curMinFps, &curMaxFps);
    if(curMinFps > 60 && curMaxFps > 60)
        goto skip_sensor_listener;

#ifdef SAMSUNG_HRM
    if(getCameraId() == CAMERA_ID_BACK && m_exynosCameraParameters->getSamsungCamera() == true &&
            m_exynosCameraParameters->getVtMode() == 0) {
        if(m_uv_rayHandle == NULL) {
            m_uv_rayHandle = sensor_listener_load();
            if(m_uv_rayHandle != NULL) {
                if(sensor_listener_enable_sensor(m_uv_rayHandle, ST_UV_RAY, 10*1000) < 0) {
                    sensor_listener_unload(&m_uv_rayHandle);
                } else {
                    m_exynosCameraParameters->m_setHRM_Hint(true);
                }
            }
        } else {
            m_exynosCameraParameters->m_setHRM_Hint(true);
        }
    }
#endif

#ifdef SAMSUNG_LIGHT_IR
    if(getCameraId() == CAMERA_ID_FRONT && m_exynosCameraParameters->getSamsungCamera() == true &&
            m_exynosCameraParameters->getVtMode() == 0) {
        if(m_light_irHandle == NULL) {
            m_light_irHandle = sensor_listener_load();
            if(m_light_irHandle != NULL) {
                if(sensor_listener_enable_sensor(m_light_irHandle, ST_LIGHT_IR, 120*1000) < 0) {
                    sensor_listener_unload(&m_light_irHandle);
                } else {
                    m_exynosCameraParameters->m_setLight_IR_Hint(true);
                }
            }
        } else {
            m_exynosCameraParameters->m_setLight_IR_Hint(true);
        }
    }
#endif

#ifdef SAMSUNG_GYRO
    if(getCameraId() == CAMERA_ID_BACK && m_exynosCameraParameters->getSamsungCamera() == true &&
            m_exynosCameraParameters->getVtMode() == 0) {
        if(m_gyroHandle == NULL) {
            m_gyroHandle = sensor_listener_load();
            if(m_gyroHandle != NULL) {
                /* HACK
                 * Below event rate(7700) is set to let gyro timestamp operate
                 * for VDIS performance. Real event rate is 10ms not 7.7ms
                 */
                if(sensor_listener_enable_sensor(m_gyroHandle,ST_GYROSCOPE, 7700) < 0) {
                    sensor_listener_unload(&m_gyroHandle);
                } else {
                    m_exynosCameraParameters->m_setGyroHint(true);
                }
            }
        } else {
            m_exynosCameraParameters->m_setGyroHint(true);
        }
    }
#endif

skip_sensor_listener:
    m_timer.stop();
    durationTime = m_timer.durationMsecs();
    CLOGD("DEBUG(%s):duration time(%5d msec)", __FUNCTION__, (int)durationTime);

    return false;
}
#endif /* SAMSUNG_SENSOR_LISTENER */

#ifdef SAMSUNG_LLV_TUNING
status_t ExynosCamera::m_checkLLVtuning(void)
{
    FILE *fp = NULL;
    int i;
    char filePath[30];
    status_t ret = NO_ERROR;

    for(i = UNI_PLUGIN_POWER_CONTROL_OFF; i <= UNI_PLUGIN_POWER_CONTROL_4; i++) {
        memset(filePath, 0, sizeof(filePath));
        sprintf(filePath, "%s_%d.txt", "/mnt/sdcard/LLVKEY", i);
        fp = fopen(filePath, "r");
        if (fp == NULL) {
            CLOGE("(%s[%d]):failed to open LLVKEY_%d", __FUNCTION__, __LINE__, i);
        }
        else {
            CLOGD("(%s[%d]):succeeded to open LLVKEY_%d", __FUNCTION__, __LINE__, i);
            m_LLVpowerLevel = i;
            fclose(fp);
            break;
        }
    }

    return NO_ERROR;
}
#endif

#ifdef SAMSUNG_HLV
status_t ExynosCamera::m_ProgramAndProcessHLV(ExynosCameraBuffer *FrameBuffer)
{
    UniPluginBufferData_t BufferInfo;
    int curVideoW = 0, curVideoH = 0;
    int numValid = 0;
    int ret = 0;
    UniPluginFocusData_t *focusData = NULL;

    m_HLVprocessStep = HLV_PROCESS_PRE;
    if (FrameBuffer == NULL) {
        ALOGE("HLV: [%s-%d]: FrameBuffer is NULL - Exit", __FUNCTION__, __LINE__);
        return -1;
    }

    int maxNumFocusAreas = m_exynosCameraParameters->getMaxNumFocusAreas();
    if (maxNumFocusAreas <= 0) {
        ALOGE("HLV: [%s-%d]: maxNumFocusAreas is <= 0 - Exit", __FUNCTION__, __LINE__);
        return -1;
    }

    focusData = new UniPluginFocusData_t[maxNumFocusAreas];

    memset(&BufferInfo, 0, sizeof(UniPluginBufferData_t));
    memset(focusData, 0, sizeof(UniPluginFocusData_t) *  maxNumFocusAreas);

    m_exynosCameraParameters->getVideoSize(&curVideoW, &curVideoH);

    BufferInfo.InWidth = curVideoW;
    BufferInfo.InHeight = curVideoH;
    BufferInfo.InBuffY = FrameBuffer->addr[0];
    BufferInfo.InBuffU = FrameBuffer->addr[1];
    BufferInfo.ZoomLevel = m_exynosCameraParameters->getZoomLevel();
    BufferInfo.Timestamp = m_lastRecordingTimeStamp / 1000000;  /* in MilliSec */

    ALOGV("HLV: [%s-%d]: uni_plugin_set BUFFER_INFO", __FUNCTION__, __LINE__);
    m_HLVprocessStep = HLV_PROCESS_SET;
    ret = uni_plugin_set(m_HLV,
                HIGHLIGHT_VIDEO_PLUGIN_NAME,
                UNI_PLUGIN_INDEX_BUFFER_INFO,
                &BufferInfo);
    if (ret < 0)
        ALOGE("HLV: uni_plugin_set fail!");


    ALOGV("HLV: uni_plugin_set FOCUS_INFO");

    if (m_exynosCameraParameters->getObjectTrackingEnable())
    {
        ExynosRect2 *objectTrackingArea = new ExynosRect2[maxNumFocusAreas];
        int* objectTrackingWeight = new int[maxNumFocusAreas];
        int validNumber;

        m_exynosCameraParameters->getObjectTrackingAreas(&validNumber,
            objectTrackingArea, objectTrackingWeight);

        for (int i = 0; i < validNumber; i++) {
            focusData[i].FocusROILeft = objectTrackingArea[i].x1;
            focusData[i].FocusROIRight = objectTrackingArea[i].x2;
            focusData[i].FocusROITop = objectTrackingArea[i].y1;
            focusData[i].FocusROIBottom = objectTrackingArea[i].y2;
            focusData[i].FocusWeight = objectTrackingWeight[i];
        }

        if (validNumber > 1)
            ALOGW("HLV: Valid Focus Area > 1");

        delete[] objectTrackingArea;
        delete[] objectTrackingWeight;
    }
    else
    {
        int FocusAreaWeight = 0;
        ExynosRect2 CurrentFocusArea;
        ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();

        memset(&CurrentFocusArea, 0, sizeof(ExynosRect2));
        autoFocusMgr->getFocusAreas(&CurrentFocusArea, &FocusAreaWeight);

        focusData[0].FocusROILeft = CurrentFocusArea.x1;
        focusData[0].FocusROIRight = CurrentFocusArea.x2;
        focusData[0].FocusROITop = CurrentFocusArea.y1;
        focusData[0].FocusROIBottom = CurrentFocusArea.y2;
        focusData[0].FocusWeight = FocusAreaWeight;

        ALOGV("HLV: CurrentFocusArea : %d, %d, %d, %d",
            CurrentFocusArea.x1, CurrentFocusArea.x2,
            CurrentFocusArea.y1, CurrentFocusArea.y2);
    }

    ret = uni_plugin_set(m_HLV,
                HIGHLIGHT_VIDEO_PLUGIN_NAME,
                UNI_PLUGIN_INDEX_FOCUS_INFO,
                focusData);
    if (ret < 0)
        ALOGE("HLV: uni_plugin_set fail!");

    m_HLVprocessStep = HLV_PROCESS_START;
    ret = uni_plugin_process(m_HLV);
    if (ret < 0)
        ALOGE("HLV: uni_plugin_process FAIL");

    delete[] focusData;

    m_HLVprocessStep = HLV_PROCESS_DONE;
    return ret;
}
#endif

void ExynosCamera::m_checkEntranceLux(struct camera2_shot_ext *meta_shot_ext) {
    uint32_t data = 0;

    if (m_checkFirstFrameLux == false || m_exynosCameraParameters->getDualMode() == true ||
        m_exynosCameraParameters->getRecordingHint() == true) {
        m_checkFirstFrameLux = false;
        return;
    }

    data = (int32_t)meta_shot_ext->shot.udm.ae.vendorSpecific[399];

    if (data <= ENTRANCE_LOW_LUX) {
        ALOGD("(%s[%d]): need skip frame for ae/awb stable(%d).", __FUNCTION__, __LINE__, data);
        m_exynosCameraParameters->setFrameSkipCount(2);
    }
    m_checkFirstFrameLux = false;
}

status_t ExynosCamera::m_copyMetaFrameToFrame(ExynosCameraFrame *srcframe, ExynosCameraFrame *dstframe, bool useDm, bool useUdm)
{
    Mutex::Autolock lock(m_metaCopyLock);
    int ret = 0;

    memset(m_tempshot, 0x00, sizeof(struct camera2_shot_ext));
    if(useDm) {
        srcframe->getDynamicMeta(m_tempshot);
        ret = dstframe->storeDynamicMeta(m_tempshot);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]): storeDynamicMeta fail ret(%d)", __FUNCTION__, __LINE__, ret);
        }
    }

    if(useUdm) {
        srcframe->getUserDynamicMeta(m_tempshot);
        ret = dstframe->storeUserDynamicMeta(m_tempshot);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]): storeUserDynamicMeta fail ret(%d)", __FUNCTION__, __LINE__, ret);
        }
    }

    return NO_ERROR;
}


#ifdef SAMSUNG_DEBLUR
void ExynosCamera::setUseDeblurCaptureOn(bool enable)
{
    m_useDeblurCaptureOn = enable;
}

bool ExynosCamera::getUseDeblurCaptureOn(void)
{
    return m_useDeblurCaptureOn;
}

bool ExynosCamera::m_deblurCaptureThreadFunc(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    int ret = 0;
    int loop = false;
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraBuffer gscReprocessingBuffer1;
    ExynosCameraBuffer gscReprocessingBuffer2;

    gscReprocessingBuffer1.index = -2;
    gscReprocessingBuffer2.index = -2;

    int pipeId_gsc = 0;
    int currentSeriesShotMode = 0;

    if (m_exynosCameraParameters->isReprocessing() == true) {
        if (needGSCForCapture(getCameraId()) == true) {
            pipeId_gsc = PIPE_GSC_REPROCESSING;
        } else {
            pipeId_gsc = (isOwnScc(getCameraId()) == true) ? PIPE_SCC_REPROCESSING : PIPE_ISP_REPROCESSING;
        }
    } else {
        if (needGSCForCapture(getCameraId()) == true) {
            pipeId_gsc = PIPE_GSC_PICTURE;
        } else {
            if (isOwnScc(getCameraId()) == true) {
                pipeId_gsc = PIPE_SCC;
            } else {
                pipeId_gsc = PIPE_ISPC;
            }
        }
    }

    ExynosCameraBufferManager *bufferMgr = NULL;
    ret = m_getBufferManager(pipeId_gsc, &bufferMgr, DST_BUFFER_DIRECTION);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getBufferManager(SRC) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_gsc, ret);
        return ret;
    }

    CLOGI("INFO(%s[%d]):wait m_deblurCaptureQ output", __FUNCTION__, __LINE__);
    ret = m_deblurCaptureQ->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        CLOGW("WARN(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        goto CLEAN;
    }
    if (newFrame == NULL) {
        CLOGE("ERR(%s[%d]):newFrame is NULL", __FUNCTION__, __LINE__);
        goto CLEAN;
    }

    if (m_jpegCounter.getCount() <= 0) {
        CLOGD("DEBUG(%s[%d]): Picture canceled", __FUNCTION__, __LINE__);
        goto CLEAN;
    }

    CLOGI("INFO(%s[%d]):m_deblurCaptureQ output done", __FUNCTION__, __LINE__);

    if (m_deblur_firstFrame == NULL) {
        CLOGI("INFO(%s[%d]): first", __FUNCTION__, __LINE__);
        m_deblur_firstFrame = newFrame;
    } else {
        m_deblur_secondFrame = newFrame;

        if (getUseDeblurCaptureOn()) {
            CLOGI("INFO(%s[%d]):second getUseDeblurCaptureOn(%d)", __FUNCTION__, __LINE__, getUseDeblurCaptureOn());
            /* Library */
            int pipeId_postPictureGsc = PIPE_GSC_REPROCESSING2;
            int previewW = 0, previewH = 0, previewFormat = 0;
            int pictureW = 0, pictureH = 0, pictureFormat = 0;
            ExynosRect srcRect_postPicturegGsc, dstRect_postPicturegGsc;
            float zoomRatio = m_exynosCameraParameters->getZoomRatio(0) / 1000;

            CLOGD("[Deblur](%s[%d]):-- Start Library --", __FUNCTION__, __LINE__);
            ExynosCameraDurationTimer m_timer;
            long long durationTime = 0;

            m_timer.start();

            ret = m_deblur_firstFrame->getDstBuffer(pipeId_gsc, &gscReprocessingBuffer1);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_gsc, ret);
                goto CLEAN;
            }

            ret = m_deblur_secondFrame->getDstBuffer(pipeId_gsc, &gscReprocessingBuffer2);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_gsc, ret);
                goto CLEAN;
            }

            m_exynosCameraParameters->getPictureSize(&pictureW, &pictureH);
            pictureFormat = m_exynosCameraParameters->getPictureFormat();

            if(m_DeblurpluginHandle != NULL) {
                UniPluginBufferData_t pluginData;
                memset(&pluginData, 0, sizeof(UniPluginBufferData_t));
                pluginData.InBuffY = gscReprocessingBuffer1.addr[0];
                pluginData.InBuffU = gscReprocessingBuffer1.addr[0] + (pictureW * pictureH);
                pluginData.InWidth = pictureW;
                pluginData.InHeight = pictureH;
                pluginData.InFormat = UNI_PLUGIN_FORMAT_NV21;
                pluginData.Index = 0;
                CLOGD("[Deblur](%s[%d]): Buffer1 Width: %d, Height: %d", __FUNCTION__, __LINE__,
                            pluginData.InWidth, pluginData.InHeight);
                ret = uni_plugin_set(m_DeblurpluginHandle,
                        DEBLUR_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &pluginData);
                if(ret < 0) {
                    CLOGE("[Deblur](%s[%d]): Deblur Plugin set UNI_PLUGIN_INDEX_BUFFER_INFO failed!!",
                                __FUNCTION__, __LINE__);
                }
#if 0
                char filePath[70] = "/data/media/0/deblur_input1.yuv";
                dumpToFile((char *)filePath,
                    gscReprocessingBuffer1.addr[0],
                    (pictureW * pictureH * 3) / 2);
#endif
                memset(&pluginData, 0, sizeof(UniPluginBufferData_t));
                pluginData.InBuffY = gscReprocessingBuffer2.addr[0];
                pluginData.InBuffU = gscReprocessingBuffer2.addr[0] + (pictureW * pictureH);
                pluginData.InWidth = pictureW;
                pluginData.InHeight = pictureH;
                pluginData.InFormat = UNI_PLUGIN_FORMAT_NV21;
                pluginData.Index = 1;
                CLOGD("[Deblur](%s[%d]): Buffer2 Width: %d, Height: %d", __FUNCTION__, __LINE__,
                            pluginData.InWidth, pluginData.InHeight);
                ret = uni_plugin_set(m_DeblurpluginHandle,
                        DEBLUR_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &pluginData);
                if(ret < 0) {
                    CLOGE("[Deblur](%s[%d]): Deblur Plugin set UNI_PLUGIN_INDEX_BUFFER_INFO failed!!",
                                __FUNCTION__, __LINE__);
                }
#if 0
                char filePath2[70] = "/data/media/0/deblur_input2.yuv";
                dumpToFile((char *)filePath2,
                    gscReprocessingBuffer2.addr[0],
                    (pictureW * pictureH * 3) / 2);
#endif
                UniPluginExtraBufferInfo_t extraData;
                memset(&extraData, 0, sizeof(UniPluginExtraBufferInfo_t));

                struct camera2_dm *dm = new struct camera2_dm;
                memset(dm, 0, sizeof(camera2_dm));

                m_deblur_firstFrame->getDynamicMeta(dm);
                extraData.iso[0] = dm->sensor.sensitivity;
                extraData.exposureTime.num = 1;
                if (dm->sensor.exposureTime != 0)
                    extraData.exposureTime.den = (uint32_t) 1e9 / dm->sensor.exposureTime;
                else
                    extraData.exposureTime.num = 0;
                CLOGD("[Deblur](%s[%d]): ISO: %d, exposure: %d/%d", __FUNCTION__, __LINE__,
                            extraData.iso[0], extraData.exposureTime.num, extraData.exposureTime.den);
                ret = uni_plugin_set(m_DeblurpluginHandle,
                        DEBLUR_PLUGIN_NAME, UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO, &extraData);
                if(ret < 0) {
                    CLOGE("[Deblur](%s[%d]): Deblur Plugin set UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO failed!!", __FUNCTION__, __LINE__);
                }

                delete dm;

                ret = uni_plugin_process(m_DeblurpluginHandle);
                if(ret < 0) {
                    CLOGE("[Deblur](%s[%d]): Deblur Plugin process failed!!", __FUNCTION__, __LINE__);
                }
#if 0
                char filePath3[70] = "/data/media/0/deblur_output2.yuv";
                dumpToFile((char *)filePath3,
                    gscReprocessingBuffer2.addr[0],
                    (pictureW * pictureH * 3) / 2);
#endif
            }
            m_timer.stop();
            durationTime = m_timer.durationMsecs();
            CLOGD("[Deblur](%s[%d]):duration time(%5d msec)", __FUNCTION__, __LINE__, (int)durationTime);
            CLOGD("[Deblur](%s[%d]):-- end Library --", __FUNCTION__, __LINE__);

            if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_POSTVIEW_FRAME)) {
                m_exynosCameraParameters->setIsThumbnailCallbackOn(true);
            }

            if (m_exynosCameraParameters->getIsThumbnailCallbackOn()) {
                CLOGD("POSTVIEW callback start, m_postPictureGscThread run (%s)(%d) pipe(%d)",
                    __FUNCTION__, __LINE__, pipeId_postPictureGsc);

                int retry1 = 0;
                do {
                    ret = -1;
                    retry1++;
                    if (m_postPictureGscBufferMgr->getNumOfAvailableBuffer() > 0) {
                        ret = m_setupEntity(pipeId_postPictureGsc, m_deblur_secondFrame, &gscReprocessingBuffer2, NULL);
                    } else {
                        /* wait available SCC buffer */
                        usleep(WAITING_TIME);
                    }
                    if (retry1 % 10 == 0)
                        CLOGW("WRAN(%s[%d]):retry setupEntity for GSC", __FUNCTION__, __LINE__);
                } while(ret < 0 && retry1 < (TOTAL_WAITING_TIME/WAITING_TIME));

                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):setupEntity fail, pipeId(%d), retry(%d), ret(%d)",
                        __FUNCTION__, __LINE__, pipeId_postPictureGsc, retry1, ret);
                    goto CLEAN;
                }

                m_exynosCameraParameters->getPreviewSize(&previewW, &previewH);
                m_exynosCameraParameters->getPictureSize(&pictureW, &pictureH);
                pictureFormat = m_exynosCameraParameters->getPictureFormat();
                previewFormat = m_exynosCameraParameters->getPreviewFormat();

                CLOGD("DEBUG(%s):size preview(%d, %d,format:%d)picture(%d, %d,format:%d)", __FUNCTION__,
                    previewW, previewH, previewFormat, pictureW, pictureH, pictureFormat);

                srcRect_postPicturegGsc.x = 0;
                srcRect_postPicturegGsc.y = 0;
                srcRect_postPicturegGsc.w = pictureW;
                srcRect_postPicturegGsc.h = pictureH;
                srcRect_postPicturegGsc.fullW = pictureW;
                srcRect_postPicturegGsc.fullH = pictureH;
                srcRect_postPicturegGsc.colorFormat = HAL_PIXEL_FORMAT_2_V4L2_PIX(HAL_PIXEL_FORMAT_YCrCb_420_SP);
                dstRect_postPicturegGsc.x = 0;
                dstRect_postPicturegGsc.y = 0;
                dstRect_postPicturegGsc.w = previewW;
                dstRect_postPicturegGsc.h = previewH;
                dstRect_postPicturegGsc.fullW = previewW;
                dstRect_postPicturegGsc.fullH = previewH;
                dstRect_postPicturegGsc.colorFormat = HAL_PIXEL_FORMAT_2_V4L2_PIX(HAL_PIXEL_FORMAT_YCrCb_420_SP);

                ret = getCropRectAlign(srcRect_postPicturegGsc.w,  srcRect_postPicturegGsc.h,
                    previewW,   previewH,
                    &srcRect_postPicturegGsc.x, &srcRect_postPicturegGsc.y,
                    &srcRect_postPicturegGsc.w, &srcRect_postPicturegGsc.h,
                    2, 2, 0, zoomRatio);

                ret = m_deblur_secondFrame->setSrcRect(pipeId_postPictureGsc, &srcRect_postPicturegGsc);
                ret = m_deblur_secondFrame->setDstRect(pipeId_postPictureGsc, &dstRect_postPicturegGsc);

                CLOGD("DEBUG(%s):size (%d, %d, %d, %d %d %d)", __FUNCTION__,
                    srcRect_postPicturegGsc.x, srcRect_postPicturegGsc.y, srcRect_postPicturegGsc.w,
                    srcRect_postPicturegGsc.h, srcRect_postPicturegGsc.fullW, srcRect_postPicturegGsc.fullH);
                CLOGD("DEBUG(%s):size (%d, %d, %d, %d %d %d)", __FUNCTION__,
                    dstRect_postPicturegGsc.x, dstRect_postPicturegGsc.y, dstRect_postPicturegGsc.w,
                    dstRect_postPicturegGsc.h, dstRect_postPicturegGsc.fullW, dstRect_postPicturegGsc.fullH);

                /* push frame to GSC pipe */
                m_pictureFrameFactory->setOutputFrameQToPipe(dstPostPictureGscQ, pipeId_postPictureGsc);
                m_pictureFrameFactory->pushFrameToPipe(&m_deblur_secondFrame, pipeId_postPictureGsc);

                ret = m_postPictureGscThread->run();
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):couldn't run m_postPictureGscThread, ret(%d)", __FUNCTION__, __LINE__, ret);
                    goto CLEAN;
                }
            }

            m_postPictureQ->pushProcessQ(&m_deblur_secondFrame);
            m_deblur_secondFrame = NULL;
        } else {
            CLOGI("INFO(%s[%d]):second getUseDeblurCaptureOn(%d)", __FUNCTION__, __LINE__, getUseDeblurCaptureOn());
            ret = m_deblur_firstFrame->getDstBuffer(pipeId_gsc, &gscReprocessingBuffer1);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_gsc, ret);
                goto CLEAN;
            }

            ret = m_deblur_secondFrame->getDstBuffer(pipeId_gsc, &gscReprocessingBuffer2);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_gsc, ret);
                goto CLEAN;
            }

            /* put GSC buffer */
            ret = m_putBuffers(bufferMgr, gscReprocessingBuffer2.index);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):bufferMgr->putBuffers() fail, pipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, pipeId_gsc, ret);
                goto CLEAN;
            }

            if (m_deblur_secondFrame != NULL) {
                m_deblur_secondFrame->printEntity();
                m_deblur_secondFrame->frameUnlock();
                ret = m_removeFrameFromList(&m_postProcessList, m_deblur_secondFrame);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):remove frame from processList fail, ret(%d)", __FUNCTION__, __LINE__, ret);
                }

                CLOGD("DEBUG(%s[%d]): Picture frame delete(%d)", __FUNCTION__, __LINE__, m_deblur_secondFrame->getFrameCount());
                m_deblur_secondFrame->decRef();
                m_frameMgr->deleteFrame(m_deblur_secondFrame);
                m_deblur_secondFrame = NULL;
            }
        }

        /* put GSC 1st buffer */
        ret = m_putBuffers(bufferMgr, gscReprocessingBuffer1.index);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):bufferMgr->putBuffers() fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipeId_gsc, ret);
            goto CLEAN;
        }

        if (m_deblur_firstFrame != NULL) {
            m_deblur_firstFrame->printEntity();
            m_deblur_firstFrame->frameUnlock();
            ret = m_removeFrameFromList(&m_postProcessList, m_deblur_firstFrame);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):remove frame from processList fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            }

            CLOGD("DEBUG(%s[%d]): Picture frame delete(%d)", __FUNCTION__, __LINE__, m_deblur_firstFrame->getFrameCount());
            m_deblur_firstFrame->decRef();
            m_frameMgr->deleteFrame(m_deblur_firstFrame);
            m_deblur_firstFrame = NULL;
        }
    }

CLEAN:

    return loop;
}

bool ExynosCamera::m_detectDeblurCaptureThreadFunc(void)
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif
    int32_t previewFormat = 0;
    status_t ret = NO_ERROR;
    ExynosCameraBuffer postgscReprocessingBuffer;
    ExynosCameraBufferManager *bufferMgr = NULL;
    int gscPipe = PIPE_GSC_REPROCESSING3;
    int loop = false;

    postgscReprocessingBuffer.index = -2;

    CLOGD("DEBUG(%s[%d]):-- IN --", __FUNCTION__, __LINE__);

    /* wait GSC */
    CLOGV("INFO(%s[%d]):wait GSC output pipe(%d)", __FUNCTION__, __LINE__);
    ret = m_detectDeblurCaptureQ->waitAndPopProcessQ(&postgscReprocessingBuffer);
    CLOGD("INFO(%s[%d]):GSC output done pipe(%d)", __FUNCTION__, __LINE__);
    if (ret < 0) {
        CLOGE("ERR(%s)(%d):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
       goto CLEAN;
    }

    m_getBufferManager(gscPipe, &bufferMgr, SRC_BUFFER_DIRECTION);

    /* Library */
    CLOGD("DEBUG(%s[%d]):-- wait Library --", __FUNCTION__, __LINE__);
    usleep(40000);
    CLOGD("DEBUG(%s[%d]):-- end Library --", __FUNCTION__, __LINE__);

#if 1
    setUseDeblurCaptureOn(true);
#else
    setUseDeblurCaptureOn(false);
#endif

    CLOGD("DEBUG(%s[%d]):--OUT--", __FUNCTION__, __LINE__);

CLEAN:

    if (postgscReprocessingBuffer.index != -2)
        m_putBuffers(bufferMgr, postgscReprocessingBuffer.index);

    return loop;
}
#endif

#ifdef SAMSUNG_LLS_DEBLUR
bool ExynosCamera::m_LDCaptureThreadFunc(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    CLOGI("[LDC](%s[%d]):", __FUNCTION__, __LINE__);

    int ret = 0;
    int loop = false;
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraBuffer gscReprocessingBuffer;
    int pipeId_gsc = 0;
    int pipeId_postPictureGsc = PIPE_GSC_REPROCESSING2;
    int previewW = 0, previewH = 0, previewFormat = 0;
    int pictureW = 0, pictureH = 0, pictureFormat = 0;
    ExynosRect srcRect_postPicturegGsc, dstRect_postPicturegGsc;
    float zoomRatio = m_exynosCameraParameters->getZoomRatio(0) / 1000;
    int retry1 = 0;
    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;

    gscReprocessingBuffer.index = -2;

    if (m_exynosCameraParameters->isReprocessing() == true) {
        if (needGSCForCapture(getCameraId()) == true) {
            pipeId_gsc = PIPE_GSC_REPROCESSING;
        } else {
            pipeId_gsc = (isOwnScc(getCameraId()) == true) ? PIPE_SCC_REPROCESSING : PIPE_ISP_REPROCESSING;
        }
    } else {
        if (needGSCForCapture(getCameraId()) == true) {
            pipeId_gsc = PIPE_GSC_PICTURE;
        } else {
            if (isOwnScc(getCameraId()) == true) {
                pipeId_gsc = PIPE_SCC;
            } else {
                pipeId_gsc = PIPE_ISPC;
            }
        }
    }

    ExynosCameraBufferManager *bufferMgr = NULL;
    ret = m_getBufferManager(pipeId_gsc, &bufferMgr, DST_BUFFER_DIRECTION);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getBufferManager(SRC) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_gsc, ret);
        return ret;
    }

    CLOGI("INFO(%s[%d]):wait m_deblurCaptureQ output", __FUNCTION__, __LINE__);
    ret = m_LDCaptureQ->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        CLOGW("WARN(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        goto CLEAN;
    }

    if (newFrame == NULL) {
        CLOGE("ERR(%s[%d]):newFrame is NULL", __FUNCTION__, __LINE__);
        goto CLEAN;
    }

    if (m_jpegCounter.getCount() <= 0) {
        CLOGD("DEBUG(%s[%d]): Picture canceled", __FUNCTION__, __LINE__);
        goto CLEAN;
    }

    ret = newFrame->getDstBuffer(pipeId_gsc, &gscReprocessingBuffer);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_gsc, ret);
        goto CLEAN;
    }

    m_LDBufIndex[m_LDCaptureCount] = gscReprocessingBuffer.index;
    m_LDCaptureCount++;

    CLOGI("[LDC](%s[%d]):m_deblurCaptureQ output done m_LDCaptureCount(%d) index(%d)",
        __FUNCTION__, __LINE__, m_LDCaptureCount, gscReprocessingBuffer.index);

    /* Library */
    CLOGD("[LDC](%s[%d]):-- Start Library --", __FUNCTION__, __LINE__);
    m_timer.start();
    if (m_LLSpluginHandle != NULL) {
        int totalCount = 0;
        UNI_PLUGIN_OPERATION_MODE opMode = UNI_PLUGIN_OP_END;

        int llsMode = m_exynosCameraParameters->getLDCaptureLLSValue();
        switch(llsMode) {
        case LLS_LEVEL_MULTI_MERGE_2:
        case LLS_LEVEL_MULTI_MERGE_INDICATOR_2:
            totalCount = 2;
            opMode = UNI_PLUGIN_OP_COMPOSE_IMAGE;
            break;
        case LLS_LEVEL_MULTI_MERGE_3:
        case LLS_LEVEL_MULTI_MERGE_INDICATOR_3:
            totalCount = 3;
            opMode = UNI_PLUGIN_OP_COMPOSE_IMAGE;
            break;
        case LLS_LEVEL_MULTI_MERGE_4:
        case LLS_LEVEL_MULTI_MERGE_INDICATOR_4:
            totalCount = 4;
            opMode = UNI_PLUGIN_OP_COMPOSE_IMAGE;
            break;
        case LLS_LEVEL_MULTI_PICK_2:
            totalCount = 2;
            opMode = UNI_PLUGIN_OP_SELECT_IMAGE;
            break;
        case LLS_LEVEL_MULTI_PICK_3:
            totalCount = 3;
            opMode = UNI_PLUGIN_OP_SELECT_IMAGE;
            break;
        case LLS_LEVEL_MULTI_PICK_4:
            totalCount = 4;
            opMode = UNI_PLUGIN_OP_SELECT_IMAGE;
            break;
        default:
            CLOGE("[LDC](%s[%d]): Wrong LLS mode has been get!!", __FUNCTION__, __LINE__);
            goto CLEAN;
        }

        /* First shot */
        if (m_LDCaptureCount == 1) {
            ret = uni_plugin_init(m_LLSpluginHandle);
            if (ret < 0) {
                CLOGE("[LDC](%s[%d]): LLS Plugin init failed!!", __FUNCTION__, __LINE__);
            }

            CLOGD("[LDC](%s[%d]): Set capture num: %d", __FUNCTION__, __LINE__, totalCount);
            ret = uni_plugin_set(m_LLSpluginHandle,
                    LLS_PLUGIN_NAME, UNI_PLUGIN_INDEX_TOTAL_BUFFER_NUM, &totalCount);
            if (ret < 0) {
                CLOGE("[LDC](%s[%d]): LLS Plugin set UNI_PLUGIN_INDEX_TOTAL_BUFFER_NUM failed!!", __FUNCTION__, __LINE__);
            }

            CLOGD("[LDC](%s[%d]): Set capture mode: %d", __FUNCTION__, __LINE__, opMode);
            ret = uni_plugin_set(m_LLSpluginHandle,
                    LLS_PLUGIN_NAME, UNI_PLUGIN_INDEX_OPERATION_MODE, &opMode);
            if (ret < 0) {
                CLOGE("[LDC](%s[%d]): LLS Plugin set UNI_PLUGIN_INDEX_OPERATION_MODE failed!!", __FUNCTION__, __LINE__);
            }

            UniPluginCameraInfo_t cameraInfo;
            cameraInfo.CameraType = (UNI_PLUGIN_CAMERA_TYPE)getCameraId();
            cameraInfo.SensorType = (UNI_PLUGIN_SENSOR_TYPE)m_cameraSensorId;
            CLOGD("[LDC](%s[%d]): Set camera info: %d:%d", __FUNCTION__, __LINE__,
                    cameraInfo.CameraType, cameraInfo.SensorType);
            ret = uni_plugin_set(m_LLSpluginHandle,
                    LLS_PLUGIN_NAME, UNI_PLUGIN_INDEX_CAMERA_INFO, &cameraInfo);
            if (ret < 0) {
                CLOGE("[LDC](%s[%d]): LLS Plugin set UNI_PLUGIN_INDEX_CAMERA_INFO failed!!", __FUNCTION__, __LINE__);
            }

            struct camera2_udm *udm = new struct camera2_udm;

            if (udm != NULL) {
                UniPluginExtraBufferInfo_t extraData;
                memset(&extraData, 0, sizeof(UniPluginExtraBufferInfo_t));
                memset(udm, 0x00, sizeof(struct camera2_udm));
                newFrame->getUserDynamicMeta(udm);
                extraData.brightnessValue.num = udm->ae.vendorSpecific[5];
                extraData.brightnessValue.den = 256;
                /* short shutter speed(us) */
                extraData.shutterSpeed[0].num = udm->ae.vendorSpecific[390];
                extraData.shutterSpeed[0].den = 1000000;
                /* long shutter speed(us) */
                extraData.shutterSpeed[1].num = udm->ae.vendorSpecific[392];
                extraData.shutterSpeed[1].den = 1000000;
                /* short ISO */
                extraData.iso[0] = udm->ae.vendorSpecific[391];
                /* long ISO */
                extraData.iso[1] = udm->ae.vendorSpecific[393];
                extraData.DRCstrength = udm->ae.vendorSpecific[394];
                ret = uni_plugin_set(m_LLSpluginHandle,
                    LLS_PLUGIN_NAME, UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO, &extraData);
                if(ret < 0) {
                    CLOGE("[LDC](%s[%d]): LLS Plugin set UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO failed!!", __FUNCTION__, __LINE__);
                }

                delete udm;
            }
        }

        UniPluginBufferData_t pluginData;
        memset(&pluginData, 0, sizeof(UniPluginBufferData_t));
        m_exynosCameraParameters->getPictureSize(&pictureW, &pictureH);
        pluginData.InBuffY = gscReprocessingBuffer.addr[0];
        pluginData.InBuffU = gscReprocessingBuffer.addr[0] + (pictureW * pictureH);
        pluginData.InWidth = pictureW;
        pluginData.InHeight = pictureH;
        pluginData.InFormat = UNI_PLUGIN_FORMAT_NV21;
#if 0
        char filePath[100] = {'\0',};
        sprintf(filePath, "/data/media/0/LLS_input_%d.yuv", m_LDCaptureCount);
        dumpToFile((char *)filePath,
                gscReprocessingBuffer.addr[0],
                (pictureW * pictureH * 3) / 2);
#endif
        CLOGD("[LDC](%s[%d]): Set buffer info, Width: %d, Height: %d", __FUNCTION__, __LINE__,
                pluginData.InWidth, pluginData.InHeight);
        ret = uni_plugin_set(m_LLSpluginHandle,
                LLS_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &pluginData);
        if (ret < 0) {
            CLOGE("[LDC](%s[%d]): LLS Plugin set UNI_PLUGIN_INDEX_BUFFER_INFO failed!!", __FUNCTION__, __LINE__);
        }

        /* Last shot */
        if (m_LDCaptureCount == totalCount) {
            ret = uni_plugin_process(m_LLSpluginHandle);
            if (ret < 0) {
                CLOGE("[LDC](%s[%d]): LLS Plugin process failed!!", __FUNCTION__, __LINE__);
            }

            UTstr debugData;
            debugData.data = new unsigned char[LLS_EXIF_SIZE];

            ret = uni_plugin_get(m_LLSpluginHandle,
                LLS_PLUGIN_NAME, UNI_PLUGIN_INDEX_DEBUG_INFO, &debugData);
            CLOGD("[LDC](%s[%d]): Debug buffer size: %d", __FUNCTION__, __LINE__, debugData.size);
            m_exynosCameraParameters->setLLSdebugInfo(debugData.data, debugData.size);

            if (debugData.data != NULL)
                delete []debugData.data;

            ret = uni_plugin_deinit(m_LLSpluginHandle);
            if (ret < 0) {
                CLOGE("[LDC](%s[%d]): LLS Plugin deinit failed!!", __FUNCTION__, __LINE__);
            }
        }
    }
    m_timer.stop();
    durationTime = m_timer.durationMsecs();
    CLOGD("[LDC](%s[%d]):duration time(%5d msec)", __FUNCTION__, __LINE__, (int)durationTime);
    CLOGD("[LDC](%s[%d]):-- end Library --", __FUNCTION__, __LINE__);

    if (m_LDCaptureCount < m_exynosCameraParameters->getLDCaptureCount()) {
        if (newFrame != NULL) {
            newFrame->printEntity();
            newFrame->frameUnlock();
            ret = m_removeFrameFromList(&m_postProcessList, newFrame);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):remove frame from processList fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            }

            CLOGD("DEBUG(%s[%d]): Picture frame delete(%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());
            newFrame->decRef();
            m_frameMgr->deleteFrame(newFrame);
            newFrame = NULL;
        }
        goto CLEAN;
    }

    if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_POSTVIEW_FRAME)) {
        m_exynosCameraParameters->setIsThumbnailCallbackOn(true);
    }

    if (m_exynosCameraParameters->getIsThumbnailCallbackOn()) {
        CLOGD("[LDC](%s[%d]): POSTVIEW callback start, m_postPictureGscThread run(%d)",
            __FUNCTION__, __LINE__ , pipeId_postPictureGsc);

        do {
            ret = -1;
            retry1++;
            if (m_postPictureGscBufferMgr->getNumOfAvailableBuffer() > 0) {
                ret = m_setupEntity(pipeId_postPictureGsc, newFrame, &gscReprocessingBuffer, NULL);
            } else {
                /* wait available SCC buffer */
                usleep(WAITING_TIME);
            }
            if (retry1 % 10 == 0)
                CLOGW("WRAN(%s[%d]):retry setupEntity for GSC", __FUNCTION__, __LINE__);
        } while(ret < 0 && retry1 < (TOTAL_WAITING_TIME/WAITING_TIME));

        if (ret < 0) {
            CLOGE("ERR(%s[%d]):setupEntity fail, pipeId(%d), retry(%d), ret(%d)",
                __FUNCTION__, __LINE__, pipeId_postPictureGsc, retry1, ret);
            goto CLEAN;
        }

        m_exynosCameraParameters->getPreviewSize(&previewW, &previewH);
        m_exynosCameraParameters->getPictureSize(&pictureW, &pictureH);
        pictureFormat = m_exynosCameraParameters->getPictureFormat();
        previewFormat = m_exynosCameraParameters->getPreviewFormat();

        CLOGD("[LDC](%s):size preview(%d, %d,format:%d)picture(%d, %d,format:%d)", __FUNCTION__,
            previewW, previewH, previewFormat, pictureW, pictureH, pictureFormat);

        srcRect_postPicturegGsc.x = 0;
        srcRect_postPicturegGsc.y = 0;
        srcRect_postPicturegGsc.w = pictureW;
        srcRect_postPicturegGsc.h = pictureH;
        srcRect_postPicturegGsc.fullW = pictureW;
        srcRect_postPicturegGsc.fullH = pictureH;
        srcRect_postPicturegGsc.colorFormat = HAL_PIXEL_FORMAT_2_V4L2_PIX(HAL_PIXEL_FORMAT_YCrCb_420_SP);
        dstRect_postPicturegGsc.x = 0;
        dstRect_postPicturegGsc.y = 0;
        dstRect_postPicturegGsc.w = previewW;
        dstRect_postPicturegGsc.h = previewH;
        dstRect_postPicturegGsc.fullW = previewW;
        dstRect_postPicturegGsc.fullH = previewH;
        dstRect_postPicturegGsc.colorFormat = HAL_PIXEL_FORMAT_2_V4L2_PIX(HAL_PIXEL_FORMAT_YCrCb_420_SP);

        ret = getCropRectAlign(srcRect_postPicturegGsc.w,  srcRect_postPicturegGsc.h,
            previewW,   previewH,
            &srcRect_postPicturegGsc.x, &srcRect_postPicturegGsc.y,
            &srcRect_postPicturegGsc.w, &srcRect_postPicturegGsc.h,
            2, 2, 0, zoomRatio);

        ret = newFrame->setSrcRect(pipeId_postPictureGsc, &srcRect_postPicturegGsc);
        ret = newFrame->setDstRect(pipeId_postPictureGsc, &dstRect_postPicturegGsc);

        CLOGD("[LDC](%s):size (%d, %d, %d, %d %d %d)", __FUNCTION__,
            srcRect_postPicturegGsc.x, srcRect_postPicturegGsc.y, srcRect_postPicturegGsc.w,
            srcRect_postPicturegGsc.h, srcRect_postPicturegGsc.fullW, srcRect_postPicturegGsc.fullH);
        CLOGD("[LDC](%s):size (%d, %d, %d, %d %d %d)", __FUNCTION__,
            dstRect_postPicturegGsc.x, dstRect_postPicturegGsc.y, dstRect_postPicturegGsc.w,
            dstRect_postPicturegGsc.h, dstRect_postPicturegGsc.fullW, dstRect_postPicturegGsc.fullH);

        /* push frame to GSC pipe */
        m_pictureFrameFactory->setOutputFrameQToPipe(dstPostPictureGscQ, pipeId_postPictureGsc);
        m_pictureFrameFactory->pushFrameToPipe(&newFrame, pipeId_postPictureGsc);

        ret = m_postPictureGscThread->run();
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):couldn't run m_postPictureGscThread, ret(%d)", __FUNCTION__, __LINE__, ret);
            goto CLEAN;
        }
    }

    m_postPictureQ->pushProcessQ(&newFrame);

    for (int i = 0; i < m_exynosCameraParameters->getLDCaptureCount() - 1; i++) {
        /* put GSC 1st buffer */
        ret = m_putBuffers(bufferMgr, m_LDBufIndex[i]);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):bufferMgr->putBuffers() fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipeId_gsc, ret);
        }
    }

CLEAN:
    int waitCount = 15;
    if (m_LDCaptureCount == m_exynosCameraParameters->getLDCaptureCount()) {
        waitCount = 0;
    }

    while (m_LDCaptureQ->getSizeOfProcessQ() == 0 && 0 < waitCount) {
        usleep(10000);
        waitCount--;
    }

    if (m_LDCaptureQ->getSizeOfProcessQ()) {
        CLOGD("[LDC](%s[%d]):m_LDCaptureThread thread will run again. m_LDCaptureQ size(%d)",
            __func__, __LINE__, m_LDCaptureQ->getSizeOfProcessQ());
        loop = true;
    }

    return loop;
}

#endif

}; /* namespace android */
