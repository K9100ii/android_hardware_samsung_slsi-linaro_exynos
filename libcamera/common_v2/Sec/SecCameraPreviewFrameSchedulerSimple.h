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

#ifndef SEC_CAMERA_PREVIEW_FRAME_SCHEDULER_SIMPLE_H_
#define SEC_CAMERA_PREVIEW_FRAME_SCHEDULER_SIMPLE_H_

#include <utils/RefBase.h>
#include <utils/Timers.h>
#include <utils/List.h>
#include <utils/threads.h>

namespace android {

#define MULTIPLE_VSYNC_DETECT_OFFSET_NS    (4000000)
#define FRAME_SCHEDULE_OFFSET_NS    (3000000)
#define DROP_OFFSET_NS    (1000000)
#define SYNC_ADJUST_COUNT    (12)

struct ISurfaceComposer;

struct SecCameraPreviewFrameScheduler : public RefBase {
    SecCameraPreviewFrameScheduler();

    void    m_release();
    void    m_schedulePreviewFrame(float targetFps = 30);

protected:
    virtual    ~SecCameraPreviewFrameScheduler();

private:
    void    m_reset(float targetFps = 30);
    nsecs_t    m_schedule();
    void    m_updateVsync();

    nsecs_t    m_vsyncTimeOld;
    nsecs_t    m_vsyncTime;
    nsecs_t    m_lastTargetTime;
    nsecs_t    m_vsyncPeriod;
    nsecs_t    m_videoSyncPeriod;
    int    m_syncAdjustCount;
    mutable Mutex        m_updateVsyncLock;
    float    m_fpsScale;
    int    m_dropCount;
    float    m_targetFps;

    sp<ISurfaceComposer>    m_composer;

};

}  // namespace android

#endif  // SEC_CAMERA_PREVIEW_FRAME_SCHEDULER_SIMPLE_H_

