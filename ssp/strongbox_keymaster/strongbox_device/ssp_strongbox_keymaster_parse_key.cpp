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

#include "ssp_strongbox_keymaster_parse_key.h"
#include "ssp_strongbox_tee_api.h"

/* OID 1.2.840.10045.2.1 */
static const uint8_t ecc_key_oid[] = {
    0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01};

/* OID 1.2.840.113549.1.1.1 */
static const uint8_t rsa_key_oid[] = {
    0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01};

/* OID 1.2.840.10045.3.1.7 */
static const uint8_t ecc_p256_oid[] = {
    0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07};

static bool der_parser_get_length(
    const uint8_t **pos,
    size_t *remain_len,
    size_t *ret_len)
{
    uint8_t val;

    if (*remain_len == 0) {
        return false;
    }

    val = (*pos)[0];
    *pos += 1;
    *remain_len -= 1;

    if (val < 0x80) {
        *ret_len = val;
    } else if (val == 0x81) {
        if (*remain_len == 0) {
            return false;
        }
        *ret_len = (*pos)[0];
        *pos += 1;
        *remain_len -= 1;
    } else if (val == 0x82) {
        if (*remain_len < 2) {
            return false;
        }
        *ret_len = 256 * (*pos)[0] + (*pos)[1];
        *pos += 2;
        *remain_len -= 2;
    } else {
        return false;
    }

    return (*ret_len <= *remain_len);
}


static bool der_parser_get_length_of_tag(
    uint8_t tag,
    const uint8_t **pos,
    size_t *remain_len,
    size_t *ret_len)
{
    if (*remain_len == 0 || (*pos)[0] != tag) {
        return false;
    }

    *pos += 1;
    *remain_len -= 1;

    return der_parser_get_length(pos, remain_len, ret_len);
}

bool der_parser_read_integer_length(
    const uint8_t **pos,
    size_t *remain_len,
    size_t *ret_len)
{
    if (!der_parser_get_length_of_tag(0x02, pos, remain_len, ret_len)) {
        return false;
    }

    if (*ret_len >= 1 && (*pos)[0] == 0x00) {
        *pos += 1;
        *remain_len -= 1;
        *ret_len -= 1;
    }
    return true;
}

bool der_parser_read_bitstring_length(
    const uint8_t **pos,
    size_t *remain_len,
    size_t *ret_len)
{
    return der_parser_get_length_of_tag(0x03, pos, remain_len, ret_len);
}

bool der_parser_read_octstring_length(
    const uint8_t **pos,
    size_t *remain_len,
    size_t *ret_len)
{
    return der_parser_get_length_of_tag(0x04, pos, remain_len, ret_len);
}

bool der_parser_read_oid_length(
    const uint8_t **pos,
    size_t *remain_len,
    size_t *ret_len)
{
    return der_parser_get_length_of_tag(0x06, pos, remain_len, ret_len);
}

bool der_parser_read_seq_length(
    const uint8_t **pos,
    size_t *remain_len,
    size_t *ret_len)
{
    return der_parser_get_length_of_tag(0x30, pos, remain_len, ret_len);
}

#define DER_DECODE_GET_VAL(val, pos, remain_len) \
    do { \
         val = **pos; \
        (*pos)++; \
        (*remain_len)--; \
    } while (0)

bool der_parser_read_tag_with_length(
    int *tag,
    size_t *ret_len,
    const uint8_t **pos,
    size_t *remain_len)
{
    uint8_t val = 0;

    if (*remain_len == 0) {
        return false;
    }

    DER_DECODE_GET_VAL(val, pos, remain_len);

    if (val < 0xA0) {
        return false;
    }

    if (val >= 0xBF) {
        if (val != 0xBF || *remain_len == 0)
            return false;

        DER_DECODE_GET_VAL(val, pos, remain_len);

        if (val < 128) {
            if (val < 31) {
                return false;
            }
            *tag = val;
        } else {
            *tag = 128 * (val - 128);
            if (*remain_len == 0) {
                return false;
            }
            DER_DECODE_GET_VAL(val, pos, remain_len);

            *tag += val;
        }
    } else {
        *tag = val - 0xA0;
    }

    return der_parser_get_length(pos, remain_len, ret_len);
}

#define DER_ADVANCE_POS(len) \
    do { \
         data += len; \
        data_len -= len; \
    } while (0)

/* algo(4) + curve_len(4) + curve_type(4) +
 * x_len(4) + y_len(4) + d_len(4)
 */
#define DER_EC_KEY_METALEN (24)

