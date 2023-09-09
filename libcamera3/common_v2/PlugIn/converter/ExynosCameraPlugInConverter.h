/*
 * Copyright (C) 2014, Samsung Electronics Co. LTD
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

#ifndef EXYNOS_CAMERA_PLUGIN_CONVERTER_H__
#define EXYNOS_CAMERA_PLUGIN_CONVERTER_H__

#include <log/log.h>
#include <utils/RefBase.h>

#include "fimc-is-metadata.h"
#include "ExynosCameraCommonInclude.h"
#include "ExynosCameraParameters.h"
#include "ExynosCameraPlugIn.h"
#include "ExynosCameraFrame.h"

namespace android {

class ExynosCameraPlugInConverter;
typedef sp<ExynosCameraFrame>  ExynosCameraFrameSP_sptr_t;
typedef sp<ExynosCameraPlugInConverter>  ExynosCameraPlugInConverterSP_sptr_t; /* single ptr */
typedef sp<ExynosCameraPlugInConverter>& ExynosCameraPlugInConverterSP_dptr_t; /* double ptr */

class ExynosCameraPlugInConverter : public virtual RefBase {
public:
    ExynosCameraPlugInConverter()
    {
        init();
    }

    ExynosCameraPlugInConverter(int cameraId, int pipeId)
    {
        m_cameraId = cameraId;
        m_pipeId  = pipeId;

        init();
    }

    virtual ~ExynosCameraPlugInConverter();

    virtual status_t init(void) final;
    virtual status_t deinit(void) final;
    virtual status_t create(Map_t *map) final;
    virtual status_t setup(Map_t *map) final;
    virtual status_t make(Map_t *map) final;

protected:
    // inherit this function.
    virtual status_t m_init(void);
    virtual status_t m_deinit(void);
    virtual status_t m_create(Map_t *map);
    virtual status_t m_setup(Map_t *map);
    virtual status_t m_make(Map_t *map);

protected:
    // help function.
    status_t m_convertBufferInfoFromFrame(ExynosCameraFrameSP_sptr_t frame, Map_t *map);

protected:
    // default variable
    int     m_cameraId;
    int     m_pipeId;
    char    m_name[EXYNOS_CAMERA_NAME_STR_SIZE];

    /* query information */
    Data_int32_t          m_version;
    Pointer_const_char_t  m_libName;
    Pointer_const_char_t  m_buildDate;
    Pointer_const_char_t  m_buildTime;
    Data_int32_t          m_curSrcBufCnt;
    Data_int32_t          m_curDstBufCnt;
    Data_int32_t          m_maxSrcBufCnt;
    Data_int32_t          m_maxDstBufCnt;

    // for default converting to send the plugIn
    char   *m_srcBuf[PLUGIN_MAX_BUF][PLUGIN_MAX_PLANE];
    int     m_srcBufPlaneCnt[PLUGIN_MAX_BUF];
    int     m_srcBufIndex[PLUGIN_MAX_BUF];
    int     m_srcBufSize[PLUGIN_MAX_BUF][PLUGIN_MAX_PLANE];
    int     m_srcBufFd[PLUGIN_MAX_BUF][PLUGIN_MAX_PLANE];
    int     m_srcBufRect[PLUGIN_MAX_BUF][6];
    int     m_srcBufWStride[PLUGIN_MAX_BUF];
    int     m_srcBufHStride[PLUGIN_MAX_BUF];
    int     m_srcBufV4L2Format[PLUGIN_MAX_BUF];
    int     m_srcBufHALFormat[PLUGIN_MAX_BUF];
    int     m_srcBufRotation[PLUGIN_MAX_BUF];
    int     m_srcBufFlipH[PLUGIN_MAX_BUF];
    int     m_srcBufFlipV[PLUGIN_MAX_BUF];

    char   *m_dstBuf[PLUGIN_MAX_BUF][PLUGIN_MAX_PLANE];
    int     m_dstBufPlaneCnt[PLUGIN_MAX_BUF];
    int     m_dstBufIndex[PLUGIN_MAX_BUF];
    int     m_dstBufSize[PLUGIN_MAX_BUF][PLUGIN_MAX_PLANE];
    int     m_dstBufFd[PLUGIN_MAX_BUF][PLUGIN_MAX_PLANE];
    int     m_dstBufRect[PLUGIN_MAX_BUF][6];
    int     m_dstBufWStride[PLUGIN_MAX_BUF];
    int     m_dstBufHStride[PLUGIN_MAX_BUF];
    int     m_dstBufV4L2Format[PLUGIN_MAX_BUF];
    int     m_dstBufHALFormat[PLUGIN_MAX_BUF];
    int     m_dstBufRotation[PLUGIN_MAX_BUF];
    int     m_dstBufFlipH[PLUGIN_MAX_BUF];
    int     m_dstBufFlipV[PLUGIN_MAX_BUF];
};
}; /* namespace android */
#endif
