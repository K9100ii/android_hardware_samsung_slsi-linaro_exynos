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

#define LOG_TAG "android.hardware.security.keymint-impl"
#include <log/log.h>

#include <aidl/android/hardware/security/keymint/ErrorCode.h>

#include "StrongboxKeymintOperation.h"
#include "KeyMintUtils.h"

namespace aidl::android::hardware::security::keymint {

using secureclock::Timestamp;
using namespace km_utils;

StrongboxKeyMintOperation::StrongboxKeyMintOperation(
    StrongboxKeymintImpl *implementation,
    keymaster_operation_handle_t opHandle)
    : impl_(std::move(implementation)), opHandle_(opHandle) {}

StrongboxKeyMintOperation::~StrongboxKeyMintOperation() {}

ScopedAStatus StrongboxKeyMintOperation::updateAad(const vector<uint8_t>& input,
        const optional<HardwareAuthToken>& authToken,
        const optional<TimeStampToken>& timestampToken)
{
    BLOGD("%s: %s: %d", __FILE__, __func__, __LINE__);

    /* input */
    const keymaster_blob_t aad = { input.data(), input.size() };
    keymaster_key_param_set_t set = {};
    keymaster_key_param_t param = {};

    param.tag = KM_TAG_ASSOCIATED_DATA;
    param.blob = aad;

    set.length = 1;
    set.params = &param;

    const keymaster_key_param_set_t additional_params = set;

    const kmHardwareAuthToken auth_token = authToken2kmAuthToken(authToken);
    const kmTimestampToken timestamp_token(timestampToken);

    keymaster_error_t ret = impl_->update_aad(
            opHandle_,
            &additional_params,
            &auth_token,
            &timestamp_token);

    return kmError2ScopedAStatus(ret);

}

ScopedAStatus StrongboxKeyMintOperation::update(const vector<uint8_t>& input,
        /* authToken */
        const optional<HardwareAuthToken>& authToken,
        /* timestampToken */
        const optional<TimeStampToken>& timeStamp,
        vector<uint8_t>* output)
{
    BLOGD("%s: %s: %d", __FILE__, __func__, __LINE__);

    if (!output) return kmError2ScopedAStatus(KM_ERROR_OUTPUT_PARAMETER_NULL);

    /* input */
    const keymaster_blob_t input_data = { input.data(), input.size() };
    const kmHardwareAuthToken auth_token = authToken2kmAuthToken(authToken);
    const kmTimestampToken timestamp_token(timeStamp);

    /* output */
    uint32_t resultConsumed = 0;
    keymaster_key_param_set_t out_params = {};
    keymaster_blob_t output_data = {};

    keymaster_error_t ret = impl_->update(
            opHandle_,
            // NULL,
            &input_data,
            &auth_token,
            &timestamp_token,
            /* output */
            &resultConsumed, &out_params, &output_data);

    if (ret != KM_ERROR_OK) {
        return kmError2ScopedAStatus(ret);
    }

    /* TODO: others? */
    *output = kmBlob2vector(output_data);

    if (output_data.data && output_data.data_length > 0) {
        free((void *)output_data.data);
        output_data.data = NULL;
        output_data.data_length = 0;
    }

    keymaster_free_param_set(&out_params);
    return ScopedAStatus::ok();
}

ScopedAStatus StrongboxKeyMintOperation::finish(const optional<vector<uint8_t>>& input,
        const optional<vector<uint8_t>>& signature,
        const optional<HardwareAuthToken>& authToken,
        const optional<TimeStampToken>& timestampToken,
        const optional<vector<uint8_t>>& confirmationToken,
        vector<uint8_t>* output)
{
    BLOGD("%s: %s: %d", __FILE__, __func__, __LINE__);

    if (!output) return kmError2ScopedAStatus(KM_ERROR_OUTPUT_PARAMETER_NULL);

    /* input */
    keymaster_blob_t input_data = { input->data(), input->size() };
    keymaster_blob_t in_signature = { signature->data(), signature->size() };
    kmHardwareAuthToken auth_token = authToken2kmAuthToken(authToken);
    keymaster_blob_t confirmation_token = { confirmationToken->data(), confirmationToken->size() };
    const kmTimestampToken timestamp_token(timestampToken);

    /* output */
    keymaster_key_param_set_t out_params = {};
    keymaster_blob_t output_data = {};

    keymaster_error_t ret = impl_->finish(
            opHandle_,
            &input_data, // when Key agreement, input data will be encoded public key
            &in_signature,
            &auth_token,
            &timestamp_token,
            &confirmation_token,
            /* output */
            &out_params, &output_data);

    if (ret != KM_ERROR_OK) {
        return kmError2ScopedAStatus(ret);
    }

    *output =  kmBlob2vector(output_data);

    if (output_data.data && output_data.data_length > 0) {
        BLOGD("output_addr(%p): size(%zu)", output_data.data, output_data.data_length);
        free((void *)output_data.data);
        output_data.data = NULL;
        output_data.data_length = 0;
    }

    opHandle_ = 0;
    keymaster_free_param_set(&out_params);

    return ScopedAStatus::ok();
}

ScopedAStatus StrongboxKeyMintOperation::abort()
{
    BLOGD("%s: %s: %d", __FILE__, __func__, __LINE__);

    keymaster_error_t ret = impl_->abort(opHandle_);

    if (ret != KM_ERROR_OK) {
        return kmError2ScopedAStatus(ret);
    }
    return ScopedAStatus::ok();
}

} // namespace
