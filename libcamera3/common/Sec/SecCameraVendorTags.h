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

typedef enum camera_metadata_enum_samsung_android_control_metering_mode {
    SAMSUNG_ANDROID_CONTROL_METERING_MODE_CENTER = 0,
    SAMSUNG_ANDROID_CONTROL_METERING_MODE_SPOT,
    SAMSUNG_ANDROID_CONTROL_METERING_MODE_MATRIX,
    SAMSUNG_ANDROID_CONTROL_METERING_MODE_MANUAL,
    SAMSUNG_ANDROID_CONTROL_METERING_MODE_WEIGHTED_CENTER,
    SAMSUNG_ANDROID_CONTROL_METERING_MODE_WEIGHTED_SPOT,
    SAMSUNG_ANDROID_CONTROL_METERING_MODE_WEIGHTED_MATRIX,
} camera_metadata_enum_samsung_android_control_metering_mode_t;

typedef enum camera_metadata_enum_samsung_android_control_touch_ae_state {
    SAMSUNG_ANDROID_CONTROL_TOUCH_AE_SEARCHING = 0,
    SAMSUNG_ANDROID_CONTROL_TOUCH_AE_DONE,
    SAMSUNG_ANDROID_CONTROL_BV_CHANGED,
} camera_metadata_enum_samsung_android_control_touch_ae_state_t;

typedef enum camera_metadata_enum_samsung_android_lens_ois_mode {
    SAMSUNG_ANDROID_LENS_OPTICAL_STABILIZATION_OPERATION_MODE_PICTURE = 0,
    SAMSUNG_ANDROID_LENS_OPTICAL_STABILIZATION_OPERATION_MODE_VIDEO,
    SAMSUNG_ANDROID_LENS_OPTICAL_STABILIZATION_OPERATION_MODE_SINE_X,
    SAMSUNG_ANDROID_LENS_OPTICAL_STABILIZATION_OPERATION_MODE_SINE_Y,
} camera_metadata_enum_samsung_android_lens_ois_mode_t;

typedef enum camera_metadata_enum_samsung_android_control_live_hdr_mode {
    SAMSUNG_ANDROID_CONTROL_LIVE_HDR_MODE_OFF = 0,
    SAMSUNG_ANDROID_CONTROL_LIVE_HDR_MODE_ON,
    SAMSUNG_ANDROID_CONTROL_LIVE_HDR_MODE_AUTO,
} camera_metadata_enum_samsung_android_control_live_hdr_mode_t;

typedef enum camera_metadata_enum_samsung_android_control_live_hdr_state{
    SAMSUNG_ANDROID_CONTROL_LIVE_HDR_AUTO_OFF = 0,
    SAMSUNG_ANDROID_CONTROL_LIVE_HDR_AUTO_REQUIRED,
} camera_metadata_enum_samsung_android_control_live_hdr_state_t;

typedef enum camera_metadata_enum_samsung_android_control_paf_mode{
    SAMSUNG_ANDROID_CONTROL_PAF_MODE_OFF = 0,
    SAMSUNG_ANDROID_CONTROL_PAF_MODE_ON,
} camera_metadata_enum_samsung_android_control_paf_mode_t;

typedef enum camera_metadata_enum_samsung_android_scaler_flip_mode {
    SAMSUNG_ANDROID_SCALER_FLIP_MODE_NONE = 0,
    SAMSUNG_ANDROID_SCALER_FLIP_MODE_HFLIP,
    SAMSUNG_ANDROID_SCALER_FLIP_MODE_VFLIP,
} camera_metadata_enum_samsung_android_scaler_flip_mode_t;

typedef enum camera_metadata_enum_samsung_android_control_light_condition_enable_mode {
    SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_ENABLE_SIMPLE               = 0,
    SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_ENABLE_FULL                 = 1,
} camera_metadata_enum_samsung_android_control_light_condition_enable_mode_t;

typedef enum camera_metadata_enum_samsung_android_control_light_condition {
    SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_HIGH               = 0x0,     /* ZSL, SR Capture */
    SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LOW                = 0x10,    /* OIS Capure, Deblur Capture */
    SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_SIS_LOW            = 0x11,    /* LLS Capture, Indicator */
    SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LLS_LOW            = 0x12,    /* Merged Capture, Indicator */
    SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_FLASH              = 0x20,    /* Flash Capture */
    SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LLS_FLASH          = 0x21,    /* LLS Flash Capture */
} camera_metadata_enum_samsung_android_control_light_condition_t;

