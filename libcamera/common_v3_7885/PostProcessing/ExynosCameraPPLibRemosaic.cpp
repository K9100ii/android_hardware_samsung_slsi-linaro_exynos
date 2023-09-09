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
#define LOG_TAG "ExynosCameraPPLibRemosaic"
#include <cutils/log.h>

#include "ExynosCameraPPLibRemosaic.h"

//#define EXYNOS_CAMERA_PP_LIB_REMOSAIC_EMULATION
#define EXYNOS_CAMERA_PP_LIB_REMOSAIC_READ_EEPROM_FILE_FOR_TEST
//#define EXYNOS_CAMERA_PP_LIB_REMOSAIC_USE_BAYER_FILE_FOR_INPUT_TEST
//#define EXYNOS_CAMERA_PP_LIB_REMOSAIC_USE_BAYER_FILE_FOR_WRITE_TEST

#define  EXYNOS_CAMERA_PP_LIB_REMOSAIC_W (4608)
#define  EXYNOS_CAMERA_PP_LIB_REMOSAIC_H (3456)

ExynosCameraPPLibRemosaic::~ExynosCameraPPLibRemosaic()
{
}

status_t ExynosCameraPPLibRemosaic::m_create(void)
{
    status_t ret = NO_ERROR;

    // create your own library.
    ret = m_ionOpen();
    if (ret != NO_ERROR) {
        CLOGE("m_ionOpen() fail");
        goto done;
    }

    if (m_flagDelayedCreated == false) {
        ret = m_delayedCreate();
        if (ret != NO_ERROR) {
            CLOGE("m_delayedCreate() fail");
            ret = INVALID_OPERATION;
            goto done;
        }
    }

done:
    if (ret != NO_ERROR) {
        m_ionClose();
    }

    return ret;
}

status_t ExynosCameraPPLibRemosaic::m_destroy(void)
{
    status_t ret = NO_ERROR;

    if (m_flagDelayedCreated == false) {
        CLOGD("m_flagDelayedCreated == false. so, skip destroy");
        goto done;
    }

    // destroy your own library.
    if (m_flagRemosaicInited == true) {
#ifdef EXYNOS_CAMERA_PP_LIB_REMOSAIC_EMULATION
#else
        remosaic_deinit();
#endif
        m_flagRemosaicInited = false;
    }

    if (0 <= m_ionEepromBufFd) {
        ret = m_ionFree(&m_ionEepromBufFd, &m_ionEepromBufAddr, m_ionEepromBufSize);
        if (ret != NO_ERROR) {
            CLOGD("m_ionFree(m_ionEepromBufFd : %d, m_ionEepromBufAddr : %p, m_ionEepromBufSize : %d) fail",
                m_ionEepromBufFd,
                m_ionEepromBufAddr,
                m_ionEepromBufSize);
            goto done;
        }
    }

    m_ionClose();

done:
    return ret;
}

