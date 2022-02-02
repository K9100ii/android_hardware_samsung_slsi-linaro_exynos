#/* ExySp : MCD Sync Start */
MOBICORE_PROJECT_PATH := $(call my-dir)

MC_INCLUDE_DIR := $(MOBICORE_PROJECT_PATH)/Daemon/include \
    $(MOBICORE_PROJECT_PATH)/ClientLib/include \
    $(MOBICORE_PROJECT_PATH)/tlcm \
    $(MOBICORE_PROJECT_PATH)/tlcm/TlCm \
    $(MOBICORE_PROJECT_PATH)/tlcm/TlCm/2.0 \
    $(MOBICORE_PROJECT_PATH)/TuiService

MC_DEBUG := _DEBUG
#SYSTEM_LIB_DIR=/system/lib
GDM_PROVLIB_SHARED_LIBS=libMcClient
# /* ExySp : MCD Sync end */

# Some things are specific to Android 9.0 and later
TRUSTONIC_ANDROID_8 =
ifeq ($(PLATFORM_VERSION), 8.0.0)
TRUSTONIC_ANDROID_8 = yes
endif
ifeq ($(PLATFORM_VERSION), 8.1.0)
TRUSTONIC_ANDROID_8 = yes
endif

# Some TUI things are specific to QC BSP (use QCPATH variable as indicator)
ifneq ($(QCPATH),)
# This is a QC BSP
QC_SOURCE_TREE = yes
else
QC_SOURCE_TREE =
endif

include $(call all-subdir-makefiles)
