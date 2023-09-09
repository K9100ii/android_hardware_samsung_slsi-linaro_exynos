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

/*#define LOG_NDEBUG 0 */
#define LOG_TAG "ExynosCmaeraPlugInUtils"
#include <log/log.h>
#include <binder/MemoryHeapBase.h>
#include <ion/ion.h>

#include "ExynosCameraPlugInUtils.h"

namespace android {

/*
 * ION memory helper Class
 */
ExynosCameraPlugInUtilsIonAllocator::ExynosCameraPlugInUtilsIonAllocator(bool isCached)
{
    status_t ret = NO_ERROR;

    m_ionClient = ion_open();
    if (m_ionClient < 0) {
        PLUGIN_LOGE("ion_open failed");
    }

    m_ionAlign = 0;
    m_ionHeapMask = ION_HEAP_SYSTEM_MASK;
    m_ionFlags = (isCached == true ?
            (ION_FLAG_CACHED | ION_FLAG_CACHED_NEEDS_SYNC ) : 0);
}

ExynosCameraPlugInUtilsIonAllocator::~ExynosCameraPlugInUtilsIonAllocator()
{
    if (m_ionClient >= 0)
        ion_close(m_ionClient);
}

status_t ExynosCameraPlugInUtilsIonAllocator::alloc(int size, int *fd, char **addr, bool mapNeeded)
{
    status_t ret = NO_ERROR;
    int ionFd = 0;
    char *ionAddr = NULL;

    if (m_ionClient == 0) {
        PLUGIN_LOGE("allocator is not yet created");
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (size == 0) {
        PLUGIN_LOGE("size equals zero");
        ret = BAD_VALUE;
        goto func_exit;
    }

    ret = ion_alloc_fd(m_ionClient, size, m_ionAlign, m_ionHeapMask, m_ionFlags, &ionFd);

    if (ret < 0) {
        PLUGIN_LOGE("ion_alloc_fd(fd=%d) failed(%s)", ionFd, strerror(errno));
        ionFd = -1;
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (mapNeeded == true) {
        ionAddr = (char *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, ionFd, 0);
        if (ionAddr == (char *)MAP_FAILED || ionAddr == NULL) {
            PLUGIN_LOGE("ion_map(size=%d) failed, (fd=%d), (%s)", size, ionFd, strerror(errno));
            close(ionFd);
            ionFd = -1;
            ionAddr = NULL;
            ret = INVALID_OPERATION;
            goto func_exit;
        }
    }

func_exit:

    *fd = ionFd;
    *addr = ionAddr;

    return ret;
}

status_t ExynosCameraPlugInUtilsIonAllocator::free(int *fd, __unused int size, char **addr, bool mapNeeded)
{
    status_t ret = NO_ERROR;
    int ionFd = *fd;
    char *ionAddr = *addr;

    if (ionFd < 0) {
        PLUGIN_LOGE("ion_fd is lower than zero");
        ret = BAD_VALUE;
        goto func_exit;
    }

    if (mapNeeded == true) {
        if (ionAddr == NULL) {
            PLUGIN_LOGE("ion_addr equals NULLi(%d)", ionFd);
            ret = BAD_VALUE;
            goto func_close_exit;
        }

        if (munmap(ionAddr, size) < 0) {
            PLUGIN_LOGE("munmap failed");
            ret = INVALID_OPERATION;
            goto func_close_exit;
        }
    }

func_close_exit:

    ion_close(ionFd);

    ionFd = -1;
    ionAddr = NULL;

func_exit:

    *fd = ionFd;
    *addr = ionAddr;

    return ret;
}

status_t dumpToFile(char *filename, PlugInBuffer *planes, int planeCount, int index)
{
    int i;
    time_t rawtime;
    struct tm* timeinfo;
    int totalSize = 0;
    FILE *fd = NULL;
    char *buffer = NULL;
    char finalFilePath[512];

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    snprintf(finalFilePath, sizeof(finalFilePath),
            PLUGIN_FILE_DUMP_PATH "%s_%d_%02d%02d%02d_%02d%02d%02d.dump",
            filename, index, timeinfo->tm_year+1900, timeinfo->tm_mon+1, timeinfo->tm_mday,
            timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

    fd = fopen(finalFilePath, "w+");
    if (fd == NULL) {
        PLUGIN_LOGE("ERR(%s):open(%s) fail", filename);
        return INVALID_OPERATION;
    }

    fflush(stdout);

    for (i = 0; i < planeCount; i++) {
        fwrite(planes[i].addr, 1, planes[i].size, fd);
        totalSize += planes[i].size;
    }

    fflush(fd);

    if (fd)
        fclose(fd);

    PLUGIN_LOGD("filedump(%s, planes(%d) totalSize(%d)) is successed!!",
            finalFilePath, i, totalSize);

    return NO_ERROR;
}
}
