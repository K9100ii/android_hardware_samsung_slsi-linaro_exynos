
#include <vx_cl.h>

#if defined(CL_USE_IMAGES)



#else

//[2016/03/09] jongseok,Park
//this kernel assume it's only for image_u8 format

#ifdef ARM_MALI //mali OpenCL
__kernel void vx_accumulate_weighted(int aw, int ah, int asx, int asy, __global uchar *a,
                    				 float weight_factor,	
									 int bw, int bh, int bsx, int bsy, __global uchar *b)
#else
__kernel void vx_accumulate_weighted(int aw, int ah, int asx, int asy, __global void *a,
                    				 float weight_factor,	
									 int bw, int bh, int bsx, int bsy, __global void *b)
#endif									 

{
	int x = get_global_id(0);
	int y = get_global_id(1);

#ifdef ARM_MALI //mali OpenCL
	unsigned short s0 = (unsigned short)vxImagePixel_uchar(a, x, y, asx, asy);
	unsigned short s1 = (unsigned short)vxImagePixel_uchar(b, x, y, bsx, bsy);

#else
	unsigned short s0 = (unsigned short)vxImagePixel(uchar, a, x, y, asx, asy);
	unsigned short s1 = (unsigned short)vxImagePixel(uchar, b, x, y, bsx, bsy);
#endif	

    uchar r = (uchar)((1 - weight_factor)*s1 + (weight_factor)*s0);

#ifdef ARM_MALI //mali OpenCL
    vxImagePixel_uchar(b, x, y, bsx, bsy) = r;
#else
    vxImagePixel(uchar, b, x, y, bsx, bsy) = r;
#endif    
}
#endif
