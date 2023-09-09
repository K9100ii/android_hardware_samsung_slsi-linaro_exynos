/*
 * Copyright 2017, Samsung Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed toggle an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*!
 * \file      ExynosCameraList.h
 * \brief     hearder file for CAMERA HAL MODULE
 * \author    Hyeonmyeong Choi(hyeon.choi@samsung.com)
 * \date      2013/03/05
 *
 */

#ifndef EXYNOS_CAMERA_LIST_H__
#define EXYNOS_CAMERA_LIST_H__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <utils/threads.h>

#include <utils/RefBase.h>
#include <utils/String8.h>
#include <utils/List.h>
#include "cutils/properties.h"

#define THREAD_NAME_DEFAULT "ExynosList%d"
#define WAIT_TIME (150 * 1000000)
#define DEFAULT_PROCESSQ_MARGIN (1)

using namespace android;

enum LIST_CMD {
    WAKE_UP = 1,
};

template<typename T>
class ExynosCameraList {
public:
    typedef List<T> listType;
    typedef typename listType::iterator iterator;

    ExynosCameraList(uint32_t processQMargin = DEFAULT_PROCESSQ_MARGIN)
    {
        m_statusException = NO_ERROR;
        m_waitProcessQ = false;
        m_waitTime = WAIT_TIME;
        m_thread = NULL;
        m_processQMargin = processQMargin;
    }

    ExynosCameraList(sp<Thread> thread, uint32_t processQMargin = DEFAULT_PROCESSQ_MARGIN)
    {
        m_statusException = NO_ERROR;
        m_waitProcessQ = false;
        m_waitTime = WAIT_TIME;
        m_thread = NULL;

        m_thread = thread;
        m_processQMargin = processQMargin;
    }

    ~ExynosCameraList()
    {
        release();
    }

    void setName(const char* name, ...)
    {
        if (name != NULL) {
            va_list params;

            m_name.clear();
            va_start(params, name);
            m_name.appendFormatV(name, params);
            va_end(params);
        } else {
            ALOGE("name is NULL");
        }
    }

    void setup(sp<Thread> thread)
    {
        m_processQMutex.lock();
        m_thread = thread;
        m_processQMutex.unlock();
    }

    void wakeupAll(void)
    {
        setStatusException(TIMED_OUT);

        Mutex::Autolock lock(m_processQMutex);
        if (m_waitProcessQ)
            m_processQCondition.signal();
    }

    void sendCmd(uint32_t cmd)
    {
        switch (cmd) {
        case WAKE_UP:
            wakeupAll();
            break;
        default:
            ALOGE("ERR(%s):unknown cmd(%d)", __FUNCTION__, cmd);
            break;
        }
    }

    void setStatusException(status_t exception)
    {
        Mutex::Autolock lock(m_flagMutex);
        m_statusException = exception;
    }

    status_t getStatusException(void)
    {
        Mutex::Autolock lock(m_flagMutex);
        return m_statusException;
    }

    /* Process Queue */
    void pushProcessQ(T *buf)
    {
        status_t ret = NO_ERROR;
        int retryCount = 3;
        bool retryFlag = false;

        if (buf == NULL) {
            ALOGW("WARN(%s[%d]):Input buf is NULL", __FUNCTION__, __LINE__);
            return;
        }

        Mutex::Autolock lock(m_processQMutex);
        m_processQ.push_back(*buf);

        if (m_waitProcessQ && m_processQ.size() >= m_processQMargin) {
            m_processQCondition.signal();
        } else if (m_thread != NULL && m_thread->isRunning() == false && m_processQ.size() >= m_processQMargin) {
            do {
                if (m_name.empty())
                    setName(THREAD_NAME_DEFAULT, gettid());

                ret = m_thread->run(m_name.c_str());
                switch (ret) {
                    case INVALID_OPERATION:
                        /* Already running */
                        ALOGW("WARN(%s[%d]):[TID %d]Failed to run thread. Already running.",
                                __FUNCTION__, __LINE__, m_thread->getTid());

                        retryFlag = false;
                        break;
                    case UNKNOWN_ERROR:
                        /* Failed to run thread */
                        ALOGE("ERR(%s[%d]):[TID %d]Failed to run Thread. Unknown error. Retry. RemainCount %d",
                                __FUNCTION__, __LINE__, m_thread->getTid(), retryCount);

                        retryFlag = true;
                        break;
                    default:
                        /* Success to run thread */
                        ALOGV("DEBUG(%s[%d]):[TID %d]Success to run thread",
                                __FUNCTION__, __LINE__, m_thread->getTid());

                        retryFlag = false;
                        break;
                }
            } while (retryFlag == true && retryCount-- > 0);
        }
    };

