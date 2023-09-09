/*
**
** Copyright 2017, Samsung Electronics Co. LTD
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

#ifndef EXYNOS_CAMERA_LUT_6B2_H
#define EXYNOS_CAMERA_LUT_6B2_H

/* -------------------------
    SIZE_RATIO_16_9 = 0,
    SIZE_RATIO_4_3,
    SIZE_RATIO_1_1,
    SIZE_RATIO_3_2,
    SIZE_RATIO_5_4,
    SIZE_RATIO_5_3,
    SIZE_RATIO_11_9,
    SIZE_RATIO_END
----------------------------
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
-----------------------------*/

static int PREVIEW_SIZE_LUT_6B2[][SIZE_OF_LUT] =
{
    /* Binning   = OFF
       BNS ratio = 1.0
       BDS       = 1080p */

    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1920      , 1080      ,   /* [bcrop  ] */
      1920      , 1080      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] */
    },
    /*  4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1440      , 1080      ,   /* [bcrop  ] */
      1440      , 1080      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
    /*  1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1080      , 1080      ,   /* [bcrop  ] */
      1080      , 1080      ,   /* [bds    ] */
      1080      , 1080      ,   /* [target ] */
    },
    /* 3:2 (Single) */
    { SIZE_RATIO_3_2,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1616      , 1080      ,   /* [bcrop  ] */
      1616      , 1080      ,   /* [bds    ] */
      1616      , 1080      ,   /* [target ] *//* w=1620, Reduced for 16 pixel align */
    },
    /* 5:4 (Single) */
    { SIZE_RATIO_5_4,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1344      , 1080      ,   /* [bcrop  ] */
      1344      , 1080      ,   /* [bds    ] */
      1344      , 1080      ,   /* [target ] *//* w=1350, Reduced for 16 pixel align */
    },
    /* 5:3 (Single) */
    { SIZE_RATIO_5_3,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1792      , 1080      ,   /* [bcrop  ] */
      1792      , 1080      ,   /* [bds    ] */
      1792      , 1080      ,   /* [target ] *//* w=1800, Reduced for 16 pixel align */
    },
    /* 11:9 (Single) */
    { SIZE_RATIO_11_9,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1312      , 1080      ,   /* [bcrop  ] */
      1312      , 1080      ,   /* [bds    ] */
      1312      , 1080      ,   /* [target ] *//* w=1320, Reduced for 16 pixel align */
    },
};

static int PICTURE_SIZE_LUT_6B2[][SIZE_OF_LUT] =
{
    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1920      , 1080      ,   /* [bcrop  ] */
      1920      , 1080      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] */
    },
    /*  4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1440      , 1080      ,   /* [bcrop  ] */
      1440      , 1080      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
    /*  1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1080      , 1080      ,   /* [bcrop  ] */
      1080      , 1080      ,   /* [bds    ] */
      1080      , 1080      ,   /* [target ] */
    },
    /* 3:2 (Single) */
    { SIZE_RATIO_3_2,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1616      , 1080      ,   /* [bcrop  ] */
      1616      , 1080      ,   /* [bds    ] */
      1616      , 1080      ,   /* [target ] *//* w=1620, Reduced for 16 pixel align */
    },
    /* 5:4 (Single) */
    { SIZE_RATIO_5_4,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1344      , 1080      ,   /* [bcrop  ] */
      1344      , 1080      ,   /* [bds    ] */
      1344      , 1080      ,   /* [target ] *//* w=1350, Reduced for 16 pixel align */
    },
    /* 5:3 (Single) */
    { SIZE_RATIO_5_3,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1792      , 1080      ,   /* [bcrop  ] */
      1792      , 1080      ,   /* [bds    ] */
      1792      , 1080      ,   /* [target ] *//* w=1800, Reduced for 16 pixel align */
    },
    /* 11:9 (Single) */
    { SIZE_RATIO_11_9,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1312      , 1080      ,   /* [bcrop  ] */
      1312      , 1080      ,   /* [bds    ] */
      1312      , 1080      ,   /* [target ] *//* w=1320, Reduced for 16 pixel align */
    },
};

