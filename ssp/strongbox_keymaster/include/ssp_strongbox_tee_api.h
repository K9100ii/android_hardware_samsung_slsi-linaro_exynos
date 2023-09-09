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

#ifndef __STRONGBOX_TEE_API_H__
#define __STRONGBOX_TEE_API_H__

#include "ssp_strongbox_auth_token.h"

/* SSP command IDs for Keymaster operation */
#define CMD_SSP_GET_HMAC_SHARING_PARAMETERS     0x0001
#define CMD_SSP_COMPUTE_SHARED_HAMC             0x0002
#define CMD_SSP_VERIFY_AUTHORIZATION            0x0003
#define CMD_SSP_ADD_RNG_ENTROPY                 0x0004
#define CMD_SSP_GENERATE_KEY                    0x0005
#define CMD_SSP_GET_KEY_CHARACTERISTICS         0x0006
#define CMD_SSP_IMPORT_KEY                      0x0007
#define CMD_SSP_IMPORT_WRAPPED_KEY              0x0008
#define CMD_SSP_EXPORT_KEY                      0x0009
#define CMD_SSP_ATTEST_KEY                      0x000A
#define CMD_SSP_UPGRADE_KEY                     0x000B
#define CMD_SSP_DELETE_KEY                      0x000C
#define CMD_SSP_DELETE_ALL_KEYS                 0x000D
#define CMD_SSP_DESTROY_ATTESTATION_IDS         0x000E
#define CMD_SSP_BEGIN                           0x000F
#define CMD_SSP_UPDATE                          0x0010
#define CMD_SSP_FINISH                          0x0011
#define CMD_SSP_ABORT                           0x0012


/* SSP command IDs for misc operation */
#define CMD_SSP_SEND_SYSTEM_VERSION             0x1001
#define CMD_SSP_SET_ATTESTATION_DATA            0x1002
#define CMD_SSP_LOAD_ATTESTATION_DATA           0x1003

/* SSP IPC operation */
#define CMD_SSP_INIT_IPC                        0x2001

/* struct for privsion test */
#define CMD_SSP_SET_SAMPLE_ATTESTATION_DATA     0xF001
#define CMD_SSP_SET_DUMMY_SYSTEM_VERSION        0xF002


/* define Operation param index */
#define OP_PARAM_0 0
#define OP_PARAM_1 1
#define OP_PARAM_2 2
#define OP_PARAM_3 3

/**
 * |algorithm(4bytes)   |
 * |keysize(4bytes)     |
 * |keysize(4bytes)     |
 * |n_len(4bytes)       |
 * |e_len(4bytes)       |
 * |d_len(4bytes)       |
 * |p_len(4bytes)       |
 * |q_len(4bytes)       |
 * |dp_len(4bytes)      |
 * |dq_len(4bytes)      |
 * |qinv_len(4bytes)    |
 */
#define SSP_DER_KEYDATA_RSA_META_LEN 44

/* todo: need to define keyblob format */
#define SSP_KM_RSA_METADATA_SIZE (9 * 4)
#define SSP_KM_EC_METADATA_SIZE (4 * 4)


#define SSP_PARAMS_SUBSAMPLE_SIZE (1024 * 4)
#define SSP_DATA_SUBSAMPLE_SIZE (1024 * 4)

/* support up to 2048 bits HMAC key */
#define SSP_HAMC_MAX_KEY_SIZE (2048)

/* define AES block size */
#define SSP_AES_BLOCK_SIZE (16)


/**
 * metadata sizeof keyblob: it shoud be synced with SSPFW
 */
#define SSP_KEYBLOB_TAG_LEN		(16)
#define SSP_KEYBLOB_HEADER_LEN	(16)
#define SSP_KEYBLOB_META_SIZE_OF_BLOB	(SSP_KEYBLOB_TAG_LEN + \
					SSP_KEYBLOB_HEADER_LEN)

#define SSP_KEYBLOB_MATERIAL_HEADER_LEN (64)


/* todo: define more exact size */
/**
 * define params size
 */
#define KM_TAG_TYPE_SIZE            4

#define KM_TAG_ENUM_VALUE_SIZE      4
#define KM_TAG_ENUM_PARAM_SIZE      ((KM_TAG_TYPE_SIZE) + (KM_TAG_ENUM_VALUE_SIZE))

#define KM_TAG_ENUMREP_VALUE_SIZE   4
#define KM_TAG_ENUMREP_PARAM_SIZE   ((KM_TAG_TYPE_SIZE) + (KM_TAG_ENUMREP_VALUE_SIZE))

#define KM_TAG_UINT_VALUE_SIZE      4
#define KM_TAG_UINT_PARAM_SIZE      ((KM_TAG_TYPE_SIZE) + (KM_TAG_UINT_VALUE_SIZE))

#define KM_TAG_UINTREP_VALUE_SIZE   4
#define KM_TAG_UINTREP_PARAM_SIZE   ((KM_TAG_TYPE_SIZE) + (KM_TAG_UINTREP_VALUE_SIZE))

#define KM_TAG_BOOL_VALUE_SIZE      4
#define KM_TAG_BOOL_PARAM_SIZE      ((KM_TAG_TYPE_SIZE) + (KM_TAG_BOOL_VALUE_SIZE))

#define KM_TAG_ULONG_VALUE_SIZE     8
#define KM_TAG_ULONG_PARAM_SIZE     ((KM_TAG_TYPE_SIZE) + (KM_TAG_ULONG_VALUE_SIZE))

