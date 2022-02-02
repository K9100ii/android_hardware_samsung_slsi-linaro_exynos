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

#define LOG_TAG "ArcsoftCaliLib"

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

#include <cutils/properties.h>

#include "ArcsoftCaliLib.h"

ArcsoftCaliLib::ArcsoftCaliLib() {
    memset(&m_calData, 0, sizeof(m_calData));
}

void ArcsoftCaliLib::execute(ASVLOFFSCREEN leftImg, ASVLOFFSCREEN rightImg, Map_t *map) {

    int veriEnable = (Data_int32_t)(*map)[PLUGIN_VERI_ENABLE];
    if (veriEnable) {
        ALOGD("DEBUG: verify enable(%d)", veriEnable);
        bool ret = (*map)[PLUGIN_VERI_RESULT] = (Map_data_t)m_doVeri(leftImg, rightImg);
        ALOGD("DEBUG: verify result(%d)", ret);
    }

    int caliEnable = (Data_int32_t)(*map)[PLUGIN_CALI_ENABLE];
    if (caliEnable) {
        ALOGD("DEBUG: calibration enable(%d)", caliEnable);
        bool ret = (*map)[PLUGIN_CALI_RESULT] = (Map_data_t)m_doCali(leftImg, rightImg);
        ALOGD("DEBUG: calibration result(%d)", ret);
    }
}

