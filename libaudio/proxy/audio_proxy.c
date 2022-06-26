/*
 * Copyright (C) 2019 The Android Open Source Project
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

#define LOG_TAG "audio_hw_proxy"
#define LOG_NDEBUG 0

//#define VERY_VERY_VERBOSE_LOGGING
#ifdef VERY_VERY_VERBOSE_LOGGING
#define ALOGVV ALOGD
#else
#define ALOGVV(a...) do { } while(0)
#endif

//#define SEAMLESS_DUMP

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <expat.h>

#include <log/log.h>
#include <cutils/str_parms.h>
#include <cutils/properties.h>

#include <audio_utils/channels.h>
#include <audio_utils/primitives.h>
#include <audio_utils/clock.h>
#include <hardware/audio.h>
#include <sound/asound.h>

#include "audio_proxy.h"
#include "audio_proxy_interface.h"
#include "audio_tables.h"
#include "audio_definition.h"
#include "audio_board_info.h"

#define STRM_TYPE stream_table[apstream->stream_type]


/******************************************************************************/
/**                                                                          **/
/** Audio Proxy is Singleton                                                 **/
/**                                                                          **/
/******************************************************************************/

static struct audio_proxy *instance = NULL;

static struct audio_proxy* getInstance(void)
{
    if (instance == NULL) {
        instance = calloc(1, sizeof(struct audio_proxy));
        ALOGI("proxy-%s: created Audio Proxy Instance!", __func__);
    }
    return instance;
}

static void destroyInstance(void)
{
    if (instance) {
        free(instance);
        instance = NULL;
        ALOGI("proxy-%s: destroyed Audio Proxy Instance!", __func__);
    }
    return;
}

/******************************************************************************/
/**                                                                          **/
/** Utility Interfaces                                                       **/
/**                                                                          **/
/******************************************************************************/
int get_supported_device_number(void *proxy, int device_type)
{
    struct audio_proxy *aproxy = (struct audio_proxy *)proxy;
    int ret = 0;

    switch (device_type) {
        case BUILTIN_EARPIECE:
            ret = aproxy->num_earpiece;
            break;

        case BUILTIN_SPEAKER:
            ret = aproxy->num_speaker;
            break;

        case BUILTIN_MIC:
            ret = aproxy->num_mic;
            break;

        case PROXIMITY_SENSOR:
            ret = aproxy->num_proximity;
            break;

        default:
            break;
    }

    return ret;
}

int  get_supported_config(void *proxy, int device_type)
{
    struct audio_proxy *aproxy = (struct audio_proxy *)proxy;
    int ret = DEVICE_CONFIG_NONE;

    switch (device_type) {
        case DEVICE_BLUETOOTH:
            if (aproxy->bt_internal)
                ret = DEVICE_CONFIG_INTERNAL;
            else if (aproxy->bt_external)
                ret = DEVICE_CONFIG_EXTERNAL;
            break;

        case DEVICE_FMRADIO:
            if (aproxy->fm_internal)
                ret = DEVICE_CONFIG_INTERNAL;
            else if (aproxy->fm_external)
                ret = DEVICE_CONFIG_EXTERNAL;
            break;

        default:
            break;
    }

    return ret;
}

bool is_active_usage_CPCall(void *proxy)
{
    struct audio_proxy *aproxy = (struct audio_proxy *)proxy;

    if (aproxy->active_playback_ausage >= AUSAGE_CPCALL_MIN &&
        aproxy->active_playback_ausage <= AUSAGE_CPCALL_MAX)
        return true;
    else
        return false;
}

bool is_usage_CPCall(audio_usage ausage)
{
    if (ausage >= AUSAGE_CPCALL_MIN && ausage <= AUSAGE_CPCALL_MAX)
        return true;
    else
        return false;
}

bool is_active_usage_APCall(void *proxy)
{
    struct audio_proxy *aproxy = (struct audio_proxy *)proxy;

    if (aproxy->active_playback_ausage >= AUSAGE_APCALL_MIN &&
        aproxy->active_playback_ausage <= AUSAGE_APCALL_MAX)
        return true;
    else
        return false;
}

bool is_usage_APCall(audio_usage ausage)
{
    if (ausage >= AUSAGE_APCALL_MIN && ausage <= AUSAGE_APCALL_MAX)
        return true;
    else
        return false;
}
bool is_audiomode_incall(struct audio_proxy *aproxy)
{
    if (aproxy->audio_mode == AUDIO_MODE_IN_CALL)
        return true;
    else
        return false;
}

bool is_audiomode_apcall(struct audio_proxy *aproxy)
{
    if (aproxy->audio_mode == AUDIO_MODE_IN_COMMUNICATION)
        return true;
    else
        return false;
}

/******************************************************************************/
/**                                                                          **/
/** Local Fuctions for Audio Device Proxy                                    **/
/**                                                                          **/
/******************************************************************************/

// If there are specific device number in mixer_paths.xml, it get the specific device number from mixer_paths.xml
static int get_pcm_device_number(void *proxy, void *proxy_stream)
{
    struct audio_proxy *aproxy = proxy;
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    int pcm_device_number = -1;

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);
    if (apstream) {
        switch(apstream->stream_type) {
            case ASTREAM_PLAYBACK_PRIMARY:
                pcm_device_number = PRIMARY_PLAYBACK_DEVICE;
                break;

            case ASTREAM_PLAYBACK_FAST:
                pcm_device_number = FAST_PLAYBACK_DEVICE;
                break;

            case ASTREAM_PLAYBACK_LOW_LATENCY:
                pcm_device_number = LOW_PLAYBACK_DEVICE;
                break;

            case ASTREAM_PLAYBACK_DEEP_BUFFER:
                pcm_device_number = DEEP_PLAYBACK_DEVICE;
                break;
            case ASTREAM_PLAYBACK_MMAP:
                pcm_device_number = MMAP_PLAYBACK_DEVICE;
                break;

            case ASTREAM_PLAYBACK_AUX_DIGITAL:
                pcm_device_number = AUX_PLAYBACK_DEVICE;
                break;

            case ASTREAM_PLAYBACK_INCALL_MUSIC:
                pcm_device_number = INCALLMUSIC_PLAYBACK_DEVICE;
                break;

            case ASTREAM_CAPTURE_PRIMARY:
                pcm_device_number = PRIMARY_CAPTURE_DEVICE;
                break;

            case ASTREAM_CAPTURE_CALL:
                pcm_device_number = CALL_RECORD_DEVICE;
                break;

            case ASTREAM_CAPTURE_LOW_LATENCY:
                pcm_device_number = LOW_CAPTURE_DEVICE;
                break;

            case ASTREAM_CAPTURE_MMAP:
                pcm_device_number = MMAP_CAPTURE_DEVICE;
                break;

            case ASTREAM_CAPTURE_FM_TUNER:
                pcm_device_number = FM_RECORD_DEVICE;
                break;

            case ASTREAM_CAPTURE_FM_RECORDING:
                pcm_device_number = FM_RECORD_DEVICE;
                break;

            default:
                break;
        }
    } else {
    }
    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return pcm_device_number;
}

static void disable_callscreen_loopback(void *proxy)
{
    ALOGI("proxy-%s: Call Screen %s with SR(%u)",
            __func__, ABOX_SAMPLE_RATE_WDMA1_NAME, INCALL_LOOPBACK_SAMPLING_RATE);

    proxy_set_mixer_value_int(proxy, ABOX_SAMPLE_RATE_WDMA1_NAME, DEFAULT_FM_REC_SAMPLINGRATE);

    return;
}
#if 0
static void enable_callscreen_loopback(void *proxy)
{
    ALOGI("proxy-%s: Call Screen %s with SR(%u)",
            __func__, ABOX_SAMPLE_RATE_WDMA1_NAME, INCALL_LOOPBACK_SAMPLING_RATE);

    proxy_set_mixer_value_int(proxy, ABOX_SAMPLE_RATE_WDMA1_NAME, INCALL_LOOPBACK_SAMPLING_RATE);

    return;
}
#endif

// Specific Mixer Control Functions for Internal Loopback Handling
static void proxy_set_mixercontrol(struct audio_proxy *aproxy, erap_trigger type, int value)
{
    struct mixer_ctl *ctrl = NULL;
    char mixer_name[MAX_MIXER_NAME_LEN];
    int ret = 0, val = value;

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);

    if (type == MUTE_CONTROL) {
        ctrl = mixer_get_ctl_by_name(aproxy->mixer, ABOX_MUTE_CONTROL_NAME);
        snprintf(mixer_name, sizeof(mixer_name), ABOX_MUTE_CONTROL_NAME);
    } else if (type == TICKLE_CONTROL) {
        ctrl = mixer_get_ctl_by_name(aproxy->mixer, ABOX_TICKLE_CONTROL_NAME);
        snprintf(mixer_name, sizeof(mixer_name), ABOX_TICKLE_CONTROL_NAME);
    }

    if (ctrl) {
        ret = mixer_ctl_set_value(ctrl, 0,val);
        if (ret != 0)
            ALOGE("proxy-%s: failed to set Mixer Control(%s)", __func__, mixer_name);
        else
            ALOGI("proxy-%s: set Mixer Control(%s) to %d", __func__, mixer_name, val);
    } else {
        ALOGE("proxy-%s: cannot find Mixer Control", __func__);
    }

    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return ;
}

// Voice Call PCM Handler
static void voice_rx_stop(struct audio_proxy *aproxy)
{
    char pcm_path[MAX_PCM_PATH_LEN];

    /* Disables Voice Call RX Playback Stream */
    if (aproxy->call_rx) {
        snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c",
                 VRX_PLAYBACK_CARD, VRX_PLAYBACK_DEVICE, 'p');

        pcm_stop(aproxy->call_rx);
        pcm_close(aproxy->call_rx);
        aproxy->call_rx = NULL;

        ALOGI("proxy-%s: Voice Call RX PCM Device(%s) is stopped & closed!", __func__, pcm_path);
    }
}

static int voice_rx_start(struct audio_proxy *aproxy)
{
    struct pcm_config pcmconfig = pcm_config_voicerx_playback;
    char pcm_path[MAX_PCM_PATH_LEN];

    /* Enables Voice Call RX Playback Stream */
    if (aproxy->call_rx == NULL) {
        snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c",
                 VRX_PLAYBACK_CARD, VRX_PLAYBACK_DEVICE, 'p');

        aproxy->call_rx = pcm_open(VRX_PLAYBACK_CARD, VRX_PLAYBACK_DEVICE,
                                   PCM_OUT | PCM_MONOTONIC, &pcmconfig);
        if (aproxy->call_rx && !pcm_is_ready(aproxy->call_rx)) {
            /* pcm_open does always return pcm structure, not NULL */
            ALOGE("proxy-%s: Voice Call RX PCM Device(%s) with SR(%u) PF(%d) CC(%d) is not ready as error(%s)",
                  __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels,
                  pcm_get_error(aproxy->call_rx));
            goto err_open;
        }
        ALOGI("proxy-%s: Voice Call RX PCM Device(%s) with SR(%u) PF(%d) CC(%d) is opened",
              __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels);

        if (pcm_start(aproxy->call_rx) == 0) {
            ALOGI("proxy-%s: Voice Call RX PCM Device(%s) with SR(%u) PF(%d) CC(%d) is started",
                  __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels);
        } else {
            ALOGE("proxy-%s: Voice Call RX PCM Device(%s) with SR(%u) PF(%d) CC(%d) cannot be started as error(%s)",
                  __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels,
                  pcm_get_error(aproxy->call_rx));
            goto err_open;
        }
    }
    return 0;

err_open:
    voice_rx_stop(aproxy);
    return -1;
}

static void voice_tx_stop(struct audio_proxy *aproxy)
{
    char pcm_path[MAX_PCM_PATH_LEN];

    /* Disables Voice Call TX Capture Stream */
    if (aproxy->call_tx) {
        snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c",
                 VTX_CAPTURE_CARD, VTX_CAPTURE_DEVICE, 'c');

        pcm_stop(aproxy->call_tx);
        pcm_close(aproxy->call_tx);
        aproxy->call_tx = NULL;

        ALOGI("proxy-%s: Voice Call TX PCM Device(%s) is stopped & closed!", __func__, pcm_path);
    }
}

static int voice_tx_start(struct audio_proxy *aproxy)
{
    struct pcm_config pcmconfig = pcm_config_voicetx_capture;
    char pcm_path[MAX_PCM_PATH_LEN];

    /* Enables Voice Call TX Capture Stream */
    if (aproxy->call_tx == NULL) {
        snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c",
                 VTX_CAPTURE_CARD, VTX_CAPTURE_DEVICE, 'c');

        aproxy->call_tx = pcm_open(VTX_CAPTURE_CARD, VTX_CAPTURE_DEVICE,
                                   PCM_IN | PCM_MONOTONIC, &pcmconfig);
        if (aproxy->call_tx && !pcm_is_ready(aproxy->call_tx)) {
            /* pcm_open does always return pcm structure, not NULL */
            ALOGE("proxy-%s: Voice Call TX PCM Device(%s) with SR(%u) PF(%d) CC(%d) is not ready as error(%s)",
                  __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels,
                  pcm_get_error(aproxy->call_tx));
            goto err_open;
        }
        ALOGI("proxy-%s: Voice Call TX PCM Device(%s) with SR(%u) PF(%d) CC(%d) is opened",
              __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels);

        if (pcm_start(aproxy->call_tx) == 0) {
            ALOGI("proxy-%s: Voice Call TX PCM Device(%s) with SR(%u) PF(%d) CC(%d) is started",
                  __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels);
        } else {
            ALOGE("proxy-%s: Voice Call TX PCM Device(%s) with SR(%u) PF(%d) CC(%d) cannot be started as error(%s)",
                  __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels,
                  pcm_get_error(aproxy->call_tx));
            goto err_open;
        }
    }
    return 0;

err_open:
    voice_tx_stop(aproxy);

    return -1;
}

