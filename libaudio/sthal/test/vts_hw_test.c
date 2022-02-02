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

/*
 * @file            : vts_hw_test.c
 * @brief           : Voice Trigger Subsystem IP Unit test case application
 * @author          : Palli Satish Kumar Reddy (palli.satish@samsung.com)
 * @version         : 1.0
 * @history
 *   2016.05.16     : Create
 *   2018.06.07     : Updated to make it a generic unit test application which
 *                         can support all possible SoCs
 */

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <poll.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <errno.h>
#include <cutils/uevent.h>
#include <tinyalsa/asoundlib.h>

#include"sound_trigger_hw.h"
#ifdef MMAP_INTERFACE_ENABLED
#include"vts.h"
#endif

#define UEVENT_MSG_LEN          64*1024

typedef enum {
    VTS_RECOGNITION_STOPPED_STATE,
    VTS_RECOGNITION_STARTED_STATE,
} VTS_HW_STATE;

typedef enum {
    VTS_RECORD_STOPPED_STATE,
    VTS_RECORD_STARTED_STATE,
} VTS_RECORD_STATE;

typedef enum {
    VTS_RESERVED2_STOPPED_STATE,
    VTS_RESERVED2_STARTED_STATE,
} VTS_ODMRSVD2_STATE;

typedef enum {
    VTS_TRIGGERED_CAPTURE,
    VTS_NORMAL_CAPTURE,
} VTS_CAP_MODE;

/* Sound model binaries */
#define SOUND_MODEL_OKGOOGLE_BINARY         "/data/voice_dva_okgoogle.bin"
#define SOUND_MODEL_ODMVOICE_BINARY           "/data/voice_dva_odmvoice.bin"

#define VTS_TRIGGER_CAPTURE_OUTPUT  "/sdcard/vts-trigger-cap.wav"
#define VTS_NORMAL_CAPTURE_OUTPUT  "/sdcard/vts-normal-cap.wav"

#define VTS_STREAMING_BUFFER_SIZE  4800 //(4 * 1024)
#define NUM_OF_SAMPLES_TO_CAPTURE 100

struct vts_hw_device {
    VTS_HW_STATE vts_state;
    VTS_RECORD_STATE vts_rec_state;
    VTS_ODMRSVD2_STATE vts_odmrsvd2_state;
    pthread_mutex_t lock;
    int send_sock;
    int term_sock;
    bool thread_exit;
    bool rec_thread_exit;
    bool odmrsvd2_thread_exit;
    pthread_t callback_thread;
    pthread_t rec_thread;
    pthread_t odmrsvd2_thread;
    bool model_loaded;
};

#define ID_RIFF 0x46464952
#define ID_WAVE 0x45564157
#define ID_FMT  0x20746d66
#define ID_DATA 0x61746164

#define FORMAT_PCM 1

struct wav_header {
    uint32_t riff_id;
    uint32_t riff_sz;
    uint32_t riff_fmt;
    uint32_t fmt_id;
    uint32_t fmt_sz;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    uint32_t data_id;
    uint32_t data_sz;
};

static struct vts_hw_device g_vtsdev = { .lock = PTHREAD_MUTEX_INITIALIZER };
struct mixer            *vtsMixerHandle = NULL;
struct mixer_ctl        *vtsMixerCtl = NULL;
#ifdef MMAP_INTERFACE_ENABLED
int   vtsdev_fd = -1;
void *mapped_addr = NULL;
#endif

void vts_recognition_stop(struct vts_hw_device *vts_dev);
void vts_odmrsvd2_stop(struct vts_hw_device *vts_dev);
void vts_record_stop(struct vts_hw_device *vts_dev);

static int dmic_usagecnt = 0;

