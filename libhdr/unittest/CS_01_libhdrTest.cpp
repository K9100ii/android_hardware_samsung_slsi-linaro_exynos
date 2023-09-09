/*
 * Copyright 2019 The Android Open Source Project
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

#include <libhdr_parcel_header.h>
#include <hdrHwInfo.h>
#include <gtest/gtest.h>
#include <hardware/exynos/hdrInterface.h>
#include <system/graphics.h>
#include <cutils/properties.h>

#include "wcgTestVector.h"
#include "hdrTestVector.h"
#include "hdr10pTestVector.h"
#include "VendorVideoAPI_hdrTest.h"

using namespace std;

namespace android {
namespace hardware {
namespace exynos {
namespace hdr {

class CS_01_libhdrTest : public ::testing::Test {
    public:
        class hdrInterface *Ihdr;
        int buf_size;
        struct hdrCoefParcel data;
        struct HdrTargetInfo tInfo;
        struct HdrLayerInfo lInfo;

        hdrHwInfo        hw;
        wcgTestVector    wcgTV;
        hdrTestVector    hdrTV;
        hdr10pTestVector    hdr10pTV;

        void SetUp() override {
            hw.init();
            wcgTV.init(&hw);
            hdrTV.init(&hw);
            hdr10pTV.init(&hw);

            Ihdr = hdrInterface::createInstance();
            buf_size = Ihdr->getHdrCoefSize(HDR_HW_DPU);
            data.hdrCoef = new char[buf_size];
            Ihdr->setLogLevel(0);
        }
        void TearDown() override {
            delete (char*)data.hdrCoef;
            delete Ihdr;
        }
        void Verify_Format() {
            void *coef = data.hdrCoef;
            struct hdr_coef_header *header_g = (struct hdr_coef_header *)coef;
            int shall = header_g->num.unpack.shall;
            int need = header_g->num.unpack.need;
            int offset = sizeof(struct hdr_coef_header);
            struct hdr_lut_header *dat = (struct hdr_lut_header*)((char*)header_g + offset);
            for (int i = 0; i < shall; i++) {
                ASSERT_EQ(dat->magic, HDR_LUT_MAGIC) << "magic number does not match, " << "shall : " << shall << " idx : " << i;
                offset = (sizeof(struct hdr_lut_header) + (dat->length * 4));
                dat = (struct hdr_lut_header*)((char*)dat + offset);
            }
            for (int i = 0; i < need; i++) {
                ASSERT_EQ(dat->magic, HDR_LUT_MAGIC) << "magic number does not match, " << "need : " << need << " idx : " << i;
                offset = (sizeof(struct hdr_lut_header) + (dat->length * 4));
                dat = (struct hdr_lut_header*)((char*)dat + offset);
            }
        }
        bool compare_data(int *dat1, int length1, int *dat2, int length2) {
            if (length1 != length2) {
                printf("diff -> length1(%d), length2(%d)\n", length1, length2);
                return false;
            }
            for (int i = 0; i < length1; i++) {
                if (dat1[i] != dat2[i]) {
                    printf("diff -> dat1[%d]:%d, dat2[%d]:%d\n", i, dat1[i], i, dat2[i]);
                    return false;
                }
            }
            return true;
        }
        int getOSVersion(void)
        {
            char PROP_OS_VERSION[PROPERTY_VALUE_MAX] = "ro.build.version.release";
            char str_value[PROPERTY_VALUE_MAX] = {0};
            property_get(PROP_OS_VERSION, str_value, "0");
            int OS_Version = stoi(std::string(str_value));
            ALOGD("OS VERSION : %d", OS_Version);
            return OS_Version;
        }
        void refineTransfer(int &ids) {
            int transfer = ids & HAL_DATASPACE_TRANSFER_MASK;
            int result = ids;
            int OS_Version = getOSVersion();
            switch (transfer) {
                case HAL_DATASPACE_TRANSFER_SRGB:
                case HAL_DATASPACE_TRANSFER_LINEAR:
                case HAL_DATASPACE_TRANSFER_ST2084:
                case HAL_DATASPACE_TRANSFER_HLG:
                    break;
                case HAL_DATASPACE_TRANSFER_SMPTE_170M:
                case HAL_DATASPACE_TRANSFER_GAMMA2_2:
                case HAL_DATASPACE_TRANSFER_GAMMA2_6:
                case HAL_DATASPACE_TRANSFER_GAMMA2_8:
                    if (OS_Version > 12)
                        break;
                    [[fallthrough]];
                default:
                    result = result & ~(HAL_DATASPACE_TRANSFER_MASK);
                    ids = (result | HAL_DATASPACE_TRANSFER_SRGB);
                    break;
            }
        }
        void Verify_Wcg_Data(int hw_id, int layer, int in_dataspace, int out_dataspace) {
            refineTransfer(in_dataspace);
            std::unordered_map<int,struct hdr_dat_node*> wcgTV_map = wcgTV.getTestVector(hw_id, layer, in_dataspace, out_dataspace);

            void *coef = data.hdrCoef;
            struct hdr_coef_header *header_g = (struct hdr_coef_header *)coef;
            int shall = header_g->num.unpack.shall;
            int need = header_g->num.unpack.need;
            int offset = sizeof(struct hdr_coef_header);

            struct hdr_lut_header *dat = (struct hdr_lut_header*)((char*)header_g + offset);
            int byte_offset;
            struct hdr_dat_node* test_vec;
            int * test_data;

            ASSERT_EQ(wcgTV_map.size(), shall + need) << "wcgCoef data size not equal to that of Test Vector (shall:" << shall << ", need:" << need <<")";
            for (int i = 0; i < shall; i++) {
                byte_offset = dat->byte_offset;
                ASSERT_NE(wcgTV_map.find(byte_offset), wcgTV_map.end()) << "data(in wcgCoef) for byte_offset(" << byte_offset  << ") does not exist in wcg test vector (dataspace : " << in_dataspace << "->" << out_dataspace << ")";

                test_vec = wcgTV_map[byte_offset];
                test_data = (int*)((char*)dat + sizeof(struct hdr_lut_header));
                bool result = compare_data((int*)test_vec->data, test_vec->header.length, (int*)test_data, dat->length);
                if (result == false) {
                    ALOGD("hw_id[%d], layer[%d] : %d -> %d", hw_id, layer, in_dataspace, out_dataspace);
                    ALOGD("[test vector]");
                    test_vec->dump(0);
                    ALOGD("[test data]");
                    ((struct hdr_dat_node*)dat)->dump_serialized(0);
                }
                ASSERT_TRUE(result) << "data in wcgCoef and wcg test vector differ (dataspace : " << in_dataspace << " -> " << out_dataspace << ")";

                /* next data */
                offset = (sizeof(struct hdr_lut_header) + (dat->length * 4));
                dat = (struct hdr_lut_header*)((char*)dat + offset);
            }
            for (int i = 0; i < need; i++) {
                byte_offset = dat->byte_offset;

                ASSERT_NE(wcgTV_map.find(byte_offset), wcgTV_map.end()) << "data(in wcgCoef) for byte_offset(" << byte_offset  << ") does not exist in wcg test vector (dataspace : " << in_dataspace << "->" << out_dataspace << ")";

                test_vec = wcgTV_map[byte_offset];
                test_data = (int*)((char*)dat + sizeof(struct hdr_lut_header));
                bool result = compare_data((int*)test_vec->data, test_vec->header.length, (int*)test_data, dat->length);
                if (result == false) {
                    ALOGD("hw_id[%d], layer[%d] : %d -> %d\n", hw_id, layer, in_dataspace, out_dataspace);
                    ALOGD("[test vector]");
                    test_vec->dump(0);
                    ALOGD("[test data]");
                    ((struct hdr_dat_node*)dat)->dump_serialized(0);
                }
                ASSERT_TRUE(result) << "data in wcgCoef and wcg test vector differ (dataspace : " << in_dataspace << " -> " << out_dataspace << ")";

                /* next data */
                offset = (sizeof(struct hdr_lut_header) + (dat->length * 4));
                dat = (struct hdr_lut_header*)((char*)dat + offset);
            }
        }
        void Verify_Hdr_Data(int hw_id, int layer, ExynosHdrStaticInfo *s_meta) {
            std::unordered_map<int,struct hdr_dat_node*> hdrTV_map = hdrTV.getTestVector(hw_id, layer, s_meta);

            void *coef = data.hdrCoef;
            struct hdr_coef_header *header_g = (struct hdr_coef_header *)coef;
            int shall = header_g->num.unpack.shall;
            int need = header_g->num.unpack.need;
            int offset = sizeof(struct hdr_coef_header);

            struct hdr_lut_header *dat = (struct hdr_lut_header*)((char*)header_g + offset);
            int byte_offset;
            struct hdr_dat_node* test_vec;
            int * test_data;

            ASSERT_EQ(hdrTV_map.size(), shall + need) << "hdrCoef data size not equal to that of Test Vector (shall:" << shall << ", need:" << need <<")";
            for (int i = 0; i < shall; i++) {
                byte_offset = dat->byte_offset;

                ASSERT_NE(hdrTV_map.find(byte_offset), hdrTV_map.end()) << "data(in hdrCoef) for byte_offset(" << byte_offset  << ") does not exist in hdr test vector";

                test_vec = hdrTV_map[byte_offset];
                test_data = (int*)((char*)dat + sizeof(struct hdr_lut_header));
                bool result = compare_data((int*)test_vec->data, test_vec->header.length, (int*)test_data, dat->length);
                if (result == false) {
                    ALOGD("hw_id[%d], layer[%d] : max luminance(%d)\n", hw_id, layer, (int)(s_meta->sType1.mMaxDisplayLuminance / 10000));
                    ALOGD("[test vector]");
                    test_vec->dump(0);
                    ALOGD("[test data]");
                    ((struct hdr_dat_node*)dat)->dump_serialized(0);
                }
                ASSERT_TRUE(result) << "data in hdrCoef and hdr test vector differ";

                /* next data */
                offset = (sizeof(struct hdr_lut_header) + (dat->length * 4));
                dat = (struct hdr_lut_header*)((char*)dat + offset);
            }
            for (int i = 0; i < need; i++) {
                byte_offset = dat->byte_offset;

                ASSERT_NE(hdrTV_map.find(byte_offset), hdrTV_map.end()) << "data(in hdrCoef) for byte_offset(" << byte_offset  << ") does not exist in hdr test vector";

                test_vec = hdrTV_map[byte_offset];
                test_data = (int*)((char*)dat + sizeof(struct hdr_lut_header));
                bool result = compare_data((int*)test_vec->data, test_vec->header.length, (int*)test_data, dat->length);
                if (result == false) {
                    ALOGD("hw_id[%d], layer[%d] : max luminance(%d)\n", hw_id, layer, (int)(s_meta->sType1.mMaxDisplayLuminance / 10000));
                    ALOGD("[test vector]");
                    test_vec->dump(0);
                    ALOGD("[test data]");
                    ((struct hdr_dat_node*)dat)->dump_serialized(0);
                }
                ASSERT_TRUE(result) << "data in hdrCoef and hdr test vector differ";

                /* next data */
                offset = (sizeof(struct hdr_lut_header) + (dat->length * 4));
                dat = (struct hdr_lut_header*)((char*)dat + offset);
            }
        }
        void setDynamicMeta(ExynosHdrDynamicInfo *d_meta, int index) {
            hdr10pTV.setDynamicMeta(d_meta, index);
        }
        void Verify_Hdr10p_Data(int hw_id, int layer, ExynosHdrStaticInfo *s_meta, ExynosHdrDynamicInfo *d_meta) {
            std::unordered_map<int,struct hdr_dat_node*> hdr10pTV_map = hdr10pTV.getTestVector(hw_id, layer, s_meta, d_meta); 
            void *coef = data.hdrCoef;
            struct hdr_coef_header *header_g = (struct hdr_coef_header *)coef;
            int shall = header_g->num.unpack.shall;
            int need = header_g->num.unpack.need;
            int offset = sizeof(struct hdr_coef_header);

            struct hdr_lut_header *dat = (struct hdr_lut_header*)((char*)header_g + offset);
            int byte_offset;
            struct hdr_dat_node* test_vec;
            int * test_data;
            for (int i = 0; i < shall; i++) {
                byte_offset = dat->byte_offset;

                if(hdr10pTV_map.find(byte_offset) == hdr10pTV_map.end())
                    continue;

                test_vec = hdr10pTV_map[byte_offset];
                test_data = (int*)((char*)dat + sizeof(struct hdr_lut_header));
                bool result = compare_data((int*)test_vec->data, test_vec->header.length, (int*)test_data, dat->length);
                if (result == false) {
                    ALOGD("hw_id[%d], layer[%d] : max luminance(%d)\n", hw_id, layer, (int)(s_meta->sType1.mMaxDisplayLuminance / 10000));
                    ALOGD("[test vector]");
                    test_vec->dump(0);
                    ALOGD("[test data]");
                    ((struct hdr_dat_node*)dat)->dump_serialized(0);
                }
                ASSERT_TRUE(result) << "data in hdrCoef and hdr test vector differ";

                /* next data */
                offset = (sizeof(struct hdr_lut_header) + (dat->length * 4));
                dat = (struct hdr_lut_header*)((char*)dat + offset);
            }
            for (int i = 0; i < need; i++) {
                byte_offset = dat->byte_offset;

                if(hdr10pTV_map.find(byte_offset) == hdr10pTV_map.end())
                    continue;

                test_vec = hdr10pTV_map[byte_offset];
                test_data = (int*)((char*)dat + sizeof(struct hdr_lut_header));
                bool result = compare_data((int*)test_vec->data, test_vec->header.length, (int*)test_data, dat->length);
                if (result == false) {
                    ALOGD("hw_id[%d], layer[%d] : max luminance(%d)\n", hw_id, layer, (int)(s_meta->sType1.mMaxDisplayLuminance / 10000));
                    ALOGD("[test vector]");
                    test_vec->dump(0);
                    ALOGD("[test data]");
                    ((struct hdr_dat_node*)dat)->dump_serialized(0);
                }
                ASSERT_TRUE(result) << "data in hdrCoef and hdr test vector differ";

                /* next data */
                offset = (sizeof(struct hdr_lut_header) + (dat->length * 4));
                dat = (struct hdr_lut_header*)((char*)dat + offset);
            }
        }
};

