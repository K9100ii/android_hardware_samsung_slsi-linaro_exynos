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

#ifndef EXYNOS_VISION_QUEUE_H
#define EXYNOS_VISION_QUEUE_H

#include <utils/threads.h>
#include <utils/Mutex.h>
#include <utils/List.h>

#define DEBUGQ
//#define DEBUGQ  ALOGD

namespace android {

typedef enum _queue_exception_t {
    QUEUE_EXCEPTION_NONE = 1,
    QUEUE_EXCEPTION_TIME_OUT,
    QUEUE_EXCEPTION_WAKE_UP,
    QUEUE_EXCEPTION_DISABLE,
    QUEUE_EXCEPTION_UNKNOWN
} queue_exception_t;

template<typename T>
class ExynosVisionQueue {
private:
    List<T>             m_processQ;
    Mutex               m_processQMutex;
    mutable Condition   m_processQCondition;
    bool                m_waitProcessQ;
    uint64_t            m_waitTime;

    bool                m_queue_enable;
    bool                m_wake_up_flag;

public:

private:

public:
    typedef List<T> listType;
    typedef typename listType::iterator iterator;

    ExynosVisionQueue(uint64_t wait_time)
    {
        m_waitProcessQ = false;
        m_waitTime = wait_time;

        m_queue_enable = true;
        m_wake_up_flag = false;
    }

    ~ExynosVisionQueue()
    {
        release();
    }

    void wakeupPendingThread(void)
    {
        m_processQMutex.lock();

        if (m_waitProcessQ) {
            m_wake_up_flag = true;
            m_processQCondition.signal();
        }

        m_processQMutex.unlock();
    }

    void wakeupPendingThreadAndQDisable(void)
    {
        m_processQMutex.lock();

        m_queue_enable = false;
        if (m_waitProcessQ) {
            m_wake_up_flag = true;
            m_processQCondition.signal();
        }

        m_processQMutex.unlock();
    }

    /* Process Queue */
    void pushProcessQ(T *buf)
    {
        Mutex::Autolock lock(m_processQMutex);
        DEBUGQ("[Q][%s]", __FUNCTION__);

        m_processQ.push_back(*buf);

        if (m_waitProcessQ)
            m_processQCondition.signal();
    };

    status_t popProcessQ(T *buf)
    {
        iterator r;

        Mutex::Autolock lock(m_processQMutex);
        DEBUGQ("[Q][%s]", __FUNCTION__);

        if (m_processQ.empty())
            return TIMED_OUT;

        r = m_processQ.begin()++;
        *buf = *r;
        m_processQ.erase(r);

        return OK;
    };

    queue_exception_t waitAndPopProcessQ(T *buf)
    {
        status_t ret;

        m_processQMutex.lock();
        DEBUGQ("[Q][%s]", __FUNCTION__);

        if (m_queue_enable == false) {
            m_processQMutex.unlock();
            return QUEUE_EXCEPTION_DISABLE;
        }

        if (m_processQ.empty()) {
            m_waitProcessQ = true;

            if (m_waitTime)
                ret = m_processQCondition.waitRelative(m_processQMutex, m_waitTime);
            else
                ret = m_processQCondition.wait(m_processQMutex);
            m_waitProcessQ = false;

            if (ret < 0) {
                if (ret == TIMED_OUT) {
                    ALOGD("DEBUG(%s):Time out, Skip to pop process Q", __FUNCTION__);
                }
                else {
                    ALOGE("ERR(%s):Fail to pop processQ", __FUNCTION__);
                }

                m_processQMutex.unlock();
                return QUEUE_EXCEPTION_TIME_OUT;
            }

            if (m_wake_up_flag == true) {
                m_wake_up_flag = false;

                if (m_queue_enable == false) {
                    m_processQMutex.unlock();
                    return QUEUE_EXCEPTION_DISABLE;
                } else {
                    m_processQMutex.unlock();
                    return QUEUE_EXCEPTION_WAKE_UP;
                }
            }
        }

        if (m_processQ.empty()) {
            ALOGE("ERR(%s[%d]): processQ is empty, invalid state", __FUNCTION__, __LINE__);
            m_processQMutex.unlock();
            return QUEUE_EXCEPTION_UNKNOWN;
        }

        iterator r;
        r = m_processQ.begin();
        *buf = *r;
        m_processQ.erase(r);

        m_processQMutex.unlock();
        return QUEUE_EXCEPTION_NONE;
    };

    uint32_t getRemainedMsgNum(void)
    {
        Mutex::Autolock lock(m_processQMutex);
        return m_processQ.size();
    }

    /* release both Queue */
    void release(void)
    {
        m_processQMutex.lock();

        DEBUGQ("[Q][%s]", __FUNCTION__);

        m_queue_enable = false;

        if (m_waitProcessQ)
            m_processQCondition.signal();

        m_processQ.clear();
        m_processQMutex.unlock();
    };
};

}; // namespace android
#endif
