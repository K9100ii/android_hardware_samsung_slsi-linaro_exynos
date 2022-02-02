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
 * \file      ArcsoftFusionZoomCapture.h
 * \brief     header file for ArcsoftFusionZoomCapture
 * \author    Sangwoo, Park(sw5771.park@samsung.com)
 * \date      2017/11/09
 *
 * <b>Revision History: </b>
 * - 2017/11/0 : Sangwoo, Park(sw5771.park@samsung.com) \n
 *   Initial version
 */

#ifndef ARCSOFT_FUSION_ZOOM_CAPTURE_H
#define ARCSOFT_FUSION_ZOOM_CAPTURE_H

#include <utils/Log.h>

#include "ArcsoftFusion.h"

#include "arcsoft_dualcam_image_optical_zoom.h"

using namespace android;

//! ArcsoftFusionZoomCapture
/*!
 * \ingroup ExynosCamera
 */
class ArcsoftFusionZoomCapture : public ArcsoftFusion {
public:
    //! Constructor
    ArcsoftFusionZoomCapture();
    //! Destructor
    virtual ~ArcsoftFusionZoomCapture();

protected:
    virtual status_t m_create(void);
    virtual status_t m_destroy(void);
    virtual status_t m_execute(Map_t *map);
    virtual status_t m_init(Map_t *map);
    virtual status_t m_query(Map_t *map);

    status_t m_initLib(Map_t *map);
    status_t m_uninitLib(void);
    status_t m_setImageSize(Map_t *map);
    status_t m_getInputCropROI(ASVLOFFSCREEN *pWideImg, ARC_DCOZ_IMAGEFD *pWideImgFd,
                               ASVLOFFSCREEN *pTeleImg, ARC_DCOZ_IMAGEFD *pTeleImgFd,
                               ASVLOFFSCREEN *pDstImg, ARC_DCOZ_IMAGEFD *pDstImgFd,
                               ARC_DCIOZ_RATIOPARAM *pRatioParam,
                               int wideCropSizeWidth, int wideCropSizeHeight,
                               int teleCropSizeWidth, int teleCropSizeHeight);

private:
    void dumpYUVtoFile(ASVLOFFSCREEN *pAsvl, char* name_prefix);

private:
    static MHandle m_handle;
    static Mutex   m_handleLock;
};

#endif //ARCSOFT_FUSION_ZOOM_CAPTURE_H
