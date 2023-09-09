/*
 *
 * Copyright 2021 Samsung Electronics S.LSI Co. LTD
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

#include <log/log.h>
#include <VendorVideoAPI.h>

typedef struct _PackedStr {
    unsigned int packed_data;
    int remained_bits_in_packed_data;
    int write_bit_size;

    int is_epb_on;
    int num_zero_byte;
    int num_epb;
} PackedStr;

typedef struct _BitstreamInfo {
    unsigned char *pStream;
    unsigned int   nSize;
    unsigned int   nIndicator;
} BitstreamInfo;

static void sei_write_2094_40(ExynosHdrData_ST2094_40 *data, BitstreamInfo *bs);
static void write_filler_data_rbsp(BitstreamInfo *bs);
static int  get_payload_size(ExynosHdrData_ST2094_40 *data);
static void write_bytes(unsigned char *data, unsigned int size, BitstreamInfo *bs);
static void put_bits(BitstreamInfo *bs, int number, unsigned int data, PackedStr *packedStr);
static void flush_bitstream(BitstreamInfo *bs, PackedStr *packedStr);
static unsigned int insert_epb_data(PackedStr *packedStr);

unsigned int Exynos_sei_write(
    ExynosHdrData_ST2094_40 *data,
    int                      size,
    unsigned char           *stream)
{
    BitstreamInfo bs;

    if ((data == NULL) || (stream == NULL)) {
        ALOGE("[%s] invalid parameters", __FUNCTION__);
        return 0;
    }

    if (size <= 0) {
        ALOGE("[%s] size(%d) is invalid", __FUNCTION__, size);
        return 0;
    }

    /* Initialized bitstream structure */
    bs.pStream    = stream;
    bs.nSize      = size;
    bs.nIndicator = 0;

    sei_write_2094_40(data, &bs);
    write_filler_data_rbsp(&bs);

    return bs.nIndicator;
}

