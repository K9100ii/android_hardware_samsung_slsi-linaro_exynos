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
 * \file      arcsoftNightShot.cpp
 * \brief     source file for arcsoftNightShot
 * \author    suyounglee(suyoung80.lee@samsung.com)
 * \date      2015/11/25gi
 *
 */

/* #define LOG_NDEBUG 0 */
#define LOG_TAG "ARCSoftNightShot"

#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <android/log.h>
#include <stdlib.h>

#include "arcsoftNightShot.h"
#include "fimc-is-metadata.h"

namespace android {

ARCSoftNightShot::ARCSoftNightShot(const char* name, int cameraId):ExynosCameraSFLInterface(name, cameraId)
{
    EXYNOS_CAMERA_NIGHTSHOT_TRACE_IN();

    /* display library version */
    ANS_Version ver;
    ANS_GetVersion(&ver);
    CLOGD("DEBUG(%s[%d]): ASMF_Version Version:%s", __FUNCTION__, __LINE__, ver.Version);
    CLOGD("DEBUG(%s[%d]): ASMF_Version CopyRight:%s", __FUNCTION__, __LINE__, ver.CopyRight);
    CLOGD("DEBUG(%s[%d]): ASMF_Version BuildDate:%s", __FUNCTION__, __LINE__, ver.BuildDate);
    CLOGD("DEBUG(%s[%d]): ASMF_Version lBuild:%d", __FUNCTION__, __LINE__, ver.lBuild);
    CLOGD("DEBUG(%s[%d]): ASMF_Version lCodebase:%d", __FUNCTION__, __LINE__, ver.lCodebase);
    CLOGD("DEBUG(%s[%d]): ASMF_Version lMajor:%d", __FUNCTION__, __LINE__, ver.lMajor);
    CLOGD("DEBUG(%s[%d]): ASMF_Version lMinor:%d", __FUNCTION__, __LINE__, ver.lMinor);

    /* 1. initalize the member */
    m_iMemSize = NIGHTSHOT_BUFFER_SIZE;
    m_ipMem = MNull;
    m_iMemMgr = MNull;
    m_enhancer = MNull;

    m_reset();

    EXYNOS_CAMERA_NIGHTSHOT_TRACE_OUT();
}

ARCSoftNightShot::~ARCSoftNightShot()
{
    EXYNOS_CAMERA_NIGHTSHOT_TRACE_IN();
    status_t ret = NO_ERROR;

    ret = m_reset();
    if (ret != NO_ERROR){
        CLOGE("DEBUG(%s[%d]): m_reset failed = %d",__FUNCTION__, __LINE__, ret);
    }

    ret = m_deinit();
    if (ret != NO_ERROR){
        CLOGE("DEBUG(%s[%d]): m_deinit failed = %d",__FUNCTION__, __LINE__, ret);
    }

    EXYNOS_CAMERA_NIGHTSHOT_TRACE_OUT();
}

SFLIBRARY_MGR::SFLType ARCSoftNightShot::getType()
{
    EXYNOS_CAMERA_NIGHTSHOT_TRACE_IN();

    EXYNOS_CAMERA_NIGHTSHOT_TRACE_OUT();
    return SFLIBRARY_MGR::NIGHT;
}

status_t ARCSoftNightShot::init()
{
    EXYNOS_CAMERA_NIGHTSHOT_TRACE_IN();
    status_t ret = NO_ERROR;

    ret = m_init();
    if (ret != NO_ERROR){
        CLOGE("DEBUG(%s[%d]): SFLInit error = %d",__FUNCTION__, __LINE__, ret);
    }

    EXYNOS_CAMERA_NIGHTSHOT_TRACE_OUT();
    return ret;
}

status_t ARCSoftNightShot::deinit()
{
    EXYNOS_CAMERA_NIGHTSHOT_TRACE_IN();
    status_t ret = NO_ERROR;

    ret = m_reset();
    if (ret != NO_ERROR){
        CLOGE("DEBUG(%s[%d]): m_reset failed = %d",__FUNCTION__, __LINE__, ret);
    }

    ret = m_deinit();
    if (ret != NO_ERROR){
        CLOGE("DEBUG(%s[%d]): m_deinit failed = %d",__FUNCTION__, __LINE__, ret);
    }

    EXYNOS_CAMERA_NIGHTSHOT_TRACE_OUT();
    return ret;
}

status_t ARCSoftNightShot::registerCallbacks()
{
    EXYNOS_CAMERA_NIGHTSHOT_TRACE_IN();
    status_t ret = NO_ERROR;

    EXYNOS_CAMERA_NIGHTSHOT_TRACE_OUT();
    return ret;
}

status_t ARCSoftNightShot::prepare()
{
    EXYNOS_CAMERA_NIGHTSHOT_TRACE_IN();
    status_t ret = NO_ERROR;

    ret = m_prepare();

    EXYNOS_CAMERA_NIGHTSHOT_TRACE_OUT();
    return ret;
}

status_t ARCSoftNightShot::setMetaInfo(void* param)
{
    EXYNOS_CAMERA_NIGHTSHOT_TRACE_IN();
    status_t ret = NO_ERROR;

    ret = m_setMetaInfo(param);

    EXYNOS_CAMERA_NIGHTSHOT_TRACE_OUT();
    return ret;
}

status_t ARCSoftNightShot::processCommand(struct CommandInfo *cmdinfo, void* arg)
{
    EXYNOS_CAMERA_NIGHTSHOT_TRACE_IN();

    status_t ret = NO_ERROR;

    SFLBuffer *bufInfo = NULL;
    uint32_t   value = 0;
    list<ExynosCameraSFLInfoType> *outBufferlist = NULL;

    CLOGV("DEBUG(%s[%d]): command(0x%3x) type(%d) pos(%d)", __FUNCTION__, __LINE__, cmdinfo->cmd, cmdinfo->type, cmdinfo->pos);
    switch(cmdinfo->cmd) {
        case SFL::PROCESS:
            CLOGD("DEBUG(%s[%d]): command(PROCESS) type(%d) pos(%d)", __FUNCTION__, __LINE__, cmdinfo->type, cmdinfo->pos);
            m_convertSetupInfoToRunInfo(&m_setupInfo, &m_runInfo);
            outBufferlist = (list<ExynosCameraSFLInfoType>*)arg;
            ret = m_processImage(outBufferlist);
            break;
        case SFL::ADD_BUFFER:
            CLOGD("DEBUG(%s[%d]): command(ADD_BUFFER) type(%d) pos(%d)", __FUNCTION__, __LINE__, cmdinfo->type, cmdinfo->pos);
            bufInfo = (struct SFLBuffer*)arg;
            m_addSflBuffer(cmdinfo, &m_setupInfo, bufInfo);
            break;
        case SFL::SET_MAXBUFFERCNT:
            m_setParam(&m_setupInfo.maxBufCount[cmdinfo->type][cmdinfo->pos], arg);
            break;
        case SFL::GET_MAXBUFFERCNT:
            m_getParam(&m_setupInfo.maxBufCount[cmdinfo->type][cmdinfo->pos], arg);
            break;
        case SFL::GET_CURBUFFERCNT:
            m_getParam(&m_setupInfo.curBufCount[cmdinfo->type][cmdinfo->pos], arg);
            break;
        case SFL::SET_MAXSELECTCNT:
            m_setParam(&m_setupInfo.maxProgress[cmdinfo->type][cmdinfo->pos], arg);
            break;
        case SFL::GET_MAXSELECTCNT:
            m_getParam(&m_setupInfo.maxProgress[cmdinfo->type][cmdinfo->pos], arg);
            break;
        case SFL::SET_CURSELECTCNT:
            m_setParam(&m_setupInfo.curProgress[cmdinfo->type][cmdinfo->pos], arg);
            break;
        case SFL::GET_CURSELECTCNT:
            m_getParam(&m_setupInfo.curProgress[cmdinfo->type][cmdinfo->pos], arg);
            break;
        case SFL::SET_SIZE:
            m_iMemSize = value;
            break;
        case SFL::GET_SIZE:
            *(uint32_t*)arg = m_iMemSize;
            break;
        default:
            CLOGD("DEBUG(%s[%d]): invalid command %d ", __FUNCTION__, __LINE__, cmdinfo->cmd);
            ret = BAD_VALUE;
            break;
    }

    EXYNOS_CAMERA_NIGHTSHOT_TRACE_OUT();
    return ret;
}

void ARCSoftNightShot::setEnable(bool enable)
{
    EXYNOS_CAMERA_NIGHTSHOT_TRACE_IN();

    m_setEnable(enable);

    EXYNOS_CAMERA_NIGHTSHOT_TRACE_OUT();
}

bool ARCSoftNightShot::getEnable()
{
    EXYNOS_CAMERA_NIGHTSHOT_TRACE_IN();
    bool ret = false;

    ret = m_getEnable();

    EXYNOS_CAMERA_NIGHTSHOT_TRACE_OUT();
    return ret;
}

void ARCSoftNightShot::setRunEnable(bool enable)
{
    EXYNOS_CAMERA_NIGHTSHOT_TRACE_IN();

    m_setRunEnable(enable);

    EXYNOS_CAMERA_NIGHTSHOT_TRACE_OUT();
}

bool ARCSoftNightShot::getRunEnable()
{
    EXYNOS_CAMERA_NIGHTSHOT_TRACE_IN();
    bool ret = false;

    ret = m_getRunEnable();

    EXYNOS_CAMERA_NIGHTSHOT_TRACE_OUT();
    return ret;
}

void ARCSoftNightShot::reset()
{
    EXYNOS_CAMERA_NIGHTSHOT_TRACE_IN();

    m_reset();

    EXYNOS_CAMERA_NIGHTSHOT_TRACE_OUT();
}

status_t ARCSoftNightShot::m_init()
{
    EXYNOS_CAMERA_NIGHTSHOT_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);

