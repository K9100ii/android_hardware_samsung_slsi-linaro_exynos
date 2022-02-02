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
 * \file      arcsoftHDR.cpp
 * \brief     source file for arcsoftHDR
 * \author    suyounglee(suyoung80.lee@samsung.com)
 * \date      2015/11/25
 *
 */

/* #define LOG_NDEBUG 0 */
#define LOG_TAG "ARCSoftHDR"

#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <android/log.h>
#include <stdlib.h>

#include "arcsoftHDR.h"
#include "fimc-is-metadata.h"

namespace android {


ARCSoftHDR::ARCSoftHDR(const char* name, int cameraId):ExynosCameraSFLInterface(name, cameraId)
{
    EXYNOS_CAMERA_HDR_TRACE_IN();

    /* display library version */
    const AME_Version *ver;
    ver = AME_GetVersion();
    CLOGD("DEBUG(%s[%d]): AME_Version Version:%s",   __FUNCTION__, __LINE__, ver->Version);
    CLOGD("DEBUG(%s[%d]): AME_Version CopyRight:%s", __FUNCTION__, __LINE__, ver->CopyRight);
    CLOGD("DEBUG(%s[%d]): AME_Version BuildDate:%s", __FUNCTION__, __LINE__, ver->BuildDate);
    CLOGD("DEBUG(%s[%d]): AME_Version lBuild:%d",    __FUNCTION__, __LINE__, ver->lBuild);
    CLOGD("DEBUG(%s[%d]): AME_Version lCodebase:%d", __FUNCTION__, __LINE__, ver->lCodebase);
    CLOGD("DEBUG(%s[%d]): AME_Version lMajor:%d",    __FUNCTION__, __LINE__, ver->lMajor);
    CLOGD("DEBUG(%s[%d]): AME_Version lMinor:%d",    __FUNCTION__, __LINE__, ver->lMinor);

    /* 1. initalize the member */
    m_iMemSize = HDR_BUFFER_SIZE;
    m_ipMem    = MNull;
    m_iMemMgr  = MNull;
    mExposureEffect = MNull;

    mParam.lBrightness = 0;
    mParam.lToneLength = 25;

    m_reset();

    EXYNOS_CAMERA_HDR_TRACE_OUT();
}

ARCSoftHDR::~ARCSoftHDR()
{
    EXYNOS_CAMERA_HDR_TRACE_IN();
    status_t ret = NO_ERROR;

    ret = m_reset();
    if (ret != NO_ERROR){
        CLOGE("DEBUG(%s[%d]): m_reset failed = %d",__FUNCTION__, __LINE__, ret);
    }

    ret = m_deinit();
    if (ret != NO_ERROR){
        CLOGE("DEBUG(%s[%d]): m_deinit failed = %d",__FUNCTION__, __LINE__, ret);
    }

    EXYNOS_CAMERA_HDR_TRACE_OUT();
}

SFLIBRARY_MGR::SFLType ARCSoftHDR::getType()
{
    EXYNOS_CAMERA_HDR_TRACE_IN();

    EXYNOS_CAMERA_HDR_TRACE_OUT();
    return SFLIBRARY_MGR::HDR;
}

status_t ARCSoftHDR::init()
{
    EXYNOS_CAMERA_HDR_TRACE_IN();
    status_t ret = NO_ERROR;

    ret = m_init();
    if (ret != NO_ERROR){
        CLOGE("DEBUG(%s[%d]): m_init error = %d",__FUNCTION__, __LINE__, ret);
    }

    EXYNOS_CAMERA_HDR_TRACE_OUT();
    return ret;
}

status_t ARCSoftHDR::deinit()
{
    EXYNOS_CAMERA_HDR_TRACE_IN();
    status_t ret = NO_ERROR;

    ret = m_reset();
    if (ret != NO_ERROR){
        CLOGE("DEBUG(%s[%d]): m_reset failed = %d",__FUNCTION__, __LINE__, ret);
    }

    ret = m_deinit();
    if (ret != NO_ERROR){
        CLOGE("DEBUG(%s[%d]): m_deinit failed = %d",__FUNCTION__, __LINE__, ret);
    }

    EXYNOS_CAMERA_HDR_TRACE_OUT();
    return ret;
}

status_t ARCSoftHDR::registerCallbacks()
{
    EXYNOS_CAMERA_HDR_TRACE_IN();
    status_t ret = NO_ERROR;

    EXYNOS_CAMERA_HDR_TRACE_OUT();
    return ret;
}

status_t ARCSoftHDR::prepare()
{
    EXYNOS_CAMERA_HDR_TRACE_IN();
    status_t ret = NO_ERROR;

    ret = m_prepare();

    EXYNOS_CAMERA_HDR_TRACE_OUT();
    return ret;
}

status_t ARCSoftHDR::setMetaInfo(void* param)
{
    EXYNOS_CAMERA_HDR_TRACE_IN();
    status_t ret = NO_ERROR;

    ret = m_setMetaInfo(param);

    EXYNOS_CAMERA_HDR_TRACE_OUT();
    return ret;
}

status_t ARCSoftHDR::processCommand(struct CommandInfo *cmdinfo, void* arg)
{
    EXYNOS_CAMERA_HDR_TRACE_IN();

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

    EXYNOS_CAMERA_HDR_TRACE_OUT();
    return ret;
}

void ARCSoftHDR::setEnable(bool enable)
{
    EXYNOS_CAMERA_HDR_TRACE_IN();

    m_setEnable(enable);

    EXYNOS_CAMERA_HDR_TRACE_OUT();
}

bool ARCSoftHDR::getEnable()
{
    EXYNOS_CAMERA_HDR_TRACE_IN();
    bool ret = false;

    ret = m_getEnable();

    EXYNOS_CAMERA_HDR_TRACE_OUT();
    return ret;
}

void ARCSoftHDR::setRunEnable(bool enable)
{
    EXYNOS_CAMERA_HDR_TRACE_IN();

    m_setRunEnable(enable);

    EXYNOS_CAMERA_HDR_TRACE_OUT();
}

bool ARCSoftHDR::getRunEnable()
{
    EXYNOS_CAMERA_HDR_TRACE_IN();
    bool ret = false;

    ret = m_getRunEnable();

    EXYNOS_CAMERA_HDR_TRACE_OUT();
    return ret;
}

void ARCSoftHDR::reset()
{
    EXYNOS_CAMERA_HDR_TRACE_IN();

    m_reset();

    EXYNOS_CAMERA_HDR_TRACE_OUT();
}

status_t ARCSoftHDR::m_init()
{
    EXYNOS_CAMERA_HDR_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);