status_t ExynosCameraPPLibRemosaic::m_draw(ExynosCameraImage srcImage, ExynosCameraImage dstImage)
{
    status_t ret = NO_ERROR;
    int      iRet = 0;

    /* draw your own library. */
    camera2_shot_ext * metaData = NULL;
    int srcMetaPlaneIndex = srcImage.buf.getMetaPlaneIndex();
    int dstMetaPlaneIndex = dstImage.buf.getMetaPlaneIndex();

    ///////////////////////
    // this will check remosaic's performance
    struct st_remosaic_param p_param;

    ExynosCameraDurationTimer remosaicTotalTimer;
    ExynosCameraDurationTimer remosaicProcessTimer;

    remosaicTotalTimer.start();

    if (srcMetaPlaneIndex == 0 || dstMetaPlaneIndex == 0) {
        CLOGE("srcMetaPlaneIndex : %d \
               dstMetaPlaneIndex : %d \
               it seems no MetaPlaneIndex. so skip remosaic",
            srcMetaPlaneIndex,
            dstMetaPlaneIndex);
        goto done;
    }

    // we must update the p_param of the current value.
    metaData = (camera2_shot_ext *)srcImage.buf.addr[srcMetaPlaneIndex];

    // default : 1024
    p_param.wb_r_gain  = (int16_t)(metaData->shot.dm.color.gains[0] * 1024.0f);

    // default : 1024
	p_param.wb_gr_gain = (int16_t)(metaData->shot.dm.color.gains[1] * 1024.0f);

    // default : 1024
	p_param.wb_gb_gain = (int16_t)(metaData->shot.dm.color.gains[2] * 1024.0f);

    // default : 1024
	p_param.wb_b_gain  = (int16_t)(metaData->shot.dm.color.gains[3] * 1024.0f);

    // default : 32
	p_param.analog_gain = (int32_t)metaData->shot.udm.sensor.analogGain;

    // default : 100
	p_param.cur_line_count = 100;

    CLOGD("Remosaic infomation(wb_r_gain : %f -> %d, wb_gr_gain : %f -> %d, wb_gb_gain : %f -> %d, wb_b_gain : %f -> %d, analog_gain : %d -> %d, cur_line_count : %d",
        metaData->shot.dm.color.gains[0],
        p_param.wb_r_gain,
        metaData->shot.dm.color.gains[1],
	    p_param.wb_gr_gain,
        metaData->shot.dm.color.gains[2],
	    p_param.wb_gb_gain,
        metaData->shot.dm.color.gains[3],
	    p_param.wb_b_gain,
        metaData->shot.udm.sensor.analogGain,
	    p_param.analog_gain,
	    p_param.cur_line_count);

    CLOGD("Remosaic infomation(srcImage.buf.fd[0] : %d, srcImage.buf.size[0] : %d, dstImage.buf.fd[0] : %d, dstImage.buf.size[0] : %d",
        srcImage.buf.fd[0], srcImage.buf.size[0],
        dstImage.buf.fd[0], dstImage.buf.size[0]);

#ifdef EXYNOS_CAMERA_PP_LIB_REMOSAIC_USE_BAYER_FILE_FOR_INPUT_TEST
    m_testReadRemosaicBayerFile(srcImage);
#endif

#ifdef EXYNOS_CAMERA_PP_LIB_REMOSAIC_EMULATION
    ///////////////////////
    // copy whole image
    for (int i = 0; i < srcImage.buf.getMetaPlaneIndex(); i++) {
        if (srcImage.buf.addr[i] == NULL || srcImage.buf.size[i] == 0 ||
            dstImage.buf.addr[i] == NULL || dstImage.buf.size[i] == 0) {
            CLOGW("skip memcpy by srcImage.buf.addr[%d] : %p, srcImage.buf.size[%d] : %d, dstImage.buf.addr[%d] : %p, dstImage.buf.size[%d] : %d",
                i, srcImage.buf.addr[i], i, srcImage.buf.size[i],
                i, dstImage.buf.addr[i], i, dstImage.buf.size[i]);

            continue;
        }

        int minSize = srcImage.buf.size[i];
        if (dstImage.buf.size[i] < minSize) {
            minSize = dstImage.buf.size[i];
            CLOGW("size different : srcImage.buf.size[%d] : %d, dstImage.buf.size[%d] : %d",
                i, srcImage.buf.size[i], i, dstImage.buf.size[i]);
        }

        memcpy(dstImage.buf.addr[i], srcImage.buf.addr[i], minSize);
    }

    goto done;
#endif // EXYNOS_CAMERA_PP_LIB_REMOSAIC_EMULATION

    if (m_flagDelayedCreated == false) {
        ret = m_delayedCreate();
        if (ret != NO_ERROR) {
            CLOGE("m_delayedCreate() fail");
            ret = INVALID_OPERATION;
            goto done;
        }
    }

    remosaicProcessTimer.start();

    ///////////////////////
    //remosaic_process_param_set
    remosaic_process_param_set(&p_param);

    ///////////////////////
    //remosaic_process

    iRet = remosaic_process(srcImage.buf.fd[0], srcImage.buf.size[0],
                            dstImage.buf.fd[0], dstImage.buf.size[0]);
    if (iRet != RET_OK) {
        CLOGE("remosaic_process(srcImage.buf.fd[0] : %d, srcImage.buf.size[0] : %d, \
                                dstImage.buf.fd[0] : %d, dstImage.buf.size[0] : %d) fail",
            srcImage.buf.fd[0], srcImage.buf.size[0],
            dstImage.buf.fd[0], dstImage.buf.size[0]);
        ret = INVALID_OPERATION;

        remosaicProcessTimer.stop();
        goto done;
    }

    remosaicProcessTimer.stop();

    CLOGD("remosaicProcessTimer : %d msec", (int)remosaicProcessTimer.durationUsecs() / 1000);

#ifdef EXYNOS_CAMERA_PP_LIB_REMOSAIC_USE_BAYER_FILE_FOR_WRITE_TEST
    m_testWriteRemosaicBayerFile(srcImage, dstImage);
#endif

done:
    ///////////////////////
    // copy metaData from src to dst
    if (srcMetaPlaneIndex == 0 || dstMetaPlaneIndex == 0) {
        CLOGE("srcMetaPlaneIndex : %d \
               dstMetaPlaneIndex : %d \
               it seems no MetaPlaneIndex. so skip the meta copy",
            srcMetaPlaneIndex,
            dstMetaPlaneIndex);
    } else if (srcMetaPlaneIndex != dstMetaPlaneIndex) {
        CLOGE("MetaPlaneIndex size of src(%d)/dst(%d) are different. so skip the meta copy",
            srcMetaPlaneIndex,
            dstMetaPlaneIndex);
    } else {
        int minSize = srcImage.buf.size[srcMetaPlaneIndex];

        if (dstImage.buf.size[dstMetaPlaneIndex] < (unsigned int)minSize) {
            minSize = dstImage.buf.size[dstMetaPlaneIndex];
            CLOGE("meta size different : srcImage.buf.size[%d] : %d, dstImage.buf.size[%d] : %d",
                srcMetaPlaneIndex, srcImage.buf.size[srcMetaPlaneIndex],
                dstMetaPlaneIndex, dstImage.buf.size[dstMetaPlaneIndex]);
        }

        memcpy(dstImage.buf.addr[dstMetaPlaneIndex], srcImage.buf.addr[srcMetaPlaneIndex], minSize);
    }

    remosaicTotalTimer.stop();

    CLOGD("remosaicTotalTimer : %d msec", (int)remosaicTotalTimer.durationUsecs() / 1000);

    return ret;
}

