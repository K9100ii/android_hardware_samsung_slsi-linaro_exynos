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
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <memory>
#include <mutex>

#include "tee_client_api.h"

#include "ssp_strongbox_keymaster_log.h"
#include "ssp_strongbox_keymaster_defs.h"
#include "ssp_strongbox_keymaster_configuration.h"
#include "ssp_strongbox_keymaster_serialization.h"
#include "ssp_strongbox_keymaster_impl.h"
#include "ssp_strongbox_keymaster_parse_key.h"
#include "ssp_strongbox_keymaster_encode_key.h"
#include "ssp_strongbox_keymaster_hwctl.h"

#include "ssp_strongbox_tee_uuid.h"
#include "ssp_strongbox_tee_api.h"

/* std */
using std::unique_ptr;
using std::lock_guard;
using std::mutex;

/* mutex */
static mutex ssp_mutex;

static const TEEC_UUID sUUID = strongbox_tee_UUID;

#define STRONGBOX_ATTESTATION_FILE "/mnt/vendor/efs/DAK/sspkeybox"

typedef struct ssp_opdata_blob {
    uint32_t op_idx;
    uint32_t memref_type;
    TEEC_TempMemoryReference *op_data;
} ssp_opdata_blob_t;

/* define shared memory type */
#define SSP_MEM_INPUT   0
#define SSP_MEM_OUTPUT  1
#define SSP_MEM_INOUT   2

#define CHECK_SSPKM_MSG_GOTOEND(ssp_msg) \
    do { \
        ret = ((keymaster_error_t)ssp_msg.response.km_errcode); \
        if (ret != KM_ERROR_OK) { \
             BLOGE("%s failed. ret(%d), sspkm_errno(0x%08x), cm_errno(0x%08x)\n",\
            __func__, ret, ssp_msg.response.sspkm_errno, ssp_msg.response.cm_errno); \
            goto end; \
        } \
    } while (0)

int ssp_open(ssp_session_t &sess);
int ssp_close(ssp_session_t &sp_sess);
int ssp_transact(ssp_session_t &sp_sess, ssp_km_message_t &ssp_msg);

/**
 * Check supported key size
 */
static bool check_supported_key_size(
    keymaster_algorithm_t algorithm,
    size_t keysize)
{
    switch (algorithm) {
        case KM_ALGORITHM_RSA:
            return (keysize == 2048);
        case KM_ALGORITHM_EC:
            return (keysize == 256); // ||
        case KM_ALGORITHM_AES:
            return ((keysize == 128) ||
                    (keysize == 192) ||
                    (keysize == 256));
        case KM_ALGORITHM_HMAC:
            return ((keysize >= 64) &&
                    (keysize <= SSP_HMAC_MAX_KEY_SIZE) &&
                    (keysize % 8 == 0));
        case KM_ALGORITHM_TRIPLE_DES:
            return (keysize == 168);
        default:
            return false;
    }
}


/* Keymaster common utils */
keymaster_error_t validate_key_params(const keymaster_key_param_set_t* params)
{
    keymaster_error_t ret = KM_ERROR_OK;
    (void)params;
    /* todo: check key params */

    return ret;
}

/* define SSP max key size for algorithms */
static uint32_t ssp_key_size_max(
    keymaster_algorithm_t algorithm)
{
    switch (algorithm) {
        case KM_ALGORITHM_RSA:
            return STRONGBOX_RSA_MAX_KEY_SIZE;
        case KM_ALGORITHM_EC:
            return STRONGBOX_EC_MAX_KEY_SIZE;
        case KM_ALGORITHM_AES:
            return STRONGBOX_AES_MAX_KEY_SIZE;
        case KM_ALGORITHM_HMAC:
            return STRONGBOX_HMAC_MAX_KEY_SIZE;
        case KM_ALGORITHM_TRIPLE_DES:
            return STRONGBOX_TRIPLE_DES_KEY_SIZE;
        default:
            return 0;
    }
}

/* define SSP max key data size for algorithms */
static uint32_t ssp_key_data_size_max(
    keymaster_algorithm_t algorithm,
    uint32_t  key_size_bits)
{
    uint32_t keylen = BITS2BYTES(
        (key_size_bits > 0) ? key_size_bits : ssp_key_size_max(algorithm));
    uint32_t premeta_size = 4 + 4; /* key type + key size */

    switch(algorithm)
    {
        case KM_ALGORITHM_AES:
            return premeta_size + keylen;
        case KM_ALGORITHM_HMAC:
            return premeta_size + keylen;
        case KM_ALGORITHM_TRIPLE_DES:
            return premeta_size + keylen;
        case KM_ALGORITHM_RSA:
            /* n + e + d + p + q + dp + dq + qinv */
            return premeta_size + SSP_KM_RSA_METADATA_SIZE + 8 * keylen;
        case KM_ALGORITHM_EC:
            /* k + x + y */
            return premeta_size + SSP_KM_EC_METADATA_SIZE + 3 * keylen;
        default:
            return 0;
    }
}

/* define upper bound of key blob for each algorithms */
static uint32_t ssp_key_blob_size_max(
    keymaster_algorithm_t algorithm,
    uint32_t key_size_bits,
    uint32_t params_size_bytes)
{
    uint32_t key_material_size = 4 + params_size_bytes;

    /* add own characteristics size */
    key_material_size += SB_EXTRA_PARAMS_MAX_SIZE;

    /* add keyblob metadata size */
    key_material_size += SSP_KEYBLOB_META_SIZE_OF_BLOB;
    key_material_size += SSP_KEYBLOB_MATERIAL_HEADER_LEN;

    /* add max keydata size */
    key_material_size += ssp_key_data_size_max(algorithm, key_size_bits);

    return key_material_size;
}

static keymaster_error_t validate_ipc_req_message(ssp_km_message_t *ssp_msg)
{
    keymaster_error_t ret = KM_ERROR_OK;

    switch(ssp_msg->command.id) {
    case CMD_SSP_INIT_IPC:
        break;
    case CMD_SSP_SEND_SYSTEM_VERSION:
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->send_system_version.system_version_len <=
            sizeof(ssp_msg->send_system_version.system_version)));
        break;
    case CMD_SSP_GET_HMAC_SHARING_PARAMETERS:
        break;
    case CMD_SSP_COMPUTE_SHARED_HAMC:
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->compute_shared_hmac.params_cnt <=
            SSP_HMAC_SHARING_PARAM_MAX_CNT));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->compute_shared_hmac.params[0].seed_len <=
            sizeof(ssp_msg->compute_shared_hmac.params[0].seed)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->compute_shared_hmac.params[0].nonce_len <=
            sizeof(ssp_msg->compute_shared_hmac.params[0].nonce)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->compute_shared_hmac.params[1].seed_len <=
            sizeof(ssp_msg->compute_shared_hmac.params[1].seed)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->compute_shared_hmac.params[1].nonce_len <=
            sizeof(ssp_msg->compute_shared_hmac.params[1].nonce)));
        break;
    case CMD_SSP_VERIFY_AUTHORIZATION:
        break;
    case CMD_SSP_ADD_RNG_ENTROPY:
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->add_rng_entropy.rng_entropy_len <=
            sizeof(ssp_msg->add_rng_entropy.rng_entropy)));
        break;
    case CMD_SSP_GENERATE_KEY:
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->generate_key.keyparams_len <=
            sizeof(ssp_msg->generate_key.keyparams)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->generate_key.keyblob_len <=
            sizeof(ssp_msg->generate_key.keyblob)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->generate_key.keychar_len <=
            sizeof(ssp_msg->generate_key.keychar)));
        break;
	case CMD_SSP_GET_KEY_CHARACTERISTICS:
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->get_key_characteristics.keyblob_len <=
            sizeof(ssp_msg->get_key_characteristics.keyblob)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->get_key_characteristics.client_id_len <=
            sizeof(ssp_msg->get_key_characteristics.client_id)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->get_key_characteristics.app_data_len <=
            sizeof(ssp_msg->get_key_characteristics.app_data)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->get_key_characteristics.keychar_len <=
            sizeof(ssp_msg->get_key_characteristics.keychar)));
        break;
    case CMD_SSP_IMPORT_KEY:
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->import_key.keyparams_len <=
            sizeof(ssp_msg->import_key.keyparams)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->import_key.keydata_len <=
            sizeof(ssp_msg->import_key.keydata)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->import_key.keyblob_len <=
            sizeof(ssp_msg->import_key.keyblob)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->import_key.keychar_len <=
            sizeof(ssp_msg->import_key.keychar)));
        break;
    case CMD_SSP_IMPORT_WRAPPED_KEY:
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->import_wrappedkey.keyparams_len <=
            sizeof(ssp_msg->import_wrappedkey.keyparams)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->import_wrappedkey.encrypted_transport_key_len <=
            sizeof(ssp_msg->import_wrappedkey.encrypted_transport_key)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->import_wrappedkey.key_description_len <=
            sizeof(ssp_msg->import_wrappedkey.key_description)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->import_wrappedkey.encrypted_key_len <=
            sizeof(ssp_msg->import_wrappedkey.encrypted_key)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->import_wrappedkey.iv_len <=
            sizeof(ssp_msg->import_wrappedkey.iv)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->import_wrappedkey.tag_len <=
            sizeof(ssp_msg->import_wrappedkey.tag)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->import_wrappedkey.masking_key_len <=
            sizeof(ssp_msg->import_wrappedkey.masking_key)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->import_wrappedkey.wrapping_keyblob_len <=
            sizeof(ssp_msg->import_wrappedkey.wrapping_keyblob)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->import_wrappedkey.unwrapping_params_blob_len <=
            sizeof(ssp_msg->import_wrappedkey.unwrapping_params_blob)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->import_wrappedkey.keyblob_len <=
            sizeof(ssp_msg->import_wrappedkey.keyblob)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->import_wrappedkey.keychar_len <=
            sizeof(ssp_msg->import_wrappedkey.keychar)));
        break;
    case CMD_SSP_EXPORT_KEY:
        /* nothing to do - legacy */
        break;
    case CMD_SSP_FACTORY_ATTEST_KEY:
    case CMD_SSP_SELF_ATTEST_KEY:
    case CMD_SSP_ATTEST_KEY:
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->attest_key.attestation_blob_len <=
            sizeof(ssp_msg->attest_key.attestation_blob)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->attest_key.keyblob_len <=
            sizeof(ssp_msg->attest_key.keyblob)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->attest_key.keyparams_len <=
            sizeof(ssp_msg->attest_key.keyparams)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->attest_key.cert_chain_len <=
            sizeof(ssp_msg->attest_key.cert_chain)));
        break;
    case CMD_SSP_UPGRADE_KEY:
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->upgrade_key.keyblob_len <=
            sizeof(ssp_msg->upgrade_key.keyblob)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->upgrade_key.keyparams_len <=
            sizeof(ssp_msg->upgrade_key.keyparams)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->upgrade_key.upgraded_keyblob_len <=
            sizeof(ssp_msg->upgrade_key.upgraded_keyblob)));
        break;
    case CMD_SSP_DELETE_KEY:
        break;
    case CMD_SSP_DELETE_ALL_KEYS:
        break;
    case CMD_SSP_DESTROY_ATTESTATION_IDS:
        break;
    case CMD_SSP_BEGIN:
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->begin.in_params_len <=
            sizeof(ssp_msg->begin.in_params)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->begin.keyblob_len <=
            sizeof(ssp_msg->begin.keyblob)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->begin.out_params_len <=
            sizeof(ssp_msg->begin.out_params)));
        break;
    case CMD_SSP_UPDATE:
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->update.in_params_len <=
            sizeof(ssp_msg->update.in_params)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->update.input_len <=
            sizeof(ssp_msg->update.input)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->update.output_len <=
            sizeof(ssp_msg->update.output)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->update.output_result_len <=
            sizeof(ssp_msg->update.output)));
        break;
    case CMD_SSP_FINISH:
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->finish.confirmation_token_len <=
            sizeof(ssp_msg->finish.confirmation_token)));

        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->finish.input_len <=
            sizeof(ssp_msg->finish.input)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->finish.output_len <=
            sizeof(ssp_msg->finish.output)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->finish.output_result_len <=
            sizeof(ssp_msg->finish.output)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->finish.signature_len <=
            sizeof(ssp_msg->finish.signature)));
        break;
    case CMD_SSP_ABORT:
        break;
    case CMD_SSP_DEVICE_LOCKED:
        break;
    case CMD_SSP_EARLY_BOOT_ENDED:
        break;
    case CMD_SSP_SET_ATTESTATION_DATA:
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->set_attestation_data.attestation_data_len + SSP_AES_BLOCK_SIZE <=
            sizeof(ssp_msg->set_attestation_data.attestation_data)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->set_attestation_data.attestation_blob_len <=
            sizeof(ssp_msg->set_attestation_data.attestation_data)));
        break;
    case CMD_SSP_LOAD_ATTESTATION_DATA:
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->load_attestation_data.attestation_blob_len <=
            sizeof(ssp_msg->load_attestation_data.attestation_blob)));
        break;

    /* for privsion test */
    case CMD_SSP_SET_SAMPLE_ATTESTATION_DATA:
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->set_sample_attestation_data.attestation_blob_len <=
            sizeof(ssp_msg->set_sample_attestation_data.attestation_blob)));
        break;

    case CMD_SSP_SET_DUMMY_SYSTEM_VERSION:
        break;

    default:
        ret = KM_ERROR_INVALID_ARGUMENT;
    }

end:

    return ret;
}

static keymaster_error_t validate_ipc_resp_message(ssp_km_message_t *ssp_msg)
{
    keymaster_error_t ret = KM_ERROR_OK;

    switch(ssp_msg->command.id) {
    case CMD_SSP_INIT_IPC:
        break;
    case CMD_SSP_SEND_SYSTEM_VERSION:
        break;
    case CMD_SSP_GET_HMAC_SHARING_PARAMETERS:
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->get_hmac_sharing_parameters.seed_len <=
            sizeof(ssp_msg->get_hmac_sharing_parameters.seed)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->get_hmac_sharing_parameters.nonce_len <=
            sizeof(ssp_msg->get_hmac_sharing_parameters.nonce)));
        break;
    case CMD_SSP_COMPUTE_SHARED_HAMC:
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->compute_shared_hmac.sharing_check_len <=
            sizeof(ssp_msg->compute_shared_hmac.sharing_check)));
        break;
    case CMD_SSP_VERIFY_AUTHORIZATION:
        break;
    case CMD_SSP_ADD_RNG_ENTROPY:
        break;
    case CMD_SSP_GENERATE_KEY:
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->generate_key.keyblob_len <=
            sizeof(ssp_msg->generate_key.keyblob)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->generate_key.keychar_len <=
            sizeof(ssp_msg->generate_key.keychar)));
        break;
    case CMD_SSP_GET_KEY_CHARACTERISTICS:
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->get_key_characteristics.keychar_len <=
            sizeof(ssp_msg->get_key_characteristics.keychar)));
        break;
    case CMD_SSP_IMPORT_KEY:
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->import_key.keyblob_len <=
            sizeof(ssp_msg->import_key.keyblob)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->import_key.keychar_len <=
            sizeof(ssp_msg->import_key.keychar)));
        break;
    case CMD_SSP_IMPORT_WRAPPED_KEY:
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->import_wrappedkey.keyblob_len <=
            sizeof(ssp_msg->import_wrappedkey.keyblob)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->import_wrappedkey.keychar_len <=
            sizeof(ssp_msg->import_wrappedkey.keychar)));
        break;
    case CMD_SSP_EXPORT_KEY:
        /* nothing to do - legacy */
        break;
    case CMD_SSP_FACTORY_ATTEST_KEY:
    case CMD_SSP_SELF_ATTEST_KEY:
    case CMD_SSP_ATTEST_KEY:
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->attest_key.cert_chain_len <=
            sizeof(ssp_msg->attest_key.cert_chain)));
        break;
    case CMD_SSP_UPGRADE_KEY:
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->upgrade_key.upgraded_keyblob_len <=
            sizeof(ssp_msg->upgrade_key.upgraded_keyblob)));
        break;
    case CMD_SSP_DELETE_KEY:
        break;
    case CMD_SSP_DELETE_ALL_KEYS:
        break;
    case CMD_SSP_DESTROY_ATTESTATION_IDS:
        break;
    case CMD_SSP_BEGIN:
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->begin.out_params_len <=
            sizeof(ssp_msg->begin.out_params)));
        break;
    case CMD_SSP_UPDATE:
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->update.output_len <=
            sizeof(ssp_msg->update.output)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->update.output_result_len <=
            sizeof(ssp_msg->update.output)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->update.input_consumed_len <=
            sizeof(ssp_msg->update.input)));
        break;
    case CMD_SSP_FINISH:
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->finish.output_len <=
            sizeof(ssp_msg->finish.output)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->finish.output_result_len <=
            sizeof(ssp_msg->finish.output)));
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->finish.input_consumed_len <=
            sizeof(ssp_msg->finish.input)));
        break;
    case CMD_SSP_ABORT:
        break;
    case CMD_SSP_DEVICE_LOCKED:
        break;
    case CMD_SSP_EARLY_BOOT_ENDED:
        break;
    case CMD_SSP_SET_ATTESTATION_DATA:
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->set_attestation_data.attestation_blob_len <=
            sizeof(ssp_msg->set_attestation_data.attestation_data)));
        break;
    case CMD_SSP_LOAD_ATTESTATION_DATA:
        break;

    /* for privsion test */
    case CMD_SSP_SET_SAMPLE_ATTESTATION_DATA:
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
            (ssp_msg->set_sample_attestation_data.attestation_blob_len <=
            sizeof(ssp_msg->set_sample_attestation_data.attestation_blob)));
        break;

    case CMD_SSP_SET_DUMMY_SYSTEM_VERSION:
        break;

    default:
        ret = KM_ERROR_INVALID_ARGUMENT;
    }

