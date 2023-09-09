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

#define LOG_TAG "android.hardware.weaver@1.0-impl"

#include "weaver_device.h"
#include <android-base/logging.h>
#include "weaver_util.h"
#include <string.h>

#include <iostream>

namespace android {
namespace hardware {
namespace weaver {
namespace V1_0 {
namespace implementation {

	static WeaverConfig convert_weaver_config(weaver_config config)
	{
		WeaverConfig ret = { config.slots, config.keySize, config.valueSize };
		return ret;
	}

	static WeaverStatus convert_weaver_status(weaver_status status)
	{
		WeaverStatus ret = WeaverStatus::OK;
		switch (status) {
		case WEAVER_STATUS_OK :
			ret = WeaverStatus::OK;
			break;
		case WEAVER_STATUS_FAILED :
			ret = WeaverStatus::FAILED;
			break;
		default:
			LOG_E("weaver_status not finded");
			ret = WeaverStatus::FAILED;
			break;
		}
		return ret;
	}

	static WeaverReadStatus convert_weaver_read_status(weaver_read_status status)
	{
		WeaverReadStatus ret = WeaverReadStatus::OK;
		switch (status) {
		case WEAVER_READ_OK :
			ret = WeaverReadStatus::OK;
			break;
		case WEAVER_READ_FAILED:
			ret = WeaverReadStatus::FAILED;
			break;
		case WEAVER_READ_INCORRECT_KEY:
			ret = WeaverReadStatus::INCORRECT_KEY;
			break;
		case WEAVER_READ_THROTTLE:
			ret = WeaverReadStatus::THROTTLE;
			break;
		default:
			LOG_E("convert_weaver_read_status not finded");
			ret = WeaverReadStatus::FAILED;
			break;
		}
		return ret;
	}

	static WeaverReadResponse convert_weaver_read_response(weaver_read_response rsp)
	{
		WeaverReadResponse ret = { rsp.timeout, {0, 0}};
		ret.value.setToExternal(const_cast<uint8_t*>(rsp.value.data), rsp.value.length);
		return ret;
	}

	WeaverDevice::~WeaverDevice() {}

	Return<void> WeaverDevice::getConfig(IWeaver::getConfig_cb _hidl_cb)
	{
		weaver_config config = { 0, 0, 0 };
		weaver_status status = WEAVER_STATUS_FAILED;

		impl_->getConfig(status, config);

		WeaverConfig ret_config = convert_weaver_config(config);
		WeaverStatus ret_status = convert_weaver_status(status);
		_hidl_cb(ret_status, ret_config);

		return Void();
	}

	Return<WeaverStatus> WeaverDevice::write(uint32_t slotId,
		const hidl_vec<uint8_t>& key,
		const hidl_vec<uint8_t>& value)
	{
		WeaverStatus ret_status = convert_weaver_status(impl_->write(slotId, key, value));
		return ret_status;
	}

	Return<void> WeaverDevice::read(uint32_t slotId,
		const hidl_vec<uint8_t>& key,
		IWeaver::read_cb _hidl_cb)
	{
		weaver_read_status status = WEAVER_READ_FAILED;
		weaver_read_response response = { 0, { {}, 0 } };

		impl_->read(slotId, key, status, response);

		WeaverReadStatus ret_status = convert_weaver_read_status(status);
		WeaverReadResponse ret_response = convert_weaver_read_response(response);

		_hidl_cb(ret_status, ret_response);
		return Void();
	}

}  // namespace implementation
}  // namespace V1_0
}  // namespace weaver
}  // namespace hardware
}  // namespace android

