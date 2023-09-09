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

#ifndef EXYNOS_CAMERA_LUT_S5K2P7SX_H
#define EXYNOS_CAMERA_LUT_S5K2P7SX_H

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

static int PREVIEW_SIZE_LUT_S5K2P7SX[][SIZE_OF_LUT] =
{
    /* Binning   = 1
       BNS ratio = 1.0
       BDS       = OFF */

    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
     (4608 +  0),(3456 +  0),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      4608      , 2592      ,   /* [bcrop  ] */
      4608      , 2592      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] */
    },
    /* 4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
     (4608 +  0),(3456 +  0),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      4608      , 3456      ,   /* [bcrop  ] */
      4608      , 3456      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
    /* 1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
     (4608 +  0),(3456 +  0),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      3456      , 3456      ,   /* [bcrop  ] */
      3456      , 3456      ,   /* [bds    ] */
      1088      , 1088      ,   /* [target ] */
    },
#if 0 /* TBD */
    /* 3:2 (Single, Dual) */
    { SIZE_RATIO_3_2,
     (4608 +  0),(3456 + 0 ),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      4608      , 2688      ,   /* [bcrop  ] */
      4608      , 2688      ,   /* [bds    ] */
      1616      , 1080      ,   /* [target ] */
    },
    /* 5:4 (Single, Dual) */
    { SIZE_RATIO_5_4,
     (4608 +  0),(3456 + 0 ),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      3780      , 3456      ,   /* [bcrop  ] */
      3780      , 3456      ,   /* [bds    ] */
      1344      , 1080      ,   /* [target ] */
    },
    /* 5:3 (Single, Dual) */
    { SIZE_RATIO_5_3,
     (4608 +  0),(3456 + 0 ),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      4030      , 2418      ,   /* [bcrop  ] */
      4030      , 2418      ,   /* [bds    ] */
      1792      , 1080      ,   /* [target ] */
    },
    /* 11:9 (Single, Dual) */
    { SIZE_RATIO_11_9,
     (4608 +  0),(3456 + 0 ),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      3696      , 3456      ,   /* [bcrop  ] */
      3696      , 3456      ,   /* [bds    ] */
      1312      , 1080      ,   /* [target ] */
    }
#endif
};

static int PREVIEW_SIZE_LUT_S5K2P7SX_BNS[][SIZE_OF_LUT] =
{
    /* Binning   = 2
       BNS ratio = 1.0
       BDS       = OFF */

    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2304      , 1296      ,   /* [bcrop  ] */
      2304      , 1296      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] */
    },
    /* 4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2304      , 1728      ,   /* [bcrop  ] */
      2304      , 1728      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
#if 0 /* TBD */
    /* 1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
     (3456 + 0),(3456 + 0),   /* [sensor ] */
      2016      , 2016      ,   /* [bns    ] */
      2016      , 2016      ,   /* [bcrop  ] */
      2016      , 2016      ,   /* [bds    ] */
      1080      , 1080      ,   /* [target ] */
    },
#endif
    /* 3:2 (Single, Dual) */
    { SIZE_RATIO_3_2,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2304      , 1536      ,   /* [bcrop  ] */
      2304      , 1536      ,   /* [bds    ] */
      1616      , 1080      ,   /* [target ] */
    },
#if 0 /* TBD */
    /* 5:4 (Single, Dual) */
    { SIZE_RATIO_5_4,
     (4608 +  0),(3456 + 0 ),   /* [sensor ] */
      2688      , 1356      ,   /* [bns    ] */
      2530      , 1356      ,   /* [bcrop  ] */
      2530      , 1356      ,   /* [bds    ] */
      1344      , 1080      ,   /* [target ] */
    },
    /* 5:3 (Single, Dual) */
    { SIZE_RATIO_5_3,
     (4608 +  0),(3456 + 0 ),   /* [sensor ] */
      2688      , 1356      ,   /* [bns    ] */
      2688      , 1612      ,   /* [bcrop  ] */
      2688      , 1612      ,   /* [bds    ] */
      1792      , 1080      ,   /* [target ] */
    },
