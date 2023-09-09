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

//#include <composer-command-buffer/2.2/ComposerCommandBuffer.h>
//#include <composer-command-buffer/2.3/ComposerCommandBuffer.h>
#include <composer-command-buffer/2.4/ComposerCommandBuffer.h>
#include <composer-vts/2.1/GraphicsComposerCallback.h>
#include <composer-vts/2.1/TestCommandReader.h>
#include <composer-vts/2.2/ComposerVts.h>
#include <composer-vts/2.2/ReadbackVts.h>
#include <composer-vts/2.2/RenderEngineVts.h>
#include <composer-vts/2.3/ComposerVts.h>
#include <composer-vts/2.4/ComposerVts.h>
#include <composer-vts/2.4/GraphicsComposerCallback.h>
#include <composer-vts/2.4/TestCommandReader.h>
#include <gtest/gtest.h>
#include <hidl/GtestPrinter.h>
#include <hidl/ServiceManagement.h>
#include <ui/GraphicBuffer.h>
#include <ui/GraphicBufferAllocator.h>
#include <ui/PixelFormat.h>
#include <ui/Rect.h>
#include <ui/Region.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <cutils/properties.h>
#include <png.h>

#include "VendorVideoAPI_hdrTest.h"
#include "wcgTestVector.h"
#include "hdrTestVector.h"
#include "hdr10pTestVector.h"
#include <hdrUtil.h>

#define __CS_04_03__ /* VerifyHwSetting_WCG */
#define __CS_04_04__ /* VerifyHwSetting_HDR */
#define __CS_04_05__ /* VerifyHwSetting_HDR10P */
#define __CS_04_06__ /* VerifyBuffer_WCG */
#define __CS_04_07__ /* VerifyBuffer_HDR */
#define __CS_04_08__ /* VerifyBuffer_HDR10P */
#define __CS_04_09__ /* VerifyRandomParameter */
#define __CS_04_10__ /* VerifyActualHDR10Effect */
#define __CS_04_11__ /* VerifyActualHDR10PEffect */

using namespace std;

namespace android {
namespace hardware {
namespace graphics {
namespace composer {

using namespace V2_2;
using namespace V2_2::vts;

using android::Rect;
using common::V1_1::BufferUsage;
using common::V1_1::Dataspace;
using common::V1_1::PixelFormat;
using V2_1::Config;
using V2_1::Display;
using V2_1::vts::NativeHandleWrapper;
using V2_1::vts::TestCommandReader;
using vts::Gralloc;

class ReadbackBuffer_mod {
  public:
      ReadbackBuffer_mod(Display display, const std::shared_ptr<ComposerClient>& client,
              const std::shared_ptr<Gralloc>& gralloc, uint32_t width, uint32_t height,
              PixelFormat pixelFormat, Dataspace dataspace) {
          mDisplay = display;

          mComposerClient = client;
          mGralloc = gralloc;

          mPixelFormat = pixelFormat;
          mDataspace = dataspace;

          mWidth = width;
          mHeight = height;
          mLayerCount = 1;
          mFormat = mPixelFormat;
          mUsage = static_cast<uint64_t>(BufferUsage::CPU_READ_OFTEN | BufferUsage::GPU_TEXTURE);

          mAccessRegion.top = 0;
          mAccessRegion.left = 0;
          mAccessRegion.width = width;
          mAccessRegion.height = height;
      }

      void setReadbackBuffer() {
          mBufferHandle.reset(new Gralloc::NativeHandleWrapper(
                      mGralloc->allocate(mWidth, mHeight, mLayerCount, mFormat, mUsage,
                          /*import*/ true, &mStride)));
          ASSERT_NE(false, mGralloc->validateBufferSize(mBufferHandle->get(), mWidth, mHeight,
                      mLayerCount, mFormat, mUsage, mStride));
          ASSERT_NO_FATAL_FAILURE(mComposerClient->setReadbackBuffer(mDisplay, mBufferHandle->get(), -1));
      }

      void checkReadbackBuffer(std::vector<IComposerClient::Color> expectedColors) {
          // lock buffer for reading
          int32_t fenceHandle;
          ASSERT_NO_FATAL_FAILURE(mComposerClient->getReadbackBufferFence(mDisplay, &fenceHandle));

          void* bufData = mGralloc->lock(mBufferHandle->get(), mUsage, mAccessRegion, fenceHandle);
          ASSERT_TRUE(mPixelFormat == PixelFormat::RGB_888 || mPixelFormat == PixelFormat::RGBA_8888);
          int allowed_err = 1; //pixel
          compareColorBuffers(expectedColors, bufData, mStride, mWidth, mHeight, mPixelFormat, allowed_err);
          int32_t unlockFence = mGralloc->unlock(mBufferHandle->get());
          if (unlockFence != -1) {
              sync_wait(unlockFence, -1);
              close(unlockFence);
          }
      }

  protected:
      void compareColorBuffers(std::vector<IComposerClient::Color>& expectedColors,
              void* bufferData, const uint32_t stride,
              const uint32_t width, const uint32_t height,
              const PixelFormat pixelFormat, int allowed_err) {
          const int32_t bytesPerPixel = ReadbackHelper::GetBytesPerPixel(pixelFormat);
          ASSERT_NE(-1, bytesPerPixel);
          for (int row = 0; row < height; row++) {
              for (int col = 0; col < width; col++) {
                  int pixel = row * width + col;
                  int offset = (row * stride + col) * bytesPerPixel;
                  uint8_t* pixelColor = (uint8_t*)bufferData + offset;
                  if (expectedColors[pixel].r - allowed_err > pixelColor[0] || pixelColor[0] > expectedColors[pixel].r + allowed_err)
                      std::cout << "pixelColor[0] : " << (int)pixelColor[0] << "expectedColors[pixel].r : " << (int)expectedColors[pixel].r << std::endl;
                  ASSERT_TRUE(expectedColors[pixel].r - allowed_err <= pixelColor[0] && pixelColor[0] <= expectedColors[pixel].r + allowed_err);
                  if (expectedColors[pixel].g - allowed_err > pixelColor[1] || pixelColor[1] > expectedColors[pixel].g + allowed_err)
                      std::cout << "pixelColor[1] : " << (int)pixelColor[1] << "expectedColors[pixel].g : " << (int)expectedColors[pixel].g << std::endl;
                  ASSERT_TRUE(expectedColors[pixel].g - allowed_err <= pixelColor[1] && pixelColor[1] <= expectedColors[pixel].g + allowed_err);
                  if (expectedColors[pixel].b - allowed_err > pixelColor[2] || pixelColor[2] > expectedColors[pixel].b + allowed_err)
                      std::cout << "pixelColor[2] : " << (int)pixelColor[2] << "expectedColors[pixel].b : " << (int)expectedColors[pixel].b << std::endl;
                  ASSERT_TRUE(expectedColors[pixel].b - allowed_err <= pixelColor[2] && pixelColor[2] <= expectedColors[pixel].b + allowed_err);
              }
          }
      }
          uint32_t mWidth;
          uint32_t mHeight;
    uint32_t mLayerCount;
    PixelFormat mFormat;
    uint64_t mUsage;
    AccessRegion mAccessRegion;
    uint32_t mStride;
    std::unique_ptr<Gralloc::NativeHandleWrapper> mBufferHandle = nullptr;
    PixelFormat mPixelFormat;
    Dataspace mDataspace;
    Display mDisplay;
    std::shared_ptr<Gralloc> mGralloc;
    std::shared_ptr<ComposerClient> mComposerClient;
};

class CS_04_hdrdrvTestBase : public ::testing::Test {
  protected:
    hdrHwInfo        hw;
    wcgTestVector    wcgTV;
    std::unordered_map<int,struct hdr_dat_node*> wcgTV_map;
    std::vector<Dataspace> ds_list = {
        //Dataspace::V0_SRGB_LINEAR,
        Dataspace::V0_SRGB,
        Dataspace::V0_BT601_625,
        Dataspace::DISPLAY_P3,
        Dataspace::BT2020,
        //Dataspace::V0_BT601_525,
        //Dataspace::V0_BT709,
        //Dataspace::DCI_P3_LINEAR,
        //Dataspace::DCI_P3,
        //Dataspace::DISPLAY_P3_LINEAR,
        //Dataspace::BT2020_LINEAR,
        //Dataspace::V0_JFIF,
    };
    hdrTestVector    hdrTV;
    std::unordered_map<int,struct hdr_dat_node*> hdrTV_map;
    std::vector<float> lum_list = {500.0,
                            1000.0, 1500.0,
                            2000.0, 3000.0};
    hdr10pTestVector    hdr10pTV;
    std::unordered_map<int,struct hdr_dat_node*> hdr10pTV_map;

    std::string dump_wr = "/sys/module/exynos_drm/parameters/dpu_hdr_save_dump";
    // wr path = dump_r_pre + dump_r_id + dump_r_post;
    std::string dump_r_pre = "/sys/class/hdr/hdr";
    std::string dump_r_post = "/dump";
    std::string dump_r_id = "0";

    using PowerMode = V2_1::IComposerClient::PowerMode;
    void SetUpBase(const std::string& service_name) {

        //system("stop");
        usleep(15000);

        hw.init();
        wcgTV.init(&hw);
        hdrTV.init(&hw);
        hdr10pTV.init(&hw);

        ASSERT_NO_FATAL_FAILURE(
                mComposer = std::make_unique<V2_3::vts::Composer>(V2_3::IComposer::getService(service_name)));
        ASSERT_NO_FATAL_FAILURE(mComposerClient = mComposer->createClient());
        mComposerCallback = new V2_1::vts::GraphicsComposerCallback;
        mComposerClient->registerCallback(mComposerCallback);

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

        // explicitly disable vsync
        ASSERT_NO_FATAL_FAILURE(mComposerClient->setVsyncEnabled(mPrimaryDisplay, false));
        mComposerCallback->setVsyncAllowed(false);

        // set up command writer/reader and gralloc
        mWriter = std::make_shared<V2_3::CommandWriterBase>(1024);
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
    }

    void TearDown() override {
        ASSERT_NO_FATAL_FAILURE(mComposerClient->setPowerMode(mPrimaryDisplay, PowerMode::OFF));
        EXPECT_EQ(0, mReader->mErrors.size());
        EXPECT_EQ(0, mReader->mCompositionChanges.size());
        if (mComposerCallback != nullptr) {
            EXPECT_EQ(0, mComposerCallback->getInvalidHotplugCount());
            EXPECT_EQ(0, mComposerCallback->getInvalidRefreshCount());
            EXPECT_EQ(0, mComposerCallback->getInvalidVsyncCount());
        }

        //system("start");
        usleep(15000);
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

    std::unique_ptr<V2_3::vts::Composer> mComposer;
    std::shared_ptr<ComposerClient> mComposerClient;

    sp<V2_1::vts::GraphicsComposerCallback> mComposerCallback;
    // the first display and is assumed never to be removed
    Display mPrimaryDisplay;
    int32_t mDisplayWidth;
    int32_t mDisplayHeight;
    std::vector<ColorMode> mTestColorModes;
    std::shared_ptr<V2_3::CommandWriterBase> mWriter;
    std::unique_ptr<TestCommandReader> mReader;
    std::shared_ptr<Gralloc> mGralloc;
    std::unique_ptr<TestRenderEngine> mTestRenderEngine;

    bool mHasReadbackBuffer;
    PixelFormat mPixelFormat;
    Dataspace mDataspace;

    //static constexpr uint32_t kClientTargetSlotCount = 64;
    //
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
        std::string dataSpaceToString(Dataspace ds) {
            switch (ds) {
                case Dataspace::V0_SRGB:
                    return "V0_SRGB";
                case Dataspace::DISPLAY_P3:
                    return "DISPLAY_P3";
                case Dataspace::DCI_P3:
                    return "DCI_P3";
                case Dataspace::ADOBE_RGB:
                    return "ADOBE_RGB";
                default:
                    return "V0_SRGB";
                    //return Dataspace::UNKNOWN;
            }
        }
        Dataspace colorModeToDataspace(ColorMode mode) {
            switch (mode) {
                case ColorMode::SRGB:
                    return Dataspace::V0_SRGB;
                case ColorMode::DISPLAY_P3:
                    return Dataspace::DISPLAY_P3;
                case ColorMode::DCI_P3:
                    return Dataspace::DCI_P3;
                case ColorMode::ADOBE_RGB:
                    return Dataspace::ADOBE_RGB;
#if 0
                case ColorMode::DISPLAY_BT2020:
                    return Dataspace::DISPLAY_BT2020;
                case ColorMode::BT2100_HLG:
                    return Dataspace::BT2020_HLG;
                case ColorMode::BT2100_PQ:
                    return Dataspace::BT2020_PQ;
#endif
                default:
                    return Dataspace::V0_SRGB;
                    //return Dataspace::UNKNOWN;
            }
        }
        void fillColorsWithPngBuffer(int32_t width, int32_t height, uint32_t stride,
                png_bytep* inputPngBuffer, std::vector<IComposerClient::Color> &outputColors) {
            for (int row = 0; row < height; row++) {
                png_bytep png_row = inputPngBuffer[row];
                for (int col = 0; col < width; col++) {
                    int pixel = row * stride + col;     //colors
                    int offset = col * 4;               //png
                    outputColors[pixel].r = png_row[offset + 0];
                    outputColors[pixel].g = png_row[offset + 1];
                    outputColors[pixel].b = png_row[offset + 2];
                }
            }
        }
      struct _png_info_ {
          int width;
          int height;
          png_byte color_type;
          png_byte bit_depth;
          png_bytep *row_pointers = NULL;
      };
      int read_png_file_to_info(char *filename, struct _png_info_ *output) {
          FILE *fp = fopen(filename, "rb");
          if (!fp) {
              ALOGD("file(%s) open fail\n", filename);
              return -1;
          }

          png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
          if(!png) abort();

          png_infop info = png_create_info_struct(png);
          if(!info) abort();

          if(setjmp(png_jmpbuf(png))) abort();

          png_init_io(png, fp);

          png_read_info(png, info);

          output->width      = png_get_image_width(png, info);
          output->height     = png_get_image_height(png, info);
          output->color_type = png_get_color_type(png, info);
          output->bit_depth  = png_get_bit_depth(png, info);

          // Read any output->color_type into 8bit depth, RGBA format.
          // See http://www.libpng.org/pub/png/libpng-manual.txt

          if(output->bit_depth == 16)
              png_set_strip_16(png);

          if(output->color_type == PNG_COLOR_TYPE_PALETTE)
              png_set_palette_to_rgb(png);

          // PNG_COLOR_TYPE_GRAY_ALPHA is always 8 or 16bit depth.
          if(output->color_type == PNG_COLOR_TYPE_GRAY && output->bit_depth < 8)
              png_set_expand_gray_1_2_4_to_8(png);

          if(png_get_valid(png, info, PNG_INFO_tRNS))
              png_set_tRNS_to_alpha(png);

          // These output->color_type don't have an alpha channel then fill it with 0xff.
          if(output->color_type == PNG_COLOR_TYPE_RGB ||
                  output->color_type == PNG_COLOR_TYPE_GRAY ||
                  output->color_type == PNG_COLOR_TYPE_PALETTE)
              png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

          if(output->color_type == PNG_COLOR_TYPE_GRAY ||
                  output->color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
              png_set_gray_to_rgb(png);

          png_read_update_info(png, info);

          if (output->row_pointers) abort();

          output->row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * output->height);
          for(int y = 0; y < output->height; y++) {
              output->row_pointers[y] = (png_byte*)malloc(png_get_rowbytes(png,info));
          }

          png_read_image(png, output->row_pointers);

          fclose(fp);

          png_destroy_read_struct(&png, &info, NULL);

          return 0;
      }
      void free_png_info(struct _png_info_ *output)
      {
          for(int y = 0; y < output->height; y++)
              free(output->row_pointers[y]);
          free(output->row_pointers);
      }


    void Verify_WCG (int in_dataspace, int out_dataspace) {
        refineTransfer(in_dataspace);
        std::unordered_map<int,struct hdr_dat_node*> wcgTV_map = wcgTV.getTestVector(HDR_HW_DPU, 0, in_dataspace, out_dataspace);

        for (auto iter = wcgTV_map.begin(); iter != wcgTV_map.end(); iter++) {
            struct hdr_dat_node *test_vec = iter->second;
            int offset = test_vec->header.byte_offset;
            int length = 4 * test_vec->header.length;
            std::string cmd = dump_wr;
            int fd = open(cmd.c_str(), O_WRONLY);
            printf("fd(%d), str(%s)\n", fd, cmd.c_str());
            cmd.clear(); cmd.resize(128);
            sprintf((char*)cmd.c_str(), "0x%x,0x%x", offset, length);
            std::cout << cmd << std::endl;
            write(fd, cmd.c_str(), (cmd.size() + 1));
            close(fd);

            mWriter->presentDisplay();
            execute();
            ASSERT_EQ(0, mReader->mErrors.size());
            usleep(15000);

            cmd = dump_r_pre + dump_r_id + dump_r_post;
            int fd_dump = open(cmd.c_str(), O_RDONLY);
            printf("fd_dump(%d), str(%s)\n", fd_dump, cmd.c_str());
            cmd.clear(); cmd.resize((length * 2) + (length / 4) + length + 1);
            int len = read(fd_dump, (void*)cmd.c_str(), (length * 2) + (length / 4) + length + 1);
            printf("read dump len(%d)\n", len);
            std::cout << cmd << std::endl;
            std::vector<std::string> result;
            std::vector<std::string> _result;
            std::vector<std::string> __result;
            split(cmd, '\n', _result);
            for (auto iter = _result.begin(); iter != _result.end(); iter++) {
                __result.clear();
                split(*iter, ' ', __result);
                for (auto _iter = __result.begin(); _iter != __result.end(); _iter++) {
                    if ((iter->c_str()[0] >= '0' && iter->c_str()[0] <= '9')
                            || (iter->c_str()[0] >= 'a' && iter->c_str()[0] <= 'f'))
                        result.push_back(*_iter);
                }
            }
            int i = 0;
            for (auto iter = result.begin(); iter != result.end(); iter++) {
                int hw = strToHex(*iter);
                int vec = *((int*)test_vec->data + i);
                printf("test_vector[%d] : %d, hw:%d\n", i, vec, hw);
                ASSERT_EQ(hw, vec);
                i++;
            }
            close(fd_dump);
        }
    }
    void Verify_HDR10 (ExynosHdrStaticInfo *s_meta) {
        std::unordered_map<int,struct hdr_dat_node*> hdrTV_map = hdrTV.getTestVector(HDR_HW_DPU, 0, s_meta);

        for (auto iter = hdrTV_map.begin(); iter != hdrTV_map.end(); iter++) {
            struct hdr_dat_node *test_vec = iter->second;
            int offset = test_vec->header.byte_offset;
            int length = 4 * test_vec->header.length;
            std::string cmd = dump_wr;
            int fd = open(cmd.c_str(), O_WRONLY);
            printf("fd(%d), str(%s)\n", fd, cmd.c_str());
            cmd.clear(); cmd.resize(128);
            sprintf((char*)cmd.c_str(), "0x%x,0x%x", offset, length);
            std::cout << cmd << std::endl;
            write(fd, cmd.c_str(), (cmd.size() + 1));
            close(fd);

            mWriter->presentDisplay();
            execute();
            ASSERT_EQ(0, mReader->mErrors.size());
            usleep(20000);

            cmd = dump_r_pre + dump_r_id + dump_r_post;
            int fd_dump = open(cmd.c_str(), O_RDONLY);
            printf("fd_dump(%d), str(%s)\n", fd_dump, cmd.c_str());
            cmd.clear(); cmd.resize((length * 2) + (length / 4) + length + 1);
            int len = read(fd_dump, (void*)cmd.c_str(), (length * 2) + (length / 4) + length + 1);
            printf("read dump len(%d)\n", len);
            std::cout << cmd << std::endl;
            std::vector<std::string> result;
            std::vector<std::string> _result;
            std::vector<std::string> __result;
            split(cmd, '\n', _result);
            for (auto iter = _result.begin(); iter != _result.end(); iter++) {
                __result.clear();
                split(*iter, ' ', __result);
                for (auto _iter = __result.begin(); _iter != __result.end(); _iter++) {
                    if ((iter->c_str()[0] >= '0' && iter->c_str()[0] <= '9')
                            || (iter->c_str()[0] >= 'a' && iter->c_str()[0] <= 'f'))
                        result.push_back(*_iter);
                }
            }
            int i = 0;
            for (auto iter = result.begin(); iter != result.end(); iter++) {
                printf("test_vector[%d] : %s\n", i, iter->c_str());
                int hw = strToHex(*iter);
                int vec = *((int*)test_vec->data + i);
                ASSERT_EQ(hw, vec);
                i++;
            }
            close(fd_dump);
        }
    }
    void Verify_HDR10P (ExynosHdrStaticInfo *s_meta, ExynosHdrDynamicInfo *d_meta) {
        std::unordered_map<int,struct hdr_dat_node*> hdr10pTV_map = hdr10pTV.getTestVector(HDR_HW_DPU, 0, s_meta, d_meta);

        for (auto iter = hdr10pTV_map.begin(); iter != hdr10pTV_map.end(); iter++) {
            struct hdr_dat_node *test_vec = iter->second;
            int offset = test_vec->header.byte_offset;
            int length = 4 * test_vec->header.length;
            std::string cmd = dump_wr;
            int fd = open(cmd.c_str(), O_WRONLY);
            printf("fd(%d), str(%s)\n", fd, cmd.c_str());
            cmd.clear(); cmd.resize(128);
            sprintf((char*)cmd.c_str(), "0x%x,0x%x", offset, length);
            std::cout << cmd << std::endl;
            write(fd, cmd.c_str(), (cmd.size() + 1));
            close(fd);

            mWriter->presentDisplay();
            execute();
            ASSERT_EQ(0, mReader->mErrors.size());
            usleep(20000);

            cmd = dump_r_pre + dump_r_id + dump_r_post;
            int fd_dump = open(cmd.c_str(), O_RDONLY);
            printf("fd_dump(%d), str(%s)\n", fd_dump, cmd.c_str());
            cmd.clear(); cmd.resize((length * 2) + (length / 4) + length + 1);
            int len = read(fd_dump, (void*)cmd.c_str(), (length * 2) + (length / 4) + length + 1);
            printf("read dump len(%d)\n", len);
            std::cout << cmd << std::endl;
            std::vector<std::string> result;
            std::vector<std::string> _result;
            std::vector<std::string> __result;
            split(cmd, '\n', _result);
            for (auto iter = _result.begin(); iter != _result.end(); iter++) {
                __result.clear();
                split(*iter, ' ', __result);
                for (auto _iter = __result.begin(); _iter != __result.end(); _iter++) {
                    if ((iter->c_str()[0] >= '0' && iter->c_str()[0] <= '9')
                            || (iter->c_str()[0] >= 'a' && iter->c_str()[0] <= 'f'))
                        result.push_back(*_iter);
                }
            }
            int i = 0;
            for (auto iter = result.begin(); iter != result.end(); iter++) {
                printf("test_vector[%d] : %s\n", i, iter->c_str());
                int hw = strToHex(*iter);
                int vec = *((int*)test_vec->data + i);
                ASSERT_EQ(hw, vec);
                i++;
            }
            close(fd_dump);
        }
    }


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

    void setTestColorModes() {
        mTestColorModes.clear();
        mComposerClient->getRaw()->getColorModes_2_2(mPrimaryDisplay, [&](const auto& tmpError,
                                                                          const auto& tmpModes) {
            ASSERT_EQ(Error::NONE, tmpError);
            for (ColorMode mode : tmpModes) {
                //if (std::find(ReadbackHelper::colorModes.begin(), ReadbackHelper::colorModes.end(),
                //              mode) != ReadbackHelper::colorModes.end()) {
                    mTestColorModes.push_back(mode);
            }
        });
    }
};

class CS_04_hdrdrvTest : public CS_04_hdrdrvTestBase,
                public testing::WithParamInterface<std::string> {
  public:
    void SetUp() override { SetUpBase(GetParam()); }
};

#ifdef __CS_04_03__
TEST_P(CS_04_hdrdrvTest, CS_04_03_0_VerifyHwSetting_WCG) {
    for (ColorMode mode : mTestColorModes) {
        std::cout << "---Testing Color Mode " << ReadbackHelper::getColorModeString(mode) << "---" << std::endl;
        mDataspace = colorModeToDataspace(mode);

        mWriter->selectDisplay(mPrimaryDisplay);
        ASSERT_NO_FATAL_FAILURE(
                mComposerClient->setColorMode(mPrimaryDisplay, mode, RenderIntent::COLORIMETRIC));

        std::vector<IComposerClient::Color> inputColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, 0, mDisplayWidth, mDisplayHeight / 4}, RED);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 4, mDisplayWidth, mDisplayHeight / 2},
                                       GREEN);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 2, mDisplayWidth, mDisplayHeight},
                                       BLUE);