void ExynosCameraPPLibRemosaic::m_setName(void)
{
    strncpy(m_name, "ExynosCameraPPLibRemosaic",  EXYNOS_CAMERA_NAME_STR_SIZE - 1);
}

void ExynosCameraPPLibRemosaic::m_init(void)
{
    m_flagRemosaicInited = false;
    m_flagDelayedCreated = false;
    m_ionEepromBufAddr = NULL;
    m_ionEepromBufSize = 0;

    m_ionClient        = 0;
    m_ionEepromBufFd   = -1;
    m_ionEepromBufAddr = NULL;
    m_ionEepromBufSize = 0;

    m_srcImageCapacity.setNumOfImage(1);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_SBGGR16);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_SBGGR12);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_SBGGR10);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_SBGGR10P);

    m_dstImageCapacity.setNumOfImage(1);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_SBGGR16);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_SBGGR12);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_SBGGR10);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_SBGGR10P);
}

status_t ExynosCameraPPLibRemosaic::m_delayedCreate(void)
{
    status_t ret = NO_ERROR;
    int      iRet = 0;

    int                    pedestal = 64;


    ExynosCameraDurationTimer remosaicInitTimer;
    remosaicInitTimer.start();

    if (m_flagDelayedCreated == true) {
        CLOGE("m_flagDelayedCreated == true. so, fail");
        ret = INVALID_OPERATION;
        goto done;
    }

#ifdef EXYNOS_CAMERA_PP_LIB_REMOSAIC_EMULATION
    m_flagDelayedCreated = true;
    goto done;
#endif
    ///////////////////////
    // remosaic_debug
    // if you want, turn on.
    //remosaic_debug(0);

    ///////////////////////
    // remosaic_init
    remosaic_init(EXYNOS_CAMERA_PP_LIB_REMOSAIC_W, EXYNOS_CAMERA_PP_LIB_REMOSAIC_H, BAYER_GRBG, pedestal);

    m_flagRemosaicInited = true;

    ///////////////////////
    //remosaic_gainmap_gen
    m_ionEepromBufAddr = m_readEEPROM(&m_ionEepromBufSize, &m_ionEepromBufFd);
    if (m_ionEepromBufAddr == NULL) {
        CLOGE("m_readEEPROM() fail");
        ret = INVALID_OPERATION;
        goto done;
    }

    iRet = remosaic_gainmap_gen(m_ionEepromBufAddr, m_ionEepromBufSize);
    if (iRet != RET_OK) {
        CLOGE("remosaic_gainmap_gen(m_ionEepromBufAddr : %p, m_ionEepromBufSize : %d) fail",
            m_ionEepromBufAddr, m_ionEepromBufSize);
        ret = INVALID_OPERATION;
        goto done;
    }

    m_flagDelayedCreated = true;

done:
    remosaicInitTimer.stop();
    CLOGD("remosaicInitTimer : %d msec", (int)remosaicInitTimer.durationUsecs() / 1000);

    if (ret != NO_ERROR) {
        if (m_flagRemosaicInited == true) {
#ifdef EXYNOS_CAMERA_PP_LIB_REMOSAIC_EMULATION
#else
            remosaic_deinit();
#endif
            m_flagRemosaicInited = false;
        }
    }

    return ret;
}

