//------------------------------------------------------------------------------
/// @file  sc_ipc_packet.h
/// @ingroup  include
/// 
/// @brief  Declarations of the IPC packet shared between SCore and other IPs
///
/// This file declares IPC packet struct about those. IPC packet is the information
/// shared between Host and other external IPs to execute SCore kernel.
///
/// @author  Dongseok Han<ds0920.han@samsung.com>
/// 
/// @section changelog Change Log
/// 2016/06/01 Dongseok Han created
/// 
/// @section copyright_section Copyright
/// &copy; 2016, Samsung Electronics Co., Ltd.
/// 
/// @section license_section License
//------------------------------------------------------------------------------
#ifndef __SSDF_DSP_COMMON_INCLUDE_SC_IPC_PACKET_H__
#define __SSDF_DSP_COMMON_INCLUDE_SC_IPC_PACKET_H__

/// @brief  IPC packet header contains meta information for calling SCore kernel
typedef struct _score_packet_header_t {
  unsigned int queue_id:    2;    ///< Uniqueue ID of message queue
  unsigned int kernel_name: 12;   ///< Name of kernel
  unsigned int task_id:     7;    ///< Uniqueue Id internally assigned to every task
  unsigned int reserved:    8;    ///< RESERVED
  unsigned int worker_name: 3;    ///< DMA option
} ScorePacketHeader, score_packet_header_t;


/// @brief  IPC packet header contains about packet size
typedef struct _score_packet_size_t {
  unsigned int packet_size: 14;   ///< Packet size of whole packets to send
  unsigned int reserved:    10;   ///< RESERVED
  unsigned int group_count: 8;    ///< Number of group in a packet
} ScorePacketSize, score_packet_size_t;

#endif
