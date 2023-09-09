/*
**
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

/* #define LOG_NDEBUG 0 */
#define LOG_TAG "ExynosCameraPipeSTK_PREVIEW"
#include <cutils/log.h>

#include "ExynosCameraPipeSTK_PREVIEW.h"

namespace android {

ExynosCameraPipeSTK_PREVIEW::~ExynosCameraPipeSTK_PREVIEW()
{
    this->destroy();
}

status_t ExynosCameraPipeSTK_PREVIEW::create(__unused int32_t *sensorIds)
{
    if (bSTKInit == false) {
        CLOGE("STK_PREVIEW_init() fail");
        return INVALID_OPERATION;
    }

    ExynosCameraSWPipe::create(sensorIds);

    return NO_ERROR;
}

status_t ExynosCameraPipeSTK_PREVIEW::destroy(void)
{

    int ret = 0;

    if (bSTKInit == false) {
        return NO_ERROR;
    }

    if (end_stk !=NULL) {
        //ret = (*end_stk)();
        ret = (*end_stk)(m_stk_handle);

        if (ret < 0) {
            CLOGE("STK_PREVIEW End fail");
        } else {
            CLOGD(" STK_PREVIEW End Success!");
        }
        end_stk = NULL;
        init_stk = NULL;
        run_stk = NULL;
    }
    CLOGD("");

    if (stkHandle !=NULL) {
        CLOGD(" STK_PREVIEW Handle : %08x", stkHandle);
        dlclose(stkHandle);
        stkHandle = NULL;
    }

    CLOGD("");

    ExynosCameraSWPipe::release();

    return NO_ERROR;
}

status_t ExynosCameraPipeSTK_PREVIEW::start(void)
{
    ExynosRect previewRect;

    CLOGV("");

    if (bSTKInit == false) {
        m_parameters->getHwPreviewSize(&previewRect.w, &previewRect.h);

        CLOGI(" PreviewSize (%d x %d), scenario(%d)",
                 previewRect.w, previewRect.h, STK_SCENARIO_PREVIEW);
        m_stk_handle = (*init_stk)(previewRect.w, previewRect.h, STK_SCENARIO_PREVIEW);
        bSTKInit = true;
    }

    return NO_ERROR;
}

status_t ExynosCameraPipeSTK_PREVIEW::stop(void)
{
    CLOGV("");
    int ret = 0;

    m_mainThread->requestExitAndWait();

    CLOGD(" thead exited");

    m_inputFrameQ->release();

    if (bSTKInit == false) {
        return NO_ERROR;
    }

    if (end_stk != NULL) {
        //ret = (*end_stk)();
        ret = (*end_stk)(m_stk_handle);

        if (ret < 0)
            CLOGE("STK_PREVIEW End fail");
        else
            CLOGD(" STK_PREVIEW End Success!");

        bSTKInit = false;
    }
    CLOGD("");

    return NO_ERROR;
}

status_t ExynosCameraPipeSTK_PREVIEW::m_run(void)
{
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraBuffer nv21_STK_in_Buffer;
    ExynosRect previewRect;
    ExynosRect srcRect, dstRect;
    int hwSensorWidth = 0;
    int hwSensorHeight = 0;
    long long durationTime = 0;
    int ret = 0;

    m_parameters->getHwPreviewSize(&previewRect.w, &previewRect.h);

    CLOGV("[ExynosCameraPipeSTK_PREVIEW thread] waitFrameQ");
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

    ret = newFrame->getSrcBuffer(getPipeId(), &nv21_STK_in_Buffer);
    if (ret < 0) {
        CLOGE("frame get src buffer fail, ret(%d)", ret);
        return OK;
    }

#if 0
    camera2_shot_ext shot_ext;
    // I could not check still that 'shot_ext.shot.dm.request.frameCount' is updated.
    newFrame->getUserDynamicMeta(&shot_ext);

    char buff[128];
    snprintf(buff, sizeof(buff), "/data/stk/CameraHAL_jpeginput_%d.nv1",
             shot_ext.shot.dm.request.frameCount);
    ret = dumpToFile2plane(buff,
        nv21_STK_in_Buffer.addr[0],
        nv21_STK_in_Buffer.addr[1],
        nv21_STK_in_Buffer.size[0],
        nv21_STK_in_Buffer.size[1]);
    if (ret != true) {
        //mflag_dumped = false;
        CLOGE("couldn't make a raw file");
    }
    else {
          //mflag_dumped = false;
          CLOGI("Raw Bayer dump Success!");
    }
#endif

    int pixelformat = STK_NV21_M;
    int stkPreviewQ;
    int seriesShotMode;
    int availableBufferCountLimit = 4;

    stkPreviewQ = m_inputFrameQ->getSizeOfProcessQ();
    seriesShotMode = m_parameters->getSeriesShotMode();

    if (run_stk != NULL) {
        if (stkPreviewQ <= availableBufferCountLimit) {
            CLOGI("Start STK_Preview frameCount(%d), stkPreviewQ(%d), SeriesShotMode(%d) nv21_STK_in_Buffer(%d)",
                newFrame->getFrameCount(), stkPreviewQ, seriesShotMode, nv21_STK_in_Buffer.index);

            m_timer.start();

            m_thread_id = (*run_stk)(m_stk_handle, nv21_STK_in_Buffer.addr[0], nv21_STK_in_Buffer.addr[1], pixelformat);

            ret = pthread_join(*m_thread_id, NULL);

            m_timer.stop();
            durationTime = m_timer.durationMsecs();
            CLOGI("STK Preview Execution Time : (%5d msec)", (int)durationTime);

            if (ret < 0) {
                CLOGE("STK run fail, ret(%d)", ret);
                return ret;
            }
        } else {
            CLOGW("Skip STK_Preview frameCount(%d), stkPreviewQ(%d), SeriesShotMode(%d)",
                 newFrame->getFrameCount(), stkPreviewQ, seriesShotMode);
        }
    }

    CLOGV(" STK Processing done");

    ret = newFrame->setEntityState(getPipeId(), ENTITY_STATE_FRAME_DONE);
    if (ret < 0) {
        CLOGE("set entity state fail, ret(%d)", ret);
        return ret;
    }

    ret = newFrame->setDstBufferState(getPipeId(), ENTITY_BUFFER_STATE_COMPLETE);
    if (ret != NO_ERROR) {
        CLOGE("setdst Buffer failed(%d) frame(%d)", ret, newFrame->getFrameCount());
        return ret;
    }

    m_outputFrameQ->pushProcessQ(&newFrame);

    return NO_ERROR;
}

status_t ExynosCameraPipeSTK_PREVIEW::m_init(int32_t *nodeNums)
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

