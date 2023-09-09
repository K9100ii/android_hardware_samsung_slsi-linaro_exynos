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
#include "SecCameraVendorTags.h"
#ifdef SAMSUNG_TN_FEATURE
#include "ExynosCameraPPUniPlugin.h"
#endif

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

    return NO_ERROR;
}

#ifdef OIS_CAPTURE
void ExynosCameraFrameSelector::setWaitTimeOISCapture(uint64_t waitTime)
{
    m_OISFrameHoldList.setWaitTime(waitTime);
}
#endif

status_t ExynosCameraFrameSelector::manageFrameHoldListHAL3(ExynosCameraFrameSP_sptr_t frame,
                                                           int pipeID, bool isSrc, int32_t dstPos)
{
    int ret = 0;

    if (m_parameters->getHdrMode() == true
#ifdef SAMSUNG_TN_FEATURE
        || m_parameters->getShotMode() == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_HDR
#endif
        ) {
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
                ret = m_manageOISFrameHoldListHAL3(frame, pipeID, isSrc, dstPos);
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
    else if (m_parameters->getDynamicPickCaptureModeOn() == true) {
        if (removeFlags == false) {
            m_CaptureCount = 0;
            m_list_release(&m_frameHoldList, pipeID, isSrc, dstPos);
            removeFlags = true;
            CLOGI("m_frameHoldList delete(%d)", m_frameHoldList.getSizeOfProcessQ());
        }
        ret = m_manageDynamicPickFrameHoldListHAL3(frame, pipeID, isSrc, dstPos);
    } else {
        if(removeFlags == true) {
            m_list_release(&m_OISFrameHoldList, pipeID, isSrc, dstPos);
            m_CaptureCount = 0;
            removeFlags = false;
            CLOGI("m_OISFrameHoldList delete(%d)", m_OISFrameHoldList.getSizeOfProcessQ());
        }
#ifdef RAWDUMP_CAPTURE
        if(removeFlags == true) {
            m_list_release(&m_RawFrameHoldList, pipeID, isSrc, dstPos);
            removeFlags = false;
            CLOGI("m_RawFrameHoldList delete(%d)", m_RawFrameHoldList.getSizeOfProcessQ());
        }
#endif
        ret = m_manageNormalFrameHoldListHAL3(frame, pipeID, isSrc, dstPos);
    }

    return ret;
}

status_t ExynosCameraFrameSelector::m_manageNormalFrameHoldListHAL3(ExynosCameraFrameSP_sptr_t newFrame,
                                                                                int pipeID, bool isSrc, int32_t dstPos)
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
            m_LockedFrameComplete(oldFrame, pipeID, isSrc, dstPos);
            oldFrame = NULL;
        }
    }

    return ret;
}

status_t ExynosCameraFrameSelector::m_list_release(frame_queue_t *list, int pipeID, bool isSrc, int32_t dstPos)
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
            m_LockedFrameComplete(frame, pipeID, isSrc, dstPos);
            frame = NULL;
        }
    }

    return NO_ERROR;
}

