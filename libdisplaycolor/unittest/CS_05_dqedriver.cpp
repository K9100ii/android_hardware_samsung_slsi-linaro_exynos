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

#define LOG_TAG "CS_05_dqedriver"

#include <composer-command-buffer/2.2/ComposerCommandBuffer.h>
#include <composer-vts/2.1/GraphicsComposerCallback.h>
#include <composer-vts/2.1/TestCommandReader.h>
#include <composer-vts/2.2/ComposerVts.h>
#include <composer-vts/2.2/ReadbackVts.h>
#include <composer-vts/2.2/RenderEngineVts.h>
#include <gtest/gtest.h>
#include <hidl/GtestPrinter.h>
#include <hidl/ServiceManagement.h>
#include <ui/GraphicBuffer.h>
#include <ui/GraphicBufferAllocator.h>
#include <ui/PixelFormat.h>
#include <ui/Rect.h>
#include <ui/Region.h>

#include <vector>
#include <map>
#include <cstring>
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

using namespace android::hardware::graphics;
using namespace android::hardware::graphics::composer;
using namespace android::hardware::graphics::composer::V2_2;
using namespace android::hardware::graphics::composer::V2_2::vts;

using android::Rect;
using common::V1_1::BufferUsage;
using common::V1_1::Dataspace;
using common::V1_1::PixelFormat;
using V2_1::Config;
using V2_1::Display;
using V2_1::vts::NativeHandleWrapper;
using V2_1::vts::TestCommandReader;
using vts::Gralloc;

