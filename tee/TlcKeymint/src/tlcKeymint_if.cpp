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

/* Get PRIiN and friends from <inttypes.h> despite the C++ standard not
 * wanting to let us.
 */
#define __STDC_FORMAT_MACROS

#include <inttypes.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include "keymint_ta_defs.h"
#include "tlcKeymint_if.h"
#include "km_util.h"
#include "serialization.h"
#include "authlist.h"
#include "km_encodings.h"
/* ExySp: add header for propery */
#include "cutils/properties.h"
#include <unistd.h>

#define RSA_MAX_KEY_SIZE 4096
#define EC_MAX_KEY_SIZE 521
#define AES_MAX_KEY_SIZE 256
#define HMAC_MAX_KEY_SIZE 2048
#define TRIPLE_DES_KEY_SIZE 192 /* Including parity bits */

#ifdef NDEBUG
/* In Release mode these macros are void. */
#define PRINT_BUFFER(data, data_length)
#define PRINT_BLOB(blob)
#define PRINT_BLOB_HEX(blob, length)
#define PRINT_PARAM_SET(params)

#else
#include <sstream>

#define SBUF_SIZE 1024
static char sbuf[SBUF_SIZE]; // for formatting debug output prior to LOG_I

static int snprint_buffer(
    char *s, size_t n,
    const uint8_t *data,
    size_t data_length)
{
/* ExySp */
    int wlen = 0, i, tmp_wlen;

    if (data == NULL) {
        return snprintf(s, n, "<NULL>\n");
    } else {
        wlen = snprintf(s, n, "<data of length %zu>\n", data_length);
	s = s + wlen;
	for (i = 0; i < (int)data_length; i++) {
	   tmp_wlen = snprintf(s, n - wlen, "0x%02x ", data[i]);
	   wlen += tmp_wlen;
           s += tmp_wlen;
	}
	tmp_wlen = snprintf(s, n - wlen, "\n");
	wlen += tmp_wlen;
	return wlen;
/* ExySp end */
    }
}

static int snprint_uint64(
    char *s, size_t n,
    uint64_t x)
{
    return snprintf(s, n, "0x%08x%08x\n", (uint32_t)(x >> 32), (uint32_t)(x & 0xFFFFFFFF));
}

static int snprint_bool(
    char *s, size_t n,
    bool x)
{
    return snprintf(s, n, x ? "true\n" : "false\n");
}

static int snprint_blob(
    char *s, size_t n,
    const keymaster_blob_t *blob)
{
    if (blob == NULL) {
        return snprintf(s, n, "<NULL>\n");
    } else {
        return snprint_buffer(s, n, blob->data, blob->data_length);
    }
}