#ifdef OIS_CAPTURE
status_t ExynosCameraFrameSelector::m_manageOISFrameHoldListHAL3(ExynosCameraFrameSP_sptr_t frame,
                                                                  int pipeID, bool isSrc, int32_t dstPos)
{
    int ret = 0;
    ExynosCameraBuffer buffer;
    ExynosCameraFrameSP_sptr_t oldFrame = NULL;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
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
        ((m_parameters->getMultiCaptureMode() == MULTI_CAPTURE_MODE_NONE && OISFcount == fliteFcount)
        || (m_parameters->getMultiCaptureMode() == MULTI_CAPTURE_MODE_BURST && OISFcount <= fliteFcount))) {
        CLOGI("zsl-like Fcount %d, fliteFcount %d, halcount(%d)",
                OISFcount, fliteFcount, newFrame->getFrameCount());

        m_pushQ(&m_OISFrameHoldList, newFrame, true);

        if (m_OISFrameHoldList.getSizeOfProcessQ() > m_frameHoldCount) {
            if( m_popQ(&m_OISFrameHoldList, oldFrame, true, 1) != NO_ERROR ) {
                CLOGE("getBufferToManageQ fail");
#if 0
                m_bufMgr->printBufferState();
                m_bufMgr->printBufferQState();
#endif
            } else {
                m_LockedFrameComplete(oldFrame, pipeID, isSrc, dstPos);
                oldFrame = NULL;
            }
        }
    } else if (OISCapture_activated && OISFcount + 2 < fliteFcount) {
        ret = m_manageNormalFrameHoldListHAL3(frame, pipeID, isSrc, dstPos);
        if (m_parameters->getMultiCaptureMode() == MULTI_CAPTURE_MODE_NONE) {
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

status_t ExynosCameraFrameSelector::m_manageLongExposureFrameHoldList(ExynosCameraFrameSP_sptr_t frame,
                                                                      int pipeID, bool isSrc, int32_t dstPos)
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
    if ((OISCapture_activated) && (OISFcount <= fliteFcount) && (m_CaptureCount < captureCount)) {
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
status_t ExynosCameraFrameSelector::m_manageLDFrameHoldList(ExynosCameraFrameSP_sptr_t frame,
                                                            int pipeID, bool isSrc, int32_t dstPos)
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
    captureCount = m_parameters->getLDCaptureCount();

#ifdef OIS_CAPTURE
    if ((OISCapture_activated) && (OISFcount <= fliteFcount) && (m_CaptureCount < captureCount)) {
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
status_t ExynosCameraFrameSelector::m_manageRawFrameHoldList(ExynosCameraFrameSP_sptr_t frame,
                                                             int pipeID, bool isSrc, int32_t dstPos)
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

status_t ExynosCameraFrameSelector::m_manageDynamicPickFrameHoldListHAL3(ExynosCameraFrameSP_sptr_t frame,
                                                                  int pipeID, bool isSrc, int32_t dstPos)
{
    int ret = 0;
    ExynosCameraBuffer buffer;
    ExynosCameraFrameSP_sptr_t oldFrame = NULL;
    ExynosCameraFrameSP_sptr_t newFrame  = NULL;
    ExynosCameraActivitySpecialCapture *m_sCaptureMgr = NULL;
    unsigned int DynamicPickFcount = 0;
    unsigned int fliteFcount = 0;
    newFrame = frame;
    bool DynamicPickCapture_activated = false;

    m_sCaptureMgr = m_activityControl->getSpecialCaptureMgr();
    DynamicPickFcount = m_sCaptureMgr->getDynamicPickCaptureFcount();
    if(DynamicPickFcount > 0) {
        DynamicPickCapture_activated = true;
    }

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

    if ((DynamicPickCapture_activated) && (DynamicPickFcount <= fliteFcount)) {
        CLOGI("dynamic pick Fcount %d, fliteFcount %d, halcount(%d)",
                DynamicPickFcount, fliteFcount, newFrame->getFrameCount());

        m_pushQ(&m_OISFrameHoldList, newFrame, true);

        if (m_OISFrameHoldList.getSizeOfProcessQ() > m_frameHoldCount) {
            if( m_popQ(&m_OISFrameHoldList, oldFrame, true, 1) != NO_ERROR ) {
                CLOGE("getBufferToManageQ fail");
#if 0
                m_bufMgr->printBufferState();
                m_bufMgr->printBufferQState();
#endif
            } else {
                m_LockedFrameComplete(oldFrame, pipeID, isSrc, dstPos);
                oldFrame = NULL;
            }
        }
    } else {
        CLOGI("Fcount %d, fliteFcount (%d) halcount(%d) delete",
                DynamicPickFcount, fliteFcount, newFrame->getFrameCount());
        m_LockedFrameComplete(newFrame, pipeID, isSrc, dstPos);
        newFrame = NULL;
    }

    return ret;
}

ExynosCameraFrameSP_sptr_t ExynosCameraFrameSelector::m_selectDynamicPickFrameHAL3(int pipeID, bool isSrc,
                                                                                    int tryCount, int32_t dstPos)
{
    int ret = 0;
    ExynosCameraFrameSP_sptr_t selectedFrame = NULL;

    ret = m_waitAndpopQ(&m_OISFrameHoldList, selectedFrame, false, tryCount);
    if (ret < 0 || selectedFrame == NULL) {
        CLOGD("getFrame Fail ret(%d)", ret);
        m_parameters->setDynamicPickCaptureModeOn(false);
        if (selectedFrame != NULL) {
            m_LockedFrameComplete(selectedFrame, pipeID, isSrc, dstPos);
        }
        return NULL;
    } else if (isCanceled == true) {
        CLOGD("isCanceled");
        m_parameters->setDynamicPickCaptureModeOn(false);
        if (selectedFrame != NULL) {
            m_LockedFrameComplete(selectedFrame, pipeID, isSrc, dstPos);
        }
        return NULL;
    }

    CLOGD("Frame Count(%d)", selectedFrame->getFrameCount());

    return selectedFrame;
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

    if (m_flashMgr->getNeedCaptureFlash() == true
        && m_parameters->getMultiCaptureMode() == MULTI_CAPTURE_MODE_NONE) {
        selectedFrame = m_selectFlashFrameV2(pipeID, isSrc, tryCount, dstPos);
        if (selectedFrame == NULL) {
            CLOGE("Failed to selectFlashFrame");
            selectedFrame = m_selectNormalFrame(pipeID, isSrc, tryCount, dstPos);
        }
    } else if (m_parameters->getSamsungCamera()) {
        CLOGD("Frame Count(%d)", frameCount);
        if (m_parameters->getDynamicPickCaptureModeOn() == true) {
            selectedFrame = m_selectDynamicPickFrameHAL3(pipeID, isSrc, tryCount, dstPos);
            if (selectedFrame == NULL) {
                CLOGE("Failed to selectCaptureFrame");
                selectedFrame = m_selectNormalFrame(pipeID, isSrc, tryCount, dstPos);
            }
        }
#ifdef OIS_CAPTURE
        else if (m_parameters->getOISCaptureModeOn() == true) {
            selectedFrame = m_selectOISNormalFrameHAL3(pipeID, isSrc, tryCount, dstPos);
            if (selectedFrame == NULL) {
                CLOGE("Failed to selectCaptureFrame");
                selectedFrame = m_selectNormalFrame(pipeID, isSrc, tryCount, dstPos);
            }
        }
#endif
        else {
            switch(m_parameters->getReprocessingBayerMode()) {
                case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON :
                case REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON :
                    selectedFrame = m_selectNormalFrame(pipeID, isSrc, tryCount, dstPos);
                    break;
                case REPROCESSING_BAYER_MODE_PURE_DYNAMIC :
                case REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC :
                    selectedFrame = m_selectCaptureFrame(frameCount, pipeID, isSrc, tryCount, dstPos);
                    break;
                default:
                    CLOGE("reprocessing is not valid pipeId(%d)", pipeID);
                    break;
            }

            if (selectedFrame == NULL) {
                CLOGE("Failed to selectCaptureFrame");
                selectedFrame = m_selectNormalFrame(pipeID, isSrc, tryCount, dstPos);
            }
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

ExynosCameraFrameSP_sptr_t ExynosCameraFrameSelector::m_selectNormalFrame(__unused int pipeID, __unused bool isSrc, int tryCount, __unused int32_t dstPos)
{
    int ret = 0;
    ExynosCameraFrameSP_sptr_t selectedFrame = NULL;

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
    ExynosCameraActivityFlash *flashMgr = NULL;
    flashMgr = m_activityControl->getFlashMgr();

    /* Choose bayerBuffer to process reprocessing */
    while (totalWaitingCount <= (FLASH_MAIN_TIMEOUT_COUNT + m_parameters->getReprocessingBayerHoldCount())) {
        ret = m_waitAndpopQ(&m_frameHoldList, selectedFrame, false, tryCount);
        if (ret < 0 ||  selectedFrame == NULL) {
            CLOGD("getFrame Fail ret(%d)", ret);
            flashMgr->resetShotFcount();
            return NULL;
        } else if (isCanceled == true) {
            CLOGD("isCanceled");
            if (selectedFrame != NULL) {
                m_LockedFrameComplete(selectedFrame, pipeID, isSrc, dstPos);
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

        if (isCanceled == true) {
            CLOGD("isCanceled");

            flashMgr->resetShotFcount();
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
            if (m_bufferSupplier == NULL) {
                CLOGE("m_bufferSupplier is NULL");
                flashMgr->resetShotFcount();
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

    flashMgr->resetShotFcount();
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
ExynosCameraFrameSP_sptr_t ExynosCameraFrameSelector::m_selectOISNormalFrameHAL3(int pipeID, bool isSrc, int tryCount, int32_t dstPos)
{
    int ret = 0;
    ExynosCameraFrameSP_sptr_t selectedFrame = NULL;

    ret = m_waitAndpopQ(&m_OISFrameHoldList, selectedFrame, false, tryCount);
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

#ifndef SAMSUNG_LLS_DEBLUR
    if (m_parameters->getSeriesShotCount() == 0) {
        m_parameters->setOISCaptureModeOn(false);
    }
#endif

    ALOGD("DEBUG(%s[%d]):Frame Count(%d)", __FUNCTION__, __LINE__, selectedFrame->getFrameCount());

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

    isCanceled = false;

    return NO_ERROR;
}

status_t ExynosCameraFrameSelector::m_releaseBuffer(ExynosCameraFrameSP_sptr_t frame, int pipeID, bool isSrc, int32_t dstPos)
{
    int ret = 0;
    ExynosCameraBuffer bayerBuffer;

#ifdef SUPPORT_DEPTH_MAP
    if (frame->getRequest(PIPE_VC1) == true
#ifdef SAMSUNG_FOCUS_PEAKING
        && frame->hasScenario(PP_SCENARIO_FOCUS_PEAKING) == false
#endif
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

#ifdef SAMSUNG_DNG_DIRTY_BAYER
    if (frame->getRequest(PIPE_VC0) == true) {
        ExynosCameraBuffer DNGBuffer;
        DNGBuffer.index = -2;
        ret = m_getBufferFromFrame(frame, pipeID, isSrc, &DNGBuffer, CAPTURE_NODE_1);
        if (ret != NO_ERROR) {
            CLOGE("[DNG] m_getBufferFromFrame fail pipeID(%d) BufferType(%s) bufferPtr(%p)",
                pipeID, (isSrc)? "Src" : "Dst", &bayerBuffer);
        }
        if (m_DNGbufMgr == NULL) {
            CLOGD("[DNG] m_DNGbufMgr is NULL");
        } else {
            ret = m_bufferSupplier->putBuffer(DNGBuffer);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to putBuffer. ret %d",
                        frame->getFrameCount(), DNGBuffer.index, ret);
            }
        }
    }
#endif

    ret = m_getBufferFromFrame(frame, pipeID, isSrc, &bayerBuffer, dstPos);
    if( ret != NO_ERROR ) {
        CLOGE("m_getBufferFromFrame fail pipeID(%d) BufferType(%s) bufferPtr(%p)",
                pipeID, (isSrc)? "Src" : "Dst", &bayerBuffer);
    }
    if (m_bufferSupplier == NULL) {
        CLOGE("m_bufferSupplier is NULL");
        return INVALID_OPERATION;
    } else {
        ret = m_bufferSupplier->putBuffer(bayerBuffer);
        if (ret < 0) {
            CLOGE("putIndex is %d", bayerBuffer.index);
#if 0
            m_bufMgr->printBufferState();
            m_bufMgr->printBufferQState();
#endif
        }
    }
    return NO_ERROR;
}

}
