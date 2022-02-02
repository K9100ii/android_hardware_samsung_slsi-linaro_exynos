
#include <vx_cl.h>

 
#if defined(CL_USE_IMAGES)

__kernel void vx_box3x3(read_only image2d_t src, write_only image2d_t dst)
{
    const int x = get_global_id(0);
    const int y = get_global_id(1);
    uint4 sum = 0;
    sum += read_imageui(src, nearest_clamp, (int2)(x-1,y-1));
    sum += read_imageui(src, nearest_clamp, (int2)(x+0,y-1));
    sum += read_imageui(src, nearest_clamp, (int2)(x+1,y-1));
    sum += read_imageui(src, nearest_clamp, (int2)(x-1,y+0));
    sum += read_imageui(src, nearest_clamp, (int2)(x+0,y+0));
    sum += read_imageui(src, nearest_clamp, (int2)(x+1,y+0));
    sum += read_imageui(src, nearest_clamp, (int2)(x-1,y+1));
    sum += read_imageui(src, nearest_clamp, (int2)(x+0,y+1));
    sum += read_imageui(src, nearest_clamp, (int2)(x+1,y+1));
    sum /= 9;
    write_imageui(dst, (int2)(x,y), sum);
}

#else

#ifdef ARM_MALI //mali OpenCL
#pragma OPENCL EXTENSION cl_arm_printf : enable
__kernel void vx_box3x3(int sw, int sh, int ssx, int ssy, __global uchar *src,
                        int dw, int dh, int dsx, int dsy, __global uchar *dst)
#else
__kernel void vx_box3x3(int sw, int sh, int ssx, int ssy, __global uchar *src,
                        int dw, int dh, int dsx, int dsy, __global uchar *dst)
#endif                        
{
    const int x = get_global_id(0);
    const int y = get_global_id(1);

#ifdef ARM_MALI //mali OpenCL
    const int w = get_global_size(0);
    const int h = get_global_size(1);
#else    
    size_t w = get_global_size(0);
    size_t h = get_global_size(1);
#endif    

    uint sum = 0;
 	
    if (y == 0 || x == 0 || x == (w - 1) || y == (h - 1))
    {
        return; // border mode...
    }

#if 1 //jspark new code for using local variable
			#ifdef ARM_MALI //mali OpenCL
			//local uint p0,p1,p2,p3,p4,p5,p6,p7,p_c;  
			uint p0,p1,p2,p3,p4,p5,p6,p7,p_c;  	
			p0 = (uint)vxImagePixel_uchar(src, x-1, y-1, ssx, ssy);
			p1 = (uint)vxImagePixel_uchar(src, x-0, y-1, ssx, ssy);
			p2 = (uint)vxImagePixel_uchar(src, x+1, y-1, ssx, ssy);
			p3 = (uint)vxImagePixel_uchar(src, x-1, y+0, ssx, ssy);
			p_c = (uint)vxImagePixel_uchar(src, x+0, y+0, ssx, ssy);		
			p4 = (uint)vxImagePixel_uchar(src, x+1, y+0, ssx, ssy);
			p5 = (uint)vxImagePixel_uchar(src, x-1, y+1, ssx, ssy);
			p6 = (uint)vxImagePixel_uchar(src, x+0, y+1, ssx, ssy);
			p7 = (uint)vxImagePixel_uchar(src, x+1, y+1, ssx, ssy);	
			sum = (p0+p1+p2+p3+p4+p5+p6+p7+p_c)/9;  
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
			sum = (p0+p1+p2+p3+p4+p5+p6+p7+p_c)/9;  
			#endif

#else
    sum += (uint)vxImagePixel(uchar, src, x-1, y-1, ssx, ssy);
    sum += (uint)vxImagePixel(uchar, src, x+0, y-1, ssx, ssy);
    sum += (uint)vxImagePixel(uchar, src, x+1, y-1, ssx, ssy);
    sum += (uint)vxImagePixel(uchar, src, x-1, y+0, ssx, ssy);
    sum += (uint)vxImagePixel(uchar, src, x+0, y+0, ssx, ssy);
    sum += (uint)vxImagePixel(uchar, src, x+1, y+0, ssx, ssy);
    sum += (uint)vxImagePixel(uchar, src, x-1, y+1, ssx, ssy);
    sum += (uint)vxImagePixel(uchar, src, x+0, y+1, ssx, ssy);
    sum += (uint)vxImagePixel(uchar, src, x+1, y+1, ssx, ssy);
    sum /= 9;
#endif

		#ifdef ARM_MALI //mali OpenCL
    vxImagePixel_uchar(dst, x, y, dsx, dsy) = sum;
		#else
    vxImagePixel(uchar, dst, x, y, dsx, dsy) = sum;
		#endif    



}

#endif

