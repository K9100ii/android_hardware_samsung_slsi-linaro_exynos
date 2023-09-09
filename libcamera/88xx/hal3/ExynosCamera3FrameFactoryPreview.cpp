/*
**
** Copyright 2015, Samsung Electronics Co. LTD
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
#define LOG_TAG "ExynosCamera3FrameFactoryPreview"
#include <cutils/log.h>

#include "ExynosCamera3FrameFactoryPreview.h"

namespace android {

ExynosCamera3FrameFactoryPreview::~ExynosCamera3FrameFactoryPreview()
{
    status_t ret = NO_ERROR;

    ret = destroy();
    if (ret != NO_ERROR)
        CLOGE("ERR(%s[%d]):destroy fail", __FUNCTION__, __LINE__);
}

status_t ExynosCamera3FrameFactoryPreview::create()
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    status_t ret = NO_ERROR;

    m_setupConfig();
    m_constructMainPipes();

    /* FLITE pipe initialize */
    ret = m_pipes[PIPE_FLITE]->create(m_sensorIds[PIPE_FLITE]);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):FLITE create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGD("DEBUG(%s[%d]):%s(%d) created", __FUNCTION__, __LINE__,
            m_pipes[PIPE_FLITE]->getPipeName(), PIPE_FLITE);

    /* 3AA pipe initialize */
    ret = m_pipes[PIPE_3AA]->create();
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):3AA create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGD("DEBUG(%s[%d]):%s(%d) created", __FUNCTION__, __LINE__,
            m_pipes[PIPE_3AA]->getPipeName(), PIPE_3AA);

    /* ISP pipe initialize */
    ret = m_pipes[PIPE_ISP]->create();
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):ISP create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGD("DEBUG(%s[%d]):%s(%d) created", __FUNCTION__, __LINE__,
            m_pipes[PIPE_ISP]->getPipeName(), PIPE_ISP);

    /* TPU pipe initialize */
    ret = m_pipes[PIPE_TPU]->create();
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):TPU create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGD("DEBUG(%s[%d]):%s(%d) created", __FUNCTION__, __LINE__,
            m_pipes[PIPE_TPU]->getPipeName(), PIPE_TPU);

    /* MCSC pipe initialize */
    ret = m_pipes[PIPE_MCSC]->create();
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):MCSC create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGD("DEBUG(%s[%d]):%s(%d) created", __FUNCTION__, __LINE__,
            m_pipes[PIPE_MCSC]->getPipeName(), PIPE_MCSC);

    /* GSC_PREVIEW pipe initialize */
    ret = m_pipes[PIPE_GSC]->create();
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):GSC create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGD("DEBUG(%s[%d]):%s(%d) created", __FUNCTION__, __LINE__,
            m_pipes[PIPE_GSC]->getPipeName(), PIPE_GSC);

    ret = m_pipes[PIPE_GSC_VIDEO]->create();
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):PIPE_GSC_VIDEO create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGD("DEBUG(%s[%d]):%s(%d) created", __FUNCTION__, __LINE__,
            m_pipes[PIPE_GSC_VIDEO]->getPipeName(), PIPE_GSC_VIDEO);

    if (m_supportReprocessing == false) {
        /* GSC_PICTURE pipe initialize */
        ret = m_pipes[PIPE_GSC_PICTURE]->create();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):GSC_PICTURE create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGD("DEBUG(%s[%d]):%s(%d) created", __FUNCTION__, __LINE__,
                m_pipes[PIPE_GSC_PICTURE]->getPipeName(), PIPE_GSC_PICTURE);

        /* JPEG pipe initialize */
        ret = m_pipes[PIPE_JPEG]->create();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):JPEG create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGD("DEBUG(%s[%d]):%s(%d) created", __FUNCTION__, __LINE__,
                m_pipes[PIPE_JPEG]->getPipeName(), PIPE_JPEG);
    }

    /* EOS */
    ret = m_pipes[PIPE_3AA]->setControl(V4L2_CID_IS_END_OF_STREAM, 1);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):PIPE_%d V4L2_CID_IS_END_OF_STREAM fail, ret(%d)", __FUNCTION__, __LINE__, PIPE_3AA, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ret = m_transitState(FRAME_FACTORY_STATE_CREATE);

    return ret;
}

status_t ExynosCamera3FrameFactoryPreview::precreate(void)
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    status_t ret = NO_ERROR;

    m_setupConfig();
    m_constructMainPipes();

    /* flite pipe initialize */
    ret = m_pipes[PIPE_FLITE]->create(m_sensorIds[PIPE_FLITE]);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):FLITE create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGD("DEBUG(%s[%d]):%s(%d) created", __FUNCTION__, __LINE__,
            m_pipes[PIPE_FLITE]->getPipeName(), PIPE_FLITE);

    /* 3AA pipe initialize */
    ret = m_pipes[PIPE_3AA]->precreate();
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):3AA create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGD("DEBUG(%s[%d]):%s(%d) precreated", __FUNCTION__, __LINE__,
            m_pipes[PIPE_3AA]->getPipeName(), PIPE_3AA);

    /* ISP pipe initialize */
    ret = m_pipes[PIPE_ISP]->precreate();
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):ISP create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGD("DEBUG(%s[%d]):%s(%d) precreated", __FUNCTION__, __LINE__,
            m_pipes[PIPE_ISP]->getPipeName(), PIPE_ISP);

    return NO_ERROR;
}

status_t ExynosCamera3FrameFactoryPreview::postcreate(void)
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);
    status_t ret = NO_ERROR;

    /* 3AA_ISP pipe initialize */
    ret = m_pipes[PIPE_3AA]->postcreate();
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):3AA_ISP postcreate fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGD("DEBUG(%s[%d]):%s(%d) postcreated", __FUNCTION__, __LINE__,
            m_pipes[PIPE_3AA]->getPipeName(), PIPE_3AA);

    /* ISP pipe initialize */
    ret = m_pipes[PIPE_ISP]->postcreate();
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):ISP create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGD("DEBUG(%s[%d]):%s(%d) postcreated", __FUNCTION__, __LINE__,
            m_pipes[PIPE_ISP]->getPipeName(), PIPE_ISP);

    /* TPU pipe initialize */
    ret = m_pipes[PIPE_TPU]->create();
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):TPU create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGD("DEBUG(%s[%d]):%s(%d) created", __FUNCTION__, __LINE__,
            m_pipes[PIPE_TPU]->getPipeName(), PIPE_TPU);

    /* MCSC pipe initialize */
    ret = m_pipes[PIPE_MCSC]->create();
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):MCSC create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGD("DEBUG(%s[%d]):%s(%d) created", __FUNCTION__, __LINE__,
            m_pipes[PIPE_MCSC]->getPipeName(), PIPE_MCSC);

    /* GSC_PREVIEW pipe initialize */
    ret = m_pipes[PIPE_GSC]->create();
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):GSC create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGD("DEBUG(%s[%d]):%s(%d) created", __FUNCTION__, __LINE__,
            m_pipes[PIPE_GSC]->getPipeName(), PIPE_GSC);

    ret = m_pipes[PIPE_GSC_VIDEO]->create();
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):PIPE_GSC_VIDEO create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGD("DEBUG(%s[%d]):%s(%d) created", __FUNCTION__, __LINE__,
            m_pipes[PIPE_GSC_VIDEO]->getPipeName(), PIPE_GSC_VIDEO);

    if (m_supportReprocessing == false) {
        /* GSC_PICTURE pipe initialize */
        ret = m_pipes[PIPE_GSC_PICTURE]->create();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):GSC_PICTURE create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGD("DEBUG(%s[%d]):%s(%d) created", __FUNCTION__, __LINE__,
                m_pipes[PIPE_GSC_PICTURE]->getPipeName(), PIPE_GSC_PICTURE);

        /* JPEG pipe initialize */
        ret = m_pipes[PIPE_JPEG]->create();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):JPEG create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGD("DEBUG(%s[%d]):%s(%d) created", __FUNCTION__, __LINE__,
                m_pipes[PIPE_JPEG]->getPipeName(), PIPE_JPEG);
    }

    /* EOS */
    ret = m_pipes[PIPE_3AA]->setControl(V4L2_CID_IS_END_OF_STREAM, 1);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):PIPE_%d V4L2_CID_IS_END_OF_STREAM fail, ret(%d)", __FUNCTION__, __LINE__, PIPE_3AA, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ret = m_transitState(FRAME_FACTORY_STATE_CREATE);

    return ret;
}

