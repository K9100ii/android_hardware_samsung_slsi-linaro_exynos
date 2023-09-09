#ifndef __SSP_STRONGBOX_KEYMASTER_LOG_H__
#define __SSP_STRONGBOX_KEYMASTER_LOG_H__

#ifndef LOG_TAG
#define LOG_TAG "StrongBox"
#endif

#include <log/log.h>
#include "ssp_strongbox_keymaster_defs.h"

#if defined(STRONGBOX_KEYMASTER_DEBUG)
#define BLOGE(fmt, ...) ALOGE("StrongBox: " fmt, ##__VA_ARGS__)
#define BLOGI(fmt, ...) ALOGI("StrongBox: " fmt, ##__VA_ARGS__)
#define BLOGD(fmt, ...) ALOGD("StrongBox: " fmt, ##__VA_ARGS__)
#else
#define BLOGE(fmt, ...) ALOGE("StrongBox: " fmt, ##__VA_ARGS__)
#define BLOGI(fmt, ...) ALOGI("StrongBox: " fmt, ##__VA_ARGS__)
#define BLOGD(fmt, ...)
#endif

void ssp_print_buf(const char *prefix, const uint8_t *data, size_t data_len);
void ssp_print_blob(const char *prefix, const keymaster_blob_t *blob);
void ssp_print_blob(const char *prefix, const keymaster_key_blob_t *blob);
void ssp_print_params(
    const char *prefix,
    const keymaster_key_param_set_t *param_set);

#endif
