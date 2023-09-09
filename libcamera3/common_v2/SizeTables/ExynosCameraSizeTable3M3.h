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

#ifndef EXYNOS_CAMERA_LUT_3M3_H
#define EXYNOS_CAMERA_LUT_3M3_H

#include "ExynosCameraConfig.h"

/* -------------------------
    SIZE_RATIO_16_9 = 0,
    SIZE_RATIO_4_3,
    SIZE_RATIO_1_1,
    SIZE_RATIO_3_2,
    SIZE_RATIO_5_4,
    SIZE_RATIO_5_3,
    SIZE_RATIO_11_9,
    SIZE_RATIO_18P5_9,
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

static int PREVIEW_SIZE_LUT_3M3[][SIZE_OF_LUT] =
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
    },
    /* 18.5:9 (Single, Dual) */
    { SIZE_RATIO_18P5_9,
     (4032 + 0) ,(1960 + 0) ,	/* [sensor ] */
      4032      , 1960      ,	/* [bns    ] */
      4032      , 1960      ,	/* [bcrop  ] */
      2688      , 1306      ,	/* [bds    ] */
      2224      , 1080      ,	/* [target ] */
    }
};

static int PREVIEW_SIZE_LUT_3M3_BNS[][SIZE_OF_LUT] =
{
    /* Binning   = OFF
       BNS ratio = 1.5
       BDS       = 1440p */

    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
     (4032 + 0) ,(2268 + 0),   /* [sensor ] */
      2688      , 1512      ,   /* [bns    ] */
      2688      , 1512      ,   /* [bcrop  ] */
      2688      , 1512      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] */
    },
    /* 4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
     (4032 + 0) ,(3024 + 0),   /* [sensor ] */
      2688      , 2016      ,   /* [bns    ] */
      2688      , 2016      ,   /* [bcrop  ] */
      2688      , 2016      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
    /* 1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
     (4032 + 0),(3024 + 0),   /* [sensor ] */
      2688      , 2016      ,   /* [bns    ] */
      2688      , 2016      ,   /* [bcrop  ] */
      2688      , 2016      ,   /* [bds    ] */
      1080      , 1080      ,   /* [target ] */
    },
    /* 3:2 (Single, Dual) */
    { SIZE_RATIO_3_2,
     (4032 + 0) ,(3024 + 0) ,   /* [sensor ] */
      2688      , 1356      ,   /* [bns    ] */
      2688      , 1356      ,   /* [bcrop  ] */
      2688      , 1356      ,   /* [bds    ] */
      1616      , 1080      ,   /* [target ] */
    },
    /* 5:4 (Single, Dual) */
    { SIZE_RATIO_5_4,
     (4032 + 0) ,(3024 + 0) ,   /* [sensor ] */
      2688      , 1356      ,   /* [bns    ] */
      2530      , 1356      ,   /* [bcrop  ] */
      2530      , 1356      ,   /* [bds    ] */
      1344      , 1080      ,   /* [target ] */
    },
    /* 5:3 (Single, Dual) */
    { SIZE_RATIO_5_3,
     (4032 + 0) ,(3024 + 0) ,   /* [sensor ] */
      2688      , 1356      ,   /* [bns    ] */
      2688      , 1612      ,   /* [bcrop  ] */
      2688      , 1612      ,   /* [bds    ] */
      1792      , 1080      ,   /* [target ] */
    },
    /* 11:9 (Single, Dual) */
    { SIZE_RATIO_11_9,
     (4032 + 0) ,(3024 + 0) ,   /* [sensor ] */
      2688      , 1356      ,   /* [bns    ] */
      2464      , 1356      ,   /* [bcrop  ] */
      2464      , 1356      ,   /* [bds    ] */
      1312      , 1080      ,   /* [target ] */
    },
    /* 18.5:9 (Single, Dual) */
    { SIZE_RATIO_18P5_9,
     (4032 + 0) ,(1960 + 0) ,	/* [sensor ] */
      2688      , 1306      ,	/* [bns    ] */
      2688      , 1306      ,	/* [bcrop  ] */
      2688      , 1306      ,	/* [bds    ] */
      2224      , 1080      ,	/* [target ] */
    }
};