class CS_05_dqedriver : public CS_00_base,
                                public testing::WithParamInterface<std::string> {
  public:
    using PowerMode = V2_1::IComposerClient::PowerMode;
    void SetUp() override {
        //GTEST_SKIP(); return;
        ASSERT_TRUE(stopSystemService());
        ASSERT_NO_FATAL_FAILURE(
                mComposer = std::make_unique<Composer>(IComposer::getService(GetParam())));
        ASSERT_NO_FATAL_FAILURE(mComposerClient = mComposer->createClient());
        mComposerCallback = new V2_1::vts::GraphicsComposerCallback;
        mComposerClient->registerCallback(mComposerCallback);

        SetUpBase();

        // assume the first display is primary and is never removed
        mPrimaryDisplay = waitForFirstDisplay();
        Config activeConfig;
        ASSERT_NO_FATAL_FAILURE(activeConfig = mComposerClient->getActiveConfig(mPrimaryDisplay));
        ASSERT_NO_FATAL_FAILURE(
            mDisplayWidth = mComposerClient->getDisplayAttribute(
                mPrimaryDisplay, activeConfig, IComposerClient::Attribute::WIDTH));
        ASSERT_NO_FATAL_FAILURE(
            mDisplayHeight = mComposerClient->getDisplayAttribute(
                mPrimaryDisplay, activeConfig, IComposerClient::Attribute::HEIGHT));

        setTestColorModes();
        setTestRenderIntents();

        // explicitly disable vsync
        ASSERT_NO_FATAL_FAILURE(mComposerClient->setVsyncEnabled(mPrimaryDisplay, false));
        mComposerCallback->setVsyncAllowed(false);

        // set up command writer/reader and gralloc
        mWriter = std::make_shared<CommandWriterBase>(1024);
        mReader = std::make_unique<TestCommandReader>();
        mGralloc = std::make_shared<Gralloc>();

        ASSERT_NO_FATAL_FAILURE(mComposerClient->setPowerMode(mPrimaryDisplay, PowerMode::ON));

        ASSERT_NO_FATAL_FAILURE(
                mTestRenderEngine = std::unique_ptr<TestRenderEngine>(new TestRenderEngine(
                        renderengine::RenderEngineCreationArgs::Builder()
                            .setPixelFormat(static_cast<int>(ui::PixelFormat::RGBA_8888))
                            .setImageCacheSize(TestRenderEngine::sMaxFrameBufferAcquireBuffers)
                            .setUseColorManagerment(true)
                            .setEnableProtectedContext(false)
                            .setPrecacheToneMapperShaderOnly(false)
                            .setContextPriority(renderengine::RenderEngine::ContextPriority::HIGH)
                            .build())));

        renderengine::DisplaySettings clientCompositionDisplay;
        clientCompositionDisplay.physicalDisplay = Rect(mDisplayWidth, mDisplayHeight);
        clientCompositionDisplay.clip = clientCompositionDisplay.physicalDisplay;

        mTestRenderEngine->initGraphicBuffer(
                static_cast<uint32_t>(mDisplayWidth), static_cast<uint32_t>(mDisplayHeight), 1,
                static_cast<uint64_t>(BufferUsage::CPU_READ_OFTEN | BufferUsage::CPU_WRITE_OFTEN |
                                      BufferUsage::GPU_RENDER_TARGET));
        mTestRenderEngine->setDisplaySettings(clientCompositionDisplay);
        ASSERT_NO_FATAL_FAILURE(
                mWriter->setColorTransform(mTestMatrix[BYPASS_NAME].data(), ColorTransform::IDENTITY));

        setExpectedColors();
    }

    void TearDown() override {
        //GTEST_SKIP(); return;
        TearDownBase();
        mTestColorModes.clear();
        mTestRenderIntents.clear();
        ASSERT_NO_FATAL_FAILURE(mComposerClient->setPowerMode(mPrimaryDisplay, PowerMode::OFF));
        EXPECT_EQ(0, mReader->mErrors.size());
        EXPECT_EQ(0, mReader->mCompositionChanges.size());
        if (mComposerCallback != nullptr) {
            EXPECT_EQ(0, mComposerCallback->getInvalidHotplugCount());
            EXPECT_EQ(0, mComposerCallback->getInvalidRefreshCount());
            EXPECT_EQ(0, mComposerCallback->getInvalidVsyncCount());
        }
    }

    void clearCommandReaderState() {
        mReader->mCompositionChanges.clear();
        mReader->mErrors.clear();
    }

    void writeLayers(const std::vector<std::shared_ptr<TestLayer>>& layers) {
        for (auto layer : layers) {
            layer->write(mWriter);
        }
        execute();
    }

    void execute() {
        ASSERT_NO_FATAL_FAILURE(mComposerClient->execute(mReader.get(), mWriter.get()));
    }

    void setTestColorModes() {
        mTestColorModes.clear();
        mComposerClient->getRaw()->getColorModes_2_2(mPrimaryDisplay, [&](const auto& tmpError,
                                                                          const auto& tmpModes) {
            ASSERT_EQ(Error::NONE, tmpError);
            for (ColorMode mode : tmpModes)
                mTestColorModes.push_back(mode);
        });
    }

    void setTestRenderIntents() {
        mTestRenderIntents.clear();

        mComposerClient->getRaw()->getColorModes_2_2(mPrimaryDisplay, [&](const auto& tmpError,
                                                                          const auto& tmpModes) {
            ASSERT_EQ(Error::NONE, tmpError);
            for (ColorMode mode : tmpModes) {
                mComposerClient->getRaw()->getRenderIntents(mPrimaryDisplay, mode,
                                            [&](const auto& tmpError, const auto& tmpIntents) {
                    ASSERT_EQ(Error::NONE, tmpError);
                    for (RenderIntent intent : tmpIntents)
                        mTestRenderIntents[mode].push_back(intent);
                });
            }
        });
    }

    std::unique_ptr<Composer> mComposer;
    std::shared_ptr<ComposerClient> mComposerClient;

    sp<V2_1::vts::GraphicsComposerCallback> mComposerCallback;
    // the first display and is assumed never to be removed
    Display mPrimaryDisplay;
    int32_t mDisplayWidth;
    int32_t mDisplayHeight;
    std::vector<ColorMode> mTestColorModes;
    std::unordered_map<ColorMode, std::vector<RenderIntent>> mTestRenderIntents;
    std::shared_ptr<CommandWriterBase> mWriter;
    std::unique_ptr<TestCommandReader> mReader;
    std::shared_ptr<Gralloc> mGralloc;
    std::unique_ptr<TestRenderEngine> mTestRenderEngine;

    bool mHasReadbackBuffer;
    PixelFormat mPixelFormat;
    Dataspace mDataspace;

    unordered_map<string, vector<pair<IComposerClient::Color, IComposerClient::Color>>> mExpectedColor;

    //static constexpr uint32_t kClientTargetSlotCount = 64;

   private:
    Display waitForFirstDisplay() {
        while (true) {
            std::vector<Display> displays = mComposerCallback->getDisplays();
            if (displays.empty()) {
                usleep(5 * 1000);
                continue;
            }
            return displays[0];
        }
    }

    void setExpectedColors() {
        mExpectedColor.clear();
        mExpectedColor[BYPASS_NAME].clear();
        mExpectedColor[BYPASS_NAME].emplace_back(RED, RED);
        mExpectedColor[BYPASS_NAME].emplace_back(GREEN, GREEN);
        mExpectedColor[BYPASS_NAME].emplace_back(BLUE, BLUE);
        mExpectedColor[BYPASS_NAME].emplace_back(IComposerClient::Color{0x38, 0x68, 0xa8, 0xff},
                                                 IComposerClient::Color{0x38, 0x68, 0xa8, 0xff});

        mExpectedColor[BOOSTED_NAME].clear();
        mExpectedColor[BOOSTED_NAME].emplace_back(RED, RED);
        mExpectedColor[BOOSTED_NAME].emplace_back(GREEN, GREEN);
        mExpectedColor[BOOSTED_NAME].emplace_back(BLUE, BLUE);
        mExpectedColor[BOOSTED_NAME].emplace_back(IComposerClient::Color{0x38, 0x68, 0xa8, 0xff},
                                                  IComposerClient::Color{0x37, 0x5B, 0xb6, 0xff});

        mExpectedColor[NIGHTLIGHT_NAME].clear();
        mExpectedColor[NIGHTLIGHT_NAME].emplace_back(RED, RED);
        mExpectedColor[NIGHTLIGHT_NAME].emplace_back(GREEN, IComposerClient::Color{0x0, 0xB8, 0x0, 0xff});
        mExpectedColor[NIGHTLIGHT_NAME].emplace_back(BLUE, IComposerClient::Color{0x0, 0x0, 0x75, 0xff});
        mExpectedColor[NIGHTLIGHT_NAME].emplace_back(IComposerClient::Color{0x38, 0x68, 0xa8, 0xff},
                                                     IComposerClient::Color{0x38, 0x4B, 0x4d, 0xff});

        EXPECT_EQ(mExpectedColor.size(), mTestMatrix.size());
    }
};