#endif
    /* 11:9 (Single, Dual) */
    { SIZE_RATIO_11_9,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2112      , 1728      ,   /* [bcrop  ] */
      2112      , 1728      ,   /* [bds    ] */
      1312      , 1080      ,   /* [target ] */
    }
};

static int PICTURE_SIZE_LUT_S5K2P7SX[][SIZE_OF_LUT] =
{
    /* Binning   = 1
       BNS ratio = 1.0
       BDS       = OFF */

    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
     (4608 +  0),(3456 +  0),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      4608      , 2592      ,   /* [bcrop  ] */
      4608      , 2592      ,   /* [bds    ] */
      4608      , 2592      ,   /* [target ] */
    },
    /* 4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
     (4608 +  0),(3456 +  0),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      4608      , 3456      ,   /* [bcrop  ] */
      4608      , 3456      ,   /* [bds    ] */
      4608      , 3456      ,   /* [target ] */
    },
    /* 1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
     (4608 +  0),(3456 +  0),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      3456      , 3456      ,   /* [bcrop  ] */
      3456      , 3456      ,   /* [bds    ] */
      3456      , 3456      ,   /* [target ] */
    }
};

static int VIDEO_SIZE_LUT_S5K2P7SX[][SIZE_OF_LUT] =
{
    /* Binning   = 1
       BNS ratio = 1.0
       BDS       = OFF */

    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
     (4608 +  0),(3456 +  0),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      4608      , 2592      ,   /* [bcrop  ] */
      4608      , 2592      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] */
    },
    /* 4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
     (4608 +  0),(3456 +  0),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      4608      , 3456      ,   /* [bcrop  ] */
      4608      , 3456      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
    /* 1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
     (4608 +  0),(3456 +  0),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      3456      , 3456      ,   /* [bcrop  ] */
      3456      , 3456      ,   /* [bds    ] */
      1088      , 1088      ,   /* [target ] */
    },
#if 0 /* TBD */
    /* 3:2 (Single, Dual) */
    { SIZE_RATIO_3_2,
     (4608 +  0),(3456 + 0 ),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      4608      , 2688      ,   /* [bcrop  ] */
      4608      , 2688      ,   /* [bds    ] */
      1616      , 1080      ,   /* [target ] */
    },
    /* 5:4 (Single, Dual) */
    { SIZE_RATIO_5_4,
     (4608 +  0),(3456 + 0 ),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      3780      , 3456      ,   /* [bcrop  ] */
      3780      , 3456      ,   /* [bds    ] */
      1344      , 1080      ,   /* [target ] */
    },
    /* 5:3 (Single, Dual) */
    { SIZE_RATIO_5_3,
     (4608 +  0),(3456 + 0 ),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      4030      , 2418      ,   /* [bcrop  ] */
      4030      , 2418      ,   /* [bds    ] */
      1792      , 1080      ,   /* [target ] */
    },
    /* 11:9 (Single, Dual) */
    { SIZE_RATIO_11_9,
     (4608 +  0),(3456 + 0 ),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      3696      , 3456      ,   /* [bcrop  ] */
      3696      , 3456      ,   /* [bds    ] */
      1312      , 1080      ,   /* [target ] */
    }
#endif
};

static int VIDEO_SIZE_LUT_S5K2P7SX_BNS[][SIZE_OF_LUT] =
{
    /* Binning   = 2
       BNS ratio = 1.0
       BDS       = OFF */

    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2304      , 1296      ,   /* [bcrop  ] */
      2304      , 1296      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] */
    },
    /* 4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2304      , 1728      ,   /* [bcrop  ] */
      2304      , 1728      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