typedef enum camera_metadata_enum_samsung_android_control_available_feature {
    SAMSUNG_ANDROID_CONTROL_AVAILABLE_FEATURE_LLS_CAPTURE              = 0,
    SAMSUNG_ANDROID_CONTROL_AVAILABLE_FEATURE_SHUTTER_NOTIFICATION     = 1,
} camera_metadata_enum_samsung_android_control_available_feature_t;

typedef enum camera_metadata_enum_samsung_android_control_shooting_mode {
    SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_SINGLE = 0,
    SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_AUTO = 1,
    SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_BEAUTY = 2,
    SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_VIDEO = 3,
    SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_PANORAMA = 4,
    SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_PRO = 5,
    SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_SELECTIVE_FOCUS = 6,
    SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_HDR = 7,
    SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_NIGHT = 8,
    SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_FOOD = 9,
    SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_DUAL = 10,
    SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_ANTIFOG = 11,
    SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_WIDE_SELFIE = 12,
    SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_SLOW_MOTION = 13,
    SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_INTERACTIVE = 14,
    SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_SPORTS = 15,
    SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_HYPER_MOTION = 16,
    SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_ANIMATED_GIF = 17,
    SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_COLOR_IRIS = 18,
} camera_metadata_enum_samsung_android_control_shooting_mode_t;

typedef enum camera_metadata_enum_samsung_android_control_capture_hint {
    SAMSUNG_ANDROID_CONTROL_CAPTURE_HINT_NONE = 0,
    SAMSUNG_ANDROID_CONTROL_CAPTURE_HINT_BURST,
    SAMSUNG_ANDROID_CONTROL_CAPTURE_HINT_HDR,
    SAMSUNG_ANDROID_CONTROL_CAPTURE_HINT_SR,
} camera_metadata_enum_samsung_android_control_capture_hint_t;

typedef enum camera_metadata_enum_samsung_android_control_awb_mode {
    SAMSUNG_ANDROID_CONTROL_AWB_MODE_START	 = 100,
    SAMSUNG_ANDROID_CONTROL_AWB_MODE_CUSTOM_K
} camera_metadata_enum_samsung_android_control_awb_mode_t;

typedef enum camera_metadata_enum_samsung_android_control_ae_mode {
    SAMSUNG_ANDROID_CONTROL_AE_MODE_START	 = 100,
    SAMSUNG_ANDROID_CONTROL_AE_MODE_ON_AUTO_SCREEN_FLASH,
    SAMSUNG_ANDROID_CONTROL_AE_MODE_ON_ALWAYS_SCREEN_FLASH,
    SAMSUNG_ANDROID_CONTROL_AE_MODE_OFF_ALWAYS_FLASH,
} camera_metadata_enum_samsung_android_control_ae_mode_t;

typedef enum camera_metadata_enum_samsung_android_control_multi_af_mode {
    SAMSUNG_ANDROID_CONTROL_MULTI_AF_MODE_OFF = 0,
    SAMSUNG_ANDROID_CONTROL_MULTI_AF_MODE_ON,
} camera_metadata_enum_samsung_android_control_multi_af_mode_t;

typedef enum camera_metadata_enum_samsung_android_control_af_mode {
    SAMSUNG_ANDROID_CONTROL_AF_MODE_START = 100,
    SAMSUNG_ANDROID_CONTROL_AF_MODE_OBJECT_TRACKING_PICTURE,
    SAMSUNG_ANDROID_CONTROL_AF_MODE_OBJECT_TRACKING_VIDEO,
    SAMSUNG_ANDROID_CONTROL_AF_MODE_FIXED_FACE,
} camera_metadata_enum_samsung_android_control_af_mode_t;

typedef enum camera_metadata_enum_samsung_android_control_effect_mode {
    SAMSUNG_ANDROID_CONTROL_EFFECT_MODE_START = 100,
    SAMSUNG_ANDROID_CONTROL_EFFECT_MODE_BEATUTY = 101,
} camera_metadata_enum_samsung_android_control_effect_mode_t;

typedef enum camera_metadata_enum_samsung_android_control_transientAction {
    SAMSUNG_ANDROID_CONTROL_TRANSIENT_ACTION_NONE = 0,
    SAMSUNG_ANDROID_CONTROL_TRANSIENT_ACTION_ZOOMING = 1,
    SAMSUNG_ANDROID_CONTROL_TRANSIENT_ACTION_MANUAL_FOCUSING = 2,
} camera_metadata_enum_samsung_android_control_transientAction;