status_t ExynosCameraPPLibRemosaic::m_dlOpen(void)
{
    #define LIB_REMOSAIC_LIB_PATH "/system/lib/libremosaic_daemon.so"

    if (m_remosaicDL.lib != NULL) {
        CLOGE("m_remosaicDL.lib != NULL. so, fail");
        return INVALID_OPERATION;
    }

    m_remosaicDL.lib = dlopen(LIB_REMOSAIC_LIB_PATH, RTLD_NOW);
    if (m_remosaicDL.lib == NULL) {
        CLOGE("dlopen(%s) fail (%s)", LIB_REMOSAIC_LIB_PATH, dlerror());
        return INVALID_OPERATION;
    }

    m_remosaicDL.remosaic_init              = (remosaicDL::remosaic_init_t)              dlsym(m_remosaicDL.lib, "remosaic_init_t");
    m_remosaicDL.remosaic_gainmap_gen       = (remosaicDL::remosaic_gainmap_gen_t)       dlsym(m_remosaicDL.lib, "remosaic_gainmap_gen_t");
    m_remosaicDL.remosaic_process_param_set = (remosaicDL::remosaic_process_param_set_t) dlsym(m_remosaicDL.lib, "remosaic_process_param_set_t");
    m_remosaicDL.remosaic_process           = (remosaicDL::remosaic_process_t)           dlsym(m_remosaicDL.lib, "remosaic_process_t");
    m_remosaicDL.remosaic_deinit            = (remosaicDL::remosaic_deinit_t)            dlsym(m_remosaicDL.lib, "remosaic_deinit_t");
    m_remosaicDL.remosaic_debug             = (remosaicDL::remosaic_debug_t)             dlsym(m_remosaicDL.lib, "remosaic_debug_t");

    CLOGD("dlopen(%s) succeed", LIB_REMOSAIC_LIB_PATH);

    return NO_ERROR;
}

