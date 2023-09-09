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

#include "ssp_weaver_tee_api.h"

#ifndef __WEAVER__THROTTLE__LIST__H__
#define __WEAVER__THROTTLE__LIST__H__

#define THROTTLE_OFF (false)
#define THROTTLE_ON (true)

static const char* filename = "/mnt/vendor/persist/weaver/throttle_list.dat";

class thList {
private:
	uint8_t list[MAX_SLOT_SIZE];

protected:
	size_t readFile(const char* pPath, uint8_t** ppContent);
	int saveFile(const char* pPath, uint8_t* ppContent, size_t size);

public:
	thList();
	bool isSlotIdThrottle(uint32_t slotId);
	void slotThrottleOn(uint32_t slotId);
	void slotThrottleOff(uint32_t slotId);

	uint8_t* getList();

	int throttleCheck();
	bool saveThList();
};

#endif  // __WEAVER__THROTTLE__LIST__H__
