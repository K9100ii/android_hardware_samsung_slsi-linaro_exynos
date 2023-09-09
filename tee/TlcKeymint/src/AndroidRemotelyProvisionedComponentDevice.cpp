/*
 * Copyright (c) 2013-2022 TRUSTONIC LIMITED
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the TRUSTONIC LIMITED nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "AndroidRemotelyProvisionedComponentDevice.h"

namespace aidl::android::hardware::security::keymint {

using ::std::vector;

namespace {

inline ScopedAStatus kmError2ScopedAStatus(const keymaster_error_t value) {
    switch (value) {
        case KM_ERROR_OK:
            return ScopedAStatus::ok();
        case KM_ERROR_RKP_STATUS_INVALID_MAC:
            return ScopedAStatus(AStatus_fromServiceSpecificError(IRemotelyProvisionedComponent::STATUS_INVALID_MAC));
        case KM_ERROR_RKP_STATUS_PRODUCTION_KEY_IN_TEST_REQUEST:
            return ScopedAStatus(AStatus_fromServiceSpecificError(IRemotelyProvisionedComponent::STATUS_PRODUCTION_KEY_IN_TEST_REQUEST));
        case KM_ERROR_RKP_STATUS_TEST_KEY_IN_PRODUCTION_REQUEST:
            return ScopedAStatus(AStatus_fromServiceSpecificError(IRemotelyProvisionedComponent::STATUS_TEST_KEY_IN_PRODUCTION_REQUEST));
        case KM_ERROR_RKP_STATUS_INVALID_EEK:
            return ScopedAStatus(AStatus_fromServiceSpecificError(IRemotelyProvisionedComponent::STATUS_INVALID_EEK));
        case KM_ERROR_RKP_STATUS_FAILED:
        default:
            return ScopedAStatus(AStatus_fromServiceSpecificError(IRemotelyProvisionedComponent::STATUS_FAILED));

    }
}

}  // namespace

AndroidRemotelyProvisionedComponentDevice::AndroidRemotelyProvisionedComponentDevice(
    const std::shared_ptr<AndroidKeyMintDevice>& keymint)
    : impl_(keymint->getImpl()) {}

ScopedAStatus AndroidRemotelyProvisionedComponentDevice::getHardwareInfo(RpcHardwareInfo* info) {
    info->versionNumber = 2;
    info->rpcAuthorName = "Trustonic";
    info->supportedEekCurve = RpcHardwareInfo::CURVE_25519;
    info->uniqueId = "Trustonic keymint";
    return ScopedAStatus::ok();
}

ScopedAStatus AndroidRemotelyProvisionedComponentDevice::generateEcdsaP256KeyPair(
    bool testMode, MacedPublicKey* macedPublicKey, std::vector<uint8_t>* privateKeyHandle) {
    keymaster_blob_t maced_public_key_blob = {};
    keymaster_key_blob_t private_key_handle_blob = {};
    keymaster_error_t ret = impl_->generate_ecdsa_p256_key(
        testMode, &maced_public_key_blob, &private_key_handle_blob);
    if (ret != KM_ERROR_OK) {
        return kmError2ScopedAStatus(ret);
    }
    macedPublicKey->macedKey = std::vector(maced_public_key_blob.data, maced_public_key_blob.data + maced_public_key_blob.data_length);
    *privateKeyHandle = std::vector(private_key_handle_blob.key_material, private_key_handle_blob.key_material + private_key_handle_blob.key_material_size);
    free((void*)maced_public_key_blob.data);
    maced_public_key_blob.data = NULL;
    maced_public_key_blob.data_length = 0;
    free((void*)private_key_handle_blob.key_material);
    private_key_handle_blob.key_material = NULL;
    private_key_handle_blob.key_material_size = 0;
    return ScopedAStatus::ok();
}

ScopedAStatus AndroidRemotelyProvisionedComponentDevice::generateCertificateRequest(
    bool testMode, const std::vector<MacedPublicKey>& keysToSign, const std::vector<uint8_t>& endpointEncCertChain,
    const std::vector<uint8_t>& challenge, DeviceInfo* deviceInfo, ProtectedData* protectedData,
    std::vector<uint8_t>* keysToSignMac) {
    keymaster_blob_t* keys_to_sign = new keymaster_blob_t[keysToSign.size()];
    keymaster_blob_t endpoint_enc_cert_chain = {endpointEncCertChain.data(), endpointEncCertChain.size()};
    keymaster_blob_t challenge_blob = {challenge.data(), challenge.size()};
    keymaster_blob_t device_info = {};
    keymaster_blob_t protected_data = {};
    keymaster_blob_t keys_to_sign_mac = {};
    for (size_t i=0; i<keysToSign.size(); i++) {
        keys_to_sign[i] = {keysToSign[i].macedKey.data(), keysToSign[i].macedKey.size()};
    }
    keymaster_error_t ret = impl_->generate_certificate_request(
        testMode, keys_to_sign, keysToSign.size(), &endpoint_enc_cert_chain, &challenge_blob,
        &device_info, &protected_data, &keys_to_sign_mac);
    delete[]keys_to_sign;
    if (ret != KM_ERROR_OK) {
        return kmError2ScopedAStatus(ret);
    }
    deviceInfo->deviceInfo = std::vector(device_info.data, device_info.data + device_info.data_length);
    free((void*)device_info.data);
    device_info.data = NULL;
    device_info.data_length = 0;
    protectedData->protectedData = std::vector(protected_data.data, protected_data.data + protected_data.data_length);
    free((void*)protected_data.data);
    protected_data.data = NULL;
    protected_data.data_length = 0;
    *keysToSignMac = std::vector(keys_to_sign_mac.data, keys_to_sign_mac.data + keys_to_sign_mac.data_length);
    free((void*)keys_to_sign_mac.data);
    keys_to_sign_mac.data = NULL;
    keys_to_sign_mac.data_length = 0;
    return ScopedAStatus::ok();
}

}  // namespace aidl::android::hardware::security::keymint
