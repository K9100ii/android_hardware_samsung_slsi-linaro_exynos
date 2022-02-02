#
# build RootPA.apk
#

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(call all-java-files-under, src)
LOCAL_STATIC_JAVA_LIBRARIES := rootpa_interface
ifdef TRUSTONIC_ANDROID_8
LOCAL_PROPRIETARY_MODULE := true
else
LOCAL_STATIC_JAVA_LIBRARIES += TeeClient
endif
LOCAL_JNI_SHARED_LIBRARIES := libcommonpawrapper
LOCAL_AAPT_FLAGS += --extra-packages com.trustonic.teeclient

LOCAL_PACKAGE_NAME := RootPA
# LOCAL_MODULE_TAGS := debug eng optional
LOCAL_CERTIFICATE := platform
LOCAL_PRIVATE_PLATFORM_APIS := true

LOCAL_PROGUARD_FLAGS := -include $(LOCAL_PATH)/proguard-project.txt

APP_PIE := true
LOCAL_DEX_PREOPT := false

include $(BUILD_PACKAGE)

include $(CLEAR_VARS)
