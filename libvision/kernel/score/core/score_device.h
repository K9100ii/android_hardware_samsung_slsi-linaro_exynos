//------------------------------------------------------------------------------
/// @file  score_device.h
/// @ingroup  core
///
/// @brief  Declarations of SCoreDevice class
///
/// This is the basic form of SCore device class. ScoreDeviceReal class and
/// ScoreDeviceEmulator class inherit this.
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

#ifndef COMMON_SCORE_DEVICE_H_
#define COMMON_SCORE_DEVICE_H_

namespace score {
/// @brief  Command sent through IPC queue to SCore
///
/// This is referred to in score_command.h
struct _ScoreIpcPacket;
typedef struct _ScoreIpcPacket ScoreIpcPacket;

/// @brief  Common device driver interface of SCore
class ScoreDevice {
 public:
  virtual ~ScoreDevice() {}
  // TODO:Change return value basing POSIX return value
  /// @brief  Open SCore device
  /// @retval 0 if function succeeds, otherwise error
  virtual int OpenDevice() = 0;
  /// @brief  Close SCore device
  virtual void CloseDevice() = 0;
  /// @brief  Write to SCore device
  /// @param  buf   Pointer to data to be written
  /// @param  size  Size of data to be written
  /// @return If function succeeds, this returns byte size written.
  ///
  /// This function does not anything actually.
  virtual int Write(void *buf, size_t size) = 0;
  /// @brief  Read from SCore device
  /// @param  buf   Pointer to data to be read from SCore device
  /// @param  size  Size of data to be read
  /// @return If function succeeds, this returns byte size read.
  ///
  /// This function does not anything actually.
  virtual int Read(void *buf, size_t size) = 0;
  /// @brief  Send a packet holding a command to execute a kernel in SCore
  /// @param  packet  Pointer to a packet holding a command to execute a kernel
  /// @retval 0 if function succeeds, otherwise error
  virtual int SendPacket(const ScoreIpcPacket *const packet) = 0;
};

} //namespace score
#endif