typedef enum camera_metadata_enum_samsung_android_control_recording_motion_speed_mode {
    SAMSUNG_ANDROID_CONTROL_RECORDING_MOTION_SPEED_MODE_AUTO   = 0,
    SAMSUNG_ANDROID_CONTROL_RECORDING_MOTION_SPEED_MODE_4X     = 1,
    SAMSUNG_ANDROID_CONTROL_RECORDING_MOTION_SPEED_MODE_8X     = 2,
    SAMSUNG_ANDROID_CONTROL_RECORDING_MOTION_SPEED_MODE_16X    = 3,
    SAMSUNG_ANDROID_CONTROL_RECORDING_MOTION_SPEED_MODE_32X    = 4,
} camera_metadata_enum_samsung_android_control_recording_motion_speed_mode_t;

typedef enum camera_metadata_enum_samsung_android_control_object_tracking_state {
    SAMSUNG_ANDROID_CONTROL_OBJECT_TRACKING_STATE_UNDEFINED = 0,
    SAMSUNG_ANDROID_CONTROL_OBJECT_TRACKING_STATE_IDLE = 1,
    SAMSUNG_ANDROID_CONTROL_OBJECT_TRACKING_STATE_OK = 2,
    SAMSUNG_ANDROID_CONTROL_OBJECT_TRACKING_STATE_TEMPLOST = 3,
    SAMSUNG_ANDROID_CONTROL_OBJECT_TRACKING_STATE_LOST = 4,
} camera_metadata_enum_samsung_android_control_object_tracking_state_t;

typedef struct camera_metadata_struct_samsung_android_control_paf_result {
    int32_t mode;
    int32_t goal_pos;
    int32_t reliability;
    int32_t val;          /* single result : current_pos, multi result : focused */
} __attribute__((packed)) paf_result_t;

typedef struct camera_metadata_struct_samsung_android_control_paf_multi_info {
    int32_t column;
    int32_t row;
    int32_t usage;
} __attribute__((packed)) paf_multi_info_t;

typedef struct camera_metadata_struct_samsung_android_lens_info_current_info {
    int32_t driver_resolution;
    int32_t pan_pos;
    int32_t macro_pos;
    int32_t current_pos;
} __attribute__((packed)) lens_current_info_t;

