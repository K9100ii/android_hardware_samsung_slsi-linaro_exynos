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
LOCAL_SHARED_LIBRARIES += libexynosvision
LOCAL_PROPRIETARY_MODULE := true

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../../include \
	$(LOCAL_PATH)/../../common \
	$(LOCAL_PATH)/../../system \
	$(LOCAL_PATH)/../../utils \
	$(LOCAL_PATH)/ \
	$(LOCAL_PATH)/api \
	$(LOCAL_PATH)/include \
	$(LOCAL_PATH)/TaskDescriptor

LOCAL_SRC_FILES:= \
	./ExynosVpuKernel.cpp \
	./ExynosVpuTaskIf.cpp \
	./ExynosVpuDrvNode.cpp \
	./exynos_vpu_driver.c \
	./vpu_kernel_util.cpp \
	./api/vpu_kernel_module.cpp \
	./api/vpu_kernel_extra.cpp \
	./api/vpu_kernel_internal.cpp \
	./test/vpu_test_api.cpp \
	./TaskDescriptor/ExynosVpuVertex.cpp \
	./TaskDescriptor/ExynosVpuProcess.cpp \
	./TaskDescriptor/ExynosVpu3dnnProcess.cpp \
	./TaskDescriptor/ExynosVpu3dnnProcessBase.cpp \
	./TaskDescriptor/ExynosVpuSubchain.cpp \
	./TaskDescriptor/ExynosVpuPu.cpp \
	./TaskDescriptor/ExynosVpuCpuOp.cpp \
	./TaskDescriptor/ExynosVpuTask.cpp \
	./vpu_kernel_colorconv.cpp \
	./ExynosVpuKernelColorConv.cpp \
	./vpu_kernel_channelextract.cpp \
	./ExynosVpuKernelChannelExtract.cpp \
	./vpu_kernel_channelcomb.cpp \
	./ExynosVpuKernelChannelComb.cpp \
	./vpu_kernel_sobel.cpp \
	./ExynosVpuKernelSobel.cpp \
	./vpu_kernel_magnitude.cpp \
	./ExynosVpuKernelMagnitude.cpp \
	./vpu_kernel_phase.cpp \
	./ExynosVpuKernelPhase.cpp \
	./vpu_kernel_scaleimage.cpp \
	./ExynosVpuKernelScaleImage.cpp \
	./vpu_kernel_lut.cpp \
	./ExynosVpuKernelLut.cpp \
	./vpu_kernel_histogram.cpp \
	./ExynosVpuKernelHistogram.cpp \
	./vpu_kernel_equalizehist.cpp \
	./ExynosVpuKernelEqualizeHist.cpp \
	./vpu_kernel_absdiff.cpp \
	./ExynosVpuKernelAbsDiff.cpp \
	./vpu_kernel_meanstddev.cpp \
	./ExynosVpuKernelMeanStdDev.cpp \
	./vpu_kernel_threshold.cpp \
	./ExynosVpuKernelThreshold.cpp \
	./vpu_kernel_integralimage.cpp \
	./ExynosVpuKernelIntegralImage.cpp \
	./vpu_kernel_dilate.cpp \
	./ExynosVpuKernelDilate.cpp \
	./vpu_kernel_erode.cpp \
	./ExynosVpuKernelErode.cpp \
	./vpu_kernel_filter.cpp \
	./ExynosVpuKernelFilter.cpp \
	./vpu_kernel_convolution.cpp \
	./ExynosVpuKernelConvolution.cpp \
	./vpu_kernel_pyramid.cpp \
	./ExynosVpuKernelPyramid.cpp \
	./vpu_kernel_accum.cpp \
	./ExynosVpuKernelAccum.cpp \
	./vpu_kernel_accumscale.cpp \
	./ExynosVpuKernelAccumScale.cpp \
	./vpu_kernel_minmaxloc.cpp \
	./ExynosVpuKernelMinMaxLoc.cpp \
	./vpu_kernel_convertdepth.cpp \
	./ExynosVpuKernelConvertDepth.cpp \
	./vpu_kernel_cannyedge.cpp \
	./ExynosVpuKernelCannyEdge.cpp \
	./vpu_kernel_bitwisebinary.cpp \
	./ExynosVpuKernelBitwiseBinary.cpp \
	./vpu_kernel_bitwiseunary.cpp \
	./ExynosVpuKernelBitwiseUnary.cpp \
	./vpu_kernel_multiply.cpp \
	./ExynosVpuKernelMultiply.cpp \
	./vpu_kernel_arithmetic.cpp \
	./ExynosVpuKernelArithmetic.cpp \
	./vpu_kernel_harris.cpp \
	./ExynosVpuKernelHarris.cpp \
	./vpu_kernel_fastcorners.cpp \
	./ExynosVpuKernelFastCorners.cpp \
	./vpu_kernel_optpyrlk.cpp \
	./ExynosVpuKernelOptPyrLk.cpp \
	./vpu_kernel_halfscalegaussian.cpp \
	./ExynosVpuKernelHalfScaleGaussian.cpp \
	./vpu_kernel_saitfr.cpp \
	./ExynosVpuKernelSaitFr.cpp \
	./micro_kernel/vpu_micro_kernel_fastcorners.cpp \
	./micro_kernel/ExynosVpuMicroKernelFastCorners.cpp \

$(foreach file,$(LOCAL_SRC_FILES),$(shell touch '$(LOCAL_PATH)/$(file)'))

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libexynosvpukernel

include $(BUILD_SHARED_LIBRARY)

$(warning ##############################################)
$(warning ##############################################)
$(warning ##########    EVF VPU Kernel     #############)
$(warning ##############################################)
$(warning ##############################################)

