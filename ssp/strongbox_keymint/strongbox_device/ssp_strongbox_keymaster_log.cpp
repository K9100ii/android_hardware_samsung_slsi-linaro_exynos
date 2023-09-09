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

#include <stdio.h>
#include <sstream>

#include <android-base/logging.h>

#include "ssp_strongbox_keymaster_log.h"
#include "ssp_strongbox_keymaster_defs.h"

#undef STRONGBOX_KEYMASTER_DUMP_DEBUG

#define LOG_BUF_SIZE 1000 /* it seems Android log buffer per line is 1000 */

#if defined(STRONGBOX_KEYMASTER_DUMP_DEBUG)
static char log_buf[LOG_BUF_SIZE];
#endif

#if defined(STRONGBOX_KEYMASTER_DUMP_DEBUG)
/* todo: support over 1000 char logs */
static int snprintf_buf(
    char *buf,
    size_t buf_len,
    const uint8_t *data,
    size_t data_len)
{
    int i;
    size_t len = 0, tmp_len;
    char tmp_buf[16];

    if (data == NULL) {
        return snprintf(buf, buf_len, "<NULL>\n");
    } else {
        len = snprintf(buf, buf_len, "<data length %zu>\n", data_len);
        buf = buf + len;
	    for (i = 0; i < (int)data_len; i++) {
            if (((i + 1) % 16) == 0) {
                tmp_len = snprintf(tmp_buf, sizeof(tmp_buf), "0x%02x,\n", data[i]);
            } else {
                tmp_len = snprintf(tmp_buf, sizeof(tmp_buf), "0x%02x, ", data[i]);
            }
            if (buf_len - 4 < tmp_len + len) {
                /* log buffer is full.
                * 4 is reserved for "NE"+ '\n' + '\0'
                */

                tmp_len = snprintf(buf, buf_len - len, "NE");
                len += tmp_len;
                buf += tmp_len;
                break;
            }
            strncpy(buf, tmp_buf, tmp_len);

            len += tmp_len;
            buf += tmp_len;

        }
        tmp_len = snprintf(buf, buf_len - len, "\n");
        len += tmp_len;

        return len;
    }
}

static int snprintf_u64(
    char *str,
    size_t len,
    uint64_t val)
{
    return snprintf(str, len, "0x%08x%08x\n",
            (uint32_t)(val >> 32),
            (uint32_t)(val & 0xFFFFFFFF));
}

static int snprintf_bool(
    char *str,
    size_t len,
    bool val)
{
    return snprintf(str, len, val ? "TRUE\n" : "FALSE\n");
}

static int snprintf_kmblob(
    char *str,
    size_t len,
    const keymaster_blob_t *blob)
{
    if (blob == NULL) {
        return snprintf(str, len, "<NULL>\n");
    } else {
        return snprintf_buf(str, len, blob->data, blob->data_length);
    }
}
#endif

void ssp_print_buf(
    const char *prefix,
    const uint8_t *data,
    size_t data_len)
{
#if defined(STRONGBOX_KEYMASTER_DUMP_DEBUG)
    char tmp_buf[256];
    int tmp_len = 0;
    int buf_len = 0;
    int i;
    int cnt = 0;

    if (data == NULL) {
        BLOGD("%s = <NULL>\n", prefix);
    } else {
        BLOGD("%s = <data length %zu>\n", prefix, data_len);
        for (i = 0; i < (int)data_len; i++) {
           if (((i + 1) % 16) == 0) {
               snprintf(tmp_buf + buf_len, sizeof(tmp_buf), "0x%02x,\n", data[i]);
               ALOGD("%04d:%s",cnt++, tmp_buf);
               buf_len = 0;
               tmp_len = 0;
               memset(tmp_buf, 0x00, sizeof(tmp_buf));
           } else {
               tmp_len = snprintf(tmp_buf + buf_len, sizeof(tmp_buf), "0x%02x, ", data[i]);
               buf_len += tmp_len;
           }
        }

        if (buf_len) {
            ALOGD("%04d:%s",cnt++, tmp_buf);
        }
    }
#else
    (void)prefix;
    (void)data;
    (void)data_len;
#endif
}

void ssp_print_blob(
    const char *prefix,
    const keymaster_blob_t *blob)
{
#if defined(STRONGBOX_KEYMASTER_DUMP_DEBUG)
    if (blob == NULL) {
        snprintf(log_buf, LOG_BUF_SIZE, "<NULL>\n");
        BLOGD("%s = %s", prefix, log_buf);
    } else {
        if (snprintf_buf(log_buf, LOG_BUF_SIZE, blob->data, blob->data_length) >= 0) {
            BLOGD("%s = %s", prefix, log_buf);
        }
    }
#else
    (void)prefix;
    (void)blob;
#endif
}

