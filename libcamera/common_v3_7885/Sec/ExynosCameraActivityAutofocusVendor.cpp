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
#include <cutils/log.h>

#include "ExynosCameraActivityAutofocus.h"

namespace android {

int ExynosCameraActivityAutofocus::t_func3ABefore(void *args)
{
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;

    camera2_shot_ext *shot_ext;
    shot_ext = (struct camera2_shot_ext *)(buf->addr[buf->getMetaPlaneIndex()]);

    if (shot_ext == NULL) {
        CLOGE("shot_ext is null");
        return false;
    }

    int currentState = this->getCurrentState();

    shot_ext->shot.ctl.aa.vendor_afState = (enum aa_afstate)m_aaAfState;

    switch (m_autofocusStep) {
        case AUTOFOCUS_STEP_STOP:
            /* m_interenalAutoFocusMode = m_autoFocusMode; */
            shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_OFF;
            shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00;
            shot_ext->shot.ctl.aa.vendor_afmode_ext = AA_AFMODE_EXT_OFF;
            shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
            break;
        case AUTOFOCUS_STEP_TRIGGER_START:
            /* Autofocus lock for capture.
             * The START afTrigger make the AF state be locked on current state.
             */
            m_AUTOFOCUS_MODE2AA_AFMODE(m_interenalAutoFocusMode, shot_ext);
            shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_START;
            break;
        case AUTOFOCUS_STEP_REQUEST:
            /* Autofocus unlock for capture.
             * If the AF request is triggered by "unlockAutofocus()",
             * Send CANCEL afTrigger to F/W and start new AF scanning.
             */
            if (m_flagAutofocusLock == true) {
                m_AUTOFOCUS_MODE2AA_AFMODE(m_interenalAutoFocusMode, shot_ext);
                shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_CANCEL;
                m_flagAutofocusLock = false;
                m_autofocusStep = AUTOFOCUS_STEP_START;
            } else {
                shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_OFF;
                shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00;
                shot_ext->shot.ctl.aa.vendor_afmode_ext = AA_AFMODE_EXT_OFF;
                shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;

                /*
                 * assure triggering is valid
                 * case 0 : adjusted m_aaAFMode is AA_AFMODE_OFF
                 * case 1 : AUTOFOCUS_STEP_REQUESTs more than 3 times.
                 */
                if (m_aaAFMode == ::AA_AFMODE_OFF ||
                        AUTOFOCUS_WAIT_COUNT_STEP_REQUEST < m_stepRequestCount) {

                    if (AUTOFOCUS_WAIT_COUNT_STEP_REQUEST < m_stepRequestCount)
                        CLOGD("m_stepRequestCount(%d), force AUTOFOCUS_STEP_START",
                                 m_stepRequestCount);

                    m_stepRequestCount = 0;

                    m_autofocusStep = AUTOFOCUS_STEP_START;
                } else {
                    m_stepRequestCount++;
                }
            }

            break;
        case AUTOFOCUS_STEP_START:
            m_interenalAutoFocusMode = m_autoFocusMode;
            m_AUTOFOCUS_MODE2AA_AFMODE(m_autoFocusMode, shot_ext);

            if (m_interenalAutoFocusMode == AUTOFOCUS_MODE_TOUCH) {
                shot_ext->shot.ctl.aa.afRegions[0] = m_focusArea.x1;
                shot_ext->shot.ctl.aa.afRegions[1] = m_focusArea.y1;
                shot_ext->shot.ctl.aa.afRegions[2] = m_focusArea.x2;
                shot_ext->shot.ctl.aa.afRegions[3] = m_focusArea.y2;
                shot_ext->shot.ctl.aa.afRegions[4] = m_focusWeight;
            } else {
#ifdef USE_CP_FUSION_LIB
                shot_ext->shot.ctl.aa.afRegions[0] = m_focusArea.x1;
                shot_ext->shot.ctl.aa.afRegions[1] = m_focusArea.y1;
                shot_ext->shot.ctl.aa.afRegions[2] = m_focusArea.x2;
                shot_ext->shot.ctl.aa.afRegions[3] = m_focusArea.y2;
#else
                shot_ext->shot.ctl.aa.afRegions[0] = 0;
                shot_ext->shot.ctl.aa.afRegions[1] = 0;
                shot_ext->shot.ctl.aa.afRegions[2] = 0;
                shot_ext->shot.ctl.aa.afRegions[3] = 0;
                shot_ext->shot.ctl.aa.afRegions[4] = 1000;
#endif
                /* macro position */
                if (m_interenalAutoFocusMode == AUTOFOCUS_MODE_CONTINUOUS_PICTURE ||
                        m_interenalAutoFocusMode == AUTOFOCUS_MODE_MACRO ||
                        m_interenalAutoFocusMode == AUTOFOCUS_MODE_AUTO) {
                    if (m_macroPosition == AUTOFOCUS_MACRO_POSITION_CENTER)
                        shot_ext->shot.ctl.aa.afRegions[4] = AA_AFMODE_EXT_ADVANCED_MACRO_FOCUS;
                    else if(m_macroPosition == AUTOFOCUS_MACRO_POSITION_CENTER_UP)
                        shot_ext->shot.ctl.aa.afRegions[4] = AA_AFMODE_EXT_FOCUS_LOCATION;
                }
            }

#ifdef SAMSUNG_OT
            if (m_isOTstart) {
                shot_ext->shot.uctl.aaUd.af_data.focusState = m_OTfocusData.FocusState;
                shot_ext->shot.uctl.aaUd.af_data.focusROILeft = m_OTfocusData.FocusROILeft;
                shot_ext->shot.uctl.aaUd.af_data.focusROIRight = m_OTfocusData.FocusROIRight;
                shot_ext->shot.uctl.aaUd.af_data.focusROITop = m_OTfocusData.FocusROITop;
                shot_ext->shot.uctl.aaUd.af_data.focusROIBottom = m_OTfocusData.FocusROIBottom;
                shot_ext->shot.uctl.aaUd.af_data.focusWeight = m_OTfocusData.FocusWeight;
                shot_ext->shot.uctl.aaUd.af_data.w_movement = m_OTfocusData.W_Movement;
                shot_ext->shot.uctl.aaUd.af_data.h_movement = m_OTfocusData.H_Movement;
                shot_ext->shot.uctl.aaUd.af_data.w_velocity = m_OTfocusData.W_Velocity;
                shot_ext->shot.uctl.aaUd.af_data.h_velocity = m_OTfocusData.H_Velocity;
                CLOGV("[OBTR]Meta for AF Library, state: %d, x1: %d, x2: %d, y1: %d, y2: %d, weight: %d",
                         m_OTfocusData.FocusState,
                        m_OTfocusData.FocusROILeft, m_OTfocusData.FocusROIRight,
                        m_OTfocusData.FocusROITop, m_OTfocusData.FocusROIBottom, m_OTfocusData.FocusWeight);
            }
#endif

            CLOGD("AF-Mode(HAL/FW)=(%d/%d(%d)) AF-Region(x1,y1,x2,y2,weight)=(%d, %d, %d, %d, %d)",
                     m_interenalAutoFocusMode, shot_ext->shot.ctl.aa.afMode, shot_ext->shot.ctl.aa.vendor_afmode_option,
                    shot_ext->shot.ctl.aa.afRegions[0],
                    shot_ext->shot.ctl.aa.afRegions[1],
                    shot_ext->shot.ctl.aa.afRegions[2],
                    shot_ext->shot.ctl.aa.afRegions[3],
                    shot_ext->shot.ctl.aa.afRegions[4]);

            switch (m_interenalAutoFocusMode) {
                /* these affect directly */
                case AUTOFOCUS_MODE_INFINITY:
                case AUTOFOCUS_MODE_FIXED:
#ifdef SAMSUNG_FIXED_FACE_FOCUS
                case AUTOFOCUS_MODE_FIXED_FACE:
#endif
                /* These above mode may be considered like CAF. */
                /*
                   m_autofocusStep = AUTOFOCUS_STEP_DONE;
                   break;
                */
                /* these start scanning directrly */
                case AUTOFOCUS_MODE_CONTINUOUS_VIDEO:
                case AUTOFOCUS_MODE_CONTINUOUS_PICTURE:
                case AUTOFOCUS_MODE_CONTINUOUS_PICTURE_MACRO:
#ifdef SAMSUNG_OT
                case AUTOFOCUS_MODE_OBJECT_TRACKING_PICTURE:
                case AUTOFOCUS_MODE_OBJECT_TRACKING_VIDEO:
#endif
                    m_autofocusStep = AUTOFOCUS_STEP_SCANNING;
                    break;
                    /* these need to wait starting af */
                default:
                    m_autofocusStep = AUTOFOCUS_STEP_START_SCANNING;
                    break;
            }

            break;
        case AUTOFOCUS_STEP_START_SCANNING:
            m_AUTOFOCUS_MODE2AA_AFMODE(m_interenalAutoFocusMode, shot_ext);

            /* set TAF regions */
            if (m_interenalAutoFocusMode == AUTOFOCUS_MODE_TOUCH) {
                shot_ext->shot.ctl.aa.afRegions[0] = m_focusArea.x1;
                shot_ext->shot.ctl.aa.afRegions[1] = m_focusArea.y1;
                shot_ext->shot.ctl.aa.afRegions[2] = m_focusArea.x2;
                shot_ext->shot.ctl.aa.afRegions[3] = m_focusArea.y2;
                shot_ext->shot.ctl.aa.afRegions[4] = m_focusWeight;
            }

            if (currentState == AUTOFOCUS_STATE_SCANNING) {
                m_autofocusStep = AUTOFOCUS_STEP_SCANNING;
                m_waitCountFailState = 0;
            }

            break;
        case AUTOFOCUS_STEP_SCANNING:
            m_AUTOFOCUS_MODE2AA_AFMODE(m_interenalAutoFocusMode, shot_ext);

            /* set TAF regions */
            if (m_interenalAutoFocusMode == AUTOFOCUS_MODE_TOUCH) {
                shot_ext->shot.ctl.aa.afRegions[0] = m_focusArea.x1;
                shot_ext->shot.ctl.aa.afRegions[1] = m_focusArea.y1;
                shot_ext->shot.ctl.aa.afRegions[2] = m_focusArea.x2;
                shot_ext->shot.ctl.aa.afRegions[3] = m_focusArea.y2;
                shot_ext->shot.ctl.aa.afRegions[4] = m_focusWeight;
            }

#ifdef SAMSUNG_OT
            if (m_isOTstart) {
                shot_ext->shot.uctl.aaUd.af_data.focusState = m_OTfocusData.FocusState;
                shot_ext->shot.uctl.aaUd.af_data.focusROILeft = m_OTfocusData.FocusROILeft;
                shot_ext->shot.uctl.aaUd.af_data.focusROIRight = m_OTfocusData.FocusROIRight;
                shot_ext->shot.uctl.aaUd.af_data.focusROITop = m_OTfocusData.FocusROITop;
                shot_ext->shot.uctl.aaUd.af_data.focusROIBottom = m_OTfocusData.FocusROIBottom;
                shot_ext->shot.uctl.aaUd.af_data.focusWeight = m_OTfocusData.FocusWeight;
                shot_ext->shot.uctl.aaUd.af_data.w_movement = m_OTfocusData.W_Movement;
                shot_ext->shot.uctl.aaUd.af_data.h_movement = m_OTfocusData.H_Movement;
                shot_ext->shot.uctl.aaUd.af_data.w_velocity = m_OTfocusData.W_Velocity;
                shot_ext->shot.uctl.aaUd.af_data.h_velocity = m_OTfocusData.H_Velocity;
                CLOGV("[OBTR]Library state: %d, x1: %d, x2: %d, y1: %d, y2: %d, weight: %d",
                         m_OTfocusData.FocusState,
                        m_OTfocusData.FocusROILeft, m_OTfocusData.FocusROIRight,
                        m_OTfocusData.FocusROITop, m_OTfocusData.FocusROIBottom, m_OTfocusData.FocusWeight);
            }
#endif
            switch (m_interenalAutoFocusMode) {
                case AUTOFOCUS_MODE_CONTINUOUS_VIDEO:
                case AUTOFOCUS_MODE_CONTINUOUS_PICTURE:
                case AUTOFOCUS_MODE_CONTINUOUS_PICTURE_MACRO:
#ifdef SAMSUNG_OT
                case AUTOFOCUS_MODE_OBJECT_TRACKING_PICTURE:
                case AUTOFOCUS_MODE_OBJECT_TRACKING_VIDEO:
#endif
#ifdef SAMSUNG_FIXED_FACE_FOCUS
                case AUTOFOCUS_MODE_FIXED_FACE:
#endif
                    break;
                default:
                    if (currentState == AUTOFOCUS_STATE_SUCCEESS ||
                            currentState == AUTOFOCUS_STATE_FAIL) {

                        /* some times fail is happen on 3, 4, 5 count while scanning */
                        if (currentState == AUTOFOCUS_STATE_FAIL &&
                                m_waitCountFailState < WAIT_COUNT_FAIL_STATE) {
                            m_waitCountFailState++;
                            break;
                        }

                        m_waitCountFailState = 0;
                        m_autofocusStep = AUTOFOCUS_STEP_DONE;
                    } else {
                        m_waitCountFailState++;
                    }
                    break;
            }
            break;
        case AUTOFOCUS_STEP_DONE:
            /* to assure next AUTOFOCUS_MODE_AUTO and AUTOFOCUS_MODE_TOUCH */
            switch (m_interenalAutoFocusMode) {
            case AUTOFOCUS_MODE_TOUCH:
                /* only for AUTOFOCUS_MODE_TOUCH */
                shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_AUTO;
                break;
            default:
                shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_OFF;
                break;
            }
            shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00;
#ifdef USE_CP_FUSION_LIB
            shot_ext->shot.ctl.aa.vendor_afmode_option |= (1 << AA_AFMODE_OPTION_BIT_AF_ROI_NO_CONV);
#endif
            shot_ext->shot.ctl.aa.vendor_afmode_ext = AA_AFMODE_EXT_OFF;
            shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
            break;
        default:
            break;
    }

    return 1;
}

int ExynosCameraActivityAutofocus::t_func3AAfter(void *args)
{
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;

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
        (shot_ext->shot.dm.aa.afState != m_aaAfState) &&
        (shot_ext->shot.dm.aa.afState == AA_AFSTATE_PASSIVE_FOCUSED ||
         shot_ext->shot.dm.aa.afState == AA_AFSTATE_FOCUSED_LOCKED ||
         shot_ext->shot.dm.aa.afState == AA_AFSTATE_NOT_FOCUSED_LOCKED ||
         shot_ext->shot.dm.aa.afState == AA_AFSTATE_PASSIVE_UNFOCUSED)) {

        for(int i = 0; i < 10; i++) {
            m_af_pos[i] = (af_meta_short[19 + 8 * i] & 0xFFFF);
            m_af_filter[i] = ((af_meta_short[25 + 8 * i] & 0xFFFF)) |
                ((af_meta_short[26 + 8 * i] & 0xFFFF) << 16);
            m_af_filter[i] = m_af_filter[i] << 32;
            m_af_filter[i] += ((af_meta_short[23 + 8 * i] & 0xFFFF)) |
                ((af_meta_short[24 + 8 * i] & 0xFFFF) << 16);
        }
    }

