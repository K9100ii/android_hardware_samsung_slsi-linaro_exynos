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
 * \file      SecAppMarker.h
 * \brief     hearder file for add debug data into APP Marker.
 * \author    dh348.chang(dh348.chang@samsung.com)
 * \date      2013/07/17
 *
 * <b>Revision History: </b>
 * - 2013/07/17 : dh348.chang(dh348.chang@samsung.com) \n
 *   Initial version
 */

#include <log/log.h>
#include <cstring>
#define MAX_APPMARKER_SIZE (64 * 1024)

typedef struct {
    int num_of_appmarker; /* number of app marker */
    int idx[15][1]; /* idx[number_of_appmarker][appmarker_number] */
    char *debugData[15]; /* 0-base */
    unsigned int debugSize[15];
} app_marker_attribute_t;

enum Jfif_Markers {
	SOI_MARKER = 0xFFD8,
	SOF_MARKER,
	DQT_MARKER = 0xFFDB,
	APP0_MARKER = 0xFFE0,
	APP1_MARKER,
	APP2_MARKER,
	APP3_MARKER,
	APP4_MARKER,
	APP5_MARKER,
	APP6_MARKER,
	APP7_MARKER,
	APP8_MARKER,
	APP9_MARKER,
	APP10_MARKER,
	APP11_MARKER,
	APP12_MARKER,
	APP13_MARKER,
	APP14_MARKER,
	APP15_MARKER,
};

class SecAppMarker {
public:
	SecAppMarker();
	~SecAppMarker();

	int destroy();
	int setExifStartAddr(char *exifAddr, unsigned int exifSize);
	char *getExifStartAddr(void) {return startExifAddr;}
	int getExifSize(void) {return exifDataSize;}

	int setJpegStartAddr(char *jpegAddr, unsigned int jpegSize);
	char *getJpegStartAddr(void) {return startJpegAddr;}
	int getJpegSize(void) {return jpegDataSize;}

	int setDebugStartAddr(char *debugAddr, unsigned int debugSize);
	char *getDebugStartAddr(void) {return startDebugAddr;}
	int getDebugSize(void) {return debugDataSize;}

	int pushDebugData();
	int pushDebugData(void *appData);
	int pushDebugData(char *exifAddr, char *debugAddr,
			unsigned int exifSize, unsigned int debugSize);
	int pushDebugData(char *exifAddr, char *debugAddr, char *jpegAddr,
			unsigned int exifSize, unsigned int debugSize, unsigned int jpegSize);

	int setAppMarker(int appMarker);

	char *getResultData(void);
	int getResultDataSize(void);

private:
	int checkExifData(void);
	int checkJpegData(void);

	char *startExifAddr;
	unsigned int exifDataSize;
	char *startJpegAddr;
	unsigned int jpegDataSize;
	char *startDebugAddr;
	unsigned int debugDataSize;
	int debugAppMarker;
	char *dataOut;
	int dataSize;
	int app_marker_flag;
	int number_of_appmarker;
};
