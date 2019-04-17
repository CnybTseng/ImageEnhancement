#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <limits>
#include <ctime>
#include <sched.h>
#ifdef __linux__
#include <signal.h>
#endif
#ifdef __ARM_NEON__ 
#include <arm_neon.h>
#include <neon_mathfun.h>
#endif
#ifdef _OPENMP
#include <omp.h>
#endif
#include <cmath>

#include "json/json.h"
#include "defog.h"
#include "tools.hpp"
#include "color.h"
#include "order_filter.hpp"
#include "cvenhance.hpp"
#include "cvgeo_tran.hpp"
#include "clahe.h"
#include "clhe.h"

#define MAX_TRANSMISSION	(100)
#define MAX_CACHE_FRAMES	(25)
#define Y_FLOOR				(16)
#define Y_CEILING			(235)
#define CBCR_FLOOR			(16)
#define CBCR_CEILING		(240)
#define vector_size 		(16)

#define vminmax(a, b) \
	do { \
		uint16x4_t minmax_tmp = (a); \
		(a) = vmin_u16((a), (b)); \
		(b) = vmax_u16(minmax_tmp, (b)); \
	} while (0)

#define vmaxmin(a, b) \
	do { \
		uint16x4_t maxmin_tmp = (a); \
		(a) = vmax_u16((a), (b)); \
		(b) = vmin_u16(maxmin_tmp, (b)); \
	} while (0)

#define vminmaxq(a, b) \
	do { \
		uint16x8_t minmax_tmp = (a); \
		(a) = vminq_u16((a), (b)); \
		(b) = vmaxq_u16(minmax_tmp, (b)); \
	} while (0)

#define vmaxminq(a, b) \
	do { \
		uint16x8_t maxmin_tmp = (a); \
		(a) = vmaxq_u16((a), (b)); \
		(b) = vminq_u16(maxmin_tmp, (b)); \
	} while (0)

#define vtrn32(a, b) \
	do { \
		uint32x2x2_t vtrn32_tmp = \
			vtrn_u32(vreinterpret_u32_u16(a), vreinterpret_u32_u16(b)); \
		(a) = vreinterpret_u16_u32(vtrn32_tmp.val[0]); \
		(b) = vreinterpret_u16_u32(vtrn32_tmp.val[1]); \
	} while (0)

#define vminmax_u8(a, b) \
    do { \
        uint8x16_t minmax_tmp = (a); \
        (a) = vminq_u8((a), (b)); \
        (b) = vmaxq_u8(minmax_tmp, (b)); \
    } while (0)

#define rank(array, count)\
	do {\
	uint32_t i, j, temp;\
	for (i = 0; i < count; i++) {\
		for (j = i; j < count; j++) {\
			if (array[i] > array[j]) {\
				temp = array[i];\
				array[i] = array[j];\
				array[j] = temp;\
			}\
		}\
	}\
} while (0)

/**
 * \typedef struct min_filter_thread_param_t
 * \brief Data structure for the minimum filter thread parameters.
 */
typedef struct {
	uint8_t *raw_data;			// Raw image.
	int32_t width;				// Image width.
	int32_t height;				// Image height.
	int32_t ksize;				// Kernel size.
	uint8_t *processed_data;	// Processed image.
}min_filter_thread_param_t;

/**
 * \typedef struct normalize_thread_param_t
 * \brief Data structure for the normalize minimum filter image parameters.
 */
typedef struct {
	uint8_t *data;			// Minimum filter image.
	int32_t npixels;		// Number of pixels.
	uint8_t atmo_light;		// Atmospheric light.
	float *norm_data;		// Normalized minimum filter image.
}normalize_thread_param_t;

/**
 * \typedef struct recover_thread_param_t
 * \brief Data structure for the recover thread parameters.
 */
typedef struct
{
	uint8_t *hazzy_data;	// Original sub-hazzy image.
	float *tran_data;		// Transmission sub-image.	
	int npixels;			// Number of pixels of sub-image.
	uint8_t *atmo_light;		// Atmospheric light.
	int32_t (*diff_table)[3];
	uint8_t *recover_data;	// Recover sub-image.
}recover_thread_param_t;

/**
 * \typedef struct auto_level_thread_param_t
 * \brief Data structure for the auto level thread parameters.
 */
typedef struct {
	uint8_t *recover_data;	// Recover sub-image.
	int32_t npixels;		// Number of pixels of sub-image.
	uint8_t **stretch_table;
	uint8_t *stretch_data;	// Stretch sub-image.
}auto_level_thread_param_t;

/**
 * Motion tranformation function.
 */
static uint8_t MTF[64] = {
	255, 250, 200, 160, 100, 65, 50, 40,
	36,  30,  28,  25,  23,  21, 19, 18,
	17,  16,  15,  14,  13,  12, 12, 11,
	11,  10,  10,  10,  10,  9,  9,  9,
	9,   8,   8,   8,   7,   7,  7,  6,
	6,   6,   6,   5,   5,   5,  5,  5,
	5,   4,   4,   4,   4,   4,  3,  3,
	3,   2,   2,   2,   1,   1,  1,  0
};

//---------------------------------------------------------
// Default constructor function of class defog.
//---------------------------------------------------------
defog::defog()
{

}

//---------------------------------------------------------
// Constructor function of class defog.
//---------------------------------------------------------
defog::defog(
	uint8_t *hazzy_img_,
	int32_t width_,
	int32_t height_
)	{
	assert(hazzy_img_);
	assert(width_ > 0);
	assert(height_ > 0);
	// Default parameters
	width = width_;
	height = height_;
	downsample = 0.5;
	kmin_size = 15;
	bright_ratio_thresh = 0.1f;
	atmo_light_ceiling = 255;
	tran_thresh = 0.1f;
	omega = 0.95f;
	lowcut_thresh = 0.00001f;
	highcut_thresh = 0.01f;
	frame_index = 0;
	update_period = 1;
	nrecover_threads = 30;
	nstretch_threads = 10;
	clip_limit = 2;
	gamma = 1.0;
	enable_uv_adjust = 0;
	enable_edge_enhan = 0;
	enable_T_noise_reduce = 0;
	enable_2D_noise_reduce = 0;
	enable_module = 1;
	slt[0] = 48;
	slt[1] = 72;
	slt[2] = 208;
	slt[3] = 184;
	// Load from configuration.
	load_parameters();
	// Downsample resolution.
	ds_width = static_cast<int32_t>(downsample * width);
	ds_height = static_cast<int32_t>(downsample * height);
	// Guided filter kernel size.
	kgud_size = 4 * kmin_size + 1;
	
	bgr_image = new uint8_t[3 * width * height];
	assert(bgr_image);
	
	dsbgr_image = new uint8_t[3 * ds_width * ds_height];
	assert(dsbgr_image);
		
	hazzy_img = hazzy_img_;
	// Allocate memory for minimum channel image.
	min_chan_img = new uint8_t[ds_width * ds_height];
	assert(min_chan_img);
	
	const int32_t nchannels = 3;
	for (int32_t c = 0; c < nchannels; c++) {
		// Allocate memory for original single channel image.
		rgb_img[c] = new uint8_t[width * height];
		assert(rgb_img[c]);
		
		dsrgb_img[c] = new uint8_t[ds_width * ds_height];
		assert(dsrgb_img[c]);
		// Allocate memory for minimum filter image.
		min_filt_rgb_img[c] = new uint8_t[ds_width * ds_height];
		assert(min_filt_rgb_img[c]);
		// Allocate memory for normalized minimum filter image.
		norm_min_filt_rgb_img[c] = new float[ds_width * ds_height];
		assert(norm_min_filt_rgb_img[c]);
	}
	// Allocate memory for normalized minimum filter image.
	norm_min_chan_img = new float[ds_width * ds_height];
	assert(norm_min_chan_img);
	// Allocate memory for transmission image.
	transm_img = new float[ds_width * ds_height];
	assert(transm_img);
	
	ustransm_img = new float[width * height];
	assert(ustransm_img);
	
	transm_inv = new float[ds_width * ds_height];
	assert(transm_inv);
	
	u8_transmission_image = new uint8_t[width * height];
	assert(u8_transmission_image);
	// Allocate memory for recover image.
	recover_img = new uint8_t[3 * width * height];
	assert(recover_img);
	// Allocate memory for stretch image.
	stretch_img = new uint8_t[3 * width * height];
	assert(stretch_img);
	// Allocate memory for transmission.
	transmission = new int32_t[width * height];
	assert(transmission);
	// Allocate memory for recover table.
	const int32_t nlevels = 256;
	recover_table = new uint8_t **[nchannels];
	assert(recover_table);
	for (int32_t i = 0; i < nchannels; i++) {
		recover_table[i] = new uint8_t *[nlevels];
		assert(recover_table[i]);
		for (int32_t j = 0; j < nlevels; j++) {
			recover_table[i][j] = new uint8_t[MAX_TRANSMISSION + 1];
			assert(recover_table[i][j]);
		}
	}
	// Allocate memory for stretch table.
	stretch_table = new uint8_t *[nchannels];
	assert(stretch_table);
	for (int32_t i = 0; i < nchannels; i++) {
		stretch_table[i] = new uint8_t[nlevels];
		assert(stretch_table[i]);
	}
	
	gamma_correct_table = new uint8_t[nlevels];
	assert(gamma_correct_table);
	
	old_y = new uint8_t[width * height];
	assert(old_y);
	
	prev_manr_y_image = new uint8_t[width * height];
	assert(prev_manr_y_image);
	
	init_prev_manr_y_image_flag = false;
	
	mfilt_y_image = new uint8_t[width * height];
	assert(mfilt_y_image);
	
	pthread_mutex_init(&in_yuv_image_mutex, NULL);
	pthread_mutex_init(&in_bgr_image_mutex, NULL);
	pthread_mutex_init(&out_bgr_image_mutex, NULL);
	pthread_mutex_init(&out_yuv_image_mutex, NULL);
	
	pthread_mutex_init(&raw_y_image_mutex, NULL);
	pthread_mutex_init(&clip_yuv_image_mutex, NULL);
	pthread_mutex_init(&slt_yuv_image_mutex, NULL);
	pthread_mutex_init(&clahe_yuv_image_mutex, NULL);
	pthread_mutex_init(&edge_yuv_image_mutex, NULL);
	pthread_mutex_init(&sa_manr_yuv_image_mutex, NULL);
}

//---------------------------------------------------------
// Destructor function of class defog.
//---------------------------------------------------------
defog::~defog()
{
	if (bgr_image) {
		delete [] bgr_image;
		bgr_image = 0;
	}
	
	if (dsbgr_image) {
		delete [] dsbgr_image;
		dsbgr_image = 0;
	}
	
	// Free minimum channel image memory.
	if (min_chan_img) {
		delete [] min_chan_img;
		min_chan_img = 0;
	}
	
	const int32_t nchannels = 3;
	for (int32_t c = 0; c < nchannels; c++) {
		// Free original single channel image memory.
		if (rgb_img[c]) {
			delete [] rgb_img[c];
			rgb_img[c] = 0;
		}
		
		if (dsrgb_img[c]) {
			delete [] dsrgb_img[c];
			dsrgb_img[c] = 0;
		}
		// Free minimum filter image image memory.
		if (min_filt_rgb_img[c]) {
			delete [] min_filt_rgb_img[c];
			min_filt_rgb_img[c] = 0;
		}
		// Free normalized minimum filter image memory.
		if (norm_min_filt_rgb_img[c]) {
			delete [] norm_min_filt_rgb_img[c];
			norm_min_filt_rgb_img[c] = 0;
		}
	}
	// Free normalized minimum channel image.
	if (norm_min_chan_img) {
		delete [] norm_min_chan_img;
		norm_min_chan_img = 0;
	}
	// Free transmission image memory.
	if (transm_img) {
		delete [] transm_img;
		transm_img = 0;
	}
	
	if (ustransm_img) {
		delete [] ustransm_img;
		ustransm_img = 0;
	}
	
	if (transm_inv) {
		delete [] transm_inv;
		transm_inv = 0;
	}
	
	if (u8_transmission_image) {
		delete [] u8_transmission_image;
		u8_transmission_image = 0;
	}
	// Free recover image memory.
	if (recover_img) {
		delete [] recover_img;
		recover_img = 0;
	}
	// Free stretch image memory.
	if (stretch_img) {
		delete [] stretch_img;
		stretch_img = 0;
	}
	// Free transmission memory.
	if (transmission) {
		delete [] transmission;
	}
	// Free recover table memory.
	const int32_t nlevels = 256;
	if (recover_table) {
		for (int32_t c = 0; c < nchannels; c++) {
			for (int32_t i = 0; i < nlevels; i++) {
				if (recover_table[c][i]) {
					delete [] recover_table[c][i];
					recover_table[c][i] = 0;
				}
			}
			if (recover_table[c]) {
				delete [] recover_table[c];
				recover_table[c] = 0;
			}
		}
		delete [] recover_table;
		recover_table = 0;
	}
	// Free stretch table memory.
	if (stretch_table) {
		for (int32_t i = 0; i < nchannels; i++) {
			if (stretch_table[i]) {
				delete [] stretch_table[i];
				stretch_table[i] = 0;
			}
		}
		delete [] stretch_table;
		stretch_table = 0;
	}
	
	if (gamma_correct_table) {
		delete [] gamma_correct_table;
		gamma_correct_table = 0;
	}
	
	if (old_y) {
		delete [] old_y;
		old_y = 0;
	}
	
	if (prev_manr_y_image) {
		delete [] prev_manr_y_image;
		prev_manr_y_image = 0;
	}
	
	if (mfilt_y_image) {
		delete [] mfilt_y_image;
		mfilt_y_image = 0;
	}
	
	pthread_mutex_destroy(&in_yuv_image_mutex);
	pthread_mutex_destroy(&in_bgr_image_mutex);
	pthread_mutex_destroy(&out_bgr_image_mutex);
	pthread_mutex_destroy(&out_yuv_image_mutex);
	
	pthread_mutex_destroy(&raw_y_image_mutex);
	pthread_mutex_destroy(&clip_yuv_image_mutex);
	pthread_mutex_destroy(&slt_yuv_image_mutex);
	pthread_mutex_destroy(&clahe_yuv_image_mutex);
	pthread_mutex_destroy(&edge_yuv_image_mutex);
	pthread_mutex_destroy(&sa_manr_yuv_image_mutex);
}

//---------------------------------------------------------
// Init gamma correction table.
//---------------------------------------------------------
void defog::init_gamma_correct_table()
{
	const int32_t max_level = 255;
	const float scale = 1.0f;
	for (int32_t i = 0; i < max_level; i++) {
		gamma_correct_table[i] = max_level * scale * pow((float)i / max_level, gamma);
	}
}

//---------------------------------------------------------
// Init defog instance.
//---------------------------------------------------------
void defog::init(
	uint8_t *hazzy_img_,
	int32_t width_,
	int32_t height_
)	{
	assert(hazzy_img_);
	assert(width_ > 0);
	assert(height_ > 0);
	// Default parameters
	width = width_;
	height = height_;
	downsample = 0.5;
	kmin_size = 15;
	bright_ratio_thresh = 0.1f;
	atmo_light_ceiling = 255;
	tran_thresh = 0.1f;
	omega = 0.95f;
	lowcut_thresh = 0.00001f;
	highcut_thresh = 0.01f;
	frame_index = 0;
	update_period = 1;
	nrecover_threads = 30;
	nstretch_threads = 10;
	clip_limit = 2;
	gamma = 1.0;
	enable_uv_adjust = 0;
	enable_edge_enhan = 0;
	enable_T_noise_reduce = 0;
	enable_2D_noise_reduce = 0;
	enable_module = 1;
	slt[0] = 48;
	slt[1] = 72;
	slt[2] = 208;
	slt[3] = 184;
	// Load from configuration.
	load_parameters();
	// Downsample resolution.
	ds_width = static_cast<int32_t>(downsample * width);
	ds_height = static_cast<int32_t>(downsample * height);
	// Guided filter kernel size.
	kgud_size = 4 * kmin_size + 1;
	
	bgr_image = new uint8_t[3 * width * height];
	assert(bgr_image);
	
	dsbgr_image = new uint8_t[3 * ds_width * ds_height];
	assert(dsbgr_image);
		
	hazzy_img = hazzy_img_;
	// Allocate memory for minimum channel image.
	min_chan_img = new uint8_t[ds_width * ds_height];
	assert(min_chan_img);
	
	const int32_t nchannels = 3;
	for (int32_t c = 0; c < nchannels; c++) {
		// Allocate memory for original single channel image.
		rgb_img[c] = new uint8_t[width * height];
		assert(rgb_img[c]);
		
		dsrgb_img[c] = new uint8_t[ds_width * ds_height];
		assert(dsrgb_img[c]);
		// Allocate memory for minimum filter image.
		min_filt_rgb_img[c] = new uint8_t[ds_width * ds_height];
		assert(min_filt_rgb_img[c]);
		// Allocate memory for normalized minimum filter image.
		norm_min_filt_rgb_img[c] = new float[ds_width * ds_height];
		assert(norm_min_filt_rgb_img[c]);
	}
	// Allocate memory for normalized minimum filter image.
	norm_min_chan_img = new float[ds_width * ds_height];
	assert(norm_min_chan_img);
	// Allocate memory for transmission image.
	transm_img = new float[ds_width * ds_height];
	assert(transm_img);
	
	ustransm_img = new float[width * height];
	assert(ustransm_img);
	
	transm_inv = new float[ds_width * ds_height];
	assert(transm_inv);
	
	u8_transmission_image = new uint8_t[width * height];
	assert(u8_transmission_image);
	// Allocate memory for recover image.
	recover_img = new uint8_t[3 * width * height];
	assert(recover_img);
	// Allocate memory for stretch image.
	stretch_img = new uint8_t[3 * width * height];
	assert(stretch_img);
	// Allocate memory for transmission.
	transmission = new int32_t[width * height];
	assert(transmission);
	// Allocate memory for recover table.
	const int32_t nlevels = 256;
	recover_table = new uint8_t **[nchannels];
	assert(recover_table);
	for (int32_t i = 0; i < nchannels; i++) {
		recover_table[i] = new uint8_t *[nlevels];
		assert(recover_table[i]);
		for (int32_t j = 0; j < nlevels; j++) {
			recover_table[i][j] = new uint8_t[MAX_TRANSMISSION + 1];
			assert(recover_table[i][j]);
		}
	}
	// Allocate memory for stretch table.
	stretch_table = new uint8_t *[nchannels];
	assert(stretch_table);
	for (int32_t i = 0; i < nchannels; i++) {
		stretch_table[i] = new uint8_t[nlevels];
		assert(stretch_table[i]);
	}
	
	gamma_correct_table = new uint8_t[nlevels];
	assert(gamma_correct_table);
	
	init_gamma_correct_table();
	
	old_y = new uint8_t[width * height];
	assert(old_y);
	
	prev_manr_y_image = new uint8_t[width * height];
	assert(prev_manr_y_image);
	
	init_prev_manr_y_image_flag = false;
	
	mfilt_y_image = new uint8_t[width * height];
	assert(mfilt_y_image);
	
	pthread_mutex_init(&in_yuv_image_mutex, NULL);
	pthread_mutex_init(&in_bgr_image_mutex, NULL);
	pthread_mutex_init(&out_bgr_image_mutex, NULL);
	pthread_mutex_init(&out_yuv_image_mutex, NULL);
	
	pthread_mutex_init(&raw_y_image_mutex, NULL);
	pthread_mutex_init(&clip_yuv_image_mutex, NULL);
	pthread_mutex_init(&slt_yuv_image_mutex, NULL);
	pthread_mutex_init(&clahe_yuv_image_mutex, NULL);
	pthread_mutex_init(&edge_yuv_image_mutex, NULL);
	pthread_mutex_init(&sa_manr_yuv_image_mutex, NULL);

#ifdef PIPELINE	
#ifdef DARK_PRIOR
	start_yuv2bgr_pipeline();
	start_defog_pipeline();
	start_bgr2yuv_pipeline();
#elif CONTRAST_ENHANCE	
	start_segment_linear_transf_thread();
	start_clahe_thread();
	start_edge_enhance_thread();
	start_sat_adjust_manr_thread();
#endif
#endif
}

