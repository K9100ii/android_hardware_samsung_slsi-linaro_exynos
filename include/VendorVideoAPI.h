/*
 *
 * Copyright 2017 Samsung Electronics S.LSI Co. LTD
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

/*
 * @file    VendorVideoAPI.h
 * @author  TaeHwan Kim    (t_h_kim@samsung.com)
 *          ByungGwan Kang (bk0917.kang@samsung.com)
 * @version 1.0
 * @history
 *   2017.06.02 : Create
 */

#ifndef VENDOR_VIDEO_API_H_
#define VENDOR_VIDEO_API_H_

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif
#define MAX_HDR10PLUS_SIZE 1024

typedef enum _ExynosVideoInfoType {
    VIDEO_INFO_TYPE_INVALID            = 0,
    VIDEO_INFO_TYPE_HDR_STATIC         = 0x1 << 0,
    VIDEO_INFO_TYPE_COLOR_ASPECTS      = 0x1 << 1,
    VIDEO_INFO_TYPE_INTERLACED         = 0x1 << 2,
    VIDEO_INFO_TYPE_YSUM_DATA          = 0x1 << 3,
    VIDEO_INFO_TYPE_HDR_DYNAMIC        = 0x1 << 4,
    VIDEO_INFO_TYPE_CHECK_PIXEL_FORMAT = 0x1 << 5,
    VIDEO_INFO_TYPE_GDC_OTF            = 0x1 << 6,
    VIDEO_INFO_TYPE_ROI_INFO           = 0x1 << 7,
    VIDEO_INFO_TYPE_CROP_INFO          = 0x1 << 8,
} ExynosVideoInfoType;

typedef struct _ExynosVideoYSUMData {
    unsigned int high;
    unsigned int low;
} ExynosVideoYSUMData;

typedef struct _ExynosColorAspects {
    int mRange;
    int mPrimaries;
    int mTransfer;
    int mMatrixCoeffs;
} ExynosColorAspects;

typedef struct _ExynosPrimaries {
    unsigned int x;
    unsigned int y;
} ExynosPrimaries;

typedef struct _ExynosType1 {
    ExynosPrimaries mR;
    ExynosPrimaries mG;
    ExynosPrimaries mB;
    ExynosPrimaries mW;
    unsigned int    mMaxDisplayLuminance;
    unsigned int    mMinDisplayLuminance;
    unsigned int    mMaxContentLightLevel;
    unsigned int    mMaxFrameAverageLightLevel;
} ExynosType1;

typedef struct _ExynosHdrStaticInfo {
    int mID;
    union {
        ExynosType1 sType1;
    };
} ExynosHdrStaticInfo;

#ifdef USE_FULL_ST2094_40
#define USE_FULL_ST2094_40_INFO
#endif

typedef struct _ExynosHdrData_ST2094_40 {
    unsigned char  country_code;
    unsigned short provider_code;
    unsigned short provider_oriented_code;

    unsigned char  application_identifier;
    unsigned char  application_version;

#ifdef USE_FULL_ST2094_40
    unsigned char num_windows;

    unsigned short window_upper_left_corner_x[2];
    unsigned short window_upper_left_corner_y[2];
    unsigned short window_lower_right_corner_x[2];
    unsigned short window_lower_right_corner_y[2];
    unsigned short center_of_ellipse_x[2];
    unsigned short center_of_ellipse_y[2];
    unsigned char  rotation_angle[2];
    unsigned short semimajor_axis_internal_ellipse[2];
    unsigned short semimajor_axis_external_ellipse[2];
    unsigned short semiminor_axis_external_ellipse[2];
    unsigned char  overlap_process_option[2];

    unsigned int  targeted_system_display_maximum_luminance;
    unsigned char targeted_system_display_actual_peak_luminance_flag;
    unsigned char num_rows_targeted_system_display_actual_peak_luminance;
    unsigned char num_cols_targeted_system_display_actual_peak_luminance;
    unsigned char targeted_system_display_actual_peak_luminance[25][25];

    unsigned int maxscl[3][3];
    unsigned int average_maxrgb[3];

    unsigned char num_maxrgb_percentiles[3];
    unsigned char maxrgb_percentages[3][15];
    unsigned int  maxrgb_percentiles[3][15];

    unsigned short fraction_bright_pixels[3];

    unsigned char mastering_display_actual_peak_luminance_flag;
    unsigned char num_rows_mastering_display_actual_peak_luminance;
    unsigned char num_cols_mastering_display_actual_peak_luminance;
    unsigned char mastering_display_actual_peak_luminance[25][25];

    struct {
        unsigned char   tone_mapping_flag[3];
        unsigned short  knee_point_x[3];
        unsigned short  knee_point_y[3];
        unsigned char   num_bezier_curve_anchors[3];
        unsigned short  bezier_curve_anchors[3][15];
    } tone_mapping;

    unsigned char color_saturation_mapping_flag[3];
    unsigned char color_saturation_weight[3];
#else
    unsigned int  display_maximum_luminance;

    unsigned int maxscl[3];

    unsigned char num_maxrgb_percentiles;
    unsigned char maxrgb_percentages[15];
    unsigned int  maxrgb_percentiles[15];

    struct {
        unsigned short  tone_mapping_flag;
        unsigned short  knee_point_x;
        unsigned short  knee_point_y;
        unsigned short  num_bezier_curve_anchors;
        unsigned short  bezier_curve_anchors[15];
    } tone_mapping;
#endif
} ExynosHdrData_ST2094_40;

