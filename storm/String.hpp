#ifndef STORM_STRING_HPP
#define STORM_STRING_HPP

#include <cstdint>
#include <cstdlib>

#define STORM_MAX_PATH 260
#define STORM_MAX_STR 0x7FFFFFFF

// 501
size_t SStrCopy(char* dest, const char* source, size_t destsize);

// 503
uint32_t SStrPack(char* dest, const char* source, uint32_t destsize);

// 504
void SStrTokenize(const char** string, char* buffer, size_t bufferchars, const char* whitespace, int32_t* quoted);

// 506
size_t SStrLen(const char* string);

// 507 (shouldn't have A?)
char* SStrDupA(const char* string, const char* filename, uint32_t linenumber);

// 508
int32_t SStrCmp(const char* string1, const char* string2, size_t maxchars);

// 509
int32_t SStrCmpI(const char* string1, const char* string2, size_t maxchars);

// 510
void SStrUpper(char* string);

// 505, 569
char* SStrChr(char* string, char search);

// 570
char* SStrChrR(char* string, char search);

// 571
const char* SStrChr(const char* string, char search);

// 572
const char* SStrChrR(const char* string, char search);

// 574
float SStrToFloat(const char* string);

// 575
int32_t SStrToInt(const char* string);

// 578
size_t SStrPrintf(char* dest, size_t maxchars, const char* format, ...);

// 579
void SStrLower(char* string);

// 581
size_t SStrVPrintf(char* dest, size_t maxchars, const char* format, va_list args);

// 584, 585
const char* SStrStr(const char* string, const char* search);

// 586, 587
const char* SStrStrI(const char* string, const char* search);

// 595
uint32_t SStrHashHT(const char* string);

#endif
