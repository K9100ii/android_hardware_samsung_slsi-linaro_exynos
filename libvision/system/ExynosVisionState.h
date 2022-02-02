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

#ifndef EXYNOS_VISION_STATE_H
#define EXYNOS_VISION_STATE_H

#include <utils/Mutex.h>

#include <VX/vx.h>

namespace android {

class ExynosVisionState {
private:
    Mutex m_state_mutex;
    vx_enum m_state;

public:
    ExynosVisionState()
    {
        m_state = 0;
    }

    ~ExynosVisionState()
    {

    }

    void setState(vx_enum state)
    {
        Mutex::Autolock lock(m_state_mutex);
        m_state = state;
    }

    vx_enum getState()
    {
        Mutex::Autolock lock(m_state_mutex);
        return m_state;
    }
};

}; // namespace android
#endif
