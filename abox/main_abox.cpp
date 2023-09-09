/*
 * Copyright (C) 2017 Samsung Electronics Co. Ltd.
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

#define LOG_TAG "main_abox"
//#define LOG_NDEBUG 0

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <fnmatch.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <log/log.h>
#include <cutils/uevent.h>

#define MAX_EPOLL_EVENTS (8)
#define BUFFER_SIZE (4096)
#define MAX_DUMP_COUNT (3)

#define DEVPATH "DEVPATH="
#define SYS_PATH "/sys"
#define SERVICE_FILE "/service"
#define RESET_FILE "/reset"
#define FAILSAFE_RESET_FILE "/failsafe/reset"
#define DEBUG_FILE "/0.abox-debug"
#define DEBUG_FILE_LEG "/0.abox_debug"
#define SRAM_FILE "/calliope_sram"
#define DRAM_FILE "/calliope_dram"
#define IVA_FILE "/calliope_iva"
#define PRIV_FILE "/calliope_priv"
#define SLOG_FILE "/calliope_slog"
#define GPR_FILE "/gpr"
#define DEBUG_PATH "/d/abox"
#define PROC_PATH "/proc/abox"
#define REGMAP_PATH "/d/regmap"
#define REGISTERS_FILE "/registers"
#define LOG_FILE "/log-00"
#define COUNT "COUNT="

struct abox_t {
    int fd;
    int epoll_fd;
};

static struct abox_t abox;

static char dev_path[128];
static int dev_path_len;
static char sys_path[128];
static int sys_path_len;
static char debug_path[128];
static int debug_path_len;
static char debug_path_leg[128];
static int debug_path_leg_len;
static char regmap_path[128];
static int regmap_path_len;
static char out_path[128];
static int out_path_len;

static void rm_old_dump(const char *path, const char *prefix)
{
    struct dirent **list;
    char pattern[128];
    int n, m;

    ALOGD("%s(%s, %s)", __func__, path, prefix);

    if (snprintf(pattern, sizeof(pattern), "%s*", prefix + 1) < 0) {
        ALOGE("%s: pattern error: %s", __func__, strerror(errno));
        return;
    }

    n = scandir(path, &list, NULL, alphasort);
    if (n < 0) {
        ALOGE("%s: scandir failed: %s", __func__, strerror(errno));
        return;
    }
    m = 0;
    while (n--) {
        if (!fnmatch(pattern, list[n]->d_name, FNM_FILE_NAME)) {
            if (++m > MAX_DUMP_COUNT) {
                char *tgt;

                if (asprintf(&tgt, "%s/%s", path, list[n]->d_name) != -1) {
                    remove(tgt);
                    free(tgt);
                }
            }
        }
        free(list[n]);
    }
    free(list);
}

static int __dump(const char *in_prefix, const char *in_file, const char *out_prefix, const char *out_suffix)
{
    char buf[4096], in_path[128], out_path[128];
    int fd_in, fd_out, n;
    int total = 0;
    mode_t mask;

    ALOGD("%s(%s, %s, %s, %s)", __func__, in_prefix, in_file, out_prefix, out_suffix);

    if (snprintf(in_path, sizeof(in_path), "%s%s", in_prefix, in_file) < 0) {
        ALOGE("%s: in path error: %s", __func__, strerror(errno));
        return -1;
    }

    if (snprintf(out_path, sizeof(out_path), "%s%s_%s", out_prefix, in_file, out_suffix) < 0) {
        ALOGE("%s: out path error: %s", __func__, strerror(errno));
        return -1;
    }

    mask = umask(002);

    fd_in = open(in_path, O_RDONLY | O_NONBLOCK);
    if (fd_in > -1) {
        fd_out = open(out_path, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
        if (fd_out > -1) {
            while((n = read(fd_in, buf, sizeof(buf))) > 0) {
                if (write(fd_out, buf, n) < 0) {
                    ALOGE("%s: write error: %s", __func__, strerror(errno));
                }
                total += n;
            }
            close(fd_out);
        } else {
            ALOGE("%s: open error: %s, fd_out=%s", __func__, strerror(errno), out_path);
            total = -1;
        }
        close(fd_in);
    } else {
        ALOGE("%s: open error: %s, fd_in=%s", __func__, strerror(errno), in_path);
        total = -1;
    }

    mask = umask(mask);

    rm_old_dump(out_prefix, in_file);
    return total;
}

static void dump(void)
{
    char str_time[32];
    time_t t;
    struct tm *lt;

    ALOGD("%s", __func__);

    t = time(NULL);
    lt = localtime(&t);
    if (lt == NULL) {
        ALOGE("%s: time conversion error: %s", __func__, strerror(errno));
        return;
    }
    if (strftime(str_time, sizeof(str_time), "%Y%m%d_%H%M%S", lt) == 0) {
        ALOGE("%s: time error: %s", __func__, strerror(errno));
    }

    if (mkdir(out_path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) < 0) {
        ALOGW("mkdir(%s) failed: %s", out_path, strerror(errno));
    }

    if (__dump(debug_path, SRAM_FILE, out_path, str_time) <= 0)
        __dump(debug_path_leg, SRAM_FILE, out_path, str_time);
    if (__dump(debug_path, DRAM_FILE, out_path, str_time) <= 0)
        __dump(debug_path_leg, DRAM_FILE, out_path, str_time);
    if (__dump(debug_path, PRIV_FILE, out_path, str_time) <= 0)
        __dump(debug_path_leg, PRIV_FILE, out_path, str_time);
    if (__dump(debug_path, SLOG_FILE, out_path, str_time) <= 0)
        __dump(debug_path_leg, SLOG_FILE, out_path, str_time);
    if (__dump(debug_path, GPR_FILE, out_path, str_time) <= 0)
        __dump(debug_path_leg, GPR_FILE, out_path, str_time);
    if (__dump(DEBUG_PATH, LOG_FILE, out_path, str_time) <= 0)
        __dump(PROC_PATH, LOG_FILE, out_path, str_time);
    __dump(regmap_path, REGISTERS_FILE, out_path, str_time);
}

static void reset(void)
{
    int fd, n;
    const char str[] = "CALLIOPE";
    char *path;

    ALOGD("%s", __func__);

    n = asprintf(&path, "%s%s", sys_path, RESET_FILE);
    if (n >= 0) {
        ALOGD("%s : %s", __func__, path);
        if (access(path, F_OK) != 0) {
            free(path);
            n = asprintf(&path, "%s%s", PROC_PATH, FAILSAFE_RESET_FILE);
            ALOGD("%s = %s", __func__, path);
        }
    }

    if (n >= 0) {
        fd = open(path, O_WRONLY);
        if (fd < 0) {
            ALOGE("%s: open error: %s", __func__, strerror(errno));
        } else {
            n = write(fd, str, strlen(str));
            if (n < (int)strlen(str)) {
                ALOGE("%s: write error: %s", __func__, strerror(errno));
            }
            close(fd);
        }
        free(path);
    } else {
        ALOGE("%s: path error: %s", __func__, strerror(errno));
    }
}

static int recv_event(void)
{
    char msg[BUFFER_SIZE + 2];
    char *cp;
    int count;
    int n;

    n = uevent_kernel_multicast_recv(abox.fd, msg, BUFFER_SIZE);
    if (n <= 0)
        return n;

    msg[n] = 0;
    msg[n+1] = 0;
    cp = msg;

    while (*cp) {
        // ALOGV("UEVENT: %s", cp);
        if (!strncmp(cp, DEVPATH, sizeof(DEVPATH) - 1) &&
            !strncmp(cp + sizeof(DEVPATH) - 1, dev_path, dev_path_len)) {
            do {
                while (*cp++) {}
                // ALOGD("UEVENT: %s", cp);
                if (sscanf(cp, COUNT"%d", &count) > 0) {
                    ALOGD("%s, count=%d", cp, count);
                    if (count > 0) {
                        ALOGW("fault report from Calliope: %d", count);
                        dump();
                        reset();
                    }
                    break;
                }
            } while (*cp);
        }
        /* advance to after the next \0 */
        while (*cp++) {}
    }

    return 0;
}

