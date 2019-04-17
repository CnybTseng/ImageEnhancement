// *****************************************************************************
/*
 * ANSI C code from the article
 * "Contrast Limited Adaptive Histogram Equalization"
 * by Karel Zuiderveld, karel@cv.ruu.nl
 * in "Graphics Gems IV", Academic Press, 1994
 *
 *
 *  These functions implement Contrast Limited Adaptive Histogram Equalization.
 *  The main routine (CLAHE) expects an input image that is stored contiguously in
 *  memory;  the CLAHE output image overwrites the original input image and has the
 *  same minimum and maximum values (which must be provided by the user).
 *  This implementation assumes that the X- and Y image resolutions are an integer
 *  multiple of the X- and Y sizes of the contextual regions. A check on various other
 *  error conditions is performed.
 *
 *  #define the symbol BYTE_IMAGE to make this implementation suitable for
 *  8-bit images. The maximum number of contextual regions can be redefined
 *  by changing uiMAX_REG_X and/or uiMAX_REG_Y; the use of more than 256
 *  contextual regions is not recommended.
 *
 *  The code is ANSI-C and is also C++ compliant.
 *
 *  Author: Karel Zuiderveld, Computer Vision Research Group,
 *           Utrecht, The Netherlands (karel@cv.ruu.nl)
 */

/*

EULA: The Graphics Gems code is copyright-protected. In other words, you cannot
claim the text of the code as your own and resell it. Using the code is permitted
in any program, product, or library, non-commercial or commercial. Giving credit
is not required, though is a nice gesture. The code comes as-is, and if there are
any flaws or problems with any Gems code, nobody involved with Gems - authors,
editors, publishers, or webmasters - are to be held responsible. Basically,
don't be a jerk, and remember that anything free comes with no guarantee.

- http://tog.acm.org/resources/GraphicsGems/ (August 2009)

*/

#include "clahe.h"
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <assert.h>
#ifdef __ARM_NEON__ 
#include <arm_neon.h>
#endif
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <queue>
#include "opencv2/opencv.hpp"

#define SLICES_PER_THREAD	(5)
#define SKEWNESS_LIMIT		(12.7278)
#define NUM_SUB_HIST		(16)
#define MAX_GRAY_LEVEL		(255)

static bool reset_flag;
static float g_skewness;
static unsigned int g_weaker_bins;

static cv::Point grid[4][5] = {
	{cv::Point(72, 72), cv::Point(216, 72), cv::Point(360, 72), cv::Point(504, 72), cv::Point(648, 72)},
	{cv::Point(72, 216), cv::Point(216, 216), cv::Point(360, 216), cv::Point(504, 216), cv::Point(648, 216)},
	{cv::Point(72, 360), cv::Point(216, 360), cv::Point(360, 360), cv::Point(504, 360), cv::Point(648, 360)},
	{cv::Point(72, 504), cv::Point(216, 504), cv::Point(360, 504), cv::Point(504, 504), cv::Point(648,504 )}
};

static void MakeLut (kz_pixel_t*, kz_pixel_t, kz_pixel_t, unsigned int);
static void MakeHistogram (kz_pixel_t*, unsigned int, unsigned int, unsigned int,
                           unsigned long*, unsigned int, kz_pixel_t*);
static void ClipHistogram (unsigned long*, unsigned int, unsigned long);
static void MapHistogram (unsigned long*, kz_pixel_t, kz_pixel_t,
                          unsigned int, unsigned long);
static void Interpolate (kz_pixel_t*, int, unsigned long*, unsigned long*,
                         unsigned long*, unsigned long*, unsigned int, unsigned int, kz_pixel_t*);
static void DrawHistogram(unsigned long* pulHistogram, char *filename);
static float HistogramSkewness(unsigned long* pulHistogram, unsigned int uiNrGreylevels);

/* To speed up histogram clipping, the input image [Min,Max] is scaled down to
 * [0,uiNrBins-1]. This function calculates the LUT.
 */
