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

#include <aidl/android/hardware/security/keymint/ErrorCode.h>

#include <log/log.h>
#include "StrongboxKeymintDevice.h"
#include "ssp_strongbox_keymaster_defs.h"
#include "ssp_strongbox_keymaster_log.h"
#include "KeyMintUtils.h"

#include "StrongboxSharedSecret.h"

namespace aidl::android::hardware::security::sharedsecret {

using secureclock::Timestamp;
using namespace aidl::android::hardware::security::keymint::km_utils;

StrongboxSharedSecret::StrongboxSharedSecret(
    StrongboxKeymintImpl *implementation)
    : impl_(std::move(implementation)) {}

StrongboxSharedSecret::~StrongboxSharedSecret() {}

ScopedAStatus StrongboxSharedSecret::getSharedSecretParameters(SharedSecretParameters* params)
{
    BLOGD("%s: %s: %d", __FILE__, __func__, __LINE__);

    /* output */
    keymaster_hmac_sharing_params_t hmac_params = {};

    keymaster_error_t ret = impl_->get_hmac_sharing_parameters(&hmac_params);
    if (ret != KM_ERROR_OK) {
        return kmError2ScopedAStatus(ret);
    }

    params->seed = kmBlob2vector(hmac_params.seed);
    params->nonce = kmBlob2vector(hmac_params.nonce);

    return ScopedAStatus::ok();
}

ScopedAStatus StrongboxSharedSecret::computeSharedSecret(
                                            const vector<SharedSecretParameters>& params,
                                            vector<uint8_t>* sharingCheck)
{
    BLOGD("%s: %s: %d", __FILE__, __func__, __LINE__);

    /* input */
    const kmHmacSharingParamsSet sharing_params(params);

    /* output */
    keymaster_blob_t sharing_check = {};

    keymaster_error_t ret = impl_->compute_shared_hmac(&sharing_params, &sharing_check);
    if (ret != KM_ERROR_OK) {
        return kmError2ScopedAStatus(ret);
    }

    *sharingCheck = kmBlob2vector(sharing_check);

    if (sharing_check.data && sharing_check.data_length > 0) {
        free((void *)sharing_check.data);
        sharing_check.data = NULL;
        sharing_check.data_length = 0;
    }

    return ScopedAStatus::ok();
}

} // namespacea ::sharedsecret
