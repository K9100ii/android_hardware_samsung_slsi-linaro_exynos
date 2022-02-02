/*
**
** Copyright 2016, Samsung Electronics Co. LTD
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

#ifndef EXYNOS_CAMERA_LUT_IMX320_3H1_H
#define EXYNOS_CAMERA_LUT_IMX320_3H1_H

/* -------------------------
    SIZE_RATIO_16_9 = 0,
    SIZE_RATIO_4_3,
    SIZE_RATIO_1_1,
    SIZE_RATIO_3_2,
    SIZE_RATIO_5_4,
    SIZE_RATIO_5_3,
    SIZE_RATIO_11_9,
    SIZE_RATIO_9_16,
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

static int PREVIEW_SIZE_LUT_IMX320_3H1[][SIZE_OF_LUT] =
{
    /* Binning   = OFF
       BNS ratio = 1.0
       BDS       = 1080p */

    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
      3264      , 1836      ,   /* [sensor ] */
      3264      , 1836      ,   /* [bns    ] */
      3264      , 1836      ,   /* [bcrop  ] */
      2176      , 1224      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] */
    },
    /*  4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
      3264      , 2448      ,   /* [sensor ] */
      3264      , 2448      ,   /* [bns    ] */
      3264      , 2448      ,   /* [bcrop  ] */
      2176      , 1632      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
    /*  1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
      2448      , 2448      ,   /* [sensor ] */
      2448      , 2448      ,   /* [bns    ] */
      2448      , 2448      ,   /* [bcrop  ] */
      1632      , 1632      ,   /* [bds    ] *//* w=1080, Increased for 16 pixel align */
      1080      , 1080      ,   /* [target ] */
    },
    /* 3:2 (Single) */
    { SIZE_RATIO_3_2,
      3264      , 2448      ,   /* [sensor ] */
      3264      , 2448      ,   /* [bns    ] */
      3264      , 2176      ,   /* [bcrop  ] */
      2448      , 1632      ,   /* [bds    ] */
      1616      , 1080      ,   /* [target ] *//* w=1620, Reduced for 16 pixel align */
    },
    /* 5:4 (Single) */
    { SIZE_RATIO_5_4,
      3264      , 2448      ,   /* [sensor ] */
      3264      , 2448      ,   /* [bns    ] */
      3040      , 2432      ,   /* [bcrop  ] */
      2240      , 1792      ,   /* [bds	 ] */
      1344      , 1080      ,   /* [target ] *//* w=1350, Reduced for 16 pixel align */
    },
    /* 5:3 (Single) */
    { SIZE_RATIO_5_3,
      3264      , 2448      ,   /* [sensor ] */
      3264      , 2448      ,   /* [bns    ] */
      3240      , 1944      ,   /* [bcrop  ] */
      2560      , 1536      ,   /* [bds	 ] */
      1792      , 1080      ,   /* [target ] *//* w=1800, Reduced for 16 pixel align */
    },
    /* 11:9 (Single) */
    { SIZE_RATIO_11_9,
      3264      , 2448      ,   /* [sensor ] */
      3264      , 2448      ,   /* [bns    ] */
      2992      , 2448      ,   /* [bcrop  ] */
      2112      , 1728      ,   /* [bds	 ] */
      1312      , 1080      ,   /* [target ] *//* w=1320, Reduced for 16 pixel align */
    },
    /*  9:16 (for COLOR_IRIS) */
    { SIZE_RATIO_9_16,
       816      , 1456      ,   /* [sensor ] */
       816      , 1456      ,   /* [bns    ] */
       816      , 1456      ,   /* [bcrop  ] */
       816      , 1456      ,   /* [bds    ] *//* w=752, Increased for 16 pixel align */
       816      , 1456      ,   /* [target ] */
    }
};

