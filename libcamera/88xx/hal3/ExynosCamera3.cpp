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
    CLOGI("INFO(%s):cameraId(%d), use_companion(%d)", __FUNCTION__, cameraId, m_use_companion);

    if (m_use_companion == true) {
        m_companionThread = new mainCameraThread(this, &ExynosCamera3::m_companionThreadFunc,
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
        m_eepromThread = new mainCameraThread(this, &ExynosCamera3::m_eepromThreadFunc,
                                                  "cameraeepromThread", PRIORITY_URGENT_DISPLAY);
        if (m_eepromThread != NULL) {
            m_eepromThread->run();
            CLOGD("DEBUG(%s): eepromThread starts", __FUNCTION__);
        } else {
            CLOGE("(%s): failed the m_eepromThread", __FUNCTION__);
        }
    }
#endif

    /* Create related classes */
    m_parameters = new ExynosCamera3Parameters(m_cameraId, m_use_companion);
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
        CLOGE("ERR(%s[%d])Create static meta data failed!!", __FUNCTION__, __LINE__);

    m_metadataConverter->setStaticInfo(cameraId, *info);

    m_streamManager->setYuvStreamMaxCount(m_parameters->getYuvStreamMaxNum());

    m_setFrameManager();

    m_setConfigInform();

    m_constructFrameFactory();

    /* Setup FrameFactory to RequestManager*/
    m_setupFrameFactoryToRequest();

    /* HACK : check capture stream */
    isCaptureConfig = false;
    isRestarted = false;

    isRecordingConfig = false;
    recordingEnabled = false;
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
    const char *intentName = NULL;
    const char *streamIDName = NULL;
    ExynosCamera3FrameFactory *factory = NULL;
    ExynosCamera3FrameFactory *factoryReprocessing = NULL;

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
        CLOGE("ERR(%s[%d]):FRAME_FACTORY_TYPE_CAPTURE_PREVIEW factory is NULL!!!!", __FUNCTION__, __LINE__);
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
        } else {
            m_requestMgr->setRequestsInfo(HAL_STREAM_ID_PREVIEW, factory);
            m_requestMgr->setRequestsInfo(HAL_STREAM_ID_VIDEO, factory);
            m_requestMgr->setRequestsInfo(HAL_STREAM_ID_CALLBACK, factory);
            m_requestMgr->setRequestsInfo(HAL_STREAM_ID_RAW, factory);
            m_requestMgr->setRequestsInfo(HAL_STREAM_ID_ZSL_OUTPUT, factory);

            m_requestMgr->setRequestsInfo(HAL_STREAM_ID_JPEG, factoryReprocessing);
            m_requestMgr->setRequestsInfo(HAL_STREAM_ID_ZSL_INPUT, factoryReprocessing);
        }
    } else {
        if(factory == NULL) {
            CLOGE("ERR(%s[%d]):FRAME_FACTORY_TYPE_CAPTURE_PREVIEW factory is NULL!!!!", __FUNCTION__, __LINE__);
        }
        if(factoryReprocessing == NULL) {
            CLOGE("ERR(%s[%d]):FRAME_FACTORY_TYPE_REPROCESSING factory is NULL!!!!", __FUNCTION__, __LINE__);
        }
    }

#endif

    return ret;
}

status_t ExynosCamera3::m_setStreamInfo(camera3_stream_configuration *streamList)
{
    int ret = OK;
    int id = 0;

    CLOGD("DEBUG(%s[%d]):In", __FUNCTION__, __LINE__);

    /* sanity check */
    if (streamList == NULL) {
        CLOGE("ERR(%s[%d]):NULL stream configuration", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    if (streamList->streams == NULL) {
        CLOGE("ERR(%s[%d]):NULL stream list", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    if (streamList->num_streams < 1) {
        CLOGE("ERR(%s): Bad number of streams requested: %d", __FUNCTION__, streamList->num_streams);
        return BAD_VALUE;
    }

    /* check input stream */
    camera3_stream_t *inputStream = NULL;
    for (size_t i = 0; i < streamList->num_streams; i++) {
        camera3_stream_t *newStream = streamList->streams[i];
        if (newStream == NULL) {
            CLOGE("ERR(%s[%d]):Stream index %zu was NULL", __FUNCTION__, __LINE__, i);
            return BAD_VALUE;
        }
        // for debug
        CLOGD("DEBUG(%s[%d]):Stream(%p), ID(%zu), type(%d), usage(%#x) format(%#x) w(%d),h(%d)",
            __FUNCTION__, __LINE__,
            newStream, i, newStream->stream_type, newStream->usage,
            newStream->format, newStream->width, newStream->height);

        if ((newStream->stream_type == CAMERA3_STREAM_INPUT) ||
            (newStream->stream_type == CAMERA3_STREAM_BIDIRECTIONAL)) {
            if (inputStream != NULL) {
                CLOGE("ERR(%s): Multiple input streams requested!", __FUNCTION__);
                return BAD_VALUE;
            }
            inputStream = newStream;
        }

        /* HACK : check capture stream */
        if (newStream->format == 0x21)
            isCaptureConfig = true;

        /* HACK : check recording stream */
        if ((newStream->format == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED)
            && (newStream->usage & GRALLOC_USAGE_HW_VIDEO_ENCODER)) {
            CLOGI("INFO(%s[%d]):recording stream checked", __FUNCTION__, __LINE__);
            isRecordingConfig = true;
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
                CLOGE("ERR(%s[%d]):getID failed id(%d)", __FUNCTION__, __LINE__, id);
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
                CLOGE("ERR(%s[%d]):Register stream failed %p", __FUNCTION__, __LINE__, newStream);
                return ret;
            }
        } else {
            /* Existing stream, reuse current stream */
            stream = static_cast<ExynosCameraStream*>(newStream->priv);
            stream->setRegisterStream(EXYNOS_STREAM::HAL_STREAM_STS_VALID);
        }
    }

    CLOGD("DEBUG(%s[%d]):out", __FUNCTION__, __LINE__);
    return ret;
}

status_t ExynosCamera3::m_enumStreamInfo(camera3_stream_t *stream)
{
    CLOGD("DEBUG(%s[%d]):In", __FUNCTION__, __LINE__);
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
        CLOGE("ERR(%s[%d]):stream is NULL.", __FUNCTION__, __LINE__);
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
        CLOGE("ERR(%s[%d]):Invalid stream_type %d",  __FUNCTION__, __LINE__, stream->stream_type);
        break;
    }

    switch (stream->stream_type) {
    case CAMERA3_STREAM_OUTPUT:
        // TODO: split this routine to function
        switch (stream->format) {
        case HAL_PIXEL_FORMAT_BLOB:
            CLOGD("DEBUG(%s[%d]):HAL_PIXEL_FORMAT_BLOB format(%#x) usage(%#x) stream_type(%#x)",
                __FUNCTION__, __LINE__, stream->format, stream->usage, stream->stream_type);
            id = HAL_STREAM_ID_JPEG;
            actualFormat = HAL_PIXEL_FORMAT_BLOB;
            planeCount = 1;
            outputPortId = 0;
            requestBuffer = m_exynosconfig->current->bufInfo.num_request_jpeg_buffers;
            break;
        case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
            if (stream->usage & GRALLOC_USAGE_HW_TEXTURE || stream->usage & GRALLOC_USAGE_HW_COMPOSER) {
                CLOGD("DEBUG(%s[%d]):GRALLOC_USAGE_HW_TEXTURE foramt(%#x) usage(%#x) stream_type(%#x)",
                    __FUNCTION__, __LINE__, stream->format, stream->usage, stream->stream_type);
                id = HAL_STREAM_ID_PREVIEW;
                actualFormat = HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M;
                planeCount = 2;
                outputPortId = m_streamManager->getYuvStreamCount();
                requestBuffer = m_exynosconfig->current->bufInfo.num_request_preview_buffers;
            } else if(stream->usage & GRALLOC_USAGE_HW_VIDEO_ENCODER) {
                CLOGD("DEBUG(%s[%d]):GRALLOC_USAGE_HW_VIDEO_ENCODER format(%#x) usage(%#x) stream_type(%#x)",
                    __FUNCTION__, __LINE__, stream->format, stream->usage, stream->stream_type);
                id = HAL_STREAM_ID_VIDEO;
                actualFormat = HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M; /* NV12M */
                planeCount = 2;
                outputPortId = m_streamManager->getYuvStreamCount();
                requestBuffer = m_exynosconfig->current->bufInfo.num_request_video_buffers;
            } else if(stream->usage & GRALLOC_USAGE_HW_CAMERA_ZSL) {
                CLOGD("DEBUG(%s[%d]):GRALLOC_USAGE_HW_CAMERA_ZSL format(%#x) usage(%#x) stream_type(%#x)",
                    __FUNCTION__, __LINE__, stream->format, stream->usage, stream->stream_type);
                id = HAL_STREAM_ID_ZSL_OUTPUT;
                actualFormat = HAL_PIXEL_FORMAT_RAW16;
                planeCount = 1;
                outputPortId = 0;
                requestBuffer = m_exynosconfig->current->bufInfo.num_request_raw_buffers;
            } else {
                CLOGE("ERR(%s[%d]):HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED unknown usage(%#x)"
                    " format(%#x) stream_type(%#x)",
                    __FUNCTION__, __LINE__,  stream->usage, stream->format, stream->stream_type);
                ret = BAD_VALUE;
                goto func_err;
                break;
            }
             break;
        case HAL_PIXEL_FORMAT_YCbCr_420_888:
            CLOGD("DEBUG(%s[%d]):HAL_PIXEL_FORMAT_YCbCr_420_888 format(%#x) usage(%#x) stream_type(%#x)",
                __FUNCTION__, __LINE__, stream->format, stream->usage, stream->stream_type);
            id = HAL_STREAM_ID_CALLBACK;
            actualFormat = HAL_PIXEL_FORMAT_YCrCb_420_SP;
            planeCount = 1;
            outputPortId = m_streamManager->getYuvStreamCount();
            requestBuffer = m_exynosconfig->current->bufInfo.num_request_callback_buffers;
            break;
        /* case HAL_PIXEL_FORMAT_RAW_SENSOR: */
        case HAL_PIXEL_FORMAT_RAW16:
            CLOGD("DEBUG(%s[%d]):HAL_PIXEL_FORMAT_RAW_XXX format(%#x) usage(%#x) stream_type(%#x)",
                __FUNCTION__, __LINE__, stream->format, stream->usage, stream->stream_type);
            id = HAL_STREAM_ID_RAW;
            actualFormat = HAL_PIXEL_FORMAT_RAW16;
            planeCount = 1;
            outputPortId = 0;
            requestBuffer = m_exynosconfig->current->bufInfo.num_request_raw_buffers;
            break;
        default:
            CLOGE("ERR(%s[%d]):Not supported image format(%#x) usage(%#x) stream_type(%#x)",
                __FUNCTION__, __LINE__, stream->format, stream->usage, stream->stream_type);
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
            CLOGD("DEBUG(%s[%d]):HAL_PIXEL_FORMAT_RAW_XXX format(%#x) usage(%#x) stream_type(%#x)",
                __FUNCTION__, __LINE__, stream->format, stream->usage, stream->stream_type);
            id = HAL_STREAM_ID_RAW;
            actualFormat = HAL_PIXEL_FORMAT_RAW16;
            planeCount = 1;
            requestBuffer = m_exynosconfig->current->bufInfo.num_request_raw_buffers;
            break;
        case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
            if (stream->usage & GRALLOC_USAGE_HW_CAMERA_ZSL) {
                CLOGD("DEBUG(%s[%d]):GRALLOC_USAGE_HW_CAMERA_ZSL foramt(%#x) usage(%#x) stream_type(%#x)",
                    __FUNCTION__, __LINE__, stream->format, stream->usage, stream->stream_type);
                id = HAL_STREAM_ID_ZSL_INPUT;
                actualFormat = HAL_PIXEL_FORMAT_RAW16;
                planeCount = 1;
                requestBuffer = m_exynosconfig->current->bufInfo.num_request_raw_buffers;
                break;
            } else {
                CLOGE("ERR(%s[%d]):HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED unknown usage(%#x)"
                    " format(%#x) stream_type(%#x)",
                    __FUNCTION__, __LINE__,  stream->usage, stream->format, stream->stream_type);
                ret = BAD_VALUE;
                goto func_err;
            }
            break;
        default:
            CLOGE("ERR(%s[%d]):Not supported image format(%#x) usage(%#x) stream_type(%#x)",
                __FUNCTION__, __LINE__, stream->format, stream->usage, stream->stream_type);
            goto func_err;
        }
        break;
    default:
        CLOGE("ERR(%s[%d]):Unknown stream_type(%#x) format(%#x) usage(%#x)",
            __FUNCTION__, __LINE__, stream->stream_type, stream->format, stream->usage);
        ret = BAD_VALUE;
        goto func_err;
    }

    newStream = m_streamManager->createStream(id, stream);
    if (newStream == NULL) {
        CLOGE("ERR(%s[%d]):createStream is NULL id(%d)", __FUNCTION__, __LINE__, id);
        goto func_err;
    }

    newStream->setRegisterStream(registerStream);
    newStream->setRegisterBuffer(registerBuffer);
    newStream->setFormat(actualFormat);
    newStream->setPlaneCount(planeCount);
    newStream->setOutputPortId(outputPortId);
    newStream->setRequestBuffer(requestBuffer);

func_err:
    CLOGD("DEBUG(%s[%d]):Out", __FUNCTION__, __LINE__);

    return ret;

}

void ExynosCamera3::m_createManagers(void)
{
    if (!m_streamManager) {
        m_streamManager = new ExynosCameraStreamManager(m_cameraId);
        CLOGD("DEBUG(%s):Stream Manager created", __FUNCTION__);
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
    m_mainThread = new mainCameraThread(this, &ExynosCamera3::m_mainThreadFunc, "m_mainThreadFunc");
    CLOGD("DEBUG(%s):Main thread created", __FUNCTION__);

    /* m_previewStreamXXXThread is for seperated frameDone each own handler */
    m_previewStreamBayerThread = new mainCameraThread(this, &ExynosCamera3::m_previewStreamBayerPipeThreadFunc, "PreviewBayerThread");
    CLOGD("DEBUG(%s):Bayer Preview stream thread created", __FUNCTION__);

    m_previewStream3AAThread = new mainCameraThread(this, &ExynosCamera3::m_previewStream3AAPipeThreadFunc, "Preview3AAThread");
    CLOGD("DEBUG(%s):3AA Preview stream thread created", __FUNCTION__);

    m_previewStreamISPThread = new mainCameraThread(this, &ExynosCamera3::m_previewStreamISPPipeThreadFunc, "PreviewISPThread");
    CLOGD("DEBUG(%s):ISP Preview stream thread created", __FUNCTION__);

    m_previewStreamMCSCThread = new mainCameraThread(this, &ExynosCamera3::m_previewStreamMCSCPipeThreadFunc, "PreviewISPThread");
    CLOGD("DEBUG(%s):MCSC Preview stream thread created", __FUNCTION__);

    m_selectBayerThread = new mainCameraThread(this, &ExynosCamera3::m_selectBayerThreadFunc, "SelectBayerThreadFunc");
    CLOGD("DEBUG(%s):SelectBayerThread created", __FUNCTION__);

    m_captureThread = new mainCameraThread(this, &ExynosCamera3::m_captureThreadFunc, "CaptureThreadFunc");
    CLOGD("DEBUG(%s):CaptureThread created", __FUNCTION__);

    m_captureStreamThread = new mainCameraThread(this, &ExynosCamera3::m_captureStreamThreadFunc, "CaptureThread");
    CLOGD("DEBUG(%s):Capture stream thread created", __FUNCTION__);

    m_setBuffersThread = new mainCameraThread(this, &ExynosCamera3::m_setBuffersThreadFunc, "setBuffersThread");
    CLOGD("DEBUG(%s):Buffer allocation thread created", __FUNCTION__);

    m_duplicateBufferThread = new mainCameraThread(this, &ExynosCamera3::m_duplicateBufferThreadFunc, "DuplicateThread");
    CLOGD("Duplicate buffer thread created");

    m_framefactoryCreateThread = new mainCameraThread(this, &ExynosCamera3::m_frameFactoryCreateThreadFunc, "FrameFactoryCreateThread");
    CLOGD("DEBUG(%s):FrameFactoryCreateThread created", __FUNCTION__);

    m_reprocessingFrameFactoryStartThread = new mainCameraThread(this, &ExynosCamera3::m_reprocessingFrameFactoryStartThreadFunc, "m_reprocessingFrameFactoryStartThread");
    CLOGD("DEBUG(%s):m_reprocessingFrameFactoryStartThread created", __FUNCTION__);

    m_startPictureBufferThread = new mainCameraThread(this, &ExynosCamera3::m_startPictureBufferThreadFunc, "startPictureBufferThread");
    CLOGD("DEBUG(%s):startPictureBufferThread created", __FUNCTION__);

    m_frameFactoryStartThread = new mainCameraThread(this, &ExynosCamera3::m_frameFactoryStartThreadFunc, "FrameFactoryStartThread");
    CLOGD("DEBUG(%s):FrameFactoryStartThread created", __FUNCTION__);

    m_internalFrameThread = new mainCameraThread(this, &ExynosCamera3::m_internalFrameThreadFunc, "InternalFrameThread");
    CLOGD("DEBUG(%s):Internal Frame Handler Thread created", __FUNCTION__);

    m_monitorThread = new mainCameraThread(this, &ExynosCamera3::m_monitorThreadFunc, "MonitorThread");
    CLOGD("DEBUG(%s):MonitorThread created", __FUNCTION__);

#ifdef YUV_DUMP
    m_dumpThread = new mainCameraThread(this, &ExynosCamera3::m_dumpThreadFunc, "m_dumpThreadFunc");
    CLOGD("DEBUG(%s):DumpThread created", __FUNCTION__);
#endif
}

ExynosCamera3::~ExynosCamera3()
{
    this->release();
}

void ExynosCamera3::release()
{
    CLOGI("INFO(%s[%d]):-IN-", __FUNCTION__, __LINE__);
    int ret = 0;

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

#if 1
    if (m_parameters != NULL) {
        delete m_parameters;
        m_parameters = NULL;
    }

    if (m_metadataConverter != NULL) {
        delete m_parameters;
        m_parameters = NULL;
    }

    if (m_captureSelector != NULL) {
        delete m_captureSelector;
        m_captureSelector = NULL;
    }

    if (m_captureZslSelector != NULL) {
        delete m_captureZslSelector;
        m_captureZslSelector = NULL;
    }
#endif

    // TODO: clean up
    // m_resultBufferVectorSet
    // m_processList
    // m_postProcessList
    // m_pipeFrameDoneQ
    CLOGI("INFO(%s[%d]):-OUT-", __FUNCTION__, __LINE__);
}

status_t ExynosCamera3::initilizeDevice(const camera3_callback_ops *callbackOps)
{
    status_t ret = NO_ERROR;
    CLOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);

    /* set callback ops */
    m_requestMgr->setCallbackOps(callbackOps);

#ifdef SUPPORT_DEPTH_MAP
    m_parameters->checkUseDepthMap();
#endif

    if (m_parameters->isReprocessing() == true) {
        if (m_captureSelector == NULL) {
            m_captureSelector = new ExynosCameraFrameSelector(m_parameters, m_fliteBufferMgr, m_frameMgr);
            ret = m_captureSelector->setFrameHoldCount(REPROCESSING_BAYER_HOLD_COUNT);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):m_captureSelector setFrameHoldCount(%d) is fail",
                    __FUNCTION__, __LINE__, REPROCESSING_BAYER_HOLD_COUNT);
            }
        }

        if (m_captureZslSelector == NULL) {
            m_captureZslSelector = new ExynosCameraFrameSelector(m_parameters, m_bayerBufferMgr, m_frameMgr);
            ret = m_captureZslSelector->setFrameHoldCount(REPROCESSING_BAYER_HOLD_COUNT);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):m_captureZslSelector setFrameHoldCount(%d) is fail",
                    __FUNCTION__, __LINE__, REPROCESSING_BAYER_HOLD_COUNT);
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
    CLOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);

    m_setBuffersThread->requestExitAndWait();
    m_framefactoryCreateThread->requestExitAndWait();
    m_monitorThread->requestExitAndWait();
    m_previewStreamBayerThread->requestExitAndWait();
    m_previewStream3AAThread->requestExitAndWait();
    m_previewStreamISPThread->requestExitAndWait();
    m_previewStreamMCSCThread->requestExitAndWait();
    m_internalFrameThread->requestExitAndWait();
    m_mainThread->requestExitAndWait();
    m_selectBayerThread->requestExitAndWait();

    if (m_shotDoneQ != NULL)
        m_shotDoneQ->release();

    if (m_flushFlag == false)
        flush();

    ret = m_clearList(&m_processList, &m_processLock);
    if (ret < 0) {
        CLOGE("ERR(%s):m_clearList fail", __FUNCTION__);
        return ret;
    }

    /* initialize frameQ */
    for (int i = 0; i < MAX_PIPE_NUM; i++) {
        if (m_pipeFrameDoneQ[i] != NULL) {
            m_pipeFrameDoneQ[i]->release();
            m_pipeFrameDoneQ[i] = NULL;
        }
    }
    if(m_duplicateBufferDoneQ != NULL) {
        m_duplicateBufferDoneQ->release();
    }
    m_yuvCaptureDoneQ->release();
    m_reprocessingDoneQ->release();

    /* internal frame */
    m_internalFrameDoneQ->release();

#ifdef YUV_DUMP
    m_dumpFrameQ->release();
#endif

    m_frameMgr->stop();
    m_frameMgr->deleteAllFrame();


    return ret;
}

