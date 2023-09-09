/*
 * Copyright (C) 2017, Samsung Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef PLUG_IN_COMMON_H
#define PLUG_IN_COMMON_H

#include <map>

#include "PlugInCommonExt.h"

namespace PLUGIN {
    enum MODE {
        BASE,
        BOKEH = 1,
        ZOOM = 2,
        MAX
    };
};

/*
 * define
 */
#define PLUGIN_MAX_BUF 32
#define PLUGIN_MAX_PLANE 8

/* To access Array_buf_rect_t */
#define PLUGIN_ARRAY_RECT_X 0
#define PLUGIN_ARRAY_RECT_Y 1
#define PLUGIN_ARRAY_RECT_W 2
#define PLUGIN_ARRAY_RECT_H 3
#define PLUGIN_ARRAY_RECT_FULL_W 4
#define PLUGIN_ARRAY_RECT_FULL_H 5
#define PLUGIN_ARRAY_RECT_MAX 6

/*
 * typedef
 */
typedef int64_t              Map_data_t;
typedef std::map<int, Map_data_t> Map_t;

typedef bool                 Data_bool_t;
typedef char                 Data_char_t;
typedef int32_t              Data_int32_t;
typedef uint32_t             Data_uint32_t;
typedef int64_t              Data_int64_t;
typedef uint64_t             Data_uint64_t;
typedef float                Data_float_t;

typedef float *              Pointer_float_t;
typedef double *             Pointer_double_t;
typedef const char *         Pointer_const_char_t;
typedef char *               Pointer_char_t;
typedef unsigned char *      Pointer_uchar_t;
typedef int *                Pointer_int_t;
typedef void *               Pointer_void_t;

typedef char *               Single_buf_t;
typedef int *                Array_buf_t;
typedef int *                Array_plane_t;
typedef int (*               Array_buf_plane_t)[PLUGIN_MAX_PLANE];
typedef int (*               Array_buf_rect_t)[PLUGIN_ARRAY_RECT_MAX];
typedef char **              Array_buf_addr_t;
typedef float **             Array_float_addr_t;
typedef int **               Array_pointer_int_t;

/*
 * macro
 */
