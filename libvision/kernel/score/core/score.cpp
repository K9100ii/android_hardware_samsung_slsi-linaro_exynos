//------------------------------------------------------------------------------
/// @file  score.cc
/// @ingroup  core
///
/// @brief  Implementations of a function to execute a kernel in SCore
/// @author  Rakie Kim<rakie.kim@samsung.com>
///
/// @section changelog Change Log
/// 2015/11/16 Rakie Kim created
/// 2015/12/09 Sanghwan Park commented
///
/// @section copyright_section Copyright
/// &copy; 2015, Samsung Electronics Co., Ltd.
///
/// @section license_section License
//------------------------------------------------------------------------------

#include "score_core.h"
#include "score_command.h"

namespace score {

static ScoreCore* instance;
ScoreCore* ScoreInstance(void) {
  if (instance != NULL) {
    delete instance;
  }
  instance = new ScoreCore;
  return instance;
}

void ReleaseScoreInstance(void) {
  if (instance !=NULL) {
    delete instance;
    instance = NULL;
  }
}

int DoOnScore(unsigned int kernel, ScoreCommand &cmd) {
  return ScoreInstance()->DoOnScore(kernel, cmd);
}

} //namespace score
