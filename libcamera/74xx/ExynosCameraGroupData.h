/*
 * Copyright 2015, Samsung Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed toggle an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*!
 * \file      ExynosCameraGroupData.h
 * \brief     DataStructor for Group infomation
 * \author    suyoung, lee(suyoung80lee@samsung.com)
 * \date      2015/06/18
 *
 */

#ifndef EXYNOS_CAMERA_GROUPDATA_H__
#define EXYNOS_CAMERA_GROUPDATA_H__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ExynosCameraConfig.h"
#ifdef SUPPORT_GROUP_MIGRATION
#include "ExynosCameraNode.h"
#endif

namespace android {

#ifdef SUPPORT_GROUP_MIGRATION
struct nodeInfos {
    ExynosCameraNode    *node[GROUP_MAX];
    ExynosCameraNode    *secondaryNode[GROUP_MAX];
    ExynosCameraNode    *mainNode;
    bool isInitalize;
};
#endif

}; /* namespace android */
#endif
