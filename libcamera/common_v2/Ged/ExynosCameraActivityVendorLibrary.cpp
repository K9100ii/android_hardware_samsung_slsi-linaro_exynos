/*
**
** Copyright 2012, Samsung Electronics Co. LTD
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
#define LOG_TAG "ExynosCameraActivityVendorLibrary"
#include <cutils/log.h>

#include "ExynosCameraActivityVendorLibrary.h"

#define TIME_CHECK 1

namespace android {

class ExynosCamera;

ExynosCameraActivityVendorLibrary::ExynosCameraActivityVendorLibrary()
{
    t_isExclusiveReq = false;
    t_isActivated = false;
    t_reqNum = 0x1F;
    t_reqStatus = 0;

    m_delay = 0;
    m_mode = SFLIBRARY_MGR::NONE;
    m_check = false;
    m_step = VENDOR_STEP_OFF;
    m_requireRestore = false;
    memset(&m_shotExt, 0x00, sizeof(m_shotExt));
}

ExynosCameraActivityVendorLibrary::~ExynosCameraActivityVendorLibrary()
{
    t_isExclusiveReq = false;
    t_isActivated = false;
    t_reqNum = 0x1F;
    t_reqStatus = 0;

    m_delay = 0;
    m_mode = SFLIBRARY_MGR::NONE;
    m_check = false;
    m_requireRestore = false;
    memset(&m_shotExt, 0x00, sizeof(m_shotExt));
}

int ExynosCameraActivityVendorLibrary::t_funcNull(void *args)
{
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;

    return 1;
}

int ExynosCameraActivityVendorLibrary::t_funcSensorBefore(void *args)
{
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;
    camera2_shot_ext *shot_ext = (struct camera2_shot_ext *)(buf->addr[1]);

    return 1;
}

int ExynosCameraActivityVendorLibrary::t_funcSensorAfter(void *args)
{
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;
    camera2_shot_ext *shot_ext = (struct camera2_shot_ext *)(buf->addr[1]);
    int ret = 1;

    return ret;
}

int ExynosCameraActivityVendorLibrary::t_funcISPBefore(void *args)
{
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;
    camera2_shot_ext *shot_ext = (struct camera2_shot_ext *)(buf->addr[1]);

done:
    return 1;
}

int ExynosCameraActivityVendorLibrary::t_funcISPAfter(void *args)
{
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;
    camera2_shot_ext *shot_ext = (struct camera2_shot_ext *)(buf->addr[1]);

    return 1;
}

int ExynosCameraActivityVendorLibrary::t_func3ABefore(void *args)
{
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;
    camera2_shot_ext *shot_ext = (struct camera2_shot_ext *)(buf->addr[1]);
    sp<ExynosCameraSFLInterface> library = NULL;
    bool flagRun = true;

    if (shot_ext == NULL) {
        return 0;
    }

    switch(m_mode) {
        case SFLIBRARY_MGR::NONE:
        case SFLIBRARY_MGR::HDR:
        case SFLIBRARY_MGR::NIGHT:
            break;
        case SFLIBRARY_MGR::ANTISHAKE:
            library = m_libraryMgr->getLibrary(m_mode);
            if (library->getEnable() && library->getRunEnable()) {
                m_updateAntiShake(shot_ext);
            }
            break;
        case SFLIBRARY_MGR::OIS:
        case SFLIBRARY_MGR::PANORAMA:
        case SFLIBRARY_MGR::FLAWLESS:
            break;
        default:
            ALOGE("ERR(%s[%d]): get status failed, invalid type(%d)", __FUNCTION__, __LINE__, m_mode);
            break;
    }

    return 1;
}

int ExynosCameraActivityVendorLibrary::t_func3AAfter(void *args)
{
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;
    camera2_shot_ext *shot_ext = (struct camera2_shot_ext *)(buf->addr[1]);
    sp<ExynosCameraSFLInterface> library = NULL;

    ALOGV("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    if (shot_ext == NULL) {
        return 0;
    }

    switch(m_mode) {
        case SFLIBRARY_MGR::NONE:
        case SFLIBRARY_MGR::HDR:
        case SFLIBRARY_MGR::NIGHT:
            break;
        case SFLIBRARY_MGR::ANTISHAKE:
            library = m_libraryMgr->getLibrary(m_mode);
            if (library->getEnable() && library->getRunEnable()) {
                m_checkAntiShake(shot_ext);
            }
            break;
        case SFLIBRARY_MGR::OIS:
        case SFLIBRARY_MGR::PANORAMA:
        case SFLIBRARY_MGR::FLAWLESS:
            break;
        default:
            ALOGE("ERR(%s[%d]): get status failed, invalid type(%d)", __FUNCTION__, __LINE__, m_mode);
            break;
    }

    return 1;
}

int ExynosCameraActivityVendorLibrary::t_func3ABeforeHAL3(__unused void *args)
{
    return 1;
}

int ExynosCameraActivityVendorLibrary::t_func3AAfterHAL3(__unused void *args)
{
    return 1;
}

int ExynosCameraActivityVendorLibrary::t_funcSCPBefore(__unused void *args)
{
    ALOGV("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    return 1;
}

int ExynosCameraActivityVendorLibrary::t_funcSCPAfter(void *args)
{
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;
    camera2_stream *shot_stream = (struct camera2_stream *)(buf->addr[2]);

    return 1;
}

int ExynosCameraActivityVendorLibrary::t_funcSCCBefore(void *args)
{
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;

    ALOGV("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    return 1;
}

int ExynosCameraActivityVendorLibrary::t_funcSCCAfter(void *args)
{
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;

    ALOGV("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    return 1;
}

int ExynosCameraActivityVendorLibrary::m_updateAntiShake(camera2_shot_ext *shot_ext)
{
    bool flagRun = true;
    sp<ExynosCameraSFLInterface> library = NULL;

    struct CommandInfo cmdinfo;
    library = m_libraryMgr->getLibrary(m_mode);

    do {
        switch(m_step) {
            case VENDOR_STEP_OFF:
                flagRun = false;
                break;
            case VENDOR_STEP_START:
                ALOGD("DEBUG(%s[%d]):VENDOR_STEP_START", __FUNCTION__, __LINE__);
                /* m_requireRestore = true; not need restore logic */
                memset(&m_shotExt, 0x00, sizeof(m_shotExt));
                m_step = VENDOR_STEP_ANTISHAKE_PARAM_SET;
                break;

            case VENDOR_STEP_ANTISHAKE_PARAM_SET:
                ALOGD("DEBUG(%s[%d]):VENDOR_STEP_ANTISHAKE_PARAM_SET", __FUNCTION__, __LINE__);
                m_step = VENDOR_STEP_ANTISHAKE_WAIT_FRAME;
                makeSFLCommand(&cmdinfo, SFL::GET_ADJUSTPARAMINFO, SFL::TYPE_PREVIEW, SFL::POS_SRC);
                library->processCommand(&cmdinfo, (void*)&m_shotExt);
            case VENDOR_STEP_ANTISHAKE_WAIT_FRAME:
                ALOGD("DEBUG(%s[%d]):VENDOR_STEP_ANTISHAKE_WAIT_FRAME", __FUNCTION__, __LINE__);

                /* get library wrapper target exposure / sensitivity */
                shot_ext->shot.ctl.sensor.exposureTime = m_shotExt.shot.ctl.sensor.exposureTime;
                shot_ext->shot.ctl.aa.aeMode = m_shotExt.shot.ctl.aa.aeMode;
                setMetaCtlAeRegion(shot_ext, m_shotExt.shot.ctl.aa.aeRegions[0], m_shotExt.shot.ctl.aa.aeRegions[1], m_shotExt.shot.ctl.aa.aeRegions[2], m_shotExt.shot.ctl.aa.aeRegions[3], m_shotExt.shot.ctl.aa.aeRegions[4]);
                setMetaCtlIso(shot_ext, AA_ISOMODE_MANUAL, m_shotExt.shot.ctl.sensor.sensitivity);
