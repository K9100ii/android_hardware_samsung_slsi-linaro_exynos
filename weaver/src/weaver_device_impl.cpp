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

#include "weaver_device_impl.h"
#include "weaver_util.h"
#include "weaver_hwctl.h"

#include "cutils/properties.h"
#include <unistd.h>

#undef  LOG_TAG
#define LOG_TAG "Weaver-Impl"

static const TEEC_UUID sUUID = weaver_tee_UUID;

/* ExySp: add timeout to check setting property */
#define SECURE_OS_TIMEOUT 50000

/* ExySp: func to check if secureOS is loaded or not. */
static int secure_os_init(void)
{
	char state[PROPERTY_VALUE_MAX];
	int i;

	for (i = 0; i < SECURE_OS_TIMEOUT; i++) {
		property_get("vendor.sys.mobicoredaemon.enable", state, 0);
		if (!strncmp(state, "true", strlen("true") + 1))
			break;
		else
			usleep(500);
	}

	if (i == SECURE_OS_TIMEOUT) {
		LOG_E("%s: secure os init timed out!", __func__);
		return WEAVER_ERROR_OK;
	} else {
		LOG_I("%s: secure os init is done", __func__);
		return WEAVER_ERROR_OK;
	}
}

static weaver_error_t validate_ipc_req_message(ssp_weaver_message_t* ssp_msg)
{
	weaver_error_t ret = WEAVER_ERROR_OK;

	switch (ssp_msg->command.id) {
		case CMD_WEAVER_INIT_IPC:
			break;
		case CMD_WEAVER_GET_CONFIG:
			break;
		case CMD_WEAVER_READ:
			EXPECT_WEAVER_TRUE_GOTOEND(WEAVER_ERROR_INVALID_ARGUMENT,
					ssp_msg->weaver_data.slot_id <= MAX_SLOT_SIZE);
			EXPECT_WEAVER_TRUE_GOTOEND(WEAVER_ERROR_INVALID_ARGUMENT,
					ssp_msg->weaver_data.key_size <= MAX_KEY_SIZE &&
					ssp_msg->weaver_data.key_size >= MIN_KEY_SIZE);
			break;
		case CMD_WEAVER_WRITE:
			EXPECT_WEAVER_TRUE_GOTOEND(WEAVER_ERROR_INVALID_ARGUMENT,
					ssp_msg->weaver_data.slot_id < MAX_SLOT_SIZE);
			EXPECT_WEAVER_TRUE_GOTOEND(WEAVER_ERROR_INVALID_ARGUMENT,
					ssp_msg->weaver_data.key_size <= MAX_KEY_SIZE &&
					ssp_msg->weaver_data.key_size >= MIN_KEY_SIZE);
			EXPECT_WEAVER_TRUE_GOTOEND(WEAVER_ERROR_INVALID_ARGUMENT,
					ssp_msg->weaver_data.value_size <= MAX_VALUE_SIZE &&
					ssp_msg->weaver_data.value_size >= MIN_VALUE_SIZE);
			break;
		default:
			ret = WEAVER_ERROR_INVALID_ARGUMENT;
	}
end:

	return ret;
}

static weaver_error_t validate_ipc_resp_message(ssp_weaver_message_t* ssp_msg)
{
	weaver_error_t ret = WEAVER_ERROR_OK;

	switch (ssp_msg->command.id) {
		case CMD_WEAVER_INIT_IPC:
			break;
		case CMD_WEAVER_GET_CONFIG:
			break;
		case CMD_WEAVER_READ:
			EXPECT_WEAVER_TRUE_GOTOEND(WEAVER_ERROR_INVALID_ARGUMENT,
					ssp_msg->weaver_data.value_size <= MAX_VALUE_SIZE &&
					ssp_msg->weaver_data.value_size >= MIN_VALUE_SIZE);
			break;
		case CMD_WEAVER_WRITE:
			break;
		default:
			ret = WEAVER_ERROR_INVALID_ARGUMENT;
	}
end:

	return ret;
}

static uint32_t g_ssp_hw_status;

WeaverDeviceImpl::WeaverDeviceImpl()
{
	int ret;
	int numof_throttle_slot = 0;

	LOG_D("%s, %s, %d", __FILE__, __func__, __LINE__);
	LOG_D("build_tag : %s", WEAVER_VERSION);

	ssp_hwctl_check_hw_status(&g_ssp_hw_status);

	/* check HW supporting mode */
	if (g_ssp_hw_status == SSP_HW_NOT_SUPPORTED) {
		LOG_I("SSP HW is not supported\n");
	}
	else {
		/* Check if secureOS is loaded or not */
		if (secure_os_init()) {
			LOG_E("%s: Failed to init secureOS", __func__);
		}
		else {
			LOG_I("secure_os_init is done");
			ret = ssp_open(sess);
			if (ret < 0) {
				LOG_E("ssp_open failed");
			}
			else {
				LOG_I("ssp_open is done");

				ret = weaverInitIpc(sess);
				if (ret != WEAVER_ERROR_OK) {
					LOG_E("weaverInitIpc failed. ret(%d)", ret);
				}
			}
		}
	}
}

