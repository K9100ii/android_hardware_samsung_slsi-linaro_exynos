
#include <vx_cl.h>

#if defined(CL_USE_IMAGES)

__kernel void vx_single_threshold(read_only image2d_t in, uchar value, write_only image2d_t out) {
    int2 coord = (int2)(get_global_id(0), get_global_id(1));
    uint4 a = read_imageui(in, nearest_clamp, coord);
    uint4 b = clamp(a, value-1u, 255u);
    uint4 c = 0u;
}

#else

//[2016/03/09] jongseok,Park
//this kernel assume it's only for image_u8 format

#ifdef ARM_MALI //mali OpenCL
__kernel void vx_threshold(int aw, int ah, int asx, int asy, __global uchar *a,
                     			 uchar value,	
                     			 uchar upper,	
                     			 uchar lower,	                     			 
			                     int bw, int bh, int bsx, int bsy, __global uchar *b)                     
#else
__kernel void vx_threshold(int aw, int ah, int asx, int asy, __global void *a,
                     			 uchar value,	
                     			 uchar upper,	
                     			 uchar lower,	                     			 
			                     int bw, int bh, int bsx, int bsy, __global void *b)                     
#endif			                     
{
    int x = get_global_id(0);
    int y = get_global_id(1);

#ifdef ARM_MALI //mali OpenCL
 	uchar s0 = vxImagePixel_uchar(a, x, y, asx, asy);
#else
 	uchar s0 = vxImagePixel(uchar, a, x, y, asx, asy);
#endif 	
 	uchar out = s0;

 	if (value) //VX_THRESHOLD_TYPE_BINARY
 	{
		if (out > value)
			out = 255;
		else
			out = 0;		
	}
	else //VX_THRESHOLD_TYPE_RANGE
	{
		if (out > upper)
			out = 255;
		else if (out < lower)
			out = 0;
		else
			out = 255;		
			
	}

#ifdef ARM_MALI //mali OpenCL
	vxImagePixel_uchar(b, x, y, bsx, bsy) = out;	
#else	
	vxImagePixel(uchar, b, x, y, bsx, bsy) = out;	
#endif	
}


#endif
