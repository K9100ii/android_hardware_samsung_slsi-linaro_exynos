/*
**
** Copyright 2014, Samsung Electronics Co. LTD
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

/* #define LOG_NDEBUG 0 */
#define LOG_TAG "ExynosCameraFrameManager"
#include <log/log.h>

#include "ExynosCameraFrameManager.h"

namespace android {

#define CREATE_WORKER_DEFAULT_MARGIN_MAX 200
#define CREATE_WORKER_DEFAULT_MARGIN_MIN 100

#define DELETE_WORKER_DEFAULT_MARGIN_MAX 200

#define RUN_THREAD_TIMEOUT (5000000000L) /* 5 sec */

FrameWorker::FrameWorker(const char* name, int cameraid, FRAMEMGR_OPER::MODE operMode)
{
    m_cameraId = cameraid;
    strncpy(m_name, name, EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_operMode = operMode;
    m_init();
}

FrameWorker::~FrameWorker()
{
    m_deinit();
}

status_t FrameWorker::m_init()
{
    m_key = 0;
    m_enable = false;
    return 0;
}

status_t FrameWorker::m_deinit()
{
    return 0;
}

uint32_t FrameWorker::getKey()
{
    return m_key;
}

status_t FrameWorker::setKey(uint32_t key)
{
    m_key = key;
    return 0;
}

status_t FrameWorker::dump()
{
    CLOGE(" do not support dump function.");
    return NO_ERROR;
}

frame_key_queue_t* FrameWorker::getQueue()
{
    CLOGE(" do not support getQueue function.");
    return NULL;
}

status_t FrameWorker::m_setEnable(bool enable)
{
    Mutex::Autolock lock(m_enableLock);
    status_t ret = FRAMEMGR_ERRCODE::OK;
    m_enable = enable;
    return ret;
}

bool FrameWorker::m_getEnable()
{
    Mutex::Autolock lock(m_enableLock);
    return m_enable;
}

status_t FrameWorker::m_setMargin(int32_t maxMargin, int32_t minMargin)
{
    int32_t ret = FRAMEMGR_ERRCODE::OK;
    int32_t oldMin = m_nMinMargin;
    int32_t oldMax = m_nMaxMargin;

    if (minMargin <= 0)
        m_nMinMargin = CREATE_WORKER_DEFAULT_MARGIN_MIN;
    else
        m_nMinMargin = minMargin;

    if (maxMargin <= 0)
        m_nMaxMargin = CREATE_WORKER_DEFAULT_MARGIN_MAX;
    else
        m_nMaxMargin = maxMargin;

    if ((oldMin != m_nMinMargin) || (oldMax != m_nMaxMargin)) {
        CLOGD("margin is updated, margin[MAX/MIN]=(%d x %d) -> (%d x %d)", oldMax, oldMin, m_nMaxMargin, m_nMinMargin);
    }

    return ret;
}

int32_t FrameWorker::m_getMargin(FrameMargin marginType)
{
    int32_t ret = 0;

    switch(marginType) {
    case FRAME_MARGIN_MIN:
        ret = m_nMinMargin;
        break;
    case FRAME_MARGIN_MAX:
        ret = m_nMaxMargin;
        break;
    }

    return ret;
}

CreateWorker::CreateWorker(const char* name,
                            int cameraid,
                            FRAMEMGR_OPER::MODE operMode,
                            int32_t maxMargin,
                            int32_t minMargin):FrameWorker(name, cameraid, operMode)
{
    CLOGD(" Worker CREATE mode(%d) min_margin(%d) max_margin(%d)  ",
            operMode, minMargin, maxMargin);
    m_init();
    m_setMargin(maxMargin, minMargin);
}
CreateWorker::~CreateWorker()
{
    CLOGD(" Worker DELETE mode(%d) ", m_operMode);
    m_deinit();
}

status_t CreateWorker::m_deinit()
{
    int32_t ret = FRAMEMGR_ERRCODE::OK;
    ExynosCameraFrameSP_sptr_t frame = NULL;

    switch (m_operMode) {
    case FRAMEMGR_OPER::ONDEMAND:
        m_setEnable(false);
        if (m_worklist != NULL) {
            delete m_worklist;
            m_worklist = NULL;
        }

        if (m_lock != NULL) {
            delete m_lock;
            m_lock = NULL;
        }

        break;
    case FRAMEMGR_OPER::SLIENT:
        m_setEnable(false);
        m_command.sendCommand(FrameWorkerCommand::STOP);
        m_thread->requestExitAndWait();
        m_command.cmdQ.release();
        m_setMargin(0, 0);
        if (m_worklist != NULL) {
            while (m_worklist->getSizeOfProcessQ() > 0) {
                m_worklist->popProcessQ(&frame);
                frame = NULL;
            }

            delete m_worklist;
            m_worklist = NULL;
        }

        if (m_lock != NULL) {
            delete m_lock;
            m_lock = NULL;
        }

        break;
    case FRAMEMGR_OPER::NONE:
    default:
        CLOGE(" operMode is invalid operMode(%d)", m_operMode);
        break;
    }

    return ret;
}

Mutex* CreateWorker::getLock()
{
    return m_lock;
}

status_t CreateWorker::execute(__unused ExynosCameraFrameSP_sptr_t inframe, ExynosCameraFrameSP_dptr_t outframe)
{
    int32_t ret = FRAMEMGR_ERRCODE::OK;
    if (m_getEnable() == false) {
        CLOGE(" invalid state, Need to start Worker before execute");
        ret = FRAMEMGR_ERRCODE::ERR;
        return ret;
    }

    outframe = m_execute();
    if (outframe == NULL) {
        CLOGE(" m_execute is invalid (outframe = NULL)");
        ret = FRAMEMGR_ERRCODE::ERR;
    }
    return ret;
}

status_t CreateWorker::setMargin(int32_t max, int32_t min)
{
    int32_t ret = FRAMEMGR_ERRCODE::OK;

    if (m_getEnable() == true) {
        CLOGE("invalid state, Need to stop Worker before update Info(%d %d)", min, max);
        ret = FRAMEMGR_ERRCODE::ERR;
        return ret;
    }

    m_setMargin(max, min);

    return ret;
}

status_t CreateWorker::start()
{
    if (m_worklist->getSizeOfProcessQ() > 0) {
        CLOGD("previous worklist size(%d)",
                 m_worklist->getSizeOfProcessQ());
        m_worklist->release();
    }

    switch (m_operMode) {
    case FRAMEMGR_OPER::ONDEMAND:
        m_setEnable(true);
        break;
    case FRAMEMGR_OPER::SLIENT:
        m_setEnable(true);
        m_command.sendCommand(FrameWorkerCommand::START);

        if (m_thread->isRunning() == false) {
            m_command.sendCommand(FrameWorkerCommand::START);
        }
        break;
    case FRAMEMGR_OPER::NONE:
    default:
        CLOGE(" operMode is invalid operMode(%d)", m_operMode);
        break;
    }
    return 0;
}

status_t CreateWorker::stop()
{
    ExynosCameraFrameSP_sptr_t frame = NULL;

    switch (m_operMode) {
    case FRAMEMGR_OPER::ONDEMAND:
        m_setEnable(false);
        break;
    case FRAMEMGR_OPER::SLIENT:
        m_setEnable(false);
        m_command.sendCommand(FrameWorkerCommand::STOP);
        m_thread->requestExitAndWait();
        CLOGD(" worker stopped remove ths remained frames(%d)",
                 m_worklist->getSizeOfProcessQ());

        while (m_worklist->getSizeOfProcessQ() > 0) {
            m_worklist->popProcessQ(&frame);
            frame = NULL;
        }
        break;
    case FRAMEMGR_OPER::NONE:
    default:
        CLOGE(" operMode is invalid operMode(%d)", m_operMode);
        break;
    }

    return 0;
}

ExynosCameraFrameSP_sptr_t CreateWorker::m_execute()
{
    ExynosCameraFrameSP_sptr_t frame = NULL;
    int32_t ret = NO_ERROR;

    switch (m_operMode) {
    case FRAMEMGR_OPER::ONDEMAND:
        frame = new ExynosCameraFrame(m_cameraId);
        break;
    case FRAMEMGR_OPER::SLIENT:
        m_worklist->popProcessQ(&frame);
        if (frame == NULL) {
            m_command.sendCommand(FrameWorkerCommand::START);
            m_worklist->waitAndPopProcessQ(&frame);
        }

        if (frame == NULL) {
            CLOGE("waitAndPopProcessQ failed, processQ size (%d) Thread return (%d) Status (%d)", m_worklist->getSizeOfProcessQ(), ret, m_thread->isRunning());
        }

        if (m_worklist->getSizeOfProcessQ() <= m_getMargin(FRAME_MARGIN_MIN))
            m_command.sendCommand(FrameWorkerCommand::START);

        break;
    case FRAMEMGR_OPER::NONE:
    default:
        CLOGE(" operMode is invalid operMode(%d)", m_operMode);
        break;
    }
    return frame;
}

status_t CreateWorker::m_init()
{
    int32_t ret = FRAMEMGR_ERRCODE::OK;

    switch (m_operMode) {
    case FRAMEMGR_OPER::ONDEMAND:
        m_worklist = new frame_manager_queue_t;
        m_lock = new Mutex();
        break;
    case FRAMEMGR_OPER::SLIENT:
        m_thread =  new FrameManagerThread(this,
                                        static_cast<func_ptr_t_>(&CreateWorker::workerMain),
                                        "Create Frame Thread",
                                        PRIORITY_URGENT_DISPLAY);
        m_worklist = new frame_manager_queue_t;
        m_lock = new Mutex();

        m_command.cmdQ.setWaitTime(RUN_THREAD_TIMEOUT);
        m_command.cmdQ.setup(m_thread);
        break;
    case FRAMEMGR_OPER::NONE:
    default:
        m_worklist = NULL;
        m_lock = NULL;
        CLOGE(" operMode is invalid operMode(%d)", m_operMode);
        ret = FRAMEMGR_ERRCODE::ERR;
        break;
    }

    return ret;
}

bool CreateWorker::workerMain()
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif
    int32_t ret = FRAMEMGR_ERRCODE::OK;
    bool loop = true;
    ExynosCameraFrameSP_sptr_t frame = NULL;
    uint32_t cmd = 0;
#if defined EXYNOS_CAMERA_FRAME_CREATE_PERFORMANCE
    uint32_t count = 0;
#endif

    if ((ret = m_command.waitCommand(&cmd)) < 0) {
        if (ret == TIMED_OUT)
            CLOGD("waiting time is expired");
        else
            CLOGE("wait and pop fail, restart, ret(%s(%d))", "ERROR", ret);

        return loop;
    }

#if defined EXYNOS_CAMERA_FRAME_CREATE_PERFORMANCE
    count = 0;
    m_createTimer.start();
#endif
    while ((m_getEnable() == true) && (cmd == FrameWorkerCommand::START)
        && (m_worklist->getSizeOfProcessQ() < m_getMargin(FRAME_MARGIN_MAX))) {
        frame = NULL;
        frame = new ExynosCameraFrame(m_cameraId);
        m_worklist->pushProcessQ(&frame);
#if defined EXYNOS_CAMERA_FRAME_CREATE_PERFORMANCE
        count++;
#endif
    }

#if defined EXYNOS_CAMERA_FRAME_CREATE_PERFORMANCE
	m_createTimer.stop();
    /* check performance frame create, total generated frame count * 1ms */
    if ((count > 0) && (m_createTimer.durationMsecs() > count))
        CLOGE("Total generated frameCount(%d) duration(%ju msec)", count, m_createTimer.durationMsecs());
#endif

    if ((m_getEnable() == false) && (cmd == FrameWorkerCommand::STOP)) {
        loop = false;
        CLOGD(" Create worker stopped delete current frame(%d)",
             m_worklist->getSizeOfProcessQ());

        m_worklist->release();
    }

    return loop;
}

DeleteWorker::DeleteWorker(const char* name,
                            int cameraid,
                            FRAMEMGR_OPER::MODE operMode,
                            int32_t maxMargin):FrameWorker(name, cameraid, operMode)
{
    CLOGD(" Worker CREATE mode(%d) ", operMode);
    m_operMode = operMode;
    m_setMargin(maxMargin, 0);
    m_init();
}

DeleteWorker::~DeleteWorker()
{
    CLOGD(" Worker DELETE mode(%d) ", m_operMode);
    m_deinit();
}

status_t DeleteWorker::m_deinit()
{
    int32_t ret = FRAMEMGR_ERRCODE::OK;
    ExynosCameraFrameSP_sptr_t frame = NULL;

    switch (m_operMode) {
    case FRAMEMGR_OPER::ONDEMAND:
        m_setEnable(false);
        if (m_worklist != NULL) {
            delete m_worklist;
            m_worklist = NULL;
        }

        if (m_lock != NULL) {
            delete m_lock;
            m_lock = NULL;
        }
        break;
    case FRAMEMGR_OPER::SLIENT:
        m_setEnable(false);
        m_command.sendCommand(FrameWorkerCommand::STOP);
        m_thread->requestExitAndWait();
        m_command.cmdQ.release();
        m_setMargin(0, 0);
        if (m_worklist != NULL) {

            m_worklist->release();

            delete m_worklist;
            m_worklist = NULL;
        }

        if (m_lock != NULL) {
            delete m_lock;
            m_lock = NULL;
        }
        break;
    case FRAMEMGR_OPER::NONE:
    default:
        CLOGE(" operMode is invalid operMode(%d)", m_operMode);
        break;
    }

    return ret;
}

Mutex* DeleteWorker::getLock()
{
    return m_lock;
}

status_t DeleteWorker::execute(ExynosCameraFrameSP_sptr_t inframe, __unused ExynosCameraFrameSP_dptr_t outframe)
{
    status_t ret = FRAMEMGR_ERRCODE::OK;
    if (m_getEnable() == false) {
        CLOGE(" invalid state, Need to start Worker before execute");
        ret = FRAMEMGR_ERRCODE::ERR;
        return ret;
    }

    ret = m_execute(inframe);
    if (ret < 0) {
        CLOGE(" m_execute is invalid ret(%d)", ret);
        ret = FRAMEMGR_ERRCODE::ERR;
    }

    return ret;
}

status_t DeleteWorker::start()
{
    if (m_worklist->getSizeOfProcessQ() > 0) {
        CLOGD("previous worklist size(%d), clear worklist",
                 m_worklist->getSizeOfProcessQ());
        m_worklist->release();
    }

    switch (m_operMode) {
    case FRAMEMGR_OPER::ONDEMAND:
        m_setEnable(true);
        break;
    case FRAMEMGR_OPER::SLIENT:
        m_setEnable(true);
        m_command.sendCommand(FrameWorkerCommand::START);

        if (m_thread->isRunning() == false)
            m_command.sendCommand(FrameWorkerCommand::START);
        break;
    case FRAMEMGR_OPER::NONE:
    default:
        CLOGE(" operMode is invalid operMode(%d)", m_operMode);
        break;
    }
    return 0;
}

status_t DeleteWorker::stop()
{
    ExynosCameraFrameSP_sptr_t frame = NULL;

    switch (m_operMode) {
    case FRAMEMGR_OPER::ONDEMAND:
        m_setEnable(false);
        break;
    case FRAMEMGR_OPER::SLIENT:
        m_setEnable(false);
        m_command.sendCommand(FrameWorkerCommand::STOP);
        m_thread->requestExitAndWait();
        CLOGD(" worker stopped remove ths remained frames(%d)",
             m_worklist->getSizeOfProcessQ());

        m_worklist->release();
        break;
    case FRAMEMGR_OPER::NONE:
    default:
        CLOGE(" operMode is invalid operMode(%d)", m_operMode);
        break;
    }

    return 0;
}

status_t DeleteWorker::m_setMargin(int32_t maxMargin, int32_t minMargin)
{
    int32_t ret = FRAMEMGR_ERRCODE::OK;

    m_nMinMargin = minMargin;

    if (maxMargin <= 0)
        m_nMaxMargin = DELETE_WORKER_DEFAULT_MARGIN_MAX;

    return ret;
}

status_t DeleteWorker::m_execute(ExynosCameraFrameSP_sptr_t frame)
{
    int32_t ret = FRAMEMGR_ERRCODE::OK;

    switch (m_operMode) {
    case FRAMEMGR_OPER::ONDEMAND:
        frame = NULL;
        break;
    case FRAMEMGR_OPER::SLIENT:
        m_worklist->pushProcessQ(&frame);
        if (m_worklist->getSizeOfProcessQ() > m_getMargin(FRAME_MARGIN_MAX))
            m_command.sendCommand(FrameWorkerCommand::START);
        break;
    case FRAMEMGR_OPER::NONE:
    default:
        ret = FRAMEMGR_ERRCODE::ERR;
        CLOGE(" operMode is invalid operMode(%d)", m_operMode);
        break;
    }
    return ret;
}

status_t DeleteWorker::m_init()
{
    int32_t ret = FRAMEMGR_ERRCODE::OK;

    switch (m_operMode) {
    case FRAMEMGR_OPER::ONDEMAND:
        m_worklist = new frame_manager_queue_t;
        m_lock = new Mutex();
        break;
    case FRAMEMGR_OPER::SLIENT:
        m_thread =  new FrameManagerThread(this,
                            static_cast<func_ptr_t_>(&DeleteWorker::workerMain),
                            "Delete Frame Thread",
                            PRIORITY_URGENT_DISPLAY);
        m_worklist = new frame_manager_queue_t;
        m_lock = new Mutex();

        m_command.cmdQ.setWaitTime(RUN_THREAD_TIMEOUT);
        m_command.cmdQ.setup(m_thread);
        break;
    case FRAMEMGR_OPER::NONE:
    default:
        m_worklist = NULL;
        m_lock = NULL;
        ret = FRAMEMGR_ERRCODE::ERR;
        CLOGE(" operMode is invalid operMode(%d)", m_operMode);
        break;
    }

    return ret;
}

bool DeleteWorker::workerMain()
{
#ifdef DEBUG
        ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif
    int32_t ret = FRAMEMGR_ERRCODE::OK;
    bool loop = true;
    ExynosCameraFrameSP_sptr_t frame = NULL;
    uint32_t cmd = 0;

    if ((ret = m_command.waitCommand(&cmd)) < 0) {
        if (ret == TIMED_OUT)
            CLOGD("waiting time is expired");
        else
            CLOGE("wait and pop fail, restart, ret(%s(%d))", "ERROR", ret);

        return loop;
    }

    m_worklist->release();

    if ((m_getEnable() == false) && (cmd == FrameWorkerCommand::STOP)) {
        loop = false;
    }

    return loop;
}

RunWorker::RunWorker(const char* name,
                           int cameraid,
                           FRAMEMGR_OPER::MODE operMode,
                           int32_t margin,
                           uint32_t dumpMargin):FrameWorker(name, cameraid, operMode)
{
    CLOGD(" Worker CREATE mode(%d) ", operMode);
    m_framekeyQueue = NULL;
    m_numOfMargin = margin;
    m_numOfDumpMargin = dumpMargin;
    m_init();
}

RunWorker::~RunWorker()
{
    CLOGD(" Worker DELETE mode(%d) ", m_operMode);
    m_deinit();
}

status_t RunWorker::m_deinit()
{
    int32_t ret = FRAMEMGR_ERRCODE::OK;
    ExynosCameraFrameSP_sptr_t frame = NULL;

    switch (m_operMode) {
    case FRAMEMGR_OPER::ONDEMAND:
        m_setEnable(false);
        m_worklist.clear();
        if (m_framekeyQueue != NULL) {
            m_framekeyQueue->release();
            delete m_framekeyQueue;
            m_framekeyQueue = NULL;
        }
        break;

    case FRAMEMGR_OPER::SLIENT:
        m_setEnable(false);
        if (m_framekeyQueue != NULL) {
            stopThreadAndInputQ(m_thread, 1, m_framekeyQueue);
            delete m_framekeyQueue;
            m_framekeyQueue = NULL;
        } else {
            m_thread->requestExitAndWait();
        }
        m_worklist.clear();
        break;
    case FRAMEMGR_OPER::NONE:
    default:
        CLOGE(" operMode is invalid operMode(%d)", m_operMode);
        break;
    }

    return ret;
}

Mutex* RunWorker::getLock()
{
    return &m_lock;
}

status_t RunWorker::execute(ExynosCameraFrameSP_sptr_t inframe, __unused ExynosCameraFrameSP_dptr_t outframe)
{
    status_t ret = FRAMEMGR_ERRCODE::OK;

    if (m_getEnable() == false) {
        CLOGE(" invalid state, Need to start Worker before execute");
        ret = FRAMEMGR_ERRCODE::ERR;
        return ret;
    }

    ret = m_execute(inframe);
    if (ret < 0) {
        CLOGE(" m_execute is invalid ret(%d)", ret);
        ret = FRAMEMGR_ERRCODE::ERR;
    }

    return ret;
}

status_t RunWorker::setMargin(int32_t max, int32_t min)
{
    int32_t ret = FRAMEMGR_ERRCODE::OK;

    CLOGD("worker(%d - %d) do not support change margin", min, max);

    return ret;
}

status_t RunWorker::start()
{
    if (m_worklist.size() > 0) {
        CLOGD(" previous worklist size(%d), clear worklist", (int)m_worklist.size());
        m_worklist.clear();
    }
    if (m_framekeyQueue->getSizeOfProcessQ() > 0) {
        CLOGD(" previous framekeyQueue size(%d), clear framekeyQueue", m_framekeyQueue->getSizeOfProcessQ());
        m_framekeyQueue->release();
    }

    switch (m_operMode) {
    case FRAMEMGR_OPER::ONDEMAND:
        m_setEnable(true);
        break;
    case FRAMEMGR_OPER::SLIENT:
        m_setEnable(true);

        if (m_thread->isRunning() == false)
            m_thread->run();
        break;
    case FRAMEMGR_OPER::NONE:
    default:
        CLOGE(" operMode is invalid operMode(%d)", m_operMode);
        break;
    }
    return 0;
}

status_t RunWorker::stop()
{
    ExynosCameraFrameSP_sptr_t frame = NULL;
    uint32_t framekey = 0;
    status_t ret = NO_ERROR;

    switch (m_operMode) {
    case FRAMEMGR_OPER::ONDEMAND:
        m_setEnable(false);
        break;
    case FRAMEMGR_OPER::SLIENT:
        m_setEnable(false);
        m_thread->requestExit();
        m_framekeyQueue->wakeupAll();
        m_thread->requestExitAndWait();
        CLOGD(" worker stopped remove ths remained frames(%d) queue(%d)",
            (int)m_worklist.size(), (int)m_framekeyQueue->getSizeOfProcessQ());

        while (m_framekeyQueue->getSizeOfProcessQ() > 0) {
            framekey = 0;
            ret = m_framekeyQueue->popProcessQ(&framekey);
            if (ret < 0) {
                CLOGE(" wait and pop fail, ret(%d)", ret);
                continue;
            }

            ret = m_removeFrame(framekey, &m_worklist, &m_lock);
            if (ret < 0){
                CLOGE(" m_removeFrame fail, ret(%d)", ret);
                continue;
            }
        }

        break;
    case FRAMEMGR_OPER::NONE:
    default:
        CLOGE(" operMode is invalid operMode(%d)", m_operMode);
        break;
    }

    return NO_ERROR;
}

status_t RunWorker::dump()
{
    Mutex::Autolock lock(m_lock);
    status_t ret = NO_ERROR;
    map<uint32_t, ExynosCameraFrameWP_t>::iterator iter;
    pair<map<uint32_t, ExynosCameraFrameWP_t>::iterator,bool> listRet;
    ExynosCameraFrameWP_t item = NULL;
    ExynosCameraFrameSP_t detect = NULL;

    int frameSize = m_worklist.size();

    if (frameSize == 0) {
        CLOGD(" No memory leak detected m_runningFrameList.size()(%d)", frameSize);
    } else {
        CLOGW(" %d memory leak detected m_runningFrameList.size()(%d)", frameSize, frameSize);
    }

    for (iter = m_worklist.begin() ; iter != m_worklist.end() ;) {
        item = iter->second;
        detect = item.promote();
        if (detect == 0) {
            CLOGW(" promote failed, already delete frame");
        } else {
            CLOGW(" delete memory leak detected FrameKey(%d) HAL-FrameCnt(%d) FrameType(%u) StrongCount(%d)",
                detect->getUniqueKey(), detect->getFrameCount(), detect->getFrameType(), detect->getStrongCount());
            detect.clear();
        }
        m_worklist.erase(iter++);
        item = NULL;
    }

    m_worklist.clear();

    return ret;
}

frame_key_queue_t* RunWorker::getQueue()
{
    if (m_framekeyQueue == NULL) {
        CLOGE(" getQueue fail, Queue == NULL ");
    }

    return m_framekeyQueue;
}

status_t RunWorker::m_execute(ExynosCameraFrameSP_dptr_t frame)
{
    int32_t ret = FRAMEMGR_ERRCODE::OK;

    switch (m_operMode) {
    case FRAMEMGR_OPER::ONDEMAND:
        ret = m_insertFrame(frame, &m_worklist, &m_lock);
        if (ret < 0) {
            CLOGE(" m_insertFrame fail, ret(%d)", ret);
        }
        break;
    case FRAMEMGR_OPER::SLIENT:
        ret = m_insertFrame(frame, &m_worklist, &m_lock);
        if (ret < 0) {
            CLOGE(" m_insertFrame fail, ret(%d)", ret);
        }

        if (m_thread->isRunning() == false){
            m_thread->run();
        }
        break;
    case FRAMEMGR_OPER::NONE:
    default:
        ret = FRAMEMGR_ERRCODE::ERR;
        CLOGE(" operMode is invalid operMode(%d)", m_operMode);
        break;
    }
    return ret;
}

status_t RunWorker::m_init()
{
    int32_t ret = FRAMEMGR_ERRCODE::OK;

    switch (m_operMode) {
    case FRAMEMGR_OPER::ONDEMAND:
        m_worklist.clear();
        m_framekeyQueue = new frame_key_queue_t(m_numOfMargin);
        m_framekeyQueue->setWaitTime(RUN_THREAD_TIMEOUT);
        break;
    case FRAMEMGR_OPER::SLIENT:
        m_thread =  new FrameManagerThread(this,
                            static_cast<func_ptr_t_>(&RunWorker::workerMain),
                            "Run Frame Thread",
                            PRIORITY_URGENT_DISPLAY);
        m_worklist.clear();
        m_framekeyQueue = new frame_key_queue_t(m_numOfMargin);
        m_framekeyQueue->setWaitTime(RUN_THREAD_TIMEOUT);
        break;
    case FRAMEMGR_OPER::NONE:
    default:
        m_worklist.clear();
        m_framekeyQueue = NULL;
        ret = FRAMEMGR_ERRCODE::ERR;
        CLOGE(" operMode is invalid operMode(%d)", m_operMode);
        break;
    }

    return ret;
}

bool RunWorker::workerMain()
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif
    int32_t ret = FRAMEMGR_ERRCODE::OK;
    bool loop = true;
    uint32_t framekey = 0;
    int size = 0;

