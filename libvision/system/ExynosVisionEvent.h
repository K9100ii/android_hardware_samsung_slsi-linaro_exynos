/*
 * Copyright (C) 2015, Samsung Electronics Co. LTD
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

#ifndef EXYNOS_VISION_EVENT_H
#define EXYNOS_VISION_EVENT_H

#include <utils/threads.h>
#include <utils/Mutex.h>
#include <utils/List.h>

namespace android {

typedef enum _event_exception_t {
    EVENT_EXCEPTION_NONE = 1,
    EVENT_EXCEPTION_TIME_OUT,
    EVENT_EXCEPTION_WAKE_UP,
    EVENT_EXCEPTION_UNKNOWN,
} event_exception_t;

class ExynosVisionEvent {
private:
    /* subgraph done event */
    uint64_t            m_waitTime;

    Mutex m_event_mutex;

    mutable Condition   m_event_cond;

    bool m_event_done_flag;
    bool m_wake_up_flag;

    uint32_t m_waiting_object_num;

public:

private:

public:
    ExynosVisionEvent(uint64_t wait_time)
    {
        m_waitTime = wait_time;

        m_event_done_flag = false;
        m_wake_up_flag = false;

        m_waiting_object_num = 0;
    }

    ~ExynosVisionEvent()
    {

    }

    void clearEvent(void)
    {
        Mutex::Autolock lock(m_event_mutex);
        m_event_done_flag = false;
    }

    void setEvent(void)
    {
        Mutex::Autolock lock(m_event_mutex);
        m_event_done_flag = true;
        if (m_waiting_object_num) {
            m_event_cond.signal();
        }
    }

    event_exception_t waitEvent(uint64_t wait_time = 0)
    {
        event_exception_t exception_state;
        status_t ret;

        m_event_mutex.lock();

        if (m_event_done_flag == false) {
            m_waiting_object_num++;

            if (m_waitTime)
                ret = m_event_cond.waitRelative(m_event_mutex, wait_time ? wait_time : m_waitTime);
            else
                ret = m_event_cond.wait(m_event_mutex);
            m_waiting_object_num--;

            if (ret < 0) {
                    ALOGE("ERR(%s):Fail to wait event, ret:%d", __FUNCTION__, ret);

                m_event_mutex.unlock();
                return EVENT_EXCEPTION_TIME_OUT;
            }

            if (m_wake_up_flag == true) {
                if (m_waiting_object_num == 0)
                    m_wake_up_flag = false;

                m_event_mutex.unlock();
                return EVENT_EXCEPTION_WAKE_UP;
            }

            if (m_event_done_flag != true) {
                ALOGE("ERR(%s):Unexpected event state", __FUNCTION__);

                m_event_mutex.unlock();
                return EVENT_EXCEPTION_UNKNOWN;
            }
        } else {
            ALOGD("already event done");
        }

        m_event_mutex.unlock();
        return EVENT_EXCEPTION_NONE;
    }

    void wakeupPendingThread(void)
    {
        Mutex::Autolock lock(m_event_mutex);

        if (m_waiting_object_num) {
            m_wake_up_flag = true;
            m_event_cond.broadcast();
        }
    }

    /* release both Queue */
    void release(void)
    {
        wakeupPendingThread();
    };

};

}; // namespace android
#endif
