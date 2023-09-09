/*
 * Copyright (c) 2021-2022 TRUSTONIC LIMITED
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
#include <cstring>
#include <sys/endian.h>
#include "km_shared_util.h"
#include <aidl/android/hardware/security/keymint/ErrorCode.h>
#include "AndroidKeyMintDevice.h"
#include "AndroidKeyMintOperation.h"
#include "TrustonicKeymintDeviceImpl.h"

namespace aidl::android::hardware::security::keymint {

using ::ndk::ScopedAStatus;

namespace {

}  // namespace

inline ScopedAStatus kmError2ScopedAStatus(const keymaster_error_t value) {
    return (value == KM_ERROR_OK
                ? ScopedAStatus::ok()
                : ScopedAStatus(AStatus_fromServiceSpecificError(static_cast<int32_t>(value))));
}

class KmKeyParamSet : public keymaster_key_param_set_t {
  public:
    KmKeyParamSet(const vector<KeyParameter>& keyParams) {
        size_t n_params = keyParams.size();
        params = new keymaster_key_param_t[n_params];
        size_t j = 0;
        for (size_t i = 0; i < n_params; ++i) {
            const KeyParameter& p = keyParams[i];
            const keymaster_tag_t tag = keymaster_tag_t(p.tag);
            if (tag != KM_TAG_INVALID) {
                keymaster_key_param_t *q = &params[j];
                q->tag = tag;
                switch (keymaster_tag_get_type(tag)) {
                case KM_ENUM:
                case KM_ENUM_REP:
                    switch (tag) {
                        case KM_TAG_ALGORITHM:
                            q->enumerated = keymaster_algorithm_t(p.value.get<KeyParameterValue::algorithm>());
                            break;
                        case KM_TAG_BLOCK_MODE:
                            q->enumerated = keymaster_block_mode_t(p.value.get<KeyParameterValue::blockMode>());
                            break;
                        case KM_TAG_PADDING:
                            q->enumerated = keymaster_padding_t(p.value.get<KeyParameterValue::paddingMode>());
                            break;
                        case KM_TAG_DIGEST:
                        case KM_TAG_RSA_OAEP_MGF_DIGEST:
                            q->enumerated = keymaster_digest_t(p.value.get<KeyParameterValue::digest>());
                            break;
                        case KM_TAG_EC_CURVE:
                            q->enumerated = keymaster_ec_curve_t(p.value.get<KeyParameterValue::ecCurve>());
                            break;
                        case KM_TAG_ORIGIN:
                            q->enumerated = keymaster_key_origin_t(p.value.get<KeyParameterValue::origin>());
                            break;
                        case KM_TAG_PURPOSE:
                            q->enumerated = keymaster_purpose_t(p.value.get<KeyParameterValue::keyPurpose>());
                            break;
                        case KM_TAG_USER_AUTH_TYPE:
                            q->enumerated = uint32_t(p.value.get<KeyParameterValue::hardwareAuthenticatorType>());
                            break;
                        case KM_TAG_HARDWARE_TYPE:
                            q->enumerated = keymaster_security_level_t(p.value.get<KeyParameterValue::securityLevel>());
                            break;
                        default:
                            break;
                    }
                    break;
                case KM_UINT:
                case KM_UINT_REP:
                    q->integer = p.value.get<KeyParameterValue::integer>();
                    break;
                case KM_ULONG:
                case KM_ULONG_REP:
                    q->long_integer = p.value.get<KeyParameterValue::longInteger>();
                    break;
                case KM_DATE:
                    q->date_time = p.value.get<KeyParameterValue::dateTime>();
                    break;
                case KM_BOOL:
                    q->boolean = true;
                    break;
                case KM_BIGNUM:
                case KM_BYTES: {
                    std::vector<uint8_t> blob = p.value.get<KeyParameterValue::blob>();
                    if (blob.size() != 0) {
                        q->blob.data = (const uint8_t *)malloc(blob.size());
                        if (q->blob.data != NULL) {
                            std::memcpy((void*)q->blob.data, blob.data(), blob.size());
                            q->blob.data_length = blob.size();
                        }
                    }
                }
                break;
                default:
                    break;
                }
                j++;
            }
        }
        length = j;
    }
    KmKeyParamSet(KmKeyParamSet&& other) : keymaster_key_param_set_t{other.params, other.length} {
        other.length = 0;
        other.params = nullptr;
    }
    KmKeyParamSet(const KmKeyParamSet&) = delete;
    ~KmKeyParamSet() { delete[] params; }
};

inline keymaster_key_blob_t convert_to_key_blob(const vector<uint8_t>& blob) {
    keymaster_key_blob_t result = {blob.data(), blob.size()};
    return result;
}

inline vector<uint8_t> kmBlob2vector(const keymaster_key_blob_t& blob) {
    vector<uint8_t> result(blob.key_material, blob.key_material + blob.key_material_size);
    return result;
}

AndroidKeyMintDevice::AndroidKeyMintDevice(SecurityLevel /*securityLevel*/)
    : impl_(new TrustonicKeymintDeviceImpl()) {}