        int i = 0;
        //for (int i = 0; i < ds_list.size(); i++) {
            ASSERT_NO_FATAL_FAILURE(
                    std::cout << "out dataspace[" << i << "] : " << dataSpaceToString(mDataspace) << std::endl);
            ASSERT_NO_FATAL_FAILURE(
                    std::cout << "in dataspace[" << i << "] : " << dataSpaceToString(ds_list[i]) << std::endl);

            auto layer = std::make_shared<TestBufferLayer>(
                    mComposerClient, mGralloc, *mTestRenderEngine, mPrimaryDisplay, mDisplayWidth,
                    mDisplayHeight, PixelFormat::RGBA_8888);
            layer->setDisplayFrame({0, 0, mDisplayWidth, mDisplayHeight});
            layer->setZOrder(10);
            layer->setDataspace(ds_list[i], mWriter);
            ASSERT_NO_FATAL_FAILURE(layer->setBuffer(inputColors));

            std::vector<std::shared_ptr<TestLayer>> layers = {layer};

            writeLayers(layers);
            ASSERT_EQ(0, mReader->mErrors.size());

            mWriter->validateDisplay();
            execute();

            if (mReader->mCompositionChanges.size() != 0) {
                clearCommandReaderState();
                GTEST_SUCCEED();
                return;
            }
            ASSERT_EQ(0, mReader->mErrors.size());

            Verify_WCG((int)ds_list[i], (int)mDataspace);

            //ASSERT_NO_FATAL_FAILURE(readbackBuffer.checkReadbackBuffer(expectedColors));
            //mTestRenderEngine->setRenderLayers(layers);
            //ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->drawLayers());
            //ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->checkColorBuffer(expectedColors));
        //}
    }
}

TEST_P(CS_04_hdrdrvTest, CS_04_03_1_VerifyHwSetting_WCG) {
    for (ColorMode mode : mTestColorModes) {
        std::cout << "---Testing Color Mode " << ReadbackHelper::getColorModeString(mode) << "---" << std::endl;
        mDataspace = colorModeToDataspace(mode);

        mWriter->selectDisplay(mPrimaryDisplay);
        ASSERT_NO_FATAL_FAILURE(
                mComposerClient->setColorMode(mPrimaryDisplay, mode, RenderIntent::COLORIMETRIC));

        std::vector<IComposerClient::Color> inputColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, 0, mDisplayWidth, mDisplayHeight / 4}, RED);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 4, mDisplayWidth, mDisplayHeight / 2},
                                       GREEN);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 2, mDisplayWidth, mDisplayHeight},
                                       BLUE);

        int i = 1;
        //for (int i = 0; i < ds_list.size(); i++) {
            ASSERT_NO_FATAL_FAILURE(
                    std::cout << "out dataspace[" << i << "] : " << dataSpaceToString(mDataspace) << std::endl);
            ASSERT_NO_FATAL_FAILURE(
                    std::cout << "in dataspace[" << i << "] : " << dataSpaceToString(ds_list[i]) << std::endl);

            auto layer = std::make_shared<TestBufferLayer>(
                    mComposerClient, mGralloc, *mTestRenderEngine, mPrimaryDisplay, mDisplayWidth,
                    mDisplayHeight, PixelFormat::RGBA_8888);
            layer->setDisplayFrame({0, 0, mDisplayWidth, mDisplayHeight});
            layer->setZOrder(10);
            layer->setDataspace(ds_list[i], mWriter);
            ASSERT_NO_FATAL_FAILURE(layer->setBuffer(inputColors));

            std::vector<std::shared_ptr<TestLayer>> layers = {layer};

            writeLayers(layers);
            ASSERT_EQ(0, mReader->mErrors.size());

            mWriter->validateDisplay();
            execute();

            if (mReader->mCompositionChanges.size() != 0) {
                clearCommandReaderState();
                GTEST_SUCCEED();
                return;
            }
            ASSERT_EQ(0, mReader->mErrors.size());

            Verify_WCG((int)ds_list[i], (int)mDataspace);

            //ASSERT_NO_FATAL_FAILURE(readbackBuffer.checkReadbackBuffer(expectedColors));
            //mTestRenderEngine->setRenderLayers(layers);
            //ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->drawLayers());
            //ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->checkColorBuffer(expectedColors));
        //}
    }
}
TEST_P(CS_04_hdrdrvTest, CS_04_03_2_VerifyHwSetting_WCG) {
    for (ColorMode mode : mTestColorModes) {
        std::cout << "---Testing Color Mode " << ReadbackHelper::getColorModeString(mode) << "---" << std::endl;
        mDataspace = colorModeToDataspace(mode);

        mWriter->selectDisplay(mPrimaryDisplay);
        ASSERT_NO_FATAL_FAILURE(
                mComposerClient->setColorMode(mPrimaryDisplay, mode, RenderIntent::COLORIMETRIC));

        std::vector<IComposerClient::Color> inputColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, 0, mDisplayWidth, mDisplayHeight / 4}, RED);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 4, mDisplayWidth, mDisplayHeight / 2},
                                       GREEN);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 2, mDisplayWidth, mDisplayHeight},
                                       BLUE);

        int i = 2;
        //for (int i = 0; i < ds_list.size(); i++) {
            ASSERT_NO_FATAL_FAILURE(
                    std::cout << "out dataspace[" << i << "] : " << dataSpaceToString(mDataspace) << std::endl);
            ASSERT_NO_FATAL_FAILURE(
                    std::cout << "in dataspace[" << i << "] : " << dataSpaceToString(ds_list[i]) << std::endl);

            auto layer = std::make_shared<TestBufferLayer>(
                    mComposerClient, mGralloc, *mTestRenderEngine, mPrimaryDisplay, mDisplayWidth,
                    mDisplayHeight, PixelFormat::RGBA_8888);
            layer->setDisplayFrame({0, 0, mDisplayWidth, mDisplayHeight});
            layer->setZOrder(10);
            layer->setDataspace(ds_list[i], mWriter);
            ASSERT_NO_FATAL_FAILURE(layer->setBuffer(inputColors));

            std::vector<std::shared_ptr<TestLayer>> layers = {layer};

            writeLayers(layers);
            ASSERT_EQ(0, mReader->mErrors.size());

            mWriter->validateDisplay();
            execute();

            if (mReader->mCompositionChanges.size() != 0) {
                clearCommandReaderState();
                GTEST_SUCCEED();
                return;
            }
            ASSERT_EQ(0, mReader->mErrors.size());

            Verify_WCG((int)ds_list[i], (int)mDataspace);

            //ASSERT_NO_FATAL_FAILURE(readbackBuffer.checkReadbackBuffer(expectedColors));
            //mTestRenderEngine->setRenderLayers(layers);
            //ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->drawLayers());
            //ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->checkColorBuffer(expectedColors));
        //}
    }
}
TEST_P(CS_04_hdrdrvTest, CS_04_03_3_VerifyHwSetting_WCG) {
    for (ColorMode mode : mTestColorModes) {
        std::cout << "---Testing Color Mode " << ReadbackHelper::getColorModeString(mode) << "---" << std::endl;
        mDataspace = colorModeToDataspace(mode);

        mWriter->selectDisplay(mPrimaryDisplay);
        ASSERT_NO_FATAL_FAILURE(
                mComposerClient->setColorMode(mPrimaryDisplay, mode, RenderIntent::COLORIMETRIC));

        std::vector<IComposerClient::Color> inputColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, 0, mDisplayWidth, mDisplayHeight / 4}, RED);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 4, mDisplayWidth, mDisplayHeight / 2},
                                       GREEN);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 2, mDisplayWidth, mDisplayHeight},
                                       BLUE);

        int i = 3;
        //for (int i = 0; i < ds_list.size(); i++) {
            ASSERT_NO_FATAL_FAILURE(
                    std::cout << "out dataspace[" << i << "] : " << dataSpaceToString(mDataspace) << std::endl);
            ASSERT_NO_FATAL_FAILURE(
                    std::cout << "in dataspace[" << i << "] : " << dataSpaceToString(ds_list[i]) << std::endl);

            auto layer = std::make_shared<TestBufferLayer>(
                    mComposerClient, mGralloc, *mTestRenderEngine, mPrimaryDisplay, mDisplayWidth,
                    mDisplayHeight, PixelFormat::RGBA_8888);
            layer->setDisplayFrame({0, 0, mDisplayWidth, mDisplayHeight});
            layer->setZOrder(10);
            layer->setDataspace(ds_list[i], mWriter);
            ASSERT_NO_FATAL_FAILURE(layer->setBuffer(inputColors));

            std::vector<std::shared_ptr<TestLayer>> layers = {layer};

            writeLayers(layers);
            ASSERT_EQ(0, mReader->mErrors.size());

            mWriter->validateDisplay();
            execute();

            if (mReader->mCompositionChanges.size() != 0) {
                clearCommandReaderState();
                GTEST_SUCCEED();
                return;
            }
            ASSERT_EQ(0, mReader->mErrors.size());

            Verify_WCG((int)ds_list[i], (int)mDataspace);

            //ASSERT_NO_FATAL_FAILURE(readbackBuffer.checkReadbackBuffer(expectedColors));
            //mTestRenderEngine->setRenderLayers(layers);
            //ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->drawLayers());
            //ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->checkColorBuffer(expectedColors));
        //}
    }
}
#endif

#ifdef __CS_04_04__
TEST_P(CS_04_hdrdrvTest, CS_04_04_0_VerifyHwSetting_HDR) {
    for (ColorMode mode : mTestColorModes) {
        std::cout << "---Testing Color Mode " << ReadbackHelper::getColorModeString(mode) << "---"
                  << std::endl;
        mDataspace = colorModeToDataspace(mode);
        mWriter->selectDisplay(mPrimaryDisplay);
        ASSERT_NO_FATAL_FAILURE(
                mComposerClient->setColorMode(mPrimaryDisplay, mode, RenderIntent::COLORIMETRIC));

        float maxLuminance;
        float maxAverageLuminance;
        float minLuminance;
        std::vector<Hdr> HdrCapa;
        ASSERT_NO_FATAL_FAILURE(HdrCapa = mComposerClient->getHdrCapabilities(
        mPrimaryDisplay,
        &maxLuminance, &maxAverageLuminance, &minLuminance));
        ASSERT_TRUE(maxLuminance >= minLuminance);
        if(HdrCapa.size() <= 0) {
            std::cout << "HDR not supported (HdrCapa.size() <= 0)" << std::endl;
            GTEST_SKIP() << "HDR not supported";
            return;
        }
        bool hdr_capable = false;
        for (int i = 0; i < HdrCapa.size(); i++) {
            //std::cout << "HdrCapa[" << i << "] : " << (int)HdrCapa[i] << std::endl;
            if ((int)HdrCapa[i] == 2) {
                hdr_capable = true;
                break;
            }
        }
        ASSERT_TRUE(hdr_capable);
        std::vector<IComposerClient::Color> inputColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, 0, mDisplayWidth, mDisplayHeight / 4}, RED);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 4, mDisplayWidth, mDisplayHeight / 2},
                                       GREEN);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 2, mDisplayWidth, mDisplayHeight},
                                       BLUE);

        int i = 0;
        //for (int i = 0; i < lum_list.size(); i++) {
            std::cout << "luminance[" << i << "] : " << lum_list[i] << std::endl;
            auto layer = std::make_shared<TestBufferLayer>(
                    mComposerClient, mGralloc, *mTestRenderEngine, mPrimaryDisplay, mDisplayWidth,
                    mDisplayHeight, PixelFormat::RGBA_8888);
            layer->setDisplayFrame({0, 0, mDisplayWidth, mDisplayHeight});
            layer->setZOrder(10);
            layer->setDataspace(Dataspace::BT2020_PQ, mWriter);
            ASSERT_NO_FATAL_FAILURE(layer->setBuffer(inputColors));

            std::vector<std::shared_ptr<TestLayer>> layers = {layer};

            writeLayers(layers);
            ASSERT_EQ(0, mReader->mErrors.size());

            ExynosHdrStaticInfo s_meta;
            std::vector<V2_3::IComposerClient::PerFrameMetadata> staticMetadata;
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_X, 0.680});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_Y, 0.320});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_X, 0.265});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_Y, 0.690});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_X, 0.150});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_Y, 0.060});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_X, 0.3127});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_Y, 0.3290});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_LUMINANCE, lum_list[i]});
            s_meta.sType1.mMaxDisplayLuminance = (lum_list[i] * 10000);
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MIN_LUMINANCE, 0.1});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_CONTENT_LIGHT_LEVEL, 78.0});
            staticMetadata.push_back(
                    {V2_3::IComposerClient::PerFrameMetadataKey::MAX_FRAME_AVERAGE_LIGHT_LEVEL, 62.0});
            mWriter->setLayerPerFrameMetadata(staticMetadata);
            execute();
            ASSERT_EQ(0, mReader->mErrors.size());

            mWriter->validateDisplay();
            execute();

            if (mReader->mCompositionChanges.size() != 0) {
                clearCommandReaderState();
                GTEST_SUCCEED();
                return;
            }

            ASSERT_EQ(0, mReader->mErrors.size());

            Verify_HDR10(&s_meta);

            //ASSERT_NO_FATAL_FAILURE(readbackBuffer.checkReadbackBuffer(expectedColors));
            //mTestRenderEngine->setRenderLayers(layers);
            //ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->drawLayers());
            //ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->checkColorBuffer(expectedColors));
        //}
    }
}

