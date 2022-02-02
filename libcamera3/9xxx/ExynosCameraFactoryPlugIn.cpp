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

#ifdef USES_DUAL_CAMERA_SOLUTION_ARCSOFT
#include "ExynosCameraPlugInConverterArcsoftFusionBokehCapture.h"
#include "ExynosCameraPlugInConverterArcsoftFusionBokehPreview.h"
#include "ExynosCameraPlugInConverterArcsoftFusionZoomCapture.h"
#include "ExynosCameraPlugInConverterArcsoftFusionZoomPreview.h"
#endif

#ifdef USES_HIFI_LLS
#include "ExynosCameraPlugInConverterHifills.h"
#endif

namespace android {

ExynosCameraFactoryPlugIn::ExynosCameraFactoryPlugIn()
{
    strncpy(m_name, "ExynosCameraFactoryPlugIn", (EXYNOS_CAMERA_NAME_STR_SIZE - 1));
    m_plugInTotalCount = sizeof(g_plugInList) / sizeof(PlugInList_t);
}

int ExynosCameraFactoryPlugIn::m_findPlugInIndexByPipeId(int cameraId, int id)
{
    status_t ret = NO_ERROR;
    int index;

    if (m_plugInTotalCount <= 0) {
        CLOGE3(cameraId, "there's no plugIn to support(%d)", m_plugInTotalCount);
        return -1;
    }

    if (id < 0) {
        CLOGE3(cameraId, "invalid id(%d))", id);
        return -1;
    }

    for (index = 0; index < m_plugInTotalCount; index++) {
        if (g_plugInList[index].id == id) {
            CLOGD3(cameraId, "%s plugIn is found(%d)!!", g_plugInList[index].plugInLibName, id);
            break;
        }
    }

    if (index == m_plugInTotalCount) {
        CLOGE3(cameraId, "there's any plugIn to match with id(%d)", id);
        index = -1;
        ret = INVALID_OPERATION;
    }

    return index;
}

status_t ExynosCameraFactoryPlugIn::create(int cameraId, int pipeId, ExynosCameraPlugInSP_dptr_t plugIn, ExynosCameraPlugInConverterSP_dptr_t converter, int scenario)
{
    int refCount;
    status_t ret = NO_ERROR;

    CLOGD3(cameraId, "pipeId(%d)", pipeId);

    plugIn = NULL;
    converter = NULL;

    Mutex::Autolock lock(m_lock);
    {
        bool firstShared = false;
        void *libHandle = NULL;
        PlugInSymbol_t createSymbol = NULL;
        int handleIndex = -1;

        /* 1. get the lib index by pipeId */
        int index = m_findPlugInIndexByPipeId(cameraId, scenario);
        if (index < 0) {
            CLOGE3(cameraId, "Invalid pipeId(%d)", pipeId);
            return INVALID_OPERATION;
        }

        handleIndex = getLibHandleID(scenario);

        /* 2. if plugIn is NULL, try to get plugIn */
        if (plugIn == NULL) {
            /* search lib handle */
            libHandle = m_mapLibHandle[handleIndex];
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

            plugIn = (ExynosCameraPlugIn *)createSymbol(cameraId, pipeId, scenario);
            if (plugIn != NULL) {
                /* 3. create the converter by pipeId */
                switch (scenario) {
#ifdef USE_DUAL_CAMERA
#ifdef USES_DUAL_CAMERA_SOLUTION_FAKE
                case PLUGIN_SCENARIO_FAKEFUSION_PREVIEW:
                    converter = (new ExynosCameraPlugInConverterFakeFusion(cameraId, pipeId));
                    break;
#endif
#ifdef USES_DUAL_CAMERA_SOLUTION_ARCSOFT
                case PLUGIN_SCENARIO_ZOOMFUSION_PREVIEW:
                    CLOGD3(cameraId, "new ExynosCameraPlugInConverterArcsoftFusionZoomPreview");
                    converter = (new ExynosCameraPlugInConverterArcsoftFusionZoomPreview(cameraId, pipeId));
                    break;
                case PLUGIN_SCENARIO_BOKEHFUSION_PREVIEW:
                    CLOGD3(cameraId, "new ExynosCameraPlugInConverterArcsoftFusionBokehPreview");
                    converter = (new ExynosCameraPlugInConverterArcsoftFusionBokehPreview(cameraId, pipeId));
                    break;
#endif

#ifdef USES_DUAL_CAMERA_SOLUTION_FAKE
                case PLUGIN_SCENARIO_FAKEFUSION_REPROCESSING:
                    converter = (new ExynosCameraPlugInConverterFakeFusion(cameraId, pipeId));
                    break;
#endif
#ifdef USES_DUAL_CAMERA_SOLUTION_ARCSOFT
                case PLUGIN_SCENARIO_ZOOMFUSION_REPROCESSING:
                    CLOGD3(cameraId, "new ExynosCameraPlugInConverterArcsoftFusionZoomCapture");
                    converter = (new ExynosCameraPlugInConverterArcsoftFusionZoomCapture(cameraId, pipeId));
                    break;

                case PLUGIN_SCENARIO_BOKEHFUSION_REPROCESSING:
                    CLOGD3(cameraId, "new ExynosCameraPlugInConverterArcsoftFusionBokehCapture");
                    converter = (new ExynosCameraPlugInConverterArcsoftFusionBokehCapture(cameraId, pipeId));
                    break;
#endif

#endif // USE_DUAL_CAMERA

#ifdef SLSI_LLS_REPROCESSING
                case PLUGIN_SCENARIO_BAYERLLS_REPROCESSING:
                    converter = (new ExynosCameraPlugInConverterLowLightShot(cameraId, pipeId));
                    break;
#endif

#ifdef USES_CAMERA_SOLUTION_VDIS
                case PLUGIN_SCENARIO_SWVDIS_PREVIEW:
                    converter = (new ExynosCameraPlugInConverterVDIS(cameraId, pipeId));
                    break;
#endif


#ifdef USES_HIFI_LLS
                case PLUGIN_SCENARIO_HIFILLS_REPROCESSING:
                    converter = (new ExynosCameraPlugInConverterHifills(cameraId, pipeId));
                    break;
#endif
                default:
                    CLOGD3(cameraId, "unknown plugin scenario pipeid(%d) scenario(%d) handle(%p) handleIndex(%d) refCount(%d)", pipeId, scenario, m_mapLibHandle[handleIndex], handleIndex, refCount);
                    ret = INVALID_OPERATION;
					return ret;
                    break;
                }
            }
        }

        /* 4. save the handle and plugIn to map */
        m_mapLibHandle[handleIndex] = libHandle;
        refCount = m_mapPlugInRefCount[handleIndex];
        refCount++;
        m_mapPlugInRefCount[handleIndex] = refCount;

        CLOGD3(cameraId, "createSymbol success!!(%d, %p, %p, %d)",
                pipeId, m_mapLibHandle[handleIndex], &plugIn, refCount);
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
    int pluginInfo;
    int vendor = 0;
    int type = 0;
    int handleIndex = 0;
    int scenario = 0;
    void *libHandle;
    bool shared;
    int refCount = 0;

    pipeId = plugIn->getPipeId();
    pluginInfo = plugIn->getLibInfo();
    vendor = getLibVendorID(pluginInfo);
    type = getLibType(pluginInfo);
    handleIndex = getLibHandleID(pluginInfo);
    scenario = getLibScenario(pluginInfo);

    CLOGD3(cameraId, "pipeId(%d) pluginInfo: vendor(%d) type(%d) handleIndex(%d) scenario(%d) ",
        pipeId, vendor, type, handleIndex, scenario);

    Mutex::Autolock lock(m_lock);
    {
        converter = NULL;
        plugIn = NULL;

        /* decrease the refCount */
        refCount = m_mapPlugInRefCount[handleIndex];
        if (refCount <= 0) {
            CLOGW3(cameraId, "pipeId(%d) refCount(%d) is invalid..", pipeId, refCount);
        } else {
            refCount--;
            m_mapPlugInRefCount[handleIndex] = refCount;
        }

        if (!refCount) {
            libHandle = m_mapLibHandle[handleIndex];
            if (libHandle != NULL) {
                ret = dlclose(libHandle);
                if (ret) {
                    CLOGE3(cameraId, "dlclose is failed!!(%d)", pipeId);
                    ret = INVALID_OPERATION;
                }
            }
            m_mapLibHandle[handleIndex] = NULL;
        }

        CLOGD3(cameraId, "destroy success!!(%d, %p, %p, %d)",
                pipeId, m_mapLibHandle[handleIndex], &plugIn, refCount);
    }

    return ret;
}

void ExynosCameraFactoryPlugIn::dump(int cameraId)
{
    /* nothing to do */
}
}