// Utility function for configuration MIC mixer controls
int set_mixer_ctrls(
        struct mixer            *mixer_handle,
        char *path_name[],
        int *path_ctlvalue,
        int ctrl_count,
        bool reverse)
{
    int i = (reverse ? (ctrl_count - 1): 0);
    int ret = 0;
    struct mixer_ctl *mixerctl = NULL;

    printf("%s, path: %s", __func__, path_name[0]);

    if (mixer_handle) {
        //for (i=0; i < ctrl_count; i++) {
        while(ctrl_count) {
            //printf("%s, ctrl_count: %d Loop index: %d", __func__, ctrl_count, i);
            /* Get required control from mixer */
            mixerctl = mixer_get_ctl_by_name(mixer_handle, path_name[i]);
            if (mixerctl) {
                /* Enable the control */
                if (path_ctlvalue)
                    ret = mixer_ctl_set_value(mixerctl, 0, path_ctlvalue[i]);
                else
                    ret = mixer_ctl_set_value(mixerctl, 0, 0);

                if (ret) {
                    printf("%s: %s Failed to configure\n", __func__, path_name[i]);
                    ret = -EINVAL;
                    break;
                } else {
                    printf("%s: %s configured value: %d\n", __func__, path_name[i],
                        (path_ctlvalue ? path_ctlvalue[i] : 0));
                }
             } else {
                printf("%s: %s control doesn't exist\n", __func__, path_name[i]);
                ret = -EINVAL;
                break;
            }
            ctrl_count--;
            if (reverse)
                i--;
            else
                i++;
        }
    } else{
        printf("%s: Failed to open mixer\n", __func__);
        return -EINVAL;
    }

    return ret;
}

#ifdef MMAP_INTERFACE_ENABLED
static void load_modelbinary(char *data, int len)
{
    printf("%s: Size: %d\n", __func__, len);

    /* Copy model binary to VTS mapped address */
    memcpy(mapped_addr, data, len);

    printf("%s: Model binary copied to Mapped memory Size: %d\n", __func__, len);

    if (len > VTSDRV_MISC_MODEL_BIN_MAXSZ) {
        printf("%s: MMAP buffer overflow Model Binary size greater then mapped size !!!\n", __func__);
    }

    /* Update model binary inforation to VTS misc driver */
    if (ioctl(vtsdev_fd, VTSDRV_MISC_IOCTL_WRITE_ODMVOICE, &len) < 0) {
        printf("%s: Failed to update model binary size\n", __func__);
    }

    return;
}
#endif
static void sysfs_write(const char *path, char *data, int len)
{
    char buf[80];
    int fd = open(path, O_WRONLY);
    int tmp = 0, written = 0;

    if (fd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        printf("Error opening %s: %s\n", path, buf);
        return;
    }

    while (len) {
        tmp = write(fd, data+written, len);
        if (tmp < 0) {
            strerror_r(errno, buf, sizeof(buf));
            printf("Error writing to %s: %s\n", path, buf);
            break;
        }
        len -= tmp;
        written += tmp;
        printf("%s: current written %d Actual %d Total written %d\n",__func__, tmp, len, written);
    }

    close(fd);
    return;
}

int set_dmic_ctrls(int flag)
{
    int i;
    int ret = EXIT_SUCCESS;
    char **active_mic_ctrls = NULL;
    int *ctrl_values = NULL;

    if (vtsMixerHandle) {
        if (!flag) {
            if (dmic_usagecnt) {
                dmic_usagecnt--;
                printf("Dmic Disabled usage count %d \n", dmic_usagecnt);
            } else {
                printf("Dmic usage count is Zero \n");
                return ret;
            }
        }

        if (!dmic_usagecnt) {
            active_mic_ctrls = main_mic_ctlname;
            ctrl_values = main_mic_ctlvalue;

            if (set_mixer_ctrls(vtsMixerHandle, active_mic_ctrls, ctrl_values, MAIN_MIC_CONTROL_COUNT, !flag)) {
                printf("%s: %s MIC control configuration Failed", __func__,
                            flag ? "Enabling" : "Disabling");
                mixer_close(vtsMixerHandle);
                vtsMixerHandle = NULL;
                return -EINVAL;
            }
            printf("%s: %s MIC Controls ", __func__, flag ? "Enable" : "Disable");
        }

        if (flag) {
            dmic_usagecnt++;
            printf("Dmic Enabled usage count %d \n", dmic_usagecnt);
        }
    } else{
        printf("%s: Failed to open mixer \n", __func__);
        return -EINVAL;
    }

    return ret;
}

void check_vts_state(struct vts_hw_device *vts_dev)
{
    if (vts_dev->vts_state == VTS_RECOGNITION_STARTED_STATE) {
        vts_recognition_stop(vts_dev);
    } else if (vts_dev->vts_odmrsvd2_state == VTS_RESERVED2_STARTED_STATE) {
        vts_odmrsvd2_stop(vts_dev);
    } else if (vts_dev->vts_rec_state == VTS_RECORD_STARTED_STATE) {
        vts_record_stop(vts_dev);
    }

    printf("%s: Exit \n", __func__);
    return;
}

