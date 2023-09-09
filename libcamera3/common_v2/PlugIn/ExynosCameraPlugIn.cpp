/*
 * Copyright@ Samsung Electronics Co. LTD
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

/*#define LOG_NDEBUG 0 */
#define LOG_TAG "ExynosCameraPlugIn"
#include <log/log.h>

#include "ExynosCameraPlugIn.h"

namespace android {

ExynosCameraPlugIn::~ExynosCameraPlugIn()
{
    /* do nothing */
    deinit();
}

/*********************************************/
/*  public functions                      */
/*********************************************/
status_t ExynosCameraPlugIn::init(void)
{
    status_t ret = NO_ERROR;

    ALOGI("%s(%d)", __FUNCTION__, __LINE__);

    Mutex::Autolock lock(m_lock);

    ret = m_init();
    if (ret != NO_ERROR) {
        CLOGE("m_init() fail");
        return INVALID_OPERATION;
    }

    CLOGD("done");

    return ret;
}

status_t ExynosCameraPlugIn::deinit(void)
{
    status_t ret = NO_ERROR;
    status_t funcRet = NO_ERROR;

    CLOGI("");

    Mutex::Autolock lock(m_lock);

    /* state check */
    switch (m_state) {
    case PLUGIN_INIT:
    case PLUGIN_DESTROY:
        /* you can access */
        break;
    default:
        CLOGE("destroy() was skipped!!(%d)", m_state);
        break;
    }

    ret = ExynosCameraPlugIn::m_deinit();
    funcRet |= ret;
    if (ret != NO_ERROR) {
        CLOGE("ExynosCameraPlugIn::m_deinit() fail");
    }

    ret = this->m_deinit();
    funcRet |= ret;
    if (ret != NO_ERROR) {
        CLOGE("this->m_deinit() fail");
    }

    CLOGD("done");

    return funcRet;
}

status_t ExynosCameraPlugIn::create(void)
{
    status_t ret = NO_ERROR;

    CLOGI("pipeId(%d)", m_pipeId);

    Mutex::Autolock lock(m_lock);

    /* state check */
    switch (m_state) {
    case PLUGIN_INIT:
        /* you can access */
        break;
    default:
        CLOGW("It is already created.(%d)", m_state);
        return NO_ERROR;
    }

    ret = ExynosCameraPlugIn::m_create();
    if (ret != NO_ERROR) {
        CLOGE("ExynosCameraPlugIn::m_create() fail");
        return INVALID_OPERATION;
    }

    ret = this->m_create();
    if (ret != NO_ERROR) {
        CLOGE("this->m_create() fail");
        return INVALID_OPERATION;
    }

    CLOGD("done, pipeId(%d)", m_pipeId);

    return ret;
}

status_t ExynosCameraPlugIn::destroy(void)
{
    status_t ret = NO_ERROR;
    status_t funcRet = NO_ERROR;

    Mutex::Autolock lock(m_lock);

    /* state check */
    switch (m_state) {
    case PLUGIN_CREATE:
    case PLUGIN_STOP:
        /* you can access */
        break;
    case PLUGIN_DESTROY:
    case PLUGIN_DEINIT:
        CLOGW("It is already destroyed(%d)", m_state);
        return NO_ERROR;
    default:
        CLOGE("stop() was skipped!!(%d)", m_state);
        break;
    }

    ret = ExynosCameraPlugIn::m_destroy();
    funcRet |= ret;
    if (ret != NO_ERROR) {
        CLOGE("ExynosCameraPlugIn::m_destroy() fail");
    }

    ret = this->m_destroy();
    funcRet |= ret;
    if (ret != NO_ERROR) {
        CLOGE("this->m_destroy() fail");
    }

    CLOGD("done");

    return funcRet;
}

status_t ExynosCameraPlugIn::setup(Map_t *map)
{
    status_t ret = NO_ERROR;

    Mutex::Autolock lock(m_lock);

    /* state check */
    switch (m_state) {
    case PLUGIN_CREATE:
    case PLUGIN_STOP:
        /* you can access */
        break;
    case PLUGIN_SETUP:
    case PLUGIN_START:
    case PLUGIN_PROCESS:
        CLOGW("It is already setuped(%d)", m_state);
        return NO_ERROR;
    default:
        CLOGE("invalid state. so, fail(%d)", m_state);
        return INVALID_OPERATION;
    }

    ret = ExynosCameraPlugIn::m_setup(map);
    if (ret != NO_ERROR) {
        CLOGE("ExynosCameraPlugIn::m_setup() fail");
        return INVALID_OPERATION;
    }

    ret = this->m_setup(map);
    if (ret != NO_ERROR) {
        CLOGE("this->m_setup() fail");
        return INVALID_OPERATION;
    }

    return ret;
}

status_t ExynosCameraPlugIn::start(void)
{
    status_t ret = NO_ERROR;

    Mutex::Autolock lock(m_lock);

    /* state check */
    switch (m_state) {
    case PLUGIN_SETUP:
    case PLUGIN_STOP:
        /* you can access */
        break;
    case PLUGIN_START:
    case PLUGIN_PROCESS:
        CLOGW("It is already started(%d)", m_state);
        return NO_ERROR;
    default:
        CLOGE("invalid state. so, fail(%d)", m_state);
        return INVALID_OPERATION;
    }

    m_state = PLUGIN_START;

    return ret;
}

status_t ExynosCameraPlugIn::stop(void)
{
    status_t ret = NO_ERROR;

    Mutex::Autolock lock(m_lock);

    /* state check */
    switch (m_state) {
    case PLUGIN_SETUP:
    case PLUGIN_START:
    case PLUGIN_PROCESS:
        /* you can access */
        break;
    case PLUGIN_STOP:
    case PLUGIN_DESTROY:
    case PLUGIN_DEINIT:
        CLOGW("It is already stoped(%d)", m_state);
        return NO_ERROR;
    default:
        CLOGE("invalid state. so, fail(%d)", m_state);
        return INVALID_OPERATION;
    }

    m_state = PLUGIN_STOP;

    return ret;
}

status_t ExynosCameraPlugIn::process(Map_t *map)
{
    status_t ret = NO_ERROR;

    Mutex::Autolock lock(m_lock);

    /* state check */
    switch (m_state) {
    case PLUGIN_START:
    case PLUGIN_PROCESS:
        /* you can access */
        break;
    default:
        CLOGE("invalid state. so, fail(%d)", m_state);
        return INVALID_OPERATION;
    }

    ret = ExynosCameraPlugIn::m_process(map);
    if (ret != NO_ERROR) {
        CLOGE("ExynosCameraPlugIn::m_process() fail");
        return INVALID_OPERATION;
    }

    ret = this->m_process(map);
    if (ret != NO_ERROR) {
        CLOGE("this->m_process() fail");
        return INVALID_OPERATION;
    }

    return ret;
}

status_t ExynosCameraPlugIn::setParameter(int key, void *data)
{
    Mutex::Autolock lock(m_lock);

    /* state check */
    switch (m_state) {
    case PLUGIN_INIT:
    case PLUGIN_DESTROY:
    case PLUGIN_DEINIT:
        CLOGE("this plugin was not ready(%d)", m_state);
        return INVALID_OPERATION;
        break;
    default:
        break;
    }

    return this->m_setParameter(key, data);
}

status_t ExynosCameraPlugIn::getParameter(int key, void *data)
{
    Mutex::Autolock lock(m_lock);

    /* state check */
    switch (m_state) {
    case PLUGIN_INIT:
    case PLUGIN_DESTROY:
    case PLUGIN_DEINIT:
        CLOGE("this plugin was not ready(%d)", m_state);
        return INVALID_OPERATION;
        break;
    default:
        break;
    }

    return this->m_getParameter(key, data);
}

void ExynosCameraPlugIn::dump(void)
{
    Mutex::Autolock lock(m_lock);

    /* state check */
    switch (m_state) {
    case PLUGIN_DESTROY:
    case PLUGIN_DEINIT:
        CLOGE("this plugin was not ready(%d)", m_state);
        return;
        break;
    default:
        break;
    }

    ExynosCameraPlugIn::m_dump();

    this->m_dump();
}

status_t ExynosCameraPlugIn::query(Map_t *map)
{
    Mutex::Autolock lock(m_lock);

    /* state check */
    switch (m_state) {
    case PLUGIN_INIT:
    case PLUGIN_DESTROY:
    case PLUGIN_DEINIT:
        CLOGE("this plugin was not ready(%d)", m_state);
        return INVALID_OPERATION;
        break;
    default:
        break;
    }

    return this->m_query(map);
}

/*********************************************/
/*  protected functions                      */
/*********************************************/
status_t ExynosCameraPlugIn::m_init(void)
{
    m_state = PLUGIN_INIT;
    strncpy(m_name, "", (PLUGIN_NAME_STR_SIZE - 1));

    return NO_ERROR;
}

status_t ExynosCameraPlugIn::m_deinit(void)
{
    m_state = PLUGIN_DEINIT;

    return NO_ERROR;
}

status_t ExynosCameraPlugIn::m_create(void)
{
    m_state = PLUGIN_CREATE;

    return NO_ERROR;
}

status_t ExynosCameraPlugIn::m_destroy(void)
{
    m_state = PLUGIN_DESTROY;

    return NO_ERROR;
}

status_t ExynosCameraPlugIn::m_setup(Map_t *map)
{
    m_state = PLUGIN_SETUP;

    return NO_ERROR;
}

status_t ExynosCameraPlugIn::m_process(Map_t *map)
{
    m_state = PLUGIN_PROCESS;

    return NO_ERROR;
}

status_t ExynosCameraPlugIn::m_setParameter(int key, void *data)
{
    /* do nothing */
    return NO_ERROR;
}

status_t ExynosCameraPlugIn::m_getParameter(int key, void *data)
{
    /* do nothing */
    return NO_ERROR;
}

void ExynosCameraPlugIn::m_dump(void)
{
    CLOGD("m_state : %d", m_state);
}

status_t ExynosCameraPlugIn::m_query(Map_t *map)
{
    /* do nothing */
    return NO_ERROR;
}

status_t ExynosCameraPlugIn::m_start(void)
{
    /* do nothing */
    return NO_ERROR;
}

status_t ExynosCameraPlugIn::m_stop(void)
{
    /* do nothing */
    return NO_ERROR;
}


}