AndroidKeyMintDevice::~AndroidKeyMintDevice() {}

ScopedAStatus AndroidKeyMintDevice::getHardwareInfo(KeyMintHardwareInfo* info) {
    keymaster_security_level_t securityLevel = KM_SECURITY_LEVEL_SOFTWARE;
    const char *keyMintName = NULL;
    const char *keyMintAuthorName = NULL;
    impl_->get_hardware_info(
        &securityLevel, &keyMintName, &keyMintAuthorName);
    info->versionNumber = 2;
    info->securityLevel = (SecurityLevel)securityLevel;
    info->keyMintName = keyMintName;
    info->keyMintAuthorName = keyMintAuthorName;
    info->timestampTokenRequired = false;
    return ScopedAStatus::ok();
}

ScopedAStatus AndroidKeyMintDevice::addRngEntropy(const vector<uint8_t>& data) {
    return kmError2ScopedAStatus(impl_->add_rng_entropy(
        data.data(), data.size()));
}

vector<KeyParameter> kmParamSet2Aidl(const keymaster_key_param_set_t& set) {
    vector<KeyParameter> result;
    if (set.length == 0 || set.params == nullptr) return result;

    keymaster_key_param_t* params = set.params;
    for (size_t i = 0; i < set.length; ++i) {
        auto tag = params[i].tag;
        KeyParameter key_param;
        key_param.tag = static_cast<Tag>(tag);
        KeyParameterValue key_param_value;
        switch (keymaster_tag_get_type(tag)) {
        case KM_ENUM:
        case KM_ENUM_REP:
            switch(tag) {
            case KM_TAG_ALGORITHM:
                key_param_value = KeyParameterValue::make<KeyParameterValue::algorithm>(static_cast<Algorithm>(params[i].enumerated));
                break;
            case KM_TAG_BLOCK_MODE:
                key_param_value = KeyParameterValue::make<KeyParameterValue::blockMode>(static_cast<BlockMode>(params[i].enumerated));
                break;
            case KM_TAG_PADDING:
                key_param_value = KeyParameterValue::make<KeyParameterValue::paddingMode>(static_cast<PaddingMode>(params[i].enumerated));
                break;
            case KM_TAG_DIGEST:
            case KM_TAG_RSA_OAEP_MGF_DIGEST:
                key_param_value = KeyParameterValue::make<KeyParameterValue::digest>(static_cast<Digest>(params[i].enumerated));
                break;
            case KM_TAG_EC_CURVE:
                key_param_value = KeyParameterValue::make<KeyParameterValue::ecCurve>(static_cast<EcCurve>(params[i].enumerated));
                break;
            case KM_TAG_ORIGIN:
                key_param_value = KeyParameterValue::make<KeyParameterValue::origin>(static_cast<KeyOrigin>(params[i].enumerated));
                break;
            case KM_TAG_PURPOSE:
                key_param_value = KeyParameterValue::make<KeyParameterValue::keyPurpose>(static_cast<KeyPurpose>(params[i].enumerated));
                break;
            case KM_TAG_USER_AUTH_TYPE:
                key_param_value = KeyParameterValue::make<KeyParameterValue::hardwareAuthenticatorType>(static_cast<HardwareAuthenticatorType>(params[i].enumerated));
                break;
            case KM_TAG_HARDWARE_TYPE:
                key_param_value = KeyParameterValue::make<KeyParameterValue::securityLevel>(static_cast<SecurityLevel>(params[i].enumerated));
                break;
            default:
                /* Shouldn't get here. */
                LOG_E("%s %d KM_TAG_INVALID tag=%d/%X", __func__, __LINE__, (uint32_t)tag, (uint32_t)tag);
                key_param.tag = static_cast<Tag>(KM_TAG_INVALID);
                break;
            }
            break;
        case KM_UINT:
        case KM_UINT_REP:
            key_param_value = KeyParameterValue::make<KeyParameterValue::integer>(params[i].integer);
            break;
        case KM_ULONG:
        case KM_ULONG_REP:
            key_param_value = KeyParameterValue::make<KeyParameterValue::longInteger>(params[i].long_integer);
            break;
        case KM_DATE:
            key_param_value = KeyParameterValue::make<KeyParameterValue::dateTime>(params[i].date_time);
            break;
        case KM_BOOL:
            key_param_value = KeyParameterValue::make<KeyParameterValue::boolValue>(params[i].boolean);
            break;
        case KM_BIGNUM:
        case KM_BYTES:
            key_param_value = KeyParameterValue::make<KeyParameterValue::blob>(std::vector<uint8_t>(params[i].blob.data, params[i].blob.data + params[i].blob.data_length));
            break;
        case KM_INVALID:
        default:
            LOG_E("%s %d KM_TAG_INVALID type=%d/%X", __func__, __LINE__, (uint32_t)keymaster_tag_get_type(tag), (uint32_t)keymaster_tag_get_type(tag));
            key_param.tag = static_cast<Tag>(KM_TAG_INVALID);
            /* just skip */
            break;
        }
        key_param.value = key_param_value;
        result.push_back(key_param);
    }
    return result;
}

