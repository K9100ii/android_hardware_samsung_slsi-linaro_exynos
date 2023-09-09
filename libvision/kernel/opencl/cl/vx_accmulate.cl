
#include <vx_cl.h>

#if defined(CL_USE_IMAGES)



#else

//[2016/03/09] jongseok,Park
//this kernel assume it's only for image_u8 format

#define INT16_MAX        32767

#ifdef ARM_MALI //mali OpenCL
__kernel void vx_accumulate(int aw, int ah, int asx, int asy, __global uchar *a,
                    		int bw, int bh, int bsx, int bsy, __global uchar *b)

#else
__kernel void vx_accumulate(int aw, int ah, int asx, int asy, __global void *a,
                    		int bw, int bh, int bsx, int bsy, __global void *b)
#endif                    		
{
    int x = get_global_id(0);
    int y = get_global_id(1);

#ifdef ARM_MALI //mali OpenCL
	short s0 = (short)vxImagePixel_uchar(a, x, y, asx, asy);
	short s1 = (short)vxImagePixel_uchar(b, x, y, bsx, bsy);

#else
	short s0 = (short)vxImagePixel(uchar, a, x, y, asx, asy);
	short s1 = (short)vxImagePixel(short, b, x, y, bsx, bsy);
#endif	
	short r = s0 + s1;

	//overflow policy : saturation
	if (r >= INT16_MAX)
		r = INT16_MAX;
		
#ifdef ARM_MALI //mali OpenCL
    vxImagePixel_uchar(b, x, y, bsx, bsy) = r;
#else
    vxImagePixel(short, b, x, y, bsx, bsy) = r;
#endif    
}

#endif