    m_aaAfState = shot_ext->shot.dm.aa.afState;

    m_aaAFMode  = shot_ext->shot.ctl.aa.afMode;

    m_frameCount = shot_ext->shot.dm.request.frameCount;

    return true;
}

bool ExynosCameraActivityAutofocus::setAutofocusMode(int autoFocusMode)
{
    CLOGI("autoFocusMode(%d)", autoFocusMode);

    bool ret = true;

    switch(autoFocusMode) {
    case AUTOFOCUS_MODE_AUTO:
    case AUTOFOCUS_MODE_INFINITY:
    case AUTOFOCUS_MODE_MACRO:
    case AUTOFOCUS_MODE_FIXED:
    case AUTOFOCUS_MODE_EDOF:
    case AUTOFOCUS_MODE_CONTINUOUS_VIDEO:
    case AUTOFOCUS_MODE_CONTINUOUS_PICTURE:
    case AUTOFOCUS_MODE_CONTINUOUS_PICTURE_MACRO:
    case AUTOFOCUS_MODE_TOUCH:
#ifdef SAMSUNG_OT
    case AUTOFOCUS_MODE_OBJECT_TRACKING_PICTURE:
    case AUTOFOCUS_MODE_OBJECT_TRACKING_VIDEO:
#endif
#ifdef SAMSUNG_MANUAL_FOCUS
    case AUTOFOCUS_MODE_MANUAL:
#endif
#ifdef SAMSUNG_FIXED_FACE_FOCUS
    case AUTOFOCUS_MODE_FIXED_FACE:
#endif
        m_autoFocusMode = autoFocusMode;
        break;
    default:
        CLOGE("invalid focus mode(%d) fail", autoFocusMode);
        ret = false;
        break;
    }

    return ret;
}