    MRESULT res = MOK;
    status_t ret = NO_ERROR;

    if (m_ipMem == MNull)
    {
        CLOGD("DEBUG(%s[%d]): mMemMgrCreate: size(%d)", __FUNCTION__, __LINE__, m_iMemSize);
        m_ipMem = MMemAlloc(MNull, m_iMemSize);
        m_iMemMgr = MMemMgrCreate(m_ipMem, m_iMemSize);
    }

    res = AME_EXP_Init(m_iMemMgr, 0, &mExposureEffect);
    if (res != MOK)
    {
        CLOGE("ERR(%s[%d]):AME_EXP_Init error = %d", __FUNCTION__, __LINE__, res);
    }

    EXYNOS_CAMERA_HDR_TRACE_OUT();
    return ret;
}

status_t ARCSoftHDR::m_deinit()
{
    EXYNOS_CAMERA_HDR_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);

    status_t ret = NO_ERROR;

    if (mExposureEffect != MNull) {
        CLOGD("DEBUG(%s[%d]): AME_Uninit", __FUNCTION__, __LINE__);
        AME_EXP_Uninit(m_iMemMgr, mExposureEffect);
        mExposureEffect = MNull;
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

    m_iMemSize = HDR_BUFFER_SIZE;

    EXYNOS_CAMERA_HDR_TRACE_OUT();
    return ret;
}

status_t ARCSoftHDR::m_addSflBuffer(struct CommandInfo *cmdinfo, struct hdr_setupInfo *setupInfo, struct SFLBuffer *buffer)
{
    EXYNOS_CAMERA_HDR_TRACE_IN();
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

    EXYNOS_CAMERA_HDR_TRACE_OUT();
    return ret;
}

