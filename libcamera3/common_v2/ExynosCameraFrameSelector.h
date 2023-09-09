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

#ifndef EXYNOS_CAMERA_BAYER_SELECTOR_H
#define EXYNOS_CAMERA_BAYER_SELECTOR_H

#include <array>
#include "ExynosCameraParameters.h"
#include "ExynosCameraBuffer.h"
#include "ExynosCameraBufferManager.h"
#include "ExynosCameraBufferSupplier.h"
#include "ExynosCameraList.h"
#include "ExynosCameraActivityControl.h"
#include "ExynosCameraFrame.h"
#include "ExynosCameraFrameManager.h"
#ifdef SUPPORT_DEPTH_MAP
#include "ExynosCameraPipe.h"
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

    /* Selector ID */
    typedef enum SELECTOR_ID {
        SELECTOR_ID_BASE,
        SELECTOR_ID_CAPTURE,
        SELECTOR_ID_MAX,
    } SELECTOR_ID_t;

    ExynosCameraFrameSelector (int cameraId,
                            ExynosCameraConfigurations *configurations,
                            ExynosCameraParameters *param,
                            ExynosCameraBufferSupplier *bufferSupplier,
                            ExynosCameraFrameManager *manager = NULL
#ifdef SAMSUNG_DNG_DIRTY_BAYER
                            , ExynosCameraBufferManager *DNGbufMgr = NULL
#endif
                            );
    virtual ~ExynosCameraFrameSelector();
    virtual status_t release(void);
    status_t manageFrameHoldListHAL3(ExynosCameraFrameSP_sptr_t frame);
    status_t manageFrameHoldListForDynamicBayer(ExynosCameraFrameSP_sptr_t frame);
    ExynosCameraFrameSP_sptr_t selectDynamicFrames(int count, int tryCount);
    ExynosCameraFrameSP_sptr_t selectCaptureFrames(int count, uint32_t frameCount, int tryCount);
    virtual status_t clearList(void);
    uint32_t getSizeOfHoldFrame(void);
    int getFrameHoldCount(void) { return m_frameHoldCount; };
    virtual status_t setFrameHoldCount(int32_t count);
    status_t cancelPicture(bool flagCancel);
    status_t wakeupQ(void);
    void setWaitTime(uint64_t waitTime);
#ifdef OIS_CAPTURE
    void setWaitTimeOISCapture(uint64_t waitTime);
#endif

    void setId(SELECTOR_ID_t selectorId);
    SELECTOR_ID_t getId(void);

protected:
    status_t m_manageNormalFrameHoldListHAL3(ExynosCameraFrameSP_sptr_t frame, int pipeID, bool isSrc, int32_t dstPos);
    status_t m_manageHdrFrameHoldList(ExynosCameraFrameSP_sptr_t frame, int pipeID, bool isSrc, int32_t dstPos);
#ifdef RAWDUMP_CAPTURE
    status_t m_manageRawFrameHoldList(ExynosCameraFrameSP_sptr_t frame, int pipeID, bool isSrc, int32_t dstPos);
    ExynosCameraFrameSP_sptr_t m_selectRawNormalFrame(int tryCount);
#endif

    status_t m_list_release(frame_queue_t *list);
    int removeFlags;

    ExynosCameraFrameSP_sptr_t m_selectNormalFrame(int tryCount);
    ExynosCameraFrameSP_sptr_t m_selectFlashFrameV2(int tryCount);
    ExynosCameraFrameSP_sptr_t m_selectCaptureFrame(uint32_t frameCount, int tryCount);
    ExynosCameraFrameSP_sptr_t m_selectHdrFrame(int tryCount);

#ifdef OIS_CAPTURE
    status_t m_manageOISFrameHoldList(ExynosCameraFrameSP_sptr_t frame, int pipeID, bool isSrc, int32_t dstPos);
    status_t m_manageOISFrameHoldListHAL3(ExynosCameraFrameSP_sptr_t frame, int pipeID, bool isSrc, int32_t dstPos);
    ExynosCameraFrameSP_sptr_t m_selectOISNormalFrameHAL3(int tryCount);
    status_t m_manageLongExposureFrameHoldList(ExynosCameraFrameSP_sptr_t frame, int pipeID, bool isSrc, int32_t dstPos);
#endif
#ifdef SAMSUNG_LLS_DEBLUR
    status_t m_manageLDFrameHoldList(ExynosCameraFrameSP_sptr_t frame, int pipeID, bool isSrc, int32_t dstPos);
#endif
#ifdef SAMSUNG_TN_FEATURE
    status_t m_manageDynamicPickFrameHoldListHAL3(ExynosCameraFrameSP_sptr_t frame, int pipeID, bool isSrc, int32_t dstPos);
    ExynosCameraFrameSP_sptr_t m_selectDynamicPickFrameHAL3(int tryCount);
#endif

    status_t m_getBufferFromFrame(ExynosCameraFrameSP_sptr_t frame, int pipeID, bool isSrc, ExynosCameraBuffer *outBuffer, int32_t dstPos);
    status_t m_pushQ(frame_queue_t *list, ExynosCameraFrameSP_sptr_t inframe, bool lockflag);
    status_t m_popQ(frame_queue_t *list, ExynosCameraFrameSP_dptr_t outframe, bool unlockflag, int tryCount);
    status_t m_waitAndpopQ(frame_queue_t *list, ExynosCameraFrameSP_dptr_t outframe, bool unlockflag, int tryCount);
    status_t m_frameComplete(ExynosCameraFrameSP_sptr_t frame, bool isForcelyDelete = false, bool flagReleaseBuf = false);
    status_t m_LockedFrameComplete(ExynosCameraFrameSP_sptr_t frame);
    status_t m_clearList(frame_queue_t *list);
    status_t m_release(frame_queue_t *list);
    status_t m_releaseBuffer(ExynosCameraFrameSP_sptr_t frame);

    bool m_isFrameMetaTypeShotExt(void);

protected:
    frame_queue_t m_frameHoldList;
    frame_queue_t m_hdrFrameHoldList;
    frame_queue_t m_OISFrameHoldList;
#ifdef RAWDUMP_CAPTURE
    frame_queue_t m_RawFrameHoldList;
#endif
    ExynosCameraFrameManager *m_frameMgr;
    ExynosCameraConfigurations *m_configurations;
    ExynosCameraParameters *m_parameters;
    ExynosCameraBufferSupplier *m_bufferSupplier;
    ExynosCameraActivityControl *m_activityControl;

#ifdef SAMSUNG_DNG
    unsigned int m_DNGFrameCount;
    unsigned int m_preDNGFrameCount;
    ExynosCameraFrameSP_sptr_t m_preDNGFrame;
#ifdef SAMSUNG_DNG_DIRTY_BAYER
    ExynosCameraBufferManager *m_DNGbufMgr;
#endif
#endif

    int m_reprocessingCount;

    mutable Mutex m_listLock;
    int32_t m_frameHoldCount;
    bool m_isCanceled;
    int32_t m_CaptureCount;
    bool m_isFirstFrame;
    int  m_cameraId;
    char m_name[EXYNOS_CAMERA_NAME_STR_SIZE];
    state_t m_state;
    SELECTOR_ID_t m_selectorId;
};

}
#endif

