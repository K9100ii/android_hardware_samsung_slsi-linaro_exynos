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

#include "ssp_strongbox_keymaster_log.h"
#include "ssp_strongbox_keymaster_defs.h"
#include "ssp_strongbox_keymaster_hwctl.h"

#define SSP_DEVICE          "/dev/ssp"
#define SSP_IOCTL_MAGIC     'c'
#define SSP_IOCTL_INIT      _IO(SSP_IOCTL_MAGIC, 1)
#define SSP_IOCTL_EXIT      _IOWR(SSP_IOCTL_MAGIC, 2, uint64_t)
#define SSP_IOCTL_CHECK		_IOWR(SSP_IOCTL_MAGIC, 3, uint64_t)

#define CM_SSP_INIT     (1)
#define CM_SSP_EXIT     (2)

#define CM_SSP_CHECK_ENABLE (0)

static int refcount;

keymaster_error_t ssp_hwctl_enable(void)
{
    int ssp_fd = -1;
    keymaster_error_t ret = KM_ERROR_OK;
    int rval;

    ssp_fd = open(SSP_DEVICE, O_RDWR);
    if (ssp_fd < 0) {
        BLOGE("ssp_hwctl_enable can't open SSP device file: %s\n", SSP_DEVICE);
        ret = KM_ERROR_SECURE_HW_ACCESS_DENIED;
        goto end;
    }

    BLOGD("ssp_hwctl_enable fd: %d\n", ssp_fd);

    rval = ioctl(ssp_fd, SSP_IOCTL_INIT);
    if (rval < 0) {
        BLOGE("SSP init fail with ret: %d\n", rval);
        ret = KM_ERROR_SECURE_HW_ACCESS_DENIED;
        goto end;
    }

    BLOGI("ssp_hwctl_enable done. refcount(%d)\n", ++refcount);

end:
    if (ssp_fd > 0)
        close(ssp_fd);

    return ret;
}

keymaster_error_t ssp_hwctl_disable(void)
{
    int ssp_fd = -1;
    keymaster_error_t ret = KM_ERROR_OK;
    int rval;
    uint64_t pm_mode = 0;

    if (refcount <= 0) {
        BLOGI("ssp_hwctl_disable is already done. refcount(%d)\n", refcount);
        return ret;
    }

    ssp_fd = open(SSP_DEVICE, O_RDWR);
    if (ssp_fd < 0) {
        BLOGE("ssp_hwctl_disable can't open SSP device file %s\n", SSP_DEVICE);
        ret = KM_ERROR_SECURE_HW_ACCESS_DENIED;
        goto end;
    }

    BLOGD("ssp_hwctl_disable fd %d\n", ssp_fd);

    rval = ioctl(ssp_fd, SSP_IOCTL_EXIT, &pm_mode);
    if (rval < 0) {
        BLOGE("ssp_hwctl_disable fail with rval: %d\n", rval);
        ret = KM_ERROR_SECURE_HW_ACCESS_DENIED;
        goto end;
    }

	BLOGI("ssp_hwctl_disable done. refcount(%d)\n", --refcount);
end:
    if (ssp_fd > 0)
        close(ssp_fd);

    return ret;
}

keymaster_error_t ssp_hwctl_check_hw_status(uint32_t *status)
{
	int ssp_fd = -1;
	keymaster_error_t ret = KM_ERROR_OK;
    int rval;
	uint64_t check_cmd = CM_SSP_CHECK_ENABLE;

	ssp_fd = open(SSP_DEVICE, O_RDWR);
	if (ssp_fd < 0) {
		BLOGE("ssp_hwctl_check_hw_status can't open SSP device file %s\n", SSP_DEVICE);
        *status = SSP_HW_NOT_SUPPORTED;
        ret = KM_ERROR_SECURE_HW_ACCESS_DENIED;
        goto end;
	}

	rval = ioctl(ssp_fd, SSP_IOCTL_CHECK, &check_cmd);
	if (ret < 0) {
        BLOGE("ssp_hwctl_check_hw_status:: ioctl error(%d)\n", rval);
        BLOGE("ssp_hwctl_check_hw_status:: SSP HW is NOT supported\n");
        ret = KM_ERROR_SECURE_HW_ACCESS_DENIED;
        *status = SSP_HW_NOT_SUPPORTED;
        goto end;
	} else if (rval == 0) {
	    BLOGE("ssp_hwctl_check_hw_status:: SSP HW is NOT supported(%d)\n", rval);
        *status = SSP_HW_NOT_SUPPORTED;
    } else {
        BLOGE("ssp_hwctl_check_hw_status:: SSP HW is supported(%d)\n", rval);
        *status = SSP_HW_SUPPORTED;
    }

end:
	if (ssp_fd > 0)
		close(ssp_fd);

	return ret;
}

