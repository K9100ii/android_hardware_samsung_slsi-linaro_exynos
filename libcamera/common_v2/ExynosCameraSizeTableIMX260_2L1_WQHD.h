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

#ifndef EXYNOS_CAMERA_LUT_IMX260_2L1_H
#define EXYNOS_CAMERA_LUT_IMX260_2L1_H

#include "ExynosCameraConfig.h"

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
    Sensor Margin Width  = 0,
    Sensor Margin Height = 0
-----------------------------*/

static int PREVIEW_SIZE_LUT_IMX260_2L1[][SIZE_OF_LUT] =
{
    /* Binning   = OFF
       BNS ratio = 1.0
       BDS       = 1440p */

    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
     (4032 + 0) ,(2268 + 0),   /* [sensor ] */
      4032      , 2268      ,   /* [bns    ] */
      4032      , 2268      ,   /* [bcrop  ] */
      2688      , 1512      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] */
    },
    /* 4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
     (4032 + 0) ,(3024 + 0),   /* [sensor ] */
      4032      , 3024      ,   /* [bns    ] */
      4032      , 3024      ,   /* [bcrop  ] */
      2688      , 2016      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
    /* 1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
     (3024 + 0),(3024 + 0),   /* [sensor ] */
      3024      , 3024      ,   /* [bns    ] */
      3024      , 3024      ,   /* [bcrop  ] */
      2016      , 2016      ,   /* [bds    ] */
      1080      , 1080      ,   /* [target ] */
    },
    /* 3:2 (Single, Dual) */
    { SIZE_RATIO_3_2,
     (4032 + 0) ,(3024 + 0) ,   /* [sensor ] */
      4032      , 3024      ,   /* [bns    ] */
      4032      , 2688      ,   /* [bcrop  ] */
      2688      , 1792      ,   /* [bds    ] */
      1616      , 1080      ,   /* [target ] */
    },
    /* 5:4 (Single, Dual) */
    { SIZE_RATIO_5_4,
     (4032 + 0) ,(3024 + 0) ,   /* [sensor ] */
      4032      , 3024      ,   /* [bns    ] */
      3780      , 3024      ,   /* [bcrop  ] */
      2480      , 1984      ,   /* [bds    ] */
      1344      , 1080      ,   /* [target ] */
    },
    /* 5:3 (Single, Dual) */
    { SIZE_RATIO_5_3,
     (4032 + 0) ,(3024 + 0) ,   /* [sensor ] */
      4032      , 3024      ,   /* [bns    ] */
      4030      , 2418      ,   /* [bcrop  ] */
      2640      , 1584      ,   /* [bds    ] */
      1792      , 1080      ,   /* [target ] */
    },
    /* 11:9 (Single, Dual) */
    { SIZE_RATIO_11_9,
     (4032 + 0) ,(3024 + 0) ,   /* [sensor ] */
      4032      , 3024      ,   /* [bns    ] */
      3696      , 3024      ,   /* [bcrop  ] */
      2464      , 2016      ,   /* [bds    ] */
      1312      , 1080      ,   /* [target ] */
    }
};

