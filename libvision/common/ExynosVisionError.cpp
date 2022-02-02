/*
 * Copyright (C) 2015, Samsung Electronics Co. LTD
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

#define LOG_TAG "ExynosVisionError"
#include <cutils/log.h>

#include "ExynosVisionError.h"

namespace android {

/* Constructor */
ExynosVisionError::ExynosVisionError(ExynosVisionContext *context, vx_status status)
                                                                    : ExynosVisionReference(context, VX_TYPE_ERROR, (ExynosVisionReference*)context, vx_false_e)
{
    m_error_type = status;
}

/* Destructor */
ExynosVisionError::~ExynosVisionError(void)
{

}

}; /* namespace android */