status_t ExynosCamera3FrameFactoryPreview::fastenAeStable(int32_t numFrames, ExynosCameraBuffer *buffers)
{
    CLOGI("INFO(%s[%d]): Start", __FUNCTION__, __LINE__);

    status_t ret = NO_ERROR;
    status_t totalRet = NO_ERROR;

    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraFrameEntity *newEntity = NULL;
    ExynosCameraList<ExynosCameraFrame *> instantQ;

    int hwSensorW = 0, hwSensorH = 0;
    int bcropX = 0, bcropY = 0, bcropW = 0, bcropH = 0;
    int hwPreviewW = 0, hwPreviewH = 0;
    uint32_t minFrameRate, maxFrameRate, sensorFrameRate = 0;
    struct v4l2_streamparm streamParam;

    if (numFrames == 0) {
        CLOGW("WRN(%s[%d]):umFrames is %d, we skip fastenAeStable", __FUNCTION__, __LINE__, numFrames);
        return NO_ERROR;
    }

    /* 1. Initialize pipes */
    m_parameters->getFastenAeStableSensorSize(&hwSensorW, &hwSensorH);
    m_parameters->getFastenAeStableBcropSize(&bcropW, &bcropH);
    m_parameters->getFastenAeStableBdsSize(&hwPreviewW, &hwPreviewH);

    bcropX = ALIGN_UP(((hwSensorW - bcropW) >> 1), 2);
    bcropY = ALIGN_UP(((hwSensorH - bcropH) >> 1), 2);

    if (m_parameters->getCameraId() == CAMERA_ID_FRONT) {
        sensorFrameRate  = FASTEN_AE_FPS_FRONT;
    } else {
        sensorFrameRate  = FASTEN_AE_FPS;
    }

    ret = m_initFlitePipe(hwSensorW, hwSensorH, sensorFrameRate);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_initFlitePipes() fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        return ret;
    }

    ret = m_initPipesFastenAeStable(numFrames, hwSensorW, hwSensorH);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_initPipesFastenAeStable() fail, ret(%d)",
            __FUNCTION__, __LINE__, ret);
        return ret;
    }

    for (int i = 0; i < numFrames; i++) {
        /* 2. Generate instant frames */
        newFrame = m_frameMgr->createFrame(m_parameters, i);

        ret = m_initFrameMetadata(newFrame);
        if (ret != NO_ERROR)
            CLOGE("(%s[%d]): frame(%d) metadata initialize fail", __FUNCTION__, __LINE__, i);

        newEntity = new ExynosCameraFrameEntity(PIPE_3AA, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
        newFrame->addSiblingEntity(NULL, newEntity);
        newFrame->setNumRequestPipe(1);

        newEntity->setSrcBuf(buffers[i]);

        /* 3. Set metadata for instant on */
        camera2_shot_ext *shot_ext = (struct camera2_shot_ext *)(buffers[i].addr[1]);

        if (shot_ext != NULL) {
            int aeRegionX = (hwSensorW) / 2;
            int aeRegionY = (hwSensorH) / 2;

            newFrame->getMetaData(shot_ext);
            m_parameters->duplicateCtrlMetadata((void *)shot_ext);
            m_activityControl->activityBeforeExecFunc(PIPE_3AA, (void *)&buffers[i]);

            setMetaCtlAeTargetFpsRange(shot_ext, sensorFrameRate, sensorFrameRate);
            setMetaCtlSensorFrameDuration(shot_ext, (uint64_t)((1000 * 1000 * 1000) / (uint64_t)sensorFrameRate));

            /* set afMode into INFINITY */
            shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_CANCEL;
            shot_ext->shot.ctl.aa.vendor_afmode_option &= (0 << AA_AFMODE_OPTION_BIT_MACRO);

            setMetaCtlAeRegion(shot_ext, aeRegionX, aeRegionY, aeRegionX, aeRegionY, 0);

            /* Set 3AS size */
            enum NODE_TYPE nodeType = getNodeType(PIPE_3AA);
            int nodeNum = m_deviceInfo[PIPE_3AA].nodeNum[nodeType];
            if (nodeNum <= 0) {
                CLOGE("ERR(%s[%d]): invalid nodeNum(%d). so fail", __FUNCTION__, __LINE__, nodeNum);
                ret = INVALID_OPERATION;
                goto cleanup;
            }

            setMetaNodeLeaderVideoID(shot_ext, nodeNum - FIMC_IS_VIDEO_BAS_NUM);
            setMetaNodeLeaderRequest(shot_ext, false);
            setMetaNodeLeaderInputSize(shot_ext, bcropX, bcropY, bcropW, bcropH);

            /* Set 3AP size */
            nodeType = getNodeType(PIPE_3AP);
            nodeNum = m_deviceInfo[PIPE_3AA].nodeNum[nodeType];
            if (nodeNum <= 0) {
                CLOGE("ERR(%s[%d]): invalid nodeNum(%d). so fail", __FUNCTION__, __LINE__, nodeNum);
                ret = INVALID_OPERATION;
                goto cleanup;
            }

            int perframePosition = 0;
            setMetaNodeCaptureVideoID(shot_ext, perframePosition, nodeNum - FIMC_IS_VIDEO_BAS_NUM);
            setMetaNodeCaptureRequest(shot_ext, perframePosition, false);
            setMetaNodeCaptureOutputSize(shot_ext, perframePosition, 0, 0, hwPreviewW, hwPreviewH);

            /* Set ISPC/ISPP size (optional) */
            if (m_flag3aaIspOTF == true) {
                /* Case of ISP-MCSC OTF or ISP-TPU-MCSC OTF */
                if (m_flagIspMcscOTF == true
                    || (m_flagIspTpuOTF == true && m_flagTpuMcscOTF == true))
                    nodeType = getNodeType(PIPE_MCSC0);
                /* Case of ISP-TPU M2M */
                else if (m_parameters->getTpuEnabledMode() == true && m_flagIspTpuOTF == false)
                    nodeType = getNodeType(PIPE_ISPP);
                /* Case of ISP-MCSC M2M */
                else
                    nodeType = getNodeType(PIPE_ISPC);

                nodeNum = m_deviceInfo[PIPE_3AA].nodeNum[nodeType];
                if (nodeNum <= 0) {
                    CLOGE("ERR(%s[%d]): invalid nodeNum(%d). so fail", __FUNCTION__, __LINE__, nodeNum);
                    ret = INVALID_OPERATION;
                    goto cleanup;
                }

                perframePosition = 1; /* 3AP:0, ISPC/ISPP:1 */
                setMetaNodeCaptureVideoID(shot_ext, perframePosition, nodeNum - FIMC_IS_VIDEO_BAS_NUM);
                setMetaNodeCaptureRequest(shot_ext, perframePosition, false);
                setMetaNodeCaptureOutputSize(shot_ext, perframePosition, 0, 0, hwPreviewW, hwPreviewH);
            }
        }

        /* 4. Push instance frames to pipe */
        ret = pushFrameToPipe(&newFrame, PIPE_3AA);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]): pushFrameToPipeFail, ret(%d)", __FUNCTION__, __LINE__, ret);
            goto cleanup;
        }
        CLOGD("DEBUG(%s[%d]): Instant shot - FD(%d, %d)", __FUNCTION__, __LINE__, buffers[i].fd[0], buffers[i].fd[1]);

        instantQ.pushProcessQ(&newFrame);
    }

    /* 5. Pipe instant on */
    ret = m_pipes[PIPE_FLITE]->instantOn(0);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]): FLITE On fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        goto cleanup;
    }

    if (m_flagIspMcscOTF == false && m_flagTpuMcscOTF == false) {
        ret = m_pipes[PIPE_MCSC]->start();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):MCSC start fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            goto cleanup;
        }
    }

    if (m_parameters->getTpuEnabledMode() == true && m_flagIspTpuOTF == false) {
        ret = m_pipes[PIPE_TPU]->start();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):TPU start fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            goto cleanup;
        }
    }

    if (m_flag3aaIspOTF == false) {
        ret = m_pipes[INDEX(PIPE_ISP)]->start();
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):ISP start fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            goto cleanup;
        }
    }

    ret = m_pipes[PIPE_3AA]->instantOn(numFrames);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):3AA instantOn fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        goto cleanup;
    }

    /* 6. SetControl to sensor instant on */
    ret = m_pipes[PIPE_FLITE]->setControl(V4L2_CID_IS_S_STREAM, (1 | numFrames << SENSOR_INSTANT_SHIFT));
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):instantOn fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        goto cleanup;
    }