#ifdef SAMSUNG_OT
bool ExynosCameraActivityAutofocus::setObjectTrackingAreas(UniPluginFocusData_t* focusData)
{
    if(focusData == NULL) {
        CLOGE(" NULL focusData is received!!");
        return false;
    }

    memcpy(&m_OTfocusData, focusData, sizeof(UniPluginFocusData_t));

    return true;
}

bool ExynosCameraActivityAutofocus::setObjectTrackingStart(bool isStart)
{
    m_isOTstart = isStart;

    return true;
}
#endif

bool ExynosCameraActivityAutofocus::getAutofocusResult(bool flagLockFocus, bool flagStartFaceDetection, int numOfFace)
{
    CLOGI("getAutofocusResult in m_autoFocusMode(%d)",
         m_autoFocusMode);

    bool ret = false;
    bool af_over = false;
    bool flagCheckStep = false;
    int  currentState = AUTOFOCUS_STATE_NONE;
    bool flagScanningStarted = false;
    int flagtrigger = true;

    unsigned int i = 0;

    unsigned int waitTimeoutFpsValue = 0;

    if (m_samsungCamera) {
        if (getFpsValue() > 0) {
            waitTimeoutFpsValue = 30 / getFpsValue();
        }
        if (waitTimeoutFpsValue < 1)
            waitTimeoutFpsValue = 1;
    } else {
        waitTimeoutFpsValue = 1;
    }

    CLOGI("waitTimeoutFpsValue(%d), getFpsValue(%d), flagStartFaceDetection(%d), numOfFace(%d)",
             waitTimeoutFpsValue, getFpsValue(), flagStartFaceDetection, numOfFace);

    for (i = 0; i < AUTOFOCUS_TOTAL_WATING_TIME * waitTimeoutFpsValue; i += AUTOFOCUS_WATING_TIME) {
        currentState = this->getCurrentState();

        /*
           * TRIGGER_START means that lock the AF state.
           */
        if(flagtrigger && flagLockFocus && (m_interenalAutoFocusMode == m_autoFocusMode)) {
            m_autofocusStep = AUTOFOCUS_STEP_TRIGGER_START;
            flagtrigger = false;
            CLOGI("m_aaAfState(%d) flagLockFocus(%d) m_interenalAutoFocusMode(%d) m_autoFocusMode(%d)",
                 m_aaAfState, flagLockFocus, m_interenalAutoFocusMode, m_autoFocusMode);
        }

        /* If stopAutofocus() called */
        if (m_autofocusStep == AUTOFOCUS_STEP_STOP
            && m_aaAfState == AA_AFSTATE_INACTIVE) {
            m_afState = AUTOFOCUS_STATE_FAIL;
            af_over = true;

            if (currentState == AUTOFOCUS_STATE_SUCCEESS)
                ret = true;
            else
                ret = false;

            break; /* break for for() loop */
        }

        switch (m_interenalAutoFocusMode) {
        case AUTOFOCUS_MODE_INFINITY:
        case AUTOFOCUS_MODE_FIXED:
            /* These above mode may be considered like CAF. */
            /*
            af_over = true;
            ret = true;
            break;
            */
        case AUTOFOCUS_MODE_CONTINUOUS_VIDEO:
        case AUTOFOCUS_MODE_CONTINUOUS_PICTURE:
        case AUTOFOCUS_MODE_CONTINUOUS_PICTURE_MACRO:
#ifdef SAMSUNG_OT
        case AUTOFOCUS_MODE_OBJECT_TRACKING_PICTURE:
        case AUTOFOCUS_MODE_OBJECT_TRACKING_VIDEO:
#endif
            if (m_autofocusStep == AUTOFOCUS_STEP_SCANNING
                || m_autofocusStep == AUTOFOCUS_STEP_DONE
                || m_autofocusStep == AUTOFOCUS_STEP_TRIGGER_START) {
                flagCheckStep = true;
            }
            break;
        default:
            if (m_autofocusStep == AUTOFOCUS_STEP_DONE)
                flagCheckStep = true;
            break;
        }

        if (flagCheckStep == true) {
            switch (currentState) {
            case AUTOFOCUS_STATE_NONE:
                if (flagScanningStarted == true)
                    CLOGW("AF restart is detected(%d)", i / 1000);

                if (m_interenalAutoFocusMode == AUTOFOCUS_MODE_CONTINUOUS_PICTURE) {
                    CLOGD("AF force-success on AUTOFOCUS_MODE_CONTINUOUS_PICTURE (%d)", i / 1000);
                    af_over = true;
                    ret = true;
                }
                break;
            case AUTOFOCUS_STATE_SCANNING:
                flagScanningStarted = true;
                break;
            case AUTOFOCUS_STATE_SUCCEESS:
                if (m_recordingHint == false && flagStartFaceDetection && numOfFace > 0) {
                    if (m_aaAfState == AA_AFSTATE_FOCUSED_LOCKED) {
                        af_over = true;
                        ret = true;
                    }
                } else {
                    af_over = true;
                    ret = true;
                }
                break;
            case AUTOFOCUS_STATE_FAIL:
                if (flagStartFaceDetection && numOfFace > 0) {
                    if (m_aaAfState == AA_AFSTATE_NOT_FOCUSED_LOCKED) {
                        af_over = true;
                        ret = false;
                    }
                } else {
                    af_over = true;
                    ret = false;
                }
                break;
             default:
                CLOGV("Invalid afState(%d)", currentState);
                ret = false;
                break;
            }
        }

        if (af_over == true)
            break;

        usleep(AUTOFOCUS_WATING_TIME);
    }

    if (AUTOFOCUS_TOTAL_WATING_TIME * waitTimeoutFpsValue <= i) {
        CLOGW("AF result time out(%d) msec", i * waitTimeoutFpsValue / 1000);
        stopAutofocus(); /* Reset Previous AF */
        m_afState = AUTOFOCUS_STATE_FAIL;
    }

    CLOGI("getAutofocusResult out m_autoFocusMode(%d) m_interenalAutoFocusMode(%d) result(%d) af_over(%d)",
         m_autoFocusMode, m_interenalAutoFocusMode, ret, af_over);

    return ret;
}

