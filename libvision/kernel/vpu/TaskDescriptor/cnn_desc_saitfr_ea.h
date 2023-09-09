
namespace cnn_ea {

#include "cnn_coeff_saitfr_ea.h"

#define BUFFER_INFO(x)  {x, sizeof(x)}

struct buffer_addr_size mem_index_1_buf = BUFFER_INFO(P0_L0_coeff_addr0);
struct buffer_addr_size mem_index_2_buf = BUFFER_INFO(P0_L0_coeff_addr1);
struct buffer_addr_size mem_index_3_buf = BUFFER_INFO(P0_L0_coeff_addr2);
struct buffer_addr_size mem_index_4_buf = BUFFER_INFO(P0_L0_coeff_addr3);

struct buffer_addr_size mem_index_6_buf = BUFFER_INFO(P0_L1_coeff_addr0);
struct buffer_addr_size mem_index_7_buf = BUFFER_INFO(P0_L1_coeff_addr1);
struct buffer_addr_size mem_index_8_buf = BUFFER_INFO(P0_L1_coeff_addr2);
struct buffer_addr_size mem_index_9_buf = BUFFER_INFO(P0_L1_coeff_addr3);

struct buffer_addr_size mem_index_11_buf = BUFFER_INFO(P1_L0_coeff_addr0);
struct buffer_addr_size mem_index_12_buf = BUFFER_INFO(P1_L0_coeff_addr1);
struct buffer_addr_size mem_index_13_buf = BUFFER_INFO(P1_L0_coeff_addr2);
struct buffer_addr_size mem_index_14_buf = BUFFER_INFO(P1_L0_coeff_addr3);

struct buffer_addr_size mem_index_16_buf = BUFFER_INFO(P2_L0_coeff_addr0);
struct buffer_addr_size mem_index_17_buf = BUFFER_INFO(P2_L0_coeff_addr1);
struct buffer_addr_size mem_index_18_buf = BUFFER_INFO(P2_L0_coeff_addr2);
struct buffer_addr_size mem_index_19_buf = BUFFER_INFO(P2_L0_coeff_addr3);

cnn_buffer_info_t sait_fr_ea_io[] = {
    {VS4L_DIRECTION_IN, 0, 0, NULL},
    {VS4L_DIRECTION_OT, 0, 10, NULL},
    {VS4L_DIRECTION_OT, 1, 20, NULL}
};

cnn_buffer_info_t sait_fr_ea_inter[] = {
    {0, 0, 1, &mem_index_1_buf},
    {0, 0, 2, &mem_index_2_buf},
    {0, 0, 3, &mem_index_3_buf},
    {0, 0, 4, &mem_index_4_buf},
    {0, 0, 5, NULL},
    {0, 0, 6, &mem_index_6_buf},
    {0, 0, 7, &mem_index_7_buf},
    {0, 0, 8, &mem_index_8_buf},
    {0, 0, 9, &mem_index_9_buf},
    {0, 0, 11, &mem_index_11_buf},
    {0, 0, 12, &mem_index_12_buf},
    {0, 0, 13, &mem_index_13_buf},
    {0, 0, 14, &mem_index_14_buf},
    {0, 0, 15, NULL},
    {0, 0, 16, &mem_index_16_buf},
    {0, 0, 17, &mem_index_17_buf},
    {0, 0, 18, &mem_index_18_buf},
    {0, 0, 19, &mem_index_19_buf}
};

};

