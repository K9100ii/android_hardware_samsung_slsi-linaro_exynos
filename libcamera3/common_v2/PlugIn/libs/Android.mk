# Copyright 2017 The Android Open Source Project

$(warning #############################################)
$(warning ########       PlugIn           #############)
$(warning #############################################)

plugin_subdirs :=

ifeq ($(BOARD_CAMERA_USES_DUAL_CAMERA_SOLUTION_FAKE), true)
plugin_subdirs := \
    libFakeFusion
endif

ifeq ($(BOARD_CAMERA_USES_DUAL_CAMERA_SOLUTION_ARCSOFT), true)
plugin_subdirs := \
    libArcsoftFusion
endif

ifeq ($(BOARD_CAMERA_USES_LLS_SOLUTION), true)
plugin_subdirs += \
    libLLS
endif

ifeq ($(BOARD_CAMERA_USES_CAMERA_SOLUTION_VDIS), true)
plugin_subdirs += \
    libVDIS
endif

# external plugins
include $(call all-named-subdir-makefiles,$(plugin_subdirs))

