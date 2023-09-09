//------------------------------------------------------------------------------
/// @file  score_kernel_name.h
/// @ingroup  include
///
/// @brief  Name of the kernels implemented in SCore
/// @author  Minwook Ahn<minwook.ahn@samsung.com>
///
/// @section changelog Change Log
/// 2015/04/14 Minwook Ahn created
/// 2016/06/13 Sanghwan Park modified
/// 2016/06/20 Nahyun Kim modified
///
/// @section copyright_section Copyright
/// &copy; 2015, Samsung Electronics Co., Ltd.
///
//------------------------------------------------------------------------------

#ifndef __SSF_COMMON_INCLUDE_SC_KERNEL_NAME_H__
#define __SSF_COMMON_INCLUDE_SC_KERNEL_NAME_H__

#include <sc_types.h>

/// @brief  Maximum size of fixed SCore kernel
#define SC_KERNEL_SIZE               100
/// @brief  Maximum size of kernel that user can define
#define SC_USER_DEFINED_KERNEL_SIZE  100
/// @brief  Maximum size of test kernel
#define SC_TEST_KERNEL_SIZE          100

/// @brief  Maximum size of All SCore kernel
///
/// SC_KERNEL_SIZE + SC_USER_DEFINED_KERNEL_SIZE
#define MAX_KERNEL_SIZE  SC_KERNEL_SIZE + SC_USER_DEFINED_KERNEL_SIZE + \
                         SC_TEST_KERNEL_SIZE

/// @brief  First number that USER_DEFINED_KERNEL can have
#define SC_USER_DEFINED_KERNEL_START  (SC_KERNEL_SIZE)
/// @brief  First number that USER_DEFINED_KERNEL can have
#define SC_USER_DEFINED_KERNEL_END  (SC_KERNEL_SIZE + \
                                     SC_USER_DEFINED_KERNEL_SIZE)

/// @brief  Registe number of USER_DEFINED_KERNEL you make
/// @param  _x_  Number of USER_DEFINED_KERNEL
///
/// You must input number from 0 to SC_USER_DEFINED_KERNEL_SIZE-1
#define SC_USER_KERNEL_NUM(_x_) ((SC_USER_DEFINED_KERNEL_START) + (_x_))

/// @brief  Name of kernels
/// @ingroup common
///
/// SCore kernel developer should update this whenever he/she develops a new kernel.
/// In order for AP-CPU to use SCore a kernel function, it uses the name of the kernel
/// from this enumeration.
enum sc_kernel_name_e {
  /* 0 */ SEQUENCER = 0,
  SCV_FASTCORNERS_NMS_USE,
  SCV_FASTCORNERS_NMS_NO_USE,
  SCV_NONMAXIMASUPPRESSION_DIRECTION,
  SCV_NONMAXIMASUPPRESSION_NODIRECTION,
  SCV_PYRAMID,
  SCV_SCALEIMAGE_X2,
  SCV_SCALEIMAGE_X4,
  SCV_SCALEIMAGE_X0_5,
  SCV_SCALEIMAGE_X0_25,
  /* 10 */ SCV_SCALEIMAGE_ANY,
  SCV_AVERAGE,
  SCV_BOX,
  SCV_GAUSSIAN,
  SCV_CONVOLUTION,
  SCV_ERODE,
  SCV_DILATE,
  SCV_INTEGRALLINE,
  SCV_INTEGRALIMAGE,
  SCV_HISTOGRAM,
  /* 20 */ SCV_EQUALIZEHIST,
  SCV_ABS,
  SCV_ABSDIFF,
  SCV_ADD,
  SCV_SUBTRACT,
  SCV_MULTIPLY,
  SCV_DIVIDE,
  SCV_ACCUMULATEIMAGE,
  SCV_ACCUMULATEPRODUCT,
  SCV_ACCUMULATESQUAREIMAGE,
  /* 30 */ SCV_ACCUMULATEWEIGHTEDIMAGE,
  SCV_AND,
  SCV_NOT,
  SCV_OR,
  SCV_XOR,
  SCV_NEG,
  SCV_COMPARE,
  SCV_MIN,
  SCV_KMINLOC,
  SCV_MINLOC,
  /* 40 */ SCV_MAX,
  SCV_KMAXLOC,
  SCV_MAXLOC,
  SCV_MINMAXLOC,
  SCV_MEANSTDDEV,
  SCV_CHANNELCOMBINE_3CH,
  SCV_CHANNELCOMBINE_4CH,
  SCV_CHANNELEXTRACT_3CH,
  SCV_CHANNELEXTRACT_4CH,
  SCV_CANNYEDGEDETECTOR,
  /* 50 */ SCV_COLORCONVERT,
  SCV_CONVERTDEPTH,
  SCV_HARRISCORNERS,
  SCV_MAGNITUDE,
  SCV_MEDIAN3X3,
  SCV_OPTICALFLOWPYRLK,
  SCV_PHASE,
  SCV_SOBEL_SINGLE,
  SCV_SOBEL_DOUBLE,
  SCV_TABLELOOKUP,
  /* 60 */ SCV_THRESHOLD_BINARY,
  SCV_THRESHOLD_RANGE,
  SCV_CONVOLUTIONVARWIDTH,
  SCV_LOCALBINARYPATTEN,
  SCV_ROTATE,
  SCV_BILATERAL,
  SCV_MINMAX3x3,
  SCV_RMCT3x3,
  SCV_SCHARR3x3,
  SCV_CLIP,
  /* 70 */ SCV_RANGE,
  SCV_RANGECHECK,
  SCV_NORMHAMMING,
  // test kernel
  SLSI_TEST_00 = 200,
  EXCEPTION_TEST_00 = 210,
  SCV_TEST_00 = 230,
  SCV_TEST_01,
  SCV_TEST_02,
  SCV_TEST_03,
  SCV_TEST_04,
  SCV_TEST_05,
  SCV_TEST_06,
  SCV_TEST_07,
  SCV_TEST_08,
  SCV_TEST_09,
  /* 240 */ SCV_TEST_10,
  SCV_TEST_11,
  SCV_TEST_12,
  SCV_TEST_13,
  SCV_TEST_14,
  SCV_TEST_15,
  SCV_TEST_16,
  SCV_TEST_17,
  SCV_TEST_18,
  SCV_TEST_19,
  /* 250 */ SCV_TEST_20,
  SCV_TEST_21,
  SCV_TEST_22,
  SCV_TEST_23,
  SCV_TEST_24,
  SCV_KERNEL_END
};

//TODO
#define SCV_COPY_U8U16    60
#define SCV_COPY_U8U32    61
#define SCV_COPY_U16U32   62
#define SCV_COPY_U16U16   63
#define SCV_COPY_U8U8     64
#define SCV_COPY_U32U8    65
#define SCV_POSTPHASE     66
#define SCV_POSTMAGNITUDE 67
#define SCV_COPY_S16S16   69
#define SCV_COPY_U16S16   70

/// @brief  SCore kernel name registered
///
/// This is used for debugging and doesn't have an effect on the execution.
/// The content of this array must be the same with sc_kernel_name_e
extern sc_s8 sc_kernel_name[][50];

/// @brief  SCore test kernel name registered
///
/// This is used for debugging and doesn't have an effect on the execution.
/// The content of this array must be the same with sc_kernel_name_e
extern sc_s8 sc_test_kernel_name[][50];

#endif
