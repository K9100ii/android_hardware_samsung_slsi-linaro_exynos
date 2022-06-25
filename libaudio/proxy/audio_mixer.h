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

#ifndef __EXYNOS_AUDIOPROXY_MIXER_H__
#define __EXYNOS_AUDIOPROXY_MIXER_H__

#include <audio_route/audio_route.h>

/* Mixer Card Definition */
#define MIXER_CARD0     0


#define MAX_PATH_NAME_LEN 50
#define MAX_GAIN_PATH_NAME_LEN 55 //"gain-" + path_name size

/* Mixer Controls for ERAP Handling */
#define MAX_MIXER_NAME_LEN 50

// Mixer Control for set USB Mode
//#define ABOX_USBMODE_CONTROL_NAME   "ABOX ERAP info USB On"

/**
 ** Sampling Rate, channels & format Modifier Configuration for USB playback
 **/
// SIFS0 config controls
#define ABOX_SAMPLE_RATE_MIXER_NAME     "ABOX SIFS0 Rate"
#define ABOX_CHANNELS_MIXER_NAME        "ABOX SIFS0 Channel"
#define ABOX_BIT_WIDTH_MIXER_NAME       "ABOX SIFS0 Width"
// WDMA1 config controls
#define ABOX_SAMPLE_RATE_WDMA1_NAME     "ABOX WDMA1 Rate"
#define ABOX_CHANNELS_WDMA1_NAME        "ABOX WDMA1 Channel"
#define ABOX_BIT_WIDTH_WDMA1_NAME       "ABOX WDMA1 Width"
#define ABOX_PERIOD_SIZE_WDMA1_NAME     "ABOX WDMA1 Period"

// Mixer Control for set MUTE Control
#define ABOX_MUTE_CONTROL_NAME "ABOX ERAP info Mute Primary"
#define ABOX_MUTE_CNT_FOR_PATH_CHANGE 15

// Mixer Control for set A-Box Early WakeUp Control
#define ABOX_TICKLE_CONTROL_NAME "ABOX Tickle"
#define ABOX_TICKLE_ON      1

typedef enum {
    /* USBMODE         = 0, */
    MUTE_CONTROL    = 1,
    TICKLE_CONTROL
} erap_trigger;

// Mixer Control for set Android Audio Mode
#define ABOX_AUDIOMODE_CONTROL_NAME "ABOX Audio Mode"

// AP Call volume
#define AP_CALL_VOLUME_NAME "ABOX Speech Volume"

typedef enum {
    UPSCALE_NONE        = 0,
    UPSCALE_48K_16B,
    UPSCALE_48K_24B,
    UPSCALE_192K_24B,
    UPSCALE_384K_24B,
} upscale_factor;

// Mixer for Call Path Parameter
#define ABOX_CALL_PATH_PARAM "ABOX Call Path Param"
#define ABOX_RXSE_VOL        "ABOX RXSE Volume"
#define ABOX_TXSE_MUTE       "ABOX TXSE Mute"
#define RXSE_VOL_MAX         5  // could be changed by Customer

/* Call parameter - CALL_TYPE from RIL */
typedef enum {
    GSM = 1,
    CDMA = 2,
    IMS = 4,
    ETC = 8,
} call_type_fromRil;

/* Call parameter - CHANNEL */
enum {
    MONO = 1,
    STEREO,
};
/* Call parameter - The number of Mic */
enum {
    MIC0 = 0,
    MIC1,
    MIC2,
};

/* Call parameter - Device */
enum {
    HANDSET = 0,
    HEADPHONE,
    HEADSET,
    SPEAKER,
    BT_SCO
    /* USB_HEADSET */
};

#define MIXER_CTL_ABOX_SIFS0_SWITCH      "ABOX SIFS0 OUT Switch"
#define MIXER_CTL_ABOX_SIFS2_SWITCH      "ABOX SIFS2 OUT Switch"

#define MIXER_ON                         1
#define MIXER_OFF                        0

#endif  // __EXYNOS_AUDIOPROXY_MIXER_H__
