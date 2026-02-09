#include "Bitmap.hpp"
#include "Error.hpp"
#include "Memory.hpp"


#define BMP_SIGNATURE 0x4D42
#define PCX_SIGNATURE 0x050A


struct PCXHEADERREC {
    uint16_t signature;
    uint8_t encoding;
    uint8_t bitsperpixel;
    uint16_t x1;
    uint16_t y1;
    uint16_t x2;
    uint16_t y2;
    uint16_t screenwidth;
    uint16_t screenheight;
};

struct PCXINFOREC {
    uint8_t mode;
    uint8_t planes;
    uint16_t bytesperline;
    uint8_t unused[60];
};

struct PCXRGBREC {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

struct PCXFILEREC {
    PCXHEADERREC header;
    PCXRGBREC pal16[16];
    PCXINFOREC info;
};

struct PCXEXTPALREC {
    uint8_t number;
    PCXRGBREC pal256[256];
};

static_assert(sizeof(PCXHEADERREC) == 16, "PCXHEADERREC size");
static_assert(sizeof(PCXINFOREC) == 64, "PCXINFOREC size");
static_assert(sizeof(PCXRGBREC) == 3, "PCXRGBREC size");
static_assert(sizeof(PCXFILEREC) == 128, "PCXFILEREC size");
static_assert(sizeof(PCXEXTPALREC) == 0x301, "PCXEXTPALREC size");


int DetermineImageType(uint16_t signature) {
    switch(signature) {
    case BMP_SIGNATURE:
        return SBMP_IMAGETYPE_BMP;
    case PCX_SIGNATURE:
        return SBMP_IMAGETYPE_PCX;
    default:
        return SBMP_IMAGETYPE_TGA;
    }
}

/*
// only needed for SBmpSaveImageEx
const char* FindExtension(const char* filename) {
    while(SStrChr(filename, '\\')) {
        filename = SStrChr(filename, '\\') + 1;
    }
    while (SStrChr(filename + 1, '.')) {
        filename = SStrChr(filename + 1, '.');
    }
    return filename;
}
*/

/*
// Whoops, don't need this yet unless implementing BMP
void FlipImage(uint8_t* dest, uint8_t* source, int destbytes, int destwidth, int sourcewidth, int height) {
    STORM_VALIDATE_BEGIN;
    STORM_VALIDATE(dest);
    STORM_VALIDATE(source);
    STORM_VALIDATE(destbytes);
    STORM_VALIDATE(destwidth);
    STORM_VALIDATE(sourcewidth);
    STORM_VALIDATE(height);
    STORM_VALIDATE_END_VOID;

    source = &source[sourcewidth * (height - 1)];
    uint8_t* destterm = &dest[destbytes];

    while(height--) {
        size_t len = destwidth >= destterm - dest ? destterm - dest : destwidth;
        if (len == 0) {
            break;
        }

        SMemCopy(dest, source, len);
        dest += len;
        source -= sourcewidth;
    }
}
*/

void UncompressPcxImage(
    uint8_t* dest,
    const uint8_t* source,
    const int32_t destbytes,
    const int32_t sourcebytes,
    const int32_t bytesperline,
    const int32_t planes
) {
    // PCX uses a simple RLE compression scheme:
    // - If byte has top 2 bits set (0xC0), it's a run-count byte
    //   - Lower 6 bits = run count (1-63)
    //   - Next byte = value to repeat
    // - Otherwise, it's a literal value (run count = 1)
    //
    // Important: Decoding breaks at the end of each scan line
    // Each scan line has 'bytesperline' bytes per plane, with 'planes' planes

    int32_t sourceIndex = 0;
    int32_t destIndex = 0;
    const int32_t scanLineLength = bytesperline * planes;

    // Process the image scan line by scan line
    while (destIndex < destbytes && sourceIndex < sourcebytes) {
        const int32_t scanLineStart = destIndex;
        int32_t scanLineEnd = scanLineStart + scanLineLength;

        // Clamp to destination buffer size
        if (scanLineEnd > destbytes) {
            scanLineEnd = destbytes;
        }

        // Decode one scan line
        while (destIndex < scanLineEnd && sourceIndex < sourcebytes) {
            const uint8_t byte = source[sourceIndex++];

            if ((byte & 0xC0) == 0xC0) {
                // Two-byte run code
                // Lower 6 bits contain the run count
                const int32_t runCount = byte & 0x3F;

                // Check if we have the run value byte
                if (sourceIndex >= sourcebytes) {
                    // Incomplete run code at end of data
                    break;
                }

                const uint8_t runValue = source[sourceIndex++];

                // Write the run, but don't exceed the scan line boundary
                for (int32_t i = 0; i < runCount && destIndex < scanLineEnd; i++) {
                    dest[destIndex++] = runValue;
                }
            } else {
                // One-byte literal value
                dest[destIndex++] = byte;
            }
        }

        // If we didn't fill the entire scan line, we've run out of source data
        // This is an error condition, but we'll just stop
        if (destIndex < scanLineEnd) {
            break;
        }
    }
}

/*
// Requires SFile
int32_t DecodePcxFile(HSFILE file, STORM_PALETTEENTRY* paletteentries, uint8_t* bitmapbits, uint32_t buffersize, uint32_t* width, uint32_t* height, uint32_t* bitdepth) {
    PCXFILEREC rec;
    if (!SFileReadFile(file, &rec, sizeof(rec), 0, 0)) {
        return 0;
    }

    uint32_t imagewidth = rec.header.x2 - rec.header.x1 + 1;
    uint32_t imageheight = rec.header.y2 - rec.header.y1 + 1;
    uint32_t imagebitdepth = rec.header.bitsperpixel;

    if (width) *width = imagewidth;
    if (height) *height = imageheight;
    if (bitdepth) *bitdepth = imagebitdepth;

    if (bitmapbits) {
        uint32_t filesize = SFileGetFileSize(file, 0);

        uint32_t extra = 0;
        if (imagebitdepth == 8) {
            extra = sizeof(PCXEXTPALREC);
        }

        uint32_t bytestoread = filesize - extra - SFileSetFilePointer(file, 0, 0, 1);
        uint8_t* compressed = static_cast<uint8_t*>(STORM_ALLOC(bytestoread));
        SFileReadFile(file, compressed, bytestoread, 0, 0);
        UncompressPcxImage(bitmapbits, compressed, buffersize, imagebitdepth == 8 ? bytestoread - 1 : bytestoread, imagewidth, rec.info.bytesperline, rec.info.planes);
        SMemFree(compressed, __FILE__, __LINE__);
    }
    else if (paletteentries && imagebitdepth == 8) {
        SFileSetFilePointer(file, -sizeof(PCXEXTPALREC) + 1, 0, 2);
    }

    if (paletteentries && imagebitdepth == 8) {
        PCXRGBREC paldata[256];
        SFileReadFile(file, &paldata, sizeof(paldata), 0, 0);
        for (int i = 0; i < 256; i++) {
            paletteentries[i].red = paldata[i].red;
            paletteentries[i].green = paldata[i].green;
            paletteentries[i].blue = paldata[i].blue;
            paletteentries[i].flags = 0;
        }
    }
    return 1;
}
*/

int32_t DecodePcxMem(PCXFILEREC *imagedata, int32_t imagebytes, STORM_PALETTEENTRY* paletteentries, uint8_t* bitmapbits, uint32_t buffersize, uint32_t* width, uint32_t* height, uint32_t* bitdepth) {
    uint32_t imagewidth = imagedata->header.x2 - imagedata->header.x1 + 1;
    uint32_t imageheight = imagedata->header.y2 - imagedata->header.y1 + 1;
    uint32_t imagebitdepth = imagedata->header.bitsperpixel;

    // Check if extended 256-color palette is present
    // According to PCX spec: 8-bit images may have a 769-byte palette at the end
    // The palette starts with a marker byte 0x0C, followed by 768 bytes (256 RGB triples)
    bool hasExtendedPalette = paletteentries &&
            imagebitdepth == 8 &&
            imagebytes >= sizeof(PCXFILEREC) + sizeof(PCXEXTPALREC);

    if (hasExtendedPalette) {
        // Check the palette marker byte (should be 0x0C)
        uint8_t* paletteMarker = reinterpret_cast<uint8_t*>(imagedata) + imagebytes - sizeof(PCXEXTPALREC);
        hasExtendedPalette = *paletteMarker == 0x0C;
    }

    if (width) *width = imagewidth;
    if (height) *height = imageheight;
    if (bitdepth) *bitdepth = imagebitdepth;

    if (bitmapbits) {
        uint32_t destbytes = imagewidth * imageheight * ((imagebitdepth + 7) / 8);
        if (buffersize < destbytes) {
            destbytes = buffersize;
        }

        uint32_t extra = 0;
        if (hasExtendedPalette) {
            extra = sizeof(PCXEXTPALREC);
        }

        UncompressPcxImage(
            bitmapbits,
            reinterpret_cast<uint8_t*>(imagedata) + sizeof(PCXFILEREC),
            destbytes,
            imagebytes - extra - sizeof(PCXFILEREC),
            imagedata->info.bytesperline,
            imagedata->info.planes);
    }

    if (hasExtendedPalette) {
        PCXRGBREC* paldata = reinterpret_cast<PCXEXTPALREC*>(reinterpret_cast<uint8_t*>(imagedata) + imagebytes - sizeof(PCXEXTPALREC))->pal256;
        for (int i = 0; i < 256; i++) {
            paletteentries[i].red = paldata[i].red;
            paletteentries[i].green = paldata[i].green;
            paletteentries[i].blue = paldata[i].blue;
            paletteentries[i].flags = 0;
        }
    }

    return 1;
}

int32_t STORMAPI SBmpAllocLoadImage(const char* filename, STORM_PALETTEENTRY* paletteentries, uint8_t** returnedbuffer, uint32_t* width, uint32_t* height, uint32_t* bitdepth, uint32_t requestedbitdepth, SBMPALLOCPROC allocproc) {
    if (returnedbuffer) *returnedbuffer = nullptr;
    if (width) *width = 0;
    if (height) *height = 0;
    if (bitdepth) *bitdepth = 0;

    STORM_VALIDATE_BEGIN;
    STORM_VALIDATE(filename);
    STORM_VALIDATE(*filename);
    STORM_VALIDATE(returnedbuffer);
    STORM_VALIDATE_END;

    // TODO requires SFile
    return 0;
}

int32_t STORMAPI SBmpDecodeImage(int32_t imagetype, uint8_t* imagedata, uint32_t imagebytes, STORM_PALETTEENTRY* paletteentries, uint8_t* bitmapbits, uint32_t buffersize, uint32_t* width, uint32_t* height, uint32_t* bitdepth) {
    if (width) *width = 0;
    if (height) *height = 0;
    if (bitdepth) *bitdepth = 0;

    STORM_VALIDATE_BEGIN;
    STORM_VALIDATE(imagedata);
    STORM_VALIDATE(imagebytes);
    STORM_VALIDATE(buffersize || !bitmapbits);
    STORM_VALIDATE_END;

    if (imagetype == SBMP_IMAGETYPE_AUTO && imagebytes >= 2) {
        imagetype = DetermineImageType(*reinterpret_cast<uint16_t*>(imagedata));
    }

    switch (imagetype) {
        case SBMP_IMAGETYPE_BMP:
            // TODO (not used by Starcraft)
            break;
        case SBMP_IMAGETYPE_PCX:
            return DecodePcxMem(reinterpret_cast<PCXFILEREC*>(imagedata), imagebytes, paletteentries, bitmapbits, buffersize, width, height, bitdepth);
        case SBMP_IMAGETYPE_TGA:
            // TODO (not used by Starcraft)
            break;
    }
    return 0;
}

int32_t STORMAPI SBmpGetPitchForAlignment(int32_t width, int32_t align) {
    if (width % align == 0) {
        return width;
    }

    int32_t padding = align - width % align;
    return width + padding;
}

int32_t STORMAPI SBmpLoadImage(const char* filename, STORM_PALETTEENTRY* paletteentries, uint8_t* bitmapbits, uint32_t buffersize, uint32_t* width, uint32_t* height, uint32_t* bitdepth) {
    if (width) *width = 0;
    if (height) *height = 0;
    if (bitdepth) *bitdepth = 0;

    STORM_VALIDATE_BEGIN;
    STORM_VALIDATE(filename);
    STORM_VALIDATE(*filename);
    STORM_VALIDATE(buffersize || !bitmapbits);
    STORM_VALIDATE_END;

    // TODO requires SFile
    return 0;
}

int32_t STORMAPI SBmpPadImage(uint8_t* imagedata, uint32_t imagebytes, int32_t width, int32_t height, int32_t alignment, int32_t* out_newwidth) {
    int32_t newwidth = SBmpGetPitchForAlignment(width, alignment);
    if (newwidth == width) {
        return 1;
    }

    uint8_t* newptr = imagedata;
    uint8_t* oldptr = &newptr[newwidth * height - width * height];
    SMemMove(oldptr, newptr, width * height);

    for (int32_t i = 0; i < height; i++) {
        SMemMove(newptr, oldptr, width);
        SMemZero(&newptr[width], newwidth - width);

        oldptr += width;
        newptr += newwidth;
    }

    if (out_newwidth) {
        *out_newwidth = newwidth;
    }
    return 1;
}