#if 0 /* TBD */
    /* 1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
     (3456 + 0),(3456 + 0),   /* [sensor ] */
      2016      , 2016      ,   /* [bns    ] */
      2016      , 2016      ,   /* [bcrop  ] */
      2016      , 2016      ,   /* [bds    ] */
      1080      , 1080      ,   /* [target ] */
    },
#endif
    /* 3:2 (Single, Dual) */
    { SIZE_RATIO_3_2,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2304      , 1536      ,   /* [bcrop  ] */
      2304      , 1536      ,   /* [bds    ] */
      1616      , 1080      ,   /* [target ] */
    },
#if 0 /* TBD */
    /* 5:4 (Single, Dual) */
    { SIZE_RATIO_5_4,
     (4608 +  0),(3456 + 0 ),   /* [sensor ] */
      2688      , 1356      ,   /* [bns    ] */
      2530      , 1356      ,   /* [bcrop  ] */
      2530      , 1356      ,   /* [bds    ] */
      1344      , 1080      ,   /* [target ] */
    },
    /* 5:3 (Single, Dual) */
    { SIZE_RATIO_5_3,
     (4608 +  0),(3456 + 0 ),   /* [sensor ] */
      2688      , 1356      ,   /* [bns    ] */
      2688      , 1612      ,   /* [bcrop  ] */
      2688      , 1612      ,   /* [bds    ] */
      1792      , 1080      ,   /* [target ] */
    },
#endif
    /* 11:9 (Single, Dual) */
    { SIZE_RATIO_11_9,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2112      , 1728      ,   /* [bcrop  ] */
      2112      , 1728      ,   /* [bds    ] */
      1312      , 1080      ,   /* [target ] */
    }
};

static int VIDEO_SIZE_LUT_120FPS_HIGH_SPEED_S5K2P7SX_BNS[][SIZE_OF_LUT] =
{
    /* Binning   = 4
       BNS ratio = 1.0
       BDS       = OFF */

    /* HD_120 16:9 (Single) */
    { SIZE_RATIO_16_9,
     (2304 +  0),(1296 +  0),   /* [sensor ] */
      2304      , 1296      ,   /* [bns    ] */
      2304      , 1296      ,   /* [bcrop  ] */
      2304      , 1296      ,   /* [bds    ] */
      1280      ,  720      ,   /* [target ] */
    },
#if 0 /* TBD */
    /* HD_120 4:3 (Single) */
    { SIZE_RATIO_4_3,
     (2016 + 0) ,(1136 + 0) ,   /* [sensor ] */
      2016      , 1136      ,   /* [bns    ] */
      1504      , 1128      ,   /* [bcrop  ] */
      1504      , 1128      ,   /* [bds    ] */
       960      ,  720      ,   /* [target ] */
    },
    /* HD_120 1:1 (Single) */
    { SIZE_RATIO_1_1,
     (2016 + 0) ,(1136 + 0) ,   /* [sensor ] */
      2016      , 1136      ,   /* [bns    ] */
      1120      , 1120      ,   /* [bcrop  ] */
      1120      , 1120      ,   /* [bds    ] */
       720      ,  720      ,   /* [target ] */
    },
    /* HD_120 3:2 (Single) */
    { SIZE_RATIO_3_2,
     (2016 + 0) ,(1136 + 0) ,   /* [sensor ] */
      2016      , 1136      ,   /* [bns    ] */
      1680      , 1120      ,   /* [bcrop  ] */
      1680      , 1120      ,   /* [bds    ] */
      1056      ,  704      ,   /* [target ] */
    }
#endif
};

