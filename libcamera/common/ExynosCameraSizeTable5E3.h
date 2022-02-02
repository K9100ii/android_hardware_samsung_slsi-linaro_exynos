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

#ifndef EXYNOS_CAMERA_LUT_5E3_H
#define EXYNOS_CAMERA_LUT_5E3_H

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

static int PREVIEW_SIZE_LUT_5E3[][SIZE_OF_LUT] =
{
    /* Binning   = OFF
       BNS ratio = 1.0
       BDS       = OFF  */

    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
     (2576 + 0) ,(1932 + 0) ,   /* [sensor ] */
      2576      , 1932      ,   /* [bns    ] */
      2560      , 1440      ,   /* [bcrop  ] */
      2560      , 1440      ,   /* [bds    ] */
      1280      ,  720      ,   /* [target ] */
    },
    /*  4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
     (2576 + 0) ,(1932 + 0) ,   /* [sensor ] */
      2576      , 1932      ,   /* [bns    ] */
      2576      , 1932      ,   /* [bcrop  ] */
      2576      , 1932      ,   /* [bds    ] */
       960      ,  720      ,   /* [target ] */
    },
    /*  1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
     (2576 + 0) ,(1932 + 0) ,   /* [sensor ] */
      2576      , 1932      ,   /* [bns    ] */
      1920      , 1920      ,   /* [bcrop  ] */
      1920      , 1920      ,   /* [bds    ] */
       720      ,  720      ,   /* [target ] */
    },
    /* 3:2 (Single) */
    { SIZE_RATIO_3_2,
     (2576 + 0) ,(1932 + 0) ,   /* [sensor ] */
      2576      , 1932      ,   /* [bns    ] */
      2544      , 1696      ,   /* [bcrop  ] */
      2544      , 1696      ,   /* [bds    ] */
      1056      ,  704      ,   /* [target ] */
    },
    /* 5:4 (Single) - Dummy Unused  */
    { SIZE_RATIO_5_4,
     (2576 + 0) ,(1932 + 0) ,   /* [sensor ] */
      2576      , 1932      ,   /* [bns    ] */
      2576      , 1932      ,   /* [bcrop  ] */
      2576      , 1932      ,   /* [bds    ] */
      2576      , 1932      ,   /* [target ] */
    },
    /* 5:3 (Single) */
    { SIZE_RATIO_5_3,
     (2576 + 0) ,(1932 + 0) ,   /* [sensor ] */
      2576      , 1932      ,   /* [bns    ] */
      2560      , 1536      ,   /* [bcrop  ] */
      2560      , 1536      ,   /* [bds    ] */
       800      ,  480      ,   /* [target ] */
    },
    /* 11:9 (Single) */
    { SIZE_RATIO_11_9,
     (2576 + 0) ,(1932 + 0) ,   /* [sensor ] */
      2576      , 1932      ,   /* [bns    ] */
      2288      , 1872      ,   /* [bcrop  ] */
      2288      , 1872      ,   /* [bds    ] */
      1056      ,  864      ,   /* [target ] */
    }
};

