//------------------------------------------------------------------------------
/// @file  sc_data_type.h
/// @ingroup  include
///
/// @brief  Declarations of buffer handler
/// @author  Sanghwan Park<senius.park@samsung.com>
///
/// @section changelog Change Log
/// 2016/06/16 Sanghwan Park created
///
/// @section copyright_section Copyright
/// &copy; 2016, Samsung Electronics Co., Ltd.
///
/// @section license_section License
//------------------------------------------------------------------------------

#ifndef SSF_HOST_INCLUDE_SC_DATA_TYPE_H_
#define SSF_HOST_INCLUDE_SC_DATA_TYPE_H_

#include <sc_types.h>
#include <sc_buf_type.h>
#include <sc_control.h>
#include <sc_kernel_policy.h>


namespace score {

/// @brief  Memory information of image buffer handler
///
/// This data structure handles memory info of any image buffer in Host.
#pragma pack(push,4)// This is necessary for 4 byte aligns
typedef struct _sc_host_buffer {
  int                memory_type;  ///< memory type like USERPTF or DMABUF
  int                fd;           ///< fd of ION
  unsigned int       addr32;       ///< address of 32bit library
  unsigned long long addr64;       ///< address of 64bit library
  //TODO : need to additional parameters for sharing memory
} sc_host_buffer;
#pragma pack(pop)

/// @brief  Image buffer handler
///
/// This data structure handles any image buffer in Host.
typedef struct _ScBuffer {
  sc_buffer      buf;       ///< image buffer handler in target
  sc_host_buffer host_buf;  ///< memory info of image buffer
} ScBuffer;

} // namespace score
#endif
