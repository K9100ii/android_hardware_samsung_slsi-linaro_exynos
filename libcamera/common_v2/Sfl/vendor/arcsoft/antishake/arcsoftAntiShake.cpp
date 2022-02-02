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
 * \file      arcsoftAntishake.cpp
 * \brief     source file for ARCSoftAntiShake
 * \author    suyounglee(suyoung80.lee@samsung.com)
 * \date      2015/11/25
 *
 */

/* #define LOG_NDEBUG 0 */
#define LOG_TAG "ARCSoftAntiShake"

#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <android/log.h>
#include <stdlib.h>

#include "arcsoftAntiShake.h"
#include "fimc-is-metadata.h"
#include "ExynosCameraUtils.h"

namespace android {


ARCSoftAntiShake::ARCSoftAntiShake(const char* name, int cameraId):ExynosCameraSFLInterface(name, cameraId)
{
    EXYNOS_CAMERA_ANTISHAKE_TRACE_IN();

    /* display library version */
    ASSF_Version ver;
    ASSF_GetVersion(&ver);
    CLOGD("DEBUG(%s[%d]): ASSF_Version Version:%s", __FUNCTION__, __LINE__, ver.Version);
    CLOGD("DEBUG(%s[%d]): ASSF_Version CopyRight:%s", __FUNCTION__, __LINE__, ver.CopyRight);
    CLOGD("DEBUG(%s[%d]): ASSF_Version BuildDate:%s", __FUNCTION__, __LINE__, ver.BuildDate);
    CLOGD("DEBUG(%s[%d]): ASSF_Version lBuild:%d", __FUNCTION__, __LINE__, ver.lBuild);
    CLOGD("DEBUG(%s[%d]): ASSF_Version lCodebase:%d", __FUNCTION__, __LINE__, ver.lCodebase);
    CLOGD("DEBUG(%s[%d]): ASSF_Version lMajor:%d", __FUNCTION__, __LINE__, ver.lMajor);
    CLOGD("DEBUG(%s[%d]): ASSF_Version lMinor:%d", __FUNCTION__, __LINE__, ver.lMinor);

    /* 1. initalize the member */
    m_iMemSize = ANTISHAKE_BUFFER_SIZE;
    m_ipMem = MNull;
    m_iMemMgr = MNull;
    m_enhancer = MNull;

    m_reset();

    for(int i = 0 ; i < ANTISHAKE_PREVIEW_INPUT_IMAGES_MAX ; i++) {
        m_previewIpMem[i] = (MByte*)MMemAlloc(MNull, 3840 * 2160 * 2);
    }

    memset(&m_sensorInfo, 0x00, sizeof(m_sensorInfo));

    EXYNOS_CAMERA_ANTISHAKE_TRACE_OUT();
}

ARCSoftAntiShake::~ARCSoftAntiShake()
{
    EXYNOS_CAMERA_ANTISHAKE_TRACE_IN();
    status_t ret = NO_ERROR;

    ret = m_reset();
    if (ret != NO_ERROR){
        CLOGE("DEBUG(%s[%d]): m_reset failed = %d",__FUNCTION__, __LINE__, ret);
    }

    ret = m_deinit();
    if (ret != NO_ERROR){
        CLOGE("DEBUG(%s[%d]): m_deinit failed = %d",__FUNCTION__, __LINE__, ret);
    }

    for(int i = 0 ; i < ANTISHAKE_PREVIEW_INPUT_IMAGES_MAX ; i++) {
        if(m_previewIpMem[i])
            MMemFree(MNull, m_previewIpMem[i]);
    }

    memset(&m_sensorInfo, 0x00, sizeof(m_sensorInfo));

    EXYNOS_CAMERA_ANTISHAKE_TRACE_OUT();
}

SFLIBRARY_MGR::SFLType ARCSoftAntiShake::getType()
{
    EXYNOS_CAMERA_ANTISHAKE_TRACE_IN();

    EXYNOS_CAMERA_ANTISHAKE_TRACE_OUT();
    return SFLIBRARY_MGR::ANTISHAKE;
}

status_t ARCSoftAntiShake::init()
{
    EXYNOS_CAMERA_ANTISHAKE_TRACE_IN();

    status_t ret = NO_ERROR;
    bool checkState = false;

    ret = m_init();
    if (ret != NO_ERROR){
        CLOGE("DEBUG(%s[%d]): SFLInit error = %d",__FUNCTION__, __LINE__, ret);
    }

    EXYNOS_CAMERA_ANTISHAKE_TRACE_OUT();
    return ret;
}

status_t ARCSoftAntiShake::deinit()
{
    EXYNOS_CAMERA_ANTISHAKE_TRACE_IN();

    status_t ret = NO_ERROR;
    bool checkState = false;
    bool flagExit = false;
    int32_t maxTry = 3;

    ret = m_reset();
    if (ret != NO_ERROR){
        CLOGE("DEBUG(%s[%d]): m_reset failed = %d",__FUNCTION__, __LINE__, ret);
    }

    ret = m_deinit();
    if (ret != NO_ERROR){
        CLOGE("DEBUG(%s[%d]): m_deinit failed = %d",__FUNCTION__, __LINE__, ret);
    }

    EXYNOS_CAMERA_ANTISHAKE_TRACE_OUT();
    return ret;
}

status_t ARCSoftAntiShake::registerCallbacks()
{
    EXYNOS_CAMERA_ANTISHAKE_TRACE_IN();
    status_t ret = NO_ERROR;

    EXYNOS_CAMERA_ANTISHAKE_TRACE_OUT();
    return ret;
}

status_t ARCSoftAntiShake::prepare()
{
    EXYNOS_CAMERA_ANTISHAKE_TRACE_IN();
    status_t ret = NO_ERROR;

    ret = m_prepare();

    ret = m_updateRunParameter(&m_setupInfo, &m_runInfo);

    EXYNOS_CAMERA_ANTISHAKE_TRACE_OUT();
    return ret;
}

status_t ARCSoftAntiShake::setMetaInfo(void* param)
{
    EXYNOS_CAMERA_ANTISHAKE_TRACE_IN();
    status_t ret = NO_ERROR;

    ret = m_setMetaInfo(param);

    EXYNOS_CAMERA_ANTISHAKE_TRACE_OUT();
    return ret;
}

status_t ARCSoftAntiShake::processCommand(struct CommandInfo *cmdinfo, void* arg)
{
    EXYNOS_CAMERA_ANTISHAKE_TRACE_IN();

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
        case SFL::SET_CONFIGDATA:
            m_setConfigData(cmdinfo, arg);
            break;
        case SFL::GET_ADJUSTPARAMINFO:
            m_getAdjustDataInfo(cmdinfo, arg);
            break;
        case SFL::SET_BESTFRAMEINFO:
            m_setParam(&m_setupInfo.bestFrameInfo[cmdinfo->type][cmdinfo->pos], arg);
            break;
        case SFL::GET_BESTFRAMEINFO:
            m_getParam(&m_setupInfo.bestFrameInfo[cmdinfo->type][cmdinfo->pos], arg);
            break;
        default:
            CLOGD("DEBUG(%s[%d]): invalid command %d ", __FUNCTION__, __LINE__, cmdinfo->cmd);
            ret = BAD_VALUE;
            break;
    }