    if (m_getEnable() == false) {
        loop = false;
        CLOGD(" Run worker stopped delete current frame(%d)", m_framekeyQueue->getSizeOfProcessQ());

        while (m_framekeyQueue->getSizeOfProcessQ() > 0) {
            framekey = 0;
            ret = m_framekeyQueue->popProcessQ(&framekey);
            if (ret < 0) {
                CLOGE(" wait and pop fail, ret(%d)", ret);
                continue;
            }

            ret = m_removeFrame(framekey, &m_worklist, &m_lock);
            if (ret < 0){
                CLOGE(" m_removeFrame fail, ret(%d)", ret);
                continue;
            }
        }
        return loop;
    }

    ret = m_framekeyQueue->waitAndPopProcessQ(&framekey);
    if (ret < 0) {
        CLOGI(" wait and pop fail, ret(%d)", ret);
        goto func_exit;
    }

    ret = m_removeFrame(framekey, &m_worklist, &m_lock);
    if (ret < 0){
        CLOGE(" m_removeFrame fail, ret(%d)", ret);
        goto func_exit;
    }

    size = m_framekeyQueue->getSizeOfProcessQ();

    for (int i = 0 ; i < size ; i++) {
        framekey = 0;
        ret = m_framekeyQueue->popProcessQ(&framekey);
        if (ret < 0) {
            CLOGE(" wait and pop fail, ret(%d)", ret);
            continue;
        }

        ret = m_removeFrame(framekey, &m_worklist, &m_lock);
        if (ret < 0){
            CLOGE(" m_removeFrame fail, ret(%d)", ret);
            continue;
        }
    }