status_t ARCSoftHDR::m_addRunBuffer(ASVLOFFSCREEN* outBufferlist, list<ExynosCameraSFLInfoType> *runSflList, int outBufCount, list<ExynosCameraSFLInfoType> *inBufferlist)
{
    EXYNOS_CAMERA_HDR_TRACE_IN();
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

    EXYNOS_CAMERA_HDR_TRACE_OUT();
    return ret;
}

status_t ARCSoftHDR::m_processImage(list<ExynosCameraSFLInfoType> *outBufferlist)
{
    EXYNOS_CAMERA_HDR_TRACE_IN();
    Mutex::Autolock lock(m_processRunLock);
    ASVLOFFSCREEN imgTemp[1];

    MRESULT res  = MERR_NONE;
    status_t ret = NO_ERROR;

    memcpy((void *)&imgTemp[0],            (const void *) &m_runInfo.mSrcImgs[1], sizeof(m_runInfo.mSrcImgs[0]));
    memcpy((void *)&m_runInfo.mSrcImgs[1], (const void *) &m_runInfo.mSrcImgs[0], sizeof(m_runInfo.mSrcImgs[0]));
    memcpy((void *)&m_runInfo.mSrcImgs[0], (const void *) &imgTemp[0], sizeof(m_runInfo.mSrcImgs[0]));
    /* 2. Process Frames */
    CLOGD("DEBUG(%s[%d]): AME_ProcessFrames",  __FUNCTION__, __LINE__);
    res = AME_ProcessFrames(m_iMemMgr, mExposureEffect, m_runInfo.mSrcImgs, 0, MNull,MNull);
    if (res != MERR_NONE)
    {
        CLOGE("AME_ProcessFrames res: %d", res);
    }

    res  = MERR_NONE;
    res = AME_ProcessFrames(m_iMemMgr, mExposureEffect, m_runInfo.mSrcImgs, 1, MNull,MNull);
    if (res != MERR_NONE)
    {
        CLOGE("AME_ProcessFrames res: %d", res);
    }

    res  = MERR_NONE;
    res = AME_ProcessFrames(m_iMemMgr, mExposureEffect, m_runInfo.mSrcImgs, 2, MNull,MNull);
    if (res != MERR_NONE)
    {
        CLOGE("AME_ProcessFrames res: %d", res);
    }

    CLOGD("DEBUG(%s[%d]): ToneLength(%d)",      __FUNCTION__, __LINE__, mParam.lToneLength);
    CLOGD("DEBUG(%s[%d]): Brightness(%d)",      __FUNCTION__, __LINE__, mParam.lBrightness);
    CLOGD("DEBUG(%s[%d]): ToneSaturation(%d)",  __FUNCTION__, __LINE__, mParam.lToneSaturation);
    CLOGD("DEBUG(%s[%d]): mExposureEffect(%p)", __FUNCTION__, __LINE__, mExposureEffect);

    CLOGD("DEBUG(%s[%d]): AME_RunAlignHighDynamicRangeImage",  __FUNCTION__, __LINE__);
    res  = MERR_NONE;
    res = AME_RunAlignHighDynamicRangeImage(
                    m_iMemMgr,
                    mExposureEffect,
                    m_runInfo.mSrcImgs,
                    3,              //MAX_NUMBER_IMAGE,
                    m_runInfo.mDstImgs,
                    1,              //ResultImgNumber,
                    &mParam,
                    2,              //HDR Mode
                    MNull,          //Callback function
                    MNull);         //User data
    if (res != MERR_NONE)
    {
        CLOGE("AME_RunAlignHighDynamicRangeImage res: %d", res);
       // return 0;
    }

    /* 4. copy the running bufinfo to outputlist */
    outBufferlist->clear();
    outBufferlist->assign(m_runInfo.mRunSflList.begin(), m_runInfo.mRunSflList.end());

    EXYNOS_CAMERA_HDR_TRACE_OUT();
    return res;
}

