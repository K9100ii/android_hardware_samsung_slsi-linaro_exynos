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

#ifndef __SSP_STRONGBOX_KEYMASTER_PARSE_KEY_H__
#define __SSP_STRONGBOX_KEYMASTER_PARSE_KEY_H__

#include <memory>

#include "ssp_strongbox_keymaster_common_util.h"

keymaster_error_t ssp_keyparser_parse_keydata_pkcs8ec(
    std::unique_ptr<uint8_t[]>& key_data,
    size_t *key_data_len,
    const uint8_t *data,
    size_t data_len);

keymaster_error_t ssp_keyparser_parse_keydata_pkcs8rsa(
    std::unique_ptr<uint8_t[]>& key_data,
    size_t *key_data_len,
    const uint8_t *data,
    size_t data_len);

keymaster_error_t ssp_keyparser_parse_keydata_raw(
    keymaster_algorithm_t algorithm,
    std::unique_ptr<uint8_t[]>& key_data,
    size_t *key_data_len,
    const uint8_t *data,
    size_t data_len);

keymaster_error_t ssp_keyparser_get_rsa_exponent_from_parsedkey(
    uint8_t *keydata,
    size_t keydata_len,
    uint64_t *rsa_e);

keymaster_error_t ssp_keyparser_get_keysize_from_parsedkey(
    uint8_t *keydata,
    uint32_t *keysize);

keymaster_error_t ssp_keyparser_parse_wrappedkey(
    keymaster_blob_t *encrypted_transport_key,
    keymaster_blob_t *iv,
    keymaster_blob_t *key_description,
    keymaster_key_format_t *key_format,
    keymaster_blob_t *auth_list,
    keymaster_blob_t *encrypted_key,
    keymaster_blob_t *tag,
    const keymaster_blob_t *wrappedkey);

keymaster_error_t ssp_keyparser_make_authorization_list(
    std::unique_ptr<uint8_t[]>& key_params_ptr,
    size_t *key_params_len,
    keymaster_algorithm_t *key_type,
    uint32_t *hw_auth_type,
    const uint8_t *data, size_t len);

#endif /* __SSP_STRONGBOX_KEYMASTER_PARSE_KEY_H__ */

