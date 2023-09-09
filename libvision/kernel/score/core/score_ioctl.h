//------------------------------------------------------------------------------
/// @file  score_ioctl.h
/// @ingroup  core
///
/// @brief  Declarations of ioctl macro about SCore
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

#ifndef COMMON_SCORE_IOCTL_H
#define COMMON_SCORE_IOCTL_H

namespace score {

/// @brief  The IPC packet is command sent through IPC queue
///
/// This is referred to in score_command.h
struct _ScoreIpcPacket;
typedef struct _ScoreIpcPacket ScoreIpcPacket;

/// @brief  Magic number of SCore ioctl
#define SCORE_IOC_MAGIC  'S'

/// @brief  IOCTL command to execute SCore
///
/// Send ScoreIpcPacket data to SCore through IPC queue and execute SCore kernel
//#define SCORE_IOCTL  _IOWR(SCORE_IOC_MAGIC, 0, ScoreIpcPacket)
#define SCORE_ALLOC_IPC_PACKET_MEM  _IOWR(SCORE_IOC_MAGIC, 0, ScoreIpcPacket)
#define SCORE_PUSH_KERNEL_PARAM     _IOWR(SCORE_IOC_MAGIC, 1, ScorePacketGroup)
#define SCORE_RUN                   _IOWR(SCORE_IOC_MAGIC, 2, void *)


} //namespace score
#endif