//---------------------------------------------------------
// Load parameters from configuration.
//---------------------------------------------------------
void defog::load_parameters()
{
	Json::Reader reader;
	Json::Value root;
	std::ifstream ifs;
	ifs.open("defog.json", std::ios::binary);
	if(reader.parse(ifs,root))	{
		downsample = root["downsample"].asDouble();
		kmin_size = root["kmin_size"].asInt();
		update_period = root["update_period"].asInt();
		bright_ratio_thresh = root["bright_ratio_thresh"].asDouble();
		atmo_light_ceiling = root["atmo_light_ceiling"].asDouble();
		tran_thresh = root["tran_thresh"].asDouble();
		omega = root["omega"].asDouble();
		lowcut_thresh = root["lowcut_thresh"].asDouble();
		highcut_thresh = root["highcut_thresh"].asDouble();
		nrecover_threads = root["nrecover_threads"].asInt();
		nstretch_threads = root["nstretch_threads"].asInt();
		clip_limit = root["clip_limit"].asDouble();
		gamma = root["gamma"].asDouble();
		enable_uv_adjust = root["enable_uv_adjust"].asInt();
		enable_edge_enhan = root["enable_edge_enhan"].asInt();
		enable_module = root["enable_module"].asInt();
		slt[0] = root["slt0"].asInt();
		slt[1] = root["slt1"].asInt();
		slt[2] = root["slt2"].asInt();
		slt[3] = root["slt3"].asInt();
		enable_T_noise_reduce = root["enable_T_noise_reduce"].asInt();
		enable_2D_noise_reduce = root["enable_2D_noise_reduce"].asInt();
		printf("downsample\t\t%f\n", downsample);
		printf("kmin_size\t\t%d\n", kmin_size);
		printf("update_period\t\t%d\n", update_period);
		printf("bright_ratio_thresh\t%f\n", bright_ratio_thresh);
		printf("atmo_light_ceiling\t%u\n", atmo_light_ceiling);
		printf("tran_thresh\t\t%f\n", tran_thresh);
		printf("omega\t\t\t%f\n", omega);
		printf("lowcut_thresh\t\t%f\n", lowcut_thresh);
		printf("highcut_thresh\t\t%f\n", highcut_thresh);
		printf("nrecover_threads\t%d\n", nrecover_threads);
		printf("nstretch_threads\t%d\n", nstretch_threads);
		printf("clip_limit\t\t%f\n", clip_limit);
		printf("gamma\t\t\t%f\n", gamma);
		printf("enable_uv_adjust\t%d\n", enable_uv_adjust);
		printf("enable_edge_enhan\t%d\n", enable_edge_enhan);
		printf("slt0\t\t\t%d\n", slt[0]);
		printf("slt1\t\t\t%d\n", slt[1]);
		printf("slt2\t\t\t%d\n", slt[2]);
		printf("slt3\t\t\t%d\n", slt[3]);
		printf("enable_T_noise_reduce\t%d\n", enable_T_noise_reduce);
		printf("enable_2D_noise_reduce\t%d\n", enable_2D_noise_reduce);
		printf("enable_module\t\t%d\n", enable_module);
	} else {
		printf("Not found configuration, use default sets.\n");
	}
	ifs.close();
}

//---------------------------------------------------------
// Convert yuv to bgr and split.
//---------------------------------------------------------
void defog::yuv2bgr(
	uint8_t *yuv_image,
	int32_t width,
	int32_t height,
	uint8_t *bgr_image
)	{
	assert(yuv_image);
	assert(bgr_image);
#ifdef __ARM_NEON__
	const int32_t nchannels = 3;
	const int32_t npixels = width * height;
	const int32_t npixels_per_loop = 8;
	float32x4_t f32_const = vdupq_n_f32(128);
	uint8_t *y_start = yuv_image;
	uint8_t *u_start = y_start + npixels;
	uint8_t *v_start = u_start + (npixels >> 2);
	uint8_t *y_pos = y_start;
	uint8_t *u_pos = u_start;
	uint8_t *v_pos = v_start;

	for (int32_t y = 0; y < height; y++) {
		int32_t row_loop_counter = 1;
		uint16x4x2_t u16x4x2_u_hig_zip_data;
		uint16x4x2_t u16x4x2_v_hig_zip_data;
		for (int32_t x = 0; x < width; x += npixels_per_loop) {		
			// Load Y data from DDR.
			uint8x8_t u8x8_y_data = vld1_u8(y_pos);
			uint16x8_t u16x8_y_data = vmovl_u8(u8x8_y_data);
			
			uint16x4x2_t u16x4x2_y_data;
			u16x4x2_y_data.val[0] = vget_low_u16(u16x8_y_data);
			u16x4x2_y_data.val[1] = vget_high_u16(u16x8_y_data);
			
			uint32x4x2_t u32x4x2_y_data;
			u32x4x2_y_data.val[0] = vmovl_u16(u16x4x2_y_data.val[0]);
			u32x4x2_y_data.val[1] = vmovl_u16(u16x4x2_y_data.val[1]);
			
			float32x4x2_t f32x4x2_y_data;
			f32x4x2_y_data.val[0] = vcvtq_f32_u32(u32x4x2_y_data.val[0]);
			f32x4x2_y_data.val[1] = vcvtq_f32_u32(u32x4x2_y_data.val[1]);
			
			//Load U and V data from DDR at odd column.
			uint16x4x2_t u16x4x2_u_zip_data;
			uint16x4x2_t u16x4x2_v_zip_data;
			if (row_loop_counter & 1) {
				// U.
				uint8x8_t u8x8_u_data = vld1_u8(u_pos);
				uint16x8_t u16x8_u_data = vmovl_u8(u8x8_u_data);

				uint16x4x2_t u16x4x2_u_data;
				u16x4x2_u_data.val[0] = vget_low_u16(u16x8_u_data);
				u16x4x2_u_data.val[1] = vget_high_u16(u16x8_u_data);
				
				uint16x4x2_t u16x4x2_u_low_zip_data = vzip_u16(u16x4x2_u_data.val[0], u16x4x2_u_data.val[0]);
	
				u16x4x2_u_hig_zip_data = vzip_u16(u16x4x2_u_data.val[1], u16x4x2_u_data.val[1]);
				u16x4x2_u_zip_data = u16x4x2_u_low_zip_data;
				
				// V.
				uint8x8_t u8x8_v_data = vld1_u8(v_pos);
				uint16x8_t u16x8_v_data = vmovl_u8(u8x8_v_data);
				
				uint16x4x2_t u16x4x2_v_data;
				u16x4x2_v_data.val[0] = vget_low_u16(u16x8_v_data);
				u16x4x2_v_data.val[1] = vget_high_u16(u16x8_v_data);
				
				uint16x4x2_t u16x4x2_v_low_zip_data = vzip_u16(u16x4x2_v_data.val[0], u16x4x2_v_data.val[0]);
				u16x4x2_v_hig_zip_data = vzip_u16(u16x4x2_v_data.val[1], u16x4x2_v_data.val[1]);
				u16x4x2_v_zip_data = u16x4x2_v_low_zip_data;				
			} else {
				u16x4x2_u_zip_data = u16x4x2_u_hig_zip_data;
				u16x4x2_v_zip_data = u16x4x2_v_hig_zip_data;
				u_pos = u_start + (y/4) * width + (x/2) + 4;
				v_pos = v_start + (y/4) * width + (x/2) + 4;
			}
			
			// U.
			uint32x4x2_t u32x4x2_u_data;
			u32x4x2_u_data.val[0] = vmovl_u16(u16x4x2_u_zip_data.val[0]);
			u32x4x2_u_data.val[1] = vmovl_u16(u16x4x2_u_zip_data.val[1]);
			
			float32x4x2_t f32x4x2_u_data;
			f32x4x2_u_data.val[0] = vcvtq_f32_u32(u32x4x2_u_data.val[0]);
			f32x4x2_u_data.val[1] = vcvtq_f32_u32(u32x4x2_u_data.val[1]);
			
			// V.
			uint32x4x2_t u32x4x2_v_data;
			u32x4x2_v_data.val[0] = vmovl_u16(u16x4x2_v_zip_data.val[0]);
			u32x4x2_v_data.val[1] = vmovl_u16(u16x4x2_v_zip_data.val[1]);
			
			float32x4x2_t f32x4x2_v_data;
			f32x4x2_v_data.val[0] = vcvtq_f32_u32(u32x4x2_v_data.val[0]);
			f32x4x2_v_data.val[1] = vcvtq_f32_u32(u32x4x2_v_data.val[1]);
			
			// U - 128.
			float32x4x2_t f32x4x2_u_sub_128_data;
			f32x4x2_u_sub_128_data.val[0] = vsubq_f32(f32x4x2_u_data.val[0], f32_const);
			f32x4x2_u_sub_128_data.val[1] = vsubq_f32(f32x4x2_u_data.val[1], f32_const);
			
			// V - 128.
			float32x4x2_t f32x4x2_v_sub_128_data;
			f32x4x2_v_sub_128_data.val[0] = vsubq_f32(f32x4x2_v_data.val[0], f32_const);
			f32x4x2_v_sub_128_data.val[1] = vsubq_f32(f32x4x2_v_data.val[1], f32_const);
			
			// Red: R = Y + 1.402 (Cr-128).
			float32x4x2_t f32x4x2_r_data;
			f32x4x2_r_data.val[0] = vaddq_f32(f32x4x2_y_data.val[0], vmulq_n_f32(f32x4x2_v_sub_128_data.val[0], 1.402));
			f32x4x2_r_data.val[1] = vaddq_f32(f32x4x2_y_data.val[1], vmulq_n_f32(f32x4x2_v_sub_128_data.val[1], 1.402));
			
			int32x4x2_t s32x4x2_r_data;
			s32x4x2_r_data.val[0] = vcvtq_s32_f32(f32x4x2_r_data.val[0]);
			s32x4x2_r_data.val[1] = vcvtq_s32_f32(f32x4x2_r_data.val[1]);
			
			int16x4x2_t s16x4x2_r_data;
			s16x4x2_r_data.val[0] = vmovn_s32(s32x4x2_r_data.val[0]);
			s16x4x2_r_data.val[1] = vmovn_s32(s32x4x2_r_data.val[1]);
			
			int16x8_t s16x8_r_data = vcombine_s16(s16x4x2_r_data.val[0], s16x4x2_r_data.val[1]);
			uint8x8_t u8x8_r_data = vqmovun_s16(s16x8_r_data);
			
			// Green: Y - 0.34414 (Cb-128) - 0.71414 (Cr-128).
			float32x4x2_t f32x4x2_g_data;
			f32x4x2_g_data.val[0] = vsubq_f32(vsubq_f32(f32x4x2_y_data.val[0], vmulq_n_f32(f32x4x2_u_sub_128_data.val[0], 0.34414)),
				vmulq_n_f32(f32x4x2_v_sub_128_data.val[0], 0.71414));	
			f32x4x2_g_data.val[1] = vsubq_f32(vsubq_f32(f32x4x2_y_data.val[1], vmulq_n_f32(f32x4x2_u_sub_128_data.val[1], 0.34414)),
				vmulq_n_f32(f32x4x2_v_sub_128_data.val[1], 0.71414));	
			
			int32x4x2_t s32x4x2_g_data;
			s32x4x2_g_data.val[0] = vcvtq_s32_f32(f32x4x2_g_data.val[0]);
			s32x4x2_g_data.val[1] = vcvtq_s32_f32(f32x4x2_g_data.val[1]);
			
			int16x4x2_t s16x4x2_g_data;
			s16x4x2_g_data.val[0] = vmovn_s32(s32x4x2_g_data.val[0]);
			s16x4x2_g_data.val[1] = vmovn_s32(s32x4x2_g_data.val[1]);
			
			int16x8_t s16x8_g_data = vcombine_s16(s16x4x2_g_data.val[0], s16x4x2_g_data.val[1]);
			uint8x8_t u8x8_g_data = vqmovun_s16(s16x8_g_data);
			
			// Blue: Y + 1.772 (Cb-128).
			float32x4x2_t f32x4x2_b_data;
			f32x4x2_b_data.val[0] = vaddq_f32(f32x4x2_y_data.val[0], vmulq_n_f32(f32x4x2_u_sub_128_data.val[0], 1.772));
			f32x4x2_b_data.val[1] = vaddq_f32(f32x4x2_y_data.val[1], vmulq_n_f32(f32x4x2_u_sub_128_data.val[1], 1.772));
			
			int32x4x2_t s32x4x2_b_data;
			s32x4x2_b_data.val[0] = vcvtq_s32_f32(f32x4x2_b_data.val[0]);
			s32x4x2_b_data.val[1] = vcvtq_s32_f32(f32x4x2_b_data.val[1]);
			
			int16x4x2_t s16x4x2_b_data;
			s16x4x2_b_data.val[0] = vmovn_s32(s32x4x2_b_data.val[0]);
			s16x4x2_b_data.val[1] = vmovn_s32(s32x4x2_b_data.val[1]);
			
			int16x8_t s16x8_b_data = vcombine_s16(s16x4x2_b_data.val[0], s16x4x2_b_data.val[1]);
			uint8x8_t u8x8_b_data = vqmovun_s16(s16x8_b_data);
			
			// Write result to DDR.		
			uint8x8x3_t u8x8x3_bgr_data;
			u8x8x3_bgr_data.val[2] = u8x8_b_data;
			u8x8x3_bgr_data.val[1] = u8x8_g_data;
			u8x8x3_bgr_data.val[0] = u8x8_r_data;
			vst3_u8(bgr_image, u8x8x3_bgr_data);
			
			bgr_image += (npixels_per_loop * nchannels);
			y_pos += npixels_per_loop;
			row_loop_counter++;
		}
	}
#endif
}

