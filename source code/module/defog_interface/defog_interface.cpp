#include <stdio.h>
#include <unistd.h>
#include "defog.h"
#ifndef __cplusplus  
#define __cplusplus 
#endif

#include "opencv2/opencv.hpp"
#include "defog_interface.h"

static defog defog_module;

void defog_module_init(unsigned char *raw_image, int width, int height)
{
	defog_module.init(raw_image, width, height);
}

void defog_module_in_out(unsigned char *image)
{
#ifndef PIPELINE
#ifdef DARK_PRIOR
	defog_module.process_yuv_dp(image);
#elif CONTRAST_ENHANCE
	defog_module.process_yuv_ce(image);
#endif
#else
#ifdef DARK_PRIOR
	defog_module.process_yuv_dp_pl(image);
#elif CONTRAST_ENHANCE	
	defog_module.process_yuv_ce_pl(image);
#endif
#endif
}

void defog_module_free()
{
	;
}

