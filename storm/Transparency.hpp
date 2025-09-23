#ifndef STORM_TRANSPARENCY_HPP
#define STORM_TRANSPARENCY_HPP

#include <cstdint>
#include "Handle.hpp"

struct TRANS;
typedef TRANS* HSTRANS;


int32_t STransDelete(HSTRANS handle);

int32_t STransDestroy();

int32_t STransDuplicate(HSTRANS source, HSTRANS* handle);

int32_t STransSetDirtyArrayInfo(int32_t screencx, int32_t screency, int32_t cellcx, int32_t cellcy);

#endif
