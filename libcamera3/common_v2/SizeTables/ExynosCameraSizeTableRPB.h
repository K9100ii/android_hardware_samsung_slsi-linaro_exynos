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

#ifndef EXYNOS_CAMERA_LUT_RPB_H
#define EXYNOS_CAMERA_LUT_RPB_H

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

static int PREVIEW_SIZE_LUT_RPB[][SIZE_OF_LUT] =
{
    /* Binning   = OFF
       BNS ratio = 1.0
       BDS       = 1440p */

    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
     (4656 + 0) ,(2656 + 0),   /* [sensor ] */
      4656      , 2656      ,   /* [bns    ] */
      4656      , 2620      ,   /* [bcrop  ] */
      1920      , 1080      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] */
    },
    /* 4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
     (4656 + 0) ,(3520 + 0),   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      4656      , 3492      ,   /* [bcrop  ] */
      1440      , 1080      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
    /* 1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
     (4656 + 0) ,(3520 + 0),   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      3488      , 3488      ,   /* [bcrop  ] */
      1080      , 1080      ,   /* [bds    ] */
      1080      , 1080      ,   /* [target ] */
    },
    /* 3:2 (Single, Dual) */
    { SIZE_RATIO_3_2,
     (4656 + 0) ,(3520 + 0),   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      4656      , 3104      ,   /* [bcrop  ] */
      1616      , 1080      ,   /* [bds    ] */
      1616      , 1080      ,   /* [target ] */
    },
    /* 5:4 (Single, Dual) */
    { SIZE_RATIO_5_4,
     (4656 + 0) ,(3520 + 0),   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      4366      , 3492      ,   /* [bcrop  ] */
      1344      , 1080      ,   /* [bds    ] */
      1344      , 1080      ,   /* [target ] */
    },
    /* 5:3 (Single, Dual) */
    { SIZE_RATIO_5_3,
     (4656 + 0) ,(3520 + 0),   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      4656      , 2794      ,   /* [bcrop  ] */
      1792      , 1080      ,   /* [bds    ] */
      1792      , 1080      ,   /* [target ] */
    },
    /* 11:9 (Single, Dual) */
    { SIZE_RATIO_11_9,
     (4656 + 0) ,(3520 + 0),   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      4268      , 3492      ,   /* [bcrop  ] */
      1312      , 1080      ,   /* [bds    ] */
      1312      , 1080      ,   /* [target ] */
    },
    /* Dummy (not used) */
    { SIZE_RATIO_9_16,
     (4656 + 0) ,(3520 + 0),   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      4268      , 3492      ,   /* [bcrop  ] */
      1312      , 1080      ,   /* [bds    ] */
      1312      , 1080      ,   /* [target ] */
    },
    /* 18.5:9 (Single, Dual) */
    { SIZE_RATIO_18P5_9,
     (4656 + 0) ,(3520 + 0),   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      4634      , 2254      ,   /* [bcrop  ] */
      2224      , 1080      ,   /* [bds    ] */
      2224      , 1080      ,   /* [target ] */
    }
};