void defog::bgr2yuv(
	uint8_t *bgr_image,
	int32_t width,
	int32_t height,
	uint8_t *yuv_image
)	{
	assert(bgr_image);
	assert(yuv_image);
#ifdef __ARM_NEON__
	const int32_t nchannels = 3;
	const int32_t npixels = width * height;
	const int32_t npixels_per_loop = 8;
	float32x4_t f32x4_16 = vdupq_n_f32(16);
	float32x4_t f32x4_128 = vdupq_n_f32(128);
	uint8_t *y_start = yuv_image;
	uint8_t *u_start = y_start + npixels;
	uint8_t *v_start = u_start + (npixels >> 2);
	uint8_t *y_pos = y_start;
	uint8_t *u_pos = u_start;
	uint8_t *v_pos = v_start;

	for (int32_t y = 0; y < height; y++) {
		int32_t row_loop_counter = 0;
		int16x4x2_t s16x4x2_low_uv_data;
		for (int32_t x = 0; x < width; x += npixels_per_loop) {
			// Load BGR from DDR.
			uint8x8x3_t u8x8x3_bgr_data = vld3_u8(bgr_image);
			
			uint16x8x3_t u16x8x3_bgr_data;
			u16x8x3_bgr_data.val[0] = vmovl_u8(u8x8x3_bgr_data.val[0]);
			u16x8x3_bgr_data.val[1] = vmovl_u8(u8x8x3_bgr_data.val[1]);
			u16x8x3_bgr_data.val[2] = vmovl_u8(u8x8x3_bgr_data.val[2]);
			
			uint16x4x3_t u16x4x3_low_bgr_data;
			u16x4x3_low_bgr_data.val[0] = vget_low_u16(u16x8x3_bgr_data.val[0]);
			u16x4x3_low_bgr_data.val[1] = vget_low_u16(u16x8x3_bgr_data.val[1]);
			u16x4x3_low_bgr_data.val[2] = vget_low_u16(u16x8x3_bgr_data.val[2]);
			
			uint16x4x3_t u16x4x3_hig_bgr_data;
			u16x4x3_hig_bgr_data.val[0] = vget_high_u16(u16x8x3_bgr_data.val[0]);
			u16x4x3_hig_bgr_data.val[1] = vget_high_u16(u16x8x3_bgr_data.val[1]);
			u16x4x3_hig_bgr_data.val[2] = vget_high_u16(u16x8x3_bgr_data.val[2]);
			
			uint32x4x3_t u32x4x3_low_bgr_data;
			u32x4x3_low_bgr_data.val[0] = vmovl_u16(u16x4x3_low_bgr_data.val[0]);
			u32x4x3_low_bgr_data.val[1] = vmovl_u16(u16x4x3_low_bgr_data.val[1]);
			u32x4x3_low_bgr_data.val[2] = vmovl_u16(u16x4x3_low_bgr_data.val[2]);
			
			uint32x4x3_t u32x4x3_hig_bgr_data;
			u32x4x3_hig_bgr_data.val[0] = vmovl_u16(u16x4x3_hig_bgr_data.val[0]);
			u32x4x3_hig_bgr_data.val[1] = vmovl_u16(u16x4x3_hig_bgr_data.val[1]);
			u32x4x3_hig_bgr_data.val[2] = vmovl_u16(u16x4x3_hig_bgr_data.val[2]);
			
			float32x4x3_t f32x4x3_low_bgr_data;
			f32x4x3_low_bgr_data.val[0] = vcvtq_f32_u32(u32x4x3_low_bgr_data.val[0]);
			f32x4x3_low_bgr_data.val[1] = vcvtq_f32_u32(u32x4x3_low_bgr_data.val[1]);
			f32x4x3_low_bgr_data.val[2] = vcvtq_f32_u32(u32x4x3_low_bgr_data.val[2]);
			
			float32x4x3_t f32x4x3_hig_bgr_data;
			f32x4x3_hig_bgr_data.val[0] = vcvtq_f32_u32(u32x4x3_hig_bgr_data.val[0]);
			f32x4x3_hig_bgr_data.val[1] = vcvtq_f32_u32(u32x4x3_hig_bgr_data.val[1]);
			f32x4x3_hig_bgr_data.val[2] = vcvtq_f32_u32(u32x4x3_hig_bgr_data.val[2]);
			
			// Y’ = 0.257*R' + 0.504*G' + 0.098*B' + 16.
			float32x4x2_t f32x4x2_y_data;
			f32x4x2_y_data.val[0] = vaddq_f32(vaddq_f32(vaddq_f32(vmulq_n_f32(f32x4x3_low_bgr_data.val[2], 0.098),
				vmulq_n_f32(f32x4x3_low_bgr_data.val[1], 0.504)), vmulq_n_f32(f32x4x3_low_bgr_data.val[0], 0.257)), f32x4_16);
			
			f32x4x2_y_data.val[1] = vaddq_f32(vaddq_f32(vaddq_f32(vmulq_n_f32(f32x4x3_hig_bgr_data.val[2], 0.098),
				vmulq_n_f32(f32x4x3_hig_bgr_data.val[1], 0.504)), vmulq_n_f32(f32x4x3_hig_bgr_data.val[0], 0.257)), f32x4_16);
			
			int32x4x2_t s32x4x2_y_data;
			s32x4x2_y_data.val[0] = vcvtq_s32_f32(f32x4x2_y_data.val[0]);
			s32x4x2_y_data.val[1] = vcvtq_s32_f32(f32x4x2_y_data.val[1]);
			
			int16x4x2_t s16x4x2_y_data;
			s16x4x2_y_data.val[0] = vmovn_s32(s32x4x2_y_data.val[0]);
			s16x4x2_y_data.val[1] = vmovn_s32(s32x4x2_y_data.val[1]);
			
			int16x8_t s16x8_y_data = vcombine_s16(s16x4x2_y_data.val[0], s16x4x2_y_data.val[1]);
			uint8x8_t u8x8_y_data = vqmovun_s16(s16x8_y_data);
			
			if (0 == y % 2) {
				// B1 B2 <= B1 B2 B3 B4.
				float32x2x3_t f32x2x3_low_low_bgr_data;
				f32x2x3_low_low_bgr_data.val[0] = vget_low_f32(f32x4x3_low_bgr_data.val[0]);
				f32x2x3_low_low_bgr_data.val[1] = vget_low_f32(f32x4x3_low_bgr_data.val[1]);
				f32x2x3_low_low_bgr_data.val[2] = vget_low_f32(f32x4x3_low_bgr_data.val[2]);

				// B3 B4 <= B1 B2 B3 B4.
				float32x2x3_t f32x2x3_low_hig_bgr_data;
				f32x2x3_low_hig_bgr_data.val[0] = vget_high_f32(f32x4x3_low_bgr_data.val[0]);
				f32x2x3_low_hig_bgr_data.val[1] = vget_high_f32(f32x4x3_low_bgr_data.val[1]);
				f32x2x3_low_hig_bgr_data.val[2] = vget_high_f32(f32x4x3_low_bgr_data.val[2]);
				
				// B1 B3;B2 B4 <= B1 B2;B3 B4.
				float32x2x2_t f32x2x2_low_b_zip_data = vzip_f32(f32x2x3_low_low_bgr_data.val[0], f32x2x3_low_hig_bgr_data.val[0]);
				// G1 G3;G2 G4 <= G1 G2;G3 G4.				
				float32x2x2_t f32x2x2_low_g_zip_data = vzip_f32(f32x2x3_low_low_bgr_data.val[1], f32x2x3_low_hig_bgr_data.val[1]);
				// R1 R3;R2 R4 <= R1 R2;R3 R4.
				float32x2x2_t f32x2x2_low_r_zip_data = vzip_f32(f32x2x3_low_low_bgr_data.val[2], f32x2x3_low_hig_bgr_data.val[2]);

				// B5 B6 <= B5 B6 B7 B8.
				float32x2x3_t f32x2x3_hig_low_bgr_data;
				f32x2x3_hig_low_bgr_data.val[0] = vget_low_f32(f32x4x3_hig_bgr_data.val[0]);
				f32x2x3_hig_low_bgr_data.val[1] = vget_low_f32(f32x4x3_hig_bgr_data.val[1]);
				f32x2x3_hig_low_bgr_data.val[2] = vget_low_f32(f32x4x3_hig_bgr_data.val[2]);
				// B7 B8 <= B5 B6 B7 B8.
				float32x2x3_t f32x2x3_hig_hig_bgr_data;
				f32x2x3_hig_hig_bgr_data.val[0] = vget_high_f32(f32x4x3_hig_bgr_data.val[0]);
				f32x2x3_hig_hig_bgr_data.val[1] = vget_high_f32(f32x4x3_hig_bgr_data.val[1]);
				f32x2x3_hig_hig_bgr_data.val[2] = vget_high_f32(f32x4x3_hig_bgr_data.val[2]);
				
				// B5 B7;B6 B8 <= B5 B6;B7 B8.
				float32x2x2_t f32x2x2_hig_b_zip_data = vzip_f32(f32x2x3_hig_low_bgr_data.val[0], f32x2x3_hig_hig_bgr_data.val[0]);
				// G5 G7;G6 G8 <= G5 G6;G7 G8.
				float32x2x2_t f32x2x2_hig_g_zip_data = vzip_f32(f32x2x3_hig_low_bgr_data.val[1], f32x2x3_hig_hig_bgr_data.val[1]);
				// R5 R7;R6 R8 <= R5 R6;R7 R8.
				float32x2x2_t f32x2x2_hig_r_zip_data = vzip_f32(f32x2x3_hig_low_bgr_data.val[2], f32x2x3_hig_hig_bgr_data.val[2]);
				
				// B1 B3 B5 B7;G1 G3 G5 G7;R1 R3 R5 R7 <= B1 B3;B5 B6;G1 G3;G5 G7;R1 R3;G5 R7.
				float32x4x3_t f32x4x3_samp_bgr_data;
				f32x4x3_samp_bgr_data.val[0] = vcombine_f32(f32x2x2_low_b_zip_data.val[0], f32x2x2_hig_b_zip_data.val[0]);
				f32x4x3_samp_bgr_data.val[1] = vcombine_f32(f32x2x2_low_g_zip_data.val[0], f32x2x2_hig_g_zip_data.val[0]);
				f32x4x3_samp_bgr_data.val[2] = vcombine_f32(f32x2x2_low_r_zip_data.val[0], f32x2x2_hig_r_zip_data.val[0]);
				
				// Cb' = -0.148*R' - 0.291*G' + 0.439*B' + 128.
				float32x4x2_t f32x4x2_uv_data;
				f32x4x2_uv_data.val[0] = vaddq_f32(vaddq_f32(vaddq_f32(vmulq_n_f32(f32x4x3_samp_bgr_data.val[2], 0.439),
					vmulq_n_f32(f32x4x3_samp_bgr_data.val[1], -0.291)), vmulq_n_f32(f32x4x3_samp_bgr_data.val[0], -0.148)), f32x4_128);
				// Cr' = 0.439*R' - 0.368*G' - 0.071*B' + 128.
				f32x4x2_uv_data.val[1] = vaddq_f32(vaddq_f32(vaddq_f32(vmulq_n_f32(f32x4x3_samp_bgr_data.val[2], -0.071),
					vmulq_n_f32(f32x4x3_samp_bgr_data.val[1], -0.368)), vmulq_n_f32(f32x4x3_samp_bgr_data.val[0], 0.439)), f32x4_128);
				
				int32x4x2_t s32x4x2_uv_data;
				s32x4x2_uv_data.val[0] = vcvtq_s32_f32(f32x4x2_uv_data.val[0]);
				s32x4x2_uv_data.val[1] = vcvtq_s32_f32(f32x4x2_uv_data.val[1]);
				
				int16x4x2_t s16x4x2_uv_data;
				s16x4x2_uv_data.val[0] = vmovn_s32(s32x4x2_uv_data.val[0]);
				s16x4x2_uv_data.val[1] = vmovn_s32(s32x4x2_uv_data.val[1]);
				
				// Combine lower four bytes and higher four bytes.
				if (1 == row_loop_counter % 2) {
					int16x8x2_t s16x8x2_uv_data;
					s16x8x2_uv_data.val[0] = vcombine_s16(s16x4x2_low_uv_data.val[0], s16x4x2_uv_data.val[0]);
					s16x8x2_uv_data.val[1] = vcombine_s16(s16x4x2_low_uv_data.val[1], s16x4x2_uv_data.val[1]);
					
					uint8x8x2_t u8x8_uv_data;
					u8x8_uv_data.val[0] = vqmovun_s16(s16x8x2_uv_data.val[0]);
					u8x8_uv_data.val[1] = vqmovun_s16(s16x8x2_uv_data.val[1]);
					
					// Write U, V to DDR.
					vst1_u8(u_pos, u8x8_uv_data.val[0]);
					vst1_u8(v_pos, u8x8_uv_data.val[1]);
					
					u_pos += npixels_per_loop;
					v_pos += npixels_per_loop;
				} else {
					s16x4x2_low_uv_data.val[0] = s16x4x2_uv_data.val[0];
					s16x4x2_low_uv_data.val[1] = s16x4x2_uv_data.val[1];
				}
				row_loop_counter++;
			}
			
			// Write Y to DDR.
			vst1_u8(y_pos, u8x8_y_data);
			
			y_pos += npixels_per_loop;
			bgr_image += (npixels_per_loop * nchannels);
		}
	}
#endif
}

//---------------------------------------------------------
// Minimum filter thread.
//---------------------------------------------------------
void *min_filter_thread(
	void *param
)	{
	min_filter_thread_param_t *min_filt = (min_filter_thread_param_t *)param;
	min_filter<uint8_t>(min_filt->raw_data, min_filt->width, min_filt->height, min_filt->ksize, min_filt->processed_data);
	return (void *)(0);
}

//---------------------------------------------------------
// Minimum filter.
//---------------------------------------------------------
void defog::min_filter(
	uint8_t *raw_image[3],
	int32_t width,
	int32_t height,
	int32_t ksize,
	uint8_t *processed_image[3]
)	{
	const int32_t nchannels = 3;
	pthread_t tid[nchannels];
	min_filter_thread_param_t min_filt[nchannels];
	
	for (int32_t c = 0; c < nchannels; c++) {
		min_filt[c].raw_data = raw_image[c];
		min_filt[c].width = width;
		min_filt[c].height = height;
		min_filt[c].ksize = ksize;
		min_filt[c].processed_data = processed_image[c];
	}
	
	for (int32_t c = 0; c < nchannels; c++) {
		int32_t ret = pthread_create(&tid[c], NULL, min_filter_thread, &min_filt[c]);
		if (0 != ret) {
			printf("Create minimum filter thread[%d] fail!\n", c);
			exit(-1);
		}
	}

	for (int32_t c = 0; c < nchannels; c++) {
		pthread_join(tid[c], NULL);
	}
}

//---------------------------------------------------------
// Guided filter.
//---------------------------------------------------------
cv::Mat defog::guided_filter(
	cv::Mat G,
	cv::Mat P,
	int ksize
)	{
	cv::Mat PG = P.mul(G);
	cv::Mat MPG;
	cv::boxFilter(PG, MPG, CV_32FC1, cv::Size(ksize, ksize));	// Mean of PG.

	cv::Mat MP, MG;
	cv::boxFilter(P, MP, CV_32FC1, cv::Size(ksize, ksize));		// Mean of P.
	cv::boxFilter(G, MG, CV_32FC1, cv::Size(ksize, ksize));		// Mean of G
	cv::Mat CovPG = MPG - MP.mul(MG);						// Covariance of MP and MG.

	cv::Mat GG = G.mul(G);
	cv::Mat MGG;
	cv::boxFilter(GG, MGG, CV_32FC1, cv::Size(ksize, ksize));	// Mean of GG.
	cv::Mat VarG = MGG - MG.mul(MG);						// Variance of MG.
	  
	double eps = 1.0e-5;
	cv::Mat A = CovPG / (VarG + eps);
	cv::Mat B = MP - A.mul(MG);

	cv::Mat MA, MB;
	cv::boxFilter(A, MA, CV_32FC1, cv::Size(ksize, ksize));		// Mean of A.
	cv::boxFilter(B, MB, CV_32FC1, cv::Size(ksize, ksize));		// Mean of B.

	cv::Mat Q = MA.mul(G) + MB;
	return Q;
}

//---------------------------------------------------------
// Totally process function.
//---------------------------------------------------------
void defog::process_rgb_dp(
	uint8_t *raw_image
)	{
	hazzy_img = raw_image;
	
	if (false == enable_module) {
		return;
	}

#ifdef EASY_TEST_DEFOG
	clock_t start = clock();
#endif
#ifdef TEST_DEFOG
	clock_t start, finish, total = 0;
	start = clock();
#endif
	if (frame_index % update_period == 0) {
		cv::resize(cv::Mat(height, width, CV_8UC3, hazzy_img), cv::Mat(ds_height, ds_width, CV_8UC3, dsbgr_image),
			cv::Size(ds_width, ds_height));
#ifdef TEST_DEFOG
		finish = clock();
		total += finish - start;
		printf("color resize %.0lfms.\n", 1000.0 * (finish - start) / CLOCKS_PER_SEC);
		start = clock();
#endif		
		cv::Mat dsbgr_mat[3] = {cv::Mat(ds_height, ds_width, CV_8UC1, dsrgb_img[2]),
			cv::Mat(ds_height, ds_width, CV_8UC1, dsrgb_img[1]), cv::Mat(ds_height, ds_width, CV_8UC1, dsrgb_img[0])};
		cv::split(cv::Mat(ds_height, ds_width, CV_8UC3, dsbgr_image), dsbgr_mat);
#ifdef TEST_DEFOG
		finish = clock();
		total += finish - start;
		printf("channel split %.0lfms.\n", 1000.0 * (finish - start) / CLOCKS_PER_SEC);
		start = clock();
#endif	
		// Minimum filter for every channel.
		min_filter(dsrgb_img, ds_width, ds_height, kmin_size, min_filt_rgb_img);
#ifdef TEST_DEFOG
		finish = clock();
		total += finish - start;
		printf("min_filter %.0lfms.\n", 1000.0 * (finish - start) / CLOCKS_PER_SEC);
		start = clock();
#endif
		// Calculate minimum channel image.
		min_channel<uint8_t>(min_filt_rgb_img, min_chan_img, ds_width, ds_height);
#ifdef TEST_DEFOG
		finish = clock();
		total += finish - start;
		printf("min_channel1 %.0lfms.\n", 1000.0 * (finish - start) / CLOCKS_PER_SEC);
		start = clock();
#endif
		// Estimate atmospheric light.
		estimate_atmospheric_light(dsrgb_img, min_chan_img, ds_width, ds_height, atmo_light_ceiling, atmo_light);
#ifdef TEST_DEFOG
		finish = clock();
		total += finish - start;
		printf("estimate_atmospheric_light %.0lfms.\n", 1000.0 * (finish - start) / CLOCKS_PER_SEC);
		start = clock();
#endif
		// Calculate normalized minimum filter image.
		normalize_min_filt_image(min_filt_rgb_img, ds_width, ds_height, atmo_light, norm_min_filt_rgb_img);
#ifdef TEST_DEFOG
		finish = clock();
		total += finish - start;
		printf("normalize_min_filt_image %.0lfms.\n", 1000.0 * (finish - start) / CLOCKS_PER_SEC);
		start = clock();
#endif
		// Calculate normalized minimum channel image.
		min_channel<float>(norm_min_filt_rgb_img, norm_min_chan_img, ds_width, ds_height);
#ifdef TEST_DEFOG
		finish = clock();
		total += finish - start;
		printf("min_channel2 %.0lfms.\n", 1000.0 * (finish - start) / CLOCKS_PER_SEC);
		start = clock();
#endif
		// Estimate transmission map.
		estimate_transmission(norm_min_chan_img, ds_width, ds_height, omega, transm_img);
#ifdef TEST_DEFOG
		finish = clock();
		total += finish - start;
		printf("estimate_transmission %.0lfms.\n", 1000.0 * (finish - start) / CLOCKS_PER_SEC);
		start = clock();
#endif
		// Refine transmission map with guided filter.
		refine_transmission(dsrgb_img[0], transm_img, ds_width, ds_height, kgud_size, tran_thresh);
#ifdef TEST_DEFOG
		finish = clock();
		total += finish - start;
		printf("refine_transmission %.0lfms.\n", 1000.0 * (finish - start) / CLOCKS_PER_SEC);
		start = clock();
#endif	
		inverse_transmission(transm_img, ds_width, ds_height, transm_inv);
#ifdef TEST_DEFOG
		finish = clock();
		total += finish - start;
		printf("inverse_transmission %.0lfms.\n", 1000.0 * (finish - start) / CLOCKS_PER_SEC);
		start = clock();
#endif
		cv::resize(cv::Mat(ds_height, ds_width, CV_32FC1, transm_inv), cv::Mat(height, width, CV_32FC1,
			ustransm_img), cv::Size(width, height));
#ifdef TEST_DEFOG
		finish = clock();
		total += finish - start;
		printf("linear_resample %.0lfms.\n", 1000.0 * (finish - start) / CLOCKS_PER_SEC);
		start = clock();
#endif
	}
#ifdef TEST_DEFOG
	start = clock();
#endif
	// Calculate recover scene radiance image.
#ifdef _WIN32
	recover_scene_radiance(hazzy_img, ustransm_img, width, height, atmo_light, recover_img);
#else
	neon_recover_scene_radiance(hazzy_img, ustransm_img, width, height, atmo_light, recover_img);
#endif
#ifdef TEST_DEFOG
	finish = clock();
	total += finish - start;
#ifdef _WIN32
	printf("recover_scene_radiance %.0lfms.\n", 1000.0 * (finish - start) / CLOCKS_PER_SEC);
#else
	printf("neon_recover_scene_radiance %.0lfms.\n", 1000.0 * (finish - start) / CLOCKS_PER_SEC);
#endif
#endif
	if (frame_index % update_period == 0) {
#ifdef TEST_DEFOG
		start = clock();
#endif
		auto_level_thresh(recover_img, width, height, lowcut_thresh, highcut_thresh, auto_level_floor, auto_level_ceiling);
#ifdef TEST_DEFOG
		finish = clock();
		total += finish - start;
		printf("auto_level_thresh %.0lfms.\n", 1000.0 * (finish - start) / CLOCKS_PER_SEC);
		start = clock();
#endif
		make_stretch_table(auto_level_floor, auto_level_ceiling, stretch_table);
#ifdef TEST_DEFOG
		finish = clock();
		total += finish - start;
		printf("make_stretch_table %.0lfms.\n", 1000.0 * (finish - start) / CLOCKS_PER_SEC);
		start = clock();
#endif
	}
#ifdef TEST_DEFOG
	start = clock();
#endif
	// Auto levels image.
	auto_levels(recover_img, width, height, stretch_table, stretch_img);
#ifdef TEST_DEFOG
	finish = clock();
	total += finish - start;
	printf("auto_levels %.0lfms.\n", 1000.0 * (finish - start) / CLOCKS_PER_SEC);
	printf("total %.0lfms.\n", 1000.0 * total / CLOCKS_PER_SEC);
#endif
#ifdef EASY_TEST_DEFOG
	clock_t finish = clock();
	printf("total %.0lfms.\n", 1000.0 * (finish - start) / CLOCKS_PER_SEC);
#endif	
	frame_index++;
}

