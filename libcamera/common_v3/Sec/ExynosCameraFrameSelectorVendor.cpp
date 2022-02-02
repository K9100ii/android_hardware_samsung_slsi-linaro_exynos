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
#define LOG_TAG "ExynosCameraFrameSelectorSec"

#include "ExynosCameraFrameSelector.h"

#define FLASHED_LLS_COUNT 4

namespace android {

status_t ExynosCameraFrameSelector::release(void)
{
    int ret = 0;
    ret = m_release(&m_frameHoldList);
    if (ret != NO_ERROR) {
        CLOGE("m_frameHoldList release failed ");
    }
    ret = m_release(&m_hdrFrameHoldList);
    if (ret != NO_ERROR) {
        CLOGE("m_hdrFrameHoldList release failed ");
    }

    isCanceled = false;

    return NO_ERROR;
}

status_t ExynosCameraFrameSelector::manageFrameHoldList(ExynosCameraFrameSP_sptr_t frame, int pipeID, bool isSrc, int32_t dstPos)
{
    int ret = 0;

    if (m_parameters->getHdrMode() == true ||
            m_parameters->getShotMode() == SHOT_MODE_RICH_TONE) {
        ret = m_manageHdrFrameHoldList(frame, pipeID, isSrc, dstPos);
    }
    else {
        ret = m_manageNormalFrameHoldList(frame, pipeID, isSrc, dstPos);
    }

    return ret;
}

status_t ExynosCameraFrameSelector::m_manageNormalFrameHoldList(ExynosCameraFrameSP_sptr_t newFrame, int pipeID, bool isSrc, int32_t dstPos)
{
    int ret = 0;
    ExynosCameraFrameSP_sptr_t oldFrame = NULL;
    ExynosCameraBuffer buffer;

    /* Skip INITIAL_SKIP_FRAME only FastenAeStable is disabled */
    /* This previous condition check is useless because almost framecount for capture is over than skip frame count */
    /*
     * if (m_parameters->getUseFastenAeStable() == true ||
     *     newFrame->getFrameCount() > INITIAL_SKIP_FRAME) {
     */
    m_pushQ(&m_frameHoldList, newFrame, true);

    while (m_frameHoldList.getSizeOfProcessQ() > m_frameHoldCount) {
        if (m_popQ(&m_frameHoldList, oldFrame, true, 1) != NO_ERROR ) {
            CLOGE("getBufferToManageQ fail");

            m_bufMgr->printBufferState();
            m_bufMgr->printBufferQState();
        } else {
            /*
            Frames in m_frameHoldList and m_hdrFrameHoldList are locked when they are inserted
            on the list. So we need to use m_LockedFrameComplete() to remove those frames.
            */
            m_LockedFrameComplete(oldFrame, pipeID, isSrc, dstPos);
            oldFrame = NULL;
        }
    }

    return ret;
}

ExynosCameraFrameSelector::result_t ExynosCameraFrameSelector::selectFrames(ExynosCameraFrameSP_dptr_t selectedFrame, int count, int pipeID, bool isSrc, int tryCount, int32_t dstPos)
{
    result_t result = RESULT_NO_FRAME;
    ExynosCameraActivityFlash *m_flashMgr = NULL;
    ExynosCameraActivityAutofocus *afMgr = m_activityControl->getAutoFocusMgr();     // shoud not be a NULL
    ExynosCameraActivitySpecialCapture *m_sCaptureMgr = m_activityControl->getSpecialCaptureMgr();

    m_reprocessingCount = count;

    m_flashMgr = m_activityControl->getFlashMgr();

    if ((m_flashMgr->getNeedCaptureFlash() == true && m_parameters->getSeriesShotCount() == 0
        )
    ) {
        {
            selectedFrame = m_selectFlashFrame(pipeID, isSrc, tryCount, dstPos);
        }
        if (selectedFrame == NULL && !isCanceled) {
            CLOGE("select Flash Frame Fail!");
            selectedFrame = m_selectNormalFrame(pipeID, isSrc, tryCount, dstPos);
        }
    } else if (m_parameters->getHdrMode() == true ||
            m_parameters->getShotMode() == SHOT_MODE_RICH_TONE) {
        selectedFrame = m_selectHdrFrame(pipeID, isSrc, tryCount, dstPos);

        if (selectedFrame == NULL && !isCanceled) {
            CLOGE("select HDR Frame Fail!");
            selectedFrame = m_selectNormalFrame(pipeID, isSrc, tryCount, dstPos);
        }
    } else if (afMgr->getRecordingHint() == true
               || m_parameters->getHighResolutionCallbackMode() == true
               || (m_parameters->getShotMode() > SHOT_MODE_AUTO
                   && m_parameters->getShotMode() != SHOT_MODE_NIGHT
                   && m_parameters->getShotMode() != SHOT_MODE_PRO_MODE)) {
        /*
         On recording mode, do not try to find focused frame but just use normal frame.
         ExynosCameraActivityAutofocus::setRecordingHint() is called
         with true argument on startRecording(), and called with false on
         stopRecording(). So it is used to determine whether the recording
         is currently progressing or not on codes below.
         */

        if(afMgr->getRecordingHint() == true && m_parameters->getRecordingHint() == false) {
            CLOGD("HACK: Applying AFManager recordingHint(true)");
        }

        selectedFrame = m_selectNormalFrame(pipeID, isSrc, tryCount, dstPos);
        if (selectedFrame == NULL)
            CLOGE("select Frame Fail!");
    } else if (m_parameters->getSeriesShotCount() > 0) {
        selectedFrame = m_selectBurstFrame(pipeID, isSrc, tryCount, dstPos);

        if (selectedFrame == NULL && !isCanceled) {
            CLOGE("select focused frame Faile!");
            selectedFrame = m_selectNormalFrame(pipeID, isSrc, tryCount, dstPos);
        }
#ifdef CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT
    } else if (m_parameters->getCaptureExposureTime() > CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT) {
        {
            selectedFrame = m_selectNormalFrame(pipeID, isSrc, tryCount, dstPos);
        }
#endif
    } else {
        selectedFrame = m_selectFocusedFrame(pipeID, isSrc, tryCount, dstPos);

        if (selectedFrame == NULL && !isCanceled) {
            CLOGE("select focused frame Faile!");
            selectedFrame = m_selectNormalFrame(pipeID, isSrc, tryCount, dstPos);
        }
    }

    if (selectedFrame == NULL) {
        CLOGE("selectedFrame is NULL");
    } else {
        CLOGD("selectedFrame is [%d]:frameCount(%d), timeStamp(%d)",
                m_parameters->getCameraId(), selectedFrame->getFrameCount(), (int)ns2ms(selectedFrame->getTimeStamp()));
        result = RESULT_HAS_FRAME;
    }

    m_isFirstFrame = false;

    return result;
}

ExynosCameraFrameSP_sptr_t ExynosCameraFrameSelector::m_selectNormalFrame(__unused int pipeID, __unused bool isSrc, int tryCount, __unused int32_t dstPos)
{
    int ret = 0;
    ExynosCameraFrameSP_sptr_t selectedFrame = NULL;

    int i = 0;
    uint32_t maxFrameCount = 0;

    array<ExynosCameraFrameSP_sptr_t, 4> candidateFrames;

    ExynosCameraBuffer buffer;
    int candidateFrameCount = m_frameHoldCount;
    unsigned int LBPframeCount[candidateFrameCount];

    for(int j = 0; j < candidateFrameCount; j++) {
        LBPframeCount[j] = 0;
    }

    /* Recevied the Frame */
    do {
        ret = m_waitAndpopQ(&m_frameHoldList, candidateFrames[i], false, tryCount);
        if (ret < 0 || candidateFrames[i] == NULL) {
            CLOGD("getFrame Fail ret(%d)", ret);
            return NULL;
        } else if (isCanceled == true) {
            CLOGD("isCanceled");
            if (candidateFrames[i] != NULL) {
                m_LockedFrameComplete(candidateFrames[i], pipeID, isSrc, dstPos);
            }
            return NULL;
        }

        /* Get the Frame Count from metadata. */
        ret = m_getBufferFromFrame(candidateFrames[i], pipeID, isSrc, &buffer, dstPos);
        if (ret != NO_ERROR) {
            CLOGE("m_getBufferFromFrame fail pipeID(%d) BufferType(%s) bufferPtr(%p)",
                    pipeID, (isSrc)? "Src" : "Dst", &buffer);
        }

        if (m_isFrameMetaTypeShotExt() == true) {
            camera2_shot_ext *shot_ext = NULL;
            shot_ext = (camera2_shot_ext *)(buffer.addr[buffer.getMetaPlaneIndex()]);
            if (shot_ext != NULL)
                LBPframeCount[i] = shot_ext->shot.dm.request.frameCount;
            else
                CLOGE("Buffer is null");
        } else {
            camera2_stream *shot_stream = NULL;
            shot_stream = (camera2_stream *)(buffer.addr[buffer.getMetaPlaneIndex()]);
            if (shot_stream != NULL)
                LBPframeCount[i] = shot_stream->fcount;
            else
                CLOGE("Buffer is null");
        }
        /***********************************/

        if(candidateFrameCount != REPROCESSING_BAYER_HOLD_COUNT) {
            if (m_bufMgr == NULL) {
                CLOGE("m_bufMgr is NULL");
                return NULL;
            } else {
                m_frameComplete(candidateFrames[i], false, pipeID, isSrc, dstPos, true);
                candidateFrames[i] = NULL;
            }
        }
        else{
            i++;
        }
    } while( (i < candidateFrameCount));

    if(candidateFrameCount != REPROCESSING_BAYER_HOLD_COUNT) {
        setFrameHoldCount(REPROCESSING_BAYER_HOLD_COUNT);
    }

    /* Selected Frame */
    for (i = 0; i < candidateFrameCount; i++) {
        if (candidateFrameCount == REPROCESSING_BAYER_HOLD_COUNT) {
            CLOGD("[LBP]:selected frame(count %d)", LBPframeCount[i]);
            selectedFrame = candidateFrames[i];
        } else {
            if (m_bufMgr == NULL) {
                CLOGE("m_bufMgr is NULL");
                return NULL;
            } else {
                m_frameComplete(candidateFrames[i], false, pipeID, isSrc, dstPos, true);
                candidateFrames[i] = NULL;
            }
        }
    }

    return selectedFrame;
}

/**
**      USAGE : HAL3, LCD_FLASH
**
**      This function does not include main flash control code.
**
**/
ExynosCameraFrameSP_sptr_t ExynosCameraFrameSelector::m_selectFlashFrameV2(int pipeID, bool isSrc, int tryCount, int32_t dstPos)
{
    int ret = 0;
    ExynosCameraFrameSP_sptr_t selectedFrame = NULL;
    ExynosCameraBuffer selectedBuffer;
    int bufferFcount = 0;
    int waitFcount = 0;
    int totalWaitingCount = 0;

    /* Choose bayerBuffer to process reprocessing */
    while (totalWaitingCount <= (FLASH_MAIN_TIMEOUT_COUNT + m_parameters->getReprocessingBayerHoldCount())) {
        ret = m_waitAndpopQ(&m_frameHoldList, selectedFrame, false, tryCount);
        if (ret < 0 ||  selectedFrame == NULL) {
            CLOGD("getFrame Fail ret(%d)", ret);
            return NULL;
        } else if (isCanceled == true) {
            CLOGD("isCanceled");
            if (selectedFrame != NULL) {
                m_LockedFrameComplete(selectedFrame, pipeID, isSrc, dstPos);
            }
            return NULL;
        }
        CLOGD("Frame Count(%d)", selectedFrame->getFrameCount());

        if (waitFcount == 0) {
            ExynosCameraActivityFlash *flashMgr = NULL;
            flashMgr = m_activityControl->getFlashMgr();
            waitFcount = flashMgr->getShotFcount() + 1;
            CLOGD("best frame count for flash capture : %d", waitFcount);
        }

        /* Handling exception cases : like preflash is not processed */
        if (waitFcount < 0) {
            CLOGW("waitFcount is negative : preflash is not processed");

            return NULL;
        }

        if (isCanceled == true) {
            CLOGD("isCanceled");

            return NULL;
        }

        ret = m_getBufferFromFrame(selectedFrame, pipeID, isSrc, &selectedBuffer, dstPos);
        if (ret != NO_ERROR) {
            CLOGE("m_getBufferFromFrame fail pipeID(%d) BufferType(%s) bufferPtr(%p)",
                    pipeID, (isSrc)? "Src" : "Dst", &selectedBuffer);
        }

        if (m_isFrameMetaTypeShotExt() == true) {
            camera2_shot_ext *shot_ext = NULL;
            shot_ext = (camera2_shot_ext *)(selectedBuffer.addr[selectedBuffer.getMetaPlaneIndex()]);
            if (shot_ext != NULL)
                bufferFcount = shot_ext->shot.dm.request.frameCount;
            else
                CLOGE("selectedBuffer is null");
        } else {
            camera2_stream *shot_stream = NULL;
            shot_stream = (camera2_stream *)(selectedBuffer.addr[selectedBuffer.getMetaPlaneIndex()]);
            if (shot_stream != NULL)
                bufferFcount = shot_stream->fcount;
            else
                CLOGE("selectedBuffer is null");
        }

        /* Put mismatched buffer */
        if (waitFcount != bufferFcount) {
            if (m_bufMgr == NULL) {
                CLOGE("m_bufMgr is NULL");
                return NULL;
            } else {
                m_frameComplete(selectedFrame, false, pipeID, isSrc, dstPos, true);
                selectedFrame = NULL;
            }
        }

        /* On HAL3, frame selector must wait the waitFcount to be updated by Flash Manager */
        if (waitFcount > 1 && waitFcount <= bufferFcount)
            break;
        else
            waitFcount = 0;

        totalWaitingCount++;
        CLOGD(" (totalWaitingCount %d)", totalWaitingCount);
    }

    if (totalWaitingCount > FLASH_MAIN_TIMEOUT_COUNT) {
        CLOGW("fail to get bayer frame count for flash capture (totalWaitingCount %d)",
                totalWaitingCount);
    }

    CLOGD("waitFcount : %d, bufferFcount : %d",
            waitFcount, bufferFcount);

    return selectedFrame;
}

status_t ExynosCameraFrameSelector::m_waitAndpopQ(frame_queue_t *list, ExynosCameraFrameSP_dptr_t outframe, bool unlockflag, int tryCount)
{
    status_t ret = NO_ERROR;
    int iter = 0;

    do {
        ret = list->waitAndPopProcessQ(&outframe);

        if (isCanceled == true) {
            CLOGD("isCanceled");

            return NO_ERROR;
        }

        if( ret < 0 ) {
            if( ret == TIMED_OUT ) {
                CLOGD("waitAndPopQ Time out -> retry[max cur](%d %d)", tryCount, iter);
                iter++;
                continue;
            }
        }

        if (outframe != NULL) {
            CLOGD("Frame Count(%d)", outframe->getFrameCount());
        }
    } while (ret != OK && tryCount > iter);

    if(ret != OK) {
        CLOGE("wait for popQ fail(%d)", ret);
        return ret;
    }

    if(outframe == NULL) {
        CLOGE("wait for popQ frame = NULL");
        return ret;
    }

    if(unlockflag) {
        outframe->frameUnlock();
    }
    return ret;
}

status_t ExynosCameraFrameSelector::clearList(int pipeID, bool isSrc, int32_t dstPos)
{
    int ret = 0;
    ExynosCameraFrameSP_sptr_t frame = NULL;

    if (m_frameHoldList.isWaiting() == false) {
        ret = m_clearList(&m_frameHoldList, pipeID, isSrc, dstPos);
        if( ret < 0 ) {
            CLOGE("m_frameHoldList clear failed, pipeID(%d)", pipeID);
        }
    } else {
        CLOGE("Cannot clear frameHoldList cause waiting for pop frame");
    }

    if (m_hdrFrameHoldList.isWaiting() == false) {
        ret = m_clearList(&m_hdrFrameHoldList, pipeID, isSrc, dstPos);
        if( ret < 0 ) {
            CLOGE("m_hdrFrameHoldList clear failed, pipeID(%d)", pipeID);
        }
    } else {
        CLOGE("Cannot clear hdrFrameHoldList cause waiting for pop frame");
    }

    isCanceled = false;

    return NO_ERROR;
}

uint32_t ExynosCameraFrameSelector::getSizeOfHoldFrame(void)
{
    uint32_t size = 0;

    size += m_frameHoldList.getSizeOfProcessQ();
    size += m_hdrFrameHoldList.getSizeOfProcessQ();

    return size;
}

status_t ExynosCameraFrameSelector::m_releaseBuffer(ExynosCameraFrameSP_sptr_t frame, int pipeID, bool isSrc, int32_t dstPos)
{
    int ret = 0;
    ExynosCameraBuffer bayerBuffer;

#ifdef SUPPORT_DEPTH_MAP
    if (frame->getRequest(PIPE_VC1) == true) {
        int pipeId = PIPE_FLITE;

        if (m_parameters->isFlite3aaOtf() == true)
            pipeId = PIPE_3AA;

        ExynosCameraBuffer depthMapBuffer;

        depthMapBuffer.index = -2;

        ret = frame->getDstBuffer(pipeId, &depthMapBuffer, CAPTURE_NODE_2);
        if (ret != NO_ERROR) {
            CLOGE("Failed to get DepthMap buffer");
        }

        if(m_parameters->getDepthCallbackOnPreview()
            && depthMapBuffer.index != -2
            && m_parameters->getFrameSkipCount() <= 1
            && m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_METADATA) == true
            && m_depthCallbackQ->getSizeOfProcessQ() < 2) {
            m_depthCallbackQ->pushProcessQ(&depthMapBuffer);
            CLOGV("[Depth] push depthMapBuffer.index(%d) HAL count(%d) getSizeOfProcessQ(%d)",
                    depthMapBuffer.index, frame->getFrameCount(),
                    m_depthCallbackQ->getSizeOfProcessQ());
        } else if (depthMapBuffer.index != -2) {
            ret = m_depthMapbufMgr->putBuffer(depthMapBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
            if (ret != NO_ERROR) {
                CLOGE("Failed to put DepthMap buffer to bufferMgr");
            }
            CLOGV("[Depth] pop depthMapBuffer.index(%d) HAL count(%d) \
                    getNumOfAvailableBuffer(%d) getSizeOfProcessQ(%d)",
                    depthMapBuffer.index, frame->getFrameCount(),
                    m_depthMapbufMgr->getNumOfAvailableBuffer(), m_depthCallbackQ->getSizeOfProcessQ());
        }
    }
#endif

    ret = m_getBufferFromFrame(frame, pipeID, isSrc, &bayerBuffer, dstPos);
    if( ret != NO_ERROR ) {
        CLOGE("m_getBufferFromFrame fail pipeID(%d) BufferType(%s) bufferPtr(%p)",
                pipeID, (isSrc)? "Src" : "Dst", &bayerBuffer);
    }
    if (m_bufMgr == NULL) {
        CLOGE("m_bufMgr is NULL");
        return INVALID_OPERATION;
    } else {
        ret = m_bufMgr->putBuffer(bayerBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
        if (ret < 0) {
            CLOGE("putIndex is %d", bayerBuffer.index);
            m_bufMgr->printBufferState();
            m_bufMgr->printBufferQState();
        }
    }
    return NO_ERROR;
}
}
