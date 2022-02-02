/*
**
** Copyright 2017, Samsung Electronics Co. LTD
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

    m_isCanceled = false;

    return NO_ERROR;
}

status_t ExynosCameraFrameSelector::manageFrameHoldListHAL3(ExynosCameraFrameSP_sptr_t frame)
{
    int ret = 0;
    int pipeID;
    bool isSrc;
    int32_t dstPos;

    /* get first tag to match with this selector */
    ExynosCameraFrameSelectorTag compareTag;
    compareTag.selectorId = m_selectorId;
    if (frame->findSelectorTag(&compareTag) == true) {
        pipeID = compareTag.pipeId;
        isSrc = compareTag.isSrc;
        dstPos = compareTag.bufPos;

    } else {
        CLOGE("can't find selectorTag(id:%d) from frame(%d)",
                m_selectorId, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto p_err;
    }

    if (m_configurations->getMode(CONFIGURATION_HDR_MODE) == true
        ) {
        ret = m_manageHdrFrameHoldList(frame, pipeID, isSrc, dstPos);
    }
    else {
        ret = m_manageNormalFrameHoldListHAL3(frame, pipeID, isSrc, dstPos);
    }

    if (ret == NO_ERROR)
	    frame->setStateInSelector(FRAME_STATE_IN_SELECTOR_PUSHED);

p_err:
    return ret;
}

status_t ExynosCameraFrameSelector::m_manageNormalFrameHoldListHAL3(ExynosCameraFrameSP_sptr_t newFrame,
                                                                                __unused int pipeID,
                                                                                __unused bool isSrc,
                                                                                __unused int32_t dstPos)
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

    if (m_frameHoldList.getSizeOfProcessQ() > m_frameHoldCount) {
        if( m_popQ(&m_frameHoldList, oldFrame, true, 1) != NO_ERROR ) {
            ALOGE("ERR(%s[%d]):getBufferToManageQ fail", __FUNCTION__, __LINE__);
#if 0
            m_bufMgr->printBufferState();
            m_bufMgr->printBufferQState();
#endif
        } else {
            /*
            Frames in m_frameHoldList and m_hdrFrameHoldList are locked when they are inserted
            on the list. So we need to use m_LockedFrameComplete() to remove those frames.
            */
            m_LockedFrameComplete(oldFrame);
            oldFrame = NULL;
        }
    }

    return ret;
}

status_t ExynosCameraFrameSelector::m_list_release(frame_queue_t *list)
{
    ExynosCameraFrameSP_sptr_t frame  = NULL;

    while (list->getSizeOfProcessQ() > 0) {
        if (m_popQ(list, frame, true, 1) != NO_ERROR) {
            CLOGE("getBufferToManageQ fail");
#if 0
            m_bufMgr->printBufferState();
            m_bufMgr->printBufferQState();
#endif
        } else {
            m_LockedFrameComplete(frame);
            frame = NULL;
        }
    }

    return NO_ERROR;
}

ExynosCameraFrameSP_sptr_t ExynosCameraFrameSelector::selectCaptureFrames(int count,
                                                                  uint32_t frameCount,
                                                                  int tryCount)
{
    ExynosCameraFrameSP_sptr_t selectedFrame = NULL;
    ExynosCameraActivityFlash *m_flashMgr = NULL;

    m_reprocessingCount = count;
    m_flashMgr = m_activityControl->getFlashMgr();

    if (m_flashMgr->getNeedCaptureFlash() == true
        ) {
        selectedFrame = m_selectFlashFrameV2(tryCount);
        if (selectedFrame == NULL && !m_isCanceled) {
            CLOGE("Failed to selectFlashFrame");
            selectedFrame = m_selectNormalFrame(tryCount);
        }
    }
    else {
        selectedFrame = m_selectCaptureFrame(frameCount, tryCount);
        if (selectedFrame == NULL && !m_isCanceled) {
            CLOGE("Failed to selectCaptureFrame");
            selectedFrame = m_selectNormalFrame(tryCount);
        }
    }

    return selectedFrame;
}

