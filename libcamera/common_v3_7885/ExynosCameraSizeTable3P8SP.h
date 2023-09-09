/*
**
**copyright 2016, Samsung Electronics Co. LTD
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

#ifndef EXYNOS_CAMERA_LUT_S5K3P8SP_H
#define EXYNOS_CAMERA_LUT_S5K3P8SP_H

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
    Sensor Margin Height = 10
-----------------------------*/

static int PREVIEW_SIZE_LUT_S5K3P8SP[][SIZE_OF_LUT] =
{
    /* Binning   = 1
       BNS ratio = 1.0
       BDS       = OFF */

    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
     (4608 +  0),(3456 +  0),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      2304      , 1296      ,   /* [bcrop  ] */
      2304      , 1296      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] */
    },
    /*  4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
     (4608 +  0),(3456 +  0),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      2304      , 1728      ,   /* [bcrop  ] */
      2304      , 1728      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
    /*  1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
     (4608 +  0),(3456 +  0),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      1728      , 1728      ,   /* [bcrop  ] */
      1088      , 1088      ,   /* [bds    ] */
      1088      , 1088      ,   /* [target ] */
    },
    /*  3:2 (Single, Dual) */
    { SIZE_RATIO_3_2,
     (4608 + 0),(3456 + 0),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      2304      , 1536      ,   /* [bcrop  ] */
      2304      , 1536      ,   /* [bds    ] *//* w=1620, Reduced for 16 pixel align */
      1616      , 1080      ,   /* [target ] */
    },
    /*  5:4 (Single, Dual) */
    { SIZE_RATIO_5_4,
     (4608 + 0),(3456 + 0),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      2160      , 1728      ,   /* [bcrop  ] */
      1344      , 1080      ,   /* [bds    ] *//* w=1350, Reduced for 16 pixel align */
      1344      , 1080      ,   /* [target ] */
    },
    /*  5:3 (Single, Dual) */
    { SIZE_RATIO_5_3,
     (4608 + 0),(3456 + 0),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      2304      , 1382      ,   /* [bcrop  ] */
      1792      , 1080      ,   /* [bds    ] *//* w=1800, Reduced for 16 pixel align */
      1792      , 1080      ,   /* [target ] */
    },
    /*  11:9 (Single, Dual) */
    { SIZE_RATIO_11_9,
     (4608 + 0),(3456 + 0),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      2112      , 1728      ,   /* [bcrop  ] */
      2112      , 1728      ,   /* [bds    ] *//* w=1320, Reduced for 16 pixel align */
      1312      , 1080      ,   /* [target ] */
    }
};

