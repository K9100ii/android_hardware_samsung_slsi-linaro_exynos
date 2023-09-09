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

#ifndef _STRONGBOX_KEYMINT_OPERATION_H_
#define _STRONGBOX_KEYMINT_OPERATION_H_

#include <aidl/android/hardware/security/keymint/BnKeyMintOperation.h>
#include <aidl/android/hardware/security/secureclock/ISecureClock.h>

#include "StrongboxKeymintOperation.h"
#include "StrongboxKeymintImpl.h"
#include "ssp_strongbox_keymaster_defs.h"
#include "ssp_strongbox_keymaster_log.h"

#include "KeyMintUtils.h"

namespace aidl::android::hardware::security::keymint {

using secureclock::TimeStampToken;
using std::string;
using ::std::shared_ptr;
using ::ndk::SpAIBinder;
using ::ndk::SharedRefBase;
using ::ndk::ScopedAStatus;
using ::std::optional;
using ::std::vector;


class StrongboxKeyMintOperation : public BnKeyMintOperation {
  public:
      explicit StrongboxKeyMintOperation(
              StrongboxKeymintImpl *implementation,
              keymaster_operation_handle_t opHandle);
      virtual ~StrongboxKeyMintOperation();

      ScopedAStatus updateAad(const vector<uint8_t>& input,
              const optional<HardwareAuthToken>& authToken,
              const optional<TimeStampToken>& timestampToken);

      ScopedAStatus update(const vector<uint8_t>& input,
              const optional<HardwareAuthToken>& authToken,
              const optional<TimeStampToken>& timestampToken,
              vector<uint8_t>* output);

      ScopedAStatus finish(const optional<vector<uint8_t>>& input,        //
              const optional<vector<uint8_t>>& signature,    //
              const optional<HardwareAuthToken>& authToken,  //
              const optional<TimeStampToken>& timestampToken,
              const optional<vector<uint8_t>>& confirmationToken,
              vector<uint8_t>* output);

      ScopedAStatus abort();

  protected:
      StrongboxKeymintImpl *impl_;
      keymaster_operation_handle_t opHandle_;
};

}
#endif //
