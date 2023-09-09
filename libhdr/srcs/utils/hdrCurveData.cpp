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
#include <vector>
#include <set>
#include <queue>
#include <cmath>

#include <system/graphics.h>
#include <log/log.h>

#ifndef HDR_TEST
#include <VendorVideoAPI.h>
#else
#include <VendorVideoAPI_hdrTest.h>
#endif

#include "hdrCurveData.h"

using namespace std;

template<typename T>
class curveData {
public:
    curveData(T &info, int inputrange, int outputrange, int minx)
        : info(info), inputRange(inputrange), outputRange(outputrange), minX(minx) {}
    virtual ~curveData() {}

    virtual int getY(int __attribute__((unused)) px) { return 0; }

    T info;
    int inputRange;
    int outputRange;
    int minX;
};

//#define NUM_DYNAMIC_TM_X_BITS 27
//#define NUM_DYNAMIC_TM_Y_BITS 20
//#define NUM_DYNAMIC_X_TM (1 << NUM_DYNAMIC_TM_X_BITS)
//#define NUM_DYNAMIC_Y_TM (1 << NUM_DYNAMIC_TM_Y_BITS)

class OETFCurveData : public curveData<ExynosHdrDynamicInfo_t> {
public:
    OETFCurveData(ExynosHdrDynamicInfo_t &meta, int inputrange, int outputrange, int minx)
        : curveData(meta, inputrange, outputrange, minx) {}
    virtual ~OETFCurveData() {}

