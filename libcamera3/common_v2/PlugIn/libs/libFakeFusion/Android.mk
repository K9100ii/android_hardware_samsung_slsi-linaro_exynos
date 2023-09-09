# Copyright 2017 The Android Open Source Project

LOCAL_PATH := $(call my-dir)

# builtin fakefusion lib
ifeq ($(TARGET_2ND_ARCH),)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := true
LOCAL_PREBUILT_LIBS := lib32/libexynoscamera_fakefusion.so

include $(BUILD_MULTI_PREBUILT)

else
include $(CLEAR_VARS)

LOCAL_MODULE := libexynoscamera_fakefusion
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES_$(TARGET_ARCH) := lib64/$(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX)
LOCAL_SRC_FILES_$(TARGET_2ND_ARCH) := lib32/$(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX)
LOCAL_MULTILIB := both

include $(BUILD_PREBUILT)
endif

# fakefusion plugin
include $(CLEAR_VARS)

LOCAL_SRC_FILES := ExynosCameraPlugInFakeFusion.cpp
LOCAL_SHARED_LIBRARIES := libutils libcutils liblog libexynoscamera_plugin libexynosutils libexynoscamera_fakefusion

LOCAL_MODULE := libexynoscamera_fakefusion_plugin

LOCAL_C_INCLUDES += \
	$(TOP)/hardware/samsung_slsi/exynos/include \
	$(TOP)/hardware/samsung_slsi/exynos/libcamera3/common_v2/ \
	$(TOP)/hardware/samsung_slsi/exynos/libcamera3/common_v2/PlugIn/ \
	$(TOP)/hardware/samsung_slsi/exynos/libcamera3/common_v2/PlugIn/include \
	$(TOP)/hardware/samsung_slsi/exynos/libcamera3/common_v2/PlugIn/libs/include \
	$(TOP)/hardware/samsung_slsi/exynos/libcamera3/common_v2/PlugIn/libs/libFakeFusion/include

LOCAL_CFLAGS := -Wno-unused-parameter
LOCAL_CFLAGS += -Wno-error=date-time

include $(BUILD_SHARED_LIBRARY)

# build sources to make builtin fakefusion lib
# include $(LOCAL_PATH)/src/Android.mk

