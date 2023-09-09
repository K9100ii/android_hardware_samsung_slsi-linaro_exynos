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

#ifndef EXYNOS_CAMERA_BOKEH_INCLUDE_H
#define EXYNOS_CAMERA_BOKEH_INCLUDE_H

using namespace android;

#include <CameraMetadata.h>

#include "ExynosCameraSingleton.h"
#include "ExynosCameraSizeTable.h"
#include "ExynosCameraBuffer.h"
#include "ExynosCameraBufferManager.h"
#include "ExynosCameraUtils.h"
#include "ExynosCameraActivityAutofocus.h"
#include "ExynosCameraParameters.h"

#include "ExynosCameraFusionWrapper.h"
#include "ExynosCameraBokehWrapper.h"

// Live Focus Lib Return Value
#define CONTROL_BOKEH_STATE_ERROR_DISTANCE_TOO_CLOSE        12
#define CONTROL_BOKEH_STATE_ERROR_DISTANCE_TOO_FAR          11
#define CONTROL_BOKEH_STATE_ERROR_INVALID_DEPTH             1
#define CONTROL_BOKEH_STATE_ERROR_LENS_PARTIALLY_COVERED    21
#define CONTROL_BOKEH_STATE_ERROR_LOW_LIGHT_CONDITION       31
#define CONTROL_BOKEH_STATE_SUCCESS                         0
#define CONTROL_BOKEH_STATE_CAPTURE_FAIL                    5

#endif