static void sei_write_2094_40(
    ExynosHdrData_ST2094_40 *data,
    BitstreamInfo           *bs)
{
    int byte_align_bit = 0;
    int payload_size   = 64; /* profile-A: 49, B: 64-byte */

    int w, i, j;

    PackedStr packedStr;

    if ((data == NULL) || (bs == NULL)) {
        ALOGE("[%s] invalid parameters", __FUNCTION__);
        return;
    }

    /* Initialized PackedStr structure */
    packedStr.packed_data                  = 0;
    packedStr.remained_bits_in_packed_data = 32;
    packedStr.write_bit_size               = 0;
    packedStr.is_epb_on                    = 0;
    packedStr.num_zero_byte                = 0;
    packedStr.num_epb                      = 0;

    /* Put start code */
    put_bits(bs, 8, 0x00, &packedStr);
    put_bits(bs, 8, 0x00, &packedStr);
    put_bits(bs, 8, 0x00, &packedStr);
    put_bits(bs, 8, 0x01, &packedStr);

    /* nal_unit_header */
    put_bits(bs, 1, 0x00, &packedStr);    /* forbidden_zero_bit(0)   */
    put_bits(bs, 6, 0x27, &packedStr);    /* nal_unit_type(39)       */
    put_bits(bs, 6, 0x00, &packedStr);    /* nuh_reserved_zero_6bits */
    put_bits(bs, 3, 0x01, &packedStr);    /* nuh_temporal_id_plus1   */

    payload_size = get_payload_size(data);
    put_bits(bs, 8, 0x04, &packedStr);          /* payload type : user_data_registered_itu_t_t35() */
    put_bits(bs, 8, payload_size, &packedStr);  /* payload size */

    put_bits(bs, 8,  data->country_code,  &packedStr);
    put_bits(bs, 16, data->provider_code, &packedStr);
    put_bits(bs, 16, data->provider_oriented_code, &packedStr);
    put_bits(bs, 8,  data->application_identifier, &packedStr);
    put_bits(bs, 8,  data->application_version, &packedStr);

    /* Enable EPB */
    packedStr.is_epb_on = 1;

#ifdef USE_FULL_ST2094_40
    put_bits(bs, 2,  data->num_windows, &packedStr);

    for (w = 0; w < data->num_windows - 1; w++) {
        put_bits(bs, 16, data->window_upper_left_corner_x[w], &packedStr);
        put_bits(bs, 16, data->window_upper_left_corner_y[w], &packedStr);
        put_bits(bs, 16, data->window_lower_right_corner_x[w], &packedStr);
        put_bits(bs, 16, data->window_lower_right_corner_y[w], &packedStr);
        put_bits(bs, 16, data->center_of_ellipse_x[w], &packedStr);
        put_bits(bs, 16, data->center_of_ellipse_y[w], &packedStr);
        put_bits(bs, 8,  data->rotation_angle[w], &packedStr);
        put_bits(bs, 16, data->semimajor_axis_internal_ellipse[w], &packedStr);
        put_bits(bs, 16, data->semimajor_axis_external_ellipse[w], &packedStr);
        put_bits(bs, 16, data->semiminor_axis_external_ellipse[w], &packedStr);
        put_bits(bs, 1,  data->overlap_process_option[w], &packedStr);
    }

    put_bits(bs, 27, data->targeted_system_display_maximum_luminance, &packedStr);
    put_bits(bs, 1,  data->targeted_system_display_actual_peak_luminance_flag, &packedStr);

    if (data->targeted_system_display_actual_peak_luminance_flag == 1) {
        put_bits(bs, 5, data->num_rows_targeted_system_display_actual_peak_luminance, &packedStr);
        put_bits(bs, 5, data->num_cols_targeted_system_display_actual_peak_luminance, &packedStr);

        for (i = 0; i < data->num_rows_targeted_system_display_actual_peak_luminance; i++) {
            for (j = 0; j < data->num_cols_targeted_system_display_actual_peak_luminance; j++) {
                put_bits(bs, 4, data->targeted_system_display_actual_peak_luminance[i][j], &packedStr);
            }
        }
    }

    for (w = 0; w < data->num_windows; w++) {
        for (i = 0; i < 3; i++) {
            put_bits(bs, 17, data->maxscl[w][i], &packedStr);
        }

        put_bits(bs, 17, data->average_maxrgb[w], &packedStr);
        put_bits(bs, 4,  data->num_maxrgb_percentiles[w], &packedStr);

        for (i = 0; i < data->num_maxrgb_percentiles[w]; i++) {
            put_bits(bs, 7,  data->maxrgb_percentages[w][i], &packedStr);
            put_bits(bs, 17, data->maxrgb_percentiles[w][i], &packedStr);
        }

        put_bits(bs, 10, data->fraction_bright_pixels[w], &packedStr);
    }

    put_bits(bs, 1, data->mastering_display_actual_peak_luminance_flag, &packedStr);

    if (data->mastering_display_actual_peak_luminance_flag == 1) {
        put_bits(bs, 5, data->num_rows_mastering_display_actual_peak_luminance, &packedStr);
        put_bits(bs, 5, data->num_cols_mastering_display_actual_peak_luminance, &packedStr);

        for (i = 0; i < data->num_rows_mastering_display_actual_peak_luminance; i++) {
            for (j = 0; j < data->num_cols_mastering_display_actual_peak_luminance; j++) {
                put_bits(bs, 4, data->mastering_display_actual_peak_luminance[i][j], &packedStr);
            }
        }
    }

    for (w = 0; w < data->num_windows; w++) {
        put_bits(bs, 1, data->tone_mapping.tone_mapping_flag[w], &packedStr);

        if (data->tone_mapping.tone_mapping_flag[w] == 1) {
            put_bits(bs, 12, data->tone_mapping.knee_point_x[w], &packedStr);
            put_bits(bs, 12, data->tone_mapping.knee_point_y[w], &packedStr);
            put_bits(bs, 4,  data->tone_mapping.num_bezier_curve_anchors[w], &packedStr);

            for (i = 0; i < data->tone_mapping.num_bezier_curve_anchors[w]; i++) {
                put_bits(bs, 10, data->tone_mapping.bezier_curve_anchors[w][i], &packedStr);
            }
        }

        put_bits(bs, 1, data->color_saturation_mapping_flag[w], &packedStr);

        if (data->color_saturation_mapping_flag[w] == 1) {
            put_bits(bs, 6, data->color_saturation_weight[w], &packedStr);
        }
    }
#else
    /* num_windows : 2bit (fixed value : 1) */
    put_bits(bs, 2,  0x01, &packedStr);

    /* window_upper_left_corner_x : 16bit (fixed value : 1) */
    put_bits(bs, 16, 0x01, &packedStr);

    /* window_upper_left_corner_y : 16bit (fixed value : 1) */
    put_bits(bs, 16, 0x01, &packedStr);

    /* window_lower_right_corner_x : 16bit (fixed value : 1) */
    put_bits(bs, 16, 0x01, &packedStr);

    /* window_lower_right_corner_y : 16bit (fixed value : 1) */
    put_bits(bs, 16, 0x01, &packedStr);

    /* center_of_ellipse_x : 16bit (fixed value : 1) */
    put_bits(bs, 16, 0x01, &packedStr);

    /* center_of_ellipse_y : 16bit (fixed value : 1) */
    put_bits(bs, 16, 0x01, &packedStr);

    /* rotation_angle : 8bit (fixed value : 1) */
    put_bits(bs, 8,  0x01, &packedStr);

    /* semimajor_axis_internal_ellipse : 16bit (fixed value : 1) */
    put_bits(bs, 16, 0x01, &packedStr);

    /* semimajor_axis_external_ellipse : 16bit (fixed value : 1) */
    put_bits(bs, 16, 0x01, &packedStr);

    /* semiminor_axis_external_ellipse : 16bit (fixed value : 1) */
    put_bits(bs, 16, 0x01, &packedStr);

    /* overlap_process_option : 1bit (fixed value : 1) */
    put_bits(bs, 1,  0x01, &packedStr);

    /* targeted_system_display_maximum_luminance: 27bit */
    put_bits(bs, 27, data->display_maximum_luminance, &packedStr);

    /* targeted_system_display_actual_peak_luminance_flag: 1bit (always 0) */
    put_bits(bs, 1,  0x00, &packedStr);

    /* NOTE: These info would not set because targeted_system_display_actual_peak_luminance_flag is always 0
    * - num_rows_targeted_system_display_actual_peak_luminance: 5bit
    * - num_cols_targeted_system_display_actual_peak_luminance: 5bit
    * - targeted_system_display_actual_peak_luminance: 4bit
    */

    /* maxscl: 17bit */
    for (i = 0; i < 3; i++) {
        put_bits(bs, 17, data->maxscl[i], &packedStr);
    }

    /* average_maxrgb: 17bit (fixed value : 1) */
    put_bits(bs, 17, 0x01, &packedStr);

    /* num_distribution_maxrgb_percentiles: 4bit */
    put_bits(bs, 4,  data->num_maxrgb_percentiles, &packedStr);

    for (i = 0; i < data->num_maxrgb_percentiles; i++) {
        /* distribution_maxrgb_percentaged: 7bit */
        put_bits(bs, 7,  data->maxrgb_percentages[i], &packedStr);

        /* distribution_maxrgb_percentiles: 17bit */
        put_bits(bs, 17, data->maxrgb_percentiles[i], &packedStr);
    }

    /* fraction_bright_pixels: 10bit (fixed value : 1) */
    put_bits(bs, 10, 0x01, &packedStr);

    /* mastering_display_actual_peak_luminance_flag: 1bit */
    put_bits(bs, 1, 0x00, &packedStr);

    /* NOTE: These infos would not be set because mastering_display_actual_peak_luminance_flag is always 0.
     * - num_rows_mastering_display_actual_peak_luminance: 5bit
     * - num_cols_mastering_display_actual_peak_luminance: 5bit
     * - mastering_display_actual_peak_luminance: 4bit
     */

     /* tone_mapping_flag: 1bit */
     put_bits(bs, 1, data->tone_mapping.tone_mapping_flag, &packedStr);

    if (data->tone_mapping.tone_mapping_flag == 1) {
        /* knee_point_x: 12bit */
        put_bits(bs, 12, data->tone_mapping.knee_point_x, &packedStr);

        /* knee_point_y: 12bit */
        put_bits(bs, 12, data->tone_mapping.knee_point_y, &packedStr);

        /* num_bezier_curve_anchors: 4bit */
        put_bits(bs, 4,  data->tone_mapping.num_bezier_curve_anchors, &packedStr);

        /* bezier_curve_anchors: 10bit */
        for (i = 0; i < data->tone_mapping.num_bezier_curve_anchors; i++) {
            put_bits(bs, 10, data->tone_mapping.bezier_curve_anchors[i], &packedStr);
        }
    }

    /* color_saturation_mapping_flag: 1bit */
    put_bits(bs, 1, 0x00, &packedStr);

    /* NOTE: This info would not be set because color_saturation_mapping_flag is always 0.
     * - color_saturation_weight: 6bit
     */
#endif

    /* Put byte align */
    byte_align_bit = packedStr.write_bit_size % 8;
    if (byte_align_bit != 0) {
        put_bits(bs, (8 - byte_align_bit), 0, &packedStr);
    }

    /* rbsp_trailing_bits */
    put_bits(bs, 8, 0x80, &packedStr);

    /* Flush the data */
    flush_bitstream(bs, &packedStr);
}

