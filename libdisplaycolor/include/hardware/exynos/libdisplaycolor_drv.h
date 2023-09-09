/*
 *   Copyright 2020 Samsung Electronics Co., Ltd.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */
#ifndef __LIB_DISPLAY_COLOR_DRV_INTERFACE_H__
#define __LIB_DISPLAY_COLOR_DRV_INTERFACE_H__

#define DQE_COLORMODE_MAGIC     0xDA

struct dqe_colormode_global_header {
    uint8_t magic;
    uint8_t seq_num;
    uint16_t total_size;
    uint16_t header_size;
    uint16_t num_data;
    uint16_t crc;
    uint16_t reserved;
};

struct dqe_colormode_data_header {
    uint8_t magic;
    uint8_t id;
    uint16_t total_size;
    uint16_t header_size;
    uint8_t attr[4];
    uint16_t crc;
};

enum dqe_colormode_node_id {
    DQE_COLORMODE_ID_CGC17_ENC = 1,
    DQE_COLORMODE_ID_CGC17_CON,
    DQE_COLORMODE_ID_CGC_DITHER,
    DQE_COLORMODE_ID_DEGAMMA,
    DQE_COLORMODE_ID_GAMMA,
    DQE_COLORMODE_ID_GAMMA_MATRIX,
    DQE_COLORMODE_ID_HSC48_LCG,
    DQE_COLORMODE_ID_HSC,
    DQE_COLORMODE_ID_SCL,
    DQE_COLORMODE_ID_DE,
    DQE_COLORMODE_ID_MAX,
};

extern const char *dqe_colormode_node[DQE_COLORMODE_ID_MAX];

#endif /* __LIB_DISPLAY_COLOR_DRV_INTERFACE_H__ */
