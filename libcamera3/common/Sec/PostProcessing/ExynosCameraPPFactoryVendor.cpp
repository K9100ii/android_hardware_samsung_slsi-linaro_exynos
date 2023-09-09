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
#define LOG_TAG "ExynosCameraPPFactorySec"

#include "ExynosCameraPPFactory.h"

#ifdef USE_CIP_M2M_HW
#include "ExynosCameraPPCIP.h"
#endif
#ifdef SAMSUNG_LLS_DEBLUR
#include "ExynosCameraPPUniPluginLD.h"
#endif
#ifdef SAMSUNG_OT
#include "ExynosCameraPPUniPluginOT.h"
#endif
#ifdef SAMSUNG_HLV
#include "ExynosCameraPPUniPluginHLV.h"
#endif
#ifdef SAMSUNG_FOCUS_PEAKING
#include "ExynosCameraPPUniPluginFP.h"
#endif
#ifdef SAMSUNG_BD
#include "ExynosCameraPPUniPluginBD.h"
#endif
#ifdef SAMSUNG_STR_PREVIEW
#include "ExynosCameraPPUniPluginSTR_Preview.h"
#endif
#ifdef SAMSUNG_STR_CAPTURE
#include "ExynosCameraPPUniPluginSTR_Capture.h"
#endif
#ifdef SAMSUNG_IDDQD
#include "ExynosCameraPPUniPluginIDDQD.h"
#endif
#ifdef SAMSUNG_SW_VDIS
#include "ExynosCameraPPUniPluginSWVdis_3_0.h"
#endif

ExynosCameraPP *ExynosCameraPPFactory::newPP(
        int cameraId,
#ifndef SAMSUNG_TN_FEATURE
        __unused
#endif
        ExynosCameraParameters *parameters,
        int nodeNum,
        int scenario)
{
    status_t ret = NO_ERROR;
    ExynosCameraPP *newPP = NULL;

    int m_cameraId = cameraId;
    char m_name[EXYNOS_CAMERA_NAME_STR_SIZE] = "ExynosCameraPPFactory";

    switch (scenario) {
#ifdef SAMSUNG_LLS_DEBLUR
    case PP_SCENARIO_LLS_DEBLUR:
    case PP_SCENARIO_HIFI_LLS:
        CLOGD("new ExynosCameraPPUniPluginLD(cameraId(%d), nodeNum(%d), scenario(%d)",
            cameraId, nodeNum, scenario);
        newPP = new ExynosCameraPPUniPluginLD(cameraId, parameters, nodeNum, scenario);
        break;
#endif
#ifdef SAMSUNG_OT
    case PP_SCENARIO_OBJECT_TRACKING:
        CLOGD("new ExynosCameraPPUniPluginOT(cameraId(%d), nodeNum(%d), scenario(%d)",
            cameraId, nodeNum, scenario);
        newPP = new ExynosCameraPPUniPluginOT(cameraId, parameters, nodeNum);
        break;
#endif
#ifdef SAMSUNG_HLV
    case PP_SCENARIO_HLV:
        CLOGD("new ExynosCameraPPUniPluginHLV(cameraId(%d), nodeNum(%d), scenario(%d)",
            cameraId, nodeNum, scenario);
        newPP = new ExynosCameraPPUniPluginHLV(cameraId, parameters, nodeNum);
        break;
#endif
#ifdef SAMSUNG_FOCUS_PEAKING
    case PP_SCENARIO_FOCUS_PEAKING:
        CLOGD("new ExynosCameraPPUniPluginFP(cameraId(%d), nodeNum(%d), scenario(%d)",
            cameraId, nodeNum, scenario);
        newPP = new ExynosCameraPPUniPluginFP(cameraId, parameters, nodeNum);
        break;
#endif
#ifdef SAMSUNG_BD
    case PP_SCENARIO_BLUR_DETECTION:
        CLOGD("new ExynosCameraPPUniPluginBD(cameraId(%d), nodeNum(%d), scenario(%d)",
            cameraId, nodeNum, scenario);
        newPP = new ExynosCameraPPUniPluginBD(cameraId, parameters, nodeNum);
        break;
#endif
#ifdef SAMSUNG_STR_PREVIEW
    case PP_SCENARIO_STR_PREVIEW:
        CLOGD("new ExynosCameraPPUniPluginSTR_Preview(cameraId(%d), nodeNum(%d), scenario(%d)",
            cameraId, nodeNum, scenario);
        newPP = new ExynosCameraPPUniPluginSTR_Preview(cameraId, parameters, nodeNum);
        break;
#endif
#ifdef SAMSUNG_STR_CAPTURE
    case PP_SCENARIO_STR_CAPTURE:
        CLOGD("new ExynosCameraPPUniPluginSTR_Capture(cameraId(%d), nodeNum(%d), scenario(%d)",
            cameraId, nodeNum, scenario);
        newPP = new ExynosCameraPPUniPluginSTR_Capture(cameraId, parameters, nodeNum);
        break;
#endif
#ifdef SAMSUNG_IDDQD
    case PP_SCENARIO_IDDQD:
        CLOGD("new ExynosCameraPPUniPluginIDDQD(cameraId(%d), nodeNum(%d), scenario(%d)",
            cameraId, nodeNum, scenario);
        newPP = new ExynosCameraPPUniPluginIDDQD(cameraId, parameters, nodeNum);
        break;
#endif
#ifdef SAMSUNG_SW_VDIS
    case PP_SCENARIO_SW_VDIS:
        CLOGD("new ExynosCameraPPUniPluginSWVdis(cameraId(%d), nodeNum(%d), scenario(%d)",
            cameraId, nodeNum, scenario);
        newPP = new ExynosCameraPPUniPluginSWVdis(cameraId, parameters, nodeNum);
        break;
#endif
    default:
        CLOGE("Invalid nodeNum(%d) fail", nodeNum);
        ret = INVALID_OPERATION;
        break;
    }

    return newPP;
}