static int PREVIEW_SIZE_LUT_RPB_BNS[][SIZE_OF_LUT] =
{
    /* No Support for BNS(so same with 1.0 BNS ratio
       Binning   = OFF
       BNS ratio = 1.5
       BDS       = 1440p */

    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
     (4656 + 0) ,(2656 + 0),   /* [sensor ] */
      4656      , 2656      ,   /* [bns    ] */
      4656      , 2620      ,   /* [bcrop  ] */
      1920      , 1080      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] */
    },
    /* 4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
     (4656 + 0) ,(3520 + 0),   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      4656      , 3492      ,   /* [bcrop  ] */
      1440      , 1080      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
    /* 1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
     (4656 + 0) ,(3520 + 0),   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      3488      , 3488      ,   /* [bcrop  ] */
      1080      , 1080      ,   /* [bds    ] */
      1080      , 1080      ,   /* [target ] */
    },
    /* 3:2 (Single, Dual) */
    { SIZE_RATIO_3_2,
     (4656 + 0) ,(3520 + 0),   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      4656      , 3104      ,   /* [bcrop  ] */
      1616      , 1080      ,   /* [bds    ] */
      1616      , 1080      ,   /* [target ] */
    },
    /* 5:4 (Single, Dual) */
    { SIZE_RATIO_5_4,
     (4656 + 0) ,(3520 + 0),   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      4366      , 3492      ,   /* [bcrop  ] */
      1344      , 1080      ,   /* [bds    ] */
      1344      , 1080      ,   /* [target ] */
    },
    /* 5:3 (Single, Dual) */
    { SIZE_RATIO_5_3,
     (4656 + 0) ,(3520 + 0),   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      4656      , 2794      ,   /* [bcrop  ] */
      1792      , 1080      ,   /* [bds    ] */
      1792      , 1080      ,   /* [target ] */
    },
    /* 11:9 (Single, Dual) */
    { SIZE_RATIO_11_9,
     (4656 + 0) ,(3520 + 0),   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      4268      , 3492      ,   /* [bcrop  ] */
      1312      , 1080      ,   /* [bds    ] */
      1312      , 1080      ,   /* [target ] */
    },
    /* Dummy (not used) */
    { SIZE_RATIO_9_16,
     (4656 + 0) ,(3520 + 0),   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      4268      , 3492      ,   /* [bcrop  ] */
      1312      , 1080      ,   /* [bds    ] */
      1312      , 1080      ,   /* [target ] */
    },
    /* 18.5:9 (Single, Dual) */
    { SIZE_RATIO_18P5_9,
     (4656 + 0) ,(3520 + 0),   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      4634      , 2254      ,   /* [bcrop  ] */
      2224      , 1080      ,   /* [bds    ] */
      2224      , 1080      ,   /* [target ] */
    }
};

static int PICTURE_SIZE_LUT_RPB[][SIZE_OF_LUT] =
{
    /* Binning   = OFF
       BNS ratio = 1.0
       BDS       = OFF */

    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
     (4656 + 0) ,(2656 + 0),   /* [sensor ] */
      4656      , 2656      ,   /* [bns    ] */
      4656      , 2620      ,   /* [bcrop  ] */
      4656      , 2620      ,   /* [bds    ] */
      4656      , 2620      ,   /* [target ] */
    },
    /* 4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
     (4656 + 0) ,(3520 + 0),   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      4656      , 3492      ,   /* [bcrop  ] */
      4656      , 4492      ,   /* [bds    ] */
      4656      , 4492      ,   /* [target ] */
    },
    /* 1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
     (4656 + 0) ,(3520 + 0),   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      3488      , 3488      ,   /* [bcrop  ] */
      3488      , 3488      ,   /* [bds    ] */
      3488      , 3488      ,   /* [target ] */
    },
    /* 18.5:9 (Single, Dual) */
    { SIZE_RATIO_18P5_9,
     (4656 + 0) ,(3520 + 0),   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      4634      , 2254      ,   /* [bcrop  ] */
      4634      , 2254      ,   /* [bds    ] */
      4634      , 2254      ,   /* [target ] */
    }
};

