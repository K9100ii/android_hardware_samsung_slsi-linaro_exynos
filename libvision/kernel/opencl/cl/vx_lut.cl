
#include <vx_cl.h>

#if defined(CL_USE_IMAGES)

__kernel void vx_lut(read_only image2d_t src, 
                       uint isize, uint nitems, uint cap, int stride, __global uchar *lut,
                       write_only image2d_t dst)
{
    const int2 coord = (int2)(get_global_id(0), get_global_id(1));
    uchar4 a = convert_uchar4(read_imageui(src, nearest_clamp, coord));
    uchar4 b = (uchar4)(lut[a.s0], lut[a.s1], lut[a.s2], lut[a.s3]);
    ushort4 c = upsample((uchar4)(0,0,0,0),b);
    uint4 d = upsample((ushort4)(0,0,0,0),c);
    write_imageui(dst, coord, d);
}

#else

#ifdef ARM_MALI //mali OpenCL
__kernel void vx_lut(int sw, int sh, int ssx, int ssy, __global uchar *src,
                     uint isize, uint nitems, uint cap, int stride, __global uchar *lut_base,
                     int dw, int dh, int dsx, int dsy, __global uchar *dst)
#else
__kernel void vx_lut(int sw, int sh, int ssx, int ssy, __global void *src,
                     uint isize, uint nitems, uint cap, int stride, __global void *lut_base,
                     int dw, int dh, int dsx, int dsy, __global void *dst)
#endif                     
{
    const int x = get_global_id(0);
    const int y = get_global_id(1);

#if 1  //jspark modififed
	size_t index = 0;
	if (stride == 1 && isize == 1)
	{
#ifdef ARM_MALI //mali OpenCL
		index = (size_t)vxImagePixel_uchar(src, x, y, ssx, ssy);
		vxImagePixel_uchar(dst, x, y, dsx, dsy) = vxArrayItem_uchar(lut_base, index, stride);
#else	
		index = (size_t)vxImagePixel(uchar, src, x, y, ssx, ssy);
		vxImagePixel(uchar, dst, x, y, dsx, dsy) = vxArrayItem(uchar, lut_base, index, stride);
#endif		
	}
	else if (stride == 2 && isize == 2)
	{
#ifdef ARM_MALI //mali OpenCL
		//we should add this code
#else	
		index = (size_t)vxImagePixel(short, src, x, y, ssx, ssy);
		vxImagePixel(short, dst, x, y, dsx, dsy) = vxArrayItem(short, lut_base, index, stride);
#endif		
	}

#else
    size_t index = 0;
    if (stride == 1 && isize == 1)
    {
        index = (size_t)vxImagePixel(uchar, src, x, y, ssx, ssy);
        if (index < 256 && index < nitems)
        {
            vxImagePixel(uchar, dst, x, y, dsx, dsy) = vxArrayItem(uchar, lut_base, index, stride);
        }
    }
    else if (stride == 2 && isize == 2)
    {
        index = (size_t)vxImagePixel(short, src, x, y, ssx, ssy);
        if (index < 65535 && index < nitems)
        {
            vxImagePixel(short, dst, x, y, dsx, dsy) = vxArrayItem(short, lut_base, index, stride);
        }
    }
#endif
}

#endif


