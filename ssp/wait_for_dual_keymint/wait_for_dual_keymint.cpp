/*
 ** Copyright 2018, The Android Open Source Project
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

#include <unistd.h>
#define LOG_TAG "wait_for_dual_keymint"

#include <android/binder_manager.h>
#include <android/binder_process.h>

#include <android-base/logging.h>
#include <android-base/properties.h>
#include <aidl/android/hardware/security/keymint/IKeyMintDevice.h>

using aidl::android::hardware::security::keymint::IKeyMintDevice;
using aidl::android::hardware::security::keymint::SecurityLevel;

useconds_t kWaitTimeMicroseconds = 1 * 1000;  // 1 milliseconds

static constexpr const char tee_km_service_name[] =
        "android.hardware.security.keymint.IKeyMintDevice/default";
static constexpr const char sb_km_service_name[] =
        "android.hardware.security.keymint.IKeyMintDevice/strongbox";

int main() {

        /* check wheather service is declared in the VINTF manifest */
        bool teeDeclared = AServiceManager_isDeclared(tee_km_service_name);
        bool sbDeclared = AServiceManager_isDeclared(sb_km_service_name);

        if (teeDeclared) {
            ::ndk::SpAIBinder binder(AServiceManager_waitForService(tee_km_service_name));
            auto tee_km_service = IKeyMintDevice::fromBinder(binder);

            if (!tee_km_service) {
                LOG(WARNING) << "unable to connect to tee keymint";
                return -1;
            }
        }

       if (sbDeclared) {
            ::ndk::SpAIBinder binder(AServiceManager_waitForService(sb_km_service_name));
            auto sb_km_service = IKeyMintDevice::fromBinder(binder);

            if (!sb_km_service) {
                LOG(WARNING) << "unable to connect to tee keymint";
                return -1;
            }

        }
}
