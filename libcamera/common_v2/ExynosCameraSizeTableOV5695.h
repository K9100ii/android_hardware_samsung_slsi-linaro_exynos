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

#ifndef EXYNOS_CAMERA_LUT_OV5695_H
#define EXYNOS_CAMERA_LUT_OV5695_H

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

static int PREVIEW_SIZE_LUT_OV5695[][SIZE_OF_LUT] =
{
    /* Binning   = OFF
       BNS ratio = 1.0
       BDS       = OFF  */

    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
     (2592 + 0),(1944 + 0),   /* [sensor ] */
      2592      , 1944      ,   /* [bns    ] */
      2560      , 1440      ,   /* [bcrop  ] */
      2560      , 1440      ,   /* [bds    ] */
      2560      , 1440      ,   /* [target ] */
    },
    /*  4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
     (2592 + 0),(1944 + 0),   /* [sensor ] */
      2592      , 1944      ,   /* [bns    ] */
      2560      , 1920      ,   /* [bcrop  ] */
      2048      , 1536      ,   /* [bds    ] */
      2048      , 1536      ,   /* [target ] */
    },
        /*  1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
     (2592 + 0),(1944 + 0),   /* [sensor ] */
      2592      , 1944      ,   /* [bns    ] */
      1920      , 1920      ,   /* [bcrop  ] */
      1440      , 1440      ,   /* [bds    ] */
      1440      , 1440      ,   /* [target ] */
    },
        /*  3:2 (Single, Dual) */
    { SIZE_RATIO_3_2,
     (2592 + 0),(1944 + 0),   /* [sensor ] */
      2592      , 1944      ,   /* [bns    ] */
      2560      , 1706      ,   /* [bcrop  ] */
      2560      , 1706      ,   /* [bds    ] */
      1440       , 960      ,   /* [target ] */
    },
};

static int PICTURE_SIZE_LUT_OV5695[][SIZE_OF_LUT] =
{
    { SIZE_RATIO_16_9,
     (2592 + 0),(1944 + 0),   /* [sensor ] */
      2592      , 1944      ,   /* [bns    ] */
      2560      , 1440      ,   /* [bcrop  ] */
      2560      , 1440      ,   /* [bds    ] */
      2560      , 1440      ,   /* [target  ] */
    },
    /*  4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
     (2592 + 0),(1944 + 0),   /* [sensor ] */
      2592      , 1944      ,   /* [bns    ] */
      2560      , 1920      ,   /* [bcrop  ] */
      2560      , 1920     ,   /* [bds	  ] */
      2560      , 1920     ,   /* [target ] */
    },
            /*  1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
     (2592 + 0),(1944 + 0),   /* [sensor ] */
      2592      , 1944      ,   /* [bns    ] */
      1920      , 1920      ,   /* [bcrop  ] */
      1920      , 1920      ,   /* [bds    ] */
      1920      , 1920      ,   /* [target ] */
    },
};

static int VIDEO_SIZE_LUT_OV5695[][SIZE_OF_LUT] =
{
    /* Binning   = OFF
       BNS ratio = 1.0
       BDS       :NO  */

    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
     (2592 + 0),(1944 + 0),   /* [sensor ] */
      2592      , 1944      ,   /* [bns    ] */
      2560      , 1440      ,   /* [bcrop  ] */
      2560      , 1440      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] */
    },
    /*  4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
     (2592 + 0),(1944 + 0),   /* [sensor ] */
      2592      , 1944      ,   /* [bns    ] */
      2560      , 1920      ,   /* [bcrop  ] */
      2560      , 1920      ,   /* [bds    ] */
      640      , 480      ,   /* [target ] */
    },
    /*  1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
     (2592 + 0),(1944 + 0),   /* [sensor ] */
      2592      , 1944      ,   /* [bns    ] */
      1944      , 1944      ,   /* [bcrop  ] */
      1944      , 1944      ,   /* [bds    ] */
      1072      , 1072      ,   /* [target ] */
    },
    /*  3:2 (Single, Dual) */
    { SIZE_RATIO_3_2,
     (2592 + 0),(1944 + 0),   /* [sensor ] */
      2592      , 1944      ,   /* [bns    ] */
      2560      , 1706      ,   /* [bcrop  ] */
      2560      , 1706      ,   /* [bds    ] */
      720       , 480      ,   /* [target ] */
    },
    /*  5:4 (Single, Dual) */
    { SIZE_RATIO_5_4,
     (2592 + 0),(1944 + 0),   /* [sensor ] */
      2592      , 1944      ,   /* [bns    ] */
      2400      , 1920      ,   /* [bcrop  ] */
      2400      , 1920      ,   /* [bds    ] */
      1344      , 1080      ,   /* [target ] */
    },
    /*  5:3 (Single, Dual) */
    { SIZE_RATIO_5_3,
     (2592 + 0),(1944 + 0),   /* [sensor ] */
      2592      , 1944      ,   /* [bns    ] */
      2560      , 1536      ,   /* [bcrop  ] */
      2560      , 1536      ,   /* [bds    ] */
      1792      , 1080      ,   /* [target ] */
    },
    /*  11:9 (Single, Dual) */
    { SIZE_RATIO_11_9,
     (2592 + 0),(1944 + 0),   /* [sensor ] */
      2592      , 1944      ,   /* [bns    ] */
      2346      , 1920      ,   /* [bcrop  ] */
      2346      , 1920      ,   /* [bds    ] */
      176      , 144      ,   /* [target ] */
    },
};

