/*
 * Copyright 2015, Samsung Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed toggle an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*!
 * \file      ExynosCameraSFLMgr.cpp
 * \brief     header file for ExynosCameraSFLMgr
 * \author    VijayaKumar S N (vijay.sathenahallin@samsung.com)
 * \date      2015/10/27
 *
 */
#ifndef EXYNOS_CAMERA_SFL_MGR_H__
#define EXYNOS_CAMERA_SFL_MGR_H__

#include "ExynosCameraSFLdefine.h"
#include "ExynosCameraSFLInterface.h"

namespace android {

/* #define EXYNOS_CAMERA_SFLMGR_TRACE */
#ifdef EXYNOS_CAMERA_SFLMGR_TRACE
#define EXYNOS_CAMERA_SFLMGR_TRACE_IN()   CLOGD("DEBUG(%s[%d]):IN.." , __FUNCTION__, __LINE__)
#define EXYNOS_CAMERA_SFLMGR_TRACE_OUT()  CLOGD("DEBUG(%s[%d]):OUT..", __FUNCTION__, __LINE__)
#else
#define EXYNOS_CAMERA_SFLMGR_TRACE_IN()   ((void *)0)
#define EXYNOS_CAMERA_SFLMGR_TRACE_OUT()  ((void *)0)
#endif

class ExynosCameraSFLMgr : public virtual RefBase {
public:
    ExynosCameraSFLMgr(const char* name, int cameraid);
    virtual ~ExynosCameraSFLMgr();
    sp<ExynosCameraSFLInterface> getLibrary(SFLIBRARY_MGR::SFLType type);

    void setStatus(SFLIBRARY_MGR::SFLType type, bool enable);
    bool getStatus(SFLIBRARY_MGR::SFLType type);

    void setType(SFLIBRARY_MGR::SFLType type);
    SFLIBRARY_MGR::SFLType getType();

    void setRunEnable(SFLIBRARY_MGR::SFLType type, bool enable);
    bool getRunEnable(SFLIBRARY_MGR::SFLType type);

private:
    void m_createLibrary(SFLIBRARY_MGR::SFLType type);
    void m_destroyLibrary(SFLIBRARY_MGR::SFLType type);
    sp<ExynosCameraSFLInterface> m_getLibrary(SFLIBRARY_MGR::SFLType type);

    void m_setStatus(SFLIBRARY_MGR::SFLType type, bool enable);
    bool m_getStatus(SFLIBRARY_MGR::SFLType type);

    void m_setType(SFLIBRARY_MGR::SFLType type);
    SFLIBRARY_MGR::SFLType m_getType();

    void m_setRunEnable(SFLIBRARY_MGR::SFLType type, bool enable);
    bool m_getRunEnable(SFLIBRARY_MGR::SFLType type);

private:
    int             m_cameraId;
    char            m_name[EXYNOS_CAMERA_NAME_STR_SIZE];
    mutable Mutex   m_stateLock;
    mutable Mutex   m_libraryLock;
    SFLIBRARY_MGR::SFLType m_curType;

    sp<ExynosCameraSFLInterface> m_library[SFLIBRARY_MGR::MAX];

}; //Class ExynosCameraSFLMgr

}; //namespace android

#endif
