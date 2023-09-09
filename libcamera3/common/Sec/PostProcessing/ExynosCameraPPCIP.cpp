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
#define LOG_TAG "ExynosCameraPPCIP"

#include "ExynosCameraPPCIP.h"

ExynosCameraPPCIP::~ExynosCameraPPCIP()
{
}

status_t ExynosCameraPPCIP::m_create(void)
{
    status_t ret = NO_ERROR;

    m_cipMode = 0;

    if (m_cipInterface == NULL) {
        m_cipInterface = cip_create(m_cipMode);
        if (m_cipInterface == NULL) {
            CLOGE("Cannot create cipInterface, mode(%d)", (int)m_cipMode);
            return INVALID_OPERATION;
        }

        ret = cip_open(m_cipInterface);
        if (ret != NO_ERROR) {
            CLOGE("CIP open fail, ret(%d)", ret);
            return ret;
        }

        CLOGI("CIP created");
    }

    return ret;
}

status_t ExynosCameraPPCIP::m_destroy(void)
{
    status_t ret = NO_ERROR;

    if (m_cipInterface != NULL) {
        ret = cip_close(m_cipInterface);
        if (ret != NO_ERROR) {
            CLOGE("close fail");
            return ret;
        }

        ret = cip_destroy(&m_cipInterface);
        if (ret != NO_ERROR) {
            CLOGE("destory fail");
            return ret;
        }

        delete m_cipInterface;
        m_cipInterface = NULL;
    }

    return ret;
}

status_t ExynosCameraPPCIP::m_draw(ExynosCameraImage *srcImage,
                                    ExynosCameraImage *dstImage)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    status_t ret = NO_ERROR;

    cip_buffer_t *cipBuffer;
    cip_in_buffer_t property = CIP_WIDE_YUV;

    /* Set format & size */
    ret = cip_setColorFormat(m_cipInterface, srcImage[0].rect.colorFormat);
    if (ret != NO_ERROR) {
        CLOGE("CIP setColorFormat fail, ret(%d)", ret);
        goto draw_done;
    }

    ret = cip_prepare(m_cipInterface, srcImage[0].rect.w, srcImage[0].rect.h, srcImage[0].rect.w, srcImage[0].rect.h);
    if (ret != NO_ERROR) {
        CLOGE("CIP prepare fail, ret(%d)", ret);
        goto draw_done;
    }

    /* Set src buffer */

    /* Common src buffer is srcImage[0] */
    if (m_cipMode == 0) {
        property = CIP_WIDE_YUV;
    } else {
        property = CIP_TELE_YUV;
    }
            
    cipBuffer->planeCount = srcImage[0].buf.planeCount;
    for (int i = 0; i < buf->planeCount; i++) {
        cipBuffer->fd[i] = srcImage[0].buf.fd[i];
        cipBuffer->size[i] = srcImage[0].buf.size[i];
        cipBuffer->addr[i] = srcImage[0].buf.addr[i];
    }

    ret = cip_setInBuf(m_cipInterface, property, rect, cipBuffer);
    if (ret != NO_ERROR) {
        CLOGE("CIP setInBuffer fail, ret(%d)", ret);
        goto draw_done;
    }

    if (m_cipMode == 5) {
        /* HBZ case */
        property CIP_TELE_YUV;
        cipBuffer->planeCount = srcImage[1].buf.planeCount;

        for (int i = 0; i < buf->planeCount; i++) {
            cipBuffer->fd[i] = srcImage[1].buf.fd[i];
            cipBuffer->size[i] = srcImage[1].buf.size[i];
            cipBuffer->addr[i] = srcImage[1].buf.addr[i];
        }

        ret = cip_setInBuf(m_cipInterface, property, rect, cipBuffer);
        if (ret != NO_ERROR) {
            CLOGE("CIP setInBuffer fail, ret(%d)", ret);
            goto draw_done;
        }
    } else if (m_cipMode == 6) {
        /* OOF preview case */
        property CIP_DEPTH_MAP;
        cipBuffer->planeCount = srcImage[2].buf.planeCount;

        for (int i = 0; i < buf->planeCount; i++) {
            cipBuffer->fd[i] = srcImage[2].buf.fd[i];
            cipBuffer->size[i] = srcImage[2].buf.size[i];
            cipBuffer->addr[i] = srcImage[2].buf.addr[i];
        }

        ret = cip_setInBuf(m_cipInterface, property, rect, cipBuffer);
        if (ret != NO_ERROR) {
            CLOGE("CIP setInBuffer fail, ret(%d)", ret);
            goto draw_done;
        }
    }

    /* Set dst buffer */
    cipBuffer->planeCount = dstImage[0].buf.planeCount;

    for (int i = 0; i < buf->planeCount; i++) {
        cipBuffer->fd[i] = dstImage[0].buf.fd[i];
        cipBuffer->size[i] = dstImage[0].buf.size[i];
        cipBuffer->addr[i] = dstImage[0].buf.addr[i];
    }

    ret = cip_setOutBuf(m_cipInterface, cipBuffer);
    if (ret != NO_ERROR) {
        CLOGE("CIP setInBuffer fail, ret(%d)", ret);
        goto draw_done;
    }

    /* Update and execute */
    ret = cip_updateConfig(m_cipInterface);
    if (ret != NO_ERROR) {
        CLOGE("CIP updateConfig fail, ret(%d)", ret);
        goto draw_done;
    }

    ret = cip_execute(m_cipInterface);
    if (ret != NO_ERROR) {
        CLOGE("CIP execute fail, ret(%d)", ret);
        goto draw_done;
    }

    ret = cip_waitDone(m_cipInterface);
    if (ret != NO_ERROR) {
        CLOGE("CIP waitDone fail, ret(%d)", ret);
        goto draw_done;
    }

draw_done:
    if (ret != NO_ERROR) {
        CLOGD("[dstBuf.fd[0] %d][dstBuf.size[0] + dstBuf.size[1] + dstBuf.size[2] %d]",
            dstImage[0].buf.fd[0], dstImage[0].buf.size[0] + dstImage[0].buf.size[1] + dstImage[0].buf.size[2]);
        CLOGD("[pictureW %d][pictureH %d][pictureFormat %d]",
            srcImage[0].rect.w, srcImage[0].rect.h, srcImage[0].rect.colorFormat);
    }

    CLOGV("-OUT-");

    return ret;
}

void ExynosCameraPPCIP::m_init(void)
{
    m_srcImageCapacity.setNumOfImage(3);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV12);

    m_dstImageCapacity.setNumOfImage(1);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV12);

    m_cipInterface = NULL;
    m_cipMode = 0;
}
