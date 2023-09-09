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

#ifndef EXYNOS_CAMERA_PLUGIN_H
#define EXYNOS_CAMERA_PLUGIN_H

#include <utils/threads.h>
#include <utils/RefBase.h>
#include <utils/Errors.h>
#include <utils/Mutex.h>
#include <utils/threads.h>
#include <utils/String8.h>
#include <cutils/properties.h>

#include "videodev2_exynos_media.h"
#include "ExynosCameraCommonDefine.h" /* just refer to CLOG */

#include "PlugInCommon.h"

#define PLUGIN_NAME_STR_SIZE 32

/*
 * This function must be declared in external lib!!
 * By this macro, PlugInFactory can get the plugIn Object to handle external lib
 */
#define DECLARE_CREATE_PLUGIN_SYMBOL(className) \
    extern "C" {                                \
        void *CREATE_PLUGIN(int cameraId, int pipeId, int vendor); \
    }                                                               \
    void *CREATE_PLUGIN(int cameraId, int pipeId, int vendor) {   \
        return (void *)(new className(cameraId, pipeId, vendor));   \
    }

#define PLUGIN_SYMBOL_NAME "CREATE_PLUGIN"

namespace android {

class ExynosCameraPlugIn;
typedef sp<ExynosCameraPlugIn>  ExynosCameraPlugInSP_sptr_t; /* single ptr */
typedef sp<ExynosCameraPlugIn>& ExynosCameraPlugInSP_dptr_t; /* double ptr */
typedef void *(*PlugInSymbol_t)(int, int, int);

/*
 * Class ExynosCameraPlugIn
 *
 * This is adjusted "Tempate method pattern"
 */
class ExynosCameraPlugIn : public RefBase {
public:
    enum state {
        PLUGIN_INIT,
        PLUGIN_CREATE,
        PLUGIN_SETUP,
        PLUGIN_START,
        PLUGIN_PROCESS,
        PLUGIN_STOP,
        PLUGIN_DESTROY,
        PLUGIN_DEINIT,
    };

public:
    ExynosCameraPlugIn()
    {
        init();
    }

    ExynosCameraPlugIn(int cameraId, int pipeId, int mode)
    {
        m_cameraId = cameraId;
        m_pipeId  = pipeId;
        m_mode = (PLUGIN::MODE)mode;

        init();
    }

    ExynosCameraPlugIn(int cameraId, int pipeId)
    {
        ExynosCameraPlugIn(cameraId, pipeId, PLUGIN::MODE::BASE);
    }

    virtual ~ExynosCameraPlugIn();

    /***********************************/
    /*  function                       */
    /***********************************/
    // user API
    virtual status_t init(void) final;
    virtual status_t deinit(void) final;
    virtual status_t create(void) final;
    virtual status_t destroy(void) final;
    virtual status_t setup(Map_t *map) final;
    virtual status_t start(void) final;
    virtual status_t stop(void) final;
    virtual status_t process(Map_t *map) final;
    virtual status_t setParameter(int key, void *data) final;
    virtual status_t getParameter(int key, void *data) final;
    virtual void     dump(void) final;
    virtual status_t query(Map_t *map) final;

    virtual int        getPipeId(void) final { return m_pipeId; };
    virtual enum state getState(void) final { return m_state; };

protected:
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

protected:
    // help function.


    /***********************************/
    /*  variables                      */
    /***********************************/
protected:
    static volatile int32_t initCount;

    // default variable
    Mutex   m_lock;
    int     m_cameraId;
    int     m_pipeId;
    char    m_name[PLUGIN_NAME_STR_SIZE];
    enum state m_state;
    PLUGIN::MODE m_mode;
};
}
#endif //EXYNOS_CAMERA_PLUGIN_H
