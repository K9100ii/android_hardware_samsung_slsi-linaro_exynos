/*
 * Copyright Samsung Electronics Co.,LTD.
 * Copyright (C) 2010 The Android Open Source Project
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
#ifndef SEC_CAMERA_DNG_H_
#define SEC_CAMERA_DNG_H_

#include <math.h>
#include "fimc-is-metadata.h"

namespace android {

#ifdef SAMSUNG_DNG

typedef struct _dng_thumbnail_t {
    char *buf;
    int size;
    int frameCount;
    struct _dng_thumbnail_t *prev;
    struct _dng_thumbnail_t *next;
} dng_thumbnail_t;

typedef struct {
    uint32_t new_subfile_type;
    uint32_t image_width;
    uint32_t image_height;
    uint16_t bits_per_sample;
    uint16_t compression;
    uint16_t photometric_interpretation;
    uint8_t image_description[1];
    uint8_t make[32];
    uint8_t model[32];
    uint32_t strip_offsets;
    uint16_t orientation;
    uint16_t samples_per_pixel;
    uint32_t rows_per_strip;
    uint32_t strip_byte_counts;
    rational x_resolution;
    rational y_resolution;
    uint16_t planar_configuration;
    uint16_t resolution_unit;
    uint8_t software[0X46];
    uint8_t date_time[20];
    uint16_t cfa_repeat_pattern_dm[2];
    uint8_t cfa_pattern[4];
    uint8_t copyright;
    rational exposure_time;
    rational f_number;
    uint16_t iso_speed_ratings;
    uint8_t date_time_original[20];
    rational focal_length;
    uint8_t tiff_ep_standard_id[4];
    uint8_t dng_version[4];
    uint8_t dng_backward_version[4];
    uint8_t unique_camera_model[32];
    uint8_t cfa_plane_color[3];
    uint16_t cfa_layout;
    uint16_t black_level_repeat_dim[2];
    uint32_t black_level_repeat[4];
    uint32_t white_level;
    rational default_scale[2];
    uint32_t default_crop_origin[2];
    uint32_t default_crop_size[2];
    rational color_matrix1[9];
    rational color_matrix2[9];
    rational camera_calibration1[9];
    rational camera_calibration2[9];
    rational as_shot_neutral[3];
    uint16_t calibration_illuminant1;
    uint16_t calibration_illuminant2;
    rational forward_matrix1[9];
    rational forward_matrix2[9];
    uint8_t opcode_list2[0x184];
    double noise_profile[8];
    uint32_t thumbnail_image_width;
    uint32_t thumbnail_image_height;
    uint32_t thumbnail_bits_per_sample[3];
    uint32_t thumbnail_rows_per_strip;
    uint32_t thumbnail_offset;
    uint32_t thumbnail_size;
    uint8_t exif_version[4];
} dng_attribute_t;

#define DNG_HEADER_LIMIT_SIZE                   64*1024
#define DNG_HEADER_FILE_SIZE                    0x6600

#define NUM_SIZE                                2
#define IFD_SIZE                                12
#define DNG_OFFSET_SIZE                         4
#define NUM_0TH_IFD                             0x30
#define NUM_EXIF_IFD                            0x06
#define NUM_1TH_IFD                             0x0B
#define TIFF_HEADER_SIZE                        0x8
#define DIRECTORY_SIZE                          0xC
#define DIRECTORY_TAG_SIZE                      0x2
#define DIRECTORY_TYPE_SIZE                     0x2
#define DIRECTORY_COUNT_SIZE                    0x4
#define DIRECTORY_VALUE_SIZE                    0x4
#define BYTE_ORDER_OFFSET                       0x0
#define BYTE_ORDER_SIZE                         0x2
#define TIFF_FORMAT_OFFSET                      0x4
#define TIFF_FORMAT_SIZE                        0x2
#define IFD0_OFFSET                             0x4
#define IFD0_OFFSET_SIZE                        0x4

/* Type */
#define TIFF_TYPES_BYTE                         0x1
#define TIFF_TYPES_ASCII                        0x2
#define TIFF_TYPES_SHORT                        0x3
#define TIFF_TYPES_LONG                         0x4
#define TIFF_TYPES_RATIONAL                     0x5
#define TIFF_TYPES_SBYTE                        0x6
#define TIFF_TYPES_UNDEFINED                    0x7
#define TIFF_TYPES_SSHORT                       0x8
#define TIFF_TYPES_SLONG                        0x9
#define TIFF_TYPES_SRATIONAL                    0xA
#define TIFF_TYPES_FLOAT                        0xB
#define TIFF_TYPES_DOUBLE                       0xC

