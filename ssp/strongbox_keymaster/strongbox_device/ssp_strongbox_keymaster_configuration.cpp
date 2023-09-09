/*
 * Copyright (C) 2016 The Android Open Source Project
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

//------ comes from aosp keymaster

#include <string>

#define LOG_TAG "strongbox"

#include <android-base/properties.h>
#include <log/log.h>

#include "ssp_strongbox_keymaster_configuration.h"
#include "ssp_strongbox_keymaster_log.h"



namespace ssp_strongbox_keymaster {

namespace {

constexpr char PROPERTY_OS_VERSION[] = "ro.build.version.release";
constexpr char PROPERTY_OS_PATCHLEVEL[] = "ro.build.version.security_patch";
constexpr char PROPERTY_VENDOR_PATCHLEVEL[] = "ro.vendor.build.security_patch";

std::string DigitsOnly(const std::string& code) {
    // Keep digits only.
    std::string filtered_code;
    std::copy_if(code.begin(), code.end(), std::back_inserter(filtered_code),
                 isdigit);
    return filtered_code;
}

/** Get one version number from a string and move loc to the point after the
 * next version delimiter.
 */
uint32_t ExtractVersion(const std::string& version, size_t* loc) {
    if (*loc == std::string::npos || *loc >= version.size()) {
        return 0;
    }

    uint32_t value = 0;
    size_t new_loc = version.find('.', *loc);
    if (new_loc == std::string::npos) {
        auto sanitized = DigitsOnly(version.substr(*loc));
        if (!sanitized.empty()) {
            if (sanitized.size() < version.size() - *loc) {
                BLOGE("Unexpected version format: %s", version.c_str());
            }
            value = std::stoi(sanitized);
        }
        *loc = new_loc;
    } else {
        auto sanitized = DigitsOnly(version.substr(*loc, new_loc - *loc));
        if (!sanitized.empty()) {
            if (sanitized.size() < new_loc - *loc) {
                BLOGE("Unexpected version format: %s", version.c_str());
            }
            value = std::stoi(sanitized);
        }
        *loc = new_loc + 1;
    }
    return value;
}

uint32_t VersionToUint32(const std::string& version) {
    size_t loc = 0;
    uint32_t major = ExtractVersion(version, &loc);
    uint32_t minor = ExtractVersion(version, &loc);
    uint32_t subminor = ExtractVersion(version, &loc);
    return major * 10000 + minor * 100 + subminor;
}

uint32_t DateCodeToUint32(const std::string& code, bool include_day) {
    // Keep digits only.
    std::string filtered_code = DigitsOnly(code);

    // Return 0 if the date string has an unexpected number of digits.
    uint32_t return_value = 0;
    if (filtered_code.size() == 8) {
        return_value = std::stoi(filtered_code);
        if (!include_day) {
            return_value /= 100;
        }
    } else if (filtered_code.size() == 6) {
        return_value = std::stoi(filtered_code);
        if (include_day) {
            return_value *= 100;
        }
    } else {
        BLOGE("Unexpected patchset format: %s", code.c_str());
    }
    return return_value;
}


std::string wait_and_get_property(const char* prop) {
    std::string prop_value;
    while (!android::base::WaitForPropertyCreation(prop)) {
        BLOGE("waited 15s for %s, still waiting...", prop);
    }
    prop_value = android::base::GetProperty(prop, "" /* default */);

    return prop_value;
}

}  // anonymous namespace

uint32_t GetOsVersion(const std::string& version) {
    BLOGD("OS version property : %s", version.c_str());
    return VersionToUint32(version);
}

uint32_t GetOsVersion() {
    std::string version = wait_and_get_property(PROPERTY_OS_VERSION);
    return GetOsVersion(version);
}

uint32_t GetOsPatchlevel(const std::string& patchlevel) {
    BLOGD("OS patchlevel property : %s", patchlevel.c_str());
    return DateCodeToUint32(patchlevel, false);
}

uint32_t GetOsPatchlevel() {
    std::string patchlevel = wait_and_get_property(PROPERTY_OS_PATCHLEVEL);
    return GetOsPatchlevel(patchlevel);
}

uint32_t GetVendorPatchlevel(const std::string& patchlevel) {
    BLOGD("Vendor patchlevel property : %s", patchlevel.c_str());
    return DateCodeToUint32(patchlevel, true);
}

uint32_t GetVendorPatchlevel() {
    std::string patchlevel = wait_and_get_property(PROPERTY_VENDOR_PATCHLEVEL);
    return GetVendorPatchlevel(patchlevel);
}


}  // namespace keymaster

