/*
 * Copyright Samsung Electronics Co.,LTD.
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef EXYNOS_JPEG_ENCODER_FOR_CAMERA_H_
#define EXYNOS_JPEG_ENCODER_FOR_CAMERA_H_

#include "ExynosCameraConfig.h"

#include "ExynosExif.h"

#include "ExynosJpegApi.h"
#include "ExynosCameraThread.h"

#include <sys/mman.h>
#include <ion/ion.h>

#define JPEG_THUMBNAIL_QUALITY 38
#define EXIF_LIMIT_SIZE 64*1024

#define MAX_IMAGE_PLANE_NUM (3)

static const char ExifAsciiPrefix[] = { 0x41, 0x53, 0x43, 0x49, 0x49, 0x0, 0x0, 0x0 };

#define THUMBNAIL_IMAGE_PIXEL_SIZE (4)
#define MAX_JPG_WIDTH (8192)
#define MAX_JPG_HEIGHT (8192)

#define MAX_INPUT_BUFFER_PLANE_NUM (3)
#define MAX_OUTPUT_BUFFER_PLANE_NUM (1)

class ExynosJpegEncoderForCamera {
public :
    ;
    enum ERROR {
        ERROR_ALREADY_CREATE = -0x200,
        ERROR_CANNOT_CREATE_EXYNOS_JPEG_ENC_HAL,
        ERROR_NOT_YET_CREATED,
        ERROR_ALREADY_DESTROY,
        ERROR_INPUT_DATA_SIZE_TOO_LARGE,
        ERROR_OUT_BUFFER_SIZE_TOO_SMALL,
        ERROR_EXIFOUT_ALLOC_FAIL,
        ERROR_MAKE_EXIF_FAIL,
        ERROR_INVALID_SCALING_WIDTH_HEIGHT,
        ERROR_CANNOT_CREATE_SEC_THUMB,
        ERROR_THUMB_JPEG_SIZE_TOO_SMALL,
        ERROR_IMPLEMENT_NOT_YET,
        ERROR_MEM_ALLOC_FAIL,
        ERROR_JPEG_DEVICE_ALREADY_CREATE = -0x100,
        ERROR_CANNOT_OPEN_JPEG_DEVICE,
        ERROR_JPEG_DEVICE_ALREADY_CLOSED,
        ERROR_JPEG_DEVICE_ALREADY_DESTROY,
        ERROR_JPEG_DEVICE_NOT_CREATE_YET,
        ERROR_INVALID_COLOR_FORMAT,
        ERROR_INVALID_JPEG_FORMAT,
        ERROR_JPEG_CONFIG_POINTER_NULL,
        ERROR_INVALID_JPEG_CONFIG,
        ERROR_IN_BUFFER_CREATE_FAIL,
        ERROR_OUT_BUFFER_CREATE_FAIL,
        ERROR_EXCUTE_FAIL,
        ERROR_JPEG_SIZE_TOO_SMALL,
        ERROR_CANNOT_CHANGE_CACHE_SETTING,
        ERROR_SIZE_NOT_SET_YET,
        ERROR_BUFFR_IS_NULL,
        ERROR_BUFFER_TOO_SMALL,
        ERROR_GET_SIZE_FAIL,
        ERROR_REQBUF_FAIL,
        ERROR_INVALID_V4l2_BUF_TYPE = -0x80,
        ERROR_MMAP_FAILED,
        ERROR_FAIL,
        ERROR_NONE = 0
    };

    ExynosJpegEncoderForCamera(bool bBTBComp = false);
    virtual ~ExynosJpegEncoderForCamera();

    bool   flagCreate();
    int     create(void);
    int     destroy(void);

    void    setExtScalerNum(int extScalerNum);
    int     setSize(int w, int h);
    int     setQuality(int quality);
    int     setQuality(const unsigned char q_table[]);
    int     setColorFormat(int colorFormat);
    int     setJpegFormat(int jpegFormat);

    int     updateConfig(void);

    int     setInBuf(int *buf, int *size);
    int     setInBuf2(int *buf, int *size) { return ERROR_NONE; }
    int     setOutBuf(int buf, int size);
    int     setInBuf(char **buf, int *size);
    int     setOutBuf(char *buf, int size);

    void    EnableHWFC(void) {}
    int     encode(int *size, exif_attribute_t *exifInfo, char** pcJpegBuffer, debug_attribute_t *debugInfo = 0);
    ssize_t WaitForCompression(void) { return 0; }

    int     setThumbnailSize(int w, int h);
    int     setThumbnailQuality(int quality);

    int     makeExif(unsigned char *exifOut,
                               exif_attribute_t *exifIn,
                               unsigned int *size,
                               bool useMainbufForThumb = false);

    void    setInBufType(int sel);
    int     getInBufType(void);

private:
    typedef ExynosCameraThread<ExynosJpegEncoderForCamera> JpegMainEncodeThread;

    inline void writeExifIfd(unsigned char **pCur,
                                         unsigned short tag,
                                         unsigned short type,
                                         unsigned int count,
                                         uint32_t value);
    inline void writeExifIfd(unsigned char **pCur,
                                        unsigned short tag,
                                        unsigned short type,
                                        unsigned int count,
                                        unsigned char *pValue);
    inline void writeExifIfd(unsigned char **pCur,
                                         unsigned short tag,
                                         unsigned short type,
                                         unsigned int count,
                                         rational_t *pValue,
                                         unsigned int *offset,
                                         unsigned char *start);
    inline void writeExifIfd(unsigned char **pCur,
                                         unsigned short tag,
                                         unsigned short type,
                                         unsigned int count,
                                         unsigned char *pValue,
                                         unsigned int *offset,
                                         unsigned char *start);
    int     roundToInt(double x);
    int     get_approximate_value(int value, int limit);
    int     scaleDownYuv422(char **srcBuf, unsigned int srcW, unsigned int srcH,
                                                char **dstBuf, unsigned int dstW, unsigned int dstH);
    int     scaleDownYuv422_2p(char **srcBuf, unsigned int srcW, unsigned int srcH,
                                                        char **dstBuf, unsigned int dstW, unsigned int dstH);
    /* thumbnail */
    int     encodeThumbnail(unsigned int *size, bool useMain = true);

    struct stJpegMem {
        int ionClient;
        int ionBuffer[MAX_IMAGE_PLANE_NUM];
        char *pcBuf[MAX_IMAGE_PLANE_NUM];
        int iSize[MAX_IMAGE_PLANE_NUM];
    };

    int     createIonClient(int ionClient);
    int     deleteIonClient(int ionClient);
    int     allocJpegMemory(struct stJpegMem *pstMem, int iMemoryNum);
    void    freeJpegMemory(struct stJpegMem *pstMem, int iMemoryNum);
    void    initJpegMemory(struct stJpegMem *pstMem, int iMemoryNum);

    bool    mmapJpegMemory(int *iFd, char **ppcBuf, int *piSize, int iMemoryNum);
    void    unmapJpegMemory(int *iFd, char **ppcBuf, int *piSize, int iMemoryNum);

    bool    m_JpegMainEncodeThreadFunc(void);
    sp<JpegMainEncodeThread> m_jpegMainEncodeThread;

    bool     m_flagCreate;

    ExynosJpegEncoder *m_jpegMain;
    ExynosJpegEncoder *m_jpegThumb;

    int m_ionJpegClient;
    struct stJpegMem m_stThumbInBuf;
    struct stJpegMem m_stThumbOutBuf;
    struct stJpegMem m_stThumbTempBuf;

    int m_thumbnailW;
    int m_thumbnailH;
    int m_thumbnailQuality;
    void *m_exynosThumbCSC;
    int m_extScalerNum;
};

#endif /* __SEC_JPG_ENC_H__ */