static void disable_internal_path(void *proxy, int ausage, device_type target_device, device_type new_device)
{

    struct audio_proxy *aproxy = proxy;

    /* skip internal pcm controls for VoiceCall bandwidth change */
    if (aproxy->vcall_bandchange) {
        ALOGI("proxy-%s: skip for bandwidth change routing", __func__);
        return ;
    }

    /* Disable CallScreen loopback path */
    if (ausage == AUSAGE_INCALL_MUSIC && target_device >= DEVICE_MAIN_MIC) {
        disable_callscreen_loopback(aproxy);
    }

    return ;
}

static void enable_internal_path(void *proxy, int ausage, device_type target_device)
{
    struct audio_proxy *aproxy = proxy;

    if (aproxy == NULL)
        return ;

    /* Enable CallScreen loopback path */
    if (ausage == AUSAGE_INCALL_MUSIC && target_device >= DEVICE_MAIN_MIC) {
        //enable_callscreen_loopback(aproxy);
    }

    return ;
}

struct mixer {
    int fd;
    struct snd_ctl_card_info card_info;
    struct snd_ctl_elem_info *elem_info;
    struct mixer_ctl *ctl;
    unsigned int count;
};

static struct snd_ctl_event *mixer_read_event(struct mixer *mixer, unsigned int mask)
{
    struct snd_ctl_event *ev;

    if (!mixer)
        return 0;

    ev = calloc(1, sizeof(*ev));
    if (!ev)
        return 0;

    while (read(mixer->fd, ev, sizeof(*ev)) > 0) {
        if (ev->type != SNDRV_CTL_EVENT_ELEM)
            continue;

        if (!(ev->data.elem.mask & mask))
            continue;

        return ev;
    }

    free(ev);
    return 0;
}

static int audio_route_missing_ctl(struct audio_route *ar) {
    return 0;
}

/* Mask for mixer_read_event()
 * It should be same with SNDRV_CTL_EVENT_MASK_* in asound.h.
 */
#define MIXER_EVENT_VALUE    (1 << 0)
#define MIXER_EVENT_INFO     (1 << 1)
#define MIXER_EVENT_ADD      (1 << 2)
#define MIXER_EVENT_TLV      (1 << 3)
#define MIXER_EVENT_REMOVE   (~0U)

static void *mixer_update_loop(void *context)
{
    struct audio_proxy *aproxy = (struct audio_proxy *)context;
    struct snd_ctl_event *event = NULL;
    struct timespec ts_start, ts_tick;

    ALOGI("proxy-%s: started running Mixer Updater Thread", __func__);

    clock_gettime(CLOCK_MONOTONIC, &ts_start);
    do {
        if (aproxy->mixer) {
            event = mixer_read_event(aproxy->mixer, MIXER_EVENT_ADD);
            if (!event) {
                clock_gettime(CLOCK_MONOTONIC, &ts_tick);
                if ((ts_tick.tv_sec - ts_start.tv_sec) > MIXER_UPDATE_TIMEOUT) {
                    ALOGI("proxy-%s: Mixer Update Timeout, it will be destroyed", __func__);
                    break;
                }
                continue;
            }
        } else
            continue;

        pthread_rwlock_wrlock(&aproxy->mixer_update_lock);

        mixer_close(aproxy->mixer);
        aproxy->mixer = mixer_open(MIXER_CARD0);
        if (!aproxy->mixer)
            ALOGE("proxy-%s: failed to re-open Mixer", __func__);

        mixer_subscribe_events(aproxy->mixer, 1);
        audio_route_free(aproxy->aroute);
        aproxy->aroute = audio_route_init(MIXER_CARD0, aproxy->xml_path);
        if (!aproxy->aroute)
            ALOGE("proxy-%s: failed to re-init audio route", __func__);

        ALOGI("proxy-%s: mixer and route are updated", __func__);

        pthread_rwlock_unlock(&aproxy->mixer_update_lock);
        free(event);
    } while (aproxy->mixer && aproxy->aroute && audio_route_missing_ctl(aproxy->aroute));

    ALOGI("proxy-%s: all mixer controls are found", __func__);

    if (aproxy->mixer)
        mixer_subscribe_events(aproxy->mixer, 0);

    ALOGI("proxy-%s: stopped running Mixer Updater Thread", __func__);
    return NULL;
}

static void make_path(audio_usage ausage, device_type device, char *path_name)
{
    memset(path_name, 0, MAX_PATH_NAME_LEN);
    strlcpy(path_name, usage_path_table[ausage], MAX_PATH_NAME_LEN);
    if (strlen(device_table[device]) > 0) {
        strlcat(path_name, "-", MAX_PATH_NAME_LEN);
        strlcat(path_name, device_table[device], MAX_PATH_NAME_LEN);
    }

    return ;
}

static void make_gain(char *path_name, char *gain_name)
{
    memset(gain_name, 0, MAX_GAIN_PATH_NAME_LEN);
    strlcpy(gain_name, "gain-", MAX_PATH_NAME_LEN);
    strlcat(gain_name, path_name, MAX_PATH_NAME_LEN);

    return ;
}

/* Enable new Audio Path */
static void set_route(void *proxy, audio_usage ausage, device_type device)
{
    struct audio_proxy *aproxy = proxy;
    char path_name[MAX_PATH_NAME_LEN];
    char gain_name[MAX_GAIN_PATH_NAME_LEN];

    if (device == DEVICE_AUX_DIGITAL)
        return ;

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);

    make_path(ausage, device, path_name);
    audio_route_apply_and_update_path(aproxy->aroute, path_name);
    ALOGI("proxy-%s: routed to %s", __func__, path_name);

    make_gain(path_name, gain_name);
    audio_route_apply_and_update_path(aproxy->aroute, gain_name);
    ALOGI("proxy-%s: set gain as %s", __func__, gain_name);

    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return ;
}

/* reroute Audio Path */
static void set_reroute(void *proxy, audio_usage old_ausage, device_type old_device,
                                     audio_usage new_ausage, device_type new_device)
{
    struct audio_proxy *aproxy = proxy;
    char path_name[MAX_PATH_NAME_LEN];
    char gain_name[MAX_GAIN_PATH_NAME_LEN];

    ALOGI("proxy-%s: enter", __func__);

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);

    // 1. Unset Active Route
    make_path(old_ausage, old_device, path_name);
    audio_route_reset_and_update_path(aproxy->aroute, path_name);
    ALOGI("proxy-%s: unrouted %s", __func__, path_name);

    make_gain(path_name, gain_name);
    audio_route_reset_and_update_path(aproxy->aroute, gain_name);
    ALOGI("proxy-%s: reset gain %s", __func__, gain_name);

    // 2. Set New Route
    if (new_device != DEVICE_AUX_DIGITAL) {
        make_path(new_ausage, new_device, path_name);
        audio_route_apply_and_update_path(aproxy->aroute, path_name);
        ALOGI("proxy-%s: routed %s", __func__, path_name);

        make_gain(path_name, gain_name);
        audio_route_apply_and_update_path(aproxy->aroute, gain_name);
        ALOGI("proxy-%s: set gain as %s", __func__, gain_name);
    }

    // 3. Update Other extra Mixers
    audio_route_update_mixer(aproxy->aroute);

    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return ;
}

/* Disable Audio Path */
static void reset_route(void *proxy, audio_usage ausage, device_type device)
{
    struct audio_proxy *aproxy = proxy;
    char path_name[MAX_PATH_NAME_LEN];
    char gain_name[MAX_GAIN_PATH_NAME_LEN];

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);

    make_path(ausage, device, path_name);
    audio_route_reset_and_update_path(aproxy->aroute, path_name);
    ALOGI("proxy-%s: unrouted %s", __func__, path_name);

    make_gain(path_name, gain_name);
    audio_route_reset_and_update_path(aproxy->aroute, gain_name);
    ALOGI("proxy-%s: reset gain %s", __func__, gain_name);

    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return ;
}

/* Enable new Modifier */
static void set_modifier(void *proxy, modifier_type modifier)
{
    struct audio_proxy *aproxy = proxy;

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);

    audio_route_apply_and_update_path(aproxy->aroute, modifier_table[modifier]);
    ALOGI("proxy-%s: enabled to %s", __func__, modifier_table[modifier]);

    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return ;
}

/* Update Modifier */
static void update_modifier(void *proxy, modifier_type old_modifier, modifier_type new_modifier)
{
    struct audio_proxy *aproxy = proxy;

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);

    // 1. Unset Active Modifier
    audio_route_reset_and_update_path(aproxy->aroute, modifier_table[old_modifier]);
    ALOGI("proxy-%s: disabled %s", __func__, modifier_table[old_modifier]);

    // 2. Set New Modifier
    audio_route_apply_and_update_path(aproxy->aroute, modifier_table[new_modifier]);
    ALOGI("proxy-%s: enabled %s", __func__, modifier_table[new_modifier]);

    // 3. Update Mixers
    audio_route_update_mixer(aproxy->aroute);

    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return ;
}

/* Disable Modifier */
static void reset_modifier(void *proxy, modifier_type modifier)
{
    struct audio_proxy *aproxy = proxy;

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);

    audio_route_reset_and_update_path(aproxy->aroute, modifier_table[modifier]);
    ALOGI("proxy-%s: disabled %s", __func__, modifier_table[modifier]);

    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return ;
}

void clr_call_path_param(void)
{
    struct audio_proxy *aproxy = getInstance();

    ALOGI("proxy-%s: enter", __func__);

    aproxy->call_param_idx.NSEC = 0;
    aproxy->call_param_idx.ext_vol = 0;
    aproxy->call_param_idx.apcall_app = 0;
    aproxy->call_param_idx.special = 0;
    aproxy->call_param_idx.device = 0;
    aproxy->call_param_idx.sample_rate = 0;
    aproxy->call_param_idx.mic_num = 0;
    aproxy->call_param_idx.channel = 0;
    aproxy->call_param_idx.call_type = 0;
}

void set_call_path_param(void)
{
    struct audio_proxy *aproxy = getInstance();
    uint32_t *call_path_param = NULL;

    // can't get these params, for now
    aproxy->call_param_idx.channel = STEREO;
    aproxy->call_param_idx.mic_num = MIC1;
    aproxy->call_param_idx.special = 0;

    call_path_param = (uint32_t *)&aproxy->call_param_idx;

    ALOGI("proxy-%s: Call Path Parameter IDX %x", __func__, *call_path_param);

    proxy_set_mixer_value_int(aproxy, ABOX_CALL_PATH_PARAM, *call_path_param);
}

/*
 * Dump functions
 */
static void calliope_cleanup_old(const char *path, const char *prefix)
{
    struct dirent **namelist;
    int n, match = 0;

    ALOGV("proxy-%s", __func__);

    n = scandir(path, &namelist, NULL, alphasort);
    if (n > 0) {
        /* interate in reverse order to get old file */
        while (n--) {
            if (strstr(namelist[n]->d_name, prefix) == namelist[n]->d_name) {
                if (++match > ABOX_DUMP_LIMIT) {
                    char *tgt;

                    if (asprintf(&tgt, "%s/%s", path, namelist[n]->d_name) != -1) {
                        remove(tgt);
                        free(tgt);
                    }
                }
            }
            free(namelist[n]);
        }
        free(namelist);
    }

    return ;
}

static void __calliope_dump(int fd, const char *in_prefix, const char *in_file,
                            const char *out_prefix, const char *out_suffix)
{
    static const int buf_size = 4096;
    char *buf, in_path[128], out_path[128];
    int fd_in = -1, fd_out = -1, n;
    mode_t mask;

    ALOGV("proxy-%s", __func__);

    if (snprintf(in_path, sizeof(in_path) - 1, "%s%s", in_prefix, in_file) < 0) {
        ALOGE("proxy-%s: in path error: %s", __func__, strerror(errno));
        return;
    }

    if (snprintf(out_path, sizeof(out_path) - 1, "%s%s_%s.bin", out_prefix, in_file, out_suffix) < 0) {
        ALOGE("proxy-%s: out path error: %s", __func__, strerror(errno));
        return;
    }

    buf = malloc(buf_size);
    if (!buf) {
        ALOGE("proxy-%s: malloc failed: %s", __func__, strerror(errno));
        return;
    }

    mask = umask(0);
    ALOGV("umask = %o", mask);

    fd_in = open(in_path, O_RDONLY | O_NONBLOCK);
    if (fd_in < 0)
        ALOGE("proxy-%s: open error: %s, fd_in=%s", __func__, strerror(errno), in_path);

    fd_out = open(out_path, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
    if (fd_out < 0)
        ALOGE("proxy-%s: open error: %s, fd_out=%s", __func__, strerror(errno), out_path);

    if (fd_in >= 0 && fd_out >= 0) {
        while((n = read(fd_in, buf, buf_size)) > 0) {
            if (write(fd_out, buf, n) < 0) {
                ALOGE("proxy-%s: write error: %s", __func__, strerror(errno));
            }
        }
        n = snprintf(buf, buf_size, " %s_%s.bin <= %s\n", in_file, out_suffix, in_file);
        write(fd, buf, n);
        ALOGI("proxy-%s", buf);
    }

    calliope_cleanup_old(out_prefix, in_file);

    if (fd_in >= 0)
        close(fd_in);
    if (fd_out >= 0)
        close(fd_out);

    mask = umask(mask);
    free(buf);

    return;
}

static void calliope_ramdump(int fd)
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

    write(fd, "\n", strlen("\n"));
    write(fd, "Calliope snapshot:\n", strlen("Calliope snapshot:\n"));
    ALOGI("Calliope snapshot:\n");

    __calliope_dump(fd, CALLIOPE_DBG_PATH, CALLIOPE_LOG, ABOX_DUMP, str_time);
    __calliope_dump(fd, SYSFS_PREFIX ABOX_DEV ABOX_DEBUG, ABOX_GPR, ABOX_DUMP, str_time);
    __calliope_dump(fd, SYSFS_PREFIX ABOX_DEV ABOX_DEBUG, ABOX_SRAM, ABOX_DUMP, str_time);
    __calliope_dump(fd, SYSFS_PREFIX ABOX_DEV ABOX_DEBUG, ABOX_DRAM, ABOX_DUMP, str_time);
    __calliope_dump(fd, SYSFS_PREFIX ABOX_DEV ABOX_DEBUG, ABOX_SLOG, ABOX_DUMP, str_time);
    __calliope_dump(fd, ABOX_REGMAP_PATH, ABOX_REG_FILE, ABOX_DUMP, str_time);
    write(fd, "Calliope snapshot done\n", strlen("Calliope snapshot done\n"));

    return ;
}