    if (m_getSizeList() > m_numOfDumpMargin){
        m_dumpFrame();
    }

func_exit:

    if (m_framekeyQueue->getSizeOfProcessQ() > 0)
        loop = true;

    return loop;

}

status_t RunWorker::m_insertFrame(ExynosCameraFrameSP_dptr_t frame,
                                  map<uint32_t, ExynosCameraFrameWP_t> *list,
                                  Mutex *lock)
{
    lock->lock();
    pair<map<uint32_t, ExynosCameraFrameWP_t>::iterator,bool> listRet;
    status_t ret = FRAMEMGR_ERRCODE::OK;

    listRet = list->insert( pair<uint32_t, ExynosCameraFrameWP_t>(frame->getUniqueKey(), frame));
    if (listRet.second == false) {
        ret = FRAMEMGR_ERRCODE::ERR;
        CLOGE(" insertFrame fail frame already exist!! HAL frameCnt(%d) UniqueKey (%u) ",
            frame->getFrameCount(), frame->getUniqueKey());
    }
    lock->unlock();
    return ret;
}

status_t RunWorker::m_removeFrame(ExynosCameraFrameSP_dptr_t frame, map<uint32_t, ExynosCameraFrameWP_t> *list, Mutex *lock)
{
    return m_removeFrame(frame->getUniqueKey(), list, lock);
}