cleanup:
    totalRet |= ret;

    /* 7. Pipe instant off */
    ret = m_pipes[PIPE_FLITE]->instantOff();
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]): FLITE Off fail, ret(%d)", __FUNCTION__, __LINE__, ret);
    }

    ret = m_pipes[PIPE_3AA]->forceDone(V4L2_CID_IS_FORCE_DONE, 0x1000);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):3AA force done fail, ret(%d)", __FUNCTION__, __LINE__, ret);
    }

    ret = m_pipes[PIPE_3AA]->instantOff();
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):3AA instantOff fail, ret(%d)", __FUNCTION__, __LINE__, ret);
    }

    if (m_flag3aaIspOTF == false) {
        ret = m_pipes[INDEX(PIPE_ISP)]->stop();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):ISP stop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }
    }

    if (m_parameters->getTpuEnabledMode() == true && m_flagIspTpuOTF == false) {
        ret = m_pipes[PIPE_TPU]->stop();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):TPU stop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }
    }

    if (m_flagIspMcscOTF == false && m_flagTpuMcscOTF == false) {
        ret = m_pipes[PIPE_MCSC]->stop();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):MCSC stop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }
    }

    /* 8. Rollback framerate after fastenfeenable done */
    /* setParam for Frame rate : must after setInput on Flite */
    memset(&streamParam, 0x0, sizeof(v4l2_streamparm));

    m_parameters->getPreviewFpsRange(&minFrameRate, &maxFrameRate);
    if (m_parameters->getScalableSensorMode() == true)
        sensorFrameRate = 24;
    else
        sensorFrameRate = maxFrameRate;

    streamParam.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    streamParam.parm.capture.timeperframe.numerator   = 1;
    streamParam.parm.capture.timeperframe.denominator = sensorFrameRate;
    CLOGI("INFO(%s[%d]:set framerate (denominator=%d)", __FUNCTION__, __LINE__, sensorFrameRate);
    ret = setParam(&streamParam, PIPE_FLITE);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):FLITE setParam fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        return INVALID_OPERATION;
    }

    /* 9. Clean up all frames */
    for (int i = 0; i < numFrames; i++) {
        newFrame = NULL;
        if (instantQ.getSizeOfProcessQ() == 0)
            break;

        ret = instantQ.popProcessQ(&newFrame);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]): pop instantQ fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            continue;
        }

        if (newFrame == NULL) {
            CLOGE("ERR(%s[%d]): newFrame is NULL,", __FUNCTION__, __LINE__);
            continue;
        }

        newFrame->decRef();
        m_frameMgr->deleteFrame(newFrame);
    }

    CLOGI("INFO(%s[%d]):Done", __FUNCTION__, __LINE__);

    ret |= totalRet;
    return ret;
}

status_t ExynosCamera3FrameFactoryPreview::initPipes(void)
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    status_t ret = NO_ERROR;

    int hwSensorW = 0, hwSensorH = 0;
    uint32_t minFrameRate = 0, maxFrameRate = 0, sensorFrameRate = 0;

    m_parameters->getHwSensorSize(&hwSensorW, &hwSensorH);
    m_parameters->getPreviewFpsRange(&minFrameRate, &maxFrameRate);
    if (m_parameters->getScalableSensorMode() == true)
        sensorFrameRate = 24;
    else
        sensorFrameRate = maxFrameRate;

    ret = m_initFlitePipe(hwSensorW, hwSensorH, sensorFrameRate);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_initFlitePipe() fail", __FUNCTION__, __LINE__);
        return ret;
    }

    /* setDeviceInfo does changing path */
    ret = m_setupConfig();
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_setupConfig(%d) fail", __FUNCTION__, __LINE__);
        return ret;
    }

    ret = m_initPipes();
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_initPipes() fail", __FUNCTION__, __LINE__);
        return ret;
    }

    m_frameCount = 0;

    ret = m_transitState(FRAME_FACTORY_STATE_INIT);

    return ret;
}

status_t ExynosCamera3FrameFactoryPreview::mapBuffers(void)
{
    status_t ret = NO_ERROR;

    ret = m_pipes[PIPE_3AA]->setMapBuffer();
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):3AA mapBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    if (m_flag3aaIspOTF == false) {
        ret = m_pipes[PIPE_ISP]->setMapBuffer();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):ISP mapBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    if (m_parameters->getTpuEnabledMode() == true && m_flagIspTpuOTF == false) {
        ret = m_pipes[PIPE_TPU]->setMapBuffer();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):TPU mapBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    if (m_flagIspMcscOTF == false && m_flagTpuMcscOTF == false) {
        ret = m_pipes[PIPE_MCSC]->setMapBuffer();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):MCSC mapBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    CLOGI("INFO(%s[%d]):Map buffer Success!", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCamera3FrameFactoryPreview::preparePipes(void)
{
    status_t ret = NO_ERROR;

    /* NOTE: Prepare for 3AA is moved after ISP stream on */

    if (m_requestFLITE) {
        ret = m_pipes[PIPE_FLITE]->prepare();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):FLITE prepare fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    return NO_ERROR;
}

status_t ExynosCamera3FrameFactoryPreview::startPipes(void)
{
    status_t ret = NO_ERROR;

    if (m_flagIspMcscOTF == false && m_flagTpuMcscOTF == false) {
        ret = m_pipes[PIPE_MCSC]->start();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):MCSC start fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    if (m_parameters->getTpuEnabledMode() == true && m_flagIspTpuOTF == false) {
        ret = m_pipes[PIPE_TPU]->start();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):TPU start fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    if (m_flag3aaIspOTF == false) {
        ret = m_pipes[PIPE_ISP]->start();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):ISP start fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    ret = m_pipes[PIPE_3AA]->start();
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):3AA start fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ret = m_pipes[PIPE_FLITE]->start();
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):FLITE start fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    if (m_flagFlite3aaOTF == true) {
        /* Here is doing 3AA prepare(qbuf) */
        ret = m_pipes[PIPE_3AA]->prepare();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):3AA prepare fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    ret = m_pipes[PIPE_FLITE]->sensorStream(true);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):FLITE sensorStream on fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ret = m_transitState(FRAME_FACTORY_STATE_RUN);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):Failed to transitState. ret %d",
                __FUNCTION__, __LINE__, ret);
        return ret;
    }

    CLOGI("INFO(%s[%d]):Starting Success!", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCamera3FrameFactoryPreview::stopPipes(void)
{
    status_t ret = NO_ERROR;

    if (m_pipes[PIPE_3AA]->isThreadRunning() == true) {
        ret = m_pipes[PIPE_3AA]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):3AA stopThread fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    if (m_pipes[PIPE_ISP]->isThreadRunning() == true) {
        ret = m_pipes[PIPE_ISP]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):ISP stopThread fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    if (m_pipes[PIPE_TPU]->isThreadRunning() == true) {
        ret = m_pipes[PIPE_TPU]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):TPU stopThread fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    if (m_pipes[PIPE_MCSC]->isThreadRunning() == true) {
        ret = m_pipes[PIPE_MCSC]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):MCSC stopThread fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    if (m_requestFLITE) {
        ret = m_pipes[PIPE_FLITE]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):FLITE stopThread fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    if (m_pipes[PIPE_GSC]->isThreadRunning() == true) {
        ret = m_pipes[PIPE_GSC]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):PIPE_GSC stopThread fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    ret = m_pipes[PIPE_FLITE]->sensorStream(false);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):FLITE sensorStream off fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ret = m_pipes[PIPE_FLITE]->stop();
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):FLITE stop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    /* 3AA force done */
    ret = m_pipes[PIPE_3AA]->forceDone(V4L2_CID_IS_FORCE_DONE, 0x1000);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):PIPE_3AA force done fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        /* return INVALID_OPERATION; */
    }

    /* stream off for 3AA */
    ret = m_pipes[PIPE_3AA]->stop();
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):3AA stop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    if (m_pipes[PIPE_ISP]->flagStart() == true) {
        /* ISP force done */
        ret = m_pipes[PIPE_ISP]->forceDone(V4L2_CID_IS_FORCE_DONE, 0x1000);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):PIPE_ISP force done fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            /* return INVALID_OPERATION; */
        }

        /* stream off for ISP */
        ret = m_pipes[PIPE_ISP]->stop();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):ISP stop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    if (m_pipes[PIPE_TPU]->flagStart() == true) {
        /* TPU force done */
        ret = m_pipes[PIPE_TPU]->forceDone(V4L2_CID_IS_FORCE_DONE, 0x1000);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):PIPE_TPU force done fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            /* return INVALID_OPERATION; */
        }

        ret = m_pipes[PIPE_TPU]->stop();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):TPU stop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    if (m_pipes[PIPE_MCSC]->flagStart() == true) {
        /* MCSC force done */
        ret = m_pipes[PIPE_MCSC]->forceDone(V4L2_CID_IS_FORCE_DONE, 0x1000);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):PIPE_MCSC force done fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            /* return INVALID_OPERATION; */
        }

        ret = m_pipes[PIPE_MCSC]->stop();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):MCSC stop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    ret = stopThreadAndWait(PIPE_GSC);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):PIPE_GSC stopThreadAndWait fail, ret(%d)", __FUNCTION__, __LINE__, ret);
    }

    ret = m_transitState(FRAME_FACTORY_STATE_CREATE);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):Failed to transitState. ret %d",
                __FUNCTION__, __LINE__, ret);
        return ret;
    }

    CLOGI("INFO(%s[%d]):Stopping  Success!", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCamera3FrameFactoryPreview::startInitialThreads(void)
{
    status_t ret = NO_ERROR;

    CLOGI("INFO(%s[%d]):start pre-ordered initial pipe thread", __FUNCTION__, __LINE__);

    if (m_requestFLITE) {
        ret = startThread(PIPE_FLITE);
        if (ret != NO_ERROR)
            return ret;
    }

    ret = startThread(PIPE_3AA);
    if (ret != NO_ERROR)
        return ret;

    if (m_flag3aaIspOTF == false) {
        ret = startThread(PIPE_ISP);
        if (ret != NO_ERROR)
            return ret;
    }

    if (m_parameters->getTpuEnabledMode() == true && m_flagIspTpuOTF == false) {
        ret = startThread(PIPE_TPU);
        if (ret != NO_ERROR)
            return ret;
    }

    if (m_flagIspMcscOTF == false && m_flagTpuMcscOTF == false) {
        ret = startThread(PIPE_MCSC);
        if (ret != NO_ERROR)
            return ret;
    }

    return NO_ERROR;
}

status_t ExynosCamera3FrameFactoryPreview::setStopFlag(void)
{
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    status_t ret = NO_ERROR;

    ret = m_pipes[PIPE_FLITE]->setStopFlag();

    if (m_pipes[PIPE_3AA]->flagStart() == true)
        ret = m_pipes[PIPE_3AA]->setStopFlag();

    if (m_pipes[PIPE_ISP]->flagStart() == true)
        ret = m_pipes[PIPE_ISP]->setStopFlag();

    if (m_pipes[PIPE_TPU]->flagStart() == true)
        ret = m_pipes[PIPE_TPU]->setStopFlag();

    if (m_pipes[PIPE_MCSC]->flagStart() == true)
        ret = m_pipes[PIPE_MCSC]->setStopFlag();

    return NO_ERROR;
}

ExynosCameraFrame *ExynosCamera3FrameFactoryPreview::createNewFrame(uint32_t frameCount)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameEntity *newEntity[MAX_NUM_PIPES] = {0};
    ExynosCameraFrame *frame = NULL;

    int requestEntityCount = 0;
    int pipeId = -1;
    int parentPipeId = -1;

    if (frameCount <= 0) {
        frameCount = m_frameCount;
    }

    frame = m_frameMgr->createFrame(m_parameters, frameCount, FRAME_TYPE_PREVIEW);

    ret = m_initFrameMetadata(frame);
    if (ret != NO_ERROR)
        CLOGE("(%s[%d]):frame(%d) metadata initialize fail", __FUNCTION__, __LINE__, m_frameCount);

    if (m_requestFLITE == true) {
        /* set flite pipe to linkageList */
        pipeId = PIPE_FLITE;
        newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_OUTPUT_ONLY, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[pipeId]);
        requestEntityCount++;
    }

    /* set 3AA pipe to linkageList */
    pipeId = PIPE_3AA;
    newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[pipeId]);
    requestEntityCount++;
    parentPipeId = pipeId;

    /* set ISP pipe to linkageList */
    if (m_flag3aaIspOTF == false) {
        pipeId = PIPE_ISP;
        newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
        frame->addChildEntity(newEntity[parentPipeId], newEntity[pipeId], PIPE_3AP);
        requestEntityCount++;
        parentPipeId = pipeId;
    }

    /* set TPU pipe to linkageList */
    if (m_parameters->getTpuEnabledMode() == true && m_flagIspTpuOTF == false) {
        pipeId = PIPE_TPU;
        newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
        frame->addChildEntity(newEntity[parentPipeId], newEntity[pipeId], PIPE_ISPP);
        requestEntityCount++;
        parentPipeId = pipeId;
    }

    /* set MCSC pipe to linkageList */
    if (m_flagIspMcscOTF == false && m_flagTpuMcscOTF == false) {
        pipeId = PIPE_MCSC;
        newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
        if (m_parameters->getTpuEnabledMode() == true)
            frame->addChildEntity(newEntity[parentPipeId], newEntity[pipeId], PIPE_TPUP);
        else
            frame->addChildEntity(newEntity[parentPipeId], newEntity[pipeId], PIPE_ISPC);
        requestEntityCount++;
    }

    /* set GSC pipe to linkageList */
    pipeId = PIPE_GSC;
    newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[pipeId]);

    /* set GSC for Video pipe to linkageList */
    pipeId = PIPE_GSC_VIDEO;
    newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[pipeId]);

    if (m_supportReprocessing == false) {
        /* set GSC for Capture pipe to linkageList */
        pipeId = PIPE_GSC_PICTURE;
        newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[pipeId]);

        /* set JPEG pipe to linkageList */
        pipeId = PIPE_JPEG;
        newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[pipeId]);
    }

    ret = m_initPipelines(frame);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_initPipelines fail, ret(%d)", __FUNCTION__, __LINE__, ret);
    }

    /* TODO: make it dynamic */
    frame->setNumRequestPipe(requestEntityCount);

    m_fillNodeGroupInfo(frame);

    m_frameCount++;

    return frame;
}