TEST_P(CS_05_dqedriver, CS_05_01_ColorModeDataTest) {
    RenderIntent intent = RenderIntent::COLORIMETRIC;
    for (ColorMode mode : mTestColorModes) {
        for (auto color : mExpectedColor[BYPASS_NAME]) {
            std::cout << "---Testing Color Mode " << static_cast<uint32_t>(mode)
                      << " colors " << (int)color.first.r << "," << (int)color.first.g
                      << "," << (int)color.first.b << "---" << std::endl;
            mWriter->selectDisplay(mPrimaryDisplay);
            ASSERT_NO_FATAL_FAILURE(
                    mComposerClient->setColorMode(mPrimaryDisplay, mode, intent));

            mWriter->validateDisplay();
            execute();
            ASSERT_EQ(0, mReader->mCompositionChanges.size());
            ASSERT_EQ(0, mReader->mErrors.size());
            mWriter->presentDisplay();
            execute();
            ASSERT_EQ(0, mReader->mErrors.size());

            parseXml((uint32_t)mode, (uint32_t)intent);
            checkData();
        }
    }
}

TEST_P(CS_05_dqedriver, CS_05_02_RenderIntentDataTest) {
    for (ColorMode mode : mTestColorModes) {
        for (RenderIntent intent : mTestRenderIntents[mode]) {
            for (auto color : mExpectedColor[BYPASS_NAME]) {
                std::cout << "---Testing Color Mode " << static_cast<uint32_t>(mode)
                    << " Render Intent " << static_cast<uint32_t>(intent)
                    << " colors " << (int)color.first.r << "," << (int)color.first.g
                    << "," << (int)color.first.b << "---" << std::endl;
                mWriter->selectDisplay(mPrimaryDisplay);
                ASSERT_NO_FATAL_FAILURE(
                        mComposerClient->setColorMode(mPrimaryDisplay, mode, intent));

                mWriter->validateDisplay();
                execute();
                ASSERT_EQ(0, mReader->mCompositionChanges.size());
                ASSERT_EQ(0, mReader->mErrors.size());
                mWriter->presentDisplay();
                execute();
                ASSERT_EQ(0, mReader->mErrors.size());

                parseXml((uint32_t)mode, (uint32_t)intent);
                checkData();
            }
        }
    }
}