static int DUAL_PREVIEW_SIZE_LUT_IMX320_3H1[][SIZE_OF_LUT] =
{
    /* Binning   = OFF
       BNS ratio = 1.0
       BDS       = 1080p */

    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
      3264      , 1836      ,   /* [sensor ] */
      3264      , 1836      ,   /* [bns    ] */
      3264      , 1836      ,   /* [bcrop  ] */
      3264      , 1836      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] */
    },
    /*  4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
      3264      , 2448      ,   /* [sensor ] */
      3264      , 2448      ,   /* [bns    ] */
      3264      , 2448      ,   /* [bcrop  ] */
      3264      , 2448      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
    /*  1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
      2448      , 2448      ,   /* [sensor ] */
      2448      , 2448      ,   /* [bns    ] */
      2448      , 2448      ,   /* [bcrop  ] */
      2448      , 2448      ,   /* [bds    ] *//* w=1080, Increased for 16 pixel align */
      1080      , 1080      ,   /* [target ] */
    },
    /* 3:2 (Single) */
    { SIZE_RATIO_3_2,
      3264      , 2448      ,   /* [sensor ] */
      3264      , 2448      ,   /* [bns    ] */
      3264      , 2176      ,   /* [bcrop  ] */
      2448      , 1632      ,   /* [bds    ] */
      1616      , 1080      ,   /* [target ] *//* w=1620, Reduced for 16 pixel align */
    },
    /* 5:4 (Single) */
    { SIZE_RATIO_5_4,
      3264      , 2448      ,   /* [sensor ] */
      3264      , 2448      ,   /* [bns    ] */
      3040      , 2432      ,   /* [bcrop  ] */
      2240      , 1792      ,   /* [bds    ] */
      1344      , 1080      ,   /* [target ] *//* w=1350, Reduced for 16 pixel align */
    },
    /* 5:3 (Single) */
    { SIZE_RATIO_5_3,
      3264      , 2448      ,   /* [sensor ] */
      3264      , 2448      ,   /* [bns    ] */
      3240      , 1944      ,   /* [bcrop  ] */
      2560      , 1536      ,   /* [bds    ] */
      1792      , 1080      ,   /* [target ] *//* w=1800, Reduced for 16 pixel align */
    },
    /* 11:9 (Single) */
    { SIZE_RATIO_11_9,
      3264      , 2448      ,   /* [sensor ] */
      3264      , 2448      ,   /* [bns    ] */
      2992      , 2448      ,   /* [bcrop  ] */
      2112      , 1728      ,   /* [bds    ] */
      1312      , 1080      ,   /* [target ] *//* w=1320, Reduced for 16 pixel align */
    }
};

static int PICTURE_SIZE_LUT_IMX320_3H1[][SIZE_OF_LUT] =
{
    { SIZE_RATIO_16_9,
      3264      , 1836      ,   /* [sensor ] */
      3264      , 1836      ,   /* [bns    ] */
      3264      , 1836      ,   /* [bcrop  ] */
      3264      , 1836      ,   /* [bds    ] */
      3264      , 1836      ,   /* [target ] */
    },
    /*  4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
      3264      , 2448      ,   /* [sensor ] */
      3264      , 2448      ,   /* [bns    ] */
      3264      , 2448      ,   /* [bcrop  ] */
      3264      , 2448      ,   /* [bds    ] */
      3264      , 2448      ,   /* [target ] */
    },
    /*  1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
      2448      , 2448      ,   /* [sensor ] */
      2448      , 2448      ,   /* [bns    ] */
      2448      , 2448      ,   /* [bcrop  ] */
      2448      , 2448      ,   /* [bds    ] *//* w=1080, Increased for 16 pixel align */
      2448      , 2448      ,   /* [target ] */
    },
    /* 3:2 (Single) */
    { SIZE_RATIO_3_2,
      3264      , 2448      ,   /* [sensor ] */
      3264      , 2448      ,   /* [bns    ] */
      3264      , 2176      ,   /* [bcrop  ] */
      3264      , 2176      ,   /* [bds    ] */
      3264      , 2176      ,   /* [target ] *//* w=1620, Reduced for 16 pixel align */
    },
    /* 5:4 (Single) */
    { SIZE_RATIO_5_4,
      3264      , 2448      ,   /* [sensor ] */
      3264      , 2448      ,   /* [bns    ] */
      3040      , 2432      ,   /* [bcrop  ] */
      3040      , 2432      ,   /* [bds    ] */
      3040      , 2432      ,   /* [target ] *//* w=1350, Reduced for 16 pixel align */
    },
    /* 5:3 (Single) */
    { SIZE_RATIO_5_3,
      3264      , 2448      ,   /* [sensor ] */
      3264      , 2448      ,   /* [bns    ] */
      3240      , 1944      ,   /* [bcrop  ] */
      3240      , 1944      ,   /* [bds    ] */
      3240      , 1944      ,   /* [target ] *//* w=1800, Reduced for 16 pixel align */
    },
    /* 11:9 (Single) */
    { SIZE_RATIO_11_9,
      3264      , 2448      ,   /* [sensor ] */
      3264      , 2448      ,   /* [bns    ] */
      2992      , 2448      ,   /* [bcrop  ] */
      2992      , 2448      ,   /* [bds    ] */
      2992      , 2448      ,   /* [target ] *//* w=1320, Reduced for 16 pixel align */
    }
};

