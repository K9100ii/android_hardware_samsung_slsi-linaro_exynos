/*
* Copyright (C) 2012 The Android Open Source Project
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

#ifndef EXYNOS_CAMERA_VENDOR_TAGS_H__
#define EXYNOS_CAMERA_VENDOR_TAGS_H__

#include <system/camera_metadata.h>
#include <system/camera_vendor_tags.h>

typedef struct vendor_tag_info {
    const char *tag_name;
    uint8_t     tag_type;
} vendor_tag_info_t;

namespace android {

enum seccamera_ext_section {
    SAMSUNG_ANDROID_CONTROL = VENDOR_SECTION,
    SAMSUNG_ANDROID_LENS,
    SAMSUNG_ANDROID_LENS_INFO,
    SAMSUNG_EXTENSION_SECTION_END
};

enum seccamera_ext_section_ranges {
    SAMSUNG_ANDROID_CONTROL_START       = SAMSUNG_ANDROID_CONTROL   << 16,
    SAMSUNG_ANDROID_LENS_START          = SAMSUNG_ANDROID_LENS      << 16,
    SAMSUNG_ANDROID_LENS_INFO_START     = SAMSUNG_ANDROID_LENS_INFO << 16,
};

enum seccamera_ext_tags {
    SAMSUNG_ANDROID_CONTROL_LIVE_HDR_LEVEL_RANGE =
            SAMSUNG_ANDROID_CONTROL_START,
    SAMSUNG_ANDROID_CONTROL_LIVE_HDR_LEVEL,
    SAMSUNG_ANDROID_CONTROL_METERING_AVAILABLE_MODE,
    SAMSUNG_ANDROID_CONTROL_METERING_MODE,
    SAMSUNG_ANDROID_CONTROL_PAF_AVAILABLE_MODE,
    SAMSUNG_ANDROID_CONTROL_PAF_MODE,
    SAMSUNG_ANDROID_CONTROL_END,

    SAMSUNG_ANDROID_LENS_OPTICAL_STABILIZATION_OPERATION_MODE =
            SAMSUNG_ANDROID_LENS_START,
    SAMSUNG_ANDROID_LENS_END,

    SAMSUNG_ANDROID_LENS_INFO_AVAILABLE_OPTICAL_STABILIZATION_OPERATION_MODE =
            SAMSUNG_ANDROID_LENS_INFO_START,
    SAMSUNG_ANDROID_LENS_INFO_END,
};

class SecCameraVendorTags {
public:
    static const char *get_ext_section_name(const vendor_tag_ops_t *v,
        uint32_t tag);
    static const char *get_ext_tag_name(
        const vendor_tag_ops_t *v,
        uint32_t tag);
    static int get_ext_tag_type(const vendor_tag_ops_t *v,
        uint32_t tag);
    static int get_ext_tag_count(const vendor_tag_ops_t *v);
    static void get_ext_all_tags(const vendor_tag_ops_t *v, uint32_t *tag_array);

    static const vendor_tag_ops_t *Ops;
};
}; /* namespace android */
#endif