status_t ExynosCamera3::construct_default_request_settings(camera_metadata_t **request, int type)
{
    Mutex::Autolock l(m_requestLock);

    CLOGD("DEBUG(%s[%d]):Type %d", __FUNCTION__, __LINE__, type);
    if ((type < 0) || (type >= CAMERA3_TEMPLATE_COUNT)) {
        CLOGE("%s: Unknown request settings template: %d", __FUNCTION__, type);
        return -ENODEV;
    }

    m_requestMgr->constructDefaultRequestSettings(type, request);

    CLOGI("INFO(%s[%d]):out", __FUNCTION__, __LINE__);
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
    bool updateSize = true;
    int currentConfigMode = m_parameters->getConfigMode();

    CLOGD("DEBUG(%s[%d]):In", __FUNCTION__, __LINE__);

    /* sanity check for stream_list */
    if (stream_list == NULL) {
        CLOGE("ERR(%s[%d]):NULL stream configuration", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    if (stream_list->streams == NULL) {
        CLOGE("ERR(%s[%d]):NULL stream list", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    if (stream_list->num_streams < 1) {
        CLOGE("ERR(%s[%d]):Bad number of streams requested: %d",
            __FUNCTION__, __LINE__, stream_list->num_streams);
        return BAD_VALUE;
    }

    if (stream_list->operation_mode == CAMERA3_STREAM_CONFIGURATION_CONSTRAINED_HIGH_SPEED_MODE) {
        CLOGI("INFO(%s[%d]):High speed mode is configured. StreamCount %d",
                __FUNCTION__, __LINE__, stream_list->num_streams);
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

    /* start allocation of internal buffers */
    if (m_checkConfigStream == false) {
        m_setBuffersThread->run(PRIORITY_DEFAULT);
    }

    ret = m_streamManager->setConfig(m_exynosconfig);
    if (ret) {
        CLOGE("ERR(%s[%d]):setMaxBuffers() failed!!", __FUNCTION__, __LINE__);
        return ret;
    }
    ret = m_setStreamInfo(stream_list);
    if (ret) {
        CLOGE("ERR(%s[%d]):setStreams() failed!!", __FUNCTION__, __LINE__);
        return ret;
    }

    /* flush request Mgr */
    m_requestMgr->flush();

    /* HACK :: restart frame factory */
    if (m_checkConfigStream == true
        || ((isCaptureConfig == true) && (stream_list->num_streams == 1))
        || ((isRecordingConfig == true) && (recordingEnabled == false))
        || ((isRecordingConfig == false) && (recordingEnabled == true))) {
        CLOGI("INFO(%s[%d]):restart frame factory isCaptureConfig(%d),"
            " isRecordingConfig(%d), stream_list->num_streams(%d)",
            __FUNCTION__, __LINE__, isCaptureConfig, isRecordingConfig, stream_list->num_streams);

        isCaptureConfig = false;
        /* In case of preview with Recording, enter this block even if not restart */
        if (m_checkConfigStream == true)
            isRestarted = true;

        recordingEnabled = false;
        isRecordingConfig = false;

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

        /* Pull all internal buffers */
        for (int bufIndex = 0; bufIndex < m_fliteBufferMgr->getAllocatedBufferCount(); bufIndex++)
            ret = m_putBuffers(m_fliteBufferMgr, bufIndex);
        for (int bufIndex = 0; bufIndex < m_3aaBufferMgr->getAllocatedBufferCount(); bufIndex++)
            ret = m_putBuffers(m_3aaBufferMgr, bufIndex);
        if(m_internalScpBufferMgr != NULL) {
            for (int bufIndex = 0; bufIndex < m_internalScpBufferMgr->getAllocatedBufferCount(); bufIndex++)
                ret = m_putBuffers(m_internalScpBufferMgr, bufIndex);
        }

        if (currentConfigMode != m_parameters->getConfigMode()) {
            CLOGI("INFO(%s[%d]):currentConfigMode %d -> newConfigMode %d. Reallocate the internal buffers.",
                __FUNCTION__, __LINE__, currentConfigMode, m_parameters->getConfigMode());
            ret = m_releaseBuffers();
            if (ret != NO_ERROR)
                CLOGE("ERR(%s[%d]):Failed to releaseBuffers. ret %d",
                        __FUNCTION__, __LINE__, ret);

            m_setBuffersThread->run(PRIORITY_DEFAULT);
            m_startPictureBufferThread->run(PRIORITY_DEFAULT);
        }
    }

    /* clear previous settings */
    ret = m_requestMgr->clearPrevRequest();
    if (ret) {
        CLOGE("ERR(%s[%d]):clearPrevRequest() failed!!", __FUNCTION__, __LINE__);
        return ret;
    }

    ret = m_requestMgr->clearPrevShot();
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):clearPrevShot() failed!! status(%d)", __FUNCTION__, __LINE__, ret);
    }

    /* check flag update size */
    updateSize = m_streamManager->findStream(HAL_STREAM_ID_PREVIEW);
#ifdef USE_BDS_2_0_480P_YUV
    m_parameters->clearYuvSizeSetupFlag();
#endif

#ifdef HAL3_YUVSIZE_BASED_BDS
    /* Reset all the YUV output sizes to smallest one
       To make sure the prior output setting do not affect current session.
    */
    ret = m_parameters->initYuvSizes();
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):clearYuvSizes() failed!! status(%d)", __FUNCTION__, __LINE__, ret);
    }
#endif

    /* To find the minimum sized YUV stream in stream configuration, reset the old data */
    (void)m_parameters->resetMinYuvSize();

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

        CLOGI("INFO(%s[%d]):list_index %zu streamId %d", __FUNCTION__, __LINE__, i, id);
        CLOGD("DEBUG(%s[%d]):stream_type %d", __FUNCTION__, __LINE__, newStream->stream_type);
        CLOGD("DEBUG(%s[%d]):size %dx%d", __FUNCTION__, __LINE__, width, height);
        CLOGD("DEBUG(%s[%d]):format %x", __FUNCTION__, __LINE__, newStream->format);
        CLOGD("DEBUG(%s[%d]):usage %x", __FUNCTION__, __LINE__, newStream->usage);
        CLOGD("DEBUG(%s[%d]):max_buffers %d", __FUNCTION__, __LINE__, newStream->max_buffers);

        privStreamInfo->getRegisterStream(&registerStreamState);
        privStreamInfo->getRegisterBuffer(&registerbufferState);
        privStreamInfo->getPlaneCount(&streamPlaneCount);
        privStreamInfo->getFormat(&streamPixelFormat);

        if (registerStreamState == EXYNOS_STREAM::HAL_STREAM_STS_INVALID) {
            CLOGE("ERR(%s[%d]):Invalid stream index %zu id %d",
                    __FUNCTION__, __LINE__, i, id);
            ret = BAD_VALUE;
            break;
        }
        if (registerbufferState != EXYNOS_STREAM::HAL_STREAM_STS_UNREGISTERED) {
            CLOGE("ERR(%s[%d]):privStreamInfo->registerBuffer state error!!",
                    __FUNCTION__, __LINE__);
            return BAD_VALUE;
        }

        CLOGD("DEBUG(%s[%d]):registerStream %d registerbuffer %d",
            __FUNCTION__, __LINE__, registerStreamState, registerbufferState);

        if ((registerStreamState == EXYNOS_STREAM::HAL_STREAM_STS_VALID) &&
            (registerbufferState == EXYNOS_STREAM::HAL_STREAM_STS_UNREGISTERED)) {
            ExynosCameraBufferManager *bufferMgr = NULL;
            switch (id % HAL_STREAM_ID_MAX) {
            case HAL_STREAM_ID_RAW:
                CLOGD("DEBUG(%s[%d]):Create buffer manager(RAW)", __FUNCTION__, __LINE__);
                ret =  m_createServiceBufferManager(&m_bayerBufferMgr, "RAW_STREAM_BUF");
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):m_createBufferManager() failed!!", __FUNCTION__, __LINE__);
                    return ret;
                }

                planeCount = streamPlaneCount + 1;
                planeSize[0] = width * height * 2;

                CLOGD("DEBUG(%s[%d]):planeCount %d+1",
                        __FUNCTION__, __LINE__, streamPlaneCount);
                CLOGD("DEBUG(%s[%d]):planeSize[0] %d",
                        __FUNCTION__, __LINE__, planeSize[0]);
                CLOGD("DEBUG(%s[%d]):bytesPerLine[0] %d",
                        __FUNCTION__, __LINE__, bytesPerLine[0]);

                /* set startIndex as the next internal buffer index */
                startIndex = m_exynosconfig->current->bufInfo.num_sensor_buffers;
                maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers - startIndex;

                m_bayerBufferMgr->setAllocator(newStream);
                m_allocBuffers(m_bayerBufferMgr, planeCount, planeSize, bytesPerLine, startIndex, maxBufferCount, true, false);

                privStreamInfo->setRegisterBuffer(EXYNOS_STREAM::HAL_STREAM_STS_REGISTERED);
                /* bayer buffer is managed specially */
                /* privStreamInfo->setBufferManager(m_bayerBufferMgr); */
                CLOGD("DEBUG(%s[%d]):m_bayerBufferMgr - %p",
                        __FUNCTION__, __LINE__, m_bayerBufferMgr);
                break;
            case HAL_STREAM_ID_ZSL_OUTPUT:
                ALOGD("DEBUG(%s[%d]):Create buffer manager(ZSL)", __FUNCTION__, __LINE__);
                ret = m_createServiceBufferManager(&m_zslBufferMgr, "ZSL_STREAM_BUF");
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):m_createBufferManager() failed!!", __FUNCTION__, __LINE__);
                    return ret;
                }

                planeCount = streamPlaneCount + 1;
                planeSize[0] = width * height * 2;

                ALOGD("DEBUG(%s[%d]):planeCount %d+1",
                        __FUNCTION__, __LINE__, streamPlaneCount);
                ALOGD("DEBUG(%s[%d]):planeSize[0] %d",
                        __FUNCTION__, __LINE__, planeSize[0]);
                ALOGD("DEBUG(%s[%d]):bytesPerLine[0] %d",
                        __FUNCTION__, __LINE__, bytesPerLine[0]);

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
                    CLOGE("ERR(%s[%d]): ZSL input is not supported, but streamID [%d] is specified.",
                            __FUNCTION__, __LINE__, id);
                    return INVALID_OPERATION;
                }

                planeCount = streamPlaneCount + 1;
                planeSize[0] = width * height * 2;

                ALOGD("DEBUG(%s[%d]):planeCount %d+1",
                        __FUNCTION__, __LINE__, streamPlaneCount);
                ALOGD("DEBUG(%s[%d]):planeSize[0] %d",
                        __FUNCTION__, __LINE__, planeSize[0]);
                ALOGD("DEBUG(%s[%d]):bytesPerLine[0] %d",
                        __FUNCTION__, __LINE__, bytesPerLine[0]);

                privStreamInfo->setRegisterBuffer(EXYNOS_STREAM::HAL_STREAM_STS_REGISTERED);
                break;
            case HAL_STREAM_ID_PREVIEW:
                ret = privStreamInfo->getOutputPortId(&outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):Failed to getOutputPortId for PREVIEW stream",
                            __FUNCTION__, __LINE__);
                    return ret;
                }

                ret = m_parameters->checkYuvSize(width, height, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):Failed to checkYuvSize for PREVIEW stream. size %dx%d outputPortId %d",
                            __FUNCTION__, __LINE__, width, height, outputPortId);
                    return ret;
                }

                ret = m_parameters->checkYuvFormat(streamPixelFormat, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):Failed to checkYuvFormat for PREVIEW stream. format %x outputPortId %d",
                            __FUNCTION__, __LINE__, streamPixelFormat, outputPortId);
                    return ret;
                }

                maxBufferCount = m_exynosconfig->current->bufInfo.num_preview_buffers;
                ret = m_parameters->setYuvBufferCount(maxBufferCount, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):Failed to setYuvBufferCount for PREVIEW stream. maxBufferCount %d outputPortId %d",
                            __FUNCTION__, __LINE__, maxBufferCount, outputPortId);
                    return ret;
                }

                CLOGD("DEBUG(%s[%d]):Create buffer manager(PREVIEW)", __FUNCTION__, __LINE__);
                ret =  m_createServiceBufferManager(&bufferMgr, "PREVIEW_STREAM_BUF");
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):m_createBufferManager() failed!!", __FUNCTION__, __LINE__);
                    return ret;
                }

                planeCount = streamPlaneCount + 1;
                planeSize[0] = width * height;
                planeSize[1] = width * height / 2;

                CLOGD("DEBUG(%s[%d]):planeCount %d+1",
                        __FUNCTION__, __LINE__, streamPlaneCount);
                CLOGD("DEBUG(%s[%d]):planeSize[0] %d planeSize[1] %d",
                        __FUNCTION__, __LINE__, planeSize[0], planeSize[1]);
                CLOGD("DEBUG(%s[%d]):bytesPerLine[0] %d",
                        __FUNCTION__, __LINE__, bytesPerLine[0]);

                bufferMgr->setAllocator(newStream);
                m_allocBuffers(bufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, true, false);

                privStreamInfo->setRegisterBuffer(EXYNOS_STREAM::HAL_STREAM_STS_REGISTERED);
                privStreamInfo->setBufferManager(bufferMgr);
                CLOGD("DEBUG(%s[%d]):previewBufferMgr - %p",
                        __FUNCTION__, __LINE__, bufferMgr);
                break;
            case HAL_STREAM_ID_VIDEO:
                ret = privStreamInfo->getOutputPortId(&outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):Failed to getOutputPortId for VIDEO stream",
                            __FUNCTION__, __LINE__);
                    return ret;
                }

                ret = m_parameters->checkYuvSize(width, height, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):Failed to checkYuvSize for VIDEO stream. size %dx%d outputPortId %d",
                            __FUNCTION__, __LINE__, width, height, outputPortId);
                    return ret;
                }

                ret = m_parameters->checkYuvFormat(streamPixelFormat, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):Failed to checkYuvFormat for VIDEO stream. format %x outputPortId %d",
                            __FUNCTION__, __LINE__, streamPixelFormat, outputPortId);
                    return ret;
                }

                maxBufferCount = m_exynosconfig->current->bufInfo.num_recording_buffers;
                ret = m_parameters->setYuvBufferCount(maxBufferCount, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):Failed to setYuvBufferCount for VIDEO stream. maxBufferCount %d outputPortId %d",
                            __FUNCTION__, __LINE__, maxBufferCount, outputPortId);
                    return ret;
                }

                CLOGD("DEBUG(%s[%d]):Create buffer manager(VIDEO)", __FUNCTION__, __LINE__);
                ret =  m_createServiceBufferManager(&bufferMgr, "RECORDING_STREAM_BUF");
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):m_createBufferManager() failed!!", __FUNCTION__, __LINE__);
                    return ret;
                }

                planeCount = streamPlaneCount + 1;
                planeSize[0] = width * height;
                planeSize[1] = width * height / 2;

                CLOGD("DEBUG(%s[%d]):planeCount %d+1",
                        __FUNCTION__, __LINE__, streamPlaneCount);
                CLOGD("DEBUG(%s[%d]):planeSize[0] %d planeSize[1] %d",
                        __FUNCTION__, __LINE__, planeSize[0], planeSize[1]);
                CLOGD("DEBUG(%s[%d]):bytesPerLine[0] %d",
                        __FUNCTION__, __LINE__, bytesPerLine[0]);

                bufferMgr->setAllocator(newStream);
                m_allocBuffers(bufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, true, false);

                privStreamInfo->setRegisterBuffer(EXYNOS_STREAM::HAL_STREAM_STS_REGISTERED);
                privStreamInfo->setBufferManager(bufferMgr);
                CLOGD("DEBUG(%s[%d]):recBufferMgr - %p",
                        __FUNCTION__, __LINE__, bufferMgr);
                break;
            case HAL_STREAM_ID_JPEG:
                ret = m_parameters->checkPictureSize(width, height);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):Failed to checkPictureSize for JPEG stream. size %dx%d",
                            __FUNCTION__, __LINE__, width, height);
                    return ret;
                }

                maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;

                CLOGD("DEBUG(%s[%d]):Create buffer manager(JPEG)", __FUNCTION__, __LINE__);
                ret =  m_createServiceBufferManager(&bufferMgr, "JPEG_STREAM_BUF");
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):m_createBufferManager() failed!!", __FUNCTION__, __LINE__);
                    return ret;
                }

                planeCount = streamPlaneCount;
                planeSize[0] = width * height * 2;

                CLOGD("DEBUG(%s[%d]):planeCount %d",
                        __FUNCTION__, __LINE__, streamPlaneCount);
                CLOGD("DEBUG(%s[%d]):planeSize[0] %d",
                        __FUNCTION__, __LINE__, planeSize[0]);
                CLOGD("DEBUG(%s[%d]):bytesPerLine[0] %d",
                        __FUNCTION__, __LINE__, bytesPerLine[0]);

                bufferMgr->setAllocator(newStream);
                m_allocBuffers(bufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, false, false);

                privStreamInfo->setRegisterBuffer(EXYNOS_STREAM::HAL_STREAM_STS_REGISTERED);
                privStreamInfo->setBufferManager(bufferMgr);
                CLOGD("DEBUG(%s[%d]):jpegBufferMgr - %p",
                        __FUNCTION__, __LINE__, bufferMgr);

                break;
            case HAL_STREAM_ID_CALLBACK:
                ret = privStreamInfo->getOutputPortId(&outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):Failed to getOutputPortId for CALLBACK stream",
                            __FUNCTION__, __LINE__);
                    return ret;
                }

                ret = m_parameters->checkYuvSize(width, height, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):Failed to checkYuvSize for CALLBACK stream. size %dx%d outputPortId %d",
                            __FUNCTION__, __LINE__, width, height, outputPortId);
                    return ret;
                }

                ret = m_parameters->checkYuvFormat(streamPixelFormat, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):Failed to checkYuvFormat for CALLBACK stream. format %x outputPortId %d",
                            __FUNCTION__, __LINE__, streamPixelFormat, outputPortId);
                    return ret;
                }

                maxBufferCount = m_exynosconfig->current->bufInfo.num_preview_cb_buffers;
                ret = m_parameters->setYuvBufferCount(maxBufferCount, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):Failed to setYuvBufferCount for CALLBACK stream. maxBufferCount %d outputPortId %d",
                            __FUNCTION__, __LINE__, maxBufferCount, outputPortId);
                    return ret;
                }

                CLOGD("DEBUG(%s[%d]):Create buffer manager(PREVIEW_CB)", __FUNCTION__, __LINE__);
                ret =  m_createServiceBufferManager(&bufferMgr, "PREVIEW_CB_STREAM_BUF");
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):m_createBufferManager() failed!!", __FUNCTION__, __LINE__);
                    return ret;
                }

                planeCount = streamPlaneCount + 1;
                planeSize[0] = (width * height * 3) / 2;

                CLOGD("DEBUG(%s[%d]):planeCount %d",
                        __FUNCTION__, __LINE__, streamPlaneCount);
                CLOGD("DEBUG(%s[%d]):planeSize[0] %d",
                        __FUNCTION__, __LINE__, planeSize[0]);
                CLOGD("DEBUG(%s[%d]):bytesPerLine[0] %d",
                        __FUNCTION__, __LINE__, bytesPerLine[0]);

                bufferMgr->setAllocator(newStream);
                m_allocBuffers(bufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, true, false);

                privStreamInfo->setRegisterBuffer(EXYNOS_STREAM::HAL_STREAM_STS_REGISTERED);
                privStreamInfo->setBufferManager(bufferMgr);
                CLOGD("DEBUG(%s[%d]):preivewCallbackBufferMgr - %p", __FUNCTION__, __LINE__, bufferMgr);
                break;
            default:
                CLOGE("ERR(%s[%d]):privStreamInfo->id is invalid !! id(%d)", __FUNCTION__, __LINE__, id);
                ret = BAD_VALUE;
                break;
            }
        }
    }

    m_checkConfigStream = true;

    CLOGD("DEBUG(%s[%d]):Out", __FUNCTION__, __LINE__);
    return ret;
}

status_t ExynosCamera3::registerStreamBuffers(const camera3_stream_buffer_set_t *buffer_set)
{
    /* deprecated function */
    if (buffer_set == NULL) {
        CLOGE("ERR(%s[%d]):buffer_set is NULL", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

status_t ExynosCamera3::processCaptureRequest(camera3_capture_request *request)
{
    Mutex::Autolock l(m_requestLock);
    ExynosCameraBuffer *buffer = NULL;
    ExynosCameraFrame *newFrame = NULL;
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

    CLOGV("DEBUG(%s[%d]):Capture request (%d) #out(%d)",
        __FUNCTION__, __LINE__, request->frame_number, request->num_output_buffers);

    /* 1. Wait for allocation completion of internal buffers and creation of frame factory */
    m_setBuffersThread->join();
    m_framefactoryCreateThread->join();

    /* 2. Check the validation of request */
    if (request == NULL) {
        CLOGE("ERR(%s[%d]):NULL request!", __FUNCTION__, __LINE__);
        ret = BAD_VALUE;
        goto req_err;
    }

#if 0
    // For Fence FD debugging
    if (request->input_buffer != NULL){
        const camera3_stream_buffer* pStreamBuf = request->input_buffer;
        CLOGD("DEBUG(%s[%d]):[Input] acqureFence[%d], releaseFence[%d], Status[%d], Buffer[%p]",
            __FUNCTION__, __LINE__, request->num_output_buffers,
            pStreamBuf->acquire_fence, pStreamBuf->release_fence, pStreamBuf->status, pStreamBuf->buffer);
    }
    for(int i = 0; i < request->num_output_buffers; i++) {
        const camera3_stream_buffer* pStreamBuf = &(request->output_buffers[i]);
        CLOGD("DEBUG(%s[%d]):[Output] Index[%d/%d] acqureFence[%d], releaseFence[%d], Status[%d], Buffer[%p]",
            __FUNCTION__, __LINE__, i, request->num_output_buffers,
            pStreamBuf->acquire_fence, pStreamBuf->release_fence, pStreamBuf->status, pStreamBuf->buffer);
    }
#endif

    /* m_streamManager->dumpCurrentStreamList(); */

    /* 3. Check NULL for service metadata */
    if ((request->settings == NULL) && (m_requestMgr->isPrevRequest())) {
        CLOGE("ERR(%s[%d]):Request%d: NULL and no prev request!!", __FUNCTION__, __LINE__, request->frame_number);
        ret = BAD_VALUE;
        goto req_err;
    }

    /* 4. Check the registeration of input buffer on stream */
    if (request->input_buffer != NULL){
        stream = request->input_buffer->stream;
        streamInfo = static_cast<ExynosCameraStream*>(stream->priv);
        streamInfo->getRegisterBuffer(&registerBuffer);

        if (registerBuffer != EXYNOS_STREAM::HAL_STREAM_STS_REGISTERED) {
            CLOGE("ERR(%s[%d]):Request %d: Input buffer not from input stream!",
                    __FUNCTION__, __LINE__, request->frame_number);
            CLOGE("ERR(%s[%d]):Bad Request %p, type %d format %x",
                    __FUNCTION__, __LINE__,
                    request->input_buffer->stream,
                    request->input_buffer->stream->stream_type,
                    request->input_buffer->stream->format);
            ret = BAD_VALUE;
            goto req_err;
        }
    }

    /* 5. Check the output buffer count */
    if ((request->num_output_buffers < 1) || (request->output_buffers == NULL)) {
        CLOGE("ERR(%s[%d]):Request %d: No output buffers provided!",
                __FUNCTION__, __LINE__, request->frame_number);
        ret = BAD_VALUE;
        goto req_err;
    }

    /* 6. Store request settings
     * Caution : All information must be copied into internal data structure
     * before receiving another request from service
     */
    ret = m_pushRequest(request);
    ret = m_registerStreamBuffers(request);

    /* 7. Calculate the timeout value for processing request based on actual fps setting */
    meta = request->settings;
    if (request->settings != NULL && meta.exists(ANDROID_CONTROL_AE_TARGET_FPS_RANGE)) {
        uint32_t minFps = 0, maxFps = 0;
        entry = meta.find(ANDROID_CONTROL_AE_TARGET_FPS_RANGE);
        minFps = entry.data.i32[0];
        maxFps = entry.data.i32[1];
        m_parameters->checkPreviewFpsRange(minFps, maxFps);
        timeOutNs = (1000 / ((minFps == 0) ? 15 : minFps)) * 1000000;
    }

    /* 8. Process initial requests for preparing the stream */
    if (request->frame_number == 0 || m_flushFlag == true || isRestarted == true) {
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

        CLOGD("DEBUG(%s[%d]):Start FrameFactory requestKey(%d) m_flushFlag(%d/%d)",
                __FUNCTION__, __LINE__, request->frame_number, isRestarted, m_flushFlag);
        m_frameFactoryStartThread->run();
    }

    m_flushWaitEnable = true;

    minBlockReqCount = m_exynosconfig->current->bufInfo.num_min_block_request;
    maxBlockReqCount = m_exynosconfig->current->bufInfo.num_max_block_request;
    waitMaxBlockCnt = minBlockReqCount * 10;

    while (m_requestMgr->getRequestCount() >= minBlockReqCount && m_flushFlag == false && waitMaxBlockCnt > 0) {
        status_t waitRet = NO_ERROR;
        m_captureResultDoneLock.lock();
        waitRet = m_captureResultDoneCondition.waitRelative(m_captureResultDoneLock, timeOutNs);
        if (waitRet == TIMED_OUT)
            CLOGD("DEBUG(%s[%d]):time out (m_processList:%zu / totalRequestCnt:%d / "
                "blockReqCount = min:%u, max:%u / waitcnt:%u)",
                __FUNCTION__, __LINE__, m_processList.size(), m_requestMgr->getRequestCount(),
                minBlockReqCount, maxBlockReqCount, waitMaxBlockCnt);

        m_captureResultDoneLock.unlock();
        waitMaxBlockCnt--;

        if (m_requestMgr->getRequestCount() < maxBlockReqCount) {
            break;
        }
    }

req_err:
    return ret;
}

void ExynosCamera3::get_metadata_vendor_tag_ops(const camera3_device_t *, vendor_tag_query_ops_t *ops)
{
    if (ops == NULL) {
        CLOGE("ERR(%s[%d]):ops is NULL", __FUNCTION__, __LINE__);
        return;
    }
}

status_t ExynosCamera3::flush()
{
    status_t ret = NO_ERROR;
    camera3_stream_buffer_t streamBuffer;
    CameraMetadata result;
    ExynosCameraBufferManager *bufferMgr = NULL;
    ExynosCamera3FrameFactory *frameFactory = NULL;
    ExynosCameraRequest* request = NULL;
    ResultRequest resultRequest = NULL;
    ExynosCameraStream *stream = NULL;
    ExynosCameraBuffer buffer;
    List<ExynosCameraFrame *> *list = &m_processList;
    ExynosCameraFrame *curFrame = NULL;
    List<int> *outputStreamId;
    List<int>::iterator outputStreamIdIter;
    List<ExynosCameraFrame *>::iterator r;
    List<request_info_t *>::iterator requestInfoR;
    request_info_t *requestInfo = NULL;
    int bufferIndex;
    int requestIndex = -1;
    int streamId = 0;

    /*
     * This flag should be set before stoping all pipes,
     * because other func(like processCaptureRequest) must know call state about flush() entry level
     */
    m_flushFlag = true;
    m_captureResultDoneCondition.signal();

    Mutex::Autolock l(m_resultLock);
    CLOGD("DEBUG(%s[%d]):IN+++", __FUNCTION__, __LINE__);

    if (m_flushWaitEnable == false) {
        CLOGD("DEBUG(%s[%d]):No need to wait & flush", __FUNCTION__, __LINE__);
        goto func_exit;
    }

    /* Wait for finishing frameFactoryStartThread */
    m_frameFactoryStartThread->requestExitAndWait();
    m_mainThread->requestExitAndWait();

    /* Create frame for the waiting request */
    while (m_requestWaitingList.size() > 0 || m_requestMgr->getServiceRequestCount() > 0) {
        CLOGV("INFO(%s[%d]):Flush request. requestWaitingListSize %d serviceRequestCount %d",
                __FUNCTION__, __LINE__,
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
                   CLOGD("DEBUG(%s[%d]):m_frameFactory index(%d) and index(%d) are same instance,"
                        " set index(%d) = NULL", __FUNCTION__, __LINE__, i, k, k);
                   m_frameFactory[k] = NULL;
               }
            }

            ret = m_stopFrameFactory(m_frameFactory[i]);
            if (ret < 0)
                CLOGE("ERR(%s[%d]):m_frameFactory[%d] stopPipes fail", __FUNCTION__, __LINE__, i);

            CLOGD("DEBUG(%s[%d]):m_frameFactory[%d] stopPipes", __FUNCTION__, __LINE__, i);
        }
    }

    m_captureSelector->release();

    /* Wait until duplicateBufferThread stop */
    if(m_duplicateBufferThread != NULL && m_duplicateBufferThread->isRunning()) {
        m_duplicateBufferThread->requestExitAndWait();
    }

    /* Wait until previewStream3AA/ISPThread stop */
    m_previewStream3AAThread->requestExitAndWait();
    m_previewStreamISPThread->requestExitAndWait();
    m_previewStreamMCSCThread->requestExitAndWait();
    m_previewStreamBayerThread->requestExitAndWait();

    /* Check queued requests from camera service */
    do {
        Mutex::Autolock l(m_processLock);
        while (!list->empty()) {
            r = list->begin()++;
            curFrame = *r;
            if (curFrame == NULL) {
                CLOGE("ERR(%s):curFrame is empty", __FUNCTION__);
                break;
            }

            if (curFrame->getFrameType() == FRAME_TYPE_INTERNAL) {
                m_releaseInternalFrame(curFrame);

                list->erase(r);
                curFrame = NULL;

                continue;
            }

            request = m_requestMgr->getServiceRequest(curFrame->getFrameCount());
            if (request == NULL) {
                CLOGW("WARN(%s[%d]):request is empty", __FUNCTION__, __LINE__);
                list->erase(r);
                curFrame->decRef();
                m_frameMgr->deleteFrame(curFrame);
                curFrame = NULL;
                continue;
            }

            /* handle notify */
            camera3_notify_msg_t notify;
            uint64_t timeStamp = 0L;

            timeStamp = request->getSensorTimestamp();
            if (timeStamp == 0L)
                timeStamp = m_lastFrametime + 15000000; /* set dummy frame time */

            notify.type = CAMERA3_MSG_SHUTTER;
            notify.message.shutter.frame_number = request->getKey();
            notify.message.shutter.timestamp = timeStamp;

            resultRequest = m_requestMgr->createResultRequest(curFrame->getFrameCount(), EXYNOS_REQUEST_RESULT::CALLBACK_NOTIFY_ONLY, NULL, &notify);
            m_requestMgr->callbackSequencerLock();
            m_requestMgr->callbackRequest(resultRequest);
            m_requestMgr->callbackSequencerUnlock();

            request = m_requestMgr->getServiceRequest(curFrame->getFrameCount());
            if (request == NULL) {
                CLOGW("WARN(%s[%d]):request is empty", __FUNCTION__, __LINE__);
                list->erase(r);
                curFrame->decRef();
                m_frameMgr->deleteFrame(curFrame);
                curFrame = NULL;
                continue;
            }

            result = request->getResultMeta();
            result.update(ANDROID_SENSOR_TIMESTAMP, (int64_t *)&timeStamp, 1);
            request->setResultMeta(result);

            request->getAllRequestOutputStreams(&outputStreamId);
            if (outputStreamId->size() > 0) {
                outputStreamIdIter = outputStreamId->begin();
                bufferIndex = 0;
                CLOGI("INFO(%s[%d]):outputStreamId->size(%zu)",
                        __FUNCTION__, __LINE__, outputStreamId->size());
                for (int i = outputStreamId->size(); i > 0; i--) {
                    CLOGI("INFO(%s[%d]):i(%d) *outputStreamIdIter(%d)",
                            __FUNCTION__, __LINE__, i, *outputStreamIdIter);
                    if (*outputStreamIdIter < 0)
                        break;

                    m_streamManager->getStream(*outputStreamIdIter, &stream);

                    if (stream == NULL) {
                        CLOGE("DEBUG(%s[%d]):stream is NULL", __FUNCTION__, __LINE__);
                        ret = INVALID_OPERATION;
                        goto func_exit;
                    }

                    stream->getID(&streamId);
                    CLOGI("INFO(%s[%d]):streamId(%d)", __FUNCTION__, __LINE__, streamId);

                    ret = stream->getStream(&streamBuffer.stream);
                    if (ret < 0) {
                        CLOGE("ERR(%s[%d]):getStream is failed, from exynoscamerastream.",
                            "Id error:HAL_STREAM_ID_PREVIEW", __FUNCTION__, __LINE__);
                        goto func_exit;
                    }

                    requestIndex = -1;
                    stream->getBufferManager(&bufferMgr);
                    ret = bufferMgr->getBuffer(&requestIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &buffer);
                    if (ret != NO_ERROR) {
                        CLOGE("ERR(%s[%d]):Buffer manager getBuffer fail, frameCount(%d), ret(%d)",
                            __FUNCTION__, __LINE__, curFrame->getFrameCount(), ret);
                    }

                    ret = bufferMgr->getHandleByIndex(&streamBuffer.buffer, buffer.index);
                    if (ret != OK) {
                        CLOGE("ERR(%s[%d]):Buffer index error(%d)!!", __FUNCTION__, __LINE__, buffer.index);
                        goto func_exit;
                    }

                    /* handle stream buffers */
                    streamBuffer.status = CAMERA3_BUFFER_STATUS_OK;
                    streamBuffer.acquire_fence = -1;
                    streamBuffer.release_fence = -1;

                    resultRequest = m_requestMgr->createResultRequest(curFrame->getFrameCount(), EXYNOS_REQUEST_RESULT::CALLBACK_BUFFER_ONLY, NULL, NULL);
                    resultRequest->pushStreamBuffer(&streamBuffer);
                    m_requestMgr->callbackSequencerLock();
                    request->increaseCompleteBufferCount();
                    m_requestMgr->callbackRequest(resultRequest);
                    m_requestMgr->callbackSequencerUnlock();

                    bufferIndex++;
                    outputStreamIdIter++;
                }
            }

            CLOGV("INFO(%s[%d]):frameCount(%d), getNumOfOutputBuffer(%d), result num_output_buffers(%d)",
                __FUNCTION__, __LINE__, request->getFrameCount(),
                request->getNumOfOutputBuffer(), bufferIndex);

            /*  frame to complete callback should be removed */
            list->erase(r);

            curFrame->decRef();
            m_frameMgr->deleteFrame(curFrame);
            curFrame = NULL;
        }
    } while(0);

    /* put all internal buffers */
    for (int bufIndex = 0; bufIndex < NUM_BAYER_BUFFERS; bufIndex++) {
        ret = m_putBuffers(m_fliteBufferMgr, bufIndex);
        ret = m_putBuffers(m_3aaBufferMgr, bufIndex);
        ret = m_putBuffers(m_ispBufferMgr, bufIndex);
        ret = m_putBuffers(m_mcscBufferMgr, bufIndex);
    }

    if(m_internalScpBufferMgr != NULL) {
        for (int bufIndex = 0; bufIndex < m_internalScpBufferMgr->getAllocatedBufferCount(); bufIndex++)
            ret = m_putBuffers(m_internalScpBufferMgr, bufIndex);
    }

    /* Clear frame list, because the frame number is initialized in startFrameFactoryThread. */
    ret = m_clearList(&m_processList, &m_processLock);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_clearList(m_processList) failed [%d]", __FUNCTION__, __LINE__, ret);
    }
    ret = m_clearList(&m_captureProcessList, &m_captureProcessLock);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_clearList(m_captureProcessList) failed [%d]", __FUNCTION__, __LINE__, ret);
    }

func_exit:
    CLOGD("DEBUG(%s[%d]) : OUT---", __FUNCTION__, __LINE__);
    return ret;
}

void ExynosCamera3::dump()
{
    CLOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);
}

int ExynosCamera3::getCameraId() const
{
    return m_cameraId;
}

status_t ExynosCamera3::setDualMode(bool enabled)
{
    if (m_parameters == NULL) {
        CLOGE("ERR(%s[%d]):m_parameters is NULL", __FUNCTION__, __LINE__);
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
        CLOGE("ERR(%s[%d]):createBufferManager failed", __FUNCTION__, __LINE__);
        goto func_exit;
    }

    ret = m_allocBuffers(skipBufferMgr, planeCount, planeSize, bytesPerLine, bufferCount, true, false);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_3aaBufferMgr m_allocBuffers(bufferCount=%d) fail",
            __FUNCTION__, __LINE__, bufferCount);
        goto done;
    }

    for (int i = 0; i < bufferCount; i++) {
        int index = i;
        ret = skipBufferMgr->getBuffer(&index, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &fastenAeBuffer[i]);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getBuffer fail", __FUNCTION__, __LINE__);
            goto done;
        }
    }

    ret = factory->fastenAeStable(bufferCount, fastenAeBuffer);
    if (ret < 0) {
        ret = INVALID_OPERATION;
        CLOGE("ERR(%s[%d]):fastenAeStable fail(%d)", __FUNCTION__, __LINE__, ret);
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
    ExynosCameraRequest *request = NULL;
    uint32_t frameCount = 0;
    status_t ret = NO_ERROR;

    /* 1. Wait the shot done with the latest framecount */
    ret = m_shotDoneQ->waitAndPopProcessQ(&frameCount);
    if (ret < 0) {
        if (ret == TIMED_OUT)
            CLOGW("WARN(%s[%d]):wait timeout", __FUNCTION__, __LINE__);
        else
            CLOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        return true;
    }

    m_captureResultDoneCondition.signal();

    if (isRestarted == true) {
        CLOGI("INFO(%s[%d]):wait configure stream", __FUNCTION__, __LINE__);
        usleep(1);

        return true;
    }

    ret = m_createFrameFunc();
    if (ret != NO_ERROR)
        CLOGE("ERR(%s[%d]):Failed to createFrameFunc. Shotdone framecount %d",
                __FUNCTION__, __LINE__, frameCount);

    return true;
}

void ExynosCamera3::m_updateCurrentShot(void)
{
    status_t ret = NO_ERROR;
    List<request_info_t *>::iterator r;
    request_info_t *requestInfo = NULL;
    ExynosCameraRequest *request = NULL;
    ExynosCameraRequest *sensitivityRequest = NULL;
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
        CLOGE("ERR(%s[%d]):Get service shot fail, requestKey(%d), ret(%d)",
                __FUNCTION__, __LINE__, request->getKey(), ret);
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
        CLOGE("ERR(%s[%d]):Get service shot fail, requestKey(%d), ret(%d)",
                __FUNCTION__, __LINE__, request->getKey(), ret);
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
    m_currentShot->shot.ctl.lens = temp_shot_ext.shot.ctl.lens;

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
            CLOGE("ERR(%s[%d]):Get service shot fail. requestKey(%d), ret(%d)",
                    __FUNCTION__, __LINE__, sensitivityRequest->getKey(), ret);
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

    CLOGV("INFO(%s[%d]):m_internalFrameCount %d needRequestFrame %d needInternalFrame %d",
        __FUNCTION__, __LINE__,
        m_internalFrameCount, m_isNeedRequestFrame, m_isNeedInternalFrame);

    return;
}

