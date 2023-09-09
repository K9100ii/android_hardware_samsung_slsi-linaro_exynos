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

/*!
 * \file      ExynosCameraPPLibRemosaic.h
 * \brief     header file for ExynosCameraPPLibRemosaic
 * \author    Sangwoo, Park(sw5771.park@samsung.com)
 * \date      2017/04/24
 *
 * <b>Revision History: </b>
 * - 2017/04/24 : Sangwoo, Park(sw5771.park@samsung.com) \n
 *   Initial version
 */

#ifndef EXYNOS_CAMERA_PP_LIBREMOSAIC_H
#define EXYNOS_CAMERA_PP_LIBREMOSAIC_H

#include <dlfcn.h>
#ifdef USE_LIB_ION_LEGACY
#include <ion/ion.h>
#else
#include <hardware/exynos/ion.h>
#endif //USE_LIB_ION_LEGACY
#include "ExynosCameraPP.h"
#include "remosaic_itf.h"

using namespace android;

/*
 * Class ExynosCameraPPLibRemosaic
 */
class ExynosCameraPPLibRemosaic : public ExynosCameraPP
{
private:
    struct remosaicDL
    {
        void *lib;

        typedef void    (*remosaic_init_t)(int32_t img_w, int32_t img_h,
                                      e_remosaic_bayer_order bayer_order, int32_t pedestal);
        remosaic_init_t remosaic_init;

        typedef int32_t (*remosaic_gainmap_gen_t)(void* eep_buf_addr, size_t eep_buf_size);
        remosaic_gainmap_gen_t remosaic_gainmap_gen;

        typedef void    (*remosaic_process_param_set_t)(struct st_remosaic_param* p_param);
        remosaic_process_param_set_t remosaic_process_param_set;

        typedef int32_t (*remosaic_process_t)(int32_t src_buf_fd, size_t src_buf_size,
                                              int32_t dst_buf_fd, size_t dst_buf_size);
        remosaic_process_t remosaic_process;

        typedef void    (*remosaic_deinit_t)(void);
        remosaic_deinit_t remosaic_deinit;

        typedef void    (*remosaic_debug_t)(int opt);
        remosaic_debug_t remosaic_debug;

        public:
            remosaicDL() {
                lib = NULL;
            }
    };

protected:
    ExynosCameraPPLibRemosaic()
    {
        m_init();
    }

    ExynosCameraPPLibRemosaic(int cameraId, int nodeNum) : ExynosCameraPP(cameraId, nodeNum)
    {
        m_init();

        // we think nodeNum = 333 is "libremosaic"
    }

    /* ExynosCameraPPLibRemosaic's constructor is protected
     * to prevent new without ExynosCameraPPFactory::newPP()
     */
    friend class ExynosCameraPPFactory;

public:
    virtual ~ExynosCameraPPLibRemosaic();

protected:
    virtual status_t m_create(void);
    virtual status_t m_destroy(void);
    virtual status_t m_draw(ExynosCameraImage srcImage, ExynosCameraImage dstImage);
    virtual void     m_setName(void);

private:
            void     m_init(void);
            status_t m_delayedCreate(void);

            status_t m_dlOpen(void);
            status_t m_dlClose(void);
            char *   m_readEEPROM(int *eepromBufSize, int *eepromBufFd);

            status_t m_ionOpen(void);
            void     m_ionClose(void);
            char    *m_ionAlloc(int size, int *fd);
            status_t m_ionFree(int *fd, char **addr, int size);


            void     m_testReadRemosaicBayerFile(ExynosCameraImage image);
            void     m_testWriteRemosaicBayerFile(ExynosCameraImage srcImage, ExynosCameraImage dstImage);
            void     m_testWrite(char *filePath, char *buf, int size);

protected:
    bool               m_flagRemosaicInited;
    bool               m_flagDelayedCreated;

    struct remosaicDL  m_remosaicDL;

    int             m_ionClient;
    int             m_ionEepromBufFd;
    char           *m_ionEepromBufAddr;
    int             m_ionEepromBufSize;
};

#endif //EXYNOS_CAMERA_PP_LIBREMOSAIC_H
