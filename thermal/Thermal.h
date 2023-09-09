#ifndef ANDROID_HARDWARE_THERMAL_V2_0_THERMAL_H
#define ANDROID_HARDWARE_THERMAL_V2_0_THERMAL_H

#include <android/hardware/thermal/2.0/IThermal.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>

#include "thermal_exynos.h"

namespace android {
namespace hardware {
namespace thermal {
namespace V2_0 {
namespace implementation {

using ::android::sp;
using ::android::hardware::hidl_array;
using ::android::hardware::hidl_memory;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::thermal::V1_0::CpuUsage;
using ::android::hardware::thermal::V2_0::CoolingType;
using ::android::hardware::thermal::V2_0::IThermal;
using CoolingDevice_1_0 = ::android::hardware::thermal::V1_0::CoolingDevice;
using CoolingDevice_2_0 = ::android::hardware::thermal::V2_0::CoolingDevice;
using Temperature_1_0 = ::android::hardware::thermal::V1_0::Temperature;
using Temperature_2_0 = ::android::hardware::thermal::V2_0::Temperature;
using ::android::hardware::thermal::V2_0::IThermalChangedCallback;
using ::android::hardware::thermal::V2_0::TemperatureThreshold;
using ::android::hardware::thermal::V2_0::TemperatureType;

struct CallbackSetting {
	CallbackSetting(sp<IThermalChangedCallback> callback, bool is_filter_type, TemperatureType type)
		: callback(callback), is_filter_type(is_filter_type), type(type) {}
	sp<IThermalChangedCallback> callback;
	bool is_filter_type;
	TemperatureType type;
};

struct Thermal : public IThermal {
	Thermal();
	// Methods from ::android::hardware::thermal::V1_0::IThermal follow.
	Return<void> getTemperatures(getTemperatures_cb _hidl_cb) override;
	Return<void> getCpuUsages(getCpuUsages_cb _hidl_cb) override;
	Return<void> getCoolingDevices(getCoolingDevices_cb _hidl_cb) override;

	// Methods from ::android::hardware::thermal::V2_0::IThermal follow.
	Return<void> getCurrentTemperatures(bool filterType, TemperatureType type,
										getCurrentTemperatures_cb _hidl_cb) override;
	Return<void> getTemperatureThresholds(bool filterType, TemperatureType type,
										  getTemperatureThresholds_cb _hidl_cb) override;
	Return<void> registerThermalChangedCallback(
		const sp<IThermalChangedCallback>& callback, bool filterType, TemperatureType type,
		registerThermalChangedCallback_cb _hidl_cb) override;
	Return<void> unregisterThermalChangedCallback(
		const sp<IThermalChangedCallback>& callback,
		unregisterThermalChangedCallback_cb _hidl_cb) override;
	Return<void> getCurrentCoolingDevices(bool filterType, CoolingType type,
										  getCurrentCoolingDevices_cb _hidl_cb) override;

	void sendThermalChangedCallback(const std::vector<Temperature_2_0> &temps);

   private:
	std::mutex thermal_callback_mutex_;
	std::vector<CallbackSetting> callbacks_;
	sp<ThermalNotifier> thermal_notifier_;
};

} // namespace implementation
} // namespace V2_0
} // namespace thermal
} // namespace hardware
} // namespace android

#endif // ANDROID_HARDWARE_THERMAL_V2_0 THERMAL_H
