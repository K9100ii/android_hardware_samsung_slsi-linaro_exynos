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

#ifndef TLTEEKEYMINT_API_H
#define TLTEEKEYMINT_API_H

#include "tci.h"
#include "keymint_ta_defs.h"

/**
 * Command IDs
 */
#define CMD_MASK_ID                  0x0000ffffu
#define CMD_MASK_VERSION             0x001f0000u
#define CMD_SHIFT_VERSION                    16
#define CMD_MASK_RESERVED            0xffe00000u

#define CMD_VERSION_TEE_KEYMINT1 (5 << CMD_SHIFT_VERSION)
#define CMD_VERSION_TEE_KEYMINT2 (6 << CMD_SHIFT_VERSION)

#define CMD_ID_TEE_ADD_RNG_ENTROPY              (0x01 | CMD_VERSION_TEE_KEYMINT1)
#define CMD_ID_TEE_GENERATE_KEY                 (0x02 | CMD_VERSION_TEE_KEYMINT1)
#define CMD_ID_TEE_GET_KEY_CHARACTERISTICS      (0x03 | CMD_VERSION_TEE_KEYMINT1)
#define CMD_ID_TEE_IMPORT_KEY                   (0x04 | CMD_VERSION_TEE_KEYMINT1)
#define CMD_ID_TEE_EXPORT_KEY                   (0x05 | CMD_VERSION_TEE_KEYMINT1)
#define CMD_ID_TEE_BEGIN                        (0x06 | CMD_VERSION_TEE_KEYMINT1)
#define CMD_ID_TEE_UPDATE                       (0x07 | CMD_VERSION_TEE_KEYMINT1)
#define CMD_ID_TEE_FINISH                       (0x08 | CMD_VERSION_TEE_KEYMINT1)
#define CMD_ID_TEE_ABORT                        (0x09 | CMD_VERSION_TEE_KEYMINT1)
#define CMD_ID_TEE_CONFIGURE                    (0x0a | CMD_VERSION_TEE_KEYMINT1)
#define CMD_ID_TEE_UPGRADE_KEY                  (0x0b | CMD_VERSION_TEE_KEYMINT1)
#define CMD_ID_TEE_UPDATE_AAD                   (0x0c | CMD_VERSION_TEE_KEYMINT1)
#define CMD_ID_TEE_DESTROY_ATTESTATION_IDS      (0x0d | CMD_VERSION_TEE_KEYMINT1)
#define CMD_ID_TEE_IMPORT_WRAPPED_KEY           (0x0e | CMD_VERSION_TEE_KEYMINT1)
#define CMD_ID_TEE_GET_HMAC_SHARING_PARAMETERS  (0x0f | CMD_VERSION_TEE_KEYMINT1)
#define CMD_ID_TEE_COMPUTE_SHARED_HMAC          (0x10 | CMD_VERSION_TEE_KEYMINT1)
#define CMD_ID_TEE_GENERATE_TIMESTAMP           (0x11 | CMD_VERSION_TEE_KEYMINT1)
#define CMD_ID_TEE_EARLY_BOOT_ENDED             (0x12 | CMD_VERSION_TEE_KEYMINT1)
#define CMD_ID_TEE_DEVICE_LOCKED                (0x13 | CMD_VERSION_TEE_KEYMINT1)
#define CMD_ID_TEE_DELETE_KEY                   (0x14 | CMD_VERSION_TEE_KEYMINT1)
#define CMD_ID_TEE_DELETE_ALL_KEYS              (0x15 | CMD_VERSION_TEE_KEYMINT1)
#define CMD_ID_TEE_CLOSE_SESSION                (0x16 | CMD_VERSION_TEE_KEYMINT1)

// for internal use by the Keymint TLC:
#define CMD_ID_TEE_GET_KEY_INFO           (0x0101 | CMD_VERSION_TEE_KEYMINT1)
#define CMD_ID_TEE_SET_DEBUG_LIES         (0x0102 | CMD_VERSION_TEE_KEYMINT1)