void MakeLut (kz_pixel_t * pLUT, kz_pixel_t Min, kz_pixel_t Max, unsigned int uiNrBins)
{
	int i;
	const kz_pixel_t BinSize = (kz_pixel_t) (1 + (Max - Min) / uiNrBins);

	for (i = Min; i <= Max; i++)  pLUT[i] = (i - Min) / BinSize;
	for (i = 0; i < Min; i++)  pLUT[i] = pLUT[Min];
	for (i = Max + 1; i <= 255; i++)  pLUT[i] = pLUT[Max];
}

/* This function classifies the greylevels present in the array image into
 * a greylevel histogram. The pLookupTable specifies the relationship
 * between the greyvalue of the pixel (typically between 0 and 4095) and
 * the corresponding bin in the histogram (usually containing only 128 bins).
 */
void MakeHistogram (kz_pixel_t* pImage, unsigned int uiXRes,
                    unsigned int uiSizeX, unsigned int uiSizeY,
                    unsigned long* pulHistogram,
                    unsigned int uiNrGreylevels, kz_pixel_t* pLookupTable
)	{
	kz_pixel_t* pImagePointer;
	unsigned int i, j, k;
	const unsigned int pixels_per_load = 16;
	unsigned int subHistogram[NUM_SUB_HIST][MAX_GRAY_LEVEL + 1];
	memset(subHistogram, 0, sizeof(subHistogram));

	for (i = 0; i < uiNrGreylevels; i++) pulHistogram[i] = 0L; /* clear histogram */

#ifdef __ARM_NEON__
	for (i = 0; i < uiSizeY; i++) {
		for (j = 0; j < uiSizeX; j += pixels_per_load) {
			uint8x16_t data_vec = vld1q_u8(pImage);
			
			for (k = 0; k < NUM_SUB_HIST; k++) {
				subHistogram[k][data_vec[k]]++;
			}
			
			pImage += pixels_per_load;
		}
		pImage = &pImage[uiXRes-uiSizeX];
	}
	
	for (i = 0; i <= MAX_GRAY_LEVEL; i++) {
		for (k = 0; k < NUM_SUB_HIST; k++) {
			pulHistogram[i] += subHistogram[k][i];
		}
	}
#else	
	for (i = 0; i < uiSizeY; i++) {
		pImagePointer = &pImage[uiSizeX];
		while (pImage < pImagePointer) pulHistogram[*pImage++]++;
		pImage = &pImage[uiXRes-uiSizeX];
	}
#endif
}

/* This function performs clipping of the histogram and redistribution of bins.
 * The histogram is clipped and the number of excess pixels is counted. Afterwards
 * the excess pixels are equally redistributed across the whole histogram (providing
 * the bin count is smaller than the cliplimit).
 */
