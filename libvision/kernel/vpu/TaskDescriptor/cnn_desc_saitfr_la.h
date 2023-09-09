
namespace cnn_la {

#include "cnn_coeff_saitfr_la.h"

#define BUFFER_INFO(x)  {x, sizeof(x)}

struct buffer_addr_size mem_index_1_buf = BUFFER_INFO(P0_L0_coeff_addr0);
struct buffer_addr_size mem_index_2_buf = BUFFER_INFO(P0_L0_coeff_addr1);
struct buffer_addr_size mem_index_3_buf = BUFFER_INFO(P0_L0_coeff_addr2);
struct buffer_addr_size mem_index_4_buf = BUFFER_INFO(P0_L0_coeff_addr3);

struct buffer_addr_size mem_index_6_buf = BUFFER_INFO(P1_L0_coeff_addr0);
struct buffer_addr_size mem_index_7_buf = BUFFER_INFO(P1_L0_coeff_addr1);
struct buffer_addr_size mem_index_8_buf = BUFFER_INFO(P1_L0_coeff_addr2);
struct buffer_addr_size mem_index_9_buf = BUFFER_INFO(P1_L0_coeff_addr3);

struct buffer_addr_size mem_index_11_buf = BUFFER_INFO(P2_L0_coeff_addr0);
struct buffer_addr_size mem_index_12_buf = BUFFER_INFO(P2_L0_coeff_addr1);
struct buffer_addr_size mem_index_13_buf = BUFFER_INFO(P2_L0_coeff_addr2);
struct buffer_addr_size mem_index_14_buf = BUFFER_INFO(P2_L0_coeff_addr3);

struct buffer_addr_size mem_index_16_buf = BUFFER_INFO(P3_L0_coeff_addr0);
struct buffer_addr_size mem_index_17_buf = BUFFER_INFO(P3_L0_coeff_addr1);
struct buffer_addr_size mem_index_18_buf = BUFFER_INFO(P3_L0_coeff_addr2);
struct buffer_addr_size mem_index_19_buf = BUFFER_INFO(P3_L0_coeff_addr3);

struct buffer_addr_size mem_index_21_buf = BUFFER_INFO(P4_L0_coeff_addr0);
struct buffer_addr_size mem_index_22_buf = BUFFER_INFO(P4_L0_coeff_addr1);
struct buffer_addr_size mem_index_23_buf = BUFFER_INFO(P4_L0_coeff_addr2);
struct buffer_addr_size mem_index_24_buf = BUFFER_INFO(P4_L0_coeff_addr3);

struct buffer_addr_size mem_index_26_buf = BUFFER_INFO(P5_L0_coeff_addr0);
struct buffer_addr_size mem_index_27_buf = BUFFER_INFO(P5_L0_coeff_addr1);
struct buffer_addr_size mem_index_28_buf = BUFFER_INFO(P5_L0_coeff_addr2);
struct buffer_addr_size mem_index_29_buf = BUFFER_INFO(P5_L0_coeff_addr3);

struct buffer_addr_size mem_index_31_buf = BUFFER_INFO(P6_L0_coeff_addr0);
struct buffer_addr_size mem_index_32_buf = BUFFER_INFO(P6_L0_coeff_addr1);
struct buffer_addr_size mem_index_33_buf = BUFFER_INFO(P6_L0_coeff_addr2);
struct buffer_addr_size mem_index_34_buf = BUFFER_INFO(P6_L0_coeff_addr3);

/* EX1_CNN_SAIT16_L27_EXT_IN_MEM_COEFF_ADDR0 */
struct buffer_addr_size mem_index_36_buf = BUFFER_INFO(P6_L1_coeff_addr0);
/* EX1_CNN_SAIT16_L27_EXT_IN_MEM_COEFF_ADDR1 */
struct buffer_addr_size mem_index_37_buf = BUFFER_INFO(P6_L1_coeff_addr1);
/* EX1_CNN_SAIT16_L27_EXT_IN_MEM_COEFF_ADDR2 */
struct buffer_addr_size mem_index_38_buf = BUFFER_INFO(P6_L1_coeff_addr2);
/* EX1_CNN_SAIT16_L27_EXT_IN_MEM_COEFF_ADDR3 */
struct buffer_addr_size mem_index_39_buf = BUFFER_INFO(P6_L1_coeff_addr3);

struct buffer_addr_size mem_index_41_buf = BUFFER_INFO(P6_L2_coeff_addr0);
struct buffer_addr_size mem_index_42_buf = BUFFER_INFO(P6_L2_coeff_addr1);
struct buffer_addr_size mem_index_43_buf = BUFFER_INFO(P6_L2_coeff_addr2);
struct buffer_addr_size mem_index_44_buf = BUFFER_INFO(P6_L2_coeff_addr3);

struct buffer_addr_size mem_index_46_buf = BUFFER_INFO(P6_L3_coeff_addr0);
struct buffer_addr_size mem_index_47_buf = BUFFER_INFO(P6_L3_coeff_addr1);
struct buffer_addr_size mem_index_48_buf = BUFFER_INFO(P6_L3_coeff_addr2);
struct buffer_addr_size mem_index_49_buf = BUFFER_INFO(P6_L3_coeff_addr3);

struct buffer_addr_size mem_index_51_buf = BUFFER_INFO(P6_L4_coeff_addr0);
struct buffer_addr_size mem_index_52_buf = BUFFER_INFO(P6_L4_coeff_addr1);
struct buffer_addr_size mem_index_53_buf = BUFFER_INFO(P6_L4_coeff_addr2);
struct buffer_addr_size mem_index_54_buf = BUFFER_INFO(P6_L4_coeff_addr3);

struct buffer_addr_size mem_index_56_buf = BUFFER_INFO(P6_L5_coeff_addr0);
struct buffer_addr_size mem_index_57_buf = BUFFER_INFO(P6_L5_coeff_addr1);
struct buffer_addr_size mem_index_58_buf = BUFFER_INFO(P6_L5_coeff_addr2);
struct buffer_addr_size mem_index_59_buf = BUFFER_INFO(P6_L5_coeff_addr3);

cnn_buffer_info_t sait_fr_la_io[] = {
    {VS4L_DIRECTION_IN, 0, 0, NULL},
    {VS4L_DIRECTION_OT, 0, 40, NULL},
    {VS4L_DIRECTION_OT, 1, 50, NULL},
    {VS4L_DIRECTION_OT, 2, 60, NULL}
};

cnn_buffer_info_t sait_fr_la_inter[] = {
    {0, 0, 1, &mem_index_1_buf},
    {0, 0, 2, &mem_index_2_buf},
    {0, 0, 3, &mem_index_3_buf},
    {0, 0, 4, &mem_index_4_buf},
    {0, 0, 5, NULL},
    {0, 0, 6, &mem_index_6_buf},
    {0, 0, 7, &mem_index_7_buf},
    {0, 0, 8, &mem_index_8_buf},
    {0, 0, 9, &mem_index_9_buf},
    {0, 0, 10, NULL},
    {0, 0, 11, &mem_index_11_buf},
    {0, 0, 12, &mem_index_12_buf},
    {0, 0, 13, &mem_index_13_buf},
    {0, 0, 14, &mem_index_14_buf},
    {0, 0, 15, NULL},
    {0, 0, 16, &mem_index_16_buf},
    {0, 0, 17, &mem_index_17_buf},
    {0, 0, 18, &mem_index_18_buf},
    {0, 0, 19, &mem_index_19_buf},
    {0, 0, 20, NULL},
    {0, 0, 21, &mem_index_21_buf},
    {0, 0, 22, &mem_index_22_buf},
    {0, 0, 23, &mem_index_23_buf},
    {0, 0, 24, &mem_index_24_buf},
    {0, 0, 25, NULL},
    {0, 0, 26, &mem_index_26_buf},
    {0, 0, 27, &mem_index_27_buf},
    {0, 0, 28, &mem_index_28_buf},
    {0, 0, 29, &mem_index_29_buf},
    {0, 0, 30, NULL},
    {0, 0, 31, &mem_index_31_buf},
    {0, 0, 32, &mem_index_32_buf},
    {0, 0, 33, &mem_index_33_buf},
    {0, 0, 34, &mem_index_34_buf},
    {0, 0, 35, NULL},
    {0, 0, 36, &mem_index_36_buf},
    {0, 0, 37, &mem_index_37_buf},
    {0, 0, 38, &mem_index_38_buf},
    {0, 0, 39, &mem_index_39_buf},

    {0, 0, 41, &mem_index_41_buf},
    {0, 0, 42, &mem_index_42_buf},
    {0, 0, 43, &mem_index_43_buf},
    {0, 0, 44, &mem_index_44_buf},
    {0, 0, 45, NULL},
    {0, 0, 46, &mem_index_46_buf},
    {0, 0, 47, &mem_index_47_buf},
    {0, 0, 48, &mem_index_48_buf},
    {0, 0, 49, &mem_index_49_buf},

    {0, 0, 51, &mem_index_51_buf},
    {0, 0, 52, &mem_index_52_buf},
    {0, 0, 53, &mem_index_53_buf},
    {0, 0, 54, &mem_index_54_buf},
    {0, 0, 55, NULL},
    {0, 0, 56, &mem_index_56_buf},
    {0, 0, 57, &mem_index_57_buf},
    {0, 0, 58, &mem_index_58_buf},
    {0, 0, 59, &mem_index_59_buf},
};

};