keymaster_error_t ssp_keyparser_parse_keydata_pkcs8ec(
    std::unique_ptr<uint8_t[]>& key_data_ptr,
    size_t *key_data_len,
    const uint8_t *data,
    size_t data_len)
{
    keymaster_error_t ret = KM_ERROR_OK;
    size_t seq_len;
    size_t algorithm_oid_len;
    size_t named_curve_oid_len;
    size_t curve_oid_len;
    size_t octstr_len;
    size_t substr_len;
    size_t bitstr_len;
    size_t curve_in_bits = 0;
    size_t curve_len = 0;
    ecc_curve_t curve;
    uint8_t *key_data;
    const uint8_t *named_curve_oid;

    /* SEQUENCE */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        der_parser_read_seq_length(&data, &data_len, &seq_len)
        && seq_len == data_len);

    /* INTEGER */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        (data_len >= 3) && (data[0] == 0x02) &&
        (data[1] == 0x01) && (data[2] == 0x00));
    DER_ADVANCE_POS(3);

    /* SEQUENCE */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        der_parser_read_seq_length(&data, &data_len, &seq_len));

    /* ECC public key OID */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        der_parser_read_oid_length(&data, &data_len, &algorithm_oid_len));
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        memcmp(data, ecc_key_oid, algorithm_oid_len) == 0);
    DER_ADVANCE_POS(algorithm_oid_len);

    /* curve OID */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        der_parser_read_oid_length(&data, &data_len, &named_curve_oid_len));
    named_curve_oid = data;
    if (named_curve_oid_len == sizeof(ecc_p256_oid) &&
        memcmp(named_curve_oid, ecc_p256_oid, named_curve_oid_len) == 0) {
        curve = ecc_curve_nist_p_256;
        curve_in_bits = 256;
    } else {
        ret = KM_ERROR_INVALID_ARGUMENT;
        goto end;
    }

    curve_len = (curve_in_bits + 7) / 8;
    DER_ADVANCE_POS(named_curve_oid_len);

    /* Allocate memory for the buffer */
    *key_data_len = 24 + (3 * curve_len);
    key_data_ptr.reset(new (std::nothrow) uint8_t[*key_data_len]);
    key_data = key_data_ptr.get();

    htob_u32(key_data, KM_ALGORITHM_EC);
    htob_u32(key_data + 4, curve_in_bits);
    htob_u32(key_data + 8, curve);
    htob_u32(key_data + 12, curve_len);
    htob_u32(key_data + 16, curve_len);
    htob_u32(key_data + 20, curve_len);

    /* OCTET STRING */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        der_parser_read_octstring_length(&data, &data_len, &octstr_len));
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        octstr_len == data_len);

    /* SEQUENCE */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        der_parser_read_seq_length(&data, &data_len, &seq_len));
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        seq_len == data_len);

    /* INTEGER */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        data_len >= 3 && data[0] == 0x02 && data[1] == 0x01 && data[2] == 0x01);
    DER_ADVANCE_POS(3);

    /* OCTET : private key */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        der_parser_read_octstring_length(&data, &data_len, &octstr_len)
        && octstr_len == curve_len);
    memcpy(key_data + DER_EC_KEY_METALEN + 2 * curve_len, data, curve_len);
    DER_ADVANCE_POS(curve_len);

    /* parse public key info */
    if (data_len >= 1 && data[0] == 0xa0) {
        DER_ADVANCE_POS(1);
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            der_parser_get_length(&data, &data_len, &substr_len));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            der_parser_read_oid_length(&data, &data_len, &curve_oid_len));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            curve_oid_len == curve_oid_len);
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            memcmp(data, named_curve_oid, curve_oid_len) == 0);
        DER_ADVANCE_POS(curve_oid_len);
    }

    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        data_len >= 1 && data[0] == 0xa1);
    DER_ADVANCE_POS(1);

    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        der_parser_get_length(&data, &data_len, &substr_len));
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        substr_len == data_len);

    /* BITS string: public key */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        der_parser_read_bitstring_length(&data, &data_len, &bitstr_len));
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        bitstr_len == data_len);

    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        data_len >= 2 && data[0] == 0x00 && data[1] == 0x04);
    DER_ADVANCE_POS(2);

    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        data_len == (2 * curve_len));

    /* curve point x */
    memcpy(key_data + DER_EC_KEY_METALEN, data, curve_len);
    /* curve point y */
    memcpy(key_data + DER_EC_KEY_METALEN + curve_len, data + curve_len, curve_len);

end:
    return ret;
}


#define DER_DECODE_GET_RSA_VAL(val, val_len) \
    do { \
        val = data; \
        data += val_len; \
        data_len -= val_len; \
    } while (0)

keymaster_error_t ssp_keyparser_parse_keydata_pkcs8rsa(
    std::unique_ptr<uint8_t[]>& key_data_ptr,
    size_t *key_data_len,
    const uint8_t *data,
    size_t data_len)
{
    keymaster_error_t ret = KM_ERROR_OK;
    uint8_t *pos;
    size_t seq_len;
    size_t algorithm_oid_len;
    size_t octstr_len;
    const uint8_t *n;
    const uint8_t *e;
    const uint8_t *d;
    const uint8_t *p;
    const uint8_t *q;
    const uint8_t *dp;
    const uint8_t *dq;
    const uint8_t *qinv;
    size_t n_len;
    size_t e_len;
    size_t d_len;
    size_t p_len;
    size_t q_len;
    size_t dp_len;
    size_t dq_len;
    size_t qinv_len;

    /* SEQUENCE */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        der_parser_read_seq_length(&data, &data_len, &seq_len)
        && seq_len == data_len);

    /* INTEGER */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        data_len >= 3 && data[0] == 0x02 && data[1] == 0x01 && data[2] == 0x00);
    DER_ADVANCE_POS(3);

    /* SEQUENCE */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        der_parser_read_seq_length(&data, &data_len, &seq_len));

    /* RSA OID */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        der_parser_read_oid_length(&data, &data_len, &algorithm_oid_len));
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        memcmp(data, rsa_key_oid, algorithm_oid_len) == 0);
    DER_ADVANCE_POS(algorithm_oid_len);

    /* NULL */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        data_len >= 2 && data[0] == 0x05 && data[1] == 0x00);
    DER_ADVANCE_POS(2);

    /* OCTET STRING */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        der_parser_read_octstring_length(&data, &data_len, &octstr_len)
        && octstr_len == data_len);

    /* SEQUENCE */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        der_parser_read_seq_length(&data, &data_len, &seq_len)
        && seq_len == data_len);

    /* INTEGER */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        data_len >= 3 && data[0] == 0x02 && data[1] == 0x01 && data[2] == 0x00);
    DER_ADVANCE_POS(3);

    /* INTEGER n */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        der_parser_read_integer_length(&data, &data_len, &n_len));
    DER_DECODE_GET_RSA_VAL(n, n_len);

    /* INTEGER e */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        der_parser_read_integer_length(&data, &data_len, &e_len));
    DER_DECODE_GET_RSA_VAL(e, e_len);

    /* INTEGER d */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        der_parser_read_integer_length(&data, &data_len, &d_len));
    DER_DECODE_GET_RSA_VAL(d, d_len);

    /* INTEGER p */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        der_parser_read_integer_length(&data, &data_len, &p_len));
    DER_DECODE_GET_RSA_VAL(p, p_len);

    /* INTEGER q */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        der_parser_read_integer_length(&data, &data_len, &q_len));
    DER_DECODE_GET_RSA_VAL(q, q_len);

    /* INTEGER dp */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        der_parser_read_integer_length(&data, &data_len, &dp_len));
    DER_DECODE_GET_RSA_VAL(dp, dp_len);

    /* INTEGER dq */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        der_parser_read_integer_length(&data, &data_len, &dq_len));
    DER_DECODE_GET_RSA_VAL(dq, dq_len);

    /* INTEGER qinv */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        der_parser_read_integer_length(&data, &data_len, &qinv_len));
    DER_DECODE_GET_RSA_VAL(qinv, qinv_len);

    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT, data_len == 0);

    /* Allocate memory for the buffer */
    *key_data_len = SSP_DER_KEYDATA_RSA_META_LEN +
        n_len + e_len + d_len + p_len + q_len + dp_len + dq_len + qinv_len;
    key_data_ptr.reset(new (std::nothrow) uint8_t[*key_data_len]);
    pos = key_data_ptr.get();

    append_u32_to_buf(&pos, KM_ALGORITHM_RSA);
    append_u32_to_buf(&pos, 8 * n_len);
    append_u32_to_buf(&pos, 8 * n_len);
    append_u32_to_buf(&pos, n_len);
    append_u32_to_buf(&pos, e_len);
    append_u32_to_buf(&pos, d_len);
    append_u32_to_buf(&pos, p_len);
    append_u32_to_buf(&pos, q_len);
    append_u32_to_buf(&pos, dp_len);
    append_u32_to_buf(&pos, dq_len);
    append_u32_to_buf(&pos, qinv_len);
    append_u8_array_to_buf(&pos, n, n_len);
    append_u8_array_to_buf(&pos, e, e_len);
    append_u8_array_to_buf(&pos, d, d_len);
    append_u8_array_to_buf(&pos, p, p_len);
    append_u8_array_to_buf(&pos, q, q_len);
    append_u8_array_to_buf(&pos, dp, dp_len);
    append_u8_array_to_buf(&pos, dq, dq_len);
    append_u8_array_to_buf(&pos, qinv, qinv_len);