void ClipHistogram (unsigned long* pulHistogram, unsigned int
                    uiNrGreylevels, unsigned long ulClipLimit
)	{
	unsigned long* pulBinPointer, *pulEndPointer, *pulHisto;
	unsigned long ulNrExcess, ulUpper, ulBinIncr, ulStepSize, i;
	unsigned long ulOldNrExcess;  // #IAC Modification

	long lBinExcess;
	
	const unsigned long weaker_clip_limit = 10;
	const unsigned long lower_clip_limit = 162;
	unsigned int weaker_bins = 0;
	float skewness;
	
	skewness = HistogramSkewness(pulHistogram, uiNrGreylevels);

	for (i = 0; i < uiNrGreylevels; i++) {
		if ((long) pulHistogram[i] < weaker_clip_limit) weaker_bins++;
	}
	
	if (skewness < -9 && weaker_bins > 85) {
		g_skewness = skewness;
		g_weaker_bins = weaker_bins;
		ulClipLimit = lower_clip_limit;
#ifdef TEST_HIST
		reset_flag = true;
#endif
	}
	
	ulNrExcess = 0;
	pulBinPointer = pulHistogram;
	for (i = 0; i < uiNrGreylevels; i++) { /* calculate total number of excess pixels */
		lBinExcess = (long) pulBinPointer[i] - (long) ulClipLimit;
		if (lBinExcess > 0) ulNrExcess += lBinExcess;     /* excess in current bin */
	};

	/* Second part: clip histogram and redistribute excess pixels in each bin */
	ulBinIncr = ulNrExcess / uiNrGreylevels;              /* average binincrement */
	ulUpper =  ulClipLimit - ulBinIncr;  /* Bins larger than ulUpper set to cliplimit */

	for (i = 0; i < uiNrGreylevels; i++) {
		if (pulHistogram[i] > ulClipLimit) pulHistogram[i] = ulClipLimit; /* clip bin */
		else {
			if (pulHistogram[i] > ulUpper) {              /* high bin count */
				ulNrExcess -= pulHistogram[i] - ulUpper;
				pulHistogram[i]=ulClipLimit;
			} else {                                      /* low bin count */
				ulNrExcess -= ulBinIncr;
				pulHistogram[i] += ulBinIncr;
			}
		}
	}

	do {   /* Redistribute remaining excess  */
		pulEndPointer = &pulHistogram[uiNrGreylevels];
		pulHisto = pulHistogram;

		ulOldNrExcess = ulNrExcess;     /* Store number of excess pixels for test later. */

		while (ulNrExcess && pulHisto < pulEndPointer) {
			ulStepSize = uiNrGreylevels / ulNrExcess;
			if (ulStepSize < 1)
				ulStepSize = 1;       /* stepsize at least 1 */
			for (pulBinPointer=pulHisto; pulBinPointer < pulEndPointer && ulNrExcess;
			     pulBinPointer += ulStepSize) {
				if (*pulBinPointer < ulClipLimit) {
					(*pulBinPointer)++;
					ulNrExcess--;    /* reduce excess */
				}
			}
			pulHisto++;       /* restart redistributing on other bin location */
		}
	} while ((ulNrExcess) && (ulNrExcess < ulOldNrExcess));
	/* Finish loop when we have no more pixels or we can't redistribute any more pixels */
}

/* This function calculates the equalized lookup table (mapping) by
 * cumulating the input histogram. Note: lookup table is rescaled in range [Min..Max].
 */
void MapHistogram (unsigned long* pulHistogram, kz_pixel_t Min, kz_pixel_t Max,
                   unsigned int uiNrGreylevels, unsigned long ulNrOfPixels
)	{
	unsigned int i;
	unsigned long ulSum = 0;
	const float fScale = ((float)(Max - Min)) / ulNrOfPixels;
	const unsigned long ulMin = (unsigned long) Min;
	
	for (i = 0; i < uiNrGreylevels; i++) {
		ulSum += pulHistogram[i];
		pulHistogram[i]=(unsigned long)(ulMin+ulSum*fScale);
		if (pulHistogram[i] > Max) pulHistogram[i] = Max;
	}
}

/* pImage      - pointer to input/output image
 * uiXRes      - resolution of image in x-direction
 * pulMap*     - mappings of greylevels from histograms
 * uiXSize     - uiXSize of image submatrix
 * uiYSize     - uiYSize of image submatrix
 * pLUT        - lookup table containing mapping greyvalues to bins
 * This function calculates the new greylevel assignments of pixels within a submatrix
 * of the image with size uiXSize and uiYSize. This is done by a bilinear interpolation
 * between four different mappings in order to eliminate boundary artifacts.
 * It uses a division; since division is often an expensive operation, I added code to
 * perform a logical shift instead when feasible.
 */