/******************************************************************************/
/**                                                                          **/
/** Local Functions for Audio Stream Proxy                                   **/
/**                                                                          **/
/******************************************************************************/
static void save_written_frames(struct audio_proxy_stream *apstream, int bytes)
{
    apstream->frames += bytes / (apstream->pcmconfig.channels *
                audio_bytes_per_sample(audio_format_from_pcm_format(apstream->pcmconfig.format)));
    ALOGVV("%s-%s: written = %u frames", stream_table[apstream->stream_type], __func__,
                                         (unsigned int)apstream->frames);
    return ;
}

static void skip_pcm_processing(struct audio_proxy_stream *apstream, int bytes)
{
    unsigned int frames = 0;

    frames = bytes / (apstream->pcmconfig.channels *
             audio_bytes_per_sample(audio_format_from_pcm_format(apstream->pcmconfig.format)));
    usleep(frames * 1000000 / proxy_get_actual_sampling_rate(apstream));
    return ;
}

static void update_capture_pcmconfig(struct audio_proxy_stream *apstream)
{
    int i;

    // Check Sampling Rate
    for (i = 0; i < MAX_NUM_CAPTURE_SR; i++) {
        if (apstream->requested_sample_rate == supported_capture_samplingrate[i]) {
            if (apstream->requested_sample_rate != apstream->pcmconfig.rate) {
                apstream->pcmconfig.rate = apstream->requested_sample_rate;
                if (apstream->stream_type == ASTREAM_CAPTURE_PRIMARY)
                    apstream->pcmconfig.period_size = (apstream->pcmconfig.rate * PREDEFINED_MEDIA_CAPTURE_DURATION) / 1000;
                else if (apstream->stream_type == ASTREAM_CAPTURE_LOW_LATENCY)
                    apstream->pcmconfig.period_size = (apstream->pcmconfig.rate * PREDEFINED_LOW_CAPTURE_DURATION) / 1000;

                // WDMA in A-Box is 128-bit aligned, so period_size has to be multiple of 4 frames
                apstream->pcmconfig.period_size &= 0xFFFFFFFC;
                ALOGD("%s-%s: updates samplig rate to %u, period_size to %u", stream_table[apstream->stream_type],
                                                           __func__, apstream->pcmconfig.rate,
                                                           apstream->pcmconfig.period_size);
            }
            break;
        }
    }

    if (i == MAX_NUM_CAPTURE_SR)
        ALOGD("%s-%s: needs re-sampling to %u", stream_table[apstream->stream_type], __func__,
                                                apstream->requested_sample_rate);

    // Check Channel Mask
    for (i = 0; i < MAX_NUM_CAPTURE_CM; i++) {
        if (apstream->requested_channel_mask == supported_capture_channelmask[i]) {
            if (audio_channel_count_from_in_mask(apstream->requested_channel_mask)
                != apstream->pcmconfig.channels) {
                apstream->pcmconfig.channels = audio_channel_count_from_in_mask(apstream->requested_channel_mask);
                ALOGD("%s-%s: updates channel count to %u", stream_table[apstream->stream_type],
                                                            __func__, apstream->pcmconfig.channels);
            }
            break;
        }
    }

    if (i == MAX_NUM_CAPTURE_CM)
        ALOGD("%s-%s: needs re-channeling to %u from %u", stream_table[apstream->stream_type], __func__,
              audio_channel_count_from_in_mask(apstream->requested_channel_mask), apstream->pcmconfig.channels);

    // Check PCM Format
    for (i = 0; i < MAX_NUM_CAPTURE_PF; i++) {
        if (apstream->requested_format == supported_capture_pcmformat[i]) {
            if (pcm_format_from_audio_format(apstream->requested_format) != apstream->pcmconfig.format) {
                apstream->pcmconfig.format = pcm_format_from_audio_format(apstream->requested_format);
                ALOGD("%s-%s: updates PCM format to %d", stream_table[apstream->stream_type], __func__,
                                                         apstream->pcmconfig.format);
            }
            break;
        }
    }

    if (i == MAX_NUM_CAPTURE_PF)
        ALOGD("%s-%s: needs re-formating to 0x%x", stream_table[apstream->stream_type], __func__,
                                                   apstream->requested_format);

    return ;
}

// For Resampler
static int proxy_get_requested_frame_size(struct audio_proxy_stream *apstream)
{
    return audio_channel_count_from_in_mask(apstream->requested_channel_mask) *
           audio_bytes_per_sample(apstream->requested_format);
}

static int get_next_buffer(struct resampler_buffer_provider *buffer_provider,
                           struct resampler_buffer* buffer)
{
    struct audio_proxy_stream *apstream;

    if (buffer_provider == NULL || buffer == NULL)
        return -EINVAL;

    apstream = (struct audio_proxy_stream *)((char *)buffer_provider -
                                             offsetof(struct audio_proxy_stream, buf_provider));

    if (apstream->pcm) {
        if (apstream->read_buf_frames == 0) {
            unsigned int size_in_bytes = pcm_frames_to_bytes(apstream->pcm, apstream->pcmconfig.period_size);
            if (apstream->actual_read_buf_size < size_in_bytes) {
                apstream->actual_read_buf_size = size_in_bytes;
                apstream->actual_read_buf = (int16_t *) realloc(apstream->actual_read_buf, size_in_bytes);
                if (apstream->actual_read_buf != NULL)
                    ALOGI("%s-%s: alloc actual read buffer with %u bytes",
                           stream_table[apstream->stream_type], __func__, size_in_bytes);
            }

            if (apstream->actual_read_buf != NULL) {
            apstream->actual_read_status = pcm_read(apstream->pcm, (void*)apstream->actual_read_buf, size_in_bytes);
            if (apstream->actual_read_status != 0) {
                ALOGE("%s-%s:  pcm_read error %d", stream_table[apstream->stream_type], __func__,
                                                   apstream->actual_read_status);
                buffer->raw = NULL;
                buffer->frame_count = 0;
                return apstream->actual_read_status;
            }

            if (apstream->stream_type == ASTREAM_CAPTURE_CALL) {
                /*
                 * [Call Recording Case]
                 * In case of Call Recording, A-Box sends stereo stream which uplink/downlink voice
                 * allocated in left/right to AudioHAL.
                 * AudioHAL has to select and mix uplink/downlink voice from left/right channel as usage.
                 */
                int16_t data_mono;
                int16_t *vc_buf = (int16_t *)(apstream->actual_read_buf);

                // Channel Selection
                // output : Stereo with Left/Right contains same selected channel PCM & Device SR
                for (unsigned int i = 0; i < apstream->pcmconfig.period_size; i++){
                    if (apstream->stream_usage == AUSAGE_INCALL_UPLINK)
                        data_mono = (*(vc_buf + 2*i + 1)); // Tx
                    else if (apstream->stream_usage == AUSAGE_INCALL_DOWNLINK){
                        data_mono = (*(vc_buf + 2*i));     // Rx
                    } else {
                            data_mono = clamp16(((int32_t)*(vc_buf+2*i) + (int32_t)*(vc_buf+2*i+1))); // mix Rx/Tx
                    }

                    *(vc_buf + 2*i)     = data_mono;
                    *(vc_buf + 2*i + 1) = data_mono;
                }
            }

            apstream->read_buf_frames = apstream->pcmconfig.period_size;
            } else {
                ALOGE("%s-%s: failed to reallocate actual_read_buf",
                      stream_table[apstream->stream_type], __func__);
                buffer->raw = NULL;
                buffer->frame_count = 0;
                apstream->actual_read_status = -ENOMEM;
                return -ENOMEM;
            }
        }

        buffer->frame_count = (buffer->frame_count > apstream->read_buf_frames) ?
                               apstream->read_buf_frames : buffer->frame_count;
        buffer->i16 = apstream->actual_read_buf + (apstream->pcmconfig.period_size - apstream->read_buf_frames) *
                                                  apstream->pcmconfig.channels;
        return apstream->actual_read_status;
    } else {
        buffer->raw = NULL;
        buffer->frame_count = 0;
        apstream->actual_read_status = -ENODEV;
        return -ENODEV;
    }
}

static void release_buffer(struct resampler_buffer_provider *buffer_provider,
                           struct resampler_buffer* buffer)
{
    struct audio_proxy_stream *apstream;

    if (buffer_provider == NULL || buffer == NULL)
        return;

    apstream = (struct audio_proxy_stream *)((char *)buffer_provider -
                                             offsetof(struct audio_proxy_stream, buf_provider));

    apstream->read_buf_frames -= buffer->frame_count;
}

static int read_frames(struct audio_proxy_stream *apstream, void *buffer, int frames)
{
    int frames_wr = 0;

    while (frames_wr < frames) {
        size_t frames_rd = frames - frames_wr;
        ALOGVV("%s-%s: frames_rd: %zd, frames_wr: %d",
           stream_table[apstream->stream_type], __func__, frames_rd, frames_wr);

        if (apstream->resampler != NULL) {
            apstream->resampler->resample_from_provider(apstream->resampler,
            (int16_t *)((char *)buffer + pcm_frames_to_bytes(apstream->pcm, frames_wr)), &frames_rd);
        } else {
            struct resampler_buffer buf;
            buf.raw= NULL;
            buf.frame_count = frames_rd;

            get_next_buffer(&apstream->buf_provider, &buf);
            if (buf.raw != NULL) {
                memcpy((char *)buffer + pcm_frames_to_bytes(apstream->pcm, frames_wr),
                        buf.raw, pcm_frames_to_bytes(apstream->pcm, buf.frame_count));
                frames_rd = buf.frame_count;
            }
            release_buffer(&apstream->buf_provider, &buf);
        }

        /* apstream->actual_read_status is updated by getNextBuffer() also called by
         * apstream->resampler->resample_from_provider() */
        if (apstream->actual_read_status != 0)
            return apstream->actual_read_status;

        frames_wr += frames_rd;
    }

    return frames_wr;
}

static int read_and_process_frames(struct audio_proxy_stream *apstream, void* buffer, int frames_num)
{
    int frames_wr = 0;
    unsigned int bytes_per_sample = (pcm_format_to_bits(apstream->pcmconfig.format) >> 3);
    void *proc_buf_out = buffer;

    int num_device_channels = proxy_get_actual_channel_count(apstream);
    int num_req_channels = audio_channel_count_from_in_mask(apstream->requested_channel_mask);

    /* Prepare Channel Conversion Input Buffer */
    if (apstream->need_monoconversion && (num_device_channels != num_req_channels)) {
        int src_buffer_size = frames_num * num_device_channels * bytes_per_sample;

        if (apstream->proc_buf_size < src_buffer_size) {
            apstream->proc_buf_size = src_buffer_size;
            apstream->proc_buf_out = realloc(apstream->proc_buf_out, src_buffer_size);
            ALOGI("%s-%s: alloc resampled read buffer with %d bytes",
                      stream_table[apstream->stream_type], __func__, src_buffer_size);
        }
        proc_buf_out = apstream->proc_buf_out;
    }

    frames_wr = read_frames(apstream, proc_buf_out, frames_num);
    if ((frames_wr > 0) && (frames_wr > frames_num))
        ALOGE("%s-%s: read more frames than requested", stream_table[apstream->stream_type], __func__);

    /*
     * A-Box can support only Stereo channel, not Mono channel.
     * If platform wants Mono Channel Recording, AudioHAL has to support mono conversion.
     */
    if (apstream->actual_read_status == 0) {
    if (apstream->need_monoconversion && (num_device_channels != num_req_channels)) {
        size_t ret = adjust_channels(proc_buf_out, num_device_channels,
                                     buffer, num_req_channels,
                                     bytes_per_sample, (frames_wr * num_device_channels * bytes_per_sample));
            if (ret != (frames_wr * num_req_channels * bytes_per_sample))
                ALOGE("%s-%s: channel convert failed", stream_table[apstream->stream_type], __func__);
        }
    } else {
        ALOGE("%s-%s: Read Fail = %d", stream_table[apstream->stream_type], __func__, frames_wr);
    }

    return frames_wr;
}

static void check_conversion(struct audio_proxy_stream *apstream)
{
    int request_cc = audio_channel_count_from_in_mask(apstream->requested_channel_mask);

    // Check Mono/stereo Conversion is needed or not
    if ((request_cc == MEDIA_1_CHANNEL && apstream->pcmconfig.channels == DEFAULT_MEDIA_CHANNELS)
        || (request_cc == DEFAULT_MEDIA_CHANNELS && apstream->pcmconfig.channels == MEDIA_1_CHANNEL)) {
        // Only support Stereo to Mono Conversion
        apstream->need_monoconversion = true;
        ALOGD("%s-%s: needs re-channeling to %u from %u", stream_table[apstream->stream_type], __func__,
              request_cc, apstream->pcmconfig.channels);
    }

    // Check Re-Sampler is needed or not
    if (apstream->requested_sample_rate &&
        apstream->requested_sample_rate != apstream->pcmconfig.rate) {
        // Only support Stereo Resampling
        if (apstream->resampler) {
            release_resampler(apstream->resampler);
            apstream->resampler = NULL;
        }

        apstream->buf_provider.get_next_buffer = get_next_buffer;
        apstream->buf_provider.release_buffer = release_buffer;
        int ret = create_resampler(apstream->pcmconfig.rate, apstream->requested_sample_rate,
                                   apstream->pcmconfig.channels, RESAMPLER_QUALITY_DEFAULT,
                                   &apstream->buf_provider, &apstream->resampler);
        if (ret !=0) {
            ALOGE("proxy-%s: failed to create resampler", __func__);
        } else {
            ALOGV("proxy-%s: resampler created in-samplerate %d out-samplereate %d",
                  __func__, apstream->pcmconfig.rate, apstream->requested_sample_rate);

            apstream->need_resampling = true;
            ALOGD("%s-%s: needs re-sampling to %u Hz from %u Hz", stream_table[apstream->stream_type], __func__,
                  apstream->requested_sample_rate, apstream->pcmconfig.rate);

            apstream->actual_read_buf = NULL;
            apstream->actual_read_buf_size = 0;
            apstream->read_buf_frames = 0;

            apstream->resampler->reset(apstream->resampler);
        }
    }

    return ;
}