static int DUAL_PREVIEW_SIZE_LUT_IMX260_2L1_FHD[][SIZE_OF_LUT] =
{
    /* Binning   = OFF
       BNS ratio = 2.0
       BDS       = 1440p */

    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
     (5312 + 16),(2988 + 12),   /* [sensor ] */
      2664      , 1500      ,   /* [bns    ] */
      2648      , 1490      ,   /* [bcrop  ] */
      1920      , 1080      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] */
    },
    /* 4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
     (3984 + 16),(2988 + 12),   /* [sensor ] */
      2000      , 1500      ,   /* [bns    ] */
      1986      , 1490      ,   /* [bcrop  ] */
      1440      , 1080      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
    /* 1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
     (2992 + 16),(2988 + 12),   /* [sensor ] */
      1504      , 1500      ,   /* [bns    ] */
      1490      , 1490      ,   /* [bcrop  ] */
      1088      , 1088      ,   /* [bds    ] */
      1088      , 1088      ,   /* [target ] */
    },
    /* 3:2 (Single, Dual) */
    { SIZE_RATIO_3_2,
     (5312 + 16),(2988 + 12),   /* [sensor ] */
      2664      , 1500      ,   /* [bns    ] */
      2236      , 1490      ,   /* [bcrop  ] */
      1616      , 1080      ,   /* [bds    ] *//* w=1620, Reduced for 16 pixel align */
      1616      , 1080      ,   /* [target ] */
    },
    /* 5:4 (Single, Dual) */
    { SIZE_RATIO_5_4,
     (3984 + 16),(2988 + 12),   /* [sensor ] */
      2000      , 1500      ,   /* [bns    ] */
      1862      , 1490      ,   /* [bcrop  ] */
      1344      , 1080      ,   /* [bds    ] *//* w=1350, Reduced for 16 pixel align */
      1344      , 1080      ,   /* [target ] */
    },
    /* 5:3 (Single, Dual) */
    { SIZE_RATIO_5_3,
     (5312 + 16),(2988 + 12),   /* [sensor ] */
      2664      , 1500      ,   /* [bns    ] */
      2484      , 1490      ,   /* [bcrop  ] */
      1792      , 1080      ,   /* [bds    ] *//* w=1800, Reduced for 16 pixel align */
      1792      , 1080      ,   /* [target ] */
    },
    /* 11:9 (Single, Dual) */
    { SIZE_RATIO_11_9,
     (3984 + 16),(2988 + 12),   /* [sensor ] */
      2000      , 1500      ,   /* [bns    ] */
      1822      , 1490      ,   /* [bcrop  ] */
      1312      , 1080      ,   /* [bds    ] *//* w=1320, Reduced for 16 pixel align */
      1312      , 1080      ,   /* [target ] */
    }
};

static int PICTURE_SIZE_LUT_IMX260_2L1[][SIZE_OF_LUT] =
{
    /* Binning   = OFF
       BNS ratio = 1.0
       BDS       = OFF */

    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
     (4032 + 0),(2268 + 0),   /* [sensor ] */
      4032      , 2268      ,   /* [bns    ] */
      4032      , 2268      ,   /* [bcrop  ] */
      4032      , 2268      ,   /* [bds    ] */
      4032      , 2268      ,   /* [target ] */
    },
    /* 4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
     (4032 + 0),(3024 + 0),   /* [sensor ] */
      4032      , 3024      ,   /* [bns    ] */
      4032      , 3024      ,   /* [bcrop  ] */
      4032      , 3024      ,   /* [bds    ] */
      4032      , 3024      ,   /* [target ] */
    },
    /* 1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
     (3024 + 0),(3024 + 0),   /* [sensor ] */
      3024      , 3024      ,   /* [bns    ] */
      3024      , 3024      ,   /* [bcrop  ] */
      3024      , 3024      ,   /* [bds    ] */
      3024      , 3024      ,   /* [target ] */
    }
};

static int VIDEO_SIZE_LUT_IMX260_2L1[][SIZE_OF_LUT] =
{
    /* Binning   = OFF
       BNS ratio = 1.0
       BDS       = 1080p */

    /* 16:9 (Single) */
    { SIZE_RATIO_16_9,
     (4032 + 0) ,(2268 + 0) ,   /* [sensor ] */
      4032      , 2268      ,   /* [bns    ] */
      4032      , 2268      ,   /* [bcrop  ] */
      2688      , 1512      ,   /* [bds    ] *//* UHD (3840x2160) special handling in ExynosCameraParameters class */
      1920      , 1080      ,   /* [target ] */
    },
    /* 4:3 (Single) */
    { SIZE_RATIO_4_3,
     (4032 + 0) ,(3024 + 0) ,   /* [sensor ] */
      4032      , 3024      ,   /* [bns    ] */
      4032      , 3024      ,   /* [bcrop  ] */
      2688      , 2016      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
    /* 1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
     (3024 + 0) ,(3024 + 0) ,   /* [sensor ] */
      3024      , 3024      ,   /* [bns    ] */
      3024      , 3024      ,   /* [bcrop  ] */
      2016      , 2016      ,   /* [bds    ] */
      1080      , 1080      ,   /* [target ] */
    },
    /* 3:2 (Single) */
    { SIZE_RATIO_3_2,
     (4032 + 0) ,(3024 + 0) ,   /* [sensor ] */
      4032      , 3024      ,   /* [bns    ] */
      4032      , 2688      ,   /* [bcrop  ] */
      2688      , 1792      ,   /* [bds    ] */
      1616      , 1080      ,   /* [target ] */
    },
    /* 5:4 (Single) */
    { SIZE_RATIO_5_4,
     (4032 + 0) ,(3024 + 0) ,   /* [sensor ] */
      4032      , 3024      ,   /* [bns    ] */
      3780      , 3024      ,   /* [bcrop  ] */
      2480      , 1984      ,   /* [bds    ] */
      1344      , 1080      ,   /* [target ] */
    },
    /* 5:3 (Single) */
    { SIZE_RATIO_5_3,
     (4032 + 0) ,(3024 + 0) ,   /* [sensor ] */
      4032      , 3024      ,   /* [bns    ] */
      4030      , 2418      ,   /* [bcrop  ] */
      2640      , 1584      ,   /* [bds    ] */
      1792      , 1080      ,   /* [target ] */
    },
    /* 11:9 (Single) */
    { SIZE_RATIO_11_9,
     (4032 + 0) ,(3024 + 0) ,   /* [sensor ] */
      4032      , 3024      ,   /* [bns    ] */
      3696      , 3024      ,   /* [bcrop  ] */
      2464      , 2016      ,   /* [bds    ] */
      1312      , 1080      ,   /* [target ] */
    }
};

