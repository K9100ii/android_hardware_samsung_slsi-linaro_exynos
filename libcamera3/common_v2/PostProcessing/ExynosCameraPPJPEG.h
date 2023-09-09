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
 * \file      ExynosCameraPPJPEG.h
 * \brief     header file for ExynosCameraPPJPEG
 * \author    Sangwoo, Park(sw5771.park@samsung.com)
 * \date      2016/10/05
 *
 * <b>Revision History: </b>
 * - 2016/10/05 : Sangwoo, Park(sw5771.park@samsung.com) \n
 *   Initial version
 */

#ifndef EXYNOS_CAMERA_PP_JPEG_H
#define EXYNOS_CAMERA_PP_JPEG_H

#include "ExynosCameraPP.h"
#include "ExynosJpegEncoderForCamera.h"

using namespace android;

/*
 * Class ExynosCameraPPJPEG
 */
class ExynosCameraPPJPEG : public ExynosCameraPP
{
protected:
    ExynosCameraPPJPEG()
    {
        m_init();
    }

    ExynosCameraPPJPEG(
            int cameraId,
            ExynosCameraConfigurations *configurations,
            ExynosCameraParameters *parameters,
            int nodeNum) : ExynosCameraPP(cameraId, configurations, parameters, nodeNum)
    {
        strncpy(m_name, "ExynosCameraPPJPEG",  EXYNOS_CAMERA_NAME_STR_SIZE - 1);

        m_init();
    }

    /* ExynosCameraPPJPEG's constructor is protected
     * to prevent new without ExynosCameraPPFactory::newPP()
     */
    friend class ExynosCameraPPFactory;

public:
    virtual ~ExynosCameraPPJPEG();

protected:
    virtual status_t m_draw(ExynosCameraImage *srcImage,
                            ExynosCameraImage *dstImage,
                            ExynosCameraParameters *params);

private:
            void     m_init(void);

protected:
    ExynosJpegEncoderForCamera  m_jpegEnc;
#ifdef SAMSUNG_JQ
    unsigned char               m_qtable[128];
#endif
};

#endif //EXYNOS_CAMERA_PP_JPEG_H
