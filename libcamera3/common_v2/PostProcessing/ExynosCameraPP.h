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
 * \file      ExynosCameraPP.h
 * \brief     header file for ExynosCameraPP
 * \author    Sangwoo, Park(sw5771.park@samsung.com)
 * \date      2016/10/05
 *
 * <b>Revision History: </b>
 * - 2016/10/05 : Sangwoo, Park(sw5771.park@samsung.com) \n
 *   Initial version
 */

#ifndef EXYNOS_CAMERA_PP_H
#define EXYNOS_CAMERA_PP_H

#include <sys/types.h>
#include <log/log.h>

#include "ExynosCameraCommonInclude.h"
#include "ExynosCameraImage.h"
#include "ExynosCameraUtils.h"
#include "ExynosCameraObject.h"
#include "ExynosCameraParameters.h"

using namespace android;

/*
 * Class ExynosCameraPP
 *
 * This is adjusted "Tempate method pattern"
 */
class ExynosCameraPP : public ExynosCameraObject
{
protected:
    ExynosCameraPP()
    {
        m_init();
    }

    ExynosCameraPP(
            int cameraId,
            ExynosCameraConfigurations *configurations,
            ExynosCameraParameters *parameters,
            int nodeNum)
    {
        m_init();

        m_cameraId = cameraId;
        m_nodeNum  = nodeNum;
        m_parameters = parameters;
        m_configurations = configurations;
    }

public:
    virtual ~ExynosCameraPP();

    // user API
    virtual status_t    create(void) final;
    virtual status_t    extControl(int controlType, void *data) final;
    virtual status_t    destroy(void) final;
    virtual bool        flagCreated(void) final;

    virtual int         getNodeNum(void) final;

    virtual ExynosCameraImageCapacity getSrcImageCapacity(void) final;
    virtual ExynosCameraImageCapacity getDstImageCapacity(void) final;

    virtual int         getNumOfSrcImage(void) final;
    virtual int         getNumOfDstImage(void) final;

    virtual bool        isSupportedSrcImage(const int colorFormat, const int width) final;
    virtual bool        isSupportedDstImage(const int colorFormat, const int width) final;

    virtual status_t    setNextPP(ExynosCameraPP *pp);
    virtual ExynosCameraPP *getNextPP(void);

    virtual status_t    draw(ExynosCameraImage *srcImage,
                            ExynosCameraImage *dstImage,
                            ExynosCameraParameters *params) final;
    virtual status_t    start(void);
    virtual status_t    stop(bool suspendFlag = false);

    virtual void        setFlagStarted(bool flag) final;
    virtual bool        getFlagStarted(void) final;

protected:

    // inherit this function.
    virtual status_t m_create(void);
    virtual status_t m_extControl(int controlType, void *data);
    virtual status_t m_destroy(void);
    virtual status_t m_draw(ExynosCameraImage *srcImage,
                            ExynosCameraImage *dstImage,
                            ExynosCameraParameters *params);

    // help function.
    ExynosCameraPP  *m_getProperPP(ExynosCameraPP *pp,
                                   ExynosCameraImage srcImage,
                                   ExynosCameraImage dstImage);
            void     m_printImage(char *prefix, ExynosCameraImage image, bool flagLogd = false);
            void     m_printImage(char *prefix, int numOfImage, ExynosCameraImage *image, bool flagLogd = false);

private:
            bool     m_flagCreated();
            void     m_init(void);

protected:
    Mutex                       m_lock;

    bool                        m_flagCreate;

    int                         m_nodeNum;

    ExynosCameraImageCapacity   m_srcImageCapacity;
    ExynosCameraImageCapacity   m_dstImageCapacity;

    ExynosCameraPP *            m_nextPP;

    ExynosCameraConfigurations *m_configurations;
    ExynosCameraParameters     *m_parameters;

    bool                        m_flagStarted;
};

#endif //EXYNOS_CAMERA_PP_H