static int VTCALL_SIZE_LUT_S5K2P7SX[][SIZE_OF_LUT] =
{
    /* Binning   = 2
       BNS ratio = 1.0
       BDS       = OFF */

    /* 16:9 (VT_Call) */
    { SIZE_RATIO_16_9,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2304      , 1296      ,   /* [bcrop  ] */
      2304      , 1296      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] */
    },
    /* 4:3 (VT_Call) */
    { SIZE_RATIO_4_3,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2304      , 1728      ,   /* [bcrop  ] */
      2304      , 1728      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
#if 0 /* TBD */
    /* 1:1 (VT_Call) */
    { SIZE_RATIO_1_1,
     (3456 + 0),(3456 + 0),   /* [sensor ] */
      2016      , 2016      ,   /* [bns    ] */
      2016      , 2016      ,   /* [bcrop  ] */
      2016      , 2016      ,   /* [bds    ] */
      1080      , 1080      ,   /* [target ] */
    },
#endif
    /* 3:2 (VT_Call) */
    { SIZE_RATIO_3_2,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2304      , 1536      ,   /* [bcrop  ] */
      2304      , 1536      ,   /* [bds    ] */
      1616      , 1080      ,   /* [target ] */
    },
#if 0 /* TBD */
    /* 5:4 (VT_Call) */
    { SIZE_RATIO_5_4,
     (4608 +  0),(3456 + 0 ),   /* [sensor ] */
      2688      , 1356      ,   /* [bns    ] */
      2530      , 1356      ,   /* [bcrop  ] */
      2530      , 1356      ,   /* [bds    ] */
      1344      , 1080      ,   /* [target ] */
    },
    /* 5:3 (VT_Call) */
    { SIZE_RATIO_5_3,
     (4608 +  0),(3456 + 0 ),   /* [sensor ] */
      2688      , 1356      ,   /* [bns    ] */
      2688      , 1612      ,   /* [bcrop  ] */
      2688      , 1612      ,   /* [bds    ] */
      1792      , 1080      ,   /* [target ] */
    },
#endif
    /* 11:9 (VT_Call) */
    { SIZE_RATIO_11_9,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2112      , 1728      ,   /* [bcrop  ] */
      2112      , 1728      ,   /* [bds    ] */
      1312      , 1080      ,   /* [target ] */
    }
};

static int LIVE_BROADCAST_SIZE_LUT_S5K2P7SX[][SIZE_OF_LUT] =
{
    /* Binning   = 2
       BNS ratio = 1.0
       BDS       = OFF */

    /* 16:9 (VT_Call) */
    { SIZE_RATIO_16_9,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2304      , 1296      ,   /* [bcrop  ] */
      2304      , 1296      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] */
    },
    /* 4:3 (VT_Call) */
    { SIZE_RATIO_4_3,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2304      , 1728      ,   /* [bcrop  ] */
      2304      , 1728      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
#if 0 /* TBD */
    /* 1:1 (VT_Call) */
    { SIZE_RATIO_1_1,
     (3456 + 0),(3456 + 0),   /* [sensor ] */
      2016      , 2016      ,   /* [bns    ] */
      2016      , 2016      ,   /* [bcrop  ] */
      2016      , 2016      ,   /* [bds    ] */
      1080      , 1080      ,   /* [target ] */
    },
#endif
    /* 3:2 (VT_Call) */
    { SIZE_RATIO_3_2,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2304      , 1536      ,   /* [bcrop  ] */
      2304      , 1536      ,   /* [bds    ] */
      1616      , 1080      ,   /* [target ] */
    },
#if 0 /* TBD */
    /* 5:4 (VT_Call) */
    { SIZE_RATIO_5_4,
     (4608 +  0),(3456 + 0 ),   /* [sensor ] */
      2688      , 1356      ,   /* [bns    ] */
      2530      , 1356      ,   /* [bcrop  ] */
      2530      , 1356      ,   /* [bds    ] */
      1344      , 1080      ,   /* [target ] */
    },
    /* 5:3 (VT_Call) */
    { SIZE_RATIO_5_3,
     (4608 +  0),(3456 + 0 ),   /* [sensor ] */
      2688      , 1356      ,   /* [bns    ] */
      2688      , 1612      ,   /* [bcrop  ] */
      2688      , 1612      ,   /* [bds    ] */
      1792      , 1080      ,   /* [target ] */
    },
#endif
    /* 11:9 (VT_Call) */
    { SIZE_RATIO_11_9,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2112      , 1728      ,   /* [bcrop  ] */
      2112      , 1728      ,   /* [bds    ] */
      1312      , 1080      ,   /* [target ] */
    }
};

