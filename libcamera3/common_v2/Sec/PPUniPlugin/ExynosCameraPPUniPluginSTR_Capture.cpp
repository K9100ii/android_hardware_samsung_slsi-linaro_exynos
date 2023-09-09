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
                                           ExynosCameraImage *dstImage,
                                           ExynosCameraParameters *params)
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

    if (m_uniPluginHandle == NULL) {
        CLOGE("[STR_CAPTURE] STR_CAPTURE Uni plugin(%s) is NULL!!", m_uniPluginName);
        return BAD_VALUE;
    }

    switch (m_strCaptureStatus) {
    case STR_CAPTURE_IDLE:
        break;
    case STR_CAPTURE_RUN:
        if (srcImage[0].buf.index < 0 || dstImage[0].buf.index < 0) {
            return NO_ERROR;
        }

        /* Set face Info */
        shot_ext = (struct camera2_shot_ext *)srcImage[0].buf.addr[srcImage[0].buf.getMetaPlaneIndex()];
        memset(faceInfo, 0x00, sizeof(faceInfo));

        if (shot_ext->shot.ctl.stats.faceDetectMode > FACEDETECT_MODE_OFF) {
            ExynosRect vraInputSize;
            ExynosRect2 detectedFace[NUM_OF_DETECTED_FACES];

            if (params->getHwConnectionMode(PIPE_MCSC, PIPE_VRA) == HW_CONNECTION_MODE_M2M) {
#ifdef CAPTURE_FD_SYNC_WITH_PREVIEW
                params->getHwVraInputSize(&vraInputSize.w, &vraInputSize.h, params->getDsInputPortId(false));
#else
                params->getHwVraInputSize(&vraInputSize.w, &vraInputSize.h,
                                                shot_ext->shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS]);
#endif
            } else {
                params->getSize(HW_INFO_HW_YUV_SIZE, (uint32_t *)&vraInputSize.w, (uint32_t *)&vraInputSize.h, 0);
            }

            if (getCameraId() == CAMERA_ID_FRONT
                && m_configurations->getModeValue(CONFIGURATION_FLIP_HORIZONTAL) == true) {
                /* convert left, right values when flip horizontal(front camera) */
                for (int i = 0; i < NUM_OF_DETECTED_FACES; i++) {
                    if (shot_ext->shot.dm.stats.faceIds[i]) {
                        detectedFace[i].x1 = vraInputSize.w - shot_ext->shot.dm.stats.faceRectangles[i][2];
                        detectedFace[i].y1 = shot_ext->shot.dm.stats.faceRectangles[i][1];
                        detectedFace[i].x2 = vraInputSize.w - shot_ext->shot.dm.stats.faceRectangles[i][0];
                        detectedFace[i].y2 = shot_ext->shot.dm.stats.faceRectangles[i][3];
                    }
                }
            } else {
                for (int i = 0; i < NUM_OF_DETECTED_FACES; i++) {
                    if (shot_ext->shot.dm.stats.faceIds[i]) {
                        detectedFace[i].x1 = shot_ext->shot.dm.stats.faceRectangles[i][0];
                        detectedFace[i].y1 = shot_ext->shot.dm.stats.faceRectangles[i][1];
                        detectedFace[i].x2 = shot_ext->shot.dm.stats.faceRectangles[i][2];
                        detectedFace[i].y2 = shot_ext->shot.dm.stats.faceRectangles[i][3];
                    }
                }
            }

            for (int i = 0; i < NUM_OF_DETECTED_FACES; i++) {
                if (shot_ext->shot.dm.stats.faceIds[i]) {
                    faceInfo[i].faceROI.left    = calibratePosition(vraInputSize.w, srcImage[0].rect.w, detectedFace[i].x1);
                    faceInfo[i].faceROI.top     = calibratePosition(vraInputSize.h, srcImage[0].rect.h, detectedFace[i].y1);
                    faceInfo[i].faceROI.right   = calibratePosition(vraInputSize.w, srcImage[0].rect.w, detectedFace[i].x2);
                    faceInfo[i].faceROI.bottom  = calibratePosition(vraInputSize.h, srcImage[0].rect.h, detectedFace[i].y2);
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
        bufferInfo.inBuffY = Y;
        bufferInfo.inBuffU = UV;
        bufferInfo.inWidth = dstImage[0].rect.w;
        bufferInfo.inHeight= dstImage[0].rect.h;
        bufferInfo.inFormat = UNI_PLUGIN_FORMAT_NV21;
        bufferInfo.outBuffY = Y;
        bufferInfo.outBuffU = UV;
        bufferInfo.outWidth = dstImage[0].rect.w;
        bufferInfo.outHeight= dstImage[0].rect.h;

        /* Set extraData Info */
        memset(&extraInfo, 0, sizeof(UniPluginExtraBufferInfo_t));
        extraInfo.orientation = m_getUniOrientationMode(true);

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
        
#ifdef SAMSUNG_STR_BV_OFFSET
        ret = m_UniPluginGet(UNI_PLUGIN_INDEX_FACE_BRIGHT_OFFSET, &dstImage[0].bvOffset);        
        if (ret < 0) {
            CLOGE("[STR_CAPTURE] Plugin get(UNI_PLUGIN_INDEX_FACE_BRIGHT_OFFSET) failed!!");
        }
        CLOGV("[STR_CAPTURE] get strBvOffset : %d", dstImage[0].bvOffset);
#endif

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
            params->setSTRdebugInfo(debugData.data, debugData.size);
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

    strncpy(m_uniPluginName, STR_CAPTURE_PLUGIN_NAME, EXYNOS_CAMERA_NAME_STR_SIZE - 1);

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

    if (m_uniPluginHandle == NULL) {
        CLOGE("[STR_CAPTURE] STR_CAPTURE Uni plugin(%s) is NULL!!", m_uniPluginName);
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

