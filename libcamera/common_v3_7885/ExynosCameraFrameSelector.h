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

#ifndef EXYNOS_CAMERA_BAYER_SELECTOR_H
#define EXYNOS_CAMERA_BAYER_SELECTOR_H

#include <array>
#include "ExynosCameraParameters.h"
#include "ExynosCameraBuffer.h"
#include "ExynosCameraBufferManager.h"
#include "ExynosCameraList.h"
#include "ExynosCameraActivityControl.h"
#include "ExynosCameraFrame.h"
#ifdef USE_FRAMEMANAGER
#include "ExynosCameraFrameManager.h"
#endif
#ifdef SUPPORT_DEPTH_MAP
#include "ExynosCameraPipe.h"

typedef ExynosCameraList<ExynosCameraBuffer> depth_callback_queue_t;
#endif

typedef ExynosCameraList<ExynosCameraFrameSP_sptr_t> frame_queue_t;

namespace android{

using namespace std;

class ExynosCameraFrameSelector {
public:
    /* Frame Select result */
    typedef enum result {
        RESULT_BASE,
        /* HAS_FRAME : The frameSelector select a frame without any problems */
        RESULT_HAS_FRAME,
        /* NO_FRAME : There's any frames in frameSelector */
        RESULT_NO_FRAME,
        /*
         * SKIP_FRAME : There's any frames in frameSelector also.
         * But capture sequence should be done without frame.
         * (eg. in switch mode, there's a slave frame and no
         *  master frame. but capture sequence should done in
         *  master camera)
         */
        RESULT_SKIP_FRAME,
        RESULT_MAX,
    } result_t;

    /* Frame Select state */
    typedef enum state {
        STATE_BASE,
        /* in this state, frameSelector can return HAS_FRAME or NO_FRAME */
        STATE_NORMAL,
        /* in this state, frameSelector can return SKIP_FRAME */
        STATE_STANDBY,
        STATE_MAX,
    } state_t;

#ifdef USE_FRAMEMANAGER
    ExynosCameraFrameSelector (int cameraId,
                            ExynosCameraParameters *param,
                            ExynosCameraBufferManager *bufMgr, ExynosCameraFrameManager *manager = NULL
#ifdef SUPPORT_DEPTH_MAP
                            , depth_callback_queue_t *depthCallbackQ = NULL
                            , ExynosCameraBufferManager *depthMapbufMgr = NULL
#endif
#if defined(SAMSUNG_DNG_DIRTY_BAYER) || defined(DEBUG_RAWDUMP_DIRTY_BAYER)
                            , ExynosCameraBufferManager *DNGbufMgr = NULL
#endif
                            );
#else
    ExynosCameraFrameSelector (ExynosCameraParameters *param,
                            ExynosCameraBufferManager *bufMgr);
#endif
    virtual ~ExynosCameraFrameSelector();
    virtual status_t release(void);
    virtual status_t manageFrameHoldList(ExynosCameraFrameSP_sptr_t frame, int pipeID, bool isSrc, int32_t dstPos = 0);
    status_t manageFrameHoldListForDynamicBayer(ExynosCameraFrameSP_sptr_t frame);
#ifdef USE_DUAL_CAMERA
    virtual status_t manageSyncFrameHoldList(ExynosCameraFrameSP_sptr_t frame, int pipeID, bool isSrc, int32_t dstPos = 0);
    /*
     * In case of sync mode, specific frame have to be reserved.
     *  ex. Maybe there's a situation that
     *      Master Camera select N frame and process the frame in takePicture,
     *      Slave Camera haven't selected N frame yet. But new bayer buffer was
     *      out from driver and ExynosCamera push the frame to selector.
     *      In this situation, we want not to remove N frame (holdCount = 1).
     */
    virtual status_t setSyncFrameHoldCount(int32_t holdCountForSync);
    uint32_t getSyncFrameHoldCount(void) { return m_syncFrameHoldCount; }
#endif
    virtual result_t selectFrames(ExynosCameraFrameSP_dptr_t selectedFrame, int count, int pipeID, bool isSrc, int tryCount, int32_t dstPos = 0);
    ExynosCameraFrameSP_sptr_t selectDynamicFrames(int count, int pipeID, bool isSrc, int tryCount, int32_t dstPos);
    ExynosCameraFrameSP_sptr_t selectCaptureFrames(int count, uint32_t frameCount, int pipeID, bool isSrc, int tryCount, int32_t dstPos = 0);
    virtual status_t clearList(int pipeID = -1 , bool isSrc = false, int32_t dstPos = 0);
    uint32_t getSizeOfHoldFrame(void);
    int getFrameHoldCount(void) { return m_frameHoldCount; };
    virtual status_t setFrameHoldCount(int32_t count);
#ifdef SAMSUNG_LBP
    unsigned int getFrameIndex(void);
    void setFrameIndex(unsigned int frameCount);
    void setBestFrameNum(unsigned int frameNum);
#endif
    status_t cancelPicture(bool flagCancel = true);
    status_t wakeupQ(void);
    void setWaitTime(uint64_t waitTime);
#ifdef OIS_CAPTURE
    void setWaitTimeOISCapture(uint64_t waitTime);
#endif
    void setIsFirstFrame(bool isFirstFrame);
    bool getIsFirstFrame();
#ifdef LLS_REPROCESSING
    bool getIsLastFrame();
    void setIsLastFrame(bool isLastFrame);
    bool getIsConvertingMeta();
    void setIsConvertingMeta(bool isConvertingMeta);
    void resetFrameCount();
#endif

