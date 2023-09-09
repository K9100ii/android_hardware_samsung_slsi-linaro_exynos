/*
 * Copyright 2022 The Android Open Source Project
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

#define LOG_TAG "CS_02_libdisplaycolor"

#include <vector>
#include <map>
#include <cstring>
#include <hardware/exynos/libdisplaycolor.h>
#include <hardware/exynos/libdisplaycolor_drv.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <stdio.h>
#include <fcntl.h>
#include <cutils/properties.h>
#include "CS_00_base.h"

namespace android {
namespace hardware {
namespace exynos {
namespace {

using std::string;
using std::vector;
using std::map;
using std::unordered_map;
using std::pair;
using std::make_pair;
using std::to_string;

class CS_02_libdisplaycolor : public CS_00_base {
  public:
    void SetUp() override {
        //GTEST_SKIP(); return;
        ASSERT_TRUE(stopSystemService());
        ASSERT_NO_FATAL_FAILURE(
                mDisplayColor = (std::shared_ptr<IDisplayColor>)IDisplayColor::createInstance(0));
        ASSERT_LT(0, mDisplayColor->getDqeLutSize());
        ASSERT_NE(nullptr, mDisplayColorMem = malloc(mDisplayColor->getDqeLutSize()));

        setTestColorModes();
        setTestRenderIntents();
        SetUpBase();
    }

    void TearDown() override {
        //GTEST_SKIP(); return;
        TearDownBase();
        mTestColorModes.clear();
        mTestRenderIntents.clear();
        if (mDisplayColorMem)
            free(mDisplayColorMem);
    }

  protected:
    void *mDisplayColorMem;
    std::shared_ptr<IDisplayColor> mDisplayColor;

    void setTestColorModes() {
      mTestColorModes.clear();
      vector<DisplayColorMode> cmList = mDisplayColor->getColorModes();

      for (auto mode : cmList) {
              mTestColorModes.push_back(mode.gamutId);
      }
    }

    void setTestRenderIntents() {
      mTestRenderIntents.clear();
      vector<DisplayColorMode> cmList = mDisplayColor->getColorModes();

      for (auto mode : cmList) {
          vector<DisplayRenderIntent> riList = mDisplayColor->getRenderIntents(mode.gamutId);

          for (auto intent : riList) {
              mTestRenderIntents[mode.gamutId].push_back(intent.intentId);
          }
      }
    }

    void checkFormat() {
        struct dqe_colormode_global_header *gl_header;
        struct dqe_colormode_data_header *dt_header;
        int size, count;

        ASSERT_TRUE(mDisplayColorMem);

        gl_header = (struct dqe_colormode_global_header *)mDisplayColorMem;
        ASSERT_EQ(DQE_COLORMODE_MAGIC, gl_header->magic);
        ASSERT_EQ(sizeof(struct dqe_colormode_global_header), gl_header->header_size);
        ASSERT_LE(gl_header->header_size, gl_header->total_size);

        size = gl_header->header_size;
        count = 0;
        while (size < gl_header->total_size) {
            dt_header = (struct dqe_colormode_data_header *) ((char *)gl_header + size);
            ASSERT_TRUE(dt_header);
            ASSERT_EQ(DQE_COLORMODE_MAGIC, dt_header->magic);
            ASSERT_EQ(sizeof(struct dqe_colormode_data_header), dt_header->header_size);
            ASSERT_LE(dt_header->header_size, dt_header->total_size);
            ASSERT_LT(0, dt_header->id);
            ASSERT_GT(DQE_COLORMODE_ID_MAX, dt_header->id);

            size += dt_header->total_size;
            count++;
        }

        ASSERT_EQ(gl_header->total_size, size);
        ASSERT_EQ(gl_header->num_data, count);
    }

    virtual void checkData() override {
        struct dqe_colormode_global_header *gl_header;
        struct dqe_colormode_data_header *dt_header;
        string data;
        int size;
        const char *node;
        vector<int> attrs;

        ASSERT_TRUE(mDisplayColorMem);

        gl_header = (struct dqe_colormode_global_header *)mDisplayColorMem;

        size = gl_header->header_size;
        while (size < gl_header->total_size) {
            dt_header = (struct dqe_colormode_data_header *) ((char *)gl_header + size);
            data = (const char *)((char *)dt_header + dt_header->header_size);

            if (dt_header->id == DQE_COLORMODE_ID_GAMMA_MATRIX && mMatrix.length()) {
                ASSERT_STREQ(mMatrix.c_str(), data.c_str());
            } else {
                ASSERT_GE(mLutMap.size(), gl_header->num_data);
                attrs.clear();
                for (auto attr : dt_header->attr)
                    attrs.push_back((attr == (uint8_t)-1) ? (int)-1 : attr);

                node = dqe_colormode_node[dt_header->id];
                ASSERT_TRUE(node);
                //ASSERT_EQ(mLutMap[make_pair(node, attrs)].size(), dt_header->total_size - dt_header->header_size -1);
                EXPECT_STRCASEEQ(mLutMap[make_pair(node, attrs)].c_str(), data.c_str());
            }

            size += dt_header->total_size;
        }
    }

    std::vector<uint32_t> mTestColorModes;
    std::unordered_map<uint32_t, std::vector<uint32_t>> mTestRenderIntents;
};

TEST_F(CS_02_libdisplaycolor, CS_02_01_ColorModeFormatTest) {
    for (auto mode : mTestColorModes) {
        std::cout << "---Testing Color Mode " << mode << "---" << std::endl;
        ASSERT_NO_FATAL_FAILURE(mDisplayColor->setColorMode(mode));
        ASSERT_NO_FATAL_FAILURE(mDisplayColor->getDqeLut(mDisplayColorMem));

        checkFormat();
    }
}

TEST_F(CS_02_libdisplaycolor, CS_02_02_ColorModeDataTest) {
    for (auto mode : mTestColorModes) {
        std::cout << "---Testing Color Mode " << mode << "---" << std::endl;
        ASSERT_NO_FATAL_FAILURE(mDisplayColor->setColorMode(mode));
        ASSERT_NO_FATAL_FAILURE(mDisplayColor->getDqeLut(mDisplayColorMem));

        parseXml(mode);
        checkData();
    }
}

TEST_F(CS_02_libdisplaycolor, CS_02_03_RenderIntentFormatTest) {
    for (auto mode : mTestColorModes) {
        for (auto intent : mTestRenderIntents[mode]) {
            std::cout << "---Testing Color Mode " << mode <<
                " Render Intent " << intent << "---" << std::endl;
            ASSERT_NO_FATAL_FAILURE(mDisplayColor->setColorModeWithRenderIntent(mode, intent));
            ASSERT_NO_FATAL_FAILURE(mDisplayColor->getDqeLut(mDisplayColorMem));

            checkFormat();
        }
    }
}

TEST_F(CS_02_libdisplaycolor, CS_02_04_RenderIntentDataTest) {
    for (auto mode : mTestColorModes) {
        for (auto intent : mTestRenderIntents[mode]) {
            std::cout << "---Testing Color Mode " << mode <<
                " Render Intent " << intent << "---" << std::endl;
            ASSERT_NO_FATAL_FAILURE(mDisplayColor->setColorModeWithRenderIntent(mode, intent));
            ASSERT_NO_FATAL_FAILURE(mDisplayColor->getDqeLut(mDisplayColorMem));

            parseXml(mode, intent);
            checkData();
        }
    }
}

TEST_F(CS_02_libdisplaycolor, CS_02_05_ColorTransformFormatTest) {
    for (const auto& matrix : mTestMatrix) {
        std::cout << "---Testing Color Transform " << matrix.first << "---" << std::endl;
        ASSERT_NO_FATAL_FAILURE(mDisplayColor->setColorTransform(matrix.second.data(), 0));
        ASSERT_NO_FATAL_FAILURE(mDisplayColor->getDqeLut(mDisplayColorMem));

        checkFormat();
    }
}

TEST_F(CS_02_libdisplaycolor, CS_02_06_ColorTransformDataTest) {
    for (const auto& matrix : mTestMatrix) {
        std::cout << "---Testing Color Transform " << matrix.first << "---" << std::endl;
        ASSERT_NO_FATAL_FAILURE(mDisplayColor->setColorTransform(matrix.second.data(), 0));
        ASSERT_NO_FATAL_FAILURE(mDisplayColor->getDqeLut(mDisplayColorMem));

        setMatrix(matrix.second);
        checkData();
    }
}

TEST_F(CS_02_libdisplaycolor, CS_02_07_RandomFuncParamTest) {
    int count = 500;

    std::cout << "---Testing Random Function/Parameter " << count << "---" << std::endl;
    while (count--) {
        int num = rand() % 7;
        int arg1 = rand() % 100;
        int arg2 = rand() % 100;

        switch (num) {
        case 0:
            mDisplayColor->setColorMode(arg1);
            break;
        case 1:
            mDisplayColor->getRenderIntents(arg1);
            break;
        case 2:
            mDisplayColor->setColorModeWithRenderIntent(arg1, arg2);
            break;
        case 3:
            mDisplayColor->getDqeLut(mDisplayColorMem);
            break;
        case 4:
            {
            auto it = mTestMatrix.begin();
            std::advance(it, rand() % mTestMatrix.size());
            mDisplayColor->setColorTransform(it->second.data(), arg2);
            }
            break;
        case 5:
            mDisplayColor->getDqeLutSize();
            break;
        case 6:
            mDisplayColor->getColorModes();
            break;
        }
    }
}

}  // anonymous namespace
}  // namespace exynos
}  // namespace hardware
}  // namespace android