static int PICTURE_SIZE_LUT_5E3[][SIZE_OF_LUT] =
{
    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
     (2576 + 0) ,(1932 + 0) ,   /* [sensor ] */
      2576      , 1932      ,   /* [bns    ] */
      2560      , 1440      ,   /* [bcrop  ] */
      2560      , 1440      ,   /* [bds    ] */
      2560      , 1440      ,   /* [target ] */
    },
    /*  4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
     (2576 + 0) ,(1932 + 0) ,   /* [sensor ] */
      2576      , 1932      ,   /* [bns    ] */
      2576      , 1932      ,   /* [bcrop  ] */
      2576      , 1932      ,   /* [bds    ] */
      2576      , 1932      ,   /* [target ] */
    },
    /*  1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
     (2576 + 0) ,(1932 + 0) ,   /* [sensor ] */
      2576      , 1932      ,   /* [bns    ] */
      1920      , 1920      ,   /* [bcrop  ] */
      1920      , 1920      ,   /* [bds    ] */
      1920      , 1920      ,   /* [target ] */
    },
    /* 3:2 (Single) */
    { SIZE_RATIO_3_2,
     (2576 + 0) ,(1932 + 0) ,   /* [sensor ] */
      2576      , 1932      ,   /* [bns    ] */
      2544      , 1696      ,   /* [bcrop  ] */
      2544      , 1696      ,   /* [bds    ] */
       720      ,  480      ,   /* [target ] */
    },
    /* 5:4 (Single) - Dummy Unused */
    { SIZE_RATIO_5_4,
     (2576 + 0) ,(1932 + 0) ,   /* [sensor ] */
      2576      , 1932      ,   /* [bns    ] */
      2576      , 1932      ,   /* [bcrop  ] */
      2576      , 1932      ,   /* [bds    ] */
      2576      , 1932      ,   /* [target ] */
    },
    /* 5:3 (Single) */
    { SIZE_RATIO_5_3,
     (2576 + 0) ,(1932 + 0) ,   /* [sensor ] */
      2576      , 1932      ,   /* [bns    ] */
      2560      , 1536      ,   /* [bcrop  ] */
      2560      , 1536      ,   /* [bds    ] */
       800      ,  480      ,   /* [target ] */
    },
    /* 11:9 (Single) */
    { SIZE_RATIO_11_9,
     (2576 + 0) ,(1932 + 0) ,   /* [sensor ] */
      2576      , 1932      ,   /* [bns    ] */
      2288      , 1872      ,   /* [bcrop  ] */
      2288      , 1872      ,   /* [bds    ] */
       512      ,  288      ,   /* [target ] */
    }
};

static int VIDEO_SIZE_LUT_5E3[][SIZE_OF_LUT] =
{
    /* Binning   = OFF
       BNS ratio = 1.0
       BDS       :NO  */

    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
     (2576 + 0) ,(1932 + 0) ,   /* [sensor ] */
      2576      , 1932      ,   /* [bns    ] */
      2560      , 1440      ,   /* [bcrop  ] */
      2560      , 1440      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] */
    },
    /*  4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
     (2576 + 0) ,(1932 + 0) ,   /* [sensor ] */
      2576      , 1932      ,   /* [bns    ] */
      2576      , 1932      ,   /* [bcrop  ] */
      2576      , 1932      ,   /* [bds    ] */
       640      ,  480      ,   /* [target ] */
    },
    /*  1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
     (2576 + 0) ,(1932 + 0) ,   /* [sensor ] */
      2576      , 1932      ,   /* [bns    ] */
      1920      , 1920      ,   /* [bcrop  ] */
      1920      , 1920      ,   /* [bds    ] */
       720      ,  720      ,   /* [target ] */
    },
    /* 3:2 (Single) */
    { SIZE_RATIO_3_2,
     (2576 + 0) ,(1932 + 0) ,   /* [sensor ] */
      2576      , 1932      ,   /* [bns    ] */
      2544      , 1696      ,   /* [bcrop  ] */
      2544      , 1696      ,   /* [bds    ] */
      1056      ,  704      ,   /* [target ] */
    },
    /* 5:4 (Single) - Dummy Unused */
    { SIZE_RATIO_5_4,
     (2576 + 0) ,(1932 + 0) ,   /* [sensor ] */
      2576      , 1932      ,   /* [bns    ] */
      2576      , 1932      ,   /* [bcrop  ] */
      2576      , 1932      ,   /* [bds    ] */
      2576      , 1932      ,   /* [target ] */
    },
    /* 5:3 (Single) */
    { SIZE_RATIO_5_3,
     (2576 + 0) ,(1932 + 0) ,   /* [sensor ] */
      2576      , 1932      ,   /* [bns    ] */
      2560      , 1536      ,   /* [bcrop  ] */
      2560      , 1536      ,   /* [bds    ] */
       800      ,  480      ,   /* [target ] */
    },
    /* 11:9 (Single) */
    { SIZE_RATIO_11_9,
     (2576 + 0) ,(1932 + 0) ,   /* [sensor ] */
      2576      , 1932      ,   /* [bns    ] */
      2288      , 1872      ,   /* [bcrop  ] */
      2288      , 1872      ,   /* [bds    ] */
      1056      ,  864      ,   /* [target ] */
    }
};

