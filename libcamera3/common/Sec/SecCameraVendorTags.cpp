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

#include <log/log.h>
#include <stdint.h>
#include "SecCameraVendorTags.h"

namespace android {

const vendor_tag_ops_t* SecCameraVendorTags::Ops = NULL;

const int SAMSUNG_EXTENSION_SECTION_COUNT = SAMSUNG_EXTENSION_SECTION_END - VENDOR_SECTION;

const char *samsung_extension_section_names[SAMSUNG_EXTENSION_SECTION_COUNT] = {
    "samsung.android.control",
    "samsung.android.jpeg",
    "samsung.android.lens",
    "samsung.android.lens.info",
    "samsung.android.scaler",
    "samsung.android.depth",
    "samsung.android.sensor",
    "samsung.android.sensor.info",
    "samsung.android.led",
};

uint32_t samsung_extension_section_bounds[SAMSUNG_EXTENSION_SECTION_COUNT][2] = {
    { (uint32_t)SAMSUNG_ANDROID_CONTROL_START,(uint32_t)SAMSUNG_ANDROID_CONTROL_END },
    { (uint32_t)SAMSUNG_ANDROID_JPEG_START,(uint32_t)SAMSUNG_ANDROID_JPEG_END },
    { (uint32_t)SAMSUNG_ANDROID_LENS_START,(uint32_t)SAMSUNG_ANDROID_LENS_END },
    { (uint32_t)SAMSUNG_ANDROID_LENS_INFO_START,(uint32_t)SAMSUNG_ANDROID_LENS_INFO_END },
    { (uint32_t)SAMSUNG_ANDROID_SCALER_START,(uint32_t)SAMSUNG_ANDROID_SCALER_END },
    { (uint32_t)SAMSUNG_ANDROID_DEPTH_START,(uint32_t)SAMSUNG_ANDROID_DEPTH_END },
    { (uint32_t)SAMSUNG_ANDROID_SENSOR_START,(uint32_t)SAMSUNG_ANDROID_SENSOR_END },
    { (uint32_t)SAMSUNG_ANDROID_SENSOR_INFO_START,(uint32_t)SAMSUNG_ANDROID_SENSOR_INFO_END },
    { (uint32_t)SAMSUNG_ANDROID_LED_START,(uint32_t)SAMSUNG_ANDROID_LED_END },
};

static vendor_tag_info_t seccamera_android_control[SAMSUNG_ANDROID_CONTROL_END -
        SAMSUNG_ANDROID_CONTROL_START] = {
    { "controlbase",                        TYPE_INT32  },
    { "liveHdrLevelRange",                  TYPE_INT32  },
    { "liveHdrLevel",                       TYPE_INT32  },
    { "liveHdrAvailableModes",              TYPE_INT32  },
    { "liveHdrMode",                        TYPE_INT32  },
    { "liveHdrState",                       TYPE_INT32  },
    { "aeAvailableModes",                    TYPE_BYTE  },
    { "meteringAvailableMode",              TYPE_INT32  },
    { "meteringMode",                       TYPE_INT32  },
    { "touchAeState",                       TYPE_INT32  },
    { "pafAvailableMode",                   TYPE_BYTE   },
    { "pafMode",                            TYPE_INT32  },
    { "lightConditionEnableMode",           TYPE_INT32  },
    { "lightCondition",                     TYPE_INT32  },
    { "shootingMode",                       TYPE_INT32  },
    { "evCompensationValue",                TYPE_INT32  },
    { "captureHint",                        TYPE_INT32  },
    { "colorTemperature",                   TYPE_INT32  },
    { "colorTemperatureRange",              TYPE_INT32  },
    { "awbAvailableModes",                  TYPE_INT32  },
    { "burstShotFpsRange",                  TYPE_INT32  },
    { "burstShotFps",                       TYPE_INT32  },
    { "dofSingleData",                      TYPE_INT32  },
    { "dofMultiInfo",                       TYPE_INT32  },
    { "dofMultiData",                       TYPE_INT32  },
    { "wbLevelRange",                       TYPE_INT32  },
    { "wbLevel",                            TYPE_INT32  },
    { "multiAfAvailableModes",              TYPE_BYTE   },
    { "multiAfMode",                        TYPE_BYTE   },
    { "afAvailableModes",                   TYPE_BYTE   },
    { "availableEffects",                   TYPE_BYTE   },
    { "transientAction",                    TYPE_INT32  },
    { "beautySceneIndex",                   TYPE_INT32  },
    { "lensDirtyDetect",                    TYPE_BYTE   },
    { "shutterNotification",                TYPE_BYTE   },
    { "availableFeatures",                  TYPE_INT32  },
    { "brightnessValue",                    TYPE_INT32  },
    { "recordingMotionSpeedMode",           TYPE_INT32  },
    { "objectTrackingState",                TYPE_INT32  },
    { "requestBuildNumber",                 TYPE_INT64  },
};

static vendor_tag_info_t seccamera_android_jpeg[SAMSUNG_ANDROID_JPEG_END -
        SAMSUNG_ANDROID_JPEG_START] = {
    { "imageUniqueId",      TYPE_BYTE  },
    { "imageDebugInfoApp4",      TYPE_BYTE  },
    { "imageDebugInfoApp5",      TYPE_BYTE  },
};

static vendor_tag_info_t seccamera_android_lens[SAMSUNG_ANDROID_LENS_END -
        SAMSUNG_ANDROID_LENS_START] = {
    { "opticalStabilizationOperationMode",      TYPE_INT32  },
    { "focusLensPos",                           TYPE_INT32  }, /* Manual focus with lens code, Need to set focus mode to off */
    { "focusLensPosStall",                      TYPE_INT32  }, /* System controlled manual focus with lens code, Optimized for capture */
};

static vendor_tag_info_t seccamera_android_lens_info[SAMSUNG_ANDROID_LENS_INFO_END -
        SAMSUNG_ANDROID_LENS_INFO_START] = {
    { "availableOpticalStabilizationOperationMode",     TYPE_INT32  },
    { "horizontalViewAngles",                           TYPE_FLOAT  },
    { "verticalViewAngle",                              TYPE_FLOAT  },
    { "focalLengthIn35mmFilm",                          TYPE_INT32  },
    { "currentInfo",                                    TYPE_INT32  },
};

static vendor_tag_info_t seccamera_android_scaler[SAMSUNG_ANDROID_SCALER_END -
        SAMSUNG_ANDROID_SCALER_START] = {
    { "flipAvailableModes",                        TYPE_INT32  },
    { "flipMode",                                  TYPE_INT32  },
    { "availableVideoConfigurations",              TYPE_INT32  },
    { "availableThumbnailStreamConfigurations",    TYPE_INT32  },
    { "availableHighSpeedVideoConfigurations",     TYPE_INT32  },
};

static vendor_tag_info_t seccamera_android_depth[SAMSUNG_ANDROID_DEPTH_END -
        SAMSUNG_ANDROID_DEPTH_START] = {
    { "availableDepthStreamConfigurations",     TYPE_INT32  },
};

static vendor_tag_info_t seccamera_android_sensor[SAMSUNG_ANDROID_SENSOR_END -
        SAMSUNG_ANDROID_SENSOR_START] = {
    { "gain",                                   TYPE_INT32  },
};

static vendor_tag_info_t seccamera_android_sensor_info[SAMSUNG_ANDROID_SENSOR_INFO_END -
        SAMSUNG_ANDROID_SENSOR_INFO_START] = {
    { "gainRange",                              TYPE_INT32  },
};

static vendor_tag_info_t seccamera_android_led[SAMSUNG_ANDROID_LED_END -
        SAMSUNG_ANDROID_LED_START] = {
    { "currentRange",                           TYPE_INT32  },
    { "pulseDelayRange",                        TYPE_INT64  },
    { "pulseWidthRange",                        TYPE_INT64  },
    { "maxTimeRange",                           TYPE_INT32  },
    { "current",                                TYPE_INT32  },
    { "pulseDelay",                             TYPE_INT64  },
    { "pulseWidth",                             TYPE_INT64  },
    { "maxTime",                                TYPE_INT32  },
};

vendor_tag_info_t *samsung_extension_tag_info[SAMSUNG_EXTENSION_SECTION_COUNT] = {
    seccamera_android_control,
    seccamera_android_jpeg,
    seccamera_android_lens,
    seccamera_android_lens_info,
    seccamera_android_scaler,
    seccamera_android_depth,
    seccamera_android_sensor,
    seccamera_android_sensor_info,
    seccamera_android_led,
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