TEST_P(CS_04_hdrdrvTest, CS_04_04_1_VerifyHwSetting_HDR) {
    for (ColorMode mode : mTestColorModes) {
        std::cout << "---Testing Color Mode " << ReadbackHelper::getColorModeString(mode) << "---"
                  << std::endl;
        mWriter->selectDisplay(mPrimaryDisplay);
        ASSERT_NO_FATAL_FAILURE(
                mComposerClient->setColorMode(mPrimaryDisplay, mode, RenderIntent::COLORIMETRIC));

        float maxLuminance;
        float maxAverageLuminance;
        float minLuminance;
        std::vector<Hdr> HdrCapa;
        ASSERT_NO_FATAL_FAILURE(HdrCapa = mComposerClient->getHdrCapabilities(
        mPrimaryDisplay,
        &maxLuminance, &maxAverageLuminance, &minLuminance));
        ASSERT_TRUE(maxLuminance >= minLuminance);
        if(HdrCapa.size() <= 0) {
            std::cout << "HDR not supported (HdrCapa.size() <= 0)" << std::endl;
            GTEST_SKIP() << "HDR not supported";
            return;
        }
        bool hdr_capable = false;
        for (int i = 0; i < HdrCapa.size(); i++) {
            //std::cout << "HdrCapa[" << i << "] : " << (int)HdrCapa[i] << std::endl;
            if ((int)HdrCapa[i] == 2) {
                hdr_capable = true;
                break;
            }
        }
        ASSERT_TRUE(hdr_capable);
        std::vector<IComposerClient::Color> inputColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, 0, mDisplayWidth, mDisplayHeight / 4}, RED);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 4, mDisplayWidth, mDisplayHeight / 2},
                                       GREEN);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 2, mDisplayWidth, mDisplayHeight},
                                       BLUE);

        int i = 1;
        //for (int i = 0; i < lum_list.size(); i++) {
            std::cout << "luminance[" << i << "] : " << lum_list[i] << std::endl;
            auto layer = std::make_shared<TestBufferLayer>(
                    mComposerClient, mGralloc, *mTestRenderEngine, mPrimaryDisplay, mDisplayWidth,
                    mDisplayHeight, PixelFormat::RGBA_8888);
            layer->setDisplayFrame({0, 0, mDisplayWidth, mDisplayHeight});
            layer->setZOrder(10);
            layer->setDataspace(Dataspace::BT2020_PQ, mWriter);
            ASSERT_NO_FATAL_FAILURE(layer->setBuffer(inputColors));

            std::vector<std::shared_ptr<TestLayer>> layers = {layer};

            writeLayers(layers);
            ASSERT_EQ(0, mReader->mErrors.size());

            ExynosHdrStaticInfo s_meta;
            std::vector<V2_3::IComposerClient::PerFrameMetadata> staticMetadata;
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_X, 0.680});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_Y, 0.320});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_X, 0.265});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_Y, 0.690});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_X, 0.150});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_Y, 0.060});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_X, 0.3127});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_Y, 0.3290});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_LUMINANCE, lum_list[i]});
            s_meta.sType1.mMaxDisplayLuminance = (lum_list[i] * 10000);
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MIN_LUMINANCE, 0.1});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_CONTENT_LIGHT_LEVEL, 78.0});
            staticMetadata.push_back(
                    {V2_3::IComposerClient::PerFrameMetadataKey::MAX_FRAME_AVERAGE_LIGHT_LEVEL, 62.0});
            mWriter->setLayerPerFrameMetadata(staticMetadata);
            execute();
            ASSERT_EQ(0, mReader->mErrors.size());

            mWriter->validateDisplay();
            execute();

            if (mReader->mCompositionChanges.size() != 0) {
                clearCommandReaderState();
                GTEST_SUCCEED();
                return;
            }
            ASSERT_EQ(0, mReader->mErrors.size());

            Verify_HDR10(&s_meta);

            //ASSERT_NO_FATAL_FAILURE(readbackBuffer.checkReadbackBuffer(expectedColors));
            //mTestRenderEngine->setRenderLayers(layers);
            //ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->drawLayers());
            //ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->checkColorBuffer(expectedColors));
        //}
    }
}

TEST_P(CS_04_hdrdrvTest, CS_04_04_2_VerifyHwSetting_HDR) {
    for (ColorMode mode : mTestColorModes) {
        std::cout << "---Testing Color Mode " << ReadbackHelper::getColorModeString(mode) << "---"
                  << std::endl;
        mWriter->selectDisplay(mPrimaryDisplay);
        ASSERT_NO_FATAL_FAILURE(
                mComposerClient->setColorMode(mPrimaryDisplay, mode, RenderIntent::COLORIMETRIC));

        float maxLuminance;
        float maxAverageLuminance;
        float minLuminance;
        std::vector<Hdr> HdrCapa;
        ASSERT_NO_FATAL_FAILURE(HdrCapa = mComposerClient->getHdrCapabilities(
        mPrimaryDisplay,
        &maxLuminance, &maxAverageLuminance, &minLuminance));
        ASSERT_TRUE(maxLuminance >= minLuminance);
        if(HdrCapa.size() <= 0) {
            std::cout << "HDR not supported (HdrCapa.size() <= 0)" << std::endl;
            GTEST_SKIP() << "HDR not supported";
            return;
        }
        bool hdr_capable = false;
        for (int i = 0; i < HdrCapa.size(); i++) {
            //std::cout << "HdrCapa[" << i << "] : " << (int)HdrCapa[i] << std::endl;
            if ((int)HdrCapa[i] == 2) {
                hdr_capable = true;
                break;
            }
        }
        ASSERT_TRUE(hdr_capable);
        std::vector<IComposerClient::Color> inputColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, 0, mDisplayWidth, mDisplayHeight / 4}, RED);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 4, mDisplayWidth, mDisplayHeight / 2},
                                       GREEN);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 2, mDisplayWidth, mDisplayHeight},
                                       BLUE);

        int i = 2;
        //for (int i = 0; i < lum_list.size(); i++) {
            std::cout << "luminance[" << i << "] : " << lum_list[i] << std::endl;
            auto layer = std::make_shared<TestBufferLayer>(
                    mComposerClient, mGralloc, *mTestRenderEngine, mPrimaryDisplay, mDisplayWidth,
                    mDisplayHeight, PixelFormat::RGBA_8888);
            layer->setDisplayFrame({0, 0, mDisplayWidth, mDisplayHeight});
            layer->setZOrder(10);
            layer->setDataspace(Dataspace::BT2020_PQ, mWriter);
            ASSERT_NO_FATAL_FAILURE(layer->setBuffer(inputColors));

            std::vector<std::shared_ptr<TestLayer>> layers = {layer};

            writeLayers(layers);
            ASSERT_EQ(0, mReader->mErrors.size());

            ExynosHdrStaticInfo s_meta;
            std::vector<V2_3::IComposerClient::PerFrameMetadata> staticMetadata;
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_X, 0.680});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_Y, 0.320});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_X, 0.265});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_Y, 0.690});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_X, 0.150});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_Y, 0.060});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_X, 0.3127});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_Y, 0.3290});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_LUMINANCE, lum_list[i]});
            s_meta.sType1.mMaxDisplayLuminance = (lum_list[i] * 10000);
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MIN_LUMINANCE, 0.1});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_CONTENT_LIGHT_LEVEL, 78.0});
            staticMetadata.push_back(
                    {V2_3::IComposerClient::PerFrameMetadataKey::MAX_FRAME_AVERAGE_LIGHT_LEVEL, 62.0});
            mWriter->setLayerPerFrameMetadata(staticMetadata);
            execute();
            ASSERT_EQ(0, mReader->mErrors.size());

            mWriter->validateDisplay();
            execute();

            if (mReader->mCompositionChanges.size() != 0) {
                clearCommandReaderState();
                GTEST_SUCCEED();
                return;
            }
            ASSERT_EQ(0, mReader->mErrors.size());

            Verify_HDR10(&s_meta);

            //ASSERT_NO_FATAL_FAILURE(readbackBuffer.checkReadbackBuffer(expectedColors));
            //mTestRenderEngine->setRenderLayers(layers);
            //ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->drawLayers());
            //ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->checkColorBuffer(expectedColors));
        //}
    }
}

TEST_P(CS_04_hdrdrvTest, CS_04_04_3_VerifyHwSetting_HDR) {
    for (ColorMode mode : mTestColorModes) {
        std::cout << "---Testing Color Mode " << ReadbackHelper::getColorModeString(mode) << "---"
                  << std::endl;
        mWriter->selectDisplay(mPrimaryDisplay);
        ASSERT_NO_FATAL_FAILURE(
                mComposerClient->setColorMode(mPrimaryDisplay, mode, RenderIntent::COLORIMETRIC));

        float maxLuminance;
        float maxAverageLuminance;
        float minLuminance;
        std::vector<Hdr> HdrCapa;
        ASSERT_NO_FATAL_FAILURE(HdrCapa = mComposerClient->getHdrCapabilities(
        mPrimaryDisplay,
        &maxLuminance, &maxAverageLuminance, &minLuminance));
        ASSERT_TRUE(maxLuminance >= minLuminance);
        if(HdrCapa.size() <= 0) {
            std::cout << "HDR not supported (HdrCapa.size() <= 0)" << std::endl;
            GTEST_SKIP() << "HDR not supported";
            return;
        }
        bool hdr_capable = false;
        for (int i = 0; i < HdrCapa.size(); i++) {
            //std::cout << "HdrCapa[" << i << "] : " << (int)HdrCapa[i] << std::endl;
            if ((int)HdrCapa[i] == 2) {
                hdr_capable = true;
                break;
            }
        }
        ASSERT_TRUE(hdr_capable);
        std::vector<IComposerClient::Color> inputColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, 0, mDisplayWidth, mDisplayHeight / 4}, RED);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 4, mDisplayWidth, mDisplayHeight / 2},
                                       GREEN);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 2, mDisplayWidth, mDisplayHeight},
                                       BLUE);

        int i = 3;
        //for (int i = 0; i < lum_list.size(); i++) {
            std::cout << "luminance[" << i << "] : " << lum_list[i] << std::endl;
            auto layer = std::make_shared<TestBufferLayer>(
                    mComposerClient, mGralloc, *mTestRenderEngine, mPrimaryDisplay, mDisplayWidth,
                    mDisplayHeight, PixelFormat::RGBA_8888);
            layer->setDisplayFrame({0, 0, mDisplayWidth, mDisplayHeight});
            layer->setZOrder(10);
            layer->setDataspace(Dataspace::BT2020_PQ, mWriter);
            ASSERT_NO_FATAL_FAILURE(layer->setBuffer(inputColors));

            std::vector<std::shared_ptr<TestLayer>> layers = {layer};

            writeLayers(layers);
            ASSERT_EQ(0, mReader->mErrors.size());

            ExynosHdrStaticInfo s_meta;
            std::vector<V2_3::IComposerClient::PerFrameMetadata> staticMetadata;
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_X, 0.680});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_Y, 0.320});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_X, 0.265});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_Y, 0.690});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_X, 0.150});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_Y, 0.060});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_X, 0.3127});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_Y, 0.3290});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_LUMINANCE, lum_list[i]});
            s_meta.sType1.mMaxDisplayLuminance = (lum_list[i] * 10000);
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MIN_LUMINANCE, 0.1});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_CONTENT_LIGHT_LEVEL, 78.0});
            staticMetadata.push_back(
                    {V2_3::IComposerClient::PerFrameMetadataKey::MAX_FRAME_AVERAGE_LIGHT_LEVEL, 62.0});
            mWriter->setLayerPerFrameMetadata(staticMetadata);
            execute();
            ASSERT_EQ(0, mReader->mErrors.size());

            mWriter->validateDisplay();
            execute();

            if (mReader->mCompositionChanges.size() != 0) {
                clearCommandReaderState();
                GTEST_SUCCEED();
                return;
            }
            ASSERT_EQ(0, mReader->mErrors.size());

            Verify_HDR10(&s_meta);

            //ASSERT_NO_FATAL_FAILURE(readbackBuffer.checkReadbackBuffer(expectedColors));
            //mTestRenderEngine->setRenderLayers(layers);
            //ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->drawLayers());
            //ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->checkColorBuffer(expectedColors));
        //}
    }
}
#endif

#ifdef __CS_04_05__
TEST_P(CS_04_hdrdrvTest, CS_04_05_0_VerifyHwSetting_HDR10P) {
    for (ColorMode mode : mTestColorModes) {
        std::cout << "---Testing Color Mode " << ReadbackHelper::getColorModeString(mode) << "---"
                  << std::endl;
        mWriter->selectDisplay(mPrimaryDisplay);
        ASSERT_NO_FATAL_FAILURE(
                mComposerClient->setColorMode(mPrimaryDisplay, mode, RenderIntent::COLORIMETRIC));

        float maxLuminance;
        float maxAverageLuminance;
        float minLuminance;
        std::vector<Hdr> HdrCapa;
        ASSERT_NO_FATAL_FAILURE(HdrCapa = mComposerClient->getHdrCapabilities(
        mPrimaryDisplay,
        &maxLuminance, &maxAverageLuminance, &minLuminance));
        ASSERT_TRUE(maxLuminance >= minLuminance);
        if(HdrCapa.size() <= 0) {
            std::cout << "HDR not supported (HdrCapa.size() <= 0)" << std::endl;
            GTEST_SKIP() << "HDR not supported";
            return;
        }
        bool hdr10p_capable = false;
        for (int i = 0; i < HdrCapa.size(); i++) {
            //std::cout << "HdrCapa[" << i << "] : " << (int)HdrCapa[i] << std::endl;
            if ((int)HdrCapa[i] == 4) {
                hdr10p_capable = true;
                break;
            }
        }
        ASSERT_TRUE(hdr10p_capable);
        std::vector<IComposerClient::Color> inputColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, 0, mDisplayWidth, mDisplayHeight / 4}, RED);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 4, mDisplayWidth, mDisplayHeight / 2},
                                       GREEN);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 2, mDisplayWidth, mDisplayHeight},
                                       BLUE);

        int i = 0;
        //for (int i = 0; i < 4; i++) {
        //    {
                ExynosHdrDynamicInfo d_meta;
                hdr10pTV.setDynamicMeta(&d_meta, i);

                auto layer = std::make_shared<TestBufferLayer>(
                        mComposerClient, mGralloc, *mTestRenderEngine, mPrimaryDisplay, mDisplayWidth,
                        mDisplayHeight, PixelFormat::RGBA_8888);
                layer->setDisplayFrame({0, 0, mDisplayWidth, mDisplayHeight});
                layer->setZOrder(10);
                layer->setDataspace(Dataspace::BT2020_PQ, mWriter);
                ASSERT_NO_FATAL_FAILURE(layer->setBuffer(inputColors));

                std::vector<std::shared_ptr<TestLayer>> layers = {layer};

                writeLayers(layers);
                ASSERT_EQ(0, mReader->mErrors.size());

                ExynosHdrStaticInfo s_meta;
                std::vector<V2_3::IComposerClient::PerFrameMetadata> staticMetadata;
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_X, 0.680});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_Y, 0.320});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_X, 0.265});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_Y, 0.690});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_X, 0.150});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_Y, 0.060});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_X, 0.3127});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_Y, 0.3290});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_LUMINANCE, 2200.0});
                s_meta.sType1.mMaxDisplayLuminance = (2200.0 * 10000);
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MIN_LUMINANCE, 0.1});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_CONTENT_LIGHT_LEVEL, 78.0});
                staticMetadata.push_back(
                        {V2_3::IComposerClient::PerFrameMetadataKey::MAX_FRAME_AVERAGE_LIGHT_LEVEL, 62.0});
                mWriter->setLayerPerFrameMetadata(staticMetadata);
                execute();
                ASSERT_EQ(0, mReader->mErrors.size());

                char data[MAX_HDR10PLUS_SIZE];
                int size = Exynos_dynamic_meta_to_itu_t_t35(&d_meta, data);
                std::cout << "meta blob size : " << size << std::endl;
                std::vector<uint8_t> vec;
                vec.resize(size);
                vec.assign(data, data + size);
                std::vector<V2_3::IComposerClient::PerFrameMetadataBlob> dynamicMetadata;
                dynamicMetadata.push_back(
                        {V2_3::IComposerClient::PerFrameMetadataKey::HDR10_PLUS_SEI, vec});

                mWriter->setLayerPerFrameMetadataBlobs(dynamicMetadata);
                execute();

                mWriter->validateDisplay();
                execute();

                if (mReader->mCompositionChanges.size() != 0) {
                    clearCommandReaderState();
                    GTEST_SUCCEED();
                    return;
                }
                ASSERT_EQ(0, mReader->mErrors.size());

                Verify_HDR10P(&s_meta, &d_meta);

                //ASSERT_NO_FATAL_FAILURE(readbackBuffer.checkReadbackBuffer(expectedColors));
                //mTestRenderEngine->setRenderLayers(layers);
                //ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->drawLayers());
                //ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->checkColorBuffer(expectedColors));
        //   }
        //}
    }
}

TEST_P(CS_04_hdrdrvTest, CS_04_05_1_VerifyHwSetting_HDR10P) {
    for (ColorMode mode : mTestColorModes) {
        std::cout << "---Testing Color Mode " << ReadbackHelper::getColorModeString(mode) << "---"
                  << std::endl;
        mWriter->selectDisplay(mPrimaryDisplay);
        ASSERT_NO_FATAL_FAILURE(
                mComposerClient->setColorMode(mPrimaryDisplay, mode, RenderIntent::COLORIMETRIC));

        float maxLuminance;
        float maxAverageLuminance;
        float minLuminance;
        std::vector<Hdr> HdrCapa;
        ASSERT_NO_FATAL_FAILURE(HdrCapa = mComposerClient->getHdrCapabilities(
        mPrimaryDisplay,
        &maxLuminance, &maxAverageLuminance, &minLuminance));
        ASSERT_TRUE(maxLuminance >= minLuminance);
        if(HdrCapa.size() <= 0) {
            std::cout << "HDR not supported (HdrCapa.size() <= 0)" << std::endl;
            GTEST_SKIP() << "HDR not supported";
            return;
        }
        bool hdr10p_capable = false;
        for (int i = 0; i < HdrCapa.size(); i++) {
            //std::cout << "HdrCapa[" << i << "] : " << (int)HdrCapa[i] << std::endl;
            if ((int)HdrCapa[i] == 4) {
                hdr10p_capable = true;
                break;
            }
        }
        ASSERT_TRUE(hdr10p_capable);

        mWriter->selectDisplay(mPrimaryDisplay);
        std::vector<IComposerClient::Color> inputColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, 0, mDisplayWidth, mDisplayHeight / 4}, RED);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 4, mDisplayWidth, mDisplayHeight / 2},
                                       GREEN);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 2, mDisplayWidth, mDisplayHeight},
                                       BLUE);

        int i = 1;
        //for (int i = 0; i < 4; i++) {
        //    {
                ExynosHdrDynamicInfo d_meta;
                hdr10pTV.setDynamicMeta(&d_meta, i);

                auto layer = std::make_shared<TestBufferLayer>(
                        mComposerClient, mGralloc, *mTestRenderEngine, mPrimaryDisplay, mDisplayWidth,
                        mDisplayHeight, PixelFormat::RGBA_8888);
                layer->setDisplayFrame({0, 0, mDisplayWidth, mDisplayHeight});
                layer->setZOrder(10);
                layer->setDataspace(Dataspace::BT2020_PQ, mWriter);
                ASSERT_NO_FATAL_FAILURE(layer->setBuffer(inputColors));

                std::vector<std::shared_ptr<TestLayer>> layers = {layer};

                writeLayers(layers);
                ASSERT_EQ(0, mReader->mErrors.size());

                ExynosHdrStaticInfo s_meta;
                std::vector<V2_3::IComposerClient::PerFrameMetadata> staticMetadata;
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_X, 0.680});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_Y, 0.320});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_X, 0.265});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_Y, 0.690});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_X, 0.150});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_Y, 0.060});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_X, 0.3127});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_Y, 0.3290});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_LUMINANCE, 2200.0});
                s_meta.sType1.mMaxDisplayLuminance = (2200.0 * 10000);
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MIN_LUMINANCE, 0.1});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_CONTENT_LIGHT_LEVEL, 78.0});
                staticMetadata.push_back(
                        {V2_3::IComposerClient::PerFrameMetadataKey::MAX_FRAME_AVERAGE_LIGHT_LEVEL, 62.0});
                mWriter->setLayerPerFrameMetadata(staticMetadata);
                execute();
                ASSERT_EQ(0, mReader->mErrors.size());

                char data[MAX_HDR10PLUS_SIZE];
                int size = Exynos_dynamic_meta_to_itu_t_t35(&d_meta, data);
                std::cout << "meta blob size : " << size << std::endl;
                std::vector<uint8_t> vec;
                vec.resize(size);
                vec.assign(data, data + size);
                std::vector<V2_3::IComposerClient::PerFrameMetadataBlob> dynamicMetadata;
                dynamicMetadata.push_back(
                        {V2_3::IComposerClient::PerFrameMetadataKey::HDR10_PLUS_SEI, vec});

                mWriter->setLayerPerFrameMetadataBlobs(dynamicMetadata);
                execute();

                mWriter->validateDisplay();
                execute();

                if (mReader->mCompositionChanges.size() != 0) {
                    clearCommandReaderState();
                    GTEST_SUCCEED();
                    return;
                }
                ASSERT_EQ(0, mReader->mErrors.size());

                Verify_HDR10P(&s_meta, &d_meta);

                //ASSERT_NO_FATAL_FAILURE(readbackBuffer.checkReadbackBuffer(expectedColors));
                //mTestRenderEngine->setRenderLayers(layers);
                //ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->drawLayers());
                //ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->checkColorBuffer(expectedColors));
        //   }
        //}
    }
}