static int OV5695_PREVIEW_LIST[][SIZE_OF_RESOLUTION] =
{
    { 2560, 1440, SIZE_RATIO_16_9},
    { 2048, 1536, SIZE_RATIO_4_3},
    { 1920, 1080, SIZE_RATIO_16_9},
    { 1440, 1080, SIZE_RATIO_4_3},
    //{ 1440, 1440, SIZE_RATIO_1_1},
    { 1280,  720, SIZE_RATIO_16_9},
   // { 1056,  704, SIZE_RATIO_3_2},
   // {  960,  720, SIZE_RATIO_4_3},
    //{  880,  720, SIZE_RATIO_11_9},
    //{  736,  736, SIZE_RATIO_1_1},
    {  720,  480, SIZE_RATIO_3_2},
    {  640,  480, SIZE_RATIO_4_3},
    //{  352,  288, SIZE_RATIO_11_9},
    {  320,  240, SIZE_RATIO_4_3},
};

static int OV5695_HIDDEN_PREVIEW_LIST[][SIZE_OF_RESOLUTION] =
{
    { 1920, 1080, SIZE_RATIO_16_9},
    { 1440, 1080, SIZE_RATIO_4_3},
    { 1072, 1072, SIZE_RATIO_1_1},
    { 1056,  864, SIZE_RATIO_11_9},
    { 1024,  768, SIZE_RATIO_4_3},
    {  800,  600, SIZE_RATIO_4_3},
    {  800,  480, SIZE_RATIO_5_3},
    {  800,  450, SIZE_RATIO_16_9},
    {  720,  720, SIZE_RATIO_1_1},
    {  720,  480, SIZE_RATIO_3_2},
    {  672,  448, SIZE_RATIO_3_2},
    {  528,  432, SIZE_RATIO_11_9},
    {  480,  320, SIZE_RATIO_3_2},
    {  480,  270, SIZE_RATIO_16_9},
#if defined (USE_HORIZONTAL_UI_TABLET_4G_VT)
    {  480,  640, SIZE_RATIO_3_4},
#endif
};

static int OV5695_PICTURE_LIST[][SIZE_OF_RESOLUTION] =
{
    //{ 2576, 1932, SIZE_RATIO_4_3},
    //{ 2560, 1440, SIZE_RATIO_16_9},
    { 2560, 1920, SIZE_RATIO_4_3},
    { 2304, 1296, SIZE_RATIO_16_9},
    { 2000, 1500, SIZE_RATIO_4_3},
    //{ 1920, 1920, SIZE_RATIO_1_1},
    { 1920, 1080, SIZE_RATIO_16_9},
    //{ 1440, 1080, SIZE_RATIO_4_3},
    { 1280,  720, SIZE_RATIO_16_9},
    {  640,  480, SIZE_RATIO_4_3},
    {  320,  240, SIZE_RATIO_4_3},
};

static int OV5695_HIDDEN_PICTURE_LIST[][SIZE_OF_RESOLUTION] =
{
    { 1600, 1200, SIZE_RATIO_4_3},
    { 1280,  960, SIZE_RATIO_4_3},
    { 1024,  768, SIZE_RATIO_4_3},
    {  800,  600, SIZE_RATIO_4_3},
    {  800,  480, SIZE_RATIO_5_3},
    {  800,  450, SIZE_RATIO_16_9},
    {  720,  480, SIZE_RATIO_3_2},
    {  512,  384, SIZE_RATIO_4_3},
    {  512,  288, SIZE_RATIO_11_9},
    {  480,  320, SIZE_RATIO_3_2},
    {  320,  240, SIZE_RATIO_4_3},
    {  320,  180, SIZE_RATIO_16_9},
    {  176,  144, SIZE_RATIO_11_9},
};

static int OV5695_THUMBNAIL_LIST[][SIZE_OF_RESOLUTION] =
{
    {  512,  384, SIZE_RATIO_4_3},
    {  512,  288, SIZE_RATIO_16_9},
    {  384,  384, SIZE_RATIO_1_1},
    {  320,  240, SIZE_RATIO_4_3},
    {    0,    0, SIZE_RATIO_1_1}
};

static int OV5695_VIDEO_LIST[][SIZE_OF_RESOLUTION] =
{
    { 1920, 1080, SIZE_RATIO_16_9},
    //{ 1440, 1080, SIZE_RATIO_4_3},
    { 1280,  720, SIZE_RATIO_16_9},
   // {  960,  720, SIZE_RATIO_4_3},
    //{  800,  450, SIZE_RATIO_16_9},
    {  720,  480, SIZE_RATIO_3_2},
    {  640,  480, SIZE_RATIO_4_3},
    //{  480,  320, SIZE_RATIO_3_2},
   // {  352,  288, SIZE_RATIO_11_9},
    {  320,  240, SIZE_RATIO_4_3},
    {  176,  144, SIZE_RATIO_11_9},
};

static int OV5695_HIDDEN_VIDEO_LIST[][SIZE_OF_RESOLUTION] =
{
#if defined (USE_HORIZONTAL_UI_TABLET_4G_VT)
    {  480,  640, SIZE_RATIO_3_4},
#endif
};

static int OV5695_FPS_RANGE_LIST[][2] =
{
    {   5000,   5000},
    {   7000,   7000},
    {  15000,  15000},
    {  24000,  24000},
    {   4000,  30000},
    {   8000,  30000},
    {  10000,  30000},
    {  15000,  30000},
    {  30000,  30000},
};

static int OV5695_HIDDEN_FPS_RANGE_LIST[][2] =
{
    {  30000,  60000},
    {  60000,  60000},
    {  60000, 120000},
    { 120000, 120000},
};
#endif
