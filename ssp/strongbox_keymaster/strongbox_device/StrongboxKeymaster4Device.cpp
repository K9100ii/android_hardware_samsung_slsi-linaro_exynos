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


#define LOG_TAG "android.hardware.keymaster@4.0-impl_strong"

#include <hardware/hw_auth_token.h>

#include <keymasterV4_0/key_param_output.h>
#include <android-base/logging.h>

#include "StrongboxKeymaster4Device.h"
#include "ssp_strongbox_keymaster_defs.h"
#include "ssp_strongbox_keymaster_log.h"

#include <log/log.h>

namespace android {
namespace hardware {
namespace keymaster {
namespace V4_0 {
namespace implementation {

StrongboxKeymaster4Device::~StrongboxKeymaster4Device() {}

//-------- comes from aosp: begin ------------------
inline keymaster_tag_t legacy_enum_conversion(const Tag value) {
    return keymaster_tag_t(value);
}

inline Tag legacy_enum_conversion(const keymaster_tag_t value) {
    return Tag(value);
}

inline keymaster_purpose_t legacy_enum_conversion(const KeyPurpose value) {
    return static_cast<keymaster_purpose_t>(value);
}

inline keymaster_key_format_t legacy_enum_conversion(const KeyFormat value) {
    return static_cast<keymaster_key_format_t>(value);
}

inline SecurityLevel legacy_enum_conversion(const keymaster_security_level_t value) {
    return static_cast<SecurityLevel>(value);
}

inline hw_authenticator_type_t legacy_enum_conversion(const HardwareAuthenticatorType value) {
    return static_cast<hw_authenticator_type_t>(value);
}

inline ErrorCode legacy_enum_conversion(const keymaster_error_t value) {
    return static_cast<ErrorCode>(value);
}

inline keymaster_tag_type_t typeFromTag(const keymaster_tag_t tag) {
    return keymaster_tag_get_type(tag);
}


class KmParamSet : public keymaster_key_param_set_t {
  public:
    explicit KmParamSet(const hidl_vec<KeyParameter>& keyParams) {
        params = new keymaster_key_param_t[keyParams.size()];
        length = keyParams.size();
        for (size_t i = 0; i < keyParams.size(); ++i) {
            auto tag = legacy_enum_conversion(keyParams[i].tag);
            switch (typeFromTag(tag)) {
            case KM_ENUM:
            case KM_ENUM_REP:
                params[i] = keymaster_param_enum(tag, keyParams[i].f.integer);
                break;
            case KM_UINT:
            case KM_UINT_REP:
                params[i] = keymaster_param_int(tag, keyParams[i].f.integer);
                break;
            case KM_ULONG:
            case KM_ULONG_REP:
                params[i] = keymaster_param_long(tag, keyParams[i].f.longInteger);
                break;
            case KM_DATE:
                params[i] = keymaster_param_date(tag, keyParams[i].f.dateTime);
                break;
            case KM_BOOL:
                if (keyParams[i].f.boolValue)
                    params[i] = keymaster_param_bool(tag);
                else
                    params[i].tag = KM_TAG_INVALID;
                break;
            case KM_BIGNUM:
            case KM_BYTES:
                params[i] =
                    keymaster_param_blob(tag, &keyParams[i].blob[0], keyParams[i].blob.size());
                break;
            case KM_INVALID:
            default:
                params[i].tag = KM_TAG_INVALID;
                /* just skip */
                break;
            }
        }
    }
    KmParamSet(KmParamSet&& other) : keymaster_key_param_set_t{other.params, other.length} {
        other.length = 0;
        other.params = nullptr;
    }
    KmParamSet(const KmParamSet&) = delete;
    ~KmParamSet() { delete[] params; }
};

inline hidl_vec<uint8_t> kmBlob2hidlVec(const keymaster_key_blob_t& blob) {
    hidl_vec<uint8_t> result;
    result.setToExternal(const_cast<unsigned char*>(blob.key_material), blob.key_material_size);
    return result;
}

inline hidl_vec<uint8_t> kmBlob2hidlVec(const keymaster_blob_t& blob) {
    hidl_vec<uint8_t> result;
    BLOGD("blob.data(%lx), blob.data_length(%zu)\n", (uint64_t)blob.data, blob.data_length);
    result.setToExternal(const_cast<unsigned char*>(blob.data), blob.data_length);
    return result;
}

inline keymaster_key_blob_t kmHidlVec2keyblob(const hidl_vec<uint8_t>& blob) {
    keymaster_key_blob_t result = {blob.data(), blob.size()};
    return result;
}

inline keymaster_blob_t kmHidlVec2blob(const hidl_vec<uint8_t>& blob) {
    keymaster_blob_t result = {blob.data(), blob.size()};
    return result;
}

inline static hidl_vec<hidl_vec<uint8_t>>
kmCertChain2Hidl(const keymaster_cert_chain_t& cert_chain) {
    hidl_vec<hidl_vec<uint8_t>> result;
    if (!cert_chain.entry_count || !cert_chain.entries)
        return result;

    result.resize(cert_chain.entry_count);
    for (size_t i = 0; i < cert_chain.entry_count; ++i) {
        result[i] = kmBlob2hidlVec(cert_chain.entries[i]);
    }

    return result;
}

static inline hidl_vec<KeyParameter> kmParamSet2Hidl(const keymaster_key_param_set_t& set) {
    hidl_vec<KeyParameter> result;
    result.resize(set.length);

    if (set.length == 0 || set.params == nullptr) {
        return result;
    }

    keymaster_key_param_t* params = set.params;
    for (size_t i = 0; i < set.length; ++i) {
        auto tag = params[i].tag;
        result[i].tag = legacy_enum_conversion(tag);
        switch (typeFromTag(tag)) {
        case KM_ENUM:
        case KM_ENUM_REP:
            result[i].f.integer = params[i].enumerated;
            break;
        case KM_UINT:
        case KM_UINT_REP:
            result[i].f.integer = params[i].integer;
            break;
        case KM_ULONG:
        case KM_ULONG_REP:
            result[i].f.longInteger = params[i].long_integer;
            break;
        case KM_DATE:
            result[i].f.dateTime = params[i].date_time;
            break;
        case KM_BOOL:
            result[i].f.boolValue = params[i].boolean;
            break;
        case KM_BIGNUM:
        case KM_BYTES:
            result[i].blob.setToExternal(const_cast<unsigned char*>(params[i].blob.data),
                                         params[i].blob.data_length);
            break;
        case KM_INVALID:
        default:
            params[i].tag = KM_TAG_INVALID;
            /* just skip */
            break;
        }
    }
    return result;
}

//-------- comes from aosp: end ------------------

inline HmacSharingParameters kmHmacSharingParam2Hidl(
    const keymaster_hmac_sharing_params_t& params)
{
    HmacSharingParameters result;
    result.seed = kmBlob2hidlVec(params.seed);
    result.nonce = params.nonce;
    return result;
}

class kmHmacSharingParamsSet : public keymaster_hmac_sharing_params_set_t {
public:
    kmHmacSharingParamsSet(const hidl_vec<HmacSharingParameters>& hmac_sharing_params) {
        params = new keymaster_hmac_sharing_params_t[hmac_sharing_params.size()];
        length = hmac_sharing_params.size();
        for (size_t i = 0; i < hmac_sharing_params.size(); ++i) {
            params[i].seed = kmHidlVec2blob(hmac_sharing_params[i].seed);
            memcpy(params[i].nonce, hmac_sharing_params[i].nonce.data(), 32);
        }
    }

