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

#ifndef __EXYNOS_AUDIOHAL_DEVICE_H__
#define __EXYNOS_AUDIOHAL_DEVICE_H__

/**
 ** Audio Input/Output Device based on Target Device
 **/
typedef enum {
    DEVICE_MIN                    = 0,

    // Single path Playback Devices
    DEVICE_HANDSET                = 0,   // handset or receiver
    DEVICE_SPEAKER,
    DEVICE_HEADSET,                      // headphone + mic
    DEVICE_HEADPHONE,                    // headphone or earphone
    DEVICE_BT_SCO_HEADSET,
    DEVICE_USB_HEADSET,
    DEVICE_AUX_DIGITAL,

    // Dual path Playback Devices
    DEVICE_SPEAKER_AND_HEADSET,
    DEVICE_SPEAKER_AND_HEADPHONE,
    DEVICE_SPEAKER_AND_BT_SCO_HEADSET,
    DEVICE_SPEAKER_AND_USB_HEADSET,

    // BT A2DP Offload playback devices
#ifdef SUPPORT_BTA2DP_OFFLOAD
    DEVICE_BT_A2DP_HEADPHONE,
    DEVICE_SPEAKER_AND_BT_A2DP_HEADPHONE,
#endif

    // Note: Request to add new ODM specific playback devices below
    // ODM specific Playback Devices
    DEVICE_SPEAKER_DEX,
    DEVICE_SPEAKER_DUAL,
    DEVICE_SPEAKER2,
    DEVICE_SPEAKER_GAMING,
    DEVICE_FM_EXTERNAL,

    DEVICE_CALL_FWD,
    DEVICE_SPECTRO,
    // Capture Devices
    DEVICE_MAIN_MIC,
    DEVICE_SUB_MIC,
    DEVICE_HEADSET_MIC,
    DEVICE_BT_SCO_HEADSET_MIC,
    DEVICE_USB_HEADSET_MIC,
    DEVICE_HANDSET_MIC,
    DEVICE_SPEAKER_MIC,
    DEVICE_HEADPHONE_MIC,

    // FM Tuner capture device
    DEVICE_FM_TUNER,

    // TTY capture devices
    DEVICE_FULL_MIC,
    DEVICE_HCO_MIC,
    DEVICE_VCO_MIC,

    // Note: Request to add new ODM specific capture devices below
    // ODM specific capture devices
    DEVICE_SPEAKER_DEX_MIC,
    DEVICE_SPEAKER_GAMING_MIC,
    DEVICE_BT_NREC_HEADSET_MIC,

    DEVICE_STEREO_MIC,
    DEVICE_THIRD_MIC,
    DEVICE_USB_FULL_MIC,
    DEVICE_USB_HCO_MIC,

    DEVICE_NONE,
    DEVICE_MAX,
    DEVICE_CNT                   = DEVICE_MAX
} device_type;


/**
 ** Audio Input/Output Sampling Rate Modifier
 **/
typedef enum {
    MODIFIER_MIN          = 0,

    /* Playback Path modifier */
    MODIFIER_BT_SCO_RX_NB = 0,
    MODIFIER_BT_SCO_RX_WB,
    MODIFIER_BT_A2DP_PLAYBACK,

    /* Capture Path modifier */
    MODIFIER_BT_SCO_TX_NB,
    MODIFIER_BT_SCO_TX_WB,

    MODIFIER_NONE,
    MODIFIER_MAX,
    MODIFIER_CNT          = MODIFIER_MAX
} modifier_type;


#endif  // __EXYNOS_AUDIOHAL_DEVICE_H__