TEST_P(CS_04_hdrdrvTest, CS_04_05_2_VerifyHwSetting_HDR10P) {
    for (ColorMode mode : mTestColorModes) {
        std::cout << "---Testing Color Mode " << ReadbackHelper::getColorModeString(mode) << "---"
                  << std::endl;
        mWriter->selectDisplay(mPrimaryDisplay);
        ASSERT_NO_FATAL_FAILURE(
                mComposerClient->setColorMode(mPrimaryDisplay, mode, RenderIntent::COLORIMETRIC));

        float maxLuminance;
        float maxAverageLuminance;
        float minLuminance;
        std::vector<Hdr> HdrCapa;
        ASSERT_NO_FATAL_FAILURE(HdrCapa = mComposerClient->getHdrCapabilities(
        mPrimaryDisplay,
        &maxLuminance, &maxAverageLuminance, &minLuminance));
        ASSERT_TRUE(maxLuminance >= minLuminance);
        if(HdrCapa.size() <= 0) {
            std::cout << "HDR not supported (HdrCapa.size() <= 0)" << std::endl;
            GTEST_SKIP() << "HDR not supported";
            return;
        }
        bool hdr10p_capable = false;
        for (int i = 0; i < HdrCapa.size(); i++) {
            //std::cout << "HdrCapa[" << i << "] : " << (int)HdrCapa[i] << std::endl;
            if ((int)HdrCapa[i] == 4) {
                hdr10p_capable = true;
                break;
            }
        }
        ASSERT_TRUE(hdr10p_capable);
        std::vector<IComposerClient::Color> inputColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, 0, mDisplayWidth, mDisplayHeight / 4}, RED);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 4, mDisplayWidth, mDisplayHeight / 2},
                                       GREEN);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 2, mDisplayWidth, mDisplayHeight},
                                       BLUE);

        int i = 2;
        //for (int i = 0; i < 4; i++) {
        //    {
                ExynosHdrDynamicInfo d_meta;
                hdr10pTV.setDynamicMeta(&d_meta, i);

                auto layer = std::make_shared<TestBufferLayer>(
                        mComposerClient, mGralloc, *mTestRenderEngine, mPrimaryDisplay, mDisplayWidth,
                        mDisplayHeight, PixelFormat::RGBA_8888);
                layer->setDisplayFrame({0, 0, mDisplayWidth, mDisplayHeight});
                layer->setZOrder(10);
                layer->setDataspace(Dataspace::BT2020_PQ, mWriter);
                ASSERT_NO_FATAL_FAILURE(layer->setBuffer(inputColors));

                std::vector<std::shared_ptr<TestLayer>> layers = {layer};

                writeLayers(layers);
                ASSERT_EQ(0, mReader->mErrors.size());

                ExynosHdrStaticInfo s_meta;
                std::vector<V2_3::IComposerClient::PerFrameMetadata> staticMetadata;
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_X, 0.680});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_Y, 0.320});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_X, 0.265});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_Y, 0.690});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_X, 0.150});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_Y, 0.060});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_X, 0.3127});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_Y, 0.3290});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_LUMINANCE, 2200.0});
                s_meta.sType1.mMaxDisplayLuminance = (2200.0 * 10000);
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MIN_LUMINANCE, 0.1});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_CONTENT_LIGHT_LEVEL, 78.0});
                staticMetadata.push_back(
                        {V2_3::IComposerClient::PerFrameMetadataKey::MAX_FRAME_AVERAGE_LIGHT_LEVEL, 62.0});
                mWriter->setLayerPerFrameMetadata(staticMetadata);
                execute();
                ASSERT_EQ(0, mReader->mErrors.size());

                char data[MAX_HDR10PLUS_SIZE];
                int size = Exynos_dynamic_meta_to_itu_t_t35(&d_meta, data);
                std::cout << "meta blob size : " << size << std::endl;
                std::vector<uint8_t> vec;
                vec.resize(size);
                vec.assign(data, data + size);
                std::vector<V2_3::IComposerClient::PerFrameMetadataBlob> dynamicMetadata;
                dynamicMetadata.push_back(
                        {V2_3::IComposerClient::PerFrameMetadataKey::HDR10_PLUS_SEI, vec});

                mWriter->setLayerPerFrameMetadataBlobs(dynamicMetadata);
                execute();

                mWriter->validateDisplay();
                execute();

                if (mReader->mCompositionChanges.size() != 0) {
                    clearCommandReaderState();
                    GTEST_SUCCEED();
                    return;
                }
                ASSERT_EQ(0, mReader->mErrors.size());

                Verify_HDR10P(&s_meta, &d_meta);

                //ASSERT_NO_FATAL_FAILURE(readbackBuffer.checkReadbackBuffer(expectedColors));
                //mTestRenderEngine->setRenderLayers(layers);
                //ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->drawLayers());
                //ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->checkColorBuffer(expectedColors));
        //   }
        //}
    }
}

TEST_P(CS_04_hdrdrvTest, CS_04_05_3_VerifyHwSetting_HDR10P) {
    for (ColorMode mode : mTestColorModes) {
        std::cout << "---Testing Color Mode " << ReadbackHelper::getColorModeString(mode) << "---"
                  << std::endl;
        mWriter->selectDisplay(mPrimaryDisplay);
        ASSERT_NO_FATAL_FAILURE(
                mComposerClient->setColorMode(mPrimaryDisplay, mode, RenderIntent::COLORIMETRIC));

        float maxLuminance;
        float maxAverageLuminance;
        float minLuminance;
        std::vector<Hdr> HdrCapa;
        ASSERT_NO_FATAL_FAILURE(HdrCapa = mComposerClient->getHdrCapabilities(
        mPrimaryDisplay,
        &maxLuminance, &maxAverageLuminance, &minLuminance));
        ASSERT_TRUE(maxLuminance >= minLuminance);
        if(HdrCapa.size() <= 0) {
            std::cout << "HDR not supported (HdrCapa.size() <= 0)" << std::endl;
            GTEST_SKIP() << "HDR not supported";
            return;
        }
        bool hdr10p_capable = false;
        for (int i = 0; i < HdrCapa.size(); i++) {
            //std::cout << "HdrCapa[" << i << "] : " << (int)HdrCapa[i] << std::endl;
            if ((int)HdrCapa[i] == 4) {
                hdr10p_capable = true;
                break;
            }
        }
        ASSERT_TRUE(hdr10p_capable);
        std::vector<IComposerClient::Color> inputColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, 0, mDisplayWidth, mDisplayHeight / 4}, RED);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 4, mDisplayWidth, mDisplayHeight / 2},
                                       GREEN);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 2, mDisplayWidth, mDisplayHeight},
                                       BLUE);

        int i = 3;
        //for (int i = 0; i < 4; i++) {
        //    {
                ExynosHdrDynamicInfo d_meta;
                hdr10pTV.setDynamicMeta(&d_meta, i);

                auto layer = std::make_shared<TestBufferLayer>(
                        mComposerClient, mGralloc, *mTestRenderEngine, mPrimaryDisplay, mDisplayWidth,
                        mDisplayHeight, PixelFormat::RGBA_8888);
                layer->setDisplayFrame({0, 0, mDisplayWidth, mDisplayHeight});
                layer->setZOrder(10);
                layer->setDataspace(Dataspace::BT2020_PQ, mWriter);
                ASSERT_NO_FATAL_FAILURE(layer->setBuffer(inputColors));

                std::vector<std::shared_ptr<TestLayer>> layers = {layer};

                writeLayers(layers);
                ASSERT_EQ(0, mReader->mErrors.size());

                ExynosHdrStaticInfo s_meta;
                std::vector<V2_3::IComposerClient::PerFrameMetadata> staticMetadata;
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_X, 0.680});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_Y, 0.320});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_X, 0.265});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_Y, 0.690});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_X, 0.150});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_Y, 0.060});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_X, 0.3127});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_Y, 0.3290});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_LUMINANCE, 2200.0});
                s_meta.sType1.mMaxDisplayLuminance = (2200.0 * 10000);
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MIN_LUMINANCE, 0.1});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_CONTENT_LIGHT_LEVEL, 78.0});
                staticMetadata.push_back(
                        {V2_3::IComposerClient::PerFrameMetadataKey::MAX_FRAME_AVERAGE_LIGHT_LEVEL, 62.0});
                mWriter->setLayerPerFrameMetadata(staticMetadata);
                execute();
                ASSERT_EQ(0, mReader->mErrors.size());

                char data[MAX_HDR10PLUS_SIZE];
                int size = Exynos_dynamic_meta_to_itu_t_t35(&d_meta, data);
                std::cout << "meta blob size : " << size << std::endl;
                std::vector<uint8_t> vec;
                vec.resize(size);
                vec.assign(data, data + size);
                std::vector<V2_3::IComposerClient::PerFrameMetadataBlob> dynamicMetadata;
                dynamicMetadata.push_back(
                        {V2_3::IComposerClient::PerFrameMetadataKey::HDR10_PLUS_SEI, vec});

                mWriter->setLayerPerFrameMetadataBlobs(dynamicMetadata);
                execute();

                mWriter->validateDisplay();
                execute();

                if (mReader->mCompositionChanges.size() != 0) {
                    clearCommandReaderState();
                    GTEST_SUCCEED();
                    return;
                }
                ASSERT_EQ(0, mReader->mErrors.size());

                Verify_HDR10P(&s_meta, &d_meta);

                //ASSERT_NO_FATAL_FAILURE(readbackBuffer.checkReadbackBuffer(expectedColors));
                //mTestRenderEngine->setRenderLayers(layers);
                //ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->drawLayers());
                //ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->checkColorBuffer(expectedColors));
        //   }
        //}
    }
}
#endif

#ifdef __CS_04_06__
TEST_P(CS_04_hdrdrvTest, CS_04_06_0_VerifyBuffer_WCG) {
    for (ColorMode mode : mTestColorModes) {
        std::cout << "---Testing Color Mode " << ReadbackHelper::getColorModeString(mode) << "---" << std::endl;
        mDataspace = colorModeToDataspace(mode);

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

        if (!mHasReadbackBuffer)
            std::cout << "Readback not supported or unsupported pixelFormat/dataspace" << std::endl;

        mWriter->selectDisplay(mPrimaryDisplay);

        ReadbackBuffer_mod readbackBuffer(mPrimaryDisplay, mComposerClient, mGralloc, mDisplayWidth,
                mDisplayHeight, mPixelFormat, mDataspace);
        if (mHasReadbackBuffer)
            ASSERT_NO_FATAL_FAILURE(readbackBuffer.setReadbackBuffer());
        std::vector<IComposerClient::Color> inputColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, 0, mDisplayWidth, mDisplayHeight / 4}, RED);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 4, mDisplayWidth, mDisplayHeight / 2},
                                       GREEN);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 2, mDisplayWidth, mDisplayHeight},
                                       BLUE);
        std::vector<IComposerClient::Color> expectedColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth,
                {0, 0, mDisplayWidth, mDisplayHeight / 4},
                IComposerClient::Color{0xff, 0x00, 0x00, 0xff});
        ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth,
                {0, mDisplayHeight / 4, mDisplayWidth, mDisplayHeight / 2},
                IComposerClient::Color{0x00, 0xff, 0x00, 0xff});
        ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth,
                {0, mDisplayHeight / 2, mDisplayWidth, mDisplayHeight},
                IComposerClient::Color{0x00, 0x00, 0xff, 0xff});

        int i = 0;
        //for (int i = 0; i < ds_list.size(); i++) {
            ASSERT_NO_FATAL_FAILURE(
                    std::cout << "out dataspace[" << i << "] : " << dataSpaceToString(mDataspace) << std::endl);
            ASSERT_NO_FATAL_FAILURE(
                    std::cout << "in dataspace[" << i << "] : " << dataSpaceToString(ds_list[i]) << std::endl);

            auto layer = std::make_shared<TestBufferLayer>(
                    mComposerClient, mGralloc, *mTestRenderEngine, mPrimaryDisplay, mDisplayWidth,
                    mDisplayHeight, PixelFormat::RGBA_8888);
            layer->setDisplayFrame({0, 0, mDisplayWidth, mDisplayHeight});
            layer->setZOrder(10);
            layer->setDataspace(ds_list[i], mWriter);
            ASSERT_NO_FATAL_FAILURE(layer->setBuffer(inputColors));

            std::vector<std::shared_ptr<TestLayer>> layers = {layer};

            writeLayers(layers);
            ASSERT_EQ(0, mReader->mErrors.size());

            mWriter->validateDisplay();
            execute();

            if (mReader->mCompositionChanges.size() != 0) {
                clearCommandReaderState();
                GTEST_SUCCEED();
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
            }
            //ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->checkColorBuffer(expectedColors));
        //}
    }
}

TEST_P(CS_04_hdrdrvTest, CS_04_06_1_VerifyBuffer_WCG) {
    for (ColorMode mode : mTestColorModes) {
        std::cout << "---Testing Color Mode " << ReadbackHelper::getColorModeString(mode) << "---" << std::endl;
        mDataspace = colorModeToDataspace(mode);

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

        if (!mHasReadbackBuffer)
            std::cout << "Readback not supported or unsupported pixelFormat/dataspace" << std::endl;

        mWriter->selectDisplay(mPrimaryDisplay);

        ReadbackBuffer_mod readbackBuffer(mPrimaryDisplay, mComposerClient, mGralloc, mDisplayWidth,
                mDisplayHeight, mPixelFormat, mDataspace);
        if (mHasReadbackBuffer)
            ASSERT_NO_FATAL_FAILURE(readbackBuffer.setReadbackBuffer());
        std::vector<IComposerClient::Color> inputColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, 0, mDisplayWidth, mDisplayHeight},
                                       GREEN);
        std::vector<IComposerClient::Color> expectedColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth,
                {0, 0, mDisplayWidth, mDisplayHeight},
                IComposerClient::Color{0, 0xff, 28, 0xff});

        int i = 1;
        //for (int i = 0; i < ds_list.size(); i++) {
            ASSERT_NO_FATAL_FAILURE(
                    std::cout << "out dataspace[" << i << "] : " << dataSpaceToString(mDataspace) << std::endl);
            ASSERT_NO_FATAL_FAILURE(
                    std::cout << "in dataspace[" << i << "] : " << dataSpaceToString(ds_list[i]) << std::endl);

            auto layer = std::make_shared<TestBufferLayer>(
                    mComposerClient, mGralloc, *mTestRenderEngine, mPrimaryDisplay, mDisplayWidth,
                    mDisplayHeight, PixelFormat::RGBA_8888);
            layer->setDisplayFrame({0, 0, mDisplayWidth, mDisplayHeight});
            layer->setZOrder(10);
            layer->setDataspace(ds_list[i], mWriter);
            ASSERT_NO_FATAL_FAILURE(layer->setBuffer(inputColors));

            std::vector<std::shared_ptr<TestLayer>> layers = {layer};

            writeLayers(layers);
            ASSERT_EQ(0, mReader->mErrors.size());

            mWriter->validateDisplay();
            execute();

            if (mReader->mCompositionChanges.size() != 0) {
                clearCommandReaderState();
                GTEST_SUCCEED();
                return;
            }
            ASSERT_EQ(0, mReader->mErrors.size());

            mWriter->presentDisplay();
            execute();

            ASSERT_EQ(0, mReader->mErrors.size());
            sleep(1);

            if (mHasReadbackBuffer) {
                ASSERT_NO_FATAL_FAILURE(readbackBuffer.checkReadbackBuffer(expectedColors));
                mTestRenderEngine->setRenderLayers(layers);
                ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->drawLayers());
            }
            //ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->checkColorBuffer(expectedColors));
        //}
    }
}

TEST_P(CS_04_hdrdrvTest, CS_04_06_2_VerifyBuffer_WCG) {
    for (ColorMode mode : mTestColorModes) {
        std::cout << "---Testing Color Mode " << ReadbackHelper::getColorModeString(mode) << "---" << std::endl;
        mDataspace = colorModeToDataspace(mode);

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

        if (!mHasReadbackBuffer)
            std::cout << "Readback not supported or unsupported pixelFormat/dataspace" << std::endl;

        mWriter->selectDisplay(mPrimaryDisplay);

        ReadbackBuffer_mod readbackBuffer(mPrimaryDisplay, mComposerClient, mGralloc, mDisplayWidth,
                mDisplayHeight, mPixelFormat, mDataspace);
        if (mHasReadbackBuffer)
            ASSERT_NO_FATAL_FAILURE(readbackBuffer.setReadbackBuffer());
        std::vector<IComposerClient::Color> inputColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, 0, mDisplayWidth, mDisplayHeight / 4}, RED);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 4, mDisplayWidth, mDisplayHeight / 2},
                                       GREEN);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 2, mDisplayWidth, mDisplayHeight},
                                       BLUE);
        std::vector<IComposerClient::Color> expectedColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth,
                {0, 0, mDisplayWidth, mDisplayHeight / 4},
                IComposerClient::Color{0xff, 0x00, 0x00, 0xff});
        ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth,
                {0, mDisplayHeight / 4, mDisplayWidth, mDisplayHeight / 2},
                IComposerClient::Color{0x00, 0xff, 0x00, 0xff});
        ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth,
                {0, mDisplayHeight / 2, mDisplayWidth, mDisplayHeight},
                IComposerClient::Color{0x00, 0x00, 0xff, 0xff});

        int i = 2;
        //for (int i = 0; i < ds_list.size(); i++) {
            ASSERT_NO_FATAL_FAILURE(
                    std::cout << "out dataspace[" << i << "] : " << dataSpaceToString(mDataspace) << std::endl);
            ASSERT_NO_FATAL_FAILURE(
                    std::cout << "in dataspace[" << i << "] : " << dataSpaceToString(ds_list[i]) << std::endl);

            auto layer = std::make_shared<TestBufferLayer>(
                    mComposerClient, mGralloc, *mTestRenderEngine, mPrimaryDisplay, mDisplayWidth,
                    mDisplayHeight, PixelFormat::RGBA_8888);
            layer->setDisplayFrame({0, 0, mDisplayWidth, mDisplayHeight});
            layer->setZOrder(10);
            layer->setDataspace(ds_list[i], mWriter);
            ASSERT_NO_FATAL_FAILURE(layer->setBuffer(inputColors));

            std::vector<std::shared_ptr<TestLayer>> layers = {layer};

            writeLayers(layers);
            ASSERT_EQ(0, mReader->mErrors.size());

            mWriter->validateDisplay();
            execute();

            if (mReader->mCompositionChanges.size() != 0) {
                clearCommandReaderState();
                GTEST_SUCCEED();
                return;
            }
            ASSERT_EQ(0, mReader->mErrors.size());

            mWriter->presentDisplay();
            execute();

            ASSERT_EQ(0, mReader->mErrors.size());
            sleep(1);

            if (mHasReadbackBuffer) {
                ASSERT_NO_FATAL_FAILURE(readbackBuffer.checkReadbackBuffer(expectedColors));
                mTestRenderEngine->setRenderLayers(layers);
                ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->drawLayers());
            }
            //ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->checkColorBuffer(expectedColors));
        //}
    }
}
TEST_P(CS_04_hdrdrvTest, CS_04_06_3_VerifyBuffer_WCG) {
    for (ColorMode mode : mTestColorModes) {
        std::cout << "---Testing Color Mode " << ReadbackHelper::getColorModeString(mode) << "---" << std::endl;
        mDataspace = colorModeToDataspace(mode);

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

        if (!mHasReadbackBuffer)
            std::cout << "Readback not supported or unsupported pixelFormat/dataspace" << std::endl;

        mWriter->selectDisplay(mPrimaryDisplay);

        ReadbackBuffer_mod readbackBuffer(mPrimaryDisplay, mComposerClient, mGralloc, mDisplayWidth,
                mDisplayHeight, mPixelFormat, mDataspace);
        if (mHasReadbackBuffer)
            ASSERT_NO_FATAL_FAILURE(readbackBuffer.setReadbackBuffer());
        std::vector<IComposerClient::Color> inputColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, 0, mDisplayWidth, mDisplayHeight},
                                       GREEN);
        std::vector<IComposerClient::Color> expectedColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth,
                {0, 0, mDisplayWidth, mDisplayHeight},
                IComposerClient::Color{0, 0xff, 28, 0xff});

        int i = 3;
        //for (int i = 0; i < ds_list.size(); i++) {
            ASSERT_NO_FATAL_FAILURE(
                    std::cout << "out dataspace[" << i << "] : " << dataSpaceToString(mDataspace) << std::endl);
            ASSERT_NO_FATAL_FAILURE(
                    std::cout << "in dataspace[" << i << "] : " << dataSpaceToString(ds_list[i]) << std::endl);

            auto layer = std::make_shared<TestBufferLayer>(
                    mComposerClient, mGralloc, *mTestRenderEngine, mPrimaryDisplay, mDisplayWidth,
                    mDisplayHeight, PixelFormat::RGBA_8888);
            layer->setDisplayFrame({0, 0, mDisplayWidth, mDisplayHeight});
            layer->setZOrder(10);
            layer->setDataspace(ds_list[i], mWriter);
            ASSERT_NO_FATAL_FAILURE(layer->setBuffer(inputColors));

            std::vector<std::shared_ptr<TestLayer>> layers = {layer};

            writeLayers(layers);
            ASSERT_EQ(0, mReader->mErrors.size());

            mWriter->validateDisplay();
            execute();

            if (mReader->mCompositionChanges.size() != 0) {
                clearCommandReaderState();
                GTEST_SUCCEED();
                return;
            }
            ASSERT_EQ(0, mReader->mErrors.size());

            mWriter->presentDisplay();
            execute();

            ASSERT_EQ(0, mReader->mErrors.size());
            sleep(1);

            if (mHasReadbackBuffer) {
                ASSERT_NO_FATAL_FAILURE(readbackBuffer.checkReadbackBuffer(expectedColors));
                mTestRenderEngine->setRenderLayers(layers);
                ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->drawLayers());
            }

            //ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->checkColorBuffer(expectedColors));
        //}
    }
}
#endif

