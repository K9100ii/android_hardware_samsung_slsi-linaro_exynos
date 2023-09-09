/*
**
** Copyright 2013, Samsung Electronics Co. LTD
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#ifndef EXYNOS_CAMERA_DOF_LUT_3M3_H
#define EXYNOS_CAMERA_DOF_LUT_3M3_H

#include "ExynosCameraFusionInclude.h"

const DOF_LUT DOF_LUT_3M3[] =
{
    DOF_LUT(10000,  0.004,  DEFAULT_DISTANCE_FAR, 3700),
    DOF_LUT( 5000,  0.007,  339580,               2700),
    DOF_LUT( 4000,  0.009,  125700,               2380),
    DOF_LUT( 3000,  0.012,   61330,               1990),
    DOF_LUT( 2000,  0.018,    3030,               1490),
    DOF_LUT( 1900,  0.019,    2810,               1440),
    DOF_LUT( 1800,  0.020,    2590,               1380),
    DOF_LUT( 1700,  0.022,    2390,               1320),
    DOF_LUT( 1600,  0.023,    2200,               1260),
    DOF_LUT( 1500,  0.025,    2010,               1200),
    DOF_LUT( 1400,  0.026,    1840,               1130),
    DOF_LUT( 1300,  0.028,    1670,               1070),
    DOF_LUT( 1200,  0.031,    1510,               1000),
    DOF_LUT( 1100,  0.034,    1350,                930),
    DOF_LUT( 1000,  0.037,    1200,                860),
    DOF_LUT(  900,  0.041,    1060,                780),
    DOF_LUT(  800,  0.046,     920,                710),
    DOF_LUT(  700,  0.053,     790,                630),
    DOF_LUT(  600,  0.062,     670,                550),
    DOF_LUT(  500,  0.074,     550,                460),
    DOF_LUT(  450,  0.083,     486,                419),
    DOF_LUT(  400,  0.093,     428,                375),
    DOF_LUT(  350,  0.107,     371,                331),
    DOF_LUT(  300,  0.125,     316,                286),
    DOF_LUT(  250,  0.151,     261,                240),
    DOF_LUT(  200,  0.189,     207,                194),
    DOF_LUT(  150,  0.255,     154,                147),
    DOF_LUT(  140,  0.274,     143,                137),
    DOF_LUT(  130,  0.296,     133,                127),
    DOF_LUT(  120,  0.322,     122,                118),
    DOF_LUT(  110,  0.353,     112,                108),
    DOF_LUT(  100,  0.391,     102,                 99),
    DOF_LUT(   90,  0.437,      91,                 89),
    DOF_LUT(   80,  0.497,      81,                 79),
    DOF_LUT(   70,  0.574,      71,                 69),
    DOF_LUT(   60,  0.681,      61,                 60),
    DOF_LUT(   50,  0.836,      50,                 50),
};

struct DOF_3M3 : public DOF
{
    DOF_3M3() {
        lut = DOF_LUT_3M3;
        lutCnt = sizeof(DOF_LUT_3M3) / sizeof(DOF_LUT);

        lensShiftOn12M = 0.001f;
        lensShiftOn01M = 0.034f; // this is 0.4M's value.
    }
};

#endif // EXYNOS_CAMERA_DOF_LUT_3M3_H