end:

    return ret;
}


/* operation bucket handling functions */
static void init_ssp_operation_buckets(ssp_session_t *sess)
{
    for (int i = 0; i < SSP_OPERATION_SESSION_MAX; i++) {
        sess->ssp_operation[i].used = false;
        sess->ssp_operation[i].final_len = 0;
        sess->ssp_operation[i].handle = 0;
    }

    sess->ssp_op_count = 0;
}

/* find ssp operation with operation handle */
static keymaster_error_t find_ssp_operation_handle(
    ssp_session_t *sess,
    keymaster_operation_handle_t op_handle,
    ssp_operation_t **ssp_operation)
{
    keymaster_error_t ret = KM_ERROR_INVALID_OPERATION_HANDLE;

    for (int i = 0; i < SSP_OPERATION_SESSION_MAX; i++) {
        if (sess->ssp_operation[i].used &&
            sess->ssp_operation[i].handle == op_handle) {
            *ssp_operation = &sess->ssp_operation[i];
            ret = KM_ERROR_OK;
            break;
        }
    }
    if (ret != KM_ERROR_OK) {
        BLOGE("operation handle is not found: op_handle(%lu)", op_handle);
    }
    return ret;
}


/* get empty operation bucket */
static keymaster_error_t alloc_ssp_operation_bucket(
    ssp_session_t *sess,
    ssp_operation_t **ssp_operation)
{
    keymaster_error_t ret = KM_ERROR_TOO_MANY_OPERATIONS;
    *ssp_operation = NULL;

    for (int i = 0; i < SSP_OPERATION_SESSION_MAX; i++) {
        if (!sess->ssp_operation[i].used) {
            *ssp_operation = &sess->ssp_operation[i];
            ret = KM_ERROR_OK;
            break;
        }
    }

    if (ret != KM_ERROR_OK) {
        BLOGE("too many operation numbers\n");
    } else {
        (*ssp_operation)->used = true;
        sess->ssp_op_count++;
    }

    return ret;
}


/**
 * free ssp operation bucket
 */
static void free_ssp_operation_bucket(
    ssp_session_t *sess,
    ssp_operation_t *ssp_operation)
{
    keymaster_operation_handle_t op_handle = 0;

    if (ssp_operation == NULL)
        return;

    if (ssp_operation->used == false)
        return;

    op_handle = ssp_operation->handle;
    ssp_operation->used = false;
    ssp_operation->final_len = 0;
    ssp_operation->handle = 0;

    sess->ssp_op_count--;

    BLOGD("operation handle(%lu) is free. remain %d sessions\n",
        op_handle, sess->ssp_op_count);
}

int ssp_open(ssp_session_t &sess)
{
    TEEC_Result ret = TEEC_SUCCESS;

    ret = TEEC_InitializeContext(NULL, &sess.teec_context);
    if (ret != TEEC_SUCCESS) {
        BLOGE("Could not initialize context with the TEE. ret=0x%x", ret);
        return -1;
    }

    memset(&sess.teec_session, 0, sizeof(sess.teec_session));
    memset(&sess.teec_operation, 0, sizeof(sess.teec_operation));
    ret = TEEC_OpenSession(
                &sess.teec_context,
                &sess.teec_session,
                &sUUID,
                TEEC_LOGIN_PUBLIC,
                NULL,                   /* connectionData */
                &sess.teec_operation,   /* IN OUT operation */
                NULL                    /* OUT returnOrigin, optional */
                );
    if (ret != TEEC_SUCCESS) {
        BLOGE("Could not open session with Trusted Application. ret=0x%x", ret);
        return -1;
    }

    /* init ssp operation array */
    init_ssp_operation_buckets(&sess);

    return 0;
}

int ssp_close(ssp_session_t &sess)
{
    TEEC_CloseSession(&sess.teec_session);
    TEEC_FinalizeContext(&sess.teec_context);

    return 0;
}

int ssp_transact(ssp_session_t &ssp_sess, ssp_km_message_t &ssp_msg)
{
    TEEC_Result ret = TEEC_SUCCESS;

    ssp_hwctl_enable();

    ret = TEEC_InvokeCommand(
                       &ssp_sess.teec_session,
                       ssp_msg.command.id,
                       &ssp_sess.teec_operation,
                       NULL               /* OUT returnOrigin, optional */
                       );
    if (ret != TEEC_SUCCESS) {
        BLOGE("Could not invoke command with Trusted Application. ret=0x%x", ret);
        ssp_hwctl_disable();
        return -1;
    }

    ssp_hwctl_disable();

    return 0;
}

inline void ssp_set_teec_opdata_tmpref(
    ssp_session_t &sess,
    ssp_opdata_blob_t &blob,
    uint32_t param_idx,
    uint32_t memref_type)
{
    blob.op_idx = param_idx;
    blob.op_data = &sess.teec_operation.params[param_idx].tmpref;
    blob.memref_type = memref_type;
}

inline void ssp_set_teec_param_memref(
    ssp_session_t &sess,
    uint32_t param_idx,
    uint8_t *addr,
    size_t size)
{
    sess.teec_operation.params[param_idx].tmpref.buffer = addr;
    sess.teec_operation.params[param_idx].tmpref.size = size;
}

/* mutex & ssp transaction */
static keymaster_error_t ssp_invoke(
    ssp_session_t sess,
    ssp_km_message_t *ssp_msg)
{
    keymaster_error_t ret = KM_ERROR_OK;
    lock_guard<mutex> ssp_lock(ssp_mutex);

    /* __MUTEX_SCOPE__ */
    /* set extra data types */
    sess.teec_operation.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INOUT,
            TEEC_NONE,
            TEEC_NONE,
            TEEC_NONE);

    /* set command data */
    ssp_set_teec_param_memref(sess, OP_PARAM_0, (uint8_t *)ssp_msg, sizeof(*ssp_msg));

    if (ssp_transact(sess, *ssp_msg) < 0) {
        ret = KM_ERROR_SECURE_HW_COMMUNICATION_FAILED;
    }

    return ret;
}   /* __MUTEX_SCOPE__ */

keymaster_error_t ssp_init_ipc(
    ssp_session_t &sess)
{
    BLOGD("%s\n", __func__);
    keymaster_error_t ret = KM_ERROR_OK;
    ssp_km_message_t ssp_msg = {};

    /* set SSP IPC params */
    memset(&ssp_msg, 0x00, sizeof(ssp_msg));
    ssp_msg.command.id = CMD_SSP_INIT_IPC;

    /* check validity of ssp msg len */
    EXPECT_KMOK_GOTOEND(validate_ipc_req_message(&ssp_msg));

    /* invoke ssp msg to ssp */
    EXPECT_KMOK_GOTOEND(ssp_invoke(sess, &ssp_msg));

    /* process reply message */
    CHECK_SSPKM_MSG_GOTOEND(ssp_msg);

    EXPECT_KMOK_GOTOEND(validate_ipc_resp_message(&ssp_msg));

end:
    if (ret != KM_ERROR_OK) {
        BLOGE("%s() exit with %d\n", __func__, ret);
    } else {
        BLOGD("%s() exit with %d\n", __func__, ret);
    }

    return ret;
}

keymaster_error_t ssp_set_attestation_data(
    ssp_session_t &sess,
    uint8_t *data_p, uint32_t data_len,
    uint8_t *blob_p, uint32_t *blob_len)
{
    BLOGD("%s\n", __func__);

    keymaster_error_t ret = KM_ERROR_OK;
    ssp_km_message_t ssp_msg = {};

    BLOGD("set attestation data");
    BLOGD("data_len(%u), blob_len(%u)\n", data_len, *blob_len);

    ssp_print_buf("attest_data", data_p, data_len);

    /* set SSP IPC params */
    memset(&ssp_msg, 0x00, sizeof(ssp_msg));
    ssp_msg.command.id = CMD_SSP_SET_ATTESTATION_DATA;
    ssp_msg.set_attestation_data.attestation_data_len = data_len;
    ssp_msg.set_attestation_data.attestation_blob_len = *blob_len;

    /* check validity of ssp msg len */
    EXPECT_KMOK_GOTOEND(validate_ipc_req_message(&ssp_msg));

    /* set ssp msg buffer */
    memcpy(ssp_msg.set_attestation_data.attestation_data,
            data_p,
            ssp_msg.set_attestation_data.attestation_data_len);

    ssp_print_buf("attest_data",
            ssp_msg.set_attestation_data.attestation_data,
            ssp_msg.set_attestation_data.attestation_blob_len);

    /* invoke ssp msg to ssp */
    EXPECT_KMOK_GOTOEND(ssp_invoke(sess, &ssp_msg));

    CHECK_SSPKM_MSG_GOTOEND(ssp_msg);

    EXPECT_KMOK_GOTOEND(validate_ipc_resp_message(&ssp_msg));

    memcpy(blob_p,
        ssp_msg.set_attestation_data.attestation_data,
        ssp_msg.set_attestation_data.attestation_blob_len);
    *blob_len = ssp_msg.set_attestation_data.attestation_blob_len;

    ssp_print_buf("wrapped_attest_data",
        ssp_msg.set_attestation_data.attestation_data,
        ssp_msg.set_attestation_data.attestation_blob_len);

end:
    if (ret != KM_ERROR_OK) {
        BLOGE("%s() exit with %d\n", __func__, ret);
    } else {
        BLOGD("%s() exit with %d\n", __func__, ret);
    }

    return ret;
}


/* for privsion test */
keymaster_error_t ssp_set_sample_attestation_data(
    ssp_session_t &sess,
    uint8_t *blob_p, uint32_t *blob_len)
{
    BLOGD("%s\n", __func__);

    keymaster_error_t ret = KM_ERROR_OK;
    ssp_km_message_t ssp_msg = {};

    /* set SSP IPC params */
    memset(&ssp_msg, 0x00, sizeof(ssp_msg));
    ssp_msg.command.id = CMD_SSP_SET_SAMPLE_ATTESTATION_DATA;
    ssp_msg.set_sample_attestation_data.attestation_blob_len = *blob_len;

    /* check validity of ssp msg len */
    EXPECT_KMOK_GOTOEND(validate_ipc_req_message(&ssp_msg));

    /* invoke ssp msg to ssp */
    EXPECT_KMOK_GOTOEND(ssp_invoke(sess, &ssp_msg));

    /* process reply message */
    CHECK_SSPKM_MSG_GOTOEND(ssp_msg);

    EXPECT_KMOK_GOTOEND(validate_ipc_resp_message(&ssp_msg));

    memcpy(blob_p,
        ssp_msg.set_sample_attestation_data.attestation_blob,
        ssp_msg.set_sample_attestation_data.attestation_blob_len);
    *blob_len = ssp_msg.set_sample_attestation_data.attestation_blob_len;

    ssp_print_buf("wrapped_attest_data",
        ssp_msg.set_sample_attestation_data.attestation_blob,
        ssp_msg.set_sample_attestation_data.attestation_blob_len);

end:
    if (ret != KM_ERROR_OK) {
        BLOGE("%s() exit with %d\n", __func__, ret);
    } else {
        BLOGD("%s() exit with %d\n", __func__, ret);
    }

    return ret;
}

keymaster_error_t ssp_send_system_version(
    ssp_session_t &sess,
    const keymaster_key_param_set_t *params)
{
    BLOGD("%s\n", __func__);

    keymaster_error_t ret = KM_ERROR_OK;
    ssp_km_message_t ssp_msg = {};

    unique_ptr<uint8_t[]> params_blob;
    uint32_t params_blob_size = 0;

    BLOGD("ssp_send_system_version");
    ssp_print_params("system version params", params);

    EXPECT_KMOK_GOTOEND(ssp_serializer_serialize_params(params_blob,
        &params_blob_size,
        params, false, 0, 0));

    ssp_print_buf("version info blob", params_blob.get(), params_blob_size);

    /* set SSP IPC params */
    memset(&ssp_msg, 0x00, sizeof(ssp_msg));
    ssp_msg.command.id = CMD_SSP_SEND_SYSTEM_VERSION;
    ssp_msg.send_system_version.system_version_len = params_blob_size;

    /* check validity of ssp msg len */
    EXPECT_KMOK_GOTOEND(validate_ipc_req_message(&ssp_msg));

    /* set ssp msg buffer */
    memcpy(ssp_msg.send_system_version.system_version,
            params_blob.get(),
            ssp_msg.send_system_version.system_version_len);

    /* invoke ssp msg to ssp */
    EXPECT_KMOK_GOTOEND(ssp_invoke(sess, &ssp_msg));

    /* process reply message */
    CHECK_SSPKM_MSG_GOTOEND(ssp_msg);

    EXPECT_KMOK_GOTOEND(validate_ipc_resp_message(&ssp_msg));

end:
    if (ret != KM_ERROR_OK) {
        BLOGE("%s() exit with %d\n", __func__, ret);
    } else {
        BLOGD("%s() exit with %d\n", __func__, ret);
    }

    return ret;
}


keymaster_error_t ssp_get_hmac_sharing_parameters(
    ssp_session_t &sess,
    keymaster_hmac_sharing_params_t *params)
{
    BLOGD("%s\n", __func__);

    ssp_km_message_t ssp_msg = {};
    keymaster_error_t ret = KM_ERROR_OK;

    params->seed.data = NULL;
    params->seed.data_length = 0;

    /* set SSP IPC params */
    memset(&ssp_msg, 0x00, sizeof(ssp_msg));
    ssp_msg.command.id = CMD_SSP_GET_HMAC_SHARING_PARAMETERS;

    /* check validity of ssp msg len */
    EXPECT_KMOK_GOTOEND(validate_ipc_req_message(&ssp_msg));

    /* invoke ssp msg to ssp */
    EXPECT_KMOK_GOTOEND(ssp_invoke(sess, &ssp_msg));

    /* process reply message */
    CHECK_SSPKM_MSG_GOTOEND(ssp_msg);

    /* validate output params */
    EXPECT_KMOK_GOTOEND(validate_ipc_resp_message(&ssp_msg));

    /* copy nonce */
    params->nonce.data_length = ssp_msg.get_hmac_sharing_parameters.nonce_len;
    params->nonce.data = (uint8_t *)calloc(ssp_msg.get_hmac_sharing_parameters.nonce_len,
                                          sizeof(uint8_t));
    memcpy((uint8_t *)params->nonce.data,
        ssp_msg.get_hmac_sharing_parameters.nonce,
        ssp_msg.get_hmac_sharing_parameters.nonce_len);

    /* copy seed */
    params->seed.data_length = ssp_msg.get_hmac_sharing_parameters.seed_len;
    params->seed.data = (uint8_t *)calloc(ssp_msg.get_hmac_sharing_parameters.seed_len,
                                          sizeof(uint8_t));
    memcpy((uint8_t *)params->seed.data,
        ssp_msg.get_hmac_sharing_parameters.seed,
        ssp_msg.get_hmac_sharing_parameters.seed_len);

    ssp_print_buf("get_hmac_nonce",
                  (uint8_t *)params->nonce.data,
                  ssp_msg.get_hmac_sharing_parameters.nonce_len);
    ssp_print_buf("get_hmac_seed",
                  (uint8_t *)params->seed.data,
                  params->seed.data_length);

end:
    if (ret != KM_ERROR_OK) {
        if (params->seed.data) {
            free((void*)params->seed.data);
        }
        BLOGE("%s() exit with %d\n", __func__, ret);
    } else {
        BLOGD("%s() exit with %d\n", __func__, ret);
    }

    return ret;
}

