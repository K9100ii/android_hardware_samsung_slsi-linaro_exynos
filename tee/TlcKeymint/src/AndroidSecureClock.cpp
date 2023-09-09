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
#define LOG_TAG "android.hardware.security.secureclock-impl"
#include <log/log.h>

#include "AndroidSecureClock.h"

#include <aidl/android/hardware/security/keymint/ErrorCode.h>

namespace aidl::android::hardware::security::secureclock {

inline ScopedAStatus kmError2ScopedAStatus(const keymaster_error_t value) {
    return (value == KM_ERROR_OK
                ? ScopedAStatus::ok()
                : ScopedAStatus(AStatus_fromServiceSpecificError(static_cast<int32_t>(value))));
}

AndroidSecureClock::AndroidSecureClock(
    std::shared_ptr<keymint::AndroidKeyMintDevice> keymint)
    : impl_(keymint->getImpl()) {}

AndroidSecureClock::~AndroidSecureClock() {}

ScopedAStatus AndroidSecureClock::generateTimeStamp(int64_t challenge, TimeStampToken* token) {
    keymaster_timestamp_token_t local_token = {};
    local_token.challenge = challenge;
    keymaster_error_t ret = impl_->generate_timestamp(&local_token);
    if (ret != KM_ERROR_OK) {
        return kmError2ScopedAStatus(ret);
    }
    token->challenge = local_token.challenge;
    token->timestamp.milliSeconds = static_cast<int64_t>(local_token.timestamp);
    token->mac = vector<uint8_t>(local_token.mac, local_token.mac+32);
    return ScopedAStatus::ok();
}

}  // namespace aidl::android::hardware::security::secureclock
