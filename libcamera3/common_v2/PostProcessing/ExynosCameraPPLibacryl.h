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
 * \file      ExynosCameraPPLibacryl.h
 * \brief     header file for ExynosCameraPPLibacryl
 * \author    Sangwoo, Park(sw5771.park@samsung.com)
 * \date      2016/10/05
 *
 * <b>Revision History: </b>
 * - 2016/10/05 : Sangwoo, Park(sw5771.park@samsung.com) \n
 *   Initial version
 */

#ifndef EXYNOS_CAMERA_PP_LIBACRYL_H
#define EXYNOS_CAMERA_PP_LIBACRYL_H

#include "ExynosCameraPP.h"
#include "acryl.h"

using namespace android;

/*
 * Class ExynosCameraPPLibacryl
 */
class ExynosCameraPPLibacryl : public ExynosCameraPP
{
protected:
    ExynosCameraPPLibacryl()
    {
        m_init();
    }

    ExynosCameraPPLibacryl(
            int cameraId,
            ExynosCameraConfigurations *configurations,
            ExynosCameraParameters *parameters,
            int nodeNum) : ExynosCameraPP(cameraId, configurations, parameters, nodeNum)
    {
        strncpy(m_name, "ExynosCameraPPLibacryl",  EXYNOS_CAMERA_NAME_STR_SIZE - 1);

        m_init();
        // we think nodeNum = 222 is "/dev/fimg2d"
    }

    /* ExynosCameraPPLibacryl's constructor is protected
     * to prevent new without ExynosCameraPPFactory::newPP()
     */
    friend class ExynosCameraPPFactory;

public:
    virtual ~ExynosCameraPPLibacryl();

protected:
    virtual status_t m_create(void);
    virtual status_t m_destroy(void);
    virtual status_t m_draw(ExynosCameraImage *srcImage,
                            ExynosCameraImage *dstImage,
                            ExynosCameraParameters *params);

private:
            void     m_init(void);

protected:
    Acrylic         *m_acylic;
    AcrylicLayer    *m_layer;
};

#endif //EXYNOS_CAMERA_PP_LIBACRYL_H