// for use by the key provisioning agent:
#define CMD_ID_TEE_SET_ATTESTATION_DATA          (0x0201 | CMD_VERSION_TEE_KEYMINT1)
#define CMD_ID_TEE_LOAD_ATTESTATION_DATA         (0x0202 | CMD_VERSION_TEE_KEYMINT1)
#define CMD_ID_TEE_DEPRECATED_203                (0x0203 | CMD_VERSION_TEE_KEYMINT1)
#define CMD_ID_TEE_DEPRECATED_204                (0x0204 | CMD_VERSION_TEE_KEYMINT1)
#define CMD_ID_TEE_SET_ATTESTATION_DATA_AND_IDS  (0x0205 | CMD_VERSION_TEE_KEYMINT1)
#define CMD_ID_TEE_LOAD_ATTESTATION_DATA_AND_IDS (0x0206 | CMD_VERSION_TEE_KEYMINT1)

// for use by Android Linux kernel / UFS driver
#define CMD_ID_TEE_UNWRAP_AES_STORAGE_KEY (0x301 | CMD_VERSION_TEE_KEYMINT1)

#define CMD_ID_TEE_RKP_GENERATE_ECDSA_P256_KEY      (0x401 | CMD_VERSION_TEE_KEYMINT1)
#define CMD_ID_TEE_RKP_GENERATE_CERTIFICATE_REQUEST (0x402 | CMD_VERSION_TEE_KEYMINT1)

#define CMD_ID_TEE_GET_ROOT_OF_TRUST                (0x501 | CMD_VERSION_TEE_KEYMINT2)

/* ... add more command IDs when needed */

/*
+ KEY FORMATS

We define here the serialized key formats used in the code.

All uint32_t are serialized in little-endian format.

++ Key Blob

This is the 'key_material' part of a keymaster_key_blob_t, and contains an
IV, an payload encrypted using AES-GCM, and a tag.  There are now multiple
versions of this, and the details are complicated.  For the full picture, see
`util/keymint_util.c'.

++ Key Material

This comprises:

- key_material = params_len (4 bytes) | params | key_data

where 'params_len' is a uint32_t representing the length of 'params', 'params'
is the serialized representation of the Keymint key parameters (see
serialization.h for documentation of this), and 'key_data' is defined next.

++ Key Data

This comprises:

- key_data = key_type | key_size | core_key_data

where 'key_type' is a uint32_t corresponding to a keymaster_algorithm_t,
'key_size' is a uint32_t representing the key size in bits (the exact meaning
of 'key size' being key-type-dependent), and 'core_key_data' is defined next.

++ Core Key Data

This comprises:

- core_key_data = key_metadata | raw_key_data

where 'key_metadata' and 'raw_key_data' are defined below.

++ Key Metadata

This is key-type-dependent.

For AES and HMAC keys, it is empty.

For RSA keys, it comprises:

- rsa_metadata = keysize(bits) | n_len | e_len | d_len | p_len | q_len | dp_len | dq_len | qinv_len

where all lengths are uint32_t and (except for keysize) measured in bytes.

For exported RSA public keys, it comprises:

- rsa_pub_metadata = keysize(bits) | n_len | e_len

For EC keys, it comprises:

- ec_metadata = curve | x_len | y_len | d_len

where these are all uint32_t, 'curve' represents curve type and lengths are in
bytes.

For exported EC public keys, it comprises:

- ec_pub_metadata = curve | x_len | y_len

For X25519 or Ed25519 keys, it comprises:

- ec_metadata = curve (uint32_t) EC_CURVE_25519

For exported X25519 or Ed25519 public keys, there is no metadata

++ Raw Key Data

This is key-type-dependent.

For AES and HMAC keys, it comprises the key bytes.

For RSA keys, it comprises:

- n | e | d | p | q | dp | dq | qinv

with lengths as specified in the key metadata, and all numbers big-endian.

For exported RSA public keys, it comprises:

- n | e

For EC keys, it comprises:

- x | y | d

with lengths as specified in the key metadata, and all numbers big-endian.

For exported EC public keys, it comprises:

- x | y

For X25519 keys, it comprises:

- x | a

For Ed25519 keys, it comprises:

- x | k

For exported X25519 or Ed25519 public keys, it comprises:

- x

*/