    kmHmacSharingParamsSet(const kmHmacSharingParamsSet&) = delete;
    kmHmacSharingParamsSet() { delete[] params; }
};

class kmHardwareAuthToken : public keymaster_hw_auth_token_t {
public:
    kmHardwareAuthToken(const HardwareAuthToken& authToken) {
        challenge = authToken.challenge;
        user_id = authToken.userId;
        authenticator_id = authToken.authenticatorId;
        authenticator_type = keymaster_hw_auth_type_t(ntohl(static_cast<uint32_t>(authToken.authenticatorType)));
        timestamp = ntohq(authToken.timestamp);
        mac = kmHidlVec2blob(authToken.mac);
    }
};

class kmVerifyToken : public keymaster_verification_token_t {
public:
    kmVerifyToken(const VerificationToken& verificationToken) {
        challenge = verificationToken.challenge;
        timestamp = verificationToken.timestamp;
        KmParamSet params_verified(verificationToken.parametersVerified);
        parameters_verified = params_verified;
        security_level = keymaster_security_level_t(verificationToken.securityLevel);
        mac = kmHidlVec2blob(verificationToken.mac);
    }
};

Return<void> StrongboxKeymaster4Device::getHardwareInfo(getHardwareInfo_cb _hidl_cb) {
	const SecurityLevel securityLevel = SecurityLevel(KM_SECURITY_LEVEL_STRONGBOX);
    _hidl_cb(securityLevel,
             "StrongboxKeymasterDevice", "S.LSI");
    return Void();
}

Return<void> StrongboxKeymaster4Device::getHmacSharingParameters(getHmacSharingParameters_cb _hidl_cb) {
	BLOGI("%s: %s: %d", __FILE__, __func__, __LINE__);

	keymaster_hmac_sharing_params_t hmac_params = {};
    keymaster_error_t ret = impl_->get_hmac_sharing_parameters(
        &hmac_params);

    ssp_print_buf("nonce", hmac_params.nonce, 32);
    HmacSharingParameters params = kmHmacSharingParam2Hidl(hmac_params);

    _hidl_cb(ErrorCode(ret), params);
    return Void();
}

Return<void> StrongboxKeymaster4Device::computeSharedHmac(
    const hidl_vec<::android::hardware::keymaster::V4_0::HmacSharingParameters>& params,
	computeSharedHmac_cb _hidl_cb) {

    BLOGI("%s: %s: %d", __FILE__, __func__, __LINE__);

    const kmHmacSharingParamsSet _params(params);
    keymaster_blob_t _sharing_check = {};
    _sharing_check.data = NULL;
    _sharing_check.data_length = 0;

    keymaster_error_t ret = impl_->compute_shared_hmac(
        &_params, &_sharing_check);

    hidl_vec<uint8_t> sharingCheck = kmBlob2hidlVec(_sharing_check);
    _hidl_cb(ErrorCode(ret), sharingCheck);

    if (_sharing_check.data && _sharing_check.data_length > 0) {
        free((void *)_sharing_check.data);
        _sharing_check.data = NULL;
        _sharing_check.data_length = 0;
    }

    return Void();
}

Return<void> StrongboxKeymaster4Device::verifyAuthorization(
    uint64_t challenge, const hidl_vec<KeyParameter>& parametersToVerify,
    const ::android::hardware::keymaster::V4_0::HardwareAuthToken& authToken,
    verifyAuthorization_cb _hidl_cb) {

	BLOGI("%s: %s: %d", __FILE__, __func__, __LINE__);

	(void)challenge;
	(void)parametersToVerify;
	(void)authToken;

    _hidl_cb(ErrorCode(KM_ERROR_UNIMPLEMENTED), VerificationToken());

    return Void();
}

Return<ErrorCode> StrongboxKeymaster4Device::addRngEntropy(const hidl_vec<uint8_t>& data) {
	BLOGI("%s: %s: %d", __FILE__, __func__, __LINE__);
    keymaster_error_t ret;

    if (data.size() == 0)
        return ErrorCode(OK);

    /*
    * ErrorCode::INVALID_INPUT_LENGTH if the caller
    *         provides more than 2 KiB of data
    */
    if (data.size() > (KM_ADD_RNG_ENTROPY_MAX_LEN))
        return ErrorCode(KM_ERROR_INVALID_INPUT_LENGTH);

    ret = impl_->add_rng_entropy(
        data.data(), data.size());

    return ErrorCode(KM_ERROR_OK);
}

Return<void> StrongboxKeymaster4Device::generateKey(const hidl_vec<KeyParameter>& keyParams,
                                                  generateKey_cb _hidl_cb) {
	BLOGI("%s: %s: %d", __FILE__, __func__, __LINE__);

    const KmParamSet key_params(keyParams);
    keymaster_key_blob_t key_blob = {};
    keymaster_key_characteristics_t key_characteristics = {};

    keymaster_error_t ret = impl_->generate_key(
        &key_params, &key_blob, &key_characteristics);

    KeyCharacteristics resultCharacteristics;
    hidl_vec<uint8_t> resultKeyBlob;

    if (ret == KM_ERROR_OK) {
        resultKeyBlob = kmBlob2hidlVec(key_blob);
        resultCharacteristics.hardwareEnforced = kmParamSet2Hidl(key_characteristics.hw_enforced);
        resultCharacteristics.softwareEnforced = kmParamSet2Hidl(key_characteristics.sw_enforced);
    }

    _hidl_cb(ErrorCode(ret), resultKeyBlob, resultCharacteristics);

    if (key_blob.key_material && key_blob.key_material_size > 0) {
        free((void *)key_blob.key_material);
        key_blob.key_material = NULL;
        key_blob.key_material_size= 0;
    }

    keymaster_free_characteristics(&key_characteristics);

    return Void();
}

Return<void> StrongboxKeymaster4Device::getKeyCharacteristics(const hidl_vec<uint8_t>& keyBlob,
                                                            const hidl_vec<uint8_t>& clientId,
                                                            const hidl_vec<uint8_t>& appData,
                                                            getKeyCharacteristics_cb _hidl_cb) {
	BLOGI("%s: %s: %d", __FILE__, __func__, __LINE__);

    const keymaster_key_blob_t key_blob = kmHidlVec2keyblob(keyBlob);
    const keymaster_blob_t client_id = kmHidlVec2blob(clientId);
    const keymaster_blob_t app_data = kmHidlVec2blob(appData);
    keymaster_key_characteristics_t key_characteristics = {};

    keymaster_error_t ret = impl_->get_key_characteristics(
        &key_blob, &client_id, &app_data, &key_characteristics);

    KeyCharacteristics resultCharacteristics;
    if (ret == KM_ERROR_OK) {
        resultCharacteristics.hardwareEnforced = kmParamSet2Hidl(key_characteristics.hw_enforced);
        resultCharacteristics.softwareEnforced = kmParamSet2Hidl(key_characteristics.sw_enforced);
    }

    LOG(DEBUG) << resultCharacteristics;

    _hidl_cb(ErrorCode(ret), resultCharacteristics);

    keymaster_free_characteristics(&key_characteristics);

    return Void();
}

Return<void> StrongboxKeymaster4Device::importKey(const hidl_vec<KeyParameter>& params,
                                                KeyFormat keyFormat,
                                                const hidl_vec<uint8_t>& keyData,
                                                importKey_cb _hidl_cb) {

	BLOGI("%s: %s: %d", __FILE__, __func__, __LINE__);

    /* input */
    const KmParamSet key_params(params);
    keymaster_key_format_t key_format = legacy_enum_conversion(keyFormat);
    const keymaster_blob_t key_data = kmHidlVec2blob(keyData);

    /* output */
    keymaster_key_blob_t key_blob = {};
    keymaster_key_characteristics_t key_characteristics = {};

    keymaster_error_t ret = impl_->import_key(
        &key_params, key_format, &key_data, &key_blob, &key_characteristics);

    KeyCharacteristics resultCharacteristics;
    hidl_vec<uint8_t> resultKeyBlob;

    if (ret == KM_ERROR_OK) {
        resultKeyBlob = kmBlob2hidlVec(key_blob);
        resultCharacteristics.hardwareEnforced = kmParamSet2Hidl(key_characteristics.hw_enforced);
        resultCharacteristics.softwareEnforced = kmParamSet2Hidl(key_characteristics.sw_enforced);
    }

    _hidl_cb(ErrorCode(ret), resultKeyBlob, resultCharacteristics);

    if (key_blob.key_material && key_blob.key_material_size > 0) {
        free((void *)key_blob.key_material);
        key_blob.key_material = NULL;
        key_blob.key_material_size= 0;
    }

    keymaster_free_characteristics(&key_characteristics);

    return Void();
}

Return<void> StrongboxKeymaster4Device::importWrappedKey(
    const hidl_vec<uint8_t>& wrappedKeyData, const hidl_vec<uint8_t>& wrappingKeyBlob,
    const hidl_vec<uint8_t>& maskingKey, const hidl_vec<KeyParameter>& unwrappingParams,
    uint64_t passwordSid, uint64_t biometricSid, importWrappedKey_cb _hidl_cb) {

	BLOGI("%s: %s: %d", __FILE__, __func__, __LINE__);

    /* input */
    const keymaster_blob_t wrapped_key_data = kmHidlVec2blob(wrappedKeyData);
    const keymaster_key_blob_t wrapping_key_blob = kmHidlVec2keyblob(wrappingKeyBlob);
    const keymaster_blob_t masking_key = kmHidlVec2blob(maskingKey);
    const KmParamSet unwrapping_params(unwrappingParams);

    /* output */
    keymaster_key_blob_t key_blob = {};
    keymaster_key_characteristics_t key_characteristics = {};

    keymaster_error_t ret = impl_->import_wrappedkey(
        &wrapped_key_data, &wrapping_key_blob, &masking_key, &unwrapping_params,
        passwordSid, biometricSid,
        &key_blob, &key_characteristics);

    KeyCharacteristics resultCharacteristics;
    hidl_vec<uint8_t> resultKeyBlob;

    if (ret == KM_ERROR_OK) {
        resultKeyBlob = kmBlob2hidlVec(key_blob);
        resultCharacteristics.hardwareEnforced = kmParamSet2Hidl(key_characteristics.hw_enforced);
        resultCharacteristics.softwareEnforced = kmParamSet2Hidl(key_characteristics.sw_enforced);
    }

    _hidl_cb(ErrorCode(ret), resultKeyBlob, resultCharacteristics);

    if (key_blob.key_material && key_blob.key_material_size > 0) {
        free((void *)key_blob.key_material);
        key_blob.key_material = NULL;
        key_blob.key_material_size = 0;
    }

    keymaster_free_characteristics(&key_characteristics);

    return Void();
}

Return<void> StrongboxKeymaster4Device::exportKey(KeyFormat exportFormat,
                                                const hidl_vec<uint8_t>& keyBlob,
                                                const hidl_vec<uint8_t>& clientId,
                                                const hidl_vec<uint8_t>& appData,
                                                exportKey_cb _hidl_cb) {
	BLOGI("%s: %s: %d", __FILE__, __func__, __LINE__);

    keymaster_key_format_t keyformat = legacy_enum_conversion(exportFormat);
    const keymaster_key_blob_t keyblob_to_export = kmHidlVec2keyblob(keyBlob);
    const keymaster_blob_t client_id = kmHidlVec2blob(clientId);
    const keymaster_blob_t app_data = kmHidlVec2blob(appData);
    keymaster_blob_t exported_keyblob = {};

    keymaster_error_t ret = impl_->export_key(
        keyformat, &keyblob_to_export, &client_id, &app_data, &exported_keyblob);

    hidl_vec<uint8_t> resultKeyBlob;
    if (ret == KM_ERROR_OK) {
        resultKeyBlob = kmBlob2hidlVec(exported_keyblob);
    }

    _hidl_cb(ErrorCode(ret), resultKeyBlob);


    if (exported_keyblob.data && exported_keyblob.data_length> 0) {
        free((void *)exported_keyblob.data);
        exported_keyblob.data = NULL;
        exported_keyblob.data_length = 0;
    }

    return Void();
}

Return<void> StrongboxKeymaster4Device::attestKey(const hidl_vec<uint8_t>& keyToAttest,
                                                const hidl_vec<KeyParameter>& attestParams,
                                                attestKey_cb _hidl_cb) {
	BLOGI("%s: %s: %d", __FILE__, __func__, __LINE__);

    const keymaster_key_blob_t attest_keyblob = kmHidlVec2keyblob(keyToAttest);
    const KmParamSet attest_params(attestParams);
    keymaster_cert_chain_t cert_chain = {};

    keymaster_error_t ret = impl_->attest_key(
        &attest_keyblob, &attest_params, &cert_chain);

    hidl_vec<hidl_vec<uint8_t>> resultCertChain;
    if (ret == KM_ERROR_OK) {
        resultCertChain = kmCertChain2Hidl(cert_chain);
    }

    _hidl_cb(ErrorCode(ret), resultCertChain);

    keymaster_free_cert_chain(&cert_chain);

    return Void();
}

Return<void> StrongboxKeymaster4Device::upgradeKey(const hidl_vec<uint8_t>& keyBlobToUpgrade,
                                                 const hidl_vec<KeyParameter>& upgradeParams,
                                                 upgradeKey_cb _hidl_cb) {
	BLOGI("%s: %s: %d", __FILE__, __func__, __LINE__);

    const keymaster_key_blob_t keyblob_to_upgrade = kmHidlVec2keyblob(keyBlobToUpgrade);
    const KmParamSet upgrade_params(upgradeParams);
    keymaster_key_blob_t upgraded_key = {};

    keymaster_error_t ret = impl_->upgrade_key(
        &keyblob_to_upgrade, &upgrade_params, &upgraded_key);

    if (ret == KM_ERROR_OK) {
        _hidl_cb(ErrorCode::OK, kmBlob2hidlVec(upgraded_key));
    } else {
        _hidl_cb(ErrorCode(ret), hidl_vec<uint8_t>());
    }

    if (upgraded_key.key_material && upgraded_key.key_material_size > 0) {
        free((void *)upgraded_key.key_material);
        upgraded_key.key_material = NULL;
        upgraded_key.key_material_size = 0;
    }

    return Void();
}

Return<ErrorCode> StrongboxKeymaster4Device::deleteKey(const hidl_vec<uint8_t>& keyBlob) {
    BLOGI("%s: %s: %d", __FILE__, __func__, __LINE__);
    (void)keyBlob;
    return ErrorCode::UNIMPLEMENTED;
}

Return<ErrorCode> StrongboxKeymaster4Device::deleteAllKeys() {
	BLOGI("%s: %s: %d", __FILE__, __func__, __LINE__);

    return ErrorCode(KM_ERROR_UNIMPLEMENTED);
}

Return<ErrorCode> StrongboxKeymaster4Device::destroyAttestationIds() {
	BLOGI("%s: %s: %d", __FILE__, __func__, __LINE__);

    return ErrorCode::UNIMPLEMENTED;
}

Return<void> StrongboxKeymaster4Device::begin(KeyPurpose purpose, const hidl_vec<uint8_t>& key,
                                            const hidl_vec<KeyParameter>& inParams,
                                            const HardwareAuthToken& authToken,
                                            begin_cb _hidl_cb) {
	BLOGI("%s: %s: %d", __FILE__, __func__, __LINE__);

    keymaster_purpose_t key_purpose = legacy_enum_conversion(purpose);
    const keymaster_key_blob_t keyblob = kmHidlVec2keyblob(key);
    const KmParamSet in_params(inParams);
    const kmHardwareAuthToken auth_token(authToken);
    keymaster_key_param_set_t out_params = {};
    uint64_t op_handle = 0;

    keymaster_error_t ret = impl_->begin(
           key_purpose, &keyblob, &in_params, &auth_token, &out_params, &op_handle);

    hidl_vec<KeyParameter> resultParams;
    if (ret == KM_ERROR_OK) {
        resultParams = kmParamSet2Hidl(out_params);
    }

    _hidl_cb(ErrorCode(ret), resultParams, op_handle);

    keymaster_free_param_set(&out_params);

    return Void();
}

Return<void> StrongboxKeymaster4Device::update(uint64_t operationHandle,
                                             const hidl_vec<KeyParameter>& inParams,
                                             const hidl_vec<uint8_t>& input,
                                             const HardwareAuthToken& authToken,
                                             const VerificationToken& verificationToken,
                                             update_cb _hidl_cb) {
	BLOGI("%s: %s: %d", __FILE__, __func__, __LINE__);

    const KmParamSet in_params(inParams);
    const keymaster_blob_t input_data = kmHidlVec2blob(input);
    const kmHardwareAuthToken auth_token(authToken);
    const kmVerifyToken verification_token(verificationToken);
    uint32_t resultConsumed = 0;
    keymaster_key_param_set_t out_params = {};
    keymaster_blob_t output_data = {};

    keymaster_error_t ret = impl_->update(
        operationHandle, &in_params, &input_data,
        &auth_token, &verification_token,
        &resultConsumed, &out_params, &output_data);

    hidl_vec<KeyParameter> resultParams;
    hidl_vec<uint8_t> resultBlob;

    if (ret == KM_ERROR_OK) {
        resultParams = kmParamSet2Hidl(out_params);
        resultBlob = kmBlob2hidlVec(output_data);
    }

    _hidl_cb(ErrorCode(ret), resultConsumed, resultParams, resultBlob);

    BLOGI("%s: %s: %d", __FILE__, __func__, __LINE__);

    if (output_data.data && output_data.data_length > 0) {
        free((void *)output_data.data);
        output_data.data = NULL;
        output_data.data_length = 0;
    }

    keymaster_free_param_set(&out_params);

    return Void();
}

Return<void> StrongboxKeymaster4Device::finish(uint64_t operationHandle,
                                             const hidl_vec<KeyParameter>& inParams,
                                             const hidl_vec<uint8_t>& input,
                                             const hidl_vec<uint8_t>& signature,
                                             const HardwareAuthToken& authToken,
                                             const VerificationToken& verificationToken,
                                             finish_cb _hidl_cb) {

	BLOGI("%s: %s: %d", __FILE__, __func__, __LINE__);

    const KmParamSet in_params(inParams);
    const keymaster_blob_t input_data = kmHidlVec2blob(input);
    const keymaster_blob_t in_signature = kmHidlVec2blob(signature);
    const kmHardwareAuthToken auth_token(authToken);
    const kmVerifyToken verification_token(verificationToken);
    keymaster_key_param_set_t out_params = {};
    keymaster_blob_t output_data = {};

    keymaster_error_t ret = impl_->finish(
            operationHandle,
            &in_params, &input_data,
            &in_signature,
            &auth_token,
            &verification_token,
            &out_params, &output_data);

    hidl_vec<KeyParameter> resultParams;
    hidl_vec<uint8_t> resultBlob;

    if (ret == KM_ERROR_OK) {
        resultParams = kmParamSet2Hidl(out_params);
        resultBlob = kmBlob2hidlVec(output_data);
    }

    _hidl_cb(ErrorCode(ret), resultParams, resultBlob);

    if (output_data.data && output_data.data_length > 0) {
        BLOGI("output_addr(%p): size(%zu)", output_data.data, output_data.data_length);
        free((void *)output_data.data);
        output_data.data = NULL;
        output_data.data_length = 0;
    }

    keymaster_free_param_set(&out_params);

    return Void();
}

Return<ErrorCode> StrongboxKeymaster4Device::abort(uint64_t operationHandle) {
    BLOGI("%s: %s: %d", __FILE__, __func__, __LINE__);

	keymaster_error_t ret = impl_->abort(operationHandle);

    return ErrorCode(ret);
}

}  // namespace implementation
}  // namespace V4_0
}  // namespace keymaster
}  // namespace hardware
}  // namespace android
