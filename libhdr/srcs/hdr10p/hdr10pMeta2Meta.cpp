/*
 *  Copyright Samsung Electronics Co.,LTD.
 *  Copyright (C) 2022 The Android Open Source Project
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include <algorithm>
#include <list>

#include "hdrUtil.h"
#include <system/graphics.h>
#include <log/log.h>

#ifndef HDR_TEST
#include <VendorVideoAPI.h>
#else
#include <VendorVideoAPI_hdrTest.h>
#endif

#include "hdrCurveData.h"

int getMaxLuminance(int target_luminance, ExynosHdrDynamicInfo_t &dyn_meta)
{
    LuminanceParameters luminanceParam;
    luminanceParam.setValues(dyn_meta.data.maxscl, dyn_meta.data.maxrgb_percentiles,
                             dyn_meta.data.maxrgb_percentages, dyn_meta.data.num_maxrgb_percentiles,
                             META_JSON_2094_100K_PERCENTILE_DIVIDE);
    int luminance_dyn = (int)getSourceMaxLuminance(luminanceParam);
    int max_luminance = (luminance_dyn > target_luminance) ? luminance_dyn : target_luminance;

    return max_luminance;
}

float getSourceMaxLuminance(LuminanceParameters &luminanceParam)
{
    float maxLuminance = 0;
    unsigned int idx = luminanceParam.percentileOrder;

    for (unsigned int i = 0; i < 3; i++)
        maxLuminance = std::max(maxLuminance, luminanceParam.maxscl[i]);
    maxLuminance = std::min(maxLuminance, 10000.0f);

    if ((idx > 0) && (luminanceParam.getPercentIndexAt(idx - 1) == 99))
        maxLuminance = std::min(luminanceParam.getPercentileLuminanceAt(idx - 1), static_cast<double> (maxLuminance));

    return maxLuminance;
}

// Fixed BasisOOTF / GudiedOOTF constants
static const int ORDER = 10;
static const int NPCOEFF = ORDER - 1;
static const float P1MIN = (1.0f / ORDER);

// Tunable BasisOOTF / GudiedOOTF parameters
struct BasisOOTF_Params
{
public:
    BasisOOTF_Params()
    {
        setDefault();
    }

    //==============================
    // Knee-Point (KP) parameters
    //==============================
    // KP ramp base thresholds (two bounds KP # 1 and KP # 2 are computed)
    float SY1_V1;
    float SY1_V2;
    float SY1_T1;
    float SY1_T2;

    float SY2_V1;
    float SY2_V2;
    float SY2_T1;
    float SY2_T2;

    // KP mixing gain (final KP from bounds KP # 1 and KP # 2 as a function of scene percentile)
    float KP_G_V1;
    float KP_G_V2;
    float KP_G_T1;
    float KP_G_T2;

    //==============================
    // P coefficient parameters
    //==============================
    // Thresholds of minimum bound of P1 coefficient
    float P1_LIMIT_V1;
    float P1_LIMIT_V2;
    float P1_LIMIT_T1;
    float P1_LIMIT_T2;

    // Thresholds to compute relative shape of curve (P2~P9 coefficient) by pre-defined bounds - as a function of scene percentile
    float P2To9_T1;
    float P2To9_T2;

    // Defined relative shape bounds (P2~P9 coefficient) for a given maximum TM dynamic compression (eg : 20x )
    float P2ToP9_MAX1[ORDER - 2];
    float P2ToP9_MAX2[ORDER - 2];

    // Ps mixing gain (obtain all Ps coefficients) - as a function of TM dynamic compression ratio
    float PS_G_T1;
    float PS_G_T2;

    // Post-processing : Reduce P1/P2 (to enhance mid tone) for high TM dynamic range compression cases
    float LOW_SY_T1;
    float LOW_SY_T2;
    float LOW_K_T1;
    float LOW_K_T2;
    float RED_P1_V1;
    float RED_P1_T1;
    float RED_P1_T2;
    float RED_P2_V1;
    float RED_P2_T1;
    float RED_P2_T2;

private:

    void setDefault()
    {
        // KP ramp base thresholds (two bounds KP # 1 and KP # 2 are computed)
        SY1_V1 = 0.0;
        SY1_V2 = 0.3;
        SY1_T1 = 0.22;
        SY1_T2 = 1;
        SY2_V1 = 0;
        SY2_V2 = 0.2;
        SY2_T1 = 0.25;
        SY2_T2 = 0.95;

        // KP mixing gain (final KP from bounds KP # 1 and KP # 2 as a function of scene percentile)
        KP_G_V1 = 1;
        KP_G_V2 = 0.05;
        KP_G_T1 = 0.05;
        KP_G_T2 = 0.5;

        // Thresholds of minimum bound of P1 coefficient
        P1_LIMIT_V1 = 0.92;
        P1_LIMIT_V2 = 0.98;
        P1_LIMIT_T1 = 0.01;
        P1_LIMIT_T2 = 0.1;

        // Thresholds to compute relative shape of curve (P2~P9 coefficient) by pre-defined bounds - as a function of scene percentile
        P2To9_T1 = 0.05;
        P2To9_T2 = 0.55;
        float P2ToP9_MAX1_init[ORDER - 2] = { 0.5582, 0.6745, 0.7703, 0.8231, 0.8729, 0.9130, 0.9599, 0.9844 };
        float P2ToP9_MAX2_init[ORDER - 2] = { 0.4839, 0.6325, 0.7253, 0.7722, 0.8201, 0.8837, 0.9208, 0.9580 };
        for (int i = 0; i < (ORDER - 2); i++) {
            P2ToP9_MAX1[i] = P2ToP9_MAX1_init[i];
            P2ToP9_MAX2[i] = P2ToP9_MAX2_init[i];
        }

        // Ps mixing gain (obtain all Ps coefficients) - as a function of TM dynamic compression ratio
        PS_G_T1 = 0.125;
        PS_G_T2 = 0.95;

        // Post-processing : Reduce P1/P2 (to enhance mid tone) for high TM dynamic range compression cases
        LOW_SY_T1 = 0.005;
        LOW_SY_T2 = 0.04;
        LOW_K_T1 = 0.12;
        LOW_K_T2 = 0.4;
        RED_P1_V1 = 0.65;
        RED_P1_T1 = 0.1;
        RED_P1_T2 = 0.75;
        RED_P2_V1 = 0.8;
        RED_P2_T1 = 0.1;
        RED_P2_T2 = 0.75;
    }
};

void getPercentile_50_99(LuminanceParameters &luminanceParam, float & psll50, float & psll99)
{
    const int npsll = luminanceParam.percentileOrder;

    // Set output to -1 (invalid)
    psll50 = -1;
    psll99 = -1;

    // Find exact percentiles if provided , else interpolate
    float psll50_1 = -1, per50_1 = -1;
    float psll50_2 = -1, per50_2 = -1;
    float psll99_1 = -1, per99_1 = -1;
    float psll99_2 = -1, per99_2 = -1;

    // Search for exact percent index or bounds
    int curPercent = 0, prevPercent = 0;
    float curPsll = 0, prevPsll = 0;
    for (int i = 0; i< npsll; i++) {
        curPercent = luminanceParam.getPercentIndexAt(i);
        curPsll    = luminanceParam.getPercentileLuminanceAt(i);

        if (curPercent == 50) {
            psll50 = curPsll;
        } else if (psll50 == -1 && curPercent > 50 && prevPercent < 50) {
            per50_1 = prevPercent;
            per50_2 = curPercent;
            psll50_1 = prevPsll;
            psll50_2 = curPsll;
        }

        if (curPercent == 99) {
            psll99 = curPsll;
        } else if (psll99 == -1 && curPercent > 99 && prevPercent < 99) {
            per99_1 = prevPercent;
            per99_2 = curPercent;
            psll99_1 = prevPsll;
            psll99_2 = curPsll;
        }
        prevPercent = curPercent;
        prevPsll = curPsll;
    }

    // Interpolated 50% percentile luminance
    if (psll50 == -1) {
        const float delta = std::max((per50_2 - per50_1), 1.0f);
        psll50 = psll50_1 + (psll50_2 - psll50_1) * (50 - per50_1) / delta;
    }

    // Interpolated 99% percentile luminance
    if (psll99 == -1) {
        const float delta = std::max((per99_2 - per99_1), 1.0f);
        psll99 = psll99_1 + (psll99_2 - psll99_1) * (99.95f - per99_1) / delta;
    }
}

float rampWeight(const float v1, const float v2, const float t1, const float t2, const float t)
{
    float retVal = v1;

    if (t1 == t2)
        retVal = (t < t1) ? (v1) : (v2);
    else if (t <= t1)
        retVal = v1;
    else if (t >= t2)
        retVal = v2;
    else
        retVal = v1 + (v2 - v1) / (t2 - t1) * (t - t1);

    return retVal;
}

float calcP1(const float sx, const float sy, const float tgtL, const float calcMaxL,
             const float p1_limit_t1, const float p1_limit_t2, const float p1_limit_v1, const float p1_limit_v2,
             const float low_sy_t1, const float low_sy_t2, const float low_k_t1, const float low_k_t2,
             const float red_p1_t1, const float red_p1_t2, const float red_p1_v1, float *p1_red_gain)
{
    const float ax = std::min(sx, 0.9999f);
    const float ay = std::min(sy, 0.9999f);

    const float k = tgtL / std::max(tgtL, calcMaxL);
    const float p1_limit = rampWeight(p1_limit_v2, p1_limit_v1, p1_limit_t1, p1_limit_t2, sy);
    float p1 = std::max(std::min(((1.0f - ax) / (ORDER * (1.0f - ay))) / k, p1_limit), 0.0f);

    // Reduce p1 to make more mid tone details for high compression TM case.
    const float low_sy_g = rampWeight(1.0, 0.0, low_sy_t1, low_sy_t2, sy);
    const float high_k_g = rampWeight(1.0, 0.0, low_k_t1, low_k_t2, k);
    const float high_tm_g = low_sy_g * high_k_g;
    const float red_p1 = rampWeight(1.0, red_p1_v1, red_p1_t1, red_p1_t2, high_tm_g);
    p1 = std::min(std::max(p1 * red_p1, P1MIN), p1_limit);

    if (p1_red_gain != nullptr)
        *p1_red_gain = high_tm_g;

    return p1;
}

void BasisOOTF(LuminanceParameters &luminanceParam, const float targetMaxLuminance, const float sourceMaxL, CurveParameters &curveParam)
{
    const float tgtL = targetMaxLuminance;
    const float srcL = std::max(tgtL, sourceMaxL);

    // KP - Knee Point
    float psll50, psll99;
    getPercentile_50_99(luminanceParam, psll50, psll99);

    BasisOOTF_Params params;
    const float ctrL = psll50 / std::max(psll99, 0.0001f);
    const float k = tgtL / srcL;
    const float sy1 = rampWeight(params.SY1_V1, params.SY1_V2, params.SY1_T1, params.SY1_T2, k);
    const float sy2 = rampWeight(params.SY2_V1, params.SY2_V2, params.SY2_T1, params.SY2_T2, k);
    const float kp_g = rampWeight(params.KP_G_V1, params.KP_G_V2, params.KP_G_T1, params.KP_G_T2, ctrL);
    const double sy = kp_g * sy1 + (1 - kp_g) * sy2;
    const double sx = sy * k;

    // P Coefficient
    float high_tm_g = 0;
    const float p1 = calcP1(sx, sy, tgtL, sourceMaxL,
                            params.P1_LIMIT_T1, params.P1_LIMIT_T2, params.P1_LIMIT_V1, params.P1_LIMIT_V2,
                            params.LOW_SY_T1, params.LOW_SY_T2, params.LOW_K_T1, params.LOW_K_T2,
                            params.RED_P1_T1, params.RED_P1_T2, params.RED_P1_V1, &high_tm_g);

    float ps2To9[META_MAX_PCOEFF_SIZE];
    for (int i = 0; i < (NPCOEFF - 1); i++) {
        const float g_p29 = rampWeight(params.P2ToP9_MAX2[i], params.P2ToP9_MAX1[i], params.P2To9_T1, params.P2To9_T2, ctrL);
        ps2To9[i] = g_p29 * params.P2ToP9_MAX1[i] + (1.0f - g_p29) * params.P2ToP9_MAX2[i];
    }

    const float ps_g = rampWeight(1, 0, params.PS_G_T1, params.PS_G_T2, k);

    double pcoeff[META_MAX_PCOEFF_SIZE];
    pcoeff[0] = p1;
    for (int i = 1; i < NPCOEFF; i++) {
        const double coeffi = i + 1;
        const double plin = coeffi / ORDER;
        pcoeff[i] = std::max(std::min(ps_g * ps2To9[i - 1] + (1.0 - ps_g) * plin, coeffi * p1), plin);
    }
    const float red_p2 = rampWeight(1.0, params.RED_P2_V1, params.RED_P2_T1, params.RED_P2_T2, high_tm_g);
    pcoeff[1] = std::max(std::min(pcoeff[1] * red_p2, 2.0 * p1), 2.0 / ORDER);

    curveParam.setValues(sx, sy, pcoeff, ORDER);
}

#define BLEND_WEIGHT(a, b, g) ((g) * (a) + (1.0f - (g)) * (b))
#define KP_AT_BYPASS (0.3f)

void GuidedOOTF(LuminanceParameters &luminanceParam, CurveParameters &refParam, const float sourceMaxL, const unsigned int referenceLuminance, const unsigned int targetMaxLuminance, CurveParameters &curveParam)
{
    const unsigned int order = refParam.order;
    const unsigned int numP = std::max(order - 1, 1U);

    const unsigned int T0 = referenceLuminance;
    const unsigned int T1 = targetMaxLuminance;

    const float srcL0 = std::max(sourceMaxL, static_cast<float>(T0));
    const float srcL = std::max(sourceMaxL, static_cast<float>(T1));
    const float kmin = 200.0f / srcL;
    const float k0 = T0 / srcL0;
    const float k1 = T1 / srcL;
    const float kmax = 1;

    double psLinear[META_MAX_PCOEFF_SIZE]; // Maximum order is under 15
    for (unsigned int i = 0; i < numP; i++)
        psLinear[i] = (i + 1) / static_cast<double>(order);

    if (T1 < 200.0f) {
        //ALOGE("Error GuidedOOTF : target peak under 200 nits not supported at this time");
        curveParam.setValues(0, 0, psLinear, order);
    } else if (T0 == T1) {
        // Reference Peak == Product Peak
        curveParam = refParam;
    } else if (T1 >= sourceMaxL) {
        // Product Peak >= Scene Max (bypass curve)
        curveParam.setValues(KP_AT_BYPASS, KP_AT_BYPASS, psLinear, order);
    } else {
        double pscalc[META_MAX_PCOEFF_SIZE];
        double sx, sy;

        // Product Peak < Reference Peak -> blend curve with highest TM range compression curve
        if (T1 < T0) {
            const float g = std::max(std::min((k1 - kmin) / (k0 - kmin), 1.0f), 0.0f);
            CurveParameters minCurveParams;
            BasisOOTF(luminanceParam, 200.0f, sourceMaxL, minCurveParams);

            sy = std::max(std::min(BLEND_WEIGHT(refParam.KPy, minCurveParams.KPy, g), 1.0), 0.0);
            sx = sy * k1;

            for (unsigned int i = 0; i < numP; i++)
                pscalc[i] = BLEND_WEIGHT(refParam.getPCoeffAt(i), minCurveParams.getPCoeffAt(i), g);
        }
        // Product Peak > Reference Peak -> blend curves with linear curve
        else {
            const float g = std::max(std::min((k1 - k0) / (kmax - k0), 1.0f), 0.0f);
            sy = std::max(std::min(BLEND_WEIGHT(KP_AT_BYPASS, refParam.KPy, g), 1.0), 0.0);
            sx = sy * k1;
            for (unsigned int i = 0; i < numP; i++)
                pscalc[i] = BLEND_WEIGHT(psLinear[i], refParam.getPCoeffAt(i), g);
        }

        const BasisOOTF_Params params;
        const float ps1 = calcP1(sx, sy, T1, sourceMaxL,
            params.P1_LIMIT_T1, params.P1_LIMIT_T2, params.P1_LIMIT_V1, params.P1_LIMIT_V2,
            params.LOW_SY_T1, params.LOW_SY_T2, params.LOW_K_T1, params.LOW_K_T2,
            params.RED_P1_T1, params.RED_P1_T2, params.RED_P1_V1,
            nullptr);

        pscalc[0] = ps1;  // Recalculate p1 to form smooth connection with knee-point slope

        for (unsigned int i = 1; i < numP; i++)
            // Limit remaining P coefficients to form curve below CI line slope
            pscalc[i] = std::min(pscalc[i], (i + 1) * pscalc[0]);

        curveParam.setValues(sx, sy, pscalc, order);
    }
}

void meta2meta(unsigned int targetMaxLuminance, unsigned int __attribute__((unused)) sourceMaxLuminance, ExynosHdrDynamicInfo_t &meta)
{
    LuminanceParameters luminanceParam;
    CurveParameters curveParam;

    luminanceParam.setValues(meta.data.maxscl, meta.data.maxrgb_percentiles,
                             meta.data.maxrgb_percentages, meta.data.num_maxrgb_percentiles,
                             META_JSON_2094_100K_PERCENTILE_DIVIDE);
    float maxLuminance = getSourceMaxLuminance(luminanceParam);

    if (meta.data.tone_mapping.tone_mapping_flag == 1) {
        CurveParameters refParam;

        refParam.setValues(meta.data.tone_mapping.knee_point_x,
                           meta.data.tone_mapping.knee_point_y,
                           meta.data.tone_mapping.bezier_curve_anchors,
                           meta.data.tone_mapping.num_bezier_curve_anchors + 1,
                           META_JSON_2094_EBZ_KNEE_POINT_MAX,
                           META_JSON_2094_EBZ_PCOEFF_MAX);

        GuidedOOTF(luminanceParam, refParam, maxLuminance,
                   meta.data.display_maximum_luminance, targetMaxLuminance, curveParam);
    } else {
        BasisOOTF(luminanceParam, targetMaxLuminance, maxLuminance, curveParam);
        meta.data.tone_mapping.tone_mapping_flag = 1;
    }

    curveParam.getValues(meta, META_JSON_2094_EBZ_KNEE_POINT_MAX, META_JSON_2094_EBZ_PCOEFF_MAX);
}
