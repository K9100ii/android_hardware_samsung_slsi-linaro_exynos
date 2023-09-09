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

 /* #define LOG_NDEBUG 0 */
#define LOG_TAG "ExynosJpegEncoderForCameraSec"

#include "ExynosCameraConfig.h"
#include "ExynosJpegEncoderForCamera.h"

#ifdef USE_CSC_FEATURE
#include <cutils/properties.h>
#include <SecNativeFeature.h>
#endif
#ifdef SAMSUNG_TN_FEATURE
#include "SecAppMarker.h"
#endif

int iJpegSize;

unsigned int measure_time(struct timeval *start, struct timeval *stop)
{
    unsigned long sec, usec, time;

    sec = stop->tv_sec - start->tv_sec;

    if (stop->tv_usec >= start->tv_usec) {
        usec = stop->tv_usec - start->tv_usec;
    } else {
        usec = stop->tv_usec + 1000000 - start->tv_usec;
        sec--;
    }

    time = (sec * 1000000) + usec;

    return time;
}

int ExynosJpegEncoderForCamera::encode(int *size, exif_attribute_t *exifInfo, char** pcJpegBuffer, debug_attribute_t *debugInfo)
{
    int ret = ERROR_NONE;
    unsigned char *exifOut = NULL;
    char *debugOut = NULL;
    struct timeval start, end;

    if (m_flagCreate == false) {
        CLOGE2("not yet created. so, fail");
        return ERROR_NOT_YET_CREATED;
    }

    gettimeofday(&start, NULL);
#if 0
    ret = m_jpegMain->encode();
    if (ret) {
        CLOGE2("encode() fail");
        return ret;
    }

    int iJpegSize = m_jpegMain->getJpegSize();

    if (iJpegSize<=0) {
        CLOGE2(" output_size is too small(%d)!!", iJpegSize);
        return ERROR_OUT_BUFFER_SIZE_TOO_SMALL;
    }
#endif

    CLOGD2("JPEG main encoder start");
    m_jpegMainEncodeThread->run(PRIORITY_DEFAULT);

    if (!(m_jpegMain->checkInBufType() & JPEG_BUF_TYPE_USER_PTR
        || m_jpegMain->checkInBufType() & JPEG_BUF_TYPE_DMA_BUF)) {
        CLOGE2(" invalid buffer type (%d) fail", m_jpegMain->checkInBufType());
        return ERROR_BUFFR_IS_NULL;
    }

    if (*pcJpegBuffer == NULL) {
        CLOGE2("pcJpegBuffer is null!!");
        return ERROR_OUT_BUFFER_CREATE_FAIL;
    }

    if (exifInfo != NULL) {
        unsigned int thumbLen = 0;
        unsigned int exifLen = 0;
        unsigned int bufSize = 0;

        if (exifInfo->enableThumb) {
            CLOGD2("JPEG thumbnail encoder enabled");
            if (encodeThumbnail(&thumbLen)) {
                CLOGE2("encodeThumbnail() fail");
                bufSize = EXIF_FILE_SIZE;
                exifInfo->enableThumb = false;
            } else {
                if (thumbLen > EXIF_LIMIT_SIZE) {
                    CLOGE2("thumbLen(%d) is too bigger than EXIF_LIMIT_SIZE(%d)",
                             thumbLen, EXIF_LIMIT_SIZE);

                    bufSize = EXIF_FILE_SIZE;
                    exifInfo->enableThumb = false;
                } else {
                    CLOGD2("JPEG thumbnail encoder done, size=%d", thumbLen);
                    bufSize = EXIF_FILE_SIZE + thumbLen;
                }
            }
        } else {
            bufSize = EXIF_FILE_SIZE;
            exifInfo->enableThumb = false;
        }

        int totalDbgSize = 0;
        if (debugInfo != NULL && debugInfo->num_of_appmarker > 0) {
               for(int i = 0; i < debugInfo->num_of_appmarker; i++) {
                    bufSize += debugInfo->debugSize[debugInfo->idx[i][0]];
                    totalDbgSize += debugInfo->debugSize[debugInfo->idx[i][0]];
                    bufSize += 4;
               }
        }

        exifOut = new unsigned char[bufSize];
        if (exifOut == NULL) {
            CLOGE2("Failed to allocate for exifOut");
            delete[] exifOut;
            return ERROR_EXIFOUT_ALLOC_FAIL;
        }
        memset(exifOut, 0, bufSize);

        if (makeExif (exifOut, exifInfo, &exifLen)) {
            CLOGE2("Failed to make EXIF");
            delete[] exifOut;
            return ERROR_MAKE_EXIF_FAIL;
        }

        /* Exif info size is overflow */
        if (exifLen - thumbLen > EXIF_INFO_LIMIT_SIZE) {
        //    CLOGE2("Exif info size(%d) is bigger than EXIF_INFO_LIMIT_SIZE(%d)",
        //             exifLen - thumbLen, EXIF_INFO_LIMIT_SIZE);
        }

#ifdef SAMSUNG_DEBUG_MARKER
        if (debugInfo != NULL && debugInfo->num_of_appmarker > 0) {
            SecAppMarker *m_debugMarker;
            char *startDbgAddr = NULL;
            m_debugMarker = new SecAppMarker;
            m_debugMarker->setExifStartAddr((char *)exifOut, exifLen);
            startDbgAddr = debugInfo->debugData[debugInfo->idx[0][0]];
            m_debugMarker->setDebugStartAddr(startDbgAddr, totalDbgSize);
            m_debugMarker->pushDebugData((void *)debugInfo);
            debugOut = m_debugMarker->getResultData();
            exifLen = m_debugMarker->getResultDataSize();
            CLOGD2("AppMarker exif + debug size = %d", exifLen);
            memcpy((void *)exifOut, (void *)debugOut, exifLen);
            m_debugMarker->destroy();
            delete m_debugMarker;
            m_debugMarker = NULL;
        }
#endif
        CLOGD2("wait JPEG main encoder");
        m_jpegMainEncodeThread->join();

        iJpegSize = m_jpegMain->getJpegSize();

        if (iJpegSize<=0) {
            CLOGE2("output_size is too small(%d)!!", iJpegSize);
            delete[] exifOut;
            return ERROR_OUT_BUFFER_SIZE_TOO_SMALL;
        }

        CLOGD2("JPEG main encoder done, size(%d)", iJpegSize);

        if (exifLen <= EXIF_LIMIT_SIZE || (debugInfo != NULL && debugInfo->num_of_appmarker
            && exifLen <= EXIF_LIMIT_SIZE + totalDbgSize)) {
            memmove(*pcJpegBuffer + exifLen + 2, *pcJpegBuffer + 2, iJpegSize - 2);

            if (exifLen <= bufSize) {
                memcpy(*pcJpegBuffer + 2, exifOut, exifLen);
                iJpegSize += exifLen;

                // elimination of trailing dummy bytes after EOI
                size_t orgsize = iJpegSize;
                while ((iJpegSize > 1) &&
                        ((*(*pcJpegBuffer + iJpegSize - 1) != 0xD9) || (*(*pcJpegBuffer + iJpegSize - 2) != 0xFF)))
                    iJpegSize--;
                if (orgsize != iJpegSize) {
                    CLOGD2("JPEG returned %zu but EOI ends at %zu", orgsize, iJpegSize);
                }
            } else {
                CLOGE2("exifLen(%d) is too bigger than EXIF_FILE_SIZE(%d)",
                       exifLen, EXIF_FILE_SIZE);
            }
        } else {
            CLOGE2("exifLen(%d) is too bingger than EXIF_LIMIT_SIZE(%d)",
               exifLen, EXIF_LIMIT_SIZE);
        }

        delete[] exifOut;
    }

    *size = iJpegSize;

    gettimeofday(&end, NULL);
    CLOGI2("Total JPEG Encoding Time (us) = %d\n",
            measure_time(&start, &end));

    return ERROR_NONE;
}