int ExynosCameraActivityAutofocus::getCAFResult(void)
{
    int ret = 0;

     /*
      * 0: fail
      * 1: success
      * 2: canceled
      * 3: focusing
      * 4: restart
      */

     static int  oldRet = AUTOFOCUS_RESULT_CANCEL;
     static bool flagCAFScannigStarted = false;

     switch (m_aaAFMode) {
     case AA_AFMODE_CONTINUOUS_VIDEO:
     case AA_AFMODE_CONTINUOUS_PICTURE:
     /* case AA_AFMODE_CONTINUOUS_VIDEO_FACE: */

         switch(m_aaAfState) {
         case AA_AFSTATE_INACTIVE:
            ret = AUTOFOCUS_RESULT_CANCEL;
            break;
         case AA_AFSTATE_PASSIVE_SCAN:
         case AA_AFSTATE_ACTIVE_SCAN:
            ret = AUTOFOCUS_RESULT_FOCUSING;
            break;
         case AA_AFSTATE_PASSIVE_FOCUSED:
         case AA_AFSTATE_FOCUSED_LOCKED:
            ret = AUTOFOCUS_RESULT_SUCCESS;
            break;
         case AA_AFSTATE_PASSIVE_UNFOCUSED:
            if (flagCAFScannigStarted == true)
                ret = AUTOFOCUS_RESULT_FAIL;
            else
                ret = oldRet;
            break;
         case AA_AFSTATE_NOT_FOCUSED_LOCKED:
             ret = AUTOFOCUS_RESULT_FAIL;
             break;
         default:
            CLOGE("invalid m_aaAfState");
            ret = AUTOFOCUS_RESULT_CANCEL;
            break;
         }

         if (m_aaAfState == AA_AFSTATE_ACTIVE_SCAN)
             flagCAFScannigStarted = true;
         else
             flagCAFScannigStarted = false;

         oldRet = ret;
         break;
     default:
         flagCAFScannigStarted = false;

         ret = oldRet;
         break;
     }

     return ret;
}

