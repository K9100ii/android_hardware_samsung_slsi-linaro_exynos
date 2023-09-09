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

/** Get a new FullHD Size GraphicBuffer
 *
 * @param[in] Exynos format want to be allocated
 * @return GraphicBufferBuffer allocated
 */

#include <ui/GraphicBuffer.h>

using namespace android;

/** Print currently running test name
 *
 */
void printTestName();

/** Allocate new FullHD size GraphicBuffer
 *
 * @param[in] format PixelFormat of GraphicBuffer want to be allocated
 * @return Strong pointer of GraphicBuffer newly allocated
 */
sp<GraphicBuffer> newFHDGB(PixelFormat format);