static int snprint_param_set(
    char *s, size_t n,
    const keymaster_key_param_set_t *param_set)
{
    if (param_set == NULL) {
        return snprintf(s, n, "<NULL>\n");
    }
    else {
        int r;
        size_t i = 0, l = 0;
        while ((i < param_set->length) && (l < SBUF_SIZE)) {
            const keymaster_key_param_t param = param_set->params[i];
            switch (param.tag) {
#define PRINT_ENUM snprintf(s + l, SBUF_SIZE - l, "0x%08x\n", param.enumerated)
#define PRINT_UINT snprintf(s + l, SBUF_SIZE - l, "%u\n", param.integer)
#define PRINT_ULONG snprint_uint64(s + l, SBUF_SIZE - l, param.long_integer)
#define PRINT_DATE snprint_uint64(s + l, SBUF_SIZE - l, param.date_time)
#define PRINT_BOOL snprint_bool(s + l, SBUF_SIZE - l, param.boolean)
#define PRINT_BYTES snprint_blob(s + l, SBUF_SIZE - l, &param.blob)
#define PRINT_STR snprintf(s + l, SBUF_SIZE - l, "%s\n", param.blob.data)
#define PARAM_CASE(tag, printval) \
    case tag: \
        r = snprintf(s + l, SBUF_SIZE - l, "%s: ", #tag); \
        if (r < 0) return r; \
        l += r; \
        if (l < SBUF_SIZE) { \
            r = printval; \
            if (r < 0) return r; \
            l += r; \
        } \
        break;
#define PARAM_CASE_ENUM(tag) PARAM_CASE(tag, PRINT_ENUM)
#define PARAM_CASE_UINT(tag) PARAM_CASE(tag, PRINT_UINT)
#define PARAM_CASE_ULONG(tag) PARAM_CASE(tag, PRINT_ULONG)
#define PARAM_CASE_DATE(tag) PARAM_CASE(tag, PRINT_DATE)
#define PARAM_CASE_BOOL(tag) PARAM_CASE(tag, PRINT_BOOL)
#define PARAM_CASE_BIGNUM(tag) PARAM_CASE(tag, PRINT_BYTES)
#define PARAM_CASE_BYTES(tag) PARAM_CASE(tag, PRINT_BYTES)
#define PARAM_CASE_STR(tag) PARAM_CASE(tag, PRINT_STR)
                PARAM_CASE_DATE(KM_TAG_ACTIVE_DATETIME)
                PARAM_CASE_ENUM(KM_TAG_ALGORITHM)
                PARAM_CASE_BOOL(KM_TAG_ALLOW_WHILE_ON_BODY)
                PARAM_CASE_BYTES(KM_TAG_APPLICATION_DATA)
                PARAM_CASE_BYTES(KM_TAG_APPLICATION_ID)
                PARAM_CASE_BYTES(KM_TAG_ATTESTATION_CHALLENGE)
                PARAM_CASE_UINT(KM_TAG_AUTH_TIMEOUT)
                PARAM_CASE_ENUM(KM_TAG_BLOCK_MODE)
                PARAM_CASE_UINT(KM_TAG_BOOT_PATCHLEVEL)
                PARAM_CASE_BOOL(KM_TAG_BOOTLOADER_ONLY)
                PARAM_CASE_BOOL(KM_TAG_CALLER_NONCE)
                PARAM_CASE_BYTES(KM_TAG_CONFIRMATION_TOKEN)
                PARAM_CASE_DATE(KM_TAG_CREATION_DATETIME)
                PARAM_CASE_ENUM(KM_TAG_DIGEST)
                PARAM_CASE_ENUM(KM_TAG_EC_CURVE)
                PARAM_CASE_ENUM(KM_TAG_HARDWARE_TYPE)
                PARAM_CASE_BOOL(KM_TAG_INCLUDE_UNIQUE_ID)
                PARAM_CASE_ENUM(KM_TAG_RSA_OAEP_MGF_DIGEST)
                PARAM_CASE_UINT(KM_TAG_KEY_SIZE)
                PARAM_CASE_UINT(KM_TAG_MAC_LENGTH)
                PARAM_CASE_UINT(KM_TAG_MAX_USES_PER_BOOT)
                PARAM_CASE_UINT(KM_TAG_MIN_MAC_LENGTH)
                PARAM_CASE_UINT(KM_TAG_MIN_SECONDS_BETWEEN_OPS)
                PARAM_CASE_BOOL(KM_TAG_NO_AUTH_REQUIRED)
                PARAM_CASE_BYTES(KM_TAG_NONCE)
                PARAM_CASE_ENUM(KM_TAG_ORIGIN)
                PARAM_CASE_DATE(KM_TAG_ORIGINATION_EXPIRE_DATETIME)
                PARAM_CASE_UINT(KM_TAG_OS_PATCHLEVEL)
                PARAM_CASE_UINT(KM_TAG_OS_VERSION)
                PARAM_CASE_ENUM(KM_TAG_PADDING)
                PARAM_CASE_ENUM(KM_TAG_PURPOSE)
                PARAM_CASE_BOOL(KM_TAG_RESET_SINCE_ID_ROTATION)
                PARAM_CASE_BOOL(KM_TAG_ROLLBACK_RESISTANCE)
                PARAM_CASE_BYTES(KM_TAG_ROOT_OF_TRUST)
                PARAM_CASE_ULONG(KM_TAG_RSA_PUBLIC_EXPONENT)
                PARAM_CASE_BOOL(KM_TAG_TRUSTED_CONFIRMATION_REQUIRED)
                PARAM_CASE_BOOL(KM_TAG_TRUSTED_USER_PRESENCE_REQUIRED)
                PARAM_CASE_BYTES(KM_TAG_UNIQUE_ID)
                PARAM_CASE_BOOL(KM_TAG_UNLOCKED_DEVICE_REQUIRED)
                PARAM_CASE_DATE(KM_TAG_USAGE_EXPIRE_DATETIME)
                PARAM_CASE_ENUM(KM_TAG_USER_AUTH_TYPE)
                PARAM_CASE_UINT(KM_TAG_USER_ID)
                PARAM_CASE_ULONG(KM_TAG_USER_SECURE_ID)
                PARAM_CASE_UINT(KM_TAG_VENDOR_PATCHLEVEL)
                PARAM_CASE_STR(KM_TAG_ATTESTATION_ID_BRAND)
                PARAM_CASE_STR(KM_TAG_ATTESTATION_ID_DEVICE)
                PARAM_CASE_STR(KM_TAG_ATTESTATION_ID_PRODUCT)
                PARAM_CASE_STR(KM_TAG_ATTESTATION_ID_SERIAL)
                PARAM_CASE_STR(KM_TAG_ATTESTATION_ID_IMEI)
                PARAM_CASE_STR(KM_TAG_ATTESTATION_ID_MEID)
                PARAM_CASE_STR(KM_TAG_ATTESTATION_ID_MANUFACTURER)
                PARAM_CASE_STR(KM_TAG_ATTESTATION_ID_MODEL)
                PARAM_CASE_BOOL(KM_TAG_EARLY_BOOT_ONLY)
                PARAM_CASE_BOOL(KM_TAG_DEVICE_UNIQUE_ATTESTATION)
                PARAM_CASE_BYTES(KM_TAG_ATTESTATION_APPLICATION_ID)
                PARAM_CASE_BIGNUM(KM_TAG_CERTIFICATE_SERIAL)
                PARAM_CASE_BYTES(KM_TAG_CERTIFICATE_SUBJECT)
                PARAM_CASE_DATE(KM_TAG_CERTIFICATE_NOT_BEFORE)
                PARAM_CASE_DATE(KM_TAG_CERTIFICATE_NOT_AFTER)
                PARAM_CASE_UINT(KM_TAG_USAGE_COUNT_LIMIT)
                default:
                    r = snprintf(s + l, SBUF_SIZE - l, "<unknown tag 0x%08x>\n", param.tag);
                    if (r < 0) return r;
                    l += r;
            }
            i++;
        }
        return l;
    }
}

#define PRINT_BUFFER(data, data_length) \
    do { \
        if (snprint_buffer(sbuf, SBUF_SIZE, data, data_length) >= 0) { \
            LOG_D("%s = %s", #data, sbuf); \
        } \
    } while (0)
#define PRINT_BLOB(blob) \
    do { \
        if (snprint_blob(sbuf, SBUF_SIZE, blob) >= 0) { \
            LOG_D("%s = %s", #blob, sbuf); \
        } \
    } while (0)
#define PRINT_PARAM_SET(params) \
    do { \
        if (snprint_param_set(sbuf, SBUF_SIZE, params) >= 0) { \
            LOG_D("%s =\n%s", #params, sbuf); \
        } \
    } while (0)

#define PRINT_BLOB_HEX(blob, len)                           \
    do {                                                    \
        std::ostringstream buf;                             \
        for(size_t i=0; i<len; ++i) {                       \
            buf << std::hex << (unsigned) blob[i] << " ";   \
        }                                                   \
        LOG_D("%s = %s\n", #blob, buf.str().c_str());       \
    }                                                       \
    while (0)

#endif

/* Global definitions */
static const uint32_t gDeviceId = MC_DEVICE_ID_DEFAULT;
static const mcUuid_t gUuid = TEE_KEYMINT_TA_UUID;

extern inline void keymaster_free_param_values(keymaster_key_param_t* param, size_t param_count);
extern inline void keymaster_free_param_set(keymaster_key_param_set_t* set);
extern inline void keymaster_free_characteristics(keymaster_key_characteristics_t* characteristics);

/* ExySp: add timeout to check setting property */
#define SECURE_OS_TIMEOUT 50000

/**
 * Get value of enumerated tag from key parameters
 */
static keymaster_error_t get_enumerated_tag(
    const keymaster_key_param_set_t *params,
    keymaster_tag_t tag,
    uint32_t *value)
{
    if ( (params == NULL) || (value == NULL) ) {
        return KM_ERROR_UNEXPECTED_NULL_POINTER;
    }
    for (size_t i = 0; i < params->length; i++) {
        if (params->params[i].tag == tag) {
            *value = params->params[i].enumerated;
            return KM_ERROR_OK;
        }
    }
    return KM_ERROR_INVALID_TAG;
}

/**
 * Test whether an enumerated tag/value pair with type KM_ENUM_REP is present.
 */
static bool test_enumerated_tag_data(
    const keymaster_key_param_set_t *params,
    keymaster_tag_t tag,
    uint32_t value)
{
    if (params == NULL) {
        return false;
    }
    for (size_t i = 0; i < params->length; i++) {
        if ((params->params[i].tag == tag) &&
            (params->params[i].enumerated == value)) {
            return true;
        }
    }
    return false;
}

/**
 * Get value of integer tag from key parameters
 */
static keymaster_error_t get_integer_tag(
    const keymaster_key_param_set_t *params,
    keymaster_tag_t tag,
    uint32_t *value)
{
    if ( (params == NULL) || (value == NULL) ) {
        return KM_ERROR_UNEXPECTED_NULL_POINTER;
    }
    for (size_t i = 0; i < params->length; i++) {
        if (params->params[i].tag == tag) {
            *value = params->params[i].integer;
            return KM_ERROR_OK;
        }
    }
    return KM_ERROR_INVALID_TAG;
}

/**
 * Get value of long integer tag from key parameters
 */
static keymaster_error_t get_long_integer_tag(
    const keymaster_key_param_set_t *params,
    keymaster_tag_t tag,
    uint64_t *value)
{
    if ( (params == NULL) || (value == NULL) ) {
        return KM_ERROR_UNEXPECTED_NULL_POINTER;
    }
    for (size_t i = 0; i < params->length; i++) {
        if (params->params[i].tag == tag) {
            *value = params->params[i].long_integer;
            return KM_ERROR_OK;
        }
    }
    return KM_ERROR_INVALID_TAG;
}

/* ExySp: func to check if secureOS is loaded or not. */
static int secure_os_init(void)
{
    char state[PROPERTY_VALUE_MAX];
    int i;

    for (i = 0; i < SECURE_OS_TIMEOUT; i++) {
        property_get("vendor.sys.mobicoredaemon.enable", state, 0);
        if (!strncmp(state, "true", strlen("true") + 1))
		break;
	else
		usleep(500);
    }

    if (i == SECURE_OS_TIMEOUT) {
	LOG_E("%s: secure os init timed out!", __func__);
	return KM_ERROR_SECURE_HW_COMMUNICATION_FAILED;
    } else {
	LOG_D("%s: secure os init is done", __func__);
	return KM_ERROR_OK;
    }
}

/**
 * Maximum supported key size in bits.
 *
 * @param algorithm key type
 * @return maximum supported size in bits
 */
static uint32_t km_max_key_size(
    keymaster_algorithm_t algorithm)
{
    switch (algorithm) {
        case KM_ALGORITHM_RSA:
            return RSA_MAX_KEY_SIZE;
        case KM_ALGORITHM_EC:
            return EC_MAX_KEY_SIZE;
        case KM_ALGORITHM_AES:
            return AES_MAX_KEY_SIZE;
        case KM_ALGORITHM_HMAC:
            return HMAC_MAX_KEY_SIZE;
        case KM_ALGORITHM_TRIPLE_DES:
            return TRIPLE_DES_KEY_SIZE;
        default:
            return 0;
    }
}

/**
 * Upper bound km_key_data in plain form.
 *
 * @param algorithm key type
 * @param key_size_in_bits key size in bits if known; if zero, assume worst case
 * @return upper bound for km_key_data size
 */
static uint32_t km_key_data_max_size(
    keymaster_algorithm_t algorithm,
    uint32_t  key_size_in_bits)
{
    uint32_t keylen = BITS_TO_BYTES(
        (key_size_in_bits > 0) ? key_size_in_bits : km_max_key_size(algorithm));
    uint32_t prelim_size = 4 + 4; // key type and key size, both uint32_t
    switch(algorithm)
    {
        case KM_ALGORITHM_RSA:
            /* n + e + d + p + q + dp + dq + qinv */
            return prelim_size + KM_RSA_METADATA_SIZE + 8 * keylen;
        case KM_ALGORITHM_EC:
            /* k + x + y */
            return prelim_size + KM_EC_METADATA_SIZE + 3 * keylen;
        case KM_ALGORITHM_AES:
        case KM_ALGORITHM_HMAC:
        case KM_ALGORITHM_TRIPLE_DES:
            /* no metadata */
            return prelim_size + keylen;
        default:
            /* Unsupported algorithm */
            return 0;
    }
}

/**
 * Calculate upper bound on length of exported key data.
 *
 * @return upper bound on length of exported data, or 0 on error
 */
static uint32_t export_data_length(
    keymaster_algorithm_t key_type,
    uint32_t key_size) // bits
{
    switch (key_type) {
        case KM_ALGORITHM_AES:
            /* ExySp hw_wrapped_key = aes 32 + gcm tag 16 +
                        aes 32 + secret 32 + secret_tag 16 */
            return BITS_TO_BYTES(key_size) + 32 + 32 + 16;
        case KM_ALGORITHM_HMAC:
        case KM_ALGORITHM_TRIPLE_DES:
            // export not supported for symmetric keys
            return 0;
        case KM_ALGORITHM_RSA:
            // type | size | size | n_len | e_len | n | e
            return 5*4 + 2*BITS_TO_BYTES(key_size);
        case KM_ALGORITHM_EC:
            // type | size | curve | x_len | y_len | x | y
            return 5*4 + 2*BITS_TO_BYTES(key_size);
        default:
            return 0;
    }
}

/**
 * Calculate upper bound for size in bytes of material in encrypted key blob.
 *
 * \param key_size_in_bits Key size in bits if known. If zero assume worst case.
 */
static uint32_t key_blob_max_size(
    keymaster_algorithm_t algorithm,
    uint32_t key_size_in_bits,
    uint32_t params_size_in_bytes)
{
    /* params_len (uint32_t) + caller-supplied params */
    uint32_t plain_material_size = 4 + params_size_in_bytes;

    /* characteristics set by us */
    plain_material_size += OWN_PARAMS_MAX_SIZE;

    /* raw key data */
    plain_material_size += km_key_data_max_size(algorithm, key_size_in_bits);

    /*
     * Blob consists of:
     * - 16-byte nonce, ciphertext, and 16-byte tag in version 0
     * - 4-byte header, 16-byte nonce, ciphertext, and 12-byte tag in version 1
     */
    return plain_material_size + 32;
}

/**
 * Map a buffer.
 */
keymaster_error_t map_buffer(
    mcSessionHandle_t* session_handle,
    const uint8_t *buf, uint32_t buflen,
    mcBulkMap_t *bufinfo)
{
    if ((buf != NULL) && (buflen != 0)) {
        mcResult_t mcRet = mcMap(session_handle, (void*)buf, buflen, bufinfo);
        if (mcRet != MC_DRV_OK) {
            LOG_E("%s: mcMap() returned 0x%08x", __func__, mcRet);
            return KM_ERROR_SECURE_HW_COMMUNICATION_FAILED;
        }
    }
    return KM_ERROR_OK;
}

/**
 * Unmap a buffer.
 */
void unmap_buffer(
    mcSessionHandle_t* session_handle,
    const uint8_t *buf,
    mcBulkMap_t *bufinfo)
{
    if (bufinfo->sVirtualAddr != 0) {
        mcResult_t mcRet = mcUnmap(session_handle, (void*)buf, bufinfo);
        if (mcRet != MC_DRV_OK) {
            LOG_E("%s: mcUnmap() returned 0x%08x", __func__, mcRet);
        }
    }
}

/**
 * Notify the trusted application and wait for response.
 */
keymaster_error_t transact(
    mcSessionHandle_t* session_handle,
    tciMessage_ptr tci)
{
    keymaster_error_t ret = KM_ERROR_OK;
    mcResult_t mcRet;

    mcRet = mcNotify(session_handle);
    if (mcRet != MC_DRV_OK) {
        LOG_E("%s: mcNotify() returned 0x%08x", __func__, mcRet);
        ret = KM_ERROR_SECURE_HW_COMMUNICATION_FAILED;
        goto end;
    }

    mcRet = mcWaitNotification(session_handle, MC_INFINITE_TIMEOUT);
    if (mcRet != MC_DRV_OK) {
        LOG_E("%s: mcWaitNotification() returned 0x%08x", __func__, mcRet);
        ret = KM_ERROR_SECURE_HW_COMMUNICATION_FAILED;
        goto end;
    }

    CHECK_RESULT_OK( (keymaster_error_t)tci->response.header.returnCode );

end:
    return ret;
}

/**
 * Find the operation record given the handle.
 */
static keymaster_error_t lookup_operation(TEE_Session *session,
    keymaster_operation_handle_t handle,
    struct operation **op)
{
    size_t i;
    keymaster_error_t ret = KM_ERROR_INVALID_OPERATION_HANDLE;

    for (i = 0; i < MAX_OPERATION_NUM; i++) {
        if (session->op[i].live &&
            session->op[i].handle == handle) {
            *op = &session->op[i];
            ret = KM_ERROR_OK;
            break;
        }
    }
    if (ret) {
        LOG_E("%s: op %#016" PRIx64 " not found", __func__, handle);
    }
    return ret;
}

/**
 * Find a spare operation slot. This can't fail; or, to put it another way,
 * if there isn't a spare slot, we've done something wrong.
 */
static struct operation *find_spare_operation_slot(TEE_Session *session,
    keymaster_operation_handle_t handle)
{
    size_t i;
    struct operation *op = NULL;

    for (i = 0; i < MAX_OPERATION_NUM; i++) {
        if (!session->op[i].live) {
            op = &session->op[i];
            break;
        }
    }
    assert(op);
    op->live = true;
    op->handle = handle;
    session->live_ops++;
    LOG_D("%s: allocate op %#016" PRIx64 " to slot #%zu (%u ops live)",
        __func__, handle, i, session->live_ops);
    return op;
}

/**
 * Release an operation slot.  Do nothing if OP is null on entry.
 */
static void release_operation_slot(TEE_Session *session,
    struct operation *op)
{
    if (!op) return;
    assert(op->live);
    op->live = 0;
    session->live_ops--;
    LOG_D("%s: release op slot #%td (handle %#016" PRIx64 "; %u ops live)",
        __func__, op - session->op, op->handle, session->live_ops);
}

/**
 * Data copy to memory with scope lifetime.
 *
 * If either buf or size is equal to NULL/0 function
 * won't change state of data and it will return KM_ERROR_OK.
 */
static keymaster_error_t copy_to_scoped_buf(const uint8_t *buf, uint32_t size, scoped_buf_ptr_t& data)
{
    if (buf+size < buf)
        return KM_ERROR_MEMORY_ALLOCATION_FAILED;

    if ( (buf!=NULL) && (size!=0) ) {
        data.buf.reset(new (std::nothrow) uint8_t[size]);
        if(data.buf) {
            memcpy(data.buf.get(), buf, size);
            data.size = size;
            return KM_ERROR_OK;
        }
        return KM_ERROR_MEMORY_ALLOCATION_FAILED;
    }
    return KM_ERROR_OK;
}

/**
 * Data copy to memory with scope lifetime.
 *
 * If either buf1/buf2 or size1/size2 is equal to NULL/0 function
 * won't change state of data and it will return KM_ERROR_OK.
 */
static keymaster_error_t copy_to_scoped_buf2(
    const uint8_t *buf1,
    uint32_t size1,
    const uint8_t *buf2,
    uint32_t size2,
    scoped_buf_ptr_t& data)
{
    if ((buf1+size1 < buf1) || (buf2+size2 < buf2)) {
        return KM_ERROR_MEMORY_ALLOCATION_FAILED;
    }
    if (size1+size2 == 0) {
        return KM_ERROR_OK;
    }
    data.buf.reset(new (std::nothrow) uint8_t[size1+size2]);
    if (data.buf) {
        if (buf1 != NULL) {
            memcpy(data.buf.get(), buf1, size1);
        }
        if (buf2 != NULL) {
            memcpy(data.buf.get()+size1, buf2, size2);
        }
        data.size = size1+size2;
        return KM_ERROR_OK;
    }
    return KM_ERROR_MEMORY_ALLOCATION_FAILED;
}

/**
 * Data copy to memory with scope lifetime.
 *
 * If either buf1/buf2/buf3 or size1/size2/size3 is equal to NULL/0 function
 * won't change state of data and it will return KM_ERROR_OK.
 */
static keymaster_error_t copy_to_scoped_buf3(
    const uint8_t *buf1,
    uint32_t size1,
    const uint8_t *buf2,
    uint32_t size2,
    const uint8_t *buf3,
    uint32_t size3,
    scoped_buf_ptr_t& data)
{
    if ((buf1+size1 < buf1) || (buf2+size2 < buf2) || (buf3+size3 < buf3)) {
        return KM_ERROR_MEMORY_ALLOCATION_FAILED;
    }
    if (size1+size2+size3 == 0) {
        return KM_ERROR_OK;
    }
    data.buf.reset(new (std::nothrow) uint8_t[size1+size2+size3]);
    if (data.buf) {
        if (buf1 != NULL) {
            memcpy(data.buf.get(), buf1, size1);
        }
        if (buf2 != NULL) {
            memcpy(data.buf.get()+size1, buf2, size2);
        }
        if (buf3 != NULL) {
            memcpy(data.buf.get()+size1+size2, buf3, size3);
        }
        data.size = size1+size2+size3;
        return KM_ERROR_OK;
    }
    return KM_ERROR_MEMORY_ALLOCATION_FAILED;
}

int TEE_Open(TEE_SessionHandle *pSessionHandle)
{
    struct TEE_Session *session;
    mcResult_t     mcRet;
    int rc = 0;
    size_t i;

    /* Validate session handle */
    if (pSessionHandle == NULL) {
        LOG_E("%s: Invalid session handle", __func__);
        return -EINVAL;
    }

    /* ExySp: Check if secureOS is loaded or not */
    if (secure_os_init()) {
        LOG_E("%s: Failed to init secureOS", __func__);
        return KM_ERROR_SECURE_HW_COMMUNICATION_FAILED;
    }

    session = (struct TEE_Session *)malloc(sizeof(*session));
    if (session == NULL) {
        return -ENOMEM;
    }

    /* Initialize session handle data */
    memset(session, 0, sizeof(*session));

    /* Open MobiCore device */
    mcRet = mcOpenDevice(gDeviceId);
    if ((MC_DRV_OK != mcRet) &&
        (MC_DRV_ERR_DEVICE_ALREADY_OPEN != mcRet))
    {
        LOG_E("%s: mcOpenDevice() returned %d", __func__, mcRet);
        rc = -EBUSY;
        goto end_session;
    }

    /* Allocating WSM for TCI */
    mcRet = mcMallocWsm(gDeviceId, 0, sizeof(*session->pTci), (uint8_t **) &session->pTci, 0);
    if (MC_DRV_OK != mcRet) {
        LOG_E("%s: mcMallocWsm() returned %d", __func__, mcRet);
        rc = -ENOMEM;
        goto end_device;
    }

    /* Open session the TEE keymint trusted application */
    session->sessionHandle.deviceId = gDeviceId;
    mcRet = mcOpenSession(&session->sessionHandle,
                          &gUuid,
                          (uint8_t *) session->pTci,
                          (uint32_t) sizeof(tciMessage_t));
    if (MC_DRV_OK != mcRet) {
        LOG_E("%s: mcOpenSession() returned %d", __func__, mcRet);
        mcFreeWsm(gDeviceId, (uint8_t*)session->pTci);
        rc = -EBUSY;
        goto end_device;
    }
    *pSessionHandle = (TEE_SessionHandle)session;

    /* No operations in progress yet. */
    for (i = 0; i < MAX_OPERATION_NUM; i++)
        session->op[i].live = false;
    session->live_ops = 0;
    goto end;

end_device:
    mcRet = mcCloseDevice(gDeviceId);
    if (MC_DRV_OK != mcRet) {
        LOG_E("%s: mcCloseDevice() returned: %d", __func__, mcRet);
    }
end_session:
    free(session);
end:
    return rc;
}

static keymaster_error_t TEE_CloseSession(
    struct TEE_Session *session)
{
    LOG_D("TEE_CloseSession");

    keymaster_error_t ret = KM_ERROR_OK;
    tciMessage_ptr tci = session->pTci;

    tci->command.header.commandId = CMD_ID_TEE_CLOSE_SESSION;

    CHECK_RESULT_OK(transact(&session->sessionHandle, tci));

end:
    return ret;
}

void TEE_Close(TEE_SessionHandle sessionHandle)
{
    mcResult_t    mcRet;

    /* Validate session handle */
    if (sessionHandle == NULL) {
        LOG_E("%s: Invalid session handle", __func__);
        return;
    }
    struct TEE_Session *session = (struct TEE_Session *)sessionHandle;

    (void)TEE_CloseSession(session);

    /* Close session */
    mcRet = mcCloseSession(&session->sessionHandle);
    if (MC_DRV_OK != mcRet) {
        LOG_E("%s: mcCloseSession() returned %d", __func__, mcRet);
    }

    mcFreeWsm(gDeviceId, (uint8_t*)session->pTci);
    if (MC_DRV_OK != mcRet) {
        LOG_E("%s: mcFreeWsm() returned %d", __func__, mcRet);
    }

    /* Close MobiCore device */
    mcRet = mcCloseDevice(gDeviceId);
    if (MC_DRV_OK != mcRet) {
        LOG_E("%s: mcCloseDevice() returned %d", __func__, mcRet);
    }
    free(session);
}

keymaster_error_t TEE_Configure(
    TEE_SessionHandle               sessionHandle,
    const keymaster_key_param_set_t *params)
{
    LOG_D("TEE_Configure");
    PRINT_PARAM_SET(params);

    struct TEE_Session *session = (struct TEE_Session *)sessionHandle;
    keymaster_error_t ret = KM_ERROR_OK;
    mcSessionHandle_t *session_handle = &session->sessionHandle;
    tciMessage_ptr tci = session->pTci;
    mcBulkMap_t param_map = {0, 0};
    scoped_buf_ptr_t serialized_params;

    CHECK_RESULT_OK(km_serialize_params(serialized_params,
        params, 0, 0));
    CHECK_RESULT_OK(map_buffer(session_handle,
        serialized_params.buf.get(), serialized_params.size, &param_map));

    tci->command.header.commandId = CMD_ID_TEE_CONFIGURE;
    tci->configure.params.data = (uint32_t)param_map.sVirtualAddr;
    tci->configure.params.data_length = param_map.sVirtualLen;

    CHECK_RESULT_OK(transact(session_handle, tci));

end:
    unmap_buffer(session_handle, serialized_params.buf.get(), &param_map);
    return ret;
}

keymaster_error_t TEE_AddRngEntropy(
    TEE_SessionHandle sessionHandle,
    const uint8_t *data,
    uint32_t dataLength)
{
    LOG_D("TEE_AddRngEntropy");
    PRINT_BUFFER(data, dataLength);

    keymaster_error_t ret = KM_ERROR_OK;
    mcBulkMap_t dataInfo = {0, 0};
    struct TEE_Session *session = (struct TEE_Session *)sessionHandle;
    tciMessage_ptr tci = session->pTci;
    mcSessionHandle_t* session_handle = &session->sessionHandle;
    scoped_buf_ptr_t data1;

    if (dataLength == 0) {
        return KM_ERROR_OK;
    }
    CHECK_TRUE(KM_ERROR_INVALID_INPUT_LENGTH,
        dataLength <= 16384);

    /* Hack to ensure that non-writable memory can be mapped. */
    CHECK_RESULT_OK(copy_to_scoped_buf(data, dataLength, data1));

    /* Map data */
    CHECK_RESULT_OK( map_buffer(session_handle, data1.buf.get(), data1.size, &dataInfo) );

    /* Update TCI buffer */
    tci->command.header.commandId = CMD_ID_TEE_ADD_RNG_ENTROPY;
    tci->add_rng_entropy.rng_data.data = (uint32_t)dataInfo.sVirtualAddr;
    tci->add_rng_entropy.rng_data.data_length = dataInfo.sVirtualLen;

    CHECK_RESULT_OK( transact(session_handle, tci) );

end:
    unmap_buffer(session_handle, data1.buf.get(), &dataInfo);
    if (data1.buf) {
        memset(data1.buf.get(), 0, data1.size);
    }

    LOG_D("TEE_AddRngEntropy exiting with %d", ret);
    return ret;
}

/**
 * Check if we can create a key of a given size
 * @param algorithm key type
 * @param keysize key size in bits as expressed by KM_TAG_KEY_SIZE
 * @return whether key size is supported
 */
static bool key_size_supported(
    keymaster_algorithm_t algorithm,
    size_t keysize)
{
    switch (algorithm) {
        case KM_ALGORITHM_RSA:
            return ((keysize >= 256) &&
                    (keysize <= RSA_MAX_KEY_SIZE) &&
                    (keysize % 64 == 0));
        case KM_ALGORITHM_EC:
            return ((keysize == 192) ||
                    (keysize == 224) ||
                    (keysize == 256) ||
                    (keysize == 384) ||
                    (keysize == 521));
        case KM_ALGORITHM_AES:
            return ((keysize == 128) ||
                    (keysize == 192) ||
                    (keysize == 256));
        case KM_ALGORITHM_HMAC:
            return ((keysize >= 64) &&
                    (keysize <= HMAC_MAX_KEY_SIZE) &&
                    (keysize % 8 == 0));
        case KM_ALGORITHM_TRIPLE_DES:
            return (keysize == 168);
        default:
            return false;
    }
}

keymaster_error_t TEE_GenerateAndAttestKey(
    TEE_SessionHandle sessionHandle,
    const keymaster_key_param_set_t* params,
    const keymaster_key_blob_t* attest_key_blob,
    const keymaster_key_param_set_t* attest_params,
    const keymaster_blob_t* attest_issuer_blob,
    keymaster_key_blob_t* key_blob,
    keymaster_key_characteristics_t* characteristics,
    keymaster_cert_chain_t* cert_chain)
{
    LOG_D("TEE_GenerateAndAttestKey");
    PRINT_PARAM_SET(params);

    keymaster_error_t ret = KM_ERROR_OK;
    mcBulkMap_t paramsInfo = {0, 0};
    mcBulkMap_t keyBlobInfo = {0, 0};
    uint32_t encoded_key_size = 0;
    uint32_t keySizeInBits = 0;
    keymaster_ec_curve_t curve = KM_EC_CURVE_NONE;
    uint64_t rsa_pubexp = 0;
    keymaster_algorithm_t algorithm;
    scoped_buf_ptr_t serializedData;
    scoped_buf_ptr_t serializedParams;
    scoped_buf_ptr_t serializedAttestParams;
    struct TEE_Session *session = (struct TEE_Session *)sessionHandle;
    tciMessage_ptr tci = session->pTci;
    mcSessionHandle_t* session_handle = &session->sessionHandle;
    size_t max_charsz = 0;
    scoped_buf_ptr_t attest_key;
    mcBulkMap_t attestKeyBlobInfo = {0, 0};
    uint8_t* certs_and_chars = NULL;
    mcBulkMap_t certsAndCharsInfo = {0, 0};
    size_t certs_and_chars_size = 0;

    if (characteristics) {
        characteristics->hw_enforced = { NULL, 0 };
        characteristics->sw_enforced = { NULL, 0 };
    }
    CHECK_NOT_NULL(params);
    CHECK_NOT_NULL(key_blob);

    key_blob->key_material = NULL;

    /* Find algorithm, key size and RSA public exponent */
    CHECK_RESULT_OK( get_enumerated_tag(params,
        KM_TAG_ALGORITHM, reinterpret_cast<uint32_t*>(&algorithm)));
    if (KM_ERROR_OK != get_integer_tag(params, KM_TAG_KEY_SIZE, &keySizeInBits)) {
        if ((algorithm == KM_ALGORITHM_EC) &&
            (KM_ERROR_OK == get_enumerated_tag(params,
                KM_TAG_EC_CURVE, reinterpret_cast<uint32_t*>(&curve))))
        {
            keySizeInBits = ec_bitlen(curve);
        } else {
            CHECK_TRUE(KM_ERROR_UNSUPPORTED_KEY_SIZE, !"Unable to determine key size");
        }
    }
    CHECK_TRUE(KM_ERROR_UNSUPPORTED_KEY_SIZE,
        key_size_supported(algorithm, keySizeInBits));
    if (algorithm == KM_ALGORITHM_RSA) {
        if (KM_ERROR_OK == get_long_integer_tag(params,
            KM_TAG_RSA_PUBLIC_EXPONENT, &rsa_pubexp))
        {
            CHECK_TRUE(KM_ERROR_INVALID_ARGUMENT,
                (rsa_pubexp % 2 == 1) && (rsa_pubexp != 1));
        } else {
            rsa_pubexp = 65537; // default
        }
    }

    /* Serialize key parameters */
    CHECK_RESULT_OK(km_serialize_params(
        serializedParams, params, 0, rsa_pubexp));

    if (attest_params != NULL) {
        /* Serialize attestation key parameters */
        CHECK_RESULT_OK(km_serialize_params(
            serializedAttestParams, attest_params, 0, 0));
    }
    CHECK_RESULT_OK( copy_to_scoped_buf2(
        serializedParams.buf.get(), serializedParams.size,
        serializedAttestParams.buf.get(), serializedAttestParams.size,
        serializedData) );

    /* Map key generation parameters */
    CHECK_RESULT_OK( map_buffer(session_handle, serializedData.buf.get(), serializedData.size, &paramsInfo) );

    /* Allocate memory for key material */
    if (algorithm == KM_ALGORITHM_TRIPLE_DES) {
        CHECK_TRUE(KM_ERROR_UNKNOWN_ERROR, keySizeInBits == 168);
        encoded_key_size = 192;
    } else {
        encoded_key_size = keySizeInBits;
    }
    key_blob->key_material_size = key_blob_max_size(
        algorithm, encoded_key_size, serializedParams.size);
    CHECK_RESULT_OK(km_alloc((uint8_t**)&key_blob->key_material, key_blob->key_material_size));

    /* Map key blob buffer */
    CHECK_RESULT_OK( map_buffer(session_handle,
        key_blob->key_material, key_blob->key_material_size, &keyBlobInfo) );

    if (characteristics != NULL) {
        max_charsz = serializedParams.size + OWN_PARAMS_MAX_SIZE;
        certs_and_chars_size = max_charsz;
    }
    if (cert_chain != NULL) {
        certs_and_chars_size += KM_CERT_CHAIN_SIZE;
    }
    if (certs_and_chars_size != 0) {
        /* Allocate memory for the key characteristics and the serialized cert
           chain. */
        CHECK_RESULT_OK(km_alloc(&certs_and_chars, certs_and_chars_size));
        /* Map buffer for key characteristics and the serialized cert chain. */
        CHECK_RESULT_OK( map_buffer(session_handle,
            certs_and_chars, certs_and_chars_size, &certsAndCharsInfo) );
    }

    /* Map input const buffer attest_key_blob */
    if (attest_key_blob != NULL) {
        /* Hack to ensure that non-writable memory can be mapped. */
        CHECK_RESULT_OK( copy_to_scoped_buf2(
            attest_key_blob->key_material, attest_key_blob->key_material_size,
            (attest_issuer_blob!=NULL)?attest_issuer_blob->data:NULL,
            (attest_issuer_blob!=NULL)?attest_issuer_blob->data_length:0,
            attest_key) );
        CHECK_RESULT_OK( map_buffer(session_handle,
        attest_key.buf.get(), attest_key.size, &attestKeyBlobInfo) );
    }

    /* Update TCI buffer */
    tci->command.header.commandId = CMD_ID_TEE_GENERATE_KEY;
    tci->generate_and_attest_key.params.data = (uint32_t)paramsInfo.sVirtualAddr;
    tci->generate_and_attest_key.params.data_length = serializedParams.size;
    tci->generate_and_attest_key.key_blob.data = (uint32_t)keyBlobInfo.sVirtualAddr;
    tci->generate_and_attest_key.key_blob.data_length = key_blob->key_material_size;
    if (characteristics != NULL) {
        tci->generate_and_attest_key.characteristics.data = (uint32_t)certsAndCharsInfo.sVirtualAddr;
        tci->generate_and_attest_key.characteristics.data_length = max_charsz;
    } else {
        tci->generate_and_attest_key.characteristics.data = 0;
        tci->generate_and_attest_key.characteristics.data_length = 0;
    }
    if (attest_key_blob) {
        tci->generate_and_attest_key.attest_key_blob.data = (uint32_t)attestKeyBlobInfo.sVirtualAddr;
        tci->generate_and_attest_key.attest_key_blob.data_length = attest_key_blob->key_material_size;
    } else {
        tci->generate_and_attest_key.attest_key_blob.data = 0;
        tci->generate_and_attest_key.attest_key_blob.data_length = 0;
    }
    if (cert_chain != NULL) {
        tci->generate_and_attest_key.cert_chain.data = (uint32_t)certsAndCharsInfo.sVirtualAddr+max_charsz;
        tci->generate_and_attest_key.cert_chain.data_length = KM_CERT_CHAIN_SIZE;
    } else {
        tci->generate_and_attest_key.cert_chain.data = 0;
        tci->generate_and_attest_key.cert_chain.data_length = 0;
    }
    if ((attest_key_blob != NULL) && (attest_params != NULL)) {
        tci->generate_and_attest_key.attest_key_params.data = (uint32_t)paramsInfo.sVirtualAddr+serializedParams.size;
        tci->generate_and_attest_key.attest_key_params.data_length = serializedAttestParams.size;
    } else {
        tci->generate_and_attest_key.attest_key_params.data = 0;
        tci->generate_and_attest_key.attest_key_params.data_length = 0;
    }
    if ((attest_key_blob != NULL) && (attest_issuer_blob != NULL)) {
        tci->generate_and_attest_key.attest_issuer_blob.data = (uint32_t)attestKeyBlobInfo.sVirtualAddr+attest_key_blob->key_material_size;
        tci->generate_and_attest_key.attest_issuer_blob.data_length = attest_issuer_blob->data_length;
    } else {
        tci->generate_and_attest_key.attest_issuer_blob.data = 0;
        tci->generate_and_attest_key.attest_issuer_blob.data_length = 0;
    }

    CHECK_RESULT_OK( transact(session_handle, tci) );

    /* Update key blob length */
    key_blob->key_material_size = tci->generate_and_attest_key.key_blob.data_length;

    if (characteristics != NULL) { // Give characteristics to caller.
        CHECK_RESULT_OK(km_deserialize_characteristics(
            characteristics, certs_and_chars, max_charsz));
    }

    if (cert_chain != NULL) {
        /* Deserialize certs -> cert_chain, allocating memory for the latter. */
        CHECK_RESULT_OK(km_deserialize_attestation(
            cert_chain, certs_and_chars+max_charsz, KM_CERT_CHAIN_SIZE));
    }

end:
    unmap_buffer(session_handle, serializedData.buf.get(), &paramsInfo);
    if (key_blob != NULL) {
        unmap_buffer(session_handle, key_blob->key_material, &keyBlobInfo);
    }

    if (ret != KM_ERROR_OK) {
        if (key_blob != NULL) {
            free((void*)key_blob->key_material);
            key_blob->key_material = NULL;
            key_blob->key_material_size = 0;
        }
        if (characteristics != NULL) {
            keymaster_free_characteristics(characteristics);
        }
    }

    if (attest_key.buf) {
        unmap_buffer(session_handle,
            attest_key.buf.get(), &attestKeyBlobInfo);
    }
    if (certs_and_chars != NULL) {
        unmap_buffer(session_handle, certs_and_chars, &certsAndCharsInfo);
    }
    free(certs_and_chars);

    if (ret != KM_ERROR_OK) {
        keymaster_free_cert_chain(cert_chain);
    }

    LOG_D("TEE_GenerateAndAttestKey exiting with %d", ret);
    return ret;
}

keymaster_error_t TEE_GetKeyCharacteristics(
    TEE_SessionHandle                 sessionHandle,
    const keymaster_key_blob_t*       key_blob,
    const keymaster_blob_t*           client_id,
    const keymaster_blob_t*           app_data,
    keymaster_key_characteristics_t*  characteristics)
{
    LOG_D("TEE_GetKeyCharacteristics");

    keymaster_error_t ret = KM_ERROR_OK;
    uint8_t *key_chars = NULL;
    mcBulkMap_t keyBlobInfo = {0, 0};
    mcBulkMap_t clientIdInfo = {0, 0};
    mcBulkMap_t appDataInfo = {0, 0};
    mcBulkMap_t characteristicsInfo = {0, 0};
    struct TEE_Session *session = (struct TEE_Session *)sessionHandle;
    tciMessage_ptr tci = session->pTci;
    mcSessionHandle_t* session_handle = &session->sessionHandle;
    scoped_buf_ptr_t key_blob1;
    scoped_buf_ptr_t client_id1;
    scoped_buf_ptr_t app_data1;
    size_t max_charsz;

    CHECK_NOT_NULL(tci);
    CHECK_NOT_NULL(key_blob);
    CHECK_NOT_NULL(characteristics);

    /* Map input buffers */
    if ( key_blob != NULL ) {
        /* Hack to ensure that non-writable memory can be mapped. */
        CHECK_RESULT_OK( copy_to_scoped_buf(key_blob->key_material, key_blob->key_material_size, key_blob1) );
        CHECK_RESULT_OK( map_buffer(session_handle,
        key_blob1.buf.get(), key_blob->key_material_size, &keyBlobInfo) );
    }

    if ( client_id != NULL ) {
        /* Hack to ensure that non-writable memory can be mapped. */
        CHECK_RESULT_OK( copy_to_scoped_buf(client_id->data, client_id->data_length, client_id1));
        CHECK_RESULT_OK( map_buffer(session_handle,
            client_id1.buf.get(), client_id1.size, &clientIdInfo) );
    }
    if ( app_data != NULL ) {
        /* Hack to ensure that non-writable memory can be mapped. */
        CHECK_RESULT_OK( copy_to_scoped_buf(app_data->data, app_data->data_length, app_data1));
        CHECK_RESULT_OK( map_buffer(session_handle,
            app_data1.buf.get(), app_data1.size, &appDataInfo) );
    }

    /* Allocate memory for characteristics */
    max_charsz = key_blob->key_material_size;
    CHECK_RESULT_OK(km_alloc(&key_chars, max_charsz));

    /* Map buffer for serialized key characteristics */
    CHECK_RESULT_OK( map_buffer(session_handle,
        key_chars, max_charsz, &characteristicsInfo) );

    /* Now the get_key_characteristics command */
    tci->command.header.commandId = CMD_ID_TEE_GET_KEY_CHARACTERISTICS;
    tci->get_key_characteristics.key_blob.data = (uint32_t)keyBlobInfo.sVirtualAddr;
    tci->get_key_characteristics.key_blob.data_length = key_blob->key_material_size;
    tci->get_key_characteristics.client_id.data = (uint32_t)clientIdInfo.sVirtualAddr;
    tci->get_key_characteristics.client_id.data_length = clientIdInfo.sVirtualLen;
    tci->get_key_characteristics.app_data.data = (uint32_t)appDataInfo.sVirtualAddr;
    tci->get_key_characteristics.app_data.data_length = appDataInfo.sVirtualLen;
    tci->get_key_characteristics.characteristics.data = (uint32_t)characteristicsInfo.sVirtualAddr;
    tci->get_key_characteristics.characteristics.data_length = max_charsz;

    CHECK_RESULT_OK( transact(session_handle, tci) );

    /* Deserialize */
    CHECK_RESULT_OK(km_deserialize_characteristics(
        characteristics, key_chars, max_charsz));

end:
    if (key_blob1.buf) {
        unmap_buffer(session_handle, key_blob1.buf.get(), &keyBlobInfo);
    }
    if (client_id1.buf) {
        unmap_buffer(session_handle, client_id1.buf.get(), &clientIdInfo);
    }
    if (app_data1.buf) {
        unmap_buffer(session_handle, app_data1.buf.get(), &appDataInfo);
    }

    if (key_chars != NULL) {
        unmap_buffer(session_handle, key_chars, &characteristicsInfo);
    }

    free(key_chars);

    if (ret != KM_ERROR_OK) {
        if (characteristics != NULL) {
            keymaster_free_characteristics(characteristics);
        }
    }

    LOG_D("TEE_GetKeyCharacteristics exiting with %d", ret);
    return ret;
}

/**
 * Allocate and populate a buffer with encoded key data.
 *
 * @param format desired export format
 * @param key_type key type
 * @param key_size key size in bits
 * @param core_data key data
 * @param core_data_len length of \p core_data
 * @param[out] export_data encoded key data
 *
 * @return KM_ERROR_OK or error
 */
static keymaster_error_t encode_key(
    keymaster_key_format_t format,
    keymaster_algorithm_t key_type,
    uint32_t key_size,
    const uint8_t *core_data,
    uint32_t core_data_len,
    keymaster_blob_t *export_data)
{
    keymaster_error_t ret = KM_ERROR_OK;

    assert(export_data != NULL);

    export_data->data = NULL;
    export_data->data_length = 0;

    if (format == KM_KEY_FORMAT_RAW) {
        CHECK_TRUE(KM_ERROR_UNSUPPORTED_KEY_FORMAT,
            (key_type == KM_ALGORITHM_AES) ||
            (key_type == KM_ALGORITHM_EC));

        if (key_type == KM_ALGORITHM_EC) {
            CHECK_TRUE(KM_ERROR_INVALID_ARGUMENT,
                core_data_len >= CURVE25519_KEYSZ);
            export_data->data = (uint8_t*)malloc(CURVE25519_KEYSZ);
            CHECK_TRUE(KM_ERROR_MEMORY_ALLOCATION_FAILED,
                export_data->data != NULL);
            export_data->data_length = CURVE25519_KEYSZ;
            memcpy((void*)export_data->data, core_data, CURVE25519_KEYSZ);
        } else {
            export_data->data = (uint8_t*)malloc(core_data_len);
            CHECK_TRUE(KM_ERROR_MEMORY_ALLOCATION_FAILED,
                export_data->data != NULL);
            export_data->data_length = core_data_len;
            memcpy((void*)export_data->data, core_data, core_data_len);
        }
    } else {
        CHECK_TRUE(KM_ERROR_UNSUPPORTED_KEY_FORMAT,
            (key_type == KM_ALGORITHM_RSA) ||
            (key_type == KM_ALGORITHM_EC));

        CHECK_RESULT_OK(encode_x509(export_data, key_type, key_size,
            core_data, core_data_len));
    }
end:
    // no need to free export_data on error
    return ret;
}

keymaster_error_t TEE_ImportAndAttestKey(
    TEE_SessionHandle                   sessionHandle,
    const keymaster_key_param_set_t*    params,
    keymaster_key_format_t              key_format,
    const keymaster_blob_t*             key_data, // encoded
    const keymaster_key_blob_t*         attest_key_blob,
    const keymaster_key_param_set_t* attest_params,
    const keymaster_blob_t* attest_issuer_blob,
    keymaster_key_blob_t*               key_blob,
    keymaster_key_characteristics_t*    characteristics,
    keymaster_cert_chain_t*             cert_chain)
{
    LOG_D("TEE_ImportAndAttestKey");
    PRINT_PARAM_SET(params);
    LOG_D("key_format = 0x%08x", key_format);
    PRINT_BLOB(key_data);

    keymaster_error_t ret = KM_ERROR_OK;
    scoped_buf_ptr_t serializedData;
    scoped_buf_ptr_t serializedParams;
    scoped_buf_ptr_t serializedAttestParams;
    mcBulkMap_t paramsInfo = {0, 0};
    mcBulkMap_t keyDataInfo = {0, 0};
    mcBulkMap_t keyBlobInfo = {0, 0};
    uint32_t keySizeInBits = 0;
    uint64_t rsaPubExp = 0;
    uint32_t decoded_key_size = 0;
    uint32_t decoded_key_size_nominal = 0;
    uint64_t rsa_e = 0;
    keymaster_algorithm_t algorithm;
    keymaster_ec_curve_t curve = KM_EC_CURVE_NONE;
    bool x25519 = false;
    uint8_t *km_key_data = NULL; // to hold km_key_data passed to TA
    size_t km_key_data_len = 0;
    struct TEE_Session *session = (struct TEE_Session *)sessionHandle;
    tciMessage_ptr tci = session->pTci;
    mcSessionHandle_t *session_handle = &session->sessionHandle;
    size_t max_charsz = 0;
    uint8_t* certs_and_chars = NULL;
    mcBulkMap_t certsAndCharsInfo = {0, 0};
    size_t certs_and_chars_size = 0;
    size_t key_material_size = 0;

    CHECK_NOT_NULL(tci);
    CHECK_NOT_NULL(key_blob);

    key_blob->key_material = NULL;

    /* Extract algorithm and (if present) key size and RSA public exponent from
     * the key parameters */
    CHECK_RESULT_OK( get_enumerated_tag(params, KM_TAG_ALGORITHM,
        reinterpret_cast<uint32_t*>(&algorithm)) );
    if (KM_ERROR_OK != get_integer_tag(params, KM_TAG_KEY_SIZE, &keySizeInBits)) {
        if (algorithm == KM_ALGORITHM_EC) {
            if (KM_ERROR_OK == get_enumerated_tag(params,
                KM_TAG_EC_CURVE, reinterpret_cast<uint32_t*>(&curve)))
            {
                keySizeInBits = ec_bitlen(curve);
            }
        }
    }
    if (algorithm == KM_ALGORITHM_EC) {
            if (KM_ERROR_OK == get_enumerated_tag(params, KM_TAG_EC_CURVE,
		                                      reinterpret_cast<uint32_t*>(&curve))) {
		    /* Ed25519 or X25519 key? */
		    if ((curve == KM_EC_CURVE_25519) &&
		        test_enumerated_tag_data(params, KM_TAG_PURPOSE, KM_PURPOSE_AGREE_KEY)) {
		        x25519 = true;
		    }
		}
    }
    CHECK_RESULT_OK(decode_key_data(&km_key_data, &km_key_data_len,
        key_format, algorithm, curve, x25519, key_data->data, key_data->data_length));

    CHECK_TRUE(KM_ERROR_UNKNOWN_ERROR, km_key_data_len >= 8);
    decoded_key_size = get_u32(km_key_data + 4);
    if (algorithm == KM_ALGORITHM_TRIPLE_DES) {
        CHECK_TRUE(KM_ERROR_UNSUPPORTED_KEY_SIZE, decoded_key_size == 192);
        decoded_key_size_nominal = 168;
    } else {
        decoded_key_size_nominal = decoded_key_size;
    }
    if (keySizeInBits == 0) {
        keySizeInBits = decoded_key_size_nominal;
    } else {
        CHECK_TRUE(KM_ERROR_IMPORT_PARAMETER_MISMATCH,
            keySizeInBits == decoded_key_size_nominal);
    }
    CHECK_TRUE(KM_ERROR_UNSUPPORTED_KEY_SIZE,
        key_size_supported(algorithm, keySizeInBits));

    if (algorithm == KM_ALGORITHM_RSA) {
        CHECK_TRUE(KM_ERROR_UNKNOWN_ERROR, km_key_data_len >= 20);
        uint32_t n_len = get_u32(km_key_data + 12);
        uint32_t e_len = get_u32(km_key_data + 16);
        CHECK_TRUE(KM_ERROR_INVALID_ARGUMENT, e_len <= 8);
        CHECK_TRUE(KM_ERROR_UNKNOWN_ERROR, km_key_data_len >= 44 + n_len + e_len);
        for (uint8_t i = 0; i < e_len; i++) {
            rsa_e += (uint64_t)km_key_data[44 + n_len + i] << (8*(e_len - 1 - i));
        }
        if (KM_ERROR_OK == get_long_integer_tag(params, KM_TAG_RSA_PUBLIC_EXPONENT, &rsaPubExp)) {
            CHECK_TRUE(KM_ERROR_IMPORT_PARAMETER_MISMATCH,
                rsa_e == rsaPubExp);
        }
    }

    /* Serialize key parameters */
    CHECK_RESULT_OK(km_serialize_params(
        serializedParams, params, keySizeInBits, rsa_e));

    if (attest_params != NULL) {
        /* Serialize attestation key parameters */
        CHECK_RESULT_OK(km_serialize_params(
            serializedAttestParams, attest_params, 0, 0));
    }
    CHECK_RESULT_OK( copy_to_scoped_buf2(
        serializedParams.buf.get(), serializedParams.size,
        serializedAttestParams.buf.get(), serializedAttestParams.size,
        serializedData) );

    /* Map key parameters */
    CHECK_RESULT_OK( map_buffer(session_handle,
        serializedData.buf.get(), serializedData.size, &paramsInfo) );

    /* Allocate memory for key blob */
    key_blob->key_material_size = key_blob_max_size(
        algorithm, decoded_key_size, serializedParams.size);

    /* The same buffer will be used as input for the attestation keyblob and
       output for the imported keyblob because we are limited by the number of
       buffers that can be mapped */
    key_material_size = key_blob->key_material_size;
    if ( attest_key_blob != NULL ) {
        key_material_size += attest_key_blob->key_material_size;
        if (attest_issuer_blob != NULL) {
            key_material_size += attest_issuer_blob->data_length;
        }
    }

    CHECK_RESULT_OK(km_alloc((uint8_t**)&key_blob->key_material, key_material_size));

    /* Map key data */
    CHECK_RESULT_OK( map_buffer(session_handle,
        km_key_data, km_key_data_len, &keyDataInfo) );

    /* Map key blob buffer */
    CHECK_RESULT_OK( map_buffer(session_handle,
        key_blob->key_material, key_material_size, &keyBlobInfo) );

    if (attest_key_blob != NULL) {
        memcpy((void*)(key_blob->key_material+key_blob->key_material_size),
            attest_key_blob->key_material, attest_key_blob->key_material_size);
        if (attest_issuer_blob != NULL) {
            memcpy((void*)(key_blob->key_material+
                   key_blob->key_material_size+
                   attest_key_blob->key_material_size),
                attest_issuer_blob->data, attest_issuer_blob->data_length);
        }
    }

    if (characteristics != NULL) {
        max_charsz = serializedParams.size + OWN_PARAMS_MAX_SIZE;
        certs_and_chars_size = max_charsz;
    }
    if (cert_chain != NULL) {
        certs_and_chars_size += KM_CERT_CHAIN_SIZE;
    }
    if (certs_and_chars_size != 0) {
        /* Allocate memory for the key characteristics and the serialized cert
           chain. */
        CHECK_RESULT_OK(km_alloc(&certs_and_chars, certs_and_chars_size));
        /* Map buffer for key characteristics and the serialized cert chain. */
        CHECK_RESULT_OK( map_buffer(session_handle,
            certs_and_chars, certs_and_chars_size, &certsAndCharsInfo) );
    }

    /* Update TCI buffer */
    tci->command.header.commandId = CMD_ID_TEE_IMPORT_KEY;
    tci->import_and_attest_key.params.data = (uint32_t)paramsInfo.sVirtualAddr;
    tci->import_and_attest_key.params.data_length = serializedParams.size;
    tci->import_and_attest_key.key_data.data = (uint32_t)keyDataInfo.sVirtualAddr;
    tci->import_and_attest_key.key_data.data_length = km_key_data_len;
    tci->import_and_attest_key.key_blob.data = (uint32_t)keyBlobInfo.sVirtualAddr;
    tci->import_and_attest_key.key_blob.data_length = key_blob->key_material_size;
    if (characteristics != NULL) {
        tci->import_and_attest_key.characteristics.data = (uint32_t)certsAndCharsInfo.sVirtualAddr;
        tci->import_and_attest_key.characteristics.data_length = max_charsz;
    } else {
        tci->import_and_attest_key.characteristics.data = 0;
        tci->import_and_attest_key.characteristics.data_length = 0;
    }
    if (attest_key_blob != NULL) {
        tci->import_and_attest_key.attest_key_blob.data = (uint32_t)keyBlobInfo.sVirtualAddr+key_blob->key_material_size;
        tci->import_and_attest_key.attest_key_blob.data_length = attest_key_blob->key_material_size;
    } else {
        tci->import_and_attest_key.attest_key_blob.data = 0;
        tci->import_and_attest_key.attest_key_blob.data_length = 0;
    }
    if (cert_chain != NULL) {
        tci->import_and_attest_key.cert_chain.data = (uint32_t)certsAndCharsInfo.sVirtualAddr+max_charsz;
        tci->import_and_attest_key.cert_chain.data_length = KM_CERT_CHAIN_SIZE;
    } else {
        tci->import_and_attest_key.cert_chain.data = 0;
        tci->import_and_attest_key.cert_chain.data_length = 0;
    }
    if ((attest_key_blob != NULL) && (attest_params != NULL)) {
        tci->import_and_attest_key.attest_key_params.data = (uint32_t)paramsInfo.sVirtualAddr+serializedParams.size;
        tci->import_and_attest_key.attest_key_params.data_length = serializedAttestParams.size;
    } else {
        tci->import_and_attest_key.attest_key_params.data = 0;
        tci->import_and_attest_key.attest_key_params.data_length = 0;
    }
    if ((attest_key_blob != NULL) && (attest_issuer_blob != NULL)) {
        tci->import_and_attest_key.attest_issuer_blob.data = (uint32_t)keyBlobInfo.sVirtualAddr+key_blob->key_material_size+attest_key_blob->key_material_size;
        tci->import_and_attest_key.attest_issuer_blob.data_length = attest_issuer_blob->data_length;
    } else {
        tci->import_and_attest_key.attest_issuer_blob.data = 0;
        tci->import_and_attest_key.attest_issuer_blob.data_length = 0;
    }

    CHECK_RESULT_OK( transact(session_handle, tci) );

    /* Update key blob length */
    key_blob->key_material_size = tci->import_and_attest_key.key_blob.data_length;

    if (characteristics != NULL) { // Give characteristics to caller.
        CHECK_RESULT_OK(km_deserialize_characteristics(
            characteristics, certs_and_chars, max_charsz));
    }

    if (cert_chain != NULL) {
        /* Deserialize certs -> cert_chain, allocating memory for the latter. */
        CHECK_RESULT_OK(km_deserialize_attestation(
            cert_chain, certs_and_chars+max_charsz, KM_CERT_CHAIN_SIZE));
    }

end:
    unmap_buffer(session_handle, serializedData.buf.get(), &paramsInfo);
    unmap_buffer(session_handle, km_key_data, &keyDataInfo);
    if (key_blob != NULL) {
        unmap_buffer(session_handle, (uint8_t*)key_blob->key_material, &keyBlobInfo);
    }
    free(km_key_data);

    if (ret != KM_ERROR_OK) {
        if (key_blob != NULL) {
            free((void*)key_blob->key_material);
            key_blob->key_material = NULL;
            key_blob->key_material_size = 0;
        }
        if (characteristics != NULL) {
            keymaster_free_characteristics(characteristics);
        }
    } else {
        /* This realloc can't fail as the buffer is always shrank */
        key_blob->key_material = (const uint8_t*)realloc(
                                    (void*)key_blob->key_material,
                                    key_blob->key_material_size);
    }

    if (certs_and_chars != NULL) {
        unmap_buffer(session_handle, certs_and_chars, &certsAndCharsInfo);
    }
    free(certs_and_chars);

    if (ret != KM_ERROR_OK) {
        keymaster_free_cert_chain(cert_chain);
    }

    LOG_D("TEE_ImportAndAttestKey exiting with %d", ret);
    return ret;
}

keymaster_error_t TEE_ExportKey(
    TEE_SessionHandle                   sessionHandle,
    keymaster_key_format_t              export_format,
    const keymaster_key_blob_t*         key_to_export,
    const keymaster_blob_t*             client_id,
    const keymaster_blob_t*             app_data,
    keymaster_blob_t*                   export_data)
{
    LOG_D("TEE_ExportKey");
    LOG_D("export_format = 0x%08x", export_format);

    keymaster_error_t ret = KM_ERROR_OK;
    mcBulkMap_t keyBlobInfo = {0, 0};
    mcBulkMap_t clientIdInfo = {0, 0};
    mcBulkMap_t appDataInfo = {0, 0};
    mcBulkMap_t keyDataInfo = {0, 0};
    struct TEE_Session *session = (struct TEE_Session *)sessionHandle;
    tciMessage_ptr     tci = session->pTci;
    mcSessionHandle_t* session_handle = &session->sessionHandle;
    uint8_t *core_data = NULL;
    uint32_t core_data_len = 0;
    keymaster_algorithm_t key_type;
    uint32_t key_size;
    scoped_buf_ptr_t key_to_export1;
    scoped_buf_ptr_t client_id1;
    scoped_buf_ptr_t app_data1;

    CHECK_NOT_NULL(tci);
    CHECK_NOT_NULL(key_to_export);
    CHECK_NOT_NULL(export_data);

    export_data->data = NULL;

    /* Map input buffers */
    if ( key_to_export != NULL ) {
        /* Hack to ensure that non-writable memory can be mapped. */
        CHECK_RESULT_OK( copy_to_scoped_buf(key_to_export->key_material, key_to_export->key_material_size, key_to_export1) );
        CHECK_RESULT_OK( map_buffer(session_handle,
        key_to_export1.buf.get(), key_to_export->key_material_size, &keyBlobInfo) );
    }

    if ( client_id != NULL ) {
        /* Hack to ensure that non-writable memory can be mapped. */
        CHECK_RESULT_OK( copy_to_scoped_buf(client_id->data, client_id->data_length, client_id1));
        CHECK_RESULT_OK( map_buffer(session_handle,
            client_id1.buf.get(), client_id1.size, &clientIdInfo) );
    }
    if ( app_data != NULL ) {
        /* Hack to ensure that non-writable memory can be mapped. */
        CHECK_RESULT_OK( copy_to_scoped_buf(app_data->data, app_data->data_length, app_data1));
        CHECK_RESULT_OK( map_buffer(session_handle,
            app_data1.buf.get(), app_data1.size, &appDataInfo) );
    }

    /* First need to determine required length of key data. */
    tci->command.header.commandId = CMD_ID_TEE_GET_KEY_INFO;
    tci->get_key_info.key_blob.data = (uint32_t)keyBlobInfo.sVirtualAddr;
    tci->get_key_info.key_blob.data_length = key_to_export->key_material_size;
    tci->get_key_info.client_id.data = (uint32_t)clientIdInfo.sVirtualAddr;
    tci->get_key_info.client_id.data_length = clientIdInfo.sVirtualLen;
    tci->get_key_info.app_data.data = (uint32_t)appDataInfo.sVirtualAddr;
    tci->get_key_info.app_data.data_length = appDataInfo.sVirtualLen;
    CHECK_RESULT_OK( transact(session_handle, tci) );
    key_type = (keymaster_algorithm_t)tci->get_key_info.key_type;
    key_size = tci->get_key_info.key_size;

    core_data_len = export_data_length(key_type, key_size);
    CHECK_TRUE(KM_ERROR_UNSUPPORTED_KEY_FORMAT,
        core_data_len > 0);

    /* Allocate memory for exported key data */
    CHECK_RESULT_OK(km_alloc(&core_data, core_data_len));

    /* Map buffer */
    CHECK_RESULT_OK( map_buffer(session_handle, core_data, core_data_len, &keyDataInfo) );

    /* Now the export_key command */
    tci->command.header.commandId = CMD_ID_TEE_EXPORT_KEY;
    tci->export_key.key_blob.data = (uint32_t)keyBlobInfo.sVirtualAddr;
    tci->export_key.key_blob.data_length = key_to_export->key_material_size;
    tci->export_key.client_id.data = (uint32_t)clientIdInfo.sVirtualAddr;
    tci->export_key.client_id.data_length = clientIdInfo.sVirtualLen;
    tci->export_key.app_data.data = (uint32_t)appDataInfo.sVirtualAddr;
    tci->export_key.app_data.data_length = appDataInfo.sVirtualLen;
    tci->export_key.key_data.data = (uint32_t)keyDataInfo.sVirtualAddr;
    tci->export_key.key_data.data_length = core_data_len;
    CHECK_RESULT_OK( transact(session_handle, tci) );

    /* Allocate and fill buffer for encoded key data for passing to caller */
    CHECK_RESULT_OK(encode_key(export_format, key_type, key_size,
        core_data, core_data_len, export_data));

end:
    if (key_to_export1.buf)
    {
        unmap_buffer(session_handle, key_to_export1.buf.get(), &keyBlobInfo);
    }
    if (client_id1.buf) {
        unmap_buffer(session_handle, client_id1.buf.get(), &clientIdInfo);
    }
    if (app_data1.buf) {
        unmap_buffer(session_handle, app_data1.buf.get(), &appDataInfo);
    }
    unmap_buffer(session_handle, (uint8_t*)core_data, &keyDataInfo);

    if (ret != KM_ERROR_OK) {
        if (export_data != NULL) {
            free((void*)export_data->data);
            export_data->data = NULL;
            export_data->data_length = 0;
        }
    }

    free(core_data);

    LOG_D("TEE_ExportKey exiting with %d", ret);
    return ret;
}

#define UPGRADE_OVERHEAD (4*(4 + 4))
/* Space for the additional parameters which `upgrade_key' might insert into the
 * blob.  Four parameters, each with 32-bit tag and value.
 */

keymaster_error_t TEE_UpgradeKey(
    TEE_SessionHandle sessionHandle,
    const keymaster_key_blob_t* key_to_upgrade,
    const keymaster_key_param_set_t* upgrade_params,
    keymaster_key_blob_t* upgraded_key)
{
    keymaster_error_t ret = KM_ERROR_OK;
    struct TEE_Session *session = (struct TEE_Session *)sessionHandle;
    mcSessionHandle_t* session_handle = &session->sessionHandle;
    tciMessage_ptr tci = session->pTci;
    mcBulkMap_t keyin_map = { 0, 0 }, keyout_map = { 0, 0 };
    mcBulkMap_t param_map = { 0, 0 };
    scoped_buf_ptr_t serialized_params;
    scoped_buf_ptr_t key_to_upgrade1;
    uint8_t *kbuf = NULL;
    size_t kbufsz;
    size_t newblobsz = 0;

    /* Picky checking on input pointers. */
    CHECK_NOT_NULL(key_to_upgrade);
    CHECK_NOT_NULL(upgrade_params);
    CHECK_NOT_NULL(upgraded_key);

    /* Map input const buffer key_to_upgrade */
    if ( key_to_upgrade != NULL ) {
        /* Hack to ensure that non-writable memory can be mapped. */
        CHECK_RESULT_OK( copy_to_scoped_buf(key_to_upgrade->key_material, key_to_upgrade->key_material_size, key_to_upgrade1) );
        CHECK_RESULT_OK( map_buffer(session_handle,
        key_to_upgrade1.buf.get(), key_to_upgrade->key_material_size, &keyin_map) );
    }

    CHECK_RESULT_OK(km_serialize_params(serialized_params,
        upgrade_params, 0, 0));
    CHECK_RESULT_OK(map_buffer(session_handle,
        serialized_params.buf.get(), serialized_params.size, &param_map));

    /* Create and map output buffer -- which means we have to guess how big to
     * make it.  If this is a really old key, it won't have the version
     * parameters at all, so it'll come out UPGRADE_OVERHEAD bytes bigger than
     * the one we fed in.  Otherwise, it should be the same length.  Of course,
     * we can't tell which from here...
     */
    kbufsz = key_to_upgrade->key_material_size + UPGRADE_OVERHEAD;
    CHECK_RESULT_OK(km_alloc(&kbuf, kbufsz));
    CHECK_RESULT_OK(map_buffer(session_handle, kbuf, kbufsz, &keyout_map));

    /* Build the command. */
    tci->command.header.commandId = CMD_ID_TEE_UPGRADE_KEY;
    tci->upgrade_key.key_to_upgrade.data = (uint32_t)keyin_map.sVirtualAddr;
    tci->upgrade_key.key_to_upgrade.data_length =
        (uint32_t)keyin_map.sVirtualLen;
    tci->upgrade_key.upgrade_params.data = (uint32_t)param_map.sVirtualAddr;
    tci->upgrade_key.upgrade_params.data_length =
        (uint32_t)param_map.sVirtualLen;
    tci->upgrade_key.upgraded_key.data = (uint32_t)keyout_map.sVirtualAddr;
    tci->upgrade_key.upgraded_key.data_length =
        (uint32_t)keyout_map.sVirtualLen;

    /* See if it worked. */
    CHECK_RESULT_OK(transact(session_handle, tci));

    /* Return the new blob, or NULL if upgrade wasn't needed after all. */
    newblobsz = tci->upgrade_key.upgraded_key.data_length;
    upgraded_key->key_material = newblobsz ? kbuf : NULL;
    upgraded_key->key_material_size = newblobsz;

end:
    if (key_to_upgrade1.buf) {
        unmap_buffer(session_handle, key_to_upgrade1.buf.get(), &keyin_map);
    }

    unmap_buffer(session_handle, serialized_params.buf.get(), &param_map);
    unmap_buffer(session_handle, kbuf, &keyout_map);
    if ((ret != KM_ERROR_OK) || !newblobsz) free(kbuf);
    return ret;
}

keymaster_error_t TEE_DeleteKey(
    TEE_SessionHandle sessionHandle,
    const keymaster_key_blob_t* key_to_delete)
{
    keymaster_error_t ret = KM_ERROR_OK;
    struct TEE_Session *session = (struct TEE_Session *)sessionHandle;
    mcSessionHandle_t* session_handle = &session->sessionHandle;
    tciMessage_ptr tci = session->pTci;
    mcBulkMap_t keyin_map = { 0, 0 };
    scoped_buf_ptr_t key_to_delete1;

    LOG_D("%s %d", __func__, __LINE__);

    /* Picky checking on input pointers. */
    CHECK_NOT_NULL(key_to_delete);

    /* Hack to ensure that non-writable memory can be mapped. */
    CHECK_RESULT_OK(copy_to_scoped_buf(key_to_delete->key_material,
                                       key_to_delete->key_material_size,
                                       key_to_delete1));
    CHECK_RESULT_OK(map_buffer(session_handle,
    key_to_delete1.buf.get(), key_to_delete->key_material_size, &keyin_map) );

    /* Build the command. */
    tci->command.header.commandId = CMD_ID_TEE_DELETE_KEY;
    tci->delete_key.key_to_delete.data = (uint32_t)keyin_map.sVirtualAddr;
    tci->delete_key.key_to_delete.data_length = (uint32_t)keyin_map.sVirtualLen;

    /* See if it worked. */
    CHECK_RESULT_OK(transact(session_handle, tci));

end:
    if (key_to_delete1.buf) {
        unmap_buffer(session_handle, key_to_delete1.buf.get(), &keyin_map);
    }
    return ret;
}

keymaster_error_t TEE_DeleteAllKeys(
    TEE_SessionHandle sessionHandle)
{
    keymaster_error_t ret = KM_ERROR_OK;
    struct TEE_Session *session = (struct TEE_Session *)sessionHandle;
    mcSessionHandle_t* session_handle = &session->sessionHandle;
    tciMessage_ptr tci = session->pTci;

    LOG_D("%s %d", __func__, __LINE__);

    /* Build the command. */
    tci->command.header.commandId = CMD_ID_TEE_DELETE_ALL_KEYS;

    /* See if it worked. */
    CHECK_RESULT_OK(transact(session_handle, tci));

end:
    return ret;
}

keymaster_error_t TEE_Begin(
    TEE_SessionHandle               sessionHandle,
    keymaster_purpose_t             purpose,
    const keymaster_key_blob_t*     key,
    const keymaster_key_param_set_t* params,
    const keymaster_hw_auth_token_t* auth_token,
    keymaster_key_param_set_t*      out_params,
    keymaster_operation_handle_t*   operation_handle)
{
    PRINT_PARAM_SET(params);

    keymaster_error_t  ret = KM_ERROR_OK;
    mcBulkMap_t        paramsInfo = {0, 0};
    mcBulkMap_t        keyBlobInfo = {0, 0};
    mcBulkMap_t        outParamsInfo = {0, 0};
    scoped_buf_ptr_t   serializedData;
    struct TEE_Session *session = (struct TEE_Session *)sessionHandle;
    tciMessage_ptr     tci = session->pTci;
    mcSessionHandle_t* session_handle = &session->sessionHandle;
    uint8_t            serialized_out_params[TEE_BEGIN_OUT_PARAMS_SIZE];
    struct operation   *op = NULL;
    /* create key_blob buffer to copy input key content */
    scoped_buf_ptr_t   key_blob;

    /* Make valgrind happy. */
    memset(serialized_out_params, 0, TEE_BEGIN_OUT_PARAMS_SIZE);

    if (out_params != NULL) {
        out_params->params = NULL;
        out_params->length = 0;
    }

    CHECK_NOT_NULL(tci);
    CHECK_NOT_NULL(key);
    CHECK_NOT_NULL(operation_handle);

    /* Serialize params */
    CHECK_RESULT_OK(km_serialize_params(
        serializedData, params, 0, 0));

    /* Map params */
    CHECK_RESULT_OK( map_buffer(session_handle,
        serializedData.buf.get(), serializedData.size, &paramsInfo) );

    if ( key->key_material != NULL ) {
        /* Hack to ensure that non-writable memory can be mapped. */
        CHECK_RESULT_OK( copy_to_scoped_buf(key->key_material, key->key_material_size, key_blob) );
        CHECK_RESULT_OK( map_buffer(session_handle,
            key_blob.buf.get(), key_blob.size, &keyBlobInfo) );
    }

    /* Map serialized_out_params */
    CHECK_RESULT_OK( map_buffer(session_handle,
        serialized_out_params, TEE_BEGIN_OUT_PARAMS_SIZE, &outParamsInfo) );

    /* Update TCI buffer */
    tci->command.header.commandId = CMD_ID_TEE_BEGIN;
    tci->begin.purpose = purpose;
    tci->begin.params.data = (uint32_t)paramsInfo.sVirtualAddr;
    tci->begin.params.data_length = serializedData.size;
    tci->begin.key_blob.data = (uint32_t)keyBlobInfo.sVirtualAddr;
    tci->begin.key_blob.data_length = key->key_material_size;
    if (out_params != NULL) {
        tci->begin.out_params.data = (uint32_t)outParamsInfo.sVirtualAddr;
        tci->begin.out_params.data_length = TEE_BEGIN_OUT_PARAMS_SIZE;
    } else {
        tci->begin.out_params.data = 0;
        tci->begin.out_params.data_length = 0;
    }
    tci->begin.auth_challenge = auth_token ? auth_token->challenge : 0;
    tci->begin.auth_user_id = auth_token ? auth_token->user_id : 0;
    tci->begin.auth_authenticator_id = auth_token ? auth_token->authenticator_id :  0;
    tci->begin.auth_authenticator_type = auth_token ? auth_token->authenticator_type : KM_HW_AUTH_TYPE_NONE;
    tci->begin.auth_timestamp = auth_token ? auth_token->timestamp : 0;
    memset(tci->begin.auth_mac, 0, 32);
    if (auth_token && auth_token->mac.data_length) {
        CHECK_TRUE(KM_ERROR_INVALID_ARGUMENT,
            auth_token->mac.data_length == 32);
        memcpy(tci->begin.auth_mac, auth_token->mac.data, 32);
    }

    CHECK_RESULT_OK( transact(session_handle, tci) );

    /* Deserialize out_params */
    if (out_params != NULL) {
        uint8_t *pos = serialized_out_params;
        uint32_t remain = tci->begin.out_params.data_length;
        if (remain > 0) {
            CHECK_RESULT_OK(deserialize_param_set(out_params, &pos, &remain));
        } else {
            out_params->params = NULL;
            out_params->length = 0;
        }
    }

    /* Update operation handle */
    op = find_spare_operation_slot(session, tci->begin.handle);
    op->algorithm = static_cast<keymaster_algorithm_t>(tci->begin.algorithm);
    op->final_length = tci->begin.final_length;
    *operation_handle = op->handle;

end:
    if (ret != KM_ERROR_OK) {
        if (out_params != NULL) {
            keymaster_free_param_set(out_params);
            out_params->length = 0;
        }
    }
    unmap_buffer(session_handle, serializedData.buf.get(), &paramsInfo);

    /* unmap key_blob */
    if (key_blob.buf) {
        unmap_buffer(session_handle, key_blob.buf.get(), &keyBlobInfo);
    }

    unmap_buffer(session_handle, serialized_out_params, &outParamsInfo);

    LOG_D("TEE_Begin exiting with %d", ret);
    return ret;
}

/**
 * Maximum size of buffer to process internally in an update() operation.
 *
 * Longer messages are split up into chunks this size.
 */
#define INPUT_CHUNK_SIZE 4096*4

/**
 * Process a chunk of input to an operation.
 *
 * @param session session pointer
 * @param operation_handle operation handle
 * @param data input data (not NULL)
 * @param data_length length of \p data
 * @param[out] output output if required (pre-allocated), or NULL
 * @param[in,out] input_consumed input consumed (incremented)
 *
 * @return KM_ERROR_OK or error
 */
static keymaster_error_t update_chunk(
    struct TEE_Session *session, const struct operation *op,
    const mcBulkMap_t *input_map, size_t *input_consumed_r,
    const mcBulkMap_t *output_map, size_t *output_used_r,
    const keymaster_hw_auth_token_t *auth_token,
    void *)
{
    tciMessage_ptr tci = session->pTci;
    mcSessionHandle_t *session_handle = &session->sessionHandle;
    keymaster_error_t ret = KM_ERROR_OK;

    tci->command.header.commandId = CMD_ID_TEE_UPDATE;
    tci->update.handle = op->handle;
    tci->update.input.data = (uint32_t)input_map->sVirtualAddr;
    tci->update.input.data_length = input_map->sVirtualLen;
    tci->update.output.data = (uint32_t)output_map->sVirtualAddr;
    tci->update.output.data_length = output_map->sVirtualLen;
    tci->update.auth_challenge = auth_token ? auth_token->challenge : 0;
    tci->update.auth_user_id = auth_token ? auth_token->user_id : 0;
    tci->update.auth_authenticator_id = auth_token ? auth_token->authenticator_id :  0;
    tci->update.auth_authenticator_type = auth_token ? auth_token->authenticator_type : KM_HW_AUTH_TYPE_NONE;
    tci->update.auth_timestamp = auth_token ? auth_token->timestamp : 0;
    memset(tci->update.auth_mac, 0, 32);
    if (auth_token && auth_token->mac.data_length) {
        CHECK_TRUE(KM_ERROR_INVALID_ARGUMENT,
            auth_token->mac.data_length == 32);
        memcpy(tci->update.auth_mac, auth_token->mac.data, 32);
    }

    CHECK_RESULT_OK( transact(session_handle, tci) );

    *input_consumed_r = tci->update.input_consumed;
    *output_used_r = tci->update.output.data_length;
end:
    return ret;
}

struct finish_chunk_ctx {
    mcBulkMap_t signature_map;
};

/**
 * Process a chunk of input to an operation.
 *
 * @param session session pointer
 * @param operation_handle operation handle
 * @param data input data (not NULL)
 * @param data_length length of \p data
 * @param[out] output output if required (pre-allocated), or NULL
 * @param[in,out] input_consumed input consumed (incremented)
 * @param ctx pointer to @c finish_chunk_ctx structure
 *
 * @return KM_ERROR_OK or error
 */
static keymaster_error_t final_chunk(
    struct TEE_Session *session, const struct operation *op,
    const mcBulkMap_t *input_map, size_t *input_consumed_r,
    const mcBulkMap_t *output_map, size_t *output_used_r,
    const keymaster_hw_auth_token_t *auth_token,
    void *ctx)
{
    tciMessage_ptr tci = session->pTci;
    mcSessionHandle_t *session_handle = &session->sessionHandle;
    struct finish_chunk_ctx *fin =
        static_cast<struct finish_chunk_ctx *>(ctx);
    keymaster_error_t ret = KM_ERROR_OK;

    tci->command.header.commandId = CMD_ID_TEE_FINISH;
    tci->finish.handle = op->handle;
    tci->finish.input.data = (uint32_t)input_map->sVirtualAddr;
    tci->finish.input.data_length = input_map->sVirtualLen;
    tci->finish.signature.data = (uint32_t)fin->signature_map.sVirtualAddr;
    tci->finish.signature.data_length = fin->signature_map.sVirtualLen;
    tci->finish.output.data = (uint32_t)output_map->sVirtualAddr;
    tci->finish.output.data_length = output_map->sVirtualLen;
    tci->finish.auth_challenge = auth_token ? auth_token->challenge : 0;
    tci->finish.auth_user_id = auth_token ? auth_token->user_id : 0;
    tci->finish.auth_authenticator_id = auth_token ? auth_token->authenticator_id :  0;
    tci->finish.auth_authenticator_type = auth_token ? auth_token->authenticator_type : KM_HW_AUTH_TYPE_NONE;
    tci->finish.auth_timestamp = auth_token ? auth_token->timestamp : 0;
    memset(tci->finish.auth_mac, 0, 32);
    if (auth_token && auth_token->mac.data_length) {
        CHECK_TRUE(KM_ERROR_INVALID_ARGUMENT,
            auth_token->mac.data_length == 32);
        memcpy(tci->finish.auth_mac, auth_token->mac.data, 32);
    }

    CHECK_RESULT_OK( transact(session_handle, tci) );

    *input_consumed_r = input_map->sVirtualLen;
    *output_used_r = tci->finish.output.data_length;
end:
    return ret;
}

typedef keymaster_error_t chunk_submit_func(
    struct TEE_Session *session, const struct operation *op,
    const mcBulkMap_t *input_map, size_t *input_consumed_r,
    const mcBulkMap_t *output_map, size_t *output_used_r,
    const keymaster_hw_auth_token_t *auth_token,
    void *ctx);

/**
 * @param session pointer to session structure
 * @param op pointer to operation in progress
 * @param at_least_one always submit at least one chunk, even if it's empty
 * @param input pointer to input buffer/length
 * @param input_consumed_r where to put the amount of input actually consumed
 * @param output pointer to the output buffer
 * @param output_limit one-past-the-end of the available output buffer
 * @param output_used_r where to put the amount of output actually written
 * @param submit function to call to submit most chunks
 * @param submit_list function to call to submit the final chunk
 * @param ctx uninterpreted context pointer for the @p submit functions
 *
 * @return @c KM_ERROR_OK or error
 *
 * General-purpose data chunking function.
 *
 * There's a (not very generous) limit to the amount of data we can squeeze
 * through the interface to our TA, and our caller isn't aware of it.  We can
 * sort out this mess by splitting the input into smaller pieces and submitting
 * them one at a time.  This function does that.
 *
 * The @p params contain parameter tags and values to be passed through.  These
 * are important because they can carry `additional authenticated data' for AEAD
 * schemes, which can also be arbitrarily large, and require their own special
 * handling.  In particular, there's an ordering rule: no more AAD can be
 * submitted once any of the main payload has been submitted.  This function
 * handles all of that.
 *
 * It submits individual chunks other than the last to its @p submit function
 * for actual submission to the TA; the last is instead passed to
 * @p submit_last.
 *
 * Usually, if there is no @p input data, and no AAD in the @p params, then this
 * function doesn't do anything, and in particular doesn't submit any chunks.
 * If @p at_least_one is true, then at least one chunk is submitted, even if
 * it's entirely empty; obviously, in this case, it will be sent to the
 * @p submit_last function.
 *
 * This function takes responsibility for setting up the necessary shared-memory
 * mappings.
 */
static keymaster_error_t split_update_chunks(
    struct TEE_Session *session, const struct operation *op,
    bool at_least_one,
    const keymaster_blob_t *input, size_t *input_consumed_r,
    uint8_t *output, const uint8_t *output_limit, size_t *output_used_r,
    const keymaster_hw_auth_token_t *auth_token,
    chunk_submit_func *submit, chunk_submit_func *submit_last,
    void *ctx)
{
    keymaster_error_t ret = KM_ERROR_OK;
    mcSessionHandle_t *session_handle = &session->sessionHandle;
    const uint8_t *inp, *inp_limit;
    uint8_t *output_window = NULL;
    mcBulkMap_t input_map = { 0, 0 };
    mcBulkMap_t output_map = { 0, 0 };
    scoped_buf_ptr_t input_window;

    LOG_D("split_update_chunks");

    /* Get the input buffer, if there is one.  (There might not be, say
     * because we're just pushing additional data at an AAD scheme.)
     */
    if (!input || !input->data_length)
        inp = inp_limit = NULL;
    else {
        inp = input->data;
        inp_limit = inp + input->data_length;
    }

    /* Now we get to do the main work. */
    while (at_least_one || inp) {
        bool maybe_last_time = true;
        size_t budget = INPUT_CHUNK_SIZE;

        at_least_one = false;

        /* Now deal with the message input, assuming we have any budget
         * available for this.  We certainly don't want any old buffer from
         * last time hanging around.  If we're using up the last of our input
         * then switch in the other submit function.
         */
        unmap_buffer(session_handle, input_window.buf.get(), &input_map);
        if (inp && budget) {
            size_t n = inp_limit - inp;
            if (n > budget) {
                n = budget;
                maybe_last_time = false;
            }
            budget -= n;
            CHECK_RESULT_OK(copy_to_scoped_buf(inp, n, input_window));
            CHECK_RESULT_OK(map_buffer(session_handle,
                input_window.buf.get(), n, &input_map));
        }

        /* Finally, arrange some output.  There are two interesting cases.
         *
         * If we're in the middle of an operation, then the long pole is AES,
         * which might have a (nearly complete) block (16 bytes) up its
         * sleeve from before this operation; nothing else (currently!) can
         * produce output at this time.
         *
         * On the other hand, if we're at the end of our input buffer, we
         * should hand over the whole of the caller's output buffer because
         * it was presumably provided for some good reason.
         */
        unmap_buffer(session_handle, output_window, &output_map);
        if (output) {
            size_t n = output_limit - output;
            if (!maybe_last_time) {
                size_t avail = input_window.size + 16;
                if (n > avail) n = avail;
            }
            output_window = output;
            CHECK_RESULT_OK(map_buffer(session_handle,
                output_window, n, &output_map));
        }

        /* Push the next chunk through the machinery.  At this point,
         * `maybe_last_time' is definitely right.
         */
        size_t input_consumed = 0;
        size_t output_used = 0;
        CHECK_RESULT_OK((maybe_last_time ? submit_last : submit)(session, op,
            &input_map, &input_consumed,
            &output_map, &output_used,
            auth_token,
            ctx));

        /* Advance the input.  (There doesn't seem to be a way for us to
         * refuse to accept some of the AAD.)
         */
        if (input_consumed_r)
            *input_consumed_r += input_consumed;
        if (inp) {
            inp += input_consumed;
            if (inp >= inp_limit)
                inp = inp_limit = NULL;
        }

        /* Advance the output. */
        if (output_used_r)
            *output_used_r += output_used;
        if (output)
            output += output_used;
    }

end:
    unmap_buffer(session_handle, input_window.buf.get(), &input_map);
    unmap_buffer(session_handle, output_window, &output_map);
    return ret;
}

keymaster_error_t TEE_Update(
    TEE_SessionHandle               sessionHandle,
    keymaster_operation_handle_t    operation_handle,
    const keymaster_blob_t*         input,
    const keymaster_hw_auth_token_t* auth_token,
    size_t*                         input_consumed,
    keymaster_blob_t*               output)
{
    LOG_D("TEE_Update");
    PRINT_BLOB(input);

    keymaster_error_t ret = KM_ERROR_OK;
    struct TEE_Session *session = (struct TEE_Session *)sessionHandle;
    size_t output_used = 0;
    struct operation *op = NULL;

    CHECK_RESULT_OK(lookup_operation(session, operation_handle, &op));

    CHECK_NOT_NULL(input_consumed);
    *input_consumed = 0;

    /* Allocate output buffer if required. */
    if (output != NULL) {
        if (op->algorithm == KM_ALGORITHM_AES) {
            output->data_length = input->data_length + 16; // up to one more block
            CHECK_RESULT_OK(km_alloc((uint8_t**)&output->data, output->data_length));
        } else if (op->algorithm == KM_ALGORITHM_TRIPLE_DES) {
            output->data_length = input->data_length + 8; // up to one more block
            CHECK_RESULT_OK(km_alloc((uint8_t**)&output->data, output->data_length));
        } else {
            /* No output. */
            output->data = NULL;
            output->data_length = 0;
        }
    }

    CHECK_RESULT_OK(split_update_chunks(session, op, false,
        input, input_consumed,
        output ? const_cast</*unconst*/ uint8_t *>(output->data) : NULL,
        output ? output->data + output->data_length : NULL,
        &output_used, auth_token, update_chunk, update_chunk, NULL));
    if (output)
        output->data_length = output_used;

end:
    if (ret != KM_ERROR_OK) {
        // free the output->data if it is allocated
        if (output != NULL) {
            free((void*)output->data);
            output->data = NULL;
            output->data_length = 0;
        }
        release_operation_slot(session, op);
    }

    LOG_D("TEE_Update exiting with %d", ret);
    return ret;
}

keymaster_error_t TEE_Finish(
    TEE_SessionHandle               sessionHandle,
    keymaster_operation_handle_t    operation_handle,
    const keymaster_blob_t*         input,
    const keymaster_blob_t*         signature,
    const keymaster_hw_auth_token_t* auth_token,
    keymaster_blob_t*               output)
{
    LOG_D("TEE_Finish");
    PRINT_BLOB(signature);

    keymaster_error_t ret = KM_ERROR_OK;
    struct TEE_Session *session = (struct TEE_Session *)sessionHandle;
    mcSessionHandle_t *session_handle = &session->sessionHandle;
    scoped_buf_ptr_t signature1;
    struct finish_chunk_ctx fin = { { 0, 0 } };
    struct operation *op = NULL;
    size_t input_consumed = 0;
    size_t output_used = 0;

    CHECK_RESULT_OK(lookup_operation(session, operation_handle, &op));

    if ( signature != NULL ) {
        /* Hack to ensure that non-writable memory can be mapped. */
        CHECK_RESULT_OK(
            copy_to_scoped_buf(signature->data, signature->data_length, signature1));
    }

    if (output != NULL) {
        output->data = NULL;
        output->data_length = 0;
    }

    /* Map signature buffer */
    if (signature1.buf) {
        CHECK_RESULT_OK( map_buffer(session_handle, signature1.buf.get(),
            signature1.size, &fin.signature_map) );
    }

    if (output != NULL) {
        /* The output buffer is used for RSA-encrypt, RSA-decrypt, RSA-sign,
         * AES-GCM-encrypt, AES or DES with PKCS7 padding, HMAC-sign, and
         * ECDSA-sign.  The `begin' request already told us how much extra
         * space we'll need for the answer.
         */
        output->data_length = op->final_length;
        if (op->algorithm == KM_ALGORITHM_AES ||
            op->algorithm == KM_ALGORITHM_TRIPLE_DES)
        {
            output->data_length += (input ? input->data_length : 0);
        }

        /* Allocate the output buffer. The caller must free this. */
        if (output->data_length != 0) {
            CHECK_RESULT_OK(km_alloc((uint8_t**)&output->data,
                output->data_length));
        }
    }

    CHECK_RESULT_OK(split_update_chunks(session, op, true,
        input, &input_consumed,
        output ? const_cast</*unconst*/ uint8_t *>(output->data) : NULL,
        output ? output->data + output->data_length : NULL,
        &output_used, auth_token, update_chunk, final_chunk, &fin));
    CHECK_TRUE(KM_ERROR_UNKNOWN_ERROR,
        !input || input_consumed == input->data_length);
    if (output)
        output->data_length = output_used;

end:
    if (signature1.buf) {
        unmap_buffer(session_handle, signature1.buf.get(),
            &fin.signature_map);
    }

    if (ret != KM_ERROR_OK) {
        if (output != NULL) {
            free((void*)output->data);
            output->data = NULL;
            output->data_length = 0;
        }
    }

    release_operation_slot(session, op);
    LOG_D("TEE_Finish exiting with %d", ret);
    return ret;
}

keymaster_error_t TEE_Abort(
    TEE_SessionHandle               sessionHandle,
    keymaster_operation_handle_t    operation_handle)
{
    LOG_D("TEE_Abort");

    keymaster_error_t  ret  = KM_ERROR_OK;
    struct TEE_Session *session = (struct TEE_Session *)sessionHandle;
    tciMessage_ptr     tci = session->pTci;
    mcSessionHandle_t* session_handle = &session->sessionHandle;
    struct operation *op = NULL;

    CHECK_RESULT_OK(lookup_operation(session, operation_handle, &op));

    /* Update TCI buffer */
    tci->command.header.commandId = CMD_ID_TEE_ABORT;
    tci->abort.handle = operation_handle;

    CHECK_RESULT_OK( transact(session_handle, tci) );

end:
    release_operation_slot(session, op);
    LOG_D("TEE_Abort exiting with %d", ret);
    return ret;
}

static keymaster_error_t parse_SecureKeyWrapper(
    const uint8_t **encrypted_transport_key, size_t *encrypted_transport_key_len,
    uint8_t *IV, size_t *IV_len,
    const uint8_t **key_description, size_t *key_description_len,
    uint32_t *key_format,
    const uint8_t **key_authlist, size_t *key_authlist_len,
    const uint8_t **encrypted_key, size_t *encrypted_key_len,
    uint8_t *tag,
    const uint8_t* data, size_t len)
{
    keymaster_error_t ret = KM_ERROR_OK;
    size_t sublen;
    const uint8_t *p = data;

    // SEQUENCE
    CHECK_TRUE(KM_ERROR_INVALID_ARGUMENT,
        der_read_sequence_length(&sublen, &p, &len) && len == sublen);

    // INTEGER 0 (version)
    CHECK_TRUE(KM_ERROR_INVALID_ARGUMENT,
        len >= 3 && p[0] == 0x02 && p[1] == 1 && p[2] == 0);
    p += 3; len -= 3;

    // OCTET STRING (128 <= length < 65536) (encryptedTransportKey)
    CHECK_TRUE(KM_ERROR_INVALID_ARGUMENT,
        der_read_octet_string_length(encrypted_transport_key_len, &p, &len));
    *encrypted_transport_key = p;
    p += *encrypted_transport_key_len; len -= *encrypted_transport_key_len;

    // OCTET STRING (length <= 16) (initializationVector)
    CHECK_TRUE(KM_ERROR_INVALID_ARGUMENT,
        len >= 2 && p[0] == 0x04 && p[1] <= 16);
    *IV_len = p[1];
    p += 2; len -= 2;
    CHECK_TRUE(KM_ERROR_INVALID_ARGUMENT,
        len >= *IV_len);
    memcpy(IV, p, *IV_len);
    p += *IV_len; len -= *IV_len;

    // SEQUENCE (keyDescription)
    *key_description = p;
    CHECK_TRUE(KM_ERROR_INVALID_ARGUMENT,
        der_read_sequence_length(&sublen, &p, &len));
    *key_description_len = 2 + sublen;
    // INTEGER (length < 128) (KeyFormat)
    CHECK_TRUE(KM_ERROR_INVALID_ARGUMENT,
        len >= 3 && p[0] == 0x02 && p[1] == 1);
    *key_format = p[2];
    p += 3; len -= 3;
    *key_authlist = p;
    *key_authlist_len = sublen - 3;
    p += *key_authlist_len; len -= *key_authlist_len;

    // OCTET STRING (encryptedKey)
    CHECK_TRUE(KM_ERROR_INVALID_ARGUMENT,
        der_read_octet_string_length(encrypted_key_len, &p, &len));
    *encrypted_key = p;
    p += *encrypted_key_len; len -= *encrypted_key_len;

    // OCTET STRING, length 16 (tag)
    CHECK_TRUE(KM_ERROR_INVALID_ARGUMENT,
        len == 18 && p[0] == 0x04 && p[1] == 16);
    memcpy(tag, p + 2, 16);

end:
    return ret;
}

keymaster_error_t TEE_ImportWrappedKey(
    TEE_SessionHandle sessionHandle,
    const keymaster_blob_t* wrapped_key_data,
    const keymaster_key_blob_t* wrapping_key_blob,
    const uint8_t* masking_key,
    const keymaster_key_param_set_t* unwrapping_params,
    uint64_t password_sid,
    uint64_t biometric_sid,
    keymaster_key_blob_t* key_blob,
    keymaster_key_characteristics_t* key_characteristics,
    keymaster_cert_chain_t* cert_chain)
{
    LOG_D("TEE_ImportWrappedKey");

    keymaster_error_t ret = KM_ERROR_OK;
    struct TEE_Session *session = (struct TEE_Session *)sessionHandle;
    tciMessage_ptr tci = session->pTci;
    mcSessionHandle_t* session_handle = &session->sessionHandle;
    uint32_t key_format = 0;
    uint32_t key_type = 0;
    uint64_t sid = 0;
    const uint8_t *key_authlist;
    size_t key_authlist_len;
    uint8_t *key_params = NULL;
    size_t key_params_len;
    const uint8_t *encrypted_transport_key = NULL;
    size_t encrypted_transport_key_len;
    uint8_t *IV = tci->import_wrapped_key.initialization_vector;
    size_t IV_len = 0;
    const uint8_t *key_description = NULL;
    size_t key_description_len;
    const uint8_t *encrypted_key = NULL;
    size_t encrypted_key_len;
    uint8_t *tag = tci->import_wrapped_key.tag;
    scoped_buf_ptr_t serialized_unwrap_params;
    uint8_t *params = NULL;
    size_t params_len;
    uint8_t *wrap = NULL;
    size_t wrap_len;
    size_t ETK_offset, KD_offset, EK_offset, WKB_offset;
    mcBulkMap_t params_Info = {0,0};
    mcBulkMap_t wrap_Info = {0,0};
    mcBulkMap_t key_blob_Info = {0,0};
    size_t max_charsz = 0;
    uint8_t* certs_and_chars = NULL;
    mcBulkMap_t certsAndCharsInfo = {0, 0};
    size_t certs_and_chars_size = 0;

    /* 1. Parse wrapped_key_data */
    CHECK_RESULT_OK(parse_SecureKeyWrapper(
        &encrypted_transport_key, &encrypted_transport_key_len,
        IV, &IV_len,
        &key_description, &key_description_len,
        &key_format,
        &key_authlist, &key_authlist_len,
        &encrypted_key, &encrypted_key_len,
        tag,
        wrapped_key_data->data, wrapped_key_data->data_length));
    CHECK_RESULT_OK(parse_AuthorizationList(
        &key_params, &key_params_len,
        &key_type, &sid,
        key_authlist, key_authlist_len));
    CHECK_TRUE(KM_ERROR_INVALID_ARGUMENT, key_type != 0);

    /* 2. Serialize parameters */
    CHECK_RESULT_OK(km_serialize_params(
        serialized_unwrap_params, unwrapping_params, 0, 0));

    /* 3. Combine buffers */
    ETK_offset = key_params_len;
    KD_offset = ETK_offset + encrypted_transport_key_len;
    EK_offset = KD_offset + key_description_len;
    params_len = EK_offset + encrypted_key_len;
    CHECK_RESULT_OK(km_alloc(&params, params_len));
    memcpy(params, key_params, key_params_len);
    memcpy(params + ETK_offset, encrypted_transport_key, encrypted_transport_key_len);
    memcpy(params + KD_offset, key_description, key_description_len);
    memcpy(params + EK_offset, encrypted_key, encrypted_key_len);
    WKB_offset = serialized_unwrap_params.size;
    wrap_len = WKB_offset + wrapping_key_blob->key_material_size;
    CHECK_RESULT_OK(km_alloc(&wrap, wrap_len));
    memcpy(wrap, serialized_unwrap_params.buf.get(), serialized_unwrap_params.size);
    memcpy(wrap + WKB_offset, wrapping_key_blob->key_material, wrapping_key_blob->key_material_size);

    /* 4. Allocate memory for key_blob. We don't know the key type or size, as
     * they are encrypted, so we assume the worst case (RSA-4096). */
    key_blob->key_material = NULL;
    key_blob->key_material_size = key_blob_max_size(
        KM_ALGORITHM_RSA, 4096, serialized_unwrap_params.size);
    CHECK_RESULT_OK(km_alloc((uint8_t**)&key_blob->key_material, key_blob->key_material_size));

    /* 5. Allocate memory for the serialized key characteristics */
    if (key_characteristics != NULL) {
        max_charsz = key_blob->key_material_size;
        certs_and_chars_size = max_charsz;
    }
    if (cert_chain != NULL) {
        certs_and_chars_size += KM_CERT_CHAIN_SIZE;
    }
    if (certs_and_chars_size != 0) {
        /* Allocate memory for the key characteristics and the serialized cert
           chain. */
        CHECK_RESULT_OK(km_alloc(&certs_and_chars, certs_and_chars_size));
        /* Map buffer for key characteristics and the serialized cert chain. */
        CHECK_RESULT_OK( map_buffer(session_handle,
            certs_and_chars, certs_and_chars_size, &certsAndCharsInfo) );
    }

    /* 6. Map buffers */
    CHECK_RESULT_OK(map_buffer(session_handle,
        params, params_len, &params_Info));
    CHECK_RESULT_OK(map_buffer(session_handle,
        wrap, wrap_len, &wrap_Info));
    CHECK_RESULT_OK(map_buffer(session_handle,
        key_blob->key_material, key_blob->key_material_size, &key_blob_Info));

    /* 7. Prepare TA command */
    tci->command.header.commandId = CMD_ID_TEE_IMPORT_WRAPPED_KEY;
    tci->import_wrapped_key.key_format = key_format;
    tci->import_wrapped_key.key_type = key_type;
    tci->import_wrapped_key.params.data = (uint32_t)params_Info.sVirtualAddr;
    tci->import_wrapped_key.params.data_length = params_len;
    tci->import_wrapped_key.ETK_offset = ETK_offset;
    tci->import_wrapped_key.KD_offset = KD_offset;
    tci->import_wrapped_key.EK_offset = EK_offset;
    tci->import_wrapped_key.IV_len = IV_len;
    memcpy(tci->import_wrapped_key.masking_key, masking_key, 32);
    tci->import_wrapped_key.wrap.data = (uint32_t)wrap_Info.sVirtualAddr;
    tci->import_wrapped_key.wrap.data_length = wrap_len;
    tci->import_wrapped_key.WKB_offset = WKB_offset;
    tci->import_wrapped_key.password_sid = (sid & KM_HW_AUTH_TYPE_PASSWORD) ? password_sid : 0;
    tci->import_wrapped_key.biometric_sid = (sid & KM_HW_AUTH_TYPE_FINGERPRINT) ? biometric_sid : 0;
    tci->import_wrapped_key.key_blob.data = (uint32_t)key_blob_Info.sVirtualAddr;
    tci->import_wrapped_key.key_blob.data_length = key_blob->key_material_size;
    if (key_characteristics != NULL) {
        tci->import_wrapped_key.key_characteristics.data = (uint32_t)certsAndCharsInfo.sVirtualAddr;
        tci->import_wrapped_key.key_characteristics.data_length = max_charsz;
    } else {
        tci->import_wrapped_key.key_characteristics.data = 0;
        tci->import_wrapped_key.key_characteristics.data_length = 0;
    }
    if (cert_chain != NULL) {
        tci->import_wrapped_key.cert_chain.data = (uint32_t)certsAndCharsInfo.sVirtualAddr+max_charsz;
        tci->import_wrapped_key.cert_chain.data_length = KM_CERT_CHAIN_SIZE;
    } else {
        tci->import_wrapped_key.cert_chain.data = 0;
        tci->import_wrapped_key.cert_chain.data_length = 0;
    }

    /* 8. Call TA */
    CHECK_RESULT_OK(transact(session_handle, tci));

    /* 9. Update key blob length */
    key_blob->key_material_size = tci->import_wrapped_key.key_blob.data_length;

    /* 10. Give characteristics to caller */
    if (key_characteristics != NULL) { // Give characteristics to caller.
        CHECK_RESULT_OK(km_deserialize_characteristics(
            key_characteristics, certs_and_chars, max_charsz));
    }

    if (cert_chain != NULL) {
        /* Deserialize certs -> cert_chain, allocating memory for the latter. */
        CHECK_RESULT_OK(km_deserialize_attestation(
            cert_chain, certs_and_chars+max_charsz, KM_CERT_CHAIN_SIZE));
    }

end:
    unmap_buffer(session_handle, params, &params_Info);
    unmap_buffer(session_handle, wrap, &wrap_Info);
    unmap_buffer(session_handle, key_blob->key_material, &key_blob_Info);
    if (certs_and_chars != NULL) {
        unmap_buffer(session_handle, certs_and_chars, &certsAndCharsInfo);
    }
    free(certs_and_chars);

    if (ret != KM_ERROR_OK) {
        keymaster_free_cert_chain(cert_chain);
    }
    free(key_params);
    free(params);
    free(wrap);
    if (ret != KM_ERROR_OK) {
        free((void*)key_blob->key_material);
        key_blob->key_material = NULL;
        key_blob->key_material_size = 0;
    }
    LOG_D("TEE_ImportWrappedKey exiting with %d", ret);
    return ret;
}

#ifndef NDEBUG
keymaster_error_t tee__set_debug_lies(
    TEE_SessionHandle sessionHandle,
    const bootimage_info_t *bootinfo)
{
    LOG_D("%s: OS_VERSION = %" PRIu32"; OS_PATCHLEVEL = %" PRIu32 "; VENDOR_PATCHLEVEL = %" PRIu32 "; BOOT_PATCHLEVEL = %" PRIu32,
        __func__, bootinfo->os_version, bootinfo->os_patchlevel, bootinfo->vendor_patchlevel, bootinfo->boot_patchlevel);

    struct TEE_Session *session = (struct TEE_Session *)sessionHandle;
    keymaster_error_t ret = KM_ERROR_OK;
    mcSessionHandle_t *session_handle = &session->sessionHandle;
    tciMessage_ptr tci = session->pTci;

    tci->command.header.commandId = CMD_ID_TEE_SET_DEBUG_LIES;
    tci->set_debug_lies.os_version = bootinfo->os_version;
    tci->set_debug_lies.os_patchlevel = bootinfo->os_patchlevel;
    tci->set_debug_lies.vendor_patchlevel = bootinfo->vendor_patchlevel;
    tci->set_debug_lies.boot_patchlevel = bootinfo->boot_patchlevel;

    CHECK_RESULT_OK(transact(session_handle, tci));

end:
    return ret;
}
#endif /* #ifndef NDEBUG */

keymaster_error_t TEE_GetHmacSharingParameters(
    TEE_SessionHandle sessionHandle,
    keymaster_hmac_sharing_parameters_t *out_params)
{
    LOG_D("TEE_GetHmacSharingParameters");

    keymaster_error_t ret = KM_ERROR_OK;
    struct TEE_Session *session = (struct TEE_Session *)sessionHandle;
    tciMessage_ptr tci = session->pTci;
    mcSessionHandle_t* session_handle = &session->sessionHandle;

    /* Parameter check */
    CHECK_NOT_NULL(out_params);

    /* Reset output parameters */
    memset(out_params, 0x0, sizeof(*out_params));

    /* Prepare TA command */
    tci->command.header.commandId = CMD_ID_TEE_GET_HMAC_SHARING_PARAMETERS;
    memset(&tci->get_hmac_sharing_parameters, 0x0, sizeof(tci->get_hmac_sharing_parameters));

    /* Call TA */
    CHECK_RESULT_OK(transact(session_handle, tci));

    /* Update output parameters */
    if (tci->get_hmac_sharing_parameters.seed_length == KM_SHARING_PARAMETERS_SEED_LENGTH) {
        CHECK_RESULT_OK(km_alloc((uint8_t**)&out_params->seed.data, KM_SHARING_PARAMETERS_SEED_LENGTH));
        memcpy((void *)out_params->seed.data, tci->get_hmac_sharing_parameters.seed, KM_SHARING_PARAMETERS_NONCE_LENGTH);
        out_params->seed.data_length = KM_SHARING_PARAMETERS_SEED_LENGTH;
    }

    memcpy(out_params->nonce, tci->get_hmac_sharing_parameters.nonce, KM_SHARING_PARAMETERS_NONCE_LENGTH);

end:
    LOG_D("TEE_GetHmacSharingParameters exiting with %d", ret);
    return ret;
}

keymaster_error_t TEE_ComputeSharedMac(
    TEE_SessionHandle sessionHandle,
    const keymaster_hmac_sharing_parameters_set_t* sharing_params,
    keymaster_blob_t *sharing_check)
{
    LOG_D("TEE_ComputeSharedMac");
    keymaster_error_t ret = KM_ERROR_OK;
    struct TEE_Session *session = (struct TEE_Session *)sessionHandle;
    tciMessage_ptr tci = session->pTci;
    mcSessionHandle_t* session_handle = &session->sessionHandle;

    /* Parameter check */
    CHECK_NOT_NULL(sharing_params);
    CHECK_NOT_NULL(sharing_check);

    if (sharing_params->length == 0) {
        LOG_E("%s: wrong number of keymints", __func__);
        return KM_ERROR_INVALID_ARGUMENT;
    }

    if (sharing_params->length > KM_SHARING_PARAMETERS_MAX_NUMBER) {
        LOG_E("%s: Maximum number of supported sharing paraeters is %u", __func__,
              KM_SHARING_PARAMETERS_MAX_NUMBER);
        return KM_ERROR_INVALID_ARGUMENT;
    }

    for (size_t i = 0; i < sharing_params->length; i++) {
        if (sharing_params->params[i].seed.data_length == 0) {
            if (sharing_params->params[i].seed.data != NULL) {
                LOG_E("%s: input parameters mismatch", __func__);
                return KM_ERROR_INVALID_ARGUMENT;
            }
        } else if (sharing_params->params[i].seed.data_length == KM_SHARING_PARAMETERS_SEED_LENGTH) {
            if (sharing_params->params[i].seed.data == NULL) {
                LOG_E("%s: input parameters mismatch", __func__);
                return KM_ERROR_INVALID_ARGUMENT;
            }
        }
        else {
            LOG_E("%s: wrong seed length", __func__);
            return KM_ERROR_INVALID_ARGUMENT;
        }
    }

    /* Reset output parameters */
    memset(sharing_check, 0x0, sizeof(*sharing_check));

    /* Prepare TA command */
    memset(&tci->compute_shared_hmac, 0x0, sizeof(tci->compute_shared_hmac));
    tci->command.header.commandId = CMD_ID_TEE_COMPUTE_SHARED_HMAC;
    tci->compute_shared_hmac.sharing_params_length = sharing_params->length;
    for (size_t i = 0; i < sharing_params->length; i++) {
        if (sharing_params->params[i].seed.data_length == KM_SHARING_PARAMETERS_SEED_LENGTH) {
            tci->compute_shared_hmac.sharing_params[i].seed_length = KM_SHARING_PARAMETERS_SEED_LENGTH;
            memcpy(tci->compute_shared_hmac.sharing_params[i].seed, sharing_params->params[i].seed.data, KM_SHARING_PARAMETERS_SEED_LENGTH);
        }
        memcpy(tci->compute_shared_hmac.sharing_params[i].nonce, sharing_params->params[i].nonce, KM_SHARING_PARAMETERS_NONCE_LENGTH);
    }

    /* Call TA */
    CHECK_RESULT_OK(transact(session_handle, tci));

    /* Update output parameters */
    CHECK_RESULT_OK(km_alloc((uint8_t**)&sharing_check->data, KM_SHARING_CHECK_LENGTH));
    memcpy((void *)sharing_check->data, tci->compute_shared_hmac.sharing_check, KM_SHARING_CHECK_LENGTH);
    sharing_check->data_length = KM_SHARING_CHECK_LENGTH;

end:
    LOG_D("TEE_ComputeSharedMac exiting with %d", ret);
    return ret;
}

keymaster_error_t TEE_GenerateTimestamp(
    TEE_SessionHandle sessionHandle,
    keymaster_timestamp_token_t* timestamp_token)
{
    LOG_D("TEE_GenerateTimestamp");

    keymaster_error_t ret = KM_ERROR_OK;
    struct TEE_Session *session = (struct TEE_Session *)sessionHandle;
    tciMessage_ptr tci = session->pTci;
    mcSessionHandle_t* session_handle = &session->sessionHandle;

    /* Parameter check */
    CHECK_NOT_NULL(timestamp_token);

    /* Prepare TA command */
    memset(&tci->generate_timestamp, 0x0, sizeof(tci->generate_timestamp));
    tci->command.header.commandId = CMD_ID_TEE_GENERATE_TIMESTAMP;
    tci->generate_timestamp.token.challenge = timestamp_token->challenge;

    /* Call TA */
    CHECK_RESULT_OK(transact(session_handle, tci));

    /* Handling output parameters */
    timestamp_token->challenge = tci->generate_timestamp.token.challenge;
    timestamp_token->version = tci->generate_timestamp.token.version;
    timestamp_token->timestamp = tci->generate_timestamp.token.timestamp;
    memcpy(timestamp_token->mac, tci->generate_timestamp.token.mac, 32);

end:
    LOG_D("TEE_GenerateTimestamp exiting with %d", ret);
    return ret;
}

keymaster_error_t TEE_DestroyAttestationIds(
    TEE_SessionHandle sessionHandle)
{
    LOG_D("TEE_DestroyAttestationIds");

    keymaster_error_t ret = KM_ERROR_OK;

    struct TEE_Session *session = (struct TEE_Session*)sessionHandle;
    tciMessage_ptr tci = session->pTci;
    mcSessionHandle_t* session_handle = &session->sessionHandle;

    /* Update TCI buffer */
    tci->command.header.commandId = CMD_ID_TEE_DESTROY_ATTESTATION_IDS;

    /* Talk to TA */
    CHECK_RESULT_OK(transact(session_handle, tci));

end:
    LOG_D("TEE_DestroyAttestationIds exiting with %d", ret);
    return ret;
}

keymaster_error_t TEE_EarlyBootEnded(
    TEE_SessionHandle sessionHandle)
{
    LOG_D("TEE_EarlyBootEnded");

    keymaster_error_t ret = KM_ERROR_OK;

    struct TEE_Session *session = (struct TEE_Session*)sessionHandle;
    tciMessage_ptr tci = session->pTci;
    mcSessionHandle_t* session_handle = &session->sessionHandle;

    /* Update TCI buffer */
    tci->command.header.commandId = CMD_ID_TEE_EARLY_BOOT_ENDED;

    /* Talk to TA */
    CHECK_RESULT_OK(transact(session_handle, tci));

end:
    LOG_D("TEE_EarlyBootEnded exiting with %d", ret);
    return ret;
}

keymaster_error_t TEE_DeviceLocked(
    TEE_SessionHandle sessionHandle,
    bool password_only)
{
    LOG_D("TEE_DeviceLocked");

    keymaster_error_t ret = KM_ERROR_OK;

    struct TEE_Session *session = (struct TEE_Session*)sessionHandle;
    tciMessage_ptr tci = session->pTci;
    mcSessionHandle_t* session_handle = &session->sessionHandle;

    /* Update TCI buffer */
    tci->command.header.commandId = CMD_ID_TEE_DEVICE_LOCKED;
    tci->device_locked.password_only = password_only;

    /* Talk to TA */
    CHECK_RESULT_OK(transact(session_handle, tci));

end:
    LOG_D("TEE_DeviceLocked exiting with %d", ret);
    return ret;
}

keymaster_error_t TEE_UnwrapAesStorageKey(
    TEE_SessionHandle sessionHandle,
    const keymaster_blob_t* wrapped_key_data)
{
    LOG_D("TEE_UnwrapAesStorageKey");

    keymaster_error_t ret = KM_ERROR_OK;
    scoped_buf_ptr_t data;
    mcBulkMap_t dataInfo = {0, 0};

    struct TEE_Session *session = (struct TEE_Session*)sessionHandle;
    tciMessage_ptr tci = session->pTci;
    mcSessionHandle_t* session_handle = &session->sessionHandle;

    /* Hack to ensure that non-writable memory can be mapped. */
    CHECK_RESULT_OK(copy_to_scoped_buf(wrapped_key_data->data, wrapped_key_data->data_length, data));

    /* Map data */
    CHECK_RESULT_OK(map_buffer(session_handle, data.buf.get(), data.size, &dataInfo));

    /* Update TCI buffer */
    tci->command.header.commandId = CMD_ID_TEE_UNWRAP_AES_STORAGE_KEY;
    tci->aes_storage_key_unwrap.wrapped_key_data.data = (uint32_t)dataInfo.sVirtualAddr;
    tci->aes_storage_key_unwrap.wrapped_key_data.data_length = dataInfo.sVirtualLen;

    /* Talk to TA */
    CHECK_RESULT_OK(transact(session_handle, tci));

end:
    unmap_buffer(session_handle, data.buf.get(), &dataInfo);
    if (data.buf) {
        memset(data.buf.get(), 0, data.size);
    }
    LOG_D("TEE_UnwrapAesStorageKey exiting with %d", ret);
    return ret;
}

keymaster_error_t TEE_UpdateAad(
    TEE_SessionHandle               sessionHandle,
    keymaster_operation_handle_t    operation_handle,
    const keymaster_blob_t*         aad,
    const keymaster_hw_auth_token_t* auth_token)
{
    LOG_D("TEE_UpdateAad");
    PRINT_BLOB(aad);

    keymaster_error_t ret = KM_ERROR_OK;
    mcBulkMap_t aad_map = {0, 0};
    struct TEE_Session *session = (struct TEE_Session *)sessionHandle;
    tciMessage_ptr tci = session->pTci;
    struct operation *op = NULL;
    mcSessionHandle_t* session_handle = &session->sessionHandle;
    scoped_buf_ptr_t aad1;

    CHECK_RESULT_OK(lookup_operation(session, operation_handle, &op));

    /* Hack to ensure that non-writable memory can be mapped. */
    CHECK_RESULT_OK(copy_to_scoped_buf(aad->data, aad->data_length, aad1));

    /* Map data */
    CHECK_RESULT_OK( map_buffer(session_handle, aad1.buf.get(), aad1.size, &aad_map) );

    /* Update TCI buffer */
    tci->command.header.commandId = CMD_ID_TEE_UPDATE_AAD;
    tci->update_aad.handle = op->handle;
    tci->update_aad.aad.data = (uint32_t)aad_map.sVirtualAddr;
    tci->update_aad.aad.data_length = aad_map.sVirtualLen;
    tci->update_aad.auth_challenge = auth_token ? auth_token->challenge : 0;
    tci->update_aad.auth_user_id = auth_token ? auth_token->user_id : 0;
    tci->update_aad.auth_authenticator_id = auth_token ? auth_token->authenticator_id :  0;
    tci->update_aad.auth_authenticator_type = auth_token ? auth_token->authenticator_type : KM_HW_AUTH_TYPE_NONE;
    tci->update_aad.auth_timestamp = auth_token ? auth_token->timestamp : 0;
    memset(tci->update_aad.auth_mac, 0, 32);
    if (auth_token && auth_token->mac.data_length) {
        CHECK_TRUE(KM_ERROR_INVALID_ARGUMENT,
            auth_token->mac.data_length == 32);
        memcpy(tci->update_aad.auth_mac, auth_token->mac.data, 32);
    }

    CHECK_RESULT_OK( transact(session_handle, tci) );

end:
    unmap_buffer(session_handle, aad1.buf.get(), &aad_map);
    if (aad1.buf) {
        memset(aad1.buf.get(), 0, aad1.size);
    }
    if (ret != KM_ERROR_OK) {
        release_operation_slot(session, op);
    }

    LOG_D("TEE_UpdateAad exiting with %d", ret);
    return ret;
}


keymaster_error_t TEE_GenerateEcdsaP256Key(
    TEE_SessionHandle sessionHandle,
    bool test_mode,
    keymaster_blob_t *maced_public_key_blob,
    keymaster_key_blob_t *private_key_handle_blob)
{
    LOG_D("TEE_GenerateEcdsaP256Key");

    keymaster_error_t ret = KM_ERROR_OK;
    mcBulkMap_t maced_public_key_map = {0, 0};
    mcBulkMap_t private_key_handle_map = {0, 0};
    struct TEE_Session *session = (struct TEE_Session *)sessionHandle;
    tciMessage_ptr tci = session->pTci;
    mcSessionHandle_t* session_handle = &session->sessionHandle;

    maced_public_key_blob->data_length = 256;
    CHECK_RESULT_OK(km_alloc((uint8_t**)&maced_public_key_blob->data, maced_public_key_blob->data_length));

    private_key_handle_blob->key_material_size = key_blob_max_size(
        KM_ALGORITHM_EC, 256, 0);
    CHECK_RESULT_OK(km_alloc((uint8_t**)&private_key_handle_blob->key_material, private_key_handle_blob->key_material_size));

    /* Map data */
    CHECK_RESULT_OK( map_buffer(session_handle,
        maced_public_key_blob->data,  maced_public_key_blob->data_length,
        &maced_public_key_map) );
    CHECK_RESULT_OK( map_buffer(session_handle,
        private_key_handle_blob->key_material, private_key_handle_blob->key_material_size,
        &private_key_handle_map) );

    /* Update TCI buffer */
    tci->command.header.commandId = CMD_ID_TEE_RKP_GENERATE_ECDSA_P256_KEY;
    tci->rkp_generate_ecdsa_p256_key.test_mode = test_mode;
    tci->rkp_generate_ecdsa_p256_key.maced_public_key.data =
        (uint32_t)maced_public_key_map.sVirtualAddr;
    tci->rkp_generate_ecdsa_p256_key.maced_public_key.data_length =
        maced_public_key_blob->data_length;
    tci->rkp_generate_ecdsa_p256_key.private_key_handle.data =
        (uint32_t)private_key_handle_map.sVirtualAddr;
    tci->rkp_generate_ecdsa_p256_key.private_key_handle.data_length =
        private_key_handle_blob->key_material_size;

    CHECK_RESULT_OK( transact(session_handle, tci) );

    maced_public_key_blob->data_length = tci->rkp_generate_ecdsa_p256_key.maced_public_key.data_length;
    private_key_handle_blob->key_material_size = tci->rkp_generate_ecdsa_p256_key.private_key_handle.data_length;

    PRINT_BLOB(maced_public_key_blob);

end:
    unmap_buffer(session_handle,
        maced_public_key_blob->data, &maced_public_key_map);
    unmap_buffer(session_handle, private_key_handle_blob->key_material,
        &private_key_handle_map);

    if (ret != KM_ERROR_OK) {
        free((void*)maced_public_key_blob->data);
        maced_public_key_blob->data = NULL;
        maced_public_key_blob->data_length = 0;

        free((void*)private_key_handle_blob->key_material);
        private_key_handle_blob->key_material = NULL;
        private_key_handle_blob->key_material_size = 0;
    }

    LOG_D("TEE_GenerateEcdsaP256Key exiting with %d", ret);
    return ret;
}

static keymaster_error_t serialize_keys_to_sign(
    const keymaster_blob_t keys_to_sign[],
    size_t nb_keys_to_sign,
    uint8_t**  keys_to_sign_buf,
    size_t* keys_to_sign_buf_length)
{
    keymaster_error_t ret = KM_ERROR_OK;
    size_t length = 4;
    uint8_t* buf = NULL;
    uint8_t* pos = NULL;

    for (size_t i=0; i<nb_keys_to_sign; i++) {
        length += 4;
        length += keys_to_sign[i].data_length;
    }

    CHECK_RESULT_OK(km_alloc(&buf, length));

    pos = buf;
    set_u32_increment_pos(&pos, nb_keys_to_sign);
    for (size_t i=0; i<nb_keys_to_sign; i++) {
        set_u32_increment_pos(&pos, keys_to_sign[i].data_length);
        set_data_increment_pos(&pos,
            keys_to_sign[i].data, keys_to_sign[i].data_length);
    }

    *keys_to_sign_buf = buf;
    *keys_to_sign_buf_length = length;

end:
    return ret;
}

keymaster_error_t TEE_GenerateCertificateRequest(
    TEE_SessionHandle sessionHandle,
    bool test_mode,
    const keymaster_blob_t keys_to_sign[],
    size_t nb_keys_to_sign,
    const keymaster_blob_t *endpoint_enc_cert_chain,
    const keymaster_blob_t *challenge_blob,
    keymaster_blob_t *device_info,
    keymaster_blob_t *protected_data,
    keymaster_blob_t *keys_to_sign_mac)
{
    LOG_D("TEE_GenerateCertificateRequest");

    keymaster_error_t ret = KM_ERROR_OK;
    struct TEE_Session *session = (struct TEE_Session *)sessionHandle;
    tciMessage_ptr tci = session->pTci;
    mcSessionHandle_t* session_handle = &session->sessionHandle;
    scoped_buf_ptr_t in_params;
    mcBulkMap_t in_params_map = {0, 0};
    mcBulkMap_t device_info_map = {0, 0};
    mcBulkMap_t protected_data_map = {0, 0};
    mcBulkMap_t keys_to_sign_mac_map = {0, 0};
    uint8_t*  keys_to_sign_buf = NULL;
    size_t keys_to_sign_buf_length = 0;

    CHECK_RESULT_OK(serialize_keys_to_sign(
        keys_to_sign, nb_keys_to_sign,
        &keys_to_sign_buf, &keys_to_sign_buf_length));

    /* Hack to ensure that non-writable memory can be mapped. */
    CHECK_RESULT_OK(copy_to_scoped_buf3(
        keys_to_sign_buf, keys_to_sign_buf_length,
        endpoint_enc_cert_chain->data, endpoint_enc_cert_chain->data_length,
        challenge_blob->data, challenge_blob->data_length,
        in_params));

    /* The length depends on the number of properties and the length of the
       values and we can't guess it by advance.
       1024 should be more than enough. */
    device_info->data_length = 1024;
    CHECK_RESULT_OK(km_alloc((uint8_t**)&device_info->data, device_info->data_length));
    protected_data->data_length = 4096;
    CHECK_RESULT_OK(km_alloc((uint8_t**)&protected_data->data, protected_data->data_length));
    keys_to_sign_mac->data_length = 32;
    CHECK_RESULT_OK(km_alloc((uint8_t**)&keys_to_sign_mac->data, keys_to_sign_mac->data_length));

    /* Map data */
    CHECK_RESULT_OK( map_buffer(session_handle,
        in_params.buf.get(),  in_params.size,
        &in_params_map) );
    CHECK_RESULT_OK( map_buffer(session_handle,
        device_info->data,  device_info->data_length,
        &device_info_map) );
    CHECK_RESULT_OK( map_buffer(session_handle,
        protected_data->data,  protected_data->data_length,
        &protected_data_map) );
    CHECK_RESULT_OK( map_buffer(session_handle,
        keys_to_sign_mac->data,  keys_to_sign_mac->data_length,
        &keys_to_sign_mac_map) );

    /* Update TCI buffer */
    tci->command.header.commandId = CMD_ID_TEE_RKP_GENERATE_CERTIFICATE_REQUEST;
    tci->rkp_generate_certificate_request.test_mode = test_mode;
    tci->rkp_generate_certificate_request.keys_to_sign.data =
        (uint32_t)in_params_map.sVirtualAddr;
    tci->rkp_generate_certificate_request.keys_to_sign.data_length =
        keys_to_sign_buf_length;
    tci->rkp_generate_certificate_request.endpoint_encryption_key.data =
        (uint32_t)in_params_map.sVirtualAddr + keys_to_sign_buf_length;
    tci->rkp_generate_certificate_request.endpoint_encryption_key.data_length =
        endpoint_enc_cert_chain->data_length;
    tci->rkp_generate_certificate_request.challenge.data =
        (uint32_t)in_params_map.sVirtualAddr +
            keys_to_sign_buf_length + endpoint_enc_cert_chain->data_length;
    tci->rkp_generate_certificate_request.challenge.data_length =
        challenge_blob->data_length;
    tci->rkp_generate_certificate_request.device_info.data =
        (uint32_t)device_info_map.sVirtualAddr;
    tci->rkp_generate_certificate_request.device_info.data_length =
        device_info->data_length;
    tci->rkp_generate_certificate_request.protected_data.data =
        (uint32_t)protected_data_map.sVirtualAddr;
    tci->rkp_generate_certificate_request.protected_data.data_length =
        protected_data->data_length;
    tci->rkp_generate_certificate_request.keys_to_sign_mac.data =
        (uint32_t)keys_to_sign_mac_map.sVirtualAddr;
    tci->rkp_generate_certificate_request.keys_to_sign_mac.data_length =
        keys_to_sign_mac->data_length;

    CHECK_RESULT_OK( transact(session_handle, tci) );

    device_info->data_length = tci->rkp_generate_certificate_request.device_info.data_length;
    protected_data->data_length = tci->rkp_generate_certificate_request.protected_data.data_length;
    keys_to_sign_mac->data_length = tci->rkp_generate_certificate_request.keys_to_sign_mac.data_length;

    PRINT_BLOB(device_info);
    PRINT_BLOB(protected_data);
    PRINT_BLOB(keys_to_sign_mac);

end:
    unmap_buffer(session_handle,
        in_params.buf.get(), &in_params_map);
    unmap_buffer(session_handle,
        device_info->data, &device_info_map);
    unmap_buffer(session_handle,
        protected_data->data, &protected_data_map);
    unmap_buffer(session_handle,
        keys_to_sign_mac->data, &keys_to_sign_mac_map);

    /* ExySp */
    if (keys_to_sign_buf)
        free(keys_to_sign_buf);
    if (ret != KM_ERROR_OK) {
        free((void*)device_info->data);
        device_info->data = NULL;
        device_info->data_length = 0;
        free((void*)protected_data->data);
        protected_data->data = NULL;
        protected_data->data_length = 0;
        free((void*)keys_to_sign_mac->data);
        keys_to_sign_mac->data = NULL;
        keys_to_sign_mac->data_length = 0;
    }

    if (in_params.buf) {
        memset(in_params.buf.get(), 0, in_params.size);
    }

    LOG_D("TEE_GenerateCertificateRequest exiting with %d", ret);
    return ret;
}

keymaster_error_t TEE_GetRootOfTrust(
    TEE_SessionHandle sessionHandle,
    const uint8_t challenge[16],
    keymaster_blob_t *rot_blob)
{
    LOG_D("TEE_GetRootOfTrust");

    keymaster_error_t ret = KM_ERROR_OK;
    struct TEE_Session *session = (struct TEE_Session *)sessionHandle;
    tciMessage_ptr tci = session->pTci;
    mcSessionHandle_t* session_handle = &session->sessionHandle;
    mcBulkMap_t rot_map = {0, 0};

    memcpy(tci->get_root_of_trust.challenge, challenge, sizeof(tci->get_root_of_trust.challenge));

    rot_blob->data_length = 125;
    CHECK_RESULT_OK(km_alloc((uint8_t**)&rot_blob->data, rot_blob->data_length));

    /* Map data */
    CHECK_RESULT_OK( map_buffer(session_handle,
        rot_blob->data,  rot_blob->data_length,
        &rot_map) );

    /* Update TCI buffer */
    tci->command.header.commandId = CMD_ID_TEE_GET_ROOT_OF_TRUST;
    tci->get_root_of_trust.maced_rot.data = (uint32_t)rot_map.sVirtualAddr;
    tci->get_root_of_trust.maced_rot.data_length = rot_blob->data_length;

    CHECK_RESULT_OK( transact(session_handle, tci) );

    rot_blob->data_length = tci->get_root_of_trust.maced_rot.data_length;

    PRINT_BLOB(rot_blob);

end:
    unmap_buffer(session_handle,
        rot_blob->data, &rot_map);

    if (ret != KM_ERROR_OK) {
        free((void*)rot_blob->data);
        rot_blob->data = NULL;
        rot_blob->data_length = 0;
    }

    LOG_D("TEE_GetRootOfTrust exiting with %d", ret);
    return ret;
}