status_t ExynosCamera3FrameFactoryPreview::m_setupConfig()
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    status_t ret = NO_ERROR;

    int pipeId = -1;

    m_flagFlite3aaOTF = m_parameters->isFlite3aaOtf();
    m_flag3aaIspOTF = m_parameters->is3aaIspOtf();
    m_flagIspTpuOTF = m_parameters->isIspTpuOtf();
    m_flagIspMcscOTF = m_parameters->isIspMcscOtf();
    m_flagTpuMcscOTF = m_parameters->isTpuMcscOtf();

    m_supportReprocessing = m_parameters->isReprocessing();
    m_supportPureBayerReprocessing = m_parameters->getUsePureBayerReprocessing();

    m_request3AP = !(m_flag3aaIspOTF);
    if (m_flagIspTpuOTF == false && m_flagIspMcscOTF == false) {
        if (m_parameters->getTpuEnabledMode() == true)
            m_requestISPP = 1;
        else
            m_requestISPC = 1;
    }

    /* FLITE */
    pipeId = PIPE_FLITE;
    m_nodeNums[pipeId][CAPTURE_NODE_1] = m_getFliteNodenum();
    m_sensorIds[pipeId][CAPTURE_NODE_1] = m_getSensorId(m_nodeNums[pipeId][CAPTURE_NODE_1], m_flagReprocessing);
#ifdef SUPPORT_DEPTH_MAP
    if (m_parameters->getUseDepthMap()) {
        m_nodeNums[pipeId][CAPTURE_NODE_2] = m_getDepthVcNodeNum();
        m_sensorIds[pipeId][CAPTURE_NODE_2] = m_getSensorId(m_nodeNums[pipeId][CAPTURE_NODE_2], m_flagReprocessing);
    }
#endif

    /* 3AA ~ MCSC */
    ret = m_setDeviceInfo();
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_setDeviceInfo() fail, ret(%d)",
                __FUNCTION__, __LINE__, ret);
        return ret;
    }

    /* GSC */
    m_nodeNums[PIPE_GSC][OUTPUT_NODE] = PREVIEW_GSC_NODE_NUM;

    /* GSC for Recording */
    m_nodeNums[PIPE_GSC_VIDEO][OUTPUT_NODE] = VIDEO_GSC_NODE_NUM;

    /* GSC for Capture */
    m_nodeNums[PIPE_GSC_PICTURE][OUTPUT_NODE] = PICTURE_GSC_NODE_NUM;

    /* JPEG */
    m_nodeNums[PIPE_JPEG][OUTPUT_NODE] = -1;

    return NO_ERROR;
}

status_t ExynosCamera3FrameFactoryPreview::m_constructMainPipes()
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    int pipeId = -1;

    /* FLITE */
    pipeId = PIPE_FLITE;
    m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraPipeFlite(m_cameraId, m_parameters, m_flagReprocessing, m_nodeNums[pipeId]);
    m_pipes[pipeId]->setPipeId(pipeId);
    m_pipes[pipeId]->setPipeName("PIPE_FLITE");

    /* 3AA */
    pipeId = PIPE_3AA;
    m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraMCPipe(m_cameraId, m_parameters, m_flagReprocessing, &m_deviceInfo[pipeId]);
    m_pipes[pipeId]->setPipeId(pipeId);
    m_pipes[pipeId]->setPipeName("PIPE_3AA");

    /* ISP */
    pipeId = PIPE_ISP;
    m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraMCPipe(m_cameraId, m_parameters, m_flagReprocessing, &m_deviceInfo[pipeId]);
    m_pipes[pipeId]->setPipeId(pipeId);
    m_pipes[pipeId]->setPipeName("PIPE_ISP");

    /* TPU */
    pipeId = PIPE_TPU;
    m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraMCPipe(m_cameraId, m_parameters, m_flagReprocessing, &m_deviceInfo[pipeId]);
    m_pipes[pipeId]->setPipeId(pipeId);
    m_pipes[pipeId]->setPipeName("PIPE_TPU");

    /* MCSC */
    pipeId = PIPE_MCSC;
    m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraMCPipe(m_cameraId, m_parameters, m_flagReprocessing, &m_deviceInfo[pipeId]);
    m_pipes[pipeId]->setPipeId(pipeId);
    m_pipes[pipeId]->setPipeName("PIPE_MCSC");

    /* GSC */
    pipeId = PIPE_GSC;
    m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraPipeGSC(m_cameraId, m_parameters, m_flagReprocessing, m_nodeNums[pipeId]);
    m_pipes[PIPE_GSC]->setPipeId(PIPE_GSC);
    m_pipes[PIPE_GSC]->setPipeName("PIPE_GSC");

    /* GSC for Recording */
    pipeId = PIPE_GSC_VIDEO;
    m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraPipeGSC(m_cameraId, m_parameters, m_flagReprocessing, m_nodeNums[pipeId]);
    m_pipes[pipeId]->setPipeId(pipeId);
    m_pipes[pipeId]->setPipeName("PIPE_GSC_VIDEO");

    /* GSC for Capture */
    pipeId = PIPE_GSC_PICTURE;
    m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraPipeGSC(m_cameraId, m_parameters, m_flagReprocessing, m_nodeNums[pipeId]);
    m_pipes[pipeId]->setPipeId(pipeId);
    m_pipes[pipeId]->setPipeName("PIPE_GSC_PICTURE");

    /* JPEG */
    pipeId = PIPE_JPEG;
    m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraPipeJpeg(m_cameraId, m_parameters, m_flagReprocessing, m_nodeNums[pipeId]);
    m_pipes[pipeId]->setPipeId(pipeId);
    m_pipes[pipeId]->setPipeName("PIPE_JPEG");

    CLOGI("INFO(%s[%d]):pipe ids for preview", __FUNCTION__, __LINE__);
    for (int i = 0; i < MAX_NUM_PIPES; i++) {
        if (m_pipes[i] != NULL) {
            CLOGI("INFO(%s[%d]):-> m_pipes[%d] : PipeId(%d)", __FUNCTION__, __LINE__ , i, m_pipes[i]->getPipeId());
        }
    }

    return NO_ERROR;
}

