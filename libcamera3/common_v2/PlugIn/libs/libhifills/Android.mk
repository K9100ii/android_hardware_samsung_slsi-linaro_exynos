# Copyright 2017 The Android Open Source Project

$(warning #############################################)
$(warning ########       VDIS PlugIn       #############)
$(warning #############################################)

LOCAL_PATH := $(call my-dir)

# builtin vdis plugin lib
include $(CLEAR_VARS)

LOCAL_SRC_FILES := ExynosCameraPlugInHIFILLS.cpp
LOCAL_SHARED_LIBRARIES := libutils libcutils liblog libexynoscamera_plugin libexynosutils

LOCAL_MODULE := libexynoscamera_hifills_plugin

LOCAL_C_INCLUDES += \
	$(TOP)/system/core/libcutils/include \
	$(TOP)/hardware/samsung_slsi-linaro/exynos/include \
	$(TOP)/hardware/samsung_slsi-linaro/exynos/libcamera3/common_v2/ \
	$(TOP)/hardware/samsung_slsi-linaro/exynos/libcamera3/common_v2/PlugIn/ \
	$(TOP)/hardware/samsung_slsi-linaro/exynos/libcamera3/common_v2/PlugIn/include \
	$(TOP)/hardware/samsung_slsi-linaro/exynos/libcamera3/common_v2/PlugIn/libs/include \
	$(TOP)/hardware/samsung_slsi-linaro/exynos/libcamera3/common_v2/PlugIn/libs/libhifills/include

LOCAL_CFLAGS := -Wno-unused-parameter
LOCAL_CFLAGS += -Wno-error=date-time

include $(TOP)/hardware/samsung_slsi-linaro/exynos/BoardConfigCFlags.mk
include $(BUILD_SHARED_LIBRARY)

# build sources to make builtin vdis lib
# include $(LOCAL_PATH)/src/Android.mk
