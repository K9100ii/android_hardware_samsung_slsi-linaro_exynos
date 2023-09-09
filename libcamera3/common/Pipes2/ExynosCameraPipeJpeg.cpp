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

/*#define LOG_NDEBUG 0 */
#define LOG_TAG "ExynosCameraPipeJpeg"

#include "ExynosCameraPipeJpeg.h"

/* For test */
#include "ExynosCameraBuffer.h"

namespace android {

status_t ExynosCameraPipeJpeg::stop(void)
{
    CLOGD("");

    m_jpegEnc.destroy();

    ExynosCameraSWPipe::stop();

    return NO_ERROR;
}

status_t ExynosCameraPipeJpeg::m_destroy(void)
{
    if (m_shot_ext != NULL) {
        delete m_shot_ext;
        m_shot_ext = NULL;
    }

    ExynosCameraSWPipe::m_destroy();

    return NO_ERROR;
}

status_t ExynosCameraPipeJpeg::m_run(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    status_t ret = 0;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;

    ExynosCameraBuffer yuvBuf;
    ExynosCameraBuffer jpegBuf;

#ifdef SAMSUNG_DNG
    dng_thumbnail_t *dngThumbnailBuf = NULL;
    char *thumbBufAddr = NULL;
    unsigned int thumbBufSize = 0;
#endif

    ExynosRect pictureRect;
    ExynosRect thumbnailRect;
    int jpegQuality = m_parameters->getJpegQuality();
    int thumbnailQuality = m_parameters->getThumbnailQuality();
    int jpegformat = V4L2_PIX_FMT_JPEG_422;
#ifdef SAMSUNG_LLS_DEBLUR
    if(m_parameters->getLDCaptureMode() > 0) {
        jpegformat = V4L2_PIX_FMT_JPEG_420;
    } else
#endif
    {
#ifdef SAMSUNG_LENS_DC
        if(m_parameters->getLensDCEnable()) {
            jpegformat = V4L2_PIX_FMT_JPEG_420;
        } else
#endif
#ifdef SAMSUNG_STR_CAPTURE
        if (m_parameters->getSTRCaptureEnable()) {
            jpegformat = V4L2_PIX_FMT_JPEG_420;
        } else
#endif
        {
            jpegformat = (JPEG_INPUT_COLOR_FMT == V4L2_PIX_FMT_YUYV) ?  V4L2_PIX_FMT_JPEG_422 : V4L2_PIX_FMT_JPEG_420;
        }
    }

    memset(m_shot_ext, 0x00, sizeof(struct camera2_shot_ext));

    exif_attribute_t exifInfo;
    m_parameters->getFixedExifInfo(&exifInfo);

    pictureRect.colorFormat = m_parameters->getHwPictureFormat();
#ifdef SAMSUNG_LLS_DEBLUR
    if(m_parameters->getLDCaptureMode() > 0) {
        pictureRect.colorFormat = V4L2_PIX_FMT_NV21;
    } else
#endif
    {
#ifdef SAMSUNG_LENS_DC
        if(m_parameters->getLensDCEnable()) {
            pictureRect.colorFormat = V4L2_PIX_FMT_NV21;
        } else
#endif
#ifdef SAMSUNG_STR_CAPTURE
        if (m_parameters->getSTRCaptureEnable()) {
            pictureRect.colorFormat = V4L2_PIX_FMT_NV21;
        } else
#endif
        {
            pictureRect.colorFormat = JPEG_INPUT_COLOR_FMT;
        }
    }

    m_parameters->getPictureSize(&pictureRect.w, &pictureRect.h);
    m_parameters->getThumbnailSize(&thumbnailRect.w, &thumbnailRect.h);

    CLOGD("picture size(%dx%d), thumbnail size(%dx%d)",
             pictureRect.w, pictureRect.h, thumbnailRect.w, thumbnailRect.h);

    CLOGD("wait JPEG pipe inputFrameQ");
    ret = m_inputFrameQ->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGW("wait timeout");
        } else {
            CLOGE("wait and pop fail, ret(%d)", ret);
            /* TODO: doing exception handling */
        }
        return ret;
    }

    if (newFrame == NULL) {
        CLOGE("new frame is NULL");
        return NO_ERROR;
    }

    CLOGD("JPEG pipe inputFrameQ output done");

    newFrame->getMetaData(m_shot_ext);

    /* JPEG Quality, Thumbnail Quality Setting */
    jpegQuality = (int) m_shot_ext->shot.ctl.jpeg.quality;
    thumbnailQuality = (int) m_shot_ext->shot.ctl.jpeg.thumbnailQuality;

    /* JPEG Thumbnail Size Setting */
    thumbnailRect.w = m_shot_ext->shot.ctl.jpeg.thumbnailSize[0];
    thumbnailRect.h = m_shot_ext->shot.ctl.jpeg.thumbnailSize[1];

    ret = newFrame->getSrcBuffer(getPipeId(), &yuvBuf);
    if (ret < 0) {
        CLOGE("frame get src buffer fail, ret(%d)", ret);
        /* TODO: doing exception handling */
        return OK;
    }

    ret = newFrame->getDstBuffer(getPipeId(), &jpegBuf);
    if (ret < 0) {
        CLOGE("frame get dst buffer fail, ret(%d)", ret);
        /* TODO: doing exception handling */
        return OK;
    }

    if (m_jpegEnc.create()) {
        CLOGE("m_jpegEnc.create() fail");
        ret = INVALID_OPERATION;
        goto jpeg_encode_done;
    }

    m_jpegEnc.setExtScalerNum(m_parameters->getScalerNodeNumPicture());