static int FAST_AE_STABLE_SIZE_LUT_S5K2P7SX_BNS[][SIZE_OF_LUT] =
{
    /* Binning   = 4
       BNS ratio = 1.0 / FPS = 120
       BDS       = OFF */

    /* FAST_AE 16:9 (Single) */
    { SIZE_RATIO_16_9,
     (2304 +  0),(1296 +  0),   /* [sensor ] */
      2304      , 1296      ,   /* [bns    ] */
      2304      , 1296      ,   /* [bcrop  ] */
      2304      , 1296      ,   /* [bds    ] */
      1280      ,  720      ,   /* [target ] */
    },
};

static int YUV_SIZE_LUT_S5K2P7SX[][SIZE_OF_LUT] =
{
    /* Binning   = 1
       BNS ratio = 1.0
       BDS       = OFF */

    /* 16:9 (Single) */
    { SIZE_RATIO_16_9,
     (4608 +  0),(3456 +  0),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      4608      , 2592      ,   /* [bcrop  ] */
      4608      , 2592      ,   /* [bds    ] */
      4608      , 2592      ,   /* [target ] */
    },
    /* 4:3 (Single) */
    { SIZE_RATIO_4_3,
     (4608 +  0),(3456 +  0),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      4608      , 3456      ,   /* [bcrop  ] */
      4608      , 3456      ,   /* [bds    ] */
      4608      , 3456      ,   /* [target ] */
    },
    /* 1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
     (4608 +  0),(3456 +  0),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      3456      , 3456      ,   /* [bcrop  ] */
      3456      , 3456      ,   /* [bds    ] */
      3456      , 3456      ,   /* [target ] */
    },
    /* 3:2 (Single) */
    { SIZE_RATIO_3_2,
     (4608 +  0),(3456 +  0),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      4608      , 2688      ,   /* [bcrop  ] */
      4608      , 2688      ,   /* [bds    ] */
      4608      , 2688      ,   /* [target ] */
    },
    /* 5:4 (Single) */
    { SIZE_RATIO_5_4,
     (4608 +  0),(3456 +  0),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      3780      , 3456      ,   /* [bcrop  ] */
      3780      , 3456      ,   /* [bds    ] */
      3780      , 3456      ,   /* [target ] */
    },
    /* 5:3 (Single) */
    { SIZE_RATIO_5_3,
     (4608 +  0),(3456 +  0),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      4030      , 2418      ,   /* [bcrop  ] */
      4030      , 2418      ,   /* [bds    ] */
      4030      , 2418      ,   /* [target ] */
    },
    /* 11:9 (Single) */
    { SIZE_RATIO_11_9,
     (4608 +  0),(3456 +  0),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      3696      , 3456      ,   /* [bcrop  ] */
      3696      , 3456      ,   /* [bds    ] */
      3696      , 3456      ,   /* [target ] */
    }
};

static int DEPTH_MAP_SIZE_LUT_S5K2P7SX[][SIZE_OF_RESOLUTION] =
{
    { SIZE_RATIO_16_9,
        504      ,  282      ,   /* [vci    ] */
    },
    { SIZE_RATIO_4_3,
        504      ,  378      ,   /* [vci    ] */
    },
    { SIZE_RATIO_1_1,
        376      ,  376      ,   /* [vci    ] */
    },
    { SIZE_RATIO_3_2,
        504      ,  378      ,   /* [vci    ] */
    },
    { SIZE_RATIO_5_4,
        504      ,  378      ,   /* [vci    ] */
    },
    { SIZE_RATIO_5_3,
        504      ,  378      ,   /* [vci    ] */
    },
    { SIZE_RATIO_11_9,
        504      ,  378      ,   /* [vci    ] */
    },
};

