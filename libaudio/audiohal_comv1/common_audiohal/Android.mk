# Copyright (C) 2014 The Android Open Source Project
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

#
# Primary Audio HAL
#
ifeq ($(BOARD_USE_AUDIOHAL_COMV1), true)
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	audio_hw.c \

LOCAL_C_INCLUDES += \
	$(TOP)/hardware/samsung_slsi/exynos/include/libaudio/audiohal_comv1 \
	$(call include-path-for, audio-utils)

LOCAL_HEADER_LIBRARIES := libhardware_headers
LOCAL_SHARED_LIBRARIES := liblog libcutils libprocessgroup
LOCAL_SHARED_LIBRARIES += libaudioproxy

LOCAL_CFLAGS += -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function

# SoC based Configuration
# A-Box Version 2.x.x
# Calliope v2.0: 9820 9825 9830
ifneq ($(filter exynos9820 exynos9830 exynos9825 exynos9630 exynos3830,$(TARGET_SOC_BASE)),)
  LOCAL_CFLAGS += -DAUDIO_CALLIOPE_V20
  LOCAL_CFLAGS += -DAUDIO_PLATFORM_ABOX_V2
endif

# Audio Feature based Configuration
ifeq ($(BOARD_USE_BTA2DP_OFFLOAD),true)
LOCAL_CFLAGS += -DSUPPORT_BTA2DP_OFFLOAD
endif

ifeq ($(BOARD_USE_SOUNDTRIGGER_HAL),true)
LOCAL_CFLAGS += -DSUPPORT_STHAL_INTERFACE
endif

ifeq ($(BOARD_USE_USB_OFFLOAD),true)
LOCAL_CFLAGS += -DSUPPORT_DIRECT_MULTI_CHANNEL_STREAM
endif

LOCAL_MODULE := audio.primary.$(TARGET_SOC)
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_TAGS := optional

LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_SHARED_LIBRARY)
endif
