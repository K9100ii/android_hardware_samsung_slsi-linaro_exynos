
#include <vx_cl.h>


#if defined(CL_USE_IMAGES)

__kernel void vx_gaussian3x3(read_only image2d_t src, write_only image2d_t dst)
{
    const int x = get_global_id(0);
    const int y = get_global_id(1);
    uint4 sum = 0;
    sum += 1*read_imageui(src, nearest_clamp, (int2)(x-1,y-1));
    sum += 2*read_imageui(src, nearest_clamp, (int2)(x+0,y-1));
    sum += 1*read_imageui(src, nearest_clamp, (int2)(x+1,y-1));
    sum += 2*read_imageui(src, nearest_clamp, (int2)(x-1,y+0));
    sum += 4*read_imageui(src, nearest_clamp, (int2)(x+0,y+0));
    sum += 2*read_imageui(src, nearest_clamp, (int2)(x+1,y+0));
    sum += 1*read_imageui(src, nearest_clamp, (int2)(x-1,y+1));
    sum += 2*read_imageui(src, nearest_clamp, (int2)(x+0,y+1));
    sum += 1*read_imageui(src, nearest_clamp, (int2)(x+1,y+1));
    sum /= 16;
    write_imageui(dst, (int2)(x,y), sum);
}

#else

#pragma OPENCL EXTENSION cl_arm_printf : enable
#ifdef ARM_MALI //mali OpenCL
__kernel void vx_gaussian3x3(int sw, int sh, int ssx, int ssy, __global uchar *src,
                             int dw, int dh, int dsx, int dsy, __global uchar *dst)
#else
__kernel void vx_gaussian3x3(int sw, int sh, int ssx, int ssy, __global void *src,
                             int dw, int dh, int dsx, int dsy, __global void *dst)
#endif                             
{
    const int x = get_global_id(0);
    const int y = get_global_id(1);
    
#ifdef ARM_MALI //mali OpenCL
    const int w = get_global_size(0);
    const int h = get_global_size(1);
#else    
    const size_t w = get_global_size(0);
    const size_t h = get_global_size(1);
#endif    
    uint sum = 0;
    
    if (y == 0 || x == 0 || x == (w - 1) || y == (h - 1))
       return;

#if 1 //jspark new code for using local variable
    
		#ifdef ARM_MALI //mali OpenCL
    uint p0,p1,p2,p3,p4,p5,p6,p7,p_c;
		p0 = (uint)vxImagePixel_uchar(src, x-1, y-1, ssx, ssy);
		p1 = (uint)vxImagePixel_uchar(src, x-0, y-1, ssx, ssy);
		p2 = (uint)vxImagePixel_uchar(src, x+1, y-1, ssx, ssy);
		p3 = (uint)vxImagePixel_uchar(src, x-1, y+0, ssx, ssy);
		p4 = (uint)vxImagePixel_uchar(src, x+1, y+0, ssx, ssy);
		p5 = (uint)vxImagePixel_uchar(src, x-1, y+1, ssx, ssy);
		p6 = (uint)vxImagePixel_uchar(src, x+0, y+1, ssx, ssy);
		p7 = (uint)vxImagePixel_uchar(src, x+1, y+1, ssx, ssy);	
		p_c = (uint)vxImagePixel_uchar(src, x+0, y+0, ssx, ssy);	
		#else    
    local uint p0,p1,p2,p3,p4,p5,p6,p7,p_c;
		p0 = (uint)vxImagePixel(uchar, src, x-1, y-1, ssx, ssy);
		p1 = (uint)vxImagePixel(uchar, src, x-0, y-1, ssx, ssy);
		p2 = (uint)vxImagePixel(uchar, src, x+1, y-1, ssx, ssy);
		p3 = (uint)vxImagePixel(uchar, src, x-1, y+0, ssx, ssy);
		p4 = (uint)vxImagePixel(uchar, src, x+1, y+0, ssx, ssy);
		p5 = (uint)vxImagePixel(uchar, src, x-1, y+1, ssx, ssy);
		p6 = (uint)vxImagePixel(uchar, src, x+0, y+1, ssx, ssy);
		p7 = (uint)vxImagePixel(uchar, src, x+1, y+1, ssx, ssy);	
		p_c = (uint)vxImagePixel(uchar, src, x+0, y+0, ssx, ssy);	
		#endif
    
    sum = (1*p0 + 2*p1 + 1*p2 + 2*p3 + 4*p_c + 2*p4 + 1*p5 + 2*p6 + 1*p7)/16;
    
#else
    sum += 1*(uint)vxImagePixel(uchar, src, x-1, y-1, ssx, ssy);
    sum += 2*(uint)vxImagePixel(uchar, src, x+0, y-1, ssx, ssy);
    sum += 1*(uint)vxImagePixel(uchar, src, x+1, y-1, ssx, ssy);
    sum += 2*(uint)vxImagePixel(uchar, src, x-1, y+0, ssx, ssy);
    sum += 4*(uint)vxImagePixel(uchar, src, x+0, y+0, ssx, ssy);
    sum += 2*(uint)vxImagePixel(uchar, src, x+1, y+0, ssx, ssy);
    sum += 1*(uint)vxImagePixel(uchar, src, x-1, y+1, ssx, ssy);
    sum += 2*(uint)vxImagePixel(uchar, src, x+0, y+1, ssx, ssy);
    sum += 1*(uint)vxImagePixel(uchar, src, x+1, y+1, ssx, ssy);
    sum /= 16;
#endif    

#ifdef ARM_MALI //mali OpenCL
    vxImagePixel_uchar(dst, x, y, dsx, dsy) = (uchar)sum;
#else
    vxImagePixel(uchar, dst, x, y, dsx, dsy) = sum;
#endif  


}

#endif
