//------------------------------------------------------------------------------
/// @file  score_device_real.h
/// @ingroup  core
///
/// @brief  Declarations of ScoreDeviceReal class
///
/// This file includes functions to control SCore device driver
///
/// @author  Rakie Kim<rakie.kim@samsung.com>
///
/// @section changelog Change Log
/// 2015/11/16 Rakie Kim created
/// 2015/12/09 Sanghwan Park commented
/// 2016/07/18 Dongseok Han modified
///
/// @section copyright_section Copyright
/// &copy; 2015, Samsung Electronics Co., Ltd.
///
/// @section license_section License
//------------------------------------------------------------------------------

#ifndef COMMON_SCORE_DEVICE_REAL_H_
#define COMMON_SCORE_DEVICE_REAL_H_

#include "score_device.h"

namespace score {

/// @brief  Class for a real SCore device driver
class ScoreDeviceReal : public ScoreDevice {
 public:
  /// @brief  Constructor of ScoreDeviceReal class
  ///
  /// This initializes score_fd_.
  explicit ScoreDeviceReal();
  /// @brief  Destructor of ScoreDeviceReal class
  ///
  /// This is empty.
  virtual ~ScoreDeviceReal();

  /// @{
  /// Interface for accessing SCore device. Please refer to class ScoreDevice.
  int OpenDevice();
  void CloseDevice();
  int Write(void *buf, size_t size);
  int Read(void *buf, size_t size);
  int SendPacket(const ScoreIpcPacket *const packet);
  int SetControl(unsigned int cmd, unsigned int val);
  /// @}

 private:
  /// @brief  File descriptor of SCore device
  int score_fd_;
};

} //namespace score
#endif