static int S5K2P7SX_PREVIEW_LIST[][SIZE_OF_RESOLUTION] =
{
#if defined(CAMERA_LCD_SIZE) && (CAMERA_LCD_SIZE >= LCD_SIZE_2560_1440)
    { 2560, 1440, SIZE_RATIO_16_9},
    { 1920, 1440, SIZE_RATIO_4_3},
    { 1440, 1440, SIZE_RATIO_1_1},
#endif
    { 1920, 1080, SIZE_RATIO_16_9},
    { 1440, 1080, SIZE_RATIO_4_3},
    { 1088, 1088, SIZE_RATIO_1_1},
    { 1280,  720, SIZE_RATIO_16_9},
    { 1056,  704, SIZE_RATIO_3_2},
    { 1024,  768, SIZE_RATIO_4_3},
    {  960,  720, SIZE_RATIO_4_3},
    {  800,  450, SIZE_RATIO_16_9},
    {  720,  720, SIZE_RATIO_1_1},
    {  720,  480, SIZE_RATIO_3_2},
    {  640,  480, SIZE_RATIO_4_3},
/* HACK: Exynos8890 EVT0 is not support */
#if !defined (JUNGFRAU_EVT0)
    {  352,  288, SIZE_RATIO_11_9},
    {  320,  240, SIZE_RATIO_4_3},
/* HACK: Exynos7870 is not support */
#if !defined (SMDK7870)
    {  256,  144, SIZE_RATIO_16_9}, /* for CAMERA2_API_SUPPORT */
    {  176,  144, SIZE_RATIO_11_9}
#endif
#endif
};

static int S5K2P7SX_HIDDEN_PREVIEW_LIST[][SIZE_OF_RESOLUTION] =
{
#if !(defined(CAMERA_LCD_SIZE) && (CAMERA_LCD_SIZE >= LCD_SIZE_1920_1080))
    { 1920, 1080, SIZE_RATIO_16_9},
    { 1440, 1080, SIZE_RATIO_4_3},
    { 1088, 1088, SIZE_RATIO_1_1},
#endif
#if defined(CAMERA_LCD_SIZE) && (CAMERA_LCD_SIZE >= LCD_SIZE_2560_1440)
    { 3840, 2160, SIZE_RATIO_16_9},
    { 3264, 1836, SIZE_RATIO_16_9},
    { 2560, 1440, SIZE_RATIO_16_9},
    { 1600, 1200, SIZE_RATIO_4_3},
#endif
    { 1280,  960, SIZE_RATIO_4_3},
    { 1056,  864, SIZE_RATIO_11_9},
    {  960,  960, SIZE_RATIO_1_1},  /* For clip movie */
    {  528,  432, SIZE_RATIO_11_9},
    {  800,  480, SIZE_RATIO_5_3},
    {  672,  448, SIZE_RATIO_3_2},
/* HACK: Exynos8890 EVT0 is not support */
#if !defined (JUNGFRAU_EVT0)
    {  480,  320, SIZE_RATIO_3_2},
    {  480,  270, SIZE_RATIO_16_9},
#endif
};

static int S5K2P7SX_PICTURE_LIST[][SIZE_OF_RESOLUTION] =
{
    { 4608, 3456, SIZE_RATIO_4_3},
    { 4608, 2592, SIZE_RATIO_16_9},
    { 3456, 3456, SIZE_RATIO_1_1},
    { 3264, 2448, SIZE_RATIO_4_3},
    { 3264, 1836, SIZE_RATIO_16_9},
    { 2448, 2448, SIZE_RATIO_1_1},
    { 2048, 1152, SIZE_RATIO_16_9},
    { 1920, 1080, SIZE_RATIO_16_9},
    { 1280,  720, SIZE_RATIO_16_9},
    {  960,  720, SIZE_RATIO_4_3},
    {  640,  480, SIZE_RATIO_4_3},
    {  320,  240, SIZE_RATIO_4_3}, /* for cts2 testAvailableStreamConfigs */
    {  256,  144, SIZE_RATIO_16_9}, /*  for cts2 testSingleImageThumbnail */
};

