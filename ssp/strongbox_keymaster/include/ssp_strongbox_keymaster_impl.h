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

#ifndef _SSP_STRONGBOX_KEYMASTER_IMPL_
#define _SSP_STRONGBOX_KEYMASTER_IMPL_

#include "tee_client_api.h"
#include "ssp_strongbox_keymaster_defs.h"
#include "ssp_strongbox_keymaster_common_util.h"

/*
 * SSP support 4 operation session at the same time
 */
#define SSP_OPERATION_SESSION_MAX 4

typedef struct ssp_operation {
    keymaster_operation_handle_t handle;
    keymaster_algorithm_t algorithm;
    size_t final_len;
    bool used;
} ssp_operation_t;

typedef struct ssp_session {
    TEEC_Context teec_context;
    TEEC_Session teec_session;
    TEEC_Operation teec_operation;
    ssp_operation_t ssp_operation[SSP_OPERATION_SESSION_MAX];
    uint32_t ssp_op_count;
} ssp_session_t;

int ssp_open(ssp_session_t &ssp_sess);
int ssp_close(ssp_session_t &ssp_sess);

/* for privsion test */
keymaster_error_t ssp_set_sample_attestation_data(
    ssp_session_t &sess,
    uint8_t *blob_p, uint32_t *blob_len);

keymaster_error_t ssp_init_ipc(
    ssp_session_t &sess);

keymaster_error_t ssp_set_attestation_data(
    ssp_session_t &sess,
    uint8_t *data_p, uint32_t data_len,
    uint8_t *blob_p, uint32_t *blob_len);

keymaster_error_t ssp_load_attestation_data(
    ssp_session_t &sess);

keymaster_error_t ssp_send_system_version(
    ssp_session_t &sess,
    const keymaster_key_param_set_t *params);

keymaster_error_t ssp_get_hmac_sharing_parameters(
    ssp_session_t &ssp_sess,
    keymaster_hmac_sharing_params_t *params);

keymaster_error_t ssp_compute_shared_hmac(
    ssp_session_t &ssp_sess,
    const keymaster_hmac_sharing_params_set_t *params,
    keymaster_blob_t *sharing_check);

keymaster_error_t ssp_add_rng_entropy(
    ssp_session_t &sess,
    const uint8_t* data,
    size_t length);

keymaster_error_t ssp_generate_key(
    ssp_session_t &sess,
    const keymaster_key_param_set_t* params,
    keymaster_key_blob_t* key_blob,
    keymaster_key_characteristics_t* characteristics);

keymaster_error_t ssp_generate_key(
    ssp_session_t &sess,
    const keymaster_key_param_set_t* params,
    keymaster_key_blob_t* key_blob,
    keymaster_key_characteristics_t* characteristics);

keymaster_error_t ssp_get_key_characteristics(
    ssp_session_t &sess,
    const keymaster_key_blob_t* key_blob,
    const keymaster_blob_t* client_id,
    const keymaster_blob_t* app_data,
    keymaster_key_characteristics_t* characteristics);

keymaster_error_t ssp_import_key(
    ssp_session_t &sess,
    const keymaster_key_param_set_t* params,
    keymaster_key_format_t key_format,
    const keymaster_blob_t* key_data,
    keymaster_key_blob_t* key_blob,
    keymaster_key_characteristics_t* characteristics);

keymaster_error_t ssp_import_wrappedkey(
    ssp_session_t &sess,
    const keymaster_blob_t* wrapped_key_data,
    const keymaster_key_blob_t* wrapping_key_blob,
    const keymaster_blob_t* masking_key,
    const keymaster_key_param_set_t* unwrapping_params,
    uint64_t pwd_sid,
    uint64_t bio_sid,
    keymaster_key_blob_t* key_blob,
    keymaster_key_characteristics_t* characteristics);

keymaster_error_t ssp_export_key(
    ssp_session_t &sess,
    keymaster_key_format_t export_format,
    const keymaster_key_blob_t* keyblob_to_export,
    const keymaster_blob_t* client_id,
    const keymaster_blob_t* app_data,
    keymaster_blob_t* exported_keyblob);

keymaster_error_t ssp_begin(
    ssp_session_t &sess,
    keymaster_purpose_t purpose,
    const keymaster_key_blob_t* keyblob,
    const keymaster_key_param_set_t* in_params,
    const keymaster_hw_auth_token_t *auth_token,
    keymaster_key_param_set_t* out_params,
    keymaster_operation_handle_t* op_handle);

keymaster_error_t ssp_update(
    ssp_session_t &sess,
    keymaster_operation_handle_t op_handle,
    const keymaster_key_param_set_t* in_params,
    const keymaster_blob_t* input_data,
    const keymaster_hw_auth_token_t* auth_token,
    const keymaster_verification_token_t *verification_token,
    uint32_t* result_consumed,
    keymaster_key_param_set_t* out_params,
    keymaster_blob_t* output_data);

keymaster_error_t ssp_finish(
    ssp_session_t &sess,
    keymaster_operation_handle_t op_handle,
    const keymaster_key_param_set_t* in_params,
    const keymaster_blob_t* input_data,
    const keymaster_blob_t* signature,
    const keymaster_hw_auth_token_t *auth_token,
    const keymaster_verification_token_t *verification_token,
    keymaster_key_param_set_t* out_params,
    keymaster_blob_t* output_data);

keymaster_error_t ssp_abort(
    ssp_session_t &sess,
    keymaster_operation_handle_t op_handle);

keymaster_error_t ssp_attest_key(
    ssp_session_t &sess,
    const keymaster_key_blob_t* attest_keyblob,
    const keymaster_key_param_set_t* attest_params,
    keymaster_cert_chain_t* cert_chain);

keymaster_error_t ssp_upgrade_key(
    ssp_session_t &sess,
    const keymaster_key_blob_t* keyblob_to_upgrade,
    const keymaster_key_param_set_t* upgrade_params,
    keymaster_key_blob_t* upgraded_key);

keymaster_error_t ssp_send_dummy_system_version(
    ssp_session_t &sess,
    const system_version_info_t *sys_version_info);


#endif