status_t ExynosCamera3::m_previewframeHandler(ExynosCameraRequest *request, ExynosCamera3FrameFactory *targetfactory)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraBufferManager *bufferMgr = NULL;
    ExynosCameraBuffer buffer;
    const camera3_stream_buffer_t *outputBuffer;
    uint32_t requestKey = 0;
    int32_t bufIndex = -1;
    bool previewFlag = false;
    bool captureFlag = false;
    bool recordingFlag = false;
    bool previewCbFlag = false;
    bool flagFactoryStart = false;
    bool rawStreamFlag = false;
    bool zslStreamFlag = false;
    bool needDynamicBayer = false;
    bool usePureBayer = false;
    int32_t reprocessingBayerMode = m_parameters->getReprocessingBayerMode();
    int pipeId = -1;

    requestKey = request->getKey();

    /* Initialize the request flags in framefactory */
    targetfactory->setRequest(PIPE_FLITE, false);
    targetfactory->setRequest(PIPE_3AC, false);
    targetfactory->setRequest(PIPE_MCSC0, false);
    targetfactory->setRequest(PIPE_MCSC1, false);
    targetfactory->setRequest(PIPE_MCSC2, false);

    /* To decide the dynamic bayer request flag for JPEG capture */
    switch (reprocessingBayerMode) {
    case REPROCESSING_BAYER_MODE_NONE :
        needDynamicBayer = false;
        break;
    case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON :
        targetfactory->setRequest(PIPE_FLITE, true);
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

    /* Setting DMA-out request flag based on stream configuration */
    for (size_t i = 0; i < request->getNumOfOutputBuffer(); i++) {
        int id = request->getStreamId(i);
        switch (id % HAL_STREAM_ID_MAX) {
        case HAL_STREAM_ID_RAW:
            CLOGV("DEBUG(%s[%d]):requestKey %d outputBuffer-Index %zu buffer-StreamType(HAL_STREAM_ID_RAW)",
                    __FUNCTION__, __LINE__, request->getKey(), i);
            targetfactory->setRequest(PIPE_FLITE, true);
            rawStreamFlag = true;
            break;
        case HAL_STREAM_ID_ZSL_OUTPUT:
            ALOGV("DEBUG(%s[%d]):request(%d) outputBuffer-Index(%zu) buffer-StreamType(HAL_STREAM_ID_ZSL)",
                    __FUNCTION__, __LINE__, request->getKey(), i);
            if (usePureBayer == true)
                targetfactory->setRequest(PIPE_FLITE, true);
            else
                targetfactory->setRequest(PIPE_3AC, true);
            zslStreamFlag = true;
            break;
        case HAL_STREAM_ID_PREVIEW:
            CLOGV("DEBUG(%s[%d]):request %d outputBuffer-Index %zu buffer-StreamType(HAL_STREAM_ID_PREVIEW)",
                    __FUNCTION__, __LINE__, request->getKey(), i);
            pipeId = m_streamManager->getOutputPortId(id) + PIPE_MCSC0;
            targetfactory->setRequest(pipeId, true);
            previewFlag = true;
            break;
        case HAL_STREAM_ID_CALLBACK:
            CLOGV("DEBUG(%s[%d]):request %d outputBuffer-Index %zu buffer-StreamType(HAL_STREAM_ID_CALLBACK)",
                    __FUNCTION__, __LINE__, request->getKey(), i);
            pipeId = m_streamManager->getOutputPortId(id) + PIPE_MCSC0;
            targetfactory->setRequest(pipeId, true);
            previewCbFlag = true;
            break;
        case HAL_STREAM_ID_VIDEO:
            CLOGV("DEBUG(%s[%d]):request %d outputBuffer-Index %zu buffer-StreamType(HAL_STREAM_ID_VIDEO)",
                    __FUNCTION__, __LINE__, request->getKey(), i);
            pipeId = m_streamManager->getOutputPortId(id) + PIPE_MCSC0;
            targetfactory->setRequest(pipeId, true);
            recordingFlag = true;
            recordingEnabled = true;
            break;
        case HAL_STREAM_ID_JPEG:
            CLOGD("DEBUG(%s[%d]):request %d outputBuffer-Index %zu buffer-StreamType(HAL_STREAM_ID_JPEG)",
                    __FUNCTION__, __LINE__, request->getKey(), i);
            if (needDynamicBayer == true) {
                if(m_parameters->getUsePureBayerReprocessing()) {
                    targetfactory->setRequest(PIPE_FLITE, true);
                } else {
                    targetfactory->setRequest(PIPE_3AC, true);
                }
            }
            captureFlag = true;
            break;
        default:
            CLOGE("ERR(%s[%d]):Invalid stream ID %d", __FUNCTION__, __LINE__, id);
            break;
        }
    }

    if (m_currentShot == NULL) {
        CLOGE("ERR(%s[%d]):m_currentShot is NULL. Use request control metadata. requestKey %d",
                __FUNCTION__, __LINE__, request->getKey());
        request->getServiceShot(m_currentShot);
    }

    m_updateCropRegion(m_currentShot);

    /* Set framecount into request */
    if (request->getFrameCount() == 0)
        m_requestMgr->setFrameCount(m_internalFrameCount++, request->getKey());

    /* Generate frame for YUV stream */
    ret = m_generateFrame(request->getFrameCount(), targetfactory, &m_processList, &m_processLock, &newFrame);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):Failed to generateRequestFrame. framecount %d",
                __FUNCTION__, __LINE__, m_internalFrameCount - 1);
        goto CLEAN;
    } else if (newFrame == NULL) {
        CLOGE("ERR(%s[%d]):newFrame is NULL. framecount %d",
                __FUNCTION__, __LINE__, m_internalFrameCount - 1);
        goto CLEAN;
    }

    CLOGV("INFO(%s[%d]):generate request framecount %d requestKey %d",
            __FUNCTION__, __LINE__,
            newFrame->getFrameCount(), request->getKey());

    /* Set control metadata to frame */
    ret = newFrame->setMetaData(m_currentShot);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):Set metadata to frame fail, Frame count(%d), ret(%d)",
                __FUNCTION__, __LINE__, newFrame->getFrameCount(), ret);
    }

    /* Attach FLITE buffer & push frame to FLITE */
    if (newFrame->getRequest(PIPE_FLITE) == true) {
        if ((rawStreamFlag == true) && (zslStreamFlag != true)) {
            ret = m_bayerBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &buffer);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to get Service Bayer Buffer. framecount %d availableBuffer %d",
                        __FUNCTION__, __LINE__,
                        newFrame->getFrameCount(),
                        m_bayerBufferMgr->getNumOfAvailableBuffer());
            } else {
                newFrame->setFrameServiceBayer(true);
                CLOGV("DEBUG(%s[%d]):Use Service Bayer Buffer. framecount %d bufferIndex %d",
                        __FUNCTION__, __LINE__,
                        newFrame->getFrameCount(), buffer.index);
            }
        } else {
            ret = m_fliteBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &buffer);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to get Internal Bayer Buffer. framecount %d availableBuffer %d",
                        __FUNCTION__, __LINE__,
                        newFrame->getFrameCount(),
                        m_fliteBufferMgr->getNumOfAvailableBuffer());
            } else {
                CLOGV("DEBUG(%s[%d]):Use Internal Bayer Buffer. framecount %d bufferIndex %d",
                        __FUNCTION__, __LINE__,
                        newFrame->getFrameCount(), buffer.index);
            }

            CLOGV("DEBUG(%s[%d]):Use Internal Bayer Buffer. framecount %d bufferIndex %d",
                    __FUNCTION__, __LINE__,
                    newFrame->getFrameCount(), buffer.index);

            if (zslStreamFlag == true)
                newFrame->setFrameZsl(true);
        }

        if (bufIndex < 0) {
            CLOGW("WANR(%s[%d]):Invalid bayer buffer index %d. Skip to pushFrame",
                    __FUNCTION__, __LINE__, bufIndex);
            ret = newFrame->setEntityState(PIPE_FLITE, ENTITY_STATE_COMPLETE);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to setEntityState with COMPLETE to FLITE. framecount %d",
                        __FUNCTION__, __LINE__, newFrame->getFrameCount());
            }
        } else {
            ret = m_setupEntity(PIPE_FLITE, newFrame, NULL, &buffer);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to setupEntity with bayer buffer. framecount %d bufferIndex %d",
                        __FUNCTION__, __LINE__,
                        newFrame->getFrameCount(), buffer.index);
            } else {
                targetfactory->pushFrameToPipe(&newFrame, PIPE_FLITE);
            }
        }
    }

    if (newFrame->getRequest(PIPE_3AC) == true) {
        if (zslStreamFlag == true) {
            ret = m_zslBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &buffer);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to get ZSL Buffer. framecount %d availableBuffer %d",
                        __FUNCTION__, __LINE__,
                        newFrame->getFrameCount(),
                        m_zslBufferMgr->getNumOfAvailableBuffer());
            } else if (bufIndex < 0) {
                CLOGW("WARN(%s[%d]):Inavlid zsl buffer index %d. Skip to use ZSL service buffer",
                        __FUNCTION__, __LINE__, bufIndex);
            } else {
                CLOGD("DEBUG(%s[%d]):Use ZSL Service Buffer. framecount %d bufferIndex %d",
                        __FUNCTION__, __LINE__,
                        newFrame->getFrameCount(), buffer.index);

                ret = newFrame->setDstBufferState(PIPE_3AA, ENTITY_BUFFER_STATE_REQUESTED, targetfactory->getNodeType(PIPE_3AC));
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):setDstBuffer state fail, pipeID(%d), frameCount(%d), ret(%d)",
                            __FUNCTION__, __LINE__, PIPE_3AA, newFrame->getFrameCount(), ret);
                }

                ret = newFrame->setDstBuffer(PIPE_3AA, buffer, targetfactory->getNodeType(PIPE_3AC));
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):Failed to setupEntity with ZSL service buffer. \
                            framecount %d bufferIndex %d",
                            __FUNCTION__, __LINE__,
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
                CLOGE("ERR(%s[%d]):Failed to getBufferManager for 3AA. framecount %d",
                        __FUNCTION__, __LINE__, newFrame->getFrameCount());
                return ret;
            }

            CLOGW("WARN(%s[%d]):Failed to get 3AA buffer. framecount %d availableBuffer %d",
                    __FUNCTION__, __LINE__,
                    newFrame->getFrameCount(),
                    bufferMgr->getNumOfAvailableBuffer());
        } else {
            targetfactory->pushFrameToPipe(&newFrame, PIPE_3AA);
        }
    }

    if (captureFlag == true) {
        CLOGI("INFO(%s[%d]):setFrameCapture true, frameCount %d",
                __FUNCTION__, __LINE__, newFrame->getFrameCount());
        newFrame->setFrameCapture(captureFlag);
    }

    /* Run PIPE_FLITE main thread to enable dynamic bayer DMA-write */
    if (targetfactory->checkPipeThreadRunning(PIPE_FLITE) == false
        && newFrame->getRequest(PIPE_FLITE) == true) {
        m_previewStreamBayerThread->run(PRIORITY_DEFAULT);
        targetfactory->startThread(PIPE_FLITE);
    }

CLEAN:
    return ret;
}

status_t ExynosCamera3::m_captureframeHandler(ExynosCameraRequest *request, ExynosCamera3FrameFactory *targetfactory)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrame *newFrame = NULL;
    struct camera2_shot_ext shot_ext;
    uint32_t requestKey = 0;
    bool captureFlag = false;
    bool rawStreamFlag = false;
    bool zslFlag = false;
    bool isNeedThumbnail = false;

    CLOGD("DEBUG(%s[%d]):Capture request. requestKey %d frameCount %d",
            __FUNCTION__, __LINE__,
            request->getKey(),
            request->getFrameCount());

    if (targetfactory == NULL) {
        CLOGE("ERR(%s[%d]):targetfactory is NULL", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    m_startPictureBufferThread->join();

    requestKey = request->getKey();

    /* Initialize the request flags in framefactory */
    if(m_parameters->isSingleChain()) {
        // MCSC2_REPROCESSING is MSCS output P0
        targetfactory->setRequest(PIPE_MCSC2_REPROCESSING, false);
    }
    targetfactory->setRequest(PIPE_MCSC3_REPROCESSING, false);
    targetfactory->setRequest(PIPE_MCSC4_REPROCESSING, false);

    /* set input buffers belonged to each stream as available */
    camera3_stream_buffer_t* buffer = request->getInputBuffer();
    if(buffer != NULL) {
        int inputStreamId = 0;
        ExynosCameraStream *stream = static_cast<ExynosCameraStream *>(buffer->stream->priv);
        if(stream != NULL) {
            stream->getID(&inputStreamId);
            CLOGD("DEBUG(%s[%d]):requestKey %d buffer-StreamType(HAL_STREAM_ID_ZSL_INPUT[%d]) Buffer[%p], Stream[%p]",
                    __FUNCTION__, __LINE__, request->getKey(), inputStreamId, buffer, stream);

            switch (inputStreamId % HAL_STREAM_ID_MAX) {
            case HAL_STREAM_ID_ZSL_INPUT:
                CLOGD("DEBUG(%s[%d]):requestKey %d buffer-StreamType(HAL_STREAM_ID_ZSL_INPUT[%d])",
                        __FUNCTION__, __LINE__, request->getKey(), inputStreamId);
                zslFlag = true;
                break;
            case HAL_STREAM_ID_JPEG:
            case HAL_STREAM_ID_PREVIEW:
            case HAL_STREAM_ID_VIDEO:
            case HAL_STREAM_ID_CALLBACK:
            case HAL_STREAM_ID_MAX:
                CLOGE("ERR(%s[%d]):requestKey %d Invalid buffer-StreamType(%d)",
                        __FUNCTION__, __LINE__, request->getKey(), inputStreamId);
                break;
            default:
                break;
            }
        }else {
            CLOGE("ERR(%s[%d]): Stream is null (%d)", __FUNCTION__, __LINE__, request->getKey());
        }
    }

    /* set output buffers belonged to each stream as available */
    for (size_t i = 0; i < request->getNumOfOutputBuffer(); i++) {
        int id = request->getStreamId(i);
        switch (id % HAL_STREAM_ID_MAX) {
        case HAL_STREAM_ID_JPEG:
            CLOGD("DEBUG(%s[%d]):requestKey %d buffer-StreamType(HAL_STREAM_ID_JPEG)",
                    __FUNCTION__, __LINE__, request->getKey());
            targetfactory->setRequest(PIPE_MCSC3_REPROCESSING, true);
            captureFlag = true;

            request->getServiceShot(&shot_ext);
            isNeedThumbnail = (shot_ext.shot.ctl.jpeg.thumbnailSize[0] > 0
                               && shot_ext.shot.ctl.jpeg.thumbnailSize[1] > 0)? true : false;
            targetfactory->setRequest(PIPE_MCSC4_REPROCESSING, isNeedThumbnail);
            break;
        case HAL_STREAM_ID_RAW:
            CLOGV("DEBUG(%s[%d]):requestKey %d buffer-StreamType(HAL_STREAM_ID_RAW)",
                    __FUNCTION__, __LINE__, request->getKey());
            rawStreamFlag = true;
            break;
        case HAL_STREAM_ID_PREVIEW:
        case HAL_STREAM_ID_VIDEO:
        case HAL_STREAM_ID_CALLBACK:
            if(zslFlag == true) {
                /* YUV output using ZSL input */
                captureFlag = true;
                if(m_parameters->isSingleChain()) {
                    targetfactory->setRequest(PIPE_MCSC2_REPROCESSING, true);
                } else {
                    CLOGE2("ZSL Reprocessing to MCSC2_REPROCESSING is not supported.");
                    return INVALID_OPERATION;
                }
            }
            break;
        case HAL_STREAM_ID_MAX:
            CLOGE("ERR(%s[%d]):requestKey %d Invalid buffer-StreamType(%d)",
                    __FUNCTION__, __LINE__, request->getKey(), id);
            break;
        default:
            break;
        }
    }

    if (m_currentShot == NULL) {
        CLOGE("ERR(%s[%d]):m_currentShot is NULL. Use request control metadata. requestKey %d",
                __FUNCTION__, __LINE__, request->getKey());
        request->getServiceShot(m_currentShot);
    }

    m_updateCropRegion(m_currentShot);
    m_updateJpegControlInfo(m_currentShot);

    /* Set framecount into request */
    if (request->getNeedInternalFrame() == true)
        /* Must use the same framecount with internal frame */
        m_internalFrameCount--;

    if (request->getFrameCount() == 0)
        m_requestMgr->setFrameCount(m_internalFrameCount++, request->getKey());

    ret = m_generateFrame(request->getFrameCount(), targetfactory, &m_captureProcessList, &m_captureProcessLock, &newFrame);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_generateFrame fail", __FUNCTION__, __LINE__);
    } else if (newFrame == NULL) {
        CLOGE("ERR(%s[%d]):new faame is NULL", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    CLOGV("DEBUG(%s[%d]):generate request framecount %d requestKey %d",
            __FUNCTION__, __LINE__,
            newFrame->getFrameCount(), request->getKey());

    ret = newFrame->setMetaData(m_currentShot);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):Set metadata to frame fail, Frame count(%d), ret(%d)",
                __FUNCTION__, __LINE__, newFrame->getFrameCount(), ret);
    }

    newFrame->setFrameServiceBayer(rawStreamFlag);
    newFrame->setFrameCapture(captureFlag);
    newFrame->setFrameZsl(zslFlag);

    m_selectBayerQ->pushProcessQ(&newFrame);

    if(m_selectBayerThread != NULL && m_selectBayerThread->isRunning() == false) {
        m_selectBayerThread->run();
        CLOGI("INFO(%s[%d]):Initiate selectBayerThread (%d)",
            __FUNCTION__, __LINE__, m_selectBayerThread->getTid());
    }

    return ret;
}

status_t ExynosCamera3::m_createRequestFrameFunc(ExynosCameraRequest *request)
{
    int32_t factoryAddrIndex = 0;
    bool removeDupFlag = false;

    ExynosCamera3FrameFactory *factory = NULL;
    ExynosCamera3FrameFactory *factoryAddr[100] ={NULL,};
    FrameFactoryList factorylist;
    FrameFactoryListIterator factorylistIter;
    factory_handler_t frameCreateHandler;

    // TODO: acquire fence
    /* 1. Remove the duplicated frame factory in request */
    factoryAddrIndex = 0;
    factorylist.clear();

    request->getFrameFactoryList(&factorylist);
    for (factorylistIter = factorylist.begin(); factorylistIter != factorylist.end(); ) {
        removeDupFlag = false;
        factory = *factorylistIter;
        CLOGV("DEBUG(%s[%d]):list Factory(%p) ", __FUNCTION__, __LINE__, factory);

        for (int i = 0; i < factoryAddrIndex ; i++) {
            if (factoryAddr[i] == factory) {
                removeDupFlag = true;
                break;
            }
        }

        if (removeDupFlag) {
            CLOGV("DEBUG(%s[%d]):remove duplicate Factory factoryAddrIndex(%d)",
                    __FUNCTION__, __LINE__, factoryAddrIndex);
            factorylist.erase(factorylistIter++);
        } else {
            CLOGV("DEBUG(%s[%d]):add frame factory, factoryAddrIndex(%d)",
                    __FUNCTION__, __LINE__, factoryAddrIndex);
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
            CLOGV("DEBUG(%s[%d]):framefactory factoryAddr[%d] = %p / m_frameFactory[%d] = %p",
                    __FUNCTION__, __LINE__, i, factoryAddr[i], factoryTypeIdx, m_frameFactory[factoryTypeIdx]);

            if( (factoryAddr[i] != NULL) && (factoryAddr[i] == m_frameFactory[factoryTypeIdx]) ) {
                CLOGV("DEBUG(%s[%d]):framefactory index(%d) maxIndex(%d) (%p)",
                        __FUNCTION__, __LINE__, i, factoryAddrIndex, factoryAddr[i]);
                frameCreateHandler = factoryAddr[i]->getFrameCreateHandler();
                (this->*frameCreateHandler)(request, factoryAddr[i]);
                factoryAddr[i] = NULL;
            }
        }
    }

    CLOGV("DEBUG(%s[%d]):- OUT - (F:%d)", __FUNCTION__, __LINE__, request->getKey());
    return NO_ERROR;
}

status_t ExynosCamera3::m_createInternalFrameFunc(void)
{
    status_t ret = NO_ERROR;
    ExynosCameraBufferManager *bufferMgr = NULL;
    ExynosCamera3FrameFactory *factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
    ExynosCameraFrame *newFrame = NULL;

    /* Generate the internal frame */
    ret = m_generateInternalFrame(m_internalFrameCount++, factory, &m_processList, &m_processLock, &newFrame);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_generateFrame failed", __FUNCTION__, __LINE__);
        return ret;
    } else if (newFrame == NULL) {
        CLOGE("ERR(%s[%d]):newFrame is NULL", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    CLOGV("INFO(%s[%d]):generate internal framecount %d",
            __FUNCTION__, __LINE__,
            newFrame->getFrameCount());

    /* Disable all DMA-out request for this internal frame
     *                    3AP,   3AC,   ISP,   ISPP,  ISPC,  SCC,   DIS,   SCP */
    newFrame->setRequest(false, false, false, false, false, false, false, false);
    newFrame->setRequest(PIPE_FLITE, false);
    newFrame->setRequest(PIPE_MCSC1, false);
    newFrame->setRequest(PIPE_MCSC2, false);

    switch (m_parameters->getReprocessingBayerMode()) {
    case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON :
        newFrame->setRequest(PIPE_FLITE, true);
        break;
    case REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON:
        newFrame->setRequest(PIPE_3AC, true);
        break;
    default:
        break;
    }

    /* Update the metadata with m_currentShot into frame */
    if (m_currentShot == NULL) {
        CLOGE("ERR(%s[%d]):m_currentShot is NULL", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    ret = newFrame->setMetaData(m_currentShot);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):Failed to setMetaData with m_currehtShot. framecount %d ret %d",
                __FUNCTION__, __LINE__,
                newFrame->getFrameCount(), ret);
        return ret;
    }

    /* Attach FLITE buffer & push frame to FLITE */
    if (newFrame->getRequest(PIPE_FLITE) == true) {
        ret = m_setupEntity(PIPE_FLITE, newFrame);
        if (ret != NO_ERROR) {
            ret = m_getBufferManager(PIPE_FLITE, &bufferMgr, DST_BUFFER_DIRECTION);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to getBufferManager for FLITE. framecount %d",
                        __FUNCTION__, __LINE__, newFrame->getFrameCount());
            }

            CLOGW("WARN(%s[%d]):Failed to get FLITE buffer. Skip to pushFrame. framecount %d availableBuffer %d",
                    __FUNCTION__, __LINE__,
                    newFrame->getFrameCount(),
                    bufferMgr->getNumOfAvailableBuffer());

            ret = newFrame->setEntityState(PIPE_FLITE, ENTITY_STATE_COMPLETE);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to setEntityState with COMPLETE to FLITE. framecount %d",
                        __FUNCTION__, __LINE__, newFrame->getFrameCount());
            }
        } else {
            factory->pushFrameToPipe(&newFrame, PIPE_FLITE);
        }
    }

    /* Attach 3AA buffer & push frame to 3AA */
    if (m_parameters->isFlite3aaOtf() == true) {
        ret = m_setupEntity(PIPE_3AA, newFrame);
        if (ret != NO_ERROR) {
            ret = m_getBufferManager(PIPE_3AA, &bufferMgr, SRC_BUFFER_DIRECTION);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to getBufferManager for 3AA. framecount %d",
                        __FUNCTION__, __LINE__, newFrame->getFrameCount());
                return ret;
            }

            CLOGW("WARN(%s[%d]):Failed to get 3AA buffer. framecount %d availableBuffer %d",
                    __FUNCTION__, __LINE__,
                    newFrame->getFrameCount(),
                    bufferMgr->getNumOfAvailableBuffer());
        } else {
            factory->pushFrameToPipe(&newFrame, PIPE_3AA);
        }
    }

    return ret;
}

status_t ExynosCamera3::m_createFrameFunc(void)
{
    status_t ret = NO_ERROR;
    ExynosCameraRequest *request = NULL;

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
        m_popRequest(&request);

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

    CLOGV("DEBUG(%s[%d]):Create New Frame %d needRequestFrame %d needInternalFrame %d waitingSize %d",
            __FUNCTION__, __LINE__,
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

status_t ExynosCamera3::m_sendRawCaptureResult(ExynosCameraFrame *frame, uint32_t pipeId, bool isSrc)
{
    status_t ret = NO_ERROR;
    ExynosCameraStream *stream = NULL;
    ExynosCameraRequest *request = NULL;
    ExynosCameraBuffer buffer;
    camera3_stream_buffer_t streamBuffer;
    ResultRequest resultRequest = NULL;

    /* 1. Get stream object for RAW */
    ret = m_streamManager->getStream(HAL_STREAM_ID_RAW, &stream);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getStream is failed, from streammanager. Id error:HAL_STREAM_ID_RAW",
                __FUNCTION__, __LINE__);
        return ret;
    }

    /* 2. Get camera3_stream structure from stream object */
    ret = stream->getStream(&streamBuffer.stream);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getStream is failed, from exynoscamerastream. Id error:HAL_STREAM_ID_RAW",
                __FUNCTION__, __LINE__);
        return ret;
    }

    /* 3. Get the bayer buffer from frame */
    if (isSrc == true)
        ret = frame->getSrcBuffer(pipeId, &buffer);
    else
        ret = frame->getDstBuffer(pipeId, &buffer);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):Get bayer buffer failed, framecount(%d), isSrc(%d), pipeId(%d)",
                __FUNCTION__, __LINE__,
                frame->getFrameCount(), isSrc, pipeId);
        return ret;
    }

    /* 4. Get the service buffer handle from buffer manager */
    ret = m_bayerBufferMgr->getHandleByIndex(&(streamBuffer.buffer), buffer.index);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):Buffer index error(%d)!!", __FUNCTION__, __LINE__, buffer.index);
        return ret;
    }

    /* 5. Update the remained buffer info */
    streamBuffer.status = CAMERA3_BUFFER_STATUS_OK;
    streamBuffer.acquire_fence = -1;
    streamBuffer.release_fence = -1;

    /* 6. Create new result for RAW buffer */
    request = m_requestMgr->getServiceRequest(frame->getFrameCount());
    resultRequest = m_requestMgr->createResultRequest(frame->getFrameCount(), EXYNOS_REQUEST_RESULT::CALLBACK_BUFFER_ONLY, NULL, NULL);
    resultRequest->pushStreamBuffer(&streamBuffer);

    /* 7. Request to callback the result to request manager */
    m_requestMgr->callbackSequencerLock();
    request->increaseCompleteBufferCount();
    m_requestMgr->callbackRequest(resultRequest);
    m_requestMgr->callbackSequencerUnlock();

    CLOGV("DEBUG(%s[%d]):request->frame_number(%d), request->getNumOfOutputBuffer(%d)"
            " request->getCompleteBufferCount(%d) frame->getFrameCapture(%d)",
            __FUNCTION__, __LINE__,
            request->getKey(),
            request->getNumOfOutputBuffer(),
            request->getCompleteBufferCount(),
            frame->getFrameCapture());

    CLOGV("DEBUG(%s[%d]):streamBuffer info: stream (%p), handle(%p)",
            __FUNCTION__, __LINE__,
            streamBuffer.stream, streamBuffer.buffer);

    return ret;
}

status_t ExynosCamera3::m_sendZSLCaptureResult(ExynosCameraFrame *frame, __unused uint32_t pipeId, __unused bool isSrc)
{
    status_t ret = NO_ERROR;
    ExynosCameraStream *stream = NULL;
    ExynosCameraRequest *request = NULL;
    camera3_stream_buffer_t streamBuffer;
    ResultRequest resultRequest = NULL;
    const camera3_stream_buffer_t *buffer;
    const camera3_stream_buffer_t *bufferList;
    int streamId = 0;
    uint32_t bufferCount = 0;


    /* 1. Get stream object for ZSL */
    ret = m_streamManager->getStream(HAL_STREAM_ID_ZSL_OUTPUT, &stream);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getStream is failed, from streammanager. Id error:HAL_STREAM_ID_ZSL",
                __FUNCTION__, __LINE__);
        return ret;
    }

    /* 2. Get camera3_stream structure from stream object */
    ret = stream->getStream(&streamBuffer.stream);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getStream is failed, from exynoscamerastream. Id error:HAL_STREAM_ID_RAW",
                __FUNCTION__, __LINE__);
        return ret;
    }

    /* 3. Get zsl buffer */
    request = m_requestMgr->getServiceRequest(frame->getFrameCount());
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
    request->increaseCompleteBufferCount();
    m_requestMgr->callbackRequest(resultRequest);
    m_requestMgr->callbackSequencerUnlock();

    CLOGV("DEBUG(%s[%d]):request->frame_number(%d), request->getNumOfOutputBuffer(%d)"
            "request->getCompleteBufferCount(%d) frame->getFrameCapture(%d)",
            __FUNCTION__, __LINE__,
            request->getKey(),
            request->getNumOfOutputBuffer(),
            request->getCompleteBufferCount(),
            frame->getFrameCapture());

    CLOGV("DEBUG(%s[%d]):streamBuffer info: stream (%p), handle(%p)",
            __FUNCTION__, __LINE__,
            streamBuffer.stream, streamBuffer.buffer);

    return ret;

}

status_t ExynosCamera3::m_sendNotify(uint32_t frameNumber, int type)
{
    camera3_notify_msg_t notify;
    ResultRequest resultRequest = NULL;
    ExynosCameraRequest *request = NULL;
    uint32_t frameCount = 0;
    uint64_t timeStamp = m_lastFrametime;

    status_t ret = OK;
    request = m_requestMgr->getServiceRequest(frameNumber);
    frameCount = request->getKey();
    timeStamp = request->getSensorTimestamp();

    CLOGV("DEBUG(%s[%d]):framecount %d timestamp %lld", __FUNCTION__, __LINE__, frameNumber, timeStamp);
    switch (type) {
    case CAMERA3_MSG_ERROR:
        notify.type = CAMERA3_MSG_ERROR;
        notify.message.error.frame_number = frameCount;
        notify.message.error.error_code = CAMERA3_MSG_ERROR_BUFFER;
        // TODO: how can handle this?
        //msg.message.error.error_stream = j->stream;
        resultRequest = m_requestMgr->createResultRequest(frameNumber, EXYNOS_REQUEST_RESULT::CALLBACK_NOTIFY_ONLY, NULL, &notify);
        m_requestMgr->callbackSequencerLock();
        m_requestMgr->callbackRequest(resultRequest);
        m_requestMgr->callbackSequencerUnlock();
        break;
    case CAMERA3_MSG_SHUTTER:
        CLOGV("DEBUG(%s[%d]) : SHUTTER (%d)frame t(%lld)", __FUNCTION__, __LINE__, frameNumber, timeStamp);
        notify.type = CAMERA3_MSG_SHUTTER;
        notify.message.shutter.frame_number = frameCount;
        notify.message.shutter.timestamp = timeStamp;
        resultRequest = m_requestMgr->createResultRequest(frameNumber, EXYNOS_REQUEST_RESULT::CALLBACK_NOTIFY_ONLY, NULL, &notify);
        /* keep current frame time for flush ops */
        m_requestMgr->callbackSequencerLock();
        m_requestMgr->callbackRequest(resultRequest);
        m_requestMgr->callbackSequencerUnlock();
        break;
    default:
        CLOGE("ERR(%s[%d]):Msg type is invalid (%d)", __FUNCTION__, __LINE__, type);
        ret = BAD_VALUE;
        break;
    }

    return ret;
}

