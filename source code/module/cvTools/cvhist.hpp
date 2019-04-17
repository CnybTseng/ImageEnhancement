#ifndef _CV_HIST_HPP_
#define _CV_HIST_HPP_

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <limits>
#include <cmath>

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

#if !defined(BITS_PER_BYTE)
#define BITS_PER_BYTE	8
#endif

/**
 * Calculate histogram of gray image.
 * @param[in] image Gray image.
 * @param[in] width Image width.
 * @param[in] height Image height.
 * @param[in] nbins Number of histogram bins.
 * @param[out] hist Gray histogram.
 * @return void.
 */
template <typename T>
void imhist(
	T *image,
	int32_t width,
	int32_t height,
	int32_t nbins,
	float *hist
)	{
	assert(image);
	assert(width > 0);
	assert(height > 0);
	assert(nbins > 0);
	assert(hist);
	
	int32_t npixels = width * height;
	const float norm_coef = 1.0f / npixels;
	int32_t ngray_levels = (std::numeric_limits<T>::max)();
	int32_t bin_step = std::ceil(ngray_levels / nbins);
	memset(hist, 0, nbins * sizeof(float));
	
	for (int i = 0; i < npixels; i++) {
		int32_t index = image[i] / bin_step;
		hist[index] += norm_coef;
	}
}

/**
 * Calculate cumulative histogram of gray image.
 * @param[in] hist Gray histogram.
 * @param[in] nbins Number of histogram bins.
 * @param[out] chist Cumulative histogram.
 * @return void.
 */
void cumhist(
	float *hist,
	uint32_t nbins,
	float *chist
)	{
	assert(hist);
	assert(nbins > 0);
	assert(chist);

	memset(chist, 0, nbins * sizeof(float));
	chist[0] = hist[0];
	
	for (int32_t i = 1; i < nbins; i++) {
		chist[i] += chist[i - 1] + hist[i];
	}
}

/**
 * Enhancement using histogram equalization.
 * @param[in] image Original gray image.
 * @param[in] width Image width.
 * @param[in] height Image height.
 * @param[out] equ_image Histogram equalized image.
 * @return void.
 */
template <typename T>
void histeq(
	T *image,
	int32_t width,
	int32_t height,
	T *equ_image
)	{
	assert(image);
	assert(width > 0);
	assert(height > 0);
	assert(equ_image);
	
	int32_t ngray_levels = (std::numeric_limits<T>::max)();
	const int32_t nbins = ngray_levels;
	float hist[nbins];
	imhist(image, width, height, nbins, hist);
	
	float chist[nbins];
	cumhist(hist, nbins, chist);
	
	T map[ngray_levels];
	for (int32_t i = 0; i < ngray_levels; i++) {
		map[i] = (T)(ngray_levels * chist[i]);
	}
	
	const int32_t npixels = width * height;
	for (int i = 0; i < npixels; i++) {
		equ_image[i] = map[image[i]];
	}
}

#endif