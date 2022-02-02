
#include <vx_cl.h>

#if defined(CL_USE_IMAGES)

__kernel void vx_histogram_8(read_only image2d_t src,
                             uint num, uint range, uint offset, uint winsize, __global int *hist)
{
    const int x = get_global_id(0);
    const int y = get_global_id(1);
    uint4 pix = read_imageui(src, nearest_clamp, (int2)(x,y));
    vxIncFrequency(hist, pix.s0, offset, range, winsize); 
}
#else

//#define vxImagePixel(type, ptr, x, y, sx, sy) 					(*(type *)(&((uchar *)ptr)[((y) * sy) + ((x) * sx)]))
//#define vxIncFrequency(ptr, value, offset, range, window_size)	((offset <= value) && (value <= (range+offset)) ? ++ptr[(value-offset)/window_size] : 0)

__kernel void vx_histogram_8(int w, int h, int sx, int sy, __global uchar *src,
                             uint num, uint range, uint offset, uint winsize, __global int *hist)
{

    const int x = get_global_id(0);
    const int y = get_global_id(1);
#ifdef ARM_MALI //mali OpenCL
    uchar pix = vxImagePixel_uchar(src, x, y, sx, sy);
#else    
    uchar pix = vxImagePixel(uchar, src, x, y, sx, sy);
#endif    

#if 1 //[2016/03/09] jspark modified to sync shared data (hist)
	if ((offset <= pix) && (pix <= (range + offset)))
	{
		//hist[(pix - offset) / winsize]++;
		atomic_inc(hist + (pix - offset) / winsize);  //we need atomic operation to sync up data
	}
#else
	vxIncFrequency(hist, pix, offset, range, winsize); 
#endif    
}
#endif
