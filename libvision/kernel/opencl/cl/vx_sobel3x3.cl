#include <vx_cl.h>

/* Sobel Gradient Operators (for reference) 
    short gx[3][3] = {
        {-1, 0, 1},
        {-2, 0, 2},
        {-1, 0, 1}
    };
    short gy[3][3] = {
        {-1, -2, -1},
        { 0,  0,  0},
        { 1,  2,  1},
    };
*/

#if defined(__IMAGE_SUPPORT__) && defined(CL_USE_LUMINANCE)

__kernel void vx_sobel3x3(sampler_t sampler, 
                          read_only image2d_t src,
                          write_only image2d_t dst_gx,
                          write_only image2d_t dst_gy) 
{   
#if defined(CLOOPY)
    int x = get_global_id(0);
    int y = get_global_id(1);
    int4 sx = 0, sy = 0;
    for (int j = -1, k = 0; j <= 1; j++, k++) {
        for (int i = -1, l = 0; i <= 1; i++, l++) {
            uint4 pixel = read_imageui(src, sampler, (int2)(x+i,y+j));
            sx.s0 += gx[k][l] * pixel.s0;
            sy.s0 += gy[k][l] * pixel.s0;
        }
    }
    write_imagei(dst_gx, (int2)(x,y), sx);
    write_imagei(dst_gy, (int2)(x,y), sy);
#else
    int x = get_global_id(0);
    int y = get_global_id(1);
    short sx = 0;
    short sy = 0;
    
    sx -= read_imageui(src, sampler, (int2)(x-1,y-1)).s0;
    sx -= read_imageui(src, sampler, (int2)(x-1,y-0)).s0 * 2;
    sx -= read_imageui(src, sampler, (int2)(x-1,y+1)).s0;
    sx += read_imageui(src, sampler, (int2)(x+1,y-1)).s0;
    sx += read_imageui(src, sampler, (int2)(x+1,y-0)).s0 * 2;
    sx += read_imageui(src, sampler, (int2)(x+1,y+1)).s0;
    write_imagei(dst_gx, (int2)(x,y), (int4)(sx, sx, sx, 1));    
    sy -= read_imageui(src, sampler, (int2)(x-1,y-1)).s0;
    sy -= read_imageui(src, sampler, (int2)(x-0,y-1)).s0 * 2;
    sy -= read_imageui(src, sampler, (int2)(x+1,y-1)).s0;
    sy += read_imageui(src, sampler, (int2)(x-1,y+1)).s0;
    sy += read_imageui(src, sampler, (int2)(x-0,y+1)).s0 * 2;
    sy += read_imageui(src, sampler, (int2)(x+1,y+1)).s0;
    write_imagei(dst_gy, (int2)(x,y), (int4)(sy, sy, sy, 1));
#endif
}

#else



#if 1 //[2016/03/09] modified by jspark
//------------------------------------------------------------------------

#pragma OPENCL EXTENSION cl_arm_printf : enable

#ifdef ARM_MALI //mali OpenCL
__kernel void vx_sobel3x3(int sw, int sh, int ssx, int ssy, __global uchar *src,
                          int dgxw, int dgxh, int dgxsx, int dgxsy, __global short *gx,
                          int dgyw, int dgyh, int dgysx, int dgysy, __global short *gy                          
                        )
#else
__kernel void vx_sobel3x3(int sw, int sh, int ssx, int ssy, __global void *src,
                          int dgxw, int dgxh, int dgxsx, int dgxsy, __global void *gx,
                          int dgyw, int dgyh, int dgysx, int dgysy, __global void *gy                          
                        )
