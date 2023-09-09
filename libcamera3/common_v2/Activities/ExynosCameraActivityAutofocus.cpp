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
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* #define LOG_NDEBUG 0 */
#define LOG_TAG "ExynosCameraActivityAutofocus"
#include <log/log.h>

#include "ExynosCameraActivityAutofocus.h"

namespace android {

#define WAIT_COUNT_FAIL_STATE                (7)
#define AUTOFOCUS_WAIT_COUNT_STEP_REQUEST    (3)

#define AUTOFOCUS_WAIT_COUNT_FRAME_COUNT_NUM (3)       /* n + x frame count */
#define AUTOFOCUS_WATING_TIME_LOCK_AF        (10000)   /* 10msec */
#define AUTOFOCUS_TOTAL_WATING_TIME_LOCK_AF  (300000)  /* 300msec */
#define AUTOFOCUS_SKIP_FRAME_LOCK_AF         (6)       /* == NUM_BAYER_BUFFERS */

#define SET_BIT(x)      (1 << x)

ExynosCameraActivityAutofocus::ExynosCameraActivityAutofocus(int cameraId) : ExynosCameraActivityBase(cameraId)
{
    /* first AF operation is trigger infinity mode */
    m_autofocusStep = AUTOFOCUS_STEP_REQUEST;
    m_aaAfState = ::AA_AFSTATE_INACTIVE;
    m_afState = AUTOFOCUS_STATE_NONE;
    m_aaAFMode = ::AA_AFMODE_OFF;
    m_metaCtlAFMode = -1;
    m_frameCount = 0;

#ifdef SAMSUNG_DOF
    m_flagLensMoveStart = false;
#endif
    m_macroPosition = AUTOFOCUS_MACRO_POSITION_BASE;

    m_af_mode_info = 0;
    m_af_pan_focus_info = 0;
    m_af_typical_macro_info = 0;
    m_af_module_version_info = 0;
    m_af_state_info = 0;
    m_af_cur_pos_info = 0;
    m_af_time_info = 0;
    m_af_factory_info = 0;
    m_paf_from_info = 0;
    m_paf_error_code = 0;

#ifdef SAMSUNG_OT
    memset(&m_OTfocusData, 0, sizeof(UniPluginFocusData_t));
#endif
}

ExynosCameraActivityAutofocus::~ExynosCameraActivityAutofocus()
{
}

int ExynosCameraActivityAutofocus::t_funcNull(__unused void *args)
{
    return 1;
}

int ExynosCameraActivityAutofocus::t_funcSensorBefore(__unused void *args)
{
    return 1;
}

int ExynosCameraActivityAutofocus::t_funcSensorAfter(__unused void *args)
{
    return 1;
}

int ExynosCameraActivityAutofocus::t_funcISPBefore(__unused void *args)
{
    return 1;
}

int ExynosCameraActivityAutofocus::t_funcISPAfter(__unused void *args)
{
    return 1;
}

int ExynosCameraActivityAutofocus::t_func3AAfterHAL3(__unused void *args)
{
    return 1;
}

int ExynosCameraActivityAutofocus::t_funcVRABefore(__unused void *args)
{
    return 1;
}

int ExynosCameraActivityAutofocus::t_funcVRAAfter(__unused void *args)
{
    return 1;
}

void ExynosCameraActivityAutofocus::displayAFInfo(void)
{
    CLOGD("(%s):==================================================", "CMGEFL");
    CLOGD("(%s):0x%x", "CMGEFL", m_af_mode_info);
    CLOGD("(%s):0x%x", "CMGEFL", m_af_pan_focus_info);
    CLOGD("(%s):0x%x", "CMGEFL", m_af_typical_macro_info);
    CLOGD("(%s):0x%x", "CMGEFL", m_af_module_version_info);
    CLOGD("(%s):0x%x", "CMGEFL", m_af_state_info);
    CLOGD("(%s):0x%x", "CMGEFL", m_af_cur_pos_info);
    CLOGD("(%s):0x%x", "CMGEFL", m_af_time_info);
    CLOGD("(%s):0x%x", "CMGEFL", m_af_factory_info);
    CLOGD("(%s):0x%x", "CMGEFL", m_paf_from_info);
    CLOGD("(%s):0x%x", "CMGEFL", m_paf_error_code);
    CLOGD("(%s):==================================================", "CMGEFL");
    return ;
}

void ExynosCameraActivityAutofocus::displayAFStatus(void)
{
    CLOGD("(%s): %d %d %d %d %d %d %d %d %d %d", "CMGEFL",
        m_af_pos[0], m_af_pos[1],
        m_af_pos[2], m_af_pos[3],
        m_af_pos[4], m_af_pos[5],
        m_af_pos[6], m_af_pos[7],
        m_af_pos[8], m_af_pos[9]);

    CLOGD("(%s): %jd %jd %jd %jd %jd %jd %jd %jd %jd %jd", "CMGEFL",
        m_af_filter[0], m_af_filter[1],
        m_af_filter[2], m_af_filter[3],
        m_af_filter[4], m_af_filter[5],
        m_af_filter[6], m_af_filter[7],
        m_af_filter[8], m_af_filter[9]);

    return ;
}
} /* namespace android */