    char stk_lib_path[] = STK_PREVIEW_LIBRARY_PATH;

    int ret = NO_ERROR;
    ExynosRect previewRect;

    stkHandle = dlopen(stk_lib_path, RTLD_NOW);

    if (stkHandle == NULL) {
        CLOGE(" STK so handle is NULL : %s", stk_lib_path);
        return INVALID_OPERATION;
    }

    //init_stk = (int(*)(STK_params *))dlsym(stkHandle, "stain_killer_init");
    init_stk = (void*(*)(int, int, enum stain_killer_scenario))dlsym(stkHandle, "stain_killer_init");

    if ((dlerror()!= NULL) && (init_stk == NULL)) {
        CLOGE(" exn_stk_init dlsym error");
        goto CLEAN;
    }

    run_stk = (pthread_t*(*)(void *, char *, char*, int))dlsym(stkHandle, "stain_killer_run");

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

    m_parameters->getHwPreviewSize(&previewRect.w, &previewRect.h);
    CLOGI("PIPE_STK_PREVIEW hwPreviewSize (%d x %d)", previewRect.w, previewRect.h);

    m_stk_handle = (*init_stk)(previewRect.w, previewRect.h, STK_SCENARIO_PREVIEW);
    CLOGV(" init_stk ret : %d", ret);

    return ret;

CLEAN:
    if (stkHandle != NULL) {
        dlclose(stkHandle);
    }
    return INVALID_OPERATION;
}

}; /* namespace android */
