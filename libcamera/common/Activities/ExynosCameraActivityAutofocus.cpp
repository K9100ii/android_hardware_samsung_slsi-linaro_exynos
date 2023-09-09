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
#define LOG_TAG "ExynosCameraActivityAutofocus"
#include <cutils/log.h>

#include "ExynosCameraActivityAutofocus.h"

namespace android {

#define WAIT_COUNT_FAIL_STATE                (7)
#define AUTOFOCUS_WAIT_COUNT_STEP_REQUEST    (3)

#define AUTOFOCUS_WAIT_COUNT_FRAME_COUNT_NUM (3)       /* n + x frame count */
#define AUTOFOCUS_WATING_TIME_LOCK_AF        (10000)   /* 10msec */
#define AUTOFOCUS_TOTAL_WATING_TIME_LOCK_AF  (300000)  /* 300msec */
#define AUTOFOCUS_SKIP_FRAME_LOCK_AF         (6)       /* == NUM_BAYER_BUFFERS */

#define SET_BIT(x)      (1 << x)

ExynosCameraActivityAutofocus::ExynosCameraActivityAutofocus()
{
    m_flagAutofocusStart = false;
    m_flagAutofocusLock = false;

    /* first Lens position is infinity */
    /* m_autoFocusMode = AUTOFOCUS_MODE_BASE; */
    m_autoFocusMode = AUTOFOCUS_MODE_INFINITY;
    m_interenalAutoFocusMode = AUTOFOCUS_MODE_BASE;

    m_focusWeight = 0;
    /* first AF operation is trigger infinity mode */
    /* m_autofocusStep = AUTOFOCUS_STEP_STOP; */
    m_autofocusStep = AUTOFOCUS_STEP_REQUEST;
    m_aaAfState = ::AA_AFSTATE_INACTIVE;
    m_afState = AUTOFOCUS_STATE_NONE;
    m_aaAFMode = ::AA_AFMODE_OFF;
    m_metaCtlAFMode = -1;
    m_waitCountFailState = 0;
    m_stepRequestCount = 0;
    m_frameCount = 0;

    m_recordingHint = false;
    m_flagFaceDetection = false;
#ifdef SAMSUNG_DOF
    m_flagPDAF = false;
    m_flagLensMoveStart = false;
#endif
#ifdef SAMSUNG_OT
    m_isOTstart = false;
#endif
    m_macroPosition = AUTOFOCUS_MACRO_POSITION_BASE;
    m_fpsValue = 0;
    m_samsungCamera = false;
    m_afInMotionResult = false;
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

int ExynosCameraActivityAutofocus::t_func3ABefore(void *args)
{
#if defined(USE_HAL3_2_METADATA_INTERFACE)
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;

    camera2_shot_ext *shot_ext;
    shot_ext = (struct camera2_shot_ext *)(buf->addr[1]);

    if (shot_ext == NULL) {
        ALOGE("ERR(%s[%d]):shot_ext is null", __FUNCTION__, __LINE__);
        return false;
    }

    int currentState = this->getCurrentState();

    shot_ext->shot.ctl.aa.vendor_afState = (enum aa_afstate)m_aaAfState;

    switch (m_autofocusStep) {
        case AUTOFOCUS_STEP_STOP:
            /* m_interenalAutoFocusMode = m_autoFocusMode; */
            shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_OFF;
            shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00;
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
                shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;

                /*
                 * assure triggering is valid
                 * case 0 : adjusted m_aaAFMode is AA_AFMODE_OFF
                 * case 1 : AUTOFOCUS_STEP_REQUESTs more than 3 times.
                 */
                if (m_aaAFMode == ::AA_AFMODE_OFF ||
                        AUTOFOCUS_WAIT_COUNT_STEP_REQUEST < m_stepRequestCount) {

                    if (AUTOFOCUS_WAIT_COUNT_STEP_REQUEST < m_stepRequestCount)
                        ALOGD("DEBUG(%s[%d]):m_stepRequestCount(%d), force AUTOFOCUS_STEP_START",
                                __FUNCTION__, __LINE__, m_stepRequestCount);

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
                shot_ext->shot.ctl.aa.afRegions[0] = 0;
                shot_ext->shot.ctl.aa.afRegions[1] = 0;
                shot_ext->shot.ctl.aa.afRegions[2] = 0;
                shot_ext->shot.ctl.aa.afRegions[3] = 0;
                shot_ext->shot.ctl.aa.afRegions[4] = 1000;

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
            if (m_interenalAutoFocusMode == AUTOFOCUS_MODE_OBJECT_TRACKING_PICTURE ||
                    m_interenalAutoFocusMode == AUTOFOCUS_MODE_OBJECT_TRACKING_VIDEO) {
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
                ALOGV("[OBTR](%s[%d])Meta for AF Library, state: %d, x1: %d, x2: %d, y1: %d, y2: %d, weight: %d",
                        __FUNCTION__, __LINE__, m_OTfocusData.FocusState,
                        m_OTfocusData.FocusROILeft, m_OTfocusData.FocusROIRight,
                        m_OTfocusData.FocusROITop, m_OTfocusData.FocusROIBottom, m_OTfocusData.FocusWeight);
            }
#endif

            ALOGD("DEBUG(%s[%d]):AF-Mode(HAL/FW)=(%d/%d(%d)) AF-Region(x1,y1,x2,y2,weight)=(%d, %d, %d, %d, %d)",
                    __FUNCTION__, __LINE__, m_interenalAutoFocusMode, shot_ext->shot.ctl.aa.afMode, shot_ext->shot.ctl.aa.vendor_afmode_option,
                    shot_ext->shot.ctl.aa.afRegions[0],
                    shot_ext->shot.ctl.aa.afRegions[1],
                    shot_ext->shot.ctl.aa.afRegions[2],
                    shot_ext->shot.ctl.aa.afRegions[3],
                    shot_ext->shot.ctl.aa.afRegions[4]);

            switch (m_interenalAutoFocusMode) {
                /* these affect directly */
                case AUTOFOCUS_MODE_INFINITY:
                case AUTOFOCUS_MODE_FIXED:
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
            if (m_interenalAutoFocusMode == AUTOFOCUS_MODE_OBJECT_TRACKING_VIDEO ||
                    m_interenalAutoFocusMode == AUTOFOCUS_MODE_OBJECT_TRACKING_PICTURE ) {
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
                ALOGV("[OBTR](%s[%d])Library state: %d, x1: %d, x2: %d, y1: %d, y2: %d, weight: %d",
                        __FUNCTION__, __LINE__, m_OTfocusData.FocusState,
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
            shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_OFF;
            shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00;
            shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
            break;
        default:
            break;
    }
#else
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;

    camera2_shot_ext *shot_ext;
    shot_ext = (struct camera2_shot_ext *)(buf->addr[1]);

    if (shot_ext == NULL) {
        ALOGE("ERR(%s[%d]):shot_ext is null", __FUNCTION__, __LINE__);
        return false;
    }

    int currentState = this->getCurrentState();

    switch (m_autofocusStep) {
    case AUTOFOCUS_STEP_STOP:
        /* m_interenalAutoFocusMode = m_autoFocusMode; */

        shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_OFF;
        shot_ext->shot.ctl.aa.afTrigger = 0;
        break;
    case AUTOFOCUS_STEP_DELAYED_STOP:
        /* Autofocus lock for capture */
        shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_DELAYED_OFF;
        shot_ext->shot.ctl.aa.afTrigger = 0;
        break;
    case AUTOFOCUS_STEP_REQUEST:
        shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_OFF;
        shot_ext->shot.ctl.aa.afTrigger = 0;

        /*
         * assure triggering is valid
         * case 0 : adjusted m_aaAFMode is AA_AFMODE_OFF
         * case 1 : AUTOFOCUS_STEP_REQUESTs more than 3 times.
         */
        if (m_aaAFMode == ::AA_AFMODE_OFF ||
            AUTOFOCUS_WAIT_COUNT_STEP_REQUEST < m_stepRequestCount) {

            if (AUTOFOCUS_WAIT_COUNT_STEP_REQUEST < m_stepRequestCount)
                ALOGD("DEBUG(%s[%d]):m_stepRequestCount(%d), force AUTOFOCUS_STEP_START",
                    __FUNCTION__, __LINE__, m_stepRequestCount);

            m_stepRequestCount = 0;

            m_autofocusStep = AUTOFOCUS_STEP_START;
        } else {
            m_stepRequestCount++;
        }

        break;
    case AUTOFOCUS_STEP_START:
        m_interenalAutoFocusMode = m_autoFocusMode;
        m_metaCtlAFMode = m_AUTOFOCUS_MODE2AA_AFMODE(m_autoFocusMode);
        if (m_metaCtlAFMode < 0) {
            ALOGE("ERR(%s):m_AUTOFOCUS_MODE2AA_AFMODE(%d) fail", __FUNCTION__, m_autoFocusMode);
            m_autofocusStep = AUTOFOCUS_STEP_STOP;
            break;
        }

        shot_ext->shot.ctl.aa.afMode = (enum aa_afmode)m_metaCtlAFMode;
        shot_ext->shot.ctl.aa.afTrigger = 1;

        if (m_interenalAutoFocusMode == AUTOFOCUS_MODE_TOUCH) {
            shot_ext->shot.ctl.aa.afRegions[0] = m_focusArea.x1;
            shot_ext->shot.ctl.aa.afRegions[1] = m_focusArea.y1;
            shot_ext->shot.ctl.aa.afRegions[2] = m_focusArea.x2;
            shot_ext->shot.ctl.aa.afRegions[3] = m_focusArea.y2;
            shot_ext->shot.ctl.aa.afRegions[4] = m_focusWeight;
        } else {
            shot_ext->shot.ctl.aa.afRegions[0] = 0;
            shot_ext->shot.ctl.aa.afRegions[1] = 0;
            shot_ext->shot.ctl.aa.afRegions[2] = 0;
            shot_ext->shot.ctl.aa.afRegions[3] = 0;
            shot_ext->shot.ctl.aa.afRegions[4] = 1000;

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
        if (m_interenalAutoFocusMode == AUTOFOCUS_MODE_OBJECT_TRACKING_PICTURE ||
            m_interenalAutoFocusMode == AUTOFOCUS_MODE_OBJECT_TRACKING_VIDEO) {
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
            ALOGV("[OBTR](%s[%d])Meta for AF Library, state: %d, x1: %d, x2: %d, y1: %d, y2: %d, weight: %d",
                        __FUNCTION__, __LINE__, m_OTfocusData.FocusState,
                        m_OTfocusData.FocusROILeft, m_OTfocusData.FocusROIRight,
                        m_OTfocusData.FocusROITop, m_OTfocusData.FocusROIBottom, m_OTfocusData.FocusWeight);
        }
#endif

        ALOGD("DEBUG(%s[%d]):AF-Mode(HAL/FW)=(%d/%d) AF-Region(x1,y1,x2,y2,weight)=(%d, %d, %d, %d, %d)",
            __FUNCTION__, __LINE__, m_interenalAutoFocusMode, m_metaCtlAFMode,
            shot_ext->shot.ctl.aa.afRegions[0],
            shot_ext->shot.ctl.aa.afRegions[1],
            shot_ext->shot.ctl.aa.afRegions[2],
            shot_ext->shot.ctl.aa.afRegions[3],
            shot_ext->shot.ctl.aa.afRegions[4]);

        switch (m_interenalAutoFocusMode) {
        /* these affect directly */
        case AUTOFOCUS_MODE_INFINITY:
        case AUTOFOCUS_MODE_FIXED:
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
        shot_ext->shot.ctl.aa.afMode = (enum aa_afmode)m_metaCtlAFMode;

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
        shot_ext->shot.ctl.aa.afMode = (enum aa_afmode)m_metaCtlAFMode;

        /* set TAF regions */
        if (m_interenalAutoFocusMode == AUTOFOCUS_MODE_TOUCH) {
            shot_ext->shot.ctl.aa.afRegions[0] = m_focusArea.x1;
            shot_ext->shot.ctl.aa.afRegions[1] = m_focusArea.y1;
            shot_ext->shot.ctl.aa.afRegions[2] = m_focusArea.x2;
            shot_ext->shot.ctl.aa.afRegions[3] = m_focusArea.y2;
            shot_ext->shot.ctl.aa.afRegions[4] = m_focusWeight;
        }

#ifdef SAMSUNG_OT
        if (m_interenalAutoFocusMode == AUTOFOCUS_MODE_OBJECT_TRACKING_VIDEO ||
            m_interenalAutoFocusMode == AUTOFOCUS_MODE_OBJECT_TRACKING_PICTURE ) {
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
            ALOGV("[OBTR](%s[%d])Library state: %d, x1: %d, x2: %d, y1: %d, y2: %d, weight: %d",
                        __FUNCTION__, __LINE__, m_OTfocusData.FocusState,
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
        shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_OFF;
        shot_ext->shot.ctl.aa.afTrigger = 0;
        break;
    default:
        break;
    }
#endif
    return 1;
}

int ExynosCameraActivityAutofocus::t_func3AAfter(void *args)
{
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;

    camera2_shot_ext *shot_ext;
    shot_ext = (struct camera2_shot_ext *)(buf->addr[1]);

    if (shot_ext == NULL) {
        ALOGE("ERR(%s[%d]):shot_ext is null", __FUNCTION__, __LINE__);
        return false;
    }

    m_af_mode_info = shot_ext->shot.udm.af.vendorSpecific[1]&0xFFFF;
    m_af_pan_focus_info = (shot_ext->shot.udm.af.vendorSpecific[1]>>16)&0xFFFF;
    m_af_typical_macro_info = shot_ext->shot.udm.af.vendorSpecific[2]&0xFFFF;
    m_af_module_version_info = (shot_ext->shot.udm.af.vendorSpecific[2]>>16)&0xFFFF;
    m_af_state_info = shot_ext->shot.udm.af.vendorSpecific[3]&0xFFFF;
    m_af_cur_pos_info = (shot_ext->shot.udm.af.vendorSpecific[3]>>16)&0xFFFF;
    m_af_time_info = shot_ext->shot.udm.af.vendorSpecific[4]&0xFFFF;
    m_af_factory_info = (shot_ext->shot.udm.af.vendorSpecific[4]>>16)&0xFFFF;
    m_paf_from_info = shot_ext->shot.udm.af.vendorSpecific[5];

    m_aaAfState = shot_ext->shot.dm.aa.afState;

    m_aaAFMode  = shot_ext->shot.ctl.aa.afMode;

    m_frameCount = shot_ext->shot.dm.request.frameCount;

    return true;
}

#ifdef USE_HAL3_2_METADATA_INTERFACE
int ExynosCameraActivityAutofocus::t_func3ABeforeHAL3(__unused void *args)
{
    return 1;
}

int ExynosCameraActivityAutofocus::t_func3AAfterHAL3(__unused void *args)
{
    return 1;
}
#endif

int ExynosCameraActivityAutofocus::t_funcSCPBefore(__unused void *args)
{
    return 1;
}

int ExynosCameraActivityAutofocus::t_funcSCPAfter(__unused void *args)
{
    return 1;
}

int ExynosCameraActivityAutofocus::t_funcSCCBefore(__unused void *args)
{
    return 1;
}

int ExynosCameraActivityAutofocus::t_funcSCCAfter(__unused void *args)
{
    return 1;
}

bool ExynosCameraActivityAutofocus::setAutofocusMode(int autoFocusMode)
{
    ALOGI("INFO(%s[%d]):autoFocusMode(%d)", __FUNCTION__, __LINE__, autoFocusMode);

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
        m_autoFocusMode = autoFocusMode;
        break;
    default:
        ALOGE("ERR(%s):invalid focus mode(%d) fail", __FUNCTION__, autoFocusMode);
        ret = false;
        break;
    }

    return ret;
}

int ExynosCameraActivityAutofocus::getAutofocusMode(void)
{
    return m_autoFocusMode;
}

bool ExynosCameraActivityAutofocus::getRecordingHint(void)
{
    return m_recordingHint;
}

bool ExynosCameraActivityAutofocus::setFocusAreas(ExynosRect2 rect, int weight)
{
    m_focusArea = rect;
    m_focusWeight = weight;

    return true;
}

bool ExynosCameraActivityAutofocus::getFocusAreas(ExynosRect2 *rect, int *weight)
{
    *rect = m_focusArea;
    *weight = m_focusWeight;

    return true;
}

#ifdef SAMSUNG_OT
bool ExynosCameraActivityAutofocus::setObjectTrackingAreas(UniPluginFocusData_t* focusData)
{
    if(focusData == NULL) {
        ALOGE("INFO(%s[%d]): NULL focusData is received!!", __FUNCTION__, __LINE__);
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

bool ExynosCameraActivityAutofocus::startAutofocus(void)
{
    ALOGI("INFO(%s[%d]):m_autoFocusMode(%d)", __FUNCTION__, __LINE__, m_autoFocusMode);

    m_autofocusStep = AUTOFOCUS_STEP_REQUEST;
    m_flagAutofocusStart = true;

    return true;
}

bool ExynosCameraActivityAutofocus::stopAutofocus(void)
{
    ALOGI("INFO(%s[%d]):m_autoFocusMode(%d)", __FUNCTION__, __LINE__, m_autoFocusMode);

    m_autofocusStep = AUTOFOCUS_STEP_STOP;
    m_flagAutofocusStart = false;

    return true;
}

bool ExynosCameraActivityAutofocus::flagAutofocusStart(void)
{
    return m_flagAutofocusStart;
}

bool ExynosCameraActivityAutofocus::lockAutofocus()
{
    ALOGI("INFO(%s[%d]):m_autoFocusMode(%d)", __FUNCTION__, __LINE__, m_autoFocusMode);

#if defined(USE_HAL3_2_METADATA_INTERFACE)
    if(m_autofocusStep != AUTOFOCUS_STEP_TRIGGER_START) {
        m_autofocusStep = AUTOFOCUS_STEP_TRIGGER_START;

        ALOGI("INFO(%s): request locked state of Focus. : m_autofocusStep(%d), m_aaAfState(%d)",
            __FUNCTION__, m_autofocusStep, m_aaAfState);
    }
#else
    m_autofocusStep = AUTOFOCUS_STEP_STOP;
#endif
    m_flagAutofocusStart = false;

    if (m_aaAfState == AA_AFSTATE_INACTIVE ||
        m_aaAfState == AA_AFSTATE_PASSIVE_SCAN ||
        m_aaAfState == AA_AFSTATE_ACTIVE_SCAN) {
        /*
         * hold, until + 3 Frame
         * n (lockFrameCount) : n - 1's state
         * n + 1              : adjust on f/w
         * n + 2              : adjust on sensor
         * n + 3              : result
         */
        int lockFrameCount = m_frameCount;
        unsigned int i = 0;
        bool flagScanningDetected = false;
        int  scanningDetectedFrameCount = 0;

        for (i = 0; i < AUTOFOCUS_TOTAL_WATING_TIME_LOCK_AF; i += AUTOFOCUS_WATING_TIME_LOCK_AF) {
            if (lockFrameCount + AUTOFOCUS_WAIT_COUNT_FRAME_COUNT_NUM <= m_frameCount) {
                ALOGD("DEBUG(%s):find lockFrameCount(%d) + %d, m_frameCount(%d), m_aaAfState(%d)",
                    __FUNCTION__, lockFrameCount, AUTOFOCUS_WAIT_COUNT_FRAME_COUNT_NUM, m_frameCount, m_aaAfState);
                break;
            }

            if (flagScanningDetected == false) {
                if (m_aaAfState == AA_AFSTATE_PASSIVE_SCAN ||
                    m_aaAfState == AA_AFSTATE_ACTIVE_SCAN) {
                    flagScanningDetected = true;
                    scanningDetectedFrameCount = m_frameCount;
                }
            }

            usleep(AUTOFOCUS_WATING_TIME_LOCK_AF);
        }

        if (AUTOFOCUS_TOTAL_WATING_TIME_LOCK_AF <= i) {
            ALOGW("WARN(%s):AF lock time out (%d)msec", __FUNCTION__, i / 1000);
        } else {
            /* skip bayer frame when scanning detected */
            if (flagScanningDetected == true) {
                for (i = 0; i < AUTOFOCUS_TOTAL_WATING_TIME_LOCK_AF; i += AUTOFOCUS_WATING_TIME_LOCK_AF) {
                    if (scanningDetectedFrameCount + AUTOFOCUS_SKIP_FRAME_LOCK_AF <= m_frameCount) {
                        ALOGD("DEBUG(%s):kcoolsw find scanningDetectedFrameCount(%d) + %d, m_frameCount(%d), m_aaAfState(%d)",
                            __FUNCTION__, scanningDetectedFrameCount, AUTOFOCUS_SKIP_FRAME_LOCK_AF, m_frameCount, m_aaAfState);
                        break;
                    }

                    usleep(AUTOFOCUS_WATING_TIME_LOCK_AF);
                }

                if (AUTOFOCUS_TOTAL_WATING_TIME_LOCK_AF <= i)
                    ALOGW("WARN(%s):kcoolsw scanningDectected skip time out (%d)msec", __FUNCTION__, i / 1000);
            }
        }
    }

    m_flagAutofocusLock = true;

    return true;
}

bool ExynosCameraActivityAutofocus::unlockAutofocus()
{
    ALOGI("INFO(%s[%d]):m_autoFocusMode(%d)", __FUNCTION__, __LINE__, m_autoFocusMode);

#ifdef USE_HAL3_2_METADATA_INTERFACE
    /*
     * With the 3.2 metadata interface,
     * unlockAutofocus() triggers the new AF scanning.
     */
    m_flagAutofocusStart = true;
    m_autofocusStep = AUTOFOCUS_STEP_REQUEST;
#else
    m_flagAutofocusLock = false;
#endif

    return true;
}

bool ExynosCameraActivityAutofocus::flagLockAutofocus(void)
{
    return m_flagAutofocusLock;
}

bool ExynosCameraActivityAutofocus::getAutofocusResult(bool flagLockFocus, bool flagStartFaceDetection, int numOfFace)
{
    ALOGI("INFO(%s[%d]):getAutofocusResult in m_autoFocusMode(%d)",
        __FUNCTION__, __LINE__, m_autoFocusMode);

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

    ALOGI("INFO(%s[%d]):waitTimeoutFpsValue(%d) , getFpsValue(%d) flagStartFaceDetection(%d), numOfFace(%d)",
        __FUNCTION__, __LINE__, waitTimeoutFpsValue, getFpsValue(), flagStartFaceDetection, numOfFace);

    for (i = 0; i < AUTOFOCUS_TOTAL_WATING_TIME * waitTimeoutFpsValue; i += AUTOFOCUS_WATING_TIME) {
        currentState = this->getCurrentState();

#ifdef USE_HAL3_2_METADATA_INTERFACE
        /*
           * TRIGGER_START means that lock the AF state.
           */
        if(flagtrigger && flagLockFocus && (m_interenalAutoFocusMode == m_autoFocusMode)) {
            m_autofocusStep = AUTOFOCUS_STEP_TRIGGER_START;
            flagtrigger = false;
            ALOGI("INFO(%s):m_aaAfState(%d) flagLockFocus(%d) m_interenalAutoFocusMode(%d) m_autoFocusMode(%d)",
                __FUNCTION__, m_aaAfState, flagLockFocus, m_interenalAutoFocusMode, m_autoFocusMode);
        }
#endif

        /* If stopAutofocus() called */
        if (m_autofocusStep == AUTOFOCUS_STEP_STOP
#ifdef USE_HAL3_2_METADATA_INTERFACE
            && m_aaAfState == AA_AFSTATE_INACTIVE
#endif
            ) {
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
#ifdef USE_HAL3_2_METADATA_INTERFACE
                || m_autofocusStep == AUTOFOCUS_STEP_TRIGGER_START
#endif
                ) {
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
                    ALOGW("WARN(%s):AF restart is detected(%d)", __FUNCTION__, i / 1000);

                if (m_interenalAutoFocusMode == AUTOFOCUS_MODE_CONTINUOUS_PICTURE) {
                    ALOGD("DEBUG(%s):AF force-success on AUTOFOCUS_MODE_CONTINUOUS_PICTURE (%d)", __FUNCTION__, i / 1000);
                    af_over = true;
                    ret = true;
                }
                break;
            case AUTOFOCUS_STATE_SCANNING:
                flagScanningStarted = true;
                break;
            case AUTOFOCUS_STATE_SUCCEESS:
#if defined(USE_HAL3_2_METADATA_INTERFACE) || defined(USE_FD_AF_WAIT_LOCKED)
                if (flagStartFaceDetection && numOfFace > 0) {
                    if (m_aaAfState == AA_AFSTATE_FOCUSED_LOCKED) {
                        af_over = true;
                        ret = true;
                    }
                } else
#endif
                {
                    af_over = true;
                    ret = true;
                }
                break;
            case AUTOFOCUS_STATE_FAIL:
#if defined(USE_HAL3_2_METADATA_INTERFACE) || defined(USE_FD_AF_WAIT_LOCKED)
                if (flagStartFaceDetection && numOfFace > 0) {
                    if (m_aaAfState == AA_AFSTATE_NOT_FOCUSED_LOCKED) {
                        af_over = true;
                        ret = false;
                    }
                } else
#endif
                {
                    af_over = true;
                    ret = false;
                }
                break;
             default:
                ALOGV("ERR(%s):Invalid afState(%d)", __FUNCTION__, currentState);
                ret = false;
                break;
            }
        }

        if (af_over == true)
            break;

        usleep(AUTOFOCUS_WATING_TIME);
    }

    if (AUTOFOCUS_TOTAL_WATING_TIME * waitTimeoutFpsValue <= i) {
        ALOGW("WARN(%s):AF result time out(%d) msec", __FUNCTION__, i * waitTimeoutFpsValue / 1000);
        stopAutofocus(); /* Reset Previous AF */
        m_afState = AUTOFOCUS_STATE_FAIL;
    }

    ALOGI("INFO(%s[%d]):getAutofocusResult out m_autoFocusMode(%d) m_interenalAutoFocusMode(%d) result(%d) af_over(%d)",
        __FUNCTION__, __LINE__, m_autoFocusMode, m_interenalAutoFocusMode, ret, af_over);

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
#if !defined(USE_HAL3_2_METADATA_INTERFACE)
     case AA_AFMODE_CONTINUOUS_PICTURE_FACE:
#ifdef SAMSUNG_DOF
     case AA_AFMODE_PDAF_OUTFOCUSING_CONTINUOUS_PICTURE:
     case AA_AFMODE_PDAF_OUTFOCUSING_CONTINUOUS_PICTURE_FACE:

#ifdef SAMSUNG_OT
     case AA_AFMODE_CONTINUOUS_VIDEO_TRACKING:
     case AA_AFMODE_CONTINUOUS_PICTURE_TRACKING:
#endif
#endif
#endif

         switch(m_aaAfState) {
         case AA_AFSTATE_INACTIVE:
            ret = AUTOFOCUS_RESULT_CANCEL;
            break;
         case AA_AFSTATE_PASSIVE_SCAN:
         case AA_AFSTATE_ACTIVE_SCAN:
            ret = AUTOFOCUS_RESULT_FOCUSING;
            break;
#if defined(USE_HAL3_2_METADATA_INTERFACE)
         case AA_AFSTATE_PASSIVE_FOCUSED:
         case AA_AFSTATE_FOCUSED_LOCKED:
#else
         case AA_AFSTATE_AF_ACQUIRED_FOCUS:
#endif
            ret = AUTOFOCUS_RESULT_SUCCESS;
            break;
#if defined(USE_HAL3_2_METADATA_INTERFACE)
         case AA_AFSTATE_PASSIVE_UNFOCUSED:
#else
         case AA_AFSTATE_AF_FAILED_FOCUS:
#endif
            if (flagCAFScannigStarted == true)
                ret = AUTOFOCUS_RESULT_FAIL;
            else
                ret = oldRet;
            break;
#if defined(USE_HAL3_2_METADATA_INTERFACE)
         case AA_AFSTATE_NOT_FOCUSED_LOCKED:
             ret = AUTOFOCUS_RESULT_FAIL;
             break;
#endif
         default:
            ALOGE("(%s[%d]):invalid m_aaAfState", __FUNCTION__, __LINE__);
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

int ExynosCameraActivityAutofocus::getCurrentState(void)
{
    int state = AUTOFOCUS_STATE_NONE;

    if (m_flagAutofocusStart == false) {
        state = m_afState;
        goto done;
    }

    switch (m_aaAfState) {
    case ::AA_AFSTATE_INACTIVE:
        state = AUTOFOCUS_STATE_NONE;
        break;
    case ::AA_AFSTATE_PASSIVE_SCAN:
    case ::AA_AFSTATE_ACTIVE_SCAN:
        state = AUTOFOCUS_STATE_SCANNING;
        break;
#if defined(USE_HAL3_2_METADATA_INTERFACE)
    case ::AA_AFSTATE_PASSIVE_FOCUSED:
    case ::AA_AFSTATE_FOCUSED_LOCKED:
#else
    case ::AA_AFSTATE_AF_ACQUIRED_FOCUS:
#endif
        state = AUTOFOCUS_STATE_SUCCEESS;
        break;
#if defined(USE_HAL3_2_METADATA_INTERFACE)
    case ::AA_AFSTATE_NOT_FOCUSED_LOCKED:
    case ::AA_AFSTATE_PASSIVE_UNFOCUSED:
#else
    case ::AA_AFSTATE_AF_FAILED_FOCUS:
#endif
        state = AUTOFOCUS_STATE_FAIL;
        break;
    default:
        state = AUTOFOCUS_STATE_NONE;
        break;
    }

done:
    m_afState = state;

    return state;
}

bool ExynosCameraActivityAutofocus::setRecordingHint(bool hint)
{
    ALOGI("INFO(%s[%d]):hint(%d)", __FUNCTION__, __LINE__, hint);

    m_recordingHint = hint;
    return true;
}

bool ExynosCameraActivityAutofocus::setFaceDetection(bool toggle)
{
    ALOGI("INFO(%s[%d]):toggle(%d)", __FUNCTION__, __LINE__, toggle);

    m_flagFaceDetection = toggle;
    return true;
}

#ifdef SAMSUNG_DOF
bool ExynosCameraActivityAutofocus::setPDAF(bool toggle)
{
    if(m_flagPDAF != toggle) {
        ALOGI("INFO(%s[%d]):toggle(%d)", __FUNCTION__, __LINE__, toggle);
        m_flagPDAF = toggle;
    }

    return true;
}

bool ExynosCameraActivityAutofocus::setStartLensMove(bool toggle)
{
    ALOGI("INFO(%s[%d]):toggle(%d)", __FUNCTION__, __LINE__, toggle);

    m_flagLensMoveStart = toggle;
    return true;
}
#endif

bool ExynosCameraActivityAutofocus::setMacroPosition(int macroPosition)
{
    ALOGI("INFO(%s[%d]):macroPosition(%d)", __FUNCTION__, __LINE__, macroPosition);

    m_macroPosition = macroPosition;
    return true;
}

void ExynosCameraActivityAutofocus::setFpsValue(int fpsValue)
{
    m_fpsValue = fpsValue;
}

int ExynosCameraActivityAutofocus::getFpsValue()
{
    return m_fpsValue;
}

void ExynosCameraActivityAutofocus::setSamsungCamera(int flags)
{
    m_samsungCamera = flags;
}

void ExynosCameraActivityAutofocus::setAfInMotionResult(bool afInMotion)
{
    m_afInMotionResult = afInMotion;
}

bool ExynosCameraActivityAutofocus::getAfInMotionResult(void)
{
    return m_afInMotionResult;
}

void ExynosCameraActivityAutofocus::displayAFInfo(void)
{
    ALOGD("(%s):==================================================", "CMGEFL");
    ALOGD("(%s):0x%x", "CMGEFL", m_af_mode_info);
    ALOGD("(%s):0x%x", "CMGEFL", m_af_pan_focus_info);
    ALOGD("(%s):0x%x", "CMGEFL", m_af_typical_macro_info);
    ALOGD("(%s):0x%x", "CMGEFL", m_af_module_version_info);
    ALOGD("(%s):0x%x", "CMGEFL", m_af_state_info);
    ALOGD("(%s):0x%x", "CMGEFL", m_af_cur_pos_info);
    ALOGD("(%s):0x%x", "CMGEFL", m_af_time_info);
    ALOGD("(%s):0x%x", "CMGEFL", m_af_factory_info);
    ALOGD("(%s):0x%x", "CMGEFL", m_paf_from_info);
    ALOGD("(%s):==================================================", "CMGEFL");
    return ;
}

void ExynosCameraActivityAutofocus::displayAFStatus(void)
{
    ALOGD("(%s):0x%x / 0x%x / 0x%x", "CMGEFL",
           m_af_state_info, m_af_cur_pos_info, m_af_time_info);
    return ;
}

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
#if defined(USE_HAL3_2_METADATA_INTERFACE)
    case AA_AFSTATE_PASSIVE_FOCUSED:
    case AA_AFSTATE_FOCUSED_LOCKED:
#else
    case AA_AFSTATE_AF_ACQUIRED_FOCUS:
#endif
        autoFocusState = AUTOFOCUS_STATE_SUCCEESS;
        break;
#if defined(USE_HAL3_2_METADATA_INTERFACE)
    case AA_AFSTATE_NOT_FOCUSED_LOCKED:
    case AA_AFSTATE_PASSIVE_UNFOCUSED:
#else
    case AA_AFSTATE_AF_FAILED_FOCUS:
#endif
        autoFocusState = AUTOFOCUS_STATE_FAIL;
        break;
    default:
        autoFocusState = AUTOFOCUS_STATE_NONE;
        break;
    }

    return autoFocusState;
}
#if defined(USE_HAL3_2_METADATA_INTERFACE)
void ExynosCameraActivityAutofocus::m_AUTOFOCUS_MODE2AA_AFMODE(int autoFocusMode, camera2_shot_ext *shot_ext)
{
    switch (autoFocusMode) {
    case AUTOFOCUS_MODE_AUTO:
#ifdef SAMSUNG_DOF
        if (m_recordingHint == true) {
            /* CONTINUOUS_VIDEO */
            shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_CONTINUOUS_VIDEO;
            shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00
                | SET_BIT(AA_AFMODE_OPTION_BIT_VIDEO);
#ifdef SAMSUNG_OT
            if(m_isOTstart == true)
                shot_ext->shot.ctl.aa.vendor_afmode_option |= SET_BIT(AA_AFMODE_OPTION_BIT_OBJECT_TRACKING);
#endif
            shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
        } else if (m_flagFaceDetection == true) {
            if (m_flagPDAF == true) {
                if (m_flagLensMoveStart == true) {
                    /* OFF */
                    shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_OFF;
                    shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00;
                    shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
                } else {
                    /* PDAF_OUTFOCUSING_AUTO */
                    shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_AUTO;
                    shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00
                        | SET_BIT(AA_AFMODE_OPTION_BIT_OUT_FOCUSING);
                    shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_START;
                }
            } else {
                /* AUTO_FACE */
                shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_AUTO;
                shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00
                    | SET_BIT(AA_AFMODE_OPTION_BIT_FACE);
                shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_START;
            }
        }
        else {
            if (m_flagPDAF == true) {
                if (m_flagLensMoveStart == true) {
                    /* OFF */
                    shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_OFF;
                    shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00;
                    shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
                } else {
                    /* PDAF_OUTFOCUSING_AUTO */
                    shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_AUTO;
                    shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00
                        | SET_BIT(AA_AFMODE_OPTION_BIT_OUT_FOCUSING);
                    shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_START;
                }
            } else {
                /* AUTO */
                shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_AUTO;
                shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00;
#ifdef SAMSUNG_OT
                if(m_isOTstart == true)
                    shot_ext->shot.ctl.aa.vendor_afmode_option |= SET_BIT(AA_AFMODE_OPTION_BIT_OBJECT_TRACKING);
#endif
                shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_START;
            }
        }
#else
        if (m_recordingHint == true) {
            /* CONTINUOUS_VIDEO */
            shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_CONTINUOUS_VIDEO;
            shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00
                | SET_BIT(AA_AFMODE_OPTION_BIT_VIDEO);
#ifdef SAMSUNG_OT
            if(m_isOTstart == true)
                shot_ext->shot.ctl.aa.vendor_afmode_option |= SET_BIT(AA_AFMODE_OPTION_BIT_OBJECT_TRACKING);
#endif
            shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
        } else if (m_flagFaceDetection == true) {
            /* AUTO_FACE */
            shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_AUTO;
            shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00
                | SET_BIT(AA_AFMODE_OPTION_BIT_FACE);
            shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_START;
        } else {
            /* AUTO */
            shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_AUTO;
            shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00;
#ifdef SAMSUNG_OT
            if(m_isOTstart == true)
                shot_ext->shot.ctl.aa.vendor_afmode_option |= SET_BIT(AA_AFMODE_OPTION_BIT_OBJECT_TRACKING);
#endif
            shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_START;
        }
#endif
        break;
    case AUTOFOCUS_MODE_INFINITY:
        /* INFINITY */
        shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_AUTO;
        shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00;
        shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_CANCEL;
        break;
    case AUTOFOCUS_MODE_MACRO:
        /* MACRO */
        shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_MACRO;
        shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00
            | SET_BIT(AA_AFMODE_OPTION_BIT_MACRO);
        shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_CANCEL;
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
        shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
        break;
    case AUTOFOCUS_MODE_CONTINUOUS_PICTURE:
    case AUTOFOCUS_MODE_CONTINUOUS_PICTURE_MACRO:
#ifdef SAMSUNG_DOF
        if (m_flagFaceDetection == true) {
            if (m_flagPDAF == true) {
                if (m_flagLensMoveStart == true) {
                    /* OFF */
                    shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_OFF;
                    shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00;
                    shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
                } else {
                    /* PDAF_OUTFOCUSING_CONTINUOUS_PICTURE */
                    shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_CONTINUOUS_PICTURE;
                    shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00
                        | SET_BIT(AA_AFMODE_OPTION_BIT_OUT_FOCUSING);
                    shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
                }
            } else {
                /* CONTINUOUS_PICTURE_FACE */
                shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_CONTINUOUS_PICTURE;
                shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00
                    | SET_BIT(AA_AFMODE_OPTION_BIT_FACE);
                shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
            }
        }
        else {
            if (m_flagPDAF == true) {
                if (m_flagLensMoveStart == true) {
                    /* OFF */
                    shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_OFF;
                    shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00;
                    shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
                } else {
                    /* PDAF_OUTFOCUSING_CONTINUOUS_PICTURE */
                    shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_CONTINUOUS_PICTURE;
                    shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00
                        | SET_BIT(AA_AFMODE_OPTION_BIT_OUT_FOCUSING);
                    shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
                }
            } else {
                /* CONTINUOUS_PICTURE */
                shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_CONTINUOUS_PICTURE;
                shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00;
                shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
            }
        }
#else
        if (m_flagFaceDetection == true) {
            /* CONTINUOUS_PICTURE_FACE */
            shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_CONTINUOUS_PICTURE;
            shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00
                | SET_BIT(AA_AFMODE_OPTION_BIT_FACE);
            shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
        } else {
            /* CONTINUOUS_PICTURE */
            shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_CONTINUOUS_PICTURE;
            shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00;
            shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
        }
#endif
        break;
    case AUTOFOCUS_MODE_TOUCH:
#ifdef SAMSUNG_DOF
        if (m_recordingHint == true) {
            /* CONTINUOUS_VIDEO */
            shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_CONTINUOUS_VIDEO;
            shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00
                | SET_BIT(AA_AFMODE_OPTION_BIT_VIDEO);
            shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
        } else {
            if (m_flagPDAF == true) {
                if (m_flagLensMoveStart == true) {
                    /* OFF */
                    shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_OFF;
                    shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00;
                    shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
                } else {
                    /* PDAF_OUTFOCUSING_AUTO */
                    shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_AUTO;
                    shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00
                        | SET_BIT(AA_AFMODE_OPTION_BIT_OUT_FOCUSING);
                    shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_START;
                }
            } else {
                /* AUTO */
                shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_AUTO;
                shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00;
                shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_START;
            }
        }
#else
        if (m_recordingHint == true) {
            /* CONTINUOUS_VIDEO */
            shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_CONTINUOUS_VIDEO;
            shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00
                | SET_BIT(AA_AFMODE_OPTION_BIT_VIDEO);
            shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
        } else {
            /* AUTO */
            shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_AUTO;
            shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00;
            shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_START;
        }
#endif
        break;
#ifdef SAMSUNG_OT
    case AUTOFOCUS_MODE_OBJECT_TRACKING_VIDEO:
        shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_CONTINUOUS_VIDEO;
        shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00
                | SET_BIT(AA_AFMODE_OPTION_BIT_VIDEO)
                | SET_BIT(AA_AFMODE_OPTION_BIT_OBJECT_TRACKING);
        shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
        break;
    case AUTOFOCUS_MODE_OBJECT_TRACKING_PICTURE:
        shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_CONTINUOUS_PICTURE;
        shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00
                | SET_BIT(AA_AFMODE_OPTION_BIT_OBJECT_TRACKING);
        shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
        break;
#endif
#ifdef SAMSUNG_MANUAL_FOCUS
    case AUTOFOCUS_MODE_MANUAL:
        shot_ext->shot.ctl.aa.afMode = ::AA_AFMODE_OFF;
        shot_ext->shot.ctl.aa.vendor_afmode_option = 0x00;
        shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
        break;
#endif
    case AUTOFOCUS_MODE_FIXED:
        break;
    default:
        ALOGE("ERR(%s):invalid focus mode (%d)", __FUNCTION__, autoFocusMode);
        break;
    }

}
#else
int ExynosCameraActivityAutofocus::m_AUTOFOCUS_MODE2AA_AFMODE(int autoFocusMode)
{
    int aaAFMode = -1;

    switch (autoFocusMode) {
    case AUTOFOCUS_MODE_AUTO:
#ifdef SAMSUNG_DOF
        if (m_recordingHint == true)
            aaAFMode = ::AA_AFMODE_AUTO_VIDEO;
        else if (m_flagFaceDetection == true) {
            if (m_flagPDAF == true) {
                if (m_flagLensMoveStart == true)
                    aaAFMode = ::AA_AFMODE_OFF;
                else
                    aaAFMode = ::AA_AFMODE_PDAF_OUTFOCUSING;
            }
            else
                aaAFMode = ::AA_AFMODE_AUTO_FACE;
        }
        else {
            if (m_flagPDAF == true) {
                if (m_flagLensMoveStart == true)
                    aaAFMode = ::AA_AFMODE_OFF;
                else
                    aaAFMode = ::AA_AFMODE_PDAF_OUTFOCUSING;
            }
            else
                aaAFMode = ::AA_AFMODE_AUTO;
        }
#else
        if (m_recordingHint == true)
            aaAFMode = ::AA_AFMODE_AUTO_VIDEO;
        else if (m_flagFaceDetection == true)
            aaAFMode = ::AA_AFMODE_AUTO_FACE;
        else
            aaAFMode = ::AA_AFMODE_AUTO;
#endif
        break;
    case AUTOFOCUS_MODE_INFINITY:
        aaAFMode = ::AA_AFMODE_INFINITY;
        break;
    case AUTOFOCUS_MODE_MACRO:
        aaAFMode = ::AA_AFMODE_AUTO_MACRO;
        break;
    case AUTOFOCUS_MODE_EDOF:
        aaAFMode = ::AA_AFMODE_EDOF;
        break;
    case AUTOFOCUS_MODE_CONTINUOUS_VIDEO:
        aaAFMode = ::AA_AFMODE_CONTINUOUS_VIDEO;
        break;
    case AUTOFOCUS_MODE_CONTINUOUS_PICTURE:
    case AUTOFOCUS_MODE_CONTINUOUS_PICTURE_MACRO:
#ifdef SAMSUNG_DOF
        if (m_flagFaceDetection == true) {
            if (m_flagPDAF == true) {
                if (m_flagLensMoveStart == true)
                    aaAFMode = ::AA_AFMODE_OFF;
                else
                    aaAFMode = ::AA_AFMODE_PDAF_OUTFOCUSING_CONTINUOUS_PICTURE;
            }
            else
                aaAFMode = ::AA_AFMODE_CONTINUOUS_PICTURE_FACE;
        }
        else {
            if (m_flagPDAF == true) {
                if (m_flagLensMoveStart == true)
                    aaAFMode = ::AA_AFMODE_OFF;
                else
                    aaAFMode = ::AA_AFMODE_PDAF_OUTFOCUSING_CONTINUOUS_PICTURE;
            }
            else
                aaAFMode = ::AA_AFMODE_CONTINUOUS_PICTURE;
        }
#else
        if (m_flagFaceDetection == true)
            aaAFMode = ::AA_AFMODE_CONTINUOUS_PICTURE_FACE;
        else
            aaAFMode = ::AA_AFMODE_CONTINUOUS_PICTURE;
#endif
        break;
    case AUTOFOCUS_MODE_TOUCH:
#ifdef SAMSUNG_DOF
        if (m_recordingHint == true)
            aaAFMode = ::AA_AFMODE_AUTO_VIDEO;
        else {
            if (m_flagPDAF == true) {
                if (m_flagLensMoveStart == true)
                    aaAFMode = ::AA_AFMODE_OFF;
                else
                    aaAFMode = ::AA_AFMODE_PDAF_OUTFOCUSING;
            }
            else
                aaAFMode = ::AA_AFMODE_AUTO;
        }
#else
        if (m_recordingHint == true)
            aaAFMode = ::AA_AFMODE_AUTO_VIDEO;
        else
            aaAFMode = ::AA_AFMODE_AUTO;
#endif
        break;
#ifdef SAMSUNG_OT
    case AUTOFOCUS_MODE_OBJECT_TRACKING_VIDEO:
        aaAFMode = ::AA_AFMODE_CONTINUOUS_VIDEO_TRACKING;
        break;
    case AUTOFOCUS_MODE_OBJECT_TRACKING_PICTURE:
        aaAFMode = ::AA_AFMODE_CONTINUOUS_PICTURE_TRACKING;
        break;
#endif
#ifdef SAMSUNG_MANUAL_FOCUS
    case AUTOFOCUS_MODE_MANUAL:
        aaAFMode = ::AA_AFMODE_OFF;
        break;
#endif
    case AUTOFOCUS_MODE_FIXED:
        break;
    default:
        ALOGE("ERR(%s):invalid focus mode (%d)", __FUNCTION__, autoFocusMode);
        break;
    }

    return aaAFMode;
}
#endif
} /* namespace android */
