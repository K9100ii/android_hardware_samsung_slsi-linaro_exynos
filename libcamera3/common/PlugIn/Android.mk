# Copyright 2017 The Android Open Source Project

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := ExynosCameraPlugIn.cpp
LOCAL_SHARED_LIBRARIES := libutils libcutils liblog

LOCAL_MODULE := libexynoscamera_plugin

LOCAL_C_INCLUDES += \
    $(TOP)/hardware/samsung_slsi-linaro/exynos/libcamera/common_v3 \
    $(TOP)/hardware/samsung_slsi-linaro/exynos/libcamera3/common/include \
    $(TOP)/hardware/samsung_slsi-linaro/exynos/libcamera3/common/PlugIn \
    $(TOP)/hardware/samsung_slsi-linaro/exynos/libcamera3/common/PlugIn/include \
	$(TOP)/bionic

LOCAL_CFLAGS := -Wno-unused-parameter
LOCAL_CFLAGS += -Wno-error=date-time
LOCAL_CFLAGS += -Wno-overloaded-virtual

include $(TOP)/hardware/samsung_slsi-linaro/exynos/BoardConfigCFlags.mk
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := ExynosCameraPlugInUtils.cpp
LOCAL_SHARED_LIBRARIES := libutils libcutils liblog libion

LOCAL_MODULE := libexynoscamera_plugin_utils

LOCAL_C_INCLUDES += \
    $(TOP)/hardware/samsung_slsi-linaro/exynos/libcamera/common_v3 \
    $(TOP)/hardware/samsung_slsi-linaro/exynos/libcamera3/common/include \
    $(TOP)/hardware/samsung_slsi-linaro/exynos/libcamera3/common/PlugIn \
    $(TOP)/hardware/samsung_slsi-linaro/exynos/libcamera3/common/PlugIn/include \
	$(TOP)/system/core/libion/include \
	$(TOP)/bionic

LOCAL_CFLAGS := -Wno-unused-parameter
LOCAL_CFLAGS += -Wno-error=date-time
LOCAL_CFLAGS += -Wno-overloaded-virtual

include $(TOP)/hardware/samsung_slsi-linaro/exynos/BoardConfigCFlags.mk
include $(BUILD_SHARED_LIBRARY)

# external plugins
include $(LOCAL_PATH)/libs/Android.mk

