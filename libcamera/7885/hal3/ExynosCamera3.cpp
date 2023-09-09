/*
 * Copyright (C) 2015, Samsung Electronics Co. LTD
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

/* #define LOG_NDEBUG 0 */
#define LOG_TAG "ExynosCamera3"
#include <cutils/log.h>
#include <ui/Fence.h>

#include "ExynosCamera3.h"

namespace android {

#ifdef MONITOR_LOG_SYNC
uint32_t ExynosCamera3::cameraSyncLogId = 0;
#endif
ExynosCamera3::ExynosCamera3(int cameraId, camera_metadata_t **info):
    m_requestMgr(NULL),
    m_parameters(NULL),
    m_streamManager(NULL),
    m_activityControl(NULL)
{
    BUILD_DATE();
    m_cameraId = cameraId;

    memset(m_name, 0x00, sizeof(m_name));

#ifdef DEBUG_CLASS_INFO
    m_dumpClassInfo();
#endif

#if defined(SAMSUNG_COMPANION) || defined(SAMSUNG_EEPROM)
    m_use_companion = isCompanion(cameraId);
#endif

    /* Initialize pointer variables */
    m_ionAllocator = NULL;

    m_fliteBufferMgr = NULL;
    m_3aaBufferMgr = NULL;
    m_ispBufferMgr = NULL;
    m_mcscBufferMgr = NULL;
#ifdef SUPPORT_DEPTH_MAP
    m_depthMapBufferMgr = NULL;
#endif
#ifdef USE_VRA_GROUP
    m_vraBufferMgr = NULL;
#endif
    m_ispReprocessingBufferMgr = NULL;
    m_yuvCaptureBufferMgr = NULL;
    m_thumbnailBufferMgr = NULL;
    m_internalScpBufferMgr = NULL;

    m_bayerBufferMgr= NULL;
    m_zslBufferMgr = NULL;

    m_captureSelector = NULL;
    m_captureZslSelector = NULL;

#ifdef SAMSUNG_COMPANION
    m_companionNode = NULL;
    m_companionThread = NULL;
    CLOGI("cameraId(%d), use_companion(%d)", cameraId, m_use_companion);

    if (m_use_companion == true) {
        m_companionThread = new mainCameraThread(this, &ExynosCamera3::m_companionThreadFunc,
                                                  "companionshotThread", PRIORITY_URGENT_DISPLAY);
        if (m_companionThread != NULL) {
            m_companionThread->run();
            CLOGD(" companionThread starts");
        } else {
            CLOGE("failed the m_companionThread.");
        }
    }
#endif

#if defined(SAMSUNG_EEPROM)
    m_eepromThread = NULL;
    if ((m_use_companion == false) && (isEEprom(getCameraId()) == true)) {
        m_eepromThread = new mainCameraThread(this, &ExynosCamera3::m_eepromThreadFunc,
                                                  "cameraeepromThread", PRIORITY_URGENT_DISPLAY);
        if (m_eepromThread != NULL) {
            m_eepromThread->run();
            CLOGD(" eepromThread starts");
        } else {
            CLOGE("failed the m_eepromThread");
        }
    }
#endif

    /* Create related classes */
#if defined(SAMSUNG_COMPANION) || defined(SAMSUNG_EEPROM)
    m_parameters = new ExynosCamera3Parameters(m_cameraId, m_use_companion);
#else
    m_parameters = new ExynosCamera3Parameters(m_cameraId, false);
#endif

    m_parameters->setHalVersion(IS_HAL_VER_3_2);

    m_activityControl = m_parameters->getActivityControl();

    m_metadataConverter = new ExynosCamera3MetadataConverter(cameraId, (ExynosCameraParameters *)m_parameters);
    m_requestMgr = new ExynosCameraRequestManager(cameraId, (ExynosCameraParameters *)m_parameters);
    m_requestMgr->setMetaDataConverter(m_metadataConverter);

    /* Create managers */
    m_createManagers();

    /* Create threads */
    m_createThreads();

    /* Create queue for preview path. If you want to control pipeDone in ExynosCamera3, try to create frame queue here */
    m_shotDoneQ = new ExynosCameraList<uint32_t>();
    for (int i = 0; i < MAX_PIPE_NUM; i++) {
        switch (i) {
        case PIPE_FLITE:
        case PIPE_3AA:
        case PIPE_ISP:
        case PIPE_MCSC:
#ifdef USE_VRA_GROUP
        case PIPE_VRA:
#endif
            m_pipeFrameDoneQ[i] = new frame_queue_t;
            break;
        default:
            m_pipeFrameDoneQ[i] = NULL;
            break;
        }
    }

    /* Create queue for capture path */
    m_yuvCaptureDoneQ = new frame_queue_t;
    m_yuvCaptureDoneQ->setWaitTime(2000000000);
    m_reprocessingDoneQ = new frame_queue_t;
    m_reprocessingDoneQ->setWaitTime(2000000000);

    m_frameFactoryQ = new framefactory3_queue_t;
    m_selectBayerQ = new frame_queue_t();
    m_duplicateBufferDoneQ = new frame_queue_t(m_duplicateBufferThread);
    m_captureQ = new frame_queue_t(m_captureThread);

    /* construct static meta data information */
    if (ExynosCamera3MetadataConverter::constructStaticInfo(cameraId, info))
        CLOGE("Create static meta data failed!!");

    m_metadataConverter->setStaticInfo(cameraId, *info);

    m_streamManager->setYuvStreamMaxCount(m_parameters->getYuvStreamMaxNum());

    m_setFrameManager();

    m_setConfigInform();

    m_constructFrameFactory();

    /* init infomation of fd orientation*/
    m_parameters->setDeviceOrientation(0);
    ExynosCameraActivityUCTL *uctlMgr = m_activityControl->getUCTLMgr();
    if (uctlMgr != NULL)
        uctlMgr->setDeviceRotation(m_parameters->getFdOrientation());

    /* Setup FrameFactory to RequestManager*/
    m_setupFrameFactoryToRequest();

    /* HACK : check capture stream */
    m_captureStreamExist = false;
    isRestarted = false;

    m_videoStreamExist = false;
    m_recordingEnabled = false;
    m_checkConfigStream = false;
    m_flushFlag = false;
    m_flushWaitEnable = false;
    m_internalFrameCount = FRAME_INTERNAL_START_COUNT;
    m_isNeedInternalFrame = false;
    m_isNeedRequestFrame = false;
    m_currentShot = new struct camera2_shot_ext;
    memset(m_currentShot, 0x00, sizeof(struct camera2_shot_ext));
    m_internalFrameDoneQ = new frame_queue_t;
#ifdef MONITOR_LOG_SYNC
    m_syncLogDuration = 0;
#endif
    m_flagRunFastAE = false;
    m_lastFrametime = 0;
#ifdef YUV_DUMP
    m_dumpFrameQ = new frame_queue_t(m_dumpThread);
#endif
}

status_t  ExynosCamera3::m_setConfigInform() {
    struct ExynosConfigInfo exynosConfig;
    memset((void *)&exynosConfig, 0x00, sizeof(exynosConfig));

    exynosConfig.mode = CONFIG_MODE::NORMAL;

    /* Internal buffers */
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_sensor_buffers = NUM_SENSOR_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_3aa_buffers = NUM_3AA_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_hwdis_buffers = NUM_HW_DIS_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_reprocessing_buffers = NUM_REPROCESSING_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_fastaestable_buffer = NUM_FASTAESTABLE_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.reprocessing_bayer_hold_count = REPROCESSING_BAYER_HOLD_COUNT;
#ifdef USE_VRA_GROUP
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_vra_buffers = NUM_VRA_BUFFERS;
#endif
    /* Service buffers */
#if 0 /* Consumer's buffer counts are not fixed */
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_bayer_buffers = NUM_REQUEST_RAW_BUFFER + NUM_SERVICE_RAW_BUFFER;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_preview_buffers = NUM_REQUEST_PREVIEW_BUFFER + NUM_SERVICE_PREVIEW_BUFFER;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_preview_cb_buffers = NUM_REQUEST_CALLBACK_BUFFER + NUM_SERVICE_CALLBACK_BUFFER;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_recording_buffers = NUM_REQUEST_VIDEO_BUFFER + NUM_SERVICE_VIDEO_BUFFER;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_picture_buffers = NUM_REQUEST_JPEG_BUFFER + NUM_SERVICE_JPEG_BUFFER;
#else
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_bayer_buffers = VIDEO_MAX_FRAME - NUM_SENSOR_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_preview_buffers = VIDEO_MAX_FRAME;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_preview_cb_buffers = VIDEO_MAX_FRAME;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_recording_buffers = VIDEO_MAX_FRAME;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_picture_buffers = VIDEO_MAX_FRAME;
#endif
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_request_raw_buffers = NUM_REQUEST_RAW_BUFFER;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_request_preview_buffers = NUM_REQUEST_PREVIEW_BUFFER;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_request_callback_buffers = NUM_REQUEST_CALLBACK_BUFFER;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_request_video_buffers = NUM_REQUEST_VIDEO_BUFFER;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_request_jpeg_buffers = NUM_REQUEST_JPEG_BUFFER;
    /* Blocking request */
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_min_block_request = NUM_REQUEST_BLOCK_MIN;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_max_block_request = NUM_REQUEST_BLOCK_MAX;
    /* Prepare buffers */
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_FLITE] = PIPE_FLITE_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_3AA] = PIPE_3AA_PREPARE_COUNT;

#if (USE_HIGHSPEED_RECORDING)
    /* Config HIGH_SPEED 120 buffer & pipe info */
    /* Internal buffers */
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_sensor_buffers = FPS120_NUM_SENSOR_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_3aa_buffers = FPS120_NUM_3AA_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_hwdis_buffers = FPS120_NUM_HW_DIS_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_reprocessing_buffers = FPS120_NUM_REPROCESSING_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_fastaestable_buffer = FPS120_NUM_FASTAESTABLE_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.reprocessing_bayer_hold_count = FPS120_REPROCESSING_BAYER_HOLD_COUNT;
#ifdef USE_VRA_GROUP
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_vra_buffers = NUM_VRA_BUFFERS;
#endif
    /* Service buffers */
#if 0 /* Consumer's buffer counts are not fixed */
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_bayer_buffers = FPS120_NUM_REQUEST_RAW_BUFFER + FPS120_NUM_SERVICE_RAW_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_preview_buffers = FPS120_NUM_REQUEST_PREVIEW_BUFFER + FPS120_NUM_SERVICE_PREVIEW_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_preview_cb_buffers = FPS120_NUM_REQUEST_CALLBACK_BUFFER + FPS120_NUM_SERVICE_CALLBACK_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_recording_buffers = FPS120_NUM_REQUEST_VIDEO_BUFFER + FPS120_NUM_SERVICE_VIDEO_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_picture_buffers = FPS120_NUM_REQUEST_JPEG_BUFFER + FPS120_NUM_SERVICE_JPEG_BUFFER;
#else
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_bayer_buffers = VIDEO_MAX_FRAME - FPS120_NUM_SENSOR_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_preview_buffers = VIDEO_MAX_FRAME;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_preview_cb_buffers = VIDEO_MAX_FRAME;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_recording_buffers = VIDEO_MAX_FRAME;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_picture_buffers = VIDEO_MAX_FRAME;
#endif
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_request_raw_buffers = FPS120_NUM_REQUEST_RAW_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_request_preview_buffers = FPS120_NUM_REQUEST_PREVIEW_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_request_callback_buffers = FPS120_NUM_REQUEST_CALLBACK_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_request_video_buffers = FPS120_NUM_REQUEST_VIDEO_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_request_jpeg_buffers = FPS120_NUM_REQUEST_JPEG_BUFFER;
    /* Blocking request */
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_min_block_request = FPS120_NUM_REQUEST_BLOCK_MIN;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_max_block_request = FPS120_NUM_REQUEST_BLOCK_MAX;
    /* Prepare buffers */
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].pipeInfo.prepare[PIPE_FLITE] = FPS120_PIPE_FLITE_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].pipeInfo.prepare[PIPE_3AA] = FPS120_PIPE_3AA_PREPARE_COUNT;

    /* Config HIGH_SPEED 240 buffer & pipe info */
    /* Internal buffers */
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_sensor_buffers = FPS240_NUM_SENSOR_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_3aa_buffers = FPS240_NUM_3AA_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_hwdis_buffers = FPS240_NUM_HW_DIS_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_reprocessing_buffers = FPS240_NUM_REPROCESSING_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_fastaestable_buffer = FPS240_NUM_FASTAESTABLE_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.reprocessing_bayer_hold_count = FPS240_REPROCESSING_BAYER_HOLD_COUNT;
#ifdef USE_VRA_GROUP
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_vra_buffers = NUM_VRA_BUFFERS;
#endif
    /* Service buffers */
    /* Consumer's buffer counts are not fixed */
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_bayer_buffers = VIDEO_MAX_FRAME - FPS240_NUM_SENSOR_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_preview_buffers = VIDEO_MAX_FRAME;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_preview_cb_buffers = VIDEO_MAX_FRAME;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_recording_buffers = VIDEO_MAX_FRAME;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_picture_buffers = VIDEO_MAX_FRAME;

    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_request_raw_buffers = FPS240_NUM_REQUEST_RAW_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_request_preview_buffers = FPS240_NUM_REQUEST_PREVIEW_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_request_callback_buffers = FPS240_NUM_REQUEST_CALLBACK_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_request_video_buffers = FPS240_NUM_REQUEST_VIDEO_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_request_jpeg_buffers = FPS240_NUM_REQUEST_JPEG_BUFFER;
    /* Blocking request */
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_min_block_request = FPS240_NUM_REQUEST_BLOCK_MIN;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_max_block_request = FPS240_NUM_REQUEST_BLOCK_MAX;
    /* Prepare buffers */
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].pipeInfo.prepare[PIPE_FLITE] = FPS240_PIPE_FLITE_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].pipeInfo.prepare[PIPE_3AA] = FPS240_PIPE_3AA_PREPARE_COUNT;
#endif

    m_parameters->setConfig(&exynosConfig);

    m_exynosconfig = m_parameters->getConfig();

    return NO_ERROR;
}

status_t ExynosCamera3::m_setupFrameFactoryToRequest()
{
    status_t ret = NO_ERROR;
    ExynosCamera3FrameFactory *factory = NULL;
#if !defined(ENABLE_FULL_FRAME)
    ExynosCamera3FrameFactory *factoryReprocessing = NULL;
#endif

    factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
#if defined(ENABLE_FULL_FRAME)
    if (factory != NULL) {
        m_requestMgr->setRequestsInfo(HAL_STREAM_ID_PREVIEW, factory);
        m_requestMgr->setRequestsInfo(HAL_STREAM_ID_VIDEO, factory);
        m_requestMgr->setRequestsInfo(HAL_STREAM_ID_CALLBACK, factory);
        m_requestMgr->setRequestsInfo(HAL_STREAM_ID_RAW, factory);
        m_requestMgr->setRequestsInfo(HAL_STREAM_ID_JPEG, factory);
        m_requestMgr->setRequestsInfo(HAL_STREAM_ID_ZSL_OUTPUT, factory);
    } else {
        CLOGE("FRAME_FACTORY_TYPE_CAPTURE_PREVIEW factory is NULL!!!!");
    }
#else
    factoryReprocessing = m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING];

    if (factory != NULL && factoryReprocessing != NULL) {
        if(m_parameters->isSingleChain()) {
            m_requestMgr->setRequestsInfo(HAL_STREAM_ID_PREVIEW, factory, factoryReprocessing);
            m_requestMgr->setRequestsInfo(HAL_STREAM_ID_VIDEO, factory, factoryReprocessing);
            m_requestMgr->setRequestsInfo(HAL_STREAM_ID_CALLBACK, factory, factoryReprocessing);
            m_requestMgr->setRequestsInfo(HAL_STREAM_ID_RAW, factory, factory);
            m_requestMgr->setRequestsInfo(HAL_STREAM_ID_ZSL_OUTPUT, factory, factory);

            m_requestMgr->setRequestsInfo(HAL_STREAM_ID_JPEG, factoryReprocessing, factoryReprocessing);
            m_requestMgr->setRequestsInfo(HAL_STREAM_ID_CALLBACK_STALL, factoryReprocessing, factoryReprocessing);
        } else {
            m_requestMgr->setRequestsInfo(HAL_STREAM_ID_PREVIEW, factory);
            m_requestMgr->setRequestsInfo(HAL_STREAM_ID_VIDEO, factory);
            m_requestMgr->setRequestsInfo(HAL_STREAM_ID_CALLBACK, factory);
            m_requestMgr->setRequestsInfo(HAL_STREAM_ID_RAW, factory);
            m_requestMgr->setRequestsInfo(HAL_STREAM_ID_ZSL_OUTPUT, factory);

            m_requestMgr->setRequestsInfo(HAL_STREAM_ID_JPEG, factoryReprocessing);
            m_requestMgr->setRequestsInfo(HAL_STREAM_ID_ZSL_INPUT, factoryReprocessing);
            m_requestMgr->setRequestsInfo(HAL_STREAM_ID_CALLBACK_STALL, factoryReprocessing);
        }
    } else {
        if(factory == NULL) {
            CLOGE("FRAME_FACTORY_TYPE_CAPTURE_PREVIEW factory is NULL!!!!");
        }
        if(factoryReprocessing == NULL) {
            CLOGE("FRAME_FACTORY_TYPE_REPROCESSING factory is NULL!!!!");
        }
    }

#endif

    return ret;
}

status_t ExynosCamera3::m_setStreamInfo(camera3_stream_configuration *streamList)
{
    int ret = OK;
    int id = 0;

    CLOGD("In");

    /* sanity check */
    if (streamList == NULL) {
        CLOGE("NULL stream configuration");
        return BAD_VALUE;
    }

    if (streamList->streams == NULL) {
        CLOGE("NULL stream list");
        return BAD_VALUE;
    }

    if (streamList->num_streams < 1) {
        CLOGE(" Bad number of streams requested: %d", streamList->num_streams);
        return BAD_VALUE;
    }

    /* check input stream */
    bool inputStreamExist = false;
    for (size_t i = 0; i < streamList->num_streams; i++) {
        camera3_stream_t *newStream = streamList->streams[i];
        if (newStream == NULL) {
            CLOGE("Stream index %zu was NULL", i);
            return BAD_VALUE;
        }
        // for debug
        CLOGD("Stream(%p), ID(%zu), type(%d), usage(%#x) format(%#x) w(%d),h(%d)",
            newStream, i, newStream->stream_type, newStream->usage,
            newStream->format, newStream->width, newStream->height);

        if ((newStream->stream_type == CAMERA3_STREAM_INPUT) ||
            (newStream->stream_type == CAMERA3_STREAM_BIDIRECTIONAL)) {
            if (inputStreamExist == true) {
                CLOGE(" Multiple input streams requested!");
                return BAD_VALUE;
            }
            inputStreamExist = true;
        }

        /* HACK : check capture stream */
        if (newStream->format == 0x21)
            m_captureStreamExist = true;

        /* HACK : check recording stream */
        if ((newStream->format == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED)
            && (newStream->usage & GRALLOC_USAGE_HW_VIDEO_ENCODER)) {
            CLOGI("recording stream checked");
            m_videoStreamExist = true;
        }

        // TODO: format validation
    }

    /* 1. Invalidate all current streams */
    List<int> keylist;
    List<int>::iterator iter;
    ExynosCameraStream *stream = NULL;
    keylist.clear();
    m_streamManager->getStreamKeys(&keylist);
    for (iter = keylist.begin(); iter != keylist.end(); iter++) {
        m_streamManager->getStream(*iter, &stream);
        stream->setRegisterStream(EXYNOS_STREAM::HAL_STREAM_STS_INVALID);
        stream->setRegisterBuffer(EXYNOS_STREAM::HAL_STREAM_STS_UNREGISTERED);
    }

    /* 2. Remove dead streams */
    keylist.clear();
    stream = NULL;
    id = 0;
    EXYNOS_STREAM::STATE registerStream = EXYNOS_STREAM::HAL_STREAM_STS_INIT;
    m_streamManager->getStreamKeys(&keylist);
    for (iter = keylist.begin(); iter != keylist.end(); iter++) {
        m_streamManager->getStream(*iter, &stream);
        ret = stream->getRegisterStream(&registerStream);
        if (registerStream == EXYNOS_STREAM::HAL_STREAM_STS_INVALID){
            ret = stream->getID(&id);
            if (id < 0) {
                CLOGE("getID failed id(%d)", id);
                continue;
            }
            m_streamManager->deleteStream(id);
        }
    }

    /* 3. Update stream info */
    for (size_t i = 0; i < streamList->num_streams; i++) {
        stream = NULL;
        camera3_stream_t *newStream = streamList->streams[i];
        if (newStream->priv == NULL) {
            /* new stream case */
            ret = m_enumStreamInfo(newStream);
            if (ret) {
                CLOGE("Register stream failed %p", newStream);
                return ret;
            }
        } else {
            /* Existing stream, reuse current stream */
            stream = static_cast<ExynosCameraStream*>(newStream->priv);
            stream->setRegisterStream(EXYNOS_STREAM::HAL_STREAM_STS_VALID);
        }
    }

    CLOGD("out");
    return ret;
}

status_t ExynosCamera3::m_enumStreamInfo(camera3_stream_t *stream)
{
    CLOGD("In");
    int ret = OK;
    ExynosCameraStream *newStream = NULL;
    int id = 0;
    int actualFormat = 0;
    int planeCount = 0;
    int requestBuffer = 0;
    int outputPortId = 0;
    EXYNOS_STREAM::STATE registerStream;
    EXYNOS_STREAM::STATE registerBuffer;

    registerStream = EXYNOS_STREAM::HAL_STREAM_STS_VALID;
    registerBuffer = EXYNOS_STREAM::HAL_STREAM_STS_UNREGISTERED;

    if (stream == NULL) {
        CLOGE("stream is NULL.");
        return INVALID_OPERATION;
    }

    /* Update gralloc usage */
    switch (stream->stream_type) {
    case CAMERA3_STREAM_INPUT:
        stream->usage |= GRALLOC_USAGE_HW_CAMERA_READ;
        break;
    case CAMERA3_STREAM_OUTPUT:
        stream->usage |= GRALLOC_USAGE_HW_CAMERA_WRITE;
        break;
    case CAMERA3_STREAM_BIDIRECTIONAL:
        stream->usage |= (GRALLOC_USAGE_HW_CAMERA_READ | GRALLOC_USAGE_HW_CAMERA_WRITE);
        break;
    default:
        CLOGE("Invalid stream_type %d",   stream->stream_type);
        break;
    }

    switch (stream->stream_type) {
    case CAMERA3_STREAM_OUTPUT:
        // TODO: split this routine to function
        switch (stream->format) {
        case HAL_PIXEL_FORMAT_BLOB:
            CLOGD("HAL_PIXEL_FORMAT_BLOB format(%#x) usage(%#x) stream_type(%#x)",
                 stream->format, stream->usage, stream->stream_type);
            id = HAL_STREAM_ID_JPEG;
            actualFormat = HAL_PIXEL_FORMAT_BLOB;
            planeCount = 1;
            outputPortId = 0;
            requestBuffer = m_exynosconfig->current->bufInfo.num_request_jpeg_buffers;
            break;
        case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
            if (stream->usage & GRALLOC_USAGE_HW_TEXTURE || stream->usage & GRALLOC_USAGE_HW_COMPOSER) {
                CLOGD("GRALLOC_USAGE_HW_TEXTURE foramt(%#x) usage(%#x) stream_type(%#x)",
                     stream->format, stream->usage, stream->stream_type);
                id = HAL_STREAM_ID_PREVIEW;
                actualFormat = HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M;
                planeCount = 2;
                outputPortId = m_streamManager->getTotalYuvStreamCount();
                requestBuffer = m_exynosconfig->current->bufInfo.num_request_preview_buffers;
            } else if(stream->usage & GRALLOC_USAGE_HW_VIDEO_ENCODER) {
                CLOGD("GRALLOC_USAGE_HW_VIDEO_ENCODER format(%#x) usage(%#x) stream_type(%#x)",
                     stream->format, stream->usage, stream->stream_type);
                id = HAL_STREAM_ID_VIDEO;
                actualFormat = HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M; /* NV12M */
                planeCount = 2;
                outputPortId = m_streamManager->getTotalYuvStreamCount();
                requestBuffer = m_exynosconfig->current->bufInfo.num_request_video_buffers;
            } else if(stream->usage & GRALLOC_USAGE_HW_CAMERA_ZSL) {
                CLOGD("GRALLOC_USAGE_HW_CAMERA_ZSL format(%#x) usage(%#x) stream_type(%#x)",
                     stream->format, stream->usage, stream->stream_type);
                id = HAL_STREAM_ID_ZSL_OUTPUT;
                actualFormat = HAL_PIXEL_FORMAT_RAW16;
                planeCount = 1;
                outputPortId = 0;
                requestBuffer = m_exynosconfig->current->bufInfo.num_request_raw_buffers;
            } else {
                CLOGE("HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED unknown usage(%#x)"
                    " format(%#x) stream_type(%#x)",
                      stream->usage, stream->format, stream->stream_type);
                ret = BAD_VALUE;
                goto func_err;
            }
             break;
        case HAL_PIXEL_FORMAT_YCbCr_420_888:
            CLOGD("HAL_PIXEL_FORMAT_YCbCr_420_888 format(%#x) usage(%#x) stream_type(%#x)",
                    stream->format, stream->usage, stream->stream_type);
            id = HAL_STREAM_ID_CALLBACK;
            outputPortId = m_streamManager->getTotalYuvStreamCount();

            actualFormat = HAL_PIXEL_FORMAT_YCrCb_420_SP;
            planeCount = 1;
            requestBuffer = m_exynosconfig->current->bufInfo.num_request_callback_buffers;
            break;
        /* case HAL_PIXEL_FORMAT_RAW_SENSOR: */
        case HAL_PIXEL_FORMAT_RAW16:
            CLOGD("HAL_PIXEL_FORMAT_RAW_XXX format(%#x) usage(%#x) stream_type(%#x)",
                 stream->format, stream->usage, stream->stream_type);
            id = HAL_STREAM_ID_RAW;
            actualFormat = HAL_PIXEL_FORMAT_RAW16;
            planeCount = 1;
            outputPortId = 0;
            requestBuffer = m_exynosconfig->current->bufInfo.num_request_raw_buffers;
            break;
        default:
            CLOGE("Not supported image format(%#x) usage(%#x) stream_type(%#x)",
                 stream->format, stream->usage, stream->stream_type);
            ret = BAD_VALUE;
            goto func_err;
            break;
        }
        break;
    case CAMERA3_STREAM_INPUT:
    case CAMERA3_STREAM_BIDIRECTIONAL:
        switch (stream->format) {
        /* case HAL_PIXEL_FORMAT_RAW_SENSOR: */
        case HAL_PIXEL_FORMAT_RAW16:
        case HAL_PIXEL_FORMAT_RAW_OPAQUE:
            CLOGD("HAL_PIXEL_FORMAT_RAW_XXX format(%#x) usage(%#x) stream_type(%#x)",
                 stream->format, stream->usage, stream->stream_type);
            id = HAL_STREAM_ID_RAW;
            actualFormat = HAL_PIXEL_FORMAT_RAW16;
            planeCount = 1;
            requestBuffer = m_exynosconfig->current->bufInfo.num_request_raw_buffers;
            break;
        case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
            if (stream->usage & GRALLOC_USAGE_HW_CAMERA_ZSL) {
                CLOGD("GRALLOC_USAGE_HW_CAMERA_ZSL foramt(%#x) usage(%#x) stream_type(%#x)",
                     stream->format, stream->usage, stream->stream_type);
                id = HAL_STREAM_ID_ZSL_INPUT;
                actualFormat = HAL_PIXEL_FORMAT_RAW16;
                planeCount = 1;
                requestBuffer = m_exynosconfig->current->bufInfo.num_request_raw_buffers;
                break;
            } else {
                CLOGE("HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED unknown usage(%#x)"
                    " format(%#x) stream_type(%#x)",
                      stream->usage, stream->format, stream->stream_type);
                ret = BAD_VALUE;
                goto func_err;
            }
            break;
        default:
            CLOGE("Not supported image format(%#x) usage(%#x) stream_type(%#x)",
                 stream->format, stream->usage, stream->stream_type);
            goto func_err;
        }
        break;
    default:
        CLOGE("Unknown stream_type(%#x) format(%#x) usage(%#x)",
             stream->stream_type, stream->format, stream->usage);
        ret = BAD_VALUE;
        goto func_err;
    }

    newStream = m_streamManager->createStream(id, stream);
    if (newStream == NULL) {
        CLOGE("createStream is NULL id(%d)", id);
        goto func_err;
    }

    newStream->setRegisterStream(registerStream);
    newStream->setRegisterBuffer(registerBuffer);
    newStream->setFormat(actualFormat);
    newStream->setPlaneCount(planeCount);
    newStream->setOutputPortId(outputPortId);
    newStream->setRequestBuffer(requestBuffer);

func_err:
    CLOGD("Out");

    return ret;

}

void ExynosCamera3::m_createManagers(void)
{
    if (!m_streamManager) {
        m_streamManager = new ExynosCameraStreamManager(m_cameraId);
        CLOGD("Stream Manager created");
    }

    /* Create buffer manager */
    if (!m_fliteBufferMgr)
        m_createInternalBufferManager(&m_fliteBufferMgr, "FLITE_BUF");
    if (!m_3aaBufferMgr)
        m_createInternalBufferManager(&m_3aaBufferMgr, "3AA_BUF");
    if (!m_ispBufferMgr)
        m_createInternalBufferManager(&m_ispBufferMgr, "ISP_BUF");
    if (!m_mcscBufferMgr)
        m_createInternalBufferManager(&m_mcscBufferMgr, "MCSC_BUF");
#ifdef SUPPORT_DEPTH_MAP
    if (!m_depthMapBufferMgr)
        m_createInternalBufferManager(&m_depthMapBufferMgr, "DEPTH_MAP_BUF");
#endif
#ifdef USE_VRA_GROUP
    if (!m_vraBufferMgr)
        m_createInternalBufferManager(&m_vraBufferMgr, "DS_OUT_BUF");
#endif
    if (!m_ispReprocessingBufferMgr)
        m_createInternalBufferManager(&m_ispReprocessingBufferMgr, "ISP_RE_BUF");
    if (!m_yuvCaptureBufferMgr)
        m_createInternalBufferManager(&m_yuvCaptureBufferMgr, "YUV_CAPTURE_BUF");
    if (!m_thumbnailBufferMgr)
        m_createInternalBufferManager(&m_thumbnailBufferMgr, "THUMBNAIL_BUF");
    if(m_parameters->isSupportZSLInput()) {
        if (!m_internalScpBufferMgr)
            m_createInternalBufferManager(&m_internalScpBufferMgr, "INTERNAL_SCP_BUF");
    }

}

void ExynosCamera3::m_createThreads(void)
{
    CLOGD("Creating threads");
    m_mainThread = new mainCameraThread(this, &ExynosCamera3::m_mainThreadFunc, "m_mainThreadFunc");
    CLOGV("Main thread created");

    /* m_previewStreamXXXThread is for seperated frameDone each own handler */
    m_previewStreamBayerThread = new mainCameraThread(this, &ExynosCamera3::m_previewStreamBayerPipeThreadFunc, "PreviewBayerThread");
    CLOGV("Bayer Preview stream thread created");

    m_previewStream3AAThread = new mainCameraThread(this, &ExynosCamera3::m_previewStream3AAPipeThreadFunc, "Preview3AAThread");
    CLOGV("3AA Preview stream thread created");

    m_previewStreamISPThread = new mainCameraThread(this, &ExynosCamera3::m_previewStreamISPPipeThreadFunc, "PreviewISPThread");
    CLOGV("ISP Preview stream thread created");

    m_previewStreamMCSCThread = new mainCameraThread(this, &ExynosCamera3::m_previewStreamMCSCPipeThreadFunc, "PreviewISPThread");
    CLOGV("MCSC Preview stream thread created");

#ifdef USE_VRA_GROUP
    m_previewStreamVRAThread = new mainCameraThread(this, &ExynosCamera3::m_previewStreamVRAPipeThreadFunc, "PreviewVRAThread");
    CLOGV("VRA Preview stream thread created");
#endif

    m_selectBayerThread = new mainCameraThread(this, &ExynosCamera3::m_selectBayerThreadFunc, "SelectBayerThreadFunc");
    CLOGV("SelectBayerThread created");

    m_captureThread = new mainCameraThread(this, &ExynosCamera3::m_captureThreadFunc, "CaptureThreadFunc");
    CLOGV("CaptureThread created");

    m_captureStreamThread = new mainCameraThread(this, &ExynosCamera3::m_captureStreamThreadFunc, "CaptureThread");
    CLOGV("Capture stream thread created");

    m_setBuffersThread = new mainCameraThread(this, &ExynosCamera3::m_setBuffersThreadFunc, "setBuffersThread");
    CLOGV("Buffer allocation thread created");

    m_duplicateBufferThread = new mainCameraThread(this, &ExynosCamera3::m_duplicateBufferThreadFunc, "DuplicateThread");
    CLOGV("Duplicate buffer thread created");

    m_framefactoryCreateThread = new mainCameraThread(this, &ExynosCamera3::m_frameFactoryCreateThreadFunc, "FrameFactoryCreateThread");
    CLOGV("FrameFactoryCreateThread created");

    m_reprocessingFrameFactoryStartThread = new mainCameraThread(this, &ExynosCamera3::m_reprocessingFrameFactoryStartThreadFunc, "m_reprocessingFrameFactoryStartThread");
    CLOGV("m_reprocessingFrameFactoryStartThread created");

    m_startPictureBufferThread = new mainCameraThread(this, &ExynosCamera3::m_startPictureBufferThreadFunc, "startPictureBufferThread");
    CLOGV("startPictureBufferThread created");

    m_frameFactoryStartThread = new mainCameraThread(this, &ExynosCamera3::m_frameFactoryStartThreadFunc, "FrameFactoryStartThread");
    CLOGV("FrameFactoryStartThread created");

    m_internalFrameThread = new mainCameraThread(this, &ExynosCamera3::m_internalFrameThreadFunc, "InternalFrameThread");
    CLOGV("Internal Frame Handler Thread created");

    m_monitorThread = new mainCameraThread(this, &ExynosCamera3::m_monitorThreadFunc, "MonitorThread");
    CLOGV("MonitorThread created");

#ifdef YUV_DUMP
    m_dumpThread = new mainCameraThread(this, &ExynosCamera3::m_dumpThreadFunc, "m_dumpThreadFunc");
    CLOGV("DumpThread created");
#endif
    CLOGD("Creating threads done");
}

ExynosCamera3::~ExynosCamera3()
{
    this->release();
}

void ExynosCamera3::release()
{
    CLOGI("-IN-");

    m_stopCompanion();
    m_checkConfigStream = false;
    m_releaseBuffers();

    if (m_fliteBufferMgr != NULL) {
        delete m_fliteBufferMgr;
        m_fliteBufferMgr = NULL;
    }
    if (m_3aaBufferMgr != NULL) {
        delete m_3aaBufferMgr;
        m_3aaBufferMgr = NULL;
    }
    if (m_ispBufferMgr != NULL) {
        delete m_ispBufferMgr;
        m_ispBufferMgr = NULL;
    }
    if (m_mcscBufferMgr != NULL) {
        delete m_mcscBufferMgr;
        m_mcscBufferMgr = NULL;
    }
#ifdef USE_VRA_GROUP
    if (m_vraBufferMgr != NULL) {
        delete m_vraBufferMgr;
        m_vraBufferMgr = NULL;
    }
#endif
    if (m_ispReprocessingBufferMgr != NULL) {
        delete m_ispReprocessingBufferMgr;
        m_ispReprocessingBufferMgr = NULL;
    }
    if (m_yuvCaptureBufferMgr != NULL) {
        delete m_yuvCaptureBufferMgr;
        m_yuvCaptureBufferMgr = NULL;
    }

    if (m_thumbnailBufferMgr != NULL) {
        delete m_thumbnailBufferMgr;
        m_thumbnailBufferMgr = NULL;
    }

    if (m_internalScpBufferMgr != NULL) {
        delete m_internalScpBufferMgr;
        m_internalScpBufferMgr = NULL;
    }

    if (m_bayerBufferMgr!= NULL) {
        delete m_bayerBufferMgr;
        m_bayerBufferMgr = NULL;
    }

    if (m_zslBufferMgr != NULL) {
        delete m_zslBufferMgr;
        m_zslBufferMgr = NULL;
    }

#ifdef SUPPORT_DEPTH_MAP
    if (m_depthMapBufferMgr != NULL) {
        delete m_depthMapBufferMgr;
        m_depthMapBufferMgr = NULL;
    }
#endif

    if (m_ionAllocator != NULL) {
        delete m_ionAllocator;
        m_ionAllocator = NULL;
    }

    if (m_shotDoneQ != NULL) {
        delete m_shotDoneQ;
        m_shotDoneQ = NULL;
    }

    for (int i = 0; i < MAX_PIPE_NUM; i++) {
        if (m_pipeFrameDoneQ[i] != NULL) {
            delete m_pipeFrameDoneQ[i];
            m_pipeFrameDoneQ[i] = NULL;
        }
    }

    if (m_duplicateBufferDoneQ != NULL) {
        delete m_duplicateBufferDoneQ;
        m_duplicateBufferDoneQ = NULL;
    }

    if (m_yuvCaptureDoneQ != NULL) {
        delete m_yuvCaptureDoneQ;
        m_yuvCaptureDoneQ = NULL;
    }

    if (m_reprocessingDoneQ != NULL) {
        delete m_reprocessingDoneQ;
        m_reprocessingDoneQ = NULL;
    }

    if (m_internalFrameDoneQ != NULL) {
        delete m_internalFrameDoneQ;
        m_internalFrameDoneQ = NULL;
    }

    if (m_frameFactoryQ != NULL) {
        delete m_frameFactoryQ;
        m_frameFactoryQ = NULL;
    }

    if (m_selectBayerQ != NULL) {
        delete m_selectBayerQ;
        m_selectBayerQ = NULL;
    }

    if (m_captureQ != NULL) {
        delete m_captureQ;
        m_captureQ = NULL;
    }

#ifdef YUV_DUMP
    if (m_dumpFrameQ != NULL) {
        delete m_dumpFrameQ;
        m_dumpFrameQ = NULL;
    }
#endif

    if (m_frameMgr != NULL) {
        delete m_frameMgr;
        m_frameMgr = NULL;
    }

    if (m_streamManager != NULL) {
        delete m_streamManager;
        m_streamManager = NULL;
    }

    if (m_requestMgr!= NULL) {
        delete m_requestMgr;
        m_requestMgr = NULL;
    }

    m_deinitFrameFactory();

    if (m_parameters != NULL) {
        delete m_parameters;
        m_parameters = NULL;
    }

    if (m_metadataConverter != NULL) {
        delete m_metadataConverter;
        m_metadataConverter = NULL;
    }

    if (m_captureSelector != NULL) {
        delete m_captureSelector;
        m_captureSelector = NULL;
    }

    if (m_captureZslSelector != NULL) {
        delete m_captureZslSelector;
        m_captureZslSelector = NULL;
    }

    if (m_currentShot != NULL) {
        delete m_currentShot;
        m_currentShot = NULL;
    }

    // TODO: clean up
    // m_resultBufferVectorSet
    // m_processList
    // m_postProcessList
    // m_pipeFrameDoneQ
    CLOGI("-OUT-");
}

status_t ExynosCamera3::initilizeDevice(const camera3_callback_ops *callbackOps)
{
    status_t ret = NO_ERROR;
    CLOGD("");

    /* set callback ops */
    m_requestMgr->setCallbackOps(callbackOps);

#ifdef SUPPORT_DEPTH_MAP
    m_parameters->checkUseDepthMap();
#endif

    if (m_parameters->isReprocessing() == true) {
        if (m_captureSelector == NULL) {
            m_captureSelector = new ExynosCameraFrameSelector(m_cameraId, m_parameters, m_fliteBufferMgr, m_frameMgr);
            ret = m_captureSelector->setFrameHoldCount(REPROCESSING_BAYER_HOLD_COUNT);
            if (ret < 0) {
                CLOGE("m_captureSelector setFrameHoldCount(%d) is fail",
                     REPROCESSING_BAYER_HOLD_COUNT);
            }
        }

        if (m_captureZslSelector == NULL) {
            m_captureZslSelector = new ExynosCameraFrameSelector(m_cameraId, m_parameters, m_bayerBufferMgr, m_frameMgr);
            ret = m_captureZslSelector->setFrameHoldCount(REPROCESSING_BAYER_HOLD_COUNT);
            if (ret < 0) {
                CLOGE("m_captureZslSelector setFrameHoldCount(%d) is fail",
                     REPROCESSING_BAYER_HOLD_COUNT);
            }
        }
    }

    m_framefactoryCreateThread->run();
    m_frameMgr->start();

    m_startPictureBufferThread->run();

    /*
     * NOTICE: Join is to avoid dual scanario's problem.
     * The problem is that back camera's EOS was not finished, but front camera opened.
     * Two instance was actually different but driver accepts same instance.
     */
    m_framefactoryCreateThread->join();
    return ret;
}

status_t ExynosCamera3::releaseDevice(void)
{
    status_t ret = NO_ERROR;
    CLOGD("");

    m_setBuffersThread->requestExitAndWait();
    m_framefactoryCreateThread->requestExitAndWait();
    m_monitorThread->requestExitAndWait();

    if (m_flushFlag == false) {
        flush();
    }

    m_frameMgr->stop();
    m_frameMgr->deleteAllFrame();

    return ret;
}

status_t ExynosCamera3::construct_default_request_settings(camera_metadata_t **request, int type)
{
    Mutex::Autolock l(m_requestLock);

    CLOGD("Type %d", type);
    if ((type < 0) || (type >= CAMERA3_TEMPLATE_COUNT)) {
        CLOGE("Unknown request settings template: %d", type);
        return -ENODEV;
    }

    m_requestMgr->constructDefaultRequestSettings(type, request);

    CLOGI("out");
    return NO_ERROR;
}

status_t ExynosCamera3::configureStreams(camera3_stream_configuration *stream_list)
{
    Mutex::Autolock l(m_requestLock);

    status_t ret = NO_ERROR;
    EXYNOS_STREAM::STATE registerStreamState = EXYNOS_STREAM::HAL_STREAM_STS_INIT;
    EXYNOS_STREAM::STATE registerbufferState = EXYNOS_STREAM::HAL_STREAM_STS_INIT;
    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
    buffer_manager_allocation_mode_t allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
    int id = 0;
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES]    = {0};
    int width = 0, height = 0;
    int planeCount = 0;
    int streamPlaneCount = 0;
    int streamPixelFormat = 0;
    int maxBufferCount = 0;
    int outputPortId = 0;
    int startIndex = 0;
    int currentConfigMode = m_parameters->getConfigMode();

    CLOGD("In");

    /* sanity check for stream_list */
    if (stream_list == NULL) {
        CLOGE("NULL stream configuration");
        return BAD_VALUE;
    }

    if (stream_list->streams == NULL) {
        CLOGE("NULL stream list");
        return BAD_VALUE;
    }

    if (stream_list->num_streams < 1) {
        CLOGE("Bad number of streams requested: %d",
             stream_list->num_streams);
        return BAD_VALUE;
    }

    if (stream_list->operation_mode == CAMERA3_STREAM_CONFIGURATION_CONSTRAINED_HIGH_SPEED_MODE) {
        CLOGI("High speed mode is configured. StreamCount %d",
                 stream_list->num_streams);
        m_parameters->setHighSpeedMode(CONFIG_MODE::HIGHSPEED_240);
        m_exynosconfig = m_parameters->getConfig();
    } else {
        m_parameters->setHighSpeedMode(CONFIG_MODE::NORMAL);
        m_exynosconfig = m_parameters->getConfig();
    }

    /* Destory service buffermanager handled by ExynosCamera3
       Those managers will be created if necessary during the
       following stream enumeration */
    if(m_bayerBufferMgr != NULL) {
        delete m_bayerBufferMgr;
        m_bayerBufferMgr = NULL;
    }
    if(m_zslBufferMgr != NULL) {
        delete m_zslBufferMgr;
        m_zslBufferMgr = NULL;
    }

    ret = m_streamManager->setConfig(m_exynosconfig);
    if (ret) {
        CLOGE("setMaxBuffers() failed!!");
        return ret;
    }
    ret = m_setStreamInfo(stream_list);
    if (ret) {
        CLOGE("setStreams() failed!!");
        return ret;
    }

    /* flush request Mgr */
    m_requestMgr->flush();

    /* HACK :: restart frame factory */
    if (m_checkConfigStream == true
        || ((m_captureStreamExist == true) && (stream_list->num_streams == 1))
        || ((m_videoStreamExist == true) && (m_recordingEnabled == false))
        || ((m_videoStreamExist == false) && (m_recordingEnabled == true))) {
        CLOGI("restart frame factory m_captureStreamExist(%d),"
            " m_videoStreamExist(%d), stream_list->num_streams(%d)",
             m_captureStreamExist, m_videoStreamExist, stream_list->num_streams);

        m_captureStreamExist = false;
        /* In case of preview with Recording, enter this block even if not restart */
        if (m_checkConfigStream == true)
            isRestarted = true;

        m_recordingEnabled = false;
        m_videoStreamExist = false;

        m_stopReprocessingFrameFactory(m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING]);
        m_stopFrameFactory(m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW]);

        m_captureSelector->release();
        m_captureZslSelector->release();

        /* clear frame lists */
        m_removeInternalFrames(&m_processList, &m_processLock);
        m_clearList(&m_captureProcessList, &m_captureProcessLock);

        /* restart frame manager */
        m_frameMgr->stop();
        m_frameMgr->deleteAllFrame();
        m_frameMgr->start();

        m_startPictureBufferThread->join();
        m_setBuffersThread->join();

        ret = m_releaseBuffers();
        if (ret != NO_ERROR) {
            CLOGE("Failed to releaseBuffers. ret %d", ret);
        }

        m_checkConfigStream = false;
    }

    /* clear previous settings */
    ret = m_requestMgr->clearPrevRequest();
    if (ret) {
        CLOGE("clearPrevRequest() failed!!");
        return ret;
    }

    ret = m_requestMgr->clearPrevShot();
    if (ret < 0) {
        CLOGE("clearPrevShot() failed!! status(%d)", ret);
    }

    ret = m_parameters->initMetadata();
    if (ret < 0) {
        CLOGE("initMetadata() failed!! status(%d)", ret);
    }

    /* Create service buffer manager at each stream */
    for (size_t i = 0; i < stream_list->num_streams; i++) {
        registerStreamState = EXYNOS_STREAM::HAL_STREAM_STS_INIT;
        registerbufferState = EXYNOS_STREAM::HAL_STREAM_STS_INIT;
        id = -1;

        camera3_stream_t *newStream = stream_list->streams[i];
        ExynosCameraStream *privStreamInfo = static_cast<ExynosCameraStream*>(newStream->priv);
        privStreamInfo->getID(&id);
        width = newStream->width;
        height = newStream->height;

        CLOGI("list_index %zu streamId %d", i, id);
        CLOGD("stream_type %d", newStream->stream_type);
        CLOGD("size %dx%d", width, height);
        CLOGD("format %x", newStream->format);
        CLOGD("usage %x", newStream->usage);
        CLOGD("max_buffers %d", newStream->max_buffers);

        privStreamInfo->getRegisterStream(&registerStreamState);
        privStreamInfo->getRegisterBuffer(&registerbufferState);
        privStreamInfo->getPlaneCount(&streamPlaneCount);
        privStreamInfo->getFormat(&streamPixelFormat);

        if (registerStreamState == EXYNOS_STREAM::HAL_STREAM_STS_INVALID) {
            CLOGE("Invalid stream index %zu id %d",
                     i, id);
            ret = BAD_VALUE;
            break;
        }
        if (registerbufferState != EXYNOS_STREAM::HAL_STREAM_STS_UNREGISTERED) {
            CLOGE("privStreamInfo->registerBuffer state error!!");
            return BAD_VALUE;
        }

        CLOGD("registerStream %d registerbuffer %d",
             registerStreamState, registerbufferState);

        if ((registerStreamState == EXYNOS_STREAM::HAL_STREAM_STS_VALID) &&
            (registerbufferState == EXYNOS_STREAM::HAL_STREAM_STS_UNREGISTERED)) {
            ExynosCameraBufferManager *bufferMgr = NULL;
            switch (id % HAL_STREAM_ID_MAX) {
            case HAL_STREAM_ID_RAW:
                ret =  m_createServiceBufferManager(&m_bayerBufferMgr, "RAW_STREAM_BUF");
                if (ret < 0) {
                    CLOGE("m_createBufferManager() failed!!");
                    return ret;
                }
                CLOGD("Create buffer manager(RAW)");

                planeCount = streamPlaneCount + 1;
                planeSize[0] = width * height * 2;

                CLOGD("planeCount %d+1, planeSize[0] %d, bytesPerLine[0] %d",
                         streamPlaneCount, planeSize[0], bytesPerLine[0]);

                /* set startIndex as the next internal buffer index */
                startIndex = m_exynosconfig->current->bufInfo.num_sensor_buffers;
                maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers - startIndex;

                m_bayerBufferMgr->setAllocator(newStream);
                m_allocBuffers(m_bayerBufferMgr, planeCount, planeSize, bytesPerLine, startIndex, maxBufferCount, true, false);

                privStreamInfo->setRegisterBuffer(EXYNOS_STREAM::HAL_STREAM_STS_REGISTERED);
                /* bayer buffer is managed specially */
                /* privStreamInfo->setBufferManager(m_bayerBufferMgr); */
                CLOGD("m_bayerBufferMgr - %p", m_bayerBufferMgr);
                break;
            case HAL_STREAM_ID_ZSL_OUTPUT:
                CLOGD("Create buffer manager(ZSL)");
                ret = m_createServiceBufferManager(&m_zslBufferMgr, "ZSL_STREAM_BUF");
                if (ret < 0) {
                    CLOGE("m_createBufferManager() failed!!");
                    return ret;
                }

                planeCount = streamPlaneCount + 1;
                planeSize[0] = width * height * 2;

                CLOGD("planeCount %d+1, planeSize[0] %d, bytesPerLine[0] %d",
                         streamPlaneCount, planeSize[0], bytesPerLine[0]);

                /* set startIndex as the next internal buffer index */
                startIndex = m_exynosconfig->current->bufInfo.num_sensor_buffers;
                maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers - startIndex;

                m_zslBufferMgr->setAllocator(newStream);
                m_allocBuffers(m_zslBufferMgr, planeCount, planeSize, bytesPerLine, startIndex, maxBufferCount, true, false);

                privStreamInfo->setRegisterBuffer(EXYNOS_STREAM::HAL_STREAM_STS_REGISTERED);
                break;
            case HAL_STREAM_ID_ZSL_INPUT:
                /* Check support for ZSL input */
                if(m_parameters->isSupportZSLInput() == false) {
                    CLOGE(" ZSL input is not supported, but streamID [%d] is specified.",
                             id);
                    return INVALID_OPERATION;
                }

                planeCount = streamPlaneCount + 1;
                planeSize[0] = width * height * 2;

                CLOGD("planeCount %d+1, planeSize[0] %d, bytesPerLine[0] %d",
                         streamPlaneCount, planeSize[0], bytesPerLine[0]);

                privStreamInfo->setRegisterBuffer(EXYNOS_STREAM::HAL_STREAM_STS_REGISTERED);
                break;
            case HAL_STREAM_ID_PREVIEW:
                ret = privStreamInfo->getOutputPortId(&outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to getOutputPortId for PREVIEW stream");
                    return ret;
                }

                ret = m_parameters->checkYuvSize(width, height, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to checkYuvSize for PREVIEW stream. size %dx%d outputPortId %d",
                             width, height, outputPortId);
                    return ret;
                }

                ret = m_parameters->checkYuvFormat(streamPixelFormat, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to checkYuvFormat for PREVIEW stream. format %x outputPortId %d",
                             streamPixelFormat, outputPortId);
                    return ret;
                }

                m_parameters->setPreviewSize(width, height);

                maxBufferCount = m_exynosconfig->current->bufInfo.num_preview_buffers;
                ret = m_parameters->setYuvBufferCount(maxBufferCount, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to setYuvBufferCount for PREVIEW stream. maxBufferCount %d outputPortId %d",
                             maxBufferCount, outputPortId);
                    return ret;
                }

                CLOGD("Create buffer manager(PREVIEW)");
                ret =  m_createServiceBufferManager(&bufferMgr, "PREVIEW_STREAM_BUF");
                if (ret != NO_ERROR) {
                    CLOGE("m_createBufferManager() failed!!");
                    return ret;
                }

                planeCount = streamPlaneCount + 1;
                planeSize[0] = width * height;
                planeSize[1] = width * height / 2;

                CLOGD("planeCount %d+1, planeSize[0] %d, bytesPerLine[0] %d",
                         streamPlaneCount, planeSize[0], bytesPerLine[0]);

                bufferMgr->setAllocator(newStream);
                m_allocBuffers(bufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, true, false);

                privStreamInfo->setRegisterBuffer(EXYNOS_STREAM::HAL_STREAM_STS_REGISTERED);
                privStreamInfo->setBufferManager(bufferMgr);
                CLOGD("previewBufferMgr - %p",
                         bufferMgr);
                break;
            case HAL_STREAM_ID_VIDEO:
                ret = privStreamInfo->getOutputPortId(&outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to getOutputPortId for VIDEO stream");
                    return ret;
                }

                ret = m_parameters->checkYuvSize(width, height, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to checkYuvSize for VIDEO stream. size %dx%d outputPortId %d",
                             width, height, outputPortId);
                    return ret;
                }

                ret = m_parameters->checkYuvFormat(streamPixelFormat, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to checkYuvFormat for VIDEO stream. format %x outputPortId %d",
                             streamPixelFormat, outputPortId);
                    return ret;
                }

                maxBufferCount = m_exynosconfig->current->bufInfo.num_recording_buffers;
                ret = m_parameters->setYuvBufferCount(maxBufferCount, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to setYuvBufferCount for VIDEO stream. maxBufferCount %d outputPortId %d",
                             maxBufferCount, outputPortId);
                    return ret;
                }

                CLOGD("Create buffer manager(VIDEO)");
                ret =  m_createServiceBufferManager(&bufferMgr, "RECORDING_STREAM_BUF");
                if (ret != NO_ERROR) {
                    CLOGE("m_createBufferManager() failed!!");
                    return ret;
                }

                planeCount = streamPlaneCount + 1;
                planeSize[0] = width * height;
                planeSize[1] = width * height / 2;

                CLOGD("planeCount %d+1, planeSize[0] %d, bytesPerLine[0] %d",
                         streamPlaneCount, planeSize[0], bytesPerLine[0]);

                bufferMgr->setAllocator(newStream);
                m_allocBuffers(bufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, true, false);

                privStreamInfo->setRegisterBuffer(EXYNOS_STREAM::HAL_STREAM_STS_REGISTERED);
                privStreamInfo->setBufferManager(bufferMgr);
                CLOGD("recBufferMgr - %p",
                         bufferMgr);
                break;
            case HAL_STREAM_ID_JPEG:
                ret = m_parameters->checkPictureSize(width, height);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to checkPictureSize for JPEG stream. size %dx%d",
                             width, height);
                    return ret;
                }

                maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;

                CLOGD("Create buffer manager(JPEG)");
                ret =  m_createServiceBufferManager(&bufferMgr, "JPEG_STREAM_BUF");
                if (ret < 0) {
                    CLOGE("m_createBufferManager() failed!!");
                    return ret;
                }

                planeCount = streamPlaneCount;
                planeSize[0] = width * height * 2;

                CLOGD("planeCount %d+1, planeSize[0] %d, bytesPerLine[0] %d",
                         streamPlaneCount, planeSize[0], bytesPerLine[0]);

                bufferMgr->setAllocator(newStream);
                m_allocBuffers(bufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, false, false);

                privStreamInfo->setRegisterBuffer(EXYNOS_STREAM::HAL_STREAM_STS_REGISTERED);
                privStreamInfo->setBufferManager(bufferMgr);
                CLOGD("jpegBufferMgr - %p",
                         bufferMgr);

                break;
            case HAL_STREAM_ID_CALLBACK:
                ret = privStreamInfo->getOutputPortId(&outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to getOutputPortId for CALLBACK stream");
                    return ret;
                }

                ret = m_parameters->checkYuvSize(width, height, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to checkYuvSize for CALLBACK stream. size %dx%d outputPortId %d",
                             width, height, outputPortId);
                    return ret;
                }

                ret = m_parameters->checkYuvFormat(streamPixelFormat, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to checkYuvFormat for CALLBACK stream. format %x outputPortId %d",
                             streamPixelFormat, outputPortId);
                    return ret;
                }

                maxBufferCount = m_exynosconfig->current->bufInfo.num_preview_cb_buffers;
                ret = m_parameters->setYuvBufferCount(maxBufferCount, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to setYuvBufferCount for CALLBACK stream. maxBufferCount %d outputPortId %d",
                             maxBufferCount, outputPortId);
                    return ret;
                }

                /* Set reprocessing stream YUV size for ZSL_INPUT */
                {
                    int stallOutputPortId = ExynosCamera3Parameters::YUV_STALL_0 + outputPortId;

                    ret = m_parameters->checkYuvSize(width, height, stallOutputPortId);
                    if (ret != NO_ERROR) {
                        CLOGE("Failed to checkYuvSize for CALLBACK_ZSL stream. size %dx%d outputPortId %d",
                                width, height, stallOutputPortId);
                        return ret;
                    }

                    ret = m_parameters->checkYuvFormat(streamPixelFormat, stallOutputPortId);
                    if (ret != NO_ERROR) {
                        CLOGE("Failed to checkYuvFormat for CALLBACK_ZSL stream. format %x outputPortId %d",
                                streamPixelFormat, stallOutputPortId);
                        return ret;
                    }

                    ret = m_parameters->setYuvBufferCount(maxBufferCount, stallOutputPortId);
                    if (ret != NO_ERROR) {
                        CLOGE("Failed to setYuvBufferCount for CALLBACK_ZSL stream. maxBufferCount %d outputPortId %d",
                                maxBufferCount, stallOutputPortId);
                        return ret;
                    }
                }

                CLOGD("Create buffer manager(PREVIEW_CB)");
                ret = m_createServiceBufferManager(&bufferMgr, "PREVIEW_CB_STREAM_BUF");
                if (ret < 0) {
                    CLOGE("m_createBufferManager() failed!!");
                    return ret;
                }

                planeCount = streamPlaneCount + 1;
                planeSize[0] = (width * height * 3) / 2;

                CLOGD("planeCount %d+1, planeSize[0] %d, bytesPerLine[0] %d",
                         streamPlaneCount, planeSize[0], bytesPerLine[0]);

                bufferMgr->setAllocator(newStream);
                m_allocBuffers(bufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, true, false);

                privStreamInfo->setRegisterBuffer(EXYNOS_STREAM::HAL_STREAM_STS_REGISTERED);
                privStreamInfo->setBufferManager(bufferMgr);
                CLOGD("preivewCallbackBufferMgr - %p", bufferMgr);
                break;
            case HAL_STREAM_ID_CALLBACK_STALL:
                ret = privStreamInfo->getOutputPortId(&outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to getOutputPortId for CALLBACK stream");
                    return ret;
                }

                ret = m_parameters->checkYuvSize(width, height, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to checkPictureSize for CALLBACK_STALL stream. size %dx%d",
                            width, height);
                    return ret;
                }

                ret = m_parameters->checkYuvFormat(streamPixelFormat, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to checkYuvFormat for CALLBACK stream. format %x outputPortId %d",
                            streamPixelFormat, outputPortId);
                    return ret;
                }

                maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;
                ret = m_parameters->setYuvBufferCount(maxBufferCount, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to setYuvBufferCount for CALLBACK_STALL stream. maxBufferCount %d outputPortId %d",
                             maxBufferCount, outputPortId);
                    return ret;
                }

                CLOGD("Create buffer manager(CAPTURE_CB)");
                ret = m_createServiceBufferManager(&bufferMgr, "CAPTURE_CB_STREAM_BUF");
                if (ret < 0) {
                    CLOGE("m_createBufferManager() failed!!");
                    return ret;
                }

                planeCount = streamPlaneCount + 1;
                planeSize[0] = (width * height * 3) / 2;

                CLOGD("planeCount = %d+1, planeSize[0] = %d, bytesPerLine[0] = %d, outputPortId = %d",
                         streamPlaneCount, planeSize[0], bytesPerLine[0], outputPortId);

                bufferMgr->setAllocator(newStream);

                m_allocBuffers(bufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, true, false);

                privStreamInfo->setRegisterBuffer(EXYNOS_STREAM::HAL_STREAM_STS_REGISTERED);
                privStreamInfo->setBufferManager(bufferMgr);
                CLOGD("captureCallbackBufferMgr - %p",
                         bufferMgr);
                break;
            default:
                CLOGE("privStreamInfo->id is invalid !! id(%d)", id);
                ret = BAD_VALUE;
                break;
            }
        }
    }

    if (m_checkConfigStream == false) {
        isRestarted = true;
        m_setBuffersThread->run(PRIORITY_DEFAULT);
        m_startPictureBufferThread->run(PRIORITY_DEFAULT);
    }

    ret = m_requestMgr->createCallbackThreads(stream_list);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):createStreamThread() failed!! status(%d)", __FUNCTION__, __LINE__, ret);
    }

    m_checkConfigStream = true;

    CLOGD("Out");
    return ret;
}

status_t ExynosCamera3::registerStreamBuffers(const camera3_stream_buffer_set_t *buffer_set)
{
    /* deprecated function */
    if (buffer_set == NULL) {
        CLOGE("buffer_set is NULL");
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

status_t ExynosCamera3::processCaptureRequest(camera3_capture_request *request)
{
    Mutex::Autolock l(m_requestLock);
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraRequestSP_sprt_t req = NULL;
    ExynosCameraBuffer dstBuf;
    ExynosCameraStream *streamInfo = NULL;
    uint32_t timeOutNs = 60 * 1000000; /* timeout default value is 60ms based on 15fps */
    uint32_t waitMaxBlockCnt = 0;
    status_t ret = NO_ERROR;
    camera3_stream_t *stream = NULL;
    EXYNOS_STREAM::STATE registerBuffer = EXYNOS_STREAM::HAL_STREAM_STS_UNREGISTERED;
    uint32_t minBlockReqCount = 0;
    uint32_t maxBlockReqCount = 0;
    CameraMetadata meta;
    camera_metadata_entry_t entry;

#ifdef DEBUG_STREAM_CONFIGURATIONS
    CLOGD("DEBUG_STREAM_CONFIGURATIONS:Capture request(%d) #out(%d)",
         request->frame_number, request->num_output_buffers);
#else
    CLOGV("Capture request(%d) #out(%d)",
         request->frame_number, request->num_output_buffers);
#endif

    /* 1. Wait for allocation completion of internal buffers and creation of frame factory */
    m_setBuffersThread->join();
    m_framefactoryCreateThread->join();

    /* 2. Check the validation of request */
    if (request == NULL) {
        CLOGE("NULL request!");
        ret = BAD_VALUE;
        goto req_err;
    }

#if 0
    // For Fence FD debugging
    if (request->input_buffer != NULL){
        const camera3_stream_buffer* pStreamBuf = request->input_buffer;
        CLOGD("[Input] acqureFence[%d], releaseFence[%d], Status[%d], Buffer[%p]",
             request->num_output_buffers,
            pStreamBuf->acquire_fence, pStreamBuf->release_fence, pStreamBuf->status, pStreamBuf->buffer);
    }
    for(int i = 0; i < request->num_output_buffers; i++) {
        const camera3_stream_buffer* pStreamBuf = &(request->output_buffers[i]);
        CLOGD("[Output] Index[%d/%d] acqureFence[%d], releaseFence[%d], Status[%d], Buffer[%p]",
             i, request->num_output_buffers,
            pStreamBuf->acquire_fence, pStreamBuf->release_fence, pStreamBuf->status, pStreamBuf->buffer);
    }
#endif

    /* m_streamManager->dumpCurrentStreamList(); */

    /* 3. Check NULL for service metadata */
    if ((request->settings == NULL) && (m_requestMgr->isPrevRequest())) {
        CLOGE("Request%d: NULL and no prev request!!", request->frame_number);
        ret = BAD_VALUE;
        goto req_err;
    }

    /* 4. Check the registeration of input buffer on stream */
    if (request->input_buffer != NULL){
        stream = request->input_buffer->stream;
        streamInfo = static_cast<ExynosCameraStream*>(stream->priv);
        streamInfo->getRegisterBuffer(&registerBuffer);

        if (registerBuffer != EXYNOS_STREAM::HAL_STREAM_STS_REGISTERED) {
            CLOGE("Request %d: Input buffer not from input stream!",
                     request->frame_number);
            CLOGE("Bad Request %p, type %d format %x",
                    request->input_buffer->stream,
                    request->input_buffer->stream->stream_type,
                    request->input_buffer->stream->format);
            ret = BAD_VALUE;
            goto req_err;
        }
    }

    /* 5. Check the output buffer count */
    if ((request->num_output_buffers < 1) || (request->output_buffers == NULL)) {
        CLOGE("Request %d: No output buffers provided!",
                 request->frame_number);
        ret = BAD_VALUE;
        goto req_err;
    }

    /* 6. Store request settings
     * Caution : All information must be copied into internal data structure
     * before receiving another request from service
     */
    ret = m_pushServiceRequest(request, req);

    /* check restart condition */
    m_checkRestartStream(req);

    ret = m_registerStreamBuffers(request);

    /* 7. Calculate the timeout value for processing request based on actual fps setting */
    meta = request->settings;
    if (request->settings != NULL && meta.exists(ANDROID_CONTROL_AE_TARGET_FPS_RANGE)) {
        uint32_t minFps = 0, maxFps = 0;
        entry = meta.find(ANDROID_CONTROL_AE_TARGET_FPS_RANGE);
        m_parameters->checkPreviewFpsRange(entry.data.i32[0], entry.data.i32[1]);
        m_parameters->getPreviewFpsRange(&minFps, &maxFps);
        timeOutNs = (1000 / ((minFps == 0) ? 15 : minFps)) * 1000000;
    }

    /* 8. Process initial requests for preparing the stream */
    if (isRestarted == true) {
        CLOGD("Start FrameFactory requestKey(%d) m_flushFlag(%d/%d)",
                 request->frame_number, isRestarted, m_flushFlag);
        isRestarted = false;
        m_flushFlag = false;

        /* for FAST AE Stable */
        if (m_parameters->getUseFastenAeStable() == true &&
            m_parameters->getDualMode() == false &&
            m_parameters->getHighSpeedRecording() == false &&
            m_flagRunFastAE == false) {
                m_flagRunFastAE = true;
                m_fastenAeStable(m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW]);
        }
        m_frameFactoryStartThread->run();
    }

    m_flushWaitEnable = true;

    minBlockReqCount = m_exynosconfig->current->bufInfo.num_min_block_request;
    maxBlockReqCount = m_exynosconfig->current->bufInfo.num_max_block_request;
    waitMaxBlockCnt = minBlockReqCount * 10;

    while (m_requestMgr->getAllRequestCount() >= minBlockReqCount && m_flushFlag == false && waitMaxBlockCnt > 0) {
        status_t waitRet = NO_ERROR;
        m_captureResultDoneLock.lock();
        waitRet = m_captureResultDoneCondition.waitRelative(m_captureResultDoneLock, timeOutNs);
        if (waitRet == TIMED_OUT)
            CLOGV("time out (m_processList:%zu / totalRequestCnt:%d / "
                "blockReqCount = min:%u, max:%u / waitcnt:%u)",
                 m_processList.size(), m_requestMgr->getAllRequestCount(),
                minBlockReqCount, maxBlockReqCount, waitMaxBlockCnt);

        m_captureResultDoneLock.unlock();
        waitMaxBlockCnt--;

        if (m_requestMgr->getAllRequestCount() < maxBlockReqCount) {
            break;
        }
    }

req_err:
    return ret;
}

void ExynosCamera3::get_metadata_vendor_tag_ops(const camera3_device_t *, vendor_tag_query_ops_t *ops)
{
    if (ops == NULL) {
        CLOGE("ops is NULL");
        return;
    }
}

status_t ExynosCamera3::flush()
{
    status_t ret = NO_ERROR;
    ExynosCamera3FrameFactory *frameFactory = NULL;
    List<ExynosCameraFrameSP_sptr_t> *list = NULL;
    ExynosCameraFrameSP_sptr_t curFrame = NULL;
    List<ExynosCameraFrameSP_sptr_t>::iterator r;

    /*
     * This flag should be set before stoping all pipes,
     * because other func(like processCaptureRequest) must know call state about flush() entry level
     */
    m_flushFlag = true;
    isRestarted = true;
    m_captureResultDoneCondition.signal();

    Mutex::Autolock l(m_resultLock);
    CLOGD("IN+++");

    if (m_flushWaitEnable == false) {
        CLOGD("No need to wait & flush");
        goto func_exit;
    }

    /* Wait for finishing to start pipeline */
    m_frameFactoryStartThread->requestExitAndWait();
    m_reprocessingFrameFactoryStartThread->requestExitAndWait();

    /* Wait for finishing pre-processing threads*/
    m_mainThread->requestExitAndWait();
    m_selectBayerThread->requestExitAndWait();

    /* Release pre-processing frame queues */
    m_shotDoneQ->release();
    m_selectBayerQ->release();

    /* Create frame for the waiting request */
    while (m_requestWaitingList.size() > 0 || m_requestMgr->getServiceRequestCount() > 0) {
        CLOGV("Flush request. requestWaitingListSize %d serviceRequestCount %d",
                m_requestWaitingList.size(),
                m_requestMgr->getServiceRequestCount());

        m_createFrameFunc();
    }

    /* Stop pipeline */
    for (int i = 0; i < FRAME_FACTORY_TYPE_MAX; i++) {
        if (m_frameFactory[i] != NULL) {
            frameFactory = m_frameFactory[i];

            for (int k = i + 1; k < FRAME_FACTORY_TYPE_MAX; k++) {
               if (frameFactory == m_frameFactory[k]) {
                   CLOGD("m_frameFactory index(%d) and index(%d) are same instance,"
                        " set index(%d) = NULL", i, k, k);
                   m_frameFactory[k] = NULL;
               }
            }

            ret = m_stopFrameFactory(m_frameFactory[i]);
            if (ret < 0)
                CLOGE("m_frameFactory[%d] stopPipes fail", i);

            CLOGD("m_frameFactory[%d] stopPipes", i);
        }
    }

    /* Wait for finishing post-processing thread */
    m_previewStreamBayerThread->requestExitAndWait();
    m_previewStream3AAThread->requestExitAndWait();
    m_previewStreamISPThread->requestExitAndWait();
    m_previewStreamMCSCThread->requestExitAndWait();
#ifdef USE_VRA_GROUP
    m_previewStreamVRAThread->requestExitAndWait();
#endif
    m_internalFrameThread->requestExitAndWait();
    m_captureThread->requestExitAndWait();
    m_captureStreamThread->requestExitAndWait();
    if (m_duplicateBufferThread != NULL && m_duplicateBufferThread->isRunning()) {
        m_duplicateBufferThread->requestExitAndWait();
    }

    /* Release post-processing frame queues */
    for (int i = 0; i < MAX_PIPE_NUM; i++) {
        if (m_pipeFrameDoneQ[i] != NULL) {
            m_pipeFrameDoneQ[i]->release();
        }
    }
    m_internalFrameDoneQ->release();
    m_captureQ->release();
    m_reprocessingDoneQ->release();
    m_yuvCaptureDoneQ->release();
    m_duplicateBufferDoneQ->release();
#ifdef YUV_DUMP
    m_dumpFrameQ->release();
#endif

    /* Result callback for the unhandled service requests */
    do {
        Mutex::Autolock l(m_processLock);
        list = &m_processList;
        while (list->empty() == false) {
            r = list->begin()++;
            curFrame = *r;
            if (curFrame == NULL) {
                CLOGE("processList:curFrame is NULL");
                list->erase(r);

                /* Continue to release other frames in list */
                continue;
            }

            if (curFrame->getFrameType() == FRAME_TYPE_INTERNAL) {
                m_releaseInternalFrame(curFrame);

                list->erase(r);
                curFrame = NULL;
                continue;
            }

            ret = m_releaseRequestFrame(curFrame);
            if (ret != NO_ERROR) {
                CLOGE("processList:[F%d]Failed to releaseRequestFrame",
                        curFrame->getFrameCount());
                /* Continue to release other frames in list */
            }

            list->erase(r);
        }
    } while(0);

    do {
        Mutex::Autolock l(m_captureProcessLock);
        list = &m_captureProcessList;
        while (list->empty() == false) {
            r = list->begin()++;
            curFrame = *r;
            if (curFrame == NULL) {
                CLOGE("captureProcessList:curFrame is NULL");
                list->erase(r);

                /* Continue to release other frames in list */
                continue;
            }

            ret = m_releaseRequestFrame(curFrame);
            if (ret != NO_ERROR) {
                CLOGE("captureProcessList:[F%d]Failed to releaseRequestFrame",
                        curFrame->getFrameCount());
                /* Continue to release other frames in list */
            }

            list->erase(r);
        }
    } while(0);

    m_captureSelector->release();

    /* put all internal buffers */
    for (int bufIndex = 0; bufIndex < m_fliteBufferMgr->getAllocatedBufferCount(); bufIndex++)
        ret = m_putBuffers(m_fliteBufferMgr, bufIndex);
    for (int bufIndex = 0; bufIndex < m_3aaBufferMgr->getAllocatedBufferCount(); bufIndex++)
        ret = m_putBuffers(m_3aaBufferMgr, bufIndex);
    for (int bufIndex = 0; bufIndex < m_ispBufferMgr->getAllocatedBufferCount(); bufIndex++)
        ret = m_putBuffers(m_ispBufferMgr, bufIndex);
    for (int bufIndex = 0; bufIndex < m_mcscBufferMgr->getAllocatedBufferCount(); bufIndex++)
        ret = m_putBuffers(m_mcscBufferMgr, bufIndex);
#ifdef USE_VRA_GROUP
    for (int bufIndex = 0; bufIndex < m_vraBufferMgr->getAllocatedBufferCount(); bufIndex++)
        ret = m_putBuffers(m_vraBufferMgr, bufIndex);
#endif
    for (int bufIndex = 0; bufIndex < m_ispReprocessingBufferMgr->getAllocatedBufferCount(); bufIndex++)
        ret = m_putBuffers(m_ispReprocessingBufferMgr, bufIndex);
    for (int bufIndex = 0; bufIndex < m_yuvCaptureBufferMgr->getAllocatedBufferCount(); bufIndex++)
        ret = m_putBuffers(m_yuvCaptureBufferMgr, bufIndex);
    for (int bufIndex = 0; bufIndex < m_thumbnailBufferMgr->getAllocatedBufferCount(); bufIndex++)
        ret = m_putBuffers(m_thumbnailBufferMgr, bufIndex);

    if (m_internalScpBufferMgr != NULL) {
        for (int bufIndex = 0; bufIndex < m_internalScpBufferMgr->getAllocatedBufferCount(); bufIndex++)
            ret = m_putBuffers(m_internalScpBufferMgr, bufIndex);
    }

#ifdef SUPPORT_DEPTH_MAP
    if (m_depthMapBufferMgr != NULL) {
        for (int bufIndex = 0; bufIndex < m_depthMapBufferMgr->getAllocatedBufferCount(); bufIndex++)
            ret = m_putBuffers(m_depthMapBufferMgr, bufIndex);
    }
#endif

    /* Clear frame list, because the frame number is initialized in startFrameFactoryThread. */
    ret = m_clearList(&m_processList, &m_processLock);
    if (ret < 0) {
        CLOGE("m_clearList(m_processList) failed [%d]", ret);
    }
    ret = m_clearList(&m_captureProcessList, &m_captureProcessLock);
    if (ret < 0) {
        CLOGE("m_clearList(m_captureProcessList) failed [%d]", ret);
    }

func_exit:
    CLOGD(" : OUT---");
    return ret;
}

void ExynosCamera3::dump()
{
    CLOGD("");
}

int ExynosCamera3::getCameraId() const
{
    return m_cameraId;
}

status_t ExynosCamera3::setDualMode(bool enabled)
{
    if (m_parameters == NULL) {
        CLOGE("m_parameters is NULL");
        return INVALID_OPERATION;
    }

    m_parameters->setDualMode(enabled);

    return NO_ERROR;
}

status_t ExynosCamera3::m_fastenAeStable(ExynosCamera3FrameFactory *factory)
{
    int ret = 0;
    ExynosCameraBuffer fastenAeBuffer[NUM_FASTAESTABLE_BUFFERS];
    ExynosCameraBufferManager *skipBufferMgr = NULL;
    m_createInternalBufferManager(&skipBufferMgr, "SKIP_BUF");

    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    planeSize[0] = 32 * 64 * 2;
    int planeCount  = 2;
    int bufferCount = NUM_FASTAESTABLE_BUFFERS;

    if (skipBufferMgr == NULL) {
        CLOGE("createBufferManager failed");
        goto func_exit;
    }

    ret = m_allocBuffers(skipBufferMgr, planeCount, planeSize, bytesPerLine, bufferCount, true, false);
    if (ret < 0) {
        CLOGE("m_3aaBufferMgr m_allocBuffers(bufferCount=%d) fail",
             bufferCount);
        goto done;
    }

    for (int i = 0; i < bufferCount; i++) {
        int index = i;
        ret = skipBufferMgr->getBuffer(&index, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &fastenAeBuffer[i]);
        if (ret < 0) {
            CLOGE("getBuffer fail");
            goto done;
        }
    }

    ret = factory->fastenAeStable(bufferCount, fastenAeBuffer);
    if (ret < 0) {
        ret = INVALID_OPERATION;
        CLOGE("fastenAeStable fail(%d)", ret);
    }

done:
    if (skipBufferMgr != NULL) {
        skipBufferMgr->deinit();
        delete skipBufferMgr;
        skipBufferMgr = NULL;
    }
func_exit:
    return ret;
}

bool ExynosCamera3::m_mainThreadFunc(void)
{
    uint32_t frameCount = 0;
    status_t ret = NO_ERROR;

    /* 1. Wait the shot done with the latest framecount */
    ret = m_shotDoneQ->waitAndPopProcessQ(&frameCount);
    if (ret < 0) {
        if (ret == TIMED_OUT)
            CLOGW("wait timeout");
        else
            CLOGE("wait and pop fail, ret(%d)", ret);
        return true;
    }

    m_captureResultDoneCondition.signal();

    if (isRestarted == true) {
        CLOGI("wait configure stream");
        usleep(1);

        return true;
    }

    ret = m_createFrameFunc();
    if (ret != NO_ERROR)
        CLOGE("Failed to createFrameFunc. Shotdone framecount %d",
                 frameCount);

    return true;
}

void ExynosCamera3::m_updateCurrentShot(void)
{
    status_t ret = NO_ERROR;
    List<request_info_t *>::iterator r;
    request_info_t *requestInfo = NULL;
    ExynosCameraRequestSP_sprt_t request = NULL;
    ExynosCameraRequestSP_sprt_t sensitivityRequest = NULL;
    struct camera2_shot_ext temp_shot_ext;
    int controlInterval = 0;

    /* 1. Get the request info from the back of the list (the newest request) */
    r = m_requestWaitingList.end();
    r--;
    requestInfo = *r;
    request = requestInfo->request;

    /* 2. Get the metadata from request */
    ret = request->getServiceShot(&temp_shot_ext);
    if (ret != NO_ERROR) {
        CLOGE("Get service shot fail, requestKey(%d), ret(%d)",
                 request->getKey(), ret);
    }

    /* 3. Store the frameCount that the sensor control is going to be delivered */
    if (requestInfo->sensorControledFrameCount == 0)
        requestInfo->sensorControledFrameCount = m_internalFrameCount;

    /* 4. Get the request info from the front of the list (the oldest request) */
    r = m_requestWaitingList.begin();
    requestInfo = *r;
    request = requestInfo->request;

    /* 5. Update the entire shot_ext structure */
    ret = request->getServiceShot(m_currentShot);
    if (ret != NO_ERROR) {
        CLOGE("Get service shot fail, requestKey(%d), ret(%d)",
                 request->getKey(), ret);
    }

    /* 6. Overwrite the sensor control metadata to m_currentShot */
    if (temp_shot_ext.shot.ctl.aa.aeMode == AA_AEMODE_OFF
        || temp_shot_ext.shot.ctl.aa.mode == AA_CONTROL_OFF) {
        m_currentShot->shot.ctl.sensor.exposureTime = temp_shot_ext.shot.ctl.sensor.exposureTime;
        m_currentShot->shot.ctl.sensor.frameDuration = temp_shot_ext.shot.ctl.sensor.frameDuration;
        m_currentShot->shot.ctl.sensor.sensitivity = temp_shot_ext.shot.ctl.sensor.sensitivity;
        m_currentShot->shot.ctl.aa.vendor_isoValue = temp_shot_ext.shot.ctl.aa.vendor_isoValue;
        m_currentShot->shot.ctl.aa.vendor_isoMode = temp_shot_ext.shot.ctl.aa.vendor_isoMode;
        m_currentShot->shot.ctl.aa.aeMode = temp_shot_ext.shot.ctl.aa.aeMode;
    }

    m_currentShot->shot.ctl.aa.aeExpCompensation = temp_shot_ext.shot.ctl.aa.aeExpCompensation;
    m_currentShot->shot.ctl.aa.aeLock = temp_shot_ext.shot.ctl.aa.aeLock;
    m_currentShot->shot.ctl.lens.opticalStabilizationMode = temp_shot_ext.shot.ctl.lens.opticalStabilizationMode;

    controlInterval = m_internalFrameCount - requestInfo->sensorControledFrameCount;

    if ((m_internalFrameCount != 0) && (controlInterval == 0)) {
        m_currentShot->shot.ctl.aa.vendor_aeflashMode = temp_shot_ext.shot.ctl.aa.vendor_aeflashMode;
    } else if (m_internalFrameCount == 0) {
        m_currentShot->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
    }

#if 0
    /* Sensitivity control always has the 1 frame delay. */
    if (SENSOR_CONTROL_DELAY > 1) {
        r = m_requestWaitingList.begin();
        requestInfo = *r;
        r++;

        if (r != m_requestWaitingList.end())
            requestInfo = *r;

        sensitivityRequest = requestInfo->request;
        ret = sensitivityRequest->getServiceShot(&temp_shot_ext);
        if (ret != NO_ERROR) {
            CLOGE("Get service shot fail. requestKey(%d), ret(%d)",
                     sensitivityRequest->getKey(), ret);
        }

        m_currentShot->shot.ctl.sensor.sensitivity = temp_shot_ext.shot.ctl.sensor.sensitivity;
    }
#endif

    /* 7. Decide to make the internal frame */
    if (controlInterval < SENSOR_CONTROL_DELAY) {
        m_isNeedInternalFrame = true;
        m_isNeedRequestFrame = false;
    } else if (request->getNeedInternalFrame() == true) {
        m_isNeedInternalFrame = true;
        m_isNeedRequestFrame = true;
    } else {
        m_isNeedInternalFrame = false;
        m_isNeedRequestFrame = true;
    }

    CLOGV("m_internalFrameCount %d needRequestFrame %d needInternalFrame %d",
        m_internalFrameCount, m_isNeedRequestFrame, m_isNeedInternalFrame);

    return;
}

status_t ExynosCamera3::m_previewFrameHandler(ExynosCameraRequestSP_sprt_t request, ExynosCamera3FrameFactory *targetfactory)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraBufferManager *bufferMgr = NULL;
    ExynosCameraBuffer buffer;
    const camera3_stream_buffer_t *outputBuffer;
    uint32_t requestKey = 0;
    int32_t bufIndex = -1;
    bool captureFlag = false;
    bool zslInputFlag = false;
    bool rawStreamFlag = false;
    bool zslStreamFlag = false;
    bool needDynamicBayer = false;
    bool usePureBayer = false;
    int32_t reprocessingBayerMode = m_parameters->getReprocessingBayerMode();
    int pipeId = -1;
    int dstPos = 0;
    bool matchServiceBufferToBayer = false;
    struct camera2_shot_ext shot_ext;

    requestKey = request->getKey();

    /* Initialize the request flags in framefactory */
    targetfactory->setRequest(PIPE_VC0, false);
    targetfactory->setRequest(PIPE_3AC, false);
    targetfactory->setRequest(PIPE_MCSC0, false);
    targetfactory->setRequest(PIPE_MCSC1, false);
    targetfactory->setRequest(PIPE_MCSC2, false);
#ifdef USE_VRA_GROUP
    targetfactory->setRequest(PIPE_MCSC_DS, false);
    targetfactory->setRequest(PIPE_VRA, false);
#endif

    /* To decide the dynamic bayer request flag for JPEG capture */
    switch (reprocessingBayerMode) {
    case REPROCESSING_BAYER_MODE_NONE :
        needDynamicBayer = false;
        break;
    case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON :
        targetfactory->setRequest(PIPE_VC0, true);
        needDynamicBayer = false;
        usePureBayer = true;
        break;
    case REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON :
        targetfactory->setRequest(PIPE_3AC, true);
        needDynamicBayer = false;
        usePureBayer = false;
        break;
    case REPROCESSING_BAYER_MODE_PURE_DYNAMIC :
        needDynamicBayer = true;
        usePureBayer = true;
        break;
    case REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC :
        needDynamicBayer = true;
        usePureBayer = false;
        break;
    default :
        break;
    }

    /* Check ZSL_INPUT stream */
    camera3_stream_buffer_t *inputBuffer = request->getInputBuffer();
    if(inputBuffer != NULL) {
        int inputStreamId = 0;
        ExynosCameraStream *stream = static_cast<ExynosCameraStream *>(inputBuffer->stream->priv);
        if(stream != NULL) {
            stream->getID(&inputStreamId);
            switch (inputStreamId % HAL_STREAM_ID_MAX) {
            case HAL_STREAM_ID_ZSL_INPUT:
                zslInputFlag = true;
                break;
            case HAL_STREAM_ID_JPEG:
            case HAL_STREAM_ID_PREVIEW:
            case HAL_STREAM_ID_VIDEO:
            case HAL_STREAM_ID_CALLBACK:
            case HAL_STREAM_ID_CALLBACK_STALL:
                CLOGE("requestKey %d Invalid buffer-StreamType(%d)",
                        request->getKey(), inputStreamId);
                break;
            default:
                break;
            }
        } else {
            CLOGE("Stream is null (%d)", request->getKey());
        }
    }

    /* Setting DMA-out request flag based on stream configuration */
    for (size_t i = 0; i < request->getNumOfOutputBuffer(); i++) {
        int id = request->getStreamId(i);
        switch (id % HAL_STREAM_ID_MAX) {
        case HAL_STREAM_ID_RAW:
            CLOGV("requestKey %d outputBuffer-Index %zu buffer-StreamType(HAL_STREAM_ID_RAW)",
                     request->getKey(), i);

            targetfactory->setRequest(PIPE_VC0, true);
            rawStreamFlag = true;
            break;
        case HAL_STREAM_ID_ZSL_OUTPUT:
            CLOGV("request(%d) outputBuffer-Index(%zu) buffer-StreamType(HAL_STREAM_ID_ZSL)",
                     request->getKey(), i);
            if (usePureBayer == true)
                targetfactory->setRequest(PIPE_VC0, true);
            else
                targetfactory->setRequest(PIPE_3AC, true);
            zslStreamFlag = true;
            break;
        case HAL_STREAM_ID_PREVIEW:
            CLOGV("request %d outputBuffer-Index %zu buffer-StreamType(HAL_STREAM_ID_PREVIEW)",
                     request->getKey(), i);
            pipeId = (m_streamManager->getOutputPortId(id) % ExynosCamera3Parameters::YUV_MAX)
                    + PIPE_MCSC0;
            targetfactory->setRequest(pipeId, true);
            break;
        case HAL_STREAM_ID_CALLBACK:
            if (zslInputFlag == true) {
                /* If there is ZSL_INPUT stream buffer,
                * It will be processed through reprocessing stream.
                */
                break;
            }

            CLOGV("request %d outputBuffer-Index %zu buffer-StreamType(HAL_STREAM_ID_CALLBACK)",
                     request->getKey(), i);
            pipeId = (m_streamManager->getOutputPortId(id) % ExynosCamera3Parameters::YUV_MAX)
                    + PIPE_MCSC0;
            targetfactory->setRequest(pipeId, true);
            break;
        case HAL_STREAM_ID_VIDEO:
            CLOGV("request %d outputBuffer-Index %zu buffer-StreamType(HAL_STREAM_ID_VIDEO)",
                     request->getKey(), i);
            pipeId = (m_streamManager->getOutputPortId(id) % ExynosCamera3Parameters::YUV_MAX)
                    + PIPE_MCSC0;
            targetfactory->setRequest(pipeId, true);
            m_recordingEnabled = true;
            break;
        case HAL_STREAM_ID_JPEG:
            CLOGD("request %d outputBuffer-Index %zu buffer-StreamType(HAL_STREAM_ID_JPEG)",
                     request->getKey(), i);
            if (needDynamicBayer == true) {
                if(m_parameters->getUsePureBayerReprocessing()) {
                    targetfactory->setRequest(PIPE_VC0, true);
                } else {
                    targetfactory->setRequest(PIPE_3AC, true);
                }
            }
            captureFlag = true;
            break;
        case HAL_STREAM_ID_CALLBACK_STALL:
            CLOGV("request %d outputBuffer-Index %zu buffer-StreamType(HAL_STREAM_ID_CALLBACK_STALL)",
                     request->getKey(), i);
            if (needDynamicBayer == true) {
                if(m_parameters->getUsePureBayerReprocessing()) {
                    targetfactory->setRequest(PIPE_VC0, true);
                } else {
                    targetfactory->setRequest(PIPE_3AC, true);
                }
            }
            captureFlag = true;
            break;
        default:
            CLOGE("Invalid stream ID %d", id);
            break;
        }
    }

    request->getServiceShot(&shot_ext);

    if (m_currentShot == NULL) {
        CLOGE("m_currentShot is NULL. Use request control metadata. requestKey %d",
                 request->getKey());
        memcpy(m_currentShot, &shot_ext, sizeof(struct camera2_shot_ext));
    }

    m_updateCropRegion(m_currentShot);
#ifdef USE_VRA_GROUP
    m_updateFD(m_currentShot, shot_ext.shot.ctl.stats.faceDetectMode, request->getDsInputPortId(), false);
    m_parameters->setDsInputPortId(m_currentShot->shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS]);
#endif
    m_updateEdgeNoiseMode(m_currentShot, false);

#ifdef USE_VRA_GROUP
    if (m_parameters->isMcscVraOtf() == false
         && m_currentShot->fd_bypass == false) {
         targetfactory->setRequest(PIPE_MCSC_DS, true);
         targetfactory->setRequest(PIPE_VRA, true);
    }
#endif

    /* Set framecount into request */
    if (request->getFrameCount() == 0)
        m_requestMgr->setFrameCount(m_internalFrameCount++, request->getKey());

    /* Generate frame for YUV stream */
    ret = m_generateFrame(request->getFrameCount(), targetfactory, &m_processList, &m_processLock, newFrame);
    if (ret != NO_ERROR) {
        CLOGE("Failed to generateRequestFrame. framecount %d",
                 m_internalFrameCount - 1);
        goto CLEAN;
    } else if (newFrame == NULL) {
        CLOGE("newFrame is NULL. framecount %d",
                 m_internalFrameCount - 1);
        goto CLEAN;
    }

    CLOGV("generate request framecount %d requestKey %d",
            newFrame->getFrameCount(), request->getKey());

    if (m_flushFlag == true) {
        CLOGD("[R%d F%d]Flush is in progress.",
                request->getKey(), newFrame->getFrameCount());
        /* Generated frame is going to be deleted at flush() */
        goto CLEAN;
    }

    /* Set control metadata to frame */
    ret = newFrame->setMetaData(m_currentShot);
    if (ret != NO_ERROR) {
        CLOGE("Set metadata to frame fail, Frame count(%d), ret(%d)",
                 newFrame->getFrameCount(), ret);
    }

    /* Attach FLITE buffer & push frame to FLITE */
    if (newFrame->getRequest(PIPE_VC0) == true) {

        pipeId = m_getBayerPipeId();

        dstPos = targetfactory->getNodeType(PIPE_VC0);

        if ((rawStreamFlag == true) && (zslStreamFlag != true)) {
            matchServiceBufferToBayer = true;
        }

        if (m_parameters->isFlite3aaOtf() == false && matchServiceBufferToBayer == true) {
            ret = m_bayerBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &buffer);
            if (ret != NO_ERROR) {
                CLOGE("Failed to get Service Bayer Buffer. framecount %d availableBuffer %d",
                        newFrame->getFrameCount(),
                        m_bayerBufferMgr->getNumOfAvailableBuffer());

                ret = newFrame->setEntityState(pipeId, ENTITY_STATE_COMPLETE);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to setEntityState with COMPLETE to FLITE. framecount %d", newFrame->getFrameCount());
                }
            } else {
                newFrame->setFrameServiceBayer(true);
                CLOGV("Use Service Bayer Buffer. framecount %d bufferIndex %d", newFrame->getFrameCount(), buffer.index);
            }
        } else if (zslStreamFlag == true) {
            ret = m_zslBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &buffer);
            if (ret != NO_ERROR) {
                CLOGE("Failed to get ZSL Buffer. framecount %d availableBuffer %d",
                        newFrame->getFrameCount(),
                        m_zslBufferMgr->getNumOfAvailableBuffer());
            } else if (bufIndex < 0) {
                CLOGW("Inavlid zsl buffer index %d. Skip to use ZSL service buffer",
                         bufIndex);
            } else {
                CLOGD("Use ZSL Service Buffer. framecount %d bufferIndex %d",
                        newFrame->getFrameCount(), buffer.index);

                ret = newFrame->setDstBufferState(PIPE_3AA, ENTITY_BUFFER_STATE_REQUESTED, targetfactory->getNodeType(PIPE_VC0));
                if (ret != NO_ERROR) {
                    CLOGE("setDstBuffer state fail, pipeID(%d), frameCount(%d), ret(%d)",
                             PIPE_3AA, newFrame->getFrameCount(), ret);
                }

                ret = newFrame->setDstBuffer(PIPE_3AA, buffer, targetfactory->getNodeType(PIPE_VC0));
                if (ret != NO_ERROR) {
                    CLOGE("Failed to setupEntity with ZSL service buffer. \
                            framecount %d bufferIndex %d",
                            newFrame->getFrameCount(), buffer.index);
                }
            }

            newFrame->setFrameZsl(true);

        } else {
            CLOGV("Use Internal Bayer Buffer. framecount %d bufferIndex %d", newFrame->getFrameCount(), buffer.index);
        }

        if (m_parameters->isFlite3aaOtf() == false) {
            if (0 <= bufIndex)
                ret = m_setupEntity(PIPE_FLITE, newFrame, NULL, &buffer, dstPos, PIPE_VC0);
            else
                ret = m_setupEntity(PIPE_FLITE, newFrame, NULL, NULL,    dstPos, PIPE_VC0);

            if (ret != NO_ERROR) {
                CLOGE("Failed to setupEntity with bayer buffer. framecount %d bufferIndex %d", newFrame->getFrameCount(), buffer.index);
            } else {
                targetfactory->pushFrameToPipe(newFrame, PIPE_FLITE);
            }
        }
    }

    if (newFrame->getRequest(PIPE_3AC) == true) {
        if (zslStreamFlag == true) {
            ret = m_zslBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &buffer);
            if (ret != NO_ERROR) {
                CLOGE("Failed to get ZSL Buffer. framecount %d availableBuffer %d",
                        newFrame->getFrameCount(),
                        m_zslBufferMgr->getNumOfAvailableBuffer());
            } else if (bufIndex < 0) {
                CLOGW("Inavlid zsl buffer index %d. Skip to use ZSL service buffer",
                         bufIndex);
            } else {
                CLOGD("Use ZSL Service Buffer. framecount %d bufferIndex %d",
                        newFrame->getFrameCount(), buffer.index);

                ret = newFrame->setDstBufferState(PIPE_3AA, ENTITY_BUFFER_STATE_REQUESTED, targetfactory->getNodeType(PIPE_3AC));
                if (ret != NO_ERROR) {
                    CLOGE("setDstBuffer state fail, pipeID(%d), frameCount(%d), ret(%d)",
                             PIPE_3AA, newFrame->getFrameCount(), ret);
                }

                ret = newFrame->setDstBuffer(PIPE_3AA, buffer, targetfactory->getNodeType(PIPE_3AC));
                if (ret != NO_ERROR) {
                    CLOGE("Failed to setupEntity with ZSL service buffer. \
                            framecount %d bufferIndex %d",
                            newFrame->getFrameCount(), buffer.index);
                }
            }

            newFrame->setFrameZsl(true);
        }
    }

    /* Attach 3AA buffer & push frame to 3AA */
    if (m_parameters->isFlite3aaOtf() == true) {
        ret = m_setupEntity(PIPE_3AA, newFrame);
        if (ret != NO_ERROR) {
            ret = m_getBufferManager(PIPE_3AA, &bufferMgr, SRC_BUFFER_DIRECTION);
            if (ret != NO_ERROR) {
                CLOGE("Failed to getBufferManager for 3AA. framecount %d",
                         newFrame->getFrameCount());
                return ret;
            }

            CLOGW("Failed to get 3AA buffer. framecount %d availableBuffer %d",
                    newFrame->getFrameCount(),
                    bufferMgr->getNumOfAvailableBuffer());
        } else {
            dstPos = targetfactory->getNodeType(PIPE_VC0);

            if (m_parameters->isFlite3aaOtf() == true && matchServiceBufferToBayer == true) {
                ret = m_bayerBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &buffer);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to get Service Bayer Buffer. framecount %d availableBuffer %d",
                            newFrame->getFrameCount(),
                            m_bayerBufferMgr->getNumOfAvailableBuffer());

                    ret = newFrame->setEntityState(pipeId, ENTITY_STATE_COMPLETE);
                    if (ret != NO_ERROR) {
                        CLOGE("Failed to setEntityState with COMPLETE to FLITE. framecount %d", newFrame->getFrameCount());
                    }
                }

                if (ret == NO_ERROR) {
                    ret = newFrame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_REQUESTED, dstPos);
                    if (ret != NO_ERROR) {
                        CLOGE("setDstBuffer state fail, pipeId(%d), dstPos(%d), frameCount(%d), ret(%d)",
                                 pipeId, dstPos, newFrame->getFrameCount(), ret);
                    }
                }

                if (ret == NO_ERROR) {
                    ret = newFrame->setDstBuffer(pipeId, buffer, dstPos);
                    if (ret < 0) {
                        CLOGE("setDstBuffer fail, pipeId(%d), dstPos(%d), frameCount(%d), ret(%d)",
                            pipeId, dstPos, newFrame->getFrameCount(), ret);

                    }
                }

                if (ret == NO_ERROR) {
                    newFrame->setFrameServiceBayer(true);
                    CLOGV("Use Service Bayer Buffer. framecount %d bufferIndex %d", newFrame->getFrameCount(), buffer.index);
                } else {
                    CLOGE("Fail to use Service Bayer Buffer. framecount %d bufferIndex %d", newFrame->getFrameCount(), buffer.index);
                    if (0 <= bufIndex) {
                        ret = m_putBuffers(m_bayerBufferMgr, bufIndex);
                        if (ret != NO_ERROR) {
                            CLOGE("m_putBuffers(m_bayerBufferMgr, %d) fail", buffer.index);
                        }
                    }
                }
            } else {
                CLOGV("Use Internal Bayer Buffer. framecount %d bufferIndex %d", newFrame->getFrameCount(), buffer.index);
            }
            targetfactory->pushFrameToPipe(newFrame, PIPE_3AA);
        }
    }

    if (captureFlag == true) {
        CLOGI("setFrameCapture true, frameCount %d",
                 newFrame->getFrameCount());
        newFrame->setFrameCapture(captureFlag);
    }

    /* Run PIPE_FLITE main thread to enable dynamic bayer DMA-write */
    if (newFrame->getRequest(PIPE_VC0) == true
        && m_parameters->isFlite3aaOtf() == false
        && targetfactory->checkPipeThreadRunning(PIPE_FLITE) == false) {
        m_previewStreamBayerThread->run(PRIORITY_DEFAULT);
        targetfactory->startThread(PIPE_FLITE);
    }

CLEAN:
    return ret;
}

status_t ExynosCamera3::m_captureFrameHandler(ExynosCameraRequestSP_sprt_t request, ExynosCamera3FrameFactory *targetfactory)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    struct camera2_shot_ext shot_ext;
    uint32_t requestKey = 0;
    bool captureFlag = false;
    bool rawStreamFlag = false;
    bool zslFlag = false;
    bool yuvCbStallFlag = false;
    bool isNeedThumbnail = false;
    int pipeId = -1;

    CLOGD("Capture request. requestKey %d frameCount %d",
            request->getKey(),
            request->getFrameCount());

    if (targetfactory == NULL) {
        CLOGE("targetfactory is NULL");
        return INVALID_OPERATION;
    }

    m_startPictureBufferThread->join();

    requestKey = request->getKey();

    /* Initialize the request flags in framefactory */
    targetfactory->setRequest(PIPE_MCSC0_REPROCESSING, false);
    targetfactory->setRequest(PIPE_MCSC1_REPROCESSING, false);
    targetfactory->setRequest(PIPE_MCSC2_REPROCESSING, false);
    targetfactory->setRequest(PIPE_MCSC3_REPROCESSING, false);
    targetfactory->setRequest(PIPE_MCSC4_REPROCESSING, false);

    if (m_parameters->isReprocessing() == true) {
        if (m_parameters->isUseHWFC()) {
            targetfactory->setRequest(PIPE_HWFC_JPEG_DST_REPROCESSING, false);
            targetfactory->setRequest(PIPE_HWFC_THUMB_DST_REPROCESSING, false);
        }
    }

    /* set input buffers belonged to each stream as available */
    camera3_stream_buffer_t* buffer = request->getInputBuffer();
    if(buffer != NULL) {
        int inputStreamId = 0;
        ExynosCameraStream *stream = static_cast<ExynosCameraStream *>(buffer->stream->priv);
        if(stream != NULL) {
            stream->getID(&inputStreamId);
            CLOGD("requestKey %d buffer-StreamType(HAL_STREAM_ID_ZSL_INPUT[%d]) Buffer[%p], Stream[%p]",
                     request->getKey(), inputStreamId, buffer, stream);

            switch (inputStreamId % HAL_STREAM_ID_MAX) {
            case HAL_STREAM_ID_ZSL_INPUT:
                CLOGD("requestKey %d buffer-StreamType(HAL_STREAM_ID_ZSL_INPUT[%d])",
                         request->getKey(), inputStreamId);
                zslFlag = true;
                break;
            case HAL_STREAM_ID_JPEG:
            case HAL_STREAM_ID_PREVIEW:
            case HAL_STREAM_ID_VIDEO:
            case HAL_STREAM_ID_CALLBACK:
            case HAL_STREAM_ID_CALLBACK_STALL:
                CLOGE("requestKey %d Invalid buffer-StreamType(%d)",
                         request->getKey(), inputStreamId);
                break;
            default:
                break;
            }
        }else {
            CLOGE(" Stream is null (%d)", request->getKey());
        }
    }

    /* set output buffers belonged to each stream as available */
    for (size_t i = 0; i < request->getNumOfOutputBuffer(); i++) {
        int id = request->getStreamId(i);
        switch (id % HAL_STREAM_ID_MAX) {
        case HAL_STREAM_ID_JPEG:
            CLOGD("requestKey %d buffer-StreamType(HAL_STREAM_ID_JPEG)",
                     request->getKey());

            if (m_parameters->getOutPutFormatNV21Enable() == true) {
               /* NV21 format size is set by CALLBACK_STALL stream */
                pipeId = (m_streamManager->getOutputPortId(HAL_STREAM_ID_CALLBACK_STALL)
                        % ExynosCamera3Parameters::YUV_MAX)
                        + PIPE_MCSC0_REPROCESSING;
                targetfactory->setRequest(pipeId, true);
            } else {
                request->getServiceShot(&shot_ext);

                targetfactory->setRequest(PIPE_MCSC3_REPROCESSING, true);
                isNeedThumbnail = (shot_ext.shot.ctl.jpeg.thumbnailSize[0] > 0
                                   && shot_ext.shot.ctl.jpeg.thumbnailSize[1] > 0)? true : false;
                targetfactory->setRequest(PIPE_MCSC4_REPROCESSING, isNeedThumbnail);
                targetfactory->setRequest(PIPE_HWFC_JPEG_DST_REPROCESSING, true);
                targetfactory->setRequest(PIPE_HWFC_THUMB_DST_REPROCESSING, true);
            }

            captureFlag = true;
            break;
        case HAL_STREAM_ID_RAW:
            CLOGV("requestKey %d buffer-StreamType(HAL_STREAM_ID_RAW)",
                     request->getKey());
            rawStreamFlag = true;
            break;
        case HAL_STREAM_ID_PREVIEW:
        case HAL_STREAM_ID_VIDEO:
            break;
        case HAL_STREAM_ID_CALLBACK:
            if (zslFlag == false) {
                /* If there is no ZSL_INPUT stream buffer,
                * It will be processed through preview stream.
                */
                break;
            }
        case HAL_STREAM_ID_CALLBACK_STALL:
            CLOGV("requestKey %d buffer-StreamType(HAL_STREAM_ID_CALLBACK_STALL)",
                     request->getKey());
            pipeId = (m_streamManager->getOutputPortId(id) % ExynosCamera3Parameters::YUV_MAX)
                    + PIPE_MCSC0_REPROCESSING;
            targetfactory->setRequest(pipeId, true);
            captureFlag = true;
            yuvCbStallFlag = true;
            break;
        default:
            CLOGE("requestKey %d Invalid buffer-StreamType(%d)",
                     request->getKey(), id);
            break;
        }
    }

    request->getServiceShot(&shot_ext);

    if (m_currentShot == NULL) {
        CLOGE("m_currentShot is NULL. Use request control metadata. requestKey %d",
                 request->getKey());

        memcpy(m_currentShot, &shot_ext, sizeof(struct camera2_shot_ext));
    }

    m_updateCropRegion(m_currentShot);
    m_updateJpegControlInfo(m_currentShot);
#ifdef USE_VRA_GROUP
    m_updateFD(m_currentShot, shot_ext.shot.ctl.stats.faceDetectMode, request->getDsInputPortId(), true);
    m_parameters->setDsInputPortId(m_currentShot->shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS]);
#endif
    m_updateEdgeNoiseMode(m_currentShot, true);

    /* Set framecount into request */
    if (request->getNeedInternalFrame() == true)
        /* Must use the same framecount with internal frame */
        m_internalFrameCount--;

    if (request->getFrameCount() == 0)
        m_requestMgr->setFrameCount(m_internalFrameCount++, request->getKey());

    ret = m_generateFrame(request->getFrameCount(), targetfactory, &m_captureProcessList, &m_captureProcessLock, newFrame);
    if (ret != NO_ERROR) {
        CLOGE("m_generateFrame fail");
    } else if (newFrame == NULL) {
        CLOGE("new faame is NULL");
        return INVALID_OPERATION;
    }

    CLOGV("generate request framecount %d requestKey %d",
            newFrame->getFrameCount(), request->getKey());

    if (m_flushFlag == true) {
        CLOGD("[R%d F%d]Flush is in progress.",
                request->getKey(), newFrame->getFrameCount());
        /* Generated frame is going to be deleted at flush() */
        return ret;
    }

    m_parameters->getFdMeta(m_parameters->isReprocessing(), m_currentShot);

    ret = newFrame->setMetaData(m_currentShot);
    if (ret != NO_ERROR) {
        CLOGE("Set metadata to frame fail, Frame count(%d), ret(%d)",
                 newFrame->getFrameCount(), ret);
    }

    newFrame->setFrameServiceBayer(rawStreamFlag);
    newFrame->setFrameCapture(captureFlag);
    newFrame->setFrameZsl(zslFlag);
    newFrame->setFrameServiceYuvCbStall(yuvCbStallFlag);

#ifdef CORRECT_TIMESTAMP_FOR_SENSORFUSION
    newFrame->setAdjustedTimestampFlag(!zslFlag);
#endif

    m_selectBayerQ->pushProcessQ(&newFrame);

    if(m_selectBayerThread != NULL && m_selectBayerThread->isRunning() == false) {
        m_selectBayerThread->run();
        CLOGI("Initiate selectBayerThread (%d)",
             m_selectBayerThread->getTid());
    }

    return ret;
}

status_t ExynosCamera3::m_createRequestFrameFunc(ExynosCameraRequestSP_sprt_t request)
{
    int32_t factoryAddrIndex = 0;
    bool removeDupFlag = false;

    ExynosCamera3FrameFactory *factory = NULL;
    ExynosCamera3FrameFactory *factoryAddr[100] ={NULL,};
    FrameFactoryList factorylist;
    FrameFactoryListIterator factorylistIter;
    factory_handler_t frameCreateHandler;

#ifdef DEBUG_STREAM_CONFIGURATIONS
    CLOGD("DEBUG_STREAM_CONFIGURATIONS::generate request frame request(%d)", request->getKey());
#endif

    // TODO: acquire fence
    /* 1. Remove the duplicated frame factory in request */
    factoryAddrIndex = 0;
    factorylist.clear();

    request->getFrameFactoryList(&factorylist);
    for (factorylistIter = factorylist.begin(); factorylistIter != factorylist.end(); ) {
        removeDupFlag = false;
        factory = *factorylistIter;
        CLOGV("list Factory(%p) ", factory);

        for (int i = 0; i < factoryAddrIndex ; i++) {
            if (factoryAddr[i] == factory) {
                removeDupFlag = true;
                break;
            }
        }

        if (removeDupFlag) {
            CLOGV("remove duplicate Factory factoryAddrIndex(%d)",
                     factoryAddrIndex);
            factorylist.erase(factorylistIter++);
        } else {
            CLOGV("add frame factory, factoryAddrIndex(%d)",
                     factoryAddrIndex);
            factoryAddr[factoryAddrIndex] = factory;
            factoryAddrIndex++;
            factorylistIter++;
        }

    }

    /* 2. Call the frame create handler for each frame factory */
    /*    Frame creation handler calling sequence is ordered in accordance with
          its frame factory type, because there are dependencies between frame
          processing routine. */
    for (int factoryTypeIdx = 0; factoryTypeIdx < FRAME_FACTORY_TYPE_MAX; factoryTypeIdx++) {
        for (int i = 0; i < factoryAddrIndex; i++) {
            CLOGV("framefactory factoryAddr[%d] = %p / m_frameFactory[%d] = %p",
                     i, factoryAddr[i], factoryTypeIdx, m_frameFactory[factoryTypeIdx]);

            if( (factoryAddr[i] != NULL) && (factoryAddr[i] == m_frameFactory[factoryTypeIdx]) ) {
                CLOGV("framefactory index(%d) maxIndex(%d) (%p)",
                         i, factoryAddrIndex, factoryAddr[i]);
                frameCreateHandler = factoryAddr[i]->getFrameCreateHandler();
                (this->*frameCreateHandler)(request, factoryAddr[i]);
                factoryAddr[i] = NULL;
            }
        }
    }

    CLOGV("- OUT - (F:%d)", request->getKey());
    return NO_ERROR;
}

status_t ExynosCamera3::m_createInternalFrameFunc(void)
{
    status_t ret = NO_ERROR;
    ExynosCameraBufferManager *bufferMgr = NULL;
    ExynosCamera3FrameFactory *factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
    ExynosCameraFrameSP_sptr_t newFrame = NULL;

    /* Generate the internal frame */
    ret = m_generateInternalFrame(m_internalFrameCount++, factory, &m_processList, &m_processLock, newFrame);
    if (ret != NO_ERROR) {
        CLOGE("m_generateFrame failed");
        return ret;
    } else if (newFrame == NULL) {
        CLOGE("newFrame is NULL");
        return INVALID_OPERATION;
    }

#ifdef DEBUG_STREAM_CONFIGURATIONS
    CLOGD("DEBUG_STREAM_CONFIGURATIONS::generate internal frame framecount(%d)",
            newFrame->getFrameCount());
#else
    CLOGV("generate internal frame framecount(%d)",
            newFrame->getFrameCount());
#endif

    if (m_flushFlag == true) {
        CLOGD("[F%d]Flush is in progress.",
                newFrame->getFrameCount());
        /* Generated frame is going to be deleted at flush() */
        return ret;
    }

    /* Disable all DMA-out request for this internal frame
     *                    3AP,   3AC,   ISP,   ISPP,  ISPC,  SCC,   DIS,   SCP */
    newFrame->setRequest(false, false, false, false, false, false, false, false);
    newFrame->setRequest(PIPE_VC0, false);
    newFrame->setRequest(PIPE_MCSC1, false);
    newFrame->setRequest(PIPE_MCSC2, false);
#ifdef USE_VRA_GROUP
    if (newFrame->getRequest(PIPE_MCSC_DS) == true || newFrame->getRequest(PIPE_VRA) == true) {
        newFrame->setRequest(PIPE_MCSC_DS, false);
        newFrame->setRequest(PIPE_VRA, false);
		ret = newFrame->setEntityState(PIPE_VRA, ENTITY_STATE_COMPLETE);
		if (ret < 0) {
			CLOGE("setEntityState fail, pipeId(%d), state(%d), ret(%d)",
				PIPE_VRA, ENTITY_STATE_COMPLETE, ret);
		}
    }
#endif

    switch (m_parameters->getReprocessingBayerMode()) {
    case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON :

        /*
         * for internal frame, we need to make all capture is not request.
         * find FRAME_TYPE_INTERNAL in MCPipe
         */
        newFrame->setRequest(PIPE_VC0, true);
        break;
    case REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON:
        newFrame->setRequest(PIPE_3AC, true);
        break;
    default:
        break;
    }

    /* Update the metadata with m_currentShot into frame */
    if (m_currentShot == NULL) {
        CLOGE("m_currentShot is NULL");
        return INVALID_OPERATION;
    }

    ret = newFrame->setMetaData(m_currentShot);
    if (ret != NO_ERROR) {
        CLOGE("Failed to setMetaData with m_currehtShot. framecount %d ret %d",
                newFrame->getFrameCount(), ret);
        return ret;
    }

    /* Attach FLITE buffer & push frame to FLITE */
    // nop

    /* Attach 3AA buffer & push frame to 3AA */
    if (m_parameters->isFlite3aaOtf() == true) {
        ret = m_setupEntity(PIPE_3AA, newFrame);
        if (ret != NO_ERROR) {
            ret = m_getBufferManager(PIPE_3AA, &bufferMgr, SRC_BUFFER_DIRECTION);
            if (ret != NO_ERROR) {
                CLOGE("Failed to getBufferManager for 3AA. framecount %d",
                         newFrame->getFrameCount());
                return ret;
            }

            CLOGW("Failed to get 3AA buffer. framecount %d availableBuffer %d",
                    newFrame->getFrameCount(),
                    bufferMgr->getNumOfAvailableBuffer());
        } else {
            factory->pushFrameToPipe(newFrame, PIPE_3AA);
        }
    }
    else {
        ret = m_setupEntity(PIPE_FLITE, newFrame);
        if (ret != NO_ERROR) {
            ret = m_getBufferManager(PIPE_FLITE, &bufferMgr, SRC_BUFFER_DIRECTION);
            if (ret != NO_ERROR) {
                CLOGE("Failed to getBufferManager for FLITE. framecount %d", newFrame->getFrameCount());
            }

            CLOGW("Failed to get FLITE buffer. Skip to pushFrame. framecount %d availableBuffer %d",
                    newFrame->getFrameCount(),
                    bufferMgr->getNumOfAvailableBuffer());
        } else {
            factory->pushFrameToPipe(newFrame, PIPE_FLITE);
        }
    }

    return ret;
}

status_t ExynosCamera3::m_createFrameFunc(void)
{
    status_t ret = NO_ERROR;
    ExynosCameraRequestSP_sprt_t request = NULL;

    /* 1. Get new service request from request manager */
#if 1
    /* HACK : Skip to update metadata for 2nd frame (fCount == 2)
     * AE library does NOT refer to the 1st shot.
     * To get the normal dynamic metadata for the 1st request from user,
     * HAL must create the two frame which have the same control metadata.
     * Therefore, the frame that has the frameCount 1 must not be updated
     * with new request.
     */
    if (m_requestMgr->getServiceRequestCount() > 0 && m_internalFrameCount != 1)
#else
    if (m_requestMgr->getServiceRequestCount() > 0)
#endif
        m_popServiceRequest(request);

    /* 2. Push back the new service request into m_requestWaitingList */
    /* Warning :
     * The List APIs for 'm_requestWaitingList' are called sequencially.
     * So the mutex is not required.
     * If the 'm_requestWaitingList' will be accessed by another thread,
     * using mutex must be considered.
     */
    if (request != NULL) {
        request_info_t *requestInfo = new request_info_t;
        requestInfo->request = request;
        requestInfo->sensorControledFrameCount = 0;
        m_requestWaitingList.push_back(requestInfo);
    }

    /* 3. Update the current shot */
    if (m_requestWaitingList.size() > 0)
        m_updateCurrentShot();

    CLOGV("Create New Frame %d needRequestFrame %d needInternalFrame %d waitingSize %d",
            m_internalFrameCount, m_isNeedRequestFrame, m_isNeedInternalFrame, m_requestWaitingList.size());

    /* 4. Select the frame creation logic between request frame and internal frame */
    if (m_isNeedInternalFrame == true || m_requestWaitingList.empty() == true) {
        m_createInternalFrameFunc();
    }
    if (m_isNeedRequestFrame == true && m_requestWaitingList.empty() == false) {
        List<request_info_t *>::iterator r;
        request_info_t *requestInfo = NULL;

        r = m_requestWaitingList.begin();
        requestInfo = *r;
        request = requestInfo->request;

        m_createRequestFrameFunc(request);

        m_requestWaitingList.erase(r);
        delete requestInfo;
    }

    return ret;
}

status_t ExynosCamera3::m_sendRawStreamResult(ExynosCameraFrameSP_sptr_t frame, uint32_t pipeId, bool isSrc, int dstPos)
{
    status_t ret = NO_ERROR;
    ExynosCameraStream *stream = NULL;
    ExynosCameraRequestSP_sprt_t request = NULL;
    ExynosCameraBuffer buffer;
    camera3_stream_buffer_t streamBuffer;
    ResultRequest resultRequest = NULL;

    /* 1. Get stream object for RAW */
    ret = m_streamManager->getStream(HAL_STREAM_ID_RAW, &stream);
    if (ret < 0) {
        CLOGE("getStream is failed, from streammanager. Id error:HAL_STREAM_ID_RAW");
        return ret;
    }

    /* 2. Get camera3_stream structure from stream object */
    ret = stream->getStream(&streamBuffer.stream);
    if (ret < 0) {
        CLOGE("getStream is failed, from exynoscamerastream. Id error:HAL_STREAM_ID_RAW");
        return ret;
    }

    /* 3. Get the bayer buffer from frame */
    if (isSrc == true)
        ret = frame->getSrcBuffer(pipeId, &buffer);
    else
        ret = frame->getDstBuffer(pipeId, &buffer, dstPos);
    if (ret < 0) {
        CLOGE("Get bayer buffer failed, framecount(%d), isSrc(%d), pipeId(%d)",
                frame->getFrameCount(), isSrc, pipeId);
        return ret;
    }

    /* 4. Get the service buffer handle from buffer manager */
    ret = m_bayerBufferMgr->getHandleByIndex(&(streamBuffer.buffer), buffer.index);
    if (ret < 0) {
        CLOGE("Buffer index error(%d)!!", buffer.index);
        return ret;
    }

    /* 5. Update the remained buffer info */
    streamBuffer.status = CAMERA3_BUFFER_STATUS_OK;
    streamBuffer.acquire_fence = -1;
    streamBuffer.release_fence = -1;

    /* 6. Create new result for RAW buffer */
    request = m_requestMgr->getRunningRequest(frame->getFrameCount());
    resultRequest = m_requestMgr->createResultRequest(frame->getFrameCount(), EXYNOS_REQUEST_RESULT::CALLBACK_BUFFER_ONLY, NULL, NULL);
    resultRequest->pushStreamBuffer(&streamBuffer);

    /* 7. Request to callback the result to request manager */
    m_requestMgr->callbackSequencerLock();
    /* increaseCompleteBufferCount not used because of using BUFFER ONLY update */
//    request->increaseCompleteBufferCount();
    m_requestMgr->registerToResultList(resultRequest);
    m_requestMgr->callbackSequencerUnlock();

    CLOGV("request->frame_number(%d), request->getNumOfOutputBuffer(%d)"
            " request->getCompleteBufferCount(%d) frame->getFrameCapture(%d)",
            request->getKey(),
            request->getNumOfOutputBuffer(),
            request->getCompleteBufferCount(),
            frame->getFrameCapture());

    CLOGV("streamBuffer info: stream (%p), handle(%p)",
            streamBuffer.stream, streamBuffer.buffer);

    return ret;
}

status_t ExynosCamera3::m_sendZslStreamResult(ExynosCameraFrameSP_sptr_t frame, __unused uint32_t pipeId, __unused bool isSrc)
{
    status_t ret = NO_ERROR;
    ExynosCameraStream *stream = NULL;
    ExynosCameraRequestSP_sprt_t request = NULL;
    camera3_stream_buffer_t streamBuffer;
    ResultRequest resultRequest = NULL;
    const camera3_stream_buffer_t *buffer;
    const camera3_stream_buffer_t *bufferList;
    int streamId = 0;
    uint32_t bufferCount = 0;

    /* 1. Get stream object for ZSL */
    ret = m_streamManager->getStream(HAL_STREAM_ID_ZSL_OUTPUT, &stream);
    if (ret < 0) {
        CLOGE("getStream is failed, from streammanager. Id error:HAL_STREAM_ID_ZSL");
        return ret;
    }

    /* 2. Get camera3_stream structure from stream object */
    ret = stream->getStream(&streamBuffer.stream);
    if (ret < 0) {
        CLOGE("getStream is failed, from exynoscamerastream. Id error:HAL_STREAM_ID_ZSL");
        return ret;
    }

    /* 3. Get zsl buffer */
    request = m_requestMgr->getRunningRequest(frame->getFrameCount());
    bufferCount = request->getNumOfOutputBuffer();
    bufferList = request->getOutputBuffers();

    for (uint32_t index = 0; index < bufferCount; index++) {
        buffer = &(bufferList[index]);
        stream = static_cast<ExynosCameraStream *>(bufferList[index].stream->priv);
        stream->getID(&streamId);

        if ((streamId % HAL_STREAM_ID_MAX) == HAL_STREAM_ID_ZSL_OUTPUT) {
            streamBuffer.buffer = bufferList[index].buffer;
        }
    }

    /* 4. Update the remained buffer info */
    streamBuffer.status = CAMERA3_BUFFER_STATUS_OK;
    streamBuffer.acquire_fence = -1;
    streamBuffer.release_fence = -1;

    /* 5. Create new result for ZSL buffer */
    resultRequest = m_requestMgr->createResultRequest(frame->getFrameCount(), EXYNOS_REQUEST_RESULT::CALLBACK_BUFFER_ONLY, NULL, NULL);
    resultRequest->pushStreamBuffer(&streamBuffer);

    /* 6. Request to callback the result to request manager */
    m_requestMgr->callbackSequencerLock();
    /* increaseCompleteBufferCount not used because of using BUFFER ONLY update */
//    request->increaseCompleteBufferCount();
    m_requestMgr->registerToResultList(resultRequest);
    m_requestMgr->callbackSequencerUnlock();

    CLOGV("request->frame_number(%d), request->getNumOfOutputBuffer(%d)"
            "request->getCompleteBufferCount(%d) frame->getFrameCapture(%d)",
            request->getKey(),
            request->getNumOfOutputBuffer(),
            request->getCompleteBufferCount(),
            frame->getFrameCapture());

    CLOGV("streamBuffer info: stream (%p), handle(%p)",
            streamBuffer.stream, streamBuffer.buffer);

    return ret;

}

status_t ExynosCamera3::m_sendMeta(uint32_t frameNumber, EXYNOS_REQUEST_RESULT::TYPE type)
{
    ResultRequest resultRequest = NULL;
    ExynosCameraRequestSP_sprt_t request = NULL;
    uint32_t frameCount = 0;
    camera3_capture_result_t requestResult;
    CameraMetadata resultMeta;

    status_t ret = OK;
    request = m_requestMgr->getRunningRequest(frameNumber);
    if (request == NULL) {
        CLOGE("frameCount(%d) request is NULL", frameNumber);
        return BAD_VALUE;
    }

    frameCount = request->getKey();
    request->updatePipelineDepth();
    resultMeta = request->getResultMeta();

    requestResult.frame_number = frameCount;
    requestResult.result = resultMeta.release();
    requestResult.num_output_buffers = 0;
    requestResult.output_buffers = NULL;
    requestResult.input_buffer = NULL;
    requestResult.partial_result = 2;

    CLOGV("DEBUG(%s[%d]):framecount %d ", __FUNCTION__, __LINE__, frameNumber);

    resultRequest = m_requestMgr->createResultRequest(frameNumber, type, &requestResult, NULL);
    m_requestMgr->callbackSequencerLock();
    m_requestMgr->registerToResultList(resultRequest);
    m_requestMgr->callbackSequencerUnlock();

    return ret;
}

status_t ExynosCamera3::m_send3AAMeta(uint32_t frameNumber, EXYNOS_REQUEST_RESULT::TYPE type)
{
    ResultRequest resultRequest = NULL;
    ExynosCameraRequestSP_sprt_t request = NULL;
    uint32_t frameCount = 0;
    camera3_capture_result_t requestResult;
    CameraMetadata resultMeta;

    status_t ret = OK;
    request = m_requestMgr->getRunningRequest(frameNumber);
    frameCount = request->getKey();
    resultMeta = request->get3AAResultMeta();

    requestResult.frame_number = frameCount;
    requestResult.result = resultMeta.release();
    requestResult.num_output_buffers = 0;
    requestResult.output_buffers = NULL;
    requestResult.input_buffer = NULL;
    requestResult.partial_result = 1;

    CLOGV("DEBUG(%s[%d]):framecount %d ", __FUNCTION__, __LINE__, frameNumber);

    resultRequest = m_requestMgr->createResultRequest(frameNumber, type, &requestResult, NULL);
    m_requestMgr->callbackSequencerLock();
    m_requestMgr->registerToResultList(resultRequest);
    m_requestMgr->callbackSequencerUnlock();

    return ret;
}

status_t ExynosCamera3::m_sendNotify(uint32_t frameNumber, int type)
{
    camera3_notify_msg_t notify;
    ResultRequest resultRequest = NULL;
    ExynosCameraRequestSP_sprt_t request = NULL;
    uint32_t frameCount = 0;
    uint64_t timeStamp = m_lastFrametime;

    status_t ret = OK;
    request = m_requestMgr->getRunningRequest(frameNumber);
    frameCount = request->getKey();
    timeStamp = request->getSensorTimestamp();

    CLOGV("framecount %d timestamp %lld", frameNumber, timeStamp);
    switch (type) {
    case CAMERA3_MSG_ERROR:
        notify.type = CAMERA3_MSG_ERROR;
        notify.message.error.frame_number = frameCount;
        notify.message.error.error_code = CAMERA3_MSG_ERROR_BUFFER;
        // TODO: how can handle this?
        //msg.message.error.error_stream = j->stream;
        resultRequest = m_requestMgr->createResultRequest(frameNumber, EXYNOS_REQUEST_RESULT::CALLBACK_NOTIFY_ONLY, NULL, &notify);
        m_requestMgr->callbackSequencerLock();
        m_requestMgr->registerToResultList(resultRequest);
        m_requestMgr->callbackSequencerUnlock();
        break;
    case CAMERA3_MSG_SHUTTER:
        CLOGV(" : SHUTTER (%d)frame t(%lld)", frameNumber, timeStamp);
        notify.type = CAMERA3_MSG_SHUTTER;
        notify.message.shutter.frame_number = frameCount;
        notify.message.shutter.timestamp = timeStamp;
        resultRequest = m_requestMgr->createResultRequest(frameNumber, EXYNOS_REQUEST_RESULT::CALLBACK_NOTIFY_ONLY, NULL, &notify);
        /* keep current frame time for flush ops */
        m_requestMgr->callbackSequencerLock();
        m_requestMgr->registerToResultList(resultRequest);
        m_requestMgr->callbackSequencerUnlock();
        break;
    default:
        CLOGE("Msg type is invalid (%d)", type);
        ret = BAD_VALUE;
        break;
    }

    return ret;
}

status_t ExynosCamera3::m_searchFrameFromList(List<ExynosCameraFrameSP_sptr_t> *list, Mutex *listLock, uint32_t frameCount, ExynosCameraFrameSP_dptr_t frame)
{
    ExynosCameraFrameSP_sptr_t curFrame = NULL;
    List<ExynosCameraFrameSP_sptr_t>::iterator r;

    Mutex::Autolock l(listLock);
    if (list->empty()) {
        CLOGD("list is empty");
        return NO_ERROR;
    }

    r = list->begin()++;

    do {
        curFrame = *r;
        if (curFrame == NULL) {
            CLOGE("curFrame is empty");
            return INVALID_OPERATION;
        }

        if (frameCount == curFrame->getFrameCount()) {
            CLOGV("frame count match: expected(%d)", frameCount);
            frame = curFrame;
            return NO_ERROR;
        }
        r++;
    } while (r != list->end());

    CLOGV("Cannot find match frame, frameCount(%d)", frameCount);

    return OK;
}

status_t ExynosCamera3::m_removeFrameFromList(List<ExynosCameraFrameSP_sptr_t> *list, Mutex *listLock, ExynosCameraFrameSP_sptr_t frame)
{
    ExynosCameraFrameSP_sptr_t curFrame = NULL;
    int frameCount = 0;
    int curFrameCount = 0;
    List<ExynosCameraFrameSP_sptr_t>::iterator r;

    if (frame == NULL) {
        CLOGE("frame is NULL");
        return BAD_VALUE;
    }

    Mutex::Autolock l(listLock);
    if (list->empty()) {
        CLOGE("list is empty");
        return INVALID_OPERATION;
    }

    frameCount = frame->getFrameCount();
    r = list->begin()++;

    do {
        curFrame = *r;
        if (curFrame == NULL) {
            CLOGE("curFrame is empty");
            return INVALID_OPERATION;
        }

        curFrameCount = curFrame->getFrameCount();
        if (frameCount == curFrameCount) {
            CLOGV("frame count match: expected(%d), current(%d)",
                 frameCount, curFrameCount);
            list->erase(r);
            return NO_ERROR;
        }
        CLOGW("frame count mismatch: expected(%d), current(%d)",
             frameCount, curFrameCount);
        /* curFrame->printEntity(); */
        r++;
    } while (r != list->end());

    CLOGE("Cannot find match frame!!!");

    return INVALID_OPERATION;
}

uint32_t ExynosCamera3::m_getSizeFrameFromList(List<ExynosCameraFrameSP_sptr_t> *list, Mutex *listLock)
{
    uint32_t count = 0;
    Mutex::Autolock l(listLock);

    count = list->size();

    return count;
}

status_t ExynosCamera3::m_clearList(List<ExynosCameraFrameSP_sptr_t> *list, Mutex *listLock)
{
    ExynosCameraFrameSP_sptr_t curFrame = NULL;
    List<ExynosCameraFrameSP_sptr_t>::iterator r;

    CLOGD("remaining frame(%zu), we remove them all", list->size());

    Mutex::Autolock l(listLock);

    list->clear();

    return OK;
}

status_t ExynosCamera3::m_removeInternalFrames(List<ExynosCameraFrameSP_sptr_t> *list, Mutex *listLock)
{
    ExynosCameraFrameSP_sptr_t curFrame = NULL;
    List<ExynosCameraFrameSP_sptr_t>::iterator r;

    CLOGD("remaining frame(%zu), we remove internal frames",
             list->size());

    Mutex::Autolock l(listLock);
    while (!list->empty()) {
        r = list->begin()++;
        curFrame = *r;
        if (curFrame != NULL) {
            if (curFrame->getFrameType() == FRAME_TYPE_INTERNAL) {
                CLOGV("remove internal frame(%d)",
                         curFrame->getFrameCount());
                m_releaseInternalFrame(curFrame);
            } else {
                CLOGW("frame(%d) is NOT internal frame and will be remained in List",
                         curFrame->getFrameCount());
            }
        }
        list->erase(r);
        curFrame = NULL;
    }

    return OK;
}

status_t ExynosCamera3::m_releaseInternalFrame(ExynosCameraFrameSP_sptr_t frame)
{
    status_t ret = NO_ERROR;
    ExynosCameraBuffer buffer;
    ExynosCameraBufferManager *bufferMgr = NULL;

    if (frame == NULL) {
        CLOGE("frame is NULL");
        return BAD_VALUE;
    }
    if (frame->getFrameType() != FRAME_TYPE_INTERNAL) {
        CLOGE("frame(%d) is NOT internal frame",
                 frame->getFrameCount());
        return BAD_VALUE;
    }

    ExynosCamera3FrameFactory *factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];

    /* Return bayer buffer */
    if (frame->getRequest(PIPE_VC0) == true) {
        int pipeId = m_getBayerPipeId();
        int dstPos = factory->getNodeType(PIPE_VC0);

        ret = frame->getDstBuffer(pipeId, &buffer, dstPos);
        if (ret != NO_ERROR) {
            CLOGE("getDstBuffer(pipeId(%d), dstPos(%d)) fail", pipeId, dstPos);
        } else if (buffer.index >= 0) {
            ret = m_getBufferManager(PIPE_VC0, &bufferMgr, DST_BUFFER_DIRECTION);
            if (ret != NO_ERROR) {
                CLOGE("Failed to getBufferManager for PIPE_VC0");
            } else {
                ret = m_putBuffers(bufferMgr, buffer.index);
                if (ret != NO_ERROR)
                    CLOGE("Failed to putBuffer for PIPE_VC0. index %d", buffer.index);
            }
        }
    }

    /* Return 3AS buffer */
    ret = frame->getSrcBuffer(PIPE_3AA, &buffer);
    if (ret != NO_ERROR) {
        CLOGE("getSrcBuffer failed. PIPE_3AA, ret(%d)",
                 ret);
    } else if (buffer.index >= 0) {
        ret = m_getBufferManager(PIPE_3AA, &bufferMgr, SRC_BUFFER_DIRECTION);
        if (ret != NO_ERROR) {
            CLOGE("Failed to getBufferManager for 3AS");
        } else {
            ret = m_putBuffers(bufferMgr, buffer.index);
            if (ret != NO_ERROR)
                CLOGE("Failed to putBuffer for 3AS. index %d",
                         buffer.index);
        }
    }

    /* Return 3AP buffer */
    if (frame->getRequest(PIPE_3AP) == true) {
        ret = frame->getDstBuffer(PIPE_3AA, &buffer);
        if (ret != NO_ERROR) {
            CLOGE("getDstBuffer failed. PIPE_3AA, ret %d",
                     ret);
        } else if (buffer.index >= 0) {
            ret = m_getBufferManager(PIPE_3AA, &bufferMgr, DST_BUFFER_DIRECTION);
            if (ret != NO_ERROR) {
                CLOGE("Failed to getBufferManager for 3AP");
            } else {
                ret = m_putBuffers(bufferMgr, buffer.index);
                if (ret != NO_ERROR)
                    CLOGE("Failed to putBuffer for 3AP. index %d",
                             buffer.index);
            }
        }
    }

    frame = NULL;

    return ret;
}

status_t ExynosCamera3::m_releaseRequestFrame(ExynosCameraFrameSP_sptr_t frame)
{
    status_t ret = NO_ERROR;
    camera3_notify_msg_t notify;
    camera3_stream_buffer_t streamBuffer;
    CameraMetadata result;
    ExynosCameraRequestSP_sprt_t request = NULL;
    ResultRequest resultRequest = NULL;
    ExynosCameraStream *stream = NULL;
    ExynosCameraBufferManager *bufferMgr = NULL;
    ExynosCameraBuffer buffer;
    List<int> *outStreamIdList;
    List<int>::iterator outStreamIdListIter;
    uint64_t timeStamp = 0L;
    int streamId = -1;
    int bufferIndex = -1;

    if (frame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        ret = BAD_VALUE;
        goto EXIT;
    }

    request = m_requestMgr->getRunningRequest(frame->getFrameCount());
    if (request == NULL) {
        CLOGW("WARN(%s[%d]):[F%d]No request for this frame",
                __FUNCTION__, __LINE__,
                frame->getFrameCount());
        /* This is NOT error state */
        goto EXIT;
    }

    /* Sned notify callback */
    timeStamp = request->getSensorTimestamp();
    timeStamp += (timeStamp == 0L)? (m_lastFrametime + 15000000) : 0; /* Set dummy timestamp */

    notify.type = CAMERA3_MSG_SHUTTER;
    notify.message.shutter.frame_number = request->getKey();
    notify.message.shutter.timestamp = timeStamp;

    resultRequest = m_requestMgr->createResultRequest(frame->getFrameCount(),
                                                      EXYNOS_REQUEST_RESULT::CALLBACK_NOTIFY_ONLY,
                                                      NULL,
                                                      &notify);
    m_requestMgr->callbackSequencerLock();
    m_requestMgr->registerToResultList(resultRequest);
    m_requestMgr->callbackSequencerUnlock();

    /* Check the validation of service request for this request frame */
    request = m_requestMgr->getRunningRequest(frame->getFrameCount());
    if (request == NULL) {
        CLOGW("WARN(%s[%d]):[F%d]No request for this frame",
                __FUNCTION__, __LINE__,
                frame->getFrameCount());
        /* This is NOT error state */
        goto EXIT;
    }

    /* Send result callback with buffer */
    result = request->getResultMeta();
    result.update(ANDROID_SENSOR_TIMESTAMP, (int64_t *)&timeStamp, 1);
    request->setResultMeta(result);
    m_send3AAMeta(frame->getFrameCount(), EXYNOS_REQUEST_RESULT::CALLBACK_PARTIAL_3AA);
    m_sendMeta(frame->getFrameCount(), EXYNOS_REQUEST_RESULT::CALLBACK_ALL_RESULT);

    if (request->getNumOfInputBuffer() > 0 && m_zslBufferMgr->getNumOfAvailableBuffer() > 0) {
        ret = m_zslBufferMgr->getBuffer(&bufferIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &buffer);
        if (ret != NO_ERROR) {
            CLOGE("[R%d F%d]Failed to getInputBuffer. ret %d",
                    request->getKey(),
                    frame->getFrameCount(),
                    ret);

            /* Continue to handle other streams */
        }

        /* Input buffer will be handled with output buffers */
    }

    request->getAllRequestOutputStreams(&outStreamIdList);
    if (outStreamIdList->size() > 0) {
        CLOGI("INFO(%s[%d]):[R%d F%d]Release outStreams. size %zu",
                __FUNCTION__, __LINE__,
                request->getKey(),
                frame->getFrameCount(),
                outStreamIdList->size());

        outStreamIdListIter = outStreamIdList->begin();
        for (int i = outStreamIdList->size(); i > 0; i--, outStreamIdListIter++) {
            if (outStreamIdListIter == outStreamIdList->end()) {
                break;
            }

            m_streamManager->getStream(*outStreamIdListIter, &stream);
            if (stream == NULL) {
                CLOGE("ERR(%s[%d]):[R%d F%d]Stream(%d) is NULL.",
                        __FUNCTION__, __LINE__,
                        request->getKey(),
                        frame->getFrameCount(),
                        *outStreamIdListIter);

                /* Continue to handle other streams */
                continue;
            }

            stream->getID(&streamId);
            ret = stream->getStream(&streamBuffer.stream);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):[R%d F%d S%d]Failed to getStream.",
                        __FUNCTION__, __LINE__,
                        request->getKey(),
                        frame->getFrameCount(),
                        streamId);

                /* Continue to handle other streams */
                continue;
            }

            stream->getBufferManager(&bufferMgr);
            ret = m_checkStreamBuffer(frame, stream, &buffer);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):[R%d F%d S%d]Failed to getBuffer. ret %d",
                        __FUNCTION__, __LINE__,
                        request->getKey(),
                        frame->getFrameCount(),
                        streamId,
                        ret);

                /* Continue to handle other streams */
                continue;
            }

            ret = bufferMgr->getHandleByIndex(&streamBuffer.buffer, buffer.index);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):[R%d F%d S%d B%d]Failed to getStreamBuffer",
                        __FUNCTION__, __LINE__,
                        request->getKey(),
                        frame->getFrameCount(),
                        streamId,
                        buffer.index);

                /* Continue to handle other streams */
                continue;
            }

            /* Sned buffer callback */
            streamBuffer.status = CAMERA3_BUFFER_STATUS_OK;
            streamBuffer.acquire_fence = -1;
            streamBuffer.release_fence = -1;

            resultRequest = m_requestMgr->createResultRequest(frame->getFrameCount(),
                                                              EXYNOS_REQUEST_RESULT::CALLBACK_BUFFER_ONLY,
                                                              NULL, NULL);
            resultRequest->pushStreamBuffer(&streamBuffer);
            m_requestMgr->callbackSequencerLock();
            m_requestMgr->registerToResultList(resultRequest);
            m_requestMgr->callbackSequencerUnlock();
        }
    }

EXIT:
    frame = NULL;

    return ret;
}

status_t ExynosCamera3::m_checkStreamBuffer(ExynosCameraFrameSP_sptr_t frame, ExynosCameraStream *stream, ExynosCameraBuffer *buffer)
{
    status_t ret = NO_ERROR;
    ExynosCameraRequestSP_sprt_t request = NULL;
    ExynosCameraBufferManager *bufferMgr = NULL;
    ExynosCamera3FrameFactory *factory = NULL;

    int bufferIndex = -2;
    int streamId = -1;
    bool usePureBayer = false;

    int pipeId = -1;
    int parentPipeId = -1;

    int32_t reprocessingBayerMode = m_parameters->getReprocessingBayerMode();

    buffer->index = -2;

    if (frame == NULL) {
        CLOGE("frame is NULL");
        ret = BAD_VALUE;
        goto func_exit;
    }

    if (stream == NULL) {
        CLOGE("stream is NULL");
        ret = BAD_VALUE;
        goto func_exit;
    }

    request = m_requestMgr->getRunningRequest(frame->getFrameCount());
    if (request == NULL) {
        CLOGW("[F%d]No request for this frame",
                frame->getFrameCount());
        /* This is NOT error state */
        ret = BAD_VALUE;
        goto func_exit;
    }

    if (frame->getFrameType() == FRAME_TYPE_PREVIEW) {
        if (m_parameters->isIspMcscOtf() == false) {
            parentPipeId = PIPE_MCSC;
        } else if (m_parameters->is3aaIspOtf() == false) {
            parentPipeId = PIPE_ISP;
        } else {
            parentPipeId = PIPE_3AA;
        }
    } else if (frame->getFrameType() == FRAME_TYPE_REPROCESSING) {
        if (m_parameters->isReprocessingIspMcscOtf() == false) {
            parentPipeId = PIPE_MCSC_REPROCESSING;
        } else if (m_parameters->isReprocessing3aaIspOTF() == false) {
            parentPipeId = PIPE_ISP_REPROCESSING;
        } else {
            parentPipeId = PIPE_3AA_REPROCESSING;
        }
    }

    switch (reprocessingBayerMode) {
    case REPROCESSING_BAYER_MODE_NONE :
        break;
    case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON :
    case REPROCESSING_BAYER_MODE_PURE_DYNAMIC :
        usePureBayer = true;
        break;
    case REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON :
    case REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC :
        usePureBayer = false;
        break;
    default :
        break;
    }

    stream->getID(&streamId);

    switch (streamId % HAL_STREAM_ID_MAX) {
    case HAL_STREAM_ID_RAW:
        CLOGV("[R%d F%d] buffer-StreamType(HAL_STREAM_ID_RAW)",
                request->getKey(), frame->getFrameCount());
        pipeId = PIPE_FLITE;
        factory = request->getFrameFactory(streamId);
        break;
    case HAL_STREAM_ID_ZSL_OUTPUT:
        ALOGV("[R%d F%d] buffer-StreamType(HAL_STREAM_ID_ZSL)",
            request->getKey(), frame->getFrameCount());
        pipeId = (usePureBayer == true) ? PIPE_FLITE : PIPE_3AC;
        factory = request->getFrameFactory(streamId);
        break;
    case HAL_STREAM_ID_PREVIEW:
        CLOGV("[R%d F%d] buffer-StreamType(HAL_STREAM_ID_PREVIEW)",
            request->getKey(), frame->getFrameCount());
        pipeId = m_streamManager->getOutputPortId(streamId) + PIPE_MCSC0;
        factory = request->getFrameFactory(streamId);
        break;
    case HAL_STREAM_ID_CALLBACK:
        CLOGV("[R%d F%d] buffer-StreamType(HAL_STREAM_ID_CALLBACK)",
            request->getKey(), frame->getFrameCount());
        pipeId = m_streamManager->getOutputPortId(streamId) + PIPE_MCSC0;
        factory = request->getFrameFactory(streamId);
        break;
    case HAL_STREAM_ID_CALLBACK_STALL:
        CLOGV("[R%d F%d] buffer-StreamType(HAL_STREAM_ID_CALLBACK_STALL)",
            request->getKey(), frame->getFrameCount());
        pipeId = (m_streamManager->getOutputPortId(streamId) % ExynosCamera3Parameters::YUV_MAX)
                + PIPE_MCSC0_REPROCESSING;
        factory = request->getFrameFactory(streamId);
        break;
    case HAL_STREAM_ID_VIDEO:
        CLOGV("[R%d F%d] buffer-StreamType(HAL_STREAM_ID_VIDEO)",
            request->getKey(), frame->getFrameCount());
        pipeId = m_streamManager->getOutputPortId(streamId) + PIPE_MCSC0;
        factory = request->getFrameFactory(streamId);
        break;
    case HAL_STREAM_ID_JPEG:
        CLOGD("[R%d F%d] buffer-StreamType(HAL_STREAM_ID_JPEG)",
            request->getKey(), frame->getFrameCount());
        pipeId = (m_parameters->isUseHWFC() == true) ? PIPE_HWFC_JPEG_DST_REPROCESSING : PIPE_JPEG_REPROCESSING;
        factory = request->getFrameFactory(streamId);
        break;
    default:
        CLOGE("Invalid stream ID %d", streamId);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    stream->getBufferManager(&bufferMgr);

    if (pipeId > 0) {
        if (pipeId == PIPE_FLITE || pipeId == PIPE_JPEG_REPROCESSING) {
            ret = frame->getDstBuffer(pipeId, buffer);
        } else {
            ret = frame->getDstBuffer(parentPipeId, buffer, factory->getNodeType(pipeId));
        }

        if (ret != NO_ERROR) {
            CLOGE("[R%d F%d] getDstBuffer fail. pipeId (%d) ret(%d)",
                request->getKey(), frame->getFrameCount(), pipeId, ret);
            goto func_exit;
        }
    }

    if (buffer->index < 0) {
        ret = bufferMgr->getBuffer(&bufferIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, buffer);
        if (ret != NO_ERROR) {
            CLOGE("[R%d F%d S%d]Failed to getBuffer. ret %d",
                request->getKey(),
                frame->getFrameCount(),
                streamId,
                ret);
            goto func_exit;
        }
    }

func_exit:
    return ret;
}

status_t ExynosCamera3::m_setFrameManager()
{
    sp<FrameWorker> worker;

    m_frameMgr = new ExynosCameraFrameManager("FRAME MANAGER", m_cameraId, FRAMEMGR_OPER::SLIENT);

    worker = new CreateWorker("CREATE FRAME WORKER", m_cameraId, FRAMEMGR_OPER::SLIENT, 300, 200);
    m_frameMgr->setWorker(FRAMEMGR_WORKER::CREATE, worker);

    worker = new RunWorker("RUNNING FRAME WORKER", m_cameraId, FRAMEMGR_OPER::SLIENT, 100, 300);
    m_frameMgr->setWorker(FRAMEMGR_WORKER::RUNNING, worker);

    sp<KeyBox> key = new KeyBox("FRAME KEYBOX", m_cameraId);

    m_frameMgr->setKeybox(key);

    return NO_ERROR;
}

bool ExynosCamera3::m_frameFactoryCreateThreadFunc(void)
{

#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif
    bool loop = false;
    status_t ret = NO_ERROR;

    ExynosCamera3FrameFactory *framefactory = NULL;

    ret = m_frameFactoryQ->waitAndPopProcessQ(&framefactory);
    if (ret < 0) {
        CLOGE("wait and pop fail, ret(%d)", ret);
        goto func_exit;
    }

    if (framefactory == NULL) {
        CLOGE("framefactory is NULL");
        goto func_exit;
    }

    if (framefactory->isCreated() == false) {
        CLOGD("framefactory create");
        framefactory->create();
    } else {
        CLOGD("framefactory already create");
    }

func_exit:
    if (0 < m_frameFactoryQ->getSizeOfProcessQ()) {
        loop = true;
    }

    return loop;
}

bool ExynosCamera3::m_frameFactoryStartThreadFunc(void)
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif
    status_t ret = NO_ERROR;
    ExynosCamera3FrameFactory *factory = NULL;
    int32_t reprocessingBayerMode = m_parameters->getReprocessingBayerMode();
    uint32_t prepare = m_exynosconfig->current->pipeInfo.prepare[PIPE_3AA];

    if (m_requestMgr->getServiceRequestCount() < 1) {
        CLOGE("There is NO available request!!! "
            "\"processCaptureRequest()\" must be called, first!!!");
        return false;
    }

    factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
    if (factory == NULL) {
        CLOGE("Can't start FrameFactory!!!! FrameFactory is NULL!! Prepare(%d), Request(%d)",
                prepare,
                m_requestMgr != NULL ? m_requestMgr->getAllRequestCount(): 0);

        return false;
    } else if (factory->isCreated() == false) {
        CLOGE("Preview FrameFactory is NOT created!");
        return false;
    } else if (factory->isRunning() == true) {
        CLOGW("Preview FrameFactory is already running.");
        return false;
    }

    m_internalFrameCount = FRAME_INTERNAL_START_COUNT;

    /* Set default request flag & buffer manager */
    ret = m_setupPipeline();
    if (ret != NO_ERROR) {
        CLOGE("Failed to setupPipeline. ret %d",
                 ret);
        return false;
    }

    ret = factory->initPipes();
    if (ret != NO_ERROR) {
        CLOGE("Failed to initPipes. ret %d",
                 ret);
        return false;
    }

    ret = factory->mapBuffers();
    if (ret != NO_ERROR) {
        CLOGE("m_previewFrameFactory->mapBuffers() failed");
        return ret;
    }

    CLOGD("prepare %d", prepare);
    for (uint32_t i = 0; i < prepare + 1; i++) {
        ret = m_createFrameFunc();
        if (ret != NO_ERROR)
            CLOGE("Failed to createFrameFunc for preparing frame. prepareCount %d/%d",
                     i, prepare);
    }

    /* - call preparePipes();
     * - call startPipes()
     * - call startInitialThreads()
     */
    m_startFrameFactory(factory);

    if (m_parameters->isReprocessing() == true) {
        m_reprocessingFrameFactoryStartThread->run();
    }

    if (m_shotDoneQ != NULL)
        m_shotDoneQ->release();

    for (int i = 0; i < MAX_PIPE_NUM; i++) {
        if (m_pipeFrameDoneQ[i] != NULL) {
            m_pipeFrameDoneQ[i]->release();
        }
    }

    m_yuvCaptureDoneQ->release();
    m_reprocessingDoneQ->release();

    m_mainThread->run(PRIORITY_URGENT_DISPLAY);
    m_previewStream3AAThread->run(PRIORITY_URGENT_DISPLAY);

#ifdef USE_VRA_GROUP
    if (m_parameters->isMcscVraOtf() == false) {
        m_previewStreamVRAThread->run(PRIORITY_URGENT_DISPLAY);
    }
#endif

    if (m_parameters->is3aaIspOtf() == false)
        m_previewStreamISPThread->run(PRIORITY_URGENT_DISPLAY);

    if (m_parameters->isIspMcscOtf() == false)
        m_previewStreamMCSCThread->run(PRIORITY_URGENT_DISPLAY);

    if (m_parameters->isFlite3aaOtf() == false) {
        m_previewStreamBayerThread->run(PRIORITY_URGENT_DISPLAY);

        if (factory->checkPipeThreadRunning(m_getBayerPipeId()) == false)
            factory->startThread(m_getBayerPipeId());
    }

    m_monitorThread->run(PRIORITY_DEFAULT);

    m_internalFrameThread->run(PRIORITY_DEFAULT);

    return false;
}

status_t ExynosCamera3::m_constructFrameFactory(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    CLOGI("-IN-");

    ExynosCamera3FrameFactory *factory = NULL;

    for(int i = 0; i < FRAME_FACTORY_TYPE_MAX; i++)
        m_frameFactory[i] = NULL;

    factory = new ExynosCamera3FrameFactoryPreview(m_cameraId, m_parameters);
    factory->setFrameCreateHandler(&ExynosCamera3::m_previewFrameHandler);
    m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW] = factory;
    m_frameFactory[FRAME_FACTORY_TYPE_RECORDING_PREVIEW] = factory;
    m_frameFactory[FRAME_FACTORY_TYPE_DUAL_PREVIEW] = factory;

    if (m_parameters->isReprocessing() == true) {
        factory = new ExynosCamera3FrameReprocessingFactory(m_cameraId, m_parameters);
        factory->setFrameCreateHandler(&ExynosCamera3::m_captureFrameHandler);
        m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING] = factory;
    }

    m_waitCompanionThreadEnd();

    for (int i = 0; i < FRAME_FACTORY_TYPE_MAX; i++) {
        factory = m_frameFactory[i];
        if ((factory != NULL) && (factory->isCreated() == false)) {
            factory->setFrameManager(m_frameMgr);
            m_frameFactoryQ->pushProcessQ(&factory);
        }
    }

    CLOGI("-OUT-");

    return NO_ERROR;
}

status_t ExynosCamera3::m_startFrameFactory(ExynosCamera3FrameFactory *factory)
{
    status_t ret = OK;

    /* prepare pipes */
    ret = factory->preparePipes();
    if (ret < 0) {
        CLOGW("Failed to prepare FLITE");
    }

    /* stream on pipes */
    ret = factory->startPipes();
    if (ret < 0) {
        CLOGE("startPipe fail");
        return ret;
    }

    /* start all thread */
    ret = factory->startInitialThreads();
    if (ret < 0) {
        CLOGE("startInitialThreads fail");
        return ret;
    }

    return ret;
}

status_t ExynosCamera3::m_stopFrameFactory(ExynosCamera3FrameFactory *factory)
{
    int ret = 0;

    CLOGD("");
    if (factory != NULL  && factory->isRunning() == true) {
        ret = factory->stopPipes();
        if (ret < 0) {
            CLOGE("stopPipe fail");
            /* stopPipe fail but all node closed */
            /* return ret; */
        }
    }

    return ret;
}

status_t ExynosCamera3::m_deinitFrameFactory()
{
    CLOGI("-IN-");

    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = NO_ERROR;
    ExynosCamera3FrameFactory *frameFactory = NULL;

    for (int i = 0; i < FRAME_FACTORY_TYPE_MAX; i++) {
        if (m_frameFactory[i] != NULL) {
            frameFactory = m_frameFactory[i];

            for (int k = i + 1; k < FRAME_FACTORY_TYPE_MAX; k++) {
               if (frameFactory == m_frameFactory[k]) {
                   CLOGD("m_frameFactory index(%d) and index(%d) are same instance, "
                        "set index(%d) = NULL", i, k, k);
                   m_frameFactory[k] = NULL;
               }
            }

            if (m_frameFactory[i]->isCreated() == true) {
                ret = m_frameFactory[i]->destroy();
                if (ret < 0)
                    CLOGE("m_frameFactory[%d] destroy fail", i);

                SAFE_DELETE(m_frameFactory[i]);

                CLOGD("m_frameFactory[%d] destroyed", i);
            }
        }
    }

    CLOGI("-OUT-");

    return ret;

}

status_t ExynosCamera3::m_setSetfile(void) {
    int configMode = m_parameters->getConfigMode();
    int setfile = 0;
    int setfileReprocessing = 0;
    int yuvRange = YUV_FULL_RANGE;
    int yuvRangeReprocessing = YUV_FULL_RANGE;
    uint32_t minFps = 0, maxFps = 0;

    switch (configMode) {
    case CONFIG_MODE::NORMAL:
        m_parameters->checkSetfileYuvRange();
        /* general */
        m_parameters->getSetfileYuvRange(false, &setfile, &yuvRange);
        /* reprocessing */
        m_parameters->getSetfileYuvRange(true, &setfileReprocessing, &yuvRangeReprocessing);
        break;
    case CONFIG_MODE::HIGHSPEED_120:
        setfile = ISS_SUB_SCENARIO_VIDEO_HIGH_SPEED;
        setfileReprocessing = ISS_SUB_SCENARIO_VIDEO_HIGH_SPEED;
        yuvRange = YUV_LIMITED_RANGE;
        break;
    case CONFIG_MODE::HIGHSPEED_240:
        m_parameters->getPreviewFpsRange(&minFps, &maxFps);
        if (maxFps == 120) {
            setfile = ISS_SUB_SCENARIO_VIDEO_HIGH_SPEED;
        } else {
            setfile = ISS_SUB_SCENARIO_FHD_240FPS;
        }
        setfileReprocessing = ISS_SUB_SCENARIO_VIDEO_HIGH_SPEED;
        yuvRange = YUV_LIMITED_RANGE;
        break;
    default:
        CLOGE("configMode is abnormal(%d)", configMode);
        break;
    }

    CLOGI("configMode(%d), dualMode(%d)",
        configMode, m_parameters->getDualMode());

    /* reprocessing */
    m_parameters->setSetfileYuvRange(true, setfileReprocessing, yuvRangeReprocessing);
    /* preview */
    m_parameters->setSetfileYuvRange(false, setfile, yuvRange);

    return NO_ERROR;
}

status_t ExynosCamera3::m_setupPipeline(void)
{
    status_t ret = NO_ERROR;
    uint32_t pipeId = MAX_PIPE_NUM;
    int32_t reprocessingBayerMode = m_parameters->getReprocessingBayerMode();
    int streamId = -1;
    m_setSetfile();
    ExynosCamera3FrameFactory *factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
    ExynosCameraStream *stream = NULL;
    ExynosCameraBufferManager *bufferMgr = NULL;

    ExynosCameraBufferManager *fliteBufferManager[MAX_NODE];
    ExynosCameraBufferManager *taaBufferManager[MAX_NODE];
    ExynosCameraBufferManager *ispBufferManager[MAX_NODE];
    ExynosCameraBufferManager *mcscBufferManager[MAX_NODE];
#ifdef USE_VRA_GROUP
    ExynosCameraBufferManager *vraBufferManager[MAX_NODE];
#endif
    ExynosCameraBufferManager **tempBufferManager;

    for (int i = 0; i < MAX_NODE; i++) {
        fliteBufferManager[i] = NULL;
        taaBufferManager[i] = NULL;
        ispBufferManager[i] = NULL;
        mcscBufferManager[i] = NULL;
#ifdef USE_VRA_GROUP
        vraBufferManager[i] = NULL;
#endif
    }

    /* Setting default Bayer DMA-out request flag */
    switch (reprocessingBayerMode) {
    case REPROCESSING_BAYER_MODE_NONE : /* Not using reprocessing */
        CLOGD(" Use REPROCESSING_BAYER_MODE_NONE");
        factory->setRequestBayer(false);
        factory->setRequest3AC(false);
        break;
    case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON :
        CLOGD(" Use REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON");
        factory->setRequestBayer(true);
        factory->setRequest3AC(false);
        break;
    case REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON :
        CLOGD(" Use REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON");
        factory->setRequestBayer(false);
        factory->setRequest3AC(true);
        break;
    case REPROCESSING_BAYER_MODE_PURE_DYNAMIC :
        CLOGD(" Use REPROCESSING_BAYER_MODE_PURE_DYNAMIC");
        factory->setRequestBayer(false);
        factory->setRequest3AC(false);
        break;
    case REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC :
        CLOGD(" Use REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC");
        factory->setRequestBayer(false);
        factory->setRequest3AC(false);
        break;
    default :
        CLOGE(" Unknown dynamic bayer mode");
        factory->setRequest3AC(false);
        break;
    }

    if (m_parameters->isFlite3aaOtf() == false)
        factory->setRequestBayer(true);

    /* Setting bufferManager based on H/W pipeline */
    if (m_parameters->isFlite3aaOtf() == true)  {
        tempBufferManager = taaBufferManager;
        pipeId = PIPE_3AA;

        tempBufferManager[factory->getNodeType(PIPE_VC0)] = m_fliteBufferMgr;
        tempBufferManager[factory->getNodeType(PIPE_3AA)] = m_3aaBufferMgr;
        tempBufferManager[factory->getNodeType(PIPE_3AC)] = m_fliteBufferMgr;
        tempBufferManager[factory->getNodeType(PIPE_3AP)] = m_ispBufferMgr;
    } else {
        tempBufferManager = fliteBufferManager;
        pipeId = PIPE_FLITE;

        tempBufferManager[factory->getNodeType(PIPE_VC0)] = m_fliteBufferMgr;

        ret = factory->setBufferManagerToPipe(tempBufferManager, pipeId);
        if (ret != NO_ERROR) {
            CLOGE("Failed to setBufferManagerToPipe into pipeId %d", pipeId);
            return ret;
        }

        tempBufferManager = taaBufferManager;
        pipeId = PIPE_3AA;

        tempBufferManager[factory->getNodeType(PIPE_3AA)] = m_fliteBufferMgr;
        tempBufferManager[factory->getNodeType(PIPE_3AC)] = m_fliteBufferMgr;
        tempBufferManager[factory->getNodeType(PIPE_3AP)] = m_ispBufferMgr;
    }

    if (m_parameters->is3aaIspOtf() == false)  {
        ret = factory->setBufferManagerToPipe(tempBufferManager, pipeId);
        if (ret != NO_ERROR) {
            CLOGE("Failed to setBufferManagerToPipe into pipeId %d",
                     pipeId);
            return ret;
        }

        // Then, set on ISP.
        tempBufferManager = ispBufferManager;
        pipeId = PIPE_ISP;
    }

    tempBufferManager[factory->getNodeType(PIPE_ISP)] = m_ispBufferMgr;
    tempBufferManager[factory->getNodeType(PIPE_ISPC)] = m_mcscBufferMgr;
    tempBufferManager[factory->getNodeType(PIPE_ISPP)] = m_mcscBufferMgr;

    if (m_parameters->isIspMcscOtf() == false) {
        ret = factory->setBufferManagerToPipe(tempBufferManager, pipeId);
        if (ret != NO_ERROR) {
            CLOGE("Failed to setBufferManagerToPipe into pipeId %d",
                     pipeId);
            return ret;
        }
        tempBufferManager = mcscBufferManager;
        pipeId = PIPE_MCSC;
    }

    tempBufferManager[factory->getNodeType(PIPE_MCSC)] = m_mcscBufferMgr;

    /* MCSC0, 1, 2 */
    for (int i = 0; i < m_streamManager->getTotalYuvStreamCount(); i++) {
        streamId = m_streamManager->getYuvStreamId(i);
        if (streamId == -1) {
            /* processed by reprocessing stream */
            continue;
        }

        ret = m_streamManager->getStream(streamId, &stream);
        if (ret != NO_ERROR) {
            CLOGE("Failed to getStream. outputPortId %d streamId %d",
                     i, streamId);
            continue;
        }

        ret = stream->getBufferManager(&bufferMgr);
        if (ret != NO_ERROR) {
            CLOGE("Failed to getBufferMgr. outputPortId %d streamId %d",
                     i, streamId);
            continue;
        }

        tempBufferManager[factory->getNodeType(PIPE_MCSC0 + i)] = bufferMgr;
    }

#ifdef USE_VRA_GROUP
    tempBufferManager[factory->getNodeType(PIPE_MCSC_DS)] = m_vraBufferMgr;

    if (m_parameters->isMcscVraOtf() == false)  {
        ret = factory->setBufferManagerToPipe(tempBufferManager, pipeId);
        if (ret != NO_ERROR) {
            CLOGE("Failed to setBufferManagerToPipe into pipeId %d",
                     pipeId);
            return ret;
        }

        // Then, set on ISP.
        tempBufferManager = vraBufferManager;
        pipeId = PIPE_VRA;

        tempBufferManager[factory->getNodeType(PIPE_VRA)] = m_vraBufferMgr;
    }
#endif
    ret = factory->setBufferManagerToPipe(tempBufferManager, pipeId);
    if (ret != NO_ERROR) {
        CLOGE("Failed to setBufferManagerToPipe into pipeId %d ",
                 pipeId);
        return ret;
    }

    /* Setting OutputFrameQ/FrameDoneQ to Pipe */
    if (m_parameters->isFlite3aaOtf() == false)
    {
        pipeId = PIPE_FLITE;
        factory->setOutputFrameQToPipe(m_pipeFrameDoneQ[pipeId], pipeId);
    }

    pipeId = PIPE_3AA;

    if (m_parameters->is3aaIspOtf() == false || m_parameters->isIspMcscOtf() == false)
        factory->setOutputFrameQToPipe(m_pipeFrameDoneQ[pipeId], pipeId);

    if (m_parameters->is3aaIspOtf() == false)
        pipeId = PIPE_ISP;

    if (m_parameters->isIspMcscOtf() == false) {
        factory->setFrameDoneQToPipe(m_pipeFrameDoneQ[pipeId], pipeId);
        pipeId = PIPE_MCSC;
    }

#ifdef USE_VRA_GROUP
    if (m_parameters->isMcscVraOtf() == false) {
        factory->setOutputFrameQToPipe(m_pipeFrameDoneQ[pipeId], pipeId);
        pipeId = PIPE_VRA;
    }
#endif

    factory->setOutputFrameQToPipe(m_pipeFrameDoneQ[pipeId], pipeId);

    return ret;
}

status_t ExynosCamera3::m_setupReprocessingPipeline(void)
{
    status_t ret = NO_ERROR;
    uint32_t pipeId = MAX_PIPE_NUM;
    ExynosCamera3FrameFactory *factory = m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING];
    ExynosCameraStream *stream = NULL;
    ExynosCameraBufferManager *bufferMgr = NULL;
    ExynosCameraBufferManager *taaBufferManager[MAX_NODE];
    ExynosCameraBufferManager *ispBufferManager[MAX_NODE];
    ExynosCameraBufferManager *mcscBufferManager[MAX_NODE];
    ExynosCameraBufferManager **tempBufferManager;

    for (int i = 0; i < MAX_NODE; i++) {
        taaBufferManager[i] = NULL;
        ispBufferManager[i] = NULL;
        mcscBufferManager[i] = NULL;
    }

    /* Setting bufferManager based on H/W pipeline */
    tempBufferManager = taaBufferManager;
    pipeId = PIPE_3AA_REPROCESSING;

    tempBufferManager[factory->getNodeType(PIPE_3AA_REPROCESSING)] = m_fliteBufferMgr;
    tempBufferManager[factory->getNodeType(PIPE_3AP_REPROCESSING)] = m_ispReprocessingBufferMgr;

    if (m_parameters->isReprocessing3aaIspOTF() == false) {
        if (m_parameters->getUsePureBayerReprocessing() == true) {
            ret = factory->setBufferManagerToPipe(tempBufferManager, pipeId);
            if (ret != NO_ERROR) {
                CLOGE("Failed to setBufferManagerToPipe into pipeId %d",
                         pipeId);
                return ret;
            }
        }
        tempBufferManager = ispBufferManager;
        pipeId = PIPE_ISP_REPROCESSING;
    }

    tempBufferManager[factory->getNodeType(PIPE_ISP_REPROCESSING)] = m_ispReprocessingBufferMgr;
    tempBufferManager[factory->getNodeType(PIPE_ISPC_REPROCESSING)] = m_yuvCaptureBufferMgr;

    if (m_parameters->isReprocessingIspMcscOtf() == false) {
        ret = factory->setBufferManagerToPipe(tempBufferManager, pipeId);
        if (ret != NO_ERROR) {
            CLOGE("Failed to setBufferManagerToPipe into pipeId %d",
                     pipeId);
            return ret;
        }
        tempBufferManager = mcscBufferManager;
        pipeId = PIPE_MCSC;
    }

    /* MCSC0, 1, 2 */
    for (int i = 0; i < m_streamManager->getTotalYuvStreamCount(); i++) {
        int stallIndex = ExynosCamera3Parameters::YUV_STALL_0 + i;
        int streamId = m_streamManager->getYuvStreamId(stallIndex);
        if (streamId % HAL_STREAM_ID_MAX != HAL_STREAM_ID_CALLBACK
            && streamId % HAL_STREAM_ID_MAX != HAL_STREAM_ID_CALLBACK_STALL) {
            /* Only YUV_CALLBACK can be processed */
            continue;
        }

        ret = m_streamManager->getStream(streamId, &stream);
        if (ret != NO_ERROR) {
            CLOGE("Failed to getStream. outputPortId %d streamId %d",
                    i, streamId);
            continue;
        }

        ret = stream->getBufferManager(&bufferMgr);
        if (ret != NO_ERROR) {
            CLOGE("Failed to getBufferMgr. outputPortId %d streamId %d",
                    i, streamId);
            continue;
        }

        tempBufferManager[factory->getNodeType(PIPE_MCSC0_REPROCESSING + i)] = bufferMgr;
    }

    if(m_parameters->isSingleChain() && m_parameters->isSupportZSLInput()) {
        tempBufferManager[factory->getNodeType(PIPE_MCSC2_REPROCESSING)] = m_internalScpBufferMgr;
    }

    tempBufferManager[factory->getNodeType(PIPE_MCSC3_REPROCESSING)] = m_yuvCaptureBufferMgr;
    tempBufferManager[factory->getNodeType(PIPE_MCSC4_REPROCESSING)] = m_thumbnailBufferMgr;
    tempBufferManager[factory->getNodeType(PIPE_HWFC_JPEG_SRC_REPROCESSING)] = m_yuvCaptureBufferMgr;
    tempBufferManager[factory->getNodeType(PIPE_HWFC_THUMB_SRC_REPROCESSING)] = m_thumbnailBufferMgr;
    /* Dummy buffer manager */
    tempBufferManager[factory->getNodeType(PIPE_HWFC_THUMB_DST_REPROCESSING)] = m_thumbnailBufferMgr;

    if (m_streamManager->findStream(HAL_STREAM_ID_JPEG) == true) {
        ret = m_streamManager->getStream(HAL_STREAM_ID_JPEG, &stream);
        if (ret != NO_ERROR)
            CLOGE("Failed to getStream from streamMgr. HAL_STREAM_ID_JPEG");

        ret = stream->getBufferManager(&bufferMgr);
        if (ret != NO_ERROR)
            CLOGE("Failed to getBufferMgr. HAL_STREAM_ID_JPEG");

        tempBufferManager[factory->getNodeType(PIPE_HWFC_JPEG_DST_REPROCESSING)] = bufferMgr;
    }

    ret = factory->setBufferManagerToPipe(tempBufferManager, pipeId);
    if (ret != NO_ERROR) {
        CLOGE("Failed to setBufferManagerToPipe into pipeId %d",
                 pipeId);
        return ret;
    }

    /* Setting OutputFrameQ/FrameDoneQ to Pipe */
    if(m_parameters->getUsePureBayerReprocessing()) {
        // Pure bayer reprocessing
        pipeId = PIPE_3AA_REPROCESSING;
    } else {
        // Dirty bayer reprocessing
        pipeId = PIPE_ISP_REPROCESSING;
    }

    /* TODO : Consider the M2M Reprocessing Scenario */
    factory->setOutputFrameQToPipe(m_yuvCaptureDoneQ, pipeId);
    factory->setFrameDoneQToPipe(m_reprocessingDoneQ, pipeId);

    return ret;
}

void ExynosCamera3::m_updateCropRegion(struct camera2_shot_ext *shot_ext)
{
    int sensorMaxW = 0, sensorMaxH = 0;
    int cropRegionMinW = 0, cropRegionMinH = 0;
    int maxZoom = 0;

    if (shot_ext == NULL) {
        CLOGE("shot_ext is NULL");
        return;
    }

    m_parameters->getMaxSensorSize(&sensorMaxW, &sensorMaxH);

    shot_ext->shot.ctl.scaler.cropRegion[0] = ALIGN_DOWN(shot_ext->shot.ctl.scaler.cropRegion[0], 2);
    shot_ext->shot.ctl.scaler.cropRegion[1] = ALIGN_DOWN(shot_ext->shot.ctl.scaler.cropRegion[1], 2);
    shot_ext->shot.ctl.scaler.cropRegion[2] = ALIGN_UP(shot_ext->shot.ctl.scaler.cropRegion[2], 2);
    shot_ext->shot.ctl.scaler.cropRegion[3] = ALIGN_UP(shot_ext->shot.ctl.scaler.cropRegion[3], 2);

    /*  Check the validation of the crop size(width x height).
     * The width and height of the crop region cannot be set to be smaller than
     floor( activeArraySize.width / android.scaler.availableMaxDigitalZoom )
     and floor( activeArraySize.height / android.scaler.availableMaxDigitalZoom )
     */
    maxZoom = (int) (m_parameters->getMaxZoomRatio() / 1000);
    cropRegionMinW = sensorMaxW / maxZoom;
    cropRegionMinH = sensorMaxH / maxZoom;

    if (cropRegionMinW > (int) shot_ext->shot.ctl.scaler.cropRegion[2]
        || cropRegionMinH > (int) shot_ext->shot.ctl.scaler.cropRegion[3]){
        CLOGE("Invalid Crop Size(%d, %d), cropRegionMin(%d, %d)",
                shot_ext->shot.ctl.scaler.cropRegion[2],
                shot_ext->shot.ctl.scaler.cropRegion[3],
                cropRegionMinW, cropRegionMinH);

        shot_ext->shot.ctl.scaler.cropRegion[2] = cropRegionMinW;
        shot_ext->shot.ctl.scaler.cropRegion[3] = cropRegionMinH;
    }

    /* 1. Check the validation of the crop size(width x height).
     * The crop size must be smaller than sensor max size.
     */
    if (sensorMaxW < (int) shot_ext->shot.ctl.scaler.cropRegion[2]
        || sensorMaxH < (int)shot_ext->shot.ctl.scaler.cropRegion[3]) {
        CLOGE("Invalid Crop Size(%d, %d), sensorMax(%d, %d)",
                shot_ext->shot.ctl.scaler.cropRegion[2],
                shot_ext->shot.ctl.scaler.cropRegion[3],
                sensorMaxW, sensorMaxH);
        shot_ext->shot.ctl.scaler.cropRegion[2] = sensorMaxW;
        shot_ext->shot.ctl.scaler.cropRegion[3] = sensorMaxH;
    }

    /* 2. Check the validation of the crop offset.
     * Offset coordinate + width or height must be smaller than sensor max size.
     */
    if (sensorMaxW < (int) shot_ext->shot.ctl.scaler.cropRegion[0]
            + (int) shot_ext->shot.ctl.scaler.cropRegion[2]) {
        CLOGE("Invalid Crop Region, offsetX(%d), width(%d) sensorMaxW(%d)",
                shot_ext->shot.ctl.scaler.cropRegion[0],
                shot_ext->shot.ctl.scaler.cropRegion[2],
                sensorMaxW);
        shot_ext->shot.ctl.scaler.cropRegion[0] = sensorMaxW - shot_ext->shot.ctl.scaler.cropRegion[2];
    }

    if (sensorMaxH < (int) shot_ext->shot.ctl.scaler.cropRegion[1]
            + (int) shot_ext->shot.ctl.scaler.cropRegion[3]) {
        CLOGE("Invalid Crop Region, offsetY(%d), height(%d) sensorMaxH(%d)",
                shot_ext->shot.ctl.scaler.cropRegion[1],
                shot_ext->shot.ctl.scaler.cropRegion[3],
                sensorMaxH);
        shot_ext->shot.ctl.scaler.cropRegion[1] = sensorMaxH - shot_ext->shot.ctl.scaler.cropRegion[3];
    }

    m_parameters->setCropRegion(
            shot_ext->shot.ctl.scaler.cropRegion[0],
            shot_ext->shot.ctl.scaler.cropRegion[1],
            shot_ext->shot.ctl.scaler.cropRegion[2],
            shot_ext->shot.ctl.scaler.cropRegion[3]);
}

#ifdef USE_VRA_GROUP
void ExynosCamera3::m_updateFD(struct camera2_shot_ext *shot_ext, enum facedetect_mode fdMode, int dsInputPortId, bool isReprocessing)
{
    if (fdMode <= FACEDETECT_MODE_OFF) {
        shot_ext->fd_bypass = 1;
        shot_ext->shot.ctl.stats.faceDetectMode = FACEDETECT_MODE_OFF;
        shot_ext->shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS] = MCSC_PORT_NONE;
    } else {
        if (isReprocessing == true) {
            if (dsInputPortId < MCSC_PORT_MAX) {
                shot_ext->fd_bypass = 1;
                shot_ext->shot.ctl.stats.faceDetectMode = FACEDETECT_MODE_OFF;
                shot_ext->shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS] = MCSC_PORT_NONE;
            } else {
                shot_ext->fd_bypass = 0;
                shot_ext->shot.ctl.stats.faceDetectMode = fdMode;
                shot_ext->shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS] = (enum mcsc_port) (dsInputPortId % MCSC_PORT_MAX);
            }
        } else {
            //To Do
            if (m_parameters->getDualMode() == true && getCameraId() == CAMERA_ID_BACK) {
                shot_ext->fd_bypass = 1;
                shot_ext->shot.ctl.stats.faceDetectMode = FACEDETECT_MODE_OFF;
                shot_ext->shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS] = MCSC_PORT_NONE;
            } else {
                if (dsInputPortId < MCSC_PORT_MAX) {
                    shot_ext->fd_bypass = 0;
                    shot_ext->shot.ctl.stats.faceDetectMode = fdMode;
                    shot_ext->shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS] = (enum mcsc_port) dsInputPortId ;
                } else {
                    shot_ext->fd_bypass = 1;
                    shot_ext->shot.ctl.stats.faceDetectMode = FACEDETECT_MODE_OFF;
                    shot_ext->shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS] = MCSC_PORT_NONE;
                }
            }
        }
    }
}
#endif

void ExynosCamera3::m_updateEdgeNoiseMode(struct camera2_shot_ext *shot_ext, bool isCaptureFrame)
{
    enum processing_mode mode = PROCESSING_MODE_FAST;

    if (shot_ext == NULL) {
        CLOGE("shot_ext is NULL");
        return;
    }

    if (isCaptureFrame == true) {
        mode = PROCESSING_MODE_OFF;
    } else {
        mode = PROCESSING_MODE_FAST;
    }

    if (shot_ext->shot.ctl.noise.mode
        == (enum processing_mode) FIMC_IS_METADATA(ANDROID_NOISE_REDUCTION_MODE_ZERO_SHUTTER_LAG)) {
        shot_ext->shot.ctl.noise.mode = mode;
    }
    if (shot_ext->shot.ctl.edge.mode
        == (enum processing_mode) FIMC_IS_METADATA(ANDROID_EDGE_MODE_ZERO_SHUTTER_LAG)) {
        shot_ext->shot.ctl.edge.mode = mode;
    }
}

status_t ExynosCamera3::m_updateJpegControlInfo(const struct camera2_shot_ext *shot_ext)
{
    status_t ret = NO_ERROR;

    if (shot_ext == NULL) {
        CLOGE("shot_ext is NULL");
        return BAD_VALUE;
    }

    ret = m_parameters->checkJpegQuality(shot_ext->shot.ctl.jpeg.quality);
    if (ret != NO_ERROR)
        CLOGE("Failed to checkJpegQuality. quality %d",
                 shot_ext->shot.ctl.jpeg.quality);

    ret = m_parameters->checkThumbnailSize(
            shot_ext->shot.ctl.jpeg.thumbnailSize[0],
            shot_ext->shot.ctl.jpeg.thumbnailSize[1]);
    if (ret != NO_ERROR)
        CLOGE("Failed to checkThumbnailSize. size %dx%d",
                shot_ext->shot.ctl.jpeg.thumbnailSize[0],
                shot_ext->shot.ctl.jpeg.thumbnailSize[1]);

    ret = m_parameters->checkThumbnailQuality(shot_ext->shot.ctl.jpeg.thumbnailQuality);
    if (ret != NO_ERROR)
        CLOGE("Failed to checkThumbnailQuality. quality %d",
                shot_ext->shot.ctl.jpeg.thumbnailQuality);

    return ret;
}

status_t ExynosCamera3::m_generateFrame(int32_t frameCount, ExynosCamera3FrameFactory *factory, List<ExynosCameraFrameSP_sptr_t> *list, Mutex *listLock, ExynosCameraFrameSP_dptr_t newFrame)
{
    status_t ret = OK;

    newFrame = NULL;

    CLOGV("(%d)", frameCount);
    if (frameCount >= 0) {
        ret = m_searchFrameFromList(list, listLock, frameCount, newFrame);
        if (ret < 0) {
            CLOGE("searchFrameFromList fail");
            return INVALID_OPERATION;
        }
    }

    if (newFrame == NULL) {
        newFrame = factory->createNewFrame(frameCount);
        if (newFrame == NULL) {
            CLOGE("newFrame is NULL");
            return UNKNOWN_ERROR;
        }
        listLock->lock();
        list->push_back(newFrame);
        listLock->unlock();
    }

    return ret;
}

status_t ExynosCamera3::m_setupEntity(uint32_t pipeId,
                                      ExynosCameraFrameSP_sptr_t newFrame,
                                      ExynosCameraBuffer *srcBuf,
                                      ExynosCameraBuffer *dstBuf,
                                      int dstNodeIndex,
                                      int dstPipeId)
{
    status_t ret = OK;
    entity_buffer_state_t entityBufferState;

    CLOGV("(%d)", pipeId);
    /* set SRC buffer */
    ret = newFrame->getSrcBufferState(pipeId, &entityBufferState);
    if (ret < 0) {
        CLOGE("getSrcBufferState fail, pipeId(%d), ret(%d)", pipeId, ret);
        return ret;
    }

    if (entityBufferState == ENTITY_BUFFER_STATE_REQUESTED) {
        ret = m_setSrcBuffer(pipeId, newFrame, srcBuf);
        if (ret < 0) {
            CLOGE("m_setSrcBuffer fail, pipeId(%d), ret(%d)", pipeId, ret);
            return ret;
        }
    }

    /* set DST buffer */
    ret = newFrame->getDstBufferState(pipeId, &entityBufferState, dstNodeIndex);
    if (ret < 0) {
        CLOGE("getDstBufferState fail, pipeId(%d), ret(%d)", pipeId, ret);
        return ret;
    }

    if (entityBufferState == ENTITY_BUFFER_STATE_REQUESTED) {
        ret = m_setDstBuffer(pipeId, newFrame, dstBuf, dstNodeIndex, dstPipeId);
        if (ret < 0) {
            CLOGE("m_setDstBuffer fail, pipeId(%d), ret(%d)", pipeId, ret);
            return ret;
        }
    }

    ret = newFrame->setEntityState(pipeId, ENTITY_STATE_PROCESSING);
    if (ret < 0) {
        CLOGE("setEntityState(ENTITY_STATE_PROCESSING) fail, pipeId(%d), ret(%d)",
             pipeId, ret);
        return ret;
    }

    return ret;
}

status_t ExynosCamera3::m_setSrcBuffer(uint32_t pipeId, ExynosCameraFrameSP_sptr_t newFrame, ExynosCameraBuffer *buffer)
{
    status_t ret = OK;
    int bufIndex = -1;
    ExynosCameraBufferManager *bufferMgr = NULL;
    ExynosCameraBuffer srcBuf;

    CLOGV("(%d)", pipeId);
    if (buffer == NULL) {
        buffer = &srcBuf;

        ret = m_getBufferManager(pipeId, &bufferMgr, SRC_BUFFER_DIRECTION);
        if (ret < 0) {
            CLOGE("getBufferManager(SRC) fail, pipeId(%d), ret(%d)",
                 pipeId, ret);
            return ret;
        }

        if (bufferMgr == NULL) {
            CLOGE("buffer manager is NULL, pipeId(%d)", pipeId);
            return BAD_VALUE;
        }

        /* get buffers */
        ret = bufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, buffer);
        if (ret < 0) {
            CLOGE("getBuffer fail, pipeId(%d), frameCount(%d), ret(%d)",
                 pipeId, newFrame->getFrameCount(), ret);
            return ret;
        }
    }

    /* set buffers */
    ret = newFrame->setSrcBuffer(pipeId, *buffer);
    if (ret < 0) {
        CLOGE("setSrcBuffer fail, pipeId(%d), ret(%d)", pipeId, ret);
        return ret;
    }

    return ret;
}

status_t ExynosCamera3::m_setDstBuffer(uint32_t pipeId,
                                       ExynosCameraFrameSP_sptr_t newFrame,
                                       ExynosCameraBuffer *buffer,
                                       int nodeIndex,
                                       int dstPipeId)
{
    status_t ret = OK;
    int bufIndex = -1;
    ExynosCameraBufferManager *bufferMgr = NULL;
    ExynosCameraBuffer dstBuf;

    int bufferManagerPipeId = pipeId;

    if (0 <= dstPipeId) {
        bufferManagerPipeId = dstPipeId;
    }

    CLOGV("pipeId(%d), bufferManagerPipeId(%d)", pipeId, bufferManagerPipeId);

    if (buffer == NULL) {
        buffer = &dstBuf;

        ret = m_getBufferManager(bufferManagerPipeId, &bufferMgr, DST_BUFFER_DIRECTION);
        if (ret < 0) {
            CLOGE("getBufferManager(DST) fail, bufferManagerPipeId(%d), ret(%d)",
                 bufferManagerPipeId, ret);
            return ret;
        }

        if (bufferMgr == NULL) {
            CLOGE("buffer manager is NULL, pipeId(%d)", pipeId);
            return BAD_VALUE;
        }

        /* get buffers */
        ret = bufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, buffer);
        if (ret < 0) {
            CLOGE("getBuffer fail, bufferManagerPipeId(%d), frameCount(%d), ret(%d)",
                 bufferManagerPipeId, newFrame->getFrameCount(), ret);
            return ret;
        }
    }

    /* set buffers */
    ret = newFrame->setDstBuffer(pipeId, *buffer, nodeIndex);
    if (ret < 0) {
        CLOGE("setDstBuffer fail, pipeId(%d) nodeIndex(%d), ret(%d)", pipeId, nodeIndex, ret);
        return ret;
    }

    return ret;
}

status_t ExynosCamera3::m_setSrcBuffer(uint32_t pipeId, ExynosCameraFrameSP_sptr_t newFrame, ExynosCameraBuffer *buffer, ExynosCameraBufferManager *bufMgr)
{
    status_t ret = OK;
    int bufIndex = -1;
    ExynosCameraBuffer srcBuf;

    CLOGD("(%d)", pipeId);
    if (bufMgr == NULL) {

        ret = m_getBufferManager(pipeId, &bufMgr, SRC_BUFFER_DIRECTION);
        if (ret < 0) {
            CLOGE("getBufferManager(SRC) fail, pipeId(%d), ret(%d)", pipeId, ret);
            return ret;
        }

        if (bufMgr == NULL) {
            CLOGE("buffer manager is NULL, pipeId(%d)", pipeId);
            return BAD_VALUE;
        }

    }

    /* get buffers */
    ret = bufMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, buffer);
    if (ret < 0) {
        CLOGE("getBuffer fail, pipeId(%d), frameCount(%d), ret(%d)",
             pipeId, newFrame->getFrameCount(), ret);
        return ret;
    }

    /* set buffers */
    ret = newFrame->setSrcBuffer(pipeId, *buffer);
    if (ret < 0) {
        CLOGE("setSrcBuffer fail, pipeId(%d), ret(%d)", pipeId, ret);
        return ret;
    }

    return ret;
}

status_t ExynosCamera3::m_setDstBuffer(uint32_t pipeId,
                                       ExynosCameraFrameSP_sptr_t newFrame,
                                       ExynosCameraBuffer *buffer,
                                       ExynosCameraBufferManager *bufMgr,
                                       int nodeIndex,
                                       int dstPipeId)
{
    status_t ret = OK;
    int bufIndex = -1;
    ExynosCameraBuffer dstBuf;

    int bufferManagerPipeId = pipeId;

    if (0 <= dstPipeId) {
        bufferManagerPipeId = dstPipeId;
    }

    CLOGD("pipeId(%d), bufferManagerPipeId(%d)", pipeId, bufferManagerPipeId);

    if (bufMgr == NULL) {
        ret = m_getBufferManager(bufferManagerPipeId, &bufMgr, DST_BUFFER_DIRECTION);
        if (ret < 0) {
            CLOGE("getBufferManager(DST) fail, bufferManagerPipeId(%d), ret(%d)", pipeId, ret);
            return ret;
        }

        if (bufMgr == NULL) {
            CLOGE("buffer manager is NULL, bufferManagerPipeId(%d)", pipeId);
            return BAD_VALUE;
        }
    }

    /* get buffers */
    ret = bufMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, buffer);
    if (ret < 0) {
        CLOGE("getBuffer fail, bufferManagerPipeId(%d), frameCount(%d), ret(%d)",
             bufferManagerPipeId, newFrame->getFrameCount(), ret);
        return ret;
    }

    /* set buffers */
    ret = newFrame->setDstBuffer(pipeId, *buffer, nodeIndex);
    if (ret < 0) {
        CLOGE("setDstBuffer fail, pipeId(%d), ret(%d)", pipeId, ret);
        return ret;
    }

    return ret;
}

/* This function reset buffer state of pipeId.
 * Some pipes are shared by multi stream. In this case, we need reset buffer for using PIPE again.
 */
status_t ExynosCamera3::m_resetBufferState(uint32_t pipeId, ExynosCameraFrameSP_sptr_t frame)
{
    status_t ret = NO_ERROR;
    entity_buffer_state_t bufState = ENTITY_BUFFER_STATE_NOREQ;

    if (frame == NULL) {
        CLOGE("frame is NULL");
        ret = BAD_VALUE;
        goto ERR;
    }

    ret = frame->getSrcBufferState(pipeId, &bufState);
    if (ret < 0) {
        CLOGE("getSrcBufferState fail, pipeId(%d), ret(%d)", pipeId, ret);
        goto ERR;
    }

    if (bufState != ENTITY_BUFFER_STATE_NOREQ && bufState != ENTITY_BUFFER_STATE_INVALID) {
        frame->setSrcBufferState(pipeId, ENTITY_BUFFER_STATE_REQUESTED);
    } else {
        CLOGW("SrcBufferState is not COMPLETE, fail to reset buffer state,"
            "pipeId(%d), state(%d)", pipeId, bufState);
        ret = INVALID_OPERATION;
        goto ERR;
    }


    ret = frame->getDstBufferState(pipeId, &bufState);
    if (ret < 0) {
        CLOGE("getDstBufferState fail, pipeId(%d), ret(%d)", pipeId, ret);
        goto ERR;
    }

    if (bufState != ENTITY_BUFFER_STATE_NOREQ && bufState != ENTITY_BUFFER_STATE_INVALID) {
        ret = frame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_REQUESTED);
        if (ret != NO_ERROR)
            CLOGE("setDstBufferState fail, pipeId(%d), ret(%d)", pipeId, ret);
    } else {
        CLOGW("DstBufferState is not COMPLETE, fail to reset buffer state,"
            "pipeId(%d), state(%d)", pipeId, bufState);
        ret = INVALID_OPERATION;
        goto ERR;
    }

ERR:
    return ret;
}

status_t ExynosCamera3::m_getBufferManager(uint32_t pipeId, ExynosCameraBufferManager **bufMgr, uint32_t direction)
{
    status_t ret = NO_ERROR;
    ExynosCameraBufferManager **bufMgrList[2] = {NULL};

    switch (pipeId) {
    case PIPE_FLITE:
        if (m_parameters->isFlite3aaOtf() == true)
            bufMgrList[0] = NULL;
        else
            bufMgrList[0] = &m_3aaBufferMgr;

        bufMgrList[1] = &m_fliteBufferMgr;
        break;
    case PIPE_VC0:
        bufMgrList[0] = NULL;
        bufMgrList[1] = &m_fliteBufferMgr;
        break;
    case PIPE_3AA:
        if (m_parameters->isFlite3aaOtf() == true)
            bufMgrList[0] = &m_3aaBufferMgr;
        else
            bufMgrList[0] = &m_fliteBufferMgr;

        bufMgrList[1] = &m_ispBufferMgr;
        break;
    case PIPE_3AC:
        bufMgrList[0] = NULL;
        bufMgrList[1] = &m_fliteBufferMgr;
        break;
    case PIPE_ISP:
        bufMgrList[0] = &m_ispBufferMgr;
        bufMgrList[1] = &m_mcscBufferMgr;
        break;
    case PIPE_JPEG:
    case PIPE_JPEG_REPROCESSING:
        bufMgrList[0] = NULL;
        bufMgrList[1] = NULL;
        break;
    case PIPE_3AA_REPROCESSING:
        bufMgrList[0] = &m_fliteBufferMgr;
        if (m_parameters->isReprocessing3aaIspOTF() == true)
            bufMgrList[1] = &m_yuvCaptureBufferMgr;
        else
            bufMgrList[1] = &m_ispReprocessingBufferMgr;
        break;
    case PIPE_ISP_REPROCESSING:
        bufMgrList[0] = &m_ispReprocessingBufferMgr;
        bufMgrList[1] = &m_yuvCaptureBufferMgr;
        break;
    case PIPE_ISPC_REPROCESSING:
        bufMgrList[0] = NULL;
        bufMgrList[1] = &m_yuvCaptureBufferMgr;
        break;
    default:
        CLOGE("Unknown pipeId(%d)", pipeId);
        bufMgrList[0] = NULL;
        bufMgrList[1] = NULL;
        ret = BAD_VALUE;
        break;
    }

    *bufMgr = *bufMgrList[direction];
    return ret;
}

status_t ExynosCamera3::m_createIonAllocator(ExynosCameraIonAllocator **allocator)
{
    status_t ret = NO_ERROR;
    int retry = 0;
    do {
        retry++;
        CLOGI("try(%d) to create IonAllocator", retry);
        *allocator = new ExynosCameraIonAllocator(m_cameraId);
        ret = (*allocator)->init(false);
        if (ret < 0)
            CLOGE("create IonAllocator fail (retryCount=%d)", retry);
        else {
            CLOGD("m_createIonAllocator success (allocator=%p)", *allocator);
            break;
        }
    } while ((ret < 0) && (retry < 3));

    if ((ret < 0) && (retry >=3)) {
        CLOGE("create IonAllocator fail (retryCount=%d)", retry);
        ret = INVALID_OPERATION;
    }

    return ret;
}

status_t ExynosCamera3::m_createBufferManager(ExynosCameraBufferManager **bufferManager, const char *name, buffer_manager_type type)
{
    status_t ret = NO_ERROR;

    if (m_ionAllocator == NULL) {
        ret = m_createIonAllocator(&m_ionAllocator);
        if (ret < 0)
            CLOGE("m_createIonAllocator fail");
        else
            CLOGD("m_createIonAllocator success");
    }

    *bufferManager = ExynosCameraBufferManager::createBufferManager(type);
    (*bufferManager)->create(name, m_cameraId, m_ionAllocator);

    CLOGD("BufferManager(%s) created", name);

    return ret;
}

status_t ExynosCamera3::m_createInternalBufferManager(ExynosCameraBufferManager **bufferManager, const char *name)
{
    return m_createBufferManager(bufferManager, name, BUFFER_MANAGER_ION_TYPE);
}

status_t ExynosCamera3::m_createServiceBufferManager(ExynosCameraBufferManager **bufferManager, const char *name)
{
    return m_createBufferManager(bufferManager, name, BUFFER_MANAGER_SERVICE_GRALLOC_TYPE);
}

status_t ExynosCamera3::m_convertingStreamToShotExt(ExynosCameraBuffer *buffer, struct camera2_node_output *outputInfo)
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

#ifdef DEBUG_CLASS_INFO
void ExynosCamera3::m_dumpClassInfo()
{
    CLOGD("-IN-");

    char propertyValue[PROPERTY_VALUE_MAX] ={0};
    property_get(CAMERA_DEBUG_PROPERTY_KEY_FUNCTION_CLASSINFO, propertyValue, "0");

    /* This code allows developer who wants to check class information. */
    /* They just set system property, camera.debug.function.classinfo, as "1". */
    /* enable : adb shell setprop camera.debug.function.classinfo 1 */
    /* disable : adb shell setprop camera.debug.function.classinfo 0 */
#ifdef DEBUG_PROPERTY_FUNCTION_CLASSINFO
    if ((strncmp(propertyValue, "1", sizeof(propertyValue)) == 0))
#endif
    {
        typedef struct classInfo {
            char *name;
            size_t size;
        } classInfo_t;
        classInfo_t info[] = {
            /* 1. static component : create one time  */
            /* camera */
            {"ExynosCamera3", sizeof(ExynosCamera3)},
            /* parameter */
            {"ExynosCamera3Parameters", sizeof(ExynosCamera3Parameters)},
            /* activity */
            {"ExynosCameraActivityControl", sizeof(ExynosCameraActivityControl)},
            {"ExynosCameraActivityAutofocus", sizeof(ExynosCameraActivityAutofocus)},
            {"ExynosCameraActivityFlash", sizeof(ExynosCameraActivityFlash)},
            {"ExynosCameraActivitySpecialCapture", sizeof(ExynosCameraActivitySpecialCapture)},
            {"ExynosCameraActivityUCTL", sizeof(ExynosCameraActivityUCTL)},
            /* framefactory */
            {"ExynosCamera3FrameFactoryPreview", sizeof(ExynosCamera3FrameFactoryPreview)},
            {"ExynosCamera3FrameReprocessingFactory", sizeof(ExynosCamera3FrameReprocessingFactory)},
            /* requestMgr */
            {"ExynosCameraRequestManager", sizeof(ExynosCameraRequestManager)},
            /* stream */
            {"ExynosCameraStreamManager", sizeof(ExynosCameraStreamManager)},
            {"ExynosCamera3Stream", sizeof(ExynosCamera3Stream)},
            {"ExynosCameraStream", sizeof(ExynosCameraStream)},
            /* pipe */
            {"ExynosCameraPipe", sizeof(ExynosCameraPipe)},
            {"ExynosCameraPipeFlite", sizeof(ExynosCameraPipeFlite)},
            {"ExynosCameraMCPipe", sizeof(ExynosCameraMCPipe)},
            {"ExynosCameraPipeJpeg", sizeof(ExynosCameraPipeJpeg)},
            {"ExynosCameraPipeGSC", sizeof(ExynosCameraPipeGSC)},
            /* bufferMgr */
            {"ExynosCameraBufferManager", sizeof(ExynosCameraBufferManager)},
            {"InternalExynosCameraBufferManager", sizeof(InternalExynosCameraBufferManager)},
            {"ServiceExynosCameraBufferManager", sizeof(ServiceExynosCameraBufferManager)},
            /* frameMgr */
            {"ExynosCameraFrameManager", sizeof(ExynosCameraFrameManager)},
            /* frameselector */
            {"ExynosCameraFrameSelector", sizeof(ExynosCameraFrameSelector)},
            /* 2. dynamic component : create / delete each scenario */
            /* frame */
            {"ExynosCameraFrame", sizeof(ExynosCameraFrame)},
            /* request */
            {"ExynosCameraRequest", sizeof(ExynosCameraRequest)},
            {"ExynosCamera3Request", sizeof(ExynosCamera3Request)}
        };

        for(int i = 0; i < (sizeof(info) / sizeof(classInfo_t)) ; i++) {
            CLOGD("class info name:%s size:%zu", info[i].name , info[i].size);
        }
    }

    CLOGD("-OUT-");
}
#endif

bool ExynosCamera3::m_selectBayerThreadFunc()
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t frame = NULL;
    ExynosCameraBuffer bayerBuffer;
    ExynosCameraBufferManager *bufferMgr = NULL;
    camera2_shot_ext *shot_ext = NULL;
    camera2_shot_ext updateDmShot;
    uint32_t pipeID = 0;

    ret = m_selectBayerQ->waitAndPopProcessQ(&frame);
    if (ret != NO_ERROR) {
        if (ret == TIMED_OUT) {
            CLOGW("Wait timeout");
        } else {
            CLOGE("Failed to waitAndPopProcessQ. ret %d",
                     ret);
        }
        goto CLEAN;
    } else if (frame == NULL) {
        CLOGE("frame is NULL!!");
        goto CLEAN;
    }

    if (frame->getFrameCapture() == false) {
        CLOGW("frame is not capture frame. frameCount %d",
                 frame->getFrameCount());
        goto CLEAN;
    }

    CLOGV("Start to select Bayer. frameCount %d",
             frame->getFrameCount());

    /* Get bayer buffer based on current reprocessing mode */
    switch(m_parameters->getReprocessingBayerMode()) {
        case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON:
        case REPROCESSING_BAYER_MODE_PURE_DYNAMIC:
            CLOGD("REPROCESSING_BAYER_MODE_PURE. isRawCapture %d",
                     frame->getFrameServiceBayer());

            if (frame->getFrameZsl() || frame->getFrameServiceBayer())
                ret = m_getBayerServiceBuffer(frame, &bayerBuffer);
            else
                ret = m_getBayerBuffer(m_getBayerPipeId(), frame->getFrameCount(), &bayerBuffer, m_captureSelector);
            if (ret != NO_ERROR) {
                CLOGE("Failed to get bayer buffer. frameCount %d useServiceBayerBuffer %d",
                         frame->getFrameCount(), frame->getFrameZsl());
                goto CLEAN;
            }

            shot_ext = (struct camera2_shot_ext *)(bayerBuffer.addr[bayerBuffer.planeCount-1]);
            if (shot_ext == NULL) {
                CLOGE("shot_ext from pure bayer buffer is NULL");
                break;
            }

            if (frame->getFrameZsl() == true) {
                ret = frame->getDynamicMeta(&shot_ext->shot.dm);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d(%d) B%d]Failed to getDynamicMetadata. ret %d",
                            frame->getFrameCount(),
                            shot_ext->shot.dm.request.frameCount,
                            bayerBuffer.index, ret);
                    goto CLEAN;
                }

                ret = frame->getUserDynamicMeta(&shot_ext->shot.udm);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d(%d) B%d]Failed to getUserDynamicMetadata. ret %d",
                            frame->getFrameCount(),
                            shot_ext->shot.dm.request.frameCount,
                            bayerBuffer.index, ret);
                    goto CLEAN;
                }
            } else {
                ret = frame->storeDynamicMeta(shot_ext);
                if (ret < 0) {
                    CLOGE("storeDynamicMeta fail ret(%d)", ret);
                    goto CLEAN;
                }

                ret = frame->storeUserDynamicMeta(shot_ext);
                if (ret < 0) {
                    CLOGE("storeUserDynamicMeta fail ret(%d)", ret);
                    goto CLEAN;
                }
            }

            break;
        case REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON:
        case REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC:
            CLOGD("REPROCESSING_BAYER_MODE_DIRTY%s. isRawCapture %d",
                    (m_parameters->getReprocessingBayerMode()
                        == REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC) ? "_DYNAMIC" : "_ALWAYS_ON",
                    frame->getFrameServiceBayer());

            if (frame->getFrameZsl()/* || frame->getFrameServiceBayer()*/) {
                ret = m_getBayerServiceBuffer(frame, &bayerBuffer);
                CLOGD("Processing bayer from serviceBuffer Cnt = %d, planeCount = %d, size[0] = %d",
                         frame->getFrameCount(), bayerBuffer.planeCount, bayerBuffer.size[0]);

                shot_ext = (struct camera2_shot_ext *)(bayerBuffer.addr[bayerBuffer.planeCount-1]);
                if (shot_ext == NULL) {
                    CLOGE("shot_ext from pure bayer buffer is NULL");
                    break;
                }

                ret = frame->getDynamicMeta(&shot_ext->shot.dm);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d(%d) B%d]Failed to getDynamicMetadata. ret %d",
                            frame->getFrameCount(),
                            shot_ext->shot.dm.request.frameCount,
                            bayerBuffer.index, ret);

                    goto CLEAN;
                }

                ret = frame->getUserDynamicMeta(&shot_ext->shot.udm);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d(%d) B%d]Failed to getUserDynamicMetadata. ret %d",
                            frame->getFrameCount(),
                            shot_ext->shot.dm.request.frameCount,
                            bayerBuffer.index, ret);
                    goto CLEAN;
                }

            } else {
                ret = m_getBayerBuffer(m_getBayerPipeId(), frame->getFrameCount(), &bayerBuffer, m_captureSelector, &updateDmShot);
                CLOGD("Processing bayer from internalBuffer Cnt = %d, planeCount = %d, size[0] = %d",
                         frame->getFrameCount(), bayerBuffer.planeCount, bayerBuffer.size[0]);

                ret = frame->storeDynamicMeta(&updateDmShot);
                if (ret < 0) {
                    CLOGE("storeDynamicMeta fail ret(%d)", ret);
                    goto CLEAN;
                }

                ret = frame->storeUserDynamicMeta(&updateDmShot);
                if (ret < 0) {
                    CLOGE("storeUserDynamicMeta fail ret(%d)", ret);
                    goto CLEAN;
                }

                shot_ext = &updateDmShot;
            }

            if (ret != NO_ERROR) {
                CLOGE("Failed to get bayer buffer. frameCount %d useServiceBayerBuffer %d",
                         frame->getFrameCount(), frame->getFrameZsl());
                goto CLEAN;
            }

            break;
        default:
            CLOGE("bayer mode is not valid(%d)",
                     m_parameters->getReprocessingBayerMode());
            goto CLEAN;
    }

    CLOGD("meta_shot_ext->shot.dm.request.frameCount : %d",
            getMetaDmRequestFrameCount(shot_ext));

    /* Get pipeId for the first entity in reprocessing frame */
    pipeID = frame->getFirstEntity()->getPipeId();
    CLOGD("Reprocessing stream first pipe ID %d",
             pipeID);

    /* Check available buffer */
    if (pipeID == PIPE_3AA &&
        m_parameters->isFlite3aaOtf() == true) {
        ret = m_getBufferManager(PIPE_VC0, &bufferMgr, DST_BUFFER_DIRECTION);
    } else {
        ret = m_getBufferManager(pipeID, &bufferMgr, DST_BUFFER_DIRECTION);
    }
    if (ret != NO_ERROR) {
        CLOGE("Failed to getBufferManager, ret %d",
                 ret);
        goto CLEAN;
    } else if (bufferMgr == NULL) {
        CLOGE("BufferMgr is NULL. pipeId %d",
                 pipeID);
        goto CLEAN;
    }

    ret = m_checkBufferAvailable(pipeID, bufferMgr);
    if (ret != NO_ERROR) {
        CLOGE("Waiting buffer timeout, PipeID %d, ret %d",
                 pipeID, ret);
        goto CLEAN;
    }

    ret = m_setupEntity(pipeID, frame, &bayerBuffer, NULL);
    if (ret < 0) {
        CLOGE("setupEntity fail, bayerPipeId(%d), ret(%d)",
                 pipeID, ret);
        goto CLEAN;
    }

    m_captureQ->pushProcessQ(&frame);

    /* Thread can exit only if timeout or error occured from inputQ (at CLEAN: label) */
    return true;

CLEAN:
    if (frame != NULL && m_flushFlag == false) {
        frame->frameUnlock();
        ret = m_removeFrameFromList(&m_captureProcessList, &m_captureProcessLock, frame);
        if (ret != NO_ERROR)
            CLOGE("Failed to remove frame from m_captureProcessList. frameCount %d ret %d",
                     frame->getFrameCount(), ret);

        frame->printEntity();
        CLOGD("Delete frame from m_captureProcessList. frameCount %d",
                 frame->getFrameCount());

        frame = NULL;
    }

    if (m_selectBayerQ->getSizeOfProcessQ() > 0) {
        return true;
    } else {
        return false;
    }
}

bool ExynosCamera3::m_captureThreadFunc()
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t frame = NULL;
    ExynosCameraRequestSP_sprt_t request = NULL;
    ExynosCamera3FrameFactory *factory = NULL;
    int retryCount = 1;
    uint32_t pipeID = 0;

    ret = m_captureQ->waitAndPopProcessQ(&frame);
    if (ret != NO_ERROR) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGW("Wait timeout");
        } else {
            CLOGE("Failed to wait&pop captureQ, ret %d", ret);
            /* TODO: doing exception handling */
        }
        goto CLEAN;
    } else if (frame == NULL) {
        CLOGE("frame is NULL!!");
        goto FUNC_EXIT;
    }

    CLOGV("frame frameCount(%d)", frame->getFrameCount());

    factory = m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING];
    pipeID = frame->getFirstEntity()->getPipeId();

    while(factory->isRunning() == false) {
        CLOGD("Wait to start reprocessing stream");

        if (m_flushFlag == true) {
            CLOGD("[F%d]Flush is in progress.",
                    frame->getFrameCount());
            goto FUNC_EXIT;
        }

        usleep(5000);
    }

    m_captureStreamThread->run(PRIORITY_DEFAULT);
    factory->pushFrameToPipe(frame, pipeID);

    /* When enabled SCC capture or pureBayerReprocessing, we need to start bayer pipe thread */
    if (m_parameters->isReprocessing() == true)
        factory->startThread(pipeID);

    /* Wait reprocesisng done */
    CLOGI("Wait reprocessing done. frameCount %d",
             frame->getFrameCount());
    do {
        ret = m_reprocessingDoneQ->waitAndPopProcessQ(&frame);
    } while (ret == TIMED_OUT && retryCount-- > 0);

    if (ret != NO_ERROR) {
        CLOGW("Failed to waitAndPopProcessQ to reprocessingDoneQ. ret %d",
                 ret);
    }

    /* Thread can exit only if timeout or error occured from inputQ (at CLEAN: label) */
    return true;

CLEAN:
    if (frame != NULL && m_flushFlag == false) {
        frame->frameUnlock();
        ret = m_removeFrameFromList(&m_captureProcessList, &m_captureProcessLock, frame);
        if (ret < 0) {
            CLOGE("remove frame from m_captureProcessList fail, ret(%d)", ret);
        }

        frame->printEntity();
        CLOGD("m_captureProcessList frame delete(%d)", frame->getFrameCount());

        frame = NULL;
    }

FUNC_EXIT:
    if (m_captureQ->getSizeOfProcessQ() > 0)
        return true;
    else
        return false;
}

status_t ExynosCamera3::m_getBayerServiceBuffer(ExynosCameraFrameSP_sptr_t frame, ExynosCameraBuffer *buffer)
{
    status_t ret = NO_ERROR;
    ExynosCameraRequestSP_sprt_t request = NULL;
    ExynosCameraFrameSP_sptr_t bayerFrame = NULL;
    int retryCount = 30;
    ExynosCamera3FrameFactory *factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];

    int dstPos = 0;
    dstPos = factory->getNodeType(PIPE_VC0);

    if (frame->getFrameServiceBayer() == true) {
        m_captureZslSelector->setWaitTime(200000000);
        bayerFrame = m_captureZslSelector->selectDynamicFrames(1, m_getBayerPipeId(), false, retryCount, dstPos);
        if (bayerFrame == NULL) {
            CLOGE("bayerFrame is NULL");
            return INVALID_OPERATION;
        }

        if (bayerFrame->getMetaDataEnable() == true)
            CLOGI("Frame metadata is updated. frameCount %d",
                     bayerFrame->getFrameCount());

        ret = bayerFrame->getDstBuffer(m_getBayerPipeId(), buffer, dstPos);
        if (ret != NO_ERROR) {
            CLOGE("Failed to getDstBuffer. pipeId %d ret %d",
                    m_getBayerPipeId(), ret);
            return ret;
        }
    } else {
        request = m_requestMgr->getRunningRequest(frame->getFrameCount());
        if (request != NULL) {
            camera3_stream_buffer_t *stream_buffer = request->getInputBuffer();
            buffer_handle_t *handle = stream_buffer->buffer;
            int bufIndex = -1;
            CLOGI(" Getting Bayer from getRunningRequest(fcount:%d / handle:%p)", frame->getFrameCount(), handle);

            m_zslBufferMgr->getIndexByHandle(handle, &bufIndex);
            if (bufIndex < 0) {
                CLOGE(" getIndexByHandle is fail(fcount:%d / handle:%p)",
                     frame->getFrameCount(), handle);
                ret = BAD_VALUE;
            } else {
                ret = m_zslBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_SERVICE, buffer);
                CLOGI(" service bayer selected(fcount:%d / handle:%p / idx:%d / ret:%d)",
                        frame->getFrameCount(), handle, bufIndex, ret);
            }
        } else {
            CLOGE(" request if NULL(fcount:%d)", frame->getFrameCount());
            ret = BAD_VALUE;
        }
    }

    return ret;
}

status_t ExynosCamera3::m_getBayerBuffer(uint32_t pipeId, uint32_t frameCount, ExynosCameraBuffer *buffer, ExynosCameraFrameSelector *selector, camera2_shot_ext *updateDmShot)
{
    status_t ret = NO_ERROR;
    bool isSrc = false;
    int retryCount = 30; /* 200ms x 30 */
    camera2_shot_ext *shot_ext = NULL;
    camera2_stream *shot_stream = NULL;
    ExynosCameraFrameSP_sptr_t inListFrame = NULL;
    ExynosCameraFrameSP_sptr_t bayerFrame = NULL;
    ExynosCamera3FrameFactory *factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];

    int dstPos = 0;

    if (m_parameters->getUsePureBayerReprocessing()) {
        dstPos = factory->getNodeType(PIPE_VC0);
    } else {
        dstPos = factory->getNodeType(PIPE_3AC);
    }

    if (m_parameters->isReprocessing() == false || selector == NULL) {
        CLOGE("INVALID_OPERATION, isReprocessing(%s) or bayerFrame is NULL",
                 m_parameters->isReprocessing() ? "True" : "False");
        ret = INVALID_OPERATION;
        goto CLEAN;
    }

    selector->setWaitTime(200000000);
    bayerFrame = selector->selectCaptureFrames(1, frameCount, pipeId, isSrc, retryCount, dstPos);
    if (bayerFrame == NULL) {
        CLOGE("bayerFrame is NULL");
        ret = INVALID_OPERATION;
        goto CLEAN;
    }

    ret = bayerFrame->getDstBuffer(pipeId, buffer, dstPos);
    if (ret < 0) {
        CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", pipeId, ret);
        goto CLEAN;
    }

    if (m_parameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON ||
        m_parameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_PURE_DYNAMIC) {
        shot_ext = (struct camera2_shot_ext *)buffer->addr[buffer->planeCount - 1];
        CLOGD("Selected frame count(hal : %d / driver : %d)",
            bayerFrame->getFrameCount(), shot_ext->shot.dm.request.frameCount);

        if (bayerFrame->getMetaDataEnable() == true)
            CLOGI("Frame metadata is updated. frameCount %d",
                     bayerFrame->getFrameCount());

        ret = bayerFrame->getMetaData(shot_ext);
        if (ret != NO_ERROR) {
            CLOGE("Failed to getMetaData from frame. frameCount %d",
                     bayerFrame->getFrameCount());
            goto CLEAN;
        }
    } else if (m_parameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON ||
        m_parameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC) {
        if (updateDmShot == NULL) {
            CLOGE("updateDmShot is NULL");
            goto CLEAN;
        }

        while(retryCount > 0) {
            if(bayerFrame->getMetaDataEnable() == false) {
                CLOGD("Waiting for update jpeg metadata failed (%d), retryCount(%d)",
                     ret, retryCount);
            } else {
                break;
            }
            retryCount--;
            usleep(DM_WAITING_TIME);
        }

        /* update meta like pure bayer */
        bayerFrame->getUserDynamicMeta(updateDmShot);
        bayerFrame->getDynamicMeta(updateDmShot);

        shot_stream = (struct camera2_stream *)buffer->addr[1];
        CLOGD("Selected fcount(hal : %d / driver : %d)",
            bayerFrame->getFrameCount(), shot_stream->fcount);
    } else {
        CLOGE("reprocessing is not valid pipeId(%d), ret(%d)", pipeId, ret);
        goto CLEAN;
    }

CLEAN:

    if (bayerFrame != NULL) {
        bayerFrame->frameUnlock();

        ret = m_searchFrameFromList(&m_processList, &m_processLock, bayerFrame->getFrameCount(), inListFrame);
        if (ret < 0) {
            CLOGE("searchFrameFromList fail");
        } else {
            CLOGD("Selected frame(%d) complete, Delete",
                 bayerFrame->getFrameCount());
            bayerFrame = NULL;
        }
    }

    return ret;
}


bool ExynosCamera3::m_captureStreamThreadFunc(void)
{
    status_t ret = 0;
    ExynosCameraFrameSP_sptr_t frame = NULL;
    ExynosCameraFrameEntity *entity = NULL;
    ExynosCameraBuffer buffer;
    camera3_stream_buffer_t streamBuffer;
    ExynosCameraRequestSP_sprt_t request = NULL;
    ResultRequest resultRequest = NULL;
    struct camera2_shot_ext *shot_ext = NULL;

    ret = m_yuvCaptureDoneQ->waitAndPopProcessQ(&frame);
    if (ret != NO_ERROR) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGW("wait timeout");
        } else {
            CLOGE("Failed to wait&pop m_yuvCaptureDoneQ, ret(%d)", ret);
            /* TODO: doing exception handling */
        }
        goto FUNC_EXIT;
    }

    if (frame == NULL) {
        CLOGE("frame is NULL");
        goto FUNC_EXIT;
    } else if (m_flushFlag == true) {
        CLOGD("[F%d]Flush is in progress.",
                frame->getFrameCount());
        goto FUNC_EXIT;
    }

    entity = frame->getFrameDoneEntity();
    if (entity == NULL) {
        CLOGE("Current entity is NULL");
        /* TODO: doing exception handling */
        goto FUNC_EXIT;
    }

    CLOGD("Yuv capture done. framecount %d entityID %d",
            frame->getFrameCount(), entity->getPipeId());

    switch(entity->getPipeId()) {
    case PIPE_3AA_REPROCESSING:
    case PIPE_ISP_REPROCESSING:
        ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
        if (ret != NO_ERROR) {
            CLOGE("Failed to getSrcBuffer, pipeId %d, ret %d",
                    entity->getPipeId(), ret);
            goto FUNC_EXIT;
        }

        shot_ext = (struct camera2_shot_ext *) buffer.addr[buffer.planeCount - 1];
        if (shot_ext == NULL) {
            CLOGE("shot_ext is NULL. pipeId %d frameCount %d",
                    entity->getPipeId(), frame->getFrameCount());
            goto FUNC_EXIT;
        }
#ifdef CORRECT_TIMESTAMP_FOR_SENSORFUSION
        if (frame->getAdjustedTimestampFlag() == true) {
            /* m_adjustTimestamp() is to do adjust timestamp for ITS/sensorFusion. */
            /* This function should be called once at here */
            ret = m_adjustTimestamp(&buffer);
            if (ret != NO_ERROR) {
                CLOGE("Failed to adjust timestamp");
                goto FUNC_EXIT;
            }
        }
#endif

        shot_ext = (struct camera2_shot_ext *)buffer.addr[buffer.planeCount -1];

        request = m_requestMgr->getRunningRequest(frame->getFrameCount());
        if (request == NULL) {
            CLOGE("getRequest failed ");
            return INVALID_OPERATION;
        }

        request->setSensorTimestamp(shot_ext->shot.udm.sensor.timeStampBoot);

        if (m_parameters->getUsePureBayerReprocessing() == true) {
            ret = m_pushResult(frame->getFrameCount(), shot_ext);
            if (ret != NO_ERROR) {
                CLOGE("Failed to pushResult. framecount %d ret %d",
                        frame->getFrameCount(), ret);
                goto FUNC_EXIT;
            }
        } else {
            // In dirty bayer case, the meta is updated if the current request
            // is reprocessing only(i.e. Internal frame is created on preview path).
            // Preview path will update the meta if the current request have preview frame
            ExynosCameraRequestSP_sprt_t request = m_requestMgr->getRunningRequest(frame->getFrameCount());
            if (request == NULL) {
                CLOGE("getRunningRequest failed ");
            } else {
                if(request->getNeedInternalFrame() == true) {
                    ret = m_pushResult(frame->getFrameCount(), shot_ext);
                    if (ret != NO_ERROR) {
                        CLOGE("Failed to pushResult. framecount %d ret %d",
                                 frame->getFrameCount(), ret);
                        goto FUNC_EXIT;
                    }
                }
            }
        }

        ret = frame->storeDynamicMeta(shot_ext);
        if (ret != NO_ERROR) {
            CLOGE("Failed to storeUserDynamicMeta, requestKey %d, ret %d",
                     request->getKey(), ret);
            goto FUNC_EXIT;
        }

        ret = frame->storeUserDynamicMeta(shot_ext);
        if (ret != NO_ERROR) {
            CLOGE("Failed to storeUserDynamicMeta, requestKey %d, ret %d",
                     request->getKey(), ret);
            goto FUNC_EXIT;
        }

        CLOGV("3AA_REPROCESSING Done. dm.request.frameCount %d frameCount %d",
                getMetaDmRequestFrameCount((struct camera2_shot_ext *)buffer.addr[buffer.planeCount-1]),
                frame->getFrameCount());

        /* Return bayer buffer, except the buffer came from the service. */
        if ((frame->getFrameZsl() == false) && (frame->getFrameServiceBayer() == false)) {
            ret = m_putBuffers(m_fliteBufferMgr, buffer.index);
            if (ret != NO_ERROR) {
                CLOGE("Failed to putBuffer to fliteBufferMgr, bufferIndex %d",
                         buffer.index);
            }
        } else if (frame->getFrameZsl() == true) {
            /* release zsl buffer */
            ret = m_zslBufferMgr->release(buffer.index);
            if (ret != NO_ERROR) {
                CLOGE("Failed to zsl buffer, bufferIndex %d", buffer.index);
            }
        }

#if !defined(ENABLE_FULL_FRAME)
        {
            ExynosCameraRequestSP_sprt_t request = m_requestMgr->getRunningRequest(frame->getFrameCount());
            /* try to notify if notify callback was not called in same framecount */
            if (request->getCallbackDone(EXYNOS_REQUEST_RESULT::CALLBACK_NOTIFY_ONLY) == false) {
                /* Check whether the shutter callback can be made on preview pipe */
                if (request->getNeedInternalFrame() == true) {
                    ret = m_pushPartialResult(frame->getFrameCount(), (struct camera2_shot_ext *)buffer.addr[buffer.planeCount-1]);
                    if (ret != NO_ERROR) {
                        CLOGE("Failed to pushPartialResult. framecount %d ret %d", frame->getFrameCount(), ret);
                    }
                    m_sendNotify(frame->getFrameCount(), CAMERA3_MSG_SHUTTER);
                    m_send3AAMeta(frame->getFrameCount(), EXYNOS_REQUEST_RESULT::CALLBACK_PARTIAL_3AA);
                }
            }
        }
#endif

        /* Handle NV21 capture callback buffer */
        if (frame->getRequest(PIPE_MCSC0_REPROCESSING) == true
            || frame->getRequest(PIPE_MCSC1_REPROCESSING) == true
            || frame->getRequest(PIPE_MCSC2_REPROCESSING) == true) {
            if (m_parameters->isSingleChain()) {
                if(frame->getRequest(PIPE_MCSC2_REPROCESSING) == true) {
                    /* Request for ZSL YUV reprocessing */
                    ret = m_generateDuplicateBuffers(frame, entity->getPipeId());
                    if (ret != NO_ERROR)
                        CLOGE("m_generateDuplicateBuffers failed. frameCnt = %d / ret = %d",
                                 frame->getFrameCount(), ret);
                }
            } else {
                ret = m_handleNV21CaptureFrame(frame);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):Failed to m_handleNV21CaptureFrame. pipeId %d ret %d",
                            __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                    return ret;
                }
            }
        }

        /* Handle yuv capture buffer */
        if (frame->getRequest(PIPE_MCSC3_REPROCESSING) == true) {
            ret = m_handleYuvCaptureFrame(frame);
            if (ret != NO_ERROR) {
                CLOGE("Failed to handleYuvCaptureFrame. pipeId %d ret %d",
                         entity->getPipeId(), ret);
                return ret;
            }
        }

        /* Continue to JPEG processing stage in HWFC mode */
        if (m_parameters->isUseHWFC() == false) {
            break;
        }

    case PIPE_JPEG_REPROCESSING:
        /* Handle JPEG buffer */
        if (frame->getRequest(PIPE_MCSC3_REPROCESSING) == true) {
            ret = m_handleJpegFrame(frame);
            if (ret != NO_ERROR) {
                CLOGE("Failed to handleJpegFrame. pipeId %d ret %d",
                         entity->getPipeId(), ret);
                return ret;
            }
        }
        break;
    default:
        CLOGE("Invalid pipeId %d", entity->getPipeId());
        break;
    }

    ret = frame->setEntityState(entity->getPipeId(), ENTITY_STATE_COMPLETE);
    if (ret < 0) {
        CLOGE("setEntityState fail, pipeId(%d), state(%d), ret(%d)",
             entity->getPipeId(), ENTITY_STATE_COMPLETE, ret);
        return ret;
    }

    if (frame->isComplete() == true && m_flushFlag == false) {
        List<ExynosCameraFrameSP_sptr_t> *list = NULL;
        Mutex *listLock = NULL;
#if defined(ENABLE_FULL_FRAME)
        list = &m_processList;
        listLock = &m_processLock;
#else
        list = &m_captureProcessList;
        listLock = &m_captureProcessLock;
#endif
        // TODO:decide proper position
        CLOGV("frame complete. framecount %d", frame->getFrameCount());
        ret = m_removeFrameFromList(list, listLock, frame);
        if (ret < 0) {
            CLOGE("remove frame from processList fail, ret(%d)", ret);
        }

        frame = NULL;
    }
FUNC_EXIT:
        Mutex::Autolock l(m_captureProcessLock);
        if (m_captureProcessList.size() > 0)
            return true;
        else
            return false;
}

status_t ExynosCamera3::m_handleNV21CaptureFrame(ExynosCameraFrameSP_sptr_t frame)
{
    status_t ret = NO_ERROR;
    ExynosCameraRequestSP_sprt_t request = NULL;
    ExynosCameraBufferManager *bufferMgr = NULL;
    ExynosCameraBuffer dstBuffer;
    int bufIndex = -1;
    ExynosCamera3FrameFactory *factory = m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING];
    int pipeId = -1;
    int streamId = -1;

    if (frame->getFrameServiceYuvCbStall() == true) {
        for (int i = 0; i < m_streamManager->getTotalYuvStreamCount(); i++) {
            pipeId = i + PIPE_MCSC0_REPROCESSING;
            streamId = m_streamManager->getYuvStreamId(ExynosCamera3Parameters::YUV_MAX + i);

            if (frame->getRequest(pipeId) == false) {
                continue;
            }

            ret = frame->getDstBuffer(PIPE_3AA_REPROCESSING, &dstBuffer, factory->getNodeType(pipeId));
            if (ret != NO_ERROR) {
                CLOGE("Failed to getDstBuffer. pipeId %d node %d ret %d",
                        PIPE_3AA_REPROCESSING, pipeId, ret);
                return INVALID_OPERATION;
            }

            CLOGD("NV21 m_resultCallback(%d) pipeId(%d)", frame->getFrameCount(), pipeId);

            ret = m_resultCallback(&dstBuffer, frame->getFrameCount(), streamId);
            if (ret != NO_ERROR) {
                CLOGE("Failed to resultCallback."
                        " pipeId %d bufferIndex %d frameCount %d streamId %d ret %d",
                        pipeId, dstBuffer.index, frame->getFrameCount(), streamId, ret);
                return ret;
            }
        }
    }

    m_sendMeta(frame->getFrameCount(), EXYNOS_REQUEST_RESULT::CALLBACK_ALL_RESULT);
    return ret;
}

status_t ExynosCamera3::m_handleYuvCaptureFrame(ExynosCameraFrameSP_sptr_t frame)
{
    status_t ret = NO_ERROR;
    ExynosCameraRequestSP_sprt_t request = NULL;
    ExynosCameraBufferManager *bufferMgr = NULL;
    ExynosCameraBuffer srcBuffer;
    ExynosCameraBuffer dstBuffer;
    int bufIndex = -1;
    ExynosCamera3FrameFactory *factory = NULL;
    int pipeId_src = -1;
    int pipeId_gsc = -1;
    int pipeId_jpeg = -1;

    float zoomRatio = 0.0F;
    struct camera2_stream *shot_stream = NULL;
    int pictureW = 0, pictureH = 0, pictureFormat = 0;
    ExynosRect srcRect, dstRect;

    request = m_requestMgr->getRunningRequest(frame->getFrameCount());
    factory = request->getFrameFactory(HAL_STREAM_ID_JPEG);

    int headerPipeId = 0;
    if (m_parameters->getUsePureBayerReprocessing() == true) {
        headerPipeId = PIPE_3AA_REPROCESSING;
    } else {
        headerPipeId = PIPE_ISP_REPROCESSING;
    }

    if (m_parameters->isUseHWFC() == true) {
        ret = frame->getDstBuffer(headerPipeId, &dstBuffer, factory->getNodeType(PIPE_MCSC3_REPROCESSING));
        if (ret != NO_ERROR) {
            CLOGE("Failed to getDstBuffer. pipeId %d node %d ret %d",
                    headerPipeId, PIPE_MCSC3_REPROCESSING, ret);
            return INVALID_OPERATION;
        }

        ret = m_putBuffers(m_yuvCaptureBufferMgr, dstBuffer.index);
        if (ret != NO_ERROR) {
            CLOGE("Failed to putBuffer to m_yuvCaptureBufferMgr. bufferIndex %d",
                     dstBuffer.index);
            return INVALID_OPERATION;
        }
#if 0 /* TODO : Why this makes error? */
        if (frame->getRequest(PIPE_MCSC4_REPROCESSING) == true) {
            ret = frame->getDstBuffer(PIPE_3AA_REPROCESSING, &dstBuffer, factory->getNodeType(PIPE_MCSC4_REPROCESSING));
            if (ret != NO_ERROR) {
                CLOGE("Failed to getDstBuffer. pipeId %d node %d ret %d",
                        PIPE_3AA_REPROCESSING, PIPE_MCSC4_REPROCESSING, ret);
                return INVALID_OPERATION;
            }

            ret = m_putBuffers(m_thumbnailBufferMgr, dstBuffer.index);
            if (ret != NO_ERROR) {
                CLOGE("Failed to putBuffer to m_thumbnailBufferMgr. bufferIndex %d",
                        dstBuffer.index);
                return INVALID_OPERATION;
            }
        }
#endif
    } else {
        zoomRatio = m_parameters->getZoomRatio(m_parameters->getZoomLevel()) / 1000;

        if (m_parameters->isReprocessing() == true) {
            /* We are using only PIPE_ISP_REPROCESSING */
            pipeId_src = PIPE_ISP_REPROCESSING;
            pipeId_gsc = PIPE_GSC_REPROCESSING;
            pipeId_jpeg = PIPE_JPEG_REPROCESSING;
        } else {
#if defined(ENABLE_FULL_FRAME)
            pipeId_src = PIPE_ISP;
            pipeId_gsc = PIPE_GSC_PICTURE;
            pipeId_jpeg = PIPE_JPEG;
#else
            switch (getCameraId()) {
                case CAMERA_ID_FRONT:
                    pipeId_src = PIPE_ISP;
                    pipeId_gsc = PIPE_GSC_PICTURE;
                    break;
                default:
                    CLOGE("Current picture mode is not yet supported, CameraId(%d), reprocessing(%d)",
                             getCameraId(), m_parameters->isReprocessing());
                    break;
            }
            pipeId_jpeg = PIPE_JPEG;
#endif
        }
        ///////////////////////////////////////////////////////////

        // Thumbnail image is currently not used
        ret = frame->getDstBuffer(pipeId_src, &dstBuffer, factory->getNodeType(PIPE_MCSC4_REPROCESSING));
        if (ret != NO_ERROR) {
            CLOGE("Failed to getDstBuffer. pipeId %d node %d ret %d",
                    pipeId_src, PIPE_MCSC4_REPROCESSING, ret);
        } else {
            ret = m_putBuffers(m_thumbnailBufferMgr, dstBuffer.index);
            if (ret != NO_ERROR) {
                CLOGE("Failed to putBuffer to m_thumbnailBufferMgr. bufferIndex %d",
                         dstBuffer.index);
            }
            CLOGI(" Thumbnail image disposed at pipeId %d node %d, FrameCnt %d",
                    pipeId_src, PIPE_MCSC4_REPROCESSING, frame->getFrameCount());
        }


        if (m_parameters->needGSCForCapture(getCameraId()) == true) {
            ret = frame->getDstBuffer(pipeId_src, &srcBuffer);
            if (ret < 0)
                CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", pipeId_src, ret);

            shot_stream = (struct camera2_stream *)(srcBuffer.addr[srcBuffer.planeCount-1]);
            if (shot_stream != NULL) {
                CLOGD("(%d %d %d %d)",
                        shot_stream->fcount,
                        shot_stream->rcount,
                        shot_stream->findex,
                        shot_stream->fvalid);
                CLOGD("(%d %d %d %d)(%d %d %d %d)",
                        shot_stream->input_crop_region[0],
                        shot_stream->input_crop_region[1],
                        shot_stream->input_crop_region[2],
                        shot_stream->input_crop_region[3],
                        shot_stream->output_crop_region[0],
                        shot_stream->output_crop_region[1],
                        shot_stream->output_crop_region[2],
                        shot_stream->output_crop_region[3]);
            } else {
                CLOGE("shot_stream is NULL");
                return INVALID_OPERATION;
            }

            /* should change size calculation code in pure bayer */
#if 0
            if (shot_stream != NULL) {
                ret = m_calcPictureRect(&srcRect, &dstRect);
                ret = newFrame->setSrcRect(pipeId_gsc, &srcRect);
                ret = newFrame->setDstRect(pipeId_gsc, &dstRect);
            }
#else
            m_parameters->getPictureSize(&pictureW, &pictureH);
#if defined(ENABLE_FULL_FRAME)
            pictureFormat = m_parameters->getHwPreviewFormat();
#else
            pictureFormat = m_parameters->getHwPictureFormat();
#endif

            srcRect.x = shot_stream->output_crop_region[0];
            srcRect.y = shot_stream->output_crop_region[1];
            srcRect.w = shot_stream->output_crop_region[2];
            srcRect.h = shot_stream->output_crop_region[3];
            srcRect.fullW = shot_stream->output_crop_region[2];
            srcRect.fullH = shot_stream->output_crop_region[3];
            srcRect.colorFormat = pictureFormat;
#if 0
            dstRect.x = 0;
            dstRect.y = 0;
            dstRect.w = srcRect.w;
            dstRect.h = srcRect.h;
            dstRect.fullW = srcRect.fullW;
            dstRect.fullH = srcRect.fullH;
            dstRect.colorFormat = JPEG_INPUT_COLOR_FMT;
#else
            dstRect.x = 0;
            dstRect.y = 0;
            dstRect.w = pictureW;
            dstRect.h = pictureH;
            dstRect.fullW = pictureW;
            dstRect.fullH = pictureH;
            dstRect.colorFormat = JPEG_INPUT_COLOR_FMT;
#endif
            ret = getCropRectAlign(srcRect.w,  srcRect.h,
                    pictureW,   pictureH,
                    &srcRect.x, &srcRect.y,
                    &srcRect.w, &srcRect.h,
                    2, 2, 0, zoomRatio);

            ret = frame->setSrcRect(pipeId_gsc, &srcRect);
            ret = frame->setDstRect(pipeId_gsc, &dstRect);
#endif

            CLOGD("size (%d, %d, %d, %d %d %d)",
                    srcRect.x, srcRect.y, srcRect.w, srcRect.h, srcRect.fullW, srcRect.fullH);
            CLOGD("size (%d, %d, %d, %d %d %d)",
                    dstRect.x, dstRect.y, dstRect.w, dstRect.h, dstRect.fullW, dstRect.fullH);

            ret = m_setupEntity(pipeId_gsc, frame, &srcBuffer, NULL);

            if (ret < 0) {
                CLOGE("setupEntity fail, pipeId(%d), ret(%d)",
                         pipeId_jpeg, ret);
            }

            factory->setOutputFrameQToPipe(m_yuvCaptureDoneQ, pipeId_gsc);
            factory->pushFrameToPipe(frame, pipeId_gsc);

        } else { /* m_parameters->needGSCForCapture(getCameraId()) == false */
            ret = frame->getDstBuffer(pipeId_src, &srcBuffer);
#if defined(ENABLE_FULL_FRAME)
            if (ret < 0)
                CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", pipeId_src, ret);

            ret = m_setupEntity(pipeId_jpeg, frame, &srcBuffer, NULL);
            if (ret < 0) {
                CLOGE("setupEntity fail, pipeId(%d), ret(%d)",
                         pipeId_jpeg, ret);
            }
#else
            if (ret < 0) {
                CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", pipeId_src, ret);
            } else {
                /* getting jpeg buffer from service buffer */
                ExynosCameraStream *stream = NULL;

                int streamId = 0;
                m_streamManager->getStream(HAL_STREAM_ID_JPEG, &stream);

                if (stream == NULL) {
                    CLOGE("stream is NULL");
                    return INVALID_OPERATION;
                }

                stream->getID(&streamId);
                stream->getBufferManager(&bufferMgr);
                CLOGV("streamId(%d), bufferMgr(%p)", streamId, bufferMgr);
                /* bufferMgr->printBufferQState(); */
                ret = bufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &dstBuffer);
                if (ret < 0) {
                    CLOGE("bufferMgr getBuffer fail, frameCount(%d), ret(%d)",
                             frame->getFrameCount(), ret);
                }
            }
            ret = m_setupEntity(pipeId_jpeg, frame, &srcBuffer, &dstBuffer);
            if (ret < 0)
                CLOGE("setupEntity fail, pipeId(%d), ret(%d)",
                         pipeId_jpeg, ret);
#endif
            factory->setOutputFrameQToPipe(m_yuvCaptureDoneQ, pipeId_jpeg);
            factory->pushFrameToPipe(frame, pipeId_jpeg);

        }
    }

    return ret;
}

status_t ExynosCamera3::m_handleJpegFrame(ExynosCameraFrameSP_sptr_t frame)
{
    status_t ret = NO_ERROR;
    int pipeId_jpeg = -1;
    int pipeId_src = -1;
    ExynosCameraRequestSP_sprt_t request = NULL;
    ExynosCamera3FrameFactory * factory = NULL;
    ExynosCameraBufferManager *bufferMgr = NULL;
    ExynosCameraBuffer buffer;
    int jpegOutputSize = -1;

    request = m_requestMgr->getRunningRequest(frame->getFrameCount());
    factory = request->getFrameFactory(HAL_STREAM_ID_JPEG);

    int headerPipeId = PIPE_3AA_REPROCESSING;

    /* We are using only PIPE_ISP_REPROCESSING */
    if (m_parameters->isReprocessing() == true) {
        if (m_parameters->needGSCForCapture(getCameraId()) == true) {
            pipeId_src = PIPE_GSC_REPROCESSING;
        } else {
            pipeId_src = (m_parameters->isOwnScc(getCameraId()) == true) ? PIPE_SCC_REPROCESSING : PIPE_ISP_REPROCESSING;
        }

        if (m_parameters->getUsePureBayerReprocessing() == true) {
            headerPipeId = PIPE_3AA_REPROCESSING;
        } else {
            headerPipeId = PIPE_ISP_REPROCESSING;
        }

        pipeId_jpeg = PIPE_JPEG_REPROCESSING;
    } else {
        if (m_parameters->needGSCForCapture(getCameraId()) == true) {
            pipeId_src = PIPE_GSC_PICTURE;
        } else {
            if (m_parameters->isOwnScc(getCameraId()) == true) {
                pipeId_src = PIPE_SCC;
            } else {
                pipeId_src = PIPE_ISP;
            }
        }
        pipeId_jpeg = PIPE_JPEG;
    }

    if (m_parameters->isUseHWFC() == true) {
        entity_buffer_state_t bufferState = ENTITY_BUFFER_STATE_NOREQ;
        ret = frame->getDstBufferState(headerPipeId, &bufferState, factory->getNodeType(PIPE_HWFC_JPEG_DST_REPROCESSING));
        if (ret != NO_ERROR) {
            CLOGE("Failed to getDstBufferState. frameCount %d pipeId %d node %d",
                    frame->getFrameCount(),
                    headerPipeId, PIPE_HWFC_JPEG_DST_REPROCESSING);
            return INVALID_OPERATION;
        } else if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
            CLOGE("Invalid JPEG buffer state. frameCount %d bufferState %d",
                    frame->getFrameCount(), bufferState);
            return INVALID_OPERATION;
        }

        ret = frame->getDstBuffer(headerPipeId, &buffer, factory->getNodeType(PIPE_HWFC_JPEG_DST_REPROCESSING));
        if (ret != NO_ERROR) {
            CLOGE("Failed to getDstBuffer. frameCount %d pipeId %d node %d",
                    frame->getFrameCount(),
                    headerPipeId, PIPE_HWFC_JPEG_DST_REPROCESSING);
            return INVALID_OPERATION;
        }
    } else {
        ret = frame->setEntityState(pipeId_jpeg, ENTITY_STATE_FRAME_DONE);
        if (ret < 0) {
            CLOGE("Failed to setEntityState, frameCoutn %d ret %d",
                    frame->getFrameCount(), ret);
            return INVALID_OPERATION;
        }

        ret = m_getBufferManager(pipeId_src, &bufferMgr, DST_BUFFER_DIRECTION);
        if (ret < 0) {
            CLOGE("getBufferManager(DST) fail, pipeId(%d), ret(%d)",
                 pipeId_src, ret);
            return ret;
        }

        ret = frame->getSrcBuffer(pipeId_jpeg, &buffer);
        if (ret < 0) {
            CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)", pipeId_jpeg, ret);
        }

        ret = m_putBuffers(bufferMgr, buffer.index);
        if (ret < 0) {
            CLOGE("bufferMgr(DST)->putBuffers() fail, pipeId(%d), ret(%d)",
                 pipeId_src, ret);
        }

        ret = frame->getDstBuffer(pipeId_jpeg, &buffer);
        if (ret < 0) {
            CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", pipeId_jpeg, ret);
        }
    }

    jpegOutputSize = buffer.size[0];
    CLOGI("Jpeg output done, jpeg size(%d)", jpegOutputSize);

    /* frame->printEntity(); */
    m_pushJpegResult(frame, jpegOutputSize, &buffer);

    return ret;
}

status_t ExynosCamera3::m_handleBayerBuffer(ExynosCameraFrameSP_sptr_t frame)
{
    status_t ret = NO_ERROR;
    uint32_t bufferDirection = INVALID_BUFFER_DIRECTION;
    uint32_t pipeId = MAX_PIPE_NUM;
    ExynosCameraBuffer buffer;
    ExynosCameraBufferManager *bufferMgr = NULL;
    ExynosCameraFrameEntity* doneEntity = NULL;

    ExynosCamera3FrameFactory *factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];

    int dstPos = 0;

    if (frame->getRequest(PIPE_VC0) == true) {
        dstPos = factory->getNodeType(PIPE_VC0);
    } else if (frame->getRequest(PIPE_3AC) == true) {
        dstPos = factory->getNodeType(PIPE_3AC);
    } else {
        CLOGE("both PIPE_VC0 and PIPE_3AC does not requested. so, fail");
        return INVALID_OPERATION;
    }

    if (frame == NULL) {
        CLOGE("Frame is NULL");
        return BAD_VALUE;
    }

    doneEntity = frame->getFrameDoneFirstEntity();
    if(doneEntity == NULL) {
        CLOGE("No frame done entity.");
        return BAD_VALUE;
    }
    pipeId = doneEntity->getPipeId();

    switch(pipeId) {
        case PIPE_FLITE:
            // Pure bayer reprocessing bayer (FLITE-3AA M2M)
            // RAW capture (Both FLITE-3AA M2M and OTF)
            bufferDirection = DST_BUFFER_DIRECTION;
            ret = frame->getDstBuffer(pipeId, &buffer, dstPos);
            if (ret != NO_ERROR) {
                CLOGE("Get bayer buffer failed, framecount(%d), direction(%d), pipeId(%d)",
                        frame->getFrameCount(), bufferDirection, pipeId);
                return ret;
            }
            ret = m_getBufferManager(pipeId, &bufferMgr, bufferDirection);
            if (ret != NO_ERROR) {
                CLOGE("getBufferManager failed, pipeId(%d), bufferDirection(%d), ret(%d)",
                         pipeId, bufferDirection, ret);
                return ret;
            }
            break;

        case PIPE_3AA:
            if (frame->getRequest(PIPE_VC0) == true) {
                bufferDirection = DST_BUFFER_DIRECTION;
                ret = m_getBufferManager(PIPE_VC0, &bufferMgr, bufferDirection);
                if (ret != NO_ERROR) {
                    CLOGE("getBufferManager failed, pipeId(%d), bufferDirection(%d), ret(%d)",
                            pipeId, bufferDirection, ret);
                    return ret;
                }

                if (m_parameters->isFlite3aaOtf() == true) {
                    bufferDirection = DST_BUFFER_DIRECTION;
                    ret = frame->getDstBuffer(pipeId, &buffer, dstPos);
                    if (ret != NO_ERROR) {
                        CLOGE("Get bayer buffer failed, framecount(%d), direction(%d), pipeId(%d)",
                                frame->getFrameCount(), bufferDirection, pipeId);
                        return ret;
                    }
                } else {
                    bufferDirection = SRC_BUFFER_DIRECTION;
                    ret = frame->getSrcBuffer(pipeId, &buffer);
                    if (ret != NO_ERROR) {
                        CLOGE("Get bayer buffer failed, framecount(%d), direction(%d), pipeId(%d)",
                                frame->getFrameCount(), bufferDirection, pipeId);
                        return ret;
                    }
                }
            } else if (frame->getRequest(PIPE_3AC) == true) {
                // Dirty bayer reprocessing bayer @3AC
                bufferDirection = DST_BUFFER_DIRECTION;
                ExynosCamera3FrameFactory* factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
                ret = frame->getDstBuffer(pipeId, &buffer, factory->getNodeType(PIPE_3AC));
                if (ret != NO_ERROR) {
                    CLOGE("Get bayer buffer failed, framecount(%d), direction(%d), pipeId(%d)",
                            frame->getFrameCount(), bufferDirection, pipeId);
                    return ret;
                }
                ret = m_getBufferManager(PIPE_3AC, &bufferMgr, bufferDirection);
                if (ret != NO_ERROR) {
                    CLOGE("getBufferManager failed, pipeId(%d), bufferDirection(%d), ret(%d)",
                             pipeId, bufferDirection, ret);
                    return ret;
                }
            } else {
                CLOGE("both PIPE_VC0 and PIPE_3AC does not requested. so, fail");
                return INVALID_OPERATION;
            }
            break;
        default:
            // Weird case
            CLOGE(" Invalid bayer handling PipeID=%d, FrameCnt=%d"
                ,  pipeId, frame->getFrameCount());
            return INVALID_OPERATION;
    }
    assert(pipeId >= 0);
    assert(pipeId < MAX_PIPE_NUM);
    assert(buffer != NULL);
    assert(bufferMgr != NULL);

#if 0
    /* Decide the bayer buffer position and pipe ID */
    if (frame->getRequest(PIPE_FLITE) == true) {
        // TODO: Improper condition.
    //if (m_parameters->isFlite3aaOtf() == true) {
        pipeId = PIPE_FLITE;
        bufferDirection = DST_BUFFER_DIRECTION;
        ret = frame->getDstBuffer(pipeId, &buffer);
    } else {
        pipeId = PIPE_3AA;
        if(m_parameters->getUsePureBayerReprocessing() == true) {
            // Pure bayer and FLITE-3AA OTF -> Bayer should be extracted from 3AA SRC buffer
            bufferDirection = SRC_BUFFER_DIRECTION;
            ret = frame->getSrcBuffer(pipeId, &buffer);
        } else {
            // Dirty bayer and FLITE-3AA OTF -> Bayer should be extracted from 3AC buffer
            bufferDirection = DST_BUFFER_DIRECTION;
            ExynosCamera3FrameFactory* factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
            ret = frame->getDstBuffer(pipeId, &buffer, factory->getNodeType(PIPE_3AC));
            /*
             * In Frame, 3AC buffer is the first buffer entry of PIPE_3AA entity.
             * So we don't have to specify dstPos for frame handling functions
             * such as manageFrameHoldList().
            */
        }
    }
    if (ret != NO_ERROR) {
        CLOGE("Get bayer buffer failed, framecount(%d), direction(%d), pipeId(%d)",
                frame->getFrameCount(), bufferDirection, pipeId);
        return ret;
    }

    // BufferManager is associated with the node, not PIPE.
    // So we need to distinguish 3AC and 3AA for dirtybayer reprocessing mode.
    if (frame->getRequest(PIPE_VC0) == true) {
        ret = m_getBufferManager(PIPE_VC0, &bufferMgr, bufferDirection);
    } else if (frame->getRequest(PIPE_3AC) == true) {
        ret = m_getBufferManager(PIPE_3AC, &bufferMgr, bufferDirection);
    } else {
        CLOGE("both PIPE_VC0 and PIPE_3AC does not requested. so fail");
        return INVALID_OPERATION;
    }

    if (ret != NO_ERROR) {
        CLOGE("getBufferManager failed, pipeId(%d), bufferDirection(%d), ret(%d)",
                 pipeId, bufferDirection, ret);
        return ret;
    }
#endif


    /* Check the validation of bayer buffer */
    if (buffer.index < 0) {
        CLOGE("Invalid bayer buffer, framecount(%d), direction(%d), pipeId(%d)",
                frame->getFrameCount(), bufferDirection, pipeId);
        return INVALID_OPERATION;
    }

    /* Handle the bayer buffer */
    if (frame->getFrameServiceBayer() == true
        && (m_parameters->getUsePureBayerReprocessing() == true ||
            pipeId == PIPE_FLITE ||
            pipeId == PIPE_3AA)) {
        /* Raw Capture Request */
        CLOGV("Handle service bayer buffer. FLITE-3AA_OTF %d Bayer_Pipe_ID %d Framecount %d",
                m_parameters->isFlite3aaOtf(), pipeId, frame->getFrameCount());

        /* Raw capture takes only the Pure bayer */
        {
            ret = m_sendRawStreamResult(frame, pipeId, (bufferDirection == SRC_BUFFER_DIRECTION), dstPos);
            if (ret != NO_ERROR) {
                CLOGE("Failed to m_sendRawStreamResult. frameCount %d bayerPipeId %d bufferIndex %d",
                        frame->getFrameCount(), pipeId, buffer.index);
                goto CHECK_RET_PUTBUFFER;
            }
        }

        if (m_parameters->isReprocessing() == true && frame->getFrameCapture() == true) {
            CLOGD("Hold service bayer buffer for reprocessing."
                    "frameCount %d bayerPipeId %d bufferIndex %d",
                    frame->getFrameCount(), pipeId, buffer.index);
            ret = m_captureZslSelector->manageFrameHoldListForDynamicBayer(frame);
            if (ret != NO_ERROR) {
                CLOGE("Failed to manageFrameHoldListForDynamicBayer to captureZslSelector."
                        "frameCount %d bayerPipeId %d bufferIndex %d",
                        frame->getFrameCount(), pipeId, buffer.index);
                goto CHECK_RET_PUTBUFFER;
            }
        }
        goto CHECK_RET_PUTBUFFER;

    } else if (frame->getFrameZsl() == true) {
        /* ZSL Capture Request */
        CLOGV("Handle ZSL buffer. FLITE-3AA_OTF %d Bayer_Pipe_ID %d Framecount %d",
                m_parameters->isFlite3aaOtf(), pipeId, frame->getFrameCount());

        ret = m_sendZslStreamResult(frame, pipeId, (bufferDirection == SRC_BUFFER_DIRECTION));
        if (ret != NO_ERROR) {
            CLOGE("Failed to m_sendZslStreamResult. frameCount %d bayerPipeId %d",
                    frame->getFrameCount(), pipeId);
        }

        /* release zsl buffer */
        ret = m_zslBufferMgr->release(buffer.index);
        if (ret != NO_ERROR) {
            CLOGE("Failed to release. bufferIndex %d",
                    buffer.index);
        }

        goto SKIP_PUTBUFFER;

    } else if (m_parameters->isReprocessing() == true){
        /* For ZSL Reprocessing */
        CLOGV("Hold internal bayer buffer for reprocessing."
                " FLITE-3AA_OTF %d Bayer_Pipe_ID %d Framecount %d",
                m_parameters->isFlite3aaOtf(), pipeId, frame->getFrameCount());
        ret = m_captureSelector->manageFrameHoldList(frame, pipeId, (bufferDirection == SRC_BUFFER_DIRECTION), dstPos);
        goto CHECK_RET_PUTBUFFER;

    } else {
        /* No request for bayer image */
        CLOGV("Return internal bayer buffer. FLITE-3AA_OTF %d Bayer_Pipe_ID %d Framecount %d",
                m_parameters->isFlite3aaOtf(), pipeId, frame->getFrameCount());

        ret = m_putBuffers(bufferMgr, buffer.index);
        if (ret !=  NO_ERROR) {
            CLOGE("putBuffers failed, pipeId(%d), bufferDirection(%d), bufferIndex(%d), ret(%d)",
                     pipeId, bufferDirection, buffer.index, ret);
        }
    }

    if (ret != NO_ERROR) {
        CLOGE("Handling bayer buffer failed, isServiceBayer(%d), direction(%d), pipeId(%d)",
                frame->getFrameServiceBayer(),
                bufferDirection, pipeId);
    }

    return ret;

CHECK_RET_PUTBUFFER:
    /* Put the bayer buffer if there was an error during processing */
    if(ret != NO_ERROR) {
        CLOGE("Handling bayer buffer failed, Bayer buffer will be put. isServiceBayer(%d), direction(%d), pipeId(%d)",
                frame->getFrameServiceBayer(),
                bufferDirection, pipeId);

        if (m_putBuffers(bufferMgr, buffer.index) !=  NO_ERROR) {
            // Do not taint 'ret' to print appropirate error log on next block.
            CLOGE("putBuffers failed, pipeId(%d), bufferDirection(%d), bufferIndex(%d), ret(%d)",
                     pipeId, bufferDirection, buffer.index, ret);
        }
    }

SKIP_PUTBUFFER:
    return ret;
}

bool ExynosCamera3::m_previewStreamBayerPipeThreadFunc(void)
{
    status_t ret = 0;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;

    ret = m_pipeFrameDoneQ[PIPE_FLITE]->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            /* CLOGW("wait timeout"); */
        } else {
            CLOGE("wait and pop fail, ret(%d)", ret);
            /* TODO: doing exception handling */
        }
        return true;
    }
    return m_previewStreamFunc(newFrame, PIPE_FLITE);
}

bool ExynosCamera3::m_previewStream3AAPipeThreadFunc(void)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;

    ret = m_pipeFrameDoneQ[PIPE_3AA]->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGW("wait timeout");
        } else {
            CLOGE("wait and pop fail, ret(%d)", ret);
            /* TODO: doing exception handling */
        }
        return true;
    }
    return m_previewStreamFunc(newFrame, PIPE_3AA);
}

bool ExynosCamera3::m_previewStreamISPPipeThreadFunc(void)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;

    ret = m_pipeFrameDoneQ[PIPE_ISP]->waitAndPopProcessQ(&newFrame);
    if (ret != NO_ERROR) {
        if (ret == TIMED_OUT)
            CLOGW("wait timeout");
        else
            CLOGE("Failed to waitAndPopProcessQ. ret %d", ret);
        return true;
    }
    return m_previewStreamFunc(newFrame, PIPE_ISP);
}

bool ExynosCamera3::m_previewStreamMCSCPipeThreadFunc(void)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;

    ret = m_pipeFrameDoneQ[PIPE_MCSC]->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGW("wait timeout");
        } else {
            CLOGE("wait and pop fail, ret(%d)", ret);
            /* TODO: doing exception handling */
        }
        return true;
    }
    return m_previewStreamFunc(newFrame, PIPE_MCSC);
}

#ifdef USE_VRA_GROUP
bool ExynosCamera3::m_previewStreamVRAPipeThreadFunc(void)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;

    ret = m_pipeFrameDoneQ[PIPE_VRA]->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGW("wait timeout");
        } else {
            CLOGE("wait and pop fail, ret(%d)", ret);
            /* TODO: doing exception handling */
        }
        return true;
    }
    return m_previewStreamFunc(newFrame, PIPE_VRA);
}
#endif

bool ExynosCamera3::m_previewStreamFunc(ExynosCameraFrameSP_sptr_t newFrame, int pipeId)
{
    status_t ret = 0;
    ExynosCameraBuffer buffer;
    struct camera2_shot_ext shot_ext;
    /* only trace */

    if (newFrame == NULL) {
        CLOGE("frame is NULL");
        return true;
    }

    if (newFrame->getFrameType() == FRAME_TYPE_INTERNAL) {
        if (m_internalFrameThread->isRunning() == false)
            m_internalFrameThread->run();
        m_internalFrameDoneQ->pushProcessQ(&newFrame);
        return true;
    }

    CLOGV("stream thread : frameCount %d pipeId %d",
             newFrame->getFrameCount(), pipeId);

    /* TODO: M2M path is also handled by this */
    ret = m_handlePreviewFrame(newFrame, pipeId);
    if (ret < 0) {
        CLOGE("handle preview frame fail");
        return ret;
    }

    m_frameCompleteLock.lock();
    ret = newFrame->setEntityState(pipeId, ENTITY_STATE_COMPLETE);
    if (ret != NO_ERROR) {
        CLOGE("setEntityState fail, pipeId(%d), state(%d), ret(%d)",
             pipeId, ENTITY_STATE_COMPLETE, ret);
        m_frameCompleteLock.unlock();
        return ret;
    }

    if (newFrame->isComplete() == true) {
//        m_sendNotify(newFrame->getFrameCount(), type);
        ret = newFrame->getMetaData(&shot_ext);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Failed to getMetaData. framecount %d ret %d",
                    __FUNCTION__, __LINE__,
                    newFrame->getFrameCount(), ret);
        }

        CLOGV("newFrame->getFrameCount(%d)", newFrame->getFrameCount());
#ifdef USE_VRA_GROUP
        ret = m_pushResult(newFrame->getFrameCount(), &shot_ext);
        if (ret != NO_ERROR) {
            CLOGE("Failed to pushResult. framecount %d ret %d", newFrame->getFrameCount(), ret);
        }
#endif

        m_sendMeta(newFrame->getFrameCount(), EXYNOS_REQUEST_RESULT::CALLBACK_ALL_RESULT);
#if defined(ENABLE_FULL_FRAME)
        if (newFrame->getFrameCapture() == false) {
#endif
            ret = m_removeFrameFromList(&m_processList, &m_processLock, newFrame);
            if (ret < 0) {
                CLOGE("remove frame from processList fail, ret(%d)", ret);
            }

            CLOGV("frame complete, frameCount %d",
                     newFrame->getFrameCount());
            newFrame = NULL;

#if defined(ENABLE_FULL_FRAME)
        }
#endif
    }
    m_frameCompleteLock.unlock();

    return true;
}


status_t ExynosCamera3::m_updateTimestamp(ExynosCameraFrameSP_sptr_t frame, ExynosCameraBuffer *timestampBuffer)
{
    ExynosCameraRequestSP_sprt_t request = NULL;
    struct camera2_shot_ext *shot_ext = NULL;
    status_t ret = NO_ERROR;

    /* handle meta data */
    shot_ext = (struct camera2_shot_ext *) timestampBuffer->addr[timestampBuffer->planeCount -1];
    if (shot_ext == NULL) {
        CLOGE("shot_ext is NULL. framecount %d buffer %d",
                 frame->getFrameCount(), timestampBuffer->index);
        return INVALID_OPERATION;
    }

    /* HACK: W/A for timeStamp reversion */
    if (shot_ext->shot.udm.sensor.timeStampBoot < m_lastFrametime) {
        CLOGW("Timestamp is zero!");
        shot_ext->shot.udm.sensor.timeStampBoot = m_lastFrametime + 15000000;
    }

    m_lastFrametime = shot_ext->shot.udm.sensor.timeStampBoot;

    return ret;
}

#ifdef CORRECT_TIMESTAMP_FOR_SENSORFUSION
status_t ExynosCamera3::m_adjustTimestamp(ExynosCameraBuffer *timestampBuffer)
{
    struct camera2_shot_ext *shot_ext = NULL;
    status_t ret = NO_ERROR;
    uint64_t exposureTime = 0;

    /* handle meta data */
    shot_ext = (struct camera2_shot_ext *) timestampBuffer->addr[timestampBuffer->planeCount -1];
    if (shot_ext == NULL) {
        CLOGE("shot_ext is NULL. buffer %d",
                 timestampBuffer->index);
        return INVALID_OPERATION;
    }
    exposureTime = (uint64_t)shot_ext->shot.dm.sensor.exposureTime;

    shot_ext->shot.udm.sensor.timeStampBoot -= (exposureTime);
#ifdef CORRECTION_SENSORFUSION
    shot_ext->shot.udm.sensor.timeStampBoot += CORRECTION_SENSORFUSION;
#endif

    return ret;
}
#endif

status_t ExynosCamera3::m_handlePreviewFrame(ExynosCameraFrameSP_sptr_t frame, int pipeId)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameEntity *entity = NULL;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraBuffer buffer;
    ExynosCamera3FrameFactory *factory = NULL;
    ExynosCameraRequestSP_sprt_t request = NULL;
    uint32_t framecount = 0;
    int capturePipeId = -1;
    int streamId = -1;
    entity_buffer_state_t bufferState = ENTITY_BUFFER_STATE_INVALID;


    factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];

    int dstPos = 0;

    entity = frame->getFrameDoneFirstEntity(pipeId);
    if (entity == NULL) {
        CLOGE("current entity is NULL. framecount %d pipeId %d",
                 frame->getFrameCount(), pipeId);
        /* TODO: doing exception handling */
        return INVALID_OPERATION;
    }

    CLOGV("handle preview frame : previewStream frameCnt(%d) entityID(%d)",
             frame->getFrameCount(), entity->getPipeId());

    switch(pipeId) {
    case PIPE_FLITE:
        /* 1. Handle bayer buffer */
        if (m_parameters->isFlite3aaOtf() == true) {
            android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):PIPE_FLITE cannot come to %s when Flite3aaOtf. so, assert!!!!",
                    __FUNCTION__, __LINE__, __FUNCTION__);

            ret = m_handleBayerBuffer(frame);
            if (ret < NO_ERROR) {
                CLOGE("Handle bayer buffer failed, framecount(%d), pipeId(%d), ret(%d)",
                         frame->getFrameCount(), entity->getPipeId(), ret);
                return ret;
            }
        } else {
            /* handle src Buffer */
            ret = frame->getSrcBuffer(pipeId, &buffer);
            if (ret != NO_ERROR) {
                CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)", pipeId, ret);
            }

            if (buffer.index >= 0) {
                ExynosCameraBufferManager *bufferMgr = NULL;
                ret = m_getBufferManager(pipeId, &bufferMgr, SRC_BUFFER_DIRECTION);
                if (ret != NO_ERROR) {
                    CLOGE("m_getBufferManager(pipeId : %d) fail", pipeId);
                } else {
                    ret = m_putBuffers(bufferMgr, buffer.index);
                    if (ret != NO_ERROR) {
                        CLOGE("m_putBuffers(%d) fail", buffer.index);
                    }
                }
            }

            /* Send the bayer buffer to 3AA Pipe */
            dstPos = factory->getNodeType(PIPE_VC0);

            ret = frame->getDstBuffer(entity->getPipeId(), &buffer, dstPos);
            if (ret < 0) {
                CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                         entity->getPipeId(), ret);
                return ret;
            }

            if (buffer.index < 0) {
                CLOGE("Invalid buffer index(%d), framecount(%d), pipeId(%d)",
                        buffer.index, frame->getFrameCount(), entity->getPipeId());
                return BAD_VALUE;
            }

            ret = m_setupEntity(PIPE_3AA, frame, &buffer, NULL);
            if (ret != NO_ERROR) {
                CLOGE("setSrcBuffer failed, pipeId(%d), ret(%d)",
                         PIPE_3AA, ret);
                return ret;
            }

            factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
            factory->pushFrameToPipe(frame, PIPE_3AA);
        }
        break;
    case PIPE_3AA:
        /* Notify ShotDone to mainThread */
        framecount = frame->getFrameCount();
        m_shotDoneQ->pushProcessQ(&framecount);

        /* Return the dummy buffer */
        ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
        if (ret < 0) {
            CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)",
                    entity->getPipeId(), ret);
            return ret;
        }

        if (buffer.index < 0) {
            CLOGE("Invalid buffer index(%d), framecount(%d), pipeId(%d)",
                    buffer.index, frame->getFrameCount(), entity->getPipeId());
            return BAD_VALUE;
        }

        ret = m_updateTimestamp(frame, &buffer);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Failed to update timestamp", __FUNCTION__, __LINE__);
            return ret;
        }

        request = m_requestMgr->getRunningRequest(frame->getFrameCount());
        if (request == NULL) {
            CLOGE("getRequest failed ");
            return INVALID_OPERATION;
        }

        request->setSensorTimestamp(m_lastFrametime);

        ret = m_pushPartialResult(frame->getFrameCount(), (struct camera2_shot_ext *)buffer.addr[buffer.planeCount-1]);
        if (ret != NO_ERROR) {
            CLOGE("Failed to pushResult. framecount %d ret %d", newFrame->getFrameCount(), ret);
        }

        /* 1. Handle the buffer from 3AA output node */
        if (m_parameters->isFlite3aaOtf() == true) {
            CLOGV("Return 3AS Buffer. driver->framecount %d hal->framecount %d, index %d",
                    getMetaDmRequestFrameCount((struct camera2_shot_ext *)buffer.addr[buffer.planeCount-1]),
                    frame->getFrameCount(), buffer.index);

            ret = m_putBuffers(m_3aaBufferMgr, buffer.index);
            if (ret != NO_ERROR) {
                CLOGE("Failed to putBuffer");
                return ret;
            }
        }

        /* Handle bayer buffer */
        if (frame->getRequest(PIPE_VC0) == true ||
                frame->getRequest(PIPE_3AC) == true) {
            ret = m_handleBayerBuffer(frame);
            if (ret < NO_ERROR) {
                CLOGE("Handle bayer buffer failed, framecount(%d), pipeId(%d), req(3AC:%d, VC0:%d) ret(%d)",
                         frame->getFrameCount(), entity->getPipeId(),
                         frame->getRequest(PIPE_VC0), frame->getRequest(PIPE_3AC), ret);
                return ret;
            }
        }

        frame->setMetaDataEnable(true);

        m_sendNotify(framecount, CAMERA3_MSG_SHUTTER);
        m_send3AAMeta(framecount, EXYNOS_REQUEST_RESULT::CALLBACK_PARTIAL_3AA);

        if (m_parameters->is3aaIspOtf() == false) {
            /* Send the bayer buffer to 3AA Pipe */
            dstPos = factory->getNodeType(PIPE_3AP);

            ret = frame->getDstBuffer(entity->getPipeId(), &buffer, dstPos);
            if (ret < 0) {
                CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                         entity->getPipeId(), ret);
            }
            if (buffer.index < 0) {
                CLOGE("Invalid buffer index(%d), framecount(%d), pipeId(%d)",
                        buffer.index, frame->getFrameCount(), entity->getPipeId());
            }

            ret = m_setupEntity(PIPE_ISP, frame, &buffer, NULL);
            if (ret != NO_ERROR) {
                CLOGE("setSrcBuffer failed, pipeId(%d), ret(%d)",
                         PIPE_ISP, ret);
            }

            factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
            factory->pushFrameToPipe(frame, PIPE_ISP);
            break;
        }
    case PIPE_ISP:
        if (entity->getPipeId() == PIPE_ISP) {
            ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
            if (ret != NO_ERROR) {
                CLOGE("Failed to getSrcBuffer. pipeId %d ret %d",
                        entity->getPipeId(), ret);
                return ret;
            }

#ifdef CORRECT_TIMESTAMP_FOR_SENSORFUSION
            /* m_adjustTimestamp() is to do adjust timestamp for ITS/sensorFusion. */
            /* This function should be called once at here */
            ret = m_adjustTimestamp(&buffer);
            if (ret != NO_ERROR) {
                   CLOGE("Failed to adjust timestamp");
            }
#endif

            ret = m_pushResult(frame->getFrameCount(), (struct camera2_shot_ext *)buffer.addr[buffer.planeCount-1]);
            if (ret != NO_ERROR) {
                CLOGE("Failed to pushResult. framecount %d ret %d", newFrame->getFrameCount(), ret);
            }

            if (buffer.index >= 0) {
                ret = m_putBuffers(m_ispBufferMgr, buffer.index);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to putBuffer into ispBufferMgr");
                    break;
                }
            }
        }

        if (m_parameters->isIspMcscOtf() == false)
            break;
    case PIPE_MCSC:
        if (entity->getPipeId() == PIPE_MCSC) {
            ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
            if (ret < 0) {
                CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)",
                         entity->getPipeId(), ret);
                return ret;
            }

            ret = m_updateTimestamp(frame, &buffer);
            if (ret != NO_ERROR) {
                CLOGE("Failed to putBuffer");
                return ret;
            }

            ret = m_pushResult(frame->getFrameCount(), (struct camera2_shot_ext *)buffer.addr[buffer.planeCount-1]);
            if (ret != NO_ERROR) {
                CLOGE("Failed to pushResult. framecount %d ret %d", newFrame->getFrameCount(), ret);
            }

            if (buffer.index >= 0) {
                ret = m_putBuffers(m_mcscBufferMgr, buffer.index);
                if (ret < 0) {
                    CLOGE("put Buffer fail");
                    break;
                }
            }

            CLOGV("Return MCSC Buffer. driver->framecount %d hal->framecount %d",
                    getMetaDmRequestFrameCount((struct camera2_shot_ext *)buffer.addr[buffer.planeCount-1]),
                    frame->getFrameCount());
        }

        if (frame->getFrameState() == FRAME_STATE_SKIPPED) {
            CLOGW("Skipped frame. Force callback result. frameCount %d",
                     frame->getFrameCount());

            ret = m_forceResultCallback(frame->getFrameCount());
            if (ret != NO_ERROR) {
                CLOGE("Failed to forceResultCallback. frameCount %d",
                         frame->getFrameCount());
                return ret;
            }
        }

        /* PIPE_MCSC 0, 1, 2 */
        for (int i = 0; i < m_streamManager->getTotalYuvStreamCount(); i++) {
            capturePipeId = PIPE_MCSC0 + i;

            if (frame->getRequest(capturePipeId) == true) {
                ret = frame->getDstBuffer(entity->getPipeId(), &buffer, factory->getNodeType(capturePipeId));
                if (ret != NO_ERROR || buffer.index < 0) {
                    CLOGE("Failed to get DST buffer. pipeId %d bufferIndex %d frameCount %d ret %d",
                            capturePipeId, buffer.index, frame->getFrameCount(), ret);
                    return ret;
                }

                streamId = m_streamManager->getYuvStreamId(i);

                ret = m_resultCallback(&buffer, frame->getFrameCount(), streamId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to resultCallback."
                            " pipeId %d bufferIndex %d frameCount %d streamId %d ret %d",
                            capturePipeId, buffer.index, frame->getFrameCount(), streamId, ret);
                    return ret;
                }
            }
        }

#if defined(ENABLE_FULL_FRAME)
        if (frame->getFrameCapture() == true) {
            CLOGI("Capture frame(%d)", frame->getFrameCount());
            ret = m_handleYuvCaptureFrame(frame);
            if (ret != NO_ERROR)
                CLOGE("Failed to handleReprocessingDoneFrame, ret %d",
                         ret);

            m_captureStreamThread->run(PRIORITY_DEFAULT);
        }
#endif

#ifdef USE_VRA_GROUP
        if (m_parameters->isMcscVraOtf() == false) {
            if (frame->getRequest(PIPE_VRA) == true) {
                /* Send the Yuv buffer to VRA Pipe */
                dstPos = factory->getNodeType(PIPE_MCSC_DS);
                ret = frame->getDstBuffer(entity->getPipeId(), &buffer, dstPos);
                if (ret < 0) {
                    CLOGE("getDstBuffer fail, framecount(%d) pipeId(%d), ret(%d)",
                            frame->getFrameCount(), entity->getPipeId(), ret);
                }

                if (buffer.index < 0) {
                    CLOGE("Invalid buffer index(%d), framecount(%d), pipeId(%d)",
                            buffer.index, frame->getFrameCount(), entity->getPipeId());
                    ret = frame->setSrcBufferState(PIPE_VRA, ENTITY_BUFFER_STATE_ERROR);
                    if (ret != NO_ERROR) {
                        CLOGE("[F%d]Failed to setSrcBufferState into ERROR. pipeId %d ret %d", frame->getFrameCount(), PIPE_VRA, ret);
                    }
                } else {
                    ret = m_setupEntity(PIPE_VRA, frame, &buffer, NULL);
                    if (ret != NO_ERROR) {
                        CLOGE("m_setupEntity failed, framecount(%d) pipeId(%d), ret(%d)", frame->getFrameCount(), PIPE_VRA, ret);
                    }
                }

                factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
                factory->pushFrameToPipe(frame, PIPE_VRA);
            } else {
                if (frame->getRequest(PIPE_VRA) == true) {
                    frame->setRequest(PIPE_VRA, false);
                    ret = frame->setEntityState(PIPE_VRA, ENTITY_STATE_COMPLETE);
                    if (ret < 0) {
                        CLOGE("setEntityState fail, pipeId(%d), state(%d), ret(%d)",
                            PIPE_VRA, ENTITY_STATE_COMPLETE, ret);
                    }
                }
            }
        }
        break;
    case PIPE_VRA:
        if (entity->getPipeId() == PIPE_VRA) {
            if (frame->getRequest(PIPE_VRA)) {
                ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to getSrcBuffer. pipeId %d ret %d",
                            entity->getPipeId(), ret);
                }

                ret = frame->getSrcBufferState(entity->getPipeId(), &bufferState);
                if (ret < 0) {
                    CLOGE("getSrcBufferState fail, pipeId(%d), ret(%d)", entity->getPipeId(), ret);
                    return ret;
                }

                if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
                    CLOGE("Src buffer state is error index(%d), framecount(%d), pipeId(%d)",
                        buffer.index, frame->getFrameCount(), entity->getPipeId());
                }

                if (buffer.index >= 0) {
                    ret = m_putBuffers(m_vraBufferMgr, buffer.index);
                    if (ret != NO_ERROR) {
                        CLOGE("Recording buffer return fail");
                    }
                }
            }
        }
#endif

        break;
    default:
        CLOGE("Invalid pipe ID");
        break;
    }

    return ret;
}

status_t ExynosCamera3::m_resultCallback(ExynosCameraBuffer *buffer, uint32_t frameCount, int streamId)
{
    status_t ret = NO_ERROR;
    ExynosCameraBufferManager *bufferMgr = NULL;
    ExynosCameraStream *stream = NULL;
    camera3_stream_buffer_t streamBuffer;
    ExynosCameraRequestSP_sprt_t request = NULL;
    ResultRequest resultRequest = NULL;

    if (buffer == NULL) {
        CLOGE("buffer is NULL");
        return BAD_VALUE;
    }

    ret = m_streamManager->getStream(streamId, &stream);
    if (ret != NO_ERROR) {
        CLOGE("Failed to get stream %d from streamMgr",
                 streamId);
        return INVALID_OPERATION;
    }

    ret = stream->getStream(&streamBuffer.stream);
    if (ret != NO_ERROR) {
        CLOGE("Failed to get stream %d from ExynosCameraStream",
                 streamId);
        return INVALID_OPERATION;
    }

    stream->getBufferManager(&bufferMgr);
    ret = bufferMgr->getHandleByIndex(&streamBuffer.buffer, buffer->index);
    if (ret != NO_ERROR) {
        CLOGE("Failed to get buffer handle. streamId %d bufferIndex %d",
                 streamId, buffer->index);
        return INVALID_OPERATION;
    }

    streamBuffer.status = CAMERA3_BUFFER_STATUS_OK;
    streamBuffer.acquire_fence = -1;
    streamBuffer.release_fence = -1;

    request = m_requestMgr->getRunningRequest(frameCount);
    resultRequest = m_requestMgr->createResultRequest(frameCount,
                                                      EXYNOS_REQUEST_RESULT::CALLBACK_BUFFER_ONLY,
                                                      NULL, NULL);
    resultRequest->pushStreamBuffer(&streamBuffer);

    m_requestMgr->callbackSequencerLock();
    /* increaseCompleteBufferCount not used because of using BUFFER ONLY update */
//    request->increaseCompleteBufferCount();
    request->increaseDuplicateBufferCount();
    m_requestMgr->registerToResultList(resultRequest);
    m_requestMgr->callbackSequencerUnlock();

    return ret;
}

status_t ExynosCamera3::m_releaseBuffers(void)
{
    CLOGI("release buffer");

    if (m_fliteBufferMgr != NULL) {
        for (int bufIndex = 0; bufIndex < m_fliteBufferMgr->getAllocatedBufferCount(); bufIndex++)
            m_putBuffers(m_fliteBufferMgr, bufIndex);
        m_fliteBufferMgr->deinit();
    }
    if (m_3aaBufferMgr != NULL) {
        for (int bufIndex = 0; bufIndex < m_3aaBufferMgr->getAllocatedBufferCount(); bufIndex++)
            m_putBuffers(m_3aaBufferMgr, bufIndex);
        m_3aaBufferMgr->deinit();
    }
    if (m_ispBufferMgr != NULL) {
        for (int bufIndex = 0; bufIndex < m_ispBufferMgr->getAllocatedBufferCount(); bufIndex++)
            m_putBuffers(m_ispBufferMgr, bufIndex);
        m_ispBufferMgr->deinit();
    }
    if (m_mcscBufferMgr != NULL) {
        for (int bufIndex = 0; bufIndex < m_mcscBufferMgr->getAllocatedBufferCount(); bufIndex++)
            m_putBuffers(m_mcscBufferMgr, bufIndex);
        m_mcscBufferMgr->deinit();
    }
#ifdef USE_VRA_GROUP
    if (m_vraBufferMgr != NULL) {
        for (int bufIndex = 0; bufIndex < m_vraBufferMgr->getAllocatedBufferCount(); bufIndex++)
            m_putBuffers(m_vraBufferMgr, bufIndex);
        m_vraBufferMgr->deinit();
    }
#endif
    if (m_ispReprocessingBufferMgr != NULL) {
        for (int bufIndex = 0; bufIndex < m_ispReprocessingBufferMgr->getAllocatedBufferCount(); bufIndex++)
            m_putBuffers(m_ispReprocessingBufferMgr, bufIndex);
        m_ispReprocessingBufferMgr->deinit();
    }
    if (m_yuvCaptureBufferMgr != NULL) {
        for (int bufIndex = 0; bufIndex < m_yuvCaptureBufferMgr->getAllocatedBufferCount(); bufIndex++)
            m_putBuffers(m_yuvCaptureBufferMgr, bufIndex);
        m_yuvCaptureBufferMgr->deinit();
    }
    if (m_thumbnailBufferMgr != NULL) {
        for (int bufIndex = 0; bufIndex < m_thumbnailBufferMgr->getAllocatedBufferCount(); bufIndex++)
            m_putBuffers(m_thumbnailBufferMgr, bufIndex);
        m_thumbnailBufferMgr->deinit();
    }
    if (m_internalScpBufferMgr != NULL) {
        for (int bufIndex = 0; bufIndex < m_internalScpBufferMgr->getAllocatedBufferCount(); bufIndex++)
            m_putBuffers(m_internalScpBufferMgr, bufIndex);
        m_internalScpBufferMgr->deinit();
    }
#ifdef SUPPORT_DEPTH_MAP
    if (m_depthMapBufferMgr != NULL) {
        for (int bufIndex = 0; bufIndex < m_depthMapBufferMgr->getAllocatedBufferCount(); bufIndex++)
            m_putBuffers(m_depthMapBufferMgr, bufIndex);
        m_depthMapBufferMgr->deinit();
    }
#endif
    CLOGI("free buffer done");

    return NO_ERROR;
}

/* m_registerStreamBuffers
 * 1. Get the input buffers from the input request
 * 2. Get the output buffers from the input request
 * 3. Register each buffers into the matched buffer manager
 * This operation must be done before another request is delivered from the service.
 */
status_t ExynosCamera3::m_registerStreamBuffers(camera3_capture_request *request)
{
    status_t ret = NO_ERROR;
    const camera3_stream_buffer_t *buffer;
    const camera3_stream_buffer_t *bufferList;
    uint32_t bufferCount = 0;
    int streamId = -1;
    uint32_t requestKey = 0;
    ExynosCameraStream *stream = NULL;
    ExynosCameraBufferManager *bufferMgr = NULL;

    /* 1. Get the information of input buffers from the input request */
    requestKey = request->frame_number;
    buffer = request->input_buffer;

    /* 2. Register the each input buffers into the matched buffer manager */
    if (buffer != NULL) {
        stream = static_cast<ExynosCameraStream *>(buffer->stream->priv);
        stream->getID(&streamId);

        switch (streamId % HAL_STREAM_ID_MAX) {
            case HAL_STREAM_ID_ZSL_INPUT:
                m_registerBuffers(m_zslBufferMgr, requestKey, buffer);
                CLOGV("request(%d) inputBuffer(%p) buffer-StreamType(HAL_STREAM_ID_ZSL) size(%u x %u) ",
                        request->frame_number, buffer,
                        buffer->stream->width,
                        buffer->stream->height);
                break;
            case HAL_STREAM_ID_PREVIEW:
            case HAL_STREAM_ID_VIDEO:
            case HAL_STREAM_ID_JPEG:
            case HAL_STREAM_ID_CALLBACK:
            case HAL_STREAM_ID_CALLBACK_STALL:
            default:
                CLOGE("request(%d) inputBuffer(%p) buffer-stream type is inavlid(%d). size(%d x %d)",
                        requestKey, buffer, streamId,
                        buffer->stream->width,
                        buffer->stream->height);
                break;
        }
    }

    /* 3. Get the information of output buffers from the input request */
    bufferCount = request->num_output_buffers;
    bufferList = request->output_buffers;

    /* 4. Register the each output buffers into the matched buffer manager */
    for (uint32_t index = 0; index < bufferCount; index++) {
        buffer = &(bufferList[index]);
        stream = static_cast<ExynosCameraStream *>(bufferList[index].stream->priv);
        stream->getID(&streamId);

        switch (streamId % HAL_STREAM_ID_MAX) {
            case HAL_STREAM_ID_RAW:
                m_registerBuffers(m_bayerBufferMgr, requestKey, buffer);
                CLOGV("request(%d) outputBuffer(%p) buffer-StreamType(HAL_STREAM_ID_RAW) size(%u x %u) ",
                        request->frame_number, buffer,
                        bufferList[index].stream->width,
                        bufferList[index].stream->height);
                break;
            case HAL_STREAM_ID_ZSL_OUTPUT:
                m_registerBuffers(m_zslBufferMgr, requestKey, buffer);
                CLOGV("request(%d) outputBuffer(%p) buffer-StreamType(HAL_STREAM_ID_ZSL_OUTPUT) size(%u x %u) ",
                        request->frame_number, buffer,
                        bufferList[index].stream->width,
                        bufferList[index].stream->height);
                break;
            case HAL_STREAM_ID_PREVIEW:
                stream->getBufferManager(&bufferMgr);
                m_registerBuffers(bufferMgr, requestKey, buffer);
                CLOGV("request(%d) outputBuffer(%p) buffer-StreamType(HAL_STREAM_ID_PREVIEW) size(%u x %u) ",
                        request->frame_number, buffer,
                        bufferList[index].stream->width,
                        bufferList[index].stream->height);
                break;
            case HAL_STREAM_ID_VIDEO:
                stream->getBufferManager(&bufferMgr);
                m_registerBuffers(bufferMgr, requestKey, buffer);
                CLOGV("request(%d) outputBuffer(%p) buffer-StreamType(HAL_STREAM_ID_VIDEO) size(%u x %u) ",
                        request->frame_number, buffer,
                        bufferList[index].stream->width,
                        bufferList[index].stream->height);
                break;
            case HAL_STREAM_ID_JPEG:
                stream->getBufferManager(&bufferMgr);
                m_registerBuffers(bufferMgr, requestKey, buffer);
                CLOGD("request(%d) outputBuffer(%p) buffer-StreamType(HAL_STREAM_ID_JPEG) size(%u x %u) ",
                        request->frame_number, buffer,
                        bufferList[index].stream->width,
                        bufferList[index].stream->height);
                break;
            case HAL_STREAM_ID_CALLBACK:
                stream->getBufferManager(&bufferMgr);
                m_registerBuffers(bufferMgr, requestKey, buffer);
                CLOGV("request(%d) outputBuffer(%p) buffer-StreamType(HAL_STREAM_ID_CALLBACK) size(%u x %u) ",
                        request->frame_number, buffer,
                        bufferList[index].stream->width,
                        bufferList[index].stream->height);
                break;
            case HAL_STREAM_ID_CALLBACK_STALL:
                stream->getBufferManager(&bufferMgr);
                m_registerBuffers(bufferMgr, requestKey, buffer);
                CLOGV("request(%d) outputBuffer(%p) buffer-StreamType(HAL_STREAM_ID_CALLBACK_STALL) size(%u x %u) ",
                        request->frame_number, buffer,
                        bufferList[index].stream->width,
                        bufferList[index].stream->height);
                break;
            default:
                CLOGE("request(%d) outputBuffer(%p) buffer-StreamType is invalid(%d) size(%d x %d) ",
                        request->frame_number, buffer, streamId,
                        bufferList[index].stream->width,
                        bufferList[index].stream->height);
                break;
        }
    }

    return ret;
}

status_t ExynosCamera3::m_registerBuffers(
        ExynosCameraBufferManager *bufManager,
        int requestKey,
        const camera3_stream_buffer_t *streamBuffer)
{
    status_t ret = OK;
    buffer_handle_t *handle = streamBuffer->buffer;
    int acquireFence = streamBuffer->acquire_fence;
    int releaseFence = streamBuffer->release_fence;

    if (bufManager != NULL) {
        ret = bufManager->registerBuffer(
                            requestKey,
                            handle,
                            acquireFence,
                            releaseFence,
                            EXYNOS_CAMERA_BUFFER_POSITION_NONE);
        if (ret < 0) {
            CLOGE("putBuffer(%d) fail(%d)", requestKey, ret);
            return BAD_VALUE;
        }
    }

    return ret;
}

status_t ExynosCamera3::m_putBuffers(ExynosCameraBufferManager *bufManager, int bufIndex)
{
    status_t ret = NO_ERROR;
    if (bufManager != NULL)
        ret = bufManager->putBuffer(bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_NONE);

    return ret;
}

status_t ExynosCamera3::m_allocBuffers(
        ExynosCameraBufferManager *bufManager,
        int  planeCount,
        unsigned int *planeSize,
        unsigned int *bytePerLine,
        int  startIndex,
        int  reqBufCount,
        bool createMetaPlane,
        bool needMmap)
{
    int ret = 0;

    ret = bufManager->setInfo(
                        planeCount,
                        planeSize,
                        bytePerLine,
                        startIndex,
                        reqBufCount,
                        createMetaPlane,
                        needMmap);
    if (ret < 0) {
        CLOGE("setInfo fail");
        goto func_exit;
    }

    ret = bufManager->alloc();
    if (ret < 0) {
        CLOGE("alloc fail");
        goto func_exit;
    }

func_exit:

    return ret;
}

status_t ExynosCamera3::m_allocBuffers(
        ExynosCameraBufferManager *bufManager,
        int  planeCount,
        unsigned int *planeSize,
        unsigned int *bytePerLine,
        int  reqBufCount,
        bool createMetaPlane,
        bool needMmap)
{
    int ret = 0;

    ret = bufManager->setInfo(
                        planeCount,
                        planeSize,
                        bytePerLine,
                        reqBufCount,
                        createMetaPlane,
                        needMmap);
    if (ret < 0) {
        CLOGE("setInfo fail");
        goto func_exit;
    }

    ret = bufManager->alloc();
    if (ret < 0) {
        CLOGE("alloc fail");
        goto func_exit;
    }

func_exit:

    return ret;
}

status_t ExynosCamera3::m_allocBuffers(
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
        CLOGE("m_allocBuffers(minBufCount=%d, maxBufCount=%d, type=%d) fail",
             minBufCount, maxBufCount, type);
    }

    return ret;
}

status_t ExynosCamera3::m_allocBuffers(
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

    CLOGI("setInfo(planeCount=%d, minBufCount=%d, maxBufCount=%d, type=%d, allocMode=%d)",
         planeCount, minBufCount, maxBufCount, (int)type, (int)allocMode);

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
        CLOGE("setInfo fail");
        goto func_exit;
    }

    ret = bufManager->alloc();
    if (ret < 0) {
        CLOGE("alloc fail");
        goto func_exit;
    }

func_exit:

    return ret;
}

status_t ExynosCamera3::m_setBuffers(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = NO_ERROR;

    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES]    = {0};
    int maxBufferCount = 1;
    int planeCount     = 1;

    int maxSensorW  = 0, maxSensorH  = 0;
    int maxPreviewW  = 0, maxPreviewH  = 0;
    ExynosRect bdsRect;
#ifdef USE_VRA_GROUP
    int dsWidth = 0, dsHeight = 0;
    unsigned int dsFormat = 0;
#endif
    unsigned int bpp = 0;
    bool needMmap = false;

    CLOGI("alloc buffer - camera ID: %d",
             m_cameraId);

    m_parameters->getMaxSensorSize(&maxSensorW, &maxSensorH);
    CLOGI("HW Sensor MAX width x height = %dx%d",
            maxSensorW, maxSensorH);
    m_parameters->getPreviewBdsSize(&bdsRect);
    CLOGI("Preview BDS width x height = %dx%d",
            bdsRect.w, bdsRect.h);

    if (m_parameters->getHighSpeedRecording() == true) {
        m_parameters->getHwPreviewSize(&maxPreviewW, &maxPreviewH);
        CLOGI("PreviewSize(HW - Highspeed) width x height = %dx%d",
                maxPreviewW, maxPreviewH);
    } else {
        m_parameters->getMaxPreviewSize(&maxPreviewW, &maxPreviewH);
        CLOGI("PreviewSize(Max) width x height = %dx%d",
                maxPreviewW, maxPreviewH);
    }

#ifdef USE_VRA_GROUP
	m_parameters->getHwVraInputSize(&dsWidth, & dsHeight);
	dsFormat = m_parameters->getHwVraInputFormat();
	CLOGI("DS Size %dx%d Format %x", dsWidth, dsHeight, dsFormat);
#endif

    /* FLITE */
#if !defined(ENABLE_FULL_FRAME)
#ifdef CAMERA_PACKED_BAYER_ENABLE
    // TODO: May be like this??
    // bytesPerLine[0] = getBayerLineSize(tempRect.fullW, m_parameters->getBayerFormat(PIPE_FLITE))
    if (m_parameters->getUsePureBayerReprocessing() == true) {
        bytesPerLine[0] = getBayerLineSize(maxSensorW, m_parameters->getBayerFormat(PIPE_FLITE));
    } else {
        bytesPerLine[0] = getBayerLineSize(maxSensorW, m_parameters->getBayerFormat(PIPE_3AC));
    }
    planeSize[0]    = bytesPerLine[0] * maxSensorH;
#else
    bytesPerLine[0] = maxSensorW * 2;
    planeSize[0]    = bytesPerLine[0] * maxSensorH;
#endif
    planeCount      = 2;
    maxBufferCount  = m_exynosconfig->current->bufInfo.num_sensor_buffers;

#ifdef RESERVED_MEMORY_ENABLE
    if (getCameraId() == CAMERA_ID_FRONT) {
        m_fliteBufferMgr->setContigBufCount(FRONT_RESERVED_NUM_BAYER_BUFFERS);
    } else {
        m_fliteBufferMgr->setContigBufCount(RESERVED_NUM_BAYER_BUFFERS);
    }
    type = EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE;
#else
    type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
#endif

    ret = m_allocBuffers(m_fliteBufferMgr, planeCount, planeSize, bytesPerLine,
                        maxBufferCount, maxBufferCount, type, true, false);
    if (ret < 0) {
        CLOGE("fliteBuffer m_allocBuffers(bufferCount=%d) fail",
             maxBufferCount);
        return ret;
    }
#endif

    /* Depth map buffer */
#ifdef SUPPORT_DEPTH_MAP
    if (m_parameters->getUseDepthMap()) {
        int depthMapW = 0, depthMapH = 0;
        int needMmap = true;
        int bayerFormat;

        depthMapW = MAX_DEPTH_SIZE_W;
        depthMapH = MAX_DEPTH_SIZE_H;

        CLOGI("Depth Map Size width x height = %dx%d",
                 depthMapW, depthMapH);

        bayerFormat     = DEPTH_MAP_FORMAT;
        bytesPerLine[0] = getBayerLineSize(depthMapW, bayerFormat);
        planeSize[0]    = getBayerPlaneSize(depthMapW, depthMapH, bayerFormat);
        planeCount      = 2;
        maxBufferCount  = m_exynosconfig->current->bufInfo.num_sensor_buffers;
        needMmap = true;
        type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;

        if (m_depthMapBufferMgr == NULL) {
            CLOGI("Failed to m_depthMapBufferMgr.");
        } else {
            ret = m_allocBuffers(m_depthMapBufferMgr,
                    planeCount, planeSize, bytesPerLine,
                    maxBufferCount, maxBufferCount,
                    type, true, needMmap);
            if (ret != NO_ERROR) {
                CLOGE("Failed to allocate Depth Map Buffers. bufferCount %d",
                    maxBufferCount);
                return ret;
            }
        }

        CLOGI("m_allocBuffers(DepthMap Buffer) %d x %d,\
            planeSize(%d), planeCount(%d), maxBufferCount(%d)",
             depthMapW, depthMapH,
            planeSize[0], planeCount, maxBufferCount);
    }
#endif

    /* 3AA */
    planeSize[0]    = 32 * 64 * 2;
    planeCount      = 2;
    maxBufferCount  = m_exynosconfig->current->bufInfo.num_3aa_buffers;
    ret = m_allocBuffers(m_3aaBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, true);
    if (ret < 0) {
        CLOGE("m_3aaBufferMgr m_allocBuffers(bufferCount=%d) fail",
             maxBufferCount);
        return ret;
    }

    /* ISP */
#if defined (USE_ISP_BUFFER_SIZE_TO_BDS)
    bytesPerLine[0] = bdsRect.w * 2;
    planeSize[0]    = bytesPerLine[0] * bdsRect.h;
#else
    bytesPerLine[0] = getBayerLineSize(maxPreviewW, m_parameters->getBayerFormat(PIPE_ISP));
    planeSize[0]    = bytesPerLine[0] * maxPreviewH;
#endif
    planeCount      = 2;
    maxBufferCount  = m_exynosconfig->current->bufInfo.num_3aa_buffers;

#ifdef RESERVED_MEMORY_ENABLE
    if (getCameraId() == CAMERA_ID_FRONT) {
        m_ispBufferMgr->setContigBufCount(FRONT_RESERVED_NUM_ISP_BUFFERS);
    } else {
        m_ispBufferMgr->setContigBufCount(RESERVED_NUM_ISP_BUFFERS);
    }
    type = EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE;
#else
    type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
#endif

    ret = m_allocBuffers(m_ispBufferMgr, planeCount, planeSize, bytesPerLine,
                        maxBufferCount, maxBufferCount, type, true, false);
    if (ret < 0) {
        CLOGE("m_ispBufferMgr m_allocBuffers(bufferCount=%d) fail",
             maxBufferCount);
        return ret;
    }

    /* MCSC */
    if (m_parameters->isIspTpuOtf() == false
        && m_parameters->isIspMcscOtf() == false) {
#if defined (USE_ISP_BUFFER_SIZE_TO_BDS)
        bytesPerLine[0] = bdsRect.w * 2;
        planeSize[0]    = bytesPerLine[0] * bdsRect.h;
#else
        bytesPerLine[0] = maxPreviewW * 2;
        planeSize[0]    = bytesPerLine[0] * maxPreviewH;
#endif
        planeCount      = 2;
        maxBufferCount  = m_exynosconfig->current->bufInfo.num_hwdis_buffers;

        ret = m_allocBuffers(m_mcscBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, true);
        if (ret < 0) {
            CLOGE("m_mcscBufferMgr m_allocBuffers(bufferCount=%d) fail",
                 maxBufferCount);
            return ret;
        }
    }

#ifdef USE_VRA_GROUP
    if (m_parameters->isMcscVraOtf() == false) {
        memset(planeSize, 0, sizeof(planeSize));

        ret = getYuvPlaneSize(dsFormat, planeSize, dsWidth, dsHeight);
        if (ret < 0) {
            CLOGE("getYuvPlaneSize(dsFormat(%x), dsWidth(%d x %d)) fail",
                dsFormat, dsWidth, dsHeight);
            return INVALID_OPERATION;
        }

        ret = getYuvFormatInfo(dsFormat, &bpp, (unsigned int*)&planeCount);
        if (ret < 0) {
            CLOGE("getYuvFormatInfo(hwPreviewFormat(%x)) fail",
                dsFormat);
            return INVALID_OPERATION;
        }

        planeCount += 1;
        maxBufferCount = m_exynosconfig->current->bufInfo.num_vra_buffers;
        needMmap = false;

        ret = m_allocBuffers(m_vraBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, true, needMmap);
        if (ret < 0) {
            CLOGE("m_vraBufferMgr m_allocBuffers(bufferCount=%d) fail", maxBufferCount);
            return ret;
        }
    }
#endif

    if (m_parameters->isSupportZSLInput()) {
        /* TODO: Can be allocated on-demand, when the ZSL input is exist. */
        ret = m_setInternalScpBuffer();
        if (ret < 0) {
            CLOGE("m_setReprocessing Buffer fail");
            return ret;
        }
    }

    CLOGI("alloc buffer done - camera ID: %d", m_cameraId);
    return ret;
}

bool ExynosCamera3::m_setBuffersThreadFunc(void)
{
    int ret;

    ret = m_setBuffers();
    if (ret < 0) {
        CLOGE("m_setBuffers failed");
        m_releaseBuffers();
        return false;
    }

    return false;
}

uint32_t ExynosCamera3::m_getBayerPipeId(void)
{
    uint32_t pipeId = 0;

    if (m_parameters->isFlite3aaOtf() == true) {
        pipeId = PIPE_3AA;
    } else {
        pipeId = PIPE_FLITE;
    }

    return pipeId;
}

status_t ExynosCamera3::m_pushServiceRequest(camera3_capture_request *request, ExynosCameraRequestSP_dptr_t req)
{
    status_t ret = OK;
    FrameFactoryList factoryList;
    FrameFactoryListIterator factorylistIter;
    ExynosCamera3FrameFactory *factory = NULL;
    bool isInternalNeed;

    CLOGV("m_pushServiceRequest frameCnt(%d)", request->frame_number);

    req = m_requestMgr->registerToServiceList(request);
    if (req == NULL) {
        CLOGE("registerToServiceList failed, FrameNumber = [%d]"
            , request->frame_number);
        return ret;
    }

    /* Recheck whether the curren request requires internal frame or not
       -> Internal frame is necessary when PreviewFrameFactory is not included in factory list.
    */
    isInternalNeed = true;
    req->getFrameFactoryList(&factoryList);
    for (factorylistIter = factoryList.begin(); factorylistIter != factoryList.end(); factorylistIter++) {
        factory = *factorylistIter;

        if(factory == m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW]) {
            isInternalNeed = false;
            break;
        }
    }
    req->setNeedInternalFrame(isInternalNeed);

    return NO_ERROR;
}

status_t ExynosCamera3::m_popServiceRequest(ExynosCameraRequestSP_dptr_t request)
{
    status_t ret = OK;

    CLOGV("m_popServiceRequest ");

    request = m_requestMgr->registerToRunningList();
    if (request == NULL) {
        CLOGE("createRequest failed ");
        ret = INVALID_OPERATION;
    }
    return ret;
}


/* m_hasCaptureStream is for reprocessing */
bool ExynosCamera3::m_hasCaptureStream(ExynosCameraRequestSP_sprt_t request)
{
    camera3_stream_buffer_t *output_buffers;
    List<int> *outputStreamId = NULL;
    List<int>::iterator outputStreamIdIter;
    ExynosCameraStream *stream = NULL;
    int streamId = 0;

    request->getAllRequestOutputStreams(&outputStreamId);
    bool notifyFlag = true;

    /* HACK: can't send notify cause of one request including render, video */
    if (outputStreamId != NULL) {
        for (outputStreamIdIter = outputStreamId->begin(); outputStreamIdIter != outputStreamId->end(); outputStreamIdIter++) {

            m_streamManager->getStream(*outputStreamIdIter, &stream);
            if (stream == NULL) {
                CLOGE("stream is NULL");
                break;
            }
            stream->getID(&streamId);

            switch (streamId % HAL_STREAM_ID_MAX) {
            case HAL_STREAM_ID_RAW:
            case HAL_STREAM_ID_PREVIEW:
            case HAL_STREAM_ID_VIDEO:
            case HAL_STREAM_ID_CALLBACK:
                notifyFlag = false;
                break;
            default:
                break;
            };
        }
    }

    return notifyFlag;
}


status_t ExynosCamera3::m_pushResult(uint32_t frameCount, struct camera2_shot_ext *src_ext)
{
    // TODO: More efficient implementation is required.
    Mutex::Autolock lock(m_updateMetaLock);

    status_t ret = NO_ERROR;
    ExynosCameraRequestSP_sprt_t request = NULL;
    struct camera2_shot_ext dst_ext;
    uint8_t currentPipelineDepth = 0;

    request = m_requestMgr->getRunningRequest(frameCount);
    if (request == NULL) {
        CLOGE("getRequest failed ");
        return INVALID_OPERATION;
    }

    ret = request->getResultShot(&dst_ext);
    if (ret < 0) {
        CLOGE("getResultShot failed ");
        return INVALID_OPERATION;
    }

    // this part not necessary at Partial update, becase partial update do work meta update twice.
/*
    if (dst_ext.shot.dm.request.frameCount >= src_ext->shot.dm.request.frameCount) {
        CLOGI("Skip to update result. frameCount %d requestKey %d shot.request.frameCount %d",
                frameCount, request->getKey(), dst_ext.shot.dm.request.frameCount);
        return ret;
    }
*/
    currentPipelineDepth = dst_ext.shot.dm.request.pipelineDepth;
    memcpy(&dst_ext.shot.dm, &src_ext->shot.dm, sizeof(struct camera2_dm));
    memcpy(&dst_ext.shot.udm, &src_ext->shot.udm, sizeof(struct camera2_udm));
    dst_ext.shot.dm.request.pipelineDepth = currentPipelineDepth;

    ret = request->setResultShot(&dst_ext);
    if (ret < 0) {
        CLOGE("setResultShot failed ");
        return INVALID_OPERATION;
    }

    ret = m_metadataConverter->updateDynamicMeta(request);

#ifdef DEBUG_STREAM_CONFIGURATIONS
    CLOGD("DEBUG_STREAM_CONFIGURATIONS:Set result. match[request(%d) / frameCount(%d) (dm : %d)]",
            request->getKey(), frameCount, dst_ext.shot.dm.request.frameCount);
#else
    CLOGV("Set result. match[request(%d) / frameCount(%d) (dm : %d)]",
            request->getKey(), frameCount, dst_ext.shot.dm.request.frameCount);
#endif

    return ret;
}

status_t ExynosCamera3::m_pushPartialResult(uint32_t frameCount, struct camera2_shot_ext *src_ext)
{
    status_t ret = NO_ERROR;
    ExynosCameraRequestSP_sprt_t request = NULL;
    struct camera2_shot_ext dst_ext;
    uint8_t currentPipelineDepth = 0;

    request = m_requestMgr->getRunningRequest(frameCount);
    if (request == NULL) {
        CLOGE("getRequest failed ");
        return INVALID_OPERATION;
    }

    ret = request->getResultShot(&dst_ext);
    if (ret < 0) {
        CLOGE("getResultShot failed ");
        return INVALID_OPERATION;
    }

    currentPipelineDepth = dst_ext.shot.dm.request.pipelineDepth;
    memcpy(&dst_ext.shot.dm, &src_ext->shot.dm, sizeof(struct camera2_dm));
    memcpy(&dst_ext.shot.udm, &src_ext->shot.udm, sizeof(struct camera2_udm));
    dst_ext.shot.dm.request.pipelineDepth = currentPipelineDepth;

    ret = request->setResultShot(&dst_ext);
    if (ret < 0) {
        CLOGE("setResultShot failed ");
        return INVALID_OPERATION;
    }

    ret = m_metadataConverter->updateDynamicMeta(request, true);

#ifdef DEBUG_STREAM_CONFIGURATIONS
    CLOGD("DEBUG_STREAM_CONFIGURATIONS:Set result. match[request(%d) / frameCount(%d) (dm : %d)]",
            request->getKey(), frameCount, dst_ext.shot.dm.request.frameCount);
#else
    CLOGV("Set result. match[request(%d) / frameCount(%d) (dm : %d)]",
            request->getKey(), frameCount, dst_ext.shot.dm.request.frameCount);
#endif

    return ret;
}


status_t ExynosCamera3::m_pushJpegResult(ExynosCameraFrameSP_sptr_t frame, int size, ExynosCameraBuffer *buffer)
{
    status_t ret = NO_ERROR;
    ExynosCameraStream *stream = NULL;
    camera3_stream_buffer_t streamBuffer;
    camera3_stream_buffer_t *output_buffers;
    ResultRequest resultRequest = NULL;
    ExynosCameraRequestSP_sprt_t request = NULL;
    camera3_capture_result_t requestResult;

    ExynosCameraBufferManager *bufferMgr = NULL;

    ret = m_streamManager->getStream(HAL_STREAM_ID_JPEG, &stream);
    if (ret != NO_ERROR) {
        CLOGE("Failed to getStream from StreamMgr. streamId HAL_STREAM_ID_JPEG");
        return ret;
    }

    if (stream == NULL) {
        CLOGE("stream is NULL");
        return INVALID_OPERATION;
    }

    ret = stream->getStream(&streamBuffer.stream);
    if (ret != NO_ERROR) {
        CLOGE("Failed to getStream from ExynosCameraStream. streamId HAL_STREAM_ID_JPEG");
        return ret;
    }

    ret = stream->getBufferManager(&bufferMgr);
    if (ret != NO_ERROR) {
        CLOGE("Failed to getBufferManager. streamId HAL_STREAM_ID_JPEG");
        return ret;
    }

    ret = bufferMgr->getHandleByIndex(&streamBuffer.buffer, buffer->index);
    if (ret != NO_ERROR) {
        CLOGE("Failed to getHandleByIndex. bufferIndex %d",
                 buffer->index);
        return ret;
    }

    /* release zsl buffer */
    ret = bufferMgr->release(buffer->index);
    if (ret != NO_ERROR) {
        CLOGE("Failed to release. bufferIndex %d",
                 buffer->index);
        return ret;
    }

    streamBuffer.status = CAMERA3_BUFFER_STATUS_OK;
    streamBuffer.acquire_fence = -1;
    streamBuffer.release_fence = -1;

    camera3_jpeg_blob_t jpeg_bolb;
    jpeg_bolb.jpeg_blob_id = CAMERA3_JPEG_BLOB_ID;
    jpeg_bolb.jpeg_size = size;
    int32_t jpegBufferSize = ((private_handle_t *)(*streamBuffer.buffer))->width;
    memcpy(buffer->addr[0] + jpegBufferSize - sizeof(camera3_jpeg_blob_t), &jpeg_bolb, sizeof(camera3_jpeg_blob_t));

    request = m_requestMgr->getRunningRequest(frame->getFrameCount());
    m_sendMeta(frame->getFrameCount(), EXYNOS_REQUEST_RESULT::CALLBACK_ALL_RESULT);

#if !defined(ENABLE_FULL_FRAME)
    /* try to notify if notify callback was not called in same framecount */
    if (request->getCallbackDone(EXYNOS_REQUEST_RESULT::CALLBACK_NOTIFY_ONLY) == false) {
        /* can't send notify cause of one request including render, video */
        if (m_hasCaptureStream(request) == true) {
            m_sendNotify(frame->getFrameCount(), CAMERA3_MSG_SHUTTER);
//            m_sendMeta(frame->getFrameCount(), EXYNOS_REQUEST_RESULT::CALLBACK_ALL_RESULT);
        }
    }
#endif

    /* update jpeg size */
    CameraMetadata setting = request->getResultMeta();
    int32_t jpegsize = size;
    ret = setting.update(ANDROID_JPEG_SIZE, &jpegsize, 1);
    if (ret < 0) {
        CLOGE(" ANDROID_JPEG_SIZE update failed(%d)", ret);
    }
    request->setResultMeta(setting);

    CLOGD("Set JPEG result Done. frameCount %d request->frame_number %d",
            frame->getFrameCount(), request->getFrameCount());

    resultRequest = m_requestMgr->createResultRequest(frame->getFrameCount(), EXYNOS_REQUEST_RESULT::CALLBACK_BUFFER_ONLY, NULL, NULL);
    resultRequest->pushStreamBuffer(&streamBuffer);

    m_requestMgr->callbackSequencerLock();
    /* increaseCompleteBufferCount not used because of using BUFFER ONLY update */
//    request->increaseCompleteBufferCount();
    m_requestMgr->registerToResultList(resultRequest);
    m_requestMgr->callbackSequencerUnlock();

    return ret;
}

ExynosCameraRequestSP_sprt_t ExynosCamera3::m_popResult(CameraMetadata &result, uint32_t frameCount)
{
    ExynosCameraRequestSP_sprt_t request = NULL;
    struct camera2_shot_ext dst_ext;

    request = m_requestMgr->getRunningRequest(frameCount);
    if (request == NULL) {
        CLOGE("getRequest failed ");
        result.clear();
        return NULL;
    }

    result = request->getResultMeta();

    CLOGV("m_popResult(%d)", request->getFrameCount());

    return request;
}

status_t ExynosCamera3::m_deleteRunningRequest(uint32_t frameCount)
{
    status_t ret = OK;

    ret = m_requestMgr->removeFromRunningList(frameCount);

    return ret;
}

status_t ExynosCamera3::m_setReprocessingBuffer(void)
{
    status_t ret = NO_ERROR;
    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
    buffer_manager_allocation_mode_t allocMode = BUFFER_MANAGER_ALLOCATION_ONDEMAND;
    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    int planeCount  = 0;
    int minBufferCount = NUM_REPROCESSING_BUFFERS;
    int maxBufferCount = NUM_PICTURE_BUFFERS;
    int maxPictureW = 0, maxPictureH = 0;
    int maxThumbnailW = 0, maxThumbnailH = 0;
    int pictureFormat = 0;

    if (m_parameters->getHighSpeedRecording() == true)
        m_parameters->getHwSensorSize(&maxPictureW, &maxPictureH);
    else
        m_parameters->getMaxPictureSize(&maxPictureW, &maxPictureH);
    m_parameters->getMaxThumbnailSize(&maxThumbnailW, &maxThumbnailH);
    pictureFormat = m_parameters->getHwPictureFormat();

    CLOGI("Max Picture %ssize %dx%d format %x",
            (m_parameters->getHighSpeedRecording() == true)? "on HighSpeedRecording ":"",
            maxPictureW, maxPictureH, pictureFormat);
    CLOGI("MAX Thumbnail size %dx%d",
            maxThumbnailW, maxThumbnailH);

    /* Reprocessing buffer from 3AA to ISP */
    if (m_parameters->isReprocessing() == true
            && m_parameters->isReprocessing3aaIspOTF() == false) {
#ifdef CAMERA_PACKED_BAYER_ENABLE
#ifdef DEBUG_RAWDUMP
        if (m_parameters->checkBayerDumpEnable()) {
            bytesPerLine[0] = maxPictureW* 2;
            planeSize[0] = maxPictureW* maxPictureH* 2;
        } else
#endif /* DEBUG_RAWDUMP */
        {
            bytesPerLine[0] = getBayerLineSize(maxPictureW, m_parameters->getBayerFormat(PIPE_ISP_REPROCESSING));
            // Old: bytesPerLine[0] = ROUND_UP((maxPictureW * 3 / 2), 16);
            planeSize[0] = bytesPerLine[0] * maxPictureH;
        }
#else
        planeSize[0] = maxPictureW* maxPictureH* 2;
#endif
        planeCount  = 2;
        minBufferCount = NUM_REPROCESSING_BUFFERS;
        type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;
        allocMode = BUFFER_MANAGER_ALLOCATION_ONDEMAND;

        ret = m_allocBuffers(
                m_ispReprocessingBufferMgr,
                planeCount, planeSize, bytesPerLine,
                minBufferCount, maxBufferCount, type,
                allocMode, true, false);
        if (ret < 0) {
            CLOGE("m_ispReprocessingBufferMgr m_allocBuffers(minBufferCount=%d/maxBufferCount=%d) fail",
                     minBufferCount, maxBufferCount);
            return ret;
        }
    }

    /* Reprocessing YUV buffer */
    switch (pictureFormat) {
        case V4L2_PIX_FMT_NV21M:
            planeCount      = 3;
            planeSize[0]    = ALIGN_UP(maxPictureW, GSCALER_IMG_ALIGN) * ALIGN_UP(maxPictureH, GSCALER_IMG_ALIGN);
            planeSize[1]    = ALIGN_UP(maxPictureW, GSCALER_IMG_ALIGN) * ALIGN_UP(maxPictureH, GSCALER_IMG_ALIGN) / 2;
            break;
        case V4L2_PIX_FMT_NV21:
            planeCount      = 2;
            planeSize[0]    = ALIGN_UP(maxPictureW, GSCALER_IMG_ALIGN) * ALIGN_UP(maxPictureH, GSCALER_IMG_ALIGN) * 3 / 2;
            break;
        default:
            planeCount      = 2;
            planeSize[0]    = ALIGN_UP(maxPictureW, GSCALER_IMG_ALIGN) * ALIGN_UP(maxPictureH, GSCALER_IMG_ALIGN) * 2;
            break;
    }
    minBufferCount = 1;
    maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;

    type = EXYNOS_CAMERA_BUFFER_ION_CACHED_SYNC_FORCE_TYPE;

    if (m_parameters->isUseHWFC() == true)
        allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
    else
        allocMode = BUFFER_MANAGER_ALLOCATION_ONDEMAND;

    ret = m_allocBuffers(
            m_yuvCaptureBufferMgr,
            planeCount, planeSize, bytesPerLine,
            minBufferCount, maxBufferCount,
            type, allocMode, true, false);
    if (ret < 0) {
        CLOGE("m_yuvCaptureBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
             minBufferCount, maxBufferCount);
        return ret;
    }

    /* Reprocessing Thumbanil buffer */
    switch (pictureFormat) {
        case V4L2_PIX_FMT_NV21M:
            planeCount      = 3;
            planeSize[0]    = maxThumbnailW * maxThumbnailH;
            planeSize[1]    = maxThumbnailW * maxThumbnailH / 2;
            break;
        case V4L2_PIX_FMT_NV21:
        default:
            planeCount      = 2;
            planeSize[0]    = FRAME_SIZE(V4L2_PIX_2_HAL_PIXEL_FORMAT(pictureFormat), maxThumbnailW, maxThumbnailH);
            break;
    }

    minBufferCount = 1;
    maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;
    type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;

    ret = m_allocBuffers(
            m_thumbnailBufferMgr,
            planeCount, planeSize, bytesPerLine,
            minBufferCount, maxBufferCount,
            type, allocMode, true, false);
    if (ret != NO_ERROR) {
        CLOGE("m_thumbnailBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
                 minBufferCount, maxBufferCount);
        return ret;
    }

    return NO_ERROR;
}

bool ExynosCamera3::m_reprocessingFrameFactoryStartThreadFunc(void)
{
    status_t ret = 0;
    ExynosCamera3FrameFactory *factory = NULL;

    factory = m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING];
    if (factory == NULL) {
        CLOGE("Can't start FrameFactory!!!! FrameFactory is NULL!!");

        return false;
    } else if (factory->isCreated() == false) {
        CLOGE("Reprocessing FrameFactory is NOT created!");
        return false;
    } else if (factory->isRunning() == true) {
        CLOGW("Reprocessing FrameFactory is already running");
        return false;
    }

    /* Set buffer manager */
    ret = m_setupReprocessingPipeline();
    if (ret != NO_ERROR) {
        CLOGE("Failed to setupReprocessingPipeline. ret %d",
                 ret);
        return false;
    }

    ret = factory->initPipes();
    if (ret < 0) {
        CLOGE("Failed to initPipes. ret %d",
                 ret);
        return false;
    }

    ret = m_startReprocessingFrameFactory(factory);
    if (ret < 0) {
        CLOGE("Failed to startReprocessingFrameFactory");
        /* TODO: Need release buffers and error exit */
        return false;
    }

    return false;
}

status_t ExynosCamera3::m_startReprocessingFrameFactory(ExynosCamera3FrameFactory *factory)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = 0;

    CLOGD("- IN -");

    ret = factory->preparePipes();
    if (ret < 0) {
        CLOGE("m_reprocessingFrameFactory preparePipe fail");
        return ret;
    }

    /* stream on pipes */
    ret = factory->startPipes();
    if (ret < 0) {
        CLOGE("m_reprocessingFrameFactory startPipe fail");
        return ret;
    }

    return NO_ERROR;
}

status_t ExynosCamera3::m_stopReprocessingFrameFactory(ExynosCamera3FrameFactory *factory)
{
    CLOGI("");
    status_t ret = 0;

    if (factory != NULL && factory->isRunning() == true) {
        ret = factory->stopPipes();
        if (ret < 0) {
            CLOGE("m_reprocessingFrameFactory0>stopPipe() fail");
        }
    }

    CLOGD("clear m_captureProcessList(Picture) Frame list");
    ret = m_clearList(&m_captureProcessList, &m_captureProcessLock);
    if (ret < 0) {
        CLOGE("m_clearList fail");
        return ret;
    }

    return NO_ERROR;
}

status_t ExynosCamera3::m_checkBufferAvailable(uint32_t pipeId, ExynosCameraBufferManager *bufferMgr)
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
            CLOGW("retry(%d) setupEntity for pipeId(%d)", retry, pipeId);
    } while(ret < 0 && retry < (TOTAL_WAITING_TIME/WAITING_TIME));

    return ret;
}

bool ExynosCamera3::m_startPictureBufferThreadFunc(void)
{
    int ret = 0;

    if (m_parameters->isReprocessing() == true) {
        ret = m_setReprocessingBuffer();
        if (ret < 0) {
            CLOGE("m_setReprocessing Buffer fail");
            return ret;
        }
    }

    return false;
}

#ifdef SAMSUNG_COMPANION
int ExynosCamera3::m_getSensorId(int m_cameraId)
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

bool ExynosCamera3::m_companionThreadFunc(void)
{
    CLOGI("");
    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;
    int loop = false;
    int ret = 0;

    m_timer.start();

    m_companionNode = new ExynosCameraNode();

    ret = m_companionNode->create("companion", m_cameraId);
    if (ret < 0) {
        CLOGE("Companion Node create fail, ret(%d)", ret);
    }

    ret = m_companionNode->open(MAIN_CAMERA_COMPANION_NUM);
    if (ret < 0) {
        CLOGE("Companion Node open fail, ret(%d)", ret);
    }
    CLOGD("Companion Node(%d) opened running)", MAIN_CAMERA_COMPANION_NUM);

    ret = m_companionNode->setInput(m_getSensorId(m_cameraId));
    if (ret < 0) {
        CLOGE("Companion Node s_input fail, ret(%d)", ret);
    }
    CLOGD("Companion Node(%d) s_input)", MAIN_CAMERA_COMPANION_NUM);

    m_timer.stop();
    durationTime = m_timer.durationMsecs();
    CLOGD("duration time(%5d msec)", (int)durationTime);

    /* one shot */
    return loop;
}
#endif

#if defined(SAMSUNG_EEPROM)
bool ExynosCamera3::m_eepromThreadFunc(void)
{
    CLOGI("");
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
        CLOGE("failed to open sysfs. camera id = %d", m_cameraId);
        goto err;
    }

    if (fgets(sensorFW, sizeof(sensorFW), fp) == NULL) {
        CLOGE("failed to read sysfs entry");
        goto err;
    }

    /* int numread = strlen(sensorFW); */
    CLOGI(" eeprom read complete. Sensor FW ver: %s", sensorFW);

err:
    if (fp != NULL)
        fclose(fp);

    m_timer.stop();
    durationTime = m_timer.durationMsecs();
    CLOGD("duration time(%5d msec)", (int)durationTime);

    /* one shot */
    return false;
}
#endif

status_t ExynosCamera3::m_startCompanion(void)
{
#ifdef SAMSUNG_COMPANION
    if(m_use_companion == true) {
        if (m_companionNode == NULL) {
            m_companionThread = new mainCameraThread(this, &ExynosCamera3::m_companionThreadFunc,
                                                      "companionshotThread", PRIORITY_URGENT_DISPLAY);
            if (m_companionThread != NULL) {
                m_companionThread->run();
                CLOGD(" companionThread starts");
            } else {
                CLOGE("failed the m_companionThread.");
            }
        } else {
            CLOGW("m_companionNode != NULL. so, it already running");
        }
    }
#endif

#if defined(SAMSUNG_EEPROM)
    if ((m_use_companion == false) && (isEEprom(getCameraId()) == true)) {
        m_eepromThread = new mainCameraThread(this, &ExynosCamera3::m_eepromThreadFunc,
                                                  "cameraeepromThread", PRIORITY_URGENT_DISPLAY);
        if (m_eepromThread != NULL) {
            m_eepromThread->run();
            CLOGD(" eepromThread starts");
        } else {
            CLOGE("failed the m_eepromThread");
        }
    }
#endif

    return NO_ERROR;
}

status_t ExynosCamera3::m_stopCompanion(void)
{
#ifdef SAMSUNG_COMPANION
    if(m_use_companion == true) {
        if (m_companionThread != NULL) {
            CLOGI("wait m_companionThread");
            m_companionThread->requestExitAndWait();
            CLOGI("wait m_companionThread end");
        } else {
            CLOGI(" m_companionThread is NULL");
        }
        if (m_companionNode != NULL) {
            ExynosCameraDurationTimer timer;

            timer.start();

            if (m_companionNode->close() != NO_ERROR) {
                CLOGE("close fail");
            }
            delete m_companionNode;
            m_companionNode = NULL;

            timer.stop();
            CLOGD("Companion Node(%d) closed. duration time(%5d msec)", MAIN_CAMERA_COMPANION_NUM, (int)timer.durationMsecs());

        }
    }
#endif

#if defined(SAMSUNG_EEPROM)
    if ((m_use_companion == false) && (isEEprom(getCameraId()) == true)) {
        if (m_eepromThread != NULL) {
            CLOGI(" wait m_eepromThread");
            m_eepromThread->requestExitAndWait();
        } else {
            CLOGI(" m_eepromThread is NULL");
        }
    }
#endif

    return NO_ERROR;
}

status_t ExynosCamera3::m_waitCompanionThreadEnd(void)
{
    ExynosCameraDurationTimer timer;

    timer.start();

#ifdef SAMSUNG_COMPANION
    if (m_use_companion == true) {
        if (m_companionThread != NULL) {
            m_companionThread->join();
        } else {
            CLOGI("m_companionThread is NULL");
        }
    }
#endif

    timer.stop();
    CLOGI("companionThread joined, waiting time : duration time(%5d msec)",
         (int)timer.durationMsecs());

    return NO_ERROR;
}

status_t ExynosCamera3::m_generateInternalFrame(uint32_t frameCount, ExynosCamera3FrameFactory *factory, List<ExynosCameraFrameSP_sptr_t> *list, Mutex *listLock, ExynosCameraFrameSP_dptr_t newFrame)
{
    status_t ret = OK;

    newFrame = NULL;

    CLOGV("(%d)", frameCount);
    ret = m_searchFrameFromList(list, listLock, frameCount, newFrame);
    if (ret < 0) {
        CLOGE("searchFrameFromList fail");
        return INVALID_OPERATION;
    }

    if (newFrame == NULL) {
        newFrame = factory->createNewFrame(frameCount);
        if (newFrame == NULL) {
            CLOGE("newFrame is NULL");
            return UNKNOWN_ERROR;
        }
        listLock->lock();
        list->push_back(newFrame);
        listLock->unlock();
    }

    /* Set frame type into FRAME_TYPE_INTERNAL */
    ret = newFrame->setFrameInfo(m_parameters, frameCount, FRAME_TYPE_INTERNAL);
    if (ret != NO_ERROR) {
        CLOGE("Failed to setFrameInfo with INTERNAL. frameCount %d",
                 frameCount);
        return ret;
    }

    return ret;
}

bool ExynosCamera3::m_internalFrameThreadFunc(void)
{
    status_t ret = 0;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;

    /* 1. Get new internal frame */
    ret = m_internalFrameDoneQ->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGW("wait timeout");
        } else {
            CLOGE("wait and pop fail, ret(%d)", ret);
            /* TODO: doing exception handling */
        }
        return false;
    }

    CLOGV("handle internal frame : previewStream frameCnt(%d) (%d)",
             newFrame->getFrameCount(), newFrame->getFrameType());

    /* 2. Redirection for the normal frame */
    if (newFrame->getFrameType() != FRAME_TYPE_INTERNAL) {
        CLOGE("push to m_pipeFrameDoneQ handler : previewStream frameCnt(%d)",
                 newFrame->getFrameCount());
        m_pipeFrameDoneQ[PIPE_3AA]->pushProcessQ(&newFrame);
        return true;
    }

    /* 3. Handle the internal frame for each pipe */
    ret = m_handleInternalFrame(newFrame);

    if (ret < 0) {
        CLOGE("handle preview frame fail");
        return ret;
    }

    if (newFrame->isComplete() == true/* && newFrame->getFrameCapture() == false */) {
        ret = m_removeFrameFromList(&m_processList, &m_processLock, newFrame);
        if (ret < 0) {
            CLOGE("remove frame from processList fail, ret(%d)", ret);
        }

        CLOGV("internal frame complete, count(%d)", newFrame->getFrameCount());
        newFrame = NULL;
    }

    return true;
}

status_t ExynosCamera3::m_handleInternalFrame(ExynosCameraFrameSP_sptr_t frame)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameEntity *entity = NULL;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraBuffer buffer;
    ExynosCamera3FrameFactory *factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
    camera3_capture_result_t captureResult;
    camera3_notify_msg_t notityMsg;
    ExynosCameraRequestSP_sprt_t request = NULL;
    ResultRequest resultRequest = NULL;
    struct camera2_shot_ext meta_shot_ext;
    uint32_t framecount = 0;
    int dstPos = 0;
    entity_buffer_state_t bufferState = ENTITY_BUFFER_STATE_INVALID;


    entity = frame->getFrameDoneFirstEntity();
    if (entity == NULL) {
        CLOGE("current entity is NULL");
        /* TODO: doing exception handling */
        return true;
    }
    CLOGV("handle internal frame : previewStream frameCnt(%d) entityID(%d)",
             frame->getFrameCount(), entity->getPipeId());

    switch(entity->getPipeId()) {
    case PIPE_3AA:
        /* Notify ShotDone to mainThread */
        framecount = frame->getFrameCount();
        m_shotDoneQ->pushProcessQ(&framecount);

        ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
        if (ret < 0) {
            CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)",
                    entity->getPipeId(), ret);
            return ret;
        }

        if (buffer.index >= 0) {
            ret = m_putBuffers(m_3aaBufferMgr, buffer.index);
            if (ret != NO_ERROR) {
                CLOGE("put Buffer failed");
            }
        }

        if (frame->getRequest(PIPE_3AP) == true) {
            ret = frame->getDstBuffer(entity->getPipeId(), &buffer);
            if (ret < 0) {
                CLOGE("getDstBuffer failed, pipeId(%d), ret(%d)",
                         entity->getPipeId(), ret);
                return ret;
            }

            if (buffer.index >= 0) {
                ret = m_putBuffers(m_ispBufferMgr, buffer.index);
                if (ret < 0) {
                    CLOGE("put Buffer failed");
                }
            }
        }

            /* Handle bayer buffer */
        if (frame->getRequest(PIPE_VC0) == true && m_parameters->isFlite3aaOtf() == true) {
            ret = m_handleBayerBuffer(frame);
            if (ret < NO_ERROR) {
                CLOGE("Handle bayer buffer failed, framecount(%d), pipeId(%d), ret(%d)",
                         frame->getFrameCount(), entity->getPipeId(), ret);
                return ret;
            }
        }

        // TODO: Check the condition
        if(frame->getRequest(PIPE_3AC) == true || m_parameters->isFlite3aaOtf() == false) {
            CLOGD("[EJ] Pass the frame to handleBayerBuffer %d / m_isNeedRequestFrame = %d, m_isNeedInternalFrame = %d",
                         frame->getFrameCount(), m_isNeedRequestFrame, m_isNeedInternalFrame);
            ret = m_handleBayerBuffer(frame);
            if (ret != NO_ERROR) {
                CLOGE("Failed to handle bayerBuffer(3AA). framecount %d pipeId %d ret %d",
                        frame->getFrameCount(), entity->getPipeId(), ret);
                return ret;
            }
        }

        frame->setMetaDataEnable(true);

        if (m_parameters->is3aaIspOtf() == false) {
            if (frame->getRequest(PIPE_3AP) == true) {
                /* Send the bayer buffer to 3AA Pipe */
                dstPos = factory->getNodeType(PIPE_3AP);
                ret = frame->getDstBuffer(entity->getPipeId(), &buffer, dstPos);
                if (ret < 0) {
                    CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                            entity->getPipeId(), ret);
                }
                if (buffer.index < 0) {
                    CLOGE("Invalid buffer index(%d), framecount(%d), pipeId(%d)",
                            buffer.index, frame->getFrameCount(), entity->getPipeId());
                }

                ret = m_setupEntity(PIPE_ISP, frame, &buffer, NULL);
                if (ret != NO_ERROR) {
                    CLOGE("setSrcBuffer failed, pipeId(%d), ret(%d)", PIPE_ISP, ret);
                }

                factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
                factory->pushFrameToPipe(frame, PIPE_ISP);
            } else {
                ret = frame->setEntityState(PIPE_ISP, ENTITY_STATE_COMPLETE);
                if (ret < 0) {
                    CLOGE("setEntityState fail, pipeId(%d), state(%d), ret(%d)",
                            PIPE_ISP, ENTITY_STATE_COMPLETE, ret);
                    return ret;
                }
            }
        }
        break;
    case PIPE_FLITE:
        /* TODO: HACK: Will be removed, this is driver's job */
        if (m_parameters->isFlite3aaOtf() == true) {
            android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):PIPE_FLITE cannot come to %s when Flite3aaOtf. so, assert!!!!",
                    __FUNCTION__, __LINE__, __FUNCTION__);
            ret = m_handleBayerBuffer(frame);
            if (ret < NO_ERROR) {
                CLOGE("Handle bayer buffer failed, framecount(%d), pipeId(%d), ret(%d)",
                         frame->getFrameCount(), entity->getPipeId(), ret);
                return ret;
            }
        } else {
            /* handle src Buffer */
            ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
            if (ret != NO_ERROR) {
                CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)", entity->getPipeId(), ret);
            }

            if (buffer.index >= 0) {
                ExynosCameraBufferManager *bufferMgr = NULL;
                ret = m_getBufferManager(entity->getPipeId(), &bufferMgr, SRC_BUFFER_DIRECTION);
                if (ret != NO_ERROR) {
                    CLOGE("m_getBufferManager(pipeId : %d) fail", entity->getPipeId());
                } else {
                    ret = m_putBuffers(bufferMgr, buffer.index);
                    if (ret != NO_ERROR) {
                        CLOGE("m_putBuffers(%d) fail", buffer.index);
                    }
                }
            }

            /* Send the bayer buffer to 3AA Pipe */
            dstPos = factory->getNodeType(PIPE_VC0);

            ret = frame->getDstBuffer(entity->getPipeId(), &buffer, dstPos);
            if (ret < 0) {
                CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                         entity->getPipeId(), ret);
                return ret;
            }

            CLOGV("Deliver Flite Buffer to 3AA. driver->framecount %d hal->framecount %d",
                    getMetaDmRequestFrameCount((struct camera2_shot_ext *)buffer.addr[buffer.planeCount-1]),
                    frame->getFrameCount());

            ret = m_setupEntity(PIPE_3AA, frame, &buffer, NULL);
            if (ret != NO_ERROR) {
                CLOGE("setSrcBuffer failed, pipeId(%d), ret(%d)",
                         PIPE_3AA, ret);
                return ret;
            }

            factory->pushFrameToPipe(frame, PIPE_3AA);
        }

        break;
    case PIPE_ISP:
        ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
        if (ret != NO_ERROR) {
            CLOGE("Failed to getSrcBuffer. pipeId %d ret %d",
                     entity->getPipeId(), ret);
        }
        if (buffer.index >= 0) {
            ret = m_putBuffers(m_ispBufferMgr, buffer.index);
            if (ret != NO_ERROR) {
                CLOGE("Failed to putBuffer into ispBufferMgr. bufferIndex %d",
                         buffer.index);
                return ret;
            }
        }
    if (m_parameters->isIspTpuOtf() == false && m_parameters->isIspMcscOtf() == false)
        break;
    case PIPE_MCSC:
        if (entity->getPipeId() == PIPE_MCSC) {
	        ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
	        if (ret != NO_ERROR) {
	            CLOGE("Failed to getSrcBuffer. pipeId %d ret %d",
	                     entity->getPipeId(), ret);
	            return ret;
	        }

	        if (buffer.index >= 0) {
	            ret = m_putBuffers(m_mcscBufferMgr, buffer.index);
	            if (ret != NO_ERROR) {
	                CLOGE("Failed to putBuffer into mcscBufferMgr. bufferIndex %d",
	                         buffer.index);
	                return ret;
	            }
	        }
        }
        break;
    default:
        CLOGE("Invalid pipe ID");
        break;
    }

    ret = frame->setEntityState(entity->getPipeId(), ENTITY_STATE_COMPLETE);
    if (ret < 0) {
        CLOGE("setEntityState fail, pipeId(%d), state(%d), ret(%d)",
             entity->getPipeId(), ENTITY_STATE_COMPLETE, ret);
        return ret;
    }

    return ret;
}

status_t ExynosCamera3::m_forceResultCallback(uint32_t frameCount)
{
    status_t ret = NO_ERROR;
    camera3_stream_buffer_t streamBuffer;
    ExynosCameraBufferManager *bufferMgr = NULL;
    ExynosCameraBuffer buffer;
    ExynosCameraRequestSP_sprt_t request = NULL;
    ExynosCameraStream *stream = NULL;
    ResultRequest resultRequest = NULL;
    List<int> *outputStreamId;
    List<int>::iterator outputStreamIdIter;
    int requestIndex = -1;
    int streamId = 0;

    request = m_requestMgr->getRunningRequest(frameCount);
    if (request == NULL) {
        CLOGE("Failed to getRunningRequest. frameCount %d",
                 frameCount);
        return INVALID_OPERATION;
    }

    request->getAllRequestOutputStreams(&outputStreamId);
    if (outputStreamId->size() > 0) {
        outputStreamIdIter = outputStreamId->begin();

        for (int i = outputStreamId->size(); i > 0; i--) {
            if (*outputStreamIdIter < 0)
                break;

            m_streamManager->getStream(*outputStreamIdIter, &stream);
            if (stream == NULL) {
                CLOGE("Failed to getStream. frameCount %d requestKey %d",
                         frameCount, request->getKey());
                return INVALID_OPERATION;
            }

            stream->getID(&streamId);

            switch (streamId % HAL_STREAM_ID_MAX) {
            case HAL_STREAM_ID_RAW:
            case HAL_STREAM_ID_ZSL_OUTPUT:
            case HAL_STREAM_ID_JPEG:
            case HAL_STREAM_ID_CALLBACK_STALL:
                outputStreamIdIter++;
                continue;
            case HAL_STREAM_ID_PREVIEW:
            case HAL_STREAM_ID_VIDEO:
            case HAL_STREAM_ID_CALLBACK:
                break;
            default:
                CLOGE("Inavlid stream Id %d",
                         streamId);
                return BAD_VALUE;
            }

            ret = stream->getStream(&streamBuffer.stream);
            if (ret != NO_ERROR) {
                CLOGE("Failed to getStream. frameCount %d requestKey %d",
                         frameCount, request->getKey());
                return ret;
            }

            requestIndex = -1;
            stream->getBufferManager(&bufferMgr);
            ret = bufferMgr->getBuffer(&requestIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &buffer);
            if (ret != NO_ERROR) {
                CLOGE("Failed to getBuffer. frameCount %d requestKey %d streamId %d",
                         frameCount, request->getKey(), streamId);
                return ret;
            }

            ret = bufferMgr->getHandleByIndex(&streamBuffer.buffer, buffer.index);
            if (ret != NO_ERROR) {
                CLOGE("Failed to getHandleByIndex. frameCount %d requestKey %d streamId %d bufferIndex %d",
                         frameCount, request->getKey(), streamId, buffer.index);
                return ret;
            }

            streamBuffer.status = CAMERA3_BUFFER_STATUS_OK;
            streamBuffer.acquire_fence = -1;
            streamBuffer.release_fence = -1;

            resultRequest = m_requestMgr->createResultRequest(frameCount, EXYNOS_REQUEST_RESULT::CALLBACK_BUFFER_ONLY, NULL, NULL);
            resultRequest->pushStreamBuffer(&streamBuffer);
            m_requestMgr->callbackSequencerLock();
            /* increaseCompleteBufferCount not used because of using BUFFER ONLY update */
            //request->increaseCompleteBufferCount();
            m_requestMgr->registerToResultList(resultRequest);
            m_requestMgr->callbackSequencerUnlock();

            outputStreamIdIter++;
        }
    }

    return NO_ERROR;
}

#ifdef MONITOR_LOG_SYNC
uint32_t ExynosCamera3::m_getSyncLogId(void)
{
    return ++cameraSyncLogId;
}
#endif

bool ExynosCamera3::m_monitorThreadFunc(void)
{
    CLOGV("");

    int *threadState;
    struct timeval dqTime;
    uint64_t *timeInterval;
    int *countRenew;
    int loopCount = 0;

    int dtpStatus = 0;
    int pipeIdFlite = 0;
    int pipeIdScp = 0;
    ExynosCamera3FrameFactory *factory = NULL;

    for (loopCount = 0; loopCount < MONITOR_THREAD_INTERVAL; loopCount += (MONITOR_THREAD_INTERVAL/20)) {
        if (m_flushFlag == true) {
            CLOGI("m_flushFlag(%d)", m_flushFlag);
            return false;
        }
        usleep(MONITOR_THREAD_INTERVAL/20);
    }

    if (m_parameters->isFlite3aaOtf() == true || getCameraId() == CAMERA_ID_BACK) {
        pipeIdFlite = PIPE_FLITE;
        if(m_parameters->isIspMcscOtf() == true) {
            pipeIdScp = PIPE_ISP;
        } else {
            pipeIdScp = PIPE_MCSC;
        }
    } else {
        pipeIdFlite = PIPE_FLITE;
        if(m_parameters->isIspMcscOtf() == true) {
            pipeIdScp = PIPE_ISP;
        } else {
            pipeIdScp = PIPE_MCSC;
        }
    }

    factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
    if (factory == NULL) {
        CLOGW("frameFactory is NULL");
        return false;
    }

    pipeIdFlite = m_getBayerPipeId();
    enum NODE_TYPE dstPos = factory->getNodeType(PIPE_VC0);

    if (factory->checkPipeThreadRunning(pipeIdScp) == false) {
        CLOGW("Scp pipe is not running.. Skip monitoring.");
        return false;
    }
#ifdef MONITOR_LOG_SYNC
    uint32_t pipeIdIsp = PIPE_3AA;

    /* If it is not front camera in dual and sensor pipe is running, do sync log */
    if (factory->checkPipeThreadRunning(pipeIdIsp) &&
        !(getCameraId() == CAMERA_ID_FRONT && m_parameters->getDualMode())) {
        if (!(m_syncLogDuration % MONITOR_LOG_SYNC_INTERVAL)) {
            uint32_t syncLogId = m_getSyncLogId();
            CLOGI("@FIMC_IS_SYNC %d", syncLogId);
            factory->syncLog(pipeIdIsp, syncLogId);
        }
        m_syncLogDuration++;
    }
#endif
    factory->getControl(V4L2_CID_IS_G_DTPSTATUS, &dtpStatus, pipeIdFlite);

    if (dtpStatus == 1) {
        CLOGE("(%d)", dtpStatus);
        dump();

#if 0//def CAMERA_GED_FEATURE
        /* in GED */
        android_printAssert(NULL, LOG_TAG, "killed by itself");
#else
        /* specifically defined */
        /* m_notifyCb(CAMERA_MSG_ERROR, 1002, 0, m_callbackCookie); */
        /* or */
        /* android_printAssert(NULL, LOG_TAG, "killed by itself"); */
#endif
        return false;
    }

#ifdef SENSOR_OVERFLOW_CHECK
    factory->getControl(V4L2_CID_IS_G_DTPSTATUS, &dtpStatus, pipeIdFlite);
    if (dtpStatus == 1) {
        CLOGE("(%d)", dtpStatus);
        dump();
#if 0//def CAMERA_GED_FEATURE
        /* in GED */
        android_printAssert(NULL, LOG_TAG, "killed by itself");
#else
        /* specifically defined */
        /* m_notifyCb(CAMERA_MSG_ERROR, 1002, 0, m_callbackCookie); */
        /* or */
        /* android_printAssert(NULL, LOG_TAG, "killed by itself"); */
#endif
        return false;
    }
#endif
    factory->getThreadState(&threadState, pipeIdScp);
    factory->getThreadRenew(&countRenew, pipeIdScp);

    if ((*threadState == ERROR_POLLING_DETECTED) || (*countRenew > ERROR_DQ_BLOCKED_COUNT)) {
        CLOGE("(%d)", *threadState);
        if((*countRenew > ERROR_DQ_BLOCKED_COUNT))
            CLOGE("ERROR_DQ_BLOCKED) ; ERROR_DQ_BLOCKED_COUNT =20");

        dump();
#ifdef SAMSUNG_TN_FEATURE
#if 0
        /* this tn feature was commented for 3.2 */
        if (m_recordingEnabled == true
          && m_parameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)
          && m_recordingCallbackHeap != NULL
          && m_callbackCookie != NULL) {
          m_dataCbTimestamp(
              0,
              CAMERA_MSG_ERROR | CAMERA_MSG_VIDEO_FRAME,
              m_recordingCallbackHeap,
              0,
              m_callbackCookie);
              CLOGD("Timestamp callback with CAMERA_MSG_ERROR");
        }
#endif
#endif
#if 0//def CAMERA_GED_FEATURE
        /* in GED */
        android_printAssert(NULL, LOG_TAG, "killed by itself");
#else
        /* specifically defined */
        /* m_notifyCb(CAMERA_MSG_ERROR, 1002, 0, m_callbackCookie); */
        /* or */
        /* android_printAssert(NULL, LOG_TAG, "killed by itself"); */
#endif
        return false;
    } else {
        CLOGV("(%d)", *threadState);
    }

    gettimeofday(&dqTime, NULL);
    factory->getThreadInterval(&timeInterval, pipeIdScp);

    CLOGV("Thread IntervalTime [%lld]", *timeInterval);
    CLOGV("Thread Renew Count [%d]", *countRenew);

    factory->incThreadRenew(pipeIdScp);

    return true;
}

#ifdef YUV_DUMP
bool ExynosCamera3::m_dumpThreadFunc(void)
{
    char filePath[70];
    int size = -1;
    uint32_t pipeId = -1;
    uint32_t nodeIndex = -1;
    status_t ret = NO_ERROR;
    ExynosCamera3FrameFactory *factory = NULL;
    ExynosCameraFrameSP_sptr_t frame = NULL;
    ExynosCameraBuffer buffer;

    ret = m_dumpFrameQ->waitAndPopProcessQ(&frame);
    if (ret == TIMED_OUT) {
        CLOGW("Time-out to wait dumpFrameQ");
        goto THREAD_EXIT;
    } else if (ret != NO_ERROR) {
        CLOGE("Failed to waitAndPopProcessQ for dumpFrameQ");
        goto THREAD_EXIT;
    } else if (frame == NULL) {
        CLOGE("frame is NULL");
        goto THREAD_EXIT;
    }

    memset(filePath, 0, sizeof(filePath));
    snprintf(filePath, sizeof(filePath), "/data/YuvCapture.yuv");

    factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
    pipeId = PIPE_3AA_REPROCESSING;
    nodeIndex = factory->getNodeType(PIPE_MCSC3_REPROCESSING);

    ret = frame->getDstBuffer(pipeId, &buffer, nodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("Failed to getDstBuffer. frameCount %d pipeId %d nodeIndex %d",
                frame->getFrameCount(), pipeId, nodeIndex);
        goto THREAD_EXIT;
    }

    size = buffer.size[0];

    ret = dumpToFile((char *)filePath, buffer.addr[0], size);
    if (ret == false) {
        CLOGE("Failed to dumpToFile. frameCount %d filePath %s size %d",
                frame->getFrameCount(), filePath, size);
        goto THREAD_EXIT;
    }

THREAD_EXIT:
    if (m_dumpFrameQ->getSizeOfProcessQ() > 0) {
        CLOGI("run again. dumpFrameQ size %d",
                 m_dumpFrameQ->getSizeOfProcessQ());
        return true;
    } else {
        CLOGI("Complete to dumpFrame. frameCount %d",
                 frame->getFrameCount());
        return false;
    }
}
#endif

status_t ExynosCamera3::m_generateDuplicateBuffers(ExynosCameraFrameSP_sptr_t frame, int pipeIdSrc)
{
    status_t ret = NO_ERROR;
    ExynosCameraRequestSP_sprt_t halRequest = NULL;
    camera3_capture_request *serviceRequest = NULL;

    ExynosCameraStream *stream = NULL;
    int streamId = 0;

    List<int> *outputStreamId;
    List<int>::iterator outputStreamIdIter;

    if (frame == NULL) {
        CLOGE("frame is NULL");
        return INVALID_OPERATION;
    }

    halRequest = m_requestMgr->getRunningRequest(frame->getFrameCount());
    if (halRequest == NULL) {
        CLOGE("halRequest is NULL");
        return INVALID_OPERATION;
    }

    serviceRequest = halRequest->getService();
    if (serviceRequest == NULL) {
        CLOGE("serviceRequest is NULL");
        return INVALID_OPERATION;
    }

    CLOGV("frame->getFrameCount(%d) halRequest->getFrameCount(%d) serviceRequest->num_output_buffers(%d)",
        frame->getFrameCount(),
        halRequest->getFrameCount(),
        serviceRequest->num_output_buffers);

    halRequest->getAllRequestOutputStreams(&outputStreamId);

    if (outputStreamId->size() > 0) {
        for (outputStreamIdIter = outputStreamId->begin(); outputStreamIdIter != outputStreamId->end(); outputStreamIdIter++) {
            m_streamManager->getStream(*outputStreamIdIter, &stream);

            if (stream == NULL) {
                CLOGE("stream is NULL");
                continue;
            }

            stream->getID(&streamId);

            switch (streamId % HAL_STREAM_ID_MAX) {
            case HAL_STREAM_ID_RAW:
                break;
            case HAL_STREAM_ID_PREVIEW:
                m_doDestCSC(true, frame, pipeIdSrc, streamId, PIPE_GSC_REPROCESSING);
                break;
            case HAL_STREAM_ID_VIDEO:
                m_doDestCSC(true, frame, pipeIdSrc, streamId, PIPE_GSC_REPROCESSING);
                break;
            case HAL_STREAM_ID_JPEG:
                break;
            case HAL_STREAM_ID_CALLBACK:
                m_doDestCSC(true, frame, pipeIdSrc, streamId, PIPE_GSC_REPROCESSING);
                break;
            default:
                break;
            }
        }
    }

    return ret;
}

bool ExynosCamera3::m_duplicateBufferThreadFunc(void)
{
    status_t ret = 0;
    ExynosCameraFrameSP_sptr_t newFrame= NULL;
    dup_buffer_info_t dupBufferInfo;
    ExynosCameraBuffer srcBuffer;
    ExynosCameraBuffer dstBuffer;
    ExynosCameraStream *stream = NULL;
    camera3_stream_buffer_t streamBuffer;
    ExynosCameraRequestSP_sprt_t request = NULL;
    ResultRequest resultRequest = NULL;
    unsigned int completeBufferCount = 0;
    ExynosCameraBufferManager *bufferMgr = NULL;

    ret = m_duplicateBufferDoneQ->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGW("wait timeout");
        } else {
            CLOGE("wait and pop fail, ret(%d)", ret);
            /* TODO: doing exception handling */
        }
        goto func_exit;
    }

    if (newFrame == NULL) {
        CLOGE("frame is NULL");
        goto func_exit;
    }

    CLOGV("CSC done (frameCount(%d))", newFrame->getFrameCount());

    dupBufferInfo = newFrame->getDupBufferInfo();
    CLOGV("streamID(%d), extScalerPipeID(%d)", dupBufferInfo.streamID, dupBufferInfo.extScalerPipeID);

    ret = m_streamManager->getStream(dupBufferInfo.streamID, &stream);
    if (ret < 0) {
        CLOGE("getStream is failed, from streammanager. Id error:HAL_STREAM_ID_PREVIEW");
        goto func_exit;
    }

    ret = stream->getStream(&streamBuffer.stream);
    if (ret < 0) {
        CLOGE("getStream is failed, from exynoscamerastream. Id error:HAL_STREAM_ID_PREVIEW");
        goto func_exit;
    }

    stream->getBufferManager(&bufferMgr);
    CLOGV("bufferMgr(%p)", bufferMgr);

    ret = newFrame->getDstBuffer(dupBufferInfo.extScalerPipeID, &dstBuffer);
    if (ret < 0) {
        CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", dupBufferInfo.extScalerPipeID, ret);
        goto func_exit;
    }

    ret = bufferMgr->getHandleByIndex(&streamBuffer.buffer, dstBuffer.index);
    if (ret != OK) {
        CLOGE("Buffer index error(%d)!!", dstBuffer.index);
        goto func_exit;
    }

    /* update output stream buffer information */
    streamBuffer.status = CAMERA3_BUFFER_STATUS_OK;
    streamBuffer.acquire_fence = -1;
    streamBuffer.release_fence = -1;

    request = m_requestMgr->getRunningRequest(newFrame->getFrameCount());

    /* Get number of YUV streams to be generated */
    completeBufferCount = 0;
    for (size_t i = 0; i < request->getNumOfOutputBuffer(); i++) {
        int id = request->getStreamId(i);
        switch (id % HAL_STREAM_ID_MAX) {
            case HAL_STREAM_ID_PREVIEW:
            case HAL_STREAM_ID_VIDEO:
            case HAL_STREAM_ID_CALLBACK:
                completeBufferCount++;
        }
    }

    resultRequest = m_requestMgr->createResultRequest(newFrame->getFrameCount(), EXYNOS_REQUEST_RESULT::CALLBACK_BUFFER_ONLY, NULL, NULL);
    resultRequest->pushStreamBuffer(&streamBuffer);

    m_requestMgr->callbackSequencerLock();
    /* increaseCompleteBufferCount not used because of using BUFFER ONLY update */
//    request->increaseCompleteBufferCount();
    request->increaseDuplicateBufferCount();

    CLOGV("OutputBuffer(%d) CompleteBufferCount(%d) DuplicateBufferCount(%d) streamID(%d),"
        " extScaler(%d), frame: Count(%d), ServiceBayer(%d), Capture(%d) completeBufferCount(%d)",
        request->getNumOfOutputBuffer(),
        request->getCompleteBufferCount(),
        request->getDuplicateBufferCount(),
        dupBufferInfo.streamID,
        dupBufferInfo.extScalerPipeID,
        newFrame->getFrameCount(),
        newFrame->getFrameServiceBayer(),
        newFrame->getFrameCapture(),
        completeBufferCount);

    if(completeBufferCount == (unsigned int)request->getDuplicateBufferCount()) {
        ret = newFrame->getSrcBuffer(dupBufferInfo.extScalerPipeID, &srcBuffer);
        if (srcBuffer.index >= 0) {
            CLOGV("Internal Scp Buffer is returned index(%d)frameCount(%d)", srcBuffer.index, newFrame->getFrameCount());
            ret = m_putBuffers(m_internalScpBufferMgr, srcBuffer.index);
            if (ret < 0) {
                CLOGE("put Buffer fail");
            }
        }
    }
    m_requestMgr->registerToResultList(resultRequest);
    m_requestMgr->callbackSequencerUnlock();

func_exit:
    if (newFrame != NULL ) {
        newFrame = NULL;
    }
/*
    if (newFrame != NULL) {
        delete newFrame;
        newFrame = NULL;
    }
*/
    if (m_duplicateBufferDoneQ->getSizeOfProcessQ() > 0)
        return true;
    else
        return false;
}

status_t ExynosCamera3::m_doDestCSC(bool enableCSC, ExynosCameraFrameSP_sptr_t frame, int pipeIdSrc, int halStreamId, int pipeExtScalerId)
{
    status_t ret = OK;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosRect srcRect, dstRect;
    ExynosCamera3FrameFactory *factory = NULL;
    ExynosCameraBuffer srcBuffer;
    ExynosCameraBuffer dstBuffer;
    ExynosCameraStream *stream = NULL;
    camera3_stream_buffer_t streamBuffer;
    ExynosCameraRequestSP_sprt_t request = NULL;
    ResultRequest resultRequest = NULL;
    int actualFormat = 0;
    int bufIndex = -1;
    dup_buffer_info_t dupBufferInfo;
    struct camera2_stream *meta = NULL;
    uint32_t *output = NULL;

    ExynosCameraBufferManager *bufferMgr = NULL;

    CLOGI("Start: FrameCnt=%d, pipeIdSrc=%d, streamId=%d, pipeExtScalerId=%d",
            frame->getFrameCount(), pipeIdSrc, halStreamId, pipeExtScalerId);

    if (enableCSC == false) {
        /* TODO: memcpy srcBuffer, dstBuffer */
        return NO_ERROR;
    }

    request = m_requestMgr->getRunningRequest(frame->getFrameCount());
    factory = request->getFrameFactory(halStreamId);
    ret = frame->getDstBuffer(pipeIdSrc, &srcBuffer, factory->getNodeType(PIPE_MCSC2_REPROCESSING));
    if (ret < 0) {
        CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", pipeIdSrc, ret);
        return ret;
    }

    ret = m_streamManager->getStream(halStreamId, &stream);
    if (ret < 0) {
        CLOGE("getStream is failed, from streammanager. Id error:HAL_STREAM_ID_PREVIEW");
        return ret;
    }

    ret = stream->getStream(&streamBuffer.stream);
    if (ret < 0) {
        CLOGE("getStream is failed, from exynoscamerastream. Id error:HAL_STREAM_ID_PREVIEW");
        return ret;
    }

    meta = (struct camera2_stream*)srcBuffer.addr[srcBuffer.planeCount-1];
    if(meta == NULL) {
        CLOGE("Meta plane is not exist or null.");
        return NOT_ENOUGH_DATA;
    }
    output = meta->output_crop_region;

    stream->getBufferManager(&bufferMgr);
    CLOGV("bufferMgr(%p)", bufferMgr);

    ret = bufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &dstBuffer);
    if (ret < 0) {
        CLOGE("bufferMgr getBuffer fail, frameCount(%d), ret(%d)", frame->getFrameCount(), ret);
        return ret;
    }

    /* Refer the code in m_handleYuvCaptureFrame() for src rect setting */
    srcRect.x = output[0];
    srcRect.y = output[1];
    srcRect.w = output[2];
    srcRect.h = output[3];
    srcRect.fullW = output[2];
    srcRect.fullH = output[3];
    srcRect.colorFormat = m_parameters->getHwPictureFormat();

    pipeExtScalerId = PIPE_GSC_REPROCESSING;
#if 0
    switch (halStreamId % HAL_STREAM_ID_MAX) {
    /* not support ZSL
     *
     *   case HAL_STREAM_ID_RAW:
     *       break;
     *
     */
    case HAL_STREAM_ID_PREVIEW:
        ret = m_parameters->calcPreviewGSCRect(&srcRect, &dstRect);
        pipeExtScalerId = PIPE_GSC;
        break;
    case HAL_STREAM_ID_VIDEO:
        ret = m_parameters->calcRecordingGSCRect(&srcRect, &dstRect);
        pipeExtScalerId = PIPE_GSC;
        break;
    case HAL_STREAM_ID_JPEG:
        ret = m_parameters->calcPictureRect(&srcRect, &dstRect);
        pipeExtScalerId = PIPE_GSC;
        break;
    case HAL_STREAM_ID_CALLBACK:
        ret = m_parameters->calcPreviewGSCRect(&srcRect, &dstRect);
        pipeExtScalerId = PIPE_GSC;
        break;
    default:
        CLOGE("halStreamId is not valid(%d)", halStreamId);
        break;
    }
    if (ret != NO_ERROR) {
        CLOGE("calcGSCRect fail, frameCount(%d), ret(%d)", frame->getFrameCount(), ret);
        return ret;
    }
#endif

    newFrame = factory->createNewFrameOnlyOnePipe(pipeExtScalerId, frame->getFrameCount());

    ret = newFrame->setSrcRect(pipeExtScalerId, srcRect);
    if (ret != NO_ERROR) {
        CLOGE("setSrcRect fail, frameCount(%d), ret(%d)", frame->getFrameCount(), ret);
        return ret;
    }

    stream->getFormat(&actualFormat);
    dstRect.x = 0;
    dstRect.y = 0;
    dstRect.fullW = dstRect.w = streamBuffer.stream->width;
    dstRect.fullH = dstRect.h = streamBuffer.stream->height;
    dstRect.colorFormat = HAL_PIXEL_FORMAT_2_V4L2_PIX(actualFormat);

    CLOGV("srcRect size (%d, %d, %d, %d %d %d)",
        srcRect.x, srcRect.y, srcRect.w, srcRect.h, srcRect.fullW, srcRect.fullH);
    CLOGV("dstRect size (%d, %d, %d, %d %d %d)",
        dstRect.x, dstRect.y, dstRect.w, dstRect.h, dstRect.fullW, dstRect.fullH);

    ret = newFrame->setDstRect(pipeExtScalerId, dstRect);
    if (ret != NO_ERROR) {
        CLOGE("setDstRect fail, frameCount(%d), ret(%d)", frame->getFrameCount(), ret);
        return ret;
    }

    /* GSC can be shared by preview and previewCb. Make sure dstBuffer for previewCb buffer. */
    /* m_resetBufferState(pipeExtScalerId, frame); */
    m_resetBufferState(pipeExtScalerId, newFrame);

    ret = m_setupEntity(pipeExtScalerId, newFrame, &srcBuffer, &dstBuffer);
    if (ret < 0) {
        CLOGE("setupEntity fail, pipeExtScalerId(%d), ret(%d)", pipeExtScalerId, ret);
    }

    dupBufferInfo.streamID = halStreamId;
    dupBufferInfo.extScalerPipeID = pipeExtScalerId;
    newFrame->setDupBufferInfo(dupBufferInfo);
    newFrame->setFrameCapture(frame->getFrameCapture());
    newFrame->setFrameServiceBayer(frame->getFrameServiceBayer());

    factory->setOutputFrameQToPipe(m_duplicateBufferDoneQ, pipeExtScalerId);
    factory->pushFrameToPipe(newFrame, pipeExtScalerId);

    return ret;
}

status_t ExynosCamera3::m_setInternalScpBuffer(void)
{
    int ret = 0;
    int maxPreviewW = 0, maxPreviewH = 0;
    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    int planeCount  = 0;
    int minBufferCount = NUM_REPROCESSING_BUFFERS;
    int maxBufferCount = NUM_PREVIEW_BUFFERS;
    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
    buffer_manager_allocation_mode_t allocMode = BUFFER_MANAGER_ALLOCATION_ONDEMAND;

    if (m_parameters->getHighSpeedRecording() == true) {
        m_parameters->getHwPreviewSize(&maxPreviewW, &maxPreviewH);
    } else {
        m_parameters->getMaxPreviewSize(&maxPreviewW, &maxPreviewH);
    }

    bytesPerLine[0] = ALIGN_UP(maxPreviewW, GSCALER_IMG_ALIGN)* 2;
    planeSize[0] = bytesPerLine[0] * ALIGN_UP(maxPreviewH, GSCALER_IMG_ALIGN);
    planeCount  = 2;

    allocMode = BUFFER_MANAGER_ALLOCATION_ONDEMAND;

    ret = m_allocBuffers(m_internalScpBufferMgr, planeCount, planeSize, bytesPerLine, minBufferCount, maxBufferCount, type, allocMode, true, false);
    if (ret < 0) {
        CLOGE("m_internalScpBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail", minBufferCount, maxBufferCount);
        return ret;
    }

    return NO_ERROR;
}

status_t ExynosCamera3::m_checkRestartStream(ExynosCameraRequestSP_sprt_t request)
{
    status_t ret = NO_ERROR;
    status_t funcRet = NO_ERROR;

    bool restart = m_parameters->getRestartStream();

    if (restart) {
        CLOGD("internal restart stream request(%d)", request->getFrameCount());
        ret = m_restartStreamInternal();
        m_requestMgr->registerToServiceList(request);
        m_parameters->setRestartStream(false);
        funcRet |= ret;
    }

    return funcRet;
}

status_t ExynosCamera3::m_restartStreamInternal()
{
    CLOGI("Internal restart stream IN");
    status_t ret = NO_ERROR;
    status_t funcRet = NO_ERROR;
    ExynosCamera3FrameFactory *frameFactory = NULL;

    Mutex::Autolock l(m_resultLock);

    /* Wait for finishing to start pipeline */
    m_frameFactoryStartThread->join();
    m_reprocessingFrameFactoryStartThread->join();

    m_frameFactoryStartThread->requestExitAndWait();
    m_reprocessingFrameFactoryStartThread->requestExitAndWait();

    /* Wait for finishing pre-processing threads*/
    m_mainThread->requestExitAndWait();

    /* Create frame for the waiting request */
    while (m_requestWaitingList.size() > 0 || m_requestMgr->getServiceRequestCount() > 0) {
        CLOGV("Flush request. requestWaitingListSize %d serviceRequestCount %d",
                m_requestWaitingList.size(),
                m_requestMgr->getServiceRequestCount());

        m_createFrameFunc();
    }

    /* 1. wait for request callback done.*/
    int count = 0;

    while (true) {
        uint32_t requestCnt = 0;
        count++;

        requestCnt = m_requestMgr->getAllRequestCount();

        if (requestCnt == 0)
            break;

        CLOGD("getAllRequestCount size(%d) retry(%d)", requestCnt, count);
        usleep(50000);

        if (count == 200)
            break;
    }

    /* 2. wait for internal frame done.*/
    count = 0;

    while (true) {
        uint32_t processCount = 0;
        uint32_t captureProcessCount = 0;
        count++;
        processCount = m_getSizeFrameFromList(&m_processList, &m_processLock);
        captureProcessCount = m_getSizeFrameFromList(&m_captureProcessList, &m_captureProcessLock);

        if (processCount == 0
            && captureProcessCount == 0)
            break;

        CLOGD("processCount size(%d) captureProcessCount size(%d) retry(%d)",
            processCount, captureProcessCount, count);
        usleep(50000);

        if (count == 200)
            break;
    }

    /* Stop pipeline */
    for (int i = 0; i < FRAME_FACTORY_TYPE_MAX; i++) {
        if (m_frameFactory[i] != NULL) {
            frameFactory = m_frameFactory[i];

            for (int k = i + 1; k < FRAME_FACTORY_TYPE_MAX; k++) {
               if (frameFactory == m_frameFactory[k]) {
                   CLOGD("m_frameFactory index(%d) and index(%d) are same instance,"
                        " set index(%d) = NULL", i, k, k);
                   m_frameFactory[k] = NULL;
               }
            }

            ret = m_stopFrameFactory(m_frameFactory[i]);
            if (ret < 0)
                CLOGE("m_frameFactory[%d] stopPipes fail", i);

            CLOGD("m_frameFactory[%d] stopPipes", i);
        }
    }

    /* Wait for finishing pre-processing threads*/
    m_selectBayerThread->requestExitAndWait();

    /* Release pre-processing frame queues */
    m_shotDoneQ->release();
    m_selectBayerQ->release();

    /* Wait for finishing post-processing thread */
    m_previewStreamBayerThread->requestExitAndWait();
    m_previewStream3AAThread->requestExitAndWait();
    m_previewStreamISPThread->requestExitAndWait();
    m_previewStreamMCSCThread->requestExitAndWait();
#ifdef USE_VRA_GROUP
    m_previewStreamVRAThread->requestExitAndWait();
#endif
    m_internalFrameThread->requestExitAndWait();
    m_captureThread->requestExitAndWait();
    m_captureStreamThread->requestExitAndWait();
    if (m_duplicateBufferThread != NULL && m_duplicateBufferThread->isRunning()) {
        m_duplicateBufferThread->requestExitAndWait();
    }

    /* Release post-processing frame queues */
    for (int i = 0; i < MAX_PIPE_NUM; i++) {
        if (m_pipeFrameDoneQ[i] != NULL) {
            m_pipeFrameDoneQ[i]->release();
        }
    }
    m_internalFrameDoneQ->release();
    m_captureQ->release();
    m_reprocessingDoneQ->release();
    m_yuvCaptureDoneQ->release();
    m_duplicateBufferDoneQ->release();
#ifdef YUV_DUMP
    m_dumpFrameQ->release();
#endif

    m_captureSelector->release();

    /* put all internal buffers */
    for (int bufIndex = 0; bufIndex < m_fliteBufferMgr->getAllocatedBufferCount(); bufIndex++)
        ret = m_putBuffers(m_fliteBufferMgr, bufIndex);
    for (int bufIndex = 0; bufIndex < m_3aaBufferMgr->getAllocatedBufferCount(); bufIndex++)
        ret = m_putBuffers(m_3aaBufferMgr, bufIndex);
    for (int bufIndex = 0; bufIndex < m_ispBufferMgr->getAllocatedBufferCount(); bufIndex++)
        ret = m_putBuffers(m_ispBufferMgr, bufIndex);
    for (int bufIndex = 0; bufIndex < m_mcscBufferMgr->getAllocatedBufferCount(); bufIndex++)
        ret = m_putBuffers(m_mcscBufferMgr, bufIndex);
#ifdef USE_VRA_GROUP
    for (int bufIndex = 0; bufIndex < m_vraBufferMgr->getAllocatedBufferCount(); bufIndex++)
        ret = m_putBuffers(m_vraBufferMgr, bufIndex);
#endif
    for (int bufIndex = 0; bufIndex < m_ispReprocessingBufferMgr->getAllocatedBufferCount(); bufIndex++)
        ret = m_putBuffers(m_ispReprocessingBufferMgr, bufIndex);
    for (int bufIndex = 0; bufIndex < m_yuvCaptureBufferMgr->getAllocatedBufferCount(); bufIndex++)
        ret = m_putBuffers(m_yuvCaptureBufferMgr, bufIndex);
    for (int bufIndex = 0; bufIndex < m_thumbnailBufferMgr->getAllocatedBufferCount(); bufIndex++)
        ret = m_putBuffers(m_thumbnailBufferMgr, bufIndex);

    if (m_internalScpBufferMgr != NULL) {
        for (int bufIndex = 0; bufIndex < m_internalScpBufferMgr->getAllocatedBufferCount(); bufIndex++)
            ret = m_putBuffers(m_internalScpBufferMgr, bufIndex);
    }

#ifdef SUPPORT_DEPTH_MAP
    if (m_depthMapBufferMgr != NULL) {
        for (int bufIndex = 0; bufIndex < m_depthMapBufferMgr->getAllocatedBufferCount(); bufIndex++)
            ret = m_putBuffers(m_depthMapBufferMgr, bufIndex);
    }
#endif

    /* Clear frame list, because the frame number is initialized in startFrameFactoryThread. */
    ret = m_clearList(&m_processList, &m_processLock);
    if (ret < 0) {
        CLOGE("m_clearList(m_processList) failed [%d]", ret);
    }
    ret = m_clearList(&m_captureProcessList, &m_captureProcessLock);
    if (ret < 0) {
        CLOGE("m_clearList(m_captureProcessList) failed [%d]", ret);
    }

    /* 2. start */
    isRestarted = true;

    CLOGI("Internal restart stream OUT");
    return funcRet;
}


}; /* namespace android */