keymaster_error_t ssp_compute_shared_hmac(
    ssp_session_t &sess,
    const keymaster_hmac_sharing_params_set_t *params,
    keymaster_blob_t *sharing_check)
{
    BLOGD("%s\n", __func__);

    keymaster_error_t ret = KM_ERROR_OK;
    ssp_km_message_t ssp_msg = {};

    int i = 0;

    /*
     * Currently support only two keymasters. TEE and Strongbox
     */
    if (params->length < 1) {
        BLOGE("Keymasters should be at least one for compute shared HMAC. km num(%lu)\n",
            params->length);
        return KM_ERROR_INVALID_ARGUMENT;
    }

    /* check seed data/length */
    for (i = 0; i < params->length; i++) {
        if (params->params[i].seed.data_length == 0) {
            if (params->params[i].seed.data != NULL) {
                BLOGE("input parameters mismatch");
                return KM_ERROR_INVALID_ARGUMENT;
            }
        } else if (params->params[i].seed.data_length == SSP_HMAC_SHARING_SEED_SIZE) {
            if (params->params[i].seed.data == NULL) {
                BLOGE("input parameters mismatch");
                return KM_ERROR_INVALID_ARGUMENT;
            }
        } else {
            BLOGE("wrong seed legnth");
            return KM_ERROR_INVALID_ARGUMENT;
        }
    }

    /* check nonce data/length */
    for (i = 0; i < params->length; i++) {
        if (params->params[i].nonce.data_length == 0) {
            if (params->params[i].nonce.data != NULL) {
                BLOGE("input parameters mismatch");
                return KM_ERROR_INVALID_ARGUMENT;
            }
        } else if (params->params[i].nonce.data_length == SSP_HMAC_SHARING_SEED_SIZE) {
            if (params->params[i].nonce.data == NULL) {
                BLOGE("input parameters mismatch");
                return KM_ERROR_INVALID_ARGUMENT;
            }
        } else {
            BLOGE("wrong nonce legnth");
            return KM_ERROR_INVALID_ARGUMENT;
        }
    }

    /* set SSP IPC params */
    memset(&ssp_msg, 0x00, sizeof(ssp_msg));
    ssp_msg.command.id = CMD_SSP_COMPUTE_SHARED_HAMC;
    ssp_msg.compute_shared_hmac.params_cnt = params->length;

    for (i = 0; i < params->length; i++) {
        ssp_msg.compute_shared_hmac.params[i].seed_len =
            params->params[i].seed.data_length;
        ssp_msg.compute_shared_hmac.params[i].nonce_len =
            params->params[i].nonce.data_length;
    }

    /* check validity of ssp msg len */
    EXPECT_KMOK_GOTOEND(validate_ipc_req_message(&ssp_msg));

    /* set ssp msg buffer */
    for (i = 0; i < params->length; i++) {
        if (ssp_msg.compute_shared_hmac.params[i].nonce_len > 0) {
            memcpy(ssp_msg.compute_shared_hmac.params[i].nonce,
                    params->params[i].nonce.data,
                    ssp_msg.compute_shared_hmac.params[i].nonce_len);
        }
        if (ssp_msg.compute_shared_hmac.params[i].seed_len > 0) {
            memcpy(ssp_msg.compute_shared_hmac.params[i].seed,
                    params->params[i].seed.data,
                    ssp_msg.compute_shared_hmac.params[i].seed_len);
        }
    }

    /* invoke ssp msg to ssp */
    EXPECT_KMOK_GOTOEND(ssp_invoke(sess, &ssp_msg));

    /* process reply message */
    CHECK_SSPKM_MSG_GOTOEND(ssp_msg);

    EXPECT_KMOK_GOTOEND(validate_ipc_resp_message(&ssp_msg));

    if (ssp_msg.compute_shared_hmac.sharing_check_len > 0) {
        sharing_check->data = (uint8_t *)calloc(ssp_msg.compute_shared_hmac.sharing_check_len,
                                                sizeof(uint8_t));
        if (sharing_check->data) {
            sharing_check->data_length = ssp_msg.compute_shared_hmac.sharing_check_len;
            memcpy((uint8_t *)sharing_check->data,
                ssp_msg.compute_shared_hmac.sharing_check,
                ssp_msg.compute_shared_hmac.sharing_check_len);
        } else {
            BLOGE("Fail to alloc sharing check buf. len(%u)\n",
                    ssp_msg.compute_shared_hmac.sharing_check_len);
            ret = KM_ERROR_INSUFFICIENT_BUFFER_SPACE;
            goto end;
        }
    } else {
        sharing_check->data = NULL;
        sharing_check->data_length = 0;
    }

    ssp_print_buf("sharing_check", sharing_check->data, sharing_check->data_length);

end:
    if (ret != KM_ERROR_OK) {
        BLOGE("%s() exit with %d\n", __func__, ret);
    } else {
        BLOGD("%s() exit with %d\n", __func__, ret);
    }

    return ret;
}

keymaster_error_t ssp_add_rng_entropy(
    ssp_session_t &sess,
    const uint8_t* data,
    size_t length)
{
    BLOGD("%s\n", __func__);

    keymaster_error_t ret = KM_ERROR_OK;
    ssp_km_message_t ssp_msg = {};

    ssp_print_buf("add entropy", data, length);

    /* set SSP IPC params */
    memset(&ssp_msg, 0x00, sizeof(ssp_msg));
    ssp_msg.command.id = CMD_SSP_ADD_RNG_ENTROPY;
    ssp_msg.add_rng_entropy.rng_entropy_len = length;

    /* check validity of ssp msg len */
    EXPECT_KMOK_GOTOEND(validate_ipc_req_message(&ssp_msg));

    /* set ssp msg buffer */
    memcpy(ssp_msg.add_rng_entropy.rng_entropy, data, length);

    /* invoke ssp msg to ssp */
    EXPECT_KMOK_GOTOEND(ssp_invoke(sess, &ssp_msg));

    /* process reply message */
    CHECK_SSPKM_MSG_GOTOEND(ssp_msg);

    EXPECT_KMOK_GOTOEND(validate_ipc_resp_message(&ssp_msg));

end:
    if (ret != KM_ERROR_OK) {
        BLOGE("%s() exit with %d\n", __func__, ret);
    } else {
        BLOGD("%s() exit with %d\n", __func__, ret);
    }

    return ret;
}

static bool is_valid_public_exponent(uint64_t exp)
{
    /* supported max exponent is F4 */
    if ((exp % 2 == 1) && (exp != 1) && (exp <= 65537)) {
        return true;
    } else {
        return false;
    }
}

keymaster_error_t ssp_generate_key(
    ssp_session_t &sess,
    const keymaster_key_param_set_t* params,
    keymaster_key_blob_t* key_blob,
    keymaster_key_characteristics_t* characteristics)
{
    BLOGD("%s\n", __func__);

    keymaster_error_t ret = KM_ERROR_OK;
    ssp_km_message_t ssp_msg = {};

    keymaster_algorithm_t algorithm = (keymaster_algorithm_t)0;
    keymaster_ec_curve_t ec_curve = (keymaster_ec_curve_t)0;
    uint32_t keysize_bits = 0;
    uint64_t rsa_exponent = 0;
    uint32_t encodekey_size = 0;

    unique_ptr<uint8_t[]> params_blob;
    uint32_t params_blob_size = 0;
    uint32_t char_blob_size = 0;

    ssp_print_params("genkey params", params);
    ssp_print_blob("key_blob", key_blob);

    EXPECT_NE_NULL_GOTOEND(params);
    EXPECT_NE_NULL_GOTOEND(key_blob);

    if (characteristics) {
        characteristics->hw_enforced = {NULL, 0};
        characteristics->sw_enforced = {NULL, 0};
    }

    key_blob->key_material = NULL;

    /* todo: check input params */
    EXPECT_KMOK_GOTOEND(validate_key_params(params));

    EXPECT_KMOK_GOTOEND(get_tag_value_enum(params,
                KM_TAG_ALGORITHM, reinterpret_cast<uint32_t*>(&algorithm)));

    if (get_tag_value_int(params, KM_TAG_KEY_SIZE, &keysize_bits) != KM_ERROR_OK) {
        if ((algorithm == KM_ALGORITHM_EC) &&
                (get_tag_value_enum(params,
                                    KM_TAG_EC_CURVE, reinterpret_cast<uint32_t*>(&ec_curve)) == KM_ERROR_OK))
        {
            keysize_bits = ec_curve_to_keysize(ec_curve);
        } else {
            ret = KM_ERROR_UNSUPPORTED_KEY_SIZE;
            goto end;
        }
    }

    EXPECT_TRUE_GOTOEND(KM_ERROR_UNSUPPORTED_KEY_SIZE,
            check_supported_key_size(algorithm, keysize_bits));

    if (algorithm == KM_ALGORITHM_RSA) {
        if (get_tag_value_long(params,
                    KM_TAG_RSA_PUBLIC_EXPONENT, &rsa_exponent) == KM_ERROR_OK)
        {
            EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT,
                    is_valid_public_exponent(rsa_exponent));
        } else {
            /* todo: check need to set default exponent ? */
            return KM_ERROR_INVALID_ARGUMENT;
        }
    }

    EXPECT_KMOK_GOTOEND(ssp_serializer_serialize_params(params_blob,
                &params_blob_size,
                params,
                false, /* not add KM_TAG_CREATION_DATETIME into extra params */
                0, rsa_exponent));

    /* alloc buffer for key blob */
    if (algorithm == KM_ALGORITHM_TRIPLE_DES) {
        EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT, keysize_bits == 168);
        encodekey_size = (168 + 24);
    } else {
        encodekey_size = keysize_bits;
    }

    key_blob->key_material_size = ssp_key_blob_size_max(
            algorithm, encodekey_size, params_blob_size);
    key_blob->key_material = (uint8_t *)calloc(key_blob->key_material_size, sizeof(uint8_t));
    if (!key_blob->key_material) {
        BLOGE("malloc fail for key blob. key_material_size(%zu)\n",
                key_blob->key_material_size);
        return KM_ERROR_MEMORY_ALLOCATION_FAILED;
    }

    BLOGD("key_blob->key_material_size(%zu)\n", key_blob->key_material_size);

    if (characteristics != NULL) {
        /* alloc buffer for the key characteristics. */
        char_blob_size = params_blob_size + SB_EXTRA_PARAMS_MAX_SIZE;
    } else {
        char_blob_size = 0;
    }

    /* set SSP IPC params */
    memset(&ssp_msg, 0x00, sizeof(ssp_msg));
    ssp_msg.command.id = CMD_SSP_GENERATE_KEY;
    ssp_msg.generate_key.algorithm = algorithm;
    ssp_msg.generate_key.keysize = encodekey_size;
    ssp_msg.generate_key.keyparams_len = params_blob_size;
    ssp_msg.generate_key.keyblob_len = key_blob->key_material_size;
    ssp_msg.generate_key.keychar_len = char_blob_size;
    if (algorithm == KM_ALGORITHM_RSA) {
        ssp_msg.generate_key.keyoption = (uint32_t)rsa_exponent;
    }

    /* check validity of ssp msg len */
    EXPECT_KMOK_GOTOEND(validate_ipc_req_message(&ssp_msg));

    /* set ssp msg buffer */
    memcpy(ssp_msg.generate_key.keyparams,
            params_blob.get(),
            ssp_msg.generate_key.keyparams_len);

    /* invoke ssp msg to ssp */
    EXPECT_KMOK_GOTOEND(ssp_invoke(sess, &ssp_msg));

    /* process reply message */
    CHECK_SSPKM_MSG_GOTOEND(ssp_msg);

    EXPECT_KMOK_GOTOEND(validate_ipc_resp_message(&ssp_msg));

    /* Update key blob length to the real size */
    key_blob->key_material_size = ssp_msg.generate_key.keyblob_len;
    memcpy((uint8_t *)key_blob->key_material,
            ssp_msg.generate_key.keyblob,
            ssp_msg.generate_key.keyblob_len);

    if (characteristics != NULL) {
        EXPECT_KMOK_GOTOEND(ssp_serializer_deserialize_characteristics(
                    ssp_msg.generate_key.keychar, char_blob_size, characteristics));
    }

    ssp_print_blob("key_blob", key_blob);
    ssp_print_buf("char_blob", ssp_msg.generate_key.keychar, char_blob_size);
    if (characteristics != NULL) {
        ssp_print_params("hwenforce params", &characteristics->hw_enforced);
        ssp_print_params("swenforce params", &characteristics->sw_enforced);
    }
end:
    if (ret != KM_ERROR_OK) {
        if (key_blob) {
            if (key_blob->key_material) {
                free((void*)key_blob->key_material);
                key_blob->key_material = NULL;
                key_blob->key_material_size = 0;
            }
        }

        if (characteristics != NULL) {
            keymaster_free_characteristics(characteristics);
        }
    }

    if (ret != KM_ERROR_OK) {
        BLOGE("%s() exit with %d\n", __func__, ret);
    } else {
        BLOGD("%s() exit with %d\n", __func__, ret);
    }

    return ret;
}

keymaster_error_t ssp_get_key_characteristics(
        ssp_session_t &sess,
        const keymaster_key_blob_t* key_blob,
        const keymaster_blob_t* client_id,
        const keymaster_blob_t* app_data,
        keymaster_key_characteristics_t* characteristics)
{
    BLOGD("%s\n", __func__);

    keymaster_error_t ret = KM_ERROR_OK;
    ssp_km_message_t ssp_msg = {};
    size_t keychar_max = 0;

    EXPECT_NE_NULL_GOTOEND(key_blob);
    EXPECT_NE_NULL_GOTOEND(characteristics);

    ssp_print_blob("key_blob", key_blob);
    ssp_print_blob("client_id", client_id);
    ssp_print_blob("app_data", app_data);

    if (client_id != NULL) {
        if (client_id->data_length > KM_TAG_APPLICATION_ID_MAX_LEN) {
            BLOGE("client_id length is too long. len(%zu)", client_id->data_length);
            ret = KM_ERROR_INSUFFICIENT_BUFFER_SPACE;
            goto end;
        }
    }

    if (app_data != NULL) {
        if (app_data->data_length > KM_TAG_APPLICATION_DATA_MAX_LEN) {
            BLOGE("app_data length is too long. len(%zu)", app_data->data_length);
            ret = KM_ERROR_INSUFFICIENT_BUFFER_SPACE;
            goto end;
        }
    }

    /* Initialize characteristics */
    if (characteristics != NULL) {
        characteristics->hw_enforced.length = 0;
        characteristics->hw_enforced.params = NULL;
        characteristics->sw_enforced.length = 0;
        characteristics->sw_enforced.params = NULL;
    }

    keychar_max = key_blob->key_material_size;

    /* set SSP IPC params */
    memset(&ssp_msg, 0x00, sizeof(ssp_msg));
    ssp_msg.command.id = CMD_SSP_GET_KEY_CHARACTERISTICS;
    ssp_msg.get_key_characteristics.keyblob_len = key_blob->key_material_size;
    ssp_msg.get_key_characteristics.keychar_len = keychar_max;

    if (client_id != NULL && client_id->data_length > 0) {
        ssp_msg.get_key_characteristics.client_id_len = client_id->data_length;
    } else {
        ssp_msg.get_key_characteristics.client_id_len = 0;
    }

    if (app_data != NULL && app_data->data_length > 0) {
        ssp_msg.get_key_characteristics.app_data_len = app_data->data_length;
    } else {
        ssp_msg.get_key_characteristics.app_data_len = 0;
    }

    /* check validity of ssp msg len */
    EXPECT_KMOK_GOTOEND(validate_ipc_req_message(&ssp_msg));

    /* set ssp msg buffer */
    memcpy(ssp_msg.get_key_characteristics.keyblob,
            key_blob->key_material,
            ssp_msg.get_key_characteristics.keyblob_len);

    if (client_id != NULL && client_id->data_length > 0) {
        memcpy(ssp_msg.get_key_characteristics.client_id,
                client_id->data,
                client_id->data_length);
    }
    if (app_data != NULL && app_data->data_length > 0) {
        memcpy(ssp_msg.get_key_characteristics.app_data,
                app_data->data,
                app_data->data_length);
    }

    /* invoke ssp msg to ssp */
    EXPECT_KMOK_GOTOEND(ssp_invoke(sess, &ssp_msg));

    /* process reply message */
    CHECK_SSPKM_MSG_GOTOEND(ssp_msg);

    EXPECT_KMOK_GOTOEND(validate_ipc_resp_message(&ssp_msg));

    EXPECT_KMOK_GOTOEND(ssp_serializer_deserialize_characteristics(
                ssp_msg.get_key_characteristics.keychar,
                ssp_msg.get_key_characteristics.keychar_len,
                characteristics));

    ssp_print_buf("char_blob", ssp_msg.get_key_characteristics.keychar,
            ssp_msg.get_key_characteristics.keychar_len);
    if (characteristics != NULL) {
        ssp_print_params("hwenforce params", &characteristics->hw_enforced);
        ssp_print_params("swenforce params", &characteristics->sw_enforced);
    }

end:
    if (ret != KM_ERROR_OK) {
        if (characteristics != NULL) {
            keymaster_free_characteristics(characteristics);
        }
    }

    if (ret != KM_ERROR_OK) {
        BLOGE("%s() exit with %d\n", __func__, ret);
    } else {
        BLOGD("%s() exit with %d\n", __func__, ret);
    }

    return ret;
}