/*
 * Modify config->period_count based on min_size_frames
 */
static void adjust_mmap_period_count(struct audio_proxy_stream *apstream, struct pcm_config *config,
                                     int32_t min_size_frames)
{
    int periodCountRequested = (min_size_frames + config->period_size - 1)
                               / config->period_size;
    int periodCount = MMAP_PERIOD_COUNT_MIN;

    ALOGV("%s-%s: original config.period_size = %d config.period_count = %d",
          stream_table[apstream->stream_type], __func__, config->period_size, config->period_count);

    while (periodCount < periodCountRequested && (periodCount * 2) < MMAP_PERIOD_COUNT_MAX) {
        periodCount *= 2;
    }
    config->period_count = periodCount;

    ALOGV("%s-%s: requested config.period_count = %d", stream_table[apstream->stream_type], __func__,
                                                       config->period_count);
}


/******************************************************************************/
/**                                                                          **/
/** Interfaces for Audio Stream Proxy                                        **/
/**                                                                          **/
/******************************************************************************/

void proxy_set_call_path_param(uint32_t set, uint32_t param, int32_t value)
{
    struct audio_proxy *aproxy = getInstance();

    ALOGI("proxy-%s: enter with param: %d val: %d", __func__, param, value);

    if (param == VoIP_APP) {
        aproxy->call_param_idx.apcall_app = value;
    } else if (param == CALL_BAND) {
        aproxy->call_param_idx.sample_rate = value;
    } else if (param == CALL_ROT) {  // change call type value format from Ril to firmware
        switch (value) {
            case GSM:
                aproxy->call_param_idx.call_type = 1;
                break;
            case CDMA:
                aproxy->call_param_idx.call_type = 3;
                break;
            case IMS:
                aproxy->call_param_idx.call_type = 2;
                break;
            case ETC:
                aproxy->call_param_idx.call_type = 0;
                break;
            default:
                ALOGE("proxy-%s: there is no matching Call type for %d", __func__, value);
                break;
        }
    } else if (param == RX_DEVICE) {
        switch (value) {
            case DEVICE_HANDSET:
                aproxy->call_param_idx.device = HANDSET;
                break;
            case DEVICE_HEADPHONE:
                aproxy->call_param_idx.device = HEADPHONE;
                break;
            case DEVICE_HEADSET:
                aproxy->call_param_idx.device = HEADSET;
                break;
            case DEVICE_SPEAKER:
                aproxy->call_param_idx.device = SPEAKER;
                break;
            case DEVICE_BT_SCO_HEADSET:
                aproxy->call_param_idx.device = BT_SCO;
                break;
            default:
               aproxy->call_param_idx.device = HANDSET;
               break;
        }
    } else if (param == EXTRA_VOL) {
        aproxy->call_param_idx.ext_vol = value;
    } else if (param == BT_NREC) {
        aproxy->call_param_idx.NSEC = value;
    }

    if (set == CALL_PATH_SET) {
        ALOGI("proxy-%s: get param(%d), set Call path parameter idx", __func__, param);
        set_call_path_param();
    }

    return ;
}

uint32_t proxy_get_actual_channel_count(void *proxy_stream)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    uint32_t actual_channel_count = 0;

    if (apstream)
        actual_channel_count = (uint32_t)apstream->pcmconfig.channels;

    return actual_channel_count;
}

uint32_t proxy_get_actual_sampling_rate(void *proxy_stream)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    uint32_t actual_sampling_rate = 0;

    if (apstream)
        actual_sampling_rate = (uint32_t)apstream->pcmconfig.rate;

    return actual_sampling_rate;
}

uint32_t proxy_get_actual_period_size(void *proxy_stream)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    uint32_t actual_period_size = 0;

    if (apstream)
        actual_period_size = (uint32_t)apstream->pcmconfig.period_size;

    return actual_period_size;
}

uint32_t proxy_get_actual_period_count(void *proxy_stream)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    uint32_t actual_period_count = 0;

    if (apstream)
        actual_period_count = (uint32_t)apstream->pcmconfig.period_count;

    return actual_period_count;
}

int32_t proxy_get_actual_format(void *proxy_stream)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    int32_t actual_format = (int32_t)AUDIO_FORMAT_INVALID;

    if (apstream)
        actual_format = (int32_t)audio_format_from_pcm_format(apstream->pcmconfig.format);

    return actual_format;
}

void  proxy_offload_set_nonblock(void *proxy_stream)
{
    return;
}

int proxy_offload_compress_func(void *proxy_stream, int func_type)
{
    return 0;
}

int proxy_offload_pause(void *proxy_stream)
{
    return 0;
}

int proxy_offload_resume(void *proxy_stream)
{
    return 0;
}


void *proxy_create_playback_stream(void *proxy, int type, void *config, char *address __unused)
{
    struct audio_proxy *aproxy = proxy;
    audio_stream_type stream_type = (audio_stream_type)type;
    struct audio_config *requested_config = (struct audio_config *)config;

    struct audio_proxy_stream *apstream;

    apstream = (struct audio_proxy_stream *)calloc(1, sizeof(struct audio_proxy_stream));
    if (!apstream) {
        ALOGE("proxy-%s: failed to allocate memory for Proxy Stream", __func__);
        return NULL;;
    }

    /* Stores the requested configurations. */
    apstream->requested_sample_rate = requested_config->sample_rate;
    apstream->requested_channel_mask = requested_config->channel_mask;
    apstream->requested_format = requested_config->format;

    apstream->stream_type = stream_type;
    apstream->need_update_pcm_config = false;

    /* Sets basic configuration from Stream Type. */
    switch (apstream->stream_type) {
        // For VTS
        case ASTREAM_PLAYBACK_NO_ATTRIBUTE:
            apstream->sound_card = PRIMARY_PLAYBACK_CARD;
            apstream->sound_device = PRIMARY_PLAYBACK_DEVICE;
            apstream->pcmconfig = pcm_config_primary_playback;

            break;

        case ASTREAM_PLAYBACK_PRIMARY:
            apstream->sound_card = PRIMARY_PLAYBACK_CARD;
            apstream->sound_device = get_pcm_device_number(aproxy, apstream);
            apstream->pcmconfig = pcm_config_primary_playback;

            if (aproxy->primary_out == NULL)
                aproxy->primary_out = apstream;
            else
                ALOGE("proxy-%s: Primary Output Proxy Stream is already created!!!", __func__);
            break;

        case ASTREAM_PLAYBACK_DEEP_BUFFER:
            apstream->sound_card = DEEP_PLAYBACK_CARD;
            apstream->sound_device = get_pcm_device_number(aproxy, apstream);
            apstream->pcmconfig = pcm_config_deep_playback;
            break;

        case ASTREAM_PLAYBACK_FAST:
            apstream->sound_card = FAST_PLAYBACK_CARD;
            apstream->sound_device = get_pcm_device_number(aproxy, apstream);
            apstream->pcmconfig = pcm_config_fast_playback;
            break;

        case ASTREAM_PLAYBACK_LOW_LATENCY:
            apstream->sound_card = LOW_PLAYBACK_CARD;
            apstream->sound_device = get_pcm_device_number(aproxy, apstream);
            apstream->pcmconfig = pcm_config_low_playback;
            break;


        case ASTREAM_PLAYBACK_MMAP:
            apstream->sound_card = MMAP_PLAYBACK_CARD;
            apstream->sound_device = get_pcm_device_number(aproxy, apstream);
            apstream->pcmconfig = pcm_config_mmap_playback;

            break;

        case ASTREAM_PLAYBACK_AUX_DIGITAL:
            apstream->sound_card = AUX_PLAYBACK_CARD;
            apstream->sound_device = get_pcm_device_number(aproxy, apstream);
            apstream->pcmconfig = pcm_config_aux_playback;

            if (apstream->requested_sample_rate != 0) {
                apstream->pcmconfig.rate = apstream->requested_sample_rate;
                // It needs Period Size adjustment based with predefined duration
                // to avoid underrun noise by small buffer at high sampling rate
                if (apstream->requested_sample_rate > DEFAULT_MEDIA_SAMPLING_RATE) {
                    apstream->pcmconfig.period_size = (apstream->requested_sample_rate * PREDEFINED_DP_PLAYBACK_DURATION) / 1000;
                    ALOGI("proxy-%s: changed Period Size(%d) as requested sampling rate(%d)",
                          __func__, apstream->pcmconfig.period_size, apstream->pcmconfig.rate);
                }
            }
            if (apstream->requested_channel_mask != AUDIO_CHANNEL_NONE) {
                apstream->pcmconfig.channels = audio_channel_count_from_out_mask(apstream->requested_channel_mask);
            }
            if (apstream->requested_format != AUDIO_FORMAT_DEFAULT) {
                apstream->pcmconfig.format = pcm_format_from_audio_format(apstream->requested_format);
            }

            break;

        case ASTREAM_PLAYBACK_INCALL_MUSIC:
            apstream->sound_card = INCALLMUSIC_PLAYBACK_CARD;
            apstream->sound_device = get_pcm_device_number(aproxy, apstream);
            apstream->pcmconfig = pcm_config_incallmusic_playback;

            break;
        default:
            ALOGE("proxy-%s: failed to open Proxy Stream as unknown stream type(%d)", __func__,
                                                                          apstream->stream_type);
            goto err_open;
    }

    apstream->pcm = NULL;

    ALOGI("proxy-%s: opened Proxy Stream(%s)", __func__, stream_table[apstream->stream_type]);
    return (void *)apstream;

err_open:
    free(apstream);
    return NULL;
}

void proxy_destroy_playback_stream(void *proxy_stream)
{
    struct audio_proxy *aproxy = getInstance();
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;

    if (apstream) {
        if (apstream->stream_type == ASTREAM_PLAYBACK_PRIMARY) {
            if (aproxy->primary_out != NULL)
                aproxy->primary_out = NULL;
        }

        free(apstream);
    }

    return ;
}

int proxy_close_playback_stream(void *proxy_stream)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    int ret = 0;

    /* Close Noamrl PCM Device */
    if (apstream->pcm) {
        ret = pcm_close(apstream->pcm);
        apstream->pcm = NULL;
    }

    return ret;
}

int proxy_open_playback_stream(void *proxy_stream, int32_t min_size_frames, void *mmap_info)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    struct audio_mmap_buffer_info *info = (struct audio_mmap_buffer_info *)mmap_info;
    unsigned int sound_card;
    unsigned int sound_device;
    unsigned int flags;
    int ret = 0;
    char pcm_path[MAX_PCM_PATH_LEN];

    /* Get PCM/Compress Device */
    sound_card = apstream->sound_card;
    sound_device = apstream->sound_device;

    /* Open Normal PCM Device */
        if (apstream->pcm == NULL) {
            if (apstream->stream_type == ASTREAM_PLAYBACK_MMAP) {
                flags = PCM_OUT | PCM_MMAP | PCM_NOIRQ | PCM_MONOTONIC;

                adjust_mmap_period_count(apstream, &apstream->pcmconfig, min_size_frames);
            } else
            flags = PCM_OUT | PCM_MONOTONIC;

            apstream->pcm = pcm_open(sound_card, sound_device, flags, &apstream->pcmconfig);
            if (apstream->pcm && !pcm_is_ready(apstream->pcm)) {
                /* pcm_open does always return pcm structure, not NULL */
                ALOGE("%s-%s: PCM Device is not ready with Sampling_Rate(%u) error(%s)!",
                      stream_table[apstream->stream_type], __func__, apstream->pcmconfig.rate,
                      pcm_get_error(apstream->pcm));
                goto err_open;
            }

            snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c", sound_card, sound_device ,'p');
            ALOGI("%s-%s: The opened PCM Device is %s with Sampling_Rate(%u) PCM_Format(%d)",
                  stream_table[apstream->stream_type], __func__, pcm_path,
                  apstream->pcmconfig.rate, apstream->pcmconfig.format);

            if (apstream->stream_type == ASTREAM_PLAYBACK_MMAP) {
                unsigned int offset1 = 0;
                unsigned int frames1 = 0;

                ret = pcm_mmap_begin(apstream->pcm, &info->shared_memory_address, &offset1, &frames1);
                if (ret == 0)  {
                    ALOGI("%s-%s: PCM Device begin MMAP", stream_table[apstream->stream_type], __func__);

                    info->buffer_size_frames = pcm_get_buffer_size(apstream->pcm);
                    info->burst_size_frames = apstream->pcmconfig.period_size;
                    info->shared_memory_fd = pcm_get_poll_fd(apstream->pcm);

                    memset(info->shared_memory_address, 0,
                           pcm_frames_to_bytes(apstream->pcm, info->buffer_size_frames));

                    ret = pcm_mmap_commit(apstream->pcm, 0, MMAP_PERIOD_SIZE);
                    if (ret < 0) {
                        ALOGE("%s-%s: PCM Device cannot commit MMAP with error(%s)",
                              stream_table[apstream->stream_type], __func__, pcm_get_error(apstream->pcm));
                        goto err_open;
                    } else {
                        ALOGI("%s-%s: PCM Device commit MMAP", stream_table[apstream->stream_type], __func__);
                        ret = 0;
                    }
                } else {
                    ALOGE("%s-%s: PCM Device cannot begin MMAP with error(%s)",
                          stream_table[apstream->stream_type], __func__, pcm_get_error(apstream->pcm));
                    goto err_open;
                }
            }
        } else
            ALOGW("%s-%s: PCM Device is already opened!", stream_table[apstream->stream_type], __func__);
    return ret;

