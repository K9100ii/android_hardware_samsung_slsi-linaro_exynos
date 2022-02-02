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
 * \file      arcsoftAntishake.h
 * \brief     header file for ARCSoftAntiShake
 * \author    suyounglee(suyoung80.lee@samsung.com)
 * \date      2015/11/25
 *
 */

#ifndef EXYNOS_CAMERA_ARCSOFT_ANTISHAKE_H__
#define EXYNOS_CAMERA_ARCSOFT_ANTISHAKE_H__

#include <utils/RefBase.h>
#include "../inc/ammem.h"
#include "../inc/merror.h"
#include "../inc/asvloffscreen.h"
#include "../inc/arcsoft_antishaking.h"
#include "../../../ExynosCameraSFLInterface.h"
#include <utils/Errors.h>
#include "ExynosCameraSensorInfo.h"

namespace android {

/* #define EXYNOS_CAMERA_ANTISHAKE_TRACE */
#ifdef EXYNOS_CAMERA_ANTISHAKE_TRACE
#define EXYNOS_CAMERA_ANTISHAKE_TRACE_IN()   CLOGD("DEBUG(%s[%d]):IN.." , __FUNCTION__, __LINE__)
#define EXYNOS_CAMERA_ANTISHAKE_TRACE_OUT()  CLOGD("DEBUG(%s[%d]):OUT..", __FUNCTION__, __LINE__)
#else
#define EXYNOS_CAMERA_ANTISHAKE_TRACE_IN()   ((void *)0)
#define EXYNOS_CAMERA_ANTISHAKE_TRACE_OUT()  ((void *)0)
#endif

#define ANTISHAKE_BUFFER_SIZE              (60*1020*1024)
#define ANTISHAKE_PREVIEW_INPUT_IMAGES_MAX (1)
#define ANTISHAKE_PARAM_MAX                (2);

struct antishake_runInfo {
    ASSF_INPUTINFO previewInput;
    ASVLOFFSCREEN  previewImgs[ANTISHAKE_PREVIEW_INPUT_IMAGES_MAX];
    ASVLOFFSCREEN  srcCaptureImg;
    ASVLOFFSCREEN  dstCaptureImg;
    ASSF_PARAM param;
    CAMERA_PARAM cameraParam;
    list<ExynosCameraSFLInfoType> mRunSflList;
    struct camera2_shot_ext metaInfo;
};

struct antishake_setupInfo {
    list< ExynosCameraSFLInfoType > previewInBufs;
    list< ExynosCameraSFLInfoType > captureInBufs;
    list< ExynosCameraSFLInfoType > captureOutBufs;
    uint32_t  curBufCount[SFL::TYPE_MAX][SFL::POS_MAX];
    uint32_t  maxBufCount[SFL::TYPE_MAX][SFL::POS_MAX];

    uint32_t curProgress[SFL::TYPE_MAX][SFL::POS_MAX];
    uint32_t maxProgress[SFL::TYPE_MAX][SFL::POS_MAX];

    uint32_t adaptiveMaxBufCount[SFL::TYPE_MAX][SFL::POS_MAX];
    struct camera2_shot_ext adaptiveMetaInfo;
    uint32_t bestFrameInfo[SFL::TYPE_MAX][SFL::POS_MAX];
};

class ARCSoftAntiShake: public ExynosCameraSFLInterface {
public:
    ARCSoftAntiShake(const char* name, int cameraId);
    virtual ~ARCSoftAntiShake();

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


    status_t m_addSflBuffer(struct CommandInfo *cmdinfo, struct antishake_setupInfo *setupInfo, struct SFLBuffer *buffer);
    status_t m_addRunBuffer(ASVLOFFSCREEN* outBufferlist, list<ExynosCameraSFLInfoType> *runSflList, int outBufCount, list<ExynosCameraSFLInfoType> *inBufferlist);
    status_t m_processImage(list<ExynosCameraSFLInfoType> *outBufferlist);
    status_t m_dump();
    status_t m_convertSetupInfoToRunInfo(struct antishake_setupInfo *setupInfo, struct antishake_runInfo *runInfo);
    status_t m_updateRunParameter(struct antishake_setupInfo *setupInfo, struct antishake_runInfo *runInfo);
    status_t m_setParam(uint32_t *param, void* arg);
    status_t m_getParam(uint32_t *param, void* arg);

    status_t m_setConfigData(struct CommandInfo *cmdinfo, void* arg);
    status_t m_setRunData(struct CommandInfo *cmdinfo, void* arg);
    status_t m_getAdjustDataInfo(struct CommandInfo *cmdinfo, void* arg);

private:
    mutable Mutex m_processRunLock;
    mutable Mutex m_setupInfoLock;

    MLong       m_iMemSize;
    MVoid      *m_ipMem;
    MLong       m_previewIMemSize;
    MVoid      *m_previewIpMem[ANTISHAKE_PREVIEW_INPUT_IMAGES_MAX];
    MHandle     m_iMemMgr;
    MHandle     m_enhancer;
    struct antishake_runInfo    m_runInfo;
    struct antishake_setupInfo  m_setupInfo;
    struct ExynosSensorInfoBase m_sensorInfo;
};

}; //namespace android

#endif