static int PICTURE_SIZE_LUT_3M3[][SIZE_OF_LUT] =
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
    },
    /* 18.5:9 (Single, Dual) */
    { SIZE_RATIO_18P5_9,
     (4032 + 0) ,(1960 + 0) ,	/* [sensor ] */
      4032      , 1960      ,	/* [bns    ] */
      4032      , 1960      ,	/* [bcrop  ] */
      4032      , 1960      ,	/* [bds    ] */
      4032      , 1960      ,	/* [target ] */
    }
};

static int VIDEO_SIZE_LUT_3M3[][SIZE_OF_LUT] =
{
    /* Binning   = OFF
       BNS ratio = 1.0
       BDS       = 1440p */

    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
     (4032 + 0) ,(2268 + 0) ,   /* [sensor ] */
      4032      , 2268      ,   /* [bns    ] */
      4032      , 2268      ,   /* [bcrop  ] */
      2688      , 1512      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] */
    },
    /* 4:3 (Single, Dual) */
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
    },
    /* 18.5:9 (Single, Dual) */
    { SIZE_RATIO_18P5_9,
     (4032 + 0) ,(1960 + 0) ,	/* [sensor ] */
      4032      , 1960      ,	/* [bns    ] */
      4032      , 1960      ,	/* [bcrop  ] */
      2688      , 1306      ,	/* [bds    ] */
      2224      , 1080      ,	/* [target ] */
    }
};

static int VIDEO_SIZE_LUT_3M3_BNS[][SIZE_OF_LUT] =
{
    /* Binning   = OFF
       BNS ratio = 1.5
       BDS       = 1080p */

    /* 16:9 (Single) */
    { SIZE_RATIO_16_9,
     (4032 + 0) ,(2268 + 0) ,   /* [sensor ] */
      2688      , 1512      ,   /* [bns    ] */
      2688      , 1512      ,   /* [bcrop  ] */
      2688      , 1512      ,   /* [bds    ] *//* UHD (3840x2160) special handling in ExynosCameraParameters class */
      1920      , 1080      ,   /* [target ] */
    },
    /* 4:3 (Single) */
    { SIZE_RATIO_4_3,
     (4032 + 0) ,(3024 + 0) ,   /* [sensor ] */
      2688      , 2016      ,   /* [bns    ] */
      2688      , 2016      ,   /* [bcrop  ] */
      2688      , 2016      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
    /* 1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
     (4032 + 0) ,(3024 + 0) ,   /* [sensor ] */
      2688      , 2016      ,   /* [bns    ] */
      2688      , 2016      ,   /* [bcrop  ] */
      2688      , 2016      ,   /* [bds    ] */
      1080      , 1080      ,   /* [target ] */
    },
};

static int VIDEO_SIZE_LUT_60FPS_HIGH_SPEED_3M3[][SIZE_OF_LUT] =
{
    /* Binning   = ON
       BNS ratio = 1.0
       BDS       = ON */

    /* FHD_60 16:9 (Single) */
    { SIZE_RATIO_16_9,
     (1920 + 0)  ,(1080 + 0) ,   /* [sensor ] */
      1920       , 1080      ,   /* [bns    ] */
      1920       , 1080      ,   /* [bcrop  ] */
      1920       , 1080      ,   /* [bds    ] */
      1920      ,  1080      ,   /* [target ] */
    },
};

static int VIDEO_SIZE_LUT_120FPS_HIGH_SPEED_3M3[][SIZE_OF_LUT] =
{
    /* Binning   = ON
       BNS ratio = 1.0
       BDS       = ON */

    /* HD_120 16:9 (Single) */
    { SIZE_RATIO_16_9,
     (1376 + 0)  ,(774 + 0) ,   /* [sensor ] */
      1376       , 774      ,   /* [bns    ] */
      1376       , 774      ,   /* [bcrop  ] */
      1376       , 774      ,   /* [bds    ] */
      1280      ,  720      ,   /* [target ] */
    },
};