static void write_filler_data_rbsp(BitstreamInfo *bs)
{
    int payload_size;
    int i;

    PackedStr packedStr;

    if (bs == NULL) {
        ALOGE("[%s] invalid parameters", __FUNCTION__);
        return;
    }

    /* Initialized PackedStr structure */
    packedStr.packed_data = 0;
    packedStr.remained_bits_in_packed_data = 32;
    packedStr.write_bit_size = bs->nIndicator * 8;
    packedStr.is_epb_on      = 0;
    packedStr.num_zero_byte  = 0;
    packedStr.num_epb        = 0;

    /* Put start code */
    put_bits(bs, 8, 0x00, &packedStr);
    put_bits(bs, 8, 0x00, &packedStr);
    put_bits(bs, 8, 0x00, &packedStr);
    put_bits(bs, 8, 0x01, &packedStr);

    /* nal_unit_header() */
    put_bits(bs, 1, 0x00, &packedStr);    /* forbidden_zero_bit(0)   */
    put_bits(bs, 6, 0x26, &packedStr);    /* nal_unit_type(38)       */
    put_bits(bs, 6, 0x00, &packedStr);    /* nuh_reserved_zero_6bits */
    put_bits(bs, 3, 0x01, &packedStr);    /* nuh_temporal_id_plus1   */

    /* write 0xff */
    payload_size = (bs->nSize - (packedStr.write_bit_size / 8)) - 1;

    for (i = 0; i < payload_size; i++) {
        put_bits(bs, 8, 0xff, &packedStr); /* ff_byte */
    }

    /* rbsp_trailing_bits() */
    put_bits(bs, 8, 0x80, &packedStr);

    /* Flush the data */
    flush_bitstream(bs, &packedStr);
}

