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
#define LOG_TAG "ExynosCameraPPJPEG"

#include "ExynosCameraPPJPEG.h"

ExynosCameraPPJPEG::~ExynosCameraPPJPEG()
{
}

status_t ExynosCameraPPJPEG::m_draw(ExynosCameraImage *srcImage,
                                    ExynosCameraImage *dstImage,
                                    ExynosCameraParameters *params)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    status_t ret = NO_ERROR;

    exif_attribute_t exifInfo;
    params->getFixedExifInfo(&exifInfo);

    camera2_shot_ext *shot_ext = (struct camera2_shot_ext *)(srcImage[0].buf.addr[srcImage[0].buf.getMetaPlaneIndex()]);

    /* JPEG Quality, Thumbnail Quality Setting */
    int jpegQuality = (int) shot_ext->shot.ctl.jpeg.quality;
    int thumbnailQuality = (int) shot_ext->shot.ctl.jpeg.thumbnailQuality;

    /* JPEG Thumbnail Size Setting */
    ExynosRect thumbnailRect;
    thumbnailRect.w = shot_ext->shot.ctl.jpeg.thumbnailSize[0];
    thumbnailRect.h = shot_ext->shot.ctl.jpeg.thumbnailSize[1];

    if (m_jpegEnc.create()) {
        CLOGE("m_jpegEnc.create() fail");
        ret = INVALID_OPERATION;
        goto jpeg_encode_done;
    }

    m_jpegEnc.setExtScalerNum(params->getScalerNodeNumPicture());

    if (m_jpegEnc.setSize(srcImage[0].rect.w, srcImage[0].rect.h)) {
        CLOGE("m_jpegEnc.setSize() fail");
        ret = INVALID_OPERATION;
        goto jpeg_encode_done;
    }

    if (0 < jpegQuality && jpegQuality <= 100) {
        if (m_jpegEnc.setQuality(jpegQuality)) {
            CLOGE("m_jpegEnc.setQuality() fail");
            ret = INVALID_OPERATION;
            goto jpeg_encode_done;
        }
    }

    if (m_jpegEnc.setColorFormat(srcImage[0].rect.colorFormat)) {
        CLOGE("m_jpegEnc.setColorFormat() fail");
        ret = INVALID_OPERATION;
        goto jpeg_encode_done;
    }

    if (m_jpegEnc.setJpegFormat(dstImage[0].rect.colorFormat)) {
        CLOGE("m_jpegEnc.setJpegFormat() fail");
        ret = INVALID_OPERATION;
        goto jpeg_encode_done;
    }

    if (thumbnailRect.w != 0 && thumbnailRect.h != 0) {
        exifInfo.enableThumb = true;

        if (srcImage[0].rect.w < 320 || srcImage[0].rect.h < 240) {
            thumbnailRect.w = 160;
            thumbnailRect.h = 120;
        }
        if (m_jpegEnc.setThumbnailSize(thumbnailRect.w, thumbnailRect.h)) {
            CLOGE("m_jpegEnc.setThumbnailSize(%d, %d) fail",
                    thumbnailRect.w, thumbnailRect.h);
            ret = INVALID_OPERATION;
            goto jpeg_encode_done;
        }

        if (thumbnailQuality > 0 && thumbnailQuality <= 100) {
            if (m_jpegEnc.setThumbnailQuality(thumbnailQuality)) {
                ret = INVALID_OPERATION;
                CLOGE("m_jpegEnc.setThumbnailQuality(%d) fail", thumbnailQuality);
            }
        }
    } else {
        exifInfo.enableThumb = false;
    }

    if (m_jpegEnc.setInBuf((int *)&(srcImage[0].buf.fd), (int *)srcImage[0].buf.size)) {
        CLOGE("m_jpegEnc.setInBuf() fail");
        ret = INVALID_OPERATION;
        goto jpeg_encode_done;
    }

    if (m_jpegEnc.setOutBuf(dstImage[0].buf.fd[0], dstImage[0].buf.size[0] + dstImage[0].buf.size[1] + dstImage[0].buf.size[2])) {
        CLOGE("m_jpegEnc.setOutBuf() fail");
        ret = INVALID_OPERATION;
        goto jpeg_encode_done;
    }

    if (m_jpegEnc.updateConfig()) {
        CLOGE("m_jpegEnc.updateConfig() fail");
        ret = INVALID_OPERATION;
        goto jpeg_encode_done;
    }

    params->setExifChangedAttribute(&exifInfo, &srcImage[0].rect, &thumbnailRect, &shot_ext->shot);

    if (m_jpegEnc.encode((int *)&dstImage[0].buf.size, &exifInfo, (char **)dstImage[0].buf.addr, params->getDebugAttribute())) {
        CLOGE("m_jpegEnc.encode() fail");
        ret = INVALID_OPERATION;
        goto jpeg_encode_done;
    }

#ifdef SAMSUNG_DNG
    if (params->getDNGCaptureModeOn() == true
        && m_configurations->getModeValue(CONFIGURATION_SERIES_SHOT_MODE) != SERIES_SHOT_MODE_BURST) {
        unsigned int thumbBufSize = thumbnailRect.w * thumbnailRect.h * 2;
        dng_thumbnail_t dngThumbnailBuf = params->createDngThumbnailBuffer(thumbBufSize);
        char *thumbBufAddr = dngThumbnailBuf->buf;

        if (thumbBufAddr) {
            dngThumbnailBuf->size = m_jpegEnc.GetThumbnailImage(thumbBufAddr, thumbBufSize);
            if (!dngThumbnailBuf->size)
                CLOGE("[DNG] GetThumbnailImage failed");
        } else {
            CLOGE("[DNG] Thumbnail buf is NULL");
            dngThumbnailBuf->size = 0;
        }

        if (m_configurations->getCaptureExposureTime() > CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT) {
            dngThumbnailBuf->frameCount = 0;
        } else {
            dngThumbnailBuf->frameCount = shot_ext->shot.dm.request.frameCount;
        }

        params->putDngThumbnailBuffer(dngThumbnailBuf);

        CLOGD("[DNG] Thumbnail enable(%d), (addr:size:frame_count) [%p:%d:%d]",
                exifInfo.enableThumb, thumbBufAddr, dngThumbnailBuf->size, dngThumbnailBuf->frameCount);
    }
#endif

jpeg_encode_done:
    if (ret != NO_ERROR) {
        CLOGD("[dstBuf.fd[0] %d][dstBuf.size[0] + dstBuf.size[1] + dstBuf.size[2] %d]",
            dstImage[0].buf.fd[0], dstImage[0].buf.size[0] + dstImage[0].buf.size[1] + dstImage[0].buf.size[2]);
        CLOGD("[pictureW %d][pictureH %d][pictureFormat %d]",
            srcImage[0].rect.w, srcImage[0].rect.h, srcImage[0].rect.colorFormat);
    }

    if (m_jpegEnc.flagCreate() == true)
        m_jpegEnc.destroy();

    CLOGI("-OUT-");

    return ret;
}

void ExynosCameraPPJPEG::m_init(void)
{
    m_srcImageCapacity.setNumOfImage(1);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV12M);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21M);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV12);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_YVU420M);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_YVU420);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_YUYV);

    m_dstImageCapacity.setNumOfImage(1);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_JPEG_422);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_JPEG_420);
}
