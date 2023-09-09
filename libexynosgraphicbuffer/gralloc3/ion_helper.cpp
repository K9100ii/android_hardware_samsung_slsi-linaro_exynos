/*
 * Copyright (C) 2020 Samsung Electronics Co. Ltd.
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

#include "ExynosGraphicBuffer.h"

#if defined(NO_ION_HELPER)

int vendor::graphics::ion::get_ion_fd() {
    return -1;
}

#else /* NO_ION_HELPER */

#include <hardware/exynos/ion.h>
#include <log/log.h>

static int ion_client;

int vendor::graphics::ion::get_ion_fd() {
    if (ion_client <= 0) {
        ion_client = exynos_ion_open();

        if (ion_client < 0) {
            int ret = errno;
            ALOGE("failed to get ion client %s", strerror(ret));
            return -ret;
        }
    }

    return ion_client;
}

#endif /* NO_ION_HELPER */
