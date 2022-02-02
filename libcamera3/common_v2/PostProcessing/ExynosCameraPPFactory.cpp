/*
 * Copyright@ Samsung Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

/*#define LOG_NDEBUG 0 */
#define LOG_TAG "ExynosCameraPPFactory"

#include "ExynosCameraPPFactory.h"

ExynosCameraPP *ExynosCameraPPFactory::newPP(
        int cameraId,
        ExynosCameraConfigurations *configurations,
        ExynosCameraParameters *parameters,
        int nodeNum)
{
    status_t ret = NO_ERROR;
    ExynosCameraPP *newPP = NULL;

    int m_cameraId = cameraId;
    char m_name[EXYNOS_CAMERA_NAME_STR_SIZE] = "ExynosCameraPPFactory";

    switch (nodeNum) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
        CLOGD("new ExynosCameraPPLibcsc(cameraId : %d, nodeNum : %d)", cameraId, nodeNum);
        newPP = new ExynosCameraPPLibcsc(cameraId, configurations, parameters, nodeNum);
        break;
    case FIMC_IS_VIDEO_GDC_NUM:
        CLOGD("new ExynosCameraPPGDC(cameraId : %d, nodeNum : %d)", cameraId, nodeNum);
        newPP = new ExynosCameraPPGDC(cameraId, configurations, parameters, nodeNum);
        break;
    default:
        CLOGE("Invalid nodeNum(%d) fail", nodeNum);
        ret = INVALID_OPERATION;
        break;
    }

    return newPP;
}