TEST_P(CS_05_dqedriver, CS_05_03_ColorTransformDataTest) {
    for (const auto& matrix : mTestMatrix) {
        for (auto color : mExpectedColor[matrix.first]) {
            std::cout << "---Testing Color Transform " << matrix.first
                    << " colors " << (int)color.first.r << "," << (int)color.first.g
                    << "," << (int)color.first.b << "---" << std::endl;
            mWriter->selectDisplay(mPrimaryDisplay);
            ASSERT_NO_FATAL_FAILURE(
                    mWriter->setColorTransform(matrix.second.data(), ColorTransform::IDENTITY));

            mWriter->validateDisplay();
            execute();
            ASSERT_EQ(0, mReader->mCompositionChanges.size());
            ASSERT_EQ(0, mReader->mErrors.size());
            mWriter->presentDisplay();
            execute();
            ASSERT_EQ(0, mReader->mErrors.size());

            setMatrix(matrix.second);
            checkData();
        }
    }
}

TEST_P(CS_05_dqedriver, CS_05_04_ColorModeReadbackTest) {
    auto layer = std::make_shared<TestColorLayer>(mComposerClient, mPrimaryDisplay);

    for (ColorMode mode : mTestColorModes) {
        for (auto color : mExpectedColor[BYPASS_NAME]) {
            std::cout << "---Testing Color Mode " << static_cast<uint32_t>(mode)
                        << " colors " << (int)color.first.r << "," << (int)color.first.g
                        << "," << (int)color.first.b << "---" << std::endl;
            mWriter->selectDisplay(mPrimaryDisplay);
            ASSERT_NO_FATAL_FAILURE(
                    mComposerClient->setColorMode(mPrimaryDisplay, mode, RenderIntent::COLORIMETRIC));

            mComposerClient->getRaw()->getReadbackBufferAttributes(
                    mPrimaryDisplay,
                    [&](const auto& tmpError, const auto& tmpPixelFormat, const auto& tmpDataspace) {
                        mHasReadbackBuffer = ReadbackHelper::readbackSupported(tmpPixelFormat,
                                                                               tmpDataspace, tmpError);
                        mPixelFormat = tmpPixelFormat;
                        mDataspace = tmpDataspace;
                    });

            IComposerClient::Rect coloredSquare({0, 0, mDisplayWidth, mDisplayHeight});
            layer->setColor(color.first);
            layer->setDisplayFrame(coloredSquare);
            layer->setZOrder(10);

            std::vector<std::shared_ptr<TestLayer>> layers = {layer};

            // expected color for each pixel
            std::vector<IComposerClient::Color> expectedColors(mDisplayWidth * mDisplayHeight);
            ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth, coloredSquare, color.second);

            ReadbackBuffer readbackBuffer(mPrimaryDisplay, mComposerClient, mGralloc, mDisplayWidth,
                                          mDisplayHeight, mPixelFormat, mDataspace);
            if (mHasReadbackBuffer)
                ASSERT_NO_FATAL_FAILURE(readbackBuffer.setReadbackBuffer());

            writeLayers(layers);
            ASSERT_EQ(0, mReader->mErrors.size());
            mWriter->validateDisplay();
            execute();
            if (mReader->mCompositionChanges.size() != 0) {
                clearCommandReaderState();
                GTEST_SKIP() << "HWC may not be ready for composition";
                return;
            }
            ASSERT_EQ(0, mReader->mErrors.size());
            mWriter->presentDisplay();
            execute();
            ASSERT_EQ(0, mReader->mErrors.size());

            if (mHasReadbackBuffer) {
                ASSERT_NO_FATAL_FAILURE(readbackBuffer.checkReadbackBuffer(expectedColors));
                mTestRenderEngine->setRenderLayers(layers);
                ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->drawLayers());
                ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->checkColorBuffer(expectedColors));
            } else {
                usleep(500000);
            }
        }
    }
}

