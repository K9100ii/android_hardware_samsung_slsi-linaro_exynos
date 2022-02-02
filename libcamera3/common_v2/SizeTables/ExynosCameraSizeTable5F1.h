/*
**
** Copyright 2014, Samsung Electronics Co. LTD
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

#ifndef EXYNOS_CAMERA_LUT_5F1_H
#define EXYNOS_CAMERA_LUT_5F1_H

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
-----------------------------
    Sensor Margin Width  = 16,
    Sensor Margin Height = 12
-----------------------------*/

static int PREVIEW_SIZE_LUT_5F1[][SIZE_OF_LUT] =
{
    /* Binning   = OFF
       BNS ratio = 1.0
       BDS       = OFF  */

    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
     (2576 + 0),(1932 + 0),   /* [sensor ] */
      2576      , 1932      ,   /* [bns    ] */
      2576      , 1448      ,   /* [bcrop  ] */
      2576      , 1448      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] */
    },
    /*  4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
     (2576 + 0),(1932 + 0),   /* [sensor ] */
      2576      , 1932      ,   /* [bns    ] */
      2576      , 1932      ,   /* [bcrop  ] */
      2576      , 1932      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
    /*  1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
    (2400 + 0),(2400 + 0),	 /* [sensor ] */
     2400	   , 2400	   ,   /* [bns	  ] */
     2400	   , 2400	   ,   /* [bcrop  ] */
     2400	   , 2400	   ,   /* [bds	  ] */
     2400	   , 2400	   ,   /* [target ] */
    },
    /* 3:2 (Single) */
    { SIZE_RATIO_3_2,
     (2576 + 0),(1932 + 0),   /* [sensor ] */
      2576      , 1932      ,   /* [bns    ] */
      2560      , 1708      ,   /* [bcrop  ] */
      2560      , 1708      ,   /* [bds    ] */
      1616      , 1080      ,   /* [target ] */
    },
    /* 5:4 (Single) */
    { SIZE_RATIO_5_4,
     (2576 + 0),(1932 + 0),   /* [sensor ] */
      2576      , 1932      ,   /* [bns    ] */
      2400      , 1920      ,   /* [bcrop  ] */
      2400      , 1920      ,   /* [bds    ] */
      1344      , 1080      ,   /* [target ] */
    },
    /* 5:3 (Single) */
    { SIZE_RATIO_5_3,
     (2576 + 0),(1932 + 0),   /* [sensor ] */
      2576      , 1932      ,   /* [bns    ] */
      2560      , 1536      ,   /* [bcrop  ] */
      2560      , 1536      ,   /* [bds    ] */
      1792      , 1080      ,   /* [target ] */
    },
    /* 11:9 (Single) */
    { SIZE_RATIO_11_9,
     (2576 + 0),(1932 + 0),   /* [sensor ] */
      2576      , 1932      ,   /* [bns    ] */
      2352      , 1920      ,   /* [bcrop  ] */
      2352      , 1920      ,   /* [bds    ] */
      1312      , 1080      ,   /* [target ] */
    }
};

