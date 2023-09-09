
#include <vx_cl.h>

#if defined(CL_USE_IMAGES)

//[2016/03/09] jongseok,Park
//this kernel assume it's only for image_u8 format

__kernel void vx_and(read_only image2d_t a, read_only image2d_t b, write_only image2d_t c) 
{
   //int2 coord = (get_global_id(0), get_global_id(1));
   //write_imageui(c, coord, (read_imageui(a, nearest_clamp, coord) & read_imageui(b, nearest_clamp, coord)));

#if 1 //intel PC
    int x = get_global_id(0);
    int y = get_global_id(1);
    
	#if 1    // for CL_LUMINANCE (0x10b9) : CL_UNORM_INT8 (0x10d2) 
		float p0 = read_imagef(a, nearest_clamp, (int2)(x,y)).x;
		float p1 = read_imagef(b, nearest_clamp, (int2)(x,y)).x;

		//   printf("(%d,%d) = (%f %f) \n", x,y, p0, p1);

		float out = (float)((uchar)(255*p0) & (uchar)(255*p1));

	    write_imagef(c, (int2)(x,y), (float4)(out/255, 0, 0, 0));
	#endif
#endif

}

#else


//[2016/03/09] jongseok,Park
//this kernel assume it's only for image_u8 format
//check done

#pragma OPENCL EXTENSION cl_arm_printf : enable
#ifdef ARM_MALI //mali OpenCL
__kernel void vx_and(int aw, int ah, int asx, int asy, __global uchar *a,
                     int bw, int bh, int bsx, int bsy, __global uchar *b,
                     int cw, int ch, int csx, int csy, __global uchar *c)
#else
__kernel void vx_and(int aw, int ah, int asx, int asy, __global void *a,
                     int bw, int bh, int bsx, int bsy, __global void *b,
                     int cw, int ch, int csx, int csy, __global void *c)
#endif                     
{
    const int x = get_global_id(0);
    const int y = get_global_id(1);


#ifdef ARM_MALI //mali OpenCL

    vxImagePixel_uchar(c, x, y, csx, csy) = vxImagePixel_uchar(a, x, y, asx, asy) & vxImagePixel_uchar(b, x, y, bsx, bsy);

//    const int w = get_global_size(0);
//    const int h0 = get_global_size(1);
//    const int h1 = get_global_size(2);
//		uchar p0 = vxImagePixel_uchar(a, x, y, asx, asy);
//		uchar p1 = vxImagePixel_uchar(b, x, y, bsx, bsy);
//		uchar p2 = p0 & p1;
//	  printf("[OPENVX] AND (%d,%d) : %d & %d -> %d (w:%d,h0:%d,h1:%d),(aw:%d,ah:%d,asx:%d,asy:%d) \n",x,y,p0,p1,p2, w,h0,h1,aw,ah,asx,asy);
//		vxImagePixel_uchar(c, x, y, csx, csy) = p2;
		
#else    
    vxImagePixel(uchar, c, x, y, csx, csy) = vxImagePixel(uchar, a, x, y, asx, asy) & vxImagePixel(uchar, b, x, y, bsx, bsy);    
#endif



}
#endif