static int VIDEO_SIZE_LUT_RPB[][SIZE_OF_LUT] =
{
    /* Binning   = OFF
       BNS ratio = 1.0
       BDS       = 1.5 */

    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
     (4656 + 0) ,(2656 + 0),   /* [sensor ] */
      4656      , 2656      ,   /* [bns    ] */
      4656      , 2620      ,   /* [bcrop  ] */
      3104      , 1746      ,   /* [bds    ] *//* UHD (3840x2160) special handling in ExynosCameraParameters class */
      1920      , 1080      ,   /* [target ] */
    },
    /* 4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
     (4656 + 0) ,(3520 + 0),   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      4656      , 3492      ,   /* [bcrop  ] */
      3104      , 2328      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
    /* 1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
     (4656 + 0) ,(3520 + 0),   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      3488      , 3488      ,   /* [bcrop  ] */
      2328      , 2328      ,   /* [bds    ] */
      1080      , 1080      ,   /* [target ] */
    },
    /* 3:2 (Single, Dual) */
    { SIZE_RATIO_3_2,
     (4656 + 0) ,(3520 + 0),   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      4656      , 3104      ,   /* [bcrop  ] */
      3104      , 2070      ,   /* [bds    ] */
      1616      , 1080      ,   /* [target ] */
    },
    /* 5:4 (Single, Dual) */
    { SIZE_RATIO_5_4,
     (4656 + 0) ,(3520 + 0),   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      4366      , 3492      ,   /* [bcrop  ] */
      2910      , 2328      ,   /* [bds    ] */
      1344      , 1080      ,   /* [target ] */
    },
    /* 5:3 (Single, Dual) */
    { SIZE_RATIO_5_3,
     (4656 + 0) ,(3520 + 0),   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      4656      , 2794      ,   /* [bcrop  ] */
      3104      , 1862      ,   /* [bds    ] */
      1792      , 1080      ,   /* [target ] */
    },
    /* 11:9 (Single, Dual) */
    { SIZE_RATIO_11_9,
     (4656 + 0) ,(3520 + 0),   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      4268      , 3492      ,   /* [bcrop  ] */
      2840      , 2326      ,   /* [bds    ] */
      1312      , 1080      ,   /* [target ] */
    },
};

static int VIDEO_SIZE_LUT_120FPS_HIGH_SPEED_RPB[][SIZE_OF_LUT] =
{
    /* Binning   = ON
       BNS ratio = 1.0
       BDS       = ON */

    /* HD_120 16:9 (Single) */
    { SIZE_RATIO_16_9,
     (1936 + 0) ,(1104 + 0) ,   /* [sensor ] */
      1936      , 1104      ,   /* [bns    ] */
      1920      , 1080      ,   /* [bcrop  ] */
      1920      , 1080      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] */
    },
};

static int VIDEO_SIZE_LUT_240FPS_HIGH_SPEED_RPB[][SIZE_OF_LUT] =
{
    /* Binning   = ON
       BNS ratio = 1.0
       BDS       = ON */

    /* HD_240 16:9 (Single) */
    { SIZE_RATIO_16_9,
     (1936 + 0) ,(1104 + 0) ,   /* [sensor ] */
      1936      , 1104      ,   /* [bns    ] */
      1920      , 1080      ,   /* [bcrop  ] */
      1920      , 1080      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] */
    },
};

static int VIDEO_SIZE_LUT_480FPS_HIGH_SPEED_RPB[][SIZE_OF_LUT] =
{
    /* Binning   = ON
       BNS ratio = 1.0
       BDS       = ON */

    /* HD_480 16:9 (Single) */
    { SIZE_RATIO_16_9,
     (1936 + 0) ,(1104 + 0) ,   /* [sensor ] */
      1936      , 1104      ,   /* [bns    ] */
      1920      , 1080      ,   /* [bcrop  ] */
      1920      , 1080      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] */
    },
};

static int VTCALL_SIZE_LUT_RPB[][SIZE_OF_LUT] =
{
    /* Binning   = 2
       BNS ratio = 1.0
       BDS       = ON */

    /* 16:9 (VT_Call) */
    { SIZE_RATIO_16_9,
     (1936 + 0) ,(1104 + 0) ,   /* [sensor ] */
      1936      , 1104      ,   /* [bns    ] */
      1936      , 1104      ,   /* [bcrop  ] */
      1936      , 1090      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] */
    },
    /* 4:3 (VT_Call) */
    { SIZE_RATIO_4_3,
     (1936 + 0) ,(1104 + 0) ,   /* [sensor ] */
      1936      , 1104      ,   /* [bns    ] */
      1936      , 1104      ,   /* [bcrop  ] */
      1440      , 1080      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
    /* 1:1 (VT_Call) */
    { SIZE_RATIO_1_1,
     (1936 + 0) ,(1104 + 0) ,   /* [sensor ] */
      1936      , 1104      ,   /* [bns    ] */
      1936      , 1104      ,   /* [bcrop  ] */
      1080      , 1080      ,   /* [bds    ] */
      1080      , 1080      ,   /* [target ] */
    },
    /* 3:2 (VT_Call) */
    { SIZE_RATIO_3_2,
     (1936 + 0) ,(1104 + 0) ,   /* [sensor ] */
      1936      , 1104      ,   /* [bns    ] */
      1936      , 1104      ,   /* [bcrop  ] */
      1616      , 1080      ,   /* [bds    ] */
      1616      , 1080      ,   /* [target ] */
    },
    /* 11:9 (VT_Call) */
    { SIZE_RATIO_11_9,
     (1936 + 0) ,(1104 + 0) ,   /* [sensor ] */
      1936      , 1104      ,   /* [bns    ] */
      1936      , 1104      ,   /* [bcrop  ] */
      1232      , 1008      ,   /* [bds    ] */
      1232      , 1008      ,   /* [target ] */
    }
};

