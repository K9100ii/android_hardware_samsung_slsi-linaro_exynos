//------------------------------------------------------------------------------
/// @file  logging.h
/// @ingroup  include
///
/// @brief  Declarations of functions and macros about debugging log
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

#ifndef INCLUDE_LOGGING_H_
#define INCLUDE_LOGGING_H_

#include <stdio.h>
#include <unistd.h>
#include <iostream>

namespace score {

// Flag whether debug mode is turned on or not
#define DEBUG_SCORE

/// @brief  Wrapper function of printf for debugging
///
/// If DEBUG_SCORE is defined, this prints the name of function together.
/// Otherwise, this is empty function.
// Not redefining sc_printf in emulator mode
#ifdef sc_printf
#undef sc_printf
#endif
#ifdef DEBUG_SCORE
#define sc_printf(fmt,arg...)  printf("[%s]" fmt,__FUNCTION__,##arg);
#else
#define sc_printf(fmt,arg...)
#endif

/// @brief  Wrapper function of printf for reporting error
///
/// This always prints the name of function together.
#define sc_error(fmt,arg...)  printf("[%s]" fmt,__FUNCTION__,##arg);

} // namespace score
#endif