err_open:
    proxy_close_playback_stream(proxy_stream);
    return -ENODEV;
}

int proxy_start_playback_stream(void *proxy_stream)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    int ret = 0;

    if (apstream->stream_type == ASTREAM_PLAYBACK_MMAP) {
        if (apstream->pcm) {
            ret = pcm_start(apstream->pcm);
            if (ret == 0)
                ALOGI("%s-%s: started MMAP Device", stream_table[apstream->stream_type], __func__);
            else
                ALOGE("%s-%s: cannot start MMAP device with error(%s)", stream_table[apstream->stream_type],
                                               __func__, pcm_get_error(apstream->pcm));
        } else
            ret = -ENOSYS;
    }

    return ret;
}

int proxy_write_playback_buffer(void *proxy_stream, void* buffer, int bytes)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    int ret = 0, wrote = 0;

    /* Skip other sounds except AUX Digital Stream when AUX_DIGITAL is connected */
    if (apstream->stream_type != ASTREAM_PLAYBACK_AUX_DIGITAL &&
        getInstance()->active_playback_device == DEVICE_AUX_DIGITAL) {
        skip_pcm_processing(apstream, wrote);
        wrote = bytes;
        save_written_frames(apstream, wrote);
        return wrote;
    }

        if (apstream->pcm) {
            ret = pcm_write(apstream->pcm, (void *)buffer, (unsigned int)bytes);
            if (ret == 0) {
                ALOGVV("%s-%s: writed %u bytes to PCM Device", stream_table[apstream->stream_type],
                                                               __func__, (unsigned int)bytes);

            } else {
                ALOGE("%s-%s: failed to write to PCM Device with %s",
                      stream_table[apstream->stream_type], __func__, pcm_get_error(apstream->pcm));
                skip_pcm_processing(apstream, wrote);
            }
            wrote = bytes;
            save_written_frames(apstream, wrote);
        }

    return wrote;
}

int proxy_stop_playback_stream(void *proxy_stream)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    int ret = 0;

    if (apstream->stream_type == ASTREAM_PLAYBACK_MMAP) {
        if (apstream->pcm) {
            ret = pcm_stop(apstream->pcm);
            if (ret == 0)
                ALOGI("%s-%s: stop MMAP Device", stream_table[apstream->stream_type], __func__);
            else
                ALOGE("%s-%s: cannot stop MMAP device with error(%s)", stream_table[apstream->stream_type],
                                               __func__, pcm_get_error(apstream->pcm));
        }
    }

    return ret;
}

int proxy_reconfig_playback_stream(void *proxy_stream, int type, void *config)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    audio_stream_type new_type = (audio_stream_type)type;
    struct audio_config *new_config = (struct audio_config *)config;

    if (apstream) {
        apstream->stream_type = new_type;
        apstream->requested_sample_rate = new_config->sample_rate;
        apstream->requested_channel_mask = new_config->channel_mask;
        apstream->requested_format = new_config->format;

        return 0;
    } else
        return -1;
}

int proxy_get_render_position(void *proxy_stream, uint32_t *frames)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    int ret = -ENODATA;

    if (frames != NULL) {
        *frames = 0;
    } else {
        ALOGE("%s-%s: Invalid Parameter with Null pointer parameter",
              stream_table[apstream->stream_type], __func__);
        ret =  -EINVAL;
    }

    return ret;
}

int proxy_get_presen_position(void *proxy_stream, uint64_t *frames, struct timespec *timestamp)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    unsigned int avail = 0;
    int ret = -ENODATA;

    if (frames != NULL && timestamp != NULL) {
        *frames = 0;
            if (apstream->pcm) {
                ret = pcm_get_htimestamp(apstream->pcm, &avail, timestamp);
                if (ret == 0) {
                    // Total Frame Count in kernel Buffer
                    uint64_t kernel_buffer_size = (uint64_t)apstream->pcmconfig.period_size *
                                                  (uint64_t)apstream->pcmconfig.period_count;

                    // Real frames which played out to device
                    int64_t signed_frames = apstream->frames - kernel_buffer_size + avail;

                    if (signed_frames >= 0)
                        *frames = (uint64_t)signed_frames;
                    else
                        ret = -ENODATA;
                } else {
                    ret = -ENODATA;
                }
            }
    } else {
        ALOGE("%s-%s: Invalid Parameter with Null pointer parameter",
              stream_table[apstream->stream_type], __func__);
        ret =  -EINVAL;
    }

    return ret;
}

int proxy_getparam_playback_stream(void *proxy_stream, void *query_params, void *reply_params)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    struct str_parms *query = (struct str_parms *)query_params;
    struct str_parms *reply = (struct str_parms *)reply_params;

    /*
     * Supported Audio Configuration can be different as Target Project.
     * AudioHAL engineers have to modify these codes based on Target Project.
     */
    // supported audio formats
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_FORMATS)) {
        char formats_list[256];

        memset(formats_list, 0, 256);
        strncpy(formats_list, stream_format_table[apstream->stream_type],
                       strlen(stream_format_table[apstream->stream_type]));
        str_parms_add_str(reply, AUDIO_PARAMETER_STREAM_SUP_FORMATS, formats_list);
    }

    // supported audio channel masks
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_CHANNELS)) {
        char channels_list[256];

        memset(channels_list, 0, 256);
        strncpy(channels_list, stream_channel_table[apstream->stream_type],
                        strlen(stream_channel_table[apstream->stream_type]));
        str_parms_add_str(reply, AUDIO_PARAMETER_STREAM_SUP_CHANNELS, channels_list);
    }

    // supported audio samspling rates
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES)) {
        char rates_list[256];

        memset(rates_list, 0, 256);
        strncpy(rates_list, stream_rate_table[apstream->stream_type],
                     strlen(stream_rate_table[apstream->stream_type]));
        str_parms_add_str(reply, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES, rates_list);
    }

    return 0;
}

int proxy_setparam_playback_stream(void *proxy_stream, void *parameters)
{
    return 0;
}

uint32_t proxy_get_playback_latency(void *proxy_stream)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    uint32_t latency;

    // Total Latency = ALSA Buffer latency + HW Latency
    latency = (apstream->pcmconfig.period_count * apstream->pcmconfig.period_size * 1000) / (apstream->pcmconfig.rate);
    latency += 0;   // Need to check HW Latency

    return latency;
}

void proxy_dump_playback_stream(void *proxy_stream, int fd)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;

    const size_t len = 256;
    char buffer[len];

    if (apstream->pcm != NULL) {
        snprintf(buffer, len, "\toutput pcm config sample rate: %d\n",apstream->pcmconfig.rate);
        write(fd,buffer,strlen(buffer));
        snprintf(buffer, len, "\toutput pcm config period size : %d\n",apstream->pcmconfig.period_size);
        write(fd,buffer,strlen(buffer));
        snprintf(buffer, len, "\toutput pcm config format: %d\n",apstream->pcmconfig.format);
        write(fd,buffer,strlen(buffer));
        snprintf(buffer, len, "\toutput pcm config period size : %d\n", apstream->pcmconfig.period_size);
        write(fd,buffer,strlen(buffer));
    }

    return ;
}


void *proxy_create_capture_stream(void *proxy, int type, int usage, void *config, char *address __unused)
{
    struct audio_proxy *aproxy = proxy;
    audio_stream_type stream_type = (audio_stream_type)type;
    audio_usage       stream_usage = (audio_usage)usage;
    struct audio_config *requested_config = (struct audio_config *)config;

    struct audio_proxy_stream *apstream;

    apstream = (struct audio_proxy_stream *)calloc(1, sizeof(struct audio_proxy_stream));
    if (!apstream) {
        ALOGE("proxy-%s: failed to allocate memory for Proxy Stream", __func__);
        return NULL;;
    }

    /* Stores the requested configurationss */
    apstream->requested_sample_rate = requested_config->sample_rate;
    apstream->requested_channel_mask = requested_config->channel_mask;
    apstream->requested_format = requested_config->format;

    apstream->stream_type = stream_type;
    apstream->stream_usage = stream_usage;

    // Initialize Post-Processing
    apstream->need_monoconversion = false;
    apstream->need_resampling = false;

    apstream->actual_read_buf = NULL;
    apstream->actual_read_buf_size = 0;

    apstream->proc_buf_out = NULL;
    apstream->proc_buf_size = 0;

    apstream->resampler = NULL;

    /* Sets basic configuration from Stream Type. */
    switch (apstream->stream_type) {
        // For VTS
        case ASTREAM_CAPTURE_NO_ATTRIBUTE:
            apstream->sound_card = PRIMARY_CAPTURE_CARD;
            apstream->sound_device = PRIMARY_CAPTURE_DEVICE;
            apstream->pcmconfig = pcm_config_primary_capture;

            break;

        case ASTREAM_CAPTURE_PRIMARY:
            apstream->sound_card = PRIMARY_CAPTURE_CARD;
            apstream->sound_device = get_pcm_device_number(aproxy, apstream);
            apstream->pcmconfig = pcm_config_primary_capture;

            update_capture_pcmconfig(apstream);
            check_conversion(apstream);
            break;

        case ASTREAM_CAPTURE_CALL:
            apstream->sound_card = CALL_RECORD_CARD;
            apstream->sound_device = get_pcm_device_number(aproxy, apstream);
            apstream->pcmconfig = pcm_config_call_record;

            check_conversion(apstream);
            break;

        case ASTREAM_CAPTURE_LOW_LATENCY:
            apstream->sound_card = LOW_CAPTURE_CARD;
            apstream->sound_device = get_pcm_device_number(aproxy, apstream);
            apstream->pcmconfig = pcm_config_low_capture;

            update_capture_pcmconfig(apstream);
            check_conversion(apstream);
            break;

        case ASTREAM_CAPTURE_MMAP:
            apstream->sound_card = MMAP_CAPTURE_CARD;
            apstream->sound_device = get_pcm_device_number(aproxy, apstream);
            apstream->pcmconfig = pcm_config_mmap_capture;
            break;

        case ASTREAM_CAPTURE_FM_TUNER:
            apstream->sound_card = FM_RECORD_CARD;
            apstream->sound_device = get_pcm_device_number(aproxy, apstream);
            apstream->pcmconfig = pcm_config_fm_record;

            check_conversion(apstream);
            break;

        case ASTREAM_CAPTURE_FM_RECORDING:
            apstream->sound_card = FM_RECORD_CARD;
            apstream->sound_device = get_pcm_device_number(aproxy, apstream);
            apstream->pcmconfig = pcm_config_fm_record;

            check_conversion(apstream);
            break;
        default:
            ALOGE("proxy-%s: failed to open Proxy Stream as unknown stream type(%d)", __func__,
                                                                          apstream->stream_type);
            goto err_open;
    }

    apstream->pcm = NULL;

    ALOGI("proxy-%s: opened Proxy Stream(%s)", __func__, stream_table[apstream->stream_type]);
    return (void *)apstream;

err_open:
    free(apstream);
    return NULL;
}

void proxy_destroy_capture_stream(void *proxy_stream)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;

    if (apstream) {
        if (apstream->resampler) {
            ALOGV("%s-%s: released resampler", stream_table[apstream->stream_type], __func__);
            release_resampler(apstream->resampler);
        }

        if (apstream->actual_read_buf)
            free(apstream->actual_read_buf);

        if (apstream->proc_buf_out)
            free(apstream->proc_buf_out);

        free(apstream);
    }

    return ;
}

int proxy_close_capture_stream(void *proxy_stream)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    int ret = 0;

    /* Close Normal PCM Device */
    if (apstream->pcm) {
        ret = pcm_close(apstream->pcm);
        apstream->pcm = NULL;
        apstream->cpcall_rec_skipcnt = 0;
        ALOGI("%s-%s: closed PCM Device", stream_table[apstream->stream_type], __func__);
    }

    return ret;
}

