/*
 * Copyright (c) 2013-2019 TRUSTONIC LIMITED
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
/** Mobicore Driver Registry.
 *
 * Implements the MobiCore driver registry which maintains trustlets.
 *
 * @file
 * @ingroup MCD_MCDIMPL_DAEMON_REG
 */
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>
#include <assert.h>
#include <string.h>
#include <string>
#include <cstring>
#include <cstddef>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>

#include "mcLoadFormat.h"
#include "mcVersionHelper.h"

#include "PrivateRegistry.h"

#include <tee_client_api.h>

#include "dynamic_log.h"

/** Maximum size of a shared object container in bytes. */
#define MAX_SO_CONT_SIZE  (512)

#define MC_REGISTRY_ALL      0
#define MC_REGISTRY_WRITABLE 1

#define AUTH_TOKEN_FILE_NAME "00000000.authtokcont"
#define AUTH_TOKEN_FILE_NAME_BACKUP_SUFFIX ".backup"
#define ENV_MC_AUTH_TOKEN_PATH "MC_AUTH_TOKEN_PATH"
#define TL_BIN_FILE_EXT ".tlbin"
#define DR_BIN_FILE_EXT ".drbin"
#define GP_TA_BIN_FILE_EXT ".tabin"

static std::vector<std::string> search_paths;
static std::string tb_storage_path;

//------------------------------------------------------------------------------
static std::string byteArrayToString(const void* bytes, size_t elems) {
    auto cbytes = static_cast<const char*>(bytes);
    char hx[elems * 2 + 1];

    for (size_t i = 0; i < elems; i++) {
        sprintf(&hx[i * 2], "%02x", cbytes[i]);
    }
    return std::string(hx);
}

//------------------------------------------------------------------------------
static bool dirExists(const char* path) {
    struct stat ss;

    return (path != NULL) && (stat(path, &ss) == 0) && S_ISDIR(ss.st_mode);
}

//------------------------------------------------------------------------------
const std::string& getTbStoragePath() {
    return tb_storage_path;
}

//------------------------------------------------------------------------------
static std::string getAuthTokenFilePath() {
    const char* path;
    std::string authTokenPath;

    // First, attempt to use regular auth token path environment variable.
    path = getenv(ENV_MC_AUTH_TOKEN_PATH);
    if (dirExists(path)) {
        LOG_D("getAuthTokenFilePath(): Using MC_AUTH_TOKEN_PATH %s", path);
        authTokenPath = path;
    } else {
        authTokenPath = search_paths[0];
        LOG_D("getAuthTokenFilePath(): Using path %s", authTokenPath.c_str());
    }

    return authTokenPath + "/" + AUTH_TOKEN_FILE_NAME;
}

//------------------------------------------------------------------------------
static std::string getAuthTokenFilePathBackup() {
    return getAuthTokenFilePath() + AUTH_TOKEN_FILE_NAME_BACKUP_SUFFIX;
}

//------------------------------------------------------------------------------
static std::string getGenericFilePath(const mcUuid_t* uuid, int registry, const std::string& ext) {
    std::string name = byteArrayToString(uuid, sizeof(*uuid)) + ext;

    if (registry == MC_REGISTRY_ALL) {
        for (size_t i = 1; i < search_paths.size(); i++) {
            std::string path = search_paths[i] + "/" + name;
            struct stat stat;

            if (::stat(path.c_str(), &stat) == 0) {
                return path;
            }
        }
    }
    return search_paths[0] + "/" + name;
}

//------------------------------------------------------------------------------
static std::string getTlBinFilePath(const mcUuid_t* uuid, int registry) {
    return getGenericFilePath(uuid, registry, TL_BIN_FILE_EXT);
}

