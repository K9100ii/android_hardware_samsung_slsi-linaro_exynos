/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef AUDIO_MIXERS_H
#define AUDIO_MIXERS_H

/* Mixer Values Definition */
enum {
    MIXER_VALUE_OFF   = 0,
    MIXER_VALUE_ON,
};

#define MIXER_CTL_VAL_INVALID       -1

/* Specific Mixer Name */
// info for next stream
#define MIXER_CTL_ABOX_PRIMARY_WIDTH        "ABOX UAIF0 Width"
#define MIXER_CTL_ABOX_PRIMARY_CHANNEL      "ABOX UAIF0 Channel"
#define MIXER_CTL_ABOX_PRIMARY_SAMPLERATE   "ABOX UAIF0 Rate"

//add for Speaker Reference mute
#define MIXER_CTL_SPK_REF_MUTE              "ABOX OEM REF LCH MUTE"


#endif /* AUDIO_MIXERS */
