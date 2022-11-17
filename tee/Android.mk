#
# Copyright (C) 2017 The Android Open Source Project
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

ifeq ($(BOARD_USES_MOBICORE_TEE),true)
ifndef TARGET_SOC_BASE
TARGET_SOC_BASE = $(TARGET_SOC)
endif

ifeq ($(TARGET_SOC_BASE), exynos9610)
 exynos9610_dirs := \
	 kinibi410

include $(call all-named-subdir-makefiles,$(exynos9610_dirs))

endif

ifeq ($(TARGET_SOC_BASE), exynos850)
 exynos850_dirs := \
	 kinibi500

include $(call all-named-subdir-makefiles,$(exynos850_dirs))

endif

ifeq ($(TARGET_SOC_BASE), exynos9630)
 exynos9630_dirs := \
	 kinibi500

include $(call all-named-subdir-makefiles,$(exynos9630_dirs))

endif
endif # BOARD_USES_MOBICORE_TEE
