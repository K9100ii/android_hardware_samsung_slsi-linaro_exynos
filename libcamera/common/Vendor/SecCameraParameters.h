/*
**
** Copyright 2013, Samsung Electronics Co. LTD
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
*/

#ifndef SEC_CAMERA_PARAMETERS_H
#define SEC_CAMERA_PARAMETERS_H

#include "ExynosCameraParameters.h"

namespace android {

enum {
	FOCUS_RESULT_FAIL = 0,
	FOCUS_RESULT_SUCCESS,
	FOCUS_RESULT_CANCEL,
	FOCUS_RESULT_FOCUSING,
	FOCUS_RESULT_RESTART,
};

enum {
	HDR_STATE_READY = 0,
	HDR_STATE_FIRST_TAKE,
	HDR_STATE_SECOND_TAKE,
	HDR_STATE_THIRD_TAKE,
};

enum {
	BURST_SAVE_PHONE = 0,
	BURST_SAVE_SDCARD,
	BURST_SAVE_CALLBACK,
};

enum {
	BURST_TAKE_STOP = 0,
	BURST_TAKE_READY,
	BURST_TAKE_START,
};

enum {
	LLS_LEVEL_ZSL                      = 0,
	LLS_LEVEL_LOW                      = 1,
	LLS_LEVEL_HIGH                     = 2,
	LLS_LEVEL_SIS                      = 3,
	LLS_LEVEL_ZSL_LIKE                 = 4,
	LLS_LEVEL_ZSL_LIKE1                = 7,
	LLS_LEVEL_SHARPEN_SINGLE           = 8,
	LLS_LEVEL_MANUAL_ISO               = 9,
	LLS_LEVEL_FLASH                    = 16,
	LLS_LEVEL_MULTI_MERGE_2            = 18,
	LLS_LEVEL_MULTI_MERGE_3            = 19,
	LLS_LEVEL_MULTI_MERGE_4            = 20,
	LLS_LEVEL_MULTI_PICK_2             = 34,
	LLS_LEVEL_MULTI_PICK_3             = 35,
	LLS_LEVEL_MULTI_PICK_4             = 36,
	LLS_LEVEL_MULTI_MERGE_INDICATOR_2  = 50,
	LLS_LEVEL_MULTI_MERGE_INDICATOR_3  = 51,
	LLS_LEVEL_MULTI_MERGE_INDICATOR_4  = 52,
	LLS_LEVEL_FLASH_2                  = 66,
	LLS_LEVEL_FLASH_3                  = 67,
	LLS_LEVEL_FLASH_4                  = 68,
	LLS_LEVEL_LLS_INDICATOR_ONLY       = 100,
	LLS_LEVEL_DUMMY                    = 150,
};

// cmdType in sendCommand functions
enum {
	CAMERA_CMD_SET_TOUCH_AF_POSITION        = 1103,
	CAMERA_CMD_START_STOP_TOUCH_AF          = 1105,
	CAMERA_CMD_SET_SAMSUNG_APP              = 1108,
	CAMERA_CMD_DISABLE_POSTVIEW             = 1109,
	CAMERA_CMD_AUTO_LOW_LIGHT_SET           = 1351,
	CAMERA_CMD_SET_FLIP                     = 1510,
	HAL_CMD_SET_INTERLEAVED_MODE         = 1515,
	CAMERA_CMD_AE_LOCK                      = 1512,
	CAMERA_CMD_AWB_LOCK                     = 1513,
	CAMERA_CMD_START_SERIES_CAPTURE         = 1516,
	CAMERA_CMD_STOP_SERIES_CAPTURE          = 1517,
	CAMERA_CMD_DEVICE_ORIENTATION           = 1521,
	HAL_CMD_START_FACEZOOM               = 1531,
	HAL_CMD_STOP_FACEZOOM                = 1532,
	HAL_CMD_START_CONTINUOUS_AF          = 1551,
	HAL_CMD_STOP_CONTINUOUS_AF           = 1552,
	CAMERA_CMD_PREPARE_FOR_FACE_DETECTION   = 1561,
	HAL_CMD_RUN_BURST_TAKE               = 1571,
	HAL_CMD_STOP_BURST_TAKE              = 1572,
	HAL_CMD_DELETE_BURST_TAKE            = 1573,
	CAMERA_CMD_START_BURST_PANORAMA		= 1600,
	CAMERA_CMD_STOP_BURST_PANORAMA		= 1601,
	CAMERA_CMD_ADVANCED_MACRO_FOCUS		= 1641,
	CAMERA_CMD_FOCUS_LOCATION			= 1642,

	/* secmsg type in sec_camera_msg_defined.h */
	HAL_AE_AWB_LOCK_UNLOCK = 1501,
	HAL_FACE_DETECT_LOCK_UNLOCK = 1502,
	HAL_OBJECT_POSITION = 1503,
	HAL_OBJECT_TRACKING_STARTSTOP = 1504,
	HAL_TOUCH_AF_STARTSTOP = 1505,
	HAL_STOP_CHK_DATALINE = 1056,
	HAL_SET_DEFAULT_IMEI = 1507,
	HAL_SET_SAMSUNG_CAMERA  = 1508,
	HAL_DISABLE_POSTVIEW_TO_OVERLAY = 1509,
	HAL_SET_FRONT_SENSOR_MIRROR = 1510,
	SET_DISPLAY_ORIENTATION_MIRROR = 1511,
	HAL_CONTROL_FIRMWARE = 1514,
	HAL_SET_INTERLEAVED_MODE = 1515,
};

#define SHOT_MODE_RICH_TONE_COUNT  (3)
#define SHOT_MODE_BEST_PHOTO_COUNT (8)
#define SHOT_MODE_BEST_FACE_COUNT  (5)
#define SHOT_MODE_ERASER_COUNT     (5)
#define SHOT_MODE_PANORAMA_COUNT   (10000)
#define SHOT_MODE_NIGHT_COUNT      (5)

}; // namespace android
#endif /* SEC_CAMERA_PARAMETERS_H */

