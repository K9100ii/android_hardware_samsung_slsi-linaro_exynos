//------------------------------------------------------------------------------
/// @file  score_ion_memory_manager.cc
/// @ingroup  memory
///
/// @brief  Implementations of ScoreIonMemoryManager class
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

#define LOG_TAG "ScoreIonMemoryManager"

#include <iostream>
#include <stdlib.h>
#include <map>
#include <sys/mman.h>
#include <ExynosVisionCommonConfig.h>

#include "score_ion_memory_manager.h"
#include "logging.h"
#include "sc_data_type.h"

namespace score {

/// @brief  Mask of ion heap for SCore
///
/// This must be same as SCore heap number of ion driver
//TODO:It is temporary value for zynq board
#define ION_HEAP_SCORE_MASK  (1<<6)

ScoreIonMemoryManager::ScoreIonMemoryManager()
    : ion_dev_(-1) {
  // Open ion driver
  ion_dev_ = ion_open();
  ion_alloc_cnt = 0;
  ion_free_cnt = 0;
}

ScoreIonMemoryManager::~ScoreIonMemoryManager() {
  if (ion_dev_ != -1) {
    // Close ion drever
    ion_close(ion_dev_);
  }
  VXLOGD("Ion alloc cnt = %d, free cnt = %d", ion_alloc_cnt, ion_free_cnt);
}

int ScoreIonMemoryManager::AllocScoreMemory(size_t size) {
  int ret;

  if (ion_dev_ < 0) {
    sc_error("ion_dev is not init\n");
    return -EBADF;
  }

  ScoreBuffer *buffer = new ScoreBuffer();
  if (buffer != NULL) {
    buffer->size = size;
  } else {
    sc_error("ScoreBuffer is not allocated\n");
    return -ENOMEM;
  }

  ret = ion_alloc_fd(ion_dev_, size, 0, ION_HEAP_SYSTEM_MASK, 0, &buffer->fd);
  if (ret) {
    sc_error("ion_alloc fail : %d\n", ret);
    goto err_buf_free;
  }
  buffer->virt.paddr =
    mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, buffer->fd, 0);
  if (buffer->virt.paddr == MAP_FAILED) {
    sc_error("ion_map error\n");
    ret = -errno;
    goto err_ion_close;
  }

  buffer_map_.insert(std::pair<int, ScoreBuffer *>
                     (buffer->fd, buffer));
  ion_alloc_cnt++;
  return buffer->fd;

err_ion_close:
  //TODO:needs to analyze ion driver
  //ion_close(buffer->fd);
err_buf_free:
  delete buffer;

  return ret;
}

unsigned long ScoreIonMemoryManager::GetVaddrFromFd(int fd) {
  ScoreBufferMapIter iter;
  iter = buffer_map_.find(fd);

  if (iter != buffer_map_.end()) {
    ScoreBuffer *buffer = iter->second;
    return (unsigned long)buffer->virt.paddr;
  }
  return (unsigned long)STS_FAILURE;
}

unsigned long ScoreIonMemoryManager::GetPhysicalMemory(int fd) {
  return (unsigned long)STS_SUCCESS;
}

void ScoreIonMemoryManager::FreeScoreMemory(int fd) {
  ScoreBufferMapIter iter;
  iter = buffer_map_.find(fd);

  if (iter != buffer_map_.end()) {
    ScoreBuffer *buffer = iter->second;

    munmap(buffer->virt.paddr, buffer->size);
    //TODO:needs to analyze ion driver
    //ion_close(buffer->fd);
    buffer_map_.erase(iter);
    delete buffer;
    ion_free_cnt++;
  } else {
    sc_error("Invalid memory error\n");
  }
}

}; // namespace score
