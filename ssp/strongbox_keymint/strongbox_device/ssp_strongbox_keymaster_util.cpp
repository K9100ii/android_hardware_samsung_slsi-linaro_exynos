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
#include <errno.h>

#include "ssp_strongbox_keymaster_common_util.h"
#include "ssp_strongbox_tee_api.h"

/* get uint32_t from serialzied little endian data buffer */
uint32_t btoh_u32(const uint8_t *pos) {
    uint32_t val = 0;
    int i = 0;

    for (i = 0; i < 4; i++) {
        val |= ((uint32_t)pos[i] << (8*i));
    }
    return val;
}

/* get uint64_t from serialzied little endian data buffer */
uint64_t btoh_u64(const uint8_t *pos) {
    uint64_t val = 0;
    int i = 0;

    for (i = 0; i < 8; i++) {
        val |= ((uint64_t)pos[i] << (8*i));
    }
    return val;
}

/* set uint32_t to serialzied little endian data buffer */
void htob_u32(uint8_t *pos, uint32_t val) {
    int i = 0;

    for (i = 0; i < 4; i++) {
        pos[i] = (val >> (8*i)) & 0xFF;
    }
}

/* set uint64_t to serialzied little endian data buffer */
void htob_u64(uint8_t *pos, uint64_t val) {
    int i = 0;

    for (i = 0; i < 8; i++) {
        pos[i] = (val >> (8*i)) & 0xFF;
    }
}

/**
 * append uint32_t to buffer as little endian
 * and advance buffer pointer as sizeof uint32_t.
 */
void append_u32_to_buf(uint8_t **pos, uint32_t val)
{
    htob_u32(*pos, val);
    *pos += 4;
}

/**
 * append uint64_t to buffer as little endian
 * and advance buffer pointer as sizeof uint64_t.
 */
void append_u64_to_buf(uint8_t **pos, uint64_t val)
{
    htob_u64(*pos, val);
    *pos += 8;
}

/**
 * append uint8_t array to buffer
 * and advance buffer pointer as array size
 */
void append_u8_array_to_buf(uint8_t **pos, const uint8_t *src, uint32_t len)
{
    memcpy(*pos, src, len);
    *pos += len;
}

uint32_t ec_curve_to_keysize(
    keymaster_ec_curve_t ec_curve)
{
    switch (ec_curve) {
        case KM_EC_CURVE_P_224:
            return 224;
        case KM_EC_CURVE_P_256:
            return 256;
        default:
            return 0;
    }
}

int get_file_contents(const char *path, std::unique_ptr<uint8_t[]>& contents, long *fsize)
{
    FILE *fp = NULL;
    long int filesize = 0;
    int ret = -1;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        BLOGE("file open fail: %s. errno(%d)", path, errno);
        goto end;
    }

    if (fseek(fp, 0L, SEEK_END) != 0) {
        BLOGE("fseek fail: %s. errno(%d)", path, errno);
        goto end;
    }

    filesize = ftell(fp);
    if (filesize <= 0) {
        BLOGE("invalid file size: %s. size(%ld), errno(%d)", path, filesize, errno);
        goto end;
    }

    contents.reset(new (std::nothrow) uint8_t[filesize]);

    if (fseek(fp, 0L, SEEK_SET) != 0) {
        BLOGE("fseek fail: %s. errno(%d)", path, errno);
        goto end;
    }

    if (fread(contents.get(), 1, (size_t)filesize, fp) != filesize) {
        BLOGE("cannot read file: %s. errno(%d)", path, errno);
        goto end;
    }

    *fsize = filesize;
    ret = 0;

end:
    if (fp) {
        fclose(fp);
    }

    return ret;
}

int get_blob_contents(const keymaster_key_blob_t *blob,
                      std::unique_ptr<uint8_t[]>& contents, long *fsize)
{
    unsigned long filesize = 0;
    int ret = -1;

    filesize = blob->key_material_size;
    if (filesize <= 0) {
        BLOGE("invalid file size: size(%ld)", filesize);
        goto end;
    }

    contents.reset(new (std::nothrow) uint8_t[filesize]);

    memcpy(contents.get(), blob->key_material, filesize);

    *fsize = filesize;
    ret = 0;

end:
    return ret;
}

int find_tag_pos(
    const keymaster_key_param_set_t *params,
    keymaster_tag_t tag)
{
    size_t num_params = params ? params->length : 0;

    for (size_t i = 0; i < num_params; i++) {
        if (params->params[i].tag == tag) {
            return i;
        }
    }

    return -1;
}

bool check_purpose(
    const keymaster_key_param_set_t *params,
    keymaster_purpose_t purpose)
{
    size_t num_params = params ? params->length : 0;

    for (size_t i = 0; i < num_params; i++) {
        if (params->params[i].tag == KM_TAG_PURPOSE) {
            if (params->params[i].enumerated == purpose) {
                return true;
            }
        }
    }
    return false;
}


/**
 * get enum value for TAG
 */
keymaster_error_t get_tag_value_enum(
    const keymaster_key_param_set_t *params,
    keymaster_tag_t tag,
    uint32_t *value)
{
    int pos = 0;

    if ( (params == NULL) || (value == NULL) ) {
        return KM_ERROR_UNEXPECTED_NULL_POINTER;
    }

    pos = find_tag_pos(params, tag);
    if (pos < 0) {
        return KM_ERROR_INVALID_TAG;
    } else {
        *value = params->params[pos].enumerated;
        return KM_ERROR_OK;
    }
}

/**
 * get int value for TAG
 */
keymaster_error_t get_tag_value_int(
    const keymaster_key_param_set_t *params,
    keymaster_tag_t tag,
    uint32_t *value)
{
    int pos = 0;

    if ( (params == NULL) || (value == NULL) ) {
        return KM_ERROR_UNEXPECTED_NULL_POINTER;
    }

    pos = find_tag_pos(params, tag);
    if (pos < 0) {
        return KM_ERROR_INVALID_TAG;
    } else {
        *value = params->params[pos].integer;
        return KM_ERROR_OK;
    }
}

/**
 * get long value for TAG
 */
keymaster_error_t get_tag_value_long(
    const keymaster_key_param_set_t *params,
    keymaster_tag_t tag,
    uint64_t *value)
{
    int pos = 0;

    if ( (params == NULL) || (value == NULL) ) {
        return KM_ERROR_UNEXPECTED_NULL_POINTER;
    }

    pos = find_tag_pos(params, tag);
    if (pos < 0) {
        return KM_ERROR_INVALID_TAG;
    } else {
        *value = params->params[pos].long_integer;
        return KM_ERROR_OK;
    }
}