static int VIDEO_SIZE_LUT_6B2[][SIZE_OF_LUT] =
{
    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1920      , 1080      ,   /* [bcrop  ] */
      1920      , 1080      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] */
    },
    /*  4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1440      , 1080      ,   /* [bcrop  ] */
      1440      , 1080      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
    /*  1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1080      , 1080      ,   /* [bcrop  ] */
      1080      , 1080      ,   /* [bds    ] */
      1080      , 1080      ,   /* [target ] */
    },
    /* 3:2 (Single) */
    { SIZE_RATIO_3_2,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1616      , 1080      ,   /* [bcrop  ] */
      1616      , 1080      ,   /* [bds    ] */
      1616      , 1080      ,   /* [target ] *//* w=1620, Reduced for 16 pixel align */
    },
    /* 5:4 (Single) */
    { SIZE_RATIO_5_4,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1344      , 1080      ,   /* [bcrop  ] */
      1344      , 1080      ,   /* [bds    ] */
      1344      , 1080      ,   /* [target ] *//* w=1350, Reduced for 16 pixel align */
    },
    /* 5:3 (Single) */
    { SIZE_RATIO_5_3,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1792      , 1080      ,   /* [bcrop  ] */
      1792      , 1080      ,   /* [bds    ] */
      1792      , 1080      ,   /* [target ] *//* w=1800, Reduced for 16 pixel align */
    },
    /* 11:9 (Single) */
    { SIZE_RATIO_11_9,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1312      , 1080      ,   /* [bcrop  ] */
      1312      , 1080      ,   /* [bds    ] */
      1312      , 1080      ,   /* [target ] *//* w=1320, Reduced for 16 pixel align */
    },
};

static int PREVIEW_FULL_SIZE_LUT_6B2[][SIZE_OF_LUT] =
{
    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1920      , 1080      ,   /* [bcrop  ] */
      1920      , 1080      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] */
    },
    /*  4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1920      , 1080      ,   /* [bcrop  ] */
      1920      , 1080      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] */
    },
    /*  1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1920      , 1080      ,   /* [bcrop  ] */
      1920      , 1080      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] */
    },
    /* 3:2 (Single) */
    { SIZE_RATIO_3_2,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1920      , 1080      ,   /* [bcrop  ] */
      1920      , 1080      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] *//* w=1620, Reduced for 16 pixel align */
    },
    /* 5:4 (Single) */
    { SIZE_RATIO_5_4,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1920      , 1080      ,   /* [bcrop  ] */
      1920      , 1080      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] *//* w=1350, Reduced for 16 pixel align */
    },
    /* 5:3 (Single) */
    { SIZE_RATIO_5_3,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1920      , 1080      ,   /* [bcrop  ] */
      1920      , 1080      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] *//* w=1800, Reduced for 16 pixel align */
    },
    /* 11:9 (Single) */
    { SIZE_RATIO_11_9,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1920      , 1080      ,   /* [bcrop  ] */
      1920      , 1080      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] *//* w=1320, Reduced for 16 pixel align */
    },
};

static int PICTURE_FULL_SIZE_LUT_6B2[][SIZE_OF_LUT] =
{
    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1920      , 1080      ,   /* [bcrop  ] */
      1920      , 1080      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] */
    },
    /*  4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1440      , 1080      ,   /* [bcrop  ] */
      1440      , 1080      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
    /*  1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1080      , 1080      ,   /* [bcrop  ] */
      1080      , 1080      ,   /* [bds    ] */
      1080      , 1080      ,   /* [target ] */
    },
    /* 3:2 (Single) */
    { SIZE_RATIO_3_2,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1616      , 1080      ,   /* [bcrop  ] */
      1616      , 1080      ,   /* [bds    ] */
      1616      , 1080      ,   /* [target ] *//* w=1620, Reduced for 16 pixel align */
    },
    /* 5:4 (Single) */
    { SIZE_RATIO_5_4,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1344      , 1080      ,   /* [bcrop  ] */
      1344      , 1080      ,   /* [bds    ] */
      1344      , 1080      ,   /* [target ] *//* w=1350, Reduced for 16 pixel align */
    },
    /* 5:3 (Single) */
    { SIZE_RATIO_5_3,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1792      , 1080      ,   /* [bcrop  ] */
      1792      , 1080      ,   /* [bds    ] */
      1792      , 1080      ,   /* [target ] *//* w=1800, Reduced for 16 pixel align */
    },
    /* 11:9 (Single) */
    { SIZE_RATIO_11_9,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1312      , 1080      ,   /* [bcrop  ] */
      1312      , 1080      ,   /* [bds    ] */
      1312      , 1080      ,   /* [target ] *//* w=1320, Reduced for 16 pixel align */
    },
};

static int VTCALL_SIZE_LUT_6B2[][SIZE_OF_LUT] =
{
    /* Binning   = 2
       BNS ratio = 1.0
       BDS       = ON */

    /* 16:9 (VT_Call) */
    { SIZE_RATIO_16_9,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1280      , 1080      ,   /* [bcrop  ] */
      1280      , 1080      ,   /* [bds    ] */
      1280      ,  720      ,   /* [target ] */
    },
    /* 4:3 (VT_Call) */
    /* Bcrop size 1152*864 -> 1280*960, for flicker algorithm */
    { SIZE_RATIO_4_3,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1440      , 1080      ,   /* [bcrop  ] */
      640       ,  480      ,   /* [bds    ] */
      640       ,  480      ,   /* [target ] */
    },
    /* 1:1 (VT_Call) */
    { SIZE_RATIO_1_1,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1080      , 1080      ,   /* [bcrop  ] */
      1080      , 1080      ,   /* [bds    ] */
      1080      , 1080      ,   /* [target ] */
    },
    /* 11:9 (VT_Call) */
    /* Bcrop size 1056*864 -> 1168*960, for flicker algorithm */
    { SIZE_RATIO_11_9,
      1936      , 1090      ,   /* [sensor ] */
      1936      , 1090      ,   /* [bns    ] */
      1168      ,  960      ,   /* [bcrop  ] */
      1168      ,  960      ,   /* [bds    ] */
      1168      ,  960      ,   /* [target ] */
    }
};

