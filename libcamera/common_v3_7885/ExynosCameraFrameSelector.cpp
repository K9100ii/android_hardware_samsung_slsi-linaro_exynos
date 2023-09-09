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
#define LOG_TAG "ExynosCameraFrameSelector"

#include "ExynosCameraFrameSelector.h"

#define FLASHED_LLS_COUNT 4

namespace android {

#ifdef USE_FRAMEMANAGER
ExynosCameraFrameSelector::ExynosCameraFrameSelector(int cameraId,
                                                     ExynosCameraParameters *param,
                                                     ExynosCameraBufferManager *bufMgr,
                                                     ExynosCameraFrameManager *manager
#ifdef SUPPORT_DEPTH_MAP
                                                    , depth_callback_queue_t *depthCallbackQ
                                                    , ExynosCameraBufferManager *depthMapbufMgr
#endif
#if defined(SAMSUNG_DNG_DIRTY_BAYER) || defined(DEBUG_RAWDUMP_DIRTY_BAYER)
                                                    , ExynosCameraBufferManager *DNGbufMgr
#endif
                                                     )
#else
    ExynosCameraFrameSelector::ExynosCameraFrameSelector(int cameraId,
                                                         ExynosCameraParameters *param,
                                                         ExynosCameraBufferManager *bufMgr)
#endif
{
#ifdef USE_FRAMEMANAGER
    m_frameMgr = manager;
#endif
    m_parameters = param;
    m_bufMgr= bufMgr;
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
#ifdef USE_DUAL_CAMERA
    m_syncFrameHoldCount = 0;
#endif
    m_isFirstFrame = true;
    isCanceled = false;

#ifdef OIS_CAPTURE
    removeFlags = false;
#endif
#ifdef LLS_REPROCESSING
    LLSCaptureFrame = NULL;
    m_isLastFrame = true;
    m_isConvertingMeta = true;
    m_LLSCaptureCount = 0;
#endif
#ifdef SAMSUNG_LBP
    m_lbpFrameCounter = 0;
    m_LBPFrameNum = 0;
#endif
#ifdef SAMSUNG_DNG
    m_DNGFrameCount = 0;
    m_preDNGFrameCount = 0;
    m_preDNGFrame = NULL;

#if defined(SAMSUNG_DNG_DIRTY_BAYER) || defined(DEBUG_RAWDUMP_DIRTY_BAYER)
    m_DNGbufMgr = DNGbufMgr;
#endif
#endif
    m_CaptureCount = 0;
#ifdef SUPPORT_DEPTH_MAP
    m_depthCallbackQ = depthCallbackQ;
    m_depthMapbufMgr = depthMapbufMgr;
#endif

    m_cameraId = cameraId;
    memset(m_name, 0x00, sizeof(m_name));
    m_state = STATE_BASE;
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
            m_bufMgr->printBufferState();
            m_bufMgr->printBufferQState();
        } else {
            m_frameComplete(frame, true);
        }
    }
    return ret;
}

#ifdef USE_DUAL_CAMERA
status_t ExynosCameraFrameSelector::manageSyncFrameHoldList(ExynosCameraFrameSP_sptr_t frame, int pipeID, bool isSrc, int32_t dstPos)
{
    return manageFrameHoldList(frame, pipeID, isSrc, dstPos);
}
#endif

status_t ExynosCameraFrameSelector::manageFrameHoldListForDynamicBayer(ExynosCameraFrameSP_sptr_t frame)
{
    int ret = 0;

    if (frame == NULL) {
        CLOGE(" frame is NULL");
        return BAD_VALUE;
    }

    m_pushQ(&m_frameHoldList, frame, true);
    CLOGI(" frameCount(%d) m_frameHoldList size(%d)",
            frame->getFrameCount(), m_frameHoldList.getSizeOfProcessQ());

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
        m_frameComplete(newFrame, false, pipeID, isSrc, dstPos, true);
        newFrame = NULL;
    }

    return ret;
}