static int VTCALL_SIZE_LUT_3M3[][SIZE_OF_LUT] =
{
    /* Binning   = 2
       BNS ratio = 1.0
       BDS       = ON */

    /* 16:9 (VT_Call) */
    { SIZE_RATIO_16_9,
     (2016 + 0) ,(1134 + 0) ,   /* [sensor ] */
      2016      , 1134      ,   /* [bns    ] */
      2016      , 1134      ,   /* [bcrop  ] */
      2016      , 1134      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] */
    },
    /* 4:3 (VT_Call) */
    { SIZE_RATIO_4_3,
     (2016 + 0) ,(1512 + 0) ,   /* [sensor ] */
      2016      , 1512      ,   /* [bns    ] */
      2016      , 1512      ,   /* [bcrop  ] */
      2016      , 1512      ,   /* [bds    ] */
       640      ,  480      ,   /* [target ] */
    },
    /* 1:1 (VT_Call) */
    { SIZE_RATIO_1_1,
     (1504 + 0) ,(1504 + 0) ,   /* [sensor ] */
      1504      , 1504      ,   /* [bns    ] */
      1504      , 1504      ,   /* [bcrop  ] */
      1504      , 1504      ,   /* [bds    ] */
      1080      , 1080      ,   /* [target ] */
    },
    /* 3:2 (VT_Call) */
    { SIZE_RATIO_3_2,
     (2016 + 0) ,(1134 + 0) ,   /* [sensor ] */
      2016      , 1134      ,   /* [bns    ] */
      1680      , 1120      ,   /* [bcrop  ] */
      1680      , 1120      ,   /* [bds    ] */
      1616      , 1080      ,   /* [target ] */
    },
    /* 11:9 (VT_Call) */
    { SIZE_RATIO_11_9,
     (2016 + 0) ,(1134 + 0) ,   /* [sensor ] */
      2016      , 1134      ,   /* [bns    ] */
      1232      , 1008      ,   /* [bcrop  ] */
      1232      , 1008      ,   /* [bds    ] */
      1232      , 1008      ,   /* [target ] */
    }
};

static int FAST_AE_STABLE_SIZE_LUT_3M3[][SIZE_OF_LUT] =
{
    /* Binning   = ON
       BNS ratio = 4.0 / FPS = 120
       BDS       = ON */

    /* FAST_AE 16:9 (Single) */
    { SIZE_RATIO_16_9,
     (1344 + 0) , (756 + 0) ,   /* [sensor ] */
      1344      ,  756      ,   /* [bns    ] */
      1344      ,  756      ,   /* [bcrop  ] */
      1344      ,  756      ,   /* [bds    ] */
      1280      ,  720      ,   /* [target ] */
    },
};

