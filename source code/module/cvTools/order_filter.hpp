#ifndef _ORDER_FILTER_H_
#define _ORDER_FILTER_H_

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <ctime>
#include <cassert>
#include <deque>
#include <limits>

#ifdef _OPENMP
#include <omp.h>
#endif

template <typename T>
void min_filter(
	T *raw_image,
	int32_t width,
	int32_t height,
	int32_t ksize,
	T *processed_image
)	{
	assert(raw_image);
	assert(processed_image);
	
	T *cache_image = new T[width * height];
	assert(cache_image);

	// Row minimum filter for central region.
	const int32_t kradii = ksize / 2;
	#pragma omp parallel for
	for (int32_t y = 0; y < height; y++) {
		std::deque<int32_t> L;
		T *data = raw_image + y * width;
		T *todata = cache_image + y * width + kradii;
		for (int32_t x = 1; x < width; x++) {
			if (x >= ksize) {
				todata[x - ksize] = data[L.size() > 0 ? L.front() : x - 1];
			}
			if (data[x] > data[x - 1]) {
				L.push_back(x - 1);
				if (x == ksize + L.front()) {
					L.pop_front();
				}
			} else {
				while (L.size() > 0) {
					if (data[x] >= data[L.back()]) {
						if (x == ksize + L.front()) {
							L.pop_front();
						}
						break;
					}
					L.pop_back();
				}
			}
		}
		todata[width - ksize] = data[L.size() > 0 ? L.front() : width - 1];
	}

	// Column minimum filter for central region.
	#pragma omp parallel for
	for (int32_t x = 0; x < width; x++) {
		std::deque<int32_t> L;
		T *data = cache_image;
		T *todata = processed_image + kradii * width;
		for (int32_t y = 1; y < height; y++) {
			if (y >= ksize) {
				todata[(y - ksize) * width + x] = data[L.size() > 0 ? L.front() : ((y - 1) * width + x)];
			}
			if (data[y * width + x] > data[(y - 1) * width + x]) {
				L.push_back((y - 1) * width + x);
				if (y * width + x == ksize * width + L.front()) {
					L.pop_front();
				}
			} else {
				while (L.size() > 0) {
					if (data[y * width + x] >= data[L.back()]) {
						if (y * width + x == ksize * width + L.front()) {
							L.pop_front();
						}
						break;
					}
					L.pop_back();
				}
			}
		}
		todata[(height - ksize) * width + x] = data[L.size() > 0 ? L.front() : ((height - 1) * width + x)];
	}
	
	for (int32_t y = 0; y < kradii; y++) {
		// Top left region.
		for (int32_t x = 0; x < kradii; x++) {
			T min_val = (std::numeric_limits<T>::max)();
			for (int32_t ky = 0; ky < y + kradii; ky++) {
				for (int32_t kx = 0; kx < x + kradii; kx++) {
					min_val = std::min(min_val, raw_image[ky * width + kx]);
				}
			}
			processed_image[y * width + x] = min_val;
		}
		// Top right region.
		for (int32_t x = width - kradii; x < width; x++) {
			T min_val = (std::numeric_limits<T>::max)();
			for (int32_t ky = 0; ky < y + kradii; ky++) {
				for (int32_t kx = x - kradii; kx < width; kx++) {
					min_val = std::min(min_val, raw_image[ky * width + kx]);
				}
			}
			processed_image[y * width + x] = min_val;
		}
	}
	
	for (int32_t y = height - kradii; y < height; y++) {
		// Lower left region.
		for (int32_t x = 0; x < kradii; x++) {
			T min_val = (std::numeric_limits<T>::max)();
			for (int32_t ky = y - kradii; ky < height; ky++) {
				for (int32_t kx = 0; kx < x + kradii; kx++) {
					min_val = std::min(min_val, raw_image[ky * width + kx]);
				}
			}
			processed_image[y * width + x] = min_val;
		}
		// Lower right region.
		for (int32_t x = width - kradii; x < width; x++) {
			T min_val = (std::numeric_limits<T>::max)();
			for (int32_t ky = y - kradii; ky < height; ky++) {
				for (int32_t kx = x - kradii; kx < width; kx++) {
					min_val = std::min(min_val, raw_image[ky * width + kx]);
				}
			}
			processed_image[y * width + x] = min_val;
		}
	}
	
	for (int32_t x = kradii; x < width - kradii; x++) {
		// Top central region.
		for (int32_t y = 0; y < kradii; y++) {
			T min_val = (std::numeric_limits<T>::max)();
			for (int32_t ky = 0; ky < y + kradii; ky++) {
				for (int32_t kx = x - kradii; kx < x + kradii; kx++) {
					min_val = std::min(min_val, raw_image[ky * width + kx]);
				}
			}
			processed_image[y * width + x] = min_val;
		}
		// Lower central region.
		for (int32_t y = height - kradii; y < height; y++) {
			T min_val = (std::numeric_limits<T>::max)();
			for (int32_t ky = y - kradii; ky < height; ky++) {
				for (int32_t kx = x - kradii; kx < x + kradii; kx++) {
					min_val = std::min(min_val, raw_image[ky * width + kx]);
				}
			}
			processed_image[y * width + x] = min_val;
		}
	}
	
	for (int32_t y = kradii; y < height - kradii; y++) {
		// Left central region.
		for (int32_t x = 0; x < kradii; x++) {
			T min_val = (std::numeric_limits<T>::max)();
			for (int32_t ky = y - kradii; ky < y + kradii; ky++) {
				for (int32_t kx = 0; kx < x + kradii; kx++) {
					min_val = std::min(min_val, raw_image[ky * width + kx]);
				}
			}
			processed_image[y * width + x] = min_val;
		}
		// Right central region.
		for (int32_t x = width - kradii; x < width; x++) {
			T min_val = (std::numeric_limits<T>::max)();
			for (int32_t ky = y - kradii; ky < y + kradii; ky++) {
				for (int32_t kx = width - kradii; kx < width; kx++) {
					min_val = std::min(min_val, raw_image[ky * width + kx]);
				}
			}
			processed_image[y * width + x] = min_val;
		}
	}

	if (cache_image) {
		delete [] cache_image;
		cache_image = 0;
	}
}

