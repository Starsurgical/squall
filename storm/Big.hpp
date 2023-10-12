#ifndef STORM_BIG_HPP
#define STORM_BIG_HPP

#include "storm/big/BigData.hpp"
#include <cstdint>

// 601
void SBigAdd(BigData* a, BigData* b, BigData* c);

void SBigBitLen(BigData* num, uint32_t* len);

// 603
int32_t SBigCompare(BigData* a, BigData* b);

// 606
void SBigDel(BigData* num);

// 607
void SBigDiv(BigData* a, BigData* b, BigData* c);

// 609
void SBigFromBinary(BigData* num, const void* data, uint32_t bytes);

// 612
void SBigFromUnsigned(BigData* num, uint32_t val);

// 614
void SBigInc(BigData* a, BigData* b);

// 616
int SBigIsEven(BigData* num);

// 617
int SBigIsOdd(BigData* num);

// 618
int SBigIsOne(BigData* num);

// 620
int SBigIsZero(BigData* num);

// 621
void SBigMod(BigData* a, BigData* b, BigData* c);

// 622
void SBigMul(BigData* a, BigData* b, BigData* c);

// 623
void SBigMulMod(BigData* a, BigData* b, BigData* c, BigData* d);

// 624
void SBigNew(BigData** num);

// 628
void SBigPowMod(BigData* a, BigData* b, BigData* c, BigData* d);

// 631
void SBigSetOne(BigData* num);

// 632
void SBigSetZero(BigData* num);

// 633
void SBigShl(BigData* a, BigData* b, uint32_t shift);

// 634
void SBigShr(BigData* a, BigData* b, uint32_t shift);

// 635
void SBigSquare(BigData* a, BigData* b);

// 636
void SBigSub(BigData* a, BigData* b, BigData* c);

// 638
void SBigToBinaryBuffer(BigData* num, uint8_t* data, uint32_t maxBytes, uint32_t* bytes);

#endif