#ifdef USE_LSI_3A
                shot_ext->shot.ctl.aa.aeMode = m_shotExt.shot.ctl.aa.aeMode;
#endif

                flagRun = false;
                break;

            case VENDOR_STEP_END:
                ALOGD("DEBUG(%s[%d]):VENDOR_STEP_END", __FUNCTION__, __LINE__);
                m_step = VENDOR_STEP_OFF;
                m_delay = 0;
                m_check = false;
                flagRun = false;
                memset(&m_shotExt, 0x00, sizeof(m_shotExt));
                break;

            default:
                ALOGW("WARN(%s[%d]): invalid step(%d)", __FUNCTION__, __LINE__, m_step);
                m_step = VENDOR_STEP_OFF;
                m_delay = 0;
                m_check = false;
                break;
        }
    } while(flagRun);

    ALOGV("DEBUG(%s[%d]): fps(%d x %d) exposure(%llu) sensitivity(%d)", __FUNCTION__, __LINE__,
           shot_ext->shot.ctl.aa.aeTargetFpsRange[0], shot_ext->shot.ctl.aa.aeTargetFpsRange[1],
           shot_ext->shot.ctl.sensor.exposureTime,
           shot_ext->shot.ctl.sensor.sensitivity
           );

    return 0;
}

int ExynosCameraActivityVendorLibrary::m_checkAntiShake(camera2_shot_ext *shot_ext)
{
    struct CommandInfo cmdinfo;
    sp<ExynosCameraSFLInterface> library = NULL;
    uint32_t curCount = 0, maxCount = 0;

    library = m_libraryMgr->getLibrary(m_mode);

    switch(m_step) {
        case VENDOR_STEP_OFF:
            ALOGD("DEBUG(%s[%d]):VENDOR_STEP_OFF", __FUNCTION__, __LINE__);
            break;
        case VENDOR_STEP_START:
            ALOGD("DEBUG(%s[%d]):VENDOR_STEP_START", __FUNCTION__, __LINE__);
            break;
        case VENDOR_STEP_ANTISHAKE_PARAM_SET:
            ALOGD("DEBUG(%s[%d]):VENDOR_STEP_ANTISHAKE_PARAM_SET", __FUNCTION__, __LINE__);
            break;
        case VENDOR_STEP_ANTISHAKE_WAIT_FRAME:
            ALOGD("DEBUG(%s[%d]):VENDOR_STEP_ANTISHAKE_WAIT_FRAME", __FUNCTION__, __LINE__);
            /* check the request and result */

            if (shot_ext->shot.dm.sensor.exposureTime == m_shotExt.shot.ctl.sensor.exposureTime && shot_ext->shot.dm.sensor.sensitivity == m_shotExt.shot.ctl.sensor.sensitivity) {

                /* update capture best frame */
                makeSFLCommand(&cmdinfo, SFL::GET_CURSELECTCNT, SFL::TYPE_PREVIEW, SFL::POS_SRC);
                library->processCommand(&cmdinfo, (void*)&curCount);

                makeSFLCommand(&cmdinfo, SFL::GET_MAXSELECTCNT, SFL::TYPE_PREVIEW, SFL::POS_SRC);
                library->processCommand(&cmdinfo, (void*)&maxCount);

                if (maxCount > curCount) {
                    curCount++;
                    ALOGD("DEBUG(%s[%d]):update best preview frame(%d)", __FUNCTION__, __LINE__, shot_ext->shot.dm.request.frameCount);
                    makeSFLCommand(&cmdinfo, SFL::SET_BESTFRAMEINFO, SFL::TYPE_PREVIEW, SFL::POS_SRC);
                    library->processCommand(&cmdinfo, (void*)&shot_ext->shot.dm.request.frameCount);

                    makeSFLCommand(&cmdinfo, SFL::SET_CURSELECTCNT, SFL::TYPE_PREVIEW, SFL::POS_SRC);
                    library->processCommand(&cmdinfo, (void*)&curCount);
                }

                /* update capture best frame */
                makeSFLCommand(&cmdinfo, SFL::SET_BESTFRAMEINFO, SFL::TYPE_CAPTURE, SFL::POS_SRC);
                library->processCommand(&cmdinfo, (void*)&shot_ext->shot.dm.request.frameCount);
                ALOGD("DEBUG(%s[%d]):update best capture frame(%d)", __FUNCTION__, __LINE__, shot_ext->shot.dm.request.frameCount);

                m_step = VENDOR_STEP_END;

             } else if (shot_ext->shot.ctl.sensor.exposureTime == m_shotExt.shot.ctl.sensor.exposureTime || 
                        shot_ext->shot.ctl.sensor.sensitivity == m_shotExt.shot.ctl.sensor.sensitivity) {
                /* done exposure or sensitivity  : fix update preview bestCount */
                makeSFLCommand(&cmdinfo, SFL::GET_CURSELECTCNT, SFL::TYPE_PREVIEW, SFL::POS_SRC);
                library->processCommand(&cmdinfo, (void*)&curCount);

                makeSFLCommand(&cmdinfo, SFL::GET_MAXSELECTCNT, SFL::TYPE_PREVIEW, SFL::POS_SRC);
                library->processCommand(&cmdinfo, (void*)&maxCount);

                if (maxCount > curCount) {
                    curCount++;
                    ALOGD("DEBUG(%s[%d]):update best preview frame(%d)", __FUNCTION__, __LINE__, shot_ext->shot.dm.request.frameCount);
                    makeSFLCommand(&cmdinfo, SFL::SET_BESTFRAMEINFO, SFL::TYPE_PREVIEW, SFL::POS_SRC);
                    library->processCommand(&cmdinfo, (void*)&shot_ext->shot.dm.request.frameCount);

                    makeSFLCommand(&cmdinfo, SFL::SET_CURSELECTCNT, SFL::TYPE_PREVIEW, SFL::POS_SRC);
                    library->processCommand(&cmdinfo, (void*)&curCount);
                }
            }

            /* update libraryMgr bestCount. */
            break;
        case VENDOR_STEP_END:
            ALOGD("DEBUG(%s[%d]):VENDOR_STEP_END", __FUNCTION__, __LINE__);
            break;
        default:
            break;
    }

    ALOGV("DEBUG(%s[%d]): ctl fcount(%d) exposure(%llu) sensitivity(%d)", __FUNCTION__, __LINE__,
           shot_ext->shot.dm.request.frameCount,
           shot_ext->shot.ctl.sensor.exposureTime,
           shot_ext->shot.ctl.sensor.sensitivity
           );


    ALOGV("DEBUG(%s[%d]): dm fcount(%d) exposure(%llu) sensitivity(%d)", __FUNCTION__, __LINE__,
           shot_ext->shot.dm.request.frameCount,
           shot_ext->shot.dm.sensor.exposureTime,
           shot_ext->shot.dm.sensor.sensitivity
           );

    return 0;
}

int ExynosCameraActivityVendorLibrary::setMode(SFLIBRARY_MGR::SFLType type)
{
    m_mode = type;

    ALOGD("DEBUG(%s[%d]):(%d)", __FUNCTION__, __LINE__, m_mode);

    return 1;
}

int ExynosCameraActivityVendorLibrary::setStep(enum VENDORLIB_STEP step)
{
    m_step = step;

    if (m_step == VENDOR_STEP_OFF) {
        m_check = false;
        /* dealloc buffers */
    }

    if (m_step == VENDOR_STEP_START) {
        /* alloc buffers */
    }

    ALOGD("DEBUG(%s[%d]):(%d)", __FUNCTION__, __LINE__, m_step);

    return 1;
}

bool ExynosCameraActivityVendorLibrary::setLibraryManager(sp<ExynosCameraSFLMgr> manager)
{
    bool ret = true;
    m_libraryMgr = manager;
    return ret;
}

} /* namespace android */

