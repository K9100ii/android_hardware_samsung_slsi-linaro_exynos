# Copyright (C) 2019 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH := $(call my-dir)

SOC_BASE_PATH := $(TOP)/hardware/samsung_slsi-linaro/exynos

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    audio_proxy.c

LOCAL_C_INCLUDES += \
    $(SOC_BASE_PATH)/include/libaudio/audiohal_comv1 \
    $(SOC_BASE_PATH)/libaudio/audiohal_comv1/odm_specific \
    external/tinyalsa/include \
    external/tinycompress/include \
    external/kernel-headers/original/uapi/sound \
    $(call include-path-for, audio-utils) \
    $(call include-path-for, audio-route) \
    $(call include-path-for, alsa-utils) \
    external/expat/lib

LOCAL_HEADER_LIBRARIES := libhardware_headers
LOCAL_SHARED_LIBRARIES := liblog libcutils libtinyalsa libtinycompress libaudioutils libaudioroute libalsautils libexpat

# To use the headers from vndk-ext libs
LOCAL_CFLAGS += -D__ANDROID_VNDK_SEC__


LOCAL_MODULE := libaudioproxy
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS += -Werror -Wall
LOCAL_CFLAGS += -Wno-unused-parameter

include $(BUILD_SHARED_LIBRARY)
