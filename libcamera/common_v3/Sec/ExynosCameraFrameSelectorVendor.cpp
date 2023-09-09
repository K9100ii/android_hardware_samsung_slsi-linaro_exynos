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
#ifdef OIS_CAPTURE
    ret = m_release(&m_OISFrameHoldList);
    if (ret != NO_ERROR) {
        CLOGE("m_OISFrameHoldList release failed ");
    }
#endif

    isCanceled = false;

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
#ifdef USE_DUAL_CAMERA_CAPTURE_FUSION
    if (m_parameters->getDualCameraMode() == true) {
        int masterCameraId = -2;
        int slaveCameraId = -2;

        getDualCameraId(&masterCameraId, &slaveCameraId);

        m_dualFrameSelector->flushSyncList(m_cameraId);
    }
#endif // USE_DUAL_CAMERA_CAPTURE_FUSION
#endif // BOARD_CAMERA_USES_DUAL_CAMERA

    return NO_ERROR;
}

#ifdef OIS_CAPTURE
void ExynosCameraFrameSelector::setWaitTimeOISCapture(uint64_t waitTime)
{
    m_OISFrameHoldList.setWaitTime(waitTime);
}
#endif

status_t ExynosCameraFrameSelector::manageFrameHoldList(ExynosCameraFrameSP_sptr_t frame, int pipeID, bool isSrc, int32_t dstPos)
{
    int ret = 0;

    if (m_parameters->getHdrMode() == true ||
            m_parameters->getShotMode() == SHOT_MODE_RICH_TONE) {
        ret = m_manageHdrFrameHoldList(frame, pipeID, isSrc, dstPos);
    }
#ifdef OIS_CAPTURE
    else if (m_parameters->getOISCaptureModeOn() == true) {
        if (removeFlags == false) {
            m_CaptureCount = 0;
            m_list_release(&m_frameHoldList, pipeID, isSrc, dstPos);
            removeFlags = true;
            CLOGI("m_frameHoldList delete(%d)", m_frameHoldList.getSizeOfProcessQ());
        }

        if (m_parameters->getCaptureExposureTime() != 0) {
            ret = m_manageLongExposureFrameHoldList(frame, pipeID, isSrc, dstPos);
        } else {
#ifdef SAMSUNG_LLS_DEBLUR
            if(m_parameters->getLDCaptureMode()) {
                ret = m_manageLDFrameHoldList(frame, pipeID, isSrc, dstPos);
            } else
#endif
            {
                ret = m_manageOISFrameHoldList(frame, pipeID, isSrc, dstPos);
            }
        }
    }
#endif
#ifdef RAWDUMP_CAPTURE
    else if (m_parameters->getRawCaptureModeOn() == true) {
        if (removeFlags == false) {
            m_list_release(&m_frameHoldList, pipeID, isSrc, dstPos);
            removeFlags = true;
            CLOGI("m_frameHoldList delete(%d)", m_frameHoldList.getSizeOfProcessQ());
        }
        ret = m_manageRawFrameHoldList(frame, pipeID, isSrc, dstPos);
    }
#endif
    else {
#ifdef OIS_CAPTURE
        if(removeFlags == true) {
            m_list_release(&m_OISFrameHoldList, pipeID, isSrc, dstPos);
#ifdef LLS_REPROCESSING
            LLSCaptureFrame = NULL;
#endif
            m_CaptureCount = 0;
            removeFlags = false;
            CLOGI("m_OISFrameHoldList delete(%d)", m_OISFrameHoldList.getSizeOfProcessQ());
        }
#endif
#ifdef RAWDUMP_CAPTURE
        if(removeFlags == true) {
            m_list_release(&m_RawFrameHoldList, pipeID, isSrc, dstPos);
            removeFlags = false;
            CLOGI("m_RawFrameHoldList delete(%d)", m_RawFrameHoldList.getSizeOfProcessQ());
        }
#endif
        ret = m_manageNormalFrameHoldList(frame, pipeID, isSrc, dstPos);
    }

    return ret;
}