int proxy_open_capture_stream(void *proxy_stream, int32_t min_size_frames, void *mmap_info)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    struct audio_proxy *aproxy = getInstance();
    struct audio_mmap_buffer_info *info = (struct audio_mmap_buffer_info *)mmap_info;
    unsigned int sound_card;
    unsigned int sound_device;
    unsigned int flags;
    int ret = 0;
    char pcm_path[MAX_PCM_PATH_LEN];

    if (is_active_usage_APCall(aproxy) && apstream->pcmconfig.rate != 48000) {
        apstream->sound_card = PRIMARY_CAPTURE_CARD;
        apstream->sound_device = get_pcm_device_number(aproxy, apstream);
        apstream->pcmconfig = pcm_config_primary_capture;

        check_conversion(apstream);
    }

    if (aproxy->audio_mode == AUDIO_MODE_IN_COMMUNICATION) { // // After TXSE, get MONO data from firmware
        /* HACK : TXSE is not prepared
        apstream->pcmconfig.channels = MEDIA_1_CHANNEL;
	*/
        update_capture_pcmconfig(apstream);
        check_conversion(apstream);
    }

    /* Get PCM Device */
    sound_card = apstream->sound_card;
    sound_device = apstream->sound_device;

    /* Open Normal PCM Device */
    if (apstream->pcm == NULL) {
        if (apstream->stream_type == ASTREAM_CAPTURE_MMAP) {
            flags = PCM_IN | PCM_MMAP | PCM_NOIRQ | PCM_MONOTONIC;

            adjust_mmap_period_count(apstream, &apstream->pcmconfig, min_size_frames);
        } else {
            flags = PCM_IN | PCM_MONOTONIC;
        }

        apstream->pcm = pcm_open(sound_card, sound_device, flags, &apstream->pcmconfig);
        if (apstream->pcm && !pcm_is_ready(apstream->pcm)) {
            /* pcm_open does always return pcm structure, not NULL */
            ALOGE("%s-%s: PCM Device is not ready with Sampling_Rate(%u) error(%s)!",
                  stream_table[apstream->stream_type], __func__, apstream->pcmconfig.rate,
                  pcm_get_error(apstream->pcm));
            goto err_open;
        }

        snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c", sound_card, sound_device, 'c');
        ALOGI("%s-%s: The opened PCM Device is %s with Sampling_Rate(%u) PCM_Format(%d) Channel(%d)",
              stream_table[apstream->stream_type], __func__, pcm_path,
              apstream->pcmconfig.rate, apstream->pcmconfig.format, apstream->pcmconfig.channels);

        if (apstream->stream_type == ASTREAM_CAPTURE_MMAP) {
            unsigned int offset1 = 0;
            unsigned int frames1 = 0;

            ret = pcm_mmap_begin(apstream->pcm, &info->shared_memory_address, &offset1, &frames1);
            if (ret == 0)  {
                ALOGI("%s-%s: PCM Device begin MMAP", stream_table[apstream->stream_type], __func__);

                info->buffer_size_frames = pcm_get_buffer_size(apstream->pcm);
                info->burst_size_frames = apstream->pcmconfig.period_size;
                info->shared_memory_fd = pcm_get_poll_fd(apstream->pcm);

                memset(info->shared_memory_address, 0,
                       pcm_frames_to_bytes(apstream->pcm, info->buffer_size_frames));

                ret = pcm_mmap_commit(apstream->pcm, 0, MMAP_PERIOD_SIZE);
                if (ret < 0) {
                    ALOGE("%s-%s: PCM Device cannot commit MMAP with error(%s)",
                          stream_table[apstream->stream_type], __func__, pcm_get_error(apstream->pcm));
                    goto err_open;
                } else {
                    ALOGI("%s-%s: PCM Device commit MMAP", stream_table[apstream->stream_type], __func__);
                    ret = 0;
                }
            } else {
                ALOGE("%s-%s: PCM Device cannot begin MMAP with error(%s)",
                      stream_table[apstream->stream_type], __func__, pcm_get_error(apstream->pcm));
                goto err_open;
            }
        }
    } else
        ALOGW("%s-%s: PCM Device is already opened!", stream_table[apstream->stream_type], __func__);

    return ret;

err_open:
    proxy_close_capture_stream(proxy_stream);
    return -ENODEV;
}

int proxy_start_capture_stream(void *proxy_stream)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    int ret = 0;

    // In case of PCM Playback, pcm_start call is not needed as auto-start
    if (apstream->pcm) {
        ret = pcm_start(apstream->pcm);
        if (ret == 0)
            ALOGI("%s-%s: started PCM Device", stream_table[apstream->stream_type], __func__);
        else
            ALOGE("%s-%s: cannot start PCM(%s)", stream_table[apstream->stream_type], __func__,
                                                 pcm_get_error(apstream->pcm));
    }

    return ret;
}

int proxy_read_capture_buffer(void *proxy_stream, void *buffer, int bytes)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    int frames_request = bytes / proxy_get_requested_frame_size(apstream);
    int frames_actual = -1;
    struct audio_proxy *aproxy = getInstance();

    if (((apstream->callrec_err_detect && apstream->cpcall_rec_skipcnt < 10) &&
        is_audiomode_incall(aproxy) &&
        apstream->sound_card == SOUND_CARD1) ||
        (!is_audiomode_incall(aproxy) &&
        apstream->sound_card == SOUND_CARD1 && apstream->stream_type != ASTREAM_CAPTURE_FM_TUNER)) {
        memset(buffer, 0, bytes);
        usleep(20 * 1000); // 20msec
        frames_actual = 0;
        apstream->cpcall_rec_skipcnt++;
        ALOGVV("%s-%s: --Mute data PCM Device(%d) : %d ", stream_table[apstream->stream_type], __func__,
            apstream->sound_device,apstream->cpcall_rec_skipcnt);
    } else {
        frames_actual = read_and_process_frames(apstream, buffer, frames_request);
        if (frames_actual > 0 && apstream->callrec_err_detect)
            apstream->callrec_err_detect = false;

        ALOGVV("%s-%s: requested read frames = %d vs. actual processed read frames = %d",
               stream_table[apstream->stream_type], __func__, frames_request, frames_actual);
    }

    if (frames_actual < 0) {
        /* Check if read error happened in case call-recording scenario and
           enable skip logic */
        if ((is_audiomode_incall(aproxy) &&
            apstream->sound_card == SOUND_CARD1) ||
            (!is_audiomode_incall(aproxy) &&
            apstream->sound_card == SOUND_CARD1 && apstream->stream_type != ASTREAM_CAPTURE_FM_TUNER)) {
            apstream->callrec_err_detect = true;
        }
        return frames_actual;
    } else {
        /* Saves read frames to calcurate timestamp */
        apstream->frames += frames_actual;
        ALOGVV("%s-%s: cumulative read = %u frames", stream_table[apstream->stream_type], __func__,
                                          (unsigned int)apstream->frames);
        return bytes;
    }
}

int proxy_stop_capture_stream(void *proxy_stream)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    int ret = 0;

    if (apstream->pcm) {
        ret = pcm_stop(apstream->pcm);
        if (ret == 0)
            ALOGI("%s-%s: stopped PCM Device", stream_table[apstream->stream_type], __func__);
        else
            ALOGE("%s-%s: cannot stop PCM(%s)", stream_table[apstream->stream_type], __func__,
                                                pcm_get_error(apstream->pcm));
    }

    return ret;
}

int proxy_reconfig_capture_stream(void *proxy_stream, int type, void *config)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    audio_stream_type new_type = (audio_stream_type)type;
    struct audio_config *new_config = (struct audio_config *)config;

    if (apstream) {
        apstream->stream_type = new_type;
        apstream->requested_sample_rate = new_config->sample_rate;
        apstream->requested_channel_mask = new_config->channel_mask;
        apstream->requested_format = new_config->format;

        // If some stream types need to be reset, it has to reconfigure conversions

        return 0;
    } else
        return -1;
}

int proxy_reconfig_capture_usage(void *proxy_stream, int type, int usage)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;

    if (apstream == NULL)
        return -1;

    struct audio_proxy *aproxy = getInstance();
    audio_stream_type stream_type = (audio_stream_type)type;
    audio_usage       stream_usage = (audio_usage)usage;

    if (stream_usage != AUSAGE_NONE)
        apstream->stream_usage = stream_usage;

     switch (stream_type) {
        case ASTREAM_CAPTURE_PRIMARY:
            apstream->stream_type = stream_type;
            apstream->sound_card = PRIMARY_CAPTURE_CARD;
            apstream->sound_device = get_pcm_device_number(aproxy, apstream);
            apstream->pcmconfig = pcm_config_primary_capture;

            update_capture_pcmconfig(apstream);
            /* Release already running resampler for reconfiguration purpose */
            if (apstream->resampler) {
                ALOGI("%s-%s: released resampler", stream_table[apstream->stream_type], __func__);
                release_resampler(apstream->resampler);
                apstream->resampler = NULL;
            }
            check_conversion(apstream);
            break;

        case ASTREAM_CAPTURE_CALL:
            apstream->stream_type = stream_type;
            apstream->sound_card = CALL_RECORD_CARD;
            apstream->sound_device = get_pcm_device_number(aproxy, apstream);
            apstream->pcmconfig = pcm_config_call_record;

            /* Release already running resampler for reconfiguration purpose */
            if (apstream->resampler) {
                ALOGI("%s-%s: released resampler", stream_table[apstream->stream_type], __func__);
                release_resampler(apstream->resampler);
                apstream->resampler = NULL;
            }
            check_conversion(apstream);
            break;
        default:
            ALOGE("proxy-%s: failed to reconfig Proxy Stream as unknown stream type(%d)", __func__, stream_type);
            return -1;
    }

    ALOGI("proxy-%s: reconfig Proxy Stream(%s)", __func__, stream_table[apstream->stream_type]);

    return 0;
}

int proxy_get_capture_pos(void *proxy_stream, int64_t *frames, int64_t *time)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    unsigned int avail = 0;
    struct timespec timestamp;
    int ret = -ENOSYS;;

    if (frames != NULL && time != NULL) {
        *frames = 0;
        *time = 0;

        if (apstream->pcm) {
            ret = pcm_get_htimestamp(apstream->pcm, &avail, &timestamp);
            if (ret == 0) {
                // Real frames which captured in from device
                *frames = apstream->frames + avail;
                // Nano Seconds Unit Time
                *time = timestamp.tv_sec * 1000000000LL + timestamp.tv_nsec;
                ret = 0;
            }
        }
    } else {
        ALOGE("%s-%s: Invalid Parameter with Null pointer parameter",
              stream_table[apstream->stream_type], __func__);
        ret =  -EINVAL;
    }

    return ret;
}

int proxy_get_active_microphones(void *proxy_stream, void *array, int *count)
{
    struct audio_proxy *aproxy = getInstance();
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    struct audio_microphone_characteristic_t *mic_array = array;
    size_t *mic_count = (size_t *)count;
    size_t actual_mic_count = 0;
    int ret = 0;

    if (apstream) {
        if (apstream->stream_type == ASTREAM_CAPTURE_NO_ATTRIBUTE ||
            apstream->stream_type == ASTREAM_CAPTURE_PRIMARY ||
            apstream->stream_type == ASTREAM_CAPTURE_LOW_LATENCY ||
            apstream->stream_type == ASTREAM_CAPTURE_MMAP) {
            device_type active_device = aproxy->active_capture_device;
            if (active_device == DEVICE_NONE) {
                ALOGE("%s-%s: There are no active MIC", stream_table[apstream->stream_type], __func__);
                ret = -ENOSYS;
            }

            if (*mic_count == 0) {
                if (active_device == DEVICE_STEREO_MIC)
                    actual_mic_count = 2;
                else
                    actual_mic_count = 1;
                ALOGI("proxy-%s: requested number of microphone, return %zu", __func__, *mic_count);
            } else {
                if (active_device == DEVICE_STEREO_MIC) {
                    for (int i = 0; i < 2; i++) {
                        mic_array[i] = aproxy->mic_info[i];
                        ALOGD("%s-%s: %dth MIC = %s", stream_table[apstream->stream_type], __func__,
                                                      i+1, mic_array[i].device_id);
                        actual_mic_count++;
                    }
                } else if (active_device == DEVICE_MAIN_MIC) {
                        mic_array[0] = aproxy->mic_info[0];
                        ALOGD("%s-%s: Active MIC = %s", stream_table[apstream->stream_type], __func__,
                                                        mic_array[0].device_id);
                        actual_mic_count = 1;
                } else if (active_device == DEVICE_SUB_MIC) {
                        mic_array[0] = aproxy->mic_info[1];
                        ALOGD("%s-%s: Active MIC = %s", stream_table[apstream->stream_type], __func__,
                                                        mic_array[0].device_id);
                        actual_mic_count = 1;
                } else {
                    ALOGE("%s-%s: Abnormal active device(%s)", stream_table[apstream->stream_type],
                                                     __func__, device_table[active_device]);
                    ret = -ENOSYS;
                }
            }
        } else {
            ALOGE("%s-%s: This stream doesn't have active MIC", stream_table[apstream->stream_type],
                                                                __func__);
            ret = -ENOSYS;
        }
    } else {
        ALOGE("proxy-%s: apstream is NULL", __func__);
        ret = -ENOSYS;
    }

    *mic_count = actual_mic_count;

    return ret;
}

int proxy_getparam_capture_stream(void *proxy_stream, void *query_params, void *reply_params)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    struct str_parms *query = (struct str_parms *)query_params;
    struct str_parms *reply = (struct str_parms *)reply_params;

    /*
     * Supported Audio Configuration can be different as Target Project.
     * AudioHAL engineers have to modify these codes based on Target Project.
     */
    // supported audio formats
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_FORMATS)) {
        char formats_list[256];

        memset(formats_list, 0, 256);
        strncpy(formats_list, stream_format_table[apstream->stream_type],
                       strlen(stream_format_table[apstream->stream_type]));
        str_parms_add_str(reply, AUDIO_PARAMETER_STREAM_SUP_FORMATS, formats_list);
    }

    // supported audio channel masks
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_CHANNELS)) {
        char channels_list[256];

        memset(channels_list, 0, 256);
        strncpy(channels_list, stream_channel_table[apstream->stream_type],
                        strlen(stream_channel_table[apstream->stream_type]));
        str_parms_add_str(reply, AUDIO_PARAMETER_STREAM_SUP_CHANNELS, channels_list);
    }

    // supported audio samspling rates
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES)) {
        char rates_list[256];

        memset(rates_list, 0, 256);
        strncpy(rates_list, stream_rate_table[apstream->stream_type],
                     strlen(stream_rate_table[apstream->stream_type]));
        str_parms_add_str(reply, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES, rates_list);
    }

    return 0;
}

int proxy_setparam_capture_stream(void *proxy_stream, void *parameters __unused)
{
    return 0;
}

void proxy_dump_capture_stream(void *proxy_stream, int fd)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;

    const size_t len = 256;
    char buffer[len];

    if (apstream->pcm != NULL) {
        snprintf(buffer, len, "\tinput pcm config sample rate: %d\n",apstream->pcmconfig.rate);
        write(fd,buffer,strlen(buffer));
        snprintf(buffer, len, "\tinput pcm config period size : %d\n",apstream->pcmconfig.period_size);
        write(fd,buffer,strlen(buffer));
        snprintf(buffer, len, "\tinput pcm config format: %d\n",apstream->pcmconfig.format);
        write(fd,buffer,strlen(buffer));
    }

    return ;
}

void proxy_update_capture_usage(void *proxy_stream, int usage)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    audio_usage    stream_usage = (audio_usage)usage;

    if(apstream) {
        apstream->stream_usage = stream_usage;
        ALOGD("proxy-%s: apstream->stream_usage = %d", __func__, apstream->stream_usage);
    } else {
        ALOGD("proxy-%s: apstream is NULL", __func__);
    }
    return ;
}