#define MAKE_VERSION(major, minor) (((major) << 16) | (minor))
#define GET_MAJOR_VERSION(version) ((version) >> 16)
#define GET_MINOR_VERSION(version) ((version) & 0xFFFF)
#define PLUGIN_LOCATION_ID         "(%s[%d]):"
#define PLUGIN_LOCATION_ID_PARM    __FUNCTION__, __LINE__
#define PLUGIN_LOG_ASSERT(fmt, ...) \
        android_printAssert(NULL, LOG_TAG, "ASSERT" PLUGIN_LOCATION_ID fmt, PLUGIN_LOCATION_ID_PARM, ##__VA_ARGS__);
#define PLUGIN_LOGD(fmt, ...) \
        ALOGD("DEBUG" PLUGIN_LOCATION_ID fmt, PLUGIN_LOCATION_ID_PARM, ##__VA_ARGS__)
#define PLUGIN_LOGV(fmt, ...) \
        ALOGV("VERB" PLUGIN_LOCATION_ID fmt, PLUGIN_LOCATION_ID_PARM, ##__VA_ARGS__)
#define PLUGIN_LOGW(fmt, ...) \
        ALOGW("WARN" PLUGIN_LOCATION_ID fmt, PLUGIN_LOCATION_ID_PARM, ##__VA_ARGS__)
#define PLUGIN_LOGE(fmt, ...) \
        ALOGE("ERR" PLUGIN_LOCATION_ID fmt, PLUGIN_LOCATION_ID_PARM, ##__VA_ARGS__)
#define PLUGIN_LOGI(fmt, ...) \
        ALOGI("INFO" PLUGIN_LOCATION_ID fmt, PLUGIN_LOCATION_ID_PARM, ##__VA_ARGS__)

/*
 * Default KEY (1 ~ 1000)
 * PIPE -> Converter
 */
#define PLUGIN_CONVERT_TYPE            1    /* int                     / int                     */
enum PLUGIN_CONVERT_TYPE_T {
    PLUGIN_CONVERT_BASE,
    PLUGIN_CONVERT_SETUP_BEFORE,
    PLUGIN_CONVERT_SETUP_AFTER,
    PLUGIN_CONVERT_PROCESS_BEFORE,
    PLUGIN_CONVERT_PROCESS_AFTER,
    PLUGIN_CONVERT_MAX,
};
#define PLUGIN_CONVERT_FRAME        2   /* ExynosCameraFrame     / ExynosCameraFrame     */
#define PLUGIN_CONVERT_PARAMETER    3   /* ExynosCameraParamerer / ExynosCameraParamerer */
#define PLUGIN_CONVERT_CONFIGURATIONS    4   /* ExynosCameraConfigurations / ExynosCameraConfigurations */

/*
 * Source Buffer Info (1001 ~ 2000)            Map Data Type    / Actual Type
 */
#define PLUGIN_SRC_BUF_CNT          1001    /* Data_int32_t     / int            */
#define PLUGIN_SRC_BUF_PLANE_CNT    1002    /* Array_plane_t    / int[PLUGIN_MAX_BUF]    */
#define PLUGIN_SRC_BUF_SIZE         1003    /* Array_buf_plane_t/ int[PLUGIN_MAX_BUF][PLUGIN_MAX_PLANE]    */
#define PLUGIN_SRC_BUF_RECT         1004    /* Array_buf_rect_t / int[PLUGIN_MAX_BUF][4] */
#define PLUGIN_SRC_BUF_WSTRIDE      1005    /* Array_buf_t      / int[PLUGIN_MAX_BUF] */
#define PLUGIN_SRC_BUF_HSTRIDE      1006    /* Array_buf_t      / int[PLUGIN_MAX_BUF] */
#define PLUGIN_SRC_BUF_V4L2_FORMAT  1007    /* Array_buf_t      / int[PLUGIN_MAX_BUF] */
#define PLUGIN_SRC_BUF_HAL_FORMAT   1008    /* Array_buf_t      / int[PLUGIN_MAX_BUF] */
#define PLUGIN_SRC_BUF_ROTATION     1009    /* Array_buf_t      / int[PLUGIN_MAX_BUF] */
#define PLUGIN_SRC_BUF_FLIPH        1010    /* Array_buf_t      / int[PLUGIN_MAX_BUF] */
#define PLUGIN_SRC_BUF_FLIPV        1011    /* Array_buf_t      / int[PLUGIN_MAX_BUF] */
#define PLUGIN_SRC_FRAMECOUNT       1012    /* Data_int32_t     / int                 */
#define PLUGIN_SRC_FRAME_STATE      1013    /* Data_int32_t     / int                 */
#define PLUGIN_SRC_FRAME_TYPE       1014    /* Data_int32_t     / int                 */
#define PLUGIN_SRC_BUF_INDEX        1015    /* Data_int32_t     / int                 */
#define PLUGIN_SRC_BUF_FD           1016    /* Array_buf_plane_t/ int[PLUGIN_MAX_BUF][PLUGIN_MAX_PLANE]    */
#define PLUGIN_SRC_BUF_USED         1017    /* Data_int32_t     / int                 */
#define PLUGIN_MASTER_FACE_RECT     1018    /* Array_buf_rect_t / int[PLUGIN_MAX_BUF][4] */
#define PLUGIN_SLAVE_FACE_RECT      1019    /* Array_buf_rect_t / int[PLUGIN_MAX_BUF][4] */
#define PLUGIN_RESULT_FACE_RECT     1020    /* Array_buf_rect_t / int[PLUGIN_MAX_BUF][4] */
#define PLUGIN_FACE_NUM             1021    /* Data_int32_t     / int                 */

/*
 * Source Buffer Address (2001 ~ 3000)
 */
#define PLUGIN_SRC_BUF_1        2001    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_SRC_BUF_2        2002    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_SRC_BUF_3        2003    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_SRC_BUF_4        2004    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_SRC_BUF_5        2005    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_SRC_BUF_6        2006    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_SRC_BUF_7        2007    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_SRC_BUF_8        2008    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_SRC_BUF_9        2009    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_SRC_BUF_10       2010    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_SRC_BUF_11       2011    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_SRC_BUF_12       2012    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_SRC_BUF_13       2013    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_SRC_BUF_14       2014    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_SRC_BUF_15       2015    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_SRC_BUF_16       2016    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_SRC_BUF_17       2017    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_SRC_BUF_18       2018    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_SRC_BUF_19       2019    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_SRC_BUF_20       2020    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_SRC_BUF_21       2021    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_SRC_BUF_22       2022    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_SRC_BUF_23       2023    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_SRC_BUF_24       2024    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_SRC_BUF_25       2025    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_SRC_BUF_26       2026    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_SRC_BUF_27       2027    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_SRC_BUF_28       2028    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_SRC_BUF_29       2029    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_SRC_BUF_30       2030    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_SRC_BUF_31       2031    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_SRC_BUF_32       2032    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */

/*
 * Destination Buffer Address (3001 ~ 4000)
 */
#define PLUGIN_DST_BUF_CNT          3001    /* Data_int32_t     / int            */
#define PLUGIN_DST_BUF_PLANE_CNT    3002    /* Array_plane_t    / int[PLUGIN_MAX_BUF]    */
#define PLUGIN_DST_BUF_SIZE         3003    /* Array_buf_plane_t/ int[PLUGIN_MAX_BUF][PLUGIN_MAX_PLANE]    */
#define PLUGIN_DST_BUF_RECT         3004    /* Array_buf_rect_t / int[PLUGIN_MAX_BUF][4] */
#define PLUGIN_DST_BUF_WSTRIDE      3005    /* Array_buf_t      / int[PLUGIN_MAX_BUF] */
#define PLUGIN_DST_BUF_HSTRIDE      3006    /* Array_buf_t      / int[PLUGIN_MAX_BUF] */
#define PLUGIN_DST_BUF_V4L2_FORMAT  3007    /* Array_buf_t      / int[PLUGIN_MAX_BUF] */
#define PLUGIN_DST_BUF_HAL_FORMAT   3008    /* Array_buf_t      / int[PLUGIN_MAX_BUF] */
#define PLUGIN_DST_BUF_ROTATION     3009    /* Array_buf_t      / int[PLUGIN_MAX_BUF] */
#define PLUGIN_DST_BUF_FLIPH        3010    /* Array_buf_t      / int[PLUGIN_MAX_BUF] */
#define PLUGIN_DST_BUF_FLIPV        3011    /* Array_buf_t      / int[PLUGIN_MAX_BUF] */
#define PLUGIN_DST_BUF_INDEX        3012    /* Data_int32_t     / int            */
#define PLUGIN_DST_BUF_FD           3013    /* Array_buf_plane_t/ int[PLUGIN_MAX_BUF][PLUGIN_MAX_PLANE]    */
#define PLUGIN_DST_BUF_VALID        3014    /* Data_int32_t     / int                 */

/*
 * Destination Buffer Address (4001 ~ 5000)
 */
#define PLUGIN_DST_BUF_1        4001    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_DST_BUF_2        4002    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_DST_BUF_3        4003    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_DST_BUF_4        4004    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_DST_BUF_5        4005    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_DST_BUF_6        4006    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_DST_BUF_7        4007    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_DST_BUF_8        4008    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_DST_BUF_9        4009    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_DST_BUF_10       4010    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_DST_BUF_11       4011    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_DST_BUF_12       4012    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_DST_BUF_13       4013    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_DST_BUF_14       4014    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_DST_BUF_15       4015    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_DST_BUF_16       4016    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_DST_BUF_17       4017    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_DST_BUF_18       4018    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_DST_BUF_19       4019    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_DST_BUF_20       4020    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_DST_BUF_21       4021    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_DST_BUF_22       4022    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_DST_BUF_23       4023    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_DST_BUF_24       4024    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_DST_BUF_25       4025    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_DST_BUF_26       4026    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_DST_BUF_27       4027    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_DST_BUF_28       4028    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_DST_BUF_29       4029    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_DST_BUF_30       4030    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_DST_BUF_31       4031    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_DST_BUF_32       4032    /* Array_buf_addr_t / char *[PLUGIN_MAX_PLANE] */

/*
 * ETC Info (5001 ~ 0xF0000000)
 */
#define PLUGIN_CONFIG_FILE_PATH             5001    /* Pointer_const_char_t/ const char * */
#define PLUGIN_CALB_FILE_PATH               5002    /* Pointer_const_char_t/ const char * */
#define PLUGIN_ZOOM_RATIO_WIDE              5003    /* Pointer_float_t     / float *      */
#define PLUGIN_ZOOM_RATIO_TELE              5004    /* Pointer_float_t     / float *      */
#define PLUGIN_LENS_POSITION_WIDE           5005    /* Data_int32_t        / int          */
#define PLUGIN_LENS_POSITION_TELE           5006    /* Data_int32_t        / int          */
#define PLUGIN_GLOBAL_DISP_X                5007    /* Data_int32_t        / int          */
#define PLUGIN_GLOBAL_DISP_Y                5008    /* Data_int32_t        / int          */
#define PLUGIN_FUSION_ENABLE                5009    /* Data_bool_t         / bool        */
#define PLUGIN_ROI2D                        5010    /* Not defined         / Not defined  */
#define PLUGIN_RECTIFICATION_RIGHT_WRAP     5011    /* Data_bool_t         / bool         */
#define PLUGIN_RECTIFICATION_DISP_MARGIN    5012    /* Data_int32_t        / int          */
#define PLUGIN_RECTIFICATION_IMG_SHIFT      5013    /* Data_int32_t        / int          */
#define PLUGIN_RECTIFICATION_MANUAL_CAL_Y   5014    /* Data_int32_t        / int          */
/* LLS Lib Start */
#define PLUGIN_LLS_INTENT                   5100    /* Data_int32_t        / int          */
enum PLUGIN_LLS_INTENT_ENUM {
    NONE,
    CAPTURE_START,      /* capture first frame */
    CAPTURE_PROCESS,    /* capture not first frame */
    PREVIEW_START,      /* preview first frame */
    PREVIEW_PROCESS,    /* preview not first frame */
};
#define PLUGIN_ME_DOWNSCALED_BUF            5101    /* Single_Buf_addr_t   / char *       */
#define PLUGIN_MOTION_VECTOR_BUF            5102    /* Single_Buf_addr_t   / char *       */
#define PLUGIN_CURRENT_PATCH_BUF            5103    /* Single_Buf_addr_t   / char *       */
#define PLUGIN_ME_FRAMECOUNT                5104    /* Data_int32_t        / int          */
#define PLUGIN_TIMESTAMP                    5105    /* Data_int64_t        / int64        */
#define PLUGIN_EXPOSURE_TIME                5106    /* Data_int64_t        / int64        */
#define PLUGIN_ISO                          5107    /* Data_int32_t        / int          */

#ifdef USE_PLUGIN_LLS
#define PLUGIN_MAX_BCROP_X                  5108    /* Data_int32_t        / int          */
#define PLUGIN_MAX_BCROP_Y                  5119    /* Data_int32_t        / int          */
#define PLUGIN_MAX_BCROP_W                  5110    /* Data_int32_t        / int          */
#define PLUGIN_MAX_BCROP_H                  5111    /* Data_int32_t        / int          */
#define PLUGIN_MAX_BCROP_FULLW              5112    /* Data_int32_t        / int          */
#define PLUGIN_MAX_BCROP_FULLH              5113    /* Data_int32_t        / int          */
#define PLUGIN_BAYER_V4L2_FORMAT            5114    /* Data_int32_t        / int          */
/* LLS Lib End */
#endif

#define PLUGIN_MAX_SENSOR_W                 5108    /* Data_int32_t        / int          */
#define PLUGIN_MAX_SENSOR_H                 5109    /* Data_int32_t        / int          */
#define PLUGIN_TIMESTAMPBOOT                5110    /* Data_int32_t        / int          */
#define PLUGIN_FRAME_DURATION               5111    /* Data_int32_t        / int          */
#define PLUGIN_ROLLING_SHUTTER_SKEW         5112    /* Data_int32_t        / int          */
#define PLUGIN_OPTICAL_STABILIZATION_MODE_CTRL   5113    /* Data_int32_t        / int          */
#define PLUGIN_OPTICAL_STABILIZATION_MODE_DM  5114    /* Data_int32_t        / int          */
#define PLUGIN_GYRO_DATA                    5115    /*Array_pointer_gyro_data_t  /plugin_gyro_data_t*    */
#define PLUGIN_GYRO_DATA_SIZE                    5116    /* Data_int32_t        / int          */


#define PLUGIN_WIDE_FULLSIZE_W              6001    /* Data_int32_t        / int          */
#define PLUGIN_WIDE_FULLSIZE_H              6002    /* Data_int32_t        / int          */
#define PLUGIN_TELE_FULLSIZE_W              6003    /* Data_int32_t        / int          */
#define PLUGIN_TELE_FULLSIZE_H              6004    /* Data_int32_t        / int          */
#define PLUGIN_FOCUS_POSTRION_X             6005    /* Data_int32_t        / int          */
#define PLUGIN_FOCUS_POSTRION_Y             6006    /* Data_int32_t        / int          */
#define PLUGIN_AF_STATUS                    6007    /* Array_buf_t         / int          */
#define PLUGIN_ZOOM_RATIO                   6007    /* Pointer_float_t     / int          */
#define PLUGIN_ZOOM_RATIO_LIST_SIZE         6012    /* Array_buf_t         / int          */
#define PLUGIN_ZOOM_RATIO_LIST              6013    /* Array_float_addr_t  / int          */
#define PLUGIN_VIEW_RATIO                   6014    /* Data_float_t        / float        */
#define PLUGIN_IMAGE_RATIO_LIST             6015    /* Array_float_addr_t  / int          */
#define PLUGIN_IMAGE_RATIO                  6016    /* Pointer_float_t     / int          */
#define PLUGIN_NEED_MARGIN_LIST             6017    /* Array_pointer_int_t  / int          */
#define PLUGIN_CAMERA_INDEX                 6018    /* Data_int32_t        / int          */
#define PLUGIN_IMAGE_SHIFT_X                6019    /* Data_int32_t        / int          */
#define PLUGIN_IMAGE_SHIFT_Y                6020    /* Data_int32_t        / int          */
#define PLUGIN_ZOOM_LEVEL                   6021    /* Data_int32_t        / int          */

#define PLUGIN_BOKEH_INTENSITY              6022    /* Data_int32_t        / int          */
#define PLUGIN_BOKEH_TEST                   6023    /* Data_int32_t        / int          */

#define PLUGIN_SYNC_TYPE                    6024    /* Data_int32_t        / int          */
#define PLUGIN_NEED_SWITCH_CAMERA           6025    /* Data_int32_t        / int          */
#define PLUGIN_MASTER_CAMERA_ARC2HAL        6026    /* Data_int32_t        / int          */
#define PLUGIN_MASTER_CAMERA_HAL2ARC        6027    /* Data_int32_t        / int          */
#define PLUGIN_FALLBACK                     6028    /* Data_bool_t         / bool         */

#define PLUGIN_BCROP_RECT                   6028    /* Data_pointer_rect_t         / struct plugin_rect_t */
#define PLUGIN_BUTTON_ZOOM                  6030    /* Data_int32_t        / int          */

#define PLUGIN_CALI_ENABLE                  7001    /* Data_int32_t        / int          */
#define PLUGIN_CALI_RESULT                  7002    /* Data_int32_t        / int          */
#define PLUGIN_VERI_ENABLE                  7003    /* Data_int32_t        / int          */
#define PLUGIN_VERI_RESULT                  7004    /* Data_int32_t        / int          */
#define PLUGIN_CALI_DATA                    7005    /* Array_buf_addr_t    / char *[PLUGIN_MAX_PLANE] */
#define PLUGIN_CALI_SIZE                    7006    /* Data_int32_t        / int          */

/*
 * Query ID (0xF0000001 ~)
 */
#define PLUGIN_VERSION                  0xF0000001    /* Data_int32_t         / int            */
#define PLUGIN_LIB_NAME                 0xF0000002    /* Pointer_const_char_t / const char *   */
#define PLUGIN_BUILD_DATE               0xF0000003    /* Pointer_const_char_t / const char *   */  //use __DATE__
#define PLUGIN_BUILD_TIME               0xF0000004    /* Pointer_const_char_t / const char *   */  //use __TIME__
#define PLUGIN_PLUGIN_CUR_SRC_BUF_CNT   0xF0000005    /* Data_int32_t         / int            */
#define PLUGIN_PLUGIN_CUR_DST_BUF_CNT   0xF0000006    /* Data_int32_t         / int            */
#define PLUGIN_PLUGIN_MAX_SRC_BUF_CNT   0xF0000007    /* Data_int32_t         / int            */
#define PLUGIN_PLUGIN_MAX_DST_BUF_CNT   0xF0000008    /* Data_int32_t         / int            */
#define PLUGIN_PRIVATE_HANDLE           0xF0000009    /* Pointer_void_t       / void *         */

#endif
