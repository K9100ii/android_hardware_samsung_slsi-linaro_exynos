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
#define LOG_TAG "ExynosCameraPipeSTK_PICTURE"

#include "ExynosCameraPipeSTK_PICTURE.h"

namespace android {

status_t ExynosCameraPipeSTK_PICTURE::create(__unused int32_t *sensorIds)
{
    if (bSTKInit == false) {
        CLOGE("STK_init() fail");
        return INVALID_OPERATION;
    }

    ExynosCameraSWPipe::create(sensorIds);

    return NO_ERROR;
}

status_t ExynosCameraPipeSTK_PICTURE::start(void)
{
    CLOGD("");
    ExynosRect pictureRect;

    if (bSTKInit == false) {
        m_parameters->getPictureSize(&pictureRect.w, &pictureRect.h);

        CLOGD(" PictureSize (%d x %d), scenario(%d)",
                 pictureRect.w, pictureRect.h, STK_SCENARIO_CAPTURE);
        m_stk_handle = (*init_stk)(pictureRect.w, pictureRect.h, STK_SCENARIO_CAPTURE);

        bSTKInit = true;
    }

    return NO_ERROR;
}

status_t ExynosCameraPipeSTK_PICTURE::stop(void)
{
    CLOGD("");
    status_t ret = NO_ERROR;

    ExynosCameraSWPipe::stop();

    if (bSTKInit == false) {
        CLOGD(" STK_PICTURE already deinit");
        return NO_ERROR;
    }

    if (end_stk != NULL) {
        ret = (*end_stk)(m_stk_handle);

        if (ret < 0)
            CLOGE("STK_PICTURE End fail");
        else
            CLOGD(" STK_PICTURE End Success!");

        bSTKInit = false;
    }
    CLOGD("");

    return NO_ERROR;
}

status_t ExynosCameraPipeSTK_PICTURE::m_destroy(void)
{
    status_t ret = NO_ERROR;

    if (bSTKInit == false) {
        return NO_ERROR;
    }

    if (end_stk !=NULL) {
        //ret = (*end_stk)();
        ret = (*end_stk)(m_stk_handle);

        if (ret < 0) {
            CLOGE("STK_PICTURE End fail");
        } else {
            CLOGD(" STK_PICTURE End Success!");
        }
        end_stk = NULL;
        init_stk = NULL;
        run_stk = NULL;
    }
    CLOGD("");

    if (stkHandle !=NULL) {
        CLOGD(" STK_PICTURE Handle : %08x", stkHandle);
        dlclose(stkHandle);
        stkHandle = NULL;
    }

    CLOGD("");

    ExynosCameraSWPipe::m_destroy();

    return NO_ERROR;
}

status_t ExynosCameraPipeSTK_PICTURE::m_run(void)
{
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraBuffer stk_in_Buffer;
    ExynosRect pictureRect;
    ExynosRect srcRect, dstRect;
    int hwSensorWidth = 0;
    int hwSensorHeight = 0;
    long long durationTime = 0;
    status_t ret = NO_ERROR;

    m_parameters->getPictureSize(&pictureRect.w, &pictureRect.h);

    CLOGV("[ExynosCameraPipeSTK_PICTURE thread] waitFrameQ");
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
        return NO_ERROR;
    }

    ret = newFrame->getSrcBuffer(getPipeId(), &stk_in_Buffer);
    if (ret < 0) {
        CLOGE("frame get src buffer fail, ret(%d)", ret);
        return OK;
    }

    newFrame->getUserDynamicMeta(&m_shot_ext);

    m_parameters->getHwSensorSize(&hwSensorWidth, &hwSensorHeight);
    m_parameters->getPictureBayerCropSize(&srcRect, &dstRect);

    m_stkdynamicMeta.src_y = stk_in_Buffer.addr[0];
    m_stkdynamicMeta.width = pictureRect.w;
    m_stkdynamicMeta.height = pictureRect.h;

    /* binning_x = (cropped_width * 1024) / capture_width
     * binning_y = (cropped_height * 1024) / capture_height
     */
    m_stkdynamicMeta.binning_x = (dstRect.w * 1024) / pictureRect.w;
    m_stkdynamicMeta.binning_y = (dstRect.h * 1024) / pictureRect.h;

    m_stkdynamicMeta.radial_alpha_R = m_shot_ext.shot.udm.as.vendorSpecific[0];
    m_stkdynamicMeta.radial_alpha_G = (m_shot_ext.shot.udm.as.vendorSpecific[1] + m_shot_ext.shot.udm.as.vendorSpecific[2])/2;
    m_stkdynamicMeta.radial_alpha_B = m_shot_ext.shot.udm.as.vendorSpecific[3];

    CLOGV("============= STK Dynamic Params===================");
    CLOGV("= width                 : %d", m_stkdynamicMeta.width);
    CLOGV("= height                : %d", m_stkdynamicMeta.height);
    CLOGV("= buffersize            : %d", stk_in_Buffer.size[0]);
    CLOGV("= BayerCropSize width   : %d", dstRect.w);
    CLOGV("= BayerCropSize height  : %d", dstRect.h);
    CLOGV("= binning_x             : %d", m_stkdynamicMeta.binning_x);
    CLOGV("= binning_y             : %d", m_stkdynamicMeta.binning_y);
    CLOGV("= radial_alpha_R        : %d", m_stkdynamicMeta.radial_alpha_R);
    CLOGV("= radial_alpha_G        : %d", m_stkdynamicMeta.radial_alpha_G);
    CLOGV("= radial_alpha_B        : %d", m_stkdynamicMeta.radial_alpha_B);
    CLOGV("===================================================");

