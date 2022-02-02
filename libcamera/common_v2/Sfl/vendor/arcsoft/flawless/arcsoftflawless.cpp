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
 * \file      ARCSoftFlawless.cpp
 * \brief     source file for ARCSoftFlawless
 * \author    suyounglee(suyoung80.lee@samsung.com)
 * \date      2015/11/25
 *
 */

/* #define LOG_NDEBUG 0 */
#define LOG_TAG "ARCSoftFlawless"

#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <android/log.h>
#include <stdlib.h>

#include "arcsoftflawless.h"
#include "fimc-is-metadata.h"

namespace android {

ARCSoftFlawless::ARCSoftFlawless(const char* name, int cameraId):ExynosCameraSFLInterface(name, cameraId)
{
    EXYNOS_CAMERA_FLAWLESS_TRACE_IN();

    /* 1. initalize the member */
    m_enhancer = MNull;

    struct CommandInfo cmdinfo;
    cmdinfo.type = SFL::TYPE_PREVIEW;
    cmdinfo.pos = SFL::POS_SRC;
    cmdinfo.cmd = SFL::CMD_NONE;
    m_reset(&cmdinfo);

    cmdinfo.type = SFL::TYPE_CAPTURE;
    cmdinfo.pos = SFL::POS_SRC;
    cmdinfo.cmd = SFL::CMD_NONE;
    m_reset(&cmdinfo);

    memset(&m_runInfo.parameter, 0x00, sizeof(m_runInfo.parameter));
    memset(&m_runInfo.property, 0x00, sizeof(m_runInfo.property));

    EXYNOS_CAMERA_FLAWLESS_TRACE_OUT();
}

ARCSoftFlawless::~ARCSoftFlawless()
{
    EXYNOS_CAMERA_FLAWLESS_TRACE_IN();
    status_t ret = NO_ERROR;

    struct CommandInfo cmdinfo;
    cmdinfo.type = SFL::TYPE_PREVIEW;
    cmdinfo.pos = SFL::POS_SRC;
    cmdinfo.cmd = SFL::CMD_NONE;
    ret = m_reset(&cmdinfo);
    if (ret != NO_ERROR){
        CLOGE("DEBUG(%s[%d]): m_reset failed = ret(%d) type(%d)",__FUNCTION__, __LINE__, ret, cmdinfo.type);
    }

    cmdinfo.type = SFL::TYPE_CAPTURE;
    cmdinfo.pos = SFL::POS_SRC;
    cmdinfo.cmd = SFL::CMD_NONE;
    ret = m_reset(&cmdinfo);
    if (ret != NO_ERROR){
        CLOGE("DEBUG(%s[%d]): m_reset failed = ret(%d) type(%d)",__FUNCTION__, __LINE__, ret, cmdinfo.type);
    }

    ret = m_deinit();
    if (ret != NO_ERROR){
        CLOGE("DEBUG(%s[%d]): m_deinit failed = %d",__FUNCTION__, __LINE__, ret);
    }

    EXYNOS_CAMERA_FLAWLESS_TRACE_OUT();
}

SFLIBRARY_MGR::SFLType ARCSoftFlawless::getType()
{
    EXYNOS_CAMERA_FLAWLESS_TRACE_IN();

    EXYNOS_CAMERA_FLAWLESS_TRACE_OUT();
    return SFLIBRARY_MGR::FLAWLESS;
}

status_t ARCSoftFlawless::init()
{
    EXYNOS_CAMERA_FLAWLESS_TRACE_IN();

    status_t ret = NO_ERROR;

    ret = m_init();
    if (ret != NO_ERROR){
        CLOGE("DEBUG(%s[%d]): SFLInit error = %d",__FUNCTION__, __LINE__, ret);
    }

    EXYNOS_CAMERA_FLAWLESS_TRACE_OUT();
    return ret;
}

status_t ARCSoftFlawless::deinit()
{
    EXYNOS_CAMERA_FLAWLESS_TRACE_IN();

    status_t ret = NO_ERROR;

    struct CommandInfo cmdinfo;
    cmdinfo.type = SFL::TYPE_PREVIEW;
    cmdinfo.pos = SFL::POS_SRC;
    cmdinfo.cmd = SFL::CMD_NONE;
    ret = m_reset(&cmdinfo);
    if (ret != NO_ERROR){
        CLOGE("DEBUG(%s[%d]): m_reset failed = ret(%d) type(%d)",__FUNCTION__, __LINE__, ret, cmdinfo.type);
    }

    cmdinfo.type = SFL::TYPE_CAPTURE;
    cmdinfo.pos = SFL::POS_SRC;
    cmdinfo.cmd = SFL::CMD_NONE;
    ret = m_reset(&cmdinfo);
    if (ret != NO_ERROR){
        CLOGE("DEBUG(%s[%d]): m_reset failed = ret(%d) type(%d)",__FUNCTION__, __LINE__, ret, cmdinfo.type);
    }

    ret = m_deinit();
    if (ret != NO_ERROR){
        CLOGE("DEBUG(%s[%d]): m_deinit failed = %d",__FUNCTION__, __LINE__, ret);
    }

    EXYNOS_CAMERA_FLAWLESS_TRACE_OUT();
    return ret;
}

status_t ARCSoftFlawless::registerCallbacks()
{
    EXYNOS_CAMERA_FLAWLESS_TRACE_IN();
    status_t ret = NO_ERROR;

    EXYNOS_CAMERA_FLAWLESS_TRACE_OUT();
    return ret;
}

status_t ARCSoftFlawless::prepare()
{
    EXYNOS_CAMERA_FLAWLESS_TRACE_IN();
    status_t ret = NO_ERROR;

    ret = m_prepare();

    EXYNOS_CAMERA_FLAWLESS_TRACE_OUT();
    return ret;
}

status_t ARCSoftFlawless::setMetaInfo(void* param)
{
    EXYNOS_CAMERA_FLAWLESS_TRACE_IN();
    status_t ret = NO_ERROR;

    ret = m_setMetaInfo(param);

    EXYNOS_CAMERA_FLAWLESS_TRACE_OUT();
    return ret;
}

status_t ARCSoftFlawless::processCommand(struct CommandInfo *cmdinfo, void* arg)
{
    EXYNOS_CAMERA_FLAWLESS_TRACE_IN();

    status_t ret = NO_ERROR;

    SFLBuffer *bufInfo = NULL;
    uint32_t   value = 0;
    list< ExynosCameraSFLInfoType > *outBufferlist = NULL;

    CLOGV("DEBUG(%s[%d]): command(0x%3x) type(%d) pos(%d)", __FUNCTION__, __LINE__, cmdinfo->cmd, cmdinfo->type, cmdinfo->pos);
    switch(cmdinfo->cmd) {
        case SFL::PROCESS:
            CLOGV("DEBUG(%s[%d]): command(PROCESS) type(%d) pos(%d)", __FUNCTION__, __LINE__, cmdinfo->type, cmdinfo->pos);
            m_convertSetupInfoToRunInfo(cmdinfo, &m_setupInfo, &m_runInfo);
            outBufferlist = (list< ExynosCameraSFLInfoType >*)arg;
            ret = m_processImage(cmdinfo, outBufferlist);
            if (cmdinfo->type ==  SFL::TYPE_PREVIEW) {
                m_reset(cmdinfo);
            }
            break;
        case SFL::ADD_BUFFER:
            CLOGV("DEBUG(%s[%d]): command(ADD_BUFFER) type(%d) pos(%d)", __FUNCTION__, __LINE__, cmdinfo->type, cmdinfo->pos);
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
        case SFL::SET_CONFIGDATA:
            m_setConfigData(cmdinfo, arg);
            if(m_enhancer != MNull) {
                m_updateRunParameter(&m_setupInfo, &m_runInfo);
            }
            break;
        default:
            CLOGD("DEBUG(%s[%d]): invalid command %d ", __FUNCTION__, __LINE__, cmdinfo->cmd);
            ret = BAD_VALUE;
            break;
    }

    EXYNOS_CAMERA_FLAWLESS_TRACE_OUT();
    return ret;
}

void ARCSoftFlawless::setEnable(bool enable)
{
    EXYNOS_CAMERA_FLAWLESS_TRACE_IN();

    m_setEnable(enable);

    EXYNOS_CAMERA_FLAWLESS_TRACE_OUT();
}

bool ARCSoftFlawless::getEnable()
{
    EXYNOS_CAMERA_FLAWLESS_TRACE_IN();
    bool ret = false;

    ret = m_getEnable();

    EXYNOS_CAMERA_FLAWLESS_TRACE_OUT();
    return ret;
}

void ARCSoftFlawless::setRunEnable(bool enable)
{
    EXYNOS_CAMERA_FLAWLESS_TRACE_IN();

    m_setRunEnable(enable);

    EXYNOS_CAMERA_FLAWLESS_TRACE_OUT();
}

bool ARCSoftFlawless::getRunEnable()
{
    EXYNOS_CAMERA_FLAWLESS_TRACE_IN();
    bool ret = false;

    ret = m_getRunEnable();

    EXYNOS_CAMERA_FLAWLESS_TRACE_OUT();
    return ret;
}

void ARCSoftFlawless::reset()
{
    EXYNOS_CAMERA_FLAWLESS_TRACE_IN();

    struct CommandInfo cmdinfo;

    cmdinfo.type = SFL::TYPE_CAPTURE;
    cmdinfo.pos = SFL::POS_SRC;
    cmdinfo.cmd = SFL::CMD_NONE;
    m_reset(&cmdinfo);

    EXYNOS_CAMERA_FLAWLESS_TRACE_OUT();
}

status_t ARCSoftFlawless::m_init()
{
    EXYNOS_CAMERA_FLAWLESS_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);

