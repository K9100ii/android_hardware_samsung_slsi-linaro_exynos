/*
 **
 **copyright 2013, Samsung Electronics Co. LTD
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

#ifndef EXYNOS_CAMERA_LUT_IMX230_H
#define EXYNOS_CAMERA_LUT_IMX230_H

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

static int PREVIEW_SIZE_LUT_IMX230_BINNING2[][SIZE_OF_LUT] =
{
    /* Binning   = ON
       BNS ratio = 1.0
       BDS       = 1080p */

    /* 16:9 (Single, Dual) -- Not used */
    { SIZE_RATIO_16_9,
        (2656 + 16),(1992 + 10),   /* [sensor ] */ /* Sensor binning ratio = 2 */
        2672      , 2002      ,   /* [bns    ] */
        2656      , 1494      ,   /* [bcrop  ] */
        1920      , 1080      ,   /* [bds    ] */
        1920      , 1080      ,   /* [target ] */
    },
    /* 4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
        (2656 + 16),(1992 + 10),   /* [sensor ] */ /* Sensor binning ratio = 2 */
        2672      , 2002      ,   /* [bns    ] */
        2656      , 1992      ,   /* [bcrop  ] */
        1440      , 1080      ,   /* [bds    ] */
        1440      , 1080      ,   /* [target ] */
    },
    /* 1:1 (Single, Dual) -- Not used */
    { SIZE_RATIO_1_1,
        (2656 + 16),(1992 + 10),   /* [sensor ] */ /* Sensor binning ratio = 2 */
        2672      , 2002      ,   /* [bns    ] */
        1494      , 1494      ,   /* [bcrop  ] */
        1088      , 1080      ,   /* [bds    ] */
        1088      , 1080      ,   /* [target ] */
    },
    /* 3:2 (Single, Dual) -- Not used */
    { SIZE_RATIO_3_2,
        (2656 + 16),(1992 + 10),   /* [sensor ] */ /* Sensor binning ratio = 2 */
        2672      , 2002      ,   /* [bns    ] */
        2241      , 1494      ,   /* [bcrop  ] */
        1616      , 1080      ,   /* [bds    ] */
        1616      , 1080      ,   /* [target ] */
    },
    /* 5:4 (Single, Dual) -- Not used */
    { SIZE_RATIO_5_4,
        (2656 + 16),(1992 + 10),   /* [sensor ] */ /* Sensor binning ratio = 2 */
        2672      , 2002      ,   /* [bns    ] */
        1868      , 1494      ,   /* [bcrop  ] */
        1344      , 1080      ,   /* [bds    ] */
        1344      , 1080      ,   /* [target ] */
    },
    /* 5:3 (Single, Dual) -- Not used */
    { SIZE_RATIO_5_3,
        (2656 + 16),(1992 + 10),   /* [sensor ] */ /* Sensor binning ratio = 2 */
        2672      , 2002      ,   /* [bns    ] */
        2490      , 1494      ,   /* [bcrop  ] */
        1792      , 1080      ,   /* [bds    ] */
        1792      , 1080      ,   /* [target ] */
    },
    /* 11:9 (Single, Dual) -- Not used */
    { SIZE_RATIO_11_9,
        (2656 + 16),(1998 + 10),   /* [sensor ] */ /* Sensor binning ratio = 2 */
        2672      , 2008      ,   /* [bns    ] */
        1826      , 1494      ,   /* [bcrop  ] */
        1320      , 1080      ,   /* [bds    ] */
        1320      , 1080      ,   /* [target ] */
    }
};

static int PREVIEW_SIZE_LUT_IMX230[][SIZE_OF_LUT] =
{
    /* Binning   = OFF
       BNS ratio = 1.0
       BDS       = 1080p */

    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
        (5312 + 16),(2990 + 10),   /* [sensor ] */
        5328      , 3000      ,   /* [bns    ] */
        5316      , 2990      ,   /* [bcrop  ] */
        1920      , 1080      ,   /* [bds    ] */
        1920      , 1080      ,   /* [target ] */
    },
    /* 4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
        (5328 + 16),(4006 + 10),   /* [sensor ] */
        5344      , 4016      ,   /* [bns    ] */
        5328      , 3996      ,   /* [bcrop  ] */
        1440      , 1080      ,   /* [bds    ] */
        1440      , 1080      ,   /* [target ] */
    },
    /* 1:1 (Single, Dual) -- Not used */
    { SIZE_RATIO_1_1,
        (5328 + 16),(4006 + 10),   /* [sensor ] */
        5344      , 4016      ,   /* [bns    ] */
        2990      , 2990      ,   /* [bcrop  ] */
        1088      , 1080      ,   /* [bds    ] */
        1088      , 1080      ,   /* [target ] */
    },
    /* 3:2 (Single, Dual) -- Not used */
    { SIZE_RATIO_3_2,
        (5328 + 16),(4006 + 10),   /* [sensor ] */
        5344      , 4016      ,   /* [bns    ] */
        4486      , 2990      ,   /* [bcrop  ] */
        1616      , 1080      ,   /* [bds    ] */
        1616      , 1080      ,   /* [target ] */
    },
    /* 5:4 (Single, Dual) -- Not used */
    { SIZE_RATIO_5_4,
        (5328 + 16),(4006 + 10),   /* [sensor ] */
        5344      , 4016      ,   /* [bns    ] */
        3738      , 2990      ,   /* [bcrop  ] */
        1344      , 1080      ,   /* [bds    ] */
        1344      , 1080      ,   /* [target ] */
    },
    /* 5:3 (Single, Dual) -- Not used */
    { SIZE_RATIO_5_3,
        (5328 + 16),(4006 + 10),   /* [sensor ] */
        5344      , 4016      ,   /* [bns    ] */
        4984      , 2990      ,   /* [bcrop  ] */
        1792      , 1080      ,   /* [bds    ] */
        1792      , 1080      ,   /* [target ] */
    },
    /* 11:9 (Single, Dual) -- Not used */
    { SIZE_RATIO_11_9,
        (5328 + 16),(4006 + 10),   /* [sensor ] */
        5344      , 4016      ,   /* [bns    ] */
        3656      , 2990      ,   /* [bcrop  ] */
        1320      , 1080      ,   /* [bds    ] */
        1320      , 1080      ,   /* [target ] */
    },

};

