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
 * \file      ExynosCameraPPUniPluginBD.h
 * \brief     header file for ExynosCameraPPUniPluginBD
 * \author    Sangwoo, Park(sw5771.park@samsung.com)
 * \date      2016/10/05
 *
 * <b>Revision History: </b>
 * - 2016/10/05 : Sangwoo, Park(sw5771.park@samsung.com) \n
 *   Initial version
 */

#ifndef EXYNOS_CAMERA_PP_UNIPLUGIN_BD_H
#define EXYNOS_CAMERA_PP_UNIPLUGIN_BD_H

#include "ExynosCameraDefine.h"

#include "ExynosCameraPPUniPlugin.h"

using namespace android;

/*
 * Class ExynosCameraPPUniPluginBD
 */
class ExynosCameraPPUniPluginBD : public ExynosCameraPPUniPlugin
{
protected:
    ExynosCameraPPUniPluginBD()
    {
        m_init();
    }

    ExynosCameraPPUniPluginBD(
            int cameraId,
            ExynosCameraParameters *parameters,
            int nodeNum) : ExynosCameraPPUniPlugin(cameraId, parameters, nodeNum)
    {
        strncpy(m_name, "ExynosCameraPPUniPluginBD",  EXYNOS_CAMERA_NAME_STR_SIZE - 1);

        m_init();
    }

    /* ExynosCameraPPUniPluginBD's constructor is protected
     * to prevent new without ExynosCameraPPFactory::newPP()
     */
    friend class ExynosCameraPPFactory;

public:
    virtual ~ExynosCameraPPUniPluginBD();
    status_t    start(void);
    status_t    stop(bool suspendFlag = false);

protected:
    virtual status_t m_draw(ExynosCameraImage *srcImage,
                            ExynosCameraImage *dstImage);

private:
    void                m_init(void);
    status_t            m_UniPluginInit(void);
#ifdef SAMSUNG_BD
    UTstr               m_BDbuffer[MAX_BD_BUFF_NUM];
#endif
    int                 m_BDbufferIndex;

    int                 m_refCount;

    mutable Mutex       m_uniPluginLock;
protected:
};

#endif //EXYNOS_CAMERA_PP_UNIPLUGIN_BD_H
