#ifndef STORM_MEMORY_HPP
#define STORM_MEMORY_HPP

#include <cstdint>
#include <cstdlib>

// 401
void* SMemAlloc(size_t bytes, const char* filename, int32_t linenumber, uint32_t flags);

void SMemFree(void* ptr);

// 403
void SMemFree(void* ptr, const char* filename, int32_t linenumber, uint32_t flags);

// 405
void* SMemReAlloc(void* ptr, size_t bytes, const char* filename, int32_t linenumber, uint32_t flags);

// 491
void SMemCopy(void* dest, const void* source, size_t size);

// 492
void SMemFill(void* location, size_t length, uint8_t fillwith);

// 493
void SMemMove(void* dest, const void* source, size_t size);

// 494
void SMemZero(void* location, size_t length);


#endif