typedef struct _ExynosHdrDynamicInfo {
    unsigned int valid;
    unsigned int reserved[42];

    ExynosHdrData_ST2094_40 data;
} ExynosHdrDynamicInfo;

typedef struct _ExynosHdrDynamicBlob {
    int  nSize;
    char pData[1020];
} ExynosHdrDynamicBlob;

typedef struct _ExynosHdrDynamicStatInfo{
    unsigned int hdr_linear_max_scl[3];
    unsigned int reserved1;

    unsigned long long hdr_linear_sum_max_rgb;
    unsigned long long reserved2;

    unsigned int hdr_pq_max_scl[3];
    unsigned int reserved3;

    unsigned long long hdr_pq_sum_max_rgb;
    unsigned long long reserved4;

    unsigned int hdr_pq_dist_max_rgb[1024];
} ExynosHdrDynamicStatInfo;

#define MAX_ROIINFO_SIZE 32400
typedef struct _ExynosVideoROIData {
    int     nUpperQpOffset;
    int     nLowerQpOffset;
    int     bUseRoiInfo;
    int     nRoiMBInfoSize;
    char    pRoiMBInfo[MAX_ROIINFO_SIZE];
} ExynosVideoROIData;

typedef enum _ExynosVideoGDCType {
    GDC_TYPE_NONE = 0,
    GDC_TYPE_VOTF,
    GDC_TYPE_OTF,
} ExynosVideoGDCType;

typedef struct _GDCRect {
    int x;
    int y;
    int w;
    int h;
} GDCRect;

typedef struct _GDCCropInfo {
    GDCRect rect;
    int fullW;
    int fullH;
} GDCCropInfo;

#define GDC_GRID_MAX_HEIGHT 33
#define GDC_GRID_MAX_WIDTH 33

typedef struct
{
     int32_t gridX[GDC_GRID_MAX_HEIGHT * GDC_GRID_MAX_WIDTH];
     int32_t gridY[GDC_GRID_MAX_HEIGHT * GDC_GRID_MAX_WIDTH];
     uint32_t width;
     uint32_t height;
} GDCGridTable;

typedef struct {
    GDCCropInfo cropInfo;
    GDCGridTable gridTable;

    int is_grid_mode;
    int is_bypass_mode;
    int index;
} GDCInfo;

typedef struct _ExynosVideoCrop {
    int left;
    int top;
    int width;
    int height;
} ExynosVideoCrop;

typedef struct _ExynosVideoDecData {
    int nInterlacedType;
} ExynosVideoDecData;

typedef struct _ExynosVideoEncData {
    ExynosVideoYSUMData sYsumData;
    unsigned long long pRoiData;  /* for fixing byte alignemnt on 64x32 problem */
    int nUseGdcOTF;
    int nIsGdcOTF;
    GDCInfo sGDCInfo;
} ExynosVideoEncData;

typedef struct _ExynosVideoMeta {
    /***********************************************/
    /****** WARNING: DO NOT MODIFY THIS AREA *******/
    /**/                                         /**/
    /**/  ExynosVideoInfoType eType;             /**/
    /**/  int                 nHDRType;          /**/
    /**/                                         /**/
    /**/  ExynosColorAspects   sColorAspects;    /**/
    /**/  ExynosHdrStaticInfo  sHdrStaticInfo;   /**/
    /**/  ExynosHdrDynamicInfo sHdrDynamicInfo;  /**/
    /**/                                         /**/
    /****** WARNING: DO NOT MODIFY THIS AREA *******/
    /***********************************************/

    int nPixelFormat;
    ExynosVideoCrop crop;

    union {
        ExynosVideoDecData dec;
        ExynosVideoEncData enc;
    } data;

    int reserved[20];   /* reserved filed for additional info */
} ExynosVideoMeta;

int Exynos_parsing_user_data_registered_itu_t_t35(ExynosHdrDynamicInfo *dest, void *src);
int Exynos_dynamic_meta_to_itu_t_t35(ExynosHdrDynamicInfo *src, char *dst);

/* SEI Write */
unsigned int Exynos_sei_write(ExynosHdrData_ST2094_40 *data, int size, unsigned char *stream);

#ifdef __cplusplus
}
#endif

#endif /* VENDOR_VIDEO_API_H_ */
