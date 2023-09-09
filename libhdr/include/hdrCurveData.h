/*
 * Copyright Samsung Electronics Co.,LTD.
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __HDR_CURVE_DATA_H__
#define __HDR_CURVE_DATA_H__

#include "dynamic_info_legacy.h"
#include "hdrMetaInterface.h"

#define META_JSON_2094_100K_PERCENTILE_DIVIDE    10.0
#define META_JSON_2094_EBZ_PCOEFF_BITS           10
#define META_JSON_2094_EBZ_PCOEFF_MAX            ((1 << META_JSON_2094_EBZ_PCOEFF_BITS)-1)
#define META_JSON_2094_EBZ_KNEE_POINT_BITS       12
#define META_JSON_2094_EBZ_KNEE_POINT_MAX        ((1 << META_JSON_2094_EBZ_KNEE_POINT_BITS)-1)

#define META_MAX_PSLL_SIZE                       15
#define META_MAX_PCOEFF_SIZE                     14

struct CurveParameters
{
    CurveParameters()
        : KPx(0), KPy(0), order(0), pcoeff{0} { }

    void setValues(const double sx, const double sy, const double *ps, const unsigned int ord)
    {
        KPx = sx;
        KPy = sy;
        order = ord;

        for (unsigned int i = 0; i < order - 1; i++)
            pcoeff[i] = ps[i];
    }

    void setValues(const unsigned short sx, const unsigned short sy, const unsigned short *ps, const unsigned short ord,
                   const double sNorm, const double pCoeffNorm)
    {
        KPx = sx / sNorm;
        KPy = sy / sNorm;
        order = ord;

        for (unsigned int i = 0; i < order - 1; i++)
            pcoeff[i] = ps[i] / pCoeffNorm;
    }

    void getValues(ExynosHdrDynamicInfo_t &meta, const double sNorm, const double pCoeffNorm)
    {
        meta.data.tone_mapping.knee_point_x = KPx * sNorm;
        meta.data.tone_mapping.knee_point_y = KPy * sNorm;

	meta.data.tone_mapping.num_bezier_curve_anchors = (order - 1);
        for (unsigned int i = 0; i < order - 1; i++)
            meta.data.tone_mapping.bezier_curve_anchors[i] = pcoeff[i] * pCoeffNorm;
    }

    double getPCoeffAt(unsigned int i) { return (isIndexInRange(i) ? pcoeff[i] : (static_cast<double> (i + 1.0) / order)); }
    bool isIndexInRange(const unsigned int i) const { return order > 0 && i < (order - 1); }

    double KPx;
    double KPy;
    unsigned int order;
    double pcoeff[META_MAX_PCOEFF_SIZE];
};

struct LuminanceParameters {
    void setValues(const unsigned int *max, const unsigned int *psll, const unsigned char *percent,
                   const unsigned char order, const double lumNorm)
    {
        percentileOrder = order;

        for (unsigned int i = 0; i < 3; i++)
            maxscl[i] = max[i] / lumNorm;
        for (unsigned int i = 0; i < order; i++) {
            percentage[i] = percent[i];
            percentile[i] = psll[i] / lumNorm;
        }
    }
    unsigned int getPercentIndexAt(unsigned int i) { return (isIndexInRange(i) ? percentage[i] : 0); }
    double getPercentileLuminanceAt(unsigned int i) { return (isIndexInRange(i) ? percentile[i] : 0); };
    bool isIndexInRange(const unsigned int idx) const { return percentileOrder && idx < percentileOrder; }

    unsigned int percentage[META_MAX_PSLL_SIZE];
    double percentile[META_MAX_PSLL_SIZE];

    float maxscl[3];
    unsigned int percentileOrder;
};

struct CurveInfo {
    int inDataspace;
    unsigned int maxInLumi;
    unsigned int maxOutLumi;
    class hdrMetaInterface* metaIf = nullptr;
};

float getSourceMaxLuminance(LuminanceParameters &luminanceParam);
int getMaxLuminance(int target_luminance, ExynosHdrDynamicInfo_t &dyn_meta);
void meta2meta(unsigned int targetMaxLuminance, unsigned int sourceMaxLuminance, ExynosHdrDynamicInfo_t &meta);
void convertDynamicMeta(ExynosHdrDynamicInfo_t *dyn_meta, ExynosHdrDynamicInfo *dynamic_metadata);
void genTMCurve(CurveInfo info, void *data,
                        std::vector<int> &arrX, std::vector<int> &arrY,
                        int numArray, int x_bits, int y_bits, int minx_bits);
void genTMCurve(CurveInfo info,
                        std::vector<int> &arrX, std::vector<int> &arrY,
                        int numArray, int x_bits, int y_bits, int minx_bits);
void genEOTFCurve(CurveInfo info,
                        std::vector<int> &arrX, std::vector<int> &arrY,
                        int numArray, int x_bits, int y_bits, int minx_bits);
#endif
