/*
 * Copyright (C) 2016 The Android Open Source Project
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

#define LOG_TAG "dumpstate"

#include "DumpstateDevice.h"

#include <hidl/HidlBinderSupport.h>
#include <log/log.h>

#include <ion/ion.h>

#include "DumpstateUtil.h"

using android::os::dumpstate::DumpFileToFd;
using android::os::dumpstate::RunCommandToFd;
using android::os::dumpstate::CommandOptions;

namespace android {
namespace hardware {
namespace dumpstate {
namespace V1_0 {
namespace implementation {

// Methods from ::android::hardware::dumpstate::V1_0::IDumpstateDevice follow.
Return<void> DumpstateDevice::dumpstateBoard(const hidl_handle& handle) {
    // NOTE: this is just an example on how to use the DumpstateUtil.h functions to implement
    // this interface - since HIDL_FETCH_IDumpstateDevice() is not defined, this function will never
    // be called by dumpstate.

    ALOGI("Starting Dumpstate for Exynos BSP on Board");

    // Exit when dump is completed since this is a lazy HAL.
    addPostCommandTask([]() {
        exit(0);
    });

    if (handle == nullptr || handle->numFds < 1) {
        ALOGE("no FDs\n");
        return Void();
    }

    int fd = handle->data[0];
    if (fd < 0) {
        ALOGE("invalid FD: %d\n", handle->data[0]);
        return Void();
    }

    bool modern_ion = true;
    int ionfd = ion_open();
    if (ionfd >= 0) {
	    if (ion_is_legacy(ionfd))
		    modern_ion = false;
	    ion_close(ionfd);
    }

    if (modern_ion) {
	    DumpFileToFd(fd, "ION: List of allocated buffers", "/d/ion/buffers");
	    RunCommandToFd(fd, "ION: Heap details", {"/vendor/bin/sh", "-c", "cout=0;for heap in $(ls /d/ion/heaps); do count=$(($count+1));cat /d/ion/heaps/$heap; echo; done; echo ::: total $count heaps found; echo"});
	    DumpFileToFd(fd, "ION: Latest activities", "/d/ion/event");
    } else {
	    DumpFileToFd(fd, "ION: List of allocated buffers", "/d/ion/buffer");
	    RunCommandToFd(fd, "ION: Heap details", {"/vendor/bin/sh", "-c", "cout=0;for heap in $(ls /d/ion/heaps); do count=$(($count+1));echo \\[ $heap \\];cat /d/ion/heaps/$heap; echo; done; echo ::: total $count heaps found; echo"});
	    RunCommandToFd(fd, "ION: Client details", {"/vendor/bin/sh", "-c", "cout=0;for client in $(ls /d/ion/clients); do count=$(($count+1));echo \\[ $client \\];cat /d/ion/clients/$client; echo; done; echo ::: total $count clients found; echo"});
	    DumpFileToFd(fd, "ION: Latest activities", "/d/ion/event");
    }

    DumpFileToFd(fd, "dma-buf: bufinfo", "/d/dma_buf/bufinfo");

    if (modern_ion)
	    RunCommandToFd(fd, "dma-buf: footprint", {"/vendor/bin/sh", "-c", "for pid in $(ls /d/dma_buf/footprint); do echo -n \\[$pid\\]; cat /d/dma_buf/footprint/$pid; echo; done"});

    DumpFileToFd(fd, "Mali: gpu_memory", "/d/mali/gpu_memory");
    RunCommandToFd(fd, "Mali: mem_profile", {"/vendor/bin/sh", "-c", "for d in $(ls /d/mali/mem); do if [ -f /d/mali/mem/$d/mem_profile ]; then echo \\[ $d/mem_profile \\];cat /d/mali/mem/$d/mem_profile; fi; done"});

    return Void();
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace dumpstate
}  // namespace hardware
}  // namespace android