status_t RunWorker::m_removeFrame(uint32_t frameKey, map<uint32_t, ExynosCameraFrameWP_t> *list, Mutex *lock)
{
    lock->lock();
    status_t ret = NO_ERROR;
    map<uint32_t, ExynosCameraFrameWP_t>::iterator iter;
    pair<map<uint32_t, ExynosCameraFrameWP_t>::iterator,bool> listRet;
    ExynosCameraFrameWP_t item = NULL;

    iter = list->find(frameKey);
    if (iter != list->end()) {
        item = iter->second;
        list->erase(iter);
    } else {
        ret = FRAMEMGR_ERRCODE::ERR;
        CLOGE(" frame is not EXIST UniqueKey(%d)", frameKey);
        ret = INVALID_OPERATION;
    }
    lock->unlock();

    return ret;
}

status_t RunWorker::m_setMargin(int32_t numOfMargin)
{
    int32_t ret = FRAMEMGR_ERRCODE::OK;
    m_numOfMargin = numOfMargin;
    return ret;
}

int32_t RunWorker::m_getMargin()
{
    return m_numOfMargin;
}

status_t RunWorker::m_dumpFrame()
{
    Mutex::Autolock lock(m_lock);
    status_t ret = NO_ERROR;

    map<uint32_t, ExynosCameraFrameWP_t>::iterator iter;
    ExynosCameraFrameWP_t frame = NULL;
    ExynosCameraFrameSP_sptr_t item = NULL;

    CLOGD("++++++++++++++++++++++++++++++");

    CLOGD("m_numOfDumpMargin(%d) m_runningFrameList.size()(%zu)",
         m_numOfDumpMargin, m_worklist.size());
    int testCount = 0;
    for (iter = m_worklist.begin() ; iter != m_worklist.end() ; ++iter) {
        frame = iter->second;
        item = frame.promote();
        if (item == 0) {
            CLOGW(" promote failed, already delete frame");
        } else {
            CLOGW(" delete memory leak detected FrameKey(%d) HAL-FrameCnt(%d) FrameType(%u) StrongCount(%d)",
                item->getUniqueKey(), item->getFrameCount(), item->getFrameType(), item->getStrongCount());
        }

        if (++testCount >= 5)
            break;
    }

    CLOGD("------------------------------");

#ifdef AVOID_ASSERT_FRAME
        CLOGE(" too many frames - m_runningFrameList.size(%zu)",
                 m_worklist.size());
#else
        android_printAssert(NULL, LOG_TAG, "HACK For ANR DEBUGGING");
#endif

    return ret;
}

