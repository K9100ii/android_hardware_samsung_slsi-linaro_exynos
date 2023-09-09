/*
 * Copyright 2008, The Android Open Source Project
 * Copyright 2013, Samsung Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

 /*!
 * \file      SecCameraHardware.h
 * \brief     source file for Android Camera Ext HAL
 * \author    teahyung kim (tkon.kim@samsung.com)
 * \date      2013/04/30
 *
 */

#ifndef ANDROID_HARDWARE_SECCAMERAHARDWARE_H
#define ANDROID_HARDWARE_SECCAMERAHARDWARE_H

#include <poll.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <private/android_filesystem_config.h>

#include <time.h>
#include <cutils/properties.h>
#include <camera/Camera.h>
#include <media/hardware/MetadataBufferType.h>
#include <utils/Endian.h>

#include <videodev2.h>

#include <ExynosJpegApi.h>
#include "ISecCameraHardware.h"
#include "Exif.h"

namespace android {

#if defined(LOG_NDEBUG) && (LOG_NDEBUG == 0) && defined(HAL_DEBUG)
#define HALDBG(...) LOGV(__VA_ARGS__)
#else
#define HALDBG(...)
#endif

typedef uint32_t phyaddr_t;

typedef struct _s5p_rect {
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
} s5p_rect;

typedef struct _s5p_img {
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint32_t offset;
    uint32_t base;
    int memory_id;
} s5p_img;

typedef struct ADDRS
{
    unsigned int addrY;
    unsigned int addrCbCr;
    unsigned int addrCr;
} addrs_t;

typedef struct {
    uint16_t soi;
    uint8_t dummy0[22];
    struct {
        uint16_t jpegWidth;
        uint16_t jpegHeight;
        uint16_t thumbWidth;
        uint16_t thumbHeight;
        uint8_t dummy0[11];
    } __attribute__((packed)) resInfo;
    uint8_t dummy1[593];
} __attribute__((packed)) jpegHeader_t;

typedef struct {
    uint16_t sosi;
    uint16_t size0;
    struct {
        uint16_t infoVer;
        uint16_t chipId;
        uint16_t evtNum;
        uint16_t imageWidth;
        uint16_t imageHeight;
        uint16_t thumbWidth;
        uint16_t thumbHeight;
        uint32_t expTime;   /* usec */
        uint16_t frameTime;     /* msec */
        uint16_t analogGain;
        uint16_t digitalGain;
        struct {
            uint16_t red;
            uint16_t green;
            uint16_t blue;
        } __attribute__((packed)) wbGain;
        uint16_t brightness;
        uint16_t constrast;
        uint16_t afPosition;
    } __attribute__((packed)) jpegInfo;
    uint16_t size1;
    uint16_t eosi;
} __attribute__((packed)) jpegInfo_t;

struct jpeg_encode_param {
    int jpegFd;             /* input */
    void *srcBuf;           /* input */
    uint32_t srcWidth;      /* input */
    uint32_t srcHeight;     /* input */
    uint32_t srcBufSize;    /* input */
    uint32_t srcFormat;     /* input */
#ifdef SAMSUNG_EXYNOS4210
    enum jpeg_img_quality_level destJpegQuality; /* input */
#endif

    void *destJpegBuf;          /* output */
    uint32_t destJpegSize;     /* output */
};

class SecCameraHardware : public ISecCameraHardware {
public:
    int createInstance(int cameraId);
    bool mInitialized;

    virtual void        release();
protected:
    virtual bool        init();
#ifdef ENABLE_MC
    virtual bool        initMC();
#endif
    virtual void        initDefaultParameters();

    image_rect_type     nativeGetWindowSize();

    virtual status_t    nativeStartPreview();
    virtual status_t    nativeStartPreviewZoom();
    virtual int         nativeGetPreview();
#ifndef FCJUNG
    virtual int         nativeSetStream(bool flag);
#endif
    virtual int         nativeReleasePreviewFrame(int index);
    virtual void        nativeStopPreview();
#if FRONT_ZSL
    virtual status_t    nativeStartFullPreview();
    virtual int         nativeGetFullPreview();
    virtual int         nativeReleaseFullPreviewFrame(int index);
    virtual void        nativeStopFullPreview();
    virtual void        nativeForceStopFullPreview();
    bool                getZSLJpeg();
#endif

#if IS_FW_DEBUG
    virtual int         nativeGetDebugAddr(unsigned int *vaddr);
    virtual void        dump_is_fw_log(const char *fname, uint8_t *buf, uint32_t size);
#endif

