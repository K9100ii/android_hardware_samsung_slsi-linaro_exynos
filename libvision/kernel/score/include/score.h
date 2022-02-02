//------------------------------------------------------------------------------
/// @file  score.h
/// @ingroup  include
///
/// @brief  Declarations of function to execute SCore
///
/// This file declares functions to execute SCore. Those are ScBuffer creator,
/// ScBuffer releaser and SCore kernel executer.
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

#ifndef SSF_HOST_INCLUDE_SCORE_H_
#define SSF_HOST_INCLUDE_SCORE_H_

#include <stdlib.h>
#include "sc_data_type.h"
#include "sc_kernel_policy.h"
#include "logging.h"
#include "scv.h"
#include "score_command.h"
#include "score_core.h"
#include "vs4l.h"
#include "vs4l_buffer_helper.h"


namespace score {

/// @brief  Get address of which ScBuffer manages area
/// @param  buffer  Pointer to ScBuffer
/// @retval unsigned long  Address of which ScBuffer manages area
unsigned long GetScBufferAddr(const ScBuffer *buffer);

/// @brief  Put address of user space to ScBuffer
/// @param  buffer  Pointer to ScBuffer
/// @param  unsigned long  Virtual address inputted according to memory type
/// @retval sc_status_t  STS_SUCCESS if it succeeds, otherwise STS_FAILURE
sc_status_t PutScBufferAddr(ScBuffer *buffer, unsigned long addr);

/// @brief  Copy memory information of which ScBuffer manages area
/// @param  dst  Pointer to dst ScBuffer
/// @param  src  Pointer to src ScBuffer
/// @retval sc_status_t  STS_SUCCESS if it succeeds, otherwise STS_FAILURE
sc_status_t CopyScBufferMemInfo(ScBuffer *dst, const ScBuffer *src);

/// @brief  Create ScBuffer
/// @param  width   Width of sc_data
/// @param  height  Height of sc_data
/// @param  type    Type of sc_data
/// @param  addr0~3 Pointer to sc_data to be processed depending on type
/// @retval ScBuffer Pointer to ScBuffer allocated or NULL pointer
///
/// This function allocates memory for ScBuffer and memory for addr parameter
/// included in ScBuffer. Size of addr parameter depends on type. When creating
/// ScBuffer as input data, you must put data to addr0~3 depending on type.
/// Value put to addr0~3 is pointer to sc_data processed in SCore. When creating
/// ScBuffer as output data, you don't have to put anything. Information about
/// type is referred to in sc_data_type.cc.\n
/// Example,\n
/// \b Case \b 1: input data that type is RGB\n
/// ScBuffer *in = score:CreateScBuffer(10, 10, RGB, data);\n
/// \b Case \b 2: input data that type is YUYV\n
/// ScBuffer *in = score:CreateScBuffer(20, 20, YUYV, data1, data2, data3);\n
/// \b Case \b 3: output data that type is HSV\n
/// ScBuffer *out = score:CreateScBuffer(30, 30, HSV);\n
ScBuffer *CreateScBuffer(int width, int height, data_buf_type type,
                         void *addr0 = NULL, void *addr1 = NULL,
                         void *addr2 = NULL, void *addr3 = NULL);

/// @brief  Create  ScBuffer with a file descriptor
/// @param  width   Width of data which fd reference
/// @param  height  Height of data which fd reference
/// @param  fd      File descriptor
/// @retval ScBuffer Pointer to sc_buffer allocated or NULL pointer
///
/// This function create ScBuffer with file descriptor. The difference between
/// CreateScBuffer and it is that this function doesn't allocate memory buffer
/// and copy data. It just assign the third parameter to the internal variable
/// of sc_buffer.
ScBuffer *BindScBuffer(int width, int height, int fd);

/// @brief  Create  ScBuffer with a file descriptor
/// @param  width   Width of data which fd reference
/// @param  height  Height of data which fd reference
/// @param  addr32  32 bits address
/// @retval ScBuffer Pointer to sc_buffer allocated or NULL pointer
///
/// This function create ScBuffer with 32bit address. The difference between
/// CreateScBuffer and it is that this function doesn't allocate memory buffer
/// and copy data. It just assign the third parameter to the internal variable
/// of sc_buffer.
ScBuffer *BindScBuffer(int width, int height, unsigned int addr32);

/// @brief  Release ScBuffer
/// @param  buffer  Pointer to ScBuffer
///
/// Release ScBuffer and memory for addr parameter included in ScBuffer
void ReleaseScBuffer(ScBuffer *buffer);

/// @brief  Execute SCore kernel
/// @param  kernel  Enum value of kernel to be executed
/// @param  cmd     Parameter sent to kernel
/// @return 0 if it succeeds, otherwise error
int DoOnScore(unsigned int kernel, ScoreCommand &cmd);

ScoreCore* ScoreInstance(void);
void ReleaseScoreInstance(void);

} //namespace score
#endif