status_t ExynosCamera3FrameFactoryPreview::m_setDeviceInfo(void)
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);


    int pipeId = -1;
    int node3aa = -1, node3ac = -1, node3ap = -1;
    int nodeIsp = -1, nodeIspc = -1, nodeIspp = -1;
    int nodeTpu = -1;
    int nodeMcsc = -1, nodeMcscp0 = -1, nodeMcscp1 = -1, nodeMcscp2 = -1;
    int previousPipeId = -1;
    int mcscSrcPipeId = -1;
    bool isMcscOtfInput = false;
    enum NODE_TYPE nodeType = INVALID_NODE;
    bool flagStreamLeader = false;

    m_initDeviceInfo(PIPE_3AA);
    m_initDeviceInfo(PIPE_ISP);
    m_initDeviceInfo(PIPE_TPU);
    m_initDeviceInfo(PIPE_MCSC);

    if (m_cameraId == CAMERA_ID_FRONT && m_parameters->getDualMode() == true) {
        node3aa = FIMC_IS_VIDEO_31S_NUM;
        node3ac = FIMC_IS_VIDEO_31C_NUM;
        node3ap = FIMC_IS_VIDEO_31P_NUM;
        nodeIsp = FIMC_IS_VIDEO_I1S_NUM;
        nodeIspc = FIMC_IS_VIDEO_I1C_NUM;
        nodeIspp = FIMC_IS_VIDEO_I1P_NUM;
        nodeMcsc = FIMC_IS_VIDEO_M1S_NUM;
        nodeMcscp0 = FIMC_IS_VIDEO_M3P_NUM;
        nodeMcscp1 = FIMC_IS_VIDEO_M4P_NUM;
    } else {
        node3aa = FIMC_IS_VIDEO_30S_NUM;
        node3ac = FIMC_IS_VIDEO_30C_NUM;
        node3ap = FIMC_IS_VIDEO_30P_NUM;
        nodeIsp = FIMC_IS_VIDEO_I0S_NUM;
        nodeIspc = FIMC_IS_VIDEO_I0C_NUM;
        nodeIspp = FIMC_IS_VIDEO_I0P_NUM;
        nodeMcsc = FIMC_IS_VIDEO_M0S_NUM;
        nodeMcscp0 = FIMC_IS_VIDEO_M0P_NUM;
        nodeMcscp1 = FIMC_IS_VIDEO_M1P_NUM;
    }

    nodeTpu = FIMC_IS_VIDEO_TPU_NUM;
    nodeMcscp2 = FIMC_IS_VIDEO_M2P_NUM;

    /*
     * 3AA
     */
    pipeId = PIPE_3AA;
    previousPipeId = PIPE_FLITE;

    /* 3AS */
    flagStreamLeader = true;
    nodeType = getNodeType(PIPE_3AA);
    m_deviceInfo[pipeId].pipeId[nodeType]  = PIPE_3AA;
    m_deviceInfo[pipeId].nodeNum[nodeType] = node3aa;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "3AA_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_nodeNums[previousPipeId][getNodeType(previousPipeId)], m_flagFlite3aaOTF, flagStreamLeader, m_flagReprocessing);

    /* Other nodes is not stream leader */
    flagStreamLeader = false;

    /* 3AC */
    nodeType = getNodeType(PIPE_3AC);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_3AC;
    m_deviceInfo[pipeId].nodeNum[nodeType] = node3ac;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "3AA_CAPTURE", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_3AA)], true, flagStreamLeader, m_flagReprocessing);

    /* 3AP */
    nodeType = getNodeType(PIPE_3AP);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_3AP;
    m_deviceInfo[pipeId].nodeNum[nodeType] = node3ap;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "3AA_PREVIEW", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_3AA)], true, flagStreamLeader, m_flagReprocessing);


    /*
     * ISP
     */
    previousPipeId = pipeId;
    mcscSrcPipeId = PIPE_ISPC;
    isMcscOtfInput = m_flagIspMcscOTF;
    pipeId = m_flag3aaIspOTF ? PIPE_3AA : PIPE_ISP;

    /* ISPS */
    nodeType = getNodeType(PIPE_ISP);
    m_deviceInfo[pipeId].pipeId[nodeType]  = PIPE_ISP;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeIsp;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "ISP_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[previousPipeId].nodeNum[getNodeType(PIPE_3AP)], m_flag3aaIspOTF, flagStreamLeader, m_flagReprocessing);

    /* ISPC */
    nodeType = getNodeType(PIPE_ISPC);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_ISPC;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeIspc;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "ISP_CAPTURE", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_ISP)], true, flagStreamLeader, m_flagReprocessing);

    /* ISPP */
    nodeType = getNodeType(PIPE_ISPP);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_ISPP;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeIspp;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "ISP_PREVIEW", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_ISP)], true, flagStreamLeader, m_flagReprocessing);

    /*
     * TPU
     */
    if (m_parameters->getTpuEnabledMode() == true) {
        previousPipeId = pipeId;
        mcscSrcPipeId = PIPE_TPU;
        isMcscOtfInput = m_flagTpuMcscOTF;

        if (m_flagIspTpuOTF == false)
            pipeId = PIPE_TPU;

        /* TPU */
        nodeType = getNodeType(PIPE_TPU);
        m_deviceInfo[pipeId].pipeId[nodeType]  = PIPE_TPU;
        m_deviceInfo[pipeId].nodeNum[nodeType] = nodeTpu;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "TPU_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[previousPipeId].nodeNum[getNodeType(PIPE_ISPP)], m_flagIspTpuOTF, flagStreamLeader, m_flagReprocessing);
    }

    /*
     * MCSC
     */
    previousPipeId = pipeId;

    if (m_flagIspMcscOTF == false && m_flagTpuMcscOTF == false)
        pipeId = PIPE_MCSC;

    /* MCSC */
    nodeType = getNodeType(PIPE_MCSC);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcsc;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[previousPipeId].nodeNum[getNodeType(mcscSrcPipeId)], isMcscOtfInput, flagStreamLeader, m_flagReprocessing);

    /* MCSC0 */
    nodeType = getNodeType(PIPE_MCSC0);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC0;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcscp0;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_PREVIEW", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC)], true, flagStreamLeader, m_flagReprocessing);

    /* MCSC1 */
    nodeType = getNodeType(PIPE_MCSC1);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC1;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcscp1;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_PREVIEW_CALLBACK", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC)], true, flagStreamLeader, m_flagReprocessing);

    if (m_cameraId == CAMERA_ID_BACK || m_parameters->getDualMode() == false) {
        /* MCSC2 */
        nodeType = getNodeType(PIPE_MCSC2);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC2;
        m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcscp2;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_RECORDING", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC)], true, flagStreamLeader, m_flagReprocessing);
    }

    return NO_ERROR;
}

status_t ExynosCamera3FrameFactoryPreview::m_initPipes(void)
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    status_t ret = NO_ERROR;
    camera_pipe_info_t pipeInfo[MAX_NODE];
    camera_pipe_info_t nullPipeInfo;

    int pipeId = -1;
    enum NODE_TYPE nodeType = INVALID_NODE;
    enum NODE_TYPE leaderNodeType = OUTPUT_NODE;

    int maxSensorW = 0, maxSensorH = 0;
    int yuvWidth[3] = {0};
    int yuvHeight[3] = {0};
    int yuvFormat[3] = {0};
    int stride = 0;
    int bayerFormat = m_parameters->getBayerFormat(PIPE_3AA);
    int pictureFormat = m_parameters->getHwPictureFormat();
    int hwVdisformat = m_parameters->getHWVdisFormat();
    int perFramePos = 0;
    struct ExynosConfigInfo *config = m_parameters->getConfig();
    ExynosRect bnsSize;
    ExynosRect bcropSize;
    ExynosRect bdsSize;
    ExynosRect tempRect;

#ifdef DEBUG_RAWDUMP
    if (m_parameters->checkBayerDumpEnable()) {
        bayerFormat = CAMERA_DUMP_BAYER_FORMAT;
    }