    void wakeselectDynamicFrames(void);
protected:
    status_t m_manageNormalFrameHoldList(ExynosCameraFrameSP_sptr_t frame, int pipeID, bool isSrc, int32_t dstPos);
    status_t m_manageHdrFrameHoldList(ExynosCameraFrameSP_sptr_t frame, int pipeID, bool isSrc, int32_t dstPos);
#ifdef RAWDUMP_CAPTURE
    status_t m_manageRawFrameHoldList(ExynosCameraFrameSP_sptr_t frame, int pipeID, bool isSrc, int32_t dstPos);
    ExynosCameraFrameSP_sptr_t m_selectRawNormalFrame(int pipeID, bool isSrc, int tryCount);
#endif
#if defined(OIS_CAPTURE) || defined(RAWDUMP_CAPTURE)
    status_t m_list_release(frame_queue_t *list, int pipeID, bool isSrc, int32_t dstPos);
    int removeFlags;
#endif

    ExynosCameraFrameSP_sptr_t m_selectNormalFrame(int pipeID, bool isSrc, int tryCount, int32_t dstPos);
    ExynosCameraFrameSP_sptr_t m_selectFlashFrame(int pipeID, bool isSrc, int tryCount, int32_t dstPos);
    ExynosCameraFrameSP_sptr_t m_selectFlashFrameV2(int pipeID, bool isSrc, int tryCount, int32_t dstPos);
    ExynosCameraFrameSP_sptr_t m_selectFocusedFrame(int pipeID, bool isSrc, int tryCount, int32_t dstPos);
    ExynosCameraFrameSP_sptr_t m_selectHdrFrame(int pipeID, bool isSrc, int tryCount, int32_t dstPos);
    ExynosCameraFrameSP_sptr_t m_selectBurstFrame(int pipeID, bool isSrc, int tryCount, int32_t dstPos);
    ExynosCameraFrameSP_sptr_t m_selectCaptureFrame(uint32_t frameCount, int pipeID, bool isSrc, int tryCount, int32_t dstPos);
#ifdef OIS_CAPTURE
    status_t m_manageOISFrameHoldList(ExynosCameraFrameSP_sptr_t frame, int pipeID, bool isSrc, int32_t dstPos);
    ExynosCameraFrameSP_sptr_t m_selectOISNormalFrame(int pipeID, bool isSrc, int tryCount, int32_t dstPos);
    ExynosCameraFrameSP_sptr_t m_selectOISBurstFrame(int pipeID, bool isSrc, int tryCount, int32_t dstPos);
    status_t m_manageLongExposureFrameHoldList(ExynosCameraFrameSP_sptr_t frame, int pipeID, bool isSrc, int32_t dstPos);
#endif
#ifdef SAMSUNG_LLS_DEBLUR
    status_t m_manageLDFrameHoldList(ExynosCameraFrameSP_sptr_t frame, int pipeID, bool isSrc, int32_t dstPos);
#endif
#ifdef LLS_STUNR
    ExynosCameraFrameSP_sptr_t m_selectStunrNormalFrame(int pipeID, bool isSrc, int tryCount);
#endif
    status_t m_getBufferFromFrame(ExynosCameraFrameSP_sptr_t frame, int pipeID, bool isSrc, ExynosCameraBuffer *outBuffer, int32_t dstPos);
    status_t m_pushQ(frame_queue_t *list, ExynosCameraFrameSP_sptr_t inframe, bool lockflag);
    status_t m_popQ(frame_queue_t *list, ExynosCameraFrameSP_dptr_t outframe, bool unlockflag, int tryCount);
    status_t m_waitAndpopQ(frame_queue_t *list, ExynosCameraFrameSP_dptr_t outframe, bool unlockflag, int tryCount);
    status_t m_frameComplete(ExynosCameraFrameSP_sptr_t frame, bool isForcelyDelete = false,
                                int pipeID = 0, bool isSrc = false, int32_t dstPos = 0, bool flagReleaseBuf = false);
    status_t m_LockedFrameComplete(ExynosCameraFrameSP_sptr_t frame, int pipeID, bool isSrc, int32_t dstPos);
    status_t m_clearList(frame_queue_t *list, int pipeID, bool isSrc, int32_t dstPos);
    status_t m_release(frame_queue_t *list);
    status_t m_releaseBuffer(ExynosCameraFrameSP_sptr_t frame, int pipeID, bool isSrc, int32_t dstPos);

