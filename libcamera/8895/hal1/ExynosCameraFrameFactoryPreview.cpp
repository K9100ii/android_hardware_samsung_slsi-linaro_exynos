/*
**
** Copyright 2013, Samsung Electronics Co. LTD
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
#define LOG_TAG "ExynosCameraFrameFactoryPreview"
#include <cutils/log.h>

#include "ExynosCameraFrameFactoryPreview.h"

namespace android {

ExynosCameraFrameFactoryPreview::~ExynosCameraFrameFactoryPreview()
{
    status_t ret = NO_ERROR;

    ret = destroy();
    if (ret != NO_ERROR)
        CLOGE("destroy fail");
}

status_t ExynosCameraFrameFactoryPreview::create(__unused bool active)
{
    Mutex::Autolock lock(ExynosCameraStreamMutex::getInstance()->getStreamMutex());
    CLOGI("");

    status_t ret = NO_ERROR;

    m_setupConfig();
    m_constructMainPipes();

    if (m_parameters->isFlite3aaOtf() == true) {
        /* 3AA pipe initialize */
        ret = m_pipes[PIPE_3AA]->create(m_sensorIds[PIPE_3AA]);
        if (ret != NO_ERROR) {
            CLOGE("3AA create fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGD("%s(%d) created", m_pipes[PIPE_3AA]->getPipeName(), PIPE_3AA);
    } else {
        /* FLITE pipe initialize */
        ret = m_pipes[PIPE_FLITE]->create(m_sensorIds[PIPE_FLITE]);
        if (ret != NO_ERROR) {
            CLOGE("FLITE create fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGD("%s(%d) created", m_pipes[PIPE_FLITE]->getPipeName(), PIPE_FLITE);

        /* 3AA pipe initialize */
        ret = m_pipes[PIPE_3AA]->create();
        if (ret != NO_ERROR) {
            CLOGE("3AA create fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGD("%s(%d) created", m_pipes[PIPE_3AA]->getPipeName(), PIPE_3AA);
    }

    /* ISP pipe initialize */
    ret = m_pipes[PIPE_ISP]->create();
    if (ret != NO_ERROR) {
        CLOGE("ISP create fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGV("%s(%d) created",
            m_pipes[PIPE_ISP]->getPipeName(), PIPE_ISP);

    /* TPU pipe initialize */
    ret = m_pipes[PIPE_TPU]->create();
    if (ret != NO_ERROR) {
        CLOGE("TPU create fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGV("%s(%d) created",
            m_pipes[PIPE_TPU]->getPipeName(), PIPE_TPU);

    /* MCSC pipe initialize */
    ret = m_pipes[PIPE_MCSC]->create();
    if (ret != NO_ERROR) {
        CLOGE("MCSC create fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGV("%s(%d) created",
            m_pipes[PIPE_MCSC]->getPipeName(), PIPE_MCSC);

#ifdef USE_GSC_FOR_PREVIEW
    /* GSC_PREVIEW pipe initialize */
    ret = m_pipes[PIPE_GSC]->create();
    if (ret != NO_ERROR) {
        CLOGE("GSC create fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGV("%s(%d) created",
            m_pipes[PIPE_GSC]->getPipeName(), PIPE_GSC);
#endif

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true) {
        /* SYNC pipe initialize */
        ret = m_pipes[INDEX(PIPE_SYNC)]->create();
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):SYNC create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGD("DEBUG(%s[%d]):%s(%d) created", __FUNCTION__, __LINE__,
                m_pipes[INDEX(PIPE_SYNC)]->getPipeName(), PIPE_SYNC);

        /* FUSION pipe initialize */
        ret = m_pipes[INDEX(PIPE_FUSION)]->create();
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):FUSION create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGD("DEBUG(%s[%d]):%s(%d) created", __FUNCTION__, __LINE__,
                m_pipes[INDEX(PIPE_FUSION)]->getPipeName(), PIPE_FUSION);
    }
#endif

    ret = m_pipes[PIPE_GSC_VIDEO]->create();
    if (ret != NO_ERROR) {
        CLOGE("PIPE_GSC_VIDEO create fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGV("%s(%d) created",
            m_pipes[PIPE_GSC_VIDEO]->getPipeName(), PIPE_GSC_VIDEO);

    if (m_supportReprocessing == false) {
        /* GSC_PICTURE pipe initialize */
        ret = m_pipes[PIPE_GSC_PICTURE]->create();
        if (ret != NO_ERROR) {
            CLOGE("GSC_PICTURE create fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGV("%s(%d) created",
                m_pipes[PIPE_GSC_PICTURE]->getPipeName(), PIPE_GSC_PICTURE);

        /* JPEG pipe initialize */
        ret = m_pipes[PIPE_JPEG]->create();
        if (ret != NO_ERROR) {
            CLOGE("JPEG create fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGV("%s(%d) created",
                m_pipes[PIPE_JPEG]->getPipeName(), PIPE_JPEG);
    }

#if 0
    /* ISP Suspend/Resume
       For improving the camera entrance time,
       The TWZ environment information must be delivered to F/W
    */
    bool useFWSuspendResume = true;

    if (m_parameters->getDualMode() == false
#ifdef SAMSUNG_QUICKSHOT
        && m_parameters->getQuickShot() == 0
#endif
#ifdef USE_LIVE_BROADCAST
        && m_parameters->getPLBMode() == false
#endif
    ) {
        ret = m_pipes[PIPE_3AA]->setControl(V4L2_CID_IS_CAMERA_TYPE, (int)useFWSuspendResume);
        if (ret != NO_ERROR) {
            CLOGE("PIPE_%d V4L2_CID_IS_CAMERA_TYPE fail, ret(%d)", PIPE_3AA, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }
#endif

    /* EOS */
    ret = m_pipes[PIPE_3AA]->setControl(V4L2_CID_IS_END_OF_STREAM, 1);
    if (ret != NO_ERROR) {
        CLOGE("PIPE_%d V4L2_CID_IS_END_OF_STREAM fail, ret(%d)", PIPE_3AA, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

#ifdef SUPPORT_GROUP_MIGRATION
    m_initNodeInfo();
#endif

    m_setCreate(true);

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryPreview::precreate(void)
{
    CLOGI("");

    status_t ret = NO_ERROR;

    m_setupConfig();
    m_constructMainPipes();

    if (m_parameters->isFlite3aaOtf() == true) {
        /* 3AA pipe initialize */
        ret = m_pipes[PIPE_3AA]->precreate(m_sensorIds[PIPE_3AA]);
        if (ret != NO_ERROR) {
            CLOGE("3AA precreated fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGD("%s(%d) precreated", m_pipes[PIPE_3AA]->getPipeName(), PIPE_3AA);
    } else {
        /* FLITE pipe initialize */
        ret = m_pipes[PIPE_FLITE]->create(m_sensorIds[PIPE_FLITE]);
        if (ret != NO_ERROR) {
            CLOGE("FLITE create fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGD("%s(%d) created", m_pipes[PIPE_FLITE]->getPipeName(), PIPE_FLITE);

        /* 3AA pipe initialize */
        ret = m_pipes[PIPE_3AA]->precreate();
        if (ret != NO_ERROR) {
            CLOGE("3AA precreated fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGD("%s(%d) precreated", m_pipes[PIPE_3AA]->getPipeName(), PIPE_3AA);
    }

    /* ISP pipe initialize */
    ret = m_pipes[PIPE_ISP]->precreate();
    if (ret != NO_ERROR) {
        CLOGE("ISP create fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGV("%s(%d) precreated",
            m_pipes[PIPE_ISP]->getPipeName(), PIPE_ISP);

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryPreview::postcreate(void)
{
    CLOGI("");
    status_t ret = NO_ERROR;

    /* 3AA_ISP pipe initialize */
    ret = m_pipes[PIPE_3AA]->postcreate();
    if (ret != NO_ERROR) {
        CLOGE("3AA_ISP postcreate fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGV("%s(%d) postcreated",
            m_pipes[PIPE_3AA]->getPipeName(), PIPE_3AA);

    /* ISP pipe initialize */
    ret = m_pipes[PIPE_ISP]->postcreate();
    if (ret != NO_ERROR) {
        CLOGE("ISP create fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGV("%s(%d) postcreated",
            m_pipes[PIPE_ISP]->getPipeName(), PIPE_ISP);

    /* TPU pipe initialize */
    ret = m_pipes[PIPE_TPU]->create();
    if (ret != NO_ERROR) {
        CLOGE("TPU create fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGV("%s(%d) created",
            m_pipes[PIPE_TPU]->getPipeName(), PIPE_TPU);

    /* MCSC pipe initialize */
    ret = m_pipes[PIPE_MCSC]->create();
    if (ret != NO_ERROR) {
        CLOGE("MCSC create fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGV("%s(%d) created",
            m_pipes[PIPE_MCSC]->getPipeName(), PIPE_MCSC);

#ifdef USE_GSC_FOR_PREVIEW
    /* GSC_PREVIEW pipe initialize */
    ret = m_pipes[PIPE_GSC]->create();
    if (ret != NO_ERROR) {
        CLOGE("GSC create fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGV("%s(%d) created",
            m_pipes[PIPE_GSC]->getPipeName(), PIPE_GSC);
#endif

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true) {
        /* SYNC pipe initialize */
        ret = m_pipes[INDEX(PIPE_SYNC)]->create();
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):SYNC create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGD("DEBUG(%s[%d]):%s(%d) created", __FUNCTION__, __LINE__,
                m_pipes[PIPE_SYNC]->getPipeName(), PIPE_SYNC);

        /* FUSION pipe initialize */
        ret = m_pipes[INDEX(PIPE_FUSION)]->create();
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):FUSION create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGD("DEBUG(%s[%d]):%s(%d) created", __FUNCTION__, __LINE__,
                m_pipes[PIPE_FUSION]->getPipeName(), PIPE_FUSION);
    }
#endif

    ret = m_pipes[PIPE_GSC_VIDEO]->create();
    if (ret != NO_ERROR) {
        CLOGE("PIPE_GSC_VIDEO create fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGV("%s(%d) created",
            m_pipes[PIPE_GSC_VIDEO]->getPipeName(), PIPE_GSC_VIDEO);

    if (m_supportReprocessing == false) {
        /* GSC_PICTURE pipe initialize */
        ret = m_pipes[PIPE_GSC_PICTURE]->create();
        if (ret != NO_ERROR) {
            CLOGE("GSC_PICTURE create fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGV("%s(%d) created",
                m_pipes[PIPE_GSC_PICTURE]->getPipeName(), PIPE_GSC_PICTURE);

        /* JPEG pipe initialize */
        ret = m_pipes[PIPE_JPEG]->create();
        if (ret != NO_ERROR) {
            CLOGE("JPEG create fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGV("%s(%d) created",
                m_pipes[PIPE_JPEG]->getPipeName(), PIPE_JPEG);
    }

#ifdef SAMSUNG_TN_FEATURE
    /* ISP Suspend/Resume
       For improving the camera entrance time,
       The TWZ environment information must be delivered to F/W
    */
    int cameraType = IS_COLD_BOOT;
    if (m_parameters->getSamsungCamera() && m_cameraId == CAMERA_ID_BACK)
        cameraType = IS_WARM_BOOT;

    CLOGD("getDualMode(%d), getQuickShot(%d)",
        m_parameters->getDualMode(),
        m_parameters->getQuickShot());

    if (m_parameters->getDualMode() == false
#ifdef SAMSUNG_QUICKSHOT
        && m_parameters->getQuickShot() == 0
#endif
#ifdef USE_LIVE_BROADCAST
        && m_parameters->getPLBMode() == false
#endif
    ) {
        ret = m_pipes[PIPE_3AA]->setControl(V4L2_CID_IS_CAMERA_TYPE, cameraType);
        if (ret < 0) {
            CLOGE("PIPE_%d V4L2_CID_IS_CAMERA_TYPE fail, ret(%d)", PIPE_3AA, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    } else {
        ret = m_pipes[INDEX(PIPE_3AA)]->setControl(V4L2_CID_IS_CAMERA_TYPE, IS_COLD_BOOT);
        if (ret < 0) {
            CLOGE("PIPE_%d V4L2_CID_IS_CAMERA_TYPE fail, ret(%d)", PIPE_3AA, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }
#endif

    /* EOS */
    ret = m_pipes[PIPE_3AA]->setControl(V4L2_CID_IS_END_OF_STREAM, 1);
    if (ret != NO_ERROR) {
        CLOGE("PIPE_%d V4L2_CID_IS_END_OF_STREAM fail, ret(%d)", PIPE_3AA, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

#ifdef SUPPORT_GROUP_MIGRATION
    m_initNodeInfo();
#endif

    m_setCreate(true);

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryPreview::fastenAeStable(int32_t numFrames, ExynosCameraBuffer *buffers)
{
    CLOGI(" Start");

    status_t ret = NO_ERROR;
    status_t totalRet = NO_ERROR;

    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraFrameEntity *newEntity = NULL;
    frame_queue_t instantQ;

    int hwSensorW = 0, hwSensorH = 0;
    int bcropX = 0, bcropY = 0, bcropW = 0, bcropH = 0;
    int hwPreviewW = 0, hwPreviewH = 0;
    uint32_t minFrameRate, maxFrameRate, sensorFrameRate = 0;
    struct v4l2_streamparm streamParam;

    int pipeId = PIPE_FLITE;

#ifdef SUPPORT_GROUP_MIGRATION
    m_updateNodeInfo();
#endif

    if (m_parameters->isFlite3aaOtf() == true) {
        pipeId = PIPE_3AA;
    }

    if (numFrames == 0) {
        CLOGW("umFrames is %d, we skip fastenAeStable", numFrames);
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

    /* setDeviceInfo does changing path */
    ret = m_setupConfig(true);
    if (ret != NO_ERROR) {
        CLOGE("m_setupConfig(%d) fail", ret);
        return ret;
    }

    /*
     * we must set flite'setupPipe on 3aa_pipe.
     * then, setControl/getControl about BNS size
     */
    ret = m_initPipesFastenAeStable(numFrames, hwSensorW, hwSensorH, sensorFrameRate);
    if (ret != NO_ERROR) {
        CLOGE("m_initPipesFastenAeStable() fail, ret(%d)", ret);
        return ret;
    }

    ret = m_initFlitePipe(hwSensorW, hwSensorH, sensorFrameRate);
    if (ret != NO_ERROR) {
        CLOGE("m_initFlitePipes() fail, ret(%d)", ret);
        return ret;
    }

    for (int i = 0; i < numFrames; i++) {
        /* 2. Generate instant frames */
        newFrame = m_frameMgr->createFrame(m_parameters, i);

        ret = m_initFrameMetadata(newFrame);
        if (ret != NO_ERROR)
            CLOGE("frame(%d) metadata initialize fail", i);

        newEntity = new ExynosCameraFrameEntity(PIPE_3AA, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
        newFrame->addSiblingEntity(NULL, newEntity);
        newFrame->setNumRequestPipe(1);

        newEntity->setSrcBuf(buffers[i]);

        /* 3. Set metadata for instant on */
        camera2_shot_ext *shot_ext = (camera2_shot_ext *)buffers[i].addr[buffers[i].getMetaPlaneIndex()];

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
                CLOGE(" invalid nodeNum(%d). so fail", nodeNum);
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
                CLOGE(" invalid nodeNum(%d). so fail", nodeNum);
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
                    nodeType = getNodeType(PIPE_ISPC);
                /* Case of ISP-MCSC M2M */
                else
                    nodeType = getNodeType(PIPE_ISPC);

                nodeNum = m_deviceInfo[PIPE_3AA].nodeNum[nodeType];
                if (nodeNum <= 0) {
                    CLOGE(" invalid nodeNum(%d). so fail", nodeNum);
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
        ret = pushFrameToPipe(newFrame, PIPE_3AA);
        if (ret != NO_ERROR) {
            CLOGE(" pushFrameToPipeFail, ret(%d)", ret);
            goto cleanup;
        }
        CLOGD(" Instant shot - FD(%d, %d)", buffers[i].fd[0], buffers[i].fd[1]);

        instantQ.pushProcessQ(&newFrame);
    }

    /* 5. Pipe instant on */
    if (m_parameters->isFlite3aaOtf() == false)
    {
        ret = m_pipes[PIPE_FLITE]->instantOn(0);
        if (ret != NO_ERROR) {
            CLOGE(" FLITE On fail, ret(%d)", ret);
            goto cleanup;
        }
    }

    if (m_flagIspMcscOTF == false && m_flagTpuMcscOTF == false) {
        ret = m_pipes[PIPE_MCSC]->start();
        if (ret != NO_ERROR) {
            CLOGE("MCSC start fail, ret(%d)", ret);
            goto cleanup;
        }
    }

    if (m_parameters->getTpuEnabledMode() == true && m_flagIspTpuOTF == false) {
        ret = m_pipes[PIPE_TPU]->start();
        if (ret != NO_ERROR) {
            CLOGE("TPU start fail, ret(%d)", ret);
            goto cleanup;
        }
    }

    if (m_flag3aaIspOTF == false) {
        ret = m_pipes[INDEX(PIPE_ISP)]->start();
        if (ret < 0) {
            CLOGE("ISP start fail, ret(%d)", ret);
            goto cleanup;
        }
    }

    ret = m_pipes[PIPE_3AA]->instantOn(numFrames);
    if (ret != NO_ERROR) {
        CLOGE("3AA instantOn fail, ret(%d)", ret);
        goto cleanup;
    }

    /* 6. SetControl to sensor instant on */
    ret = m_pipes[pipeId]->setControl(V4L2_CID_IS_S_STREAM, (1 | numFrames << SENSOR_INSTANT_SHIFT));
    if (ret != NO_ERROR) {
        CLOGE("instantOn fail, ret(%d)", ret);
        goto cleanup;
    }

cleanup:
    totalRet |= ret;

    /* 7. Pipe instant off */
    if (m_parameters->isFlite3aaOtf() == false)
    {
        ret = m_pipes[PIPE_FLITE]->instantOff();
        if (ret != NO_ERROR) {
            CLOGE(" FLITE Off fail, ret(%d)", ret);
        }
    }

    ret = m_pipes[PIPE_3AA]->forceDone(V4L2_CID_IS_FORCE_DONE, 0x1000);
    if (ret != NO_ERROR) {
        CLOGE("3AA force done fail, ret(%d)", ret);
    }

    ret = m_pipes[PIPE_3AA]->instantOff();
    if (ret != NO_ERROR) {
        CLOGE("3AA instantOff fail, ret(%d)", ret);
    }

    if (m_flag3aaIspOTF == false) {
        ret = m_pipes[INDEX(PIPE_ISP)]->stop();
        if (ret != NO_ERROR) {
            CLOGE("ISP stop fail, ret(%d)", ret);
        }
    }

    if (m_parameters->getTpuEnabledMode() == true && m_flagIspTpuOTF == false) {
        ret = m_pipes[PIPE_TPU]->stop();
        if (ret != NO_ERROR) {
            CLOGE("TPU stop fail, ret(%d)", ret);
        }
    }

    if (m_flagIspMcscOTF == false && m_flagTpuMcscOTF == false) {
        ret = m_pipes[PIPE_MCSC]->stop();
        if (ret != NO_ERROR) {
            CLOGE("MCSC stop fail, ret(%d)", ret);
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
    CLOGI("set framerate (denominator=%d)", sensorFrameRate);

    ret = setParam(&streamParam, pipeId);
    if (ret != NO_ERROR) {
        CLOGE("setParam(%d) fail, ret(%d)", pipeId, ret);
        return INVALID_OPERATION;
    }

    /* 9. Clean up all frames */
    for (int i = 0; i < numFrames; i++) {
        newFrame = NULL;
        if (instantQ.getSizeOfProcessQ() == 0)
            break;

        ret = instantQ.popProcessQ(&newFrame);
        if (ret != NO_ERROR) {
            CLOGE(" pop instantQ fail, ret(%d)", ret);
            continue;
        }

        if (newFrame == NULL) {
            CLOGE(" newFrame is NULL,");
            continue;
        }

        newFrame = NULL;
    }

    CLOGI("Done");

    ret |= totalRet;
    return ret;
}

status_t ExynosCameraFrameFactoryPreview::initPipes(void)
{
    CLOGV("");

    status_t ret = NO_ERROR;

    int hwSensorW = 0, hwSensorH = 0;
    uint32_t minFrameRate = 0, maxFrameRate = 0, sensorFrameRate = 0;

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    m_preSyncType = SYNC_TYPE_BASE;
    m_preZoom = 0;
    m_transitionCount = 0;
#endif

    m_parameters->getHwSensorSize(&hwSensorW, &hwSensorH);
    m_parameters->getPreviewFpsRange(&minFrameRate, &maxFrameRate);
    if (m_parameters->getScalableSensorMode() == true)
        sensorFrameRate = 24;
    else
        sensorFrameRate = maxFrameRate;

    /* setDeviceInfo does changing path */
    ret = m_setupConfig();
    if (ret != NO_ERROR) {
        CLOGE("m_setupConfig() fail");
        return ret;
    }

    /*
     * we must set flite'setupPipe on 3aa_pipe.
     * then, setControl/getControl about BNS size
     */
    ret = m_initPipes(sensorFrameRate);
    if (ret != NO_ERROR) {
        CLOGE("m_initPipes() fail");
        return ret;
    }

    ret = m_initFlitePipe(hwSensorW, hwSensorH, sensorFrameRate);
    if (ret != NO_ERROR) {
        CLOGE("m_initFlitePipe() fail");
        return ret;
    }

    m_frameCount = 0;

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryPreview::mapBuffers(void)
{
    status_t ret = NO_ERROR;

    ret = m_pipes[PIPE_3AA]->setMapBuffer();
    if (ret != NO_ERROR) {
        CLOGE("3AA mapBuffer fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    if (m_flag3aaIspOTF == false) {
        ret = m_pipes[PIPE_ISP]->setMapBuffer();
        if (ret != NO_ERROR) {
            CLOGE("ISP mapBuffer fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    if (m_parameters->getTpuEnabledMode() == true && m_flagIspTpuOTF == false) {
        ret = m_pipes[PIPE_TPU]->setMapBuffer();
        if (ret != NO_ERROR) {
            CLOGE("TPU mapBuffer fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    if (m_flagIspMcscOTF == false && m_flagTpuMcscOTF == false) {
        ret = m_pipes[PIPE_MCSC]->setMapBuffer();
        if (ret != NO_ERROR) {
            CLOGE("MCSC mapBuffer fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    CLOGI("Map buffer Success!");

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryPreview::preparePipes(void)
{
    status_t ret = NO_ERROR;

    /* NOTE: Prepare for 3AA is moved after ISP stream on */
    /* we must not qbuf before stream on, when sensor group. */

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryPreview::startPipes(void)
{
    status_t ret = NO_ERROR;

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true) {
        ret = m_pipes[INDEX(PIPE_FUSION)]->start();
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):PIPE_FUSION start fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }

        ret = m_pipes[INDEX(PIPE_SYNC)]->start();
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):PIPE_SYNC start fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }
#endif

    if (m_flagIspMcscOTF == false && m_flagTpuMcscOTF == false) {
        ret = m_pipes[PIPE_MCSC]->start();
        if (ret != NO_ERROR) {
            CLOGE("MCSC start fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    if (m_parameters->getTpuEnabledMode() == true && m_flagIspTpuOTF == false) {
        ret = m_pipes[PIPE_TPU]->start();
        if (ret != NO_ERROR) {
            CLOGE("TPU start fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    if (m_flag3aaIspOTF == false) {
        ret = m_pipes[PIPE_ISP]->start();
        if (ret != NO_ERROR) {
            CLOGE("ISP start fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    ret = m_pipes[PIPE_3AA]->start();
    if (ret != NO_ERROR) {
        CLOGE("3AA start fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    if (m_parameters->isFlite3aaOtf() == false)
    {
        ret = m_pipes[PIPE_FLITE]->start();
        if (ret != NO_ERROR) {
            CLOGE("FLITE start fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }

        /* Here is doing FLITE prepare(qbuf) */
        ret = m_pipes[PIPE_FLITE]->prepare();
        if (ret != NO_ERROR) {
            CLOGE("FLITE prepare fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    if (m_flagFlite3aaOTF == true) {
        /* Here is doing 3AA prepare(qbuf) */
        ret = m_pipes[PIPE_3AA]->prepare();
        if (ret != NO_ERROR) {
            CLOGE("3AA prepare fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    if (m_parameters->isFlite3aaOtf() == true) {
        ret = m_pipes[PIPE_3AA]->sensorStream(true);
    } else
    {
        ret = m_pipes[PIPE_FLITE]->sensorStream(true);
    }
    if (ret != NO_ERROR) {
        CLOGE("FLITE sensorStream on fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    CLOGI("Starting Success!");

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryPreview::stopPipes(void)
{
    status_t ret = NO_ERROR;
    status_t funcRet = NO_ERROR;

    if (m_pipes[PIPE_3AA]->isThreadRunning() == true) {
        ret = m_pipes[PIPE_3AA]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("3AA stopThread fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    if (m_pipes[PIPE_ISP]->isThreadRunning() == true) {
        ret = m_pipes[PIPE_ISP]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("ISP stopThread fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    if (m_pipes[PIPE_TPU]->isThreadRunning() == true) {
        ret = m_pipes[PIPE_TPU]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("TPU stopThread fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    if (m_pipes[PIPE_MCSC]->isThreadRunning() == true) {
        ret = m_pipes[PIPE_MCSC]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("MCSC stopThread fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    if (m_requestVC0 && m_parameters->isFlite3aaOtf() == false) {
        ret = m_pipes[PIPE_FLITE]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("FLITE stopThread fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

#ifdef USE_GSC_FOR_PREVIEW
    if (m_pipes[PIPE_GSC]->isThreadRunning() == true) {
        ret = m_pipes[PIPE_GSC]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("PIPE_GSC stopThread fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }
#endif

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true) {
        if (m_pipes[INDEX(PIPE_SYNC)] != NULL &&
                m_pipes[INDEX(PIPE_SYNC)]->isThreadRunning() == true) {
            ret = m_pipes[INDEX(PIPE_SYNC)]->stopThread();
            if (ret != NO_ERROR) {
                CLOGE("SYNC stopThread fail, ret(%d)", ret);
                /* TODO: exception handling */
                funcRet |= ret;
            }
        }

        if (m_pipes[INDEX(PIPE_FUSION)] != NULL &&
                m_pipes[INDEX(PIPE_FUSION)]->isThreadRunning() == true) {
            ret = stopThread(INDEX(PIPE_FUSION));
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):PIPE_FUSION stopThread fail, ret(%d)", __FUNCTION__, __LINE__, ret);
                /* TODO: exception handling */
                funcRet |= ret;
            }
        }
    }
#endif

    if (m_parameters->isFlite3aaOtf() == true) {
        ret = m_pipes[PIPE_3AA]->sensorStream(false);
    } else
    {
        ret = m_pipes[PIPE_FLITE]->sensorStream(false);
    }
    if (ret != NO_ERROR) {
        CLOGE("FLITE sensorStream off fail, ret(%d)", ret);
        /* TODO: exception handling */
        funcRet |= ret;
    }

    if (m_parameters->isFlite3aaOtf() == false)
    {
        ret = m_pipes[PIPE_FLITE]->stop();
        if (ret != NO_ERROR) {
            CLOGE("FLITE stop fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    /* 3AA force done */
    ret = m_pipes[PIPE_3AA]->forceDone(V4L2_CID_IS_FORCE_DONE, 0x1000);
    if (ret != NO_ERROR) {
        CLOGE("PIPE_3AA force done fail, ret(%d)", ret);
        /* TODO: exception handling */
        funcRet |= ret;
    }

    /* stream off for 3AA */
    ret = m_pipes[PIPE_3AA]->stop();
    if (ret != NO_ERROR) {
        CLOGE("3AA stop fail, ret(%d)", ret);
        /* TODO: exception handling */
        funcRet |= ret;
    }

    if (m_pipes[PIPE_ISP]->flagStart() == true) {
        /* ISP force done */
        ret = m_pipes[PIPE_ISP]->forceDone(V4L2_CID_IS_FORCE_DONE, 0x1000);
        if (ret != NO_ERROR) {
            CLOGE("PIPE_ISP force done fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }

        /* stream off for ISP */
        ret = m_pipes[PIPE_ISP]->stop();
        if (ret != NO_ERROR) {
            CLOGE("ISP stop fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    if (m_pipes[PIPE_TPU]->flagStart() == true) {
        /* TPU force done */
        ret = m_pipes[PIPE_TPU]->forceDone(V4L2_CID_IS_FORCE_DONE, 0x1000);
        if (ret != NO_ERROR) {
            CLOGE("PIPE_TPU force done fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }

        ret = m_pipes[PIPE_TPU]->stop();
        if (ret != NO_ERROR) {
            CLOGE("TPU stop fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    if (m_pipes[PIPE_MCSC]->flagStart() == true) {
        /* MCSC force done */
        ret = m_pipes[PIPE_MCSC]->forceDone(V4L2_CID_IS_FORCE_DONE, 0x1000);
        if (ret != NO_ERROR) {
            CLOGE("PIPE_MCSC force done fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }

        ret = m_pipes[PIPE_MCSC]->stop();
        if (ret != NO_ERROR) {
            CLOGE("MCSC stop fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true) {
        if (m_pipes[INDEX(PIPE_SYNC)] != NULL) {
            ret = m_pipes[INDEX(PIPE_SYNC)]->stop();
            if (ret != NO_ERROR) {
                CLOGE("SYNC stop fail, ret(%d)", ret);
                /* TODO: exception handling */
                funcRet |= ret;
            }
        }

        if (m_pipes[INDEX(PIPE_FUSION)] != NULL) {
            ret = m_pipes[INDEX(PIPE_FUSION)]->stop();
            if (ret < 0) {
                CLOGE("ERR(%s[%d]): m_pipes[INDEX(PIPE_FUSION)]->stop() fail, ret(%d)", __FUNCTION__, __LINE__, ret);
                /* TODO: exception handling */
                funcRet |= ret;
            }
        }
    }
#endif

#ifdef USE_GSC_FOR_PREVIEW
    ret = stopThreadAndWait(PIPE_GSC);
    if (ret != NO_ERROR) {
        CLOGE("PIPE_GSC stopThreadAndWait fail, ret(%d)", ret);
    }
#endif

    CLOGI("Stopping  Success!");

    return funcRet;
}

status_t ExynosCameraFrameFactoryPreview::startInitialThreads(void)
{
    status_t ret = NO_ERROR;

    CLOGV("start pre-ordered initial pipe thread");

    if (m_requestVC0 && m_parameters->isFlite3aaOtf() == false) {
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

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true) {
        ret = startThread(PIPE_SYNC);
        if (ret != NO_ERROR)
            return ret;
    }
#endif

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryPreview::setStopFlag(void)
{
    CLOGI("");

    status_t ret = NO_ERROR;

    if (m_parameters->isFlite3aaOtf() == false)
    {
        ret = m_pipes[PIPE_FLITE]->setStopFlag();
    }

    if (m_pipes[PIPE_3AA]->flagStart() == true)
        ret = m_pipes[PIPE_3AA]->setStopFlag();

    if (m_pipes[PIPE_ISP]->flagStart() == true)
        ret = m_pipes[PIPE_ISP]->setStopFlag();

    if (m_pipes[PIPE_TPU]->flagStart() == true)
        ret = m_pipes[PIPE_TPU]->setStopFlag();

    if (m_pipes[PIPE_MCSC]->flagStart() == true)
        ret = m_pipes[PIPE_MCSC]->setStopFlag();

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true) {
        if (m_pipes[INDEX(PIPE_SYNC)] != NULL &&
                m_pipes[INDEX(PIPE_SYNC)]->flagStart() == true)
            ret = m_pipes[INDEX(PIPE_SYNC)]->setStopFlag();

        if (m_pipes[INDEX(PIPE_FUSION)] != NULL &&
                m_pipes[INDEX(PIPE_FUSION)]->flagStart() == true)
            ret = m_pipes[INDEX(PIPE_FUSION)]->setStopFlag();
    }
#endif

    return NO_ERROR;
}

ExynosCameraFrameSP_sptr_t ExynosCameraFrameFactoryPreview::createNewFrame(ExynosCameraFrameSP_sptr_t refFrame)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameEntity *newEntity[MAX_NUM_PIPES] = {0};
    ExynosCameraFrameSP_sptr_t frame = m_frameMgr->createFrame(m_parameters, m_frameCount, FRAME_TYPE_PREVIEW);

    int requestEntityCount = 0;
    int pipeId = -1;
    int parentPipeId = -1;
#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    int masterCameraId = -1;
    int slaveCameraId = -1;
#endif

    ret = m_initFrameMetadata(frame);
    if (ret != NO_ERROR)
        CLOGE("frame(%d) metadata initialize fail", m_frameCount);

    if (m_requestVC0 == true && m_flagFlite3aaOTF == false) {
        /* set flite pipe to linkageList */
        pipeId = PIPE_FLITE;

        newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
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
        frame->addChildEntity(newEntity[parentPipeId], newEntity[pipeId], PIPE_ISPC);
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

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true) {
        getDualCameraId(&masterCameraId, &slaveCameraId);

        newEntity[INDEX(PIPE_SYNC)] = new ExynosCameraFrameEntity(PIPE_SYNC, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_SYNC)]);
        requestEntityCount++;

        /* slave frame's life cycle is util sync pipe */
        if (m_cameraId == slaveCameraId)
            goto SKIP_SLAVE_CAMERA;

        newEntity[INDEX(PIPE_FUSION)] = new ExynosCameraFrameEntity(PIPE_FUSION, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_FUSION)]);
        requestEntityCount++;
    }
#endif

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

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
SKIP_SLAVE_CAMERA:
#endif
    ret = m_initPipelines(frame);
    if (ret != NO_ERROR) {
        CLOGE("m_initPipelines fail, ret(%d)", ret);
    }

    /* TODO: make it dynamic */
    frame->setNumRequestPipe(requestEntityCount);

    m_fillNodeGroupInfo(frame);

    m_frameCount++;

    return frame;
}

status_t ExynosCameraFrameFactoryPreview::m_setupConfig(bool isFastAE)
{
    CLOGI("");

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
        m_requestISPC = 1;
    }
    m_requestMCSC0 = 1;

    /* FLITE ~ MCSC */
    ret = m_setDeviceInfo(isFastAE);
    if (ret != NO_ERROR) {
        CLOGE("m_setDeviceInfo() fail, ret(%d)",
                ret);
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

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    m_nodeNums[INDEX(PIPE_SYNC)][OUTPUT_NODE] = -1;

    m_nodeNums[INDEX(PIPE_FUSION)][OUTPUT_NODE] = 0;
    m_nodeNums[INDEX(PIPE_FUSION)][CAPTURE_NODE_1] = -1;
    m_nodeNums[INDEX(PIPE_FUSION)][CAPTURE_NODE_2] = -1;
#endif

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryPreview::m_constructMainPipes()
{
    CLOGI("");

    int pipeId = -1;

    /* FLITE */
    pipeId = PIPE_FLITE;
    m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraMCPipe(m_cameraId, m_parameters, m_flagReprocessing, &m_deviceInfo[pipeId]);
    m_pipes[pipeId]->setPipeName("PIPE_FLITE");
    m_pipes[pipeId]->setPipeId(pipeId);

    /* 3AA */
    pipeId = PIPE_3AA;
    m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraMCPipe(m_cameraId, m_parameters, m_flagReprocessing, &m_deviceInfo[pipeId]);
    m_pipes[pipeId]->setPipeName("PIPE_3AA");
    m_pipes[pipeId]->setPipeId(pipeId);

    /* ISP */
    pipeId = PIPE_ISP;
    m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraMCPipe(m_cameraId, m_parameters, m_flagReprocessing, &m_deviceInfo[pipeId]);
    m_pipes[pipeId]->setPipeName("PIPE_ISP");
    m_pipes[pipeId]->setPipeId(pipeId);

    /* TPU */
    pipeId = PIPE_TPU;
    m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraMCPipe(m_cameraId, m_parameters, m_flagReprocessing, &m_deviceInfo[pipeId]);
    m_pipes[pipeId]->setPipeName("PIPE_TPU");
    m_pipes[pipeId]->setPipeId(pipeId);

    /* MCSC */

    pipeId = PIPE_MCSC;
    m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraMCPipe(m_cameraId, m_parameters, m_flagReprocessing, &m_deviceInfo[pipeId]);
    m_pipes[pipeId]->setPipeName("PIPE_MCSC");
    m_pipes[pipeId]->setPipeId(pipeId);

    /* GSC */
    pipeId = PIPE_GSC;
    m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraPipeGSC(m_cameraId, m_parameters, m_flagReprocessing, m_nodeNums[pipeId]);
    m_pipes[PIPE_GSC]->setPipeName("PIPE_GSC");
    m_pipes[PIPE_GSC]->setPipeId(PIPE_GSC);

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true) {
        /* Sync */
        ExynosCameraPipeSync *syncPipe = new ExynosCameraPipeSync(m_cameraId, m_parameters, false, m_nodeNums[INDEX(PIPE_SYNC)]);
        m_pipes[INDEX(PIPE_SYNC)] = (ExynosCameraPipe*)syncPipe;
        m_pipes[INDEX(PIPE_SYNC)]->setPipeId(PIPE_SYNC);
        m_pipes[INDEX(PIPE_SYNC)]->setPipeName("PIPE_SYNC");

        /* Fusion */
        ExynosCameraPipeFusion *fusionPipe = new ExynosCameraPipeFusion(m_cameraId, m_parameters, false, m_nodeNums[INDEX(PIPE_FUSION)]);
        m_pipes[INDEX(PIPE_FUSION)] = (ExynosCameraPipe*)fusionPipe;
        m_pipes[INDEX(PIPE_FUSION)]->setPipeId(PIPE_FUSION);
        m_pipes[INDEX(PIPE_FUSION)]->setPipeName("PIPE_FUSION");

        /* set the dual selector or fusionWrapper */
        ExynosCameraDualPreviewFrameSelector *dualSelector = ExynosCameraSingleton<ExynosCameraDualPreviewFrameSelector>::getInstance();
#ifdef USE_CP_FUSION_LIB
        ExynosCameraFusionWrapper *fusionWrapper = ExynosCameraSingleton<ExynosCameraFusionPreviewWrapperCP>::getInstance();
#else
        ExynosCameraFusionWrapper *fusionWrapper = ExynosCameraSingleton<ExynosCameraFusionPreviewWrapper>::getInstance();
#endif
        syncPipe->setDualSelector(dualSelector);
        fusionPipe->setDualSelector(dualSelector);
        fusionPipe->setFusionWrapper(fusionWrapper);
    }
#endif

    /* GSC for Recording */
    pipeId = PIPE_GSC_VIDEO;
    m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraPipeGSC(m_cameraId, m_parameters, m_flagReprocessing, m_nodeNums[pipeId]);
    m_pipes[pipeId]->setPipeName("PIPE_GSC_VIDEO");
    m_pipes[pipeId]->setPipeId(pipeId);

    /* GSC for Capture */
    pipeId = PIPE_GSC_PICTURE;
    m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraPipeGSC(m_cameraId, m_parameters, m_flagReprocessing, m_nodeNums[pipeId]);
    m_pipes[pipeId]->setPipeName("PIPE_GSC_PICTURE");
    m_pipes[pipeId]->setPipeId(pipeId);

    /* JPEG */
    pipeId = PIPE_JPEG;
    m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraPipeJpeg(m_cameraId, m_parameters, m_flagReprocessing, m_nodeNums[pipeId]);
    m_pipes[pipeId]->setPipeName("PIPE_JPEG");
    m_pipes[pipeId]->setPipeId(pipeId);

    CLOGI("pipe ids for preview");
    for (int i = 0; i < MAX_NUM_PIPES; i++) {
        if (m_pipes[i] != NULL) {
            CLOGI("-> m_pipes[%d] : PipeId(%d), Name(%s)" , i, m_pipes[i]->getPipeId(), m_pipes[i]->getPipeName());
        }
    }

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryPreview::m_setDeviceInfo(bool isFastAE)
{
    CLOGI("");


    int pipeId = -1;
    int node3aa = -1, node3ac = -1, node3ap = -1;
    int nodeIsp = -1, nodeIspc = -1, nodeIspp = -1;
    int nodeTpu = -1;
    int nodeMcsc = -1, nodeMcscp0 = -1, nodeMcscp1 = -1, nodeMcscp2 = -1;
    int previousPipeId = -1;
    int mcscSrcPipeId = -1;
    unsigned int flite3aaConnectionMode = (unsigned int)m_flagFlite3aaOTF;
    unsigned int taaIspConnectionMode = (unsigned int)m_flag3aaIspOTF;
    unsigned int ispMcscConnectionMode = (unsigned int)m_flagIspMcscOTF;
    unsigned int ispTpuConnectionMode = (unsigned int)m_flagIspTpuOTF;
    unsigned int tpuMcscConnectionMode = (unsigned int)m_flagTpuMcscOTF;
    unsigned int mcscConnectionMode = 0;
    enum NODE_TYPE nodeType = INVALID_NODE;
    bool flagStreamLeader = true;

    m_initDeviceInfo(PIPE_FLITE);
    m_initDeviceInfo(PIPE_3AA);
    m_initDeviceInfo(PIPE_ISP);
    m_initDeviceInfo(PIPE_TPU);
    m_initDeviceInfo(PIPE_MCSC);

    if (((m_cameraId == CAMERA_ID_FRONT || m_cameraId == CAMERA_ID_BACK_1) &&
        (m_parameters->getDualMode() == true)) ||
        (m_parameters->getUseCompanion() == false)) {
        node3aa = FIMC_IS_VIDEO_30S_NUM;
        node3ac = FIMC_IS_VIDEO_30C_NUM;
        node3ap = FIMC_IS_VIDEO_30P_NUM;
        nodeIsp = FIMC_IS_VIDEO_I0S_NUM;
        nodeIspc = FIMC_IS_VIDEO_I0C_NUM;
        nodeIspp = FIMC_IS_VIDEO_I0P_NUM;
        nodeMcsc = FIMC_IS_VIDEO_M0S_NUM;
        nodeMcscp0 = FIMC_IS_VIDEO_M0P_NUM;
        nodeMcscp1 = FIMC_IS_VIDEO_M1P_NUM;
        nodeMcscp2 = FIMC_IS_VIDEO_M2P_NUM;
    } else {
        node3aa = FIMC_IS_VIDEO_31S_NUM;
        node3ac = FIMC_IS_VIDEO_31C_NUM;
        node3ap = FIMC_IS_VIDEO_31P_NUM;
        nodeIsp = FIMC_IS_VIDEO_I0S_NUM;
        nodeIspc = FIMC_IS_VIDEO_I0C_NUM;
        nodeIspp = FIMC_IS_VIDEO_I0P_NUM;
        nodeMcsc = FIMC_IS_VIDEO_M0S_NUM;
        nodeMcscp0 = FIMC_IS_VIDEO_M0P_NUM;
        nodeMcscp1 = FIMC_IS_VIDEO_M1P_NUM;
        nodeMcscp2 = FIMC_IS_VIDEO_M2P_NUM;
    }

    nodeTpu = FIMC_IS_VIDEO_D0S_NUM;

    if (m_cameraId == CAMERA_ID_BACK || m_parameters->getDualMode() == false) {
#ifdef USE_FLITE_3AA_BUFFER_HIDING_MODE
        if (USE_FLITE_3AA_BUFFER_HIDING_MODE == true && m_flagFlite3aaOTF == true)
            flite3aaConnectionMode = HW_CONNECTION_MODE_M2M_BUFFER_HIDING;
#endif
#ifdef USE_3AA_ISP_BUFFER_HIDING_MODE
        if (USE_3AA_ISP_BUFFER_HIDING_MODE == true && m_flag3aaIspOTF == true)
            taaIspConnectionMode = HW_CONNECTION_MODE_M2M_BUFFER_HIDING;
#endif
#ifdef USE_ISP_MCSC_BUFFER_HIDING_MODE
        if (USE_ISP_MCSC_BUFFER_HIDING_MODE == true && m_flagIspMcscOTF == true)
            ispMcscConnectionMode = HW_CONNECTION_MODE_M2M_BUFFER_HIDING;
#endif
#ifdef USE_ISP_TPU_BUFFER_HIDING_MODE
        if (USE_ISP_TPU_BUFFER_HIDING_MODE == true && m_flagIspTpuOTF == true)
            ispTpuConnectionMode = HW_CONNECTION_MODE_M2M_BUFFER_HIDING;
#endif
#ifdef USE_TPU_MCSC_BUFFER_HIDING_MODE
        if (USE_TPU_MCSC_BUFFER_HIDING_MODE == true && m_flagTpuMcscOTF == true)
            tpuMcscConnectionMode = HW_CONNECTION_MODE_M2M_BUFFER_HIDING;
#endif
    }

    /*
     * FLITE
     */
    bool flagUseCompanion = false;
    bool flagQuickSwitchFlag = false;

#ifdef SAMSUNG_COMPANION
    flagUseCompanion = m_parameters->getUseCompanion();
#endif

#ifdef SAMSUNG_QUICK_SWITCH
    flagQuickSwitchFlag = m_parameters->getQuickSwitchFlag();
#endif

    if (m_flagFlite3aaOTF == true) {
        pipeId = PIPE_3AA;
    } else
    {
        pipeId = PIPE_FLITE;
    }

    /* FLITE */
    nodeType = getNodeType(PIPE_FLITE);
    m_deviceInfo[pipeId].pipeId[nodeType]  = PIPE_FLITE;
    m_deviceInfo[pipeId].nodeNum[nodeType] = getFliteNodenum(m_cameraId, flagUseCompanion, flagQuickSwitchFlag);
    m_deviceInfo[pipeId].connectionMode[nodeType] = HW_CONNECTION_MODE_OTF;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "FLITE", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[nodeType], false, flagStreamLeader, m_flagReprocessing);

    /* Other nodes is not stream leader */
    flagStreamLeader = false;

    /* VC0 for bayer */
    nodeType = getNodeType(PIPE_VC0);
    m_deviceInfo[pipeId].pipeId[nodeType]  = PIPE_VC0;
    m_deviceInfo[pipeId].connectionMode[nodeType] = flite3aaConnectionMode;
    m_deviceInfo[pipeId].nodeNum[nodeType] = getFliteCaptureNodenum(m_cameraId, m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_FLITE)]);
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "BAYER", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_FLITE)], false, flagStreamLeader, m_flagReprocessing);

#ifdef SUPPORT_DEPTH_MAP
    /* VC1 for depth */
    if (m_parameters->getUseDepthMap()) {
        nodeType = getNodeType(PIPE_VC1);
        m_deviceInfo[pipeId].pipeId[nodeType]  = PIPE_VC1;
        m_deviceInfo[pipeId].nodeNum[nodeType] = getDepthVcNodeNum(m_cameraId, flagUseCompanion);
        m_deviceInfo[pipeId].connectionMode[nodeType] = HW_CONNECTION_MODE_OTF;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "DEPTH", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_FLITE)], false, flagStreamLeader, m_flagReprocessing);
    }
#endif // SUPPORT_DEPTH_MAP

    /*
     * 3AA
     */
    previousPipeId = pipeId;
    pipeId = PIPE_3AA;

    /* 3AS */
    nodeType = getNodeType(PIPE_3AA);
    m_deviceInfo[pipeId].pipeId[nodeType]  = PIPE_3AA;
    m_deviceInfo[pipeId].nodeNum[nodeType] = node3aa;
    m_deviceInfo[pipeId].connectionMode[nodeType] = flite3aaConnectionMode;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "3AA_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    if (isFastAE == true && flite3aaConnectionMode == HW_CONNECTION_MODE_M2M_BUFFER_HIDING)
        flite3aaConnectionMode = false;

    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[previousPipeId].nodeNum[getNodeType(PIPE_VC0)], flite3aaConnectionMode, flagStreamLeader, m_flagReprocessing);

    /* 3AC */
    nodeType = getNodeType(PIPE_3AC);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_3AC;
    m_deviceInfo[pipeId].nodeNum[nodeType] = node3ac;
    m_deviceInfo[pipeId].connectionMode[nodeType] = HW_CONNECTION_MODE_OTF;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "3AA_CAPTURE", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_3AA)], true, flagStreamLeader, m_flagReprocessing);

    /* 3AP */
    nodeType = getNodeType(PIPE_3AP);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_3AP;
    m_deviceInfo[pipeId].nodeNum[nodeType] = node3ap;
    m_deviceInfo[pipeId].connectionMode[nodeType] = taaIspConnectionMode;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "3AA_PREVIEW", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_3AA)], true, flagStreamLeader, m_flagReprocessing);


    /*
     * ISP
     */
    previousPipeId = pipeId;
    mcscSrcPipeId = PIPE_ISPC;
    mcscConnectionMode = ispMcscConnectionMode;
    pipeId = m_flag3aaIspOTF ? PIPE_3AA : PIPE_ISP;

    /* ISPS */
    nodeType = getNodeType(PIPE_ISP);
    m_deviceInfo[pipeId].pipeId[nodeType]  = PIPE_ISP;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeIsp;
    m_deviceInfo[pipeId].connectionMode[nodeType] = taaIspConnectionMode;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "ISP_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    if (isFastAE == true && taaIspConnectionMode == HW_CONNECTION_MODE_M2M_BUFFER_HIDING)
        taaIspConnectionMode = false;
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[previousPipeId].nodeNum[getNodeType(PIPE_3AP)], taaIspConnectionMode, flagStreamLeader, m_flagReprocessing);

    /* ISPC */
    nodeType = getNodeType(PIPE_ISPC);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_ISPC;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeIspc;
    m_deviceInfo[pipeId].connectionMode[nodeType] = ispTpuConnectionMode;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "ISP_CAPTURE", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_ISP)], true, flagStreamLeader, m_flagReprocessing);
#if 0
    /* ISPP */
    nodeType = getNodeType(PIPE_ISPP);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_ISPP;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeIspp;
    m_deviceInfo[pipeId].connectionMode[nodeType] = (ispTpuConnectionMode | ispMcscConnectionMode);
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "ISP_PREVIEW", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_ISP)], true, flagStreamLeader, m_flagReprocessing);
#endif
    /*
     * TPU
     */
    if (m_parameters->getTpuEnabledMode() == true) {
        previousPipeId = pipeId;
        mcscSrcPipeId = PIPE_TPU;
        mcscConnectionMode = tpuMcscConnectionMode;

        if (m_flagIspTpuOTF == false)
            pipeId = PIPE_TPU;

        /* TPU */
        nodeType = getNodeType(PIPE_TPU);
        m_deviceInfo[pipeId].pipeId[nodeType]  = PIPE_TPU;
        m_deviceInfo[pipeId].nodeNum[nodeType] = nodeTpu;
        m_deviceInfo[pipeId].connectionMode[nodeType] = ispTpuConnectionMode;

        if (isFastAE == true && ispTpuConnectionMode == HW_CONNECTION_MODE_M2M_BUFFER_HIDING)
            ispTpuConnectionMode = false;

        if (m_parameters->getTpuEnabledMode() == true) {
            strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "TPU_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
            m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[previousPipeId].nodeNum[getNodeType(PIPE_ISPC)], ispTpuConnectionMode, flagStreamLeader, m_flagReprocessing);
        } else {
            strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "TPU_OTF", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
            m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[previousPipeId].nodeNum[getNodeType(PIPE_ISP)], ispTpuConnectionMode, flagStreamLeader, m_flagReprocessing);
        }
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
    m_deviceInfo[pipeId].connectionMode[nodeType] = mcscConnectionMode;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    if (isFastAE == true && mcscConnectionMode == HW_CONNECTION_MODE_M2M_BUFFER_HIDING)
        mcscConnectionMode = false;
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[previousPipeId].nodeNum[getNodeType(mcscSrcPipeId)], mcscConnectionMode, flagStreamLeader, m_flagReprocessing);

    /* MCSC0 */
    nodeType = getNodeType(PIPE_MCSC0);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC0;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcscp0;
    m_deviceInfo[pipeId].connectionMode[nodeType] = HW_CONNECTION_MODE_OTF;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_PREVIEW", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC)], true, flagStreamLeader, m_flagReprocessing);

#ifdef USE_MCSC1_FOR_PREVIEWCALLBACK
    /* MCSC1 */
    nodeType = getNodeType(PIPE_MCSC1);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC1;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcscp1;
    m_deviceInfo[pipeId].connectionMode[nodeType] = HW_CONNECTION_MODE_OTF;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_PREVIEW_CALLBACK", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC)], true, flagStreamLeader, m_flagReprocessing);
#endif

    if (m_cameraId == CAMERA_ID_BACK || m_parameters->getDualMode() == false) {
        /* MCSC2 */
        nodeType = getNodeType(PIPE_MCSC2);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC2;
        m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcscp2;
        m_deviceInfo[pipeId].connectionMode[nodeType] = HW_CONNECTION_MODE_OTF;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_RECORDING", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC)], true, flagStreamLeader, m_flagReprocessing);
    }

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryPreview::m_initPipes(uint32_t frameRate)
{
    CLOGI("");

    status_t ret = NO_ERROR;
    camera_pipe_info_t pipeInfo[MAX_NODE];
    camera_pipe_info_t nullPipeInfo;

    int pipeId = -1;
    enum NODE_TYPE nodeType = INVALID_NODE;
    enum NODE_TYPE leaderNodeType = OUTPUT_NODE;

    ExynosRect tempRect;
    int maxSensorW = 0, maxSensorH = 0, hwSensorW = 0, hwSensorH = 0;
    int maxPreviewW = 0, maxPreviewH = 0, hwPreviewW = 0, hwPreviewH = 0;
    int maxPictureW = 0, maxPictureH = 0, hwPictureW = 0, hwPictureH = 0;
    int videoH = 0, videoW = 0;
    int previewW = 0, previewH = 0;
    int bayerFormat = m_parameters->getBayerFormat(PIPE_3AA);
    int hwPreviewFormat = m_parameters->getHwPreviewFormat();
    int previewFormat = m_parameters->getPreviewFormat();
    int videoFormat = m_parameters->getVideoFormat();
    int pictureFormat = m_parameters->getHwPictureFormat();
    int hwVdisformat = m_parameters->getHWVdisFormat();
    struct ExynosConfigInfo *config = m_parameters->getConfig();
    ExynosRect bnsSize;
    ExynosRect bcropSize;
    ExynosRect bdsSize;
    int perFramePos = 0;
    int stride = 0;

#ifdef DEBUG_RAWDUMP
    if (m_parameters->checkBayerDumpEnable()) {
        bayerFormat = CAMERA_DUMP_BAYER_FORMAT;
    }
#endif

#ifdef SUPPORT_GROUP_MIGRATION
    m_updateNodeInfo();
#endif

    m_parameters->getMaxSensorSize(&maxSensorW, &maxSensorH);
    m_parameters->getHwSensorSize(&hwSensorW, &hwSensorH);

    m_parameters->getMaxPreviewSize(&maxPreviewW, &maxPreviewH);
    m_parameters->getHwPreviewSize(&hwPreviewW, &hwPreviewH);

    m_parameters->getMaxPictureSize(&maxPictureW, &maxPictureH);
    m_parameters->getHwPictureSize(&hwPictureW, &hwPictureH);

    m_parameters->getPreviewBayerCropSize(&bnsSize, &bcropSize, false);
    m_parameters->getPreviewBdsSize(&bdsSize, false);

    m_parameters->getHwVideoSize(&videoW, &videoH);
    m_parameters->getPreviewSize(&previewW, &previewH);
    /* When high speed recording mode, hw sensor size is fixed.
     * So, maxPreview size cannot exceed hw sensor size
     */
    if (m_parameters->getHighSpeedRecording()) {
        maxPreviewW = hwSensorW;
        maxPreviewH = hwSensorH;
    }

    CLOGI("MaxSensorSize(%dx%d), HWSensorSize(%dx%d)",
        maxSensorW, maxSensorH, hwSensorW, hwSensorH);
    CLOGI("MaxPreviewSize(%dx%d), HwPreviewSize(%dx%d)",
        maxPreviewW, maxPreviewH, hwPreviewW, hwPreviewH);
    CLOGI("HWPictureSize(%dx%d)", hwPictureW, hwPictureH);
    CLOGI("PreviewSize(%dx%d)", previewW, previewH);
    CLOGI("PreviewFormat(%d), HwPreviewFormat(%d), VideoFormat(%d)",
        previewFormat, hwPreviewFormat, videoFormat);

    /*
     * FLITE
     */
    if (m_flagFlite3aaOTF == true) {
        pipeId = PIPE_3AA;
    } else {
        pipeId = PIPE_FLITE;
    }

    /* setParam for Frame rate : must after setInput on Flite */
    struct v4l2_streamparm streamParam;
    memset(&streamParam, 0x0, sizeof(v4l2_streamparm));

    streamParam.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    streamParam.parm.capture.timeperframe.numerator   = 1;
    streamParam.parm.capture.timeperframe.denominator = frameRate;
    CLOGI("Set framerate (denominator=%d)", frameRate);

    ret = setParam(&streamParam, pipeId);
    if (ret != NO_ERROR) {
        CLOGE("FLITE setParam(frameRate(%d), pipeId(%d)) fail", frameRate, pipeId);
        return INVALID_OPERATION;
    }

    ret = m_setSensorSize(pipeId, hwSensorW, hwSensorH);
    if (ret != NO_ERROR) {
        CLOGE("m_setSensorSize(pipeId(%d), hwSensorW(%d), hwSensorH(%d)) fail", pipeId, hwSensorW, hwSensorH);
        return ret;
    }

    /* FLITE */
    nodeType = getNodeType(PIPE_FLITE);

    /* set v4l2 buffer size */
    tempRect.fullW = 32;
    tempRect.fullH = 64;
    tempRect.colorFormat = bayerFormat;

    /* set v4l2 video node buffer count */
    pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_3aa_buffers;

    /* Set output node default info */
    SET_OUTPUT_DEVICE_BASIC_INFO(PERFRAME_INFO_FLITE);

    /* BAYER */
    nodeType = getNodeType(PIPE_VC0);
    perFramePos = PERFRAME_BACK_VC0_POS;

    /* set v4l2 buffer size */
    tempRect.fullW = hwSensorW;
    tempRect.fullH = hwSensorH;
    tempRect.colorFormat = bayerFormat;

    /* set v4l2 video node bytes per plane */
    pipeInfo[nodeType].bytesPerPlane[0] = getBayerLineSize(tempRect.fullW, bayerFormat);

    /* set v4l2 video node buffer count */
    pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_bayer_buffers;

    /* Set capture node default info */
    SET_CAPTURE_DEVICE_BASIC_INFO();

#ifdef SUPPORT_DEPTH_MAP
    if (m_parameters->getUseDepthMap()) {
        /* Depth Map Configuration */
        int depthMapW = 0, depthMapH = 0;
        int depthMapFormat = DEPTH_MAP_FORMAT;

        ret = m_parameters->getDepthMapSize(&depthMapW, &depthMapH);
        if (ret != NO_ERROR) {
            CLOGE("Failed to getDepthMapSize");
            return ret;
        }

        CLOGI("DepthMapSize %dx%d", depthMapW, depthMapH);

        tempRect.fullW = depthMapW;
        tempRect.fullH = depthMapH;
        tempRect.colorFormat = depthMapFormat;

        nodeType = getNodeType(PIPE_VC1);
        pipeInfo[nodeType].bytesPerPlane[0] = getBayerLineSize(tempRect.fullW, tempRect.colorFormat);
        pipeInfo[nodeType].bufInfo.count = NUM_DEPTHMAP_BUFFERS;

        SET_CAPTURE_DEVICE_BASIC_INFO();
    }
#endif

    /* setup pipe info to FLITE pipe */
    if (m_flagFlite3aaOTF == false) {
        ret = m_pipes[pipeId]->setupPipe(pipeInfo, m_sensorIds[pipeId]);
        if (ret != NO_ERROR) {
            CLOGE("FLITE setupPipe fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }

        /* clear pipeInfo for next setupPipe */
        for (int i = 0; i < MAX_NODE; i++)
            pipeInfo[i] = nullPipeInfo;
    }

    /*
     * 3AA
     */
    pipeId = PIPE_3AA;

    /* 3AS */
    nodeType = getNodeType(PIPE_3AA);
    bayerFormat = m_parameters->getBayerFormat(PIPE_3AA);

    if (m_flagFlite3aaOTF == true
#ifdef USE_FLITE_3AA_BUFFER_HIDING_MODE
        && (USE_FLITE_3AA_BUFFER_HIDING_MODE == false
            || ((m_cameraId == CAMERA_ID_FRONT || m_cameraId == CAMERA_ID_BACK_1)
                && m_parameters->getDualMode() == true))
#endif
       ) {
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

        /* set v4l2 video node buffer count */
#ifdef USE_FLITE_3AA_BUFFER_HIDING_MODE
        if (USE_FLITE_3AA_BUFFER_HIDING_MODE == true
            && m_flagFlite3aaOTF == true
            && (m_cameraId == CAMERA_ID_BACK
                || m_parameters->getDualMode() == false))
            pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_hiding_mode_buffers;
        else
#endif
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

    /* set v4l2 video node bytes per plane */
    pipeInfo[nodeType].bytesPerPlane[0] = getBayerLineSize(tempRect.fullW, bayerFormat);

    /* set v4l2 video node buffer count */
    pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_bayer_buffers;

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

    /* set v4l2 video node buffer count */
#ifdef USE_3AA_ISP_BUFFER_HIDING_MODE
    if (USE_3AA_ISP_BUFFER_HIDING_MODE == true
        && m_flag3aaIspOTF == true
        && (m_cameraId == CAMERA_ID_BACK
            || m_parameters->getDualMode() == false))
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_hiding_mode_buffers;
    else
#endif
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_3aa_buffers;

    /* Set capture node default info */
    SET_CAPTURE_DEVICE_BASIC_INFO();

    /* setup pipe info to 3AA pipe */
    if (m_flag3aaIspOTF == false) {
        ret = m_pipes[pipeId]->setupPipe(pipeInfo, m_sensorIds[pipeId]);
        if (ret != NO_ERROR) {
            CLOGE("3AA setupPipe fail, ret(%d)", ret);
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
    if (m_flag3aaIspOTF == false)
        pipeId = PIPE_ISP;
    nodeType = getNodeType(PIPE_ISP);
    bayerFormat = m_parameters->getBayerFormat(PIPE_ISP);

    /* set v4l2 buffer size */
    tempRect.fullW = bdsSize.w;
    tempRect.fullH = bdsSize.h;
    tempRect.colorFormat = bayerFormat;

    /* set v4l2 video node bytes per plane */
    pipeInfo[nodeType].bytesPerPlane[0] = getBayerLineSize(tempRect.fullW, bayerFormat);

    /* set v4l2 video node buffer count */
#ifdef USE_3AA_ISP_BUFFER_HIDING_MODE
    if (USE_3AA_ISP_BUFFER_HIDING_MODE == true
        && m_flag3aaIspOTF == true
        && (m_cameraId == CAMERA_ID_BACK
            || m_parameters->getDualMode() == false))
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_hiding_mode_buffers;
    else
#endif
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_3aa_buffers;

    /* Set output node default info */
    SET_OUTPUT_DEVICE_BASIC_INFO(PERFRAME_INFO_ISP);

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
#ifdef USE_ISP_TPU_BUFFER_HIDING_MODE
        if (USE_ISP_TPU_BUFFER_HIDING_MODE == true
            && m_flagIspTpuOTF == true
            && (m_cameraId == CAMERA_ID_BACK
                || m_parameters->getDualMode() == false))
            pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_hiding_mode_buffers;
        else
#endif
#ifdef USE_ISP_MCSC_BUFFER_HIDING_MODE
        if (USE_ISP_MCSC_BUFFER_HIDING_MODE == true
            && m_flagIspMcscOTF == true
            && (m_cameraId == CAMERA_ID_BACK
                || m_parameters->getDualMode() == false))
            pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_hiding_mode_buffers;
        else
#endif
            pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_hwdis_buffers;

    /* Set capture node default info */
    SET_CAPTURE_DEVICE_BASIC_INFO();

    /* setup pipe info to ISP pipe */
    if (m_flagIspTpuOTF == false && m_flagIspMcscOTF == false) {
        ret = m_pipes[pipeId]->setupPipe(pipeInfo, m_sensorIds[pipeId]);
        if (ret != NO_ERROR) {
            CLOGE("ISP setupPipe fail, ret(%d)", ret);
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
        if (m_flagIspTpuOTF == false)
            pipeId = PIPE_TPU;
        nodeType = getNodeType(PIPE_TPU);

        /* set v4l2 buffer size */
        tempRect.fullW = bdsSize.w;
        tempRect.fullH = bdsSize.h;
        tempRect.colorFormat = hwVdisformat;

        /* set v4l2 video node bytes per plane */
#ifdef TPU_INPUT_CHUNK_TYPE
        pipeInfo[nodeType].bytesPerPlane[0] = ROUND_UP(tempRect.fullW * 2, CAMERA_TPU_CHUNK_ALIGN_W);
#else
        pipeInfo[nodeType].bytesPerPlane[0] = ROUND_UP(tempRect.fullW * 2, CAMERA_16PX_ALIGN);
#endif

        /* set v4l2 video node buffer count */
#ifdef USE_ISP_TPU_BUFFER_HIDING_MODE
        if (USE_ISP_TPU_BUFFER_HIDING_MODE == true
            && m_flagIspTpuOTF == true
            && (m_cameraId == CAMERA_ID_BACK
                || m_parameters->getDualMode() == false))
            pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_hiding_mode_buffers;
        else
#endif
            pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_hwdis_buffers;

        /* Set output node default info */
        SET_OUTPUT_DEVICE_BASIC_INFO(PERFRAME_INFO_TPU);

        /* setup pipe info to TPU pipe */
        if (m_flagTpuMcscOTF == false) {
            ret = m_pipes[pipeId]->setupPipe(pipeInfo, m_sensorIds[pipeId]);
            if (ret != NO_ERROR) {
                CLOGE("TPU setupPipe fail, ret(%d)", ret);
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
    if (m_flagIspMcscOTF == false && m_flagTpuMcscOTF == false)
        pipeId = PIPE_MCSC;
    nodeType = getNodeType(PIPE_MCSC);

    /* set v4l2 buffer size */
    tempRect.fullW = bdsSize.w;
    tempRect.fullH = bdsSize.h;
    tempRect.colorFormat = hwVdisformat;

    /* set v4l2 video node bytes per plane */
    pipeInfo[nodeType].bytesPerPlane[0] = ROUND_UP(tempRect.fullW * 2, CAMERA_16PX_ALIGN);

    /* set v4l2 video node buffer count */
#ifdef USE_ISP_MCSC_BUFFER_HIDING_MODE
    if (USE_ISP_MCSC_BUFFER_HIDING_MODE == true
        && m_flagIspMcscOTF == true
        && (m_cameraId == CAMERA_ID_BACK
            || m_parameters->getDualMode() == false))
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_hiding_mode_buffers;
    else
#endif
#ifdef USE_TPU_MCSC_BUFFER_HIDING_MODE
        if (USE_ISP_TPU_BUFFER_HIDING_MODE == true
            && m_flagTpuMcscOTF == true
            && (m_cameraId == CAMERA_ID_BACK
                || m_parameters->getDualMode() == false))
            pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_hiding_mode_buffers;
        else
#endif
            pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_hwdis_buffers;

    /* Set output node default info */
    SET_OUTPUT_DEVICE_BASIC_INFO(PERFRAME_INFO_MCSC);

    /* MCSC0 */
    nodeType = getNodeType(PIPE_MCSC0);
    perFramePos = PERFRAME_BACK_SCP_POS;

    /* set v4l2 buffer size */
    stride = m_parameters->getHwPreviewStride();
    CLOGV("stride=%d", stride);
    tempRect.fullW = stride;
    tempRect.fullH = hwPreviewH;
    tempRect.colorFormat = hwPreviewFormat;

    /* set v4l2 video node bytes per plane */
#ifdef USE_BUFFER_WITH_STRIDE
    /* to use stride for preview buffer, set the bytesPerPlane */
    pipeInfo[nodeType].bytesPerPlane[0] = hwPreviewW;
#endif

    /* set v4l2 video node buffer count */
    if (m_parameters->increaseMaxBufferOfPreview() == true)
        pipeInfo[nodeType].bufInfo.count = m_parameters->getPreviewBufferCount();
    else
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_preview_buffers;

    /* Set capture node default info */
    SET_CAPTURE_DEVICE_BASIC_INFO();

    /* MCSC1 */
    nodeType = getNodeType(PIPE_MCSC1);
    perFramePos = PERFRAME_BACK_MCSC1_POS;

    /* set v4l2 buffer size */
    stride = m_parameters->getHwPreviewStride();
    CLOGV("stride=%d", stride);
    if (m_parameters->getHighResolutionCallbackMode() == true) {
        tempRect.fullW = stride;
        tempRect.fullH = hwPreviewH;
    } else {
        tempRect.fullW = previewW;
        tempRect.fullH = previewH;
    }
    tempRect.colorFormat = previewFormat;

    /* set v4l2 video node bytes per plane */
#ifdef USE_BUFFER_WITH_STRIDE
    /* to use stride for preview buffer, set the bytesPerPlane */
    if (m_parameters->getHighResolutionCallbackMode() == true) {
        pipeInfo[nodeType].bytesPerPlane[0] = hwPreviewW;
    } else {
        pipeInfo[nodeType].bytesPerPlane[0] = previewW;
    }
#endif

    /* set v4l2 video node buffer count */
    if (m_parameters->increaseMaxBufferOfPreview() == true)
        pipeInfo[nodeType].bufInfo.count = m_parameters->getPreviewBufferCount();
    else
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_preview_buffers;

    /* Set capture node default info */
    SET_CAPTURE_DEVICE_BASIC_INFO();

    if (m_cameraId == CAMERA_ID_BACK || m_parameters->getDualMode() == false) {
        /* MCSC2 */
        nodeType = getNodeType(PIPE_MCSC2);
        perFramePos = PERFRAME_BACK_MCSC2_POS;

        /* set v4l2 buffer size */
        stride = m_parameters->getHwPreviewStride();
        CLOGV("stride=%d", stride);
        tempRect.fullW = videoW;
        tempRect.fullH = videoH;
        tempRect.colorFormat = videoFormat;

        /* set v4l2 video node buffer count */
        if (m_parameters->increaseMaxBufferOfPreview() == true)
            pipeInfo[nodeType].bufInfo.count = m_parameters->getPreviewBufferCount();
        else
            pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_recording_buffers;

        /* Set capture node default info */
        SET_CAPTURE_DEVICE_BASIC_INFO();
    }

    ret = m_pipes[pipeId]->setupPipe(pipeInfo, m_sensorIds[pipeId]);
    if (ret != NO_ERROR) {
        CLOGE("MCSC setupPipe fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryPreview::m_initPipesFastenAeStable(int32_t numFrames,
                                                                         int hwSensorW,
                                                                         int hwSensorH,
                                                                         uint32_t frameRate)
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
    int hwPreviewFormat = m_parameters->getHwPreviewFormat();
    int perFramePos = 0;

#ifdef DEBUG_RAWDUMP
    if (m_parameters->checkBayerDumpEnable()) {
        bayerFormat = CAMERA_DUMP_BAYER_FORMAT;
    }
#endif

    /*
     * FLITE
     */
    if (m_flagFlite3aaOTF == true) {
        pipeId = PIPE_3AA;
    } else {
        pipeId = PIPE_FLITE;
    }

    /* setParam for Frame rate : must after setInput on Flite */
    struct v4l2_streamparm streamParam;
    memset(&streamParam, 0x0, sizeof(v4l2_streamparm));

    streamParam.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    streamParam.parm.capture.timeperframe.numerator   = 1;
    streamParam.parm.capture.timeperframe.denominator = frameRate;
    CLOGI("Set framerate (denominator=%d)", frameRate);

    ret = setParam(&streamParam, pipeId);
    if (ret != NO_ERROR) {
        CLOGE("FLITE setParam(frameRate(%d), pipeId(%d)) fail", frameRate, pipeId);
        return INVALID_OPERATION;
    }

    ret = m_setSensorSize(pipeId, hwSensorW, hwSensorH);
    if (ret != NO_ERROR) {
        CLOGE("m_setSensorSize(pipeId(%d), hwSensorW(%d), hwSensorH(%d)) fail", pipeId, hwSensorW, hwSensorH);
        return ret;
    }

    /* FLITE */
    nodeType = getNodeType(PIPE_FLITE);

    /* set v4l2 buffer size */
    tempRect.fullW = 32;
    tempRect.fullH = 64;
    tempRect.colorFormat = bayerFormat;

    /* set v4l2 video node buffer count */
    pipeInfo[nodeType].bufInfo.count = numFrames;

    /* Set output node default info */
    SET_OUTPUT_DEVICE_BASIC_INFO(PERFRAME_INFO_FLITE);

    /* BAYER */
    nodeType = getNodeType(PIPE_VC0);
    perFramePos = PERFRAME_BACK_VC0_POS;

    /* set v4l2 buffer size */
    tempRect.fullW = hwSensorW;
    tempRect.fullH = hwSensorH;
    tempRect.colorFormat = bayerFormat;

    /* packed bayer bytesPerPlane */
    pipeInfo[nodeType].bytesPerPlane[0] = getBayerLineSize(tempRect.fullW, bayerFormat);

    /* set v4l2 video node buffer count */
    pipeInfo[nodeType].bufInfo.count = numFrames;

    /* Set capture node default info */
    SET_CAPTURE_DEVICE_BASIC_INFO();

    /* setup pipe info to FLITE pipe */
    if (m_flagFlite3aaOTF == false) {
        ret = m_pipes[pipeId]->setupPipe(pipeInfo, m_sensorIds[pipeId]);
        if (ret != NO_ERROR) {
            CLOGE("FLITE setupPipe fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }

        /* clear pipeInfo for next setupPipe */
        for (int i = 0; i < MAX_NODE; i++)
            pipeInfo[i] = nullPipeInfo;
    }

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
            CLOGE("3AA setupPipe fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }

        /* clear pipeInfo for next setupPipe */
        for (int i = 0; i < MAX_NODE; i++)
            pipeInfo[i] = nullPipeInfo;
    }

    /* ISPS */
    if (m_flag3aaIspOTF == false)
        pipeId = PIPE_ISP;
    nodeType = getNodeType(PIPE_ISP);
    bayerFormat = m_parameters->getBayerFormat(PIPE_ISP);

    tempRect.colorFormat = bayerFormat;
    pipeInfo[nodeType].bufInfo.count = numFrames;

    SET_OUTPUT_DEVICE_BASIC_INFO(PERFRAME_INFO_ISP);

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
            CLOGE("ISP setupPipe fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }

        /* clear pipeInfo for next setupPipe */
        for (int i = 0; i < MAX_NODE; i++)
            pipeInfo[i] = nullPipeInfo;
    }

    /* TPU */
    if (m_parameters->getTpuEnabledMode() == true) {
        if (m_flagIspTpuOTF == false)
            pipeId = PIPE_TPU;
        nodeType = getNodeType(PIPE_TPU);

        pipeInfo[nodeType].bufInfo.count = numFrames;

        SET_OUTPUT_DEVICE_BASIC_INFO(PERFRAME_INFO_ISP);

        /* setup pipe info to TPU pipe */
        if (m_flagTpuMcscOTF == false) {
            ret = m_pipes[pipeId]->setupPipe(pipeInfo, m_sensorIds[pipeId]);
            if (ret != NO_ERROR) {
                CLOGE("TPU setupPipe fail, ret(%d)", ret);
                /* TODO: exception handling */
                return INVALID_OPERATION;
            }

            /* clear pipeInfo for next setupPipe */
            for (int i = 0; i < MAX_NODE; i++)
                pipeInfo[i] = nullPipeInfo;
        }
    }

    /* MCSC */
    if (m_flagIspMcscOTF == false && m_flagTpuMcscOTF == false)
        pipeId = PIPE_MCSC;
    nodeType = getNodeType(PIPE_MCSC);

    pipeInfo[nodeType].bufInfo.count = numFrames;

    SET_OUTPUT_DEVICE_BASIC_INFO(PERFRAME_INFO_MCSC);

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
        CLOGE("MCSC setupPipe fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryPreview::m_fillNodeGroupInfo(ExynosCameraFrameSP_sptr_t frame)
{
    size_control_info_t sizeControlInfo;
    camera2_node_group node_group_info_flite;
    camera2_node_group node_group_info_3aa;
    camera2_node_group node_group_info_isp;
    camera2_node_group node_group_info_tpu;
    camera2_node_group node_group_info_mcsc;
    camera2_node_group *node_group_info_temp;

    int zoom = m_parameters->getZoomLevel();
    int pipeId = -1;
    uint32_t perframePosition = 0;

    /* for capture node's request */
    uint32_t requestVC0 = m_requestVC0;
    uint32_t requestVC1 = m_requestVC1;
    uint32_t request3AC = m_request3AC;
    uint32_t request3AP = m_request3AP;
    uint32_t requestISPC = m_requestISPC;
    uint32_t requestISPP = m_requestISPP;
    uint32_t requestMCSC0 = m_requestMCSC0;
    uint32_t requestMCSC1 = m_requestMCSC1;
    uint32_t requestMCSC2 = m_requestMCSC2;

    memset(&node_group_info_flite, 0x0, sizeof(camera2_node_group));
    memset(&node_group_info_3aa, 0x0, sizeof(camera2_node_group));
    memset(&node_group_info_isp, 0x0, sizeof(camera2_node_group));
    memset(&node_group_info_tpu, 0x0, sizeof(camera2_node_group));
    memset(&node_group_info_mcsc, 0x0, sizeof(camera2_node_group));

    /* TODO:should be moved to parameter class */
#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true) {
        int masterCameraId = -1;
        int slaveCameraId = -1;

        sync_type_t syncType = SYNC_TYPE_BASE;
        frame_type_t frameType = FRAME_TYPE_PREVIEW;

        getDualCameraId(&masterCameraId, &slaveCameraId);

        /*
         *  master camera's zoom range
         *    BYPASS ~ SYNC
         *  slave camera's zoom range
         *    SYNC ~ SWITCH
         */
        if (zoom > DUAL_SYNC_MODE_MAX_ZOOM_LEVEL) {
            /* SWITCH MODE */
            if (m_cameraId == masterCameraId) {
                /* over than zoom range in this camera */
                zoom = DUAL_SYNC_MODE_MAX_ZOOM_LEVEL;
                syncType = SYNC_TYPE_SYNC;
                frameType = FRAME_TYPE_TRANSITION;
            } else {
                syncType = SYNC_TYPE_SWITCH;
            }
        } else if (zoom < DUAL_SYNC_MODE_MIN_ZOOM_LEVEL) {
            /* BYPASS MODE */
            if (m_cameraId == slaveCameraId) {
                /* over than zoom range in this camera */
                zoom = DUAL_SYNC_MODE_MIN_ZOOM_LEVEL;
                syncType = SYNC_TYPE_SYNC;
                frameType = FRAME_TYPE_TRANSITION;
            } else {
                syncType = SYNC_TYPE_BYPASS;
            }
        } else {
            /* SYNC MODE */
            syncType = SYNC_TYPE_SYNC;
        }

        /* check the internal frame */
        if (frameType == FRAME_TYPE_TRANSITION) {
            if (m_preSyncType != SYNC_TYPE_BASE) {
                /* set the previous information */
                syncType = m_preSyncType;
                zoom = m_preZoom;
            }

            if (m_transitionCount > 0) {
                m_transitionCount--;
            } else {
                /* make internal frame */
                frameType = FRAME_TYPE_INTERNAL;
            }
        } else {
            m_transitionCount = DUAL_TRANSITION_COUNT;
        }

        frame->setSyncType(syncType);
        frame->setFrameType(frameType);

        /* backup for next frame */
        m_preSyncType = syncType;
        m_preZoom = zoom;

        /* for internal frame */
        if (frame->getFrameType() == FRAME_TYPE_INTERNAL) {
            /*
             * Clear all capture node's request.
             * But don't touch MCSC0 for preview in master camera.
             */
            requestVC0 = 0;
            requestVC1 = 0;
            request3AC = 0;
            request3AP = 0;
            requestISPC = 0;
            requestISPP = 0;
            /* requestMCSC0 = 0; */
            requestMCSC1 = 0;
            requestMCSC2 = 0;

            frame->setRequest(PIPE_VC0, requestVC0);
            frame->setRequest(PIPE_VC1, requestVC1);
            frame->setRequest(PIPE_3AC, request3AC);
            frame->setRequest(PIPE_3AP, request3AP);
            frame->setRequest(PIPE_ISPC, requestISPC);
            frame->setRequest(PIPE_ISPP, requestISPP);
            /* frame->setRequest(PIPE_MCSC0, requestMCSC0); */
            frame->setRequest(PIPE_MCSC1, requestMCSC1);
            frame->setRequest(PIPE_MCSC2, requestMCSC2);
        }

        CLOGV("frame(%d) : zoom(%d), frame type(%d), syncType(%d), transitionCount(%d)",
                frame->getFrameCount(), zoom, frame->getFrameType(),
                frame->getSyncType(), m_transitionCount);
    }
#endif


    if (m_flagFlite3aaOTF == true) {
        /* FLITE */
        pipeId = PIPE_3AA; // this is hack. (not PIPE_FLITE)
        perframePosition = 0;

        node_group_info_temp = &node_group_info_flite;
        node_group_info_temp->leader.request = 1;

        node_group_info_temp->capture[perframePosition].request = requestVC0;
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_VC0)] - FIMC_IS_VIDEO_BAS_NUM;
        perframePosition++;

#ifdef SUPPORT_DEPTH_MAP
        /* VC1 for depth */
        if (m_parameters->getUseDepthMap() == true) {
            node_group_info_temp->capture[perframePosition].request = requestVC1;
            node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_VC1)] - FIMC_IS_VIDEO_BAS_NUM;
            perframePosition++;
        }
#endif

        node_group_info_temp->capture[perframePosition].request = request3AC;
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_3AC)] - FIMC_IS_VIDEO_BAS_NUM;
        perframePosition++;

        node_group_info_temp->capture[perframePosition].request = request3AP;
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_3AP)] - FIMC_IS_VIDEO_BAS_NUM;
        perframePosition++;
    } else {
        /* FLITE */
        pipeId = PIPE_FLITE;
        perframePosition = 0;

        node_group_info_temp = &node_group_info_flite;
        node_group_info_temp->leader.request = 1;

        node_group_info_temp->capture[perframePosition].request = requestVC0;
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_VC0)] - FIMC_IS_VIDEO_BAS_NUM;
        perframePosition++;

#ifdef SUPPORT_DEPTH_MAP
        /* VC1 for depth */
        if (m_parameters->getUseDepthMap() == true) {
            node_group_info_temp->capture[perframePosition].request = requestVC1;
            node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_VC1)] - FIMC_IS_VIDEO_BAS_NUM;
            perframePosition++;
        }
#endif

        /* 3AA */
        pipeId = PIPE_3AA;
        perframePosition = 0;
        node_group_info_temp = &node_group_info_3aa;

        node_group_info_temp->leader.request = 1;

        node_group_info_temp->capture[perframePosition].request = request3AC;
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_3AC)] - FIMC_IS_VIDEO_BAS_NUM;
        perframePosition++;

        node_group_info_temp->capture[perframePosition].request = request3AP;
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_3AP)] - FIMC_IS_VIDEO_BAS_NUM;
        perframePosition++;
    }

    /* ISP */
    if (m_flag3aaIspOTF == false) {
        pipeId = PIPE_ISP;
        perframePosition = 0;
        node_group_info_temp = &node_group_info_isp;
        node_group_info_temp->leader.request = 1;
    }

    /* TPU */
    if (m_parameters->getTpuEnabledMode() == true && m_flagIspTpuOTF == false) {
        node_group_info_temp->capture[perframePosition].request = requestISPC;
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_ISPC)] - FIMC_IS_VIDEO_BAS_NUM;
        perframePosition++;

        pipeId = PIPE_TPU;
        perframePosition = 0;
        node_group_info_temp = &node_group_info_tpu;
        node_group_info_temp->leader.request = 1;
    } else if (m_parameters->getTpuEnabledMode() == false && m_flagIspMcscOTF == false) {
        node_group_info_temp->capture[perframePosition].request = requestISPC;
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

    if (perframePosition < CAPTURE_NODE_MAX) {
        node_group_info_temp->capture[perframePosition].request = requestMCSC0;
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC0)] - FIMC_IS_VIDEO_BAS_NUM;
        perframePosition++;
    } else {
        CLOGE("Perframe capture node index out of bound, requestMCSC0 index(%d) CAPTURE_NODE_MAX(%d)", perframePosition, CAPTURE_NODE_MAX);
    }

#ifdef USE_MCSC1_FOR_PREVIEWCALLBACK
    if (perframePosition < CAPTURE_NODE_MAX) {
        node_group_info_temp->capture[perframePosition].request = requestMCSC1;
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC1)] - FIMC_IS_VIDEO_BAS_NUM;
        perframePosition++;
    } else {
        CLOGE("Perframe capture node index out of bound, requestMCSC1 index(%d) CAPTURE_NODE_MAX(%d)", perframePosition, CAPTURE_NODE_MAX);
    }
#endif

    if (m_cameraId == CAMERA_ID_BACK || m_parameters->getDualMode() == false) {
        if (perframePosition < CAPTURE_NODE_MAX) {
            node_group_info_temp->capture[perframePosition].request = requestMCSC2;
            node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC2)] - FIMC_IS_VIDEO_BAS_NUM;
        } else {
            CLOGE("Perframe capture node index out of bound, requestMCSC2 index(%d) CAPTURE_NODE_MAX(%d)", perframePosition, CAPTURE_NODE_MAX);
        }
    }

    m_parameters->getHwSensorSize(&sizeControlInfo.sensorSize.w,
                                  &sizeControlInfo.sensorSize.h);
#ifdef SUPPORT_DEPTH_MAP
    m_parameters->getDepthMapSize(&sizeControlInfo.depthMapSize.w,
                                  &sizeControlInfo.depthMapSize.h);
#endif // SUPPORT_DEPTH_MAP

    m_parameters->getPreviewBayerCropSize(&sizeControlInfo.bnsSize,
                                          &sizeControlInfo.bayerCropSize);
    m_parameters->getPreviewBdsSize(&sizeControlInfo.bdsSize);
    m_parameters->getPreviewYuvCropSize(&sizeControlInfo.yuvCropSize);
    m_parameters->getHwPreviewSize(&sizeControlInfo.hwPreviewSize.w,
                                   &sizeControlInfo.hwPreviewSize.h);
    m_parameters->getPreviewSize(&sizeControlInfo.previewSize.w,
                                 &sizeControlInfo.previewSize.h);
    m_parameters->getHwVideoSize(&sizeControlInfo.hwVideoSize.w,
                                 &sizeControlInfo.hwVideoSize.h);

    frame->setZoom(zoom);

    if (m_flagFlite3aaOTF == true) {
        updateNodeGroupInfo(
                PIPE_3AA,
                m_parameters,
                sizeControlInfo,
                &node_group_info_flite);
        frame->storeNodeGroupInfo(&node_group_info_flite, PERFRAME_INFO_FLITE);
    } else {
        updateNodeGroupInfo(
                PIPE_FLITE,
                m_parameters,
                sizeControlInfo,
                &node_group_info_flite);
        frame->storeNodeGroupInfo(&node_group_info_flite, PERFRAME_INFO_FLITE);

        updateNodeGroupInfo(
            PIPE_3AA,
            m_parameters,
            sizeControlInfo,
            &node_group_info_3aa);
        frame->storeNodeGroupInfo(&node_group_info_3aa, PERFRAME_INFO_3AA);
    }

    if (m_flag3aaIspOTF == false) {
        updateNodeGroupInfo(
                PIPE_ISP,
                m_parameters,
                sizeControlInfo,
                &node_group_info_isp);
        frame->storeNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_ISP);
    }

    if (m_parameters->getTpuEnabledMode() == true && m_flagIspTpuOTF == false) {
        updateNodeGroupInfo(
                PIPE_TPU,
                m_parameters,
                sizeControlInfo,
                &node_group_info_tpu);
        frame->storeNodeGroupInfo(&node_group_info_tpu, PERFRAME_INFO_TPU);
    }

    if (m_flagIspMcscOTF == false && m_flagTpuMcscOTF == false) {
        updateNodeGroupInfo(
                PIPE_MCSC,
                m_parameters,
                sizeControlInfo,
                &node_group_info_mcsc);
        frame->storeNodeGroupInfo(&node_group_info_mcsc, PERFRAME_INFO_MCSC);
    }

    return NO_ERROR;
}

void ExynosCameraFrameFactoryPreview::m_init(void)
{
    m_flagReprocessing = false;

#ifdef SUPPORT_GROUP_MIGRATION
    memset(m_nodeInfoTpu, 0x00, sizeof(camera_node_objects_t) * MAX_NODE);
    memset(m_nodeInfo, 0x00, sizeof(camera_node_objects_t) * MAX_NODE);
    m_nodeInfoTpu[0].isInitalize = false;
    m_nodeInfo[0].isInitalize = false;
    m_curNodeInfo = NULL;
#endif
}

#ifdef SUPPORT_GROUP_MIGRATION
status_t ExynosCameraFrameFactoryPreview::m_initNodeInfo(void)
{
    CLOGD("__IN__");
    int ispTpuOtf = m_parameters->isIspTpuOtf();
    //int flagTpu = m_parameters->getTpuEnabledMode();

    camera_node_objects_t *curNodeInfos = NULL;

    if (ispTpuOtf)
        curNodeInfos = m_nodeInfo;
    else
        curNodeInfos = m_nodeInfoTpu;

    m_curNodeInfo = curNodeInfos;

    m_pipes[INDEX(PIPE_3AA)]->getNodeInfos(&curNodeInfos[INDEX(PIPE_3AA)]);
    m_dumpNodeInfo("m_initNodeInfo PIPE_3AA", &curNodeInfos[INDEX(PIPE_3AA)]);
    m_pipes[INDEX(PIPE_ISP)]->getNodeInfos(&curNodeInfos[INDEX(PIPE_ISP)]);
    m_dumpNodeInfo("m_initNodeInfo PIPE_ISP", &curNodeInfos[INDEX(PIPE_ISP)]);
    m_pipes[INDEX(PIPE_TPU)]->getNodeInfos(&curNodeInfos[INDEX(PIPE_TPU)]);
    m_dumpNodeInfo("m_initNodeInfo PIPE_TPU", &curNodeInfos[INDEX(PIPE_TPU)]);
    curNodeInfos->isInitalize = true;

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryPreview::m_updateNodeInfo(void)
{
    CLOGD("__IN__");
    bool flagDirtyBayer = false;
    int ispTpuOtf = m_parameters->isIspTpuOtf();
    //int flagTpu = m_parameters->getTpuEnabledMode();
    bool flagReset = false;
    if (m_supportReprocessing == true && m_supportPureBayerReprocessing == false)
        flagDirtyBayer = true;

    camera_node_objects_t *curNodeInfos = NULL;
    camera_node_objects_t *srcNodeInfos = NULL;

    if (ispTpuOtf) {
        curNodeInfos = m_nodeInfo;
        srcNodeInfos = m_nodeInfoTpu;
    } else {
        curNodeInfos = m_nodeInfoTpu;
        srcNodeInfos = m_nodeInfo;
    }

    if (m_curNodeInfo != curNodeInfos) {
        if (ispTpuOtf)
            CLOGD("scenario change TPU ON-> OFF");
        else
            CLOGD("scenario change TPU OFF -> ON");

        flagReset = true;
        m_curNodeInfo = curNodeInfos;
    }

    if (curNodeInfos->isInitalize == true) {
        CLOGD("group migration is already done, use preset");
        m_pipes[INDEX(PIPE_3AA)]->setNodeInfos(&curNodeInfos[INDEX(PIPE_3AA)], flagReset);
        m_pipes[PIPE_3AA]->setDeviceInfo(&m_deviceInfo[PIPE_3AA]);
        m_pipes[PIPE_3AA]->setPipeName("PIPE_3AA");
        m_pipes[PIPE_3AA]->setPipeId(PIPE_3AA);
        m_dumpNodeInfo("m_updateVideoNode PIPE_3AA", &curNodeInfos[INDEX(PIPE_3AA)]);

        m_pipes[INDEX(PIPE_ISP)]->setNodeInfos(&curNodeInfos[INDEX(PIPE_ISP)], flagReset);
        m_pipes[PIPE_ISP]->setDeviceInfo(&m_deviceInfo[PIPE_ISP]);
        m_pipes[PIPE_ISP]->setPipeName("PIPE_ISP");
        m_pipes[PIPE_ISP]->setPipeId(PIPE_ISP);
        m_dumpNodeInfo("m_updateVideoNode PIPE_ISP", &curNodeInfos[INDEX(PIPE_ISP)]);

        m_pipes[INDEX(PIPE_TPU)]->setNodeInfos(&curNodeInfos[INDEX(PIPE_TPU)], flagReset);
        m_pipes[PIPE_TPU]->setDeviceInfo(&m_deviceInfo[PIPE_TPU]);
        m_pipes[PIPE_TPU]->setPipeName("PIPE_TPU");
        m_pipes[PIPE_TPU]->setPipeId(PIPE_TPU);
        m_dumpNodeInfo("m_updateVideoNode PIPE_TPU", &curNodeInfos[INDEX(PIPE_TPU)]);

        return NO_ERROR;
    }

    m_pipes[INDEX(PIPE_3AA)]->getNodeInfos(&curNodeInfos[INDEX(PIPE_3AA)]);

    if (ispTpuOtf == true) {
        // tpu on -> tpu off
        CLOGD("TPU group migration ON->OFF");

        // ISP
        curNodeInfos[INDEX(PIPE_ISP)].node[OUTPUT_NODE] = srcNodeInfos[INDEX(PIPE_ISP)].node[OUTPUT_NODE];
        curNodeInfos[INDEX(PIPE_ISP)].node[CAPTURE_NODE_6] = srcNodeInfos[INDEX(PIPE_ISP)].node[CAPTURE_NODE_6];
        curNodeInfos[INDEX(PIPE_ISP)].node[CAPTURE_NODE_11] = srcNodeInfos[INDEX(PIPE_TPU)].node[CAPTURE_NODE_11];
        curNodeInfos[INDEX(PIPE_ISP)].node[CAPTURE_NODE_13] = srcNodeInfos[INDEX(PIPE_TPU)].node[CAPTURE_NODE_13];
        curNodeInfos[INDEX(PIPE_ISP)].node[OTF_NODE_3] = srcNodeInfos[INDEX(PIPE_ISP)].node[CAPTURE_NODE_7];
        curNodeInfos[INDEX(PIPE_ISP)].node[OTF_NODE_4] = srcNodeInfos[INDEX(PIPE_TPU)].node[OUTPUT_NODE];
        curNodeInfos[INDEX(PIPE_ISP)].node[OTF_NODE_6] = srcNodeInfos[INDEX(PIPE_TPU)].node[OTF_NODE_6];
    }  else {
        // tpu on -> tpu off
        CLOGD("TPU group migration OFF->ON");

        // ISP
        curNodeInfos[INDEX(PIPE_ISP)].node[OUTPUT_NODE] = srcNodeInfos[INDEX(PIPE_ISP)].node[OUTPUT_NODE];
        curNodeInfos[INDEX(PIPE_ISP)].node[CAPTURE_NODE_6] = srcNodeInfos[INDEX(PIPE_ISP)].node[CAPTURE_NODE_6];
        curNodeInfos[INDEX(PIPE_ISP)].node[CAPTURE_NODE_7] = srcNodeInfos[INDEX(PIPE_ISP)].node[OTF_NODE_3];
        // TPU
        curNodeInfos[INDEX(PIPE_TPU)].node[OUTPUT_NODE] = srcNodeInfos[INDEX(PIPE_ISP)].node[OTF_NODE_4];
        curNodeInfos[INDEX(PIPE_TPU)].node[CAPTURE_NODE_11] = srcNodeInfos[INDEX(PIPE_ISP)].node[CAPTURE_NODE_11];
        curNodeInfos[INDEX(PIPE_TPU)].node[CAPTURE_NODE_13] = srcNodeInfos[INDEX(PIPE_ISP)].node[CAPTURE_NODE_13];
        curNodeInfos[INDEX(PIPE_TPU)].node[OTF_NODE_6] = srcNodeInfos[INDEX(PIPE_ISP)].node[OTF_NODE_6];
    }

    m_pipes[INDEX(PIPE_3AA)]->setNodeInfos(&curNodeInfos[INDEX(PIPE_3AA)], flagReset);
    m_pipes[PIPE_3AA]->setDeviceInfo(&m_deviceInfo[PIPE_3AA]);
    m_pipes[PIPE_3AA]->setPipeName("PIPE_3AA");
    m_pipes[PIPE_3AA]->setPipeId(PIPE_3AA);
    m_dumpNodeInfo("m_updateNodeInfo PIPE_3AA", &curNodeInfos[INDEX(PIPE_3AA)]);

    m_pipes[INDEX(PIPE_ISP)]->setNodeInfos(&curNodeInfos[INDEX(PIPE_ISP)], flagReset);
    m_pipes[PIPE_ISP]->setDeviceInfo(&m_deviceInfo[PIPE_ISP]);
    m_pipes[PIPE_ISP]->setPipeName("PIPE_ISP");
    m_pipes[PIPE_ISP]->setPipeId(PIPE_ISP);
    m_dumpNodeInfo("m_updateNodeInfo PIPE_ISP", &curNodeInfos[INDEX(PIPE_ISP)]);

    m_pipes[INDEX(PIPE_TPU)]->setNodeInfos(&curNodeInfos[INDEX(PIPE_TPU)], flagReset);
    m_pipes[PIPE_TPU]->setDeviceInfo(&m_deviceInfo[PIPE_TPU]);
    m_pipes[PIPE_TPU]->setPipeName("PIPE_TPU");
    m_pipes[PIPE_TPU]->setPipeId(PIPE_TPU);
    m_dumpNodeInfo("m_updateNodeInfo PIPE_TPU", &curNodeInfos[INDEX(PIPE_TPU)]);

    curNodeInfos->isInitalize = true;

    return NO_ERROR;
}

void ExynosCameraFrameFactoryPreview::m_dumpNodeInfo(const char* name, camera_node_objects_t* nodeInfo)
{
    for(int i = 0 ; i < MAX_OTF_NODE ; i++) {
        if (nodeInfo->node[i] != NULL) {
            CLOGD("%s mainNode[%d] = %d %p", name, i, nodeInfo->node[i]->getNodeNum(), nodeInfo->node[i]);
        }

        if (nodeInfo->secondaryNode[i] != NULL) {
            CLOGD("%s secondaryNode[%d] = %d %p", name, i, nodeInfo->secondaryNode[i]->getNodeNum(), nodeInfo->secondaryNode[i]);
        }
    }
}
#endif

}; /* namespace android */
