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
#define LOG_TAG "ExynosCameraFrameSelector"

#include "ExynosCameraFrameSelector.h"

#define FLASHED_LLS_COUNT 4

namespace android {

ExynosCameraFrameSelector::ExynosCameraFrameSelector(int cameraId,
                                                     ExynosCameraConfigurations *configurations,
                                                     ExynosCameraParameters *param,
                                                     ExynosCameraBufferSupplier *bufferSupplier,
                                                     ExynosCameraFrameManager *manager
#ifdef SAMSUNG_DNG_DIRTY_BAYER
                                                    , ExynosCameraBufferManager *DNGbufMgr
#endif
                                                    )
{
    m_frameMgr = manager;
    m_configurations = configurations;
    m_parameters = param;
    m_bufferSupplier = bufferSupplier;
    m_activityControl = m_parameters->getActivityControl();
    m_frameHoldList.setWaitTime(2000000000);
#ifdef OIS_CAPTURE
    m_OISFrameHoldList.setWaitTime(130000000);
#endif
#ifdef RAWDUMP_CAPTURE
    m_RawFrameHoldList.setWaitTime(2000000000);
#endif

    m_reprocessingCount = 0;
    m_frameHoldCount = 1;
    m_isFirstFrame = true;
    m_isCanceled = false;
    removeFlags = false;
#ifdef SAMSUNG_DNG
    m_DNGFrameCount = 0;
    m_preDNGFrameCount = 0;
    m_preDNGFrame = NULL;

#ifdef SAMSUNG_DNG_DIRTY_BAYER
    m_DNGbufMgr = DNGbufMgr;
#endif
#endif
    m_CaptureCount = 0;

    m_cameraId = cameraId;
    memset(m_name, 0x00, sizeof(m_name));
    m_state = STATE_BASE;
    m_selectorId = SELECTOR_ID_BASE;
}

ExynosCameraFrameSelector::~ExynosCameraFrameSelector()
{
    /* empty destructor */
}

status_t ExynosCameraFrameSelector::m_release(frame_queue_t *list)
{
    int ret = 0;
    ExynosCameraFrameSP_sptr_t frame = NULL;

    while (list->getSizeOfProcessQ() > 0) {
        ret = m_popQ(list, frame, true, 1);
        if (ret != NO_ERROR) {
            CLOGE("getBufferToManageQ fail");
#if 0
            m_bufMgr->printBufferState();
            m_bufMgr->printBufferQState();
#endif
        } else {
            m_frameComplete(frame, true);
        }
    }
    return ret;
}

status_t ExynosCameraFrameSelector::manageFrameHoldListForDynamicBayer(ExynosCameraFrameSP_sptr_t frame)
{
    int ret = 0;

    if (frame == NULL) {
        CLOGE(" frame is NULL");
        return BAD_VALUE;
    }

    m_pushQ(&m_frameHoldList, frame, true);
    CLOGI(" [F%d T%d] m_frameHoldList size(%d)",
            frame->getFrameCount(), frame->getFrameType(), m_frameHoldList.getSizeOfProcessQ());

    return ret;
}

status_t ExynosCameraFrameSelector::m_manageHdrFrameHoldList(ExynosCameraFrameSP_sptr_t frame,
                                                                int pipeID,
                                                                bool isSrc,
                                                                int32_t dstPos)
{
    int ret = 0;
    ExynosCameraBuffer buffer;
    ExynosCameraFrameSP_sptr_t newFrame  = NULL;
    ExynosCameraActivitySpecialCapture *m_sCaptureMgr = NULL;
    unsigned int hdrFcount = 0;
    unsigned int fliteFcount = 0;
    newFrame = frame;

    m_sCaptureMgr = m_activityControl->getSpecialCaptureMgr();
    hdrFcount = m_sCaptureMgr->getHdrDropFcount();
    hdrFcount += m_parameters->getHDRDelay();

    ret = m_getBufferFromFrame(newFrame, pipeID, isSrc, &buffer, dstPos);
    if( ret != NO_ERROR ) {
        CLOGE("m_getBufferFromFrame fail pipeID(%d) BufferType(%s)",
                pipeID, (isSrc)?"Src":"Dst");
    }

    if (m_parameters->getUsePureBayerReprocessing() == true) {
        camera2_shot_ext *shot_ext = NULL;
        shot_ext = (camera2_shot_ext *)(buffer.addr[buffer.getMetaPlaneIndex()]);
        if (shot_ext != NULL)
            fliteFcount = shot_ext->shot.dm.request.frameCount;
        else
            CLOGE("fliteReprocessingBuffer is null");
    } else {
        camera2_stream *shot_stream = NULL;
        shot_stream = (camera2_stream *)(buffer.addr[buffer.getMetaPlaneIndex()]);
        if (shot_stream != NULL)
            fliteFcount = shot_stream->fcount;
        else
            CLOGE("fliteReprocessingBuffer is null");
    }

    if (hdrFcount + 1 == fliteFcount || hdrFcount + 2 == fliteFcount || hdrFcount + 3 == fliteFcount) {
        CLOGI("hdrFcount %d, fliteFcount %d",  hdrFcount, fliteFcount);
        m_pushQ(&m_hdrFrameHoldList, newFrame, true);
    } else {
        m_frameComplete(newFrame, false, true);
        newFrame = NULL;
    }

    return ret;
}

ExynosCameraFrameSP_sptr_t ExynosCameraFrameSelector::m_selectHdrFrame(int tryCount)
{
    int ret = 0;
    ExynosCameraFrameSP_sptr_t selectedFrame = NULL;

    ret = m_waitAndpopQ(&m_hdrFrameHoldList, selectedFrame, false, tryCount);
    if( ret < 0 ||  selectedFrame == NULL ) {
        CLOGD("getFrame Fail ret(%d)",  ret);
        return NULL;
    }

    return selectedFrame;
}

/* It's for dynamic bayer */
ExynosCameraFrameSP_sptr_t ExynosCameraFrameSelector::selectDynamicFrames(__unused int count,
                                                                  int tryCount)
{
        return m_selectNormalFrame(tryCount);
}

