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
 * \file      ExynosCameraPPUniPluginHyperlapse.h
 * \brief     header file for ExynosCameraPPUniPluginHyperlapse
 * \author    Byungho, Ahn(bh1212.ahn@samsung.com)
 * \date      2017/05/18
 *
 * <b>Revision History: </b>
 * - 2017/05/18 : Byungho, Ahn(bh1212.ahn@samsung.com) \n
 *   Initial version
 */

#ifndef EXYNOS_CAMERA_PP_UNIPLUGIN_Hyperlapse_H
#define EXYNOS_CAMERA_PP_UNIPLUGIN_Hyperlapse_H

#include "ExynosCameraDefine.h"
#include "ExynosCameraPPUniPlugin.h"

using namespace android;

#define ABLE_FACECNT 10

enum hyperlapse_status {
    HYPERLAPSE_IDLE = 0,
    HYPERLAPSE_RUN,
};

/*
 * Class ExynosCameraPPUniPluginHyperlapse
 */
class ExynosCameraPPUniPluginHyperlapse : public ExynosCameraPPUniPlugin
{
protected:
    ExynosCameraPPUniPluginHyperlapse()
    {
        m_init();
    }

    ExynosCameraPPUniPluginHyperlapse(
            int cameraId,
            ExynosCameraConfigurations *configurations,
            ExynosCameraParameters *parameters,
            int nodeNum) : ExynosCameraPPUniPlugin(cameraId, configurations, parameters, nodeNum)
    {
        strncpy(m_name, "ExynosCameraPPUniPluginHyperlapse",  EXYNOS_CAMERA_NAME_STR_SIZE - 1);

        m_init();
    }

    /* ExynosCameraPPUniPluginHyperlapse's constructor is protected
     * to prevent new without ExynosCameraPPFactory::newPP()
     */
    friend class ExynosCameraPPFactory;

public:
    virtual ~ExynosCameraPPUniPluginHyperlapse();
    status_t    start(void);
    status_t    stop(bool suspendFlag);

protected:
    virtual status_t m_draw(ExynosCameraImage *srcImage,
                            ExynosCameraImage *dstImage,
                            ExynosCameraParameters *params);

private:
    void                m_init(void);
    status_t            m_UniPluginInit(void);
    status_t            process(ExynosCameraImage *srcImage,
                                    ExynosCameraImage *dstImage,
                                    ExynosCameraParameters *params);

    int                 m_refCount;

    int                 m_hyperlapse_Status;

    nsecs_t             m_hyperlapseTimeStamp;
    nsecs_t             m_hyperlapseTimeBTWFrames;

    UniPluginBufferData_t       m_hyperlapse_pluginData;
    UniPluginExtraBufferInfo_t  m_hyperlapse_pluginExtraBufferInfo;

    mutable Mutex               m_uniPluginLock;
};

#endif //EXYNOS_CAMERA_PP_UNIPLUGIN_Hyperlapse_H