    EXYNOS_CAMERA_ANTISHAKE_TRACE_OUT();
    return ret;
}

void ARCSoftAntiShake::setEnable(bool enable)
{
    EXYNOS_CAMERA_ANTISHAKE_TRACE_IN();

    m_setEnable(enable);

    EXYNOS_CAMERA_ANTISHAKE_TRACE_OUT();
}

bool ARCSoftAntiShake::getEnable()
{
    EXYNOS_CAMERA_ANTISHAKE_TRACE_IN();
    bool ret = false;

    ret = m_getEnable();

    EXYNOS_CAMERA_ANTISHAKE_TRACE_OUT();
    return ret;
}

void ARCSoftAntiShake::setRunEnable(bool enable)
{
    EXYNOS_CAMERA_ANTISHAKE_TRACE_IN();

    m_setRunEnable(enable);

    EXYNOS_CAMERA_ANTISHAKE_TRACE_OUT();
}

bool ARCSoftAntiShake::getRunEnable()
{
    EXYNOS_CAMERA_ANTISHAKE_TRACE_IN();
    bool ret = false;

    ret = m_getRunEnable();

    EXYNOS_CAMERA_ANTISHAKE_TRACE_OUT();
    return ret;
}

void ARCSoftAntiShake::reset()
{
    EXYNOS_CAMERA_ANTISHAKE_TRACE_IN();

    m_reset();

    EXYNOS_CAMERA_ANTISHAKE_TRACE_OUT();
}

