/*
 * Copyright (c) 2021 TRUSTONIC LIMITED
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
#define LOG_TAG "android.hardware.security.keymint-impl"

#include <sys/endian.h>
#include <log/log.h>
#include <aidl/android/hardware/security/keymint/ErrorCode.h>
#include <aidl/android/hardware/security/secureclock/ISecureClock.h>
#include "AndroidKeyMintOperation.h"

namespace aidl::android::hardware::security::keymint {

using secureclock::TimeStampToken;

AndroidKeyMintOperation::AndroidKeyMintOperation(
    TrustonicKeymintDeviceImpl *impl,
    keymaster_operation_handle_t opHandle)
    : impl_(impl), opHandle_(opHandle) {}

AndroidKeyMintOperation::~AndroidKeyMintOperation() {
    if (opHandle_ != 0) {
        abort();
    }
}

inline ScopedAStatus kmError2ScopedAStatus(const keymaster_error_t value) {
    return (value == KM_ERROR_OK
                ? ScopedAStatus::ok()
                : ScopedAStatus(AStatus_fromServiceSpecificError(static_cast<int32_t>(value))));
}

ScopedAStatus
AndroidKeyMintOperation::updateAad(const vector<uint8_t>& input,
                                   const optional<HardwareAuthToken>& authToken,
                                   const optional<TimeStampToken>& /* timestampToken */) {
    const keymaster_blob_t input_blob = {
        input.data(), input.size()};
    keymaster_hw_auth_token_t auth_token = {};
    if (authToken) {
        auth_token.challenge = authToken->challenge;
        auth_token.user_id = authToken->userId;
        auth_token.authenticator_id = authToken->authenticatorId;
        auth_token.authenticator_type = keymaster_hw_auth_type_t(ntohl(static_cast<uint32_t>(authToken->authenticatorType)));
        auth_token.timestamp = ntohq(authToken->timestamp.milliSeconds);
        auth_token.mac = {authToken->mac.data(), authToken->mac.size()};
    }
    keymaster_error_t ret = impl_->update_aad(
        opHandle_, &input_blob, (authToken)?&auth_token:NULL, NULL);
    if (ret != KM_ERROR_OK) {
        return kmError2ScopedAStatus(ret);
    }
    return ScopedAStatus::ok();
}

ScopedAStatus AndroidKeyMintOperation::update(const vector<uint8_t>& input,
                                              const optional<HardwareAuthToken>& authToken,
                                              const optional<TimeStampToken>&
                                              /*timestampToken*/,
                                              vector<uint8_t>* output) {
    const keymaster_blob_t input_blob = {
        input.data(), input.size()};
    keymaster_hw_auth_token_t auth_token = {};
    if (authToken) {
        auth_token.challenge = authToken->challenge;
        auth_token.user_id = authToken->userId;
        auth_token.authenticator_id = authToken->authenticatorId;
        auth_token.authenticator_type = keymaster_hw_auth_type_t(ntohl(static_cast<uint32_t>(authToken->authenticatorType)));
        auth_token.timestamp = ntohq(authToken->timestamp.milliSeconds);
        auth_token.mac = {authToken->mac.data(), authToken->mac.size()};
    }
    size_t input_consumed = 0;
    keymaster_blob_t output_blob = {};
    keymaster_error_t ret = impl_->update(
        opHandle_, &input_blob, (authToken)?&auth_token:NULL, NULL, &input_consumed, &output_blob);
    if (ret != KM_ERROR_OK) {
        return kmError2ScopedAStatus(ret);
    }
    if (input_consumed != input.size()) {
        return kmError2ScopedAStatus(KM_ERROR_UNKNOWN_ERROR);
    }
    *output = std::vector(output_blob.data, output_blob.data + output_blob.data_length);
    free((void*)output_blob.data);
    output_blob.data = NULL;
    output_blob.data_length = 0;
    return ScopedAStatus::ok();
}

ScopedAStatus
AndroidKeyMintOperation::finish(const optional<vector<uint8_t>>& input,
                                const optional<vector<uint8_t>>& signature,
                                const optional<HardwareAuthToken>& authToken,
                                const optional<TimeStampToken>& /* timestampToken */,
                                const optional<vector<uint8_t>>& /*confirmationToken*/,
                                vector<uint8_t>* output) {

    // FIXME Add support for confirmationToken
    keymaster_blob_t input_blob = {};
    if (input) {
        input_blob = {input->data(), input->size()};
    }
    keymaster_blob_t signature_blob  = {};
    if (signature) {
        signature_blob = {signature->data(), signature->size()};
    }
    keymaster_hw_auth_token_t auth_token = {};
    if (authToken) {
        auth_token.challenge = authToken->challenge;
        auth_token.user_id = authToken->userId;
        auth_token.authenticator_id = authToken->authenticatorId;
        auth_token.authenticator_type = keymaster_hw_auth_type_t(ntohl(static_cast<uint32_t>(authToken->authenticatorType)));
        auth_token.timestamp = ntohq(authToken->timestamp.milliSeconds);
        auth_token.mac = {authToken->mac.data(), authToken->mac.size()};
    }
    keymaster_blob_t output_blob = {};
    keymaster_error_t ret = impl_->finish(
        opHandle_, (input)?&input_blob:NULL,
        (signature)?&signature_blob:NULL, (authToken)?&auth_token:NULL, NULL, &output_blob);
    if (ret != KM_ERROR_OK) {
        return kmError2ScopedAStatus(ret);
    }
    *output = std::vector(output_blob.data, output_blob.data + output_blob.data_length);
    free((void*)output_blob.data);
    output_blob.data = NULL;
    output_blob.data_length = 0;
    return ScopedAStatus::ok();
}

ScopedAStatus AndroidKeyMintOperation::abort() {
    keymaster_error_t ret = impl_->abort(opHandle_);
    return kmError2ScopedAStatus(ret);
}

}  // namespace aidl::android::hardware::security::keymint
