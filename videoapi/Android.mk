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

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true

LOCAL_CFLAGS :=

ifeq ($(BOARD_USE_FULL_ST2094_40), true)
LOCAL_CFLAGS += -DUSE_FULL_ST2094_40
endif

ifeq ($(BOARD_USE_HDR10PLUS_STAT_ENC), true)
LOCAL_CFLAGS += -DUSE_FULL_ST2094_40
endif

LOCAL_SRC_FILES := \
	VendorVideoAPI.cpp \
	GenerateSei.cpp

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/../include

LOCAL_SHARED_LIBRARIES := \
	liblog \
	libutils

LOCAL_MODULE := libVendorVideoApi

LOCAL_PRELINK_MODULE := false
LOCAL_ARM_MODE := arm

LOCAL_CFLAGS += -Werror -Wno-unused-parameter -Wno-unused-function

include $(BUILD_STATIC_LIBRARY)
