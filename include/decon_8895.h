/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was automatically generated from a Linux kernel header
 ***   of the same name, to make information necessary for userspace to
 ***   call into the kernel available to libc.  It contains only constants,
 ***   structures, and macros generated from the original header, and thus,
 ***   contains no copyrightable information.
 ***
 ***   To edit the content of this header, modify the corresponding
 ***   source file (e.g. under external/kernel-headers/original/) then
 ***   run bionic/libc/kernel/tools/update_all.py
 ***
 ***   Any manual change here will be lost the next time this script will
 ***   be run. You've been warned!
 ***
 ****************************************************************************
 ****************************************************************************/
#ifndef ___SAMSUNG_DECON_H__
#define ___SAMSUNG_DECON_H__
#define S3C_FB_MAX_WIN (6)
#define MAX_DECON_WIN (6)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define DECON_WIN_UPDATE_IDX MAX_DECON_WIN
#define MAX_PLANE_CNT (3)
#define SUCCESS_EXYNOS_SMC 0
#define HDR_CAPABILITIES_NUM 4
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
typedef unsigned int u32;
#ifdef USES_ARCH_ARM64
typedef uint64_t dma_addr_t;
#else
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
typedef uint32_t dma_addr_t;
#endif
struct decon_win_rect {
  int x;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  int y;
  u32 w;
  u32 h;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct decon_rect {
  int left;
  int top;
  int right;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  int bottom;
};
struct s3c_fb_user_window {
  int x;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  int y;
};
struct s3c_fb_user_plane_alpha {
  int channel;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  unsigned char red;
  unsigned char green;
  unsigned char blue;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct s3c_fb_user_chroma {
  int enabled;
  unsigned char red;
  unsigned char green;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  unsigned char blue;
};
enum decon_pixel_format {
  DECON_PIXEL_FORMAT_ARGB_8888 = 0,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  DECON_PIXEL_FORMAT_ABGR_8888,
  DECON_PIXEL_FORMAT_RGBA_8888,
  DECON_PIXEL_FORMAT_BGRA_8888,
  DECON_PIXEL_FORMAT_XRGB_8888,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  DECON_PIXEL_FORMAT_XBGR_8888,
  DECON_PIXEL_FORMAT_RGBX_8888,
  DECON_PIXEL_FORMAT_BGRX_8888,
  DECON_PIXEL_FORMAT_RGBA_5551,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  DECON_PIXEL_FORMAT_RGB_565,
  DECON_PIXEL_FORMAT_NV16,
  DECON_PIXEL_FORMAT_NV61,
  DECON_PIXEL_FORMAT_YVU422_3P,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  DECON_PIXEL_FORMAT_NV12,
  DECON_PIXEL_FORMAT_NV21,
  DECON_PIXEL_FORMAT_NV12M,
  DECON_PIXEL_FORMAT_NV21M,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  DECON_PIXEL_FORMAT_YUV420,
  DECON_PIXEL_FORMAT_YVU420,
  DECON_PIXEL_FORMAT_YUV420M,
  DECON_PIXEL_FORMAT_YVU420M,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  DECON_PIXEL_FORMAT_NV12N,
  DECON_PIXEL_FORMAT_NV12N_10B,
  DECON_PIXEL_FORMAT_MAX,
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
enum decon_blending {
  DECON_BLENDING_NONE = 0,
  DECON_BLENDING_PREMULT = 1,
  DECON_BLENDING_COVERAGE = 2,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  DECON_BLENDING_MAX = 3,
};
enum dpp_flip {
  DPP_FLIP_NONE = 0x0,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  DPP_FLIP_X,
  DPP_FLIP_Y,
  DPP_FLIP_XY,
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
enum dpp_comp_src {
  DPP_COMP_SRC_NONE = 0,
  DPP_COMP_SRC_G2D,
  DPP_COMP_SRC_GPU
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
enum dpp_csc_eq {
  CSC_STANDARD_SHIFT = 0,
  CSC_BT_601 = 0,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  CSC_BT_709 = 1,
  CSC_BT_2020 = 2,
  CSC_DCI_P3 = 3,
  CSC_RANGE_SHIFT = 6,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  CSC_RANGE_LIMITED = 0x0,
  CSC_RANGE_FULL,
};
enum decon_idma_type {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  IDMA_G0 = 0x0,
  IDMA_G1,
  IDMA_VG0,
  IDMA_VG1,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  IDMA_VGF0,
  IDMA_VGF1,
  ODMA_WB,
  IDMA_G0_S,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  MAX_DECON_DMA_TYPE
};
struct dpp_params {
  dma_addr_t addr[MAX_PLANE_CNT];
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  enum dpp_flip flip;
  enum dpp_csc_eq eq_mode;
  enum dpp_comp_src comp_src;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct decon_frame {
  int x;
  int y;
  u32 w;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  u32 h;
  u32 f_w;
  u32 f_h;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct decon_win_config {
  enum {
    DECON_WIN_STATE_DISABLED = 0,
    DECON_WIN_STATE_COLOR,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
    DECON_WIN_STATE_BUFFER,
    DECON_WIN_STATE_UPDATE,
  } state;
  union {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
    __u32 color;
    struct {
      int fd_idma[3];
      int fence_fd;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
      int plane_alpha;
      enum decon_blending blending;
      enum decon_idma_type idma_type;
      enum decon_pixel_format format;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
      struct dpp_params dpp_parm;
      struct decon_win_rect block_area;
      struct decon_win_rect transparent_area;
      struct decon_win_rect opaque_area;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
      struct decon_frame src;
    };
  };
  struct decon_frame dst;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  bool protection;
  bool compression;
};
struct decon_win_config_data {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  int fence;
  int fd_odma;
  struct decon_win_config config[MAX_DECON_WIN + 1];
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct decon_dual_display_blank_data {
  enum {
    DECON_PRIMARY_DISPLAY = 0,
    DECON_SECONDARY_DISPLAY,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  } display_type;
  int blank;
};
enum disp_pwr_mode {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  DECON_POWER_MODE_OFF = 0,
  DECON_POWER_MODE_DOZE,
  DECON_POWER_MODE_NORMAL,
  DECON_POWER_MODE_DOZE_SUSPEND,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
struct decon_hdr_capabilities {
  unsigned int out_types[HDR_CAPABILITIES_NUM];
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct decon_hdr_capabilities_info {
  int out_num;
  int max_luminance;
  int max_average_luminance;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  int min_luminance;
};
#define S3CFB_WIN_POSITION _IOW('F', 203, struct s3c_fb_user_window)
#define S3CFB_WIN_SET_PLANE_ALPHA _IOW('F', 204, struct s3c_fb_user_plane_alpha)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define S3CFB_WIN_SET_CHROMA _IOW('F', 205, struct s3c_fb_user_chroma)
#define S3CFB_SET_VSYNC_INT _IOW('F', 206, __u32)
#define S3CFB_DECON_SELF_REFRESH _IOW('F', 207, __u32)
#define S3CFB_GET_ION_USER_HANDLE _IOWR('F', 208, struct s3c_fb_user_ion_client)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define S3CFB_WIN_CONFIG _IOW('F', 209, struct decon_win_config_data)
#define S3CFB_FORCE_PANIC _IOW('F', 211, __u32)
#define EXYNOS_GET_DISPLAYPORT_CONFIG _IOW('F', 300, struct exynos_displayport_data)
#define EXYNOS_SET_DISPLAYPORT_CONFIG _IOW('F', 301, struct exynos_displayport_data)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define S3CFB_POWER_MODE _IOW('F', 223, __u32)
#define EXYNOS_SET_HPD_FOR_TEST _IOW('F', 999, struct exynos_displayport_data)
#define S3CFB_GET_HDR_CAPABILITIES _IOW('F', 400, struct decon_hdr_capabilities)
#define S3CFB_GET_HDR_CAPABILITIES_NUM _IOW('F', 401, struct decon_hdr_capabilities_info)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#endif