#endif

    m_parameters->getMaxSensorSize(&maxSensorW, &maxSensorH);

    m_parameters->getPreviewBayerCropSize(&bnsSize, &bcropSize, false);
    m_parameters->getPreviewBdsSize(&bdsSize, false);

    CLOGI("INFO(%s[%d]):MaxSensorSize %dx%d bayerFormat %x",
            __FUNCTION__, __LINE__, maxSensorW, maxSensorH, bayerFormat);
    CLOGI("INFO(%s[%d]):BnsSize %dx%d BcropSize %dx%d BdsSize %dx%d",
            __FUNCTION__, __LINE__,
            bnsSize.w, bnsSize.h, bcropSize.w, bcropSize.h, bdsSize.w, bdsSize.h);

    for (int i = 0; i < 3; i++) {
        m_parameters->getYuvSize(&yuvWidth[i], &yuvHeight[i], i);
        yuvFormat[i] = m_parameters->getYuvFormat(i);

        CLOGI("INFO(%s[%d]):YUV[%d] Size %dx%d Format %x",
                __FUNCTION__, __LINE__, i,
                yuvWidth[i], yuvHeight[i], yuvFormat[i]);
    }

    /*
     * 3AA
     */
    pipeId = PIPE_3AA;

    /* 3AS */
    nodeType = getNodeType(PIPE_3AA);
    bayerFormat = m_parameters->getBayerFormat(PIPE_3AA);

    if (m_flagFlite3aaOTF == true) {
        /* set v4l2 buffer size */
        tempRect.fullW = 32;
        tempRect.fullH = 64;
        tempRect.colorFormat = bayerFormat;

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_3aa_buffers;
    } else {
        /* set v4l2 buffer size */
        tempRect.fullW = maxSensorW;
        tempRect.fullH = maxSensorH;
        tempRect.colorFormat = bayerFormat;

        /* set v4l2 video node bytes per plane */
        pipeInfo[nodeType].bytesPerPlane[0] = getBayerLineSize(tempRect.fullW, bayerFormat);
#if 0
        switch (bayerFormat) {
        case V4L2_PIX_FMT_SBGGR16:
            pipeInfo[nodeType].bytesPerPlane[0] = ROUND_UP((tempRect.fullW * 2), CAMERA_16PX_ALIGN);
            break;
        case V4L2_PIX_FMT_SBGGR12:
            pipeInfo[nodeType].bytesPerPlane[0] = ROUND_UP((tempRect.fullW * 3 / 2), CAMERA_16PX_ALIGN);
            break;
        case V4L2_PIX_FMT_SBGGR10:
            pipeInfo[nodeType].bytesPerPlane[0] = ROUND_UP((tempRect.fullW * 5 / 4), CAMERA_16PX_ALIGN);
            break;
        default:
            CLOGW("WRN(%s[%d]):Invalid bayer format(%d)", __FUNCTION__, __LINE__, bayerFormat);
            pipeInfo[nodeType].bytesPerPlane[0] = ROUND_UP((tempRect.fullW * 2), CAMERA_16PX_ALIGN);
            break;
        }
#endif
        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_bayer_buffers;
    }

    /* Set output node default info */
    SET_OUTPUT_DEVICE_BASIC_INFO(PERFRAME_INFO_3AA);

    /* 3AC */
    nodeType = getNodeType(PIPE_3AC);
    perFramePos = PERFRAME_BACK_3AC_POS;
    bayerFormat = m_parameters->getBayerFormat(PIPE_3AC);

    /* set v4l2 buffer size */
    tempRect.fullW = bcropSize.w;
    tempRect.fullH = bcropSize.h;
    tempRect.colorFormat = bayerFormat;

    pipeInfo[nodeType].bytesPerPlane[0] = getBayerLineSize(tempRect.fullW, bayerFormat);
#if 0
    /* set v4l2 video node bytes per plane */
    switch (bayerFormat) {
    case V4L2_PIX_FMT_SBGGR16:
        pipeInfo[nodeType].bytesPerPlane[0] = ROUND_UP((tempRect.fullW * 2), CAMERA_16PX_ALIGN);
        break;
    case V4L2_PIX_FMT_SBGGR12:
        pipeInfo[nodeType].bytesPerPlane[0] = ROUND_UP((tempRect.fullW * 3 / 2), CAMERA_16PX_ALIGN);
        break;
    case V4L2_PIX_FMT_SBGGR10:
        pipeInfo[nodeType].bytesPerPlane[0] = ROUND_UP((tempRect.fullW * 5 / 4), CAMERA_16PX_ALIGN);
        break;
    default:
        CLOGW("WRN(%s[%d]):Invalid bayer format(%d)", __FUNCTION__, __LINE__, bayerFormat);
        pipeInfo[nodeType].bytesPerPlane[0] = ROUND_UP((tempRect.fullW * 2), CAMERA_16PX_ALIGN);
        break;
    }
#endif
    /* set v4l2 video node buffer count */
    switch(m_parameters->getReprocessingBayerMode()) {
        case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON:
        case REPROCESSING_BAYER_MODE_PURE_DYNAMIC:
        case REPROCESSING_BAYER_MODE_NONE:
            pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_3aa_buffers;
            break;
        case REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON:
        case REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC:
            if (m_parameters->isSupportZSLInput()) {
                pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_picture_buffers;
            } else {
                pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_sensor_buffers
                                                   + config->current->bufInfo.num_bayer_buffers;
            }
            break;
        default:
            CLOGE("ERR(%s[%d]):Invalid reprocessing mode(%d)", __FUNCTION__, __LINE__, m_parameters->getReprocessingBayerMode());
    }

    /* Set capture node default info */
    SET_CAPTURE_DEVICE_BASIC_INFO();

    /* 3AP */
    nodeType = getNodeType(PIPE_3AP);
    perFramePos = PERFRAME_BACK_3AP_POS;
    bayerFormat = m_parameters->getBayerFormat(PIPE_3AP);

    /* set v4l2 buffer size */
    tempRect.fullW = bdsSize.w;
    tempRect.fullH = bdsSize.h;
    tempRect.colorFormat = bayerFormat;

    /* set v4l2 video node bytes per plane */
    pipeInfo[nodeType].bytesPerPlane[0] = getBayerLineSize(tempRect.fullW, bayerFormat);
#if 0
    switch (bayerFormat) {
    case V4L2_PIX_FMT_SBGGR16:
        pipeInfo[nodeType].bytesPerPlane[0] = ROUND_UP((tempRect.fullW * 2), CAMERA_16PX_ALIGN);
        break;
    case V4L2_PIX_FMT_SBGGR12:
        pipeInfo[nodeType].bytesPerPlane[0] = ROUND_UP((tempRect.fullW * 3 / 2), CAMERA_16PX_ALIGN);
        break;
    case V4L2_PIX_FMT_SBGGR10:
        pipeInfo[nodeType].bytesPerPlane[0] = ROUND_UP((tempRect.fullW * 5 / 4), CAMERA_16PX_ALIGN);
        break;
    default:
        CLOGW("WRN(%s[%d]):Invalid bayer format(%d)", __FUNCTION__, __LINE__, bayerFormat);
        pipeInfo[nodeType].bytesPerPlane[0] = ROUND_UP((tempRect.fullW * 2), CAMERA_16PX_ALIGN);
        break;
    }
