/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "android.hardware.thermal@2.0-service.exynos"

#include <hidl/HidlTransportSupport.h>
#include <hardware/thermal.h>
#include "Thermal.h"

using android::sp;

// libhwbinder:
using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;

// Generated HIDL files
using android::hardware::thermal::V2_0::IThermal;
using android::hardware::thermal::V2_0::implementation::Thermal;

using android::status_t;
using android::OK;

int main() {
	android::sp<IThermal> service = nullptr; 

	ALOGI("Thermal HAL Start.");
	service = new Thermal();

	if (service == nullptr) {
	  ALOGE("Can not create an instance of Thermal HAL Iface, exiting.");
	  return 1;
	}

	configureRpcThreadpool(1, true /*callerWillJoin*/);
	status_t status = service->registerAsService();

	if (status != OK) {
		ALOGE("Cannot register Thermal HAL service");
		return 1;
	}

	ALOGI("Thermal HAL Ready.");
	joinRpcThreadpool();
	// Under noraml cases, execution will not reach this line.
	ALOGI("Thermal HAL failed to join thread pool.");
	return 1;
}
