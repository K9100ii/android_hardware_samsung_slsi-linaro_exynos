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

#include <stdlib.h>
#include <assert.h>
#include <memory>
#include <time.h>

#include "ssp_strongbox_keymaster_defs.h"
#include "ssp_strongbox_keymaster_serialization.h"
#include "ssp_strongbox_keymaster_common_util.h"

static keymaster_error_t serializer_cal_serialized_len(
    const keymaster_key_param_set_t *params,
    uint32_t *serialized_size,
    uint32_t *serialized_num)
{
    keymaster_error_t ret = KM_ERROR_OK;
    uint32_t params_num;
    keymaster_tag_t tag;
    uint32_t calc_num = 0;
    uint32_t calc_size = 0;

    params_num = (params != NULL) ? params->length : 0;

    calc_size = 4; /* number of parameters(u32) */

    /* calculate the serialized params size */
    for (size_t i = 0; i < params_num; i++) {
        tag = params->params[i].tag;

        /* tag length: keymaster_tag_t */
        calc_size += 4;

        /* length of value */
        switch (keymaster_tag_get_type(tag)) {
            case KM_ENUM:
                calc_size += 4;
                break;
            case KM_ENUM_REP:
                calc_size += 4;
                break;
            case KM_UINT:
                calc_size += 4;
                break;
            case KM_UINT_REP:
                calc_size += 4;
                break;
            case KM_BOOL:
                calc_size += 4;
                break;
            case KM_ULONG:
                calc_size += 8;
                break;
            case KM_DATE:
                calc_size += 8;
                break;
            case KM_ULONG_REP:
                calc_size += 8;
                break;
            case KM_BIGNUM:
                calc_size += 4;
                calc_size += params->params[i].blob.data_length;
                break;
            case KM_BYTES:
                calc_size += 4;
                calc_size += params->params[i].blob.data_length;
                break;
            default: // bad tag
                BLOGE("type is %d", keymaster_tag_get_type(tag));
                ret = KM_ERROR_INVALID_TAG;
                goto end;
        }

        calc_num += 1;
    }
end:
    if (ret == KM_ERROR_OK) {
        *serialized_num = calc_num;
        *serialized_size = calc_size;
    } else {
        *serialized_num = 0;
        *serialized_size = 0;
    }

    return ret;
}


