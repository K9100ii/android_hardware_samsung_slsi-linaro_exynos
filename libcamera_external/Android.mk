# Copyright (C) 2012 The Android Open Source Project
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

LOCAL_PRELINK_MODULE := false
LOCAL_PROPRIETARY_MODULE := true

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../include \
	$(TOP)/hardware/samsung_slsi/exynos/include \
	$(TOP)/hardware/samsung_slsi/exynos/libcamera_external \
	$(TOP)/hardware/samsung_slsi/$(TARGET_BOARD_PLATFORM)/include \
	$(TOP)/hardware/samsung_slsi/$(TARGET_SOC)/include \
	$(TOP)/hardware/samsung_slsi/$(TARGET_SOC)/libcamera \
	$(TOP)/hardware/samsung_slsi/$(TARGET_SOC)/libcamera_external \
	frameworks/native/include \
	system/media/camera/include

LOCAL_SRC_FILES:= \
	Exif.cpp \
	SecCameraParameters.cpp \
	ISecCameraHardware.cpp \
	SecCameraInterface.cpp \
	SecCameraHardware.cpp

LOCAL_SHARED_LIBRARIES:= libutils libcutils libbinder liblog libcamera_client libhardware

LOCAL_CFLAGS += -DGAIA_FW_BETA

ifeq ($(BOARD_USE_MHB_ION), true)
LOCAL_CFLAGS += -DBOARD_USE_MHB_ION
endif

ifeq ($(BOARD_BACK_CAMERA_USES_EXTERNAL_CAMERA), true)
	LOCAL_CFLAGS += -DBOARD_BACK_CAMERA_USES_EXTERNAL_CAMERA
endif

ifeq ($(BOARD_FRONT_CAMERA_USES_EXTERNAL_CAMERA), true)
	LOCAL_CFLAGS += -DBOARD_FRONT_CAMERA_USES_EXTERNAL_CAMERA
endif

LOCAL_SHARED_LIBRARIES += libexynosutils libhwjpeg libexynosv4l2 libcsc libion_exynos libcamera_metadata

LOCAL_MODULE := libexynoscameraexternal

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