int proxy_get_mmap_position(void *proxy_stream, void *pos)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    struct audio_mmap_position *position = (struct audio_mmap_position *)pos;
    int ret = -ENOSYS;

    if ((apstream->stream_type == ASTREAM_PLAYBACK_MMAP || apstream->stream_type == ASTREAM_CAPTURE_MMAP)&&
         apstream->pcm) {
        struct timespec ts = { 0, 0 };

        ret = pcm_mmap_get_hw_ptr(apstream->pcm, (unsigned int *)&position->position_frames, &ts);
        if (ret == 0)
            position->time_nanoseconds = audio_utils_ns_from_timespec(&ts);
    }

    return ret;
}


/******************************************************************************/
/**                                                                          **/
/** Interfaces for Audio Device Proxy                                        **/
/**                                                                          **/
/******************************************************************************/

/*
 *  Route Control Functions
 */
bool proxy_init_route(void *proxy, char *path)
{
    struct audio_proxy *aproxy = proxy;
    struct audio_route *ar = NULL;
    bool ret = false;

    if (aproxy) {
        aproxy->mixer = mixer_open(MIXER_CARD0);
        proxy_set_mixercontrol(aproxy, TICKLE_CONTROL, ABOX_TICKLE_ON);
        if (aproxy->mixer) {
            // In order to get add event, subscription has to be here!
            mixer_subscribe_events(aproxy->mixer, 1);

            ar = audio_route_init(MIXER_CARD0, path);
            if (!ar) {
                ALOGE("proxy-%s: failed to init audio route", __func__);
                mixer_subscribe_events(aproxy->mixer, 0);
                mixer_close(aproxy->mixer);
                aproxy->mixer = NULL;
            } else {
                aproxy->aroute = ar;
                aproxy->xml_path = strdup(path);    // Save Mixer Paths XML File path

                aproxy->active_playback_ausage   = AUSAGE_NONE;
                aproxy->active_playback_device   = DEVICE_NONE;
                aproxy->active_playback_modifier = MODIFIER_NONE;

                aproxy->active_capture_ausage   = AUSAGE_NONE;
                aproxy->active_capture_device   = DEVICE_NONE;
                aproxy->active_capture_modifier = MODIFIER_NONE;

                ALOGI("proxy-%s: opened Mixer & initialized audio route", __func__);
                ret = true;

                /* Create Mixer Control Update Thread */
                pthread_rwlock_init(&aproxy->mixer_update_lock, NULL);

                if (audio_route_missing_ctl(ar)) {
                    pthread_create(&aproxy->mixer_update_thread, NULL, mixer_update_loop, aproxy);
                    ALOGI("proxy-%s: missing control found, update thread is created", __func__);
                } else
                    mixer_subscribe_events(aproxy->mixer, 0);
            }
        } else
            ALOGE("proxy-%s: failed to open Mixer", __func__);
    }

    return ret;
}

void proxy_deinit_route(void *proxy)
{
    struct audio_proxy *aproxy = proxy;

    if (aproxy) {
        pthread_rwlock_wrlock(&aproxy->mixer_update_lock);

        if (aproxy->aroute) {
            audio_route_free(aproxy->aroute);
            aproxy->aroute = NULL;
        }
        if (aproxy->mixer) {
            mixer_close(aproxy->mixer);
            aproxy->mixer = NULL;
        }

        pthread_rwlock_unlock(&aproxy->mixer_update_lock);
        pthread_rwlock_destroy(&aproxy->mixer_update_lock);
        free(aproxy->xml_path);
    }
    ALOGI("proxy-%s: closed Mixer & deinitialized audio route", __func__);

    return ;
}

bool proxy_update_route(void *proxy, int ausage, int device)
{
    struct audio_proxy *aproxy = proxy;
    audio_usage routed_ausage = (audio_usage)ausage;
    device_type routed_device = (device_type)device;

    // Temp
    if (aproxy != NULL) {
        routed_ausage = AUSAGE_NONE;
        routed_device = DEVICE_NONE;
    }

    return true;
}

bool proxy_set_route(void *proxy, int ausage, int device, int modifier, bool set)
{
    struct audio_proxy *aproxy = proxy;

    audio_usage   routed_ausage = (audio_usage)ausage;
    device_type   routed_device = (device_type)device;
    modifier_type routed_modifier = (modifier_type)modifier;

    if (set) {
        if (routed_device < DEVICE_MAIN_MIC) {
            // Audio Path Routing for Playback Path
            if ((aproxy->active_playback_device != routed_device) && (is_active_usage_APCall(aproxy) ||
                (routed_ausage >= AUSAGE_APCALL_MIN && routed_ausage <= AUSAGE_APCALL_MAX)))
                proxy_set_mixercontrol(aproxy, MUTE_CONTROL, ABOX_MUTE_CNT_FOR_PATH_CHANGE);

            if (aproxy->active_playback_ausage != AUSAGE_NONE &&
                aproxy->active_playback_device != DEVICE_NONE) {
                disable_internal_path(aproxy, aproxy->active_playback_ausage,
                                      aproxy->active_playback_device, routed_device);
                set_reroute(aproxy, aproxy->active_playback_ausage, aproxy->active_playback_device,
                                    routed_ausage, routed_device);
            } else {
                set_route(aproxy, routed_ausage, routed_device);
            }

            aproxy->active_playback_ausage = routed_ausage;
            aproxy->active_playback_device = routed_device;

            // Audio Path Modifier for Playback Path
            if (routed_modifier < MODIFIER_BT_SCO_TX_NB) {
                proxy_set_mixer_value_int(proxy, MIXER_CTL_ABOX_SIFS2_SWITCH, MIXER_OFF);
                if (aproxy->active_playback_modifier == MODIFIER_NONE)
                    set_modifier(aproxy, routed_modifier);
                else
                    update_modifier(aproxy, aproxy->active_playback_modifier, routed_modifier);

                proxy_set_mixer_value_int(proxy, MIXER_CTL_ABOX_SIFS2_SWITCH, MIXER_ON);
            } else if (routed_modifier == MODIFIER_NONE && aproxy->active_playback_modifier != MODIFIER_NONE) {
                reset_modifier(aproxy, aproxy->active_playback_modifier);
            }

            aproxy->active_playback_modifier = routed_modifier;

            if (aproxy->audio_mode == AUDIO_MODE_IN_CALL ||
                aproxy->audio_mode == AUDIO_MODE_IN_COMMUNICATION) {
                    proxy_set_call_path_param(CALL_PATH_SET, RX_DEVICE, routed_device);
            }

            // Set Loopback for Playback Path
            enable_internal_path(aproxy, routed_ausage, routed_device);
        } else {
            // Audio Path Routing for Capture Path
            if (aproxy->active_capture_ausage != AUSAGE_NONE &&
                aproxy->active_capture_device != DEVICE_NONE) {
                disable_internal_path(aproxy, aproxy->active_capture_ausage,
                                      aproxy->active_capture_device, routed_device);
                set_reroute(aproxy, aproxy->active_capture_ausage, aproxy->active_capture_device,
                                    routed_ausage, routed_device);
            } else {
                // In case of capture routing setup, it needs A-Box early-wakeup
                proxy_set_mixercontrol(aproxy, TICKLE_CONTROL, ABOX_TICKLE_ON);

                set_route(aproxy, routed_ausage, routed_device);
            }

            aproxy->active_capture_ausage = routed_ausage;
            aproxy->active_capture_device = routed_device;

            // Audio Path Modifier for Capture Path
            if (routed_modifier >= MODIFIER_BT_SCO_TX_NB && routed_modifier < MODIFIER_NONE) {
                proxy_set_mixer_value_int(proxy, MIXER_CTL_ABOX_SIFS2_SWITCH, MIXER_OFF);
                if (aproxy->active_capture_modifier == MODIFIER_NONE)
                    set_modifier(aproxy, routed_modifier);
                else
                    update_modifier(aproxy, aproxy->active_capture_modifier, routed_modifier);

                proxy_set_mixer_value_int(proxy, MIXER_CTL_ABOX_SIFS2_SWITCH, MIXER_ON);
            } else if (routed_modifier == MODIFIER_NONE && aproxy->active_capture_modifier != MODIFIER_NONE) {
                reset_modifier(aproxy, aproxy->active_capture_modifier);
            }

            aproxy->active_capture_modifier = routed_modifier;

            // Set Loopback for Capture Path
            enable_internal_path(aproxy, routed_ausage, routed_device);

            if (aproxy->audio_mode == AUDIO_MODE_IN_COMMUNICATION)
                proxy_set_call_path_param(CALL_PATH_SET, PARAM_NONE, 0);
        }
    } else {
        // Reset Loopback
        disable_internal_path(aproxy, routed_ausage, routed_device, DEVICE_NONE);

        // Audio Path Modifier
        if (routed_modifier != MODIFIER_NONE) {
            reset_modifier(aproxy, routed_modifier);

            if (routed_modifier < MODIFIER_BT_SCO_TX_NB)
                aproxy->active_playback_modifier = MODIFIER_NONE;
            else
                aproxy->active_capture_modifier = MODIFIER_NONE;
        } else {
            aproxy->active_playback_modifier = MODIFIER_NONE;
            aproxy->active_capture_modifier = MODIFIER_NONE;
        }

        // Audio Path Routing
        reset_route(aproxy, routed_ausage, routed_device);

        if (routed_device < DEVICE_MAIN_MIC) {
            aproxy->active_playback_ausage = AUSAGE_NONE;
            aproxy->active_playback_device = DEVICE_NONE;
        } else {
            aproxy->active_capture_ausage = AUSAGE_NONE;
            aproxy->active_capture_device = DEVICE_NONE;
        }
    }

    return true;
}


/*
 *  Proxy Voice Call Control
 */
void  proxy_stop_voice_call(void *proxy)
{
    struct audio_proxy *aproxy = (struct audio_proxy *)proxy;

    voice_rx_stop(aproxy);
    voice_tx_stop(aproxy);

    return ;
}

void proxy_start_voice_call(void *proxy)
{
    struct audio_proxy *aproxy = (struct audio_proxy *)proxy;

    voice_rx_start(aproxy);
    voice_tx_start(aproxy);

    return ;
}

// General Mixer Control Functions
int proxy_get_mixer_value_int(void *proxy, const char *name)
{
    struct audio_proxy *aproxy = proxy;
    struct mixer_ctl *ctrl = NULL;
    int ret = -1;

    if (name == NULL)
        return ret;

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);

    ctrl = mixer_get_ctl_by_name(aproxy->mixer, name);
    if (ctrl) {
        ret = mixer_ctl_get_value(ctrl, 0);
    } else {
        ALOGE("proxy-%s: cannot find %s Mixer Control", __func__, name);
    }

    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return ret;
}

int proxy_get_mixer_value_array(void *proxy, const char *name, void *value, int count)
{
    struct audio_proxy *aproxy = proxy;
    struct mixer_ctl *ctrl = NULL;
    int ret = -1;

    if (name == NULL)
        return ret;

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);

    ctrl = mixer_get_ctl_by_name(aproxy->mixer, name);
    if (ctrl) {
        ret = mixer_ctl_get_array(ctrl, value, count);
    } else {
        ALOGE("proxy-%s: cannot find %s Mixer Control", __func__, name);
    }

    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return ret;
}

void proxy_set_mixer_value_int(void *proxy, const char *name, int value)
{
    struct audio_proxy *aproxy = proxy;
    struct mixer_ctl *ctrl = NULL;
    int ret = 0, val = value;

    if (name == NULL)
        return ;

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);

    ctrl = mixer_get_ctl_by_name(aproxy->mixer, name);
    if (ctrl) {
        ret = mixer_ctl_set_value(ctrl, 0, val);
        if (ret != 0)
            ALOGE("proxy-%s: failed to set %s", __func__, name);
    } else {
        ALOGE("proxy-%s: cannot find %s Mixer Control", __func__, name);
    }

    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return ;
}

void proxy_set_mixer_value_string(void *proxy, const char *name, const char *value)
{
    struct audio_proxy *aproxy = proxy;
    struct mixer_ctl *ctrl = NULL;
    int ret = 0;

    if (name == NULL)
        return ;

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);

    ctrl = mixer_get_ctl_by_name(aproxy->mixer, name);
    if (ctrl) {
        ret = mixer_ctl_set_enum_by_string(ctrl, value);
        if (ret != 0)
            ALOGE("proxy-%s: failed to set %s", __func__, name);
    } else {
        ALOGE("proxy-%s: cannot find %s Mixer Control", __func__, name);
    }

    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return ;
}

void proxy_set_mixer_value_array(void *proxy, const char *name, const void *value, int count)
{
    struct audio_proxy *aproxy = proxy;
    struct mixer_ctl *ctrl = NULL;
    int ret = 0;

    if (name == NULL)
        return ;

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);

    ctrl = mixer_get_ctl_by_name(aproxy->mixer, name);
    if (ctrl) {
        ret = mixer_ctl_set_array(ctrl, value, count);
        if (ret != 0)
            ALOGE("proxy-%s: failed to set %s", __func__, name);
    } else {
        ALOGE("proxy-%s: cannot find %s Mixer Control", __func__, name);
    }

    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return ;
}

// Specific Mixer Control Functions
void proxy_set_audiomode(void *proxy, int audiomode)
{
    struct audio_proxy *aproxy = proxy;
    struct mixer_ctl *ctrl = NULL;
    int ret = 0, val = audiomode;

    if ((aproxy->audio_mode == AUDIO_MODE_IN_CALL || aproxy->audio_mode == AUDIO_MODE_IN_COMMUNICATION) &&
         audiomode == AUDIO_MODE_NORMAL)
        clr_call_path_param();

    aproxy->audio_mode = val; // set audio mode

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);

    /* Set Audio Mode to Kernel */
    ctrl = mixer_get_ctl_by_name(aproxy->mixer, ABOX_AUDIOMODE_CONTROL_NAME);
    if (ctrl) {
        ret = mixer_ctl_set_value(ctrl, 0,val);
        if (ret != 0)
            ALOGE("proxy-%s: failed to set Android AudioMode to Kernel", __func__);
    } else {
        ALOGE("proxy-%s: cannot find AudioMode Mixer Control", __func__);
    }

    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return ;
}

