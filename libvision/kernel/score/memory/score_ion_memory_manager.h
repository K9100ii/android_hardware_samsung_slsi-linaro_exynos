//------------------------------------------------------------------------------
/// @file  score_ion_memory_manager.h
/// @ingroup  memory
///
/// @brief  Declarations of ScoreIonMemoryManager class
///
/// Ion memory manager is used to get continuous physical memory. Ion heap
/// information for SCore must be registered at ion driver in order to use these
/// functions.
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

#ifndef MEMORY_SCORE_ION_MEMORY_MANAGER_H_
#define MEMORY_SCORE_ION_MEMORY_MANAGER_H_

#include <map>
#include "ion/ion.h"

namespace score
{

/// @brief  Class for ion memory manager for SCore
class ScoreIonMemoryManager
{
public:
  /// @brief  Union of address type
  typedef union _AddressType {
    void *paddr;         ///< Variable for virtual memory address
    unsigned long addr;  ///< Variable for physical memory address
  } AddressType;

  /// @brief  Information of memory allocated by ion for SCore
  typedef struct _ScoreBuffer {
    int fd;            ///< File descriptor for ion memory
    AddressType virt;  ///< Virtual memory address
    AddressType phys;  ///< Physical memory address
    size_t size;       ///< Size of memory
  } ScoreBuffer;

  /// @brief  Type define for SCore buffer memory map
  typedef struct std::map<int, ScoreBuffer *> ScoreBufferMap;
  /// @brief  Type define for iterator of SCore buffer memory map
  typedef ScoreBufferMap::iterator ScoreBufferMapIter;

  /// @brief  Create ScoreIonMemoryManger instance
  /// @retval instance  Address of instance created
  static ScoreIonMemoryManager *Instance()
  {
    static ScoreIonMemoryManager instance;
    return &instance;
  }
  /// @brief  Destructor of ScoreIonMemoryManager class
  ///
  /// This closes ion driver
  virtual ~ScoreIonMemoryManager();
  /// @brief  Alloc continuous memory physically
  /// @param  size     Size of memory to be allocated
  /// @retval fd       Fd of the allocated ion memory
  int AllocScoreMemory(size_t size);
  /// @brief  Get virtual memory address corresponding to fd of ion memory area
  /// @param  fd       fd of ion memory area
  /// @retval address  Virtual memory address if function succeeds,
  ///                  otherwise STS_FAILURE
  unsigned long GetVaddrFromFd(int fd);
  /// @brief  Get physical memory address corresponding to fd of ion memory area
  /// @param  fd       fd of ion memory area
  /// @retval address  Physical memory address if function succeeds,
  ///                  otherwise STS_FAILURE
  unsigned long GetPhysicalMemory(int fd);
  /// @brief  Free memory allocated by ion for SCore
  /// @param  fd       fd of ion memory area
  void FreeScoreMemory(int fd);
private:
  /// @brief  Constructor of ScoreIonMemoryManager class
  ///
  /// This opens ion driver
  ScoreIonMemoryManager();

private:
  /// @brief  File descriptor of ion driver for SCore
  int                 ion_dev_;
  /// @brief  SCore buffer memory map
  ScoreBufferMap      buffer_map_;
  uint32_t            ion_alloc_cnt;
  uint32_t            ion_free_cnt;
};

}; // namespace score

#endif
