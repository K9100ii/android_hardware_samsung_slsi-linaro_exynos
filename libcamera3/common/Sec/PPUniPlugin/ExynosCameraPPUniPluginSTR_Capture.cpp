/*
 * Copyright@ Samsung Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

/*#define LOG_NDEBUG 0 */
#define LOG_TAG "ExynosCameraPPUniPluginSTR_Capture"

#include "ExynosCameraPPUniPluginSTR_Capture.h"

ExynosCameraPPUniPluginSTR_Capture::~ExynosCameraPPUniPluginSTR_Capture()
{
}

status_t ExynosCameraPPUniPluginSTR_Capture::m_draw(ExynosCameraImage *srcImage,
                                           ExynosCameraImage *dstImage)
{
    status_t ret = NO_ERROR;
    UniPluginBufferData_t bufferInfo;
    UniPluginExtraBufferInfo_t extraInfo;
    UniPluginFaceInfo_t faceInfo[NUM_OF_DETECTED_FACES];
    uint32_t faceNum = 0;
    uint32_t hdrData = 0;
    uint32_t flashData = 0;
    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;
    camera2_shot_ext *shot_ext = NULL;
    char *Y = NULL;
    char *UV = NULL;

    Mutex::Autolock l(m_uniPluginLock);

    if (m_UniPluginHandle == NULL) {
        CLOGE("[STR_CAPTURE] STR_CAPTURE Uni plugin(%s) is NULL!!", m_UniPluginName);
        return BAD_VALUE;
    }

    switch (m_strCaptureStatus) {
    case STR_CAPTURE_IDLE:
        break;
    case STR_CAPTURE_RUN:
        /* Set face Info */
        shot_ext = (struct camera2_shot_ext *)srcImage[0].buf.addr[srcImage[0].buf.getMetaPlaneIndex()];

        if (shot_ext->shot.dm.stats.faceDetectMode != FACEDETECT_MODE_OFF) {
            for (int i = 0; i < NUM_OF_DETECTED_FACES; i++) {
                if (shot_ext->shot.dm.stats.faceIds[i]) {
                    faceInfo[i].FaceROI.left    = shot_ext->shot.dm.stats.faceRectangles[i][0];
                    faceInfo[i].FaceROI.top     = shot_ext->shot.dm.stats.faceRectangles[i][1];
                    faceInfo[i].FaceROI.right   = shot_ext->shot.dm.stats.faceRectangles[i][2];
                    faceInfo[i].FaceROI.bottom  = shot_ext->shot.dm.stats.faceRectangles[i][3];
                    faceNum++;
                    CLOGV("faceIds[%d](%d) FD(%d,%d,%d,%d)",
                        i, shot_ext->shot.dm.stats.faceIds[i],
                        shot_ext->shot.dm.stats.faceRectangles[i][0],
                        shot_ext->shot.dm.stats.faceRectangles[i][1],
                        shot_ext->shot.dm.stats.faceRectangles[i][2],
                        shot_ext->shot.dm.stats.faceRectangles[i][3]);

                }
            }
        }

        if (faceNum == 0) {
            CLOGD("[STR_CAPTURE]: faceNum: %d", faceNum);
            return NO_ERROR;
        }

        Y = dstImage[0].buf.addr[0];
        UV = dstImage[0].buf.addr[0] + (dstImage[0].rect.w * dstImage[0].rect.h);

        /* Set buffer Info */
        bufferInfo.InBuffY = Y;
        bufferInfo.InBuffU = UV;
        bufferInfo.InWidth = dstImage[0].rect.w;
        bufferInfo.InHeight= dstImage[0].rect.h;
        bufferInfo.InFormat = UNI_PLUGIN_FORMAT_NV21;
        bufferInfo.OutBuffY = Y;
        bufferInfo.OutBuffU = UV;
        bufferInfo.OutWidth = dstImage[0].rect.w;
        bufferInfo.OutHeight= dstImage[0].rect.h;

        /* Set extraData Info */
        memset(&extraInfo, 0, sizeof(UniPluginExtraBufferInfo_t));
        extraInfo.orientation = m_getUniOrientationMode();

        hdrData = m_getUniHDRMode();
        flashData = m_getUniFlashMode();

        ret = m_UniPluginSet(UNI_PLUGIN_INDEX_BUFFER_INFO, &bufferInfo);
        if (ret < 0) {
            CLOGE("[STR_CAPTURE] Plugin set(UNI_PLUGIN_INDEX_BUFFER_INFO) failed!!");
        }

        ret = m_UniPluginSet(UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO, &extraInfo);
        if (ret < 0) {
            CLOGE("[STR_CAPTURE] Plugin set(UNI_PLUGIN_INDEX_CAMERA_INFO) failed!!");
        }

        ret = m_UniPluginSet(UNI_PLUGIN_INDEX_FACE_NUM, &faceNum);
        if (ret < 0) {
            CLOGE("[STR_CAPTURE] Plugin set(UNI_PLUGIN_INDEX_FACE_NUM) failed!!");
        }

        ret = m_UniPluginSet(UNI_PLUGIN_INDEX_FACE_INFO, &faceInfo);
        if (ret < 0) {
            CLOGE("[STR_CAPTURE] Plugin set(UNI_PLUGIN_INDEX_FACE_INFO) failed!!");
        }

        ret = m_UniPluginSet(UNI_PLUGIN_INDEX_HDR_INFO, &hdrData);
        if (ret < 0) {
            CLOGE("[STR_CAPTURE] Plugin set(UNI_PLUGIN_INDEX_HDR_INFO) failed!!");
        }

        ret = m_UniPluginSet(UNI_PLUGIN_INDEX_FLASH_INFO, &flashData);
        if (ret < 0) {
            CLOGE("[STR_CAPTURE] Plugin set(UNI_PLUGIN_INDEX_FLASH_INFO) failed!!");
        }

        CLOGV("[STR_CAPTURE] Y(%p) UV(%p)", Y, UV);
        CLOGV("[STR_CAPTURE] rotaition(%d)", extraInfo.orientation);
        CLOGV("[STR_CAPTURE] face num(%d)", faceNum);
        CLOGV("[STR_CAPTURE] hdr mode(%d)", hdrData);
        CLOGV("[STR_CAPTURE] flash mode(%d)", flashData);

        /* Start processing */
        m_timer.start();
        ret = m_UniPluginProcess();
        if (ret < 0) {
            ALOGE("[STR_CAPTURE] STR CAPTURE plugin process failed!!");
            ret = INVALID_OPERATION;
        }

#ifdef SAMSUNG_STR_CAPTURE
        UTstr debugData;
        debugData.data = new unsigned char[STR_EXIF_SIZE];
        memset(debugData.data, 0, sizeof(debugData));
        ret = m_UniPluginGet(UNI_PLUGIN_INDEX_DEBUG_INFO, &debugData);
        if (ret < 0) {
            CLOGE("[STR_CAPTURE] get debug info failed!!");
        }

        CLOGD("[STR_CAPTURE] Debug buffer size: %d", debugData.size);
        if (debugData.size <= STR_EXIF_SIZE) {
            CLOGD("[STR_CAPTURE] debugData.data = %s", debugData.data);
            m_parameters->setSTRdebugInfo(debugData.data, debugData.size);
        }

        if (debugData.data != NULL)
            delete[] debugData.data;
#endif

        m_timer.stop();
        durationTime = m_timer.durationMsecs();
        CLOGV("[STR_CAPTURE] duration time(%5d msec)", (int)durationTime);
        break;
    case STR_CAPTURE_DEINIT:
        CLOGD("[STR_CAPTURE] STR_CAPTURE_DEINIT");
        break;
    default:
        break;
    }

    return ret;
}