    MRESULT res = MOK;
    status_t ret = NO_ERROR;

    if(m_enhancer == MNull) {
        res = ArcsoftDetectEngine_Init(&m_enhancer, &m_runInfo.parameter);
        if (res != MOK)
        {
            CLOGE("ERR(%s[%d]):ArcsoftDetectEngine_Init error(%d)", __FUNCTION__, __LINE__, res);
        }
    } else {
        CLOGD("DEBUG(%s[%d]):ArcsoftDetectEngine_Init skip, already initalized engine(%p)", __FUNCTION__, __LINE__, m_enhancer);
    }

    CLOGD("DEBUG(%s[%d]):ArcsoftDetectEngine_Init engine(%p)", __FUNCTION__, __LINE__, m_enhancer);

    const ADE_Version* pVersion = ArcsoftDetectEngine_GetVersion(m_enhancer);
    if (pVersion != NULL)
    {
        CLOGD("DEBUG(%s[%d]:ArcsoftDetectEngine_GetVersion szVersion:%s",__FUNCTION__, __LINE__, pVersion->szVersion);
        CLOGD("DEBUG(%s[%d]:ArcsoftDetectEngine_GetVersion szBuildDate:%s",__FUNCTION__, __LINE__, pVersion->szBuildDate);
        CLOGD("DEBUG(%s[%d]:ArcsoftDetectEngine_GetVersion szCopyRight:%s",__FUNCTION__, __LINE__, pVersion->szCopyRight);
    }

