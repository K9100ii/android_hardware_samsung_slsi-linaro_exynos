
#include <vx_cl.h>

#if defined(CL_USE_IMAGES)



#else

//[2016/03/09] jongseok,Park
//this kernel assume it's only for image_s16 format

#define INT16_MAX        32767

#ifdef ARM_MALI //mali OpenCL
__kernel void vx_magnitude(int aw, int ah, int asx, int asy, __global short *a, 
						 int bw, int bh, int bsx, int bsy, __global short *b,
						 int cw, int ch, int csx, int csy, __global short *c)
#else
__kernel void vx_magnitude(int aw, int ah, int asx, int asy, __global uchar *a, 
						 int bw, int bh, int bsx, int bsy, __global uchar *b,
						 int cw, int ch, int csx, int csy, __global uchar *c)
#endif						 
{
    const int x = get_global_id(0);
    const int y = get_global_id(1);

//	printf("magnitude : (%d,%d) :(%dx%d),(%dx%d) -> (%dx%d) \n", x, y, aw, ah, bw, bh, cw, ch);

#ifdef ARM_MALI //mali OpenCL
    short in_x = (signed short)vxImagePixel_short(a, x, y, asx/2, asy/2);
    short in_y = (signed short)vxImagePixel_short(b, x, y, bsx/2, bsy/2);    
#else
    short in_x = (signed short)vxImagePixel(short, a, x, y, asx, asy);
    short in_y = (signed short)vxImagePixel(short, b, x, y, bsx, bsy);    
#endif    

	double sum = (double)(in_x*in_x) + (double)(in_y*in_y);                
	int value = (int)(sqrt(sum) + 0.5);

#ifdef ARM_MALI //mali OpenCL
   vxImagePixel_short(c, x, y, csx/2, csy/2) = (short)(value > INT16_MAX ? INT16_MAX : value);
#else
   vxImagePixel(short, c, x, y, csx, csy) = (short)(value > INT16_MAX ? INT16_MAX : value);
#endif
              
}

#endif