 ExynosCameraFrameSP_sptr_t ExynosCameraFrameSelector::m_selectCaptureFrame(uint32_t frameCount, int tryCount)
{
    ExynosCameraFrameSP_sptr_t selectedFrame = NULL;

    for (int i = 0; i < CAPTURE_WAITING_COUNT; i++) {
        selectedFrame = m_selectNormalFrame(tryCount);
        if (selectedFrame == NULL) {
            CLOGE("selectedFrame is NULL");
            break;
        }

        if (selectedFrame->getFrameCount() < frameCount) {
            CLOGD("skip capture frame(count %d), waiting frame(count %d)",
                     selectedFrame->getFrameCount(), frameCount);

            if (m_bufferSupplier == NULL) {
                CLOGE("m_bufferSupplier is NULL");
                return NULL;
            } else {
                m_frameComplete(selectedFrame, false, true);
                selectedFrame = NULL;
            }
        } else {
            CLOGD("capture frame (count %d)", selectedFrame->getFrameCount());
            break;
        }
    }

    return selectedFrame;
}

status_t ExynosCameraFrameSelector::m_getBufferFromFrame(ExynosCameraFrameSP_sptr_t frame,
                                                            int pipeID,
                                                            bool isSrc,
                                                            ExynosCameraBuffer *outBuffer,
                                                            int32_t dstPos)
{
    status_t ret = NO_ERROR;
    ExynosCameraBuffer selectedBuffer;

    if (frame == NULL) {
        CLOGE("frame == NULL");
        return BAD_VALUE;
    }

    if (outBuffer == NULL) {
        CLOGE("outBuffer == NULL");
        return BAD_VALUE;
    }

    if (isSrc) {
        ret = frame->getSrcBuffer(pipeID, &selectedBuffer);
        if (ret != NO_ERROR)
            CLOGE("getSrcBuffer(pipeID %d) fail", pipeID);
    } else {
        if (dstPos < 0) {
            ret = frame->getDstBuffer(pipeID, &selectedBuffer);
            if (ret != NO_ERROR)
                CLOGE("getDstBuffer(pipeID %d) fail", pipeID);
        } else {
            ret = frame->getDstBuffer(pipeID, &selectedBuffer, dstPos);
            if (ret != NO_ERROR)
                CLOGE("getDstBuffer(pipeID %d, dstPos %d) fail", pipeID, dstPos);
        }
    }

    *outBuffer = selectedBuffer;
    return ret;
}

status_t ExynosCameraFrameSelector::m_pushQ(frame_queue_t *list,
                                            ExynosCameraFrameSP_sptr_t inframe,
                                            bool lockflag)
{
    status_t ret = NO_ERROR;
    if( lockflag ) {
        inframe->frameLock();
    }
    list->pushProcessQ(&inframe);
    return ret;
}
status_t ExynosCameraFrameSelector::m_popQ(frame_queue_t *list,
                                            ExynosCameraFrameSP_dptr_t outframe,
                                            bool unlockflag,
                                            int tryCount)
{
    status_t ret = NO_ERROR;
    int iter = 0;

    do {

        ret = list->popProcessQ(&outframe);
        if( ret < 0 ) {
            if( ret == TIMED_OUT ) {
                CLOGD("PopQ Time out -> retry[max cur](%d %d)",
                         tryCount, iter);

                iter++;
                continue;
            }
        }
    } while (ret != OK && tryCount > iter);

    if( ret != OK ) {
        CLOGE("popQ fail(%d)", ret);
        return ret;
    }

    if (outframe == NULL){
        CLOGE("popQ frame = NULL ");
        return ret;
    }

    if( unlockflag ) {
        outframe->frameUnlock();
    }
    return ret;
}

status_t ExynosCameraFrameSelector::m_frameComplete(ExynosCameraFrameSP_sptr_t frame, bool isForcelyDelete, bool flagReleaseBuf)
{
    int ret = OK;

    if (flagReleaseBuf) {
        m_releaseBuffer(frame);
    }

    if (isForcelyDelete == true) {
        CLOGD("frame deleted forcely : isComplete(%d) count(%d) LockState(%d)",
            frame->isComplete(),
            frame->getFrameCount(),
            frame->getFrameLockState());

        if (m_frameMgr != NULL) {
        } else {
            CLOGE("m_frameMgr is NULL (%d)", frame->getFrameCount());
        }
        frame = NULL;
    } else {
        CLOGV("frame complete, count(%d)", frame->getFrameCount());
        if (m_frameMgr != NULL) {
        } else {
            CLOGE("m_frameMgr is NULL (%d)", frame->getFrameCount());
        }
        frame = NULL;
    }

    return ret;
}

/*
 * Check complete flag of the Frame and deallocate it if it is completed.
 * This function ignores lock flag of the frame(Lock flag is usually set to protect
 * the frame from deallocation), so please use with caution.
 * This function is required to remove a frame from frameHoldingList.
 */
status_t ExynosCameraFrameSelector::m_LockedFrameComplete(ExynosCameraFrameSP_sptr_t frame)
{
    int ret = OK;

    m_releaseBuffer(frame);

    if (m_frameMgr != NULL) {
    } else {
        CLOGE("m_frameMgr is NULL (%d)",  frame->getFrameCount());
    }

    return ret;
}

uint32_t ExynosCameraFrameSelector::getSizeOfHoldFrame(void)
{
    uint32_t size = 0;

    size += m_frameHoldList.getSizeOfProcessQ();
    size += m_hdrFrameHoldList.getSizeOfProcessQ();
#ifdef OIS_CAPTURE
    size += m_OISFrameHoldList.getSizeOfProcessQ();
#endif

    return size;
}

status_t ExynosCameraFrameSelector::cancelPicture(bool flagCancel)
{
    m_isCanceled = flagCancel;

    return NO_ERROR;
}

status_t ExynosCameraFrameSelector::wakeupQ(void)
{
    m_frameHoldList.sendCmd(WAKE_UP);
    m_OISFrameHoldList.sendCmd(WAKE_UP);

    return NO_ERROR;
}

status_t ExynosCameraFrameSelector::m_clearList(frame_queue_t *list)
{
    int ret = 0;
    ExynosCameraFrameSP_sptr_t frame = NULL;
    ExynosCameraBuffer buffer;

    if (m_bufferSupplier == NULL) {
        CLOGE("m_bufferSupplier is NULL");
        return INVALID_OPERATION;
    }

    while (list->getSizeOfProcessQ() > 0) {
        if (m_popQ(list, frame, false, 1) != NO_ERROR) {
            CLOGE("getBufferToManageQ fail");
#if 0
            m_bufMgr->printBufferState();
            m_bufMgr->printBufferQState();
#endif
        } else {
            m_releaseBuffer(frame);
            /*
               Frames in m_frameHoldList and m_hdrFrameHoldList are locked when they are inserted
               on the list. So we need to use m_LockedFrameComplete() to remove those frames.
               Please beware that the frame might be deleted in elsewhere, epically on erroneous
               conditions. So if the program encounters memory fault here, please check the other
               frame deallocation(delete) routines.
               */

            /* Rather than blindly deleting frame in m_LockedFrameComplete(), we do following:
             * 1. Check whether frame is complete. Delete the frame if it is complete.
             * 2. If the frame is not complete, unlock it. mainThread will delete this frame.
             */

            //m_LockedFrameComplete(frame);

            if (m_frameMgr == NULL) {
                CLOGE("m_frameMgr is NULL (%d)",  frame->getFrameCount());
            }
        }
    }
    return ret;
}

status_t ExynosCameraFrameSelector::setFrameHoldCount(int32_t count)
{
    if (count < 0) {
        CLOGE("frame hold count cannot be negative value, current value(%d)",
                 count);
        return BAD_VALUE;
    }

    CLOGD("holdCount : %d", count);

    m_frameHoldCount = count;

    return NO_ERROR;
}

bool ExynosCameraFrameSelector::m_isFrameMetaTypeShotExt(void)
{
    bool isShotExt = true;

    if (m_parameters->isSccCapture() == true) {
        if (m_parameters->isReprocessing() == true)
            isShotExt = true;
        else
            isShotExt = false;
    } else {
        if (m_parameters->getUsePureBayerReprocessing() == false)
            isShotExt = false;
    }

    return isShotExt;
}

void ExynosCameraFrameSelector::setWaitTime(uint64_t waitTime)
{
    m_frameHoldList.setWaitTime(waitTime);
}

void ExynosCameraFrameSelector::setId(SELECTOR_ID_t selectorId)
{
    m_selectorId = selectorId;
}

ExynosCameraFrameSelector::SELECTOR_ID_t ExynosCameraFrameSelector::getId(void)
{
    return m_selectorId;
}
}
