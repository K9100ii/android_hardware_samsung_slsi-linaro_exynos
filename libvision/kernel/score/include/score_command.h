//------------------------------------------------------------------------------
/// @file  score_command.h
/// @ingroup  include
///
/// @brief  Declarations of ScoreCommand class
///
/// ScoreCommand class includes functions making ScoreIpcPacket struct that is
/// the command to execute SCore kernel.
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

#ifndef SSF_HOST_INCLUDE_SOCRE_COMMAND_H_
#define SSF_HOST_INCLUDE_SOCRE_COMMAND_H_

#include <utility>    ///< std::pair
#include <vector>
#include <typeinfo>

#include <sc_data_type.h>
#include <logging.h>
#include "scv.h"

namespace score {

typedef unsigned int                                WORD;
typedef unsigned char                               BYTE;
typedef std::vector<WORD>                           RAW;
typedef std::pair<unsigned int, RAW>                PARAM;    ///<
typedef std::vector<PARAM>                          PAYLOAD;  ///< concatenating of PARAM

/// @brief  Number of parameters in each group
#define NUM_OF_GRP_PARAM        (26)
/// @brief  Size of single parameter
#define SCORE_PER_PARAM_SIZE    (sizeof(unsigned int))
/// @brief  Maximum size of parameter to a single kernel
#define SCORE_MAX_PARAM         (26)
/// @brief  Maximum size of parameter to a single kernel in byte
#define SCORE_COMMAND_SIZE      ((SCORE_PER_PARAM_SIZE) * (NUM_OF_GRP_PARAM))

typedef struct _score_packet_size_t {
  unsigned int packet_size: 14;   ///< Packet size of whole packets to send
  unsigned int reserved:    10;   ///< RESERVED
  unsigned int group_count: 8;    ///< Number of group in a packet
} ScorePacketSize, score_packet_size_t;

typedef struct _score_packet_header_t {
  unsigned int queue_id:    2;    ///< Uniqueue ID of message queue
  unsigned int kernel_name: 12;   ///< Name of kernel
  unsigned int task_id:     7;    ///< Uniqueue Id internally assigned to every task
  unsigned int reserved:    8;    ///< RESERVED
  unsigned int worker_name: 3;    ///< DMA option
} ScorePacketHeader, score_packet_header_t;

/// @brief  Packet group parameter struct about raw parameters
typedef struct _ScorePacketGroupData {
  WORD params[NUM_OF_GRP_PARAM];  ///< Raw packet parameters
} ScorePacketGroupData;

/// @brief  Packet group information struct. each group has this
typedef struct _ScorePacketGroupHeader {
  unsigned int valid_size:  6;    ///< Number of valid words in a packet group
  unsigned int fd_bitmap:   26;   ///< Represent the start word of ScBuffer in
  ///  the params array
} ScorePacketGroupHeader;

/// @brief  Packet group struct. Group means a bunch of params and its meta info
typedef struct _ScorePacketGroup {
  ScorePacketGroupHeader  header; ///< Meta information of packet group
  ScorePacketGroupData    data;   ///< Raw payload of packet group
} ScorePacketGroup;

/// @brief  IPC packet struct. it defines whole a packet structure
typedef struct _ScoreIpcPacket {
  ScorePacketSize   size;         ///< Packet header for representing size
  ScorePacketHeader header;       ///< Packet header for representing general info
  ScorePacketGroup  group[0];     ///< Real payload of packet and its meta info
} ScoreIpcPacket;

/// @brief  Packet header size (in byte)
#define PACKET_HEADER_SIZE            sizeof(ScoreIpcPacket)

/// @brief  Packet header size (in word)
#define PACKET_HEADER_SIZE_IN_WORD    (sizeof(ScoreIpcPacket)>>2)

/// @brief  Data information used in SCore
///
/// This struct is located in sc_data_type.h
struct _ScBuffer;
typedef struct _ScBuffer ScBuffer;


/// @brief  The IDs of the external IPs communicating with SCore such as
///         host(ARM) and IVA
enum QueueId {
  INTER_HOST_QUEUE = 0x0,  ///< Host <-> SCore
  INTER_IVA_QUEUE = 0x1    ///< Iva <-> SCore
};

/// @brief  Function pointer type to pre and post processsing
typedef int (*postamble_t)(class ScoreCommand *cmd);
typedef int (*preamble_t)(class ScoreCommand *cmd);

/// @brief  Class for command to execute SCore kernel
class ScoreCommand {
 public:
  /// @brief  Constructor of ScoreCommand class
  ///
  /// This allocates memory for packet_.
  explicit ScoreCommand(sc_kernel_name_e kernel_id);