/* It's for dynamic bayer */
ExynosCameraFrameSP_sptr_t ExynosCameraFrameSelector::selectDynamicFrames(__unused int count,
                                                                  int pipeID,
                                                                  bool isSrc,
                                                                  int tryCount,
                                                                  int32_t dstPos)
{
        return m_selectNormalFrame(pipeID, isSrc, tryCount, dstPos);
}

ExynosCameraFrameSP_sptr_t ExynosCameraFrameSelector::selectCaptureFrames(int count,
                                                                  uint32_t frameCount,
                                                                  int pipeID,
                                                                  bool isSrc,
                                                                  int tryCount,
                                                                  int32_t dstPos)
{
    ExynosCameraFrameSP_sptr_t selectedFrame = NULL;
    ExynosCameraActivityFlash *m_flashMgr = NULL;

    m_reprocessingCount = count;
    m_flashMgr = m_activityControl->getFlashMgr();

    if (m_flashMgr->getNeedCaptureFlash() == true) {
        selectedFrame = m_selectFlashFrameV2(pipeID, isSrc, tryCount, dstPos);
        if (selectedFrame == NULL) {
            CLOGE("Failed to selectFlashFrame");
            selectedFrame = m_selectNormalFrame(pipeID, isSrc, tryCount, dstPos);
        }
    } else {
        selectedFrame = m_selectCaptureFrame(frameCount, pipeID, isSrc, tryCount, dstPos);
        if (selectedFrame == NULL) {
            CLOGE("Failed to selectCaptureFrame");
            selectedFrame = m_selectNormalFrame(pipeID, isSrc, tryCount, dstPos);
        }
    }

    return selectedFrame;
}

ExynosCameraFrameSP_sptr_t ExynosCameraFrameSelector::m_selectFocusedFrame(int pipeID, bool isSrc, int tryCount, int32_t dstPos)
{
    int ret = 0;
    ExynosCameraFrameSP_sptr_t selectedFrame = NULL;
    struct camera2_shot_ext shot_ext;
    memset(&shot_ext, 0x00, sizeof(struct camera2_shot_ext));

    for (int i = 0; i < CAPTURE_WAITING_COUNT; i++) {
        selectedFrame = m_selectNormalFrame(pipeID, isSrc, tryCount, dstPos);
        if (selectedFrame == NULL) {
            CLOGE("selectedFrame is NULL");
            break;
        }

        /* get dynamic metadata for af state */
        if (selectedFrame->getMetaDataEnable() == false)
            CLOGD("Waiting for update metadata failed (%d) ",  ret);
        selectedFrame->getDynamicMeta(&shot_ext);

        /* Skip focusing frame */
        if (m_activityControl->flagFocusing(&shot_ext, m_parameters->getFocusMode()) == true) {
            CLOGD("skip focusing frame(count %d)",
                    selectedFrame->getFrameCount());
            if (m_bufMgr == NULL) {
                CLOGE("m_bufMgr is NULL");
                return NULL;
            } else {
                m_frameComplete(selectedFrame, false, pipeID, isSrc, dstPos, true);
                selectedFrame = NULL;
            }
        } else {
            CLOGD("focusing complete (count %d)",
                    selectedFrame->getFrameCount());
            break;
        }

        usleep(DM_WAITING_TIME);
    }

    return selectedFrame;
}