    status_t popProcessQ(T *buf)
    {
        iterator r;

        Mutex::Autolock lock(m_processQMutex);
        if (m_processQ.empty())
            return TIMED_OUT;

        r = m_processQ.begin()++;
        *buf = *r;
        m_processQ.erase(r);

        return OK;
    };

    status_t waitAndPopProcessQ(T *buf)
    {
        iterator r;

        status_t ret;
        m_processQMutex.lock();
        if (m_processQ.size() < m_processQMargin) {
            m_waitProcessQ = true;

            setStatusException(NO_ERROR);
            ret = m_processQCondition.waitRelative(m_processQMutex, m_waitTime);
            m_waitProcessQ = false;

            if (ret < 0) {
                if (ret == TIMED_OUT)
                    ALOGV("DEBUG(%s):Time out, Skip to pop process Q", __FUNCTION__);
                else
                    ALOGE("ERR(%s):Fail to pop processQ", __FUNCTION__);

                m_processQMutex.unlock();
                return ret;
            }

            ret = getStatusException();
            if (ret != NO_ERROR) {
                if (ret == TIMED_OUT) {
                    ALOGV("DEBUG(%s):return CAM_ECANCELED.(%d).", __FUNCTION__, ret);
                } else {
                    ALOGW("WARN(%s[%d]): Exception status(%d)", __FUNCTION__, __LINE__, ret);
                }
                m_processQMutex.unlock();
                return ret;
            }
        }

        if (m_processQ.empty()) {
            ALOGE("ERR(%s[%d]): processQ is empty, invalid state", __FUNCTION__, __LINE__);
            m_processQMutex.unlock();
            return INVALID_OPERATION;
        }

        r = m_processQ.begin();
        *buf = *r;
        m_processQ.erase(r);

        m_processQMutex.unlock();
        return OK;
    };

    int getSizeOfProcessQ(void)
    {
        Mutex::Autolock lock(m_processQMutex);
        return m_processQ.size();
    };

    /* release both Queue */
    void release(void)
    {
        setStatusException(TIMED_OUT);

        m_processQMutex.lock();
        if (m_waitProcessQ)
            m_processQCondition.signal();

        if (m_processQ.size() > 0) {
            ALOGD("DEBUG(%s):Remained item %zu will be deleted",
                    __FUNCTION__, m_processQ.size());
        }

        m_processQ.clear();
        m_processQMutex.unlock();
    };

    void setWaitTime(uint64_t waitTime)
    {
        m_waitTime = waitTime;
        ALOGV("DEBUG(%s):m_waitTime : %ju", __FUNCTION__, m_waitTime);
    }

    bool isWaiting(void) {
        Mutex::Autolock lock(m_processQMutex);
        return m_waitProcessQ;
    }

    /* for all element control in loop */
    void rawLockProcessList(void) {
        m_processQMutex.lock();
    }

    /* for all element control in loop */
    void rawUnLockProcessList(void) {
        m_processQMutex.unlock();
    }

    /* for all element control in loop */
    List<T> *getRawProcessList(void) {
        return &m_processQ;
    }

private:
    List<T>             m_processQ;
    Mutex               m_processQMutex;
    Mutex               m_flagMutex;
    mutable Condition   m_processQCondition;
    bool                m_waitProcessQ;
    status_t            m_statusException;
    uint64_t            m_waitTime;

    uint32_t            m_processQMargin;

    String8             m_name;
    sp<Thread>          m_thread;
};
#endif
