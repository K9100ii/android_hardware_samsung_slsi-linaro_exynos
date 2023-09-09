/*
 **
 ** Copyright 2021, The Android Open Source Project
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
 ** LINT_KERNEL_FILE
 */

#ifndef __WEAVER__UTIL__H
#define __WEAVER__UTIL__H

#include <log/log.h>
#define LOG_E ALOGE
#define LOG_I ALOGI
#define LOG_D ALOGD

#define EXPECT_WEAVER_OK_GOTOEND(expr) \
	do { \
		ret = (expr); \
		if (ret != WEAVER_ERROR_OK) { \
			LOG_E("%s == %d", #expr, ret); \
			goto end; \
		} \
	} while (0)

#define CHECK_SSP_WEAVER_MSG_GOTOEND(ssp_msg) \
	do { \
		ret = ((weaver_error_t)ssp_msg.response.weaver_errcode); \
		if (ret != WEAVER_ERROR_OK) { \
			LOG_E("%s failed. ret(%x), cm_errno(0x%08x)\n", \
					__func__, ret, ssp_msg.response.cm_errno); \
			goto end; \
		} \
	} while (0)

#define EXPECT_WEAVER_TRUE_GOTOEND(errcode, expr) \
	do { \
		if (!(expr)) { \
			LOG_E("'%s' is false\n", #expr); \
			ret = (errcode); \
			goto end; \
		} \
	} while (false)

#define CHECK_RESULT(expr) \
	do { \
		ret = (expr); \
		if (ret != WEAVER_ERROR_OK) { \
			goto err; \
		} \
	} while (false)

#endif  // __WEAVER__UTIL__H