namespace android {

enum seccamera_ext_section {
    SAMSUNG_ANDROID_CONTROL = VENDOR_SECTION,
    SAMSUNG_ANDROID_JPEG,
    SAMSUNG_ANDROID_LENS,
    SAMSUNG_ANDROID_LENS_INFO,
    SAMSUNG_ANDROID_SCALER,
    SAMSUNG_ANDROID_DEPTH,
    SAMSUNG_ANDROID_SENSOR,
    SAMSUNG_ANDROID_SENSOR_INFO,
    SAMSUNG_ANDROID_LED,
    SAMSUNG_EXTENSION_SECTION_END
};

enum seccamera_ext_section_ranges {
    SAMSUNG_ANDROID_CONTROL_START       = SAMSUNG_ANDROID_CONTROL      << 16,
    SAMSUNG_ANDROID_JPEG_START          = SAMSUNG_ANDROID_JPEG         << 16,
    SAMSUNG_ANDROID_LENS_START          = SAMSUNG_ANDROID_LENS         << 16,
    SAMSUNG_ANDROID_LENS_INFO_START     = SAMSUNG_ANDROID_LENS_INFO    << 16,
    SAMSUNG_ANDROID_SCALER_START        = SAMSUNG_ANDROID_SCALER       << 16,
    SAMSUNG_ANDROID_DEPTH_START         = SAMSUNG_ANDROID_DEPTH        << 16,
    SAMSUNG_ANDROID_SENSOR_START        = SAMSUNG_ANDROID_SENSOR       << 16,
    SAMSUNG_ANDROID_SENSOR_INFO_START   = SAMSUNG_ANDROID_SENSOR_INFO  << 16,
    SAMSUNG_ANDROID_LED_START           = SAMSUNG_ANDROID_LED          << 16,
};

enum seccamera_ext_tags {
    SAMSUNG_ANDROID_CONTROL_BASE =
            SAMSUNG_ANDROID_CONTROL_START,
    SAMSUNG_ANDROID_CONTROL_LIVE_HDR_LEVEL_RANGE,               // int32[]    Static meta
    SAMSUNG_ANDROID_CONTROL_LIVE_HDR_LEVEL,                     // byte       Control meta
    SAMSUNG_ANDROID_CONTROL_LIVE_HDR_AVAILABLE_MODES,           // int32[]    Static meta
    SAMSUNG_ANDROID_CONTROL_LIVE_HDR_MODE,                      // int32      Control meta
    SAMSUNG_ANDROID_CONTROL_LIVE_HDR_STATE,                     // int32      Dynamic meta
    SAMSUNG_ANDROID_CONTROL_AE_AVAILABLE_MODES,                 // byte[]     Static meta
    SAMSUNG_ANDROID_CONTROL_METERING_AVAILABLE_MODE,            // int32[]    Static meta
    SAMSUNG_ANDROID_CONTROL_METERING_MODE,                      // int32      Control meta
    SAMSUNG_ANDROID_CONTROL_TOUCH_AE_STATE,                     // int32      Dynamic meta
    SAMSUNG_ANDROID_CONTROL_PAF_AVAILABLE_MODE,                 // byte       Static meta
    SAMSUNG_ANDROID_CONTROL_PAF_MODE,                           // int32      Control meta
    SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_ENABLE_MODE,        // int32      Control meta
    SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION,                    // int32      Dynamic meta
    SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE,                      // int32      Control meta
    SAMSUNG_ANDROID_CONTROL_EV_COMPENSATION_VALUE,              // int32      Dynamic meta
    SAMSUNG_ANDROID_CONTROL_CAPTURE_HINT,                       // int32      Control meta
    SAMSUNG_ANDROID_CONTROL_COLOR_TEMPERATURE,                  // int32      Control meta
    SAMSUNG_ANDROID_CONTROL_COLOR_TEMPERATURE_RANGE,            // int32[]    Static meta
    SAMSUNG_ANDROID_CONTROL_AWB_AVAILABLE_MODES,                // int32[]    Static meta
    SAMSUNG_ANDROID_CONTROL_BURST_SHOT_FPS_RANGE,               // int32[]    Static meta
    SAMSUNG_ANDROID_CONTROL_BURST_SHOT_FPS,                     // int32[]    Dynamic meta
    SAMSUNG_ANDROID_CONTROL_DOF_SINGLE_DATA,                    // int32[]    Dynamic meta
    SAMSUNG_ANDROID_CONTROL_DOF_MULTI_INFO,                     // int32[]    Dynamic meta
    SAMSUNG_ANDROID_CONTROL_DOF_MULTI_DATA,                     // int32[]    Dynamic meta
    SAMSUNG_ANDROID_CONTROL_WBLEVEL_RANGE,                      // int32[]    Static meta
    SAMSUNG_ANDROID_CONTROL_WBLEVEL,                            // int32      Control meta
    SAMSUNG_ANDROID_CONTROL_MULTI_AF_AVAILABLE_MODES,           // byte[]     Static meta
    SAMSUNG_ANDROID_CONTROL_MULTI_AF_MODE,                      // byte       Control meta
    SAMSUNG_ANDROID_CONTROL_AF_AVAILABLE_MODES,                 // byte[]     Static meta
    SAMSUNG_ANDROID_CONTROL_EFFECT_AVAILABLE_MODES,             // byte[]     Static meta
    SAMSUNG_ANDROID_CONTROL_TRANSIENTACTION,                    // int32      Control meta
    SAMSUNG_ANDROID_CONTROL_BEAUTY_SCENE_INDEX,                 // int32      Dynamic meta
    SAMSUNG_ANDROID_CONTROL_LENS_DIRTY_DETECT,                  // byte       Dynamic meta
    SAMSUNG_ANDROID_CONTROL_SHUTTER_NOTIFICATION,               // byte       Dynamic meta
    SAMSUNG_ANDROID_CONTROL_AVAILABLE_FEATURES,                 // int32[]    Static meta
    SAMSUNG_ANDROID_CONTROL_BRIGHTNESS_VALUE,                   // int32[]    Dynamic meta
    SAMSUNG_ANDROID_CONTROL_RECORDING_MOTION_SPEED_MODE,        // int32[]    Control meta
    SAMSUNG_ANDROID_CONTROL_OBJECT_TRACKING_STATE,              // int32[]    Dynamic meta
    SAMSUNG_ANDROID_CONTROL_REQUEST_BUILD_NUMBER,               // int64[]    Control meta
    SAMSUNG_ANDROID_CONTROL_END,