void Interpolate (kz_pixel_t * pImage, int uiXRes, unsigned long * pulMapLU,
                  unsigned long * pulMapRU, unsigned long * pulMapLB,  unsigned long * pulMapRB,
                  unsigned int uiXSize, unsigned int uiYSize, kz_pixel_t * pLUT
)	{
	const unsigned int uiIncr = uiXRes-uiXSize; /* Pointer increment after processing row */
	kz_pixel_t GreyValue;
	unsigned int uiNum = uiXSize*uiYSize; /* Normalization factor */

	unsigned int uiXCoef, uiYCoef, uiXInvCoef, uiYInvCoef, uiShift = 0;

	if (uiNum & (uiNum - 1))   /* If uiNum is not a power of two, use division */
		for (uiYCoef = 0, uiYInvCoef = uiYSize; uiYCoef < uiYSize;
		     uiYCoef++, uiYInvCoef--,pImage+=uiIncr) {
			for (uiXCoef = 0, uiXInvCoef = uiXSize; uiXCoef < uiXSize;
			     uiXCoef++, uiXInvCoef--) {
				GreyValue = *pImage;             /* get histogram bin value */
				*pImage++ = (kz_pixel_t ) ((uiYInvCoef * (uiXInvCoef*pulMapLU[GreyValue]
				                            + uiXCoef * pulMapRU[GreyValue])
				                            + uiYCoef * (uiXInvCoef * pulMapLB[GreyValue]
				                                            + uiXCoef * pulMapRB[GreyValue])) / uiNum);
			}
		}
	else {                         /* avoid the division and use a right shift instead */
		while (uiNum >>= 1) uiShift++;             /* Calculate 2log of uiNum */
		for (uiYCoef = 0, uiYInvCoef = uiYSize; uiYCoef < uiYSize;
		     uiYCoef++, uiYInvCoef--,pImage+=uiIncr) {
			for (uiXCoef = 0, uiXInvCoef = uiXSize; uiXCoef < uiXSize;
			     uiXCoef++, uiXInvCoef--) {
				GreyValue = *pImage;         /* get histogram bin value */
				*pImage++ = (kz_pixel_t)((uiYInvCoef* (uiXInvCoef * pulMapLU[GreyValue]
				                                       + uiXCoef * pulMapRU[GreyValue])
				                          + uiYCoef * (uiXInvCoef * pulMapLB[GreyValue]
				                                       + uiXCoef * pulMapRB[GreyValue])) >> uiShift);
			}
		}
	}
}

/* pImage      - pointer to input/output image
 * uiXRes      - resolution of image in x-direction
 * pulMap*     - mappings of greylevels from histograms
 * uiXSize     - uiXSize of image submatrix
 * uiYSize     - uiYSize of image submatrix
 * pLUT        - lookup table containing mapping greyvalues to bins
 * This function calculates the new greylevel assignments of pixels within a submatrix
 * of the image with size uiXSize and uiYSize. This is done by a bilinear interpolation
 * between four different mappings in order to eliminate boundary artifacts.
 * It uses Neon to speed up.
 */