static void main_loop(void)
{
    ALOGI("%s", __func__);

    while (true) {
        epoll_event events[MAX_EPOLL_EVENTS];
        int nevents;

        nevents = epoll_wait(abox.epoll_fd, events, MAX_EPOLL_EVENTS, -1);
        if (nevents < 0 && errno != EINTR) {
            ALOGE("epoll_wait failed");
            break;
        }

        recv_event();
    }
}

static void report_service(void)
{
    int fd, n;
    const char str[] = "1";
    char *path;

    ALOGI("%s", __func__);

    n = asprintf(&path, "%s%s", sys_path, SERVICE_FILE);
    if (n >= 0) {
        fd = open(path, O_WRONLY);
        if (fd < 0) {
            ALOGE("%s: open error: %s", __func__, strerror(errno));
        } else {
            n = write(fd, str, strlen(str));
            if (n < (int)strlen(str)) {
                ALOGE("%s: write error: %s", __func__, strerror(errno));
            }
            close(fd);
        }
        free(path);
    } else {
        ALOGE("%s: path error: %s", __func__, strerror(errno));
    }

}

int main(int argc, char **argv)
{
    epoll_event ev;
    int ret = 0;

    ALOGD("%s", __func__);

    if (argc > 2) {
        dev_path_len = snprintf(dev_path, sizeof(dev_path), "/devices/platform/%s", argv[1]);
        if (dev_path_len < 0 || (size_t)dev_path_len >= sizeof(dev_path)) {
            ALOGE("invalid argument: %s", argv[1]);
            return -1;
        }
        ALOGD("dev_path=%s", dev_path);

        sys_path_len = snprintf(sys_path, sizeof(sys_path), "%s%s", SYS_PATH, dev_path);
        if (sys_path_len < 0 || (size_t)sys_path_len >= sizeof(sys_path)) {
            ALOGE("invalid argument: %s", argv[1]);
            return -1;
        }
        ALOGD("sys_path=%s", sys_path);

        debug_path_len = snprintf(debug_path, sizeof(debug_path), "%s%s", sys_path, DEBUG_FILE);
        if (debug_path_len < 0 || (size_t)debug_path_len >= sizeof(debug_path)) {
            ALOGE("invalid argument: %s", argv[1]);
            return -1;
        }
        ALOGD("debug_path=%s", debug_path);

        /* compatibility with legacy kernel */
        debug_path_leg_len = snprintf(debug_path_leg, sizeof(debug_path_leg), "%s%s", sys_path, DEBUG_FILE_LEG);
        if (debug_path_leg_len < 0 || (size_t)debug_path_leg_len >= sizeof(debug_path_leg)) {
            ALOGE("invalid argument: %s", argv[1]);
            return -1;
        }
        ALOGD("debug_path_leg=%s", debug_path_leg);

        regmap_path_len = snprintf(regmap_path, sizeof(regmap_path), "%s/%s", REGMAP_PATH, argv[1]);
        if (regmap_path_len < 0 || (size_t)regmap_path_len >= sizeof(regmap_path)) {
            ALOGE("invalid argument: %s", argv[1]);
            return -1;
        }
        ALOGD("regmap_path=%s", regmap_path);

        out_path_len = snprintf(out_path, sizeof(out_path), "%s", argv[2]);
        if (out_path_len < 0 || (size_t)out_path_len >= sizeof(out_path)) {
            ALOGE("invalid argument: %s", argv[2]);
            return -1;
        }
        ALOGD("out_path=%s", out_path);
    } else {
        ALOGE("insufficient argument");
        return -1;
    }

    ret = abox.epoll_fd = epoll_create(MAX_EPOLL_EVENTS);
    if (ret >= 0) {
        ret = abox.fd = uevent_open_socket(BUFFER_SIZE, true);
        if (ret >= 0) {
            ret = fcntl(abox.fd, F_SETFL, O_NONBLOCK);
            if (ret >= 0) {
                ev.events = EPOLLIN | EPOLLWAKEUP;
                ret = epoll_ctl(abox.epoll_fd, EPOLL_CTL_ADD, abox.fd, &ev);
                if (ret >= 0) {
                    report_service();
                    main_loop();
                } else {
                    ALOGE("epoll_ctl failed: %s", strerror(errno));
                }
            } else {
                ALOGE("fcntl failed: %s", strerror(errno));
            }
            close(abox.fd);
        } else {
            ALOGE("uevent_open_socket failed: %s", strerror(errno));
        }
        close(abox.epoll_fd);
    } else {
        ALOGE("epoll_create failed: %s", strerror(errno));
    }

    return ret;
}

