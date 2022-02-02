//------------------------------------------------------------------------------
/// @file  scv.h
/// @ingroup  include
///
/// @brief  Declarations of SCore kernel from host
/// @author  Sanghwan Park<senius.park@samsung.com>
///
/// @section changelog Change Log
/// 2016/03/02 Sanghwan Park created
///
/// @section copyright_section Copyright
/// &copy; 2016, Samsung Electronics Co., Ltd.
///
/// @section license_section License
//------------------------------------------------------------------------------

#ifndef SSF_HOST_INCLUDE_SCV_H_
#define SSF_HOST_INCLUDE_SCV_H_

#include "sc_data_type.h"
#include "sc_kernel_name.h"

namespace score {
class Sequencer;
/// @{
/// Declarations of S.LSI test kernel wrappers in host side
sc_status_t slsiTest00(ScBuffer *in, ScBuffer *out,
                       int case_num, int score_debug);  // ASB & PSVE
/// @}

/// @{
/// Declarations of Exception tset kernel wrappers in host side
sc_status_t exceptionTest00(ScBuffer *in, ScBuffer *out);  // ASSERT
/// @}

/// @{
/// Declarations of SCV test kernel wrappers in host side
sc_status_t scvTest00(ScBuffer *in, ScBuffer *out);  // not
sc_status_t scvTest01(ScBuffer *in, ScBuffer *out, int kernel_size);  // erode
sc_status_t scvTest02(ScBuffer *in1, ScBuffer *out);  // integralimage
sc_status_t scvTest03(ScBuffer *in1, ScBuffer *out,
                      sc_enum_e policy);  //integralline
sc_status_t scvTest04(ScBuffer *in, ScBuffer *out,
                      int kernel_size);  // gaussian
sc_status_t scvTest05(ScBuffer *in, ScBuffer *out1, ScBuffer *out2,
                      int kernel_size, sc_enum_e policy);  // sobel
sc_status_t scvTest06(ScBuffer *in1, ScBuffer *in2, ScBuffer *out,
                      sc_enum_e policy);  // magnitude
sc_status_t scvTest07(ScBuffer *in1, ScBuffer *in2, ScBuffer *out);  // phase
sc_status_t scvTest08(
  ScBuffer *in1, ScBuffer *dir, ScBuffer *out, int th_low, int th_high,
  int cedge, int pedge);  // nonmaximasuppression_direction
sc_status_t scvTest09(ScBuffer *in1, ScBuffer *out, sc_enum_e norm,
                      int th_low, int th_high);  // cannyedgedetector
sc_status_t scvTest10(ScBuffer *in1, ScBuffer *out, sc_float32 scale); // accumulate_weighted_image
sc_status_t scvTest11(ScBuffer* in1, ScBuffer* mean, ScBuffer* stddev); // meanstddev
sc_status_t scvTest12(ScBuffer *in1, ScBuffer *out); // minmaxloc
sc_status_t scvTest13(ScBuffer* in1, ScBuffer* in2, ScBuffer* out, sc_enum_e policy); // multiply
sc_status_t scvTest14(ScBuffer* in, ScBuffer* out); // colorconvert
sc_status_t scvTest15(ScBuffer* in1, ScBuffer* in2, ScBuffer* out,
                      sc_u32 kernel_size, sc_s32 k_norm); // convolution
sc_status_t scvTest16(ScBuffer* in1, ScBuffer* out); // equalizehist
sc_status_t scvTest17(ScBuffer *in1, ScBuffer *out); // median3x3
sc_status_t scvTest18(ScBuffer* in, ScBuffer* out); // histogram
sc_status_t scvTest19(ScBuffer* in1, ScBuffer* out1, ScBuffer* out2, ScBuffer* out3, ScBuffer* out4); // channelextract_4ch
sc_status_t scvTest20(ScBuffer* in1, ScBuffer* in2, ScBuffer* in3, ScBuffer* in4, ScBuffer* out); // channelcombine_4ch
sc_status_t scvTest21(ScBuffer* in1, ScBuffer* out, sc_s32 threshold, sc_enum_e corner_policy, sc_enum_e nms_policy); // fastcorners
sc_status_t scvTest22(ScBuffer* in1, ScBuffer* out, sc_s32 th_low, sc_s32 th_high, sc_s32 cedge, sc_s32 pedge); // nonmaximasuppression_nodirection
sc_status_t scvTest23(ScBuffer* in1, ScBuffer* out, sc_enum_e policy); // pyramid
sc_status_t scvTest24(ScBuffer* in1, ScBuffer* out, sc_enum_e policy); // scaleimage
/// @}

class ScoreCommand;

sc_status_t scvAnd(ScBuffer *in1, ScBuffer *in2, ScBuffer *out);
sc_status_t scvCannyEdgeDetector(ScBuffer *in, ScBuffer *out, sc_enum_e norm,
  sc_s32 th_low, sc_s32 th_high);
sc_status_t scvFastCorners(ScBuffer *in1, ScBuffer *out, sc_s32 threshold,
  sc_enum_e corner_policy, sc_enum_e nms_policy);
sc_status_t scvConvolution(ScBuffer* in1, ScBuffer* in2, ScBuffer* out,
  sc_u32 kernel_size, sc_s32 k_norm);
sc_status_t scvHistogram(ScBuffer* in1, ScBuffer* out);
sc_status_t scvTableLookup(ScBuffer* in1, ScBuffer* in2, ScBuffer* out);


} // namespace score

#endif