bool ArcsoftCaliLib::m_doVeri(ASVLOFFSCREEN leftImg, ASVLOFFSCREEN rightImg) {
    MRESULT ret = -1;
    MHandle mHandle;
    ARC_DCVF_INITPARAM mParam;

    mParam.i32ChessboardWidth = 19;
    mParam.i32ChessboardHeight = 14;
    ALOGD("DEBUG: chessboard width %d, height %d", mParam.i32ChessboardWidth, mParam.i32ChessboardHeight);

    char prop[70];
    MDouble avg_error = 0.0;
    MDouble max_error = 0.0;
    MDouble error_range = 0.0;
    MDouble mVerifyAvgError = 0.0, mVerifyMaxError = 0.0, mErrorRange = 0.0;
    bool mVerifResult = false, mRetryVerify = false, mSaveImg = false;

    memset(prop,0,sizeof(prop));
    property_get("persist.dualCam.verify.avgerror", prop, "7.0");
    avg_error = atof(prop);
    memset(prop,0,sizeof(prop));
    property_get("persist.dualCam.verify.maxerror", prop, "10.0");
    max_error = atof(prop);
    ALOGD("DEBUG: avg_error %0.6f, max_error %0.6f", avg_error, max_error);
    memset(prop,0,sizeof(prop));
    property_get("persist.dualCam.verify.errrange", prop, "11.0");
    error_range = atof(prop);
    ALOGD("DEBUG: avg_error %0.6f, max_error %0.6f, error_range %0.6f", avg_error, max_error, error_range);
    memset(prop,0,sizeof(prop));
    property_get("persist.dualCam.verify.retry_file", prop, "0");
    if (atoi(prop)) {
        mRetryVerify = true;
    }
    ALOGD("DEBUG: retry %d", mRetryVerify);
    memset(prop,0,sizeof(prop));
    property_get("persist.dualCam.verify.save_img", prop, "0");
    if (atoi(prop)) {
        mSaveImg = true;
    }

    if (avg_error != 0.0) {
        mVerifyAvgError = avg_error;
    } else {
        mVerifyAvgError = 7.0;
    }

    if (max_error != 0.0) {
        mVerifyMaxError = max_error;
    } else {
        mVerifyMaxError = 10.0;
    }

    if (error_range != 0.0) {
        mErrorRange = error_range;
    } else {
        mErrorRange = 11.0;
    }

    if (leftImg.ppu8Plane[0] != NULL && rightImg.ppu8Plane[0] != NULL) {
        ret = MOK;
    } else {
        return false;
    }

    if (ret == MOK) {
        ret = ARC_DCVF_Init(&mHandle, &mParam);
    }

    if (ret == MOK) {
        ARC_DCVF_RESULT mResult;
        ssize_t read_bytes = 0;
        bool m_flagCalDataLoaded = false;

        if (mRetryVerify == true) {
            FILE * fp = fopen(ARCSOFT_CALI_DATA_PATH, "r");
            if (fp) {
                memset(m_calData, 0, sizeof(m_calData));
                int rc = fread(m_calData, sizeof(MByte), sizeof(m_calData), fp);
                if (rc >= 0) {
                    m_flagCalDataLoaded = true;
                    ALOGD("DEBUG: read file calibration data bytes = %ld", sizeof(m_calData) / sizeof (m_calData[0]));
                }
                fclose(fp);
            }
        }

        if (m_flagCalDataLoaded == false) {
            FILE * ofp = fopen(ARCSOFT_RMO_DATA_PATH, "r");
            ALOGD("DEBUG: open: %s fd: %d", ARCSOFT_RMO_DATA_PATH, ofp);
            if (ofp) {
                memset(m_calData, 0, sizeof(m_calData));
                fseek(ofp, ARCSOFT_RMO_DATA_SEEK, SEEK_SET);
                int rc = fread(m_calData, sizeof(MByte), sizeof(m_calData), ofp);
                if (rc >= 0) {
                    ALOGD("DEBUG: read otp calibration data bytes = %ld", sizeof(m_calData) / sizeof (m_calData[0]));
                }
                fclose(ofp);
            }
        }

        if (mSaveImg == true) {
            FILE * fp = fopen(RMO_CALI_DATA_PATH, "wb");
            ALOGD("DEBUG: open: %s fd: %d", RMO_CALI_DATA_PATH, fp);
            if (fp) {
                fwrite(m_calData, ARC_DCCAL_CALDATA_LEN, 1, fp);
                ALOGD("DEBUG: write data to: %s", RMO_CALI_DATA_PATH);
                fclose(fp);
            }
        }

        ALOGD("DEBUG: ARC_DCVF_Process data(%d,%d,%d,%d)", m_calData[0], m_calData[1], m_calData[2], m_calData[3]);

        ret = ARC_DCVF_Process(mHandle, &leftImg, &rightImg, m_calData, &mResult);
        if (ret == MOK) {
            ALOGD("DEBUG: avg = %0.6f max = %0.6f", mResult.AvgError, mResult.MaxError);
            if (mResult.AvgError < mVerifyAvgError && mResult.MaxError < mVerifyMaxError && mResult.ErrorRange < mErrorRange) {
                mVerifResult = true;
            } else {
                ret = -1;
                mVerifResult = false;
            }
            if (mSaveImg == true) {
                dumpYUVtoFile(&leftImg, "sv_left");
                dumpYUVtoFile(&rightImg, "sv_right");
            }
        } else {
            ALOGE("ERROR: rc = %d, avg = %0.6f, max = %0.6f, err_range = %0.6f", ret, mResult.AvgError, mResult.MaxError, mResult.ErrorRange);
            dumpYUVtoFile(&leftImg, "fv_left");
            dumpYUVtoFile(&rightImg, "fv_right");
        }

        ARC_DCVF_Uninit(&mHandle);

        FILE *resultFd = fopen(ARCSOFT_RESULT_DATA_PATH, "w+");
        if (resultFd) {
            fprintf(resultFd, "%s", "max AvgError:");
            fprintf(resultFd, "%0.6f\n", mVerifyAvgError);
            fprintf(resultFd, "%s", "max MaxError:");
            fprintf(resultFd, "%0.6f\n", mVerifyMaxError);
            fprintf(resultFd, "%s", "max ErrorRange:");
            fprintf(resultFd, "%0.6f\n", mErrorRange);

            fprintf(resultFd, "%s", "DualCamVerify_AvgError:");
            fprintf(resultFd, "%0.6f\n", mResult.AvgError);
            fprintf(resultFd, "%s", "DualCamVerify_MaxError:");
            fprintf(resultFd, "%0.6f\n", mResult.MaxError);
            fprintf(resultFd, "%s", "DualCamVerify_ErrorRange:");
            fprintf(resultFd, "%0.6f\n", mResult.ErrorRange);

            if (mVerifResult == true) {
                fprintf(resultFd, "%s", "verify OK\n");
            } else {
                fprintf(resultFd, "%s", "verify FAIL\n");
            }

            fclose(resultFd);
        }
    } else {
        ALOGE("ERROR: rc = %d", ret);
    }

    return ret == MOK;
}

