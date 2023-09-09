# Copyright 2017 The Android Open Source Project

ifeq ($(BOARD_CAMERA_BUILD_SOLUTION_SRC), true)
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := LowLightShot.cpp
LOCAL_SHARED_LIBRARIES := libutils libcutils liblog libexynosutils libexynoscamera_plugin_utils libexynosv4l2

LOCAL_MODULE := liblowlightshot

LOCAL_C_INCLUDES += \
	$(TOP)/hardware/samsung_slsi/exynos/include \
	$(TOP)/hardware/samsung_slsi/exynos/libcamera3/9810 \
	$(TOP)/hardware/samsung_slsi/exynos/libcamera3/common/PlugIn/include \
	$(LOCAL_PATH)/../include \

LOCAL_CFLAGS := -Wno-unused-parameter
LOCAL_CFLAGS += -Wno-error=date-time

include $(TOP)/hardware/samsung_slsi/exynos/BoardConfigCFlags.mk
include $(BUILD_SHARED_LIBRARY)
endif