status_t ExynosCamera3::m_searchFrameFromList(List<ExynosCameraFrame *> *list, Mutex *listLock, uint32_t frameCount, ExynosCameraFrame **frame)
{
    ExynosCameraFrame *curFrame = NULL;
    List<ExynosCameraFrame *>::iterator r;

    Mutex::Autolock l(listLock);
    if (list->empty()) {
        CLOGD("DEBUG(%s[%d]):list is empty", __FUNCTION__, __LINE__);
        return NO_ERROR;
    }

    r = list->begin()++;

    do {
        curFrame = *r;
        if (curFrame == NULL) {
            CLOGE("ERR(%s[%d]):curFrame is empty", __FUNCTION__, __LINE__);
            return INVALID_OPERATION;
        }

        if (frameCount == curFrame->getFrameCount()) {
            CLOGV("DEBUG(%s[%d]):frame count match: expected(%d)", __FUNCTION__, __LINE__, frameCount);
            *frame = curFrame;
            return NO_ERROR;
        }
        r++;
    } while (r != list->end());

    CLOGV("WARN(%s[%d]):Cannot find match frame, frameCount(%d)", __FUNCTION__, __LINE__, frameCount);

    return OK;
}

status_t ExynosCamera3::m_removeFrameFromList(List<ExynosCameraFrame *> *list, Mutex *listLock, ExynosCameraFrame *frame)
{
    ExynosCameraFrame *curFrame = NULL;
    int frameCount = 0;
    int curFrameCount = 0;
    List<ExynosCameraFrame *>::iterator r;

    if (frame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    Mutex::Autolock l(listLock);
    if (list->empty()) {
        CLOGE("ERR(%s[%d]):list is empty", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    frameCount = frame->getFrameCount();
    r = list->begin()++;

    do {
        curFrame = *r;
        if (curFrame == NULL) {
            CLOGE("ERR(%s[%d]):curFrame is empty", __FUNCTION__, __LINE__);
            return INVALID_OPERATION;
        }

        curFrameCount = curFrame->getFrameCount();
        if (frameCount == curFrameCount) {
            CLOGV("DEBUG(%s[%d]):frame count match: expected(%d), current(%d)",
                __FUNCTION__, __LINE__, frameCount, curFrameCount);
            list->erase(r);
            return NO_ERROR;
        }
        CLOGW("WARN(%s[%d]):frame count mismatch: expected(%d), current(%d)",
            __FUNCTION__, __LINE__, frameCount, curFrameCount);
        /* curFrame->printEntity(); */
        r++;
    } while (r != list->end());

    CLOGE("ERR(%s[%d]):Cannot find match frame!!!", __FUNCTION__, __LINE__);

    return INVALID_OPERATION;
}

status_t ExynosCamera3::m_clearList(List<ExynosCameraFrame *> *list, Mutex *listLock)
{
    ExynosCameraFrame *curFrame = NULL;
    List<ExynosCameraFrame *>::iterator r;

    CLOGD("DEBUG(%s[%d]):remaining frame(%zu), we remove them all", __FUNCTION__, __LINE__, list->size());

    Mutex::Autolock l(listLock);
    while (!list->empty()) {
        r = list->begin()++;
        curFrame = *r;
        if (curFrame != NULL) {
            CLOGV("DEBUG(%s[%d]):remove frame count(%d)", __FUNCTION__, __LINE__, curFrame->getFrameCount());
            curFrame->decRef();
            m_frameMgr->deleteFrame(curFrame);
        }
        list->erase(r);
    }

    return OK;
}

status_t ExynosCamera3::m_removeInternalFrames(List<ExynosCameraFrame *> *list, Mutex *listLock)
{
    ExynosCameraFrame *curFrame = NULL;
    List<ExynosCameraFrame *>::iterator r;

    CLOGD("DEBUG(%s[%d]):remaining frame(%zu), we remove internal frames",
            __FUNCTION__, __LINE__, list->size());

    Mutex::Autolock l(listLock);
    while (!list->empty()) {
        r = list->begin()++;
        curFrame = *r;
        if (curFrame != NULL) {
            if (curFrame->getFrameType() == FRAME_TYPE_INTERNAL) {
                CLOGV("DEBUG(%s[%d]):remove internal frame(%d)",
                        __FUNCTION__, __LINE__, curFrame->getFrameCount());
                m_releaseInternalFrame(curFrame);
            } else {
                CLOGW("WARN(%s[%d]):frame(%d) is NOT internal frame and will be remained in List",
                        __FUNCTION__, __LINE__, curFrame->getFrameCount());
            }
        }
        list->erase(r);
        curFrame = NULL;
    }

    return OK;
}

status_t ExynosCamera3::m_releaseInternalFrame(ExynosCameraFrame *frame)
{
    status_t ret = NO_ERROR;
    ExynosCameraBuffer buffer;
    ExynosCameraBufferManager *bufferMgr = NULL;

    if (frame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }
    if (frame->getFrameType() != FRAME_TYPE_INTERNAL) {
        CLOGE("ERR(%s[%d]):frame(%d) is NOT internal frame",
                __FUNCTION__, __LINE__, frame->getFrameCount());
        return BAD_VALUE;
    }

    /* Return bayer buffer */
    if (frame->getRequest(PIPE_FLITE) == true) {
        ret = frame->getDstBuffer(PIPE_FLITE, &buffer);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):getDstBuffer failed. PIPE_FLITE, ret %d",
                    __FUNCTION__, __LINE__, ret);
        } else if (buffer.index >= 0) {
            ret = m_getBufferManager(PIPE_FLITE, &bufferMgr, DST_BUFFER_DIRECTION);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to getBufferManager for FLITE",
                        __FUNCTION__, __LINE__);
            } else {
                ret = m_putBuffers(bufferMgr, buffer.index);
                if (ret != NO_ERROR)
                    CLOGE("ERR(%s[%d]):Failed to putBuffer for FLITE. index %d",
                            __FUNCTION__, __LINE__, buffer.index);
            }
        }
    }

    /* Return 3AS buffer */
    ret = frame->getSrcBuffer(PIPE_3AA, &buffer);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):getSrcBuffer failed. PIPE_3AA, ret(%d)",
                __FUNCTION__, __LINE__, ret);
    } else if (buffer.index >= 0) {
        ret = m_getBufferManager(PIPE_3AA, &bufferMgr, SRC_BUFFER_DIRECTION);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Failed to getBufferManager for 3AS",
                    __FUNCTION__, __LINE__);
        } else {
            ret = m_putBuffers(bufferMgr, buffer.index);
            if (ret != NO_ERROR)
                CLOGE("ERR(%s[%d]):Failed to putBuffer for 3AS. index %d",
                        __FUNCTION__, __LINE__, buffer.index);
        }
    }

    /* Return 3AP buffer */
    if (frame->getRequest(PIPE_3AP) == true) {
        ret = frame->getDstBuffer(PIPE_3AA, &buffer);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):getDstBuffer failed. PIPE_3AA, ret %d",
                    __FUNCTION__, __LINE__, ret);
        } else if (buffer.index >= 0) {
            ret = m_getBufferManager(PIPE_3AA, &bufferMgr, DST_BUFFER_DIRECTION);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to getBufferManager for 3AP",
                        __FUNCTION__, __LINE__);
            } else {
                ret = m_putBuffers(bufferMgr, buffer.index);
                if (ret != NO_ERROR)
                    CLOGE("ERR(%s[%d]):Failed to putBuffer for 3AP. index %d",
                            __FUNCTION__, __LINE__, buffer.index);
            }
        }
    }

    frame->decRef();
    m_frameMgr->deleteFrame(frame);
    frame = NULL;

    return ret;
}

status_t ExynosCamera3::m_setFrameManager()
{
    sp<FrameWorker> worker;
    m_frameMgr = new ExynosCameraFrameManager("FRAME MANAGER", m_cameraId, FRAMEMGR_OPER::SLIENT, 50, 100);

    worker = new CreateWorker("CREATE FRAME WORKER", m_cameraId, FRAMEMGR_OPER::SLIENT, 40);
    m_frameMgr->setWorker(FRAMEMGR_WORKER::CREATE, worker);

    worker = new DeleteWorker("DELETE FRAME WORKER", m_cameraId, FRAMEMGR_OPER::SLIENT);
    m_frameMgr->setWorker(FRAMEMGR_WORKER::DELETE, worker);

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
        CLOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        goto func_exit;
    }

    if (framefactory == NULL) {
        CLOGE("ERR(%s[%d]):framefactory is NULL", __FUNCTION__, __LINE__);
        goto func_exit;
    }

    if (framefactory->isCreated() == false) {
        CLOGD("DEBUG(%s[%d]):framefactory create", __FUNCTION__, __LINE__);
        framefactory->create();
    } else {
        CLOGD("DEBUG(%s[%d]):framefactory already create", __FUNCTION__, __LINE__);
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
    ExynosCameraRequest *request = NULL;
    int32_t reprocessingBayerMode = m_parameters->getReprocessingBayerMode();
    uint32_t prepare = m_exynosconfig->current->pipeInfo.prepare[PIPE_3AA];
    bool startBayerThreads = false;

    if (m_requestMgr->getServiceRequestCount() < 1) {
        CLOGE("ERR(%s[%d]):There is NO available request!!! "
            "\"processCaptureRequest()\" must be called, first!!!", __FUNCTION__, __LINE__);
        return false;
    }

    factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
    if (factory == NULL) {
        CLOGE("ERR(%s[%d]):Can't start FrameFactory!!!! FrameFactory is NULL!! Prepare(%d), Request(%d)",
                __FUNCTION__, __LINE__,
                prepare,
                m_requestMgr != NULL ? m_requestMgr->getRequestCount(): 0);

        return false;
    } else if (factory->isCreated() == false) {
        CLOGE("ERR(%s[%d]):Preview FrameFactory is NOT created!", __FUNCTION__, __LINE__);
        return false;
    } else if (factory->isRunning() == true) {
        CLOGW("WARN(%s[%d]):Preview FrameFactory is already running.", __FUNCTION__, __LINE__);
        return false;
    }

    /* To decide to run FLITE threads  */
    switch (reprocessingBayerMode) {
    case REPROCESSING_BAYER_MODE_NONE :
    case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON :
    case REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON :
        startBayerThreads = true;
        break;
    case REPROCESSING_BAYER_MODE_PURE_DYNAMIC :
    case REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC :
        startBayerThreads = false;
        break;
    default :
        break;
    }

    m_internalFrameCount = FRAME_INTERNAL_START_COUNT;

    /* Set default request flag & buffer manager */
    ret = m_setupPipeline();
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):Failed to setupPipeline. ret %d",
                __FUNCTION__, __LINE__, ret);
        return false;
    }

    ret = factory->initPipes();
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):Failed to initPipes. ret %d",
                __FUNCTION__, __LINE__, ret);
        return false;
    }

    ret = factory->mapBuffers();
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_previewFrameFactory->mapBuffers() failed", __FUNCTION__, __LINE__);
        return ret;
    }

    CLOGD("DEBUG(%s[%d]):prepare %d", __FUNCTION__, __LINE__, prepare);
    for (uint32_t i = 0; i < prepare + 1; i++) {
        ret = m_createFrameFunc();
        if (ret != NO_ERROR)
            CLOGE("ERR(%s[%d]):Failed to createFrameFunc for preparing frame. prepareCount %d/%d",
                    __FUNCTION__, __LINE__, i, prepare);
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

    if (m_parameters->is3aaIspOtf() == false)
        m_previewStreamISPThread->run(PRIORITY_URGENT_DISPLAY);

    if (m_parameters->isIspMcscOtf() == false)
        m_previewStreamMCSCThread->run(PRIORITY_URGENT_DISPLAY);

    if (startBayerThreads == true) {
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

    CLOGI("INFO(%s[%d]):-IN-", __FUNCTION__, __LINE__);

    ExynosCamera3FrameFactory *factory = NULL;

    for(int i = 0; i < FRAME_FACTORY_TYPE_MAX; i++)
        m_frameFactory[i] = NULL;

    factory = new ExynosCamera3FrameFactoryPreview(m_cameraId, m_parameters);
    factory->setFrameCreateHandler(&ExynosCamera3::m_previewframeHandler);
    m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW] = factory;
    m_frameFactory[FRAME_FACTORY_TYPE_RECORDING_PREVIEW] = factory;
    m_frameFactory[FRAME_FACTORY_TYPE_DUAL_PREVIEW] = factory;

    if (m_parameters->isReprocessing() == true) {
        factory = new ExynosCamera3FrameReprocessingFactory(m_cameraId, m_parameters);
        factory->setFrameCreateHandler(&ExynosCamera3::m_captureframeHandler);
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

    CLOGI("INFO(%s[%d]):-OUT-", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCamera3::m_startFrameFactory(ExynosCamera3FrameFactory *factory)
{
    status_t ret = OK;

    /* prepare pipes */
    ret = factory->preparePipes();
    if (ret < 0) {
        CLOGW("ERR(%s[%d]):Failed to prepare FLITE", __FUNCTION__, __LINE__);
    }

    /* s_ctrl HAL version for selecting dvfs table */
    ret = factory->setControl(V4L2_CID_IS_HAL_VERSION, IS_HAL_VER_3_2, PIPE_3AA);

    if (ret < 0)
        CLOGW("WARN(%s): V4L2_CID_IS_HAL_VERSION is fail", __FUNCTION__);

    /* stream on pipes */
    ret = factory->startPipes();
    if (ret < 0) {
        CLOGE("ERR(%s):startPipe fail", __FUNCTION__);
        return ret;
    }

    /* start all thread */
    ret = factory->startInitialThreads();
    if (ret < 0) {
        CLOGE("ERR(%s):startInitialThreads fail", __FUNCTION__);
        return ret;
    }

    return ret;
}

status_t ExynosCamera3::m_stopFrameFactory(ExynosCamera3FrameFactory *factory)
{
    int ret = 0;

    CLOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);
    if (factory != NULL  && factory->isRunning() == true) {
        ret = factory->stopPipes();
        if (ret < 0) {
            CLOGE("ERR(%s):stopPipe fail", __FUNCTION__);
            return ret;
        }
    }

    return ret;
}

status_t ExynosCamera3::m_deinitFrameFactory()
{
    CLOGI("INFO(%s[%d]):-IN-", __FUNCTION__, __LINE__);

    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = NO_ERROR;
    ExynosCamera3FrameFactory *frameFactory = NULL;

    for (int i = 0; i < FRAME_FACTORY_TYPE_MAX; i++) {
        if (m_frameFactory[i] != NULL) {
            frameFactory = m_frameFactory[i];

            for (int k = i + 1; k < FRAME_FACTORY_TYPE_MAX; k++) {
               if (frameFactory == m_frameFactory[k]) {
                   CLOGD("DEBUG(%s[%d]):m_frameFactory index(%d) and index(%d) are same instance, "
                        "set index(%d) = NULL", __FUNCTION__, __LINE__, i, k, k);
                   m_frameFactory[k] = NULL;
               }
            }

            if (m_frameFactory[i]->isCreated() == true) {
                ret = m_frameFactory[i]->destroy();
                if (ret < 0)
                    CLOGE("ERR(%s[%d]):m_frameFactory[%d] destroy fail", __FUNCTION__, __LINE__, i);

                SAFE_DELETE(m_frameFactory[i]);

                CLOGD("DEBUG(%s[%d]):m_frameFactory[%d] destroyed", __FUNCTION__, __LINE__, i);
            }
        }
    }

    CLOGI("INFO(%s[%d]):-OUT-", __FUNCTION__, __LINE__);

    return ret;

}

status_t ExynosCamera3::m_setSetfile(void) {
    int configMode = m_parameters->getConfigMode();
    int setfile = 0;
    int setfileReprocessing = 0;
    int yuvRange = YUV_FULL_RANGE;
    int yuvRangeReprocessing = YUV_FULL_RANGE;

    switch (configMode) {
    case CONFIG_MODE::NORMAL:
        if (m_parameters->getDualMode()) {
            setfile = ISS_SUB_SCENARIO_DUAL_STILL;
            setfileReprocessing = ISS_SUB_SCENARIO_STILL_CAPTURE;
        } else {
            if (getCameraId() == CAMERA_ID_FRONT) {
#if FRONT_CAMERA_USE_SAMSUNG_COMPANION
                if (!m_parameters->getUseCompanion())
                    setfile = ISS_SUB_SCENARIO_FRONT_C2_OFF_STILL_PREVIEW;
                else
#endif
                     setfile = ISS_SUB_SCENARIO_STILL_PREVIEW;
            } else {
                setfile = ISS_SUB_SCENARIO_STILL_PREVIEW;
            }
            setfileReprocessing = ISS_SUB_SCENARIO_STILL_CAPTURE;
        }
        yuvRange = YUV_FULL_RANGE;
        break;
    case CONFIG_MODE::HIGHSPEED_120:
        setfile = ISS_SUB_SCENARIO_VIDEO_HIGH_SPEED;
        setfileReprocessing = ISS_SUB_SCENARIO_VIDEO_HIGH_SPEED;
        yuvRange = YUV_LIMITED_RANGE;
        break;
    case CONFIG_MODE::HIGHSPEED_240:
        setfile = ISS_SUB_SCENARIO_FHD_240FPS;
        setfileReprocessing = ISS_SUB_SCENARIO_VIDEO_HIGH_SPEED;
        yuvRange = YUV_LIMITED_RANGE;
        break;
    default:
        CLOGE("ERROR(%s[%d]):configMode is abnormal(%d)",
            __FUNCTION__, __LINE__,
            configMode);
        break;
    }

    CLOGE("INFO(%s[%d]) configMode(%d), dualMode(%d)",
        __FUNCTION__, __LINE__,
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
    ExynosCameraBufferManager *taaBufferManager[MAX_NODE];
    ExynosCameraBufferManager *ispBufferManager[MAX_NODE];
    ExynosCameraBufferManager *mcscBufferManager[MAX_NODE];
    ExynosCameraBufferManager **tempBufferManager;

    for (int i = 0; i < MAX_NODE; i++) {
        taaBufferManager[i] = NULL;
        ispBufferManager[i] = NULL;
        mcscBufferManager[i] = NULL;
    }

    /* Setting default Bayer DMA-out request flag */
    switch (reprocessingBayerMode) {
    case REPROCESSING_BAYER_MODE_NONE : /* Not using reprocessing */
        CLOGD("DEBUG(%s[%d]): Use REPROCESSING_BAYER_MODE_NONE", __FUNCTION__, __LINE__);
        factory->setRequestFLITE(false);
        factory->setRequest3AC(false);
        break;
    case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON :
        CLOGD("DEBUG(%s[%d]): Use REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON", __FUNCTION__, __LINE__);
        factory->setRequestFLITE(true);
        factory->setRequest3AC(false);
        break;
    case REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON :
        CLOGD("DEBUG(%s[%d]): Use REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON", __FUNCTION__, __LINE__);
        factory->setRequestFLITE(false);
        factory->setRequest3AC(true);
        break;
    case REPROCESSING_BAYER_MODE_PURE_DYNAMIC :
        CLOGD("DEBUG(%s[%d]): Use REPROCESSING_BAYER_MODE_PURE_DYNAMIC", __FUNCTION__, __LINE__);
        factory->setRequestFLITE(false);
        factory->setRequest3AC(false);
        break;
    case REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC :
        CLOGD("DEBUG(%s[%d]): Use REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC", __FUNCTION__, __LINE__);
        factory->setRequestFLITE(false);
        factory->setRequest3AC(false);
        break;
    default :
        CLOGE("ERR(%s[%d]): Unknown dynamic bayer mode", __FUNCTION__, __LINE__);
        factory->setRequest3AC(false);
        break;
    }

    if (m_parameters->isFlite3aaOtf() == false)
        factory->setRequestFLITE(true);

    /* Setting bufferManager based on H/W pipeline */
    tempBufferManager = taaBufferManager;
    pipeId = PIPE_3AA;

    if (m_parameters->getDualMode() == true && getCameraId() == CAMERA_ID_FRONT)
        tempBufferManager[factory->getNodeType(PIPE_3AA)] = m_fliteBufferMgr;
    else
        tempBufferManager[factory->getNodeType(PIPE_3AA)] = m_3aaBufferMgr;

    tempBufferManager[factory->getNodeType(PIPE_3AC)] = m_fliteBufferMgr;
    tempBufferManager[factory->getNodeType(PIPE_3AP)] = m_ispBufferMgr;

    if (m_parameters->is3aaIspOtf() == false)  {
        ret = factory->setBufferManagerToPipe(tempBufferManager, pipeId);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Failed to setBufferManagerToPipe into pipeId %d",
                    __FUNCTION__, __LINE__, pipeId);
            return ret;
        }
        tempBufferManager = ispBufferManager;
        pipeId = PIPE_ISP;
    }

    tempBufferManager[factory->getNodeType(PIPE_ISP)] = m_ispBufferMgr;
    tempBufferManager[factory->getNodeType(PIPE_ISPC)] = m_mcscBufferMgr;
    tempBufferManager[factory->getNodeType(PIPE_ISPP)] = m_mcscBufferMgr;

    if (m_parameters->isIspMcscOtf() == false) {
        ret = factory->setBufferManagerToPipe(tempBufferManager, pipeId);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Failed to setBufferManagerToPipe into pipeId %d",
                    __FUNCTION__, __LINE__, pipeId);
            return ret;
        }
        tempBufferManager = mcscBufferManager;
        pipeId = PIPE_MCSC;
    }

    tempBufferManager[factory->getNodeType(PIPE_MCSC)] = m_mcscBufferMgr;

    /* MCSC0, 1, 2 */
    for (int i = 0; i < m_streamManager->getYuvStreamCount(); i++) {
        streamId = m_streamManager->getYuvStreamId(i);
        ret = m_streamManager->getStream(streamId, &stream);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Failed to getStream. outputPortId %d streamId %d",
                    __FUNCTION__, __LINE__, i, streamId);
            continue;
        }

        ret = stream->getBufferManager(&bufferMgr);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Failed to getBufferMgr. outputPortId %d streamId %d",
                    __FUNCTION__, __LINE__, i, streamId);
            continue;
        }

        tempBufferManager[factory->getNodeType(PIPE_MCSC0 + i)] = bufferMgr;
    }

    ret = factory->setBufferManagerToPipe(tempBufferManager, pipeId);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):Failed to setBufferManagerToPipe into pipeId %d ",
                __FUNCTION__, __LINE__, pipeId);
        return ret;
    }

    /* Setting OutputFrameQ/FrameDoneQ to Pipe */
    pipeId = PIPE_FLITE;
    factory->setOutputFrameQToPipe(m_pipeFrameDoneQ[pipeId], pipeId);

    pipeId = PIPE_3AA;

    if (m_parameters->is3aaIspOtf() == false
            || m_parameters->isIspMcscOtf() == false)
        factory->setFrameDoneQToPipe(m_pipeFrameDoneQ[pipeId], pipeId);

    if (m_parameters->is3aaIspOtf() == false)
        pipeId = PIPE_ISP;

    if (m_parameters->isIspMcscOtf() == false) {
        factory->setFrameDoneQToPipe(m_pipeFrameDoneQ[pipeId], pipeId);
        pipeId = PIPE_MCSC;
    }

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
        ret = factory->setBufferManagerToPipe(tempBufferManager, pipeId);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Failed to setBufferManagerToPipe into pipeId %d",
                    __FUNCTION__, __LINE__, pipeId);
            return ret;
        }
        tempBufferManager = ispBufferManager;
        pipeId = PIPE_ISP_REPROCESSING;
    }

    tempBufferManager[factory->getNodeType(PIPE_ISP_REPROCESSING)] = m_ispReprocessingBufferMgr;
    tempBufferManager[factory->getNodeType(PIPE_ISPC_REPROCESSING)] = m_yuvCaptureBufferMgr;

    if (m_parameters->isReprocessingIspMcscOtf() == false) {
        ret = factory->setBufferManagerToPipe(tempBufferManager, pipeId);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Failed to setBufferManagerToPipe into pipeId %d",
                    __FUNCTION__, __LINE__, pipeId);
            return ret;
        }
        tempBufferManager = mcscBufferManager;
        pipeId = PIPE_MCSC;
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
            CLOGE("ERR(%s[%d]):Failed to getStream from streamMgr. HAL_STREAM_ID_JPEG",
                    __FUNCTION__, __LINE__);

        ret = stream->getBufferManager(&bufferMgr);
        if (ret != NO_ERROR)
            CLOGE("ERR(%s[%d]):Failed to getBufferMgr. HAL_STREAM_ID_JPEG",
                    __FUNCTION__, __LINE__);

        tempBufferManager[factory->getNodeType(PIPE_HWFC_JPEG_DST_REPROCESSING)] = bufferMgr;
    }

    ret = factory->setBufferManagerToPipe(tempBufferManager, pipeId);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):Failed to setBufferManagerToPipe into pipeId %d",
                __FUNCTION__, __LINE__, pipeId);
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

    m_parameters->getMaxSensorSize(&sensorMaxW, &sensorMaxH);

    shot_ext->shot.ctl.scaler.cropRegion[0] = ALIGN_DOWN(shot_ext->shot.ctl.scaler.cropRegion[0], 2);
    shot_ext->shot.ctl.scaler.cropRegion[1] = ALIGN_DOWN(shot_ext->shot.ctl.scaler.cropRegion[1], 2);
    shot_ext->shot.ctl.scaler.cropRegion[2] = ALIGN_UP(shot_ext->shot.ctl.scaler.cropRegion[2], 2);
    shot_ext->shot.ctl.scaler.cropRegion[3] = ALIGN_UP(shot_ext->shot.ctl.scaler.cropRegion[3], 2);

    /* 1. Check the validation of the crop size(width x height).
     * The crop size must be smaller than sensor max size.
     */
    if (sensorMaxW < (int) shot_ext->shot.ctl.scaler.cropRegion[2]
        || sensorMaxH < (int)shot_ext->shot.ctl.scaler.cropRegion[3]) {
        CLOGE("ERR(%s[%d]):Invalid Crop Size(%d, %d), sensorMax(%d, %d)",
                __FUNCTION__, __LINE__,
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
        CLOGE("ERR(%s[%d]):Invalid Crop Region, offsetX(%d), width(%d) sensorMaxW(%d)",
                __FUNCTION__, __LINE__,
                shot_ext->shot.ctl.scaler.cropRegion[0],
                shot_ext->shot.ctl.scaler.cropRegion[2],
                sensorMaxW);
        shot_ext->shot.ctl.scaler.cropRegion[0] = sensorMaxW - shot_ext->shot.ctl.scaler.cropRegion[2];
    }

    if (sensorMaxH < (int) shot_ext->shot.ctl.scaler.cropRegion[1]
            + (int) shot_ext->shot.ctl.scaler.cropRegion[3]) {
        CLOGE("ERR(%s[%d]):Invalid Crop Region, offsetY(%d), height(%d) sensorMaxH(%d)",
                __FUNCTION__, __LINE__,
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

status_t ExynosCamera3::m_updateJpegControlInfo(const struct camera2_shot_ext *shot_ext)
{
    status_t ret = NO_ERROR;

    if (shot_ext == NULL) {
        CLOGE("ERR(%s[%d]):shot_ext is NULL", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    ret = m_parameters->checkJpegQuality(shot_ext->shot.ctl.jpeg.quality);
    if (ret != NO_ERROR)
        CLOGE("ERR(%s[%d]):Failed to checkJpegQuality. quality %d",
                __FUNCTION__, __LINE__, shot_ext->shot.ctl.jpeg.quality);

    ret = m_parameters->checkThumbnailSize(
            shot_ext->shot.ctl.jpeg.thumbnailSize[0],
            shot_ext->shot.ctl.jpeg.thumbnailSize[1]);
    if (ret != NO_ERROR)
        CLOGE("ERR(%s[%d]):Failed to checkThumbnailSize. size %dx%d",
                __FUNCTION__, __LINE__,
                shot_ext->shot.ctl.jpeg.thumbnailSize[0],
                shot_ext->shot.ctl.jpeg.thumbnailSize[1]);

    ret = m_parameters->checkThumbnailQuality(shot_ext->shot.ctl.jpeg.thumbnailQuality);
    if (ret != NO_ERROR)
        CLOGE("ERR(%s[%d]):Failed to checkThumbnailQuality. quality %d",
                __FUNCTION__, __LINE__,
                shot_ext->shot.ctl.jpeg.thumbnailQuality);

    return ret;
}

status_t ExynosCamera3::m_generateFrame(int32_t frameCount, ExynosCamera3FrameFactory *factory, List<ExynosCameraFrame *> *list, Mutex *listLock, ExynosCameraFrame **newFrame)
{
    status_t ret = OK;
    *newFrame = NULL;

    CLOGV("DEBUG(%s[%d]):(%d)", __FUNCTION__, __LINE__, frameCount);
    if (frameCount >= 0) {
        ret = m_searchFrameFromList(list, listLock, frameCount, newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):searchFrameFromList fail", __FUNCTION__, __LINE__);
            return INVALID_OPERATION;
        }
    }

    if (*newFrame == NULL) {
        *newFrame = factory->createNewFrame(frameCount);
        if (*newFrame == NULL) {
            CLOGE("ERR(%s):newFrame is NULL", __FUNCTION__);
            return UNKNOWN_ERROR;
        }
        listLock->lock();
        list->push_back(*newFrame);
        listLock->unlock();
    }

    return ret;
}

status_t ExynosCamera3::m_setupEntity(uint32_t pipeId, ExynosCameraFrame *newFrame, ExynosCameraBuffer *srcBuf, ExynosCameraBuffer *dstBuf)
{
    status_t ret = OK;
    entity_buffer_state_t entityBufferState;

    CLOGV("DEBUG(%s[%d]):(%d)", __FUNCTION__, __LINE__, pipeId);
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
        CLOGE("ERR(%s[%d]):setEntityState(ENTITY_STATE_PROCESSING) fail, pipeId(%d), ret(%d)",
            __FUNCTION__, __LINE__, pipeId, ret);
        return ret;
    }

    return ret;
}

status_t ExynosCamera3::m_setSrcBuffer(uint32_t pipeId, ExynosCameraFrame *newFrame, ExynosCameraBuffer *buffer)
{
    status_t ret = OK;
    int bufIndex = -1;
    ExynosCameraBufferManager *bufferMgr = NULL;
    ExynosCameraBuffer srcBuf;

    CLOGV("DEBUG(%s[%d]):(%d)", __FUNCTION__, __LINE__, pipeId);
    if (buffer == NULL) {
        buffer = &srcBuf;

        ret = m_getBufferManager(pipeId, &bufferMgr, SRC_BUFFER_DIRECTION);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getBufferManager(SRC) fail, pipeId(%d), ret(%d)",
                __FUNCTION__, __LINE__, pipeId, ret);
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
            return ret;
        }
    }

    /* set buffers */
    ret = newFrame->setSrcBuffer(pipeId, *buffer);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setSrcBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
        return ret;
    }

    return ret;
}