//------------------------------------------------------------------------------
static std::string getDriverBinFilePath(const mcUuid_t* uuid, int registry) {
    std::vector<std::string> names;
    names.push_back(byteArrayToString(uuid, sizeof(*uuid)) + TL_BIN_FILE_EXT);
    names.push_back(byteArrayToString(uuid, sizeof(*uuid)) + DR_BIN_FILE_EXT);

    if (registry == MC_REGISTRY_ALL) {
        for (size_t i = 1; i < search_paths.size(); i++) {
            for (size_t k = 0; k < names.size(); k++) {
                std::string path = search_paths[i] + "/" + names[k];
                struct stat stat;
                if (::stat(path.c_str(), &stat) == 0) {
                    return path;
                }
            }
        }
    }
    return search_paths[0] + "/" + names[0];
}
//------------------------------------------------------------------------------
static std::string getTABinFilePath(const mcUuid_t* uuid, int registry) {
    return getGenericFilePath(uuid, registry, GP_TA_BIN_FILE_EXT);
}

//------------------------------------------------------------------------------
void setSearchPaths(const std::vector<std::string>& paths) {
    search_paths = paths;
    tb_storage_path = search_paths[0] + "/TbStorage";
}

static inline bool isAllZeros(const unsigned char* so, uint32_t size) {
    for (uint32_t i = 0; i < size; i++) {
        if (so[i]) {
            return false;
        }
    }

    return true;
}

//------------------------------------------------------------------------------
bool mcRegistryHasEndorsementToken() {
    const std::string& authTokenFilePath = getAuthTokenFilePath();
    LOG_D("testing AuthToken: %s", authTokenFilePath.c_str());
    struct stat stat;
    if (::stat(authTokenFilePath.c_str(), &stat) == 0) {
        return true;
    }

    const std::string& authTokenFilePathBackup = getAuthTokenFilePathBackup();
    LOG_D("testing AuthToken: %s", authTokenFilePathBackup.c_str());
    if (::stat(authTokenFilePathBackup.c_str(), &stat) == 0) {
        return true;
    }

    return false;
}


//------------------------------------------------------------------------------
mcResult_t mcRegistryStoreAuthToken(const void* so, size_t size) {
    int res = 0;
    if (so == NULL || size > 3 * MAX_SO_CONT_SIZE) {
        LOG_E("mcRegistry store So.Soc failed: %d", MC_DRV_ERR_INVALID_PARAMETER);
        return MC_DRV_ERR_INVALID_PARAMETER;
    }
    const std::string& authTokenFilePath = getAuthTokenFilePath();
    LOG_D("store AuthToken: %s", authTokenFilePath.c_str());

    /*
     * This special handling is needed because some of our OEM partners do not
     * supply the AuthToken to Kinibi Sphere. Instead, they retain the
     * AuthToken in a dedicated (reliable) storage area on the device. And in
     * that case, Kinibi Sphere is populated with a all-zero-padded AuthToken.
     * (Obviously the zero-padded AuthToken won't work, so we use that zero-
     * padding as an indicator to trigger the special behaviour.)
     *
     * When Kinibi Sphere supplies the device with the zero-padded AuthToken,
     * the device must look retrieve the AuthToken from its dedicated storage,
     * and use it in place of the zero-padded AuthToken supplied by Kinibi
     * Sphere. Since the AuthToken Backup will already have been retrieved from
     * the dedicated storage area by the time this method is called, using the
     * AuthToken Backup as our source is an acceptable proxy for retrieving it
     * from the dedicated storage area directly.
     *
     * This behaviour is triggered following Root.PA detecting a Factory Reset.
     */
    void* backup = NULL;
    if (isAllZeros((const unsigned char*)so, static_cast<uint32_t>(size))) {
        const std::string& authTokenFilePathBackup = getAuthTokenFilePathBackup();

        LOG_D("AuthToken is all zeros");
        FILE* backupfs = fopen(authTokenFilePathBackup.c_str(), "rb");
        if (backupfs) {
            backup = malloc(size);
            if (backup) {
                size_t readsize = fread(backup, 1, size, backupfs);
                if (readsize == size) {
                    LOG_D("AuthToken reset backup");
                    so = backup;
                } else {
                    LOG_E("AuthToken read size = %zu (%zu)", readsize, size);
                }
            }
            fclose(backupfs);
        } else {
            LOG_W("can't open AuthToken %s", authTokenFilePathBackup.c_str());
        }
    }

    mcResult_t ret = MC_DRV_OK;
    FILE* fs = fopen(authTokenFilePath.c_str(), "wb");
    if (fs == NULL) {
        ret = MC_DRV_ERR_INVALID_DEVICE_FILE;
    } else {
        res = fseek(fs, 0, SEEK_SET);
        if (res != 0) {
            ret = MC_DRV_ERR_INVALID_PARAMETER;
        } else {
            fwrite(so, 1, size, fs);
            if (ferror(fs)) {
                ret = MC_DRV_ERR_OUT_OF_RESOURCES;
            }
        }
        fclose(fs);
    }
    if (ret != MC_DRV_OK) {
        LOG_ERRNO("mcRegistry store So.Soc failed");
    }
    free(backup);

    return ret;
}