    virtual status_t    nativeSetZoomRatio(int value);
    virtual status_t    nativePreviewCallback(int index, ExynosBuffer *grallocBuf);
    virtual status_t    nativeStartRecording();
    virtual status_t    nativeStartRecordingZoom();
    virtual void        nativeStopRecording();
    virtual status_t    nativeCSCPreview(int index, int type);
    virtual status_t    nativeCSCRecording(rec_src_buf_t *srcBuf, int dstIdx);
    virtual bool        getCropRect(unsigned int src_w, unsigned int src_h, unsigned int dst_w, unsigned int dst_h, unsigned int *crop_x, unsigned int *crop_y, unsigned int *crop_w, unsigned int *crop_h, int align_w, int align_h, int zoom);
    virtual bool        getRectZoomAlign(unsigned int src_w, unsigned int src_h, unsigned int dst_w, unsigned int dst_h, unsigned int *crop_x, unsigned int *crop_y, unsigned int *crop_w, unsigned int *crop_h, int align_w, int align_h, int zoom);

    virtual bool        nativeGetRecordingJpeg(ExynosBuffer *yuvBuf, uint32_t width, uint32_t height);

	virtual bool        nativeSetAutoFocus();
	virtual int         nativeGetPreAutoFocus();
    virtual int         nativeGetAutoFocus();
    virtual status_t    nativeCancelAutoFocus();
	virtual bool        nativePrepareYUVSnapshot();
	virtual bool        nativeStartYUVSnapshot();
    virtual bool        nativeGetYUVSnapshot(int numF, int *postviewOffset);

#ifndef FCJUNG
    virtual int		nativeGetFactoryOISDecenter();

    virtual int         nativeGetFactoryDownResult();
    virtual int         nativeGetFactoryEndResult();
    virtual int         nativeGetFactoryIspFwVerData();
    virtual int         nativeGetFactoryOisVerData();
    virtual int         nativeGetFactoryAFIntResult();
    virtual int         nativeGetFactoryFlashCharge();
#endif
#ifndef RCJUNG
    virtual bool        nativeGetSnapshotMainSeq(uint32_t mode, int numf);
    virtual bool        nativeGetOneYUVSnapshot();
    virtual bool        nativeDumpYUV();
#endif

    virtual bool        nativeStartSnapshot();
	virtual bool		nativeStartPostview();
    virtual void        nativeMakeJpegDump();
    virtual bool        nativeGetSnapshot(int numF, int *postviewOffset);
	virtual bool		nativeGetPostview(int numF);
    virtual void        nativeStopSnapshot();
    virtual bool        nativeStartDualCapture(int numF);	
    virtual status_t    nativeCSCCapture(ExynosBuffer *srcBuf, ExynosBuffer *dstBuf);
    virtual status_t    nativeCSCRecordingCapture(ExynosBuffer *srcBuf, ExynosBuffer *dstBuf);
    virtual int        nativegetWBcustomX();
    virtual int        nativegetWBcustomY();


#ifndef FCJUNG
	virtual int         nativeSetFactoryTestNum(uint32_t mFactoryTestNum);
#endif

	virtual int			nativeSetFastCapture(bool onOff);

    virtual status_t    nativeSetParameters(cam_control_id id, int value, bool recording = false);
    virtual status_t    nativeGetParameters(cam_control_id id, int *value, bool recordingMode = false);
	virtual status_t    nativeGetNotiParameters(cam_control_id id, int *read_val);

#ifndef FCJUNG
    virtual status_t    nativeGetExtParameters(cam_control_id id, char *value, int size, bool recordingMode = false);
    virtual status_t	nativeSetExtParameters(cam_control_id id, char *value, int size, bool recordingMode = false);
#endif