status_t ExynosCameraPPLibRemosaic::m_dlClose(void)
{
    status_t ret = 0;

    if (m_remosaicDL.lib == NULL) {
        CLOGD("m_remosaicDL.lib == NULL. so, ignore");
        return ret;
    }

    ret = dlclose(m_remosaicDL.lib);
    if (ret != 0) {
        CLOGE("dlclose(%s) fail (%s)", LIB_REMOSAIC_LIB_PATH, dlerror());
        ret = INVALID_OPERATION;
    }

    m_remosaicDL.lib = NULL;

    return ret;
}

char * ExynosCameraPPLibRemosaic::m_readEEPROM(int *eepromBufSize, int *eepromBufFd)
{
    status_t ret = NO_ERROR;

    FILE *fp = NULL;
    char *eepromfilePath = NULL;
    char *eepromBufAddr = NULL;
    size_t result = 0;
    ExynosCameraDurationTimer eepromTimer;

    if (eepromBufSize == NULL) {
        CLOGE("eepromBufSize == NULL. so, fail");
        return NULL;
    }

    if (eepromBufFd == NULL) {
        CLOGE("eepromBufFd == NULL. so, fail");
        return NULL;
    }

    eepromTimer.start();

#ifdef EXYNOS_CAMERA_PP_LIB_REMOSAIC_READ_EEPROM_FILE_FOR_TEST
    eepromfilePath = (char*)"/data/media/0/eeprom_data_little.dat";
#else
#ifdef BOARD_USES_MAIN_CAMERA_REMOSAIC_CAPTURE
    eepromfilePath = SENSOR_FW_PATH_BACK;
#else
    eepromfilePath = SENSOR_FW_PATH_FRONT;
#endif // BOARD_USES_MAIN_CAMERA_REMOSAIC_CAPTURE
#endif // EXYNOS_CAMERA_PP_LIB_REMOSAIC_READ_EEPROM_FILE_FOR_TEST

    fp = fopen(eepromfilePath, "r");

    if (fp == NULL) {
        CLOGE("fopen(%s) fail", eepromfilePath);
        goto done;
    }

    fseek(fp, 0, SEEK_END);
    *eepromBufSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (*eepromBufSize <= 0) {
        CLOGE("*eepromBufSize(%d) < = 0. so, skip fread", *eepromBufSize);
        goto done;
    }

    eepromBufAddr = m_ionAlloc(*eepromBufSize, eepromBufFd);

    // Read complete data
    result = fread(eepromBufAddr, 1, *eepromBufSize, fp);
    if((int)result != *eepromBufSize) {
        CLOGE("failed to read eepromBufAddr. result(%zu), *eepromBufSize(%d)", result, *eepromBufSize);

        ret = m_ionFree(eepromBufFd, &eepromBufAddr, *eepromBufSize);
        if (ret != NO_ERROR) {
            CLOGD("m_ionFree(eepromBufFd : %d, eepromBufAddr : %p, eepromBufSize : %d) fail",
                *eepromBufFd,
                eepromBufAddr,
                *eepromBufSize);
            goto done;
        }
        *eepromBufSize = 0;
    }

done:
    if (fp != NULL)
        fclose(fp);

    eepromTimer.stop();

    if (eepromBufAddr) {
        CLOGD("eeprom file(%s), eepromBufAddr(%p), *eepromBufSize(%d), eepromTimer(%d msec)",
            eepromfilePath, eepromBufAddr, *eepromBufSize, (int)eepromTimer.durationUsecs() / 1000);
    } else {
        CLOGE("eeprom file(%s) fail. so, just use dummybuf, eepromTimer(%d msec)",
            eepromfilePath, (int)eepromTimer.durationUsecs() / 1000);

        #define EEPROM_BUF_SIZE (2048)

        *eepromBufSize = EEPROM_BUF_SIZE;

        eepromBufAddr = m_ionAlloc(*eepromBufSize, eepromBufFd);
    }

    return eepromBufAddr;
}