#define KM_TAG_ULONGREP_VALUE_SIZE  8
#define KM_TAG_ULONGREP_PARAM_SIZE  ((KM_TAG_TYPE_SIZE) + (KM_TAG_ULONGREP_VALUE_SIZE))

#define KM_TAG_DATE_VALUE_SIZE      8
#define KM_TAG_DATE_PARAM_SIZE      ((KM_TAG_TYPE_SIZE) + (KM_TAG_DATE_VALUE_SIZE))

#define KM_TAG_BYTE_LEN_SIZE        4


#define KM_TAG_PURPOSE_COUNT_MAX                    5
#define KM_TAG_BLOCK_MODE_COUNT_MAX                 4
#define KM_TAG_DIGEST_MODE_COUNT_MAX                5
#define KM_TAG_PADDING_MODE_COUNT_MAX               5
#define KM_TAG_SECURE_USER_ID_COUNT_MAX             5
#define KM_TAG_APPLICATION_ID_MAX_LEN               128
#define KM_TAG_APPLICATION_DATA_MAX_LEN             128
#define KM_TAG_ROOT_OF_TRUST_MAX_LEN                128
#define KM_TAG_UNIQUE_ID_MAX_LEN                    128
#define KM_TAG_ATTESTATION_CHALLENGE_MAX_LEN        256
#define KM_TAG_ATTESTATION_APPLICATION_ID_MAX_LEN   (1024 + 128)
#define KM_TAG_ATTESTATION_ID_BRAND_MAX_LEN         128
#define KM_TAG_ATTESTATION_ID_DEVICE_MAX_LEN        128
#define KM_TAG_ATTESTATION_ID_PRODUCT_MAX_LEN       128
#define KM_TAG_ATTESTATION_ID_SERIAL_MAX_LEN        128
#define KM_TAG_ATTESTATION_ID_IMEI_MAX_LEN          32
#define KM_TAG_ATTESTATION_ID_MEID_MAX_LEN          32
#define KM_TAG_ATTESTATION_ID_MANUFACTURER_MAX_LEN  128
#define KM_TAG_ATTESTATION_ID_MODEL_MAX_LEN         128
#define KM_TAG_ASSOCIATED_DATA_MAX_LEN              (SSP_PARAMS_SUBSAMPLE_SIZE)
#define KM_TAG_NONCE_MAX_LEN                        128
#define KM_TAG_CONFIRMATION_TOKEN_MAX_LEN           128

/* define serialzed key param size */
/* TAG_TYPE(4bytes) | byte len(4bytes) | n bytes */
#define KM_TAG_BYTE_PARAM_SIZE(byte_len)    ((KM_TAG_TYPE_SIZE) +\
                                        (KM_TAG_BYTE_LEN_SIZE) +\
                                        (byte_len))

#define KM_KEY_PARAM_PURPOSE_LEN    ((KM_TAG_ENUM_PARAM_SIZE) *\
                                        (KM_TAG_PURPOSE_COUNT_MAX))
#define KM_KEY_PARAM_ALGORITHM_LEN  (KM_TAG_ENUM_PARAM_SIZE)
#define KM_KEY_PARAM_KEY_SIZE_LEN   (KM_TAG_UINT_PARAM_SIZE)
#define KM_KEY_PARAM_BLOCK_MODE_LEN ((KM_TAG_ENUMREP_PARAM_SIZE) *\
                                        (KM_TAG_BLOCK_MODE_COUNT_MAX))
#define KM_EKY_PARAM_DIGEST_LEN     ((KM_TAG_ENUMREP_PARAM_SIZE) *\
                                        (KM_TAG_DIGEST_MODE_COUNT_MAX))
#define KM_EKY_PARAM_PADDING_LEN    ((KM_TAG_ENUMREP_PARAM_SIZE) *\
                                        (KM_TAG_PADDING_MODE_COUNT_MAX))
#define KM_KEY_PARAM_CALLER_NONCE_LEN   (KM_TAG_BOOL_PARAM_SIZE)
#define KM_KEY_PARAM_MIN_MAC_LENGTH_LEN (KM_TAG_UINT_PARAM_SIZE)
#define KM_KEY_PARAM_EC_CURVE_LEN       (KM_TAG_ENUM_PARAM_SIZE)
#define KM_KEY_PARAM_RSA_PUBLIC_EXPONENT_LEN        (KM_TAG_ULONG_PARAM_SIZE)
#define KM_KEY_PARAM_INCLUDE_UNIQUE_ID_LEN          (KM_TAG_BOOL_PARAM_SIZE)
#define KM_KEY_PARAM_BLOB_USAGE_REQUIREMENTS_LEN    (KM_TAG_ENUM_PARAM_SIZE)
#define KM_KEY_PARAM_BOOTLOADER_ONLY_LEN        (KM_TAG_BOOL_PARAM_SIZE)
#define KM_KEY_PARAM_ROLLBACK_RESISTANCE_LEN    (KM_TAG_BOOL_PARAM_SIZE)
#define KM_KEY_PARAM_HARDWARE_TYPE_LEN          (KM_TAG_ENUM_PARAM_SIZE)
#define KM_KEY_PARAM_ACTIVE_DATETIME_LEN        (KM_TAG_DATE_PARAM_SIZE)
#define KM_KEY_PARAM_ORIGINATION_EXPIRE_DATETIME_LEN    (KM_TAG_DATE_PARAM_SIZE)
#define KM_KEY_PARAM_USAGE_EXPIRE_DATETIME_LEN          (KM_TAG_DATE_PARAM_SIZE)
#define KM_KEY_PARAM_MIN_SECONDS_BETWEEN_OPS_LEN        (KM_TAG_UINT_PARAM_SIZE)
#define KM_KEY_PARAM_MAX_USES_PER_BOOT_LEN              (KM_TAG_UINT_PARAM_SIZE)
#define KM_KEY_PARAM_USER_ID_LEN                        (KM_TAG_UINT_PARAM_SIZE)
#define KM_KEY_PARAM_USER_SECURE_ID_LEN     ((KM_TAG_ULONGREP_PARAM_SIZE) *\
                                            (KM_TAG_SECURE_USER_ID_COUNT_MAX))
