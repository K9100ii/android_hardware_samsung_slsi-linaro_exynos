/*
 **
 ** Copyright 2021, The Android Open Source Project
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **     http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 ** LINT_KERNEL_FILE
 */

#include "weaver_throttle_list.h"
#include "weaver_util.h"
#include <string.h>
#include <iostream>

thList::thList()
{
	memset((void*)list, 0x00, sizeof(list));
}

int thList::throttleCheck()
{
	uint8_t* addr = NULL;
	size_t len = 0;
	int count = 0;

	// copy file to TCI SO
	len = readFile(filename, &addr);
	if ((NULL != addr) && (0 < len) && (len <= sizeof(list)))
		memcpy(list, addr, len);
	// else: don't care, just no copy
	if (NULL != addr)
		free(addr);

	for (uint8_t i = 0; i < MAX_SLOT_SIZE; i++) {
		if (list[i]) {
			count++;
		}
	}
	return count;
}

bool thList::saveThList()
{
	if (saveFile(filename, list, sizeof(list)) != 0) {
		LOG_E("saveFile() Error");
		return false;
	}
	return true;
}

bool thList::isSlotIdThrottle(uint32_t slotId)
{
	return list[slotId];
}

void thList::slotThrottleOn(uint32_t slotId)
{
	list[slotId] = THROTTLE_ON;
}

void thList::slotThrottleOff(uint32_t slotId)
{
	list[slotId] = THROTTLE_OFF;
}

uint8_t* thList::getList()
{
	return list;
}

size_t thList::readFile(const char* pPath, uint8_t** ppContent)
{
	FILE* pStream = NULL;
	long    filesize;
	uint8_t* content = NULL;

	/* Open the file */
	pStream = fopen(pPath, "rb");
	if (pStream == NULL) {
		LOG_E("%s:%d Cannot open file: %s\n", __func__, __LINE__, pPath);
		*ppContent = NULL;
		return 0;
	}

	if (fseek(pStream, 0L, SEEK_END) != 0) {
		LOG_E("%s:%d Cannot find end of file: %s\n", __func__, __LINE__, pPath);
		goto error;
	}

	filesize = ftell(pStream);
	if (filesize < 0) {
		LOG_E("%s:%d Cannot get the file size: %s\n", __func__, __LINE__, pPath);
		goto error;
	}

	if (filesize == 0) {
		LOG_E("%s:%d Empty file: %s\n", __func__, __LINE__, pPath);
		goto error;
	}

	/* Set the file pointer at the beginning of the file */
	if (fseek(pStream, 0L, SEEK_SET) != 0) {
		LOG_E("%s:%d Cannot find beginning of file: %s\n", __func__, __LINE__, pPath);
		goto error;
	}

	/* Allocate a buffer for the content */
	content = (uint8_t*)malloc(filesize);
	if (content == NULL) {
		LOG_E("%s:%d Cannot read file: Out of memory\n", __func__, __LINE__);
		goto error;
	}

	/* Read data from the file into the buffer */
	if (fread(content, (size_t)filesize, 1, pStream) != 1) {
		LOG_E("%s:%d Cannot read file: %s\n", __func__, __LINE__, pPath);
		goto error;
	}

	/* Close the file */
	fclose(pStream);
	*ppContent = content;

	/* Return number of bytes read */
	return (size_t)filesize;

error:
	*ppContent = NULL;
	free(content);
	fclose(pStream);
	return 0;
}

int thList::saveFile(const char* pPath, uint8_t* ppContent, size_t size)
{
	FILE* f = fopen(pPath, "w");
	size_t res = 0;
	if (f == NULL) {
		LOG_E("%s:%d Error opening file %s\n", __func__, __LINE__, pPath);
		return 1;
	}

	res = fwrite(ppContent, sizeof(uint8_t), size, f);
	if (res != size)
		LOG_E("%s:%d Data size mismatch when saving file %s\n", __func__, __LINE__, pPath);

	fclose(f);
	return (res != size) ? 1 : 0;
}