/* Internal function */
static int get_payload_size(ExynosHdrData_ST2094_40 *data)
{
    int num_bits = 0;
    int w, i, j;

    if (data == NULL) {
        ALOGE("[%s] invalid parameters", __FUNCTION__);
        return -1;
    }

#ifdef USE_FULL_ST2094_40
    /* itu_t_t35_country_code                      u(8)
     * itu_t_t35_terminal_provider_code            u(16)
     * itu_t_t35_terminal_provider_oriented_code   u(16)
     * application_identifier                      u(8)
     * application_version                         u(8)
     * num_windows                                 u(2)
     */
    num_bits = 8 + 16 + 16 + 8 + 8 + 2;

    for (w = 0; w < data->num_windows - 1; w++) {
        num_bits += (16 + 16 + 16 + 16 + 16 + 16 + 8 + 16 + 16 + 16 + 1);
    }

    num_bits += (27 + 1);

    if (data->targeted_system_display_actual_peak_luminance_flag == 1) {
        num_bits += (5 + 5);

        for (i = 0; i < data->num_rows_targeted_system_display_actual_peak_luminance; i++) {
            for (j = 0; j < data->num_cols_targeted_system_display_actual_peak_luminance; j++) {
                num_bits += 4;
            }
        }
    }

    for (w = 0; w < data->num_windows; w++) {
        for (i = 0; i < 3; i++) {
            num_bits += 17;
        }
        num_bits += (17 + 4);

        for (i = 0; i < data->num_maxrgb_percentiles[w]; i++) {
            num_bits += (7 + 17);
        }
        num_bits += 10;
    }

    num_bits += 1;

    if (data->targeted_system_display_actual_peak_luminance_flag == 1) {
        num_bits += (5 + 5);

        for (i = 0; i < data->num_rows_targeted_system_display_actual_peak_luminance; i++) {
            for (j = 0; j < data->num_cols_targeted_system_display_actual_peak_luminance; j++) {
                num_bits += 4;
            }
        }
    }

    for (w = 0; w < data->num_windows; w++) {
        num_bits += 1;

        if (data->tone_mapping.tone_mapping_flag[w] == 1) {
            num_bits += (12 + 12 + 4);

            for (i = 0; i < data->tone_mapping.num_bezier_curve_anchors[w]; i++) {
                num_bits += 10;
            }
        }

        num_bits += 1;

        if (data->color_saturation_mapping_flag[w] == 1) {
            num_bits += 6;
        }
    }
#else
    /* itu_t_t35_country_code                      u(8)
     * itu_t_t35_terminal_provider_code            u(16)
     * itu_t_t35_terminal_provider_oriented_code   u(16)
     * application_identifier                      u(8)
     * application_version                         u(8)
     * num_windows                                 u(2)
     */

    num_bits = 8 + 16 + 16 + 8 + 8 + 2;

    /* window */
    num_bits += (16 + 16 + 16 + 16 + 16 + 16 + 8 + 16 + 16 + 16 + 1);

    num_bits += (27 + 1);

    for (i = 0; i < 3; i++) {
        num_bits += 17;
    }
    num_bits += (17 + 4);

    for (i = 0; i < data->num_maxrgb_percentiles; i++) {
        num_bits += (7 + 17);
    }
    num_bits += (10 + 1 + 1);

    if (data->tone_mapping.tone_mapping_flag == 1) {
        num_bits += (12 + 12 + 4);

        for (i = 0; i < data->tone_mapping.num_bezier_curve_anchors; i++) {
            num_bits += 10;
        }
    }

    num_bits += 1;
#endif

    /* Put byte align */
    int byte_align_bit = (num_bits % 8);
    if (byte_align_bit != 0) {
        num_bits += (8 - byte_align_bit);
    }

    return (num_bits / 8);
}