static int PREVIEW_SIZE_LUT_IMX230_ZSL_16M[][SIZE_OF_LUT] =
{
    /* Binning   = OFF
       BNS ratio = 1.0
       BDS       = 1080p */

    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
        (5312 + 16),(2990 + 10),   /* [sensor ] */
        5328      , 3000      ,   /* [bns    ] */
        5316      , 2990      ,   /* [bcrop  ] */
        1920      , 1080      ,   /* [bds    ] */
        1920      , 1080      ,   /* [target ] */
    },
    /* 4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
        (5312 + 16),(2990 + 10),   /* [sensor ] */
        5328      , 3000      ,   /* [bns    ] */
        3988      , 2990      ,   /* [bcrop  ] */
        1440      , 1080      ,   /* [bds    ] */
        1440      , 1080      ,   /* [target ] */
    },
    /* 1:1 (Single, Dual) -- Not used */
    { SIZE_RATIO_1_1,
        (5312 + 16),(2990 + 10),   /* [sensor ] */
        5328      , 3000      ,   /* [bns    ] */
        2990      , 2990      ,   /* [bcrop  ] */
        1088      , 1080      ,   /* [bds    ] */
        1088      , 1080      ,   /* [target ] */
    },
    /* 3:2 (Single, Dual) -- Not used */
    { SIZE_RATIO_3_2,
        (5312 + 16),(2990 + 10),   /* [sensor ] */
        5328      , 3000      ,   /* [bns    ] */
        4486      , 2990      ,   /* [bcrop  ] */
        1616      , 1080      ,   /* [bds    ] */
        1616      , 1080      ,   /* [target ] */
    },
    /* 5:4 (Single, Dual) -- Not used */
    { SIZE_RATIO_5_4,
        (5312 + 16),(2990 + 10),   /* [sensor ] */
        5328      , 3000      ,   /* [bns    ] */
        3738      , 2990      ,   /* [bcrop  ] */
        1344      , 1080      ,   /* [bds    ] */
        1344      , 1080      ,   /* [target ] */
    },
    /* 5:3 (Single, Dual) -- Not used */
    { SIZE_RATIO_5_3,
        (5312 + 16),(2990 + 10),   /* [sensor ] */
        5328      , 3000      ,   /* [bns    ] */
        4984      , 2990      ,   /* [bcrop  ] */
        1792      , 1080      ,   /* [bds    ] */
        1792      , 1080      ,   /* [target ] */
    },
    /* 11:9 (Single, Dual) -- Not used */
    { SIZE_RATIO_11_9,
        (5312 + 16),(2990 + 10),   /* [sensor ] */
        5328      , 3000      ,   /* [bns    ] */
        3656      , 2990      ,   /* [bcrop  ] */
        1320      , 1080      ,   /* [bds    ] */
        1320      , 1080      ,   /* [target ] */
    },

};

