/*
 * Copyright Samsung Electronics Co.,LTD.
 * Copyright (C) 2015 The Android Open Source Project
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

#define LOG_TAG "SecCameraDngCreator"

#include <utils/Log.h>

#include "ExynosCameraParameters.h"
#include "SecCameraDngCreator.h"

namespace android {

#ifdef SAMSUNG_DNG

SecCameraDngCreator::SecCameraDngCreator()
{
}

SecCameraDngCreator::~SecCameraDngCreator()
{
}

int SecCameraDngCreator::makeDng(ExynosCameraParameters *param,
                                char* dngBuffer,
                                unsigned int *rawSize)
{
    int ret = NO_ERROR;
    int nDngSize = 0;
    char *rawBuffer;
    unsigned char *dngHeaderOut = NULL;
    dng_attribute_t* dngInfo;

    ALOGD("DEBUG(%s[%d]): make DNG file", __FUNCTION__, __LINE__);

    param->setIsUsefulDngInfo(false);
    dngInfo = param->getDngInfo();

    if (dngBuffer == NULL) {
        ALOGE("ERR(%s):dngBuffer is null!!", __FUNCTION__);
        return NO_MEMORY;
    }

    rawBuffer = dngBuffer;

    unsigned int dngHeaderLen = 0;
    unsigned int bufSize = DNG_HEADER_FILE_SIZE;

    dngHeaderOut = new unsigned char[bufSize];
    if (dngHeaderOut == NULL) {
        ALOGE("ERR(%s):Failed to allocate for dngHeaderOut", __FUNCTION__);
        return NO_MEMORY;
    }
    memset(dngHeaderOut, 0, bufSize);

    if (makeDngHeader(dngHeaderOut, dngInfo, &dngHeaderLen)) {
        ALOGE("ERR(%s):Failed to make DNG header", __FUNCTION__);
        ret = INVALID_OPERATION;
        goto CLEAN_MEMORY;
    }

    nDngSize = dngInfo->image_width * dngInfo->image_length * 2;

    if (nDngSize <= 0) {
        ALOGE("ERR(%s):output_size is too small(%d)!!", __FUNCTION__, nDngSize);
        ret = BAD_VALUE;
        goto CLEAN_MEMORY;
    }

    if (dngHeaderLen <= DNG_HEADER_LIMIT_SIZE) {
        memmove(dngBuffer + dngHeaderLen, rawBuffer, nDngSize);
        if (dngHeaderLen <= bufSize) {
            memcpy(dngBuffer, dngHeaderOut, dngHeaderLen);
            nDngSize += dngHeaderLen;
        } else {
            ALOGE("ERR(%s):dngHeaderLen(%d) is too bigger than DNG_HEADER_FILE_SIZE(%d)",
                __FUNCTION__, dngHeaderLen, DNG_HEADER_FILE_SIZE);
            ret = BAD_VALUE;
        }
    } else {
        ALOGE("ERR(%s):dngHeaderLen(%d) is too bingger than DNG_HEADER_LIMIT_SIZE(%d)",
            __FUNCTION__, dngHeaderLen, DNG_HEADER_LIMIT_SIZE);
        ret = BAD_VALUE;
    }

CLEAN_MEMORY:
    *rawSize = nDngSize;
    delete[] dngHeaderOut;

    return ret;
}

int SecCameraDngCreator::makeDngHeader (unsigned char *dngHeaderOut,
                              dng_attribute_t *dngInfo,
                              unsigned int *size,
                              bool useMainbufForThumb)
{
    unsigned char *pCur, *pIfdStart;
    unsigned int numIFD, LongerTagOffest = 0, exifIFDOffset;
    pIfdStart = pCur = dngHeaderOut;

    /* Byte Order - little endian, Offset of IFD - 0x00000008.H */
    unsigned char TiffHeader[8] = { 0x49, 0x49, 0x2A, 0x00, 0x08, 0x00, 0x00, 0x00 };
    memcpy(pCur, TiffHeader, 8);
    pCur += 8;

    /* 0th IFD TIFF Tags */
    numIFD = NUM_0TH_IFD;
    memcpy(pCur, &numIFD, NUM_SIZE);
    pCur += NUM_SIZE;

    LongerTagOffest += 8 + (NUM_SIZE * 2) + (NUM_0TH_IFD + NUM_EXIF_IFD) * IFD_SIZE + (DNG_OFFSET_SIZE * 2);
    exifIFDOffset = 8 + NUM_SIZE + (NUM_0TH_IFD * IFD_SIZE) + DNG_OFFSET_SIZE;

    writeTiffIfd(&pCur, TIFF_TAG_NEW_SUBFILE_TYPE, TIFF_TYPES_LONG,
                1, (unsigned int)0);
    writeTiffIfd(&pCur, TIFF_TAG_IMAGE_WIDTH, TIFF_TYPES_LONG,
                1, dngInfo->image_width);
    writeTiffIfd(&pCur, TIFF_TAG_IMAGE_LENGTH, TIFF_TYPES_LONG,
                1, dngInfo->image_length);
    writeTiffIfd(&pCur, TIFF_TAG_BITS_PER_SAMPLE, TIFF_TYPES_SHORT,
                1, dngInfo->bits_per_sample);
    writeTiffIfd(&pCur, TIFF_TAG_COMPRESSION, TIFF_TYPES_SHORT,
                1, dngInfo->compression);
    writeTiffIfd(&pCur, TIFF_TAG_PHOTOMETRIC_INTERPRETATION, TIFF_TYPES_SHORT,
                1, dngInfo->photometric_interpretation);
    writeTiffIfd(&pCur, TIFF_TAG_IMAGE_DESCRIPTION, TIFF_TYPES_ASCII,
                1, (unsigned int)0);
    writeTiffIfd(&pCur, TIFF_TAG_MAKE, TIFF_TYPES_ASCII,
                strlen((char *)dngInfo->make) + 1, dngInfo->make, &LongerTagOffest, pIfdStart);
    writeTiffIfd(&pCur, TIFF_TAG_MODEL, TIFF_TYPES_ASCII,
                strlen((char *)dngInfo->model) + 1, dngInfo->model, &LongerTagOffest, pIfdStart);
    setStripOffset(&pCur, TIFF_TAG_STRIP_OFFSETS, TIFF_TYPES_LONG,
                dngInfo->image_length, DNG_HEADER_FILE_SIZE, &LongerTagOffest, pIfdStart, dngInfo->image_width * 2);
    writeTiffIfd(&pCur, TIFF_TAG_ORIENTATION, TIFF_TYPES_SHORT,
                1, dngInfo->orientation);
    writeTiffIfd(&pCur, TIFF_TAG_SAMPLES_PER_PIXEL, TIFF_TYPES_SHORT,
                1, dngInfo->samples_per_pixel);
    writeTiffIfd(&pCur, TIFF_TAG_ROWS_PER_STRIP, TIFF_TYPES_LONG,
                1, dngInfo->rows_per_strip);
    setStripByteCount(&pCur, TIFF_TAG_STRIP_BYTE_COUNTS, TIFF_TYPES_LONG,
                dngInfo->image_length, dngInfo->image_width * 2, &LongerTagOffest, pIfdStart);
    writeTiffIfd(&pCur, TIFF_TAG_X_RESOLUTION, TIFF_TYPES_RATIONAL,
                1, (rational_t *)&dngInfo->x_resolution, &LongerTagOffest, pIfdStart);
    writeTiffIfd(&pCur, TIFF_TAG_Y_RESOLUTION, TIFF_TYPES_RATIONAL,
                1, (rational_t *)&dngInfo->y_resolution, &LongerTagOffest, pIfdStart);
    writeTiffIfd(&pCur, TIFF_TAG_PLANAR_CONFIGURATION, TIFF_TYPES_SHORT,
                1, dngInfo->planar_configuration);
    writeTiffIfd(&pCur, TIFF_TAG_RESOLUTION_UNIT, TIFF_TYPES_SHORT,
                1, dngInfo->resolution_unit);
    writeTiffIfd(&pCur, TIFF_TAG_SOFTWARE, TIFF_TYPES_ASCII,
                strlen((char *)dngInfo->software) + 1, dngInfo->software, &LongerTagOffest, pIfdStart);
    writeTiffIfd(&pCur, TIFF_TAG_DATE_TIME, TIFF_TYPES_ASCII,
                strlen((char *)dngInfo->date_time) + 1, dngInfo->date_time, &LongerTagOffest, pIfdStart);
    writeTiffIfd(&pCur, TIFF_TAG_CFA_REPEAT_PATTERN_DM, TIFF_TYPES_SHORT,
                2, 0x20002);
    writeTiffIfd(&pCur, TIFF_TAG_CFA_PATTERN, TIFF_TYPES_BYTE,
                4, dngInfo->cfa_pattern);
    writeTiffIfd(&pCur, TIFF_TAG_COPYRIGHT, TIFF_TYPES_ASCII,
                1, (unsigned int)0);
    writeTiffIfd(&pCur, TIFF_TAG_EXIF_IFD, TIFF_TYPES_LONG,
                1, exifIFDOffset);
    writeTiffIfd(&pCur, TIFF_TAG_TIFF_EP_STANDARD_ID, TIFF_TYPES_BYTE,
                4, dngInfo->tiff_ep_standard_id);
    writeTiffIfd(&pCur, TIFF_TAG_DNG_VERSION, TIFF_TYPES_BYTE,
                4, dngInfo->dng_version);
    writeTiffIfd(&pCur, TIFF_TAG_DNG_BACKWARD_VERSION, TIFF_TYPES_BYTE,
                4, dngInfo->dng_backward_version);
    writeTiffIfd(&pCur, TIFF_TAG_UNIQUE_CAMERA_MODEL, TIFF_TYPES_ASCII,
                strlen((char *)dngInfo->unique_camera_model) + 1, dngInfo->unique_camera_model, &LongerTagOffest, pIfdStart);
    writeTiffIfd(&pCur, TIFF_TAG_CFA_PLANE_COLOR, TIFF_TYPES_BYTE,
                3, dngInfo->cfa_plane_color);
    writeTiffIfd(&pCur, TIFF_TAG_CFA_LAYOUT, TIFF_TYPES_SHORT,
                1, dngInfo->cfa_layout);
    writeTiffIfd(&pCur, TIFF_TAG_BLACK_LEVEL_REPEAT_DM, TIFF_TYPES_SHORT,
                2, 0x20002);
    writeTiffIfd(&pCur, TIFF_TAG_BLACK_LEVEL, TIFF_TYPES_LONG,
                4, dngInfo->black_level_repeat, &LongerTagOffest, pIfdStart);
    writeTiffIfd(&pCur, TIFF_TAG_WHITE_LEVEL, TIFF_TYPES_LONG,
                1, dngInfo->white_level);
    writeTiffIfd(&pCur, TIFF_TAG_DEFAULT_SCALE, TIFF_TYPES_RATIONAL,
                2, (rational_t *)dngInfo->default_scale, &LongerTagOffest, pIfdStart);
    writeTiffIfd(&pCur, TIFF_TAG_DEFAULT_CROP_ORIGIN, TIFF_TYPES_LONG,
                2, dngInfo->default_crop_origin, &LongerTagOffest, pIfdStart);
    writeTiffIfd(&pCur, TIFF_TAG_DEFAULT_CROP_SIZE, TIFF_TYPES_LONG,
                2, dngInfo->default_crop_size, &LongerTagOffest, pIfdStart);
    writeTiffIfd(&pCur, TIFF_TAG_COLOR_MATRIX1, TIFF_TYPES_SRATIONAL,
                9, (rational_t *)dngInfo->color_matrix1, &LongerTagOffest, pIfdStart);
    writeTiffIfd(&pCur, TIFF_TAG_COLOR_MATRIX2, TIFF_TYPES_SRATIONAL,
                9, (rational_t *)dngInfo->color_matrix2, &LongerTagOffest, pIfdStart);
    writeTiffIfd(&pCur, TIFF_TAG_CAMERA_CALIBRATION1, TIFF_TYPES_SRATIONAL,
                9, (rational_t *)dngInfo->camera_calibration1, &LongerTagOffest, pIfdStart);
    writeTiffIfd(&pCur, TIFF_TAG_CAMERA_CALIBRATION2, TIFF_TYPES_SRATIONAL,
                9, (rational_t *)dngInfo->camera_calibration2, &LongerTagOffest, pIfdStart);
    writeTiffIfd(&pCur, TIFF_TAG_AS_SHOT_NEUTRAL, TIFF_TYPES_RATIONAL,
                3, (rational_t *)dngInfo->as_shot_neutral, &LongerTagOffest, pIfdStart);
    writeTiffIfd(&pCur, TIFF_TAG_CALIBRATION_ILLUMINANT1, TIFF_TYPES_SHORT,
                1, dngInfo->calibration_illuminant1);
    writeTiffIfd(&pCur, TIFF_TAG_CALIBRATION_ILLUMINANT2, TIFF_TYPES_SHORT,
                1, dngInfo->calibration_illuminant2);
    writeTiffIfd(&pCur, TIFF_TAG_FORWARD_MATRIX1, TIFF_TYPES_SRATIONAL,
                9, (rational_t *)dngInfo->forward_matrix1, &LongerTagOffest, pIfdStart);
    writeTiffIfd(&pCur, TIFF_TAG_FORWARD_MATRIX2, TIFF_TYPES_SRATIONAL,
                9, (rational_t *)dngInfo->forward_matrix2, &LongerTagOffest, pIfdStart);
    writeTiffIfd(&pCur, TIFF_TAG_OPCODE_LIST2, TIFF_TYPES_UNDEFINED,
                sizeof(dngInfo->opcode_list2), dngInfo->opcode_list2, &LongerTagOffest, pIfdStart);
    writeTiffIfd(&pCur, TIFF_TAG_NOISE_PROFILE, TIFF_TYPES_DOUBLE,
                6, dngInfo->noise_profile, &LongerTagOffest, pIfdStart);

    unsigned char endOfIFD[4] = { 0x00, 0x00, 0x00, 0x00 };
    memcpy(pCur, endOfIFD, 4);
    pCur += 4;

    /* EXIF IFD TIFF Tags */
    numIFD = NUM_EXIF_IFD;
    memcpy(pCur, &numIFD, NUM_SIZE);
    pCur += NUM_SIZE;

    writeTiffIfd(&pCur, TIFF_TAG_EXPOSURE_TIME, TIFF_TYPES_RATIONAL,
                1, (rational_t *)&dngInfo->exposure_time, &LongerTagOffest, pIfdStart);
    writeTiffIfd(&pCur, TIFF_TAG_F_NUMBER, TIFF_TYPES_RATIONAL,
                1, (rational_t *)&dngInfo->f_number, &LongerTagOffest, pIfdStart);
    writeTiffIfd(&pCur, TIFF_TAG_ISO_SPEED_RATINGS, TIFF_TYPES_SHORT,
                1, dngInfo->iso_speed_ratings);
    writeTiffIfd(&pCur, TIFF_TAG_EXIF_VERSION, TIFF_TYPES_UNDEFINED,
                4, dngInfo->exif_version);
    writeTiffIfd(&pCur, TIFF_TAG_DATE_TIME_ORIGINAL, TIFF_TYPES_ASCII,
                strlen((char *)dngInfo->date_time_original) + 1, dngInfo->date_time_original, &LongerTagOffest, pIfdStart);
    writeTiffIfd(&pCur, TIFF_TAG_FOCAL_LENGTH, TIFF_TYPES_RATIONAL,
                1, (rational_t *)&dngInfo->focal_length, &LongerTagOffest, pIfdStart);

    memcpy(pCur, endOfIFD, 4);
    pCur += 4;

    // 16-align memory
    LongerTagOffest = ((((LongerTagOffest + (1 << 5)) - 1) >> 5) << 5);
    *size = DNG_HEADER_FILE_SIZE;//LongerTagOffest;

    return NO_ERROR;
}