static void vts_close_term_sock(struct vts_hw_device *vts_dev)
{
    if (vts_dev->send_sock >=0) {
        close(vts_dev->send_sock);
        vts_dev->send_sock = -1;
    }
    if (vts_dev->term_sock >=0) {
        close(vts_dev->term_sock);
        vts_dev->term_sock = -1;
    }

    return;
}

// The vts_dev should be locked when you call this function.
static int fetch_streaming_buffer(struct vts_hw_device *vts_dev, int cap_mode)
{
    int ret = 0;
    unsigned int flags, frames;
    int i = 0;
    struct pcm *vcap_pcm = NULL;
    FILE *out_vcap_fp = NULL;
    char *streaming_buf;
    unsigned int pcmnode = (cap_mode == VTS_TRIGGERED_CAPTURE ? VTS_TRICAP_DEVICE_NODE : VTS_RECORD_DEVICE_NODE);
    struct wav_header header;
    struct pcm_config pcmconfig = {
        .channels = 1,
        .rate = 16000,
        .period_size = 160,
        .period_count = 1024,
        .format = PCM_FORMAT_S16_LE,
    };

    streaming_buf = malloc(VTS_STREAMING_BUFFER_SIZE);
    if (!streaming_buf) {
        printf("Failed to malloc streaming buffer!!\n");
        goto out;
    }

    printf("%s: Fetching bytes \n", __func__);
    if (!vcap_pcm) {
        /* Open vts capture pcm node */
        pcmnode = (cap_mode == VTS_TRIGGERED_CAPTURE ? VTS_TRICAP_DEVICE_NODE : VTS_RECORD_DEVICE_NODE);
        flags = PCM_IN;
        vcap_pcm = pcm_open(VTS_SOUND_CARD, pcmnode, flags, &pcmconfig);
        if (vcap_pcm && !pcm_is_ready(vcap_pcm)) {
            printf("%s - FAILED to open VTS PCM Node : %d\n", __func__, pcmnode);
            goto out;
        }

        sysfs_write("/sys/power/wake_lock", "vtshw-test", sizeof("vtshw-test"));
        printf("%s - Wake Lock Acquired\n", __func__);
        /* initialize the Wav Header information */
        header.riff_id = ID_RIFF;
        header.riff_sz = 0;
        header.riff_fmt = ID_WAVE;
        header.fmt_id = ID_FMT;
        header.fmt_sz = 16;
        header.audio_format = FORMAT_PCM;
        header.num_channels = pcmconfig.channels;
        header.sample_rate = pcmconfig.rate;

        header.bits_per_sample = pcm_format_to_bits(PCM_FORMAT_S16_LE);
        header.byte_rate = (header.bits_per_sample / 8) * pcmconfig.channels * pcmconfig.rate;
        header.block_align = pcmconfig.channels * (header.bits_per_sample / 8);
        header.data_id = ID_DATA;

        /* open an output file to dump capture vts voice commands */
        out_vcap_fp = fopen((cap_mode == VTS_TRIGGERED_CAPTURE ? VTS_TRIGGER_CAPTURE_OUTPUT :
                                            VTS_NORMAL_CAPTURE_OUTPUT), "w+");
        if (!out_vcap_fp ) {
            printf("%s - FAILED to open output file !!! %s\n", __func__, (cap_mode == VTS_TRIGGERED_CAPTURE ?
                        VTS_TRIGGER_CAPTURE_OUTPUT : VTS_NORMAL_CAPTURE_OUTPUT));
            /* Release VTS capture node */
            pcm_close(vcap_pcm);
            vcap_pcm = NULL;
            goto out;
        } else {
            /* leave enough room for header */
            fseek(out_vcap_fp, sizeof(struct wav_header), SEEK_SET);

            for (i=0; i < NUM_OF_SAMPLES_TO_CAPTURE; i++) {
                ret = pcm_read(vcap_pcm, (void*)streaming_buf, (unsigned int)VTS_STREAMING_BUFFER_SIZE);
                if (ret == 0) {
                    printf("%s - Captured %d samples\n", __func__, VTS_STREAMING_BUFFER_SIZE);
                    /* write capture pcm data to output file */
                    fwrite((void*)streaming_buf, (size_t)VTS_STREAMING_BUFFER_SIZE, 1, out_vcap_fp);
                } else {
                    printf("%s - Failed to capture requested samples %s \n", __func__, pcm_get_error(vcap_pcm));
                    sleep(10);
                }

                if ((cap_mode == VTS_TRIGGERED_CAPTURE && vts_dev->thread_exit == true) ||
                    (cap_mode == VTS_NORMAL_CAPTURE && vts_dev->rec_thread_exit == true))
                    break;
            }
        }

        /* write header now all information is known */
        frames = pcm_bytes_to_frames(vcap_pcm, (i * VTS_STREAMING_BUFFER_SIZE));
        header.data_sz = frames * header.block_align;
        header.riff_sz = (uint32_t)(header.data_sz + sizeof(header) - 8);
        fseek(out_vcap_fp, 0, SEEK_SET);
        fwrite(&header, sizeof(struct wav_header), 1, out_vcap_fp);

        /* close output file */
        fclose(out_vcap_fp);
        out_vcap_fp = NULL;

        /* Release VTS capture node */
        pcm_close(vcap_pcm);
        vcap_pcm = NULL;
        sysfs_write("/sys/power/wake_unlock", "vtshw-test", sizeof("vtshw-test"));
        printf("%s - Wake Lock Released\n", __func__);
    }

out:
    if (streaming_buf)
        free(streaming_buf);
    return ret;
}