status_t ExynosCamera3::m_setDstBuffer(uint32_t pipeId, ExynosCameraFrame *newFrame, ExynosCameraBuffer *buffer)
{
    status_t ret = OK;
    int bufIndex = -1;
    ExynosCameraBufferManager *bufferMgr = NULL;
    ExynosCameraBuffer dstBuf;

    CLOGV("DEBUG(%s[%d]):(%d)", __FUNCTION__, __LINE__, pipeId);
    if (buffer == NULL) {
        buffer = &dstBuf;

        ret = m_getBufferManager(pipeId, &bufferMgr, DST_BUFFER_DIRECTION);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getBufferManager(DST) fail, pipeId(%d), ret(%d)",
                __FUNCTION__, __LINE__, pipeId, ret);
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
            return ret;
        }
    }

    /* set buffers */
    ret = newFrame->setDstBuffer(pipeId, *buffer);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
        return ret;
    }

    return ret;
}

status_t ExynosCamera3::m_setSrcBuffer(uint32_t pipeId, ExynosCameraFrame *newFrame, ExynosCameraBuffer *buffer, ExynosCameraBufferManager *bufMgr)
{
    status_t ret = OK;
    int bufIndex = -1;
    ExynosCameraBuffer srcBuf;

    CLOGD("DEBUG(%s[%d]):(%d)", __FUNCTION__, __LINE__, pipeId);
    if (bufMgr == NULL) {

        ret = m_getBufferManager(pipeId, &bufMgr, SRC_BUFFER_DIRECTION);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getBufferManager(SRC) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
            return ret;
        }

        if (bufMgr == NULL) {
            CLOGE("ERR(%s[%d]):buffer manager is NULL, pipeId(%d)", __FUNCTION__, __LINE__, pipeId);
            return BAD_VALUE;
        }

    }

    /* get buffers */
    ret = bufMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, buffer);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getBuffer fail, pipeId(%d), frameCount(%d), ret(%d)",
            __FUNCTION__, __LINE__, pipeId, newFrame->getFrameCount(), ret);
        return ret;
    }

    /* set buffers */
    ret = newFrame->setSrcBuffer(pipeId, *buffer);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setSrcBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
        return ret;
    }

    return ret;
}

status_t ExynosCamera3::m_setDstBuffer(uint32_t pipeId, ExynosCameraFrame *newFrame, ExynosCameraBuffer *buffer, ExynosCameraBufferManager *bufMgr)
{
    status_t ret = OK;
    int bufIndex = -1;
    ExynosCameraBuffer dstBuf;

    CLOGD("DEBUG(%s[%d]):(%d)", __FUNCTION__, __LINE__, pipeId);
    if (bufMgr == NULL) {

        ret = m_getBufferManager(pipeId, &bufMgr, DST_BUFFER_DIRECTION);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getBufferManager(DST) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
            return ret;
        }

        if (bufMgr == NULL) {
            CLOGE("ERR(%s[%d]):buffer manager is NULL, pipeId(%d)", __FUNCTION__, __LINE__, pipeId);
            return BAD_VALUE;
        }
    }

    /* get buffers */
    ret = bufMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, buffer);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getBuffer fail, pipeId(%d), frameCount(%d), ret(%d)",
            __FUNCTION__, __LINE__, pipeId, newFrame->getFrameCount(), ret);
        return ret;
    }

    /* set buffers */
    ret = newFrame->setDstBuffer(pipeId, *buffer);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
        return ret;
    }

    return ret;
}

/* This function reset buffer state of pipeId.
 * Some pipes are shared by multi stream. In this case, we need reset buffer for using PIPE again.
 */
status_t ExynosCamera3::m_resetBufferState(uint32_t pipeId, ExynosCameraFrame *frame)
{
    status_t ret = NO_ERROR;
    entity_buffer_state_t bufState = ENTITY_BUFFER_STATE_NOREQ;

    if (frame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        ret = BAD_VALUE;
        goto ERR;
    }

    ret = frame->getSrcBufferState(pipeId, &bufState);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getSrcBufferState fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
        goto ERR;
    }

    if (bufState != ENTITY_BUFFER_STATE_NOREQ && bufState != ENTITY_BUFFER_STATE_INVALID) {
        frame->setSrcBufferState(pipeId, ENTITY_BUFFER_STATE_REQUESTED);
    } else {
        CLOGW("WARN(%s[%d]):SrcBufferState is not COMPLETE, fail to reset buffer state,"
            "pipeId(%d), state(%d)", __FUNCTION__, __LINE__, pipeId, bufState);
        ret = INVALID_OPERATION;
        goto ERR;
    }


    ret = frame->getDstBufferState(pipeId, &bufState);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getDstBufferState fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
        goto ERR;
    }

    if (bufState != ENTITY_BUFFER_STATE_NOREQ && bufState != ENTITY_BUFFER_STATE_INVALID) {
        ret = frame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_REQUESTED);
        if (ret != NO_ERROR)
            CLOGE("ERR(%s[%d]):setDstBufferState fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
    } else {
        CLOGW("WARN(%s[%d]):DstBufferState is not COMPLETE, fail to reset buffer state,"
            "pipeId(%d), state(%d)", __FUNCTION__, __LINE__, pipeId, bufState);
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
        bufMgrList[0] = NULL;
        bufMgrList[1] = &m_fliteBufferMgr;
        break;
    case PIPE_3AA:
        bufMgrList[0] = &m_3aaBufferMgr;
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
        CLOGE("ERR(%s[%d]):Unknown pipeId(%d)", __FUNCTION__, __LINE__, pipeId);
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
        CLOGI("INFO(%s[%d]):try(%d) to create IonAllocator", __FUNCTION__, __LINE__, retry);
        *allocator = new ExynosCameraIonAllocator();
        ret = (*allocator)->init(false);
        if (ret < 0)
            CLOGE("ERR(%s[%d]):create IonAllocator fail (retryCount=%d)", __FUNCTION__, __LINE__, retry);
        else {
            CLOGD("DEBUG(%s[%d]):m_createIonAllocator success (allocator=%p)", __FUNCTION__, __LINE__, *allocator);
            break;
        }
    } while ((ret < 0) && (retry < 3));

    if ((ret < 0) && (retry >=3)) {
        CLOGE("ERR(%s[%d]):create IonAllocator fail (retryCount=%d)", __FUNCTION__, __LINE__, retry);
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
            CLOGE("ERR(%s[%d]):m_createIonAllocator fail", __FUNCTION__, __LINE__);
        else
            CLOGD("DEBUG(%s[%d]):m_createIonAllocator success", __FUNCTION__, __LINE__);
    }

    *bufferManager = ExynosCameraBufferManager::createBufferManager(type);
    (*bufferManager)->create(name, m_cameraId, m_ionAllocator);

    CLOGD("DEBUG(%s):BufferManager(%s) created", __FUNCTION__, name);

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

bool ExynosCamera3::m_selectBayerThreadFunc()
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = NO_ERROR;
    ExynosCameraFrame *frame = NULL;
    ExynosCameraBuffer bayerBuffer;
    ExynosCameraBufferManager *bufferMgr = NULL;
    camera2_shot_ext *shot_ext = NULL;
    camera2_shot_ext updateDmShot;
    uint32_t pipeID = 0;

    ret = m_selectBayerQ->waitAndPopProcessQ(&frame);
    if (ret != NO_ERROR) {
        if (ret == TIMED_OUT) {
            CLOGW("WANR(%s[%d]):Wait timeout",
                    __FUNCTION__, __LINE__);
        } else {
            CLOGE("ERR(%s[%d]):Failed to waitAndPopProcessQ. ret %d",
                    __FUNCTION__, __LINE__, ret);
        }
        goto CLEAN;
    } else if (frame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL!!",
                __FUNCTION__, __LINE__);
        goto CLEAN;
    }

    if (frame->getFrameCapture() == false) {
        CLOGW("WARN(%s[%d]):frame is not capture frame. frameCount %d",
                __FUNCTION__, __LINE__, frame->getFrameCount());
        goto CLEAN;
    }

    CLOGV("DEBUG(%s[%d]):Start to select Bayer. frameCount %d",
            __FUNCTION__, __LINE__, frame->getFrameCount());

    /* Get bayer buffer based on current reprocessing mode */
    switch(m_parameters->getReprocessingBayerMode()) {
        case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON:
        case REPROCESSING_BAYER_MODE_PURE_DYNAMIC:
            CLOGD("DEBUG(%s[%d]):REPROCESSING_BAYER_MODE_PURE. isRawCapture %d",
                    __FUNCTION__, __LINE__, frame->getFrameServiceBayer());

            if (frame->getFrameZsl() || frame->getFrameServiceBayer())
                ret = m_getBayerServiceBuffer(frame, &bayerBuffer);
            else
                ret = m_getBayerBuffer(m_getBayerPipeId(), frame->getFrameCount(), &bayerBuffer, m_captureSelector);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to get bayer buffer. frameCount %d useServiceBayerBuffer %d",
                        __FUNCTION__, __LINE__, frame->getFrameCount(), frame->getFrameZsl());
                goto CLEAN;
            }

            shot_ext = (struct camera2_shot_ext *)(bayerBuffer.addr[bayerBuffer.planeCount-1]);
            if (shot_ext == NULL) {
                CLOGE("ERR(%s[%d]):shot_ext from pure bayer buffer is NULL", __FUNCTION__, __LINE__);
                break;
            }

            ret = frame->storeDynamicMeta(shot_ext);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):storeDynamicMeta fail ret(%d)", __FUNCTION__, __LINE__, ret);
                goto CLEAN;
            }

            ret = frame->storeUserDynamicMeta(shot_ext);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):storeUserDynamicMeta fail ret(%d)", __FUNCTION__, __LINE__, ret);
                goto CLEAN;
            }

            break;
        case REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON:
        case REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC:
            CLOGD("DEBUG(%s[%d]):REPROCESSING_BAYER_MODE_DIRTY%s. isRawCapture %d",
                    __FUNCTION__, __LINE__,
                    (m_parameters->getReprocessingBayerMode()
                        == REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC) ? "_DYNAMIC" : "_ALWAYS_ON",
                    frame->getFrameServiceBayer());

            if (frame->getFrameZsl()/* || frame->getFrameServiceBayer()*/) {
                ret = m_getBayerServiceBuffer(frame, &bayerBuffer);
                CLOGD("DEBUG(%s[%d]):Processing bayer from serviceBuffer Cnt = %d, planeCount = %d, size[0] = %d",
                        __FUNCTION__, __LINE__, frame->getFrameCount(), bayerBuffer.planeCount, bayerBuffer.size[0]);

                /* Restore the meta plane from the end of free space of bayer buffer
                   which is saved in m_handleBayerBuffer()
                */
                char* metaPlane = bayerBuffer.addr[bayerBuffer.planeCount -1];
                char* bayerSpareArea = bayerBuffer.addr[0] + (bayerBuffer.size[0]-bayerBuffer.size[bayerBuffer.planeCount -1]);

                if(bayerBuffer.addr[0] != NULL && bayerBuffer.planeCount >= 2) {

                    CLOGD("DEBUG(%s[%d]):metaPlane[%p], Len=%d / bayerBuffer[%p], Len=%d / bayerSpareArea[%p]",
                            __FUNCTION__, __LINE__,
                            metaPlane, bayerBuffer.size[bayerBuffer.planeCount -1], bayerBuffer.addr[0], bayerBuffer.size[0], bayerSpareArea);
                    memcpy(metaPlane, bayerSpareArea, bayerBuffer.size[bayerBuffer.planeCount -1]);

                    CLOGD("DEBUG(%s[%d]):shot.dm.request.frameCount : Original : %d / Copied : %d",
                        __FUNCTION__, __LINE__,
                        getMetaDmRequestFrameCount((camera2_shot_ext *)metaPlane), getMetaDmRequestFrameCount((camera2_shot_ext *)bayerSpareArea));
                } else {
                    CLOGE("DEBUG(%s[%d]): Null bayer buffer metaPlane[%p], Len=%d / bayerBuffer[%p], Len=%d / bayerSpareArea[%p]",
                            __FUNCTION__, __LINE__,
                            metaPlane, bayerBuffer.size[bayerBuffer.planeCount -1], bayerBuffer.addr[0], bayerBuffer.size[0], bayerSpareArea);
                }

                shot_ext = (struct camera2_shot_ext *)(bayerBuffer.addr[bayerBuffer.planeCount-1]);
                if (shot_ext == NULL) {
                    CLOGE("ERR(%s[%d]):shot_ext from pure bayer buffer is NULL", __FUNCTION__, __LINE__);
                    break;
                }

                ret = frame->storeDynamicMeta(shot_ext);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):storeDynamicMeta fail ret(%d)", __FUNCTION__, __LINE__, ret);
                    goto CLEAN;
                }

                ret = frame->storeUserDynamicMeta(shot_ext);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):storeUserDynamicMeta fail ret(%d)", __FUNCTION__, __LINE__, ret);
                    goto CLEAN;
                }

            } else {
                ret = m_getBayerBuffer(m_getBayerPipeId(), frame->getFrameCount(), &bayerBuffer, m_captureSelector, &updateDmShot);
                CLOGD("DEBUG(%s[%d]):Processing bayer from internalBuffer Cnt = %d, planeCount = %d, size[0] = %d",
                        __FUNCTION__, __LINE__, frame->getFrameCount(), bayerBuffer.planeCount, bayerBuffer.size[0]);

                ret = frame->storeDynamicMeta(&updateDmShot);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):storeDynamicMeta fail ret(%d)", __FUNCTION__, __LINE__, ret);
                    goto CLEAN;
                }

                ret = frame->storeUserDynamicMeta(&updateDmShot);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):storeUserDynamicMeta fail ret(%d)", __FUNCTION__, __LINE__, ret);
                    goto CLEAN;
                }

                shot_ext = &updateDmShot;
            }

            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to get bayer buffer. frameCount %d useServiceBayerBuffer %d",
                        __FUNCTION__, __LINE__, frame->getFrameCount(), frame->getFrameZsl());
                goto CLEAN;
            }


            break;
        default:
            CLOGE("ERR(%s[%d]):bayer mode is not valid(%d)",
                    __FUNCTION__, __LINE__, m_parameters->getReprocessingBayerMode());
            goto CLEAN;
    }

    CLOGD("DEBUG(%s[%d]):meta_shot_ext->shot.dm.request.frameCount : %d",
            __FUNCTION__, __LINE__,
            getMetaDmRequestFrameCount(shot_ext));

    /* Get pipeId for the first entity in reprocessing frame */
    pipeID = frame->getFirstEntity()->getPipeId();
    CLOGD("DEBUG(%s[%d]):Reprocessing stream first pipe ID %d",
            __FUNCTION__, __LINE__, pipeID);

    /* Check available buffer */
    ret = m_getBufferManager(pipeID, &bufferMgr, DST_BUFFER_DIRECTION);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):Failed to getBufferManager, ret %d",
                __FUNCTION__, __LINE__, ret);
        goto CLEAN;
    } else if (bufferMgr == NULL) {
        CLOGE("ERR(%s[%d]):BufferMgr is NULL. pipeId %d",
                __FUNCTION__, __LINE__, pipeID);
        goto CLEAN;
    }

    ret = m_checkBufferAvailable(pipeID, bufferMgr);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):Waiting buffer timeout, PipeID %d, ret %d",
                __FUNCTION__, __LINE__, pipeID, ret);
        goto CLEAN;
    }

    ret = m_setupEntity(pipeID, frame, &bayerBuffer, NULL);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]:setupEntity fail, bayerPipeId(%d), ret(%d)",
                __FUNCTION__, __LINE__, pipeID, ret);
        goto CLEAN;
    }

    m_captureQ->pushProcessQ(&frame);

    return true;

CLEAN:
    if (frame != NULL) {
        frame->frameUnlock();
        ret = m_removeFrameFromList(&m_captureProcessList, &m_captureProcessLock, frame);
        if (ret != NO_ERROR)
            CLOGE("ERR(%s[%d]):Failed to remove frame from m_captureProcessList. frameCount %d ret %d",
                    __FUNCTION__, __LINE__, frame->getFrameCount(), ret);

        frame->printEntity();
        CLOGD("DEBUG(%s[%d]):Delete frame from m_captureProcessList. frameCount %d",
                __FUNCTION__, __LINE__, frame->getFrameCount());
        frame->decRef();
        m_frameMgr->deleteFrame(frame);
        frame = NULL;
    }
    return true;

}

bool ExynosCamera3::m_captureThreadFunc()
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = NO_ERROR;
    ExynosCameraFrame *frame = NULL;
    ExynosCameraRequest *request = NULL;
    ExynosCamera3FrameFactory *factory = NULL;
    int retryCount = 1;
    uint32_t pipeID = 0;

    ret = m_captureQ->waitAndPopProcessQ(&frame);
    if (ret != NO_ERROR) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGW("WARN(%s):Wait timeout", __FUNCTION__);
        } else {
            CLOGE("ERR(%s):Failed to wait&pop captureQ, ret %d", __FUNCTION__, ret);
            /* TODO: doing exception handling */
        }
        goto CLEAN;
    } else if (frame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL!!", __FUNCTION__, __LINE__);
        goto CLEAN;
    }

    m_captureStreamThread->run(PRIORITY_DEFAULT);

    CLOGV("DEBUG(%s[%d]):frame frameCount(%d)", __FUNCTION__, __LINE__, frame->getFrameCount());

    factory = m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING];
    pipeID = frame->getFirstEntity()->getPipeId();

    while(factory->isRunning() == false) {
        CLOGD("DEBUG(%s[%d]):Wait to start reprocessing stream",
                __FUNCTION__, __LINE__);
        usleep(5000);
    }

    factory->pushFrameToPipe(&frame, pipeID);

    /* When enabled SCC capture or pureBayerReprocessing, we need to start bayer pipe thread */
    if (m_parameters->isReprocessing() == true)
        factory->startThread(pipeID);

    /* Wait reprocesisng done */
    CLOGI("INFO(%s[%d]):Wait reprocessing done. frameCount %d",
            __FUNCTION__, __LINE__, frame->getFrameCount());
    do {
        ret = m_reprocessingDoneQ->waitAndPopProcessQ(&frame);
    } while (ret == TIMED_OUT && retryCount-- > 0);

    if (ret != NO_ERROR) {
        CLOGW("WARN(%s[%d]):Failed to waitAndPopProcessQ to reprocessingDoneQ. ret %d",
                __FUNCTION__, __LINE__, ret);
    }

    /* Thread can exit only if timeout or error occured from inputQ (at CLEAN: label) */
    return true;

CLEAN:
    if (frame != NULL) {
        frame->frameUnlock();
        ret = m_removeFrameFromList(&m_captureProcessList, &m_captureProcessLock, frame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):remove frame from m_captureProcessList fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }

        frame->printEntity();
        CLOGD("DEBUG(%s[%d]):m_captureProcessList frame delete(%d)", __FUNCTION__, __LINE__, frame->getFrameCount());
        frame->decRef();
        m_frameMgr->deleteFrame(frame);
        frame = NULL;
    }

    if (m_captureQ->getSizeOfProcessQ() > 0)
        return true;
    else
        return false;
}

status_t ExynosCamera3::m_getBayerServiceBuffer(ExynosCameraFrame *frame, ExynosCameraBuffer *buffer)
{
    status_t ret = NO_ERROR;
    ExynosCameraRequest *request = NULL;
    ExynosCameraBufferManager *bufferMgr = NULL;
    ExynosCameraFrame *bayerFrame = NULL;
    int retryCount = 30;

    if (frame->getFrameServiceBayer() == true) {
        m_captureZslSelector->setWaitTime(200000000);
        bayerFrame = m_captureZslSelector->selectDynamicFrames(1, m_getBayerPipeId(), false, retryCount, 0);
        if (bayerFrame == NULL) {
            CLOGE("ERR(%s[%d]):bayerFrame is NULL", __FUNCTION__, __LINE__);
            return INVALID_OPERATION;
        }

        if (bayerFrame->getMetaDataEnable() == true)
            CLOGI("INFO(%s[%d]):Frame metadata is updated. frameCount %d",
                    __FUNCTION__, __LINE__, bayerFrame->getFrameCount());

        ret = bayerFrame->getDstBuffer(m_getBayerPipeId(), buffer);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Failed to getDstBuffer. pipeId %d ret %d",
                    __FUNCTION__, __LINE__,
                    m_getBayerPipeId(), ret);
            return ret;
        }
    } else {
        request = m_requestMgr->getServiceRequest(frame->getFrameCount());
        if (request != NULL) {
            camera3_stream_buffer_t *stream_buffer = request->getInputBuffer();
            buffer_handle_t *handle = stream_buffer->buffer;
            int bufIndex = -1;
            CLOGI("INFO(%s[%d]): Getting Bayer from getServiceRequest(fcount:%d / handle:%p)", __FUNCTION__, __LINE__, frame->getFrameCount(), handle);

            m_zslBufferMgr->getIndexByHandle(handle, &bufIndex);
            if (bufIndex < 0) {
                CLOGE("ERR(%s[%d]): getIndexByHandle is fail(fcount:%d / handle:%p)",
                    __FUNCTION__, __LINE__, frame->getFrameCount(), handle);
                ret = BAD_VALUE;
            } else {
                ret = m_zslBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_SERVICE, buffer);
                CLOGI("INFO(%s[%d]): service bayer selected(fcount:%d / handle:%p / idx:%d / ret:%d)", __FUNCTION__, __LINE__,
                        frame->getFrameCount(), handle, bufIndex, ret);
            }
        } else {
            CLOGE("ERR(%s[%d]): request if NULL(fcount:%d)", __FUNCTION__, __LINE__, frame->getFrameCount());
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
    ExynosCameraFrame *inListFrame = NULL;
    ExynosCameraFrame *bayerFrame = NULL;

    if (m_parameters->isReprocessing() == false || selector == NULL) {
        CLOGE("ERR(%s[%d]):INVALID_OPERATION, isReprocessing(%s) or bayerFrame is NULL",
                __FUNCTION__, __LINE__, m_parameters->isReprocessing() ? "True" : "False");
        ret = INVALID_OPERATION;
        goto CLEAN;
    }

    selector->setWaitTime(200000000);
    bayerFrame = selector->selectCaptureFrames(1, frameCount, pipeId, isSrc, retryCount, 0);
    if (bayerFrame == NULL) {
        CLOGE("ERR(%s[%d]):bayerFrame is NULL", __FUNCTION__, __LINE__);
        ret = INVALID_OPERATION;
        goto CLEAN;
    }

    ret = bayerFrame->getDstBuffer(pipeId, buffer);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
        goto CLEAN;
    }

    if (m_parameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON ||
        m_parameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_PURE_DYNAMIC) {
        shot_ext = (struct camera2_shot_ext *)buffer->addr[buffer->planeCount - 1];
        CLOGD("DEBUG(%s[%d]):Selected frame count(hal : %d / driver : %d)", __FUNCTION__, __LINE__,
            bayerFrame->getFrameCount(), shot_ext->shot.dm.request.frameCount);

        if (bayerFrame->getMetaDataEnable() == true)
            CLOGI("INFO(%s[%d]):Frame metadata is updated. frameCount %d",
                    __FUNCTION__, __LINE__, bayerFrame->getFrameCount());

        ret = bayerFrame->getMetaData(shot_ext);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Failed to getMetaData from frame. frameCount %d",
                    __FUNCTION__, __LINE__, bayerFrame->getFrameCount());
            goto CLEAN;
        }
    } else if (m_parameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON ||
        m_parameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC) {
        if (updateDmShot == NULL) {
            CLOGE("ERR(%s[%d]):updateDmShot is NULL", __FUNCTION__, __LINE__);
            goto CLEAN;
        }

        while(retryCount > 0) {
            if(bayerFrame->getMetaDataEnable() == false) {
                CLOGD("DEBUG(%s[%d]):Waiting for update jpeg metadata failed (%d), retryCount(%d)",
                    __FUNCTION__, __LINE__, ret, retryCount);
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
        CLOGD("DEBUG(%s[%d]):Selected fcount(hal : %d / driver : %d)", __FUNCTION__, __LINE__,
            bayerFrame->getFrameCount(), shot_stream->fcount);
    } else {
        CLOGE("ERR(%s[%d]):reprocessing is not valid pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
        goto CLEAN;
    }

CLEAN:

    if (bayerFrame != NULL) {
        bayerFrame->frameUnlock();

        ret = m_searchFrameFromList(&m_processList, &m_processLock, bayerFrame->getFrameCount(), &inListFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):searchFrameFromList fail", __FUNCTION__, __LINE__);
        } else {
            CLOGD("DEBUG(%s[%d]):Selected frame(%d) complete, Delete",
                __FUNCTION__, __LINE__, bayerFrame->getFrameCount());
            bayerFrame->decRef();
            m_frameMgr->deleteFrame(bayerFrame);
            bayerFrame = NULL;
        }
    }

    return ret;
}


bool ExynosCamera3::m_captureStreamThreadFunc(void)
{
    status_t ret = 0;
    ExynosCameraFrame *frame = NULL;
    ExynosCameraFrameEntity *entity = NULL;
    ExynosCameraBuffer buffer;
    ExynosCameraStream *stream = NULL;
    camera3_stream_buffer_t streamBuffer;
    ExynosCameraRequest* request = NULL;
    ResultRequest resultRequest = NULL;
    int bayerPipeId = 0;
    struct camera2_shot_ext *shot_ext = NULL;

    ret = m_yuvCaptureDoneQ->waitAndPopProcessQ(&frame);
    if (ret != NO_ERROR) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGW("WARN(%s):wait timeout", __FUNCTION__);
        } else {
            CLOGE("ERR(%s):Failed to wait&pop m_yuvCaptureDoneQ, ret(%d)", __FUNCTION__, ret);
            /* TODO: doing exception handling */
        }
        goto FUNC_EXIT;
    }

    if (frame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        goto FUNC_EXIT;
    }

    entity = frame->getFrameDoneEntity();
    if (entity == NULL) {
        CLOGE("ERR(%s[%d]):Current entity is NULL", __FUNCTION__, __LINE__);
        /* TODO: doing exception handling */
        goto FUNC_EXIT;
    }

    CLOGD("DEBUG(%s[%d]):Yuv capture done. framecount %d entityID %d",
            __FUNCTION__, __LINE__, frame->getFrameCount(), entity->getPipeId());

    switch(entity->getPipeId()) {
    case PIPE_3AA_REPROCESSING:
    case PIPE_ISP_REPROCESSING:
        ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Failed to getSrcBuffer, pipeId %d, ret %d",
                    __FUNCTION__, __LINE__, entity->getPipeId(), ret);
            goto FUNC_EXIT;
        }

        shot_ext = (struct camera2_shot_ext *) buffer.addr[buffer.planeCount - 1];
        if (shot_ext == NULL) {
            CLOGE("ERR(%s[%d]):shot_ext is NULL. pipeId %d frameCount %d",
                    __FUNCTION__, __LINE__,
                    entity->getPipeId(), frame->getFrameCount());
            goto FUNC_EXIT;
        }
#ifdef CORRECT_TIMESTAMP_FOR_SENSORFUSION
        /* m_adjustTimestamp() is to do adjust timestamp for ITS/sensorFusion. */
        /* This function should be called once at here */
        ret = m_adjustTimestamp(&buffer);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Failed to adjust timestamp", __FUNCTION__, __LINE__);
            goto FUNC_EXIT;
        }
#endif

        if (m_parameters->getUsePureBayerReprocessing() == true) {
            ret = m_pushResult(frame->getFrameCount(), shot_ext);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to pushResult. framecount %d ret %d",
                        __FUNCTION__, __LINE__,
                        frame->getFrameCount(), ret);
                goto FUNC_EXIT;
            }
        } else {
            // In dirty bayer case, the meta is updated if the current request
            // is reprocessing only(i.e. Internal frame is created on preview path).
            // Preview path will update the meta if the current request have preview frame
            ExynosCameraRequest* request = m_requestMgr->getServiceRequest(frame->getFrameCount());
            if (request == NULL) {
                CLOGE("ERR(%s[%d]):getServiceRequest failed ",__FUNCTION__, __LINE__);
            } else {
                if(request->getNeedInternalFrame() == true) {
                    ret = m_pushResult(frame->getFrameCount(), shot_ext);
                    if (ret != NO_ERROR) {
                        CLOGE("ERR(%s[%d]):Failed to pushResult. framecount %d ret %d",
                                __FUNCTION__, __LINE__, frame->getFrameCount(), ret);
                        goto FUNC_EXIT;
                    }
                }
            }
        }

        ret = frame->storeDynamicMeta(shot_ext);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Failed to storeUserDynamicMeta, requestKey %d, ret %d",
                    __FUNCTION__, __LINE__, request->getKey(), ret);
            goto FUNC_EXIT;
        }

        ret = frame->storeUserDynamicMeta(shot_ext);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Failed to storeUserDynamicMeta, requestKey %d, ret %d",
                    __FUNCTION__, __LINE__, request->getKey(), ret);
            goto FUNC_EXIT;
        }

        CLOGV("DEBUG(%s[%d]):3AA_REPROCESSING Done. dm.request.frameCount %d frameCount %d",
                __FUNCTION__, __LINE__,
                getMetaDmRequestFrameCount((struct camera2_shot_ext *)buffer.addr[buffer.planeCount-1]),
                frame->getFrameCount());

        /* Return bayer buffer, except the buffer came from the service. */
        if ( (frame->getFrameZsl() == false) && (frame->getFrameServiceBayer() == false) ) {
            ret = m_putBuffers(m_fliteBufferMgr, buffer.index);
            if (ret != NO_ERROR)
                CLOGE("ERR(%s[%d]):Failed to putBuffer to fliteBufferMgr, bufferIndex %d",
                        __FUNCTION__, __LINE__, buffer.index);
        }

#if !defined(ENABLE_FULL_FRAME)
        {
            ExynosCameraRequest* request = m_requestMgr->getServiceRequest(frame->getFrameCount());
            /* try to notify if notify callback was not called in same framecount */
            if (request->getCallbackDone(EXYNOS_REQUEST_RESULT::CALLBACK_NOTIFY_ONLY) == false) {
                /* Check whether the shutter callback can be made on preview pipe */
                if (request->getNeedInternalFrame() == true)
                    m_sendNotify(frame->getFrameCount(), CAMERA3_MSG_SHUTTER);
            }
        }
