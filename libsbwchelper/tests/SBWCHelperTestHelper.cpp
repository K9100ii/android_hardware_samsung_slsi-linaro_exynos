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

#include <gtest/gtest.h>

#include "SBWCHelperTestHelper.h"

void printTestName()
{
	const testing::TestInfo *const test_info =
		testing::UnitTest::GetInstance()->current_test_info();

	ALOGI("[SBWC] Run %s.%s", test_info->test_suite_name(), test_info->name());
}

sp<GraphicBuffer> newFHDGB(PixelFormat format)
{
        uint32_t width = 1920;
        uint32_t height = 1080;
        uint64_t usage = GraphicBuffer::USAGE_HW_TEXTURE | GraphicBuffer::USAGE_HW_RENDER | GraphicBuffer::USAGE_HW_VIDEO_ENCODER;

        sp<GraphicBuffer> gb(new GraphicBuffer(width, height, format, usage));
        status_t err = gb->initCheck();

        if (err != 0)
        {
                ALOGE("[SBWC] GraphicBuffer allocation failed");
                return nullptr;
        }

        return gb;
}