status_t ARCSoftHDR::m_dump()
{
    EXYNOS_CAMERA_HDR_TRACE_IN();

    MRESULT ret = MOK;

    ASVLOFFSCREEN *ptr = &m_runInfo.mDstImgs[0];
    CLOGD("DEBUG(%s[%d]): DST format(%d), size(%d x %d) ", __FUNCTION__, __LINE__, ptr->u32PixelArrayFormat, ptr->i32Width, ptr->i32Height);
    CLOGD("DEBUG(%s[%d]): DST Pitch[0]=%d pitch[1]=%d", __FUNCTION__, __LINE__, ptr->pi32Pitch[0], ptr->pi32Pitch[1]);
    CLOGD("DEBUG(%s[%d]): DST addr[0]=%p addr[1]=%p", __FUNCTION__, __LINE__, ptr->ppu8Plane[0], ptr->ppu8Plane[1]);

    EXYNOS_CAMERA_HDR_TRACE_OUT();
    return ret;
}

status_t ARCSoftHDR::m_convertSetupInfoToRunInfo(struct hdr_setupInfo *setupInfo, struct hdr_runInfo *runInfo)
{
    EXYNOS_CAMERA_HDR_TRACE_IN();

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
    m_addRunBuffer(runInfo->mSrcImgs, &runInfo->mRunSflList, curBufCount[pos], &setupInfo->inBufs);

    pos = SFL::POS_DST;
    m_addRunBuffer(runInfo->mDstImgs, &runInfo->mRunSflList,curBufCount[pos], &setupInfo->outBufs);

    EXYNOS_CAMERA_HDR_TRACE_OUT();
    return ret;
}

status_t ARCSoftHDR::m_prepare()
{
    EXYNOS_CAMERA_HDR_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);

    status_t ret = NO_ERROR;
    m_setupInfo.maxBufCount[SFL::TYPE_CAPTURE][SFL::POS_SRC] = 3;
    m_setupInfo.maxBufCount[SFL::TYPE_CAPTURE][SFL::POS_DST] = 1;

    EXYNOS_CAMERA_HDR_TRACE_OUT();
    return ret;
}

status_t ARCSoftHDR::m_setMetaInfo(__unused void* param)
{
    EXYNOS_CAMERA_HDR_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);

    status_t ret = NO_ERROR;

    EXYNOS_CAMERA_HDR_TRACE_OUT();
    return ret;
}

status_t ARCSoftHDR::m_reset()
{
    EXYNOS_CAMERA_HDR_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);

    status_t ret = NO_ERROR;
    ExynosCameraSFLInfoType item = NULL;

    /* 1. reset all runinfo / setupInfo */
    memset(&m_runInfo.mSrcImgs, 0x00, sizeof(m_runInfo.mSrcImgs));
    memset(&m_runInfo.mDstImgs, 0x00, sizeof(m_runInfo.mDstImgs));
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

    EXYNOS_CAMERA_HDR_TRACE_OUT();
    return ret;
}

void ARCSoftHDR::m_setEnable(bool enable)
{
    EXYNOS_CAMERA_HDR_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);

    m_enable = enable;

    EXYNOS_CAMERA_HDR_TRACE_OUT();
}

bool ARCSoftHDR::m_getEnable()
{
    EXYNOS_CAMERA_HDR_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);

    bool ret = false;

    ret = m_enable;

    EXYNOS_CAMERA_HDR_TRACE_OUT();
    return ret;
}

void ARCSoftHDR::m_setRunEnable(bool enable)
{
    EXYNOS_CAMERA_HDR_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);

    m_runningEnable = enable;

    EXYNOS_CAMERA_HDR_TRACE_OUT();
}

bool ARCSoftHDR::m_getRunEnable()
{
    EXYNOS_CAMERA_HDR_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);
    bool ret = false;

    ret = m_runningEnable;

    EXYNOS_CAMERA_HDR_TRACE_OUT();
    return ret;
}


status_t ARCSoftHDR::m_setParam(uint32_t *param, void* arg)
{
    EXYNOS_CAMERA_HDR_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);
    uint32_t value = 0;
    status_t ret = NO_ERROR;

    value = *(uint32_t*)arg;
    *param = value;

    EXYNOS_CAMERA_HDR_TRACE_OUT();
    return ret;
}

status_t ARCSoftHDR::m_getParam(uint32_t *param, void* arg)
{
    EXYNOS_CAMERA_HDR_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);
    status_t ret = NO_ERROR;

    *(uint32_t*)arg = *param;

    EXYNOS_CAMERA_HDR_TRACE_OUT();
    return ret;
}

}; //namespace android