TEST_F (CS_01_libhdrTest, CS_01_01_VerifyFormatWCG) {
    tInfo = {HAL_DATASPACE_V0_SRGB, 0, 1000, HDR_BPC_10, HDR_CAPA_INNER};
    Ihdr->setTargetInfo(&tInfo);

    Ihdr->initHdrCoefBuildup(HDR_HW_DPU);
    Ihdr->setHDRlayer(false);

    lInfo = {HAL_DATASPACE_BT2020,// dataspace
           NULL, 0,             // static
           NULL, 0,             // dynamic
           true,                // pre mult
           HDR_BPC_10,          // bpc
           REND_ORI,            // source
           NULL,                // tf_matrix
           false};              // bypass
    Ihdr->setLayerInfo(0,       //layer
            &lInfo);
    Ihdr->getHdrCoefData(HDR_HW_DPU, 0, &data);
    Verify_Format();
}

TEST_F (CS_01_libhdrTest, CS_01_02_VerifyDataWCG) {
    const int max_std = HAL_DATASPACE_STANDARD_ADOBE_RGB >> HAL_DATASPACE_STANDARD_SHIFT;
    const int max_tf = HAL_DATASPACE_TRANSFER_ST2084 >> HAL_DATASPACE_TRANSFER_SHIFT;
    //int i, j;
    int k, l;
    int out_ds, in_ds;

    tInfo = {HAL_DATASPACE_V0_SRGB, 0, 1000, HDR_BPC_10, HDR_CAPA_INNER};
    Ihdr->setTargetInfo(&tInfo);
    out_ds = HAL_DATASPACE_V0_SRGB;
    for (k = 0; k < max_std; k++) {
        for (l = 0; l < max_tf; l++) {
            Ihdr->initHdrCoefBuildup(HDR_HW_DPU);
            Ihdr->setHDRlayer(false);

            in_ds = ((k << HAL_DATASPACE_STANDARD_SHIFT)
                        | (l << HAL_DATASPACE_TRANSFER_SHIFT)
                        | (HAL_DATASPACE_RANGE_FULL));
            lInfo = {in_ds,// dataspace
                   NULL, 0,             // static
                   NULL, 0,             // dynamic
                   true,                // pre mult
                   HDR_BPC_10,          // bpc
                   REND_ORI,            // source
                   NULL,                // tf_matrix
                   false};              // bypass
            Ihdr->setLayerInfo(0,       //layer
                    &lInfo);
            Ihdr->getHdrCoefData(HDR_HW_DPU, 0, &data);
            Verify_Wcg_Data(HDR_HW_DPU ,0, in_ds, out_ds);
        }
    }
}

