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

/* #define LOG_NDEBUG 0 */
#define LOG_TAG "ExynosCameraRequestManagerSec"

#include "ExynosCameraRequestManager.h"

namespace android {

ExynosCameraRequestSP_sprt_t ExynosCameraRequestManager::registerToServiceList(camera3_capture_request_t *srcRequest)
{
    status_t ret = NO_ERROR;
    ExynosCameraRequestSP_sprt_t request = NULL;
    CameraMetadata *meta;
    uint32_t bufferCnt = 0;
    camera3_stream_buffer_t *inputbuffer = NULL;
    const camera3_stream_buffer_t *outputbuffer = NULL;
    ExynosCameraStream *stream = NULL;
    ExynosCameraFrameFactory *factory = NULL;
    int32_t streamID = 0;
    int32_t factoryID = 0;
    int reqId;
    bool isZslInput = false;
    struct camera2_shot_ext *shot_ext = NULL;
    int previewPortId = MCSC_PORT_NONE;
    int yuvPortId = MCSC_PORT_NONE;
    int tempPortId = MCSC_PORT_NONE;
    int jpegPortId = MCSC_PORT_NONE;
    int yuvStallPortId = MCSC_PORT_NONE;

    /* Check whether the input buffer (ZSL input) is specified.
       Use zslFramFactory in the following section if ZSL input is used
    */
    request = new ExynosCameraRequest(srcRequest, m_previousMeta);
    bufferCnt = request->getNumOfInputBuffer();
    inputbuffer = request->getInputBuffer();
    for(uint32_t i = 0 ; i < bufferCnt ; i++) {
        stream = static_cast<ExynosCameraStream*>(inputbuffer[i].stream->priv);
        stream->getID(&streamID);
        factoryID = streamID % HAL_STREAM_ID_MAX;

        /* Stream ID validity */
        if(factoryID == HAL_STREAM_ID_ZSL_INPUT) {
            isZslInput = true;
        } else {
            /* Ignore input buffer */
            CLOGE("Invalid input streamID. streamID(%d)", streamID);
        }
    }

    bufferCnt = request->getNumOfOutputBuffer();
    outputbuffer = request->getOutputBuffers();
    for(uint32_t i = 0 ; i < bufferCnt ; i++) {
        stream = static_cast<ExynosCameraStream*>(outputbuffer[i].stream->priv);
        stream->getID(&streamID);
        factoryID = streamID % HAL_STREAM_ID_MAX;

        /* If current request has ZSL Input stream buffer,
         * CALLBACK stream must be processed by reprocessing stream.
         */
        if (isZslInput == true && factoryID == HAL_STREAM_ID_CALLBACK) {
            CLOGV("[R%d]CALLBACK stream will be replaced with CALLBACK_STALL stream", request->getKey());

            factoryID = HAL_STREAM_ID_CALLBACK_STALL;
        }
        ret = m_getFactory(factoryID, &factory, &m_factoryMap, &m_factoryMapLock);
        if (ret < 0) {
            CLOGD("[R%d]m_getFactory is failed. streamID(%d)",
                request->getKey(), streamID);
        }

        switch(factoryID % HAL_STREAM_ID_MAX) {
        case HAL_STREAM_ID_PREVIEW:
            stream->getOutputPortId(&tempPortId);
            if (previewPortId < 0) {
                previewPortId = tempPortId;
            } else {
                previewPortId = (previewPortId > tempPortId) ? tempPortId : previewPortId;
            }
            break;
        case HAL_STREAM_ID_VIDEO:
        case HAL_STREAM_ID_CALLBACK:
            stream->getOutputPortId(&tempPortId);
            if (yuvPortId < 0) {
                yuvPortId = tempPortId;
            } else {
                yuvPortId = (yuvPortId > tempPortId) ? tempPortId : yuvPortId;
            }
            break;
        case HAL_STREAM_ID_CALLBACK_STALL:
            stream->getOutputPortId(&tempPortId);
            if (yuvStallPortId < 0) {
                yuvStallPortId = tempPortId;
                yuvStallPortId = yuvStallPortId + MCSC_PORT_MAX;
            } else {
                yuvStallPortId = yuvStallPortId - MCSC_PORT_MAX;
                yuvStallPortId = (yuvStallPortId > tempPortId) ? tempPortId : yuvStallPortId;
                yuvStallPortId = yuvStallPortId + MCSC_PORT_MAX;
            }
            break;
        case HAL_STREAM_ID_JPEG:
            jpegPortId = MCSC_PORT_3 + MCSC_PORT_MAX;
            break;
        }

        request->pushFrameFactory(streamID, factory);
        request->pushRequestOutputStreams(streamID);
    }

    m_converter->setPreviousMeta(&m_previousMeta);

    meta = request->getServiceMeta();
    if (meta->isEmpty()) {
        CLOGD("meta is EMPTY");
    } else {
        CLOGV("meta is NOT EMPTY");
    }

    CLOGV("m_currReqeustList size(%zu), fn(%d)",
            m_serviceRequests.size(), request->getFrameCount());


    ret = m_converter->convertRequestToShot(request, &reqId);
    request->setRequestId(reqId);

    m_previousMeta = *meta;

    shot_ext = request->getServiceShot();

    if (shot_ext->shot.ctl.stats.faceDetectMode == FACEDETECT_MODE_OFF) {
        request->setDsInputPortId(MCSC_PORT_NONE);
    } else {
        if (previewPortId >= 0) {
            request->setDsInputPortId(previewPortId);
        } else if (yuvPortId >= 0) {
            request->setDsInputPortId(yuvPortId);
        } else if (jpegPortId >= 0) {
            request->setDsInputPortId(jpegPortId);
        } else {
            request->setDsInputPortId(yuvStallPortId);
        }
    }

    if (m_parameters->getRestartStream() == false) {
        ret = m_pushBack(request, &m_serviceRequests, &m_requestLock);
        if (ret < 0){
            CLOGE("request m_pushBack is failed request(%d)", request->getFrameCount());
            request = NULL;
            return NULL;
        }
    } else {
        CLOGD("restartStream flag checked, request[R%d]", request->getKey());
    }

    if (m_getFlushFlag() == false && m_resultCallbackThread->isRunning() == false) {
        m_resultCallbackThread->run();
    }

    return request;
}

void ExynosCameraRequest::m_updateMetaDataU8(uint32_t tag, CameraMetadata &resultMeta)
{
    camera_metadata_entry_t entry;

    entry = m_serviceMeta.find(tag);
    if (entry.count > 0) {
        resultMeta.update(tag, entry.data.u8, entry.count);

        for (size_t i = 0; i < entry.count; i++) {
            CLOGV2("%s[%zu/%zu] is (%d)", get_camera_metadata_tag_name(tag), i, entry.count, entry.data.u8[i]);
        }
    }
}

void ExynosCameraRequest::m_updateMetaDataI32(uint32_t tag, CameraMetadata &resultMeta)
{
    camera_metadata_entry_t entry;

    entry = m_serviceMeta.find(tag);
    if (entry.count > 0) {
        resultMeta.update(tag, entry.data.i32, entry.count);

        for (size_t i = 0; i < entry.count; i++) {
            CLOGV2("%s[%zu/%zu] is (%d)", get_camera_metadata_tag_name(tag), i, entry.count, entry.data.i32[i]);
        }
    }
}

void ExynosCameraRequest::get3AAResultMetaVendor(CameraMetadata &minimal_resultMeta)
{
    m_updateMetaDataU8(ANDROID_CONTROL_AE_MODE, minimal_resultMeta);
    m_updateMetaDataU8(ANDROID_CONTROL_AF_TRIGGER, minimal_resultMeta);

    m_updateMetaDataI32(ANDROID_CONTROL_AF_REGIONS, minimal_resultMeta);
    m_updateMetaDataI32(ANDROID_CONTROL_AE_REGIONS, minimal_resultMeta);
    m_updateMetaDataI32(ANDROID_CONTROL_AWB_REGIONS, minimal_resultMeta);
#ifdef SAMSUNG_CONTROL_METERING
    m_updateMetaDataI32(SAMSUNG_ANDROID_CONTROL_TOUCH_AE_STATE, minimal_resultMeta);
#endif
#ifdef SAMSUNG_TN_FEATURE
    m_updateMetaDataI32(SAMSUNG_ANDROID_LENS_INFO_CURRENTINFO, minimal_resultMeta);
    m_updateMetaDataI32(SAMSUNG_ANDROID_CONTROL_DOF_SINGLE_DATA, minimal_resultMeta);
    m_updateMetaDataI32(SAMSUNG_ANDROID_CONTROL_DOF_MULTI_INFO, minimal_resultMeta);
    m_updateMetaDataI32(SAMSUNG_ANDROID_CONTROL_DOF_MULTI_DATA, minimal_resultMeta);
#endif

#ifdef SAMSUNG_OT
     m_updateMetaDataI32(SAMSUNG_ANDROID_CONTROL_OBJECT_TRACKING_STATE, minimal_resultMeta);
#endif

    return;
}
}; /* namespace android */
