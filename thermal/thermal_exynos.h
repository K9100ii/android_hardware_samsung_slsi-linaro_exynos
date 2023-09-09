/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * *    * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef __THERMAL_EXYNOS_H__
#define __THERMAL_EXYNOS_H__

#include <vector>
#include <set>
#include <thread>

#include <hardware/thermal.h>
#include <utils/threads.h>

using namespace std;

namespace android {
namespace hardware {
namespace thermal {
namespace V2_0 {
namespace implementation {

using ::android::hardware::thermal::V1_0::ThermalStatus;
using ::android::hardware::thermal::V2_0::CoolingType;
using ::android::hardware::thermal::V2_0::TemperatureType;
using ::android::hardware::thermal::V1_0::CpuUsage;
using CoolingDevice_1_0 = ::android::hardware::thermal::V1_0::CoolingDevice;
using CoolingDevice_2_0 = ::android::hardware::thermal::V2_0::CoolingDevice;
using Temperature_1_0 = ::android::hardware::thermal::V1_0::Temperature;
using Temperature_2_0 = ::android::hardware::thermal::V2_0::Temperature;
using NotifierCallback = std::function<void(const hidl_vec<Temperature_2_0> &temps)>;

void initExynosThermalHal(void);
ThermalStatus getAllTemperatures(hidl_vec<Temperature_1_0> &temperatures);
ThermalStatus getTypeTemperatures(TemperatureType tType, hidl_vec<Temperature_2_0> &temperatures);
ThermalStatus getCpuUsage(hidl_vec<CpuUsage> &cpuUsage);
ThermalStatus coolingDevices(vector<CoolingDevice_1_0> cdevs);
ThermalStatus curCoolingDevices(CoolingType type, hidl_vec<CoolingDevice_2_0> &cdevs);
ThermalStatus temperatureThresholds(TemperatureType tType, vector<TemperatureThreshold> &temperature_thresholds);
//ThermalStatus updateThermalStatusAllType(std::vector<Temperature_2_0> &temperatures);

class ThermalNotifier : public ::android::Thread {
	public:
		ThermalNotifier(const NotifierCallback &cb)
			: Thread(false), cb_(cb) {}
		~ThermalNotifier() = default;

		bool startWatchingDeviceFiles();

	private:
		bool threadLoop() override;
		const NotifierCallback cb_;
};

}
}
}
}
}

#endif //__THERMAL_EXYNOS_H__
