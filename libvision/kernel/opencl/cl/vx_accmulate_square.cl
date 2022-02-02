
#include <vx_cl.h>

#if defined(CL_USE_IMAGES)



#else

//[2016/03/09] jongseok,Park
//this kernel assume it's only for image_u8 format

#define INT16_MAX        32767

#ifdef ARM_MALI //mali OpenCL
__kernel void vx_accumulate_squre(int aw, int ah, int asx, int asy, __global uchar *a,
                    				 unsigned int shift_factor,	
									 int bw, int bh, int bsx, int bsy, __global short *b)
#else
__kernel void vx_accumulate_squre(int aw, int ah, int asx, int asy, __global void *a,
                    				 unsigned int shift_factor,	
									 int bw, int bh, int bsx, int bsy, __global void *b)
#endif									 
{
	const int x = get_global_id(0);
	const int y = get_global_id(1);

#ifdef ARM_MALI //mali OpenCL
	signed int s0 = (signed int)vxImagePixel_uchar(a, x, y, asx, asy);
	signed int s1 = (signed int)vxImagePixel_short(b, x, y, bsx/2, bsy/2);
#else
	int s0 = (int)vxImagePixel(uchar, a, x, y, asx, asy);
	int s1 = (int)vxImagePixel(short, b, x, y, bsx, bsy);
#endif	

	
	int r =  s1 + ((s0*s0)>>shift_factor);

	//overflow policy : saturation
	if (r >= INT16_MAX)
		r = INT16_MAX;

#ifdef ARM_MALI //mali OpenCL
    vxImagePixel_short(b, x, y, bsx/2, bsy/2) = (short)r;
#else
    vxImagePixel(short, b, x, y, bsx, bsy) = (short)r;
#endif    
    
}

#endif
