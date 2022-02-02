#include <cstring>

#include "score.h"
#include "score_command.h"
#include "scv.h"

namespace score {

/// @ingroup  SCV_kernels
/// @brief  Generates a histogram from the image and counts the number of
/// each pixel value
/// @param [in] in1 input buffer(sc_u8)
/// @param [out] out output buffer(width:256 / height:1 / sc_u32)
/// @retval STS_SUCCESS if function succeeds, otherwise error
sc_status_t scvHistogram(ScBuffer* in1, ScBuffer* out)
{
    score::ScoreCommand cmd(SCV_HISTOGRAM);

    cmd.Put(in1);
    cmd.Put(out);

    if (score::DoOnScore(SCV_HISTOGRAM, cmd) < 0)
    {
      return STS_FAILURE;
    }

    return STS_SUCCESS;
}

} /// namespace score
