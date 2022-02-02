LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := public.libraries-trustonic.txt
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_SYSTEM_ETC)
LOCAL_SRC_FILES := public.libraries-trustonic.txt
include $(BUILD_PREBUILT)
