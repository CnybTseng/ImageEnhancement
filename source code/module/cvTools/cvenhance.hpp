#ifndef _CVENHENCE_H_
#define _CVENHENCE_H_

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <limits>
#include <cmath>

#ifdef _OPENMP
#include <omp.h>
#endif

#ifdef _MSC_VER
typedef char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
#else
#include <cstdint>
#endif

/**
 * Basical gray transformation using gamma transformation.
 * @param[in] image Original gray image.
 * @param[in] width Image width.
 * @param[in] height Image height.
 * @param[in] gamma Transformation exponent.
 * @param[out] adj_image Gamma transformed image.
 * @return void.
 */
template <typename T>
void imadjust(
	T *image,
	int32_t width,
	int32_t height,
	float gamma,
	T *adj_image
)	{
	assert(image);
	assert(width > 0);
	assert(height > 0);
	assert(gamma > 0);
	assert(adj_image);
	
	T min_val = (std::numeric_limits<T>::max)();
	T max_val = (std::numeric_limits<T>::min)();
	const int32_t npixels = width * height;
	for (int32_t i = 0; i < npixels; i++) {
		min_val = std::min<T>(min_val, image[i]);
		max_val = std::max<T>(max_val, image[i]);
	}
	
	float scale = 1.0f / (max_val - min_val);
	const T ceiling = (std::numeric_limits<T>::max)();
	for (int32_t i = 0; i < npixels; i++) {
		float norm_val = scale * (image[i] - min_val);
		adj_image[i] = (T)(pow(norm_val, gamma) * ceiling);
	}
}

/**
 * Auto stretch image.
 * @param[in] raw_image Raw image.
 * @param[in] width Image width.
 * @param[in] height Image height.
 * @param[in] mean Gray mean.
 * @param[in] stdev Gray standard deviation.
 * @param[in] floor Stretch gray value floor.
 * @param[in] ceiling Stretch image gray value ceiling.
 * @param[out] processed_image Processed image.
 * @return void.
 */
void auto_stretch(
	uint8_t *raw_image,
	int32_t width,
	int32_t height,
	float mean,
	float stdev,
	uint8_t floor,
	uint8_t ceiling,
	uint8_t *processed_image
)	{
	assert(raw_image);
	assert(processed_image);
	
	const int32_t npixels = width * height;
	const int32_t smaller = mean - 3 * stdev;
	const int32_t bigger = mean + 3 * stdev;
	const float scale = (ceiling - floor) / (6 * stdev);
	const float bias = floor - scale * (mean + 3 * stdev);
	const int32_t nlevels = 256;
	uint8_t stretch_table[nlevels];
	
	for (int32_t i = 0; i < nlevels; i++) {
		stretch_table[i] = (uint8_t)(scale * i + bias);
	}
	
	#pragma omp parallel for
	for (int32_t i = 0; i < npixels; i++) {
		if (raw_image[i] < smaller) {
			processed_image[i] = floor;
		} else if (raw_image[i] < bigger) {
			processed_image[i] = stretch_table[raw_image[i]];
		} else {
			processed_image[i] = ceiling;
		}
	}
}

#endif