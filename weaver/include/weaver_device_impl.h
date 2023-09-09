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

#ifndef __WEAVER__DEVICE__IMPL__H__
#define __WEAVER__DEVICE__IMPL__H__

#include <android/hardware/weaver/1.0/IWeaver.h>
#include <vector>
#include "weaver_def.h"
#include "tee_client_api.h"
#include "weaver_tee_uuid.h"
#include "ssp_weaver_tee_api.h"
#include "weaver_throttle_list.h"

using namespace std;

typedef struct ssp_session {
	TEEC_Context teec_context;
	TEEC_Session teec_session;
	TEEC_Operation teec_operation;
} ssp_session_t;

class WeaverDeviceImpl {
public:
	WeaverDeviceImpl();
	~WeaverDeviceImpl();

	weaver_error_t weaverInitIpc(ssp_session_t& sess);

	void getConfig(weaver_status &status, weaver_config &config);

	void read(
		uint32_t slotId,
		vector<uint8_t> key,
		weaver_read_status &status,
		weaver_read_response &readResponse);

	weaver_status write(
		uint32_t slotId,
		vector<uint8_t> key,
		vector<uint8_t> value);

	weaver_error_t isKeyVaild(size_t key_size);
	weaver_error_t isValueVaild(size_t value_size);

	int ssp_open(ssp_session_t& sess);
	int ssp_close(ssp_session_t& sp_sess);
	int ssp_transact(ssp_session_t& sp_sess, ssp_weaver_message_t& ssp_msg);

	int ssp_read_transact(ssp_session_t& ssp_sess,
		ssp_weaver_message_t& ssp_msg,
		uint32_t slotId);

	inline void ssp_set_teec_param_memref(
		ssp_session_t& sess,
		uint32_t param_idx,
		uint8_t* addr,
		size_t size);

private:
	struct ssp_session sess;
	weaver_config config;
	thList tlist;
};
#endif
