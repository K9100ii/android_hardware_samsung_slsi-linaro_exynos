/*
 * Copyright 2015, Samsung Electronics Co. LTD
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
 * \file      ExynosCameraSFLMgr.cpp
 * \brief     source file for ExynosCameraSFLMgr
 * \author    VijayaKumar S N (vijay.sathenahallin@samsung.com)
 * \date      2015/10/27
 *
 */

/* #define LOG_NDEBUG 0 */
#define LOG_TAG "ExynosCameraSFLMgr"
#include "ExynosCameraSFLMgr.h"
#include "Sfl/vendor/arcsoft/hdr/arcsoftHDR.h"
#include "Sfl/vendor/arcsoft/antishake/arcsoftAntiShake.h"
#include "Sfl/vendor/arcsoft/nightShot/arcsoftNightShot.h"
#include "Sfl/vendor/arcsoft/flawless/arcsoftflawless.h"

namespace android {

#ifdef SFLINFO_TRACE
int sflInfoObjectCount;
Mutex sflInfoObjectCountLock;
#endif

ExynosCameraSFLMgr::ExynosCameraSFLMgr(const char* name, int cameraId)
{
    EXYNOS_CAMERA_SFLMGR_TRACE_IN();

    memset(m_name, 0x00, sizeof(m_name));

    m_cameraId = cameraId;
    strncpy(m_name, name, EXYNOS_CAMERA_NAME_STR_SIZE - 1);

    for (int i = 0 ; i < SFLIBRARY_MGR::MAX ; i++) {
        m_createLibrary((SFLIBRARY_MGR::SFLType)i);
    }

    m_curType = SFLIBRARY_MGR::NONE;

    EXYNOS_CAMERA_SFLMGR_TRACE_OUT();
}

ExynosCameraSFLMgr::~ExynosCameraSFLMgr()
{
    EXYNOS_CAMERA_SFLMGR_TRACE_IN();

    m_curType = SFLIBRARY_MGR::NONE;

    for (int i = 0 ; i < SFLIBRARY_MGR::MAX ; i++) {
        m_destroyLibrary((SFLIBRARY_MGR::SFLType)i);
    }

    EXYNOS_CAMERA_SFLMGR_TRACE_OUT();
}

sp<ExynosCameraSFLInterface> ExynosCameraSFLMgr::getLibrary(SFLIBRARY_MGR::SFLType type)
{
    EXYNOS_CAMERA_SFLMGR_TRACE_IN();
    sp<ExynosCameraSFLInterface> ret = NULL;

    ret = m_getLibrary(type);

    EXYNOS_CAMERA_SFLMGR_TRACE_OUT();
    return ret;
}

void ExynosCameraSFLMgr::setStatus(SFLIBRARY_MGR::SFLType type, bool enable)
{
    EXYNOS_CAMERA_SFLMGR_TRACE_IN();

    m_setStatus(type, enable);

    EXYNOS_CAMERA_SFLMGR_TRACE_OUT();
}

bool ExynosCameraSFLMgr::getStatus(SFLIBRARY_MGR::SFLType type)
{
    EXYNOS_CAMERA_SFLMGR_TRACE_IN();
    bool ret = false;

    ret = m_getStatus(type);

    EXYNOS_CAMERA_SFLMGR_TRACE_OUT();
    return ret;
}

void ExynosCameraSFLMgr::setType(SFLIBRARY_MGR::SFLType type)
{
    EXYNOS_CAMERA_SFLMGR_TRACE_IN();

    m_setType(type);

    EXYNOS_CAMERA_SFLMGR_TRACE_OUT();
}

SFLIBRARY_MGR::SFLType ExynosCameraSFLMgr::getType()
{
    EXYNOS_CAMERA_SFLMGR_TRACE_IN();
    SFLIBRARY_MGR::SFLType ret = SFLIBRARY_MGR::NONE;

    ret = m_getType();

    EXYNOS_CAMERA_SFLMGR_TRACE_OUT();
    return ret;
}

void ExynosCameraSFLMgr::setRunEnable(SFLIBRARY_MGR::SFLType type, bool enable)
{
    EXYNOS_CAMERA_SFLMGR_TRACE_IN();

    m_setRunEnable(type, enable);

    EXYNOS_CAMERA_SFLMGR_TRACE_OUT();
}

bool ExynosCameraSFLMgr::getRunEnable(SFLIBRARY_MGR::SFLType type)
{
    EXYNOS_CAMERA_SFLMGR_TRACE_IN();
    bool ret = false;

    ret = m_getRunEnable(type);

    EXYNOS_CAMERA_SFLMGR_TRACE_OUT();
    return ret;
}

void ExynosCameraSFLMgr::m_setRunEnable(SFLIBRARY_MGR::SFLType type, bool enable)
{
    EXYNOS_CAMERA_SFLMGR_TRACE_IN();
    Mutex::Autolock lock(m_stateLock);

    switch(type) {
        case SFLIBRARY_MGR::NONE:
            break;
        case SFLIBRARY_MGR::HDR:
        case SFLIBRARY_MGR::NIGHT:
        case SFLIBRARY_MGR::ANTISHAKE:
        case SFLIBRARY_MGR::OIS:
        case SFLIBRARY_MGR::PANORAMA:
        case SFLIBRARY_MGR::FLAWLESS:
            if (m_library[type] != NULL) {
                m_library[type]->setRunEnable(enable);
            } else {
                CLOGE("ERR(%s[%d]): set runEnable failed, m_library[type] == NULL, invalid type(%d)", __FUNCTION__, __LINE__, type);
            }

            break;
        default:
            CLOGE("ERR(%s[%d]): set runEnable failed, invalid type(%d)", __FUNCTION__, __LINE__, type);
            break;
    }

    EXYNOS_CAMERA_SFLMGR_TRACE_OUT();
}

bool ExynosCameraSFLMgr::m_getRunEnable(SFLIBRARY_MGR::SFLType type)
{
    EXYNOS_CAMERA_SFLMGR_TRACE_IN();
    Mutex::Autolock lock(m_stateLock);

    bool ret = false;
    switch(type) {
        case SFLIBRARY_MGR::NONE:
            break;
        case SFLIBRARY_MGR::HDR:
        case SFLIBRARY_MGR::NIGHT:
        case SFLIBRARY_MGR::ANTISHAKE:
        case SFLIBRARY_MGR::OIS:
        case SFLIBRARY_MGR::PANORAMA:
        case SFLIBRARY_MGR::FLAWLESS:
            if (m_library[type] != NULL) {
                ret = m_library[type]->getRunEnable();
            } else {
                CLOGE("ERR(%s[%d]): set runEnable failed, m_library[type] == NULL, invalid type(%d)", __FUNCTION__, __LINE__, type);
            }

            break;
        default:
            CLOGE("ERR(%s[%d]): set runEnable failed, invalid type(%d)", __FUNCTION__, __LINE__, type);
            break;
    }

    EXYNOS_CAMERA_SFLMGR_TRACE_OUT();
    return ret;
}

void ExynosCameraSFLMgr::m_createLibrary(SFLIBRARY_MGR::SFLType type)
{
    EXYNOS_CAMERA_SFLMGR_TRACE_IN();
    Mutex::Autolock lock(m_libraryLock);

    switch(type) {
        case SFLIBRARY_MGR::NONE:
            break;
        case SFLIBRARY_MGR::HDR:
            m_library[type] = new ARCSoftHDR("ARCSOFT HDR", m_cameraId);
            break;
        case SFLIBRARY_MGR::NIGHT:
            m_library[type] = new ARCSoftNightShot("ARCSOFT NIGHT", m_cameraId);
            break;
        case SFLIBRARY_MGR::ANTISHAKE:
            m_library[type] = new ARCSoftAntiShake("ARCSOFT ANTISHAKE", m_cameraId);
            break;
        case SFLIBRARY_MGR::OIS:
            /*m_library[type] = new ARCSoftOIS("ARCSOFT OIS", m_cameraId);*/
            m_library[type] = NULL;
            break;
        case SFLIBRARY_MGR::PANORAMA:
            /*m_library[type] = new ARCSoftPanorama("ARCSOFT PANORAMA", m_cameraId);*/
            m_library[type] = NULL;
            break;
        case SFLIBRARY_MGR::FLAWLESS:
            m_library[type] = new ARCSoftFlawless("ARCSOFT FLAWLESS", m_cameraId);
            break;
        default:
            CLOGE("ERR(%s[%d]): create Library failed, invalid type(%d)", __FUNCTION__, __LINE__, type);
            break;
    }

    EXYNOS_CAMERA_SFLMGR_TRACE_OUT();
}

void ExynosCameraSFLMgr::m_destroyLibrary(SFLIBRARY_MGR::SFLType type)
{
    EXYNOS_CAMERA_SFLMGR_TRACE_IN();
    Mutex::Autolock lock(m_libraryLock);

    switch(type) {
        case SFLIBRARY_MGR::NONE:
            break;
        case SFLIBRARY_MGR::HDR:
        case SFLIBRARY_MGR::NIGHT:
        case SFLIBRARY_MGR::ANTISHAKE:
        case SFLIBRARY_MGR::OIS:
        case SFLIBRARY_MGR::PANORAMA:
        case SFLIBRARY_MGR::FLAWLESS:
            if (m_library[type] == NULL) {
                CLOGE("ERR(%s[%d]): destroy Library failed, invalid type(%d)", __FUNCTION__, __LINE__, type);
            }
            m_library[type] = NULL;
            break;
        default:
            CLOGE("ERR(%s[%d]): destroy Library failed, invalid type(%d)", __FUNCTION__, __LINE__, type);
            break;
    }

    EXYNOS_CAMERA_SFLMGR_TRACE_OUT();
    return;
}

sp<ExynosCameraSFLInterface> ExynosCameraSFLMgr::m_getLibrary(SFLIBRARY_MGR::SFLType type)
{
    EXYNOS_CAMERA_SFLMGR_TRACE_IN();
    Mutex::Autolock lock(m_libraryLock);
    sp<ExynosCameraSFLInterface> ret = NULL;

    switch(type) {
        case SFLIBRARY_MGR::HDR:
        case SFLIBRARY_MGR::NIGHT:
        case SFLIBRARY_MGR::ANTISHAKE:
        case SFLIBRARY_MGR::OIS:
        case SFLIBRARY_MGR::PANORAMA:
        case SFLIBRARY_MGR::FLAWLESS:
            ret = m_library[type];
            break;
        default:
            CLOGE("ERR(%s[%d]): get Library failed, type(%d)", __FUNCTION__, __LINE__, type);
            break;
    }

    if (ret == NULL) {
        CLOGE("ERR(%s[%d]): get Library failed, library is NULL, type(%d) ", __FUNCTION__, __LINE__, type);
    }

    EXYNOS_CAMERA_SFLMGR_TRACE_OUT();
    return ret;
}

void ExynosCameraSFLMgr::m_setStatus(SFLIBRARY_MGR::SFLType type, bool enable)
{
    EXYNOS_CAMERA_SFLMGR_TRACE_IN();
    Mutex::Autolock lock(m_stateLock);

    switch(type) {
        case SFLIBRARY_MGR::NONE:
            break;
        case SFLIBRARY_MGR::HDR:
        case SFLIBRARY_MGR::NIGHT:
        case SFLIBRARY_MGR::ANTISHAKE:
        case SFLIBRARY_MGR::OIS:
        case SFLIBRARY_MGR::PANORAMA:
        case SFLIBRARY_MGR::FLAWLESS:
            if (m_library[type] != NULL) {
                m_library[type]->setEnable(enable);
            } else {
                CLOGE("ERR(%s[%d]): set status failed, invalid type(%d)", __FUNCTION__, __LINE__, type);
            }

            break;
        default:
            CLOGE("ERR(%s[%d]): set status failed, invalid type(%d)", __FUNCTION__, __LINE__, type);
            break;
    }

    EXYNOS_CAMERA_SFLMGR_TRACE_OUT();
}
bool ExynosCameraSFLMgr::m_getStatus(SFLIBRARY_MGR::SFLType type)
{
    EXYNOS_CAMERA_SFLMGR_TRACE_IN();
    Mutex::Autolock lock(m_stateLock);

    bool ret = false;
    switch(type) {
        case SFLIBRARY_MGR::NONE:
            break;
        case SFLIBRARY_MGR::HDR:
        case SFLIBRARY_MGR::NIGHT:
        case SFLIBRARY_MGR::ANTISHAKE:
        case SFLIBRARY_MGR::OIS:
        case SFLIBRARY_MGR::PANORAMA:
        case SFLIBRARY_MGR::FLAWLESS:
            if (m_library[type] != NULL) {
                ret = m_library[type]->getEnable();
            } else {
                CLOGE("ERR(%s[%d]): get status failed, invalid type(%d)", __FUNCTION__, __LINE__, type);
            }

            break;
        default:
            CLOGE("ERR(%s[%d]): get status failed, invalid type(%d)", __FUNCTION__, __LINE__, type);
            break;
    }

    EXYNOS_CAMERA_SFLMGR_TRACE_OUT();
    return ret;
}

void ExynosCameraSFLMgr::m_setType(SFLIBRARY_MGR::SFLType type)
{
    EXYNOS_CAMERA_SFLMGR_TRACE_IN();
    Mutex::Autolock lock(m_stateLock);

    bool ret       = false;
    bool runEnable = false;

    if (m_curType != SFLIBRARY_MGR::NONE && m_curType != type) {
        runEnable = m_library[m_curType]->getRunEnable();
    }

    if (runEnable == true) {
        CLOGE("ERR(%s[%d]): set curType skipped, curType is in-progress, curType(%d) newType(%d) curRunEnable(%s)", __FUNCTION__, __LINE__, m_curType, type, (runEnable)?"true":"false");
    } else {
        switch(type) {
            case SFLIBRARY_MGR::NONE:
            case SFLIBRARY_MGR::HDR:
            case SFLIBRARY_MGR::NIGHT:
            case SFLIBRARY_MGR::ANTISHAKE:
            case SFLIBRARY_MGR::OIS:
            case SFLIBRARY_MGR::PANORAMA:
            case SFLIBRARY_MGR::FLAWLESS:
                if (m_curType != type) {
                    m_curType = type;
                }
                break;
            default:
                CLOGE("ERR(%s[%d]): set curType failed, invalid type(%d)", __FUNCTION__, __LINE__, type);
                break;
        }
    }

    EXYNOS_CAMERA_SFLMGR_TRACE_OUT();
}

SFLIBRARY_MGR::SFLType ExynosCameraSFLMgr::m_getType()
{
    EXYNOS_CAMERA_SFLMGR_TRACE_IN();
    Mutex::Autolock lock(m_stateLock);

    SFLIBRARY_MGR::SFLType ret = SFLIBRARY_MGR::NONE;
    switch(m_curType) {
        case SFLIBRARY_MGR::NONE:
        case SFLIBRARY_MGR::HDR:
        case SFLIBRARY_MGR::NIGHT:
        case SFLIBRARY_MGR::ANTISHAKE:
        case SFLIBRARY_MGR::OIS:
        case SFLIBRARY_MGR::PANORAMA:
        case SFLIBRARY_MGR::FLAWLESS:
            ret = m_curType;
            break;
        default:
            CLOGE("ERR(%s[%d]): set curType failed, invalid type(%d)", __FUNCTION__, __LINE__, ret);
            break;
    }

    EXYNOS_CAMERA_SFLMGR_TRACE_OUT();
    return ret;
}


}; //namespace android