#ifdef __ARM_NEON__ 
void NeonInterpolate (kz_pixel_t * pImage, int uiXRes,
                  unsigned long * pulMapLU, unsigned long * pulMapRU,
				  unsigned long * pulMapLB, unsigned long * pulMapRB,
                  unsigned int uiXSize, unsigned int uiYSize, kz_pixel_t * pLUT
)	{
	const unsigned int uiIncr = uiXRes-uiXSize; /* Pointer increment after processing row */
	kz_pixel_t GreyValue;
	unsigned int uiNum = uiXSize*uiYSize; /* Normalization factor */

	unsigned int uiXCoef, uiYCoef, uiXInvCoef, uiYInvCoef, uiShift = 0;

	uint32x4_t ycoef_inv = vdupq_n_u32(uiYSize);
	uint32x4_t ycoef_step = vdupq_n_u32(1);

	uint32x4_t xcoef_step = vdupq_n_u32(8);

	float32x4_t f32x4_num = vdupq_n_f32(uiNum);
	float32x4_t inv = vrecpeq_f32(f32x4_num);
	float32x4_t restep = vrecpsq_f32(f32x4_num, inv);
	f32x4_num = vmulq_f32(restep, inv);	
/*	
	float scale = 1 << 16;
	float32x4_t f32x4_scale_num = vmulq_n_f32(f32x4_num, scale);
	uint32x4_t u32x4_scale_num = vcvtq_u32_f32(f32x4_scale_num);
*/
	for (uiYCoef = 0; uiYCoef < uiYSize; uiYCoef++, pImage+=uiIncr) {
		uint32x4_t ycoef = vdupq_n_u32(uiYCoef);

		uint32x4x2_t xcoef;
		uint32x4x2_t xcoef_inv;
		
		for (int32_t i = 0; i < 4; i++) {
			xcoef.val[0][i] = i;
			xcoef_inv.val[0][i] = uiXSize - i;
		}
		
		for (int32_t i = 4; i < 8; i++) {
			xcoef.val[1][i - 4] = i;
			xcoef_inv.val[1][i - 4] = uiXSize - i;
		}

		for (uiXCoef = 0; uiXCoef < uiXSize; uiXCoef += 8) {
			uint8x8_t u8x8_from_data = vld1_u8(pImage);

			uint32x4x2_t map_lu;
			uint32x4x2_t map_ru;
			uint32x4x2_t map_lb;
			uint32x4x2_t map_rb;
			
			for (int32_t i = 0; i < 4; i++) {
				map_lu.val[0][i] = pulMapLU[u8x8_from_data[i]];
				map_ru.val[0][i] = pulMapRU[u8x8_from_data[i]];
				map_lb.val[0][i] = pulMapLB[u8x8_from_data[i]];
				map_rb.val[0][i] = pulMapRB[u8x8_from_data[i]];
			}
			
			for (int32_t i = 4; i < 8; i++) {
				map_lu.val[1][i - 4] = pulMapLU[u8x8_from_data[i]];
				map_ru.val[1][i - 4] = pulMapRU[u8x8_from_data[i]];
				map_lb.val[1][i - 4] = pulMapLB[u8x8_from_data[i]];
				map_rb.val[1][i - 4] = pulMapRB[u8x8_from_data[i]];
			}

			uint32x4x2_t u32x4x2_temp;
			u32x4x2_temp.val[0] = vaddq_u32(vmulq_u32(ycoef_inv, vaddq_u32(vmulq_u32(xcoef_inv.val[0],
				map_lu.val[0]), vmulq_u32(xcoef.val[0], map_ru.val[0]))), vmulq_u32(ycoef,
				vaddq_u32(vmulq_u32(xcoef_inv.val[0], map_lb.val[0]), vmulq_u32(xcoef.val[0], map_rb.val[0]))));

			u32x4x2_temp.val[1] = vaddq_u32(vmulq_u32(ycoef_inv, vaddq_u32(vmulq_u32(xcoef_inv.val[1],
				map_lu.val[1]), vmulq_u32(xcoef.val[1], map_ru.val[1]))), vmulq_u32(ycoef,
				vaddq_u32(vmulq_u32(xcoef_inv.val[1], map_lb.val[1]), vmulq_u32(xcoef.val[1], map_rb.val[1]))));

			float32x4x2_t f32x4x2_temp;
			f32x4x2_temp.val[0] = vcvtq_f32_u32(u32x4x2_temp.val[0]);
			f32x4x2_temp.val[1] = vcvtq_f32_u32(u32x4x2_temp.val[1]);

			float32x4x2_t f32x4x2_result;
			f32x4x2_result.val[0] = vmulq_f32(f32x4x2_temp.val[0], f32x4_num);
			f32x4x2_result.val[1] = vmulq_f32(f32x4x2_temp.val[1], f32x4_num);

			uint32x4x2_t u32x4x2_result;
			u32x4x2_result.val[0] = vcvtq_u32_f32(f32x4x2_result.val[0]);
			u32x4x2_result.val[1] = vcvtq_u32_f32(f32x4x2_result.val[1]);

			uint16x4x2_t u16x4x2_result;
			u16x4x2_result.val[0] = vmovn_u32(u32x4x2_result.val[0]);
			u16x4x2_result.val[1] = vmovn_u32(u32x4x2_result.val[1]);
/*			
			uint16x4x2_t u16x4x2_result;
			u16x4x2_result.val[0] = vshrn_n_u32(vmulq_u32(u32x4x2_temp.val[0], u32x4_scale_num), 16);
			u16x4x2_result.val[1] = vshrn_n_u32(vmulq_u32(u32x4x2_temp.val[1], u32x4_scale_num), 16);
*/
			uint16x8_t u16x8_result;
			u16x8_result = vcombine_u16(u16x4x2_result.val[0], u16x4x2_result.val[1]);

			uint8x8_t u8x8_result;
			u8x8_result = vqmovn_u16(u16x8_result);

			vst1_u8(pImage, u8x8_result);

			pImage += 8;
			xcoef.val[0] = vaddq_u32(xcoef.val[0], xcoef_step);
			xcoef.val[1] = vaddq_u32(xcoef.val[1], xcoef_step);
			xcoef_inv.val[0] = vsubq_u32(xcoef_inv.val[0], xcoef_step);
			xcoef_inv.val[1] = vsubq_u32(xcoef_inv.val[1], xcoef_step);
		}
		ycoef_inv = vsubq_u32(ycoef_inv, ycoef_step);
	}
}
#endif