TEST_P(CS_05_dqedriver, CS_05_05_RenderIntentReadbackTest) {
    auto layer = std::make_shared<TestColorLayer>(mComposerClient, mPrimaryDisplay);

    for (ColorMode mode : mTestColorModes) {
        for (RenderIntent intent : mTestRenderIntents[mode]) {
            for (auto color : mExpectedColor[BYPASS_NAME]) {
                std::cout << "---Testing Color Mode " << static_cast<uint32_t>(mode)
                    << " Render Intent " << static_cast<uint32_t>(intent)
                    << " colors " << (int)color.first.r << "," << (int)color.first.g
                    << "," << (int)color.first.b << "---" << std::endl;
                mWriter->selectDisplay(mPrimaryDisplay);
                ASSERT_NO_FATAL_FAILURE(
                        mComposerClient->setColorMode(mPrimaryDisplay, mode, intent));

                mComposerClient->getRaw()->getReadbackBufferAttributes(
                        mPrimaryDisplay,
                        [&](const auto& tmpError, const auto& tmpPixelFormat, const auto& tmpDataspace) {
                            mHasReadbackBuffer = ReadbackHelper::readbackSupported(tmpPixelFormat,
                                                                                   tmpDataspace, tmpError);
                            mPixelFormat = tmpPixelFormat;
                            mDataspace = tmpDataspace;
                        });

                IComposerClient::Rect coloredSquare({0, 0, mDisplayWidth, mDisplayHeight});
                layer->setColor(color.first);
                layer->setDisplayFrame(coloredSquare);
                layer->setZOrder(10);

                std::vector<std::shared_ptr<TestLayer>> layers = {layer};

                // expected color for each pixel
                std::vector<IComposerClient::Color> expectedColors(mDisplayWidth * mDisplayHeight);
                ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth, coloredSquare, color.second);

                ReadbackBuffer readbackBuffer(mPrimaryDisplay, mComposerClient, mGralloc, mDisplayWidth,
                                              mDisplayHeight, mPixelFormat, mDataspace);
                if (mHasReadbackBuffer)
                    ASSERT_NO_FATAL_FAILURE(readbackBuffer.setReadbackBuffer());

                writeLayers(layers);
                ASSERT_EQ(0, mReader->mErrors.size());
                mWriter->validateDisplay();
                execute();
                if (mReader->mCompositionChanges.size() != 0) {
                    clearCommandReaderState();
                    GTEST_SKIP() << "HWC may not be ready for composition";
                    return;
                }
                ASSERT_EQ(0, mReader->mErrors.size());
                mWriter->presentDisplay();
                execute();
                ASSERT_EQ(0, mReader->mErrors.size());

                if (mHasReadbackBuffer) {
                    ASSERT_NO_FATAL_FAILURE(readbackBuffer.checkReadbackBuffer(expectedColors));
                    mTestRenderEngine->setRenderLayers(layers);
                    ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->drawLayers());
                    ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->checkColorBuffer(expectedColors));
                } else {
                    usleep(500000);
                }
            }
        }
    }
}

