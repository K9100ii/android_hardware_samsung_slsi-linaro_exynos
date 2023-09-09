/*
**
** Copyright 2016, Samsung Electronics Co. LTD
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
#define LOG_TAG "ExynosCameraSyncFrameSelectorVendor"

#include "ExynosCameraSyncFrameSelector.h"

namespace android {

status_t ExynosCameraSyncFrameSelector::release(void)
{
    status_t ret = NO_ERROR;
    if (m_parameters->getDualCameraMode() == true) {
        m_dualFrameSelector->flush(m_cameraId);
    }
    {
        Mutex::Autolock lock(m_lock);
        /* call parent release() */
        ret = ExynosCameraFrameSelector::release();
        if (ret < 0) {
            CLOGE("release() is fail!!");
            ret = INVALID_OPERATION;
        }
    }

    return ret;
}

status_t ExynosCameraSyncFrameSelector::manageFrameHoldList(ExynosCameraFrameSP_sptr_t frame, int pipeID, bool isSrc, int32_t dstPos)
{
    status_t ret = NO_ERROR;

    if (frame->getFrameState() == FRAME_STATE_SKIPPED ||
            frame->getFrameState() == FRAME_STATE_INVALID ||
            frame->getFrameType() == FRAME_TYPE_INTERNAL) {
        CLOGV("skip manageSyncFrameHoldList(frame(%d), type(%d), state(%d), syncType(%d)) pipeId(%d), isSrc(%d), dstPos(%d)",
                frame->getFrameCount(),
                frame->getFrameType(),
                frame->getFrameState(),
                frame->getReprocessingSyncType(),
                pipeID, isSrc, dstPos);
        return ret;
    }

    /*
     * HACK: in case of sync mode, there's no support special capture like ois
     */
#if 0
    /* don't sync in cameraHAL anymore!! */
    if (frame->getSyncType() == SYNC_TYPE_SYNC) {
        ret = m_manageNormalFrameHoldList(frame, pipeID, isSrc, dstPos);
    } else {
        ret = ExynosCameraFrameSelector::manageFrameHoldList(frame, pipeID, isSrc, dstPos);
    }
#endif
    ret = ExynosCameraFrameSelector::manageFrameHoldList(frame, pipeID, isSrc, dstPos);
    if (ret < 0) {
        CLOGE("manageFrameHoldList(frame(%d), pipe(%d), isSrc(%d), dstPos(%d)) fail!!",
                frame->getFrameCount(), pipeID, isSrc, dstPos);
        ret = INVALID_OPERATION;
    }

    return ret;
}

status_t ExynosCameraSyncFrameSelector::manageSyncFrameHoldList(ExynosCameraFrameSP_sptr_t frame, int pipeID, bool isSrc, int32_t dstPos)
{
    status_t ret = NO_ERROR;

    if (frame == NULL) {
        CLOGE("frame is null (pipeID(%d), isSrc(%d), dstPos(%d))", pipeID, isSrc, dstPos);
        return INVALID_OPERATION;
    }

    if (m_parameters->getDualCameraMode() == true) {
        /* just change the state in case of coming internal/transition frame */
        switch (m_parameters->getDualStandbyMode()) {
        case ExynosCameraParameters::DUAL_STANDBY_MODE_ACTIVE_IN_POST:
        case ExynosCameraParameters::DUAL_STANDBY_MODE_ACTIVE_IN_SENSOR:
            if (m_state != STATE_STANDBY) {
                CLOGI("This selector is going to be standby");
                ret = clearList(pipeID, isSrc, dstPos);
            }

            /* if this frame has the bayer buffer, try to release it */
            if (frame->getFrameType() != FRAME_TYPE_INTERNAL) {
                m_releaseBuffer(frame, pipeID, isSrc, dstPos);
            }

            m_state = STATE_STANDBY;

            return ret;
        default:
            if (m_state != STATE_NORMAL) {
                CLOGI("This selector is going to be normal");
            }
            m_state = STATE_NORMAL;
            break;
        }

#if 0
        /* don't sync in cameraHAL anymore!! */
        switch (frame->getSyncType()) {
        case SYNC_TYPE_SYNC:
            /*
             * flush() the dual selector in case of toggling
             * from bypass/switch to sync in master camera
             */
            if (m_lastSyncType == SYNC_TYPE_BYPASS ||
                    m_lastSyncType == SYNC_TYPE_SWITCH) {
                ret = clearList(pipeID, isSrc, dstPos);
                if (ret != NO_ERROR) {
                    CLOGE("clearList fail(%d, %d, %d)", pipeID, isSrc, dstPos);
                }
            }
            ret = m_dualFrameSelector->manageNormalFrameHoldList(m_cameraId, frame, pipeID, isSrc, dstPos);
            break;
        case SYNC_TYPE_BYPASS:
        case SYNC_TYPE_SWITCH:
            /*
             * flush() the dual selector in case of toggling
             * from sync to bypass/switch in master camera
             */
            if (m_lastSyncType == SYNC_TYPE_SYNC) {
                int masterCameraId, slaveCameraId;
                getDualCameraId(&masterCameraId, &slaveCameraId);
                ret = clearList(pipeID, isSrc, dstPos);
                if (ret != NO_ERROR) {
                    CLOGE("clearList fail(%d, %d, %d)", pipeID, isSrc, dstPos);
                }
            }

            ret = manageFrameHoldList(frame, pipeID, isSrc, dstPos);
            break;
        }

        m_lastSyncType = frame->getSyncType();
#endif
        ret = manageFrameHoldList(frame, pipeID, isSrc, dstPos);
    } else {
        ret = manageFrameHoldList(frame, pipeID, isSrc, dstPos);
    }

    if (ret != NO_ERROR) {
        CLOGE("manageFrameHoldList fail!! (fcount(%d), frameType(%d), frameState(%d), syncType(%d), " \
              " pipeID(%d), BufferType(%s), dstPos(%d), " \
              " m_state(%d), m_lastSyncType(%d))",
                frame->getFrameCount(),
                frame->getFrameType(),
                frame->getFrameState(),
                frame->getSyncType(),
                pipeID, isSrc, dstPos,
                m_state, m_lastSyncType);
    }

    return ret;
}

