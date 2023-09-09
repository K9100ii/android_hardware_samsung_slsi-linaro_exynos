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

#include "SecCameraPreviewFrameScheduler.h"

namespace android {

SecCameraPreviewFrameScheduler::SecCameraPreviewFrameScheduler()
    : m_vsyncTime(0),
      m_vsyncPeriod(0),
      m_frameCount(0),
      m_frameDropCount(0),
      m_frameScheduleMode(FRAME_SCHEDULE_MODE_OFF),
      m_enableSkip(false),
      m_scpBufferMgr(NULL),
      m_previewWindow(NULL),
      m_frameMgr(NULL)
{
    ALOGV("(%s[%d])", __FUNCTION__, __LINE__);
}

void SecCameraPreviewFrameScheduler::m_init()
{
    ALOGD("(%s[%d]", __FUNCTION__, __LINE__);

    m_reset(FRAME_SCHEDULE_MODE_OFF, 30, true);
    m_previewFrameQ.setWaitTime(2000000000);

    m_scheduler = new m_schedulerThread(this,
        &SecCameraPreviewFrameScheduler::m_schedulerThreadFunc,
        "PreviewFrameScheduler", PRIORITY_DISPLAY);
}

void SecCameraPreviewFrameScheduler::m_setHandles(ExynosCameraBufferManager *scpBufferMgr,
    preview_stream_ops *previewWindow, ExynosCameraFrameManager *frameMgr)
{
    ALOGD("(%s[%d]", __FUNCTION__, __LINE__);

    m_scpBufferMgr = scpBufferMgr;
    m_previewWindow = previewWindow;
    m_frameMgr = frameMgr;
}

void SecCameraPreviewFrameScheduler::m_reset(frame_schedule_mode_t mode, float targetFps, bool skip)
{
    if(m_frameCount > 240) {
        ALOGD("(%s[%d]) m_frameCount=%lld, m_frameDropCount=%lld", __FUNCTION__, __LINE__, m_frameCount, m_frameDropCount);
    }

    m_vsyncTime = 0;
    m_frameCount = 0;
    m_frameDropCount = 0;

    m_videoSyncPeriod = (nsecs_t)(1e9 / targetFps + 0.5);
    m_frameScheduleMode = mode;
    m_enableSkip = skip;
    m_frameDurationUs = m_videoSyncPeriod / 1000;

    if (m_frameDurationUs > FRAME_DURATION_OFFSET_US) {
        m_frameDurationUs -= FRAME_DURATION_OFFSET_US;
    }

    if(mode == FRAME_SCHEDULE_MODE_SCHEDULE) {
        m_updateVsync();
        ALOGD("(%s[%d] m_vsyncPeriod=%lld", __FUNCTION__, __LINE__, m_vsyncPeriod);
    }

    ALOGD("(%s[%d] targetFps=%f, videoSyncPeriod=%lld, frameScheduleMode=%d, enableSkip=%d, frameDurationUs=%lld)",
         __FUNCTION__, __LINE__, targetFps, m_videoSyncPeriod, m_frameScheduleMode, m_enableSkip, m_frameDurationUs);
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

    if (m_vsyncTime - m_vsyncTimeOld > m_videoSyncPeriod*3/2 + MULTIPLE_VSYNC_DETECT_OFFSET_NS) {
        ALOGV("Multiple vsyncs have been triggered: %lld, %lld", m_vsyncTimeOld, m_vsyncTime);
        goto func_exit;
    }

    if (m_videoSyncPeriod > m_vsyncPeriod) {
        delayTime = (m_vsyncTime - curTime + (m_videoSyncPeriod - m_vsyncPeriod) + FRAME_SCHEDULE_OFFSET_NS) % m_videoSyncPeriod;
    } else {
        delayTime = (m_vsyncTime - curTime + FRAME_SCHEDULE_OFFSET_NS) % m_videoSyncPeriod;
    }

func_exit:

    ALOGV("schedule() VsyncOld(%lld), Vsync(%lld), cur(%lld), VsyncPeriod(%lld), VideoPeriod(%lld), Delay(%lld), target(%lld)",
            m_vsyncTimeOld, m_vsyncTime, curTime, m_vsyncPeriod, m_videoSyncPeriod, delayTime, curTime+delayTime);

    return delayTime;
}

void SecCameraPreviewFrameScheduler::m_run()
{
    ALOGD("(%s[%d])", __FUNCTION__, __LINE__);

    m_flagThreadStop = false;
    m_scheduler->run();
}

void SecCameraPreviewFrameScheduler::m_stop()
{
    ALOGD("(%s[%d])", __FUNCTION__, __LINE__);

    m_flagThreadStop = true;
    stopThreadAndInputQ(m_scheduler, 1, &m_previewFrameQ);
    m_clearList(&m_previewFrameQ);
}

void SecCameraPreviewFrameScheduler::m_release()
{
    ALOGD("(%s[%d])", __FUNCTION__, __LINE__);

    m_composer.clear();
    m_previewFrameQ.release();
}

status_t SecCameraPreviewFrameScheduler::m_pushProcessQ(ExynosCameraFrameSP_dptr_t frame)
{
    status_t ret = NO_ERROR;
    m_previewFrameQ.pushProcessQ(frame);

    return ret;
}

status_t SecCameraPreviewFrameScheduler::m_clearList(frame_queue_t *queue)
{
    ExynosCameraFrameSP_sptr_t curFrame = NULL;

    if(queue->getSizeOfProcessQ() == 0) {
        return NO_ERROR;
    }

    ALOGD("DEBUG(%s):remaining frame(%d), we remove them all", __FUNCTION__, queue->getSizeOfProcessQ());

    while (0 < queue->getSizeOfProcessQ()) {
        queue->popProcessQ(&curFrame);
        if (curFrame != NULL) {
            ALOGV("DEBUG(%s):remove frame count %d", __FUNCTION__, curFrame->getFrameCount() );
            curFrame = NULL;
        }
    }
    ALOGD("DEBUG(%s):EXIT ", __FUNCTION__);

    return NO_ERROR;
}

bool SecCameraPreviewFrameScheduler::m_schedulerThreadFunc(void)
{
    int ret = 0;
    ExynosCameraBuffer buffer;
    uint64_t delayUs = 0;
    int qnum = 0;
    uint32_t offset = FRAME_DURATION_OFFSET_US;
    ExynosCameraFrameSP_sptr_t frame = NULL;

    /* Wait and pop preview frame */
    ret = m_previewFrameQ.waitAndPopProcessQ(&frame);
    if (m_flagThreadStop == true) {
        ALOGD("INFO(%s[%d]):m_flagThreadStop(%d)", __FUNCTION__, __LINE__, m_flagThreadStop);
        goto func_exit;
    }
    if (ret < 0) {
        ALOGW("WARN(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        goto func_exit;
    }

    /* Get destBuffer form frame */
    ret = frame->getDstBuffer(PIPE_SCP, &buffer);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):getDstBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        goto func_exit;
    }

