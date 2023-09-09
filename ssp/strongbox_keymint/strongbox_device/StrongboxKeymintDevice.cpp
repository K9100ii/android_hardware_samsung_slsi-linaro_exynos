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

#define LOG_TAG "android.hardware.keymint@1.0-impl_strong"

#include <android-base/logging.h>
#include <log/log.h>

#include "StrongboxKeymintDevice.h"
#include "StrongboxKeymintImpl.h"
#include "ssp_strongbox_keymaster_defs.h"
#include "ssp_strongbox_keymaster_log.h"

namespace aidl {
namespace android {
namespace hardware {
namespace security {
namespace keymint {

static keymaster_error_t set_attestation_key(
        const optional<AttestationKey>& attestationKey,
        const vector<KeyParameter>& params,
        keymaster_key_blob_t *attest_keyblob,
        keymaster_key_param_set_t *attest_params,
        keymaster_blob_t *issuer_subject)
{
    *attest_keyblob = {attestationKey->keyBlob.data(), attestationKey->keyBlob.size()};
    *attest_params = aidlKeyParams2Km(attestationKey->attestKeyParams);

    /* if issuer_subject == null : return ErrorCode::INVALID_ARGUMENT */
    if (!attestationKey->issuerSubjectName.size() ||
            !attestationKey->issuerSubjectName.data()) {
        return KM_ERROR_INVALID_ARGUMENT;
    } else {
        *issuer_subject = {
            attestationKey->issuerSubjectName.data(),
            attestationKey->issuerSubjectName.size()};
    }

    /* if attestationKey is non-null, keyParams should contain ATTESTATION_CHALLENGE */
    if (!checkParamTag(params, Tag::ATTESTATION_CHALLENGE)) {
        return KM_ERROR_ATTESTATION_CHALLENGE_MISSING;
    }

    return KM_ERROR_OK;
}

keymaster_error_t support_device_unique_attestation(
        const vector<KeyParameter>& params,
        bool is_supported)
{
    bool support = false;

    if (checkParamTag(params, Tag::ATTESTATION_ID_BRAND) ||
            checkParamTag(params, Tag::ATTESTATION_ID_DEVICE) ||
            checkParamTag(params, Tag::ATTESTATION_ID_PRODUCT) ||
            checkParamTag(params, Tag::ATTESTATION_ID_MANUFACTURER) ||
            checkParamTag(params, Tag::ATTESTATION_ID_MODEL) ||
            checkParamTag(params, Tag::ATTESTATION_ID_SERIAL) ||
            checkParamTag(params, Tag::ATTESTATION_ID_IMEI) ||
            checkParamTag(params, Tag::ATTESTATION_ID_MEID)) {
        BLOGD("have ATTESTATION_ID_*** in params");
        support = true;
    }

    if (checkParamTag(params, Tag::DEVICE_UNIQUE_ATTESTATION)) {
        BLOGD("have DEVICE_UNIQUE_ATTESTATION in params");
        support = true;
    }

    if (!is_supported)
        if (support)
            return KM_ERROR_CANNOT_ATTEST_IDS;

    return KM_ERROR_OK;
}

vector<KeyCharacteristics> convertKeyCharacteristics(
        const keymaster_key_param_set_t& sw_enforced,
        const keymaster_key_param_set_t& hw_enforced,
        bool strongbox_only)
{
    vector<KeyCharacteristics> ret;

    KeyCharacteristics keymint_hw_enforced{SecurityLevel::STRONGBOX, {}};

    keymint_hw_enforced.authorizations = kmParamSet2Aidl(hw_enforced);

    if (strongbox_only) {
        (void)sw_enforced;
    } else {
        KeyCharacteristics keymint_sw_enforced{
            SecurityLevel::KEYSTORE, kmParamSet2Aidl(sw_enforced)};

        if (!keymint_sw_enforced.authorizations.empty()) ret.push_back(keymint_sw_enforced);
    }

    if (!keymint_hw_enforced.authorizations.empty()) ret.push_back(keymint_hw_enforced);

    return ret;
}

StrongboxKeymintDevice::~StrongboxKeymintDevice() {}

ScopedAStatus StrongboxKeymintDevice::getHardwareInfo(KeyMintHardwareInfo* info)
{
    BLOGD("%s: %s: %d", __FILE__, __func__, __LINE__);

    info->versionNumber = 1;
    info->securityLevel = SecurityLevel(KM_SECURITY_LEVEL_STRONGBOX);
    info->keyMintName = "StrongboxKeyMintDevice";
    info->keyMintAuthorName = "S.LSI";

    /* timestamp is required by the StrongBox implementations */
    info->timestampTokenRequired = true;

    return ScopedAStatus::ok();
}

ScopedAStatus StrongboxKeymintDevice::addRngEntropy(const vector<uint8_t>& data)
{
    BLOGD("%s: %s: %d", __FILE__, __func__, __LINE__);

    keymaster_error_t ret = KM_ERROR_OK;

    if (data.size() == 0)
        return ScopedAStatus::ok();

    /*
     * ErrorCode::INVALID_INPUT_LENGTH if the caller
     *         provides more than 2 KiB of data
     */
    BLOGD("data size is : %lu", data.size());
    if (data.size() > (KM_ADD_RNG_ENTROPY_MAX_LEN))
        return kmError2ScopedAStatus(KM_ERROR_INVALID_INPUT_LENGTH);

    ret = impl_->add_rng_entropy(
            data.data(), data.size());

    return kmError2ScopedAStatus(ret);
}

ScopedAStatus StrongboxKeymintDevice::generateKey(
                              const vector<KeyParameter>& keyParams,
                              const optional<AttestationKey>& attestationKey,
                              KeyCreationResult* creationResult)
{
    BLOGD("%s: %s: %d", __FILE__, __func__, __LINE__);

    /* parameters */
    const KmParamSet key_params(keyParams);
    keymaster_key_blob_t key_blob = {};
    keymaster_key_characteristics_t key_characteristics = {};

    keymaster_key_blob_t attest_keyblob = {};
    keymaster_key_param_set_t attest_params = {};
    keymaster_cert_chain_t cert_chain = {};
    keymaster_blob_t issuer_subject = {};

    keymaster_error_t res = KM_ERROR_OK;

    /* attestation key */
    if (attestationKey) {
        if (KM_ERROR_OK != (res = set_attestation_key(attestationKey,
                    keyParams,
                    &attest_keyblob,
                    &attest_params,
                    &issuer_subject))) {
            /* no need to check issuer subject name in generateKey() */
            if (res != KM_ERROR_INVALID_ARGUMENT) {
                return kmError2ScopedAStatus(res);
            }
        }
    }

    if (KM_ERROR_OK != (res = support_device_unique_attestation(keyParams, false))) {
        return kmError2ScopedAStatus(res);
    }

    if (checkParamTag(keyParams, Tag::TRUSTED_USER_PRESENCE_REQUIRED)) {
        return kmError2ScopedAStatus(KM_ERROR_UNSUPPORTED_TAG);
    }

    keymaster_error_t ret = impl_->generate_attest_key(
            &key_params,
            &issuer_subject, &attest_keyblob, &attest_params,
            /* output */
            &key_blob, &key_characteristics, &cert_chain);

    if (ret != KM_ERROR_OK) {
        return kmError2ScopedAStatus(ret);
    }

    creationResult->keyBlob = kmBlob2vector(key_blob);
    creationResult->keyCharacteristics =
        convertKeyCharacteristics(key_characteristics.sw_enforced,
                key_characteristics.hw_enforced,
                false /* include sw enforced */);
    creationResult->certificateChain = kmCertChain2Aidl(cert_chain);

    /* free */
    if (key_blob.key_material && key_blob.key_material_size > 0) {
        free((void *)key_blob.key_material);
        key_blob.key_material = NULL;
        key_blob.key_material_size = 0;
    }

    keymaster_free_characteristics(&key_characteristics);
    keymaster_free_cert_chain(&cert_chain);

    return ScopedAStatus::ok();
}

ScopedAStatus StrongboxKeymintDevice::importKey(const vector<KeyParameter>& keyParams,
                            KeyFormat keyFormat,
                            const vector<uint8_t>& keyData,
                            const optional<AttestationKey>& attestationKey,
                            KeyCreationResult* creationResult)
{
    BLOGD("%s: %s: %d", __FILE__, __func__, __LINE__);

    /* parameters */
    const KmParamSet key_params(keyParams);
    keymaster_key_format_t key_format = legacy_enum_conversion(keyFormat);
    const keymaster_blob_t key_data = { keyData.data(), keyData.size() };
    keymaster_key_blob_t key_blob = {};
    keymaster_key_characteristics_t key_characteristics = {};

    keymaster_key_blob_t attest_keyblob = {};
    keymaster_key_param_set_t attest_params = {};
    keymaster_cert_chain_t cert_chain = {};
    keymaster_blob_t issuer_subject = {};

    keymaster_error_t res = KM_ERROR_OK;

    /* attestation key */
    if (attestationKey) {
        if (KM_ERROR_OK != (res = set_attestation_key(attestationKey,
                    keyParams,
                    &attest_keyblob,
                    &attest_params,
                    &issuer_subject))) {
            return kmError2ScopedAStatus(res);
        }
    }

    if (KM_ERROR_OK != (res = support_device_unique_attestation(keyParams, false))) {
        return kmError2ScopedAStatus(res);
    }

    if (checkParamTag(keyParams, Tag::TRUSTED_USER_PRESENCE_REQUIRED)) {
        return kmError2ScopedAStatus(KM_ERROR_UNSUPPORTED_TAG);
    }

    keymaster_error_t ret = impl_->import_attest_key(
            &key_params, key_format, &key_data,
            &issuer_subject, &attest_keyblob, &attest_params,
            /* output */
            &key_blob, &key_characteristics, &cert_chain);


    if (ret != KM_ERROR_OK) {
        return kmError2ScopedAStatus(ret);
    }

    /* output */
    creationResult->keyBlob = kmBlob2vector(key_blob);
    creationResult->keyCharacteristics =
        convertKeyCharacteristics(key_characteristics.sw_enforced,
                key_characteristics.hw_enforced,
                false /* include sw enforced */);
    creationResult->certificateChain = kmCertChain2Aidl(cert_chain);

    /* free */
    if (key_blob.key_material && key_blob.key_material_size > 0) {
        free((void *)key_blob.key_material);
        key_blob.key_material = NULL;
        key_blob.key_material_size = 0;
    }

    keymaster_free_characteristics(&key_characteristics);
    keymaster_free_cert_chain(&cert_chain);

    return ScopedAStatus::ok();
}

ScopedAStatus StrongboxKeymintDevice::importWrappedKey(const vector<uint8_t>& wrappedKeyData,
                               const vector<uint8_t>& wrappingKeyBlob,
                               const vector<uint8_t>& maskingKey,
                               const vector<KeyParameter>& unwrappingParams,
                               int64_t passwordSid,
                               int64_t biometricSid,
                               KeyCreationResult* creationResult)
{
    BLOGD("%s: %s: %d", __FILE__, __func__, __LINE__);

    /* input */
    const keymaster_blob_t wrapped_key_data = { wrappedKeyData.data(),
                                                wrappedKeyData.size() };
    const keymaster_key_blob_t wrapping_key_blob = { wrappingKeyBlob.data(),
                                                     wrappingKeyBlob.size() };
    const keymaster_blob_t masking_key = { maskingKey.data(), maskingKey.size() };
    const KmParamSet unwrapping_params(unwrappingParams);

    /* output */
    keymaster_key_blob_t key_blob = {};
    keymaster_key_characteristics_t key_characteristics = {};
    keymaster_cert_chain_t cert_chain = {};

    keymaster_error_t ret = impl_->import_attest_wrappedkey(
            &wrapped_key_data,
            &wrapping_key_blob,
            &masking_key,
            &unwrapping_params,
            passwordSid,
            biometricSid,
            /* output */
            &key_blob, &key_characteristics, &cert_chain);

    if (ret != KM_ERROR_OK) {
        return kmError2ScopedAStatus(ret);
    }

    creationResult->keyBlob = kmBlob2vector(key_blob);
    creationResult->keyCharacteristics =
        convertKeyCharacteristics(key_characteristics.sw_enforced,
                key_characteristics.hw_enforced,
                false /* include sw enforced */);
    creationResult->certificateChain = kmCertChain2Aidl(cert_chain);

    /* free */
    if (key_blob.key_material && key_blob.key_material_size > 0) {
        free((void *)key_blob.key_material);
        key_blob.key_material = NULL;
        key_blob.key_material_size = 0;
    }

    keymaster_free_characteristics(&key_characteristics);
    keymaster_free_cert_chain(&cert_chain);

    return ScopedAStatus::ok();
}

ScopedAStatus StrongboxKeymintDevice::upgradeKey(const vector<uint8_t>& keyBlobToUpgrade,
        const vector<KeyParameter>& upgradeParams,
        vector<uint8_t>* keyBlob)
{
    BLOGD("%s: %s: %d", __FILE__, __func__, __LINE__);

    /* input */
    const keymaster_key_blob_t keyblob_to_upgrade = { keyBlobToUpgrade.data(),
        keyBlobToUpgrade.size() };
    const KmParamSet upgrade_params(upgradeParams);

    /* output */
    keymaster_key_blob_t upgraded_key = {};

    keymaster_error_t ret = impl_->upgrade_key(
            &keyblob_to_upgrade, &upgrade_params,
            /* output */
            &upgraded_key);

    if (ret != KM_ERROR_OK) {
        return kmError2ScopedAStatus(ret);
    }

    *keyBlob = kmBlob2vector(upgraded_key);

    /* free */
    if (upgraded_key.key_material && upgraded_key.key_material_size > 0) {
        free((void *)upgraded_key.key_material);
        upgraded_key.key_material = NULL;
        upgraded_key.key_material_size = 0;
    }

    return ScopedAStatus::ok();
}

ScopedAStatus StrongboxKeymintDevice::begin(KeyPurpose purpose,
        const vector<uint8_t>& keyBlob,
        const vector<KeyParameter>& params,
        const optional<HardwareAuthToken>& authToken,
        BeginResult* result)
{
    BLOGD("%s: %s: %d", __FILE__, __func__, __LINE__);

    /* input */
    keymaster_purpose_t key_purpose = legacy_enum_conversion(purpose);
    const keymaster_key_blob_t keyblob = { keyBlob.data(), keyBlob.size() };
    const KmParamSet in_params(params);
    const kmHardwareAuthToken auth_token = authToken2kmAuthToken(authToken);

    /* output */
    keymaster_key_param_set_t out_params = {};
    uint64_t op_handle = 0; // challenge

    keymaster_error_t ret = impl_->begin(
            key_purpose, &keyblob, &in_params, &auth_token, &out_params, &op_handle);

    if (ret != KM_ERROR_OK) {
        return kmError2ScopedAStatus(ret);
    }

    result->params = kmParamSet2Aidl(out_params);
    result->challenge = op_handle;

    result->operation =
        ndk::SharedRefBase::make<StrongboxKeyMintOperation>(impl_, op_handle);

    keymaster_free_param_set(&out_params);

    return ScopedAStatus::ok();
}

ScopedAStatus StrongboxKeymintDevice::deviceLocked(bool in_passwordOnly,
        const optional<TimeStampToken>& in_timestampToken)
{
    BLOGD("%s: %s: %d", __FILE__, __func__, __LINE__);

    const kmTimestampToken timestamp_token(in_timestampToken);

    keymaster_error_t ret = impl_->device_locked(in_passwordOnly, &timestamp_token);

    return kmError2ScopedAStatus(ret);
}

ScopedAStatus StrongboxKeymintDevice::earlyBootEnded()
{
    BLOGD("%s: %s: %d", __FILE__, __func__, __LINE__);

    keymaster_error_t ret = impl_->early_boot_ended();

    return kmError2ScopedAStatus(ret);
}

ScopedAStatus StrongboxKeymintDevice::getKeyCharacteristics(
        const vector<uint8_t>& keyBlob,
        const vector<uint8_t>& appId,
        const vector<uint8_t>& appData,
        std::vector<KeyCharacteristics>* keyCharacteristics)
{
    BLOGD("%s: %s: %d", __FILE__, __func__, __LINE__);
    const keymaster_key_blob_t key_blob = { keyBlob.data(), keyBlob.size() };
    const keymaster_blob_t app_id = { appId.data(), appId.size() };
    const keymaster_blob_t app_data = { appData.data(), appData.size() };
    keymaster_key_characteristics_t key_characteristics = {};

    /* output */
    keymaster_error_t ret = impl_->get_key_characteristics(
            &key_blob, &app_id, &app_data, &key_characteristics);

    vector<KeyCharacteristics> res;

    if (ret == KM_ERROR_OK) {
        *keyCharacteristics = convertKeyCharacteristics(key_characteristics.sw_enforced,
                key_characteristics.hw_enforced,
                true /* hw enforced only */);
    }

    keymaster_free_characteristics(&key_characteristics);

    return kmError2ScopedAStatus(ret);
}

}  // namespace keymint
}  // namespace security
}  // namespace hardware
}  // namespace android
}  // namespace aidl