end:
    return ret;
}

keymaster_error_t ssp_keyparser_parse_keydata_raw(
    keymaster_algorithm_t algorithm,
    std::unique_ptr<uint8_t[]>& key_data_ptr,
    size_t *key_data_len,
    const uint8_t *data,
    size_t data_len)
{
    keymaster_error_t ret = KM_ERROR_OK;
    uint8_t *key_data;

    *key_data_len = 4 + 4 + data_len;
    key_data_ptr.reset(new (std::nothrow) uint8_t[*key_data_len]);
    key_data = key_data_ptr.get();

    htob_u32(key_data, algorithm);
    htob_u32(key_data + 4, 8 * data_len);
    memcpy(key_data + 8, data, data_len);

    return ret;
}

keymaster_error_t ssp_keyparser_get_rsa_exponent_from_parsedkey(
    uint8_t *keydata,
    size_t keydata_len,
    uint64_t *rsa_e)
{
    keymaster_error_t ret = KM_ERROR_OK;
    uint32_t n_len = btoh_u32(keydata + 12);
    uint32_t e_len = btoh_u32(keydata + 16);
    *rsa_e = 0;

    if (e_len > 8) {
        return KM_ERROR_INVALID_ARGUMENT;
    }
    if (keydata_len < SSP_DER_KEYDATA_RSA_META_LEN + n_len + e_len) {
        return KM_ERROR_UNKNOWN_ERROR;
    }
    for (uint8_t i = 0; i < e_len; i++) {
        *rsa_e += (uint64_t)keydata[SSP_DER_KEYDATA_RSA_META_LEN + n_len + i] << (8*(e_len - 1 - i));
    }

    return ret;
}

keymaster_error_t ssp_keyparser_get_keysize_from_parsedkey(
    uint8_t *keydata,
    uint32_t *keysize)
{
    keymaster_error_t ret = KM_ERROR_OK;

    *keysize = btoh_u32(keydata + 4);

    return ret;
}


#define WKEY_ADVANCE_POS(len) \
    do { \
         pos += len; \
        data_len -= len; \
    } while (0)

