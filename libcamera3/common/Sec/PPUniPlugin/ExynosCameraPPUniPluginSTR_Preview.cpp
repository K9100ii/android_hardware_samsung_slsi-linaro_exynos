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
#define LOG_TAG "ExynosCameraPPUniPluginSTR_Preview"

#include "ExynosCameraPPUniPluginSTR_Preview.h"

ExynosCameraPPUniPluginSTR_Preview::~ExynosCameraPPUniPluginSTR_Preview()
{
}

status_t ExynosCameraPPUniPluginSTR_Preview::m_draw(ExynosCameraImage *srcImage,
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
        CLOGE("[STR_PREVIEW] STR_PREVIEW Uni plugin(%s) is NULL!!", m_UniPluginName);
        return BAD_VALUE;
    }

    switch (m_strPreviewStatus) {
    case STR_PREVIEW_IDLE:
        break;
    case STR_PREVIEW_RUN:
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
            return NO_ERROR;
        }

        if (dstImage[0].rect.colorFormat == V4L2_PIX_FMT_NV21) { /* preview callback stream */
            Y = dstImage[0].buf.addr[0];
            UV = dstImage[0].buf.addr[0] + (dstImage[0].rect.w * dstImage[0].rect.h);
        } else { /* preview stream */
            Y = dstImage[0].buf.addr[0];
            UV = dstImage[0].buf.addr[1];
        }

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
            CLOGE("[STR_PREVIEW] Plugin set(UNI_PLUGIN_INDEX_BUFFER_INFO) failed!!");
        }

        ret = m_UniPluginSet(UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO, &extraInfo);
        if (ret < 0) {
            CLOGE("[STR_PREVIEW] Plugin set(UNI_PLUGIN_INDEX_CAMERA_INFO) failed!!");
        }

        ret = m_UniPluginSet(UNI_PLUGIN_INDEX_FACE_NUM, &faceNum);
        if (ret < 0) {
            CLOGE("[STR_PREVIEW] Plugin set(UNI_PLUGIN_INDEX_FACE_NUM) failed!!");
        }

        ret = m_UniPluginSet(UNI_PLUGIN_INDEX_FACE_INFO, &faceInfo);
        if (ret < 0) {
            CLOGE("[STR_PREVIEW] Plugin set(UNI_PLUGIN_INDEX_FACE_INFO) failed!!");
        }

        ret = m_UniPluginSet(UNI_PLUGIN_INDEX_HDR_INFO, &hdrData);
        if (ret < 0) {
            CLOGE("[STR_PREVIEW] Plugin set(UNI_PLUGIN_INDEX_HDR_INFO) failed!!");
        }

        ret = m_UniPluginSet(UNI_PLUGIN_INDEX_FLASH_INFO, &flashData);
        if (ret < 0) {
            CLOGE("[STR_PREVIEW] Plugin set(UNI_PLUGIN_INDEX_FLASH_INFO) failed!!");
        }

        CLOGV("[STR_PREVIEW] Y(%p) UV(%p)", Y, UV);
        CLOGV("[STR_PREVIEW] rotaition(%d)", extraInfo.orientation);
        CLOGV("[STR_PREVIEW] face num(%d)", faceNum);
        CLOGV("[STR_PREVIEW] hdr mode(%d)", hdrData);
        CLOGV("[STR_PREVIEW] flash mode(%d)", flashData);

        /* Start processing */
        m_timer.start();
        ret = m_UniPluginProcess();
        if (ret < 0) {
            ALOGE("[STR_PREVIEW] STR Preview plugin process failed!!");
            ret = INVALID_OPERATION;
        }
        m_timer.stop();
        durationTime = m_timer.durationMsecs();
        CLOGV("[STR_PREVIEW] duration time(%5d msec)", (int)durationTime);
        break;
    case STR_PREVIEW_DEINIT:
        CLOGD("[STR_PREVIEW] STR_PREVIEW_DEINIT");
        break;
    default:
        break;
    }

    return ret;
}

void ExynosCameraPPUniPluginSTR_Preview::m_init(void)
{
    CLOGD(" ");

    strncpy(m_UniPluginName, STR_PREVIEW_PLUGIN_NAME, EXYNOS_CAMERA_NAME_STR_SIZE - 1);

    m_srcImageCapacity.setNumOfImage(1);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21M);

    m_dstImageCapacity.setNumOfImage(1);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21M);

    m_strPreviewStatus = STR_PREVIEW_IDLE;

    m_refCount = 0;
}

status_t ExynosCameraPPUniPluginSTR_Preview::start(void)
{
    status_t ret = NO_ERROR;

    Mutex::Autolock l(m_uniPluginLock);

    if (m_refCount++ > 0) {
        return ret;
    }

    CLOGD("STR_PREVIEW]");

    ret = m_UniPluginInit();
    if (ret != NO_ERROR) {
        CLOGE("[STR_PREVIEW] STR_PREVIEW Plugin init failed!!");
    }

    m_strPreviewStatus = STR_PREVIEW_RUN;

    return ret;
}

status_t ExynosCameraPPUniPluginSTR_Preview::stop(bool suspendFlag)
{
    status_t ret = NO_ERROR;

    Mutex::Autolock l(m_uniPluginLock);

    if (--m_refCount > 0) {
        return ret;
    }

    CLOGD("[STR_PREVIEW]");

    if (m_UniPluginHandle == NULL) {
        CLOGE("[STR_PREVIEW] STR_PREVIEW Uni plugin(%s) is NULL!!", m_UniPluginName);
        return BAD_VALUE;
    }

    /* Stop case */
    ret = m_UniPluginDeinit();
    if (ret < 0) {
        CLOGE("[STR_PREVIEW] STR_PREVIEW plugin deinit failed!!");
    }

    m_strPreviewStatus = STR_PREVIEW_DEINIT;

    return ret;
}