#endif
    /* set v4l2 video node buffer count */
    pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_3aa_buffers;

    /* Set capture node default info */
    SET_CAPTURE_DEVICE_BASIC_INFO();

    /* setup pipe info to 3AA pipe */
    if (m_flag3aaIspOTF == false) {
        ret = m_pipes[pipeId]->setupPipe(pipeInfo, m_sensorIds[pipeId]);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):3AA setupPipe fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }

        /* clear pipeInfo for next setupPipe */
        for (int i = 0; i < MAX_NODE; i++)
            pipeInfo[i] = nullPipeInfo;
    }

    /*
     * ISP
     */

    /* ISPS */
    if (m_flag3aaIspOTF == false) {
        pipeId = PIPE_ISP;
        nodeType = getNodeType(PIPE_ISP);
        bayerFormat = m_parameters->getBayerFormat(PIPE_ISP);

        /* set v4l2 buffer size */
        tempRect.fullW = bdsSize.w;
        tempRect.fullH = bdsSize.h;
        tempRect.colorFormat = bayerFormat;

        /* set v4l2 video node bytes per plane */
        pipeInfo[nodeType].bytesPerPlane[0] = getBayerLineSize(tempRect.fullW, bayerFormat);
#if 0
        switch (bayerFormat) {
        case V4L2_PIX_FMT_SBGGR16:
            pipeInfo[nodeType].bytesPerPlane[0] = ROUND_UP((tempRect.fullW * 2), CAMERA_16PX_ALIGN);
            break;
        case V4L2_PIX_FMT_SBGGR12:
            pipeInfo[nodeType].bytesPerPlane[0] = ROUND_UP((tempRect.fullW * 3 / 2), CAMERA_16PX_ALIGN);
            break;
        case V4L2_PIX_FMT_SBGGR10:
            pipeInfo[nodeType].bytesPerPlane[0] = ROUND_UP((tempRect.fullW * 5 / 4), CAMERA_16PX_ALIGN);
            break;
        default:
            CLOGW("WRN(%s[%d]):Invalid bayer format(%d)", __FUNCTION__, __LINE__, bayerFormat);
            pipeInfo[nodeType].bytesPerPlane[0] = ROUND_UP((tempRect.fullW * 2), CAMERA_16PX_ALIGN);
            break;
        }
#endif
        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_3aa_buffers;

        /* Set output node default info */
        SET_OUTPUT_DEVICE_BASIC_INFO(PERFRAME_INFO_ISP);
    }

    /* ISPC */
    nodeType = getNodeType(PIPE_ISPC);
    perFramePos = PERFRAME_BACK_ISPC_POS;

    /* set v4l2 buffer size */
    tempRect.fullW = bdsSize.w;
    tempRect.fullH = bdsSize.h;
    tempRect.colorFormat = hwVdisformat;

    /* set v4l2 video node bytes per plane */
    pipeInfo[nodeType].bytesPerPlane[0] = ROUND_UP(tempRect.fullW * 2, CAMERA_16PX_ALIGN);

    /* set v4l2 video node buffer count */
    pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_hwdis_buffers;

    /* Set capture node default info */
    SET_CAPTURE_DEVICE_BASIC_INFO();

    /* ISPP */
    nodeType = getNodeType(PIPE_ISPP);
    perFramePos = PERFRAME_BACK_ISPP_POS;

    /* set v4l2 buffer size */
    tempRect.fullW = bdsSize.w;
    tempRect.fullH = bdsSize.h;
    tempRect.colorFormat = hwVdisformat;

    /* set v4l2 video node bytes per plane */
    pipeInfo[nodeType].bytesPerPlane[0] = ROUND_UP(tempRect.fullW * 2, CAMERA_TPU_CHUNK_ALIGN_W);

    /* set v4l2 video node buffer count */
    pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_hwdis_buffers;

    /* Set capture node default info */
    SET_CAPTURE_DEVICE_BASIC_INFO();

    /* setup pipe info to ISP pipe */
    if (m_flagIspTpuOTF == false && m_flagIspMcscOTF == false) {
        ret = m_pipes[pipeId]->setupPipe(pipeInfo, m_sensorIds[pipeId]);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):ISP setupPipe fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }

        /* clear pipeInfo for next setupPipe */
        for (int i = 0; i < MAX_NODE; i++)
            pipeInfo[i] = nullPipeInfo;
    }

    /*
     * TPU 
     */

    /* TPU */
    if (m_parameters->getTpuEnabledMode() == true) {
        if (m_flagIspTpuOTF == false) {
            pipeId = PIPE_TPU;
            nodeType = getNodeType(PIPE_TPU);

            /* set v4l2 buffer size */
            tempRect.fullW = bdsSize.w;
            tempRect.fullH = bdsSize.h;
            tempRect.colorFormat = hwVdisformat;

            /* set v4l2 video node bytes per plane */
            pipeInfo[nodeType].bytesPerPlane[0] = ROUND_UP(tempRect.fullW * 2, CAMERA_TPU_CHUNK_ALIGN_W);

            /* set v4l2 video node buffer count */
            pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_hwdis_buffers;

            /* Set output node default info */
            SET_OUTPUT_DEVICE_BASIC_INFO(PERFRAME_INFO_TPU);
        }

        /* setup pipe info to TPU pipe */
        if (m_flagTpuMcscOTF == false) {
            ret = m_pipes[pipeId]->setupPipe(pipeInfo, m_sensorIds[pipeId]);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):TPU setupPipe fail, ret(%d)", __FUNCTION__, __LINE__, ret);
                /* TODO: exception handling */
                return INVALID_OPERATION;
            }

            /* clear pipeInfo for next setupPipe */
            for (int i = 0; i < MAX_NODE; i++)
                pipeInfo[i] = nullPipeInfo;
        }
    }

    /*
     * MCSC
     */

    /* MCSC */
    if (m_flagIspMcscOTF == false && m_flagTpuMcscOTF == false) {
        pipeId = PIPE_MCSC;
        nodeType = getNodeType(PIPE_MCSC);

        /* set v4l2 buffer size */
        tempRect.fullW = bdsSize.w;
        tempRect.fullH = bdsSize.h;
        tempRect.colorFormat = hwVdisformat;

        /* set v4l2 video node bytes per plane */
        pipeInfo[nodeType].bytesPerPlane[0] = ROUND_UP(tempRect.fullW * 2, CAMERA_16PX_ALIGN);

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_hwdis_buffers;

        /* Set output node default info */
        SET_OUTPUT_DEVICE_BASIC_INFO(PERFRAME_INFO_MCSC);
    }

    /* MCSC0 */
    nodeType = getNodeType(PIPE_MCSC0);
    perFramePos = PERFRAME_BACK_SCP_POS;

    /* set v4l2 buffer size */
    tempRect.fullW = yuvWidth[0];
    tempRect.fullH = yuvHeight[0];
    tempRect.colorFormat = yuvFormat[0];

    /* set v4l2 video node bytes per plane */
#ifdef USE_BUFFER_WITH_STRIDE
    /* to use stride for preview buffer, set the bytesPerPlane */
    pipeInfo[nodeType].bytesPerPlane[0] = yuvWidth[0];
#endif

    /* set v4l2 video node buffer count */
    pipeInfo[nodeType].bufInfo.count = m_parameters->getYuvBufferCount(0);

    /* Set capture node default info */
    SET_CAPTURE_DEVICE_BASIC_INFO();

    /* MCSC1 */
    nodeType = getNodeType(PIPE_MCSC1);
    perFramePos = PERFRAME_BACK_MCSC1_POS;

    /* set v4l2 buffer size */
    tempRect.fullW = yuvWidth[1];
    tempRect.fullH = yuvHeight[1];
    tempRect.colorFormat = yuvFormat[1];

    /* set v4l2 video node bytes per plane */
#ifdef USE_BUFFER_WITH_STRIDE
    /* to use stride for preview buffer, set the bytesPerPlane */
    pipeInfo[nodeType].bytesPerPlane[0] = yuvWidth[1];
#endif

    /* set v4l2 video node buffer count */
    pipeInfo[nodeType].bufInfo.count = m_parameters->getYuvBufferCount(1);

    /* Set capture node default info */
    SET_CAPTURE_DEVICE_BASIC_INFO();

    if (m_cameraId == CAMERA_ID_BACK || m_parameters->getDualMode() == false) {
        /* MCSC2 */
        nodeType = getNodeType(PIPE_MCSC2);
        perFramePos = PERFRAME_BACK_MCSC2_POS;

        /* set v4l2 buffer size */
        tempRect.fullW = yuvWidth[2];
        tempRect.fullH = yuvHeight[2];
        tempRect.colorFormat = yuvFormat[2];

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = m_parameters->getYuvBufferCount(2);

        /* Set capture node default info */
        SET_CAPTURE_DEVICE_BASIC_INFO();
    }

    ret = m_pipes[pipeId]->setupPipe(pipeInfo, m_sensorIds[pipeId]);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):MCSC setupPipe fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