//---------------------------------------------------------
// Enhance YUV image with DEFOG.
//---------------------------------------------------------
void defog::process_yuv_dp(
	uint8_t *yuv_image
)	{
	assert(yuv_image);
	
	if (false == enable_module) {
		return;
	}
	
#ifdef TEST_DEFOG
	clock_t start = clock();
#endif
	yuv2bgr(yuv_image, width, height, bgr_image);
#ifdef TEST_DEFOG
	clock_t finish = clock();
	printf("\nyuv2bgr %.0lfms.\n", 1000.0 * (finish - start) / CLOCKS_PER_SEC);
#endif
	process_rgb_dp(bgr_image);

#ifdef TEST_DEFOG
	start = clock();
#endif
	bgr2yuv(stretch_img, width, height, yuv_image);
#ifdef TEST_DEFOG
	finish = clock();
	printf("bgr2yuv %.0lfms.\n", 1000.0 * (finish - start) / CLOCKS_PER_SEC);
#endif	
}

//---------------------------------------------------------
// Find the minimum with Neon acceleration.
//---------------------------------------------------------
static void neon_min(
	uint8_t *image,
	int32_t npixels,
	uint8_t *minv
)	{
	assert(image);
#ifdef __ARM_NEON__ 	
	uint8x16_t min_val = vdupq_n_u8(255);
	for (int32_t i = 0; i < npixels; i += 16) {
		uint8x16_t data = vld1q_u8(image);
		min_val = vminq_u8(data, min_val);
		image += 16;
	}
	
	uint8_t min_buf[16];
	vst1q_u8(min_buf, min_val);
	*minv = 255;
	
	for (int32_t i = 0; i < 16; i++) {
		if (min_buf[i] < *minv) {
			*minv = min_buf[i];
		}
	}
#endif
}

//---------------------------------------------------------
// Find the maximum with Neon acceleration.
//---------------------------------------------------------
static void neon_max(
	uint8_t *image,
	int32_t npixels,
	uint8_t *maxv
)	{
	assert(image);
#ifdef __ARM_NEON__ 	
	uint8x16_t max_val = vdupq_n_u8(0);
	for (int32_t i = 0; i < npixels; i += 16) {
		uint8x16_t data = vld1q_u8(image);
		max_val = vmaxq_u8(data, max_val);
		image += 16;
	}
	
	uint8_t max_buf[16];
	vst1q_u8(max_buf, max_val);
	*maxv = 0;
	
	for (int32_t i = 0; i < 16; i++) {
		if (max_buf[i] > *maxv) {
			*maxv = max_buf[i];
		}
	}
#endif
}

//---------------------------------------------------------
// Find the minimum and maximum with Neon acceleration.
//---------------------------------------------------------
static void neon_minmax(
	uint8_t *image,
	int32_t npixels,
	uint8_t *minv,
	uint8_t *maxv
)	{
	assert(image);
#ifdef __ARM_NEON__ 	
	uint8x16_t min_val = vdupq_n_u8(255);
	uint8x16_t max_val = vdupq_n_u8(0);
	for (int32_t i = 0; i < npixels; i += 16) {
		uint8x16_t data = vld1q_u8(image);
		min_val = vminq_u8(data, min_val);
		max_val = vmaxq_u8(data, max_val);
		image += 16;
	}
	
	uint8_t min_buf[16];
	vst1q_u8(min_buf, min_val);
	*minv = 255;
	
	uint8_t max_buf[16];
	vst1q_u8(max_buf, max_val);
	*maxv = 0;
	
	for (int32_t i = 0; i < 16; i++) {
		if (min_buf[i] < *minv) {
			*minv = min_buf[i];
		}
		if (max_buf[i] > *maxv) {
			*maxv = max_buf[i];
		}
	}
#endif
}

//---------------------------------------------------------
// Test adjust UV components.
//---------------------------------------------------------
static void adjust_uv(
	uint8_t *raw_y,
	uint8_t *new_y,
	uint8_t *uv,
	int32_t width,
	int32_t height
)	{
	uint8_t *pU = uv;
	uint8_t *pV = pU + ((width * height) / 4);
	const int32_t uv_height = height / 2;
	const int32_t uv_width = width / 2;
	int32_t counter = 0;
	for (int32_t y = 0; y < uv_height; y++) {
		for (int32_t x = 0; x < uv_width; x++) {
			int32_t uv_idx = y * uv_width + x;
			int8_t cb = (int32_t)pU[uv_idx] - 128;
			int8_t cr = (int32_t)pV[uv_idx] - 128;
			float sat = sqrt(cb * cb + cr * cr);
			float theta = atan2(cr, cb);
			float hue = 180 * theta / 3.14159f + 180;
			uint32_t u32_hue = (uint32_t)hue;
			
			float vx, vy;
			if ((u32_hue > 319 && u32_hue <= 360) || (u32_hue >= 0 && u32_hue < 22)) {
				vx = 95;
				vy = 87;
			} else if (u32_hue >= 22 && u32_hue < 80) {
				vx = 105;
				vy = 83;
			} else if (u32_hue >= 80 && u32_hue < 138) {
				vx = 94;
				vy = 123;
			} else if (u32_hue >= 138 && u32_hue < 202) {
				vx = 95;
				vy = 168;
			} else if (u32_hue >= 202 && u32_hue < 259) {
				vx = 105;
				vy = 172;
			} else {
				vx = 94;
				vy = 131;
			}
			
			float S0, S1;
			int32_t y_idx = (y * 2) * width + x * 2;
			uint8_t Y0 = raw_y[y_idx];
			uint8_t Y1 = new_y[y_idx];
			
			if (Y0 < vy) {
				S0 = vx * Y0 / vy;
			} else {
				S0 = vx * (Y0 - 255) / (vy - 255);
			}
			
			if (Y1 < vy) {
				S1 = vx * Y1 / vy;
			} else {
				S1 = vx * (Y1 - 255) / (vy - 255);
			}
			
			float new_sat;
			if (0 == Y0) {
				new_sat = sat;
			} else {
				new_sat = sat * S1 / S0;
			}
			
			if (new_sat > sat) {counter++;}
			
			int8_t new_cb, new_cr;
			if (cb == 0) {
				new_cb = cb;
				new_cr = cr;
			} else {
				float tan_hue = (float)cr / cb;
				new_cb = new_sat / sqrt(1.0 + tan_hue * tan_hue);
				if (cb < 0) {
					new_cb = -new_cb;
				}
				
				new_cr = tan_hue * new_cb;
			}

			pU[uv_idx] = (int32_t)new_cb + 128;
			pV[uv_idx] = (int32_t)new_cr + 128;
		}
	}
}

//---------------------------------------------------------
// Test classify image type.
//---------------------------------------------------------
static void statistic_feature(
	uint8_t *yuv_image,
	int32_t width,
	int32_t height
)	{
	assert(yuv_image);
	const int32_t npixels = width * height;
	unsigned long hist[256];
	memset(hist, 0, sizeof(hist));
	
	for (int32_t i = 0; i < npixels; i++) {
		hist[yuv_image[i]]++;
	}

	unsigned long ndark_pixels = 0;
	unsigned long npoor_contrast_pixels = 0;
	unsigned long nhigh_brightness_pixels = 0;
	for (int32_t i = 0; i < 127; i++) {
		ndark_pixels += hist[i];
	}
	
	for (int32_t i = 85; i < 170; i++) {
		npoor_contrast_pixels += hist[i];
	}
	
	for (int32_t i = 128; i <= 255; i++) {
		nhigh_brightness_pixels += hist[i];
	}
	
	const unsigned long threshold = 0.7 * width * height;
	if (ndark_pixels > threshold) {
		printf("dark image with %f!\n", (float)ndark_pixels / npixels);
	} else if (npoor_contrast_pixels > threshold) {
		printf("poor contrast image with %f!\n", (float)npoor_contrast_pixels / npixels);
	} else if (nhigh_brightness_pixels > threshold) {
		printf("high brightness image with %f!\n", (float)nhigh_brightness_pixels / npixels);
	} else {
		printf("normal image!\n");
	}
}

//---------------------------------------------------------
// Test adjust UV.
//---------------------------------------------------------
void defog::adjust_uv_HY(
	uint8_t *raw_y,
	uint8_t *new_y,
	uint8_t *uv,
	int32_t width,
	int32_t height
)	{
	uint8_t *pU = uv;
	uint8_t *pV = pU + ((width * height) / 4);
	const int32_t uv_height = height / 2;
	const int32_t uv_width = width / 2;
	int32_t counter = 0;
	for (int32_t y = 0; y < uv_height; y++) {
		for (int32_t x = 0; x < uv_width; x++) {
			int32_t uv_idx = y * uv_width + x;
			int32_t y_idx = (y * 2) * width + x * 2;
			uint8_t Y0 = raw_y[y_idx];
			uint8_t Y1 = new_y[y_idx];
			float y_chag = (float)Y1 / Y0;
			int8_t cb = (int32_t)pU[uv_idx] - 128;
			int8_t cr = (int32_t)pV[uv_idx] - 128;
			float uv_chag = 127.0f / std::max<uint8_t>(abs(cb), abs(cr));
			float chag = 1.2f * y_chag;
			// printf("cb %d, cr %d, uv_chag %f, chag %f\n", cb, cr, uv_chag, chag);
			if (uv_chag < chag)	counter++;
			chag = std::min<float>(chag, uv_chag);
						
			pU[uv_idx] = cv::saturate_cast<uint8_t>(chag * cb + 128);
			pV[uv_idx] = cv::saturate_cast<uint8_t>(chag * cr + 128);
		}
	}
	printf("uv_chag < y_chag: %d\n", counter);
}

//---------------------------------------------------------
// Saturation adjustment with Neon speed up.
//---------------------------------------------------------
void defog::saturation_adjustment(
	uint8_t *raw_y,
	uint8_t *new_y,
	uint8_t *uv,
	int32_t width,
	int32_t height
)	{
	assert(raw_y);
	assert(new_y);
	assert(uv);
#ifdef __ARM_NEON__ 
	uint8_t *pU = uv;
	uint8_t *pV = pU + ((width * height) / 4);
	const int32_t uv_height = height / 2;
	const int32_t uv_width = width / 2;
	uint8x8_t y0_vec;
	uint8x8_t y1_vec;
	
	for (int32_t y = 0; y < uv_height; y++) {
		for (int32_t x = 0; x < uv_width; x += 8) {
			uint8x16_t y_raw_vec = vld1q_u8(raw_y);
			uint8x16_t y_new_vec = vld1q_u8(new_y);
					
			for (int32_t i = 0, j = 0; i < 8; i++, j += 2) {
				y0_vec[i] = y_raw_vec[j];
				y1_vec[i] = y_new_vec[j];
			}

			uint16x8_t u16_y0_vec = vmovl_u8(y0_vec);
			uint16x8_t u16_y1_vec = vmovl_u8(y1_vec);
			
			// y0.
			float32x4_t f32_y0_vec[2] = {
				vcvtq_f32_u32(vmovl_u16(vget_low_u16(u16_y0_vec))),
				vcvtq_f32_u32(vmovl_u16(vget_high_u16(u16_y0_vec))),
			}; 
			
			// 1 / y0.
			float32x4_t f32_y0_inv_vec[2];
			
			float32x4_t inverse = vrecpeq_f32(f32_y0_vec[0]);
			float32x4_t restep = vrecpsq_f32(f32_y0_vec[0], inverse);
			f32_y0_inv_vec[0] = vmulq_f32(restep, inverse);
			
			inverse = vrecpeq_f32(f32_y0_vec[1]);
			restep = vrecpsq_f32(f32_y0_vec[1], inverse);
			f32_y0_inv_vec[1] = vmulq_f32(restep, inverse);

			// y1.
			float32x4_t f32_y1_vec[2] = {
				vcvtq_f32_u32(vmovl_u16(vget_low_u16(u16_y1_vec))),
				vcvtq_f32_u32(vmovl_u16(vget_high_u16(u16_y1_vec))),
			};
			
			// k * y1 / y0.
			float32x4_t y_dev[2] = {
				vmulq_n_f32(vmulq_f32(f32_y1_vec[0], f32_y0_inv_vec[0]), 1.2f),
				vmulq_n_f32(vmulq_f32(f32_y1_vec[1], f32_y0_inv_vec[1]), 1.2f)
			};

			uint8x8_t u0_vec = vld1_u8(pU);
			uint8x8_t v0_vec = vld1_u8(pV);

			int16x8_t s16_u0_vec = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(u0_vec)), vdupq_n_s16(128));
			int16x8_t s16_v0_vec = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(v0_vec)), vdupq_n_s16(128));
			
			float32x4_t f32_u0_vec[2] = {
				vcvtq_f32_s32(vmovl_s16(vget_low_s16(s16_u0_vec))),
				vcvtq_f32_s32(vmovl_s16(vget_high_s16(s16_u0_vec)))
			};

			float32x4_t f32_v0_vec[2] = {
				vcvtq_f32_s32(vmovl_s16(vget_low_s16(s16_v0_vec))),
				vcvtq_f32_s32(vmovl_s16(vget_high_s16(s16_v0_vec)))
			};
			
			// u1.
			int16x4_t s16_u1_vec[2] = {
				vmovn_s32(vcvtq_s32_f32(vaddq_f32(vmulq_f32(y_dev[0], f32_u0_vec[0]), vdupq_n_f32(128)))),
				vmovn_s32(vcvtq_s32_f32(vaddq_f32(vmulq_f32(y_dev[1], f32_u0_vec[1]), vdupq_n_f32(128))))
			};
			
			uint8x8_t u1_vec = vqmovun_s16(vcombine_s16(s16_u1_vec[0], s16_u1_vec[1]));

			// v1.
			int16x4_t s16_v1_vec[2] = {
				vmovn_s32(vcvtq_s32_f32(vaddq_f32(vmulq_f32(y_dev[0], f32_v0_vec[0]), vdupq_n_f32(128)))),
				vmovn_s32(vcvtq_s32_f32(vaddq_f32(vmulq_f32(y_dev[1], f32_v0_vec[1]), vdupq_n_f32(128))))
			};
			
			uint8x8_t v1_vec = vqmovun_s16(vcombine_s16(s16_v1_vec[0], s16_v1_vec[1]));
			
			// Save.
			vst1_u8(pU, u1_vec);
			vst1_u8(pV, v1_vec);
			
			pU += 8;
			pV += 8;
			raw_y += 16;
			new_y += 16;
		}
		raw_y += width;
		new_y += width;
	}
#endif
}

//---------------------------------------------------------
// Segmented linear transformation.
//---------------------------------------------------------
static void seg_linar_transf(
	uint8_t *image,
	int32_t width,
	int32_t height,
	uint8_t low_in,
	uint8_t low_out,
	uint8_t hig_in,
	uint8_t hig_out
)	{
	assert(image);
#ifdef __ARM_NEON__ 	
	const int32_t npixels = width * height;
	const int16_t scale[3] = {
		(int16_t)((low_out << 4) / low_in),
		(int16_t)(((hig_out - low_out) << 4) / (hig_in - low_in)),
		(int16_t)(((255 - hig_out) << 4) / (255 - hig_in))
	};
	const int16_t bias[3] = {
		0,
		(int16_t)((low_out << 4) - scale[1] * low_in),
		(int16_t)((hig_out << 4) - scale[2] * hig_in)
	};

	int16x8_t scale_vec;
	int16x8_t bias_vec;
	uint8x8_t result_vec;
	
	for (int32_t i = 0; i < npixels; i += 8) {
		uint8x8_t load_vec = vld1_u8(image);
		for (int32_t j = 0; j < 8; j++) {
			if (load_vec[j] < low_in) {
				scale_vec[j] = scale[0];
				bias_vec[j] = bias[0];
			} else if (load_vec[j] >= low_in && load_vec[j] < hig_in) {
				scale_vec[j] = scale[1];
				bias_vec[j] = bias[1];
			} else {
				scale_vec[j] = scale[2];
				bias_vec[j] = bias[2];
			}
		}
		
		result_vec = vqmovun_s16(vshrq_n_s16(vaddq_s16(vmulq_s16(scale_vec,
			vreinterpretq_s16_u16(vmovl_u8(load_vec))), bias_vec), 4));
		
		vst1_u8(image, result_vec);
		image += 8;
	}
#endif
}

