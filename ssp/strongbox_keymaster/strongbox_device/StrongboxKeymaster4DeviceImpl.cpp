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


#include <StrongboxKeymaster4DeviceImpl.h>
#include <log/log.h>
#include <memory>

#include "ssp_strongbox_keymaster_defs.h"
#include "ssp_strongbox_keymaster_impl.h"
#include "ssp_strongbox_keymaster_configuration.h"
#include "ssp_strongbox_keymaster_hwctl.h"

#include "cutils/properties.h"
#include <unistd.h>

#undef  LOG_TAG
#define LOG_TAG "StrongboxKeymaster4DeviceImpl"

/* ExySp: add timeout to check setting property */
#define SECURE_OS_TIMEOUT 50000

#if defined(CONF_SECUREOS_TEEGRIS)
/* ExySp: func to check if secureOS is loaded or not. */
static int secure_os_init(void)
{
    char state[PROPERTY_VALUE_MAX];
    int i;

    for (i = 0; i < SECURE_OS_TIMEOUT; i++) {
        property_get("vendor.tzts_daemon", state, 0);
        if (!strncmp(state, "Ready", strlen("Ready") + 1))
            break;
        else
            usleep(500);
    }

    if (i == SECURE_OS_TIMEOUT) {
		BLOGE("%s: secure os init timed out!", __func__);
		return KM_ERROR_OK;
    } else {
		BLOGI("%s: secure os init is done", __func__);
		return KM_ERROR_OK;
    }
}
#else
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
		BLOGE("%s: secure os init timed out!", __func__);
		return KM_ERROR_OK;
    } else {
		BLOGI("%s: secure os init is done", __func__);
		return KM_ERROR_OK;
    }
}
#endif


static uint32_t g_ssp_hw_status;


/**
 * Constructor
 */
StrongboxKeymaster4DeviceImpl::StrongboxKeymaster4DeviceImpl()
    : errcode(0)
{
    int ret;
    keymaster_key_param_t param[3];
    keymaster_key_param_set_t param_set;

	BLOGD("%s, %s, %d", __FILE__, __func__, __LINE__);

    ssp_hwctl_check_hw_status(&g_ssp_hw_status);

    /* check HW supporting mode */
    if (g_ssp_hw_status == SSP_HW_NOT_SUPPORTED) {
        BLOGI("SSP HW is not supported\n");
        errcode = KM_ERROR_OK;
    } else {
        /* Check if secureOS is loaded or not */
        if (secure_os_init()) {
            BLOGE("%s: Failed to init secureOS", __func__);
            errcode = KM_ERROR_SECURE_HW_COMMUNICATION_FAILED;
        } else {
            BLOGI("secure_os_init is done");
            ret = ssp_open(sess);
            if (ret < 0) {
                BLOGE("ssp_open failed");
                errcode = ret;
            } else {
                BLOGI("ssp_open is done");

                /* enable ssp hw until ssp initialization is done */
                ssp_hwctl_enable();

                ret = ssp_init_ipc(sess);
                if (ret != KM_ERROR_OK) {
                    BLOGE("ssp_init_ipc failed. ret(%d)", ret);
                    errcode = ret;
                } else {
                    BLOGI("ssp_init_ipc is done");

                    /* Set Os configuration info */
                    _os_version = ssp_strongbox_keymaster::GetOsVersion();
                    _os_patchlevel = ssp_strongbox_keymaster::GetOsPatchlevel();
                    _vendor_patchlevel = ssp_strongbox_keymaster::GetVendorPatchlevel();

                    BLOGI("os_version:%u, os_patchlevel:%u, vendor_patchlevel:%u",
                        _os_version, _os_patchlevel, _vendor_patchlevel);

                    /* send to ssp */
                    param[0].tag = KM_TAG_OS_VERSION;
                    param[0].integer = _os_version;
                    param[1].tag = KM_TAG_OS_PATCHLEVEL;
                    param[1].integer = _os_patchlevel;
                    param[2].tag = KM_TAG_VENDOR_PATCHLEVEL;
                    param[2].integer = _vendor_patchlevel;

                    param_set.params = param;
                    param_set.length = sizeof(param)/sizeof(keymaster_key_param_t);

                    ret = ssp_send_system_version(sess, &param_set);
                    if (ret != KM_ERROR_OK) {
                        BLOGE("ssp_send_system_version failed. ret(%d)", ret);
                        errcode = ret;
                    } else {
                        BLOGI("ssp_send_system_version is done");
                    }
                }

                /* disable ssp hw */
                ssp_hwctl_disable();
            }
        }
    }
}

/**
 * Destructor
 */
StrongboxKeymaster4DeviceImpl::~StrongboxKeymaster4DeviceImpl()
{
   BLOGI("%s, %s, %d", __FILE__, __func__, __LINE__);
   ssp_close(sess);
}

