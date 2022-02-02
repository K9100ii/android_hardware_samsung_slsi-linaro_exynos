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
 * \file      FakeFusion.h
 * \brief     header file for FakeFusion
 * \author    Teahyung, Kim(tkon.kim.com)
 * \date      2017/07/19
 *
 * <b>Revision History: </b>
 * - 2017/07/19 : Teahyung, Kim(tkon.kim@samsung.com) \n
 *   Initial version
 */

#ifndef FAKE_FUSION_H
#define FAKE_FUSION_H

#include <utils/Log.h>
#include <utils/threads.h>
#include <utils/Timers.h>
#include "string.h"

#include "exynos_format.h"
#include "PlugInCommon.h"

using namespace android;

#define FAKE_FUSION_DEBUG
#define FUSION_PROCESSTIME_STANDARD (34000)

#ifdef FAKE_FUSION_DEBUG
#define FAKE_FUSION_LOGD ALOGD
#else
#define FAKE_FUSION_LOGD ALOGV
#endif

//! FakeFusion
/*!
 * \ingroup ExynosCamera
 */
class FakeFusion
{
public:
    //! Constructor
    FakeFusion();
    //! Destructor
    virtual ~FakeFusion();

    //! create
    virtual status_t create(void);

    //! destroy
    virtual status_t  destroy(void);

    //! execute
    virtual status_t  execute(Map_t *map);

    //! init
    virtual status_t  init(Map_t *map);

    //! query
    virtual status_t  query(Map_t *map);

protected:
    void              m_copyMetaData(char *srcMetaAddr, int srcMetaSize, char *dstMetaAddr, int dstMetaSize);

protected:
    int                       m_emulationProcessTime;
    float                     m_emulationCopyRatio;
};

#endif //FAKE_FUSION_H
