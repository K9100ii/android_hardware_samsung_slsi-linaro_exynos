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

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#include "keymint_ta_defs.h"
#include "km_util.h"

#include "exynos_ssp_hwctl.h"

#define SSP_DEVICE          "/dev/ssp"
#define SSP_IOCTL_MAGIC     'c'
#define SSP_IOCTL_INIT      _IO(SSP_IOCTL_MAGIC, 1)
#define SSP_IOCTL_EXIT      _IOWR(SSP_IOCTL_MAGIC, 2, uint64_t)
#define SSP_IOCTL_CHECK		_IOWR(SSP_IOCTL_MAGIC, 3, uint64_t)

#define CM_SSP_INIT     (1)
#define CM_SSP_EXIT     (2)

#define CM_SSP_CHECK_ENABLE (0)

keymaster_error_t exynos_ssp_hwctl_check_hw_status(uint32_t *status)
{
	int ssp_fd = -1;
	keymaster_error_t ret = KM_ERROR_OK;
    int rval;
	uint64_t check_cmd = CM_SSP_CHECK_ENABLE;

	ssp_fd = open(SSP_DEVICE, O_RDWR);
	if (ssp_fd < 0) {
		LOG_E("ssp_hwctl_check_hw_status can't open SSP device file %s\n", SSP_DEVICE);
        *status = SSP_HW_NOT_SUPPORTED;
        ret = KM_ERROR_SECURE_HW_ACCESS_DENIED;
        goto end;
	}

	rval = ioctl(ssp_fd, SSP_IOCTL_CHECK, &check_cmd);
	if (ret < 0) {
        LOG_E("ssp_hwctl_check_hw_status:: ioctl error(%d)\n", rval);
        LOG_E("ssp_hwctl_check_hw_status:: SSP HW is NOT supported\n");
        ret = KM_ERROR_SECURE_HW_ACCESS_DENIED;
        *status = SSP_HW_NOT_SUPPORTED;
        goto end;
	} else if (rval == 0) {
	    LOG_E("ssp_hwctl_check_hw_status:: SSP HW is NOT supported(%d)\n", rval);
        *status = SSP_HW_NOT_SUPPORTED;
    } else {
        LOG_E("ssp_hwctl_check_hw_status:: SSP HW is supported(%d)\n", rval);
        *status = SSP_HW_SUPPORTED;
    }

end:
	if (ssp_fd > 0)
		close(ssp_fd);

	return ret;
}