static int VIDEO_SIZE_LUT_120FPS_HIGH_SPEED_IMX260_2L1[][SIZE_OF_LUT] =
{
    /* Binning   = ON
       BNS ratio = 1.0
       BDS       = ON */

    /* FHD_120 16:9 (Single) */
    { SIZE_RATIO_16_9,
     (2016 + 0) ,(1134 + 0) ,   /* [sensor ] */
      2016      , 1134      ,   /* [bns    ] */
      2016      , 1134      ,   /* [bcrop  ] */
      2016      , 1134      ,   /* [bds    ] *//* UHD (3840x2160) special handling in ExynosCameraParameters class */
      1920      , 1080      ,   /* [target ] */
    }
};

static int VIDEO_SIZE_LUT_240FPS_HIGH_SPEED_IMX260_2L1[][SIZE_OF_LUT] =
{
    /* Binning   = ON
       BNS ratio = 1.0
       BDS       = ON */

    /* HD_240 16:9 (Single) */
    { SIZE_RATIO_16_9,
     (2016 + 0) ,(1134 + 0) ,   /* [sensor ] */
      2016      , 1134      ,   /* [bns    ] */
      2016      , 1134      ,   /* [bcrop  ] */
      2016      , 1134      ,   /* [bds    ] *//* UHD (3840x2160) special handling in ExynosCameraParameters class */
      1280      ,  720      ,   /* [target ] */
    },
};