#if defined(ENABLE_8MP_FULL_FRAME) || defined(ENABLE_13MP_FULL_FRAME)
static int VIDEO_SIZE_LUT_5E3_FULL[][SIZE_OF_LUT] =
{
    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
     (2576 + 0) ,(1932 + 0) ,   /* [sensor ] */
      2576      , 1932      ,   /* [bns    ] */
      2560      , 1440      ,   /* [bcrop  ] */
      1920      , 1440      ,   /* [bds    ] */
      1920      , 1440      ,   /* [target ] */
    },
    /*  4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
     (2576 + 0) ,(1932 + 0) ,   /* [sensor ] */
      2576      , 1932      ,   /* [bns    ] */
      2576      , 1932      ,   /* [bcrop  ] */
      1920      , 1440      ,   /* [bds    ] */
      1920      , 1440      ,   /* [target ] */
    },
    /*  1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
     (2576 + 0) ,(1932 + 0) ,   /* [sensor ] */
      2576      , 1932      ,   /* [bns    ] */
      1920      , 1920      ,   /* [bcrop  ] */
      1920      , 1440      ,   /* [bds    ] */
      1920      , 1440      ,   /* [target ] */
    },
    /* 3:2 (Single) */
    { SIZE_RATIO_3_2,
     (2576 + 0) ,(1932 + 0) ,   /* [sensor ] */
      2576      , 1932      ,   /* [bns    ] */
      2544      , 1696      ,   /* [bcrop  ] */
      1920      , 1440      ,   /* [bds    ] */
      1920      , 1440      ,   /* [target ] */
    },
    /* 5:4 (Single) - Dummy Unused */
    { SIZE_RATIO_5_4,
     (2576 + 0) ,(1932 + 0) ,   /* [sensor ] */
      2576      , 1932      ,   /* [bns    ] */
      2576      , 1932      ,   /* [bcrop  ] */
      1920      , 1440      ,   /* [bds    ] */
      1920      , 1440      ,   /* [target ] */
    },
    /* 5:3 (Single) */
    { SIZE_RATIO_5_3,
     (2576 + 0) ,(1932 + 0) ,   /* [sensor ] */
      2576      , 1932      ,   /* [bns    ] */
      2560      , 1536      ,   /* [bcrop  ] */
      1920      , 1440      ,   /* [bds    ] */
      1920      , 1440      ,   /* [target ] */
    },
    /* 11:9 (Single) */
    { SIZE_RATIO_11_9,
     (2576 + 0) ,(1932 + 0) ,   /* [sensor ] */
      2576      , 1932      ,   /* [bns    ] */
      2288      , 1872      ,   /* [bcrop  ] */
      1920      , 1440      ,   /* [bds    ] */
      1920      , 1440      ,   /* [target ] */
    }
};
#endif
static int VTCALL_SIZE_LUT_5E3[][SIZE_OF_LUT] =
{
    /* 16:9 (VT Call) */
    { SIZE_RATIO_16_9,
     (1280 + 0) ,( 960 + 0) ,   /* [sensor ] */
      1280      ,  960      ,   /* [bns    ] */
      1280      ,  720      ,   /* [bcrop  ] */
      1280      ,  720      ,   /* [bds    ] */
      1280      ,  720      ,   /* [target ] */
    },
    /*  4:3 (VT Call) */
    { SIZE_RATIO_4_3,
     (1280 + 0) ,( 960 + 0) ,   /* [sensor ] */
      1280      ,  960      ,   /* [bns    ] */
      1280      ,  960      ,   /* [bcrop  ] */
      1280      ,  960      ,   /* [bds    ] */
      1280      ,  960      ,   /* [target ] */
    },
    /*  1:1 (VT Call) */
    { SIZE_RATIO_1_1,
     (1280 + 0) ,( 960 + 0) ,   /* [sensor ] */
      1280      ,  960      ,   /* [bns    ] */
       720      ,  720      ,   /* [bcrop  ] */
       720      ,  720      ,   /* [bds    ] */
       720      ,  720      ,   /* [target ] */
    },
    /* 11:9 (VT Call) */
    { SIZE_RATIO_11_9,
     (1280 + 0) ,( 960 + 0) ,   /* [sensor ] */
      1280      ,  960      ,   /* [bns    ] */
      1056      ,  864      ,   /* [bcrop  ] */
      1056      ,  864      ,   /* [bds    ] */
       352      ,  288      ,   /* [target ] */
    }
};