    MRESULT res = MOK;
    status_t ret = NO_ERROR;

    if (m_ipMem == MNull)
    {
        CLOGD("DEBUG(%s[%d]): mMemMgrCreate: size(%d)", __FUNCTION__, __LINE__, m_iMemSize);
        m_ipMem = MMemAlloc(MNull, m_iMemSize);
        m_iMemMgr = MMemMgrCreate(m_ipMem, m_iMemSize);
    }

    res = ANS_Init(m_iMemMgr, &m_enhancer);
    if (res != MOK)
    {
        CLOGE("ERR(%s[%d]):AME_EXP_Init error = %d", __FUNCTION__, __LINE__, res);
    }

    EXYNOS_CAMERA_NIGHTSHOT_TRACE_OUT();
    return ret;
}

status_t ARCSoftNightShot::m_deinit()
{
    EXYNOS_CAMERA_NIGHTSHOT_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);

    status_t ret = NO_ERROR;

    if (m_enhancer != MNull) {
        CLOGD("DEBUG(%s[%d]): ANS_Uninit", __FUNCTION__, __LINE__);
        ANS_Uninit(&m_enhancer);
        m_enhancer = MNull;
    }

    if(m_iMemMgr != MNull) {
        CLOGD("DEBUG(%s[%d]): MMemMgrDestroy", __FUNCTION__, __LINE__);
        MMemMgrDestroy(m_iMemMgr);
        m_iMemMgr = MNull;
    }

    if(m_ipMem != MNull) {
        CLOGD("DEBUG(%s[%d]): MMemFree", __FUNCTION__, __LINE__);
        MMemFree(MNull, m_ipMem);
        m_ipMem = MNull;
    }

    m_iMemSize = NIGHTSHOT_BUFFER_SIZE;

    EXYNOS_CAMERA_NIGHTSHOT_TRACE_OUT();
    return ret;
}

