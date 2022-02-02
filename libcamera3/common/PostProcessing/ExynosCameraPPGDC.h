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
 * \file      ExynosCameraPPGDC.h
 * \brief     header file for ExynosCameraPPGDC
 * \author    Sangwoo, Park(sw5771.park@samsung.com)
 * \date      2016/10/14
 *
 * <b>Revision History: </b>
 * - 2016/10/05 : Sangwoo, Park(sw5771.park@samsung.com) \n
 *   Initial version
 */

#ifndef EXYNOS_CAMERA_PP_GDC_H
#define EXYNOS_CAMERA_PP_GDC_H

#include "ExynosCameraPP.h"
#include "ExynosCameraNode.h"

#include "videodev2_exynos_camera.h"

using namespace android;

/*
 * Class ExynosCameraPPGDC
 */
class ExynosCameraPPGDC : public ExynosCameraPP
{
protected:
    ExynosCameraPPGDC()
    {
        m_init();
    }

    ExynosCameraPPGDC(
            int cameraId,
            ExynosCameraParameters *parameters,
            int nodeNum) : ExynosCameraPP(cameraId, parameters, nodeNum)
    {
        strncpy(m_name, "ExynosCameraPPGDC",  EXYNOS_CAMERA_NAME_STR_SIZE - 1);

        m_init();
    }

    /* ExynosCameraPPGDC's constructor is protected
     * to prevent new without ExynosCameraPPFactory::newPP()
     */
    friend class ExynosCameraPPFactory;

public:
    virtual ~ExynosCameraPPGDC();

protected:
    enum IMAGE_POS {
        IMAGE_POS_SRC = 0,
        IMAGE_POS_BCROP,
        IMAGE_POS_DST,
        IMAGE_POS_MAX,
    };

    virtual status_t m_create(void);
    virtual status_t m_destroy(void);
    virtual status_t m_draw(ExynosCameraImage *srcImage,
                            ExynosCameraImage *dstImage);

            status_t m_destroyNode(ExynosCameraNode *node);
            status_t m_resetNode(ExynosCameraNode *node);
            status_t m_setCtrl(ExynosCameraNode *node, ExynosCameraImage *image);
            status_t m_setFormat(ExynosCameraNode *node, ExynosCameraImage *image, enum IMAGE_POS nodeType);
            status_t m_start(ExynosCameraNode *node);
            status_t m_stop(ExynosCameraNode *node);
            status_t m_putBuffer(ExynosCameraNode *node, ExynosCameraImage *image);
            status_t m_getBuffer(ExynosCameraNode *node, ExynosCameraImage *image);
            bool     m_flagValidNode(int nodeNum);

private:
            void     m_init(void);

protected:
    ExynosCameraNode *m_node[IMAGE_POS_MAX];
    ExynosRect        m_sensorBcropRect;
};

#endif //EXYNOS_CAMERA_PP_GDC_H