keymaster_error_t ssp_import_key(
        ssp_session_t &sess,
        const keymaster_key_param_set_t* params,
        keymaster_key_format_t key_format,
        const keymaster_blob_t* key_data,
        keymaster_key_blob_t* key_blob,
        keymaster_key_characteristics_t* characteristics)
{
    BLOGD("%s\n", __func__);

    keymaster_error_t ret = KM_ERROR_OK;
    ssp_km_message_t ssp_msg = {};

    keymaster_algorithm_t algorithm = (keymaster_algorithm_t)0;
    keymaster_ec_curve_t ec_curve = (keymaster_ec_curve_t)0;
    uint32_t keysize_bits = 0;

    unique_ptr<uint8_t[]> parsed_keydata;
    size_t parsed_keydata_len = 0;
    uint32_t parsed_key_size = 0;
    uint32_t real_key_size = 0;
    uint64_t tag_rsa_exp = 0;
    uint64_t rsa_exp = 0;

    unique_ptr<uint8_t[]> params_blob;
    uint32_t params_blob_size = 0;
    uint32_t char_blob_size = 0;

    ssp_print_params("import key params", params);
    ssp_print_buf("key_data", key_data->data, key_data->data_length);

    EXPECT_NE_NULL_GOTOEND(params);
    EXPECT_NE_NULL_GOTOEND(key_data);
    EXPECT_NE_NULL_GOTOEND(key_blob);

    if (characteristics) {
        characteristics->hw_enforced = {NULL, 0};
        characteristics->sw_enforced = {NULL, 0};
    }

    key_blob->key_material = NULL;

    /* todo: check input params */
    EXPECT_KMOK_GOTOEND(validate_key_params(params));

    /* extract algorithm from keyparam */
    EXPECT_KMOK_GOTOEND(get_tag_value_enum(params,
                KM_TAG_ALGORITHM, reinterpret_cast<uint32_t*>(&algorithm)));

    /* extract keysize from keyparam */
    if (get_tag_value_int(params, KM_TAG_KEY_SIZE, &keysize_bits) != KM_ERROR_OK) {
        BLOGD("there is no provided key size in Tags");
        if (algorithm == KM_ALGORITHM_EC) {
            if (get_tag_value_enum(
                        params,
                        KM_TAG_EC_CURVE,
                        reinterpret_cast<uint32_t*>(&ec_curve)) == KM_ERROR_OK) {
                keysize_bits = ec_curve_to_keysize(ec_curve);
            }
        }
    } else {
        BLOGD("there is provided key size in Tags, %u", keysize_bits);
    }

    /* decode keydata and serialize it */
    if (key_format == KM_KEY_FORMAT_PKCS8) {
        switch (algorithm) {
            case KM_ALGORITHM_RSA:
                EXPECT_KMOK_GOTOEND(ssp_keyparser_parse_keydata_pkcs8rsa(
                            parsed_keydata,
                            &parsed_keydata_len,
                            key_data->data,
                            key_data->data_length));
                EXPECT_KMOK_GOTOEND(ssp_keyparser_get_rsa_exponent_from_parsedkey(
                            parsed_keydata.get(), parsed_keydata_len, &rsa_exp));
                if (get_tag_value_long(params,
                            KM_TAG_RSA_PUBLIC_EXPONENT,
                            &tag_rsa_exp) == KM_ERROR_OK) {
                    BLOGD("tag rsa exp : %lu, rsa_exp : %lu", tag_rsa_exp, rsa_exp);
                    if (tag_rsa_exp != rsa_exp) {
                        ret = KM_ERROR_IMPORT_PARAMETER_MISMATCH;
                        goto end;
                    }
                }
                break;
            case KM_ALGORITHM_EC:
                EXPECT_KMOK_GOTOEND(ssp_keyparser_parse_keydata_pkcs8ec(
                            parsed_keydata,
                            &parsed_keydata_len,
                            key_data->data,
                            key_data->data_length));
                break;
            default:
                BLOGE("ImportKey request: unsupported algoritm: %d", algorithm);
                ret = KM_ERROR_UNSUPPORTED_ALGORITHM;
                break;
        }
    } else {
        EXPECT_KMOK_GOTOEND(ssp_keyparser_parse_keydata_raw(
                    algorithm,
                    parsed_keydata,
                    &parsed_keydata_len,
                    key_data->data,
                    key_data->data_length));
    }

    if (ret != KM_ERROR_OK) {
        BLOGE("Key format converting failed");
        goto end;
    }
    if (parsed_keydata_len < SSP_SERIALIZED_KEY_LEN_MIN) {
        ret = KM_ERROR_INVALID_INPUT_LENGTH;
        goto end;
    }

    BLOGD("get keysize from parsed key");
    ssp_keyparser_get_keysize_from_parsedkey(parsed_keydata.get(), &parsed_key_size);

    /* set keysize */
    if (algorithm == KM_ALGORITHM_TRIPLE_DES) {
        EXPECT_TRUE_GOTOEND(KM_ERROR_UNSUPPORTED_KEY_SIZE, parsed_key_size == 192);
        real_key_size = 168;
    } else {
        real_key_size = parsed_key_size;
    }

    BLOGD("parsed key size = %u", parsed_key_size);

    BLOGD("compare keysize between parsed key size and provided size in TAG");
    /* compare keysize between parsed size and size in TAG */
    if (keysize_bits != 0) {
        if (keysize_bits != real_key_size) {
            ret = KM_ERROR_IMPORT_PARAMETER_MISMATCH;
            BLOGE("keysize_bits(%u) != real_key_size(%u)\n",
                    keysize_bits, real_key_size);

            goto end;
        }
    } else {
        keysize_bits = real_key_size;
    }

    EXPECT_TRUE_GOTOEND(KM_ERROR_UNSUPPORTED_KEY_SIZE,
            check_supported_key_size(algorithm, keysize_bits));

    /* serialize key params */
    EXPECT_KMOK_GOTOEND(ssp_serializer_serialize_params(params_blob,
                &params_blob_size,
                params, false, keysize_bits, rsa_exp));

    if (params_blob_size > SSP_KEY_CHARACTERISTICS_COMMON_MAX_LEN) {
        BLOGE("params_blob_size(%d) > KM_KEY_PARAMS_TOTAL_MAX_LEN(%d)\n",
                params_blob_size, KM_KEY_PARAMS_TOTAL_MAX_LEN);
        ret = KM_ERROR_INSUFFICIENT_BUFFER_SPACE;
        goto end;
    }

    /* alloc buffer for key blob */
    key_blob->key_material_size = ssp_key_blob_size_max(
            algorithm, parsed_key_size, params_blob_size);
    key_blob->key_material = (uint8_t *)calloc(key_blob->key_material_size, sizeof(uint8_t));
    if (!key_blob->key_material) {
        BLOGE("malloc fail for key blob. key_material_size(%zu)\n",
                key_blob->key_material_size);
        ret = KM_ERROR_MEMORY_ALLOCATION_FAILED;
        goto end;
    }

    if (characteristics != NULL) {
        /* set char_blob_size for the key characteristics. */
        char_blob_size = params_blob_size + SB_EXTRA_PARAMS_MAX_SIZE;
    } else {
        char_blob_size = 0;
    }

    /* set SSP IPC params */
    memset(&ssp_msg, 0x00, sizeof(ssp_msg));
    ssp_msg.command.id = CMD_SSP_IMPORT_KEY;
    ssp_msg.import_key.keyparams_len = params_blob_size;
    ssp_msg.import_key.keydata_len = parsed_keydata_len;
    ssp_msg.import_key.keyblob_len = key_blob->key_material_size;
    ssp_msg.import_key.keychar_len = char_blob_size;

    /* check validity of ssp msg len */
    EXPECT_KMOK_GOTOEND(validate_ipc_req_message(&ssp_msg));

    /* set ssp msg buffer */
    memset(ssp_msg.import_key.keyparams, 0x00, sizeof(ssp_msg.import_key.keyparams));
    memcpy(ssp_msg.import_key.keyparams,
            params_blob.get(),
            ssp_msg.import_key.keyparams_len);

    memset(ssp_msg.import_key.keydata, 0x00, sizeof(ssp_msg.import_key.keydata));
    memcpy(ssp_msg.import_key.keydata,
            parsed_keydata.get(),
            ssp_msg.import_key.keydata_len);

    /* invoke ssp msg to ssp */
    EXPECT_KMOK_GOTOEND(ssp_invoke(sess, &ssp_msg));

    /* process reply message */
    CHECK_SSPKM_MSG_GOTOEND(ssp_msg);

    EXPECT_KMOK_GOTOEND(validate_ipc_resp_message(&ssp_msg));

    /* Update key blob length to the real size */
    key_blob->key_material_size = ssp_msg.import_key.keyblob_len;
    memcpy((uint8_t *)key_blob->key_material,
            ssp_msg.import_key.keyblob,
            key_blob->key_material_size);

    if (characteristics != NULL) {
        EXPECT_KMOK_GOTOEND(ssp_serializer_deserialize_characteristics(
                    ssp_msg.import_key.keychar,
                    ssp_msg.import_key.keychar_len,
                    characteristics));
    }

    ssp_print_buf("key_blob", key_blob->key_material, key_blob->key_material_size);
    ssp_print_buf("char_blob", ssp_msg.import_key.keychar, ssp_msg.import_key.keychar_len);
    if (characteristics != NULL) {
        ssp_print_params("hwenforce params", &characteristics->hw_enforced);
        ssp_print_params("swenforce params", &characteristics->sw_enforced);
    }

end:
    if (ret != KM_ERROR_OK) {
        if (key_blob) {
            if (key_blob->key_material) {
                free((void*)key_blob->key_material);
                key_blob->key_material = NULL;
                key_blob->key_material_size = 0;
            }
        }

        if (characteristics != NULL) {
            keymaster_free_characteristics(characteristics);
        }
    }

    if (ret != KM_ERROR_OK) {
        BLOGE("%s() exit with %d\n", __func__, ret);
    } else {
        BLOGD("%s() exit with %d\n", __func__, ret);
    }

    return ret;
}