status_t ARCSoftNightShot::m_addSflBuffer(struct CommandInfo *cmdinfo, struct night_setupInfo *setupInfo, struct SFLBuffer *buffer)
{
    EXYNOS_CAMERA_NIGHTSHOT_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);
    ExynosCameraSFLInfoType sflInfo = NULL;
    status_t ret = NO_ERROR;
    int curIndex = 0;
    int maxIndex = 0;

    buffer->info.type  = cmdinfo->type;
    buffer->info.pos   = cmdinfo->pos;
    buffer->info.value = cmdinfo->cmd;

    sflInfo = new ExynosCameraSFLInfo(buffer);

    curIndex = setupInfo->curBufCount[cmdinfo->type][cmdinfo->pos];
    maxIndex = setupInfo->maxBufCount[cmdinfo->type][cmdinfo->pos];

    if (curIndex > maxIndex) {
        CLOGE("ERR(%s[%d]): Too many buffer, cur(%d) max(%d)", __FUNCTION__, __LINE__, curIndex, maxIndex);
    }

    switch (cmdinfo->type){
    case SFL::TYPE_CAPTURE:
        if (cmdinfo->pos == SFL::POS_SRC) {
              setupInfo->inBufs.push_back(sflInfo);
        } else {
              setupInfo->outBufs.push_back(sflInfo);
        }
        setupInfo->curBufCount[cmdinfo->type][cmdinfo->pos]++;

        break;
    case SFL::TYPE_PREVIEW:
    case SFL::TYPE_PREVIEW_CB:
    case SFL::TYPE_RECORDING:
    default:
        CLOGE("ERR(%s[%d]): invalid buffer type(%d) ", __FUNCTION__, __LINE__, cmdinfo->type);
        ret = INVALID_OPERATION;
        break;
    }

    EXYNOS_CAMERA_NIGHTSHOT_TRACE_OUT();
    return ret;
}