    EXYNOS_CAMERA_FLAWLESS_TRACE_OUT();
    return ret;
}

status_t ARCSoftFlawless::m_deinit()
{
    EXYNOS_CAMERA_FLAWLESS_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);

    status_t ret = NO_ERROR;

    if (m_enhancer != MNull) {
        CLOGD("DEBUG(%s[%d]): ArcsoftDetectEngine_UnInit engine(%p)", __FUNCTION__, __LINE__, m_enhancer);
        ArcsoftDetectEngine_UnInit(m_enhancer);
        m_enhancer = MNull;
    }

    EXYNOS_CAMERA_FLAWLESS_TRACE_OUT();
    return ret;
}

status_t ARCSoftFlawless::m_addSflBuffer(struct CommandInfo *cmdinfo, struct flawless_setupInfo *setupInfo, struct SFLBuffer *buffer)
{
    EXYNOS_CAMERA_FLAWLESS_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);
    ExynosCameraSFLInfoType sflInfo = NULL;
    status_t ret = NO_ERROR;
    int curIndex = 0;
    int maxIndex = 0;
    char *dst = NULL;

    buffer->info.type  = cmdinfo->type;
    buffer->info.pos   = cmdinfo->pos;
    buffer->info.value = cmdinfo->cmd;

    curIndex = setupInfo->curBufCount[cmdinfo->type][cmdinfo->pos];
    maxIndex = setupInfo->maxBufCount[cmdinfo->type][cmdinfo->pos];

    if (curIndex > maxIndex) {
        CLOGE("ERR(%s[%d]): Too many buffer, cur(%d) max(%d)", __FUNCTION__, __LINE__, curIndex, maxIndex);
    }

    sflInfo = new ExynosCameraSFLInfo(buffer);

    switch (cmdinfo->type){
    case SFL::TYPE_CAPTURE:
        if (cmdinfo->pos == SFL::POS_SRC) {
            setupInfo->captureInBufs.push_back(sflInfo);
        } else {
            setupInfo->captureOutBufs.push_back(sflInfo);
        }
        setupInfo->curBufCount[cmdinfo->type][cmdinfo->pos]++;
        break;
    case SFL::TYPE_PREVIEW:
        if (cmdinfo->pos == SFL::POS_SRC) {
            setupInfo->previewInBufs.push_back(sflInfo);
        } else {
            setupInfo->previewOutBufs.push_back(sflInfo);
        }
        setupInfo->curBufCount[cmdinfo->type][cmdinfo->pos]++;
        break;
    case SFL::TYPE_PREVIEW_CB:
    case SFL::TYPE_RECORDING:
    default:
        CLOGE("ERR(%s[%d]): invalid buffer type(%d) ", __FUNCTION__, __LINE__, cmdinfo->type);
        ret = INVALID_OPERATION;
        break;
    }

    EXYNOS_CAMERA_FLAWLESS_TRACE_OUT();
    return ret;
}