static int FAST_AE_STABLE_SIZE_LUT_RPB[][SIZE_OF_LUT] =
{
    /* Binning   = ON
       BNS ratio = 4.0 / FPS = 480
       BDS       = ON */

    /* FAST_AE 16:9 (Single) */
    { SIZE_RATIO_4_3,
     (1936 + 0) ,(1104 + 0) ,   /* [sensor ] */
      1936      , 1104      ,   /* [bns    ] */
      1936      , 1104      ,   /* [bcrop  ] */
      1936      , 1090      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] */
    },
};

static int PREVIEW_FULL_SIZE_LUT_RPB[][SIZE_OF_LUT] =
{
    /* Binning   = OFF
       BNS ratio = 1.0
       BDS       = 1080p */

    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
     (4656 + 0) ,(2656 + 0),   /* [sensor ] */
      4656      , 2656      ,   /* [bns    ] */
      4656      , 2620      ,   /* [bcrop  ] */
      1920      , 1080      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] */
    },
    /* 4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
     (4656 + 0) ,(3520 + 0),   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      4656      , 3492      ,   /* [bcrop  ] */
      1440      , 1080      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
    /* 1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
     (4656 + 0) ,(3520 + 0),   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      3488      , 3488      ,   /* [bcrop  ] */
      1080      , 1080      ,   /* [bds    ] */
      1080      , 1080      ,   /* [target ] */
    },
    /* 3:2 (Single, Dual) */
    { SIZE_RATIO_3_2,
     (4656 + 0) ,(3520 + 0),   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      4656      , 3104      ,   /* [bcrop  ] */
      1616      , 1080      ,   /* [bds    ] */
      1616      , 1080      ,   /* [target ] */
    },
    /* 5:4 (Single, Dual) */
    { SIZE_RATIO_5_4,
     (4656 + 0) ,(3520 + 0),   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      4366      , 3492      ,   /* [bcrop  ] */
      1344      , 1080      ,   /* [bds    ] */
      1344      , 1080      ,   /* [target ] */
    },
    /* 5:3 (Single, Dual) */
    { SIZE_RATIO_5_3,
     (4656 + 0) ,(3520 + 0),   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      4656      , 2794      ,   /* [bcrop  ] */
      1792      , 1080      ,   /* [bds    ] */
      1792      , 1080      ,   /* [target ] */
    },
    /* 11:9 (Single, Dual) */
    { SIZE_RATIO_11_9,
     (4656 + 0) ,(3520 + 0),   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      4268      , 3492      ,   /* [bcrop  ] */
      1312      , 1080      ,   /* [bds    ] */
      1312      , 1080      ,   /* [target ] */
    },
    /* Dummy (not used) */
    { SIZE_RATIO_9_16,
     (4656 + 0) ,(3520 + 0),   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      4268      , 3492      ,   /* [bcrop  ] */
      1312      , 1080      ,   /* [bds    ] */
      1312      , 1080      ,   /* [target ] */
    },
    /* 18.5:9 (Single, Dual) */
    { SIZE_RATIO_18P5_9,
     (4656 + 0) ,(3520 + 0),   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      4634      , 2254      ,   /* [bcrop  ] */
      2224      , 1080      ,   /* [bds    ] */
      2224      , 1080      ,   /* [target ] */
    }
};

