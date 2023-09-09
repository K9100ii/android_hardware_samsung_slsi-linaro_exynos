//------------------------------------------------------------------------------
/// @file  buf_type.h
/// @ingroup  include
///
/// @brief  Defines of structures related with data buffer
/// @author  Rakie Kim<rakie.kim@samsung.com>
///
/// @section changelog Change Log
/// 2016/04/12 Rakie Kim created
///
/// @section copyright_section Copyright
/// &copy; 2016, Samsung Electronics Co., Ltd.
///
/// @section license_section License
///
//------------------------------------------------------------------------------

#ifndef __SSDF_DSP_COMMON_INCLUDE_SC_BUF_TYPE_H__
#define __SSDF_DSP_COMMON_INCLUDE_SC_BUF_TYPE_H__

#include <sc_types.h>

/// @brief  structure of plane-type for buffer
typedef struct _data_buf_type {
  sc_u32 sc_type     : 8; ///< index of type
  sc_u32 plane0      : 6; ///< pixel bit of plain0
  sc_u32 plane1      : 6; ///< pixel bit of plain1
  sc_u32 plane2      : 6; ///< pixel bit of plain2
  sc_u32 plane3      : 6; ///< pixel bit of plain3

  bool operator==(const _data_buf_type &cmp)
  {
    return sc_type == cmp.sc_type;
  }
} data_buf_type;

#define CREATE_TYPE(IDX, PLANE0, PLANE1, PLANE2, PLANE3) \
  {IDX, PLANE0, PLANE1, PLANE2, PLANE3}

#define SUM_OF_PLANE(TYPE) TYPE.plane0 + TYPE.plane1 + TYPE.plane2 + \
  TYPE.plane3

#define TYPE_SZ(TYPE) (SUM_OF_PLANE(TYPE) / 8) + \
  ((SUM_OF_PLANE(TYPE)%8) ? 1 : 0)

#define INIT_BUF_TYPE(T) {T.sc_type, T.plane0, T.plane1, T.plane2, T.plane3}

#define SC_BGR         (data_buf_type)CREATE_TYPE(0, 24, 0, 0, 0)
#define SC_BGR565      (data_buf_type)CREATE_TYPE(1, 16, 0, 0, 0)
#define SC_BGRA        (data_buf_type)CREATE_TYPE(2, 32, 0, 0, 0)
#define SC_ARGB        (data_buf_type)CREATE_TYPE(3, 32, 0, 0, 0)
#define SC_ARGB4444    (data_buf_type)CREATE_TYPE(4, 16, 0, 0, 0)
#define SC_RGB         (data_buf_type)CREATE_TYPE(5, 24, 0, 0, 0)
#define SC_RGB565      (data_buf_type)CREATE_TYPE(6, 16, 0, 0, 0)
#define SC_RGBA        (data_buf_type)CREATE_TYPE(7, 32, 0, 0, 0)
#define SC_RGB8        (data_buf_type)CREATE_TYPE(8, 8, 0, 0, 0)
#define SC_YUV4        (data_buf_type)CREATE_TYPE(9, 8, 8, 8, 0)
#define SC_YV16        (data_buf_type)CREATE_TYPE(10, 8, 4, 4, 0)
#define SC_YV12        (data_buf_type)CREATE_TYPE(11, 8, 2, 2, 0)
#define SC_IYUV        (data_buf_type)CREATE_TYPE(12, 8, 2, 2, 0)
#define SC_NV12        (data_buf_type)CREATE_TYPE(13, 8, 4, 0, 0)
#define SC_NV21        (data_buf_type)CREATE_TYPE(14, 8, 4, 0, 0)
#define SC_NV16        (data_buf_type)CREATE_TYPE(15, 8, 8, 0, 0)
#define SC_YUV444pp    (data_buf_type)CREATE_TYPE(16, 8, 16, 0, 0)
#define SC_UYVY        (data_buf_type)CREATE_TYPE(17, 8, 4, 4, 0)
#define SC_YUYV        (data_buf_type)CREATE_TYPE(18, 8, 4, 4, 0)
#define SC_YVU         (data_buf_type)CREATE_TYPE(19, 0, 0, 0, 0)
#define SC_YVUINTLV    (data_buf_type)CREATE_TYPE(20, 0, 0, 0, 0)
#define SC_LAB         (data_buf_type)CREATE_TYPE(21, 0, 0, 0, 0)
#define SC_HSV         (data_buf_type)CREATE_TYPE(22, 8, 8, 8, 0)
#define SC_H1V1        (data_buf_type)CREATE_TYPE(23, 0, 0, 0, 0)
#define SC_H1V2        (data_buf_type)CREATE_TYPE(24, 0, 0, 0, 0)
#define SC_H2V1        (data_buf_type)CREATE_TYPE(25, 0, 0, 0, 0)
#define SC_H2V2        (data_buf_type)CREATE_TYPE(26, 0, 0, 0, 0)
#define SC_TYPE_S8     (data_buf_type)CREATE_TYPE(27, 8, 0, 0, 0)
#define SC_TYPE_U8     (data_buf_type)CREATE_TYPE(28, 8, 0, 0, 0)
#define SC_TYPE_S16    (data_buf_type)CREATE_TYPE(29, 16, 0, 0, 0)
#define SC_TYPE_U16    (data_buf_type)CREATE_TYPE(30, 16, 0, 0, 0)
#define SC_TYPE_S32    (data_buf_type)CREATE_TYPE(31, 32, 0, 0, 0)
#define SC_TYPE_U32    (data_buf_type)CREATE_TYPE(32, 32, 0, 0, 0)
#define SC_TYPE_F32    (data_buf_type)CREATE_TYPE(33, 32, 0, 0, 0)
#define SC_TYPE_VIR    (data_buf_type)CREATE_TYPE(34, 32, 0, 0, 0)
#define SC_TYPE_RANDOM (data_buf_type)CREATE_TYPE(35, 32, 0, 0, 0)
#define SC_GRAY        SC_TYPE_U8

/// @brief  Gets a data_buf_type by matching sc_type and idx
/// @retval Plane-type for buffer corresponding to idx
data_buf_type data_buf_type_from_sc_type(unsigned type_idx);

/// @brief  Image buffer handler
///
/// This data structure handles any image buffer in SCore.
typedef struct _sc_buffer {
  sc_u32 width;        ///< Width of image (in pixel)
  sc_u32 height;       ///< Height of image (in pixel)
  data_buf_type type;  ///< Memory and data type
  sc_u32 addr;         ///< Address of the start of image data
} sc_buffer;

#endif