status_t ARCSoftFlawless::m_addRunBuffer(ASVLOFFSCREEN* outBufferlist, list<ExynosCameraSFLInfoType> *runSflList, int outBufCount, list<ExynosCameraSFLInfoType> *inBufferlist)
{
    EXYNOS_CAMERA_FLAWLESS_TRACE_IN();
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

    EXYNOS_CAMERA_FLAWLESS_TRACE_OUT();
    return ret;
}

status_t ARCSoftFlawless::m_processImage(struct CommandInfo *cmdinfo, list<ExynosCameraSFLInfoType> *outBufferlist)
{
    EXYNOS_CAMERA_FLAWLESS_TRACE_IN();
    Mutex::Autolock lock(m_processRunLock[cmdinfo->type]);

    MRESULT res = MOK;
    status_t ret = NO_ERROR;
    switch(cmdinfo->type) {
        case SFL::TYPE_PREVIEW:
            /* 1. todo : adjust turnning parameter */
            CLOGV("DEBUG(%s[%d]): m_enhancer(%p)", __FUNCTION__, __LINE__, m_enhancer);
            /* 2. run the algorithm */
            res = ArcsoftDetectEngine_Process(m_enhancer,  &m_runInfo.previewInput, &m_runInfo.previewOutput);
            if(MOK != res) {
                ret = INVALID_OPERATION;
                CLOGE("ERR(%s[%d]): ArcsoftDetectEngine_Process failed, error(%d)", __FUNCTION__, __LINE__, res);
            }
            CLOGV("DEBUG(%s[%d]): m_enhancer(%p) m_runInfo.previewOutput.lBeautyRes(%d) ", __FUNCTION__, __LINE__, m_enhancer, m_runInfo.previewOutput.lBeautyRes);

            /* 3. copy the running bufinfo to outputlist */
            outBufferlist->clear();
            outBufferlist->assign(m_runInfo.mRunSflList[cmdinfo->type].begin(), m_runInfo.mRunSflList[cmdinfo->type].end());
            break;
        case SFL::TYPE_CAPTURE:
            /* 1. todo : adjust turnning parameter */
            CLOGV("DEBUG(%s[%d]): m_enhancer(%p)", __FUNCTION__, __LINE__, m_enhancer);

            /* 2. run the algorithm */
            res = ArcsoftDetectEngine_BeautyForCapture(m_enhancer,  &m_runInfo.captureInput, &m_runInfo.captureOutput);
            if(MOK != res) {
                ret = INVALID_OPERATION;
                CLOGE("ERR(%s[%d]): ArcsoftDetectEngine_BeautyForCapture failed, error(%d)", __FUNCTION__, __LINE__, res);
            }

            /* 3. copy the running bufinfo to outputlist */
            outBufferlist->clear();
            outBufferlist->assign(m_runInfo.mRunSflList[cmdinfo->type].begin(), m_runInfo.mRunSflList[cmdinfo->type].end());
            break;
        default:
            break;
    }


    EXYNOS_CAMERA_FLAWLESS_TRACE_OUT();
    return res;
}