static int YUV_SIZE_LUT_IMX260_2L1[][SIZE_OF_LUT] =
{
    /* Binning   = OFF
       BNS ratio = 1.0
       BDS       = 1080p */

    /* 16:9 (Single) */
    { SIZE_RATIO_16_9,
     (4032 + 0) ,(2268 + 0) ,   /* [sensor ] */
      4032      , 2268      ,   /* [bns    ] */
      4032      , 2268      ,   /* [bcrop  ] */
      4032      , 2268      ,   /* [bds    ] *//* UHD (3840x2160) special handling in ExynosCameraParameters class */
      4032      , 2268      ,   /* [target ] */
    },
    /* 4:3 (Single) */
    { SIZE_RATIO_4_3,
     (4032 + 0) ,(3024 + 0) ,   /* [sensor ] */
      4032      , 3024      ,   /* [bns    ] */
      4032      , 3024      ,   /* [bcrop  ] */
      4032      , 3024      ,   /* [bds    ] */
      4032      , 3024      ,   /* [target ] */
    },
    /* 1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
     (3024 + 0) ,(3024 + 0) ,   /* [sensor ] */
      3024      , 3024      ,   /* [bns    ] */
      3024      , 3024      ,   /* [bcrop  ] */
      3024      , 3024      ,   /* [bds    ] */
      3024      , 3024      ,   /* [target ] */
    },
    /* 3:2 (Single) */
    { SIZE_RATIO_3_2,
     (4032 + 0) ,(3024 + 0) ,   /* [sensor ] */
      4032      , 3024      ,   /* [bns    ] */
      4032      , 2688      ,   /* [bcrop  ] */
      4032      , 2688      ,   /* [bds    ] */
      4032      , 2688      ,   /* [target ] */
    },
    /* 5:4 (Single) */
    { SIZE_RATIO_5_4,
     (4032 + 0) ,(3024 + 0) ,   /* [sensor ] */
      4032      , 3024      ,   /* [bns    ] */
      3780      , 3024      ,   /* [bcrop  ] */
      3780      , 3024      ,   /* [bds    ] */
      3780      , 3024      ,   /* [target ] */
    },
    /* 5:3 (Single) */
    { SIZE_RATIO_5_3,
     (4032 + 0) ,(3024 + 0) ,   /* [sensor ] */
      4032      , 3024      ,   /* [bns    ] */
      4030      , 2418      ,   /* [bcrop  ] */
      4030      , 2418      ,   /* [bds    ] */
      4030      , 2418      ,   /* [target ] */
    },
    /* 11:9 (Single) */
    { SIZE_RATIO_11_9,
     (4032 + 0) ,(3024 + 0) ,   /* [sensor ] */
      4032      , 3024      ,   /* [bns    ] */
      3696      , 3024      ,   /* [bcrop  ] */
      3696      , 3024      ,   /* [bds    ] */
      3696      , 3024      ,   /* [target ] */
    }
};

static int IMX260_2L1_PREVIEW_LIST[][SIZE_OF_RESOLUTION] =
{
#if defined(LIMIT_SCP_SIZE_UNTIL_FHD_ON_CAPTURE)
#else
    { 2560, 1440, SIZE_RATIO_16_9},
    { 1920, 1440, SIZE_RATIO_4_3},
    { 1440, 1440, SIZE_RATIO_1_1},
#endif
#if defined(CAMERA_LCD_SIZE) && (CAMERA_LCD_SIZE >= LCD_SIZE_1920_1080)
    { 1920, 1080, SIZE_RATIO_16_9},
    { 1440, 1080, SIZE_RATIO_4_3},
    { 1088, 1088, SIZE_RATIO_1_1},
#endif
    { 1280,  720, SIZE_RATIO_16_9},
    { 1056,  704, SIZE_RATIO_3_2},
    { 1024,  768, SIZE_RATIO_4_3},
    {  960,  720, SIZE_RATIO_4_3},
    {  800,  450, SIZE_RATIO_16_9},
    {  720,  720, SIZE_RATIO_1_1},
    {  720,  480, SIZE_RATIO_3_2},
    {  640,  480, SIZE_RATIO_4_3},
    {  352,  288, SIZE_RATIO_11_9},
    {  320,  240, SIZE_RATIO_4_3},
    {  256,  144, SIZE_RATIO_16_9}, /* for CAMERA2_API_SUPPORT */
    {  176,  144, SIZE_RATIO_11_9}
};

static int IMX260_2L1_HIDDEN_PREVIEW_LIST[][SIZE_OF_RESOLUTION] =
{
#if !(defined(CAMERA_LCD_SIZE) && (CAMERA_LCD_SIZE >= LCD_SIZE_1920_1080))
    { 1920, 1080, SIZE_RATIO_16_9},
    { 1440, 1080, SIZE_RATIO_4_3},
    { 1088, 1088, SIZE_RATIO_1_1},
#endif
    { 3840, 2160, SIZE_RATIO_16_9},
    { 3264, 1836, SIZE_RATIO_16_9},
    { 2560, 1440, SIZE_RATIO_16_9},
    { 1600, 1200, SIZE_RATIO_4_3},
    { 1280,  960, SIZE_RATIO_4_3},
    { 1056,  864, SIZE_RATIO_11_9},
    {  528,  432, SIZE_RATIO_11_9},
    {  800,  480, SIZE_RATIO_5_3},
    {  672,  448, SIZE_RATIO_3_2},
    {  480,  320, SIZE_RATIO_3_2},
    {  480,  270, SIZE_RATIO_16_9},
};