template <typename T>
void max_filter(
	T *raw_image,
	int32_t width,
	int32_t height,
	int32_t ksize,
	T *processed_image
)	{
	assert(raw_image);
	assert(processed_image);
	
	T *cache_image = new T[width * height];
	assert(cache_image);
	
	// Row maximum filter for central region.
	const int32_t kradii = ksize / 2;
	for (int32_t y = 0; y < height; y++) {
		std::deque<int32_t> U;
		T *data = raw_image + y * width;
		T *todata = cache_image + y * width + kradii;
		for (int32_t x = 1; x < width; x++) {
			if (x >= ksize) {
				todata[x - ksize] = data[U.size() > 0 ? U.front() : x - 1];
			}
			if (data[x] > data[x - 1]) {
				while (U.size() > 0) {
					if (data[x] <= data[U.back()]) {
						if (x == ksize + U.front()) {
							U.pop_front();
						}
						break;
					}
					U.pop_back();
				}
			} else {
				U.push_back(x - 1);
				if (x == ksize + U.front()) {
					U.pop_front();
				}
			}
		}
		todata[width - ksize] = data[U.size() > 0 ? U.front() : width - 1];
	}
	
	// Column maximum filter for central region.
	for (int32_t x = 0; x < width; x++) {
		std::deque<int32_t> U;
		std::deque<int32_t> L;
		T *data = cache_image;
		T *todata = processed_image + kradii * width;
		for (int32_t y = 1; y < height; y++) {
			if (y >= ksize) {
				todata[(y - ksize) * width + x] = data[U.size() > 0 ? U.front() : ((y - 1) * width + x)];
			}
			if (data[y * width + x] > data[(y - 1) * width + x]) {
				while (U.size() > 0) {
					if (data[y * width + x] <= data[U.back()]) {
						if (y * width + x == ksize * width + U.front()) {
							U.pop_front();
						}
						break;
					}
					U.pop_back();
				}
			} else {
				U.push_back((y - 1) * width + x);
				if (y * width + x == ksize * width + U.front()) {
					U.pop_front();
				}
			}
		}
		todata[(height - ksize) * width + x] = data[U.size() > 0 ? U.front() : ((height - 1) * width + x)];
	}
	
	for (int32_t y = 0; y < kradii; y++) {
		// Top left region.
		for (int32_t x = 0; x < kradii; x++) {
			T max_val = (std::numeric_limits<T>::min)();
			for (int32_t ky = 0; ky < y + kradii; ky++) {
				for (int32_t kx = 0; kx < x + kradii; kx++) {
					max_val = std::max(max_val, raw_image[ky * width + kx]);
				}
			}
			processed_image[y * width + x] = max_val;
		}
		// Top right region.
		for (int32_t x = width - kradii; x < width; x++) {
			T max_val = (std::numeric_limits<T>::min)();
			for (int32_t ky = 0; ky < y + kradii; ky++) {
				for (int32_t kx = x - kradii; kx < width; kx++) {
					max_val = std::max(max_val, raw_image[ky * width + kx]);
				}
			}
			processed_image[y * width + x] = max_val;
		}
	}
	
	for (int32_t y = height - kradii; y < height; y++) {
		// Lower left region.
		for (int32_t x = 0; x < kradii; x++) {
			T max_val = (std::numeric_limits<T>::min)();
			for (int32_t ky = y - kradii; ky < height; ky++) {
				for (int32_t kx = 0; kx < x + kradii; kx++) {
					max_val = std::max(max_val, raw_image[ky * width + kx]);
				}
			}
			processed_image[y * width + x] = max_val;
		}
		// Lower right region.
		for (int32_t x = width - kradii; x < width; x++) {
			T max_val = (std::numeric_limits<T>::min)();
			for (int32_t ky = y - kradii; ky < height; ky++) {
				for (int32_t kx = x - kradii; kx < width; kx++) {
					max_val = std::max(max_val, raw_image[ky * width + kx]);
				}
			}
			processed_image[y * width + x] = max_val;
		}
	}
	
	for (int32_t x = kradii; x < width - kradii; x++) {
		// Top central region.
		for (int32_t y = 0; y < kradii; y++) {
			T max_val = (std::numeric_limits<T>::min)();
			for (int32_t ky = 0; ky < y + kradii; ky++) {
				for (int32_t kx = x - kradii; kx < x + kradii; kx++) {
					max_val = std::max(max_val, raw_image[ky * width + kx]);
				}
			}
			processed_image[y * width + x] = max_val;
		}
		// Lower central region.
		for (int32_t y = height - kradii; y < height; y++) {
			T max_val = (std::numeric_limits<T>::min)();
			for (int32_t ky = y - kradii; ky < height; ky++) {
				for (int32_t kx = x - kradii; kx < x + kradii; kx++) {
					max_val = std::max(max_val, raw_image[ky * width + kx]);
				}
			}
			processed_image[y * width + x] = max_val;
		}
	}
	
	for (int32_t y = kradii; y < height - kradii; y++) {
		// Left central region.
		for (int32_t x = 0; x < kradii; x++) {
			T max_val = (std::numeric_limits<T>::min)();
			for (int32_t ky = y - kradii; ky < y + kradii; ky++) {
				for (int32_t kx = 0; kx < x + kradii; kx++) {
					max_val = std::max(max_val, raw_image[ky * width + kx]);
				}
			}
			processed_image[y * width + x] = max_val;
		}
		// Right central region.
		for (int32_t x = width - kradii; x < width; x++) {
			T max_val = (std::numeric_limits<T>::min)();
			for (int32_t ky = y - kradii; ky < y + kradii; ky++) {
				for (int32_t kx = width - kradii; kx < width; kx++) {
					max_val = std::max(max_val, raw_image[ky * width + kx]);
				}
			}
			processed_image[y * width + x] = max_val;
		}
	}
	
	if (cache_image) {
		delete [] cache_image;
		cache_image = 0;
	}
}

#endif