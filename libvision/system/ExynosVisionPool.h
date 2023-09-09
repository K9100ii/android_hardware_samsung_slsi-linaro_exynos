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

#ifndef EXYNOS_VISION_POOL_H
#define EXYNOS_VISION_POOL_H

#include <utils/threads.h>
#include <utils/Mutex.h>
#include <utils/List.h>

#include <VX/vx.h>

#define POOL_WAIT_TIME (150 * 1000000)

namespace android {

typedef enum _pool_exception_t {
    POOL_EXCEPTION_NONE = 1,
    POOL_EXCEPTION_TIME_OUT,
    POOL_EXCEPTION_WAKE_UP,
    POOL_EXCEPTION_UNKNOWN
} pool_exception_t;

template<typename T>
class ExynosVisionPool {
private:
    List<T>             m_pool_list;
    Mutex               m_pool_mutex;
    mutable Condition   m_pool_cond;
    vx_uint32                m_waiting_object_num;
    uint64_t            m_waitTime;

    bool                m_wake_up_flag;

public:
    typedef List<T> listType;
    typedef typename listType::iterator iterator;

    ExynosVisionPool()
    {
        m_waiting_object_num = 0;
        m_waitTime = POOL_WAIT_TIME;
        m_wake_up_flag = false;
    }

    ~ExynosVisionPool()
    {

    }

    void release(void)
    {
        Mutex::Autolock lock(m_pool_mutex);

        if (m_waiting_object_num)
            m_pool_cond.broadcast();
    };

    void flush(void)
    {
        m_pool_list.clear();
    }

    void putResource(T resource)
    {
        Mutex::Autolock lock(m_pool_mutex);
        m_pool_list.push_back(resource);
        if (m_waiting_object_num) {
            ALOGD("[%s]send wake-up signal", __FUNCTION__);
            m_pool_cond.signal();
        }
    }

    pool_exception_t getResource(T *ret_resource)
    {
        status_t ret;

        m_pool_mutex.lock();
        if (m_pool_list.empty()) {
            m_waiting_object_num++;
            ret = m_pool_cond.wait(m_pool_mutex);
            m_waiting_object_num--;

            if (ret < 0) {
                ALOGE("ERR(%s):Fail to get resource, ret:%d", __FUNCTION__, ret);

                m_pool_mutex.unlock();
                return POOL_EXCEPTION_TIME_OUT;
            }

            if (m_wake_up_flag == true) {
                m_wake_up_flag = false;

                m_pool_mutex.unlock();
                return POOL_EXCEPTION_WAKE_UP;
            }
        }

        if (m_pool_list.empty()) {
            ALOGE("ERR(%s[%d]): pool is empty, invalid state", __FUNCTION__, __LINE__);

            m_pool_mutex.unlock();
            return POOL_EXCEPTION_UNKNOWN;
        }

        iterator iter = m_pool_list.begin();
        *ret_resource = *iter;
        m_pool_list.erase(iter);

        m_pool_mutex.unlock();

        return POOL_EXCEPTION_NONE;
    }

    vx_uint32 getFreeNum(void)
    {
        Mutex::Autolock lock(m_pool_mutex);
        return m_pool_list.size();
    }

    void displayInfo(void)
    {
        Mutex::Autolock lock(m_pool_mutex);
        ALOGD("[%s]waiting_num:%d, pool num:%d", __FUNCTION__, m_waiting_object_num, m_pool_list.size());
    }
};

}; // namespace android
#endif