status_t ARCSoftAntiShake::m_init()
{
    EXYNOS_CAMERA_ANTISHAKE_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);

    MRESULT res = MOK;
    status_t ret = NO_ERROR;

    if (m_ipMem == MNull)
    {
        CLOGD("DEBUG(%s[%d]): mMemMgrCreate: size(%d)", __FUNCTION__, __LINE__, m_iMemSize);
        m_ipMem = MMemAlloc(MNull, m_iMemSize);
        m_iMemMgr = MMemMgrCreate(m_ipMem, m_iMemSize);
    }

    res = ASSF_Init(m_iMemMgr, &m_enhancer);
    if (res != MOK)
    {
        CLOGE("ERR(%s[%d]):AME_EXP_Init error = %d", __FUNCTION__, __LINE__, res);
    }

    EXYNOS_CAMERA_ANTISHAKE_TRACE_OUT();
    return ret;
}

status_t ARCSoftAntiShake::m_deinit()
{
    EXYNOS_CAMERA_ANTISHAKE_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);

    status_t ret = NO_ERROR;

    if (m_enhancer != MNull) {
        CLOGD("DEBUG(%s[%d]): ASSF_Uninit", __FUNCTION__, __LINE__);
        ASSF_Uninit(&m_enhancer);
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

    m_iMemSize = ANTISHAKE_BUFFER_SIZE;

    EXYNOS_CAMERA_ANTISHAKE_TRACE_OUT();
    return ret;
}

status_t ARCSoftAntiShake::m_addSflBuffer(struct CommandInfo *cmdinfo, struct antishake_setupInfo *setupInfo, struct SFLBuffer *buffer)
{
    EXYNOS_CAMERA_ANTISHAKE_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);
    ExynosCameraSFLInfoType sflInfo = NULL;
    status_t ret = NO_ERROR;
    int curIndex = 0;
    int maxIndex = 0;
    char *dst = NULL;
    int offset = 0;

    buffer->info.type  = cmdinfo->type;
    buffer->info.pos   = cmdinfo->pos;
    buffer->info.value = cmdinfo->cmd;

    curIndex = setupInfo->curBufCount[cmdinfo->type][cmdinfo->pos];
    maxIndex = setupInfo->maxBufCount[cmdinfo->type][cmdinfo->pos];

    if (curIndex > maxIndex) {
        CLOGE("ERR(%s[%d]): Too many buffer, cur(%d) max(%d)", __FUNCTION__, __LINE__, curIndex, maxIndex);
    }

    switch (cmdinfo->type){
    case SFL::TYPE_CAPTURE:
        sflInfo = new ExynosCameraSFLInfo(buffer);
        if (cmdinfo->pos == SFL::POS_SRC) {
            setupInfo->captureInBufs.push_back(sflInfo);
        } else {
            setupInfo->captureOutBufs.push_back(sflInfo);
        }
        setupInfo->curBufCount[cmdinfo->type][cmdinfo->pos]++;
        break;
    case SFL::TYPE_PREVIEW:
        offset = 0;
        for(int i = 0; i < buffer->planeCount ; i++) {
            dst = (char*)m_previewIpMem[curIndex];
            dst += offset;
            memcpy(dst, buffer->plane[i], buffer->size[i]);
            buffer->plane[i] = dst;
            offset += buffer->size[i];
        }
        sflInfo = new ExynosCameraSFLInfo(buffer,  true);
        if (cmdinfo->pos == SFL::POS_SRC) {
            setupInfo->previewInBufs.push_back(sflInfo);
            setupInfo->curBufCount[cmdinfo->type][cmdinfo->pos]++;
        } else {
            CLOGE("ERR(%s[%d]): no preview output, invalid buffer type(%d) pos(%d)", __FUNCTION__, __LINE__, cmdinfo->type, cmdinfo->pos);
        }
        break;
    case SFL::TYPE_PREVIEW_CB:
    case SFL::TYPE_RECORDING:
    default:
        CLOGE("ERR(%s[%d]): invalid buffer type(%d) ", __FUNCTION__, __LINE__, cmdinfo->type);
        ret = INVALID_OPERATION;
        break;
    }

    EXYNOS_CAMERA_ANTISHAKE_TRACE_OUT();
    return ret;
}

