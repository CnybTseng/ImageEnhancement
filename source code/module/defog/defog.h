#ifndef _DEFOG_H_
#define _DEFOG_H_

#include <cstdint>
#include <queue>
#include "pthread.h"
#include "opencv2/opencv.hpp"

class defog
{
public:
	/**
	 * Default constructor function.
	 */
	defog();
	/**
	 * Constructor function.
	 * @param[in] hazzy_img_ Input RGB image.
	 * @param[in] width_ Image width.
	 * @param[in] height_ Image height.
	 */
	defog(
		uint8_t *hazzy_img_,
		int32_t width_,
		int32_t height_
	);
	/**
	 * Destructor function.
	 */
	~defog();
	/**
	 * Init defog instance.
	 * @param[in] hazzy_img_ Input RGB image.
	 * @param[in] width_ Image width.
	 * @param[in] height_ Image height.
	 * @return void.
	 */
	void init(
		uint8_t *hazzy_img_,
		int32_t width_,
		int32_t height_
	);
	/**
	 * Process interface of class defog.
	 * @return void.
	 */
	void process_rgb_dp(
		uint8_t *raw_image
	);
	/**
	 * YUV image process interface.
	 * @param[in] yuv_image YUV420 image.
	 * @return void.
	 */
	void process_yuv_dp(
		uint8_t *yuv_image
	);
	/**
	 * Output dark channel image.
	 * @param[out] dark_chan_img_ Dark channel image.
	 * @return void.
	 */
	void get_dark_channel_image(
		uint8_t *dark_chan_img_
	);
	/**
	 * Output transmission image.
	 * @param[out] tran_img_ Transmission image.
	 * @return void.
	 */
	void get_transmission_image(
		uint8_t *tran_img_
	);
	/**
	 * Output defog image.
	 * @param[out] rec_img_ Defog image.
	 * @return void.
	 */
	void get_defog_image(
		uint8_t *defog_image
	);
	/**
	 * Get scale factor.
	 * @return Scale factor.
	 */
	float get_scale();
	/**
	 * Process image with dark prior defog.
	 * @param[in] yuv_image YUV420 image.
	 * @return void.
	 */
	void process_yuv_dp_pl(
		uint8_t *yuv_image
	);
	/**
	 * SLT & CLAHE & LOG & SA.
	 * @param[in] yuv_image YUV420 image.
	 * @return void.
	 */
	void process_yuv_ce(
		uint8_t *yuv_image
	);
	/**
	 * SLT & CLAHE & LOG & SA.
	 * @param[in] yuv_image YUV420 image.
	 * @return void.
	 */
	void process_yuv_ce_pl(
		uint8_t *yuv_image
	);
private:
	/**
	 * Load parameters from configuration.
	 * @return void.
	 */
	void load_parameters();
	/**
	 * YUV420 to BGR24.
	 * @param[in] yuv_image YUV420 image.
	 * @param[in] width Image width.
	 * @param[in] height Image height.
	 * @param[out] bgr_image BGR24 image.
	 * @return void.
	 */
	void yuv2bgr(
		uint8_t *yuv_image,
		int32_t width,
		int32_t height,
		uint8_t *bgr_image
	);
	/**
	 * BGR24 to YUV420.
	 * @param[in] bgr_image BGR24 image.
	 * @param[in] width Image width.
	 * @param[in] height Image height.
	 * @param[out] yuv_image YUV420 image.
	 * @return void.
	 */
	void bgr2yuv(
		uint8_t *bgr_image,
		int32_t width,
		int32_t height,
		uint8_t *yuv_image
	);
	/**
	 * Minimum filter.
	 * @return void.
	 */
	void min_filter(
		uint8_t *raw_image[3],
		int32_t width,
		int32_t height,
		int32_t ksize,
		uint8_t *processed_image[3]
	);
	/**
	 * Calculate minimum channel image.
	 * @param[in] chan1_img Channel 1 image.
	 * @param[in] chan2_img Channel 2 image.
	 * @param[in] chan3_img Channel 3 image.
	 * @param[out] min_chan_img Minimum channel image.
	 * @param[in] width Image width.
	 * @param[in] height Image height.
	 * @return void.
	 */
	template<typename T>
	void min_channel(
		T *rgb_img[3],
		T *min_chan_img,
		int32_t width,
		int32_t height
	);
	/**
	 * Estimate atmospheric light.
	 * @return void.
	 */
	void estimate_atmospheric_light(
		uint8_t *raw_image[3],
		uint8_t *dark_channel_image,
		int32_t width,
		int32_t height,
		uint8_t atmo_light_ceiling,
		uint8_t atmo_light[3]
	);
	/**
	 * Normalize minimum filter image.
	 * @return void.
	 */
	void normalize_min_filt_image(
		uint8_t *raw_image[3],
		int32_t width,
		int32_t height,
		uint8_t atmo_light[3],
		float *processed_image[3]
	);
	/**
	 * Estimate transmission.
	 * @return void.
	 */
	void estimate_transmission(
		float *norm_dark_channel_image,
		int32_t width,
		int32_t height,
		float omega,
		float *transmission_image
	);
	/**
	 * Guided filter.
	 * @param[in] G_ Guided image.
	 * @param[in] P_ Input image.
	 * @param[in] ksize Kernel size.
	 * @return Filtered image.
	 */
	cv::Mat guided_filter(
		cv::Mat G,
		cv::Mat P,
		int ksize
	);
	/**
	 * Refine transmission with guided filter.
	 * @return void.
	 */
	void refine_transmission(
		uint8_t *red_image,
		float *transmission_image,
		int32_t width,
		int32_t height,
		int32_t ksize,
		float tran_thresh
	);
	/**
	 * Make transmission map.
	 * @param[in] transmission_image Transmission image.
	 * @param[in] width Image width.
	 * @param[in] height Image height.
	 * @param[out] transmission_map Transmission map.
	 * @return void.
	 */
	void make_transmission_map(
		float *transmission_image,
		int32_t width,
		int32_t height,
		int32_t *transmission_map
	);
	/**
	 * Inverse transmission.
	 * @param[in] transmission_image Transmission image.
	 * @param[in] width Image width.
	 * @param[in] height Image height.
	 * @param[out] transmission_inv Transmission inverse.
	 * @return void.
	 */
	void inverse_transmission(
		float *transmission_image,
		int32_t width,
		int32_t height,
		float *transmission_inv
	);
	/**
	 * Calculate auto level threshold.
	 * @return void.
	 */
	void auto_level_thresh(
		uint8_t *image,
		int32_t width,
		int32_t height,
		float lowcut_thresh,
		float highcut_thresh,
		uint8_t auto_level_floor[3],
		uint8_t auto_level_ceiling[3]
	);
	/**
	 * Make stretch table.
	 * @param[in] auto_level_floor Auto level floor.
	 * @param[in] auto_level_ceiling Auto level ceiling.
	 * @param[out] stretch_table Stretch table.
	 */
	void make_stretch_table(
		uint8_t auto_level_floor[3],
		uint8_t auto_level_ceiling[3],
		uint8_t **stretch_table
	);
	/**
	 * Recover scene radiance image.
	 * @return void.
	 */
	void recover_scene_radiance(
		uint8_t *raw_image,
		float *transmission_image,
		int32_t width,
		int32_t height,
		uint8_t atmo_light[3],
		uint8_t *processed_image
	);
#ifdef __ARM_NEON__
	void neon_recover_scene_radiance(
		uint8_t *raw_image,
		float *transmission_image,
		int32_t width,
		int32_t height,
		uint8_t atmo_light[3],
		uint8_t *processed_image
	);
#endif
	/**
	 * Auto levels.
	 * @return void.
	 */
	void auto_levels(
		uint8_t *raw_image,
		int32_t width,
		int32_t height,
		uint8_t **stretch_table,
		uint8_t *processed_image
	);
	/**
	 * Gamma correction with Neon speed up.
	 * @param[in] raw_image Raw image.
	 * @param[in] width Image width.
	 * @param[in] height Image height.
	 * @param[in] scale Scale factor.
	 * @param[in] power Power of input value.
	 * @param[out] processed_image Gamma corrected image.
	 * @return void.
	 */
	void gamma_correct(
		uint8_t *raw_image,
		int32_t width,
		int32_t height,
		float scale,
		float power,
		uint8_t *processed_image
	);
	/**
	 * Test adjust UV.
	 * @param[in] raw_y Raw Y component.
	 * @param[in] new_y Processed Y component.
	 * @param[in,out] uv UV component of image.
	 * @param[in] width Image width.
	 * @param[in] height Image height.
	 * @return void.
	 */
	void adjust_uv_HY(
		uint8_t *raw_y,
		uint8_t *new_y,
		uint8_t *uv,
		int32_t width,
		int32_t height
	);
	/**
	 * Saturation adjustment with Neon speed up.
	 * @param[in] raw_y Raw Y component.
	 * @param[in] new_y Processed Y component.
	 * @param[in,out] uv UV component of image.
	 * @param[in] width Image width.
	 * @param[in] height Image height.
	 * @return void.
	 */
	void saturation_adjustment(
		uint8_t *raw_y,
		uint8_t *new_y,
		uint8_t *uv,
		int32_t width,
		int32_t height
	);
	/**
	 * YUV420 to BGR24 pipeline thread.
	 * @return void*.
	 */
	friend void *yuv2bgr_pipeline_thread(
		void *param
	);
	/**
	 * Start YUV420 to BGR24 pipeline.
	 * @return void.
	 */
	void start_yuv2bgr_pipeline();
	/**
	 * Defog pipeline thread.
	 * @return void*.
	 */
	friend void *defog_pipeline_thread(
		void *param
	);
	/**
	 * Start defog pipeline.
	 * @return void.
	 */
	void start_defog_pipeline();
	/**
	 * BGR24 to YUV420 pipeline thread.
	 * @return void*.
	 */
	friend void *bgr2yuv_pipeline_thread(
		void *param
	);
	/**
	 * Start BGR24 to YUV420 pipeline.
	 * @return void.
	 */
	void start_bgr2yuv_pipeline();
	/**
	 * Init gamma correction table.
	 * @return void.
	 */
	void init_gamma_correct_table();
	/**
	 * Test classify image type.
	 * @param[in] yuv_image YUV image.
	 * @parma[in] width Image width.
	 * @parma[in] height Image height.
	 * @return void.
	 */
	float statistic_feature(
		uint8_t *yuv_image,
		int32_t width,
		int32_t height
	);
	/**
	 * Segmented linear transformation thread.
	 * @param[in] param Thread parameters.
	 * @return void.
	 */
	friend void *segment_linear_transf_thread(
		void *param
	);
	/**
	 * Start segmented linear transformation thread.
	 * @return void.
	 */
	void start_segment_linear_transf_thread();
	/**
	 * CLAHE thread.
	 * @param[in] param Thread parameters.
	 * @return void.
	 */
	friend void *clahe_thread(
		void *param
	);
	/**
	 * Start CLAHE thread.
	 * @return void.
	 */
	void start_clahe_thread();
	/**
	 * Edge enhancement thread.
	 * @param[in] param Thread parameters.
	 * @return void.
	 */
	friend void *edge_enhance_thread(
		void *param
	);
	/**
	 * Start edge enhancement thread.
	 * return void.
	 */
	void start_edge_enhance_thread();
	/**
	 * Saturation adjustment and motion adaptive noise reduction thread.
	 * @param[in] param Thread parameters.
	 * @return void.
	 */
	friend void *sat_adjust_manr_thread(
		void *param
	);
	/**
	 * Start saturation adjustment and motion adaptive noise reduction thread.
	 * @return void.
	 */
	void start_sat_adjust_manr_thread();