#endif                        
{
    const int x = get_global_id(0);
    const int y = get_global_id(1);


    const int w = get_global_size(0);
    const int h = get_global_size(1);

    int sx = 0, sy = 0;
#ifdef ARM_MALI //mali OpenCL
    uchar p0,p1,p2,p3,p4,p5,p6,p7;  //if we use local , data cannot have a same bit matching 
#else    
    local uchar p0,p1,p2,p3,p4,p5,p6,p7;
#endif    
    
    if (y == 0 || x == 0 || x == (w - 1) || y == (h - 1))
		  return; // border mode...

#if 1 //disable test
//----------
// p0 p1 p2
// p3    p4
// p5 p6 p7
//----------
	//get pixel data (we must read one time here)
#ifdef ARM_MALI //mali OpenCL
	p0 = vxImagePixel_uchar(src, x-1, y-1, ssx, ssy);
	p1 = vxImagePixel_uchar(src, x-0, y-1, ssx, ssy);
	p2 = vxImagePixel_uchar(src, x+1, y-1, ssx, ssy);
	
	p3 = vxImagePixel_uchar(src, x-1, y+0, ssx, ssy);
	p4 = vxImagePixel_uchar(src, x+1, y+0, ssx, ssy);
	
	p5 = vxImagePixel_uchar(src, x-1, y+1, ssx, ssy);
	p6 = vxImagePixel_uchar(src, x+0, y+1, ssx, ssy);
	p7 = vxImagePixel_uchar(src, x+1, y+1, ssx, ssy);	
#else	
	p0 = vxImagePixel(uchar, src, x-1, y-1, ssx, ssy);
	p1 = vxImagePixel(uchar, src, x-0, y-1, ssx, ssy);
	p2 = vxImagePixel(uchar, src, x+1, y-1, ssx, ssy);
	p3 = vxImagePixel(uchar, src, x-1, y+0, ssx, ssy);
	p4 = vxImagePixel(uchar, src, x+1, y+0, ssx, ssy);
	p5 = vxImagePixel(uchar, src, x-1, y+1, ssx, ssy);
	p6 = vxImagePixel(uchar, src, x+0, y+1, ssx, ssy);
	p7 = vxImagePixel(uchar, src, x+1, y+1, ssx, ssy);	
#endif
	  
    if (gx) {
    #if 1
    	sx = -1*(uint)p0 + 1*(uint)p2 -2*(uint)p3 + 2*(uint)p4 -1*(uint)p5  + 1*(uint)p7;
    #else
        sx -= (uint)vxImagePixel(uchar, src, x-1, y-1, ssx, ssy);
        sx -= (uint)vxImagePixel(uchar, src, x-1, y-0, ssx, ssy) * 2;
        sx -= (uint)vxImagePixel(uchar, src, x-1, y+1, ssx, ssy);
        sx += (uint)vxImagePixel(uchar, src, x+1, y-1, ssx, ssy);
        sx += (uint)vxImagePixel(uchar, src, x+1, y-0, ssx, ssy) * 2;
        sx += (uint)vxImagePixel(uchar, src, x+1, y+1, ssx, ssy);
	#endif

#ifdef ARM_MALI //mali OpenCL
		//if ((x==1) && (y==1)){
	  //printf("[OPENVX] SOBEL3x3 (%d,%d) (%d,%d,%d,%d,%d,%d,%d,%d)->(sx:%d),(%d,%d,%d,%d)\n",x,y, p0,p1,p2,p3,p4,p5,p6,p7, sx, dgxw, dgxh, dgxsx,dgxsx);
	  //}
		vxImagePixel_short(gx,x,y,dgxsx/2,dgxsy/2) = (short)sx;
#else	
		vxImagePixel(short,gx,x,y,dgxsx,dgxsy) = sx;
#endif		
    }
    
    if (gy) {
    #if 1
    	sy = -1*(uint)p0 -2*(uint)p1 -1*(uint)p2 + 1*(uint)p5 +2*(uint)p6  + 1*(uint)p7;
    #else    
	    //if we read pixel again as below, sy result cannot be generated,, i don't know why...
        sy -= (uint)vxImagePixel(uchar, src, x-1, y-1, ssx, ssy);
        sy -= (uint)vxImagePixel(uchar, src, x-0, y-1, ssx, ssy) * 2;
        sy -= (uint)vxImagePixel(uchar, src, x+1, y-1, ssx, ssy);
        sy += (uint)vxImagePixel(uchar, src, x-1, y-1, ssx, ssy);
        sy += (uint)vxImagePixel(uchar, src, x-0, y-1, ssx, ssy) * 2;
        sy += (uint)vxImagePixel(uchar, src, x+1, y-1, ssx, ssy);
	#endif

#ifdef ARM_MALI //mali OpenCL
		vxImagePixel_short(gy,x,y,dgysx/2,dgysy/2) = (short)sy;
#else	
		vxImagePixel(short,gy,x,y,dgysx,dgysy) = sy;
#endif		
    }   
#endif //end of disable 
 
}

//------------------------------------------------------------------------
#else

__kernel void vx_sobel3x3(
    __global vx_cl_imagepatch_addressing_t *sa,    __global void *src,
    __global vx_cl_imagepatch_addressing_t *gxa,    __global void *gx,
    __global vx_cl_imagepatch_addressing_t *gya,    __global void *gy)
{
    const int x = get_global_id(0);
    const int y = get_global_id(1);
    int sx = 0, sy = 0;
    if (gx) {
        sx -= (uint)vxImagePixel(uchar, src, x-1, y-1, sa);
        sx -= (uint)vxImagePixel(uchar, src, x-1, y-0, sa) * 2;
        sx -= (uint)vxImagePixel(uchar, src, x-1, y+1, sa);
        sx += (uint)vxImagePixel(uchar, src, x+1, y-1, sa);
        sx += (uint)vxImagePixel(uchar, src, x+1, y-0, sa) * 2;
        sx += (uint)vxImagePixel(uchar, src, x+1, y+1, sa);
        vxImagePixel(short, gx, x, y, gxa) = sx;
    }
    if (gy) {
        sy -= (uint)vxImagePixel(uchar, src, x-1, y-1, sa);
        sy -= (uint)vxImagePixel(uchar, src, x-0, y-1, sa) * 2;
        sy -= (uint)vxImagePixel(uchar, src, x+1, y-1, sa);
        sy += (uint)vxImagePixel(uchar, src, x-1, y-1, sa);
        sy += (uint)vxImagePixel(uchar, src, x-0, y-1, sa) * 2;
        sy += (uint)vxImagePixel(uchar, src, x+1, y-1, sa);
        vxImagePixel(short, gy, x, y, gya) = sy;
    }
}

#endif


#endif