ExynosCameraFrameSP_sptr_t ExynosCameraFrameSelector::m_selectNormalFrame(int tryCount)
{
    int ret = 0;
    ExynosCameraFrameSP_sptr_t selectedFrame = NULL;

    ret = m_waitAndpopQ(&m_frameHoldList, selectedFrame, false, tryCount);
    if (ret < 0 || selectedFrame == NULL) {
        CLOGD("getFrame Fail ret(%d)", ret);
        return NULL;
    } else if (m_isCanceled == true) {
        CLOGD("m_isCanceled");
        if (selectedFrame != NULL) {
            m_LockedFrameComplete(selectedFrame);
        }
        return NULL;
    }
    CLOGD("Frame Count(%d)", selectedFrame->getFrameCount());

    return selectedFrame;
}

/**
**      USAGE : HAL3, LCD_FLASH
**
**      This function does not include main flash control code.
**
**/
ExynosCameraFrameSP_sptr_t ExynosCameraFrameSelector::m_selectFlashFrameV2(int tryCount)
{
    int ret = 0;
    ExynosCameraFrameSP_sptr_t selectedFrame = NULL;
    ExynosCameraBuffer selectedBuffer;
    int bufferFcount = 0;
    int waitFcount = 0;
    int totalWaitingCount = 0;
    ExynosCameraActivityFlash *flashMgr = NULL;
    flashMgr = m_activityControl->getFlashMgr();
    int pipeID;
    bool isSrc;
    int32_t dstPos;

    /* Choose bayerBuffer to process reprocessing */
    while (totalWaitingCount <= (FLASH_MAIN_TIMEOUT_COUNT + m_parameters->getReprocessingBayerHoldCount())) {
        ret = m_waitAndpopQ(&m_frameHoldList, selectedFrame, false, tryCount);
        if (ret < 0 ||  selectedFrame == NULL) {
            CLOGD("getFrame Fail ret(%d)", ret);
            flashMgr->resetShotFcount();
            return NULL;
        } else if (m_isCanceled == true) {
            CLOGD("m_isCanceled");
            if (selectedFrame != NULL) {
                m_LockedFrameComplete(selectedFrame);
            }
            flashMgr->resetShotFcount();
            return NULL;
        }
        CLOGD("Frame Count(%d)", selectedFrame->getFrameCount());

        if (waitFcount == 0) {
            waitFcount = flashMgr->getShotFcount() + 1;
            CLOGD("best frame count for flash capture : %d", waitFcount);
        }

        /* Handling exception cases : like preflash is not processed */
        if (waitFcount < 0) {
            CLOGW("waitFcount is negative : preflash is not processed");

            flashMgr->resetShotFcount();
            return NULL;
        }

        if (m_isCanceled == true) {
            CLOGD("m_isCanceled");

            flashMgr->resetShotFcount();
            return NULL;
        }

        ExynosCameraFrameSelectorTag compareTag;
        compareTag.selectorId = m_selectorId;
        /* get first tag to match with this selector */
        if (selectedFrame->findSelectorTag(&compareTag) == true) {
            pipeID = compareTag.pipeId;
            isSrc = compareTag.isSrc;
            dstPos = compareTag.bufPos;
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
            if (m_bufferSupplier == NULL) {
                CLOGE("m_bufferSupplier is NULL");
                flashMgr->resetShotFcount();
                return NULL;
            } else {
                m_frameComplete(selectedFrame, false, true);
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

    flashMgr->resetShotFcount();
    return selectedFrame;
}

status_t ExynosCameraFrameSelector::m_waitAndpopQ(frame_queue_t *list, ExynosCameraFrameSP_dptr_t outframe, bool unlockflag, int tryCount)
{
    status_t ret = NO_ERROR;
    int iter = 0;

    do {
        ret = list->waitAndPopProcessQ(&outframe);

        if (m_isCanceled == true) {
            CLOGD("m_isCanceled");

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

status_t ExynosCameraFrameSelector::clearList(void)
{
    int ret = 0;
    ExynosCameraFrameSP_sptr_t frame = NULL;

    if (m_frameHoldList.isWaiting() == false) {
        ret = m_clearList(&m_frameHoldList);
        if( ret < 0 ) {
            CLOGE("m_frameHoldList clear failed");
        }
    } else {
        CLOGE("Cannot clear frameHoldList cause waiting for pop frame");
    }

    if (m_hdrFrameHoldList.isWaiting() == false) {
        ret = m_clearList(&m_hdrFrameHoldList);
        if( ret < 0 ) {
            CLOGE("m_hdrFrameHoldList clear failed");
        }
    } else {
        CLOGE("Cannot clear hdrFrameHoldList cause waiting for pop frame");
    }

    m_isCanceled = false;

    return NO_ERROR;
}

status_t ExynosCameraFrameSelector::m_releaseBuffer(ExynosCameraFrameSP_sptr_t frame)
{
    selector_tag_queue_t::iterator r;
    int ret = 0;
    ExynosCameraBuffer buffer;
    ExynosCameraBuffer bayerBuffer;
    int pipeID;
    bool isSrc;
    int32_t dstPos;
    selector_tag_queue_t *selectorTagList = frame->getSelectorTagList();

    if (m_bufferSupplier == NULL) {
        CLOGE("m_bufferSupplier is NULL");
        return INVALID_OPERATION;
    }

#ifdef SUPPORT_DEPTH_MAP
    if (frame->getRequest(PIPE_VC1) == true
        && frame->getStreamRequested(STREAM_TYPE_DEPTH) == false
        ) {
        int pipeId = PIPE_3AA;

        if (m_parameters->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) == HW_CONNECTION_MODE_M2M) {
            pipeId = PIPE_FLITE;
        }

        ExynosCameraBuffer depthMapBuffer;

        depthMapBuffer.index = -2;

        ret = frame->getDstBuffer(pipeId, &depthMapBuffer, CAPTURE_NODE_2);
        if (ret != NO_ERROR) {
            CLOGE("Failed to get DepthMap buffer");
        }

        ret = m_bufferSupplier->putBuffer(depthMapBuffer);
        if (ret != NO_ERROR) {
            CLOGE("[F%d B%d]Failed to putBuffer. ret %d",
                    frame->getFrameCount(), depthMapBuffer.index, ret);
        }
    }
#endif

    /* lock for selectorTag */
    frame->lockSelectorTagList();
    frame->setRawStateInSelector(FRAME_STATE_IN_SELECTOR_REMOVE);

    if (selectorTagList->empty())
        goto func_exit;

    r = selectorTagList->begin()++;
    do {
        /* only removed the tag matched with selectorId */
        if (r->selectorId == m_selectorId) {
            pipeID = r->pipeId;
            isSrc = r->isSrc;
            dstPos = r->bufPos;

#ifdef DEBUG_RAWDUMP
    /* TODO: Do not use dstPos to check processed bayer reprocessing mode */
    if (dstPos != CAPTURE_NODE_1 && frame->getRequest(PIPE_VC0) == true) {
        bayerBuffer.index = -2;
        ret = m_getBufferFromFrame(frame, pipeID, isSrc, &bayerBuffer, CAPTURE_NODE_1);
        if (ret != NO_ERROR) {
            CLOGE("[F%d B%d]Failed to getBufferFromFrame for RAWDUMP. pipeID %d ret %d",
                    frame->getFrameCount(), bayerBuffer.index, pipeID, ret);
            /* continue */
        } else {
            ret = m_bufferSupplier->putBuffer(bayerBuffer);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to putBuffer. ret %d",
                        frame->getFrameCount(), bayerBuffer.index, ret);
                /* continue */
            }
        }
    }
#endif

            ret = m_getBufferFromFrame(frame, pipeID, isSrc, &buffer, dstPos);
            if( ret != NO_ERROR ) {
                CLOGE("m_getBufferFromFrame fail pipeID(%d) BufferType(%s) bufferPtr(%p)",
                        pipeID, (isSrc)? "Src" : "Dst", &buffer);
            }

            ret = m_bufferSupplier->putBuffer(buffer);
            if (ret < 0) {
                CLOGE("putIndex is %d", buffer.index);
#if 0
                m_bufMgr->printBufferState();
                m_bufMgr->printBufferQState();
#endif
            }

            r = selectorTagList->erase(r);
        } else {
            r++;
        }
    } while (r != selectorTagList->end());

func_exit:
    /* unlock for selectorTag */
    frame->unlockSelectorTagList();

    return NO_ERROR;
}

}
