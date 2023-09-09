/*
** Copyright 2015, Samsung Electronics Co. LTD
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

#ifndef SEC_CAMERA_AVAILABILITY_TABLE_H
#define SEC_CAMERA_AVAILABILITY_TABLE_H

#include "SecCameraVendorTags.h"

#ifdef SAMSUNG_CONTROL_METERING
static int32_t AVAILABLE_VENDOR_METERING_MODES[] =
{
    (int32_t)SAMSUNG_ANDROID_CONTROL_METERING_MODE_CENTER,
    (int32_t)SAMSUNG_ANDROID_CONTROL_METERING_MODE_SPOT,
    (int32_t)SAMSUNG_ANDROID_CONTROL_METERING_MODE_MATRIX,
    (int32_t)SAMSUNG_ANDROID_CONTROL_METERING_MODE_MANUAL,
};
#endif

#ifdef SAMSUNG_OIS
static int32_t AVAILABLE_VENDOR_OIS_MODES[] =
{
    (int32_t)SAMSUNG_ANDROID_LENS_OPTICAL_STABILIZATION_OPERATION_MODE_PICTURE,
    (int32_t)SAMSUNG_ANDROID_LENS_OPTICAL_STABILIZATION_OPERATION_MODE_VIDEO,
};
#endif

#endif