    bool m_isFrameMetaTypeShotExt(void);

protected:
    frame_queue_t m_frameHoldList;
    frame_queue_t m_hdrFrameHoldList;
#ifdef OIS_CAPTURE
    frame_queue_t m_OISFrameHoldList;
#endif
#ifdef RAWDUMP_CAPTURE
    frame_queue_t m_RawFrameHoldList;
#endif
#ifdef USE_FRAMEMANAGER
    ExynosCameraFrameManager *m_frameMgr;
#endif
    ExynosCameraParameters *m_parameters;
    ExynosCameraBufferManager *m_bufMgr;
    ExynosCameraActivityControl *m_activityControl;

#ifdef LLS_REPROCESSING
    ExynosCameraFrameSP_sptr_t LLSCaptureFrame;
    bool m_isLastFrame;
    bool m_isConvertingMeta;
    int32_t m_LLSCaptureCount;
#endif

#ifdef SAMSUNG_DNG
    unsigned int m_DNGFrameCount;
    unsigned int m_preDNGFrameCount;
    ExynosCameraFrameSP_sptr_t m_preDNGFrame;
#if defined(SAMSUNG_DNG_DIRTY_BAYER) || defined(DEBUG_RAWDUMP_DIRTY_BAYER)
    ExynosCameraBufferManager *m_DNGbufMgr;
#endif
#else
#if defined(SAMSUNG_DNG_DIRTY_BAYER) || defined(DEBUG_RAWDUMP_DIRTY_BAYER)
    ExynosCameraBufferManager *m_DNGbufMgr;
#endif
#endif

#ifdef SUPPORT_DEPTH_MAP
    depth_callback_queue_t *m_depthCallbackQ;
    ExynosCameraBufferManager *m_depthMapbufMgr;
#endif

    int m_reprocessingCount;

    mutable Mutex m_listLock;
    int32_t m_frameHoldCount;
    bool isCanceled;
#ifdef SAMSUNG_LBP
    mutable Mutex m_LBPlock;
    mutable Condition   m_LBPcondition;
    unsigned int m_lbpFrameCounter;
    unsigned int m_LBPFrameNum;
#endif
    int32_t m_CaptureCount;
    bool m_isFirstFrame;
    int  m_cameraId;
    char m_name[EXYNOS_CAMERA_NAME_STR_SIZE];
    state_t m_state;
#ifdef USE_DUAL_CAMERA
    Mutex           m_lock;
    sync_type_t     m_lastSyncType;
    int32_t         m_syncFrameHoldCount;
#endif // USE_DUAL_CAMERA
};
}

#endif

