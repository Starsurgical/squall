#ifndef STORM_COMP_HPP
#define STORM_COMP_HPP

#include "storm/Core.hpp"
#include <cstdint>


int32_t STORMAPI SCompCompress(void* dest, uint32_t* destsize, const void* source, uint32_t sourcesize, uint32_t compressiontypes, uint32_t hint, uint32_t optimization);

int32_t STORMAPI SCompDecompress(void* dest, uint32_t* destsize, const void* source, uint32_t sourcesize);


#endif