#ifdef M_LONG_SHUTTER
static int PREVIEW_SIZE_LUT_IMX230_LONG_EXPOSURE[][SIZE_OF_LUT] =
{
    /* Binning   = OFF
       BNS ratio = 1.0
       BDS       = 1080p */

    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
        (5328 + 16),(4006 + 10),   /* [sensor ] */
        5344      , 4016      ,   /* [bns    ] */
        5316      , 2990      ,   /* [bcrop  ] */
        1920      , 1080      ,   /* [bds    ] */
        1920      , 1080      ,   /* [target ] */
    },
    /* 4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
        (5328 + 16),(4006 + 10),   /* [sensor ] */
        5344      , 4016      ,   /* [bns    ] */
        5328      , 3996      ,   /* [bcrop  ] */
        1440      , 1080      ,   /* [bds    ] */
        1440      , 1080      ,   /* [target ] */
    },
    /* 1:1 (Single, Dual) -- Not used */
    { SIZE_RATIO_1_1,
        (5328 + 16),(4006 + 10),   /* [sensor ] */
        5344      , 4016      ,   /* [bns    ] */
        2990      , 2990      ,   /* [bcrop  ] */
        1088      , 1080      ,   /* [bds    ] */
        1088      , 1080      ,   /* [target ] */
    },
    /* 3:2 (Single, Dual) -- Not used */
    { SIZE_RATIO_3_2,
        (5328 + 16),(4006 + 10),   /* [sensor ] */
        5344      , 4016      ,   /* [bns    ] */
        4486      , 2990      ,   /* [bcrop  ] */
        1616      , 1080      ,   /* [bds    ] */
        1616      , 1080      ,   /* [target ] */
    },
    /* 5:4 (Single, Dual) -- Not used */
    { SIZE_RATIO_5_4,
        (5328 + 16),(4006 + 10),   /* [sensor ] */
        5344      , 4016      ,   /* [bns    ] */
        3738      , 2990      ,   /* [bcrop  ] */
        1344      , 1080      ,   /* [bds    ] */
        1344      , 1080      ,   /* [target ] */
    },
    /* 5:3 (Single, Dual) -- Not used */
    { SIZE_RATIO_5_3,
        (5328 + 16),(4006 + 10),   /* [sensor ] */
        5344      , 4016      ,   /* [bns    ] */
        4984      , 2990      ,   /* [bcrop  ] */
        1792      , 1080      ,   /* [bds    ] */
        1792      , 1080      ,   /* [target ] */
    },
    /* 11:9 (Single, Dual) -- Not used */
    { SIZE_RATIO_11_9,
        (5328 + 16),(4006 + 10),   /* [sensor ] */
        5344      , 4016      ,   /* [bns    ] */
        3656      , 2990      ,   /* [bcrop  ] */
        1320      , 1080      ,   /* [bds    ] */
        1320      , 1080      ,   /* [target ] */
    },

};

static int PICTURE_SIZE_LUT_IMX230_LONG_EXPOSURE[][SIZE_OF_LUT] =
{
    /* Binning   = OFF
       BNS ratio = 1.0
       BDS       = OFF */

    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
        (5328 + 16),(4006 + 10),   /* [sensor ] */  // sensor margin 16x12
        5344      , 3000      ,   /* [bns    ] */
        5316      , 4016      ,   /* [bcrop  ] */
        5316      , 2990      ,   /* [bds    ] */
        5316      , 2990      ,   /* [target ] */
    },
    /* 4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
        (5328 + 16),(4006 + 10),   /* [sensor ] */
        5344      , 4016      ,   /* [bns    ] */
        5328      , 3996      ,   /* [bcrop  ] */
        5328      , 3996      ,   /* [bds    ] */
        5328      , 3996      ,   /* [target ] */
    },
    /* 1:1 (Single, Dual) -- Not used */
    { SIZE_RATIO_1_1,
        (5328 + 16),(4006 + 10),   /* [sensor ] */
        5344      , 4016      ,   /* [bns    ] */
        3984      , 3984      ,   /* [bcrop  ] */
        3984      , 3984      ,   /* [bds    ] */
        3984      , 3984      ,   /* [target ] */
    },
    /* 3:2 (Single, Dual) -- Not used */
    { SIZE_RATIO_3_2,
        (5328 + 16),(4006 + 10),   /* [sensor ] */
        5344      , 4016      ,   /* [bns    ] */
        5328      , 3552      ,   /* [bcrop  ] */
        5328      , 3552      ,   /* [bds    ] */
        5328      , 3552      ,   /* [target ] */
    },
    /* 5:4 (Single, Dual) -- Not used */
    { SIZE_RATIO_5_4,
        (5328 + 16),(4006 + 10),   /* [sensor ] */
        5344      , 4016      ,   /* [bns    ] */
        4980      , 3984      ,   /* [bcrop  ] */
        4980      , 3984      ,   /* [bds    ] */
        4980      , 3984      ,   /* [target ] */
    },
    /* 5:3 (Single, Dual) -- Not used */
    { SIZE_RATIO_5_3,
        (5328 + 16),(4006 + 10),   /* [sensor ] */
        5344      , 4016      ,   /* [bns    ] */
        5328      , 3188      ,   /* [bcrop  ] */
        5328      , 3188      ,   /* [bds    ] */
        5328      , 3188      ,   /* [target ] */
    },
    /* 11:9 (Single, Dual) -- Not used */
    { SIZE_RATIO_11_9,
        (5328 + 16),(4006 + 10),   /* [sensor ] */
        5344      , 4016      ,   /* [bns    ] */
        4868      , 3984      ,   /* [bcrop  ] */
        4868      , 3984      ,   /* [bds    ] */
        4868      , 3984      ,   /* [target ] */
    }
};
#endif