/******** VTS Trigger Mode support function ***************/
static void *callback_thread_loop(void * context)
{
    char msg[UEVENT_MSG_LEN];
    struct pollfd fds[2];
    int exit_sockets[2];
    int err = 0;
    int i, n;
    struct vts_hw_device *vts_dev = (struct vts_hw_device *)context;

    prctl(PR_SET_NAME, (unsigned long)"VTS callback", 0, 0, 0);

#if 0
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, exit_sockets) == -1)
        goto func_exit;

    vts_close_term_sock(vts_dev);
    vts_dev->send_sock = exit_sockets[0];
    vts_dev->term_sock = exit_sockets[1];

    printf("%s - Terminate Socket %d, %d\n", __func__, exit_sockets[0], exit_sockets[1]);
#else
    vts_dev->send_sock = -1;
    vts_dev->term_sock = -1;
#endif

    memset(fds, 0, 2 * sizeof(struct pollfd));
    fds[0].events = POLLIN;
    fds[0].fd = uevent_open_socket(64*1024, true);
    //fds[0].fd = vts_dev->term_sock;
    if (fds[0].fd == -1) {
        printf("Error opening socket for hotplug uevent");
        goto func_exit;
    }
#if 0
    fds[1].events = POLLIN;
    fds[1].fd = vts_dev->term_sock;
#endif

    while (1) {
        printf("%s: Before poll \n", __func__);
        /* wait for VTS trigger Uevent */
        err = poll(fds, 1, -1);
        printf("%s: After poll \n", __func__);

        if (fds[0].revents & POLLIN) {
            n = uevent_kernel_multicast_recv(fds[0].fd, msg, UEVENT_MSG_LEN);
            if (n <= 0) {
                printf("%s: Cannot Read Socket message %d \n", __func__, __LINE__);
                continue;
            }
            for (i=0; i < n;) {
                //if (strstr(msg + i, "DEVPATH=/devices/platform/14020000.vts")) {
                if (strstr(msg + i, "VOICE_WAKEUP_WORD_ID=1")) {
                    printf("%s VTS Trigger received for model %s\n", __func__, (msg+i));
                    /* Start reading data from the VTS IP */
                        fetch_streaming_buffer(vts_dev, VTS_TRIGGERED_CAPTURE);
                    printf("\n%s Want to Continue...then Start Recognitoin again\n", __func__);
                    goto found;
                 }
                 i += strlen(msg + i) + 1;
            }
#if EN_DEBUG
            if (i >= n) {
                i = 0;
                printf("%s UEVENT MSG INFO: ---\n", __func__);
                while (i < n) {
                    printf("%s \n", (msg+i));
                    i += strlen(msg+i)+1;
                }
            }
#else
            if (i >= n)
                printf("%s UEVENT is NOT VTS event !!\n", __func__);
#endif
         }  else {
            printf("%s: Poll returned %d\n", __func__, err);
        }
        if (vts_dev->thread_exit == true)
            break;
    }

