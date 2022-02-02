/*
 * Copyright (C) 2014, Samsung Electronics Co. LTD
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

//#define LOG_NDEBUG 0
#define LOG_TAG "ExynosCameraPlugInConverterArcsoftFusion"

#include "ExynosCameraPlugInConverterArcsoftFusion.h"

namespace android {

/*********************************************/
/*  protected functions                      */
/*********************************************/
status_t ExynosCameraPlugInConverterArcsoftFusion::m_init(void)
{
    strncpy(m_name, "ConverterArcsoftFusion", (PLUGIN_NAME_STR_SIZE - 1));

    return NO_ERROR;
}

void ExynosCameraPlugInConverterArcsoftFusion::getZoomRatioList(float *list, int maxZoom, float maxZoomRatio)
{
    // refer : getZoomRatioList

    int cur = 0;
    int step = maxZoom - 1;

    float zoom_ratio_delta = pow(maxZoomRatio, 1.0f / step);

    for (int i = 0; i <= step; i++) {
        //list[i] = (int)(pow(zoom_ratio_delta, i) * 1000);
        list[i] = (float)pow(zoom_ratio_delta, i);
        ALOGV("INFO(%s):list[%d]:(%f), (%f)", __func__, i, list[i], (float)((float)list[i] / 1000));
    }

    list[maxZoom] = maxZoomRatio;
}

int ExynosCameraPlugInConverterArcsoftFusion::getOtherDualCameraId(int cameraId)
{
    int otherCameraId = -1;

    switch (cameraId) {
    case CAMERA_ID_BACK:
        otherCameraId = CAMERA_ID_BACK_1;
        break;
    case CAMERA_ID_FRONT:
        otherCameraId = CAMERA_ID_FRONT_1;
        break;
    case CAMERA_ID_BACK_1:
        otherCameraId = CAMERA_ID_BACK;
        break;
    case CAMERA_ID_FRONT_1:
        otherCameraId = CAMERA_ID_FRONT;
        break;
    default:
        android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):Invalid cameraId(%d) so, it cannot return proper otherCameraId",
            __FUNCTION__, __LINE__, cameraId);
        break;
    }

    return otherCameraId;
}

int ExynosCameraPlugInConverterArcsoftFusion::frameType2SyncType(frame_type_t frameType)
{
    int syncType = 1;

    switch (frameType) {
    case FRAME_TYPE_PREVIEW:
    case FRAME_TYPE_REPROCESSING:
        syncType = 1;
        break;
    case FRAME_TYPE_PREVIEW_DUAL_MASTER:
    case FRAME_TYPE_PREVIEW_DUAL_SLAVE:
    case FRAME_TYPE_REPROCESSING_DUAL_MASTER:
    case FRAME_TYPE_REPROCESSING_DUAL_SLAVE:
        syncType = 2;
        break;
    case FRAME_TYPE_PREVIEW_SLAVE:
    case FRAME_TYPE_REPROCESSING_SLAVE:
        syncType = 3;
        break;
    default:
        ALOGW("Invalid frameType(%d). so, just set BYPASS", (int)frameType);
        syncType = 1;
        break;
    }

    return syncType;
}

enum DUAL_OPERATION_MODE ExynosCameraPlugInConverterArcsoftFusion::frameType2DualOperationMode(frame_type_t frameType)
{
    enum DUAL_OPERATION_MODE dualOpMode = DUAL_OPERATION_MODE_MASTER;

    switch (frameType) {
    case FRAME_TYPE_PREVIEW:
    case FRAME_TYPE_REPROCESSING:
        dualOpMode = DUAL_OPERATION_MODE_MASTER;
        break;
    case FRAME_TYPE_PREVIEW_DUAL_MASTER:
    case FRAME_TYPE_PREVIEW_DUAL_SLAVE:
    case FRAME_TYPE_REPROCESSING_DUAL_MASTER:
    case FRAME_TYPE_REPROCESSING_DUAL_SLAVE:
        dualOpMode = DUAL_OPERATION_MODE_SYNC;
        break;
    case FRAME_TYPE_PREVIEW_SLAVE:
    case FRAME_TYPE_REPROCESSING_SLAVE:
        dualOpMode = DUAL_OPERATION_MODE_SLAVE;
        break;
    default:
        ALOGW("Invalid frameType(%d). so, just set DUAL_OPERATION_MODE_MASTER", (int)frameType);
        dualOpMode = DUAL_OPERATION_MODE_MASTER;
        break;
    }

    return dualOpMode;
}

void ExynosCameraPlugInConverterArcsoftFusion::getMetaData(ExynosCameraFrameSP_sptr_t frame,
                                                           const struct camera2_shot_ext *metaData[2])
{
    frame_type_t frameType = (frame_type_t)frame->getFrameType();
    enum DUAL_OPERATION_MODE dualOpMode = DUAL_OPERATION_MODE_MASTER;

    dualOpMode = ExynosCameraPlugInConverterArcsoftFusion::frameType2DualOperationMode(frameType);

    /*
     * in case of 1(DUAL_OPERATION_MODE_MASTER), 3(DUAL_OPERATION_MODE_SLAVE),
     *      meta will come from metaData[0].
     *
     * in case of 2(DUAL_OPERATION_MODE_SYNC),
     *      meta will come from metaData[0] and metaData[1]
     *
     * please, refer the below codes.
     *
     *   ExynosCameraFrameFactoryPreview::m_fillNodeGroupInfo {
     *    ...
     *    frame->setMetaData(&shot, output_node_index);
     *    ...
     *   }
     *
     *   ExynosCameraDualFrameSelector::selectFrame() {
     *    ...
     *      } else if (syncType == DUAL_OPERATION_MODE_SYNC && otherSyncType == DUAL_OPERATION_MODE_SYNC &&
     *         (othersyncType != FRAME_TYPE_INTERNAL)) {
     *         output_node_index = OUTPUT_NODE_2;
     *      }
     *    ...
     *   }
     */

    switch (dualOpMode) {
    case DUAL_OPERATION_MODE_MASTER:
    case DUAL_OPERATION_MODE_SYNC:
        metaData[0] = frame->getConstMeta(OUTPUT_NODE_1);
        metaData[1] = frame->getConstMeta(OUTPUT_NODE_2);
        break;
    case DUAL_OPERATION_MODE_SLAVE:
        /*
         * we switch metadata between 0 and 1.
         * after this code, code is not confused of.
         */
        metaData[0] = frame->getConstMeta(OUTPUT_NODE_2);
        metaData[1] = frame->getConstMeta(OUTPUT_NODE_1);
        break;
    default:
        ALOGW("Invalid dualOpMode(%d). so, just set DUAL_OPERATION_MODE_MASTER", (int)dualOpMode);
        metaData[0] = frame->getConstMeta(OUTPUT_NODE_1);
        metaData[1] = frame->getConstMeta(OUTPUT_NODE_2);
        break;
    }
}

}; /* namespace android */