/* TIFF Tags */
#define TIFF_TAG_NEW_SUBFILE_TYPE               0x00FE
#define TIFF_TAG_IMAGE_WIDTH                    0x0100
#define TIFF_TAG_IMAGE_LENGTH                   0x0101
#define TIFF_TAG_BITS_PER_SAMPLE                0x0102
#define TIFF_TAG_COMPRESSION                    0x0103
#define TIFF_TAG_PHOTOMETRIC_INTERPRETATION     0x0106
#define TIFF_TAG_IMAGE_DESCRIPTION              0x010E
#define TIFF_TAG_MAKE                           0x010F
#define TIFF_TAG_MODEL                          0x0110
#define TIFF_TAG_STRIP_OFFSETS                  0x0111
#define TIFF_TAG_ORIENTATION                    0x0112
#define TIFF_TAG_SAMPLES_PER_PIXEL              0x0115
#define TIFF_TAG_ROWS_PER_STRIP                 0x0116
#define TIFF_TAG_STRIP_BYTE_COUNTS              0x0117
#define TIFF_TAG_X_RESOLUTION                   0x011A
#define TIFF_TAG_Y_RESOLUTION                   0x011B
#define TIFF_TAG_PLANAR_CONFIGURATION           0x011C
#define TIFF_TAG_RESOLUTION_UNIT                0x0128
#define TIFF_TAG_SOFTWARE                       0x0131
#define TIFF_TAG_DATE_TIME                      0x0132
#define TIFF_TAG_SUB_IFDS                       0x014A
#define TIFF_TAG_CFA_REPEAT_PATTERN_DM          0x828D
#define TIFF_TAG_CFA_PATTERN                    0x828E
#define TIFF_TAG_COPYRIGHT                      0x8298
#define TIFF_TAG_EXPOSURE_TIME                  0x829A
#define TIFF_TAG_F_NUMBER                       0x829D
#define TIFF_TAG_EXIF_IFD                       0x8769
#define TIFF_TAG_ISO_SPEED_RATINGS              0x8827
#define TIFF_TAG_EXIF_VERSION                   0x9000
#define TIFF_TAG_DATE_TIME_ORIGINAL             0x9003
#define TIFF_TAG_FOCAL_LENGTH                   0x920A
#define TIFF_TAG_TIFF_EP_STANDARD_ID            0x9216
#define TIFF_TAG_DNG_VERSION                    0xC612
#define TIFF_TAG_DNG_BACKWARD_VERSION           0xC613
#define TIFF_TAG_UNIQUE_CAMERA_MODEL            0xC614
#define TIFF_TAG_CFA_PLANE_COLOR                0xC616
#define TIFF_TAG_CFA_LAYOUT                     0xC617
#define TIFF_TAG_BLACK_LEVEL_REPEAT_DM          0xC619
#define TIFF_TAG_BLACK_LEVEL                    0xC61A
#define TIFF_TAG_WHITE_LEVEL                    0xC61D
#define TIFF_TAG_DEFAULT_SCALE                  0xC61E
#define TIFF_TAG_DEFAULT_CROP_ORIGIN            0xC61F
#define TIFF_TAG_DEFAULT_CROP_SIZE              0xC620
#define TIFF_TAG_COLOR_MATRIX1                  0xC621
#define TIFF_TAG_COLOR_MATRIX2                  0xC622
#define TIFF_TAG_CAMERA_CALIBRATION1            0xC623
#define TIFF_TAG_CAMERA_CALIBRATION2            0xC624
#define TIFF_TAG_AS_SHOT_NEUTRAL                0xC628
#define TIFF_TAG_CALIBRATION_ILLUMINANT1        0xC65A
#define TIFF_TAG_CALIBRATION_ILLUMINANT2        0xC65B
#define TIFF_TAG_FORWARD_MATRIX1                0xC714
#define TIFF_TAG_FORWARD_MATRIX2                0xC715
#define TIFF_TAG_OPCODE_LIST2                   0xC741
#define TIFF_TAG_NOISE_PROFILE                  0xC761