TEST_P(CS_05_dqedriver, CS_05_06_ColorTransformReadbackTest) {
    auto layer = std::make_shared<TestColorLayer>(mComposerClient, mPrimaryDisplay);

    for (const auto& matrix : mTestMatrix) {
        for (auto color : mExpectedColor[matrix.first]) {
            std::cout << "---Testing Color Transform " << matrix.first
                    << " colors " << (int)color.first.r << "," << (int)color.first.g
                    << "," << (int)color.first.b << "---" << std::endl;
            mWriter->selectDisplay(mPrimaryDisplay);
            ASSERT_NO_FATAL_FAILURE(
                    mWriter->setColorTransform(matrix.second.data(), ColorTransform::IDENTITY));

            mComposerClient->getRaw()->getReadbackBufferAttributes(
                    mPrimaryDisplay,
                    [&](const auto& tmpError, const auto& tmpPixelFormat, const auto& tmpDataspace) {
                        mHasReadbackBuffer = ReadbackHelper::readbackSupported(tmpPixelFormat,
                                                                               tmpDataspace, tmpError);
                        mPixelFormat = tmpPixelFormat;
                        mDataspace = tmpDataspace;
                    });

            IComposerClient::Rect coloredSquare({0, 0, mDisplayWidth, mDisplayHeight});
            layer->setColor(color.first);
            layer->setDisplayFrame(coloredSquare);
            layer->setZOrder(10);

            std::vector<std::shared_ptr<TestLayer>> layers = {layer};

            // expected color for each pixel
            std::vector<IComposerClient::Color> expectedColors(mDisplayWidth * mDisplayHeight);
            ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth, coloredSquare, color.second);

            ReadbackBuffer readbackBuffer(mPrimaryDisplay, mComposerClient, mGralloc, mDisplayWidth,
                                          mDisplayHeight, mPixelFormat, mDataspace);
            if (mHasReadbackBuffer)
                ASSERT_NO_FATAL_FAILURE(readbackBuffer.setReadbackBuffer());

            writeLayers(layers);
            ASSERT_EQ(0, mReader->mErrors.size());
            mWriter->validateDisplay();
            execute();
            if (mReader->mCompositionChanges.size() != 0) {
                clearCommandReaderState();
                GTEST_SKIP() << "HWC may not be ready for composition";
                return;
            }
            ASSERT_EQ(0, mReader->mErrors.size());
            mWriter->presentDisplay();
            execute();
            ASSERT_EQ(0, mReader->mErrors.size());

            if (mHasReadbackBuffer) {
                EXPECT_NO_FATAL_FAILURE(readbackBuffer.checkReadbackBuffer(expectedColors));

                std::vector<IComposerClient::Color> inputColors(mDisplayWidth * mDisplayHeight);
                ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth, coloredSquare, color.first);
                mTestRenderEngine->setRenderLayers(layers);
                ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->drawLayers());
                EXPECT_NO_FATAL_FAILURE(mTestRenderEngine->checkColorBuffer(inputColors));
            } else {
                usleep(500000);
            }
        }
    }
}

TEST_P(CS_05_dqedriver, CS_05_07_RandomFuncParamTest) {
    int count = 500;

    std::cout << "---Testing Random Function/Parameter " << count << "---" << std::endl;
    while (count--) {
        mWriter->selectDisplay(mPrimaryDisplay);

        vector<ColorMode> modes;
        ColorMode mode;
        RenderIntent intent;
        int num = (count > 0) ? (rand() % 4) : 255;

        switch (num) {
        case 0:
            mode = static_cast<ColorMode>(rand() % 13);
            intent = static_cast<RenderIntent>(rand() % 4);
            ASSERT_NO_FATAL_FAILURE(
                    mComposerClient->setColorMode(mPrimaryDisplay, mode, intent));
            break;
        case 1:
            mComposerClient->getColorModes(mPrimaryDisplay);
            break;
        case 2:
            modes = mComposerClient->getColorModes(mPrimaryDisplay);
            mode = modes.at(rand() % modes.size());
            mComposerClient->getRenderIntents(mPrimaryDisplay, mode);
            break;
        case 3:
            {
            auto it = mTestMatrix.begin();
            std::advance(it, rand() % mTestMatrix.size());
            ASSERT_NO_FATAL_FAILURE(
                    mWriter->setColorTransform(it->second.data(), ColorTransform::IDENTITY));
            }
            break;
        case 255: /* reset with Native Color Mode & Bypass Matrix*/
            ASSERT_NO_FATAL_FAILURE(
                    mComposerClient->setColorMode(mPrimaryDisplay,
                                ColorMode::NATIVE, RenderIntent::COLORIMETRIC));
            ASSERT_NO_FATAL_FAILURE(
                    mWriter->setColorTransform(mTestMatrix[BYPASS_NAME].data(), ColorTransform::IDENTITY));
            break;
        }

        mWriter->validateDisplay();
        execute();
        ASSERT_EQ(0, mReader->mCompositionChanges.size());
        ASSERT_EQ(0, mReader->mErrors.size());
        mWriter->presentDisplay();
        execute();
        ASSERT_EQ(0, mReader->mErrors.size());
    }
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(CS_05_dqedriver);
INSTANTIATE_TEST_SUITE_P(
        PerInstance, CS_05_dqedriver,
        testing::ValuesIn(android::hardware::getAllHalInstanceNames(IComposer::descriptor)),
        android::hardware::PrintInstanceNameToString);

}  // anonymous namespace
}  // namespace exynos
}  // namespace hardware
}  // namespace android
