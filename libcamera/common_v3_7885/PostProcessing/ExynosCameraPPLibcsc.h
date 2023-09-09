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
 * \file      ExynosCameraPPLibcsc.h
 * \brief     header file for ExynosCameraPPLibcsc
 * \author    Sangwoo, Park(sw5771.park@samsung.com)
 * \date      2016/10/05
 *
 * <b>Revision History: </b>
 * - 2016/10/05 : Sangwoo, Park(sw5771.park@samsung.com) \n
 *   Initial version
 */

#ifndef EXYNOS_CAMERA_PP_LIBCSC_H
#define EXYNOS_CAMERA_PP_LIBCSC_H

#include "ExynosCameraPP.h"
#include "csc.h"

using namespace android;

/*
 * Class ExynosCameraPPLibcsc
 */
class ExynosCameraPPLibcsc : public ExynosCameraPP
{
protected:
    ExynosCameraPPLibcsc()
    {
        m_init();
    }

    ExynosCameraPPLibcsc(int cameraId, int nodeNum) : ExynosCameraPP(cameraId, nodeNum)
    {
        m_init();
    }

    /* ExynosCameraPPLibcsc's constructor is protected
     * to prevent new without ExynosCameraPPFactory::newPP()
     */
    friend class ExynosCameraPPFactory;

public:
    virtual ~ExynosCameraPPLibcsc();

protected:
    virtual status_t m_create(void);
    virtual status_t m_destroy(void);
    virtual status_t m_draw(ExynosCameraImage srcImage, ExynosCameraImage dstImage);
    virtual void     m_setName(void);

private:
            void     m_init(void);

protected:
    void                 *m_csc;
    CSC_HW_PROPERTY_TYPE  m_property;
};

#endif //EXYNOS_CAMERA_PP_LIBCSC_H
