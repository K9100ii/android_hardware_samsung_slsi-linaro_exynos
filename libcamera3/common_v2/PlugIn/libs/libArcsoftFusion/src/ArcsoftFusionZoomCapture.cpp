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

#define LOG_TAG "ArcsoftFusionZoomCapture"

#include <fcntl.h>
#include <dlfcn.h>
#include <cutils/properties.h>
#include "ArcsoftFusionZoomCapture.h"

extern "C" long long GetCurrentTime();

#define MODE_BYPASS   1
#define MODE_SYNC     2
#define MODE_SWITCH   3

MHandle ArcsoftFusionZoomCapture::m_handle = MNull;
Mutex   ArcsoftFusionZoomCapture::m_handleLock;

ArcsoftFusionZoomCapture::ArcsoftFusionZoomCapture()
{
    ALOGD("DEBUG(%s[%d]):new ArcsoftFusionZoomCapture object allocated", __FUNCTION__, __LINE__);
}

ArcsoftFusionZoomCapture::~ArcsoftFusionZoomCapture()
{
    ALOGD("DEBUG(%s[%d]):new ArcsoftFusionZoomCapture object deallocated", __FUNCTION__, __LINE__);
}

status_t ArcsoftFusionZoomCapture::m_create(void)
{
    status_t ret = NO_ERROR;

    ALOGD("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    return ret;
}

status_t ArcsoftFusionZoomCapture::m_destroy(void)
{
    status_t ret = NO_ERROR;

    ALOGD("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    return ret;
}

status_t ArcsoftFusionZoomCapture::m_execute(Map_t *map)
{
    status_t ret = NO_ERROR;
    MRESULT retVal = MOK;
    MUInt32 format = ASVL_PAF_NV21;
    ASVLOFFSCREEN wideImg, teleImg, dstImg;
    ARC_DCOZ_IMAGEFD wideImgFd, teleImgFd, dstImgFd;
    ARC_DCIOZ_RATIOPARAM ratioParam;

    memset(&wideImg, 0, sizeof(ASVLOFFSCREEN));
    memset(&teleImg, 0, sizeof(ASVLOFFSCREEN));
    memset(&dstImg, 0, sizeof(ASVLOFFSCREEN));
    memset(&wideImgFd, 0, sizeof(ARC_DCOZ_IMAGEFD));
    memset(&teleImgFd, 0, sizeof(ARC_DCOZ_IMAGEFD));
    memset(&dstImgFd, 0, sizeof(ARC_DCOZ_IMAGEFD));
    memset(&ratioParam, 0, sizeof(ARC_DCIOZ_RATIOPARAM));

    //for dump
    long long timestamp = GetCurrentTime();
    char szRatio[256];
    char prop[100];
    memset(prop, 0, sizeof(prop));
    property_get("persist.camera.arcfsn.dumpimg", prop, "0");
    int dumpimg = atoi(prop);

    ret = m_initLib(map);
    if (ret != NO_ERROR) {
        ALOGE("ERR(%s[%d]):m_initLib() fail", __FUNCTION__, __LINE__);
        goto EXIT;
    }

    ret = m_setImageSize(map);
    if (ret != NO_ERROR) {
        ALOGE("ERR(%s[%d]):m_setImageSize() fail", __FUNCTION__, __LINE__);
        goto EXIT;
    }

    ALOGD("DEBUG(%s[%d]) : zoom capture parameters m_mapSrcBufCnt = %d, m_syncType = %d",
        __FUNCTION__, __LINE__,
        m_mapSrcBufCnt, m_syncType);

    ALOGD("DEBUG(%s[%d]) : zoom capture parameters m_mapSrcBufAddr[0][0] = %p, m_mapSrcBufAddr[1][0] = %p",
        __FUNCTION__, __LINE__,
        m_mapSrcBufAddr[0], m_mapSrcBufAddr[1]);
    switch (m_syncType) {
        case MODE_BYPASS: //bypass mode
        {
            // wide
            if (m_mapSrcBufAddr[0] && m_mapSrcRect[0][PLUGIN_ARRAY_RECT_W] && m_mapSrcRect[0][PLUGIN_ARRAY_RECT_H]) {
                wideImg.u32PixelArrayFormat = format;
                wideImg.i32Width     = m_mapSrcRect[0][PLUGIN_ARRAY_RECT_W];
                wideImg.i32Height     = m_mapSrcRect[0][PLUGIN_ARRAY_RECT_H];
                wideImg.ppu8Plane[0] = (MUInt8 *)m_mapSrcBufAddr[0][0];
                wideImg.ppu8Plane[1] = wideImg.ppu8Plane[0] + (wideImg.i32Width * wideImg.i32Height);
                wideImg.pi32Pitch[0] = m_mapSrcBufWStride[0];
                wideImg.pi32Pitch[1] = m_mapSrcBufWStride[0];

                wideImgFd.i32FdPlane1 = m_mapSrcBufFd[0][0];
                wideImgFd.i32FdPlane2 = m_mapSrcBufFd[0][1];
            }
        }
        break;
        case MODE_SYNC: //sync mode
        {
            // wide
            if (m_mapSrcBufAddr[0] && m_mapSrcRect[0][PLUGIN_ARRAY_RECT_W] && m_mapSrcRect[0][PLUGIN_ARRAY_RECT_H]) {
                wideImg.u32PixelArrayFormat = format;
                wideImg.i32Width     = m_mapSrcRect[0][PLUGIN_ARRAY_RECT_W];
                wideImg.i32Height     = m_mapSrcRect[0][PLUGIN_ARRAY_RECT_H];
                wideImg.ppu8Plane[0] = (MUInt8 *)m_mapSrcBufAddr[0][0];
                wideImg.ppu8Plane[1] = wideImg.ppu8Plane[0] + (wideImg.i32Width * wideImg.i32Height);
                wideImg.pi32Pitch[0] = m_mapSrcBufWStride[0];
                wideImg.pi32Pitch[1] = m_mapSrcBufWStride[0];

                wideImgFd.i32FdPlane1 = m_mapSrcBufFd[0][0];
                wideImgFd.i32FdPlane2 = m_mapSrcBufFd[0][1];
            }
            // tele
            if (m_mapSrcBufAddr[1] && m_mapSrcRect[1][PLUGIN_ARRAY_RECT_W] && m_mapSrcRect[1][PLUGIN_ARRAY_RECT_H]) {
                teleImg.u32PixelArrayFormat = format;
                teleImg.i32Width     = m_mapSrcRect[1][PLUGIN_ARRAY_RECT_W];
                teleImg.i32Height     = m_mapSrcRect[1][PLUGIN_ARRAY_RECT_H];
                teleImg.ppu8Plane[0] = (MUInt8 *)m_mapSrcBufAddr[1][0];
                teleImg.ppu8Plane[1] = teleImg.ppu8Plane[0] + (teleImg.i32Width * teleImg.i32Height);
                teleImg.pi32Pitch[0] = m_mapSrcBufWStride[1];
                teleImg.pi32Pitch[1] = m_mapSrcBufWStride[1];

                teleImgFd.i32FdPlane1 = m_mapSrcBufFd[1][0];
                teleImgFd.i32FdPlane2 = m_mapSrcBufFd[1][1];
            }
        }
        break;
        case MODE_SWITCH: //switch mode
        {
            // tele
            if (m_mapSrcBufAddr[0] && m_mapSrcRect[0][PLUGIN_ARRAY_RECT_W] && m_mapSrcRect[0][PLUGIN_ARRAY_RECT_H]) {
                teleImg.u32PixelArrayFormat = format;
                teleImg.i32Width     = m_mapSrcRect[0][PLUGIN_ARRAY_RECT_W];
                teleImg.i32Height     = m_mapSrcRect[0][PLUGIN_ARRAY_RECT_H];
                teleImg.ppu8Plane[0] = (MUInt8 *)m_mapSrcBufAddr[0][0];
                teleImg.ppu8Plane[1] = teleImg.ppu8Plane[0] + (teleImg.i32Width * teleImg.i32Height);
                teleImg.pi32Pitch[0] = m_mapSrcBufWStride[0];
                teleImg.pi32Pitch[1] = m_mapSrcBufWStride[0];

                teleImgFd.i32FdPlane1 = m_mapSrcBufFd[0][0];
                teleImgFd.i32FdPlane2 = m_mapSrcBufFd[0][1];
            }
        }
        break;
        default:
        break;
    }

    // dst
    dstImg.u32PixelArrayFormat = format;
    dstImg.i32Width     = m_mapDstRect[0][PLUGIN_ARRAY_RECT_W];
    dstImg.i32Height    = m_mapDstRect[0][PLUGIN_ARRAY_RECT_H];
    dstImg.ppu8Plane[0] = (MUInt8*)m_mapDstBufAddr[0];
    dstImg.ppu8Plane[1] = (MUInt8*)m_mapDstBufAddr[0] + dstImg.i32Width * dstImg.i32Height;
    dstImg.pi32Pitch[0] = m_mapDstBufWStride[0];
    dstImg.pi32Pitch[1] = m_mapDstBufWStride[0];

    dstImgFd.i32FdPlane1 = m_mapDstBufFd[0][0];
    dstImgFd.i32FdPlane2 = m_mapDstBufFd[0][1];

    ALOGD("DEBUG(%s[%d]) : zoom capture parameters wide image size:(%d, %d), pitch(%d, %d), plane(%p, %p), fd(%d, %d)",
        __FUNCTION__, __LINE__,
        wideImg.i32Width, wideImg.i32Height,
        wideImg.pi32Pitch[0], wideImg.pi32Pitch[1],
        wideImg.ppu8Plane[0], wideImg.ppu8Plane[1],
        wideImgFd.i32FdPlane1, wideImgFd.i32FdPlane2);

    ALOGD("DEBUG(%s[%d]) : zoom capture parameters tele image size:(%d, %d), pitch(%d, %d), plane(%p, %p), fd(%d, %d)",
        __FUNCTION__, __LINE__,
        teleImg.i32Width, teleImg.i32Height,
        teleImg.pi32Pitch[0], teleImg.pi32Pitch[1],
        teleImg.ppu8Plane[0], teleImg.ppu8Plane[1],
        teleImgFd.i32FdPlane1, teleImgFd.i32FdPlane2);

    ALOGD("DEBUG(%s[%d]) : zoom capture parameters dst image size:(%d, %d), pitch(%d, %d), plane(%p, %p), fd(%d, %d)",
        __FUNCTION__, __LINE__,
        dstImg.i32Width, dstImg.i32Height,
        dstImg.pi32Pitch[0], dstImg.pi32Pitch[1],
        dstImg.ppu8Plane[0], dstImg.ppu8Plane[1],
        dstImgFd.i32FdPlane1, dstImgFd.i32FdPlane2);

    // other setting
    ratioParam.fViewRatio = ((Data_float_t)(*map)[PLUGIN_VIEW_RATIO] / 1000.0f);     // this value is updatd by ArcsoftFusionZoomPreview::getFrameParams()'s m_viewRatio
    ratioParam.fWideImageRatio = ((Pointer_float_t)(*map)[PLUGIN_IMAGE_RATIO])[0];
    ratioParam.fTeleImageRatio = ((Pointer_float_t)(*map)[PLUGIN_IMAGE_RATIO])[1];

    m_cameraIndex = (Data_int32_t)(*map)[PLUGIN_CAMERA_INDEX];
    m_imageShiftX = (Data_int32_t)(*map)[PLUGIN_IMAGE_SHIFT_X];
    m_imageShiftY = (Data_int32_t)(*map)[PLUGIN_IMAGE_SHIFT_Y];

    if (dumpimg)
    {
        if (wideImg.ppu8Plane[0]) {
            memset(szRatio, 0x0, sizeof(char)*256);
            sprintf(szRatio,"zoom_cpture_b_%lld_wide_%.2f_%.2f",
                timestamp, ratioParam.fViewRatio, ratioParam.fWideImageRatio);
            dumpYUVtoFile(&wideImg, szRatio);
        }
        if (teleImg.ppu8Plane[0]) {
            memset(szRatio, 0x0, sizeof(char)*256);
            sprintf(szRatio,"zoom_cpture_b_%lld_tele_%.2f_%.2f",
                timestamp, ratioParam.fViewRatio, ratioParam.fTeleImageRatio);
            dumpYUVtoFile(&teleImg, szRatio);
        }
    }

    {
        int  wideCropSizeW = (Data_int32_t)(*map)[PLUGIN_WIDE_FULLSIZE_W];
        int  wideCropSizeH = (Data_int32_t)(*map)[PLUGIN_WIDE_FULLSIZE_H];
        int  teleCropSizeW = (Data_int32_t)(*map)[PLUGIN_TELE_FULLSIZE_W];
        int  teleCropSizeH = (Data_int32_t)(*map)[PLUGIN_TELE_FULLSIZE_H];

        ret = m_getInputCropROI(&wideImg, &wideImgFd,
                                &teleImg, &teleImgFd,
                                &dstImg, &dstImgFd,
                                &ratioParam,
                                wideCropSizeW, wideCropSizeH,
                                teleCropSizeW, teleCropSizeH);
        if (ret != NO_ERROR) {
            ALOGE("ERR(%s[%d]):m_getInputCropROI() fail", __FUNCTION__, __LINE__);
            goto EXIT;
        }
    }

    if (dumpimg)
    {
        if (wideImg.ppu8Plane[0]) {
            memset(szRatio, 0x0, sizeof(char)*256);
            sprintf(szRatio,"zoom_cpture_a_%lld_wide_%.2f_%.2f",
                timestamp, ratioParam.fViewRatio, ratioParam.fWideImageRatio);
            dumpYUVtoFile(&wideImg, szRatio);
        }
        if (wideImg.ppu8Plane[0]) {
            memset(szRatio, 0x0, sizeof(char)*256);
            sprintf(szRatio,"zoom_cpture_a_%lld_tele_%.2f_%.2f",
                timestamp, ratioParam.fViewRatio, ratioParam.fTeleImageRatio);
            dumpYUVtoFile(&teleImg, szRatio);
        }
    }

    ALOGD("DEBUG(%s[%d]) : zoom capture parameters wide image size:(%d, %d), pitch(%d, %d), plane(%p, %p), fd(%d, %d)",
        __FUNCTION__, __LINE__,
        wideImg.i32Width, wideImg.i32Height,
        wideImg.pi32Pitch[0], wideImg.pi32Pitch[1],
        wideImg.ppu8Plane[0], wideImg.ppu8Plane[1],
        wideImgFd.i32FdPlane1, wideImgFd.i32FdPlane2);

    ALOGD("DEBUG(%s[%d]) : zoom capture parameters tele image size:(%d, %d), pitch(%d, %d), plane(%p, %p), fd(%d, %d)",
        __FUNCTION__, __LINE__,
        teleImg.i32Width, teleImg.i32Height,
        teleImg.pi32Pitch[0], teleImg.pi32Pitch[1],
        teleImg.ppu8Plane[0], teleImg.ppu8Plane[1],
        teleImgFd.i32FdPlane1, teleImgFd.i32FdPlane2);

    ALOGD("DEBUG(%s[%d]) : zoom capture parameters ratioParam(%f, %f, %f)",
        __FUNCTION__, __LINE__,
        ratioParam.fViewRatio, ratioParam.fWideImageRatio, ratioParam.fTeleImageRatio);

    switch (m_syncType) {
        case MODE_BYPASS:
        {
            // no need to copy here, already m_drawScaler wide to dst in m_getInputCropROI
            /*
            ALOGD("DEBUG(%s[%d]):ArcSoft zoom capture process return wide buffer", __FUNCTION__, __LINE__);
            memcpy(dstImg.ppu8Plane[0], wideImg.ppu8Plane[0], sizeof(MByte)*dstImg.pi32Pitch[0]* dstImg.i32Height);
            memcpy(dstImg.ppu8Plane[1], wideImg.ppu8Plane[1], sizeof(MByte)*dstImg.pi32Pitch[1]* dstImg.i32Height >> 1);
            */
        }
        break;
        case MODE_SYNC:
        {
            ARC_DCIOZ_CAMERAPARAM  cameraParam;
            memset(&cameraParam, 0, sizeof(ARC_DCIOZ_CAMERAPARAM));

            Array_buf_t iso = (Array_buf_t)(*map)[PLUGIN_ISO_LIST];
            cameraParam.ISOWide = iso[0];
            cameraParam.ISOTele = iso[1];

            cameraParam.WideFocusFlag = m_mapSrcAfStatus[0];
            cameraParam.TeleFocusFlag = m_mapSrcAfStatus[1];

            if ((wideImg.i32Width * 9) == (wideImg.i32Height * 16)) {
                cameraParam.OriginalWideROI.i32Left = 0;
                cameraParam.OriginalWideROI.i32Top = (wideImg.i32Width * 3 / 4 - wideImg.i32Height) / 2;
                cameraParam.OriginalWideROI.i32Right = wideImg.i32Width;
                cameraParam.OriginalWideROI.i32Bottom = cameraParam.OriginalWideROI.i32Top + wideImg.i32Height;
            } else {
                cameraParam.OriginalWideROI.i32Left = 0;
                cameraParam.OriginalWideROI.i32Top = 0;
                cameraParam.OriginalWideROI.i32Right = wideImg.i32Width;
                cameraParam.OriginalWideROI.i32Bottom = wideImg.i32Height;
            }
            ALOGD("DEBUG(%s[%d]):ArcSoft zoom capture cameraParam OriginalWideROI (%d,%d,%d,%d)",
                __FUNCTION__, __LINE__,
                cameraParam.OriginalWideROI.i32Left, cameraParam.OriginalWideROI.i32Top,
                cameraParam.OriginalWideROI.i32Right, cameraParam.OriginalWideROI.i32Bottom);

            ALOGD("DEBUG(%s[%d]):ArcSoft zoom capture process in", __FUNCTION__, __LINE__);
            retVal = ARC_DCIOZ_Process(
                m_handle,
                &wideImg,
                &teleImg,
                &ratioParam,
                &cameraParam,
                &dstImg,
                &wideImgFd,
                &teleImgFd,
                &dstImgFd
                );
            ALOGD("DEBUG(%s[%d]):ArcSoft zoom capture process out", __FUNCTION__, __LINE__);
            if (retVal != MOK) {
                ALOGE("ERR(%s[%d]):ARC_DCIOZ_Process() fail", __FUNCTION__, __LINE__);
                goto EXIT;
            }
        }
        break;
        case MODE_SWITCH:
        {
            ALOGD("DEBUG(%s[%d]):ArcSoft zoom capture process return tele buffer", __FUNCTION__, __LINE__);
            memcpy(dstImg.ppu8Plane[0], teleImg.ppu8Plane[0], sizeof(MByte)*dstImg.pi32Pitch[0]* dstImg.i32Height);
            memcpy(dstImg.ppu8Plane[1], teleImg.ppu8Plane[1], sizeof(MByte)*dstImg.pi32Pitch[0]* dstImg.i32Height >> 1);
        }
        break;
        default:
        break;
    }

    if (dumpimg)
    {
        memset(szRatio, 0x0, sizeof(char)*256);
        sprintf(szRatio,"zoom_cpture_a_%lld_dst_%.2f_%.2f",
            timestamp, ratioParam.fViewRatio, ratioParam.fWideImageRatio);
        dumpYUVtoFile(&dstImg, szRatio);
    }

EXIT:
    ret = m_uninitLib();
    if (ret != NO_ERROR) {
        ALOGE("ERR(%s[%d]):m_getInputCropROI() fail", __FUNCTION__, __LINE__);
    }

    return ret;
}

status_t ArcsoftFusionZoomCapture::m_init(Map_t *map)
{
    status_t ret = NO_ERROR;

    return ret;
}

status_t ArcsoftFusionZoomCapture::m_query(Map_t *map)
{
    status_t ret = NO_ERROR;

    return ret;
}

status_t ArcsoftFusionZoomCapture::m_initLib(Map_t *map)
{
    Mutex::Autolock lock(m_handleLock);

    status_t ret = NO_ERROR;
    MRESULT retVal = MOK;

    ARC_DCOZ_INITPARAM initParam;
    m_init_ARC_DCOZ_INITPARAM(&initParam);

    if (m_handle == NULL) {
        ALOGD("DEBUG(%s[%d]):ARC_DCIOZ_Init cali data %p, %d", __FUNCTION__, __LINE__, initParam.pCalData, initParam.i32CalDataLen);

        retVal = ARC_DCIOZ_Init(&m_handle, &initParam);
        if (retVal != MOK) {
            ALOGE("ERR(%s[%d]):ARC_DCIOZ_Init() fail ret = %d", __FUNCTION__, __LINE__, retVal);
            ret = INVALID_OPERATION;
        }
    }

    return ret;
}

status_t ArcsoftFusionZoomCapture::m_uninitLib(void)
{
    Mutex::Autolock lock(m_handleLock);

    status_t ret = NO_ERROR;
    MRESULT retVal = MOK;

    if(m_handle != NULL) {
        ALOGD("DEBUG(%s[%d]):ARC_DCIOZ_Uninit", __FUNCTION__, __LINE__);

        retVal = ARC_DCIOZ_Uninit(&m_handle);
        if(retVal != MOK) {
            ALOGE("ERR(%s[%d]):ARC_DCIOZ_Uninit() fail ret = %d", __FUNCTION__, __LINE__, retVal);
            ret = INVALID_OPERATION;
        } else {
            m_handle = NULL;
        }
    }

    return NO_ERROR;
}

status_t ArcsoftFusionZoomCapture::m_setImageSize(Map_t *map)
{
    status_t ret = NO_ERROR;
    MRESULT retVal;

    ARC_DCIOZ_CAMERAIMAGEINFO cameraImageInfo;
    memset(&cameraImageInfo, 0, sizeof(ARC_DCIOZ_CAMERAIMAGEINFO));

    MInt32    i32Mode = ARC_DCOZ_MODE_C;

    //suppose image size is same with crop size
    if (m_mapSrcRect[0][PLUGIN_ARRAY_RECT_W] != 0 && m_mapSrcRect[0][PLUGIN_ARRAY_RECT_H] != 0) {
        cameraImageInfo.i32WideFullWidth = m_mapSrcRect[0][PLUGIN_ARRAY_RECT_W];
        cameraImageInfo.i32WideFullHeight = m_mapSrcRect[0][PLUGIN_ARRAY_RECT_H];
        cameraImageInfo.i32TeleFullWidth = m_mapSrcRect[0][PLUGIN_ARRAY_RECT_W];
        cameraImageInfo.i32TeleFullHeight = m_mapSrcRect[0][PLUGIN_ARRAY_RECT_H];
    } else if (m_mapSrcRect[1][PLUGIN_ARRAY_RECT_W] != 0 && m_mapSrcRect[1][PLUGIN_ARRAY_RECT_H] != 0) {
        cameraImageInfo.i32WideFullWidth = m_mapSrcRect[1][PLUGIN_ARRAY_RECT_W];
        cameraImageInfo.i32WideFullHeight = m_mapSrcRect[1][PLUGIN_ARRAY_RECT_H];
        cameraImageInfo.i32TeleFullWidth = m_mapSrcRect[1][PLUGIN_ARRAY_RECT_W];
        cameraImageInfo.i32TeleFullHeight = m_mapSrcRect[1][PLUGIN_ARRAY_RECT_H];
    }
    //add for burst capture mode resolution 2000 x 1504 /2000 x 1126;
    if (cameraImageInfo.i32WideFullWidth < ARCSOFT_FUSION_EMULATOR_WIDE_WIDTH) {
        if( (cameraImageInfo.i32WideFullWidth / 16) == (cameraImageInfo.i32WideFullHeight / 9)) {
            //16:9
            cameraImageInfo.i32WideFullWidth = ARCSOFT_FUSION_EMULATOR_WIDE_WIDTH;
            cameraImageInfo.i32WideFullHeight = ARCSOFT_FUSION_EMULATOR_WIDE_HEIGHT * 3 / 4;
            cameraImageInfo.i32TeleFullWidth = ARCSOFT_FUSION_EMULATOR_TELE_WIDTH;
            cameraImageInfo.i32TeleFullHeight =  ARCSOFT_FUSION_EMULATOR_TELE_HEIGHT * 3 / 4;
        } else {//4:3
            cameraImageInfo.i32WideFullWidth = ARCSOFT_FUSION_EMULATOR_WIDE_WIDTH;
            cameraImageInfo.i32WideFullHeight = ARCSOFT_FUSION_EMULATOR_WIDE_HEIGHT;
            cameraImageInfo.i32TeleFullWidth = ARCSOFT_FUSION_EMULATOR_TELE_WIDTH;
            cameraImageInfo.i32TeleFullHeight = ARCSOFT_FUSION_EMULATOR_TELE_HEIGHT;
        }
    }
    ALOGD("DEBUG(%s[%d]):ARC_DCIOZ_SetImageSize(wide(%d x %d) / tele(%d x %d)",
        __FUNCTION__, __LINE__,
        cameraImageInfo.i32WideFullWidth,
        cameraImageInfo.i32WideFullHeight,
        cameraImageInfo.i32TeleFullWidth,
        cameraImageInfo.i32TeleFullHeight);

    retVal = ARC_DCIOZ_SetImageSize(
        m_handle,
        &cameraImageInfo,
        i32Mode);
    if (retVal != MOK) {
        ALOGE("ERR(%s[%d]):ARC_DCIOZ_SetImageSize(wide(%d x %d) / tele(%d x %d) fail ret = %d",
            __FUNCTION__, __LINE__,
            cameraImageInfo.i32WideFullWidth,
            cameraImageInfo.i32WideFullHeight,
            cameraImageInfo.i32TeleFullWidth,
            cameraImageInfo.i32TeleFullHeight,
            retVal);
        return INVALID_OPERATION;
    }

    return ret;
}

status_t ArcsoftFusionZoomCapture::m_getInputCropROI(ASVLOFFSCREEN *pWideImg, ARC_DCOZ_IMAGEFD *pWideImgFd,
                                                     ASVLOFFSCREEN *pTeleImg, ARC_DCOZ_IMAGEFD *pTeleImgFd,
                                                     ASVLOFFSCREEN *pDstImg, ARC_DCOZ_IMAGEFD *pDstImgFd,
                                                     ARC_DCIOZ_RATIOPARAM *pRatioParam,
                                                     int wideCropSizeWidth, int wideCropSizeHeight,
                                                     int teleCropSizeWidth, int teleCropSizeHeight)
{
    status_t ret = NO_ERROR;
    MRESULT retVal;

    MRECT fWideCropROI;
    MRECT fTeleCropROI;

    MFloat orinalFWideImageRatio = pRatioParam->fWideImageRatio;
    MFloat orinalFTeleImageRatio = pRatioParam->fTeleImageRatio;

    MInt32 wideBayerCropLeft = 0;
    MInt32 wideBayerCropTop = 0;
    MInt32 teleBayerCropTop = 0;
    MInt32 teleBayerCropLeft = 0;

    ALOGD("DEBUG(%s[%d]):WIDE image size(%d, %d) pitch(%d, %d) plane(%p, %p)",
        __FUNCTION__, __LINE__,
        pWideImg->i32Width, pWideImg->i32Height,
        pWideImg->pi32Pitch[0], pWideImg->pi32Pitch[1],
        pWideImg->ppu8Plane[0], pWideImg->ppu8Plane[1]);

    ALOGD("DEBUG(%s[%d]):TELE image size(%d, %d) pitch(%d, %d) plane(%p, %p)",
        __FUNCTION__, __LINE__,
        pTeleImg->i32Width, pTeleImg->i32Height,
        pTeleImg->pi32Pitch[0], pTeleImg->pi32Pitch[1],
        pTeleImg->ppu8Plane[0], pTeleImg->ppu8Plane[1]);

    ALOGD("DEBUG(%s[%d]):RatioParam(%f, %f, %f)",
        __FUNCTION__, __LINE__,
        pRatioParam->fViewRatio, pRatioParam->fWideImageRatio, pRatioParam->fTeleImageRatio);

    ALOGD("DEBUG(%s[%d]):CameraIndex(%d), ImageShift(%d, %d)",
        __FUNCTION__, __LINE__,
        m_cameraIndex, m_imageShiftX, m_imageShiftY);

    retVal = ARC_DCIOZ_GetInputCropROI(
        m_handle,
        pWideImg,
        pTeleImg,
        pRatioParam,
        m_cameraIndex,
        m_imageShiftX,
        m_imageShiftY,
        &fWideCropROI,
        &fTeleCropROI,
        &pRatioParam->fWideImageRatio,
        &pRatioParam->fTeleImageRatio
        );
    if (retVal != MOK) {
        ALOGE("ERR(%s[%d]):ARC_DCIOZ_GetInputCropROI() fail", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    ALOGD("DEBUG(%s[%d]):before transform WideCropROI(%d, %d, %d, %d), TeleCropROI(%d, %d, %d, %d)",
        __FUNCTION__, __LINE__,
        fWideCropROI.left, fWideCropROI.right, fWideCropROI.top, fWideCropROI.bottom,
        fTeleCropROI.left, fTeleCropROI.right, fTeleCropROI.top, fTeleCropROI.bottom);

    if (pWideImg->i32Width && pWideImg->i32Height) {
        int width = fWideCropROI.right - fWideCropROI.left;
        int height = fWideCropROI.bottom - fWideCropROI.top;
        fWideCropROI.left = fWideCropROI.left * pWideImg->i32Width / wideCropSizeWidth;
        fWideCropROI.top = fWideCropROI.top  * pWideImg->i32Height / wideCropSizeHeight;
        fWideCropROI.right = fWideCropROI.left + width * pWideImg->i32Width / wideCropSizeWidth;
        fWideCropROI.bottom = fWideCropROI.top + height * pWideImg->i32Height / wideCropSizeHeight;
    }

    if (pTeleImg->i32Width && pTeleImg->i32Height) {
        int width = fTeleCropROI.right - fTeleCropROI.left;
        int height = fTeleCropROI.bottom - fTeleCropROI.top;
        fTeleCropROI.left = fTeleCropROI.left  * pTeleImg->i32Width / teleCropSizeWidth;
        fTeleCropROI.top = fTeleCropROI.top  * pTeleImg->i32Height / teleCropSizeHeight;
        fTeleCropROI.right = fTeleCropROI.left + width * pTeleImg->i32Width / teleCropSizeWidth;
        fTeleCropROI.bottom = fTeleCropROI.top + height * pTeleImg->i32Height / teleCropSizeHeight;
    }

    ALOGD("DEBUG(%s[%d]):after transform WideCropROI(%d, %d, %d, %d), TeleCropROI(%d, %d, %d, %d)",
        __FUNCTION__, __LINE__,
        fWideCropROI.left, fWideCropROI.right, fWideCropROI.top, fWideCropROI.bottom,
        fTeleCropROI.left, fTeleCropROI.right, fTeleCropROI.top, fTeleCropROI.bottom);

    ALOGD("DEBUG(%s[%d]):WideImageRatio(%f), TeleImageRatio(%f)",
        __FUNCTION__, __LINE__,
        pRatioParam->fWideImageRatio, pRatioParam->fTeleImageRatio);

    switch (m_syncType) {
        case MODE_BYPASS: //bypass mode
        {
            int srcRect[6];
            srcRect[0] = fWideCropROI.left;
            srcRect[1] = fWideCropROI.top;
            srcRect[2] = fWideCropROI.right - fWideCropROI.left;
            srcRect[3] = fWideCropROI.bottom - fWideCropROI.top;
            srcRect[4] = pWideImg->i32Width;
            srcRect[5] = pWideImg->i32Height;

            // dst
            int dstRect[6];
            dstRect[0] = 0;
            dstRect[1] = 0;
            dstRect[2] = pDstImg->i32Width;
            dstRect[3] = pDstImg->i32Height;
            dstRect[4] = pDstImg->i32Width;
            dstRect[5] = pDstImg->i32Height;

            // assume NV21
            int srcBufSize = (srcRect[4] * srcRect[5] * 3) >> 1;
            int dstBufSize = (dstRect[4] * dstRect[5] * 3) >> 1;

            ALOGD("DEBUG(%s[%d]):WIDE update size(%d x %d) fd(%d, %d) -[crop %d, %d, %d, %d, in %d x %d]-> size(%d, %d, %d, %d, in %d x %d) fd(%d, %d)",
                __FUNCTION__, __LINE__,
                pWideImg->i32Width,
                pWideImg->i32Height,
                pWideImgFd->i32FdPlane1,
                pWideImgFd->i32FdPlane2,
                srcRect[0],
                srcRect[1],
                srcRect[2],
                srcRect[3],
                srcRect[4],
                srcRect[5],
                dstRect[0],
                dstRect[1],
                dstRect[2],
                dstRect[3],
                dstRect[4],
                dstRect[5],
                pDstImgFd->i32FdPlane1,
                0);

            ret = m_drawScaler(srcRect, pWideImgFd->i32FdPlane1, srcBufSize,
                           dstRect, pDstImgFd->i32FdPlane1, dstBufSize);
            if (ret != NO_ERROR) {
                ALOGE("ERR(%s[%d]):m_drawScaler() fail", __FUNCTION__, __LINE__);
                // ignore the resize, then, it will use original information.
                //return ret;
                return NO_ERROR;
            }
        }
        break;
        case MODE_SWITCH: //switch mode
        {
            int srcRect[6];
            srcRect[0] = fTeleCropROI.left;
            srcRect[1] = fTeleCropROI.top;
            srcRect[2] = fTeleCropROI.right - fTeleCropROI.left;
            srcRect[3] = fTeleCropROI.bottom - fTeleCropROI.top;
            srcRect[4] = pTeleImg->i32Width;
            srcRect[5] = pTeleImg->i32Height;

            // dst
            int dstRect[6];
            dstRect[0] = 0;
            dstRect[1] = 0;
            dstRect[2] = pDstImg->i32Width;
            dstRect[3] = pDstImg->i32Height;
            dstRect[4] = pDstImg->i32Width;
            dstRect[5] = pDstImg->i32Height;

            // assume NV21
            int srcBufSize = (srcRect[4] * srcRect[5] * 3) >> 1;
            int dstBufSize = (dstRect[4] * dstRect[5] * 3) >> 1;

            ALOGD("DEBUG(%s[%d]):TELE update size(%d x %d) fd(%d, %d) -[crop %d, %d, %d, %d, in %d x %d]-> size(%d, %d, %d, %d, in %d x %d) fd(%d, %d)",
                __FUNCTION__, __LINE__,
                pTeleImg->i32Width,
                pTeleImg->i32Height,
                pTeleImgFd->i32FdPlane1,
                pTeleImgFd->i32FdPlane2,
                srcRect[0],
                srcRect[1],
                srcRect[2],
                srcRect[3],
                srcRect[4],
                srcRect[5],
                dstRect[0],
                dstRect[1],
                dstRect[2],
                dstRect[3],
                dstRect[4],
                dstRect[5],
                pDstImgFd->i32FdPlane1,
                0);

            ret = m_drawScaler(srcRect, pTeleImgFd->i32FdPlane1, srcBufSize,
                           dstRect, pDstImgFd->i32FdPlane1, dstBufSize);
            if (ret != NO_ERROR) {
                ALOGE("ERR(%s[%d]):m_drawScaler() fail", __FUNCTION__, __LINE__);
                // ignore the resize, then, it will use original information.
                //return ret;
                return NO_ERROR;
            }
        }
        break;
        case MODE_SYNC: //sync mode
        { // crop wide image
			int srcRect[6];
            srcRect[0] = fWideCropROI.left;
            srcRect[1] = fWideCropROI.top;
            srcRect[2] = fWideCropROI.right - fWideCropROI.left;
            srcRect[3] = fWideCropROI.bottom - fWideCropROI.top;
            srcRect[4] = pWideImg->i32Width;
            srcRect[5] = pWideImg->i32Height;

            // dst
            int newSrcRect[6];
            newSrcRect[0] = 0;
            newSrcRect[1] = 0;
            newSrcRect[2] = pWideImg->i32Width;
            newSrcRect[3] = pWideImg->i32Height;
            newSrcRect[4] = pWideImg->i32Width;
            newSrcRect[5] = pWideImg->i32Height;

            if (m_tempSrcBufferFd[0] == 0) {
                // assume NV21
                m_tempSrcBufferSize[0] = (newSrcRect[4] * newSrcRect[5] * 3) >> 1;
                m_allocIonBuf(m_tempSrcBufferSize[0], &m_tempSrcBufferFd[0], &m_tempSrcBufferAddr[0], true);
            }

            ALOGD("DEBUG(%s[%d]):WIDE update size(%d x %d) fd(%d, %d) -[crop %d, %d, %d, %d, in %d x %d]-> size(%d, %d, %d, %d, in %d x %d) fd(%d, %d)",
                __FUNCTION__, __LINE__,
                pWideImg->i32Width,
                pWideImg->i32Height,
                pWideImgFd->i32FdPlane1,
                pWideImgFd->i32FdPlane2,
                srcRect[0],
                srcRect[1],
                srcRect[2],
                srcRect[3],
                srcRect[4],
                srcRect[5],
                newSrcRect[0],
                newSrcRect[1],
                newSrcRect[2],
                newSrcRect[3],
                newSrcRect[4],
                newSrcRect[5],
                m_tempSrcBufferFd[0],
               0);

            ret = m_drawScaler(srcRect,    m_mapSrcBufFd[0][0],  m_mapSrcBufSize[0][0],
                           newSrcRect, m_tempSrcBufferFd[0], m_tempSrcBufferSize[0]);
            if (ret != NO_ERROR) {
                ALOGE("ERR(%s[%d]):m_drawScaler() fail", __FUNCTION__, __LINE__);
                // ignore the resize, then, it will use original information.
                //return ret;
                return NO_ERROR;
            }

            // we update here from original src to temp
            pWideImg->i32Width  = newSrcRect[2];
            pWideImg->i32Height = newSrcRect[3];
            pWideImg->ppu8Plane[0] = (MUInt8 *)m_tempSrcBufferAddr[0];
            pWideImg->ppu8Plane[1] = pWideImg->ppu8Plane[0] + (pWideImg->i32Width * pWideImg->i32Height);
            pWideImg->pi32Pitch[0] = newSrcRect[2];
            pWideImg->pi32Pitch[1] = newSrcRect[2];

            pWideImgFd->i32FdPlane1 = m_tempSrcBufferFd[0];
            pWideImgFd->i32FdPlane2 = 0;
        }
        { // crop tele image
            int srcRect[6];
            srcRect[0] = fTeleCropROI.left;
            srcRect[1] = fTeleCropROI.top;
            srcRect[2] = fTeleCropROI.right - fTeleCropROI.left;
            srcRect[3] = fTeleCropROI.bottom - fTeleCropROI.top;
            srcRect[4] = pTeleImg->i32Width;
            srcRect[5] = pTeleImg->i32Height;

            int newSrcRect[6];
            newSrcRect[0] = 0;
            newSrcRect[1] = 0;
            newSrcRect[2] = pTeleImg->i32Width;
            newSrcRect[3] = pTeleImg->i32Height;
            newSrcRect[4] = pTeleImg->i32Width;
            newSrcRect[5] = pTeleImg->i32Height;

            if (m_tempSrcBufferFd[1] == 0) {
                // assume NV21
                m_tempSrcBufferSize[1] = (newSrcRect[4] * newSrcRect[5] * 3) >> 1;
                m_allocIonBuf(m_tempSrcBufferSize[1], &m_tempSrcBufferFd[1], &m_tempSrcBufferAddr[1], true);
            }

            ALOGD("DEBUG(%s[%d]):TELE update size(%d x %d) fd(%d, %d) -[crop %d, %d, %d, %d, in %d x %d]-> size(%d, %d, %d, %d, in %d x %d) fd(%d, %d)",
                __FUNCTION__, __LINE__,
                pTeleImg->i32Width,
                pTeleImg->i32Height,
                pTeleImgFd->i32FdPlane1,
                pTeleImgFd->i32FdPlane2,
                srcRect[0],
                srcRect[1],
                srcRect[2],
                srcRect[3],
                srcRect[4],
                srcRect[5],
                newSrcRect[0],
                newSrcRect[1],
                newSrcRect[2],
                newSrcRect[3],
                newSrcRect[4],
                newSrcRect[5],
                m_tempSrcBufferFd[0],
                0);

            ret = m_drawScaler(srcRect, m_mapSrcBufFd[1][0], m_mapSrcBufSize[1][0],
							   newSrcRect, m_tempSrcBufferFd[1], m_tempSrcBufferSize[1]);
            if (ret != NO_ERROR) {
                ALOGE("ERR(%s[%d]):m_drawScaler() fail", __FUNCTION__, __LINE__);
                // ignore the resize, then, it will use original information.
                //return ret;
                return NO_ERROR;
            }

            // we update here from original src to temp
            pTeleImg->i32Width	= newSrcRect[2];
            pTeleImg->i32Height = newSrcRect[3];
            pTeleImg->ppu8Plane[0] = (MUInt8 *)m_tempSrcBufferAddr[1];
            pTeleImg->ppu8Plane[1] = pTeleImg->ppu8Plane[0] + (pTeleImg->i32Width * pTeleImg->i32Height);
            pTeleImg->pi32Pitch[0] = newSrcRect[2];
            pTeleImg->pi32Pitch[1] = newSrcRect[2];

            pTeleImgFd->i32FdPlane1 = m_tempSrcBufferFd[0];
            pTeleImgFd->i32FdPlane2 = 0;
        }
        break;
        default:
        break;
    }

    ALOGD("DEBUG(%s[%d]):ImageRatio update WIDE(%f -> %f) / TELE(%f -> %f)",
        __FUNCTION__, __LINE__,
        orinalFWideImageRatio,
        orinalFTeleImageRatio,
        pRatioParam->fWideImageRatio,
        pRatioParam->fTeleImageRatio);

    return ret;
}

void ArcsoftFusionZoomCapture::dumpYUVtoFile(ASVLOFFSCREEN *pAsvl, char* name_prefix)
{
    ALOGD("ArcsoftFusionZoomCapture----dumpYUVtoFile E");
    char filename[256];
    memset(filename, 0, sizeof(char)*256);

    snprintf(filename, sizeof(filename), "/data/misc/cameraserver/%s_%dx%d.nv21",
    name_prefix, pAsvl->pi32Pitch[0], pAsvl->i32Height);

    int file_fd = open(filename, O_RDWR | O_CREAT, 0777);
    if (file_fd >= 0)
    {
        //fchmod(file_fd, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        ssize_t writen_bytes = 0;
        writen_bytes = write(file_fd, pAsvl->ppu8Plane[0], pAsvl->pi32Pitch[0] * pAsvl->i32Height);//only for NV21 or NV12
        ALOGD("ArcsoftFusionZoomCapture---- %d writen_bytes %d", pAsvl->pi32Pitch[0] * pAsvl->i32Height, writen_bytes);
        writen_bytes = write(file_fd, pAsvl->ppu8Plane[1], pAsvl->pi32Pitch[1] * (pAsvl->i32Height >> 1));//only for NV21 or NV12
        ALOGD("ArcsoftFusionZoomCapture---- %d writen_bytes %d", pAsvl->pi32Pitch[1] * (pAsvl->i32Height >> 1), writen_bytes);
        ALOGD("DEBUG(%s[%d]):fd_close FD(%d)", __FUNCTION__, __LINE__, file_fd);
        close(file_fd);
    }

    ALOGD("ArcsoftFusionZoomCapture----dump frame to file: %s", filename);
}

#ifdef ARCSOFT_FUSION_WRAPPER

ARC_DCIOZ_API MRESULT ARC_DCIOZ_Init(
    MHandle*             phHandle,
    LPARC_DCOZ_INITPARAM pInitParam
    )
{
    return MOK;
}

ARC_DCIOZ_API MRESULT ARC_DCIOZ_Uninit(MHandle* phHandle)
{
    return MOK;
}

ARC_DCIOZ_API MRESULT ARC_DCIOZ_SetImageSize(
    MHandle        hHandle,
    LPARC_DCIOZ_CAMERAIMAGEINFO    pCameraImageInfo,
    MInt32                i32Mode
    )
{
    return MOK;
}

MRESULT ARC_DCIOZ_GetInputCropROI(
    MHandle hHandle,
    LPASVLOFFSCREEN pWideImg,
    LPASVLOFFSCREEN pTeleImg,
    LPARC_DCIOZ_RATIOPARAM pRatioParam,
    MInt32 CameraIndex,
    MInt32 i32ImageShiftX,
    MInt32 i32ImageShiftY,
    MRECT* pWideCropROI,
    MRECT* pTeleCropROI,
    MFloat* pfWideCropROIRatio,
    MFloat* pfTeleCropROIRatio
    )
{
    return MOK;
}

ARC_DCIOZ_API MRESULT ARC_DCIOZ_Process(
    MHandle                    hHandle,
    LPASVLOFFSCREEN            pWideImg,
    LPASVLOFFSCREEN            pTeleImg,    LPARC_DCIOZ_RATIOPARAM     pRatioParam,
    LPARC_DCIOZ_CAMERAPARAM    pCameraParam,
    LPASVLOFFSCREEN            pDstImg,
    LPARC_DCOZ_IMAGEFD           pFdWideImg,
    LPARC_DCOZ_IMAGEFD           pFdTeleImg,
    LPARC_DCOZ_IMAGEFD           pFdResultImg
    )
{
    return MOK;
}

#endif // ARCSOFT_FUSION_WRAPPER