keymaster_error_t ssp_import_wrappedkey(
        ssp_session_t &sess,
        const keymaster_blob_t* wrapped_key_data,
        const keymaster_key_blob_t* wrapping_keyblob,
        const keymaster_blob_t* masking_key,
        const keymaster_key_param_set_t* unwrapping_params,
        uint64_t pwd_sid,
        uint64_t bio_sid,
        keymaster_key_blob_t* key_blob,
        keymaster_key_characteristics_t* characteristics)
{
    BLOGD("%s\n", __func__);

    keymaster_error_t ret = KM_ERROR_OK;
    ssp_km_message_t ssp_msg = {};

    keymaster_key_format_t key_format = (keymaster_key_format_t)0;
    keymaster_algorithm_t key_type = (keymaster_algorithm_t)0;
    keymaster_blob_t encrypted_transport_key = {};
    keymaster_blob_t key_description = {};
    keymaster_blob_t encrypted_key = {};
    keymaster_blob_t auth_list = {};
    keymaster_blob_t tag = {};
    keymaster_blob_t iv = {};

    uint8_t initialization_vector[16] = {};
    uint8_t aes_gcm_tag[16] = {};
    uint32_t hw_auth_type = 0;

    unique_ptr<uint8_t[]> key_params_blob;
    size_t key_params_blob_size = 0;

    unique_ptr<uint8_t[]> unwrapping_params_blob;
    uint32_t unwrapping_params_blob_size = 0;

    uint32_t char_blob_size = 0;

    EXPECT_NE_NULL_GOTOEND(key_blob);
    EXPECT_NE_NULL_GOTOEND(wrapped_key_data);
    EXPECT_NE_NULL_GOTOEND(wrapping_keyblob);
    EXPECT_NE_NULL_GOTOEND(masking_key);

    key_blob->key_material = NULL;

    if (characteristics) {
        characteristics->hw_enforced = {NULL, 0};
        characteristics->sw_enforced = {NULL, 0};
    }

    BLOGD("pwd_sid: %lu, bio_sid:%lu\n", pwd_sid, bio_sid);

    iv.data = initialization_vector;
    tag.data = aes_gcm_tag;
    EXPECT_KMOK_GOTOEND(ssp_keyparser_parse_wrappedkey(
                &encrypted_transport_key,
                &iv,
                &key_description,
                &key_format,
                &auth_list,
                &encrypted_key,
                &tag,
                wrapped_key_data));

    EXPECT_KMOK_GOTOEND(ssp_keyparser_make_authorization_list(
                key_params_blob, &key_params_blob_size,
                &key_type, &hw_auth_type,
                auth_list.data, auth_list.data_length));
    EXPECT_TRUE_GOTOEND(KM_ERROR_INVALID_ARGUMENT, key_type != 0);

    BLOGD("hw_auth_type: %u\n", hw_auth_type);

    EXPECT_KMOK_GOTOEND(validate_key_params(unwrapping_params));
    EXPECT_KMOK_GOTOEND(ssp_serializer_serialize_params(unwrapping_params_blob,
                &unwrapping_params_blob_size,
                unwrapping_params, false, 0, 0));

    if (unwrapping_params_blob_size > SSP_KEY_CHARACTERISTICS_COMMON_MAX_LEN) {
        BLOGE("unwrapping_params_blob_size(%d) > KM_KEY_PARAMS_TOTAL_MAX_LEN(%d)\n",
                unwrapping_params_blob_size, KM_KEY_PARAMS_TOTAL_MAX_LEN);
        ret = KM_ERROR_INSUFFICIENT_BUFFER_SPACE;
        goto end;
    }

    /* alloc buffer for key blob
     * Strongbox Support RSA 2048
     */
    key_blob->key_material_size = ssp_key_blob_size_max(
            KM_ALGORITHM_RSA, 2048, key_params_blob_size);
    key_blob->key_material = (uint8_t *)calloc(key_blob->key_material_size, sizeof(uint8_t));
    if (!key_blob->key_material) {
        BLOGE("malloc fail for key blob. key_material_size(%zu)\n",
                key_blob->key_material_size);
        ret = KM_ERROR_MEMORY_ALLOCATION_FAILED;
        goto end;
    }

    if (characteristics != NULL) {
        /* set char_blob_size for the key characteristics. */
        char_blob_size = key_params_blob_size + SB_EXTRA_PARAMS_MAX_SIZE;
    } else {
        char_blob_size = 0;
    }

    /* set SSP IPC params */
    memset(&ssp_msg, 0x00, sizeof(ssp_msg));
    ssp_msg.command.id = CMD_SSP_IMPORT_WRAPPED_KEY;
    ssp_msg.import_wrappedkey.keyparams_len = key_params_blob_size;
    ssp_msg.import_wrappedkey.encrypted_transport_key_len =
        encrypted_transport_key.data_length;
    ssp_msg.import_wrappedkey.key_description_len = key_description.data_length;
    ssp_msg.import_wrappedkey.encrypted_key_len = encrypted_key.data_length;
    ssp_msg.import_wrappedkey.iv_len = iv.data_length;
    ssp_msg.import_wrappedkey.tag_len = tag.data_length;
    ssp_msg.import_wrappedkey.masking_key_len = masking_key->data_length;
    ssp_msg.import_wrappedkey.wrapping_keyblob_len = wrapping_keyblob->key_material_size;
    ssp_msg.import_wrappedkey.unwrapping_params_blob_len = unwrapping_params_blob_size;
    ssp_msg.import_wrappedkey.keyblob_len = key_blob->key_material_size;
    ssp_msg.import_wrappedkey.keychar_len = char_blob_size;

    ssp_msg.import_wrappedkey.keyformat = key_format;
    ssp_msg.import_wrappedkey.keytype = key_type;
    ssp_msg.import_wrappedkey.pwd_sid = (hw_auth_type & KM_HW_AUTH_TYPE_PASSWORD) ?
        pwd_sid : 0;
    ssp_msg.import_wrappedkey.bio_sid = (hw_auth_type & KM_HW_AUTH_TYPE_FINGERPRINT) ?
        bio_sid : 0;

    /* check validity of ssp msg len */
    EXPECT_KMOK_GOTOEND(validate_ipc_req_message(&ssp_msg));

    /* set ssp msg buffer */
    memset(ssp_msg.import_wrappedkey.keyparams,
            0x00, sizeof(ssp_msg.import_wrappedkey.keyparams));
    memcpy(ssp_msg.import_wrappedkey.keyparams,
            key_params_blob.get(),
            ssp_msg.import_wrappedkey.keyparams_len);

    memset(ssp_msg.import_wrappedkey.encrypted_transport_key,
            0x00, sizeof(ssp_msg.import_wrappedkey.encrypted_transport_key));
    memcpy(ssp_msg.import_wrappedkey.encrypted_transport_key,
            encrypted_transport_key.data,
            ssp_msg.import_wrappedkey.encrypted_transport_key_len);

    memset(ssp_msg.import_wrappedkey.key_description,
            0x00, sizeof(ssp_msg.import_wrappedkey.key_description));
    memcpy(ssp_msg.import_wrappedkey.key_description,
            key_description.data,
            ssp_msg.import_wrappedkey.key_description_len);

    memset(ssp_msg.import_wrappedkey.encrypted_key,
            0x00, sizeof(ssp_msg.import_wrappedkey.encrypted_key));
    memcpy(ssp_msg.import_wrappedkey.encrypted_key,
            encrypted_key.data,
            ssp_msg.import_wrappedkey.encrypted_key_len);

    memset(ssp_msg.import_wrappedkey.iv,
            0x00, sizeof(ssp_msg.import_wrappedkey.iv));
    memcpy(ssp_msg.import_wrappedkey.iv,
            iv.data,
            ssp_msg.import_wrappedkey.iv_len);

    memset(ssp_msg.import_wrappedkey.tag,
            0x00, sizeof(ssp_msg.import_wrappedkey.tag));
    memcpy(ssp_msg.import_wrappedkey.tag,
            tag.data,
            ssp_msg.import_wrappedkey.tag_len);

    memset(ssp_msg.import_wrappedkey.masking_key,
            0x00, sizeof(ssp_msg.import_wrappedkey.masking_key));
    memcpy(ssp_msg.import_wrappedkey.masking_key,
            masking_key->data,
            ssp_msg.import_wrappedkey.masking_key_len);

    memset(ssp_msg.import_wrappedkey.wrapping_keyblob,
            0x00, sizeof(ssp_msg.import_wrappedkey.wrapping_keyblob));
    memcpy(ssp_msg.import_wrappedkey.wrapping_keyblob,
            wrapping_keyblob->key_material,
            ssp_msg.import_wrappedkey.wrapping_keyblob_len);

    memset(ssp_msg.import_wrappedkey.unwrapping_params_blob,
            0x00, sizeof(ssp_msg.import_wrappedkey.unwrapping_params_blob));
    memcpy(ssp_msg.import_wrappedkey.unwrapping_params_blob,
            unwrapping_params_blob.get(),
            ssp_msg.import_wrappedkey.unwrapping_params_blob_len);

    /* invoke ssp msg to ssp */
    EXPECT_KMOK_GOTOEND(ssp_invoke(sess, &ssp_msg));

    /* process reply message */
    CHECK_SSPKM_MSG_GOTOEND(ssp_msg);

    EXPECT_KMOK_GOTOEND(validate_ipc_resp_message(&ssp_msg));

    /* Update key blob length to the real size */
    key_blob->key_material_size = ssp_msg.import_wrappedkey.keyblob_len;
    memcpy((uint8_t *)key_blob->key_material,
            ssp_msg.import_wrappedkey.keyblob,
            key_blob->key_material_size);

    if (characteristics != NULL) {
        EXPECT_KMOK_GOTOEND(ssp_serializer_deserialize_characteristics(
                    ssp_msg.import_wrappedkey.keychar,
                    ssp_msg.import_wrappedkey.keychar_len,
                    characteristics));
    }

    ssp_print_buf("key_blob",
            key_blob->key_material,
            key_blob->key_material_size);
    ssp_print_buf("char_blob",
            ssp_msg.import_wrappedkey.keychar,
            ssp_msg.import_wrappedkey.keychar_len);
    if (characteristics != NULL) {
        ssp_print_params("hwenforce params", &characteristics->hw_enforced);
        ssp_print_params("swenforce params", &characteristics->sw_enforced);
    }

end:

    if (ret != KM_ERROR_OK) {
        if (key_blob) {
            if (key_blob->key_material) {
                free((void*)key_blob->key_material);
                key_blob->key_material = NULL;
                key_blob->key_material_size = 0;
            }
        }

        if (characteristics != NULL) {
            keymaster_free_characteristics(characteristics);
        }
    }

    if (ret != KM_ERROR_OK) {
        BLOGE("%s() exit with %d\n", __func__, ret);
    } else {
        BLOGD("%s() exit with %d\n", __func__, ret);
    }

    return ret;
}

void dump_hwauth_token(const keymaster_hw_auth_token_t *auth_token)
{
    if (auth_token) {
        BLOGD("auth_token:challenge(0x%lx)\n", auth_token->challenge);
        BLOGD("auth_token:user_id(0x%lx)\n", auth_token->user_id);
        BLOGD("auth_token:authenticator_id(0x%lx)\n", auth_token->authenticator_id);
        BLOGD("auth_token:authenticator_type(0x%x)\n", auth_token->authenticator_type);
        BLOGD("auth_token:timestamp(0x%lx)\n", auth_token->timestamp);
        ssp_print_buf("auth_token:mac", auth_token->mac.data, auth_token->mac.data_length);
    }
}

void dump_timestamp_token(const keymaster_timestamp_token_t *token)
{
    if (token) {
        BLOGD("timestamp_token:challenge(0x%lx)\n", token->challenge);
        BLOGD("timestamp_token:timestamp(0x%lx)\n", token->timestamp);
        ssp_print_buf("timestamp_token:mac", token->mac.data, token->mac.data_length);
    }
}

keymaster_error_t ssp_begin(
        ssp_session_t &sess,
        keymaster_purpose_t purpose,
        const keymaster_key_blob_t* keyblob,
        const keymaster_key_param_set_t* in_params,
        const keymaster_hw_auth_token_t *auth_token,
        keymaster_key_param_set_t* out_params,
        keymaster_operation_handle_t* op_handle)
{
    BLOGD("%s()\n", __func__);

    keymaster_error_t ret = KM_ERROR_OK;
    ssp_operation_t *ssp_operation = NULL;
    ssp_km_message_t ssp_msg = {};

    unique_ptr<uint8_t[]> in_params_blob;
    uint32_t in_params_blob_size = 0;

    /* for_debug */
    ssp_print_params("input key params", in_params);
    ssp_print_params("out key params", out_params);

    BLOGD("ssp_km_message_t size(%lu)\n", sizeof(ssp_msg));
    BLOGD("ssp_km_message_t begin size(%lu)\n", sizeof(ssp_msg.begin));
    dump_hwauth_token(auth_token);

    ssp_hwctl_enable();

    EXPECT_NE_NULL_GOTOEND(keyblob);
    EXPECT_NE_NULL_GOTOEND(op_handle);

    /* set default out params */
    if (out_params != NULL) {
        out_params->params = NULL;
        out_params->length = 0;
    }

    /* todo: check input params for begin */
    EXPECT_KMOK_GOTOEND(validate_key_params(in_params));

    EXPECT_KMOK_GOTOEND(ssp_serializer_serialize_params(in_params_blob,
                &in_params_blob_size,
                in_params, false, 0, 0));

    /* set SSP IPC params */
    memset(&ssp_msg, 0x00, sizeof(ssp_msg));
    ssp_msg.command.id = CMD_SSP_BEGIN;
    ssp_msg.begin.in_params_len = in_params_blob_size;
    ssp_msg.begin.keyblob_len = keyblob->key_material ? keyblob->key_material_size : 0;
    ssp_msg.begin.purpose = purpose;
    if (out_params) {
        ssp_msg.begin.out_params_len = SSP_BEGIN_OUT_PARAMS_SIZE;
    } else {
        ssp_msg.begin.out_params_len = 0;
    }

    if (auth_token) {
        ssp_msg.begin.auth_token.challenge = auth_token->challenge;
        ssp_msg.begin.auth_token.user_id = auth_token->user_id;
        ssp_msg.begin.auth_token.authenticator_id = auth_token->authenticator_id;
        ssp_msg.begin.auth_token.authenticator_type = auth_token->authenticator_type;
        ssp_msg.begin.auth_token.timestamp = auth_token->timestamp;
        memset(ssp_msg.begin.auth_token.mac, 0, 32);
        if (auth_token->mac.data_length) {
            if (auth_token->mac.data_length != 32) {
                BLOGE("hw_auth_token len(%zu) != 32\n", auth_token->mac.data_length);
                ret = KM_ERROR_INVALID_ARGUMENT;
                goto end;
            }
            memcpy(ssp_msg.begin.auth_token.mac, auth_token->mac.data, 32);
        }
    } else {
        ssp_msg.begin.auth_token.challenge = 0;
        ssp_msg.begin.auth_token.user_id = 0;
        ssp_msg.begin.auth_token.authenticator_id = 0;
        ssp_msg.begin.auth_token.authenticator_type = KM_HW_AUTH_TYPE_NONE;
        ssp_msg.begin.auth_token.timestamp = 0;
        memset(ssp_msg.begin.auth_token.mac, 0, 32);
    }

    /* check validity of ssp msg len */
    EXPECT_KMOK_GOTOEND(validate_ipc_req_message(&ssp_msg));

    /* set ssp msg buffer */
    if (keyblob->key_material) {
        memcpy(ssp_msg.begin.keyblob, keyblob->key_material, ssp_msg.begin.keyblob_len);
    }
    memcpy(ssp_msg.begin.in_params, in_params_blob.get(), ssp_msg.begin.in_params_len);

    /* invoke ssp msg to ssp */
    EXPECT_KMOK_GOTOEND(ssp_invoke(sess, &ssp_msg));

    /* process reply message */
    CHECK_SSPKM_MSG_GOTOEND(ssp_msg);

    EXPECT_KMOK_GOTOEND(validate_ipc_resp_message(&ssp_msg));

    /* deserialize out params */
    if (out_params != NULL) {
        uint8_t *out_params_ptr = ssp_msg.begin.out_params;
        uint32_t out_params_len = ssp_msg.begin.out_params_len;
        if (out_params_len > 0) {
            EXPECT_KMOK_GOTOEND(ssp_serializer_deserialize_params(
                        &out_params_ptr, &out_params_len, out_params));
        } else {
            out_params->params = NULL;
            out_params->length = 0;
        }

        /* for_debug */
        ssp_print_buf("out params blob", out_params_ptr, out_params_len);
        ssp_print_params("out params", out_params);
    }

    /* Update operation handle */
    EXPECT_KMOK_GOTOEND(alloc_ssp_operation_bucket(
                &sess, &ssp_operation));

    ssp_operation->handle = ssp_msg.begin.op_handle;
    ssp_operation->algorithm = ssp_msg.begin.algorithm;
    ssp_operation->final_len = ssp_msg.begin.final_len;

    *op_handle = ssp_operation->handle;

    /* for_debug */
    BLOGD("op_handle(%lu: 0x%08lx)\n", *op_handle, *op_handle);

end:
    if (ret != KM_ERROR_OK) {
        ssp_hwctl_disable();
        if (out_params != NULL) {
            keymaster_free_param_set(out_params);
            out_params->length = 0;
        }
    }

    if (ret != KM_ERROR_OK) {
        BLOGE("%s() exit with %d\n", __func__, ret);
    } else {
        BLOGD("%s() exit with %d\n", __func__, ret);
    }

    return ret;
}

static keymaster_error_t update_subsample(
        ssp_session_t &sess,
        ssp_operation_t *ssp_operation,
        const keymaster_hw_auth_token_t *auth_token,
        const keymaster_timestamp_token_t *timestamp_token,
        // uint8_t *params_blob, size_t params_blob_size,
        uint8_t *input, size_t input_len, uint32_t *input_consumed_len,
        uint8_t *output, size_t output_len, uint32_t *output_result_len)
{
    keymaster_error_t ret = KM_ERROR_OK;
    ssp_km_message_t ssp_msg = {};

    BLOGD("input(%lx), input_len(%zu), input_consumed_len(%d)\n",
            (uint64_t)input, input_len, input_consumed_len ? *input_consumed_len : 0);
    BLOGD("output(%lx), output_len(%zu), output_result_len(%d)\n",
            (uint64_t)output, output_len, output_result_len ? *output_result_len : 0);

    /* set SSP IPC params */
    memset(&ssp_msg, 0x00, sizeof(ssp_msg));
    ssp_msg.command.id = CMD_SSP_UPDATE;
    ssp_msg.update.op_handle = ssp_operation->handle;
    ssp_msg.update.input_len = (input_len > 0 ? input_len : 0);
    ssp_msg.update.output_len = (output_len > 0 ? output_len : 0);
    ssp_msg.update.output_result_len = (output_len > 0 ? output_len : 0);

    if (auth_token) {
        ssp_msg.update.auth_token.challenge = auth_token->challenge;
        ssp_msg.update.auth_token.user_id = auth_token->user_id;
        ssp_msg.update.auth_token.authenticator_id = auth_token->authenticator_id;
        ssp_msg.update.auth_token.authenticator_type = auth_token->authenticator_type;
        ssp_msg.update.auth_token.timestamp = auth_token->timestamp;
        memset(ssp_msg.update.auth_token.mac, 0, 32);
        if (auth_token->mac.data_length) {
            if (auth_token->mac.data_length != 32) {
                BLOGE("hw_auth_token len(%zu) != 32\n", auth_token->mac.data_length);
                ret = KM_ERROR_INVALID_ARGUMENT;
                goto end;
            }
            memcpy(ssp_msg.update.auth_token.mac, auth_token->mac.data, 32);
        }
    } else {
        ssp_msg.update.auth_token.challenge = 0;
        ssp_msg.update.auth_token.user_id = 0;
        ssp_msg.update.auth_token.authenticator_id = 0;
        ssp_msg.update.auth_token.authenticator_type = KM_HW_AUTH_TYPE_NONE;
        ssp_msg.update.auth_token.timestamp = 0;
        memset(ssp_msg.update.auth_token.mac, 0, 32);
    }

    if (timestamp_token) {
        ssp_msg.update.timestamp_token.challenge = timestamp_token->challenge;
        ssp_msg.update.timestamp_token.timestamp = timestamp_token->timestamp;

        /* set mac */
        memset(ssp_msg.update.timestamp_token.mac, 0, 32);
        if (timestamp_token->mac.data_length) {
            if (timestamp_token->mac.data_length != 32) {
                BLOGE("hw_timestamp_token len(%zu) != 32\n",
                        timestamp_token->mac.data_length);
                ret = KM_ERROR_INVALID_ARGUMENT;
                goto end;
            }
            memcpy(ssp_msg.update.timestamp_token.mac, timestamp_token->mac.data, 32);
        }
    } else {
        ssp_msg.update.timestamp_token.challenge = 0;
        ssp_msg.update.timestamp_token.timestamp = 0;
        memset(ssp_msg.update.timestamp_token.mac, 0, 32);
    }

    /* check validity of ssp msg len */
    EXPECT_KMOK_GOTOEND(validate_ipc_req_message(&ssp_msg));

    /* set ssp msg buffer */
    memset(ssp_msg.update.input, 0x00, sizeof(ssp_msg.update.input));
    if (input != NULL && ssp_msg.update.input_len > 0) {
        memcpy(ssp_msg.update.input, input, ssp_msg.update.input_len);
    }

    /* invoke ssp msg to ssp */
    EXPECT_KMOK_GOTOEND(ssp_invoke(sess, &ssp_msg));

    /* process reply message */
    CHECK_SSPKM_MSG_GOTOEND(ssp_msg);

    EXPECT_KMOK_GOTOEND(validate_ipc_resp_message(&ssp_msg));

    /* update used input size and output data size */
    if (input_consumed_len) {
        *input_consumed_len = ssp_msg.update.input_consumed_len;
    }

    if (output_result_len) {
        BLOGD("output_result_len(%u)\n", ssp_msg.update.output_result_len);
        *output_result_len = ssp_msg.update.output_result_len;
    }

    if (output_len > 0) {
        memcpy(output, ssp_msg.update.output, ssp_msg.update.output_result_len);
    }

end:

    return ret;
}

