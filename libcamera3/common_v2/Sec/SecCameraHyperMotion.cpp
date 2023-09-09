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

#include "SecCameraHyperMotion.h"

namespace android {

#ifdef SAMSUNG_HYPER_MOTION
bool m_hyperMotion_Mode;
UniPluginBufferData_t m_hyperMotion_pluginData;
UniPluginCameraInfo_t m_hyperMotion_pluginCameraInfo;
UniPluginExtraBufferInfo_t m_hyperMotion_pluginExtraBufferInfo;
UniPluginFaceInfo_t m_swHM_pluginFaceInfo[ABLE_FACECNT];
void *m_hyperMotionPluginHandle;

ExynosCameraBuffer m_hyperMotion_OutBuf[NUM_HYPERMOTION_BUFFERS];
int m_hyperMotion_InIndex[NUM_HYPERMOTION_BUFFERS];
int m_hyperMotion_OutIndex[NUM_HYPERMOTION_BUFFERS];
long long int m_hyperMotion_timeStamp[NUM_HYPERMOTION_BUFFERS];
int m_hyperMotion_OutW, m_hyperMotion_OutH, m_hyperMotion_InW, m_hyperMotion_InH;
int m_hyperMotion_CameraID;
int m_hyperMotion_SensorType;
int m_hyperMotion_APType;
int m_hyperMotion_FrameNum, m_hyperMotion_Delay;
int m_hyperMotion_InQIndex, m_hyperMotion_OutQIndex;
unsigned int m_swHM_pluginFaceNum;
char m_hyperMotion_solutionName[40];
bool m_isHyperMotionOn;
bool m_canSendFrame;
UNI_PLUGIN_OPERATION_MODE m_hyperMotionPlaySpeed;
nsecs_t m_hyperMotionTimeStamp;
nsecs_t m_hyperMotionTimeBTWFrames;
struct camera2_dm *m_swHM_fd_dm;
swHM_FaceData_t *m_swHM_FaceData;

status_t m_hyperMotion_AdjustPreviewSize(int *Width, int *Height)
{
    if (*Width == 2304 && *Height == 1296) {
        *Width = 1920;
        *Height = 1080;
    } else if (*Width == 1536 && *Height == 864) {
        *Width = 1280;
        *Height = 720;
    } else {
        *Width = (*Width * 5) / 6;
        *Height = (*Height * 5) / 6;
    }

    return NO_ERROR;
}

status_t m_hyperMotion_init()
{
    int hyperMotion_CameraID;

    memset(m_hyperMotion_OutBuf, 0x00, sizeof(struct ExynosCameraBuffer) * NUM_HYPERMOTION_BUFFERS);
    memset(m_hyperMotion_InIndex, 0x00, sizeof(int) * NUM_HYPERMOTION_BUFFERS);
    memset(m_hyperMotion_OutIndex, 0x00, sizeof(int) * NUM_HYPERMOTION_BUFFERS);
    memset(m_hyperMotion_timeStamp, 0x00, sizeof(long long int) * NUM_HYPERMOTION_BUFFERS);
    memset(m_hyperMotion_solutionName, 0x00, sizeof(m_hyperMotion_solutionName));

    m_hyperMotion_Mode = true;
    m_hyperMotion_Delay = 15;
    m_hyperMotion_FrameNum = 0;

    m_hyperMotion_OutW = m_hyperMotion_InW;
    m_hyperMotion_OutH = m_hyperMotion_InH;

    m_hyperMotion_AdjustPreviewSize(&m_hyperMotion_OutW, &m_hyperMotion_OutH);

    strncpy(m_hyperMotion_solutionName, HYPER_MOTION_PLUGIN_NAME, sizeof(HYPER_MOTION_PLUGIN_NAME));

    m_hyperMotionPluginHandle = uni_plugin_load(m_hyperMotion_solutionName);

    memset(&m_hyperMotion_pluginData, 0, sizeof(UniPluginBufferData_t));
    m_hyperMotion_pluginData.inWidth = m_hyperMotion_InW;
    m_hyperMotion_pluginData.inHeight = m_hyperMotion_InH;
    m_hyperMotion_pluginData.outWidth = m_hyperMotion_OutW;
    m_hyperMotion_pluginData.outHeight = m_hyperMotion_OutH;
    uni_plugin_set(m_hyperMotionPluginHandle, m_hyperMotion_solutionName, UNI_PLUGIN_INDEX_BUFFER_INFO, &m_hyperMotion_pluginData);

    memset(&m_hyperMotion_pluginExtraBufferInfo, 0, sizeof(UniPluginExtraBufferInfo_t));
    memset(&m_hyperMotion_pluginCameraInfo, 0, sizeof(UniPluginCameraInfo_t));
    m_hyperMotion_pluginCameraInfo.cameraType = (UNI_PLUGIN_CAMERA_TYPE)m_hyperMotion_CameraID;
    m_hyperMotion_pluginCameraInfo.sensorType = (UNI_PLUGIN_SENSOR_TYPE)m_hyperMotion_SensorType;
    m_hyperMotion_pluginCameraInfo.APType = (UNI_PLUGIN_AP_TYPE)m_hyperMotion_APType;

    uni_plugin_set(m_hyperMotionPluginHandle, m_hyperMotion_solutionName, UNI_PLUGIN_INDEX_CAMERA_INFO, &m_hyperMotion_pluginCameraInfo);

    m_canSendFrame = false;
    m_hyperMotionTimeStamp = 0;

    UniPluginFPS_t fps;
    uni_plugin_get(m_hyperMotionPluginHandle, m_hyperMotion_solutionName, UNI_PLUGIN_INDEX_FPS_INFO, &fps);
    if (fps.maxFPS.num && fps.maxFPS.den) {
        m_hyperMotionTimeBTWFrames = (1000000000 * fps.maxFPS.den) / fps.maxFPS.num;
        ALOGD("[HyperMotion] m_hyperMotionTimeBTWFrames is %lld", m_hyperMotionTimeBTWFrames);
    } else {
        ALOGE("[HyperMotion] fps getting is 0!! default set it to 30");
        m_hyperMotionTimeBTWFrames = 33333333;
    }

    uni_plugin_set(m_hyperMotionPluginHandle, m_hyperMotion_solutionName, UNI_PLUGIN_INDEX_OPERATION_MODE, &m_hyperMotionPlaySpeed);

    uni_plugin_init(m_hyperMotionPluginHandle);

    ALOGD("HyperMotion_HAL_INIT: HyperMotion_INIT_DONE: Input: %d x %d  Output: %d x %d Delay: %d NUM_Buffer: %d",
        m_hyperMotion_InW, m_hyperMotion_InH, m_hyperMotion_OutW, m_hyperMotion_OutH, m_hyperMotion_Delay, NUM_HYPERMOTION_BUFFERS);
    return NO_ERROR;
}

status_t m_hyperMotion_process(ExynosCameraBuffer *hyperMotion_InBuf, int hyperMotion_IndexCount,
                          nsecs_t hyperMotion_timeStamp, nsecs_t hyperMotion_timeStampBoot, int hyperMotion_Lux, int hyperMotion_ZoomLevel,
                          float hyperMotion_ExposureValue, int hyperMotion_ExposureTime, swHM_FaceData_t *FaceData, int orientation)
{
    //faceData input
    m_swHM_pluginFaceNum = 0;
    memset(&m_swHM_pluginFaceInfo, 0, sizeof(UniPluginFaceInfo_t) * ABLE_FACECNT);

    for(int i = 0; i < FaceData->FaceNum; i++) {
        m_swHM_pluginFaceInfo[i].index = i;
        m_swHM_pluginFaceInfo[i].faceROI.left = FaceData->FaceRect[i].left;
        m_swHM_pluginFaceInfo[i].faceROI.top = FaceData->FaceRect[i].top;
        m_swHM_pluginFaceInfo[i].faceROI.right = FaceData->FaceRect[i].right;
        m_swHM_pluginFaceInfo[i].faceROI.bottom = FaceData->FaceRect[i].bottom;
    }

    m_swHM_pluginFaceNum = FaceData->FaceNum;

    uni_plugin_set(m_hyperMotionPluginHandle, m_hyperMotion_solutionName, UNI_PLUGIN_INDEX_FACE_INFO, &m_swHM_pluginFaceInfo);
    uni_plugin_set(m_hyperMotionPluginHandle, m_hyperMotion_solutionName, UNI_PLUGIN_INDEX_FACE_NUM, &m_swHM_pluginFaceNum);

    m_hyperMotion_InIndex[hyperMotion_IndexCount] = hyperMotion_InBuf->index;
    m_hyperMotion_OutIndex[hyperMotion_IndexCount] = m_hyperMotion_OutBuf[hyperMotion_IndexCount].index;
    m_hyperMotion_timeStamp[hyperMotion_IndexCount] = hyperMotion_timeStamp;

    if (m_hyperMotionTimeStamp == 0)
        m_hyperMotionTimeStamp = hyperMotion_timeStamp;

    if (hyperMotion_Lux > 20)
        hyperMotion_Lux = -1*(16777216- hyperMotion_Lux);     //2^24 = 16777216

    m_hyperMotion_pluginData.inBuffY = (char *)hyperMotion_InBuf->addr[0];
    m_hyperMotion_pluginData.inBuffU = (char *)hyperMotion_InBuf->addr[1];
    m_hyperMotion_pluginData.outBuffY = (char *)m_hyperMotion_OutBuf[hyperMotion_IndexCount].addr[0];
    m_hyperMotion_pluginData.outBuffU = (char *)m_hyperMotion_OutBuf[hyperMotion_IndexCount].addr[1];

    uni_plugin_set(m_hyperMotionPluginHandle, m_hyperMotion_solutionName, UNI_PLUGIN_INDEX_EXPOSURE_VALUE, &hyperMotion_ExposureValue);
    //set to exposure time
    m_hyperMotion_pluginExtraBufferInfo.exposureTime.snum = hyperMotion_ExposureTime;
    m_hyperMotion_pluginExtraBufferInfo.orientation = (UNI_PLUGIN_DEVICE_ORIENTATION)(orientation);
    m_hyperMotion_pluginExtraBufferInfo.zoomRatio = m_configurations->getZoomRatio();
    m_hyperMotion_pluginExtraBufferInfo.brightnessValue.num = hyperMotion_Lux;
    uni_plugin_set(m_hyperMotionPluginHandle, m_hyperMotion_solutionName, UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO, &m_hyperMotion_pluginExtraBufferInfo);

    m_hyperMotion_pluginData.timestamp = hyperMotion_timeStampBoot;
    uni_plugin_set(m_hyperMotionPluginHandle, m_hyperMotion_solutionName, UNI_PLUGIN_INDEX_BUFFER_INFO, &m_hyperMotion_pluginData);

    uni_plugin_process(m_hyperMotionPluginHandle);

    if (hyperMotion_IndexCount > m_hyperMotion_Delay - 1) {
        m_hyperMotion_OutQIndex = m_hyperMotion_OutIndex[hyperMotion_IndexCount - m_hyperMotion_Delay];
    } else {
        m_hyperMotion_OutQIndex = m_hyperMotion_OutIndex[hyperMotion_IndexCount + NUM_HYPERMOTION_BUFFERS - m_hyperMotion_Delay];
    }

    UTpoll hyperMotion_FrameToEncodeStatus = UTprocessing;
    uni_plugin_set(m_hyperMotionPluginHandle, m_hyperMotion_solutionName, UNI_PLUGIN_INDEX_BUFFER_INDEX, &m_hyperMotion_OutQIndex);
    uni_plugin_get(m_hyperMotionPluginHandle, m_hyperMotion_solutionName, UNI_PLUGIN_INDEX_POLLING, &hyperMotion_FrameToEncodeStatus );
    if (hyperMotion_FrameToEncodeStatus == UTsuccess) {
        m_canSendFrame = true;
        m_hyperMotionTimeStamp += m_hyperMotionTimeBTWFrames;
    } else {
        m_canSendFrame = false;
    }

    m_hyperMotion_InQIndex = hyperMotion_InBuf->index;
    m_hyperMotion_FrameNum++;

    return NO_ERROR;
}

status_t m_hyperMotion_deinit()
{
    int ret = 0;

    if (m_hyperMotionPluginHandle != NULL) {
        uni_plugin_deinit(m_hyperMotionPluginHandle);
        ret = uni_plugin_unload(&m_hyperMotionPluginHandle);
        if (ret < 0) {
            ALOGE("[HyperMotion](%s[%d]):%s plugin unload failed!!",
                __FUNCTION__, __LINE__, m_hyperMotion_solutionName);
        }
        m_hyperMotionPluginHandle = NULL;
    }

    m_isHyperMotionOn = false;

    m_hyperMotion_Mode = false;
    m_hyperMotion_FrameNum = 0;

    ALOGD("HyperMotion_HAL: HyperMotion_DEINIT_DONE");
    return NO_ERROR;
}

void m_hyperMotionModeSet(bool mode)
{
    m_isHyperMotionOn = mode;
    return;
}

void m_hyperMotionPlaySpeedSet(UNI_PLUGIN_OPERATION_MODE playspeed)
{
    m_hyperMotionPlaySpeed = playspeed;
    return;
}

bool m_hyperMotionModeGet()
{
    return m_isHyperMotionOn;
}

bool m_hyperMotionCanSendFrame()
{
    return m_canSendFrame;
}

nsecs_t m_hyperMotionTimeStampGet()
{
    return m_hyperMotionTimeStamp;
}
#endif /*SAMSUNG_HYPER_MOTION*/
}; /* namespace android */
