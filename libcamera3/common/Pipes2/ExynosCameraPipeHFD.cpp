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
#define LOG_TAG "ExynosCameraPipeHFD"

#include "ExynosCameraPipeHFD.h"

namespace android {

status_t ExynosCameraPipeHFD::create(__unused int32_t *sensorIds)
{
    CLOGD("-IN-");
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    if (m_checkState(PIPE_STATE_CREATE) == false) {
        CLOGE("Invalid curState %d", m_state);
        return INVALID_OPERATION;
    }

    char hfdLibPath[] = HFD_LIBRARY_PATH;

    ExynosCameraSWPipe::create(sensorIds);

    m_hfdDl = NULL;
    m_hfdHandle = NULL;
    createHfdCnnLib = NULL;
    destroyHfdCnnLib = NULL;
    handleHfdFrame = NULL;

    m_hfdDl = dlopen(hfdLibPath, RTLD_NOW);
    if (m_hfdDl == NULL) {
        CLOGE("Failed to open HFD Library. path %s", hfdLibPath);
        return INVALID_OPERATION;
    }

    createHfdCnnLib = (void* (*)(void)) dlsym(m_hfdDl, "Create_HFD_CnnLib");
    if (dlerror() != NULL && createHfdCnnLib == NULL) {
        CLOGE("Failed to dlsym Create_HFD_CnnLib");
        goto err_exit;
    }

    destroyHfdCnnLib = (void (*)(void *)) dlsym(m_hfdDl, "Destroy_HFD_CnnLib");
    if (dlerror() != NULL && destroyHfdCnnLib == NULL) {
        CLOGE("Failed to dlsym Destroy_HFD_CnnLib");
        goto err_exit;
    }

    handleHfdFrame = (bool (*)(void*, FrameStr*, FrameProperties*, bool, const Vector<struct FacesStr>&, Vector<struct FacesStr>&)) dlsym(m_hfdDl, "HFD_HandleFrame");
    if (dlerror() != NULL && handleHfdFrame== NULL) {
        CLOGE("Failed to dlsym HFD_HandleFrame");
        goto err_exit;
    }

    m_transitState(PIPE_STATE_CREATE);

    return NO_ERROR;
err_exit:
    if (m_hfdDl != NULL) {
        dlclose(m_hfdDl);
    }

    return INVALID_OPERATION;
}

status_t ExynosCameraPipeHFD::start(void)
{
    CLOGD("-IN-");
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    if (m_checkState(PIPE_STATE_INIT) == false) {
        CLOGE("Invalid curState %d", m_state);
        return INVALID_OPERATION;
    }

    m_hfdHandle = (*createHfdCnnLib)();

    m_transitState(PIPE_STATE_INIT);

    return NO_ERROR;
}

status_t ExynosCameraPipeHFD::stop(void)
{
    CLOGD("-IN-");
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    if (m_checkState(PIPE_STATE_CREATE) == false) {
        CLOGE("Invalid curState %d", m_state);
        return INVALID_OPERATION;
    }

    m_mainThread->requestExitAndWait();

    CLOGD("thead exited");

    m_inputFrameQ->release();

    if (m_hfdHandle == NULL) {
        CLOGW("hfdHandle is already destroyed");
    } else {
        (*destroyHfdCnnLib)(m_hfdHandle);
        m_hfdHandle = NULL;
    }

    m_transitState(PIPE_STATE_CREATE);

    return NO_ERROR;
}

status_t ExynosCameraPipeHFD::m_destroy(void)
{
    CLOGD("-IN-");
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    ExynosCameraSWPipe::m_destroy();

    if (m_hfdDl != NULL) {
        dlclose(m_hfdDl);
        m_hfdDl = NULL;
    }

    m_transitState(PIPE_STATE_NONE);

    return NO_ERROR;
}

status_t ExynosCameraPipeHFD::m_run(void)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraBuffer buffer;
    struct camera2_shot_ext frameShotExt;
    struct camera2_shot_ext *bufferShotExt = NULL;
    entity_buffer_state_t bufferState;

