/*
 * Copyright Samsung Electronics Co.,LTD.
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __DYNAMIC_INFO_LEGACY_H__
#define __DYNAMIC_INFO_LEGACY_H__

#include <stdint.h>
#ifndef HDR_TEST
#include <VendorVideoAPI.h>
#else
#include <VendorVideoAPI_hdrTest.h>
#endif

#ifndef USE_FULL_ST2094_40
typedef ExynosHdrDynamicInfo ExynosHdrDynamicInfo_t;
#else
typedef struct _ExynosHdrDynamicInfo_legacy {
    unsigned int valid;

    struct {
        unsigned char  country_code;
        unsigned short provider_code;
        unsigned short provider_oriented_code;

        unsigned char  application_identifier;
        unsigned short application_version;

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
    } data;

    unsigned int reserved[42];
} ExynosHdrDynamicInfo_legacy;

typedef ExynosHdrDynamicInfo_legacy ExynosHdrDynamicInfo_t;
#endif

void convertDynamicMeta(ExynosHdrDynamicInfo_t *dyn_meta, ExynosHdrDynamicInfo *dynamic_metadata);

#endif