#endif
        if(m_parameters->isSingleChain()) {
            if(frame->getRequest(PIPE_MCSC2_REPROCESSING) == true) {
                /* Request for ZSL YUV reprocessing */
                ret = m_generateDuplicateBuffers(frame, entity->getPipeId());
                if (ret != NO_ERROR)
                    CLOGE("ERR(%s[%d]):m_generateDuplicateBuffers failed. frameCnt = %d / ret = %d %d",
                            __FUNCTION__, __LINE__, frame->getFrameCount(), ret);
            }
        }

        /* Handle yuv capture buffer */
        if(frame->getRequest(PIPE_MCSC3_REPROCESSING) == true) {
            ret = m_handleYuvCaptureFrame(frame);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to handleYuvCaptureFrame. pipeId %d ret %d",
                        __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                return ret;
            }
        }

        /* Continue to JPEG processing stage in HWFC mode */
        if(m_parameters->isUseHWFC() == false) {
            break;
        }

    case PIPE_JPEG_REPROCESSING:
        /* Handle JPEG buffer */
        if(frame->getRequest(PIPE_MCSC3_REPROCESSING) == true) {
            ret = m_handleJpegFrame(frame);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to handleJpegFrame. pipeId %d ret %d",
                        __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                return ret;
            }
        }
        break;
    default:
        CLOGE("ERR(%s[%d]):Invalid pipeId %d",
                __FUNCTION__, __LINE__, entity->getPipeId());
        break;
    }

    ret = frame->setEntityState(entity->getPipeId(), ENTITY_STATE_COMPLETE);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setEntityState fail, pipeId(%d), state(%d), ret(%d)",
            __FUNCTION__, __LINE__, entity->getPipeId(), ENTITY_STATE_COMPLETE, ret);
        return ret;
    }

    if (frame->isComplete() == true) {
        List<ExynosCameraFrame *> *list = NULL;
        Mutex *listLock = NULL;
#if defined(ENABLE_FULL_FRAME)
        list = &m_processList;
        listLock = &m_processLock;
#else
        list = &m_captureProcessList;
        listLock = &m_captureProcessLock;
#endif
        // TODO:decide proper position
        CLOGV("DEBUG(%s[%d]):frame complete. framecount %d", __FUNCTION__, __LINE__, frame->getFrameCount());
        ret = m_removeFrameFromList(list, listLock, frame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):remove frame from processList fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }

        frame->decRef();
        m_frameMgr->deleteFrame(frame);
        frame = NULL;
    }
FUNC_EXIT:
        Mutex::Autolock l(m_captureProcessLock);
        if (m_captureProcessList.size() > 0)
            return true;
        else
            return false;
}

status_t ExynosCamera3::m_handleYuvCaptureFrame(ExynosCameraFrame *frame)
{
    status_t ret = NO_ERROR;
    ExynosCameraRequest* request = NULL;
    ExynosCameraBufferManager *bufferMgr = NULL;
    ExynosCameraBuffer srcBuffer;
    ExynosCameraBuffer dstBuffer;
    int bufIndex = -1;
    ExynosCamera3FrameFactory *factory = NULL;
    int pipeId_src = -1;
    int pipeId_gsc = -1;
    int pipeId_jpeg = -1;

    bool isSrc = false;
    float zoomRatio = 0.0F;
    struct camera2_stream *shot_stream = NULL;
    int pictureW = 0, pictureH = 0, pictureFormat = 0;
    ExynosRect srcRect, dstRect;

    request = m_requestMgr->getServiceRequest(frame->getFrameCount());
    factory = request->getFrameFactory(HAL_STREAM_ID_JPEG);

    if (m_parameters->isUseHWFC() == true) {
        ret = frame->getDstBuffer(PIPE_3AA_REPROCESSING, &dstBuffer, factory->getNodeType(PIPE_MCSC3_REPROCESSING));
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Failed to getDstBuffer. pipeId %d node %d ret %d",
                    __FUNCTION__, __LINE__,
                    PIPE_3AA_REPROCESSING, PIPE_MCSC3_REPROCESSING, ret);
            return INVALID_OPERATION;
        }

        ret = m_putBuffers(m_yuvCaptureBufferMgr, dstBuffer.index);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Failed to putBuffer to m_yuvCaptureBufferMgr. bufferIndex %d",
                    __FUNCTION__, __LINE__, dstBuffer.index);
            return INVALID_OPERATION;
        }
#if 0 /* TODO : Why this makes error? */
        ret = frame->getDstBuffer(PIPE_3AA_REPROCESSING, &dstBuffer, factory->getNodeType(PIPE_MCSC4_REPROCESSING));
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Failed to getDstBuffer. pipeId %d node %d ret %d",
                    __FUNCTION__, __LINE__,
                    PIPE_3AA_REPROCESSING, PIPE_MCSC4_REPROCESSING, ret);
            return INVALID_OPERATION;
        }

        ret = m_putBuffers(m_thumbnailBufferMgr, dstBuffer.index);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Failed to putBuffer to m_thumbnailBufferMgr. bufferIndex %d",
                    __FUNCTION__, __LINE__, dstBuffer.index);
            return INVALID_OPERATION;
        }
#endif
    } else {
        zoomRatio = m_parameters->getZoomRatio(m_parameters->getZoomLevel()) / 1000;

        if (m_parameters->isReprocessing() == true) {
            /* We are using only PIPE_ISP_REPROCESSING */
            pipeId_src = PIPE_ISP_REPROCESSING;
            pipeId_gsc = PIPE_GSC_REPROCESSING;
            pipeId_jpeg = PIPE_JPEG_REPROCESSING;
            isSrc = true;
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
                    CLOGE("ERR(%s[%d]):Current picture mode is not yet supported, CameraId(%d), reprocessing(%d)",
                            __FUNCTION__, __LINE__, getCameraId(), m_parameters->isReprocessing());
                    break;
            }
            pipeId_jpeg = PIPE_JPEG;
#endif
        }
        ///////////////////////////////////////////////////////////

        // Thumbnail image is currently not used
        ret = frame->getDstBuffer(pipeId_src, &dstBuffer, factory->getNodeType(PIPE_MCSC4_REPROCESSING));
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Failed to getDstBuffer. pipeId %d node %d ret %d",
                    __FUNCTION__, __LINE__,
                    pipeId_src, PIPE_MCSC4_REPROCESSING, ret);
        } else {
            ret = m_putBuffers(m_thumbnailBufferMgr, dstBuffer.index);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to putBuffer to m_thumbnailBufferMgr. bufferIndex %d",
                        __FUNCTION__, __LINE__, dstBuffer.index);
            }
            CLOGI("INFO(%s[%d]): Thumbnail image disposed at pipeId %d node %d, FrameCnt %d",
                    __FUNCTION__, __LINE__,
                    pipeId_src, PIPE_MCSC4_REPROCESSING, frame->getFrameCount());
        }


        if (m_parameters->needGSCForCapture(getCameraId()) == true) {
            ret = frame->getDstBuffer(pipeId_src, &srcBuffer);
            if (ret < 0)
                CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_src, ret);

            shot_stream = (struct camera2_stream *)(srcBuffer.addr[srcBuffer.planeCount-1]);
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

            CLOGD("DEBUG(%s):size (%d, %d, %d, %d %d %d)", __FUNCTION__,
                    srcRect.x, srcRect.y, srcRect.w, srcRect.h, srcRect.fullW, srcRect.fullH);
            CLOGD("DEBUG(%s):size (%d, %d, %d, %d %d %d)", __FUNCTION__,
                    dstRect.x, dstRect.y, dstRect.w, dstRect.h, dstRect.fullW, dstRect.fullH);

            ret = m_setupEntity(pipeId_gsc, frame, &srcBuffer, NULL);

            if (ret < 0) {
                CLOGE("ERR(%s[%d]):setupEntity fail, pipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, pipeId_jpeg, ret);
            }

            factory->setOutputFrameQToPipe(m_yuvCaptureDoneQ, pipeId_gsc);
            factory->pushFrameToPipe(&frame, pipeId_gsc);

        } else { /* m_parameters->needGSCForCapture(getCameraId()) == false */
            ret = frame->getDstBuffer(pipeId_src, &srcBuffer);
#if defined(ENABLE_FULL_FRAME)
            if (ret < 0)
                CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_src, ret);

            ret = m_setupEntity(pipeId_jpeg, frame, &srcBuffer, NULL);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):setupEntity fail, pipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, pipeId_jpeg, ret);
            }
#else
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_src, ret);
            } else {
                /* getting jpeg buffer from service buffer */
                ExynosCameraStream *stream = NULL;

                int streamId = 0;
                m_streamManager->getStream(HAL_STREAM_ID_JPEG, &stream);

                if (stream == NULL) {
                    CLOGE("DEBUG(%s[%d]):stream is NULL", __FUNCTION__, __LINE__);
                    return INVALID_OPERATION;
                }

                stream->getID(&streamId);
                stream->getBufferManager(&bufferMgr);
                CLOGV("DEBUG(%s[%d]):streamId(%d), bufferMgr(%p)", __FUNCTION__, __LINE__, streamId, bufferMgr);
                /* bufferMgr->printBufferQState(); */
                ret = bufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &dstBuffer);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):bufferMgr getBuffer fail, frameCount(%d), ret(%d)",
                            __FUNCTION__, __LINE__, frame->getFrameCount(), ret);
                }
            }
            ret = m_setupEntity(pipeId_jpeg, frame, &srcBuffer, &dstBuffer);
            if (ret < 0)
                CLOGE("ERR(%s[%d]):setupEntity fail, pipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, pipeId_jpeg, ret);
#endif
            factory->setOutputFrameQToPipe(m_yuvCaptureDoneQ, pipeId_jpeg);
            factory->pushFrameToPipe(&frame, pipeId_jpeg);
        }
    }

    return ret;
}

status_t ExynosCamera3::m_handleJpegFrame(ExynosCameraFrame *frame)
{
    status_t ret = NO_ERROR;
    int pipeId_jpeg = -1;
    int pipeId_src = -1;
    ExynosCameraRequest *request = NULL;
    ExynosCamera3FrameFactory * factory = NULL;
    ExynosCameraBufferManager *bufferMgr = NULL;
    ExynosCameraBuffer buffer;
    int jpegOutputSize = -1;

    request = m_requestMgr->getServiceRequest(frame->getFrameCount());
    factory = request->getFrameFactory(HAL_STREAM_ID_JPEG);

    /* We are using only PIPE_ISP_REPROCESSING */
    if (m_parameters->isReprocessing() == true) {
        if (m_parameters->needGSCForCapture(getCameraId()) == true) {
            pipeId_src = PIPE_GSC_REPROCESSING;
        } else {
            pipeId_src = (m_parameters->isOwnScc(getCameraId()) == true) ? PIPE_SCC_REPROCESSING : PIPE_ISP_REPROCESSING;
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
        ret = frame->getDstBufferState(PIPE_3AA_REPROCESSING, &bufferState, factory->getNodeType(PIPE_HWFC_JPEG_DST_REPROCESSING));
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Failed to getDstBufferState. frameCount %d pipeId %d node %d",
                    __FUNCTION__, __LINE__,
                    frame->getFrameCount(),
                    PIPE_3AA_REPROCESSING, PIPE_HWFC_JPEG_DST_REPROCESSING);
            return INVALID_OPERATION;
        } else if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
            CLOGE("ERR(%s[%d]):Invalid JPEG buffer state. frameCount %d bufferState %d",
                    __FUNCTION__, __LINE__,
                    frame->getFrameCount(), bufferState);
            return INVALID_OPERATION;
        }

        ret = frame->getDstBuffer(PIPE_3AA_REPROCESSING, &buffer, factory->getNodeType(PIPE_HWFC_JPEG_DST_REPROCESSING));
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Failed to getDstBuffer. frameCount %d pipeId %d node %d",
                    __FUNCTION__, __LINE__,
                    frame->getFrameCount(),
                    PIPE_3AA_REPROCESSING, PIPE_HWFC_JPEG_DST_REPROCESSING);
            return INVALID_OPERATION;
        }
    } else {
        ret = frame->setEntityState(pipeId_jpeg, ENTITY_STATE_FRAME_DONE);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):Failed to setEntityState, frameCoutn %d ret %d",
                    __FUNCTION__, __LINE__,
                    frame->getFrameCount(), ret);
            return INVALID_OPERATION;
        }

        ret = m_getBufferManager(pipeId_src, &bufferMgr, DST_BUFFER_DIRECTION);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getBufferManager(DST) fail, pipeId(%d), ret(%d)",
                __FUNCTION__, __LINE__, pipeId_src, ret);
            return ret;
        }

        ret = frame->getSrcBuffer(pipeId_jpeg, &buffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getSrcBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_jpeg, ret);
        }

        ret = m_putBuffers(bufferMgr, buffer.index);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):bufferMgr(DST)->putBuffers() fail, pipeId(%d), ret(%d)",
                __FUNCTION__, __LINE__, pipeId_src, ret);
        }

        ret = frame->getDstBuffer(pipeId_jpeg, &buffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_jpeg, ret);
        }
    }

    jpegOutputSize = buffer.size[0];
    CLOGI("INFO(%s[%d]):Jpeg output done, jpeg size(%d)", __FUNCTION__, __LINE__, jpegOutputSize);

    /* frame->printEntity(); */
    m_pushJpegResult(frame, jpegOutputSize, &buffer);

    return ret;
}

status_t ExynosCamera3::m_handleBayerBuffer(ExynosCameraFrame *frame)
{
    status_t ret = NO_ERROR;
    uint32_t bufferDirection = INVALID_BUFFER_DIRECTION;
    uint32_t pipeId = MAX_PIPE_NUM;
    ExynosCameraBuffer buffer;
    ExynosCameraBufferManager *bufferMgr = NULL;
    ExynosCameraFrameEntity* doneEntity = NULL;

    if (frame == NULL) {
        CLOGE("ERR(%s[%d]):Frame is NULL", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    doneEntity = frame->getFrameDoneFirstEntity();
    if(doneEntity == NULL) {
        CLOGE("ERR(%s[%d]):No frame done entity.", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }
    pipeId = doneEntity->getPipeId();

    switch(pipeId) {
        case PIPE_FLITE:
            // Pure bayer reprocessing bayer (FLITE-3AA M2M)
            // RAW capture (Both FLITE-3AA M2M and OTF)
            bufferDirection = DST_BUFFER_DIRECTION;
            ret = frame->getDstBuffer(pipeId, &buffer);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Get bayer buffer failed, framecount(%d), direction(%d), pipeId(%d)",
                        __FUNCTION__, __LINE__,
                        frame->getFrameCount(), bufferDirection, pipeId);
                return ret;
            }
            ret = m_getBufferManager(pipeId, &bufferMgr, bufferDirection);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):getBufferManager failed, pipeId(%d), bufferDirection(%d), ret(%d)",
                        __FUNCTION__, __LINE__, pipeId, bufferDirection, ret);
                return ret;
            }
            break;

        case PIPE_3AA:
            if(m_parameters->getUsePureBayerReprocessing() == true) {
                // Pure bayer reporcessing bayer (FLITE-3AA OTF) @3AA
                bufferDirection = SRC_BUFFER_DIRECTION;
                ret = frame->getSrcBuffer(pipeId, &buffer);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):Get bayer buffer failed, framecount(%d), direction(%d), pipeId(%d)",
                            __FUNCTION__, __LINE__,
                            frame->getFrameCount(), bufferDirection, pipeId);
                    return ret;
                }
                ret = m_getBufferManager(pipeId, &bufferMgr, bufferDirection);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):getBufferManager failed, pipeId(%d), bufferDirection(%d), ret(%d)",
                            __FUNCTION__, __LINE__, pipeId, bufferDirection, ret);
                    return ret;
                }
            } else {
                // Dirty bayer reprocessing bayer @3AC
                bufferDirection = DST_BUFFER_DIRECTION;
                ExynosCamera3FrameFactory* factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
                ret = frame->getDstBuffer(pipeId, &buffer, factory->getNodeType(PIPE_3AC));
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):Get bayer buffer failed, framecount(%d), direction(%d), pipeId(%d)",
                            __FUNCTION__, __LINE__,
                            frame->getFrameCount(), bufferDirection, pipeId);
                    return ret;
                }
                ret = m_getBufferManager(PIPE_3AC, &bufferMgr, bufferDirection);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):getBufferManager failed, pipeId(%d), bufferDirection(%d), ret(%d)",
                            __FUNCTION__, __LINE__, pipeId, bufferDirection, ret);
                    return ret;
                }
            }
            break;
        default:
            // Weird case
            CLOGE("ERR(%s[%d]): Invalid bayer handling PipeID=%d, FrameCnt=%d"
                , __FUNCTION__, __LINE__, pipeId, frame->getFrameCount());
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
        CLOGE("ERR(%s[%d]):Get bayer buffer failed, framecount(%d), direction(%d), pipeId(%d)",
                __FUNCTION__, __LINE__,
                frame->getFrameCount(), bufferDirection, pipeId);
        return ret;
    }

    // BufferManager is associated with the node, not PIPE.
    // So we need to distinguish 3AC and 3AA for dirtybayer reprocessing mode.
    if( (m_parameters->getUsePureBayerReprocessing() == false) && (pipeId == PIPE_3AA) ) {
        ret = m_getBufferManager(PIPE_3AC, &bufferMgr, bufferDirection);
    } else {
        ret = m_getBufferManager(pipeId, &bufferMgr, bufferDirection);
    }
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):getBufferManager failed, pipeId(%d), bufferDirection(%d), ret(%d)",
                __FUNCTION__, __LINE__, pipeId, bufferDirection, ret);
        return ret;
    }
#endif


    /* Check the validation of bayer buffer */
    if (buffer.index < 0) {
        CLOGE("ERR(%s[%d]):Invalid bayer buffer, framecount(%d), direction(%d), pipeId(%d)",
                __FUNCTION__, __LINE__,
                frame->getFrameCount(), bufferDirection, pipeId);
        return INVALID_OPERATION;
    }

    // Update only if pure bayer cases.
    // Dirty bayer case will be handled in m_handlePreviewFrame()
    if ((pipeId == PIPE_3AA) && (m_parameters->getUsePureBayerReprocessing() == true)) {
        struct camera2_shot_ext *shot_ext = NULL;

        shot_ext = (struct camera2_shot_ext *) buffer.addr[buffer.planeCount -1];
        CLOGW("WRN(%s[%d]):Timestamp(%lld)", __FUNCTION__, __LINE__, shot_ext->shot.udm.sensor.timeStampBoot);

        ret = m_updateTimestamp(frame, &buffer);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Failed to update time stamp", __FUNCTION__, __LINE__);
            return ret;
        }
    }

    /* Handle the bayer buffer */
    if (frame->getFrameServiceBayer() == true
        && (m_parameters->getUsePureBayerReprocessing() == true || pipeId == PIPE_FLITE)) {
        /* Raw Capture Request */
        CLOGV("INFO(%s[%d]):Handle service bayer buffer. FLITE-3AA_OTF %d Bayer_Pipe_ID %d Framecount %d",
                __FUNCTION__, __LINE__,
                m_parameters->isFlite3aaOtf(), pipeId, frame->getFrameCount());

        /* Raw capture takes only the Pure bayer */
        if (pipeId == PIPE_FLITE) {
            ret = m_sendRawCaptureResult(frame, pipeId, (bufferDirection == SRC_BUFFER_DIRECTION));
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to sendRawCaptureResult. frameCount %d bayerPipeId %d bufferIndex %d",
                        __FUNCTION__, __LINE__,
                        frame->getFrameCount(), pipeId, buffer.index);
                goto CHECK_RET_PUTBUFFER;
            }
        }

        if (m_parameters->isReprocessing() == true && frame->getFrameCapture() == true) {
            CLOGD("DEBUG(%s[%d]):Hold service bayer buffer for reprocessing."
                    "frameCount %d bayerPipeId %d bufferIndex %d",
                    __FUNCTION__, __LINE__,
                    frame->getFrameCount(), pipeId, buffer.index);
            ret = m_captureZslSelector->manageFrameHoldListForDynamicBayer(frame);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to manageFrameHoldListForDynamicBayer to captureZslSelector."
                        "frameCount %d bayerPipeId %d bufferIndex %d",
                        __FUNCTION__, __LINE__,
                        frame->getFrameCount(), pipeId, buffer.index);
                goto CHECK_RET_PUTBUFFER;
            }
        }
        goto CHECK_RET_PUTBUFFER;

    } else if (frame->getFrameZsl() == true) {
        /* ZSL Capture Request */
        CLOGV("INFO(%s[%d]):Handle ZSL buffer. FLITE-3AA_OTF %d Bayer_Pipe_ID %d Framecount %d",
                __FUNCTION__, __LINE__,
                m_parameters->isFlite3aaOtf(), pipeId, frame->getFrameCount());

        /* Save the meta plane to the end of free space of bayer buffer
           It will be restored in m_selectBayerThreadFunc()
        */
        char* metaPlane = buffer.addr[buffer.planeCount -1];
        char* bayerSpareArea = buffer.addr[0] + (buffer.size[0]-buffer.size[buffer.planeCount -1]);

        CLOGD("DEBUG(%s[%d]):BufIdx[%d], metaPlane[%p], Len=%d / bayerBuffer[%p], Len=%d / bayerSpareArea[%p]",
                __FUNCTION__, __LINE__,
                buffer.index, metaPlane, buffer.size[buffer.planeCount -1], buffer.addr[0], buffer.size[0], bayerSpareArea);
        if(buffer.addr[0] == NULL) {
            CLOGW("WARN(%s[%d]): buffer.addr[0] is NULL. Buffer may not come from the Service [Index:%d, FD:%d, Size:%d]",
                __FUNCTION__, __LINE__, buffer.index, buffer.fd[0], buffer.size[0]);
        } else if (metaPlane == NULL) {
            CLOGW("WARN(%s[%d]): metaPlane is NULL. Buffer may not be initialized correctly [MetaPlaneIndex: %d, Index:%d, FD:%d]",
                __FUNCTION__, __LINE__, buffer.planeCount -1, buffer.index, buffer.fd[buffer.planeCount -1], buffer.size[buffer.planeCount -1]);
        } else {
            memcpy(bayerSpareArea, metaPlane, buffer.size[buffer.planeCount -1]);
            CLOGD("DEBUG(%s[%d]):shot.dm.request.frameCount : Original : %d / Copied : %d",
                __FUNCTION__, __LINE__,
                getMetaDmRequestFrameCount((camera2_shot_ext *)metaPlane), getMetaDmRequestFrameCount((camera2_shot_ext *)bayerSpareArea));
        }

        ret = m_sendZSLCaptureResult(frame, pipeId, (bufferDirection == SRC_BUFFER_DIRECTION));
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Failed to sendZslCaptureResult. frameCount %d bayerPipeId %d",
                    __FUNCTION__, __LINE__,
                    frame->getFrameCount(), pipeId);
        }
        goto SKIP_PUTBUFFER;

    } else if (m_parameters->isReprocessing() == true){
        /* For ZSL Reprocessing */
        CLOGV("INFO(%s[%d]):Hold internal bayer buffer for reprocessing."
                " FLITE-3AA_OTF %d Bayer_Pipe_ID %d Framecount %d",
                __FUNCTION__, __LINE__,
                m_parameters->isFlite3aaOtf(), pipeId, frame->getFrameCount());
        ret = m_captureSelector->manageFrameHoldList(frame, pipeId, (bufferDirection == SRC_BUFFER_DIRECTION));
        goto CHECK_RET_PUTBUFFER;

    } else {
        /* No request for bayer image */
        CLOGV("INFO(%s[%d]):Return internal bayer buffer. FLITE-3AA_OTF %d Bayer_Pipe_ID %d Framecount %d",
                __FUNCTION__, __LINE__,
                m_parameters->isFlite3aaOtf(), pipeId, frame->getFrameCount());

        ret = m_putBuffers(bufferMgr, buffer.index);
        if (ret !=  NO_ERROR) {
            CLOGE("ERR(%s[%d]):putBuffers failed, pipeId(%d), bufferDirection(%d), bufferIndex(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipeId, bufferDirection, buffer.index, ret);
        }
    }

    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):Handling bayer buffer failed, isServiceBayer(%d), direction(%d), pipeId(%d)",
                __FUNCTION__, __LINE__,
                frame->getFrameServiceBayer(),
                bufferDirection, pipeId);
    }

    return ret;

CHECK_RET_PUTBUFFER:
    /* Put the bayer buffer if there was an error during processing */
    if(ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):Handling bayer buffer failed, Bayer buffer will be put. isServiceBayer(%d), direction(%d), pipeId(%d)",
                __FUNCTION__, __LINE__,
                frame->getFrameServiceBayer(),
                bufferDirection, pipeId);

        if (m_putBuffers(bufferMgr, buffer.index) !=  NO_ERROR) {
            // Do not taint 'ret' to print appropirate error log on next block.
            CLOGE("ERR(%s[%d]):putBuffers failed, pipeId(%d), bufferDirection(%d), bufferIndex(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipeId, bufferDirection, buffer.index, ret);
        }
    }

SKIP_PUTBUFFER:
    return ret;
    
}

bool ExynosCamera3::m_previewStreamBayerPipeThreadFunc(void)
{
    status_t ret = 0;
    ExynosCameraFrame *newFrame = NULL;

    ret = m_pipeFrameDoneQ[PIPE_FLITE]->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            /* CLOGW("WARN(%s):wait timeout", __FUNCTION__); */
        } else {
            CLOGE("ERR(%s):wait and pop fail, ret(%d)", __FUNCTION__, ret);
            /* TODO: doing exception handling */
        }
        return true;
    }
    return m_previewStreamFunc(newFrame, PIPE_FLITE);
}

bool ExynosCamera3::m_previewStream3AAPipeThreadFunc(void)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrame *newFrame = NULL;

    ret = m_pipeFrameDoneQ[PIPE_3AA]->waitAndPopProcessQ(&newFrame);
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
    return m_previewStreamFunc(newFrame, PIPE_3AA);
}

bool ExynosCamera3::m_previewStreamISPPipeThreadFunc(void)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrame *newFrame = NULL;

    ret = m_pipeFrameDoneQ[PIPE_ISP]->waitAndPopProcessQ(&newFrame);
    if (ret != NO_ERROR) {
        if (ret == TIMED_OUT)
            CLOGW("WARN(%s):wait timeout", __FUNCTION__);
        else
            CLOGE("ERR(%s[):Failed to waitAndPopProcessQ. ret %d", __FUNCTION__, ret);
        return true;
    }
    return m_previewStreamFunc(newFrame, PIPE_ISP);
}

bool ExynosCamera3::m_previewStreamMCSCPipeThreadFunc(void)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrame *newFrame = NULL;

    ret = m_pipeFrameDoneQ[PIPE_MCSC]->waitAndPopProcessQ(&newFrame);
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
    return m_previewStreamFunc(newFrame, PIPE_MCSC);
}

bool ExynosCamera3::m_previewStreamFunc(ExynosCameraFrame *newFrame, int pipeId)
{
    status_t ret = 0;
    int type = CAMERA3_MSG_SHUTTER;
    /* only trace */

    if (newFrame == NULL) {
        CLOGE("ERROR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        return true;
    }

    if (newFrame->getFrameType() == FRAME_TYPE_INTERNAL) {
        if (m_internalFrameThread->isRunning() == false)
            m_internalFrameThread->run();
        m_internalFrameDoneQ->pushProcessQ(&newFrame);
        return true;
    }

    CLOGV("DEBUG(%s[%d]):stream thread : frameCount %d pipeId %d",
            __FUNCTION__, __LINE__, newFrame->getFrameCount(), pipeId);

    /* TODO: M2M path is also handled by this */
    ret = m_handlePreviewFrame(newFrame, pipeId);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):handle preview frame fail", __FUNCTION__, __LINE__);
        return ret;
    }

    if (newFrame->isComplete() == true) {
        m_sendNotify(newFrame->getFrameCount(), type);

        CLOGV("DEBUG(%s[%d]):newFrame->getFrameCount(%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());
#if defined(ENABLE_FULL_FRAME)
        if (newFrame->getFrameCapture() == false) {
#endif
            ret = m_removeFrameFromList(&m_processList, &m_processLock, newFrame);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):remove frame from processList fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            }

            CLOGV("DEBUG(%s[%d]):frame complete, frameCount %d",
                    __FUNCTION__, __LINE__, newFrame->getFrameCount());
            newFrame->decRef();
            m_frameMgr->deleteFrame(newFrame);
            newFrame = NULL;
#if defined(ENABLE_FULL_FRAME)
        }
#endif
    }

    return true;
}