static int VIDEO_SIZE_LUT_IMX320_3H1[][SIZE_OF_LUT] =
{
    { SIZE_RATIO_16_9,
      3264      , 1836      ,   /* [sensor ] */
      3264      , 1836      ,   /* [bns    ] */
      3264      , 1836      ,   /* [bcrop  ] */
      2176      , 1224      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] */
    },
    /*  4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
      3264      , 2448      ,   /* [sensor ] */
      3264      , 2448      ,   /* [bns    ] */
      3264      , 2448      ,   /* [bcrop  ] */
      1440      , 1080      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
    /*  1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
      2448      , 2448      ,   /* [sensor ] */
      2448      , 2448      ,   /* [bns    ] */
      2448      , 2448      ,   /* [bcrop  ] */
      1632      , 1632      ,   /* [bds    ] *//* w=1080, Increased for 16 pixel align */
      1080      , 1080      ,   /* [target ] */
    },
    /* 3:2 (Single) */
    { SIZE_RATIO_3_2,
      3264      , 2448      ,   /* [sensor ] */
      3264      , 2448      ,   /* [bns    ] */
      3264      , 2176      ,   /* [bcrop  ] */
      2448      , 1632      ,   /* [bds    ] */
      1616      , 1080      ,   /* [target ] *//* w=1620, Reduced for 16 pixel align */
    },
    /* 5:4 (Single) */
    { SIZE_RATIO_5_4,
      3264      , 2448      ,   /* [sensor ] */
      3264      , 2448      ,   /* [bns    ] */
      3040      , 2432      ,   /* [bcrop  ] */
      2240      , 1792      ,   /* [bds    ] */
      1344      , 1080      ,   /* [target ] *//* w=1350, Reduced for 16 pixel align */
    },
    /* 5:3 (Single) */
    { SIZE_RATIO_5_3,
      3264      , 2448      ,   /* [sensor ] */
      3264      , 2448      ,   /* [bns    ] */
      3240      , 1944      ,   /* [bcrop  ] */
      2560      , 1536      ,   /* [bds    ] */
      1792      , 1080      ,   /* [target ] *//* w=1800, Reduced for 16 pixel align */
    },
    /* 11:9 (Single) */
    { SIZE_RATIO_11_9,
      3264      , 2448      ,   /* [sensor ] */
      3264      , 2448      ,   /* [bns    ] */
      2992      , 2448      ,   /* [bcrop  ] */
      2112      , 1728      ,   /* [bds    ] */
      1312      , 1080      ,   /* [target ] *//* w=1320, Reduced for 16 pixel align */
    }
};

static int VIDEO_SIZE_LUT_60FPS_HIGH_SPEED_IMX320_3H1[][SIZE_OF_LUT] =
{
    /* Binning   = ON
       BNS ratio = 1.0
       BDS       = ON */

    /*   HD_60  16:9 (Single) */
    { SIZE_RATIO_16_9,
      1632      ,  918      ,   /* [sensor ] *//* Sensor binning ratio = 2 */
      1632      ,  918      ,   /* [bns    ] */
      1632      ,  918      ,   /* [bcrop  ] */
      1632      ,  918      ,   /* [bds    ] */
      1280      ,  720      ,   /* [target ] */
    }
};