    SAMSUNG_ANDROID_JPEG_IMAGE_UNIQUE_ID =
            SAMSUNG_ANDROID_JPEG_START,                         // byte[]     Dynamic meta
    SAMSUNG_ANDROID_JPEG_IMAGE_DEBUGINFO_APP4,                  // byte[]     Dynamic meta
    SAMSUNG_ANDROID_JPEG_IMAGE_DEBUGINFO_APP5,                  // byte[]     Dynamic meta
    SAMSUNG_ANDROID_JPEG_END,

    SAMSUNG_ANDROID_LENS_OPTICAL_STABILIZATION_OPERATION_MODE =
            SAMSUNG_ANDROID_LENS_START,                         // int32      Control meta
    SAMSUNG_ANDROID_LENS_FOCUS_LENS_POS,                        // int32      Control meta
    SAMSUNG_ANDROID_LENS_FOCUS_LENS_POS_STALL,                  // int32      Control meta
    SAMSUNG_ANDROID_LENS_END,

    SAMSUNG_ANDROID_LENS_INFO_AVAILABLE_OPTICAL_STABILIZATION_OPERATION_MODE =
            SAMSUNG_ANDROID_LENS_INFO_START,                    // int32[]    Static meta
    SAMSUNG_ANDROID_LENS_INFO_HORIZONTAL_VIEW_ANGLES,           // float[]    Static meta
    SAMSUNG_ANDROID_LENS_INFO_VERTICAL_VIEW_ANGLE,              // float      Static meta
    SAMSUNG_ANDROID_LENS_INFO_FOCALLENGTH_IN_35MM_FILM,         // int32      Dynamic meta
    SAMSUNG_ANDROID_LENS_INFO_CURRENTINFO,                      // int32[]    Dynamic meta
    SAMSUNG_ANDROID_LENS_INFO_END,

    SAMSUNG_ANDROID_SCALER_FLIP_AVAILABLE_MODES =
            SAMSUNG_ANDROID_SCALER_START,                       // int32[]    Static meta
    SAMSUNG_ANDROID_SCALER_FLIP_MODE,                           // int32      Control meta
    SAMSUNG_ANDROID_SCALER_AVAILABLE_VIDEO_CONFIGURATIONS,                      // int32[]    Static meta
    SAMSUNG_ANDROID_SCALER_AVAILABLE_THUMBNAIL_STREAM_CONFIGURATIONS,           // int32[]    Static meta
    SAMSUNG_ANDROID_SCALER_AVAILABLE_HIGH_SPEED_VIDEO_CONFIGURATIONS,           // int32[]    Static meta
    SAMSUNG_ANDROID_SCALER_END,

    SAMSUNG_ANDROID_DEPTH_AVAILABLE_DEPTH_STREAM_CONFIGURATIONS  =
            SAMSUNG_ANDROID_DEPTH_START,                        // int32[]    Static meta
    SAMSUNG_ANDROID_DEPTH_END,
    SAMSUNG_ANDROID_SENSOR_GAIN =                               // int32      Control meta
            SAMSUNG_ANDROID_SENSOR_START,
    SAMSUNG_ANDROID_SENSOR_END,
    SAMSUNG_ANDROID_SENSOR_INFO_GAIN_RANGE =
            SAMSUNG_ANDROID_SENSOR_INFO_START,                  // int32[]    Static meta
    SAMSUNG_ANDROID_SENSOR_INFO_END,
    SAMSUNG_ANDROID_LED_CURRENT_RANGE =                         // int32[]    Static meta
            SAMSUNG_ANDROID_LED_START,
    SAMSUNG_ANDROID_LED_PULSE_DELAY_RANGE,                      // int64[]    Static meta
    SAMSUNG_ANDROID_LED_PULSE_WIDTH_RANGE,                      // int64[]    Static meta
    SAMSUNG_ANDROID_LED_LED_MAX_TIME_RANGE,                     // int32[]    Static meta
    SAMSUNG_ANDROID_LED_CURRENT,                                // int32      Control meta
    SAMSUNG_ANDROID_LED_PULSE_DELAY,                            // int64      Control meta
    SAMSUNG_ANDROID_LED_PULSE_WIDTH,                            // int64      Control meta
    SAMSUNG_ANDROID_LED_MAX_TIME,                               // int32      Control meta
    SAMSUNG_ANDROID_LED_END
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
