/*
 * Copyright 2014, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SEC_CAMERA_PREVIEW_FRAME_SCHEDULER_H_
#define SEC_CAMERA_PREVIEW_FRAME_SCHEDULER_H_

#include <utils/RefBase.h>
#include <utils/Timers.h>
#include <utils/List.h>
#include <utils/threads.h>

#include "ExynosCameraConfig.h"
#include "ExynosCameraThread.h"
#include "ExynosCameraDefine.h"

namespace android {

struct ISurfaceComposer;

enum frame_schedule_mode {
    FRAME_SCHEDULE_MODE_OFF = 0,
    FRAME_SCHEDULE_MODE_DURATION,
    FRAME_SCHEDULE_MODE_SCHEDULE,
};

typedef enum frame_schedule_mode frame_schedule_mode_t;

struct SecCameraPreviewFrameScheduler : public RefBase {
    SecCameraPreviewFrameScheduler();

    void    m_init();
    void    m_run();
    void    m_stop();
    void    m_release();

    void    m_setHandles(ExynosCameraBufferManager *scpBufferMgr = NULL, preview_stream_ops *previewWindow = NULL,
        ExynosCameraFrameManager *frameMgr = NULL);
    void    m_reset(frame_schedule_mode_t mode = FRAME_SCHEDULE_MODE_OFF, float targetFps = -1, bool skip = false);
    nsecs_t    m_schedule();

    status_t    m_pushProcessQ(ExynosCameraFrameSP_dptr_t frame);
    status_t    m_clearList(frame_queue_t *queue);

protected:
    virtual    ~SecCameraPreviewFrameScheduler();

private:
    void    m_updateVsync();
    bool    m_schedulerThreadFunc(void);

    nsecs_t    m_vsyncTimeOld;
    nsecs_t    m_vsyncTime;
    nsecs_t    m_vsyncPeriod;
    nsecs_t    m_videoSyncPeriod;
    uint64_t    m_frameCount;
    uint64_t    m_frameDurationUs;
    uint64_t    m_frameDropCount;
    mutable Mutex        m_updateVsyncLock;

    sp<ISurfaceComposer>    m_composer;

    typedef ExynosCameraThread<SecCameraPreviewFrameScheduler>    m_schedulerThread;
    typedef ExynosCameraList<ExynosCameraFrameSP_sptr_t> frame_queue_t;

    frame_queue_t    m_previewFrameQ;
    sp<m_schedulerThread>    m_scheduler;
    frame_schedule_mode_t    m_frameScheduleMode;
    bool        m_enableSkip;
    ExynosCameraDurationTimer    m_previewDurationTimer;
    uint64_t        m_previewDurationTime;
#ifdef PREVIEW_DURATION_DEBUG
    ExynosCameraDurationTimer    m_previewDurationDebugTimer;
#endif
    ExynosCameraBufferManager    *m_scpBufferMgr;
    preview_stream_ops                *m_previewWindow;
    ExynosCameraFrameManager    *m_frameMgr;
    bool                            m_flagThreadStop;

};

}  // namespace android

#endif  // SEC_CAMERA_PREVIEW_FRAME_SCHEDULER_H_

