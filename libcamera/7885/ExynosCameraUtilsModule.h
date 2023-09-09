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

#ifndef EXYNOS_CAMERA_UTILS_MODULE_H
#define EXYNOS_CAMERA_UTILS_MODULE_H

#include <cutils/properties.h>
#include <utils/threads.h>
#include <utils/String8.h>

#include "ExynosRect.h"
#include "fimc-is-metadata.h"
#include "ExynosCameraConfig.h"
#include "ExynosCameraUtils.h"

namespace android {

void updateNodeGroupInfoMain(
        int pipeId,
        camera2_node_group *node_group_info,
        ExynosRect bayerCropSize,
        ExynosRect bdsSize,
        int previewW, int previewH,
        int recordingW, int recordingH);

void updateNodeGroupInfoReprocessing(
        camera2_node_group *node_group_info_1,
        camera2_node_group *node_group_info_2,
        ExynosRect bayerCropSizePreview,
        ExynosRect bayerCropSizePicture,
        ExynosRect bdsSize,
        int pictureW, int pictureH,
        int thumbnailW, int thumbnailH,
        bool pureBayerReprocessing,
        bool flag3aaIspOtf);

class ExynosCameraNodeGroup {
private:
    ExynosCameraNodeGroup();
    virtual ~ExynosCameraNodeGroup();

public:
    /* this is for reprocessing */
    static void updateNodeGroupInfo(
        int cameraId,
        camera2_node_group *node_group_info_1,
        camera2_node_group *node_group_info_2,
        ExynosRect bayerCropSizePreview,
        ExynosRect bayerCropSizePicture,
        ExynosRect bdsSize,
        int pictureW, int pictureH,
        int thumbnailW, int thumbnailH,
        bool pureBayerReprocessing,
        bool flag3aaIspOtf);

    static void updateNodeGroupInfo(
        int cameraId,
        camera2_node_group *node_group_info_1,
        camera2_node_group *node_group_info_2,
        ExynosRect bayerCropSizePreview,
        ExynosRect bayerCropSizePicture,
        ExynosRect bdsSize,
        int pictureW, int pictureH,
        bool pureBayerReprocessing,
        bool flag3aaIspOtf);

protected:
    static void m_dump(const char *name, int cameraId, camera2_node_group *node_group_info);
};

class ExynosCameraNodeGroup3AA : public ExynosCameraNodeGroup {
private:
    ExynosCameraNodeGroup3AA();
    virtual ~ExynosCameraNodeGroup3AA();

public:
    /* this is for preview */
    static void updateNodeGroupInfo(
        int cameraId,
        camera2_node_group *node_group_info,
        ExynosRect bayerCropSize,
        ExynosRect bdsSize,
        int previewW, int previewH,
        int recordingW, int recordingH);
};

class ExynosCameraNodeGroupISP : public ExynosCameraNodeGroup {
private:
    ExynosCameraNodeGroupISP();
    virtual ~ExynosCameraNodeGroupISP();

public:
    /* this is for preview */
    static void updateNodeGroupInfo(
        int cameraId,
        camera2_node_group *node_group_info,
        ExynosRect bayerCropSize,
        ExynosRect bdsSize,
        int previewW, int previewH,
        int recordingW, int recordingH,
        bool tpuEnabled = false);
};

class ExynosCameraNodeGroupDIS : public ExynosCameraNodeGroup {
private:
    ExynosCameraNodeGroupDIS();
    virtual ~ExynosCameraNodeGroupDIS();

public:
    /* this is for preview */
    static void updateNodeGroupInfo(
        int cameraId,
        camera2_node_group *node_group_info,
        ExynosRect bayerCropSize,
        ExynosRect bdsSize,
        int previewW, int previewH,
        int recordingW, int recordingH,
        bool tpuEnabled = false);
};

}; /* namespace android */

#endif

