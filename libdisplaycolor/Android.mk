LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_CLFLAGS += -Wno-unused-function
LOCAL_SRC_FILES := \
    libdisplaycolor_test.cpp

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libutils \
    libcutils \
    libdisplaycolor_default

LOCAL_HEADER_LIBRARIES += libdisplaycolor_interface

LOCAL_MODULE := libdisplaycolor_test
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_EXECUTABLE)