inline vector<KeyCharacteristics> convertKeyCharacteristics(
    const keymaster_key_characteristics_t& characteristics,
    bool add_keystore_enforced)
{
    vector<KeyCharacteristics> key_characteristics = {};
    KeyCharacteristics keyMintEnforced{SecurityLevel::TRUSTED_ENVIRONMENT, kmParamSet2Aidl(characteristics.hw_enforced)};
    key_characteristics.push_back({std::move(keyMintEnforced)});
    if ((add_keystore_enforced) && (characteristics.sw_enforced.length != 0)) {
        // Put all the software authorizations in the keystore list.*
        KeyCharacteristics keystoreEnforced{SecurityLevel::KEYSTORE, kmParamSet2Aidl(characteristics.sw_enforced)};
        key_characteristics.push_back({std::move(keystoreEnforced)});
    }
    return key_characteristics;
}

Certificate convertCertificate(const keymaster_blob_t& cert) {
    return {std::vector<uint8_t>(cert.data, cert.data + cert.data_length)};
}

vector<Certificate> convertCertificateChain(const keymaster_cert_chain_t& chain) {
    vector<Certificate> retval;
    for (size_t i = 0; i < chain.entry_count; ++i) {
        retval.push_back(convertCertificate(chain.entries[i]));
    }
    return retval;
}

ScopedAStatus AndroidKeyMintDevice::generateKey(const vector<KeyParameter>& keyParams,
                                                const optional<AttestationKey>& attestationKey,
                                                KeyCreationResult* creationResult) {
    KmKeyParamSet key_params(keyParams);
    keymaster_key_blob_t key_blob = {};
    keymaster_key_characteristics_t characteristics = {};
    KmKeyParamSet* attestation_key_params = NULL;
    keymaster_key_blob_t attestation_key_blob = {};
    keymaster_blob_t attestation_issuer_blob = {};
    if (attestationKey) {
        attestation_key_blob.key_material = attestationKey->keyBlob.data();
        attestation_key_blob.key_material_size = attestationKey->keyBlob.size();
        attestation_issuer_blob.data = attestationKey->issuerSubjectName.data();
        attestation_issuer_blob.data_length = attestationKey->issuerSubjectName.size();
        attestation_key_params = new KmKeyParamSet(attestationKey->attestKeyParams);
    }
    keymaster_cert_chain_t cert_chain = {};
    keymaster_error_t ret = impl_->generate_and_attest_key(
        &key_params,
        attestationKey?&attestation_key_blob:NULL,
        attestation_key_params,
        (attestationKey->issuerSubjectName.size() != 0)?&attestation_issuer_blob:NULL,
        &key_blob, &characteristics, &cert_chain);
    if (ret != KM_ERROR_OK) {
        /* ExySp */
        keymaster_free_param_set(&key_params);
        keymaster_free_param_set(attestation_key_params);
        return kmError2ScopedAStatus(ret);
    }
    creationResult->keyBlob = kmBlob2vector(key_blob);
    creationResult->keyCharacteristics =
        convertKeyCharacteristics(characteristics, true);
    creationResult->certificateChain = convertCertificateChain(cert_chain);
    keymaster_free_param_set(&key_params);
    keymaster_free_param_set(attestation_key_params);
    keymaster_free_cert_chain(&cert_chain);
    free((void*)key_blob.key_material);
    key_blob.key_material = NULL;
    key_blob.key_material_size = 0;
    keymaster_free_characteristics(&characteristics);
    return ScopedAStatus::ok();
}