status_t ExynosCamera3FrameFactoryPreview::m_initPipesFastenAeStable(int32_t numFrames,
                                                                         int hwSensorW,
                                                                         int hwSensorH)
{
    status_t ret = NO_ERROR;
    camera_pipe_info_t pipeInfo[MAX_NODE];
    camera_pipe_info_t nullPipeInfo;

    int pipeId = -1;
    enum NODE_TYPE nodeType = INVALID_NODE;
    enum NODE_TYPE leaderNodeType = OUTPUT_NODE;

    ExynosRect tempRect;
    int bayerFormat = m_parameters->getBayerFormat(PIPE_3AA);
    int hwVdisformat = m_parameters->getHWVdisFormat();
    int hwPreviewFormat = (m_parameters->getHwPreviewFormat() == 0) ? V4L2_PIX_FMT_NV21M : m_parameters->getHwPreviewFormat();
    int perFramePos = 0;

#ifdef DEBUG_RAWDUMP
    if (m_parameters->checkBayerDumpEnable()) {
        bayerFormat = CAMERA_DUMP_BAYER_FORMAT;
    }
#endif

    pipeId = PIPE_3AA;

    /* 3AS */
    nodeType = getNodeType(PIPE_3AA);
    bayerFormat = m_parameters->getBayerFormat(PIPE_3AA);

    /* set v4l2 buffer size */
    tempRect.fullW = 32;
    tempRect.fullH = 64;
    tempRect.colorFormat = bayerFormat;

    pipeInfo[nodeType].bufInfo.count = numFrames;

    SET_OUTPUT_DEVICE_BASIC_INFO(PERFRAME_INFO_3AA);

    /* 3AC */
    nodeType = getNodeType(PIPE_3AC);
    perFramePos = PERFRAME_BACK_3AC_POS;
    bayerFormat = m_parameters->getBayerFormat(PIPE_3AC);

    /* set v4l2 buffer size */
    tempRect.fullW = hwSensorW;
    tempRect.fullH = hwSensorH;
    tempRect.colorFormat = bayerFormat;

    pipeInfo[nodeType].bufInfo.count = numFrames;

    SET_CAPTURE_DEVICE_BASIC_INFO();

    /* 3AP */
    nodeType = getNodeType(PIPE_3AP);
    perFramePos = PERFRAME_BACK_3AP_POS;
    bayerFormat = m_parameters->getBayerFormat(PIPE_3AP);

    tempRect.colorFormat = bayerFormat;
    pipeInfo[nodeType].bufInfo.count = numFrames;

    SET_CAPTURE_DEVICE_BASIC_INFO();

    /* setup pipe info to 3AA pipe */
    if (m_flag3aaIspOTF == false) {
        ret = m_pipes[pipeId]->setupPipe(pipeInfo, m_sensorIds[pipeId]);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):3AA setupPipe fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }

        /* clear pipeInfo for next setupPipe */
        for (int i = 0; i < MAX_NODE; i++)
            pipeInfo[i] = nullPipeInfo;
    }

    /* ISPS */
    if (m_flag3aaIspOTF == false) {
        pipeId = PIPE_ISP;
        nodeType = getNodeType(PIPE_ISP);
        bayerFormat = m_parameters->getBayerFormat(PIPE_ISP);

        tempRect.colorFormat = bayerFormat;
        pipeInfo[nodeType].bufInfo.count = numFrames;

        SET_OUTPUT_DEVICE_BASIC_INFO(PERFRAME_INFO_ISP);
    }

    /* ISPC */
    nodeType = getNodeType(PIPE_ISPC);
    perFramePos = PERFRAME_BACK_ISPC_POS;

    tempRect.colorFormat = hwVdisformat;

    pipeInfo[nodeType].bufInfo.count = numFrames;

    SET_CAPTURE_DEVICE_BASIC_INFO();

    /* ISPP */
    nodeType = getNodeType(PIPE_ISPP);
    perFramePos = PERFRAME_BACK_ISPP_POS;

    pipeInfo[nodeType].bufInfo.count = numFrames;

    SET_CAPTURE_DEVICE_BASIC_INFO();

    /* setup pipe info to ISP pipe */
    if (m_flagIspTpuOTF == false && m_flagIspMcscOTF == false) {
        ret = m_pipes[pipeId]->setupPipe(pipeInfo, m_sensorIds[pipeId]);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):ISP setupPipe fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }

        /* clear pipeInfo for next setupPipe */
        for (int i = 0; i < MAX_NODE; i++)
            pipeInfo[i] = nullPipeInfo;
    }

    /* TPU */
    if (m_parameters->getTpuEnabledMode() == true) {
        if (m_flagIspTpuOTF == false) {
            pipeId = PIPE_TPU;
            nodeType = getNodeType(PIPE_TPU);

            pipeInfo[nodeType].bufInfo.count = numFrames;

            SET_OUTPUT_DEVICE_BASIC_INFO(PERFRAME_INFO_ISP);
        }

        /* setup pipe info to TPU pipe */
        if (m_flagTpuMcscOTF == false) {
            ret = m_pipes[pipeId]->setupPipe(pipeInfo, m_sensorIds[pipeId]);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):TPU setupPipe fail, ret(%d)", __FUNCTION__, __LINE__, ret);
                /* TODO: exception handling */
                return INVALID_OPERATION;
            }

            /* clear pipeInfo for next setupPipe */
            for (int i = 0; i < MAX_NODE; i++)
                pipeInfo[i] = nullPipeInfo;
        }
    }

    /* MCSC */
    if (m_flagIspMcscOTF == false && m_flagTpuMcscOTF == false) {
        pipeId = PIPE_MCSC;
        nodeType = getNodeType(PIPE_MCSC);

        pipeInfo[nodeType].bufInfo.count = numFrames;

        SET_OUTPUT_DEVICE_BASIC_INFO(PERFRAME_INFO_MCSC);
    }

    /* MCSC0 */
    nodeType = getNodeType(PIPE_MCSC0);
    perFramePos = PERFRAME_BACK_SCP_POS;

    tempRect.colorFormat = hwPreviewFormat;

    pipeInfo[nodeType].bufInfo.count = numFrames;

    SET_CAPTURE_DEVICE_BASIC_INFO();

    /* MCSC1 */
    nodeType = getNodeType(PIPE_MCSC1);
    perFramePos = PERFRAME_BACK_MCSC1_POS;

    pipeInfo[nodeType].bufInfo.count = numFrames;

    SET_CAPTURE_DEVICE_BASIC_INFO();

    /* MCSC2 */
    nodeType = getNodeType(PIPE_MCSC2);
    perFramePos = PERFRAME_BACK_MCSC2_POS;

    pipeInfo[nodeType].bufInfo.count = numFrames;

    SET_CAPTURE_DEVICE_BASIC_INFO();

    ret = m_pipes[pipeId]->setupPipe(pipeInfo, m_sensorIds[pipeId]);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):MCSC setupPipe fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

status_t ExynosCamera3FrameFactoryPreview::m_fillNodeGroupInfo(ExynosCameraFrame *frame)
{
    camera2_node_group node_group_info_3aa;
    camera2_node_group node_group_info_isp;
    camera2_node_group node_group_info_tpu;
    camera2_node_group node_group_info_mcsc;
    camera2_node_group *node_group_info_temp;

    int zoom = m_parameters->getZoomLevel();
    int pipeId = -1;
    uint32_t perframePosition = 0;

    memset(&node_group_info_3aa, 0x0, sizeof(camera2_node_group));
    memset(&node_group_info_isp, 0x0, sizeof(camera2_node_group));
    memset(&node_group_info_tpu, 0x0, sizeof(camera2_node_group));
    memset(&node_group_info_mcsc, 0x0, sizeof(camera2_node_group));


    /* 3AA */
    pipeId = PIPE_3AA;
    node_group_info_temp = &node_group_info_3aa;

    node_group_info_temp->leader.request = 1;
    node_group_info_temp->capture[perframePosition].request = m_request3AC;
    node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_3AC)] - FIMC_IS_VIDEO_BAS_NUM;
    perframePosition++;

    node_group_info_temp->capture[perframePosition].request = m_request3AP;
    node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_3AP)] - FIMC_IS_VIDEO_BAS_NUM;
    perframePosition++;

    /* ISP */
    if (m_flag3aaIspOTF == false) {
        pipeId = PIPE_ISP;
        perframePosition = 0;
        node_group_info_temp = &node_group_info_isp;
        node_group_info_temp->leader.request = 1;
    }

    /* TPU */
    if (m_parameters->getTpuEnabledMode() == true && m_flagIspTpuOTF == false) {
        node_group_info_temp->capture[perframePosition].request = m_requestISPP;
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_ISPP)] - FIMC_IS_VIDEO_BAS_NUM;
        perframePosition++;

        pipeId = PIPE_TPU;
        perframePosition = 0;
        node_group_info_temp = &node_group_info_tpu;
        node_group_info_temp->leader.request = 1;
    } else if (m_flagIspMcscOTF == false) {
        node_group_info_temp->capture[perframePosition].request = m_requestISPC;
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_ISPC)] - FIMC_IS_VIDEO_BAS_NUM;
        perframePosition++;
    }

    /* MCSC */
    if (m_flagIspMcscOTF == false && m_flagTpuMcscOTF == false) {
        pipeId = PIPE_MCSC;
        perframePosition = 0;
        node_group_info_temp = &node_group_info_mcsc;
        node_group_info_temp->leader.request = 1;
    }

    node_group_info_temp->capture[perframePosition].request = m_requestMCSC0;
    node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC0)] - FIMC_IS_VIDEO_BAS_NUM;
    perframePosition++;

    node_group_info_temp->capture[perframePosition].request = m_requestMCSC1;
    node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC1)] - FIMC_IS_VIDEO_BAS_NUM;
    perframePosition++;

    if (m_cameraId == CAMERA_ID_BACK || m_parameters->getDualMode() == false) {
        node_group_info_temp->capture[perframePosition].request = m_requestMCSC2;
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC2)] - FIMC_IS_VIDEO_BAS_NUM;
    }

    updateNodeGroupInfo(
            PIPE_3AA,
            m_parameters,
            &node_group_info_3aa);
    frame->storeNodeGroupInfo(&node_group_info_3aa, PERFRAME_INFO_3AA, zoom);

    if (m_flag3aaIspOTF == false) {
        updateNodeGroupInfo(
                PIPE_ISP,
                m_parameters,
                &node_group_info_isp);
        frame->storeNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_ISP, zoom);
    }

    if (m_parameters->getTpuEnabledMode() == true && m_flagIspTpuOTF == false) {
        updateNodeGroupInfo(
                PIPE_TPU,
                m_parameters,
                &node_group_info_tpu);
        frame->storeNodeGroupInfo(&node_group_info_tpu, PERFRAME_INFO_TPU, zoom);
    }

    if (m_flagIspMcscOTF == false && m_flagTpuMcscOTF == false) {
        updateNodeGroupInfo(
                PIPE_MCSC,
                m_parameters,
                &node_group_info_mcsc);
        frame->storeNodeGroupInfo(&node_group_info_mcsc, PERFRAME_INFO_MCSC, zoom);
    }

    return NO_ERROR;
}

void ExynosCamera3FrameFactoryPreview::m_init(void)
{
    m_flagReprocessing = false;
}

}; /* namespace android */