found:
    close(fds[0].fd);
func_exit:
    vts_close_term_sock(vts_dev);
    if (vts_dev->thread_exit == false) {
        /* reset the state as Loaded by stopping the recognition,
             * so that we restart the recogniton again
             * if we have received the Hotwork notification */
        /* once callback thread is closed set vts_stop using sysfs */
        /* Set active keyphrase for voice recognition */
        set_mixer_ctrls(vtsMixerHandle, model_recognize_stop_ctlname,
                odmvoice_triggerrecognize_stop_ctlvalue, MODEL_CONTROL_COUNT, false);
        vts_dev->vts_state = VTS_RECOGNITION_STOPPED_STATE;
        /* reset DMIC controls */
        set_dmic_ctrls(false);
    }
    vts_dev->thread_exit = false;
    printf("%s: Exit \n", __func__);
    return (void *)(long)err;
}

void vts_recognition_start(struct vts_hw_device *vts_dev)
{
    int ret = 0;
    pthread_mutex_lock(&vts_dev->lock);
    if (vts_dev->vts_state == VTS_RECOGNITION_STOPPED_STATE) {
        if (!vts_dev->model_loaded ) {
            FILE *pfile = NULL;
            int rd_sz, bin_sz;
            char * data = NULL;
            /* Read model net binay file*/
            pfile = fopen(SOUND_MODEL_ODMVOICE_BINARY, "rb+");
            if (!pfile) {
                printf("Model Binary voice_dva_svoice.bin should be copied to \\data\\firmware folder \n");
                printf("Failed to Open Model Binary from [%s]\n", SOUND_MODEL_ODMVOICE_BINARY);
                goto error;
            } else {
                printf("Successfully [%s] file opened!! \n", SOUND_MODEL_ODMVOICE_BINARY);
            }

            fseek(pfile, 0L, SEEK_END);
            bin_sz = ftell(pfile);
            fseek(pfile, 0L, SEEK_SET);
            printf(" Model %s File size %d \n", SOUND_MODEL_ODMVOICE_BINARY, bin_sz);

            data = (char *)calloc(1, bin_sz);
            if (!data) {
                printf("Failed to allocated buffer of Size: %d\n", bin_sz);
                fclose(pfile);
                goto error;
            }
            /* read file data to allocated buffer */
            rd_sz = fread(data, 1, bin_sz, pfile);

            if (rd_sz != bin_sz) {
                printf("%s -  Failed to read data from %s file\n", __func__, SOUND_MODEL_ODMVOICE_BINARY);
                fclose(pfile);
                goto error;
            }

            /* Load net binary to VTS driver */
#ifdef MMAP_INTERFACE_ENABLED
            load_modelbinary(data, rd_sz);
#else
            sysfs_write(VTS_SVOICE_MODEL, data, rd_sz);
#endif

            fclose(pfile);
            free(data);
            vts_dev->model_loaded = true;
        }

        /* configure DMIC controls */
        set_dmic_ctrls(true);

        /* Create Callback thread to catch VTS trigger notification through uevent*/
        vts_dev->thread_exit = false;
        pthread_create(&vts_dev->callback_thread, (const pthread_attr_t *) NULL,
                            callback_thread_loop, vts_dev);

        set_mixer_ctrls(vtsMixerHandle, model_recognize_start_ctlname,
                odmvoice_triggerrecognize_start_ctlvalue, MODEL_CONTROL_COUNT, false);

        vts_dev->vts_state = VTS_RECOGNITION_STARTED_STATE;
    } else {
        printf("VTS Voice Recognition already running \n");
    }

error:
    pthread_mutex_unlock(&vts_dev->lock);
    printf("%s: Exit Tar-size 1000ms\n", __func__);
    return;
}

