LOCAL_PATH	:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := caKeyInjectionTool
LOCAL_PROPRIETARY_MODULE := true

# Enable logging to logcat per default
LOCAL_CFLAGS += -DLOG_ANDROID
LOCAL_CFLAGS += -DTBASE_API_LEVEL=11

LOCAL_SRC_FILES += \
	src/main.cpp \
	src/caKeyInjection.cpp

LOCAL_SHARED_LIBRARIES := libMcClient

ifeq ($(APP_PROJECT_PATH),)
LOCAL_SHARED_LIBRARIES +=  liblog
else # !NDK
LOCAL_LDLIBS := -llog
endif # NDK

include $(BUILD_EXECUTABLE)

# =============================================================================

# adding the root folder to the search path appears to make absolute paths
# work for import-module - lets see how long this works and what surprises
# future developers get from this.
$(call import-add-path,/)
$(call import-module,$(COMP_PATH_MobiCoreClientLib_module))