    ALOGV("(%s[%d])index:%d", __FUNCTION__, __LINE__, buffer.index);

    qnum = m_previewFrameQ.getSizeOfProcessQ();

    /* Do skip frame by stacked qnum  */
    if ((m_enableSkip == true) && (qnum >= 1)) {
        m_frameDropCount++;
        ALOGD("(%s[%d])skip frame Qnum=%d", __FUNCTION__, __LINE__, qnum);
        frame->setFrameState(FRAME_STATE_SKIPPED);
        if (m_scpBufferMgr != NULL) {
            ret = m_scpBufferMgr->cancelBuffer(buffer.index, true);
            if (ret < 0) {
                ALOGE("ERR(%s[%d]):cancelBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);

            }
        }
        goto func_exit;
    }

    m_frameCount++;

    /* Do frame scheduling */
    if (m_frameScheduleMode == FRAME_SCHEDULE_MODE_SCHEDULE) {
        if ((m_enableSkip == false) && (qnum >= 1)) {
            m_frameDropCount++;
            ALOGD("(%s[%d])overwrite frame Qnum=%d", __FUNCTION__, __LINE__, qnum);
        } else {
            delayUs = m_schedule() / 1000;
        }
    } else if (m_frameScheduleMode == FRAME_SCHEDULE_MODE_DURATION) {
        m_previewDurationTimer.stop();
        m_previewDurationTime = m_previewDurationTimer.durationUsecs();
        if (qnum == 1) {
            if (m_frameDurationUs > offset) {
                m_frameDurationUs -= offset;
            }
        } else if (qnum >= 2) {
            if (m_frameDurationUs > offset*2) {
                m_frameDurationUs -= offset*2;
            }
        }
        if (m_frameDurationUs > m_previewDurationTime) {
            delayUs = m_frameDurationUs - m_previewDurationTime;
        }
    }

    /* Delay frame using delayUs value */
    if (delayUs > 0) {
        ALOGV("(%s[%d])m_frameScheduleMode=%d delayUs=%lld", __FUNCTION__, __LINE__,
            m_frameScheduleMode, delayUs);
        usleep(delayUs);
    }

    if(m_frameScheduleMode == FRAME_SCHEDULE_MODE_DURATION) {
        m_previewDurationTimer.start();
    }

    /* Set time stamp */
    if (m_previewWindow != NULL) {
        int64_t timeStamp = (int64_t)frame->getTimeStamp();

        if (timeStamp > 0L) {
            m_previewWindow->set_timestamp(m_previewWindow, timeStamp);
        } else {
            uint32_t fcount = 0;
            int frameCount = frame->getFrameCount();
            getStreamFrameCount((struct camera2_stream *)buffer.addr[buffer.getMetaPlaneIndex()], &fcount);
            ALOGW("WRN(%s[%d]): frameCount(%d)(%d), Invalid timeStamp(%lld)",
                __FUNCTION__, __LINE__,
                frameCount,
                fcount,
                timeStamp);
        }
    }

#ifdef PREVIEW_DURATION_DEBUG
    m_previewDurationDebugTimer.stop();
    ALOGW("(%s):PreviewDelay Time(%lld), Qnum(%d)", __FUNCTION__,
        m_previewDurationDebugTimer.durationUsecs(), m_previewFrameQ.getSizeOfProcessQ());
    m_previewDurationDebugTimer.start();
#endif

    /* putBuffer to surface */
    if (m_scpBufferMgr != NULL) {
        m_scpBufferMgr->putBuffer(buffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
    }

func_exit:

    if (frame != NULL) {
        frame = NULL;
    }

    return true;
}

SecCameraPreviewFrameScheduler::~SecCameraPreviewFrameScheduler()
{
    m_release();
}

} // namespace android