status_t ARCSoftNightShot::m_addRunBuffer(ASVLOFFSCREEN* outBufferlist, list<ExynosCameraSFLInfoType> *runSflList, int outBufCount, list<ExynosCameraSFLInfoType> *inBufferlist)
{
    EXYNOS_CAMERA_NIGHTSHOT_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);

    status_t ret = NO_ERROR;
    ExynosCameraSFLInfoType obj;
    struct SFLBuffer buffer;
    int index = 0;
    memset(&buffer, 0x00, sizeof(buffer));

    if ((uint32_t)outBufCount != inBufferlist->size()) {
        CLOGE("ERR(%s[%d]): buffer count is not matched count(%d) listSize(%d)", __FUNCTION__, __LINE__, outBufCount, inBufferlist->size());
    }


    for(list<ExynosCameraSFLInfoType>::iterator iter = inBufferlist->begin() ; iter != inBufferlist->end() ; ) {
        obj = NULL;

        /* 1. reset current buffer information */
        memset(&outBufferlist[index], 0x00, sizeof(ASVLOFFSCREEN));

        obj = *iter;
        obj->getBuffer(&buffer);
        /* 2. migration new buffer information to bufferlist */
        /* bufferlist->u32PixelArrayFormat = buffer->pixelFormat; */
        outBufferlist[index].u32PixelArrayFormat = ASVL_PAF_NV21;
        outBufferlist[index].i32Width            = buffer.width;
        outBufferlist[index].i32Height           = buffer.height;
        for(int i = 0 ; i < buffer.planeCount ; i++) {
            outBufferlist[index].ppu8Plane[i] = (MUInt8*)buffer.plane[i];
            outBufferlist[index].pi32Pitch[i] = buffer.stride[i];
        }

        runSflList->push_back(obj);

        iter = inBufferlist->erase(iter);
        index++;
    }

    EXYNOS_CAMERA_NIGHTSHOT_TRACE_OUT();
    return ret;
}

status_t ARCSoftNightShot::m_processImage(list<ExynosCameraSFLInfoType> *outBufferlist)
{
    EXYNOS_CAMERA_NIGHTSHOT_TRACE_IN();
    Mutex::Autolock lock(m_processRunLock);

    MRESULT res = MOK;
    status_t ret = NO_ERROR;

    /* 1. get default parameter */
    ANS_GetDefaultParam(&m_runInfo.mParam);

    /* 2. todo : adjust turnning parameter */
    CLOGD("DEBUG(%s[%d]): Intensity(%d) NeedSharpen(%s)", __FUNCTION__, __LINE__, m_runInfo.mParam.lIntensity, (m_runInfo.mParam.bNeedSharpen)?"true":"false");

    CLOGD("DEBUG(%s[%d]): m_enhancer(%p)", __FUNCTION__, __LINE__, m_enhancer);
    CLOGD("DEBUG(%s[%d]): lImgNum(%d) ", __FUNCTION__, __LINE__, m_runInfo.mSrcInput.lImgNum);

    /* 3. run the algorithm */
    res = ANS_Enhancement(m_enhancer, &m_runInfo.mSrcInput, &m_runInfo.mDstImg, &m_runInfo.mParam, MNull, MNull);
    if(MOK != res) {
        ret = INVALID_OPERATION;
        CLOGE("ERR(%s[%d]): ANS_Enhancement failed, error(%d)", __FUNCTION__, __LINE__, res);
    }

    /* 4. copy the running bufinfo to outputlist */
    outBufferlist->clear();
    outBufferlist->assign(m_runInfo.mRunSflList.begin(), m_runInfo.mRunSflList.end());

    EXYNOS_CAMERA_NIGHTSHOT_TRACE_OUT();
    return res;
}