static void write_bytes(
    unsigned char *byte,
    unsigned int   size,
    BitstreamInfo *bs)
{
    int i;

    if ((byte == NULL) || (bs == NULL)) {
        ALOGE("[%s] invalid parameters", __FUNCTION__);
        return;
    }

    for (i = 0; i < size; i++) {
        bs->pStream[bs->nIndicator++] = byte[i];
    }
}

static void put_bits(
    BitstreamInfo *bs,
    int            number,
    unsigned int   data,
    PackedStr     *packedStr)
{
    if ((bs == NULL) || (packedStr == NULL)) {
        ALOGE("[%s] invalid parameter", __FUNCTION__);
        return;
    }

    /* check if there is enough room for currently request data */
    if (packedStr->remained_bits_in_packed_data >= number) {
        /* Enough room for data */
        packedStr->packed_data |= (data << (packedStr->remained_bits_in_packed_data - number));

        /* Update remained bit */
        packedStr->remained_bits_in_packed_data -= number;

        packedStr->write_bit_size += number;
        if (packedStr->remained_bits_in_packed_data == 0) {
            /* Write data in memory */
            unsigned int swapdata  = 0;
            unsigned int writedata = 0;

            if (packedStr->is_epb_on == 1) {
                /* Insert the EPB byte and Update the packedStr->num_epb (0, 1, 2) */
                writedata = insert_epb_data(packedStr);

                if ((writedata & 0xFFFF) == 0x00) {
                    packedStr->num_zero_byte = 2;
                } else if ((writedata & 0xFF) == 0x00) {
                    packedStr->num_zero_byte = 1;
                } else {
                    packedStr->num_zero_byte = 0;
                }

                packedStr->write_bit_size += (packedStr->num_epb * 8);
                packedStr->remained_bits_in_packed_data = (32 - (packedStr->num_epb * 8));

                if (packedStr->remained_bits_in_packed_data == 32) {
                    packedStr->packed_data = 0;
                } else {
                    packedStr->packed_data = (packedStr->packed_data << packedStr->remained_bits_in_packed_data);
                }
            } else {
                writedata = packedStr->packed_data;
                packedStr->remained_bits_in_packed_data = 32;
                packedStr->packed_data = 0;
            }

            swapdata |= (writedata & 0x000000ff) << 24;
            swapdata |= (writedata & 0x0000ff00) << 8;
            swapdata |= (writedata & 0x00ff0000) >> 8;
            swapdata |= (writedata & 0xff000000) >> 24;

            writedata = swapdata;
            write_bytes((unsigned char *)&writedata, sizeof(writedata), bs);
        }
    } else {
        int part_num;
        int part_data;
        int num_second_half_in_bit = (number - packedStr->remained_bits_in_packed_data);

        /* Write first half data */
        part_num  = packedStr->remained_bits_in_packed_data;
        part_data = ((data >> num_second_half_in_bit) & ((1 << packedStr->remained_bits_in_packed_data) - 1));
        put_bits(bs, part_num, part_data, packedStr);

        /* Write second half data */
        part_num  = num_second_half_in_bit;
        part_data = (data & ((1 << num_second_half_in_bit) - 1));
        put_bits(bs, part_num, part_data, packedStr);
    }
}