static int PREVIEW_FULL_SIZE_LUT_5F1[][SIZE_OF_LUT] =
{
    { SIZE_RATIO_16_9,
     (2592 + 16),(1950 + 10),   /* [sensor ] */
      2608      , 1960      ,   /* [bns    ] */
      2592      , 1458      ,   /* [bcrop  ] */
      2592      , 1458      ,   /* [bds    ] */
      2592      , 1458      ,   /* [target ] */
    },
    /*  4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
     (2592 + 16),(1950 + 10),   /* [sensor ] */
      2608      , 1960      ,   /* [bns    ] */
      2592      , 1944      ,   /* [bcrop  ] */
      2592      , 1944      ,   /* [bds    ] */
      2592      , 1944      ,   /* [target ] */
    },
    /*  1:1 (Single) */
    { SIZE_RATIO_1_1,
     (2400 + 0),(2400 + 0),	 /* [sensor ] */
      2400	   , 2400	   ,   /* [bns	  ] */
      2400	   , 2400	   ,   /* [bcrop  ] */
      2400	   , 2400	   ,   /* [bds	  ] */
      2400	   , 2400	   ,   /* [target ] */
    },
    /* 3:2 (Single, Dual) */
    { SIZE_RATIO_3_2,
     (2592 + 16), (1950 + 10),  /* [sensor ] */
      2608      , 1960      ,   /* [bns    ] */
      2592      , 1728      ,   /* [bcrop  ] */
      2592      , 1728      ,   /* [bds    ] */
      2592      , 1728      ,   /* [target ] */
    },
    /* 5:4 (Single, Dual) */
    { SIZE_RATIO_5_4,
     (2592 + 16), (1950 + 10),  /* [sensor ] */
      2608      , 1960      ,   /* [bns    ] */
      2432      , 1950      ,   /* [bcrop  ] */
      2432      , 1950      ,   /* [bds    ] */
      2432      , 1950      ,   /* [target ] */
    },
    /* 5:3 (Single, Dual) */
    { SIZE_RATIO_5_3,
     (2592 + 16), (1950 + 10),  /* [sensor ] */
      2608      , 1960      ,   /* [bns    ] */
      2560      , 1536      ,   /* [bcrop  ] */
      2560      , 1536      ,   /* [bds    ] */
      2560      , 1536      ,   /* [target ] */
    },
    /* 11:9 (Single, Dual) */
    { SIZE_RATIO_11_9,
     (2592 + 16), (1950 + 10),  /* [sensor ] */
      2608      , 1960      ,   /* [bns    ] */
      2384      , 1950      ,   /* [bcrop  ] */
      2384      , 1950      ,   /* [bds    ] */
      2384      , 1950      ,   /* [target ] */
    }
};

static int S5K5F1_YUV_LIST[][SIZE_OF_RESOLUTION] =
{
    { 2576, 1932, SIZE_RATIO_4_3},
    { 2560, 1440, SIZE_RATIO_16_9},
    { 2048, 1536, SIZE_RATIO_4_3},
    { 2048, 1152, SIZE_RATIO_16_9},
    { 2400, 2400, SIZE_RATIO_1_1},
    { 1920, 1080, SIZE_RATIO_16_9},
    { 1440, 1080, SIZE_RATIO_4_3},
    { 1280,  720, SIZE_RATIO_16_9},
    { 1072, 1072, SIZE_RATIO_1_1},
    {  960,  720, SIZE_RATIO_4_3},
    {  720,  480, SIZE_RATIO_3_2},
    {  640,  480, SIZE_RATIO_4_3},
    {  352,  288, SIZE_RATIO_11_9},
    {  320,  240, SIZE_RATIO_4_3},
    {  256,  144, SIZE_RATIO_16_9},
};

/* For HAL3 : width, height, min_fps, max_fpx, vdis width, vdis height , recording limit time(sec)*/
static int S5K5F1_AVAILABLE_VIDEO_LIST[][7] =
{
#ifdef USE_WQHD_RECORDING
    { 2560, 1440, 30000, 30000, 3072, 1728, 0},
#endif
    { 1440, 1440, 30000, 30000, 0, 0, 0},
    { 1920, 1080, 30000, 30000, 2304, 1296, 0},
    { 1280,  720, 30000, 30000, 1536, 864, 0},
    {  640,  480, 30000, 30000, 0, 0, 0},
};

static int S5K5F1_AVAILABLE_IRIS_SIZE_LIST[][2] =
{
    { 2400, 2400},
};

static int S5K5F1_AVAILABLE_IRIS_FORMATS_LIST[] =
{
    HAL_PIXEL_FORMAT_Y8,
};

static int S5K5F1_FPS_RANGE_LIST[][2] =
{
    {   5000,   5000},
    {   7000,   7000},
};
#endif