keymaster_error_t ssp_keyparser_parse_wrappedkey(
    keymaster_blob_t *encrypted_transport_key,
    keymaster_blob_t *iv,
    keymaster_blob_t *key_description,
    keymaster_key_format_t *key_format,
    keymaster_blob_t *auth_list,
    keymaster_blob_t *encrypted_key,
    keymaster_blob_t *tag,
    const keymaster_blob_t *wrappedkey)
{
    keymaster_error_t ret = KM_ERROR_OK;
    size_t seq_len;
    const uint8_t *pos = wrappedkey->data;
    size_t data_len = wrappedkey->data_length;

    /* ::= SEQUENCE */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        der_parser_read_seq_length(&pos, &data_len, &seq_len) && data_len == seq_len);

    /* version: INTEGER */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        data_len >= 3 && pos[0] == 0x02 && pos[1] == 1 && pos[2] == 0);
    WKEY_ADVANCE_POS(3);

    /* encryptedTransportKey: OCTET_STRING */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        der_parser_read_octstring_length(&pos, &data_len,
            &encrypted_transport_key->data_length));
    encrypted_transport_key->data = pos;
    WKEY_ADVANCE_POS(encrypted_transport_key->data_length);

    /* initializationVector: OCTET_STRING */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        data_len >= 2 && pos[0] == 0x04 && pos[1] <= 16);
    iv->data_length = pos[1];
    WKEY_ADVANCE_POS(2);
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        data_len >= iv->data_length);
    memcpy((uint8_t *)iv->data, pos, iv->data_length);
    WKEY_ADVANCE_POS(iv->data_length);

    /* keyDescription ::= SEQUENCE */
    key_description->data = pos;
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        der_parser_read_seq_length(&pos, &data_len, &seq_len));
    key_description->data_length = 2 + seq_len;

    /* KeyFormat: INTEGER */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        data_len >= 3 && pos[0] == 0x02 && pos[1] == 1);
    *key_format = (keymaster_key_format_t)pos[2];
    WKEY_ADVANCE_POS(3);

    /* KeyParams: AuthorizationList ::= SEQUENCE */
    auth_list->data = pos;
    auth_list->data_length = seq_len - 3;
    WKEY_ADVANCE_POS(auth_list->data_length);

    /* encryptedKey: OCTET_STRING */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        der_parser_read_octstring_length(&pos, &data_len, &encrypted_key->data_length));
    encrypted_key->data = pos;
    WKEY_ADVANCE_POS(encrypted_key->data_length);

    /* tag: OCTET_STRING
     * AES GCM TAG size is 16
     */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        data_len == 18 && pos[0] == 0x04 && pos[1] == 16);
    memcpy((uint8_t *)tag->data, pos + 2, 16);
    tag->data_length = 16;

end:
    return ret;
}

/*
 * AuthorizationList Parsing and serialization
 */
static const keymaster_tag_t km4_attest_authlist[] = {
    KM_TAG_PURPOSE,                        /* [1] EXPLICIT SET OF INTEGER OPTIONAL */
    KM_TAG_ALGORITHM,                      /* [2] EXPLICIT INTEGER OPTIONAL */
    KM_TAG_KEY_SIZE,                       /* [3] EXPLICIT INTEGER OPTIONAL */
    KM_TAG_BLOCK_MODE,                     /* [4] EXPLICIT SET OF INTEGER OPTIONAL */
    KM_TAG_DIGEST,                         /* [5] EXPLICIT SET OF INTEGER OPTIONAL */
    KM_TAG_PADDING,                        /* [6] EXPLICIT SET OF INTEGER OPTIONAL */
    KM_TAG_CALLER_NONCE,                   /* [7] EXPLICIT NULL OPTIONAL */
    KM_TAG_MIN_MAC_LENGTH,                 /* [8] EXPLICIT INTEGER OPTIONAL */
    KM_TAG_EC_CURVE,                       /* [10] EXPLICIT INTEGER OPTIONAL */
    KM_TAG_RSA_PUBLIC_EXPONENT,            /* [200] EXPLICIT INTEGER OPTIONAL */
    KM_TAG_ROLLBACK_RESISTANCE,            /* [303] EXPLICIT NULL OPTIONAL */
    KM_TAG_ACTIVE_DATETIME,                /* [400] EXPLICIT INTEGER OPTIONAL */
    KM_TAG_ORIGINATION_EXPIRE_DATETIME,    /* [401] EXPLICIT INTEGER OPTIONAL */
    KM_TAG_USAGE_EXPIRE_DATETIME,          /* [402] EXPLICIT INTEGER OPTIONAL */
    KM_TAG_USER_SECURE_ID,                 /* [502] EXPLICIT INTEGER OPTIONAL */
    KM_TAG_NO_AUTH_REQUIRED,               /* [503] EXPLICIT NULL OPTIONAL */
    KM_TAG_USER_AUTH_TYPE,                 /* [504] EXPLICIT INTEGER OPTIONAL */
    KM_TAG_AUTH_TIMEOUT,                   /* [505] EXPLICIT INTEGER OPTIONAL */
    KM_TAG_ALLOW_WHILE_ON_BODY,            /* [506] EXPLICIT NULL OPTIONAL */
    KM_TAG_TRUSTED_USER_PRESENCE_REQUIRED, /* [507] EXPLICIT NULL OPTIONAL */
    KM_TAG_TRUSTED_CONFIRMATION_REQUIRED,  /* [508] EXPLICIT NULL OPTIONAL */
    KM_TAG_UNLOCKED_DEVICE_REQUIRED,       /* [509] EXPLICIT NULL OPTIONAL */
    KM_TAG_CREATION_DATETIME,              /* [701] EXPLICIT INTEGER OPTIONAL */
    KM_TAG_ORIGIN,                         /* [702] EXPLICIT INTEGER OPTIONAL */
    KM_TAG_ROOT_OF_TRUST,                  /* [704] EXPLICIT RootOfTrust OPTIONAL */
    KM_TAG_OS_VERSION,                     /* [705] EXPLICIT INTEGER OPTIONAL */
    KM_TAG_OS_PATCHLEVEL,                  /* [706] EXPLICIT INTEGER OPTIONAL */
    KM_TAG_ATTESTATION_APPLICATION_ID,     /* [709] EXPLICIT OCTET_STRING OPTIONAL */
    KM_TAG_ATTESTATION_ID_BRAND,           /* [710] EXPLICIT OCTET_STRING OPTIONAL */
    KM_TAG_ATTESTATION_ID_DEVICE,          /* [711] EXPLICIT OCTET_STRING OPTIONAL */
    KM_TAG_ATTESTATION_ID_PRODUCT,         /* [712] EXPLICIT OCTET_STRING OPTIONAL */
    KM_TAG_ATTESTATION_ID_SERIAL,          /* [713] EXPLICIT OCTET_STRING OPTIONAL */
    KM_TAG_ATTESTATION_ID_IMEI,            /* [714] EXPLICIT OCTET_STRING OPTIONAL */
    KM_TAG_ATTESTATION_ID_MEID,            /* [715] EXPLICIT OCTET_STRING OPTIONAL */
    KM_TAG_ATTESTATION_ID_MANUFACTURER,    /* [716] EXPLICIT OCTET_STRING OPTIONAL */
    KM_TAG_ATTESTATION_ID_MODEL,           /* [717] EXPLICIT OCTET_STRING OPTIONAL */
    KM_TAG_VENDOR_PATCHLEVEL,              /* [718] EXPLICIT INTEGER OPTIONAL */
    KM_TAG_BOOT_PATCHLEVEL,                /* [719] EXPLICIT INTEGER OPTIONAL */
};

