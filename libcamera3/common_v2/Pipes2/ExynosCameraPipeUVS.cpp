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

/* #define LOG_NDEBUG 0 */
#define LOG_TAG "ExynosCameraPipeUVS"

#include "ExynosCameraPipeUVS.h"

namespace android {

status_t ExynosCameraPipeUVS::create(int32_t *sensorIds)
{
    if (bUVSInit == false) {
        CLOGE("UVS_init() fail");
        return INVALID_OPERATION;
    }

    ExynosCameraSWPipe::create(sensorIds);

    return NO_ERROR;
}

status_t ExynosCameraPipeUVS::m_destroy(void)
{
    status_t ret = NO_ERROR;

    if (bUVSInit == false) {
        return NO_ERROR;
    }

    if (end_uvs !=NULL) {
        ret = (*end_uvs)();

        if (ret < 0) {
                CLOGE("UVS End fail");
        }
        else {
            CLOGD(" UVS End Success!");
        }
        end_uvs = NULL;
        init_uvs = NULL;
        run_uvs = NULL;
    }
    CLOGD("");

    if (uvsHandle !=NULL) {
        CLOGD(" uvsHandle : %08x", uvsHandle);
        dlclose(uvsHandle);
        uvsHandle = NULL;
    }

    CLOGD("");

    ExynosCameraSWPipe::m_destroy();

    return NO_ERROR;
}

status_t ExynosCameraPipeUVS::m_run(void)
{
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraBuffer yuv_UVS_in_Buffer;
    ExynosCameraBuffer yuv_UVS_out_Buffer;
    ExynosRect pictureRect;
    ExynosRect srcRect, dstRect;
    int hwSensorWidth = 0;
    int hwSensorHeight = 0;
    long long durationTime = 0;
    status_t ret = NO_ERROR;

    m_configurations->getSize(CONFIGURATION_PICTURE_SIZE, &pictureRect.w, &pictureRect.h);

    CLOGI("[ExynosCameraPipeUVS thread] waitFrameQ");
    ret = m_inputFrameQ->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        if (ret == TIMED_OUT) {
            CLOGW("wait timeout");

        } else {
            CLOGE("wait and pop fail, ret(%d)", ret);
        }
        return ret;
    }

    if (newFrame == NULL) {
        CLOGE("new frame is NULL");
        CLOGI("[ExynosCameraPipeUVS thread] new frame is NULL");
        return NO_ERROR;
    }

    ret = newFrame->getSrcBuffer(getPipeId(), &yuv_UVS_in_Buffer);
    if (ret < 0) {
        CLOGE("frame get src buffer fail, ret(%d)", ret);
        return OK;
    }

    camera2_shot_ext shot_ext;

    newFrame->getUserDynamicMeta(&shot_ext);

    m_parameters->getSize(HW_INFO_HW_SENSOR_SIZE, &hwSensorWidth, &hwSensorHeight);
    m_parameters->getPictureBayerCropSize(&srcRect, &dstRect);

    m_uvsdynamicMeta.src_y = yuv_UVS_in_Buffer.addr[0];
    m_uvsdynamicMeta.width = pictureRect.w;
    m_uvsdynamicMeta.height = pictureRect.h;

    /* binning_x = (cropped_width * 1024) / capture_width
     * binning_y = (cropped_height * 1024) / capture_height
     */
    m_uvsdynamicMeta.binning_x = (dstRect.w * 1024) / pictureRect.w;
    m_uvsdynamicMeta.binning_y = (dstRect.h * 1024) / pictureRect.h;

    m_uvsdynamicMeta.radial_alpha_R = shot_ext.shot.udm.as.vendorSpecific[0];
    m_uvsdynamicMeta.radial_alpha_G = (shot_ext.shot.udm.as.vendorSpecific[1] + shot_ext.shot.udm.as.vendorSpecific[2])/2;
    m_uvsdynamicMeta.radial_alpha_B = shot_ext.shot.udm.as.vendorSpecific[3];

