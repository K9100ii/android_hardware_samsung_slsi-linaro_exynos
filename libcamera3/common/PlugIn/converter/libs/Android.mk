# Copyright 2017 The Android Open Source Project

ifeq ($(BOARD_CAMERA_USES_DUAL_CAMERA_SOLUTION_FAKE), true)
LOCAL_CFLAGS += -DUSES_DUAL_CAMERA_SOLUTION_FAKE
LOCAL_C_INCLUDES += \
    $(TOP)/hardware/samsung_slsi-linaro/exynos/libcamera3/common/PlugIn/converter/libs/libFakeFusion
LOCAL_SRC_FILES += \
	../../exynos/libcamera3/common/PlugIn/converter/libs/libFakeFusion/ExynosCameraPlugInConverterFakeFusion.cpp
endif

ifeq ($(BOARD_CAMERA_USES_LLS_SOLUTION), true)
LOCAL_CFLAGS += -DUSES_LLS_SOLUTION
LOCAL_C_INCLUDES += \
    $(TOP)/hardware/samsung_slsi-linaro/exynos/libcamera3/common/PlugIn/converter/libs/libLLS
LOCAL_SRC_FILES += \
	../../exynos/libcamera3/common/PlugIn/converter/libs/libLLS/ExynosCameraPlugInConverterLowLightShot.cpp
endif


