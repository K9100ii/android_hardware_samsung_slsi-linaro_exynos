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

#ifndef EXYNOS_CAMERA_LUT_OV5670_H
#define EXYNOS_CAMERA_LUT_OV5670_H

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
   -----------------------------*/

static int PREVIEW_SIZE_LUT_OV5670[][SIZE_OF_LUT] =
{
    /* Binning     = OFF
       BNS ratio = 1.0
       BDS         = 1080p */

    /* 16:9 (Single, Dual) */
#if 0 //temp_blocking_150701
    { SIZE_RATIO_16_9,
        (2576 + 16),(1448 + 10),    /* [sensor ] */
        2592        , 1458        ,    /* [bns    ] */
        2576        , 1448        ,    /* [bcrop  ] */
        1920        , 1080        ,    /* [bds    ] */
        1920        , 1080        ,    /* [target ] */
    },
#else
    { SIZE_RATIO_16_9,
        (2560 + 16),(1440 + 10),    /* [sensor ] */
        2576        , 1450        ,    /* [bns    ] */
        2560        , 1440        ,    /* [bcrop  ] */
        1920        , 1080        ,    /* [bds    ] */
        1920        , 1080        ,    /* [target ] */
    },
#endif
    /* 4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
        (2576 + 16),(1934 + 10),    /* [sensor ] */
        2592        , 1944        ,    /* [bns    ] */
        2576        , 1934        ,    /* [bcrop  ] */
        1440        , 1080        ,    /* [bds    ] */
        1440        , 1080        ,    /* [target ] */
    },
    /* 1:1 (Single, Dual) -- Not used */
    { SIZE_RATIO_1_1,
        (2576 + 16),(1934 + 10),    /* [sensor ] */
        2592        , 1944        ,    /* [bns    ] */
        1448        , 1448        ,    /* [bcrop  ] */
        1088        , 1088        ,    /* [bds    ] */
        1088        , 1080        ,    /* [target ] */
    },
    /* 3:2 (Single, Dual) -- Not used */
    { SIZE_RATIO_3_2,
        (2576 + 16),(1934 + 10),    /* [sensor ] */
        2592        , 1944        ,    /* [bns    ] */
        2172        , 1448        ,    /* [bcrop  ] */
        1616        , 1080        ,    /* [bds    ] */
        1616        , 1080        ,    /* [target ] */
    },
    /* 5:4 (Single, Dual) -- Not used */
    { SIZE_RATIO_5_4,
        (2576 + 16),(1934 + 10),    /* [sensor ] */
        2592        , 1944        ,    /* [bns    ] */
        1810        , 1448        ,    /* [bcrop  ] */
        1344        , 1080        ,    /* [bds    ] */
        1344        , 1080        ,    /* [target ] */
    },
    /* 5:3 (Single, Dual) -- Not used */
    { SIZE_RATIO_5_3,
        (2576 + 16),(1934 + 10),    /* [sensor ] */
        2592        , 1944        ,    /* [bns    ] */
        2414        , 1448        ,    /* [bcrop  ] */
        1792        , 1080        ,    /* [bds    ] */
        1792        , 1080        ,    /* [target ] */
    },
    /* 11:9 (Single, Dual) -- Not used */
    { SIZE_RATIO_11_9,
        (2576 + 16),(1934 + 10),    /* [sensor ] */
        2592        , 1944        ,    /* [bns    ] */
        1770        , 1448        ,    /* [bcrop  ] */
        1320        , 1080        ,    /* [bds    ] */
        1320        , 1080        ,    /* [target ] */
    }
};