WeaverDeviceImpl::~WeaverDeviceImpl()
{
	LOG_I("%s, %s, %d", __FILE__, __func__, __LINE__);
	ssp_close(sess);
}

weaver_error_t WeaverDeviceImpl::weaverInitIpc(
		ssp_session_t& sess)
{
	weaver_error_t ret = WEAVER_ERROR_OK;
	ssp_weaver_message_t ssp_msg;
	int number_of_throttle_slot = 0;

	LOG_I("%s, %s, %d", __FILE__, __func__, __LINE__);

	/* set SSP IPC params */
	memset(&ssp_msg, 0x00, sizeof(ssp_msg));
	ssp_msg.command.id = CMD_WEAVER_INIT_IPC;

	/* get throttle list */
	tlist.throttleCheck();

	std::copy(tlist.getList(),
			tlist.getList() + MAX_SLOT_SIZE, ssp_msg.weaver_data.th_list);

	EXPECT_WEAVER_OK_GOTOEND(validate_ipc_req_message(&ssp_msg));

	/* set extra data types */
	sess.teec_operation.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INOUT,
			TEEC_NONE,
			TEEC_NONE,
			TEEC_NONE);

	/* set command data */
	ssp_set_teec_param_memref(sess, OP_PARAM_0, (uint8_t*)&ssp_msg, sizeof(ssp_msg));

	if (ssp_transact(sess, ssp_msg) < 0) {
		ret = WEAVER_ERROR_SECURE_HW_COMMUNICATION_FAILED;
		goto end;
	}

	/* process reply message */
	CHECK_SSP_WEAVER_MSG_GOTOEND(ssp_msg);
	EXPECT_WEAVER_OK_GOTOEND(validate_ipc_resp_message(&ssp_msg));

	/* check throttle list from f/w */
	std::copy(ssp_msg.weaver_data.th_list,
			ssp_msg.weaver_data.th_list + MAX_SLOT_SIZE, tlist.getList());

	/* save throttle list */
	tlist.saveThList();

	number_of_throttle_slot = tlist.throttleCheck();

	/* It is enabled as many as the number of slots with throttle. */
	for (int i = 0; i < number_of_throttle_slot; i++) {
		ssp_hwctl_enable();
	}

end:

	if (ret != WEAVER_ERROR_OK) {
		LOG_E("%s() exit with %d\n", __func__, ret);
	}
	else {
		LOG_D("%s() exit with %d\n", __func__, ret);
	}
	return ret;
}

void WeaverDeviceImpl::getConfig(weaver_status &status, weaver_config &config)
{
	weaver_error_t ret = WEAVER_ERROR_OK;
	ssp_weaver_message_t ssp_msg;

	LOG_I("%s, %s, %d", __FILE__, __func__, __LINE__);

	if (g_ssp_hw_status == SSP_HW_NOT_SUPPORTED) {
		LOG_E("SSP HW is not supported\n");
		return;
	}

	/* set SSP IPC params */
	memset(&ssp_msg, 0x00, sizeof(ssp_msg));
	ssp_msg.command.id = CMD_WEAVER_GET_CONFIG;

	EXPECT_WEAVER_OK_GOTOEND(validate_ipc_req_message(&ssp_msg));

	/* set extra data types */
	sess.teec_operation.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INOUT,
			TEEC_NONE,
			TEEC_NONE,
			TEEC_NONE);

	/* set command data */
	ssp_set_teec_param_memref(sess, OP_PARAM_0, (uint8_t*)&ssp_msg, sizeof(ssp_msg));

	if (ssp_transact(sess, ssp_msg) < 0) {
		ret = WEAVER_ERROR_SECURE_HW_COMMUNICATION_FAILED;
		goto end;
	}

	/* process reply message */
	CHECK_SSP_WEAVER_MSG_GOTOEND(ssp_msg);
	EXPECT_WEAVER_OK_GOTOEND(validate_ipc_resp_message(&ssp_msg));

	status = static_cast<weaver_status>(ssp_msg.result.status);
	config.keySize = ssp_msg.config.key_size;
	config.slots = ssp_msg.config.slot_size;
	config.valueSize = ssp_msg.config.value_size;

