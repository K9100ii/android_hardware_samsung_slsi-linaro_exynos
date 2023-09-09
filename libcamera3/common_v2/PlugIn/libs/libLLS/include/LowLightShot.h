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
 * \file      LowLightShot.h
 * \brief     header file for LowLightShot
 * \author    Sicheon, Kim(sicheon.oh)
 * \date      2017/07/19
 *
 * <b>Revision History: </b>
 * - 2017/08/17 : sicheon, Oh(sicheon.oh@samsung.com) \n
 *   Initial version
 */

#ifndef LOW_LIGHT_SHOT_H
#define LOW_LIGHT_SHOT_H

#include <utils/Log.h>
#include <utils/threads.h>
#include <utils/Timers.h>
#include "string.h"

#include "exynos_format.h"
#include "exynos_v4l2.h"
#include "PlugInCommon.h"
#include "ExynosCameraPlugInUtils.h"
#include "videodev2_exynos_media.h"

using namespace android;

#define LLS_DEBUG
#define LLS_MAX_COUNT 7

#ifdef LLS_DEBUG
#define LLS_LOGD ALOGD
#else
#define LLS_LOGD ALOGV
#endif

#define LLS_GDC_VIDEO_NAME "/dev/video155"
#define LLS_GDC_VIDEO_TYPE_MAX (2)
#define LLS_GDC_VIDEO_FORMAT (V4L2_PIX_FMT_NV12M_S10B)
//#define LLS_GDC_VIDEO_FORMAT (V4L2_PIX_FMT_NV12M_P010)


class LowLightShot;
typedef ExynosCameraPlugInUtilsThread<LowLightShot> LowLightShotThread;

//! LowLightShot
/*!
 * \ingroup ExynosCamera
 */
class LowLightShot
{
public:
    //! Constructor
    LowLightShot();
    //! Destructor
    virtual ~LowLightShot();

    //! create
    virtual status_t create(void);

    //! destroy
    virtual status_t  destroy(void);

    //! execute
    virtual status_t  setup(Map_t *map);

    //! execute
    virtual status_t  execute(Map_t *map);

    //! init
    virtual status_t  init(Map_t *map);

    //! query
    virtual status_t  query(Map_t *map);

protected:
    status_t    setupGDC();
    status_t    runGDC(PlugInBuffer_t *srcBuf, PlugInBuffer_t *dstBuf);
    status_t    closeGDC();

protected:
    int     m_processCount;

    /* to calculate buffer size */
    int     m_bayerV4L2Format;
    int     m_maxBcropX;
    int     m_maxBcropY;
    int     m_maxBcropW;
    int     m_maxBcropH;
    int     m_maxBcropFullW;
    int     m_maxBcropFullH;

    /* Thread to alloc all memory */
    sp<LowLightShotThread>  m_gdcPrepareThread;
    bool                    m_gdcPrepareThreadFunc(void);

    /* Buffer */
    ExynosCameraPlugInUtilsIonAllocator *allocator;
    PlugInBuffer_t m_mergeBuf;    //1plane
    PlugInBuffer_t m_gdcSrcBuf[2]; //2plane
    PlugInBuffer_t m_gdcDstBuf[2]; //2plane

    /* GDC v4l2 */
    int                         m_gdcFd;
    struct v4l2_format          m_v4l2Format;
    struct v4l2_requestbuffers  m_v4l2ReqBufs;
};

#endif //LOW_LIGHT_SHOT_H