static int PREVIEW_FULL_SIZE_LUT_3M3[][SIZE_OF_LUT] =
{
    /* Binning   = OFF
       BNS ratio = 1.0
       BDS       = 1080p */

    /* 16:9 (Single) */
    { SIZE_RATIO_16_9,
     (4032 + 0) ,(3024 + 0) ,   /* [sensor ] */
      4032      , 3024      ,   /* [bns    ] */
      4032      , 3024      ,   /* [bcrop  ] */
      2688      , 2016      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
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
     (4032 + 0) ,(3024 + 0) ,   /* [sensor ] */
      4032      , 3024      ,   /* [bns    ] */
      4032      , 3024      ,   /* [bcrop  ] */
      2688      , 2016      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
    /* 3:2 (Single) */
    { SIZE_RATIO_3_2,
     (4032 + 0) ,(3024 + 0) ,   /* [sensor ] */
      4032      , 3024      ,   /* [bns    ] */
      4032      , 3024      ,   /* [bcrop  ] */
      2688      , 2016      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
    /* 5:4 (Single) */
    { SIZE_RATIO_5_4,
     (4032 + 0) ,(3024 + 0) ,   /* [sensor ] */
      4032      , 3024      ,   /* [bns    ] */
      4032      , 3024      ,   /* [bcrop  ] */
      2688      , 2016      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
    /* 5:3 (Single) */
    { SIZE_RATIO_5_3,
     (4032 + 0) ,(3024 + 0) ,   /* [sensor ] */
      4032      , 3024      ,   /* [bns    ] */
      4032      , 3024      ,   /* [bcrop  ] */
      2688      , 2016      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
    /* 11:9 (Single) */
    { SIZE_RATIO_11_9,
     (4032 + 0) ,(3024 + 0) ,   /* [sensor ] */
      4032      , 3024      ,   /* [bns    ] */
      4032      , 3024      ,   /* [bcrop  ] */
      2688      , 2016      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
};

static int PICTURE_FULL_SIZE_LUT_3M3[][SIZE_OF_LUT] =
{
    /* Binning   = OFF
       BNS ratio = 1.0
       BDS       = 1080p */

    /* 16:9 (Single) */
    { SIZE_RATIO_16_9,
     (4032 + 0) ,(3024 + 0) ,   /* [sensor ] */
      4032      , 3024      ,   /* [bns    ] */
      4032      , 3024      ,   /* [bcrop  ] */
      4032      , 3024      ,   /* [bds    ] *//* UHD (3840x2160) special handling in ExynosCameraParameters class */
      4032      , 3024      ,   /* [target ] */
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
     (4032 + 0) ,(3024 + 0) ,   /* [sensor ] */
      4032      , 3024      ,   /* [bns    ] */
      4032      , 3024      ,   /* [bcrop  ] */
      4032      , 3024      ,   /* [bds    ] */
      4032      , 3024      ,   /* [target ] */
    },
    /* 3:2 (Single) */
    { SIZE_RATIO_3_2,
     (4032 + 0) ,(3024 + 0) ,   /* [sensor ] */
      4032      , 3024      ,   /* [bns    ] */
      4032      , 3024      ,   /* [bcrop  ] */
      4032      , 3024      ,   /* [bds    ] */
      4032      , 3024      ,   /* [target ] */
    },
    /* 5:4 (Single) */
    { SIZE_RATIO_5_4,
     (4032 + 0) ,(3024 + 0) ,   /* [sensor ] */
      4032      , 3024      ,   /* [bns    ] */
      4032      , 3024      ,   /* [bcrop  ] */
      4032      , 3024      ,   /* [bds    ] */
      4032      , 3024      ,   /* [target ] */
    },
    /* 5:3 (Single) */
    { SIZE_RATIO_5_3,
     (4032 + 0) ,(3024 + 0) ,   /* [sensor ] */
      4032      , 3024      ,   /* [bns    ] */
      4032      , 3024      ,   /* [bcrop  ] */
      4032      , 3024      ,   /* [bds    ] */
      4032      , 3024      ,   /* [target ] */
    },
    /* 11:9 (Single) */
    { SIZE_RATIO_11_9,
     (4032 + 0) ,(3024 + 0) ,   /* [sensor ] */
      4032      , 3024      ,   /* [bns    ] */
      4032      , 3024      ,   /* [bcrop  ] */
      4032      , 3024      ,   /* [bds    ] */
      4032      , 3024      ,   /* [target ] */
    },
};

/* yuv stream size list */
static int S5K3M3_YUV_LIST[][SIZE_OF_RESOLUTION] =
{
    /* { width, height, minFrameDuration, ratioId } */
    { 4032, 3024, 33331760, SIZE_RATIO_4_3},
    { 4032, 2268, 33331760, SIZE_RATIO_16_9},
    { 3024, 3024, 33331760, SIZE_RATIO_1_1},
    { 3984, 2988, 33331760, SIZE_RATIO_4_3},
    { 3840, 2160, 16665880, SIZE_RATIO_16_9},
    { 3264, 2448, 16665880, SIZE_RATIO_4_3},
    { 3264, 1836, 16665880, SIZE_RATIO_16_9},
    { 2976, 2976, 16665880, SIZE_RATIO_1_1},
    { 2880, 2160, 16665880, SIZE_RATIO_4_3},
    { 2560, 1440, 16665880, SIZE_RATIO_16_9},
    { 2160, 2160, 16665880, SIZE_RATIO_1_1},
    { 2048, 1152, 16665880, SIZE_RATIO_16_9},
    { 1920, 1080, 16665880, SIZE_RATIO_16_9},
    { 1440, 1080, 16665880, SIZE_RATIO_4_3},
    { 1088, 1088, 16665880, SIZE_RATIO_1_1},
    { 1280,  720, 16665880, SIZE_RATIO_16_9},
    { 1056,  704, 16665880, SIZE_RATIO_3_2},
    { 1024,  768, 16665880, SIZE_RATIO_4_3},
    {  960,  720, 16665880, SIZE_RATIO_4_3},
    {  800,  450, 16665880, SIZE_RATIO_16_9},
    {  720,  720, 16665880, SIZE_RATIO_1_1},
    {  720,  480, 16665880, SIZE_RATIO_3_2},
    {  640,  480, 16665880, SIZE_RATIO_4_3},
    {  352,  288, 16665880, SIZE_RATIO_11_9},
    {  320,  240, 16665880, SIZE_RATIO_4_3},
    {  256,  144, 16665880, SIZE_RATIO_16_9}, /* DngCreatorTest */
    {  176,  144, 16665880, SIZE_RATIO_11_9}, /* RecordingTest */
};

/* availble Jpeg size (only for  HAL_PIXEL_FORMAT_BLOB) */
static int S5K3M3_JPEG_LIST[][SIZE_OF_RESOLUTION] =
{
    /* { width, height, minFrameDuration, ratioId } */
    { 4032, 3024, 50000000, SIZE_RATIO_4_3},
    { 4032, 2268, 50000000, SIZE_RATIO_16_9},
    { 3024, 3024, 50000000, SIZE_RATIO_1_1},
    { 3984, 2988, 50000000, SIZE_RATIO_4_3},
    { 3840, 2160, 50000000, SIZE_RATIO_16_9},
    { 3264, 2448, 50000000, SIZE_RATIO_4_3},
    { 3264, 1836, 50000000, SIZE_RATIO_16_9},
    { 2976, 2976, 50000000, SIZE_RATIO_1_1},
    { 2880, 2160, 50000000, SIZE_RATIO_4_3},
    { 2560, 1440, 50000000, SIZE_RATIO_16_9},
    { 2160, 2160, 50000000, SIZE_RATIO_1_1},
    { 2048, 1152, 50000000, SIZE_RATIO_16_9},
    { 1920, 1080, 33331760, SIZE_RATIO_16_9},
    { 1440, 1080, 33331760, SIZE_RATIO_4_3},
    { 1088, 1088, 33331760, SIZE_RATIO_1_1},
    { 1280,  720, 33331760, SIZE_RATIO_16_9},
    { 1056,  704, 33331760, SIZE_RATIO_3_2},
    { 1024,  768, 33331760, SIZE_RATIO_4_3},
    {  960,  720, 33331760, SIZE_RATIO_4_3},
    {  800,  450, 33331760, SIZE_RATIO_16_9},
    {  720,  720, 33331760, SIZE_RATIO_1_1},
    {  720,  480, 33331760, SIZE_RATIO_3_2},
    {  640,  480, 33331760, SIZE_RATIO_4_3},
    {  352,  288, 33331760, SIZE_RATIO_11_9},
    {  320,  240, 33331760, SIZE_RATIO_4_3},
};

/* vendor static info : hidden preview size list */
static int S5K3M3_HIDDEN_PREVIEW_SIZE_LIST[][SIZE_OF_RESOLUTION] =
{
    { 2224, 1080, 16665880, SIZE_RATIO_18P5_9},
    { 1440, 1440, 16665880, SIZE_RATIO_1_1}, /* for 1440*1440 recording*/
};

/* vendor static info : hidden picture size list */
static int S5K3M3_HIDDEN_PICTURE_SIZE_LIST[][SIZE_OF_RESOLUTION] =
{
    { 4032, 1960, 50000000, SIZE_RATIO_18P5_9},
};

/* For HAL3 */
static int S5K3M3_HIGH_SPEED_VIDEO_LIST[][SIZE_OF_RESOLUTION] =
{
    { 2224, 1080, 0, SIZE_RATIO_18P5_9},
    { 1280,  720, 0, SIZE_RATIO_16_9},
};

static int S5K3M3_FPS_RANGE_LIST[][2] =
{
    //{   7000,   7000},
    {  15000,  15000},
    {  10000,  24000},
    {  15000,  24000},
    {  24000,  24000},
    //{   4000,  30000},
    {   7000,  30000},
    {  10000,  30000},
    {  15000,  30000},
    {  30000,  30000},
};

/* For HAL3 */
static int S5K3M3_HIGH_SPEED_VIDEO_FPS_RANGE_LIST[][2] =
{
    {  30000, 120000},
    { 120000, 120000},
};

/* vendor static info : width, height, min_fps, max_fps, vdis width, vdis height, recording limit time(sec) */
static int S5K3M3_AVAILABLE_VIDEO_LIST[][7] =
{
#ifdef USE_UHD_RECORDING
#ifdef SAMSUNG_SW_VDIS_UHD_20MARGIN
    { 3840, 2160, 30000, 30000, 4608, 2592, 600},
#else
    { 3840, 2160, 30000, 30000, 4032, 2268, 600},
#endif
#endif
#ifdef USE_WQHD_RECORDING
    { 2560, 1440, 30000, 30000, 3072, 1728, 0},
#endif
    { 2224, 1080, 30000, 30000, 2672, 1296, 0},
    { 1920, 1080, 60000, 60000, 2304, 1296, 600},
    { 1920, 1080, 30000, 30000, 2304, 1296, 0},
    { 1440, 1440, 30000, 30000, 0, 0, 0},
    { 1280,  720, 30000, 30000, 1536, 864, 0},
    {  640,  480, 30000, 30000, 0, 0, 0},
    {  320,  240, 30000, 30000, 0, 0, 0},    /* For support the CameraProvider lib of Message app*/
    {  176,  144, 30000, 30000, 0, 0, 0},    /* For support the CameraProvider lib of Message app*/
};

static camera_metadata_rational COLOR_MATRIX1_3M3_3X3[] =
{
    {593, 1024},  {78, 1024},  {-118, 1024},
    {-491, 1024}, {1412, 1024},{72, 1024},
    {-137, 1024}, {417, 1024}, {428, 1024}
};

static camera_metadata_rational COLOR_MATRIX2_3M3_3X3[] =
{
    {1032, 1024}, {-174, 1024}, {-276, 1024},
    {-442, 1024}, {1495, 1024}, {39, 1024},
    {-76, 1024},  {333, 1024},  {614, 1024}
};

static camera_metadata_rational FORWARD_MATRIX1_3M3_3X3[] =
{
    {893, 1024}, {-200, 1024}, {294, 1024},
    {317, 1024}, {697, 1024},  {10, 1024},
    {-16, 1024}, {-557, 1024}, {1418, 1024}
};

static camera_metadata_rational FORWARD_MATRIX2_3M3_3X3[] =
{
    {750, 1024}, {-150, 1024}, {387, 1024},
    {186, 1024}, {748, 1024},  {90, 1024},
    {-55, 1024}, {-987, 1024}, {1887, 1024}
};

#endif
