
#include <vx_cl.h>

#if defined(CL_USE_IMAGES)

//[2016/03/09] jongseok,Park

#pragma OPENCL EXTENSION cl_arm_printf : enable

__kernel void vx_warp_perspective(__read_only image2d_t input,
			 	  int cols, int rows,
				  __constant float *perspective,
				  uint type,
				  write_only image2d_t output,
				  int border_mode, int border_constant)
{
	const sampler_t sampler0 = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_NONE | CLK_FILTER_NEAREST;
	//const sampler_t sampler1 = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_NONE | CLK_FILTER_LINEAR;

	const int pX = get_global_id(0);
	const int pY = get_global_id(1);

	int width = get_image_width(input);
	int height = get_image_height(input);

	float z = pX * perspective[6] + pY * perspective[7] + perspective[8];
	float srcX = (pX * perspective[0] + pY * perspective[1] + perspective[2]) / z;
	float srcY = (pX * perspective[3] + pY * perspective[4] + perspective[5]) / z;

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
}

#else

//[2016/03/09] jongseok,Park
//this kernel assume it's only for image_u8 format

#ifdef ARM_MALI //mali OpenCL
#else
static void transform_perspective(unsigned int dst_x, unsigned int dst_y, float m[], float *src_x, float *src_y)
{
	float z = dst_x * m[2] + dst_y * m[5] + m[8];
	*src_x = (dst_x * m[0] + dst_y * m[3] + m[6]) / z;
	*src_y = (dst_x * m[1] + dst_y * m[4] + m[7]) / z;
}
#endif

#ifdef ARM_MALI //mali OpenCL
__kernel void vx_warp_perspective(int aw, int ah, int asx, int asy, __global uchar *a,
							int cols, int rows,	__global float *mat,
                   			 uint type,
		                     int bw, int bh, int bsx, int bsy, __global uchar *b)
#else
__kernel void vx_warp_perspective(int aw, int ah, int asx, int asy, __global void *a,
							int cols, int rows,	__global void *m,
                   			 uint type,
		                     int bw, int bh, int bsx, int bsy, __global void *b)
#endif
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    int w = get_global_size(0);
    int h = get_global_size(1);

#ifdef ARM_MALI //mali OpenCL
  float z = x * mat[2] + y * mat[5] + mat[8];
	float xf= (x * mat[0] + y * mat[3] + mat[6]) / z;
	float yf= (x * mat[1] + y * mat[4] + mat[7]) / z;
#else
	float *mat = (float *)m;
  float xf,yf;
  transform_perspective(x, y, mat, &xf, &yf);
#endif


//	printf("%d x %d with type(0x%x) : %f,%f,%f / %f,%f,%f \n", cols,rows,type, mat[0],	mat[1],	mat[2],	mat[3],	mat[4],	mat[5]);
//	printf("src : (%f,%f) \n",xf,yf);

	int sx = (unsigned int)xf;
	int sy = (unsigned int)yf;
	uchar out = 0;
	if ( (sx >= 0) && (sy >= 0) && (sx < w) && (sy < h)) {
#ifdef ARM_MALI //mali OpenCL
			out = vxImagePixel_uchar(a, sx, sy, asx, asy);
#else
   	 	out = vxImagePixel(uchar, a, sx, sy, asx, asy);
#endif
  }

#ifdef ARM_MALI //mali OpenCL
	vxImagePixel_uchar(b, x, y, bsx, bsy) = out;
#else
	vxImagePixel(uchar, b, x, y, bsx, bsy) = out;
#endif
}

#endif
