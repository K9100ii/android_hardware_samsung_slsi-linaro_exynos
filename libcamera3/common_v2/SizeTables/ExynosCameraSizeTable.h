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

#ifndef EXYNOS_CAMERA_SIZE_TABLE_H
#define EXYNOS_CAMERA_SIZE_TABLE_H
#include <log/log.h>
#include <utils/String8.h>
#include <system/graphics.h>
#include "ExynosCameraConfig.h"

namespace android {

#define SIZE_OF_LUT         11
#define SIZE_OF_RESOLUTION  4

enum EXYNOS_CAMERA_SIZE_RATIO_ID {
    SIZE_RATIO_16_9 = 0,
    SIZE_RATIO_4_3,
    SIZE_RATIO_1_1,
    SIZE_RATIO_3_2,
    SIZE_RATIO_5_4,
    SIZE_RATIO_5_3,
    SIZE_RATIO_11_9,
#if defined (USE_HORIZONTAL_UI_TABLET_4G_VT)
    SIZE_RATIO_3_4,
#endif
    SIZE_RATIO_9_16,
    SIZE_RATIO_18P5_9, /* 18.5 : 9 */
    SIZE_RATIO_END
};

enum SIZE_LUT_INDEX {
    RATIO_ID,
    SENSOR_W   = 1,
    SENSOR_H,
    BNS_W,
    BNS_H,
    BCROP_W,
    BCROP_H,
    BDS_W,
    BDS_H,
    TARGET_W,
    TARGET_H,
    SIZE_LUT_INDEX_END
};

/* width, height, min_fps, max_fps, vdis width, vdis height, recording limit time(sec) */
enum VIDEO_SIZE_LUT_INDEX {
    VIDEO_WIDTH,
    VIDEO_HEIGHT,
    VIDEO_MIN_FPS,
    VIDEO_MAX_FPS,
    VIDEO_VDIS_WIDTH,
    VIDEO_VDIS_HEIGHT,
    VIDEO_LIMIT_TIME,
};

/* LSI Sensor */
#include "ExynosCameraSizeTable2L7_WQHD.h"
#include "ExynosCameraSizeTable2P8_WQHD.h"
#include "ExynosCameraSizeTable2L3_WQHD.h"
#include "ExynosCameraSizeTable3M3.h"
#include "ExynosCameraSizeTable5F1.h"
#include "ExynosCameraSizeTableRPB.h"
#include "ExynosCameraSizeTable_2P7SQ_WQHD.h"
#include "ExynosCameraSizeTable_2T7SX_WQHD.h"

/* Sony Sensor */
#include "ExynosCameraSizeTableIMX260_2L1_WQHD.h"
#include "ExynosCameraSizeTableIMX333_2L2_WQHD.h"
#include "ExynosCameraSizeTableIMX320_3H1.h"
#include "ExynosCameraSizeTable6B2.h"

}; /* namespace android */
#endif