TEST_F (CS_01_libhdrTest, CS_01_03_VerifyFormatHDR) {
    tInfo = {HAL_DATASPACE_V0_SRGB, 0, 1000, HDR_BPC_10, HDR_CAPA_INNER};
    Ihdr->setTargetInfo(&tInfo);

    ExynosHdrStaticInfo s_meta;
    s_meta.sType1.mMaxDisplayLuminance = (1000 * 10000);

    Ihdr->initHdrCoefBuildup(HDR_HW_DPU);
    Ihdr->setHDRlayer(false);
    lInfo = {HAL_DATASPACE_BT2020_PQ,// dataspace
           &s_meta, sizeof(ExynosHdrStaticInfo),             // static
           NULL, 0,             // dynamic
           true,                // pre mult
           HDR_BPC_10,          // bpc
           REND_ORI,            // source
           NULL,                // tf_matrix
           false};              // bypass
    Ihdr->setLayerInfo(0,       //layer
            &lInfo);
    Ihdr->getHdrCoefData(HDR_HW_DPU, 0, &data);
    Verify_Format();
}

TEST_F (CS_01_libhdrTest, CS_01_04_VerifyDataHDR) {
    tInfo = {HAL_DATASPACE_V0_SRGB, 0, 1000, HDR_BPC_10, HDR_CAPA_INNER};
    Ihdr->setTargetInfo(&tInfo);
    ExynosHdrStaticInfo s_meta;
    vector<int> lum_list = {0,
        (300 * 10000), (500 * 10000), (700 * 10000),
        (1000 * 10000), (1200 * 10000), (1500 * 10000),
        (2000 * 10000), (3000 * 10000), (10000 * 10000)};

    for (int i = 0; i < lum_list.size(); i++) {
        s_meta.sType1.mMaxDisplayLuminance = lum_list[i];

        Ihdr->initHdrCoefBuildup(HDR_HW_DPU);
        Ihdr->setHDRlayer(false);
        lInfo = {HAL_DATASPACE_BT2020_PQ,// dataspace
               &s_meta, sizeof(ExynosHdrStaticInfo),             // static
               NULL, 0,             // dynamic
               true,                // pre mult
               HDR_BPC_10,          // bpc
               REND_ORI,            // source
               NULL,                // tf_matrix
               false};              // bypass
        Ihdr->setLayerInfo(0,       //layer
                &lInfo);
        Ihdr->getHdrCoefData(HDR_HW_DPU, 0, &data);

        Verify_Hdr_Data(HDR_HW_DPU, 0, &s_meta);
    }
}

