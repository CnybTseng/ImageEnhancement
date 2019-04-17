#ifndef _CLHE_H_
#define _CLHE_H_

#include <stdint.h>

#define uiNR_OF_GREY (256)

#ifdef __cplusplus
extern "C"
{
#endif

int32_t CLHE(uint8_t *pImage, uint32_t uiXRes, uint32_t uiYRes, uint8_t minm, uint8_t maxm, uint32_t uiNrBins, float fCliplimit);

#ifdef __cplusplus
}
#endif

#endif