static int PREVIEW_SIZE_LUT_S5K3P8SP_BNS[][SIZE_OF_LUT] =
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
    /*  4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2304      , 1728      ,   /* [bcrop  ] */
      2304      , 1728      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
    /*  1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      1728      , 1728      ,   /* [bcrop  ] */
      1088      , 1088      ,   /* [bds    ] */
      1088      , 1088      ,   /* [target ] */
    },

    /*  3:2 (Single, Dual) */
    { SIZE_RATIO_3_2,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2304      , 1536      ,   /* [bcrop  ] */
      2304      , 1536      ,   /* [bds    ] */
      1616      , 1080      ,   /* [target ] */
    },
    /*  5:4 (Single, Dual) */
    { SIZE_RATIO_5_4,
     (2304 + 0),(1728 + 0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2160      , 1728      ,   /* [bcrop  ] */
      1344      , 1080      ,   /* [bds    ] *//* w=1350, Reduced for 16 pixel align */
      1344      , 1080      ,   /* [target ] */
    },
    /*  5:3 (Single, Dual) */
    { SIZE_RATIO_5_3,
     (2304 + 0),(1728 + 0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2304      , 1382      ,   /* [bcrop  ] */
      1792      , 1080      ,   /* [bds    ] *//* w=1800, Reduced for 16 pixel align */
      1792      , 1080      ,   /* [target ] */
    },
    /*  11:9 (Single, Dual) */
    { SIZE_RATIO_11_9,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2112      , 1728      ,   /* [bcrop  ] */
      2112      , 1728      ,   /* [bds    ] */
      1312      , 1080      ,   /* [target ] */
    }
};

static int YUV_SIZE_LUT_S5K3P8SP_BNS[][SIZE_OF_LUT] =
{
    /* Binning   = 2
       BNS ratio = 1.0
       BDS       = OFF */

    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2304      , 1728      ,   /* [bcrop  ] */
      2304      , 1728      ,   /* [bds    ] */
      2304      , 1728      ,   /* [target ] */
    },
    /*  4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2304      , 1728      ,   /* [bcrop  ] */
      2304      , 1728      ,   /* [bds    ] */
      2304      , 1728      ,   /* [target ] */
    },
    /*  1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2304      , 1728      ,   /* [bcrop  ] */
      2304      , 1728      ,   /* [bds    ] */
      2304      , 1728      ,   /* [target ] */
    },

    /*  3:2 (Single, Dual) */
    { SIZE_RATIO_3_2,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2304      , 1728      ,   /* [bcrop  ] */
      2304      , 1728      ,   /* [bds    ] */
      2304      , 1728      ,   /* [target ] */
    },
    /*  5:4 (Single, Dual) */
    { SIZE_RATIO_5_4,
     (2304 + 0),(1728 + 0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2304      , 1728      ,   /* [bcrop  ] */
      2304      , 1728      ,   /* [bds    ] */
      2304      , 1728      ,   /* [target ] */
    },
    /*  5:3 (Single, Dual) */
    { SIZE_RATIO_5_3,
     (2304 + 0),(1728 + 0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2304      , 1728      ,   /* [bcrop  ] */
      2304      , 1728      ,   /* [bds    ] */
      2304      , 1728      ,   /* [target ] */
    },
    /*  11:9 (Single, Dual) */
    { SIZE_RATIO_11_9,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2304      , 1728      ,   /* [bcrop  ] */
      2304      , 1728      ,   /* [bds    ] */
      2304      , 1728      ,   /* [target ] */
    }
};

static int PICTURE_SIZE_LUT_S5K3P8SP[][SIZE_OF_LUT] =
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
    /*  4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
     (4608 +  0),(3456 +  0),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      4608      , 3456      ,   /* [bcrop  ] */
      4608      , 3456      ,   /* [bds    ] */
      4608      , 3456      ,   /* [target ] */
    },
    /*  1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
     (4608 +  0),(3456 +  0),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      3456      , 3456      ,   /* [bcrop  ] */
      3456      , 3456      ,   /* [bds    ] */
      3456      , 3456      ,   /* [target ] */
    },
    /*  3:2 (Single, Dual) */
    { SIZE_RATIO_3_2,
     (4608 + 0),(3456 + 0),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      4608      , 3072      ,   /* [bcrop  ] */
      4608      , 3072      ,   /* [bds    ] */
      4608      , 3072      ,   /* [target ] */
    },
    /*  5:4 (Single, Dual) */
    { SIZE_RATIO_5_4,
     (4608 + 0),(3456 + 0),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      4320      , 3456      ,   /* [bcrop  ] */
      4320      , 3456      ,   /* [bds    ] *//* w=3060, Reduced for 16 pixel align */
      4320      , 3456      ,   /* [target ] */
    },
    /*  5:3 (Single, Dual) */
    { SIZE_RATIO_5_3,
     (4608 + 0),(3456 + 0),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      4608      , 2764      ,   /* [bcrop  ] */
      4608      , 2764      ,   /* [bds    ] */
      4608      , 2764      ,   /* [target ] */
    },
    /*  11:9 (Single, Dual) */
    { SIZE_RATIO_11_9,
     (4608 + 0),(3456 + 0),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      4224      , 3456      ,   /* [bcrop  ] */
      4224      , 3456      ,   /* [bds    ] *//* w=1320, Reduced for 16 pixel align */
      4224      , 3456      ,   /* [target ] */
    }
};

