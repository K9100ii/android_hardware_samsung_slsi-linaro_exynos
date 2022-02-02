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

#define LOG_TAG "ArcsoftFusion"

#include <fcntl.h>
#include <dlfcn.h>
#include "ArcsoftFusion.h"



MByte ArcsoftFusion::m_calData[ARC_DCOZ_CALIBRATIONDATA_LEN] = { 0 };
bool  ArcsoftFusion::m_flagCalDataLoaded = false;

ArcsoftFusion::ArcsoftFusion()
{
    m_ionClient = 0;
    ALOGD("DEBUG(%s[%d]):new ArcsoftFusion object allocated", __FUNCTION__, __LINE__);
}

ArcsoftFusion::~ArcsoftFusion()
{
    ALOGD("DEBUG(%s[%d]):new ArcsoftFusion object deallocated", __FUNCTION__, __LINE__);
}

status_t ArcsoftFusion::create(void)
{
    status_t ret = NO_ERROR;

    ret = ArcsoftFusion::m_create();
    if (ret != NO_ERROR) {
        ALOGE("ERR(%s[%d]):ArcsoftFusion::m_create() fail", __FUNCTION__, __LINE__);
        return ret;
    }

#ifdef ARCSOFT_FUSION_EMULATOR
#else
    ret = this->m_create();
    if (ret != NO_ERROR) {
        ALOGE("ERR(%s[%d]):this->m_create() fail", __FUNCTION__, __LINE__);
        return ret;
    }
#endif

    ALOGD("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    return ret;
}

status_t ArcsoftFusion::destroy(void)
{
    status_t ret = NO_ERROR;

    ret = this->m_destroy();
    if (ret != NO_ERROR) {
        ALOGE("ERR(%s[%d]):this->m_destroy() fail", __FUNCTION__, __LINE__);
        return ret;
    }

#ifdef ARCSOFT_FUSION_EMULATOR
#else
    ret = ArcsoftFusion::m_destroy();
    if (ret != NO_ERROR) {
        ALOGE("ERR(%s[%d]):ArcsoftFusion::m_destroy() fail", __FUNCTION__, __LINE__);
        return ret;
    }
#endif

    ALOGD("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ArcsoftFusion::execute(Map_t *map)
{
    status_t ret = NO_ERROR;

    m_checkExcuteStartTime();

    ret = ArcsoftFusion::m_execute(map);
    if (ret != NO_ERROR) {
        ALOGE("ERR(%s[%d]):ArcsoftFusion::m_excute() fail", __FUNCTION__, __LINE__);
        goto done;
    }

    ret = this->m_execute(map);
    if (ret != NO_ERROR) {
        ALOGE("ERR(%s[%d]):this->m_excute() fail", __FUNCTION__, __LINE__);
        goto done;
    }

#ifdef ARCSOFT_FUSION_EMULATOR
    m_copyHalfHalf2Dst(map);

    {
        float zoomRatio = m_mapSrcZoomRatio[0];

        /*
        // refer ExynosCameraConfig.h
        #define DUAL_PREVIEW_SYNC_MODE_MIN_ZOOM_RATIO 1.5f
        #define DUAL_PREVIEW_SYNC_MODE_MAX_ZOOM_RATIO 4.0f
        #define DUAL_CAPTURE_SYNC_MODE_MIN_ZOOM_RATIO 1.5f
        #define DUAL_CAPTURE_SYNC_MODE_MAX_ZOOM_RATIO 2.0f
        #define DUAL_SWITCHING_SYNC_MODE_MIN_ZOOM_RATIO 1.5f
        #define DUAL_SWITCHING_SYNC_MODE_MAX_ZOOM_RATIO 2.0f
        */
        if (zoomRatio < 1.5f) {
            /* DUAL_OPERATION_MODE_MASTER */
            m_cameraIndex = 0x2;
            m_needSwitchCamera = 0;
            m_masterCameraArc2HAL = 0x30;
        } else if(zoomRatio > 4.0f) {
            /* DUAL_OPERATION_MODE_SLAVE */
            m_cameraIndex = 0x3;
            m_needSwitchCamera = 1;
            m_masterCameraArc2HAL = 0x10;
        } else {
            /* DUAL_OPERATION_MODE_SYNC */
            m_cameraIndex = 0x2;
            m_needSwitchCamera = 2;
            m_masterCameraArc2HAL = 0x30;
        }
    }

    m_saveValue(map);
#endif

done:
    m_checkExcuteStopTime();

    return ret;
}

status_t ArcsoftFusion::init(Map_t *map)
{
    status_t ret = NO_ERROR;

    ret = ArcsoftFusion::m_init(map);
    if (ret != NO_ERROR) {
        ALOGE("ERR(%s[%d]):ArcsoftFusion::m_init() fail", __FUNCTION__, __LINE__);
        return ret;
    }

    ret = this->m_init(map);
    if (ret != NO_ERROR) {
        ALOGE("ERR(%s[%d]):this->m_init() fail", __FUNCTION__, __LINE__);
        return ret;
    }

    ALOGD("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    return ret;
}

status_t ArcsoftFusion::uninit(Map_t *map)
{
    status_t ret = NO_ERROR;

    ret = ArcsoftFusion::m_uninit(map);
    if (ret != NO_ERROR) {
        ALOGE("ERR(%s[%d]):ArcsoftFusion::m_uninit() fail", __FUNCTION__, __LINE__);
        return ret;
    }

    ret = this->m_uninit(map);
    if (ret != NO_ERROR) {
        ALOGE("ERR(%s[%d]):this->m_uninit() fail", __FUNCTION__, __LINE__);
        return ret;
    }

    ALOGD("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    return ret;
}

status_t ArcsoftFusion::query(Map_t *map)
{
    status_t ret = NO_ERROR;

    ret = ArcsoftFusion::m_query(map);
    if (ret != NO_ERROR) {
        ALOGE("ERR(%s[%d]):ArcsoftFusion::m_query() fail", __FUNCTION__, __LINE__);
        return ret;
    }

#ifdef ARCSOFT_FUSION_EMULATOR
#else
    ret = this->m_query(map);
    if (ret != NO_ERROR) {
        ALOGE("ERR(%s[%d]):this->m_query() fail", __FUNCTION__, __LINE__);
        return ret;
    }
#endif

    ALOGD("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    return ret;
}

status_t ArcsoftFusion::m_create(void)
{
    status_t ret = NO_ERROR;

    m_emulationProcessTime = FUSION_PROCESSTIME_STANDARD;
    m_emulationCopyRatio = 1.0f;

    m_excuteStartTime = 0L;
    m_excuteStopTime = 0L;

    m_emulationProcessTime = 0;
    m_emulationCopyRatio   = 1.0f;

    m_syncType = 1;

    // src
    m_mapSrcBufCnt = 0;

    m_wideFullsizeW = 0;
    m_wideFullsizeH = 0;
    m_teleFullsizeW = 0;
    m_teleFullsizeH = 0;

    // dst
    m_mapDstBufCnt = 0;

    m_bokehIntensity = 0;

    m_loadCalData();

    for (int i = 0; i < 2; i++) {
        m_tempSrcBufferSize[i] = 0;

        m_tempSrcBufferFd[i] = 0;
        m_tempSrcBufferAddr[i] = NULL;

        m_zoomRatioList[i] = NULL;
    }

    m_ionClient = 0;

    m_csc = NULL;
    m_acylic = NULL;
    m_layer = NULL;

    m_cameraIndex = 0x2;
    m_needSwitchCamera = 0;
    m_masterCameraArc2HAL = 0;
    m_viewRatio = 1.0f;

    m_imageShiftX = 0;
    m_imageShiftY = 0;

    m_zoomMaxSize = 0;

    ALOGD("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    return ret;
}

status_t ArcsoftFusion::m_destroy(void)
{
    status_t ret = NO_ERROR;

    m_destroyIon();
    m_destroyScaler();

    ALOGD("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    return ret;
}

status_t ArcsoftFusion::m_execute(Map_t *map)
{
    status_t ret = NO_ERROR;

    /* from map */
    m_syncType          = (Data_int32_t)     (*map)[PLUGIN_SYNC_TYPE];

    // wide : 0 / tele : 1
    m_mapSrcBufCnt      = (Data_int32_t)     (*map)[PLUGIN_SRC_BUF_CNT];
    m_mapSrcBufPlaneCnt = (Array_buf_t)      (*map)[PLUGIN_SRC_BUF_PLANE_CNT];
    m_mapSrcBufSize     = (Array_buf_plane_t)(*map)[PLUGIN_SRC_BUF_SIZE];
    m_mapSrcRect        = (Array_buf_rect_t) (*map)[PLUGIN_SRC_BUF_RECT];
    m_mapSrcV4L2Format  = (Array_buf_t)      (*map)[PLUGIN_SRC_BUF_V4L2_FORMAT];
    m_mapSrcBufAddr[0]  = (Array_buf_addr_t) (*map)[PLUGIN_SRC_BUF_1]; //master
    m_mapSrcBufAddr[1]  = (Array_buf_addr_t) (*map)[PLUGIN_SRC_BUF_2]; //slave
    m_mapSrcBufFd       = (Array_buf_plane_t)(*map)[PLUGIN_SRC_BUF_FD];
    m_mapSrcBufWStride  = (Array_buf_t)      (*map)[PLUGIN_SRC_BUF_WSTRIDE];

    m_mapSrcAfStatus    = (Array_buf_t)      (*map)[PLUGIN_AF_STATUS];
    m_mapSrcZoomRatio   = (Pointer_float_t)  (*map)[PLUGIN_ZOOM_RATIO];
    m_mapSrcZoomRatioListSize = (Array_buf_t)(*map)[PLUGIN_ZOOM_RATIO_LIST_SIZE];
    m_mapSrcZoomRatioList = (Array_float_addr_t)(*map)[PLUGIN_ZOOM_RATIO_LIST];

    m_wideFullsizeW     = (Data_int32_t)     (*map)[PLUGIN_WIDE_FULLSIZE_W];
    m_wideFullsizeH     = (Data_int32_t)     (*map)[PLUGIN_WIDE_FULLSIZE_H];
    m_teleFullsizeW     = (Data_int32_t)     (*map)[PLUGIN_TELE_FULLSIZE_W];
    m_teleFullsizeH     = (Data_int32_t)     (*map)[PLUGIN_TELE_FULLSIZE_H];

    m_mapDstBufCnt      = (Data_int32_t)     (*map)[PLUGIN_DST_BUF_CNT];
    m_mapDstBufPlaneCnt = (Array_buf_t)      (*map)[PLUGIN_DST_BUF_PLANE_CNT];
    m_mapDstBufSize     = (Array_buf_plane_t)(*map)[PLUGIN_DST_BUF_SIZE];
    m_mapDstRect        = (Array_buf_rect_t) (*map)[PLUGIN_DST_BUF_RECT];
    m_mapDstV4L2Format  = (Array_buf_t)      (*map)[PLUGIN_DST_BUF_V4L2_FORMAT];
    m_mapDstBufAddr     = (Array_buf_addr_t) (*map)[PLUGIN_DST_BUF_1];  //target;
    m_mapDstBufFd       = (Array_buf_plane_t)(*map)[PLUGIN_DST_BUF_FD];
    m_mapDstBufWStride  = (Array_buf_t)      (*map)[PLUGIN_DST_BUF_WSTRIDE];

    m_bokehIntensity    = (Data_int32_t)     (*map)[PLUGIN_BOKEH_INTENSITY];

    m_buttonZoomMode     = (Data_int32_t)     (*map)[PLUGIN_BUTTON_ZOOM];
	
    // meta copy from src(wide) to dst.
    // then, every child class can use metadata.
    //
    // but, we need to decide to use which meta(wide or tele).
    // if it is, meta copy position can be change to child class.
    unsigned int bpp = 0;
    unsigned int planeCount = 1;

    ret = V4L2_PIX_2_YUV_INFO(m_mapSrcV4L2Format[0], &bpp, &planeCount);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):getYuvFormatInfo(srcRect[%d].colorFormat(%x)) fail", __FUNCTION__, __LINE__, m_mapSrcV4L2Format[0]);
    }

    int metaPlaneIndex = planeCount;

    char *srcMetaAddr = (char *)m_mapSrcBufAddr[0][metaPlaneIndex];
    char *dstMetaAddr = (char *)m_mapDstBufAddr[metaPlaneIndex];
    int   srcMetaSize = m_mapSrcBufSize[0][metaPlaneIndex];
    int   dstMetaSize = m_mapDstBufSize[0][metaPlaneIndex];

    m_copyMetaData(srcMetaAddr, srcMetaSize, dstMetaAddr, dstMetaSize);

    return ret;
}

status_t ArcsoftFusion::m_init(Map_t *map)
{
    status_t ret = NO_ERROR;

    m_destroyIon();

    ALOGD("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    return ret;
}

status_t ArcsoftFusion::m_uninit(Map_t *map)
{
    status_t ret = NO_ERROR;

    ALOGD("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    return ret;
}

status_t ArcsoftFusion::m_query(Map_t *map)
{
    status_t ret = NO_ERROR;

    if (map != NULL) {
        (*map)[PLUGIN_VERSION] = (Map_data_t)MAKE_VERSION(1, 0);
        (*map)[PLUGIN_LIB_NAME] = (Map_data_t) "ArcsoftFusionLib";
        (*map)[PLUGIN_BUILD_DATE] = (Map_data_t)__DATE__;
        (*map)[PLUGIN_BUILD_TIME] = (Map_data_t)__TIME__;
        (*map)[PLUGIN_PLUGIN_CUR_SRC_BUF_CNT] = (Map_data_t)2;
        (*map)[PLUGIN_PLUGIN_CUR_DST_BUF_CNT] = (Map_data_t)1;
        (*map)[PLUGIN_PLUGIN_MAX_SRC_BUF_CNT] = (Map_data_t)2;
        (*map)[PLUGIN_PLUGIN_MAX_DST_BUF_CNT] = (Map_data_t)1;
    }

    return ret;
}

void ArcsoftFusion::m_checkExcuteStartTime(void)
{
    m_excuteStartTime = systemTime();
}

void ArcsoftFusion::m_checkExcuteStopTime(bool flagLog)
{
    m_excuteStopTime = systemTime();

    m_emulationProcessTime = (m_excuteStopTime - m_excuteStartTime) / 1000LL;

    if (flagLog == true) {
        ALOGD("DEBUG(%s[%d]):m_emulationProcessTime : %d", __FUNCTION__, __LINE__, m_emulationProcessTime);
    }
}

void ArcsoftFusion::m_copyHalfHalf2Dst(Map_t *map)
{
    status_t ret = NO_ERROR;

    char *srcYAddr = NULL;
    char *srcCbcrAddr = NULL;
    char *dstYAddr = NULL;
    char *dstCbcrAddr = NULL;

    unsigned int bpp = 0;
    unsigned int planeCount = 1;

    int srcPlaneSize = 0;
    int srcHalfPlaneSize = 0;
    int dstPlaneSize = 0;
    int dstHalfPlaneSize = 0;
    int copySize = 0;

    for (int i = 0; i < m_mapSrcBufCnt; i++) {
        ARCSOFT_FUSION_LOGD("%s(%d): src[%d/%d]::(adr:%p, P%d, S%d, FD%d [%d, %d, %d, %d, %d, %d] format :%d",
            __FUNCTION__, __LINE__,
            i,
            m_mapSrcBufCnt,
            m_mapSrcBufAddr[i][0],
            m_mapSrcBufPlaneCnt[i],
            m_mapSrcBufSize[i][0],
            m_mapSrcBufFd[i][0],
            m_mapSrcRect[i][PLUGIN_ARRAY_RECT_X],
            m_mapSrcRect[i][PLUGIN_ARRAY_RECT_Y],
            m_mapSrcRect[i][PLUGIN_ARRAY_RECT_W],
            m_mapSrcRect[i][PLUGIN_ARRAY_RECT_H],
            m_mapSrcRect[i][PLUGIN_ARRAY_RECT_FULL_W],
            m_mapSrcRect[i][PLUGIN_ARRAY_RECT_FULL_H],
            m_mapSrcV4L2Format[i]);
    }

    ARCSOFT_FUSION_LOGD("%s(%d): dst[%d]::(adr:%p, P%d, S%d, FD%d [%d, %d, %d, %d, %d, %d] format :%d",
        __FUNCTION__, __LINE__,
        m_mapDstBufCnt,
        m_mapDstBufAddr[0],
        m_mapDstBufPlaneCnt[0],
        m_mapDstBufSize[0][0],
        m_mapDstBufFd[0][0],
        m_mapDstRect[0][PLUGIN_ARRAY_RECT_X],
        m_mapDstRect[0][PLUGIN_ARRAY_RECT_Y],
        m_mapDstRect[0][PLUGIN_ARRAY_RECT_W],
        m_mapDstRect[0][PLUGIN_ARRAY_RECT_H],
        m_mapDstRect[0][PLUGIN_ARRAY_RECT_FULL_W],
        m_mapDstRect[0][PLUGIN_ARRAY_RECT_FULL_H],
        m_mapDstV4L2Format[0]);

    /*
    * if previous emulationProcessTime is slow than 33msec,
    * we need change the next copy time
    *
    * ex :
    * frame 0 :
    * 1.0(copyRatio) = 33333 / 33333(previousFusionProcessTime : init value)
    * 1.0  (copyRatio) = 1.0(copyRatio) * 1.0(m_emulationCopyRatio);
    * m_emulationCopyRatio = 1.0
    * m_emulationProcessTime = 66666

    * frame 1 : because of frame0's low performance, shrink down copyRatio.
    * 0.5(copyRatio) = 33333 / 66666(previousFusionProcessTime)
    * 0.5(copyRatio) = 0.5(copyRatio) * 1.0(m_emulationCopyRatio);
    * m_emulationCopyRatio = 0.5
    * m_emulationProcessTime = 33333

    * frame 2 : acquire the proper copy time
    * 1.0(copyRatio) = 33333 / 33333(previousFusionProcessTime)
    * 0.5(copyRatio) = 1.0(copyRatio) * 0.5(m_emulationCopyRatio);
    * m_emulationCopyRatio = 0.5
    * m_emulationProcessTime = 16666

    * frame 3 : because of frame2's fast performance, increase copyRatio.
    * 2.0(copyRatio) = 33333 / 16666(previousFusionProcessTime)
    * 1.0(copyRatio) = 2.0(copyRatio) * 0.5(m_emulationCopyRatio);
    * m_emulationCopyRatio = 1.0
    * m_emulationProcessTime = 33333
    */

    int previousFusionProcessTime = m_emulationProcessTime;
    if (previousFusionProcessTime <= 0)
        previousFusionProcessTime = 1;

    float copyRatio = (float)FUSION_PROCESSTIME_STANDARD / (float)previousFusionProcessTime;

    if (previousFusionProcessTime < 1000)
        copyRatio = 1.0f;
    else
        copyRatio = copyRatio * m_emulationCopyRatio;

    if (1.0f <= copyRatio) {
        copyRatio = 1.0f;
    }
    else if (0.1f < copyRatio) {
        copyRatio -= 0.05f; // threshold value : 5%
    }
    else {
        ALOGW("WARN(%s[%d]):copyRatio(%f) is too smaller than 0.1f. previousFusionProcessTime(%d), m_emulationCopyRatio(%f)",
            __FUNCTION__, __LINE__, copyRatio, previousFusionProcessTime, m_emulationCopyRatio);
    }

    ALOGD("copyRatio:%f", copyRatio);
    m_emulationCopyRatio = copyRatio;

    for (int i = 0; i < m_mapSrcBufCnt; i++) {
        int srcFullW = m_mapSrcRect[i][PLUGIN_ARRAY_RECT_FULL_W];
        int srcFullH = m_mapSrcRect[i][PLUGIN_ARRAY_RECT_FULL_H];
        int dstFullW = m_mapDstRect[0][PLUGIN_ARRAY_RECT_FULL_W];
        int dstFullH = m_mapDstRect[0][PLUGIN_ARRAY_RECT_FULL_H];

        srcPlaneSize = srcFullW * srcFullH;
        srcHalfPlaneSize = srcPlaneSize / 2;

        dstPlaneSize = dstFullW * dstFullH;
        dstHalfPlaneSize = dstPlaneSize / 2;

        copySize = (srcHalfPlaneSize < dstHalfPlaneSize) ? srcHalfPlaneSize : dstHalfPlaneSize;

        ret = V4L2_PIX_2_YUV_INFO(m_mapSrcV4L2Format[i], &bpp, &planeCount);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):getYuvFormatInfo(srcRect[%d].colorFormat(%x)) fail", __FUNCTION__, __LINE__, i, m_mapSrcV4L2Format[i]);
        }

        srcYAddr = m_mapSrcBufAddr[i][0];
        dstYAddr = m_mapDstBufAddr[0];

        switch (planeCount) {
        case 1:
            srcCbcrAddr = m_mapSrcBufAddr[i][0] + srcFullW * srcFullH;
            dstCbcrAddr = m_mapDstBufAddr[0] + dstFullW * dstFullH;
            break;
        case 2:
            srcCbcrAddr = m_mapSrcBufAddr[i][1];
            dstCbcrAddr = m_mapDstBufAddr[1];
            break;
        default:
            android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):Invalid planeCount(%d), assert!!!!",
                __FUNCTION__, __LINE__, planeCount);
            break;
        }

        ARCSOFT_FUSION_LOGD("DEBUG(%s[%d]):fusion emulationn running ~~~ memcpy(%p, %p, %d)" \
            "by src(%d, %d), dst(%d, %d), previousFusionProcessTime(%d) copyRatio(%f)",
            __FUNCTION__, __LINE__,
            m_mapDstBufAddr[0], m_mapSrcBufAddr[i][0], copySize,
            srcFullW, srcFullH,
            dstFullW, dstFullH,
            previousFusionProcessTime, copyRatio);

        /* in case of source 2 */
        if (i == 1) {
            dstYAddr += dstHalfPlaneSize;
            dstCbcrAddr += dstHalfPlaneSize / 2;
        }

        if (srcFullW == dstFullW && srcFullH == dstFullH) {
            int oldCopySize = copySize;
            copySize = (int)((float)copySize * copyRatio);

            if (oldCopySize < copySize) {
                ALOGW("WARN(%s[%d]):oldCopySize(%d) < copySize(%d). just adjust oldCopySize",
                    __FUNCTION__, __LINE__, oldCopySize, copySize);

                copySize = oldCopySize;
            }

            memcpy(dstYAddr, srcYAddr, copySize);
            memcpy(dstCbcrAddr, srcCbcrAddr, copySize / 2);
        }
        else {
            int width = (srcFullW < dstFullW) ? srcFullW : dstFullW;
            int height = (srcFullH < dstFullH) ? srcFullH : dstFullH;

            int oldHeight = height;
            height = (int)((float)height * copyRatio);

            if (oldHeight < height) {
                ALOGW("WARN(%s[%d]):oldHeight(%d) < height(%d). just adjust oldHeight",
                    __FUNCTION__, __LINE__, oldHeight, height);

                height = oldHeight;
            }

            for (int h = 0; h < height / 2; h++) {
                memcpy(dstYAddr, srcYAddr, width);
                srcYAddr += srcFullW;
                dstYAddr += dstFullW;
            }

            for (int h = 0; h < height / 4; h++) {
                memcpy(dstCbcrAddr, srcCbcrAddr, width);
                srcCbcrAddr += srcFullW;
                dstCbcrAddr += dstFullW;
            }
        }
    }
}

void ArcsoftFusion::m_copyMetaData(char *srcMetaAddr, int srcMetaSize, char *dstMetaAddr, int dstMetaSize)
{
    if (srcMetaSize <= 0 || srcMetaAddr == NULL ||
        dstMetaSize <= 0 || dstMetaAddr == NULL ||
        srcMetaSize != dstMetaSize) {

        ALOGW("WARN(%s[%d]):no meta plane. be carefull.. srcMetaSize(%d), dstMetaSize(%d)",
            __FUNCTION__, __LINE__,
            srcMetaSize, dstMetaSize);
    }
    else {
        memcpy(dstMetaAddr, srcMetaAddr, dstMetaSize);
    }
}

void ArcsoftFusion::m_loadCalData(void)
{
    if (m_flagCalDataLoaded == false) {
        // loading cal data
        memset(m_calData, 0, ARC_DCOZ_CALIBRATIONDATA_LEN);

        int file_fd = open("/data/misc/cameraserver/RMO_imx350.bin", O_RDONLY);
        ssize_t read_bytes = 0;
        if (file_fd > 0) {
            lseek(file_fd, 13572, SEEK_SET);
            read_bytes = read(file_fd, m_calData, ARC_DCOZ_CALIBRATIONDATA_LEN);
            ALOGD("DEBUG(%s[%d]): read otp calibration data bytes = %ld", __FUNCTION__, __LINE__, read_bytes);

            ALOGD("DEBUG(%s[%d]):fd_close FD(%d)", __FUNCTION__, __LINE__, file_fd);
            close(file_fd);
        } else {
            ALOGE("ERR(%s[%d]): open file fail", __FUNCTION__, __LINE__);
        }

        if (read_bytes == ARC_DCOZ_CALIBRATIONDATA_LEN) {
            m_flagCalDataLoaded = true;
            ALOGD("DEBUG(%s[%d]): calData loaded success", __FUNCTION__, __LINE__);
        } else {
            ALOGE("ERR(%s[%d]): calData loaded fail", __FUNCTION__, __LINE__);
        }
    } else {
        ALOGD("DEBUG(%s[%d]): calData already loaded. so skip", __FUNCTION__, __LINE__);
    }
}

void ArcsoftFusion::m_init_ARC_DCOZ_INITPARAM(ARC_DCOZ_INITPARAM *initParam)
{
    initParam->fWideFov = 70.0f; //base on device
    initParam->fTeleFov = 40.0f; //base on device

    initParam->pCalData = m_calData;
    initParam->i32CalDataLen = ARC_DCOZ_CALIBRATIONDATA_LEN;
    initParam->fnCallback = MNull;
    initParam->pUserData = MNull;
}

// refer ExynosCameraMemory.cpp
status_t ArcsoftFusion::m_allocIonBuf(int size, int *fd, char **addr, bool mapNeeded)
{

    status_t ret = NO_ERROR;
    int ionFd = 0;
    char *ionAddr = NULL;

    size_t ionAlign = 0;
    unsigned int ionHeapMask = 0;
    unsigned int ionFlags = 0;
    bool isCached = false;

    if (m_ionClient == 0) {
        m_ionClient = ion_open();

        if (m_ionClient < 0) {
            ALOGE("ERR(%s):ion_open failed", __FUNCTION__);
            ret = BAD_VALUE;
            goto func_exit;
        }
    }

    ionAlign = 0;
    ionHeapMask = ION_HEAP_SYSTEM_MASK;
    ionFlags = (isCached == true ?
        (ION_FLAG_CACHED | ION_FLAG_CACHED_NEEDS_SYNC) : 0);

    ret = ion_alloc_fd(m_ionClient, size, ionAlign, ionHeapMask, ionFlags, &ionFd);

    if (ret < 0) {
        ALOGE("ERR(%s):ion_alloc_fd(fd=%d) failed(%s)", __FUNCTION__, ionFd, strerror(errno));
        ionFd = -1;
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (mapNeeded == true) {
        ionAddr = (char *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, ionFd, 0);

        if (ionAddr == (char *)MAP_FAILED || ionAddr == NULL) {
            ALOGE("ERR(%s):ion_map(size=%d) failed, (ionFd=%d), (%s)", __FUNCTION__, size, ionFd, strerror(errno));
            close(ionFd);
            ionAddr = NULL;
            ret = INVALID_OPERATION;
            goto func_exit;
        }

    }

func_exit:

    *fd = ionFd;
    *addr = ionAddr;

    return ret;
}

status_t ArcsoftFusion::m_freeIonBuf(int size, int *fd, char **addr)
{
    status_t ret = NO_ERROR;
    int ionFd = *fd;
    char *ionAddr = *addr;

    if (ionFd < 0) {
        ALOGE("ERR(%s):ion_fd is lower than zero", __FUNCTION__);
        ret = BAD_VALUE;
        goto func_exit;
    }

    if (ionAddr == NULL) {
        ALOGW("WARN(%s):ion_addr equals NULL", __FUNCTION__);
    } else {
        if (munmap(ionAddr, size) < 0) {
            ALOGE("ERR(%s):munmap failed", __FUNCTION__);
            ret = INVALID_OPERATION;
            goto func_close_exit;
        }
    }

func_close_exit:

    ALOGD("DEBUG(%s[%d]):ion_close FD(%d)", __FUNCTION__, __LINE__, ionFd);
    ion_close(ionFd);

    ionFd = -1;
    ionAddr = NULL;

func_exit:

    *fd = ionFd;
    *addr = ionAddr;

    return ret;
}

void ArcsoftFusion::m_destroyIon(void)
{
    for (int i = 0; i < 2; i++) {
        if (0 < m_tempSrcBufferFd[i]) {
            m_freeIonBuf(m_tempSrcBufferSize[i], &m_tempSrcBufferFd[i], &m_tempSrcBufferAddr[i]);
        }

        m_tempSrcBufferSize[i] = 0;

        m_tempSrcBufferFd[i] = 0;
        m_tempSrcBufferAddr[i] = NULL;
    }

    if (m_ionClient != 0) {
        ALOGD("DEBUG(%s[%d]):ion_close FD(%d)", __FUNCTION__, m_ionClient);
        ion_close(m_ionClient);
        m_ionClient = 0;
    }
}

status_t ArcsoftFusion::m_createScaler(void)
{
    status_t ret = NO_ERROR;

#ifdef ARCSOFT_FUSION_USE_G2D
    ret = m_createG2D();
    if (ret != NO_ERROR) {
        ALOGE("ERR(%s[%d]):m_createG2D() fail", __FUNCTION__, __LINE__);
    }
#else
    ret = m_createCsc();
    if (ret != NO_ERROR) {
        ALOGE("ERR(%s[%d]):m_createCsc() fail", __FUNCTION__, __LINE__);
    }
#endif

    return ret;
}

void ArcsoftFusion::m_destroyScaler(void)
{
#ifdef ARCSOFT_FUSION_USE_G2D
    m_destroyG2D();
#else
    m_destroyCsc();
#endif
}

status_t ArcsoftFusion::m_drawScaler(int *srcRect, int srcFd, int srcBufSize,
                                     int *dstRect, int dstFd, int dstBufSize)
{
    status_t ret = NO_ERROR;

    ALOGD("DEBUG(%s[%d]):m_drawScaler:src(%d, %d, %d, %d, in %d x %d), srcFd(%d), srcBufSize(%d)",
        __FUNCTION__, __LINE__,
        srcRect[0], srcRect[1], srcRect[2], srcRect[3], srcRect[4], srcRect[5],
        srcFd,
        srcBufSize);

    ALOGD("DEBUG(%s[%d]):m_drawScaler:dst(%d, %d, %d, %d, in %d x %d), dstFd(%d), dstBufSize(%d)",
        __FUNCTION__, __LINE__,
        dstRect[0], dstRect[1], dstRect[2], dstRect[3], dstRect[4], dstRect[5],
        dstFd,
        dstBufSize);

#ifdef ARCSOFT_FUSION_USE_G2D
    ret = m_drawG2D(srcRect, srcFd, srcBufSize,
                    dstRect, dstFd, dstBufSize);
    if (ret != NO_ERROR) {
        ALOGE("ERR(%s[%d]):m_drawG2D() fail", __FUNCTION__, __LINE__);
    }
#else
    ret = m_drawCsc(srcRect, srcFd, srcBufSize,
                    dstRect, dstFd, dstBufSize);
    if (ret != NO_ERROR) {
        ALOGE("ERR(%s[%d]):m_drawCsc() fail", __FUNCTION__, __LINE__);
    }
#endif

    return ret;
}

// refer : ExynosCameraUtils.h
#ifdef SAFE_DELETE
#else
#define SAFE_DELETE(obj) \
    do { \
        if (obj) { \
            delete obj; \
            obj = NULL; \
        } \
    } while(0)
#endif

// refer : ExynosCameraPPLibacryl.cpp
status_t ArcsoftFusion::m_createG2D(void)
{
    status_t ret = NO_ERROR;
    int     iRet = -1;

    if (m_acylic != NULL) {
        //ALOGD("it already m_acylic != NULL)");
        return NO_ERROR;
    }

    // create your own library.
    m_acylic = AcrylicFactory::createAcrylic("default_compositor");
    if (m_acylic == NULL) {
        ALOGE("AcrylicFactory::createAcrylic() fail");
        ret = INVALID_OPERATION;
        goto done;
    }

    m_layer = m_acylic->createLayer();
    if (m_layer == NULL) {
        ALOGE("m_acylic->createLayer() fail");
        ret = INVALID_OPERATION;
        goto done;
    }

    iRet = m_acylic->prioritize(2);
    if (iRet < 0) {
        ALOGE("m_acylic->priortize() fail");
    }

done:
    if (ret != NO_ERROR) {
        SAFE_DELETE(m_layer);
        SAFE_DELETE(m_acylic);
    }

    return ret;
}

void ArcsoftFusion::m_destroyG2D(void)
{
    int     iRet = -1;

    if (m_acylic != NULL) {
        iRet = m_acylic->prioritize(0);
        if (iRet < 0) {
            ALOGE("m_acylic->priortize() fail");
        }
    }

    // destroy your own library.
    SAFE_DELETE(m_layer);
    SAFE_DELETE(m_acylic);
}

status_t ArcsoftFusion::m_drawG2D(int *srcRect, int srcFd, int srcBufSize,
                                  int *dstRect, int dstFd, int dstBufSize)
{
    status_t ret = NO_ERROR;
    bool    bRet = false;

    ret = m_createG2D();
    if (ret != NO_ERROR) {
        ALOGE("ERR(%s[%d]):m_createCsc() fail", __FUNCTION__, __LINE__);
        return ret;
    }

    /* draw your own library. */

    ///////////////////////
    // src image
    //int srcW = ALIGN_UP(srcRect[2], GSCALER_IMG_ALIGN);
    int srcW = srcRect[4];
    int srcH = srcRect[5];
    // int srcColorFormat = V4L2_PIX_2_HAL_PIXEL_FORMAT(srcImage.rect.colorFormat);
    int srcColorFormat = HAL_PIXEL_FORMAT_YCrCb_420_SP;
    int srcDataSpace = HAL_DATASPACE_V0_JFIF;

    bRet = m_layer->setImageDimension(srcW, srcH);
    if (bRet == false) {
        ALOGE("m_layer->setImageDimension(srcW : %d, srcH : %d) fail", srcW, srcH);
        return INVALID_OPERATION;
    }

    hwc_rect_t rect;
    rect.left   = srcRect[0];
    rect.top    = srcRect[1];
    rect.right  = srcRect[0] + srcRect[2];
    rect.bottom = srcRect[1] + srcRect[3];

    // caller set rotation on destination.
    uint32_t transform = 0;

    /*
    switch (dstImage.rotation) {
    case 0:
        transform = 0;
        break;
    case 90:
        transform |= HW2DCapability::TRANSFORM_ROT_90;
        break;
    case 180:
        transform |= HW2DCapability::TRANSFORM_ROT_180;
        break;
    case 270:
        transform |= HW2DCapability::TRANSFORM_ROT_270;
        break;
    default:
        ALOGE("Invalid dstImage.rotation(%d). so, fail", dstImage.rotation);
        return INVALID_OPERATION;
        break;
    }
    */

    /*
    // caller set flip on destination.
    if (dstImage.flipH == true) {
        transform |= HW2DCapability::TRANSFORM_FLIP_H;
    }

    if (dstImage.flipV == true) {
        transform |= HW2DCapability::TRANSFORM_FLIP_V;
    }
    */

    uint32_t attr = 0;

    if ((srcRect[2] > dstRect[2] && srcRect[2] < dstRect[2] * 4) ||
        (srcRect[3] > dstRect[3] && srcRect[3] < dstRect[3] * 4)) {
        // when making smaller, dst line is bigger than src line by x1/4 : ex : 1/2 ~ 1/4
        attr = 0;
    } else {
        // when making smaller, dst line is smaller than src line by x1/4 : ex : 1/4 ~ 1/8
        // when make bigger or same. : ex x1 ~ x8
        attr = AcrylicLayer::ATTR_NORESAMPLING;
    }

    m_layer->setCompositArea(rect, transform, attr);
    if (bRet == false) {
        ALOGE("m_layer->setCompositArea(left : %d, top : %d, right : %d, bottom : %d, transform : %d, attr : %d) fail",
            rect.left, rect.top, rect.right, rect.bottom, transform, attr);
        return INVALID_OPERATION;
    }

    bRet = m_layer->setImageType(srcColorFormat, srcDataSpace);
    if (bRet == false) {
        ALOGE("m_layer->setImageType(srcColorFormat : %d, srcDataSpace : %d) fail",
            srcColorFormat, srcDataSpace);
        return INVALID_OPERATION;
    }

    /*
    int srcPlaneCount = getYuvPlaneCount(srcImage.rect.colorFormat);
    if (srcPlaneCount < 0) {
        ALOGE("getYuvPlaneCount(format:%c%c%c%c) fail",
            v4l2Format2Char(srcImage.rect.colorFormat, 0),
            v4l2Format2Char(srcImage.rect.colorFormat, 1),
            v4l2Format2Char(srcImage.rect.colorFormat, 2),
            v4l2Format2Char(srcImage.rect.colorFormat, 3));
        return INVALID_OPERATION;
    }
    */
    int srcPlaneCount = 1; // NV21

    int    fd[MAX_HW2D_PLANES] = {-1,};
    size_t len[MAX_HW2D_PLANES] = {0,};

    /*
    for (int i = 0; i < srcPlaneCount; i++) {
        fd[i]  = srcImage.buf.fd[i];
        len[i] = srcImage.buf.size[i];
    }
    */
    fd[0] = srcFd;
    len[0] = srcBufSize;

    bRet = m_layer->setImageBuffer(fd, len, srcPlaneCount);
    if (bRet == false) {
        ALOGE("m_layer->setImageBuffer(fd : %d/%d/%d, len : %d/%d/%d, srcPlaneCount : %d) fail",
            fd[0],  fd[1],  fd[2],
            len[0], len[2], len[2],
            srcPlaneCount);
        return INVALID_OPERATION;
    }

    ///////////////////////
    // dst image
    int dstW = dstRect[4];
    int dstH = dstRect[5];
    //int dstColorFormat = V4L2_PIX_2_HAL_PIXEL_FORMAT(dstImage.rect.colorFormat);
    int dstColorFormat = HAL_PIXEL_FORMAT_YCrCb_420_SP;
    int dstDataSpace = HAL_DATASPACE_V0_JFIF;

    bRet = m_acylic->setCanvasDimension(dstW, dstH);
    if (bRet == false) {
        ALOGE("m_acylic->setCanvasDimension(dstW : %d, dstH : %d) fail", dstW, dstH);
        return INVALID_OPERATION;
    }

    bRet = m_acylic->setCanvasImageType(dstColorFormat, dstDataSpace);
    if (bRet == false) {
        ALOGE("m_acylic->setCanvasImageType(dstColorFormat : %d, dstDataSpace : %d) fail",
            dstColorFormat, dstDataSpace);
        return INVALID_OPERATION;
    }

    /*
    int dstPlaneCount = getYuvPlaneCount(dstImage.rect.colorFormat);
    if (dstPlaneCount < 0) {
        ALOGE("getYuvPlaneCount(format:%c%c%c%c) fail",
            v4l2Format2Char(srcImage.rect.colorFormat, 0),
            v4l2Format2Char(srcImage.rect.colorFormat, 1),
            v4l2Format2Char(srcImage.rect.colorFormat, 2),
            v4l2Format2Char(srcImage.rect.colorFormat, 3));
        return INVALID_OPERATION;
    }
    */
    int dstPlaneCount = 1; // NV21

    /*
    for (int i = 0; i < dstPlaneCount; i++) {
        fd[i]  = dstImage.buf.fd[i];
        len[i] = dstImage.buf.size[i];
    }
    */
    fd[0] = dstFd;
    len[0] = dstBufSize;

    bRet = m_acylic->setCanvasBuffer(fd, len, dstPlaneCount);
    if (bRet == false) {
        ALOGE("m_acylic->setCanvasImageType(fd : %d/%d/%d, len : %d/%d/%d, dstPlaneCount : %d) fail",
            fd[0],  fd[1],  fd[2],
            len[0], len[2], len[2],
            dstPlaneCount);
        return INVALID_OPERATION;
    }

    ///////////////////////
    // execute
    bRet = m_acylic->execute(NULL);
    if (bRet == false) {
        ALOGE("m_acylic->execute() fail");
        return INVALID_OPERATION;
    }

    return ret;
}

// refer : ExynosCameraPPLibcsc.cpp
status_t ArcsoftFusion::m_createCsc(void)
{
    if (m_csc != NULL) {
        //ALOGD("it already m_csc != NULL)");
        return NO_ERROR;
    }

    // create your own library.
    CSC_METHOD cscMethod = CSC_METHOD_HW;

    m_csc = csc_init(cscMethod);
    if (m_csc == NULL) {
        ALOGE("ERR(%s[%d]):csc_init() fail", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    csc_set_hw_property(m_csc, CSC_HW_PROPERTY_FIXED_NODE, (4)); // PICTURE_GSC_NODE_NUM

    return NO_ERROR;
}

void ArcsoftFusion::m_destroyCsc(void)
{
    // destroy your own library.
    if (m_csc != NULL)
        csc_deinit(m_csc);
    m_csc = NULL;
}

status_t ArcsoftFusion::m_drawCsc(int *srcRect, int srcFd, int srcBufSize,
                                  int *dstRect, int dstFd, int dstBufSize)
{
    status_t ret = NO_ERROR;

    ret = m_createCsc();
    if (ret != NO_ERROR) {
        ALOGE("ERR(%s[%d]):m_createCsc() fail", __FUNCTION__, __LINE__);
        return ret;
    }

#if 1
    csc_set_src_format(m_csc,
        srcRect[4], srcRect[5], // full size
        srcRect[0], srcRect[1], srcRect[2], srcRect[3], // x, y, w, h
        HAL_PIXEL_FORMAT_YCrCb_420_SP,
        0);

    csc_set_dst_format(m_csc,
        dstRect[4], dstRect[5], // full size
        dstRect[0], dstRect[1], dstRect[2], dstRect[3], // x, y, w, h
        HAL_PIXEL_FORMAT_YCrCb_420_SP,
        0);

    int srcFdArr[4];
    srcFdArr[0] = srcFd;

    int dstFdArr[4];
    dstFdArr[0] = dstFd;

    csc_set_src_buffer(m_csc,
        (void **)&srcFdArr, CSC_MEMORY_DMABUF);

    csc_set_dst_buffer(m_csc,
        (void **)&dstFdArr, CSC_MEMORY_DMABUF);

    ret = csc_convert_with_rotation(m_csc, 0, 0, 0);
    if (ret != NO_ERROR) {
        ALOGE("ERR(%s[%d]):csc_convert_with_rotation(srcRect(%d, %d, %d, %d, in %d x %d) srcFd(%d) / dstRect(%d, %d, %d, %d, in %d x %d) dstFd(%d) fail",
            __FUNCTION__, __LINE__,
            srcRect[0], srcRect[1], srcRect[2], srcRect[3], srcRect[4], srcRect[5],
            srcFd,
            dstRect[0], dstRect[1], dstRect[2], dstRect[3], dstRect[4], dstRect[5],
            dstFd);
        ret = INVALID_OPERATION;
    }
#endif

    return ret;
}

void ArcsoftFusion::m_saveValue(Map_t *map)
{
    // save the current value
    (*map)[PLUGIN_CAMERA_INDEX]  = (Map_data_t)m_cameraIndex;
    (*map)[PLUGIN_NEED_SWITCH_CAMERA]    = (Map_data_t)m_needSwitchCamera;
    (*map)[PLUGIN_MASTER_CAMERA_ARC2HAL] = (Map_data_t)m_masterCameraArc2HAL;

    (*map)[PLUGIN_VIEW_RATIO]    = (Map_data_t)(m_viewRatio * 1000.0f);
    (*map)[PLUGIN_IMAGE_RATIO]   = (Map_data_t)m_imageRatio;
    (*map)[PLUGIN_IMAGE_SHIFT_X] = (Map_data_t)m_imageShiftX;
    (*map)[PLUGIN_IMAGE_SHIFT_Y] = (Map_data_t)m_imageShiftY;
}