    virtual bool        nativeCreateSurface(uint32_t width, uint32_t height,
                                uint32_t halPixelFormat);
    virtual bool        nativeDestroySurface(void);
	virtual bool        nativeFlushSurfaceYUV420(uint32_t width, uint32_t height, uint32_t size, uint32_t index, int type = CAMERA_HEAP_POSTVIEW);
    virtual bool        nativeFlushSurface(uint32_t width, uint32_t height, uint32_t size, uint32_t index, int type=CAMERA_HEAP_PREVIEW);
	virtual bool        beautyLiveFlushSurface(uint32_t width, uint32_t height, uint32_t size, uint32_t index, int type=CAMERA_HEAP_PREVIEW);
    virtual bool        nativeIsInternalISP() { return mFlite.mIsInternalISP; };

    virtual bool        conversion420to422(uint8_t *src, uint8_t *dest, int width, int height);
    virtual bool        conversion420Tto422(uint8_t *src, uint8_t *dest, int width, int height);

public:
    SecCameraHardware(int cameraId, camera_device_t *dev);
    virtual ~SecCameraHardware();
    int mSaveDump(const char *filepath, ExynosBuffer *dstBuf, int index);

private:
    static gralloc_module_t const* mGrallocHal;

    class FLiteV4l2 {
    public:
        FLiteV4l2();
        virtual ~FLiteV4l2();

        int getfd() {
            return mCameraFd;
        }

        int init(const char *devPath, const int cameraId);
        void deinit();

        void getSensorSize(int frm_ratio, uint32_t *width, uint32_t *height);
        /* preview */
        int startPreview(image_rect_type *fliteSize, cam_pixel_format format,
                           int numBufs, int fps, bool movieMode, node_info_t *mFliteNode);

        /* capture */
        int startCapture(image_rect_type *img, cam_pixel_format format, int numBufs, int capKind);
        int startRecord(image_rect_type *img, image_rect_type *videoSize, cam_pixel_format format, int numBufs);
        sp<MemoryHeapBase> querybuf(uint32_t *frmsize);
        int reqBufZero(node_info_t *mFliteNode);
        int querybuf2(unsigned int index, int planeCnt, ExynosBuffer *buf);
        int expBuf(unsigned int index, int planeCnt, ExynosBuffer *buf);

        int qbuf(uint32_t index);
        int dqbuf();
        int dqbuf(uint32_t *index);

        int qbuf2(node_info_t *node, uint32_t index);
        int qbufForCapture(ExynosBuffer *buf, uint32_t index);
        int dqbuf2(node_info_t *node);
        int dqbufForCapture(ExynosBuffer *buf);

#ifdef FAKE_SENSOR
        int fakeQbuf2(node_info_t *node, uint32_t index);
        int fakeDqbuf2(node_info_t *node);
#endif
        int stream(bool on);
#ifndef FPOLLING
        int polling(bool bFactoryTest, bool recordingMode=false);
#endif
		int notictrl(uint32_t id, int *read_val);
        int gctrl(uint32_t id, int *value);
        int gctrl(uint32_t id, unsigned short *value);
        int gctrl(uint32_t id, char *value, int size);
        int sctrl(uint32_t id, int value);
#ifndef FCJUNG
        int sctrl(uint32_t id, char *value, int size);
#endif
	 int sparm(struct v4l2_streamparm *stream_parm);
        int getYuvPhyaddr(int index, phyaddr_t *y, phyaddr_t *cbcr);
        int getYuvPhyaddr(int index, phyaddr_t *y, phyaddr_t *cb, phyaddr_t *cr);
        int reset();
        void forceStop();
        bool getStreamStatus() {
            return mStreamOn;
        }

#ifndef FCJUNG
        int setFactoryTestNumFimc(uint32_t mFactoryTestNum);
#endif

		int setFastCaptureFimc(uint32_t IsFastCaptureCalled);

        int mIsInternalISP;
        int getFd();

#ifdef APPLY_ESD
	bool mI2CErr;
#endif

    private:
#ifdef FAKE_SENSOR
        int fakeIndex;
        int fakeByteData;
#endif
		int mCameraId;
        int mCameraFd;
        int mBufferCount;
        bool mStreamOn;
#ifndef FCJUNG
        int mFactoryTestNum_fimc;
#endif
        int mCmdStop;
		bool mFastCapture;
    };

#if FRONT_ZSL
    static const int kBufferZSLCount = 4;
#endif

#if IS_FW_DEBUG
    int m_mem_fd;
#endif