status_t ARCSoftNightShot::m_dump()
{
    EXYNOS_CAMERA_NIGHTSHOT_TRACE_IN();

    MRESULT ret = MOK;

    for(int i = 0 ; i < 3 ; i++) {
        ASVLOFFSCREEN *ptr = m_runInfo.mSrcInput.pImages[i];
        CLOGD("DEBUG(%s[%d]): I(%d) SRC mBufIndex(%d)", __FUNCTION__, __LINE__, i, m_runInfo.mSrcInput.lImgNum);
        CLOGD("DEBUG(%s[%d]): I(%d) SRC format(%d), size(%d x %d) ", __FUNCTION__, __LINE__, i, ptr->u32PixelArrayFormat, ptr->i32Width, ptr->i32Height);
        CLOGD("DEBUG(%s[%d]): I(%d) SRC Pitch[0]=%d pitch[1]=%d", __FUNCTION__, __LINE__, i, ptr->pi32Pitch[0], ptr->pi32Pitch[1]);
        CLOGD("DEBUG(%s[%d]): I(%d) SRC addr[0]=%p addr[1]=%p", __FUNCTION__, __LINE__, i, ptr->ppu8Plane[0], ptr->ppu8Plane[1]);
    }

    ASVLOFFSCREEN *ptr = &m_runInfo.mDstImg;
    CLOGD("DEBUG(%s[%d]): DST format(%d), size(%d x %d) ", __FUNCTION__, __LINE__, ptr->u32PixelArrayFormat, ptr->i32Width, ptr->i32Height);
    CLOGD("DEBUG(%s[%d]): DST Pitch[0]=%d pitch[1]=%d", __FUNCTION__, __LINE__, ptr->pi32Pitch[0], ptr->pi32Pitch[1]);
    CLOGD("DEBUG(%s[%d]): DST addr[0]=%p addr[1]=%p", __FUNCTION__, __LINE__, ptr->ppu8Plane[0], ptr->ppu8Plane[1]);

    EXYNOS_CAMERA_NIGHTSHOT_TRACE_OUT();
    return ret;
}

status_t ARCSoftNightShot::m_convertSetupInfoToRunInfo(struct night_setupInfo *setupInfo, struct night_runInfo *runInfo)
{
    EXYNOS_CAMERA_NIGHTSHOT_TRACE_IN();

    MRESULT ret = MOK;
    int curProgressCount[SFL::POS_MAX] = {0};
    int maxProgressCount[SFL::POS_MAX] = {0};
    int curBufCount[SFL::POS_MAX] = {0};
    int maxBufCount[SFL::POS_MAX] = {0};
    SFL::BufferPos pos = SFL::POS_SRC;
    SFL::BufferType type = SFL::TYPE_CAPTURE;

    for(int i = 0 ; i < SFL::POS_MAX; i++) {
        curBufCount[i] = setupInfo->curBufCount[type][i];
        maxBufCount[i] = setupInfo->maxBufCount[type][i];
        curProgressCount[i] = setupInfo->curProgress[type][i];
        maxProgressCount[i] = setupInfo->maxProgress[type][i];
    }

    pos = SFL::POS_SRC;
    /* check the src Buffer error. */
    if (curBufCount[pos] != maxBufCount[pos] || curProgressCount[pos] != maxProgressCount[pos]) {
        CLOGE("ERR(%s[%d]): insufficient status, maxBufCnt(%d) curBufCnt(%d) maxCnt(%d) curCnt(%d)", __FUNCTION__, __LINE__, curBufCount[pos], maxBufCount[pos], maxProgressCount[pos], curProgressCount[pos]);
    }

    pos = SFL::POS_DST;
    /* check the dst Buffer error. */
    if (curBufCount[pos] != maxBufCount[pos] || curProgressCount[pos] != maxProgressCount[pos]) {
        CLOGE("ERR(%s[%d]): insufficient status, maxBufCnt(%d) curBufCnt(%d) maxCnt(%d) curCnt(%d)", __FUNCTION__, __LINE__, curBufCount[pos], maxBufCount[pos], maxProgressCount[pos], curProgressCount[pos]);
    }

    pos = SFL::POS_SRC;
    runInfo->mSrcInput.lImgNum = curBufCount[pos];
    m_addRunBuffer(runInfo->mSrcImgs, &runInfo->mRunSflList, curBufCount[pos], &setupInfo->inBufs);

    pos = SFL::POS_DST;
    m_addRunBuffer(&runInfo->mDstImg, &runInfo->mRunSflList,curBufCount[pos], &setupInfo->outBufs);

    EXYNOS_CAMERA_NIGHTSHOT_TRACE_OUT();
    return ret;
}

