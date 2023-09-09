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

#ifndef _STRONGBOX_KEYMINT_IMPL_H_
#define _STRONGBOX_KEYMINT_IMPL_H_

#include "ssp_strongbox_keymaster_defs.h"
#include "ssp_strongbox_keymaster_log.h"
#include "ssp_strongbox_keymaster_impl.h"
#include "ssp_strongbox_keymaster_configuration.h"
#include "ssp_strongbox_keymaster_hwctl.h"

class StrongboxKeymintImpl {
  public:
      StrongboxKeymintImpl();

      ~StrongboxKeymintImpl();

      /* Shared secret */
      keymaster_error_t get_hmac_sharing_parameters(
              /* output */
              keymaster_hmac_sharing_params_t *params);

      keymaster_error_t compute_shared_hmac(
              /* input */
              const keymaster_hmac_sharing_params_set_t *params,
              /* output */
              keymaster_blob_t *sharing_check);

      /* Keymint device */
      keymaster_error_t add_rng_entropy(
              const uint8_t* data,
              size_t length);

      keymaster_error_t generate_attest_key(
              /* input */
              const keymaster_key_param_set_t* params,
              /* input for attestation : can be null */
              keymaster_blob_t* issuer_subject,
              keymaster_key_blob_t* attest_keyblob,
              const keymaster_key_param_set_t* attest_params,
              /* output */
              keymaster_key_blob_t* key_blob,
              keymaster_key_characteristics_t* characteristics,
              keymaster_cert_chain_t* cert_chain);

      keymaster_error_t import_attest_key(
              /* input */
              const keymaster_key_param_set_t* params,
              keymaster_key_format_t key_format,
              const keymaster_blob_t* key_data,
              /* input for attestation : can be null */
              keymaster_blob_t* issuer_subject,
              keymaster_key_blob_t* attest_keyblob,
              const keymaster_key_param_set_t* attest_params,
              /* output */
              keymaster_key_blob_t* key_blob,
              keymaster_key_characteristics_t* characteristics,
              keymaster_cert_chain_t* cert_chain);

      keymaster_error_t import_attest_wrappedkey(
              /* input */
              const keymaster_blob_t* wrapped_key_data,
              const keymaster_key_blob_t* wrapping_key_blob,
              const keymaster_blob_t* masking_key,
              const keymaster_key_param_set_t* unwrapping_params,
              uint64_t pwd_sid,
              uint64_t bio_sid,
              /* output */
              keymaster_key_blob_t* key_blob,
              keymaster_key_characteristics_t* characteristics,
              keymaster_cert_chain_t* cert_chain);

      keymaster_error_t upgrade_key(
              /* input */
              const keymaster_key_blob_t* keyblob_to_upgrade,
              const keymaster_key_param_set_t* upgrade_params,
              /* output */
              keymaster_key_blob_t* upgraded_key);

      keymaster_error_t begin(
              /* input */
              keymaster_purpose_t purpose,
              const keymaster_key_blob_t* keyblob,
              const keymaster_key_param_set_t* in_params,
              const keymaster_hw_auth_token_t *auth_token,
              /* output */
              keymaster_key_param_set_t* out_params,
              keymaster_operation_handle_t* op_handle);

      /* Keymint operation */
      keymaster_error_t update_aad(
              /* input */
              keymaster_operation_handle_t op_handle,
              const keymaster_key_param_set_t* params,
              const keymaster_hw_auth_token_t *auth_token,
              const keymaster_timestamp_token_t *timestamp_token);

      keymaster_error_t update(
              /* input */
              keymaster_operation_handle_t op_handle,
              const keymaster_blob_t* input,
              const keymaster_hw_auth_token_t *auth_token,
              const keymaster_timestamp_token_t *timestamp_token,
              /* output */
              uint32_t* result_consumed,
              keymaster_key_param_set_t* out_params,
              keymaster_blob_t* output);

      keymaster_error_t finish(
              /* input */
              keymaster_operation_handle_t op_handle,
              const keymaster_blob_t* input,
              const keymaster_blob_t* signature,
              const keymaster_hw_auth_token_t *auth_token,
              const keymaster_timestamp_token_t *timestamp_token,
              const keymaster_blob_t* confirmation_token,
              /* output */
              keymaster_key_param_set_t* out_params,
              keymaster_blob_t* output);

      keymaster_error_t abort(
              keymaster_operation_handle_t op_handle);

      /* keymatser 4.1 function */
      keymaster_error_t device_locked(
              /* input */
              const bool password_only,
              /* output */
              const keymaster_timestamp_token_t *timestamp_token);

      keymaster_error_t early_boot_ended();

      keymaster_error_t get_key_characteristics(
              /* input */
              const keymaster_key_blob_t* key_blob,
              const keymaster_blob_t* app_id,
              const keymaster_blob_t* app_data,
              /* output */
              keymaster_key_characteristics_t* characteristics);

      struct ssp_session sess;
      int errcode;
  private:
      uint32_t _os_version;
      uint32_t _os_patchlevel;
      uint32_t _vendor_patchlevel;

};
#endif //  _STRONGBOX_KEYMINT_IMPL_H_