ScopedAStatus AndroidKeyMintDevice::importKey(const vector<KeyParameter>& keyParams,
                                              KeyFormat keyFormat, const vector<uint8_t>& keyData,
                                              const optional<AttestationKey>& attestationKey,
                                              KeyCreationResult* creationResult) {
    KmKeyParamSet key_params(keyParams);
    const keymaster_blob_t key_data = {keyData.data(), keyData.size()};
    keymaster_key_blob_t key_blob = {};
    keymaster_key_characteristics_t characteristics = {};
    KmKeyParamSet* attestation_key_params = NULL;
    keymaster_key_blob_t attestation_key_blob = {};
    keymaster_blob_t attestation_issuer_blob = {};
    if (attestationKey) {
        attestation_key_blob.key_material = attestationKey->keyBlob.data();
        attestation_key_blob.key_material_size = attestationKey->keyBlob.size();
        attestation_issuer_blob.data = attestationKey->issuerSubjectName.data();
        attestation_issuer_blob.data_length = attestationKey->issuerSubjectName.size();
        attestation_key_params = new KmKeyParamSet(attestationKey->attestKeyParams);
    }
    keymaster_cert_chain_t cert_chain = {};
    keymaster_error_t ret = impl_->import_and_attest_key(
        &key_params, keymaster_key_format_t(keyFormat), &key_data,
        attestationKey?&attestation_key_blob:NULL,
        attestation_key_params,
        (attestationKey->issuerSubjectName.size() != 0)?&attestation_issuer_blob:NULL,
        &key_blob, &characteristics, &cert_chain);
    if (ret != KM_ERROR_OK) {
        /* ExySp */
        keymaster_free_param_set(&key_params);
        keymaster_free_param_set(attestation_key_params);
        return kmError2ScopedAStatus(ret);
    }
    creationResult->keyBlob = kmBlob2vector(key_blob);
    creationResult->keyCharacteristics =
        convertKeyCharacteristics(characteristics, true);
    creationResult->certificateChain = convertCertificateChain(cert_chain);
    keymaster_free_param_set(&key_params);
    keymaster_free_param_set(attestation_key_params);
    keymaster_free_cert_chain(&cert_chain);
    free((void*)key_blob.key_material);
    key_blob.key_material = NULL;
    key_blob.key_material_size = 0;
    keymaster_free_characteristics(&characteristics);
    return ScopedAStatus::ok();
}