    CLOGI(" STK Processing call");

#if 0
    char buff[128];
    snprintf(buff, sizeof(buff), "/data/media/0/CameraHAL_jpeginput_%d.yuv",
             m_shot_ext.shot.dm.request.frameCount);
    ret = dumpToFile(buff,
        stk_in_Buffer.addr[0],
        stk_in_Buffer.size[0]);
    if (ret != true) {
        //mflag_dumped = false;
        CLOGE("couldn't make a raw file");
    }
    else {
          //mflag_dumped = false;
          CLOGI("Raw Bayer dump Success!");
    }
#endif

    int pixelformat = STK_YUYV;
    int nv21Align = 0;

    if(m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_LLS
            || m_parameters->getShotMode() == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_HDR
            || m_parameters->getPictureFormat() == V4L2_PIX_FMT_NV21
            || m_parameters->getShotMode() == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_SELECTIVE_FOCUS) {
        pixelformat = STK_NV21;
        nv21Align = ALIGN_UP(pictureRect.w, CAMERA_16PX_ALIGN) * ALIGN_UP(pictureRect.h, CAMERA_16PX_ALIGN);
    } else {
        pixelformat = STK_YUYV;
    }

    if (run_stk !=NULL) {
        m_timer.start();

        if (pixelformat == STK_NV21)
            m_thread_id = (*run_stk)(m_stk_handle, stk_in_Buffer.addr[0], stk_in_Buffer.addr[0] + nv21Align, pixelformat);
        else
            m_thread_id = (*run_stk)(m_stk_handle, stk_in_Buffer.addr[0], NULL, pixelformat);

        ret = pthread_join(*m_thread_id, NULL);

        m_timer.stop();
        durationTime = m_timer.durationMsecs();
        m_totalCaptureCount++;
        m_totalProcessingTime += durationTime;
        CLOGI("STK PICTURE Execution Time : (%5d msec), Average(%5d msec), Count=%d",
            (int)durationTime, (int)(m_totalProcessingTime/m_totalCaptureCount), m_totalCaptureCount);

        if (ret < 0) {
            CLOGE("STK run fail, ret(%d)", ret);
            return OK;
        }
    }

    CLOGI(" STK Processing done");

    ret = newFrame->setEntityState(getPipeId(), ENTITY_STATE_FRAME_DONE);
    if (ret < 0) {
        CLOGE("set entity state fail, ret(%d)", ret);
        return OK;
    }

    m_outputFrameQ->pushProcessQ(&newFrame);

    return NO_ERROR;
}

status_t ExynosCameraPipeSTK_PICTURE::m_init(int32_t *nodeNums)
{
    if (nodeNums == NULL)
        m_stkNum = -1;
    else
        m_stkNum = nodeNums[0];

    m_stk = NULL;

    /*
     * Load the Stain-Killer libarry
     * Initialize the Stain-Killer library
     */
    bSTKInit = false;
    hSTK_object = NULL;
    stkHandle = NULL;
    init_stk = NULL;
    run_stk = NULL;
    end_stk = NULL;

    memset(&m_shot_ext, 0x00, sizeof(struct camera2_shot_ext));

    m_totalCaptureCount = 0;
    m_totalProcessingTime = 0;

    char stk_lib_path[] = STK_LIBRARY_PATH;

    int ret = NO_ERROR;
    ExynosRect pictureRect;

    stkHandle = dlopen(stk_lib_path, RTLD_NOW);

    if (stkHandle == NULL) {
        CLOGE(" STK so handle is NULL : %s", stk_lib_path);
        return INVALID_OPERATION;
    }

    //init_stk = (int(*)(STK_params *))dlsym(stkHandle, "stain_killer_init");
    init_stk = (void*(*)(int, int, enum stain_killer_scenario))dlsym(stkHandle, "stain_killer_init");

    if ((dlerror()!= NULL) && (init_stk == NULL)) {
        CLOGE("  exn_stk_init dlsym error");
        goto CLEAN;
    }

    run_stk = (pthread_t*(*)(void *, char *, char *, int))dlsym(stkHandle, "stain_killer_run");

    if ((dlerror()!= NULL) && (run_stk == NULL)) {
        CLOGE(" exn_stk_run dlsym error");
        goto CLEAN;
    }

    end_stk = (int(*)(void *))dlsym(stkHandle, "stain_killer_deinit");

    if ((dlerror()!= NULL) && (end_stk == NULL)) {
        CLOGE(" exn_stk_end dlsym error");
        goto CLEAN;
    }

    /*
     * Call the Stain-Killer library initialization function.
     *
     */

    //ret = (*init_stk)(&m_stkdynamicMeta);

    m_parameters->getPictureSize(&pictureRect.w, &pictureRect.h);

    CLOGI("[ExynosCameraPipeSTK_PICTURE] PictureSize (%d x %d)", pictureRect.w, pictureRect.h);

    m_stk_handle = (*init_stk)(pictureRect.w, pictureRect.h, STK_SCENARIO_CAPTURE);

    CLOGI(" init_stk ret : %d", ret);

    return ret;

CLEAN:
    if (stkHandle != NULL) {
        dlclose(stkHandle);
    }
    return INVALID_OPERATION;
}

}; /* namespace android */