static int PICTURE_SIZE_LUT_S5K3P8SP_BNS[][SIZE_OF_LUT] =
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
      4608      , 2592      ,   /* [target ] */
    },
    /*  4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2304      , 1728      ,   /* [bcrop  ] */
      2304      , 1728      ,   /* [bds    ] */
      4608      , 3456      ,   /* [target ] */
    },
    /*  1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      1728      , 1728      ,   /* [bcrop  ] */
      1728      , 1728      ,   /* [bds    ] */
      3456      , 3456      ,   /* [target ] */
    },

    /*  3:2 (Single, Dual) */
    { SIZE_RATIO_3_2,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2304      , 1536      ,   /* [bcrop  ] */
      2304      , 1536      ,   /* [bds    ] */
      4608      , 3072      ,   /* [target ] */
    },
    /*  5:4 (Single, Dual) */
    { SIZE_RATIO_5_4,
     (2304 + 0),(1728 + 0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2160      , 1728      ,   /* [bcrop  ] */
      2160      , 1728      ,   /* [bds    ] *//* w=1350, Reduced for 16 pixel align */
      4320      , 3456      ,   /* [target ] */
    },
    /*  5:3 (Single, Dual) */
    { SIZE_RATIO_5_3,
     (2304 + 0),(1728 + 0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2304      , 1382      ,   /* [bcrop  ] */
      2304      , 1382      ,   /* [bds    ] *//* w=1800, Reduced for 16 pixel align */
      4608      , 2764      ,   /* [target ] */
    },
    /*  11:9 (Single, Dual) */
    { SIZE_RATIO_11_9,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2112      , 1728      ,   /* [bcrop  ] */
      2112      , 1728      ,   /* [bds    ] */
      4224      , 3456      ,   /* [target ] */
    }
};

static int VIDEO_SIZE_LUT_S5K3P8SP[][SIZE_OF_LUT] =
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
    /*  4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
     (4608 +  0),(3456 +  0),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      4608      , 3456      ,   /* [bcrop  ] */
      4608      , 3456      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
    /*  1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
     (4608 +  0),(3456 +  0),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      3456      , 3456      ,   /* [bcrop  ] */
      3456      , 3456      ,   /* [bds    ] */
      1088      , 1088      ,   /* [target ] */
    },
    /*  3:2 (Single, Dual) */
    { SIZE_RATIO_3_2,
     (4608 + 0),(3456 + 0),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      4608      , 3072      ,   /* [bcrop  ] */
      1616      , 1080      ,   /* [bds    ] *//* w=1620, Reduced for 16 pixel align */
      1616      , 1080      ,   /* [target ] */
    },
    /*  5:4 (Single, Dual) */
    { SIZE_RATIO_5_4,
     (4608 + 0),(3456 + 0),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      4320      , 3456      ,   /* [bcrop  ] */
      1344      , 1080      ,   /* [bds    ] *//* w=1350, Reduced for 16 pixel align */
      1344      , 1080      ,   /* [target ] */
    },
    /*  5:3 (Single, Dual) */
    { SIZE_RATIO_5_3,
     (4608 + 0),(3456 + 0),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      4608      , 2764      ,   /* [bcrop  ] */
      1792      , 1080      ,   /* [bds    ] *//* w=1800, Reduced for 16 pixel align */
      1792      , 1080      ,   /* [target ] */
    },
    /*  11:9 (Single, Dual) */
    { SIZE_RATIO_11_9,
     (4608 + 0),(3456 + 0),   /* [sensor ] */
      4608      , 3456      ,   /* [bns    ] */
      4224      , 3456      ,   /* [bcrop  ] */
      1312      , 1080      ,   /* [bds    ] *//* w=1320, Reduced for 16 pixel align */
      1312      , 1080      ,   /* [target ] */
    }
};