status_t ARCSoftAntiShake::m_addRunBuffer(ASVLOFFSCREEN* outBufferlist, list<ExynosCameraSFLInfoType> *runSflList, int outBufCount, list<ExynosCameraSFLInfoType> *inBufferlist)
{
    EXYNOS_CAMERA_ANTISHAKE_TRACE_IN();
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

        if (obj->getBufferUsage() == true) {
            obj = NULL;
        } else {
            runSflList->push_back(obj);
        }

        iter = inBufferlist->erase(iter);
        index++;
    }

    EXYNOS_CAMERA_ANTISHAKE_TRACE_OUT();
    return ret;
}

status_t ARCSoftAntiShake::m_processImage(list<ExynosCameraSFLInfoType> *outBufferlist)
{
    EXYNOS_CAMERA_ANTISHAKE_TRACE_IN();
    Mutex::Autolock lock(m_processRunLock);

    MRESULT res = MOK;
    status_t ret = NO_ERROR;

    /* 1. todo : adjust turnning parameter */
    CLOGD("DEBUG(%s[%d]): Intensity(%d) iso(%f) exposure(%f)", __FUNCTION__, __LINE__, m_runInfo.param.lIntensity, m_runInfo.param.camParam.fISO, m_runInfo.param.camParam.fExpoTime);

    CLOGD("DEBUG(%s[%d]): m_enhancer(%p)", __FUNCTION__, __LINE__, m_enhancer);
    CLOGD("DEBUG(%s[%d]): lImgNum(%d) ", __FUNCTION__, __LINE__, m_runInfo.previewInput.lImgNum);

    /* 2. run the algorithm */
    res = ASSF_Enhancement(m_enhancer, &m_runInfo.previewInput, &m_runInfo.srcCaptureImg, &m_runInfo.dstCaptureImg, &m_runInfo.param, MNull, MNull);
    if(MOK != res) {
        ret = INVALID_OPERATION;
        CLOGE("ERR(%s[%d]): ASSF_Enhancement failed, error(%d)", __FUNCTION__, __LINE__, res);
    }

    /* 3. copy the running bufinfo to outputlist */
    outBufferlist->clear();
    outBufferlist->assign(m_runInfo.mRunSflList.begin(), m_runInfo.mRunSflList.end());

    EXYNOS_CAMERA_ANTISHAKE_TRACE_OUT();
    return res;
}

status_t ARCSoftAntiShake::m_dump()
{
    EXYNOS_CAMERA_ANTISHAKE_TRACE_IN();

    MRESULT ret = MOK;
    ASVLOFFSCREEN *ptr = NULL;

    for(int i = 0 ; i < 3 ; i++) {
        ASVLOFFSCREEN *ptr = m_runInfo.previewInput.pImages[i];
        CLOGD("DEBUG(%s[%d]): I(%d) SRC mBufIndex(%d)", __FUNCTION__, __LINE__, i, m_runInfo.previewInput.lImgNum);
        CLOGD("DEBUG(%s[%d]): I(%d) SRC format(%d), size(%d x %d) ", __FUNCTION__, __LINE__, i, ptr->u32PixelArrayFormat, ptr->i32Width, ptr->i32Height);
        CLOGD("DEBUG(%s[%d]): I(%d) SRC Pitch[0]=%d pitch[1]=%d", __FUNCTION__, __LINE__, i, ptr->pi32Pitch[0], ptr->pi32Pitch[1]);
        CLOGD("DEBUG(%s[%d]): I(%d) SRC addr[0]=%p addr[1]=%p", __FUNCTION__, __LINE__, i, ptr->ppu8Plane[0], ptr->ppu8Plane[1]);
    }

    ptr = &m_runInfo.srcCaptureImg;
    CLOGD("DEBUG(%s[%d]): SRC format(%d), size(%d x %d) ", __FUNCTION__, __LINE__, ptr->u32PixelArrayFormat, ptr->i32Width, ptr->i32Height);
    CLOGD("DEBUG(%s[%d]): SRC Pitch[0]=%d pitch[1]=%d", __FUNCTION__, __LINE__, ptr->pi32Pitch[0], ptr->pi32Pitch[1]);
    CLOGD("DEBUG(%s[%d]): SRC addr[0]=%p addr[1]=%p", __FUNCTION__, __LINE__, ptr->ppu8Plane[0], ptr->ppu8Plane[1]);

    ptr = &m_runInfo.dstCaptureImg;
    CLOGD("DEBUG(%s[%d]): DST format(%d), size(%d x %d) ", __FUNCTION__, __LINE__, ptr->u32PixelArrayFormat, ptr->i32Width, ptr->i32Height);
    CLOGD("DEBUG(%s[%d]): DST Pitch[0]=%d pitch[1]=%d", __FUNCTION__, __LINE__, ptr->pi32Pitch[0], ptr->pi32Pitch[1]);
    CLOGD("DEBUG(%s[%d]): DST addr[0]=%p addr[1]=%p", __FUNCTION__, __LINE__, ptr->ppu8Plane[0], ptr->ppu8Plane[1]);

    EXYNOS_CAMERA_ANTISHAKE_TRACE_OUT();
    return ret;
}