status_t ARCSoftFlawless::m_dump(struct CommandInfo *cmdinfo)
{
    EXYNOS_CAMERA_FLAWLESS_TRACE_IN();

    MRESULT ret = MOK;
    ASVLOFFSCREEN *ptr = NULL;

    switch(cmdinfo->type) {
        case SFL::TYPE_PREVIEW:
            ptr = &m_runInfo.previewInput;
            CLOGD("DEBUG(%s[%d]): PREVIEW SRC format(%d), size(%d x %d) ", __FUNCTION__, __LINE__, ptr->u32PixelArrayFormat, ptr->i32Width, ptr->i32Height);
            CLOGD("DEBUG(%s[%d]): PREVIEW SRC Pitch[0]=%d pitch[1]=%d", __FUNCTION__, __LINE__, ptr->pi32Pitch[0], ptr->pi32Pitch[1]);
            CLOGD("DEBUG(%s[%d]): PREVIEW SRC addr[0]=%p addr[1]=%p", __FUNCTION__, __LINE__, ptr->ppu8Plane[0], ptr->ppu8Plane[1]);

            ptr = &m_runInfo.previewOutput.stBeautyImg;
            CLOGD("DEBUG(%s[%d]): PREVIEW DST format(%d), size(%d x %d) ", __FUNCTION__, __LINE__, ptr->u32PixelArrayFormat, ptr->i32Width, ptr->i32Height);
            CLOGD("DEBUG(%s[%d]): PREVIEW DST Pitch[0]=%d pitch[1]=%d", __FUNCTION__, __LINE__, ptr->pi32Pitch[0], ptr->pi32Pitch[1]);
            CLOGD("DEBUG(%s[%d]): PREVIEW DST addr[0]=%p addr[1]=%p", __FUNCTION__, __LINE__, ptr->ppu8Plane[0], ptr->ppu8Plane[1]);
            break;
        case SFL::TYPE_CAPTURE:
            ptr = &m_runInfo.captureInput;
            CLOGD("DEBUG(%s[%d]): CAPTURE SRC format(%d), size(%d x %d) ", __FUNCTION__, __LINE__, ptr->u32PixelArrayFormat, ptr->i32Width, ptr->i32Height);
            CLOGD("DEBUG(%s[%d]): CAPTURE SRC Pitch[0]=%d pitch[1]=%d", __FUNCTION__, __LINE__, ptr->pi32Pitch[0], ptr->pi32Pitch[1]);
            CLOGD("DEBUG(%s[%d]): CAPTURE SRC addr[0]=%p addr[1]=%p", __FUNCTION__, __LINE__, ptr->ppu8Plane[0], ptr->ppu8Plane[1]);

            ptr = &m_runInfo.captureOutput;
            CLOGD("DEBUG(%s[%d]): CAPTURE DST format(%d), size(%d x %d) ", __FUNCTION__, __LINE__, ptr->u32PixelArrayFormat, ptr->i32Width, ptr->i32Height);
            CLOGD("DEBUG(%s[%d]): CAPTURE DST Pitch[0]=%d pitch[1]=%d", __FUNCTION__, __LINE__, ptr->pi32Pitch[0], ptr->pi32Pitch[1]);
            CLOGD("DEBUG(%s[%d]): CAPTURE DST addr[0]=%p addr[1]=%p", __FUNCTION__, __LINE__, ptr->ppu8Plane[0], ptr->ppu8Plane[1]);
            break;
        default:
            CLOGE("ERR(%s[%d]): m_dump failed, invalid type(%d)", __FUNCTION__, __LINE__, cmdinfo->type);
            break;
    }

    EXYNOS_CAMERA_FLAWLESS_TRACE_OUT();
    return ret;
}

