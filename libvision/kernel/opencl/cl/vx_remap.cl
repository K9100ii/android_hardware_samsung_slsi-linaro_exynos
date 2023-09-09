
#include <vx_cl.h>

#if defined(CL_USE_IMAGES)

//[2016/03/09] jongseok,Park
//this kernel assume it's only for image_u8 format

__kernel void vx_remap(__read_only image2d_t input,
		       int src_w, int src_h, int dst_w, int dst_h,
		       global float *table,
		       int type,
			   __write_only image2d_t output,
		       int border_mode, int border_constant)
{
#if 1 //intel PC
	const sampler_t sampler0 = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_NONE | CLK_FILTER_NEAREST;
	//const sampler_t sampler1 = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_NONE | CLK_FILTER_LINEAR;

	const int pX = get_global_id(0);
	const int pY = get_global_id(1);

	int width = get_image_width(input);
	int height = get_image_height(input);

	float srcX = *(table + pX*2 + pY*dst_w*2);
	float srcY = *(table + pX*2 + pY*dst_w*2 + 1);
	float4 color;

	if (border_mode == 0xc000) // VX_BORDER_UNDEFINED
	{
		color = read_imagef(input, sampler0, (float2)(srcX, srcY));
		write_imagef(output, (int2)(pX, pY), color);
	}
	else if (border_mode == 0xc001) // VX_BORDER_MODE_CONSTANT
	{
		if (srcX < 0 || srcX >= width || srcY < 0 || srcY >= height)
  			color = (float4)(border_constant/255.0f, border_constant/255.0f, border_constant/255.0f, 1.0f);
		else
			color = read_imagef(input, sampler0, (float2)(srcX, srcY));

		write_imagef(output, (int2)(pX, pY), color);
	}
#endif
}

#else

//[2016/03/09] jongseok,Park
//this kernel assume it's only for image_u8 format

#if 0
static VX_INLINE void *vxFormatMemoryPtr(vx_memory_t *memory,
                                          vx_uint32 c,
                                          vx_uint32 x,
                                          vx_uint32 y,
                                          vx_uint32 p)
{
    intmax_t offset = (memory->strides[p][VX_DIM_Y] * y) +
                      (memory->strides[p][VX_DIM_X] * x) +
                      (memory->strides[p][VX_DIM_C] * c);
    void *ptr = (void *)&memory->ptrs[p][offset];
    //vxPrintMemory(memory);
    //VX_PRINT(VX_ZONE_INFO, "&(%p[%zu]) = %p\n", memory->ptrs[p], offset, ptr);
    return ptr;
}

void vxGetRemapPoint(vx_remap remap, int dst_x, int dst_y, float *src_x, float *src_y)
{
    if ((dst_x < remap->dst_width) &&
        (dst_y < remap->dst_height))
    {
        vx_float32 *coords[] = {
             vxFormatMemoryPtr(&remap->memory, 0, dst_x, dst_y, 0),
             vxFormatMemoryPtr(&remap->memory, 1, dst_x, dst_y, 0),
        };
        *src_x = *coords[0];
        *src_y = *coords[1];
        remap->base.read_count++;
    }
}
#endif


#ifdef ARM_MALI //mali OpenCL
__kernel void vx_remap(int aw, int ah, int asx, int asy, __global uchar *a,
					   int src_w, int src_h, int dst_w, int dst_h, __global float *tbl,
					   int type,
                       int bw, int bh, int bsx, int bsy, __global uchar *b)
#else
__kernel void vx_remap(int aw, int ah, int asx, int asy, __global void *a,
					   int src_w, int src_h, int dst_w, int dst_h, __global void *remap_tbl,
					   int type,
                       int bw, int bh, int bsx, int bsy, __global void *b)
#endif
{

    const int x = get_global_id(0);
    const int y = get_global_id(1);

#ifdef ARM_MALI //mali OpenCL
	float src_x =  *(tbl + x*2 + y*dst_w*2);
	float src_y =  *(tbl + x*2 + y*dst_w*2 + 1);
#else
	float *tbl = (float *)remap_tbl;
	float src_x =  *(float *)(tbl + x*2 + y*dst_w*2);
	float src_y =  *(float *)(tbl + x*2 + y*dst_w*2 + 1);
#endif

//	printf("(%d,%d) : (type:%d) src_x,y=(%f,%f) (src:%dx%d, dst:%dx%d)\n", x, y, type, src_x, src_y,  src_w,src_h,dst_w,dst_h);

	if (type == 0x4000) //VX_INTERPOLATION_TYPE_NEAREST_NEIGHBOR
	{
#ifdef ARM_MALI //mali OpenCL
	    vxImagePixel_uchar(b, x, y, bsx, bsy) = vxImagePixel_uchar(a, (int)src_x, (int)src_y, asx, asy);
#else
	    vxImagePixel(uchar, b, x, y, bsx, bsy) = vxImagePixel(uchar, a, (int)src_x, (int)src_y, asx, asy);
#endif
    }
	else  //VX_INTERPOLATION_TYPE_BILINEAR
	{
		//TODO later
	}

}


#endif
