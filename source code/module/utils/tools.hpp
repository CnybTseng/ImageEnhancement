#ifndef _TOOLS_HPP_
#define _TOOLS_HPP_

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cassert>
#include <iostream>
#include <limits>
#include <vector>
#include <string>
#include <sys/stat.h> 

#ifdef _OPENMP
#include <omp.h>
#endif

#include "pthread.h"

#define UNLIMITED_TO_PZEOR		(0.000001)
#define MAX_TEXT_LINE_LENGTH	(10240)
#define EPS						(1e-8)

#if defined(WIN32) || defined(WIN64)
#include <windows.h>
#define sleep(n) Sleep(1000 * (n))
#else
#include <unistd.h>
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
 * \template struct extreme_params_t
 * \brief Data structure for the extreme value parameters.
 */
template <typename T>
struct extreme_params_t{
	T *data;				// Image data.
	int32_t npixels;		// Number of pixels.
	int32_t channel;		// Channel number.
	T *min_val;				// Minimum value.
	T *max_val;				// Maximum value.
};

/**
 * Get extreme value of a array.
 * @param[in] data Data with any type.
 * @param[in]length data array length.
 * @param[out] min_val Minimum value.
 * @param[out] max_val Maximum value.
 */
template <typename T>
static void extreme_value(
	T *data,
	int32_t length,
	T &min_val,
	T &max_val
)	{
	assert(data);
	assert(length > 0);
	min_val = (std::numeric_limits<T>::max)();
	max_val = (std::numeric_limits<T>::min)();
	for (int32_t i = 0; i < length; i++) {
		if (data[i] < min_val) {
			min_val = data[i];
		}
		if (data[i] > max_val) {
			max_val = data[i];
		}
	}
}

/**
 * Get extreme value and corresponding index of a array.
 * @param[in] data Data with any type.
 * @param[in]length data array length.
 * @param[out] min_val Minimum value.
 * @param[out] max_val Maximum value.
 */
template <typename T>
static void extreme_value(
	T *data,
	int32_t length,
	T &min_val,
	T &max_val,
	int32_t &min_idx,
	int32_t &max_idx
)	{
	assert(data);
	assert(length > 0);
	min_idx = 0;
	max_idx = 0;
	min_val = (std::numeric_limits<T>::max)();
	max_val = (std::numeric_limits<T>::min)();
	for (int32_t i = 0; i < length; i++) {
		if (data[i] < min_val) {
			min_val = data[i];
			min_idx = i;
		}
		if (data[i] > max_val) {
			max_val = data[i];
			max_idx = i;
		}
	}
}

/**
 * Thread for function of getting extreme value of multi-channel image.
 * @param[in,out] param Thread parameters.
 * @return void *.
 */
template <typename T>
void *extreme_value_thread(
	void *param
)	{
	extreme_params_t<T> *params = (extreme_params_t<T> *)param;
	const int32_t bytes_per_pixel = 3;
	T min_val = (std::numeric_limits<T>::max)();
	T max_val = (std::numeric_limits<T>::min)();
	
	#pragma omp parallel for reduction(min:min_val) reduction(max:max_val) num_threads(4)
	for (int32_t i = 0; i < params->npixels; i++) {
		int32_t ic = i * bytes_per_pixel + params->channel;
		min_val = min_val < params->data[ic] ? min_val : params->data[ic];
		max_val = max_val > params->data[ic] ? max_val : params->data[ic];
	}
	
	*(params->min_val) = min_val;
	*(params->max_val) = max_val;
}

/**
 * Get extreme value and corresponding index of multi-channel image.
 * @param[in] data Image with any type.
 * @param[in] width Image width.
 * @param[in] height Image height.
 * @param[out] min_val Minimum value.
 * @param[out] max_val Maximum value.
 */
template <typename T>
static void extreme_value(
	T *data,
	int32_t width,
	int32_t height,
	T *min_val,
	T *max_val
)	{
	assert(data);
	assert(min_val);
	assert(max_val);
	
	const int32_t nchannels = 3;
	pthread_t tid[nchannels];
	extreme_params_t<T> params[3];
	
	for (int32_t c = 0; c < nchannels; c++) {
		params[c].data = data;
		params[c].npixels = width * height;
		params[c].channel = c;
		params[c].min_val = &min_val[c];
		params[c].max_val = &max_val[c];
	}
	
	for (int32_t c = 0; c < nchannels; c++) {
		int32_t ret = pthread_create(&tid[c], NULL, extreme_value_thread<T>, &params[c]);
		if (0 != ret) {
			printf("Create extreme value thread[%d] fail!\n", c);
			exit(-1);
		}
	}

	for (int32_t c = 0; c < nchannels; c++) {
		pthread_join(tid[c], NULL);
	}
}

/**
 * Linear scale data to range [0, 255].
 * @param[in] raw_data Raw data width any type.
 * @param[out] u8_data Scaled data with type uint8_t.
 * @param[in] length Data array length.
 * @return void.
 */