void DrawHistogram(unsigned long* pulHistogram, char *filename)
{
	const unsigned int width = 256;
	const unsigned int height = 200;
	const unsigned int toppest = 180;
	cv::Mat hist_mat(height, width, CV_8UC1, cv::Scalar(0));
	unsigned long maximum = 0;
	for (int32_t i = 0; i < width; i++) {
		if (pulHistogram[i] > maximum) {
			maximum = pulHistogram[i];
		}
	}

	for (int32_t i = 0; i < width; i++) {
		cv::line(hist_mat, cv::Point(i, height - (float)toppest * pulHistogram[i] / maximum),
			cv::Point(i, height - 1), cv::Scalar(255));
	}
	
	cv::imwrite(filename, hist_mat);
}

float HistogramSkewness(unsigned long* pulHistogram, unsigned int uiNrGreylevels)
{
	unsigned long sum = 0;
	int mean = 0;
	float third_order_moment = 0;
	
	for (int i = 0; i < uiNrGreylevels; i++) {
		sum += i * pulHistogram[i];
	}
	
	mean = (int)((float)sum / 20736);
	float sum1 = 0, sum2 = 0;
	
	for (int i = 0; i < uiNrGreylevels; i++) {
		int delta = i - mean;
		int delta_square = delta * delta;
		sum1 += delta_square * delta;
		sum2 += delta_square;
	}
	
	sum1 /= 20736;
	sum2 /= 20736;
	sum2 = pow(sum2, 1.5);
	third_order_moment = sum1 / sum2;

	return third_order_moment;
}

/*   pImage - Pointer to the input/output image
 *   uiXRes - Image resolution in the X direction
 *   uiYRes - Image resolution in the Y direction
 *   Min - Minimum greyvalue of input image (also becomes minimum of output image)
 *   Max - Maximum greyvalue of input image (also becomes maximum of output image)
 *   uiNrX - Number of contextial regions in the X direction (min 2, max uiMAX_REG_X)
 *   uiNrY - Number of contextial regions in the Y direction (min 2, max uiMAX_REG_Y)
 *   uiNrBins - Number of greybins for histogram ("dynamic range")
 *   float fCliplimit - Normalized cliplimit (higher values give more contrast)
 * The number of "effective" greylevels in the output image is set by uiNrBins; selecting
 * a small value (eg. 128) speeds up processing and still produce an output image of
 * good quality. The output image will have the same minimum and maximum value as the input
 * image. A clip limit smaller than 1 results in standard (non-contrast limited) AHE.
 */