  /// @brief Copy constructor of ScoreCommand class
  ScoreCommand(const ScoreCommand &rhs);

  /// @brief  Destructor of ScoreCommand class
  ///
  /// This releases memory for packet_.
  virtual ~ScoreCommand();

  /// @brief  Put data to packet_
  /// @param  input_param  Input data
  /// @return STS_SUCCESS if succeeds, otherwise error
  template<typename T> int Put(T *input_param) {
    int ret;
    PARAM p;

    size_t size;
    size = sizeof(*input_param);

    p.first = 0;
    ret = MakeParam(reinterpret_cast<void *>(input_param), p, size);

    if (ret == STS_SUCCESS) {
      payload_.push_back(p);
    } else {
      sc_printf("Error at ScoreCommand::MakeParam %d\n", ret);
    }

    return ret;
  }

  /// @brief  Put packet header to packet_
  /// @param  kernel  Enum value of SCore kernel
  ///
  /// This puts kernel_name and param_sz. task_id of packet header is decided
  /// in the kernel space. So, this puts 0 to task_id.
  void PutKernelInfo(sc_kernel_name_e kernel);

  /// @brief  Get data of packet_
  /// @retval packet_ Data of packet_ in ScoreCommand class
  const ScoreIpcPacket *const GetPacket(void) {
    BuildPacket();
    return packet_;
  }

  /// @brief  Sets a pre processing funciton
  /// @param  preamble pre processing funciton
  void SetPreamble(preamble_t preamble);

  /// @brief  Sets a post processing funciton
  /// @param  preamble post processing funciton
  void SetPostamble(postamble_t postamble);

  /// @brief  Gets a kenel id
  /// @retval kernel_id_ in ScoreCommand class
  sc_kernel_name_e GetKernelID();

  /// @brief  Gets a pre processing funciton
  /// @retval preamble_ in ScoreCommand class
  preamble_t GetPreamble();

  /// @brief  Gets a post processing funciton
  /// @retval postamble_ in ScoreCommand class
  postamble_t GetPostamble();

 private:
  /// @brief  Translate addr value from virtual address to physical address
  /// @param  buffer Sc_buffer including address of buffer
  /// @retval STS_SUCCESS if succeeds, otherwise error.
  int TranslateMemory(ScBuffer &buffer);

  /// @brief  Put data whose type is not ScBuffer to packet_
  /// @param  data  Input data
  /// @param  dest  a structure stores raw param as structured form
  /// @param  size  Size of input data
  /// @retval STS_SUCCESS if secceeds, otherwise error
  int MakeParam(const void *const data, PARAM &dest, const size_t size);

  /// @brief  Make full packet for deliverying by packet header and payload structures
  int BuildPacket();

  /// @brief  IPC command to sent to SCore
  ScoreIpcPacket *packet_;

  /// @brief  IPC packet header information
  ScorePacketHeader packet_header_;

  /// @brief  IPC packet payload contains parameters
  PAYLOAD           payload_;

  /// @brief  Pre processing funciton
  preamble_t preamble_;

  /// @brief Pre processing funciton
  postamble_t postamble_;

  /// @brief  SCore kernel identifier
  sc_kernel_name_e kernel_id_;
};

} //namespace score
#endif