static int S5K2P7SX_HIDDEN_PICTURE_LIST[][SIZE_OF_RESOLUTION] =
{
    { 4128, 3096, SIZE_RATIO_4_3},
    { 4128, 2322, SIZE_RATIO_16_9},
    { 4096, 3072, SIZE_RATIO_4_3},
    { 4096, 2304, SIZE_RATIO_16_9},
    { 3840, 2160, SIZE_RATIO_16_9},
    { 3456, 2592, SIZE_RATIO_4_3},
    { 3200, 2400, SIZE_RATIO_4_3},
    { 3072, 1728, SIZE_RATIO_16_9},
    { 2988, 2988, SIZE_RATIO_1_1},
    { 2592, 2592, SIZE_RATIO_1_1},
    { 2592, 1944, SIZE_RATIO_4_3},
    { 2592, 1936, SIZE_RATIO_4_3}, /* not exactly matched ratio */
    { 2560, 1920, SIZE_RATIO_4_3},
    { 2048, 1536, SIZE_RATIO_4_3},
    { 1280,  960, SIZE_RATIO_4_3}, /* for VtCamera Test*/
    {  720,  720, SIZE_RATIO_1_1}, /* dummy size for binning mode */
    {  352,  288, SIZE_RATIO_11_9}, /* dummy size for binning mode */
};

static int S5K2P7SX_THUMBNAIL_LIST[][SIZE_OF_RESOLUTION] =
{
    {  512,  384, SIZE_RATIO_4_3},
    {  512,  288, SIZE_RATIO_16_9},
    {  384,  384, SIZE_RATIO_1_1},
/* TODO : will be supported after enable S/W scaler correctly */
//  {  320,  240, SIZE_RATIO_4_3},
    {    0,    0, SIZE_RATIO_1_1}
};