/*
 * private member functions
*/
inline void SecCameraDngCreator::setStripOffset(unsigned char **pCur,
                                             unsigned short tag,
                                             unsigned short type,
                                             unsigned int count,
                                             unsigned int value,
                                             unsigned int *offset,
                                             unsigned char *start,
                                             unsigned int interval)
{
    int i;

    memcpy(*pCur, &tag, 2);
    *pCur += 2;
    memcpy(*pCur, &type, 2);
    *pCur += 2;
    memcpy(*pCur, &count, 4);
    *pCur += 4;
    memcpy(*pCur, offset, 4);
    *pCur += 4;

    for(i=0; i<count; i++) {
        memcpy(start + *offset, &value, 4);
        *offset += 4;
        value += interval;
    }
}

inline void SecCameraDngCreator::setStripByteCount(unsigned char **pCur,
                                             unsigned short tag,
                                             unsigned short type,
                                             unsigned int count,
                                             unsigned int value,
                                             unsigned int *offset,
                                             unsigned char *start)
{
    int i;

    memcpy(*pCur, &tag, 2);
    *pCur += 2;
    memcpy(*pCur, &type, 2);
    *pCur += 2;
    memcpy(*pCur, &count, 4);
    *pCur += 4;
    memcpy(*pCur, offset, 4);
    *pCur += 4;

    for(i=0; i<count; i++) {
        memcpy(start + *offset, &value, 4);
        *offset += 4;
    }
}