status_t ARCSoftAntiShake::m_convertSetupInfoToRunInfo(struct antishake_setupInfo *setupInfo, struct antishake_runInfo *runInfo)
{
    EXYNOS_CAMERA_ANTISHAKE_TRACE_IN();

    MRESULT ret = MOK;
    int curProgressCount[SFL::POS_MAX] = {0};
    int maxProgressCount[SFL::POS_MAX] = {0};
    int curBufCount[SFL::POS_MAX] = {0};
    int maxBufCount[SFL::POS_MAX] = {0};
    SFL::BufferPos pos = SFL::POS_SRC;
    SFL::BufferType type = SFL::TYPE_CAPTURE;

    type = SFL::TYPE_PREVIEW;
    pos  = SFL::POS_SRC;
    /* check the dst Buffer error. */
    if (setupInfo->curBufCount[type][pos] != setupInfo->maxBufCount[type][pos] || setupInfo->curProgress[type][pos] != setupInfo->maxProgress[type][pos]) {
        CLOGE("ERR(%s[%d]): insufficient status, maxBufCnt(%d) curBufCnt(%d) maxCnt(%d) curCnt(%d)", __FUNCTION__, __LINE__, setupInfo->maxBufCount[type][pos], setupInfo->curBufCount[type][pos], setupInfo->maxProgress[type][pos], setupInfo->curProgress[type][pos]);
    }

    type = SFL::TYPE_CAPTURE;
    pos  = SFL::POS_SRC;
    /* check the src Buffer error. */
    if (setupInfo->curBufCount[type][pos] != setupInfo->maxBufCount[type][pos] || setupInfo->curProgress[type][pos] != setupInfo->maxProgress[type][pos]) {
        CLOGE("ERR(%s[%d]): insufficient status, maxBufCnt(%d) curBufCnt(%d) maxCnt(%d) curCnt(%d)", __FUNCTION__, __LINE__, setupInfo->maxBufCount[type][pos], setupInfo->curBufCount[type][pos], setupInfo->maxProgress[type][pos], setupInfo->curProgress[type][pos]);
    }

    type = SFL::TYPE_CAPTURE;
    pos  = SFL::POS_DST;
    /* check the dst Buffer error. */
    if (setupInfo->curBufCount[type][pos] != setupInfo->maxBufCount[type][pos] || setupInfo->curProgress[type][pos] != setupInfo->maxProgress[type][pos]) {
        CLOGE("ERR(%s[%d]): insufficient status, maxBufCnt(%d) curBufCnt(%d) maxCnt(%d) curCnt(%d)", __FUNCTION__, __LINE__, setupInfo->maxBufCount[type][pos], setupInfo->curBufCount[type][pos], setupInfo->maxProgress[type][pos], setupInfo->curProgress[type][pos]);
    }

    type = SFL::TYPE_PREVIEW;
    pos  = SFL::POS_SRC;
    runInfo->previewInput.lImgNum = setupInfo->curBufCount[type][pos];
    m_addRunBuffer(runInfo->previewImgs, &runInfo->mRunSflList, setupInfo->curBufCount[type][pos], &setupInfo->previewInBufs);

    type = SFL::TYPE_CAPTURE;
    pos  = SFL::POS_SRC;
    m_addRunBuffer(&runInfo->srcCaptureImg, &runInfo->mRunSflList, setupInfo->curBufCount[type][pos], &setupInfo->captureInBufs);

    type = SFL::TYPE_CAPTURE;
    pos  = SFL::POS_DST;
    m_addRunBuffer(&runInfo->dstCaptureImg, &runInfo->mRunSflList, setupInfo->curBufCount[type][pos], &setupInfo->captureOutBufs);

    EXYNOS_CAMERA_ANTISHAKE_TRACE_OUT();
    return ret;
}