/* Values */
#define TIFF_DEF_MAKE                           "samsung"
#define TIFF_DEF_MODEL                          "SAMSUNG"
#define TIFF_DEF_SOFTWARE                       "SAMSUNG"
#define TIFF_DEF_SAMPLES_PER_PIXEL              1
#define TIFF_DEF_COMPRESSION                    1
#define TIFF_DEF_RESOLUTION_NUM                 72
#define TIFF_DEF_RESOLUTION_DEN                 1
#define TIFF_DEF_RESOLUTION_UNIT                2   /* inches */
#define TIFF_DEF_CFA_LAYOUT                     1
#define TIFF_DEF_BITS_PER_SAMPLE                0x0010
#define TIFF_DEF_ROWS_PER_STRIP                 0x1
#define TIFF_DEF_PHOTOMETRIC_INTERPRETATION     0x8023
#define TIFF_DEF_PLANAR_CONFIGURATION           0x0001

const uint8_t TIFF_DEF_TIFF_EP_STANDARD_ID[4] = {0x01, 0x00, 0x00, 0x00};
const uint8_t TIFF_DEF_DNG_VERSION[4] = {0x01, 0x04, 0x00, 0x00};
const uint8_t TIFF_DEF_DNG_BACKWARD_VERSION[4] = {0x01, 0x01, 0x00, 0x00};
const uint8_t TIFF_DEF_CFA_PLANE_COLOR[3] = {0x00, 0x01, 0x02};
const uint16_t TIFF_DEF_CFA_REPEAT_PATTERN_DM[2] = {0x0002, 0x0002};
const uint8_t TIFF_DEF_CFA_PATTERN[4] = {0x01, 0x00, 0x02, 0x01};
const uint16_t TIFF_DEF_BLACK_LEVEL_REPEAT_DIM[2] = {0x0002, 0x0002};
const rational TIFF_DEF_DEFAULT_SCALE[2] = {{1,1}, {1,1}};
const uint32_t TIFF_DEF_DEFAULT_CROP_ORIGIN[2] = {0x10, 0xC};
const uint8_t TIFF_DEF_EXIF_VERSION[4] = {0x30, 0x32, 0x32, 0x31};
const uint32_t TIFF_DEF_THUMB_BIT_PER_SAMPLE[3] = {0x08, 0x08, 0x08};
const uint8_t TIFF_DEF_OPCODE_LIST2[0x184] = {
    0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x09, 0x01, 0x03, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x0B, 0xB7, 0x00, 0x00, 0x14, 0xCF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
    0x3F, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x3F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x01, 0x03, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0B, 0xB7, 0x00, 0x00, 0x14, 0xCF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
    0x3F, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x3F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x01, 0x03, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x50, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x0B, 0xB7, 0x00, 0x00, 0x14, 0xCF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
    0x3F, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x3F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x01, 0x03, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x50, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0B, 0xB7, 0x00, 0x00, 0x14, 0xCF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
    0x3F, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x3F, 0x80, 0x00, 0x00};

#endif /*SUPPORT_SAMSUNG_DNG*/

}; /* namespace android */

#endif /* SEC_CAMERA_DNG_H_ */

