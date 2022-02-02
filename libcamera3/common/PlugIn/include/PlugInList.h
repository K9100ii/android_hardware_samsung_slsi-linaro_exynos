/*
 * Copyright (C) 2017, Samsung Electronics Co. LTD
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
#ifndef PLUG_IN_LIST_H
#define PLUG_IN_LIST_H

#include "ExynosCameraConfig.h" /* for pipeId */

#define PLUGIN_LIB_NM_FAKEFUSION "libexynoscamera_fakefusion_plugin.so"
#define PLUGIN_LIB_NM_LOWLIGHTSHOT "libexynoscamera_lowlightshot_plugin.so"

typedef struct PlugInList {
    int pipeId;
    const char *plugInLibName;
} PlugInList_t;

PlugInList_t g_plugInList[] = {
#ifdef USE_DUAL_CAMERA
    { PIPE_FUSION,              PLUGIN_LIB_NM_FAKEFUSION },
    { PIPE_FUSION_FRONT,        PLUGIN_LIB_NM_FAKEFUSION },
    { PIPE_FUSION_REPROCESSING, PLUGIN_LIB_NM_FAKEFUSION },
#endif
#ifdef USE_LLS_REPROCESSING
    { PIPE_LLS_REPROCESSING,    PLUGIN_LIB_NM_LOWLIGHTSHOT },
#endif
};
#endif