status_t ARCSoftAntiShake::m_updateRunParameter(struct antishake_setupInfo *setupInfo, struct antishake_runInfo *runInfo)
{
    EXYNOS_CAMERA_ANTISHAKE_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);
    status_t ret = NO_ERROR;

    /* get default parameter */
    ASSF_GetDefaultParam(&m_runInfo.param);

    uint64_t sensitivity = setupInfo->adaptiveMetaInfo.shot.dm.sensor.sensitivity;
    uint32_t exposureTime = setupInfo->adaptiveMetaInfo.shot.dm.sensor.exposureTime;

    uint32_t calSensitivity = 0;
    uint64_t calExposureTime = 0l;
    MInt32   calIntensity = 0;

    /* adjust turning parameter : sensitivity */
    if (sensitivity < 200l) {
        calIntensity = 5;
        calSensitivity = 400;
    } else if (sensitivity >= 200l && sensitivity < 400l) {
        calIntensity = 6;
        calSensitivity = 600;
    } else if (sensitivity >= 400l && sensitivity < 600l) {
        calIntensity = 7;
        calSensitivity = 1000;
    } else if (sensitivity >= 600l && sensitivity < 800l) {
        calIntensity = 8;
        calSensitivity = 1400;
    } else {
        calIntensity = 10;
        calSensitivity = 1600;
    }

    runInfo->param.lIntensity = calIntensity;

    setMetaCtlIso(&runInfo->metaInfo, AA_ISOMODE_MANUAL, calSensitivity);
#ifdef USE_LSI_3A
    runInfo->metaInfo.ctl.aa.aeMode = AA_AEMODE_CENTER;
#endif

    /* adjust turning parameter : exposure */
    if (exposureTime < (1000000/15)*1000 ) {
        calExposureTime = (1000000/120) * 1000;
    } else if (exposureTime >= (1000000/15)*1000 && exposureTime < (1000000/30)*1000 ) {
        calExposureTime = (1000000/120) * 1000;
    } else {
        calExposureTime = (1000000/120) * 1000;
    }

    runInfo->metaInfo.shot.ctl.sensor.exposureTime = calExposureTime;
    runInfo->metaInfo.shot.ctl.aa.aeMode = AA_AEMODE_OFF;
    setMetaCtlAeRegion(&runInfo->metaInfo, 0, 0, 0, 0, 0);

    CLOGD("INFO(%s[%d]): calSensitivity(%d) calExposureTime(%llu)", __FUNCTION__, __LINE__, calSensitivity, calExposureTime);

    m_runInfo.cameraParam.fISO = calSensitivity;
    m_runInfo.cameraParam.fExpoTime = calExposureTime;

    m_runInfo.param.camParam.fISO = calSensitivity;
    m_runInfo.param.camParam.fExpoTime = calExposureTime;


    EXYNOS_CAMERA_ANTISHAKE_TRACE_OUT();
    return ret;
}

status_t ARCSoftAntiShake::m_prepare()
{
    EXYNOS_CAMERA_ANTISHAKE_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);

    status_t ret = NO_ERROR;
    CLOGD("DEBUG(%s[%d]): m_prepare preview count is (%d)", __FUNCTION__, __LINE__, m_setupInfo.adaptiveMaxBufCount[SFL::TYPE_PREVIEW][SFL::POS_SRC]);
    memcpy(m_setupInfo.maxBufCount, m_setupInfo.adaptiveMaxBufCount, sizeof(m_setupInfo.adaptiveMaxBufCount));

    EXYNOS_CAMERA_ANTISHAKE_TRACE_OUT();
    return ret;
}

