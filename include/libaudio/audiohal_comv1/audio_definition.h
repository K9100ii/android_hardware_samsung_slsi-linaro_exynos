/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef __EXYNOS_AUDIOHAL_DEFINITION_H__
#define __EXYNOS_AUDIOHAL_DEFINITION_H__

/*
 * This header file has common definition and parameter for AudioHAL and AudioProxy
 */

/* Stream Status */
typedef enum {
    STATUS_STANDBY   = 0,   // Stream is opened, but Device(PCM or Compress) is not opened yet.
    STATUS_READY,           // Stream is opened, but Device(PCM or Compress) is not opened yet. But, started something
    STATUS_IDLE,            // Stream is opened & Device(PCM or Compress) is opened.
    STATUS_PLAYING,         // Stream is opened & Device(PCM or Compress) is opened & Device is working.
    STATUS_PAUSED,          // Stream is opened & Device(Compress) is opened & Device is pausing.(only available for Compress Offload Stream)
} stream_status;

/* Call Mode */
typedef enum {
    CALL_OFF   = 0,         // No Call Mode
    VOICE_CALL,             // CP Centric Voice Call Mode
    VOLTE_CALL,             // CP Centric VOLTE Call Mode
    VOWIFI_CALL,            // AP Centric VoWiFi Call Mode

    CALL_MODE_CNT,
    CALL_MODE_MAX   = CALL_MODE_CNT - 1,
} call_mode;

/* Macro for Routing */
#define ROUTE               true
#define UNROUTE             false

typedef enum {
    NON_FORCE_ROUTE     = 0,
    FORCE_ROUTE,
    CALL_DRIVE
} force_route;

#define PREDEFINED_CAPTURE_DURATION     20   // 20ms
#define LOW_LATENCY_CAPTURE_SAMPLE_RATE 48000

#define MAX_MIXER_LENGTH    256
#define DEFAULT_MIXER_PATH  "/vendor/etc/"
#define DEFAULT_MIXER_FILE  "mixer_paths.xml"
#define MIXER_PATH_INFO     "/proc/device-tree/sound/mixer-paths"

// Duration for Normal Capture
#define PREDEFINED_MEDIA_CAPTURE_DURATION   20  // 20ms
#define PREDEFINED_LOW_CAPTURE_DURATION     4   // 4ms

#define PREDEFINED_DEFAULT_PLAYBACK_DURATION    10  // 10ms

// Duration for USB Playback and Capture
#define PREDEFINED_USB_PLAYBACK_DURATION    5  // 5ms usb_outcom
#define PREDEFINED_USB_CAPTURE_DURATION     5  // 5ms usb_incom

/* Direct Stream playback Volume Unit */
#define DIRECT_PLAYBACK_VOLUME_MAX   8192

/* definitions for mixer ctl */
// info for next stream
#define MIXER_CTL_ABOX_PRIMARY_WIDTH        "ABOX UAIF0 width"
#define MIXER_CTL_ABOX_PRIMARY_CHANNEL      "ABOX UAIF0 channel"
#define MIXER_CTL_ABOX_PRIMARY_SAMPLERATE   "ABOX UAIF0 rate"
// info for current stream
#define MIXER_CTL_ABOX_CURRENT_WIDTH        "ABOX Bit Width Mixer"

#define MIXER_CTL_OFFLOAD_UPSCALER          "UPSCALER"
#define MIXER_CTL_OFFLOAD_FORMAT            "ComprTx0 Format"

#define MMAP_MIN_SIZE_FRAMES_MAX            64 * 1024

#endif  // __EXYNOS_AUDIOHAL_DEFINITION_H__
