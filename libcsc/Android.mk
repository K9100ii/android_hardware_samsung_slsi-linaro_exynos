LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true
LOCAL_HEADER_LIBRARIES := libsystem_headers

LOCAL_SRC_FILES := \
	csc.c

LOCAL_C_INCLUDES := \
	hardware/samsung_slsi/$(TARGET_BOARD_PLATFORM)/include \
	$(LOCAL_PATH)/../include \
	$(LOCAL_PATH)/../libexynosutils

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)

LOCAL_CFLAGS :=

LOCAL_MODULE := libcsc

LOCAL_PRELINK_MODULE := false

LOCAL_ARM_MODE := arm

LOCAL_STATIC_LIBRARIES := libswconverter
LOCAL_SHARED_LIBRARIES := liblog libexynosscaler

LOCAL_CFLAGS += -DUSE_SAMSUNG_COLORFORMAT

ifdef BOARD_DEFAULT_CSC_HW_SCALER
LOCAL_CFLAGS += -DDEFAULT_CSC_HW=$(BOARD_DEFAULT_CSC_HW_SCALER)
else
LOCAL_CFLAGS += -DDEFAULT_CSC_HW=5
endif

ifeq ($(BOARD_USES_FIMC), true)
LOCAL_SHARED_LIBRARIES += libexynosfimc
endif

LOCAL_CFLAGS += -Wno-unused-variable -Wno-unused-label

include $(TOP)/hardware/samsung_slsi/exynos/BoardConfigCFlags.mk
include $(BUILD_SHARED_LIBRARY)
