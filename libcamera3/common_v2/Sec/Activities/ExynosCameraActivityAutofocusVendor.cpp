/*
 * Copyright 2012, Samsung Electronics Co. LTD
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
#define LOG_TAG "ExynosCameraActivityAutofocusSec"
#include <log/log.h>

#include "ExynosCameraActivityAutofocus.h"

namespace android {

int ExynosCameraActivityAutofocus::t_func3ABefore(__unused void *args)
{
    return 1;
}

int ExynosCameraActivityAutofocus::t_func3AAfter(void *args)
{
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;
    enum aa_afstate curAfState;
    camera2_shot_ext *shot_ext;
    shot_ext = (struct camera2_shot_ext *)(buf->addr[buf->getMetaPlaneIndex()]);

    if (shot_ext == NULL) {
        CLOGE("shot_ext is null");
        return false;
    }

    short *af_meta_short = (short*)shot_ext->shot.udm.af.vendorSpecific;

    if (af_meta_short == NULL) {
        CLOGE("af_meta_short is null");
        return false;
    }

    curAfState = shot_ext->shot.dm.aa.afState;
    m_af_mode_info = af_meta_short[4];
    m_af_pan_focus_info = af_meta_short[5];
    m_af_typical_macro_info = af_meta_short[6];
    m_af_module_version_info = af_meta_short[7];
    m_af_state_info = af_meta_short[8];
    m_af_cur_pos_info = af_meta_short[9];
    m_af_time_info = af_meta_short[10];
    m_af_factory_info = af_meta_short[11];
    m_paf_from_info = (af_meta_short[12] & 0xFFFF) | ((af_meta_short[13] & 0xFFFF) << 16);
    m_paf_error_code = (af_meta_short[14] & 0xFFFF) | ((af_meta_short[15] & 0xFFFF) << 16);

    if ((shot_ext->shot.ctl.aa.afMode == AA_AFMODE_AUTO ||
         shot_ext->shot.ctl.aa.afMode == AA_AFMODE_CONTINUOUS_PICTURE) &&
        (curAfState != m_aaAfState) &&
        (curAfState == AA_AFSTATE_PASSIVE_FOCUSED ||
         curAfState == AA_AFSTATE_FOCUSED_LOCKED ||
         curAfState == AA_AFSTATE_NOT_FOCUSED_LOCKED ||
         curAfState == AA_AFSTATE_PASSIVE_UNFOCUSED)) {

        for(int i = 0; i < 10; i++) {
            m_af_pos[i] = (af_meta_short[19 + 8 * i] & 0xFFFF);
            m_af_filter[i] = ((af_meta_short[25 + 8 * i] & 0xFFFF)) |
                ((af_meta_short[26 + 8 * i] & 0xFFFF) << 16);
            m_af_filter[i] = m_af_filter[i] << 32;
            m_af_filter[i] += ((af_meta_short[23 + 8 * i] & 0xFFFF)) |
                ((af_meta_short[24 + 8 * i] & 0xFFFF) << 16);
        }
        displayAFStatus();
    }

    m_aaAfState = curAfState;
    m_aaAFMode  = shot_ext->shot.ctl.aa.afMode;
    m_frameCount = shot_ext->shot.dm.request.frameCount;

    return true;
}

int ExynosCameraActivityAutofocus::t_func3ABeforeHAL3(void *args)
{
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;
    camera2_shot_ext *shot_ext;
    shot_ext = (struct camera2_shot_ext *)(buf->addr[buf->getMetaPlaneIndex()]);

    if (shot_ext == NULL) {
        CLOGE("shot_ext is null");
        return false;
    }

#ifdef SAMSUNG_OT
    if ((shot_ext->shot.ctl.aa.vendor_afmode_option & SET_BIT(AA_AFMODE_OPTION_BIT_OBJECT_TRACKING)) != 0) {
        switch (m_autofocusStep) {
        case AUTOFOCUS_STEP_REQUEST:
            shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_OFF;
            shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00;
            shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;

            m_autofocusStep = AUTOFOCUS_STEP_SCANNING;
            break;
        case AUTOFOCUS_STEP_SCANNING:
            shot_ext->shot.ctl.aa.afRegions[0] = 0;
            shot_ext->shot.ctl.aa.afRegions[1] = 0;
            shot_ext->shot.ctl.aa.afRegions[2] = 0;
            shot_ext->shot.ctl.aa.afRegions[3] = 0;

            shot_ext->shot.uctl.aaUd.af_data.focusState = m_OTfocusData.focusState;
            shot_ext->shot.uctl.aaUd.af_data.focusROILeft = m_OTfocusData.focusROI.left;
            shot_ext->shot.uctl.aaUd.af_data.focusROIRight = m_OTfocusData.focusROI.right;
            shot_ext->shot.uctl.aaUd.af_data.focusROITop = m_OTfocusData.focusROI.top;
            shot_ext->shot.uctl.aaUd.af_data.focusROIBottom = m_OTfocusData.focusROI.bottom;
            shot_ext->shot.uctl.aaUd.af_data.focusWeight = m_OTfocusData.focusWeight;
            shot_ext->shot.uctl.aaUd.af_data.w_movement = m_OTfocusData.w_movement;
            shot_ext->shot.uctl.aaUd.af_data.h_movement = m_OTfocusData.h_movement;
            shot_ext->shot.uctl.aaUd.af_data.w_velocity = m_OTfocusData.w_velocity;
            shot_ext->shot.uctl.aaUd.af_data.h_velocity = m_OTfocusData.h_velocity;
            CLOGV("[OBTR]Library state: %d, x1: %d, x2: %d, y1: %d, y2: %d, weight: %d",
                    m_OTfocusData.focusState,
                    m_OTfocusData.focusROI.left, m_OTfocusData.focusROI.right,
                    m_OTfocusData.focusROI.top, m_OTfocusData.focusROI.bottom, m_OTfocusData.focusWeight);

            CLOGV("AF-Mode(FW)=(%d(%d)) AF-Region(x1,y1,x2,y2,weight)=(%d, %d, %d, %d, %d)",
                    shot_ext->shot.ctl.aa.afMode, shot_ext->shot.ctl.aa.vendor_afmode_option,
                    shot_ext->shot.ctl.aa.afRegions[0],
                    shot_ext->shot.ctl.aa.afRegions[1],
                    shot_ext->shot.ctl.aa.afRegions[2],
                    shot_ext->shot.ctl.aa.afRegions[3],
                    shot_ext->shot.ctl.aa.afRegions[4]);
            break;
        }
    } else {
        m_autofocusStep = AUTOFOCUS_STEP_REQUEST;
    }
#endif

#ifdef SAMSUNG_DOF
    if (m_flagLensMoveStart) {
        shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_OFF;
        shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00;
        shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
    }
#endif

    return 1;
}

enum aa_afstate ExynosCameraActivityAutofocus::getAfState(void)
{
    return (enum aa_afstate)m_aaAfState;
}

#ifdef SAMSUNG_OT
bool ExynosCameraActivityAutofocus::setObjectTrackingAreas(UniPluginFocusData_t *focusData)
{
    if(focusData == NULL) {
        CLOGE(" NULL focusData is received!!");
        return false;
    }
    Mutex::Autolock l(m_OTfocusDataLock);

    memcpy(&m_OTfocusData, focusData, sizeof(UniPluginFocusData_t));

    return true;
}

bool ExynosCameraActivityAutofocus::getObjectTrackingAreas(UniPluginFocusData_t *focusData)
{
    Mutex::Autolock l(m_OTfocusDataLock);

    memcpy(focusData, &m_OTfocusData, sizeof(UniPluginFocusData_t));

    return true;
}
#endif

#ifdef SAMSUNG_DOF
void ExynosCameraActivityAutofocus::setStartLensMove(bool toggle)
{
    CLOGI("toggle(%d)", toggle);

    m_flagLensMoveStart = toggle;
}
#endif

ExynosCameraActivityAutofocus::AUTOFOCUS_STATE ExynosCameraActivityAutofocus::afState2AUTOFOCUS_STATE(enum aa_afstate aaAfState)
{
    AUTOFOCUS_STATE autoFocusState;

    switch (aaAfState) {
    case AA_AFSTATE_INACTIVE:
        autoFocusState = AUTOFOCUS_STATE_NONE;
        break;
    case AA_AFSTATE_PASSIVE_SCAN:
    case AA_AFSTATE_ACTIVE_SCAN:
        autoFocusState = AUTOFOCUS_STATE_SCANNING;
        break;
    case AA_AFSTATE_PASSIVE_FOCUSED:
    case AA_AFSTATE_FOCUSED_LOCKED:
        autoFocusState = AUTOFOCUS_STATE_SUCCEESS;
        break;
    case AA_AFSTATE_NOT_FOCUSED_LOCKED:
    case AA_AFSTATE_PASSIVE_UNFOCUSED:
        autoFocusState = AUTOFOCUS_STATE_FAIL;
        break;
    default:
        autoFocusState = AUTOFOCUS_STATE_NONE;
        break;
    }

    return autoFocusState;
}

} /* namespace android */