#define ATTEST_AUTH_NUMS (sizeof(km4_attest_authlist) / sizeof(keymaster_tag_t))
#define TAG_ID_MASK (0x0FFFFFFF)
#define GET_TAG_ID(x) ((x) & TAG_ID_MASK)

static keymaster_error_t authorization_list_parse_asn1_set_of_integer(
    const uint8_t *data,
    size_t len,
    uint32_t *params_num,
    size_t *required_len)
{
    keymaster_error_t ret = KM_ERROR_OK;
    uint32_t local_param_num;

    /* SET OF INTEGER */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        len >= 2 && data[0] == 0x31 && data[1] == len - 2);

    /* max value of integer is 128.
     * so interger takes 3 bytes: [0x02][1][val]
     */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        len % 3 == 2);
    local_param_num = (len - 2)/3;

    *params_num += local_param_num;

    /* TAG(4bytes) + VALUE(4bytes) */
    *required_len += (4 + 4) * local_param_num;

end:
    return ret;
}

static keymaster_error_t authorization_list_parse_asn1_integer_u32(
    const uint8_t *data,
    size_t len,
    uint32_t *params_num,
    size_t *required_len)
{
    keymaster_error_t ret = KM_ERROR_OK;

    /* INTEGER */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        len >= 2 && data[0] == 0x02 && data[1] == len - 2);

    (*params_num)++;

    /* TAG(4bytes) + VALUE(4bytes) */
    *required_len += (4 + 4);

end:
    return ret;
}

static keymaster_error_t authorization_list_parse_asn1_integer_u64(
    const uint8_t *data,
    size_t len,
    uint32_t *params_num,
    size_t *required_len)
{
    keymaster_error_t ret = KM_ERROR_OK;

    /* INTEGER (8 bytes) */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        len >= 2 && data[0] == 0x02 && data[1] == len - 2);

    (*params_num)++;

    /* TAG(4bytes) + VALUE(8bytes) */
    *required_len += (4 + 8);

end:
    return ret;
}

static keymaster_error_t authorization_list_parse_asn1_null(
    const uint8_t *data,
    size_t len,
    uint32_t *params_num,
    size_t *required_len)
{
    keymaster_error_t ret = KM_ERROR_OK;

    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        len == 2 && data[0] == 0x05 && data[1] == 0);

    (*params_num)++;

    /* TAG(4bytes) + VALUE(4bytes) */
    *required_len += (4 + 4);

end:
    return ret;
}

static keymaster_error_t authorization_list_parse_asn1_octet_string(
    const uint8_t *data,
    size_t len,
    uint32_t *params_num,
    size_t *required_len)
{
    keymaster_error_t ret = KM_ERROR_OK;

    size_t ret_len;
    size_t remain_len = len;

    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        der_parser_read_octstring_length(&data, &remain_len, &ret_len));

    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        ret_len == len && remain_len == 0);

    (*params_num)++;

    /* TAG(4bytes) + length(4bytes) + value(n bytes) */
    *required_len += (4 + 4 + ret_len);

end:
    return ret;
}


static keymaster_error_t authorization_list_parse_asn1_octet_string_rot(
    const uint8_t *data,
    size_t len,
    uint32_t *params_num,
    size_t *required_len)
{
    (void)params_num;
    (void)required_len;
    (void)data;
    (void)len;
    keymaster_error_t ret = KM_ERROR_OK;

    /* ROT is not added to key params. ROT is just for AuthorizationList of Attestation
     * So, this function just skip parsing.
     *
     * Tag::ROOT_OF_TRUST specifies the root of trust, the key used by verified boot to validate the
     * operating system booted (if any).  This tag is never provided to or returned from Keymaster
     * in the key characteristics.  It exists only to define the tag for use in the attestation
     * record.
     *
     * Must never appear in KeyCharacteristics.
     */

    return ret;
}

