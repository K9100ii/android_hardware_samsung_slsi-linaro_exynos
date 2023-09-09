/*
**
** Copyright 2014, Samsung Electronics Co. LTD
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
    int ret = 0;

    ret = destroy();
    if (ret < 0)
        CLOGE2("destroy fail");
}

status_t ExynosCamera3FrameFactoryPreview::create(bool active)
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    m_setupConfig();

    int ret = 0;
    int leaderPipe = PIPE_3AA;
    int32_t nodeNums[MAX_NODE];
    for (int i = 0; i < MAX_NODE; i++)
        nodeNums[i] = -1;

    m_pipes[INDEX(PIPE_FLITE)] = (ExynosCameraPipe*)new ExynosCameraPipeFlite(m_cameraId, (ExynosCameraParameters *)m_parameters, false, m_nodeNums[INDEX(PIPE_FLITE)]);
    m_pipes[INDEX(PIPE_FLITE)]->setPipeId(PIPE_FLITE);
    m_pipes[INDEX(PIPE_FLITE)]->setPipeName("PIPE_FLITE");

    m_pipes[INDEX(PIPE_3AA)] = (ExynosCameraPipe*)new ExynosCameraMCPipe(m_cameraId, (ExynosCameraParameters *)m_parameters, false, &m_nodeInfo[INDEX(PIPE_3AA)]);
    m_pipes[INDEX(PIPE_3AA)]->setPipeId(PIPE_3AA);
    m_pipes[INDEX(PIPE_3AA)]->setPipeName("PIPE_3AA");

    m_pipes[INDEX(PIPE_ISP)] = (ExynosCameraPipe*)new ExynosCameraMCPipe(m_cameraId, (ExynosCameraParameters *)m_parameters, false, &m_nodeInfo[INDEX(PIPE_ISP)]);
    m_pipes[INDEX(PIPE_ISP)]->setPipeId(PIPE_ISP);
    m_pipes[INDEX(PIPE_ISP)]->setPipeName("PIPE_ISP");

    if (m_parameters->getHWVdisMode()) {
        m_pipes[INDEX(PIPE_DIS)] = (ExynosCameraPipe*)new ExynosCameraMCPipe(m_cameraId, (ExynosCameraParameters *)m_parameters, false, &m_nodeInfo[INDEX(PIPE_DIS)]);
        m_pipes[INDEX(PIPE_DIS)]->setPipeId(PIPE_DIS);
        m_pipes[INDEX(PIPE_DIS)]->setPipeName("PIPE_DIS");
    }

/* Comment out, because it included ISP */
/*
    m_pipes[INDEX(PIPE_SCP)] = (ExynosCameraPipe*)new ExynosCameraPipeSCP(m_cameraId, m_parameters, false, m_nodeNums[INDEX(PIPE_SCP)]);
    m_pipes[INDEX(PIPE_SCP)]->setPipeId(PIPE_SCP);
    m_pipes[INDEX(PIPE_SCP)]->setPipeName("PIPE_SCP");
*/

    if (m_flagMcscVraOTF == false) {
        m_pipes[INDEX(PIPE_VRA)] = (ExynosCameraPipe*)new ExynosCameraPipeVRA(m_cameraId, m_parameters, false, m_nodeNums[INDEX(PIPE_VRA)]);
        m_pipes[INDEX(PIPE_VRA)]->setPipeId(PIPE_VRA);
        m_pipes[INDEX(PIPE_VRA)]->setPipeName("PIPE_VRA");
    }

    m_pipes[INDEX(PIPE_GSC)] = (ExynosCameraPipe*)new ExynosCameraPipeGSC(m_cameraId, (ExynosCameraParameters *)m_parameters, true, m_nodeNums[INDEX(PIPE_GSC)]);
    m_pipes[INDEX(PIPE_GSC)]->setPipeId(PIPE_GSC);
    m_pipes[INDEX(PIPE_GSC)]->setPipeName("PIPE_GSC");

    m_pipes[INDEX(PIPE_GSC_VIDEO)] = (ExynosCameraPipe*)new ExynosCameraPipeGSC(m_cameraId, (ExynosCameraParameters *)m_parameters, true, m_nodeNums[INDEX(PIPE_GSC_VIDEO)]);
    m_pipes[INDEX(PIPE_GSC_VIDEO)]->setPipeId(PIPE_GSC_VIDEO);
    m_pipes[INDEX(PIPE_GSC_VIDEO)]->setPipeName("PIPE_GSC_VIDEO");

    if (m_supportReprocessing == false) {
        m_pipes[INDEX(PIPE_GSC_PICTURE)] = (ExynosCameraPipe*)new ExynosCameraPipeGSC(m_cameraId, (ExynosCameraParameters *)m_parameters, true, m_nodeNums[INDEX(PIPE_GSC_PICTURE)]);
        m_pipes[INDEX(PIPE_GSC_PICTURE)]->setPipeId(PIPE_GSC_PICTURE);
        m_pipes[INDEX(PIPE_GSC_PICTURE)]->setPipeName("PIPE_GSC_PICTURE");

        m_pipes[INDEX(PIPE_JPEG)] = (ExynosCameraPipe*)new ExynosCameraPipeJpeg(m_cameraId, (ExynosCameraParameters *)m_parameters, true, m_nodeNums[INDEX(PIPE_JPEG)]);
        m_pipes[INDEX(PIPE_JPEG)]->setPipeId(PIPE_JPEG);
        m_pipes[INDEX(PIPE_JPEG)]->setPipeName("PIPE_JPEG");
    }

    /* flite pipe initialize */
    ret = m_pipes[INDEX(PIPE_FLITE)]->create(m_sensorIds[INDEX(PIPE_FLITE)]);
    if (ret < 0) {
        CLOGE2("FLITE create fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGD2("Pipe(%d) created", INDEX(PIPE_FLITE));

    /* FAST AE init before ISP setInput */
    if ( active == true &&
        m_parameters->getUseFastenAeStable() == true &&
        m_parameters->getCameraId() == CAMERA_ID_BACK &&
        m_parameters->getDualMode() == false &&
        m_parameters->getRecordingHint() == false &&
        m_parameters->getIsFirstStartFlag() == true) {

        int sensorMarginW, sensorMarginH;
        int32_t sensorIds[MAX_NODE];
        for (int i = 0; i < MAX_NODE; i++)
            sensorIds[i] = -1;

        camera_pipe_info_t pipeInfo[MAX_NODE];
        camera_pipe_info_t nullPipeInfo;
        ExynosRect tempRect;
        int hwSensorW = 0, hwSensorH = 0;
        int bayerFormat = CAMERA_BAYER_FORMAT;

        m_parameters->getFastenAeStableSensorSize(&hwSensorW, &hwSensorH);

        CLOGI2("hwSensorSize(%dx%d)", hwSensorW, hwSensorH);

        for (int i = 0; i < MAX_NODE; i++)
            pipeInfo[i] = nullPipeInfo;


        struct v4l2_streamparm streamParam;
        uint32_t frameRate = 0;

        frameRate  = FASTEN_AE_FPS;

        /* setParam for Frame rate : must after setInput on Flite */
        memset(&streamParam, 0x0, sizeof(v4l2_streamparm));

        streamParam.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        streamParam.parm.capture.timeperframe.numerator   = 1;
        streamParam.parm.capture.timeperframe.denominator = frameRate;
        CLOGI2("set framerate (denominator=%d)", frameRate);
        ret = setParam(&streamParam, PIPE_FLITE);
        if (ret < 0) {
            CLOGE2("FLITE setParam fail, ret(%d)", ret);
        }

        /* FLITE pipe */
        tempRect.fullW = hwSensorW;
        tempRect.fullH = hwSensorH;
        tempRect.colorFormat = bayerFormat;

        pipeInfo[0].rectInfo = tempRect;
        pipeInfo[0].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        pipeInfo[0].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
        pipeInfo[0].bufInfo.count = 10;
        /* per frame info */
        pipeInfo[0].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
        pipeInfo[0].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;

#ifdef CAMERA_PACKED_BAYER_ENABLE
#ifdef DEBUG_RAWDUMP
        if (m_parameters->checkBayerDumpEnable()) {
            /* packed bayer bytesPerPlane */
            pipeInfo[0].bytesPerPlane[0] = ROUND_UP(pipeInfo[0].rectInfo.fullW, 10) * 2;
        }
        else
#endif
        {
            /* packed bayer bytesPerPlane */
            pipeInfo[0].bytesPerPlane[0] = ROUND_UP(pipeInfo[0].rectInfo.fullW, 10) * 8 / 5;
        }
#endif

        ret = m_pipes[INDEX(PIPE_FLITE)]->setupPipe(pipeInfo, m_sensorIds[INDEX(PIPE_FLITE)]);
        if (ret < 0) {
            CLOGE2("FLITE setupPipe fail, ret(%d)", ret);
        }
    }

/* Comment out, because it same with M2M */
#if 0
    if (m_flagFlite3aaOTF == true) {
        /* 3AA_ISP pipe initialize */
        ret = m_pipes[INDEX(PIPE_3AA_ISP)]->create();
        if (ret < 0) {
            CLOGE2("3AA_ISP create fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGD2("Pipe(%d) created", INDEX(PIPE_3AA_ISP));
    } else
#endif
    {
        /* ISP pipe initialize */
        ret = m_pipes[INDEX(PIPE_ISP)]->create();
        if (ret < 0) {
            CLOGE2("ISP create fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGD2("Pipe(%d) created", INDEX(PIPE_ISP));

        /* 3AA pipe initialize */
        ret = m_pipes[INDEX(PIPE_3AA)]->create();
        if (ret < 0) {
            CLOGE2("3AA create fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGD2("Pipe(%d) created", INDEX(PIPE_3AA));
    }

    /* DIS pipe initialize */
    if (m_parameters->getHWVdisMode()) {
        ret = m_pipes[INDEX(PIPE_DIS)]->create();
        if (ret < 0) {
            CLOGE2("DIS create fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGD2("Pipe(%d) created", INDEX(PIPE_DIS));
    }

    /* SCP pipe initialize */
/* Comment out SCP, because it included ISP */
#if 0
    ret = m_pipes[INDEX(PIPE_SCP)]->create();
    if (ret < 0) {
        CLOGE2("SCP create fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGD2("Pipe(%d) created", INDEX(PIPE_SCP));
#endif

    /* VRA pipe initialize */
    if (m_flagMcscVraOTF == false) {
        /* EOS */
        ret = m_pipes[INDEX(leaderPipe)]->setControl(V4L2_CID_IS_END_OF_STREAM, 1);
        if (ret < 0) {
            CLOGE2("PIPE_%d V4L2_CID_IS_END_OF_STREAM fail, ret(%d)", leaderPipe, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }

        /* Change leaderPipe to VRA, Create new instance */
        leaderPipe = PIPE_VRA;

        ret = m_pipes[INDEX(PIPE_VRA)]->create();
        if (ret < 0) {
            CLOGE2("VRA create fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGD2("Pipe(%d) created", INDEX(PIPE_VRA));
    }

    /* GSC_PREVIEW pipe initialize */
    ret = m_pipes[INDEX(PIPE_GSC)]->create();
    if (ret < 0) {
        CLOGE2("GSC create fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGD2("Pipe(%d) created", INDEX(PIPE_GSC));

    ret = m_pipes[INDEX(PIPE_GSC_VIDEO)]->create();
    if (ret < 0) {
        CLOGE2("PIPE_GSC_VIDEO create fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGD2("Pipe(%d) created", INDEX(PIPE_GSC_VIDEO));

    if (m_supportReprocessing == false) {
        /* GSC_PICTURE pipe initialize */
        ret = m_pipes[INDEX(PIPE_GSC_PICTURE)]->create();
        if (ret < 0) {
            CLOGE2("GSC_PICTURE create fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGD2("Pipe(%d) created", INDEX(PIPE_GSC_PICTURE));

        /* JPEG pipe initialize */
        ret = m_pipes[INDEX(PIPE_JPEG)]->create();
        if (ret < 0) {
            CLOGE2("JPEG create fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGD2("Pipe(%d) created", INDEX(PIPE_JPEG));
    }

    /* EOS */
    ret = m_pipes[INDEX(leaderPipe)]->setControl(V4L2_CID_IS_END_OF_STREAM, 1);
    if (ret < 0) {
        CLOGE2("PIPE_%d V4L2_CID_IS_END_OF_STREAM fail, ret(%d)", leaderPipe, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    /* s_ctrl HAL version for selecting dvfs table */
    ret = m_pipes[PIPE_3AA]->setControl(V4L2_CID_IS_HAL_VERSION, IS_HAL_VER_3_2);

    if (ret < 0)
        CLOGW("WARN(%s): V4L2_CID_IS_HAL_VERSION is fail", __FUNCTION__);

    m_setCreate(true);

    return NO_ERROR;
}

#ifdef SAMSUNG_COMPANION
status_t ExynosCamera3FrameFactoryPreview::precreate(void)
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    m_setupConfig();

    int ret = 0;
    int32_t nodeNums[MAX_NODE];
    for (int i = 0; i < MAX_NODE; i++)
        nodeNums[i] = -1;

    m_pipes[INDEX(PIPE_FLITE)] = (ExynosCameraPipe*)new ExynosCameraPipeFlite(m_cameraId, m_parameters, false, m_nodeNums[INDEX(PIPE_FLITE)]);
    m_pipes[INDEX(PIPE_FLITE)]->setPipeId(PIPE_FLITE);
    m_pipes[INDEX(PIPE_FLITE)]->setPipeName("PIPE_FLITE");

    if (m_flagFlite3aaOTF == true) {
        m_pipes[INDEX(PIPE_3AA)] = (ExynosCameraPipe*)new ExynosCameraMCPipe(m_cameraId, m_parameters, false, &m_nodeInfo[INDEX(PIPE_3AA)]);
        m_pipes[INDEX(PIPE_3AA)]->setPipeId(PIPE_3AA);
        m_pipes[INDEX(PIPE_3AA)]->setPipeName("PIPE_3AA");

        m_pipes[INDEX(PIPE_ISP)] = (ExynosCameraPipe*)new ExynosCameraMCPipe(m_cameraId, m_parameters, false, &m_nodeInfo[INDEX(PIPE_ISP)]);
        m_pipes[INDEX(PIPE_ISP)]->setPipeId(PIPE_ISP);
        m_pipes[INDEX(PIPE_ISP)]->setPipeName("PIPE_ISP");
    } else {
        m_pipes[INDEX(PIPE_3AA)] = (ExynosCameraPipe*)new ExynosCameraPipe3AA(m_cameraId, m_parameters, false, m_nodeNums[INDEX(PIPE_3AA)]);
        m_pipes[INDEX(PIPE_3AA)]->setPipeId(PIPE_3AA);
        m_pipes[INDEX(PIPE_3AA)]->setPipeName("PIPE_3AA");

        m_pipes[INDEX(PIPE_ISP)] = (ExynosCameraPipe*)new ExynosCameraPipeISP(m_cameraId, m_parameters, false, m_nodeNums[INDEX(PIPE_ISP)]);
        m_pipes[INDEX(PIPE_ISP)]->setPipeId(PIPE_ISP);
        m_pipes[INDEX(PIPE_ISP)]->setPipeName("PIPE_ISP");
    }

    if (m_parameters->getHWVdisMode()) {
        m_pipes[INDEX(PIPE_DIS)] = (ExynosCameraPipe*)new ExynosCameraMCPipe(m_cameraId, m_parameters, false, &m_nodeInfo[INDEX(PIPE_DIS)]);
        m_pipes[INDEX(PIPE_DIS)]->setPipeId(PIPE_DIS);
        m_pipes[INDEX(PIPE_DIS)]->setPipeName("PIPE_DIS");
    }

/* Comment out, because it included 3AA, ISP */
/*
    m_pipes[INDEX(PIPE_SCP)] = (ExynosCameraPipe*)new ExynosCameraPipeSCP(m_cameraId, m_parameters, false, m_nodeNums[INDEX(PIPE_SCP)]);
    m_pipes[INDEX(PIPE_SCP)]->setPipeId(PIPE_SCP);
    m_pipes[INDEX(PIPE_SCP)]->setPipeName("PIPE_SCP");
*/

    if (m_flagMcscVraOTF == false) {
        m_pipes[INDEX(PIPE_VRA)] = new ExynosCameraPipe(m_cameraId, m_parameters, false, m_nodeNums[INDEX(PIPE_VRA)]);
        m_pipes[INDEX(PIPE_VRA)]->setPipeId(PIPE_VRA);
        m_pipes[INDEX(PIPE_VRA)]->setPipeName("PIPE_VRA");
    }

    m_pipes[INDEX(PIPE_GSC)] = (ExynosCameraPipe*)new ExynosCameraPipeGSC(m_cameraId, m_parameters, true, m_nodeNums[INDEX(PIPE_GSC)]);
    m_pipes[INDEX(PIPE_GSC)]->setPipeId(PIPE_GSC);
    m_pipes[INDEX(PIPE_GSC)]->setPipeName("PIPE_GSC");

    m_pipes[INDEX(PIPE_GSC_VIDEO)] = (ExynosCameraPipe*)new ExynosCameraPipeGSC(m_cameraId, m_parameters, false, m_nodeNums[INDEX(PIPE_GSC_VIDEO)]);
    m_pipes[INDEX(PIPE_GSC_VIDEO)]->setPipeId(PIPE_GSC_VIDEO);
    m_pipes[INDEX(PIPE_GSC_VIDEO)]->setPipeName("PIPE_GSC_VIDEO");

    if (m_supportReprocessing == false) {
        m_pipes[INDEX(PIPE_GSC_PICTURE)] = (ExynosCameraPipe*)new ExynosCameraPipeGSC(m_cameraId, m_parameters, true, m_nodeNums[INDEX(PIPE_GSC_PICTURE)]);
        m_pipes[INDEX(PIPE_GSC_PICTURE)]->setPipeId(PIPE_GSC_PICTURE);
        m_pipes[INDEX(PIPE_GSC_PICTURE)]->setPipeName("PIPE_GSC_PICTURE");

        m_pipes[INDEX(PIPE_JPEG)] = (ExynosCameraPipe*)new ExynosCameraPipeJpeg(m_cameraId, m_parameters, true, m_nodeNums[INDEX(PIPE_JPEG)]);
        m_pipes[INDEX(PIPE_JPEG)]->setPipeId(PIPE_JPEG);
        m_pipes[INDEX(PIPE_JPEG)]->setPipeName("PIPE_JPEG");
    }

    /* flite pipe initialize */
    ret = m_pipes[INDEX(PIPE_FLITE)]->create(m_sensorIds[INDEX(PIPE_FLITE)]);
    if (ret < 0) {
        CLOGE2("FLITE create fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGD2("Pipe(%d) created", INDEX(PIPE_FLITE));

    /* FAST AE init before ISP setInput */
    if (m_parameters->getUseFastenAeStable() == true &&
        m_parameters->getCameraId() == CAMERA_ID_BACK &&
        m_parameters->getDualMode() == false &&
        m_parameters->getRecordingHint() == false &&
        m_parameters->getIsFirstStartFlag() == true) {

        int sensorMarginW, sensorMarginH;
        int32_t sensorIds[MAX_NODE];
        for (int i = 0; i < MAX_NODE; i++)
            sensorIds[i] = -1;

        camera_pipe_info_t pipeInfo[MAX_NODE];
        camera_pipe_info_t nullPipeInfo;
        ExynosRect tempRect;
        int hwSensorW = 0, hwSensorH = 0;
        int bayerFormat = CAMERA_BAYER_FORMAT;

        m_parameters->getFastenAeStableSensorSize(&hwSensorW, &hwSensorH);

        CLOGI2("hwSensorSize(%dx%d)", hwSensorW, hwSensorH);

        for (int i = 0; i < MAX_NODE; i++)
            pipeInfo[i] = nullPipeInfo;


        /* setParam for Frame rate : must after setInput on Flite */
        struct v4l2_streamparm streamParam;
        uint32_t frameRate = 0;

        frameRate  = FASTEN_AE_FPS;

        memset(&streamParam, 0x0, sizeof(v4l2_streamparm));

        streamParam.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        streamParam.parm.capture.timeperframe.numerator   = 1;
        streamParam.parm.capture.timeperframe.denominator = frameRate;
        CLOGI2("set framerate (denominator=%d)", frameRate);
        ret = setParam(&streamParam, PIPE_FLITE);
        if (ret < 0) {
            CLOGE2("FLITE setParam fail, ret(%d)", ret);
        }

        /* FLITE pipe */
        tempRect.fullW = hwSensorW;
        tempRect.fullH = hwSensorH;
        tempRect.colorFormat = bayerFormat;

        pipeInfo[0].rectInfo = tempRect;
        pipeInfo[0].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        pipeInfo[0].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
        pipeInfo[0].bufInfo.count = 10;
        /* per frame info */
        pipeInfo[0].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
        pipeInfo[0].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;

#ifdef CAMERA_PACKED_BAYER_ENABLE
#ifdef DEBUG_RAWDUMP
        if (m_parameters->checkBayerDumpEnable()) {
            /* packed bayer bytesPerPlane */
            pipeInfo[0].bytesPerPlane[0] = ROUND_UP(pipeInfo[0].rectInfo.fullW, 10) * 2;
        }
        else
#endif
        {
            /* packed bayer bytesPerPlane */
            pipeInfo[0].bytesPerPlane[0] = ROUND_UP(pipeInfo[0].rectInfo.fullW, 10) * 8 / 5;
        }
#endif

        ret = m_pipes[INDEX(PIPE_FLITE)]->setupPipe(pipeInfo, m_sensorIds[INDEX(PIPE_FLITE)]);
        if (ret < 0) {
            CLOGE2("FLITE setupPipe fail, ret(%d)", ret);
        }
    }

/* Comment out, because it same with M2M */
#if 0
    if (m_flagFlite3aaOTF == true) {
        /* 3AA_ISP pipe initialize */
        ret = m_pipes[INDEX(PIPE_3AA_ISP)]->precreate();
        if (ret < 0) {
            CLOGE2("3AA_ISP precreate fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGD2("Pipe(%d) precreated", INDEX(PIPE_3AA_ISP));
    } else {
#endif
    {
        /* ISP pipe initialize */
        ret = m_pipes[INDEX(PIPE_ISP)]->precreate();
        if (ret < 0) {
            CLOGE2("ISP create fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGD2("Pipe(%d) created", INDEX(PIPE_ISP));

        /* 3AA pipe initialize */
        ret = m_pipes[INDEX(PIPE_3AA)]->precreate();
        if (ret < 0) {
            CLOGE2("3AA create fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGD2("Pipe(%d) created", INDEX(PIPE_3AA));
    }

    return NO_ERROR;
}

status_t ExynosCamera3FrameFactoryPreview::postcreate(void)
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);
    int ret = 0;

    if (m_flagFlite3aaOTF == true) {
        /* 3AA_ISP pipe initialize */
        ret = m_pipes[INDEX(PIPE_3AA)]->postcreate();
        if (ret < 0) {
            CLOGE2("3AA_ISP postcreate fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGD2("Pipe(%d) postcreated", INDEX(PIPE_3AA_ISP));
    }

    /* ISP pipe initialize */
    ret = m_pipes[INDEX(PIPE_ISP)]->postcreate();
    if (ret < 0) {
        CLOGE2("ISP create fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGD2("Pipe(%d) created", INDEX(PIPE_ISP));

    if (m_parameters->getHWVdisMode()) {
        /* DIS pipe initialize */
        ret = m_pipes[INDEX(PIPE_DIS)]->create();
        if (ret < 0) {
            CLOGE2("DIS create fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGD2("Pipe(%d) created", INDEX(PIPE_DIS));
    }

    /* VRA pipe initialize */
    if (m_flagMcscVraOTF == false) {
        /* EOS */
        ret = m_pipes[INDEX(leaderPipe)]->setControl(V4L2_CID_IS_END_OF_STREAM, 1);
        if (ret < 0) {
            CLOGE2("PIPE_%d V4L2_CID_IS_END_OF_STREAM fail, ret(%d)", leaderPipe, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }

        /* Change leaderPipe to VRA, Create new instance */
        leaderPipe = PIPE_VRA;

        ret = m_pipes[INDEX(PIPE_VRA)]->create();
        if (ret < 0) {
            CLOGE2("VRA create fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGD2("Pipe(%d) created", INDEX(PIPE_VRA));
    }

    /* GSC pipe initialize */
    ret = m_pipes[INDEX(PIPE_GSC)]->create();
    if (ret < 0) {
        CLOGE2("GSC create fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGD2("Pipe(%d) created", INDEX(PIPE_GSC));

    ret = m_pipes[INDEX(PIPE_GSC_VIDEO)]->create();
    if (ret < 0) {
        CLOGE2("GSC create fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGD2("Pipe(%d) created", INDEX(PIPE_GSC_VIDEO));

    if (m_supportReprocessing == false) {
        /* GSC_PICTURE pipe initialize */
        ret = m_pipes[INDEX(PIPE_GSC_PICTURE)]->create();
        if (ret < 0) {
            CLOGE2("GSC_PICTURE create fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGD2("Pipe(%d) created", INDEX(PIPE_GSC_PICTURE));

        /* JPEG pipe initialize */
        ret = m_pipes[INDEX(PIPE_JPEG)]->create();
        if (ret < 0) {
            CLOGE2("JPEG create fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGD2("Pipe(%d) created", INDEX(PIPE_JPEG));
    }

#ifdef SAMSUNG_TN_FEATURE
    int isSamsungCamera = 0;
    if (m_parameters->getSamsungCamera() && m_cameraId == CAMERA_ID_BACK)
        isSamsungCamera = 1;
    if (m_parameters->getDualMode() == false
#ifdef SAMSUNG_QUICKSHOT
        && m_parameters->getQuickShot() == 0
#endif
    ) {
        ret = m_pipes[INDEX(PIPE_3AA)]->setControl(V4L2_CID_IS_CAMERA_TYPE, isSamsungCamera);
        if (ret < 0) {
            CLOGE2("PIPE_%d V4L2_CID_IS_CAMERA_TYPE fail, ret(%d)", PIPE_3AA, ret);
            return INVALID_OPERATION;
        }
    }
#endif
    /* EOS */
    ret = m_pipes[INDEX(leaderPipe)]->setControl(V4L2_CID_IS_END_OF_STREAM, 1);
    if (ret < 0) {
        CLOGE2("PIPE_%d V4L2_CID_IS_END_OF_STREAM fail, ret(%d)", leaderPipe, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    /* s_ctrl HAL version for selecting dvfs table */
    ret = m_pipes[PIPE_3AA]->setControl(V4L2_CID_IS_HAL_VERSION, IS_HAL_VER_3_2);

    if (ret < 0)
        CLOGW("WARN(%s): V4L2_CID_IS_HAL_VERSION is fail", __FUNCTION__);

    m_setCreate(true);

    return NO_ERROR;
}
#endif

status_t ExynosCamera3FrameFactoryPreview::fastenAeStable(int32_t numFrames, ExynosCameraBuffer *buffers)
{
    CLOGI("INFO(%s[%d]): Start", __FUNCTION__, __LINE__);

    int ret = 0;
    status_t totalRet = NO_ERROR;

    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraFrameEntity *newEntity = NULL;
    ExynosCameraList<ExynosCameraFrame *> instantQ;

    /* TODO 1. setup pipes for 120FPS */
    camera_pipe_info_t pipeInfo[MAX_NODE];
    camera_pipe_info_t nullPipeInfo;

    int32_t nodeNums[MAX_NODE];
    int32_t sensorIds[MAX_NODE];
    int32_t secondarySensorIds[MAX_NODE];
    for (int i = 0; i < MAX_NODE; i++) {
        nodeNums[i] = -1;
        sensorIds[i] = -1;
        secondarySensorIds[i] = -1;
    }

    ExynosRect tempRect;
    int hwSensorW = 0, hwSensorH = 0;
    int bcropX = 0, bcropY = 0, bcropW = 0, bcropH = 0;
    int hwPreviewW = 0, hwPreviewH = 0;
    int bayerFormat = CAMERA_BAYER_FORMAT;
    int previewFormat = m_parameters->getHwPreviewFormat();
    int hwVdisformat = m_parameters->getHWVdisFormat();
    struct ExynosConfigInfo *config = m_parameters->getConfig();
    ExynosRect bdsSize;
    uint32_t frameRate = 0;
    struct v4l2_streamparm streamParam;
    int perFramePos = 0;

#ifdef DEBUG_RAWDUMP
    if (m_parameters->checkBayerDumpEnable()) {
        bayerFormat = CAMERA_DUMP_BAYER_FORMAT;
    }
#endif

    if (numFrames < 1) {
        CLOGW2("numFrames is %d, we skip fastenAeStable", numFrames);
        return NO_ERROR;
    }

    if (m_parameters->getCameraId() == CAMERA_ID_FRONT) {
        frameRate  = FASTEN_AE_FPS_FRONT;
    } else {
        frameRate  = FASTEN_AE_FPS;
    }
    m_parameters->getFastenAeStableSensorSize(&hwSensorW, &hwSensorH);
    m_parameters->getFastenAeStableBcropSize(&bcropW, &bcropH);
    m_parameters->getFastenAeStableBdsSize(&hwPreviewW, &hwPreviewH);

    bcropX = ALIGN_UP(((hwSensorW - bcropW) >> 1), 2);
    bcropY = ALIGN_UP(((hwSensorH - bcropH) >> 1), 2);

    m_parameters->getPreviewBdsSize(&bdsSize);

    CLOGI2("hwSensorSize(%dx%d)", hwSensorW, hwSensorH);

    /* FLITE pipe */
    for (int i = 0; i < MAX_NODE; i++)
        pipeInfo[i] = nullPipeInfo;

    /* setParam for Frame rate : must after setInput on Flite */
    memset(&streamParam, 0x0, sizeof(v4l2_streamparm));

    streamParam.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    streamParam.parm.capture.timeperframe.numerator   = 1;
    streamParam.parm.capture.timeperframe.denominator = frameRate;
    CLOGI2("set framerate (denominator=%d)", frameRate);
    ret = setParam(&streamParam, PIPE_FLITE);
    if (ret < 0) {
        CLOGE2("FLITE setParam fail, ret(%d)", ret);
        return INVALID_OPERATION;
    }

    tempRect.fullW = hwSensorW;
    tempRect.fullH = hwSensorH;
    tempRect.colorFormat = bayerFormat;

    pipeInfo[0].rectInfo = tempRect;
    pipeInfo[0].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pipeInfo[0].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
    pipeInfo[0].bufInfo.count = numFrames;
    /* per frame info */
    pipeInfo[0].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
    pipeInfo[0].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;

#ifdef CAMERA_PACKED_BAYER_ENABLE
#ifdef DEBUG_RAWDUMP
    if (m_parameters->checkBayerDumpEnable()) {
        /* packed bayer bytesPerPlane */
        pipeInfo[0].bytesPerPlane[0] = ROUND_UP(pipeInfo[0].rectInfo.fullW, 10) * 2;
    }
    else
#endif
    {
        /* packed bayer bytesPerPlane */
        pipeInfo[0].bytesPerPlane[0] = ROUND_UP(pipeInfo[0].rectInfo.fullW, 10) * 8 / 5;
    }
#endif

    ret = m_pipes[INDEX(PIPE_FLITE)]->setupPipe(pipeInfo, m_sensorIds[INDEX(PIPE_FLITE)]);
    if (ret < 0) {
        CLOGE2("FLITE setupPipe fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    /* setParam for Frame rate : must after setInput on Flite */
    int bnsScaleRatio = 1000;
    ret = m_pipes[INDEX(PIPE_FLITE)]->setControl(V4L2_CID_IS_S_BNS, bnsScaleRatio);
    if (ret < 0) {
        CLOGE2("set BNS(%d) fail, ret(%d)", bnsScaleRatio, ret);
    }

    ret = m_setDeviceInfo();
    if (ret != NO_ERROR) {
        CLOGE2("m_setDeviceInfo() fail");
        return ret;
    }

    ret = m_initPipesFastenAeStable(numFrames,
                                       hwSensorW, hwSensorH,
                                       hwPreviewW, hwPreviewH);
    if (ret != NO_ERROR) {
        CLOGE2("m_initPipesFastenAeStable(%d) fail");
        return ret;
    }

    for (int i = 0; i < numFrames; i++) {
        /* 2. generate instant frames */
        newFrame = m_frameMgr->createFrame(m_parameters, i);
        if (newFrame == NULL)
            return INVALID_OPERATION;

        ret = m_initFrameMetadata(newFrame);
        if (ret < 0)
            CLOGE2("frame(%d) metadata initialize fail", i);

        newEntity = new ExynosCameraFrameEntity(PIPE_3AA, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
        newFrame->addSiblingEntity(NULL, newEntity);
        newFrame->setNumRequestPipe(1);

        newEntity->setSrcBuf(buffers[i]);

        /* set metadata for instant on */
        camera2_shot_ext *shot_ext = (struct camera2_shot_ext *)(buffers[i].addr[1]);

        if (shot_ext != NULL) {
            int aeRegionX = (bcropW) / 2;
            int aeRegionY = (bcropH) / 2;

            newFrame->getMetaData(shot_ext);
            m_parameters->duplicateCtrlMetadata((void *)shot_ext);
            m_activityControl->activityBeforeExecFunc(newEntity->getPipeId(), (void *)&buffers[i]);

#ifdef SR_CAPTURE
            /* setfile setting */
            int setfile = 0;
            int yuvRange = 0;
            m_parameters->getSetfileYuvRange(0, &setfile, &yuvRange);
            CLOGV2("setfile(%d)", setfile);
            setMetaSetfile(shot_ext, setfile);
#endif

            /* set metadata for instant on */
            shot_ext->shot.ctl.scaler.cropRegion[0] = 0;
            shot_ext->shot.ctl.scaler.cropRegion[1] = 0;
            shot_ext->shot.ctl.scaler.cropRegion[2] = hwPreviewW;
            shot_ext->shot.ctl.scaler.cropRegion[3] = hwPreviewH;

            setMetaCtlAeTargetFpsRange(shot_ext, frameRate, frameRate);
            setMetaCtlSensorFrameDuration(shot_ext, (uint64_t)((1000 * 1000 * 1000) / (uint64_t)frameRate));

            /* set afMode into INFINITY */
            shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_CANCEL;
            shot_ext->shot.ctl.aa.vendor_afmode_option &= (0 << AA_AFMODE_OPTION_BIT_MACRO);
            setMetaCtlAeRegion(shot_ext, aeRegionX, aeRegionY, aeRegionX, aeRegionY, 0);

            /* Set 3AS size */
            enum NODE_TYPE nodeType = getNodeType(PIPE_3AA);
            int nodeNum = m_nodeInfo[PIPE_3AA].nodeNum[nodeType];
            if (nodeNum <= 0) {
                CLOGE2("invalid nodeNum(%d). so fail", nodeNum);
                ret = INVALID_OPERATION;
                goto cleanup;
            }

            setMetaNodeLeaderVideoID(shot_ext, nodeNum - FIMC_IS_VIDEO_BAS_NUM);
            setMetaNodeLeaderRequest(shot_ext, false);
            setMetaNodeLeaderInputSize(shot_ext, bcropX, bcropY, bcropW, bcropH);

            /* Set 3AP size */
            nodeType = getNodeType(PIPE_3AP);
            nodeNum = m_nodeInfo[PIPE_3AA].nodeNum[nodeType];

            if (m_flag3aaIspOTF == true)
                nodeNum = m_nodeInfo[PIPE_3AA].secondaryNodeNum[nodeType];

            if (nodeNum <= 0) {
                CLOGE2("invalid t3apNodeNum(%d). so fail", nodeNum);
                ret = INVALID_OPERATION;
                goto cleanup;
            }

            int perframePosition = 0; /* 3AP:0, SCP/ISPC/ISPP:1 */
            setMetaNodeCaptureVideoID(shot_ext, perframePosition, nodeNum - FIMC_IS_VIDEO_BAS_NUM);
            setMetaNodeCaptureRequest(shot_ext, perframePosition, false);
            setMetaNodeCaptureOutputSize(shot_ext, perframePosition, 0, 0, bcropW, bcropH);

            if (m_flag3aaIspOTF == true) {
                nodeType = getNodeType(PIPE_SCP);
                nodeNum = m_nodeInfo[PIPE_3AA].nodeNum[nodeType];
                if (nodeNum <= 0) {
                    CLOGE2("invalid nodeNum(%d). so fail", nodeNum);
                    ret = INVALID_OPERATION;
                    goto cleanup;
                }

                perframePosition = 1; /* 3AP:0, SCP/ISPC/ISPP:1 */
                setMetaNodeCaptureVideoID(shot_ext, perframePosition, nodeNum - FIMC_IS_VIDEO_BAS_NUM);
                setMetaNodeCaptureRequest(shot_ext, perframePosition, false);
                setMetaNodeCaptureOutputSize(shot_ext, perframePosition, 0, 0, hwPreviewW, hwPreviewH);
            }
        }

        /* 3. push instance frames to pipe */
        ret = pushFrameToPipe(&newFrame, newEntity->getPipeId());
        if (ret < 0) {
            CLOGE2("pushFrameToPipeFail, ret(%d)", ret);
            goto cleanup;
        }
        CLOGD2("Instant shot - FD(%d, %d)", buffers[i].fd[0], buffers[i].fd[1]);

        instantQ.pushProcessQ(&newFrame);
    }

    /* 4. pipe instant on */
    ret = m_pipes[INDEX(PIPE_FLITE)]->instantOn(0);
    if (ret < 0) {
        CLOGE2("FLITE On fail, ret(%d)", ret);
        goto cleanup;
    }

    if (newEntity == NULL)
        goto cleanup;

    if (m_parameters->getTpuEnabledMode() == true) {
        ret = m_pipes[INDEX(PIPE_DIS)]->start();
        if (ret < 0) {
            CLOGE2("DIS start fail, ret(%d)", ret);
            goto cleanup;
        }
    }

    if (m_parameters->isUsing3acForIspc() == false) {
        ret = m_pipes[INDEX(PIPE_ISP)]->start();
        if (ret < 0) {
            CLOGE2("PIPE_ISP On fail, ret(%d)", ret);
            goto cleanup;
        }
    }
    ret = m_pipes[INDEX(newEntity->getPipeId())]->instantOn(numFrames);
    if (ret < 0) {
        CLOGE2("3AA On fail, ret(%d)", ret);
        goto cleanup;
    }

    /* 5. setControl to sensor instant on */
    ret = m_pipes[INDEX(PIPE_FLITE)]->setControl(V4L2_CID_IS_S_STREAM, (1 | numFrames << SENSOR_INSTANT_SHIFT));
    if (ret < 0) {
        CLOGE2("instantOn fail, ret(%d)", ret);
        goto cleanup;
    }

cleanup:
    totalRet |= ret;

    /* 6. pipe instant off */
    ret = m_pipes[INDEX(PIPE_FLITE)]->instantOff();
    if (ret < 0) {
        CLOGE2("FLITE Off fail, ret(%d)", ret);
    }

    /* 3AA force done */
    ret = m_pipes[newEntity->getPipeId()]->forceDone(V4L2_CID_IS_FORCE_DONE, 0x1000);
    if (ret < 0) {
        CLOGE2("3AA force done fail, ret(%d)", ret);
    }

    ret = m_pipes[INDEX(newEntity->getPipeId())]->instantOff();
    if (ret < 0) {
        CLOGE2("3AA Off fail, ret(%d)", ret);
    }
    if (m_parameters->isUsing3acForIspc() == false) {
        ret = m_pipes[INDEX(PIPE_ISP)]->stop();
        if (ret < 0) {
            CLOGE2("PIPE_ISP Off fail, ret(%d)", ret);
        }
    }

    if (m_parameters->getTpuEnabledMode() == true) {
        if (m_pipes[INDEX(PIPE_DIS)]->flagStart() == true) {
            ret = m_pipes[INDEX(PIPE_DIS)]->stop();
            if (ret < 0) {
                CLOGE2("PIPE_DIS Off fail, ret(%d)", ret);
            }
        }
    }

    /* setParam for Frame rate : must after setInput on Flite */
    /* rollback framerate after fastenfeenable done */
    memset(&streamParam, 0x0, sizeof(v4l2_streamparm));

    streamParam.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    streamParam.parm.capture.timeperframe.numerator   = 1;
    streamParam.parm.capture.timeperframe.denominator = 30;
    CLOGI2("set framerate (denominator=%d)", frameRate);
    ret = setParam(&streamParam, PIPE_FLITE);
    if (ret < 0) {
        CLOGE2("FLITE setParam fail, ret(%d)", ret);
        return INVALID_OPERATION;
    }

    newFrame = NULL;

    /* clean up all frames */
    for (int i = 0; i < numFrames; i++) {
        if (instantQ.getSizeOfProcessQ() == 0)
            break;

        ret = instantQ.popProcessQ(&newFrame);
        if (ret < 0) {
            CLOGE2("pop instantQ fail, ret(%d)", ret);
            continue;
        }
        if (newFrame == NULL) {
            CLOGE2("newFrame is NULL,");
            continue;
        }
        newFrame->decRef();
        m_frameMgr->deleteFrame(newFrame);
        newFrame = NULL;
    }

    CLOGI2("Done");

    ret |= totalRet;
    return ret;
}

status_t ExynosCamera3FrameFactoryPreview::m_fillNodeGroupInfo(ExynosCameraFrame *frame)
{
    camera2_node_group node_group_info_3aa, node_group_info_isp, node_group_info_dis;
    int zoom = m_parameters->getZoomLevel();
    int previewW = 0, previewH = 0;
    int pictureW = 0, pictureH = 0;
    ExynosRect bnsSize;       /* == bayerCropInputSize */
    ExynosRect bayerCropSize;
    ExynosRect bdsSize;
    int perFramePos = 0;
    bool tpu = false;
    bool dis = false;

    m_parameters->getHwPreviewSize(&previewW, &previewH);
    /* m_parameters->getCallbackSize(&previewW, &previewH); */

    m_parameters->getPictureSize(&pictureW, &pictureH);
    m_parameters->getPreviewBayerCropSize(&bnsSize, &bayerCropSize);
    m_parameters->getPreviewBdsSize(&bdsSize);
    tpu = m_parameters->getTpuEnabledMode();
    dis = m_parameters->getHWVdisMode();

    memset(&node_group_info_3aa, 0x0, sizeof(camera2_node_group));
    memset(&node_group_info_isp, 0x0, sizeof(camera2_node_group));
    memset(&node_group_info_dis, 0x0, sizeof(camera2_node_group));

    /* should add this request value in FrameFactory */
    /* 3AA */
    node_group_info_3aa.leader.request = 1;

    /* 3AC */
    perFramePos = (m_cameraId == CAMERA_ID_BACK) ? PERFRAME_BACK_3AC_POS : PERFRAME_FRONT_3AC_POS;
    node_group_info_3aa.capture[perFramePos].request = frame->getRequest(PIPE_3AC);

    /* 3AP */
    perFramePos = (m_cameraId == CAMERA_ID_BACK) ? PERFRAME_BACK_3AP_POS : PERFRAME_FRONT_3AP_POS;
    node_group_info_3aa.capture[perFramePos].request = frame->getRequest(PIPE_3AP);

    /* should add this request value in FrameFactory */
    /* ISP */
    node_group_info_isp.leader.request = 1;

    /* SCC */
    perFramePos = (m_cameraId == CAMERA_ID_BACK) ? PERFRAME_BACK_SCC_POS : PERFRAME_FRONT_SCC_POS;

    if (m_supportSCC == true)
        node_group_info_isp.capture[perFramePos].request = frame->getRequest(PIPE_SCC);
    else
        node_group_info_isp.capture[perFramePos].request = frame->getRequest(PIPE_ISPC);

    /* DIS */
    memcpy(&node_group_info_dis, &node_group_info_isp, sizeof (camera2_node_group));

    if (tpu == true) {
        /* ISPP */
        if (m_requestISPP == true) {
            perFramePos = (m_cameraId == CAMERA_ID_BACK) ? PERFRAME_BACK_ISPP_POS : PERFRAME_FRONT_ISPP_POS;
            node_group_info_isp.capture[perFramePos].request = frame->getRequest(PIPE_ISPP);
        }

        /* SCP */
        perFramePos = (m_cameraId == CAMERA_ID_BACK) ? PERFRAME_BACK_SCP_POS : PERFRAME_FRONT_SCP_POS;
        node_group_info_dis.capture[perFramePos].request = frame->getRequest(PIPE_SCP);
    } else {
        /* SCP */
        perFramePos = (m_cameraId == CAMERA_ID_BACK) ? PERFRAME_BACK_SCP_POS : PERFRAME_FRONT_SCP_POS;

        if (m_flag3aaIspOTF == true)
            node_group_info_3aa.capture[perFramePos].request = frame->getRequest(PIPE_SCP);
        else
            node_group_info_isp.capture[perFramePos].request = frame->getRequest(PIPE_SCP);
    }

    ExynosCameraNodeGroup3AA::updateNodeGroupInfo(
        m_cameraId,
        &node_group_info_3aa,
        bayerCropSize,
        bdsSize,
        previewW, previewH,
        pictureW, pictureH);

    ExynosCameraNodeGroupISP::updateNodeGroupInfo(
        m_cameraId,
        &node_group_info_isp,
        bayerCropSize,
        bdsSize,
        previewW, previewH,
        pictureW, pictureH,
        dis);

    ExynosCameraNodeGroupDIS::updateNodeGroupInfo(
        m_cameraId,
        &node_group_info_dis,
        bayerCropSize,
        bdsSize,
        previewW, previewH,
        pictureW, pictureH,
        dis);

    frame->storeNodeGroupInfo(&node_group_info_3aa, PERFRAME_INFO_3AA, zoom);
    frame->storeNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_ISP, zoom);
    frame->storeNodeGroupInfo(&node_group_info_dis, PERFRAME_INFO_DIS, zoom);

    return NO_ERROR;
}

ExynosCameraFrame *ExynosCamera3FrameFactoryPreview::createNewFrame(uint32_t frameCount)
{
    int ret = 0;
    ExynosCameraFrameEntity *newEntity[MAX_NUM_PIPES] = {0};
    if (frameCount <= 0) {
        frameCount = m_frameCount;
    }

    ExynosCameraFrame *frame = m_frameMgr->createFrame(m_parameters, frameCount, FRAME_TYPE_PREVIEW);

    int requestEntityCount = 0;
    bool dzoomScaler = false;

    dzoomScaler = m_parameters->getZoomPreviewWIthScaler();

    ret = m_initFrameMetadata(frame);
    if (ret < 0)
        CLOGE2("frame(%d) metadata initialize fail", frameCount);

    if (m_flagFlite3aaOTF == true) {
        if (m_requestFLITE) {
            /* set flite pipe to linkageList */
            newEntity[INDEX(PIPE_FLITE)] = new ExynosCameraFrameEntity(PIPE_FLITE, ENTITY_TYPE_OUTPUT_ONLY, ENTITY_BUFFER_FIXED);
            frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_FLITE)]);
            requestEntityCount++;

        }
        /* set 3AA_ISP pipe to linkageList */
        newEntity[INDEX(PIPE_3AA)] = new ExynosCameraFrameEntity(PIPE_3AA, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_3AA)]);
        requestEntityCount++;

        if (m_requestDIS == true) {
            if (m_flag3aaIspOTF == true) {
                /* set DIS pipe to linkageList */
                newEntity[INDEX(PIPE_DIS)] = new ExynosCameraFrameEntity(PIPE_DIS, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_DELIVERY);
                frame->addChildEntity(newEntity[INDEX(PIPE_3AA)], newEntity[INDEX(PIPE_DIS)], INDEX(PIPE_ISPP));
                requestEntityCount++;
            } else {
                /* set ISP pipe to linkageList */
                newEntity[INDEX(PIPE_ISP)] = new ExynosCameraFrameEntity(PIPE_ISP, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
                frame->addChildEntity(newEntity[INDEX(PIPE_3AA)], newEntity[INDEX(PIPE_ISP)], INDEX(PIPE_3AP));
                requestEntityCount++;

                /* set DIS pipe to linkageList */
                newEntity[INDEX(PIPE_DIS)] = new ExynosCameraFrameEntity(PIPE_DIS, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_DELIVERY);
                frame->addChildEntity(newEntity[INDEX(PIPE_ISP)], newEntity[INDEX(PIPE_DIS)], INDEX(PIPE_ISPP));
                requestEntityCount++;
            }
        } else {
            if (m_flag3aaIspOTF == true) {
                /* skip ISP pipe to linkageList */
            } else {
                /* set ISP pipe to linkageList */
                newEntity[INDEX(PIPE_ISP)] = new ExynosCameraFrameEntity(PIPE_ISP, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
                frame->addChildEntity(newEntity[INDEX(PIPE_3AA)], newEntity[INDEX(PIPE_ISP)], INDEX(PIPE_3AP));
                requestEntityCount++;
            }
        }
    } else {
        /* set flite pipe to linkageList */
        newEntity[INDEX(PIPE_FLITE)] = new ExynosCameraFrameEntity(PIPE_FLITE, ENTITY_TYPE_OUTPUT_ONLY, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_FLITE)]);

        /* set 3AA pipe to linkageList */
        newEntity[INDEX(PIPE_3AA)] = new ExynosCameraFrameEntity(PIPE_3AA, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
        frame->addChildEntity(newEntity[INDEX(PIPE_FLITE)], newEntity[INDEX(PIPE_3AA)]);

        /* set ISP pipe to linkageList */
        if (m_requestISP == true) {
            newEntity[INDEX(PIPE_ISP)] = new ExynosCameraFrameEntity(PIPE_ISP, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
            frame->addChildEntity(newEntity[INDEX(PIPE_3AA)], newEntity[INDEX(PIPE_ISP)]);
        }

        /* set DIS pipe to linkageList */
        if (m_requestDIS == true) {
            newEntity[INDEX(PIPE_DIS)] = new ExynosCameraFrameEntity(PIPE_DIS, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_DELIVERY);
            frame->addChildEntity(newEntity[INDEX(PIPE_ISP)], newEntity[INDEX(PIPE_DIS)]);
        }

        /* flite, 3aa, isp, dis as one. */
        requestEntityCount++;
    }

/* Comment out, because it included 3AA */
#if 0
    if (m_request3AC) {
        /* set 3AC pipe to linkageList */
        newEntity[INDEX(PIPE_3AC)] = new ExynosCameraFrameEntity(PIPE_3AC, ENTITY_TYPE_OUTPUT_ONLY, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_3AC)]);
        requestEntityCount++;
    }
#endif

/* Comment out, because it included ISP */
#if 0
       if (m_requestISPC) {
           /* set ISPC pipe to linkageList */
           newEntity[INDEX(PIPE_ISPC)] = new ExynosCameraFrameEntity(PIPE_ISPC, ENTITY_TYPE_OUTPUT_ONLY, ENTITY_BUFFER_FIXED);
           frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_ISPC)]);
           requestEntityCount++;
       }
#endif

    /* set SCP pipe to linkageList */
/* Comment out, because it included ISP */
#if 0
    newEntity[INDEX(PIPE_SCP)] = new ExynosCameraFrameEntity(PIPE_SCP, ENTITY_TYPE_OUTPUT_ONLY, ENTITY_BUFFER_DELIVERY);
    frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_SCP)]);
    requestEntityCount++;
#endif

    if (m_flagMcscVraOTF == false) {
        newEntity[INDEX(PIPE_VRA)] = new ExynosCameraFrameEntity(PIPE_VRA, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
        frame->addChildEntity(newEntity[INDEX(PIPE_3AA)], newEntity[INDEX(PIPE_VRA)], /* Parent of VRA @ 34xx */PIPE_MCSC0);
        requestEntityCount++;
    }

    if (m_supportReprocessing == false) {
        /* set GSC-Picture pipe to linkageList */
        newEntity[INDEX(PIPE_GSC_PICTURE)] = new ExynosCameraFrameEntity(PIPE_GSC_PICTURE, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_GSC_PICTURE)]);
    }

    /* set GSC pipe to linkageList */
    newEntity[INDEX(PIPE_GSC)] = new ExynosCameraFrameEntity(PIPE_GSC, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_GSC)]);

    if (dzoomScaler) {
        requestEntityCount++;
    }

    newEntity[INDEX(PIPE_GSC_VIDEO)] = new ExynosCameraFrameEntity(PIPE_GSC_VIDEO, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_GSC_VIDEO)]);

    /* PIPE_VRA's internal pipe entity */
    newEntity[INDEX(PIPE_GSC_VRA)] = new ExynosCameraFrameEntity(PIPE_GSC_VRA, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_GSC_VRA)]);

    if (m_supportReprocessing == false) {
        /* set JPEG pipe to linkageList */
        newEntity[INDEX(PIPE_JPEG)] = new ExynosCameraFrameEntity(PIPE_JPEG, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_JPEG)]);
    }

    ret = m_initPipelines(frame);
    if (ret < 0) {
        CLOGE2("m_initPipelines fail, ret(%d)", ret);
    }

    /* TODO: make it dynamic */
    frame->setNumRequestPipe(requestEntityCount);

    m_fillNodeGroupInfo(frame);

    m_frameCount++;

    return frame;
}

status_t ExynosCamera3FrameFactoryPreview::initPipes(void)
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    int ret = 0;

    ret = m_initFlitePipe();
    if (ret != NO_ERROR) {
        CLOGE2("m_initFlitePipe() fail");
        return ret;
    }

    ret = m_setDeviceInfo();
    if (ret != NO_ERROR) {
        CLOGE2("m_setDeviceInfo() fail");
        return ret;
    }

    ret = m_initPipes();
    if (ret != NO_ERROR) {
        CLOGE2("m_initPipes() fail");
        return ret;
    }

    m_frameCount = 0;

    return NO_ERROR;
}

status_t ExynosCamera3FrameFactoryPreview::preparePipes(void)
{
    int ret = 0;

    /* NOTE: Prepare for 3AA is moved after ISP stream on */

    if (m_requestFLITE) {
        ret = m_pipes[INDEX(PIPE_FLITE)]->prepare();
        if (ret < 0) {
            CLOGE2("FLITE prepare fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

/* Comment out, because it included ISP */
#if 0
    ret = m_pipes[INDEX(PIPE_SCP)]->prepare();
    if (ret < 0) {
        CLOGE2("SCP prepare fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
#endif

    return NO_ERROR;
}

status_t ExynosCamera3FrameFactoryPreview::preparePipes(uint32_t prepareCnt)
{
    int ret = 0;

    /* NOTE: Prepare for 3AA is moved after ISP stream on */

    if (prepareCnt) {
        ret = m_pipes[INDEX(PIPE_FLITE)]->prepare(prepareCnt);
        if (ret < 0) {
            CLOGE2("PIPE_FLITE prepare(%d) fail, ret(%d)", prepareCnt, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }

        if (m_supportSCC == true) {
            enum pipeline pipe = (m_supportSCC == true) ? PIPE_SCC : PIPE_ISPC;

            ret = m_pipes[INDEX(pipe)]->prepare();
            if (ret < 0) {
                CLOGE2("%s prepare fail, ret(%d)", m_pipes[INDEX(pipe)]->getPipeName(), ret);
                /* TODO: exception handling */
                return INVALID_OPERATION;
            }
        }
    }

    return NO_ERROR;
}

status_t ExynosCamera3FrameFactoryPreview::startPipes(void)
{
    int ret = 0;

    if (m_flagMcscVraOTF == false) {
        ret = m_pipes[INDEX(PIPE_VRA)]->start();
        if (ret < 0) {
            CLOGE2("VRA start fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    if (m_parameters->getTpuEnabledMode() == true) {
        ret = m_pipes[INDEX(PIPE_DIS)]->start();
        if (ret < 0) {
            CLOGE2("DIS start fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    ret = m_pipes[INDEX(PIPE_3AA)]->start();
    if (ret < 0) {
        CLOGE2("3AA start fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ret = m_pipes[INDEX(PIPE_FLITE)]->start();
    if (ret < 0) {
        CLOGE("FLITE start fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    if (m_flagFlite3aaOTF == true) {
        /* Here is doing 3AA prepare(qbuf) */
        ret = m_pipes[INDEX(PIPE_3AA)]->prepare();
        if (ret < 0) {
            CLOGE2("3AA prepare fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    ret = m_pipes[INDEX(PIPE_FLITE)]->sensorStream(true);
    if (ret < 0) {
        CLOGE2("FLITE sensorStream on fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    CLOGI2("Starting Success!");

    return NO_ERROR;
}

status_t ExynosCamera3FrameFactoryPreview::startInitialThreads(void)
{
    int ret = 0;

    CLOGI2("start pre-ordered initial pipe thread");

    if (m_requestFLITE) {
        ret = startThread(PIPE_FLITE);
        if (ret < 0)
            return ret;
    }

    ret = startThread(PIPE_3AA);
    if (ret < 0)
        return ret;

    if (m_parameters->is3aaIspOtf() == false) {
        ret = startThread(PIPE_ISP);
        if (ret < 0)
            return ret;
    }

    if (m_parameters->getTpuEnabledMode() == true) {
        ret = startThread(PIPE_DIS);
        if (ret < 0)
            return ret;
    }

    if (m_parameters->isMcscVraOtf() == false) {
        ret = startThread(PIPE_VRA);
        if (ret < 0)
            return ret;
    }

    return NO_ERROR;
}

status_t ExynosCamera3FrameFactoryPreview::setStopFlag(void)
{
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    int ret = 0;

    ret = m_pipes[INDEX(PIPE_FLITE)]->setStopFlag();

    if (m_pipes[INDEX(PIPE_3AA)]->flagStart() == true)
        ret |= m_pipes[INDEX(PIPE_3AA)]->setStopFlag();

    if (m_pipes[INDEX(PIPE_ISP)]->flagStart() == true)
        ret |= m_pipes[INDEX(PIPE_ISP)]->setStopFlag();

    if (m_parameters->getHWVdisMode() == true
        && m_pipes[INDEX(PIPE_DIS)]->flagStart() == true)
        ret |= m_pipes[INDEX(PIPE_DIS)]->setStopFlag();

    if (m_flagMcscVraOTF == false
        && m_pipes[INDEX(PIPE_VRA)]->flagStart() == true)
        ret |= m_pipes[INDEX(PIPE_VRA)]->setStopFlag();

    return NO_ERROR;
}

status_t ExynosCamera3FrameFactoryPreview::stopPipes(void)
{
    int ret = 0;

    if (m_pipes[INDEX(PIPE_VRA)] != NULL
        && m_pipes[INDEX(PIPE_VRA)]->isThreadRunning() == true) {
        ret = m_pipes[INDEX(PIPE_VRA)]->stopThread();
        if (ret < 0) {
            CLOGE2("VRA stopThread fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    if (m_pipes[INDEX(PIPE_DIS)] != NULL
        && m_pipes[INDEX(PIPE_DIS)]->isThreadRunning() == true) {
        ret = m_pipes[INDEX(PIPE_DIS)]->stopThread();
        if (ret < 0) {
            CLOGE2("DIS stopThread fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

/* Comment out, because it included 3AA */
#if 0
    if (m_supportReprocessing == true) {
        if (m_supportPureBayerReprocessing == false) {
            ret = m_pipes[INDEX(PIPE_3AC)]->stopThread();
            if (ret < 0) {
                CLOGE2("3AC stopThread fail, ret(%d)", ret);
                /* TODO: exception handling */
                return INVALID_OPERATION;
            }
        }
    }
#endif

/* Comment out, because it same with M2M */
#if 0
    if (m_flagFlite3aaOTF == true) {
        ret = m_pipes[INDEX(PIPE_3AA_ISP)]->stopThread();
        if (ret < 0) {
            CLOGE2("3AA_ISP stopThread fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    } else
#endif
    {
        if (m_pipes[INDEX(PIPE_3AA)]->isThreadRunning() == true) {
            ret = m_pipes[INDEX(PIPE_3AA)]->stopThread();
            if (ret < 0) {
                CLOGE2("3AA stopThread fail, ret(%d)", ret);
                /* TODO: exception handling */
                return INVALID_OPERATION;
            }
        }

        /* stream off for ISP */
        if (m_pipes[INDEX(PIPE_ISP)] != NULL
            && m_pipes[INDEX(PIPE_ISP)]->isThreadRunning() == true) {
            ret = m_pipes[INDEX(PIPE_ISP)]->stopThread();
            if (ret < 0) {
                CLOGE2("ISP stopThread fail, ret(%d)", ret);
                /* TODO: exception handling */
                return INVALID_OPERATION;
            }
        }
    }

    if (m_requestFLITE) {
        ret = m_pipes[INDEX(PIPE_FLITE)]->stopThread();
        if (ret < 0) {
            CLOGE2("FLITE stopThread fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    if (m_pipes[INDEX(PIPE_GSC)]->isThreadRunning() == true) {
        ret = stopThread(INDEX(PIPE_GSC));
        if (ret < 0) {
            CLOGE2("PIPE_GSC stopThread fail, ret(%d)", ret);
            return INVALID_OPERATION;
        }
    }

    ret = m_pipes[INDEX(PIPE_FLITE)]->sensorStream(false);
    if (ret < 0) {
        CLOGE2("FLITE sensorStream off fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ret = m_pipes[INDEX(PIPE_FLITE)]->stop();
    if (ret < 0) {
        CLOGE2("FLITE stop fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    /* 3AA force done */
    ret = m_pipes[INDEX(PIPE_3AA)]->forceDone(V4L2_CID_IS_FORCE_DONE, 0x1000);
    if (ret < 0) {
        CLOGE2("PIPE_3AA force done fail, ret(%d)", ret);
        /* TODO: exception handling */
        /* return INVALID_OPERATION; */
    }

    /* stream off for 3AA */
    ret = m_pipes[INDEX(PIPE_3AA)]->stop();
    if (ret < 0) {
        CLOGE2("3AA stop fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    /* ISP force done */
    if (m_pipes[INDEX(PIPE_ISP)]->flagStart() == true) {
        ret = m_pipes[INDEX(PIPE_ISP)]->forceDone(V4L2_CID_IS_FORCE_DONE, 0x1000);
        if (ret < 0) {
            CLOGE2("PIPE_ISP force done fail, ret(%d)", ret);
            /* TODO: exception handling */
            /* return INVALID_OPERATION; */
        }

        /* stream off for ISP */
        ret = m_pipes[INDEX(PIPE_ISP)]->stop();
        if (ret < 0) {
            CLOGE2("ISP stop fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    if (m_parameters->getHWVdisMode()) {
        if (m_pipes[INDEX(PIPE_DIS)]->flagStart() == true) {
            /* DIS force done */
            ret = m_pipes[INDEX(PIPE_DIS)]->forceDone(V4L2_CID_IS_FORCE_DONE, 0x1000);
            if (ret < 0) {
                CLOGE2("PIPE_DIS force done fail, ret(%d)", ret);
                /* TODO: exception handling */
                /* return INVALID_OPERATION; */
            }

            ret = m_pipes[INDEX(PIPE_DIS)]->stop();
            if (ret < 0) {
                CLOGE2("DIS stop fail, ret(%d)", ret);
                /* TODO: exception handling */
                return INVALID_OPERATION;
            }
        }
    }

    if (m_flagMcscVraOTF == false) {
        if (m_pipes[INDEX(PIPE_VRA)]->flagStart() == true) {
            /* VRA force done */
            ret = m_pipes[INDEX(PIPE_VRA)]->forceDone(V4L2_CID_IS_FORCE_DONE, 0x1000);
            if (ret < 0) {
                CLOGE2("PIPE_VRA force done fail, ret(%d)", ret);
                /* TODO: exception handling */
                /* return INVALID_OPERATION; */
            }

            ret = m_pipes[INDEX(PIPE_VRA)]->stop();
            if (ret < 0) {
                CLOGE2("VRA stop fail, ret(%d)", ret);
                /* TODO: exception handling */
                return INVALID_OPERATION;
            }
        }
    }

    ret = stopThreadAndWait(INDEX(PIPE_GSC));
    if (ret < 0) {
        CLOGE2("PIPE_GSC stopThreadAndWait fail, ret(%d)", ret);
    }

    CLOGI2("Stopping Success!");

    return NO_ERROR;
}

void ExynosCamera3FrameFactoryPreview::m_init(void)
{
    m_supportReprocessing = false;
    m_flagFlite3aaOTF = false;
    m_flagIspMcscOTF = false;
    m_flagMcscVraOTF = false;
    m_supportSCC = false;
    m_supportMCSC = false;
    m_supportPureBayerReprocessing = false;
    m_flagReprocessing = false;
}

status_t ExynosCamera3FrameFactoryPreview::m_setupConfig()
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    status_t ret = NO_ERROR;

    int32_t *nodeNums = NULL;
    int32_t *controlId = NULL;
    int32_t *secondaryControlId = NULL;
    int32_t *prevNode = NULL;

    m_flagFlite3aaOTF = m_parameters->isFlite3aaOtf();
    m_flag3aaIspOTF = m_parameters->is3aaIspOtf();
    m_flagIspMcscOTF = m_parameters->isIspMcscOtf();
    m_flagMcscVraOTF = m_parameters->isMcscVraOtf();
    m_supportReprocessing = m_parameters->isReprocessing();
    m_supportSCC = m_parameters->isOwnScc(m_cameraId);
    m_supportMCSC = m_parameters->isOwnMCSC();

    if (m_parameters->getRecordingHint() == true) {
        m_supportPureBayerReprocessing = (m_cameraId == CAMERA_ID_BACK) ? USE_PURE_BAYER_REPROCESSING_ON_RECORDING : USE_PURE_BAYER_REPROCESSING_FRONT_ON_RECORDING;
    } else {
        m_supportPureBayerReprocessing = (m_cameraId == CAMERA_ID_BACK) ? USE_PURE_BAYER_REPROCESSING : USE_PURE_BAYER_REPROCESSING_FRONT;
    }

    m_flagReprocessing = false;

    if (m_supportReprocessing == false) {
        if (m_supportSCC == true)
            m_requestSCC = 1;
        else
            m_requestISPC = 1;
    }

    if (m_flag3aaIspOTF == true) {
        m_request3AP = 0;
        m_requestISP = 0;
    } else {
        m_request3AP = 1;
        m_requestISP = 1;
    }

    if (m_flagMcscVraOTF == true)
        m_requestVRA = 0;
    else
        m_requestVRA = 1;

    nodeNums = m_nodeNums[INDEX(PIPE_FLITE)];
    nodeNums[OUTPUT_NODE] = -1;
    nodeNums[CAPTURE_NODE_1] = m_getFliteNodenum();
    nodeNums[CAPTURE_NODE_2] = -1;
    controlId = m_sensorIds[INDEX(PIPE_FLITE)];
    controlId[CAPTURE_NODE_1] = m_getSensorId(nodeNums[CAPTURE_NODE_1], m_flagReprocessing);

    ret = m_setDeviceInfo();
    if (ret != NO_ERROR) {
        CLOGE2("m_setDeviceInfo() fail");
        return ret;
    }

    nodeNums = m_nodeNums[INDEX(PIPE_GSC)];
    nodeNums[OUTPUT_NODE] = PREVIEW_GSC_NODE_NUM;
    nodeNums[CAPTURE_NODE_1] = -1;
    nodeNums[CAPTURE_NODE_2] = -1;

    nodeNums = m_nodeNums[INDEX(PIPE_GSC_VIDEO)];
    nodeNums[OUTPUT_NODE] = VIDEO_GSC_NODE_NUM;
    nodeNums[CAPTURE_NODE_1] = -1;
    nodeNums[CAPTURE_NODE_2] = -1;

    nodeNums = m_nodeNums[INDEX(PIPE_GSC_PICTURE)];
    nodeNums[OUTPUT_NODE] = PICTURE_GSC_NODE_NUM;
    nodeNums[CAPTURE_NODE_1] = -1;
    nodeNums[CAPTURE_NODE_2] = -1;

    nodeNums = m_nodeNums[INDEX(PIPE_JPEG)];
    nodeNums[OUTPUT_NODE] = -1;
    nodeNums[CAPTURE_NODE_1] = -1;
    nodeNums[CAPTURE_NODE_2] = -1;

    return NO_ERROR;
}

status_t ExynosCamera3FrameFactoryPreview::m_setDeviceInfo(void)
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    bool flagDirtyBayer = false;

    if (m_supportReprocessing == true && m_supportPureBayerReprocessing == false)
        flagDirtyBayer = true;

    int pipeId = -1;
    int previousPipeId = -1;
    enum NODE_TYPE nodeType = INVALID_NODE;

    int32_t *nodeNums = NULL;
    int32_t *controlId = NULL;

    int t3aaNums[MAX_NODE];
    int ispNums[MAX_NODE];

    if (m_parameters->getDualMode() == true) {
        t3aaNums[OUTPUT_NODE]    = FIMC_IS_VIDEO_31S_NUM;
        t3aaNums[CAPTURE_NODE_1] = FIMC_IS_VIDEO_31C_NUM;
        t3aaNums[CAPTURE_NODE_2] = FIMC_IS_VIDEO_31P_NUM;
    } else {
        t3aaNums[OUTPUT_NODE]    = FIMC_IS_VIDEO_30S_NUM;
        t3aaNums[CAPTURE_NODE_1] = FIMC_IS_VIDEO_30C_NUM;
        t3aaNums[CAPTURE_NODE_2] = FIMC_IS_VIDEO_30P_NUM;
    }

    ispNums[OUTPUT_NODE]    = FIMC_IS_VIDEO_I0S_NUM;
    ispNums[CAPTURE_NODE_1] = FIMC_IS_VIDEO_I0C_NUM;
    ispNums[CAPTURE_NODE_2] = FIMC_IS_VIDEO_I0P_NUM;

    m_initDeviceInfo(INDEX(PIPE_3AA));
    m_initDeviceInfo(INDEX(PIPE_ISP));
    m_initDeviceInfo(INDEX(PIPE_DIS));

    /*******
     * 3AA
     ******/
    pipeId = INDEX(PIPE_3AA);

    // 3AS
    nodeType = getNodeType(PIPE_3AA);
    m_nodeInfo[pipeId].nodeNum[nodeType] = t3aaNums[OUTPUT_NODE];
    strncpy(m_nodeInfo[pipeId].nodeName[nodeType], "3AA_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType]  = m_getSensorId(m_nodeNums[INDEX(PIPE_FLITE)][getNodeType(PIPE_FLITE)], m_flagFlite3aaOTF, true, m_flagReprocessing);
    m_nodeInfo[pipeId].pipeId[nodeType]  = PIPE_3AA;

    // 3AC
    nodeType = getNodeType(PIPE_3AC);
    if (flagDirtyBayer == true || m_parameters->isUsing3acForIspc() == true) {
        m_nodeInfo[pipeId].nodeNum[nodeType] = t3aaNums[CAPTURE_NODE_1];
        strncpy(m_nodeInfo[pipeId].nodeName[nodeType], "3AA_CAPTURE", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_nodeInfo[pipeId].nodeNum[getNodeType(PIPE_3AA)], false, false, m_flagReprocessing);
        m_nodeInfo[pipeId].pipeId[nodeType] = PIPE_3AC;
    } else {
        m_nodeInfo[pipeId].secondaryNodeNum[nodeType] = t3aaNums[CAPTURE_NODE_1];
        strncpy(m_nodeInfo[pipeId].secondaryNodeName[nodeType], "3AA_CAPTURE", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_secondarySensorIds[pipeId][nodeType] = m_getSensorId(m_nodeInfo[pipeId].nodeNum[getNodeType(PIPE_3AA)], true, false, m_flagReprocessing);
        m_nodeInfo[pipeId].pipeId[nodeType] = PIPE_3AC;
    }

    // 3AP
    nodeType = getNodeType(PIPE_3AP);
    m_nodeInfo[pipeId].secondaryNodeNum[nodeType] = t3aaNums[CAPTURE_NODE_2];
    strncpy(m_nodeInfo[pipeId].secondaryNodeName[nodeType], "3AA_PREVIEW", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_secondarySensorIds[pipeId][nodeType] = m_getSensorId(m_nodeInfo[pipeId].nodeNum[getNodeType(PIPE_3AA)], m_flag3aaIspOTF, false, m_flagReprocessing);
    m_nodeInfo[pipeId].pipeId[nodeType] = PIPE_3AP;

    // ISPS
    nodeType = getNodeType(PIPE_ISP);
    m_nodeInfo[pipeId].secondaryNodeNum[nodeType] = ispNums[OUTPUT_NODE];
    strncpy(m_nodeInfo[pipeId].secondaryNodeName[nodeType], "ISP_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_secondarySensorIds[pipeId][nodeType]  = m_getSensorId(m_nodeInfo[pipeId].secondaryNodeNum[getNodeType(PIPE_3AP)], m_flag3aaIspOTF, false, m_flagReprocessing);
    m_nodeInfo[pipeId].pipeId[nodeType]  = PIPE_ISP;

    // ISPP
    nodeType = getNodeType(PIPE_ISPP);
    m_nodeInfo[pipeId].secondaryNodeNum[nodeType] = ispNums[CAPTURE_NODE_2];
    strncpy(m_nodeInfo[pipeId].secondaryNodeName[nodeType], "ISP_PREVIEW", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_secondarySensorIds[pipeId][nodeType] = m_getSensorId(m_nodeInfo[pipeId].secondaryNodeNum[getNodeType(PIPE_ISP)], true, false, m_flagReprocessing);
    m_nodeInfo[pipeId].pipeId[nodeType] = PIPE_ISPP;

    // DIS
    if (m_parameters->getHWVdisMode()) {
        nodeType = getNodeType(PIPE_DIS);
        m_nodeInfo[pipeId].secondaryNodeNum[nodeType] = FIMC_IS_VIDEO_TPU_NUM;
        strncpy(m_nodeInfo[pipeId].secondaryNodeName[nodeType], "DIS_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_secondarySensorIds[pipeId][nodeType] = m_getSensorId(m_nodeInfo[pipeId].secondaryNodeNum[getNodeType(PIPE_ISPP)], true, false, m_flagReprocessing);
        m_nodeInfo[pipeId].pipeId[nodeType]  = PIPE_DIS;
    }

    if (m_supportMCSC == true) {
        // MCSC
        nodeType = getNodeType(PIPE_MCSC);
        m_nodeInfo[pipeId].secondaryNodeNum[nodeType] = FIMC_IS_VIDEO_M0S_NUM;
        strncpy(m_nodeInfo[pipeId].secondaryNodeName[nodeType], "MCSC_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_secondarySensorIds[pipeId][nodeType]  = m_getSensorId(m_nodeInfo[pipeId].secondaryNodeNum[getNodeType(PIPE_ISPP)], m_flagIspMcscOTF, false, m_flagReprocessing);
        m_nodeInfo[pipeId].pipeId[nodeType]  = PIPE_MCSC;

        // MCSC0
        nodeType = getNodeType(PIPE_MCSC0);
        m_nodeInfo[pipeId].nodeNum[nodeType] = FIMC_IS_VIDEO_M0P_NUM;
        strncpy(m_nodeInfo[pipeId].nodeName[nodeType], "MCSC_PREVIEW", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_nodeInfo[pipeId].secondaryNodeNum[getNodeType(PIPE_MCSC)], true, false, m_flagReprocessing);
        m_nodeInfo[pipeId].pipeId[nodeType] = PIPE_MCSC0;
    } else {
        // SCP
        nodeType = getNodeType(PIPE_SCP);
        m_nodeInfo[pipeId].nodeNum[nodeType] = FIMC_IS_VIDEO_SCP_NUM;
        strncpy(m_nodeInfo[pipeId].nodeName[nodeType], "SCP_PREVIEW", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_nodeInfo[pipeId].secondaryNodeNum[getNodeType(PIPE_ISPP)], true, false, m_flagReprocessing);
        m_nodeInfo[pipeId].pipeId[nodeType] = PIPE_SCP;
    }

    // set nodeNum
    for (int i = 0; i < MAX_NODE; i++)
        m_nodeNums[pipeId][i] = m_nodeInfo[pipeId].nodeNum[i];

    if (m_checkNodeSetting(pipeId) != NO_ERROR) {
        CLOGE2("m_checkNodeSetting(%d) fail", pipeId);
        return INVALID_OPERATION;
    }

    // VRA
    previousPipeId = pipeId;
    nodeNums = m_nodeNums[INDEX(PIPE_VRA)];
    nodeNums[OUTPUT_NODE] = FIMC_IS_VIDEO_VRA_NUM;
    nodeNums[CAPTURE_NODE_1] = -1;
    nodeNums[CAPTURE_NODE_2] = -1;
    controlId = m_sensorIds[INDEX(PIPE_VRA)];
    controlId[OUTPUT_NODE] = m_getSensorId(m_nodeInfo[previousPipeId].nodeNum[getNodeType(PIPE_SCP)], m_flagMcscVraOTF, true, true);
    controlId[CAPTURE_NODE_1] = -1;
    controlId[CAPTURE_NODE_2] = -1;

    return NO_ERROR;
}

status_t ExynosCamera3FrameFactoryPreview::m_initPipes(void)
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    int ret = 0;
    camera_pipe_info_t pipeInfo[MAX_NODE];
    camera_pipe_info_t nullPipeInfo;

    int32_t nodeNums[MAX_NODE];
    int32_t sensorIds[MAX_NODE];
    int32_t secondarySensorIds[MAX_NODE];
    for (int i = 0; i < MAX_NODE; i++) {
        nodeNums[i] = -1;
        sensorIds[i] = -1;
        secondarySensorIds[i] = -1;
    }

    ExynosRect tempRect;
    int maxSensorW = 0, maxSensorH = 0, hwSensorW = 0, hwSensorH = 0;
    int maxPreviewW = 0, maxPreviewH = 0, hwPreviewW = 0, hwPreviewH = 0;
    int maxPictureW = 0, maxPictureH = 0, hwPictureW = 0, hwPictureH = 0;
    int bayerFormat = CAMERA_BAYER_FORMAT;
    int previewFormat = m_parameters->getHwPreviewFormat();
    int pictureFormat = m_parameters->getPictureFormat();
    int hwVdisformat = m_parameters->getHWVdisFormat();
    struct ExynosConfigInfo *config = m_parameters->getConfig();
    ExynosRect bdsSize;
    ExynosRect bnsSize;
    ExynosRect bcropSize;
    int perFramePos = 0;
    int stride = 0;

#ifdef DEBUG_RAWDUMP
    if (m_parameters->checkBayerDumpEnable()) {
        bayerFormat = CAMERA_DUMP_BAYER_FORMAT;
    }
#endif

    m_parameters->getMaxSensorSize(&maxSensorW, &maxSensorH);
    m_parameters->getHwSensorSize(&hwSensorW, &hwSensorH);

    m_parameters->getMaxPreviewSize(&maxPreviewW, &maxPreviewH);
    m_parameters->getHwPreviewSize(&hwPreviewW, &hwPreviewH);

    m_parameters->getMaxPictureSize(&maxPictureW, &maxPictureH);
    m_parameters->getHwPictureSize(&hwPictureW, &hwPictureH);
    m_parameters->getPreviewBayerCropSize(&bnsSize, &bcropSize);
    m_parameters->getPreviewBdsSize(&bdsSize);

    /* When high speed recording mode, hw sensor size is fixed.
     * So, maxPreview size cannot exceed hw sensor size
     */
    if (m_parameters->getHighSpeedRecording()) {
        maxPreviewW = hwSensorW;
        maxPreviewH = hwSensorH;
    }

    CLOGI2("MaxSensorSize(%dx%d), HWSensorSize(%dx%d)", maxSensorW, maxSensorH, hwSensorW, hwSensorH);
    CLOGI2("MaxPreviewSize(%dx%d), HwPreviewSize(%dx%d)", maxPreviewW, maxPreviewH, hwPreviewW, hwPreviewH);
    CLOGI2("HWPictureSize(%dx%d)", hwPictureW, hwPictureH);
    CLOGI2("BcropSize(%dx%d)", bcropSize.w, bcropSize.h);

    /* 3AS */
    enum NODE_TYPE t3asNodeType = getNodeType(PIPE_3AA);

    for (int i = 0; i < MAX_NODE; i++)
        pipeInfo[i] = nullPipeInfo;

    for (int i = 0; i < MAX_NODE; i++) {
        ret = m_pipes[INDEX(PIPE_3AA)]->setPipeId((enum NODE_TYPE)i, m_nodeInfo[PIPE_3AA].pipeId[i]);
        if (ret < 0) {
            CLOGE2("setPipeId(%d, %d) fail, ret(%d)", i, m_nodeInfo[PIPE_3AA].pipeId[i], ret);
            return ret;
        }

        sensorIds[i] = m_sensorIds[INDEX(PIPE_3AA)][i];
        secondarySensorIds[i] = m_secondarySensorIds[INDEX(PIPE_3AA)][i];
    }

    if (m_flagFlite3aaOTF == true) {
        tempRect.fullW = 32;
        tempRect.fullH = 64;
        tempRect.colorFormat = bayerFormat;

        pipeInfo[t3asNodeType].bufInfo.count = config->current->bufInfo.num_3aa_buffers;
    } else {
        tempRect.fullW = maxSensorW;
        tempRect.fullH = maxSensorH;
        tempRect.colorFormat = bayerFormat;

#ifdef CAMERA_PACKED_BAYER_ENABLE
#ifdef DEBUG_RAWDUMP
        if (m_parameters->checkBayerDumpEnable()) {
            /* packed bayer bytesPerPlane */
            pipeInfo[t3asNodeType].bytesPerPlane[0] = ROUND_UP(tempRect.fullW, 10) * 2;
        }
        else
#endif
        {
            /* packed bayer bytesPerPlane */
            pipeInfo[t3asNodeType].bytesPerPlane[0] = ROUND_UP(tempRect.fullW, 10) * 8 / 5;
        }
#endif

        pipeInfo[t3asNodeType].bufInfo.count = config->current->bufInfo.num_bayer_buffers;
    }

    pipeInfo[t3asNodeType].rectInfo = tempRect;
    pipeInfo[t3asNodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    pipeInfo[t3asNodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;

    /* per frame info */
    pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = CAPTURE_NODE_MAX;
    pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perframeInfoIndex = PERFRAME_INFO_3AA;

    pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_LEADER;
    pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameVideoID = (m_nodeInfo[INDEX(PIPE_3AA)].nodeNum[t3asNodeType] - FIMC_IS_VIDEO_BAS_NUM);

    /* 3AC */
    if ((m_supportReprocessing == true && m_supportPureBayerReprocessing == false) || m_parameters->isUsing3acForIspc() == true) {
        enum NODE_TYPE t3acNodeType = getNodeType(PIPE_3AC);
        perFramePos = (m_cameraId == CAMERA_ID_BACK) ? PERFRAME_BACK_3AC_POS : PERFRAME_FRONT_3AC_POS;
        pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameNodeType = PERFRAME_NODE_TYPE_CAPTURE;
        pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameVideoID = (m_nodeInfo[INDEX(PIPE_3AA)].nodeNum[t3acNodeType] - FIMC_IS_VIDEO_BAS_NUM);

#ifdef FIXED_SENSOR_SIZE
        tempRect.fullW = maxSensorW;
        tempRect.fullH = maxSensorH;
#else
        tempRect.fullW = hwSensorW;
        tempRect.fullH = hwSensorH;
#endif
        if (m_parameters->isUsing3acForIspc() == true) {
            tempRect.colorFormat = SCC_OUTPUT_COLOR_FMT;
            /* tempRect.fullW = hwPictureW; */
            /* tempRect.fullH = hwPictureH; */
            tempRect.fullW = bcropSize.w;
            tempRect.fullH = bcropSize.h;
        } else {
            tempRect.colorFormat = bayerFormat;
        }

        pipeInfo[t3acNodeType].rectInfo = tempRect;
        pipeInfo[t3acNodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        pipeInfo[t3acNodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
        pipeInfo[t3acNodeType].bufInfo.count = config->current->bufInfo.num_sensor_buffers + config->current->bufInfo.num_bayer_buffers;
        /* per frame info */
        pipeInfo[t3acNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
        pipeInfo[t3acNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;
    }

    /* 3AP */
    enum NODE_TYPE t3apNodeType = getNodeType(PIPE_3AP);

    perFramePos = (m_cameraId == CAMERA_ID_BACK) ? PERFRAME_BACK_3AP_POS : PERFRAME_FRONT_3AP_POS;
    pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameNodeType = PERFRAME_NODE_TYPE_CAPTURE;
    pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameVideoID = (m_nodeInfo[INDEX(PIPE_3AA)].secondaryNodeNum[t3apNodeType] - FIMC_IS_VIDEO_BAS_NUM);

    pipeInfo[t3apNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
    pipeInfo[t3apNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;

    /* ISPS */
    enum NODE_TYPE ispsNodeType = getNodeType(PIPE_ISP);

    pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
    pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perframeInfoIndex = PERFRAME_INFO_ISP;
    pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_LEADER;
    pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameVideoID = (m_nodeInfo[INDEX(PIPE_3AA)].secondaryNodeNum[ispsNodeType] - FIMC_IS_VIDEO_BAS_NUM);

    /* ISPP */
    enum NODE_TYPE isppNodeType = getNodeType(PIPE_ISPP);

    perFramePos = (m_cameraId == CAMERA_ID_BACK) ? PERFRAME_BACK_ISPP_POS : PERFRAME_FRONT_ISPP_POS;
    pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameNodeType = PERFRAME_NODE_TYPE_CAPTURE;
    pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameVideoID = (m_nodeInfo[PIPE_3AA].secondaryNodeNum[isppNodeType] - FIMC_IS_VIDEO_BAS_NUM);

    pipeInfo[isppNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
    pipeInfo[isppNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;
    if (m_parameters->getHWVdisMode()) {
        /* DIS */
        enum NODE_TYPE disNodeType = getNodeType(PIPE_DIS);

        pipeInfo[disNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
        pipeInfo[disNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perframeInfoIndex = PERFRAME_INFO_DIS;
        pipeInfo[disNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_LEADER;
        pipeInfo[disNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameVideoID = (m_nodeInfo[INDEX(PIPE_3AA)].secondaryNodeNum[disNodeType] - FIMC_IS_VIDEO_BAS_NUM);
    }

    if (m_supportMCSC == true) {
        /* MCSC */
        enum NODE_TYPE mcscNodeType = getNodeType(PIPE_MCSC);

        pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
        pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perframeInfoIndex = PERFRAME_INFO_MCSC;
        pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_LEADER;
        pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameVideoID = (m_nodeInfo[INDEX(PIPE_3AA)].secondaryNodeNum[mcscNodeType] - FIMC_IS_VIDEO_BAS_NUM);
    }

    /* SCP & MCSC0 */
    enum NODE_TYPE scpNodeType = getNodeType(PIPE_SCP);

    perFramePos = (m_cameraId == CAMERA_ID_BACK) ? PERFRAME_BACK_SCP_POS : PERFRAME_FRONT_SCP_POS;
    pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameNodeType = PERFRAME_NODE_TYPE_CAPTURE;
    pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameVideoID = (m_nodeInfo[INDEX(PIPE_3AA)].nodeNum[scpNodeType] - FIMC_IS_VIDEO_BAS_NUM);

    pipeInfo[scpNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
    pipeInfo[scpNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;

    stride = m_parameters->getHwPreviewStride();
    CLOGV2("stride=%d", stride);
    tempRect.fullW = hwPreviewW;
    tempRect.fullH = hwPreviewH;
    tempRect.colorFormat = previewFormat;
#ifdef USE_BUFFER_WITH_STRIDE
/* to use stride for preview buffer, set the bytesPerPlane */
    pipeInfo[scpNodeType].bytesPerPlane[0] = tempRect.fullW;
#endif

    pipeInfo[scpNodeType].rectInfo = tempRect;
    pipeInfo[scpNodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pipeInfo[scpNodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;

    if (m_parameters->increaseMaxBufferOfPreview() == true) {
        pipeInfo[scpNodeType].bufInfo.count = m_parameters->getPreviewBufferCount();
    } else {
        pipeInfo[scpNodeType].bufInfo.count = config->current->bufInfo.num_preview_buffers;
    }

    ret = m_pipes[INDEX(PIPE_3AA)]->setupPipe(pipeInfo, sensorIds, secondarySensorIds);
    if (ret < 0) {
        CLOGE2("3AA setupPipe fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    if (m_flagMcscVraOTF == false) {
        for (int i = 0; i < MAX_NODE; i++)
            pipeInfo[i] = nullPipeInfo;

        int vraWidth = 0, vraHeight = 0;
        m_parameters->getHwVraInputSize(&vraWidth, &vraHeight);

        /* VRA pipe */
        tempRect.fullW = vraWidth;
        tempRect.fullH = vraHeight;
        tempRect.colorFormat = m_parameters->getHwVraInputFormat();

        pipeInfo[0].rectInfo = tempRect;
        pipeInfo[0].bufInfo.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        pipeInfo[0].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
        pipeInfo[0].bufInfo.count = config->current->bufInfo.num_vra_buffers;
        /* per frame info */
        pipeInfo[0].perFrameNodeGroupInfo.perframeSupportNodeNum = CAPTURE_NODE_MAX;
        pipeInfo[0].perFrameNodeGroupInfo.perFrameLeaderInfo.perframeInfoIndex = PERFRAME_INFO_VRA;
        pipeInfo[0].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_LEADER;
        pipeInfo[0].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameVideoID = (FIMC_IS_VIDEO_VRA_NUM - FIMC_IS_VIDEO_BAS_NUM);

        ret = m_pipes[INDEX(PIPE_VRA)]->setupPipe(pipeInfo, m_sensorIds[INDEX(PIPE_VRA)]);
        if (ret < 0) {
            CLOGE2("VRA setupPipe fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    return NO_ERROR;
}

status_t ExynosCamera3FrameFactoryPreview::m_initPipesFastenAeStable(int32_t numFrames,
                                                                      int hwSensorW, int hwSensorH,
                                                                      int hwPreviewW, int hwPreviewH)
{
    status_t ret = NO_ERROR;

    /* TODO 1. setup pipes for 120FPS */
    camera_pipe_info_t pipeInfo[MAX_NODE];
    camera_pipe_info_t nullPipeInfo;

    int32_t nodeNums[MAX_NODE];
    int32_t sensorIds[MAX_NODE];
    int32_t secondarySensorIds[MAX_NODE];
    for (int i = 0; i < MAX_NODE; i++) {
        nodeNums[i] = -1;
        sensorIds[i] = -1;
        secondarySensorIds[i] = -1;
    }

    ExynosRect tempRect;
    int bayerFormat = CAMERA_BAYER_FORMAT;
    int previewFormat = m_parameters->getHwPreviewFormat();
    int hwVdisformat = m_parameters->getHWVdisFormat();
    struct ExynosConfigInfo *config = m_parameters->getConfig();
    ExynosRect bdsSize;
    uint32_t frameRate = 0;
    struct v4l2_streamparm streamParam;
    int perFramePos = 0;

#ifdef DEBUG_RAWDUMP
    if (m_parameters->checkBayerDumpEnable()) {
        bayerFormat = CAMERA_DUMP_BAYER_FORMAT;
    }
#endif

    m_parameters->getPreviewBdsSize(&bdsSize);

    /* 3AS */
    enum NODE_TYPE t3asNodeType = getNodeType(PIPE_3AA);

    for (int i = 0; i < MAX_NODE; i++)
        pipeInfo[i] = nullPipeInfo;

    for (int i = 0; i < MAX_NODE; i++) {
        ret = m_pipes[INDEX(PIPE_3AA)]->setPipeId((enum NODE_TYPE)i, m_nodeInfo[PIPE_3AA].pipeId[i]);
        if (ret < 0) {
            CLOGE2("setPipeId(%d, %d) fail, ret(%d)", i, m_nodeInfo[PIPE_3AA].pipeId[i], ret);
            return ret;
        }

        sensorIds[i] = m_sensorIds[INDEX(PIPE_3AA)][i];
        secondarySensorIds[i] = m_secondarySensorIds[INDEX(PIPE_3AA)][i];
    }


    tempRect.fullW = 32;
    tempRect.fullH = 64;
    tempRect.colorFormat = bayerFormat;

    pipeInfo[t3asNodeType].rectInfo = tempRect;
    pipeInfo[t3asNodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    pipeInfo[t3asNodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
    pipeInfo[t3asNodeType].bufInfo.count = numFrames;

    /* per frame info */
    pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = CAPTURE_NODE_MAX;
    pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perframeInfoIndex = PERFRAME_INFO_3AA;

    pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_LEADER;
    pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameVideoID = (m_nodeInfo[INDEX(PIPE_3AA)].nodeNum[t3asNodeType] - FIMC_IS_VIDEO_BAS_NUM);

    /* 3AC */
    if ((m_supportReprocessing == true && m_supportPureBayerReprocessing == false) || m_parameters->isUsing3acForIspc() == true) {
        enum NODE_TYPE t3acNodeType = getNodeType(PIPE_3AC);

        perFramePos = (m_cameraId == CAMERA_ID_BACK) ? PERFRAME_BACK_3AC_POS : PERFRAME_FRONT_3AC_POS;
        pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameNodeType = PERFRAME_NODE_TYPE_CAPTURE;
        pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameVideoID = (m_nodeInfo[INDEX(PIPE_3AA)].nodeNum[t3acNodeType] - FIMC_IS_VIDEO_BAS_NUM);

#ifdef FIXED_SENSOR_SIZE
        tempRect.fullW = maxSensorW;
        tempRect.fullH = maxSensorH;
#else
        tempRect.fullW = hwSensorW;
        tempRect.fullH = hwSensorH;
#endif
        if (m_parameters->isUsing3acForIspc() == true) {
            tempRect.colorFormat = SCC_OUTPUT_COLOR_FMT;
        } else {
            tempRect.colorFormat = bayerFormat;
        }

        pipeInfo[t3acNodeType].rectInfo = tempRect;
        pipeInfo[t3acNodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        pipeInfo[t3acNodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
        pipeInfo[t3acNodeType].bufInfo.count = numFrames;
        /* per frame info */
        pipeInfo[t3acNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
        pipeInfo[t3acNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;
    }

    /* 3AP */
    enum NODE_TYPE t3apNodeType = getNodeType(PIPE_3AP);

    perFramePos = (m_cameraId == CAMERA_ID_BACK) ? PERFRAME_BACK_3AP_POS : PERFRAME_FRONT_3AP_POS;
    pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameNodeType = PERFRAME_NODE_TYPE_CAPTURE;
    pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameVideoID = (m_nodeInfo[INDEX(PIPE_3AA)].secondaryNodeNum[t3apNodeType] - FIMC_IS_VIDEO_BAS_NUM);

    pipeInfo[t3apNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
    pipeInfo[t3apNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;

    /* ISP pipe */
    enum NODE_TYPE ispsNodeType = getNodeType(PIPE_ISP);

    pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
    pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perframeInfoIndex = PERFRAME_INFO_ISP;
    pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_LEADER;
    pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameVideoID = (m_nodeInfo[INDEX(PIPE_3AA)].secondaryNodeNum[ispsNodeType] - FIMC_IS_VIDEO_BAS_NUM);

    /* ISPP */
    enum NODE_TYPE isppNodeType = getNodeType(PIPE_ISPP);

    perFramePos = (m_cameraId == CAMERA_ID_BACK) ? PERFRAME_BACK_ISPP_POS : PERFRAME_FRONT_ISPP_POS;
    pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameNodeType = PERFRAME_NODE_TYPE_CAPTURE;
    pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameVideoID = (m_nodeInfo[PIPE_3AA].secondaryNodeNum[isppNodeType] - FIMC_IS_VIDEO_BAS_NUM);

    pipeInfo[isppNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
    pipeInfo[isppNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;

    if (m_parameters->getHWVdisMode()) {
        /* DIS */
        enum NODE_TYPE disNodeType = getNodeType(PIPE_DIS);

        /* per frame info */
        pipeInfo[disNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
        pipeInfo[disNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perframeInfoIndex = PERFRAME_INFO_DIS;
        pipeInfo[disNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_LEADER;
        pipeInfo[disNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameVideoID = (m_nodeInfo[PIPE_3AA].secondaryNodeNum[disNodeType] - FIMC_IS_VIDEO_BAS_NUM);
    }

    if (m_supportMCSC == true) {
        /* MCSC */
        enum NODE_TYPE mcscNodeType = getNodeType(PIPE_MCSC);

        pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
        pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perframeInfoIndex = PERFRAME_INFO_MCSC;
        pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_LEADER;
        pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameVideoID = (m_nodeInfo[INDEX(PIPE_3AA)].secondaryNodeNum[mcscNodeType] - FIMC_IS_VIDEO_BAS_NUM);
    }

    /* SCP & MCSC0 */
    enum NODE_TYPE scpNodeType = getNodeType(PIPE_SCP);

    perFramePos = (m_cameraId == CAMERA_ID_BACK) ? PERFRAME_BACK_SCP_POS : PERFRAME_FRONT_SCP_POS;

    pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameNodeType = PERFRAME_NODE_TYPE_CAPTURE;
    pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameVideoID = (m_nodeInfo[INDEX(PIPE_3AA)].nodeNum[scpNodeType] - FIMC_IS_VIDEO_BAS_NUM);

    pipeInfo[scpNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
    pipeInfo[scpNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;

    tempRect.fullW = hwPreviewW;
    tempRect.fullH = hwPreviewH;
    tempRect.colorFormat = previewFormat;

#ifdef CAMERA_PACKED_BAYER_ENABLE
#ifdef DEBUG_RAWDUMP
    if (m_parameters->checkBayerDumpEnable()) {
        /* packed bayer bytesPerPlane */
        pipeInfo[scpNodeType].bytesPerPlane[0] = ROUND_UP(tempRect.fullW * 2, 16);
    }
    else
#endif
    {
        /* packed bayer bytesPerPlane */
        pipeInfo[scpNodeType].bytesPerPlane[0] = ROUND_UP(tempRect.fullW * 3 / 2, 16);
    }
#endif

    pipeInfo[scpNodeType].rectInfo = tempRect;
    pipeInfo[scpNodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pipeInfo[scpNodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
    pipeInfo[scpNodeType].bufInfo.count = numFrames;

    ret = m_pipes[INDEX(PIPE_3AA)]->setupPipe(pipeInfo, sensorIds, secondarySensorIds);
    if (ret < 0) {
        CLOGE2("3AA setupPipe fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    if (m_flagMcscVraOTF == false) {
        for (int i = 0; i < MAX_NODE; i++)
            pipeInfo[i] = nullPipeInfo;

        int vraWidth = 0, vraHeight = 0;
        m_parameters->getHwVraInputSize(&vraWidth, &vraHeight);

        /* VRA pipe */
        tempRect.fullW = vraWidth;
        tempRect.fullH = vraHeight;
        tempRect.colorFormat = m_parameters->getHwVraInputFormat();

        pipeInfo[0].rectInfo = tempRect;
        pipeInfo[0].bufInfo.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        pipeInfo[0].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
        pipeInfo[0].bufInfo.count = numFrames;
        /* per frame info */
        pipeInfo[0].perFrameNodeGroupInfo.perframeSupportNodeNum = CAPTURE_NODE_MAX;
        pipeInfo[0].perFrameNodeGroupInfo.perFrameLeaderInfo.perframeInfoIndex = PERFRAME_INFO_VRA;
        pipeInfo[0].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_LEADER;
        pipeInfo[0].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameVideoID = (FIMC_IS_VIDEO_VRA_NUM - FIMC_IS_VIDEO_BAS_NUM);

        ret = m_pipes[INDEX(PIPE_VRA)]->setupPipe(pipeInfo, m_sensorIds[INDEX(PIPE_VRA)]);
        if (ret < 0) {
            CLOGE2("VRA setupPipe fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    return NO_ERROR;
}

ExynosCameraFrame *ExynosCamera3FrameFactoryPreview::createNewFrame(void)
{
    int ret = 0;
    ExynosCameraFrameEntity *newEntity[MAX_NUM_PIPES] = {0};
    ExynosCameraFrame *frame = m_frameMgr->createFrame(m_parameters, m_frameCount, FRAME_TYPE_PREVIEW);

    int requestEntityCount = 0;
    bool dzoomScaler = false;

    dzoomScaler = m_parameters->getZoomPreviewWIthScaler();

    ret = m_initFrameMetadata(frame);
    if (ret < 0)
        CLOGE2("frame(%d) metadata initialize fail", m_frameCount);

    if (m_flagFlite3aaOTF == true) {
        if (m_requestFLITE) {
            /* set flite pipe to linkageList */
            newEntity[INDEX(PIPE_FLITE)] = new ExynosCameraFrameEntity(PIPE_FLITE, ENTITY_TYPE_OUTPUT_ONLY, ENTITY_BUFFER_FIXED);
            frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_FLITE)]);
            requestEntityCount++;
        }

        /* set 3AA_ISP pipe to linkageList */
        newEntity[INDEX(PIPE_3AA)] = new ExynosCameraFrameEntity(PIPE_3AA, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_3AA)]);
        requestEntityCount++;

        if (m_requestDIS == true) {
            if (m_flag3aaIspOTF == true) {
                /* set DIS pipe to linkageList */
                newEntity[INDEX(PIPE_DIS)] = new ExynosCameraFrameEntity(PIPE_DIS, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_DELIVERY);
                frame->addChildEntity(newEntity[INDEX(PIPE_3AA)], newEntity[INDEX(PIPE_DIS)], INDEX(PIPE_ISPP));
                requestEntityCount++;
            } else {
                /* set ISP pipe to linkageList */
                newEntity[INDEX(PIPE_ISP)] = new ExynosCameraFrameEntity(PIPE_ISP, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
                frame->addChildEntity(newEntity[INDEX(PIPE_3AA)], newEntity[INDEX(PIPE_ISP)], INDEX(PIPE_3AP));
                requestEntityCount++;

                /* set DIS pipe to linkageList */
                newEntity[INDEX(PIPE_DIS)] = new ExynosCameraFrameEntity(PIPE_DIS, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_DELIVERY);
                frame->addChildEntity(newEntity[INDEX(PIPE_ISP)], newEntity[INDEX(PIPE_DIS)], INDEX(PIPE_ISPP));
                requestEntityCount++;
            }
        } else {
            if (m_flag3aaIspOTF == true) {
                /* skip ISP pipe to linkageList */
            } else {
                /* set ISP pipe to linkageList */
                newEntity[INDEX(PIPE_ISP)] = new ExynosCameraFrameEntity(PIPE_ISP, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
                frame->addChildEntity(newEntity[INDEX(PIPE_3AA)], newEntity[INDEX(PIPE_ISP)], INDEX(PIPE_3AP));
                requestEntityCount++;
            }
        }
    } else {
        /* set flite pipe to linkageList */
        newEntity[INDEX(PIPE_FLITE)] = new ExynosCameraFrameEntity(PIPE_FLITE, ENTITY_TYPE_OUTPUT_ONLY, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_FLITE)]);

        /* set 3AA pipe to linkageList */
        newEntity[INDEX(PIPE_3AA)] = new ExynosCameraFrameEntity(PIPE_3AA, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
        frame->addChildEntity(newEntity[INDEX(PIPE_FLITE)], newEntity[INDEX(PIPE_3AA)]);

        /* set ISP pipe to linkageList */
        if (m_requestISP == true) {
            newEntity[INDEX(PIPE_ISP)] = new ExynosCameraFrameEntity(PIPE_ISP, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
            frame->addChildEntity(newEntity[INDEX(PIPE_3AA)], newEntity[INDEX(PIPE_ISP)]);
        }

        /* set DIS pipe to linkageList */
        if (m_requestDIS == true) {
            newEntity[INDEX(PIPE_DIS)] = new ExynosCameraFrameEntity(PIPE_DIS, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_DELIVERY);
            frame->addChildEntity(newEntity[INDEX(PIPE_ISP)], newEntity[INDEX(PIPE_DIS)]);
        }

        /* flite, 3aa, isp, dis as one. */
        requestEntityCount++;
    }

    if (m_supportReprocessing == false) {
        /* set GSC-Picture pipe to linkageList */
        newEntity[INDEX(PIPE_GSC_PICTURE)] = new ExynosCameraFrameEntity(PIPE_GSC_PICTURE, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_GSC_PICTURE)]);
    }

    /* set GSC pipe to linkageList */
    newEntity[INDEX(PIPE_GSC)] = new ExynosCameraFrameEntity(PIPE_GSC, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_GSC)]);

    if (dzoomScaler) {
        requestEntityCount++;
    }

    newEntity[INDEX(PIPE_GSC_VIDEO)] = new ExynosCameraFrameEntity(PIPE_GSC_VIDEO, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_GSC_VIDEO)]);

    /* PIPE_VRA's internal pipe entity */
    newEntity[INDEX(PIPE_GSC_VRA)] = new ExynosCameraFrameEntity(PIPE_GSC_VRA, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_GSC_VRA)]);

    if (m_supportReprocessing == false) {
        /* set JPEG pipe to linkageList */
        newEntity[INDEX(PIPE_JPEG)] = new ExynosCameraFrameEntity(PIPE_JPEG, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_JPEG)]);
    }

    ret = m_initPipelines(frame);
    if (ret < 0) {
        CLOGE2("m_initPipelines fail, ret(%d)", ret);
    }

    /* TODO: make it dynamic */
    frame->setNumRequestPipe(requestEntityCount);

    m_fillNodeGroupInfo(frame);

    m_frameCount++;

    return frame;
}

enum NODE_TYPE ExynosCamera3FrameFactoryPreview::getNodeType(uint32_t pipeId)
{
    enum NODE_TYPE nodeType = INVALID_NODE;

    switch (pipeId) {
    case PIPE_FLITE:
        nodeType = CAPTURE_NODE_1;
        break;
    case PIPE_3AA:
        nodeType = OUTPUT_NODE;
        break;
    case PIPE_3AC:
        nodeType = CAPTURE_NODE_1;
        break;
    case PIPE_3AP:
        nodeType = OTF_NODE_1;
        break;
    case PIPE_ISP:
        nodeType = OTF_NODE_2;
        break;
    case PIPE_ISPP:
        nodeType = OTF_NODE_3;
        break;
    case PIPE_DIS:
        nodeType = OTF_NODE_4;
        break;
    case PIPE_MCSC:
        nodeType = OTF_NODE_5;
        break;
    case PIPE_ISPC:
    case PIPE_SCC:
    case PIPE_JPEG:
        nodeType = CAPTURE_NODE_6;
        break;
    case PIPE_SCP:
        nodeType = CAPTURE_NODE_7;
        break;
    case PIPE_VRA:
        nodeType = OUTPUT_NODE;
        break;
    default:
        android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):Unexpected pipe_id(%d), assert!!!!",
            __FUNCTION__, __LINE__, pipeId);
        break;
    }

    return nodeType;
}

}; /* namespace android */
