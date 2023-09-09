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
    LLS_LEVEL_SHARPEN_DR                    = 10,   /* 0x0A */
    LLS_LEVEL_SHARPEN_IMA                   = 11,   /* 0x0B */
    LLS_LEVEL_STK                           = 12,
    LLS_LEVEL_LLS_FLASH                     = 16,   /* 0x10 */
    LLS_LEVEL_MULTI_MERGE_2                 = 18,   /* 0x12 */
    LLS_LEVEL_MULTI_MERGE_3                 = 19,   /* 0x13 */
    LLS_LEVEL_MULTI_MERGE_4                 = 20,   /* 0x14 */
    LLS_LEVEL_MULTI_MERGE_5                 = 21,   /* 0x15 */
    LLS_LEVEL_MULTI_PICK_2                  = 34,   /* 0x22 */
    LLS_LEVEL_MULTI_PICK_3                  = 35,   /* 0x23 */
    LLS_LEVEL_MULTI_PICK_4                  = 36,   /* 0x24 */
    LLS_LEVEL_MULTI_PICK_5                  = 37,   /* 0x25 */
    LLS_LEVEL_MULTI_MERGE_INDICATOR_2       = 50,   /* 0x32 */
    LLS_LEVEL_MULTI_MERGE_INDICATOR_3       = 51,   /* 0x33 */
    LLS_LEVEL_MULTI_MERGE_INDICATOR_4       = 52,   /* 0x34 */
    LLS_LEVEL_MULTI_MERGE_INDICATOR_5       = 53,   /* 0x35 */
    LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_2   = 82,   /* 0x52 */
    LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_3   = 83,   /* 0x53 */
    LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_4   = 84,   /* 0x54 */
    LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_5   = 85,   /* 0x55 */

    LLS_LEVEL_MULTI_MFHDR_2                 = 98,   /* 0x62 */
    LLS_LEVEL_MULTI_MFHDR_3                 = 99,   /* 0x63 */
    LLS_LEVEL_MULTI_MFHDR_4                 = 100,  /* 0x64 */
    LLS_LEVEL_MULTI_MFHDR_5                 = 101,  /* 0x65 */

    /* SUPPORT_ZSL_MULTIFRAME */
    LLS_LEVEL_MULTI_MERGE_LOW_2             = 114,  /* 0x72 */
    LLS_LEVEL_MULTI_MERGE_LOW_3             = 115,  /* 0x73 */
    LLS_LEVEL_MULTI_MERGE_LOW_4             = 116,  /* 0x74 */
    LLS_LEVEL_MULTI_MERGE_LOW_5             = 117,  /* 0x75 */
    LLS_LEVEL_MULTI_MERGE_ZSL_2             = 130,  /* 0x82 */
    LLS_LEVEL_MULTI_MERGE_ZSL_3             = 131,  /* 0x83 */
    LLS_LEVEL_MULTI_MERGE_ZSL_4             = 132,  /* 0x84 */
    LLS_LEVEL_MULTI_MERGE_ZSL_5             = 133,  /* 0x85 */

    LLS_LEVEL_MULTI_LLHDR_LOW_2             = 146,  /* 0x92 */
    LLS_LEVEL_MULTI_LLHDR_LOW_3             = 147,  /* 0x93 */
    LLS_LEVEL_MULTI_LLHDR_LOW_4             = 148,  /* 0x94 */
    LLS_LEVEL_MULTI_LLHDR_LOW_5             = 149,  /* 0x95 */
    LLS_LEVEL_MULTI_LLHDR_2                 = 162,  /* 0xA2 */
    LLS_LEVEL_MULTI_LLHDR_3                 = 163,  /* 0xA3 */
    LLS_LEVEL_MULTI_LLHDR_4                 = 164,  /* 0xA4 */
    LLS_LEVEL_MULTI_LLHDR_5                 = 165,  /* 0xA5 */
};

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

#ifdef SAMSUNG_HIFI_VIDEO
enum {
    HIFIVIDEO_OPMODE_NONE = 0,
    HIFIVIDEO_OPMODE_HIFIONLY_PREVIEW,
    HIFIVIDEO_OPMODE_HIFIONLY_RECORDING,
    HIFIVIDEO_OPMODE_VDISONLY_RECORDING,
    HIFIVIDEO_OPMODE_HIFIVDIS_RECORDING,
};
#endif

enum Operation_Mode {
    OPERATION_MODE_NONE              = 0,
    OPERATION_MODE_SMART_STAY        = 1,
    OPERATION_MODE_DRAM_TEST         = 2,
    OPERATION_MODE_LN2_TEST          = 3,
    OPERATION_MODE_LN4_TEST          = 4,
    OPERATION_MODE_SSM_TEST          = 5,
};

#ifdef SAMSUNG_SSM
enum SSM_STATE {
    SSM_STATE_NONE               = 0,
    SSM_STATE_NORMAL             = 1,
    SSM_STATE_PREVIEW_ONLY       = 2, /* Section C1 : Process frame id 4 (Preview Only) */
    SSM_STATE_PROCESSING         = 3, /* Section C2 : Receive and process 960fps frames */
    SSM_STATE_POSTPROCESSING     = 4, /* Section C3 : Post-Process frames from SSMSaveBufferQ */
};

enum SSM_FRAME_ID {
    SSM_FRAME_NORMAL            = 1,
    SSM_FRAME_NORMAL_RECORDING  = 2,
    SSM_FRAME_RECORDING_FIRST   = 3,
    SSM_FRAME_PREVIEW_ONLY      = 4,
    SSM_FRAME_RECORDING         = 5,
    SSM_FRAME_PREVIEW           = 6,
};
#endif

}; // namespace android
#endif /* SEC_CAMERA_PARAMETERS_H */