uint32_t RunWorker::m_getSizeList()
{
    Mutex::Autolock lock(m_lock);
    return m_worklist.size();
}

ExynosCameraFrameManager::ExynosCameraFrameManager(const char* name,
                                                            int cameraid,
                                                            FRAMEMGR_OPER::MODE operMode)
{
    m_cameraId = cameraid;
    strncpy(m_name, name, EXYNOS_CAMERA_NAME_STR_SIZE - 1);

    CLOGD(" FrameManager CREATE mode(%d)", operMode);

    m_setOperMode(operMode);
    m_init();
}

ExynosCameraFrameManager::~ExynosCameraFrameManager()
{
    CLOGD(" FrameManager DELETE mode(%d)", m_operMode);
    m_deinit();
}

ExynosCameraFrameSP_sptr_t ExynosCameraFrameManager::createFrame(ExynosCameraConfigurations *configurations,
                                                                 uint32_t framecnt,
                                                                 uint32_t frametype)
{
    return m_createFrame(configurations, framecnt, frametype);
}

status_t ExynosCameraFrameManager::setKeybox(sp<KeyBox> keybox)
{
    Mutex::Autolock lock(m_stateLock);
    m_keybox = keybox;
    return 0;
}

sp<KeyBox> ExynosCameraFrameManager::getKeybox()
{
    Mutex::Autolock lock(m_stateLock);
    return m_keybox;
}

