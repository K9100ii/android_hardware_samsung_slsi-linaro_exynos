/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "SecCameraPreviewFrameScheduler"
#include <utils/Log.h>
#include <utils/Trace.h>

#include <sys/time.h>

#include <binder/IServiceManager.h>
#include <gui/ISurfaceComposer.h>
#include <ui/DisplayStatInfo.h>

#include "SecCameraPreviewFrameSchedulerSimple.h"

namespace android {

SecCameraPreviewFrameScheduler::SecCameraPreviewFrameScheduler()
    : m_vsyncTime(0),
      m_vsyncPeriod(0)
{
    ALOGD("(%s[%d])", __FUNCTION__, __LINE__);
    m_reset(30);
}

void SecCameraPreviewFrameScheduler::m_reset(float targetFps)
{
    m_vsyncTime = 0;
    m_lastTargetTime = 0;
    m_syncAdjustCount = 0;
    m_dropCount = 0;

    m_targetFps = targetFps;
    m_videoSyncPeriod = (nsecs_t)(1e9 / targetFps + 0.5);
    m_fpsScale = targetFps / 60.0;

    ALOGD("(%s[%d] targetFps=%f, m_videoSyncPeriod=%lld)",
        __FUNCTION__, __LINE__, targetFps, m_videoSyncPeriod);
}

void SecCameraPreviewFrameScheduler::m_updateVsync()
{
    Mutex::Autolock lock(m_updateVsyncLock);
    m_vsyncPeriod = 0;
    m_vsyncTimeOld = m_vsyncTime;
    m_vsyncTime = 0;

    // For now, surface flinger only schedules frames on the primary display
    if (m_composer == NULL) {
        String16 name("SurfaceFlinger");
        sp<IServiceManager> sm = defaultServiceManager();
        m_composer = interface_cast<ISurfaceComposer>(sm->checkService(name));
    }

    if (m_composer != NULL) {
        DisplayStatInfo stats;
        status_t res = m_composer->getDisplayStats(NULL /* display */, &stats);
        if (res == OK) {
            ALOGV("vsync time:%lld period:%lld",
                    (long long)stats.vsyncTime, (long long)stats.vsyncPeriod);
            m_vsyncTime = stats.vsyncTime;
            m_vsyncPeriod = stats.vsyncPeriod;
        } else {
            ALOGW("getDisplayStats returned %d", res);
        }
    } else {
        ALOGW("could not get surface m_composer service");
    }
}

nsecs_t SecCameraPreviewFrameScheduler::m_schedule()
{
    nsecs_t curTime = systemTime(SYSTEM_TIME_MONOTONIC);
    nsecs_t delayTime = 0;
    nsecs_t targetTime = 0;

    m_updateVsync();

    // without VSYNC info, there is nothing to do
    if (m_vsyncPeriod == 0) {
        ALOGE("no vsync: render=%lld", (long long)curTime);
        goto func_exit;
    }

    if (m_vsyncTime < m_vsyncTimeOld) {
        ALOGE("Invalid vsync time m_vsyncTime(%lld) < m_vsyncTimeOld(%lld)", m_vsyncTime, m_vsyncTimeOld);
        goto func_exit;
    }

    if (m_vsyncTime < curTime) {
        ALOGE("Vsync is older than now : m_vsyncTime(%lld), curTime(%lld)", m_vsyncTime, curTime);
        goto func_exit;
    }

    if ((m_vsyncTime - m_vsyncTimeOld) > (m_videoSyncPeriod * 1.5 + MULTIPLE_VSYNC_DETECT_OFFSET_NS)) {
        ALOGV("Multiple vsyncs have been triggered: %lld, %lld", m_vsyncTimeOld, m_vsyncTime);
        if (m_vsyncPeriod > m_videoSyncPeriod * m_fpsScale) {
            goto func_exit;
        }
    }

    delayTime = (m_vsyncTime - curTime + FRAME_SCHEDULE_OFFSET_NS) % m_vsyncPeriod;
    targetTime = curTime + delayTime;

    if ((targetTime - m_lastTargetTime) < (m_videoSyncPeriod - MULTIPLE_VSYNC_DETECT_OFFSET_NS)) {
        ALOGV("Adjust Target Time: TargetDiff(%lld), delayTime(%lld), count(%d)",
        targetTime - m_lastTargetTime, delayTime, m_syncAdjustCount);
        if (m_syncAdjustCount < SYNC_ADJUST_COUNT) {
            delayTime += m_vsyncPeriod;
            if (m_vsyncPeriod > m_videoSyncPeriod * m_fpsScale) {
                m_syncAdjustCount++;
            }
        } else {
            m_syncAdjustCount = 0;
        }
    } else {
        m_syncAdjustCount = 0;
    }

func_exit:

    if (((curTime + delayTime - m_lastTargetTime) >= (m_vsyncPeriod * 2 * (2 - m_fpsScale) - DROP_OFFSET_NS)) ||
        ((curTime + delayTime - m_lastTargetTime) <= (m_vsyncPeriod * 2 * (1 - m_fpsScale) + DROP_OFFSET_NS)))
    {
        m_dropCount++;
    }

    m_lastTargetTime = curTime + delayTime;

    ALOGV("schedule() VsyncOld(%lld), Vsync(%lld), cur(%lld), VsyncPeriod(%lld), VideoPeriod(%lld), Delay(%lld), target(%lld)",
            m_vsyncTimeOld, m_vsyncTime, curTime, m_vsyncPeriod, m_videoSyncPeriod, delayTime, curTime+delayTime);

    return delayTime;
}

void SecCameraPreviewFrameScheduler::m_schedulePreviewFrame(float targetFps)
{
    uint64_t delayUs = 0;

    if (m_targetFps != targetFps) {
        m_reset(targetFps);
    }

    delayUs = m_schedule() / 1000;

    if (delayUs > 0) {
        usleep(delayUs);
    }
}

void SecCameraPreviewFrameScheduler::m_release()
{
    ALOGD("(%s[%d] m_vsyncPeriod=%lld, Drop Count %d)",
        __FUNCTION__, __LINE__, m_vsyncPeriod, m_dropCount);

    m_composer.clear();
}

SecCameraPreviewFrameScheduler::~SecCameraPreviewFrameScheduler()
{
    m_release();
}

} // namespace android