    int mJpegIndex;

#ifdef CHG_ENCODE_JPEG
    int     EncodeToJpeg(unsigned char* src, int width, int height,
                cam_pixel_format srcformat, unsigned char* dst, int* jpegSize, int quality = ExynosJpegEncoder::QUALITY_LEVEL_1);
#endif
    int     EncodeToJpeg(ExynosBuffer *yuvBuf, ExynosBuffer *jpegBuf,
                int width, int height, cam_pixel_format srcformat, int* jpegSize, int quality = ExynosJpegEncoder::QUALITY_LEVEL_1);

    FLiteV4l2        mFlite;
    FLiteV4l2        mFimc1;
    void             *mFimc1CSC;
    mutable Mutex     mFimc1CSCLock;

    void             *mFimc2CSC;
    mutable Mutex     mFimc2CSCLock;

    // media controller variable
    struct media_device *mMedia;
    struct media_entity *mSensorEntity;
    struct media_entity *mMipiEntity;
    struct media_entity *mFliteSdEntity;
    struct media_entity *mFliteVdEntity;

    s5p_rect        mPreviewZoomRect;
    s5p_rect        mPictureZoomRect;
    s5p_rect        mRecordZoomRect;
    int             mZoomValue;

    addrs_t             mWindowBuffer;

    cam_pixel_format    mRecordingFormat;
    mutable Mutex       mNativeRecordLock;

    exif_attribute_t    mExifInfo;

    bool            setMCSubdevFormat(struct media_entity *entity,
                                     int w, int h, unsigned int pad,
                                     int ext_cam_mode);
    bool            setAllMCSubdevFormat(int w, int h, int ext_cam_mode);


    bool    allocatePreviewHeap();
    bool    allocateRecordingHeap();

    bool    allocateRecordingSnapshotHeap();
    bool    allocateSnapshotHeap();
    bool    allocatePostviewHeap();
#if FRONT_ZSL
    bool    allocateFullPreviewHeap();
#endif

#ifdef BURST_SHOT_SUPPORT
	bool	nativeSaveJpegPicture(const char *fname, burst_item* item);
#endif

	int		save_dump_path(uint8_t *real_data, int data_size, const char* filePath);
    bool	allocateHDRHeap();
    int		getYUV(int fnum);
#ifndef RCJUNG
    int     getOneYUV();
    bool    allocateYUVHeap();
#endif
#ifndef FPOLLING
    bool    isFactoryTest();
#endif
	int		getPostview(int num);
    int     getJpegOnBack(int *postviewOffset);
    int     getJpegOnFront(int *postviewOffset);

    int		findMinFnumber(int value);
    int		findFocalLength(int value);
    int		findFocalLength35mm(int value);
    int		findExposureProgramMode();
    void	findRTShutterSpeed(int *rt_num, int *rt_den, int *index);
    void    setExifFixedAttribute();
    void    setExifChangedAttribute();
    void    setMakerNote(bool enable);
    bool    addExif(int &exifSize);

    int     internalGetJpegForISP(int *postviewOffset);
#if !defined(REAR_USE_YUV_CAPTURE)
    int     internalGetJpegForSocJpeg(int *postviewOffset);
#endif
    int     internalGetJpegForSocYuvWithZoom(int *postviewOffset);
	int     getCombinedJpeg(int *postviewOffset);
    int     internalGetJpegForSocYuv(int *postviewOffset);
	int		internalGetJpegForRawWithZoom(int *postviewOffset);
    bool    internalEncodingJpeg(struct jpeg_encode_param *encodeParam);
    bool    jpegOpenEncoding(struct jpeg_encode_param &encodeParam);
    bool    jpegExecuteEncoding(struct jpeg_encode_param &encodeParam);
    bool    jpegCloseEncoding(struct jpeg_encode_param &encodeParam);
#if !defined(REAR_USE_YUV_CAPTURE)
    bool    extractJpegAndRawThumb(uint32_t jpegOffset, uint32_t *rawThumbSize,
                            uint32_t rawThumbOffset, void *srcInterleavedDataBuf,
                            uint32_t *jpegSize, void *dstJpegBuf, void *dstRawThumbBuf);
#endif
    bool    scaleDownYuv422(uint8_t *srcBuf, int srcW, int srcH,
                                            uint8_t *dstBuf, int dstW, int dstH);

};
}; /* namespace android */

#endif /* ANDROID_HARDWARE_SECCAMERAHARDWARE_H */