end:
	if (ret != WEAVER_ERROR_OK) {
		LOG_E("%s, %s, num of slots :[%d], key_size :[%d], value_size :[%d]\n",
				__FILE__, __func__, config.slots, config.keySize, config.valueSize);
		this->config = config;
	}
	else {
		LOG_D("%s() exit with %d\n", __func__, ret);
	}
}

void WeaverDeviceImpl::read(
		uint32_t slotId,
		vector<uint8_t> key,
		weaver_read_status &status,
		weaver_read_response &response)
{
	weaver_error_t ret = WEAVER_ERROR_OK;
	ssp_weaver_message_t ssp_msg;

	LOG_I("%s, %s, %d (slot : %u)", __FILE__, __func__, __LINE__, slotId);

	/* key param check */
	CHECK_RESULT(isKeyVaild(key.size()));

	/* set SSP IPC params */
	memset(&ssp_msg, 0x00, sizeof(ssp_msg));
	ssp_msg.command.id = CMD_WEAVER_READ;
	ssp_msg.weaver_data.slot_id = slotId;
	ssp_msg.weaver_data.key_size = key.size();
	std::copy(key.begin(), key.end(), ssp_msg.weaver_data.key);

	EXPECT_WEAVER_OK_GOTOEND(validate_ipc_req_message(&ssp_msg));

	/* set extra data types */
	sess.teec_operation.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INOUT,
			TEEC_NONE,
			TEEC_NONE,
			TEEC_NONE);

	/* set command data */
	ssp_set_teec_param_memref(sess, OP_PARAM_0, (uint8_t*)&ssp_msg, sizeof(ssp_msg));

	if (ssp_read_transact(sess, ssp_msg, slotId) < 0) {
		ret = WEAVER_ERROR_SECURE_HW_COMMUNICATION_FAILED;
		goto end;
	}

	/* process reply message */
	CHECK_SSP_WEAVER_MSG_GOTOEND(ssp_msg);
	EXPECT_WEAVER_OK_GOTOEND(validate_ipc_resp_message(&ssp_msg));

end:
	/* read response copy */
	status = static_cast<weaver_read_status>(ssp_msg.result.read_status);
	response.timeout = 0;

	if (status == WEAVER_READ_THROTTLE) {
		response.timeout = ssp_msg.weaver_data.throttle;
		tlist.slotThrottleOn(slotId);
	} else {
		if (status == WEAVER_READ_OK) {
			response.value.length = ssp_msg.weaver_data.value_size;
			memcpy((uint8_t*)response.value.data,
					(uint8_t*)ssp_msg.weaver_data.value, response.value.length);
		}
		tlist.slotThrottleOff(slotId);
		ssp_hwctl_disable();
	}

	tlist.saveThList();

err:
	if (ret != WEAVER_ERROR_OK) {
		LOG_E("%s() failure_counter %u, exit with %d\n", __func__, ssp_msg.weaver_data.failure_counter, ret);
	}
	else {
		LOG_D("%s() exit with %d\n", __func__, ret);
	}
}

weaver_status WeaverDeviceImpl::write(
		uint32_t slotId,
		vector<uint8_t> key,
		vector<uint8_t> value)
{
	weaver_status status = WEAVER_STATUS_FAILED;
	weaver_error_t ret = WEAVER_ERROR_OK;
	ssp_weaver_message_t ssp_msg;

	LOG_I("%s, %s, %d", __FILE__, __func__, __LINE__);

	/* key & Value param check */
	CHECK_RESULT(isKeyVaild(key.size()));
	CHECK_RESULT(isValueVaild(value.size()));

	/* set SSP IPC params */
	memset(&ssp_msg, 0x00, sizeof(ssp_msg));
	ssp_msg.command.id = CMD_WEAVER_WRITE;
	ssp_msg.weaver_data.slot_id = slotId;
	ssp_msg.weaver_data.key_size = key.size();
	ssp_msg.weaver_data.value_size = value.size();
	std::copy(key.begin(), key.end(), ssp_msg.weaver_data.key);
	std::copy(value.begin(), value.end(), ssp_msg.weaver_data.value);

	EXPECT_WEAVER_OK_GOTOEND(validate_ipc_req_message(&ssp_msg));

	/* set extra data types */
	sess.teec_operation.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INOUT,
			TEEC_NONE,
			TEEC_NONE,
			TEEC_NONE);

	/* set command data */
	ssp_set_teec_param_memref(sess, OP_PARAM_0, (uint8_t*)&ssp_msg, sizeof(ssp_msg));

	if (ssp_transact(sess, ssp_msg) < 0) {
		ret = WEAVER_ERROR_SECURE_HW_COMMUNICATION_FAILED;
		goto end;
	}

	/* process reply message */
	CHECK_SSP_WEAVER_MSG_GOTOEND(ssp_msg);
	EXPECT_WEAVER_OK_GOTOEND(validate_ipc_resp_message(&ssp_msg));

	/* write response copy */
	status = static_cast<weaver_status>(ssp_msg.result.status);
	tlist.slotThrottleOff(slotId);

