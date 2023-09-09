//------------------------------------------------------------------------------
/// @file  score_core.h
/// @ingroup  core
///
/// @brief  Declarations of ScoreCore class
///
/// This is a singleton pattern providing the interface of accessing SCore device
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

#ifndef COMMON_SCORE_CORE_H_
#define COMMON_SCORE_CORE_H_

#include <unistd.h>

namespace score {
/// @brief  Class for command to execute SCore kernel
///
/// This is referred to in score_command.h
class ScoreCommand;
/// @brief  Class for the basic form of SCore device
///
/// This is referred to in score_device.h
class ScoreDevice;
/// @brief  The top class to execute SCore
class ScoreCore {
 public:
  /// @brief  Destructor of ScoreCore class
  ///
  /// This closes SCore device
  virtual ~ScoreCore();
  /// @brief  Execute SCore
  /// @param  Kernel Enum value about SCore kernel
  /// @param  cmd    Total packet sent to SCore
  /// @retval 0 if function succeeds, otherwise error
  int DoOnScore(unsigned int Kernel, ScoreCommand &cmd);
 public:
  /// @brief  Constructor of ScoreCore class
  ///
  /// This allocates memory for ScoreDevice class and opens SCore device
  ScoreCore();

 private:
  /// @brief  Pointer to real or emulator ScoreDevice class
  ScoreDevice *device_;
};

} //namespace score
#endif