ScopedAStatus
AndroidKeyMintDevice::importWrappedKey(const vector<uint8_t>& wrappedKeyData,         //
                                       const vector<uint8_t>& wrappingKeyBlob,        //
                                       const vector<uint8_t>& maskingKey,             //
                                       const vector<KeyParameter>& unwrappingParams,  //
                                       int64_t passwordSid, int64_t biometricSid,     //
                                       KeyCreationResult* creationResult) {
    KmKeyParamSet unwrapping_key_params(unwrappingParams);
    const keymaster_blob_t wrapped_key_data = {
        wrappedKeyData.data(), wrappedKeyData.size()};
    const keymaster_key_blob_t wrapping_key_blob = {
        wrappingKeyBlob.data(), wrappingKeyBlob.size()};
    keymaster_key_blob_t key_blob = {};
    keymaster_key_characteristics_t characteristics = {};
    if (maskingKey.size() != 32) {
        return kmError2ScopedAStatus(KM_ERROR_INVALID_ARGUMENT);
    }
    keymaster_cert_chain_t cert_chain = {};
    keymaster_error_t ret = impl_->import_wrapped_key(
        &wrapped_key_data, &wrapping_key_blob,
        maskingKey.data(),
        &unwrapping_key_params,
        passwordSid, biometricSid,
        &key_blob, &characteristics, &cert_chain);
    if (ret != KM_ERROR_OK) {
        return kmError2ScopedAStatus(ret);
    }
    creationResult->keyBlob = kmBlob2vector(key_blob);
    creationResult->keyCharacteristics =
        convertKeyCharacteristics(characteristics, true);
    creationResult->certificateChain = convertCertificateChain(cert_chain);
    keymaster_free_cert_chain(&cert_chain);
    keymaster_free_param_set(&unwrapping_key_params);
    free((void*)key_blob.key_material);
    key_blob.key_material = NULL;
    key_blob.key_material_size = 0;
    keymaster_free_characteristics(&characteristics);
    return ScopedAStatus::ok();
}

ScopedAStatus AndroidKeyMintDevice::upgradeKey(const vector<uint8_t>& keyBlobToUpgrade,
                                               const vector<KeyParameter>& upgradeParams,
                                               vector<uint8_t>* keyBlob) {
    KmKeyParamSet upgrade_key_params(upgradeParams);
    const keymaster_key_blob_t key_to_upgrade = {
        keyBlobToUpgrade.data(), keyBlobToUpgrade.size()};
    keymaster_key_blob_t key_blob = {};
    keymaster_error_t ret = impl_->upgrade_key(
        &key_to_upgrade, &upgrade_key_params, &key_blob);
    if (ret != KM_ERROR_OK) {
        return kmError2ScopedAStatus(ret);
    }
    *keyBlob = kmBlob2vector(key_blob);
    keymaster_free_param_set(&upgrade_key_params);
    free((void*)key_blob.key_material);
    key_blob.key_material = NULL;
    key_blob.key_material_size = 0;
    return ScopedAStatus::ok();
}

ScopedAStatus AndroidKeyMintDevice::deleteKey(const vector<uint8_t>& keyBlob) {
    const keymaster_key_blob_t key = {
        keyBlob.data(), keyBlob.size()};
    keymaster_error_t ret = impl_->delete_key(&key);
    return kmError2ScopedAStatus(ret);
}

ScopedAStatus AndroidKeyMintDevice::deleteAllKeys() {
    keymaster_error_t ret = impl_->delete_all_keys();
    return kmError2ScopedAStatus(ret);
}

ScopedAStatus AndroidKeyMintDevice::destroyAttestationIds() {
    return kmError2ScopedAStatus(impl_->destroy_attestation_ids());
}

ScopedAStatus AndroidKeyMintDevice::begin(KeyPurpose purpose, const vector<uint8_t>& keyBlob,
                                          const vector<KeyParameter>& params,
                                          const optional<HardwareAuthToken>& authToken, BeginResult* result) {
    const keymaster_key_blob_t key = {
        keyBlob.data(), keyBlob.size()};
    KmKeyParamSet in_params(params);
    keymaster_key_param_set_t out_params = {};
    uint64_t operation_handle = 0;
    keymaster_hw_auth_token_t auth_token = {};
    if (authToken) {
        auth_token.challenge = authToken->challenge;
        auth_token.user_id = authToken->userId;
        auth_token.authenticator_id = authToken->authenticatorId;
        auth_token.authenticator_type = keymaster_hw_auth_type_t(ntohl(static_cast<uint32_t>(authToken->authenticatorType)));
        auth_token.timestamp = ntohq(authToken->timestamp.milliSeconds);
        auth_token.mac = {authToken->mac.data(), authToken->mac.size()};
    }
    keymaster_error_t ret = impl_->begin(
        keymaster_purpose_t(purpose), &key, &in_params, (authToken)?&auth_token:NULL, &out_params, &operation_handle);
    if (ret != KM_ERROR_OK) {
        return kmError2ScopedAStatus(ret);
    }
    result->params = kmParamSet2Aidl(out_params);
    result->challenge = operation_handle;
    result->operation =
        ndk::SharedRefBase::make<AndroidKeyMintOperation>(impl_, operation_handle);
    keymaster_free_param_set(&in_params);
    keymaster_free_param_set(&out_params);
    return ScopedAStatus::ok();

}

