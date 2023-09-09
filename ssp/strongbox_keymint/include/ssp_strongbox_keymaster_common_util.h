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

#ifndef __SSP_STRONGBOX_KEYMASTER_COMMON_UTIL_H__
#define __SSP_STRONGBOX_KEYMASTER_COMMON_UTIL_H__

#include <memory>

#include "ssp_strongbox_keymaster_defs.h"
#include "ssp_strongbox_keymaster_log.h"

/* version binding info */
typedef struct {
    uint32_t os_version;
    uint32_t os_patchlevel;
    uint32_t vendor_patchlevel;
    uint32_t boot_patchlevel;
} system_version_info_t;

#define EXPECT_KMOK_RETURN(expr) \
    do { \
        ret = (expr); \
        if (ret != KM_ERROR_OK) { \
            BLOGE("%s == %d", #expr, ret); \
            return ret; \
        } \
    } while (0)


#define EXPECT_KMOK_GOTOEND(expr) \
    do { \
        ret = (expr); \
        if (ret != KM_ERROR_OK) { \
            BLOGE("%s == %d", #expr, ret); \
            goto end; \
        } \
    } while (0)

#define EXPECT_TRUE_GOTOEND(errcode, expr) \
    do { \
        if (!(expr)) { \
            BLOGE("'%s' is false", #expr); \
            ret = (errcode); \
            goto end; \
        } \
    } while (false)

#define EXPECT_TRUE_RETURN(errcode, expr) \
    do { \
        if (!(expr)) { \
            BLOGE("'%s' is false", #expr); \
            return errcode; \
        } \
    } while (false)

#define EXPECT_NE_NULL_GOTOEND(expr) \
    EXPECT_TRUE_GOTOEND(KM_ERROR_UNEXPECTED_NULL_POINTER, (expr) != NULL)

#define EXPECT_NE_NULL_RETURN(expr) \
    EXPECT_TRUE_RETURN(KM_ERROR_UNEXPECTED_NULL_POINTER, (expr) != NULL)


#ifndef BITS2BYTES
#define BITS2BYTES(n) (((n)+7)/8)
#endif

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

uint32_t btoh_u32(
        const uint8_t *pos);

uint64_t btoh_u64(
        const uint8_t *pos);

void htob_u32(
        uint8_t *pos,
        uint32_t val);

void htob_u64(
        uint8_t *pos,
        uint64_t val);

/**
 * append uint32_t to buffer as little endian
 * and advance buffer pointer sizeof uint32_t.
 */
void append_u32_to_buf(uint8_t **pos, uint32_t val);

/**
 * append uint64_t to buffer as little endian
 * and advance buffer pointer sizeof uint64_t.
 */
void append_u64_to_buf(uint8_t **pos, uint64_t val);

/**
 * append uint8_t array to buffer
 * and advance buffer pointer as array size
 */
void append_u8_array_to_buf(
        uint8_t **pos,
        const uint8_t *src,
        uint32_t len);

int find_tag_pos(
        const keymaster_key_param_set_t *params,
        keymaster_tag_t tag);

bool check_purpose(
        const keymaster_key_param_set_t *params,
        keymaster_purpose_t purpose);

/**
 * get int value for TAG
 */
keymaster_error_t get_tag_value_int(
        const keymaster_key_param_set_t *params,
        keymaster_tag_t tag,
        uint32_t *value);

/**
 * get enum value for TAG
 */
keymaster_error_t get_tag_value_enum(
        const keymaster_key_param_set_t *params,
        keymaster_tag_t tag,
        uint32_t *value);

/**
 * get long value for TAG
 */
keymaster_error_t get_tag_value_long(
        const keymaster_key_param_set_t *params,
        keymaster_tag_t tag,
        uint64_t *value);

uint32_t ec_curve_to_keysize(
        keymaster_ec_curve_t ec_curve);


int get_file_contents(
        const char *path,
        std::unique_ptr<uint8_t[]>& contents,
        long *fsize);

int get_blob_contents(
        const keymaster_key_blob_t* blob,
        std::unique_ptr<uint8_t[]>& contents,
        long *fsize);


/* FIXME: optimize for strongbox keymaster */
/* max size of array to store params for import or generation.
 *
 * 32-bit tags (max 8):
 *
 * KM_TAG_BLOB_USAGE_REQUIREMENTS
 * KM_TAG_EC_CURVE
 * KM_TAG_ORIGIN
 * KM_TAG_ROLLBACK_RESISTANCE -- todo: need to check
 * KM_TAG_OS_VERSION
 * KM_TAG_OS_PATCHLEVEL
 * KM_TAG_VENDOR_PATCHLEVEL
 * KM_TAG_BOOT_PATCHLEVEL
 *
 * 64-bit tags (max 2):
 *
 * KM_TAG_USER_SECURE_ID (up to 2)
 */
#define SB_EXTRA_PARAMS_MAX_SIZE (8*(4+4) + 2*(4+8)) /* for each tag + value*/


/**
 * ECC curve types supported by strongbox(sync with sspfw)
 */
typedef enum {
    ecc_curve_nist_p_256 = 0,
} ecc_curve_t;

keymaster_error_t ssp_keyparser_parse_keydata_pkcs8ec(
        std::unique_ptr<uint8_t[]>& key_data_ptr,
        size_t *key_data_len,
        const uint8_t *data,
        size_t data_len);

keymaster_error_t ssp_keyparser_parse_keydata_pkcs8rsa(
        std::unique_ptr<uint8_t[]>& key_data_ptr,
        size_t *key_data_len,
        const uint8_t *data,
        size_t data_len);

keymaster_error_t ssp_keyparser_parse_keydata_raw(
        keymaster_algorithm_t algorithm,
        std::unique_ptr<uint8_t[]>& key_data_ptr,
        size_t *key_data_len,
        const uint8_t *data,
        size_t data_len);

keymaster_error_t ssp_keyparser_get_rsa_exponent_from_parsedkey(
        uint8_t *keydata,
        size_t keydata_len,
        uint64_t *rsa_e);

#endif /* __SSP_STRONGBOX_KEYMASTER_COMMON_UTIL_H__ */

