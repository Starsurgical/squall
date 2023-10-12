#ifndef STORM_REGION_HPP
#define STORM_REGION_HPP

#include "storm/region/Types.hpp"
#include <cstdint>

// 521
void SRgnClear(HSRGN handle);

// 523
void SRgnCombineRect(HSRGN handle, RECTI* rect, void* param, int32_t combineMode);

// 524
void SRgnCreate(HSRGN* handlePtr, uint32_t reserved);

// 525
void SRgnDelete(HSRGN handle);

// 530
void SRgnGetBoundingRect(HSRGN handle, RECTI* rect);

// 531
int SRgnIsPointInRegion(HSRGN handle, int x, int y);

// 532
int SRgnIsRectInRegion(HSRGN handle, RECTI* rect);

// 534
void SRgnCombineRectf(HSRGN handle, RECTF* rect, void* param, int32_t combineMode);

// 537
void SRgnGetBoundingRectf(HSRGN handle, RECTF* rect);

// 538
int SRgnIsPointInRegionf(HSRGN handle, float x, float y);

// 539
int SRgnIsRectInRegionf(HSRGN handle, RECTF* rect);

#endif
