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

#ifndef ARCSOFT_CALI_H
#define ARCSOFT_CALI_H

#include <utils/Log.h>

#include "exynos_format.h"
#include "PlugInCommon.h"

#include "amcomdef.h"
#include "asvloffscreen.h"

#include "libarcsoft_dualcam_calibration.h"
#include "libarcsoft_dualcam_verification.h"

#define ARCSOFT_CALI_DATA_PATH "/data/misc/cameraserver/arcsoft_cali.bin"
#define RMO_CALI_DATA_PATH "/data/misc/cameraserver/rmo_cali.bin"
#define ARCSOFT_RMO_DATA_PATH "/data/misc/cameraserver/RMO_imx350.bin"
#define ARCSOFT_RESULT_DATA_PATH "/data/misc/cameraserver/dualCam_Verify_result.txt"
#define ARCSOFT_RMO_DATA_SEEK 13572

using namespace android;

class ArcsoftCaliLib
{
public:
    static ArcsoftCaliLib & getInstance() {
        static ArcsoftCaliLib inst;
        return inst;
    }
    void execute(ASVLOFFSCREEN leftImg, ASVLOFFSCREEN rightImg, Map_t *map);
private:
    MByte m_calData[ARC_DCVF_CALDATA_LEN];
private:
    ArcsoftCaliLib();
    bool m_doVeri(ASVLOFFSCREEN leftImg, ASVLOFFSCREEN rightImg);
    bool m_doCali(ASVLOFFSCREEN leftImg, ASVLOFFSCREEN rightImg);
    void dumpYUVtoFile(ASVLOFFSCREEN *pAsvl, char* name_prefix);
};

#endif
