#
# Copyright (C) 2012 The Android Open Source Project
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
#
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := libOpenCL_symlink
LOCAL_MODULE_TAGS := optional
LOCAL_VENDOR_MODULE := true
LOCAL_POST_INSTALL_CMD := \
	mkdir -p $(TARGET_OUT_VENDOR_SHARED_LIBRARIES); \
	mkdir -p $(2ND_TARGET_OUT_VENDOR_SHARED_LIBRARIES); \
	ln -sf /vendor/lib64/egl/libGLES_mali.so $(TARGET_OUT_VENDOR_SHARED_LIBRARIES)/libGLES_mali.so; \
	ln -sf /vendor/lib/egl/libGLES_mali.so $(2ND_TARGET_OUT_VENDOR_SHARED_LIBRARIES)/libGLES_mali.so;

include $(BUILD_SHARED_LIBRARY)