err:
end:
	if (ret != WEAVER_ERROR_OK) {
		LOG_E("%s() exit with %d\n", __func__, ret);
	}
	else {
		LOG_D("%s() exit with %d\n", __func__, ret);
	}

	return status;
}

int WeaverDeviceImpl::ssp_open(ssp_session_t& sess)
{
	TEEC_Result ret = TEEC_SUCCESS;

	ret = TEEC_InitializeContext(NULL, &sess.teec_context);
	if (ret != TEEC_SUCCESS) {
		LOG_E("Could not initialize context with the TEE. ret=0x%x", ret);
		return -1;
	}

	memset(&sess.teec_session, 0, sizeof(sess.teec_session));
	memset(&sess.teec_operation, 0, sizeof(sess.teec_operation));
	ret = TEEC_OpenSession(
			&sess.teec_context,
			&sess.teec_session,
			&sUUID,
			TEEC_LOGIN_PUBLIC,
			NULL,                   /* connectionData */
			&sess.teec_operation,   /* IN OUT operation */
			NULL                    /* OUT returnOrigin, optional */
			);
	if (ret != TEEC_SUCCESS) {
		LOG_E("Could not open session with Trusted Application. ret=0x%x", ret);
		return -1;
	}

	return 0;
}

int WeaverDeviceImpl::ssp_close(ssp_session_t& sess)
{
	TEEC_CloseSession(&sess.teec_session);
	TEEC_FinalizeContext(&sess.teec_context);
	return 0;
}

int WeaverDeviceImpl::ssp_transact(ssp_session_t& ssp_sess, ssp_weaver_message_t& ssp_msg)
{
	TEEC_Result ret = TEEC_SUCCESS;

	ssp_hwctl_enable();

	ret = TEEC_InvokeCommand(
			&ssp_sess.teec_session,
			ssp_msg.command.id,
			&ssp_sess.teec_operation,
			NULL               /* OUT returnOrigin, optional */
			);
	if (ret != TEEC_SUCCESS) {
		LOG_E("Could not invoke command with Trusted Application. ret=0x%x", ret);
		ssp_hwctl_disable();
		return -1;
	}

	ssp_hwctl_disable();

	return 0;
}

int WeaverDeviceImpl::ssp_read_transact(ssp_session_t& ssp_sess, ssp_weaver_message_t& ssp_msg, uint32_t slotId)
{
	TEEC_Result ret = TEEC_SUCCESS;

	if (tlist.isSlotIdThrottle(slotId) == false)
		ssp_hwctl_enable();

	ret = TEEC_InvokeCommand(
			&ssp_sess.teec_session,
			ssp_msg.command.id,
			&ssp_sess.teec_operation,
			NULL               /* OUT returnOrigin, optional */
			);

	if (ret != TEEC_SUCCESS) {
		LOG_E("Could not invoke command with Trusted Application. ret=0x%x", ret);
		if (!tlist.isSlotIdThrottle(slotId))
			ssp_hwctl_disable();
		return -1;
	}

	return 0;
}
inline void WeaverDeviceImpl::ssp_set_teec_param_memref(
		ssp_session_t& sess,
		uint32_t param_idx,
		uint8_t* addr,
		size_t size)
{
	sess.teec_operation.params[param_idx].tmpref.buffer = addr;
	sess.teec_operation.params[param_idx].tmpref.size = size;
}

weaver_error_t WeaverDeviceImpl::isKeyVaild(size_t key_size)
{
	return ((key_size >= MIN_KEY_SIZE && key_size <= MAX_KEY_SIZE) ?
			(WEAVER_ERROR_OK) : (WEAVER_ERROR_INVAILD_KEY_SIZE)) ;
}
weaver_error_t WeaverDeviceImpl::isValueVaild(size_t value_size)
{
	return ((value_size >= MIN_VALUE_SIZE && value_size <= MAX_VALUE_SIZE) ?
			(WEAVER_ERROR_OK) : (WEAVER_ERROR_INVAILD_VALUE_SIZE));
}
