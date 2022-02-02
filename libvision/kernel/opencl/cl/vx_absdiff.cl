
#include <vx_cl.h>

#if defined(CL_USE_IMAGES)



#else

//[2016/03/09] jongseok,Park
//this kernel assume it's only for image_u8 format

#ifdef ARM_MALI //mali OpenCL
__kernel void vx_absdiff(int aw, int ah, int asx, int asy, __global uchar *a, 
                     int bw, int bh, int bsx, int bsy, __global uchar *b,
                     int cw, int ch, int csx, int csy, __global uchar *c)
#else
__kernel void vx_absdiff(int aw, int ah, int asx, int asy, __global void *a, 
                     int bw, int bh, int bsx, int bsy, __global void *b,
                     int cw, int ch, int csx, int csy, __global void *c)
#endif                     
{
   int x = get_global_id(0);
   int y = get_global_id(1);

#ifdef ARM_MALI //mali OpenCL
   uchar s0 = (int)vxImagePixel_uchar(a, x, y, asx, asy);
   uchar s1 = (int)vxImagePixel_uchar(b, x, y, bsx, bsy);

#else
   uchar s0 = (int)vxImagePixel(uchar, a, x, y, asx, asy);
   uchar s1 = (int)vxImagePixel(uchar, b, x, y, bsx, bsy);
#endif   
   uchar out;

   if (s0 > s1)
	   out = s0 - s1;
   else	
	   out = s1 - s0;
	   
#ifdef ARM_MALI //mali OpenCL	   
   vxImagePixel_uchar(c, x, y, csx, csy) = out;
#else
   vxImagePixel(uchar, c, x, y, csx, csy) = out;
#endif
}

#endif
