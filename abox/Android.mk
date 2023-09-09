LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := main_abox.cpp
LOCAL_MODULE := main_abox
LOCAL_SHARED_LIBRARIES := libc libcutils liblog
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true
include $(BUILD_EXECUTABLE)
