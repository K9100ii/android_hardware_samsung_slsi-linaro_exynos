//------------------------------------------------------------------------------
/// @file  score_device_real.cc
/// @ingroup  core
///
/// @brief  Implementations of ScoreDeviceReal class
///
/// This file includes functions to control SCore device driver
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
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <ExynosVisionCommonConfig.h>

#include "score_device_real.h"
#include "score_ioctl.h"
#include "score_command.h"
#include "logging.h"
#include "sc_data_type.h"

#include "score_send_packet_delegate.h"
#include "score_send_packet.h"
namespace score {

/// @brief  Real device name of SCore
//#define SCORE_DEVICE_NAME "/dev/score"
#define SCORE_DEVICE_NAME "/dev/vertex1"

ScoreDeviceReal::ScoreDeviceReal() : score_fd_(-1) {}

ScoreDeviceReal::~ScoreDeviceReal() {}

int ScoreDeviceReal::OpenDevice() {
  score_fd_ = open(SCORE_DEVICE_NAME, O_RDWR);
  VXLOGI("Score Device Open (fd = %d)", score_fd_);

  if (score_fd_ < 0) {
    sc_error("SCore Device Open Fail:[%s]\n", SCORE_DEVICE_NAME);
    exit(-1);
  }

  return 0;
}

int ScoreDeviceReal::SetControl(unsigned int cmd, unsigned int val) {
  SendPacketDelegate *delegator = new ExynosSendPacket();
  if(delegator == NULL)
    return -1;

  int ret = delegator->SetControl(score_fd_, cmd, val);
  delete delegator;

  return ret;
}

void ScoreDeviceReal::CloseDevice() {
  if (score_fd_ != -1) {
    close(score_fd_);
    VXLOGI("Score Device Close (fd = %d)", score_fd_);
  }
}

int ScoreDeviceReal::Write(void *buf, size_t size) {
  if (score_fd_ < 0) {
    return -1;
  }
  return write(score_fd_, buf, size);
}

int ScoreDeviceReal::Read(void *buf, size_t size) {
  if (score_fd_ < 0) {
    return -1;
  }
  return read(score_fd_, buf, size);
}

int ScoreDeviceReal::SendPacket(const ScoreIpcPacket *const packet) {
  if (score_fd_ < 0 || packet == NULL) {
    return -1;
  }
  SendPacketDelegate *delegator = new ExynosSendPacket();
  if(delegator == NULL)
    return -1;

  int ret = delegator->SendPacket(score_fd_, packet);
  delete delegator;

  return ret;
}

} //namespace score
