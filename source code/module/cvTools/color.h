#ifndef _COLOR_H_
#define _COLOR_H_

#include <cstdint>

/**
 * Channel split thread.
 * @param[in,out] param Parameter.
 * @return void.
 */
void *channel_split_thread(
	void *param
);

/**
 * RGB channel split.
 * @param[in] rgb_image RGB image.
 * @param[in] width Image width.
 * @param[in] height Image height.
 * @param[out] red_image Red channel image.
 * @param[out] green_image Green channel image.
 * @param[out] blue_image Blue channel image.
 * @return void.
 */
void channel_split(
	uint8_t *rgb_image,
	int32_t width,
	int32_t height,
	uint8_t *red_image,
	uint8_t *green_image,
	uint8_t *blue_image
);

/**
 * Neon version color channel split.
 * @param[in] bgr_image BGR image.
 * @param[in] width Image width.
 * @param[in] height Image height.
 * @param[out] red_image Red channel image.
 * @param[out] green_image Green channel image.
 * @param[out] blue_image Blue channel image.
 * @return void. 
 */
void neon_channel_split(
	uint8_t *bgr_image,
	int32_t width,
	int32_t height,
	uint8_t *red_image,
	uint8_t *green_image,
	uint8_t *blue_image
);

/**
 * Convert YV12 to BGR24 image array.
 * @param[in] yuv_image YUV image.
 * @param[out] blue_image Blue image.
 * @param[out] green_image Green image.
 * @param[out] red_image Red image.
 * @param[in] width Image width.
 * @param[in] height Image height.
 * @return void.
 */
void yv12_2bgr24(
	uint8_t* yuv_image,
	uint8_t* blue_image,
	uint8_t* green_image,
	uint8_t* red_image,
	int32_t width,
	int32_t height
);

#endif