static int PICTURE_SIZE_LUT_OV5670[][SIZE_OF_LUT] =
{
    /* Binning   = OFF
       BNS ratio = 1.0
       BDS       = OFF */

    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
        (2560 + 16),(1440 + 10),    /* [sensor ] */
        2576        , 1450        ,    /* [bns    ] */
        2560        , 1440        ,    /* [bcrop  ] */
        2560        , 1440        ,    /* [bds    ] */
        2560        , 1440        ,    /* [target ] */
    },
    /* 4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
        (2576 + 16),(1934 + 10),   /* [sensor ] */
        2592      , 1944      ,   /* [bns    ] */
        2576      , 1934      ,   /* [bcrop  ] */
        2576      , 1934      ,   /* [bds    ] */
        2576      , 1934      ,   /* [target ] */
    },
    /* 1:1 (Single, Dual) -- Not used */
    { SIZE_RATIO_1_1,
        (2576 + 16),(1934 + 10),    /* [sensor ] */
        2592        , 1944        ,    /* [bns    ] */
        1932        , 1932        ,    /* [bcrop  ] */
        1932        , 1932        ,    /* [bds    ] */
        1932        , 1932        ,    /* [target ] */
    },
    /* 3:2 (Single, Dual) -- Not used */
    { SIZE_RATIO_3_2,
        (2576 + 16),(1934 + 10),    /* [sensor ] */
        2592        , 1944        ,    /* [bns    ] */
        2576        , 1716        ,    /* [bcrop  ] */
        2576        , 1716        ,    /* [bds    ] */
        2576        , 1716        ,    /* [target ] */
    },
    /* 5:4 (Single, Dual) -- Not used */
    { SIZE_RATIO_5_4,
        (2576 + 16),(1934 + 10),    /* [sensor ] */
        2592        , 1944        ,    /* [bns    ] */
        2416        , 1934        ,    /* [bcrop  ] */
        2416        , 1934        ,    /* [bds    ] */
        2416        , 1934        ,    /* [target ] */
    },
    /* 5:3 (Single, Dual) -- Not used */
    { SIZE_RATIO_5_3,
        (2576 + 16),(1934 + 10),    /* [sensor ] */
        2592        , 1944        ,    /* [bns    ] */
        2576        , 1544        ,    /* [bcrop  ] */
        2576        , 1544        ,    /* [bds    ] */
        2576        , 1544        ,    /* [target ] */
    },
    /* 11:9 (Single, Dual) -- Not used */
    { SIZE_RATIO_11_9,
        (2576 + 16),(1934 + 10),    /* [sensor ] */
        2592        , 1944        ,    /* [bns    ] */
        2364        , 1934        ,    /* [bcrop  ] */
        2364        , 1934        ,    /* [bds    ] */
        2364        , 1934        ,    /* [target ] */
    }

};

static int VIDEO_SIZE_LUT_OV5670[][SIZE_OF_LUT] =
{
    /* Binning     = OFF
       BNS ratio = 1.0
       BDS         = 1080p */

    /* 16:9 (Single, Dual) */

    { SIZE_RATIO_16_9,
        (2560 + 16),(1440 + 10),    /* [sensor ] */
        2576        , 1450        ,    /* [bns    ] */
        2560        , 1440        ,    /* [bcrop  ] */
        1920        , 1080        ,    /* [bds    ] */
        1920        , 1080        ,    /* [target ] */
    },

    /* 4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
        (2576 + 16),(1934 + 10),    /* [sensor ] */
        2592        , 1944        ,    /* [bns    ] */
        1932        , 1448        ,    /* [bcrop  ] */
        1440        , 1080        ,    /* [bds    ] */
        1440        , 1080        ,    /* [target ] */
    },
    /* 1:1 (Single, Dual) -- Not used */
    { SIZE_RATIO_1_1,
        (2576 + 16),(1934 + 10),    /* [sensor ] */
        2592        , 1944        ,    /* [bns    ] */
        1448        , 1448        ,    /* [bcrop  ] */
        1088        , 1088        ,    /* [bds    ] */
        1088        , 1080        ,    /* [target ] */
    },
    /* 3:2 (Single, Dual) -- Not used */
    { SIZE_RATIO_3_2,
        (2576 + 16),(1934 + 10),    /* [sensor ] */
        2592        , 1944        ,    /* [bns    ] */
        2172        , 1448        ,    /* [bcrop  ] */
        1616        , 1080        ,    /* [bds    ] */
        1616        , 1080        ,    /* [target ] */
    },
    /* 5:4 (Single, Dual) -- Not used */
    { SIZE_RATIO_5_4,
        (2576 + 16),(1934 + 10),    /* [sensor ] */
        2592        , 1944        ,    /* [bns    ] */
        1810        , 1448        ,    /* [bcrop  ] */
        1344        , 1080        ,    /* [bds    ] */
        1344        , 1080        ,    /* [target ] */
    },
    /* 5:3 (Single, Dual) -- Not used */
    { SIZE_RATIO_5_3,
        (2576 + 16),(1934 + 10),    /* [sensor ] */
        2592        , 1944        ,    /* [bns    ] */
        2414        , 1448        ,    /* [bcrop  ] */
        1792        , 1080        ,    /* [bds    ] */
        1792        , 1080        ,    /* [target ] */
    },
    /* 11:9 (Single, Dual) -- Not used */
    { SIZE_RATIO_11_9,
        (2576 + 16),(1934 + 10),    /* [sensor ] */
        2592        , 1944        ,    /* [bns    ] */
        1770        , 1448        ,    /* [bcrop  ] */
        1320        , 1080        ,    /* [bds    ] */
        1320        , 1080        ,    /* [target ] */
    }
};