//------------------------------------------------------------------------------
mcResult_t mcRegistryReadAuthToken(mcSoAuthTokenCont_t* so) {
    int res = 0;
    if (NULL == so) {
        LOG_E("mcRegistry read So.Soc failed: %d", MC_DRV_ERR_INVALID_PARAMETER);
        return MC_DRV_ERR_INVALID_PARAMETER;
    }
    const std::string& authTokenFilePath = getAuthTokenFilePath();
    LOG_D("read AuthToken: %s", authTokenFilePath.c_str());

    FILE* fs = fopen(authTokenFilePath.c_str(), "rb");
    if (fs == NULL) {
        LOG_W("mcRegistry read So.Soc failed: %d", MC_DRV_ERR_INVALID_DEVICE_FILE);
        return MC_DRV_ERR_INVALID_DEVICE_FILE;
    }
    res = fseek(fs, 0, SEEK_END);
    if (res != 0) {
        LOG_E("mcRegistry read So.Soc failed: %d", MC_DRV_ERR_INVALID_PARAMETER);
        fclose(fs);
        return MC_DRV_ERR_INVALID_PARAMETER;
    }
    long filesize = ftell(fs);
    // We ensure that mcSoAuthTokenCont_t matches with filesize, as ferror (during fread operation) can't
    // handle the case where mcSoAuthTokenCont_t < filesize
    if (sizeof(mcSoAuthTokenCont_t) != filesize) {
        fclose(fs);
        LOG_W("mcRegistry read So.Soc failed: %d", MC_DRV_ERR_OUT_OF_RESOURCES);
        return MC_DRV_ERR_OUT_OF_RESOURCES;
    }
    res = fseek(fs, 0, SEEK_SET);
    if (res != 0) {
        LOG_E("mcRegistry read So.Soc failed: %d", MC_DRV_ERR_INVALID_PARAMETER);
        fclose(fs);
        return MC_DRV_ERR_INVALID_PARAMETER;
    }
    size_t read_res = fread(so, 1, sizeof(mcSoAuthTokenCont_t), fs);
    if (ferror(fs)) {
        fclose(fs);
        LOG_E("mcRegistry read So.Soc failed: %d", MC_DRV_ERR_INVALID_PARAMETER);
        return MC_DRV_ERR_INVALID_PARAMETER;
    }
    if (read_res < sizeof(mcSoAuthTokenCont_t)) {
        //File is shorter than expected
        if (feof(fs)) {
            LOG_E("%s(): EOF reached: res is %zu, size of mcSoAuthTokenCont_t is %zu",
                  __func__, read_res, sizeof(mcSoAuthTokenCont_t));
        }
        fclose(fs);
        return MC_DRV_ERR_INVALID_PARAMETER;
    }
    fclose(fs);

    return MC_DRV_OK;
}

