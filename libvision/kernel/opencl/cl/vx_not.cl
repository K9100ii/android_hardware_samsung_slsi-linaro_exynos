
#include <vx_cl.h>

#if defined(CL_USE_IMAGES)

__kernel void vx_not(read_only image2d_t a, write_only image2d_t b) {
    int2 coord = (get_global_id(0), get_global_id(1));
    write_imageui(b, coord, ~read_imageui(a, nearest_clamp, coord));
}

#else

#pragma OPENCL EXTENSION cl_arm_printf : enable
#ifdef ARM_MALI //mali OpenCL
__kernel void vx_not(int aw, int ah, int asx, int asy, __global uchar *a, 
                     int bw, int bh, int bsx, int bsy, __global uchar *b)
#else
__kernel void vx_not(int aw, int ah, int asx, int asy, __global void *a, 
                     int bw, int bh, int bsx, int bsy, __global void *b)
#endif                     
{
    const int x = get_global_id(0);
    const int y = get_global_id(1);

        
#ifdef ARM_MALI //mali OpenCL
		uchar p0 = vxImagePixel_uchar(a, x, y, asx, asy);
		uchar p1;
		p1 = ~p0;
    vxImagePixel_uchar(b, x, y, bsx, bsy) = p1;
#else    
    vxImagePixel(uchar, b, x, y, bsx, bsy) = ~vxImagePixel(uchar, a, x, y, asx, asy);
#endif    
}

#endif
