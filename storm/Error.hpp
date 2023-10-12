#ifndef STORM_ERROR_HPP
#define STORM_ERROR_HPP

#include <cstdint>

#if defined(WHOA_SYSTEM_WIN)
#include <WinError.h>
#endif

#if defined(WHOA_SYSTEM_MAC) || defined(WHOA_SYSTEM_LINUX)
#define ERROR_SUCCESS           0x0
#define ERROR_INVALID_PARAMETER 0x57
#endif

#if defined(NDEBUG)
#define STORM_ASSERT(x)                          \
    (void)0
#else
#define STORM_ASSERT(x)                          \
    if (!(x)) {                                  \
        SErrPrepareAppFatal(__FILE__, __LINE__); \
        SErrDisplayAppFatal(#x);                 \
    }                                            \
    (void)0
#endif

#define STORM_VALIDATE(x, y, ...)                \
    if (!(x)) {                                  \
        SErrSetLastError(y);                     \
        return __VA_ARGS__;                      \
    }                                            \
    (void)0

// 461
int32_t SErrDisplayError(uint32_t errorcode, const char* filename, int32_t linenumber, const char* description, int32_t recoverable, uint32_t exitcode, uint32_t a7);

// 463
uint32_t SErrGetLastError();

// 465
void SErrSetLastError(uint32_t errorcode);

// 468
void SErrSuppressErrors(int suppress);

// 562
int32_t SErrDisplayErrorFmt(uint32_t errorcode, const char* filename, int32_t linenumber, int32_t recoverable, uint32_t exitcode, const char* format, ...);

// 563
int SErrIsDisplayingError();

// 564
void SErrPrepareAppFatal(const char* filename, int32_t linenumber);

// 566
[[noreturn]] void SErrDisplayAppFatal(const char* format, ...);

#endif