#define KM_KEY_PARAM_NO_AUTH_REQUIRED_LEN   (KM_TAG_BOOL_PARAM_SIZE)
#define KM_KEY_PARAM_USER_AUTH_TYPE_LEN     (KM_TAG_ENUM_PARAM_SIZE)
#define KM_KEY_PARAM_AUTH_TIMEOUT_LEN       (KM_TAG_UINT_PARAM_SIZE)
#define KM_KEY_PARAM_ALLOW_WHILE_ON_BODY_LEN    (KM_TAG_BOOL_PARAM_SIZE)
#define KM_KEY_PARAM_TRUSTED_USER_PRESENCE_REQUIRED_LEN     (KM_TAG_BOOL_PARAM_SIZE)
#define KM_KEY_PARAM_TRUSTED_CONFIRMATION_REQUIRED_LEN      (KM_TAG_BOOL_PARAM_SIZE)
#define KM_KEY_PARAM_UNLOCKED_DEVICE_REQUIRED_LEN           (KM_TAG_BOOL_PARAM_SIZE)
#define KM_KEY_PARAM_APPLICATION_ID_LEN      (KM_TAG_BYTE_PARAM_SIZE(\
                                            KM_TAG_APPLICATION_ID_MAX_LEN))
#define KM_KEY_PARAM_APPLICATION_DATA_LEN       (KM_TAG_BYTE_PARAM_SIZE(\
                                                KM_TAG_APPLICATION_DATA_MAX_LEN))
#define KM_KEY_PARAM_CREATION_DATETIME_LEN      (KM_TAG_DATE_PARAM_SIZE)
#define KM_KEY_PARAM_ORIGIN_LEN                 (KM_TAG_ENUM_PARAM_SIZE)
#define KM_KEY_PARAM_ROOT_OF_TRUST_LEN          (KM_TAG_BYTE_PARAM_SIZE(\
                                                KM_TAG_ROOT_OF_TRUST_MAX_LEN))
#define KM_KEY_PARAM_OS_VERSION_LEN         (KM_TAG_UINT_PARAM_SIZE)
#define KM_KEY_PARAM_OS_PATCHLEVEL_LEN      (KM_TAG_UINT_PARAM_SIZE)
#define KM_KEY_PARAM_UNIQUE_ID_LEN      (KM_TAG_BYTE_PARAM_SIZE(\
                                        KM_TAG_UNIQUE_ID_MAX_LEN))
#define KM_KEY_PARAM_ATTESTATION_CHALLENGE_LEN  (KM_TAG_BYTE_PARAM_SIZE(\
                                                KM_TAG_ATTESTATION_CHALLENGE_MAX_LEN))
#define KM_KEY_PARAM_ATTESTATION_APPLICATION_ID_LEN  (KM_TAG_BYTE_PARAM_SIZE(\
                                                    KM_TAG_ATTESTATION_APPLICATION_ID_MAX_LEN))
#define KM_KEY_PARAM_ATTESTATION_ID_BRAND_LEN   (KM_TAG_BYTE_PARAM_SIZE(\
                                                KM_TAG_ATTESTATION_ID_BRAND_MAX_LEN))
#define KM_KEY_PARAM_ATTESTATION_ID_DEVICE_LEN  (KM_TAG_BYTE_PARAM_SIZE(\
                                                KM_TAG_ATTESTATION_ID_DEVICE_MAX_LEN))
#define KM_KEY_PARAM_ATTESTATION_ID_PRODUCT_LEN (KM_TAG_BYTE_PARAM_SIZE(\
                                                KM_TAG_ATTESTATION_ID_PRODUCT_MAX_LEN))
#define KM_KEY_PARAM_ATTESTATION_ID_SERIAL_LEN  (KM_TAG_BYTE_PARAM_SIZE(\
                                                KM_TAG_ATTESTATION_ID_SERIAL_MAX_LEN))
#define KM_KEY_PARAM_ATTESTATION_ID_IMEI_LEN    (KM_TAG_BYTE_PARAM_SIZE(\
                                                KM_TAG_ATTESTATION_ID_IMEI_MAX_LEN))
#define KM_KEY_PARAM_ATTESTATION_ID_MEID_LEN    (KM_TAG_BYTE_PARAM_SIZE(\
                                                KM_TAG_ATTESTATION_ID_MEID_MAX_LEN))
#define KM_KEY_PARAM_ATTESTATION_ID_MANUFACTURER_LEN    (KM_TAG_BYTE_PARAM_SIZE(\
                                                        KM_TAG_ATTESTATION_ID_MANUFACTURER_MAX_LEN))