static int PICTURE_SIZE_LUT_IMX230[][SIZE_OF_LUT] =
{
    /* Binning   = OFF
       BNS ratio = 1.0
       BDS       = OFF */

    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
        (5312 + 16),(2990 + 10),   /* [sensor ] */  // sensor margin 16x12
        5328      , 3000      ,   /* [bns    ] */
        5316      , 2990      ,   /* [bcrop  ] */
        5316      , 2990      ,   /* [bds    ] */
        5316      , 2990      ,   /* [target ] */
    },
    /* 4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
        (5328 + 16),(4006 + 10),   /* [sensor ] */
        5344      , 4016      ,   /* [bns    ] */
        5328      , 3996      ,   /* [bcrop  ] */
        5328      , 3996      ,   /* [bds    ] */
        5328      , 3996      ,   /* [target ] */
    },
    /* 1:1 (Single, Dual) -- Not used */
    { SIZE_RATIO_1_1,
        (5328 + 16),(4006 + 10),   /* [sensor ] */
        5344      , 4016      ,   /* [bns    ] */
        3984      , 3984      ,   /* [bcrop  ] */
        3984      , 3984      ,   /* [bds    ] */
        3984      , 3984      ,   /* [target ] */
    },
    /* 3:2 (Single, Dual) -- Not used */
    { SIZE_RATIO_3_2,
        (5328 + 16),(4006 + 10),   /* [sensor ] */
        5344      , 4016      ,   /* [bns    ] */
        5328      , 3552      ,   /* [bcrop  ] */
        5328      , 3552      ,   /* [bds    ] */
        5328      , 3552      ,   /* [target ] */
    },
    /* 5:4 (Single, Dual) -- Not used */
    { SIZE_RATIO_5_4,
        (5328 + 16),(4006 + 10),   /* [sensor ] */
        5344      , 4016      ,   /* [bns    ] */
        4980      , 3984      ,   /* [bcrop  ] */
        4980      , 3984      ,   /* [bds    ] */
        4980      , 3984      ,   /* [target ] */
    },
    /* 5:3 (Single, Dual) -- Not used */
    { SIZE_RATIO_5_3,
        (5328 + 16),(4006 + 10),   /* [sensor ] */
        5344      , 4016      ,   /* [bns    ] */
        5328      , 3188      ,   /* [bcrop  ] */
        5328      , 3188      ,   /* [bds    ] */
        5328      , 3188      ,   /* [target ] */
    },
    /* 11:9 (Single, Dual) -- Not used */
    { SIZE_RATIO_11_9,
        (5328 + 16),(4006 + 10),   /* [sensor ] */
        5344      , 4016      ,   /* [bns    ] */
        4868      , 3984      ,   /* [bcrop  ] */
        4868      , 3984      ,   /* [bds    ] */
        4868      , 3984      ,   /* [target ] */
    }
};

static int PICTURE_SIZE_LUT_IMX230_ZSL_16M[][SIZE_OF_LUT] =
{
    /* Binning   = OFF
       BNS ratio = 1.0
       BDS       = OFF */

    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
        (5312 + 16),(2990 + 10),   /* [sensor ] */
        5328      , 3000      ,   /* [bns    ] */
        5312      , 2988      ,   /* [bcrop  ] */
        5312      , 2988      ,   /* [bds    ] */
        5312      , 2988      ,   /* [target ] */
    },
    /* 4:3 (Single, Dual) -- Not used */
    { SIZE_RATIO_4_3,
        (5312 + 16),(2990 + 10),   /* [sensor ] */
        5328      , 3000      ,   /* [bns    ] */
        3984      , 2988      ,   /* [bcrop  ] */
        3984      , 2988      ,   /* [bds    ] */
        3984      , 2988      ,   /* [target ] */
    },
    /* 1:1 (Single, Dual) -- Not used */
    { SIZE_RATIO_1_1,
        (5312 + 16),(2990 + 10),   /* [sensor ] */
        5328      , 3000      ,   /* [bns    ] */
        2988      , 2988      ,   /* [bcrop  ] */
        2988      , 2988      ,   /* [bds    ] */
        2988      , 2988      ,   /* [target ] */
    },
    /* 3:2 (Single, Dual) -- Not used */
    { SIZE_RATIO_3_2,
        (5312 + 16),(2990 + 10),   /* [sensor ] */
        5328      , 3000      ,   /* [bns    ] */
        4480      , 2988      ,   /* [bcrop  ] */
        4480      , 2988      ,   /* [bds    ] */
        4480      , 2988      ,   /* [target ] */
    },
    /* 5:4 (Single, Dual) -- Not used */
    { SIZE_RATIO_5_4,
        (5312 + 16),(2990 + 10),   /* [sensor ] */
        5328      , 3000      ,   /* [bns    ] */
        3736      , 2988      ,   /* [bcrop  ] */
        3736      , 2988      ,   /* [bds    ] */
        3736      , 2988      ,   /* [target ] */
    },
    /* 5:3 (Single, Dual) -- Not used */
    { SIZE_RATIO_5_3,
        (5312 + 16),(2990 + 10),   /* [sensor ] */
        5328      , 3000      ,   /* [bns    ] */
        4980      , 2988      ,   /* [bcrop  ] */
        4980      , 2988      ,   /* [bds    ] */
        4980      , 2988      ,   /* [target ] */
    },
    /* 11:9 (Single, Dual) -- Not used */
    { SIZE_RATIO_11_9,
        (5312 + 16),(2990 + 10),   /* [sensor ] */
        5328      , 3000      ,   /* [bns    ] */
        3652      , 2988      ,   /* [bcrop  ] */
        3652      , 2988      ,   /* [bds    ] */
        3652      , 2988      ,   /* [target ] */
    }
};