#define KM_RSA_METADATA_SIZE (9*4)
#define KM_EC_METADATA_SIZE (4*4)
#define KM_EC25519_METADATA_SIZE (1*4)

/* Versions and compatibility.
 *
 * This TA is used to support several versions of the TCI protocol.  Clients
 * running old protocols don't allocate enough TCI buffer to accommodate
 * extensions in later protocol versions.
 *
 * When you want to add new structures, arguments, or return values, here's how
 * you do it.
 *
 *   1. Allocate a new API version constant at the top of this file.  We'll call
 *      this new version `N' in what follows.
 *
 *   2. If you're editing the structure for an existing command, then find the
 *      latest version of the current structure, and copy it, calling the new
 *      version `CMD_vN_t'.  Then modify this version.  It's best to limit your
 *      edits to adding new members at the end of the structure if you can: see
 *      steps 4 and 7 below.  (This copying is not a maintenance disaster
 *      because nobody will ever edit the old structures.)
 *
 *   3. If you're adding new commands, just create new structures named
 *      `CMD_vN_t'.
 *
 *   4. Find the latest version of the `tciMessage_vM_t' structure, and copy it,
 *      naming the new one `tciMessage_vN'.  Modify this copied structure
 *      appropriately to refer to your new command structures added in steps 2
 *      and 3.  If you only added members to the ends of structures, then you
 *      can just switch the appropriate member's type to the new one.  If you
 *      rearranged things more drastically, then you'll have to keep the old one
 *      around: name it `CMD_vM'.  The structure MUST NOT get smaller with
 *      increasing version numbers.
 *
 *   5. Edit the `typedef' of `tciMessage_t' to refer to `tciMessage_vN_t' which
 *      you made in step 4.
 *
 *   6. Dig up `tlMain.c' and add a new entry to the table at the top so that it
 *      can check a TCI buffer size it's been given and set its version number
 *      cap appropriately.
 *
 *   7. Modify the handler functions in `tlCryptoHandler.c'.  If you only added
 *      new commands, and/or new members to the ends of existing structures,
 *      then this will be fairly straightforward.  Entirely new commands just
 *      need to check that they've been invoked with a sufficient version (if
 *      you don't do this then the TCI buffer might be too short and you'll
 *      overrun it).  If you added new members to an existing structure, then
 *      you can most likely add a piece to the end of the argument-reading code
 *      guarded by a version check which either reads the new arguments or picks
 *      some suitable defaults.  If you did something more complicated then
 *      you'll need to cope with that the hard way.
 */

typedef struct {
    uint32_t data; /**< secure address */
    uint32_t data_length; /**< data length */
} data_blob_t;

/**
 * Command message.
 */
typedef struct __attribute__((packed))
{
    tciCommandHeader_t header; /**< Command header */
    uint32_t len; /**< Length of data to process */
}
command_t;

/**
 * Response structure
 */
typedef struct __attribute__((packed))
{
    tciResponseHeader_t header; /**< Response header */
    uint32_t len;
}
response_t;

/**
 * add_rng_entropy data structure
 */
typedef struct __attribute__((packed))
{
    data_blob_t rng_data; /**< [in] random data to be mixed in */
}
add_rng_entropy_t;

/**
 * generate_key data structure
 */
typedef struct __attribute__((packed))
{
    data_blob_t params; /**< [in] serialized */
    data_blob_t key_blob; /**< [out] keymaster_key_blob_t */
    data_blob_t characteristics; /**< [out] serialized */
}
generate_key_t;

/**
 * get_key_characteristics data structure
 */
typedef struct __attribute__((packed))
{
    data_blob_t key_blob; /**< [in] keymaster_key_blob_t */
    data_blob_t client_id; /**< [in] */
    data_blob_t app_data; /**< [in] */
    data_blob_t characteristics; /**< [out] serialized */
}
get_key_characteristics_t;

