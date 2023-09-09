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
 * \file      ExynosCameraPlugIn.h
 * \brief     header file for ExynosCameraPlugIn
 * \author    Teahyung, Kim(tkon.kim@samsung.com)
 * \date      2017/07/17
 *
 * <b>Revision History: </b>
 * - 2017/07/17 : Teahyung, Kim(tkon.kim@samsung.com) \n
 *   Initial version
 */

#ifndef EXYNOS_CAMERA_PLUGIN_FAKE_FUSION_H
#define EXYNOS_CAMERA_PLUGIN_FAKE_FUSION_H

#include <utils/threads.h>
#include <utils/RefBase.h>
#include <utils/Errors.h>
#include <utils/Mutex.h>
#include <utils/threads.h>
#include <utils/String8.h>
#include <cutils/properties.h>
#include <cutils/atomic.h>

#include "videodev2_exynos_media.h"
#include "ExynosCameraCommonDefine.h" /* just refer to CLOG */

#include "ExynosCameraPlugIn.h"
#include "PlugInCommon.h"
#include "FakeFusion.h" /* external lib header */

namespace android {

/*
 * Class ExynosCameraPlugInFakeFusion
 *
 * This is adjusted "Tempate method pattern"
 */
class ExynosCameraPlugInFakeFusion : public ExynosCameraPlugIn {
public:
    ExynosCameraPlugInFakeFusion() : ExynosCameraPlugIn() {}
    ExynosCameraPlugInFakeFusion(int cameraId, int pipeId) : ExynosCameraPlugIn(cameraId, pipeId) {
        strncpy(m_name, "FakeFusionPlugIn", (PLUGIN_NAME_STR_SIZE - 1));
    };
    virtual ~ExynosCameraPlugInFakeFusion() { ALOGD("%s", __FUNCTION__); };

protected:
    /***********************************/
    /*  function                       */
    /***********************************/

protected:
    static volatile int32_t initCount;

    // inherit this function.
    virtual status_t m_init(void);
    virtual status_t m_deinit(void);
    virtual status_t m_create(void);
    virtual status_t m_destroy(void);
    virtual status_t m_setup(Map_t *map);
    virtual status_t m_process(Map_t *map);
    virtual status_t m_setParameter(int key, void *data);
    virtual status_t m_getParameter(int key, void *data);
    virtual void     m_dump(void);
    virtual status_t m_query(Map_t *map);
    virtual status_t m_start(void);
    virtual status_t m_stop(void);


    /***********************************/
    /*  variables                      */
    /***********************************/
private:
    FakeFusion *fusion;
};
}
#endif //EXYNOS_CAMERA_PLUGIN_FAKE_FUSION_H