static int IMX260_2L1_PICTURE_LIST[][SIZE_OF_RESOLUTION] =
{
    { 4032, 3024, SIZE_RATIO_4_3},
    { 4032, 2268, SIZE_RATIO_16_9},
    { 3024, 3024, SIZE_RATIO_1_1},
    { 3984, 2988, SIZE_RATIO_4_3},
    { 3264, 2448, SIZE_RATIO_4_3},
    { 3264, 1836, SIZE_RATIO_16_9},
    { 2976, 2976, SIZE_RATIO_1_1},
    { 2048, 1152, SIZE_RATIO_16_9},
    { 1920, 1080, SIZE_RATIO_16_9},
    { 1280,  720, SIZE_RATIO_16_9},
    {  960,  720, SIZE_RATIO_4_3},
    {  640,  480, SIZE_RATIO_4_3},
};

static int IMX260_2L1_HIDDEN_PICTURE_LIST[][SIZE_OF_RESOLUTION] =
{
    { 4128, 3096, SIZE_RATIO_4_3},
    { 4128, 2322, SIZE_RATIO_16_9},
    { 4096, 3072, SIZE_RATIO_4_3},
    { 4096, 2304, SIZE_RATIO_16_9},
    { 3840, 2160, SIZE_RATIO_16_9},
    { 3200, 2400, SIZE_RATIO_4_3},
    { 3072, 1728, SIZE_RATIO_16_9},
    { 2988, 2988, SIZE_RATIO_1_1},
    { 2656, 1494, SIZE_RATIO_16_9}, /* use S-note */
    { 2592, 1944, SIZE_RATIO_4_3},
    { 2592, 1936, SIZE_RATIO_4_3},  /* not exactly matched ratio */
    { 2560, 1920, SIZE_RATIO_4_3},
    { 2448, 2448, SIZE_RATIO_1_1},
    { 2048, 1536, SIZE_RATIO_4_3},
    {  720,  720, SIZE_RATIO_1_1}, /* dummy size for binning mode */
    {  352,  288, SIZE_RATIO_11_9}, /* dummy size for binning mode */
};

static int IMX260_2L1_THUMBNAIL_LIST[][SIZE_OF_RESOLUTION] =
{
    {  512,  384, SIZE_RATIO_4_3},
    {  512,  288, SIZE_RATIO_16_9},
    {  384,  384, SIZE_RATIO_1_1},
/* TODO : will be supported after enable S/W scaler correctly */
//  {  320,  240, SIZE_RATIO_4_3},
    {    0,    0, SIZE_RATIO_1_1}
};

static int IMX260_2L1_VIDEO_LIST[][SIZE_OF_RESOLUTION] =
{
    { 1920, 1080, SIZE_RATIO_16_9},
    { 1440, 1080, SIZE_RATIO_4_3},
    { 1088, 1088, SIZE_RATIO_1_1},
    { 1280,  720, SIZE_RATIO_16_9},
    {  960,  720, SIZE_RATIO_4_3},
    {  800,  450, SIZE_RATIO_16_9},
    {  720,  480, SIZE_RATIO_3_2},
    {  640,  480, SIZE_RATIO_4_3},
    {  480,  320, SIZE_RATIO_3_2},
    {  352,  288, SIZE_RATIO_11_9},
    {  320,  240, SIZE_RATIO_4_3},
    {  256,  144, SIZE_RATIO_16_9}, /* for CAMERA2_API_SUPPORT */
    {  176,  144, SIZE_RATIO_11_9},
};

static int IMX260_2L1_HIDDEN_VIDEO_LIST[][SIZE_OF_RESOLUTION] =
{
#ifdef USE_UHD_RECORDING
    { 3840, 2160, SIZE_RATIO_16_9},
#endif
#ifdef USE_WQHD_RECORDING
    { 2560, 1440, SIZE_RATIO_16_9},
#endif
};