/**
 * export_key data structure
 */
typedef struct __attribute__((packed))
{
    data_blob_t key_blob; /**< [in] keymaster_key_blob_t */
    data_blob_t client_id; /**< [in] */
    data_blob_t app_data; /**< [in] */
    data_blob_t key_data; /**< [out] km_key_data */
}
export_key_t;

/**
 * upgrade_key data structure
 */
typedef struct __attribute__((packed))
{
    data_blob_t key_to_upgrade; /**< [in] keymaster_key_blob_t */
    data_blob_t upgrade_params; /**< [in] keymaster_key_param_set_t */
    data_blob_t upgraded_key; /**< [out] keymaster_key_blob_t */
}
upgrade_key_t;

/**
 * delete_key data structure
 */
typedef struct __attribute__((packed))
{
    data_blob_t key_to_delete; /**< [in] keymaster_key_blob_t */
}
delete_key_t;

/**
 * Maximum number of concurrent Keymint operations
 *
 * This is relied on by both sides of the interface.  If it increases a lot,
 * or the limit disappears entirely, then both ends will need to be
 * rethought.
 */
#define MAX_OPERATION_NUM 32

/**
 * begin data structure
 */
typedef struct __attribute__((packed))
{
    keymaster_operation_handle_t handle; /**< [out] */
    keymaster_purpose_t purpose; /**< [in] */
    data_blob_t params; /**< [in] serialized */
    data_blob_t key_blob; /**< [in] keymaster_key_blob_t */
    data_blob_t out_params; /**< [out] serialized */
    uint32_t algorithm; /**< [out] key type of operation (keymaster_algorithm_t) */
    uint32_t final_length; /**< [out] upper bound for extra length in bytes of output from finish() */
    uint64_t auth_challenge; /**< [in] HardwareAuthToken.challenge */
    uint64_t auth_user_id; /**< [in] HardwareAuthToken.userId */
    uint64_t auth_authenticator_id; /**< [in] HardwareAuthToken.authenticatorId */
    uint32_t auth_authenticator_type; /**< [in] HardwareAuthToken.authenticatorType */
    uint64_t auth_timestamp; /**< [in] HardwareAuthToken.timestamp */
    uint8_t auth_mac[32]; /**< [in] HardwareAuthToken.mac */
}
begin_t;

/**
 * update data structure
 */
typedef struct __attribute__((packed))
{
    keymaster_operation_handle_t handle; /**< [in] */
    data_blob_t input; /**< [in] */
    uint32_t input_consumed; /**< [out] */
    data_blob_t output; /**< [out] */
    uint64_t auth_challenge; /**< [in] HardwareAuthToken.challenge */
    uint64_t auth_user_id; /**< [in] HardwareAuthToken.userId */
    uint64_t auth_authenticator_id; /**< [in] HardwareAuthToken.authenticatorId */
    uint32_t auth_authenticator_type; /**< [in] HardwareAuthToken.authenticatorType */
    uint64_t auth_timestamp; /**< [in] HardwareAuthToken.timestamp */
    uint8_t auth_mac[32]; /**< [in] HardwareAuthToken.mac */
}
update_t;

/**
 * finish data structure
 */
typedef struct __attribute__((packed))
{
    keymaster_operation_handle_t handle; /**< [in] */
    data_blob_t signature; /**< [in] */
    data_blob_t output; /**< [out] */
    data_blob_t input; /**< [in] */
    uint64_t auth_challenge; /**< [in] HardwareAuthToken.challenge */
    uint64_t auth_user_id; /**< [in] HardwareAuthToken.userId */
    uint64_t auth_authenticator_id; /**< [in] HardwareAuthToken.authenticatorId */
    uint32_t auth_authenticator_type; /**< [in] HardwareAuthToken.authenticatorType */
    uint64_t auth_timestamp; /**< [in] HardwareAuthToken.timestamp */
    uint8_t auth_mac[32]; /**< [in] HardwareAuthToken.mac */
}
finish_t;

