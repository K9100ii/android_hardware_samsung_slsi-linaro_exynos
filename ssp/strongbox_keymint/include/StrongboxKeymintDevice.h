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

#ifndef _STRONGBOX_KEYMINT_DEVICE_H_
#define _STRONGBOX_KEYMINT_DEVICE_H_

#include <aidl/android/hardware/security/keymint/BnKeyMintDevice.h>
#include <hidl/Status.h>

#include <hidl/MQDescriptor.h>

#include "StrongboxKeymintOperation.h"
#include "StrongboxKeymintImpl.h"
#include "ssp_strongbox_keymaster_defs.h"
#include "ssp_strongbox_keymaster_log.h"

#include "KeyMintUtils.h"

namespace aidl {
namespace android {
namespace hardware {
namespace security {
namespace keymint {

using ::std::shared_ptr;
using ::ndk::SpAIBinder;
using ::ndk::SharedRefBase;
using ::ndk::ScopedAStatus;
using ::std::optional;
using ::std::vector;

/* keymint */
using ::aidl::android::hardware::security::keymint::BeginResult;
using ::aidl::android::hardware::security::keymint::HardwareAuthToken;
using ::aidl::android::hardware::security::keymint::IKeyMintOperation;
using ::aidl::android::hardware::security::keymint::KeyFormat;
using ::aidl::android::hardware::security::keymint::KeyPurpose;
using ::aidl::android::hardware::security::keymint::KeyMintHardwareInfo;
using ::aidl::android::hardware::security::keymint::KeyParameter;
using ::aidl::android::hardware::security::keymint::AttestationKey;
using ::aidl::android::hardware::security::keymint::KeyCreationResult;
using ::aidl::android::hardware::security::keymint::SecurityLevel;

/* secure clock */
using ::aidl::android::hardware::security::secureclock::TimeStampToken;

/* km utils */
using namespace km_utils;

class StrongboxKeymintDevice : public BnKeyMintDevice {
  public:
    StrongboxKeymintDevice(StrongboxKeymintImpl *impl)
    : impl_(impl) {
        /* constructor */
        BLOGD("[Keymint Constructor]\n %s: %s: %d", __FILE__, __func__, __LINE__);
    }

    virtual ~StrongboxKeymintDevice();

    ScopedAStatus getHardwareInfo(KeyMintHardwareInfo* info);

    ScopedAStatus addRngEntropy(const vector<uint8_t>& data);

    ScopedAStatus generateKey(const vector<KeyParameter>& keyParams,
            const optional<AttestationKey>& attestationKey,
            KeyCreationResult* creationResult);

    ScopedAStatus importKey(const vector<KeyParameter>& keyParams,
            KeyFormat keyFormat,
            const vector<uint8_t>& keyData,
            const optional<AttestationKey>& attestationKey,
            KeyCreationResult* creationResult);

    ScopedAStatus importWrappedKey(const vector<uint8_t>& wrappedKeyData,
            const vector<uint8_t>& wrappingKeyBlob,
            const vector<uint8_t>& maskingKey,
            const vector<KeyParameter>& unwrappingParams,
            int64_t passwordSid,
            int64_t biometricSid,
            KeyCreationResult* creationResult);

    ScopedAStatus upgradeKey(const vector<uint8_t>& keyBlobToUpgrade,
            const vector<KeyParameter>& upgradeParams,
            vector<uint8_t>* keyBlob);


    ScopedAStatus begin(KeyPurpose purpose,
            const vector<uint8_t>& keyBlob,
            const vector<KeyParameter>& params,
            const optional<HardwareAuthToken>& authToken,
            BeginResult* result);

    ScopedAStatus deviceLocked(
            bool in_passwordOnly,
            const optional<TimeStampToken>& in_timestampToken);

    ScopedAStatus earlyBootEnded();

    ScopedAStatus getKeyCharacteristics(
            const vector<uint8_t>& keyBlob,
            const vector<uint8_t>& appId,
            const vector<uint8_t>& appData,
            std::vector<KeyCharacteristics>* keyCharacteristics);

    /* unimplemented APIs */
    ScopedAStatus deleteKey( [[maybe_unused]] const vector<uint8_t>& keyBlob) {
        return ScopedAStatus::ok();
    }
    ScopedAStatus deleteAllKeys() {
        return ScopedAStatus::ok();
    }
    ScopedAStatus destroyAttestationIds() {
        return ScopedAStatus::ok();
    }

    ScopedAStatus convertStorageKeyToEphemeral(
            [[maybe_unused]] const vector<uint8_t>& storageKeyBlob,
            [[maybe_unused]] vector<uint8_t>* perBootEphemeralKeyBlob) {
        return ScopedAStatus::ok();
    }

    ScopedAStatus performOperation(
            [[maybe_unused]] const vector<uint8_t>& request,
            [[maybe_unused]] vector<uint8_t>* encryptedBuf) {
        return ScopedAStatus::ok();
    }

private:
    StrongboxKeymintImpl *impl_;
};

}  // namespace keymint
}  // namespace security
}  // namespace hardware
}  // namespace android
}  // namespace aidl

#endif  // _STRONGBOX_KEYMINT_DEVICE_H_
