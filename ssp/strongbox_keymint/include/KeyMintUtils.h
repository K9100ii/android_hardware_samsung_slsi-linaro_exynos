/*
 * Copyright 2020, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _KEYMINT_UTILS_H_
#define _KEYMINT_UTILS_H_

#include <android-base/logging.h>
#include <android-base/properties.h>
#include <keymasterV4_0/keymaster_utils.h>

#include <log/log.h>

#include <aidl/android/hardware/security/keymint/Certificate.h>
#include <aidl/android/hardware/security/keymint/HardwareAuthToken.h>
#include <aidl/android/hardware/security/keymint/HardwareAuthenticatorType.h>
#include <aidl/android/hardware/security/keymint/KeyFormat.h>
#include <aidl/android/hardware/security/keymint/KeyParameter.h>
#include <aidl/android/hardware/security/keymint/KeyParameterValue.h>
#include <aidl/android/hardware/security/keymint/KeyPurpose.h>
#include <aidl/android/hardware/security/keymint/SecurityLevel.h>
#include <aidl/android/hardware/security/keymint/Tag.h>
#include <aidl/android/hardware/security/secureclock/ISecureClock.h>
#include <aidl/android/hardware/security/sharedsecret/SharedSecretParameters.h>

#include "ssp_strongbox_auth_token.h"
#include "ssp_strongbox_keymaster_defs.h"
#include "ssp_strongbox_keymaster_log.h"

namespace aidl::android::hardware::security::keymint::km_utils {

    using ::ndk::ScopedAStatus;
    using std::vector;
    using std::optional;
    using ::android::hardware::hidl_vec;
    using ::aidl::android::hardware::security::secureclock::TimeStampToken;
    using ::aidl::android::hardware::security::sharedsecret::SharedSecretParameters;

    inline keymaster_tag_t legacy_enum_conversion(const Tag value) {
        return static_cast<keymaster_tag_t>(value);
    }

    inline Tag legacy_enum_conversion(const keymaster_tag_t value) {
        return static_cast<Tag>(value);
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

    inline ScopedAStatus kmError2ScopedAStatus(const keymaster_error_t value) {
        return (value == KM_ERROR_OK
                ? ScopedAStatus::ok()
                : ScopedAStatus(AStatus_fromServiceSpecificError(static_cast<int32_t>(value))));
    }

    inline keymaster_tag_type_t typeFromTag(const keymaster_tag_t tag) {
        return keymaster_tag_get_type(tag);
    }

    inline keymaster_key_blob_t kmAidlVec2keyblob(const hidl_vec<uint8_t>& blob) {
        keymaster_key_blob_t result = {blob.data(), blob.size()};
        return result;
    }

    inline keymaster_blob_t kmAidlVec2blob(const hidl_vec<uint8_t>& blob) {
        keymaster_blob_t result = {blob.data(), blob.size()};
        return result;
    }

    keymaster_key_param_set_t aidlKeyParams2Km(const vector<KeyParameter>& keyParams);
    KeyParameter kmEnumParam2Aidl(const keymaster_key_param_t& param);

    inline keymaster_key_param_set_t addBlobValueParamSet(keymaster_tag_t tag,
            const keymaster_blob_t blob) {
        keymaster_key_param_set_t set = {};
        keymaster_key_param_t param = {};
        param.tag = tag;
        param.blob = blob;

        set.length = 1;
        set.params = &param;
        return set;

    }

    inline keymaster_key_param_t kInvalidTag{.tag = KM_TAG_INVALID, .integer = 0};

    template <KeyParameterValue::Tag aidl_tag>
        keymaster_key_param_t aidlEnumVal2Km(keymaster_tag_t km_tag, const KeyParameterValue& value) {
            return value.getTag() == aidl_tag
                ? keymaster_param_enum(km_tag, static_cast<uint32_t>(value.get<aidl_tag>()))
                : kInvalidTag;
        }

    inline keymaster_key_param_t aidlEnumParam2Km(const KeyParameter& param) {
        auto tag = km_utils::legacy_enum_conversion(param.tag);
        switch (tag) {
            case KM_TAG_PURPOSE:
                return aidlEnumVal2Km<KeyParameterValue::keyPurpose>(tag, param.value);
            case KM_TAG_ALGORITHM:
                return aidlEnumVal2Km<KeyParameterValue::algorithm>(tag, param.value);
            case KM_TAG_BLOCK_MODE:
                return aidlEnumVal2Km<KeyParameterValue::blockMode>(tag, param.value);
            case KM_TAG_DIGEST:
            case KM_TAG_RSA_OAEP_MGF_DIGEST:
                return aidlEnumVal2Km<KeyParameterValue::digest>(tag, param.value);
            case KM_TAG_PADDING:
                return aidlEnumVal2Km<KeyParameterValue::paddingMode>(tag, param.value);
            case KM_TAG_EC_CURVE:
                return aidlEnumVal2Km<KeyParameterValue::ecCurve>(tag, param.value);
            case KM_TAG_USER_AUTH_TYPE:
                return aidlEnumVal2Km<KeyParameterValue::hardwareAuthenticatorType>(tag, param.value);
            case KM_TAG_ORIGIN:
                return aidlEnumVal2Km<KeyParameterValue::origin>(tag, param.value);
            case KM_TAG_BLOB_USAGE_REQUIREMENTS:
            case KM_TAG_KDF:
            default:
                CHECK(false) << "Unknown or unused enum tag: Something is broken";
                return keymaster_param_enum(tag, false);
        }
    }

    inline keymaster_key_param_set_t aidlKeyParams2Km(const vector<KeyParameter>& keyParams) {
        keymaster_key_param_set_t set;

        set.params = static_cast<keymaster_key_param_t*>(
                malloc(keyParams.size() * sizeof(keymaster_key_param_t)));
        set.length = keyParams.size();

        for (size_t i = 0; i < keyParams.size(); ++i) {
            const auto& param = keyParams[i];
            auto tag = legacy_enum_conversion(param.tag);
            switch (typeFromTag(tag)) {

                case KM_ENUM:
                case KM_ENUM_REP:
                    set.params[i] = aidlEnumParam2Km(param);
                    break;

                case KM_UINT:
                case KM_UINT_REP:
                    set.params[i] =
                        param.value.getTag() == KeyParameterValue::integer
                        ? keymaster_param_int(tag, param.value.get<KeyParameterValue::integer>())
                        : kInvalidTag;
                    break;

                case KM_ULONG:
                case KM_ULONG_REP:
                    set.params[i] =
                        param.value.getTag() == KeyParameterValue::longInteger
                        ? keymaster_param_long(tag, param.value.get<KeyParameterValue::longInteger>())
                        : kInvalidTag;
                    break;

                case KM_DATE:
                    set.params[i] =
                        param.value.getTag() == KeyParameterValue::dateTime
                        ? keymaster_param_date(tag, param.value.get<KeyParameterValue::dateTime>())
                        : kInvalidTag;
                    break;

                case KM_BOOL:
                    set.params[i] = keymaster_param_bool(tag);
                    break;

                case KM_BIGNUM:
                case KM_BYTES:
                    if (param.value.getTag() == KeyParameterValue::blob) {
                        const auto& value = param.value.get<KeyParameterValue::blob>();
                        uint8_t* copy = static_cast<uint8_t*>(malloc(value.size()));
                        std::copy(value.begin(), value.end(), copy);
                        set.params[i] = keymaster_param_blob(tag, copy, value.size());
                    } else {
                        set.params[i] = kInvalidTag;
                    }
                    break;

                case KM_INVALID:
                default:
                    CHECK(false) << "Invalid tag: Something is broken";
                    set.params[i].tag = KM_TAG_INVALID;
                    /* just skip */
                    break;
            }
        }

        return set;
    }

    inline KeyParameter kmEnumParam2Aidl(const keymaster_key_param_t& param) {
        switch (param.tag) {
            case KM_TAG_PURPOSE:
                return KeyParameter{Tag::PURPOSE,
                    KeyParameterValue::make<KeyParameterValue::keyPurpose>(
                            static_cast<KeyPurpose>(param.enumerated))};
            case KM_TAG_ALGORITHM:
                return KeyParameter{Tag::ALGORITHM,
                    KeyParameterValue::make<KeyParameterValue::algorithm>(
                            static_cast<Algorithm>(param.enumerated))};
            case KM_TAG_BLOCK_MODE:
                return KeyParameter{Tag::BLOCK_MODE,
                    KeyParameterValue::make<KeyParameterValue::blockMode>(
                            static_cast<BlockMode>(param.enumerated))};
            case KM_TAG_DIGEST:
                return KeyParameter{Tag::DIGEST,
                    KeyParameterValue::make<KeyParameterValue::digest>(
                            static_cast<Digest>(param.enumerated))};
            case KM_TAG_PADDING:
                return KeyParameter{Tag::PADDING,
                    KeyParameterValue::make<KeyParameterValue::paddingMode>(
                            static_cast<PaddingMode>(param.enumerated))};
            case KM_TAG_EC_CURVE:
                return KeyParameter{Tag::EC_CURVE,
                    KeyParameterValue::make<KeyParameterValue::ecCurve>(
                            static_cast<EcCurve>(param.enumerated))};
            case KM_TAG_USER_AUTH_TYPE:
                return KeyParameter{Tag::USER_AUTH_TYPE,
                    KeyParameterValue::make
                        <KeyParameterValue::hardwareAuthenticatorType>(
                                static_cast<HardwareAuthenticatorType>
                                (param.enumerated))};
            case KM_TAG_ORIGIN:
                return KeyParameter{Tag::ORIGIN,
                    KeyParameterValue::make<KeyParameterValue::origin>(
                            static_cast<KeyOrigin>(param.enumerated))};
            case KM_TAG_BLOB_USAGE_REQUIREMENTS:
            case KM_TAG_KDF:
            default:
                return KeyParameter{Tag::INVALID, false};
        }
    }

    inline KeyParameter kmParam2Aidl(const keymaster_key_param_t& param) {
        auto tag = legacy_enum_conversion(param.tag);
        switch (typeFromTag(param.tag)) {
            case KM_ENUM:
            case KM_ENUM_REP:
                return kmEnumParam2Aidl(param);
                break;

            case KM_UINT:
            case KM_UINT_REP:
                return KeyParameter{tag,
                    KeyParameterValue::make<KeyParameterValue::integer>(param.integer)};

            case KM_ULONG:
            case KM_ULONG_REP:
                return KeyParameter{
                    tag, KeyParameterValue::make<KeyParameterValue::longInteger>(param.long_integer)};
                break;

            case KM_DATE:
                return KeyParameter{tag,
                    KeyParameterValue::make<KeyParameterValue::dateTime>(param.date_time)};
                break;

            case KM_BOOL:
                return KeyParameter{tag, param.boolean};
                break;

            case KM_BIGNUM:
            case KM_BYTES:
                return {tag, KeyParameterValue::make<KeyParameterValue::blob>(
                        std::vector(param.blob.data, param.blob.data + param.blob.data_length))};
                break;

            case KM_INVALID:
            default:
                CHECK(false) << "Unknown or unused tag type: Something is broken";
                return KeyParameter{Tag::INVALID, false};
                break;
        }
    }

    inline vector<KeyParameter> kmParamSet2Aidl(const keymaster_key_param_set_t& set) {
        vector<KeyParameter> result;
        if (set.length == 0 || set.params == nullptr) return result;

        result.resize(set.length);

        keymaster_key_param_t* params = set.params;
        for (size_t i = 0; i < set.length; ++i) {
            result[i] = kmParam2Aidl(params[i]);
        }

        for (auto it = result.begin(); it != result.end();) {
            if (it->tag == Tag::INVALID) {
                it = result.erase(it);
            } else ++it;
        }

        for (auto a: result) {
            if (a.tag == Tag::INVALID) BLOGE("INVALID");
        }

        return result;
    }

    class KmParamSet : public keymaster_key_param_set_t {
        public:
            explicit KmParamSet(const vector<KeyParameter>& keyParams)
                : keymaster_key_param_set_t(aidlKeyParams2Km(keyParams)) {}

            KmParamSet(KmParamSet&& other) : keymaster_key_param_set_t{other.params, other.length} {
                other.length = 0;
                other.params = nullptr;
            }

            KmParamSet(const KmParamSet&) = delete;
            ~KmParamSet() { keymaster_free_param_set(this); }
    };

    inline vector<uint8_t> kmBlob2vector(const keymaster_key_blob_t& blob) {
        vector<uint8_t> result(blob.key_material, blob.key_material + blob.key_material_size);
        return result;
    }

    inline vector<uint8_t> kmBlob2vector(const keymaster_blob_t& blob) {
        vector<uint8_t> result(blob.data, blob.data + blob.data_length);
        return result;
    }

    inline vector<Certificate> kmCertChain2Aidl(const keymaster_cert_chain_t& cert_chain) {
        vector<Certificate> result;
        if (!cert_chain.entry_count || !cert_chain.entries) return result;

        result.resize(cert_chain.entry_count);
        for (size_t i = 0; i < cert_chain.entry_count; ++i) {
            result[i].encodedCertificate = kmBlob2vector(cert_chain.entries[i]);
            BLOGD("size of [%zu].encodedCertificate : %lu", i, sizeof(result[i].encodedCertificate));
        }

        return result;
    }

    class kmHardwareAuthToken : public keymaster_hw_auth_token_t {
        public:
            kmHardwareAuthToken (const HardwareAuthToken& authToken) {
                challenge = authToken.challenge;
                user_id = authToken.userId;
                authenticator_id = authToken.authenticatorId;
                authenticator_type = keymaster_hw_auth_type_t(
                        ntohl(static_cast<uint32_t>(authToken.authenticatorType)));
                timestamp = ntohq(authToken.timestamp.milliSeconds);
                mac = { authToken.mac.data(), authToken.mac.size() };
            }

            kmHardwareAuthToken() {
                challenge = 0;
                user_id = 0;
                authenticator_id = 0;
                authenticator_type = (keymaster_hw_auth_type_t)0;
                timestamp = 0;
                mac = {};
            }
    };

    class kmVerifyToken : public keymaster_verification_token_t {
        public:
            kmVerifyToken() {
                challenge = 0;
                timestamp = 0;
                parameters_verified = {};
                security_level = KM_SECURITY_LEVEL_STRONGBOX;
                mac = {};
            }
    };

    class kmTimestampToken : public keymaster_timestamp_token_t {
        public:
            kmTimestampToken (const optional<TimeStampToken>& timestampToken) {
                challenge = timestampToken->challenge;
                timestamp = timestampToken->timestamp.milliSeconds;
                mac = { timestampToken->mac.data(), timestampToken->mac.size() };
            }
    };

    inline kmHardwareAuthToken authToken2kmAuthToken(
            const optional<HardwareAuthToken>& authToken) {
        kmHardwareAuthToken auth;

        auth.challenge = authToken->challenge;
        auth.user_id = authToken->userId;
        auth.authenticator_id = authToken->authenticatorId;
        auth.authenticator_type = keymaster_hw_auth_type_t(
                ntohl(static_cast<uint32_t>(authToken->authenticatorType)));
        auth.timestamp = ntohq(authToken->timestamp.milliSeconds);
        auth.mac = { authToken->mac.data(), authToken->mac.size() };

        return auth;
    }

    inline kmVerifyToken timeStamp2kmVerifyToken(const optional<TimeStampToken>& timeStamp) {
        kmVerifyToken verify;

        verify.challenge = timeStamp->challenge;
        //verify.timestamp = ntohq(timeStamp->timestamp.milliSeconds);
        verify.timestamp = timeStamp->timestamp.milliSeconds;
        verify.mac = { timeStamp->mac.data(), timeStamp->mac.size() };

        return verify;
    }

    inline vector<vector<uint8_t>> kmCertChain2Vector(const keymaster_cert_chain_t& cert_chain) {
        vector<vector<uint8_t>> result;
        if (!cert_chain.entry_count || !cert_chain.entries)
            return result;

        result.resize(cert_chain.entry_count);
        for (size_t i = 0; i < cert_chain.entry_count; ++i) {
            result[i] = kmBlob2vector(cert_chain.entries[i]);
        }

        return result;
    }

    inline vector<Certificate> Vec2CertChain(const vector<vector<uint8_t>> chain) {
        vector<Certificate> retval;
        retval.resize(chain.size());

        for (size_t i = 0; i < chain.size(); i++) {
            retval[i].encodedCertificate = chain[i];
        }

        return retval;
    }

    inline bool checkParamTag(const vector<KeyParameter>& params, Tag check_tag) {
        for (auto p : params) {
            if (p.tag == check_tag) return true;
        }

        return false;
    }

    inline bool checkParamTag(const vector<KeyParameter>& params, Tag check_tag, KeyPurpose pur) {
        for (auto p : params) {
            if (p.tag == check_tag) {
                if (p.value == pur) return true;
            }
        }

        return false;
    }

    inline keymaster_error_t get_tag_value_date_time(
            const keymaster_key_param_set_t *params,
            keymaster_tag_t tag,
            long *value)
    {
        int pos;

        if ( (params == NULL) || (value == NULL) ) {
            return KM_ERROR_UNEXPECTED_NULL_POINTER;
        }

        pos = find_tag_pos(params, tag);
        if (pos < 0) {
            return KM_ERROR_INVALID_TAG;
        } else {
            *value = params->params[pos].date_time;
            return KM_ERROR_OK;
        }
    }

    class kmHmacSharingParamsSet : public keymaster_hmac_sharing_params_set_t {
        public:
            kmHmacSharingParamsSet(const vector<SharedSecretParameters>& hmac_sharing_params) {
                params = new keymaster_hmac_sharing_params_t[hmac_sharing_params.size()];
                length = hmac_sharing_params.size();
                for (size_t i = 0; i < hmac_sharing_params.size(); ++i) {
                    params[i].seed = { hmac_sharing_params[i].seed.data(),
                        hmac_sharing_params[i].seed.size() };
                    params[i].nonce = { hmac_sharing_params[i].nonce.data(),
                        hmac_sharing_params[i].nonce.size() };

                }
            }

            kmHmacSharingParamsSet(const kmHmacSharingParamsSet&) = delete;
            kmHmacSharingParamsSet() { delete[] params; }
    };

}  // namespace aidl::android::hardware::security::keymint::km_utils

#endif // _KEYMINT_UTILS_H_