    int lookupTonemapGain(int px)
    {
        static float powX[META_MAX_PCOEFF_SIZE];
        static float powDX[META_MAX_PCOEFF_SIZE];
        static const float EBZ_COEFF[META_MAX_PCOEFF_SIZE + 2][META_MAX_PCOEFF_SIZE] =
        {
            /*order 0*/{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
            /*order 1*/{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
            /*order 2*/{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
            /*order 3*/{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
            /*order 4*/{ 4, 6, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
            /*order 5*/{ 5, 10, 10, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
            /*order 6*/{ 6, 15, 20, 15, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
            /*order 7*/{ 7, 21, 35, 35, 21, 7, 0, 0, 0, 0, 0, 0, 0, 0 },
            /*order 8*/{ 8, 28, 56, 70, 56, 28, 8, 0, 0, 0, 0, 0, 0, 0 },
            /*order 9*/{ 9, 36, 84, 126, 126, 84, 36, 9, 0, 0, 0, 0, 0, 0 },
            /*order 10*/{ 10, 45, 120, 210, 252, 210, 120, 45, 10, 0, 0, 0, 0, 0 },
            /*order 11*/{ 11, 55, 165, 330, 462, 462, 330, 165, 55, 11, 0, 0, 0, 0 },
            /*order 12*/{ 12, 66, 220, 495, 792, 924, 792, 495, 220, 66, 12, 0, 0, 0 },
            /*order 13*/{ 13, 78, 286, 715, 1287, 1716, 1716, 1287, 715, 286, 78, 13, 0, 0 },
            /*order 14*/{ 14, 91, 364, 1001, 2002, 3003, 3432, 3003, 2002, 1001, 364, 91, 14, 0 },
            /*order 15*/{ 15, 105, 455, 1365, 3003, 5005, 6435, 6435, 5005, 3003, 1365, 455, 105, 15 }
        };

        CurveParameters curveParam;
        curveParam.setValues(info.data.tone_mapping.knee_point_x,
                             info.data.tone_mapping.knee_point_y,
                             info.data.tone_mapping.bezier_curve_anchors,
                             info.data.tone_mapping.num_bezier_curve_anchors + 1,
                             META_JSON_2094_EBZ_KNEE_POINT_MAX,
                             META_JSON_2094_EBZ_PCOEFF_MAX);

        const float sx = curveParam.KPx;
        const float sy = curveParam.KPy;
        const int order = curveParam.order;
        const int numPCoeff = order - 1;
        float out;

        if ((double)px / inputRange < sx) {
            float k = (sx > 0) ? (sy / sx) : (0.0f);
            out = std::max(std::min((double)px  * k / inputRange, 1.0), 0.0);
        }
        else {
            // Pre compute constants
            const float x = ((double)px / inputRange - sx) / (1.0f - sx);
            const float dx = 1.0f - x;

            // Compute power of x and dx by mul instead of pow() function : faster
            powX[0] = x;
            powDX[0] = dx;
            for (int i = 1; i < numPCoeff; i++) {
                powX[i] = powX[i - 1] * x;
                powDX[i] = powDX[i - 1] * dx;
            }

            /******** Compute curve output ********
            //  Original Equation :
            //  y = sum for i=0:order ( NChooseK(order, i) * pow(x,i) * pow((1-x),order-i) * pi
            //
            //  Where,
            //  Fixed : p0 = 0.0f and pn = 1.0f
            //  Input : p1....pn : specified in range 0~1
            //  x : Input in range 0~1
            //
            //  This is simplified as below (ignore first term as it is always zero, last term is always pow(x, order)
            //  NChooseK() coefficients for each order is saved as LUT : EBZ_COEFF
            //   dx = (1.0f-x) is precomputed
            //   all powers of x and (1-x) are precomuted as DP table powX and powDX
            *****************************************/

            float ebzy = 0.0f;
            for (int i = 0; i < numPCoeff; i++)
                ebzy += EBZ_COEFF[order][i] * powX[i] * powDX[numPCoeff - i - 1] * curveParam.pcoeff[i];

            ebzy += powX[numPCoeff - 1] * x;

            out = sy + (1.0f - sy) * ebzy;
        }

        if (px == 0)
            px = 1;
        return (int)(out * inputRange * outputRange / px );
    }

    int getY(int px)
    {
        return lookupTonemapGain(max(px, minX));
    }
};

// copy Android ToneMapper from frameworks/native/libs/tonemap/tonemap.cpp
class ATMCurveData : public curveData<CurveInfo> {
public:
    ATMCurveData(CurveInfo &info, int inputrange, int outputrange, int minx)
        : curveData(info, inputrange, outputrange, minx) {
            int transfer = info.inDataspace & HAL_DATASPACE_TRANSFER_MASK;
            if (transfer != HAL_DATASPACE_TRANSFER_HLG && info.metaIf != nullptr)
                info.metaIf->configHDR10Tonemap(info.maxInLumi, info.maxOutLumi);
        }
    virtual ~ATMCurveData() {}

    double lookupTonemapGainO(double nit)
    {
        //# three control points
        double x0 = 10.0;
        double y0 = 17.0;
        double x1 = info.maxOutLumi * 0.75;
        double y1 = x1;
        double x2 = x1 + (info.maxInLumi - x1) / 2.0;
        double y2 = y1 + (info.maxOutLumi - y1) * 0.75;

        //#horizontal distances between the last three control points
        double h12 = x2 - x1;
        double h23 = info.maxInLumi - x2;

        //# tangents at the last three control points
        double m1 = (y2 - y1) / h12;
        double m3 = (info.maxOutLumi - y2) / h23;
        double m2 = (m1 + m3) / 2.0;

        if ((info.inDataspace & HAL_DATASPACE_TRANSFER_MASK) == HAL_DATASPACE_TRANSFER_HLG) {
            nit *= std::pow(nit, 0.2);
        }

        if (info.maxInLumi <= info.maxOutLumi) {
            return nit;
        }

        if (nit < x0) {    //# scale [0.0, x0] to [0.0, y0] linearly
            double slope = y0 / x0;
            return nit * slope;
        } else if (nit < x1) { //# scale [x0, x1] to [y0, y1] linearly
            double slope = (y1 - y0) / (x1 - x0);
            return y0 + (nit - x0) * slope;
        } else if (nit < x2) {   //# scale [x1, x2] to [y1, y2] using Hermite interp
            double t = (nit - x1) / h12;
            return (y1 * (1.0 + 2.0 * t) + h12 * m1 * t) * (1.0 - t) * (1.0 - t)
                    + (y2 * (3.0 - 2.0 * t) + h12 * m2 * (t - 1.0)) * t * t;
        } else {       //# scale [x2, maxInLumi] to [y2, maxOutLumi] using Hermite interp
            double t = (nit - x2) / h23;
            return (y2 * (1.0 + 2.0 * t) + h23 * m2 * t) * (1.0 - t) * (1.0 - t)
                    + (info.maxOutLumi * (3.0 - 2.0 * t) + h23 * m3 * (t - 1.0)) * t * t;
        }
    }

    double OETF_ST2084(double nits) {
        nits = nits / 10000.0;
        double m1 = (2610.0 / 4096.0) / 4.0;
        double m2 = (2523.0 / 4096.0) * 128.0;
        double c1 = (3424.0 / 4096.0);
        double c2 = (2413.0 / 4096.0) * 32.0;
        double c3 = (2392.0 / 4096.0) * 32.0;

        double tmp = std::pow(nits, m1);
        tmp = (c1 + c2 * tmp) / (1.0 + c3 * tmp);
        return std::pow(tmp, m2);
    }

    double lookupTonemapGain13_PQ(double nit)
    {
        const double x1 = info.maxOutLumi * 0.65;
        const double y1 = x1;

        const double x3 = info.maxInLumi;
        const double y3 = info.maxOutLumi;

        const double x2 = x1 + (x3 - x1) * 4.0 / 17.0;
        const double y2 = info.maxOutLumi * 0.9;

        const double greyNorm1 = OETF_ST2084(x1);
        const double greyNorm2 = OETF_ST2084(x2);
        const double greyNorm3 = OETF_ST2084(x3);

        const double slope2 = (y2 - y1) / (greyNorm2 - greyNorm1);
        const double slope3 = (y3 - y2) / (greyNorm3 - greyNorm2);

        if (nit < x1) {
            return nit;
        }

        if (nit > info.maxInLumi) {
            return info.maxOutLumi;
        }

        const double greyNits = OETF_ST2084(nit);

        if (greyNits <= greyNorm2) {
            return (greyNits - greyNorm2) * slope2 + y2;
        } else if (greyNits <= greyNorm3) {
            return (greyNits - greyNorm3) * slope3 + y3;
        } else {
            return info.maxOutLumi;
        }
    }

    float computeHlgGamma(float currentDisplayBrightnessNits) {
        /* BT 2100-2's recommendation for taking into account the nominal max
         * brightness of the display does not work when the current brightness is
         * very low. For instance, the gamma becomes negative when the current
         * brightness is between 1 and 2 nits, which would be a bad experience in a
         * dark environment. Furthermore, BT2100-2 recommends applying
         * channel^(gamma - 1) as its OOTF, which means that when the current
         * brightness is lower than 335 nits then channel * channel^(gamma - 1) >
         * channel, which makes dark scenes very bright. As a workaround for those
         * problems, lower-bound the brightness to 500 nits.
         *
         * remove 500 nit lower-bound to meet BT2100 specification
         *constexpr float minBrightnessNits = 500.f;
         *currentDisplayBrightnessNits = std::max(minBrightnessNits, currentDisplayBrightnessNits);
        */
        return 1.2 + 0.42 * std::log10(currentDisplayBrightnessNits / 1000);
    }

    double lookupTonemapGain13_HLG(double nit)
    {
        const double hlgGamma = computeHlgGamma(info.maxOutLumi);
        return nit
                * std::pow(nit / 1000.0, hlgGamma - 1)
                * info.maxOutLumi / 1000.0;
    }

    int getY(int px)
    {
        int transfer = info.inDataspace & HAL_DATASPACE_TRANSFER_MASK;
        double px_norm = (double)max(px, minX) / inputRange; // scale x to [0, 1]
        double py_out, px_in = px_norm * info.maxInLumi; // scale x to [0, maxInLumi]
        // x: [0, maxInLumi] / y: [0, maxOutLumi]
        if (transfer == HAL_DATASPACE_TRANSFER_HLG)
            py_out = lookupTonemapGain13_HLG(px_in);
        else {
            int ret = HDRMETAIF_ERR_MAX;
            if (info.metaIf != nullptr)
                ret = info.metaIf->computeHDR10Tonemap(px_in, &py_out);
            if (ret != HDRMETAIF_ERR_NO)
                py_out = lookupTonemapGain13_PQ(px_in);
        }
        double py_norm = py_out / info.maxOutLumi; // scale y to [0, 1]
        return (int)(py_norm * outputRange / px_norm); // scale y/x to [0, outputRange]
    }
};

class EOTFCurveData : public curveData<CurveInfo> {
public:
    EOTFCurveData(CurveInfo &info, int inputrange, int outputrange, int minx)
        : curveData(info, inputrange, outputrange, minx) {}
    virtual ~EOTFCurveData() {}

    double get_SMPTE_PQ (double N)
    {
        double m1 = 0.1593017578125;
        double m2 = 78.84375;
        double c2 = 18.8515625;
        double c3 = 18.6875;
        double c1 = c3 - c2 + 1.0;

        return std::pow((fmax((std::pow(N, (1/m2)))-c1, 0)
                    / (c2 - c3 * (std::pow(N, (1/m2)))))
                ,(1/m1));
    }

    double computePQ(double px_norm)
    {
        return std::min(get_SMPTE_PQ(px_norm) / (info.maxInLumi/10000.0), 1.0);
    }

    double computeHLG(double px_norm)
    {
        double a = 0.17883277;
        double b = 0.28466892;
        double c = 0.55991073;

        if (px_norm <= 0.5)
            return px_norm * px_norm / 3.0;
        else
            return (std::exp((px_norm - c) / a) + b) / 12.0;
    }

    int getY(int px)
    {
        int transfer = info.inDataspace & HAL_DATASPACE_TRANSFER_MASK;
        double py_norm, px_norm = (double)px / inputRange; // scale x [0,inputRange] to [0, 1]
        // x: [0, 1] / y: [0, 1]
        if (transfer == HAL_DATASPACE_TRANSFER_HLG)
            py_norm = computeHLG(px_norm);
        else
            py_norm = computePQ(px_norm);
        return (int)(py_norm * outputRange); // scale y to [0,outputRange]
    }
};

struct _Node {
    _Node(int lx, int ly, int rx, int ry, int cy, int py, int ny)
        : leftX(lx), leftY(ly), rightX(rx), rightY(ry), curY(cy), prevY(py), nextY(ny)
    {
        curX = (leftX + rightX) >> 1;
        cost = 0;
    }
    virtual ~_Node() {}

    int leftX;
    int leftY;
    int rightX;
    int rightY;
    int curX;
    int curY;
    int prevY;
    int nextY;
    int64_t cost;

    virtual int64_t getCost() = 0;
};

struct Node : _Node {
    Node(int lx, int ly, int rx, int ry, int cy, int py, int ny)
        : _Node(lx, ly, rx, ry, cy, py, ny)
    {
        cost = getCost();
    }
    virtual ~Node() {}

    virtual int64_t getCost() {
        return (int64_t)abs(curY - ((leftY + rightY) >> 1));
    }
};

struct NodeAdv : _Node {
    NodeAdv(int lx, int ly, int rx, int ry, int cy, int py, int ny)
        : _Node(lx, ly, rx, ry, cy, py, ny)
    {
        cost = getCost();
    }
    virtual ~NodeAdv() {}

    virtual int64_t getCost() { // consider "x distance" to spread out
        return (int64_t)abs(curY - ((leftY + rightY) >> 1)) * (curX - leftX);
    }
};

struct NodeMultiPoints : _Node {
    NodeMultiPoints(int lx, int ly, int rx, int ry, int cy, int py, int ny)
        : _Node(lx, ly, rx, ry, cy, py, ny)
    {
        cost = getCost();
    }
    virtual ~NodeMultiPoints() {}

    virtual int64_t getCost() { // sum of 1/4, 2/4, 3/4 points
        return  (int64_t)abs(curY - ((leftY + rightY) >> 1)) +
                (int64_t)abs(prevY - ((leftY * 3 + rightY) >> 2)) +
                (int64_t)abs(nextY - ((leftY + rightY * 3) >> 2));
    }
};

template<typename N>
struct selectComp {
    bool operator()(const N &l, const N &r) const {
        if (l.curX == r.curX)
            ALOGD("duplicated points (%d, %d) vs (%d, %d)", l.curX, l.curY, r.curX, r.curY);
        return l.curX < r.curX;
    }
};

template<typename N>
struct candidateComp {
    bool operator()(const N &l, const N &r) const {
        if (l.cost == r.cost)
            return l.curX < r.curX;
        return l.cost < r.cost;
    }
};

template<typename T, typename N>
class kneePointExtractor {
public:
    kneePointExtractor(curveData<T> &data)
        : curvedata(data)
    {
        // Initial start point of select list.
        int lx = 0;
        int ly = curvedata.getY(lx);

        selectList.insert({lx, ly, lx, ly, ly, ly, ly});

        // Initial end point of select list.
        int rx = data.inputRange;
        int ry = curvedata.getY(rx);

        selectList.insert({rx, ry, rx, ry, ry, ry, ry});

        // Initial start point of searching candidate list
        int cx = (lx + rx) >> 1;
        int cy = curvedata.getY(cx);
        int py = curvedata.getY(cx - ((rx - lx) >> 2));
        int ny = curvedata.getY(cx + ((rx - lx) >> 2));

        candidateList.push({lx, ly, rx, ry, cy, py, ny});
    }

    void select_from_candidate(int numArray)
    {
        for (int i = 0; i < numArray; i++) {
            N node = candidateList.top();

            // Move from candidate to select
            candidateList.pop();
            selectList.insert(node);

            // Leaf node
            if (node.rightX - node.leftX <= (curvedata.minX << 1))
                continue;

            candidateList.push({ node.leftX, node.leftY, node.curX, node.curY,
                    curvedata.getY((node.leftX + node.curX) >> 1),
                    curvedata.getY(((node.leftX + node.curX) >> 1) - ((node.curX - node.leftX) >> 2)),
                    curvedata.getY(((node.leftX + node.curX) >> 1) + ((node.curX - node.leftX) >> 2)),
            });
            candidateList.push({ node.curX, node.curY, node.rightX, node.rightY,
                    curvedata.getY((node.curX + node.rightX) >> 1),
                    curvedata.getY(((node.curX + node.rightX) >> 1) - ((node.rightX - node.curX) >> 2)),
                    curvedata.getY(((node.curX + node.rightX) >> 1) + ((node.rightX - node.curX) >> 2)),
            });
        }
    }

    void select(int numArray, std::vector<int> &arrX, std::vector<int> &arrY)
    {
        // Select node already contains start and end point, so subtract two.
        select_from_candidate(numArray - 2);

        // Write select node
        int i = 0;
        for (auto &node : selectList) {
            arrX[i] = node.curX;
            arrY[i] = node.curY;
            i++;
        }

        // The last coordinate has difference with prior value
        arrX[i - 1] -= arrX[i - 2];
        arrY[i - 1] -= arrY[i - 2];
    }

private:
    set<N, selectComp<N>> selectList;
    priority_queue<N, vector<N>, candidateComp<N>> candidateList;
    curveData<T> &curvedata;
};

void genTMCurve(
        CurveInfo info,
        void *data,
        std::vector<int> &arrX, std::vector<int> &arrY, int numArray,
        int x_bits, int y_bits, int minx_bits)
{
    ExynosHdrDynamicInfo_t meta = *(ExynosHdrDynamicInfo_t *)data;

    meta2meta(info.maxOutLumi, info.maxInLumi, meta);

    int NUM_X = 1 << x_bits;
    int NUM_Y = 1 << y_bits;
    int MIN_X = 1 << minx_bits;
    OETFCurveData curvedata(meta, NUM_X, NUM_Y, MIN_X);
    kneePointExtractor<ExynosHdrDynamicInfo_t, NodeMultiPoints> points(curvedata);

    points.select(numArray, arrX, arrY);
}

void genTMCurve(
        CurveInfo info,
        std::vector<int> &arrX, std::vector<int> &arrY, int numArray,
        int x_bits, int y_bits, int minx_bits)
{
    int NUM_X = 1 << x_bits;
    int NUM_Y = 1 << y_bits;
    int MIN_X = 1 << minx_bits;
    ATMCurveData curvedata(info, NUM_X, NUM_Y, MIN_X);
    kneePointExtractor<CurveInfo, NodeMultiPoints> points(curvedata);

    points.select(numArray, arrX, arrY);
}

void genEOTFCurve(
        CurveInfo info,
        std::vector<int> &arrX, std::vector<int> &arrY, int numArray,
        int x_bits, int y_bits, int minx_bits)
{
    int NUM_X = 1 << x_bits;
    int NUM_Y = (1 << y_bits) - 1;
    int MIN_X = 1 << minx_bits;
    EOTFCurveData curvedata(info, NUM_X, NUM_Y, MIN_X);
    kneePointExtractor<CurveInfo, NodeMultiPoints> points(curvedata);

    points.select(numArray, arrX, arrY);
}