#define KM_KEY_PARAM_ATTESTATION_ID_MODEL_LEN   (KM_TAG_BYTE_PARAM_SIZE(\
                                                KM_TAG_ATTESTATION_ID_MODEL_MAX_LEN))
#define KM_KEY_PARAM_VENDOR_PATCHLEVEL_LEN      (KM_TAG_UINT_PARAM_SIZE)
#define KM_KEY_PARAM_BOOT_PATCHLEVEL_LEN        (KM_TAG_UINT_PARAM_SIZE)
#define KM_KEY_PARAM_ASSOCIATED_DATA_LEN        (KM_TAG_BYTE_PARAM_SIZE(\
                                                KM_TAG_ASSOCIATED_DATA_MAX_LEN))
#define KM_KEY_PARAM_NONCE_LEN  (KM_TAG_BYTE_PARAM_SIZE(\
                                KM_TAG_NONCE_MAX_LEN))
#define KM_KEY_PARAM_MAC_LENGTH_LEN                 (KM_TAG_UINT_PARAM_SIZE)
#define KM_KEY_PARAM_RESET_SINCE_ID_ROTATION_LEN    (KM_TAG_BOOL_PARAM_SIZE)
#define KM_KEY_PARAM_CONFIRMATION_TOKEN_LEN (KM_TAG_BYTE_PARAM_SIZE(\
                                            KM_TAG_CONFIRMATION_TOKEN_MAX_LEN))


/**
 * All key params max length is about 7KB + a.
 * total key params size is defined as
 * (key params max lenght + reserved + 1KB align)
 */
#define KM_KEY_PARAMS_TOTAL_MAX_LEN     (8 * 1024)

/* todo: need to remove */
#if 0
#define KM_KEY_MATERIAL_PARAM_LEN	4
#define KM_KEY_MATERIAL_TYPE_LEN	4
#define KM_KEY_MATERIAL_SIZE_LEN	4
#define KM_KEY_MATERIAL_HEADER_LEN(params_len) (KM_KEY_MATERIAL_PARAM_LEN + \
						params_len + \
						KM_KEY_MATERIAL_TYPE_LEN + \
						KM_KEY_MATERIAL_SIZE_LEN)
#endif
/* todo: need to remove: end */

/**
 * size of out_params buffer
 * (param_count | tag | blob_length | blob_data(16bytes)).
 */
#define SSP_BEGIN_OUT_PARAMS_SIZE (4 + 4 + 4 + 16)


#define SSP_VERSION_INFO_PARAMS_SIZE ((KM_KEY_PARAM_OS_VERSION_LEN) +\
            (KM_KEY_PARAM_OS_PATCHLEVEL_LEN) +\
            (KM_KEY_PARAM_VENDOR_PATCHLEVEL_LEN) +\
            (KM_KEY_PARAM_BOOT_PATCHLEVEL_LEN))

/* currently only consider TEE and Strongbox keymaster */
#define SSP_HMAC_SHARING_PARAM_MAX_CNT 2
#define SSP_HMAC_SHARING_NONCE_SIZE 32
#define SSP_HMAC_SHARING_SEED_SIZE  32
#define SSP_HMAC_SHARING_CHECK_SIZE 32

#define SSP_ADD_RNG_ENTROPY_MAX_LEN (KM_ADD_RNG_ENTROPY_MAX_LEN)

/* key characteristic does not include aad data.
 * But aad data buffer is too big.
 * So, remove aad data len from characteristics length
 * to reduce shared buffer length.
 */
/* todo: remove more unused key params? */
#define SSP_KEY_CHARACTERISTICS_COMMON_MAX_LEN ((KM_KEY_PARAMS_TOTAL_MAX_LEN) -\
            (KM_KEY_PARAM_ASSOCIATED_DATA_LEN) -\
            (KM_KEY_PARAM_ATTESTATION_CHALLENGE_LEN) -\
            (KM_KEY_PARAM_ATTESTATION_APPLICATION_ID_LEN) -\
            (KM_KEY_PARAM_ATTESTATION_ID_BRAND_LEN) -\
            (KM_KEY_PARAM_ATTESTATION_ID_SERIAL_LEN) -\
            (KM_KEY_PARAM_ATTESTATION_ID_IMEI_LEN) -\
            (KM_KEY_PARAM_ATTESTATION_ID_MEID_LEN) -\
            (KM_KEY_PARAM_ATTESTATION_ID_MANUFACTURER_LEN) -\
            (KM_KEY_PARAM_ATTESTATION_ID_MODEL_LEN))

#define SSP_KEY_CHARACTERISTICS_OPERATION_MAX_LEN ((KM_KEY_PARAMS_TOTAL_MAX_LEN) -\
            (KM_KEY_PARAM_ATTESTATION_CHALLENGE_LEN) -\
            (KM_KEY_PARAM_ATTESTATION_APPLICATION_ID_LEN) -\
            (KM_KEY_PARAM_ATTESTATION_ID_BRAND_LEN) -\
            (KM_KEY_PARAM_ATTESTATION_ID_SERIAL_LEN) -\
            (KM_KEY_PARAM_ATTESTATION_ID_IMEI_LEN) -\
            (KM_KEY_PARAM_ATTESTATION_ID_MEID_LEN) -\
            (KM_KEY_PARAM_ATTESTATION_ID_MANUFACTURER_LEN) -\
            (KM_KEY_PARAM_ATTESTATION_ID_MODEL_LEN))

