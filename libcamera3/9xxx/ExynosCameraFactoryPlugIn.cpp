/*
 * Copyright@ Samsung Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

/*#define LOG_NDEBUG 0 */
#define LOG_TAG "ExynosCameraFactoryPlugIn"
#include <log/log.h>
#include <dlfcn.h>

#include "PlugInList.h"
#include "ExynosCameraFactoryPlugIn.h"

/* include the converter header depending on the feature */
#ifdef USES_DUAL_CAMERA_SOLUTION_FAKE
#include "ExynosCameraPlugInConverterFakeFusion.h"
#endif
#ifdef SLSI_LLS_REPROCESSING
#include "ExynosCameraPlugInConverterLowLightShot.h"
#endif

#ifdef USES_CAMERA_SOLUTION_VDIS
#include "ExynosCameraPlugInConverterVDIS.h"
#endif

namespace android {

ExynosCameraFactoryPlugIn::ExynosCameraFactoryPlugIn()
{
    strncpy(m_name, "ExynosCameraFactoryPlugIn", (EXYNOS_CAMERA_NAME_STR_SIZE - 1));
    m_plugInTotalCount = sizeof(g_plugInList) / sizeof(PlugInList_t);
}

int ExynosCameraFactoryPlugIn::m_findPlugInIndexByPipeId(int cameraId, int pipeId)
{
    int index;

    if (m_plugInTotalCount <= 0) {
        CLOGE3(cameraId, "there's no plugIn to support(%d)", m_plugInTotalCount);
        return -1;
    }

    if (pipeId < 0) {
        CLOGE3(cameraId, "invalid pipeId(%d))", pipeId);
        return -1;
    }

    for (index = 0; index < m_plugInTotalCount; index++) {
        if (g_plugInList[index].pipeId == pipeId) {
            CLOGD3(cameraId, "%s plugIn is found(%d)!!", g_plugInList[index].plugInLibName, pipeId);
            break;
        }
    }

    if (index == m_plugInTotalCount) {
        CLOGE3(cameraId, "there's any plugIn to match with pipeId(%d)", pipeId);
        index = -1;
    }

    return index;
}

status_t ExynosCameraFactoryPlugIn::create(int cameraId, int pipeId, ExynosCameraPlugInSP_dptr_t plugIn, ExynosCameraPlugInConverterSP_dptr_t converter, PLUGIN::MODE mode)
{
    int refCount;
    status_t ret = NO_ERROR;

    CLOGD3(cameraId, "pipeId(%d)", pipeId);

    plugIn = NULL;
    converter = NULL;

    Mutex::Autolock lock(m_lock);
    {
        void *libHandle = NULL;
        PlugInSymbol_t createSymbol = NULL;

        /* 1. get the lib index by pipeId */
        int index = m_findPlugInIndexByPipeId(cameraId, pipeId);
        if (index < 0) {
            CLOGE3(cameraId, "Invalid pipeId(%d)", pipeId);
            return INVALID_OPERATION;
        }

        /* 2. if plugIn is NULL, try to get plugIn */
        if (plugIn == NULL) {
            /* search lib handle */
            libHandle = m_mapLibHandle[pipeId];
            if (libHandle == NULL) {
                libHandle = dlopen(g_plugInList[index].plugInLibName, RTLD_NOW);
                if (libHandle == NULL) {
                    CLOGE3(cameraId, "Can't load %s library(%d, %d)",
                            g_plugInList[index].plugInLibName, index, pipeId);
                    return INVALID_OPERATION;
                }
            }

            *(reinterpret_cast<void **>(&createSymbol)) = dlsym(libHandle, PLUGIN_SYMBOL_NAME);
            if (createSymbol == NULL) {
                CLOGE3(cameraId, "Can't find symbol(%s) from library(%s, %d, %d)",
                        PLUGIN_SYMBOL_NAME,
                        g_plugInList[index].plugInLibName, index, pipeId);
                dlclose(libHandle);
                return INVALID_OPERATION;
            }

            plugIn = (ExynosCameraPlugIn *)createSymbol(cameraId, pipeId, mode);
            if (plugIn != NULL) {
                /* 3. create the converter by pipeId */
                switch (pipeId) {
#ifdef USE_DUAL_CAMERA
                case PIPE_FUSION:
                case PIPE_FUSION_FRONT:
                case PIPE_FUSION_REPROCESSING:
#ifdef USES_DUAL_CAMERA_SOLUTION_FAKE
                    converter = (new ExynosCameraPlugInConverterFakeFusion(cameraId, pipeId));
#endif
                    break;
#endif
#ifdef SLSI_LLS_REPROCESSING
                case PIPE_LLS_REPROCESSING:
                    converter = (new ExynosCameraPlugInConverterLowLightShot(cameraId, pipeId));
                    break;
#endif
                case PIPE_VDIS:
#ifdef USES_CAMERA_SOLUTION_VDIS
                    converter = (new ExynosCameraPlugInConverterVDIS(cameraId, pipeId));
#endif
                    break;
                default:
                    break;
                }
            }
        }

        /* 4. save the handle and plugIn to map */
        m_mapLibHandle[pipeId] = libHandle;
        refCount = m_mapPlugInRefCount[pipeId];
        refCount++;
        m_mapPlugInRefCount[pipeId] = refCount;

        CLOGD3(cameraId, "createSymbol success!!(%d, %p, %p, %d)",
                pipeId, m_mapLibHandle[pipeId], &plugIn, refCount);
    }

    return ret;
}

status_t ExynosCameraFactoryPlugIn::destroy(int cameraId, ExynosCameraPlugInSP_dptr_t plugIn, ExynosCameraPlugInConverterSP_dptr_t converter)
{
    if (plugIn == NULL) {
        CLOGE3(cameraId, "plugIn is NULL");
        return INVALID_OPERATION;
    }

    status_t ret = NO_ERROR;
    int pipeId;
    void *libHandle;
    int refCount = 0;

    pipeId = plugIn->getPipeId();
    CLOGD3(cameraId, "pipeId(%d)", pipeId);

    Mutex::Autolock lock(m_lock);
    {
        converter = NULL;
        plugIn = NULL;

        /* decrease the refCount */
        refCount = m_mapPlugInRefCount[pipeId];
        if (refCount <= 0) {
            CLOGW3(cameraId, "pipeId(%d) refCount(%d) is invalid..", pipeId, refCount);
        } else {
            refCount--;
            m_mapPlugInRefCount[pipeId] = refCount;
        }

        if (!refCount) {
            libHandle = m_mapLibHandle[pipeId];
            if (libHandle != NULL) {
                ret = dlclose(libHandle);
                if (ret) {
                    CLOGE3(cameraId, "dlclose is failed!!(%d)", pipeId);
                    ret = INVALID_OPERATION;
                }
            }
            m_mapLibHandle[pipeId] = NULL;
        }

        CLOGD3(cameraId, "destroy success!!(%d, %p, %p, %d)",
                pipeId, m_mapLibHandle[pipeId], &plugIn, refCount);
    }

    return ret;
}

void ExynosCameraFactoryPlugIn::dump(__unused int cameraId)
{
    /* nothing to do */
}
}