status_t ExynosCameraFrameManager::setWorker(int key, sp<FrameWorker> worker)
{
    Mutex::Autolock lock(m_stateLock);
    map<uint32_t, sp<FrameWorker> >::iterator iter;
    pair<map<uint32_t, sp<FrameWorker> >::iterator,bool> workerRet;
    pair<map<uint32_t, Mutex*>::iterator,bool> mutexRet;

    iter = m_workerList.find(key);
    if (iter != m_workerList.end()) {
        CLOGE(" already worker is EXIST(%d)", key);
    } else {
        m_setupWorkerInfo(key, worker);
    }

    return 0;
}

status_t ExynosCameraFrameManager::setOperMode(FRAMEMGR_OPER::MODE mode)
{
   return m_setOperMode(mode);
}

int ExynosCameraFrameManager::getOperMode()
{
    return m_getOperMode();
}

status_t ExynosCameraFrameManager::m_init()
{
    int ret = FRAMEMGR_ERRCODE::OK;

    m_keybox = NULL;
    m_framekeyQueue = NULL;
    m_setEnable(false);
    return ret;
}

status_t ExynosCameraFrameManager::m_setupWorkerInfo(int key, sp<FrameWorker> worker)
{
    int ret = FRAMEMGR_ERRCODE::OK;
    pair<map<uint32_t, sp<FrameWorker> >::iterator,bool> workerRet;
    pair<map<uint32_t, Mutex*>::iterator,bool> mutexRet;

    workerRet = m_workerList.insert( pair<uint32_t, sp<FrameWorker> >(key, worker));
    if (workerRet.second == false) {
        ret = FRAMEMGR_ERRCODE::ERR;
        CLOGE(" work insert failed(%d)", key);
    }
    return ret;
}