#define SSP_KEY_CHARACTERISTICS_ATTEST_MAX_LEN ((KM_KEY_PARAMS_TOTAL_MAX_LEN) -\
            (KM_KEY_PARAM_ASSOCIATED_DATA_LEN))


/* todo: define more exact keyblob max len */
/* currently considering RSA 2048 as largest key size
 * So, basic key blob max size length is,
 * 8 * RSA 2048 Keysize + //2KB
 * max input key params + //SSP_KEY_CHARACTERISTICS_MAX_LEN := 4KB
 * extra data(keyblob metadata, own params, ...) //shoule be under 1KB
 */
/* RSA key includes 8 params: n, e, d, p, q, dp, dq, qinv */
#define SSP_SUPPORTED_ALGORITH_MAX_KEY_LEN   (((STRONGBOX_RSA_MAX_KEY_SIZE) / 8) * 8)
/* key blob max size consider attest key blob */
#define SSP_KEY_BLOB_MAX_LEN        ((SSP_SUPPORTED_ALGORITH_MAX_KEY_LEN) +\
                                (SSP_KEY_CHARACTERISTICS_ATTEST_MAX_LEN) + 1024)

/* currently considering RSA 2048 as largest key size
 * So, the parsed max keysize is based on RSA key data + meta data size
 */
#define SSP_PARSED_KEY_DATA_MAX_LEN ((SSP_SUPPORTED_ALGORITH_MAX_KEY_LEN) + 1024)

/* consider RSA 2048 and below key data
 * type | size | size | n_len | e_len | n | e + reserve(512 bytes)
 */
#define SSP_EXPORTED_PUBKEY_MAX_LEN ((5 * 4) + (2 * (2048 / 8)) + 512)

/* reserved output buffer TAG value of GCM */
#define SSP_RESERVED_BUF_FOR_TAG_LEN    16

/* consider signature max len as 512Bytes */
#define SSP_FINISH_SIGNATURE_MAX_LEN    512

/* todo: define ssp key data length */
#define SSP_SERIALIZED_KEY_LEN_MIN      8

/* todo: need optimize
 * consider RSA2048 + EC P-256 Key size
 * attestation key is
 * RSA private(max 2048) +
 * RSA cert1(max 2048) +
 * RSA cert2(max 2048) +
 * RSA root cert(max 2048)
 * EC private(1024) +
 * EC cert1(1024) +
 * EC cert2(1024) +
 * EC root cert(max 2048)
 */
#define SSP_ATTESTATION_EC_DATA_MAX_LEN ((1024 * 3) + (2048 * 1))
#define SSP_ATTESTATION_RSA_DATA_MAX_LEN (2048 * 4)
#define SSP_ATTESTATION_DATA_MAX_LEN (\
    (SSP_ATTESTATION_EC_DATA_MAX_LEN) +\
	(SSP_ATTESTATION_RSA_DATA_MAX_LEN))

#define SSP_WRAP_HEADER_LEN 4
#define SSP_WRAP_TAIL_LEN   4

/* todo: need optimize
 * consider RSA2048
 * attestation key is
 * RSA cert1(max 2048) +
 * RSA cert2(max 2048) +
 * RSA cert3(max 2048) +
 * RSA root cert(max 2048)
 */
#define SSP_ATTEST_CERT_CHAIN_MAX_LEN (2048 * 4)

/* it is defined arbitrary */
#define SSP_VERIFICATION_TOKEN_PARAM_MAX_LEN (512)

/* Encrypted Transport Key is encrypted with RSA OAEP
 * Max encrypted data size is based on RSA 2048 + reserved 256 bytes
 */
#define SSP_IMPORTWKEY_ENC_TRANS_KEY_MAX_LEN (((STRONGBOX_RSA_MAX_KEY_SIZE) / 8) + 256)

/* consider AES GCM 256 bit */
#define SSP_IMPORTWKEY_IV_MAX_LEN   (32)
#define SSP_IMPORTWKEY_TAG_MAX_LEN  (32)
#define SSP_IMPORTWKEY_MASKING_KEY_MAX_LEN  (32)
#define SSP_IMPORTWKEY_UNWRAPPING_PARAMS_MAX_LEN    (SSP_KEY_CHARACTERISTICS_COMMON_MAX_LEN)


typedef struct __attribute__((packed, aligned(4))) {
    uint32_t magic;
    uint32_t version;
    uint8_t data[32]; /* reserved */
} header_t;

/**
 * Command message.
 */
typedef struct __attribute__((packed, aligned(4))) {
    uint32_t id;
    uint8_t data[32]; /* reserved */
} command_t;


/**
 * Response structure
 */
typedef struct __attribute__((packed, aligned(4))) {
    int km_errcode;
	uint32_t sspkm_errno;
	uint32_t cm_errno;
    uint8_t data[32]; /* reserved */
} response_t;

/**
 * type of index for extra parameter
 */
typedef uint32_t op_param_t;

/**
 * init ipc with ssp
 */
typedef struct __attribute__((packed, aligned(4))) {
    uint8_t reserved[1024];   /* in */
} init_ssp_ipc_t;

