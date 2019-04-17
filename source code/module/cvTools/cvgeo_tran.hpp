#ifndef _CVGEO_TRAN_H_
#define _CVGEO_TRAN_H_

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cassert>

#ifdef _OPENMP
#include <omp.h>
#endif

template <typename T>
void linear_resample(
	T *raw_image,
	int32_t width,
	int32_t height,
	float scale,
	T *processed_image
)	{
	assert(raw_image);
	assert(processed_image);
	
	const int32_t rs_width = static_cast<int32_t>(scale * width);
	const int32_t rs_height = static_cast<int32_t>(scale * height);
	const int32_t rss_width = static_cast<int32_t>(scale * (width - 1));
	const int32_t rss_height = static_cast<int32_t>(scale * (height - 1));
	
	#pragma omp parallel for
	for (int32_t y = 0; y < rss_height; y++) {
		float yy = y / scale;
		int32_t y1 = static_cast<int32_t>(yy);
		int32_t y2 = y1 + 1;
		for (int32_t x = 0; x < rss_width; x++) {
			float xx = x / scale;
			int32_t x1 = static_cast<int32_t>(xx);
			int32_t x2 = x1 + 1;

			T tlc_value = raw_image[y1 * width + x1];
			T trc_value = raw_image[y1 * width + x2];
			T llc_value = raw_image[y2 * width + x1];
			T lrc_value = raw_image[y2 * width + x2];
			
			float inter1 = (xx - x1) * tlc_value + (x2 - xx) * trc_value;
			float inter2 = (xx - x1) * llc_value + (x2 - xx) * lrc_value;
			float inter3 = (yy - y1) * inter1 + (y2 - yy) * inter2;
			processed_image[y * rs_width + x] = (T)inter3;
		}
	}
	
	for (int32_t y = 0; y < rs_height - rss_height; y++) {
		for (int32_t x = rss_width; x < rs_width; x++) {
			processed_image[y * rs_width + x] = raw_image[width - 1];
		}
	}
	
	for (int32_t y = rss_height; y < rs_height; y++) {
		for (int32_t x = 0; x < rs_width - rss_width; x++) {
			processed_image[y * rs_width + x] = raw_image[(height - 1) * width];
		}
	}
	
	for (int32_t y = rss_height; y < rs_height; y++) {
		for (int32_t x = rss_width; x < rs_width; x++) {
			processed_image[y * rs_width + x] = raw_image[(height - 1) * width + width - 1];
		}
	}
	
	for (int32_t y = rs_height - rss_height; y < rss_height; y++) {
		float yy = y / scale;
		int32_t y1 = static_cast<int32_t>(yy);
		int32_t y2 = y1 + 1;
		for (int32_t x = rss_width; x < rs_width; x++) {
			T top_value = raw_image[y1 * width + width - 1];
			T bottom_value = raw_image[y2 * width + width - 1];
			float inter = (yy - y1) * top_value + (y2 - yy) * bottom_value;
			processed_image[y * rs_width + x] = (T)inter;
		}
	}
	
	for (int32_t y = rss_height; y < rs_height; y++) {
		for (int32_t x = rs_width - rss_width; x < rss_width; x++) {
			float xx = x / scale;
			int32_t x1 = static_cast<int32_t>(xx);
			int32_t x2 = x1 + 1;
			T left_value = raw_image[(height - 1) * width + x1];
			T right_value = raw_image[(height - 1) * width + x2];
			float inter = (xx - x1) * left_value + (x2 - xx) * right_value;
			processed_image[y * rs_width + x] = (T)inter;
		}
	}
}

#endif