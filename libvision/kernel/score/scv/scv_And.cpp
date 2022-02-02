#include <cstring>

#include "score.h"
#include "score_command.h"
#include "scv.h"

namespace score {

sc_status_t scvAnd(ScBuffer* in1, ScBuffer* in2, ScBuffer* out)
{
  score::ScoreCommand cmd(SCV_AND);

  cmd.Put(in1);
  cmd.Put(in2);
  cmd.Put(out);

  if (score::DoOnScore(SCV_AND, cmd) < 0)
  {
    return STS_FAILURE;
  }

  return STS_SUCCESS;
}

}