template <typename T>
static void touint8(
	T *raw_data,
	uint8_t *u8_data,
	int32_t length
)	{
	assert(raw_data);
	assert(u8_data);
	assert(length > 0);
	// Get extreme value of type T.
	T min_val = (std::numeric_limits<T>::max)();
	T max_val = (std::numeric_limits<T>::min)();
	// Get extreme value of raw data.
	for (int32_t i = 0; i < length; i++) {
		if (raw_data[i] < min_val) {
			min_val = raw_data[i];
		}
		if (raw_data[i] > max_val) {
			max_val = raw_data[i];
		}
	}
	// printf("%u %u\n", min_val, max_val);
	// Linear scale raw data to range [0, 255].
	const float scale = 255.0f / (max_val - min_val + EPS);
	for (int32_t i = 0; i < length; i++) {
		u8_data[i] = (uint8_t)(scale * (raw_data[i] - min_val));
	}
}

/**
 * Linear scale data to range [0, 255].
 * @param[in] raw_data Raw data width any type.
 * @param[out] u8_data Scaled data with type uint8_t.
 * @param[in] length Data array length.
 * @param[in] p Weighted coefficient.
 * @return void.
 */
template <typename T>
static void touint8(
	T *raw_data,
	uint8_t *u8_data,
	int32_t length,
	float p
)	{
	assert(raw_data);
	assert(u8_data);
	assert(length > 0);
	
	static uint32_t timer = 0;
	T min_val = (std::numeric_limits<T>::max)();
	T max_val = (std::numeric_limits<T>::min)();
	static float min_aval, max_aval;
	// Get extreme value of raw data.
	for (int32_t i = 0; i < length; i++) {
		min_val = std::min(min_val, raw_data[i]);
		max_val = std::max(max_val, raw_data[i]);
	}
	
	// Exponentially weighted moving average for extreme value.
	if (timer == 0) {
		min_aval = min_val;
		max_aval = max_val;
	} else {
		min_aval = p * min_val + (1 - p) * min_aval;
		max_aval = p * max_val + (1 - p) * max_aval;
	}
	// Linear scale raw data to range [0, 255].
	const float scale = 255.0f / (max_aval - min_aval + EPS);
	for (int32_t i = 0; i < length; i++) {
		float tval = 0;
		if (raw_data[i] < min_aval) {
			tval = min_aval;
		} else if (raw_data[i] > max_aval) {
			tval = max_aval;
		} else {
			tval = raw_data[i];
		}
		u8_data[i] = uint8_t(scale * (tval - min_aval));
	}

	timer++;
}

/**
 * Get command line printing.
 * @param[in] cmd Command.
 * @param[out] printing Command line printing.
 * @return void.
 */
static void get_cmd_printing(
	const char *const cmd,
	char *const printing
)	{
	assert(cmd);
	assert(printing);
	char line_buf[MAX_TEXT_LINE_LENGTH];
#ifdef _WIN32
	FILE *fstream = _popen(cmd, "rt");
#elif __linux__
	FILE *fstream = popen(cmd, "rt");
#else
#   error "Unknown compiler"
#endif
	if (!fstream) {
		perror("popen");
		return;
	}
	
	while (fgets(line_buf, MAX_TEXT_LINE_LENGTH, fstream)) {
		strcat(printing, line_buf);
	}
#ifdef _WIN32	
	_pclose(fstream);
#elif __linux__
	pclose(fstream);
#else
#   error "Unknown compiler"
#endif
	fstream = 0;
}

/**
 * Get filenames using command line.
 * @param[in] cmd Command line.
 * @param[in] format File format.
 * @param[out] filenames Filename vector.
 * @return void.
 */
static void get_filenames(
	const char *const cmd,
	const char *const format,
	std::vector<std::string> &filenames
)	{
	assert(cmd);
	assert(format);
	
	// Command like 'dir /b /s Path'.
#ifdef _WIN32
	FILE *fstream = _popen(cmd, "rt");
#elif __linux__
	FILE *fstream = popen(cmd, "rt");
#else
#   error "Unknown compiler"
#endif
	if (!fstream) {
		perror("popen");
		return;
	}
	
	const int32_t max_name_length = 128;
	char line_buf[max_name_length];
	char name[max_name_length];
	while (fgets(line_buf, max_name_length, fstream)) {
		if (strstr(line_buf, format)) {	
			sscanf(line_buf, "%s\n", name);
			filenames.push_back(name);
		}
	}

#ifdef _WIN32	
	_pclose(fstream);
#elif __linux__
	pclose(fstream);
#else
#   error "Unknown compiler"
#endif
	fstream = 0;
}

/**
 * Get file size.
 * @param[in] filename Filename.
 * @return File size.
 */
static uint32_t file_size(
	const char *const filename
)  	{  
    struct stat statbuf;  
    stat(filename,&statbuf);  
    uint32_t size = statbuf.st_size;  

    return size;  
} 

#endif