static keymaster_error_t authorization_list_parse_asn1(
    keymaster_tag_t auth_tag,
    uint32_t *params_num,
    size_t *required_len,
    const uint8_t *data, size_t len)
{
    keymaster_error_t ret = KM_ERROR_OK;

    switch (auth_tag) {
        /* ENUM_REP */
        case KM_TAG_PURPOSE:
        case KM_TAG_BLOCK_MODE:
        case KM_TAG_DIGEST:
        case KM_TAG_PADDING:
            EXPECT_KMOK_GOTOEND(authorization_list_parse_asn1_set_of_integer(
                data, len, params_num, required_len));
            break;
        /* ENUM */
        case KM_TAG_ALGORITHM:
        case KM_TAG_EC_CURVE:
        case KM_TAG_USER_AUTH_TYPE:
        case KM_TAG_ORIGIN:
            EXPECT_KMOK_GOTOEND(authorization_list_parse_asn1_integer_u32(
                data, len, params_num, required_len));
            break;
        /* UINT */
        case KM_TAG_KEY_SIZE:
        case KM_TAG_AUTH_TIMEOUT:
        case KM_TAG_OS_VERSION:
        case KM_TAG_OS_PATCHLEVEL:
        case KM_TAG_VENDOR_PATCHLEVEL:
        case KM_TAG_BOOT_PATCHLEVEL:
        case KM_TAG_MIN_MAC_LENGTH:
            EXPECT_KMOK_GOTOEND(authorization_list_parse_asn1_integer_u32(
                data, len, params_num, required_len));
            break;
        /* ULONG */
        case KM_TAG_RSA_PUBLIC_EXPONENT:
        case KM_TAG_USER_SECURE_ID:
            EXPECT_KMOK_GOTOEND(authorization_list_parse_asn1_integer_u64(
                data, len, params_num, required_len));
            break;
        /* DATE */
        case KM_TAG_ACTIVE_DATETIME:
        case KM_TAG_ORIGINATION_EXPIRE_DATETIME:
        case KM_TAG_USAGE_EXPIRE_DATETIME:
        case KM_TAG_CREATION_DATETIME:
            EXPECT_KMOK_GOTOEND(authorization_list_parse_asn1_integer_u64(
                data, len, params_num, required_len));
            break;
        /* BOOL */
        case KM_TAG_ROLLBACK_RESISTANCE:
        case KM_TAG_NO_AUTH_REQUIRED:
        case KM_TAG_ALLOW_WHILE_ON_BODY:
        case KM_TAG_TRUSTED_USER_PRESENCE_REQUIRED:
        case KM_TAG_TRUSTED_CONFIRMATION_REQUIRED:
        case KM_TAG_UNLOCKED_DEVICE_REQUIRED:
        case KM_TAG_CALLER_NONCE:
            EXPECT_KMOK_GOTOEND(authorization_list_parse_asn1_null(
                data, len, params_num, required_len));
            break;
        /* BYTES */
        case KM_TAG_ATTESTATION_APPLICATION_ID:
        case KM_TAG_ATTESTATION_ID_BRAND:
        case KM_TAG_ATTESTATION_ID_DEVICE:
        case KM_TAG_ATTESTATION_ID_PRODUCT:
        case KM_TAG_ATTESTATION_ID_SERIAL:
        case KM_TAG_ATTESTATION_ID_IMEI:
        case KM_TAG_ATTESTATION_ID_MEID:
        case KM_TAG_ATTESTATION_ID_MANUFACTURER:
        case KM_TAG_ATTESTATION_ID_MODEL:
            EXPECT_KMOK_GOTOEND(authorization_list_parse_asn1_octet_string(
                data, len, params_num, required_len));
            break;
        case KM_TAG_ROOT_OF_TRUST:
            EXPECT_KMOK_GOTOEND(authorization_list_parse_asn1_octet_string_rot(
                data, len, params_num, required_len));
            break;
        default:
            EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT, false);
    }

end:
    return ret;
}

/* get uint32 with big endian byte string */
static uint32_t get_sized_u32(const uint8_t *pos, size_t len) {
    uint32_t val = 0;

    for (size_t i = 0; i < len; i++) {
        val = (val << 8);
        val += pos[i];
    }

    return val;
}

/* get uint64 with big endian byte string */
static uint64_t get_sized_u64(const uint8_t *pos, size_t len) {
    uint64_t val = 0;

    for (size_t i = 0; i < len; i++) {
        val = (val << 8);
        val += pos[i];
    }

    return val;
}

static keymaster_error_t authorization_list_serialize_tag_set_of_integer(
    keymaster_tag_t auth_tag,
    uint8_t **key_params,
    const uint8_t *data,
    size_t len)
{
    keymaster_error_t ret = KM_ERROR_OK;

    int num = 0;
    uint32_t val;
    int i;

    /* SET OF INTEGER */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        len >= 2 && data[0] == 0x31 && data[1] == len - 2);
    num = data[1];
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        num % 3 == 0);
    num /= 3;

    for (i = 0; i < num; i++) {
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            data[2 + (3 * i)] == 0x02);
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            data[2 + (3 * i) + 1] == 1);
        val = data[2 + (3 * i) + 2];
        append_u32_to_buf(key_params, auth_tag);
        append_u32_to_buf(key_params, val);
    }

end:

    return ret;
}

static keymaster_error_t authorization_list_serialize_tag_integer_u32(
    keymaster_tag_t auth_tag,
    uint8_t **key_params,
    const uint8_t *data,
    size_t len)
{
    keymaster_error_t ret = KM_ERROR_OK;
    uint32_t val;

    /* INTEGER (4 bytes) */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        len >= 2 && data[0] == 0x02 && data[1] == len - 2);
    data += 2;
    len -= 2;

    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        len <= 4);

    val = get_sized_u32(data, len);
    append_u32_to_buf(key_params, auth_tag);
    append_u32_to_buf(key_params, val);
end:

    return ret;
}

static keymaster_error_t authorization_list_serialize_tag_integer_u64(
    keymaster_tag_t auth_tag,
    uint8_t **key_params,
    const uint8_t *data,
    size_t len)
{
    keymaster_error_t ret = KM_ERROR_OK;
    uint64_t val;

    /* INTEGER (8 bytes) */
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        len >= 2 && data[0] == 0x02 && data[1] == len - 2);

    data += 2;
    len -= 2;

    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        len <= 8);

    val = get_sized_u64(data, len);
    append_u32_to_buf(key_params, auth_tag);
    append_u64_to_buf(key_params, val);

end:

    return ret;
}

static keymaster_error_t authorization_list_serialize_tag_null(
    keymaster_tag_t auth_tag,
    uint8_t **key_params,
    const uint8_t *data,
    size_t len)
{
    keymaster_error_t ret = KM_ERROR_OK;

    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        len == 2 && data[0] == 0x05 && data[1] == 0);

    append_u32_to_buf(key_params, auth_tag);
    append_u32_to_buf(key_params, 1);

end:

    return ret;
}

static keymaster_error_t authorization_list_serialize_tag_octet_string(
    keymaster_tag_t auth_tag,
    uint8_t **key_params,
    const uint8_t *data,
    size_t len)
{
    keymaster_error_t ret = KM_ERROR_OK;

    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        len >= 2 && data[0] == 0x04 && data[1] == len - 2);

    append_u32_to_buf(key_params, auth_tag);
    append_u32_to_buf(key_params, len - 2);
    memcpy(*key_params, data + 2, len - 2);
    *key_params += (len - 2);

