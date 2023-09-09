#ifndef EXYNOS_CAMERA_TUNING_DEFINES_H__
#define EXYNOS_CAMERA_TUNING_DEFINES_H__

#define SIMMIAN_CTRL_MAXPACKET 0x8000

typedef enum {
    TUNE_GET_REQUEST = 0,
    TUNE_SET_REQUEST = 1,
}TUNE_REQUEST;

typedef enum {
    TUNE_VENDOR_FPGA_CTRL = 0x17, //< Image Control Vendor Command that check the avail signal(FPGA) for Image Read
    TUNE_VENDOR_IMG_READY = 0x1a, //< Image Control Vendor Command that check the avail signal(FPGA) for Image Read
    TUNE_VENDOR_IMG_UPDATE = 0x1b,//< Image Control Vender Command that Send 1frame Start Command(the Trans signal)  to FPGA
    TUNE_VENDOR_WRITE_JSON = 0x43,
    TUNE_VENDOR_READ_JSON = 0x40,
    TUNE_VENDOR_WRITE_ISP32 = 0x51,
    TUNE_VENDOR_READ_ISP32 = 0x52,
    TUNE_VENDOR_WRITE_ISPMEM = 0x53,
    TUNE_VENDOR_READ_ISPMEM = 0x54,
    TUNE_VENDOR_READ_ISPMEM2 = 0x56,
}TUNE_VENDOR_REQUEST;

typedef enum {
    TUNE_META_SET_FROM_HAL = 0x0,
    TUNE_META_SET_FROM_TUNING_MODULE = 0x1,
} TUNE_META_SET_FROM;

typedef enum {
    TUNE_META_SET_TARGET_SENSOR = 0x0,
    TUNE_META_SET_TARGET_3AA = 0x1,
    TUNE_META_SET_TARGET_ISP = 0x2,
} TUNE_META_SET_TARGET;

typedef enum {
    TUNE_JSON_READ = 0x1,
    TUNE_JSON_WRITE = 0x2,
}TUNE_JSON_OPERATION;

typedef enum {
    TUNE_IMAGE_INTERNAL_CMD_FPGA_CTRL = 0x1,
    TUNE_IMAGE_INTERNAL_CMD_READY = 0x2,
    TUNE_IMAGE_INTERNAL_CMD_UPDATE = 0x3,
    TUNE_IMAGE_INTERNAL_CMD_WRITE = 0x4,
}TUNE_IMAGE_INTERNAL_CMD;

typedef enum {
    TUNE_JSON_INTERNAL_CMD_READ_DONE = 0x1,
    TUNE_JSON_INTERNAL_CMD_WRITE_DONE = 0x2,
}TUNE_JSON_INTERNAL_CMD;

typedef enum {
    TUNE_SOCKET_CMD = 0,
    TUNE_SOCKET_JSON,
    TUNE_SOCKET_IMAGE,
    TUNE_SOCKET_ID_MAX,
} TUNE_SOCKET_ID;

#define SIRC_ISP_AF_WINDOW_RESOLUTION   32768

#define SINGLE_INNER_WIDTH      (SIRC_ISP_AF_WINDOW_RESOLUTION / 10) /** Width of inner window in single mode     */
#define SINGLE_INNER_HEIGHT     (SIRC_ISP_AF_WINDOW_RESOLUTION / 10) /** Height of inner window in single mode     */
#define CONTINUOUS_INNER_WIDTH  (SIRC_ISP_AF_WINDOW_RESOLUTION / 4)  /** Width of inner window in continuous mode */
#define CONTINUOUS_INNER_HEIGHT (SIRC_ISP_AF_WINDOW_RESOLUTION / 3)  /** Height of inner window in continuous mode */

//! SIMMY Memory Define
#define SIMMY_RESERVED_PARAM_NUM    256
#define SIMMY_MAGIC                 0x12345678
#define SIMMY_DONEFLAG              0x5A5AA5A5

#define FIMC_IS_SIMMIAN_INIT _IOW('S', 0x01, int)
#define FIMC_IS_SIMMIAN_WRITE _IOW('S', 0x02, int)
#define FIMC_IS_SET_TUNEID _IOW('S', 0x03, int)
#define FIMC_IS_SIMMIAN_READ _IOR('S', 0x04, int)
#define FIMC_IS_SET_ISPID _IOW('S', 0x05, int)
#define FIMC_IS_SHAREBASE_READ _IOR('S', 0x06, int)
#define FIMC_IS_DEBUGBASE_READ _IOR('S', 0x07, int)
#define FIMC_IS_SENSOR_INFO_CHG _IOW('S', 0x08, int)
#define FIMC_IS_DDK_VERSION _IOR('S', 0x09, int)
#define FIMC_IS_AFLOG_READ _IOR('S', 0x0A, int)

//! physical address information with predefined address
#define ADDR_GET_SIMMYCMD_BASE_ADDR             0xfffffff0
#define ADDR_GET_SIMMYCMD_AF_LOG_BASE_ADDR      0xfffffff1
#define ADDR_GET_A5_FWLOAD_BASE_ADDR            0xfffffff2
#define ADDR_GET_SETFILE_BASE_ADDR              0xfffffff3
#define ADDR_GET_SIMULATOR_BAYER_IMAGE_ADDR     0xfffffff4
#define ADDR_GET_COMPANION_FW_ADDR              0xfffffff5
#define ADDR_GET_COMPANION_MASTER_SETFILE_ADDR  0xfffffff6
#define ADDR_GET_COMPANION_MODE_SETFILE_ADDR    0xfffffff7
#define ADDR_GET_SETFILE_BASE_ADDR_FRONT        0xfffffff8
#define ADDR_GET_RTA_BIN_BASE_ADDR              0xfffffff9
#define ADDR_GET_VRA_BIN_BASE_ADDR              0xfffffffa
#define ADDR_GET_SIMMIAN_SOURCE_TYPE_ADDR       0xfffffffb
#define ADDR_GET_SIMMIAN_FRAME_NUMBER_ADDR      0xfffffffc
#define ADDR_GET_EXIF_DUMP_BASE_ADDR            0xfffffffd

#define ADDR_GET_SIMMYCMD_MAGIC                 0xffffffff

#endif //EXYNOS_CAMERA_TUNING_DEFINES_H__
