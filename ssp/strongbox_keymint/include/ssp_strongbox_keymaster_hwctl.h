/*
 **
 ** Copyright 2019, The Android Open Source Project
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **     http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */

#ifndef _SSP_STRONGBOX_KEYMASTER_HWCTL_
#define _SSP_STRONGBOX_KEYMASTER_HWCTL_

#define SSP_HW_SUPPORTED	(1)
#define SSP_HW_NOT_SUPPORTED	(0)

keymaster_error_t ssp_hwctl_enable(void);
keymaster_error_t ssp_hwctl_disable(void);
keymaster_error_t ssp_hwctl_check_hw_status(uint32_t *status);


#endif