status_t ARCSoftAntiShake::m_setMetaInfo(void* param)
{
    EXYNOS_CAMERA_ANTISHAKE_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);

    status_t ret = NO_ERROR;
    struct camera2_shot_ext *shot_ext = NULL;
    uint64_t    exposureTime;
    uint32_t    sensitivity;
    int previewCount = 0;

    shot_ext = (struct camera2_shot_ext*)param;

    exposureTime = shot_ext->shot.ctl.sensor.exposureTime;
    sensitivity  = shot_ext->shot.ctl.sensor.sensitivity;

    /* fixed preview count. */
    previewCount = 1;

    if (m_setupInfo.adaptiveMetaInfo.shot.dm.sensor.sensitivity != sensitivity || m_setupInfo.adaptiveMetaInfo.shot.dm.sensor.exposureTime != exposureTime) {
        CLOGD("INFO(%s[%d]): changed sensitivity(%u) exposureTime(%llu) previewCount(%d)", __FUNCTION__, __LINE__, sensitivity, exposureTime, previewCount);
        m_setupInfo.adaptiveMetaInfo.shot.dm.sensor.sensitivity  = sensitivity;
        m_setupInfo.adaptiveMetaInfo.shot.dm.sensor.exposureTime = exposureTime;
    }

    m_setupInfo.adaptiveMaxBufCount[SFL::TYPE_PREVIEW][SFL::POS_SRC] = previewCount;
    m_setupInfo.adaptiveMaxBufCount[SFL::TYPE_CAPTURE][SFL::POS_SRC] = 1;
    m_setupInfo.adaptiveMaxBufCount[SFL::TYPE_CAPTURE][SFL::POS_DST] = 1;

    EXYNOS_CAMERA_ANTISHAKE_TRACE_OUT();
    return ret;
}

status_t ARCSoftAntiShake::m_reset()
{
    EXYNOS_CAMERA_ANTISHAKE_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);

    status_t ret = NO_ERROR;
    ExynosCameraSFLInfoType item = NULL;

    /* 1. reset all runinfo / setupInfo */
    memset(&m_runInfo.previewInput, 0x00, sizeof(m_runInfo.previewInput));
    memset(&m_runInfo.previewImgs, 0x00, sizeof(m_runInfo.previewImgs));
    memset(&m_runInfo.srcCaptureImg, 0x00, sizeof(m_runInfo.srcCaptureImg));
    memset(&m_runInfo.dstCaptureImg, 0x00, sizeof(m_runInfo.dstCaptureImg));
    memset(&m_runInfo.param, 0x00, sizeof(m_runInfo.param));
    memset(&m_runInfo.cameraParam, 0x00, sizeof(m_runInfo.cameraParam));
    memset(&m_runInfo.metaInfo, 0x00, sizeof(m_runInfo.metaInfo));

    for(list<ExynosCameraSFLInfoType>::iterator iter = m_runInfo.mRunSflList.begin() ; iter != m_runInfo.mRunSflList.end() ; )
    {
        item = *iter;
        iter = m_runInfo.mRunSflList.erase(iter);
        item = NULL;
    }
    m_runInfo.mRunSflList.clear();

    for(list<ExynosCameraSFLInfoType>::iterator iter = m_setupInfo.previewInBufs.begin() ; iter != m_setupInfo.previewInBufs.end() ; )
    {
        item = *iter;
        iter = m_setupInfo.previewInBufs.erase(iter);
        item = NULL;
    }
    m_setupInfo.previewInBufs.clear();

    for(list<ExynosCameraSFLInfoType>::iterator iter = m_setupInfo.captureInBufs.begin() ; iter != m_setupInfo.captureInBufs.end() ; )
    {
        item = *iter;
        iter = m_setupInfo.captureInBufs.erase(iter);
        item = NULL;
    }
    m_setupInfo.captureInBufs.clear();

    for(list<ExynosCameraSFLInfoType>::iterator iter = m_setupInfo.captureOutBufs.begin() ; iter != m_setupInfo.captureOutBufs.end() ; )
    {
        item = *iter;
        iter = m_setupInfo.captureOutBufs.erase(iter);
        item = NULL;
    }
    m_setupInfo.captureOutBufs.clear();

    memset(&m_setupInfo.curBufCount, 0x00, sizeof(m_setupInfo.curBufCount));
    memset(&m_setupInfo.maxBufCount, 0x00, sizeof(m_setupInfo.maxBufCount));
    memset(&m_setupInfo.curProgress, 0x00, sizeof(m_setupInfo.curProgress));
    memset(&m_setupInfo.maxProgress, 0x00, sizeof(m_setupInfo.maxProgress));
    memset(&m_setupInfo.adaptiveMaxBufCount, 0x00, sizeof(m_setupInfo.adaptiveMaxBufCount));
    memset(&m_setupInfo.adaptiveMetaInfo, 0x00, sizeof(m_setupInfo.adaptiveMetaInfo));
    memset(&m_setupInfo.bestFrameInfo, 0x00, sizeof(m_setupInfo.bestFrameInfo));

    /* 2. set data depedency */
    for(int i = 0 ; i < ANTISHAKE_PREVIEW_INPUT_IMAGES_MAX ; i++ ) {
        m_runInfo.previewInput.pImages[i] = &m_runInfo.previewImgs[i];
    }

    EXYNOS_CAMERA_ANTISHAKE_TRACE_OUT();
    return ret;
}

