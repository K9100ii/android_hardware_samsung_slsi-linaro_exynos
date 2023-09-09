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
 * \file      ExynosCameraPPUniPluginOT.h
 * \brief     header file for ExynosCameraPPUniPluginOT
 * \author    Sangwoo, Park(sw5771.park@samsung.com)
 * \date      2016/10/05
 *
 * <b>Revision History: </b>
 * - 2016/10/05 : Sangwoo, Park(sw5771.park@samsung.com) \n
 *   Initial version
 */

#ifndef EXYNOS_CAMERA_PP_UNIPLUGIN_OT_H
#define EXYNOS_CAMERA_PP_UNIPLUGIN_OT_H

#include "ExynosCameraDefine.h"

#include "ExynosCameraPPUniPlugin.h"

using namespace android;

enum objet_tracking_status {
    OBJECT_TRACKING_DEINIT              = 0,
    OBJECT_TRACKING_IDLE,
    OBJECT_TRACKING_RUN,
};

/*
 * Class ExynosCameraPPUniPluginOT
 */
class ExynosCameraPPUniPluginOT : public ExynosCameraPPUniPlugin
{
protected:
    ExynosCameraPPUniPluginOT()
    {
        m_init();
    }

    ExynosCameraPPUniPluginOT(
            int cameraId,
            ExynosCameraConfigurations *configurations,
            ExynosCameraParameters *parameters,
            int nodeNum) : ExynosCameraPPUniPlugin(cameraId, configurations, parameters, nodeNum)
    {
        strncpy(m_name, "ExynosCameraPPUniPluginOT",  EXYNOS_CAMERA_NAME_STR_SIZE - 1);

        m_init();
    }

    /* ExynosCameraPPUniPluginOT's constructor is protected
     * to prevent new without ExynosCameraPPFactory::newPP()
     */
    friend class ExynosCameraPPFactory;

public:
    virtual ~ExynosCameraPPUniPluginOT();
    status_t    start(void);
    status_t    stop(bool suspendFlag = false);

protected:
    virtual status_t m_draw(ExynosCameraImage *srcImage,
                            ExynosCameraImage *dstImage,
                            ExynosCameraParameters *params);

private:
    void     m_init(void);

    UniPluginFocusData_t            m_OTfocusData;
    UniPluginFocusData_t            m_OTpredictedData;

    int                             m_refCount;

    int                             m_OTstatus;
    mutable Mutex                   m_uniPluginLock;
protected:
};

#endif //EXYNOS_CAMERA_PP_UNIPLUGIN_OT_H