#ifdef SAMSUNG_DOF
bool ExynosCameraActivityAutofocus::setPDAF(bool toggle)
{
    if(m_flagPDAF != toggle) {
        CLOGI("toggle(%d)", toggle);
        m_flagPDAF = toggle;
    }

    return true;
}

bool ExynosCameraActivityAutofocus::setStartLensMove(bool toggle)
{
    CLOGI("toggle(%d)", toggle);

    m_flagLensMoveStart = toggle;
    return true;
}
#endif

#ifdef SUPPORT_MULTI_AF
void ExynosCameraActivityAutofocus::setMultiAf(bool enable)
{
    m_flagMultiAf = enable;
}

bool ExynosCameraActivityAutofocus::getMultiAf(void)
{
    return m_flagMultiAf;
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

void ExynosCameraActivityAutofocus::m_AUTOFOCUS_MODE2AA_AFMODE(int autoFocusMode, camera2_shot_ext *shot_ext)
{
    switch (autoFocusMode) {
    case AUTOFOCUS_MODE_AUTO:
        if (m_recordingHint == true) {
            /* CONTINUOUS_VIDEO */
            shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_CONTINUOUS_VIDEO;
            shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00
                | SET_BIT(AA_AFMODE_OPTION_BIT_VIDEO);
#ifdef SAMSUNG_OT
            if(m_isOTstart == true)
                shot_ext->shot.ctl.aa.vendor_afmode_option |= SET_BIT(AA_AFMODE_OPTION_BIT_OBJECT_TRACKING);
#endif
#ifdef SUPPORT_MULTI_AF
            if (m_flagMultiAf == true)
                shot_ext->shot.ctl.aa.vendor_afmode_option |= SET_BIT(AA_AFMODE_OPTION_BIT_MULTI_AF);
#endif
            shot_ext->shot.ctl.aa.vendor_afmode_ext = AA_AFMODE_EXT_OFF;
            shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
        } else if (m_flagFaceDetection == true) {
#ifdef SAMSUNG_DOF
            if (m_flagPDAF == true) {
                if (m_flagLensMoveStart == true) {
                    /* OFF */
                    shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_OFF;
                    shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00;
                    shot_ext->shot.ctl.aa.vendor_afmode_ext = AA_AFMODE_EXT_OFF;
                    shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
                } else {
                    /* PDAF_OUTFOCUSING_AUTO */
                    shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_AUTO;
                    shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00
                        | SET_BIT(AA_AFMODE_OPTION_BIT_OUT_FOCUSING);
#ifdef SUPPORT_MULTI_AF
                    if (m_flagMultiAf == true)
                        shot_ext->shot.ctl.aa.vendor_afmode_option |= SET_BIT(AA_AFMODE_OPTION_BIT_MULTI_AF);
#endif
                    shot_ext->shot.ctl.aa.vendor_afmode_ext = AA_AFMODE_EXT_OFF;
                    shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_START;
                }
            } else
#endif
            {
                /* AUTO_FACE */
                shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_AUTO;
                shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00
                    | SET_BIT(AA_AFMODE_OPTION_BIT_FACE);
#ifdef SUPPORT_MULTI_AF
                if (m_flagMultiAf == true)
                    shot_ext->shot.ctl.aa.vendor_afmode_option |= SET_BIT(AA_AFMODE_OPTION_BIT_MULTI_AF);
#endif
                shot_ext->shot.ctl.aa.vendor_afmode_ext = AA_AFMODE_EXT_OFF;
                shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_START;
            }
        } else {
#ifdef SAMSUNG_DOF
            if (m_flagPDAF == true) {
                if (m_flagLensMoveStart == true) {
                    /* OFF */
                    shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_OFF;
                    shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00;
                    shot_ext->shot.ctl.aa.vendor_afmode_ext = AA_AFMODE_EXT_OFF;
                    shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
                } else {
                    /* PDAF_OUTFOCUSING_AUTO */
                    shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_AUTO;
                    shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00
                        | SET_BIT(AA_AFMODE_OPTION_BIT_OUT_FOCUSING);
#ifdef SUPPORT_MULTI_AF
                    if (m_flagMultiAf == true)
                        shot_ext->shot.ctl.aa.vendor_afmode_option |= SET_BIT(AA_AFMODE_OPTION_BIT_MULTI_AF);
#endif
                    shot_ext->shot.ctl.aa.vendor_afmode_ext = AA_AFMODE_EXT_OFF;
                    shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_START;
                }
            } else
#endif
            {
                /* AUTO */
                shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_AUTO;
                shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00;
#ifdef SAMSUNG_OT
                if(m_isOTstart == true)
                    shot_ext->shot.ctl.aa.vendor_afmode_option |= SET_BIT(AA_AFMODE_OPTION_BIT_OBJECT_TRACKING);
#endif
#ifdef SUPPORT_MULTI_AF
                if (m_flagMultiAf == true)
                    shot_ext->shot.ctl.aa.vendor_afmode_option |= SET_BIT(AA_AFMODE_OPTION_BIT_MULTI_AF);
#endif
                shot_ext->shot.ctl.aa.vendor_afmode_ext = AA_AFMODE_EXT_OFF;
                shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_START;
            }
        }
        break;
    case AUTOFOCUS_MODE_INFINITY:
        /* INFINITY */
        shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_AUTO;
        shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00;
        shot_ext->shot.ctl.aa.vendor_afmode_ext = AA_AFMODE_EXT_OFF;
        shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_CANCEL;
        break;
    case AUTOFOCUS_MODE_MACRO:
        /* MACRO */
        shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_MACRO;
        shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00
            | SET_BIT(AA_AFMODE_OPTION_BIT_MACRO);
#ifdef SUPPORT_MULTI_AF
        if (m_flagMultiAf == true)
            shot_ext->shot.ctl.aa.vendor_afmode_option |= SET_BIT(AA_AFMODE_OPTION_BIT_MULTI_AF);
#endif
        shot_ext->shot.ctl.aa.vendor_afmode_ext = AA_AFMODE_EXT_OFF;
        shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_START;
        break;
    case AUTOFOCUS_MODE_EDOF:
        /* EDOF */
        shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_EDOF;
        break;
    case AUTOFOCUS_MODE_CONTINUOUS_VIDEO:
        /* CONTINUOUS_VIDEO */
        shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_CONTINUOUS_VIDEO;
        shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00
            | SET_BIT(AA_AFMODE_OPTION_BIT_VIDEO);
#ifdef SUPPORT_MULTI_AF
        if (m_flagMultiAf == true)
            shot_ext->shot.ctl.aa.vendor_afmode_option |= SET_BIT(AA_AFMODE_OPTION_BIT_MULTI_AF);
#endif
        shot_ext->shot.ctl.aa.vendor_afmode_ext = AA_AFMODE_EXT_OFF;
        shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
        break;
    case AUTOFOCUS_MODE_CONTINUOUS_PICTURE:
    case AUTOFOCUS_MODE_CONTINUOUS_PICTURE_MACRO:
        if (m_flagFaceDetection == true) {
#ifdef SAMSUNG_DOF
            if (m_flagPDAF == true) {
                if (m_flagLensMoveStart == true) {
                    /* OFF */
                    shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_OFF;
                    shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00;
                    shot_ext->shot.ctl.aa.vendor_afmode_ext = AA_AFMODE_EXT_OFF;
                    shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
                } else {
                    /* PDAF_OUTFOCUSING_CONTINUOUS_PICTURE */
                    shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_CONTINUOUS_PICTURE;
                    shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00
                        | SET_BIT(AA_AFMODE_OPTION_BIT_OUT_FOCUSING);
#ifdef SUPPORT_MULTI_AF
                    if (m_flagMultiAf == true)
                        shot_ext->shot.ctl.aa.vendor_afmode_option |= SET_BIT(AA_AFMODE_OPTION_BIT_MULTI_AF);
#endif
                    shot_ext->shot.ctl.aa.vendor_afmode_ext = AA_AFMODE_EXT_OFF;
                    shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
                }
            } else
#endif
            {
                /* CONTINUOUS_PICTURE_FACE */
                shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_CONTINUOUS_PICTURE;
                shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00
                    | SET_BIT(AA_AFMODE_OPTION_BIT_FACE);
#ifdef SUPPORT_MULTI_AF
                if (m_flagMultiAf == true)
                    shot_ext->shot.ctl.aa.vendor_afmode_option |= SET_BIT(AA_AFMODE_OPTION_BIT_MULTI_AF);
#endif
                shot_ext->shot.ctl.aa.vendor_afmode_ext = AA_AFMODE_EXT_OFF;
                shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
            }
        } else {
#ifdef SAMSUNG_DOF
            if (m_flagPDAF == true) {
                if (m_flagLensMoveStart == true) {
                    /* OFF */
                    shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_OFF;
                    shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00;
                    shot_ext->shot.ctl.aa.vendor_afmode_ext = AA_AFMODE_EXT_OFF;
                    shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
                } else {
                    /* PDAF_OUTFOCUSING_CONTINUOUS_PICTURE */
                    shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_CONTINUOUS_PICTURE;
                    shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00
                        | SET_BIT(AA_AFMODE_OPTION_BIT_OUT_FOCUSING);
#ifdef SUPPORT_MULTI_AF
                    if (m_flagMultiAf == true)
                        shot_ext->shot.ctl.aa.vendor_afmode_option |= SET_BIT(AA_AFMODE_OPTION_BIT_MULTI_AF);
#endif
                    shot_ext->shot.ctl.aa.vendor_afmode_ext = AA_AFMODE_EXT_OFF;
                    shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
                }
            } else
#endif
            {
                /* CONTINUOUS_PICTURE */
                shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_CONTINUOUS_PICTURE;
                shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00;
#ifdef SUPPORT_MULTI_AF
                if (m_flagMultiAf == true)
                    shot_ext->shot.ctl.aa.vendor_afmode_option |= SET_BIT(AA_AFMODE_OPTION_BIT_MULTI_AF);
#endif
                shot_ext->shot.ctl.aa.vendor_afmode_ext = AA_AFMODE_EXT_OFF;
                shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
            }
        }
        break;
    case AUTOFOCUS_MODE_TOUCH:
        if (m_recordingHint == true) {
            /* CONTINUOUS_VIDEO */
            shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_CONTINUOUS_VIDEO;
            shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00
                | SET_BIT(AA_AFMODE_OPTION_BIT_VIDEO);
#ifdef SUPPORT_MULTI_AF
            if (m_flagMultiAf == true)
                shot_ext->shot.ctl.aa.vendor_afmode_option |= SET_BIT(AA_AFMODE_OPTION_BIT_MULTI_AF);
#endif
            shot_ext->shot.ctl.aa.vendor_afmode_ext = AA_AFMODE_EXT_OFF;
            shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
        } else {
#ifdef SAMSUNG_DOF
            if (m_flagPDAF == true) {
                if (m_flagLensMoveStart == true) {
                    /* OFF */
                    shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_OFF;
                    shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00;
                    shot_ext->shot.ctl.aa.vendor_afmode_ext = AA_AFMODE_EXT_OFF;
                    shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
                } else {
                    /* PDAF_OUTFOCUSING_AUTO */
                    shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_AUTO;
                    shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00
                        | SET_BIT(AA_AFMODE_OPTION_BIT_OUT_FOCUSING);
#ifdef SUPPORT_MULTI_AF
                    if (m_flagMultiAf == true)
                        shot_ext->shot.ctl.aa.vendor_afmode_option |= SET_BIT(AA_AFMODE_OPTION_BIT_MULTI_AF);
#endif
                    shot_ext->shot.ctl.aa.vendor_afmode_ext = AA_AFMODE_EXT_OFF;
                    shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_START;
                }
            } else
#endif
            {
                /* AUTO */
                shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_AUTO;
                shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00;
#ifdef SUPPORT_MULTI_AF
                if (m_flagMultiAf == true)
                    shot_ext->shot.ctl.aa.vendor_afmode_option |= SET_BIT(AA_AFMODE_OPTION_BIT_MULTI_AF);
#endif
                shot_ext->shot.ctl.aa.vendor_afmode_ext = AA_AFMODE_EXT_OFF;
                shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_START;
            }
        }
        break;
#ifdef SAMSUNG_OT
    case AUTOFOCUS_MODE_OBJECT_TRACKING_VIDEO:
        shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_CONTINUOUS_VIDEO;
        shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00
                | SET_BIT(AA_AFMODE_OPTION_BIT_VIDEO)
                | SET_BIT(AA_AFMODE_OPTION_BIT_OBJECT_TRACKING);
#ifdef SUPPORT_MULTI_AF
        if (m_flagMultiAf == true)
            shot_ext->shot.ctl.aa.vendor_afmode_option |= SET_BIT(AA_AFMODE_OPTION_BIT_MULTI_AF);
#endif
        shot_ext->shot.ctl.aa.vendor_afmode_ext = AA_AFMODE_EXT_OFF;
        shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
        break;
    case AUTOFOCUS_MODE_OBJECT_TRACKING_PICTURE:
        shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_CONTINUOUS_PICTURE;
        shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00
                | SET_BIT(AA_AFMODE_OPTION_BIT_OBJECT_TRACKING);
#ifdef SUPPORT_MULTI_AF
        if (m_flagMultiAf == true)
            shot_ext->shot.ctl.aa.vendor_afmode_option |= SET_BIT(AA_AFMODE_OPTION_BIT_MULTI_AF);
#endif
        shot_ext->shot.ctl.aa.vendor_afmode_ext = AA_AFMODE_EXT_OFF;
        shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
        break;
#endif
#ifdef SAMSUNG_MANUAL_FOCUS
    case AUTOFOCUS_MODE_MANUAL:
        shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_OFF;
        shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00;
        shot_ext->shot.ctl.aa.vendor_afmode_ext = AA_AFMODE_EXT_OFF;
        shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
        break;
#endif
#ifdef SAMSUNG_FIXED_FACE_FOCUS
    case AUTOFOCUS_MODE_FIXED_FACE:
        shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_OFF;
        shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00;
        shot_ext->shot.ctl.aa.vendor_afmode_ext = AA_AFMODE_EXT_FIXED_FACE;
        shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
        break;
#endif
    case AUTOFOCUS_MODE_FIXED:
        break;
    default:
        CLOGE("invalid focus mode (%d)", autoFocusMode);
        break;
    }

#ifdef USE_CP_FUSION_LIB
    shot_ext->shot.ctl.aa.vendor_afmode_option |= (1 << AA_AFMODE_OPTION_BIT_AF_ROI_NO_CONV);
#endif

}

} /* namespace android */
