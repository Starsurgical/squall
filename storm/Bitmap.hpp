#ifndef STORM_BITMAP_HPP
#define STORM_BITMAP_HPP

#include <cstdint>


#define SBMP_IMAGETYPE_AUTO 0
#define SBMP_IMAGETYPE_BMP 1
#define SBMP_IMAGETYPE_PCX 2
#define SBMP_IMAGETYPE_TGA 3


struct STORM_PALETTEENTRY {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t flags;
};




#endif
