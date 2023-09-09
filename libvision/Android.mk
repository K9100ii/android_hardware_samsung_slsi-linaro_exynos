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
LOCAL_ROOT_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_PRELINK_MODULE := false

LOCAL_SHARED_LIBRARIES:= libutils libcutils libbinder liblog libcamera_client libhardware libui
LOCAL_SHARED_LIBRARIES += libexynosutils libion
LOCAL_SHARED_LIBRARIES += libexpat

LOCAL_CFLAGS += -DDISPLAY_PROCESS_GRAPH_TIME=0

LOCAL_LDLIBS := -llog -ldl

LOCAL_C_INCLUDES += \
	$(TOP)/system/core/libion/include \
	$(TOP)/bionic \
	$(TOP)/external/expat/lib \
	$(TOP)/external/stlport/stlport \
	$(LOCAL_PATH)/include \
	$(LOCAL_PATH)/include/VX \
	$(LOCAL_PATH)/common \
	$(LOCAL_PATH)/system \
	$(LOCAL_PATH)/utils \
	$(LOCAL_PATH)/kernel \
	$(LOCAL_PATH)/kernel/vpu \
	$(LOCAL_PATH)/kernel/vpu/include \
	$(LOCAL_PATH)/kernel/vpu/TaskDescriptor

LOCAL_SRC_FILES:= \
	./api/vx_api.cpp \
	./api/vx_api_ext.cpp \
	./api/vx_cl_ext.cpp \
	./api/vx_node_api.cpp \
	./api/vxu_api.cpp \
	./system/ExynosVisionBufObject.cpp \
	./system/ExynosVisionResManager.cpp \
	./system/ExynosVisionMemoryAllocator.cpp \
	./system/ExynosVisionSubgraph.cpp \
	./common/ExynosVisionContext.cpp \
	./common/ExynosVisionGraph.cpp \
	./common/ExynosVisionTarget.cpp \
	./common/ExynosVisionKernel.cpp \
	./common/ExynosVisionNode.cpp \
	./common/ExynosVisionParameter.cpp \
	./common/ExynosVisionImage.cpp \
	./common/ExynosVisionReference.cpp \
	./common/ExynosVisionPyramid.cpp \
	./common/ExynosVisionDistribution.cpp \
	./common/ExynosVisionConvolution.cpp \
	./common/ExynosVisionThreshold.cpp \
	./common/ExynosVisionDelay.cpp \
	./common/ExynosVisionMatrix.cpp \
	./common/ExynosVisionArray.cpp \
	./common/ExynosVisionScalar.cpp \
	./common/ExynosVisionError.cpp \
	./common/ExynosVisionRemap.cpp \
	./common/ExynosVisionLut.cpp \
	./common/ExynosVisionMeta.cpp \
	./utils/vx_osal.cpp \
	./utils/vx_helper.cpp

$(foreach file,$(LOCAL_SRC_FILES),$(shell touch '$(LOCAL_PATH)/$(file)'))

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libexynosvision

include $(BUILD_SHARED_LIBRARY)

$(warning ##############################################)
$(warning ##############################################)
$(warning #######    Exynos Vision Framework     #######)
$(warning ##############################################)
$(warning ##############################################)

include $(LOCAL_ROOT_PATH)/kernel/vpu/Android.mk
include $(LOCAL_ROOT_PATH)/kernel/score/Android.mk
#include $(LOCAL_ROOT_PATH)/kernel/opencl/Android.mk