ExynosCameraFrameSelector::result_t ExynosCameraSyncFrameSelector::selectFrames(ExynosCameraFrameSP_dptr_t selectedFrame,
        int count, int pipeID, bool isSrc, int tryCount, int32_t dstPos)
{
    result_t result = RESULT_NO_FRAME;

    /*
     * If selector dont' have any frame intentionally
     * like postprocessing standby in dual camera,
     * it return the result "skip_frame".
     * Caller(ExynosCamera) have to make progress "takePicture"
     * without the frame.
     */
    if (m_parameters->getDualCameraMode() == true) {
        /* start check */
        switch (m_parameters->getDualStandbyMode()) {
        case ExynosCameraParameters::DUAL_STANDBY_MODE_ACTIVE_IN_POST:
        case ExynosCameraParameters::DUAL_STANDBY_MODE_ACTIVE_IN_SENSOR:
            return RESULT_SKIP_FRAME;
            break;
        default:
            /* keep going */
            break;
        }
#if 0
        /* don't sync in cameraHAL anymore!! */
        if (m_parameters->getDualCameraReprocessingSyncType() == SYNC_TYPE_SYNC) {
            selectedFrame = m_dualFrameSelector->selectSingleFrame(m_cameraId);
            if (selectedFrame != NULL) {
                result = RESULT_HAS_FRAME;
            }
        } else {
            result = ExynosCameraFrameSelector::selectFrames(selectedFrame, count, pipeID, isSrc, tryCount, dstPos);
        }
#endif
        result = ExynosCameraFrameSelector::selectFrames(selectedFrame, count, pipeID, isSrc, tryCount, dstPos);

        /* end check */
        switch (m_parameters->getDualStandbyMode()) {
        case ExynosCameraParameters::DUAL_STANDBY_MODE_ACTIVE_IN_POST:
        case ExynosCameraParameters::DUAL_STANDBY_MODE_ACTIVE_IN_SENSOR:
            if (selectedFrame != NULL &&
                    selectedFrame->getFrameType() != FRAME_TYPE_INTERNAL) {
                m_releaseBuffer(selectedFrame, pipeID, isSrc, dstPos);
                selectedFrame = NULL;
            }

            result = RESULT_SKIP_FRAME;
            break;
        default:
            if (selectedFrame != NULL &&
                    (selectedFrame->getFrameState() == FRAME_STATE_SKIPPED ||
                     selectedFrame->getFrameState() == FRAME_STATE_INVALID ||
                     selectedFrame->getFrameType() == FRAME_TYPE_INTERNAL)) {
                CLOGE("can't use this frame(%d) type(%d) state(%d)",
                        selectedFrame->getFrameCount(), selectedFrame->getFrameType(),
                        selectedFrame->getFrameState());
                if (selectedFrame->getFrameType() != FRAME_TYPE_INTERNAL)
                    m_releaseBuffer(selectedFrame, pipeID, isSrc, dstPos);
                selectedFrame = NULL;
                result = RESULT_NO_FRAME;
            }
            break;
        }
    } else {
        result = ExynosCameraFrameSelector::selectFrames(selectedFrame, count, pipeID, isSrc, tryCount, dstPos);
    }

    return result;
}

status_t ExynosCameraSyncFrameSelector::clearList(int pipeID, bool isSrc, int32_t dstPos)
{
    status_t ret = NO_ERROR;
    {
        Mutex::Autolock lock(m_lock);
        ret = ExynosCameraFrameSelector::clearList(pipeID, isSrc, dstPos);
        if (ret < 0) {
            CLOGE("clearList(pipe(%d), isSrc(%d), dstPos(%d)) fail!!",
                    pipeID, isSrc, dstPos);
            ret = INVALID_OPERATION;
        }
    }

#if 0
    /* don't sync in cameraHAL anymore!! */
    if (m_parameters->getDualCameraMode() == true) {
        m_dualFrameSelector->flush(m_cameraId);
    }
#endif

    return ret;
}
}
