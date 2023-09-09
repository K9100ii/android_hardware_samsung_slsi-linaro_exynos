#ifndef __DPU_SBWC_DECODER_H__
#define __DPU_SBWC_DECODER_H__

class DpuSbwcDecoder {
private:
    unsigned int mSrcFmtSBWC = 0;
    unsigned int mDstFmtSBWC = 0;
    unsigned int mSrcWidth = 0;
    unsigned int mSrcHeight = 0;
    unsigned int mDstWidth = 0;
    unsigned int mDstHeight = 0;
    unsigned int mStride = 0;
    unsigned int mDstStride = 0;
    unsigned int mColorSpace = 0;
    bool mIsProtected = false;
    int mTempBuf[2][2] = {{-1, -1}, {-1, -1}};
    unsigned int log_level = 0;
public:
    bool decodeSBWC (unsigned int src_format, unsigned int dst_format,
		unsigned int src_width, unsigned int src_height,
                unsigned int dst_width, unsigned int dst_height,
                unsigned int stride, unsigned int dst_stride, unsigned int attr,
		int inBuf[], int ouBuf[])
    {
	    mSrcFmtSBWC = src_format;
	    mSrcWidth = src_width;
	    mSrcHeight = src_height;
	    mDstFmtSBWC = dst_format;
	    mDstWidth = dst_width;
	    mDstHeight = dst_height;
	    mStride = stride;
	    mDstStride = dst_stride;
	    mIsProtected = !!attr;
	    mTempBuf[0][0] = inBuf[0];
	    mTempBuf[1][0] = ouBuf[0];
	    return false;
    }
    bool decodeSBWC (unsigned int src_format, unsigned int dst_format, unsigned int colorspace,
		unsigned int src_width, unsigned int src_height,
		unsigned int dst_width, unsigned int dst_height,
		unsigned int stride, unsigned int dst_stride, unsigned int attr,
		int inBuf[], int outBuf[])
    {
	    mSrcFmtSBWC = src_format;
	    mSrcWidth = src_width;
	    mSrcHeight = src_height;
	    mDstFmtSBWC = dst_format;
	    mDstWidth = dst_width;
	    mDstHeight = dst_height;
	    mStride = stride;
	    mColorSpace = colorspace;
	    mDstStride = dst_stride;
	    mIsProtected = !!attr;
	    mTempBuf[0][0] = inBuf[0];
	    mTempBuf[1][0] = outBuf[0];
	    return false;
    }
    void setLogLevel(unsigned int __log_level)
    {
	    log_level = __log_level;
    }
};

#endif