/* FIXME: remove extra params */
keymaster_error_t ssp_serializer_serialize_params(
    std::unique_ptr<uint8_t[]>& params_blob,
    uint32_t *params_blob_size,
    const keymaster_key_param_set_t *params,
    bool extra_param_add_time,
    uint32_t extra_param_keysize,
    uint64_t extra_param_rsapubexp)
{
    keymaster_error_t ret = KM_ERROR_OK;
    uint8_t *blob = NULL;
    uint32_t serialized_size;
    uint32_t params_num;
    uint32_t serialized_params_num = 0;

    bool add_key_size = false;
    bool add_rsa_pubexp = false;

    params_num = (params != NULL) ? params->length : 0;

    EXPECT_KMOK_GOTOEND(serializer_cal_serialized_len(params,
        &serialized_size,
        &serialized_params_num));

    /* if there are no key size TAG, add TAG */
    if (find_tag_pos(params, KM_TAG_KEY_SIZE) < 0 &&
        extra_param_keysize != 0) {
        add_key_size = true;
        serialized_size += (4 + 4);
        serialized_params_num++;
    }

    /* if there are no rsa public exponent TAG, add TAG */
    if (find_tag_pos(params, KM_TAG_RSA_PUBLIC_EXPONENT) < 0 &&
        extra_param_rsapubexp != 0) {
        add_rsa_pubexp = true;
        serialized_size += (4 + 8);
        serialized_params_num++;
    }

    if (extra_param_add_time) {
        serialized_size += (4 + 8);
        serialized_params_num++;
    }

    /* allocate memory for the buffer */
    *params_blob_size = serialized_size;
    params_blob.reset(new (std::nothrow) uint8_t[serialized_size]);
    EXPECT_TRUE_GOTOEND(KM_ERROR_MEMORY_ALLOCATION_FAILED, params_blob);

    /* begin of the buffer */
    blob = params_blob.get();

    append_u32_to_buf(&blob, serialized_params_num);

    /* copy data to the serialized buffer */
    for (size_t i = 0; i < params_num; i++) {
        const keymaster_tag_t tag = params->params[i].tag;

        append_u32_to_buf(&blob, tag);

        switch (keymaster_tag_get_type(tag)) {
            case KM_ENUM:
                append_u32_to_buf(&blob, params->params[i].enumerated);
                break;
            case KM_ENUM_REP:
                append_u32_to_buf(&blob, params->params[i].enumerated);
                break;
            case KM_UINT:
                append_u32_to_buf(&blob, params->params[i].integer);
                break;
            case KM_UINT_REP:
                append_u32_to_buf(&blob, params->params[i].integer);
                break;
            case KM_BOOL:
                append_u32_to_buf(&blob, params->params[i].boolean ? 1 : 0);
                break;
            case KM_ULONG:
                append_u64_to_buf(&blob, params->params[i].long_integer);
                break;
            case KM_ULONG_REP:
                append_u64_to_buf(&blob, params->params[i].long_integer);
                break;
            case KM_DATE:
                append_u64_to_buf(&blob, params->params[i].date_time);
                break;
            case KM_BIGNUM:
                append_u32_to_buf(&blob, params->params[i].blob.data_length);
                append_u8_array_to_buf(&blob, params->params[i].blob.data,
                    params->params[i].blob.data_length);
                break;
            case KM_BYTES:
                append_u32_to_buf(&blob, params->params[i].blob.data_length);
                append_u8_array_to_buf(&blob, params->params[i].blob.data,
                    params->params[i].blob.data_length);
                break;
            default:
                ret = KM_ERROR_INVALID_TAG;
                goto end;
        }
    }
    if (extra_param_add_time) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        append_u32_to_buf(&blob, KM_TAG_CREATION_DATETIME);
        /* creation date time is base on miliseconds */
        append_u64_to_buf(
            &blob,
            ((uint64_t)tv.tv_usec + ((uint64_t)tv.tv_sec * 1000000ull))/1000);
    }
    if (add_key_size) {
        append_u32_to_buf(&blob, KM_TAG_KEY_SIZE);
        append_u32_to_buf(&blob, extra_param_keysize);
    }
    if (add_rsa_pubexp) {
        append_u32_to_buf(&blob, KM_TAG_RSA_PUBLIC_EXPONENT);
        append_u64_to_buf(&blob, extra_param_rsapubexp);
    }

end:
    return ret;
}

keymaster_error_t ssp_serializer_serialize_verification_token(
    std::unique_ptr<uint8_t[]>& params_blob,
    uint32_t *params_blob_size,
    const keymaster_key_param_set_t *params)
{
    keymaster_error_t ret = KM_ERROR_OK;
    uint8_t *blob = NULL;
    uint32_t serialized_size;
    uint32_t params_num;
    uint32_t serialized_params_num = 0;

    params_num = (params != NULL) ? params->length : 0;

    EXPECT_KMOK_GOTOEND(serializer_cal_serialized_len(params,
        &serialized_size,
        &serialized_params_num));

    /* allocate memory for the buffer */
    *params_blob_size = serialized_size;
    params_blob.reset(new (std::nothrow) uint8_t[serialized_size]);
    EXPECT_TRUE_GOTOEND(KM_ERROR_MEMORY_ALLOCATION_FAILED, params_blob);

    /* begin of the buffer */
    blob = params_blob.get();

    append_u32_to_buf(&blob, serialized_params_num);

    /* copy data to the serialized buffer */
    for (size_t i = 0; i < params_num; i++) {
        const keymaster_tag_t tag = params->params[i].tag;

        append_u32_to_buf(&blob, tag);

        switch (keymaster_tag_get_type(tag)) {
            case KM_ENUM:
                append_u32_to_buf(&blob, params->params[i].enumerated);
                break;
            case KM_ENUM_REP:
                append_u32_to_buf(&blob, params->params[i].enumerated);
                break;
            case KM_UINT:
                append_u32_to_buf(&blob, params->params[i].integer);
                break;
            case KM_UINT_REP:
                append_u32_to_buf(&blob, params->params[i].integer);
                break;
            case KM_BOOL:
                append_u32_to_buf(&blob, params->params[i].boolean ? 1 : 0);
                break;
            case KM_ULONG:
                append_u64_to_buf(&blob, params->params[i].long_integer);
                break;
            case KM_ULONG_REP:
                append_u64_to_buf(&blob, params->params[i].long_integer);
                break;
            case KM_DATE:
                append_u64_to_buf(&blob, params->params[i].date_time);
                break;
            case KM_BIGNUM:
                append_u32_to_buf(&blob, params->params[i].blob.data_length);
                append_u8_array_to_buf(&blob, params->params[i].blob.data,
                    params->params[i].blob.data_length);
                break;
            case KM_BYTES:
                append_u32_to_buf(&blob, params->params[i].blob.data_length);
                append_u8_array_to_buf(&blob, params->params[i].blob.data,
                    params->params[i].blob.data_length);
                break;
            default:
                ret = KM_ERROR_INVALID_TAG;
                goto end;
        }
    }

end:
    return ret;
}


