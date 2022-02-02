# Copyright (C) 2015 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_PRELINK_MODULE := false

LOCAL_SHARED_LIBRARIES:= libutils libcutils libbinder liblog libcamera_client libhardware libui
LOCAL_SHARED_LIBRARIES += libexynosutils libion
LOCAL_SHARED_LIBRARIES += libexpat
LOCAL_SHARED_LIBRARIES += libpower
LOCAL_SHARED_LIBRARIES += libexynosvision

LOCAL_LDLIBS := -landroid -llog -ldl -L$(TARGET_OUT_VENDOR)/lib/egl -lGLES_mali

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../../include \
	$(LOCAL_PATH)/../../common \
	$(LOCAL_PATH)/../../system \
	$(LOCAL_PATH)/../../utils \
	$(LOCAL_PATH)/ \

LOCAL_SRC_FILES:= \
        ./vxcl_kernel_module.cpp \
        ./vxcl_kernel_extra.cpp \
        ./vxcl_kernel_util.cpp \
        ./vx_remap.cpp \
        ./vx_warp.cpp \
        ./vx_warp_rgb.cpp \

$(foreach file,$(LOCAL_SRC_FILES),$(shell touch '$(LOCAL_PATH)/$(file)'))

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libopenvx-opencl

LOCAL_POST_INSTALL_CMD := cp -r $(LOCAL_PATH)/cl/ $(TARGET_OUT)/usr/vxcl/

include $(BUILD_SHARED_LIBRARY)

$(warning ##############################################)
$(warning ##############################################)
$(warning #######    openvx-opencl Kernel     ##########)
$(warning ##############################################)
$(warning ##############################################)

