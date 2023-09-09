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

#include <openssl/x509.h>
#include <assert.h>

#include "ssp_strongbox_keymaster_encode_key.h"
#include "ssp_strongbox_keymaster_common_util.h"

#include "ssp_strongbox_tee_api.h"

#define SSP_PUBKEY_BLOB_HDR_LEN (12)

static keymaster_error_t get_ec_nid(
    int *nid,
    ecc_curve_t ec_curve)
{
    switch (ec_curve) {
        case ecc_curve_nist_p_256:
            *nid = NID_X9_62_prime256v1;
            break;
        default:
            return KM_ERROR_INVALID_KEY_BLOB;
    }

    return KM_ERROR_OK;
}

/* Encoding ECC pub key using openssl */
static keymaster_error_t ssp_encode_ecc_pub_to_x509(
    uint32_t keysize,
    const uint8_t *pubkey_data,
    uint32_t pubkey_data_len,
    keymaster_blob_t *export_data)
{
    keymaster_error_t ret = KM_ERROR_OK;

    EC_GROUP *ec_group = NULL;
    EC_KEY *ec_key = NULL;
    EC_POINT *ec_point = NULL;
    BIGNUM *ec_point_x = NULL;
    BIGNUM *ec_point_y = NULL;
    EVP_PKEY *pubkey = NULL;
    ecc_curve_t ec_curve;
    uint32_t x_len, y_len;
    int encoded_len;
    int nid;
    unsigned char *pos = NULL;

    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_KEY_BLOB,
        pubkey_data_len >= SSP_PUBKEY_BLOB_HDR_LEN);

    ec_curve = (ecc_curve_t)btoh_u32(pubkey_data);
    x_len = btoh_u32(pubkey_data + 4);
    y_len = btoh_u32(pubkey_data + 8);
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_KEY_BLOB,
        (x_len <= BITS2BYTES(keysize)) &&
        (y_len <= BITS2BYTES(keysize)));

    EXPECT_KMOK_GOTOEND(get_ec_nid(&nid, ec_curve));

    ec_group = EC_GROUP_new_by_curve_name(nid);
    EXPECT_NE_NULL_GOTOEND(ec_group);
    ec_key = EC_KEY_new_by_curve_name(nid);
    EXPECT_NE_NULL_GOTOEND(ec_key);

    ec_point_x = BN_bin2bn(
                    pubkey_data + SSP_PUBKEY_BLOB_HDR_LEN,
                    x_len, NULL);
    EXPECT_NE_NULL_GOTOEND(ec_point_x);
    ec_point_y = BN_bin2bn(
                    pubkey_data + SSP_PUBKEY_BLOB_HDR_LEN + x_len,
                    y_len, NULL);
    EXPECT_NE_NULL_GOTOEND(ec_point_y);

    ec_point = EC_POINT_new(ec_group);
    EXPECT_NE_NULL_GOTOEND(ec_point);

    EXPECT_TRUE_GOTOEND(KM_ERROR_UNKNOWN_ERROR,
        EC_POINT_set_affine_coordinates_GFp(
            ec_group, ec_point,
            ec_point_x, ec_point_y,
            NULL));

    EXPECT_TRUE_GOTOEND(KM_ERROR_UNKNOWN_ERROR,
        EC_KEY_set_public_key(ec_key, ec_point));

    pubkey = EVP_PKEY_new();
    EXPECT_NE_NULL_GOTOEND(pubkey);

    EXPECT_TRUE_GOTOEND(KM_ERROR_UNKNOWN_ERROR,
        EVP_PKEY_set1_EC_KEY(pubkey, ec_key));

    encoded_len = i2d_PUBKEY(pubkey, NULL);
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_KEY_BLOB, encoded_len > 0);

    /* alloc buffer and encode export data */
    export_data->data = (uint8_t*)calloc(encoded_len, sizeof(uint8_t));
    EXPECT_NE_NULL_GOTOEND(export_data->data);

    pos = (unsigned char*)export_data->data;
    EXPECT_TRUE_GOTOEND(KM_ERROR_UNKNOWN_ERROR,
        i2d_PUBKEY(pubkey, &pos) == encoded_len);
    export_data->data_length = encoded_len;

end:
    if (ret != KM_ERROR_OK) {
        free((void*)export_data->data);
        export_data->data = NULL;
        export_data->data_length = 0;
    }

    /* free openssl context */
    if (pubkey)
        EVP_PKEY_free(pubkey);

    if (ec_point)
        EC_POINT_free(ec_point);

    if (ec_point_x)
        BN_free(ec_point_x);

    if (ec_point_y)
        BN_free(ec_point_y);

    if (ec_key)
        EC_KEY_free(ec_key);

    if (ec_group)
        EC_GROUP_free(ec_group);

    return ret;
}

