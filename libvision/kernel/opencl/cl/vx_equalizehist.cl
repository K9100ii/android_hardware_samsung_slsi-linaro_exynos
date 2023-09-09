
#include <vx_cl.h>

#if defined(CL_USE_IMAGES)


#else

//[2016/03/09] jongseok,Park
//this kernel assume it's only for image_u8 format

#define NUM_BINS 256

__kernel void vx_equalizehist(int sw, int sh, int ssx, int ssy, __global uchar *src,
							  int dw, int dh, int dsx, int dsy, __global uchar *dst)
{

    const int x = get_global_id(0);
    const int y = get_global_id(1);
    const size_t w = get_global_size(0);
    const size_t h = get_global_size(1);
    int i;
    uint hist[NUM_BINS];  //the pixel distribution (histogram)
    uint cdf[NUM_BINS];   //the cumulative distribution
	uchar pix;
	
	//inital distribution array
	for (i = 0; i < NUM_BINS; i++)
	{
		hist[i]=0; 
		cdf[i]=0;		
	}
	barrier(CLK_LOCAL_MEM_FENCE);
	
	//calculate histogram distribution	
#ifdef ARM_MALI //mali OpenCL	
	pix = vxImagePixel_uchar(src, x, y, ssx, ssy);
#else
	pix = vxImagePixel(uchar, src, x, y, ssx, ssy);
#endif	
	hist[pix]++;
		
	barrier(CLK_LOCAL_MEM_FENCE);
	
//	printf("(equalizehist)(%d,%d)-dist:%d,%d,%d \n", x, y, hist[0], hist[1], hist[2]);



}
#endif
