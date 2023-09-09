/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <cstdlib>

#define LOG_TAG "ThermalHAL"
#include <log/log.h>
#include <android-base/logging.h>

#include <hardware/hardware.h>
#include <hardware/thermal.h>

#include "Thermal.h"

using namespace std;

namespace android {
namespace hardware {
namespace thermal {
namespace V2_0 {
namespace implementation {

using ::android::hardware::thermal::V1_0::ThermalStatus;
using ::android::hardware::thermal::V1_0::ThermalStatusCode;

map<string, ifstream> temperatureNodes;
map<string, vector<ifstream>> throttleNodes;
map<string, vector<ifstream>> hysteresisNodes;
map<TemperatureType, vector<string>> thermalNames;
vector<ifstream> cpuOnlineNodes;
ifstream cpuUsageNode;

map<CoolingType, vector<string>> cdevNames;
map<string, ifstream> cdevCurStates;

string thermalZonePath = "/sys/class/thermal/thermal_zone";
string coolingDevicePath = "/sys/class/thermal/cooling_device";
string cpuPath = "/sys/devices/system/cpu/cpu";

unsigned int readValue(ifstream &node) {
	string buf;

	node.seekg(0, node.beg);
	node >> buf;
	return atoi(buf.c_str());
}

unsigned int readCurTemp(string &thermalName) {
	return readValue(temperatureNodes[thermalName]);
}

unsigned int readThrottleTemp(string &thermalName, int lv) {
	return readValue(throttleNodes[thermalName][lv]);
}

unsigned int readCpuOnline(int cpuNum) {
	return readValue(cpuOnlineNodes[cpuNum]);
}

unsigned int readCurCdevState(string &cdevName) {
	return readValue(cdevCurStates[cdevName]);
}

unsigned int readHysteresisTemp(string &thermalName, int lv) {
	return readValue(hysteresisNodes[thermalName][lv]);
}

void initExynosThermalHal(void) {
	int i = 0;
	while (true) {
		string curThermalZonePath = thermalZonePath + to_string(i);
		i++;
		ifstream zoneTypeNode(curThermalZonePath + "/type");
		if (!zoneTypeNode.is_open())
			break;

		string zoneName;
		TemperatureType tempType;
		zoneTypeNode >> zoneName;
		if (zoneName == "BIG" || zoneName == "MID" || zoneName == "LITTLE") {
			tempType = TemperatureType::CPU;
		}
		else if (zoneName == "G3D") {
			tempType = TemperatureType::GPU;
		}
		else if (zoneName == "NPU") {
			tempType = TemperatureType::NPU;
		}
		else {
			continue;
		}

		// Set temperature node
		temperatureNodes[zoneName].open(curThermalZonePath + "/temp");
		thermalNames[tempType].push_back(zoneName);

		// Set throttling node
		for (int j = 1; j < 8; j++) {
			throttleNodes[zoneName].emplace_back(
					curThermalZonePath + "/trip_point_" + to_string(j) + "_temp");
			hysteresisNodes[zoneName].emplace_back(
				curThermalZonePath + "/trip_point_" + to_string(j) + "_hyst");
		}
	}

	i = 0;
	while (true) {
		string curCdevPath = coolingDevicePath + to_string(i++);
		ifstream cdevTypeNode(curCdevPath + "/type");
		if (!cdevTypeNode.is_open())
			break;

		string cdevName;
		CoolingType cdevType;
		cdevTypeNode >> cdevName;
		if (cdevName.find("cpufreq") != string::npos) {
			cdevType = CoolingType::CPU;
		}
		else if (cdevName.find("gpufreq") != string::npos) {
			cdevType = CoolingType::GPU;
		}
		else if (cdevName.find("npu") != string::npos) {
			cdevType = CoolingType::NPU;
		}
		else {
			continue;
		}

		// Set cooling device node
		cdevNames[cdevType].push_back(cdevName);
		cdevCurStates[cdevName].open(curCdevPath + "/cur_state");
	}

	i = 0;
	while (true) {
		string curCpuPath = cpuPath + to_string(i) + "/online";
		ifstream onlineNode(curCpuPath);

		if (!onlineNode.is_open())
			break;

		cpuOnlineNodes.emplace_back(curCpuPath);
		i++;
	}
}

ThermalStatus getAllTemperatures(hidl_vec<Temperature_1_0> &temperatures)
{
	ThermalStatus status;
	status.code = ThermalStatusCode::SUCCESS;
	unsigned int cnt = 0;

	temperatures.resize(temperatureNodes.size());

	for (auto it = thermalNames.begin(); it != thermalNames.end(); it++) {
		for (int i = 0; i < it->second.size(); i++) {
			string &name = it->second[i];
			temperatures[cnt].currentValue = readCurTemp(name) / 1000;
			temperatures[cnt].throttlingThreshold = readThrottleTemp(name, 0);
			temperatures[cnt].shutdownThreshold = readThrottleTemp(name, 6);
			temperatures[cnt].vrThrottlingThreshold = UNKNOWN_TEMPERATURE;
			temperatures[cnt].type = static_cast<::android::hardware::thermal::V1_0::TemperatureType>(it->first);
			temperatures[cnt].name = it->second[i];
			cnt++;
		}
	}

	return status;
}

ThermalStatus getTypeTemperatures(TemperatureType tType, hidl_vec<Temperature_2_0> &temperatures)
{
	ThermalStatus status;
	status.code = ThermalStatusCode::SUCCESS;
	vector<string> &tTypeName = thermalNames[tType];

	if(tTypeName.size() == 0) {
		status.code = ThermalStatusCode::FAILURE;
		return status;
	}

	temperatures.resize(tTypeName.size());

	for (int i = 0; i < tTypeName.size(); i++) {
		string &name = tTypeName[i];
		unsigned int temp = readCurTemp(name);
		temperatures[i].value = temp / 1000;
		temperatures[i].type = tType;
		temperatures[i].name = tTypeName[i];
		temperatures[i].throttlingStatus = ThrottlingSeverity::NONE;

		// Check throttle status
		for (int j = throttleNodes.size() - 1; j >= 0; j--) {
			if (temp >= readThrottleTemp(name, j)) {
				temperatures[i].throttlingStatus = (ThrottlingSeverity)j;
				break;
			}
		}
	}

	return status;
}

ThermalStatus getCpuUsage(hidl_vec<CpuUsage> &cpuUsage) {
	string str, value;
	stringstream line;
	ThermalStatus status;
	status.code = ThermalStatusCode::SUCCESS;
	bool flag = true;
	string statPath = "/proc/stat";
	ifstream statNode(statPath);
	unsigned int cpuNum = 0;

	cpuUsage.resize(cpuOnlineNodes.size());

	while (!statNode.eof()) {
		getline(statNode, str);
		if (!str.length())
			break;

		line.str(str);
		line >> value;

		unsigned long long user, nice, system, idle, active, total;

		if (value != "cpu0" && flag)
			continue;

		if (value.compare(0, 3, "cpu")) {
			flag = true;
			continue;
		}
		else
			flag = false;

		line >> user >> nice >> system >> idle;
		active = user + nice + system;
		total = active + idle;

		cpuUsage[cpuNum].name = value;
		cpuUsage[cpuNum].active = active;
		cpuUsage[cpuNum].total = total;
		cpuUsage[cpuNum].isOnline = 1;
		cpuNum++;
	}

	return status;
}

/* NOT SUPPORT FAN COOLING DEVICE */
ThermalStatus coolingDevices(vector<CoolingDevice_1_0> cdevs) {
	ThermalStatus status;
	status.code = ThermalStatusCode::SUCCESS;

	cdevs.clear();

	return status;
}

ThermalStatus curCoolingDevices(CoolingType type, hidl_vec<CoolingDevice_2_0> &cdevs) {
	ThermalStatus status;
	status.code = ThermalStatusCode::SUCCESS;
	vector<string> &typeCdevNames = cdevNames[type];

	if (typeCdevNames.size() == 0) {
		status.code = ThermalStatusCode::FAILURE;
		return status;
	}

	cdevs.resize(typeCdevNames.size());

	for (int i = 0; i < typeCdevNames.size(); i++) {
		cdevs[i].name = typeCdevNames[i];
		cdevs[i].type = type;
		cdevs[i].value = readCurCdevState(typeCdevNames[i]);
	}

	return status;
}

ThermalStatus temperatureThresholds(TemperatureType tType, vector<TemperatureThreshold> &thresholds) {
	ThermalStatus status;
	status.code = ThermalStatusCode::SUCCESS;
	vector<string> &zoneNames = thermalNames[tType];

	if (zoneNames.size() == 0) {
		status.code = ThermalStatusCode::FAILURE;
		return status;
	}

	thresholds.resize(zoneNames.size());

	for (int i = 0; i < zoneNames.size(); i++) {
		thresholds[i].type = tType;
		thresholds[i].name = zoneNames[i];
		thresholds[i].vrThrottlingThreshold = UNKNOWN_TEMPERATURE;
		for (int j = 0; j < throttleNodes[zoneNames[i]].size(); j++) {
			thresholds[i].hotThrottlingThresholds[j] = readThrottleTemp(zoneNames[i], j);
			thresholds[i].coldThrottlingThresholds[j] = thresholds[i].hotThrottlingThresholds[j] - readHysteresisTemp(zoneNames[i], j);
		}
	}

	return status;
}

bool ThermalNotifier::startWatchingDeviceFiles() {
	if (cb_) {
		auto ret = this->run("FileNotifierThread", PRIORITY_HIGHEST);
		if (ret != NO_ERROR) {
			LOG(ERROR) << "ThermalNotifierThread start fail";
			return false;
		} else {
			LOG(INFO) << "ThermalNotifierThread started";
			return true;
		}
	}
	return false;
}

bool ThermalNotifier::threadLoop() {
	hidl_vec<Temperature_2_0> getTemps;
	std::vector<Temperature_2_0> sendTemps;

	LOG(VERBOSE) << "ThermalNotifier polling...";

	this_thread::sleep_for(chrono::milliseconds(2000));
	for (auto it = thermalNames.begin(); it != thermalNames.end(); it++) {
		getTypeTemperatures(it->first, getTemps);
		for (auto &t : getTemps) {
			LOG(VERBOSE) << "ThermalNotifier push_back :" << t.name;
			sendTemps.push_back(t);
		}
	}

	if (cb_)
		cb_(sendTemps);

	return true;
}

}  // namespace implementation
}  // namespace V2_0
}  // namespace thermal
}  // namespace hardware
}  // namespace android
