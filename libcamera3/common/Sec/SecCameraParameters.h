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

#ifndef SEC_CAMERA_PARAMETERS_H
#define SEC_CAMERA_PARAMETERS_H

#include "ExynosCameraParameters.h"

namespace android {

enum {
    LLS_LEVEL_ZSL                           = 0,    /* 0x00 */
    LLS_LEVEL_LOW                           = 1,    /* 0x01 */
    LLS_LEVEL_FLASH                         = 2,    /* 0x02 flash capture */
    LLS_LEVEL_SIS                           = 3,    /* 0x03 */
    LLS_LEVEL_ZSL_LIKE                      = 4,    /* 0x04 */
    LLS_LEVEL_ZSL_LIKE1                     = 7,    /* 0x07 */
    LLS_LEVEL_SHARPEN_SINGLE                = 8,    /* 0x08 */
    LLS_LEVEL_MANUAL_ISO                    = 9,    /* 0x09 */
    LLS_LEVEL_SHARPEN_DR                    = 10,   /* 0x0A */
    LLS_LEVEL_SHARPEN_IMA                   = 11,   /* 0x0B */
    LLS_LEVEL_STK                           = 12,
    LLS_LEVEL_LLS_FLASH                     = 16,   /* 0x10 */
    LLS_LEVEL_MULTI_MERGE_2                 = 18,   /* 0x12 */
    LLS_LEVEL_MULTI_MERGE_3                 = 19,   /* 0x13 */
    LLS_LEVEL_MULTI_MERGE_4                 = 20,   /* 0x14 */
    LLS_LEVEL_MULTI_PICK_2                  = 34,   /* 0x22 */
    LLS_LEVEL_MULTI_PICK_3                  = 35,   /* 0x23 */
    LLS_LEVEL_MULTI_PICK_4                  = 36,   /* 0x24 */
    LLS_LEVEL_MULTI_MERGE_INDICATOR_2       = 50,   /* 0x32 */
    LLS_LEVEL_MULTI_MERGE_INDICATOR_3       = 51,   /* 0x33 */
    LLS_LEVEL_MULTI_MERGE_INDICATOR_4       = 52,   /* 0x34 */
    LLS_LEVEL_FLASH_2                       = 66,   /* 0x42 */
    LLS_LEVEL_FLASH_3                       = 67,   /* 0x43 */
    LLS_LEVEL_FLASH_4                       = 68,   /* 0x44 */
    LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_2   = 82,   /* 0x52 */
    LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_3   = 83,   /* 0x53 */
    LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_4   = 84,   /* 0x54 */
    LLS_LEVEL_FLASH_LOW_2                   = 98,   /* 0x62 */
    LLS_LEVEL_FLASH_LOW_3                   = 99,   /* 0x63 */
    LLS_LEVEL_FLASH_LOW_4                   = 100,  /* 0x64 */
    LLS_LEVEL_LLS_INDICATOR_ONLY            = 100,  /* 0x64 */

    /* SUPPORT_ZSL_MULTIFRAME */
    LLS_LEVEL_MULTI_MERGE_LOW_2             = 114,  /* 0x72 */
    LLS_LEVEL_MULTI_MERGE_LOW_3             = 115,  /* 0x73 */
    LLS_LEVEL_MULTI_MERGE_LOW_4             = 116,  /* 0x74 */
    LLS_LEVEL_MULTI_MERGE_ZSL_2             = 130,  /* 0x82 */
    LLS_LEVEL_MULTI_MERGE_ZSL_3             = 131,  /* 0x83 */
    LLS_LEVEL_MULTI_MERGE_ZSL_4             = 132,  /* 0x84 */

    LLS_LEVEL_DUMMY                         = 150,  /* 0x96 */
};

#ifdef SAMSUNG_QUICK_SWITCH
enum QuickSwitch_state {
    QUICK_SWITCH_IDLE = 0,
    QUICK_SWITCH_STBY,
    QUICK_SWITCH_MAIN
};

enum QuickSwitch_cmd {
    QUICK_SWITCH_CMD_DONE = 0,
    QUICK_SWITCH_CMD_IDLE_TO_STBY,
    QUICK_SWITCH_CMD_MAIN_TO_STBY,
};
#endif

enum SpecialCapture_step {
    SCAPTURE_STEP_NONE       = 0,
    SCAPTURE_STEP_COUNT_1    = 1,
    SCAPTURE_STEP_COUNT_2    = 2,
    SCAPTURE_STEP_COUNT_3    = 3,
    SCAPTURE_STEP_COUNT_4    = 4,
    SCAPTURE_STEP_COUNT_END = 9,
    SCAPTURE_STEP_YUV        = 10,
    SCAPTURE_STEP_JPEG       = 11,
    SCAPTURE_STEP_NV21       = 12,
    SCAPTURE_STEP_DONE       = 13,
};

enum Intelligent_Mode {
    INTELLIGENT_MODE_NONE = 0,
    INTELLIGENT_MODE_SMART_STAY = 1,
};

}; // namespace android
#endif /* SEC_CAMERA_PARAMETERS_H */