/* For HAL3 */
static int IMX260_2L1_YUV_LIST[][SIZE_OF_RESOLUTION] =
{
    { 4032, 3024, SIZE_RATIO_4_3},
    { 4032, 2268, SIZE_RATIO_16_9},
    { 3024, 3024, SIZE_RATIO_1_1},
    { 3984, 2988, SIZE_RATIO_4_3},
    { 3264, 2448, SIZE_RATIO_4_3},
    { 3264, 1836, SIZE_RATIO_16_9},
    { 2976, 2976, SIZE_RATIO_1_1},
    { 2048, 1152, SIZE_RATIO_16_9},
    { 1920, 1080, SIZE_RATIO_16_9},
    { 1280,  720, SIZE_RATIO_16_9},
    {  960,  720, SIZE_RATIO_4_3},
    {  720,  480, SIZE_RATIO_3_2},
    {  640,  480, SIZE_RATIO_4_3},
    {  320,  240, SIZE_RATIO_4_3},
    {  256,  144, SIZE_RATIO_16_9},
};

/* For HAL3 */
static int IMX260_2L1_HIGH_SPEED_VIDEO_LIST[][SIZE_OF_RESOLUTION] =
{
    { 1280,  720, SIZE_RATIO_16_9},
};

static int IMX260_2L1_FPS_RANGE_LIST[][2] =
{
    //{   5000,   5000},
    //{   7000,   7000},
    {  15000,  15000},
    {  24000,  24000},
    //{   4000,  30000},
    {  10000,  30000},
    {  15000,  30000},
    {  30000,  30000},
};

static int IMX260_2L1_HIDDEN_FPS_RANGE_LIST[][2] =
{
    {  10000,  24000},
    {  30000,  60000},
    {  60000,  60000},
    {  60000, 120000},
    { 120000, 120000},
};


/* For HAL3 */
static int IMX260_2L1_HIGH_SPEED_VIDEO_FPS_RANGE_LIST[][2] =
{
    {  30000, 120000},
    { 120000, 120000},
};

static camera_metadata_rational UNIT_MATRIX_IMX260_2L1_3X3[] =
{
    {128, 128}, {0, 128}, {0, 128},
    {0, 128}, {128, 128}, {0, 128},
    {0, 128}, {0, 128}, {128, 128}
};

static camera_metadata_rational COLOR_MATRIX1_IMX260_3X3[] =
{
    {680, 1024}, {-47, 1024}, {-123, 1024},
    {-556, 1024}, {1473, 1024}, {73, 1024},
    {-181, 1024}, {415, 1024}, {487, 1024}
};

static camera_metadata_rational COLOR_MATRIX2_IMX260_3X3[] =
{
    {1268, 1024}, {-455, 1024}, {-285, 1024},
    {-461, 1024}, {1505, 1024}, {71, 1024},
    {-92, 1024}, {299, 1024}, {628, 1024}
};

static camera_metadata_rational COLOR_MATRIX1_2L1_3X3[] =
{
    {669, 1024}, {-81, 1024}, {-124, 1024},
    {-533, 1024}, {1447, 1024}, {78, 1024},
    {-133, 1024}, {331, 1024}, {442, 1024}
};

static camera_metadata_rational COLOR_MATRIX2_2L1_3X3[] =
{
    {1059, 1024}, {-343, 1024}, {-262, 1024},
    {-498, 1024}, {1555, 1024}, {47, 1024},
    {-69, 1024}, {231, 1024}, {660, 1024}
};

static camera_metadata_rational FORWARD_MATRIX1_IMX260_3X3[] =
{
    {764, 1024}, {-20, 1024}, {243, 1024},
    {290, 1024}, {720, 1024}, {13, 1024},
    {26, 1024}, {-467, 1024}, {1286, 1024}
};

static camera_metadata_rational FORWARD_MATRIX2_IMX260_3X3[] =
{
    {645, 1024}, {19, 1024}, {323, 1024},
    {169, 1024}, {795, 1024}, {60, 1024},
    {19, 1024}, {-870, 1024}, {1696, 1024}
};

static camera_metadata_rational FORWARD_MATRIX1_2L1_3X3[] =
{
    {713, 1024}, {31, 1024}, {244, 1024},
    {265, 1024}, {752, 1024}, {7, 1024},
    {10, 1024}, {-413, 1024}, {1248, 1024}
};

static camera_metadata_rational FORWARD_MATRIX2_2L1_3X3[] =
{
    {663, 1024}, {29, 1024}, {295, 1024},
    {187, 1024}, {767, 1024}, {70, 1024},
    {5, 1024}, {-627, 1024}, {1467, 1024}
};
#endif
