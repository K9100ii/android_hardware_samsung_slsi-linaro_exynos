/*
 * Samsung Exynos SoC series VPU driver
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#ifndef VISION_FOR_LINUX_BUFFER_HELPER_H_
#define VISION_FOR_LINUX_BUFFER_HELPER_H_

struct vs4l_buffer_config {
	struct vs4l_format_list		flist;
	struct vs4l_container_list	*clist;
};

struct vs4l_request_config {
	int				buffer_cnt;
	struct vs4l_buffer_config	incfg;
	struct vs4l_buffer_config	otcfg;
};


#endif /* VISION_FOR_LINUX_BUFFER_HELPER_H */ 