static int PICTURE_SIZE_LUT_IMX230_ZSL_BINNING2[][SIZE_OF_LUT] =
{
    /* Binning   = ON
       BNS ratio = 1.0
       BDS       = 1080p */

    /* 16:9 (Single, Dual) -- Not used */
    { SIZE_RATIO_16_9,
        (2656 + 16),(1992 + 10),   /* [sensor ] */ /* Sensor binning ratio = 2 */
        2672      , 2002      ,   /* [bns    ] */
        2656      , 1494      ,   /* [bcrop  ] */
        2656      , 1494      ,   /* [bds    ] */
        2656      , 1494      ,   /* [target ] */
    },
    /* 4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
        (2656 + 16),(1992 + 10),   /* [sensor ] */ /* Sensor binning ratio = 2 */
        2672      , 2002      ,   /* [bns    ] */
        2656      , 1992      ,   /* [bcrop  ] */
        2656      , 1992      ,   /* [bds    ] */
        2656      , 1992      ,   /* [target ] */
    },
    /* 1:1 (Single, Dual) -- Not used */
    { SIZE_RATIO_1_1,
        (2656 + 16),(1992 + 10),   /* [sensor ] */ /* Sensor binning ratio = 2 */
        2672      , 2002      ,   /* [bns    ] */
        1494      , 1494      ,   /* [bcrop  ] */
        1494      , 1494      ,   /* [bds    ] */
        1494      , 1494      ,   /* [target ] */
    },
    /* 3:2 (Single, Dual) -- Not used */
    { SIZE_RATIO_3_2,
        (2656 + 16),(1992 + 10),   /* [sensor ] */ /* Sensor binning ratio = 2 */
        2672      , 2002      ,   /* [bns    ] */
        2241      , 1494      ,   /* [bcrop  ] */
        2241      , 1494      ,   /* [bds    ] */
        2241      , 1494      ,   /* [target ] */
    },
    /* 5:4 (Single, Dual) -- Not used */
    { SIZE_RATIO_5_4,
        (2656 + 16),(1992 + 10),   /* [sensor ] */ /* Sensor binning ratio = 2 */
        2672      , 2002      ,   /* [bns    ] */
        1868      , 1494      ,   /* [bcrop  ] */
        1868      , 1494      ,   /* [bds    ] */
        1868      , 1494      ,   /* [target ] */
    },
    /* 5:3 (Single, Dual) -- Not used */
    { SIZE_RATIO_5_3,
        (2656 + 16),(1992 + 10),   /* [sensor ] */ /* Sensor binning ratio = 2 */
        2672      , 2002      ,   /* [bns    ] */
        2490      , 1494      ,   /* [bcrop  ] */
        2490      , 1494      ,   /* [bds    ] */
        2490      , 1494      ,   /* [target ] */
    },
    /* 11:9 (Single, Dual) -- Not used */
    { SIZE_RATIO_11_9,
        (2656 + 16),(1992 + 10),   /* [sensor ] */ /* Sensor binning ratio = 2 */
        2672      , 2002      ,   /* [bns    ] */
        1826      , 1494      ,   /* [bcrop  ] */
        1826      , 1494      ,   /* [bds    ] */
        1826      , 1494      ,   /* [target ] */
    }
};

static int VIDEO_SIZE_LUT_IMX230_FHD[][SIZE_OF_LUT] =
{

    /* 16:9 (Single, Dual) -- Not used */
    { SIZE_RATIO_16_9,
        (2656 + 16),(1992 + 10),   /* [sensor ] */
        2672      , 2002      ,   /* [bns    ] */
        2656      , 1494      ,   /* [bcrop  ] */
        1920      , 1080      ,   /* [bds    ] */
        1920      , 1080      ,   /* [target ] */
    },
    /* 4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
        (2656 + 16),(1992 + 10),   /* [sensor ] */
        2672      , 2002      ,   /* [bns    ] */
        1992      , 1494      ,   /* [bcrop  ] */
        1440      , 1080      ,   /* [bds    ] */
        1440      , 1080      ,   /* [target ] */
    },
    /* 1:1 (Single, Dual) -- Not used */
    { SIZE_RATIO_1_1,
        (2656 + 16),(1992 + 10),   /* [sensor ] */
        2672      , 2002      ,   /* [bns    ] */
        1494      , 1494      ,   /* [bcrop  ] */
        1088      , 1080      ,   /* [bds    ] */
        1088      , 1080      ,   /* [target ] */
    },
    /* 3:2 (Single, Dual) -- Not used */
    { SIZE_RATIO_3_2,
        (2656 + 16),(1992 + 10),   /* [sensor ] */
        2672      , 2002      ,   /* [bns    ] */
        2241      , 1494      ,   /* [bcrop  ] */
        1616      , 1080      ,   /* [bds    ] */
        1616      , 1080      ,   /* [target ] */
    },
    /* 5:4 (Single, Dual) -- Not used */
    { SIZE_RATIO_5_4,
        (2656 + 16),(1992 + 10),   /* [sensor ] */
        2672      , 2002      ,   /* [bns    ] */
        1868      , 1494      ,   /* [bcrop  ] */
        1344      , 1080      ,   /* [bds    ] */
        1344      , 1080      ,   /* [target ] */
    },
    /* 5:3 (Single, Dual) -- Not used */
    { SIZE_RATIO_5_3,
        (2656 + 16),(1992 + 10),   /* [sensor ] */
        2672      , 2002      ,   /* [bns    ] */
        2490      , 1494      ,   /* [bcrop  ] */
        1792      , 1080      ,   /* [bds    ] */
        1792      , 1080      ,   /* [target ] */
    },
    /* 11:9 (Single, Dual) -- Not used */
    { SIZE_RATIO_11_9,
        (2656 + 16),(1998 + 10),   /* [sensor ] */
        2672      , 2008      ,   /* [bns    ] */
        1826      , 1494      ,   /* [bcrop  ] */
        1320      , 1080      ,   /* [bds    ] */
        1320      , 1080      ,   /* [target ] */
    }
};