TEST_F (CS_01_libhdrTest, CS_01_05_VerifyFormatHDR10P) {
    tInfo = {HAL_DATASPACE_V0_SRGB, 0, 1000, HDR_BPC_10, HDR_CAPA_INNER};
    Ihdr->setTargetInfo(&tInfo);

    ExynosHdrStaticInfo s_meta;
    s_meta.sType1.mMaxDisplayLuminance = (1000 * 10000);

    ExynosHdrDynamicInfo d_meta;

    for (int i = 0; i < 4; i++) {
        setDynamicMeta(&d_meta, i);

        Ihdr->initHdrCoefBuildup(HDR_HW_DPU);
        Ihdr->setHDRlayer(false);

        lInfo = {HAL_DATASPACE_BT2020_PQ,// dataspace
               &s_meta, sizeof(ExynosHdrStaticInfo),        // static
               &d_meta, sizeof(ExynosHdrDynamicInfo),       // dynamic
               true,                // pre mult
               HDR_BPC_10,          // bpc
               REND_ORI,            // source
               NULL,                // tf_matrix
               false};              // bypass
        Ihdr->setLayerInfo(0,       //layer
                &lInfo);
        Ihdr->getHdrCoefData(HDR_HW_DPU, 0, &data);
        Verify_Format();
    }
}

