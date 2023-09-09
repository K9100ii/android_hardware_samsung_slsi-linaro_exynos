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

#include <TrustonicKeymintDeviceImpl.h>
#include <tlcKeymint_if.h>
#include "km_shared_util.h"
#include "cust_tee_keymint_impl.h"
#include <time.h>

#undef  LOG_TAG
#define LOG_TAG "TrustonicKeymintDeviceImpl"

/**
 * Constructor
 */
TrustonicKeymintDeviceImpl::TrustonicKeymintDeviceImpl()
    : session_handle_(NULL), errcode(0)
{
    int rc;

    rc = TEE_Open(&session_handle_);
    if (rc) {
        LOG_E("Failed to open session to Keymint TA.");
        session_handle_ = NULL;
        errcode = rc;
    } else {
        /* Configure the TA. */
        rc = HAL_Configure(session_handle_);
        if (rc) {
            LOG_E("HAL_Configure() failed.");
            TEE_Close(session_handle_);
            session_handle_ = NULL;
            errcode = rc;
        }
    }
}

/**
 * Destructor
 */
TrustonicKeymintDeviceImpl::~TrustonicKeymintDeviceImpl()
{
    TEE_Close(session_handle_);
}


#define CHECK_SESSION(handle) \
    if (handle == NULL) { \
        LOG_E("%s: Invalid session handle", __func__); \
        return KM_ERROR_SECURE_HW_COMMUNICATION_FAILED; \
    }

static const char *km_name = "Kinibi Keymint";
static const char *km_author_name = "Trustonic";

void TrustonicKeymintDeviceImpl::get_hardware_info(
    keymaster_security_level_t *security_level,
    const char **keymint_name,
    const char **keymint_author_name)
{
    *security_level = KM_SECURITY_LEVEL_TRUSTED_ENVIRONMENT;
    *keymint_name = km_name;
    *keymint_author_name = km_author_name;
}

keymaster_error_t TrustonicKeymintDeviceImpl::get_hmac_sharing_parameters(
    keymaster_hmac_sharing_parameters_t *params)
{
    CHECK_SESSION(session_handle_);
    return TEE_GetHmacSharingParameters(session_handle_, params);
}

keymaster_error_t TrustonicKeymintDeviceImpl::compute_shared_hmac(
    const keymaster_hmac_sharing_parameters_set_t *params,
    keymaster_blob_t *sharing_check)
{
    CHECK_SESSION(session_handle_);
    return TEE_ComputeSharedMac(session_handle_, params, sharing_check);
}

keymaster_error_t TrustonicKeymintDeviceImpl::generate_timestamp(
    keymaster_timestamp_token_t *timestamp_token)
{
    CHECK_SESSION(session_handle_);
    return TEE_GenerateTimestamp(session_handle_,
                                   timestamp_token);
}

keymaster_error_t TrustonicKeymintDeviceImpl::add_rng_entropy(
    const uint8_t* data,
    size_t data_length)
{
    CHECK_SESSION(session_handle_);
    return TEE_AddRngEntropy(session_handle_, data, data_length);
}

keymaster_error_t TrustonicKeymintDeviceImpl::generate_key(
    const keymaster_key_param_set_t* params,
    keymaster_key_blob_t* key_blob,
    keymaster_key_characteristics_t* characteristics)
{
    CHECK_SESSION(session_handle_);
    return TEE_GenerateAndAttestKey(session_handle_,
        params, NULL, NULL, NULL, key_blob, characteristics, NULL);
}

keymaster_error_t TrustonicKeymintDeviceImpl::get_key_characteristics(
    const keymaster_key_blob_t* key_blob,
    const keymaster_blob_t* client_id,
    const keymaster_blob_t* app_data,
    keymaster_key_characteristics_t* characteristics)
{
    CHECK_SESSION(session_handle_);
    return TEE_GetKeyCharacteristics(session_handle_,
        key_blob, client_id, app_data, characteristics);
}

keymaster_error_t TrustonicKeymintDeviceImpl::import_key(
    const keymaster_key_param_set_t* params,
    keymaster_key_format_t key_format,
    const keymaster_blob_t* key_data,
    keymaster_key_blob_t* key_blob,
    keymaster_key_characteristics_t* characteristics)
{
    CHECK_SESSION(session_handle_);
    return TEE_ImportAndAttestKey(session_handle_,
        params, key_format, key_data, NULL, NULL, NULL, key_blob, characteristics, NULL);
}