//---------------------------------------------------------
// Convolution with NEON speed up.
//---------------------------------------------------------
#ifdef __ARM_NEON__ 
static void filter3x3(
	uint8_t *raw_image,
	int32_t width,
	int32_t height,
	int16x8_t *mask,
	uint8_t *new_image
)	{
	assert(raw_image);
	assert(mask);
	assert(new_image);
	
	uint8_t *raw_line[3] = {raw_image, raw_image + width, raw_image + 2 * width};
	uint8_t *new_line = new_image + width + 1;
	
	for (int32_t y = 0; y < height - 2; y++) {
		uint8x8_t prev_vec[3];
		prev_vec[0] = vld1_u8(raw_line[0]);
		prev_vec[1] = vld1_u8(raw_line[1]);
		prev_vec[2] = vld1_u8(raw_line[2]);
		
		raw_line[0] += 8;
		raw_line[1] += 8;
		raw_line[2] += 8;
		
		for (int32_t x = 0; x < width; x += 8) {
			uint8x8_t next_vec[3];
			next_vec[0] = vld1_u8(raw_line[0]);
			next_vec[1] = vld1_u8(raw_line[1]);
			next_vec[2] = vld1_u8(raw_line[2]);
			
			// First line.
			uint8x8_t first_vec = prev_vec[0];
			uint8x8_t secnd_vec = vext_u8(prev_vec[0], next_vec[0], 1);
			uint8x8_t third_vec = vext_u8(prev_vec[0], next_vec[0], 2);
			
			int16x8_t first_prod_vec = vmulq_s16(vreinterpretq_s16_u16(vmovl_u8(first_vec)), mask[0]);			
			int16x8_t secnd_prod_vec = vmulq_s16(vreinterpretq_s16_u16(vmovl_u8(secnd_vec)), mask[1]);		
			int16x8_t third_prod_vec = vmulq_s16(vreinterpretq_s16_u16(vmovl_u8(third_vec)), mask[2]);
					
			int16x8_t add_vec = vdupq_n_s16(0);
			add_vec = vaddq_s16(add_vec, first_prod_vec);
			add_vec = vaddq_s16(add_vec, secnd_prod_vec);
			add_vec = vaddq_s16(add_vec, third_prod_vec);

			// Second line.
			first_vec = prev_vec[1];
			secnd_vec = vext_u8(prev_vec[1], next_vec[1], 1);
			third_vec = vext_u8(prev_vec[1], next_vec[1], 2);
			
			// Keep center pixel vector.
			uint8x8_t original = secnd_vec;
			
			first_prod_vec = vmulq_s16(vreinterpretq_s16_u16(vmovl_u8(first_vec)), mask[3]);
			secnd_prod_vec = vmulq_s16(vreinterpretq_s16_u16(vmovl_u8(secnd_vec)), mask[4]);
			third_prod_vec = vmulq_s16(vreinterpretq_s16_u16(vmovl_u8(third_vec)), mask[5]);
			
			add_vec = vaddq_s16(add_vec, first_prod_vec);
			add_vec = vaddq_s16(add_vec, secnd_prod_vec);
			add_vec = vaddq_s16(add_vec, third_prod_vec);

			// Third line.
			first_vec = prev_vec[2];
			secnd_vec = vext_u8(prev_vec[2], next_vec[2], 1);
			third_vec = vext_u8(prev_vec[2], next_vec[2], 2);
			
			first_prod_vec = vmulq_s16(vreinterpretq_s16_u16(vmovl_u8(first_vec)), mask[6]);
			secnd_prod_vec = vmulq_s16(vreinterpretq_s16_u16(vmovl_u8(secnd_vec)), mask[7]);
			third_prod_vec = vmulq_s16(vreinterpretq_s16_u16(vmovl_u8(third_vec)), mask[8]);
			
			add_vec = vaddq_s16(add_vec, first_prod_vec);
			add_vec = vaddq_s16(add_vec, secnd_prod_vec);
			add_vec = vaddq_s16(add_vec, third_prod_vec);
						
			// add_vec = vshrq_n_s16(add_vec, 1);
			
			// Last result.
			int16x8_t sub_result = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(original)), add_vec);
			uint8x8_t result = vqmovun_s16(sub_result);
			
			// Save result.
			vst1_u8(new_line, result);
			
			prev_vec[0] = next_vec[0];
			prev_vec[1] = next_vec[1];
			prev_vec[2] = next_vec[2];
			
			// Shift line pointer.
			raw_line[0] += 8;
			raw_line[1] += 8;
			raw_line[2] += 8;
			new_line    += 8;
		}
		// Back to line header.
		raw_line[0] -= 8;
		raw_line[1] -= 8;
		raw_line[2] -= 8;
	}
}
#endif

//---------------------------------------------------------
// Clip gray level of Y component.
//---------------------------------------------------------
static void clip_gray_level(
	uint8_t *image,
	int32_t width,
	int32_t height,
	uint8_t minv,
	uint8_t maxv
)	{
	assert(image);
#ifdef __ARM_NEON__ 
	const int32_t npixels = width * height;
	uint8x16_t floor_vec = vdupq_n_u8(minv);
	uint8x16_t ceiling_vec = vdupq_n_u8(maxv);
	
	for (int32_t i = 0; i < npixels; i += 16) {
		uint8x16_t data_vec = vld1q_u8(image);
		data_vec = vmaxq_u8(data_vec, floor_vec);
		data_vec = vminq_u8(data_vec, ceiling_vec);
		vst1q_u8(image, data_vec);
		image += 16;
	}
#endif
}

//---------------------------------------------------------
// Unsigned short 4x4 matrix transpose.
//---------------------------------------------------------
#ifdef __ARM_NEON__ 
static inline uint16x4x4_t transpose_u16x4x4_matrix(
	uint16x4x4_t matrix
)	{
	uint16x4x2_t u16_trn_temp;
	uint32x2x2_t u32_trn_temp;
	
	u16_trn_temp = vtrn_u16(matrix.val[0], matrix.val[1]);
	matrix.val[0] = u16_trn_temp.val[0];
	matrix.val[1] = u16_trn_temp.val[1];
	
	u16_trn_temp = vtrn_u16(matrix.val[2], matrix.val[3]);
	matrix.val[2] = u16_trn_temp.val[0];
	matrix.val[3] = u16_trn_temp.val[1];
	
	u32_trn_temp = vtrn_u32(vreinterpret_u32_u16(matrix.val[0]), vreinterpret_u32_u16(matrix.val[2]));	
	matrix.val[0] = vreinterpret_u16_u32(u32_trn_temp.val[0]);
	matrix.val[2] = vreinterpret_u16_u32(u32_trn_temp.val[1]);
	
	u32_trn_temp = vtrn_u32(vreinterpret_u32_u16(matrix.val[1]), vreinterpret_u32_u16(matrix.val[3]));	
	matrix.val[1] = vreinterpret_u16_u32(u32_trn_temp.val[0]);
	matrix.val[3] = vreinterpret_u16_u32(u32_trn_temp.val[1]);
	
	return matrix;
}
#endif

//---------------------------------------------------------
// Paeth's sort network.
//---------------------------------------------------------
#ifdef __ARM_NEON__ 
static inline uint8x16_t paeth_sort_network_u8x16x9(
	uint8x16_t q0,
	uint8x16_t q1,
	uint8x16_t q2,
	uint8x16_t q3,
	uint8x16_t q4,
	uint8x16_t q5,
	uint8x16_t q6,
	uint8x16_t q7,
	uint8x16_t q8
)	{
	vminmax_u8(q0, q3);
	vminmax_u8(q1, q4);

	vminmax_u8(q0, q1);
	vminmax_u8(q2, q5);

	vminmax_u8(q0, q2);
	vminmax_u8(q4, q5);

	vminmax_u8(q1, q2);
	vminmax_u8(q3, q5);

	vminmax_u8(q3, q4);

	vminmax_u8(q1, q3);

	vminmax_u8(q1, q6);

	vminmax_u8(q4, q6);

	vminmax_u8(q2, q6);

	vminmax_u8(q2, q3);
	vminmax_u8(q4, q7);

	vminmax_u8(q2, q4);

	vminmax_u8(q3, q7);

	vminmax_u8(q4, q8);

	vminmax_u8(q3, q8);

	vminmax_u8(q3, q4);
	
	return q4;
}
#endif

//---------------------------------------------------------
// Median filter with Neon speed up.
//---------------------------------------------------------
void median_filter3x3(
	uint8_t *raw_image,
	int32_t width,
	int32_t height,
	uint8_t *new_image
)	{
	assert(raw_image);
	assert(new_image);
#ifdef __ARM_NEON__ 
	int32_t x, y;
	const int32_t bytes_per_load = 16;
	const int32_t xlim = 1 + (width / bytes_per_load - 2) * bytes_per_load;
	uint8_t *raw_line[3] = {raw_image, raw_image + width, raw_image + 2 * width};
	
	for (y = 1; y < height - 1; y++) {
		uint8x16_t q0, q1, q2, q3, q4, q5, q6, q7, q8;
		uint8x16_t border_fill = vdupq_n_u8(raw_image[width * y]);
		uint8x16_t prev_q0, prev_q3, prev_q6;
		uint8x16_t next_q0, next_q3, next_q6;
		
		// First column.
		x = 0;
		q1 = vld1q_u8(&raw_image[width * (y - 1) + x]);
		q0 = vextq_u8(border_fill, q1, 15);
		q2 = vld1q_u8(&raw_image[width * (y - 1) + x + 1]);
		
		q4 = vld1q_u8(&raw_image[width * y + x]);
		q3 = vextq_u8(border_fill, q4, 15);
		q5 = vld1q_u8(&raw_image[width * y + x + 1]);
		
		q7 = vld1q_u8(&raw_image[width * (y + 1) + x]);
		q6 = vextq_u8(border_fill, q7, 15);
		q8 = vld1q_u8(&raw_image[width * (y + 1) + x + 1]);
		
		q4 = paeth_sort_network_u8x16x9(q0, q1, q2, q3, q4, q5, q6, q7, q8);

		vst1q_u8(&new_image[width * y + x], q4);
		
		// Initialize previous q0, q3, q6.
		prev_q0 = vld1q_u8(raw_line[0]);
		prev_q3 = vld1q_u8(raw_line[1]);
		prev_q6 = vld1q_u8(raw_line[2]);
		
		raw_line[0] += bytes_per_load;
		raw_line[1] += bytes_per_load;
		raw_line[2] += bytes_per_load;
		
		// Second to (width - 15) column.
		for (x = 1; x <= xlim; x += bytes_per_load) {
			next_q0 = vld1q_u8(raw_line[0]);
			next_q3 = vld1q_u8(raw_line[1]);
			next_q6 = vld1q_u8(raw_line[2]);
			
			// Load 3x3x16 pixels.
			q0 = prev_q0;
			q1 = vextq_u8(prev_q0, next_q0, 1);
			q2 = vextq_u8(prev_q0, next_q0, 2);

			q3 = prev_q3;
			q4 = vextq_u8(prev_q3, next_q3, 1);
			q5 = vextq_u8(prev_q3, next_q3, 2);

			q6 = prev_q6;
			q7 = vextq_u8(prev_q6, next_q6, 1);
			q8 = vextq_u8(prev_q6, next_q6, 2);

			// Paeth's 9-element sorting network.			
			q4 = paeth_sort_network_u8x16x9(q0, q1, q2, q3, q4, q5, q6, q7, q8);

			// Median values in q4 now.
			vst1q_u8(&new_image[width * y + x], q4);
			
			prev_q0 = next_q0;
			prev_q3 = next_q3;
			prev_q6 = next_q6;
			
			raw_line[0] += bytes_per_load;
			raw_line[1] += bytes_per_load;
			raw_line[2] += bytes_per_load;
		}
		
		// (width - 14) to last column.
		border_fill = vdupq_n_u8(raw_image[width * y + width - 1]);
		
		q0 = vld1q_u8(&raw_image[width * (y - 1) + x - 1]);
		q1 = vextq_u8(q0, border_fill, 1);
		q2 = vextq_u8(q0, border_fill, 2);
		
		q3 = vld1q_u8(&raw_image[width * y + x - 1]);
		q4 = vextq_u8(q3, border_fill, 1);
		q5 = vextq_u8(q3, border_fill, 2);
		
		q6 = vld1q_u8(&raw_image[width * (y + 1) + x - 1]);
		q7 = vextq_u8(q6, border_fill, 1);
		q8 = vextq_u8(q6, border_fill, 2);
		
		q4 = paeth_sort_network_u8x16x9(q0, q1, q2, q3, q4, q5, q6, q7, q8);

		vst1q_u8(&new_image[width * y + x], q4);
	}
	
#endif
}

//---------------------------------------------------------
// Segmented linear transformation thread.
//---------------------------------------------------------
void *segment_linear_transf_thread(
	void *param
)	{
	defog *pdefog = (defog *)param;
	printf("start segment_linear_transf_thread\n");
	while (1) {
		pthread_mutex_lock(&pdefog->clip_yuv_image_mutex);
		int32_t queue_size = pdefog->clip_yuv_image_queue.size();
		pthread_mutex_unlock(&pdefog->clip_yuv_image_mutex);
		if (queue_size > 0) {
#ifdef TEST_DEFOG
			printf("clip_yuv_image_queue %d\n", queue_size);
#endif
			pthread_mutex_lock(&pdefog->clip_yuv_image_mutex);
			uint8_t *clip_yuv_image = pdefog->clip_yuv_image_queue.front();
			
			seg_linar_transf(clip_yuv_image, pdefog->width, pdefog->height, pdefog->slt[0],
				pdefog->slt[1], pdefog->slt[2], pdefog->slt[3]);
			
			uint8_t *slt_yuv_image = new uint8_t[pdefog->width * pdefog->height * 3 / 2];
			assert(slt_yuv_image);
			
			memmove(slt_yuv_image, clip_yuv_image, pdefog->width * pdefog->height * 3 / 2);
			
			pthread_mutex_lock(&pdefog->slt_yuv_image_mutex);
			pdefog->slt_yuv_image_queue.push(slt_yuv_image);
			pthread_mutex_unlock(&pdefog->slt_yuv_image_mutex);
			
			if (clip_yuv_image) {
				delete [] clip_yuv_image;
				clip_yuv_image = 0;
			}
			pdefog->clip_yuv_image_queue.pop();
			pthread_mutex_unlock(&pdefog->clip_yuv_image_mutex);
		} else {
#ifdef _WIN32
			Sleep(1);
#elif __linux__
			usleep(1000);
#else
#			error "Unknown compiler"
#endif
		}
	}
}

//---------------------------------------------------------
// Start segmented linear transformation thread.
//---------------------------------------------------------
void defog::start_segment_linear_transf_thread()
{
	pthread_t tid;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	int32_t ret = pthread_create(&tid, &attr, segment_linear_transf_thread, this);
	if (0 != ret) {
		printf("Create segmented linear transformation thread fail!\n");
		exit(-1);
	}
	
	pthread_attr_destroy(&attr);
}

//---------------------------------------------------------
// CLAHE thread.
//---------------------------------------------------------
void *clahe_thread(
	void *param
)	{
	defog *pdefog = (defog *)param;
	printf("start clahe_thread\n");
	while (1) {
		pthread_mutex_lock(&pdefog->slt_yuv_image_mutex);
		int32_t queue_size = pdefog->slt_yuv_image_queue.size();
		pthread_mutex_unlock(&pdefog->slt_yuv_image_mutex);
		if (queue_size > 0) {
#ifdef TEST_DEFOG
			printf("slt_yuv_image_queue %d\n", queue_size);
#endif
			pthread_mutex_lock(&pdefog->slt_yuv_image_mutex);
			uint8_t *slt_yuv_image = pdefog->slt_yuv_image_queue.front();
			
			uint8_t minimum, maximum;
			neon_minmax(slt_yuv_image, pdefog->width * pdefog->height, &minimum, &maximum);
			
			CLAHEq(slt_yuv_image, pdefog->width, pdefog->height, minimum, maximum, 5, 4, 256, pdefog->clip_limit);
			
			uint8_t *clahe_yuv_image = new uint8_t[pdefog->width * pdefog->height * 3 / 2];
			assert(clahe_yuv_image);
			
			memmove(clahe_yuv_image, slt_yuv_image, pdefog->width * pdefog->height * 3 / 2);
			
			pthread_mutex_lock(&pdefog->clahe_yuv_image_mutex);
			pdefog->clahe_yuv_image_queue.push(clahe_yuv_image);
			pthread_mutex_unlock(&pdefog->clahe_yuv_image_mutex);
			
			if (slt_yuv_image) {
				delete [] slt_yuv_image;
				slt_yuv_image = 0;
			}
			pdefog->slt_yuv_image_queue.pop();
			pthread_mutex_unlock(&pdefog->slt_yuv_image_mutex);
		} else {
#ifdef _WIN32
			Sleep(1);
#elif __linux__
			usleep(1000);
#else
#			error "Unknown compiler"
#endif
		}
	}
}

//---------------------------------------------------------
// Start CLAHE thread.
//---------------------------------------------------------
void defog::start_clahe_thread()
{
	pthread_t tid;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	int32_t ret = pthread_create(&tid, &attr, clahe_thread, this);
	if (0 != ret) {
		printf("Create CLAHE thread fail!\n");
		exit(-1);
	}
	
	pthread_attr_destroy(&attr);
}