bool ArcsoftCaliLib::m_doCali(ASVLOFFSCREEN leftImg, ASVLOFFSCREEN rightImg) {
    MRESULT ret = -1;
    MHandle mHandle;
    ARC_DCCAL_INITPARAM mParam;

    mParam.dbBlockSize = 20;
    mParam.i32NumberOfImages = 1;
    mParam.i32ChessboardWidth = 19;
    mParam.i32ChessboardHeight = 14;
    mParam.i32NumberOfChessboards = 4;

    ALOGD("DEBUG: chessboard width %d, height %d", mParam.i32ChessboardWidth, mParam.i32ChessboardHeight);

    char prop[70];
    bool mSaveImg = false;

    memset(prop,0,sizeof(prop));
    property_get("persist.dualCam.cali.save_img", prop, "0");
    if (atoi(prop)) {
        mSaveImg = true;
    }

    if (leftImg.ppu8Plane[0] != NULL && rightImg.ppu8Plane[0] != NULL) {
        ret = MOK;
    } else {
        return false;
    }

    if (ret == MOK) {
        ret = ARC_DCCAL_Init(&mHandle, &mParam);
    }

    if (ret == MOK) {
        MDouble mRetParam;
        memset(m_calData, 0, sizeof(m_calData));

        ret = ARC_DCCAL_Process(mHandle, &leftImg, &rightImg, m_calData);

        ALOGD("DEBUG: ARC_DCCAL_Process data(%d,%d,%d,%d)", m_calData[0], m_calData[1], m_calData[2], m_calData[3]);

        if (ret == MOK) {
            FILE * fp = fopen(ARCSOFT_CALI_DATA_PATH, "wb");
            ALOGD("DEBUG: open: %s fd: %d", ARCSOFT_CALI_DATA_PATH, fp);
            if (fp) {
                fwrite(m_calData, ARC_DCCAL_CALDATA_LEN, 1, fp);
                ALOGD("DEBUG: write data to: %s", ARCSOFT_CALI_DATA_PATH);
                fclose(fp);
            }

            if (mSaveImg == true) {
                dumpYUVtoFile(&leftImg, "sc_left");
                dumpYUVtoFile(&rightImg, "sc_right");
            }

            ret = ARC_DCCAL_GetCalDataElement(mHandle, ARC_DCCAL_RET_ROTATION_X, &mRetParam);
            if (ret == MOK) {
                ALOGD("DEBUG: ARC_DCCAL_RET_ROTATION_X = %0.6f\n", mRetParam);
            }
            ret = ARC_DCCAL_GetCalDataElement(mHandle, ARC_DCCAL_RET_ROTATION_Y, &mRetParam);
            if (ret == MOK) {
                ALOGD("DEBUG: ARC_DCCAL_RET_ROTATION_Y = %0.6f\n", mRetParam);
            }
            ret = ARC_DCCAL_GetCalDataElement(mHandle, ARC_DCCAL_RET_ROTATION_Z, &mRetParam);
            if (ret == MOK) {
                ALOGD("DEBUG: ARC_DCCAL_RET_ROTATION_Z = %0.6f\n", mRetParam);
            }

        }  else {
            ALOGE("ERROR: rc = %d", ret);
            dumpYUVtoFile(&leftImg, "fc_left");
            dumpYUVtoFile(&rightImg, "fc_right");
        }

        ARC_DCCAL_Uninit(&mHandle);

    } else {
        ALOGE("ERROR: rc = %d", ret);

    }

    return ret == MOK;
}

void ArcsoftCaliLib::dumpYUVtoFile(ASVLOFFSCREEN *pAsvl, char* name_prefix)
{
    ALOGD("DEBUG: -IN-");
    char filename[256];
    memset(filename, 0, sizeof(char)*256);

    snprintf(filename, sizeof(filename), "/data/misc/cameraserver/%s_%dx%d.nv21", name_prefix, pAsvl->pi32Pitch[0], pAsvl->i32Height);

    int file_fd = open(filename, O_RDWR | O_CREAT, 0777);
    if (file_fd >= 0)　{
        ssize_t writen_bytes = 0;
        writen_bytes = write(file_fd, pAsvl->ppu8Plane[0], pAsvl->pi32Pitch[0] * pAsvl->i32Height);
        ALOGD("DEBUG: %d writen_bytes %d", pAsvl->pi32Pitch[0] * pAsvl->i32Height, writen_bytes);
        writen_bytes = write(file_fd, pAsvl->ppu8Plane[1], pAsvl->pi32Pitch[1] * (pAsvl->i32Height >> 1));
        ALOGD("DEBUG: %d writen_bytes %d", pAsvl->pi32Pitch[1] * (pAsvl->i32Height >> 1), writen_bytes);
        close(file_fd);
    }

    ALOGD("DEBUG: -OUT-");
}
