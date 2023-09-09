/*
 * Copyright (c) 2013-2017 TRUSTONIC LIMITED
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
#ifndef MOBICORE_REGISTRY_H_
#define MOBICORE_REGISTRY_H_

#include <string>
#include <vector>

#include "MobiCoreDriverApi.h"
#include "mcContainer.h"
#include "MobiCoreDriverCmd.h"

#ifdef __cplusplus
extern "C" {
#endif
//-----------------------------------------------------------------

void setSearchPaths(const std::vector<std::string>& paths);

/** Checks if a suitable endorsement token is present in the registry.
 * @return true if a taken is present.
 */
bool mcRegistryHasEndorsementToken(void);
/** Stores an authentication token in registry.
 * @param  so Authentication token secure object.
 * @param size Authentication token secure object size.
 * @return MC_DRV_OK if successful, otherwise error code.
 */
mcResult_t mcRegistryStoreAuthToken(const void *so, size_t size);

/** Reads an authentication token from registry.
 * @param[out] so Authentication token secure object.
 * @return MC_DRV_OK if successful, otherwise error code.
 */
mcResult_t mcRegistryReadAuthToken(mcSoAuthTokenCont_t *so);

/** Reads an authentication token backup from registry.
 * @param[out] so Authentication token secure object.
 * @return MC_DRV_OK if successful, otherwise error code.
 */
mcResult_t mcRegistryReadAuthTokenBackup(mcSoAuthTokenCont_t *so);

/** Deletes the authentication token secure object from the registry.
 * @return MC_DRV_OK if successful, otherwise error code.
 */
mcResult_t mcRegistryDeleteAuthToken(void);

/** Returns a registry object for a given service.
 * @param uuid service UUID
 * @param isGpUuid UUID is for a GP trustlet
 * @param[out] path The trustlet file path.
 * @return MC_DRV_OK if successful, otherwise error code.
 */
mcResult_t mcRegistryGetTrustletInfo(const mcUuid_t *uuid, bool isGpUuid, std::string& path);

/** Returns a registry object for a given driver UUID.
 * @param uuid driver UUID
 * @param[out] path The driver file path.
 * @return MC_DRV_OK if successful, otherwise error code.
 */
mcResult_t mcRegistryGetDriverInfo(const mcUuid_t* uuid, std::string& path);

#ifdef __cplusplus
}
#endif

#endif // MOBICORE_REGISTRY_H_

