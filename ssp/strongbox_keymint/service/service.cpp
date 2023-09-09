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

#include <android/binder_manager.h> /* AServiceManager_ */
#include <android/binder_process.h> /* ABinderProcess_ */
#include <android-base/logging.h>

#include <hidl/HidlTransportSupport.h>
#include <aidl/android/hardware/security/keymint/BnKeyMintDevice.h>
#include <aidl/android/hardware/security/sharedsecret/BnSharedSecret.h>
#include <StrongboxKeymintDevice.h>
#include <StrongboxKeymintImpl.h>
#include <StrongboxSharedSecret.h>

using aidl::android::hardware::security::keymint::IKeyMintDevice;
using aidl::android::hardware::security::keymint::StrongboxKeymintDevice;
using aidl::android::hardware::security::sharedsecret::ISharedSecret;
using aidl::android::hardware::security::sharedsecret::StrongboxSharedSecret;
using aidl::android::hardware::security::keymint::SecurityLevel;

int main() {
    /* thread pool */
    ABinderProcess_setThreadPoolMaxThreadCount(0);

    LOG(INFO) << "SBKM: STRONGBOX KEYMINT 1.0 SERVICE LOAD...";

    /* impl */
    StrongboxKeymintImpl *impl = new StrongboxKeymintImpl();

    std::shared_ptr<IKeyMintDevice> keymint_service =
        ndk::SharedRefBase::make<StrongboxKeymintDevice>(impl);
    auto keymint_service_name = std::string(IKeyMintDevice::descriptor) + "/strongbox";

    LOG(INFO) << "service name is " << keymint_service_name.c_str();

    /* registering */
    auto status = AServiceManager_addService(keymint_service->asBinder().get(),
            keymint_service_name.c_str());
    if (status != STATUS_OK) {
        keymint_service.reset();
        LOG(FATAL) << "SBKM: STRONGBOX KEYMINT 1.0 SERVICE REGISTERING FAILED...";
    }
    LOG(INFO) << "SBKM: STRONGBOX KEYMINT 1.0 SERVICE REGITSTERED...";

    /* shared_secret registering */
    std::shared_ptr<ISharedSecret> shared_secret_service =
        ndk::SharedRefBase::make<StrongboxSharedSecret>(impl);
    auto shared_secret_service_name = std::string(ISharedSecret::descriptor) + "/strongbox";


    LOG(INFO) << "service name is " << shared_secret_service_name.c_str();
    status = AServiceManager_addService(shared_secret_service->asBinder().get(),
            shared_secret_service_name.c_str());
    if (status != STATUS_OK) {
        shared_secret_service.reset();
        LOG(FATAL) << "SBKM: STRONGBOX SHARED SECRET 1.0 SERVICE REGISTERING FAILED...";
    }

    LOG(INFO) << "SBKM: STRONGBOX SHARED SECRET 1.0 SERVICE REGITSTERED...";

    LOG(INFO) << "SBKM: STRONGBOX KEYMINT 1.0 SERVICE LOADED...";

    /* join thread pool */
    ABinderProcess_joinThreadPool();
    return -1;  // Should never get here.
}