end:

    return ret;
}

static keymaster_error_t authorization_list_serialize_tag(
    keymaster_tag_t auth_tag,
    uint8_t **key_params,
    keymaster_algorithm_t *key_type,
    uint32_t *hw_auth_type,
    const uint8_t *data, size_t len)
{
    keymaster_error_t ret = KM_ERROR_OK;

    switch (auth_tag) {
        /* ENUM_REP */
        case KM_TAG_PURPOSE:
        case KM_TAG_BLOCK_MODE:
        case KM_TAG_DIGEST:
        case KM_TAG_PADDING:
            EXPECT_KMOK_GOTOEND(authorization_list_serialize_tag_set_of_integer(
                auth_tag, key_params, data, len));
            break;
        /* ENUM */
        case KM_TAG_ALGORITHM:
        case KM_TAG_EC_CURVE:
        case KM_TAG_USER_AUTH_TYPE:
        case KM_TAG_ORIGIN:
            EXPECT_KMOK_GOTOEND(authorization_list_serialize_tag_integer_u32(
                auth_tag, key_params, data, len));

            if (auth_tag == KM_TAG_ALGORITHM) {
                /* skip 2 bytes tag */
                data += 2;
                len -= 2;
                /* get key type */
                *key_type = (keymaster_algorithm_t)get_sized_u32(data, len);
            }

            if (auth_tag == KM_TAG_USER_AUTH_TYPE) {
                /* skip 2 bytes tag */
                data += 2;
                len -= 2;
                /* get key type */
                *hw_auth_type = (keymaster_algorithm_t)get_sized_u32(data, len);
            }
            break;
        /* UINT */
        case KM_TAG_KEY_SIZE:
        case KM_TAG_AUTH_TIMEOUT:
        case KM_TAG_OS_VERSION:
        case KM_TAG_OS_PATCHLEVEL:
        case KM_TAG_VENDOR_PATCHLEVEL:
        case KM_TAG_BOOT_PATCHLEVEL:
        case KM_TAG_MIN_MAC_LENGTH:
            EXPECT_KMOK_GOTOEND(authorization_list_serialize_tag_integer_u32(
                auth_tag, key_params, data, len));
            break;
        /* ULONG */
        case KM_TAG_RSA_PUBLIC_EXPONENT:
        case KM_TAG_USER_SECURE_ID:
            EXPECT_KMOK_GOTOEND(authorization_list_serialize_tag_integer_u64(
                auth_tag, key_params, data, len));
            break;
        /* DATE */
        case KM_TAG_ACTIVE_DATETIME:
        case KM_TAG_ORIGINATION_EXPIRE_DATETIME:
        case KM_TAG_USAGE_EXPIRE_DATETIME:
        case KM_TAG_CREATION_DATETIME:
            EXPECT_KMOK_GOTOEND(authorization_list_serialize_tag_integer_u64(
                auth_tag, key_params, data, len));
            break;
        /* BOOL */
        case KM_TAG_ROLLBACK_RESISTANCE:
        case KM_TAG_NO_AUTH_REQUIRED:
        case KM_TAG_ALLOW_WHILE_ON_BODY:
        case KM_TAG_TRUSTED_USER_PRESENCE_REQUIRED:
        case KM_TAG_TRUSTED_CONFIRMATION_REQUIRED:
        case KM_TAG_UNLOCKED_DEVICE_REQUIRED:
        case KM_TAG_CALLER_NONCE:
            EXPECT_KMOK_GOTOEND(authorization_list_serialize_tag_null(
                auth_tag, key_params, data, len));
            break;
        /* BYTES */
        case KM_TAG_ATTESTATION_APPLICATION_ID:
        case KM_TAG_ATTESTATION_ID_BRAND:
        case KM_TAG_ATTESTATION_ID_DEVICE:
        case KM_TAG_ATTESTATION_ID_PRODUCT:
        case KM_TAG_ATTESTATION_ID_SERIAL:
        case KM_TAG_ATTESTATION_ID_IMEI:
        case KM_TAG_ATTESTATION_ID_MEID:
        case KM_TAG_ATTESTATION_ID_MANUFACTURER:
        case KM_TAG_ATTESTATION_ID_MODEL:
            EXPECT_KMOK_GOTOEND(authorization_list_serialize_tag_octet_string(
                auth_tag, key_params, data, len));
            break;
        case KM_TAG_ROOT_OF_TRUST:
            /* RootOfTrust is used for Attest only */
            break;
        default:
            EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
                false);
    }

end:
    return ret;
}


