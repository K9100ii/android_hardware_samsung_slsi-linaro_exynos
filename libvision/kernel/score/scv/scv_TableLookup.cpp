#include <cstring>

#include "score.h"
#include "score_command.h"
#include "scv.h"

namespace score {

/// @ingroup SCV_kernels
/// @brief  Performs a look-up table transform of an array.
/// the function LUT fills the output array with values from
/// the look-up table. Indices of the entries are taken from the input array.
/// @param [in] in1 input buffer(sc_u8)
/// @param [in] in2 input buffer(sc_u8)
/// @param [out] out output buffer(sc_u8)
/// @retval STS_SUCCESS if function succeeds, otherwise error
sc_status_t scvTableLookup(ScBuffer* in1, ScBuffer* in2, ScBuffer* out)
{
    score::ScoreCommand cmd(SCV_TABLELOOKUP);

    cmd.Put(in1);
    cmd.Put(in2);
    cmd.Put(out);

    if (score::DoOnScore(SCV_TABLELOOKUP, cmd) < 0)
    {
      return STS_FAILURE;
    }

    return STS_SUCCESS;
}

} /// namespace score
