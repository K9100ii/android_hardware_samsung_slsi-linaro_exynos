/*
 * Copyright@ Samsung Electronics Co. LTD
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

/*!
 * \file      ExynosCameraImage.h
 * \brief     header file for ExynosCameraImage
 * \author    Sangwoo, Park(sw5771.park@samsung.com)
 * \date      2016/10/05
 *
 * <b>Revision History: </b>
 * - 2016/10/05 : Sangwoo, Park(sw5771.park@samsung.com) \n
 *   Initial version
 */

#ifndef EXYNOS_CAMERA_IMAGE_H
#define EXYNOS_CAMERA_IMAGE_H

#include "ExynosRect.h"
#include "ExynosCameraBuffer.h"

using namespace android;

/*
 * struct ExynosCameraImage
 */
struct ExynosCameraImage {
    ExynosRect         rect;
    ExynosCameraBuffer buf;

    int                rotation; // 0, 90, 180, 270
    bool               flipH;
    bool               flipV;

    ExynosCameraImage() {
        rotation = 0;
        flipH    = false;
        flipV    = false;
    }

    ExynosCameraImage& operator =(const ExynosCameraImage &other) {
        rect     = other.rect;
        buf      = other.buf;

        rotation = other.rotation;
        flipH    = other.flipH;
        flipV    = other.flipV;

        return *this;
    }

    bool operator ==(const ExynosCameraImage &other) const {
        bool ret = true;

        if (rect     != other.rect ||
            buf      != other.buf ||

            rotation != other.rotation ||
            flipH    != other.flipH ||
            flipV    != other.flipV) {
            ret = false;
        }

        return ret;
    }

    bool operator !=(const ExynosCameraImage &other) const {
        return !(*this == other);
    }
};

/*
 * struct ExynosCameraImageCapacity
 */
struct ExynosCameraImageCapacity {
public:
    enum MAX_NUM_OF_IMAGE {
        MAX_NUM_OF_IMAGE_BASE = 0,
        MAX_NUM_OF_IMAGE_MAX  = 2,
    };

    enum MAX_NUM_OF_FORMAT {
        MAX_NUM_OF_FORMAT_BASE = 0,
        MAX_NUM_OF_FORMAT_MAX  = 8,
    };

private:
    int m_numOfImage;

    int m_numOfColorFormat;
    int m_colorFormat[MAX_NUM_OF_FORMAT_MAX];
    int m_widthConstraint[MAX_NUM_OF_FORMAT_MAX];
    // you can add here.

public:
    ExynosCameraImageCapacity() {
        m_numOfImage = 1;

        m_numOfColorFormat = 0;
        for (int i = 0; i < MAX_NUM_OF_FORMAT_MAX; i++) {
            m_colorFormat[i] = 0;
            m_widthConstraint[i] = 1;
        }
    }

    virtual ~ExynosCameraImageCapacity() {
    }

    // numOfImage
    void setNumOfImage(int numOfImage) {
        m_numOfImage = numOfImage;
    }

    int getNumOfImage(void) {
        return m_numOfImage;
    }

    // colorFormat
    void addColorFormat(int colorFormat, int widthContraint = 1) {
        if (MAX_NUM_OF_FORMAT_MAX <= m_numOfColorFormat) {
            ALOGD("MAX_NUM_OF_FORMAT_MAX(%d) <= m_numOfColorFormat(%d). so fail",
                MAX_NUM_OF_FORMAT_MAX, m_numOfColorFormat);
            return;
        }

        m_colorFormat[m_numOfColorFormat] = colorFormat;
        m_widthConstraint[m_numOfColorFormat] = widthContraint;
        m_numOfColorFormat++;
    }

    bool flagSupportedColorFormat(const int colorFormat, const int width = 1) const {
        bool ret = false;

        for (int i = 0; i < m_numOfColorFormat; i++) {
            if (colorFormat == m_colorFormat[i]) {
                if (width % m_widthConstraint[i] != 0) {
                    ret = false;
                } else {
                    ret = true;
                }

                break;
            }
        }

        return ret;
    }

    ///////////////////////////////////////////////////////////////////////////
    // operator overloading

    ExynosCameraImageCapacity& operator =(const ExynosCameraImageCapacity &other) {
        m_numOfImage = other.m_numOfImage;

        m_numOfColorFormat = other.m_numOfColorFormat;
        for (int i = 0; i < MAX_NUM_OF_FORMAT_MAX; i++) {
            m_colorFormat[i]     = other.m_colorFormat[i];
            m_widthConstraint[i] = other.m_widthConstraint[i];
        }

        return *this;
    }

    bool operator ==(const ExynosCameraImageCapacity &other) const {
        bool ret = true;

        if (m_numOfImage != other.m_numOfImage) {
            ret = false;
        }

        if (m_numOfColorFormat != other.m_numOfColorFormat) {
            ret = false;
        }

        for (int i = 0; i < MAX_NUM_OF_FORMAT_MAX; i++) {
            if (m_colorFormat[i]     != other.m_colorFormat[i] ||
                m_widthConstraint[i] != other.m_widthConstraint[i]) {
                ret = false;
            }
        }

        return ret;
    }

    bool operator !=(const ExynosCameraImageCapacity &other) const {
        return !(*this == other);
    }
};

#endif //EXYNOS_CAMERA_IMAGE_H
