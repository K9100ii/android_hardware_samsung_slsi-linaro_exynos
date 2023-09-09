/**
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *
 * This software is proprietary of Samsung Electronics.
 * No part of this software, either material or conceptual may be copied
 * or distributed, transmitted, transcribed, stored in a retrieval system
 * or translated into any human or computer language in any form by any means,
 * electronic, mechanical, manual or otherwise, or disclosed to third parties
 * without the express written permission of Samsung Electronics.
 * LINT_KERNEL_FILE
 */

#ifndef __WEAVER__DEF__H__
#define __WEAVER__DEF__H__

#include <stdint.h>

#define MAX_VALUE 32

typedef enum {
	WEAVER_STATUS_OK,
	WEAVER_STATUS_FAILED,
} weaver_status;

typedef struct {
	/** The number of slots available. */
	uint32_t slots;
	/** The number of bytes used for a key. */
	uint32_t keySize;
	/** The number of bytes used for a value. */
	uint32_t valueSize;
} weaver_config;

typedef enum {
	WEAVER_READ_OK,
	WEAVER_READ_FAILED,
	WEAVER_READ_INCORRECT_KEY = 2,
	WEAVER_READ_THROTTLE = 3,
} weaver_read_status;

typedef struct {
	const uint8_t data[MAX_VALUE];
	size_t length;
} weaver_data;

typedef struct {
	/** The time to wait, in milliseconds, before making the next request. */
	uint32_t timeout;
	/** The value read from the slot or empty if the value was not read. */
	weaver_data value;
} weaver_read_response;

typedef enum {
	WEAVER_ERROR_OK = 0,
	WEAVER_ERROR_SECURE_HW_ACCESS_DENIED = 1,
	WEAVER_ERROR_SECURE_HW_COMMUNICATION_FAILED = 2,
	WEAVER_ERROR_INVALID_ARGUMENT = 3,
	WEAVER_ERROR_UNEXPECTED_NULL_POINTER = 4,
	WEAVER_ERROR_INVALID_INPUT_LENGTH = 5,
	WEAVER_ERROR_UNKNOWN_ERROR = 6,
	WEAVER_ERROR_READ_FAIL = 7,
	WEAVER_ERROR_READ_INCORRECT_KEY = 8,
	WEAVER_ERROR_READ_ALREADY_IN_THROTTLE = 9,
	WEAVER_ERROR_READ_THROTTLE = 10,
	WEAVER_ERROR_WRITE_FAIL = 11,
	WEAVER_ERROR_SNVM_COMMUNICATION_FAILED = 12,
	WEAVER_ERROR_TIMER_FAILED = 13,

	/* only HAL */
	WEAVER_ERROR_INVAILD_KEY_SIZE = 14,
	WEAVER_ERROR_INVAILD_VALUE_SIZE = 15,
} weaver_error_t;

#endif
