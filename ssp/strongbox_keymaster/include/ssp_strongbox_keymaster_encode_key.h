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

#ifndef __SSP_STRONGBOX_KEYMASTER_ENCODE_KEY_H__
#define __SSP_STRONGBOX_KEYMASTER_ENCODE_KEY_H__

#include <memory>

#include "ssp_strongbox_keymaster_common_util.h"

keymaster_error_t ssp_encode_pubkey(
    keymaster_algorithm_t keytype,
    uint32_t keysize,
    keymaster_key_format_t export_format,
    const uint8_t *exported_keyblob_raw,
    uint32_t exported_keyblob_len,
    keymaster_blob_t *exported_keyblob);


#endif