status_t ARCSoftNightShot::m_prepare()
{
    EXYNOS_CAMERA_NIGHTSHOT_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);

    status_t ret = NO_ERROR;
    if (m_setupInfo.adaptiveMaxBufCount[SFL::TYPE_CAPTURE][SFL::POS_SRC] == 1) {
        CLOGD("DEBUG(%s[%d]): skip prepare, brightness value is low, capture count is (%d)", __FUNCTION__, __LINE__, m_setupInfo.adaptiveMaxBufCount[SFL::TYPE_CAPTURE][SFL::POS_SRC]);
        ret = INVALID_OPERATION;
    } else {
        CLOGD("DEBUG(%s[%d]): m_prepare capture count is (%d)", __FUNCTION__, __LINE__, m_setupInfo.adaptiveMaxBufCount[SFL::TYPE_CAPTURE][SFL::POS_SRC]);
        memcpy(m_setupInfo.maxBufCount, m_setupInfo.adaptiveMaxBufCount, sizeof(m_setupInfo.adaptiveMaxBufCount));
    }


    EXYNOS_CAMERA_NIGHTSHOT_TRACE_OUT();
    return ret;
}

status_t ARCSoftNightShot::m_setMetaInfo(void* param)
{
    EXYNOS_CAMERA_NIGHTSHOT_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);

    status_t ret = NO_ERROR;
    struct camera2_shot_ext shot_ext;
    int brightness = 0;
    uint32_t captureCount = 0;

    memset(&shot_ext, 0x00, sizeof(shot_ext));

    memcpy(&shot_ext, (struct camera2_shot_ext*)param, sizeof(struct camera2_shot_ext));

    brightness = (int)shot_ext.shot.udm.internal.vendorSpecific2[102];

    if (brightness < (-4 * 256)) {
        captureCount = 1;
    } else if (brightness >= (-4 * 256) && brightness < (-2 * 256)) {
        captureCount = 3;
    } else if (brightness >= (-2 * 256) && brightness < (-1 * 256)) {
        captureCount = 2;
    } else {
        captureCount = 1;
    }

    if (captureCount !=  m_setupInfo.adaptiveMaxBufCount[SFL::TYPE_CAPTURE][SFL::POS_SRC]) {
        CLOGD("INFO(%s[%d]): changed brightness(%d) captureCount(%u -> %u)", __FUNCTION__, __LINE__, brightness, m_setupInfo.adaptiveMaxBufCount[SFL::TYPE_CAPTURE][SFL::POS_SRC], captureCount);
    }


    m_setupInfo.adaptiveMaxBufCount[SFL::TYPE_CAPTURE][SFL::POS_SRC] = captureCount;
    m_setupInfo.adaptiveMaxBufCount[SFL::TYPE_CAPTURE][SFL::POS_DST] = 1;

    EXYNOS_CAMERA_NIGHTSHOT_TRACE_OUT();
    return ret;
}

