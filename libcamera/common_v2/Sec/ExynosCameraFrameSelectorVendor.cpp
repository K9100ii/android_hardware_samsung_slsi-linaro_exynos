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
        ALOGE("DEBUG(%s[%d]):m_frameHoldList release failed ", __FUNCTION__, __LINE__);
    }
    ret = m_release(&m_hdrFrameHoldList);
    if (ret != NO_ERROR) {
        ALOGE("DEBUG(%s[%d]):m_hdrFrameHoldList release failed ", __FUNCTION__, __LINE__);
    }
#ifdef OIS_CAPTURE
    ret = m_release(&m_OISFrameHoldList);
    if (ret != NO_ERROR) {
        ALOGE("DEBUG(%s[%d]):m_OISFrameHoldList release failed ", __FUNCTION__, __LINE__);
    }
#endif

    isCanceled = false;

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    if (m_parameters->getCameraDualMode() == true) {
        ExynosCameraDualFrameSelector *dualFrameSelector = ExynosCameraSingleton<ExynosCameraDualFrameSelector>::getInstance();
        dualFrameSelector->clear(m_parameters->getCameraId());
    }
#endif

    return NO_ERROR;
}

#ifdef OIS_CAPTURE
void ExynosCameraFrameSelector::setWaitTimeOISCapture(uint64_t waitTime)
{
    m_OISFrameHoldList.setWaitTime(waitTime);
}
#endif

status_t ExynosCameraFrameSelector::manageFrameHoldList(ExynosCameraFrame *frame, int pipeID, bool isSrc, int32_t dstPos)
{
    int ret = 0;
#ifdef USE_FRAME_REFERENCE_COUNT
    frame->incRef();
#endif
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
            ALOGI("INFO(%s[%d]):m_frameHoldList delete(%d)", __FUNCTION__, __LINE__, m_frameHoldList.getSizeOfProcessQ());
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
            ALOGI("INFO(%s[%d]):m_frameHoldList delete(%d)", __FUNCTION__, __LINE__, m_frameHoldList.getSizeOfProcessQ());
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
            ALOGI("INFO(%s[%d]):m_OISFrameHoldList delete(%d)", __FUNCTION__, __LINE__, m_OISFrameHoldList.getSizeOfProcessQ());
        }
#endif
#ifdef RAWDUMP_CAPTURE
        if(removeFlags == true) {
            m_list_release(&m_RawFrameHoldList, pipeID, isSrc, dstPos);
            removeFlags = false;
            ALOGI("INFO(%s[%d]):m_RawFrameHoldList delete(%d)", __FUNCTION__, __LINE__, m_RawFrameHoldList.getSizeOfProcessQ());
        }
#endif
        ret = m_manageNormalFrameHoldList(frame, pipeID, isSrc, dstPos);
    }

    return ret;
}

status_t ExynosCameraFrameSelector::m_manageNormalFrameHoldList(ExynosCameraFrame *newFrame, int pipeID, bool isSrc, int32_t dstPos)
{
    int ret = 0;
#ifdef SAMSUNG_LBP
    unsigned int bayer_fcount = 0;
#endif
    ExynosCameraFrame *oldFrame = NULL;
    ExynosCameraBuffer buffer;

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    if (m_parameters->getCameraDualMode() == true) {
        /* handle dual sync frame */
        ExynosCameraDualFrameSelector *dualFrameSelector = ExynosCameraSingleton<ExynosCameraDualFrameSelector>::getInstance();

        ret = dualFrameSelector->setInfo(m_parameters->getCameraId(),
#ifdef USE_FRAMEMANAGER
                                         m_frameMgr,
#endif
                                         m_frameHoldCount);
        if (ret != NO_ERROR) {
            ALOGE("ERR(%s[%d]):dualFrameSelector->setInfo(id(%d)",
                __FUNCTION__, __LINE__, m_parameters->getCameraId());
            return ret;
        }

        ret = dualFrameSelector->manageNormalFrameHoldList(m_parameters->getCameraId(),
                                                          &m_frameHoldList,
                                                           newFrame, pipeID, isSrc, dstPos, m_bufMgr);

        /* if dual frame sync is fail, just use original code. */
        if (ret != NO_ERROR) {
            ALOGE("ERR(%s[%d]):dualFrameSelector->manageFrameHoldList(id(%d), pipeID(%d), BufferType(%s), dstPos(%d)",
                __FUNCTION__, __LINE__, m_parameters->getCameraId(), pipeID, (isSrc)?"Src":"Dst", dstPos);
        } else {
            return ret;
        }
    }
#endif //BOARD_CAMERA_USES_DUAL_CAMERA

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
        ALOGE("ERR(%s[%d]):m_getBufferFromFrame fail pipeID(%d) BufferType(%s) bufferPtr(%p)",
            __FUNCTION__, __LINE__, pipeID, (isSrc)?"Src":"Dst", &buffer);
    }

    if (m_parameters->getUsePureBayerReprocessing() == true) {
        camera2_shot_ext *shot_ext = NULL;
        shot_ext = (camera2_shot_ext *)(buffer.addr[1]);
        if (shot_ext != NULL)
            bayer_fcount = shot_ext->shot.dm.request.frameCount;
        else
            ALOGE("ERR(%s[%d]):Buffer is null", __FUNCTION__, __LINE__);
    } else {
        camera2_stream *shot_stream = NULL;
        shot_stream = (camera2_stream *)(buffer.addr[1]);
        if (shot_stream != NULL)
            bayer_fcount = shot_stream->fcount;
        else
            ALOGE("ERR(%s[%d]):Buffer is null", __FUNCTION__, __LINE__);
    }

    m_parameters->setBayerFrameCount(bayer_fcount);