#ifdef __CS_04_07__
TEST_P(CS_04_hdrdrvTest, CS_04_07_0_VerifyBuffer_HDR10) {
    for (ColorMode mode : mTestColorModes) {
        std::cout << "---Testing Color Mode " << ReadbackHelper::getColorModeString(mode) << "---"
                  << std::endl;
        mDataspace = colorModeToDataspace(mode);
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

        if (!mHasReadbackBuffer)
            std::cout << "Readback not supported or unsupported pixelFormat/dataspace" << std::endl;

        float maxLuminance;
        float maxAverageLuminance;
        float minLuminance;
        std::vector<Hdr> HdrCapa;
        ASSERT_NO_FATAL_FAILURE(HdrCapa = mComposerClient->getHdrCapabilities(
        mPrimaryDisplay,
        &maxLuminance, &maxAverageLuminance, &minLuminance));
        ASSERT_TRUE(maxLuminance >= minLuminance);
        if(HdrCapa.size() <= 0) {
            std::cout << "HDR not supported (HdrCapa.size() <= 0)" << std::endl;
            GTEST_SKIP() << "HDR not supported";
            return;
        }
        bool hdr_capable = false;
        for (int i = 0; i < HdrCapa.size(); i++) {
            //std::cout << "HdrCapa[" << i << "] : " << (int)HdrCapa[i] << std::endl;
            if ((int)HdrCapa[i] == 2) {
                hdr_capable = true;
                break;
            }
        }
        ASSERT_TRUE(hdr_capable);

        mWriter->selectDisplay(mPrimaryDisplay);

        ReadbackBuffer readbackBuffer(mPrimaryDisplay, mComposerClient, mGralloc, mDisplayWidth,
                                      mDisplayHeight, mPixelFormat, mDataspace);
        if (mHasReadbackBuffer)
            ASSERT_NO_FATAL_FAILURE(readbackBuffer.setReadbackBuffer());
        std::vector<IComposerClient::Color> inputColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, 0, mDisplayWidth, mDisplayHeight / 4}, RED);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 4, mDisplayWidth, mDisplayHeight / 2},
                                       GREEN);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 2, mDisplayWidth, mDisplayHeight},
                                       BLUE);
        std::vector<IComposerClient::Color> expectedColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth,
                                       {0, 0, mDisplayWidth, mDisplayHeight / 4}, RED);
        ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth,
                                       {0, mDisplayHeight / 4, mDisplayWidth, mDisplayHeight / 2},
                                       GREEN);
        ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth,
                                       {0, mDisplayHeight / 2, mDisplayWidth, mDisplayHeight},
                                       BLUE);

        int i = 0;
        //for (int i = 0; i < lum_list.size(); i++) {
            std::cout << "luminance[" << i << "] : " << lum_list[i] << std::endl;
            auto layer = std::make_shared<TestBufferLayer>(
                    mComposerClient, mGralloc, *mTestRenderEngine, mPrimaryDisplay, mDisplayWidth,
                    mDisplayHeight, PixelFormat::RGBA_8888);
            layer->setDisplayFrame({0, 0, mDisplayWidth, mDisplayHeight});
            layer->setZOrder(10);
            layer->setDataspace(Dataspace::BT2020_PQ, mWriter);
            ASSERT_NO_FATAL_FAILURE(layer->setBuffer(inputColors));

            std::vector<std::shared_ptr<TestLayer>> layers = {layer};

            writeLayers(layers);
            ASSERT_EQ(0, mReader->mErrors.size());

            std::vector<V2_3::IComposerClient::PerFrameMetadata> staticMetadata;
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_X, 0.680});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_Y, 0.320});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_X, 0.265});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_Y, 0.690});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_X, 0.150});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_Y, 0.060});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_X, 0.3127});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_Y, 0.3290});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_LUMINANCE, lum_list[i]});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MIN_LUMINANCE, 0.1});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_CONTENT_LIGHT_LEVEL, 78.0});
            staticMetadata.push_back(
                    {V2_3::IComposerClient::PerFrameMetadataKey::MAX_FRAME_AVERAGE_LIGHT_LEVEL, 62.0});
            mWriter->setLayerPerFrameMetadata(staticMetadata);
            execute();
            ASSERT_EQ(0, mReader->mErrors.size());

            mWriter->validateDisplay();
            execute();

            if (mReader->mCompositionChanges.size() != 0) {
                clearCommandReaderState();
                GTEST_SUCCEED();
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
            }
        //}
    }
}

TEST_P(CS_04_hdrdrvTest, CS_04_07_1_VerifyBuffer_HDR10) {
    for (ColorMode mode : mTestColorModes) {
        std::cout << "---Testing Color Mode " << ReadbackHelper::getColorModeString(mode) << "---"
                  << std::endl;
        mDataspace = colorModeToDataspace(mode);
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

        if (!mHasReadbackBuffer)
            std::cout << "Readback not supported or unsupported pixelFormat/dataspace" << std::endl;

        float maxLuminance;
        float maxAverageLuminance;
        float minLuminance;
        std::vector<Hdr> HdrCapa;
        ASSERT_NO_FATAL_FAILURE(HdrCapa = mComposerClient->getHdrCapabilities(
        mPrimaryDisplay,
        &maxLuminance, &maxAverageLuminance, &minLuminance));
        ASSERT_TRUE(maxLuminance >= minLuminance);
        if(HdrCapa.size() <= 0) {
            std::cout << "HDR not supported (HdrCapa.size() <= 0)" << std::endl;
            GTEST_SKIP() << "HDR not supported";
            return;
        }
        bool hdr_capable = false;
        for (int i = 0; i < HdrCapa.size(); i++) {
            //std::cout << "HdrCapa[" << i << "] : " << (int)HdrCapa[i] << std::endl;
            if ((int)HdrCapa[i] == 2) {
                hdr_capable = true;
                break;
            }
        }
        ASSERT_TRUE(hdr_capable);

        mWriter->selectDisplay(mPrimaryDisplay);

        ReadbackBuffer readbackBuffer(mPrimaryDisplay, mComposerClient, mGralloc, mDisplayWidth,
                                      mDisplayHeight, mPixelFormat, mDataspace);
        if (mHasReadbackBuffer)
            ASSERT_NO_FATAL_FAILURE(readbackBuffer.setReadbackBuffer());
        std::vector<IComposerClient::Color> inputColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, 0, mDisplayWidth, mDisplayHeight / 4}, RED);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 4, mDisplayWidth, mDisplayHeight / 2},
                                       GREEN);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 2, mDisplayWidth, mDisplayHeight},
                                       BLUE);
        std::vector<IComposerClient::Color> expectedColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth,
                                       {0, 0, mDisplayWidth, mDisplayHeight / 4}, RED);
        ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth,
                                       {0, mDisplayHeight / 4, mDisplayWidth, mDisplayHeight / 2},
                                       GREEN);
        ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth,
                                       {0, mDisplayHeight / 2, mDisplayWidth, mDisplayHeight},
                                       BLUE);

        int i = 1;
        //for (int i = 0; i < lum_list.size(); i++) {
            std::cout << "luminance[" << i << "] : " << lum_list[i] << std::endl;
            auto layer = std::make_shared<TestBufferLayer>(
                    mComposerClient, mGralloc, *mTestRenderEngine, mPrimaryDisplay, mDisplayWidth,
                    mDisplayHeight, PixelFormat::RGBA_8888);
            layer->setDisplayFrame({0, 0, mDisplayWidth, mDisplayHeight});
            layer->setZOrder(10);
            layer->setDataspace(Dataspace::BT2020_PQ, mWriter);
            ASSERT_NO_FATAL_FAILURE(layer->setBuffer(inputColors));

            std::vector<std::shared_ptr<TestLayer>> layers = {layer};

            writeLayers(layers);
            ASSERT_EQ(0, mReader->mErrors.size());

            std::vector<V2_3::IComposerClient::PerFrameMetadata> staticMetadata;
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_X, 0.680});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_Y, 0.320});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_X, 0.265});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_Y, 0.690});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_X, 0.150});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_Y, 0.060});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_X, 0.3127});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_Y, 0.3290});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_LUMINANCE, lum_list[i]});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MIN_LUMINANCE, 0.1});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_CONTENT_LIGHT_LEVEL, 78.0});
            staticMetadata.push_back(
                    {V2_3::IComposerClient::PerFrameMetadataKey::MAX_FRAME_AVERAGE_LIGHT_LEVEL, 62.0});
            mWriter->setLayerPerFrameMetadata(staticMetadata);
            execute();
            ASSERT_EQ(0, mReader->mErrors.size());

            mWriter->validateDisplay();
            execute();

            if (mReader->mCompositionChanges.size() != 0) {
                clearCommandReaderState();
                GTEST_SUCCEED();
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
            }
        //}
    }
}

TEST_P(CS_04_hdrdrvTest, CS_04_07_2_VerifyBuffer_HDR10) {
    for (ColorMode mode : mTestColorModes) {
        std::cout << "---Testing Color Mode " << ReadbackHelper::getColorModeString(mode) << "---"
                  << std::endl;
        mDataspace = colorModeToDataspace(mode);
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

        if (!mHasReadbackBuffer)
            std::cout << "Readback not supported or unsupported pixelFormat/dataspace" << std::endl;

        float maxLuminance;
        float maxAverageLuminance;
        float minLuminance;
        std::vector<Hdr> HdrCapa;
        ASSERT_NO_FATAL_FAILURE(HdrCapa = mComposerClient->getHdrCapabilities(
        mPrimaryDisplay,
        &maxLuminance, &maxAverageLuminance, &minLuminance));
        ASSERT_TRUE(maxLuminance >= minLuminance);
        if(HdrCapa.size() <= 0) {
            std::cout << "HDR not supported (HdrCapa.size() <= 0)" << std::endl;
            GTEST_SKIP() << "HDR not supported";
            return;
        }
        bool hdr_capable = false;
        for (int i = 0; i < HdrCapa.size(); i++) {
            //std::cout << "HdrCapa[" << i << "] : " << (int)HdrCapa[i] << std::endl;
            if ((int)HdrCapa[i] == 2) {
                hdr_capable = true;
                break;
            }
        }
        ASSERT_TRUE(hdr_capable);

        mWriter->selectDisplay(mPrimaryDisplay);

        ReadbackBuffer readbackBuffer(mPrimaryDisplay, mComposerClient, mGralloc, mDisplayWidth,
                                      mDisplayHeight, mPixelFormat, mDataspace);
        if (mHasReadbackBuffer)
            ASSERT_NO_FATAL_FAILURE(readbackBuffer.setReadbackBuffer());
        std::vector<IComposerClient::Color> inputColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, 0, mDisplayWidth, mDisplayHeight / 4}, RED);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 4, mDisplayWidth, mDisplayHeight / 2},
                                       GREEN);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 2, mDisplayWidth, mDisplayHeight},
                                       BLUE);
        std::vector<IComposerClient::Color> expectedColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth,
                                       {0, 0, mDisplayWidth, mDisplayHeight / 4}, RED);
        ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth,
                                       {0, mDisplayHeight / 4, mDisplayWidth, mDisplayHeight / 2},
                                       GREEN);
        ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth,
                                       {0, mDisplayHeight / 2, mDisplayWidth, mDisplayHeight},
                                       BLUE);

        int i = 2;
        //for (int i = 0; i < lum_list.size(); i++) {
            std::cout << "luminance[" << i << "] : " << lum_list[i] << std::endl;
            auto layer = std::make_shared<TestBufferLayer>(
                    mComposerClient, mGralloc, *mTestRenderEngine, mPrimaryDisplay, mDisplayWidth,
                    mDisplayHeight, PixelFormat::RGBA_8888);
            layer->setDisplayFrame({0, 0, mDisplayWidth, mDisplayHeight});
            layer->setZOrder(10);
            layer->setDataspace(Dataspace::BT2020_PQ, mWriter);
            ASSERT_NO_FATAL_FAILURE(layer->setBuffer(inputColors));

            std::vector<std::shared_ptr<TestLayer>> layers = {layer};

            writeLayers(layers);
            ASSERT_EQ(0, mReader->mErrors.size());

            std::vector<V2_3::IComposerClient::PerFrameMetadata> staticMetadata;
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_X, 0.680});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_Y, 0.320});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_X, 0.265});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_Y, 0.690});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_X, 0.150});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_Y, 0.060});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_X, 0.3127});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_Y, 0.3290});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_LUMINANCE, lum_list[i]});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MIN_LUMINANCE, 0.1});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_CONTENT_LIGHT_LEVEL, 78.0});
            staticMetadata.push_back(
                    {V2_3::IComposerClient::PerFrameMetadataKey::MAX_FRAME_AVERAGE_LIGHT_LEVEL, 62.0});
            mWriter->setLayerPerFrameMetadata(staticMetadata);
            execute();
            ASSERT_EQ(0, mReader->mErrors.size());

            mWriter->validateDisplay();
            execute();

            if (mReader->mCompositionChanges.size() != 0) {
                clearCommandReaderState();
                GTEST_SUCCEED();
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
            }
        //}
    }
}

TEST_P(CS_04_hdrdrvTest, CS_04_07_3_VerifyBuffer_HDR10) {
    for (ColorMode mode : mTestColorModes) {
        std::cout << "---Testing Color Mode " << ReadbackHelper::getColorModeString(mode) << "---"
                  << std::endl;
        mDataspace = colorModeToDataspace(mode);
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

        if (!mHasReadbackBuffer)
            std::cout << "Readback not supported or unsupported pixelFormat/dataspace" << std::endl;

        float maxLuminance;
        float maxAverageLuminance;
        float minLuminance;
        std::vector<Hdr> HdrCapa;
        ASSERT_NO_FATAL_FAILURE(HdrCapa = mComposerClient->getHdrCapabilities(
        mPrimaryDisplay,
        &maxLuminance, &maxAverageLuminance, &minLuminance));
        ASSERT_TRUE(maxLuminance >= minLuminance);
        if(HdrCapa.size() <= 0) {
            std::cout << "HDR not supported (HdrCapa.size() <= 0)" << std::endl;
            GTEST_SKIP() << "HDR not supported";
            return;
        }
        bool hdr_capable = false;
        for (int i = 0; i < HdrCapa.size(); i++) {
            //std::cout << "HdrCapa[" << i << "] : " << (int)HdrCapa[i] << std::endl;
            if ((int)HdrCapa[i] == 2) {
                hdr_capable = true;
                break;
            }
        }
        ASSERT_TRUE(hdr_capable);

        mWriter->selectDisplay(mPrimaryDisplay);

        ReadbackBuffer readbackBuffer(mPrimaryDisplay, mComposerClient, mGralloc, mDisplayWidth,
                                      mDisplayHeight, mPixelFormat, mDataspace);
        if (mHasReadbackBuffer)
            ASSERT_NO_FATAL_FAILURE(readbackBuffer.setReadbackBuffer());
        std::vector<IComposerClient::Color> inputColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, 0, mDisplayWidth, mDisplayHeight / 4}, RED);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 4, mDisplayWidth, mDisplayHeight / 2},
                                       GREEN);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 2, mDisplayWidth, mDisplayHeight},
                                       BLUE);
        std::vector<IComposerClient::Color> expectedColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth,
                                       {0, 0, mDisplayWidth, mDisplayHeight / 4}, RED);
        ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth,
                                       {0, mDisplayHeight / 4, mDisplayWidth, mDisplayHeight / 2},
                                       GREEN);
        ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth,
                                       {0, mDisplayHeight / 2, mDisplayWidth, mDisplayHeight},
                                       BLUE);

        int i = 3;
        //for (int i = 0; i < lum_list.size(); i++) {
            std::cout << "luminance[" << i << "] : " << lum_list[i] << std::endl;
            auto layer = std::make_shared<TestBufferLayer>(
                    mComposerClient, mGralloc, *mTestRenderEngine, mPrimaryDisplay, mDisplayWidth,
                    mDisplayHeight, PixelFormat::RGBA_8888);
            layer->setDisplayFrame({0, 0, mDisplayWidth, mDisplayHeight});
            layer->setZOrder(10);
            layer->setDataspace(Dataspace::BT2020_PQ, mWriter);
            ASSERT_NO_FATAL_FAILURE(layer->setBuffer(inputColors));

            std::vector<std::shared_ptr<TestLayer>> layers = {layer};

            writeLayers(layers);
            ASSERT_EQ(0, mReader->mErrors.size());

            std::vector<V2_3::IComposerClient::PerFrameMetadata> staticMetadata;
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_X, 0.680});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_Y, 0.320});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_X, 0.265});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_Y, 0.690});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_X, 0.150});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_Y, 0.060});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_X, 0.3127});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_Y, 0.3290});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_LUMINANCE, lum_list[i]});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MIN_LUMINANCE, 0.1});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_CONTENT_LIGHT_LEVEL, 78.0});
            staticMetadata.push_back(
                    {V2_3::IComposerClient::PerFrameMetadataKey::MAX_FRAME_AVERAGE_LIGHT_LEVEL, 62.0});
            mWriter->setLayerPerFrameMetadata(staticMetadata);
            execute();
            ASSERT_EQ(0, mReader->mErrors.size());

            mWriter->validateDisplay();
            execute();

            if (mReader->mCompositionChanges.size() != 0) {
                clearCommandReaderState();
                GTEST_SUCCEED();
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
            }
        //}
    }
}

