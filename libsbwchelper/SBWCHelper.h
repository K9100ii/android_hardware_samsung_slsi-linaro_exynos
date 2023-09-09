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

namespace SBWCHelper
{

/** Check SBWC format or not
 *
 * @param[in] inAHB The buffer will be used to check for format
 * @return a boolean that shows whether it is in SBWC format or not
 */
bool isSbwcFormat(AHardwareBuffer *inAHB);

/** Check if actual SBWC data is present in the buffer
 *
 * @param[in] inAHB The buffer to check the content
 * @return true if SBWC data is present, false otherwise.
 */
bool ContainsSbwcData(AHardwareBuffer *inAHB);

/** Obtain a YUV buffer for use by the GPU
 *
 * @param[in] inSbwcAHB AHardwareBuffer will be used to allocate YUV AHardwareBuffer
 * @param[out] outYuvAHB Double pointer of AHardwareBuffer newly will be allocated
 * @return result
 */
bool newYuvAHB(AHardwareBuffer *inSbwcAHB, AHardwareBuffer **outYuvAHB);

/** Request decompress to SBWCHelper
 *
 * @param[in] inSbwcAHB The buffer to decompress
 * @return result
 */
bool decompress(AHardwareBuffer *inSbwcAHB);

/** Inform SBWCHelper that you are no longer using YUV AHB to avoid memory leak
 *
 * @param[in] Double pointer of the buffer want to free
 * @return result
 */
bool freeYuvAHB(AHardwareBuffer **inYuvAHB);

/** Derive byte stride of a SBWC buffer
 *
 * @param[in] format SBWC buffer format to use in stride calculation
 * @param[in] alloc_width alloc_width of the buffer (can be larger than requested width)
 * @param[in] plane plane index
 * @return derived stride in bytes
 */
uint32_t getByteStride(uint32_t format, uint32_t alloc_width, uint32_t plane);

//---------------------------------------------------------------------------------------
// DEPRECATED
//---------------------------------------------------------------------------------------

/** Obtain a YUV buffer for use by the GPU
 *
 * @param[in] sbwcAHB The buffer will be used to allocate YUV AHardwareBuffer
 * @return AHardwareBuffer to be used by GPU
 */
AHardwareBuffer *newYuvAHB(AHardwareBuffer *sbwcAHB);

/** Inform SBWCHelper that you are no longer using YUV AHB to avoid memory leak
 *
 * @param[in] yuvAHB The buffer want to free
 * @return result
 */
bool freeYuvAHB(AHardwareBuffer *yuvAHB);

} // namespace SBWCHelper