	std::queue<uint8_t *> in_yuv_image_queue;
	std::queue<uint8_t *> in_bgr_image_queue;
	std::queue<uint8_t *> out_bgr_image_queue;
	std::queue<uint8_t *> out_yuv_image_queue;
	pthread_mutex_t in_yuv_image_mutex;
	pthread_mutex_t in_bgr_image_mutex;
	pthread_mutex_t out_bgr_image_mutex;
	pthread_mutex_t out_yuv_image_mutex;
	std::queue<uint8_t *> raw_y_image_queue;
	std::queue<uint8_t *> clip_yuv_image_queue;
	std::queue<uint8_t *> slt_yuv_image_queue;
	std::queue<uint8_t *> clahe_yuv_image_queue;
	std::queue<uint8_t *> edge_yuv_image_queue;
	std::queue<uint8_t *> sa_manr_yuv_image_queue;
	pthread_mutex_t raw_y_image_mutex;
	pthread_mutex_t clip_yuv_image_mutex;
	pthread_mutex_t slt_yuv_image_mutex;
	pthread_mutex_t clahe_yuv_image_mutex;
	pthread_mutex_t edge_yuv_image_mutex;
	pthread_mutex_t sa_manr_yuv_image_mutex;
	int32_t width;						// Image width.
	int32_t height;						// Image height.
	float downsample;					// Down sample
	int32_t ds_width;					// Down sampled width.
	int32_t ds_height;					// Down sampled height.
	int32_t kmin_size;					// Patch size of minimum filter.
	int32_t kgud_size;					// Guided filter size.
	uint8_t *bgr_image;					// BGR image.
	uint8_t *dsbgr_image;				// Downsampled BGR image.
	uint8_t *hazzy_img;					// Input RGB image.
	uint8_t *rgb_img[3];				// RGB image array.
	uint8_t *dsrgb_img[3];				// Down sampled RGB image array.
	uint8_t *min_filt_rgb_img[3];		// Minimum filter RGB image array.
	uint8_t *min_chan_img;				// Minimum channel image.
	float bright_ratio_thresh;			// Brightest pixel ratio threshold of dark channel image.
	uint8_t atmo_light_ceiling;			// Atmospheric light ceiling.
	uint8_t atmo_light[3];				// Atmospheric light
	float *norm_min_filt_rgb_img[3];	// Normalized minimum filter RGB image array.
	float *norm_min_chan_img;			// Normalized minimum channel image.
	uint8_t *dark_chan_img;				// Dark channel image.
	float omega;						// Adjust factor of transmission image.
	float *transm_img;					// Transmission image.
	float *ustransm_img;				// Up sampled transmission image.
	float *transm_inv;					// Transmission inverse.
	uint8_t *u8_transmission_image;		// Unsigned int8-transmission image.
	float tran_thresh;					// Transmission threshold.
	uint8_t *recover_img;				// Recover image.
	float lowcut_thresh;				// Lowcut threshold of recover image.
	float highcut_thresh;				// Highcut threshold of recover image.
	uint8_t *stretch_img;				// Stretch image.
	int32_t frame_index;				// Frame index.
	uint8_t auto_level_floor[3];		// Auto level floor.
	uint8_t auto_level_ceiling[3];		// Auto level ceiling.
	int32_t update_period;				// Parameter update period.
	int32_t *transmission;				// Transmission map.
	uint8_t ***recover_table;			// Recover table.
	uint8_t **stretch_table;			// Stretch table.
	int32_t nrecover_threads;			// Number of recover threads.
	int32_t nstretch_threads;			// Number of stretch threads.
	float clip_limit;					// Histogram clip limit.
	float gamma;						// Gamma transformation power.
	uint8_t *gamma_correct_table;		// Gamma transformation table.
	uint8_t *old_y;						// Input Y image.
	uint8_t *prev_manr_y_image;			// Previous MANR Y image.
	uint8_t *mfilt_y_image;				// Median filter image.
	int32_t enable_uv_adjust;			// Saturation adjustment switch.
	int32_t enable_edge_enhan;			// Edge enhancement switch.
	int32_t slt[4];						// Segmented linear transformation parameters.
	bool init_prev_manr_y_image_flag;	// Init previous MANR Y image flag.
	int32_t enable_T_noise_reduce;		// Enable time noise reduction.
	int32_t enable_2D_noise_reduce;		// Enable 2D noise reduction.
	int32_t enable_module;
};

#endif