//---------------------------------------------------------
// Edge enhancement thread.
//---------------------------------------------------------
void *edge_enhance_thread(
	void *param
)	{
	defog *pdefog = (defog *)param;
	printf("start edge_enhance_thread\n");
	while (1) {
		pthread_mutex_lock(&pdefog->clahe_yuv_image_mutex);
		int32_t queue_size = pdefog->clahe_yuv_image_queue.size();
		pthread_mutex_unlock(&pdefog->clahe_yuv_image_mutex);
		if (queue_size > 0) {
#ifdef TEST_DEFOG
			printf("clahe_yuv_image_queue %d\n", queue_size);
#endif
			pthread_mutex_lock(&pdefog->clahe_yuv_image_mutex);
			uint8_t *clahe_yuv_image = pdefog->clahe_yuv_image_queue.front();
			
			uint8_t *edge_yuv_image = new uint8_t[pdefog->width * pdefog->height * 3 / 2];
			assert(edge_yuv_image);
			if (pdefog->enable_edge_enhan) {
				cv::GaussianBlur(cv::Mat(pdefog->height, pdefog->width, CV_8UC1, clahe_yuv_image),
					cv::Mat(pdefog->height, pdefog->width, CV_8UC1, edge_yuv_image), cv::Size(3, 3), 0, 0);
#ifdef __ARM_NEON__ 		
				int16x8_t mask[9] = {vdupq_n_s16(1), vdupq_n_s16(1),  vdupq_n_s16(1),
									 vdupq_n_s16(1), vdupq_n_s16(-8), vdupq_n_s16(1),
									 vdupq_n_s16(1), vdupq_n_s16(1),  vdupq_n_s16(1)};
									 
				filter3x3(edge_yuv_image, pdefog->width, pdefog->height, mask, clahe_yuv_image);
#endif
			}
		
			memmove(edge_yuv_image, clahe_yuv_image, pdefog->width * pdefog->height * 3 / 2);
			pthread_mutex_lock(&pdefog->edge_yuv_image_mutex);
			pdefog->edge_yuv_image_queue.push(edge_yuv_image);
			pthread_mutex_unlock(&pdefog->edge_yuv_image_mutex);
			
			if (clahe_yuv_image) {
				delete [] clahe_yuv_image;
				clahe_yuv_image = 0;
			}
			pdefog->clahe_yuv_image_queue.pop();
			pthread_mutex_unlock(&pdefog->clahe_yuv_image_mutex);
		} else {
#ifdef _WIN32
			Sleep(1);
#elif __linux__
			usleep(1000);
#else
#			error "Unknown compiler"
#endif
		}
	}
}

//---------------------------------------------------------
// Start edge enhancement thread.
//---------------------------------------------------------
void defog::start_edge_enhance_thread()
{
	pthread_t tid;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	int32_t ret = pthread_create(&tid, &attr, edge_enhance_thread, this);
	if (0 != ret) {
		printf("Create edge enhancement thread fail!\n");
		exit(-1);
	}
	
	pthread_attr_destroy(&attr);
}

//---------------------------------------------------------
// Motion adaptive noise reduction.
//---------------------------------------------------------
static void motion_adapt_noise_reduction(
	uint8_t *curr_image,
	uint8_t *prev_image,
	int32_t width,
	int32_t height
)	{
	assert(curr_image);
	assert(prev_image);
#ifdef __ARM_NEON__ 
	const int32_t npixels = width * height;
	int16x8_t abs_diff_limit = vdupq_n_s16(63);
	
	for (int32_t i = 0; i < npixels; i += 8) {
		int16x8_t curr_data_vec = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(curr_image)));
		int16x8_t prev_data_vec = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(prev_image)));
		
		int16x8_t abs_diff_vec = vminq_s16(vabdq_s16(curr_data_vec, prev_data_vec), abs_diff_limit);
		abs_diff_vec = vshrq_n_s16(abs_diff_vec, 2);
	
		int16x8_t alpha;
		for (int32_t j = 0; j < 8; j++) {
			alpha[j] = MTF[abs_diff_vec[j]];
		}

		int16x8_t delta = vshrq_n_s16(vmulq_s16(alpha, vsubq_s16(prev_data_vec, curr_data_vec)), 8);
	
		int16x8_t manr_data_vec = vaddq_s16(curr_data_vec, delta);

		uint8x8_t result_vec = vqmovun_s16(manr_data_vec);
		
		vst1_u8(curr_image, result_vec);
		
		curr_image += 8;
		prev_image += 8;
	}
#endif
}

//---------------------------------------------------------
// Saturation adjustment and motion adaptive noise reduction thread.
//---------------------------------------------------------
void *sat_adjust_manr_thread(
	void *param
)	{
	defog *pdefog = (defog *)param;
	printf("start sat_adjust_manr_thread\n");
	while (1) {
		pthread_mutex_lock(&pdefog->edge_yuv_image_mutex);
		int32_t queue_size = pdefog->edge_yuv_image_queue.size();
		pthread_mutex_unlock(&pdefog->edge_yuv_image_mutex);
		if (queue_size > 0) {
#ifdef TEST_DEFOG
			printf("edge_yuv_image_queue %d\n", queue_size);
#endif
			pthread_mutex_lock(&pdefog->edge_yuv_image_mutex);
			uint8_t *edge_yuv_image = pdefog->edge_yuv_image_queue.front();
			
			uint8_t *sa_manr_yuv_image = new uint8_t[pdefog->width * pdefog->height * 3 / 2];
			assert(sa_manr_yuv_image);
			
			if (pdefog->enable_T_noise_reduce) {
				if (false == pdefog->init_prev_manr_y_image_flag) {
					memmove(pdefog->prev_manr_y_image, edge_yuv_image, pdefog->width * pdefog->height);
					pdefog->init_prev_manr_y_image_flag = true;
				} else {
					motion_adapt_noise_reduction(edge_yuv_image, pdefog->prev_manr_y_image, pdefog->width, pdefog->height);
					memmove(pdefog->prev_manr_y_image, edge_yuv_image, pdefog->width * pdefog->height);
				}
			}
			
			if (pdefog->enable_2D_noise_reduce) {
				median_filter3x3(edge_yuv_image, pdefog->width, pdefog->height, pdefog->mfilt_y_image);
				memmove(edge_yuv_image, pdefog->mfilt_y_image, pdefog->width * pdefog->height);
			}
			
			if (pdefog->enable_uv_adjust) {
				pthread_mutex_lock(&pdefog->raw_y_image_mutex);
				queue_size = pdefog->raw_y_image_queue.size();
				pthread_mutex_unlock(&pdefog->raw_y_image_mutex);
				
				if (queue_size > 0) {
#ifdef TEST_DEFOG
					printf("raw_y_image_queue %d\n", queue_size);
#endif
					pthread_mutex_lock(&pdefog->raw_y_image_mutex);
					uint8_t *raw_y_image = pdefog->raw_y_image_queue.front();
					pdefog->saturation_adjustment(raw_y_image, edge_yuv_image, edge_yuv_image +
						pdefog->width * pdefog->height, pdefog->width, pdefog->height);
					if (raw_y_image) {
						delete [] raw_y_image;
						raw_y_image = 0;
					}
					
					pdefog->raw_y_image_queue.pop();
					pthread_mutex_unlock(&pdefog->raw_y_image_mutex);
				}
			}
			
			memmove(sa_manr_yuv_image, edge_yuv_image, pdefog->width * pdefog->height * 3 / 2);
			pthread_mutex_lock(&pdefog->sa_manr_yuv_image_mutex);
			pdefog->sa_manr_yuv_image_queue.push(sa_manr_yuv_image);
			pthread_mutex_unlock(&pdefog->sa_manr_yuv_image_mutex);
			
			if (edge_yuv_image) {
				delete [] edge_yuv_image;
				edge_yuv_image = 0;
			}
			
			pdefog->edge_yuv_image_queue.pop();
			pthread_mutex_unlock(&pdefog->edge_yuv_image_mutex);
		} else {
#ifdef _WIN32
			Sleep(1);
#elif __linux__
			usleep(1000);
#else
#			error "Unknown compiler"
#endif
		}
	}
}

//---------------------------------------------------------
// Start saturation adjustment and motion adaptive noise reduction thread.
//---------------------------------------------------------
void defog::start_sat_adjust_manr_thread()
{
	pthread_t tid;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	int32_t ret = pthread_create(&tid, &attr, sat_adjust_manr_thread, this);
	if (0 != ret) {
		printf("Create saturation adjust and motion adaptive noise reduction thread fail!\n");
		exit(-1);
	}
	
	pthread_attr_destroy(&attr);
}

//---------------------------------------------------------
// SLT & CLAHE & LOG & SA serial implement.
//---------------------------------------------------------
void defog::process_yuv_ce(
	uint8_t *yuv_image
)	{
	assert(yuv_image);
	
	if (false == enable_module) {
		return;
	}
	
#ifdef EASY_TEST_DEFOG
	clock_t start = clock();
#endif
#ifdef TEST_DEFOG
	clock_t start, finish;
	printf("\n");
#endif

#ifdef TEST_DEFOG	
	start = clock();
#endif
	clip_gray_level(yuv_image, width, height, Y_FLOOR, Y_CEILING);
#ifdef TEST_DEFOG
	finish = clock();
	printf("clip_gray_level %lfms.\n", 1000.0 * (finish - start) / CLOCKS_PER_SEC);
#endif
	if (enable_uv_adjust) {
		memmove(old_y, yuv_image, width * height);
	}

	if (gamma < 0.999f || gamma > 1.001f) {
#ifdef TEST_DEFOG	
		start = clock();
#endif
		seg_linar_transf(yuv_image, width, height, slt[0], slt[1], slt[2], slt[3]);
#ifdef TEST_DEFOG
		finish = clock();
		printf("seg_linar_transf %lfms.\n", 1000.0 * (finish - start) / CLOCKS_PER_SEC);
#endif	
	}
	
#ifdef TEST_DEFOG
	start = clock();
#endif
	uint8_t minv, maxv;
	neon_minmax(yuv_image, width * height, &minv, &maxv);
#ifdef TEST_DEFOG
	finish = clock();
	printf("neon_minmax %lfms.\n", 1000.0 * (finish - start) / CLOCKS_PER_SEC);
	start = clock();
#endif
	
	CLAHEq(yuv_image, width, height, minv, maxv, 5, 4, 256, clip_limit);
#ifdef TEST_DEFOG
	finish = clock();
	printf("CLAHEq %lfms.\n", 1000.0 * (finish - start) / CLOCKS_PER_SEC);
#endif

	if (enable_edge_enhan) {
		cv::Mat blur_y;
#ifdef TEST_DEFOG		
		start = clock();
#endif
		cv::GaussianBlur(cv::Mat(height, width, CV_8UC1, yuv_image), blur_y, cv::Size(3, 3), 0, 0);
#ifdef TEST_DEFOG
		finish = clock();
		printf("GaussianBlur %lfms.\n", 1000.0 * (finish - start) / CLOCKS_PER_SEC);
#endif		
		int16x8_t mask[9] = {vdupq_n_s16(1), vdupq_n_s16(1),  vdupq_n_s16(1),
							 vdupq_n_s16(1), vdupq_n_s16(-8), vdupq_n_s16(1),
							 vdupq_n_s16(1), vdupq_n_s16(1),  vdupq_n_s16(1)};

#ifdef TEST_DEFOG		
		start = clock();
#endif
		filter3x3(blur_y.data, width, height, mask, yuv_image);
#ifdef TEST_DEFOG
		finish = clock();
		printf("filter3x3 %lfms.\n", 1000.0 * (finish - start) / CLOCKS_PER_SEC);
#endif		
	}
	
	if (false == init_prev_manr_y_image_flag) {
		memmove(prev_manr_y_image, yuv_image, width * height);
		init_prev_manr_y_image_flag = true;
	} else {
#ifdef TEST_DEFOG		
		start = clock();
#endif
		motion_adapt_noise_reduction(yuv_image, prev_manr_y_image, width, height);
#ifdef TEST_DEFOG
		finish = clock();
		printf("motion_adapt_noise_reduction %lfms.\n", 1000.0 * (finish - start) / CLOCKS_PER_SEC);
#endif
		memmove(prev_manr_y_image, yuv_image, width * height);
	}
	
	if (enable_uv_adjust) {
		// cv::imwrite("raw.png", cv::Mat(height, width, CV_8UC1, yuv_image));
#ifdef TEST_DEFOG		
		start = clock();
#endif
		saturation_adjustment(old_y, yuv_image, yuv_image + width * height, width, height);
#ifdef TEST_DEFOG
		finish = clock();
		printf("saturation_adjustment %lfms.\n", 1000.0 * (finish - start) / CLOCKS_PER_SEC);
#endif
#ifdef TEST_DEFOG		
		start = clock();
#endif
		median_filter3x3(yuv_image, width, height, mfilt_y_image);	
#ifdef TEST_DEFOG
		finish = clock();
		printf("median_filter3x3 %lfms.\n", 1000.0 * (finish - start) / CLOCKS_PER_SEC);
#endif
		memmove(yuv_image, mfilt_y_image, width * height);
		// cv::imwrite("mfilt.png", cv::Mat(height, width, CV_8UC1, mfilt_y_image));
	}
#ifdef EASY_TEST_DEFOG
	clock_t finish = clock();
	printf("CLAHE all %lfms.\n", 1000.0 * (finish - start) / CLOCKS_PER_SEC);
#endif
}

//---------------------------------------------------------
// SLT & CLAHE & LOG & SA pipeline implement.
//---------------------------------------------------------
void defog::process_yuv_ce_pl(
	uint8_t *yuv_image
)	{
	assert(yuv_image);
	
	if (false == enable_module) {
		return;
	}
	
	uint8_t *clip_yuv_image = new uint8_t[width * height * 3 / 2];
	assert(clip_yuv_image);
	
	memmove(clip_yuv_image, yuv_image, width * height * 3 / 2);
	clip_gray_level(clip_yuv_image, width, height, Y_FLOOR, Y_CEILING);
	
	if (enable_uv_adjust) {
		uint8_t *raw_y_image = new uint8_t[width * height];
		assert(raw_y_image);
		memmove(raw_y_image, clip_yuv_image, width * height);
		pthread_mutex_lock(&raw_y_image_mutex);
		raw_y_image_queue.push(raw_y_image);
		pthread_mutex_unlock(&raw_y_image_mutex);
	}
	
	pthread_mutex_lock(&clip_yuv_image_mutex);
	clip_yuv_image_queue.push(clip_yuv_image);
	pthread_mutex_unlock(&clip_yuv_image_mutex);
	
#ifdef _WIN32
	Sleep(1);
#elif __linux__
	usleep(1000);
#else
#	error "Unknown compiler"
#endif
	
	pthread_mutex_lock(&sa_manr_yuv_image_mutex);
	int32_t queue_size = sa_manr_yuv_image_queue.size();
	pthread_mutex_unlock(&sa_manr_yuv_image_mutex);
	if (queue_size > 0) {
#ifdef TEST_DEFOG
		printf("sa_manr_yuv_image_queue %d\n", queue_size);
#endif
		pthread_mutex_lock(&sa_manr_yuv_image_mutex);
		uint8_t *sa_manr_yuv_image = sa_manr_yuv_image_queue.front();			
		memmove(yuv_image, sa_manr_yuv_image, width * height * 3 / 2);
	
		if (sa_manr_yuv_image) {
			delete [] sa_manr_yuv_image;
			sa_manr_yuv_image = 0;
		}
		sa_manr_yuv_image_queue.pop();
		pthread_mutex_unlock(&sa_manr_yuv_image_mutex);
	}
}

//---------------------------------------------------------
// Calculate minimum channel image.
//---------------------------------------------------------
template<typename T>
void defog::min_channel(
	T *rgb_img[3],
	T *min_chan_img,
	int32_t width,
	int32_t height
)	{
	assert(rgb_img[0]);
	assert(rgb_img[1]);
	assert(rgb_img[2]);
	assert(min_chan_img);
	
	const int32_t npixels = width * height;
	for (int32_t i = 0; i < npixels; i++) {
		T min_val = rgb_img[0][i];
		if (rgb_img[1][i] < min_val) {
			min_val = rgb_img[1][i];
		} 
		if (rgb_img[2][i] < min_val) {
			min_val = rgb_img[2][i];
		} 
		min_chan_img[i] = min_val;
	}
}

//---------------------------------------------------------
// Estimate atmospheric light.
//---------------------------------------------------------
void defog::estimate_atmospheric_light(
	uint8_t *raw_image[3],
	uint8_t *dark_channel_image,
	int32_t width,
	int32_t height,
	uint8_t atmo_light_ceiling,
	uint8_t atmo_light[3]
)	{
	// Histogram for dark channel image.
	int32_t hist[256];
	memset(hist, 0, sizeof(hist));
	
	const int32_t npixels = width * height;
	for (int32_t i = 0; i < npixels; i++) {
		hist[dark_channel_image[i]]++;
	}

	// Pick the top bright_ratio_thresh percent brightest pixels.
	uint8_t brightest_pixel_floor = 0;
	int32_t brightest_pixel_count = 0;
	int32_t brightest_pixel_count_thresh = (int32_t)(bright_ratio_thresh * width * height);
	for (int32_t lev = 255; lev >= 0; lev--) {
		brightest_pixel_count += hist[lev];
		if (brightest_pixel_count > brightest_pixel_count_thresh) {
			brightest_pixel_floor = lev;
			break;
		}
	}

	// Pick atmospheric light in original image.
	atmo_light[0] = 0;
	atmo_light[1] = 0;
	atmo_light[2] = 0;
	const int32_t nchannels = 3;
	int32_t counter[3] = {0, 0, 0};
	float accumulator[3] = {0, 0, 0};
	for (int32_t i = 0; i < npixels; i++) {
		if (dark_channel_image[i] >= brightest_pixel_floor) {
			if (raw_image[0][i] > atmo_light[0]) {
				atmo_light[0] = raw_image[0][i];
				accumulator[0] += raw_image[0][i];
				counter[0]++;
			}
			if (raw_image[1][i] > atmo_light[1]) {
				atmo_light[1] = raw_image[1][i];
				accumulator[1] += raw_image[1][i];
				counter[1]++;
			}
			if (raw_image[2][i] > atmo_light[2]) {
				atmo_light[2] = raw_image[2][i];
				accumulator[2] += raw_image[2][i];
				counter[2]++;
			}
		}
	}
	
	atmo_light[0] = (uint8_t)(accumulator[0] / counter[0]);
	atmo_light[1] = (uint8_t)(accumulator[1] / counter[1]);
	atmo_light[2] = (uint8_t)(accumulator[2] / counter[2]);
	// printf("Atmospheric light: %u, %u, %u.\n", atmo_light[0], atmo_light[1], atmo_light[2]);
	for (int32_t c = 0; c < nchannels; c++) {
		if (atmo_light[c] > atmo_light_ceiling) {
			atmo_light[c] = atmo_light_ceiling;
		}
	}
}

//---------------------------------------------------------
// Normalize minimum filter image thread.
//---------------------------------------------------------
void *normalize_min_filt_image_thread(
	void *param
)	{
	normalize_thread_param_t *normalize_min_filt = (normalize_thread_param_t *)param;
	const int32_t npixels = normalize_min_filt->npixels;
	const uint8_t atmo_light = normalize_min_filt->atmo_light;
	const int32_t nlevels = 256;
	float norm_table[nlevels];
	
	for (int32_t i = 0; i < nlevels; i++) {
		norm_table[i] = (float)i / atmo_light;
	}
	
	for (int32_t i = 0; i < npixels; i++) {
		normalize_min_filt->norm_data[i] = norm_table[normalize_min_filt->data[i]];
	}
	return (void *)(0);
}

