# Copyright 2017 The Android Open Source Project

$(warning #############################################)
$(warning ########       VDIS PlugIn       #############)
$(warning #############################################)

LOCAL_PATH := $(call my-dir)

# builtin vdis lib
ifeq ($(TARGET_2ND_ARCH),)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := true
LOCAL_PREBUILT_LIBS := lib32/libvdis.so

include $(TOP)/hardware/samsung_slsi/exynos/BoardConfigCFlags.mk
include $(BUILD_MULTI_PREBUILT)

else
include $(CLEAR_VARS)

LOCAL_MODULE := libvdis
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES_$(TARGET_ARCH) := lib64/$(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX)
LOCAL_SRC_FILES_$(TARGET_2ND_ARCH) := lib32/$(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX)
LOCAL_MULTILIB := both

include $(TOP)/hardware/samsung_slsi/exynos/BoardConfigCFlags.mk
include $(BUILD_PREBUILT)
endif

# builtin vdis plugin lib
include $(CLEAR_VARS)

LOCAL_SRC_FILES := ExynosCameraPlugInVDIS.cpp
LOCAL_SHARED_LIBRARIES := libutils libcutils liblog libexynoscamera_plugin libexynosutils libvdis

LOCAL_MODULE := libexynoscamera_vdis_plugin

LOCAL_C_INCLUDES += \
	$(TOP)/system/core/libcutils/include \
	$(TOP)/hardware/samsung_slsi/exynos/include \
	$(TOP)/hardware/samsung_slsi/exynos/libcamera3/common_v2/ \
	$(TOP)/hardware/samsung_slsi/exynos/libcamera3/common_v2/PlugIn/ \
	$(TOP)/hardware/samsung_slsi/exynos/libcamera3/common_v2/PlugIn/include \
	$(TOP)/hardware/samsung_slsi/exynos/libcamera3/common_v2/PlugIn/libs/include \
	$(TOP)/hardware/samsung_slsi/exynos/libcamera3/common_v2/PlugIn/libs/libVDIS/include

LOCAL_CFLAGS := -Wno-unused-parameter
LOCAL_CFLAGS += -Wno-error=date-time

include $(TOP)/hardware/samsung_slsi/exynos/BoardConfigCFlags.mk
include $(BUILD_SHARED_LIBRARY)

# build sources to make builtin vdis lib
# include $(LOCAL_PATH)/src/Android.mk