static int VIDEO_SIZE_LUT_IMX230_4K[][SIZE_OF_LUT] =
{

    /* 16:9 (Single) */
    { SIZE_RATIO_16_9,
        (3840 + 16),(2160 + 10),   /* [sensor ] */
        3856      , 2170      ,   /* [bns    ] */
        3840      , 2160      ,   /* [bcrop  ] */
        3840      , 2160      ,   /* [bds    ] *//* UHD (3840x2160) special handling in ExynosCameraParameters class */
        3840      , 2160      ,   /* [target ] */
    },
    /* 4:3 (Single) -- Not used */
    { SIZE_RATIO_4_3,
        (3840 + 16),(2160 + 10),   /* [sensor ] */
        3856      , 2170      ,   /* [bns    ] */
        2880      , 2160      ,   /* [bcrop  ] */
        2880      , 2160      ,   /* [bds    ] *//* UHD (3840x2160) special handling in ExynosCameraParameters class */
        2880      , 2160      ,   /* [target ] */
    },
    /* 1:1 (Single) -- Not used */
    { SIZE_RATIO_1_1,
        (3840 + 16),(2160 + 10),   /* [sensor ] */
        3856      , 2170      ,   /* [bns    ] */
        2160      , 2160      ,   /* [bcrop  ] */
        2160      , 2160      ,   /* [bds    ] *//* UHD (3840x2160) special handling in ExynosCameraParameters class */
        2160      , 2160      ,   /* [target ] */
    },
    /* 3:2 (Single) -- Not used */
    { SIZE_RATIO_3_2,
        (3840 + 16),(2160 + 10),   /* [sensor ] */
        3856      , 2170      ,   /* [bns    ] */
        3240      , 2160      ,   /* [bcrop  ] */
        3240      , 2160      ,   /* [bds    ] *//* UHD (3840x2160) special handling in ExynosCameraParameters class */
        3240      , 2160      ,   /* [target ] */
    },
    /* 5:4 (Single) -- Not used */
    { SIZE_RATIO_5_4,
        (3840 + 16),(2160 + 10),   /* [sensor ] */
        3856      , 2170      ,   /* [bns    ] */
        2700      , 2160      ,   /* [bcrop  ] */
        2700      , 2160      ,   /* [bds    ] *//* UHD (3840x2160) special handling in ExynosCameraParameters class */
        2700      , 2160      ,   /* [target ] */
    },
    /* 5:3 (Single) -- Not used */
    { SIZE_RATIO_5_3,
        (3840 + 16),(2160 + 10),   /* [sensor ] */
        3856      , 2170      ,   /* [bns    ] */
        3600      , 2160      ,   /* [bcrop  ] */
        3600      , 2160      ,   /* [bds    ] *//* UHD (3840x2160) special handling in ExynosCameraParameters class */
        3600      , 2160      ,   /* [target ] */
    },
    /* 11:9 (Single) -- Not used */
    { SIZE_RATIO_11_9,
        (3840 + 16),(2160 + 10),   /* [sensor ] */
        3856      , 2170      ,   /* [bns    ] */
        2640      , 2160      ,   /* [bcrop  ] */
        2640      , 2160      ,   /* [bds    ] *//* UHD (3840x2160) special handling in ExynosCameraParameters class */
        2640      , 2160      ,   /* [target ] */
    },
};


