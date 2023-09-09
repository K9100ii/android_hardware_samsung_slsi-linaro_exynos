//------------------------------------------------------------------------------
/// @file  score_core.cc
/// @ingroup  core
///
/// @brief  Implementations of ScoreCore class
///
/// This is the implementation of ScoreCore interface.
///
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

#include <stdlib.h>

#include "score_core.h"
#include "score_command.h"
#include "score_device.h"
#include "score_device_real.h"

namespace score {

ScoreCore::ScoreCore()
    : device_(NULL) {
  device_ = new ScoreDeviceReal();
  device_->OpenDevice();
}

ScoreCore::~ScoreCore() {
  if (device_ != NULL) {
    device_->CloseDevice();
  }
}

int ScoreCore::DoOnScore(unsigned int kernel, ScoreCommand &cmd) {
  // TODO Check ScoreCommand is valid
  cmd.PutKernelInfo((sc_kernel_name_e)kernel);
  return device_->SendPacket(cmd.GetPacket());
}

} //namespace score
