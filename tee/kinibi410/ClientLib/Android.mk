TRUSTONIC_LEGACY_BUILD =

ifdef TRUSTONIC_ANDROID_8
TRUSTONIC_LEGACY_BUILD = yes
endif

ifneq ($(APP_PROJECT_PATH),)
TRUSTONIC_LEGACY_BUILD = yes
endif

ifdef TRUSTONIC_LEGACY_BUILD

LOCAL_PATH := $(call my-dir)

# Client lib

include $(CLEAR_VARS)

LOCAL_MODULE := libMcClient
# LOCAL_MODULE_TAGS := eng
LOCAL_PROPRIETARY_MODULE := true

LOCAL_CFLAGS := -fvisibility=hidden
LOCAL_CFLAGS += -DTBASE_API_LEVEL=9
LOCAL_CFLAGS += -Wall -Wextra -Werror
LOCAL_CFLAGS += -std=c++11

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include \
	$(LOCAL_PATH)/include/GP

ifeq ($(APP_PROJECT_PATH),)
LOCAL_SHARED_LIBRARIES := \
	liblog

else # !NDK
LOCAL_LDLIBS := -llog

LOCAL_CFLAGS += -static-libstdc++
endif # NDK

LOCAL_SRC_FILES := \
	src/client_log.cpp \
	src/common.cpp \
	src/driver.cpp \
	src/native_interface.cpp

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_C_INCLUDES)
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_EXPORT_C_INCLUDES)

include $(BUILD_SHARED_LIBRARY)

# Static version of the client lib for recovery

include $(CLEAR_VARS)

LOCAL_MODULE := libMcClient_static
# LOCAL_MODULE_TAGS := eng
LOCAL_PROPRIETARY_MODULE := true

LOCAL_CFLAGS := -fvisibility=hidden
LOCAL_CFLAGS += -DTBASE_API_LEVEL=5
LOCAL_CFLAGS += -Wall -Wextra -Werror
LOCAL_CFLAGS += -std=c++11

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include \
	$(LOCAL_PATH)/include/GP

LOCAL_SRC_FILES := \
	src/client_log.cpp \
	src/common.cpp \
	src/driver.cpp \
	src/native_interface.cpp

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_C_INCLUDES)
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_EXPORT_C_INCLUDES)

include $(BUILD_STATIC_LIBRARY)

endif # TRUSTONIC_LEGACY_BUILD
