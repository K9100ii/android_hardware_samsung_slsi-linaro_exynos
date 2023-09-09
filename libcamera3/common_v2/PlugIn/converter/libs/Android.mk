# Copyright 2017 The Android Open Source Project

ifeq ($(BOARD_CAMERA_USES_DUAL_CAMERA_SOLUTION_FAKE), true)
LOCAL_CFLAGS += -DUSES_DUAL_CAMERA_SOLUTION_FAKE
LOCAL_C_INCLUDES += \
    $(TOP)/hardware/samsung_slsi/exynos/libcamera3/common_v2/PlugIn/converter/libs/libFakeFusion
LOCAL_SRC_FILES += \
	../../exynos/libcamera3/common_v2/PlugIn/converter/libs/libFakeFusion/ExynosCameraPlugInConverterFakeFusion.cpp
endif

ifeq ($(BOARD_CAMERA_USES_DUAL_CAMERA_SOLUTION_ARCSOFT), true)
LOCAL_CFLAGS += -DUSES_DUAL_CAMERA_SOLUTION_ARCSOFT
LOCAL_C_INCLUDES += \
    $(TOP)/hardware/samsung_slsi/exynos/libcamera/common_v3/PlugIn/converter/libs/libArcsoftFusion
LOCAL_SRC_FILES += \
	../../exynos/libcamera3/common_v2/PlugIn/converter/libs/libArcsoftFusion/ExynosCameraPlugInConverterArcsoftFusion.cpp \
	../../exynos/libcamera3/common_v2/PlugIn/converter/libs/libArcsoftFusion/ExynosCameraPlugInConverterArcsoftFusionBokehCapture.cpp \
	../../exynos/libcamera3/common_v2/PlugIn/converter/libs/libArcsoftFusion/ExynosCameraPlugInConverterArcsoftFusionBokehPreview.cpp \
	../../exynos/libcamera3/common_v2/PlugIn/converter/libs/libArcsoftFusion/ExynosCameraPlugInConverterArcsoftFusionZoomCapture.cpp \
	../../exynos/libcamera3/common_v2/PlugIn/converter/libs/libArcsoftFusion/ExynosCameraPlugInConverterArcsoftFusionZoomPreview.cpp
endif

ifeq ($(BOARD_CAMERA_USES_LLS_SOLUTION), true)
LOCAL_CFLAGS += -DUSES_LLS_SOLUTION
LOCAL_C_INCLUDES += \
    $(TOP)/hardware/samsung_slsi/exynos/libcamera3/common_v2/PlugIn/converter/libs/libLLS
LOCAL_SRC_FILES += \
	../../exynos/libcamera3/common_v2/PlugIn/converter/libs/libLLS/ExynosCameraPlugInConverterLowLightShot.cpp
endif

ifeq ($(BOARD_CAMERA_USES_CAMERA_SOLUTION_VDIS), true)
LOCAL_CFLAGS += -DUSES_CAMERA_SOLUTION_VDIS
LOCAL_C_INCLUDES += \
    $(TOP)/hardware/samsung_slsi/exynos/libcamera3/common_v2/PlugIn/converter/libs/libVDIS
LOCAL_SRC_FILES += \
	../../exynos/libcamera3/common_v2/PlugIn/converter/libs/libVDIS/ExynosCameraPlugInConverterVDIS.cpp
endif
