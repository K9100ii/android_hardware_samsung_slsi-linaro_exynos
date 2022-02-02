# Copyright (C) 2016 The Android Open Source Project
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
# VTS Unit Test Console application
#
ifeq ($(BOARD_USE_SOUNDTRIGGER_HAL), STHAL_TEST)
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := vtshw-test

LOCAL_SRC_FILES := \
    vts_hw_test.c

LOCAL_C_INCLUDES += \
    external/tinyalsa/include \
    $(TOP)/hardware/samsung_slsi/exynos/libaudio/sthal

LOCAL_C_INCLUDES += \
    $(TOP)/device/samsung/$(TARGET_BOOTLOADER_BOARD_NAME)/conf

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libcutils \
    libtinyalsa

LOCAL_CFLAGS += -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function

ifeq ($(BOARD_USE_SOUNDTRIGGER_HAL_MMAP),true)
LOCAL_CFLAGS += -DMMAP_INTERFACE_ENABLED
endif

LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)
endif
