/*
**
** Copyright 2017, Samsung Electronics Co. LTD
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

/* #define LOG_NDEBUG 0 */
#define LOG_TAG "ExynosCameraPipeGMV"

#include "ExynosCameraPipeGMV.h"

namespace android {

status_t ExynosCameraPipeGMV::create(__unused int32_t *sensorIds)
{
    CLOGD("-IN-");
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    if (m_checkState(PIPE_STATE_CREATE) == false) {
        CLOGE("Invalid curState %d", m_state);
        return INVALID_OPERATION;
    }

    char gmvLibPath[] = GMV_LIBRARY_PATH;

    ExynosCameraSWPipe::create(sensorIds);

    m_gmvDl = NULL;
    getGlobalMotionEst = NULL;

    m_gmvDl = dlopen(gmvLibPath, RTLD_NOW);
    if (m_gmvDl == NULL) {
        CLOGE("Failed to open GMV Library. path %s", gmvLibPath);
        return INVALID_OPERATION;
    }

    getGlobalMotionEst = (void (*)(const uint8_t *,
                                   const uint32_t *, const uint32_t *,
                                   int *, int *))
                         dlsym(m_gmvDl, "getGlobalMotionEst");
    void *error = dlerror();
    if (error != NULL && getGlobalMotionEst == NULL) {
        CLOGE("Failed to dlsym getGlobalMotionEst %p/%p", error, getGlobalMotionEst);
        goto err_exit;
    }

    m_transitState(PIPE_STATE_CREATE);

    return NO_ERROR;
err_exit:
    if (m_gmvDl != NULL) {
        dlclose(m_gmvDl);
    }

    return INVALID_OPERATION;
}

status_t ExynosCameraPipeGMV::start(void)
{
    CLOGD("-IN-");
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    if (m_checkState(PIPE_STATE_INIT) == false) {
        CLOGE("Invalid curState %d", m_state);
        return INVALID_OPERATION;
    }

#ifdef DUMP_GMV_INPUT
    char filePath[200];

    /* Open File for prevVecList */
    snprintf(filePath, sizeof(filePath), "%s/GMV_PreviousVectorList_CamID%d_%lld.csv",
            DUMP_GMV_INPUT_PATH, m_cameraId, systemTime(SYSTEM_TIME_MONOTONIC));

    prevVecListFd = fopen(filePath, "w+");
    if (prevVecListFd == NULL) {
        CLOGE("Failed to open file %s", filePath);
        return INVALID_OPERATION;
    }

    CLOGD("Open %s", filePath);

    /* Open FILE for currVecList */
    memset(filePath, 0x00, sizeof(filePath));
    snprintf(filePath, sizeof(filePath), "%s/GMV_CurrentVectorList_CamID%d_%lld.csv",
            DUMP_GMV_INPUT_PATH, m_cameraId, systemTime(SYSTEM_TIME_MONOTONIC));

    currVecListFd = fopen(filePath, "w+");
    if (currVecListFd == NULL) {
        CLOGE("Failed to open file %s", filePath);
        return INVALID_OPERATION;
    }

    CLOGD("Open %s", filePath);
#endif

    m_transitState(PIPE_STATE_INIT);

    return NO_ERROR;
}

status_t ExynosCameraPipeGMV::stop(void)
{
    CLOGD("-IN-");
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    if (m_checkState(PIPE_STATE_CREATE) == false) {
        CLOGE("Invalid curState %d", m_state);
        return INVALID_OPERATION;
    }

    stopThreadAndInputQ(m_mainThread, 1, m_inputFrameQ);
    CLOGD("thead exited");

#ifdef DUMP_GMV_INPUT
    if (prevVecListFd != NULL) {
        CLOGD("Close prevVecListFd");
        fflush(prevVecListFd);
        fclose(prevVecListFd);
        prevVecListFd = NULL;
    }

    if (currVecListFd != NULL) {
        CLOGD("Close currVecListFd");
        fflush(currVecListFd);
        fclose(currVecListFd);
        currVecListFd = NULL;
    }
#endif

    m_transitState(PIPE_STATE_CREATE);

    return NO_ERROR;
}

status_t ExynosCameraPipeGMV::m_destroy(void)
{
    CLOGD("-IN-");
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = NO_ERROR;

    ExynosCameraSWPipe::m_destroy();

    if (m_gmvDl != NULL) {
        dlclose(m_gmvDl);
        m_gmvDl = NULL;
    }

    m_transitState(PIPE_STATE_NONE);

    return NO_ERROR;
}

status_t ExynosCameraPipeGMV::m_run(void)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraBuffer buffer;
    struct camera2_shot_ext frameShotExt;
    entity_buffer_state_t bufferState;

    if (m_checkState(PIPE_STATE_RUN) == false) {
        CLOGE("Invalid curState %d", m_state);
        return INVALID_OPERATION;
    }

    if (m_state != PIPE_STATE_RUN) {
        ret = m_transitState(PIPE_STATE_RUN);
        if (ret != NO_ERROR) {
            CLOGE("Failed to transitState into RUN. ret %d", ret);
            return ret;
        }
    }

    ret = m_inputFrameQ->waitAndPopProcessQ(&newFrame);
    if (ret != NO_ERROR) {
        if (ret == TIMED_OUT) {
            CLOGW("wait timeout");
        } else {
            CLOGE("wait and pop fail. ret %d", ret);
        }

        return ret;
    } else if (newFrame == NULL) {
        CLOGE("new frame is NULL");
        return NO_ERROR;
    }

