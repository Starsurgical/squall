#include "StormLib.h"
#include <cstdint>

int32_t StormLib_SCompCompress(void* dest, uint32_t* destsize, const void* source, uint32_t sourcesize, uint32_t compressiontypes, uint32_t hint, uint32_t optimization) {
    return SCompCompress(dest, reinterpret_cast<int*>(destsize), const_cast<void*>(source), sourcesize, compressiontypes, hint, optimization);
}

int32_t StormLib_SCompDecompress(void* dest, uint32_t* destsize, const void* source, uint32_t sourcesize) {
#if defined(WHOA_SCOMP_OLD)
    return SCompDecompress(dest, reinterpret_cast<int*>(destsize), const_cast<void*>(source), sourcesize);
#else
    return SCompDecompress2(dest, reinterpret_cast<int*>(destsize), const_cast<void*>(source), sourcesize);
#endif
}
