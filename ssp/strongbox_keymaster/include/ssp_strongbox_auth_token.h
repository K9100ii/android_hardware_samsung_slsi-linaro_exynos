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

#ifndef __SSP_STRONGBOX_AUTH_TOKEN_H__
#define __SSP_STRONGBOX_AUTH_TOKEN_H__

#include <stdint.h>

/* need to be synced with TEE keymaster
 * Android HW auth token in hardware/libhardware/include/hardware/hw_auth_token.h
 */
typedef enum {
    HW_AUTH_NONE = 0,
    HW_AUTH_PASSWORD = (int)(1 << 0),
    HW_AUTH_FINGERPRINT = (int)(1 << 1),
    // Additional entries should be powers of 2.
    HW_AUTH_ANY = (int)UINT32_MAX,
    HW_AUTH_TYPE_MAX = ((int)(1u << 31)),
} hw_authenticator_type_t;

/* NOTE: need 8bytes align for uint64_t variables */
typedef struct __attribute__((packed, aligned(8))) {
	/* version is changed to uint32_t to meet 4bytes align */
	uint32_t version;
    uint64_t challenge;
    uint64_t user_id;
    uint64_t authenticator_id;
    uint32_t authenticator_type;
    uint64_t timestamp;
    uint8_t mac[32];
} hw_auth_token_t;

#endif