static const uint8_t dummy_sharing_check[32] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
};

static const uint8_t dummy_sharing_seed[32] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t dummy_sharing_nonce[32] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
};

keymaster_error_t StrongboxKeymaster4DeviceImpl::get_hmac_sharing_parameters(
    keymaster_hmac_sharing_params_t *params)
{
    if (g_ssp_hw_status == SSP_HW_NOT_SUPPORTED) {
        BLOGI("SSP HW is not supported\n");
        BLOGI("set default sharing param\n");
        memcpy(params->nonce, dummy_sharing_nonce, sizeof(params->nonce));
        params->seed.data_length = sizeof(dummy_sharing_seed);
        params->seed.data = (uint8_t *)calloc(params->seed.data_length, sizeof(uint8_t));
        memcpy((uint8_t *)params->seed.data, dummy_sharing_seed, sizeof(dummy_sharing_seed));

        return KM_ERROR_OK;
    } else {
        return ssp_get_hmac_sharing_parameters(sess, params);
    }
}

keymaster_error_t StrongboxKeymaster4DeviceImpl::compute_shared_hmac(
    const keymaster_hmac_sharing_params_set_t *params,
    keymaster_blob_t *sharing_check)
{
    if (g_ssp_hw_status == SSP_HW_NOT_SUPPORTED) {
        BLOGI("SSP HW is not supported\n");
        BLOGI("set default sharing check\n");
        sharing_check->data_length = sizeof(dummy_sharing_check);
        sharing_check->data = (uint8_t *)calloc(sharing_check->data_length, sizeof(uint8_t));
        memcpy((uint8_t *)sharing_check->data, dummy_sharing_check, sizeof(dummy_sharing_check));
        return KM_ERROR_OK;
    } else {
        return ssp_compute_shared_hmac(sess, params, sharing_check);
    }
}

keymaster_error_t StrongboxKeymaster4DeviceImpl::add_rng_entropy(
    const uint8_t* data,
    size_t length)
{
    if (g_ssp_hw_status == SSP_HW_NOT_SUPPORTED) {
        BLOGI("SSP HW is not supported\n");
        return KM_ERROR_HARDWARE_TYPE_UNAVAILABLE;
    } else {
        return ssp_add_rng_entropy(sess, data, length);
    }
}

keymaster_error_t StrongboxKeymaster4DeviceImpl::generate_key(
    const keymaster_key_param_set_t* params,
    keymaster_key_blob_t* key_blob,
    keymaster_key_characteristics_t* characteristics)
{
    if (g_ssp_hw_status == SSP_HW_NOT_SUPPORTED) {
        BLOGI("SSP HW is not supported\n");
        return KM_ERROR_HARDWARE_TYPE_UNAVAILABLE;
    } else {
        return ssp_generate_key(sess,
            params, key_blob, characteristics);
    }
}

keymaster_error_t StrongboxKeymaster4DeviceImpl::get_key_characteristics(
    const keymaster_key_blob_t* key_blob,
    const keymaster_blob_t* client_id,
    const keymaster_blob_t* app_data,
    keymaster_key_characteristics_t* characteristics)
{
    if (g_ssp_hw_status == SSP_HW_NOT_SUPPORTED) {
        BLOGI("SSP HW is not supported\n");
        return KM_ERROR_HARDWARE_TYPE_UNAVAILABLE;
    } else {
        return ssp_get_key_characteristics(sess,
            key_blob, client_id, app_data, characteristics);
    }
}

keymaster_error_t StrongboxKeymaster4DeviceImpl::import_key(
    const keymaster_key_param_set_t* params,
    keymaster_key_format_t key_format,
    const keymaster_blob_t* key_data,
    keymaster_key_blob_t* key_blob,
    keymaster_key_characteristics_t* characteristics)
{
    if (g_ssp_hw_status == SSP_HW_NOT_SUPPORTED) {
        BLOGI("SSP HW is not supported\n");
        return KM_ERROR_HARDWARE_TYPE_UNAVAILABLE;
    } else {
        return ssp_import_key(sess,
            params, key_format, key_data, key_blob, characteristics);
    }
}

keymaster_error_t StrongboxKeymaster4DeviceImpl::import_wrappedkey(
    const keymaster_blob_t* wrapped_key_data,
    const keymaster_key_blob_t* wrapping_key_blob,
    const keymaster_blob_t* masking_key,
    const keymaster_key_param_set_t* unwrapping_params,
    uint64_t pwd_sid,
    uint64_t bio_sid,
    keymaster_key_blob_t* key_blob,
    keymaster_key_characteristics_t* characteristics)
{
    if (g_ssp_hw_status == SSP_HW_NOT_SUPPORTED) {
        BLOGI("SSP HW is not supported\n");
        return KM_ERROR_HARDWARE_TYPE_UNAVAILABLE;
    } else {
        return ssp_import_wrappedkey(sess,
            wrapped_key_data, wrapping_key_blob, masking_key, unwrapping_params,
            pwd_sid, bio_sid,
            key_blob, characteristics);
    }
}