static int VIDEO_SIZE_LUT_OV5670_QHD[][SIZE_OF_LUT] =
{
    /* Binning   = OFF
       BNS ratio = 1.0
       BDS       = OFF */

    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
        (2560 + 16),(1440 + 10),    /* [sensor ] */
        2576      , 1450        ,    /* [bns    ] */
        2560        , 1440        ,    /* [bcrop  ] */
        2560        , 1440        ,    /* [bds    ] */
        1920        , 1080        ,    /* [target ] */
    },
    /* 4:3 (Single, Dual) -- Not used */
    { SIZE_RATIO_4_3,
        (2560 + 16),(1440 + 10),    /* [sensor ] */
        2576      , 1450        ,    /* [bns    ] */
        1920        , 1440        ,    /* [bcrop  ] */
        1920        , 1440        ,    /* [bds    ] */
        1440        , 1080        ,    /* [target ] */
    },
    /* 1:1 (Single, Dual) -- Not used */
    { SIZE_RATIO_1_1,
        (2560 + 16),(1440 + 10),    /* [sensor ] */
        2576        , 1450        ,    /* [bns    ] */
        1440        , 1440        ,    /* [bcrop  ] */
        1440        , 1440        ,    /* [bds    ] */
        1088        , 1080        ,    /* [target ] */
    },
    /* 3:2 (Single, Dual) -- Not used */
    { SIZE_RATIO_3_2,
        (2560 + 16),(1440 + 10),    /* [sensor ] */
        2576        , 1450        ,    /* [bns    ] */
        2160        , 1440        ,    /* [bcrop  ] */
        2160        , 1440        ,    /* [bds    ] */
        1616        , 1080        ,    /* [target ] */
    },
    /* 5:4 (Single, Dual) -- Not used */
    { SIZE_RATIO_5_4,
        (2560 + 16),(1440 + 10),    /* [sensor ] */
        2576        , 1450        ,    /* [bns    ] */
        1800        , 1440        ,    /* [bcrop  ] */
        1800        , 1440        ,    /* [bds    ] */
        1344        , 1080        ,    /* [target ] */
    },
    /* 5:3 (Single, Dual) -- Not used */
    { SIZE_RATIO_5_3,
        (2560 + 16),(1440 + 10),    /* [sensor ] */
        2576        , 1450        ,    /* [bns    ] */
        2400        , 1440        ,    /* [bcrop  ] */
        2400        , 1440      ,   /* [bds     ] */
        1792        , 1080      ,   /* [target ] */
    },
    /* 11:9 (Single, Dual) -- Not used */
    { SIZE_RATIO_11_9,
        (2560 + 16),(1440 + 10),    /* [sensor ] */
        2576        , 1450        ,    /* [bns    ] */
        1760        , 1440        ,    /* [bcrop  ] */
        1760        , 1440        ,    /* [bds    ] */
        1320        , 1080        ,    /* [target ] */
    }

};

static int VIDEO_SIZE_LUT_OV5670_VGA_BINNING2[][SIZE_OF_LUT] =
{
    /* Binning   = OFF
       BNS ratio = 1.0
       BDS       = OFF */

    /* 16:9 (Single) -- Not used */
    { SIZE_RATIO_16_9,
        (1280 + 16),( 720 + 10),   /* [sensor ] *//* Sensor binning ratio = 2 */
        1296        ,  730       ,   /* [bns     ] */
        1280        ,  720       ,   /* [bcrop  ] */
        1280        ,  720       ,   /* [bds      ] */
        1280        ,  720       ,   /* [target ] */
    },
    /* 4:3 (Single) -- Not used */
    { SIZE_RATIO_4_3,
        (1280 + 16),( 720 + 10),   /* [sensor ] *//* Sensor binning ratio = 2 */
        1296        ,  730       ,   /* [bns     ] */
        960        ,  720       ,   /* [bcrop  ] */
        960        ,  720       ,   /* [bds      ] */
        640        ,  480       ,   /* [target ] */
    },
    /* 1:1 (Single) -- Not used */
    { SIZE_RATIO_1_1,
        (1280 + 16),( 720 + 10),   /* [sensor ] *//* Sensor binning ratio = 2 */
        1296        ,  730       ,   /* [bns     ] */
        720        ,  720       ,   /* [bcrop  ] */
        720        ,  720       ,   /* [bds      ] */
        480        ,  480       ,   /* [target ] */
    },
    /* 3:2 (Single) */
    { SIZE_RATIO_3_2,
        (1280 + 16),( 720 + 10),   /* [sensor ] *//* Sensor binning ratio = 2 */
        1296        ,  730        ,   /* [bns      ] */
        1080      ,  720      ,   /* [bcrop  ] */
        1080      ,  720      ,   /* [bds    ] */
        1080      ,  720      ,   /* [target ] */
    },
    /* 5:4 (Single) -- Not used */
    { SIZE_RATIO_5_4,
        (1280 + 16),( 720 + 10),   /* [sensor ] *//* Sensor binning ratio = 2 */
        1296        ,  730        ,   /* [bns      ] */
        900      ,  720      ,   /* [bcrop  ] */
        900      ,  720      ,   /* [bds    ] */
        600      ,  480      ,   /* [target ] */
    },
    /* 5:3 (Single) -- Not used */
    { SIZE_RATIO_5_3,
        (1280 + 16),( 720 + 10),    /* [sensor ] *//* Sensor binning ratio = 2 */
        1296        ,  730        ,    /* [bns   ] */
        1200        ,  720        ,    /* [bcrop  ] */
        1200        ,  720        ,    /* [bds    ] */
        800        ,  480        ,    /* [target ] */
    },
    /* 11:9 (Single) -- Not used */
    { SIZE_RATIO_11_9,
        (1280 + 16),( 720 + 10),    /* [sensor ] *//* Sensor binning ratio = 2 */
        1296        ,  730        ,    /* [bns   ] */
        880        ,  720        ,    /* [bcrop  ] */
        880        ,  720        ,    /* [bds    ] */
        588        ,  480        ,    /* [target ] */
    }
};