int CLAHEq (kz_pixel_t* pImage, unsigned int uiXRes, unsigned int uiYRes,
            kz_pixel_t Min, kz_pixel_t Max, unsigned int uiNrX, unsigned int uiNrY,
            unsigned int uiNrBins, float fCliplimit
)	{
	unsigned int uiX, uiY;                /* counters */
	unsigned int uiXSize, uiYSize, uiSubX, uiSubY; /* size of context. reg. and subimages */
	unsigned int uiXL, uiXR, uiYU, uiYB;  /* auxiliary variables interpolation routine */
	unsigned long ulClipLimit, ulNrPixels;/* clip limit and region pixel count */
	kz_pixel_t* pImPointer;                /* pointer to image */
	kz_pixel_t aLUT[uiNR_OF_GREY];          /* lookup table used for scaling of input image */
	unsigned long* pulHist, *pulMapArray; /* pointer to histogram and mappings*/
	unsigned long* pulLU, *pulLB, *pulRU, *pulRB; /* auxiliary pointers interpolation */

	if (uiNrX > uiXRes) return -1;    /* # of regions x-direction too large */
	if (uiNrY > uiYRes) return -2;    /* # of regions y-direction too large */
	if (uiXRes % uiNrX) return -3;        /* x-resolution no multiple of uiNrX */
	if (uiYRes % uiNrY) return -4;        /* y-resolution no multiple of uiNrY #TPB FIX */
#ifndef BYTE_IMAGE					  /* #TPB FIX */
	if (Max >= uiNR_OF_GREY) return -5;    /* maximum too large */
#endif
	if (Min >= Max) return -6;            /* minimum equal or larger than maximum */
	if (uiNrX < 2 || uiNrY < 2) return -7;/* at least 4 contextual regions required */
	if (fCliplimit == 1.0) return 0;      /* is OK, immediately returns original image. */
	if (uiNrBins == 0) uiNrBins = 128;    /* default value when not specified */

	pulMapArray=(unsigned long *)malloc(sizeof(unsigned long)*uiNrX*uiNrY*uiNrBins);
	if (pulMapArray == 0) return -8;      /* Not enough memory! (try reducing uiNrBins) */

	uiXSize = uiXRes/uiNrX;
	uiYSize = uiYRes/uiNrY;  /* Actual size of contextual regions */
	ulNrPixels = (unsigned long)uiXSize * (unsigned long)uiYSize;

	if(fCliplimit > 0.0) {                /* Calculate actual cliplimit  */
		ulClipLimit = (unsigned long) (fCliplimit * (uiXSize * uiYSize) / uiNrBins);
		ulClipLimit = (ulClipLimit < 1UL) ? 1UL : ulClipLimit;
	} else ulClipLimit = 1UL<<14;         /* Large value, do not clip (AHE) */

#ifdef TEST_CLAHE	
	clock_t total[3] = {0, 0, 0};
	clock_t start = 0, finish = 0;
#endif

	// MakeLut(aLUT, Min, Max, uiNrBins);    /* Make lookup table for mapping of greyvalues */

	char filename[64];
	/* Calculate greylevel mappings for each contextual region */
	for (uiY = 0, pImPointer = pImage; uiY < uiNrY; uiY++) {
		for (uiX = 0; uiX < uiNrX; uiX++, pImPointer += uiXSize) {
			pulHist = &pulMapArray[uiNrBins * (uiY * uiNrX + uiX)];
#ifdef TEST_CLAHE
			start = clock();
#endif
			MakeHistogram(pImPointer,uiXRes,uiXSize,uiYSize,pulHist,uiNrBins,aLUT);
#ifdef TEST_CLAHE
			finish = clock();
			total[0] += (finish - start);
#endif

#ifdef TEST_CLAHE
			start = clock();
#endif
#ifdef TEST_HIST
			sprintf(filename, "hist_%d_%d.png", uiY, uiX);
			DrawHistogram(pulHist, filename);
			reset_flag = false;
#endif
			ClipHistogram(pulHist, uiNrBins, ulClipLimit);
#ifdef TEST_HIST
			sprintf(filename, "clip_hist_%d_%d.png", uiY, uiX);
			DrawHistogram(pulHist, filename);

			if (reset_flag) {
				printf("Reset clip limit with skewness %.0f and weaker bins %u\n", g_skewness, g_weaker_bins);
				cv::circle(cv::Mat(uiYRes, uiXRes, CV_8UC1, pImage), grid[uiY][uiX], 16, cv::Scalar(255), 16);
			}
#endif
#ifdef TEST_CLAHE
			finish = clock();
			total[1] += (finish - start);

			start = clock();
#endif
			MapHistogram(pulHist, Min, Max, uiNrBins, ulNrPixels);
#ifdef TEST_HIST
			sprintf(filename, "map_hist%d_%d.png", uiY, uiX);
			DrawHistogram(pulHist, filename);
#endif
#ifdef TEST_CLAHE
			finish = clock();
			total[2] += (finish - start);
#endif
		}
		pImPointer += (uiYSize - 1) * uiXRes;             /* skip lines, set pointer */
	}
	
#ifdef TEST_CLAHE
	printf("MakeHistogram %lfms\n", 1000.0 * total[0] / CLOCKS_PER_SEC);
	printf("ClipHistogram %lfms\n", 1000.0 * total[1] / CLOCKS_PER_SEC);
	printf("MapHistogram %lfms\n", 1000.0 * total[2] / CLOCKS_PER_SEC);
	start = clock();
#endif

	/* Interpolate greylevel mappings to get CLAHE image */
	for (pImPointer = pImage, uiY = 0; uiY <= uiNrY; uiY++) {
		if (uiY == 0) {                                   /* special case: top row */
			uiSubY = uiYSize >> 1;
			uiYU = 0;
			uiYB = 0;
		} else {
			if (uiY == uiNrY) {                           /* special case: bottom row */
				uiSubY = uiYSize >> 1;
				uiYU = uiNrY-1;
				uiYB = uiYU;
			} else {                                      /* default values */
				uiSubY = uiYSize;
				uiYU = uiY - 1;
				uiYB = uiYU + 1;
			}
		}
		for (uiX = 0; uiX <= uiNrX; uiX++) {
			if (uiX == 0) {                               /* special case: left column */
				uiSubX = uiXSize >> 1;
				uiXL = 0;
				uiXR = 0;
			} else {
				if (uiX == uiNrX) {                       /* special case: right column */
					uiSubX = uiXSize >> 1;
					uiXL = uiNrX - 1;
					uiXR = uiXL;
				} else {                                  /* default values */
					uiSubX = uiXSize;
					uiXL = uiX - 1;
					uiXR = uiXL + 1;
				}
			}

			pulLU = &pulMapArray[uiNrBins * (uiYU * uiNrX + uiXL)];
			pulRU = &pulMapArray[uiNrBins * (uiYU * uiNrX + uiXR)];
			pulLB = &pulMapArray[uiNrBins * (uiYB * uiNrX + uiXL)];
			pulRB = &pulMapArray[uiNrBins * (uiYB * uiNrX + uiXR)];
#ifdef __ARM_NEON__ 
			NeonInterpolate(pImPointer,uiXRes,pulLU,pulRU,pulLB,pulRB,uiSubX,uiSubY,aLUT);
#else
			Interpolate(pImPointer,uiXRes,pulLU,pulRU,pulLB,pulRB,uiSubX,uiSubY,aLUT);
#endif
			pImPointer += uiSubX;                         /* set pointer on next matrix */
		}
		pImPointer += (uiSubY - 1) * uiXRes;
	}
#ifdef TEST_CLAHE
	finish = clock();
	printf("Interpolate %lfms.\n", 1000.0 * (finish - start) / CLOCKS_PER_SEC);
#endif
	free(pulMapArray);                                    /* free space for histograms */
	return 0;                                             /* return status OK */
}