    ret = newFrame->getSrcBufferState(getPipeId(), &bufferState);
    if (ret != NO_ERROR || bufferState == ENTITY_BUFFER_STATE_ERROR) {
        CLOGE("[F%d]Invalid SrcBufferState %d. ret %d",
                newFrame->getFrameCount(), bufferState, ret);
        goto err_exit;
    }

    ret = newFrame->getSrcBuffer(getPipeId(), &buffer);
    if (ret != NO_ERROR || buffer.index < 0) {
        CLOGE("[F%d B%d]Failed to getSrcBuffer. ret %d",
                newFrame->getFrameCount(), buffer.index, ret);
        goto err_exit;
    }

    ret = newFrame->getMetaData(&frameShotExt);
    if (ret != NO_ERROR) {
        CLOGE("[F%d]Failed to getMetaData from frame. ret %d",
                newFrame->getFrameCount(), ret);
    }

#ifdef DUMP_GMV_INPUT
    ret = m_dumpGmvInput(buffer.addr[0], &frameShotExt);
    if (ret != NO_ERROR) {
        CLOGE("[F%d(%d]Failed to dumpGmvInput. ret %d",
                newFrame->getFrameCount(),
                frameShotExt.shot.dm.request.frameCount,
                ret);
        /* continue */
    }
#endif

    CLOGV("[F%d(%d) B%d]GMV Processing start.",
            newFrame->getFrameCount(),
            frameShotExt.shot.dm.request.frameCount,
            buffer.index);

    m_timer.start();
    getGlobalMotionEst((const uint8_t *) buffer.addr[0],
                       (const uint32_t *) frameShotExt.shot.udm.me.motion_vector,
                       (const uint32_t *) frameShotExt.shot.udm.me.current_patch,
                       (int *) &frameShotExt.shot.uctl.gmvUd.gmX,
                       (int *) &frameShotExt.shot.uctl.gmvUd.gmY);
    m_timer.stop();

    if ((int) m_timer.durationMsecs() > (int) (1000 / frameShotExt.shot.dm.aa.aeTargetFpsRange[0])) {
        CLOGW("[F%d B%d]Too delayed to handleGMVFrame. %d msec",
                newFrame->getFrameCount(), buffer.index,
                (int) m_timer.durationMsecs());
    }

    CLOGV("[F%d(%d) B%d]GMV Processing done. gmv %d,%d duration %d msec",
            newFrame->getFrameCount(), frameShotExt.shot.dm.request.frameCount,
            buffer.index,
            frameShotExt.shot.uctl.gmvUd.gmX, frameShotExt.shot.uctl.gmvUd.gmY,
            (int) m_timer.durationMsecs());

    ret = newFrame->storeDynamicMeta(&(frameShotExt.shot.dm));
    if (ret != NO_ERROR) {
        CLOGE("[F%d]Failed to storeDynamicMeta to frame. ret %d",
                newFrame->getFrameCount(), ret);
    }

    ret = newFrame->setSrcBufferState(getPipeId(), ENTITY_BUFFER_STATE_COMPLETE);
    if (ret != NO_ERROR) {
        CLOGE("[F%d]setSrcBufferState failed. ret %d",
                newFrame->getFrameCount(), ret);
        return ret;
    }

    goto func_exit;

err_exit:
    ret = newFrame->setSrcBufferState(getPipeId(), ENTITY_BUFFER_STATE_ERROR);
    if (ret != NO_ERROR) {
        CLOGE("[F%d]setSrcBufferState failed. ret %d",
                newFrame->getFrameCount(), ret);
        return ret;
    }

func_exit:
    ret = newFrame->setEntityState(getPipeId(), ENTITY_STATE_FRAME_DONE);
    if (ret != NO_ERROR) {
        CLOGE("[F%d]setEntityState failed. ret %d",
                newFrame->getFrameCount(), ret);
        return ret;
    }

    m_outputFrameQ->pushProcessQ(&newFrame);

    return NO_ERROR;
}

void ExynosCameraPipeGMV::m_init(void)
{
#ifdef DUMP_GMV_INPUT
    prevVecListFd = NULL;
    currVecListFd = NULL;
#endif
}

#ifdef DUMP_GMV_INPUT
status_t ExynosCameraPipeGMV::m_dumpGmvInput(const char *pkdQvga, const struct camera2_shot_ext *shotExt)
{
    if (pkdQvga == NULL || shotExt == NULL) {
        CLOGE("Invalid parameters. pkdQvga %p shotExt %p",
                pkdQvga, shotExt);
        return BAD_VALUE;
    } else if (prevVecListFd == NULL || currVecListFd == NULL) {
        CLOGE("FDs are not opened. prevVecListFd %p currVecListFd %p",
                prevVecListFd, currVecListFd);
        return INVALID_OPERATION;
    }

    /* Print sensor frame count */
    fprintf(prevVecListFd, "%d,", shotExt->shot.dm.request.frameCount);
    fprintf(currVecListFd, "%d,", shotExt->shot.dm.request.frameCount);

    /* Print vector list */
    for (int i = 0; i < CAMERA2_MAX_ME_MV; i++) {
        fprintf(prevVecListFd, "%d,", shotExt->shot.udm.me.motion_vector[i]);
        fprintf(currVecListFd, "%d,", shotExt->shot.udm.me.motion_vector[i]);

    }

    /* Print new line */
    fprintf(prevVecListFd, "\n");
    fprintf(currVecListFd, "\n");

    return NO_ERROR;
}
#endif

}; /* namespace android */