#define SSP_DESERIALIZE_BLOB_POS_INC(pos, remain, val) \
    do { \
        pos += val; \
        remain -= val; \
    } while (0)

keymaster_error_t ssp_serializer_deserialize_params(
    uint8_t **blob,
    uint32_t *blob_len,
    keymaster_key_param_set_t *params)
{
    keymaster_error_t ret = KM_ERROR_OK;
    uint32_t val;
    uint32_t data_length;
    keymaster_key_param_t *nparam;

    EXPECT_NE_NULL_GOTOEND(blob);
    EXPECT_NE_NULL_GOTOEND(blob_len);
    EXPECT_NE_NULL_GOTOEND(params);

    EXPECT_TRUE_GOTOEND(KM_ERROR_INSUFFICIENT_BUFFER_SPACE, *blob_len >= 4);
    val = btoh_u32(*blob);
    SSP_DESERIALIZE_BLOB_POS_INC(*blob, *blob_len, 4);
    params->length = val;

    BLOGD("keyparam count(%u)\n", val);

    if (val > KEYPARAMS_COUNT_MAX) {
        BLOGE("Invalid key params to deserialize: count(%u)\n", val);
        ret = KM_ERROR_UNKNOWN_ERROR;
        goto end;
    }
    if (val == 0) {
        BLOGE("No params to deserialize: count(%u)\n", val);
        params->params = NULL;
        goto end;
    }

    params->params = (keymaster_key_param_t*)calloc(val, sizeof(keymaster_key_param_t));
    EXPECT_TRUE_GOTOEND(KM_ERROR_MEMORY_ALLOCATION_FAILED, params->params != NULL);
    memset(params->params, 0x00, val * sizeof(keymaster_key_param_t));

    for (uint32_t i = 0; i < val; i++) {
        nparam = &params->params[i];

        /* get tag */
        EXPECT_TRUE_GOTOEND(KM_ERROR_INSUFFICIENT_BUFFER_SPACE,
            *blob_len >= 4);
        nparam->tag = (keymaster_tag_t)btoh_u32(*blob);
        SSP_DESERIALIZE_BLOB_POS_INC(*blob, *blob_len, 4);

        /* get value */
        switch (keymaster_tag_get_type(nparam->tag)) {
            case KM_ENUM:
                EXPECT_TRUE_GOTOEND(KM_ERROR_INSUFFICIENT_BUFFER_SPACE, *blob_len >= 4);
                nparam->enumerated = btoh_u32(*blob);
                SSP_DESERIALIZE_BLOB_POS_INC(*blob, *blob_len, 4);
                break;
            case KM_ENUM_REP:
                EXPECT_TRUE_GOTOEND(KM_ERROR_INSUFFICIENT_BUFFER_SPACE, *blob_len >= 4);
                nparam->enumerated = btoh_u32(*blob);
                SSP_DESERIALIZE_BLOB_POS_INC(*blob, *blob_len, 4);
                break;
            case KM_UINT:
                EXPECT_TRUE_GOTOEND(KM_ERROR_INSUFFICIENT_BUFFER_SPACE, *blob_len >= 4);
                nparam->integer = btoh_u32(*blob);
                SSP_DESERIALIZE_BLOB_POS_INC(*blob, *blob_len, 4);
                break;
            case KM_UINT_REP:
                EXPECT_TRUE_GOTOEND(KM_ERROR_INSUFFICIENT_BUFFER_SPACE, *blob_len >= 4);
                nparam->integer = btoh_u32(*blob);
                SSP_DESERIALIZE_BLOB_POS_INC(*blob, *blob_len, 4);
                break;
            case KM_BOOL:
                EXPECT_TRUE_GOTOEND(KM_ERROR_INSUFFICIENT_BUFFER_SPACE, *blob_len >= 4);
                nparam->boolean = (btoh_u32(*blob) != 0);
                SSP_DESERIALIZE_BLOB_POS_INC(*blob, *blob_len, 4);
                break;
            case KM_ULONG:
                EXPECT_TRUE_GOTOEND(KM_ERROR_INSUFFICIENT_BUFFER_SPACE, *blob_len >= 8);
                nparam->long_integer = btoh_u64(*blob);
                SSP_DESERIALIZE_BLOB_POS_INC(*blob, *blob_len, 8);
                break;
            case KM_ULONG_REP:
                EXPECT_TRUE_GOTOEND(KM_ERROR_INSUFFICIENT_BUFFER_SPACE, *blob_len >= 8);
                nparam->long_integer = btoh_u64(*blob);
                SSP_DESERIALIZE_BLOB_POS_INC(*blob, *blob_len, 8);
                break;
            case KM_DATE:
                EXPECT_TRUE_GOTOEND(KM_ERROR_INSUFFICIENT_BUFFER_SPACE, *blob_len >= 8);
                nparam->date_time = btoh_u64(*blob);
                SSP_DESERIALIZE_BLOB_POS_INC(*blob, *blob_len, 8);
                break;
            case KM_BIGNUM:
                EXPECT_TRUE_GOTOEND(KM_ERROR_INSUFFICIENT_BUFFER_SPACE, *blob_len >= 4);
                data_length = btoh_u32(*blob);
                SSP_DESERIALIZE_BLOB_POS_INC(*blob, *blob_len, 4);

                EXPECT_TRUE_GOTOEND(KM_ERROR_INSUFFICIENT_BUFFER_SPACE,
                    *blob_len >= data_length);
                nparam->blob.data = (const uint8_t*)calloc(data_length, sizeof(uint8_t));
                EXPECT_TRUE_GOTOEND(KM_ERROR_MEMORY_ALLOCATION_FAILED,
                    nparam->blob.data != NULL);
                memcpy((void*)nparam->blob.data, *blob, data_length);
                nparam->blob.data_length = data_length;
                SSP_DESERIALIZE_BLOB_POS_INC(*blob, *blob_len, data_length);
                break;
            case KM_BYTES:
                EXPECT_TRUE_GOTOEND(KM_ERROR_INSUFFICIENT_BUFFER_SPACE, *blob_len >= 4);
                data_length = btoh_u32(*blob);
                SSP_DESERIALIZE_BLOB_POS_INC(*blob, *blob_len, 4);

                EXPECT_TRUE_GOTOEND(KM_ERROR_INSUFFICIENT_BUFFER_SPACE,
                    *blob_len >= data_length);
                nparam->blob.data = (const uint8_t*)calloc(data_length, sizeof(uint8_t));
                EXPECT_TRUE_GOTOEND(KM_ERROR_MEMORY_ALLOCATION_FAILED,
                    nparam->blob.data != NULL);
                memcpy((void*)nparam->blob.data, *blob, data_length);
                nparam->blob.data_length = data_length;
                SSP_DESERIALIZE_BLOB_POS_INC(*blob, *blob_len, data_length);
                break;
            default:
                ret = KM_ERROR_INVALID_TAG;
                goto end;
        }
    }

end:
    return ret;
}