/* NOTE: need 8bytes align for uint64_t variables */
typedef struct __attribute__((packed, aligned(8))) {
    /* version is changed to uint32_t to meet 4bytes align */
    uint32_t version;
    uint64_t challenge;
    uint64_t timestamp;
    uint32_t params_verified_len;
    uint8_t params_verified[SSP_VERIFICATION_TOKEN_PARAM_MAX_LEN];
    keymaster_security_level_t security_level;
    uint8_t mac[32];
} hw_verification_token_t;

/**
 * send_system_version_t structure
 */
typedef struct __attribute__((packed, aligned(4))) {
    uint32_t system_version_len;                            /* in */
    uint8_t system_version[SSP_VERSION_INFO_PARAMS_SIZE];   /* in */
    uint8_t reserved[64];                                   /* inout */
} send_system_version_t;

/**
 * get_hmac_sharing_parameters_t structure
 */
typedef struct __attribute__((packed, aligned(4))) {
    uint32_t seed_len;                          /* out */
    uint32_t nonce_len;                         /* out */
    uint8_t seed[SSP_HMAC_SHARING_SEED_SIZE];   /* out */
    uint8_t nonce[SSP_HMAC_SHARING_NONCE_SIZE]; /* out */
    uint8_t reserved[64];                       /* inout */
} get_hmac_sharing_parameters_t;

typedef struct __attribute__((packed, aligned(4))) {
    uint32_t seed_len;                                  /* in */
    uint32_t nonce_len;                                 /* in */
    uint8_t seed[SSP_HMAC_SHARING_SEED_SIZE];           /* in */
    uint8_t nonce[SSP_HMAC_SHARING_NONCE_SIZE];         /* in */
    uint8_t reserved[64];                               /* inout */
} shared_hmac_param_t;

/**
 * compute_shared_hmac_t structure
 */
typedef struct __attribute__((packed, aligned(4))) {
    uint32_t params_cnt;
    uint32_t sharing_check_len;                                 /* out */
    shared_hmac_param_t params[SSP_HMAC_SHARING_PARAM_MAX_CNT]; /* in */
    uint8_t sharing_check[SSP_HMAC_SHARING_CHECK_SIZE];         /* out */
    uint8_t reserved[64];                                       /* inout */
} compute_shared_hmac_t;

/**
 * add_rng_entropy_t structure
 */
typedef struct __attribute__((packed, aligned(4))) {
    uint32_t rng_entropy_len;                           /* in */
    uint8_t rng_entropy[SSP_ADD_RNG_ENTROPY_MAX_LEN];   /* in */
    uint8_t reserved[64];                               /* inout */
} add_rng_entropy_t;

/**
 * generate_key_t structure
 */
typedef struct __attribute__((packed, aligned(4))) {
    uint32_t algorithm;                                         /* in */
    uint32_t keysize;                                           /* in */
    uint32_t keyoption;						/* in */
    uint32_t keyparams_len;                                     /* in */
    uint32_t keyblob_len;                                       /* out */
    uint32_t keychar_len;                                       /* out */
    uint8_t keyparams[SSP_KEY_CHARACTERISTICS_COMMON_MAX_LEN];  /* in */
    uint8_t keyblob[SSP_KEY_BLOB_MAX_LEN];                      /* out */
	uint8_t keychar[SSP_KEY_CHARACTERISTICS_COMMON_MAX_LEN];    /* out */
    uint8_t reserved[64];                                       /* inout */
} generate_key_t;

/**
 * get_key_characteristics_t structure
 */
typedef struct __attribute__((packed, aligned(4))) {
    uint32_t keyblob_len;                                       /* in */
    uint32_t client_id_len;                                     /* in */
    uint32_t app_data_len;                                      /* in */
    uint32_t keychar_len;                                       /* out */
    uint8_t keyblob[SSP_KEY_BLOB_MAX_LEN];                      /* in */
    uint8_t client_id[KM_TAG_APPLICATION_ID_MAX_LEN];           /* in */
    uint8_t app_data[KM_TAG_APPLICATION_DATA_MAX_LEN];          /* in */
    uint8_t keychar[SSP_KEY_CHARACTERISTICS_COMMON_MAX_LEN];    /* out */
    uint8_t reserved[64];                                       /* inout */
} get_key_characteristics_t;

/**
 * import_key_t structure
 */
typedef struct __attribute__((packed, aligned(4))) {
    uint32_t keyparams_len;                                     /* in */
    uint32_t keydata_len;                                       /* in */
    uint32_t keyblob_len;                                       /* out */
    uint32_t keychar_len;                                       /* out */
    uint8_t keyparams[SSP_KEY_CHARACTERISTICS_COMMON_MAX_LEN];  /* in */
    uint8_t keydata[SSP_PARSED_KEY_DATA_MAX_LEN];               /* in */
    uint8_t keyblob[SSP_KEY_BLOB_MAX_LEN];                      /* out */
    uint8_t keychar[SSP_KEY_CHARACTERISTICS_COMMON_MAX_LEN];    /* out */
    uint8_t reserved[64];                                       /* inout */
} import_key_t;

/**
 * import_wrappedkey_t structure
 * about 25KB
 */
