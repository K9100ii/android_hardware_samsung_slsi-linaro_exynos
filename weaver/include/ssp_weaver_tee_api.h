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

#ifndef __STRONGBOX_WEAVER_API_H__
#define __STRONGBOX_WEAVER_API_H__

#include <stdint.h>

#define WEAVER_VERSION				("weaver-1.0.0-210420")

/* define Operation param index */
#define OP_PARAM_0				(0)
#define OP_PARAM_1				(1)
#define OP_PARAM_2				(2)
#define OP_PARAM_3				(3)

/* SSP IPC operation */
#define CMD_WEAVER_INIT_IPC			(0x1)
#define CMD_WEAVER_GET_CONFIG			(0x2)
#define CMD_WEAVER_READ				(0x3)
#define CMD_WEAVER_WRITE			(0x4)

/* define weaver data size */
#define MAX_SLOT_SIZE				(16)
#define MAX_KEY_SIZE				(32)
#define MAX_VALUE_SIZE				(32)
#define MIN_KEY_SIZE				(16)
#define MIN_VALUE_SIZE				(16)

typedef struct __attribute__((packed, aligned(4))) {
	uint32_t magic;
	uint32_t version;
	uint8_t data[40];			/* reserved */
} header_t;

/**
 * Command message.
 */
typedef struct __attribute__((packed, aligned(4))) {
	uint32_t id;
	uint8_t data[28];			/* reserved */
} command_t;

/**
 * Response structure
 */
typedef struct __attribute__((packed, aligned(4))) {
	uint32_t weaver_errcode;
	uint32_t cm_errno;
	uint8_t data[40];			/* reserved */
} response_t;

typedef struct __attribute__((packed, aligned(4))) {
	uint32_t slot_size;
	uint32_t key_size;
	uint32_t value_size;
	uint8_t reserved[4];
} config_t;

typedef struct __attribute__((packed, aligned(4))) {
	uint8_t slot_id;			/* in */
	uint8_t key[MAX_KEY_SIZE];		/* in */
	uint8_t key_size;			/* in */
	uint8_t value[MAX_VALUE_SIZE];		/* in & out */
	uint8_t value_size;			/* in */
	uint32_t failure_counter;		/* out */
	uint32_t throttle;			/* out */
	uint8_t th_list[MAX_SLOT_SIZE];
	uint8_t data[37];
} weaver_data_t;

typedef struct __attribute__((packed, aligned(4))) {
	uint8_t read_status;			/* Read result */
	uint8_t status;				/* Write, Config result */
	uint8_t data[14];
} result_t;

typedef struct __attribute__((packed, aligned(4))) {
	header_t header;
	command_t command;
	response_t response;
	config_t config;
	weaver_data_t weaver_data;
	result_t result;
} ssp_weaver_message_t;

#endif  // __STRONGBOX_WEAVER_API_H__