void vts_recognition_stop(struct vts_hw_device *vts_dev)
{
    pthread_mutex_lock(&vts_dev->lock);
    if (vts_dev->vts_state == VTS_RECOGNITION_STARTED_STATE) {
        vts_dev->thread_exit = true;
        /* Stop Callback thread first */
        if (vts_dev->send_sock >= 0)
            write(vts_dev->send_sock, "T", 1);

        pthread_mutex_unlock(&vts_dev->lock);

        pthread_join(vts_dev->callback_thread, (void**)NULL);

        pthread_mutex_lock(&vts_dev->lock);

        set_mixer_ctrls(vtsMixerHandle, model_recognize_stop_ctlname,
                odmvoice_triggerrecognize_stop_ctlvalue, MODEL_CONTROL_COUNT, false);

        /* configure DMIC controls */
        set_dmic_ctrls(false);

        vts_dev->vts_state = VTS_RECOGNITION_STOPPED_STATE;
    } else {
        printf("Sound Model Recognition is NOT Started\n");
    }
    pthread_mutex_unlock(&vts_dev->lock);
    printf("%s: Exit \n", __func__);
    return;
}

/******** VTS Recording support function ***************/
static void *record_thread_loop(void * context)
{
    int err = 0;
    int i, n;
    struct vts_hw_device *vts_dev = (struct vts_hw_device *)context;

    prctl(PR_SET_NAME, (unsigned long)"VTSRecord", 0, 0, 0);

    printf("%s Started!!\n", __func__);
    fetch_streaming_buffer(vts_dev, VTS_NORMAL_CAPTURE);

    vts_dev->vts_rec_state = VTS_RECORD_STOPPED_STATE;

    if (vts_dev->rec_thread_exit == false) {
        /* reset DMIC controls */
        set_dmic_ctrls(false);
    }
    printf("%s: Exit \n", __func__);
    return (void *)(long)err;
}

void vts_record_start(struct vts_hw_device *vts_dev)
{
    pthread_mutex_lock(&vts_dev->lock);
    if (vts_dev->vts_rec_state == VTS_RECORD_STOPPED_STATE) {
        /* configure DMIC controls */
        set_dmic_ctrls(true);

        /* Create recorrd thread to capture data from VTS IP*/
        vts_dev->rec_thread_exit = false;
        pthread_create(&vts_dev->rec_thread, (const pthread_attr_t *) NULL,
                            record_thread_loop, vts_dev);

        vts_dev->vts_rec_state = VTS_RECORD_STARTED_STATE;
    } else {
        printf("VTS Recording Already started \n");
    }
    pthread_mutex_unlock(&vts_dev->lock);
    printf("%s: Exit \n", __func__);
    return;
}

void vts_record_stop(struct vts_hw_device *vts_dev)
{
    pthread_mutex_lock(&vts_dev->lock);
    if (vts_dev->vts_rec_state == VTS_RECORD_STARTED_STATE) {
        /* Stop record thread first */
        vts_dev->rec_thread_exit = true;
        pthread_mutex_unlock(&vts_dev->lock);

        pthread_join(vts_dev->rec_thread, (void**)NULL);

        vts_dev->vts_rec_state = VTS_RECORD_STOPPED_STATE;

        /* reset DMIC controls */
        set_dmic_ctrls(false);
        pthread_mutex_lock(&vts_dev->lock);

    } else {
        printf("VTS Recording NOT started yet \n");
    }
    pthread_mutex_unlock(&vts_dev->lock);
    printf("%s: Exit \n", __func__);
    return;
}

