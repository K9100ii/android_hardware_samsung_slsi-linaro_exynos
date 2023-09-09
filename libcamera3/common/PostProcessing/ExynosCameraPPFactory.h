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
 * \file      ExynosCameraPPFactory.h
 * \brief     header file for ExynosCameraPPFactory
 * \author    Sangwoo, Park(sw5771.park@samsung.com)
 * \date      2016/10/05
 *
 * <b>Revision History: </b>
 * - 2016/10/05 : Sangwoo, Park(sw5771.park@samsung.com) \n
 *   Initial version
 */

#ifndef EXYNOS_CAMERA_PP_FACTORY_H
#define EXYNOS_CAMERA_PP_FACTORY_H

#include "ExynosCameraConfig.h"

#include "ExynosCameraPP.h"
#include "ExynosCameraPPGDC.h"
#include "ExynosCameraPPLibcsc.h"
/* #include "ExynosCameraPPLibacryl.h" */
#include "ExynosCameraPPJPEG.h"
#ifdef SAMSUNG_TN_FEATURE
#include "ExynosCameraPPUniPlugin.h"
#endif

using namespace android;

/*
 * Class ExynosCameraPPFactory
 *
 * This is adjusted "Abstraction factory pattern"
 */
class ExynosCameraPPFactory
{
protected:
    ExynosCameraPPFactory(){}
    virtual ~ExynosCameraPPFactory(){}

    /*
     * ExynosCameraPPLibcsc's constructor is protected
     * to prevent new without ExynosCameraPPFactory::newPP()
     */

public:
    /*
     * Use this API to get the real object.
     */
    static ExynosCameraPP *newPP(
            int cameraId,
            ExynosCameraParameters *parameters,
            int nodeNum);

    static ExynosCameraPP *newPP(
            int cameraId,
            ExynosCameraParameters *parameters,
            int nodeNum,
            int scenario);
};

#endif //EXYNOS_CAMERA_PP_FACTORY_H