#ifdef SAMSUNG_JQ
    if (m_parameters->getJpegQtableOn() == true && m_parameters->getJpegQtableStatus() != JPEG_QTABLE_DEINIT) {
        if (m_parameters->getJpegQtableStatus() == JPEG_QTABLE_UPDATED) {
            CLOGD("[JQ]:Get JPEG Q-table");
            m_parameters->setJpegQtableStatus(JPEG_QTABLE_RETRIEVED);
            m_parameters->getJpegQtable(m_qtable);
        }

        CLOGD("[JQ]:Set JPEG Q-table");
        if (m_jpegEnc.setQuality(m_qtable)) {
            CLOGE("[JQ]:m_jpegEnc.setQuality(qtable[]) fail");
            ret = INVALID_OPERATION;
            goto jpeg_encode_done;
        }
    } else
#endif
    {
        if (m_jpegEnc.setQuality(jpegQuality)) {
            CLOGE("m_jpegEnc.setQuality() fail");
            ret = INVALID_OPERATION;
            goto jpeg_encode_done;
        }
    }

    if (m_jpegEnc.setSize(pictureRect.w, pictureRect.h)) {
        CLOGE("m_jpegEnc.setSize() fail");
        ret = INVALID_OPERATION;
        goto jpeg_encode_done;
    }

    if (m_jpegEnc.setColorFormat(pictureRect.colorFormat)) {
        CLOGE("m_jpegEnc.setColorFormat() fail");
        ret = INVALID_OPERATION;
        goto jpeg_encode_done;
    }

    if (m_jpegEnc.setJpegFormat(jpegformat)) {
        CLOGE("m_jpegEnc.setJpegFormat() fail");
        ret = INVALID_OPERATION;
        goto jpeg_encode_done;
    }

    if (thumbnailRect.w != 0 && thumbnailRect.h != 0) {
        exifInfo.enableThumb = true;
        if (pictureRect.w < 320 || pictureRect.h < 240) {
            thumbnailRect.w = 160;
            thumbnailRect.h = 120;
        }
        if (m_jpegEnc.setThumbnailSize(thumbnailRect.w, thumbnailRect.h)) {
            CLOGE("m_jpegEnc.setThumbnailSize(%d, %d) fail", thumbnailRect.w, thumbnailRect.h);
            ret = INVALID_OPERATION;
            goto jpeg_encode_done;
        }
        if (0 < thumbnailQuality && thumbnailQuality <= 100) {
            if (m_jpegEnc.setThumbnailQuality(thumbnailQuality)) {
                ret = INVALID_OPERATION;
                CLOGE("m_jpegEnc.setThumbnailQuality(%d) fail", thumbnailQuality);
            }
        }
    } else {
        exifInfo.enableThumb = false;
    }

    /* wait for medata update */
    if(newFrame->getMetaDataEnable() == false) {
        CLOGD(" Waiting for update jpeg metadata failed (%d) ", ret);
    }

    /* get dynamic meters for make exif info */
    newFrame->getDynamicMeta(m_shot_ext);
    newFrame->getUserDynamicMeta(m_shot_ext);

    m_parameters->setExifChangedAttribute(&exifInfo, &pictureRect, &thumbnailRect, &m_shot_ext->shot);

    if (m_jpegEnc.setInBuf((int *)&(yuvBuf.fd), (int *)yuvBuf.size)) {
        CLOGE("m_jpegEnc.setInBuf() fail");
        ret = INVALID_OPERATION;
        goto jpeg_encode_done;
    }

    if (m_jpegEnc.setOutBuf(jpegBuf.fd[0], jpegBuf.size[0] + jpegBuf.size[1] + jpegBuf.size[2])) {
        CLOGE("m_jpegEnc.setOutBuf() fail");
        ret = INVALID_OPERATION;
        goto jpeg_encode_done;
    }

    if (m_jpegEnc.updateConfig()) {
        CLOGE("m_jpegEnc.updateConfig() fail");
        ret = INVALID_OPERATION;
        goto jpeg_encode_done;
    }