keymaster_error_t TrustonicKeymintDeviceImpl::export_key(
    keymaster_key_format_t export_format,
    const keymaster_key_blob_t* key_to_export,
    const keymaster_blob_t* client_id,
    const keymaster_blob_t* app_data,
    keymaster_blob_t* export_data)
{
    CHECK_SESSION(session_handle_);
    return TEE_ExportKey(session_handle_,
        export_format, key_to_export, client_id, app_data, export_data);
}

keymaster_error_t TrustonicKeymintDeviceImpl::upgrade_key(
    const keymaster_key_blob_t* key_to_upgrade,
    const keymaster_key_param_set_t* upgrade_params,
    keymaster_key_blob_t* upgraded_key)
{
    CHECK_SESSION(session_handle_);
    return TEE_UpgradeKey(session_handle_, key_to_upgrade, upgrade_params, upgraded_key);
}

keymaster_error_t TrustonicKeymintDeviceImpl::delete_key(
    const keymaster_key_blob_t* key_to_delete)
{
    CHECK_SESSION(session_handle_);
    return TEE_DeleteKey(session_handle_, key_to_delete);
}

keymaster_error_t TrustonicKeymintDeviceImpl::delete_all_keys(void)
{
    CHECK_SESSION(session_handle_);
    return TEE_DeleteAllKeys(session_handle_);
}

keymaster_error_t TrustonicKeymintDeviceImpl::begin(
    keymaster_purpose_t purpose,
    const keymaster_key_blob_t* key,
    const keymaster_key_param_set_t* params,
    const keymaster_hw_auth_token_t *auth_token,
    keymaster_key_param_set_t* out_params,
    keymaster_operation_handle_t* operation_handle)
{
    CHECK_SESSION(session_handle_);
    return TEE_Begin(session_handle_,
        purpose, key, params, auth_token, out_params, operation_handle);
}

keymaster_error_t TrustonicKeymintDeviceImpl::update(
    keymaster_operation_handle_t operation_handle,
    const keymaster_blob_t* input,
    const keymaster_hw_auth_token_t *auth_token,
    const keymaster_timestamp_token_t *timestamp_token,
    size_t* input_consumed,
    keymaster_blob_t* output)
{
    CHECK_SESSION(session_handle_);
    (void)timestamp_token;
    return TEE_Update(session_handle_,
        operation_handle, input, auth_token, input_consumed, output);
}

keymaster_error_t TrustonicKeymintDeviceImpl::finish(
    keymaster_operation_handle_t operation_handle,
    const keymaster_blob_t* input,
    const keymaster_blob_t* signature,
    const keymaster_hw_auth_token_t *auth_token,
    const keymaster_timestamp_token_t *timestamp_token,
    keymaster_blob_t* output)
{
    CHECK_SESSION(session_handle_);
    (void)timestamp_token;
    return TEE_Finish(session_handle_,
        operation_handle, input, signature, auth_token, output);
}

keymaster_error_t TrustonicKeymintDeviceImpl::abort(
    keymaster_operation_handle_t operation_handle)
{
    CHECK_SESSION(session_handle_);
    return TEE_Abort(session_handle_, operation_handle);
}

keymaster_error_t TrustonicKeymintDeviceImpl::import_wrapped_key(
    const keymaster_blob_t* wrapped_key_data,
    const keymaster_key_blob_t* wrapping_key_blob,
    const uint8_t* masking_key,
    const keymaster_key_param_set_t* unwrapping_params,
    uint64_t password_sid,
    uint64_t biometric_sid,
    keymaster_key_blob_t* key_blob,
    keymaster_key_characteristics_t* key_characteristics,
    keymaster_cert_chain_t* cert_chain)
{
    CHECK_SESSION(session_handle_);
    return TEE_ImportWrappedKey(session_handle_, wrapped_key_data,
        wrapping_key_blob, masking_key, unwrapping_params, password_sid,
        biometric_sid, key_blob, key_characteristics, cert_chain);
}

keymaster_error_t TrustonicKeymintDeviceImpl::destroy_attestation_ids(void)
{
    CHECK_SESSION(session_handle_);
    return HAL_DestroyAttestationIds(session_handle_);
}

keymaster_error_t TrustonicKeymintDeviceImpl::early_boot_ended(void)
{
    CHECK_SESSION(session_handle_);
    return TEE_EarlyBootEnded(session_handle_);
}