status_t ARCSoftFlawless::m_convertSetupInfoToRunInfo(struct CommandInfo *cmdinfo, struct flawless_setupInfo *setupInfo, struct flawless_runInfo *runInfo)
{
    EXYNOS_CAMERA_FLAWLESS_TRACE_IN();

    MRESULT ret = MOK;
    int curProgressCount[SFL::POS_MAX] = {0};
    int maxProgressCount[SFL::POS_MAX] = {0};
    int curBufCount[SFL::POS_MAX] = {0};
    int maxBufCount[SFL::POS_MAX] = {0};
    SFL::BufferPos pos = SFL::POS_SRC;
    SFL::BufferType type = SFL::TYPE_CAPTURE;

    switch(cmdinfo->type) {
        case SFL::TYPE_PREVIEW:
            type = SFL::TYPE_PREVIEW;
            pos  = SFL::POS_SRC;
            /* check the src Buffer error. */
            if (setupInfo->curBufCount[type][pos] != setupInfo->maxBufCount[type][pos] || setupInfo->curProgress[type][pos] != setupInfo->maxProgress[type][pos]) {
                CLOGE("ERR(%s[%d]): insufficient status, maxBufCnt(%d) curBufCnt(%d) maxSelectCnt(%d) curSelectCnt(%d)", __FUNCTION__, __LINE__, setupInfo->maxBufCount[type][pos], setupInfo->curBufCount[type][pos], setupInfo->maxProgress[type][pos], setupInfo->curProgress[type][pos]);
            }

            type = SFL::TYPE_PREVIEW;
            pos  = SFL::POS_DST;
            /* check the dst Buffer error. */
            if (setupInfo->curBufCount[type][pos] != setupInfo->maxBufCount[type][pos] || setupInfo->curProgress[type][pos] != setupInfo->maxProgress[type][pos]) {
                CLOGE("ERR(%s[%d]): insufficient status, maxBufCnt(%d) curBufCnt(%d) maxSelectCnt(%d) curSelectCnt(%d)", __FUNCTION__, __LINE__, setupInfo->maxBufCount[type][pos], setupInfo->curBufCount[type][pos], setupInfo->maxProgress[type][pos], setupInfo->curProgress[type][pos]);
            }

            type = SFL::TYPE_PREVIEW;
            pos  = SFL::POS_SRC;
            m_addRunBuffer(&runInfo->previewInput, &runInfo->mRunSflList[cmdinfo->type], setupInfo->curBufCount[type][pos], &setupInfo->previewInBufs);

            type = SFL::TYPE_PREVIEW;
            pos  = SFL::POS_DST;
            m_addRunBuffer(&runInfo->previewOutput.stBeautyImg, &runInfo->mRunSflList[cmdinfo->type], setupInfo->curBufCount[type][pos], &setupInfo->previewOutBufs);
            break;
        case SFL::TYPE_CAPTURE:

            type = SFL::TYPE_CAPTURE;
            pos  = SFL::POS_SRC;
            /* check the src Buffer error. */
            if (setupInfo->curBufCount[type][pos] != setupInfo->maxBufCount[type][pos] || setupInfo->curProgress[type][pos] != setupInfo->maxProgress[type][pos]) {
                CLOGE("ERR(%s[%d]): insufficient status, maxBufCnt(%d) curBufCnt(%d) maxSelectCnt(%d) curSelectCnt(%d)", __FUNCTION__, __LINE__, setupInfo->maxBufCount[type][pos], setupInfo->curBufCount[type][pos], setupInfo->maxProgress[type][pos], setupInfo->curProgress[type][pos]);
            }

            type = SFL::TYPE_CAPTURE;
            pos  = SFL::POS_DST;
            /* check the dst Buffer error. */
            if (setupInfo->curBufCount[type][pos] != setupInfo->maxBufCount[type][pos] || setupInfo->curProgress[type][pos] != setupInfo->maxProgress[type][pos]) {
                CLOGE("ERR(%s[%d]): insufficient status, maxBufCnt(%d) curBufCnt(%d) maxSelectCnt(%d) curSelectCnt(%d)", __FUNCTION__, __LINE__, setupInfo->maxBufCount[type][pos], setupInfo->curBufCount[type][pos], setupInfo->maxProgress[type][pos], setupInfo->curProgress[type][pos]);
            }

            type = SFL::TYPE_CAPTURE;
            pos  = SFL::POS_SRC;
            m_addRunBuffer(&runInfo->captureInput, &runInfo->mRunSflList[cmdinfo->type], setupInfo->curBufCount[type][pos], &setupInfo->captureInBufs);

            type = SFL::TYPE_CAPTURE;
            pos  = SFL::POS_DST;
            m_addRunBuffer(&runInfo->captureOutput, &runInfo->mRunSflList[cmdinfo->type], setupInfo->curBufCount[type][pos], &setupInfo->captureOutBufs);
            break;
        default:
            break;
    }

    EXYNOS_CAMERA_FLAWLESS_TRACE_OUT();
    return ret;
}

status_t ARCSoftFlawless::m_updateRunParameter(__unused struct flawless_setupInfo *setupInfo, __unused struct flawless_runInfo *runInfo)
{
    EXYNOS_CAMERA_FLAWLESS_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);
    status_t ret = NO_ERROR;

    /* get current parameter */
    ArcsoftDetectEngine_GetProperty(m_enhancer, &m_runInfo.property);

    if (m_runInfo.property.iSkinSoftenLevel    != m_runInfo.parameter.iSkinSoftenLevel  ||
        m_runInfo.property.iSkinBrightLevel    != m_runInfo.parameter.iSkinBrightLevel  ||
        m_runInfo.property.iSlenderFaceLevel   != m_runInfo.parameter.iSlenderFaceLevel ||
        m_runInfo.property.iEyeEnlargmentLevel != m_runInfo.parameter.iEyeEnlargmentLevel) {

        CLOGD("DEBUG(%s[%d]): FeatureNeeded(%d->%d) SkinSoft(%d->%d) SkinBright(%d->%d) slenderFace(%d->%d) eyeEnlarge(%d->%d)", __FUNCTION__, __LINE__,
                              m_runInfo.property.iFeatureNeeded, m_runInfo.parameter.iFeatureNeeded,
                              m_runInfo.property.iSkinSoftenLevel, m_runInfo.parameter.iSkinSoftenLevel,
                              m_runInfo.property.iSkinBrightLevel, m_runInfo.parameter.iSkinBrightLevel,
                              m_runInfo.property.iSlenderFaceLevel, m_runInfo.parameter.iSlenderFaceLevel,
                              m_runInfo.property.iEyeEnlargmentLevel, m_runInfo.parameter.iEyeEnlargmentLevel);

        m_runInfo.property.iSkinSoftenLevel    = m_runInfo.parameter.iSkinSoftenLevel;
        m_runInfo.property.iSkinBrightLevel    = m_runInfo.parameter.iSkinBrightLevel;
        m_runInfo.property.iSlenderFaceLevel   = m_runInfo.parameter.iSlenderFaceLevel;
        m_runInfo.property.iEyeEnlargmentLevel = m_runInfo.parameter.iEyeEnlargmentLevel;
        m_runInfo.property.iFeatureNeeded      = m_runInfo.parameter.iFeatureNeeded;

        /* set current parameter */
        ArcsoftDetectEngine_SetProperty(m_enhancer, &m_runInfo.property);
    }

    EXYNOS_CAMERA_FLAWLESS_TRACE_OUT();
    return ret;
}

