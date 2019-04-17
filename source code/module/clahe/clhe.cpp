#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <assert.h>
#include "opencv2/opencv.hpp"
#include "clhe.h"

static void MakeLut(uint8_t*, uint8_t, uint8_t, uint32_t);
static void MakeHistogram(uint8_t*, uint32_t, uint32_t, uint64_t*, uint32_t, uint8_t*);
static void ClipHistogram (uint64_t*, uint32_t, uint64_t);
static void MapHistogram (uint64_t*, uint8_t, uint8_t, uint32_t, uint64_t);
static void draw_histogram(uint64_t* pulHistogram, char *filename);

int32_t CLHE(uint8_t *pImage, uint32_t uiXRes, uint32_t uiYRes, uint8_t minm, uint8_t maxm, uint32_t uiNrBins, float fCliplimit)
{
	uint64_t ulClipLimit, ulNrPixels;			/* clip limit and region pixel count */
	uint8_t aLUT[uiNR_OF_GREY];					/* lookup table used for scaling of input image */
	uint64_t *pulMapArray;						/* pointer to histogram and mappings*/
	int32_t i;
#ifndef BYTE_IMAGE								/* #TPB FIX */
	if (maxm >= uiNR_OF_GREY) return -5;    	/* maximum too large */
#endif
	if (minm >= maxm) return -6;				/* minimum equal or larger than maximum */
	// if (uiNrX < 2 || uiNrY < 2) return -7;	/* at least 4 contextual regions required */
	if (fCliplimit == 1.0) return 0;			/* is OK, immediately returns original image. */
	if (uiNrBins == 0) uiNrBins = 128;			/* default value when not specified */

	pulMapArray = (uint64_t *)malloc(sizeof(uint64_t) * uiNrBins);
	if (pulMapArray == 0) return -8;			/* Not enough memory! (try reducing uiNrBins) */

	ulNrPixels = (uint64_t)uiXRes * (uint64_t)uiYRes;

	if(fCliplimit > 0.0) {						/* Calculate actual cliplimit  */
		ulClipLimit = (uint64_t) (fCliplimit * (uiXRes * uiYRes) / uiNrBins);
		ulClipLimit = (ulClipLimit < 1UL) ? 1UL : ulClipLimit;
	} else ulClipLimit = 1UL<<14;				/* Large value, do not clip (AHE) */
	
	MakeLut(aLUT, minm, maxm, uiNrBins);		/* Make lookup table for mapping of greyvalues */
	//for (i = minm; i <= maxm; i++) {
		//printf("%d %u\n", i, aLUT[i]);
	//}
	MakeHistogram(pImage, uiXRes, uiYRes, pulMapArray, uiNrBins, aLUT);
	// draw_histogram(pulMapArray, "hist.png");
	
	ClipHistogram(pulMapArray, uiNrBins, ulClipLimit);
	// draw_histogram(pulMapArray, "clip_hist.png");
	
	MapHistogram(pulMapArray, minm, maxm, uiNrBins, ulNrPixels);
	// draw_histogram(pulMapArray, "maphist.png");
	//for (i = minm; i <= maxm; i++) {
		//printf("%d %u\n", i, pulMapArray[i]);
	//}
	
	for (i = 0; i < ulNrPixels; i++) {
		pImage[i] = pulMapArray[aLUT[pImage[i]]];
	}
}

void MakeLut(uint8_t * pLUT, uint8_t minm, uint8_t maxm, uint32_t uiNrBins)
{
	int32_t i;
	const uint8_t BinSize = (uint8_t) (1 + (maxm - minm) / uiNrBins);

	for (i = minm; i <= maxm; i++)  pLUT[i] = (i - minm) / BinSize;
	for (i = 0; i < minm; i++)  pLUT[i] = pLUT[minm];
	for (i = maxm + 1; i <= 255; i++)  pLUT[i] = pLUT[maxm];
}

void MakeHistogram(uint8_t* pImage, uint32_t uiXRes, uint32_t uiYRes,
                   uint64_t* pulHistogram, uint32_t uiNrGreylevels, uint8_t* pLookupTable
)	{
	uint32_t i;
	uint32_t npixels;
	
	for (i = 0; i < uiNrGreylevels; i++) pulHistogram[i] = 0L; /* clear histogram */
	
	npixels = uiXRes * uiYRes;
	for (i = 0; i < npixels; i++) {
		pulHistogram[pLookupTable[*pImage++]]++;
	}
}

void ClipHistogram(uint64_t* pulHistogram, uint32_t uiNrGreylevels, uint64_t ulClipLimit)
{
	uint64_t* pulBinPointer, *pulEndPointer, *pulHisto;
	uint64_t ulNrExcess, ulUpper, ulBinIncr, ulStepSize, i;
	uint64_t ulOldNrExcess;  // #IAC Modification

	int64_t lBinExcess;

	ulNrExcess = 0;
	pulBinPointer = pulHistogram;
	for (i = 0; i < uiNrGreylevels; i++) { /* calculate total number of excess pixels */
		lBinExcess = (int64_t) pulBinPointer[i] - (int64_t) ulClipLimit;
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

void MapHistogram(uint64_t* pulHistogram, uint8_t minm, uint8_t maxm,
                  uint32_t uiNrGreylevels, uint64_t ulNrOfPixels
)	{
	unsigned int i;
	unsigned long ulSum = 0;
	const float fScale = ((float)(maxm - minm)) / ulNrOfPixels;
	const unsigned long ulMin = (unsigned long) minm;
	
	for (i = 0; i < uiNrGreylevels; i++) {
		ulSum += pulHistogram[i];
		pulHistogram[i]=(unsigned long)(ulMin+ulSum*fScale);
		if (pulHistogram[i] > maxm) pulHistogram[i] = maxm;
	}
}

void draw_histogram(uint64_t* pulHistogram, char *filename)
{
	cv::Mat hist_mat(200, 256, CV_8UC3, cv::Scalar(0, 0, 0));
	uint64_t max_val = 0;
	for (int32_t i = 0; i < 256; i++) {
		if (pulHistogram[i] > max_val) {
			max_val = pulHistogram[i];
		}
	}

	for (int32_t i = 0; i < 256; i++) {
		cv::line(hist_mat, cv::Point(i, 200 - 180.0 * pulHistogram[i] / max_val), cv::Point(i, 199), cv::Scalar(255, 255, 255));
	}
	
	cv::imwrite(filename, hist_mat);
}