//------------------------------------------------------------------------------
mcResult_t mcRegistryReadAuthTokenBackup(mcSoAuthTokenCont_t* so) {
    int res = 0;
    if (NULL == so) {
        LOG_E("mcRegistry read So.Soc failed: %d", MC_DRV_ERR_INVALID_PARAMETER);
        return MC_DRV_ERR_INVALID_PARAMETER;
    }
    const std::string& authTokenFilePath = getAuthTokenFilePathBackup();
    LOG_D("read AuthToken: %s", authTokenFilePath.c_str());

    FILE* fs = fopen(authTokenFilePath.c_str(), "rb");
    if (fs == NULL) {
        LOG_W("mcRegistry read So.Soc failed: %d", MC_DRV_ERR_INVALID_DEVICE_FILE);
        return MC_DRV_ERR_INVALID_DEVICE_FILE;
    }
    res = fseek(fs, 0, SEEK_END);
    if (res != 0) {
        LOG_E("mcRegistry read So.Soc failed: %d", MC_DRV_ERR_INVALID_PARAMETER);
        fclose(fs);
        return MC_DRV_ERR_INVALID_PARAMETER;
    }
    long filesize = ftell(fs);
    // We ensure that mcSoAuthTokenCont_t matches with filesize, as ferror (during fread operation) can't
    // handle the case where mcSoAuthTokenCont_t < filesize
    if (sizeof(mcSoAuthTokenCont_t) != filesize) {
        fclose(fs);
        LOG_W("mcRegistry read So.Soc failed: %d", MC_DRV_ERR_OUT_OF_RESOURCES);
        return MC_DRV_ERR_OUT_OF_RESOURCES;
    }
    res = fseek(fs, 0, SEEK_SET);
    if (res != 0) {
        LOG_E("mcRegistry read So.Soc failed: %d", MC_DRV_ERR_INVALID_PARAMETER);
        fclose(fs);
        return MC_DRV_ERR_INVALID_PARAMETER;
    }
    size_t read_res = fread(so, 1, sizeof(mcSoAuthTokenCont_t), fs);
    if (ferror(fs)) {
        fclose(fs);
        LOG_E("mcRegistry read So.Soc failed: %d", MC_DRV_ERR_INVALID_PARAMETER);
        return MC_DRV_ERR_INVALID_PARAMETER;
    }
    if (read_res < sizeof(mcSoAuthTokenCont_t)) {
        //File is shorter than expected
        if (feof(fs)) {
            LOG_E("%s(): EOF reached: res is %zu, size of mcSoAuthTokenCont_t is %zu",
                  __func__,
                  read_res, sizeof(mcSoAuthTokenCont_t));
        }
        fclose(fs);
        return MC_DRV_ERR_INVALID_PARAMETER;
    }
    fclose(fs);

    return MC_DRV_OK;
}

//------------------------------------------------------------------------------
mcResult_t mcRegistryDeleteAuthToken(void) {
    if (rename(getAuthTokenFilePath().c_str(),
               getAuthTokenFilePathBackup().c_str())) {
        LOG_ERRNO("Rename Auth token file!");
        return MC_DRV_ERR_UNKNOWN;
    } else {
        return MC_DRV_OK;
    }
}

//------------------------------------------------------------------------------
mcResult_t mcRegistryGetTrustletInfo(const mcUuid_t* uuid, bool isGpUuid,
                                     std::string& path) {
    // Ensure that a UUID is provided.
    if (NULL == uuid) {
        LOG_E("No UUID given");
        return MC_DRV_ERR_INVALID_PARAMETER;
    }

    if (isGpUuid) {
        path = getTABinFilePath(uuid, MC_REGISTRY_ALL);
    } else {
        path = getTlBinFilePath(uuid, MC_REGISTRY_ALL);
    }

    LOG_D("Returning %s", path.c_str());
    return MC_DRV_OK;
}

//------------------------------------------------------------------------------
mcResult_t mcRegistryGetDriverInfo(const mcUuid_t* uuid, std::string& path) {
    // Ensure that a UUID is provided.
    if (NULL == uuid) {
        LOG_E("No UUID given");
        return MC_DRV_ERR_INVALID_PARAMETER;
    }

    path = getDriverBinFilePath(uuid, MC_REGISTRY_ALL);

    LOG_D("Returning %s", path.c_str());
    return MC_DRV_OK;
}