TEST_P(CS_04_hdrdrvTest, CS_04_07_4_VerifyBuffer_HDR10) {
    for (ColorMode mode : mTestColorModes) {
        std::cout << "---Testing Color Mode " << ReadbackHelper::getColorModeString(mode) << "---"
                  << std::endl;
        mDataspace = colorModeToDataspace(mode);
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

        if (!mHasReadbackBuffer)
            std::cout << "Readback not supported or unsupported pixelFormat/dataspace" << std::endl;

        float maxLuminance;
        float maxAverageLuminance;
        float minLuminance;
        std::vector<Hdr> HdrCapa;
        ASSERT_NO_FATAL_FAILURE(HdrCapa = mComposerClient->getHdrCapabilities(
        mPrimaryDisplay,
        &maxLuminance, &maxAverageLuminance, &minLuminance));
        ASSERT_TRUE(maxLuminance >= minLuminance);
        if(HdrCapa.size() <= 0) {
            std::cout << "HDR not supported (HdrCapa.size() <= 0)" << std::endl;
            GTEST_SKIP() << "HDR not supported";
            return;
        }
        bool hdr_capable = false;
        for (int i = 0; i < HdrCapa.size(); i++) {
            //std::cout << "HdrCapa[" << i << "] : " << (int)HdrCapa[i] << std::endl;
            if ((int)HdrCapa[i] == 2) {
                hdr_capable = true;
                break;
            }
        }
        ASSERT_TRUE(hdr_capable);

        mWriter->selectDisplay(mPrimaryDisplay);

        ReadbackBuffer readbackBuffer(mPrimaryDisplay, mComposerClient, mGralloc, mDisplayWidth,
                                      mDisplayHeight, mPixelFormat, mDataspace);
        if (mHasReadbackBuffer)
            ASSERT_NO_FATAL_FAILURE(readbackBuffer.setReadbackBuffer());
        std::vector<IComposerClient::Color> inputColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, 0, mDisplayWidth, mDisplayHeight / 4}, RED);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 4, mDisplayWidth, mDisplayHeight / 2},
                                       GREEN);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 2, mDisplayWidth, mDisplayHeight},
                                       BLUE);
        std::vector<IComposerClient::Color> expectedColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth,
                                       {0, 0, mDisplayWidth, mDisplayHeight / 4}, RED);
        ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth,
                                       {0, mDisplayHeight / 4, mDisplayWidth, mDisplayHeight / 2},
                                       GREEN);
        ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth,
                                       {0, mDisplayHeight / 2, mDisplayWidth, mDisplayHeight},
                                       BLUE);

        int i = 4;
        //for (int i = 0; i < lum_list.size(); i++) {
            std::cout << "luminance[" << i << "] : " << lum_list[i] << std::endl;
            auto layer = std::make_shared<TestBufferLayer>(
                    mComposerClient, mGralloc, *mTestRenderEngine, mPrimaryDisplay, mDisplayWidth,
                    mDisplayHeight, PixelFormat::RGBA_8888);
            layer->setDisplayFrame({0, 0, mDisplayWidth, mDisplayHeight});
            layer->setZOrder(10);
            layer->setDataspace(Dataspace::BT2020_PQ, mWriter);
            ASSERT_NO_FATAL_FAILURE(layer->setBuffer(inputColors));

            std::vector<std::shared_ptr<TestLayer>> layers = {layer};

            writeLayers(layers);
            ASSERT_EQ(0, mReader->mErrors.size());

            std::vector<V2_3::IComposerClient::PerFrameMetadata> staticMetadata;
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_X, 0.680});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_Y, 0.320});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_X, 0.265});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_Y, 0.690});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_X, 0.150});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_Y, 0.060});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_X, 0.3127});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_Y, 0.3290});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_LUMINANCE, lum_list[i]});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MIN_LUMINANCE, 0.1});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_CONTENT_LIGHT_LEVEL, 78.0});
            staticMetadata.push_back(
                    {V2_3::IComposerClient::PerFrameMetadataKey::MAX_FRAME_AVERAGE_LIGHT_LEVEL, 62.0});
            mWriter->setLayerPerFrameMetadata(staticMetadata);
            execute();
            ASSERT_EQ(0, mReader->mErrors.size());

            mWriter->validateDisplay();
            execute();

            if (mReader->mCompositionChanges.size() != 0) {
                clearCommandReaderState();
                GTEST_SUCCEED();
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
            }
        //}
    }
}
#endif

#ifdef __CS_04_08__
TEST_P(CS_04_hdrdrvTest, CS_04_08_0_VerifyBuffer_HDR10P) {
    for (ColorMode mode : mTestColorModes) {
        std::cout << "---Testing Color Mode " << ReadbackHelper::getColorModeString(mode) << "---"
                  << std::endl;
        mDataspace = colorModeToDataspace(mode);
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

        if (!mHasReadbackBuffer)
            std::cout << "Readback not supported or unsupported pixelFormat/dataspace" << std::endl;

        float maxLuminance;
        float maxAverageLuminance;
        float minLuminance;
        std::vector<Hdr> HdrCapa;
        ASSERT_NO_FATAL_FAILURE(HdrCapa = mComposerClient->getHdrCapabilities(
        mPrimaryDisplay,
        &maxLuminance, &maxAverageLuminance, &minLuminance));
        ASSERT_TRUE(maxLuminance >= minLuminance);
        if(HdrCapa.size() <= 0) {
            std::cout << "HDR not supported (HdrCapa.size() <= 0)" << std::endl;
            GTEST_SKIP() << "HDR not supported";
            return;
        }
        bool hdr10p_capable = false;
        for (int i = 0; i < HdrCapa.size(); i++) {
            //std::cout << "HdrCapa[" << i << "] : " << (int)HdrCapa[i] << std::endl;
            if ((int)HdrCapa[i] == 4) {
                hdr10p_capable = true;
                break;
            }
        }
        ASSERT_TRUE(hdr10p_capable);

        mWriter->selectDisplay(mPrimaryDisplay);

        ReadbackBuffer readbackBuffer(mPrimaryDisplay, mComposerClient, mGralloc, mDisplayWidth,
                                      mDisplayHeight, mPixelFormat, mDataspace);
        if (mHasReadbackBuffer)
            ASSERT_NO_FATAL_FAILURE(readbackBuffer.setReadbackBuffer());
        std::vector<IComposerClient::Color> inputColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, 0, mDisplayWidth, mDisplayHeight / 4}, RED);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 4, mDisplayWidth, mDisplayHeight / 2},
                                       GREEN);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 2, mDisplayWidth, mDisplayHeight},
                                       BLUE);
        std::vector<IComposerClient::Color> expectedColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth,
                                       {0, 0, mDisplayWidth, mDisplayHeight / 4}, RED);
        ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth,
                                       {0, mDisplayHeight / 4, mDisplayWidth, mDisplayHeight / 2},
                                       GREEN);
        ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth,
                                       {0, mDisplayHeight / 2, mDisplayWidth, mDisplayHeight},
                                       BLUE);

        int i = 0;
        //for (int i = 0; i < 4; i++) {
        //    {
                ExynosHdrDynamicInfo d_meta;
                hdr10pTV.setDynamicMeta(&d_meta, i);
                auto dyn_lum = hdr10pTV.getSrcMaxLuminance(&d_meta);
                std::cout << "dyn_lum : " << dyn_lum << std::endl;

                auto layer = std::make_shared<TestBufferLayer>(
                        mComposerClient, mGralloc, *mTestRenderEngine, mPrimaryDisplay, mDisplayWidth,
                        mDisplayHeight, PixelFormat::RGBA_8888);
                layer->setDisplayFrame({0, 0, mDisplayWidth, mDisplayHeight});
                layer->setZOrder(10);
                layer->setDataspace(Dataspace::BT2020_PQ, mWriter);
                ASSERT_NO_FATAL_FAILURE(layer->setBuffer(inputColors));

                std::vector<std::shared_ptr<TestLayer>> layers = {layer};

                writeLayers(layers);
                ASSERT_EQ(0, mReader->mErrors.size());

                std::vector<V2_3::IComposerClient::PerFrameMetadata> staticMetadata;
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_X, 0.680});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_Y, 0.320});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_X, 0.265});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_Y, 0.690});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_X, 0.150});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_Y, 0.060});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_X, 0.3127});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_Y, 0.3290});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_LUMINANCE, 100.0});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MIN_LUMINANCE, 0.1});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_CONTENT_LIGHT_LEVEL, 78.0});
                staticMetadata.push_back(
                        {V2_3::IComposerClient::PerFrameMetadataKey::MAX_FRAME_AVERAGE_LIGHT_LEVEL, 62.0});
                mWriter->setLayerPerFrameMetadata(staticMetadata);
                execute();
                ASSERT_EQ(0, mReader->mErrors.size());

                char data[MAX_HDR10PLUS_SIZE];
                int size = Exynos_dynamic_meta_to_itu_t_t35(&d_meta, data);
                std::cout << "meta blob size : " << size << std::endl;
                std::vector<uint8_t> vec;
                vec.resize(size);
                vec.assign(data, data + size);
                std::vector<V2_3::IComposerClient::PerFrameMetadataBlob> dynamicMetadata;
                dynamicMetadata.push_back(
                        {V2_3::IComposerClient::PerFrameMetadataKey::HDR10_PLUS_SEI, vec});

                mWriter->setLayerPerFrameMetadataBlobs(dynamicMetadata);
                execute();

                mWriter->validateDisplay();
                execute();

                if (mReader->mCompositionChanges.size() != 0) {
                    clearCommandReaderState();
                    GTEST_SUCCEED();
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
                }
        //   }
        //}
    }
}

TEST_P(CS_04_hdrdrvTest, CS_04_08_1_VerifyBuffer_HDR10P) {
    for (ColorMode mode : mTestColorModes) {
        std::cout << "---Testing Color Mode " << ReadbackHelper::getColorModeString(mode) << "---"
                  << std::endl;
        mDataspace = colorModeToDataspace(mode);
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

        if (!mHasReadbackBuffer)
            std::cout << "Readback not supported or unsupported pixelFormat/dataspace" << std::endl;

        float maxLuminance;
        float maxAverageLuminance;
        float minLuminance;
        std::vector<Hdr> HdrCapa;
        ASSERT_NO_FATAL_FAILURE(HdrCapa = mComposerClient->getHdrCapabilities(
        mPrimaryDisplay,
        &maxLuminance, &maxAverageLuminance, &minLuminance));
        ASSERT_TRUE(maxLuminance >= minLuminance);
        if(HdrCapa.size() <= 0) {
            std::cout << "HDR not supported (HdrCapa.size() <= 0)" << std::endl;
            GTEST_SKIP() << "HDR not supported";
            return;
        }
        bool hdr10p_capable = false;
        for (int i = 0; i < HdrCapa.size(); i++) {
            //std::cout << "HdrCapa[" << i << "] : " << (int)HdrCapa[i] << std::endl;
            if ((int)HdrCapa[i] == 4) {
                hdr10p_capable = true;
                break;
            }
        }
        ASSERT_TRUE(hdr10p_capable);

        mWriter->selectDisplay(mPrimaryDisplay);

        ReadbackBuffer readbackBuffer(mPrimaryDisplay, mComposerClient, mGralloc, mDisplayWidth,
                                      mDisplayHeight, mPixelFormat, mDataspace);
        if (mHasReadbackBuffer)
            ASSERT_NO_FATAL_FAILURE(readbackBuffer.setReadbackBuffer());
        std::vector<IComposerClient::Color> inputColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, 0, mDisplayWidth, mDisplayHeight / 4}, RED);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 4, mDisplayWidth, mDisplayHeight / 2},
                                       GREEN);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 2, mDisplayWidth, mDisplayHeight},
                                       BLUE);
        std::vector<IComposerClient::Color> expectedColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth,
                                       {0, 0, mDisplayWidth, mDisplayHeight / 4}, RED);
        ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth,
                                       {0, mDisplayHeight / 4, mDisplayWidth, mDisplayHeight / 2},
                                       GREEN);
        ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth,
                                       {0, mDisplayHeight / 2, mDisplayWidth, mDisplayHeight},
                                       BLUE);

        int i = 1;
        //for (int i = 0; i < 4; i++) {
        //    {
                ExynosHdrDynamicInfo d_meta;
                hdr10pTV.setDynamicMeta(&d_meta, i);
                auto dyn_lum = hdr10pTV.getSrcMaxLuminance(&d_meta);
                std::cout << "dyn_lum : " << dyn_lum << std::endl;

                auto layer = std::make_shared<TestBufferLayer>(
                        mComposerClient, mGralloc, *mTestRenderEngine, mPrimaryDisplay, mDisplayWidth,
                        mDisplayHeight, PixelFormat::RGBA_8888);
                layer->setDisplayFrame({0, 0, mDisplayWidth, mDisplayHeight});
                layer->setZOrder(10);
                layer->setDataspace(Dataspace::BT2020_PQ, mWriter);
                ASSERT_NO_FATAL_FAILURE(layer->setBuffer(inputColors));

                std::vector<std::shared_ptr<TestLayer>> layers = {layer};

                writeLayers(layers);
                ASSERT_EQ(0, mReader->mErrors.size());

                std::vector<V2_3::IComposerClient::PerFrameMetadata> staticMetadata;
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_X, 0.680});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_Y, 0.320});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_X, 0.265});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_Y, 0.690});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_X, 0.150});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_Y, 0.060});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_X, 0.3127});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_Y, 0.3290});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_LUMINANCE, 100.0});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MIN_LUMINANCE, 0.1});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_CONTENT_LIGHT_LEVEL, 78.0});
                staticMetadata.push_back(
                        {V2_3::IComposerClient::PerFrameMetadataKey::MAX_FRAME_AVERAGE_LIGHT_LEVEL, 62.0});
                mWriter->setLayerPerFrameMetadata(staticMetadata);
                execute();
                ASSERT_EQ(0, mReader->mErrors.size());

                char data[MAX_HDR10PLUS_SIZE];
                int size = Exynos_dynamic_meta_to_itu_t_t35(&d_meta, data);
                std::cout << "meta blob size : " << size << std::endl;
                std::vector<uint8_t> vec;
                vec.resize(size);
                vec.assign(data, data + size);
                std::vector<V2_3::IComposerClient::PerFrameMetadataBlob> dynamicMetadata;
                dynamicMetadata.push_back(
                        {V2_3::IComposerClient::PerFrameMetadataKey::HDR10_PLUS_SEI, vec});

                mWriter->setLayerPerFrameMetadataBlobs(dynamicMetadata);
                execute();

                mWriter->validateDisplay();
                execute();

                if (mReader->mCompositionChanges.size() != 0) {
                    clearCommandReaderState();
                    GTEST_SUCCEED();
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
                }
        //   }
        //}
    }
}

TEST_P(CS_04_hdrdrvTest, CS_04_08_2_VerifyBuffer_HDR10P) {
    for (ColorMode mode : mTestColorModes) {
        std::cout << "---Testing Color Mode " << ReadbackHelper::getColorModeString(mode) << "---"
                  << std::endl;
        mDataspace = colorModeToDataspace(mode);
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

        if (!mHasReadbackBuffer)
            std::cout << "Readback not supported or unsupported pixelFormat/dataspace" << std::endl;

        float maxLuminance;
        float maxAverageLuminance;
        float minLuminance;
        std::vector<Hdr> HdrCapa;
        ASSERT_NO_FATAL_FAILURE(HdrCapa = mComposerClient->getHdrCapabilities(
        mPrimaryDisplay,
        &maxLuminance, &maxAverageLuminance, &minLuminance));
        ASSERT_TRUE(maxLuminance >= minLuminance);
        if(HdrCapa.size() <= 0) {
            std::cout << "HDR not supported (HdrCapa.size() <= 0)" << std::endl;
            GTEST_SKIP() << "HDR not supported";
            return;
        }
        bool hdr10p_capable = false;
        for (int i = 0; i < HdrCapa.size(); i++) {
            //std::cout << "HdrCapa[" << i << "] : " << (int)HdrCapa[i] << std::endl;
            if ((int)HdrCapa[i] == 4) {
                hdr10p_capable = true;
                break;
            }
        }
        ASSERT_TRUE(hdr10p_capable);

        mWriter->selectDisplay(mPrimaryDisplay);

        ReadbackBuffer readbackBuffer(mPrimaryDisplay, mComposerClient, mGralloc, mDisplayWidth,
                                      mDisplayHeight, mPixelFormat, mDataspace);
        if (mHasReadbackBuffer)
            ASSERT_NO_FATAL_FAILURE(readbackBuffer.setReadbackBuffer());
        std::vector<IComposerClient::Color> inputColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, 0, mDisplayWidth, mDisplayHeight / 4}, RED);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 4, mDisplayWidth, mDisplayHeight / 2},
                                       GREEN);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 2, mDisplayWidth, mDisplayHeight},
                                       BLUE);
        std::vector<IComposerClient::Color> expectedColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth,
                                       {0, 0, mDisplayWidth, mDisplayHeight / 4}, RED);
        ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth,
                                       {0, mDisplayHeight / 4, mDisplayWidth, mDisplayHeight / 2},
                                       GREEN);
        ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth,
                                       {0, mDisplayHeight / 2, mDisplayWidth, mDisplayHeight},
                                       BLUE);

        int i = 2;
        //for (int i = 0; i < 4; i++) {
        //    {
                ExynosHdrDynamicInfo d_meta;
                hdr10pTV.setDynamicMeta(&d_meta, i);
                auto dyn_lum = hdr10pTV.getSrcMaxLuminance(&d_meta);
                std::cout << "dyn_lum : " << dyn_lum << std::endl;

                auto layer = std::make_shared<TestBufferLayer>(
                        mComposerClient, mGralloc, *mTestRenderEngine, mPrimaryDisplay, mDisplayWidth,
                        mDisplayHeight, PixelFormat::RGBA_8888);
                layer->setDisplayFrame({0, 0, mDisplayWidth, mDisplayHeight});
                layer->setZOrder(10);
                layer->setDataspace(Dataspace::BT2020_PQ, mWriter);
                ASSERT_NO_FATAL_FAILURE(layer->setBuffer(inputColors));

                std::vector<std::shared_ptr<TestLayer>> layers = {layer};

                writeLayers(layers);
                ASSERT_EQ(0, mReader->mErrors.size());

                std::vector<V2_3::IComposerClient::PerFrameMetadata> staticMetadata;
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_X, 0.680});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_Y, 0.320});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_X, 0.265});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_Y, 0.690});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_X, 0.150});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_Y, 0.060});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_X, 0.3127});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_Y, 0.3290});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_LUMINANCE, 100.0});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MIN_LUMINANCE, 0.1});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_CONTENT_LIGHT_LEVEL, 78.0});
                staticMetadata.push_back(
                        {V2_3::IComposerClient::PerFrameMetadataKey::MAX_FRAME_AVERAGE_LIGHT_LEVEL, 62.0});
                mWriter->setLayerPerFrameMetadata(staticMetadata);
                execute();
                ASSERT_EQ(0, mReader->mErrors.size());

                char data[MAX_HDR10PLUS_SIZE];
                int size = Exynos_dynamic_meta_to_itu_t_t35(&d_meta, data);
                std::cout << "meta blob size : " << size << std::endl;
                std::vector<uint8_t> vec;
                vec.resize(size);
                vec.assign(data, data + size);
                std::vector<V2_3::IComposerClient::PerFrameMetadataBlob> dynamicMetadata;
                dynamicMetadata.push_back(
                        {V2_3::IComposerClient::PerFrameMetadataKey::HDR10_PLUS_SEI, vec});

                mWriter->setLayerPerFrameMetadataBlobs(dynamicMetadata);
                execute();

                mWriter->validateDisplay();
                execute();

                if (mReader->mCompositionChanges.size() != 0) {
                    clearCommandReaderState();
                    GTEST_SUCCEED();
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
                }
        //   }
        //}
    }
}

