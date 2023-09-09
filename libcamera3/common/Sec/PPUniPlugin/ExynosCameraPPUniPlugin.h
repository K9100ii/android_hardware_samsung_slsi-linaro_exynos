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

/*!
 * \file      ExynosCameraPPUniPlugin.h
 * \brief     header file for ExynosCameraPPUniPlugin
 * \author    Sangwoo, Park(sw5771.park@samsung.com)
 * \date      2016/10/05
 *
 * <b>Revision History: </b>
 * - 2016/10/05 : Sangwoo, Park(sw5771.park@samsung.com) \n
 *   Initial version
 */

#ifndef EXYNOS_CAMERA_PP_UNIPLUGIN_H
#define EXYNOS_CAMERA_PP_UNIPLUGIN_H

#include <cutils/log.h>

#include "ExynosCameraThread.h"
#include "ExynosCameraPP.h"
#include "ExynosCameraParameters.h"
#ifdef SAMSUNG_UNIPLUGIN
#include "uni_plugin_wrapper.h"
#endif

using namespace android;

enum PP_SCENARIO_TYPE {
    PP_SCENARIO_NONE,
    PP_SCENARIO_LLS_DEBLUR,
    PP_SCENARIO_HIFI_LLS,
    PP_SCENARIO_OBJECT_TRACKING,
    PP_SCENARIO_HLV,
    PP_SCENARIO_FOCUS_PEAKING,
    PP_SCENARIO_BLUR_DETECTION,
    PP_SCENARIO_STR_PREVIEW,
    PP_SCENARIO_STR_CAPTURE,
    PP_SCENARIO_IDDQD,
    PP_SCENARIO_SW_VDIS,
    PP_SCENARIO_MAX,
};

typedef struct PPConnectionInfo {
    int scenario;
    int type;
} PPConnectionInfo_t;

enum PP_CONNECTION_TYPE {
    PP_CONNECTION_NORMAL,
    PP_CONNECTION_ONLY,
};

enum {
    PP_STAGE_0,
    PP_STAGE_1,
    PP_STAGE_MAX,
};

/*
 * Class ExynosCameraPPUniPlugin
 */
class ExynosCameraPPUniPlugin : public ExynosCameraPP
{
protected:
    ExynosCameraPPUniPlugin()
    {
        m_init();
    }

    ExynosCameraPPUniPlugin(
            int cameraId,
            ExynosCameraParameters *parameters,
            int nodeNum) : ExynosCameraPP(cameraId, parameters, nodeNum)
    {
        strncpy(m_name, "ExynosCameraPPUniPlugin",  EXYNOS_CAMERA_NAME_STR_SIZE - 1);

        m_init();
    }

    /* ExynosCameraPPUniPlugin's constructor is protected
     * to prevent new without ExynosCameraPPFactory::newPP()
     */
    friend class ExynosCameraPPFactory;

public:
    virtual ~ExynosCameraPPUniPlugin();

protected:
    virtual status_t m_create(void);
    virtual status_t m_destroy(void);

    virtual status_t m_UniPluginInit(void);
    virtual status_t m_UniPluginDeinit(void);
    virtual status_t m_UniPluginProcess(void);
    virtual status_t m_UniPluginSet(int index, void *data);
    virtual status_t m_UniPluginGet(int index, void *data);
    UNI_PLUGIN_HDR_MODE             m_getUniHDRMode();
    UNI_PLUGIN_FLASH_MODE           m_getUniFlashMode();
    UNI_PLUGIN_DEVICE_ORIENTATION   m_getUniOrientationMode();

    virtual status_t m_UniPluginThreadJoin(void);
private:
    void            m_init(void);

    typedef ExynosCameraThread<ExynosCameraPPUniPlugin>	  ExynosCameraPPUniPluginThread;
    bool m_loadThreadFunc(void);

protected:
    void                                 *m_UniPluginHandle;
    char                                 m_UniPluginName[EXYNOS_CAMERA_NAME_STR_SIZE];
    sp<ExynosCameraPPUniPluginThread>	 m_loadThread;
};

#endif //EXYNOS_CAMERA_PP_UNIPLUGIN_H