status_t ExynosCameraFrameSelector::m_manageNormalFrameHoldList(ExynosCameraFrameSP_sptr_t newFrame, int pipeID, bool isSrc, int32_t dstPos)
{
    int ret = 0;
#ifdef SAMSUNG_LBP
    unsigned int bayer_fcount = 0;
#endif
    ExynosCameraFrameSP_sptr_t oldFrame = NULL;
    ExynosCameraBuffer buffer;

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
#ifdef USE_DUAL_CAMERA_CAPTURE_FUSION
    if (m_parameters->getDualCameraMode() == true) {
        ret = m_dualFrameSelector->manageNormalFrameHoldList(m_cameraId, newFrame, pipeID, isSrc, dstPos);

        /* if dual frame sync is fail, just use original code. */
        if (ret != NO_ERROR) {
            CLOGE("dualFrameSelector->manageFrameHoldList(id(%d), pipeID(%d), BufferType(%s), dstPos(%d))",
                    m_cameraId, pipeID, (isSrc)? "Src" : "Dst", dstPos);
        } else {
            return ret;
        }
    }
#endif // USE_DUAL_CAMERA_CAPTURE_FUSION
#endif // BOARD_CAMERA_USES_DUAL_CAMERA

    /* Skip INITIAL_SKIP_FRAME only FastenAeStable is disabled */
    /* This previous condition check is useless because almost framecount for capture is over than skip frame count */
    /*
     * if (m_parameters->getUseFastenAeStable() == true ||
     *     newFrame->getFrameCount() > INITIAL_SKIP_FRAME) {
     */
    m_pushQ(&m_frameHoldList, newFrame, true);
#ifdef SAMSUNG_LBP
    /* Set the Frame Count of BayerFrame. */
    ret = m_getBufferFromFrame(newFrame, pipeID, isSrc, &buffer, dstPos);
    if( ret != NO_ERROR ) {
        CLOGE("m_getBufferFromFrame fail pipeID(%d) BufferType(%s) bufferPtr(%p)",
             pipeID, (isSrc)? "Src" : "Dst", &buffer);
    }

    if (m_parameters->getUsePureBayerReprocessing() == true) {
        camera2_shot_ext *shot_ext = NULL;
        shot_ext = (camera2_shot_ext *)(buffer.addr[buffer.getMetaPlaneIndex()]);
        if (shot_ext != NULL)
            bayer_fcount = shot_ext->shot.dm.request.frameCount;
        else
            CLOGE("Buffer is null");
    } else {
        camera2_stream *shot_stream = NULL;
        shot_stream = (camera2_stream *)(buffer.addr[buffer.getMetaPlaneIndex()]);
        if (shot_stream != NULL)
            bayer_fcount = shot_stream->fcount;
        else
            CLOGE("Buffer is null");
    }

    m_parameters->setBayerFrameCount(bayer_fcount);
#endif

    if (m_frameHoldList.getSizeOfProcessQ() > m_frameHoldCount) {
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

#if defined(OIS_CAPTURE) || defined(RAWDUMP_CAPTURE)
status_t ExynosCameraFrameSelector::m_list_release(frame_queue_t *list, int pipeID, bool isSrc, int32_t dstPos)
{
    int ret = 0;
    ExynosCameraFrameSP_sptr_t frame  = NULL;

    while (list->getSizeOfProcessQ() > 0) {
        if (m_popQ(list, frame, true, 1) != NO_ERROR) {
            CLOGE("getBufferToManageQ fail");
            m_bufMgr->printBufferState();
            m_bufMgr->printBufferQState();
        } else {
            m_LockedFrameComplete(frame, pipeID, isSrc, dstPos);
            frame = NULL;
        }
    }

    return NO_ERROR;
}
#endif

#ifdef OIS_CAPTURE
status_t ExynosCameraFrameSelector::m_manageOISFrameHoldList(ExynosCameraFrameSP_sptr_t frame, int pipeID, bool isSrc, int32_t dstPos)
{
    int ret = 0;
    ExynosCameraBuffer buffer;
    ExynosCameraFrameSP_sptr_t oldFrame = NULL;
    ExynosCameraFrameSP_sptr_t newFrame  = NULL;

    ExynosCameraActivitySpecialCapture *m_sCaptureMgr = NULL;
    unsigned int OISFcount = 0;
    unsigned int fliteFcount = 0;
    newFrame = frame;
    bool OISCapture_activated = false;

    m_sCaptureMgr = m_activityControl->getSpecialCaptureMgr();
    OISFcount = m_sCaptureMgr->getOISCaptureFcount();
    if(OISFcount > 0) {
        OISCapture_activated = true;
    }
#ifdef SAMSUNG_LBP
    m_lbpFrameCounter = OISFcount;
#endif

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

    if ((OISCapture_activated) &&
        ((m_parameters->getSeriesShotCount() == 0 && OISFcount == fliteFcount)
        || (m_parameters->getSeriesShotCount() > 0 && OISFcount <= fliteFcount)
#ifdef SAMSUNG_LBP
        || (m_parameters->getUseBestPic() && OISFcount <= fliteFcount)
#endif
    )) {
        CLOGI("zsl-like Fcount %d, fliteFcount %d, halcount(%d)",
                OISFcount, fliteFcount, newFrame->getFrameCount());

#ifdef LLS_REPROCESSING
        if (m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_LLS && LLSCaptureFrame != NULL) {
            CLOGI("list size(%d), halcount(%d) LLS halcount(%d)",
                    m_OISFrameHoldList.getSizeOfProcessQ(),
                    newFrame->getFrameCount(), LLSCaptureFrame->getFrameCount());

            if (m_bufMgr == NULL) {
                CLOGE("m_bufMgr is NULL");
                return INVALID_OPERATION;
            } else {
                m_frameComplete(newFrame, false, pipeID, isSrc, dstPos, true);
                newFrame = NULL;
            }
            return ret;
        } else
#endif
        {
#ifdef LLS_REPROCESSING
            if (m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_LLS && LLSCaptureFrame == NULL) {
                LLSCaptureFrame = newFrame;
                setIsLastFrame(false);
                ret = m_getBufferFromFrame(newFrame, pipeID, isSrc, &buffer, dstPos);
                if( ret != NO_ERROR ) {
                    CLOGE("m_getBufferFromFrame fail pipeID(%d) BufferType(%s)",
                            pipeID, (isSrc)? "Src" : "Dst");
                }

                for(int j = 0; j < m_parameters->getSeriesShotCount(); j++) {
                    m_pushQ(&m_OISFrameHoldList, LLSCaptureFrame, true);
                    CLOGI("list size(%d) index(%d) lls frame(%d)",
                            m_OISFrameHoldList.getSizeOfProcessQ(), LLSCaptureFrame->getFrameCount(), j);
                }
            } else
#endif
            {
                m_pushQ(&m_OISFrameHoldList, newFrame, true);
            }
        }

        if (m_OISFrameHoldList.getSizeOfProcessQ() > m_frameHoldCount
#ifdef LLS_REPROCESSING
            && m_parameters->getSeriesShotMode() != SERIES_SHOT_MODE_LLS
#endif
            ) {
            if (m_popQ(&m_OISFrameHoldList, oldFrame, true, 1) != NO_ERROR) {
                CLOGE("getBufferToManageQ fail");

                m_bufMgr->printBufferState();
                m_bufMgr->printBufferQState();
            } else {
                m_LockedFrameComplete(oldFrame, pipeID, isSrc, dstPos);
                oldFrame = NULL;
            }
        }
    } else if (OISCapture_activated && OISFcount + 2 < fliteFcount) {
        ret = m_manageNormalFrameHoldList(frame, pipeID, isSrc, dstPos);
        if (m_parameters->getSeriesShotCount() == 0) {
            CLOGI("Fcount %d, fliteFcount (%d)halcount(%d)",
                    OISFcount, fliteFcount, newFrame->getFrameCount());
            m_parameters->setOISCaptureModeOn(false);
        }
    } else {
        CLOGI("Fcount %d, fliteFcount (%d) halcount(%d) delete",
                OISFcount, fliteFcount, newFrame->getFrameCount());
        m_LockedFrameComplete(newFrame, pipeID, isSrc, dstPos);
        newFrame = NULL;
    }

    return ret;
}

status_t ExynosCameraFrameSelector::m_manageLongExposureFrameHoldList(ExynosCameraFrameSP_sptr_t frame, int pipeID, bool isSrc, int32_t dstPos)
{
    int ret = 0;
    ExynosCameraBuffer buffer;
    ExynosCameraFrameSP_sptr_t oldFrame = NULL;
    ExynosCameraFrameSP_sptr_t newFrame  = NULL;

    ExynosCameraActivitySpecialCapture *m_sCaptureMgr = NULL;
    unsigned int OISFcount = 0;
    unsigned int fliteFcount = 0;
    newFrame = frame;
#ifdef OIS_CAPTURE
    bool OISCapture_activated = false;
#endif
    int captureCount = m_parameters->getLongExposureShotCount() + 1;

    m_sCaptureMgr = m_activityControl->getSpecialCaptureMgr();
#ifdef OIS_CAPTURE
    OISFcount = m_sCaptureMgr->getOISCaptureFcount();
    if(OISFcount > 0) {
        OISCapture_activated = true;
    }
#endif

    ret = m_getBufferFromFrame(newFrame, pipeID, isSrc, &buffer, dstPos);
    if( ret != NO_ERROR ) {
        CLOGE("m_getBufferFromFrame fail pipeID(%d) BufferType(%s) bufferPtr(%p)",
                pipeID, (isSrc)? "Src" : "Dst", &buffer);
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

#ifdef OIS_CAPTURE
    if ((OISCapture_activated) && (OISFcount <= fliteFcount) &&
            (m_CaptureCount < captureCount)) {
        CLOGI(" Capture Fcount %d, fliteFcount %d, halcount(%d) getSizeOfProcessQ(%d)",
                OISFcount, fliteFcount, newFrame->getFrameCount(),
                m_OISFrameHoldList.getSizeOfProcessQ());
        m_pushQ(&m_OISFrameHoldList, newFrame, true);
        m_CaptureCount++;
    } else
#endif
    {
        CLOGI("Fcount %d, fliteFcount (%d) halcount(%d) delete",
                OISFcount, fliteFcount, newFrame->getFrameCount());
        m_LockedFrameComplete(newFrame, pipeID, isSrc, dstPos);
        newFrame = NULL;
    }

    return ret;
}
#endif

#ifdef SAMSUNG_LLS_DEBLUR
status_t ExynosCameraFrameSelector::m_manageLDFrameHoldList(ExynosCameraFrameSP_sptr_t frame, int pipeID, bool isSrc, int32_t dstPos)
{
    int ret = 0;
    ExynosCameraBuffer buffer;
    ExynosCameraFrameSP_sptr_t oldFrame = NULL;
    ExynosCameraFrameSP_sptr_t newFrame  = NULL;
    ExynosCameraActivitySpecialCapture *m_sCaptureMgr = NULL;
    unsigned int OISFcount = 0;
    unsigned int fliteFcount = 0;
    newFrame = frame;
#ifdef OIS_CAPTURE
    bool OISCapture_activated = false;
#endif
    int captureCount = 0;

    m_sCaptureMgr = m_activityControl->getSpecialCaptureMgr();
#ifdef OIS_CAPTURE
    OISFcount = m_sCaptureMgr->getOISCaptureFcount();
    if(OISFcount > 0) {
        OISCapture_activated = true;
    }
#endif

    ret = m_getBufferFromFrame(newFrame, pipeID, isSrc, &buffer, dstPos);
    if( ret != NO_ERROR ) {
        CLOGE("m_getBufferFromFrame fail pipeID(%d) BufferType(%s)",
                pipeID, (isSrc)? "Src" : "Dst");
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

#ifdef FLASHED_LLS_CAPTURE
    if (m_parameters->getFlashedLLSCapture()) {
        captureCount = m_parameters->getLDCaptureCount() - 1;
    } else
#endif
    {
        captureCount = m_parameters->getLDCaptureCount();
    }

#ifdef OIS_CAPTURE
    if ((OISCapture_activated) && (OISFcount <= fliteFcount) &&
            (m_CaptureCount < captureCount)) {
        CLOGI(" Capture Fcount %d, fliteFcount %d, halcount(%d) getSizeOfProcessQ(%d)",
                OISFcount, fliteFcount, newFrame->getFrameCount(),
                m_OISFrameHoldList.getSizeOfProcessQ());
        m_pushQ(&m_OISFrameHoldList, newFrame, true);
        m_CaptureCount++;
    } else
#endif
    {
        CLOGI("Fcount %d, fliteFcount (%d) halcount(%d) delete",
                 OISFcount, fliteFcount, newFrame->getFrameCount());
        m_LockedFrameComplete(newFrame, pipeID, isSrc, dstPos);
        newFrame = NULL;
    }

    return ret;
}
#endif

#ifdef RAWDUMP_CAPTURE
status_t ExynosCameraFrameSelector::m_manageRawFrameHoldList(ExynosCameraFrameSP_sptr_t frame, int pipeID, bool isSrc, int32_t dstPos)
{
    int ret = 0;
    ExynosCameraBuffer buffer;
    ExynosCameraFrameSP_sptr_t newFrame  = NULL;

    ExynosCameraActivitySpecialCapture *m_sCaptureMgr = NULL;
    unsigned int RawFcount = 0;
    unsigned int fliteFcount = 0;
    newFrame = frame;

    m_sCaptureMgr = m_activityControl->getSpecialCaptureMgr();
    RawFcount = m_sCaptureMgr->getRawCaptureFcount();

    ret = m_getBufferFromFrame(newFrame, pipeID, isSrc, &buffer, dstPos);
    if( ret != NO_ERROR ) {
        CLOGE("m_getBufferFromFrame fail pipeID(%d) BufferType(%s)",
                pipeID, (isSrc)? "Src" : "Dst");
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

    if (RawFcount != 0 && ((RawFcount + 1 == fliteFcount) || (RawFcount + 2 == fliteFcount))) {
        CLOGI("Raw %s Capture Fcount %d, fliteFcount %d, halcount(%d)",
                (RawFcount + 1 == fliteFcount) ? "first" : "sencond",
                RawFcount , fliteFcount, newFrame->getFrameCount());
        m_pushQ(&m_RawFrameHoldList, newFrame, true);
    } else {
        CLOGI("Fcount %d, fliteFcount (%d) halcount(%d) delete",
                RawFcount, fliteFcount, newFrame->getFrameCount());
        m_LockedFrameComplete(newFrame, pipeID, isSrc, dstPos);
        newFrame = NULL;
    }

    return ret;
}
#endif

ExynosCameraFrameSP_sptr_t ExynosCameraFrameSelector::selectFrames(int count, int pipeID, bool isSrc, int tryCount, int32_t dstPos)
{
    ExynosCameraFrameSP_sptr_t selectedFrame = NULL;

    ExynosCameraActivityFlash *m_flashMgr = NULL;
    ExynosCameraActivityAutofocus *afMgr = m_activityControl->getAutoFocusMgr();     // shoud not be a NULL
    ExynosCameraActivitySpecialCapture *m_sCaptureMgr = m_activityControl->getSpecialCaptureMgr();

#if defined(RAWDUMP_CAPTURE) && defined(SAMSUNG_COMPANION)
    selectedFrame = m_selectRawNormalFrame(pipeID, isSrc, tryCount);
    if (selectedFrame == NULL) {
        CLOGE("select focused frame Faile!");
        selectedFrame = m_selectNormalFrame(pipeID, isSrc, tryCount, dstPos);
    }
    return selectedFrame;
#endif

#ifdef LLS_STUNR
    if (m_parameters->getLLSStunrMode()) {
        selectedFrame = m_selectStunrNormalFrame(pipeID, isSrc, tryCount);
        if (selectedFrame == NULL)
            CLOGE2("STUNR : select frame for sturn running fail!");
        return selectedFrame;
    }
#endif

    m_reprocessingCount = count;

    m_flashMgr = m_activityControl->getFlashMgr();
#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
#ifdef USE_DUAL_CAMERA_CAPTURE_FUSION
    if (m_parameters->getDualCameraMode() == true) {
        selectedFrame = m_dualFrameSelector->selectSingleFrame(m_cameraId);
    } else
#endif // USE_DUAL_CAMERA_CAPTURE_FUSION
#endif // BOARD_CAMERA_USES_DUAL_CAMERA

    if ((m_flashMgr->getNeedCaptureFlash() == true && m_parameters->getSeriesShotCount() == 0
#ifdef FLASHED_LLS_CAPTURE
        && m_parameters->getLDCaptureMode() == MULTI_SHOT_MODE_NONE
#endif
        )
#ifdef FLASHED_LLS_CAPTURE
        || (m_parameters->getLDCaptureMode() == MULTI_SHOT_MODE_FLASHED_LLS && m_reprocessingCount == 1)
#endif
    ) {
#if defined(OIS_CAPTURE) && defined(FLASHED_LLS_CAPTURE)
        if (m_parameters->getLDCaptureMode() == MULTI_SHOT_MODE_FLASHED_LLS && m_reprocessingCount == 1) {
#ifdef LLS_REPROCESSING
            setIsConvertingMeta(true);
#endif
            m_parameters->setOISCaptureModeOn(false);
        }
#endif

#ifdef SAMSUNG_FRONT_LCD_FLASH
        if (m_parameters->getCameraId() == CAMERA_ID_FRONT) {
            selectedFrame = m_selectFlashFrameV2(pipeID, isSrc, tryCount, dstPos);
        } else
#endif
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

#ifdef OIS_CAPTURE
        if(m_parameters->getOISCaptureModeOn() == true) {
            selectedFrame = m_selectOISNormalFrame(pipeID, isSrc, tryCount, dstPos);
            if (selectedFrame == NULL && !isCanceled) {
                CLOGE("select OISCapture frame Fail!");
#ifdef SAMSUNG_LBP
                ExynosCameraActivitySpecialCapture *m_sCaptureMgr = m_activityControl->getSpecialCaptureMgr();
                m_sCaptureMgr->setBestMultiCaptureMode(false);
                m_sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_WAIT_CAPTURE);
                setFrameHoldCount(REPROCESSING_BAYER_HOLD_COUNT);
#endif
                selectedFrame = m_selectNormalFrame(pipeID, isSrc, tryCount, dstPos);
            }
        } else {
            selectedFrame = m_selectNormalFrame(pipeID, isSrc, tryCount, dstPos);
        }
#else
        selectedFrame = m_selectNormalFrame(pipeID, isSrc, tryCount, dstPos);
#endif
        if (selectedFrame == NULL)
            CLOGE("select Frame Fail!");
    } else if (m_parameters->getSeriesShotCount() > 0) {
#ifdef OIS_CAPTURE
        if (m_parameters->getOISCaptureModeOn() == true) {
            selectedFrame = m_selectOISBurstFrame(pipeID, isSrc, tryCount, dstPos);
#ifdef LLS_REPROCESSING
            if (m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_LLS) {
                m_LLSCaptureCount++;
                m_parameters->setLLSCaptureCount(m_LLSCaptureCount);
                if (m_parameters->getSeriesShotCount() == m_LLSCaptureCount) {
                    CLOGD(" last LLS capture m_LLSCaptureCount(%d)", m_LLSCaptureCount);
                    setIsLastFrame(true);
                    if (m_LLSCaptureCount == 2) {
                        setIsConvertingMeta(false);
                    }
                } else if (m_LLSCaptureCount == 2) {
                    setIsConvertingMeta(false);
                }
            }
#endif
        } else {
            selectedFrame = m_selectBurstFrame(pipeID, isSrc, tryCount, dstPos);
        }
#else
        selectedFrame = m_selectBurstFrame(pipeID, isSrc, tryCount, dstPos);
#endif
        if (selectedFrame == NULL && !isCanceled) {
            CLOGE("select focused frame Faile!");
            selectedFrame = m_selectNormalFrame(pipeID, isSrc, tryCount, dstPos);
        }
#ifdef CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT
    } else if (m_parameters->getCaptureExposureTime() > CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT) {
#ifdef OIS_CAPTURE
        if(m_parameters->getOISCaptureModeOn() == true) {
            selectedFrame = m_selectOISNormalFrame(pipeID, isSrc, tryCount, dstPos);
        } else
#endif
        {
            selectedFrame = m_selectNormalFrame(pipeID, isSrc, tryCount, dstPos);
        }
#endif
    } else {
#ifdef OIS_CAPTURE
        if(m_parameters->getOISCaptureModeOn() == true) {
            selectedFrame = m_selectOISNormalFrame(pipeID, isSrc, tryCount, dstPos);
        } else {
            selectedFrame = m_selectFocusedFrame(pipeID, isSrc, tryCount, dstPos);
        }
#else
        selectedFrame = m_selectFocusedFrame(pipeID, isSrc, tryCount, dstPos);
#endif

        if (selectedFrame == NULL && !isCanceled) {
            CLOGE("select focused frame Faile!");
#ifdef SAMSUNG_LBP
            ExynosCameraActivitySpecialCapture *m_sCaptureMgr = m_activityControl->getSpecialCaptureMgr();
            m_sCaptureMgr->setBestMultiCaptureMode(false);
            m_sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_WAIT_CAPTURE);
            setFrameHoldCount(REPROCESSING_BAYER_HOLD_COUNT);
#endif
            selectedFrame = m_selectNormalFrame(pipeID, isSrc, tryCount, dstPos);
        }
    }

    if (selectedFrame == NULL) {
        CLOGE("selectedFrame is NULL");
    } else {
        CLOGD("selectedFrame is [%d]:frameCount(%d), timeStamp(%d)",
                m_parameters->getCameraId(), selectedFrame->getFrameCount(), (int)ns2ms(selectedFrame->getTimeStamp()));
    }

    m_isFirstFrame = false;

    return selectedFrame;
}

ExynosCameraFrameSP_sptr_t ExynosCameraFrameSelector::m_selectNormalFrame(__unused int pipeID, __unused bool isSrc, int tryCount, __unused int32_t dstPos)
{
    int ret = 0;
    ExynosCameraFrameSP_sptr_t selectedFrame = NULL;

#ifndef SAMSUNG_LBP
    ret = m_waitAndpopQ(&m_frameHoldList, selectedFrame, false, tryCount);
    if (ret < 0 || selectedFrame == NULL) {
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
#else
    int i = 0;
    uint32_t maxFrameCount = 0;

    array<ExynosCameraFrameSP_sptr_t, 4> candidateFrames;

    ExynosCameraBuffer buffer;
    int candidateFrameCount = m_frameHoldCount;
    unsigned int LBPframeCount[candidateFrameCount];

    m_LBPFrameNum = 0;
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

        if (m_parameters->getUsePureBayerReprocessing() == true) {
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
            if (LBPframeCount[0] > m_parameters->getNormalBestFrameCount()) {
                CLOGD("[LBP]:missed target frame(%d/%d).",
                        LBPframeCount[i],
                        m_parameters->getNormalBestFrameCount());
                m_lbpFrameCounter = m_parameters->getNormalBestFrameCount();
                i++;
            } else if (LBPframeCount[0] == m_parameters->getNormalBestFrameCount()) {
                CLOGD("[LBP]:Frame Count(%d)", LBPframeCount[i]);
                m_lbpFrameCounter = LBPframeCount[0];
                i++;
            } else {
                CLOGD("[LBP]:skip frame(count %d/%d)",
                        LBPframeCount[i], m_parameters->getNormalBestFrameCount());

                if (m_bufMgr == NULL) {
                    CLOGE("m_bufMgr is NULL");
                    return NULL;
                } else {
                    m_frameComplete(candidateFrames[i], false, pipeID, isSrc, dstPos, true);
                    candidateFrames[i] = NULL;
                }
            }
        }
        else{
            i++;
            m_lbpFrameCounter = 0;
        }
    } while( (i < candidateFrameCount));

    if(candidateFrameCount != REPROCESSING_BAYER_HOLD_COUNT) {
        setFrameHoldCount(REPROCESSING_BAYER_HOLD_COUNT);
        if (m_LBPFrameNum == 0) {
            CLOGV("[LBP] wait signal");
            m_LBPlock.lock();
            m_LBPcondition.waitRelative(m_LBPlock, 2000000000); //2sec
            m_LBPlock.unlock();
        }

        if (m_LBPFrameNum == 0) {
            CLOGD("[LBP] LBPFrameNum is Null. %d/%d", m_LBPFrameNum, LBPframeCount[0]);
            m_LBPFrameNum = LBPframeCount[0];
        } else if (m_LBPFrameNum > LBPframeCount[candidateFrameCount - 1] || m_LBPFrameNum < LBPframeCount[0]) {
            CLOGD("[LBP] incorrect number of m_LBPFrameNum. %d/%d", m_LBPFrameNum, LBPframeCount[0]);
            m_LBPFrameNum = LBPframeCount[0];
        }
    }

    /* Selected Frame */
    for (i = 0; i < candidateFrameCount; i++) {
        if (candidateFrameCount == REPROCESSING_BAYER_HOLD_COUNT || m_LBPFrameNum == LBPframeCount[i]) {
            CLOGD("[LBP]:selected frame(count %d)", LBPframeCount[i]);
            selectedFrame = candidateFrames[i];
        } else {
            CLOGD("[LBP]:skip frame(count %d/%d)", m_LBPFrameNum, LBPframeCount[i]);

            if (m_bufMgr == NULL) {
                CLOGE("m_bufMgr is NULL");
                return NULL;
            } else {
                m_frameComplete(candidateFrames[i], false, pipeID, isSrc, dstPos, true);
                candidateFrames[i] = NULL;
            }
        }
    }
#endif

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

#ifdef RAWDUMP_CAPTURE
ExynosCameraFrameSP_sptr_t ExynosCameraFrameSelector::m_selectRawNormalFrame(int pipeID, bool isSrc, int tryCount)
{
    int ret = 0;
    ExynosCameraFrameSP_sptr_t selectedFrame = NULL;

    ret = m_waitAndpopQ(&m_RawFrameHoldList, selectedFrame, false, tryCount);
    if( ret < 0 ||  selectedFrame == NULL ) {
        CLOGD("getFrame Fail ret(%d)", ret);
        return NULL;
    }

    CLOGD("Frame Count(%d)", selectedFrame->getFrameCount());

    return selectedFrame;
}
#endif

#ifdef OIS_CAPTURE
ExynosCameraFrameSP_sptr_t ExynosCameraFrameSelector::m_selectOISNormalFrame(int pipeID, bool isSrc, int tryCount, int32_t dstPos)
{
    int ret = 0;
    ExynosCameraFrameSP_sptr_t selectedFrame = NULL;

#ifndef SAMSUNG_LBP
    ret = m_waitAndpopQ(&m_OISFrameHoldList, selectedFrame, false, tryCount);
    if (ret < 0 || selectedFrame == NULL) {
        CLOGD("getFrame Fail ret(%d)", ret);
        m_parameters->setOISCaptureModeOn(false);
        if (selectedFrame != NULL) {
            m_LockedFrameComplete(selectedFrame, pipeID, isSrc, dstPos);
        }
        return NULL;
    } else if (isCanceled == true) {
        CLOGD("isCanceled");
        m_parameters->setOISCaptureModeOn(false);
        if (selectedFrame != NULL) {
            m_LockedFrameComplete(selectedFrame, pipeID, isSrc, dstPos);
        }
        return NULL;
    }
#else
    int i = 0;
    int candidateFrameCount = m_frameHoldCount;

    array<ExynosCameraFrameSP_sptr_t, 4> candidateFrames;

    ExynosCameraBuffer buffer;
    unsigned int LBPframeCount[candidateFrameCount];

    m_LBPFrameNum = 0;
    for(int j = 0; j < candidateFrameCount; j++) {
        LBPframeCount[j] = 0;
    }

    CLOGD("candidateFrameCount(%d).", candidateFrameCount);
    for(i = 0; i < candidateFrameCount; i++) {
        ret = m_waitAndpopQ(&m_OISFrameHoldList, candidateFrames[i], false, tryCount);
        if (ret < 0 || candidateFrames[i] == NULL) {
            CLOGD("getFrame Fail ret(%d)", ret);
            m_parameters->setOISCaptureModeOn(false);
            return NULL;
        } else if (isCanceled == true) {
            CLOGD("isCanceled");
            if (candidateFrames[i] != NULL) {
                m_LockedFrameComplete(candidateFrames[i], pipeID, isSrc, dstPos);
            }
            m_parameters->setOISCaptureModeOn(false);
            return NULL;
        }

        /* Get the Frame Count from metadata. */
        ret = m_getBufferFromFrame(candidateFrames[i], pipeID, isSrc, &buffer, dstPos);
        if (ret != NO_ERROR) {
            CLOGE("m_getBufferFromFrame fail pipeID(%d) BufferType(%s) bufferPtr(%p)",
                 pipeID, (isSrc)? "Src" : "Dst", &buffer);
        }
        if (m_parameters->getUsePureBayerReprocessing() == true) {
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

        CLOGD("[LBP]:Frame Count(%d)", LBPframeCount[i]);
        if (candidateFrameCount != REPROCESSING_BAYER_HOLD_COUNT)
            m_lbpFrameCounter = LBPframeCount[0];
        else
            m_lbpFrameCounter = 0;
    }

    CLOGD(" last capture frame");
    if (candidateFrameCount != REPROCESSING_BAYER_HOLD_COUNT) {
        ExynosCameraActivitySpecialCapture *m_sCaptureMgr = m_activityControl->getSpecialCaptureMgr();
        m_sCaptureMgr->setBestMultiCaptureMode(false);
        m_sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_WAIT_CAPTURE);

        setFrameHoldCount(REPROCESSING_BAYER_HOLD_COUNT);
        if (m_LBPFrameNum == 0) {
            CLOGV("[LBP] wait signal");
            m_LBPlock.lock();
            m_LBPcondition.waitRelative(m_LBPlock, 2000000000); //2sec
            m_LBPlock.unlock();
        }

        if (m_LBPFrameNum == 0) {
            CLOGD("[LBP] LBPFrameNum is Null. %d/%d", m_LBPFrameNum, LBPframeCount[0]);
            m_LBPFrameNum = LBPframeCount[0];
        } else if (m_LBPFrameNum > LBPframeCount[candidateFrameCount - 1] || m_LBPFrameNum < LBPframeCount[0]) {
            CLOGD("[LBP] incorrect number of m_LBPFrameNum. %d/%d", m_LBPFrameNum, LBPframeCount[0]);
            m_LBPFrameNum = LBPframeCount[0];
        }
    }

    for (i = 0; i < candidateFrameCount; i++) {
        if (m_LBPFrameNum == LBPframeCount[i] || candidateFrameCount == REPROCESSING_BAYER_HOLD_COUNT) {
            CLOGD("[LBP]:selected frame(count %d)", LBPframeCount[i]);

            selectedFrame = candidateFrames[i];
        } else {
            CLOGD("[LBP]:skip candidateFrames(count %d)", LBPframeCount[i]);

            if (m_bufMgr == NULL) {
                CLOGE("m_bufMgr is NULL");
                return NULL;
            } else {
                m_frameComplete(candidateFrames[i], false, pipeID, isSrc, dstPos, true);
                candidateFrames[i] = NULL;
            }
        }
    }
#endif

#ifndef SAMSUNG_LLS_DEBLUR
    if (m_parameters->getSeriesShotCount() == 0) {
        m_parameters->setOISCaptureModeOn(false);
    }
#endif

    CLOGD("Frame Count(%d)", selectedFrame->getFrameCount());

    return selectedFrame;
}

ExynosCameraFrameSP_sptr_t ExynosCameraFrameSelector::m_selectOISBurstFrame(int pipeID, bool isSrc, int tryCount, int32_t dstPos)
{
    int ret = 0;
    ExynosCameraFrameSP_sptr_t selectedFrame = NULL;

    ExynosCameraActivityFlash *m_flashMgr = m_activityControl->getFlashMgr();

    for (int i = 0; i < TOTAL_WAITING_TIME; i += DM_WAITING_TIME) {
        selectedFrame = m_selectOISNormalFrame(pipeID, isSrc, tryCount, dstPos);
        if (isCanceled == true) {
            CLOGD("isCanceled");
            if (selectedFrame != NULL) {
                m_LockedFrameComplete(selectedFrame, pipeID, isSrc, dstPos);
            }
            return NULL;
        } else if (selectedFrame == NULL) {
            CLOGE("selectedFrame is NULL");
            selectedFrame = m_selectNormalFrame(pipeID, isSrc, tryCount, dstPos);
        }

        /* Skip flash frame */
        if (m_flashMgr->checkPreFlash() == true) {
            if (m_flashMgr->checkFlashOff() == false) {
                CLOGD("skip flash frame(count %d)", selectedFrame->getFrameCount());

                if (m_bufMgr == NULL) {
                    CLOGE("m_bufMgr is NULL");
                    return NULL;
                } else {
                    m_frameComplete(selectedFrame, false, pipeID, isSrc, dstPos, true);
                    selectedFrame = NULL;
                }
            } else {
                CLOGD("flash off done (count %d)", selectedFrame->getFrameCount());
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
#endif

#ifdef LLS_STUNR
ExynosCameraFrameSP_sptr_t ExynosCameraFrameSelector::m_selectStunrNormalFrame(int pipeID, bool isSrc, int tryCount)
{
    int ret = 0;
    ExynosCameraFrameSP_sptr_t selectedFrame = NULL;

    ret = m_waitAndpopQ(&m_frameHoldList, selectedFrame, false, tryCount);

    if( ret < 0 ||  selectedFrame == NULL ) {
        CLOGD2("getFrame Fail ret(%d)", ret);
        return NULL;
    }

    CLOGD2("Frame Count(%d)", selectedFrame->getFrameCount());

    return selectedFrame;
}
#endif

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
#ifdef OIS_CAPTURE
            else if (ret == INVALID_OPERATION && list == &m_OISFrameHoldList) {
                CLOGE("m_OISFrameHoldList is empty");
                return ret;
            }
#endif
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

#ifdef OIS_CAPTURE
    if (m_OISFrameHoldList.isWaiting() == false) {
        ret = m_clearList(&m_OISFrameHoldList, pipeID, isSrc, dstPos);
        if( ret < 0 ) {
            CLOGE("m_OISFrameHoldList clear failed, pipeID(%d)", pipeID);
        }
    } else {
        CLOGE("Cannot clear m_OISFrameHoldList cause waiting for pop frame");
    }
#endif

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
#ifdef USE_DUAL_CAMERA_CAPTURE_FUSION
    if (m_parameters->getDualCameraMode() == true) {
        m_dualFrameSelector->flush(m_cameraId);
    }
#endif // USE_DUAL_CAMERA_CAPTURE_FUSION
#endif // BOARD_CAMERA_USES_DUAL_CAMERA

    isCanceled = false;

    return NO_ERROR;
}

#ifdef SAMSUNG_LBP
unsigned int ExynosCameraFrameSelector::getFrameIndex(void)
{
    return m_lbpFrameCounter;
}

void ExynosCameraFrameSelector::setFrameIndex(unsigned int frameCount)
{
    m_lbpFrameCounter = frameCount;
}

void ExynosCameraFrameSelector::setBestFrameNum(unsigned int frameNum)
{
    m_LBPFrameNum = frameNum;

    m_LBPlock.lock();
    m_LBPcondition.signal();
    m_LBPlock.unlock();
}
#endif

#ifdef LLS_REPROCESSING
bool ExynosCameraFrameSelector::getIsConvertingMeta()
{
    return m_isConvertingMeta;
}

void ExynosCameraFrameSelector::setIsConvertingMeta(bool isConvertingMeta)
{
    m_isConvertingMeta = isConvertingMeta;
}

bool ExynosCameraFrameSelector::getIsLastFrame()
{
    return m_isLastFrame;
}

void ExynosCameraFrameSelector::setIsLastFrame(bool isLastFrame)
{
    m_isLastFrame = isLastFrame;
}

void ExynosCameraFrameSelector::resetFrameCount()
{
    m_LLSCaptureCount = 0;
}
#endif

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