keymaster_error_t ssp_serializer_deserialize_attestation(
    const uint8_t *buffer,
    uint32_t buffer_length,
    keymaster_cert_chain_t *cert_chain)
{
    keymaster_error_t ret = KM_ERROR_OK;
    uint8_t *blob = (uint8_t*)buffer;
    uint32_t blob_len = buffer_length;
    uint32_t val = 0;
    keymaster_blob_t *cert_blob;

    EXPECT_NE_NULL_GOTOEND(cert_chain);
    EXPECT_NE_NULL_GOTOEND(buffer);

    memset(cert_chain, 0, sizeof(keymaster_cert_chain_t));

    EXPECT_TRUE_GOTOEND(KM_ERROR_INSUFFICIENT_BUFFER_SPACE,
        blob_len >= 4);
    val = btoh_u32(blob);
    SSP_DESERIALIZE_BLOB_POS_INC(blob, blob_len, 4);

    cert_chain->entry_count = val;
    BLOGD(" cert_chain->entry_count : %lu", cert_chain->entry_count);
    if (val == 0) {
        cert_chain->entries = NULL;
        goto end;
    }

    cert_chain->entries = (keymaster_blob_t*)calloc(sizeof(keymaster_blob_t), val);
    EXPECT_TRUE_GOTOEND(KM_ERROR_MEMORY_ALLOCATION_FAILED,
        cert_chain->entries != NULL);
    memset(cert_chain->entries, 0, val * sizeof(keymaster_blob_t));

    for (uint32_t i = 0; i < val; i++) {
        cert_blob = &cert_chain->entries[i];

        EXPECT_TRUE_GOTOEND(KM_ERROR_INSUFFICIENT_BUFFER_SPACE, blob_len >= 4);
        cert_blob->data_length = btoh_u32(blob);
        SSP_DESERIALIZE_BLOB_POS_INC(blob, blob_len, 4);
        EXPECT_TRUE_GOTOEND(KM_ERROR_UNKNOWN_ERROR, cert_blob->data_length != 0);

        cert_blob->data = (const uint8_t*)calloc(cert_blob->data_length, sizeof(uint8_t));
        EXPECT_TRUE_GOTOEND(KM_ERROR_MEMORY_ALLOCATION_FAILED,
            cert_blob->data != NULL);
        EXPECT_TRUE_GOTOEND(KM_ERROR_INSUFFICIENT_BUFFER_SPACE,
            blob_len >= cert_blob->data_length);
        memcpy((void*)cert_blob->data, blob, cert_blob->data_length);
        SSP_DESERIALIZE_BLOB_POS_INC(blob, blob_len, cert_blob->data_length);
    }

end:
    if (ret != KM_ERROR_OK) {
        keymaster_free_cert_chain(cert_chain);
    }
    return ret;
}

keymaster_error_t ssp_serializer_deserialize_characteristics(
    const uint8_t *buffer,
    uint32_t buffer_length,
    keymaster_key_characteristics_t *characteristics)
{
    keymaster_error_t ret = KM_ERROR_OK;
    uint8_t *blob = (uint8_t*)buffer;
    uint32_t blob_len = buffer_length;

    EXPECT_NE_NULL_GOTOEND(characteristics);
    EXPECT_NE_NULL_GOTOEND(buffer);

    memset(characteristics, 0x00, sizeof(keymaster_key_characteristics_t));

    EXPECT_KMOK_GOTOEND(ssp_serializer_deserialize_params(
        &blob,
        &blob_len,
        &characteristics->hw_enforced));
    EXPECT_KMOK_GOTOEND(ssp_serializer_deserialize_params(
        &blob,
        &blob_len,
        &characteristics->sw_enforced));

end:
    if (ret != KM_ERROR_OK) {
        keymaster_free_characteristics(characteristics);
    }

    return ret;
}