/**
 * abort structure
 */
typedef struct __attribute__((packed))
{
    keymaster_operation_handle_t handle; /**< [in] */
}
abort_t;

/**
 * configure structure
 */
typedef struct __attribute__((packed))
{
    data_blob_t params; /**< [in] serialized */
}
configure_t;

/**
 * get_hardware_features structure
 */
typedef struct __attribute__((packed))
{
    bool is_secure; /**< [out] keys are stored in secure hardware and never leave it */
    bool supports_elliptic_curve; /**< [out] hardware supports ECC with P224, P256, P384 and P521 */
    bool supports_symmetric_cryptography; /**< [out] hardware supports AES and HMAC */
    bool supports_attestation; /**< [out] hardware supports generation of public key attestation certificates */
}
get_hardware_features_v3_t;

/**
 * get_key_info data structure
 */
typedef struct __attribute__((packed))
{
    data_blob_t key_blob; /**< [in] keymaster_key_blob_t */
    data_blob_t client_id; /**< [in] KM_TAG_APPLICATION_ID */
    data_blob_t app_data; /**< [in] KM_TAG_APPLICATION_DATA */
    uint32_t key_type; /**< [out] (keymaster_algorithm_t) */
    uint32_t key_size; /**< [out] bits */
}
get_key_info_t;

/**
 * set_debug_lies structure
 *
 * This command is not recognized in a non-debug build of the TA.
 */
typedef struct __attribute__((packed))
{
    uint32_t os_version; /**< [in] */
    uint32_t os_patchlevel; /**< [in] */
    uint32_t vendor_patchlevel; /**< [in] */
    uint32_t boot_patchlevel; /**< [in] */
}
set_debug_lies_t;

/**
 * set_attestation_data data structure
 */
typedef struct __attribute__((packed))
{
    data_blob_t data; /**< [in] attestation data (format is OEM-specific) */
    data_blob_t blob; /**< [out] encrypted attestation data (Secure Object) */
}
set_attestation_data_t;

/**
 * load_attestation_data data structure
 */
typedef struct __attribute__((packed))
{
    data_blob_t data; /**< [in] attestation data (format is OEM-specific) */
}
load_attestation_data_t;

/**
 * set_attestation_data_and_ids data structure
 */
typedef struct __attribute__((packed))
{
    data_blob_t data; /**< [in] attestation data (format is OEM-specific) */
    data_blob_t ids; /**< [in] attestation ids (format is OEM-specific) */
    data_blob_t blob; /**< [out] encrypted attestation data and ids (Secure Object) */
}
set_attestation_data_and_ids_t;

/**
 * load_attestation_data_and_ids data structure
 */
typedef struct __attribute__((packed))
{
    data_blob_t data; /**< [in] attestation data and ids (format is OEM-specific) */
}
load_attestation_data_and_ids_t;

/**
 * get_hmac_sharing_parameters structure
 */
#define KM_SHARING_PARAMETERS_SEED_LENGTH       32
#define KM_SHARING_PARAMETERS_NONCE_LENGTH      32
typedef struct {
    uint8_t seed[KM_SHARING_PARAMETERS_SEED_LENGTH];        /**< [out] */
    uint32_t seed_length;                                   /**< [out] */
    uint8_t nonce[KM_SHARING_PARAMETERS_NONCE_LENGTH];      /**< [out] */
} __attribute__((packed)) get_hmac_sharing_parameters_t;

/**
 * compute_shared_hmac structure
 */
#define KM_SHARING_PARAMETERS_MAX_NUMBER        4
#define KM_SHARING_CHECK_LENGTH                 32
typedef struct {
    uint32_t sharing_params_length;
    /**< [in] */
    get_hmac_sharing_parameters_t sharing_params[KM_SHARING_PARAMETERS_MAX_NUMBER];
    /**< [out] */
    uint8_t sharing_check[KM_SHARING_CHECK_LENGTH];
} __attribute__((packed)) compute_shared_hmac_t;

