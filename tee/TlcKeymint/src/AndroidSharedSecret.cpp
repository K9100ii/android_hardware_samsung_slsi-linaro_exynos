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
#define LOG_TAG "android.hardware.security.sharedsecret-impl"
#include <log/log.h>

#include "AndroidSharedSecret.h"
#include <aidl/android/hardware/security/keymint/ErrorCode.h>

namespace aidl::android::hardware::security::sharedsecret {

inline ScopedAStatus kmError2ScopedAStatus(const keymaster_error_t value) {
    return (value == KM_ERROR_OK
                ? ScopedAStatus::ok()
                : ScopedAStatus(AStatus_fromServiceSpecificError(static_cast<int32_t>(value))));
}

class KmSharedSecretParameters : public keymaster_hmac_sharing_parameters_set_t {
public:
    KmSharedSecretParameters(const vector<SharedSecretParameters>& sharedSecretParams) {
        params = new keymaster_hmac_sharing_parameters_t[sharedSecretParams.size()];
        length = sharedSecretParams.size();
        for (size_t i = 0; i < sharedSecretParams.size(); ++i) {
            params[i].seed.data = sharedSecretParams[i].seed.data();
            params[i].seed.data_length = sharedSecretParams[i].seed.size();
            memcpy(params[i].nonce, sharedSecretParams[i].nonce.data(), 32); // FIXME make more robust
        }
    }
    KmSharedSecretParameters(const KmSharedSecretParameters&) = delete;
    ~KmSharedSecretParameters() {
        delete[] params;
    }
};

AndroidSharedSecret::AndroidSharedSecret(std::shared_ptr<keymint::AndroidKeyMintDevice> keymint)
    : impl_(keymint->getImpl()) {}

AndroidSharedSecret::~AndroidSharedSecret() {}

ScopedAStatus AndroidSharedSecret::getSharedSecretParameters(SharedSecretParameters* params) {
    keymaster_hmac_sharing_parameters_t _params = {};
    keymaster_error_t ret = impl_->get_hmac_sharing_parameters(
        &_params);
    if (ret != KM_ERROR_OK) {
        return kmError2ScopedAStatus(ret);
    }
    params->seed = vector<uint8_t>(_params.seed.data, _params.seed.data + _params.seed.data_length);
    params->nonce = vector<uint8_t>(_params.nonce, _params.nonce+sizeof(_params.nonce));
    return ScopedAStatus::ok();
}

ScopedAStatus AndroidSharedSecret::computeSharedSecret(const vector<SharedSecretParameters>& params,
                                                       vector<uint8_t>* sharingCheck) {
    const KmSharedSecretParameters _params(params);
    keymaster_blob_t _sharing_check = {};
    keymaster_error_t ret = impl_->compute_shared_hmac(
        &_params, &_sharing_check);
    if (ret != KM_ERROR_OK) {
        return kmError2ScopedAStatus(ret);
    }
    *sharingCheck = vector<uint8_t>(_sharing_check.data, _sharing_check.data + _sharing_check.data_length);
    return ScopedAStatus::ok();
}

}  // namespace aidl::android::hardware::security::sharedsecret
