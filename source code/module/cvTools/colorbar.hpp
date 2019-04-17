#ifndef _COLORBAR_H_
#define _COLORBAR_H_

#include "opencv2/opencv.hpp"

/**
 * Load pseudo color map table.
 * @param[out] map_tab Map table.
 * @param[in] colorbar_code Pseudo color style.
 * @return void.
 */
static void load_pseudo_color_map_table(
	uint32_t (*map_tab)[3],
	int32_t colorbar_code
)	{
	assert(map_tab);
	char filename[64] = {0};
	sprintf(filename, "colorbar\\colorbar-%d.txt", colorbar_code);
	FILE *fp = fopen(filename, "r");
	if (!fp) {
		printf("Cannot load colorbar, use default sets.\n");
		const int32_t max_val = 255;
		for (int32_t lev = 0; lev < max_val; lev++) {
			map_tab[lev][0] = lev;
			map_tab[lev][1] = lev;
			map_tab[lev][2] = lev;
		}
		
		return;
	}
	
	const uint32_t read_length = 128;
	char line_buf[128];
	uint32_t index = 0;
	int32_t lev = 255;
	while ((fgets(line_buf, read_length, fp)) != NULL) {
		sscanf(line_buf, "%u %u %u %u\n", &index, &map_tab[lev][0], &map_tab[lev][1], &map_tab[lev][2]);
		lev--;
	}
	
	fclose(fp);
}

/**
 * Map grayscale to pseudo color value.
 * @param[in] gmat Grayscale mat.
 * @param[out] cmat Pseudo color mat.
 * @param[in] map_tab Pseudo map table.
 * @param[in] min_idx Minimum pixel index.
 * @param[in] max_idx Maximum pixel index.
 * @param[in] min_temper Minimum temperature.
 * @param[in] max_temper Maximum temperature.
 * @return void.
 */
static void draw_pseudo_color_map(
	cv::Mat gmat,
	cv::Mat cmat,
	uint32_t (*map_tab)[3],
	int32_t colorbar_code,
	int32_t min_idx,
	int32_t max_idx,
	double min_temper,
	double max_temper
)	{
	assert(!gmat.empty());
	assert(!cmat.empty());

	// Overlay pseudo color for image.
	const int32_t nchannels = 3;
	for (int32_t y = 0; y < gmat.rows; y++) {
		for (int32_t x = 0; x < gmat.cols; x++) {
			int32_t gindex = y * gmat.cols + x;
			for (int32_t c = 0; c < nchannels; c++) {
				*(cmat.data + 3 * gindex + c) = map_tab[*(gmat.data + gindex)][nchannels - 1 - c];
			}
		}
	}

	// Make colorbar mask.
	char filename[64] = {0};
	sprintf(filename, "colorbar\\colorbar-%d.png", colorbar_code);
	cv::Mat strip_mat = cv::imread(filename);
	if (strip_mat.empty()) {
		return;
	}
	
	// Draw colorbar.
	const int32_t right_margin = 32;
	const int32_t up_down_margin = 64;
	cv::Mat resize_strip_mat = cv::Mat(gmat.rows - up_down_margin * 2, strip_mat.cols, CV_8UC3);
	cv::resize(strip_mat, resize_strip_mat, cv::Size(strip_mat.cols, gmat.rows - up_down_margin * 2));
	
	const int32_t strip_x_position = gmat.cols - resize_strip_mat.cols - right_margin;
	const int32_t strip_y_position = up_down_margin;
	cv::Mat strip_rect = cmat(cv::Rect(strip_x_position, strip_y_position, resize_strip_mat.cols, resize_strip_mat.rows));
	resize_strip_mat.copyTo(strip_rect);
	
	// Draw minimum temperature point.
	int32_t x = min_idx % gmat.cols;
	int32_t y = min_idx / gmat.cols;
	
	char min_text[64];
	sprintf(min_text, "%.1lf", min_temper);
	cv::Point bar_min_pt = cv::Point(strip_x_position - 5, gmat.rows - up_down_margin + 20);
	putText(cmat, min_text, bar_min_pt, CV_FONT_HERSHEY_SIMPLEX, 0.4, CV_RGB(255, 255, 255), 1, 8, false);
	
	// Draw maximum temperature point.
	x = max_idx % gmat.cols;
	y = max_idx / gmat.cols;
	
	char max_text[64];
	sprintf(max_text, "%.1lf", max_temper);
	cv::Point bar_max_pt = cv::Point(strip_x_position - 5, up_down_margin - 15);
	putText(cmat, max_text, bar_max_pt, CV_FONT_HERSHEY_SIMPLEX, 0.4, CV_RGB(255, 255, 255), 1, 8, false);
}

#endif