/**
 * timestamp_token structure
 */
typedef struct {
    uint64_t challenge;         /**< [in/out] SecureTimestampToken.challenge */
    uint64_t timestamp;         /**< [out] timestamp */
    uint8_t mac[32];            /**< [out] VerificationToken.mac */

} __attribute__((packed)) timestamp_token_t;

/**
 * get_shared_key structure
 */
typedef struct {
    data_blob_t encrypted_K;    /**< [out] */
} __attribute__((packed)) get_shared_key_t;

/**
 * device_locked structure
 */
typedef struct {
    bool password_only;    /**< [in] */
} __attribute__((packed)) device_locked_t;

typedef struct __attribute__((packed))
{
    keymaster_operation_handle_t handle; /**< [in] */
    data_blob_t aad; /**< [in] */
    uint64_t auth_challenge; /**< [in] HardwareAuthToken.challenge */
    uint64_t auth_user_id; /**< [in] HardwareAuthToken.userId */
    uint64_t auth_authenticator_id; /**< [in] HardwareAuthToken.authenticatorId */
    uint32_t auth_authenticator_type; /**< [in] HardwareAuthToken.authenticatorType */
    uint64_t auth_timestamp; /**< [in] HardwareAuthToken.timestamp */
    uint8_t auth_mac[32]; /**< [in] HardwareAuthToken.mac */
}
update_aad_t;

/**
 * generate_and_attest_key data structure
 */
typedef struct __attribute__((packed))
{
    data_blob_t params; /**< [in] serialized */
    data_blob_t attest_key_params; /**< [in] serialized */
    data_blob_t attest_key_blob; /**< [in] keymaster_key_blob_t */
    data_blob_t attest_issuer_blob; /**< [in] keymaster_blob_t */
    data_blob_t key_blob; /**< [out] keymaster_key_blob_t */
    data_blob_t characteristics; /**< [out] serialized */
    data_blob_t cert_chain; /**< [out] serialized */
}
generate_and_attest_key_t;

/**
 * import_and_attest_key data structure
 */
typedef struct __attribute__((packed))
{
    data_blob_t params; /**< [in] serialized */
    data_blob_t attest_key_params; /**< [in] serialized */
    data_blob_t attest_key_blob; /**< [in] keymaster_key_blob_t */
    data_blob_t attest_issuer_blob; /**< [in] keymaster_blob_t */
    data_blob_t key_data; /**< [in] km_key_data */
    data_blob_t key_blob; /**< [out] keymaster_key_blob_t */
    data_blob_t characteristics; /**< [out] serialized */
    data_blob_t cert_chain; /**< [out] serialized */
}
import_and_attest_key_t;

/**
 * import_wrapped_key data structure
 *
 * We are limited to 4 independent mapped buffers per TA command. Therefore we
 * have to combine several parameters into a single data_blob_t. Offsets within
 * this blob are passed separately.
 */
typedef struct __attribute__((packed))
{
    uint32_t key_format; /**< [in] */
    uint32_t key_type;  /**< [in] */

    /* 1. serialized key parameters
       2. encryptedTransportKey
       3. keyDescription
       4. encryptedKey */
    data_blob_t params; /**< [in] */
    uint32_t ETK_offset; /**< [in] */
    uint32_t KD_offset; /**< [in] */
    uint32_t EK_offset; /**< [in] */

    uint8_t initialization_vector[16]; /**< [in] */
    uint8_t IV_len; /** [in] */
    uint8_t tag[16]; /**< [in] */
    uint8_t masking_key[32]; /**< [in] */

    /* 1. serialized unwrapParams
     * 2. wrappingKeyBlob */
    data_blob_t wrap; /**< [in] */
    uint32_t WKB_offset; /**< [in] */

    uint64_t password_sid; /**< [in] */
    uint64_t biometric_sid; /**< [in] */
    data_blob_t key_blob; /**< [out] */
    data_blob_t key_characteristics; /**< [out] serialized */
    data_blob_t cert_chain; /**< [out] serialized */
}
import_wrapped_key_t;