status_t ARCSoftNightShot::m_reset()
{
    EXYNOS_CAMERA_NIGHTSHOT_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);

    status_t ret = NO_ERROR;
    ExynosCameraSFLInfoType item = NULL;

    /* 1. reset all runinfo / setupInfo */
    memset(&m_runInfo.mSrcInput, 0x00, sizeof(m_runInfo.mSrcInput));
    memset(&m_runInfo.mSrcImgs, 0x00, sizeof(m_runInfo.mSrcImgs));
    memset(&m_runInfo.mDstImg, 0x00, sizeof(m_runInfo.mDstImg));
    memset(&m_runInfo.mParam, 0x00, sizeof(m_runInfo.mParam));

    for(list<ExynosCameraSFLInfoType>::iterator iter = m_runInfo.mRunSflList.begin() ; iter != m_runInfo.mRunSflList.end() ; )
    {
        item = *iter;
        iter = m_runInfo.mRunSflList.erase(iter);
        item = NULL;
    }
    m_runInfo.mRunSflList.clear();

    for(list<ExynosCameraSFLInfoType>::iterator iter = m_setupInfo.inBufs.begin() ; iter != m_setupInfo.inBufs.end() ; )
    {
        item = *iter;
        iter = m_setupInfo.inBufs.erase(iter);
        item = NULL;
    }
    m_setupInfo.inBufs.clear();

    for(list<ExynosCameraSFLInfoType>::iterator iter = m_setupInfo.outBufs.begin() ; iter != m_setupInfo.outBufs.end() ; )
    {
        item = *iter;
        iter = m_setupInfo.outBufs.erase(iter);
        item = NULL;
    }
    m_setupInfo.outBufs.clear();

    memset(&m_setupInfo.curBufCount, 0x00, sizeof(m_setupInfo.curBufCount));
    memset(&m_setupInfo.maxBufCount, 0x00, sizeof(m_setupInfo.maxBufCount));
    memset(&m_setupInfo.curProgress, 0x00, sizeof(m_setupInfo.curProgress));
    memset(&m_setupInfo.maxProgress, 0x00, sizeof(m_setupInfo.maxProgress));
    memset(&m_setupInfo.adaptiveMaxBufCount, 0x00, sizeof(m_setupInfo.adaptiveMaxBufCount));

    /* 2. set data depedency */
    for(int i = 0 ; i < NIGHTSHOT_CAPTURE_INPUT_IMAGES_MAX ; i++ ) {
        m_runInfo.mSrcInput.pImages[i] = &m_runInfo.mSrcImgs[i];
    }

    EXYNOS_CAMERA_NIGHTSHOT_TRACE_OUT();
    return ret;
}

void ARCSoftNightShot::m_setEnable(bool enable)
{
    EXYNOS_CAMERA_NIGHTSHOT_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);

    m_enable = enable;

    EXYNOS_CAMERA_NIGHTSHOT_TRACE_OUT();
}

bool ARCSoftNightShot::m_getEnable()
{
    EXYNOS_CAMERA_NIGHTSHOT_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);

    bool ret = false;

    ret = m_enable;

    EXYNOS_CAMERA_NIGHTSHOT_TRACE_OUT();
    return ret;
}

void ARCSoftNightShot::m_setRunEnable(bool enable)
{
    EXYNOS_CAMERA_NIGHTSHOT_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);

    m_runningEnable = enable;

    EXYNOS_CAMERA_NIGHTSHOT_TRACE_OUT();
}

bool ARCSoftNightShot::m_getRunEnable()
{
    EXYNOS_CAMERA_NIGHTSHOT_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);
    bool ret = false;

    ret = m_runningEnable;

    EXYNOS_CAMERA_NIGHTSHOT_TRACE_OUT();
    return ret;
}


status_t ARCSoftNightShot::m_setParam(uint32_t *param, void* arg)
{
    EXYNOS_CAMERA_NIGHTSHOT_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);
    uint32_t value = 0;
    status_t ret = NO_ERROR;

    value = *(uint32_t*)arg;
    *param = value;

    EXYNOS_CAMERA_NIGHTSHOT_TRACE_OUT();
    return ret;
}

status_t ARCSoftNightShot::m_getParam(uint32_t *param, void* arg)
{
    EXYNOS_CAMERA_NIGHTSHOT_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);
    status_t ret = NO_ERROR;

    *(uint32_t*)arg = *param;

    EXYNOS_CAMERA_NIGHTSHOT_TRACE_OUT();
    return ret;
}

}; //namespace android
