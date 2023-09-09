/*
 * Copyright (C) 2021 Samsung Electronics Co. Ltd.
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

#include <iostream>

#include <log/log.h>
#include <gtest/gtest.h>
#include <ui/GraphicBuffer.h>
#include <vndk/hardware_buffer.h>

#include "exynos_format.h"
#include "../SBWCHelper.h"
#include "SBWCHelperTestHelper.h"
#include "ExynosGraphicBufferCore.h"

using namespace android;
using namespace vendor::graphics;

TEST(SBWCHelperTest, YUVFormatCheck)
{
	printTestName();

	sp<GraphicBuffer> yuvGB = newFHDGB(HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M);
	AHardwareBuffer *yuvAHB = yuvGB->toAHardwareBuffer();

	EXPECT_FALSE(SBWCHelper::isSbwcFormat(yuvAHB));
}

TEST(SBWCHelperTest, SBWCFormatCheck)
{
	printTestName();

	sp<GraphicBuffer> sbwcGB = newFHDGB(HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_SBWC);
	AHardwareBuffer *sbwcAHB = sbwcGB->toAHardwareBuffer();

	EXPECT_TRUE(SBWCHelper::isSbwcFormat(sbwcAHB));
}

TEST(SBWCHelperTest, NewYuvAHBWithNull)
{
	printTestName();

	AHardwareBuffer *ahb = nullptr;

	EXPECT_FALSE(SBWCHelper::newYuvAHB(nullptr, nullptr));
	EXPECT_FALSE(SBWCHelper::newYuvAHB(nullptr, &ahb));
	EXPECT_FALSE(SBWCHelper::newYuvAHB(ahb, nullptr));
}

TEST(SBWCHelperTest, DeprecatedNewYuvAHBWithNull)
{
	printTestName();

	AHardwareBuffer *newAHB = nullptr;

	newAHB = SBWCHelper::newYuvAHB(nullptr);
	EXPECT_TRUE(newAHB == nullptr);
}

TEST(SBWCHelperTest, NewYuvAHBWithYuv)
{
	printTestName();

	sp<GraphicBuffer> yuvGB = newFHDGB(HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M);
	AHardwareBuffer *yuvAHB = yuvGB->toAHardwareBuffer();

	AHardwareBuffer *sbwcAHB = nullptr;

	EXPECT_FALSE(SBWCHelper::newYuvAHB(yuvAHB, &sbwcAHB));
}

TEST(SBWCHelperTest, DeprecatedNewYuvAHBWithYuv)
{
	printTestName();

	sp<GraphicBuffer> yuvGB = newFHDGB(HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M);
	AHardwareBuffer *yuvAHB = yuvGB->toAHardwareBuffer();

	AHardwareBuffer *sbwcAHB = nullptr;
	sbwcAHB = SBWCHelper::newYuvAHB(yuvAHB);

	EXPECT_TRUE(sbwcAHB == nullptr);
}

TEST(SBWCHelperTest, NewYuvAHBWithSbwc)
{
	printTestName();

	sp<GraphicBuffer> sbwcGB = newFHDGB(HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_SBWC);
	AHardwareBuffer *sbwcAHB = sbwcGB->toAHardwareBuffer();

	AHardwareBuffer *yuvAHB = nullptr;

	EXPECT_TRUE(SBWCHelper::newYuvAHB(sbwcAHB, &yuvAHB));

	const native_handle_t *handle = AHardwareBuffer_getNativeHandle(yuvAHB);
	uint32_t format = ExynosGraphicBufferMeta::get_format(handle);

	EXPECT_TRUE(format == HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M);

	EXPECT_TRUE(SBWCHelper::freeYuvAHB(&yuvAHB));
}

TEST(SBWCHelperTest, DeprecatedNewYuvAHBWithSbwc)
{
	printTestName();

	sp<GraphicBuffer> sbwcGB = newFHDGB(HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_SBWC);
	AHardwareBuffer *sbwcAHB = sbwcGB->toAHardwareBuffer();

	AHardwareBuffer *yuvAHB = SBWCHelper::newYuvAHB(sbwcAHB);

	const native_handle_t *handle = AHardwareBuffer_getNativeHandle(yuvAHB);
	uint32_t format = ExynosGraphicBufferMeta::get_format(handle);

	EXPECT_TRUE(format == HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M);

	EXPECT_TRUE(SBWCHelper::freeYuvAHB(yuvAHB));
}

TEST(SBWCHelperTest, NewYuvAHBWith10Bit)
{
	printTestName();

	sp<GraphicBuffer> sbwcGB = newFHDGB(HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_10B_SBWC);
	AHardwareBuffer *sbwcAHB = sbwcGB->toAHardwareBuffer();

	AHardwareBuffer *yuvAHB = nullptr;

	EXPECT_TRUE(SBWCHelper::newYuvAHB(sbwcAHB, &yuvAHB));

	const native_handle_t *handle = AHardwareBuffer_getNativeHandle(yuvAHB);
	uint32_t format = ExynosGraphicBufferMeta::get_format(handle);

	EXPECT_TRUE(format == HAL_PIXEL_FORMAT_EXYNOS_YCbCr_P010_M);

	EXPECT_TRUE(SBWCHelper::freeYuvAHB(&yuvAHB));
}

TEST(SBWCHelperTest, DeprecatedNewYuvAHBWith10Bit)
{
	printTestName();

	sp<GraphicBuffer> sbwcGB = newFHDGB(HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_10B_SBWC);
	AHardwareBuffer *sbwcAHB = sbwcGB->toAHardwareBuffer();

	AHardwareBuffer *yuvAHB = SBWCHelper::newYuvAHB(sbwcAHB);

	const native_handle_t *handle = AHardwareBuffer_getNativeHandle(yuvAHB);
	uint32_t format = ExynosGraphicBufferMeta::get_format(handle);

	EXPECT_TRUE(format == HAL_PIXEL_FORMAT_EXYNOS_YCbCr_P010_M);

	EXPECT_TRUE(SBWCHelper::freeYuvAHB(yuvAHB));
}

TEST(SBWCHelperTest, NewYuvAHBRepeat)
{
	printTestName();

	sp<GraphicBuffer> sbwcGB = newFHDGB(HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_SBWC);
	AHardwareBuffer *sbwcAHB = sbwcGB->toAHardwareBuffer();

	AHardwareBuffer *yuvAHB = nullptr;

	for (int i = 0; i < 5; i++)
	{
		yuvAHB = nullptr;

		EXPECT_TRUE(SBWCHelper::newYuvAHB(sbwcAHB, &yuvAHB));

		const native_handle_t *handle = AHardwareBuffer_getNativeHandle(yuvAHB);
		uint32_t format = ExynosGraphicBufferMeta::get_format(handle);

		EXPECT_TRUE(format == HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M);
	}

	for (int i = 0; i < 5; i++)
	{
		EXPECT_TRUE(SBWCHelper::freeYuvAHB(&yuvAHB));
	}
}

TEST(SBWCHelperTest, NewYuvAHBAndFreeYuvAHBRepeat)
{
	printTestName();

	for(int i = 0; i < 10; i++)
	{
		sp<GraphicBuffer> sbwcGB = newFHDGB(HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_SBWC);
		AHardwareBuffer *sbwcAHB = sbwcGB->toAHardwareBuffer();

		AHardwareBuffer *yuvAHB = nullptr;

		EXPECT_TRUE(SBWCHelper::newYuvAHB(sbwcAHB, &yuvAHB));

		const native_handle_t *handle = AHardwareBuffer_getNativeHandle(yuvAHB);
		uint32_t format = ExynosGraphicBufferMeta::get_format(handle);

		EXPECT_TRUE(format == HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M);

		EXPECT_TRUE(SBWCHelper::freeYuvAHB(&yuvAHB));
		sbwcGB = nullptr;
	}
}

TEST(SBWCHelperTest, DeprecatedNewYuvAHBRepeat)
{
	printTestName();

	sp<GraphicBuffer> sbwcGB = newFHDGB(HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_SBWC);
	AHardwareBuffer *sbwcAHB = sbwcGB->toAHardwareBuffer();

	AHardwareBuffer *yuvAHB = nullptr;

	for (int i = 0; i < 5; i++)
	{
		yuvAHB = SBWCHelper::newYuvAHB(sbwcAHB);

		const native_handle_t *handle = AHardwareBuffer_getNativeHandle(yuvAHB);
		uint32_t format = ExynosGraphicBufferMeta::get_format(handle);

		EXPECT_TRUE(format == HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M);
	}

	for (int i = 0; i < 5; i++)
	{
		EXPECT_TRUE(SBWCHelper::freeYuvAHB(yuvAHB));
	}
}

TEST(SBWCHelperTest, DecompressWithNull)
{
	printTestName();

	AHardwareBuffer *sbwcAHB = nullptr;
	EXPECT_FALSE(SBWCHelper::decompress(sbwcAHB));
}

TEST(SBWCHelperTest, DecompressWithYuv)
{
	printTestName();

	sp<GraphicBuffer> yuvGB = newFHDGB(HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M);
	AHardwareBuffer *yuvAHB = yuvGB->toAHardwareBuffer();

	EXPECT_FALSE(SBWCHelper::decompress(yuvAHB));
}

TEST(SBWCHelperTest, DecompressWithSbwcNotRegistered)
{
	printTestName();

	sp<GraphicBuffer> sbwcGB = newFHDGB(HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_SBWC);
	AHardwareBuffer *sbwcAHB = sbwcGB->toAHardwareBuffer();

	EXPECT_FALSE(SBWCHelper::decompress(sbwcAHB));
}

TEST(SBWCHelperTest, DecompressWithSbwcAfterFreed)
{
	printTestName();

	sp<GraphicBuffer> sbwcGB = newFHDGB(HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_SBWC);
	AHardwareBuffer *sbwcAHB = sbwcGB->toAHardwareBuffer();

	AHardwareBuffer *yuvAHB = nullptr;
	EXPECT_TRUE(SBWCHelper::newYuvAHB(sbwcAHB, &yuvAHB));

	EXPECT_TRUE(SBWCHelper::decompress(sbwcAHB));

	EXPECT_TRUE(SBWCHelper::freeYuvAHB(&yuvAHB));

	EXPECT_FALSE(SBWCHelper::decompress(sbwcAHB));
}

TEST(SBWCHelperTest, DecompressWithSbwcRegistered)
{
	printTestName();

	sp<GraphicBuffer> sbwcGB = newFHDGB(HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_SBWC);
	AHardwareBuffer *sbwcAHB = sbwcGB->toAHardwareBuffer();

	AHardwareBuffer *yuvAHB = nullptr;
	EXPECT_TRUE(SBWCHelper::newYuvAHB(sbwcAHB, &yuvAHB));

	EXPECT_TRUE(SBWCHelper::decompress(sbwcAHB));

	EXPECT_TRUE(SBWCHelper::freeYuvAHB(&yuvAHB));
}

TEST(SBWCHelperTest, DecompressWithSbwc10BitRegistered)
{
	printTestName();

	sp<GraphicBuffer> sbwcGB = newFHDGB(HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_10B_SBWC);
	AHardwareBuffer *sbwcAHB = sbwcGB->toAHardwareBuffer();

	AHardwareBuffer *yuvAHB = nullptr;
	EXPECT_TRUE(SBWCHelper::newYuvAHB(sbwcAHB, &yuvAHB));

	EXPECT_TRUE(SBWCHelper::decompress(sbwcAHB));

	EXPECT_TRUE(SBWCHelper::freeYuvAHB(&yuvAHB));
}

TEST(SBWCHelperTest, FreeYuvAHBWithNull)
{
	printTestName();

	// Explicit to call bool freeYuvAHB(AHardwareBuffer **inYuvAHB)
	EXPECT_FALSE(SBWCHelper::freeYuvAHB((AHardwareBuffer **)nullptr));
}

TEST(SBWCHelperTest, DeprecatedFreeYuvAHBWithNull)
{
	printTestName();

	// Explicit to call bool freeYuvAHB(AHardwareBuffer *inYuvAHB)
	EXPECT_FALSE(SBWCHelper::freeYuvAHB((AHardwareBuffer *)nullptr));
}

TEST(SBWCHelperTest, FreeYuvAHBWithNotRegistered)
{
	printTestName();

	sp<GraphicBuffer> yuvGB = newFHDGB(HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M);
	AHardwareBuffer *yuvAHB = yuvGB->toAHardwareBuffer();

	EXPECT_FALSE(SBWCHelper::freeYuvAHB(&yuvAHB));
}

TEST(SBWCHelperTest, DeprecatedFreeYuvAHBWithNotRegistered)
{
	printTestName();

	sp<GraphicBuffer> yuvGB = newFHDGB(HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M);
	AHardwareBuffer *yuvAHB = yuvGB->toAHardwareBuffer();

	EXPECT_FALSE(SBWCHelper::freeYuvAHB(yuvAHB));
}

TEST(SBWCHelperTest, FreeYuvAHBWithSbwc)
{
	printTestName();

	sp<GraphicBuffer> sbwcGB = newFHDGB(HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_SBWC);
	AHardwareBuffer *sbwcAHB = sbwcGB->toAHardwareBuffer();

	EXPECT_FALSE(SBWCHelper::freeYuvAHB(&sbwcAHB));
}

TEST(SBWCHelperTest, DeprecatedFreeYuvAHBWithSbwc)
{
	printTestName();

	sp<GraphicBuffer> sbwcGB = newFHDGB(HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_SBWC);
	AHardwareBuffer *sbwcAHB = sbwcGB->toAHardwareBuffer();

	EXPECT_FALSE(SBWCHelper::freeYuvAHB(sbwcAHB));
}

TEST(SBWCHelperTest, FreeYuvAHBMoreThanAlloc)
{
	printTestName();

	sp<GraphicBuffer> sbwcGB = newFHDGB(HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_SBWC);
	AHardwareBuffer *sbwcAHB = sbwcGB->toAHardwareBuffer();

	AHardwareBuffer *yuvAHB = nullptr;
	EXPECT_TRUE(SBWCHelper::newYuvAHB(sbwcAHB, &yuvAHB));

	EXPECT_TRUE(SBWCHelper::freeYuvAHB(&yuvAHB));
	EXPECT_FALSE(SBWCHelper::freeYuvAHB(&yuvAHB));
}

int main(int argc, char* argv[])
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