    CLOGD("============= UVS Dynamic Params===================");
    CLOGD("= width                 : %d", m_uvsdynamicMeta.width);
    CLOGD("= height                : %d", m_uvsdynamicMeta.height);
    CLOGD("= buffersize            : %d", yuv_UVS_in_Buffer.size[0]);
    CLOGD("= BayerCropSize width   : %d", dstRect.w);
    CLOGD("= BayerCropSize height  : %d", dstRect.h);
    CLOGD("= binning_x             : %d", m_uvsdynamicMeta.binning_x);
    CLOGD("= binning_y             : %d", m_uvsdynamicMeta.binning_y);
    CLOGD("= radial_alpha_R        : %d", m_uvsdynamicMeta.radial_alpha_R);
    CLOGD("= radial_alpha_G        : %d", m_uvsdynamicMeta.radial_alpha_G);
    CLOGD("= radial_alpha_B        : %d", m_uvsdynamicMeta.radial_alpha_B);
    CLOGD("===================================================");

    CLOGI(" UVS Processing call");

    if (run_uvs !=NULL) {
        m_timer.start();
        ret = (*run_uvs)(&m_uvsdynamicMeta);
        m_timer.stop();
        durationTime = m_timer.durationMsecs();
        CLOGI("UVS Execution Time : (%5d msec)", (int)durationTime);

    if (ret < 0) {
            CLOGE("UVS run fail, ret(%d)", ret);
            return OK;
        }
    }

    CLOGI(" UVS Processing done");

    ret = newFrame->setEntityState(getPipeId(), ENTITY_STATE_FRAME_DONE);
    if (ret < 0) {
        CLOGE("set entity state fail, ret(%d)", ret);
        return OK;
    }

    m_outputFrameQ->pushProcessQ(&newFrame);

    return NO_ERROR;
}

status_t ExynosCameraPipeUVS::m_init(int32_t *nodeNums)
{
    if (nodeNums == NULL)
        m_uvsNum = -1;
    else
        m_uvsNum = nodeNums[0];

    m_uvs = NULL;

    /*
     * Load the UVSuppression libarry
     * Initialize the UVSuppression library
     */
    bUVSInit = false;
    hUVS_object = NULL;
    uvsHandle = NULL;
    init_uvs = NULL;
    run_uvs = NULL;
    end_uvs = NULL;

    char uvs_lib_path[] = UVS_LIBRARY_PATH;

    int ret = NO_ERROR;

    uvsHandle = dlopen(uvs_lib_path, RTLD_NOW);

    if (uvsHandle == NULL) {
        CLOGE(" UVS so handle is NULL : %s", uvs_lib_path);
        return INVALID_OPERATION;
    }

    init_uvs = (int(*)(UVS_params *))dlsym(uvsHandle, "exn_uvs_init");

    if ((dlerror()!= NULL) && (init_uvs == NULL)) {
        CLOGE("  exn_uvs_init dlsym error");
        goto CLEAN;
    }

    run_uvs = (int(*)(UVS_params *))dlsym(uvsHandle, "exn_uvs_run");

    if ((dlerror()!= NULL) && (run_uvs == NULL)) {
        CLOGE(" exn_uvs_run dlsym error");
        goto CLEAN;
    }

    end_uvs = (int(*)())dlsym(uvsHandle, "exn_uvs_end");

    if ((dlerror()!= NULL) && (end_uvs == NULL)) {
        CLOGE(" exn_uvs_end dlsym error");
        goto CLEAN;
    }

    /*
     * Call the UVSuppression library initialization function.
     *
     */

    ret = (*init_uvs)(&m_uvsdynamicMeta);

    CLOGI(" init_uvs ret : %d", ret);

    return ret;

CLEAN:
    if (uvsHandle != NULL) {
        dlclose(uvsHandle);
    }
    return INVALID_OPERATION;
}

}; /* namespace android */
