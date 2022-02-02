/*
 * Copyright 2013, Samsung Electronics Co. LTD
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
 * \file      ExynosCameraSFLdefine.h
 * \brief     define file for ExynosCameraSFLInterface / ExynosCameraSFLMgr
 * \author    suyounglee(suyoung80.lee@samsung.com)
 * \date      2015/11/25
 *
 */
#ifndef EXYNOS_CAMERA_SFL_DEFINE_H__
#define EXYNOS_CAMERA_SFL_DEFINE_H__

namespace android {

/* library type */
namespace SFLIBRARY_MGR {
enum SFLType{
    NONE = -1,
    HDR,
    NIGHT,
    ANTISHAKE,
    OIS,
    PANORAMA,
    FLAWLESS,
    MAX
};
};

/* library process command */
namespace SFL {
enum Command{
    PROCESS = 0x100, /* run the algorithm */
    ADD_BUFFER,      /* add buffer */
    SET_MAXBUFFERCNT,/* set max run buffer count */
    GET_MAXBUFFERCNT,/* get max run buffer count */

    SET_MAXSELECTCNT,/* set max run select count */
    GET_MAXSELECTCNT,/* get max run select count */

    GET_CURBUFFERCNT,/* get cur run buffer count */

    SET_CURSELECTCNT,/* set cur run select count */
    GET_CURSELECTCNT,/* get cur run select count */

    SET_CONFIGDATA , /* setup config data */

    GET_ADJUSTPARAMINFO , /* get adjust param info */

    SET_BESTFRAMEINFO , /* set best FrameInfo */
    GET_BESTFRAMEINFO , /* get best FrameInfo */

    SET_SIZE,
    GET_SIZE,
    CMD_NONE,
};

/* library info buffer position */
enum BufferPos {
    POS_SRC = 1,
    POS_DST,
    POS_MAX
};

/* library info buffer type */
enum BufferType {
    TYPE_PREVIEW = 1,
    TYPE_PREVIEW_CB,
    TYPE_RECORDING,
    TYPE_CAPTURE,
    TYPE_MAX,
};

};

/* ExynosCameraSFLInfo debugging trace */
/* #define SFLINFO_TRACE */

}; //namespace android

#endif
