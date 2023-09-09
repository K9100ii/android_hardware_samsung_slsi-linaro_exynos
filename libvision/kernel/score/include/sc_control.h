//------------------------------------------------------------------------------
/// @file  control.h
/// @ingroup  include
///
/// @brief  Declarations related to processing control
/// @author  Minwook Ahn<minwook.ahn@samsung.com>
///
/// @section changelog Change Log
/// 2015/04/10 Minwook Ahn created
/// 2015/12/21 Jongkwon Park added
/// 2015/12/21 Sanghwan Park commented
///
/// @section copyright_section Copyright
/// &copy; 2015, Samsung Electronics Co., Ltd.
///
/// @section license_section License
//------------------------------------------------------------------------------

#ifndef __SSDF_DSP_COMMON_INCLUDE_SC_CONTROL_H__
#define __SSDF_DSP_COMMON_INCLUDE_SC_CONTROL_H__

#include <stdlib.h>
#include <sc_errno.h>

/// @brief  Exit status of an execution
/// @ingroup include
///
///
enum sc_status_t {
  STS_SUCCESS           = 0,                ///< success
  STS_FAILURE           = -EFAILURE,        ///< failure
  STS_WRONG_TYPE        = -EWRONGTYPE,      ///< not supported type
  STS_INVALID_VALUE     = -EINVALVALUE,     ///< values specified in flag are invalid
  STS_NULL_PTR          = -ENULLPTR,        ///< input or output image is NULL.
  STS_OUT_OF_RANGE      = -EOUTRANGE,       ///< out of range
  STS_TCM_MALLOC_FAILED = -ETCMMALLOCFAIL,  ///< TCM malloc failed
  STS_FOPEN_FAILED      = -EFOPENFAIL,      ///< fopen failed
};

/// @brief  Check the exit code of X after X is done.
/// @param  X   A function or status flag
/// @retval Exit a function and return the sc_status_t from X
#define CHECK_STATUS(X) \
  do { \
    sc_status_t status = (sc_status_t) (X); \
    if (status != STS_SUCCESS) return status; \
  } while(0)

/// @brief  Check the return condition.
/// @param  COND    Condition to be checked
/// @param  RETVAL  Value to be returned if COND is true
#define CHECK_COND(COND, RETVAL) \
  do { \
    if ((COND)) return (RETVAL); \
  } while(0)

#endif