typedef struct __attribute__((packed, aligned(8))) {
    uint32_t keyparams_len;                                                 /* in */
    uint32_t encrypted_transport_key_len;                                   /* in */
    uint32_t key_description_len;                                           /* in */
    uint32_t encrypted_key_len;                                             /* in */
    uint32_t iv_len;                                                        /* in */
    uint32_t tag_len;                                                       /* in */
    uint32_t masking_key_len;                                               /* in */
    uint32_t wrapping_keyblob_len;                                          /* in */
    uint32_t unwrapping_params_blob_len;                                    /* in */
    uint32_t keyblob_len;                                                   /* out */
    uint32_t keychar_len;                                                   /* out */
    keymaster_key_format_t keyformat;                                       /* in */
    keymaster_algorithm_t keytype;                                          /* in */
    uint64_t pwd_sid;                                                       /* in */
    uint64_t bio_sid;                                                       /* in */
    uint8_t keyparams[SSP_KEY_CHARACTERISTICS_COMMON_MAX_LEN];              /* in */
    uint8_t encrypted_transport_key[SSP_IMPORTWKEY_ENC_TRANS_KEY_MAX_LEN];  /* in */
    uint8_t key_description[SSP_KEY_CHARACTERISTICS_COMMON_MAX_LEN];        /* in */
    uint8_t encrypted_key[SSP_SUPPORTED_ALGORITH_MAX_KEY_LEN];              /* in */
    uint8_t iv[SSP_IMPORTWKEY_IV_MAX_LEN];                                  /* in */
    uint8_t tag[SSP_IMPORTWKEY_TAG_MAX_LEN];                                /* in */
    uint8_t masking_key[SSP_IMPORTWKEY_MASKING_KEY_MAX_LEN];                /* in */
    uint8_t wrapping_keyblob[SSP_KEY_BLOB_MAX_LEN];                         /* in */
    uint8_t unwrapping_params_blob[SSP_KEY_CHARACTERISTICS_COMMON_MAX_LEN]; /* in */
    uint8_t keyblob[SSP_KEY_BLOB_MAX_LEN];                                  /* out */
    uint8_t keychar[SSP_KEY_CHARACTERISTICS_COMMON_MAX_LEN];                /* out */
    uint8_t reserved[64];                                                   /* inout */
} import_wrappedkey_t;


/**
 * export_key_t structure
 */
typedef struct __attribute__((packed, aligned(4))) {
    uint32_t keyblob2export_len;                            /* in */
    uint32_t client_id_len;                                 /* in */
    uint32_t app_data_len;                                  /* in */
    uint32_t exported_keyblob_len;                          /* out */
    keymaster_algorithm_t keytype;                          /* out */
    uint32_t keysize;                                       /* out */
    uint8_t keyblob2export[SSP_KEY_BLOB_MAX_LEN];           /* in */
    uint8_t client_id[KM_TAG_APPLICATION_ID_MAX_LEN];       /* in */
    uint8_t app_data[KM_TAG_APPLICATION_DATA_MAX_LEN];      /* in */
    uint8_t exported_keyblob[SSP_EXPORTED_PUBKEY_MAX_LEN];  /* out */
    uint8_t reserved[64];                                       /* inout */
} export_key_t;

/* NOTE: need 8bytes align for uint64_t variables */
typedef struct __attribute__((packed, aligned(8))) {
    keymaster_purpose_t purpose;                                    /* in */
    hw_auth_token_t auth_token __attribute__((aligned(8)));        /* in */
    uint32_t authtoken_info;					                    /* in */
    uint64_t timestamp;					                            /* in */
    uint32_t in_params_len;                                         /* in */
    uint32_t keyblob_len;                                           /* in */
    uint32_t out_params_len;                                        /* inout */
    keymaster_operation_handle_t op_handle;                         /* out */
    keymaster_algorithm_t algorithm;                                /* out */
    uint32_t final_len;                                             /* out */
    uint8_t in_params[SSP_KEY_CHARACTERISTICS_OPERATION_MAX_LEN];   /* in */
    uint8_t keyblob[SSP_KEY_BLOB_MAX_LEN];                          /* in */
    uint8_t out_params[SSP_BEGIN_OUT_PARAMS_SIZE];                  /* out */
    uint8_t reserved[64];                                       /* inout */
} begin_t;

/* NOTE: need 8bytes align for uint64_t variables */
typedef struct __attribute__((packed, aligned(8))) {
    keymaster_operation_handle_t op_handle;                         /* in */
    hw_auth_token_t auth_token __attribute__((aligned(8)));        /* in */
    hw_verification_token_t verification_token;                     /* in */
    uint32_t authtoken_info;					                    /* in */
    uint32_t in_params_len;                                         /* in */
    uint32_t input_len;                                             /* in */
    uint32_t output_len;                                            /* out */
    uint32_t output_result_len;                                     /* out */
    uint32_t input_consumed_len;                                    /* out */
    uint8_t in_params[SSP_KEY_CHARACTERISTICS_OPERATION_MAX_LEN];   /* in */
    /* 128 bytes are reserved and bumper */
    uint8_t input[SSP_DATA_SUBSAMPLE_SIZE + 128];                   /* in */
    /**
     * 16 byte is for tag value of GCM mode
     * 128 bytes are reserved and bumper
     */
    uint8_t output[(SSP_DATA_SUBSAMPLE_SIZE) +\
                (SSP_RESERVED_BUF_FOR_TAG_LEN) + 128];              /* out */
    uint8_t reserved[64];                                           /* inout */
} update_t;

