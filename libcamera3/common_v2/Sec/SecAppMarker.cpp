/*
**
** Copyright 2013, Samsung Electronics Co. LTD
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed toggle an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

/*!
 * \file      SecAppMarker.cpp
 * \brief     source file for add debug data into APP Marker.
 * \author    dh348.chang(dh348.chang@samsung.com)
 * \date      2013/07/17
 *
 * <b>Revision History: </b>
 * - 2013/07/17 : dh348.chang(dh348.chang@samsung.com) \n
 *   Initial version
 */

#include "SecAppMarker.h"

SecAppMarker::SecAppMarker()
    :
	startExifAddr(NULL),
	exifDataSize(0),
	startJpegAddr(NULL),
	jpegDataSize(0),
	startDebugAddr(NULL),
	debugDataSize(0),
	debugAppMarker(0),
	dataOut(NULL),
	dataSize(0),
	app_marker_flag(0x0003),
	number_of_appmarker(1)
{
	ALOGD("%s Opened\n", __func__);
}

SecAppMarker::~SecAppMarker()
{
	if (dataOut) {
		delete[] dataOut;
		dataOut = NULL;
	}
	ALOGD("%s Closed\n", __func__);
}

int SecAppMarker::destroy()
{
	return 0;
}

int SecAppMarker::setExifStartAddr(char *exifAddr, unsigned int exifSize)
{
	if (exifSize == 0 || exifSize > MAX_APPMARKER_SIZE) {
		ALOGE("%s : exif Size is wrong!", __func__);
		return -1;
	}
	startExifAddr = exifAddr;
	exifDataSize = exifSize;

	return 0;
}

int SecAppMarker::setJpegStartAddr(char *jpegAddr, unsigned int jpegSize)
{
	if (jpegSize == 0) {
		ALOGE("%s : jpeg Size is wrong!", __func__);
		return -1;
	}
	startJpegAddr = jpegAddr;
	jpegDataSize = jpegSize;

	return 0;
}

int SecAppMarker::setDebugStartAddr(char *debugAddr, unsigned int debugSize)
{
	if (debugSize == 0 || debugSize > MAX_APPMARKER_SIZE - 2) {
		ALOGE("%s : debug Size is wrong!", __func__);
		return -1;
	}
	startDebugAddr = debugAddr;
	debugDataSize = debugSize;

	return 0;
}

int SecAppMarker::checkExifData()
{
	char *ep;
	int appMarker;
	char markerSize_h;
	char markerSize_l;
	unsigned int markerSize;

	ep = startExifAddr;

	appMarker = 0;
	appMarker = (*ep << 8 & 0xff00) + *(ep + 1);

	markerSize_h = *(ep + 2);
	markerSize_l = *(ep + 3);

	if (appMarker != APP1_MARKER) {
		ALOGE("%s : %x, %x", __func__, *ep, *(ep + 1));
		ALOGE("%s : this data is not Exif Data(%x)", __func__, appMarker);
		return -1;
	}
	markerSize = (markerSize_h << 8 & 0xff00) + markerSize_l;

	ep = ep + 2 + markerSize;

	if (ep != startExifAddr + exifDataSize) {
		ALOGE("%s : Exif data Size is wrong! ep = %p, calSize = %p",
				__func__, ep, startExifAddr + exifDataSize);
		return -1;
	}

	return 0;
}

int SecAppMarker::checkJpegData()
{
	char *ep;
	int appMarker;
	char markerSize_h;
	char markerSize_l;
	unsigned int markerSize;
	int i;

	ep = startJpegAddr;

	appMarker = 0;

	appMarker = (*ep << 8 & 0xff00) + *(ep + 1);

	if (appMarker != SOI_MARKER) {
		ALOGE("%s : this data is not Jpeg Data(%x)", __func__, appMarker);
		return -1;
	}

	ep = ep + 2;

	for (i = 0; i < 16; i++) {
		appMarker = (*ep << 8 & 0xff00) + *(ep + 1);

		if (appMarker == DQT_MARKER)
			goto checkJpegDataEnd;
		if (appMarker != APP0_MARKER + i)
			continue;
		markerSize_h = *(ep + 2);
		markerSize_l = *(ep + 3);
		markerSize = (markerSize_h << 8) + markerSize_l;

		ep = ep + 2 + markerSize;
		app_marker_flag = app_marker_flag | (0x1 << i);
	}
checkJpegDataEnd:

	return 0;
}