sp<FrameWorker> ExynosCameraFrameManager::getWorker(int key)
{
    Mutex::Autolock lock(m_stateLock);
    sp<FrameWorker> ret;
    map<uint32_t, sp<FrameWorker> >::iterator iter;

    if (m_getEnable() == true) {
        CLOGE("invalid state, Need to stop FrameMgr before getWorker(%d)", key);
        return NULL;
    }

    iter = m_workerList.find(key);
    if (iter != m_workerList.end()) {
        ret = iter->second;
    } else {
        CLOGE(" worker is not EXIST(%d)", key);
        ret = NULL;
    }

    return ret;
}

sp<FrameWorker> ExynosCameraFrameManager::eraseWorker(int key)
{
    Mutex::Autolock lock(m_stateLock);
    sp<FrameWorker> ret;
    map<uint32_t, sp<FrameWorker> >::iterator iter;

    iter = m_workerList.find(key);
    if (iter != m_workerList.end()) {
        ret = iter->second;
        m_workerList.erase(iter);
    } else {
        CLOGE(" worker is not EXIST(%d)", key);
        ret = NULL;
    }
    return ret;
}

status_t ExynosCameraFrameManager::start()
{
    Mutex::Autolock lock(m_stateLock);
    status_t ret = FRAMEMGR_ERRCODE::OK;
    sp<FrameWorker> worker = NULL;
    map<uint32_t, sp<FrameWorker> >::iterator iter;

    if (m_getEnable() == true) {
        CLOGD("frameManager already start!!");
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    {
        iter = m_workerList.find(FRAMEMGR_WORKER::RUNNING);
        if (iter == m_workerList.end()) {
            CLOGE("do not find worker(%d) !!", FRAMEMGR_WORKER::RUNNING);
            ret = INVALID_OPERATION;
            goto func_exit;
        }
        worker = iter->second;
        m_framekeyQueue = worker->getQueue();
    }

    for (iter = m_workerList.begin() ; iter != m_workerList.end() ; ++iter) {
        worker = iter->second;
        ret = worker->start();
    }

    m_setEnable(true);

func_exit:
    return ret;
}

status_t ExynosCameraFrameManager::stop()
{
    Mutex::Autolock lock(m_stateLock);
    status_t ret = FRAMEMGR_ERRCODE::OK;
    sp<FrameWorker> worker;

    if (m_getEnable() == false) {
        CLOGD("frameManager already stop!!");
        return ret;
    }

    m_setEnable(false);

    map<uint32_t, sp<FrameWorker> >::iterator iter;
    for (iter = m_workerList.begin() ; iter != m_workerList.end() ; ++iter) {
        worker = iter->second;
        ret = worker->stop();
    }

    return ret;
}

int ExynosCameraFrameManager::deleteAllFrame()
{
    Mutex::Autolock lock(m_stateLock);
    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t frame = NULL;

    if (m_getEnable() == true) {
        CLOGE(" invalid state, module state enabled, state must disabled");
        ret = INVALID_OPERATION;
        return ret;
    }

    sp<FrameWorker> worker = NULL;
    map<uint32_t, sp<FrameWorker> >::iterator iter;
    iter = m_workerList.find(FRAMEMGR_WORKER::RUNNING);
    if (iter == m_workerList.end()) {
        CLOGE("do not find worker(%d) !!", FRAMEMGR_WORKER::RUNNING);
        ret = INVALID_OPERATION;
        goto func_exit;
    }
    worker = iter->second;
    worker->dump();

func_exit:
    return ret;
}

ExynosCameraFrameSP_sptr_t ExynosCameraFrameManager::m_createFrame(ExynosCameraConfigurations *configurations,
                                                                   uint32_t framecnt,
                                                                   uint32_t frametype)
{
    int ret = FRAMEMGR_ERRCODE::OK;
    uint32_t nextKey = 0;
    ExynosCameraFrameSP_sptr_t frame = NULL;
    ExynosCameraFrameSP_sptr_t outframe = NULL;
    map<uint32_t, sp<FrameWorker> >::iterator iter;
    sp<FrameWorker> worker = NULL;
    if (m_getEnable() == false) {
        CLOGE(" module state disabled");
        return frame;
    }

    iter = m_workerList.find(FRAMEMGR_WORKER::CREATE);
    worker = iter->second;

    switch (m_operMode) {
    case FRAMEMGR_OPER::ONDEMAND:
    case FRAMEMGR_OPER::SLIENT:
        worker->execute(NULL, frame);
        if (frame == NULL) {
            CLOGE("Frame is NULL");
            return frame;
        }

        frame->setUniqueKey(m_keybox->createKey());
        nextKey = m_keybox->getKey();
        if (nextKey == frame->getUniqueKey()) {
            CLOGW("[F%d]Failed to increase frameUniqueKey. curKey %d nextKey %d",
                    framecnt, frame->getUniqueKey(), nextKey);

            /* Increase the next frameUniqueKey forcely */
            nextKey += 1;
            m_keybox->setKey(nextKey);
        }
        frame->setFrameInfo(configurations, /*parameters*/NULL, framecnt, frametype);

        frame->setFrameMgrInfo(m_framekeyQueue);

        iter = m_workerList.find(FRAMEMGR_WORKER::RUNNING);
        if (iter != m_workerList.end()) {
            worker = iter->second;
        } else {
            CLOGE(" worker is not EXIST FRAMEMGR_WORKER::RUNNING");
        }

        ret = worker->execute(frame, outframe);
        if (ret < 0) {
            CLOGE("m_insertFrame is Failed ");
            return NULL;
        }
        break;
    }

    return frame;
}

status_t ExynosCameraFrameManager::dump()
{
    status_t ret = FRAMEMGR_ERRCODE::OK;

    return ret;
}

status_t ExynosCameraFrameManager::m_deinit()
{
    status_t ret = FRAMEMGR_ERRCODE::OK;
    ExynosCameraFrameSP_sptr_t frame = NULL;
    sp<FrameWorker> worker;

    map<uint32_t, sp<FrameWorker> >::iterator iter;
    for (iter = m_workerList.begin() ; iter != m_workerList.end() ; ++iter) {
        worker = iter->second;
        m_workerList.erase(iter++);
        worker = NULL;
    }

    m_workerList.clear();

    if (m_keybox != NULL) {
        CLOGD(" delete m_keybox");
        m_keybox = NULL;
    }

    m_framekeyQueue = NULL;

    return ret;
}

status_t ExynosCameraFrameManager::m_setOperMode(FRAMEMGR_OPER::MODE mode)
{
    int32_t ret = FRAMEMGR_ERRCODE::OK;
    switch (mode) {
    case FRAMEMGR_OPER::ONDEMAND:
    case FRAMEMGR_OPER::SLIENT:
        m_operMode = mode;
        break;
    default:
        m_operMode = -1;
        ret = FRAMEMGR_ERRCODE::ERR;
        CLOGE(" operMode is invalid operMode(%d)", m_operMode);
        break;
    }

    return ret;
}

int ExynosCameraFrameManager::m_getOperMode()
{
    return m_operMode;
}

status_t ExynosCameraFrameManager::m_setEnable(bool enable)
{
    Mutex::Autolock lock(m_enableLock);
    status_t ret = FRAMEMGR_ERRCODE::OK;
    m_enable = enable;
    return ret;
}

bool ExynosCameraFrameManager::m_getEnable()
{
    Mutex::Autolock lock(m_enableLock);
    return m_enable;
}




//************FrameManager END *************************//




}; /* namespace android */
