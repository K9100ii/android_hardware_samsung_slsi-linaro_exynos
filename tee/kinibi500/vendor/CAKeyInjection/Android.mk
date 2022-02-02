LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

# Module name (sets name of output binary / library)
LOCAL_MODULE := caKeyInjectionTool
LOCAL_PROPRIETARY_MODULE := true

FILE_LIST := $(wildcard $(LOCAL_PATH)/src/*.c $(LOCAL_PATH)/src/*.cpp)
LOCAL_SRC_FILES := $(FILE_LIST:$(LOCAL_PATH)/%=%)

LOCAL_C_INCLUDES := include src .

LOCAL_CFLAGS := -Wall -Wextra -Werror
LOCAL_CFLAGS += -DTBASE_API_LEVEL=11

# Enable logging to logcat by default
LOCAL_CFLAGS += -DLOG_ANDROID
LOCAL_CFLAGS += -DLOG_TAG=\"caKeyInjectionTool\"

LOCAL_SHARED_LIBRARIES := libMcClient

ifeq ($(APP_PROJECT_PATH),)
LOCAL_SHARED_LIBRARIES += liblog
else
LOCAL_LDLIBS := -llog
endif

include $(BUILD_EXECUTABLE)

# adding the root folder to the search path appears to make absolute paths
# work for import-module - lets see how long this works and what surprises
# future developers get from this.
$(call import-add-path,/)
$(call import-module,$(COMP_PATH_MobiCoreClientLib_module))