#endif

    if (m_frameHoldList.getSizeOfProcessQ() > m_frameHoldCount) {
        if( m_popQ(&m_frameHoldList, &oldFrame, true, 1) != NO_ERROR ) {
            ALOGE("ERR(%s[%d]):getBufferToManageQ fail", __FUNCTION__, __LINE__);

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
status_t ExynosCameraFrameSelector::m_list_release(ExynosCameraList<ExynosCameraFrame *> *list, int pipeID, bool isSrc, int32_t dstPos)
{
    int ret = 0;
    ExynosCameraFrame *frame  = NULL;

    while (list->getSizeOfProcessQ() > 0) {
        if( m_popQ(list, &frame, true, 1) != NO_ERROR ) {
            ALOGE("ERR(%s[%d]):getBufferToManageQ fail", __FUNCTION__, __LINE__);
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
status_t ExynosCameraFrameSelector::m_manageOISFrameHoldList(ExynosCameraFrame *frame, int pipeID, bool isSrc, int32_t dstPos)
{
    int ret = 0;
    ExynosCameraBuffer buffer;
    ExynosCameraFrame *oldFrame = NULL;
    ExynosCameraFrame *newFrame  = NULL;
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
        ALOGE("ERR(%s[%d]):m_getBufferFromFrame fail pipeID(%d) BufferType(%s)",
            __FUNCTION__, __LINE__, pipeID, (isSrc)?"Src":"Dst");
    }

    if (m_parameters->getUsePureBayerReprocessing() == true) {
        camera2_shot_ext *shot_ext = NULL;
        shot_ext = (camera2_shot_ext *)(buffer.addr[1]);
        if (shot_ext != NULL)
            fliteFcount = shot_ext->shot.dm.request.frameCount;
        else
            ALOGE("ERR(%s[%d]):fliteReprocessingBuffer is null", __FUNCTION__, __LINE__);
    } else {
        camera2_stream *shot_stream = NULL;
        shot_stream = (camera2_stream *)(buffer.addr[1]);
        if (shot_stream != NULL)
            fliteFcount = shot_stream->fcount;
        else
            ALOGE("ERR(%s[%d]):fliteReprocessingBuffer is null", __FUNCTION__, __LINE__);
    }

    if ((OISCapture_activated) &&
        ((m_parameters->getSeriesShotCount() == 0 && OISFcount == fliteFcount)
        || (m_parameters->getSeriesShotCount() > 0 && OISFcount <= fliteFcount)
#ifdef SAMSUNG_LBP
        || (m_parameters->getUseBestPic() && OISFcount <= fliteFcount)
#endif
    )) {
        ALOGI("INFO(%s[%d]):zsl-like Fcount %d, fliteFcount %d, halcount(%d)", __FUNCTION__, __LINE__,
                OISFcount, fliteFcount, newFrame->getFrameCount());

#ifdef LLS_REPROCESSING
        if (m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_LLS && LLSCaptureFrame != NULL) {
            ALOGI("INFO(%s[%d]):list size(%d), halcount(%d) LLS halcount(%d)"
                , __FUNCTION__, __LINE__, m_OISFrameHoldList.getSizeOfProcessQ(),
                newFrame->getFrameCount(), LLSCaptureFrame->getFrameCount());

            if (m_bufMgr == NULL) {
                ALOGE("ERR(%s[%d]):m_bufMgr is NULL", __FUNCTION__, __LINE__);
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
                    ALOGE("ERR(%s[%d]):m_getBufferFromFrame fail pipeID(%d) BufferType(%s)",
                        __FUNCTION__, __LINE__, pipeID, (isSrc)?"Src":"Dst");
                }

                for(int j = 0; j < m_parameters->getSeriesShotCount(); j++) {
                    m_pushQ(&m_OISFrameHoldList, LLSCaptureFrame, true);
                    ALOGI("INFO(%s[%d]):list size(%d) index(%d) lls frame(%d)",
                        __FUNCTION__, __LINE__, m_OISFrameHoldList.getSizeOfProcessQ(), LLSCaptureFrame->getFrameCount(), j);
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
            if( m_popQ(&m_OISFrameHoldList, &oldFrame, true, 1) != NO_ERROR ) {
                ALOGE("ERR(%s[%d]):getBufferToManageQ fail", __FUNCTION__, __LINE__);

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
            ALOGI("INFO(%s[%d]):Fcount %d, fliteFcount (%d)halcount(%d)",
                    __FUNCTION__, __LINE__, OISFcount, fliteFcount, newFrame->getFrameCount());
            m_parameters->setOISCaptureModeOn(false);
        }
    } else {
        ALOGI("INFO(%s[%d]):Fcount %d, fliteFcount (%d) halcount(%d) delete",
                __FUNCTION__, __LINE__, OISFcount, fliteFcount, newFrame->getFrameCount());
        m_LockedFrameComplete(newFrame, pipeID, isSrc, dstPos);
        newFrame = NULL;
    }

    return ret;
}

status_t ExynosCameraFrameSelector::m_manageLongExposureFrameHoldList(ExynosCameraFrame *frame, int pipeID, bool isSrc, int32_t dstPos)
{
    int ret = 0;
    ExynosCameraBuffer buffer;
    ExynosCameraFrame *oldFrame = NULL;
    ExynosCameraFrame *newFrame  = NULL;
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
        ALOGE("ERR(%s[%d]):m_getBufferFromFrame fail pipeID(%d) BufferType(%s) bufferPtr(%p)",
                __FUNCTION__, __LINE__, pipeID, (isSrc)?"Src":"Dst", &buffer);
    }

    if (m_parameters->getUsePureBayerReprocessing() == true) {
        camera2_shot_ext *shot_ext = NULL;
        shot_ext = (camera2_shot_ext *)(buffer.addr[1]);
        if (shot_ext != NULL)
            fliteFcount = shot_ext->shot.dm.request.frameCount;
        else
            ALOGE("ERR(%s[%d]):fliteReprocessingBuffer is null", __FUNCTION__, __LINE__);
    } else {
        camera2_stream *shot_stream = NULL;
        shot_stream = (camera2_stream *)(buffer.addr[1]);
        if (shot_stream != NULL)
            fliteFcount = shot_stream->fcount;
        else
            ALOGE("ERR(%s[%d]):fliteReprocessingBuffer is null", __FUNCTION__, __LINE__);
    }

#ifdef OIS_CAPTURE
    if ((OISCapture_activated) && (OISFcount <= fliteFcount) &&
            (m_CaptureCount < captureCount)) {
        ALOGI("INFO(%s[%d]): Capture Fcount %d, fliteFcount %d, halcount(%d) getSizeOfProcessQ(%d)",
                __FUNCTION__, __LINE__, OISFcount, fliteFcount, newFrame->getFrameCount(),
                m_OISFrameHoldList.getSizeOfProcessQ());
        m_pushQ(&m_OISFrameHoldList, newFrame, true);
        m_CaptureCount++;
    } else
#endif
    {
        ALOGI("INFO(%s[%d]):Fcount %d, fliteFcount (%d) halcount(%d) delete",
                __FUNCTION__, __LINE__, OISFcount, fliteFcount, newFrame->getFrameCount());
        m_LockedFrameComplete(newFrame, pipeID, isSrc, dstPos);
        newFrame = NULL;
    }

    return ret;
}
#endif

#ifdef SAMSUNG_LLS_DEBLUR
status_t ExynosCameraFrameSelector::m_manageLDFrameHoldList(ExynosCameraFrame *frame, int pipeID, bool isSrc, int32_t dstPos)
{
    int ret = 0;
    ExynosCameraBuffer buffer;
    ExynosCameraFrame *oldFrame = NULL;
    ExynosCameraFrame *newFrame  = NULL;
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
        ALOGE("ERR(%s[%d]):m_getBufferFromFrame fail pipeID(%d) BufferType(%s)",
                __FUNCTION__, __LINE__, pipeID, (isSrc)?"Src":"Dst");
    }

    if (m_parameters->getUsePureBayerReprocessing() == true) {
        camera2_shot_ext *shot_ext = NULL;
        shot_ext = (camera2_shot_ext *)(buffer.addr[1]);
        if (shot_ext != NULL)
            fliteFcount = shot_ext->shot.dm.request.frameCount;
        else
            ALOGE("ERR(%s[%d]):fliteReprocessingBuffer is null", __FUNCTION__, __LINE__);
    } else {
        camera2_stream *shot_stream = NULL;
        shot_stream = (camera2_stream *)(buffer.addr[1]);
        if (shot_stream != NULL)
            fliteFcount = shot_stream->fcount;
        else
            ALOGE("ERR(%s[%d]):fliteReprocessingBuffer is null", __FUNCTION__, __LINE__);
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
        ALOGI("INFO(%s[%d]): Capture Fcount %d, fliteFcount %d, halcount(%d) getSizeOfProcessQ(%d)",
                __FUNCTION__, __LINE__, OISFcount, fliteFcount, newFrame->getFrameCount(),
                m_OISFrameHoldList.getSizeOfProcessQ());
        m_pushQ(&m_OISFrameHoldList, newFrame, true);
        m_CaptureCount++;
    } else
#endif
    {
        ALOGI("INFO(%s[%d]):Fcount %d, fliteFcount (%d) halcount(%d) delete",
                __FUNCTION__, __LINE__, OISFcount, fliteFcount, newFrame->getFrameCount());
        m_LockedFrameComplete(newFrame, pipeID, isSrc, dstPos);
        newFrame = NULL;
    }

    return ret;
}
#endif

#ifdef RAWDUMP_CAPTURE
status_t ExynosCameraFrameSelector::m_manageRawFrameHoldList(ExynosCameraFrame *frame, int pipeID, bool isSrc, int32_t dstPos)
{
    int ret = 0;
    ExynosCameraBuffer buffer;
    ExynosCameraFrame *newFrame  = NULL;
    ExynosCameraActivitySpecialCapture *m_sCaptureMgr = NULL;
    unsigned int RawFcount = 0;
    unsigned int fliteFcount = 0;
    newFrame = frame;

    m_sCaptureMgr = m_activityControl->getSpecialCaptureMgr();
    RawFcount = m_sCaptureMgr->getRawCaptureFcount();

    ret = m_getBufferFromFrame(newFrame, pipeID, isSrc, &buffer, dstPos);
    if( ret != NO_ERROR ) {
        ALOGE("ERR(%s[%d]):m_getBufferFromFrame fail pipeID(%d) BufferType(%s)",
            __FUNCTION__, __LINE__, pipeID, (isSrc)?"Src":"Dst");
    }

    if (m_parameters->getUsePureBayerReprocessing() == true) {
        camera2_shot_ext *shot_ext = NULL;
        shot_ext = (camera2_shot_ext *)(buffer.addr[1]);
        if (shot_ext != NULL)
            fliteFcount = shot_ext->shot.dm.request.frameCount;
        else
            ALOGE("ERR(%s[%d]):fliteReprocessingBuffer is null", __FUNCTION__, __LINE__);
    } else {
        camera2_stream *shot_stream = NULL;
        shot_stream = (camera2_stream *)(buffer.addr[1]);
        if (shot_stream != NULL)
            fliteFcount = shot_stream->fcount;
        else
            ALOGE("ERR(%s[%d]):fliteReprocessingBuffer is null", __FUNCTION__, __LINE__);
    }

    if (RawFcount != 0 && ((RawFcount + 1 == fliteFcount) || (RawFcount + 2 == fliteFcount))) {
        ALOGI("INFO(%s[%d]):Raw %s Capture Fcount %d, fliteFcount %d, halcount(%d)",
            __FUNCTION__, __LINE__,(RawFcount + 1 == fliteFcount) ? "first" : "sencond",
            RawFcount , fliteFcount, newFrame->getFrameCount());
        m_pushQ(&m_RawFrameHoldList, newFrame, true);
    } else {
        ALOGI("INFO(%s[%d]):Fcount %d, fliteFcount (%d) halcount(%d) delete",
            __FUNCTION__, __LINE__, RawFcount, fliteFcount, newFrame->getFrameCount());
        m_LockedFrameComplete(newFrame, pipeID, isSrc, dstPos);
        newFrame = NULL;
    }

    return ret;
}
#endif

ExynosCameraFrame* ExynosCameraFrameSelector::selectFrames(int count, int pipeID, bool isSrc, int tryCount, int32_t dstPos)
{
    ExynosCameraFrame* selectedFrame = NULL;
    ExynosCameraActivityFlash *m_flashMgr = NULL;
    ExynosCameraActivityAutofocus *afMgr = m_activityControl->getAutoFocusMgr();     // shoud not be a NULL
    ExynosCameraActivitySpecialCapture *m_sCaptureMgr = m_activityControl->getSpecialCaptureMgr();

#if defined(RAWDUMP_CAPTURE) && defined(SAMSUNG_COMPANION)
    selectedFrame = m_selectRawNormalFrame(pipeID, isSrc, tryCount);
    if (selectedFrame == NULL) {
        ALOGE("ERR(%s[%d]:select focused frame Faile!", __FUNCTION__, __LINE__);
        selectedFrame = m_selectNormalFrame(pipeID, isSrc, tryCount, dstPos);
    }
    return selectedFrame;
#endif

    m_reprocessingCount = count;

    m_flashMgr = m_activityControl->getFlashMgr();

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    if (m_parameters->getCameraDualMode() == true) {
        /* handle dual sync frame */
        ExynosCameraDualFrameSelector *dualFrameSelector = ExynosCameraSingleton<ExynosCameraDualFrameSelector>::getInstance();

        selectedFrame = dualFrameSelector->selectFrames(m_parameters->getCameraId());
    } else
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
            ALOGE("ERR(%s[%d]):select Flash Frame Fail!", __FUNCTION__, __LINE__);
            selectedFrame = m_selectNormalFrame(pipeID, isSrc, tryCount, dstPos);
        }
    } else if (m_parameters->getHdrMode() == true ||
            m_parameters->getShotMode() == SHOT_MODE_RICH_TONE) {
        selectedFrame = m_selectHdrFrame(pipeID, isSrc, tryCount, dstPos);

        if (selectedFrame == NULL && !isCanceled) {
            ALOGE("ERR(%s[%d]):select HDR Frame Fail!", __FUNCTION__, __LINE__);
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
            ALOGD("DEBUG(%s[%d]):HACK: Applying AFManager recordingHint(true)", __FUNCTION__, __LINE__);
        }

#ifdef OIS_CAPTURE
        if(m_parameters->getOISCaptureModeOn() == true) {
            selectedFrame = m_selectOISNormalFrame(pipeID, isSrc, tryCount, dstPos);
            if (selectedFrame == NULL && !isCanceled) {
                ALOGE("ERR(%s[%d]:select OISCapture frame Fail!", __FUNCTION__, __LINE__);
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
            ALOGE("ERR(%s[%d]):select Frame Fail!", __FUNCTION__, __LINE__);
    } else if (m_parameters->getSeriesShotCount() > 0) {
#ifdef OIS_CAPTURE
        if (m_parameters->getOISCaptureModeOn() == true) {
            selectedFrame = m_selectOISBurstFrame(pipeID, isSrc, tryCount, dstPos);
#ifdef LLS_REPROCESSING
            if (m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_LLS) {
                m_LLSCaptureCount++;
                m_parameters->setLLSCaptureCount(m_LLSCaptureCount);
                if (m_parameters->getSeriesShotCount() == m_LLSCaptureCount) {
                    ALOGD("DEBUG(%s[%d]): last LLS capture m_LLSCaptureCount(%d)", __FUNCTION__, __LINE__, m_LLSCaptureCount);
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
            ALOGE("ERR(%s[%d]:select focused frame Faile!", __FUNCTION__, __LINE__);
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
            ALOGE("ERR(%s[%d]:select focused frame Faile!", __FUNCTION__, __LINE__);
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
        ALOGE("ERR(%s[%d]):selectedFrame is NULL", __FUNCTION__, __LINE__);
    } else {
        ALOGD("DEBUG(%s[%d]):selectedFrame is [%d]:frameCount(%d), timeStamp(%d)",
            __FUNCTION__, __LINE__, m_parameters->getCameraId(), selectedFrame->getFrameCount(), (int)ns2ms(selectedFrame->getTimeStamp()));
    }

    m_isFirstFrame = false;

    return selectedFrame;
}

ExynosCameraFrame* ExynosCameraFrameSelector::m_selectNormalFrame(__unused int pipeID, __unused bool isSrc, int tryCount, __unused int32_t dstPos)
{
    int ret = 0;
    ExynosCameraFrame *selectedFrame = NULL;

#ifndef SAMSUNG_LBP
    ret = m_waitAndpopQ(&m_frameHoldList, &selectedFrame, false, tryCount);
    if (ret < 0 || selectedFrame == NULL) {
        ALOGD("DEBUG(%s[%d]):getFrame Fail ret(%d)", __FUNCTION__, __LINE__, ret);
        return NULL;
    } else if (isCanceled == true) {
        ALOGD("DEBUG(%s[%d]):isCanceled", __FUNCTION__, __LINE__);
        if (selectedFrame != NULL) {
            m_LockedFrameComplete(selectedFrame, pipeID, isSrc, dstPos);
        }
        return NULL;
    }
    ALOGD("DEBUG(%s[%d]):Frame Count(%d)", __FUNCTION__, __LINE__, selectedFrame->getFrameCount());
#else
    int i = 0;
    uint32_t maxFrameCount = 0;
    ExynosCameraFrame *candidateFrames[4] = {0,};
    ExynosCameraBuffer buffer;
    int candidateFrameCount = m_frameHoldCount;
    unsigned int LBPframeCount[candidateFrameCount];

    m_LBPFrameNum = 0;
    for(int j = 0; j < candidateFrameCount; j++) {
        LBPframeCount[j] = 0;
    }

    /* Recevied the Frame */
    do {
        ret = m_waitAndpopQ(&m_frameHoldList, &candidateFrames[i], false, tryCount);
        if (ret < 0 || candidateFrames[i] == NULL) {
            ALOGD("DEBUG(%s[%d]):getFrame Fail ret(%d)", __FUNCTION__, __LINE__, ret);
            return NULL;
        } else if (isCanceled == true) {
            ALOGD("DEBUG(%s[%d]):isCanceled", __FUNCTION__, __LINE__);
            if (candidateFrames[i] != NULL) {
                m_LockedFrameComplete(candidateFrames[i], pipeID, isSrc, dstPos);
            }
            return NULL;
        }

        /* Get the Frame Count from metadata. */
        ret = m_getBufferFromFrame(candidateFrames[i], pipeID, isSrc, &buffer, dstPos);
        if (ret != NO_ERROR) {
            ALOGE("ERR(%s[%d]):m_getBufferFromFrame fail pipeID(%d) BufferType(%s) bufferPtr(%p)",
                    __FUNCTION__, __LINE__, pipeID, (isSrc)?"Src":"Dst", &buffer);
        }

        if (m_parameters->getUsePureBayerReprocessing() == true) {
            camera2_shot_ext *shot_ext = NULL;
            shot_ext = (camera2_shot_ext *)(buffer.addr[1]);
            if (shot_ext != NULL)
                LBPframeCount[i] = shot_ext->shot.dm.request.frameCount;
            else
                ALOGE("ERR(%s[%d]):Buffer is null", __FUNCTION__, __LINE__);
        } else {
            camera2_stream *shot_stream = NULL;
            shot_stream = (camera2_stream *)(buffer.addr[1]);
            if (shot_stream != NULL)
                LBPframeCount[i] = shot_stream->fcount;
            else
                ALOGE("ERR(%s[%d]):Buffer is null", __FUNCTION__, __LINE__);
        }
        /***********************************/

        if(candidateFrameCount != REPROCESSING_BAYER_HOLD_COUNT) {
            if (LBPframeCount[0] > m_parameters->getNormalBestFrameCount()) {
                ALOGD("DEBUG [LBP]:(%s[%d]):missed target frame(%d/%d).",
                    __FUNCTION__, __LINE__,
                    LBPframeCount[i],
                    m_parameters->getNormalBestFrameCount());
                m_lbpFrameCounter = m_parameters->getNormalBestFrameCount();
                i++;
            } else if (LBPframeCount[0] == m_parameters->getNormalBestFrameCount()) {
                ALOGD("DEBUG [LBP]:(%s[%d]):Frame Count(%d)", __FUNCTION__, __LINE__, LBPframeCount[i]);
                m_lbpFrameCounter = LBPframeCount[0];
                i++;
            } else {
                ALOGD("DEBUG [LBP]:(%s[%d]):skip frame(count %d/%d)", __FUNCTION__, __LINE__,
                        LBPframeCount[i], m_parameters->getNormalBestFrameCount());

                if (m_bufMgr == NULL) {
                    ALOGE("ERR(%s[%d]):m_bufMgr is NULL", __FUNCTION__, __LINE__);
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
            ALOGV("[LBP] wait signal");
            m_LBPlock.lock();
            m_LBPcondition.waitRelative(m_LBPlock, 2000000000); //2sec
            m_LBPlock.unlock();
        }

        if (m_LBPFrameNum == 0) {
            ALOGD("[LBP] LBPFrameNum is Null. %d/%d", m_LBPFrameNum, LBPframeCount[0]);
            m_LBPFrameNum = LBPframeCount[0];
        } else if (m_LBPFrameNum > LBPframeCount[candidateFrameCount - 1] || m_LBPFrameNum < LBPframeCount[0]) {
            ALOGD("[LBP] incorrect number of m_LBPFrameNum. %d/%d", m_LBPFrameNum, LBPframeCount[0]);
            m_LBPFrameNum = LBPframeCount[0];
        }
    }

    /* Selected Frame */
    for (i = 0; i < candidateFrameCount; i++) {
        if (candidateFrameCount == REPROCESSING_BAYER_HOLD_COUNT || m_LBPFrameNum == LBPframeCount[i]) {
            ALOGD("[LBP](%s[%d]):selected frame(count %d)", __FUNCTION__, __LINE__, LBPframeCount[i]);
            selectedFrame = candidateFrames[i];
        } else {
            ALOGD("[LBP](%s[%d]):skip frame(count %d/%d)", __FUNCTION__, __LINE__, m_LBPFrameNum, LBPframeCount[i]);

            if (m_bufMgr == NULL) {
                ALOGE("ERR(%s[%d]):m_bufMgr is NULL", __FUNCTION__, __LINE__);
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
ExynosCameraFrame* ExynosCameraFrameSelector::m_selectFlashFrameV2(int pipeID, bool isSrc, int tryCount, int32_t dstPos)
{
    int ret = 0;
    ExynosCameraFrame *selectedFrame = NULL;
    ExynosCameraBuffer selectedBuffer;
    int bufferFcount = 0;
    int waitFcount = 0;
    int totalWaitingCount = 0;

    /* Choose bayerBuffer to process reprocessing */
    while (totalWaitingCount <= (FLASH_MAIN_TIMEOUT_COUNT + m_parameters->getReprocessingBayerHoldCount())) {
        ret = m_waitAndpopQ(&m_frameHoldList, &selectedFrame, false, tryCount);
        if (ret < 0 ||  selectedFrame == NULL) {
            ALOGD("DEBUG(%s[%d]):getFrame Fail ret(%d)", __FUNCTION__, __LINE__, ret);
            return NULL;
        } else if (isCanceled == true) {
            ALOGD("DEBUG(%s[%d]):isCanceled", __FUNCTION__, __LINE__);
            if (selectedFrame != NULL) {
                m_LockedFrameComplete(selectedFrame, pipeID, isSrc, dstPos);
            }
            return NULL;
        }
        ALOGD("DEBUG(%s[%d]):Frame Count(%d)", __FUNCTION__, __LINE__, selectedFrame->getFrameCount());

        if (waitFcount == 0) {
            ExynosCameraActivityFlash *flashMgr = NULL;
            flashMgr = m_activityControl->getFlashMgr();
            waitFcount = flashMgr->getShotFcount() + 1;
            ALOGD("DEBUG(%s):best frame count for flash capture : %d", __FUNCTION__, waitFcount);
        }

        /* Handling exception cases : like preflash is not processed */
        if (waitFcount < 0) {
            ALOGW("WARN(%s[%d]):waitFcount is negative : preflash is not processed", __FUNCTION__, __LINE__);

            return NULL;
        }

        if (isCanceled == true) {
            ALOGD("DEBUG(%s[%d]):isCanceled", __FUNCTION__, __LINE__);

            return NULL;
        }

        ret = m_getBufferFromFrame(selectedFrame, pipeID, isSrc, &selectedBuffer, dstPos);
        if (ret != NO_ERROR) {
            ALOGE("ERR(%s[%d]):m_getBufferFromFrame fail pipeID(%d) BufferType(%s) bufferPtr(%p)",
                    __FUNCTION__, __LINE__, pipeID, (isSrc)?"Src":"Dst", &selectedBuffer);
        }

        if (m_isFrameMetaTypeShotExt() == true) {
            camera2_shot_ext *shot_ext = NULL;
            shot_ext = (camera2_shot_ext *)(selectedBuffer.addr[1]);
            if (shot_ext != NULL)
                bufferFcount = shot_ext->shot.dm.request.frameCount;
            else
                ALOGE("ERR(%s[%d]):selectedBuffer is null", __FUNCTION__, __LINE__);
        } else {
            camera2_stream *shot_stream = NULL;
            shot_stream = (camera2_stream *)(selectedBuffer.addr[1]);
            if (shot_stream != NULL)
                bufferFcount = shot_stream->fcount;
            else
                ALOGE("ERR(%s[%d]):selectedBuffer is null", __FUNCTION__, __LINE__);
        }

        /* Put mismatched buffer */
        if (waitFcount != bufferFcount) {
            if (m_bufMgr == NULL) {
                ALOGE("ERR(%s[%d]):m_bufMgr is NULL", __FUNCTION__, __LINE__);
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
        ALOGD("DEBUG(%s[%d]) (totalWaitingCount %d)", __FUNCTION__, __LINE__, totalWaitingCount);
    }

    if (totalWaitingCount > FLASH_MAIN_TIMEOUT_COUNT) {
        ALOGW("WARN(%s[%d]):fail to get bayer frame count for flash capture (totalWaitingCount %d)",
                __FUNCTION__, __LINE__, totalWaitingCount);
    }

    ALOGD("DEBUG(%s[%d]):waitFcount : %d, bufferFcount : %d",
            __FUNCTION__, __LINE__, waitFcount, bufferFcount);

    return selectedFrame;
}

#ifdef RAWDUMP_CAPTURE
ExynosCameraFrame* ExynosCameraFrameSelector::m_selectRawNormalFrame(int pipeID, bool isSrc, int tryCount)
{
    int ret = 0;
    ExynosCameraFrame* selectedFrame = NULL;

    ret = m_waitAndpopQ(&m_RawFrameHoldList, &selectedFrame, false, tryCount);
    if( ret < 0 ||  selectedFrame == NULL ) {
        ALOGD("DEBUG(%s[%d]):getFrame Fail ret(%d)", __FUNCTION__, __LINE__, ret);
        return NULL;
    }

    ALOGD("DEBUG(%s[%d]):Frame Count(%d)", __FUNCTION__, __LINE__, selectedFrame->getFrameCount());

    return selectedFrame;
}
#endif

#ifdef OIS_CAPTURE
ExynosCameraFrame* ExynosCameraFrameSelector::m_selectOISNormalFrame(int pipeID, bool isSrc, int tryCount, int32_t dstPos)
{
    int ret = 0;
    ExynosCameraFrame* selectedFrame = NULL;

#ifndef SAMSUNG_LBP
    ret = m_waitAndpopQ(&m_OISFrameHoldList, &selectedFrame, false, tryCount);
    if (ret < 0 || selectedFrame == NULL) {
        ALOGD("DEBUG(%s[%d]):getFrame Fail ret(%d)", __FUNCTION__, __LINE__, ret);
        m_parameters->setOISCaptureModeOn(false);
        if (selectedFrame != NULL) {
            m_LockedFrameComplete(selectedFrame, pipeID, isSrc, dstPos);
        }
        return NULL;
    } else if (isCanceled == true) {
        ALOGD("DEBUG(%s[%d]):isCanceled", __FUNCTION__, __LINE__);
        m_parameters->setOISCaptureModeOn(false);
        if (selectedFrame != NULL) {
            m_LockedFrameComplete(selectedFrame, pipeID, isSrc, dstPos);
        }
        return NULL;
    }
#else
    int i = 0;
    int candidateFrameCount = m_frameHoldCount;
    ExynosCameraFrame *candidateFrames[4] = {0,};
    ExynosCameraBuffer buffer;
    unsigned int LBPframeCount[candidateFrameCount];

    m_LBPFrameNum = 0;
    for(int j = 0; j < candidateFrameCount; j++) {
        LBPframeCount[j] = 0;
    }

    ALOGD("DEBUG(%s:[%d]):candidateFrameCount(%d).", __FUNCTION__, __LINE__, candidateFrameCount);
    for(i = 0; i < candidateFrameCount; i++) {
        ret = m_waitAndpopQ(&m_OISFrameHoldList, &candidateFrames[i], false, tryCount);
        if (ret < 0 || candidateFrames[i] == NULL) {
            ALOGD("DEBUG(%s[%d]):getFrame Fail ret(%d)", __FUNCTION__, __LINE__, ret);
            m_parameters->setOISCaptureModeOn(false);
            return NULL;
        } else if (isCanceled == true) {
            ALOGD("DEBUG(%s[%d]):isCanceled", __FUNCTION__, __LINE__);
            if (candidateFrames[i] != NULL) {
                m_LockedFrameComplete(candidateFrames[i], pipeID, isSrc, dstPos);
            }
            m_parameters->setOISCaptureModeOn(false);
            return NULL;
        }

        /* Get the Frame Count from metadata. */
        ret = m_getBufferFromFrame(candidateFrames[i], pipeID, isSrc, &buffer, dstPos);
        if (ret != NO_ERROR) {
            ALOGE("ERR(%s[%d]):m_getBufferFromFrame fail pipeID(%d) BufferType(%s) bufferPtr(%p)",
                __FUNCTION__, __LINE__, pipeID, (isSrc)?"Src":"Dst", &buffer);
        }
        if (m_parameters->getUsePureBayerReprocessing() == true) {
            camera2_shot_ext *shot_ext = NULL;
            shot_ext = (camera2_shot_ext *)(buffer.addr[1]);
            if (shot_ext != NULL)
                LBPframeCount[i] = shot_ext->shot.dm.request.frameCount;
            else
                ALOGE("ERR(%s[%d]):Buffer is null", __FUNCTION__, __LINE__);
        } else {
            camera2_stream *shot_stream = NULL;
            shot_stream = (camera2_stream *)(buffer.addr[1]);
            if (shot_stream != NULL)
                LBPframeCount[i] = shot_stream->fcount;
            else
                ALOGE("ERR(%s[%d]):Buffer is null", __FUNCTION__, __LINE__);
        }

        ALOGD("[LBP](%s[%d]):Frame Count(%d)", __FUNCTION__, __LINE__, LBPframeCount[i]);
        if (candidateFrameCount != REPROCESSING_BAYER_HOLD_COUNT)
            m_lbpFrameCounter = LBPframeCount[0];
        else
            m_lbpFrameCounter = 0;
    }

    ALOGD("DEBUG(%s[%d]): last capture frame", __FUNCTION__, __LINE__);
    if (candidateFrameCount != REPROCESSING_BAYER_HOLD_COUNT) {
        ExynosCameraActivitySpecialCapture *m_sCaptureMgr = m_activityControl->getSpecialCaptureMgr();
        m_sCaptureMgr->setBestMultiCaptureMode(false);
        m_sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_WAIT_CAPTURE);

        setFrameHoldCount(REPROCESSING_BAYER_HOLD_COUNT);
        if (m_LBPFrameNum == 0) {
            ALOGV("[LBP] wait signal");
            m_LBPlock.lock();
            m_LBPcondition.waitRelative(m_LBPlock, 2000000000); //2sec
            m_LBPlock.unlock();
        }

        if (m_LBPFrameNum == 0) {
            ALOGD("[LBP] LBPFrameNum is Null. %d/%d", m_LBPFrameNum, LBPframeCount[0]);
            m_LBPFrameNum = LBPframeCount[0];
        } else if (m_LBPFrameNum > LBPframeCount[candidateFrameCount - 1] || m_LBPFrameNum < LBPframeCount[0]) {
            ALOGD("[LBP] incorrect number of m_LBPFrameNum. %d/%d", m_LBPFrameNum, LBPframeCount[0]);
            m_LBPFrameNum = LBPframeCount[0];
        }
    }

    for (i = 0; i < candidateFrameCount; i++) {
        if (m_LBPFrameNum == LBPframeCount[i] || candidateFrameCount == REPROCESSING_BAYER_HOLD_COUNT) {
            ALOGD("[LBP](%s[%d]):selected frame(count %d)", __FUNCTION__, __LINE__, LBPframeCount[i]);

            selectedFrame = candidateFrames[i];
        } else {
            ALOGD("[LBP](%s[%d]):skip candidateFrames(count %d)", __FUNCTION__, __LINE__, LBPframeCount[i]);

            if (m_bufMgr == NULL) {
                ALOGE("ERR(%s[%d]):m_bufMgr is NULL", __FUNCTION__, __LINE__);
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

    ALOGD("DEBUG(%s[%d]):Frame Count(%d)", __FUNCTION__, __LINE__, selectedFrame->getFrameCount());

    return selectedFrame;
}

ExynosCameraFrame* ExynosCameraFrameSelector::m_selectOISBurstFrame(int pipeID, bool isSrc, int tryCount, int32_t dstPos)
{
    int ret = 0;
    ExynosCameraFrame *selectedFrame = NULL;

    ExynosCameraActivityFlash *m_flashMgr = m_activityControl->getFlashMgr();

    for (int i = 0; i < TOTAL_WAITING_TIME; i += DM_WAITING_TIME) {
        selectedFrame = m_selectOISNormalFrame(pipeID, isSrc, tryCount, dstPos);
        if (isCanceled == true) {
            ALOGD("DEBUG(%s[%d]):isCanceled", __FUNCTION__, __LINE__);
            if (selectedFrame != NULL) {
                m_LockedFrameComplete(selectedFrame, pipeID, isSrc, dstPos);
            }
            return NULL;
        } else if (selectedFrame == NULL) {
            ALOGE("ERR(%s[%d]):selectedFrame is NULL", __FUNCTION__, __LINE__);
            selectedFrame = m_selectNormalFrame(pipeID, isSrc, tryCount, dstPos);
        }

        /* Skip flash frame */
        if (m_flashMgr->checkPreFlash() == true) {
            if (m_flashMgr->checkFlashOff() == false) {
                ALOGD("DEBUG(%s[%d]):skip flash frame(count %d)", __FUNCTION__, __LINE__, selectedFrame->getFrameCount());

                if (m_bufMgr == NULL) {
                    ALOGE("ERR(%s[%d]):m_bufMgr is NULL", __FUNCTION__, __LINE__);
                    return NULL;
                } else {
                    m_frameComplete(selectedFrame, false, pipeID, isSrc, dstPos, true);
                    selectedFrame = NULL;
                }
            } else {
                ALOGD("DEBUG(%s[%d]):flash off done (count %d)", __FUNCTION__, __LINE__, selectedFrame->getFrameCount());
                break;
            }
        } else {
            ALOGD("DEBUG(%s[%d]):pre-flash off", __FUNCTION__, __LINE__);
            break;
        }
        usleep(DM_WAITING_TIME);
    }

    return selectedFrame;
}
#endif

status_t ExynosCameraFrameSelector::m_waitAndpopQ(ExynosCameraList<ExynosCameraFrame *> *list, ExynosCameraFrame** outframe, bool unlockflag, int tryCount)
{
    status_t ret = NO_ERROR;
    int iter = 0;

    do {
        ret = list->waitAndPopProcessQ(outframe);

        if (isCanceled == true) {
            ALOGD("DEBUG(%s[%d]):isCanceled", __FUNCTION__, __LINE__);

            return NO_ERROR;
        }

        if( ret < 0 ) {
            if( ret == TIMED_OUT ) {
                ALOGD("DEBUG(%s[%d]):waitAndPopQ Time out -> retry[max cur](%d %d)", __FUNCTION__, __LINE__, tryCount, iter);
                iter++;
                continue;
            }
#ifdef OIS_CAPTURE
            else if (ret == INVALID_OPERATION && list == &m_OISFrameHoldList) {
                ALOGE("ERR(%s[%d]):m_OISFrameHoldList is empty", __FUNCTION__, __LINE__);
                return ret;
            }
#endif
        }
        if(*outframe != NULL) {
            ALOGD("DEBUG(%s[%d]):Frame Count(%d)", __FUNCTION__, __LINE__, (*outframe)->getFrameCount());
        }
    } while (ret != OK && tryCount > iter);

    if(ret != OK) {
        ALOGE("ERR(%s[%d]):wait for popQ fail(%d)", __FUNCTION__, __LINE__, ret);
        return ret;
    }

    if(*outframe == NULL) {
        ALOGE("ERR(%s[%d]):wait for popQ frame = NULL frame(%p)", __FUNCTION__, __LINE__, *outframe);
        return ret;
    }

    if(unlockflag) {
        (*outframe)->frameUnlock();
    }
    return ret;
}

status_t ExynosCameraFrameSelector::clearList(int pipeID, bool isSrc, int32_t dstPos)
{
    int ret = 0;
    ExynosCameraFrame *frame = NULL;
    if (m_frameHoldList.isWaiting() == false) {
        ret = m_clearList(&m_frameHoldList, pipeID, isSrc, dstPos);
        if( ret < 0 ) {
            ALOGE("DEBUG(%s[%d]):m_frameHoldList clear failed, pipeID(%d)", __FUNCTION__, __LINE__, pipeID);
        }
    } else {
        ALOGE("ERR(%s[%d]):Cannot clear frameHoldList cause waiting for pop frame", __FUNCTION__, __LINE__);
    }

    if (m_hdrFrameHoldList.isWaiting() == false) {
        ret = m_clearList(&m_hdrFrameHoldList, pipeID, isSrc, dstPos);
        if( ret < 0 ) {
            ALOGE("DEBUG(%s[%d]):m_hdrFrameHoldList clear failed, pipeID(%d)", __FUNCTION__, __LINE__, pipeID);
        }
    } else {
        ALOGE("ERR(%s[%d]):Cannot clear hdrFrameHoldList cause waiting for pop frame", __FUNCTION__, __LINE__);
    }

#ifdef OIS_CAPTURE
    if (m_OISFrameHoldList.isWaiting() == false) {
        ret = m_clearList(&m_OISFrameHoldList, pipeID, isSrc, dstPos);
        if( ret < 0 ) {
            ALOGE("DEBUG(%s[%d]):m_OISFrameHoldList clear failed, pipeID(%d)", __FUNCTION__, __LINE__, pipeID);
        }
    } else {
        ALOGE("ERR(%s[%d]):Cannot clear m_OISFrameHoldList cause waiting for pop frame", __FUNCTION__, __LINE__);
    }
#endif

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

status_t ExynosCameraFrameSelector::m_releaseBuffer(ExynosCameraFrame *frame, int pipeID, bool isSrc, int32_t dstPos)
{
    int ret = 0;
    ExynosCameraBuffer bayerBuffer;

#ifdef SUPPORT_DEPTH_MAP
    if (frame->getRequest(PIPE_VC1) == true) {
        int pipeId = PIPE_FLITE;
        ExynosCameraBuffer depthMapBuffer;

        depthMapBuffer.index = -2;

        ret = frame->getDstBuffer(pipeId, &depthMapBuffer, CAPTURE_NODE_2);
        if (ret != NO_ERROR) {
            ALOGE("ERR(%s[%d]):Failed to get DepthMap buffer", __FUNCTION__, __LINE__);
        }

        if(m_parameters->getDepthCallbackOnPreview()
            && depthMapBuffer.index != -2
            && m_parameters->getFrameSkipCount() <= 1
            && m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_METADATA) == true
            && m_depthCallbackQ->getSizeOfProcessQ() < 2) {
            m_depthCallbackQ->pushProcessQ(&depthMapBuffer);
            ALOGV("DEBUG(%s[%d]):[Depth] push depthMapBuffer.index(%d) HAL count(%d) getSizeOfProcessQ(%d)",
                    __FUNCTION__, __LINE__, depthMapBuffer.index, frame->getFrameCount(),
                    m_depthCallbackQ->getSizeOfProcessQ());
        } else if (depthMapBuffer.index != -2) {
            ret = m_depthMapbufMgr->putBuffer(depthMapBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
            if (ret != NO_ERROR) {
                ALOGE("ERR(%s[%d]):Failed to put DepthMap buffer to bufferMgr",
                        __FUNCTION__, __LINE__);
            }
            ALOGV("DEBUG(%s[%d]):[Depth] pop depthMapBuffer.index(%d) HAL count(%d) \
                    getNumOfAvailableBuffer(%d) getSizeOfProcessQ(%d)",
                    __FUNCTION__, __LINE__, depthMapBuffer.index, frame->getFrameCount()
                    , m_depthMapbufMgr->getNumOfAvailableBuffer(), m_depthCallbackQ->getSizeOfProcessQ());
        }
    }
#endif

    ret = m_getBufferFromFrame(frame, pipeID, isSrc, &bayerBuffer, dstPos);
    if( ret != NO_ERROR ) {
        ALOGE("ERR(%s[%d]):m_getBufferFromFrame fail pipeID(%d) BufferType(%s) bufferPtr(%p)",
                __FUNCTION__, __LINE__, pipeID, (isSrc)?"Src":"Dst", &bayerBuffer);
    }
    if (m_bufMgr == NULL) {
        ALOGE("ERR(%s[%d]):m_bufMgr is NULL", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    } else {
        ret = m_bufMgr->putBuffer(bayerBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):putIndex is %d", __FUNCTION__, __LINE__, bayerBuffer.index);
            m_bufMgr->printBufferState();
            m_bufMgr->printBufferQState();
        }
    }
    return NO_ERROR;
}
}
