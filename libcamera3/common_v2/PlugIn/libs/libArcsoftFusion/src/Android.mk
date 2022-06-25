# Copyright 2017 The Android Open Source Project

LOCAL_PATH := $(call my-dir)

#ARCSOFT_REAL_LIB = true
#ARCSOFT_BASE_LIB = true

ifeq ($(ARCSOFT_REAL_LIB), true)
# builtin arcsoftfusion lib
#bokeh
ifeq ($(TARGET_2ND_ARCH),)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
LOCAL_PROPRIETARY_MODULE := true
LOCAL_PREBUILT_LIBS := lib32/libarcsoft_dualcam_refocus.so

include $(BUILD_MULTI_PREBUILT)

else
include $(CLEAR_VARS)

LOCAL_MODULE := libarcsoft_dualcam_refocus
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_TAGS := optional

LOCAL_PRELINK_MODULE := false
LOCAL_PROPRIETARY_MODULE := true

LOCAL_SRC_FILES_$(TARGET_ARCH) := lib64/$(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX)
LOCAL_SRC_FILES_$(TARGET_2ND_ARCH) := lib32/$(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX)
LOCAL_MULTILIB := 32

include $(BUILD_PREBUILT)
endif

#zoom
ifeq ($(TARGET_2ND_ARCH),)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
LOCAL_PROPRIETARY_MODULE := true
LOCAL_PREBUILT_LIBS := lib32/libdualcam_optical_zoom.so

include $(BUILD_MULTI_PREBUILT)

else
include $(CLEAR_VARS)

LOCAL_MODULE := libdualcam_optical_zoom
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_TAGS := optional

LOCAL_PRELINK_MODULE := false
LOCAL_PROPRIETARY_MODULE := true

LOCAL_SRC_FILES_$(TARGET_ARCH) := lib64/$(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX)
LOCAL_SRC_FILES_$(TARGET_2ND_ARCH) := lib32/$(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX)
LOCAL_MULTILIB := 32

include $(BUILD_PREBUILT)
endif
endif

ifeq ($(ARCSOFT_BASE_LIB), true)
#mpbase
ifeq ($(TARGET_2ND_ARCH),)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
LOCAL_PROPRIETARY_MODULE := true
LOCAL_PREBUILT_LIBS := lib32/libmpbase.so

include $(BUILD_MULTI_PREBUILT)

else
include $(CLEAR_VARS)

LOCAL_MODULE := libmpbase
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES_$(TARGET_ARCH) := lib64/$(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX)
LOCAL_SRC_FILES_$(TARGET_2ND_ARCH) := lib32/$(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX)
LOCAL_MULTILIB := 32

include $(BUILD_PREBUILT)
endif
endif

include $(CLEAR_VARS)

LOCAL_PRELINK_MODULE := false
LOCAL_PROPRIETARY_MODULE := true

LOCAL_SRC_FILES := \
	ArcsoftFusion.cpp \
	ArcsoftFusionBokehCapture.cpp \
	ArcsoftFusionBokehPreview.cpp \
	ArcsoftFusionZoomCapture.cpp \
	ArcsoftFusionZoomPreview.cpp

LOCAL_SHARED_LIBRARIES := libutils libcutils liblog libexynosutils libion libcsc libacryl

ifeq ($(ARCSOFT_REAL_LIB), true)
LOCAL_SHARED_LIBRARIES += libarcsoft_dualcam_refocus libdualcam_optical_zoom
endif

ifeq ($(ARCSOFT_BASE_LIB), true)
LOCAL_SHARED_LIBRARIES += libmpbase
endif

ifeq ($(FACTORY_BUILD), 1)
LOCAL_SHARED_LIBRARIES += libexynoscamera_arcsoftcali
endif

LOCAL_MODULE := libexynoscamera_arcsoftfusion

LOCAL_C_INCLUDES += \
	$(TOP)/system/core/libion/include \
    $(TOP)/hardware/libhardware/include/ \
	$(TOP)/hardware/samsung_slsi-linaro/exynos/include \
	$(TOP)/hardware/samsung_slsi-linaro/exynos/include/hardware/exynos \
	$(TOP)/hardware/samsung_slsi-linaro/exynos/libcamera3/common_v2/PlugIn/include \
	$(TOP)/hardware/samsung_slsi-linaro/exynos/libcamera3/common_v2/PlugIn/libs/libArcsoftCali/include \
	$(LOCAL_PATH)/../include \

LOCAL_CFLAGS := -Wno-unused-parameter
LOCAL_CFLAGS += -Wno-error=date-time

ifeq ($(FACTORY_BUILD), 1)
LOCAL_CFLAGS += -DUSES_DUAL_CAMERA_CAL_ARCSOFT
LOCAL_CFLAGS += -DFACTORY_BUILD
endif

#LOCAL_MULTILIB := 32
LOCAL_MULTILIB := both

include $(BUILD_SHARED_LIBRARY)
