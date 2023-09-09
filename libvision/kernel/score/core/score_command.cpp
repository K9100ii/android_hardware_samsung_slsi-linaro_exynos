//------------------------------------------------------------------------------
/// @file  score_command.cc
/// @ingroup  core
///
/// @brief  Implementations of ScoreCommand class
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

#include "score_command.h"

#include <stdlib.h>
#include <cstring>
#include "memory/score_ion_memory_manager.h"
#include "sc_data_type.h"
#include "logging.h"
#include "score.h"

namespace score {

extern data_buf_type buf_type_container[];

typedef union _AddressType {
    void *paddr;
    unsigned long addr;
} AddressType;

ScoreCommand::ScoreCommand(sc_kernel_name_e kernel_id)
  : kernel_id_(kernel_id), packet_(NULL), preamble_(NULL), postamble_(NULL) {
}

ScoreCommand::ScoreCommand(const ScoreCommand &rhs) {
  packet_ = NULL;
  kernel_id_ = rhs.kernel_id_;
  preamble_ = rhs.preamble_;
  postamble_ = rhs.postamble_;
  payload_.assign(rhs.payload_.begin(), rhs.payload_.end());
}

ScoreCommand::~ScoreCommand() {
  if (packet_ != NULL)
    delete packet_;
}

int ScoreCommand::TranslateMemory(ScBuffer &buffer) {
  unsigned long addr = 0;

//#ifndef EMULATOR
//  // Get the pyhsical address corresponding the buffer.memory.addr
//  addr = ScoreIonMemoryManager::Instance()->
//         GetPhysicalMemory((void *)buffer.memory.addr);
//  if (addr == (sc_addr)STS_FAILURE) {
//    score_error("Physical Address Translation Fail\n");
//    return STS_FAILURE;
//  }
//
//  score_print("Virt to Phys: %16X -> %08X\n",
//              buffer.memory.addr, (unsigned int)addr);
//  // Put the physical address
//  buffer.memory.addr = reinterpret_cast<sc_addr>(addr);
//#endif

  return STS_SUCCESS;
}

int ScoreCommand::MakeParam(const void *const data,
                            PARAM &dest, const size_t size) {
  WORD *p = static_cast<WORD *>(const_cast<void *>(data));
  ScBuffer *buf = NULL;

  size_t n_aligned_data = size >> 2;

  /// TODO remove here after merged
  /// This logic for the TranslateMemory of ScBuffer
  /// {{
  if (dest.first) {
    buf = new ScBuffer;
    memcpy(buf, data, size);
    TranslateMemory(*buf);
    p = reinterpret_cast<WORD *>(buf);
  }
  /// }}

  for (int i = 0; i < n_aligned_data; i++) {
    dest.second.push_back(p[i]);
  }

  size_t n_remain_data = size - (n_aligned_data << 2);
  if (n_remain_data > 0) {
    dest.second.push_back(0);

    const BYTE *const pbyte =
      reinterpret_cast<const BYTE *const>(p + n_aligned_data);

    for (int i = 0; i < n_remain_data; i++) {
      /// TODO considering endianess
      dest.second[n_aligned_data] |= static_cast<WORD>(*(pbyte + i)) << (i * 8);
    }
  }

  if (buf != NULL)
    delete []buf;

  return STS_SUCCESS;
}

void ScoreCommand::PutKernelInfo(sc_kernel_name_e kernel) {
//  SC_ASSERT(kernel_id_ == kernel, kernel_id and kernel are not equals);

  packet_header_.queue_id = INTER_HOST_QUEUE;
  packet_header_.kernel_name = kernel;
  packet_header_.task_id = 0;
  /// TODO considering 'worker_name'
  packet_header_.worker_name = 0;
}

/// For debugging,
static void dump_raw(RAW &v) {
  RAW::iterator itor;

  for (itor = v.begin(); itor != v.end(); itor++) {
    for (int i = 0; i < 4; i++) {
      printf("%02X ", (unsigned char)(*itor >> (i*8)));
    }
  }
}

inline int sc_cmd_get(void **param, unsigned char *pkt, unsigned int pos, sc_size size)
{
	//(*param) = (void*)(cmd->param + cmd->pos);
	(*param) = (void*)(pkt + pos);
	//cmd->pos += size;
	return 0;
}
#if 0
#define command_get(X, CMD, POS, SIZE) \
	sc_cmd_get((void**)(X), (unsigned char*)(CMD), (POS), (SIZE))

int ScoreCommand::m_dumpPacket(ScoreIpcPacket *packet)
{
    if (packet == NULL)
        packet = packet_;

    BYTE loop = 0;
    BYTE *dumpData = (BYTE *)packet;
    size_t packet_size = sizeof(ScoreIpcPacket) + (sizeof(ScorePacketGroup) * packet->size.group_count);

	printf("[%s(%d)] packet_size(%d) \n", __func__,__LINE__, packet_size);
	printf("[%s(%d)] (%d) (%d) (%d) \n",
	        __func__,__LINE__,
		sizeof(ScoreIpcPacket),
	        sizeof(ScorePacketGroup),
		packet->size.group_count);

	printf("------------------------------------------------ \n");

    for (loop = 0; loop < packet_size; loop ++) {
	if ((loop % 8) == 0)
		printf("\n");
        printf("{%02X }", (unsigned char)(dumpData[loop]));
    }
	printf("\n ------------------------------------------------ \n");

	printf("[%s(%d)] size.size(%d), reserved(%d), group_count(%d)\n",
	        __func__,__LINE__,
		packet->size.packet_size,
	        packet->size.reserved,
		packet->size.group_count);

	printf("[%s(%d)] header.queue_id(%d) kernel_name(%d) task_id(%d), reserved(%d) worker_name(%d) \n",
	        __func__,__LINE__,
		packet->header.queue_id,
	        packet->header.kernel_name,
		packet->header.task_id,
		packet->header.reserved,
	        packet->header.worker_name);

	printf("[%s(%d)] group_header.valid_is(%d) fd_bitmap(%d) \n",
	        __func__,__LINE__,
		packet->group[0].header.valid_size,
	        packet->group[0].header.fd_bitmap);

	for (loop = 0; loop < packet->size.group_count; loop ++) {
		int loopFdBitmap = 0;
		ScorePacketGroupHeader *packetGroupHeader = NULL;
		ScorePacketGroupData *packetGroupData = NULL;

		ScorePacketGroup *packetGroup = &packet->group[loop];
		packetGroupHeader = &packetGroup->header;
		packetGroupData = &packetGroup->data;

	printf("[%s(%d)] valid_size(%d) fd_bitmap(%d) \n",
		__func__,__LINE__,
	        packetGroupHeader->valid_size,
		packetGroupHeader->fd_bitmap);

		for (loopFdBitmap = 0; loopFdBitmap < 26; loopFdBitmap ++) {
			if ((packetGroupHeader->fd_bitmap & (0x1 << loopFdBitmap)) != 0x0) {
				sc_buffer *buffer = NULL;
				printf("[%s(%d)] FdBitmap(%d) \n",
						__func__,__LINE__,
						loopFdBitmap);

				command_get(&buffer, packetGroupData, loopFdBitmap * sizeof(unsigned int), sizeof(sc_buffer));

//				printf("[%s(%d)] buffer fd(%d) size(%d x %d) memory_type(0x%x) \n",
//						__func__,__LINE__,
//						buffer->host_buf.fd,
//						buffer->width,
//						buffer->height,
//						buffer->memory_type);
			} else {
				printf("[%s(%d)] not FdBitmap(%d) \n",
						__func__,__LINE__,
						loopFdBitmap);
			}
		}
	}

    return 0;
}
#endif

int ScoreCommand::BuildPacket() {
  PAYLOAD::iterator itor;
  unsigned char     group_idx = 0;
  unsigned int      acc_param_size = 0;

  /// calculating number of groups needed
  for (itor = payload_.begin(); itor != payload_.end(); itor++) {
    unsigned int param_sz = itor->second.size();
    if ((acc_param_size + param_sz > NUM_OF_GRP_PARAM) || group_idx == 0) {
      acc_param_size = 0;
      group_idx++;
    }
    acc_param_size += param_sz;
  }

  /// allocating ScoreIpcPacket
  if (packet_ != NULL)
    delete packet_;
  size_t packet_size = sizeof(ScoreIpcPacket) + sizeof(ScorePacketGroup)*group_idx;

  packet_ = reinterpret_cast<ScoreIpcPacket *>(new BYTE[packet_size]);
  if (packet_ == NULL)
    return STS_FAILURE;

  /// Initializing PacketHeader
  memcpy(&(packet_->header), &packet_header_, sizeof(ScorePacketHeader));
  packet_->size.group_count = group_idx;

  /// Building payload from raw data
  group_idx = 0;
  acc_param_size = 0;
  ScorePacketGroup *group = NULL;
  for (itor = payload_.begin(); itor != payload_.end(); itor++) {
    unsigned int param_sz = itor->second.size();
    if (acc_param_size + param_sz > NUM_OF_GRP_PARAM || group_idx == 0) {
      if (group != NULL) {
        group->header.valid_size = acc_param_size;
      }

      /// Initializing group header infomation
      acc_param_size = 0;
      group = &packet_->group[group_idx];
      group_idx++;
      group->header.fd_bitmap = 0;
      group->header.valid_size = 0;
    }

    //// If the param is ScBuffer
    if (itor->first != 0) {
      group->header.fd_bitmap |= (1 << acc_param_size);
    }

    std::copy(itor->second.begin(), itor->second.end(), &group->data.params[acc_param_size]);
    acc_param_size += itor->second.size();

#if 0//def  SCORE_DEBUG
    dump_raw(itor->second);
#endif
  }

  /// Put the valid param size into the last group
  if (acc_param_size > 0 && group != NULL)
    group->header.valid_size = acc_param_size;

  /// 2 words for ScorePacketSize + ScorePacketHeader
  /// each ScorePacketGroupHeader consumes 1 word
  /// each group consumes sizeof(ScorePacketGroupData)
  packet_->size.packet_size =
    PACKET_HEADER_SIZE_IN_WORD + group_idx + ((group_idx*sizeof(ScorePacketGroupData)) >> 2);

    unsigned int size = packet_->size.packet_size;
    unsigned int *dump = reinterpret_cast<unsigned int *>(packet_);
    int i;

#if 0
    printf("Packet are (size %d)--------------------------- \n", size);
    for (i = 0; i < size; ++i) {
        printf("[%d][0x%08X] \n", i, dump[i]);
    }
    printf("End --------------------------------- \n");
#endif

  return STS_SUCCESS;
}


template<> int ScoreCommand::Put(ScBuffer *input_param) {
  int ret;
  PARAM p;

  size_t size;
  size = sizeof(*input_param);

  p.first = 1;
  ret = MakeParam(reinterpret_cast<void *>(input_param), p, size);

  if (ret == STS_SUCCESS) {
    payload_.push_back(p);
  } else {
    printf("Error at ScoreCommand::MakeParam %d\n", ret);
  }

  return ret;
}

void ScoreCommand::SetPreamble(preamble_t preamble) {
  preamble_ = preamble;
}

void ScoreCommand::SetPostamble(postamble_t postamble) {
  postamble_ = postamble;
}

sc_kernel_name_e ScoreCommand::GetKernelID() {
  return kernel_id_;
}

preamble_t ScoreCommand::GetPreamble() {
  return preamble_;
}

postamble_t ScoreCommand::GetPostamble() {
  return postamble_;
}

} //namespace score