TEST_F (CS_01_libhdrTest, CS_01_06_VerifyDataHDR10P) {
    tInfo = {HAL_DATASPACE_V0_SRGB, 0, 1000, HDR_BPC_10, HDR_CAPA_INNER};
    Ihdr->setTargetInfo(&tInfo);

    ExynosHdrStaticInfo s_meta;
    s_meta.sType1.mMaxDisplayLuminance = (1000 * 10000);

    ExynosHdrDynamicInfo d_meta;

    for (int i = 0; i < 4; i++) {
        setDynamicMeta(&d_meta, i);

        Ihdr->initHdrCoefBuildup(HDR_HW_DPU);
        Ihdr->setHDRlayer(false);
        lInfo = {HAL_DATASPACE_BT2020_PQ,// dataspace
               &s_meta, sizeof(ExynosHdrStaticInfo),        // static
               &d_meta, sizeof(ExynosHdrDynamicInfo),       // dynamic
               true,                // pre mult
               HDR_BPC_10,          // bpc
               REND_ORI,            // source
               NULL,                // tf_matrix
               false};              // bypass
        Ihdr->setLayerInfo(0,       //layer
                &lInfo);
        Ihdr->getHdrCoefData(HDR_HW_DPU, 0, &data);
        Verify_Hdr10p_Data(HDR_HW_DPU, 0, &s_meta, &d_meta);
    }
}

