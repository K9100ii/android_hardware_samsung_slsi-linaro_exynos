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

#include "SecCameraSWVdis_3_0.h"

namespace android {

#ifdef SUPPORT_SW_VDIS
bool m_swVDIS_Mode;

UniPluginBufferData_t m_swVDIS_pluginData;
UniPluginCameraInfo_t m_swVDIS_pluginCameraInfo;
UniPluginFaceInfo_t m_swVDIS_pluginFaceInfo;
UniPluginFPS_t m_swVDIS_Fps;
unsigned int m_swVDIS_pluginFaceNum;
void *m_VDISpluginHandle;

ExynosCameraBuffer m_swVDIS_OutBuf[NUM_VDIS_OUT_BUFFERS_MAX];
int m_swVDIS_OutIndex[NUM_VDIS_OUT_BUFFERS_MAX];
int m_swVDIS_TSIndex, m_swVDIS_TSIndexCount;
int m_sw_VDIS_OutBufferNum, m_swVDIS_OutDelay;
long long int m_swVDIS_timeStamp[NUM_VDIS_BUFFERS];
int m_swVDIS_OutW, m_swVDIS_OutH, m_swVDIS_InW, m_swVDIS_InH;
int m_swVDIS_CameraID;
int m_swVDIS_SensorType;
int m_swVDIS_APType;
int m_swVDIS_FrameNum, m_swVDIS_Delay;
int m_swVDIS_InQIndex, m_swVDIS_OutQIndex;
struct camera2_dm *m_swVDIS_fd_dm;
swVDIS_FaceData_t *m_swVDIS_FaceData;
#ifdef SAMSUNG_OIS_VDIS
UNI_PLUGIN_OIS_MODE m_swVDIS_OISMode;
#endif
uint32_t m_swVDIS_MinFps, m_swVDIS_MaxFps;
UTrect m_swVDIS_RectInfo;

UniPluginBufferData_t m_swVDIS_pluginData_tstamp;
static swVDIS_Preview_t m_swVDIS_Preview;
static swVDIS_Offset_t m_swVDIS_Offset[NUM_VDIS_PREVIEW_INFO_BUFFERS];

status_t m_swVDIS_AdjustPreviewSize(int *Width, int *Height)
{
    if(*Width == 2304 && *Height == 1296) {
        *Width = 1920;
        *Height = 1080;
    }
    else if(*Width == 1536 && *Height == 864) {
        *Width = 1280;
        *Height = 720;
    }
    else
    {
        *Width = (*Width * 5) / 6;
        *Height = (*Height * 5) / 6;
    }

    return NO_ERROR;
}

status_t m_swVDIS_init()
{
    int swVDIS_CameraID;

    memset(m_swVDIS_OutBuf, 0x00, sizeof(struct ExynosCameraBuffer) * NUM_VDIS_OUT_BUFFERS_MAX);
    memset(m_swVDIS_OutIndex, 0x00, sizeof(int) * NUM_VDIS_OUT_BUFFERS_MAX);
    memset(m_swVDIS_timeStamp, 0x00, sizeof(long long int) * NUM_VDIS_BUFFERS);
    memset(m_swVDIS_Offset, 0x0, sizeof(swVDIS_Offset_t) * NUM_VDIS_PREVIEW_INFO_BUFFERS);
    memset(&m_swVDIS_Preview, 0x0, sizeof(swVDIS_Preview_t));

    m_swVDIS_TSIndex = 0;
    m_swVDIS_TSIndexCount = 0;
    m_swVDIS_Mode = true;
    m_swVDIS_Delay = 21;
    m_swVDIS_FrameNum = 0;

    m_swVDIS_RectInfo.left = 0;
    m_swVDIS_RectInfo.top = 0;
    m_swVDIS_pluginData_tstamp.Timestamp = 0;

    m_swVDIS_OutW = m_swVDIS_InW;
    m_swVDIS_OutH = m_swVDIS_InH;

    m_swVDIS_AdjustPreviewSize(&m_swVDIS_OutW, &m_swVDIS_OutH);

    m_VDISpluginHandle = uni_plugin_load(VDIS_PLUGIN_NAME);
    memset(&m_swVDIS_pluginData, 0, sizeof(UniPluginBufferData_t));
    m_swVDIS_pluginData.InWidth = m_swVDIS_InW;
    m_swVDIS_pluginData.InHeight = m_swVDIS_InH;
    m_swVDIS_pluginData.OutWidth = m_swVDIS_OutW;
    m_swVDIS_pluginData.OutHeight = m_swVDIS_OutH;
    uni_plugin_set(m_VDISpluginHandle, VDIS_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &m_swVDIS_pluginData);

    memset(&m_swVDIS_pluginCameraInfo, 0, sizeof(UniPluginCameraInfo_t));

    m_swVDIS_pluginCameraInfo.CameraType = (UNI_PLUGIN_CAMERA_TYPE)m_swVDIS_CameraID;
    m_swVDIS_pluginCameraInfo.SensorType = (UNI_PLUGIN_SENSOR_TYPE)m_swVDIS_SensorType;
    m_swVDIS_pluginCameraInfo.APType = (UNI_PLUGIN_AP_TYPE)m_swVDIS_APType;

    memset(&m_swVDIS_Fps, 0, sizeof(UniPluginFPS_t));
    if (m_swVDIS_MaxFps >= 60 && m_swVDIS_MinFps >= 60) {
        m_swVDIS_Fps.maxFPS.num = 60;
        m_swVDIS_Fps.maxFPS.den = 1;
        m_sw_VDIS_OutBufferNum = FPS60_NUM_RECORDING_BUFFERS;
    } else {
        m_swVDIS_Fps.maxFPS.num = 30;
        m_swVDIS_Fps.maxFPS.den = 1;
#ifdef NUM_RECORDING_WITH_VDIS_BUFFERS
        m_sw_VDIS_OutBufferNum = NUM_RECORDING_WITH_VDIS_BUFFERS;
#else
        m_sw_VDIS_OutBufferNum = NUM_RECORDING_BUFFERS;
#endif
    }

    m_swVDIS_OutDelay = (m_swVDIS_Delay % m_sw_VDIS_OutBufferNum);

    uni_plugin_set(m_VDISpluginHandle, VDIS_PLUGIN_NAME, UNI_PLUGIN_INDEX_FPS_INFO, &m_swVDIS_Fps);

    uni_plugin_set(m_VDISpluginHandle, VDIS_PLUGIN_NAME, UNI_PLUGIN_INDEX_CAMERA_INFO, &m_swVDIS_pluginCameraInfo);
#ifdef SAMSUNG_OIS_VDIS
    uni_plugin_set(m_VDISpluginHandle, VDIS_PLUGIN_NAME, UNI_PLUGIN_INDEX_OIS_MODE, &m_swVDIS_OISMode);
#endif
    uni_plugin_init(m_VDISpluginHandle);

    VDIS_LOG("VDIS_HAL_INIT: VS_INIT_DONE: Input: %d x %d  Output: %d x %d Delay: %d OutDelay: %d NUM_InBuffer: %d NUM_OutBuffer: %d",
        m_swVDIS_InW, m_swVDIS_InH, m_swVDIS_OutW, m_swVDIS_OutH, m_swVDIS_Delay, m_swVDIS_OutDelay, NUM_VDIS_BUFFERS, m_sw_VDIS_OutBufferNum);

    return NO_ERROR;
}

status_t m_swVDIS_process(ExynosCameraBuffer *swVDIS_InBuf, int swVDIS_IndexCount,
                          nsecs_t swVDIS_timeStamp, nsecs_t swVDIS_timeStampBoot, int swVDIS_Lux, int swVDIS_ZoomLevel,
                          float swVDIS_ExposureValue, swVDIS_FaceData_t *FaceData)
{
    m_swVDIS_OutIndex[swVDIS_IndexCount] = m_swVDIS_OutBuf[swVDIS_IndexCount].index;
    m_swVDIS_timeStamp[m_swVDIS_TSIndexCount] = swVDIS_timeStamp;

    m_swVDIS_pluginFaceNum = 0;
    memset(&m_swVDIS_pluginFaceInfo, 0, sizeof(UniPluginFaceInfo_t));

    for(int i = 0; i < FaceData->FaceNum; i++) {
        m_swVDIS_pluginFaceInfo.index = i;
        m_swVDIS_pluginFaceInfo.FaceROI.left = FaceData->FaceRect[i].left;
        m_swVDIS_pluginFaceInfo.FaceROI.top = FaceData->FaceRect[i].top;
        m_swVDIS_pluginFaceInfo.FaceROI.right = FaceData->FaceRect[i].right;
        m_swVDIS_pluginFaceInfo.FaceROI.bottom = FaceData->FaceRect[i].bottom;
        m_swVDIS_pluginFaceNum = FaceData->FaceNum;

        uni_plugin_set(m_VDISpluginHandle, VDIS_PLUGIN_NAME, UNI_PLUGIN_INDEX_FACE_INFO, &m_swVDIS_pluginFaceInfo);
        uni_plugin_set(m_VDISpluginHandle, VDIS_PLUGIN_NAME, UNI_PLUGIN_INDEX_FACE_NUM, &m_swVDIS_pluginFaceNum);
    }

    if(FaceData->FaceNum == 0) {
        uni_plugin_set(m_VDISpluginHandle, VDIS_PLUGIN_NAME, UNI_PLUGIN_INDEX_FACE_INFO, &m_swVDIS_pluginFaceInfo);
        uni_plugin_set(m_VDISpluginHandle, VDIS_PLUGIN_NAME, UNI_PLUGIN_INDEX_FACE_NUM, &m_swVDIS_pluginFaceNum);
    }

    if (swVDIS_Lux > 20)
        swVDIS_Lux = -1*(16777216- swVDIS_Lux);     //2^24 = 16777216

    m_swVDIS_pluginData.InBuffY = (char *)swVDIS_InBuf->addr[0];
    m_swVDIS_pluginData.InBuffU = (char *)swVDIS_InBuf->addr[1];
    m_swVDIS_pluginData.OutBuffY = (char *)m_swVDIS_OutBuf[swVDIS_IndexCount].addr[0];
    m_swVDIS_pluginData.OutBuffU = (char *)m_swVDIS_OutBuf[swVDIS_IndexCount].addr[1];

    uni_plugin_set(m_VDISpluginHandle, VDIS_PLUGIN_NAME, UNI_PLUGIN_INDEX_EXPOSURE_VALUE, &swVDIS_ExposureValue);

    m_swVDIS_pluginData.Timestamp = swVDIS_timeStampBoot;
    m_swVDIS_pluginData.BrightnessLux = swVDIS_Lux;
    m_swVDIS_pluginData.ZoomLevel = swVDIS_ZoomLevel;

    uni_plugin_set(m_VDISpluginHandle, VDIS_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &m_swVDIS_pluginData);

    uni_plugin_process(m_VDISpluginHandle);

    if (swVDIS_IndexCount > m_swVDIS_OutDelay - 1) {
        m_swVDIS_OutQIndex = m_swVDIS_OutIndex[swVDIS_IndexCount - m_swVDIS_OutDelay];
    } else {
        m_swVDIS_OutQIndex = m_swVDIS_OutIndex[swVDIS_IndexCount + m_sw_VDIS_OutBufferNum - m_swVDIS_OutDelay];
    }

    if (m_swVDIS_TSIndexCount > m_swVDIS_Delay - 1) {
        m_swVDIS_TSIndex = m_swVDIS_TSIndexCount - m_swVDIS_Delay;
    } else {
        m_swVDIS_TSIndex = m_swVDIS_TSIndexCount + NUM_VDIS_BUFFERS - m_swVDIS_Delay;
    }

    m_swVDIS_InQIndex = swVDIS_InBuf->index;
    m_swVDIS_FrameNum++;

    return NO_ERROR;
}

status_t m_swVDIS_StoreOffset()
{
    if (m_VDISpluginHandle == NULL) {
        ALOGD("Plugin Handle is NULL");
        return -1;
    }

    uni_plugin_get(m_VDISpluginHandle, VDIS_PLUGIN_NAME, UNI_PLUGIN_INDEX_CROP_INFO, &m_swVDIS_RectInfo);
    uni_plugin_get(m_VDISpluginHandle, VDIS_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &m_swVDIS_pluginData_tstamp);

    if (m_swVDIS_pluginData_tstamp.Timestamp == 0) {
        return NO_ERROR;
    }

    int store_cnt = m_swVDIS_Preview.store_offset_cnt;
    m_swVDIS_Offset[store_cnt].left = m_swVDIS_RectInfo.left;
    m_swVDIS_Offset[store_cnt].top = m_swVDIS_RectInfo.top;
    m_swVDIS_Offset[store_cnt].timeStamp = m_swVDIS_pluginData_tstamp.Timestamp;

    m_swVDIS_Preview.store_offset_cnt = (m_swVDIS_Preview.store_offset_cnt + 1) % NUM_VDIS_PREVIEW_INFO_BUFFERS;

    return NO_ERROR;
}

static void m_swVDIS_SetCrop(int **offset_x, int **offset_y)
{
    int offset_left = m_swVDIS_Offset[m_swVDIS_Preview.get_offset_cnt].left;
    int offset_top = m_swVDIS_Offset[m_swVDIS_Preview.get_offset_cnt].top;

    **offset_x = offset_left;
    **offset_y = offset_top;
    m_swVDIS_Preview.prev_left = offset_left;
    m_swVDIS_Preview.prev_top = offset_top;

    m_swVDIS_Preview.get_offset_cnt = (m_swVDIS_Preview.get_offset_cnt + 1) % NUM_VDIS_PREVIEW_INFO_BUFFERS;
}

status_t m_swVDIS_GetOffset(int *offset_x, int *offset_y, long long int displayTimeStamp)
{
    long long int offsetTimeStamp = m_swVDIS_Offset[m_swVDIS_Preview.get_offset_cnt].timeStamp;

    if (displayTimeStamp == offsetTimeStamp) {
        m_swVDIS_SetCrop(&offset_x, &offset_y);
        return NO_ERROR;
    } else {
        if (offsetTimeStamp != 0) {
            if (offsetTimeStamp < displayTimeStamp) {
                /* check stored frame */
                for(int i = 0; i < NUM_VDIS_PREVIEW_INFO_BUFFERS; i++) {
                    long long int prev_offsetTimeStamp = offsetTimeStamp;

                    m_swVDIS_Preview.get_offset_cnt = (m_swVDIS_Preview.get_offset_cnt + 1) % NUM_VDIS_PREVIEW_INFO_BUFFERS;
                    offsetTimeStamp = m_swVDIS_Offset[m_swVDIS_Preview.get_offset_cnt].timeStamp;

                    if (offsetTimeStamp < prev_offsetTimeStamp) {
                        break;
                    }

                    if (offsetTimeStamp == displayTimeStamp) {
                        m_swVDIS_SetCrop(&offset_x, &offset_y);
                        return NO_ERROR;
                    } else if (offsetTimeStamp > displayTimeStamp) {
                        break;
                    }
                }
            }

            ALOGD("VDIS_HAL: use previous offset");
            *offset_x = m_swVDIS_Preview.prev_left;
            *offset_y = m_swVDIS_Preview.prev_top;
        }
    }

    return NO_ERROR;
}

#ifdef SAMSUNG_OIS_VDIS
UNI_PLUGIN_OIS_MODE m_swVDIS_getOISmode()
{
    uni_plugin_get(m_VDISpluginHandle, VDIS_PLUGIN_NAME, UNI_PLUGIN_INDEX_OIS_MODE, &m_swVDIS_OISMode);
    return m_swVDIS_OISMode;
}
#endif

status_t m_swVDIS_deinit()
{
    int ret = 0;

    if(m_VDISpluginHandle != NULL) {
        uni_plugin_deinit(m_VDISpluginHandle);
        ret = uni_plugin_unload(&m_VDISpluginHandle);
        if(ret < 0) {
            ALOGE("[VDIS](%s[%d]):VDIS plugin unload failed!!", __FUNCTION__, __LINE__);
        }
        m_VDISpluginHandle = NULL;
    }

    m_swVDIS_Mode = false;
    m_swVDIS_FrameNum = 0;

    VDIS_LOG("VDIS_HAL: VS_DEINIT_DONE");

    return NO_ERROR;
}

#endif /*SUPPORT_SW_VDIS*/

}; /* namespace android */