status_t ExynosCameraPPLibRemosaic::m_ionOpen(void)
{
    status_t ret = NO_ERROR;

    if (m_ionClient == 0) {
#ifdef USE_LIB_ION_LEGACY
        m_ionClient = ion_open();
#else
        m_ionClient = exynos_ion_open();
#endif
        if (m_ionClient < 0) {
            CLOGE("ion_open failed");
            ret = BAD_VALUE;
            goto done;
        }
    }
done:
    return ret;
}

void ExynosCameraPPLibRemosaic::m_ionClose(void)
{
    if (m_ionClient != 0)
#ifdef USE_LIB_ION_LEGACY
        ion_close(m_ionClient);
#else
        exynos_ion_close(m_ionClient);
#endif
}

char *ExynosCameraPPLibRemosaic::m_ionAlloc(int size, int *fd)
{
    status_t ret = NO_ERROR;
    int ionFd = 0;
    char *ionAddr = NULL;

    if (m_ionClient == 0) {
        CLOGE("allocator is not yet created");
        ret = INVALID_OPERATION;
        goto done;
    }

    if (size == 0) {
        CLOGE("size equals zero");
        ret = BAD_VALUE;
        goto done;
    }

    // alloc
#ifdef USE_LIB_ION_LEGACY
    ret  = ion_alloc_fd(m_ionClient,
                        size,
                        0,
                        EXYNOS_CAMERA_BUFFER_ION_MASK_CACHED,
                        EXYNOS_CAMERA_BUFFER_ION_FLAG_CACHED,
                        &ionFd);

    if (ret < 0) {
        CLOGE("ion_alloc_fd(fd=%d) failed(%s)", ionFd, strerror(errno));
        ionFd = -1;
        ret = INVALID_OPERATION;
        goto done;
    }
#else
    ionFd  = exynos_ion_alloc(m_ionClient,
                        size,
                        EXYNOS_CAMERA_BUFFER_ION_MASK_CACHED,
                        EXYNOS_CAMERA_BUFFER_ION_FLAG_CACHED);

    if (ionFd < 0) {
        CLOGE("exynos_ion_alloc(fd=%d) failed(%s)", ionFd, strerror(errno));
        ionFd = -1;
        ret = INVALID_OPERATION;
        goto done;
    }

#endif

    // mmap
    ionAddr = (char *)mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, ionFd, 0);
    if (ionAddr == (char *)MAP_FAILED || ionAddr == NULL) {
        CLOGE("ion_map(size=%d) failed, (fd=%d), (%s)", size, *fd, strerror(errno));
        close(ionFd);
        ionAddr = NULL;
        ret = INVALID_OPERATION;
        goto done;
    }

done:
    *fd   = ionFd;

    return ionAddr;
}

status_t ExynosCameraPPLibRemosaic::m_ionFree(int *fd, char **addr, int size)
{
    status_t ret = NO_ERROR;

    if (*fd < 0) {
        CLOGV("*fd(%d) < 0. so, ignore", *fd);
        goto done;
    }

    if (*addr == NULL) {
        CLOGE("*addr == NULL. so, fail");
        ret = BAD_VALUE;
        goto done;
    }

    // munmap
    if (munmap(*addr, size) < 0) {
        CLOGE("munmap( failed");
        ret = INVALID_OPERATION;
        goto done;
    }

    *addr = NULL;

done:
    // close
    if (0 <= *fd) {
#ifdef USE_LIB_ION_LEGACY
        ion_close(*fd);
#else
        exynos_ion_close(*fd);
#endif
    }
    *fd = -1;

    return ret;
}

