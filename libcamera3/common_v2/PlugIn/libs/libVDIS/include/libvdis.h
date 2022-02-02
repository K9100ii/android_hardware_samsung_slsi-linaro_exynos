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
 * \file      VDIS.h
 * \brief     header file for VDIS
 * \author    Sicheon, Kim(sicheon.oh)
 * \date      2017/07/19
 *
 * <b>Revision History: </b>
 * - 2017/08/17 : sicheon, Oh(sicheon.oh@samsung.com) \n
 *   Initial version
 */

#ifndef VDIS_H
#define VDIS_H

#include <utils/Log.h>
#include <utils/threads.h>
#include <utils/Timers.h>
#include "string.h"

#include "exynos_format.h"
#include "PlugInCommon.h"
#include "ExynosCameraPlugInUtils.h"

using namespace android;

#define VDIS_DEBUG
#define VDIS_PROCESSTIME_STANDARD (34000)

#ifdef VDIS_DEBUG
#define VDIS_LOGD ALOGD
#else
#define VDIS_LOGD ALOGV
#endif

//! vdis
/*!
 * \ingroup ExynosCamera
 */
class VDIS 
{
public:
    //! Constructor
    VDIS();
    //! Destructor
    virtual ~VDIS();

    //! create
    virtual status_t create(void);

    //! destroy
    virtual status_t  destroy(void);

    //! execute
    virtual status_t  execute(Map_t *map);

    //! init
    virtual status_t  init(Map_t *map);

    //! setup
    virtual status_t  setup(Map_t *map);

    //! query
    virtual status_t  query(Map_t *map);

    virtual status_t  start(void);

    virtual status_t  stop(void);

protected:
#if 0
    int                       m_emulationProcessTime;
    float                     m_emulationCopyRatio;
#endif
};

#endif //VDIS_H
