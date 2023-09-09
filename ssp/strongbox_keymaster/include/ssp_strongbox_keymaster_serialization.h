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

#ifndef __SSP_STRONGBOX_KEYMASTER_SERIALIZATION_H__
#define __SSP_STRONGBOX_KEYMASTER_SERIALIZATION_H__

#include <memory>

#include "ssp_strongbox_keymaster_defs.h"
#include "ssp_strongbox_keymaster_common_util.h"

keymaster_error_t ssp_serializer_serialize_params(
    std::unique_ptr<uint8_t[]>& params_blob,
    uint32_t *params_blob_size,
    const keymaster_key_param_set_t *params,
    bool extra_param_time,
    uint32_t extra_param_keysize,
    uint64_t extra_param_rsapubexp);

keymaster_error_t ssp_serializer_serialize_verification_token(
    std::unique_ptr<uint8_t[]>& params_blob,
    uint32_t *params_blob_size,
    const keymaster_key_param_set_t *params);

keymaster_error_t ssp_serializer_deserialize_params(
    uint8_t **pos,
    uint32_t *remain,
    keymaster_key_param_set_t *params);

keymaster_error_t ssp_serializer_deserialize_characteristics(
    const uint8_t *buffer,
    uint32_t buffer_length,
    keymaster_key_characteristics_t *characteristics);

keymaster_error_t ssp_serializer_deserialize_attestation(
    const uint8_t *buffer,
    uint32_t buffer_length,
    keymaster_cert_chain_t *cert_chain);

#endif /* __SSP_STRONGBOX_KEYMASTER_SERIALIZATION_H__ */