/******** VTS ODM Reserved2 Mode support function ***************/
static void *odmrsvd2_thread_loop(void * context)
{
    char msg[UEVENT_MSG_LEN];
    struct pollfd fds[2];
    int exit_sockets[2];
    int err = 0;
    int i, n;
    struct vts_hw_device *vts_dev = (struct vts_hw_device *)context;

    prctl(PR_SET_NAME, (unsigned long)"VTSODMReserved2Mode", 0, 0, 0);

    printf("%s Started!!\n", __func__);

    memset(fds, 0, 2 * sizeof(struct pollfd));
    fds[0].events = POLLIN;
    fds[0].fd = uevent_open_socket(64*1024, true);
    if (fds[0].fd == -1) {
        printf("Error opening socket for hotplug uevent");
        goto func_exit;
    }

    while (1) {
        printf("%s: ODMReserved2Mode Before poll \n", __func__);
        /* wait for VTS ODMReserved2Mode Uevent */
        err = poll(fds, 1, 10000);
        printf("%s: ODMReserved2Mode After poll \n", __func__);

        if (fds[0].revents & POLLIN) {
            n = uevent_kernel_multicast_recv(fds[0].fd, msg, UEVENT_MSG_LEN);
            if (n <= 0) {
                printf("%s: Cannot Read Socket message %d \n", __func__, __LINE__);
                continue;
            }

            for (i=0; i < n;) {
                if (strstr(msg + i, "VOICE_WAKEUP_WORD_ID=3")) {
                    printf("%s VTS ODMReserved2Mode UEVENT received %s\n", __func__, (msg+i));
                    goto found;
                 }
                 i += strlen(msg + i) + 1;
            }
#if EN_DEBUG
            if (i >= n) {
                i = 0;
                printf("%s ODMReserved2Mode UEVENT MSG INFO: \n", __func__);
                while (i < n) {
                    printf("%s \n", (msg+i));
                    i += strlen(msg+i)+1;
                }
            }
#else
            if (i >= n)
                printf("%s Received UEVENT is NOT ODMReserved2Mode event !!\n", __func__);
#endif
         }  else {
            printf("%s: Poll returned %d\n", __func__, err);
        }
        if (vts_dev->odmrsvd2_thread_exit == true)
            break;
    }

found:
    close(fds[0].fd);
func_exit:
    if (vts_dev->odmrsvd2_thread_exit == false) {
        /* once callback thread is closed set vts_stop using sysfs */
        set_mixer_ctrls(vtsMixerHandle, model_recognize_stop_ctlname,
                odmvoice_reserved2recognize_stop_ctlvalue, MODEL_CONTROL_COUNT, false);

        vts_dev->vts_odmrsvd2_state = VTS_RESERVED2_STOPPED_STATE;

        /* reset DMIC controls */
        set_dmic_ctrls(false);
    }

    vts_dev->odmrsvd2_thread_exit = false;
    printf("%s: Exit \n", __func__);
    return (void *)(long)err;
}

void vts_odmrsvd2_start(struct vts_hw_device *vts_dev)
{
    pthread_mutex_lock(&vts_dev->lock);
    if (vts_dev->vts_odmrsvd2_state == VTS_RESERVED2_STOPPED_STATE) {
        /* Create odmrsvd2 thread to wait on uevent from VTS IP*/
        vts_dev->odmrsvd2_thread_exit = false;
        pthread_create(&vts_dev->odmrsvd2_thread, (const pthread_attr_t *) NULL,
                            odmrsvd2_thread_loop, vts_dev);

        /* configure DMIC controls */
        set_dmic_ctrls(true);

        set_mixer_ctrls(vtsMixerHandle, model_recognize_start_ctlname,
                        odmvoice_reserved2recognize_start_ctlvalue, MODEL_CONTROL_COUNT, false);

        vts_dev->vts_odmrsvd2_state = VTS_RESERVED2_STARTED_STATE;
    } else {
        printf("VTS ODMRsvd2 Already started \n");
    }
    pthread_mutex_unlock(&vts_dev->lock);
    printf("%s: Exit \n", __func__);
    return;
}

void vts_odmrsvd2_stop(struct vts_hw_device *vts_dev)
{
    pthread_mutex_lock(&vts_dev->lock);
    if (vts_dev->vts_odmrsvd2_state == VTS_RESERVED2_STARTED_STATE) {
        /* Stop ODMRsvd2 thread first */
        vts_dev->odmrsvd2_thread_exit = true;
        /* once callback thread is closed disable voice recognition */

        pthread_mutex_unlock(&vts_dev->lock);

        pthread_join(vts_dev->odmrsvd2_thread, (void**)NULL);

        pthread_mutex_lock(&vts_dev->lock);

        set_mixer_ctrls(vtsMixerHandle, model_recognize_stop_ctlname,
                        odmvoice_reserved2recognize_stop_ctlvalue, MODEL_CONTROL_COUNT, false);

        /* configure DMIC controls */
        set_dmic_ctrls(false);

        vts_dev->vts_odmrsvd2_state = VTS_RESERVED2_STOPPED_STATE;
    } else {
        printf("VTS ODMRsvd2 NOT started yet \n");
    }
    pthread_mutex_unlock(&vts_dev->lock);
    printf("%s: Exit \n", __func__);
    return;
}

/****************** Unit test main function *************************/

