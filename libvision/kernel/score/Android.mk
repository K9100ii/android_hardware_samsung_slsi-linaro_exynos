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

LOCAL_CPPFLAGS += -frtti

LOCAL_SHARED_LIBRARIES:= libutils libcutils libbinder liblog libcamera_client libhardware libui
LOCAL_SHARED_LIBRARIES += libexynosutils libion
LOCAL_SHARED_LIBRARIES += libexpat
LOCAL_SHARED_LIBRARIES += libexynosvision
LOCAL_PROPRIETARY_MODULE := true

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../../include \
	$(LOCAL_PATH)/../../common \
	$(LOCAL_PATH)/../../system \
	$(LOCAL_PATH)/../../utils \
	$(LOCAL_PATH)/ \
	$(LOCAL_PATH)/api \
	$(LOCAL_PATH)/include

LOCAL_SRC_FILES:= \
	./core/score.cpp \
	./core/score_command.cpp \
	./core/score_core.cpp \
	./core/score_device_real.cpp \
	./core/score_send_packet.cpp \
	./memory/score_ion_memory_manager.cpp \
	./memory/score_memory.cpp \
	./scv/scv_And.cpp \
	./scv/scv_FastCorners.cpp \
	./scv/scv_CannyEdgeDetector.cpp \
	./scv/scv_TableLookup.cpp \
	./scv/scv_Histogram.cpp \
	./scv/scv_Convolution.cpp \
	./api/score_kernel_module.cpp \
	./ExynosScoreKernel.cpp \
	./score_kernel_util.cpp \
	./score_kernel_and.cpp \
	./ExynosScoreKernelAnd.cpp \
	./score_kernel_fastcorners.cpp \
	./ExynosScoreKernelFastCorners.cpp \
	./score_kernel_cannyedge.cpp \
	./ExynosScoreKernelCannyEdge.cpp \
	./score_kernel_lut.cpp \
	./ExynosScoreKernelLut.cpp \
	./score_kernel_histogram.cpp \
	./ExynosScoreKernelHistogram.cpp \
	./score_kernel_convolution.cpp \
	./ExynosScoreKernelConvolution.cpp \

$(foreach file,$(LOCAL_SRC_FILES),$(shell touch '$(LOCAL_PATH)/$(file)'))

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libexynosscorekernel

include $(BUILD_SHARED_LIBRARY)

$(warning ##############################################)
$(warning ##############################################)
$(warning ##########   EVF SCORE Kernel    #############)
$(warning ##############################################)
$(warning ##############################################)

