#ifndef _DEFOG_INTERFACE_H_
#define _DEFOG_INTERFACE_H_

#ifdef __cplusplus
extern "C"
{
#endif

void defog_module_init(unsigned char *raw_image, int width, int height);

void defog_module_in_out(unsigned char *image);

void defog_module_free();

#ifdef __cplusplus
}
#endif

#endif