/*
 * Copyright 2015, Samsung Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed toggle an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*!
 * \file      ARCSoftFlawless.h
 * \brief     header file for ARCSoftFlawless
 * \author    suyounglee(suyoung80.lee@samsung.com)
 * \date      2015/11/25
 *
 */

#ifndef EXYNOS_CAMERA_ARCSOFT_FLAWLESS_H__
#define EXYNOS_CAMERA_ARCSOFT_FLAWLESS_H__

#include <utils/RefBase.h>
#include "../inc/ammem.h"
#include "../inc/merror.h"
#include "../inc/asvloffscreen.h"
#include "../inc/arcsoft_detect_engine.h"
#include "../../../ExynosCameraSFLInterface.h"
#include <utils/Errors.h>

namespace android {

/* #define EXYNOS_CAMERA_FLAWLESS_TRACE */
#ifdef EXYNOS_CAMERA_FLAWLESS_TRACE
#define EXYNOS_CAMERA_FLAWLESS_TRACE_IN()   CLOGD("DEBUG(%s[%d]):IN.." , __FUNCTION__, __LINE__)
#define EXYNOS_CAMERA_FLAWLESS_TRACE_OUT()  CLOGD("DEBUG(%s[%d]):OUT..", __FUNCTION__, __LINE__)
#else
#define EXYNOS_CAMERA_FLAWLESS_TRACE_IN()   ((void *)0)
#define EXYNOS_CAMERA_FLAWLESS_TRACE_OUT()  ((void *)0)
#endif

struct flawless_runInfo {
    ASVLOFFSCREEN      previewInput;
    ADE_PROCESS_RESULT previewOutput;
    ASVLOFFSCREEN      captureInput;
    ASVLOFFSCREEN      captureOutput;
    ADE_PARAMETERS     parameter;
    ADE_PROPERTY       property;
    list< ExynosCameraSFLInfoType > mRunSflList[SFL::TYPE_MAX];
};

struct flawless_setupInfo {
    list< ExynosCameraSFLInfoType  > previewInBufs;
    list< ExynosCameraSFLInfoType  > previewOutBufs;
    list< ExynosCameraSFLInfoType  > captureInBufs;
    list< ExynosCameraSFLInfoType  > captureOutBufs;
    uint32_t curBufCount[SFL::TYPE_MAX][SFL::POS_MAX];
    uint32_t maxBufCount[SFL::TYPE_MAX][SFL::POS_MAX];

    uint32_t curProgress[SFL::TYPE_MAX][SFL::POS_MAX];
    uint32_t maxProgress[SFL::TYPE_MAX][SFL::POS_MAX];
    ADE_PARAMETERS parameter;
    ADE_PROPERTY   property;
};

class ARCSoftFlawless: public ExynosCameraSFLInterface {
public:
    ARCSoftFlawless(const char* name, int cameraId);
    virtual ~ARCSoftFlawless();

public:
    SFLIBRARY_MGR::SFLType getType();
    status_t init();
    status_t deinit();
    status_t registerCallbacks();
    status_t prepare(); /* adaptive MAX buffer count migration to MAX buffer */
    status_t setMetaInfo(void* param);
    status_t processCommand(struct CommandInfo *cmdinfo, void* arg);
    void     setEnable(bool enable);
    bool     getEnable();
    void     setRunEnable(bool enable);
    bool     getRunEnable();
    void     reset();

private:
    status_t m_init();
    status_t m_deinit();
    status_t m_prepare(); /* adaptive MAX buffer count migration to MAX buffer */
    status_t m_setMetaInfo(void* param);
    status_t m_reset(struct CommandInfo *cmdinfo);
    void     m_setEnable(bool enable);
    bool     m_getEnable();
    void     m_setRunEnable(bool enable);
    bool     m_getRunEnable();


    status_t m_addSflBuffer(struct CommandInfo *cmdinfo, struct flawless_setupInfo *setupInfo, struct SFLBuffer *buffer);
    status_t m_addRunBuffer(ASVLOFFSCREEN* outBufferlist, list< ExynosCameraSFLInfoType > *runSflList, int outBufCount, list< ExynosCameraSFLInfoType > *inBufferlist);
    status_t m_processImage(struct CommandInfo *cmdinfo, list< ExynosCameraSFLInfoType > *outBufferlist);
    status_t m_dump(struct CommandInfo *cmdinfo);
    status_t m_convertSetupInfoToRunInfo(struct CommandInfo *cmdinfo, struct flawless_setupInfo *setupInfo, struct flawless_runInfo *runInfo);
    status_t m_updateRunParameter(struct flawless_setupInfo *setupInfo, struct flawless_runInfo *runInfo);
    status_t m_setParam(uint32_t *param, void* arg);
    status_t m_getParam(uint32_t *param, void* arg);
    status_t m_setConfigData(struct CommandInfo *cmdinfo, void* arg);

private:
    mutable Mutex m_processRunLock[SFL::TYPE_MAX];
    mutable Mutex m_setupInfoLock;

    MHandle     m_enhancer;
    struct flawless_runInfo    m_runInfo;
    struct flawless_setupInfo  m_setupInfo;
};

}; //namespace android

#endif