keymaster_error_t TrustonicKeymintDeviceImpl::device_locked(
    bool password_only)
{
    CHECK_SESSION(session_handle_);
    return TEE_DeviceLocked(session_handle_, password_only);
}

keymaster_error_t TrustonicKeymintDeviceImpl::unwrap_aes_storage_key(
    const keymaster_blob_t* wrapped_key_data)
{
    CHECK_SESSION(session_handle_);
    return TEE_UnwrapAesStorageKey(session_handle_, wrapped_key_data);
}

keymaster_error_t TrustonicKeymintDeviceImpl::update_aad(
    keymaster_operation_handle_t operation_handle,
    const keymaster_blob_t* aad,
    const keymaster_hw_auth_token_t *auth_token,
    const keymaster_timestamp_token_t *timestamp_token)
{
    CHECK_SESSION(session_handle_);
    (void)timestamp_token;
    return TEE_UpdateAad(session_handle_,
        operation_handle, aad, auth_token);
}

keymaster_error_t TrustonicKeymintDeviceImpl::generate_and_attest_key(
        const keymaster_key_param_set_t* params,
        const keymaster_key_blob_t* attest_key_blob,
        const keymaster_key_param_set_t* attest_params,
        const keymaster_blob_t* attest_issuer_blob,
        keymaster_key_blob_t* key_blob,
        keymaster_key_characteristics_t* characteristics,
        keymaster_cert_chain_t* cert_chain)
{
    CHECK_SESSION(session_handle_);
    return TEE_GenerateAndAttestKey(session_handle_,
        params,
        attest_key_blob, attest_params, attest_issuer_blob,
        key_blob, characteristics, cert_chain);
}

keymaster_error_t TrustonicKeymintDeviceImpl::import_and_attest_key(
    const keymaster_key_param_set_t* params,
    keymaster_key_format_t key_format,
    const keymaster_blob_t* key_data,
    const keymaster_key_blob_t* attest_key_blob,
    const keymaster_key_param_set_t* attest_params,
    const keymaster_blob_t* attest_issuer_blob,
    keymaster_key_blob_t* key_blob,
    keymaster_key_characteristics_t* characteristics,
    keymaster_cert_chain_t* cert_chain)
{
    CHECK_SESSION(session_handle_);
    return TEE_ImportAndAttestKey(session_handle_,
        params, key_format, key_data, attest_key_blob,
        attest_params, attest_issuer_blob,
        key_blob, characteristics, cert_chain);
}

keymaster_error_t TrustonicKeymintDeviceImpl::generate_ecdsa_p256_key(
    bool test_mode,
    keymaster_blob_t *maced_public_key_blob,
    keymaster_key_blob_t *private_key_handle_blob)
{
    CHECK_SESSION(session_handle_);
    return TEE_GenerateEcdsaP256Key(session_handle_,
        test_mode,
        maced_public_key_blob,
        private_key_handle_blob);
}

keymaster_error_t TrustonicKeymintDeviceImpl::generate_certificate_request(
    bool test_mode,
    const keymaster_blob_t keys_to_sign[],
    size_t nb_keys_to_sign,
    const keymaster_blob_t *endpoint_enc_cert_chain,
    const keymaster_blob_t *challenge_blob,
    keymaster_blob_t *device_info,
    keymaster_blob_t *protected_data,
    keymaster_blob_t *keys_to_sign_mac)
{
    CHECK_SESSION(session_handle_);
    return TEE_GenerateCertificateRequest(session_handle_,
        test_mode,
        keys_to_sign,
        nb_keys_to_sign,
        endpoint_enc_cert_chain,
        challenge_blob,
        device_info,
        protected_data,
        keys_to_sign_mac);
}

keymaster_error_t TrustonicKeymintDeviceImpl::convert_storage_key(
        const keymaster_key_blob_t *storage_key_blob,
        keymaster_blob_t *ephemeral_key_blob)
{
    CHECK_SESSION(session_handle_);
    return TEE_ExportKey(session_handle_,
        KM_KEY_FORMAT_RAW, storage_key_blob, NULL, NULL, ephemeral_key_blob);
}

keymaster_error_t TrustonicKeymintDeviceImpl::get_root_of_trust(
        const uint8_t challenge[16],
        keymaster_blob_t *rot_blob)
{
    CHECK_SESSION(session_handle_);
    return TEE_GetRootOfTrust(session_handle_, challenge, rot_blob);
}