static int S5K2P7SX_VIDEO_LIST[][SIZE_OF_RESOLUTION] =
{
    { 1920, 1080, SIZE_RATIO_16_9},
    { 1440, 1440, SIZE_RATIO_1_1},
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

static int S5K2P7SX_HIDDEN_VIDEO_LIST[][SIZE_OF_RESOLUTION] =
{
#ifdef USE_UHD_RECORDING
    { 3840, 2160, SIZE_RATIO_16_9},
#endif
#ifdef USE_WQHD_RECORDING
    { 2560, 1440, SIZE_RATIO_16_9},
#endif
    {  960,  960, SIZE_RATIO_1_1},  /* for Clip Movie */
    {  864,  480, SIZE_RATIO_16_9}, /* for PLB mode */
    {  432,  240, SIZE_RATIO_16_9}, /* for PLB mode */
};

/* For HAL3 */
static int S5K2P7SX_YUV_LIST[][SIZE_OF_RESOLUTION] =
{
    { 4608, 3456, SIZE_RATIO_4_3},
    { 4608, 2592, SIZE_RATIO_16_9},
    { 3456, 3456, SIZE_RATIO_1_1},
    { 3984, 2988, SIZE_RATIO_4_3},
    { 3264, 2448, SIZE_RATIO_4_3},
    { 3264, 1836, SIZE_RATIO_16_9},
    { 2976, 2976, SIZE_RATIO_1_1},
    { 2880, 2160, SIZE_RATIO_4_3},
    { 2560, 1440, SIZE_RATIO_16_9},
    { 2160, 2160, SIZE_RATIO_1_1},
    { 2048, 1152, SIZE_RATIO_16_9},
    { 1920, 1080, SIZE_RATIO_16_9},
    { 1440, 1080, SIZE_RATIO_4_3},
    { 1088, 1088, SIZE_RATIO_1_1},
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
    {  256,  144, SIZE_RATIO_16_9}, /* DngCreatorTest */
    {  176,  144, SIZE_RATIO_11_9}, /* RecordingTest */
};

/* availble Jpeg size (only for  HAL_PIXEL_FORMAT_BLOB) */
static int S5K2P7SX_JPEG_LIST[][SIZE_OF_RESOLUTION] =
{
    { 4608, 3456, SIZE_RATIO_4_3},
    { 4608, 2592, SIZE_RATIO_16_9},
    { 3456, 3456, SIZE_RATIO_1_1},
    { 3984, 2988, SIZE_RATIO_4_3},
    { 3264, 2448, SIZE_RATIO_4_3},
    { 3264, 1836, SIZE_RATIO_16_9},
    { 2976, 2976, SIZE_RATIO_1_1},
    { 2880, 2160, SIZE_RATIO_4_3},
    { 2560, 1440, SIZE_RATIO_16_9},
    { 2160, 2160, SIZE_RATIO_1_1},
    { 2048, 1152, SIZE_RATIO_16_9},
    { 1920, 1080, SIZE_RATIO_16_9},
    { 1440, 1080, SIZE_RATIO_4_3},
    { 1088, 1088, SIZE_RATIO_1_1},
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
};

/* For HAL3 */
static int S5K2P7SX_HIGH_SPEED_VIDEO_LIST[][SIZE_OF_RESOLUTION] =
{
    { 1280,  720, SIZE_RATIO_16_9},
};

static int S5K2P7SX_FPS_RANGE_LIST[][2] =
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

static int S5K2P7SX_HIDDEN_FPS_RANGE_LIST[][2] =
{
    {  10000,  24000},
    {  30000,  60000},
    {  60000,  60000},
    {  60000, 120000},
    { 120000, 120000},
    { 240000, 240000},
};

/* For HAL3 */
static int S5K2P7SX_HIGH_SPEED_VIDEO_FPS_RANGE_LIST[][2] =
{
    {  30000, 120000},
    { 120000, 120000},
    {  30000, 240000},
    { 240000, 240000},
};

static camera_metadata_rational UNIT_MATRIX_S5K2P7SX_3X3[] =
{
    {128, 128}, {0, 128}, {0, 128},
    {0, 128}, {128, 128}, {0, 128},
    {0, 128}, {0, 128}, {128, 128}
};

static camera_metadata_rational COLOR_MATRIX1_S5K2P7SX_3X3[] =
{
    {593, 1024},  {78, 1024},  {-118, 1024},
    {-491, 1024}, {1412, 1024},{72, 1024},
    {-137, 1024}, {417, 1024}, {428, 1024}
};

static camera_metadata_rational COLOR_MATRIX2_S5K2P7SX_3X3[] =
{
    {1032, 1024}, {-174, 1024}, {-276, 1024},
    {-442, 1024}, {1495, 1024}, {39, 1024},
    {-76, 1024},  {333, 1024},  {614, 1024}
};

static camera_metadata_rational FORWARD_MATRIX1_S5K2P7SX_3X3[] =
{
    {893, 1024}, {-200, 1024}, {294, 1024},
    {317, 1024}, {697, 1024},  {10, 1024},
    {-16, 1024}, {-557, 1024}, {1418, 1024}
};

static camera_metadata_rational FORWARD_MATRIX2_S5K2P7SX_3X3[] =
{
    {750, 1024}, {-150, 1024}, {387, 1024},
    {186, 1024}, {748, 1024},  {90, 1024},
    {-55, 1024}, {-987, 1024}, {1887, 1024}
};
#endif
