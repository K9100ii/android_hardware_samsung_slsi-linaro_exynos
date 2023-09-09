LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

ifeq ($(TARGET_SOC_BASE), exynos9630)
    LIB_PATH := $(LOCAL_PATH)/$(TARGET_SOC_BASE)
else
    LIB_PATH := $(LOCAL_PATH)
endif

LOCAL_MODULE := libhdrinterface_header
LOCAL_PROPRIETARY_MODULE := true

ifeq ($(TARGET_SOC_BASE), exynos9630)
LOCAL_HEADER_LIBRARIES := libhdrinterface_header_exynos9630
LOCAL_EXPORT_HEADER_LIBRARY_HEADERS := libhdrinterface_header_exynos9630
else
LOCAL_HEADER_LIBRARIES := libhdrinterface_header_default
LOCAL_EXPORT_HEADER_LIBRARY_HEADERS := libhdrinterface_header_default
endif
include $(BUILD_HEADER_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libhdrInterface_headers
LOCAL_PROPRIETARY_MODULE := true

LOCAL_EXPORT_C_INCLUDE_DIRS := \
    $(LOCAL_PATH)/exynos9630/include

include $(BUILD_HEADER_LIBRARY)
