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
 * \file      ExynosCameraPPUniPluginSTR_Preview.h
 * \brief     header file for ExynosCameraPPUniPluginSTR_Preview
 * \author    Sangwoo, Park(sw5771.park@samsung.com)
 * \date      2016/10/05
 *
 * <b>Revision History: </b>
 * - 2016/10/05 : Sangwoo, Park(sw5771.park@samsung.com) \n
 *   Initial version
 */

#ifndef EXYNOS_CAMERA_PP_UNIPLUGIN_STR_PREVIEW_H
#define EXYNOS_CAMERA_PP_UNIPLUGIN_STR_PREVIEW_H

#include "ExynosCameraDefine.h"

#include "ExynosCameraPPUniPlugin.h"

using namespace android;

enum str_preview_status {
    STR_PREVIEW_DEINIT              = 0,
    STR_PREVIEW_IDLE,
    STR_PREVIEW_RUN,
};

/*
 * Class ExynosCameraPPUniPluginSTR_Preview
 */
class ExynosCameraPPUniPluginSTR_Preview : public ExynosCameraPPUniPlugin
{
protected:
    ExynosCameraPPUniPluginSTR_Preview()
    {
        m_init();
    }

    ExynosCameraPPUniPluginSTR_Preview(
            int cameraId,
            ExynosCameraParameters *parameters,
            int nodeNum) : ExynosCameraPPUniPlugin(cameraId, parameters, nodeNum)
    {
        strncpy(m_name, "ExynosCameraPPUniPluginSTR_Preview",  EXYNOS_CAMERA_NAME_STR_SIZE - 1);

        m_init();
    }

    /* ExynosCameraPPUniPluginSTR_Preview's constructor is protected
     * to prevent new without ExynosCameraPPFactory::newPP()
     */
    friend class ExynosCameraPPFactory;

public:
    virtual ~ExynosCameraPPUniPluginSTR_Preview();
    status_t    start(void);
    status_t    stop(bool suspendFlag = false);

protected:
    virtual status_t m_draw(ExynosCameraImage *srcImage,
                            ExynosCameraImage *dstImage);

private:
    void     m_init(void);

    int                             m_refCount;

    int                             m_strPreviewStatus;
    mutable Mutex                   m_uniPluginLock;
protected:
};

#endif //EXYNOS_CAMERA_PP_UNIPLUGIN_STR_PREVIEW_H