static keymaster_error_t finish_subsample(
        ssp_session_t &sess,
        ssp_operation_t *ssp_operation,
        const keymaster_hw_auth_token_t *auth_token,
        const keymaster_timestamp_token_t *timestamp_token,
        uint8_t *input, size_t input_len, uint32_t *input_consumed_len,
        uint8_t *output, size_t output_len, uint32_t *output_result_len,
        const keymaster_blob_t *signature,
        const keymaster_blob_t *confirmation_token)
{
    keymaster_error_t ret = KM_ERROR_OK;
    ssp_km_message_t ssp_msg = {};

    /* set SSP IPC params */
    memset(&ssp_msg, 0x00, sizeof(ssp_msg));
    ssp_msg.command.id = CMD_SSP_FINISH;
    ssp_msg.finish.op_handle = ssp_operation->handle;
    if (signature != NULL && signature->data != NULL) {
        ssp_msg.finish.signature_len = signature->data_length;
    } else {
        ssp_msg.finish.signature_len = 0;
    }

    if (confirmation_token != NULL && confirmation_token->data != NULL) {
        ssp_msg.finish.confirmation_token_len = confirmation_token->data_length;
    } else {
        ssp_msg.finish.confirmation_token_len = 0;
    }
    ssp_msg.finish.input_len = (input_len > 0 ? input_len : 0);
    ssp_msg.finish.input_consumed_len= (input_len > 0 ? input_len : 0);
    ssp_msg.finish.output_len = (output_len > 0 ? output_len : 0);
    ssp_msg.finish.output_result_len = (output_len > 0 ? output_len : 0);

    if (auth_token) {
        ssp_msg.finish.auth_token.challenge = auth_token->challenge;
        ssp_msg.finish.auth_token.user_id = auth_token->user_id;
        ssp_msg.finish.auth_token.authenticator_id = auth_token->authenticator_id;
        ssp_msg.finish.auth_token.authenticator_type = auth_token->authenticator_type;
        ssp_msg.finish.auth_token.timestamp = auth_token->timestamp;
        memset(ssp_msg.finish.auth_token.mac, 0, 32);
        if (auth_token->mac.data_length) {
            if (auth_token->mac.data_length != 32) {
                BLOGE("hw_auth_token len(%zu) != 32\n", auth_token->mac.data_length);
                ret = KM_ERROR_INVALID_ARGUMENT;
                goto end;
            }
            memcpy(ssp_msg.finish.auth_token.mac, auth_token->mac.data, 32);
        }
    } else {
        ssp_msg.finish.auth_token.challenge = 0;
        ssp_msg.finish.auth_token.user_id = 0;
        ssp_msg.finish.auth_token.authenticator_id = 0;
        ssp_msg.finish.auth_token.authenticator_type = KM_HW_AUTH_TYPE_NONE;
        ssp_msg.finish.auth_token.timestamp = 0;
        memset(ssp_msg.finish.auth_token.mac, 0, 32);
    }

    if (timestamp_token) {
        ssp_msg.finish.timestamp_token.challenge = timestamp_token->challenge;
        ssp_msg.finish.timestamp_token.timestamp = timestamp_token->timestamp;
        /* set mac */
        memset(ssp_msg.finish.timestamp_token.mac, 0, 32);
        if (timestamp_token->mac.data_length) {
            if (timestamp_token->mac.data_length != 32) {
                BLOGE("hw_timestamp_token len(%zu) != 32\n",
                        timestamp_token->mac.data_length);
                ret = KM_ERROR_INVALID_ARGUMENT;
                goto end;
            }
            memcpy(ssp_msg.finish.timestamp_token.mac, timestamp_token->mac.data, 32);
        }
    } else {
        ssp_msg.finish.timestamp_token.challenge = 0;
        ssp_msg.finish.timestamp_token.timestamp = 0;
        memset(ssp_msg.finish.timestamp_token.mac, 0, 32);
    }

    /* check validity of ssp msg len */
    EXPECT_KMOK_GOTOEND(validate_ipc_req_message(&ssp_msg));

    /* set ssp msg buffer */
    memset(ssp_msg.finish.input,
            0x00, sizeof(ssp_msg.finish.input));
    if (input != NULL && ssp_msg.finish.input_len > 0) {
        memcpy(ssp_msg.finish.input, input, ssp_msg.finish.input_len);
    }

    memset(ssp_msg.finish.signature,
            0x00, sizeof(ssp_msg.finish.signature));
    if (signature != NULL &&
            signature->data != NULL &&
            ssp_msg.finish.signature_len > 0) {
        memcpy(ssp_msg.finish.signature, signature->data, ssp_msg.finish.signature_len);
    }

    memset(ssp_msg.finish.confirmation_token,
            0x00, sizeof(ssp_msg.finish.confirmation_token));
    if (confirmation_token != NULL &&
            confirmation_token->data != NULL &&
            ssp_msg.finish.confirmation_token_len > 0) {
        memcpy(ssp_msg.finish.confirmation_token,
                confirmation_token->data, ssp_msg.finish.confirmation_token_len);
    }

    /* invoke ssp msg to ssp */
    EXPECT_KMOK_GOTOEND(ssp_invoke(sess, &ssp_msg));

    /* process reply message */
    CHECK_SSPKM_MSG_GOTOEND(ssp_msg);

    EXPECT_KMOK_GOTOEND(validate_ipc_resp_message(&ssp_msg));

    /* update used input size and output data size */
    if (input_consumed_len) {
        *input_consumed_len = ssp_msg.finish.input_consumed_len;
    }

    if (output_result_len) {
        *output_result_len = ssp_msg.finish.output_result_len;
    }

    if (output_len > 0) {
        memcpy(output, ssp_msg.finish.output, ssp_msg.finish.output_result_len);
    }

end:

    return ret;
}

static keymaster_error_t subsample_input_data(
        ssp_session_t &sess, ssp_operation_t *ssp_operation,
        const keymaster_blob_t *input, uint32_t *input_consumed_len_r,
        uint8_t *output, const uint8_t *output_eob, size_t *output_result_len_r,
        const keymaster_hw_auth_token_t *auth_token,
        const keymaster_timestamp_token_t *timestamp_token,
        const keymaster_blob_t *signature,
        const keymaster_blob_t *confirmation_token,
        bool is_finish_op)
{
    keymaster_error_t ret = KM_ERROR_OK;

    const uint8_t *input_data = NULL;
    const uint8_t *input_data_eob = NULL;

    unique_ptr<uint8_t[]> input_bucket;
    uint8_t *output_bucket = NULL;
    uint32_t input_bucket_len = 0;
    uint32_t output_bucket_len = 0;

    /* finish operation needs at least last subsample
     * even though there are no input data
     */
    bool need_last_subsample = is_finish_op;

    if (!input || !input->data_length) {
        BLOGD("input len(0)\n");
        input_data = input_data_eob = NULL;
    }
    else {
        input_data = input->data;
        input_data_eob = input_data + input->data_length;
        BLOGD("input len(%zu)\n", input->data_length);
    }

    /* subsampling input data and send input subsample
     * In case of finish(),
     * finish() should called even there are no input data
     */
    while (input_data || need_last_subsample) {
        bool is_last_subsample = true;
        size_t subsample_bucket_len = SSP_DATA_SUBSAMPLE_SIZE;

        need_last_subsample = false;

        if (input_data != NULL && subsample_bucket_len > 0) {
            size_t n = input_data_eob - input_data;
            if (n > subsample_bucket_len) {
                n = subsample_bucket_len;
                is_last_subsample = false;
            }
            subsample_bucket_len -= n;

            input_bucket_len = n;
            input_bucket.reset(new (std::nothrow) uint8_t[n]);

            memcpy(input_bucket.get(), input_data, n);
        }

        if (output) {
            size_t n = output_eob - output;
            if (!is_last_subsample) {
                /* need tag buffer(16bytes) */
                size_t avail_len = input_bucket_len + SSP_RESERVED_BUF_FOR_TAG_LEN;

                if (n > avail_len) {
                    n = avail_len;
                }
            }
            output_bucket = output;
            output_bucket_len = n;
            BLOGD("output_bucket(%lx), output_bucket_len(%u)\n",
                    (uint64_t)output, output_bucket_len);
        }

        uint32_t input_consumed_len = 0;
        uint32_t output_result_len = 0;

        if (is_last_subsample && is_finish_op) {
            EXPECT_KMOK_GOTOEND(finish_subsample(
                        sess, ssp_operation,
                        auth_token,
                        timestamp_token,
                        input_bucket.get(), input_bucket_len, &input_consumed_len,
                        output_bucket, output_bucket_len, &output_result_len, signature,
                        confirmation_token));
        } else {
            EXPECT_KMOK_GOTOEND(update_subsample(
                        sess, ssp_operation,
                        auth_token,
                        timestamp_token,
                        input_bucket.get(), input_bucket_len, &input_consumed_len,
                        output_bucket, output_bucket_len, &output_result_len));
        }

        if (input_consumed_len_r) {
            *input_consumed_len_r += input_consumed_len;
        }

        BLOGD("input_consumed_len(%d), \
                input_consumed_len_r(%d), \
                output_result_len_r(%zu), \
                output_result_len(%u)\n",
                input_consumed_len,
                *input_consumed_len_r,
                output_result_len_r ? *output_result_len_r : 0,
                output_result_len);

        if (input_data) {
            input_data += input_consumed_len;
            if (input_data >= input_data_eob)
                input_data = input_data_eob = NULL;
        }

        if (output_result_len_r) {
            *output_result_len_r += output_result_len;
        }

        if (output) {
            output += output_result_len;
        }
    }

end:

    return ret;
}


static keymaster_error_t subsampling_update_operation(
        ssp_session_t &sess,
        ssp_operation_t *ssp_operation,
        const keymaster_blob_t *input, uint32_t *input_consumed_len,
        uint8_t *output, const uint8_t *output_eob, size_t *output_result_len,
        const keymaster_hw_auth_token_t *auth_token,
        const keymaster_timestamp_token_t *timestamp_token)
{
    keymaster_error_t ret = KM_ERROR_OK;

    EXPECT_KMOK_GOTOEND(subsample_input_data(
                sess, ssp_operation,
                input, input_consumed_len,
                output, output_eob, output_result_len,
                auth_token,
                timestamp_token,
                NULL,
                NULL,
                false));

end:
    return ret;
}


static keymaster_error_t subsampling_finish_operation(
        ssp_session_t &sess,
        ssp_operation_t *ssp_operation,
        const keymaster_hw_auth_token_t *auth_token,
        const keymaster_timestamp_token_t *timestamp_token,
        const keymaster_blob_t *signature,
        const keymaster_blob_t* confirmation_token,
        const keymaster_blob_t *input, uint32_t *input_consumed_len,
        uint8_t *output, const uint8_t *output_eob, size_t *output_result_len)
{
    keymaster_error_t ret = KM_ERROR_OK;

    EXPECT_KMOK_GOTOEND(subsample_input_data(
                sess, ssp_operation,
                input, input_consumed_len,
                output, output_eob, output_result_len,
                auth_token, timestamp_token, signature,
                confirmation_token,
                true));
end:

    return ret;
}

keymaster_error_t ssp_update_aad(
        ssp_session_t &sess,
        keymaster_operation_handle_t op_handle,
        const keymaster_key_param_set_t* in_params,
        const keymaster_hw_auth_token_t* auth_token,
        const keymaster_timestamp_token_t *timestamp_token)
{
    BLOGD("%s()\n", __func__);

    keymaster_error_t ret = KM_ERROR_OK;
    ssp_operation_t *ssp_operation = NULL;
    ssp_km_message_t ssp_msg = {};

    unique_ptr<uint8_t[]> in_params_blob;
    uint32_t in_params_blob_size = 0;

    /* for debug */
    ssp_print_params("input key params", in_params);
    BLOGD("op_handle(%lu: 0x%08lx)\n", op_handle, op_handle);
    dump_hwauth_token(auth_token);
    dump_timestamp_token(timestamp_token);

    EXPECT_KMOK_GOTOEND(find_ssp_operation_handle(&sess, op_handle, &ssp_operation));

    EXPECT_KMOK_GOTOEND(validate_key_params(in_params));
    EXPECT_KMOK_GOTOEND(ssp_serializer_serialize_params(in_params_blob,
                &in_params_blob_size,
                in_params, false, 0, 0));

    /* TODO: check for update order. must not call update before update_aad */

    /* only support aes-gcm */
    if (ssp_operation->algorithm != KM_ALGORITHM_AES) {
        ret = KM_ERROR_INCOMPATIBLE_ALGORITHM;
        goto end;
    }

    /* set SSP IPC params */
    memset(&ssp_msg, 0x00, sizeof(ssp_msg));
    ssp_msg.command.id = CMD_SSP_UPDATE;
    ssp_msg.update.op_handle = ssp_operation->handle;
    ssp_msg.update.in_params_len = in_params_blob_size;

    if (auth_token) {
        ssp_msg.update.auth_token.challenge = auth_token->challenge;
        ssp_msg.update.auth_token.user_id = auth_token->user_id;
        ssp_msg.update.auth_token.authenticator_id = auth_token->authenticator_id;
        ssp_msg.update.auth_token.authenticator_type = auth_token->authenticator_type;
        ssp_msg.update.auth_token.timestamp = auth_token->timestamp;
        memset(ssp_msg.update.auth_token.mac, 0, 32);
        if (auth_token->mac.data_length) {
            if (auth_token->mac.data_length != 32) {
                BLOGE("hw_auth_token len(%zu) != 32\n", auth_token->mac.data_length);
                ret = KM_ERROR_INVALID_ARGUMENT;
                goto end;
            }
            memcpy(ssp_msg.update.auth_token.mac, auth_token->mac.data, 32);
        }
    } else {
        ssp_msg.update.auth_token.challenge = 0;
        ssp_msg.update.auth_token.user_id = 0;
        ssp_msg.update.auth_token.authenticator_id = 0;
        ssp_msg.update.auth_token.authenticator_type = KM_HW_AUTH_TYPE_NONE;
        ssp_msg.update.auth_token.timestamp = 0;
        memset(ssp_msg.update.auth_token.mac, 0, 32);
    }

    if (timestamp_token) {
        ssp_msg.update.timestamp_token.challenge = timestamp_token->challenge;
        ssp_msg.update.timestamp_token.timestamp = timestamp_token->timestamp;
        /* set mac */
        memset(ssp_msg.update.timestamp_token.mac, 0, 32);
        if (timestamp_token->mac.data_length) {
            if (timestamp_token->mac.data_length != 32) {
                BLOGE("hw_timestamp_token len(%zu) != 32\n",
                        timestamp_token->mac.data_length);
                ret = KM_ERROR_INVALID_ARGUMENT;
                goto end;
            }
            memcpy(ssp_msg.update.timestamp_token.mac, timestamp_token->mac.data, 32);
        }
    } else {
        ssp_msg.update.timestamp_token.challenge = 0;
        ssp_msg.update.timestamp_token.timestamp = 0;
        memset(ssp_msg.update.timestamp_token.mac, 0, 32);
    }

    /* check validity of ssp msg len */
    EXPECT_KMOK_GOTOEND(validate_ipc_req_message(&ssp_msg));

    /* set ssp msg buffer */
    memset(ssp_msg.update.in_params, 0x00, sizeof(ssp_msg.update.in_params));
    if (in_params_blob.get() != NULL && ssp_msg.update.in_params_len > 0) {
        memcpy(ssp_msg.update.in_params, in_params_blob.get(), ssp_msg.update.in_params_len);
    }

    /* invoke ssp msg to ssp */
    EXPECT_KMOK_GOTOEND(ssp_invoke(sess, &ssp_msg));

    /* process reply message */
    CHECK_SSPKM_MSG_GOTOEND(ssp_msg);

    EXPECT_KMOK_GOTOEND(validate_ipc_resp_message(&ssp_msg));

end:
    if (ret != KM_ERROR_OK) {
        /* disable hw in error case and if session exist */
        if (ret != KM_ERROR_INVALID_OPERATION_HANDLE) {
            ssp_hwctl_disable();
        }
        free_ssp_operation_bucket(&sess, ssp_operation);
    }

    if (ret != KM_ERROR_OK) {
        BLOGE("%s() exit with %d\n", __func__, ret);
    } else {
        BLOGD("%s() exit with %d\n", __func__, ret);
    }

    return ret;
}