/**
 * AES storage key unwrap data structure
 */
typedef struct __attribute__((packed))
{
    data_blob_t wrapped_key_data; /**< [in] AES storage key to unwrap */
}
aes_storage_key_unwrap_t;

/**
 * RKP generate ECDSA P-256 key pair structure
 */
typedef struct __attribute__((packed))
{
    bool        test_mode;          /**< [in] indicates whether the generated key is for testing only */
    data_blob_t maced_public_key;   /**< [out] public key */
    data_blob_t private_key_handle; /**< [out] private key handle */
}
rkp_generate_ecdsa_p256_key_t;

/**
 * RKP generate certificate request structure
 */
typedef struct __attribute__((packed))
{
    bool        test_mode;               /**< [in] indicates whether the generated key is for testing only */
    data_blob_t keys_to_sign;            /**< [in] public keys */
    data_blob_t endpoint_encryption_key; /**< [in] EEK cert chain */
    data_blob_t challenge;               /**< [in] challenge */
    data_blob_t device_info;             /**< [out] verified device info */
    data_blob_t protected_data;          /**< [out] protected data */
    data_blob_t keys_to_sign_mac;        /**< [out] keys to sign mac */
}
rkp_generate_certificate_request_t;

/**
 * SecureTime request structure
 */
typedef struct __attribute__((packed))
{
    keymaster_timestamp_token_t token;
}
generate_timestamp_req_t;

/**
 * root_of_trust structure
 */
typedef struct {
    uint8_t challenge[16];            /**< [in] Challenge given by StrongBox */
    data_blob_t maced_rot;            /**< [out] Root of Trust data */

} __attribute__((packed)) get_root_of_trust_t;

/**
 * TCI message data.
 */
typedef struct __attribute__((packed))
{
    union {
        command_t command;
        response_t response;
    };

    union {
        add_rng_entropy_t                  add_rng_entropy;
        generate_key_t                     generate_key;
        get_key_characteristics_t          get_key_characteristics;
        export_key_t                       export_key;
        begin_t                            begin;
        update_t                           update;
        finish_t                           finish;
        abort_t                            abort;
        configure_t                        configure;
        upgrade_key_t                      upgrade_key;
        get_key_info_t                     get_key_info;
        set_debug_lies_t                   set_debug_lies;
        set_attestation_data_t             set_attestation_data;
        load_attestation_data_t            load_attestation_data;
        set_attestation_data_and_ids_t     set_attestation_data_and_ids;
        load_attestation_data_and_ids_t    load_attestation_data_and_ids;
        import_wrapped_key_t               import_wrapped_key;
        get_hmac_sharing_parameters_t      get_hmac_sharing_parameters;
        compute_shared_hmac_t              compute_shared_hmac;
        generate_timestamp_req_t           generate_timestamp;
        get_shared_key_t                   get_shared_key;
        device_locked_t                    device_locked;
        aes_storage_key_unwrap_t           aes_storage_key_unwrap;
        update_aad_t                       update_aad;
        generate_and_attest_key_t          generate_and_attest_key;
        import_and_attest_key_t            import_and_attest_key;
        rkp_generate_ecdsa_p256_key_t      rkp_generate_ecdsa_p256_key;
        rkp_generate_certificate_request_t rkp_generate_certificate_request;
        delete_key_t                       delete_key;
        get_root_of_trust_t                get_root_of_trust;
    };
}
tciMessage_v1_t;

/* Define the TCI message format for the requested version. */
typedef tciMessage_v1_t tciMessage_t, *tciMessage_ptr;

/**
 * Overall TCI structure.
 */
typedef struct __attribute__((packed))
{
    tciMessage_t message; /**< TCI message */
}
tci_t;

/**
 * TA UUID
 */
#define TEE_KEYMINT_TA_UUID { { 7, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x4D } }

#endif // TLTEEKEYMINT_API_H