static int PICTURE_FULL_SIZE_LUT_RPB[][SIZE_OF_LUT] =
{
    /* Binning   = OFF
       BNS ratio = 1.0
       BDS       = 1080p */

    /* 16:9 (Single) */
    { SIZE_RATIO_16_9,
     (4656 + 0) ,(3520 + 0) ,   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      4656      , 3492      ,   /* [bcrop  ] */
      4656      , 3492      ,   /* [bds    ] */
      4656      , 3492      ,   /* [target ] */
    },
    /* 4:3 (Single) */
    { SIZE_RATIO_4_3,
     (4656 + 0) ,(3520 + 0) ,   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      4656      , 3492      ,   /* [bcrop  ] */
      4656      , 3492      ,   /* [bds    ] */
      4656      , 3492      ,   /* [target ] */
    },
    /* 1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
     (4656 + 0) ,(3520 + 0) ,   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      4656      , 3492      ,   /* [bcrop  ] */
      4656      , 3492      ,   /* [bds    ] */
      4656      , 3492      ,   /* [target ] */
    },
    /* 3:2 (Single) */
    { SIZE_RATIO_3_2,
     (4656 + 0) ,(3520 + 0) ,   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      4656      , 3492      ,   /* [bcrop  ] */
      4656      , 3492      ,   /* [bds    ] */
      4656      , 3492      ,   /* [target ] */
    },
    /* 5:4 (Single) */
    { SIZE_RATIO_5_4,
     (4656 + 0) ,(3520 + 0) ,   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      4656      , 3492      ,   /* [bcrop  ] */
      4656      , 3492      ,   /* [bds    ] */
      4656      , 3492      ,   /* [target ] */
    },
    /* 5:3 (Single) */
    { SIZE_RATIO_5_3,
     (4656 + 0) ,(3520 + 0) ,   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      4656      , 3492      ,   /* [bcrop  ] */
      4656      , 3492      ,   /* [bds    ] */
      4656      , 3492      ,   /* [target ] */
    },
    /* 11:9 (Single) */
    { SIZE_RATIO_11_9,
     (4656 + 0) ,(3520 + 0) ,   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      4656      , 3492      ,   /* [bcrop  ] */
      4656      , 3492      ,   /* [bds    ] */
      4656      , 3492      ,   /* [target ] */
    },
    /* Dummy (not used) */
    { SIZE_RATIO_9_16,
     (4656 + 0) ,(3520 + 0) ,   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      4656      , 3492      ,   /* [bcrop  ] */
      4656      , 3492      ,   /* [bds    ] */
      4656      , 3492      ,   /* [target ] */
    },
    /* 18.5:9 (Single) */
    { SIZE_RATIO_18P5_9,
     (4656 + 0) ,(3520 + 0) ,   /* [sensor ] */
      4656      , 3520      ,   /* [bns    ] */
      4656      , 3492      ,   /* [bcrop  ] */
      4656      , 3492      ,   /* [bds    ] */
      4656      , 3492      ,   /* [target ] */
    }
};