void print_options(struct vts_hw_device *vts_dev __unused)
{
    printf("********************** Generic Dual VA VTS HW Test Options ***********************\n");
#ifdef MMAP_INTERFACE_ENABLED
    printf("********************** MMAP interface for Model Binary loading***********************\n");
#else
    printf("********************** SYSFS interface for Model Binary loading***********************\n");
#endif
    printf("1. Voice Recoginition Start\n");
    printf("2. Voice Recoginition Stop\n");
    printf("3. VTS Record Start\n");
    printf("4. VTS Record Stop\n");
    printf("5. ODMRsvd2 Mode Start\n");
    printf("6. ODMRsvd2 Mode Stop\n");
    printf("7. Exit - VTS Test Application\n");
    printf("****************************************************************!\n");
    printf("Enter an Option: \n");
    return;
}

int main(void)
{
    struct vts_hw_device *vts_dev = NULL;
    int option, i;
    int ret = EXIT_SUCCESS;
    char pstr[50];
    struct pollfd fds[1];

    vts_dev = &g_vtsdev;
    dmic_usagecnt = 0;

#ifdef MMAP_INTERFACE_ENABLED
    /* Open VTS Misc device for loading Model binary through MMAP interface */
    vtsdev_fd = open("/dev/vts_fio_dev", O_RDWR);
    if (vtsdev_fd < 0) {
        printf("%s: Failed to open VTS-Misc device %d\n", __func__,errno);
        return -EINVAL;
    }

    /* memory map VTS misc driver */
    mapped_addr = mmap(NULL, VTSDRV_MISC_MODEL_BIN_MAXSZ, PROT_READ | PROT_WRITE, MAP_SHARED, vtsdev_fd, 0);
    if (mapped_addr == MAP_FAILED) {
        printf("%s: Unable to MMAP VTS Model downloadable memory \n", __func__);
        close(vtsdev_fd);
        return -EINVAL;
    }
#endif

    /* open mixer control */
    vtsMixerHandle = mixer_open(VTS_MIXER_CARD);
    if (!vtsMixerHandle) {
        printf("%s: Failed to open mixer \n", __func__);
        close(vtsdev_fd);
        return -EINVAL;
    }

    pthread_mutex_lock(&vts_dev->lock);

    vts_dev->vts_state = VTS_RECOGNITION_STOPPED_STATE;
    vts_dev->vts_rec_state = VTS_RECORD_STOPPED_STATE;
    vts_dev->thread_exit = false;
    vts_dev->model_loaded = false;

    memset(fds, 0, sizeof(struct pollfd));
    fds[0].events = POLLIN;
    fds[0].fd = fileno(stdin);

    pthread_mutex_unlock(&vts_dev->lock);

    while (1) {
        print_options(vts_dev);
        ret = poll(fds,1,-1);
        if (fds[0].revents & POLLIN) {
            if (fgets(pstr, 50, stdin) == NULL) {
                printf("Failed to get data from stdin \n");
                continue;
            }
        } else {
            printf("%s - Poll ret value %d\n", __func__, ret);
            continue;
        }
        option = atoi(pstr);
        printf("%s - Selected option %d\n", __func__, option);
        /* Called corresponding function based on Option selected */
        switch (option) {
        case 1: /* Start loaded sound Model Recognition */
            vts_recognition_start(vts_dev);
            break;
        case 2: /* Stop loaded sound Model Recognition */
            vts_recognition_stop(vts_dev);
            break;
        case 3: /* Start VTS recording */
            vts_record_start(vts_dev);
            break;
        case 4: /* Stop VTS recording */
            vts_record_stop(vts_dev);
            break;
        case 5:
            vts_odmrsvd2_start(vts_dev);
            break;
        case 6:
            vts_odmrsvd2_stop(vts_dev);
            break;
        case 7:
            check_vts_state(vts_dev);
            printf("VTS HW Testing completed\n");
            break;

        default:
            printf("UNSUPPORTED Option - Try again !\n");
            break;
        }

        if (option == 7) break;
        option = 0;
    }

    if (vtsMixerHandle) {
        mixer_close(vtsMixerHandle);
        vtsMixerHandle = NULL;
    }

#ifdef MMAP_INTERFACE_ENABLED
    close(vtsdev_fd);
#endif
    return EXIT_SUCCESS;
}