TEST_P(CS_04_hdrdrvTest, CS_04_08_3_VerifyBuffer_HDR10P) {
    for (ColorMode mode : mTestColorModes) {
        std::cout << "---Testing Color Mode " << ReadbackHelper::getColorModeString(mode) << "---"
                  << std::endl;
        mDataspace = colorModeToDataspace(mode);
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

        if (!mHasReadbackBuffer)
            std::cout << "Readback not supported or unsupported pixelFormat/dataspace" << std::endl;

        float maxLuminance;
        float maxAverageLuminance;
        float minLuminance;
        std::vector<Hdr> HdrCapa;
        ASSERT_NO_FATAL_FAILURE(HdrCapa = mComposerClient->getHdrCapabilities(
        mPrimaryDisplay,
        &maxLuminance, &maxAverageLuminance, &minLuminance));
        ASSERT_TRUE(maxLuminance >= minLuminance);
        if(HdrCapa.size() <= 0) {
            std::cout << "HDR not supported (HdrCapa.size() <= 0)" << std::endl;
            GTEST_SKIP() << "HDR not supported";
            return;
        }
        bool hdr10p_capable = false;
        for (int i = 0; i < HdrCapa.size(); i++) {
            //std::cout << "HdrCapa[" << i << "] : " << (int)HdrCapa[i] << std::endl;
            if ((int)HdrCapa[i] == 4) {
                hdr10p_capable = true;
                break;
            }
        }
        ASSERT_TRUE(hdr10p_capable);

        mWriter->selectDisplay(mPrimaryDisplay);

        ReadbackBuffer readbackBuffer(mPrimaryDisplay, mComposerClient, mGralloc, mDisplayWidth,
                                      mDisplayHeight, mPixelFormat, mDataspace);
        if (mHasReadbackBuffer)
            ASSERT_NO_FATAL_FAILURE(readbackBuffer.setReadbackBuffer());
        std::vector<IComposerClient::Color> inputColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, 0, mDisplayWidth, mDisplayHeight / 4}, RED);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 4, mDisplayWidth, mDisplayHeight / 2},
                                       GREEN);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 2, mDisplayWidth, mDisplayHeight},
                                       BLUE);
        std::vector<IComposerClient::Color> expectedColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth,
                                       {0, 0, mDisplayWidth, mDisplayHeight / 4}, RED);
        ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth,
                                       {0, mDisplayHeight / 4, mDisplayWidth, mDisplayHeight / 2},
                                       GREEN);
        ReadbackHelper::fillColorsArea(expectedColors, mDisplayWidth,
                                       {0, mDisplayHeight / 2, mDisplayWidth, mDisplayHeight},
                                       BLUE);

        int i = 3;
        //for (int i = 0; i < 4; i++) {
        //    {
                ExynosHdrDynamicInfo d_meta;
                hdr10pTV.setDynamicMeta(&d_meta, i);
                auto dyn_lum = hdr10pTV.getSrcMaxLuminance(&d_meta);
                std::cout << "dyn_lum : " << dyn_lum << std::endl;

                auto layer = std::make_shared<TestBufferLayer>(
                        mComposerClient, mGralloc, *mTestRenderEngine, mPrimaryDisplay, mDisplayWidth,
                        mDisplayHeight, PixelFormat::RGBA_8888);
                layer->setDisplayFrame({0, 0, mDisplayWidth, mDisplayHeight});
                layer->setZOrder(10);
                layer->setDataspace(Dataspace::BT2020_PQ, mWriter);
                ASSERT_NO_FATAL_FAILURE(layer->setBuffer(inputColors));

                std::vector<std::shared_ptr<TestLayer>> layers = {layer};

                writeLayers(layers);
                ASSERT_EQ(0, mReader->mErrors.size());

                std::vector<V2_3::IComposerClient::PerFrameMetadata> staticMetadata;
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_X, 0.680});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_Y, 0.320});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_X, 0.265});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_Y, 0.690});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_X, 0.150});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_Y, 0.060});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_X, 0.3127});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_Y, 0.3290});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_LUMINANCE, 100.0});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MIN_LUMINANCE, 0.1});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_CONTENT_LIGHT_LEVEL, 78.0});
                staticMetadata.push_back(
                        {V2_3::IComposerClient::PerFrameMetadataKey::MAX_FRAME_AVERAGE_LIGHT_LEVEL, 62.0});
                mWriter->setLayerPerFrameMetadata(staticMetadata);
                execute();
                ASSERT_EQ(0, mReader->mErrors.size());

                char data[MAX_HDR10PLUS_SIZE];
                int size = Exynos_dynamic_meta_to_itu_t_t35(&d_meta, data);
                std::cout << "meta blob size : " << size << std::endl;
                std::vector<uint8_t> vec;
                vec.resize(size);
                vec.assign(data, data + size);
                std::vector<V2_3::IComposerClient::PerFrameMetadataBlob> dynamicMetadata;
                dynamicMetadata.push_back(
                        {V2_3::IComposerClient::PerFrameMetadataKey::HDR10_PLUS_SEI, vec});

                mWriter->setLayerPerFrameMetadataBlobs(dynamicMetadata);
                execute();

                mWriter->validateDisplay();
                execute();

                if (mReader->mCompositionChanges.size() != 0) {
                    clearCommandReaderState();
                    GTEST_SUCCEED();
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
                }
        //   }
        //}
    }
}
#endif

#ifdef __CS_04_09__
TEST_P(CS_04_hdrdrvTest, CS_04_09_0_RandomParameter) {
    for (ColorMode mode : mTestColorModes) {
        std::cout << "---Testing Color Mode " << ReadbackHelper::getColorModeString(mode) << "---"
                  << std::endl;
        mWriter->selectDisplay(mPrimaryDisplay);
        ASSERT_NO_FATAL_FAILURE(
                mComposerClient->setColorMode(mPrimaryDisplay, mode, RenderIntent::COLORIMETRIC));

        float maxLuminance;
        float maxAverageLuminance;
        float minLuminance;
        std::vector<Hdr> HdrCapa;
        ASSERT_NO_FATAL_FAILURE(HdrCapa = mComposerClient->getHdrCapabilities(
        mPrimaryDisplay,
        &maxLuminance, &maxAverageLuminance, &minLuminance));
        ASSERT_TRUE(maxLuminance >= minLuminance);
        if(HdrCapa.size() <= 0) {
            std::cout << "HDR not supported (HdrCapa.size() <= 0)" << std::endl;
            GTEST_SKIP() << "HDR not supported";
            return;
        }
        bool hdr10p_capable = false;
        for (int i = 0; i < HdrCapa.size(); i++) {
            //std::cout << "HdrCapa[" << i << "] : " << (int)HdrCapa[i] << std::endl;
            if ((int)HdrCapa[i] == 4) {
                hdr10p_capable = true;
                break;
            }
        }
        ASSERT_TRUE(hdr10p_capable);
        std::vector<IComposerClient::Color> inputColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, 0, mDisplayWidth, mDisplayHeight / 4}, RED);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 4, mDisplayWidth, mDisplayHeight / 2},
                                       GREEN);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 2, mDisplayWidth, mDisplayHeight},
                                       BLUE);

        srand(time(NULL) + 0);
        //for (int i = 0; i < 4; i++) {
        //    {
                ExynosHdrDynamicInfo d_meta;
                hdr10pTV.setDynamicMeta(&d_meta, -1);

                auto layer = std::make_shared<TestBufferLayer>(
                        mComposerClient, mGralloc, *mTestRenderEngine, mPrimaryDisplay, mDisplayWidth,
                        mDisplayHeight, PixelFormat::RGBA_8888);
                layer->setDisplayFrame({0, 0, mDisplayWidth, mDisplayHeight});
                layer->setZOrder(10);
                int rand_st = (rand() % (((int)Dataspace::STANDARD_ADOBE_RGB >> (int)Dataspace::STANDARD_SHIFT) + 1)) << (int)Dataspace::STANDARD_SHIFT;
                int rand_tf = (rand() % (((int)Dataspace::TRANSFER_HLG >> (int)Dataspace::TRANSFER_SHIFT) + 1)) << (int)Dataspace::TRANSFER_SHIFT;
                int rand_li = (rand() % (((int)Dataspace::RANGE_EXTENDED >> (int)Dataspace::RANGE_SHIFT) + 1)) << (int)Dataspace::RANGE_SHIFT;
                Dataspace ds = (Dataspace)(rand_st | rand_tf | rand_li);
                layer->setDataspace(ds, mWriter);
                ASSERT_NO_FATAL_FAILURE(layer->setBuffer(inputColors));

                std::vector<std::shared_ptr<TestLayer>> layers = {layer};

                writeLayers(layers);
                ASSERT_EQ(0, mReader->mErrors.size());

                std::vector<V2_3::IComposerClient::PerFrameMetadata> staticMetadata;
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_X,    (float)(rand() % 1000) /1000});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_Y,    (float)(rand() % 1000) /1000});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_X,  (float)(rand() % 1000) /1000});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_Y,  (float)(rand() % 1000) /1000});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_X,   (float)(rand() % 1000) /1000});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_Y,   (float)(rand() % 1000) /1000});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_X, (float)(rand() % 10000) / 10000});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_Y, (float)(rand() % 10000) / 10000});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_LUMINANCE, (float)(rand() % 10000)});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MIN_LUMINANCE, (float)(rand() % 10) / 10});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_CONTENT_LIGHT_LEVEL, (float)(rand() % 100) / 100});
                staticMetadata.push_back(
                        {V2_3::IComposerClient::PerFrameMetadataKey::MAX_FRAME_AVERAGE_LIGHT_LEVEL, (float)(rand() % 100) / 100});
                mWriter->setLayerPerFrameMetadata(staticMetadata);
                execute();
                ASSERT_EQ(0, mReader->mErrors.size());

                char data[MAX_HDR10PLUS_SIZE];
                int size = Exynos_dynamic_meta_to_itu_t_t35(&d_meta, data);
                std::cout << "meta blob size : " << size << std::endl;
                std::vector<uint8_t> vec;
                vec.resize(size);
                vec.assign(data, data + size);
                std::vector<V2_3::IComposerClient::PerFrameMetadataBlob> dynamicMetadata;
                dynamicMetadata.push_back(
                        {V2_3::IComposerClient::PerFrameMetadataKey::HDR10_PLUS_SEI, vec});

                mWriter->setLayerPerFrameMetadataBlobs(dynamicMetadata);
                execute();

                mWriter->validateDisplay();
                execute();

                if (mReader->mCompositionChanges.size() != 0) {
                    clearCommandReaderState();
                    GTEST_SUCCEED();
                    return;
                }
                ASSERT_EQ(0, mReader->mErrors.size());

                mWriter->presentDisplay();
                execute();

                ASSERT_EQ(0, mReader->mErrors.size());


                //ASSERT_NO_FATAL_FAILURE(readbackBuffer.checkReadbackBuffer(expectedColors));
                //mTestRenderEngine->setRenderLayers(layers);
                //ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->drawLayers());
                //ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->checkColorBuffer(expectedColors));
        //   }
        //}
    }
}

TEST_P(CS_04_hdrdrvTest, CS_04_09_1_RandomParameter) {
    for (ColorMode mode : mTestColorModes) {
        std::cout << "---Testing Color Mode " << ReadbackHelper::getColorModeString(mode) << "---"
                  << std::endl;
        mWriter->selectDisplay(mPrimaryDisplay);
        ASSERT_NO_FATAL_FAILURE(
                mComposerClient->setColorMode(mPrimaryDisplay, mode, RenderIntent::COLORIMETRIC));

        float maxLuminance;
        float maxAverageLuminance;
        float minLuminance;
        std::vector<Hdr> HdrCapa;
        ASSERT_NO_FATAL_FAILURE(HdrCapa = mComposerClient->getHdrCapabilities(
        mPrimaryDisplay,
        &maxLuminance, &maxAverageLuminance, &minLuminance));
        ASSERT_TRUE(maxLuminance >= minLuminance);
        if(HdrCapa.size() <= 0) {
            std::cout << "HDR not supported (HdrCapa.size() <= 0)" << std::endl;
            GTEST_SKIP() << "HDR not supported";
            return;
        }
        bool hdr10p_capable = false;
        for (int i = 0; i < HdrCapa.size(); i++) {
            //std::cout << "HdrCapa[" << i << "] : " << (int)HdrCapa[i] << std::endl;
            if ((int)HdrCapa[i] == 4) {
                hdr10p_capable = true;
                break;
            }
        }
        ASSERT_TRUE(hdr10p_capable);
        std::vector<IComposerClient::Color> inputColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, 0, mDisplayWidth, mDisplayHeight / 4}, RED);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 4, mDisplayWidth, mDisplayHeight / 2},
                                       GREEN);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 2, mDisplayWidth, mDisplayHeight},
                                       BLUE);

        srand(time(NULL) + 1);
        //for (int i = 0; i < 4; i++) {
        //    {
                ExynosHdrDynamicInfo d_meta;
                hdr10pTV.setDynamicMeta(&d_meta, -1);

                auto layer = std::make_shared<TestBufferLayer>(
                        mComposerClient, mGralloc, *mTestRenderEngine, mPrimaryDisplay, mDisplayWidth,
                        mDisplayHeight, PixelFormat::RGBA_8888);
                layer->setDisplayFrame({0, 0, mDisplayWidth, mDisplayHeight});
                layer->setZOrder(10);
                int rand_st = (rand() % (((int)Dataspace::STANDARD_ADOBE_RGB >> (int)Dataspace::STANDARD_SHIFT) + 1)) << (int)Dataspace::STANDARD_SHIFT;
                int rand_tf = (rand() % (((int)Dataspace::TRANSFER_HLG >> (int)Dataspace::TRANSFER_SHIFT) + 1)) << (int)Dataspace::TRANSFER_SHIFT;
                int rand_li = (rand() % (((int)Dataspace::RANGE_EXTENDED >> (int)Dataspace::RANGE_SHIFT) + 1)) << (int)Dataspace::RANGE_SHIFT;
                Dataspace ds = (Dataspace)(rand_st | rand_tf | rand_li);
                layer->setDataspace(ds, mWriter);
                ASSERT_NO_FATAL_FAILURE(layer->setBuffer(inputColors));

                std::vector<std::shared_ptr<TestLayer>> layers = {layer};

                writeLayers(layers);
                ASSERT_EQ(0, mReader->mErrors.size());

                std::vector<V2_3::IComposerClient::PerFrameMetadata> staticMetadata;
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_X,    (float)(rand() % 1000) /1000});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_Y,    (float)(rand() % 1000) /1000});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_X,  (float)(rand() % 1000) /1000});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_Y,  (float)(rand() % 1000) /1000});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_X,   (float)(rand() % 1000) /1000});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_Y,   (float)(rand() % 1000) /1000});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_X, (float)(rand() % 10000) / 10000});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_Y, (float)(rand() % 10000) / 10000});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_LUMINANCE, (float)(rand() % 10000)});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MIN_LUMINANCE, (float)(rand() % 10) / 10});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_CONTENT_LIGHT_LEVEL, (float)(rand() % 100) / 100});
                staticMetadata.push_back(
                        {V2_3::IComposerClient::PerFrameMetadataKey::MAX_FRAME_AVERAGE_LIGHT_LEVEL, (float)(rand() % 100) / 100});
                mWriter->setLayerPerFrameMetadata(staticMetadata);
                execute();
                ASSERT_EQ(0, mReader->mErrors.size());

                char data[MAX_HDR10PLUS_SIZE];
                int size = Exynos_dynamic_meta_to_itu_t_t35(&d_meta, data);
                std::cout << "meta blob size : " << size << std::endl;
                std::vector<uint8_t> vec;
                vec.resize(size);
                vec.assign(data, data + size);
                std::vector<V2_3::IComposerClient::PerFrameMetadataBlob> dynamicMetadata;
                dynamicMetadata.push_back(
                        {V2_3::IComposerClient::PerFrameMetadataKey::HDR10_PLUS_SEI, vec});

                mWriter->setLayerPerFrameMetadataBlobs(dynamicMetadata);
                execute();

                mWriter->validateDisplay();
                execute();

                if (mReader->mCompositionChanges.size() != 0) {
                    clearCommandReaderState();
                    GTEST_SUCCEED();
                    return;
                }
                ASSERT_EQ(0, mReader->mErrors.size());

                mWriter->presentDisplay();
                execute();

                ASSERT_EQ(0, mReader->mErrors.size());


                //ASSERT_NO_FATAL_FAILURE(readbackBuffer.checkReadbackBuffer(expectedColors));
                //mTestRenderEngine->setRenderLayers(layers);
                //ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->drawLayers());
                //ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->checkColorBuffer(expectedColors));
        //   }
        //}
    }
}

TEST_P(CS_04_hdrdrvTest, CS_04_09_2_RandomParameter) {
    for (ColorMode mode : mTestColorModes) {
        std::cout << "---Testing Color Mode " << ReadbackHelper::getColorModeString(mode) << "---"
                  << std::endl;
        mWriter->selectDisplay(mPrimaryDisplay);
        ASSERT_NO_FATAL_FAILURE(
                mComposerClient->setColorMode(mPrimaryDisplay, mode, RenderIntent::COLORIMETRIC));

        float maxLuminance;
        float maxAverageLuminance;
        float minLuminance;
        std::vector<Hdr> HdrCapa;
        ASSERT_NO_FATAL_FAILURE(HdrCapa = mComposerClient->getHdrCapabilities(
        mPrimaryDisplay,
        &maxLuminance, &maxAverageLuminance, &minLuminance));
        ASSERT_TRUE(maxLuminance >= minLuminance);
        if(HdrCapa.size() <= 0) {
            std::cout << "HDR not supported (HdrCapa.size() <= 0)" << std::endl;
            GTEST_SKIP() << "HDR not supported";
            return;
        }
        bool hdr10p_capable = false;
        for (int i = 0; i < HdrCapa.size(); i++) {
            //std::cout << "HdrCapa[" << i << "] : " << (int)HdrCapa[i] << std::endl;
            if ((int)HdrCapa[i] == 4) {
                hdr10p_capable = true;
                break;
            }
        }
        ASSERT_TRUE(hdr10p_capable);
        std::vector<IComposerClient::Color> inputColors(mDisplayWidth * mDisplayHeight);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, 0, mDisplayWidth, mDisplayHeight / 4}, RED);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 4, mDisplayWidth, mDisplayHeight / 2},
                                       GREEN);
        ReadbackHelper::fillColorsArea(inputColors, mDisplayWidth,
                                       {0, mDisplayHeight / 2, mDisplayWidth, mDisplayHeight},
                                       BLUE);

        srand(time(NULL) + 2);
        //for (int i = 0; i < 4; i++) {
        //    {
                ExynosHdrDynamicInfo d_meta;
                hdr10pTV.setDynamicMeta(&d_meta, -1);

                auto layer = std::make_shared<TestBufferLayer>(
                        mComposerClient, mGralloc, *mTestRenderEngine, mPrimaryDisplay, mDisplayWidth,
                        mDisplayHeight, PixelFormat::RGBA_8888);
                layer->setDisplayFrame({0, 0, mDisplayWidth, mDisplayHeight});
                layer->setZOrder(10);
                int rand_st = (rand() % (((int)Dataspace::STANDARD_ADOBE_RGB >> (int)Dataspace::STANDARD_SHIFT) + 1)) << (int)Dataspace::STANDARD_SHIFT;
                int rand_tf = (rand() % (((int)Dataspace::TRANSFER_HLG >> (int)Dataspace::TRANSFER_SHIFT) + 1)) << (int)Dataspace::TRANSFER_SHIFT;
                int rand_li = (rand() % (((int)Dataspace::RANGE_EXTENDED >> (int)Dataspace::RANGE_SHIFT) + 1)) << (int)Dataspace::RANGE_SHIFT;
                Dataspace ds = (Dataspace)(rand_st | rand_tf | rand_li);
                layer->setDataspace(ds, mWriter);
                ASSERT_NO_FATAL_FAILURE(layer->setBuffer(inputColors));

                std::vector<std::shared_ptr<TestLayer>> layers = {layer};

                writeLayers(layers);
                ASSERT_EQ(0, mReader->mErrors.size());

                std::vector<V2_3::IComposerClient::PerFrameMetadata> staticMetadata;
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_X,    (float)(rand() % 1000) /1000});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_Y,    (float)(rand() % 1000) /1000});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_X,  (float)(rand() % 1000) /1000});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_Y,  (float)(rand() % 1000) /1000});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_X,   (float)(rand() % 1000) /1000});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_Y,   (float)(rand() % 1000) /1000});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_X, (float)(rand() % 10000) / 10000});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_Y, (float)(rand() % 10000) / 10000});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_LUMINANCE, (float)(rand() % 10000)});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MIN_LUMINANCE, (float)(rand() % 10) / 10});
                staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_CONTENT_LIGHT_LEVEL, (float)(rand() % 100) / 100});
                staticMetadata.push_back(
                        {V2_3::IComposerClient::PerFrameMetadataKey::MAX_FRAME_AVERAGE_LIGHT_LEVEL, (float)(rand() % 100) / 100});
                mWriter->setLayerPerFrameMetadata(staticMetadata);
                execute();
                ASSERT_EQ(0, mReader->mErrors.size());

                char data[MAX_HDR10PLUS_SIZE];
                int size = Exynos_dynamic_meta_to_itu_t_t35(&d_meta, data);
                std::cout << "meta blob size : " << size << std::endl;
                std::vector<uint8_t> vec;
                vec.resize(size);
                vec.assign(data, data + size);
                std::vector<V2_3::IComposerClient::PerFrameMetadataBlob> dynamicMetadata;
                dynamicMetadata.push_back(
                        {V2_3::IComposerClient::PerFrameMetadataKey::HDR10_PLUS_SEI, vec});

                mWriter->setLayerPerFrameMetadataBlobs(dynamicMetadata);
                execute();

                mWriter->validateDisplay();
                execute();

                if (mReader->mCompositionChanges.size() != 0) {
                    clearCommandReaderState();
                    GTEST_SUCCEED();
                    return;
                }
                ASSERT_EQ(0, mReader->mErrors.size());

                mWriter->presentDisplay();
                execute();

                ASSERT_EQ(0, mReader->mErrors.size());


                //ASSERT_NO_FATAL_FAILURE(readbackBuffer.checkReadbackBuffer(expectedColors));
                //mTestRenderEngine->setRenderLayers(layers);
                //ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->drawLayers());
                //ASSERT_NO_FATAL_FAILURE(mTestRenderEngine->checkColorBuffer(expectedColors));
        //   }
        //}
    }
}
#endif