static int VIDEO_SIZE_LUT_OV5670_VGA_BINNING4[][SIZE_OF_LUT] =
{
    /* Binning   = OFF
       BNS ratio = 1.0
       BDS       = OFF */

    /* 16:9 (Single) -- Not used */
    { SIZE_RATIO_16_9,
        ( 632 + 16),( 476 + 10),    /* [sensor ] *//* Sensor binning ratio = 4 */
        648        ,  486        ,    /* [bns    ] */
        632        ,  356        ,    /* [bcrop  ] */
        632        ,  356        ,    /* [bds    ] */
        632        ,  356        ,    /* [target ] */
    },
    /* 4:3 (Single) */
    { SIZE_RATIO_4_3,
        ( 632 + 16),( 476 + 10),    /* [sensor ] *//* Sensor binning ratio = 4 */
        648        ,  486        ,    /* [bns    ] */
        634        ,  476        ,    /* [bcrop  ] */
        634        ,  476        ,    /* [bds    ] */
        634        ,  476        ,    /* [target ] */
    },
    /* 1:1 (Single) -- Not used */
    { SIZE_RATIO_1_1,
        ( 632 + 16),( 476 + 10),    /* [sensor ] *//* Sensor binning ratio = 4 */
        648        ,  486        ,    /* [bns    ] */
        356        ,  356        ,    /* [bcrop  ] */
        356        ,  356        ,    /* [bds    ] */
        356        ,  356        ,    /* [target ] */
    },
    /* 3:2 (Single) -- Not used */
    { SIZE_RATIO_3_2,
        ( 632 + 16),( 476 + 10),    /* [sensor ] *//* Sensor binning ratio = 4 */
        648        ,  486        ,    /* [bns    ] */
        534        ,  356        ,    /* [bcrop  ] */
        534        ,  356        ,    /* [bds    ] */
        534        ,  356        ,    /* [target ] */
    },
    /* 5:4 (Single) -- Not used */
    { SIZE_RATIO_5_4,
        ( 632 + 16),( 476 + 10),    /* [sensor ] *//* Sensor binning ratio = 4 */
        648        ,  486        ,    /* [bns    ] */
        445        ,  356        ,    /* [bcrop  ] */
        445        ,  356        ,    /* [bds    ] */
        445        ,  356        ,    /* [target ] */
    },
    /* 5:3 (Single) -- Not used */
    { SIZE_RATIO_5_3,
        ( 632 + 16),( 476 + 10),    /* [sensor ] *//* Sensor binning ratio = 4 */
        648        ,  486        ,    /* [bns    ] */
        594        ,  356        ,    /* [bcrop  ] */
        594        ,  356        ,    /* [bds    ] */
        594        ,  356        ,    /* [target ] */
    },
    /* 11:9 (Single) -- Not used */
    { SIZE_RATIO_11_9,
        ( 632 + 16),( 476 + 10),    /* [sensor ] *//* Sensor binning ratio = 4 */
        648        ,  486        ,    /* [bns    ] */
        582        ,  476        ,    /* [bcrop  ] */
        582        ,  476        ,    /* [bds    ] */
        582        ,  476        ,    /* [target ] */
    }

};

