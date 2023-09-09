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

#ifndef __WEAVER__DEVICE__H__
#define __WEAVER__DEVICE__H__

#include "weaver_device_impl.h"

#include <hidl/Status.h>
#include <hidl/MQDescriptor.h>

namespace android {
namespace hardware {
namespace weaver {
namespace V1_0 {
namespace implementation {

using ::android::hardware::weaver::V1_0::WeaverConfig;
using ::android::hardware::weaver::V1_0::WeaverStatus;
using ::android::hardware::weaver::V1_0::WeaverReadStatus;
using ::android::hardware::weaver::V1_0::WeaverReadResponse;
using ::android::hardware::weaver::V1_0::IWeaver;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::hidl_vec;
using ::android::hardware::hidl_string;
using ::android::sp;

struct WeaverDevice : public IWeaver {
	WeaverDevice(WeaverDeviceImpl* impl) : impl_(impl) {}
	virtual ~WeaverDevice();

	Return<void> getConfig(
		IWeaver::getConfig_cb _hidl_cb);

	Return<WeaverStatus> write(
		uint32_t slotId,
		const hidl_vec<uint8_t>& key,
		const hidl_vec<uint8_t>& value);

	Return<void> read(
		uint32_t slotId,
		const hidl_vec<uint8_t>& key,
		read_cb _hidl_cb);

	private:
		WeaverDeviceImpl* impl_;
	};
}  // namespace implementation
}  // namespace V1_0
}  // namespace weaver
}  // namespace hardware
}  // namespace android

#endif  // __WEAVER__DEVICE__H__
