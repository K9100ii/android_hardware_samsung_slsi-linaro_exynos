#include "score.h"
#include "score_command.h"
#include "scv.h"

namespace score {
/// @ingroup SCV_kernels
/// @brief  Performs convolution of size 3, 5, and 7
/// @param  [in] in1 Input image buffer (sc_u8)
/// @param  [in] in2 Input kernel coefficients (sc_s16)
/// @param  [out] out Output image buffer (sc_s16)
/// @param  [in] kernel_size Size of 2D kernel (3,5 or 7)
/// @param  [in] k_norm Kernel's normalization factor
/// @retval STS_SUCCESS if functions succeeds, else error
sc_status_t scvConvolution(ScBuffer* in1, ScBuffer* in2, ScBuffer* out,
                         sc_u32 kernel_size, sc_s32 k_norm)
{
  score::ScoreCommand cmd(SCV_CONVOLUTION);

  cmd.Put(in1);
  cmd.Put(in2);
  cmd.Put(out);
  cmd.Put(&kernel_size);
  cmd.Put(&k_norm);

  if (score::DoOnScore(SCV_CONVOLUTION, cmd) < 0)
  {
    return STS_FAILURE;
  }

  return STS_SUCCESS;
}

} /// namespace score