void ExynosCameraPPUniPluginSTR_Capture::m_init(void)
{
    CLOGD(" ");

    strncpy(m_UniPluginName, STR_PREVIEW_PLUGIN_NAME, EXYNOS_CAMERA_NAME_STR_SIZE - 1);

    m_srcImageCapacity.setNumOfImage(1);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21);

    m_dstImageCapacity.setNumOfImage(1);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21);

    m_strCaptureStatus = STR_CAPTURE_IDLE;

    m_refCount = 0;
}

status_t ExynosCameraPPUniPluginSTR_Capture::start(void)
{
    status_t ret = NO_ERROR;

    Mutex::Autolock l(m_uniPluginLock);

    if (m_refCount++ > 0) {
        return ret;
    }

    CLOGD("STR_CAPTURE]");

    ret = m_UniPluginInit();
    if (ret != NO_ERROR) {
        CLOGE("[STR_CAPTURE] STR_CAPTURE Plugin init failed!!");
    }

    m_strCaptureStatus = STR_CAPTURE_RUN;

    return ret;
}

status_t ExynosCameraPPUniPluginSTR_Capture::stop(bool suspendFlag)
{
    status_t ret = NO_ERROR;

    Mutex::Autolock l(m_uniPluginLock);

    if (--m_refCount > 0) {
        return ret;
    }

    CLOGD("[STR_CAPTURE]");

    if (m_UniPluginHandle == NULL) {
        CLOGE("[STR_CAPTURE] STR_CAPTURE Uni plugin(%s) is NULL!!", m_UniPluginName);
        return BAD_VALUE;
    }

    /* Stop case */
    ret = m_UniPluginDeinit();
    if (ret < 0) {
        CLOGE("[STR_CAPTURE] STR_CAPTURE plugin deinit failed!!");
    }

    m_strCaptureStatus = STR_CAPTURE_DEINIT;

    return ret;
}

