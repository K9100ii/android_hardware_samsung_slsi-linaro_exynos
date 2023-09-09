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

#ifndef _STRONGBOX_KEYMASTER4_DEVICE_H_
#define _STRONGBOX_KEYMASTER4_DEVICE_H_

#include "StrongboxKeymaster4DeviceImpl.h"

#include <android/hardware/keymaster/4.0/IKeymasterDevice.h>
#include <hidl/Status.h>

#include <hidl/MQDescriptor.h>
namespace android {
namespace hardware {
namespace keymaster {
namespace V4_0 {
namespace implementation {

using ::android::hardware::keymaster::V4_0::ErrorCode;
using ::android::hardware::keymaster::V4_0::HardwareAuthenticatorType;
using ::android::hardware::keymaster::V4_0::HardwareAuthToken;
using ::android::hardware::keymaster::V4_0::HmacSharingParameters;
using ::android::hardware::keymaster::V4_0::IKeymasterDevice;
using ::android::hardware::keymaster::V4_0::KeyCharacteristics;
using ::android::hardware::keymaster::V4_0::KeyFormat;
using ::android::hardware::keymaster::V4_0::KeyParameter;
using ::android::hardware::keymaster::V4_0::KeyPurpose;
using ::android::hardware::keymaster::V4_0::SecurityLevel;
using ::android::hardware::keymaster::V4_0::VerificationToken;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::hidl_vec;
using ::android::hardware::hidl_string;
using ::android::sp;

class StrongboxKeymaster4Device : public IKeymasterDevice {
  public:
    StrongboxKeymaster4Device(StrongboxKeymaster4DeviceImpl *impl)
        : impl_(impl) {}
    virtual ~StrongboxKeymaster4Device();

    // Methods from ::android::hardware::keymaster::V4_0::IKeymasterDevice follow.
    Return<void> getHardwareInfo(
        getHardwareInfo_cb _hidl_cb);
    Return<void> getHmacSharingParameters(
        getHmacSharingParameters_cb _hidl_cb);
    Return<void> computeSharedHmac(
        const hidl_vec<HmacSharingParameters>& params,
        computeSharedHmac_cb _hidl_cb);
    Return<void> verifyAuthorization(
        uint64_t operationHandle,
        const hidl_vec<KeyParameter>& parametersToVerify,
        const HardwareAuthToken& authToken,
        verifyAuthorization_cb _hidl_cb);
    Return<ErrorCode> addRngEntropy(
        const hidl_vec<uint8_t>& data);
    Return<void> generateKey(
        const hidl_vec<KeyParameter>& keyParams,
        generateKey_cb _hidl_cb);
    Return<void> importKey(
        const hidl_vec<KeyParameter>& keyParams,
        KeyFormat keyFormat,
        const hidl_vec<uint8_t>& keyData,
        importKey_cb _hidl_cb);
    Return<void> importWrappedKey(
        const hidl_vec<uint8_t>& wrappedKeyData,
        const hidl_vec<uint8_t>& wrappingKeyBlob,
        const hidl_vec<uint8_t>& maskingKey,
        const hidl_vec<KeyParameter>& unwrappingParams,
        uint64_t passwordSid,
        uint64_t biometricSid,
        importWrappedKey_cb _hidl_cb);
    Return<void> getKeyCharacteristics(
        const hidl_vec<uint8_t>& keyBlob,
        const hidl_vec<uint8_t>& clientId,
        const hidl_vec<uint8_t>& appData,
        getKeyCharacteristics_cb _hidl_cb);
    Return<void> exportKey(
        KeyFormat keyFormat,
        const hidl_vec<uint8_t>& keyBlob,
        const hidl_vec<uint8_t>& clientId,
        const hidl_vec<uint8_t>& appData,
        exportKey_cb _hidl_cb);
    Return<void> attestKey(
        const hidl_vec<uint8_t>& keyToAttest,
        const hidl_vec<KeyParameter>& attestParams,
        attestKey_cb _hidl_cb);
    Return<void> upgradeKey(
        const hidl_vec<uint8_t>& keyBlobToUpgrade,
        const hidl_vec<KeyParameter>& upgradeParams,
        upgradeKey_cb _hidl_cb);
    Return<ErrorCode> deleteKey(
        const hidl_vec<uint8_t>& keyBlob);
    Return<ErrorCode> deleteAllKeys();
    Return<ErrorCode> destroyAttestationIds();
    Return<void> begin(
        KeyPurpose purpose,
        const hidl_vec<uint8_t>& keyBlob,
        const hidl_vec<KeyParameter>& inParams,
        const HardwareAuthToken& authToken,
        begin_cb _hidl_cb);
    Return<void> update(
        uint64_t operationHandle,
        const hidl_vec<KeyParameter>& inParams,
        const hidl_vec<uint8_t>& input,
        const HardwareAuthToken& authToken,
        const VerificationToken& verificationToken,
        update_cb _hidl_cb);
    Return<void> finish(
        uint64_t operationHandle,
        const hidl_vec<KeyParameter>& inParams,
        const hidl_vec<uint8_t>& input,
        const hidl_vec<uint8_t>& signature,
        const HardwareAuthToken& authToken,
        const VerificationToken& verificationToken,
        finish_cb _hidl_cb);
    Return<ErrorCode> abort(
        uint64_t operationHandle);

  private:
    StrongboxKeymaster4DeviceImpl *impl_;
};

//extern "C" IKeymasterDevice* HIDL_FETCH_IKeymasterDevice(const char* name);

}  // namespace implementation
}  // namespace V4_0
}  // namespace keymaster
}  // namespace hardware
}  // namespace android

#endif  // _STRONGBOX_KEYMASTER4_DEVICE_H_