status_t ExynosCamera3::m_updateTimestamp(ExynosCameraFrame *frame, ExynosCameraBuffer *timestampBuffer)
{
    struct camera2_shot_ext *shot_ext = NULL;
    status_t ret = NO_ERROR;

    /* handle meta data */
    shot_ext = (struct camera2_shot_ext *) timestampBuffer->addr[timestampBuffer->planeCount -1];
    if (shot_ext == NULL) {
        CLOGE("ERR(%s[%d]):shot_ext is NULL. framecount %d buffer %d",
                __FUNCTION__, __LINE__, frame->getFrameCount(), timestampBuffer->index);
        return INVALID_OPERATION;
    }

    /* HACK: W/A for timeStamp reversion */
    if (shot_ext->shot.udm.sensor.timeStampBoot < m_lastFrametime) {
        CLOGW("WRN(%s[%d]):Timestamp is zero!", __FUNCTION__, __LINE__);
        shot_ext->shot.udm.sensor.timeStampBoot = m_lastFrametime + 15000000;
    }
    m_lastFrametime = shot_ext->shot.udm.sensor.timeStampBoot;

    ret = m_pushResult(frame->getFrameCount(), shot_ext);

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
        CLOGE("ERR(%s[%d]):shot_ext is NULL. buffer %d",
                __FUNCTION__, __LINE__, timestampBuffer->index);
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

status_t ExynosCamera3::m_handlePreviewFrame(ExynosCameraFrame *frame, int pipeId)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameEntity *entity = NULL;
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraBuffer buffer;
    ExynosCamera3FrameFactory *factory = NULL;
    struct camera2_shot_ext *shot_ext = NULL;
    uint32_t framecount = 0;
    int capturePipeId = -1;
    int streamId = -1;

    factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];

    entity = frame->getFrameDoneFirstEntity(pipeId);
    if (entity == NULL) {
        CLOGE("ERR(%s[%d]):current entity is NULL. framecount %d pipeId %d",
                __FUNCTION__, __LINE__, frame->getFrameCount(), pipeId);
        /* TODO: doing exception handling */
        return INVALID_OPERATION;
    }

    CLOGV("DEBUG(%s[%d]):handle preview frame : previewStream frameCnt(%d) entityID(%d)",
            __FUNCTION__, __LINE__, frame->getFrameCount(), entity->getPipeId());

    switch(pipeId) {
    case PIPE_FLITE:
        /* 1. Handle bayer buffer */
        if (m_parameters->isFlite3aaOtf() == true) {
            ret = m_handleBayerBuffer(frame);
            if (ret < NO_ERROR) {
                CLOGE("ERR(%s[%d]):Handle bayer buffer failed, framecount(%d), pipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, frame->getFrameCount(), entity->getPipeId(), ret);
                return ret;
            }
        } else {
            /* Send the bayer buffer to 3AA Pipe */
            ret = frame->getDstBuffer(entity->getPipeId(), &buffer);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                return ret;
            }

            if (buffer.index < 0) {
                CLOGE("ERR(%s[%d]):Invalid buffer index(%d), framecount(%d), pipeId(%d)",
                        __FUNCTION__, __LINE__,
                        buffer.index, frame->getFrameCount(), entity->getPipeId());
                return BAD_VALUE;
            }

            ret = m_setupEntity(PIPE_3AA, frame, &buffer, NULL);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):setSrcBuffer failed, pipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, PIPE_3AA, ret);
                return ret;
            }

            factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
            factory->pushFrameToPipe(&frame, PIPE_3AA);
        }
        break;
    case PIPE_3AA:
        /* Notify ShotDone to mainThread */
        framecount = frame->getFrameCount();
        m_shotDoneQ->pushProcessQ(&framecount);

        /* 1. Handle the buffer from 3AA output node */
        if (m_parameters->isFlite3aaOtf() == true) {
            ExynosCameraBufferManager *bufferMgr = NULL;

            /* Return the dummy buffer */
            ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):getSrcBuffer fail, pipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                return ret;
            }

            if (buffer.index < 0) {
                CLOGE("ERR(%s[%d]):Invalid buffer index(%d), framecount(%d), pipeId(%d)",
                        __FUNCTION__, __LINE__,
                        buffer.index, frame->getFrameCount(), entity->getPipeId());
                return BAD_VALUE;
            }

            CLOGV("DEBUG(%s[%d]):Return 3AS Buffer. driver->framecount %d hal->framecount %d",
                    __FUNCTION__, __LINE__,
                    getMetaDmRequestFrameCount((struct camera2_shot_ext *)buffer.addr[buffer.planeCount-1]),
                    frame->getFrameCount());

            ret = m_getBufferManager(entity->getPipeId(), &bufferMgr, SRC_BUFFER_DIRECTION);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failted to getBufferMgr. pipeId %d ret %d",
                        __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                return ret;
            }

            ret = m_putBuffers(bufferMgr, buffer.index);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to putBuffer", __FUNCTION__, __LINE__);
                return ret;
            }
        }
        if(frame->getRequest(PIPE_3AC) == true) {
            ret = m_handleBayerBuffer(frame);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to handle bayerBuffer(3AA). framecount %d pipeId %d ret %d",
                        __FUNCTION__, __LINE__,
                        frame->getFrameCount(), entity->getPipeId(), ret);
                return ret;
            }
        }

        frame->setMetaDataEnable(true);

        if (m_parameters->is3aaIspOtf() == false)
            break;
    case PIPE_ISP:
        if (entity->getPipeId() == PIPE_ISP) {
            ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to getSrcBuffer. pipeId %d ret %d",
                        __FUNCTION__, __LINE__,
                        entity->getPipeId(), ret);
                return ret;
            }
#ifdef CORRECT_TIMESTAMP_FOR_SENSORFUSION
            /* m_adjustTimestamp() is to do adjust timestamp for ITS/sensorFusion. */
            /* This function should be called once at here */
            ret = m_adjustTimestamp(&buffer);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to adjust timestamp", __FUNCTION__, __LINE__);
                return ret;
            }
#endif
            ret = m_updateTimestamp(frame, &buffer);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to update timestamp", __FUNCTION__, __LINE__);
                return ret;
            }

            if (buffer.index >= 0) {
                ret = m_putBuffers(m_ispBufferMgr, buffer.index);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):Failed to putBuffer into ispBufferMgr",
                            __FUNCTION__, __LINE__);
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
                CLOGE("ERR(%s[%d]):getSrcBuffer fail, pipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                return ret;
            }

            ret = m_updateTimestamp(frame, &buffer);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to putBuffer", __FUNCTION__, __LINE__);
                return ret;
            }

            if (buffer.index >= 0) {
                ret = m_putBuffers(m_mcscBufferMgr, buffer.index);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
                    break;
                }
            }

            CLOGV("DEBUG(%s[%d]):Return MCSC Buffer. driver->framecount %d hal->framecount %d",
                    __FUNCTION__, __LINE__,
                    getMetaDmRequestFrameCount((struct camera2_shot_ext *)buffer.addr[buffer.planeCount-1]),
                    frame->getFrameCount());
        }

        if (frame->getFrameState() == FRAME_STATE_SKIPPED) {
            CLOGW("WARN(%s[%d]):Skipped frame. Force callback result. frameCount %d",
                    __FUNCTION__, __LINE__, frame->getFrameCount());

            ret = m_forceResultCallback(frame->getFrameCount());
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to forceResultCallback. frameCount %d",
                        __FUNCTION__, __LINE__, frame->getFrameCount());
                return ret;
            }
        }

        /* PIPE_MCSC 0, 1, 2 */
        for (int i = 0; i < m_streamManager->getYuvStreamCount(); i++) {
            capturePipeId = PIPE_MCSC0 + i;

            if (frame->getRequest(capturePipeId) == true) {
                ret = frame->getDstBuffer(entity->getPipeId(), &buffer, factory->getNodeType(capturePipeId));
                if (ret != NO_ERROR || buffer.index < 0) {
                    CLOGE("ERR(%s[%d]):Failed to get DST buffer. pipeId %d bufferIndex %d frameCount %d ret %d",
                            __FUNCTION__, __LINE__,
                            capturePipeId, buffer.index, frame->getFrameCount(), ret);
                    return ret;
                }

                streamId = m_streamManager->getYuvStreamId(i);

                ret = m_resultCallback(&buffer, frame->getFrameCount(), streamId);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):Failed to resultCallback."
                            " pipeId %d bufferIndex %d frameCount %d streamId %d ret %d",
                            __FUNCTION__, __LINE__,
                            capturePipeId, buffer.index, frame->getFrameCount(), streamId, ret);
                    return ret;
                }
            }
        }

#if defined(ENABLE_FULL_FRAME)
        if (frame->getFrameCapture() == true) {
            CLOGI("INFO(%s[%d]):Capture frame(%d)", __FUNCTION__, __LINE__, frame->getFrameCount());
            ret = m_handleYuvCaptureFrame(frame);
            if (ret != NO_ERROR)
                CLOGE("ERR(%s[%d]):Failed to handleReprocessingDoneFrame, ret %d",
                        __FUNCTION__, __LINE__, ret);

            m_captureStreamThread->run(PRIORITY_DEFAULT);
        }
#endif
        break;
    default:
        CLOGE("ERR(%s[%d]):Invalid pipe ID", __FUNCTION__, __LINE__);
        break;
    }

    ret = frame->setEntityState(entity->getPipeId(), ENTITY_STATE_COMPLETE);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setEntityState fail, pipeId(%d), state(%d), ret(%d)",
            __FUNCTION__, __LINE__, entity->getPipeId(), ENTITY_STATE_COMPLETE, ret);
        return ret;
    }

    return ret;
}

status_t ExynosCamera3::m_resultCallback(ExynosCameraBuffer *buffer, uint32_t frameCount, int streamId)
{
    status_t ret = NO_ERROR;
    ExynosCameraBufferManager *bufferMgr = NULL;
    ExynosCameraStream *stream = NULL;
    camera3_stream_buffer_t streamBuffer;
    ExynosCameraRequest *request = NULL;
    ResultRequest resultRequest = NULL;

    if (buffer == NULL) {
        CLOGE("ERR(%s[%d]):buffer is NULL", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    ret = m_streamManager->getStream(streamId, &stream);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):Failed to get stream %d from streamMgr",
                __FUNCTION__, __LINE__, streamId);
        return INVALID_OPERATION;
    }

    ret = stream->getStream(&streamBuffer.stream);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):Failed to get stream %d from ExynosCameraStream",
                __FUNCTION__, __LINE__, streamId);
        return INVALID_OPERATION;
    }

    stream->getBufferManager(&bufferMgr);
    ret = bufferMgr->getHandleByIndex(&streamBuffer.buffer, buffer->index);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):Failed to get buffer handle. streamId %d bufferIndex %d",
                __FUNCTION__, __LINE__, streamId, buffer->index);
        return INVALID_OPERATION;
    }

    streamBuffer.status = CAMERA3_BUFFER_STATUS_OK;
    streamBuffer.acquire_fence = -1;
    streamBuffer.release_fence = -1;

    request = m_requestMgr->getServiceRequest(frameCount);
    resultRequest = m_requestMgr->createResultRequest(frameCount,
                                                      EXYNOS_REQUEST_RESULT::CALLBACK_BUFFER_ONLY,
                                                      NULL, NULL);
    resultRequest->pushStreamBuffer(&streamBuffer);

    m_requestMgr->callbackSequencerLock();
    request->increaseCompleteBufferCount();
    request->increaseDuplicateBufferCount();
    m_requestMgr->callbackRequest(resultRequest);
    m_requestMgr->callbackSequencerUnlock();

    return ret;
}

status_t ExynosCamera3::m_releaseBuffers(void)
{
    CLOGI("INFO(%s[%d]):release buffer", __FUNCTION__, __LINE__);
    status_t ret = NO_ERROR;

    for (int bufIndex = 0; bufIndex < NUM_BAYER_BUFFERS; bufIndex++) {
        ret = m_putBuffers(m_fliteBufferMgr, bufIndex);
        ret = m_putBuffers(m_3aaBufferMgr, bufIndex);
        ret = m_putBuffers(m_ispBufferMgr, bufIndex);
        ret = m_putBuffers(m_mcscBufferMgr, bufIndex);
        if(m_internalScpBufferMgr != NULL) {
            ret = m_putBuffers(m_internalScpBufferMgr, bufIndex);
        }
#ifdef SUPPORT_DEPTH_MAP
        ret = m_putBuffers(m_depthMapBufferMgr, bufIndex);
#endif
    }

    if (m_fliteBufferMgr != NULL)
        m_fliteBufferMgr->deinit();
    if (m_3aaBufferMgr != NULL)
        m_3aaBufferMgr->deinit();
    if (m_ispBufferMgr != NULL)
        m_ispBufferMgr->deinit();
    if (m_mcscBufferMgr != NULL)
        m_mcscBufferMgr->deinit();
    if (m_internalScpBufferMgr!= NULL)
        m_internalScpBufferMgr->deinit();
#ifdef SUPPORT_DEPTH_MAP
    if (m_depthMapBufferMgr != NULL)
        m_depthMapBufferMgr->deinit();
#endif

    if (m_ispReprocessingBufferMgr != NULL)
        m_ispReprocessingBufferMgr->deinit();
    if (m_yuvCaptureBufferMgr != NULL)
        m_yuvCaptureBufferMgr->deinit();
    if (m_thumbnailBufferMgr != NULL)
        m_thumbnailBufferMgr->deinit();

    CLOGI("INFO(%s[%d]):free buffer done", __FUNCTION__, __LINE__);

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
                CLOGV("DEBUG(%s[%d]):request(%d) inputBuffer(%p) buffer-StreamType(HAL_STREAM_ID_ZSL) size(%u x %u) ",
                        __FUNCTION__, __LINE__,
                        request->frame_number, buffer,
                        buffer->stream->width,
                        buffer->stream->height);
                break;
            case HAL_STREAM_ID_PREVIEW:
            case HAL_STREAM_ID_VIDEO:
            case HAL_STREAM_ID_JPEG:
            case HAL_STREAM_ID_CALLBACK:
            default:
                CLOGE("ERR(%s[%d]):request(%d) inputBuffer(%p) buffer-stream type is inavlid(%d). size(%d x %d)",
                        __FUNCTION__, __LINE__,
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
                CLOGV("DEBUG(%s[%d]):request(%d) outputBuffer(%p) buffer-StreamType(HAL_STREAM_ID_RAW) size(%u x %u) ",
                        __FUNCTION__, __LINE__,
                        request->frame_number, buffer,
                        bufferList[index].stream->width,
                        bufferList[index].stream->height);
                break;
            case HAL_STREAM_ID_ZSL_OUTPUT:
                m_registerBuffers(m_zslBufferMgr, requestKey, buffer);
                CLOGV("DEBUG(%s[%d]):request(%d) outputBuffer(%p) buffer-StreamType(HAL_STREAM_ID_ZSL_OUTPUT) size(%u x %u) ",
                        __FUNCTION__, __LINE__,
                        request->frame_number, buffer,
                        bufferList[index].stream->width,
                        bufferList[index].stream->height);
                break;
            case HAL_STREAM_ID_PREVIEW:
                stream->getBufferManager(&bufferMgr);
                m_registerBuffers(bufferMgr, requestKey, buffer);
                CLOGV("DEBUG(%s[%d]):request(%d) outputBuffer(%p) buffer-StreamType(HAL_STREAM_ID_PREVIEW) size(%u x %u) ",
                        __FUNCTION__, __LINE__,
                        request->frame_number, buffer,
                        bufferList[index].stream->width,
                        bufferList[index].stream->height);
                break;
            case HAL_STREAM_ID_VIDEO:
                stream->getBufferManager(&bufferMgr);
                m_registerBuffers(bufferMgr, requestKey, buffer);
                CLOGV("DEBUG(%s[%d]):request(%d) outputBuffer(%p) buffer-StreamType(HAL_STREAM_ID_VIDEO) size(%u x %u) ",
                        __FUNCTION__, __LINE__,
                        request->frame_number, buffer,
                        bufferList[index].stream->width,
                        bufferList[index].stream->height);
                break;
            case HAL_STREAM_ID_JPEG:
                stream->getBufferManager(&bufferMgr);
                m_registerBuffers(bufferMgr, requestKey, buffer);
                CLOGD("DEBUG(%s[%d]):request(%d) outputBuffer(%p) buffer-StreamType(HAL_STREAM_ID_JPEG) size(%u x %u) ",
                        __FUNCTION__, __LINE__,
                        request->frame_number, buffer,
                        bufferList[index].stream->width,
                        bufferList[index].stream->height);
                break;
            case HAL_STREAM_ID_CALLBACK:
                stream->getBufferManager(&bufferMgr);
                m_registerBuffers(bufferMgr, requestKey, buffer);
                CLOGV("DEBUG(%s[%d]):request(%d) outputBuffer(%p) buffer-StreamType(HAL_STREAM_ID_CALLBACK) size(%u x %u) ",
                        __FUNCTION__, __LINE__,
                        request->frame_number, buffer,
                        bufferList[index].stream->width,
                        bufferList[index].stream->height);
                break;
            default:
                CLOGE("ERR(%s[%d]):request(%d) outputBuffer(%p) buffer-StreamType is invalid(%d) size(%d x %d) ",
                        __FUNCTION__, __LINE__,
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
            CLOGE("ERR(%s[%d]):putBuffer(%d) fail(%d)", __FUNCTION__, __LINE__, requestKey, ret);
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
        CLOGE("ERR(%s[%d]):m_allocBuffers(minBufCount=%d, maxBufCount=%d, type=%d) fail",
            __FUNCTION__, __LINE__, minBufCount, maxBufCount, type);
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

status_t ExynosCamera3::m_setBuffers(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = NO_ERROR;

    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES]    = {0};
    int minBufferCount = 1;
    int maxBufferCount = 1;
    int planeCount     = 1;

    int maxSensorW  = 0, maxSensorH  = 0;
    int maxPreviewW  = 0, maxPreviewH  = 0;
    ExynosRect bdsRect;

    CLOGI("INFO(%s[%d]):alloc buffer - camera ID: %d",
            __FUNCTION__, __LINE__, m_cameraId);

    m_parameters->getMaxSensorSize(&maxSensorW, &maxSensorH);
    CLOGI("(%s):HW Sensor MAX width x height = %dx%d",
            __FUNCTION__, maxSensorW, maxSensorH);
    m_parameters->getPreviewBdsSize(&bdsRect);
    CLOGI("(%s):Preview BDS width x height = %dx%d",
            __FUNCTION__, bdsRect.w, bdsRect.h);

    if (m_parameters->getHighSpeedRecording() == true) {
        m_parameters->getHwPreviewSize(&maxPreviewW, &maxPreviewH);
        CLOGI("(%s):PreviewSize(HW - Highspeed) width x height = %dx%d",
                __FUNCTION__, maxPreviewW, maxPreviewH);
    } else {
        m_parameters->getMaxPreviewSize(&maxPreviewW, &maxPreviewH);
        CLOGI("(%s):PreviewSize(Max) width x height = %dx%d",
                __FUNCTION__, maxPreviewW, maxPreviewH);
    }

    /* FLITE */
#if !defined(ENABLE_FULL_FRAME)
#ifdef CAMERA_PACKED_BAYER_ENABLE
    // TODO: May be like this??
    // bytesPerLine[0] = getBayerLineSize(tempRect.fullW, m_parameters->getBayerFormat(PIPE_FLITE))
    if(m_parameters->getUsePureBayerReprocessing() == true) {
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
    type            = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;

    ret = m_allocBuffers(m_fliteBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, maxBufferCount, type, true, false);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):fliteBuffer m_allocBuffers(bufferCount=%d) fail",
            __FUNCTION__, __LINE__, maxBufferCount);
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

        CLOGI("INFO(%s[%d]):Depth Map Size width x height = %dx%d",
                __FUNCTION__, __LINE__, depthMapW, depthMapH);

        bayerFormat     = DEPTH_MAP_FORMAT;
        bytesPerLine[0] = getBayerLineSize(depthMapW, bayerFormat);
        planeSize[0]    = getBayerPlaneSize(depthMapW, depthMapH, bayerFormat);
        planeCount      = 2;
        maxBufferCount  = m_exynosconfig->current->bufInfo.num_sensor_buffers;
        needMmap = true;
        type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;

        if (m_depthMapBufferMgr == NULL) {
            CLOGI("ERR(%s[%d]):Failed to m_depthMapBufferMgr.", __FUNCTION__, __LINE__);
        }

        ret = m_allocBuffers(m_depthMapBufferMgr,
                planeCount, planeSize, bytesPerLine,
                maxBufferCount, maxBufferCount,
                type, true, needMmap);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Failed to allocate Depth Map Buffers. bufferCount %d",
                    __FUNCTION__, __LINE__, maxBufferCount);
            return ret;
        }

        CLOGI("INFO(%s[%d]):m_allocBuffers(DepthMap Buffer) %d x %d,\
            planeSize(%d), planeCount(%d), maxBufferCount(%d)",
            __FUNCTION__, __LINE__, depthMapW, depthMapH,
            planeSize[0], planeCount, maxBufferCount);
    }
#endif

    /* 3AA */
    planeSize[0]    = 32 * 64 * 2;
    planeCount      = 2;
    maxBufferCount  = m_exynosconfig->current->bufInfo.num_3aa_buffers;
    ret = m_allocBuffers(m_3aaBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, true);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_3aaBufferMgr m_allocBuffers(bufferCount=%d) fail",
            __FUNCTION__, __LINE__, maxBufferCount);
        return ret;
    }

    /* ISP */
    bytesPerLine[0] = getBayerLineSize(maxPreviewW, m_parameters->getBayerFormat(PIPE_ISP));
    //bytesPerLine[0] = bdsRect.w * 2;
    planeSize[0]    = bytesPerLine[0] * maxPreviewH;
    planeCount      = 2;
    maxBufferCount  = m_exynosconfig->current->bufInfo.num_3aa_buffers;
    ret = m_allocBuffers(m_ispBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, true);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_ispBufferMgr m_allocBuffers(bufferCount=%d) fail",
            __FUNCTION__, __LINE__, maxBufferCount);
        return ret;
    }

    /* MCSC */
    bytesPerLine[0] = maxPreviewW * 2;
    planeSize[0]    = bytesPerLine[0] * maxPreviewH;
    planeCount      = 2;
    maxBufferCount  = m_exynosconfig->current->bufInfo.num_hwdis_buffers;
    ret = m_allocBuffers(m_mcscBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, true);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_mcscBufferMgr m_allocBuffers(bufferCount=%d) fail",
            __FUNCTION__, __LINE__, maxBufferCount);
        return ret;
    }

    if(m_parameters->isSupportZSLInput()) {
        /* TODO: Can be allocated on-demand, when the ZSL input is exist. */
        ret = m_setInternalScpBuffer();
        if (ret < 0) {
            CLOGE2("m_setReprocessing Buffer fail");
            return ret;
        }
    }

    CLOGI("INFO(%s[%d]):alloc buffer done - camera ID: %d", __FUNCTION__, __LINE__, m_cameraId);
    return ret;
}

bool ExynosCamera3::m_setBuffersThreadFunc(void)
{
    int ret;

    ret = m_setBuffers();
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_setBuffers failed", __FUNCTION__, __LINE__);
        m_releaseBuffers();
        return false;
    }

    return false;
}

uint32_t ExynosCamera3::m_getBayerPipeId(void)
{
    uint32_t pipeId = 0;

    if (m_parameters->getUsePureBayerReprocessing() == true)
        pipeId = PIPE_FLITE;
    else
        pipeId = PIPE_3AA;
#if defined(ENABLE_FULL_FRAME)
    pipeId = PIPE_FLITE;
#endif

    return pipeId;
}

status_t ExynosCamera3::m_pushRequest(camera3_capture_request *request)
{
    status_t ret = OK;
    ExynosCameraRequest* req;
    FrameFactoryList factoryList;
    FrameFactoryListIterator factorylistIter;
    ExynosCamera3FrameFactory *factory = NULL;
    bool isInternalNeed;

    CLOGV("DEBUG(%s[%d]):m_pushRequest frameCnt(%d)", __FUNCTION__, __LINE__, request->frame_number);

    req = m_requestMgr->registerServiceRequest(request);
    if (req == NULL) {
        CLOGE("ERR(%s[%d]):registerServiceRequest failed, FrameNumber = [%d]"
            ,__FUNCTION__, __LINE__, request->frame_number);
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

status_t ExynosCamera3::m_popRequest(ExynosCameraRequest **request)
{
    status_t ret = OK;

    CLOGV("DEBUG(%s[%d]):m_popRequest ", __FUNCTION__, __LINE__);

    *request = m_requestMgr->createServiceRequest();
    if (*request == NULL) {
        CLOGE("ERR(%s[%d]):createRequest failed ",__FUNCTION__, __LINE__);
        ret = INVALID_OPERATION;
    }
    return ret;
}


/* m_needNotify is for reprocessing */
bool ExynosCamera3::m_needNotify(ExynosCameraRequest *request)
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
                CLOGE("DEBUG(%s[%d]):stream is NULL", __FUNCTION__, __LINE__);
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
    ExynosCameraRequest *request = NULL;
    struct camera2_shot_ext dst_ext;
    uint8_t currentPipelineDepth = 0;

    request = m_requestMgr->getServiceRequest(frameCount);
    if (request == NULL) {
        CLOGE("ERR(%s[%d]):getRequest failed ",__FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    ret = request->getResultShot(&dst_ext);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getResultShot failed ",__FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    if (dst_ext.shot.dm.request.frameCount >= src_ext->shot.dm.request.frameCount) {
        CLOGI("INFO(%s[%d]):Skip to update result. frameCount %d requestKey %d shot.request.frameCount %d",
                __FUNCTION__, __LINE__,
                frameCount, request->getKey(), dst_ext.shot.dm.request.frameCount);
        return ret;
    }

    currentPipelineDepth = dst_ext.shot.dm.request.pipelineDepth;
    memcpy(&dst_ext.shot.dm, &src_ext->shot.dm, sizeof(struct camera2_dm));
    memcpy(&dst_ext.shot.udm, &src_ext->shot.udm, sizeof(struct camera2_udm));
    dst_ext.shot.dm.request.pipelineDepth = currentPipelineDepth;

    ret = request->setResultShot(&dst_ext);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setResultShot failed ",__FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    ret = m_metadataConverter->updateDynamicMeta(request);

    CLOGV("DEBUG(%s[%d]):Set result. frameCount %d dm.request.frameCount %d",
            __FUNCTION__, __LINE__,
            request->getFrameCount(),
            dst_ext.shot.dm.request.frameCount);

    return ret;
}

status_t ExynosCamera3::m_pushJpegResult(ExynosCameraFrame *frame, int size, ExynosCameraBuffer *buffer)
{
    status_t ret = NO_ERROR;
    ExynosCameraStream *stream = NULL;
    camera3_stream_buffer_t streamBuffer;
    camera3_stream_buffer_t *output_buffers;
    ResultRequest resultRequest = NULL;
    ExynosCameraRequest *request = NULL;
    camera3_capture_result_t requestResult;

    ExynosCameraBufferManager *bufferMgr = NULL;

    ret = m_streamManager->getStream(HAL_STREAM_ID_JPEG, &stream);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):Failed to getStream from StreamMgr. streamId HAL_STREAM_ID_JPEG",
                __FUNCTION__, __LINE__);
        return ret;
    }

    if (stream == NULL) {
        CLOGE("DEBUG(%s[%d]):stream is NULL", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    ret = stream->getStream(&streamBuffer.stream);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):Failed to getStream from ExynosCameraStream. streamId HAL_STREAM_ID_JPEG",
                __FUNCTION__, __LINE__);
        return ret;
    }

    ret = stream->getBufferManager(&bufferMgr);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):Failed to getBufferManager. streamId HAL_STREAM_ID_JPEG",
                __FUNCTION__, __LINE__);
        return ret;
    }

    ret = bufferMgr->getHandleByIndex(&streamBuffer.buffer, buffer->index);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):Failed to getHandleByIndex. bufferIndex %d",
                __FUNCTION__, __LINE__, buffer->index);
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

    request = m_requestMgr->getServiceRequest(frame->getFrameCount());

#if !defined(ENABLE_FULL_FRAME)
    /* try to notify if notify callback was not called in same framecount */
    if (request->getCallbackDone(EXYNOS_REQUEST_RESULT::CALLBACK_NOTIFY_ONLY) == false) {
        /* can't send notify cause of one request including render, video */
        if (m_needNotify(request) == true)
            m_sendNotify(frame->getFrameCount(), CAMERA3_MSG_SHUTTER);
    }
#endif

    /* update jpeg size */
    CameraMetadata setting = request->getResultMeta();
    int32_t jpegsize = size;
    ret = setting.update(ANDROID_JPEG_SIZE, &jpegsize, 1);
    if (ret < 0) {
        CLOGE("ERR(%s): ANDROID_JPEG_SIZE update failed(%d)",  __FUNCTION__, ret);
    }
    request->setResultMeta(setting);

    CLOGD("DEBUG(%s[%d]):Set JPEG result Done. frameCount %d request->frame_number %d",
            __FUNCTION__, __LINE__,
            frame->getFrameCount(), request->getFrameCount());

    resultRequest = m_requestMgr->createResultRequest(frame->getFrameCount(), EXYNOS_REQUEST_RESULT::CALLBACK_BUFFER_ONLY, NULL, NULL);
    resultRequest->pushStreamBuffer(&streamBuffer);

    m_requestMgr->callbackSequencerLock();
    request->increaseCompleteBufferCount();
    m_requestMgr->callbackRequest(resultRequest);
    m_requestMgr->callbackSequencerUnlock();

    return ret;
}

ExynosCameraRequest* ExynosCamera3::m_popResult(CameraMetadata &result, uint32_t frameCount)
{
    ExynosCameraRequest *request = NULL;
    struct camera2_shot_ext dst_ext;

    request = m_requestMgr->getServiceRequest(frameCount);
    if (request == NULL) {
        CLOGE("ERR(%s[%d]):getRequest failed ",__FUNCTION__, __LINE__);
        result.clear();
        return NULL;
    }

    result = request->getResultMeta();

    CLOGV("DEBUG(%s[%d]):m_popResult(%d)", __FUNCTION__, __LINE__, request->getFrameCount());

    return request;
}

status_t ExynosCamera3::m_deleteRequest(uint32_t frameCount)
{
    status_t ret = OK;

    ret = m_requestMgr->deleteServiceRequest(frameCount);

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
    int bufferCount = 0;
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

    CLOGI("(%s[%d]):Max Picture %ssize %dx%d format %x",
            __FUNCTION__, __LINE__,
            (m_parameters->getHighSpeedRecording() == true)? "on HighSpeedRecording ":"",
            maxPictureW, maxPictureH, pictureFormat);
    CLOGI("(%s[%d]):MAX Thumbnail size %dx%d",
            __FUNCTION__, __LINE__,
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
        bufferCount = NUM_REPROCESSING_BUFFERS;
        type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;
        allocMode = BUFFER_MANAGER_ALLOCATION_ONDEMAND;

        ret = m_allocBuffers(
                m_ispReprocessingBufferMgr,
                planeCount, planeSize, bytesPerLine,
                minBufferCount, maxBufferCount, type,
                allocMode, true, false);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_ispReprocessingBufferMgr m_allocBuffers(minBufferCount=%d/maxBufferCount=%d) fail",
                    __FUNCTION__, __LINE__, minBufferCount, maxBufferCount);
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
        CLOGE("ERR(%s[%d]):m_yuvCaptureBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
            __FUNCTION__, __LINE__, minBufferCount, maxBufferCount);
        return ret;
    }

    /* Reprocessing Thumbanil buffer */
    switch (pictureFormat) {
        case V4L2_PIX_FMT_NV21M:
            planeCount      = 3;
            planeSize[0]    = maxThumbnailW * maxThumbnailH;
            planeSize[1]    = maxThumbnailW * maxThumbnailH / 2;
        case V4L2_PIX_FMT_NV21:
        default:
            planeCount      = 2;
            planeSize[0]    = FRAME_SIZE(V4L2_PIX_2_HAL_PIXEL_FORMAT(pictureFormat), maxThumbnailW, maxThumbnailH);
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
        CLOGE("ERR(%s[%d]):m_thumbnailBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
                __FUNCTION__, __LINE__, minBufferCount, maxBufferCount);
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
        CLOGE("ERR(%s[%d]):Can't start FrameFactory!!!! FrameFactory is NULL!!",
                __FUNCTION__, __LINE__);

        return false;
    } else if (factory->isCreated() == false) {
        CLOGE("ERR(%s[%d]):Reprocessing FrameFactory is NOT created!", __FUNCTION__, __LINE__);
        return false;
    } else if (factory->isRunning() == true) {
        CLOGW("ERR(%s[%d]):Reprocessing FrameFactory is already running", __FUNCTION__, __LINE__);
        return false;
    }

    /* Set buffer manager */
    ret = m_setupReprocessingPipeline();
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):Failed to setupReprocessingPipeline. ret %d",
                __FUNCTION__, __LINE__, ret);
        return false;
    }

    ret = factory->initPipes();
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):Failed to initPipes. ret %d",
                __FUNCTION__, __LINE__, ret);
        return false;
    }

    ret = m_startReprocessingFrameFactory(factory);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):Failed to startReprocessingFrameFactory",
                __FUNCTION__, __LINE__);
        /* TODO: Need release buffers and error exit */
        return false;
    }

    return false;
}

