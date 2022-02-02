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
 * \file      arcsoftNightShot.h
 * \brief     header file for arcsoftNightShot
 * \author    suyounglee(suyoung80.lee@samsung.com)
 * \date      2015/11/25
 *
 */

#ifndef EXYNOS_CAMERA_ARCSOFT_NIGHTSHOT_H__
#define EXYNOS_CAMERA_ARCSOFT_NIGHTSHOT_H__

#include <utils/RefBase.h>
#include "../inc/ammem.h"
#include "../inc/merror.h"
#include "../inc/asvloffscreen.h"
#include "../inc/arcsoft_night_shot.h"
#include "../../../ExynosCameraSFLInterface.h"
#include <utils/Errors.h>

namespace android {

/* #define EXYNOS_CAMERA_NIGHTSHOT_TRACE */
#ifdef EXYNOS_CAMERA_NIGHTSHOT_TRACE
#define EXYNOS_CAMERA_NIGHTSHOT_TRACE_IN()   CLOGD("DEBUG(%s[%d]):IN.." , __FUNCTION__, __LINE__)
#define EXYNOS_CAMERA_NIGHTSHOT_TRACE_OUT()  CLOGD("DEBUG(%s[%d]):OUT..", __FUNCTION__, __LINE__)
#else
#define EXYNOS_CAMERA_NIGHTSHOT_TRACE_IN()   ((void *)0)
#define EXYNOS_CAMERA_NIGHTSHOT_TRACE_OUT()  ((void *)0)
#endif

#define NIGHTSHOT_BUFFER_SIZE              (50*1020*1024)
#define NIGHTSHOT_CAPTURE_INPUT_IMAGES_MAX (MAX_INPUT_IMAGES)

struct night_runInfo {
    ANS_INPUTINFO mSrcInput;
    ASVLOFFSCREEN mSrcImgs[NIGHTSHOT_CAPTURE_INPUT_IMAGES_MAX];
    ASVLOFFSCREEN mDstImg;
    ANS_PARAM mParam;
    list<ExynosCameraSFLInfoType> mRunSflList;
};

struct night_setupInfo {
    list< ExynosCameraSFLInfoType > inBufs;
    list< ExynosCameraSFLInfoType > outBufs;
    uint32_t  curBufCount[SFL::TYPE_MAX][SFL::POS_MAX];
    uint32_t  maxBufCount[SFL::TYPE_MAX][SFL::POS_MAX];

    uint32_t curProgress[SFL::TYPE_MAX][SFL::POS_MAX];
    uint32_t maxProgress[SFL::TYPE_MAX][SFL::POS_MAX];

    uint32_t adaptiveMaxBufCount[SFL::TYPE_MAX][SFL::POS_MAX];
};

class ARCSoftNightShot: public ExynosCameraSFLInterface {
public:
    ARCSoftNightShot(const char* name, int cameraId);
    virtual ~ARCSoftNightShot();

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
    status_t m_reset();
    void     m_setEnable(bool enable);
    bool     m_getEnable();
    void     m_setRunEnable(bool enable);
    bool     m_getRunEnable();


    status_t m_addSflBuffer(struct CommandInfo *cmdinfo, struct night_setupInfo *setupInfo, struct SFLBuffer *buffer);
    status_t m_addRunBuffer(ASVLOFFSCREEN* outBufferlist, list<ExynosCameraSFLInfoType> *runSflList, int outBufCount, list<ExynosCameraSFLInfoType> *inBufferlist);
    status_t m_processImage(list<ExynosCameraSFLInfoType> *outBufferlist);
    status_t m_dump();
    status_t m_convertSetupInfoToRunInfo(struct night_setupInfo *setupInfo, struct night_runInfo *runInfo);
    status_t m_setParam(uint32_t *param, void* arg);
    status_t m_getParam(uint32_t *param, void* arg);

private:
    mutable Mutex m_processRunLock;
    mutable Mutex m_setupInfoLock;

    MLong       m_iMemSize;
    MVoid      *m_ipMem;
    MHandle     m_iMemMgr;
    MHandle     m_enhancer;
    struct night_runInfo    m_runInfo;
    struct night_setupInfo  m_setupInfo;
};

}; //namespace android

#endif