//---------------------------------------------------------
// Normalize minimum filter image.
//---------------------------------------------------------
void defog::normalize_min_filt_image(
	uint8_t *raw_image[3],
	int32_t width,
	int32_t height,
	uint8_t atmo_light[3],
	float *processed_image[3]
)	{	
	const int32_t nchannels = 3;
	pthread_t tid[nchannels];
	normalize_thread_param_t normalize_min_filt[nchannels];
	
	for (int32_t c = 0; c < nchannels; c++) {
		normalize_min_filt[c].data = raw_image[c];
		normalize_min_filt[c].npixels = width * height;
		normalize_min_filt[c].atmo_light = atmo_light[c];
		normalize_min_filt[c].norm_data = processed_image[c];
	}
	
	for (int32_t c = 0; c < nchannels; c++) {
		int32_t ret = pthread_create(&tid[c], NULL, normalize_min_filt_image_thread, &normalize_min_filt[c]);
		if (0 != ret) {
			printf("Create normalize minimum filter image thread[%d] fail!\n", c);
			exit(-1);
		}
	}

	for (int32_t c = 0; c < nchannels; c++) {
		pthread_join(tid[c], NULL);
	}
}

//---------------------------------------------------------
// Estimate transmission.
//---------------------------------------------------------
void defog::estimate_transmission(
	float *norm_dark_channel_image,
	int32_t width,
	int32_t height,
	float omega,
	float *transmission_image
)	{	
	const int32_t npixels = width * height;
	for (int32_t i = 0; i < npixels; i++) {
		transmission_image[i] = 1 - omega * norm_dark_channel_image[i];
	}
}

//---------------------------------------------------------
// Refine transmission with guided filter.
//---------------------------------------------------------
void defog::refine_transmission(
	uint8_t *red_image,
	float *transmission_image,
	int32_t width,
	int32_t height,
	int32_t ksize,
	float tran_thresh
)	{
	cv::Mat P(height, width, CV_32FC1, transmission_image);

	cv::Mat G(height, width, CV_32FC1);
	float *pG = (float *)G.data;
	
	const int32_t nlevels = 256;
	float norm_table[nlevels];
	for (int32_t i = 0; i < nlevels; i++) {
		norm_table[i] = i / 255.0f;
	}
	
	const int32_t npixels = width * height;
	#pragma omp parallel for
	for (int32_t i = 0; i < npixels; i++) {
		pG[i] = norm_table[red_image[i]];
	}

	cv::Mat Q = guided_filter(G, P, ksize);
	float *pQ = (float *)Q.data;

	#pragma omp parallel for
	for (int32_t i = 0; i < npixels; i++) {
		if (pQ[i] > 1) {
			pQ[i] = 1;
		} else if (pQ[i] < tran_thresh) {
			pQ[i] = tran_thresh;
		}
		transmission_image[i] = pQ[i];
	}
}

//---------------------------------------------------------
// Make transmission map.
//---------------------------------------------------------
void defog::make_transmission_map(
	float *transmission_image,
	int32_t width,
	int32_t height,
	int32_t *transmission_map
)	{
	const int32_t npixels = width * height;
	
	#pragma omp parallel for
	for (int32_t i = 0; i < npixels; i++) {
		transmission_map[i] = transmission_image[i] * MAX_TRANSMISSION;
	}
}

//---------------------------------------------------------
// Inverse transmission image.
//---------------------------------------------------------
void defog::inverse_transmission(
	float *transmission_image,
	int32_t width,
	int32_t height,
	float *transmission_inv
)	{
#ifdef _WIN32
	const int32_t npixels = width * height;
	
	#pragma omp parallel for
	for (int32_t i = 0; i < npixels; i++) {
		transmission_inv[i] = 1 / transmission_image[i];
	}
#elif __ARM_NEON__
	const int32_t npixels_per_loop = 4;
	const int32_t nloops = width * height / npixels_per_loop;
	for (int32_t i = 0; i < nloops; i++) {
		float32x4_t transm = vld1q_f32(transmission_image);
		float32x4_t inv = vrecpeq_f32(transm);
		float32x4_t restep = vrecpsq_f32(transm, inv);
		inv = vmulq_f32(restep, inv);
		vst1q_f32(transmission_inv, inv);
		transmission_image += npixels_per_loop;
		transmission_inv += npixels_per_loop;
	}
#endif
}

//---------------------------------------------------------
// Recover scene radiance thread.
//---------------------------------------------------------
void *recover_scene_radiance_thread(
	void *param
)	{
	recover_thread_param_t *thread_param = (recover_thread_param_t *)param;
	
	uint8_t *hazzy_data = thread_param->hazzy_data;
	uint8_t *atmo_light = thread_param->atmo_light;
	float *tran_data = thread_param->tran_data;
	const int32_t npixels = thread_param->npixels;
	int32_t (*diff_table)[3] = thread_param->diff_table;
	uint8_t *recover_data = thread_param->recover_data;

	const int32_t nchannels = 3;
	const int32_t bytes_per_pixel = 3;
	for (int32_t i = 0; i < npixels; i++) {
		float trans = tran_data[i];
		for (int32_t c = 0; c < nchannels; c++) {
			int32_t ic = i * bytes_per_pixel + 2 - c;
			float val = diff_table[hazzy_data[ic]][c] * trans + atmo_light[c];
			recover_data[ic] = cv::saturate_cast<uint8_t>(val);
		}
	}
	return (void *)(0);
}

//---------------------------------------------------------
// Recover scene radiance.
//---------------------------------------------------------
void defog::recover_scene_radiance(
	uint8_t *raw_image,
	float *transmission_image,
	int32_t width,
	int32_t height,
	uint8_t atmo_light[3],
	uint8_t *processed_image
)	{
	const int32_t nchannels = 3;
	const int32_t nlevels = 256;
	int32_t diff_table[nlevels][nchannels];
	for (int32_t i = 0; i < nlevels; i++) {
		diff_table[i][0] = i - atmo_light[0];
		diff_table[i][1] = i - atmo_light[1];
		diff_table[i][2] = i - atmo_light[2];
	}

	const int32_t nthreads = nrecover_threads;
	pthread_t tid[nthreads];
	recover_thread_param_t thread_param[nthreads];
	const int32_t sample_height = height / nthreads;
	const int32_t nsample_pixels = width * sample_height;

	for (int32_t t = 0; t < nthreads - 1; t++) {
		thread_param[t].hazzy_data = raw_image + t * nsample_pixels * nchannels;
		thread_param[t].tran_data = transmission_image + t * nsample_pixels;	
		thread_param[t].npixels = nsample_pixels;		
		thread_param[t].atmo_light = atmo_light;	
		thread_param[t].diff_table = diff_table;
		thread_param[t].recover_data = processed_image + t * nsample_pixels * nchannels;
	}
	
	int32_t t = nthreads - 1;
	thread_param[t].hazzy_data = raw_image + t * nsample_pixels * nchannels;
	thread_param[t].tran_data = transmission_image + t * nsample_pixels;	
	thread_param[t].npixels = width * height - t * nsample_pixels;		
	thread_param[t].atmo_light = atmo_light;		
	thread_param[t].diff_table = diff_table;
	thread_param[t].recover_data = processed_image + t * nsample_pixels * nchannels;
	
	for (int32_t t = 0; t < nthreads; t++) {
		int32_t ret = pthread_create(&tid[t], NULL, recover_scene_radiance_thread, &thread_param[t]);
		if (0 != ret) {
			printf("Create recover scene radiance thread[%d] fail!\n", t);
			exit(-1);
		}
	}

	for (int32_t t = 0; t < nthreads; t++) {
		pthread_join(tid[t], NULL);
	}
}

//---------------------------------------------------------
// Recover scene radiance.
//---------------------------------------------------------
#ifdef __ARM_NEON__
void defog::neon_recover_scene_radiance(
	uint8_t * __restrict raw_image,
	float * __restrict transmission_image,
	int32_t width,
	int32_t height,
	uint8_t atmo_light[3],
	uint8_t * __restrict processed_image
)	{
	assert(raw_image);
	assert(transmission_image);
	assert(processed_image);
	
	const int32_t nchannels = 3;
	const int32_t npixels_per_loop = 8;
	const int32_t nloops = width * height / npixels_per_loop;

	float32x4x3_t atmo;
	atmo.val[0] = vdupq_n_f32(atmo_light[2]);
	atmo.val[1] = vdupq_n_f32(atmo_light[1]);
	atmo.val[2] = vdupq_n_f32(atmo_light[0]);
	
	for (int32_t i = 0; i < nloops; i++) {
		// Load eight BGR pixels from DDR.
		uint8x8x3_t bgr = vld3_u8(raw_image);
		
		uint16x8x3_t u16x8x3_bgr;
		u16x8x3_bgr.val[0] = vmovl_u8(bgr.val[0]);
		u16x8x3_bgr.val[1] = vmovl_u8(bgr.val[1]);
		u16x8x3_bgr.val[2] = vmovl_u8(bgr.val[2]);
		
		// Process first four pixels.
		uint16x4x3_t u16x4x3_bgr;
		u16x4x3_bgr.val[0] = vget_low_u16(u16x8x3_bgr.val[0]);
		u16x4x3_bgr.val[1] = vget_low_u16(u16x8x3_bgr.val[1]);
		u16x4x3_bgr.val[2] = vget_low_u16(u16x8x3_bgr.val[2]);
		
		uint32x4x3_t u32x4x3_bgr;
		u32x4x3_bgr.val[0] = vmovl_u16(u16x4x3_bgr.val[0]);
		u32x4x3_bgr.val[1] = vmovl_u16(u16x4x3_bgr.val[1]);
		u32x4x3_bgr.val[2] = vmovl_u16(u16x4x3_bgr.val[2]);
		
		float32x4x3_t f32x4x3_bgr;
		f32x4x3_bgr.val[0] = vcvtq_f32_u32(u32x4x3_bgr.val[0]);
		f32x4x3_bgr.val[1] = vcvtq_f32_u32(u32x4x3_bgr.val[1]);
		f32x4x3_bgr.val[2] = vcvtq_f32_u32(u32x4x3_bgr.val[2]);
		
		float32x4x3_t diff;
		diff.val[0] = vsubq_f32(f32x4x3_bgr.val[0], atmo.val[0]);
		diff.val[1] = vsubq_f32(f32x4x3_bgr.val[1], atmo.val[1]);
		diff.val[2] = vsubq_f32(f32x4x3_bgr.val[2], atmo.val[2]);
		
		// Load first four transmission pixles from DDR.
		float32x4_t transm_inv = vld1q_f32(transmission_image);
		
		float32x4x3_t product;
		product.val[0] = vmulq_f32(diff.val[0], transm_inv);
		product.val[1] = vmulq_f32(diff.val[1], transm_inv);
		product.val[2] = vmulq_f32(diff.val[2], transm_inv);
		
		float32x4x3_t result;
		result.val[0] = vaddq_f32(product.val[0], atmo.val[0]);
		result.val[1] = vaddq_f32(product.val[1], atmo.val[1]);
		result.val[2] = vaddq_f32(product.val[2], atmo.val[2]);
		
		int32x4x3_t s32x4x3_rev_bgr;
		s32x4x3_rev_bgr.val[0] = vcvtq_s32_f32(result.val[0]);
		s32x4x3_rev_bgr.val[1] = vcvtq_s32_f32(result.val[1]);
		s32x4x3_rev_bgr.val[2] = vcvtq_s32_f32(result.val[2]);
		
		int16x4x3_t s16x4x3_low_rev_bgr;
		s16x4x3_low_rev_bgr.val[0] = vmovn_s32(s32x4x3_rev_bgr.val[0]);
		s16x4x3_low_rev_bgr.val[1] = vmovn_s32(s32x4x3_rev_bgr.val[1]);
		s16x4x3_low_rev_bgr.val[2] = vmovn_s32(s32x4x3_rev_bgr.val[2]);
				
		// Process second four pixels.	
		u16x4x3_bgr.val[0] = vget_high_u16(u16x8x3_bgr.val[0]);
		u16x4x3_bgr.val[1] = vget_high_u16(u16x8x3_bgr.val[1]);
		u16x4x3_bgr.val[2] = vget_high_u16(u16x8x3_bgr.val[2]);
		
		u32x4x3_bgr.val[0] = vmovl_u16(u16x4x3_bgr.val[0]);
		u32x4x3_bgr.val[1] = vmovl_u16(u16x4x3_bgr.val[1]);
		u32x4x3_bgr.val[2] = vmovl_u16(u16x4x3_bgr.val[2]);
		
		f32x4x3_bgr.val[0] = vcvtq_f32_u32(u32x4x3_bgr.val[0]);
		f32x4x3_bgr.val[1] = vcvtq_f32_u32(u32x4x3_bgr.val[1]);
		f32x4x3_bgr.val[2] = vcvtq_f32_u32(u32x4x3_bgr.val[2]);
		
		diff.val[0] = vsubq_f32(f32x4x3_bgr.val[0], atmo.val[0]);
		diff.val[1] = vsubq_f32(f32x4x3_bgr.val[1], atmo.val[1]);
		diff.val[2] = vsubq_f32(f32x4x3_bgr.val[2], atmo.val[2]);
		
		// Load second four transmission pixles from DDR.
		transmission_image += 4;
		transm_inv = vld1q_f32(transmission_image);
		
		product.val[0] = vmulq_f32(diff.val[0], transm_inv);
		product.val[1] = vmulq_f32(diff.val[1], transm_inv);
		product.val[2] = vmulq_f32(diff.val[2], transm_inv);
		
		result.val[0] = vaddq_f32(product.val[0], atmo.val[0]);
		result.val[1] = vaddq_f32(product.val[1], atmo.val[1]);
		result.val[2] = vaddq_f32(product.val[2], atmo.val[2]);	
		
		s32x4x3_rev_bgr.val[0] = vcvtq_s32_f32(result.val[0]);
		s32x4x3_rev_bgr.val[1] = vcvtq_s32_f32(result.val[1]);
		s32x4x3_rev_bgr.val[2] = vcvtq_s32_f32(result.val[2]);
		
		int16x4x3_t s16x4x3_hig_rev_bgr;
		s16x4x3_hig_rev_bgr.val[0] = vmovn_s32(s32x4x3_rev_bgr.val[0]);
		s16x4x3_hig_rev_bgr.val[1] = vmovn_s32(s32x4x3_rev_bgr.val[1]);
		s16x4x3_hig_rev_bgr.val[2] = vmovn_s32(s32x4x3_rev_bgr.val[2]);
		
		// Combine result.
		int16x8x3_t s16x8x3_rev_bgr;
		s16x8x3_rev_bgr.val[0] = vcombine_s16(s16x4x3_low_rev_bgr.val[0], s16x4x3_hig_rev_bgr.val[0]);
		s16x8x3_rev_bgr.val[1] = vcombine_s16(s16x4x3_low_rev_bgr.val[1], s16x4x3_hig_rev_bgr.val[1]);
		s16x8x3_rev_bgr.val[2] = vcombine_s16(s16x4x3_low_rev_bgr.val[2], s16x4x3_hig_rev_bgr.val[2]);
		
		uint8x8x3_t recover_bgr;
		recover_bgr.val[0] = vqmovun_s16(s16x8x3_rev_bgr.val[0]);
		recover_bgr.val[1] = vqmovun_s16(s16x8x3_rev_bgr.val[1]);
		recover_bgr.val[2] = vqmovun_s16(s16x8x3_rev_bgr.val[2]);
		
		// Write result to DDR.
		vst3_u8(processed_image, recover_bgr);
		
		raw_image += nchannels * npixels_per_loop;
		transmission_image += 4;
		processed_image += nchannels * npixels_per_loop;
	}
}
#endif

//---------------------------------------------------------
// Calculate auto level threshold.
//---------------------------------------------------------
void defog::auto_level_thresh(
	uint8_t *image,
	int32_t width,
	int32_t height,
	float lowcut_thresh,
	float highcut_thresh,
	uint8_t auto_level_floor[3],
	uint8_t auto_level_ceiling[3]
)	{
	const int32_t nlevels = 256;
	const int32_t nchannels = 3;
	int32_t hist[nchannels][nlevels];
	memset(hist, 0, sizeof(hist));
	
	const int32_t bytes_per_pixel = 3;
	const int32_t npixels = width * height;
	
	for (int32_t i = 0; i < npixels; i += 2) {
		int32_t i0 = i * bytes_per_pixel;
		hist[0][image[i0]]++;
		hist[1][image[i0 + 1]]++;
		hist[2][image[i0 + 2]]++;
	}
	
	const int32_t lowcut_pixels = lowcut_thresh * width * height;
	const int32_t highcut_pixels = highcut_thresh * width * height;
	for (int32_t c = 0; c < nchannels; c++) {
		int32_t counter = 0;
		for (int32_t lev = 0; lev <= 255; lev++) {
			counter += hist[c][lev];
			if (counter > lowcut_pixels) {
				auto_level_floor[c] = lev;
				break;
			}
		}
		
		counter = 0;
		for (int32_t lev = 255; lev >= 0; lev--) {
			counter += hist[c][lev];
			if (counter > highcut_pixels) {
				auto_level_ceiling[c] = lev;
				break;
			}
		}
	}
}

//---------------------------------------------------------
// Make stretch table.
//---------------------------------------------------------
void defog::make_stretch_table(
	uint8_t auto_level_floor[3],
	uint8_t auto_level_ceiling[3],
	uint8_t **stretch_table
)	{
	float scale[3];
	scale[0] = 255.0f / (auto_level_ceiling[0] - auto_level_floor[0]);
	scale[1] = 255.0f / (auto_level_ceiling[1] - auto_level_floor[1]);
	scale[2] = 255.0f / (auto_level_ceiling[2] - auto_level_floor[2]);
	const int32_t nlevels = 256;
	const int32_t nchannels = 3;

	#pragma omp parallel for
	for (int32_t c = 0; c < nchannels; c++) {
		for (int32_t i = 0; i < auto_level_floor[c]; i++) {
			stretch_table[c][i] = 0;
		}
		
		for (int32_t i = auto_level_floor[c]; i <= auto_level_ceiling[c]; i++) {
			stretch_table[c][i] = (uint8_t)(scale[c] * (i - auto_level_floor[c]) + 0.5);
		}
		
		for (int32_t i = auto_level_ceiling[c] + 1; i < nlevels; i++) {
			stretch_table[c][i] = 255;
		}
	}
}