status_t ExynosCamera3::m_startReprocessingFrameFactory(ExynosCamera3FrameFactory *factory)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = 0;

    CLOGD("DEBUG(%s[%d]):- IN -", __FUNCTION__, __LINE__);

    ret = factory->preparePipes();
    if (ret < 0) {
        CLOGE("ERR(%s):m_reprocessingFrameFactory preparePipe fail", __FUNCTION__);
        return ret;
    }

    /* stream on pipes */
    ret = factory->startPipes();
    if (ret < 0) {
        CLOGE("ERR(%s):m_reprocessingFrameFactory startPipe fail", __FUNCTION__);
        return ret;
    }

    return NO_ERROR;
}

status_t ExynosCamera3::m_stopReprocessingFrameFactory(ExynosCamera3FrameFactory *factory)
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);
    status_t ret = 0;

    if (factory != NULL && factory->isRunning() == true) {
        ret = factory->stopPipes();
        if (ret < 0) {
            CLOGE("ERR(%s):m_reprocessingFrameFactory0>stopPipe() fail", __FUNCTION__);
        }
    }

    CLOGD("DEBUG(%s[%d]):clear m_captureProcessList(Picture) Frame list", __FUNCTION__, __LINE__);
    ret = m_clearList(&m_captureProcessList, &m_captureProcessLock);
    if (ret < 0) {
        CLOGE("ERR(%s):m_clearList fail", __FUNCTION__);
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
            CLOGW("WRAN(%s[%d]):retry(%d) setupEntity for pipeId(%d)", __FUNCTION__, __LINE__, retry, pipeId);
    } while(ret < 0 && retry < (TOTAL_WAITING_TIME/WAITING_TIME));

    return ret;
}

bool ExynosCamera3::m_startPictureBufferThreadFunc(void)
{
    int ret = 0;

    if (m_parameters->isReprocessing() == true) {
        ret = m_setReprocessingBuffer();
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_setReprocessing Buffer fail", __FUNCTION__, __LINE__);
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
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);
    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;
    int loop = false;
    int ret = 0;

    m_timer.start();

    m_companionNode = new ExynosCameraNode();

    ret = m_companionNode->create("companion", m_cameraId);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):Companion Node create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
    }

    ret = m_companionNode->open(MAIN_CAMERA_COMPANION_NUM);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):Companion Node open fail, ret(%d)", __FUNCTION__, __LINE__, ret);
    }
    CLOGD("DEBUG(%s):Companion Node(%d) opened running)", __FUNCTION__, MAIN_CAMERA_COMPANION_NUM);

    ret = m_companionNode->setInput(m_getSensorId(m_cameraId));
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):Companion Node s_input fail, ret(%d)", __FUNCTION__, __LINE__, ret);
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
bool ExynosCamera3::m_eepromThreadFunc(void)
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

status_t ExynosCamera3::m_startCompanion(void)
{
#ifdef SAMSUNG_COMPANION
    if(m_use_companion == true) {
        if (m_companionNode == NULL) {
            m_companionThread = new mainCameraThread(this, &ExynosCamera3::m_companionThreadFunc,
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
        m_eepromThread = new mainCameraThread(this, &ExynosCamera3::m_eepromThreadFunc,
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

status_t ExynosCamera3::m_stopCompanion(void)
{
#ifdef SAMSUNG_COMPANION
    if(m_use_companion == true) {
        if (m_companionThread != NULL) {
            CLOGI("INFO(%s[%d]):wait m_companionThread", __FUNCTION__, __LINE__);
            m_companionThread->requestExitAndWait();
            CLOGI("INFO(%s[%d]):wait m_companionThread end", __FUNCTION__, __LINE__);
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

status_t ExynosCamera3::m_waitCompanionThreadEnd(void)
{
    ExynosCameraDurationTimer timer;

    timer.start();

#ifdef SAMSUNG_COMPANION
    if (m_use_companion == true) {
        if (m_companionThread != NULL) {
            m_companionThread->join();
        } else {
            CLOGI("INFO(%s[%d]):m_companionThread is NULL", __FUNCTION__, __LINE__);
        }
    }
#endif

    timer.stop();
    CLOGI("DEBUG(%s[%d]):companion waiting time : duration time(%5d msec)",
        __FUNCTION__, __LINE__, (int)timer.durationMsecs());

    CLOGI("DEBUG(%s[%d]):companionThread join", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCamera3::m_generateInternalFrame(uint32_t frameCount, ExynosCamera3FrameFactory *factory, List<ExynosCameraFrame *> *list, Mutex *listLock, ExynosCameraFrame **newFrame)
{
    status_t ret = OK;
    *newFrame = NULL;

    CLOGV("DEBUG(%s[%d]):(%d)", __FUNCTION__, __LINE__, frameCount);
    ret = m_searchFrameFromList(list, listLock, frameCount, newFrame);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):searchFrameFromList fail", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    if (*newFrame == NULL) {
        *newFrame = factory->createNewFrame(frameCount);
        if (*newFrame == NULL) {
            CLOGE("ERR(%s):newFrame is NULL", __FUNCTION__);
            return UNKNOWN_ERROR;
        }
        listLock->lock();
        list->push_back(*newFrame);
        listLock->unlock();
    }

    /* Set frame type into FRAME_TYPE_INTERNAL */
    ret = (*newFrame)->setFrameInfo(m_parameters, frameCount, FRAME_TYPE_INTERNAL);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):Failed to setFrameInfo with INTERNAL. frameCount %d",
                __FUNCTION__, __LINE__, frameCount);
        return ret;
    }

    return ret;
}

bool ExynosCamera3::m_internalFrameThreadFunc(void)
{
    status_t ret = 0;
    int index = 0;
    ExynosCameraFrame *newFrame = NULL;

    /* 1. Get new internal frame */
    ret = m_internalFrameDoneQ->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGW("WARN(%s):wait timeout", __FUNCTION__);
        } else {
            CLOGE("ERR(%s):wait and pop fail, ret(%d)", __FUNCTION__, ret);
            /* TODO: doing exception handling */
        }
        return false;
    }

    CLOGV("DEBUG(%s[%d]):handle internal frame : previewStream frameCnt(%d) (%d)",
            __FUNCTION__, __LINE__, newFrame->getFrameCount(), newFrame->getFrameType());

    /* 2. Redirection for the normal frame */
    if (newFrame->getFrameType() != FRAME_TYPE_INTERNAL) {
        CLOGE("DEBUG(%s[%d]):push to m_pipeFrameDoneQ handler : previewStream frameCnt(%d)",
                __FUNCTION__, __LINE__, newFrame->getFrameCount());
        m_pipeFrameDoneQ[PIPE_3AA]->pushProcessQ(&newFrame);
        return true;
    }

    /* 3. Handle the internal frame for each pipe */
    ret = m_handleInternalFrame(newFrame);

    if (ret < 0) {
        CLOGE("ERR(%s[%d]):handle preview frame fail", __FUNCTION__, __LINE__);
        return ret;
    }

    if (newFrame->isComplete() == true/* && newFrame->getFrameCapture() == false */) {
        ret = m_removeFrameFromList(&m_processList, &m_processLock, newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):remove frame from processList fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }

        CLOGV("DEBUG(%s[%d]):internal frame complete, count(%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());
        newFrame->decRef();
        m_frameMgr->deleteFrame(newFrame);
        newFrame = NULL;
    }

    return true;
}

status_t ExynosCamera3::m_handleInternalFrame(ExynosCameraFrame *frame)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameEntity *entity = NULL;
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraBuffer buffer;
    ExynosCamera3FrameFactory *factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
    ExynosCameraStream *stream = NULL;
    camera3_capture_result_t captureResult;
    camera3_notify_msg_t notityMsg;
    ExynosCameraRequest* request = NULL;
    ResultRequest resultRequest = NULL;

    struct camera2_shot_ext meta_shot_ext;
    struct camera2_dm *dm = NULL;
    uint32_t framecount = 0;

    entity = frame->getFrameDoneFirstEntity();
    if (entity == NULL) {
        CLOGE("ERR(%s[%d]):current entity is NULL", __FUNCTION__, __LINE__);
        /* TODO: doing exception handling */
        return true;
    }
    CLOGV("DEBUG(%s[%d]):handle internal frame : previewStream frameCnt(%d) entityID(%d)",
            __FUNCTION__, __LINE__, frame->getFrameCount(), entity->getPipeId());

    switch(entity->getPipeId()) {
    case PIPE_3AA:
        /* Notify ShotDone to mainThread */
        framecount = frame->getFrameCount();
        m_shotDoneQ->pushProcessQ(&framecount);

        ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getSrcBuffer fail, pipeId(%d), ret(%d)",
                   __FUNCTION__, __LINE__, entity->getPipeId(), ret);
            return ret;
        }

        if (buffer.index >= 0) {
            if (m_parameters->isFlite3aaOtf() == false)
                ret = m_putBuffers(m_fliteBufferMgr, buffer.index);
            else
                ret = m_putBuffers(m_3aaBufferMgr, buffer.index);

            if (ret < 0) {
                CLOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
            }
        }

        if (frame->getRequest(PIPE_3AP) == true) {
            ret = frame->getDstBuffer(entity->getPipeId(), &buffer);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):getDstBuffer failed, pipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                return ret;
            }

            if (buffer.index >= 0) {
                ret = m_putBuffers(m_ispBufferMgr, buffer.index);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):put Buffer failed", __FUNCTION__, __LINE__);
                }
            }
        }

        // TODO: Check the condition
        if(frame->getRequest(PIPE_3AC) == true || m_parameters->isFlite3aaOtf() == false) {
            CLOGD("DEBUG(%s[%d]):[EJ] Pass the frame to handleBayerBuffer %d / m_isNeedRequestFrame = %d, m_isNeedInternalFrame = %d",
                        __FUNCTION__, __LINE__, frame->getFrameCount(), m_isNeedRequestFrame, m_isNeedInternalFrame);
            ret = m_handleBayerBuffer(frame);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to handle bayerBuffer(3AA). framecount %d pipeId %d ret %d",
                        __FUNCTION__, __LINE__,
                        frame->getFrameCount(), entity->getPipeId(), ret);
                return ret;
            }
        }


        frame->setMetaDataEnable(true);
        break;
    case PIPE_FLITE:
        /* TODO: HACK: Will be removed, this is driver's job */
        if (m_parameters->isFlite3aaOtf() == true) {
            ret = m_handleBayerBuffer(frame);
            if (ret < NO_ERROR) {
                CLOGE("ERR(%s[%d]):Handle bayer buffer failed, framecount(%d), pipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, frame->getFrameCount(), entity->getPipeId(), ret);
                return ret;
            }
        } else {
            ret = frame->getDstBuffer(entity->getPipeId(), &buffer);

            if (ret < 0) {
                CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                return ret;
            }

            CLOGV("DEBUG(%s[%d]):Deliver Flite Buffer to 3AA. driver->framecount %d hal->framecount %d",
                    __FUNCTION__, __LINE__,
                    getMetaDmRequestFrameCount((struct camera2_shot_ext *)buffer.addr[buffer.planeCount-1]),
                    frame->getFrameCount());

            ret = m_setupEntity(PIPE_3AA, frame, &buffer, NULL);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):setSrcBuffer failed, pipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, PIPE_3AA, ret);
                return ret;
            }

            factory->pushFrameToPipe(&frame, PIPE_3AA);
        }

        break;
    case PIPE_ISP:
        ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Failed to getSrcBuffer. pipeId %d ret %d",
                    __FUNCTION__, __LINE__, entity->getPipeId(), ret);
        }
        if (buffer.index >= 0) {
            ret = m_putBuffers(m_ispBufferMgr, buffer.index);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to putBuffer into ispBufferMgr. bufferIndex %d",
                        __FUNCTION__, __LINE__, buffer.index);
                return ret;
            }
        }
        break;
    case PIPE_MCSC:
        ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Failed to getSrcBuffer. pipeId %d ret %d",
                    __FUNCTION__, __LINE__, entity->getPipeId(), ret);
            return ret;
        }

        if (buffer.index >= 0) {
            ret = m_putBuffers(m_mcscBufferMgr, buffer.index);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to putBuffer into mcscBufferMgr. bufferIndex %d",
                        __FUNCTION__, __LINE__, buffer.index);
                return ret;
            }
        }
        break;
    default:
        CLOGE("ERR(%s[%d]):Invalid pipe ID", __FUNCTION__, __LINE__);
        break;
    }

    ret = frame->setEntityState(entity->getPipeId(), ENTITY_STATE_COMPLETE);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setEntityState fail, pipeId(%d), state(%d), ret(%d)",
            __FUNCTION__, __LINE__, entity->getPipeId(), ENTITY_STATE_COMPLETE, ret);
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
    ExynosCameraRequest* request = NULL;
    ExynosCameraStream *stream = NULL;
    ResultRequest resultRequest = NULL;
    List<int> *outputStreamId;
    List<int>::iterator outputStreamIdIter;
    int requestIndex = -1;
    int streamId = 0;

    request = m_requestMgr->getServiceRequest(frameCount);
    if (request == NULL) {
        CLOGE("ERR(%s[%d]):Failed to getServiceRequest. frameCount %d",
                __FUNCTION__, __LINE__, frameCount);
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
                CLOGE("ERR(%s[%d]):Failed to getStream. frameCount %d requestKey %d",
                        __FUNCTION__, __LINE__, frameCount, request->getKey());
                return INVALID_OPERATION;
            }

            stream->getID(&streamId);

            switch (streamId % HAL_STREAM_ID_MAX) {
            case HAL_STREAM_ID_RAW:
            case HAL_STREAM_ID_ZSL_OUTPUT:
            case HAL_STREAM_ID_JPEG:
                outputStreamIdIter++;
                continue;
            case HAL_STREAM_ID_PREVIEW:
            case HAL_STREAM_ID_VIDEO:
            case HAL_STREAM_ID_CALLBACK:
                break;
            default:
                CLOGE("ERR(%s[%d]):Inavlid stream Id %d",
                        __FUNCTION__, __LINE__, streamId);
                return BAD_VALUE;
            }

            ret = stream->getStream(&streamBuffer.stream);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to getStream. frameCount %d requestKey %d",
                        __FUNCTION__, __LINE__, frameCount, request->getKey());
                return ret;
            }

            requestIndex = -1;
            stream->getBufferManager(&bufferMgr);
            ret = bufferMgr->getBuffer(&requestIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &buffer);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to getBuffer. frameCount %d requestKey %d streamId %d",
                        __FUNCTION__, __LINE__, frameCount, request->getKey(), streamId);
                return ret;
            }

            ret = bufferMgr->getHandleByIndex(&streamBuffer.buffer, buffer.index);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to getHandleByIndex. frameCount %d requestKey %d streamId %d bufferIndex %d",
                        __FUNCTION__, __LINE__, frameCount, request->getKey(), streamId, buffer.index);
                return ret;
            }

            streamBuffer.status = CAMERA3_BUFFER_STATUS_OK;
            streamBuffer.acquire_fence = -1;
            streamBuffer.release_fence = -1;

            resultRequest = m_requestMgr->createResultRequest(frameCount, EXYNOS_REQUEST_RESULT::CALLBACK_BUFFER_ONLY, NULL, NULL);
            resultRequest->pushStreamBuffer(&streamBuffer);
            m_requestMgr->callbackSequencerLock();
            request->increaseCompleteBufferCount();
            m_requestMgr->callbackRequest(resultRequest);
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
    int pipeIdScp = 0;
    ExynosCamera3FrameFactory *factory = NULL;

    for (loopCount = 0; loopCount < MONITOR_THREAD_INTERVAL; loopCount += (MONITOR_THREAD_INTERVAL/20)) {
        if (m_flushFlag == true) {
            CLOGI("INFO(%s[%d]):m_flushFlag(%d)", __FUNCTION__, __LINE__, m_flushFlag);
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
        CLOGW("WARN(%s[%d]:frameFactory is NULL", __FUNCTION__, __LINE__);
        return false;
    }

    if (factory->checkPipeThreadRunning(pipeIdScp) == false) {
        CLOGW("WARN(%s[%d]):Scp pipe is not running.. Skip monitoring.", __FUNCTION__, __LINE__);
        return false;
    }
#ifdef MONITOR_LOG_SYNC
    uint32_t pipeIdIsp = 0;

    if (m_parameters->isFlite3aaOtf() == true || getCameraId() == CAMERA_ID_BACK)
        pipeIdIsp = PIPE_3AA;
    else
        pipeIdIsp = PIPE_3AA;

    /* If it is not front camera in dual and sensor pipe is running, do sync log */
    if (factory->checkPipeThreadRunning(pipeIdIsp) &&
        !(getCameraId() == CAMERA_ID_FRONT && m_parameters->getDualMode())) {
        if (!(m_syncLogDuration % MONITOR_LOG_SYNC_INTERVAL)) {
            uint32_t syncLogId = m_getSyncLogId();
            CLOGI("INFO(%s[%d]):@FIMC_IS_SYNC %d", __FUNCTION__, __LINE__, syncLogId);
            factory->syncLog(pipeIdIsp, syncLogId);
        }
        m_syncLogDuration++;
    }
#endif
    factory->getControl(V4L2_CID_IS_G_DTPSTATUS, &dtpStatus, pipeIdFlite);

    if (dtpStatus == 1) {
        CLOGE("[%s] (%d) (%d)", __FUNCTION__, __LINE__, dtpStatus);
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
        CLOGE("[%s] (%d) (%d)", __FUNCTION__, __LINE__, dtpStatus);
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
        CLOGE("[%s] (%d) (%d)", __FUNCTION__, __LINE__, *threadState);
        if((*countRenew > ERROR_DQ_BLOCKED_COUNT))
            CLOGE("[%s] (%d) ERROR_DQ_BLOCKED) ; ERROR_DQ_BLOCKED_COUNT =20", __FUNCTION__, __LINE__);

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
              CLOGD("[%s] (%d) Timestamp callback with CAMERA_MSG_ERROR", __FUNCTION__, __LINE__);
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
        CLOGV("[%s] (%d) (%d)", __FUNCTION__, __LINE__, *threadState);
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
    ExynosCameraFrame *frame = NULL;
    ExynosCameraBuffer buffer;

    ret = m_dumpFrameQ->waitAndPopProcessQ(&frame);
    if (ret == TIMED_OUT) {
        CLOGW("WARN(%s[%d]):Time-out to wait dumpFrameQ", __FUNCTION__, __LINE__);
        goto THREAD_EXIT;
    } else if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):Failed to waitAndPopProcessQ for dumpFrameQ",
                __FUNCTION__, __LINE__);
        goto THREAD_EXIT;
    } else if (frame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        goto THREAD_EXIT;
    }

    memset(filePath, 0, sizeof(filePath));
    snprintf(filePath, sizeof(filePath), "/data/YuvCapture.yuv");

    factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
    pipeId = PIPE_3AA_REPROCESSING;
    nodeIndex = factory->getNodeType(PIPE_MCSC3_REPROCESSING);

    ret = frame->getDstBuffer(pipeId, &buffer, nodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):Failed to getDstBuffer. frameCount %d pipeId %d nodeIndex %d",
                __FUNCTION__, __LINE__,
                frame->getFrameCount(), pipeId, nodeIndex);
        goto THREAD_EXIT;
    }

    size = buffer.size[0];

    ret = dumpToFile((char *)filePath, buffer.addr[0], size);
    if (ret == false) {
        CLOGE("ERR(%s[%d]):Failed to dumpToFile. frameCount %d filePath %s size %d",
                __FUNCTION__, __LINE__,
                frame->getFrameCount(), filePath, size);
        goto THREAD_EXIT;
    }

THREAD_EXIT:
    if (m_dumpFrameQ->getSizeOfProcessQ() > 0) {
        CLOGI("INFO(%s[%d]):run again. dumpFrameQ size %d",
                __FUNCTION__, __LINE__, m_dumpFrameQ->getSizeOfProcessQ());
        return true;
    } else {
        CLOGI("INFO(%s[%d]):Complete to dumpFrame. frameCount %d",
                __FUNCTION__, __LINE__, frame->getFrameCount());
        return false;
    }
}
#endif

status_t ExynosCamera3::m_generateDuplicateBuffers(ExynosCameraFrame *frame, int pipeIdSrc)
{
    status_t ret = NO_ERROR;
    ExynosCameraRequest *halRequest = NULL;
    camera3_capture_request *serviceRequest = NULL;

    ExynosCameraStream *stream = NULL;
    int streamId = 0;

    List<int> *outputStreamId;
    List<int>::iterator outputStreamIdIter;

    if (frame == NULL) {
        CLOGE2("frame is NULL");
        return INVALID_OPERATION;
    }

    halRequest = m_requestMgr->getServiceRequest(frame->getFrameCount());
    if (halRequest == NULL) {
        CLOGE2("halRequest is NULL");
        return INVALID_OPERATION;
    }

    serviceRequest = halRequest->getService();
    if (serviceRequest == NULL) {
        CLOGE2("serviceRequest is NULL");
        return INVALID_OPERATION;
    }

    CLOGV2("frame->getFrameCount(%d) halRequest->getFrameCount(%d) serviceRequest->num_output_buffers(%d)",
        frame->getFrameCount(),
        halRequest->getFrameCount(),
        serviceRequest->num_output_buffers);

    halRequest->getAllRequestOutputStreams(&outputStreamId);

    if (outputStreamId->size() > 0) {
        for (outputStreamIdIter = outputStreamId->begin(); outputStreamIdIter != outputStreamId->end(); outputStreamIdIter++) {
            m_streamManager->getStream(*outputStreamIdIter, &stream);

            if (stream == NULL) {
                CLOGE2("stream is NULL");
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
    int index = 0;
    ExynosCameraFrame *newFrame= NULL;
    dup_buffer_info_t dupBufferInfo;
    ExynosCameraBuffer srcBuffer;
    ExynosCameraBuffer dstBuffer;
    ExynosCameraStream *stream = NULL;
    camera3_stream_buffer_t streamBuffer;
    ExynosCameraRequest *request = NULL;
    ResultRequest resultRequest = NULL;
    int actualFormat = 0;
    int bufIndex = -1;

    unsigned int completeBufferCount = 0;

    ExynosCameraBufferManager *bufferMgr = NULL;

    ret = m_duplicateBufferDoneQ->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGW2("wait timeout");
        } else {
            CLOGE2("wait and pop fail, ret(%d)", ret);
            /* TODO: doing exception handling */
        }
        goto func_exit;
    }

    if (newFrame == NULL) {
        CLOGE2("frame is NULL");
        goto func_exit;
    }

    CLOGV2("CSC done (frameCount(%d))", newFrame->getFrameCount());

    dupBufferInfo = newFrame->getDupBufferInfo();
    CLOGV2("streamID(%d), extScalerPipeID(%d)", dupBufferInfo.streamID, dupBufferInfo.extScalerPipeID);

    ret = m_streamManager->getStream(dupBufferInfo.streamID, &stream);
    if (ret < 0) {
        CLOGE2("getStream is failed, from streammanager. Id error:HAL_STREAM_ID_PREVIEW");
        goto func_exit;
    }

    ret = stream->getStream(&streamBuffer.stream);
    if (ret < 0) {
        CLOGE2("ERR(%s[%d]):getStream is failed, from exynoscamerastream. Id error:HAL_STREAM_ID_PREVIEW");
        goto func_exit;
    }

    stream->getBufferManager(&bufferMgr);
    CLOGV2("bufferMgr(%p)", bufferMgr);

    ret = newFrame->getDstBuffer(dupBufferInfo.extScalerPipeID, &dstBuffer);
    if (ret < 0) {
        CLOGE2("getDstBuffer fail, pipeId(%d), ret(%d)", dupBufferInfo.extScalerPipeID, ret);
        goto func_exit;
    }

    ret = bufferMgr->getHandleByIndex(&streamBuffer.buffer, dstBuffer.index);
    if (ret != OK) {
        CLOGE2("Buffer index error(%d)!!", dstBuffer.index);
        goto func_exit;
    }

    /* update output stream buffer information */
    streamBuffer.status = CAMERA3_BUFFER_STATUS_OK;
    streamBuffer.acquire_fence = -1;
    streamBuffer.release_fence = -1;

    request = m_requestMgr->getServiceRequest(newFrame->getFrameCount());

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
    request->increaseCompleteBufferCount();
    request->increaseDuplicateBufferCount();

    CLOGV2("OutputBuffer(%d) CompleteBufferCount(%d) DuplicateBufferCount(%d) streamID(%d), extScaler(%d), frame: Count(%d), ServiceBayer(%d), Capture(%d) completeBufferCount(%d)",
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
            CLOGV2("Internal Scp Buffer is returned index(%d)frameCount(%d)", srcBuffer.index, newFrame->getFrameCount());
            ret = m_putBuffers(m_internalScpBufferMgr, srcBuffer.index);
            if (ret < 0) {
                CLOGE2("put Buffer fail");
            }
        }
    }
    m_requestMgr->callbackRequest(resultRequest);
    m_requestMgr->callbackSequencerUnlock();

func_exit:
    if (newFrame != NULL ) {
        newFrame->decRef();
        m_frameMgr->deleteFrame(newFrame);;
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

status_t ExynosCamera3::m_doDestCSC(bool enableCSC, ExynosCameraFrame *frame, int pipeIdSrc, int halStreamId, int pipeExtScalerId)
{
    status_t ret = OK;
    ExynosCameraFrame *newFrame = NULL;
    ExynosRect srcRect, dstRect;
    ExynosCamera3FrameFactory *factory = NULL;
    ExynosCameraBuffer srcBuffer;
    ExynosCameraBuffer dstBuffer;
    ExynosCameraStream *stream = NULL;
    camera3_stream_buffer_t streamBuffer;
    ExynosCameraRequest *request = NULL;
    ResultRequest resultRequest = NULL;
    int actualFormat = 0;
    int bufIndex = -1;
    dup_buffer_info_t dupBufferInfo;
    struct camera2_stream *meta = NULL;
    uint32_t *output = NULL;

    ExynosCameraBufferManager *bufferMgr = NULL;

    CLOGI2("Start: FrameCnt=%d, pipeIdSrc=%d, streamId=%d, pipeExtScalerId=%d",
            frame->getFrameCount(), pipeIdSrc, halStreamId, pipeExtScalerId);

    if (enableCSC == false) {
        /* TODO: memcpy srcBuffer, dstBuffer */
        return NO_ERROR;
    }

    request = m_requestMgr->getServiceRequest(frame->getFrameCount());
    factory = request->getFrameFactory(halStreamId);
    ret = frame->getDstBuffer(pipeIdSrc, &srcBuffer, factory->getNodeType(PIPE_MCSC2_REPROCESSING));
    if (ret < 0) {
        CLOGE2("getDstBuffer fail, pipeId(%d), ret(%d)", pipeIdSrc, ret);
        return ret;
    }

    ret = m_streamManager->getStream(halStreamId, &stream);
    if (ret < 0) {
        CLOGE2("getStream is failed, from streammanager. Id error:HAL_STREAM_ID_PREVIEW");
        return ret;
    }

    ret = stream->getStream(&streamBuffer.stream);
    if (ret < 0) {
        CLOGE2("getStream is failed, from exynoscamerastream. Id error:HAL_STREAM_ID_PREVIEW");
        return ret;
    }

    meta = (struct camera2_stream*)srcBuffer.addr[srcBuffer.planeCount-1];
    output = meta->output_crop_region;
    if(meta == NULL) {
        CLOGE2("Meta plane is not exist or null.");
        return NOT_ENOUGH_DATA;
    }

    stream->getBufferManager(&bufferMgr);
    CLOGV2("bufferMgr(%p)", bufferMgr);

    ret = bufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &dstBuffer);
    if (ret < 0) {
        CLOGE2("bufferMgr getBuffer fail, frameCount(%d), ret(%d)", frame->getFrameCount(), ret);
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
        CLOGE2("halStreamId is not valid(%d)", halStreamId);
        break;
    }
    if (ret != NO_ERROR) {
        CLOGE2("calcGSCRect fail, frameCount(%d), ret(%d)", frame->getFrameCount(), ret);
        return ret;
    }
#endif

    newFrame = factory->createNewFrameOnlyOnePipe(pipeExtScalerId, frame->getFrameCount());

    ret = newFrame->setSrcRect(pipeExtScalerId, srcRect);
    if (ret != NO_ERROR) {
        CLOGE2("setSrcRect fail, frameCount(%d), ret(%d)", frame->getFrameCount(), ret);
        return ret;
    }

    stream->getFormat(&actualFormat);
    dstRect.x = 0;
    dstRect.y = 0;
    dstRect.fullW = dstRect.w = streamBuffer.stream->width;
    dstRect.fullH = dstRect.h = streamBuffer.stream->height;
    dstRect.colorFormat = HAL_PIXEL_FORMAT_2_V4L2_PIX(actualFormat);

    CLOGV2("srcRect size (%d, %d, %d, %d %d %d)",
        srcRect.x, srcRect.y, srcRect.w, srcRect.h, srcRect.fullW, srcRect.fullH);
    CLOGV2("dstRect size (%d, %d, %d, %d %d %d)",
        dstRect.x, dstRect.y, dstRect.w, dstRect.h, dstRect.fullW, dstRect.fullH);

    ret = newFrame->setDstRect(pipeExtScalerId, dstRect);
    if (ret != NO_ERROR) {
        CLOGE2("setDstRect fail, frameCount(%d), ret(%d)", frame->getFrameCount(), ret);
        return ret;
    }

    /* GSC can be shared by preview and previewCb. Make sure dstBuffer for previewCb buffer. */
    /* m_resetBufferState(pipeExtScalerId, frame); */
    m_resetBufferState(pipeExtScalerId, newFrame);

    ret = m_setupEntity(pipeExtScalerId, newFrame, &srcBuffer, &dstBuffer);
    if (ret < 0) {
        CLOGE2("setupEntity fail, pipeExtScalerId(%d), ret(%d)", pipeExtScalerId, ret);
    }

    dupBufferInfo.streamID = halStreamId;
    dupBufferInfo.extScalerPipeID = pipeExtScalerId;
    newFrame->setDupBufferInfo(dupBufferInfo);
    newFrame->setFrameCapture(frame->getFrameCapture());
    newFrame->setFrameServiceBayer(frame->getFrameServiceBayer());

    factory->setOutputFrameQToPipe(m_duplicateBufferDoneQ, pipeExtScalerId);
    factory->pushFrameToPipe(&newFrame, pipeExtScalerId);

    return ret;
}

status_t ExynosCamera3::m_setInternalScpBuffer(void)
{
    int ret = 0;
    int maxPreviewW = 0, maxPreviewH = 0;
    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    int planeCount  = 0;
    int bufferCount = 0;
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
        CLOGE2("m_internalScpBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail", minBufferCount, maxBufferCount);
        return ret;
    }

    return NO_ERROR;
}

}; /* namespace android */