/* yuv stream size list */
static int RPB_YUV_LIST[][SIZE_OF_RESOLUTION] =
{
    /* { width, height, minFrameDuration, ratioId } */
    { 4656, 3492, 33331760, SIZE_RATIO_4_3},
    { 4656, 2620, 33331760, SIZE_RATIO_16_9},
    { 3488, 3488, 33331760, SIZE_RATIO_1_1},
    { 3840, 2160, 16665880, SIZE_RATIO_16_9},
    { 2560, 1440, 16665880, SIZE_RATIO_16_9},
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
static int RPB_JPEG_LIST[][SIZE_OF_RESOLUTION] =
{
    /* { width, height, minFrameDuration, ratioId } */
    { 4656, 3492, 33331760, SIZE_RATIO_4_3},
    { 4656, 2620, 33331760, SIZE_RATIO_16_9},
    { 3488, 3488, 33331760, SIZE_RATIO_1_1},
    { 3840, 2160, 50000000, SIZE_RATIO_16_9},
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

static int RPB_THUMBNAIL_LIST[][SIZE_OF_RESOLUTION] =
{
    /* { width, height, minFrameDuration, ratioId } */
    {  512,  384, 0, SIZE_RATIO_4_3},
    {  512,  288, 0, SIZE_RATIO_16_9},
    {  384,  384, 0, SIZE_RATIO_1_1},
/* TODO : will be supported after enable S/W scaler correctly */
//  {  320,  240, 0, SIZE_RATIO_4_3},
    {    0,    0, 0, SIZE_RATIO_1_1}
};

/* For HAL3 */
static int RPB_HIGH_SPEED_VIDEO_LIST[][SIZE_OF_RESOLUTION] =
{
    { 1280,  720, 0, SIZE_RATIO_16_9},
    { 1920,  1080, 0, SIZE_RATIO_16_9},
};

static int RPB_FPS_RANGE_LIST[][2] =
{
    {  15000,  15000},
    {  24000,  24000},
    {  7000,  30000},
    {  10000,  30000},
    {  15000,  30000},
    {  30000,  30000},
//    {  60000,  60000},
};

/* For HAL3 */
static int RPB_HIGH_SPEED_VIDEO_FPS_RANGE_LIST[][2] =
{
    {  60000, 120000},
    { 120000, 120000},
    {  60000, 240000},
    { 240000, 240000},
    {  60000, 480000},
    { 480000, 480000},
};

/* vendor static info : width, height, min_fps, max_fps, vdis width, vdis height, recording limit time(sec) */
static int RPB_AVAILABLE_VIDEO_LIST[][7] =
{
#ifdef USE_UHD_RECORDING
    { 3840, 2160, 30000, 30000, 0, 0, 600},
#endif
#ifdef USE_WQHD_RECORDING
    { 2560, 1440, 30000, 30000, 0, 0, 0},
#endif
    { 1920, 1080, 60000, 60000, 0, 0, 600},
    { 1920, 1080, 30000, 30000, 0, 0, 0},
    { 1440, 1440, 30000, 30000, 0, 0, 0},
    { 1280,  720, 30000, 30000, 0, 0, 0},
    {  640,  480, 30000, 30000, 0, 0, 0},
};

/*  vendor static info :  width, height, min_fps, max_fps, recording limit time(sec) */
static int RPB_AVAILABLE_HIGH_SPEED_VIDEO_LIST[][5] =
{
    { 1280, 720, 480000, 480000, 0},
    { 1920, 1080, 480000, 480000, 0},
    { 1920, 1080, 240000, 240000, 0},
    { 1920, 1080, 120000, 120000, 0},
};

static camera_metadata_rational COLOR_MATRIX1_RPB_3X3[] =
{
    {778, 1024}, {-182, 1024}, {-117, 1024},
    {-579, 1024}, {1468, 1024}, {97, 1024},
    {-171, 1024}, {341, 1024}, {456, 1024}
};

static camera_metadata_rational COLOR_MATRIX2_RPB_3X3[] =
{
    {1400, 1024}, {-659, 1024}, {-182, 1024},
    {-496, 1024}, {1506, 1024}, {179, 1024},
    {-80, 1024}, {234, 1024}, {646, 1024}
};

static camera_metadata_rational FORWARD_MATRIX1_RPB_3X3[] =
{
    {664, 1024}, {152, 1024}, {171, 1024},
    {261, 1024}, {796, 1024}, {-32, 1024},
    {38, 1024}, {-405, 1024}, {1213, 1024}
};

static camera_metadata_rational FORWARD_MATRIX2_RPB_3X3[] =
{
    {599, 1024}, {176, 1024}, {213, 1024},
    {175, 1024}, {863, 1024}, {-14, 1024},
    {23, 1024}, {-681, 1024}, {1503, 1024}
};
#endif