void ExynosCameraPPLibRemosaic::m_testReadRemosaicBayerFile(ExynosCameraImage image)
{
    FILE *fp = NULL;
    char *filePath = NULL;
    char *fileAddr = NULL;
    int   fileSize = 0;

    size_t result = 0;

    char *dstAddr = image.buf.addr[0];

    if (dstAddr == NULL) {
        CLOGE("dstAddr == NULL. so, fail");
        goto done;
    }

    #define REMOSAIC_TEMP_INPUT_FILE_PATH "/data/media/0/test_wBP_4608x3456_byr0.raw"
    filePath = (char*)REMOSAIC_TEMP_INPUT_FILE_PATH;

    fp = fopen(filePath, "rb");
    if (fp == NULL) {
        CLOGE("fopen(%s) fail", filePath);
        goto done;
    }

    CLOGD("Open(%s) done", filePath);

    fseek(fp, 0, SEEK_END);
    fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (fileSize <= 0) {
        CLOGE("fileSize(%d) < = 0. so, skip fread", fileSize);
        goto done;
    }

    if ((int)image.buf.size[0] < fileSize) {
        CLOGE("image.buf.size[0](%d) is too small than fileSize(%d). please alloc more..", image.buf.size[0], fileSize);
        goto done;
    }

    fileAddr = new char[fileSize];

    // Read complete data
    result = fread(fileAddr, 1, fileSize, fp);
    if((int)result != fileSize) {
        CLOGE("failed to read fileAddr. result(%zu), fileSize(%d)", result, fileSize);

        delete [] fileAddr;
        fileAddr = NULL;

        goto done;
    }

    memcpy(dstAddr, fileAddr, fileSize);

    CLOGD("Read(%s) done, fileSize(%d)", filePath, fileSize);

done:
    if (fileAddr)
        delete [] fileAddr;
    fileAddr = NULL;

    if (fp != NULL)
        fclose(fp);
}

void ExynosCameraPPLibRemosaic::m_testWriteRemosaicBayerFile(ExynosCameraImage srcImage, ExynosCameraImage dstImage)
{
    char filePath[EXYNOS_CAMERA_NAME_STR_SIZE];
    int width  = EXYNOS_CAMERA_PP_LIB_REMOSAIC_W;
    int height = EXYNOS_CAMERA_PP_LIB_REMOSAIC_H;

    int fileSize = getBayerPlaneSize(width, height, V4L2_PIX_FMT_SBGGR16);

    if (srcImage.buf.addr[0] == NULL) {
        CLOGE("srcImage.buf.addr[0] == NULL. so, fail");
        return;
    }

    if (dstImage.buf.addr[0] == NULL) {
        CLOGE("dstImage.buf.addr[0] == NULL. so, fail");
        return;
    }

    // write src buffer
    memset(filePath, 0, sizeof(filePath));
    snprintf(filePath, sizeof(filePath), "/data/media/0/test_src_%dx%d.raw", width, height);

    m_testWrite(filePath, (char *)srcImage.buf.addr[0], fileSize);

    // write dst buffer
    memset(filePath, 0, sizeof(filePath));
    snprintf(filePath, sizeof(filePath), "/data/media/0/test_result_%dx%d.raw", width, height);

    m_testWrite(filePath, (char *)dstImage.buf.addr[0], fileSize);
}

void ExynosCameraPPLibRemosaic::m_testWrite(char *filePath, char *buf, int size)
{
    FILE *fp = NULL;
    size_t result = 0;

    if (buf == NULL) {
        CLOGE("%s's buf == NULL. so fail", filePath);
        goto done;
    }

    fp = fopen(filePath, "w+");
    if (fp == NULL) {
        CLOGE("fopen(%s) fail", filePath);
        goto done;
    }

    CLOGD("Open(%s) done", filePath);

    // Write complete data
    result = fwrite(buf, 1, size, fp);
    if(result <= 0) {
        CLOGE("failed to write buf(%p), size(%d)", buf, size);
        goto done;
    }

    CLOGD("Write(%s) done, size(%d)", filePath, size);

done:
    if (fp != NULL)
        fclose(fp);
}
