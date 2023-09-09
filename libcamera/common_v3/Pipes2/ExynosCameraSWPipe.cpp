/*
**
** Copyright 2013, Samsung Electronics Co. LTD
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
#define LOG_TAG "ExynosCameraSWPipe"
#include <cutils/log.h>

#include "ExynosCameraSWPipe.h"

namespace android {

ExynosCameraSWPipe::~ExynosCameraSWPipe()
{
}

status_t ExynosCameraSWPipe::create(int32_t *sensorIds)
{
    m_mainNode = NULL;
    char SuperClassName[EXYNOS_CAMERA_NAME_STR_SIZE] = {0,};

    snprintf(SuperClassName, sizeof(SuperClassName), "%s_SWThread", m_name);

    m_mainThread = ExynosCameraThreadFactory::createThread(this, &ExynosCameraSWPipe::m_mainThreadFunc, SuperClassName);

    m_inputFrameQ = new frame_queue_t(m_mainThread);

    CLOGI("create() is succeed (%d)", getPipeId());

    return NO_ERROR;

}

status_t ExynosCameraSWPipe::release(void)
{
    if (m_inputFrameQ != NULL) {
        m_inputFrameQ->release();
        delete m_inputFrameQ;
        m_inputFrameQ = NULL;
    }

    CLOGI("destroy() is succeed (%d)", getPipeId());

    return NO_ERROR;
}

status_t ExynosCameraSWPipe::start(void)
{
    CLOGD("");

    return NO_ERROR;
}

status_t ExynosCameraSWPipe::stop(void)
{
    CLOGD("");
    int ret = 0;

    m_mainThread->requestExitAndWait();

    CLOGD("thead exited");

    m_inputFrameQ->release();

    return NO_ERROR;
}

status_t ExynosCameraSWPipe::startThread(void)
{
    CLOGV("");

    if (m_outputFrameQ == NULL) {
        CLOGE("outputFrameQ is NULL, cannot start");
        return INVALID_OPERATION;
    }

    m_mainThread->run(m_name);

    CLOGI("startThread is succeed (%d)", getPipeId());

    return NO_ERROR;
}

bool ExynosCameraSWPipe::m_mainThreadFunc(void)
{
    int ret = 0;

    ret = m_run();
    if (ret < 0) {
        if (ret != TIMED_OUT)
            CLOGE("m_putBuffer fail");
    }

    return m_checkThreadLoop();
}

}; /* namespace android */

