//------------------------------------------------------------------------------
/// @file  sc_errno.h
/// @ingroup  include
///
/// @brief  Declarations the error number for SCore
/// @author  Sanghwan Park<senius.park@samsung.com>
///
/// @section changelog Change Log
/// 2016/06/28 Sanghwan Park created
///
/// @section copyright_section Copyright
/// &copy; 2016, Samsung Electronics Co., Ltd.
///
/// @section license_section License
//------------------------------------------------------------------------------

#ifndef __SSF_COMMON_INCLUDE_SC_ERRNO_H__
#define __SSF_COMMON_INCLUDE_SC_ERRNO_H__

#include <errno.h>

/// @brief Define of error for SCore
///
/// Error number is temporary value. This can be changed.
/// {
#define EFAILURE        300  /* Execution failure */
#define EWRONGTYPE      301  /* Not supported type */
#define EINVALVALUE     302  /* Invalid value specified in flag */
#define ENULLPTR        303  /* Unwished NULL pointer */
#define EOUTRANGE       304  /* Out of valid range */
#define ETCMMALLOCFAIL  305  /* TCM malloc failed */
#define EFOPENFAIL      306  /* File open failed */
/// }

#endif