ScopedAStatus AndroidKeyMintDevice::deviceLocked(bool passwordOnly,
                               const optional<TimeStampToken>& /*timestampToken*/) {
    return kmError2ScopedAStatus(impl_->device_locked(passwordOnly));
}

ScopedAStatus AndroidKeyMintDevice::earlyBootEnded() {
    return kmError2ScopedAStatus(impl_->early_boot_ended());
}

/* ExySp */
ScopedAStatus
AndroidKeyMintDevice::convertStorageKeyToEphemeral(const std::vector<uint8_t>&storageKeyBlob ,
                                                   std::vector<uint8_t>* ephemeralKeyBlob) {

    keymaster_key_blob_t key_storage_blob = {storageKeyBlob.data(), storageKeyBlob.size()};
    keymaster_blob_t output_blob = {};

    keymaster_error_t ret = impl_->export_key(KM_KEY_FORMAT_RAW,
                                     &key_storage_blob, {}, {}, &output_blob );

    if (ret != KM_ERROR_OK) {
        return kmError2ScopedAStatus(ret);
    }

    *ephemeralKeyBlob = std::vector(output_blob.data, output_blob.data + output_blob.data_length);
    free((void*)output_blob.data);
    output_blob.data = NULL;
    output_blob.data_length = 0;
    return ScopedAStatus::ok();
}
/* End */

ScopedAStatus AndroidKeyMintDevice::getKeyCharacteristics(
    const std::vector<uint8_t>& keyBlob,
    const std::vector<uint8_t>& appId,
    const std::vector<uint8_t>& appData,
    std::vector<KeyCharacteristics>* keyCharacteristics)
{
    keymaster_key_blob_t key_blob = {keyBlob.data(), keyBlob.size()};
    keymaster_blob_t app_id = {appId.data(), appId.size()};
    keymaster_blob_t app_data = {appData.data(), appData.size()};
    keymaster_key_characteristics_t characteristics = {};
    keymaster_error_t ret = impl_->get_key_characteristics(
        &key_blob, &app_id, &app_data, &characteristics);
    if (ret != KM_ERROR_OK) {
        return kmError2ScopedAStatus(ret);
    }
    *keyCharacteristics = convertKeyCharacteristics(characteristics, false);
    keymaster_free_characteristics(&characteristics);
    return ScopedAStatus::ok();
}

ScopedAStatus AndroidKeyMintDevice::getRootOfTrustChallenge(std::array<uint8_t, 16>* rootOfTrustChallenge){
    (void)rootOfTrustChallenge;
    return kmError2ScopedAStatus(KM_ERROR_UNIMPLEMENTED);
}

ScopedAStatus AndroidKeyMintDevice::getRootOfTrust(const std::array<uint8_t, 16>& in_challenge,
        std::vector<uint8_t>* rootOfTrust) {
    keymaster_blob_t rot_blob = {};
    keymaster_error_t ret = impl_->get_root_of_trust(in_challenge.data(), &rot_blob);
    if (ret != KM_ERROR_OK) {
        return kmError2ScopedAStatus(ret);
    }
    *rootOfTrust = vector<uint8_t>(
        rot_blob.data,
        rot_blob.data + rot_blob.data_length);
    free((void*)rot_blob.data);
    rot_blob.data = NULL;
    rot_blob.data_length = 0;
    return ScopedAStatus::ok();
}

ScopedAStatus AndroidKeyMintDevice::sendRootOfTrust(const std::vector<uint8_t>& in_rootOfTrust) {
    (void)in_rootOfTrust;
    return kmError2ScopedAStatus(KM_ERROR_UNIMPLEMENTED);
}

IKeyMintDevice* CreateKeyMintDevice(SecurityLevel securityLevel) {
    return ::new AndroidKeyMintDevice(securityLevel);
}

}  // namespace aidl::android::hardware::security::keymint