static int VIDEO_SIZE_LUT_IMX230_DEFALT[][SIZE_OF_LUT] =
{
    /* Binning   = ON
       BNS ratio = 1.0
       BDS       = 1080p */

    /* 16:9 (Single) */
    { SIZE_RATIO_16_9,
        (1280 + 16),(720 + 10),   /* [sensor ] *//* Sensor binning ratio = 4 */
        1296      , 730      ,   /* [bns    ] */
        1280      , 720      ,   /* [bcrop  ] */
        1280      , 720      ,   /* [bds    ] */
        1280      , 720      ,   /* [target ] */
    },
    /* 4:3 (Single) -- Not used */
    { SIZE_RATIO_4_3,
        (1280 + 16),(720 + 10),   /* [sensor ] *//* Sensor binning ratio = 4 */
        1296      , 730      ,   /* [bns    ] */
        960      , 720      ,   /* [bcrop  ] */
        960      , 720      ,   /* [bds    ] */
        960      , 720      ,   /* [target ] */
    },
    /* 1:1 (Single) -- Not used */
    { SIZE_RATIO_1_1,
        (1280 + 16),(720 + 10),   /* [sensor ] *//* Sensor binning ratio = 4 */
        1296      , 730      ,   /* [bns    ] */
        720      , 720      ,   /* [bcrop  ] */
        720      , 720      ,   /* [bds    ] */
        720      , 720      ,   /* [target ] */
    },
    /* 3:2 (Single) -- Not used */
    { SIZE_RATIO_3_2,
        (1280 + 16),(720 + 10),   /* [sensor ] *//* Sensor binning ratio = 4 */
        1296      , 730      ,   /* [bns    ] */
        1080      , 720      ,   /* [bcrop  ] */
        1080      , 720      ,   /* [bds    ] */
        1080      , 720      ,   /* [target ] */
    },
    /* 5:4 (Single) -- Not used */
    { SIZE_RATIO_5_4,
        (1280 + 16),(720 + 10),   /* [sensor ] *//* Sensor binning ratio = 4 */
        1296      , 730      ,   /* [bns    ] */
        900      , 720      ,   /* [bcrop  ] */
        900      , 720      ,   /* [bds    ] */
        900      , 720      ,   /* [target ] */
    },
    /* 5:3 (Single) -- Not used */
    { SIZE_RATIO_5_3,
        (1280 + 16),(720 + 10),   /* [sensor ] *//* Sensor binning ratio = 4 */
        1296      , 730      ,   /* [bns    ] */
        1200      , 720      ,   /* [bcrop  ] */
        1200      , 720      ,   /* [bds    ] */
        1200      , 720      ,   /* [target ] */
    },
    /* 11:9 (Single) -- Not used */
    { SIZE_RATIO_11_9,
        (1280 + 16),(720 + 10),   /* [sensor ] *//* Sensor binning ratio = 4 */
        1296      , 730      ,   /* [bns    ] */
        880      , 720      ,   /* [bcrop  ] */
        880      , 720      ,   /* [bds    ] */
        880      , 720      ,   /* [target ] */
    }

};
static int VIDEO_SIZE_LUT_IMX230[][SIZE_OF_LUT] =
{

    /* 16:9 (Single) */
    { SIZE_RATIO_16_9,
        (3840 + 16),(2160 + 10),   /* [sensor ] */
        3856      , 2170      ,   /* [bns    ] */
        3840      , 2160      ,   /* [bcrop  ] */
        3840      , 2160      ,   /* [bds    ] *//* UHD (3840x2160) special handling in ExynosCameraParameters class */
        3840      , 2160      ,   /* [target ] */
    },
    /* 4:3 (Single) -- Not used */
    { SIZE_RATIO_4_3,
        (3840 + 16),(2160 + 10),   /* [sensor ] */
        3856      , 2170      ,   /* [bns    ] */
        2880      , 2160      ,   /* [bcrop  ] */
        2880      , 2160      ,   /* [bds    ] *//* UHD (3840x2160) special handling in ExynosCameraParameters class */
        2880      , 2160      ,   /* [target ] */
    },
    /* 1:1 (Single) -- Not used */
    { SIZE_RATIO_1_1,
        (3840 + 16),(2160 + 10),   /* [sensor ] */
        3856      , 2170      ,   /* [bns    ] */
        2160      , 2160      ,   /* [bcrop  ] */
        2160      , 2160      ,   /* [bds    ] *//* UHD (3840x2160) special handling in ExynosCameraParameters class */
        2160      , 2160      ,   /* [target ] */
    },
    /* 3:2 (Single) -- Not used */
    { SIZE_RATIO_3_2,
        (3840 + 16),(2160 + 10),   /* [sensor ] */
        3856      , 2170      ,   /* [bns    ] */
        3240      , 2160      ,   /* [bcrop  ] */
        3240      , 2160      ,   /* [bds    ] *//* UHD (3840x2160) special handling in ExynosCameraParameters class */
        3240      , 2160      ,   /* [target ] */
    },
    /* 5:4 (Single) -- Not used */
    { SIZE_RATIO_5_4,
        (3840 + 16),(2160 + 10),   /* [sensor ] */
        3856      , 2170      ,   /* [bns    ] */
        2700      , 2160      ,   /* [bcrop  ] */
        2700      , 2160      ,   /* [bds    ] *//* UHD (3840x2160) special handling in ExynosCameraParameters class */
        2700      , 2160      ,   /* [target ] */
    },
    /* 5:3 (Single) -- Not used */
    { SIZE_RATIO_5_3,
        (3840 + 16),(2160 + 10),   /* [sensor ] */
        3856      , 2170      ,   /* [bns    ] */
        3600      , 2160      ,   /* [bcrop  ] */
        3600      , 2160      ,   /* [bds    ] *//* UHD (3840x2160) special handling in ExynosCameraParameters class */
        3600      , 2160      ,   /* [target ] */
    },
    /* 11:9 (Single) -- Not used */
    { SIZE_RATIO_11_9,
        (3840 + 16),(2160 + 10),   /* [sensor ] */
        3856      , 2170      ,   /* [bns    ] */
        2640      , 2160      ,   /* [bcrop  ] */
        2640      , 2160      ,   /* [bds    ] *//* UHD (3840x2160) special handling in ExynosCameraParameters class */
        2640      , 2160      ,   /* [target ] */
    },
};