/* Encoding RSA pub key using openssl */
static keymaster_error_t ssp_encode_rsa_pub_to_x509(
    uint32_t keysize,
    const uint8_t *pubkey_data,
    uint32_t pubkey_data_len,
    keymaster_blob_t *export_data)
{
    keymaster_error_t ret = KM_ERROR_OK;

    RSA *rsa_ctx = NULL;
    EVP_PKEY *pubkey = NULL;
    uint32_t mod_len;
    uint32_t exponent_len;
    int encoded_len;
    unsigned char *pos = NULL;

    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_KEY_BLOB,
        pubkey_data_len >= SSP_PUBKEY_BLOB_HDR_LEN);

    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_KEY_BLOB,
        keysize == btoh_u32(pubkey_data));
    mod_len = btoh_u32(pubkey_data + 4);
    exponent_len = btoh_u32(pubkey_data + 8);

    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_KEY_BLOB,
        (mod_len <= BITS2BYTES(keysize)) &&
        (exponent_len <= BITS2BYTES(keysize)));

    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_KEY_BLOB,
        pubkey_data_len >= SSP_PUBKEY_BLOB_HDR_LEN + mod_len + exponent_len);

    rsa_ctx = RSA_new();
    EXPECT_NE_NULL_GOTOEND(rsa_ctx);

    rsa_ctx->n = BN_bin2bn(
                    pubkey_data + SSP_PUBKEY_BLOB_HDR_LEN,
                    mod_len, NULL);
    EXPECT_NE_NULL_GOTOEND(rsa_ctx->n);

    rsa_ctx->e = BN_bin2bn(
                    pubkey_data + SSP_PUBKEY_BLOB_HDR_LEN + mod_len,
                    exponent_len, NULL);
    EXPECT_NE_NULL_GOTOEND(rsa_ctx->e);

    pubkey = EVP_PKEY_new();
    EXPECT_NE_NULL_GOTOEND(pubkey);
    EXPECT_TRUE_GOTOEND(KM_ERROR_UNKNOWN_ERROR,
        EVP_PKEY_set1_RSA(pubkey, rsa_ctx));

    encoded_len = i2d_PUBKEY(pubkey, NULL);
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_KEY_BLOB, encoded_len > 0);

    export_data->data = (uint8_t *)calloc(encoded_len, sizeof(uint8_t));
    EXPECT_NE_NULL_GOTOEND(export_data->data);

    pos = (unsigned char*)export_data->data;
    EXPECT_TRUE_GOTOEND(KM_ERROR_UNKNOWN_ERROR,
        i2d_PUBKEY(pubkey, &pos) == encoded_len);
    export_data->data_length = encoded_len;

end:
    if (ret != KM_ERROR_OK) {
        free((void*)export_data->data);
        export_data->data = NULL;
        export_data->data_length = 0;
    }

    /* free openssl context */
    if (pubkey)
        EVP_PKEY_free(pubkey);

    if (rsa_ctx)
        RSA_free(rsa_ctx);

    return ret;
}

static keymaster_error_t ssp_encode_pubkey_to_x509(
    keymaster_algorithm_t keytype,
    uint32_t keysize,
    const uint8_t *pubkey_data,
    uint32_t pubkey_data_len,
    keymaster_blob_t *export_data)
{
    switch (keytype) {
        case KM_ALGORITHM_EC:
            return ssp_encode_ecc_pub_to_x509(
                keysize,
                pubkey_data, pubkey_data_len,
                export_data);
        case KM_ALGORITHM_RSA:
            return ssp_encode_rsa_pub_to_x509(
                keysize,
                pubkey_data, pubkey_data_len,
                export_data);
        default:
            return KM_ERROR_INCOMPATIBLE_ALGORITHM;
    }
}

keymaster_error_t ssp_encode_pubkey(
    keymaster_algorithm_t keytype,
    uint32_t keysize,
    keymaster_key_format_t export_format,
    const uint8_t *exported_keyblob_raw,
    uint32_t exported_keyblob_len,
    keymaster_blob_t *exported_keyblob)
{
    keymaster_error_t ret = KM_ERROR_OK;

    EXPECT_NE_NULL_GOTOEND(exported_keyblob);

    exported_keyblob->data = NULL;
    exported_keyblob->data_length = 0;

    EXPECT_TRUE_GOTOEND(KM_ERROR_UNSUPPORTED_KEY_FORMAT,
        export_format == KM_KEY_FORMAT_X509);

    EXPECT_KMOK_GOTOEND(ssp_encode_pubkey_to_x509(
        keytype, keysize,
        exported_keyblob_raw, exported_keyblob_len,
        exported_keyblob));

end:
    return ret;
}


