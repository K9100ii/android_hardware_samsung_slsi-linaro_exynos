# Copyright 2017 The Android Open Source Project

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := libvdis.cpp
LOCAL_SHARED_LIBRARIES := libutils libcutils liblog libexynosutils libexynoscamera_plugin_utils

LOCAL_MODULE := libvdis

LOCAL_C_INCLUDES += \
	$(TOP)/hardware/samsung_slsi/exynos/include \
	$(TOP)/hardware/samsung_slsi/exynos/libcamera3/9xxx \
	$(TOP)/hardware/samsung_slsi/exynos/libcamera3/common_v2/PlugIn/include \
	$(LOCAL_PATH)/../include \

LOCAL_CFLAGS := -Wno-unused-parameter
LOCAL_CFLAGS += -Wno-error=date-time

include $(TOP)/hardware/samsung_slsi/exynos/BoardConfigCFlags.mk
include $(BUILD_SHARED_LIBRARY)