TEST_F (CS_01_libhdrTest, CS_01_07_RandomSequence) {
    for (int i = 0; i < 500; i++) {
        int idx = rand() % 5;
        switch(idx) {
            case 0:
                tInfo = {HAL_DATASPACE_V0_SRGB, 0, 1000, HDR_BPC_10, HDR_CAPA_INNER};
                Ihdr->setTargetInfo(&tInfo);
                break;
            case 1:
                Ihdr->initHdrCoefBuildup(HDR_HW_DPU);
                break;
            case 2:
                Ihdr->setHDRlayer(false);
                break;
            case 3:
                ExynosHdrStaticInfo s_meta;
                s_meta.sType1.mMaxDisplayLuminance = (1000 * 10000);

                ExynosHdrDynamicInfo d_meta;
                setDynamicMeta(&d_meta, 0);

                lInfo = {HAL_DATASPACE_BT2020_PQ,// dataspace
                    &s_meta, sizeof(ExynosHdrStaticInfo),        // static
                    &d_meta, sizeof(ExynosHdrDynamicInfo),       // dynamic
                    true,                // pre mult
                    HDR_BPC_10,          // bpc
                    REND_ORI,            // source
                    NULL,                // tf_matrix
                    false};              // bypass
                Ihdr->setLayerInfo(0,       //layer
                        &lInfo);
                break;
            case 4:
                Ihdr->getHdrCoefData(HDR_HW_DPU, 0, &data);
                break;
        }
    }
}

}
}
}
}