static void flush_bitstream (
    BitstreamInfo *bs,
    PackedStr     *packedStr)
{
    if ((bs == NULL) || (packedStr == NULL)) {
        ALOGE("[%s] invalid parameter", __FUNCTION__);
        return;
    }

    if (packedStr->remained_bits_in_packed_data != 32) {
        unsigned int swapdata    = 0;
        unsigned int writedata   = 0;
        int remained_wirte_bytes = 0;

        /* If the EPB byte be inserted when data is flushed, then the number of EPB is only 1.
         * 0x80 is rbsp_trailing_bits byte.
         * 0xXX_80_00_00.
         * 0xXX_XX_80_00.
         */
        if ((packedStr->is_epb_on == 1) &&
            ((packedStr->remained_bits_in_packed_data == 16) ||
             (packedStr->remained_bits_in_packed_data == 8))) {
            writedata = insert_epb_data(packedStr);

            if (packedStr->num_epb != 0) {
                packedStr->write_bit_size += 8;
                packedStr->remained_bits_in_packed_data -= 8;
                packedStr->packed_data = writedata;
            }
        } else {
            writedata = packedStr->packed_data;
        }

        swapdata |= (writedata & 0x000000ff) << 24;
        swapdata |= (writedata & 0x0000ff00) << 8;
        swapdata |= (writedata & 0x00ff0000) >> 8;
        swapdata |= (writedata & 0xff000000) >> 24;

        writedata = swapdata;
        remained_wirte_bytes = ((32 - packedStr->remained_bits_in_packed_data) / 8); // 1, 2, 3, 4
        write_bytes((unsigned char *)&writedata, remained_wirte_bytes, bs);

        /* Reset Packed structure */
        if (packedStr->remained_bits_in_packed_data == 0) {
            packedStr->remained_bits_in_packed_data = 32;
            packedStr->packed_data = 0;
        }
    }
}

