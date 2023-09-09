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
#define LOG_TAG "ExynosCameraStreamManagerSec"

#include "ExynosCameraStreamManager.h"

namespace android {

int ExynosCameraStreamManager::getOutputPortId(int streamId)
{
    int outputPortId = -1;
    int startIndex = -1;
    int endIndex = -1;

    if (streamId < 0) {
        ALOGE("ERR(%s[%d]):Invalid streamId %d",
                __FUNCTION__, __LINE__, streamId);
        return -1;
    }

    switch (streamId % HAL_STREAM_ID_MAX) {
    case HAL_STREAM_ID_PREVIEW:
    case HAL_STREAM_ID_VIDEO:
    case HAL_STREAM_ID_CALLBACK:
        startIndex = ExynosCameraParameters::YUV_0;
        endIndex = ExynosCameraParameters::YUV_MAX;
        break;
    case HAL_STREAM_ID_CALLBACK_STALL:
        startIndex = ExynosCameraParameters::YUV_STALL_0;
        endIndex = ExynosCameraParameters::YUV_STALL_MAX;
        break;
    case HAL_STREAM_ID_RAW:
    case HAL_STREAM_ID_JPEG:
    case HAL_STREAM_ID_ZSL_OUTPUT:
    case HAL_STREAM_ID_ZSL_INPUT:
    case HAL_STREAM_ID_THUMBNAIL_CALLBACK:
#ifdef SUPPORT_DEPTH_MAP
    case HAL_STREAM_ID_DEPTHMAP:
    case HAL_STREAM_ID_DEPTHMAP_STALL:
#endif
    case HAL_STREAM_ID_VISION:
    default:
        ALOGE("ERR(%s[%d]):NOT YUV Stream ID %d",
                __FUNCTION__, __LINE__, streamId);
        return -1;
    }

    for (int i = startIndex; i < endIndex; i++) {
        if (m_yuvStreamIdMap[i] == streamId)
            outputPortId = i;
    }

    return outputPortId;
}

status_t ExynosCameraStreamManager::m_increaseYuvStreamCount(int streamId)
{
    status_t ret = NO_ERROR;
    int yuvIndex = -1;

    switch (streamId % HAL_STREAM_ID_MAX) {
        case HAL_STREAM_ID_CALLBACK:
            /* Reprocessing stream */
            yuvIndex = ExynosCameraParameters::YUV_STALL_0
                       + getTotalYuvStreamCount();
            m_setYuvStreamId(yuvIndex, streamId);
        case HAL_STREAM_ID_PREVIEW:
        case HAL_STREAM_ID_VIDEO:
            /* Preview stream */
            yuvIndex = ExynosCameraParameters::YUV_0
                       + getTotalYuvStreamCount();
            m_setYuvStreamId(yuvIndex, streamId);
            m_yuvStreamCount++;
            break;
        case HAL_STREAM_ID_CALLBACK_STALL:
            /* Reprocessing stream */
            yuvIndex = ExynosCameraParameters::YUV_STALL_0
                       + getTotalYuvStreamCount();
            m_setYuvStreamId(yuvIndex, streamId);
            m_yuvStallStreamCount++;
            break;
        case HAL_STREAM_ID_RAW:
        case HAL_STREAM_ID_JPEG:
        case HAL_STREAM_ID_ZSL_OUTPUT:
        case HAL_STREAM_ID_ZSL_INPUT:
        case HAL_STREAM_ID_THUMBNAIL_CALLBACK:
#ifdef SUPPORT_DEPTH_MAP
        case HAL_STREAM_ID_DEPTHMAP:
        case HAL_STREAM_ID_DEPTHMAP_STALL:
#endif
        case HAL_STREAM_ID_VISION:
            /* Not YUV streams */
            break;
        default:
            ALOGE("ERR(%s[%d]):Unsupported stream id %d",
                    __FUNCTION__, __LINE__, streamId);
            ret = BAD_VALUE;
            break;
    }

    if (getTotalYuvStreamCount() > m_yuvStreamMaxCount) {
        ALOGE("ERR(%s[%d]):Too many YUV stream!. maxYuvStreamCount %d currentYuvStreamCount %d/%d",
                __FUNCTION__, __LINE__,
                m_yuvStreamMaxCount, getTotalYuvStreamCount(), m_yuvStallStreamCount);
        ret = BAD_VALUE;
    }

    return ret;
}

status_t ExynosCameraStreamManager::m_decreaseYuvStreamCount(int streamId)
{
    int yuvIndex = -1;

    if (streamId < 0) {
        ALOGE("ERR(%s[%d]):Invalid streamId %d",
                __FUNCTION__, __LINE__, streamId);
        return BAD_VALUE;
    }

    switch (streamId % HAL_STREAM_ID_MAX) {
        case HAL_STREAM_ID_CALLBACK:
            /* Reprocessing stream */
            yuvIndex = ExynosCameraParameters::YUV_STALL_0
                       + getOutputPortId(streamId);
            m_setYuvStreamId(yuvIndex, -1);
        case HAL_STREAM_ID_PREVIEW:
        case HAL_STREAM_ID_VIDEO:
            /* Preview stream */
            yuvIndex = getOutputPortId(streamId);
            m_setYuvStreamId(yuvIndex, -1);
            m_yuvStreamCount--;
            break;
        case HAL_STREAM_ID_CALLBACK_STALL:
            /* Reprocessing stream */
            yuvIndex = getOutputPortId(streamId);
            m_setYuvStreamId(yuvIndex, -1);
            m_yuvStallStreamCount--;
            break;
        case HAL_STREAM_ID_RAW:
        case HAL_STREAM_ID_JPEG:
        case HAL_STREAM_ID_ZSL_OUTPUT:
        case HAL_STREAM_ID_ZSL_INPUT:
        case HAL_STREAM_ID_THUMBNAIL_CALLBACK:
#ifdef SUPPORT_DEPTH_MAP
        case HAL_STREAM_ID_DEPTHMAP:
        case HAL_STREAM_ID_DEPTHMAP_STALL:
#endif
        case HAL_STREAM_ID_VISION:
            /* Not YUV streams */
            break;
        default:
            ALOGE("ERR(%s[%d]):Unsupported stream id %d",
                    __FUNCTION__, __LINE__, streamId);
            return BAD_VALUE;
            break;
    }

    return NO_ERROR;
}

}; /* namespace android */