void proxy_set_volume(void *proxy, int volume_type, float left, float right)
{
    struct audio_proxy *aproxy = proxy;
    uint32_t call_vol = 0;
    uint32_t call_mute = 0;

    if (volume_type == VOLUME_TYPE_CALL) {
        call_vol = right * RXSE_VOL_MAX;
        proxy_set_mixer_value_int(aproxy, ABOX_RXSE_VOL, call_vol);
    } else if (volume_type == VOLUME_TYPE_CALL_MUTE) {
        call_mute = right;  // 0 is unmute, 1 is mute
        proxy_set_mixer_value_int(aproxy, ABOX_TXSE_MUTE, call_mute);
    }

    return;
}
void proxy_set_communication_volume(void *proxy, int volume)
{

    struct audio_proxy *aproxy = proxy;
    struct mixer_ctl *ctrl = NULL;
    int ret = 0, val = volume;

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);

    ctrl = mixer_get_ctl_by_name(aproxy->mixer, AP_CALL_VOLUME_NAME);
    if (ctrl) {
        ret = mixer_ctl_set_value(ctrl, 0, val);
        if (ret != 0)
            ALOGE("proxy-%s: failed to set %d", __func__, val);
    } else {
        ALOGE("proxy-%s: cannot find %d Mixer Control", __func__, val);
    }

    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return ;
}

void proxy_clear_apcall_txse(void)
{
    struct audio_proxy *aproxy = getInstance();
    char path_name[MAX_PATH_NAME_LEN];
    audio_usage ausage = aproxy->active_capture_ausage;

    memset(path_name, 0, MAX_PATH_NAME_LEN);

    if (snprintf(path_name, MAX_PATH_NAME_LEN - 1, "set-%s-txse", usage_path_table[ausage]) < 0) {
        ALOGE("proxy-%s: path name has error: %s", __func__, strerror(errno));
        return;
    }

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);

    audio_route_reset_and_update_path(aproxy->aroute, path_name);
    ALOGI("proxy-%s: %s is disabled", __func__, path_name);

    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return ;
}

void proxy_set_apcall_txse(void)
{
    struct audio_proxy *aproxy = getInstance();
    char path_name[MAX_PATH_NAME_LEN];
    audio_usage ausage = aproxy->active_capture_ausage;

    memset(path_name, 0, MAX_PATH_NAME_LEN);

    if (snprintf(path_name, MAX_PATH_NAME_LEN - 1, "set-%s-txse", usage_path_table[ausage]) < 0) {
        ALOGE("proxy-%s: path name has error: %s", __func__, strerror(errno));
        return;
    }

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);

    audio_route_apply_and_update_path(aproxy->aroute, path_name);
    ALOGI("proxy-%s: %s is enabled", __func__, path_name);

    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return ;
}

void proxy_set_upscale(void *proxy, int sampling_rate, int pcm_format)
{
    return;
}

void proxy_call_status(void *proxy, int status)
{
    struct audio_proxy *aproxy = (struct audio_proxy *)proxy;

    /* status : TRUE means call starting
        FALSE means call stopped
    */
    if (status)
        aproxy->call_state = true;
    else
        aproxy->call_state = false;

    return;
}

int proxy_set_parameters(void *proxy, void *parameters)
{
    struct audio_proxy *aproxy = (struct audio_proxy *)proxy;
    struct str_parms *parms = (struct str_parms *)parameters;
    int val;
    int ret = 0;     // for parameter handling
    int status = 0;  // for return value

    if (aproxy == NULL)
        return -1;

    ret = str_parms_get_int(parms, AUDIO_PARAMETER_DEVICE_CONNECT, &val);
    if (ret >= 0) {
        if ((audio_devices_t)val == AUDIO_DEVICE_IN_WIRED_HEADSET) {
            ALOGD("proxy-%s: Headset Device connected 0x%x", __func__, val);
        }
    }

    ret = str_parms_get_int(parms, AUDIO_PARAMETER_DEVICE_DISCONNECT, &val);
    if (ret >= 0) {
        if ((audio_devices_t)val == AUDIO_DEVICE_IN_WIRED_HEADSET) {
            ALOGD("proxy-%s: Headset Device disconnected 0x%x", __func__, val);
        }
    }

    return status;
}

int proxy_get_microphones(void *proxy, void *array, int *count)
{
    struct audio_proxy *aproxy = (struct audio_proxy *)proxy;
    struct audio_microphone_characteristic_t *mic_array = array;
    size_t *mic_count = (size_t *)count;
    size_t actual_mic_count = 0;
    int ret = 0;

    if (aproxy) {
        if (*mic_count == 0) {
            *mic_count = (size_t)aproxy->num_mic;
            ALOGI("proxy-%s: requested number of microphone, return %zu", __func__, *mic_count);
        } else {
            for (int i = 0; i < (int)*mic_count && i < aproxy->num_mic; i++) {
                mic_array[i] = aproxy->mic_info[i];
                ALOGD("proxy-%s: %dth MIC = %s", __func__, i+1, mic_array[i].device_id);
                actual_mic_count++;
            }
            *mic_count = actual_mic_count;
        }
    } else {
        ALOGE("proxy-%s: aproxy is NULL", __func__);
        ret = -ENOSYS;
    }

    return ret;
}

void proxy_init_offload_effect_lib(void *proxy)
{
    return;
}

void proxy_update_offload_effect(void *proxy, int type)
{
    return;
}

/*
 *  Proxy Dump
 */
int proxy_fw_dump(int fd)
{
    ALOGV("proxy-%s: enter with file descriptor(%d)", __func__, fd);

    calliope_ramdump(fd);

    ALOGV("proxy-%s: exit with file descriptor(%d)", __func__, fd);

    return 0;
}


/*
 *  Proxy Device Creation/Destruction
 */
static bool find_enum_from_string(struct audio_string_to_enum *table, const char *name,
                                  int32_t table_cnt, int *value)
{
    int i;

    for (i = 0; i < table_cnt; i++) {
        if (strcmp(table[i].name, name) == 0) {
            *value = table[i].value;
            return true;
        }
    }
    return false;
}

static void set_microphone_info(struct audio_microphone_characteristic_t *microphone, const XML_Char **attr)
{
    uint32_t curIdx = 0;
    uint32_t array_cnt = 0;
    float f_value[3] = {0, };
    char *ptr = NULL;

    if (strcmp(attr[curIdx++], "device_id") == 0)
        strcpy(microphone->device_id, attr[curIdx++]);

    if (strcmp(attr[curIdx++], "id") == 0)
        microphone->id = atoi(attr[curIdx++]);

    if (strcmp(attr[curIdx++], "device") == 0)
        find_enum_from_string(device_in_type, attr[curIdx++], ARRAY_SIZE(device_in_type), (int *)&microphone->device);

    if (strcmp(attr[curIdx++], "address") == 0)
        strcpy(microphone->address, attr[curIdx++]);

    if (strcmp(attr[curIdx++], "location") == 0)
        find_enum_from_string(microphone_location, attr[curIdx++], AUDIO_MICROPHONE_LOCATION_CNT, (int *)&microphone->location);

    if (strcmp(attr[curIdx++], "group") == 0)
        microphone->group = atoi(attr[curIdx++]);

    if (strcmp(attr[curIdx++], "index_in_the_group") == 0)
        microphone->index_in_the_group = atoi(attr[curIdx++]);

    if (strcmp(attr[curIdx++], "sensitivity") == 0)
        microphone->sensitivity = atof(attr[curIdx++]);

    if (strcmp(attr[curIdx++], "max_spl") == 0)
        microphone->max_spl = atof(attr[curIdx++]);

    if (strcmp(attr[curIdx++], "min_spl") == 0)
        microphone->min_spl = atof(attr[curIdx++]);

    if (strcmp(attr[curIdx++], "directionality") == 0)
        find_enum_from_string(microphone_directionality, attr[curIdx++],
                              AUDIO_MICROPHONE_LOCATION_CNT, (int *)&microphone->directionality);

    if (strcmp(attr[curIdx++], "num_frequency_responses") == 0) {
        microphone->num_frequency_responses = atoi(attr[curIdx++]);
        if (microphone->num_frequency_responses > 0) {
            if (strcmp(attr[curIdx++], "frequencies") == 0) {
                ptr = strtok((char *)attr[curIdx++], " ");
                while(ptr != NULL) {
                    microphone->frequency_responses[0][array_cnt++] = atof(ptr);
                    ptr = strtok(NULL, " ");
                }
            }
            array_cnt = 0;
            if (strcmp(attr[curIdx++], "responses") == 0) {
                ptr = strtok((char *)attr[curIdx++], " ");
                while(ptr != NULL) {
                    microphone->frequency_responses[1][array_cnt++] = atof(ptr);
                    ptr = strtok(NULL, " ");
                }
            }
        }
    }

    if (strcmp(attr[curIdx++], "geometric_location") == 0) {
        ptr = strtok((char *)attr[curIdx++], " ");
        array_cnt = 0;
        while (ptr != NULL) {
            f_value[array_cnt++] = atof(ptr);
            ptr = strtok(NULL, " ");
        }
        microphone->geometric_location.x = f_value[0];
        microphone->geometric_location.y = f_value[1];
        microphone->geometric_location.z = f_value[2];
    }

    if (strcmp(attr[curIdx++], "orientation") == 0) {
        ptr = strtok((char *)attr[curIdx++], " ");
        array_cnt = 0;
        while (ptr != NULL) {
            f_value[array_cnt++] = atof(ptr);
            ptr = strtok(NULL, " ");
        }
        microphone->orientation.x = f_value[0];
        microphone->orientation.y = f_value[1];
        microphone->orientation.z = f_value[2];
    }

    /* Channel mapping doesn't used for now. */
    for (array_cnt = 0; array_cnt < AUDIO_CHANNEL_COUNT_MAX; array_cnt++)
        microphone->channel_mapping[array_cnt] = AUDIO_MICROPHONE_CHANNEL_MAPPING_UNUSED;
}

static void end_tag(void *data, const XML_Char *tag_name)
{
    if (strcmp(tag_name, "microphone_characteristis") == 0)
        set_info = INFO_NONE;
}

static void start_tag(void *data, const XML_Char *tag_name, const XML_Char **attr)
{
    struct audio_proxy *aproxy = getInstance();

    if (strcmp(tag_name, "microphone_characteristics") == 0) {
        set_info = MICROPHONE_CHARACTERISTIC;
    } else if (strcmp(tag_name, "microphone") == 0) {
        if (set_info != MICROPHONE_CHARACTERISTIC)
            ALOGE("proxy-%s microphone tag should be supported with microphone_characteristics tag", __func__);
        set_microphone_info(&aproxy->mic_info[aproxy->num_mic++], attr);
    }
}

void proxy_set_board_info()
{
    XML_Parser parser = 0;
    FILE *file = NULL;
    char info_file_name[MAX_MIXER_NAME_LEN] = {0};
    void *buf = NULL;
    uint32_t buf_size = 1024;
    int32_t bytes_read = 0;

    strlcpy(info_file_name, BOARD_INFO_XML_PATH, MAX_MIXER_NAME_LEN);

    file = fopen(info_file_name, "r");
    if (file == NULL)
        ALOGE("proxy-%s: open error: %s, file=%s", __func__, strerror(errno), info_file_name);
    else
        ALOGI("proxy-%s: Board info file name is %s", __func__, info_file_name);

    parser = XML_ParserCreate(NULL);

    XML_SetElementHandler(parser, start_tag, end_tag);

    while (1) {
        buf = XML_GetBuffer(parser, buf_size);
        if (buf == NULL) {
            ALOGE("proxy-%s fail to get buffer", __func__);
            break;
        }

        bytes_read = fread(buf, 1, buf_size, file);
        if (bytes_read < 0) {
            ALOGE("proxy-%s fail to read from file", __func__);
            break;
        }

        XML_ParseBuffer(parser, bytes_read, bytes_read == 0);

        if (bytes_read == 0)
            break;
    }

    XML_ParserFree(parser);
    fclose(file);
}

bool proxy_is_initialized(void)
{
    return instance ? true : false;
}

void * proxy_init(void)
{
    struct audio_proxy *aproxy;

    /* Creates the structure for audio_proxy. */
    aproxy = getInstance();
    if (!aproxy) {
        ALOGE("proxy-%s: failed to create for audio_proxy", __func__);
        return NULL;
    }

    aproxy->primary_out = NULL;

    // In case of Output Loopback Support, initializes Out Loopback Stream
    aproxy->support_out_loopback = true;
    aproxy->out_loopback = NULL;
    aproxy->erap_in = NULL;

    // In case of External Speaker AMP Support, initializes Reference & Playback Stream
    aproxy->spkamp_reference = NULL;
    aproxy->spkamp_playback = NULL;

    // In case of External BT-SCO Support, initializes Playback Stream
    aproxy->support_btsco = true;
    aproxy->btsco_playback = NULL;

    // Voice Call PCM Devices
    aproxy->call_rx = NULL;
    aproxy->call_tx = NULL;

    // FM Radio PCM Devices
    aproxy->fm_playback = NULL;
    aproxy->fm_capture  = NULL;

    // BT SCO erap PCM flags
    aproxy->btsco_erap_flag[0] = (PCM_IN | PCM_MONOTONIC);
    aproxy->btsco_erap_flag[1] = (PCM_OUT | PCM_MONOTONIC);

    // Call State
    aproxy->call_state = false;

    // Audio Mode
    aproxy->audio_mode = AUDIO_MODE_NORMAL;

    proxy_set_board_info();

    ALOGI("proxy-%s: opened & initialized Audio Proxy", __func__);
    return (void *)aproxy;
}

void proxy_deinit(void *proxy)
{
    struct audio_proxy *aproxy = (struct audio_proxy *)proxy;

    if (aproxy) {
        destroyInstance();
        ALOGI("proxy-%s: destroyed for audio_proxy", __func__);
    }
    return ;
}