static int VIDEO_SIZE_LUT_S5K3P8SP_BNS[][SIZE_OF_LUT] =
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
    /*  4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2304      , 1728      ,   /* [bcrop  ] */
      2304      , 1728      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
    /*  1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      1728      , 1728      ,   /* [bcrop  ] */
      1728      , 1728      ,   /* [bds    ] */
      1088      , 1088      ,   /* [target ] */
    },
    /*  3:2 (Single, Dual) */
    { SIZE_RATIO_3_2,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2304      , 1536      ,   /* [bcrop  ] */
      2304      , 1536      ,   /* [bds    ] */
      1616      , 1080      ,   /* [target ] */
    },
    /*  5:4 (Single, Dual) */
    { SIZE_RATIO_5_4,
     (2304 + 0),(1728 + 0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2160      , 1728      ,   /* [bcrop  ] */
      1344      , 1080      ,   /* [bds    ] *//* w=1350, Reduced for 16 pixel align */
      1344      , 1080      ,   /* [target ] */
    },
    /*  5:3 (Single, Dual) */
    { SIZE_RATIO_5_3,
     (2304 + 0),(1728 + 0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2304      , 1382      ,   /* [bcrop  ] */
      1792      , 1080      ,   /* [bds    ] *//* w=1800, Reduced for 16 pixel align */
      1792      , 1080      ,   /* [target ] */
    },
    /*  11:9 (Single, Dual) */
    { SIZE_RATIO_11_9,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2112      , 1728      ,   /* [bcrop  ] */
      2112      , 1728      ,   /* [bds    ] */
      1312      , 1080      ,   /* [target ] */
    }
};

static int VIDEO_SIZE_LUT_120FPS_HIGH_SPEED_S5K3P8SP_BNS[][SIZE_OF_LUT] =
{
    /* Binning   = 2
       BNS ratio = 1.0
       BDS       = OFF */

    /*   HD_120  16:9 (Single) */
    { SIZE_RATIO_16_9,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2304      , 1296      ,   /* [bcrop  ] */
      2304      , 1296      ,   /* [bds    ] */
      1280      ,  720      ,   /* [target ] */
    },
};

static int FAST_AE_STABLE_SIZE_LUT_S5K3P8SP[][SIZE_OF_LUT] =
{
    /* Binning   = 4
       BNS ratio = 1.0 / FPS = 120
       BDS       = OFF */

    /* FAST_AE 16:9 (Single) */
    { SIZE_RATIO_16_9,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2304      , 1296      ,   /* [bcrop  ] */
      2304      , 1296      ,   /* [bds    ] */
      1280      ,  720      ,   /* [target ] */
    },
};

static int VTCALL_SIZE_LUT_S5K3P8SP[][SIZE_OF_LUT] =
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
    /*  4:3 (VT_Call) */
    { SIZE_RATIO_4_3,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2304      , 1728      ,   /* [bcrop  ] */
      2304      , 1728      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
    /*  1:1 (VT_Call) */
    { SIZE_RATIO_1_1,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      1728      , 1728      ,   /* [bcrop  ] */
      1728      , 1728      ,   /* [bds    ] */
      1088      , 1088      ,   /* [target ] */
    },
    /*  11:9 (VT_Call) */
    { SIZE_RATIO_11_9,
     (2304 +  0),(1728 +  0),   /* [sensor ] */
      2304      , 1728      ,   /* [bns    ] */
      2112      , 1728      ,   /* [bcrop  ] */
      2112      , 1728      ,   /* [bds    ] */
      1312      , 1080      ,   /* [target ] */
    }
};

static int S5K3P8SP_PREVIEW_LIST[][SIZE_OF_RESOLUTION] =
{
#if defined(CAMERA_LCD_SIZE) && (CAMERA_LCD_SIZE >= LCD_SIZE_2560_1440)
    { 2560, 1440, SIZE_RATIO_16_9},
    { 1920, 1440, SIZE_RATIO_4_3},
    { 1440, 1440, SIZE_RATIO_1_1},
    { 1088, 1088, SIZE_RATIO_1_1},
#endif
    { 1920, 1080, SIZE_RATIO_16_9},
    { 1440, 1080, SIZE_RATIO_4_3},
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

static int S5K3P8SP_HIDDEN_PREVIEW_LIST[][SIZE_OF_RESOLUTION] =
{
#if !(defined(CAMERA_LCD_SIZE) && (CAMERA_LCD_SIZE >= LCD_SIZE_1920_1080))
    { 1920, 1080, SIZE_RATIO_16_9},
    { 1440, 1080, SIZE_RATIO_4_3},
    { 1072, 1072, SIZE_RATIO_1_1},
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

static int S5K3P8SP_PICTURE_LIST[][SIZE_OF_RESOLUTION] =
{
    { 4608, 3456, SIZE_RATIO_4_3},
    { 4608, 2592, SIZE_RATIO_16_9},
    { 3456, 3456, SIZE_RATIO_1_1},
    { 3264, 2448, SIZE_RATIO_4_3},
    { 3264, 1836, SIZE_RATIO_16_9},
    { 2448, 2448, SIZE_RATIO_1_1},
    { 2048, 1152, SIZE_RATIO_16_9},
    { 2304, 1728, SIZE_RATIO_4_3},
    { 2304, 1296, SIZE_RATIO_16_9},
    { 1728, 1728, SIZE_RATIO_1_1},
    { 1920, 1080, SIZE_RATIO_16_9},
    { 1280,  720, SIZE_RATIO_16_9},
    {  960,  720, SIZE_RATIO_4_3},
    {  640,  480, SIZE_RATIO_4_3},
    {  320,  240, SIZE_RATIO_4_3}, /* for cts2 testAvailableStreamConfigs */
    {  256,  144, SIZE_RATIO_16_9}, /*  for cts2 testSingleImageThumbnail */
};

static int S5K3P8SP_HIDDEN_PICTURE_LIST[][SIZE_OF_RESOLUTION] =
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

static int S5K3P8SP_THUMBNAIL_LIST[][SIZE_OF_RESOLUTION] =
{
    {  512,  384, SIZE_RATIO_4_3},
    {  512,  288, SIZE_RATIO_16_9},
    {  384,  384, SIZE_RATIO_1_1},
/* TODO : will be supported after enable S/W scaler correctly */
//  {  320,  240, SIZE_RATIO_4_3},
    {    0,    0, SIZE_RATIO_1_1}
};

static int S5K3P8SP_VIDEO_LIST[][SIZE_OF_RESOLUTION] =
{
    { 1920, 1080, SIZE_RATIO_16_9},
    { 1440, 1080, SIZE_RATIO_4_3},
    { 1072, 1072, SIZE_RATIO_1_1},
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

static int S5K3P8SP_HIDDEN_VIDEO_LIST[][SIZE_OF_RESOLUTION] =
{
    {  960,  960, SIZE_RATIO_1_1},  /* for Clip Movie */
    {  864,  480, SIZE_RATIO_16_9}, /* for PLB mode */
    {  432,  240, SIZE_RATIO_16_9}, /* for PLB mode */
};

static int S5K3P8SP_YUV_LIST[][SIZE_OF_RESOLUTION] =
{
    { 2304, 1728, SIZE_RATIO_4_3},
    { 2304, 1296, SIZE_RATIO_16_9},
    { 1920, 1080, SIZE_RATIO_16_9},
    { 1440, 1080, SIZE_RATIO_4_3},
    { 1280,  720, SIZE_RATIO_16_9},
    { 1088, 1088, SIZE_RATIO_1_1},
    { 1072, 1072, SIZE_RATIO_1_1},
    { 1056,  704, SIZE_RATIO_3_2},
    { 1024,  768, SIZE_RATIO_4_3},
    {  960,  720, SIZE_RATIO_4_3},
    {  800,  450, SIZE_RATIO_16_9},
    {  720,  720, SIZE_RATIO_1_1},
    {  720,  480, SIZE_RATIO_3_2},
    {  640,  480, SIZE_RATIO_4_3},
    {  480,  320, SIZE_RATIO_3_2},
    {  352,  288, SIZE_RATIO_11_9},
    {  320,  240, SIZE_RATIO_4_3},
    {  256,  144, SIZE_RATIO_16_9}, /* DngCreatorTest */
    {  176,  144, SIZE_RATIO_11_9}, /* RecordingTest */
};

static int S5K3P8SP_HIGH_SPEED_VIDEO_LIST[][SIZE_OF_RESOLUTION] =
{
    { 1280,  720, SIZE_RATIO_16_9},
};

static int S5K3P8SP_HIGH_SPEED_VIDEO_FPS_RANGE_LIST[][2] =
{
    {  30000, 120000},
    { 120000, 120000},
};

static int S5K3P8SP_FPS_RANGE_LIST[][2] =
{
//    {   5000,   5000},
//    {   7000,   7000},
    {  15000,  15000},
    {  24000,  24000},
//    {   4000,  30000},
    {   8000,  30000},
    {  10000,  30000},
    {  15000,  30000},
    {  30000,  30000},
};

static int S5K3P8SP_HIDDEN_FPS_RANGE_LIST[][2] =
{
    {  10000,  24000},
    {  30000,  60000},
    {  60000,  60000},
    {  60000, 120000},
    { 120000, 120000},
};

static camera_metadata_rational UNIT_MATRIX_S5K3P8SP_3X3[] =
{
    {128, 128}, {0, 128}, {0, 128},
    {0, 128}, {128, 128}, {0, 128},
    {0, 128}, {0, 128}, {128, 128}
};

static camera_metadata_rational COLOR_MATRIX1_S5K3P8SP_3X3[] = {
    {1094, 1024}, {-306, 1024}, {-146, 1024},
    {-442, 1024}, {1388, 1024}, {52, 1024},
    {-104, 1024}, {250, 1024}, {600, 1024}
};

static camera_metadata_rational COLOR_MATRIX2_S5K3P8SP_3X3[] = {
    {2263, 1024}, {-1364, 1024}, {-145, 1024},
    {-194, 1024}, {1257, 1024}, {-56, 1024},
    {-24, 1024}, {187, 1024}, {618, 1024}
};

static camera_metadata_rational FORWARD_MATRIX1_S5K3P8SP_3X3[] = {
    {612, 1024}, {233, 1024}, {139, 1024},
    {199, 1024}, {831, 1024}, {-6, 1024},
    {15, 1024}, {-224, 1024}, {1049, 1024}
};

static camera_metadata_rational FORWARD_MATRIX2_S5K3P8SP_3X3[] = {
    {441, 1024}, {317, 1024}, {226, 1024},
    {29, 1024}, {908, 1024}, {87, 1024},
    {9, 1024}, {-655, 1024}, {1486, 1024}
};

#endif