static int VIDEO_SIZE_LUT_IMX230_HIGHSPEED[][SIZE_OF_LUT] =
{
    /* Binning   = 4
       BNS ratio = 1.0
       BDS       = 1080p */

    /* 16:9 (Single) */
    { SIZE_RATIO_16_9,
        (1280 + 16),(720 + 10),   /* [sensor ] *//* Sensor binning ratio = 4 */
        1296      , 730      ,   /* [bns    ] */
        1280      , 720      ,   /* [bcrop  ] */
        1280      , 720      ,   /* [bds    ] */
        1280      , 720      ,   /* [target ] */
    },
    /* 16:9 (Single) --- fastAE */
    { SIZE_RATIO_16_9,
        (1280 + 16),(720 + 10),   /* [sensor ] *//* Sensor binning ratio = 4 */
        1296      , 730      ,   /* [bns    ] */
        1280      , 720      ,   /* [bcrop  ] */
        1280      , 720      ,   /* [bds    ] */
        1280      , 720      ,   /* [target ] */
    },
    /* 16:9 (Single) --- 120fps */
    { SIZE_RATIO_16_9,
        (1280 + 16),(720 + 10),   /* [sensor ] *//* Sensor binning ratio = 4 */
        1296      , 730      ,   /* [bns    ] */
        1280      , 720      ,   /* [bcrop  ] */
        1280      , 720      ,   /* [bds    ] */
        1280      , 720      ,   /* [target ] */
    }
};

static int VIDEO_SIZE_LUT_IMX230_HDR[][SIZE_OF_LUT] =
{
    /* Binning   = 2
       BNS ratio = 1.0
       BDS       = 1080p */

    /* 16:9 (Single) */
    { SIZE_RATIO_16_9,
        (2656 + 16),(1998 + 10),   /* [sensor ] *//* Sensor binning ratio = 2 */
        2672      , 2008      ,   /* [bns    ] */
        1280      ,  720      ,   /* [bcrop  ] */
        1280      ,  720      ,   /* [bds    ] */
        1920      , 1080      ,   /* [target ] */
    }
};

static int IMX230_PREVIEW_LIST[][SIZE_OF_RESOLUTION] =
{
    { 1920, 1080, SIZE_RATIO_16_9},  // M
    { 1440, 1080, SIZE_RATIO_4_3},
    { 1280,  720, SIZE_RATIO_16_9},  // M
    { 720,  480, SIZE_RATIO_3_2},   // M
    { 640,  480, SIZE_RATIO_4_3},   // M
    { 320,  240, SIZE_RATIO_4_3},
    { 176,  144, SIZE_RATIO_11_9}
};


static int IMX230_HIDDEN_PREVIEW_LIST[][SIZE_OF_RESOLUTION] =
{
    { 3840, 2160, SIZE_RATIO_16_9}, // for 4K
    { 2560, 1536, SIZE_RATIO_4_3},
    { 1920, 1080, SIZE_RATIO_16_9},
    { 1440, 1080, SIZE_RATIO_4_3},
    { 1600, 1200, SIZE_RATIO_4_3},
    { 1280,  960, SIZE_RATIO_4_3},
    {   528,  432, SIZE_RATIO_11_9},
    {   480,  270, SIZE_RATIO_16_9},
    {   320,  240, SIZE_RATIO_4_3},
    {   176,  144, SIZE_RATIO_11_9}
};

static int IMX230_PICTURE_LIST[][SIZE_OF_RESOLUTION] =
{
    { 5312, 3984, SIZE_RATIO_4_3},  // M
    { 5312, 2988, SIZE_RATIO_16_9}, // M
    { 2656, 1992, SIZE_RATIO_4_3},  // M, ZSL
    { 1920, 1080, SIZE_RATIO_16_9},  // for CTS of testMandatoryOutputCombinations
};

static int IMX230_HIDDEN_PICTURE_LIST[][SIZE_OF_RESOLUTION] =
{

    { 3840, 2160, SIZE_RATIO_16_9},  // M
    { 2592, 1944, SIZE_RATIO_4_3}, // M
    { 2656, 1494, SIZE_RATIO_16_9},
    { 1920, 1080, SIZE_RATIO_16_9},  // M
    { 1280,  720, SIZE_RATIO_16_9},  // M
    {  880,  720, SIZE_RATIO_11_9},  // M
    {  720,  480, SIZE_RATIO_3_2},   // M
    {  640,  480, SIZE_RATIO_4_3},   // M
    {  320,  240, SIZE_RATIO_4_3},   // M
    {  176,  144, SIZE_RATIO_11_9},  // M
};

static int IMX230_THUMBNAIL_LIST[][SIZE_OF_RESOLUTION] =
{
    {  512,  384, SIZE_RATIO_4_3},
    {  512,  288, SIZE_RATIO_16_9},
    {  384,  384, SIZE_RATIO_1_1},
    {    0,    0, SIZE_RATIO_1_1}
};

static int IMX230_VIDEO_LIST[][SIZE_OF_RESOLUTION] =
{
    { 3840, 2160, SIZE_RATIO_16_9},  // M
    { 1920, 1080, SIZE_RATIO_16_9},  // M
    { 1280,  720, SIZE_RATIO_16_9},  // M
    {  720,  480, SIZE_RATIO_3_2},   // M
    {  640,  480, SIZE_RATIO_4_3},   // M
    {  320,  240, SIZE_RATIO_4_3},   // M
    {  176,  144, SIZE_RATIO_11_9},  // M
};

static int IMX230_FPS_RANGE_LIST[][2] =
{
    {  15000,  15000},
    {  15000,  24000},
    {  24000,  24000},
    {  15000,  30000},
    {  24000,  30000},
    {  30000,  30000},
};

static int IMX230_HIDDEN_FPS_RANGE_LIST[][2] =
{
    {  30000,  60000},
    {  60000,  60000},
    {  60000, 120000},
    { 120000, 120000},
};
#endif