/* Insert the EPB byte and Update the packedStr->num_epb (0, 1, 2) */
static unsigned int insert_epb_data(PackedStr *packedStr)
{
    unsigned int packed_data;
    unsigned int data;

    if (packedStr == NULL) {
        ALOGE("[%s] invalid parameters", __FUNCTION__);
        return 0;
    }

    packed_data = 0;
    data        = packedStr->packed_data;

    /* Epb control : 0x0A = 00 or 01 or 02 or 03 */
    if (packedStr->num_zero_byte == 2) {
        /* 0x00_00 | 00_00_0A_XX => 03_00_00_03 | 0A_XX : Insert two EPB
         * 0x00_00 | 0A_XX_XX_XX => 03_0A_XX_XX | XX    : Insert one EPB
         * 0x00_00 | XX_00_00_0A => XX_00_00_03 | 0A    : Insert one EPB
         * 0x00_00 | XX_00_00_XX => XX_00_00_XX |       : No Insert EPB
         */

        if (((data & 0xFFFFFF00) == 0x00000000) ||
            ((data & 0xFFFFFF00) == 0x00000100) ||
            ((data & 0xFFFFFF00) == 0x00000200) ||
            ((data & 0xFFFFFF00) == 0x00000300)) {
            packed_data = 0x03000000;
            packed_data |= (data & 0xFFFF0000) >> 8;
            packed_data |= 0x00000003;
            packedStr->num_epb = 2;
        } else if (((data & 0xFF000000) == 0x00000000) ||
                   ((data & 0xFF000000) == 0x01000000) ||
                   ((data & 0xFF000000) == 0x02000000) ||
                   ((data & 0xFF000000) == 0x03000000)) {
            packed_data = 0x03000000;
            packed_data |= (data & 0xFFFFFF00) >> 8;
            packedStr->num_epb = 1;
        } else if (((data & 0x00FFFFFF) == 0x00000000) ||
                   ((data & 0x00FFFFFF) == 0x00000001) ||
                   ((data & 0x00FFFFFF) == 0x00000002) ||
                   ((data & 0x00FFFFFF) == 0x00000003)) {
            packed_data = (data & 0xFFFFFF00);
            packed_data |= 0x00000003;
            packedStr->num_epb = 1;
        } else {
            packed_data = data;
            packedStr->num_epb = 0;
        }
    } else if (packedStr->num_zero_byte == 1) {
        /* 0xXX_00 | 00_0A_XX_XX => 00_03_0A_XX | XX : Insert one EPB
         * 0xXX_00 | XX_00_00_0A => XX_00_00_03 | 0A : Insert one EPB
         * 0xXX_00 | XX_00_00_XX => XX_00_00_XX |    : No Insert EPB
         */

        if (((data & 0xFFFF0000) == 0x00000000) ||
            ((data & 0xFFFF0000) == 0x00010000) ||
            ((data & 0xFFFF0000) == 0x00020000) ||
            ((data & 0xFFFF0000) == 0x00030000)) {
            packed_data = (data & 0xFF000000);
            packed_data |= 0x00030000;
            packed_data |= (data & 0x00FFFF00) >> 8;
            packedStr->num_epb = 1;
        } else if (((data & 0x00FFFFFF) == 0x00000000) ||
                   ((data & 0x00FFFFFF) == 0x00000001) ||
                   ((data & 0x00FFFFFF) == 0x00000002) ||
                   ((data & 0x00FFFFFF) == 0x00000003)) {
            packed_data = (data & 0xFFFFFF00);
            packed_data |= 0x00000003;
            packedStr->num_epb = 1;
        } else {
            packed_data = data;
            packedStr->num_epb = 0;
        }
    } else {
        /* 0xXX_XX | 00_00_0A_XX => 00_00_03_0A | XX : Insert one EPB
         * 0xXX_XX | XX_00_00_0A => XX_00_00_03 | 0A : Insert one EPB
         * 0xXX_XX | XX_00_00_XX => XX_00_00_XX |    : No Insert EPB
         */

        if (((data & 0xFFFFFF00) == 0x00000000) ||
            ((data & 0xFFFFFF00) == 0x00000100) ||
            ((data & 0xFFFFFF00) == 0x00000200) ||
            ((data & 0xFFFFFF00) == 0x00000300)) {
            packed_data = (data & 0xFFFF0000);
            packed_data |= 0x00000300;
            packed_data |= (data & 0x0000FF00) >> 8;
            packedStr->num_epb = 1;
        } else if (((data & 0x00FFFFFF) == 0x00000000) ||
                   ((data & 0x00FFFFFF) == 0x00000001) ||
                   ((data & 0x00FFFFFF) == 0x00000002) ||
                   ((data & 0x00FFFFFF) == 0x00000003)) {
            packed_data = (data & 0xFFFFFF00);
            packed_data |= 0x00000003;
            packedStr->num_epb = 1;
        } else {
            packed_data = data;
            packedStr->num_epb = 0;
        }
    }

    return packed_data;
}