int SecAppMarker::pushDebugData()
{
	char data[2];
	char *dP;
	unsigned int size;

	ALOGD("%s: start! exif(%04x), debug(%04x), jpeg(%04x)",
			__func__, exifDataSize, debugDataSize, jpegDataSize);
	ALOGD("%s: debugAppMarker(%d), app_marker_flag(%04x)",
			__func__, debugAppMarker, app_marker_flag);

	size = exifDataSize + 4 + debugDataSize + jpegDataSize;
	dataOut = new char[size];
	dP = dataOut;

	if (jpegDataSize) {
		snprintf(data, 2, "%04x", SOI_MARKER);
		memcpy(dP, data, 2);
		dP = dP + 2;
	}

	if (exifDataSize) {
		if (checkExifData() < 0) {
			ALOGE("%s : checkExifData Failed!", __func__);
			return -1;
		}
		memcpy(dP, startExifAddr, exifDataSize);
		dP = dP + exifDataSize;
	}

	if (debugDataSize) {
		if (debugAppMarker > 1) {
			if (!(app_marker_flag & (0x1 << debugAppMarker))) {
				data[0] = 0xff;
				data[1] = 0xe0 + (0xf & debugAppMarker);
				ALOGD("%s: make app marker %02x, %02x", __func__, data[0], data[1]);
			} else {
				ALOGE("%s : already APP%d_MARKER used!", __func__, debugAppMarker);
				return -1;
			}
		} else {
			if (!(app_marker_flag & (0x1 << 4))) {
				data[0] = 0xff;
				data[1] = 0xe4;
				ALOGD("%s: make default app marker %02x, %02x", __func__, data[0], data[1]);
			} else {
				ALOGE("%s : already APP4_MARKER used!", __func__);
				return -1;
			}
		}

		memcpy(dP, data, 2);
		dP = dP + 2;

		data[0] = (0xff00 & debugDataSize) >> 8;
		data[1] = (0xff & debugDataSize) + 2;

		memcpy(dP, data, 2);
		dP = dP + 2;

		ALOGD("%s: make app marker size %02x, %02x", __func__, data[0], data[1]);

		memcpy(dP, startDebugAddr, debugDataSize);
		dP = dP + debugDataSize;
	}

	if (jpegDataSize) {
		memcpy(dP, startJpegAddr + 2, jpegDataSize);
	}

	return 0;
}

int SecAppMarker::pushDebugData(void *appData)
{
    char data[2];
    char *dP;
    unsigned int size;

    app_marker_attribute_t *debugInfo;
    debugInfo = (app_marker_attribute_t *)appData;

    ALOGD("%s: start! exif(%04x), debug(%04x), jpeg(%04x)",
            __func__, exifDataSize, debugDataSize, jpegDataSize);
    ALOGD("%s: debugAppMarker(%d), app_marker_flag(%04x)",
            __func__, debugAppMarker, app_marker_flag);

    number_of_appmarker = debugInfo->num_of_appmarker;
    size = exifDataSize + (4 * number_of_appmarker) + debugDataSize + jpegDataSize;
    dataOut = new char[size];
    dP = dataOut;

    if (jpegDataSize) {
        snprintf(data, 2, "%04x", SOI_MARKER);
        memcpy(dP, data, 2);
        dP = dP + 2;
    }

    if (exifDataSize) {
        if (checkExifData() < 0) {
            ALOGE("%s : checkExifData Failed!", __func__);
            return -1;
        }
        memcpy(dP, startExifAddr, exifDataSize);
        dP = dP + exifDataSize;
    }

    if (debugDataSize) {
        for(int i = 0; i < debugInfo->num_of_appmarker; i++) {
            /* Add App marker */
            data[0] = 0xff;
            data[1] = 0xe0 + debugInfo->idx[i][0];

            memcpy(dP, data, 2);
            dP = dP + 2;

            data[0] = (0xff00 & debugInfo->debugSize[debugInfo->idx[i][0]]) >> 8;
            data[1] = (0xff & debugInfo->debugSize[debugInfo->idx[i][0]]) + 2;

            memcpy(dP, data, 2);
            dP = dP + 2;

            memcpy(dP, debugInfo->debugData[debugInfo->idx[i][0]],
                    debugInfo->debugSize[debugInfo->idx[i][0]]);
            dP = dP + debugInfo->debugSize[debugInfo->idx[i][0]];
        }
    }

    if (jpegDataSize) {
        memcpy(dP, startJpegAddr + 2, jpegDataSize);
    }

    return 0;
}

int SecAppMarker::pushDebugData(char *exifAddr, char *debugAddr,
		unsigned int exifSize, unsigned int debugSize)
{
	if (!exifSize || !debugSize ||
		exifSize > MAX_APPMARKER_SIZE || debugSize > MAX_APPMARKER_SIZE - 2) {
		ALOGE("%s : exif or debug Size is wrong!", __func__);
		return -1;
	} else {
		startExifAddr = exifAddr;
		startDebugAddr = debugAddr;
		exifDataSize = exifSize;
		debugDataSize = debugSize;
		jpegDataSize = 0;
	}

	pushDebugData();

	return 0;
}

int SecAppMarker::pushDebugData(char *exifAddr, char *debugAddr, char *jpegAddr,
		unsigned int exifSize, unsigned int debugSize, unsigned int jpegSize)
{
	if (!exifSize || !debugSize || !jpegSize ||
		exifSize > MAX_APPMARKER_SIZE || debugSize > MAX_APPMARKER_SIZE - 2) {
		ALOGE("%s : exif or debug or jpeg Size is wrong!", __func__);
		return -1;
	} else {
		startExifAddr = exifAddr;
		startDebugAddr = debugAddr;
		startJpegAddr = jpegAddr;
		exifDataSize = exifSize;
		debugDataSize = debugSize;
		jpegDataSize = jpegSize;
	}

	pushDebugData();

	return 0;
}

int SecAppMarker::setAppMarker(int appMarker)
{
	if (appMarker <= 1 || appMarker > 15) {
		ALOGE("%s : app marker cannot set %d!", __func__, appMarker);
		return -1;
	} else
		debugAppMarker = appMarker;
	return 0;
}

char *SecAppMarker::getResultData(void)
{
	return dataOut;
}

int SecAppMarker::getResultDataSize(void)
{
	dataSize = exifDataSize + (4 * number_of_appmarker) + debugDataSize + jpegDataSize;
	return dataSize;
}
