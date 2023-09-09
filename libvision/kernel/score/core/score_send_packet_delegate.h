//------------------------------------------------------------------------------
/// @file  send_packet_delegate.h
/// @ingroup  core
///
/// @brief  Declarations of SendPacketDelegate interface class
/// @author  Dongseok Han<ds0920.han@samsung.com>
///
/// @section changelog Change Log
/// 2016/07/18 Dongseok Han created
///
/// @section copyright_section Copyright
/// &copy; 2016, Samsung Electronics Co., Ltd.
///
/// @section license_section License
//------------------------------------------------------------------------------
#ifndef COMMON_SEND_PACKET_DELEGATE_H_
#define COMMON_SEND_PACKET_DELEGATE_H_

#include "score_ioctl.h"
#include "score_command.h"
#include "logging.h"
#include "sc_data_type.h"

namespace score {

/// @brief  A interface for SendPacket logics it depends on each platform
class SendPacketDelegate {
 public:
 /// @brief Interface to send a packet to SCore
 /// @param fd        File descriptor of SCore
 /// @param packet    Packet class to be sent
 /// @retval  0 if succeed, otherwise non-zero value
  virtual int SendPacket(int fd, const ScoreIpcPacket *const packet) = 0;
  virtual int SetControl(int fd, unsigned int cmd, unsigned int val) = 0;
  virtual ~SendPacketDelegate() {}
};

} //namespace score

#endif