static int S5K6B2_YUV_LIST[][SIZE_OF_RESOLUTION] =
{
    /* { width, height, minFrameDuration, ratioId } */
    { 1920, 1080, 33331760, SIZE_RATIO_16_9},
    { 1440, 1080, 33331760, SIZE_RATIO_4_3},
    { 1072, 1072, 33331760, SIZE_RATIO_1_1},
    { 1280,  720, 16665880, SIZE_RATIO_16_9},
    { 1056,  704, 16665880, SIZE_RATIO_3_2},
    {  960,  720, 16665880, SIZE_RATIO_4_3},
    //{  816, 1456, 16665880, SIZE_RATIO_9_16},
    {  800,  600, 16665880, SIZE_RATIO_4_3},
    {  800,  450, 16665880, SIZE_RATIO_16_9},
    {  736,  736, 16665880, SIZE_RATIO_1_1},
    {  720,  480, 16665880, SIZE_RATIO_3_2},
    {  640,  480, 16665880, SIZE_RATIO_4_3},
    {  352,  288, 16665880, SIZE_RATIO_11_9},
    {  320,  240, 16665880, SIZE_RATIO_4_3},
    {  256,  144, 16665880, SIZE_RATIO_16_9},
    {  176,  144, 16665880, SIZE_RATIO_11_9},
};

/* availble Jpeg size (only for  HAL_PIXEL_FORMAT_BLOB) */
static int S5K6B2_JPEG_LIST[][SIZE_OF_RESOLUTION] =
{
    /* { width, height, minFrameDuration, ratioId } */
    { 1920, 1080, 33331760, SIZE_RATIO_16_9},
    { 1440, 1080, 33331760, SIZE_RATIO_4_3},
    { 1072, 1072, 33331760, SIZE_RATIO_1_1},
    { 1280,  720, 33331760, SIZE_RATIO_16_9},
    { 1056,  704, 33331760, SIZE_RATIO_3_2},
    {  960,  720, 33331760, SIZE_RATIO_4_3},
    {  800,  600, 16665880, SIZE_RATIO_4_3},
    {  800,  450, 33331760, SIZE_RATIO_16_9},
    {  736,  736, 33331760, SIZE_RATIO_1_1},
    {  720,  480, 33331760, SIZE_RATIO_3_2},
    {  640,  480, 33331760, SIZE_RATIO_4_3},
    {  352,  288, 33331760, SIZE_RATIO_11_9},
    {  320,  240, 33331760, SIZE_RATIO_4_3},
};

static int S5K6B2_THUMBNAIL_LIST[][SIZE_OF_RESOLUTION] =
{
    {  512,  384, 0, SIZE_RATIO_4_3},
    {  512,  288, 0, SIZE_RATIO_16_9},
    {  384,  384, 0, SIZE_RATIO_1_1},
    {  320,  240, 0, SIZE_RATIO_4_3},
    {    0,    0, 0, SIZE_RATIO_1_1}
};

/* vendor static info : width, height, min_fps, max_fps, vdis width, vdis height, recording limit time(sec) */
static int S5K6B2_AVAILABLE_VIDEO_LIST[][7] =
{
    { 1920, 1080, 30000, 30000, 0, 0, 0},
    { 1280,  720, 30000, 30000, 0, 0, 0},
    {  640,  480, 30000, 30000, 0, 0, 0},
};

static int S5K6B2_FPS_RANGE_LIST[][2] =
{
    {  15000,  15000},
    {  24000,  24000},
    {  7000,  30000},
    {  8000,  30000},
    {  10000,  30000},
    {  15000,  30000},
    {  30000,  30000},
};

static int S5K6B2_PREVIEW_LIST[][SIZE_OF_RESOLUTION] =
{
    { 1920, 1080, SIZE_RATIO_16_9},
    { 1440, 1080, SIZE_RATIO_4_3},
    { 1072, 1072, SIZE_RATIO_1_1},
    { 1280,  720, SIZE_RATIO_16_9},
    { 1056,  704, SIZE_RATIO_3_2},
    {  960,  720, SIZE_RATIO_4_3},
    {  736,  736, SIZE_RATIO_1_1},
    {  720,  480, SIZE_RATIO_3_2},
    {  640,  480, SIZE_RATIO_4_3},
    {  352,  288, SIZE_RATIO_11_9},
    {  320,  240, SIZE_RATIO_4_3},
};
#endif