    bool hfdRet = false;
    struct FrameStr frameStr;
    struct FrameProperties properties;
    Vector<struct FacesStr> facesIn;
    Vector<struct FacesStr> facesOut;

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

    /* Skip delayed frames */
    while (m_inputFrameQ->getSizeOfProcessQ() > 1) {
        ret = m_inputFrameQ->popProcessQ(&newFrame);
        if (ret != NO_ERROR) {
            CLOGE("Failed to popProcessQ. ret %d", ret);
            return ret;
        } else if (newFrame == NULL) {
            CLOGE("frame is NULL");
            continue;
        }

        /* Clear VRA's face detection dm */
        newFrame->getMetaData(&frameShotExt);
        frameShotExt.shot.dm.stats.faceDetectMode = FACEDETECT_MODE_OFF;
        for (int index = 0; index < NUM_OF_DETECTED_FACES; index++) {
            frameShotExt.shot.dm.stats.faceIds[index] = 0;
            frameShotExt.shot.dm.stats.faceScores[index] = 0;
            frameShotExt.shot.dm.stats.faceRectangles[index][X1] = 0;
            frameShotExt.shot.dm.stats.faceRectangles[index][Y1] = 0;
            frameShotExt.shot.dm.stats.faceRectangles[index][X2] = 0;
            frameShotExt.shot.dm.stats.faceRectangles[index][Y2] = 0;
        }

        /* Restore the latest face detection dm */
        if (m_parameters->checkFaceDetectMeta(&frameShotExt) == true) {
            newFrame->storeDynamicMeta(&(frameShotExt.shot.dm));
        }

        ret = newFrame->setEntityState(getPipeId(), ENTITY_STATE_FRAME_DONE);
        if (ret != NO_ERROR) {
            CLOGE("[F%d]Failed to setEntityState with FRAME_DONE. ret %d",
                    newFrame->getFrameCount(), ret);
            return ret;
        }

        m_outputFrameQ->pushProcessQ(&newFrame);
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

    bufferShotExt = (struct camera2_shot_ext *)buffer.addr[buffer.getMetaPlaneIndex()];
    if (bufferShotExt == NULL) {
        CLOGE("[F%d B%d]shot_ext is NULL", newFrame->getFrameCount(), buffer.index);
    }

    frameStr.yPtr = (unsigned char *)buffer.addr[0];
    frameStr.yLength = buffer.size[0];
    properties.width = bufferShotExt->node_group.leader.output.cropRegion[2];
    properties.height = bufferShotExt->node_group.leader.output.cropRegion[3];

    CLOGV("[F%d B%d]param frame: YPtr %p YLength %d size %dx%d",
            newFrame->getFrameCount(), buffer.index,
            frameStr.yPtr, frameStr.yLength,
            properties.width, properties.height);

    ret = newFrame->getMetaData(&frameShotExt);
    if (ret != NO_ERROR) {
        CLOGE("[F%d]Failed to getMetaData from frame. ret %d",
                newFrame->getFrameCount(), ret);
    }

    for (int index = 0; index < NUM_OF_DETECTED_FACES; index++) {
        if (frameShotExt.shot.dm.stats.faceScores[index] > 0) {
            struct FacesStr facesStr;

            facesStr.id = frameShotExt.shot.dm.stats.faceIds[index];
            facesStr.score = (float)frameShotExt.shot.dm.stats.faceScores[index];
            facesStr.rect.x1 = frameShotExt.shot.dm.stats.faceRectangles[index][X1];
            facesStr.rect.y1 = frameShotExt.shot.dm.stats.faceRectangles[index][Y1];
            facesStr.rect.x2 = frameShotExt.shot.dm.stats.faceRectangles[index][X2];
            facesStr.rect.y2 = frameShotExt.shot.dm.stats.faceRectangles[index][Y2];
            facesStr.isRot = frameShotExt.hfd.is_rot[index];
            facesStr.isYaw = frameShotExt.hfd.is_yaw[index];
            facesStr.rot = frameShotExt.hfd.rot[index];
            facesStr.mirrorX = frameShotExt.hfd.mirror_x[index];
            facesStr.hwRotAndMirror = frameShotExt.hfd.hw_rot_mirror[index];

            CLOGV("[F%d B%d]param facesIn: ID %d Score %f Rect %d,%d %d,%d IsRot %d IsYaw %d ROT %d MirrorX %d HwRotAndMirror %d",
                    newFrame->getFrameCount(), buffer.index,
                    facesStr.id,
                    facesStr.score,
                    facesStr.rect.x1,
                    facesStr.rect.y1,
                    facesStr.rect.x2,
                    facesStr.rect.y2,
                    facesStr.isRot,
                    facesStr.isYaw,
                    facesStr.rot,
                    facesStr.mirrorX,
                    facesStr.hwRotAndMirror);
                    
            facesIn.add(facesStr);

            /* Initialize FD metadata */
            frameShotExt.shot.dm.stats.faceIds[index] = 0;
            frameShotExt.shot.dm.stats.faceScores[index] = 0;
            frameShotExt.shot.dm.stats.faceRectangles[index][X1] = 0;
            frameShotExt.shot.dm.stats.faceRectangles[index][Y1] = 0;
            frameShotExt.shot.dm.stats.faceRectangles[index][X2] = 0;
            frameShotExt.shot.dm.stats.faceRectangles[index][Y2] = 0;
        }
    }

    m_timer.start();
    hfdRet = (*handleHfdFrame)(m_hfdHandle, &frameStr, &properties, /* resetTrackerDB */false,
                               facesIn, facesOut);
    m_timer.stop();
    if (hfdRet == false) {
        CLOGE("[F%d B%d]Failed to handleHfdFrame", newFrame->getFrameCount(), buffer.index);
    } else if ((int) m_timer.durationMsecs() > (int) (1000 / frameShotExt.shot.dm.aa.aeTargetFpsRange[0])) {
        CLOGW("[F%d B%d]Too delayed to handleHfdFrame. %d msec",
                newFrame->getFrameCount(), buffer.index,
                (int) m_timer.durationMsecs());
    }

    /* Update frameShotExt with updated face metadata */
    for (size_t index = 0; index < facesOut.size(); index++) {
        struct FacesStr facesStr = facesOut[index];
        frameShotExt.shot.dm.stats.faceIds[index] = facesStr.id;
        frameShotExt.shot.dm.stats.faceScores[index] = (int) facesStr.score;
        frameShotExt.shot.dm.stats.faceRectangles[index][X1] = facesStr.rect.x1;
        frameShotExt.shot.dm.stats.faceRectangles[index][Y1] = facesStr.rect.y1;
        frameShotExt.shot.dm.stats.faceRectangles[index][X2] = facesStr.rect.x2;
        frameShotExt.shot.dm.stats.faceRectangles[index][Y2] = facesStr.rect.y2;

        CLOGV("[F%d B%d]param facesOut:ID %d Score %d Rect %d,%d %d,%d",
                newFrame->getFrameCount(), buffer.index,
                frameShotExt.shot.dm.stats.faceIds[index],
                frameShotExt.shot.dm.stats.faceScores[index],
                frameShotExt.shot.dm.stats.faceRectangles[index][X1],
                frameShotExt.shot.dm.stats.faceRectangles[index][Y1],
                frameShotExt.shot.dm.stats.faceRectangles[index][X2],
                frameShotExt.shot.dm.stats.faceRectangles[index][Y2]);
    }

    m_parameters->checkFaceDetectMeta(&frameShotExt);

    ret = newFrame->storeDynamicMeta(&(frameShotExt.shot.dm));
    if (ret != NO_ERROR) {
        CLOGE("[F%d]Failed to storeDynamicMeta to frame. ret %d",
                newFrame->getFrameCount(), ret);
    }

    CLOGV("[F%d B%d]HFD Processing done. duration %d msec",
            newFrame->getFrameCount(), buffer.index,
            (int) m_timer.durationMsecs());

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

void ExynosCameraPipeHFD::m_init(void)
{
    /* HFD is one-shot mode */
    m_reprocessing = true;

    createHfdCnnLib = NULL;
    destroyHfdCnnLib = NULL;
    handleHfdFrame = NULL;
    m_hfdDl = NULL;
    m_hfdHandle = NULL;
}

}; /* namespace android */