keymaster_error_t ssp_update(
        ssp_session_t &sess,
        keymaster_operation_handle_t op_handle,
        const keymaster_blob_t* input_data,
        const keymaster_hw_auth_token_t* auth_token,
        const keymaster_timestamp_token_t *timestamp_token,
        uint32_t* input_consumed_len,
        keymaster_key_param_set_t* out_params,
        keymaster_blob_t* output_data)
{
    BLOGD("%s()\n", __func__);

    keymaster_error_t ret = KM_ERROR_OK;
    ssp_operation_t *ssp_operation = NULL;
    size_t output_result_len = 0;

    /* for_debug */
    BLOGD("op_handle(%lu: 0x%08lx)\n", op_handle, op_handle);
    dump_hwauth_token(auth_token);
    dump_timestamp_token(timestamp_token);

    EXPECT_NE_NULL_GOTOEND(input_consumed_len);

    EXPECT_KMOK_GOTOEND(find_ssp_operation_handle(&sess, op_handle, &ssp_operation));

    /* set default output parameters */
    if (out_params != NULL) {
        out_params->params = NULL;
        out_params->length = 0;
    }
    *input_consumed_len = 0;

    /* alloc output buffer */
    if (output_data != NULL) {
        if (ssp_operation->algorithm == KM_ALGORITHM_AES) {
            output_data->data_length = input_data->data_length + 16; /* spare on more block */
            output_data->data = (uint8_t *)calloc(output_data->data_length, sizeof(uint8_t));
            if (output_data->data == NULL) {
                output_data->data_length = 0;
                ret = KM_ERROR_MEMORY_ALLOCATION_FAILED;
                goto end;
            }
        } else if (ssp_operation->algorithm == KM_ALGORITHM_TRIPLE_DES) {
            output_data->data_length = input_data->data_length + 8; /* spare one more block */
            output_data->data = (uint8_t *)calloc(output_data->data_length, sizeof(uint8_t));
            if (output_data->data == NULL) {
                output_data->data_length = 0;
                ret = KM_ERROR_MEMORY_ALLOCATION_FAILED;
                goto end;
            }
        } else {
            /* empty output */
            output_data->data = NULL;
            output_data->data_length = 0;


        }
    }

    EXPECT_KMOK_GOTOEND(subsampling_update_operation(sess, ssp_operation,
                input_data, input_consumed_len,
                output_data ? const_cast<uint8_t *>(output_data->data) : NULL,
                output_data ? output_data->data + output_data->data_length : NULL,
                &output_result_len, auth_token, timestamp_token));

    if (output_data) {
        BLOGD("output_result_len(%zu)\n", output_result_len);
        output_data->data_length = output_result_len;
    }

end:
    ssp_print_params("out params", out_params);
    if (output_data != NULL) {
        ssp_print_buf("output_data", output_data->data, output_data->data_length);
    }

    if (ret != KM_ERROR_OK) {
        /* disable hw in error case and if session exist */
        if (ret != KM_ERROR_INVALID_OPERATION_HANDLE) {
            ssp_hwctl_disable();
        }

        if (output_data != NULL) {
            if (output_data->data) {
                free((void*)output_data->data);
            }
            output_data->data = NULL;
            output_data->data_length = 0;
        }
        free_ssp_operation_bucket(&sess, ssp_operation);
    }

    if (ret != KM_ERROR_OK) {
        BLOGE("%s() exit with %d\n", __func__, ret);
    } else {
        BLOGD("%s() exit with %d\n", __func__, ret);
    }

    return ret;
}


keymaster_error_t ssp_finish(
        ssp_session_t &sess,
        keymaster_operation_handle_t op_handle,
        const keymaster_blob_t* input_data,
        const keymaster_blob_t* signature,
        const keymaster_hw_auth_token_t *auth_token,
        const keymaster_timestamp_token_t *timestamp_token,
        const keymaster_blob_t* confirmation_token,
        keymaster_key_param_set_t* out_params,
        keymaster_blob_t* output_data)
{
    BLOGD("%s()\n", __func__);

    keymaster_error_t ret = KM_ERROR_OK;

    ssp_operation_t *ssp_operation = NULL;
    uint32_t input_consumed_len = 0;
    size_t output_result_len = 0;


    /* for_debug */
    BLOGD("op_handle(%lu: 0x%08lx)\n", op_handle, op_handle);
    dump_hwauth_token(auth_token);
    dump_timestamp_token(timestamp_token);

    EXPECT_KMOK_GOTOEND(find_ssp_operation_handle(&sess, op_handle, &ssp_operation));

    /* set default output_data & parameters */
    if (out_params != NULL) {
        out_params->params = NULL;
        out_params->length = 0;
    }

    if (output_data != NULL) {
        output_data->data = NULL;
        output_data->data_length = 0;
    }

    if (output_data != NULL) {
        output_data->data_length = ssp_operation->final_len;
        if (ssp_operation->algorithm == KM_ALGORITHM_AES ||
                ssp_operation->algorithm == KM_ALGORITHM_TRIPLE_DES)
        {
            /* need extra block length */
            output_data->data_length += (input_data ? input_data->data_length : 0);
        }

        /* TODO: should set output length */
        if (ssp_operation->algorithm == KM_ALGORITHM_EC) {
            if (output_data->data_length == 0) output_data->data_length = 32; // for ec
        }

        BLOGD("output data length : %zu", output_data->data_length);
        /* alloc the output buffer and the caller will free this buffer */
        if (output_data->data_length != 0) {
            output_data->data = (uint8_t *)calloc(output_data->data_length, sizeof(uint8_t));
            if (output_data->data == NULL) {
                ret = KM_ERROR_MEMORY_ALLOCATION_FAILED;
                goto end;
            }
        }
    }

    EXPECT_KMOK_GOTOEND(subsampling_finish_operation(
                sess, ssp_operation,
                auth_token,
                timestamp_token,
                signature,
                confirmation_token,
                input_data, &input_consumed_len,
                output_data ? const_cast</*unconst*/ uint8_t *>(output_data->data) : NULL,
                output_data ? output_data->data + output_data->data_length : NULL,
                &output_result_len));

    /* after finish operation, all input data should be consomed */
    EXPECT_TRUE_GOTOEND(KM_ERROR_UNKNOWN_ERROR,
            !input_data || input_data->data_length == input_consumed_len);

    if (output_data)
        output_data->data_length = output_result_len;

end:
    /* for debug */
    if (output_data)
        ssp_print_buf("output_data", output_data->data, output_data->data_length);
    if (signature)
        ssp_print_buf("signature", signature->data, signature->data_length);

    if (ret != KM_ERROR_INVALID_OPERATION_HANDLE) {
        /* if there are no session, we don't need to disable hw */
        ssp_hwctl_disable();
    }

    ssp_print_params("out params", out_params);

    free_ssp_operation_bucket(&sess, ssp_operation);

    if (ret != KM_ERROR_OK) {
        if (output_data) {
            if (output_data->data) {
                free((void*)output_data->data);
                output_data->data = NULL;
                output_data->data_length = 0;
            }
        }
        BLOGE("%s() exit with %d\n", __func__, ret);
    } else {
        BLOGD("%s() exit with %d\n", __func__, ret);
    }

    return ret;
}

keymaster_error_t ssp_abort(
        ssp_session_t &sess,
        keymaster_operation_handle_t op_handle)
{
    BLOGD("%s()\n", __func__);

    keymaster_error_t ret = KM_ERROR_OK;
    ssp_operation_t *ssp_operation = NULL;
    ssp_km_message_t ssp_msg = {};

    EXPECT_KMOK_GOTOEND(find_ssp_operation_handle(&sess, op_handle, &ssp_operation));

    /* set SSP IPC params */
    memset(&ssp_msg, 0x00, sizeof(ssp_msg));
    ssp_msg.command.id = CMD_SSP_ABORT;
    ssp_msg.abort.op_handle = op_handle;

    /* check validity of ssp msg len */
    EXPECT_KMOK_GOTOEND(validate_ipc_req_message(&ssp_msg));

    /* invoke ssp msg to ssp */
    EXPECT_KMOK_GOTOEND(ssp_invoke(sess, &ssp_msg));

    /* process reply message */
    CHECK_SSPKM_MSG_GOTOEND(ssp_msg);

    EXPECT_KMOK_GOTOEND(validate_ipc_resp_message(&ssp_msg));

end:
    if (ret != KM_ERROR_INVALID_OPERATION_HANDLE) {
        /* if there are no session, we don't need to disable hw */
        ssp_hwctl_disable();
    }

    free_ssp_operation_bucket(&sess, ssp_operation);

    if (ret != KM_ERROR_OK) {
        BLOGE("%s() exit with %d\n", __func__, ret);
    } else {
        BLOGD("%s() exit with %d\n", __func__, ret);
    }

    return ret;
}

keymaster_error_t ssp_self_attest_key(
        ssp_session_t &sess,
        const keymaster_key_blob_t* attest_keyblob,
        const keymaster_key_param_set_t* attest_params,
        keymaster_cert_chain_t* cert_chain)
{
    BLOGD("%s()\n", __func__);

    keymaster_error_t ret = KM_ERROR_OK;
    ssp_km_message_t ssp_msg = {};

    unique_ptr<uint8_t[]> params_blob;
    uint32_t params_blob_size = 0;

    EXPECT_NE_NULL_GOTOEND(attest_keyblob);
    EXPECT_NE_NULL_GOTOEND(cert_chain);

    EXPECT_KMOK_GOTOEND(validate_key_params(attest_params));

    EXPECT_KMOK_GOTOEND(ssp_serializer_serialize_params(params_blob,
                &params_blob_size,
                attest_params, false, 0, 0));

    BLOGD("attest key blob len : %lu", attest_keyblob->key_material_size);

    /* set SSP IPC params */
    memset(&ssp_msg, 0x00, sizeof(ssp_msg));
    ssp_msg.command.id = CMD_SSP_SELF_ATTEST_KEY;
    ssp_msg.attest_key.keyblob_len = attest_keyblob->key_material_size;
    ssp_msg.attest_key.attestation_blob_len = attest_keyblob->key_material_size;
    ssp_msg.attest_key.keyparams_len = params_blob_size;
    ssp_msg.attest_key.cert_chain_len = sizeof(ssp_msg.attest_key.cert_chain);

    /* check validity of ssp msg len */
    EXPECT_KMOK_GOTOEND(validate_ipc_req_message(&ssp_msg));

    /* set ssp msg buffer */
    memset(ssp_msg.attest_key.keyblob, 0x00, sizeof(ssp_msg.attest_key.keyblob));
    memcpy(ssp_msg.attest_key.keyblob,
            attest_keyblob->key_material,
            ssp_msg.attest_key.keyblob_len);
    memset(ssp_msg.attest_key.attestation_blob,
            0x00, sizeof(ssp_msg.attest_key.attestation_blob));
    memcpy(ssp_msg.attest_key.attestation_blob,
            attest_keyblob->key_material,
            ssp_msg.attest_key.keyblob_len);
    memset(ssp_msg.attest_key.keyparams, 0x00, sizeof(ssp_msg.attest_key.keyparams));
    if (ssp_msg.attest_key.keyparams_len > 0) {
        memcpy(ssp_msg.attest_key.keyparams,
                params_blob.get(),
                ssp_msg.attest_key.keyparams_len);
    }

    memset(ssp_msg.attest_key.cert_chain, 0x00, sizeof(ssp_msg.attest_key.cert_chain));

    /* invoke ssp msg to ssp */
    EXPECT_KMOK_GOTOEND(ssp_invoke(sess, &ssp_msg));

    /* process reply message */
    CHECK_SSPKM_MSG_GOTOEND(ssp_msg);

    EXPECT_KMOK_GOTOEND(validate_ipc_resp_message(&ssp_msg));

    /* deserialize certs to cert_chain */
    EXPECT_KMOK_GOTOEND(ssp_serializer_deserialize_attestation(
                ssp_msg.attest_key.cert_chain,
                ssp_msg.attest_key.cert_chain_len,
                cert_chain));

end:
    ssp_print_buf("IPC attestation_blob",
            ssp_msg.attest_key.attestation_blob,
            ssp_msg.attest_key.attestation_blob_len);

    if (ret != KM_ERROR_OK) {
        keymaster_free_cert_chain(cert_chain);
        BLOGE("%s() exit with %d\n", __func__, ret);
    } else {
        BLOGD("%s() exit with %d\n", __func__, ret);
    }

    return ret;
}

