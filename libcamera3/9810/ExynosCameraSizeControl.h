/*
**
** Copyright 2017, Samsung Electronics Co. LTD
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

#ifndef EXYNOS_CAMERA_SIZE_CONTROL_H
#define EXYNOS_CAMERA_SIZE_CONTROL_H

#include <cutils/properties.h>
#include <utils/threads.h>
#include <utils/String8.h>

#include "ExynosCameraParameters.h"
#include "ExynosCameraConfig.h"
#include "ExynosRect.h"
#include "fimc-is-metadata.h"
#include "ExynosCameraUtils.h"

/* #define DEBUG_PERFRAME_SIZE */

namespace android {

void updateNodeGroupInfo(
        int pipeId,
        ExynosCameraParameters *params,
        camera2_node_group *node_group_info);

/* Helper function */
void setLeaderSizeToNodeGroupInfo(
        camera2_node_group *node_group_info,
        int cropX, int cropY,
        int width, int height);

void setCaptureSizeToNodeGroupInfo(
        camera2_node_group *node_group_info,
        uint32_t perframePosition,
        int width, int height);

void setCaptureCropNScaleSizeToNodeGroupInfo(
        camera2_node_group *node_group_info,
        uint32_t perframePosition,
        int inCropX, int inCropY,
        int inCropWidth, int inCropHeight,
        int outCropX, int outCropY,
        int outCropWidth, int outCropHeight);

}; /* namespace android */

#endif

