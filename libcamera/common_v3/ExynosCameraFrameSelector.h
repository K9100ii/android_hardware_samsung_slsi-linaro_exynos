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
#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
#include "ExynosCameraDualFrameSelector.h"
#endif

typedef ExynosCameraList<ExynosCameraFrameSP_sptr_t> frame_queue_t;

namespace android{

using namespace std;

class ExynosCameraFrameSelector {
public:
#ifdef USE_FRAMEMANAGER
    ExynosCameraFrameSelector (int cameraId,
                            ExynosCameraParameters *param,
                            ExynosCameraBufferManager *bufMgr, ExynosCameraFrameManager *manager = NULL
#ifdef SUPPORT_DEPTH_MAP
                            , depth_callback_queue_t *depthCallbackQ = NULL
                            , ExynosCameraBufferManager *depthMapbufMgr = NULL
#endif
                            );
#else
    ExynosCameraFrameSelector (ExynosCameraParameters *param,
                            ExynosCameraBufferManager *bufMgr);
#endif
    ~ExynosCameraFrameSelector();
    status_t release(void);
    status_t manageFrameHoldList(ExynosCameraFrameSP_sptr_t frame, int pipeID, bool isSrc, int32_t dstPos = 0);
    status_t manageFrameHoldListForDynamicBayer(ExynosCameraFrameSP_sptr_t frame);
    ExynosCameraFrameSP_sptr_t selectFrames(int count, int pipeID, bool isSrc, int tryCount, int32_t dstPos = 0);
    ExynosCameraFrameSP_sptr_t selectDynamicFrames(int count, int pipeID, bool isSrc, int tryCount, int32_t dstPos);
    ExynosCameraFrameSP_sptr_t selectCaptureFrames(int count, uint32_t frameCount, int pipeID, bool isSrc, int tryCount, int32_t dstPos = 0);
    status_t clearList(int pipeID = -1 , bool isSrc = false, int32_t dstPos = 0);
    int getHoldCount(void) { return m_frameHoldList.getSizeOfProcessQ(); };
    status_t setFrameHoldCount(int32_t count);
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
private:
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

private:
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

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
#ifdef USE_DUAL_CAMERA_CAPTURE_FUSION
    ExynosCameraDualFrameSelector *m_dualFrameSelector;
#endif // USE_DUAL_CAMERA_CAPTURE_FUSION
#endif // BOARD_CAMERA_USES_DUAL_CAMERA
};
}

#endif