inline void SecCameraDngCreator::writeTiffIfd(unsigned char **pCur,
                                             unsigned short tag,
                                             unsigned short type,
                                             unsigned int count,
                                             unsigned int value)
{
    memcpy(*pCur, &tag, 2);
    *pCur += 2;
    memcpy(*pCur, &type, 2);
    *pCur += 2;
    memcpy(*pCur, &count, 4);
    *pCur += 4;
    memcpy(*pCur, &value, 4);
    *pCur += 4;
}

inline void SecCameraDngCreator::writeTiffIfd(unsigned char **pCur,
                                             unsigned short tag,
                                             unsigned short type,
                                             unsigned int count,
                                             unsigned char *pValue)
{
    char buf[4] = { 0,};

    memcpy(buf, pValue, count);
    memcpy(*pCur, &tag, 2);
    *pCur += 2;
    memcpy(*pCur, &type, 2);
    *pCur += 2;
    memcpy(*pCur, &count, 4);
    *pCur += 4;
    memcpy(*pCur, buf, 4);
    *pCur += 4;
}

inline void SecCameraDngCreator::writeTiffIfd(unsigned char **pCur,
                                             unsigned short tag,
                                             unsigned short type,
                                             unsigned int count,
                                             unsigned char *pValue,
                                             unsigned int *offset,
                                             unsigned char *start)
{
    memcpy(*pCur, &tag, 2);
    *pCur += 2;
    memcpy(*pCur, &type, 2);
    *pCur += 2;
    memcpy(*pCur, &count, 4);
    *pCur += 4;
    memcpy(*pCur, offset, 4);
    *pCur += 4;
    memcpy(start + *offset, pValue, count);
    *offset += count;
    *offset += (4 - count % 4) % 4;
}