static int VIDEO_SIZE_LUT_120FPS_HIGH_SPEED_IMX320_3H1[][SIZE_OF_LUT] =
{
    /* Binning   = ON
       BNS ratio = 1.0
       BDS       = ON */

    /*  HD_120  4:3 (Single) */
    { SIZE_RATIO_4_3,
       800      ,  600      ,   /* [sensor ] *//* Sensor binning ratio = 4 */
       800      ,  600      ,   /* [bns    ] */
       800      ,  600      ,   /* [bcrop  ] */
       800      ,  600      ,   /* [bds    ] */
       800      ,  600      ,   /* [target ] */
    },
    /*  HD_110  9:16 (Single) */
    { SIZE_RATIO_9_16,
       408      ,  728      ,   /* [sensor ] */
       408      ,  728      ,   /* [bns    ] */
       408      ,  728      ,   /* [bcrop  ] */
       408      ,  728      ,   /* [bds    ] */
       408      ,  728      ,   /* [target ] */
    }
};

static int PREVIEW_FULL_SIZE_LUT_IMX320_3H1[][SIZE_OF_LUT] =
{
    { SIZE_RATIO_16_9,
      3264      , 2448      ,   /* [sensor ] */
      3264      , 2448      ,   /* [bns	 ] */
      3264      , 2448      ,   /* [bcrop  ] */
      2176      , 1632      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
    /*  4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
      3264      , 2448      ,   /* [sensor ] */
      3264      , 2448      ,   /* [bns	 ] */
      3264      , 2448      ,   /* [bcrop  ] */
      2176      , 1632      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
    /*  1:1 (Single) */
    { SIZE_RATIO_1_1,
      3264      , 2448      ,   /* [sensor ] */
      3264      , 2448      ,   /* [bns	 ] */
      3264      , 2448      ,   /* [bcrop  ] */
      2176      , 1632      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
    /* 3:2 (Single, Dual) */
    { SIZE_RATIO_3_2,
      3264      , 2448      ,   /* [sensor ] */
      3264      , 2448      ,   /* [bns	 ] */
      3264      , 2448      ,   /* [bcrop  ] */
      2176      , 1632      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
    /* 5:4 (Single, Dual) */
    { SIZE_RATIO_5_4,
      3264      , 2448      ,   /* [sensor ] */
      3264      , 2448      ,   /* [bns	 ] */
      3264      , 2448      ,   /* [bcrop  ] */
      2176      , 1632      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
    /* 5:3 (Single, Dual) */
    { SIZE_RATIO_5_3,
      3264      , 2448      ,	/* [sensor ] */
      3264      , 2448      ,	/* [bns    ] */
      3264      , 2448      ,   /* [bcrop  ] */
      2176      , 1632      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
    /* 11:9 (Single, Dual) */
    { SIZE_RATIO_11_9,
      3264      , 2448      ,   /* [sensor ] */
      3264      , 2448      ,   /* [bns	 ] */
      3264      , 2448      ,   /* [bcrop  ] */
      2176      , 1632      ,   /* [bds    ] */
      1440      , 1080      ,   /* [target ] */
    },
    /*	9:16 (for COLOR_IRIS) 816 1456 */
    { SIZE_RATIO_9_16,
       816      , 1456      ,   /* [sensor ] */
       816      , 1456      ,   /* [bns    ] */
       816      , 1456      ,   /* [bcrop  ] */
       816      , 1456      ,   /* [bds    ] *//* w=752, Increased for 16 pixel align */
       816      , 1456      ,   /* [target ] */
    },
};

static int PICTURE_FULL_SIZE_LUT_IMX320_3H1[][SIZE_OF_LUT] =
{
    { SIZE_RATIO_16_9,
      3264      , 2448      ,   /* [sensor ] */
      3264      , 2448      ,   /* [bns	 ] */
      3264      , 2448      ,   /* [bcrop  ] */
      3264      , 2448      ,   /* [bds    ] */
      3264      , 2448      ,   /* [target ] */
    },
    /*  4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
      3264      , 2448      ,   /* [sensor ] */
      3264      , 2448      ,   /* [bns	 ] */
      3264      , 2448      ,   /* [bcrop  ] */
      3264      , 2448      ,   /* [bds    ] */
      3264      , 2448      ,   /* [target ] */
    },
    /*  1:1 (Single) */
    { SIZE_RATIO_1_1,
      3264      , 2448      ,   /* [sensor ] */
      3264      , 2448      ,   /* [bns	 ] */
      3264      , 2448      ,   /* [bcrop  ] */
      3264      , 2448      ,   /* [bds    ] */
      3264      , 2448      ,   /* [target ] */
    },
    /* 3:2 (Single, Dual) */
    { SIZE_RATIO_3_2,
      3264      , 2448      ,   /* [sensor ] */
      3264      , 2448      ,   /* [bns	 ] */
      3264      , 2448      ,   /* [bcrop  ] */
      3264      , 2448      ,   /* [bds    ] */
      3264      , 2448      ,   /* [target ] */
    },
    /* 5:4 (Single, Dual) */
    { SIZE_RATIO_5_4,
      3264      , 2448      ,   /* [sensor ] */
      3264      , 2448      ,   /* [bns	 ] */
      3264      , 2448      ,   /* [bcrop  ] */
      3264      , 2448      ,   /* [bds    ] */
      3264      , 2448      ,   /* [target ] */
    },
    /* 5:3 (Single, Dual) */
    { SIZE_RATIO_5_3,
      3264      , 2448      ,	/* [sensor ] */
      3264      , 2448      ,	/* [bns    ] */
      3264      , 2448      ,   /* [bcrop  ] */
      3264      , 2448      ,   /* [bds    ] */
      3264      , 2448      ,   /* [target ] */
    },
    /* 11:9 (Single, Dual) */
    { SIZE_RATIO_11_9,
      3264      , 2448      ,   /* [sensor ] */
      3264      , 2448      ,   /* [bns	 ] */
      3264      , 2448      ,   /* [bcrop  ] */
      3264      , 2448      ,   /* [bds    ] */
      3264      , 2448      ,   /* [target ] */
    }
};

static int VTCALL_SIZE_LUT_IMX320_3H1[][SIZE_OF_LUT] =
{
    /* Binning   = 2
       BNS ratio = 1.0
       BDS       = ON */

    /* 16:9 (VT_Call) */
    { SIZE_RATIO_16_9,
      1632      ,  918     ,   /* [sensor ] */
      1632      ,  918     ,   /* [bns    ] */
      1632      ,  918     ,   /* [bcrop  ] */
      1280      ,  720     ,   /* [bds    ] */
      1280      ,  720     ,   /* [target ] */
    },
    /* 4:3 (VT_Call) */
    /* Bcrop size 1152*864 -> 1280*960, for flicker algorithm */
    { SIZE_RATIO_4_3,
     1632      ,  1224     ,   /* [sensor ] */
     1632      ,  1224     ,   /* [bns    ] */
     1632      ,  1224     ,   /* [bcrop  ] */
     1280      ,  960      ,   /* [bds    ] */
      640      ,  480      ,   /* [target ] */
    },
    /* 1:1 (VT_Call) */
    { SIZE_RATIO_1_1,
     1224      ,  1224     ,   /* [sensor ] */
     1224      ,  1224     ,   /* [bns    ] */
     1224      ,  1224     ,   /* [bcrop  ] */
     1224      ,  1224     ,   /* [bds    ] */
     1080      ,  1080     ,   /* [target ] */
    },
    /* 11:9 (VT_Call) */
    /* Bcrop size 1056*864 -> 1168*960, for flicker algorithm */
    { SIZE_RATIO_11_9,
     1632      ,  1224     ,   /* [sensor ] */
     1632      ,  1224     ,   /* [bns    ] */
     1496      ,  1224     ,   /* [bcrop  ] */
     1496      ,  1224     ,   /* [bds    ] */
     1168      ,   960     ,   /* [target ] */
    }
};

static int DUAL_VIDEO_SIZE_LUT_IMX320_3H1[][SIZE_OF_LUT] =
{
    /* Binning   = OFF
       BNS ratio = 1.0
       BDS       = 1080p */

    { SIZE_RATIO_16_9,
      3264      , 1836      ,   /* [sensor ] */
      3264      , 1836      ,   /* [bns    ] */
      3264      , 1836      ,   /* [bcrop  ] */
      3264      , 1836      ,   /* [bds    ] */
      1920      , 1080      ,   /* [target ] */
    },
    /*  4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
      3264      , 2448      ,   /* [sensor ] */
      3264      , 2448      ,   /* [bns    ] */
      3264      , 2448      ,   /* [bcrop  ] */
      3264      , 2448      ,   /* [bds    ] */
       640      , 480      ,   /* [target ] */
    },
    /*  1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
      2448      , 2448      ,   /* [sensor ] */
      2448      , 2448      ,   /* [bns    ] */
      2448      , 2448      ,   /* [bcrop  ] */
      1080      , 1080      ,   /* [bds    ] *//* w=1080, Increased for 16 pixel align */
      1080      , 1080      ,   /* [target ] */
    },
};

static int IMX320_3H1_YUV_LIST[][SIZE_OF_RESOLUTION] =
{
    /* { width, height, minFrameDuration, ratioId } */
    { 3264, 2448, 33331760, SIZE_RATIO_4_3},
    { 3264, 1836, 33331760, SIZE_RATIO_16_9},
    { 2592, 1944, 33331760, SIZE_RATIO_4_3},
    { 2592, 1458, 33331760, SIZE_RATIO_16_9},
    { 2560, 1440, 33331760, SIZE_RATIO_16_9},
    { 2048, 1536, 33331760, SIZE_RATIO_4_3},
    { 1936, 1936, 33331760, SIZE_RATIO_1_1},
    { 1920, 1440, 33331760, SIZE_RATIO_4_3},
    { 1920, 1080, 33331760, SIZE_RATIO_16_9},
    { 1440, 1080, 33331760, SIZE_RATIO_4_3},
    { 1088, 1088, 33331760, SIZE_RATIO_1_1},
    { 1072, 1072, 33331760, SIZE_RATIO_1_1},
    { 1440, 1440, 33331760, SIZE_RATIO_1_1},
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
static int IMX320_3H1_JPEG_LIST[][SIZE_OF_RESOLUTION] =
{
    /* { width, height, minFrameDuration, ratioId } */
    { 3264, 2448, 50000000, SIZE_RATIO_4_3},
    { 3264, 1836, 50000000, SIZE_RATIO_16_9},
    { 2592, 1944, 50000000, SIZE_RATIO_4_3},
    { 2592, 1458, 50000000, SIZE_RATIO_16_9},
    { 2560, 1440, 50000000, SIZE_RATIO_16_9},
    { 2048, 1536, 50000000, SIZE_RATIO_4_3},
    { 1936, 1936, 50000000, SIZE_RATIO_1_1},
    { 1920, 1440, 50000000, SIZE_RATIO_4_3},
    { 1920, 1080, 33331760, SIZE_RATIO_16_9},
    { 1440, 1080, 33331760, SIZE_RATIO_4_3},
    { 1088, 1088, 33331760, SIZE_RATIO_1_1},
    { 1072, 1072, 33331760, SIZE_RATIO_1_1},
    { 1440, 1440, 33331760, SIZE_RATIO_1_1},
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

static int IMX320_3H1_THUMBNAIL_LIST[][SIZE_OF_RESOLUTION] =
{
    {  512,  384, 0, SIZE_RATIO_4_3},
    {  512,  288, 0, SIZE_RATIO_16_9},
    {  384,  384, 0, SIZE_RATIO_1_1},
    {  320,  240, 0, SIZE_RATIO_4_3},
    {    0,    0, 0, SIZE_RATIO_1_1}
};

/* vendor static info : width, height, min_fps, max_fps, vdis width, vdis height, recording limit time(sec) */
static int IMX320_3H1_AVAILABLE_VIDEO_LIST[][7] =
{
#ifdef USE_WQHD_RECORDING
    { 2560, 1440, 30000, 30000, 3072, 1728, 0},
#endif
    { 1440, 1440, 30000, 30000, 0, 0, 0},
    { 1920, 1080, 30000, 30000, 2304, 1296, 0},
    { 1280,  720, 30000, 30000, 1536, 864, 0},
    {  640,  480, 30000, 30000, 0, 0, 0},
};

static int IMX320_3H1_HIGH_SPEED_VIDEO_LIST[][SIZE_OF_RESOLUTION] =
{
    {  800,  600, 0, SIZE_RATIO_4_3},
};

static int IMX320_3H1_FPS_RANGE_LIST[][2] =
{
    {  15000,  15000},
    {  24000,  24000},
    {  7000,  30000},
    {  8000,  30000},
    {  10000,  30000},
    {  15000,  30000},
    {  30000,  30000},
};

static int IMX320_3H1_HIGH_SPEED_VIDEO_FPS_RANGE_LIST[][2] =
{
    {  30000, 120000},
    {  60000, 120000},
    { 120000, 120000},
};

#endif