status_t ARCSoftFlawless::m_prepare()
{
    EXYNOS_CAMERA_FLAWLESS_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);
    status_t ret = NO_ERROR;

    EXYNOS_CAMERA_FLAWLESS_TRACE_OUT();
    return ret;
}

status_t ARCSoftFlawless::m_setMetaInfo(__unused void* param)
{
    EXYNOS_CAMERA_FLAWLESS_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);

    status_t ret = NO_ERROR;

    CLOGD("INFO(%s[%d]): changed ", __FUNCTION__, __LINE__);

    EXYNOS_CAMERA_FLAWLESS_TRACE_OUT();
    return ret;
}

status_t ARCSoftFlawless::m_reset(struct CommandInfo *cmdinfo)
{
    EXYNOS_CAMERA_FLAWLESS_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);

    status_t ret = NO_ERROR;
    ExynosCameraSFLInfoType item = NULL;

    switch(cmdinfo->type) {
        case SFL::TYPE_PREVIEW:
            /* 1. reset all runinfo / setupInfo */
            memset(&m_runInfo.previewInput, 0x00, sizeof(m_runInfo.previewInput));
            memset(&m_runInfo.previewOutput, 0x00, sizeof(m_runInfo.previewOutput));

            for(list<ExynosCameraSFLInfoType>::iterator iter = m_runInfo.mRunSflList[cmdinfo->type].begin() ; iter != m_runInfo.mRunSflList[cmdinfo->type].end() ; )
            {
                item = *iter;
                iter = m_runInfo.mRunSflList[cmdinfo->type].erase(iter);
                item = NULL;
            }
            m_runInfo.mRunSflList[cmdinfo->type].clear();

            for(list<ExynosCameraSFLInfoType>::iterator iter = m_setupInfo.previewInBufs.begin() ; iter != m_setupInfo.previewInBufs.end() ; )
            {
                item = *iter;
                iter = m_setupInfo.previewInBufs.erase(iter);
                item = NULL;
            }
            m_setupInfo.previewInBufs.clear();

            for(list<ExynosCameraSFLInfoType>::iterator iter = m_setupInfo.previewOutBufs.begin() ; iter != m_setupInfo.previewOutBufs.end() ; )
            {
                item = *iter;
                iter = m_setupInfo.previewOutBufs.erase(iter);
                item = NULL;
            }
            m_setupInfo.previewOutBufs.clear();

            m_setupInfo.curBufCount[cmdinfo->type][SFL::POS_SRC] = 0;
            m_setupInfo.curBufCount[cmdinfo->type][SFL::POS_DST] = 0;
            m_setupInfo.maxBufCount[cmdinfo->type][SFL::POS_SRC] = 1;
            m_setupInfo.maxBufCount[cmdinfo->type][SFL::POS_DST] = 1;

            memset(m_setupInfo.maxProgress[cmdinfo->type], 0x00, sizeof(m_setupInfo.maxProgress[cmdinfo->type]));
            memset(m_setupInfo.curProgress[cmdinfo->type], 0x00, sizeof(m_setupInfo.curProgress[cmdinfo->type]));
            break;
        case SFL::TYPE_CAPTURE:
            /* 1. reset all runinfo / setupInfo */
            memset(&m_runInfo.captureInput, 0x00, sizeof(m_runInfo.captureInput));
            memset(&m_runInfo.captureOutput, 0x00, sizeof(m_runInfo.captureOutput));

            for(list<ExynosCameraSFLInfoType>::iterator iter = m_runInfo.mRunSflList[cmdinfo->type].begin() ; iter != m_runInfo.mRunSflList[cmdinfo->type].end() ; )
            {
                item = *iter;
                iter = m_runInfo.mRunSflList[cmdinfo->type].erase(iter);
                item = NULL;
            }
            m_runInfo.mRunSflList[cmdinfo->type].clear();

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

            m_setupInfo.curBufCount[cmdinfo->type][SFL::POS_SRC] = 0;
            m_setupInfo.curBufCount[cmdinfo->type][SFL::POS_DST] = 0;
            m_setupInfo.maxBufCount[cmdinfo->type][SFL::POS_SRC] = 1;
            m_setupInfo.maxBufCount[cmdinfo->type][SFL::POS_DST] = 1;

            memset(m_setupInfo.maxProgress[cmdinfo->type], 0x00, sizeof(m_setupInfo.maxProgress[cmdinfo->type]));
            memset(m_setupInfo.curProgress[cmdinfo->type], 0x00, sizeof(m_setupInfo.curProgress[cmdinfo->type]));
            break;
        default:
            break;
    }

    EXYNOS_CAMERA_FLAWLESS_TRACE_OUT();
    return ret;
}