static int OV5670_PREVIEW_LIST[][SIZE_OF_RESOLUTION] =
{
    { 1920, 1080, SIZE_RATIO_16_9}, // M
    { 1440, 1080, SIZE_RATIO_4_3},
    { 1280,  720, SIZE_RATIO_16_9}, // M
    {  720,  480, SIZE_RATIO_3_2}, // M
    {  640,  480, SIZE_RATIO_4_3}, // M
    {  176,  144, SIZE_RATIO_11_9}, // M
};

static int OV5670_HIDDEN_PREVIEW_LIST[][SIZE_OF_RESOLUTION] =
{
#if !(defined(CAMERA_LCD_SIZE) && (CAMERA_LCD_SIZE >= LCD_SIZE_1920_1080))
    { 1920, 1080, SIZE_RATIO_16_9},
    { 1440, 1080, SIZE_RATIO_4_3},
    { 1088, 1080, SIZE_RATIO_1_1},
#endif
    { 3840, 2160, SIZE_RATIO_16_9},
    { 1600, 1200, SIZE_RATIO_4_3},
    { 1280,  960, SIZE_RATIO_4_3},
    { 1056,  864, SIZE_RATIO_11_9},
    {  528,  432, SIZE_RATIO_11_9},
    {  800,  480, SIZE_RATIO_5_3},
    {  480,  320, SIZE_RATIO_3_2},
    {  480,  270, SIZE_RATIO_16_9},
};

static int OV5670_PICTURE_LIST[][SIZE_OF_RESOLUTION] =
{
    { 2592, 1944, SIZE_RATIO_4_3}, //M
    { 2592, 1458, SIZE_RATIO_16_9}, //M
    { 1920, 1080, SIZE_RATIO_16_9}, //M
    {  640,  480, SIZE_RATIO_4_3}, // M
};

static int OV5670_HIDDEN_PICTURE_LIST[][SIZE_OF_RESOLUTION] =
{
    { 2560, 1440, SIZE_RATIO_16_9}, // M
    { 1920, 1080, SIZE_RATIO_16_9}, // M
    { 1280,  720, SIZE_RATIO_16_9}, // M
    {  880,  720, SIZE_RATIO_11_9},  // M
    {  720,  480, SIZE_RATIO_3_2}, // M
    {  640,  480, SIZE_RATIO_4_3}, // M
    {  320,  240, SIZE_RATIO_4_3},   // M
    {  176,  144, SIZE_RATIO_11_9}, // M

};

static int OV5670_THUMBNAIL_LIST[][SIZE_OF_RESOLUTION] =
{
    {  512,  384, SIZE_RATIO_4_3},
    {  512,  288, SIZE_RATIO_16_9},
    {  384,  384, SIZE_RATIO_1_1},
    {  320,  240, SIZE_RATIO_4_3},
    {    0,    0, SIZE_RATIO_1_1},
};

static int OV5670_VIDEO_LIST[][SIZE_OF_RESOLUTION] =
{
    { 1920, 1080, SIZE_RATIO_16_9}, // M
    { 1280,  720, SIZE_RATIO_16_9}, // M
    {  720,  480, SIZE_RATIO_3_2}, // M
    {  640,  480, SIZE_RATIO_4_3}, // M
    {  320,  240, SIZE_RATIO_4_3},   // M
    {  176,  144, SIZE_RATIO_11_9}, // M
};

static int OV5670_HIDDEN_VIDEO_LIST[][SIZE_OF_RESOLUTION] =
{
    { 1440, 1080, SIZE_RATIO_4_3},
    {  960,  720, SIZE_RATIO_4_3},
    {  800,  450, SIZE_RATIO_16_9},
    {  480,  320, SIZE_RATIO_3_2},
    {  352,  288, SIZE_RATIO_11_9},
};
/*
static int OV5670_HIDDEN_VIDEO_LIST[][SIZE_OF_RESOLUTION] =
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
    {  176,  144, SIZE_RATIO_11_9},
};
*/

static int OV5670_FPS_RANGE_LIST[][2] =
{
    {   5000,   5000},
    {   7000,   7000},
    {  15000,  15000},
    {  24000,  24000},
    {   4000,  30000},
    {   8000,  30000},
    {  10000,  30000},
    {  15000,  30000},
    {  24000,  30000},
    {  30000,  30000},
};

static int OV5670_HIDDEN_FPS_RANGE_LIST[][2] =
{
    {  30000,  60000},
    {  60000,  60000},
    {  60000, 120000},
    { 120000, 120000},
};

#endif