#ifdef __CS_04_10__
TEST_P(CS_04_hdrdrvTest, CS_04_10_0_Check_Normal_Effect) {
    ColorMode mode = mTestColorModes[0];
    std::cout << "---Testing Color Mode " << ReadbackHelper::getColorModeString(mode) << "---"
              << std::endl;
    mDataspace = colorModeToDataspace(mode);
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
    
    if (!mHasReadbackBuffer)
        std::cout << "Readback not supported or unsupported pixelFormat/dataspace" << std::endl;
    
    float maxLuminance;
    float maxAverageLuminance;
    float minLuminance;
    std::vector<Hdr> HdrCapa;
    ASSERT_NO_FATAL_FAILURE(HdrCapa = mComposerClient->getHdrCapabilities(
    mPrimaryDisplay,
    &maxLuminance, &maxAverageLuminance, &minLuminance));
    ASSERT_TRUE(maxLuminance >= minLuminance);
    if(HdrCapa.size() <= 0) {
        std::cout << "HDR not supported (HdrCapa.size() <= 0)" << std::endl;
        GTEST_SKIP() << "HDR not supported";
        return;
    }
    bool hdr10p_capable = false;
    for (int i = 0; i < HdrCapa.size(); i++) {
        //std::cout << "HdrCapa[" << i << "] : " << (int)HdrCapa[i] << std::endl;
        if ((int)HdrCapa[i] == 4) {
            hdr10p_capable = true;
            break;
        }
    }
    ASSERT_TRUE(hdr10p_capable);
    
    mWriter->selectDisplay(mPrimaryDisplay);
    
    ReadbackBuffer readbackBuffer(mPrimaryDisplay, mComposerClient, mGralloc, mDisplayWidth,
                                  mDisplayHeight, mPixelFormat, mDataspace);
    if (mHasReadbackBuffer)
        ASSERT_NO_FATAL_FAILURE(readbackBuffer.setReadbackBuffer());
    std::vector<IComposerClient::Color> inputColors(mDisplayWidth * mDisplayHeight);
    struct _png_info_ pngInfo;
    read_png_file_to_info((char*)hdrTV.testimage.c_str(), &pngInfo);
    fillColorsWithPngBuffer(pngInfo.width, pngInfo.height,
                            mDisplayWidth, pngInfo.row_pointers, inputColors);
    free_png_info(&pngInfo);

    //for (int i = 0; i < 4; i++) {
    //    {
            auto layer = std::make_shared<TestBufferLayer>(
                    mComposerClient, mGralloc, *mTestRenderEngine, mPrimaryDisplay, mDisplayWidth,
                    mDisplayHeight, PixelFormat::RGBA_8888);
            layer->setDisplayFrame({0, 0, mDisplayWidth, mDisplayHeight});
            layer->setZOrder(10);
            layer->setDataspace(Dataspace::BT2020_PQ, mWriter);
            ASSERT_NO_FATAL_FAILURE(layer->setBuffer(inputColors));
    
            std::vector<std::shared_ptr<TestLayer>> layers = {layer};
    
            writeLayers(layers);
            ASSERT_EQ(0, mReader->mErrors.size());
    
            mWriter->validateDisplay();
            execute();
    
            if (mReader->mCompositionChanges.size() != 0) {
                clearCommandReaderState();
                GTEST_SUCCEED();
                return;
            }
            ASSERT_EQ(0, mReader->mErrors.size());
    
            mWriter->presentDisplay();
            execute();
    
            ASSERT_EQ(0, mReader->mErrors.size());
            sleep(2);
    //   }
    //}
}

TEST_P(CS_04_hdrdrvTest, CS_04_10_1_Check_HDR10_Effect) {
    ColorMode mode = mTestColorModes[0];
    std::cout << "---Testing Color Mode " << ReadbackHelper::getColorModeString(mode) << "---"
              << std::endl;
    mDataspace = colorModeToDataspace(mode);
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
    
    if (!mHasReadbackBuffer)
        std::cout << "Readback not supported or unsupported pixelFormat/dataspace" << std::endl;
    
    float maxLuminance;
    float maxAverageLuminance;
    float minLuminance;
    std::vector<Hdr> HdrCapa;
    ASSERT_NO_FATAL_FAILURE(HdrCapa = mComposerClient->getHdrCapabilities(
    mPrimaryDisplay,
    &maxLuminance, &maxAverageLuminance, &minLuminance));
    ASSERT_TRUE(maxLuminance >= minLuminance);
    if(HdrCapa.size() <= 0) {
        std::cout << "HDR not supported (HdrCapa.size() <= 0)" << std::endl;
        GTEST_SKIP() << "HDR not supported";
        return;
    }
    bool hdr10p_capable = false;
    for (int i = 0; i < HdrCapa.size(); i++) {
        //std::cout << "HdrCapa[" << i << "] : " << (int)HdrCapa[i] << std::endl;
        if ((int)HdrCapa[i] == 4) {
            hdr10p_capable = true;
            break;
        }
    }
    ASSERT_TRUE(hdr10p_capable);
    
    mWriter->selectDisplay(mPrimaryDisplay);
    
    ReadbackBuffer readbackBuffer(mPrimaryDisplay, mComposerClient, mGralloc, mDisplayWidth,
                                  mDisplayHeight, mPixelFormat, mDataspace);
    if (mHasReadbackBuffer)
        ASSERT_NO_FATAL_FAILURE(readbackBuffer.setReadbackBuffer());
    std::vector<IComposerClient::Color> inputColors(mDisplayWidth * mDisplayHeight);
    struct _png_info_ pngInfo;
    read_png_file_to_info((char*)hdrTV.testimage.c_str(), &pngInfo);
    fillColorsWithPngBuffer(pngInfo.width, pngInfo.height,
                            mDisplayWidth, pngInfo.row_pointers, inputColors);
    free_png_info(&pngInfo);

    //for (int i = 0; i < 4; i++) {
    //    {
            auto layer = std::make_shared<TestBufferLayer>(
                    mComposerClient, mGralloc, *mTestRenderEngine, mPrimaryDisplay, mDisplayWidth,
                    mDisplayHeight, PixelFormat::RGBA_8888);
            layer->setDisplayFrame({0, 0, mDisplayWidth, mDisplayHeight});
            layer->setZOrder(10);
            layer->setDataspace(Dataspace::BT2020_PQ, mWriter);
            ASSERT_NO_FATAL_FAILURE(layer->setBuffer(inputColors));
    
            std::vector<std::shared_ptr<TestLayer>> layers = {layer};
    
            writeLayers(layers);
            ASSERT_EQ(0, mReader->mErrors.size());
    
            std::vector<V2_3::IComposerClient::PerFrameMetadata> staticMetadata;
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_X, 0.680});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_Y, 0.320});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_X, 0.265});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_Y, 0.690});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_X, 0.150});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_Y, 0.060});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_X, 0.3127});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_Y, 0.3290});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_LUMINANCE, 100.0});
            std::cout << "static_lum : 100.0" << std::endl;
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MIN_LUMINANCE, 0.1});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_CONTENT_LIGHT_LEVEL, 78.0});
            staticMetadata.push_back(
                    {V2_3::IComposerClient::PerFrameMetadataKey::MAX_FRAME_AVERAGE_LIGHT_LEVEL, 62.0});
            mWriter->setLayerPerFrameMetadata(staticMetadata);
            execute();
            ASSERT_EQ(0, mReader->mErrors.size());
    
            mWriter->validateDisplay();
            execute();
    
            if (mReader->mCompositionChanges.size() != 0) {
                clearCommandReaderState();
                GTEST_SUCCEED();
                return;
            }
            ASSERT_EQ(0, mReader->mErrors.size());
    
            mWriter->presentDisplay();
            execute();
    
            ASSERT_EQ(0, mReader->mErrors.size());
            sleep(2);
    //   }
    //}
}
#endif

#ifdef __CS_04_11__
TEST_P(CS_04_hdrdrvTest, CS_04_11_0_Check_Normal_Effect) {
    ColorMode mode = mTestColorModes[0];
    std::cout << "---Testing Color Mode " << ReadbackHelper::getColorModeString(mode) << "---"
              << std::endl;
    mDataspace = colorModeToDataspace(mode);
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
    
    if (!mHasReadbackBuffer)
        std::cout << "Readback not supported or unsupported pixelFormat/dataspace" << std::endl;
    
    float maxLuminance;
    float maxAverageLuminance;
    float minLuminance;
    std::vector<Hdr> HdrCapa;
    ASSERT_NO_FATAL_FAILURE(HdrCapa = mComposerClient->getHdrCapabilities(
    mPrimaryDisplay,
    &maxLuminance, &maxAverageLuminance, &minLuminance));
    ASSERT_TRUE(maxLuminance >= minLuminance);
    if(HdrCapa.size() <= 0) {
        std::cout << "HDR not supported (HdrCapa.size() <= 0)" << std::endl;
        GTEST_SKIP() << "HDR not supported";
        return;
    }
    bool hdr10p_capable = false;
    for (int i = 0; i < HdrCapa.size(); i++) {
        //std::cout << "HdrCapa[" << i << "] : " << (int)HdrCapa[i] << std::endl;
        if ((int)HdrCapa[i] == 4) {
            hdr10p_capable = true;
            break;
        }
    }
    ASSERT_TRUE(hdr10p_capable);
    
    mWriter->selectDisplay(mPrimaryDisplay);
    
    ReadbackBuffer readbackBuffer(mPrimaryDisplay, mComposerClient, mGralloc, mDisplayWidth,
                                  mDisplayHeight, mPixelFormat, mDataspace);
    if (mHasReadbackBuffer)
        ASSERT_NO_FATAL_FAILURE(readbackBuffer.setReadbackBuffer());
    std::vector<IComposerClient::Color> inputColors(mDisplayWidth * mDisplayHeight);
    struct _png_info_ pngInfo;
    read_png_file_to_info((char*)hdrTV.testimage.c_str(), &pngInfo);
    fillColorsWithPngBuffer(pngInfo.width, pngInfo.height,
                            mDisplayWidth, pngInfo.row_pointers, inputColors);
    free_png_info(&pngInfo);

    //for (int i = 0; i < 4; i++) {
    //    {
            auto layer = std::make_shared<TestBufferLayer>(
                    mComposerClient, mGralloc, *mTestRenderEngine, mPrimaryDisplay, mDisplayWidth,
                    mDisplayHeight, PixelFormat::RGBA_8888);
            layer->setDisplayFrame({0, 0, mDisplayWidth, mDisplayHeight});
            layer->setZOrder(10);
            layer->setDataspace(Dataspace::BT2020_PQ, mWriter);
            ASSERT_NO_FATAL_FAILURE(layer->setBuffer(inputColors));
    
            std::vector<std::shared_ptr<TestLayer>> layers = {layer};
    
            writeLayers(layers);
            ASSERT_EQ(0, mReader->mErrors.size());
    
            mWriter->validateDisplay();
            execute();
    
            if (mReader->mCompositionChanges.size() != 0) {
                clearCommandReaderState();
                GTEST_SUCCEED();
                return;
            }
            ASSERT_EQ(0, mReader->mErrors.size());
    
            mWriter->presentDisplay();
            execute();
    
            ASSERT_EQ(0, mReader->mErrors.size());
            sleep(2);
    //   }
    //}
}

TEST_P(CS_04_hdrdrvTest, CS_04_11_1_Check_HDR10P_Effect) {
    ColorMode mode = mTestColorModes[0];
    std::cout << "---Testing Color Mode " << ReadbackHelper::getColorModeString(mode) << "---"
              << std::endl;
    mDataspace = colorModeToDataspace(mode);
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
    
    if (!mHasReadbackBuffer)
        std::cout << "Readback not supported or unsupported pixelFormat/dataspace" << std::endl;
    
    float maxLuminance;
    float maxAverageLuminance;
    float minLuminance;
    std::vector<Hdr> HdrCapa;
    ASSERT_NO_FATAL_FAILURE(HdrCapa = mComposerClient->getHdrCapabilities(
    mPrimaryDisplay,
    &maxLuminance, &maxAverageLuminance, &minLuminance));
    ASSERT_TRUE(maxLuminance >= minLuminance);
    if(HdrCapa.size() <= 0) {
        std::cout << "HDR not supported (HdrCapa.size() <= 0)" << std::endl;
        GTEST_SKIP() << "HDR not supported";
        return;
    }
    bool hdr10p_capable = false;
    for (int i = 0; i < HdrCapa.size(); i++) {
        //std::cout << "HdrCapa[" << i << "] : " << (int)HdrCapa[i] << std::endl;
        if ((int)HdrCapa[i] == 4) {
            hdr10p_capable = true;
            break;
        }
    }
    ASSERT_TRUE(hdr10p_capable);
    
    mWriter->selectDisplay(mPrimaryDisplay);
    
    ReadbackBuffer readbackBuffer(mPrimaryDisplay, mComposerClient, mGralloc, mDisplayWidth,
                                  mDisplayHeight, mPixelFormat, mDataspace);
    if (mHasReadbackBuffer)
        ASSERT_NO_FATAL_FAILURE(readbackBuffer.setReadbackBuffer());
    std::vector<IComposerClient::Color> inputColors(mDisplayWidth * mDisplayHeight);
    struct _png_info_ pngInfo;
    read_png_file_to_info((char*)hdr10pTV.testimage.c_str(), &pngInfo);
    fillColorsWithPngBuffer(pngInfo.width, pngInfo.height,
                            mDisplayWidth, pngInfo.row_pointers, inputColors);
    free_png_info(&pngInfo);

    int i = 1;
    //for (int i = 0; i < 4; i++) {
    //    {
            ExynosHdrDynamicInfo d_meta;
            hdr10pTV.setDynamicMeta(&d_meta, i);
            auto dyn_lum = hdr10pTV.getSrcMaxLuminance(&d_meta);
            std::cout << "dyn_lum : " << dyn_lum << std::endl;
    
            auto layer = std::make_shared<TestBufferLayer>(
                    mComposerClient, mGralloc, *mTestRenderEngine, mPrimaryDisplay, mDisplayWidth,
                    mDisplayHeight, PixelFormat::RGBA_8888);
            layer->setDisplayFrame({0, 0, mDisplayWidth, mDisplayHeight});
            layer->setZOrder(10);
            layer->setDataspace(Dataspace::BT2020_PQ, mWriter);
            ASSERT_NO_FATAL_FAILURE(layer->setBuffer(inputColors));
    
            std::vector<std::shared_ptr<TestLayer>> layers = {layer};
    
            writeLayers(layers);
            ASSERT_EQ(0, mReader->mErrors.size());
    
            std::vector<V2_3::IComposerClient::PerFrameMetadata> staticMetadata;
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_X, 0.680});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_RED_PRIMARY_Y, 0.320});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_X, 0.265});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_GREEN_PRIMARY_Y, 0.690});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_X, 0.150});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::DISPLAY_BLUE_PRIMARY_Y, 0.060});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_X, 0.3127});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::WHITE_POINT_Y, 0.3290});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_LUMINANCE, 100.0});
            std::cout << "static_lum : 100.0" << std::endl;
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MIN_LUMINANCE, 0.1});
            staticMetadata.push_back({V2_3::IComposerClient::PerFrameMetadataKey::MAX_CONTENT_LIGHT_LEVEL, 78.0});
            staticMetadata.push_back(
                    {V2_3::IComposerClient::PerFrameMetadataKey::MAX_FRAME_AVERAGE_LIGHT_LEVEL, 62.0});
            mWriter->setLayerPerFrameMetadata(staticMetadata);
            execute();
            ASSERT_EQ(0, mReader->mErrors.size());
    
            char data[MAX_HDR10PLUS_SIZE];
            int size = Exynos_dynamic_meta_to_itu_t_t35(&d_meta, data);
            std::cout << "meta blob size : " << size << std::endl;
            std::vector<uint8_t> vec;
            vec.resize(size);
            vec.assign(data, data + size);
            std::vector<V2_3::IComposerClient::PerFrameMetadataBlob> dynamicMetadata;
            dynamicMetadata.push_back(
                    {V2_3::IComposerClient::PerFrameMetadataKey::HDR10_PLUS_SEI, vec});
    
            mWriter->setLayerPerFrameMetadataBlobs(dynamicMetadata);
            execute();
    
            mWriter->validateDisplay();
            execute();
    
            if (mReader->mCompositionChanges.size() != 0) {
                clearCommandReaderState();
                GTEST_SUCCEED();
                return;
            }
            ASSERT_EQ(0, mReader->mErrors.size());
    
            mWriter->presentDisplay();
            execute();
    
            ASSERT_EQ(0, mReader->mErrors.size());
            sleep(2);
    //   }
    //}
}
#endif

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(CS_04_hdrdrvTest);
INSTANTIATE_TEST_SUITE_P(
        PerInstance, CS_04_hdrdrvTest,
        testing::ValuesIn(android::hardware::getAllHalInstanceNames(IComposer::descriptor)),
        android::hardware::PrintInstanceNameToString);

}
}
}
}

