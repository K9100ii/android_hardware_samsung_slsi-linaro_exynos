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

#ifndef _STRONGBOX_SHARED_SECRET_H_
#define _STRONGBOX_SHARED_SECRET_H_

#include <aidl/android/hardware/security/sharedsecret/BnSharedSecret.h>
#include <hidl/Status.h>

#include <hidl/MQDescriptor.h>

namespace aidl::android::hardware::security::sharedsecret {

using ::ndk::ScopedAStatus;
using ::std::shared_ptr;
using ::std::vector;


class StrongboxSharedSecret : public BnSharedSecret {
public:
    explicit StrongboxSharedSecret(StrongboxKeymintImpl *implementation);
    virtual ~StrongboxSharedSecret();

    ScopedAStatus getSharedSecretParameters(SharedSecretParameters* params);
    ScopedAStatus computeSharedSecret(const vector<SharedSecretParameters>& params,
            vector<uint8_t>* sharingCheck);

private:
    StrongboxKeymintImpl *impl_;
};

} // namespace ::sharedsecret

#endif // _STRONGBOX_SHARED_SECRET_H_