ExynosCameraFrameSP_sptr_t ExynosCameraFrameSelector::m_selectFlashFrame(int pipeID, bool isSrc, int tryCount, int32_t dstPos)
{
    int ret = 0;
    ExynosCameraFrameSP_sptr_t selectedFrame = NULL;
    ExynosCameraBuffer selectedBuffer;
    int bufferFcount = 0;
    int waitFcount = 0;
    int totalWaitingCount = 0;

    /* Choose bayerBuffer to process reprocessing */
    while (totalWaitingCount <= (FLASH_MAIN_TIMEOUT_COUNT + m_parameters->getReprocessingBayerHoldCount())) {
        /* Start main flash & Get best frame count for flash */
        if (waitFcount == 0) {
            waitFcount = m_activityControl->startMainFlash() + 1;
            CLOGD("best frame count for flash capture : %d", waitFcount);
        }

        ret = m_waitAndpopQ(&m_frameHoldList, selectedFrame, false, tryCount);
        if (ret < 0 ||  selectedFrame == NULL) {
            CLOGD("getFrame Fail ret(%d)",  ret);
            return NULL;
        } else if (isCanceled == true) {
            CLOGD("isCanceled");
            if (selectedFrame != NULL) {
                m_LockedFrameComplete(selectedFrame, pipeID, isSrc, dstPos);
            }
            return NULL;
        }

        CLOGD("Frame Count(%d)",  selectedFrame->getFrameCount());

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

        if (waitFcount <= bufferFcount) {
            break;
        }

        totalWaitingCount++;
        CLOGD("(totalWaitingCount %d)",  totalWaitingCount);
    }

    if (totalWaitingCount > FLASH_MAIN_TIMEOUT_COUNT) {
        CLOGW("fail to get bayer frame count for flash capture (totalWaitingCount %d)",
                 totalWaitingCount);
    }

    CLOGD("waitFcount : %d, bufferFcount : %d",
             waitFcount, bufferFcount);

    /* Stop main flash */
    m_activityControl->stopMainFlash();

    return selectedFrame;

}

ExynosCameraFrameSP_sptr_t ExynosCameraFrameSelector::m_selectHdrFrame(__unused int pipeID, __unused bool isSrc, int tryCount, __unused int32_t dstPos)
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

ExynosCameraFrameSP_sptr_t ExynosCameraFrameSelector::m_selectBurstFrame(int pipeID, bool isSrc, int tryCount, int32_t dstPos)
{
    ExynosCameraFrameSP_sptr_t selectedFrame = NULL;

    ExynosCameraActivityFlash *m_flashMgr = m_activityControl->getFlashMgr();

    for (int i = 0; i < TOTAL_WAITING_TIME; i += DM_WAITING_TIME) {
        if (m_isFirstFrame == true) {
            selectedFrame = m_selectFocusedFrame(pipeID, isSrc, tryCount, dstPos);
            if (selectedFrame == NULL) {
                CLOGE("selectedFrame is NULL");
                selectedFrame = m_selectNormalFrame(pipeID, isSrc, tryCount, dstPos);
            }
        } else {
            selectedFrame = m_selectNormalFrame(pipeID, isSrc, tryCount, dstPos);
        }

        /* Skip flash frame */
        if (m_flashMgr->checkPreFlash() == true) {
            if (m_flashMgr->checkFlashOff() == false) {
                CLOGD("skip flash frame(count %d)",
                         selectedFrame->getFrameCount());

                if (m_bufMgr == NULL) {
                    CLOGE("m_bufMgr is NULL");
                    return NULL;
                } else {
                    m_frameComplete(selectedFrame, false, pipeID, isSrc, dstPos, true);
                    selectedFrame = NULL;
                }
            } else {
                CLOGD("flash off done (count %d)",
                         selectedFrame->getFrameCount());
                break;
            }
        } else {
            CLOGD("pre-flash off");
            break;
        }
        usleep(DM_WAITING_TIME);
    }

    return selectedFrame;
}