inline void SecCameraDngCreator::writeTiffIfd(unsigned char **pCur,
                                             unsigned short tag,
                                             unsigned short type,
                                             unsigned int count,
                                             unsigned int *pValue,
                                             unsigned int *offset,
                                             unsigned char *start)
{
    memcpy(*pCur, &tag, 2);
    *pCur += 2;
    memcpy(*pCur, &type, 2);
    *pCur += 2;
    memcpy(*pCur, &count, 4);
    *pCur += 4;
    memcpy(*pCur, offset, 4);
    *pCur += 4;
    memcpy(start + *offset, pValue, count * 4);
    *offset += (count * 4);
}

inline void SecCameraDngCreator::writeTiffIfd(unsigned char **pCur,
                                             unsigned short tag,
                                             unsigned short type,
                                             unsigned int count,
                                             double *pValue,
                                             unsigned int *offset,
                                             unsigned char *start)
{
    memcpy(*pCur, &tag, 2);
    *pCur += 2;
    memcpy(*pCur, &type, 2);
    *pCur += 2;
    memcpy(*pCur, &count, 4);
    *pCur += 4;
    memcpy(*pCur, offset, 4);
    *pCur += 4;
    memcpy(start + *offset, pValue, count * 8);
    *offset += (count * 8);
}

inline void SecCameraDngCreator::writeTiffIfd(unsigned char **pCur,
                                             unsigned short tag,
                                             unsigned short type,
                                             unsigned int count,
                                             rational_t *pValue,
                                             unsigned int *offset,
                                             unsigned char *start)
{
    memcpy(*pCur, &tag, 2);
    *pCur += 2;
    memcpy(*pCur, &type, 2);
    *pCur += 2;
    memcpy(*pCur, &count, 4);
    *pCur += 4;
    memcpy(*pCur, offset, 4);
    *pCur += 4;
    memcpy(start + *offset, pValue, 8 * count);
    *offset += 8 * count;
}

#endif /*SUPPORT_SAMSUNG_DNG*/

}; /* namespace android */