//---------------------------------------------------------
// Auto levels thread.
//---------------------------------------------------------
void *auto_levels_thread(
	void *param
)	{
	auto_level_thread_param_t *thread_param = (auto_level_thread_param_t *)param;
	
	uint8_t *recover_data = thread_param->recover_data;
	int32_t npixels = thread_param->npixels;
	uint8_t **stretch_table = thread_param->stretch_table;
	uint8_t *stretch_data = thread_param->stretch_data;
	
	const int32_t bytes_per_pixel = 3;
	for (int32_t i = 0; i < npixels; i++) {
		int32_t i0 = i * bytes_per_pixel;
		stretch_data[i0] = stretch_table[0][recover_data[i0]];
		stretch_data[i0 + 1] = stretch_table[1][recover_data[i0 + 1]];
		stretch_data[i0 + 2] = stretch_table[2][recover_data[i0 + 2]];
	}
	return (void *)(0);
}

//---------------------------------------------------------
// Auto levels.
//---------------------------------------------------------
void defog::auto_levels(
	uint8_t *raw_image,
	int32_t width,
	int32_t height,
	uint8_t **stretch_table,
	uint8_t *processed_image
)	{
	const int32_t nchannels = 3;
	int32_t nthreads = nstretch_threads;
	pthread_t tid[nthreads];
	auto_level_thread_param_t thread_param[nthreads];
	const int32_t sample_height = height / nthreads;
	const int32_t nsample_pixels = width * sample_height;
	
	for (int32_t t = 0; t < nthreads - 1; t++) {
		thread_param[t].recover_data = raw_image + t * nsample_pixels * nchannels;	
		thread_param[t].npixels = nsample_pixels;		
		thread_param[t].stretch_table = stretch_table;
		thread_param[t].stretch_data = processed_image + t * nsample_pixels * nchannels;	
	}
	
	int32_t t = nthreads - 1;
	thread_param[t].recover_data = raw_image + t * nsample_pixels * nchannels;	
	thread_param[t].npixels = width * height - t * nsample_pixels;		
	thread_param[t].stretch_table = stretch_table;
	thread_param[t].stretch_data = processed_image + t * nsample_pixels * nchannels;
	
	for (int32_t t = 0; t < nthreads; t++) {
		int32_t ret = pthread_create(&tid[t], NULL, auto_levels_thread, &thread_param[t]);
		if (0 != ret) {
			printf("Create auto level thread[%d] fail!\n", t);
			exit(-1);
		}
	}

	for (int32_t t = 0; t < nthreads; t++) {
		pthread_join(tid[t], NULL);
	}
}

//---------------------------------------------------------
// Gamma correction.
//---------------------------------------------------------
void defog::gamma_correct(
	uint8_t *raw_image,
	int32_t width,
	int32_t height,
	float scale,
	float power,
	uint8_t *processed_image
)	{
	assert(raw_image);
	assert(processed_image);
#ifdef __ARM_NEON__ 	
	const int32_t max_level = 255;
	const int32_t npixels = width * height;
	const int32_t npixels_per_loop = 8;
	const float constant = scale * pow(max_level, 1 - power);
	float32x4_t f32x4_power = vdupq_n_f32(power);
	
	float table[max_level + 1];
	for (int32_t i = 0; i <= max_level; i++) {
		table[i] = pow(i, power);
	}
	
	for (int32_t i = 0; i < npixels; i += npixels_per_loop) {
		uint8x8_t u8x8_data = vld1_u8(raw_image);
	
		float32x4x2_t f32x4x2_result;
		f32x4x2_result.val[0] = vsetq_lane_f32(table[vget_lane_u8(u8x8_data, 0)], f32x4x2_result.val[0], 0);
		f32x4x2_result.val[0] = vsetq_lane_f32(table[vget_lane_u8(u8x8_data, 1)], f32x4x2_result.val[0], 1);
		f32x4x2_result.val[0] = vsetq_lane_f32(table[vget_lane_u8(u8x8_data, 2)], f32x4x2_result.val[0], 2);
		f32x4x2_result.val[0] = vsetq_lane_f32(table[vget_lane_u8(u8x8_data, 3)], f32x4x2_result.val[0], 3);
		f32x4x2_result.val[1] = vsetq_lane_f32(table[vget_lane_u8(u8x8_data, 4)], f32x4x2_result.val[1], 0);
		f32x4x2_result.val[1] = vsetq_lane_f32(table[vget_lane_u8(u8x8_data, 5)], f32x4x2_result.val[1], 1);
		f32x4x2_result.val[1] = vsetq_lane_f32(table[vget_lane_u8(u8x8_data, 6)], f32x4x2_result.val[1], 2);
		f32x4x2_result.val[1] = vsetq_lane_f32(table[vget_lane_u8(u8x8_data, 7)], f32x4x2_result.val[1], 3);
		
		f32x4x2_result.val[0] = vmulq_n_f32(f32x4x2_result.val[0], constant);
		f32x4x2_result.val[1] = vmulq_n_f32(f32x4x2_result.val[1], constant);
		
		uint32x4x2_t u32x4x2_result;
		u32x4x2_result.val[0] = vcvtq_u32_f32(f32x4x2_result.val[0]);
		u32x4x2_result.val[1] = vcvtq_u32_f32(f32x4x2_result.val[1]);
		
		uint16x4x2_t u16x4x2_result;
		u16x4x2_result.val[0] = vmovn_u32(u32x4x2_result.val[0]);
		u16x4x2_result.val[1] = vmovn_u32(u32x4x2_result.val[1]);
		
		uint16x8_t u16x8_result = vcombine_u16(u16x4x2_result.val[0], u16x4x2_result.val[1]);
		
		uint8x8_t u8x8_result = vqmovn_u16(u16x8_result);
		
		vst1_u8(processed_image, u8x8_result);
		
		raw_image += npixels_per_loop;
		processed_image += npixels_per_loop;
	}
#endif
}

//---------------------------------------------------------
// Get minimum filter image.
//---------------------------------------------------------
void defog::get_dark_channel_image(
	uint8_t *dark_chan_img_
)	{
	assert(dark_chan_img_);
	dark_chan_img = min_chan_img;
	memmove(dark_chan_img_, dark_chan_img, ds_width * ds_height);
}

//---------------------------------------------------------
// Get transmission image.
//---------------------------------------------------------
void defog::get_transmission_image(
	uint8_t *transm_img_
)	{
	assert(transm_img_);
	touint8<float>(transm_img, transm_img_, ds_width * ds_height);
}

//---------------------------------------------------------
// Get defog image.
//---------------------------------------------------------
void defog::get_defog_image(
	uint8_t *defog_image
)	{
	assert(defog_image);
	const int32_t bytes_per_pixel = 3;
	memmove(defog_image, recover_img, width * height * bytes_per_pixel);
}

//---------------------------------------------------------
// Get scale factor.
//---------------------------------------------------------
float defog::get_scale()
{
	return downsample;
}

//---------------------------------------------------------
// YUV420 to BGR24 pipeline thread.
//---------------------------------------------------------
void *yuv2bgr_pipeline_thread(
	void *param
)	{
	defog *pdefog = (defog *)param;
	printf("start yuv2bgr_pipeline_thread\n");
	while (1) {
		pthread_mutex_lock(&pdefog->in_yuv_image_mutex);
		int32_t queue_size = pdefog->in_yuv_image_queue.size();
		pthread_mutex_unlock(&pdefog->in_yuv_image_mutex);
		if (queue_size > 0) {
#ifdef TEST_DEFOG
			printf("in_yuv_image_queue %d\n", queue_size);
#endif
			pthread_mutex_lock(&pdefog->in_yuv_image_mutex);
			
			while (pdefog->in_yuv_image_queue.size() > MAX_CACHE_FRAMES) {
				uint8_t *oldest = pdefog->in_yuv_image_queue.front();
				if (oldest) {
					delete [] oldest;
					oldest = 0;
				}
				pdefog->in_yuv_image_queue.pop();
			}
			
			uint8_t *in_yuv_image = pdefog->in_yuv_image_queue.front();
			uint8_t *in_bgr_image = new uint8_t[pdefog->width * pdefog->height * 3];
			assert(in_bgr_image);
#ifdef TEST_DEFOG
			clock_t start = clock();
#endif
			pdefog->yuv2bgr(in_yuv_image, pdefog->width, pdefog->height, in_bgr_image);
#ifdef TEST_DEFOG
			clock_t finish = clock();
			printf("yuv2bgr %.0lfms.\n", 1000.0 * (finish - start) / CLOCKS_PER_SEC);
#endif
			pthread_mutex_lock(&pdefog->in_bgr_image_mutex);
			pdefog->in_bgr_image_queue.push(in_bgr_image);
			pthread_mutex_unlock(&pdefog->in_bgr_image_mutex);
			
			if (in_yuv_image) {
				delete [] in_yuv_image;
				in_yuv_image = 0;
			}
			pdefog->in_yuv_image_queue.pop();
			pthread_mutex_unlock(&pdefog->in_yuv_image_mutex);
		} else {
#ifdef _WIN32
			Sleep(1);
#elif __linux__
			usleep(1000);
#else
#			error "Unknown compiler"
#endif
		}
	}
}

//---------------------------------------------------------
// Start YUV420 to BGR24 pipeline.
//---------------------------------------------------------
void defog::start_yuv2bgr_pipeline()
{
	pthread_t tid;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	int32_t ret = pthread_create(&tid, &attr, yuv2bgr_pipeline_thread, this);
	if (0 != ret) {
		printf("Create stretch pipeline thread fail!\n");
		exit(-1);
	}
	
	pthread_attr_destroy(&attr);
}

//---------------------------------------------------------
// Defog pipeline thread.
//---------------------------------------------------------
void *defog_pipeline_thread(
	void *param
)	{
	defog *pdefog = (defog *)param;
	printf("start defog_pipeline_thread\n");
	while (1) {
		pthread_mutex_lock(&pdefog->in_bgr_image_mutex);
		int32_t queue_size = pdefog->in_bgr_image_queue.size();
		pthread_mutex_unlock(&pdefog->in_bgr_image_mutex);
		if (queue_size > 0) {
#ifdef TEST_DEFOG
			printf("in_bgr_image_queue %d\n", queue_size);
#endif
			pthread_mutex_lock(&pdefog->in_bgr_image_mutex);
			
			while (pdefog->in_bgr_image_queue.size() > MAX_CACHE_FRAMES) {
				uint8_t *oldest = pdefog->in_bgr_image_queue.front();
				if (oldest) {
					delete [] oldest;
					oldest = 0;
				}
				pdefog->in_bgr_image_queue.pop();
			}
			
			uint8_t *in_bgr_image = pdefog->in_bgr_image_queue.front();
			uint8_t *out_bgr_image = new uint8_t[pdefog->width * pdefog->height * 3];
			assert(out_bgr_image);
#ifdef TEST_DEFOG
			clock_t start = clock();
#endif			
			pdefog->process_rgb_dp(in_bgr_image);
#ifdef TEST_DEFOG
			clock_t finish = clock();
			printf("process %.0lfms.\n", 1000.0 * (finish - start) / CLOCKS_PER_SEC);
#endif
			memmove(out_bgr_image, pdefog->stretch_img, pdefog->width * pdefog->height * 3);
			pthread_mutex_lock(&pdefog->out_bgr_image_mutex);
			pdefog->out_bgr_image_queue.push(out_bgr_image);
			pthread_mutex_unlock(&pdefog->out_bgr_image_mutex);
			
			if (in_bgr_image) {
				delete [] in_bgr_image;
				in_bgr_image = 0;
			}
			pdefog->in_bgr_image_queue.pop();
			pthread_mutex_unlock(&pdefog->in_bgr_image_mutex);
		} else {
#ifdef _WIN32
			Sleep(1);
#elif __linux__
			usleep(1000);
#else
#			error "Unknown compiler"
#endif
		}
	}
}

//---------------------------------------------------------
// Start defog pipeline.
//---------------------------------------------------------
void defog::start_defog_pipeline()
{
	pthread_t tid;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	int32_t ret = pthread_create(&tid, &attr, defog_pipeline_thread, this);
	if (0 != ret) {
		printf("Create stretch pipeline thread fail!\n");
		exit(-1);
	}
	
	pthread_attr_destroy(&attr);
}

//---------------------------------------------------------
// BGR24 to YUV420 pipeline thread.
//---------------------------------------------------------
void *bgr2yuv_pipeline_thread(
	void *param
)	{
	defog *pdefog = (defog *)param;
	printf("start bgr2yuv_pipeline_thread\n");
	while (1) {
#if 1
		pthread_mutex_lock(&pdefog->out_bgr_image_mutex);
		int32_t queue_size = pdefog->out_bgr_image_queue.size();
		pthread_mutex_unlock(&pdefog->out_bgr_image_mutex);
		if (queue_size > 0) {
#ifdef TEST_DEFOG
			printf("out_bgr_image_queue %d\n", queue_size);
#endif
			pthread_mutex_lock(&pdefog->out_bgr_image_mutex);
			
			while (pdefog->out_bgr_image_queue.size() > MAX_CACHE_FRAMES) {
				uint8_t *oldest = pdefog->out_bgr_image_queue.front();
				if (oldest) {
					delete [] oldest;
					oldest = 0;
				}
				pdefog->out_bgr_image_queue.pop();
			}
			
			uint8_t *out_bgr_image = pdefog->out_bgr_image_queue.front();
			uint8_t *out_yuv_image = new uint8_t[(pdefog->width * pdefog->height * 3) >> 1];
			assert(out_yuv_image);
#ifdef TEST_DEFOG
			clock_t start = clock();
#endif				
			pdefog->bgr2yuv(out_bgr_image, pdefog->width, pdefog->height, out_yuv_image);
#ifdef TEST_DEFOG
			clock_t finish = clock();
			printf("bgr2yuv %.0lfms.\n", 1000.0 * (finish - start) / CLOCKS_PER_SEC);
#endif
			pthread_mutex_lock(&pdefog->out_yuv_image_mutex);
			pdefog->out_yuv_image_queue.push(out_yuv_image);
			pthread_mutex_unlock(&pdefog->out_yuv_image_mutex);
			
			if (out_bgr_image) {
				delete [] out_bgr_image;
				out_bgr_image = 0;
			}
			pdefog->out_bgr_image_queue.pop();
			pthread_mutex_unlock(&pdefog->out_bgr_image_mutex);
		} else {
#ifdef _WIN32
			Sleep(1);
#elif __linux__
			usleep(1000);
#else
#			error "Unknown compiler"
#endif
		}
#else
		pthread_mutex_lock(&pdefog->in_bgr_image_mutex);
		int32_t queue_size = pdefog->in_bgr_image_queue.size();
		pthread_mutex_unlock(&pdefog->in_bgr_image_mutex);
		if (queue_size > 0) {
#ifdef TEST_DEFOG
			printf("in_bgr_image_queue %d\n", queue_size);
#endif
			pthread_mutex_lock(&pdefog->in_bgr_image_mutex);
			uint8_t *out_bgr_image = pdefog->in_bgr_image_queue.front();
			uint8_t *out_yuv_image = new uint8_t[(pdefog->width * pdefog->height * 3) >> 1];
			assert(out_yuv_image);
#ifdef TEST_DEFOG
			clock_t start = clock();
#endif				
			pdefog->bgr2yuv(out_bgr_image, pdefog->width, pdefog->height, out_yuv_image);
#ifdef TEST_DEFOG
			clock_t finish = clock();
			printf("bgr2yuv %.0lfms.\n", 1000.0 * (finish - start) / CLOCKS_PER_SEC);
#endif
			pthread_mutex_lock(&pdefog->out_yuv_image_mutex);
			pdefog->out_yuv_image_queue.push(out_yuv_image);
			pthread_mutex_unlock(&pdefog->out_yuv_image_mutex);
			
			if (out_bgr_image) {
				delete [] out_bgr_image;
				out_bgr_image = 0;
			}
			pdefog->in_bgr_image_queue.pop();
			pthread_mutex_unlock(&pdefog->in_bgr_image_mutex);
		} else {
#ifdef _WIN32
			Sleep(1);
#elif __linux__
			usleep(1000);
#else
#			error "Unknown compiler"
#endif
		}
#endif
	}
}

//---------------------------------------------------------
// Start BGR24 to YUV420 pipeline.
//---------------------------------------------------------
void defog::start_bgr2yuv_pipeline()
{
	pthread_t tid;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	int32_t ret = pthread_create(&tid, &attr, bgr2yuv_pipeline_thread, this);
	if (0 != ret) {
		printf("Create stretch pipeline thread fail!\n");
		exit(-1);
	}
	
	pthread_attr_destroy(&attr);
}

//---------------------------------------------------------
// Push YUV image in queue.
//---------------------------------------------------------
void defog::process_yuv_dp_pl(
	uint8_t *yuv_image
)	{
	assert(yuv_image);
	
	if (false == enable_module) {
		return;
	}
	
	uint8_t *in_yuv_image = new uint8_t[(width * height * 3) >> 1];
	assert(in_yuv_image);
	
	memmove(in_yuv_image, yuv_image, (width * height * 3) >> 1);
	pthread_mutex_lock(&in_yuv_image_mutex);
	in_yuv_image_queue.push(in_yuv_image);
	pthread_mutex_unlock(&in_yuv_image_mutex);
	
	pthread_mutex_lock(&out_yuv_image_mutex);
	int32_t queue_size = out_yuv_image_queue.size();
	pthread_mutex_unlock(&out_yuv_image_mutex);
	
	if (queue_size > 0) {
#ifdef TEST_DEFOG
		printf("out_yuv_image_queue %d\n", queue_size);
#endif
		pthread_mutex_lock(&out_yuv_image_mutex);
		uint8_t *out_yuv_image = out_yuv_image_queue.front();
		memmove(yuv_image, out_yuv_image, (width * height * 3) >> 1);
		
		if (out_yuv_image) {
			delete [] out_yuv_image;
			out_yuv_image = 0;
		}
		out_yuv_image_queue.pop();
		pthread_mutex_unlock(&out_yuv_image_mutex);
	}
}