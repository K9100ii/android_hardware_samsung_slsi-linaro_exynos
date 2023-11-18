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
ifeq ($(BOARD_USE_AUDIOHAL), true)
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	audio_hw.c \
	factory_manager.c

ifeq ($(BOARD_USE_SITRIL), true)
LOCAL_SRC_FILES += \
    voice_manager_sit.c

LOCAL_C_INCLUDES += \
    $(TOP)/hardware/samsung_slsi-linaro_13-e850-96/exynos/libaudio/audioril-sit \
    $(TOP)/hardware/samsung_slsi-linaro_13-e850-96/exynos/libaudio/audioril-sit/include

LOCAL_CFLAGS += -DUSE_SITRIL
else
LOCAL_SRC_FILES += \
    voice_manager_sec.c

LOCAL_C_INCLUDES += \
    $(TOP)/hardware/samsung_slsi-linaro_13-e850-96/exynos/libaudio/audioril-sec \
    $(TOP)/hardware/samsung_slsi-linaro_13-e850-96/exynos/libaudio/audioril-sec/include
endif

LOCAL_C_INCLUDES += \
	$(TOP)/hardware/samsung_slsi-linaro_13-e850-96/exynos/include/libaudio/audiohal

LOCAL_HEADER_LIBRARIES := libhardware_headers
LOCAL_SHARED_LIBRARIES := liblog libcutils libprocessgroup libaudioproxy
LOCAL_SHARED_LIBRARIES += libaudio-ril

ifeq ($(BOARD_USE_SOUNDTRIGGER_HAL),true)
LOCAL_CFLAGS += -DSUPPORT_STHAL_INTERFACE
endif

LOCAL_MODULE := audio.primary.$(TARGET_SOC)
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
endif