void ARCSoftAntiShake::m_setEnable(bool enable)
{
    EXYNOS_CAMERA_ANTISHAKE_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);

    m_enable = enable;

    EXYNOS_CAMERA_ANTISHAKE_TRACE_OUT();
}

bool ARCSoftAntiShake::m_getEnable()
{
    EXYNOS_CAMERA_ANTISHAKE_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);

    bool ret = false;

    ret = m_enable;

    EXYNOS_CAMERA_ANTISHAKE_TRACE_OUT();
    return ret;
}

void ARCSoftAntiShake::m_setRunEnable(bool enable)
{
    EXYNOS_CAMERA_ANTISHAKE_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);

    m_runningEnable = enable;

    EXYNOS_CAMERA_ANTISHAKE_TRACE_OUT();
}

bool ARCSoftAntiShake::m_getRunEnable()
{
    EXYNOS_CAMERA_ANTISHAKE_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);
    bool ret = false;

    ret = m_runningEnable;

    EXYNOS_CAMERA_ANTISHAKE_TRACE_OUT();
    return ret;
}


status_t ARCSoftAntiShake::m_setParam(uint32_t *param, void* arg)
{
    EXYNOS_CAMERA_ANTISHAKE_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);
    uint32_t value = 0;
    status_t ret = NO_ERROR;

    value = *(uint32_t*)arg;
    *param = value;

    EXYNOS_CAMERA_ANTISHAKE_TRACE_OUT();
    return ret;
}

status_t ARCSoftAntiShake::m_getParam(uint32_t *param, void* arg)
{
    EXYNOS_CAMERA_ANTISHAKE_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);
    status_t ret = NO_ERROR;

    *(uint32_t*)arg = *param;

    EXYNOS_CAMERA_ANTISHAKE_TRACE_OUT();
    return ret;
}

status_t ARCSoftAntiShake::m_setConfigData(__unused struct CommandInfo *cmdinfo, void* arg)
{
    EXYNOS_CAMERA_ANTISHAKE_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);
    status_t ret = NO_ERROR;

    ExynosSensorInfoBase *sensor_static = (struct ExynosSensorInfoBase*)arg;

    memcpy(&m_sensorInfo, sensor_static, sizeof(m_sensorInfo));

    EXYNOS_CAMERA_ANTISHAKE_TRACE_OUT();
    return ret;
}

status_t ARCSoftAntiShake::m_getAdjustDataInfo(__unused struct CommandInfo *cmdinfo, void* arg)
{
    EXYNOS_CAMERA_ANTISHAKE_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);
    status_t ret = NO_ERROR;

    struct camera2_shot_ext *shot_ext = NULL;

    shot_ext = (struct camera2_shot_ext*)arg;

    memcpy(shot_ext, &m_runInfo.metaInfo, sizeof(struct camera2_shot_ext));

    EXYNOS_CAMERA_ANTISHAKE_TRACE_OUT();
    return ret;
}

}; //namespace android