/* AuthorizationList is sorted sequence as below
 * AuthorizationList ::= SEQUENCE {
 *     purpose                    [1] EXPLICIT SET OF INTEGER OPTIONAL,
 *     algorithm                  [2] EXPLICIT INTEGER OPTIONAL,
 *     keySize                    [3] EXPLICIT INTEGER OPTIONAL,
 *     blockMode                  [4] EXPLICIT SET OF INTEGER OPTIONAL,
 *     digest                     [5] EXPLICIT SET OF INTEGER OPTIONAL,
 *     padding                    [6] EXPLICIT SET OF INTEGER OPTIONAL,
 *     callerNonce                [7] EXPLICIT NULL OPTIONAL,
 *     minMacLength               [8] EXPLICIT INTEGER OPTIONAL,
 *     ecCurve                    [10] EXPLICIT INTEGER OPTIONAL,
 *     rsaPublicExponent          [200] EXPLICIT INTEGER OPTIONAL,
 *     rollbackResistance         [303] EXPLICIT NULL OPTIONAL,
 *     activeDateTime             [400] EXPLICIT INTEGER OPTIONAL,
 *     originationExpireDateTime  [401] EXPLICIT INTEGER OPTIONAL,
 *     usageExpireDateTime        [402] EXPLICIT INTEGER OPTIONAL,
 *     userSecureId               [502] EXPLICIT INTEGER OPTIONAL,
 *     noAuthRequired             [503] EXPLICIT NULL OPTIONAL,
 *     userAuthType               [504] EXPLICIT INTEGER OPTIONAL,
 *     authTimeout                [505] EXPLICIT INTEGER OPTIONAL,
 *     allowWhileOnBody           [506] EXPLICIT NULL OPTIONAL,
 *     trustedUserPresenceReq     [507] EXPLICIT NULL OPTIONAL,
 *     trustedConfirmationReq     [508] EXPLICIT NULL OPTIONAL,
 *     unlockedDeviceReq          [509] EXPLICIT NULL OPTIONAL,
 *     creationDateTime           [701] EXPLICIT INTEGER OPTIONAL,
 *     origin                     [702] EXPLICIT INTEGER OPTIONAL,
 *     rootOfTrust                [704] EXPLICIT RootOfTrust OPTIONAL,
 *     osVersion                  [705] EXPLICIT INTEGER OPTIONAL,
 *     osPatchLevel               [706] EXPLICIT INTEGER OPTIONAL,
 *     attestationApplicationId   [709] EXPLICIT OCTET_STRING OPTIONAL,
 *     attestationIdBrand         [710] EXPLICIT OCTET_STRING OPTIONAL,
 *     attestationIdDevice        [711] EXPLICIT OCTET_STRING OPTIONAL,
 *     attestationIdProduct       [712] EXPLICIT OCTET_STRING OPTIONAL,
 *     attestationIdSerial        [713] EXPLICIT OCTET_STRING OPTIONAL,
 *     attestationIdImei          [714] EXPLICIT OCTET_STRING OPTIONAL,
 *     attestationIdMeid          [715] EXPLICIT OCTET_STRING OPTIONAL,
 *     attestationIdManufacturer  [716] EXPLICIT OCTET_STRING OPTIONAL,
 *     attestationIdModel         [717] EXPLICIT OCTET_STRING OPTIONAL,
 *     vendorPatchLevel           [718] EXPLICIT INTEGER OPTIONAL,
 *     bootPatchLevel             [719] EXPLICIT INTEGER OPTIONAL,
 * }
 *
 */
static keymaster_error_t authorization_list_parse_authlist(
    uint32_t *params_num,
    size_t *required_len,
    const uint8_t *data, size_t remain_len)
{
    keymaster_error_t ret = KM_ERROR_OK;
    int tag;
    size_t tag_len;
    const uint8_t *pos = data;
    unsigned i = 0;
    keymaster_tag_t auth_tag;

    *params_num = 0;
    *required_len = 4; /* space for params_num */

    while (remain_len) {
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            der_parser_read_tag_with_length(&tag, &tag_len, &pos, &remain_len));

        /* find tag from AuthorizationList.
         * We can just increae index becuase of we assume that AuthorizationList is sorted
         */
        while (i < ATTEST_AUTH_NUMS) {
            /* if found tag, goto parsing */
            if (GET_TAG_ID(km4_attest_authlist[i]) == tag ) {
                break;
            }
            i++;
        }

        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            i < ATTEST_AUTH_NUMS);

        auth_tag = km4_attest_authlist[i];
        EXPECT_KMOK_GOTOEND(authorization_list_parse_asn1(
            auth_tag, params_num, required_len, pos, tag_len));
        pos += tag_len;
        remain_len -= tag_len;
    }

end:
    return ret;
}


static keymaster_error_t authorization_list_serialize_authlist(
    uint8_t *key_params,
    keymaster_algorithm_t *key_type,
    uint32_t *hw_auth_type,
    const uint8_t *data, size_t len)
{
    keymaster_error_t ret = KM_ERROR_OK;
    int tag;
    size_t tag_len;
    const uint8_t *pos = data;
    unsigned i = 0;
    keymaster_tag_t auth_tag;

    while (len) {
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            der_parser_read_tag_with_length(&tag, &tag_len, &pos, &len));

        while (i < ATTEST_AUTH_NUMS) {
            /* if found tag, goto serialization */
            if (GET_TAG_ID(km4_attest_authlist[i]) == tag) {
                break;
            }
            i++;
        }

        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            i < ATTEST_AUTH_NUMS);

        auth_tag = km4_attest_authlist[i];
        EXPECT_KMOK_GOTOEND(authorization_list_serialize_tag(
            auth_tag, &key_params, key_type, hw_auth_type, pos, tag_len));
        pos += tag_len;
        len -= tag_len;
    }

end:
    return ret;
}

keymaster_error_t ssp_keyparser_make_authorization_list(
    std::unique_ptr<uint8_t[]>& key_params_ptr,
    size_t *key_params_len,
    keymaster_algorithm_t *key_type,
    uint32_t *hw_auth_type,
    const uint8_t *data, size_t len)
{
    keymaster_error_t ret = KM_ERROR_OK;
    size_t seq_len;
    uint32_t params_num;
    const uint8_t *p = data;
    uint8_t *key_params;

    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
        der_parser_read_seq_length(&p, &len, &seq_len) && seq_len == len);

    /* cal required length for authorization lis */
    EXPECT_KMOK_GOTOEND(authorization_list_parse_authlist(
        &params_num, key_params_len, p, len));

    key_params_ptr.reset(new (std::nothrow) uint8_t[*key_params_len]);
    key_params = key_params_ptr.get();

    /* set the number of parameters to the serialized buffer */
    htob_u32(key_params, params_num);

    /* set serialized key parameters */
    EXPECT_KMOK_GOTOEND(authorization_list_serialize_authlist(
        key_params + 4, key_type, hw_auth_type, p, len));

end:
    return ret;
}

