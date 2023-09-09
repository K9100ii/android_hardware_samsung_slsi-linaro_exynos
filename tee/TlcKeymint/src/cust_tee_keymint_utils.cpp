/*
 * Copyright (c) 2016-2021 TRUSTONIC LIMITED
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
#include "cust_tee_keymint_utils.h"
#include "km_util.h"
#include <sys/system_properties.h>

/*
 *
 */
long getFileContent(const char* pPath, uint8_t** ppContent)
{
    FILE*   pStream;
    long    filesize;
    uint8_t* content = NULL;

   /* Open the file */
   pStream = fopen(pPath, "rb");
   if (pStream == NULL)
   {
      LOG_E("Error: Cannot open file: %s.", pPath);
      goto error;
   }

   if (fseek(pStream, 0L, SEEK_END) != 0)
   {
      LOG_E("Error: Cannot read file: %s.", pPath);
      goto error;
   }

   filesize = ftell(pStream);
   if (filesize < 0)
   {
      LOG_E("Error: Cannot get the file size: %s.", pPath);
      goto error;
   }

   if (filesize == 0)
   {
      LOG_E("Error: Empty file: %s.", pPath);
      goto error;
   }

   /* Set the file pointer at the beginning of the file */
   if (fseek(pStream, 0L, SEEK_SET) != 0)
   {
      LOG_E("Error: Cannot read file: %s.", pPath);
      goto error;
   }

   /* Allocate a buffer for the content */
   content = (uint8_t*)malloc(filesize);
   if (content == NULL)
   {
      LOG_E("Error: Cannot read file: Out of memory.");
      goto error;
   }

   /* Read data from the file into the buffer */
   if (fread(content, (size_t)filesize, 1, pStream) != 1)
   {
      LOG_E("Error: Cannot read file: %s.", pPath);
      goto error;
   }

   /* Close the file */
   fclose(pStream);
   *ppContent = content;

   /* Return number of bytes read */

   return filesize;

error:
   if (content  != NULL)
   {
       free(content);
   }
   if (pStream != NULL)
   {
       fclose(pStream);
   }
   return 0;
}

/* Assume date string is "YYYY-MM" or "YYYY-MM-DD"; convert to integer YYYYMMDD
   (where in the former case DD is 00). */
static bool int_from_datestr(
    uint32_t *D,
    const char *s,
    size_t s_len)
{
    uint32_t x;
    if (s_len < 7) return false;
    x = s[0] - '0';
    x *= 10; x += s[1] - '0';
    x *= 10; x += s[2] - '0';
    x *= 10; x += s[3] - '0';
    if (s[4] != '-') return false;
    x *= 10; x += s[5] - '0';
    x *= 10; x += s[6] - '0';
    if (s_len >= 10 && s[7] == '-') {
        x *= 10; x += s[8] - '0';
        x *= 10; x += s[9] - '0';
    } else {
        x *= 100;
    }
    *D = x;
    return true;
}

/* Read OS_VERSION, OS_PATCHLEVEL and VENDOR_PATCHLEVEL from system properties.
 *
 * OS_VERSION should have the format XXYYZZ where XX, YY and ZZ are the major,
 * minor and sub-minor version numbers.
 *
 * OS_PATCHLEVEL should have the format YYYYMM (year and month).
 *
 * VENDOR_PATCHLEVEL should have the format YYYYMMDD (year, month and day).
 *
 * Returns 0 on success, non-zero error code on failure.
 */
int get_version_info(
    bootimage_info_t *bootinfo)
{
    char os_version_str[PROP_VALUE_MAX];
    int os_version_strlen = __system_property_get("ro.build.version.release", os_version_str);
    char os_patchlevel_str[PROP_VALUE_MAX];
    int os_patchlevel_strlen = __system_property_get("ro.build.version.security_patch", os_patchlevel_str);
#ifdef ANDROID_P
    char vendor_patchlevel_str[PROP_VALUE_MAX];
    int vendor_patchlevel_strlen = __system_property_get("ro.vendor.build.security_patch", vendor_patchlevel_str);
#endif

    /* Assume os_version_str is "X" or "X.Y" or "X.Y.Z" (etc) where each number is less than 100.
     * Convert to integer XXYYZZ (XX[3]). */
    int XX[3] = {0};
    int i = 0;
    bool not_a_number = false;
    for (int j = 0; j < 3; j++) {
        while (i < os_version_strlen) {
            if (os_version_str[i] == '.') break;
            if ((os_version_str[i] < '0') || (os_version_str[i] > '9')) {
                LOG_I("The os_version obtained is not a number, will use 0 instead");
                bootinfo->os_version = 0;
                not_a_number = true;
                break;
            }
            XX[j] *= 10; XX[j] += (os_version_str[i] - '0');
            if (XX[j] >= 100) return -1;
            i++;
        }
        if (not_a_number) break;
        i++; // skip the '.'
    }
    if (!not_a_number)
        bootinfo->os_version = 100*(100*XX[0] +  XX[1]) + XX[2];

    if (!int_from_datestr(&bootinfo->os_patchlevel,
        os_patchlevel_str, os_patchlevel_strlen))
    {
        bootinfo->os_patchlevel = 0;
    }
    bootinfo->os_patchlevel /= 100;

#ifdef ANDROID_P
    if (!int_from_datestr(&bootinfo->vendor_patchlevel,
        vendor_patchlevel_str, vendor_patchlevel_strlen))
    {
        return -2;
    }
#else
    /* Dummy value used here, as the property will only be available on
     * Android P. See
     * https://android-review.googlesource.com/c/platform/system/sepolicy/+/660840/
     */
    bootinfo->vendor_patchlevel = 00000000;
#endif

    /* The boot patch level is not available from system properties. Instead,
     * the bootloader reads it from boot.img and provides it to Keymint (via
     * a driver). */
    bootinfo->boot_patchlevel = 0;

    return 0;
}
