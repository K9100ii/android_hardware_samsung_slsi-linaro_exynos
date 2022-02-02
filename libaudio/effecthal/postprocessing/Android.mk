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

#
# Effect HAL for offload
#
ifeq ($(BOARD_USE_OFFLOAD_EFFECT),true)
LOCAL_PATH:= $(call my-dir)

# bundle wrapper for offload
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	Bundle/exynos_effectbundle.cpp

LOCAL_C_INCLUDES += \
	external/tinyalsa/include \
	$(LOCAL_PATH)/Bundle \
	$(call include-path-for, audio-effects)

LOCAL_CFLAGS += -fvisibility=hidden

LOCAL_MODULE:= libbundlewrapper_offload

LOCAL_MODULE_RELATIVE_PATH := soundfx

LOCAL_HEADER_LIBRARIES := libhardware_headers

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libdl \
    libtinyalsa \
    liblog

LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_SHARED_LIBRARY)

# reverb wrapper for offload
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	Reverb/exynos_effectReverb.cpp

LOCAL_CFLAGS += -fvisibility=hidden

LOCAL_MODULE:= libreverbwrapper_offload

LOCAL_MODULE_RELATIVE_PATH := soundfx

LOCAL_HEADER_LIBRARIES := libhardware_headers

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libdl \
    libtinyalsa \
    liblog

LOCAL_C_INCLUDES += \
	external/tinyalsa/include \
	$(LOCAL_PATH)/Reverb \
	$(LOCAL_PATH)/Bundle \
	$(call include-path-for, audio-effects)
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_SHARED_LIBRARY)
endif
