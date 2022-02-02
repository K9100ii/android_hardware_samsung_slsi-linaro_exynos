//------------------------------------------------------------------------------
/// @file  score_memory.cc
/// @ingroup  memory
///
/// @brief  Implementations of memory manager about ScBuffer
///
/// This file is the implementations of creating and releasing function of
/// memory manager for ScBuffer and internal functions for that.
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

#include <cstring>

#include "score.h"
#include "score_ion_memory_manager.h"
#include "sc_data_type.h"
#include "logging.h"
#include "vs4l.h"

namespace score {

/// @brief  Base value to calculate pixel size
#define SCV_BASE_PIXEL_SIZE 8

//TODO Temp: need to static class type
/// @brief  Get pixel size depending on type
/// @param  type  Type of sc_data
/// @retval size  Size to be allocated depending on type
static float GetPixelSize(data_buf_type type) {
  float size = 0;
  size += (static_cast<float>(type.plane0) / SCV_BASE_PIXEL_SIZE);
  size += (static_cast<float>(type.plane1) / SCV_BASE_PIXEL_SIZE);
  size += (static_cast<float>(type.plane2) / SCV_BASE_PIXEL_SIZE);
  size += (static_cast<float>(type.plane3) / SCV_BASE_PIXEL_SIZE);

  return size;
}

/// @brief  Copy data to addr parameter of ScBuffer when there is input data
/// @param  type       Type of sc_data
/// @param  dest       Pointer to addr of ScBuffer
/// @param  base_size  Width * height of image data
/// @param  addr0~3    Pointer to image data to be processed depending on type
static void CopyToScBuffer(data_buf_type type, char *dest, size_t base_size,
                           void *addr0 = NULL, void *addr1 = NULL,
                           void *addr2 = NULL, void *addr3 = NULL) {
  size_t size;
  size_t offset = 0;

  if (addr0 != NULL) {
    size = (size_t)(static_cast<float>(base_size) *
                    (static_cast<float>(type.plane0) / SCV_BASE_PIXEL_SIZE));
    memcpy(dest, addr0, size);
    offset += size;
  }

  if (addr1 != NULL) {
    size = (size_t)(static_cast<float>(base_size) *
                    (static_cast<float>(type.plane1) / SCV_BASE_PIXEL_SIZE));
    memcpy(&dest[offset], addr1, size);
    offset += size;
  }

  if (addr2 != NULL) {
    size = (size_t)(static_cast<float>(base_size) *
                    (static_cast<float>(type.plane2) / SCV_BASE_PIXEL_SIZE));
    memcpy(&dest[offset], addr2, size);
    offset += size;
  }

  if (addr3 != NULL) {
    size = (size_t)(static_cast<float>(base_size) *
                    (static_cast<float>(type.plane3) / SCV_BASE_PIXEL_SIZE));
    memcpy(&dest[offset], addr3, size);
    offset += size;
  }
}

unsigned long GetScBufferAddr(const ScBuffer *buffer) {
  int memory_type;
  unsigned long buf_addr;

  if (buffer == NULL) {
    buf_addr = (unsigned long)NULL;
  } else {
    memory_type = buffer->host_buf.memory_type;
    if (memory_type == VS4L_MEMORY_USERPTR) {
      buf_addr = buffer->host_buf.addr32;
    } else if (memory_type == VS4L_MEMORY_DMABUF) {
      buf_addr = ScoreIonMemoryManager::Instance()->
                 GetVaddrFromFd(buffer->host_buf.fd);
    } else {
      buf_addr = (unsigned long)NULL;
    }
  }

  return buf_addr;
}

sc_status_t PutScBufferAddr(ScBuffer *buffer, unsigned long addr) {
  sc_status_t ret = STS_SUCCESS;

  if (buffer != NULL) {
    buffer->host_buf.addr32 = addr;
    buffer->host_buf.memory_type = VS4L_MEMORY_USERPTR;
  } else {
    ret = STS_FAILURE;
  }

  return ret;
}

sc_status_t CopyScBufferMemInfo(ScBuffer *dst, const ScBuffer *src) {
  sc_status_t ret = STS_SUCCESS;
  int memory_type;

  if (dst == NULL || src == NULL) {
    ret = STS_FAILURE;
  } else {
    memory_type = src->host_buf.memory_type;
    if (memory_type == VS4L_MEMORY_USERPTR) {
      dst->host_buf.memory_type = memory_type;
      dst->host_buf.addr32 = src->host_buf.addr32;
    } else if (memory_type == VS4L_MEMORY_DMABUF) {
      dst->host_buf.memory_type = memory_type;
      dst->host_buf.fd = src->host_buf.fd;
    } else {
      ret = STS_FAILURE;
    }
  }

  return ret;
}

// TODO:There are several memory type te be used for SCore like
// ion memory already mapped with device virtual address of SMMU,
// ion memory not te be mapped with address of SMMU, or malloc memory or etc.
// So, the api about ScBuffer will be improved.
ScBuffer *CreateScBuffer(int width, int height, data_buf_type type,
                         void *addr0, void *addr1, void *addr2, void *addr3) {
  size_t alloc_size;
  ScBuffer  *buffer = NULL;
  data_buf_type temp_type = type;
  unsigned long buf_addr;

  if (width <= 0 || height <= 0) {
    goto fail;
  }

  buffer = new ScBuffer;
  if (buffer == NULL) {
    goto fail;
  }

  buffer->buf.width   = width;
  buffer->buf.height  = height;
  buffer->buf.type    = type;

  temp_type.sc_type = 0;
  if (*(sc_u32 *)&temp_type == 0)
    goto fail;

  // Total size is different depending type. If type is RGB, size per pixel is
  // 3 bytes. But size per pixel is 4 bytes if type is RGBA.
  alloc_size =
    (size_t)(static_cast<float>(width * height) * GetPixelSize(type));

  buffer->host_buf.fd = ScoreIonMemoryManager::Instance()->
                        AllocScoreMemory(alloc_size);
  if (buffer->host_buf.fd < 0) {
    goto fail;
  }
  buffer->host_buf.memory_type = VS4L_MEMORY_DMABUF;

  // TODO:copy will be improved
  buf_addr = GetScBufferAddr(buffer);
  CopyToScBuffer(type, reinterpret_cast<char *>(buf_addr),
                 (width*height), addr0, addr1, addr2, addr3);
out:
  return buffer;
fail:
  sc_error("CreateScBuffer fail\n");
  if (buffer != NULL) {
    delete buffer;
  }
  return NULL;
}

ScBuffer *BindScBuffer(int width, int height, int fd) {
  ScBuffer *buffer = NULL;

  if (width <= 0 || height <= 0 || fd < 0) {
    goto fail;
  }

  buffer = new ScBuffer;
  if (buffer == NULL) {
    goto fail;
  }

  buffer->buf.width             = width;
  buffer->buf.height            = height;
  buffer->buf.type              = SC_TYPE_U8;
  buffer->host_buf.memory_type  = VS4L_MEMORY_DMABUF;
  buffer->host_buf.fd           = fd;

  return buffer;

fail:
  sc_error("BindScBuffer fail\n");
  if (buffer != NULL) {
    delete buffer;
  }
  return NULL;
}

ScBuffer *BindScBuffer(int width, int height, unsigned int addr32) {
  ScBuffer *buffer = NULL;

  if (width <= 0 || height <= 0) {
    goto fail;
  }

  buffer = new ScBuffer;
  if (buffer == NULL) {
    goto fail;
  }

  buffer->buf.width             = width;
  buffer->buf.height            = height;
  buffer->buf.type              = SC_TYPE_U8;
  buffer->host_buf.memory_type  = VS4L_MEMORY_USERPTR;
  buffer->host_buf.addr32       = addr32;

  return buffer;

fail:
  sc_error("BindScBuffer fail\n");
  if (buffer != NULL) {
    delete buffer;
  }
  return NULL;
}

void ReleaseScBuffer(ScBuffer *buffer) {
  int memory_type;
  unsigned long buf_addr;

  if (buffer == NULL) {
    return;
  } else {
    memory_type = buffer->host_buf.memory_type;
    if (memory_type == VS4L_MEMORY_DMABUF) {
      ScoreIonMemoryManager::Instance()->FreeScoreMemory(buffer->host_buf.fd);
    } else if (memory_type == VS4L_MEMORY_USERPTR) {
      buf_addr = GetScBufferAddr(buffer);
      if (buf_addr != (unsigned long)NULL) {
        ::operator delete((void *)buf_addr);
      }
    }
    delete buffer;
  }
}

} //namespace score