void ARCSoftFlawless::m_setEnable(bool enable)
{
    EXYNOS_CAMERA_FLAWLESS_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);

    m_enable = enable;

    EXYNOS_CAMERA_FLAWLESS_TRACE_OUT();
}

bool ARCSoftFlawless::m_getEnable()
{
    EXYNOS_CAMERA_FLAWLESS_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);

    bool ret = false;

    ret = m_enable;

    EXYNOS_CAMERA_FLAWLESS_TRACE_OUT();
    return ret;
}

void ARCSoftFlawless::m_setRunEnable(bool enable)
{
    EXYNOS_CAMERA_FLAWLESS_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);

    m_runningEnable = enable;

    EXYNOS_CAMERA_FLAWLESS_TRACE_OUT();
}

bool ARCSoftFlawless::m_getRunEnable()
{
    EXYNOS_CAMERA_FLAWLESS_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);
    bool ret = false;

    ret = m_runningEnable;

    EXYNOS_CAMERA_FLAWLESS_TRACE_OUT();
    return ret;
}


status_t ARCSoftFlawless::m_setParam(uint32_t *param, void* arg)
{
    EXYNOS_CAMERA_FLAWLESS_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);
    uint32_t value = 0;
    status_t ret = NO_ERROR;

    value = *(uint32_t*)arg;
    *param = value;

    EXYNOS_CAMERA_FLAWLESS_TRACE_OUT();
    return ret;
}

status_t ARCSoftFlawless::m_getParam(uint32_t *param, void* arg)
{
    EXYNOS_CAMERA_FLAWLESS_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);
    status_t ret = NO_ERROR;

    *(uint32_t*)arg = *param;

    EXYNOS_CAMERA_FLAWLESS_TRACE_OUT();
    return ret;
}

status_t ARCSoftFlawless::m_setConfigData(__unused struct CommandInfo *cmdinfo, void* arg)
{
    EXYNOS_CAMERA_FLAWLESS_TRACE_IN();
    Mutex::Autolock lock(m_setupInfoLock);
    status_t ret = NO_ERROR;
    FlawLess::configInfo *param;

    param = (FlawLess::configInfo*)arg;

    if (param->faceBeauty == true)
        m_runInfo.parameter.iFeatureNeeded |= ARCSOFT_FEATURE_FACEBEAUTIFY;

    if (param->slimFace == true)
        m_runInfo.parameter.iFeatureNeeded |= ARCSOFT_FEATURE_SLIM_FACE;

    if (param->skinBright == true)
        m_runInfo.parameter.iFeatureNeeded |= ARCSOFT_FEATURE_SKIN_BRIGHT;

    if (param->eyeEnlargment == true)
        m_runInfo.parameter.iFeatureNeeded |= ARCSOFT_FEATURE_EYE_ENLARGE;

    /* Tunning parameter : fix the FD count for preview performance */
    /*                     ARCSOFT_FACE_BASE_MODE :  iPreviewMaxFaceSupport = 3*/
    /*                     ARCSOFT_SKIN_BASE_MODE :  iPreviewMaxFaceSupport = 1*/
    m_runInfo.parameter.iPreviewMaxFaceSupport = 3;
    m_runInfo.parameter.iCaptureMaxFaceSupport  = MAX_CAPTURE_FACE_NUM;

    if (param->width != (uint32_t)m_runInfo.parameter.i32Width && (uint32_t)param->width != 0) {
        m_runInfo.parameter.i32Width = param->width;
    }

    if (param->height != (uint32_t)m_runInfo.parameter.i32Height && (uint32_t)param->height != 0) {
        m_runInfo.parameter.i32Height = param->height;
    }

    m_runInfo.parameter.u32PixelArrayFormat = ASVL_PAF_NV21;
    m_runInfo.parameter.iSkinSoftenLevel    = param->faceBeautyIntensity;
    m_runInfo.parameter.iSkinBrightLevel    = param->skinBrightIntensity;
    m_runInfo.parameter.iSlenderFaceLevel   = param->slimFaceIntensity;
    m_runInfo.parameter.iEyeEnlargmentLevel = param->eyeEnlargmentIntensity;
    m_runInfo.parameter.iMode = ARCSOFT_SKIN_BASE_MODE;

    EXYNOS_CAMERA_FLAWLESS_TRACE_OUT();
    return ret;
}

}; //namespace android