keymaster_error_t ssp_factory_attest_key(
        ssp_session_t &sess,
        const keymaster_key_blob_t* attest_keyblob,
        const keymaster_key_param_set_t* attest_params,
        keymaster_cert_chain_t* cert_chain)
{
    BLOGD("%s()\n", __func__);

    keymaster_error_t ret = KM_ERROR_OK;
    ssp_km_message_t ssp_msg = {};

    unique_ptr<uint8_t[]> attest_blob;
    long attest_blob_len = 0;

    unique_ptr<uint8_t[]> params_blob;
    uint32_t params_blob_size = 0;

    EXPECT_NE_NULL_GOTOEND(attest_keyblob);
    EXPECT_NE_NULL_GOTOEND(cert_chain);

    EXPECT_KMOK_GOTOEND(validate_key_params(attest_params));

    EXPECT_KMOK_GOTOEND(ssp_serializer_serialize_params(params_blob,
                &params_blob_size,
                attest_params, false, 0, 0));

    BLOGD(" attest key blob size : %lu", attest_keyblob->key_material_size);
    if (get_file_contents(STRONGBOX_ATTESTATION_FILE,
                attest_blob, &attest_blob_len) < 0) {
        /* If attestation data is not exist, skip load attestation data */
        BLOGE("strongbox attestation data(factory key) is not loaded\n");
        ret = KM_ERROR_ATTESTATION_KEYS_NOT_PROVISIONED;
        goto end;
    }

    ssp_print_buf("attest blob", attest_blob.get(), attest_blob_len);
    BLOGD("attest blob : %lu", attest_blob_len);

    /* set SSP IPC params */
    memset(&ssp_msg, 0x00, sizeof(ssp_msg));
    ssp_msg.command.id = CMD_SSP_FACTORY_ATTEST_KEY;
    ssp_msg.attest_key.keyblob_len = attest_keyblob->key_material_size;
    ssp_msg.attest_key.attestation_blob_len = attest_blob_len;
    ssp_msg.attest_key.keyparams_len = params_blob_size;
    ssp_msg.attest_key.cert_chain_len = sizeof(ssp_msg.attest_key.cert_chain);

    /* check validity of ssp msg len */
    EXPECT_KMOK_GOTOEND(validate_ipc_req_message(&ssp_msg));

    /* set ssp msg buffer */
    memset(ssp_msg.attest_key.keyblob, 0x00, sizeof(ssp_msg.attest_key.keyblob));
    memcpy(ssp_msg.attest_key.keyblob,
            attest_keyblob->key_material,
            ssp_msg.attest_key.keyblob_len);
    memset(ssp_msg.attest_key.attestation_blob,
            0x00, sizeof(ssp_msg.attest_key.attestation_blob));
    memcpy(ssp_msg.attest_key.attestation_blob,
            attest_blob.get(),
            ssp_msg.attest_key.attestation_blob_len);
    memset(ssp_msg.attest_key.keyparams, 0x00, sizeof(ssp_msg.attest_key.keyparams));
    if (ssp_msg.attest_key.keyparams_len > 0) {
        memcpy(ssp_msg.attest_key.keyparams,
                params_blob.get(),
                ssp_msg.attest_key.keyparams_len);
    }

    memset(ssp_msg.attest_key.cert_chain, 0x00, sizeof(ssp_msg.attest_key.cert_chain));

    /* invoke ssp msg to ssp */
    EXPECT_KMOK_GOTOEND(ssp_invoke(sess, &ssp_msg));

    /* process reply message */
    CHECK_SSPKM_MSG_GOTOEND(ssp_msg);

    EXPECT_KMOK_GOTOEND(validate_ipc_resp_message(&ssp_msg));

    /* deserialize certs to cert_chain */
    EXPECT_KMOK_GOTOEND(ssp_serializer_deserialize_attestation(
                ssp_msg.attest_key.cert_chain,
                ssp_msg.attest_key.cert_chain_len,
                cert_chain));

end:
    ssp_print_buf("IPC attestation_blob",
            ssp_msg.attest_key.attestation_blob,
            ssp_msg.attest_key.attestation_blob_len);

    if (ret != KM_ERROR_OK) {
        keymaster_free_cert_chain(cert_chain);
        BLOGE("%s() exit with %d\n", __func__, ret);
    } else {
        BLOGD("%s() exit with %d\n", __func__, ret);
    }

    return ret;
}

keymaster_error_t ssp_attest_key(
        ssp_session_t &sess,
        const keymaster_blob_t* issuer_subject,
        const keymaster_key_blob_t* signing_keyblob,
        const keymaster_key_blob_t* attest_keyblob,
        const keymaster_key_param_set_t* attest_params,
        keymaster_cert_chain_t* cert_chain)
{
    BLOGD("%s()\n", __func__);

    keymaster_error_t ret = KM_ERROR_OK;
    ssp_km_message_t ssp_msg = {};

    unique_ptr<uint8_t[]> params_blob;
    uint32_t params_blob_size = 0;

    EXPECT_NE_NULL_GOTOEND(attest_keyblob);
    EXPECT_NE_NULL_GOTOEND(cert_chain);

    EXPECT_KMOK_GOTOEND(validate_key_params(attest_params));

    EXPECT_KMOK_GOTOEND(ssp_serializer_serialize_params(params_blob,
                &params_blob_size,
                attest_params, false, 0, 0));

    /* set SSP IPC params */
    memset(&ssp_msg, 0x00, sizeof(ssp_msg));
    ssp_msg.command.id = CMD_SSP_ATTEST_KEY;
    ssp_msg.attest_key.keyblob_len = signing_keyblob->key_material_size;
    ssp_msg.attest_key.attestation_blob_len = attest_keyblob->key_material_size;
    ssp_msg.attest_key.keyparams_len = params_blob_size;
    ssp_msg.attest_key.cert_chain_len = sizeof(ssp_msg.attest_key.cert_chain);
    ssp_msg.attest_key.issuer_blob_len = issuer_subject->data_length;

    /* check validity of ssp msg len */
    EXPECT_KMOK_GOTOEND(validate_ipc_req_message(&ssp_msg));

    /* set ssp msg buffer */
    memset(ssp_msg.attest_key.keyblob, 0x00, sizeof(ssp_msg.attest_key.keyblob));
    memcpy(ssp_msg.attest_key.keyblob,
            signing_keyblob->key_material,
            ssp_msg.attest_key.keyblob_len);

    memset(ssp_msg.attest_key.issuer_blob, 0x00, sizeof(ssp_msg.attest_key.issuer_blob));
    if (ssp_msg.attest_key.issuer_blob_len > 0) {
        memcpy(ssp_msg.attest_key.issuer_blob,
                issuer_subject->data,
                ssp_msg.attest_key.issuer_blob_len);
    }

    memset(ssp_msg.attest_key.attestation_blob,
            0x00, sizeof(ssp_msg.attest_key.attestation_blob));
    memcpy(ssp_msg.attest_key.attestation_blob,
            attest_keyblob->key_material,
            ssp_msg.attest_key.attestation_blob_len);

    memset(ssp_msg.attest_key.keyparams, 0x00, sizeof(ssp_msg.attest_key.keyparams));
    if (ssp_msg.attest_key.keyparams_len > 0) {
        memcpy(ssp_msg.attest_key.keyparams,
                params_blob.get(),
                ssp_msg.attest_key.keyparams_len);
    }

    memset(ssp_msg.attest_key.cert_chain, 0x00, sizeof(ssp_msg.attest_key.cert_chain));

    /* invoke ssp msg to ssp */
    EXPECT_KMOK_GOTOEND(ssp_invoke(sess, &ssp_msg));

    /* process reply message */
    CHECK_SSPKM_MSG_GOTOEND(ssp_msg);

    EXPECT_KMOK_GOTOEND(validate_ipc_resp_message(&ssp_msg));

    /* deserialize certs to cert_chain */
    EXPECT_KMOK_GOTOEND(ssp_serializer_deserialize_attestation(
                ssp_msg.attest_key.cert_chain,
                ssp_msg.attest_key.cert_chain_len,
                cert_chain));

end:
    ssp_print_buf("IPC attestation_blob",
            ssp_msg.attest_key.attestation_blob,
            ssp_msg.attest_key.attestation_blob_len);

    if (ret != KM_ERROR_OK) {
        keymaster_free_cert_chain(cert_chain);
        BLOGE("%s() exit with %d\n", __func__, ret);
    } else {
        BLOGD("%s() exit with %d\n", __func__, ret);
    }

    return ret;
}


keymaster_error_t ssp_upgrade_key(
        ssp_session_t &sess,
        const keymaster_key_blob_t* keyblob_to_upgrade,
        const keymaster_key_param_set_t* upgrade_params,
        keymaster_key_blob_t* upgraded_keyblob)
{

    BLOGD("%s()\n", __func__);

    keymaster_error_t ret = KM_ERROR_OK;
    ssp_km_message_t ssp_msg = {};

    unique_ptr<uint8_t[]> params_blob;
    uint32_t params_blob_size = 0;

    EXPECT_NE_NULL_GOTOEND(keyblob_to_upgrade);
    EXPECT_NE_NULL_GOTOEND(upgrade_params);
    EXPECT_NE_NULL_GOTOEND(upgraded_keyblob);

    upgraded_keyblob->key_material = NULL;

    ssp_print_buf("keyblob to upgrade",
            keyblob_to_upgrade->key_material,
            keyblob_to_upgrade->key_material_size);

    EXPECT_KMOK_GOTOEND(ssp_serializer_serialize_params(params_blob,
                &params_blob_size,
                upgrade_params, false, 0, 0));


    /* set SSP IPC params */
    memset(&ssp_msg, 0x00, sizeof(ssp_msg));
    ssp_msg.command.id = CMD_SSP_UPGRADE_KEY;
    ssp_msg.upgrade_key.keyblob_len = keyblob_to_upgrade->key_material ?
        keyblob_to_upgrade->key_material_size : 0;
    ssp_msg.upgrade_key.keyparams_len = params_blob_size;
    ssp_msg.upgrade_key.upgraded_keyblob_len = sizeof(ssp_msg.upgrade_key.upgraded_keyblob);

    /* check validity of ssp msg len */
    EXPECT_KMOK_GOTOEND(validate_ipc_req_message(&ssp_msg));

    /* set ssp msg buffer */
    memset(ssp_msg.upgrade_key.keyblob,
            0x00, sizeof(ssp_msg.upgrade_key.keyblob));
    memcpy(ssp_msg.upgrade_key.keyblob,
            keyblob_to_upgrade->key_material,
            ssp_msg.upgrade_key.keyblob_len);

    memset(ssp_msg.upgrade_key.keyparams,
            0x00, sizeof(ssp_msg.upgrade_key.keyparams));
    memcpy(ssp_msg.upgrade_key.keyparams,
            params_blob.get(),
            ssp_msg.upgrade_key.keyparams_len);

    memset(ssp_msg.upgrade_key.upgraded_keyblob,
            0x00, sizeof(ssp_msg.upgrade_key.upgraded_keyblob));

    /* invoke ssp msg to ssp */
    EXPECT_KMOK_GOTOEND(ssp_invoke(sess, &ssp_msg));

    /* process reply message */
    CHECK_SSPKM_MSG_GOTOEND(ssp_msg);

    EXPECT_KMOK_GOTOEND(validate_ipc_resp_message(&ssp_msg));

    if (ssp_msg.upgrade_key.upgraded_keyblob_len) {
        upgraded_keyblob->key_material =
            (uint8_t *)calloc(ssp_msg.upgrade_key.upgraded_keyblob_len, sizeof(uint8_t));
        if (!upgraded_keyblob->key_material) {
            BLOGE("malloc fail for upgraded keyblob. size(%u)\n",
                    ssp_msg.upgrade_key.upgraded_keyblob_len);
            ret = KM_ERROR_MEMORY_ALLOCATION_FAILED;
            goto end;
        }
        memcpy((uint8_t *)upgraded_keyblob->key_material,
                ssp_msg.upgrade_key.upgraded_keyblob,
                ssp_msg.upgrade_key.upgraded_keyblob_len);

        upgraded_keyblob->key_material_size = ssp_msg.upgrade_key.upgraded_keyblob_len;

        ssp_print_buf("upgraded_keyblob",
                upgraded_keyblob->key_material,
                upgraded_keyblob->key_material_size);
    } else {
        BLOGD("don't need to updage key");
        upgraded_keyblob->key_material_size = 0;
        upgraded_keyblob->key_material = NULL;
    }

end:

    return ret;
}

keymaster_error_t ssp_device_locked(
        ssp_session_t &sess,
        const bool password_only,
        const keymaster_timestamp_token_t *timestamp_token)
{
    BLOGD("%s()\n", __func__);

    keymaster_error_t ret = KM_ERROR_OK;
    ssp_km_message_t ssp_msg = {};

    /* for debugging */
    dump_timestamp_token(timestamp_token);

    /* set SSP IPC params */
    memset(&ssp_msg, 0x00, sizeof(ssp_msg));
    ssp_msg.command.id = CMD_SSP_DEVICE_LOCKED;
    ssp_msg.device_locked.password_only = password_only;

    /* add msg for verificationToken */
    if (timestamp_token) {
        ssp_msg.device_locked.timestamp_token.challenge = timestamp_token->challenge;
        ssp_msg.device_locked.timestamp_token.timestamp = timestamp_token->timestamp;
        /* set mac */
        memset(ssp_msg.device_locked.timestamp_token.mac, 0, 32);
        if (timestamp_token->mac.data_length) {
            if (timestamp_token->mac.data_length != 32) {
                BLOGE("hw_timestamp_token len(%zu) != 32\n",
                        timestamp_token->mac.data_length);
                ret = KM_ERROR_INVALID_ARGUMENT;
                goto end;
            }
            memcpy(ssp_msg.device_locked.timestamp_token.mac,
                    timestamp_token->mac.data,
                    32);
        }
    } else {
        ssp_msg.device_locked.timestamp_token.challenge = 0;
        ssp_msg.device_locked.timestamp_token.timestamp = 0;
        memset(ssp_msg.device_locked.timestamp_token.mac, 0x00, 32);
    }

    /* check validity of ssp msg len */
    EXPECT_KMOK_GOTOEND(validate_ipc_req_message(&ssp_msg));

    /* invoke ssp msg to ssp */
    EXPECT_KMOK_GOTOEND(ssp_invoke(sess, &ssp_msg));

    /* process reply message */
    CHECK_SSPKM_MSG_GOTOEND(ssp_msg);

    EXPECT_KMOK_GOTOEND(validate_ipc_resp_message(&ssp_msg));

end:

    if (ret != KM_ERROR_OK) {
        BLOGE("%s() exit with %d\n", __func__, ret);
    } else {
        BLOGD("%s() exit with %d\n", __func__, ret);
    }

    return ret;
}

keymaster_error_t ssp_early_boot_ended(
        ssp_session_t &sess)
{
    BLOGD("%s()\n", __func__);

    keymaster_error_t ret = KM_ERROR_OK;
    ssp_km_message_t ssp_msg = {};

    /* set SSP IPC params */
    memset(&ssp_msg, 0x00, sizeof(ssp_msg));
    ssp_msg.command.id = CMD_SSP_EARLY_BOOT_ENDED;

    /* check validity of ssp msg len */
    EXPECT_KMOK_GOTOEND(validate_ipc_req_message(&ssp_msg));

    /* invoke ssp msg to ssp */
    EXPECT_KMOK_GOTOEND(ssp_invoke(sess, &ssp_msg));

    /* process reply message */
    CHECK_SSPKM_MSG_GOTOEND(ssp_msg);

    EXPECT_KMOK_GOTOEND(validate_ipc_resp_message(&ssp_msg));

end:

    if (ret != KM_ERROR_OK) {
        BLOGE("%s() exit with %d\n", __func__, ret);
    } else {
        BLOGD("%s() exit with %d\n", __func__, ret);
    }

    return ret;
}


keymaster_error_t ssp_send_dummy_system_version(
        ssp_session_t &sess,
        const system_version_info_t *sys_version_info)
{
    keymaster_error_t ret = KM_ERROR_OK;
    ssp_km_message_t ssp_msg = {};

    BLOGD("OS_VERSION = %u, OS_PATCHLEVEL = %u, VENDOR_PATCHLEVEL = %u, BOOT_PATCHLEVEL = %u",
            sys_version_info->os_version,
            sys_version_info->os_patchlevel,
            sys_version_info->vendor_patchlevel,
            sys_version_info->boot_patchlevel);

    /* set SSP IPC params */
    memset(&ssp_msg, 0x00, sizeof(ssp_msg));
    ssp_msg.command.id = CMD_SSP_SET_DUMMY_SYSTEM_VERSION;
    ssp_msg.set_dummy_system_version.os_version = sys_version_info->os_version;
    ssp_msg.set_dummy_system_version.os_patchlevel = sys_version_info->os_patchlevel;
    ssp_msg.set_dummy_system_version.boot_patchlevel = sys_version_info->boot_patchlevel;
    ssp_msg.set_dummy_system_version.vendor_patchlevel = sys_version_info->vendor_patchlevel;

    EXPECT_KMOK_GOTOEND(validate_ipc_req_message(&ssp_msg));

    /* invoke ssp msg to ssp */
    EXPECT_KMOK_GOTOEND(ssp_invoke(sess, &ssp_msg));

    /* process reply message */
    CHECK_SSPKM_MSG_GOTOEND(ssp_msg);

    EXPECT_KMOK_GOTOEND(validate_ipc_resp_message(&ssp_msg));

end:
    if (ret != KM_ERROR_OK) {
        BLOGE("%s() exit with %d\n", __func__, ret);
    } else {
        BLOGD("%s() exit with %d\n", __func__, ret);
    }

    return ret;
}