static int S5K5E3_PREVIEW_LIST[][SIZE_OF_RESOLUTION] =
{
#if defined(CAMERA_LCD_SIZE) && (CAMERA_LCD_SIZE >= LCD_SIZE_1920_1080)
    { 1920, 1080, SIZE_RATIO_16_9},
    { 1440, 1080, SIZE_RATIO_4_3},
    { 1072, 1072, SIZE_RATIO_1_1},
#else
    // for android 5.1 CTS
    // Aspect ratio of maximum preview size should be same with maximum picture size's AR
    // https://android.googlesource.com/platform/frameworks/base/+/a0496d3%5E!/
    { 1280,  960, SIZE_RATIO_4_3},
#endif
    { 1280,  720, SIZE_RATIO_16_9},
    { 1056,  704, SIZE_RATIO_3_2},
    {  960,  720, SIZE_RATIO_4_3},
    {  880,  720, SIZE_RATIO_11_9},
    {  736,  736, SIZE_RATIO_1_1},
    {  720,  480, SIZE_RATIO_3_2},
    {  640,  480, SIZE_RATIO_4_3},
    {  352,  288, SIZE_RATIO_11_9},
    {  320,  240, SIZE_RATIO_4_3},
    {  176,  144, SIZE_RATIO_11_9}
};

static int S5K5E3_HIDDEN_PREVIEW_LIST[][SIZE_OF_RESOLUTION] =
{
#if !(defined(CAMERA_LCD_SIZE) && (CAMERA_LCD_SIZE >= LCD_SIZE_1920_1080))
    { 1920, 1080, SIZE_RATIO_16_9},
    { 1440, 1080, SIZE_RATIO_4_3},
    { 1072, 1072, SIZE_RATIO_1_1},
#endif
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

static int S5K5E3_PICTURE_LIST[][SIZE_OF_RESOLUTION] =
{
    { 2576, 1932, SIZE_RATIO_4_3},
    { 2560, 1440, SIZE_RATIO_16_9},
    { 2048, 1536, SIZE_RATIO_4_3},
    { 2048, 1152, SIZE_RATIO_16_9},
    { 1920, 1920, SIZE_RATIO_1_1},
    { 1920, 1080, SIZE_RATIO_16_9},
    { 1440, 1080, SIZE_RATIO_4_3},
    { 1280,  720, SIZE_RATIO_16_9},
    { 1072, 1072, SIZE_RATIO_1_1},
    {  960,  720, SIZE_RATIO_4_3},
    {  640,  480, SIZE_RATIO_4_3},
};

static int S5K5E3_HIDDEN_PICTURE_LIST[][SIZE_OF_RESOLUTION] =
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
};

static int S5K5E3_THUMBNAIL_LIST[][SIZE_OF_RESOLUTION] =
{
    {  512,  384, SIZE_RATIO_4_3},
    {  512,  288, SIZE_RATIO_16_9},
    {  384,  384, SIZE_RATIO_1_1},
    {  320,  240, SIZE_RATIO_4_3},
    {    0,    0, SIZE_RATIO_1_1}
};

static int S5K5E3_VIDEO_LIST[][SIZE_OF_RESOLUTION] =
{
    { 1920, 1080, SIZE_RATIO_16_9},
    { 1440, 1080, SIZE_RATIO_4_3},
    { 1280,  720, SIZE_RATIO_16_9},
    {  960,  720, SIZE_RATIO_4_3},
    {  800,  450, SIZE_RATIO_16_9},
    {  720,  480, SIZE_RATIO_3_2},
    {  640,  480, SIZE_RATIO_4_3},
    {  480,  320, SIZE_RATIO_3_2},
    {  352,  288, SIZE_RATIO_11_9},
    {  320,  240, SIZE_RATIO_4_3},
    {  176,  144, SIZE_RATIO_11_9}
};

static int S5K5E3_HIDDEN_VIDEO_LIST[][SIZE_OF_RESOLUTION] =
{
#if defined (USE_HORIZONTAL_UI_TABLET_4G_VT)
    {  480,  640, SIZE_RATIO_3_4},
#endif
};

static int S5K5E3_FPS_RANGE_LIST[][2] =
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

static int S5K5E3_HIDDEN_FPS_RANGE_LIST[][2] =
{
    {  30000,  60000},
    {  60000,  60000},
    {  60000, 120000},
    { 120000, 120000},
};
#endif
