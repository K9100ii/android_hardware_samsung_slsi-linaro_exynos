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
 * \file      ExynosCameraFactoryPlugIn.h
 * \brief     header file for ExynosCameraFactoryPlugIn
 * \author    Teahyung, Kim(tkon.kim@samsung.com)
 * \date      2017/07/17
 *
 * <b>Revision History: </b>
 * - 2017/07/17 : Teahyung, Kim(tkon.kim@samsung.com) \n
 *   Initial version
 */

#ifndef EXYNOS_CAMERA_FACTORY_PLUGIN_H
#define EXYNOS_CAMERA_FACTORY_PLUGIN_H

#include "PlugInCommon.h"
#include "ExynosCameraSingleton.h"
#include "ExynosCameraPlugInConverter.h"

namespace android {

/*
 * Class ExynosCameraFactoryPlugIn
 *
 * This is adjusted "Abstraction factory pattern"
 */
class ExynosCameraFactoryPlugIn
{
protected:
    friend class ExynosCameraSingleton<ExynosCameraFactoryPlugIn>;

    ExynosCameraFactoryPlugIn();
    virtual ~ExynosCameraFactoryPlugIn(){}

public:
    /*
     * Use this API to get the real object.
     */
    status_t create(int cameraId, int pipeId, ExynosCameraPlugInSP_dptr_t plugIn, ExynosCameraPlugInConverterSP_dptr_t converter, PLUGIN::MODE mode);
    status_t destroy(int cameraId, ExynosCameraPlugInSP_dptr_t plugIn, ExynosCameraPlugInConverterSP_dptr_t converter);
    void     dump(int cameraId);

protected:
    int      m_findPlugInIndexByPipeId(int cameraId, int pipeId);

private:
    Mutex                    m_lock;
    int                      m_plugInTotalCount;
    char                     m_name[EXYNOS_CAMERA_NAME_STR_SIZE];
    std::map<int, void *>    m_mapLibHandle;
    std::map<int, int>       m_mapPlugInRefCount;
};
}
#endif //EXYNOS_CAMERA_PLUGIN_FACTORY_H
