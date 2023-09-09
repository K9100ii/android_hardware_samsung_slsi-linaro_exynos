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

/*
 * SAMSUNG Extension
*/

#define LOG_TAG "SecsCameraVendorTags"

#include <cutils/log.h>
#include <stdint.h>
#include "SecCameraVendorTags.h"

namespace android {

const vendor_tag_ops_t* SecCameraVendorTags::Ops = NULL;

const int SAMSUNG_EXTENSION_SECTION_COUNT = SAMSUNG_EXTENSION_SECTION_END - VENDOR_SECTION;

const char *samsung_extension_section_names[SAMSUNG_EXTENSION_SECTION_COUNT] = {
    "samsung.android.control",
    "samsung.android.lens",
    "samsung.android.lens.info",
};

uint32_t samsung_extension_section_bounds[SAMSUNG_EXTENSION_SECTION_COUNT][2] = {
    { (uint32_t)SAMSUNG_ANDROID_CONTROL_START,(uint32_t)SAMSUNG_ANDROID_CONTROL_END },
    { (uint32_t)SAMSUNG_ANDROID_LENS_START,(uint32_t)SAMSUNG_ANDROID_LENS_END },
    { (uint32_t)SAMSUNG_ANDROID_LENS_INFO_START,(uint32_t)SAMSUNG_ANDROID_LENS_INFO_END },
};

uint32_t seccamera_ext_all_tags[] = {
    (uint32_t)SAMSUNG_ANDROID_CONTROL_LIVE_HDR_LEVEL_RANGE,
    (uint32_t)SAMSUNG_ANDROID_CONTROL_LIVE_HDR_LEVEL,
    (uint32_t)SAMSUNG_ANDROID_CONTROL_METERING_AVAILABLE_MODE,
    (uint32_t)SAMSUNG_ANDROID_CONTROL_METERING_MODE,
    (uint32_t)SAMSUNG_ANDROID_CONTROL_PAF_AVAILABLE_MODE,
    (uint32_t)SAMSUNG_ANDROID_CONTROL_PAF_MODE,
    (uint32_t)SAMSUNG_ANDROID_CONTROL_END,
    (uint32_t)SAMSUNG_ANDROID_LENS_OPTICAL_STABILIZATION_OPERATION_MODE,
    (uint32_t)SAMSUNG_ANDROID_LENS_END,
    (uint32_t)SAMSUNG_ANDROID_LENS_INFO_AVAILABLE_OPTICAL_STABILIZATION_OPERATION_MODE,
    (uint32_t)SAMSUNG_ANDROID_LENS_INFO_END,
};

static vendor_tag_info_t seccamera_android_control[SAMSUNG_ANDROID_CONTROL_END -
        SAMSUNG_ANDROID_CONTROL_START] = {
    { "liveHdrLevelRange",                  TYPE_INT32  },
    { "liveHdrLevel",                       TYPE_INT32  },
    { "meteringAvailableMode",              TYPE_INT32  },
    { "meteringMode",                       TYPE_INT32  },
    { "pafAvailableMode",                   TYPE_BYTE  },
    { "pafMode",                            TYPE_INT32  },
};

static vendor_tag_info_t seccamera_android_lens[SAMSUNG_ANDROID_LENS_END -
        SAMSUNG_ANDROID_LENS_START] = {
    { "opticalStabilizationOperationMode",      TYPE_INT32  },
};

static vendor_tag_info_t seccamera_android_lens_info[SAMSUNG_ANDROID_LENS_INFO_END -
        SAMSUNG_ANDROID_LENS_INFO_START] = {
    { "availableOpticalStabilizationOperationMode",     TYPE_INT32  },
};

vendor_tag_info_t *samsung_extension_tag_info[SAMSUNG_EXTENSION_SECTION_COUNT] = {
    seccamera_android_control,
    seccamera_android_lens,
    seccamera_android_lens_info,
};

const char* SecCameraVendorTags::get_ext_section_name(__unused const vendor_tag_ops_t *v,
        uint32_t tag) {
    int tag_section = (tag >> 16) - VENDOR_SECTION;

    if (tag_section < 0 || tag_section >= SAMSUNG_EXTENSION_SECTION_COUNT) {
        return NULL;
    }

    return samsung_extension_section_names[tag_section];
}

const char* SecCameraVendorTags::get_ext_tag_name(__unused const vendor_tag_ops_t *v,
        uint32_t tag) {
    int tag_section = (tag >> 16) - VENDOR_SECTION;
    int tag_index = tag & 0xFFFF;

    if (tag_section < 0 || tag_section >= SAMSUNG_EXTENSION_SECTION_COUNT ||
        tag >= samsung_extension_section_bounds[tag_section][1]) {
        return NULL;
    }
    return samsung_extension_tag_info[tag_section][tag_index].tag_name;
}

int SecCameraVendorTags::get_ext_tag_type(__unused const vendor_tag_ops_t *v,
        uint32_t tag) {
    int tag_section = (tag >> 16) - VENDOR_SECTION;
    int tag_index = tag & 0xFFFF;

    if (tag_section < 0 || tag_section >= SAMSUNG_EXTENSION_SECTION_COUNT ||
        tag >= samsung_extension_section_bounds[tag_section][1]) {
        return -1;
    }
    return samsung_extension_tag_info[tag_section][tag_index].tag_type;
}

int SecCameraVendorTags::get_ext_tag_count(__unused const vendor_tag_ops_t *v) {
    int section;
    int count = 0;
    uint32_t start, end;

    for (section = 0; section < SAMSUNG_EXTENSION_SECTION_COUNT; section++) {
        start = samsung_extension_section_bounds[section][0];
        end = samsung_extension_section_bounds[section][1];
        count += end - start;
    }
    return count;
}

void SecCameraVendorTags::get_ext_all_tags(__unused const vendor_tag_ops_t *v, uint32_t *tag_array) {
    int section;
    uint32_t start, end, tag;

    for (section = 0; section < SAMSUNG_EXTENSION_SECTION_COUNT; section++) {
        start = samsung_extension_section_bounds[section][0];
        end = samsung_extension_section_bounds[section][1];
        for (tag = start; tag < end; tag++) {
            *tag_array++ = tag;
        }
    }
}

};