ExynosCameraFrameSP_sptr_t ExynosCameraFrameSelector::m_selectCaptureFrame(uint32_t frameCount, int pipeID, bool isSrc, int tryCount, int32_t dstPos)
{
    ExynosCameraFrameSP_sptr_t selectedFrame = NULL;

    for (int i = 0; i < CAPTURE_WAITING_COUNT; i++) {
        selectedFrame = m_selectNormalFrame(pipeID, isSrc, tryCount, dstPos);
        if (selectedFrame == NULL) {
            CLOGE("selectedFrame is NULL");
            break;
        }

        if (selectedFrame->getFrameCount() < frameCount) {
            CLOGD("skip capture frame(count %d), waiting frame(count %d)",
                     selectedFrame->getFrameCount(), frameCount);

            if (m_bufMgr == NULL) {
                CLOGE("m_bufMgr is NULL");
                return NULL;
            } else {
                m_frameComplete(selectedFrame, false, pipeID, isSrc, dstPos, true);
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

#ifdef USE_FRAMEMANAGER
status_t ExynosCameraFrameSelector::m_frameComplete(ExynosCameraFrameSP_sptr_t frame, bool isForcelyDelete,
                                                        int pipeID, bool isSrc, int32_t dstPos, bool flagReleaseBuf)
{
    int ret = OK;

    if(flagReleaseBuf) {
        m_releaseBuffer(frame, pipeID, isSrc, dstPos);
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
#ifndef USE_FRAME_REFERENCE_COUNT
        if (frame->isComplete() == true) {
            if (frame->getFrameLockState() == false) {
                CLOGV("frame complete, count(%d)", frame->getFrameCount());

                if (m_frameMgr != NULL) {
                } else {
                    CLOGE("m_frameMgr is NULL (%d)", frame->getFrameCount());
                }
                frame = NULL;
            } else {
                CLOGV("frame is locked : isComplete(%d) count(%d) LockState(%d)",
                    frame->isComplete(),
                    frame->getFrameCount(),
                    frame->getFrameLockState());
            }
        } else {
            if (m_frameMgr != NULL) {
            } else {
                CLOGE("m_frameMgr is NULL (%d)", frame->getFrameCount());
            }
            frame = NULL;
        }
#else /* USE_FRAME_REFERENCE_COUNT */
        if (m_frameMgr != NULL) {
        } else {
            CLOGE("m_frameMgr is NULL (%d)", frame->getFrameCount());
        }
#endif
        frame = NULL;
    }

    return ret;
}
#else /* USE_FRAMEMANAGER */
status_t ExynosCameraFrameSelector::m_frameComplete(ExynosCameraFrameSP_sptr_t frame, bool isForcelyDelete,                                                  int pipeID,
                                                        int pipeID, bool isSrc, int32_t dstPos, bool flagReleaseBuf)
{
    int ret = OK;

    if(flagReleaseBuf) {
        m_releaseBuffer(frame, pipeID, isSrc, dstPos);
    }

    if (isForcelyDelete == true) {
        CLOGD("frame deleted forcely : isComplete(%d) count(%d) LockState(%d)",
            frame->isComplete(),
            frame->getFrameCount(),
            frame->getFrameLockState());
        delete frame;
        frame = NULL;
    } else {
        if (frame->isComplete() == true) {
            if (frame->getFrameLockState() == false) {
                CLOGV("frame complete, count(%d)", frame->getFrameCount());
                delete frame;
                frame = NULL;
            } else {
                CLOGV("frame is locked : isComplete(%d) count(%d) LockState(%d)",
                    frame->isComplete(),
                    frame->getFrameCount(),
                    frame->getFrameLockState());
            }
        } else {
            CLOGV("frame is not completed : isComplete(%d) count(%d) LockState(%d)",
                frame->isComplete(),
                frame->getFrameCount(),
                frame->getFrameLockState());
        }
    }
    return ret;
}
#endif

/*
 * Check complete flag of the Frame and deallocate it if it is completed.
 * This function ignores lock flag of the frame(Lock flag is usually set to protect
 * the frame from deallocation), so please use with caution.
 * This function is required to remove a frame from frameHoldingList.
 */
#ifdef USE_FRAMEMANAGER
status_t ExynosCameraFrameSelector::m_LockedFrameComplete(ExynosCameraFrameSP_sptr_t frame, int pipeID,
                                                                bool isSrc, int32_t dstPos)
{
    int ret = OK;

    m_releaseBuffer(frame, pipeID, isSrc, dstPos);

#ifndef USE_FRAME_REFERENCE_COUNT
    if (frame->isComplete() == true) {
        if (frame->getFrameLockState() == true)
        {
            CLOGV("Deallocating locked frame, count(%d)",
                    frame->getFrameCount());
        }

        if (m_frameMgr != NULL) {
        }
        frame = NULL;
    }
#else
	if (m_frameMgr != NULL) {
	} else {
		CLOGE("m_frameMgr is NULL (%d)",  frame->getFrameCount());
	}
#endif

    return ret;
}
#else
status_t ExynosCameraFrameSelector::m_LockedFrameComplete(ExynosCameraFrameSP_sptr_t frame, int pipeID,
                                                                bool isSrc, int32_t dstPos)
{
    int ret = OK;

    m_releaseBuffer(frame, pipeID, isSrc, dstPos);

    if (frame->isComplete() == true) {
        if (frame->getFrameLockState() == true)
        {
            CLOGV("Deallocating locked frame, count(%d)",
                    frame->getFrameCount());
            CLOGV("frame is locked : isComplete(%d) count(%d) LockState(%d)",
                    frame->isComplete(),
                    frame->getFrameCount(),
                    frame->getFrameLockState());
        }
        delete frame;
        frame = NULL;
    }
    return ret;
}
#endif

status_t ExynosCameraFrameSelector::wakeupQ(void)
{
    m_frameHoldList.sendCmd(WAKE_UP);

    return NO_ERROR;
}

status_t ExynosCameraFrameSelector::cancelPicture(bool flagCancel)
{
    isCanceled = flagCancel;

    return NO_ERROR;
}

status_t ExynosCameraFrameSelector::m_clearList(frame_queue_t *list,int pipeID, bool isSrc, int32_t dstPos)
{
    int ret = 0;
    ExynosCameraFrameSP_sptr_t frame = NULL;
    ExynosCameraBuffer buffer;

    while (list->getSizeOfProcessQ() > 0) {
        if (m_popQ(list, frame, false, 1) != NO_ERROR) {
            CLOGE("getBufferToManageQ fail");

            m_bufMgr->printBufferState();
            m_bufMgr->printBufferQState();
        } else {
            ret = m_getBufferFromFrame(frame, pipeID, isSrc, &buffer, dstPos);
            if( ret != NO_ERROR ) {
                CLOGE("m_getBufferFromFrame fail pipeID(%d) BufferType(%s)",
                         pipeID, (isSrc)?"Src":"Dst");
            }
            if (m_bufMgr == NULL) {
                CLOGE("m_bufMgr is NULL");
                return INVALID_OPERATION;
            } else {
                if (buffer.index >= 0)
                    ret = m_bufMgr->putBuffer(buffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
                if (ret < 0) {
                    CLOGE("putIndex is %d",  buffer.index);
                    m_bufMgr->printBufferState();
                    m_bufMgr->printBufferQState();
                }
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

#ifdef USE_FRAMEMANAGER
#ifndef USE_FRAME_REFERENCE_COUNT
                if (frame->isComplete() == true) {
                    if (frame->getFrameLockState() == true)
                        CLOGV("Deallocating locked frame, count(%d)",
                                 frame->getFrameCount());
#else
                {
#endif
                    if (m_frameMgr != NULL) {
                    } else {
                        CLOGE("m_frameMgr is NULL (%d)",  frame->getFrameCount());
                    }
                }
#else
                if (frame->isComplete() == true) {
                    delete frame;
                    frame = NULL;
                } else {
                    if (frame->getFrameLockState() == true)
                        frame->frameUnlock();
                }
#endif
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

#ifdef USE_DUAL_CAMERA
status_t ExynosCameraFrameSelector::setSyncFrameHoldCount(int32_t holdCountForSync)
{
    return NO_ERROR;
}
#endif

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

void ExynosCameraFrameSelector::setIsFirstFrame(bool isFirstFrame)
{
    m_isFirstFrame = isFirstFrame;
}

bool ExynosCameraFrameSelector::getIsFirstFrame()
{
    return m_isFirstFrame;
}

void ExynosCameraFrameSelector::wakeselectDynamicFrames(void)
{
    isCanceled = true;
    m_frameHoldList.wakeupAll();
}
}
