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

#ifndef STRONGBOX_KEYMASTER4_DEVICE_IMPL_H_
#define STRONGBOX_KEYMASTER4_DEVICE_IMPL_H_

#include "ssp_strongbox_keymaster_defs.h"
#include "ssp_strongbox_keymaster_log.h"
#include "ssp_strongbox_keymaster_impl.h"

class StrongboxKeymaster4DeviceImpl {
  public:
    StrongboxKeymaster4DeviceImpl();

    ~StrongboxKeymaster4DeviceImpl();

    keymaster_error_t get_hmac_sharing_parameters(
        keymaster_hmac_sharing_params_t *params);

    keymaster_error_t compute_shared_hmac(
        const keymaster_hmac_sharing_params_set_t *params,
        keymaster_blob_t *sharing_check);

    keymaster_error_t add_rng_entropy(
        const uint8_t* data,
        size_t length);

    keymaster_error_t generate_key(
        const keymaster_key_param_set_t* params,
        keymaster_key_blob_t* key_blob,
        keymaster_key_characteristics_t* characteristics);

    keymaster_error_t get_key_characteristics(
        const keymaster_key_blob_t* key_blob,
        const keymaster_blob_t* client_id,
        const keymaster_blob_t* app_data,
        keymaster_key_characteristics_t* characteristics);

    keymaster_error_t import_key(
        const keymaster_key_param_set_t* params,
        keymaster_key_format_t key_format,
        const keymaster_blob_t* key_data,
        keymaster_key_blob_t* key_blob,
        keymaster_key_characteristics_t* characteristics);

    keymaster_error_t import_wrappedkey(
        const keymaster_blob_t* wrapped_key_data,
        const keymaster_key_blob_t* wrapping_key_blob,
        const keymaster_blob_t* masking_key,
        const keymaster_key_param_set_t* unwrapping_params,
        uint64_t pwd_sid,
        uint64_t bio_sid,
        keymaster_key_blob_t* key_blob,
        keymaster_key_characteristics_t* characteristics);

    keymaster_error_t export_key(
        keymaster_key_format_t export_format,
        const keymaster_key_blob_t* keyblob_to_export,
        const keymaster_blob_t* client_id,
        const keymaster_blob_t* app_data,
        keymaster_blob_t* exported_keyblob);

    keymaster_error_t begin(
        keymaster_purpose_t purpose,
        const keymaster_key_blob_t* keyblob,
        const keymaster_key_param_set_t* in_params,
        const keymaster_hw_auth_token_t *auth_token,
        keymaster_key_param_set_t* out_params,
        keymaster_operation_handle_t* op_handle);

    keymaster_error_t update(
        keymaster_operation_handle_t op_handle,
        const keymaster_key_param_set_t* params,
        const keymaster_blob_t* input,
        const keymaster_hw_auth_token_t *auth_token,
        const keymaster_verification_token_t *verification_token,
        uint32_t* result_consumed,
        keymaster_key_param_set_t* out_params,
        keymaster_blob_t* output);

    keymaster_error_t finish(
        keymaster_operation_handle_t op_handle,
        const keymaster_key_param_set_t* params,
        const keymaster_blob_t* input,
        const keymaster_blob_t* signature,
        const keymaster_hw_auth_token_t *auth_token,
        const keymaster_verification_token_t *verification_token,
        keymaster_key_param_set_t* out_params,
        keymaster_blob_t* output);

    keymaster_error_t abort(
        keymaster_operation_handle_t op_handle);

    keymaster_error_t attest_key(
        const keymaster_key_blob_t* attest_keyblob,
        const keymaster_key_param_set_t* attest_params,
        keymaster_cert_chain_t* cert_chain);

    keymaster_error_t upgrade_key(
        const keymaster_key_blob_t* keyblob_to_upgrade,
        const keymaster_key_param_set_t* upgrade_params,
        keymaster_key_blob_t* upgraded_key);

    struct ssp_session sess;

    int errcode;

private:
    uint32_t _os_version;
    uint32_t _os_patchlevel;
    uint32_t _vendor_patchlevel;

};


#endif  //  STRONGBOX_KEYMASTER4_DEVICE_IMPL_H_