/* NOTE: need 8bytes align for uint64_t variables */
typedef struct __attribute__((packed, aligned(8))) {
    keymaster_operation_handle_t op_handle;                         /* in */
    hw_auth_token_t auth_token __attribute__((aligned(8)));        /* in */
    hw_verification_token_t verification_token;                     /* in */
    uint32_t authtoken_info;					                    /* in */
    uint32_t in_params_len;                                         /* in */
    uint32_t input_len;                                             /* in */
    uint32_t signature_len;                                         /* in */
    uint32_t input_consumed_len;                                    /* out */
    uint32_t output_len;                                            /* out */
    uint32_t output_result_len;                                     /* out */
    uint8_t in_params[SSP_KEY_CHARACTERISTICS_OPERATION_MAX_LEN];   /* in */
    uint8_t signature[SSP_FINISH_SIGNATURE_MAX_LEN];                /* in */
    /* 128 bytes are reserved and bumper */
    uint8_t input[SSP_DATA_SUBSAMPLE_SIZE + 128];                   /* in */
    /**
     * 16 byte is for tag value of GCM mode
     * 128 bytes are reserved and bumper
     */
    uint8_t output[(SSP_DATA_SUBSAMPLE_SIZE) +\
                (SSP_RESERVED_BUF_FOR_TAG_LEN) + 128];              /* out */
    uint8_t reserved[64];                                           /* inout */
} finish_t;

/* NOTE: need 8bytes align for uint64_t variables */
typedef struct __attribute__((packed, aligned(8))) {
    keymaster_operation_handle_t op_handle; /* in */
    uint8_t reserved[64];                                       /* inout */
} abort_t;

typedef struct __attribute__((packed, aligned(4))) {
    uint32_t attestation_data_len;   /* inout */
    uint32_t attestation_blob_len;   /* inout */
    uint8_t attestation_data[SSP_ATTESTATION_DATA_MAX_LEN];
    uint8_t reserved[64];                                       /* inout */
} set_attestation_data_t;

typedef struct __attribute__((packed, aligned(4))) {
    uint32_t attestation_blob_len;                          /* in */
    uint8_t attestation_blob[SSP_ATTESTATION_DATA_MAX_LEN]; /* in */
    uint8_t reserved[64];                                   /* inout */
} load_attestation_data_t;

/**
 * attest_key_t structure
 */
typedef struct __attribute__((packed, aligned(4))) {
    uint32_t keyblob_len;                       /* in */
    uint32_t keyparams_len;                     /* in */
    uint32_t cert_chain_len;                    /* inout */
    uint32_t attestation_blob_len;              /* in */
    uint8_t attestation_blob[SSP_ATTESTATION_DATA_MAX_LEN];     /* in */
    uint8_t keyblob[SSP_KEY_BLOB_MAX_LEN];                      /* in */
    uint8_t keyparams[SSP_KEY_CHARACTERISTICS_COMMON_MAX_LEN];  /* out */
    uint8_t cert_chain[SSP_ATTEST_CERT_CHAIN_MAX_LEN];          /* out */
    uint8_t reserved[64];                                       /* inout */
} attest_key_t;

/**
 * upgrade_key_t structure
 */
typedef struct __attribute__((packed, aligned(4))) {
    uint32_t keyblob_len;                       /* in */
    uint32_t keyparams_len;                     /* in */
    uint32_t upgraded_keyblob_len;              /* out */
    uint8_t keyblob[SSP_KEY_BLOB_MAX_LEN];                      /* in */
    uint8_t keyparams[SSP_KEY_CHARACTERISTICS_COMMON_MAX_LEN];  /* in */
    uint8_t upgraded_keyblob[SSP_KEY_BLOB_MAX_LEN];             /* out */
    uint8_t reserved[64];                                       /* inout */
} upgrade_key_t;


/* struct for privsion test */
typedef struct __attribute__((packed, aligned(4))) {
    uint32_t attestation_blob_len;   /* out */
    uint8_t attestation_blob[SSP_ATTESTATION_DATA_MAX_LEN]; /* out */
    uint8_t reserved[64];                                       /* inout */
} set_sample_attestation_data_t;

typedef struct __attribute__((packed, aligned(4))) {
    uint32_t os_version;            /* in */
    uint32_t os_patchlevel;         /* in */
    uint32_t boot_patchlevel;       /* in */
    uint32_t vendor_patchlevel;     /* in */
} set_dummy_system_version_t;


/**
 * ssp strongbox keymaster message structure
 */
typedef struct __attribute__((packed, aligned(4))) {
    header_t header;
    command_t command;
    response_t response;
    union {
        init_ssp_ipc_t init_ssp_ipc;
        send_system_version_t send_system_version;
        get_hmac_sharing_parameters_t get_hmac_sharing_parameters;
        compute_shared_hmac_t compute_shared_hmac;
        add_rng_entropy_t add_rng_entropy;
        generate_key_t generate_key;
        get_key_characteristics_t get_key_characteristics;
        import_key_t import_key;
        import_wrappedkey_t import_wrappedkey;
        export_key_t export_key;
        begin_t begin;
        update_t update;
        finish_t finish;
        abort_t abort;
        set_attestation_data_t set_attestation_data;
        load_attestation_data_t load_attestation_data;
        attest_key_t attest_key;
        upgrade_key_t upgrade_key;
	    /* struct for provision test */
	    set_sample_attestation_data_t set_sample_attestation_data;
        /* struct for version binding test */
        set_dummy_system_version_t set_dummy_system_version;
    };
} ssp_km_message_t;

#endif /* __STRONGBOX_TEE_API_H__ */