void ssp_print_blob(
    const char *prefix,
    const keymaster_key_blob_t *blob)
{
#if defined(STRONGBOX_KEYMASTER_DUMP_DEBUG)
    if (blob == NULL) {
        snprintf(log_buf, LOG_BUF_SIZE, "<NULL>\n");
        BLOGD("%s = %s", prefix, log_buf);
    } else {
        BLOGD("ptr: %p, size(%zu)\n", blob->key_material, blob->key_material_size);
        if (snprintf_buf(log_buf, LOG_BUF_SIZE, blob->key_material, blob->key_material_size) >= 0) {
            BLOGD("%s = %s", prefix, log_buf);
        }
    }
#else
    (void)prefix;
    (void)blob;
#endif
}

#define PARAM_ENUM snprintf(log_buf + total_len, LOG_BUF_SIZE - total_len, "0x%08x\n", param.enumerated)
#define PARAM_UINT snprintf(log_buf + total_len, LOG_BUF_SIZE - total_len, "%u\n", param.integer)
#define PARAM_ULONG snprintf_u64(log_buf + total_len, LOG_BUF_SIZE - total_len, param.long_integer)
#define PARAM_DATE snprintf_u64(log_buf + total_len, LOG_BUF_SIZE - total_len, param.date_time)
#define PARAM_BOOL snprintf_bool(log_buf + total_len, LOG_BUF_SIZE - total_len, param.boolean)
#define PARAM_BYTES snprintf_kmblob(log_buf + total_len, LOG_BUF_SIZE - total_len, &param.blob)
#define SPRINT_PARAM(prefix, param) \
        do { \
            len = snprintf(log_buf + total_len, LOG_BUF_SIZE - total_len, "%s: ", #prefix); \
            if (len < 0) \
                return; \
            total_len += len; \
            if (total_len < LOG_BUF_SIZE) { \
                len = param; \
                if (len < 0) \
                    return; \
                total_len += len; \
            } \
        } while (0)

void ssp_print_params(
    const char *prefix,
    const keymaster_key_param_set_t *param_set)
{
#if defined(STRONGBOX_KEYMASTER_DUMP_DEBUG)
    memset(log_buf, 0x00, sizeof(log_buf));

    if (param_set == NULL || param_set->length == 0) {
        snprintf(log_buf, LOG_BUF_SIZE, "<NULL>\n");
        BLOGD("%s = %s", prefix, log_buf);
    } else {
        int len;
        size_t i = 0, total_len = 0;
        while ((i < param_set->length) && (total_len < LOG_BUF_SIZE)) {
            const keymaster_key_param_t param = param_set->params[i];
            switch (param.tag) {
                case KM_TAG_PURPOSE:
                    SPRINT_PARAM(KM_TAG_PURPOSE, PARAM_ENUM);
                    break;
                case KM_TAG_ALGORITHM:
                    SPRINT_PARAM(KM_TAG_ALGORITHM, PARAM_ENUM);
                    break;
                case KM_TAG_KEY_SIZE:
                    SPRINT_PARAM(KM_TAG_KEY_SIZE, PARAM_UINT);
                    break;
                case KM_TAG_BLOCK_MODE:
                    SPRINT_PARAM(KM_TAG_BLOCK_MODE, PARAM_ENUM);
                    break;
                case KM_TAG_DIGEST:
                    SPRINT_PARAM(KM_TAG_DIGEST, PARAM_ENUM);
                    break;
                case KM_TAG_PADDING:
                    SPRINT_PARAM(KM_TAG_PADDING, PARAM_ENUM);
                    break;
                case KM_TAG_CALLER_NONCE:
                    SPRINT_PARAM(KM_TAG_CALLER_NONCE, PARAM_BOOL);
                    break;
                case KM_TAG_MIN_MAC_LENGTH:
                    SPRINT_PARAM(KM_TAG_MIN_MAC_LENGTH, PARAM_UINT);
                    break;
                case KM_TAG_KDF:
                     SPRINT_PARAM(KM_TAG_KDF, PARAM_ENUM);
                     break;
                case KM_TAG_EC_CURVE:
                    SPRINT_PARAM(KM_TAG_EC_CURVE, PARAM_ENUM);
                    break;
                case KM_TAG_RSA_PUBLIC_EXPONENT:
                    SPRINT_PARAM(KM_TAG_RSA_PUBLIC_EXPONENT, PARAM_ULONG);
                    break;
                case KM_TAG_ECIES_SINGLE_HASH_MODE:
                    SPRINT_PARAM(KM_TAG_ECIES_SINGLE_HASH_MODE, PARAM_BOOL);
                    break;
                case KM_TAG_INCLUDE_UNIQUE_ID:
                    SPRINT_PARAM(KM_TAG_INCLUDE_UNIQUE_ID, PARAM_BOOL);
                    break;
                case KM_TAG_BLOB_USAGE_REQUIREMENTS:
                    SPRINT_PARAM(KM_TAG_BLOB_USAGE_REQUIREMENTS, PARAM_ENUM);
                    break;
                case KM_TAG_BOOTLOADER_ONLY:
                    SPRINT_PARAM(KM_TAG_BOOTLOADER_ONLY, PARAM_BOOL);
                    break;
                case KM_TAG_ROLLBACK_RESISTANCE:
                    SPRINT_PARAM(KM_TAG_ROLLBACK_RESISTANCE, PARAM_BOOL);
                    break;
                case KM_TAG_HARDWARE_TYPE:
                    SPRINT_PARAM(KM_TAG_HARDWARE_TYPE, PARAM_ENUM);
                    break;
                case KM_TAG_ACTIVE_DATETIME:
                    SPRINT_PARAM(KM_TAG_ACTIVE_DATETIME, PARAM_DATE);
                    break;
                case KM_TAG_ORIGINATION_EXPIRE_DATETIME:
                    SPRINT_PARAM(KM_TAG_ORIGINATION_EXPIRE_DATETIME, PARAM_DATE);
                    break;
                case KM_TAG_USAGE_EXPIRE_DATETIME:
                    SPRINT_PARAM(KM_TAG_USAGE_EXPIRE_DATETIME, PARAM_DATE);
                    break;
                case KM_TAG_MIN_SECONDS_BETWEEN_OPS:
                    SPRINT_PARAM(KM_TAG_MIN_SECONDS_BETWEEN_OPS, PARAM_UINT);
                    break;
                case KM_TAG_MAX_USES_PER_BOOT:
                    SPRINT_PARAM(KM_TAG_MAX_USES_PER_BOOT, PARAM_UINT);
                    break;
                case KM_TAG_ALL_USERS:
                    SPRINT_PARAM(KM_TAG_ALL_USERS, PARAM_BOOL);
                    break;
                case KM_TAG_USER_ID:
                    SPRINT_PARAM(KM_TAG_USER_ID, PARAM_UINT);
                    break;
                case KM_TAG_USER_SECURE_ID:
                    SPRINT_PARAM(KM_TAG_USER_SECURE_ID, PARAM_ULONG);
                    break;
                case KM_TAG_NO_AUTH_REQUIRED:
                    SPRINT_PARAM(KM_TAG_NO_AUTH_REQUIRED, PARAM_BOOL);
                    break;
                case KM_TAG_USER_AUTH_TYPE:
                    SPRINT_PARAM(KM_TAG_USER_AUTH_TYPE, PARAM_ENUM);
                    break;
                case KM_TAG_AUTH_TIMEOUT:
                    SPRINT_PARAM(KM_TAG_AUTH_TIMEOUT, PARAM_UINT);
                    break;
                case KM_TAG_ALLOW_WHILE_ON_BODY:
                    SPRINT_PARAM(KM_TAG_ALLOW_WHILE_ON_BODY, PARAM_BOOL);
                    break;
                case KM_TAG_TRUSTED_USER_PRESENCE_REQUIRED:
                    SPRINT_PARAM(KM_TAG_TRUSTED_USER_PRESENCE_REQUIRED, PARAM_BOOL);
                    break;
                case KM_TAG_TRUSTED_CONFIRMATION_REQUIRED:
                    SPRINT_PARAM(KM_TAG_TRUSTED_CONFIRMATION_REQUIRED, PARAM_BOOL);
                    break;
                case KM_TAG_UNLOCKED_DEVICE_REQUIRED:
                    SPRINT_PARAM(KM_TAG_UNLOCKED_DEVICE_REQUIRED, PARAM_BOOL);
                    break;
                case KM_TAG_ALL_APPLICATIONS:
                    SPRINT_PARAM(KM_TAG_ALL_APPLICATIONS, PARAM_BOOL);
                    break;
                case KM_TAG_APPLICATION_ID:
                    SPRINT_PARAM(KM_TAG_APPLICATION_ID, PARAM_BYTES);
                    break;
                case KM_TAG_EXPORTABLE:
                    SPRINT_PARAM(KM_TAG_EXPORTABLE, PARAM_BOOL);
                    break;
                case KM_TAG_APPLICATION_DATA:
                    SPRINT_PARAM(KM_TAG_APPLICATION_DATA, PARAM_BYTES);
                    break;
                case KM_TAG_CREATION_DATETIME:
                    SPRINT_PARAM(KM_TAG_CREATION_DATETIME, PARAM_DATE);
                    break;
                case KM_TAG_ORIGIN:
                    SPRINT_PARAM(KM_TAG_ORIGIN, PARAM_ENUM);
                    break;
                case KM_TAG_ROOT_OF_TRUST:
                    SPRINT_PARAM(KM_TAG_ROOT_OF_TRUST, PARAM_BYTES);
                    break;
                case KM_TAG_OS_VERSION:
                    SPRINT_PARAM(KM_TAG_OS_VERSION, PARAM_UINT);
                    break;
                case KM_TAG_OS_PATCHLEVEL:
                    SPRINT_PARAM(KM_TAG_OS_PATCHLEVEL, PARAM_UINT);
                    break;
                case KM_TAG_UNIQUE_ID:
                    SPRINT_PARAM(KM_TAG_UNIQUE_ID, PARAM_BYTES);
                    break;
                case KM_TAG_ATTESTATION_CHALLENGE:
                    SPRINT_PARAM(KM_TAG_ATTESTATION_CHALLENGE, PARAM_BYTES);
                    break;
                case KM_TAG_ATTESTATION_APPLICATION_ID:
                    SPRINT_PARAM(KM_TAG_ATTESTATION_APPLICATION_ID, PARAM_BYTES);
                    break;
                case KM_TAG_ATTESTATION_ID_BRAND:
                    SPRINT_PARAM(KM_TAG_ATTESTATION_ID_BRAND, PARAM_BYTES);
                    break;
                case KM_TAG_ATTESTATION_ID_DEVICE:
                    SPRINT_PARAM(KM_TAG_ATTESTATION_ID_DEVICE, PARAM_BYTES);
                    break;
                case KM_TAG_ATTESTATION_ID_PRODUCT:
                    SPRINT_PARAM(KM_TAG_ATTESTATION_ID_PRODUCT, PARAM_BYTES);
                    break;
                case KM_TAG_ATTESTATION_ID_SERIAL:
                    SPRINT_PARAM(KM_TAG_ATTESTATION_ID_SERIAL, PARAM_BYTES);
                    break;
                case KM_TAG_ATTESTATION_ID_IMEI:
                    SPRINT_PARAM(KM_TAG_ATTESTATION_ID_IMEI, PARAM_BYTES);
                    break;
                case KM_TAG_ATTESTATION_ID_MEID :
                    SPRINT_PARAM(KM_TAG_ATTESTATION_ID_MEID , PARAM_BYTES);
                    break;
                case KM_TAG_ATTESTATION_ID_MANUFACTURER:
                    SPRINT_PARAM(KM_TAG_ATTESTATION_ID_MANUFACTURER, PARAM_BYTES);
                    break;
                case KM_TAG_ATTESTATION_ID_MODEL:
                    SPRINT_PARAM(KM_TAG_ATTESTATION_ID_MODEL, PARAM_BYTES);
                    break;
                case KM_TAG_VENDOR_PATCHLEVEL:
                    SPRINT_PARAM(KM_TAG_VENDOR_PATCHLEVEL, PARAM_UINT);
                    break;
                case KM_TAG_BOOT_PATCHLEVEL:
                    SPRINT_PARAM(KM_TAG_BOOT_PATCHLEVEL, PARAM_UINT);
                    break;
                case KM_TAG_ASSOCIATED_DATA:
                    SPRINT_PARAM(KM_TAG_ASSOCIATED_DATA, PARAM_BYTES);
                    break;
                case KM_TAG_NONCE:
                    SPRINT_PARAM(KM_TAG_NONCE, PARAM_BYTES);
                    break;
                case KM_TAG_AUTH_TOKEN:
                    SPRINT_PARAM(KM_TAG_AUTH_TOKEN, PARAM_BYTES);
                    break;
                case KM_TAG_MAC_LENGTH:
                    SPRINT_PARAM(KM_TAG_MAC_LENGTH, PARAM_UINT);
                    break;
                case KM_TAG_RESET_SINCE_ID_ROTATION:
                    SPRINT_PARAM(KM_TAG_RESET_SINCE_ID_ROTATION, PARAM_BOOL);
                    break;
                case KM_TAG_CONFIRMATION_TOKEN:
                    SPRINT_PARAM(KM_TAG_CONFIRMATION_TOKEN, PARAM_BYTES);
                    break;
                default:
                    len = snprintf(log_buf + total_len, LOG_BUF_SIZE - total_len,
                            "<invalid tag 0x%08x>\n", param.tag);
                    if (len < 0)
                        return;

                    total_len += len;
            }

            i++;
        }

        if (total_len >= 0) {
            BLOGD("%s =\n%s", prefix, log_buf);
        }
    }
#else
    (void)prefix;
    (void)param_set;
#endif
}