keymaster_error_t StrongboxKeymaster4DeviceImpl::export_key(
    keymaster_key_format_t export_format,
    const keymaster_key_blob_t* keyblob_to_export,
    const keymaster_blob_t* client_id,
    const keymaster_blob_t* app_data,
    keymaster_blob_t* exported_keyblob)
{
    if (g_ssp_hw_status == SSP_HW_NOT_SUPPORTED) {
        BLOGI("SSP HW is not supported\n");
        return KM_ERROR_HARDWARE_TYPE_UNAVAILABLE;
    } else {
        return ssp_export_key(sess,
            export_format, keyblob_to_export, client_id, app_data, exported_keyblob);
    }
}

keymaster_error_t StrongboxKeymaster4DeviceImpl::begin(
    keymaster_purpose_t purpose,
    const keymaster_key_blob_t* keyblob,
    const keymaster_key_param_set_t* in_params,
    const keymaster_hw_auth_token_t *auth_token,
    keymaster_key_param_set_t* out_params,
    keymaster_operation_handle_t* op_handle)
{
    if (g_ssp_hw_status == SSP_HW_NOT_SUPPORTED) {
        BLOGI("SSP HW is not supported\n");
        return KM_ERROR_HARDWARE_TYPE_UNAVAILABLE;
    } else {
        return ssp_begin(sess,
            purpose, keyblob, in_params, auth_token, out_params, op_handle);
    }
}

keymaster_error_t StrongboxKeymaster4DeviceImpl::update(
    keymaster_operation_handle_t op_handle,
    const keymaster_key_param_set_t* params,
    const keymaster_blob_t* input,
    const keymaster_hw_auth_token_t *auth_token,
    const keymaster_verification_token_t *verification_token,
    uint32_t* result_consumed,
    keymaster_key_param_set_t* out_params,
    keymaster_blob_t* output)
{
    if (g_ssp_hw_status == SSP_HW_NOT_SUPPORTED) {
        BLOGI("SSP HW is not supported\n");
        return KM_ERROR_HARDWARE_TYPE_UNAVAILABLE;
    } else {
        return ssp_update(sess,
            op_handle, params, input, auth_token, verification_token, result_consumed, out_params, output);
    }
}

keymaster_error_t StrongboxKeymaster4DeviceImpl::finish(
    keymaster_operation_handle_t op_handle,
    const keymaster_key_param_set_t* params,
    const keymaster_blob_t* input,
    const keymaster_blob_t* signature,
    const keymaster_hw_auth_token_t *auth_token,
    const keymaster_verification_token_t *verification_token,
    keymaster_key_param_set_t* out_params,
    keymaster_blob_t* output)
{
    if (g_ssp_hw_status == SSP_HW_NOT_SUPPORTED) {
        BLOGI("SSP HW is not supported\n");
        return KM_ERROR_HARDWARE_TYPE_UNAVAILABLE;
    } else {
        return ssp_finish(sess,
            op_handle, params, input, signature, auth_token, verification_token, out_params, output);
    }
}

keymaster_error_t StrongboxKeymaster4DeviceImpl::abort(
    keymaster_operation_handle_t op_handle)
{
    if (g_ssp_hw_status == SSP_HW_NOT_SUPPORTED) {
        BLOGI("SSP HW is not supported\n");
        return KM_ERROR_HARDWARE_TYPE_UNAVAILABLE;
    } else {
        return ssp_abort(sess, op_handle);
    }
}

keymaster_error_t StrongboxKeymaster4DeviceImpl::attest_key(
    const keymaster_key_blob_t* attest_keyblob,
    const keymaster_key_param_set_t* attest_params,
    keymaster_cert_chain_t* cert_chain)
{
    if (g_ssp_hw_status == SSP_HW_NOT_SUPPORTED) {
            BLOGI("SSP HW is not supported\n");
            return KM_ERROR_HARDWARE_TYPE_UNAVAILABLE;
    } else {
        return ssp_attest_key(sess,
            attest_keyblob, attest_params, cert_chain);
    }
}

keymaster_error_t StrongboxKeymaster4DeviceImpl::upgrade_key(
    const keymaster_key_blob_t* keyblob_to_upgrade,
    const keymaster_key_param_set_t* upgrade_params,
    keymaster_key_blob_t* upgraded_key)
{
    if (g_ssp_hw_status == SSP_HW_NOT_SUPPORTED) {
            BLOGI("SSP HW is not supported\n");
            return KM_ERROR_HARDWARE_TYPE_UNAVAILABLE;
    } else {
        return ssp_upgrade_key(sess,
            keyblob_to_upgrade, upgrade_params, upgraded_key);
    }
}