#ifdef SAMSUNG_DNG
    if (m_parameters->getDNGCaptureModeOn() == true
        && m_parameters->getSeriesShotMode() != SERIES_SHOT_MODE_BURST) {
        thumbBufSize = thumbnailRect.w * thumbnailRect.h * 2;
        dngThumbnailBuf = m_parameters->createDngThumbnailBuffer(thumbBufSize);
        thumbBufAddr = dngThumbnailBuf->buf;
    }
#endif

    if (m_jpegEnc.encode((int *)&jpegBuf.size, &exifInfo, (char **)jpegBuf.addr, m_parameters->getDebugAttribute())) {
        CLOGE("m_jpegEnc.encode() fail");
        ret = INVALID_OPERATION;
        goto jpeg_encode_done;
    }

#ifdef SAMSUNG_DNG
    if (m_parameters->getDNGCaptureModeOn() == true
        && m_parameters->getSeriesShotMode() != SERIES_SHOT_MODE_BURST) {
        if (thumbBufAddr) {
            dngThumbnailBuf->size = m_jpegEnc.GetThumbnailImage(thumbBufAddr, thumbBufSize);
            if (!dngThumbnailBuf->size)
                CLOGE("[DNG] GetThumbnailImage failed");
        } else {
            CLOGE("[DNG] Thumbnail buf is NULL");
            dngThumbnailBuf->size = 0;
        }

        if (m_parameters->getCaptureExposureTime() > CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT) {
            dngThumbnailBuf->frameCount = 0;
        } else {
            dngThumbnailBuf->frameCount = m_shot_ext->shot.dm.request.frameCount;
        }

        m_parameters->putDngThumbnailBuffer(dngThumbnailBuf);

        CLOGD("[DNG] Thumbnail enable(%d), (addr:size:frame_count) [%p:%d:%d]",
                exifInfo.enableThumb, thumbBufAddr, dngThumbnailBuf->size, dngThumbnailBuf->frameCount);
    }
#endif

    newFrame->setJpegSize(jpegBuf.size[0]);

    ret = newFrame->setEntityState(getPipeId(), ENTITY_STATE_FRAME_DONE);
    if (ret < 0) {
        CLOGE("set entity state fail, ret(%d)", ret);
        /* TODO: doing exception handling */
        return OK;
    }

    m_outputFrameQ->pushProcessQ(&newFrame);

jpeg_encode_done:
    if (ret != NO_ERROR) {
        CLOGD("[jpegBuf.fd[0] %d][jpegBuf.size[0] + jpegBuf.size[1] + jpegBuf.size[2] %d]",
            jpegBuf.fd[0], jpegBuf.size[0] + jpegBuf.size[1] + jpegBuf.size[2]);
        CLOGD("[pictureW %d][pictureH %d][pictureFormat %d]",
            pictureRect.w, pictureRect.h, pictureRect.colorFormat);
    }

    if (m_jpegEnc.flagCreate() == true)
        m_jpegEnc.destroy();

    CLOGI(" -OUT-");

    return ret;
}

void ExynosCameraPipeJpeg::m_init(void)
{
    m_reprocessing = 1;
    m_shot_ext = new struct camera2_shot_ext;
}

}; /* namespace android */
