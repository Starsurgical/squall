#include "test/Test.hpp"
#include "storm/Bitmap.hpp"
#include <cstring>
#include <vector>
#include <sstream>

// Helper structures matching the internal PCX format
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

// Helper function to create a minimal valid PCX header
std::vector<uint8_t> createMinimalPcxFile(
    const uint16_t width,
    const uint16_t height,
    const uint8_t bitdepth,
    const uint8_t planes = 1,
    const bool withExtendedPalette = false
) {
    std::vector<uint8_t> data(sizeof(PCXFILEREC), 0);
    auto* pcx = reinterpret_cast<PCXFILEREC*>(data.data());

    // Set up header
    pcx->header.signature = 0x050A;  // PCX signature
    pcx->header.encoding = 1;        // RLE encoding
    pcx->header.bitsperpixel = bitdepth;
    pcx->header.x1 = 0;
    pcx->header.y1 = 0;
    pcx->header.x2 = width - 1;
    pcx->header.y2 = height - 1;
    pcx->header.screenwidth = width;
    pcx->header.screenheight = height;

    // Set up info
    pcx->info.mode = 0;
    pcx->info.planes = planes;
    pcx->info.bytesperline = (width * bitdepth + 7) / 8;

    // Add some dummy compressed data (minimal)
    data.push_back(0xC1);  // Run count of 1
    data.push_back(0x00);  // Value 0

    // Add extended palette if requested
    if (withExtendedPalette && bitdepth == 8) {
        const size_t paletteStart = data.size();
        data.resize(data.size() + sizeof(PCXEXTPALREC));
        auto* extPal = reinterpret_cast<PCXEXTPALREC*>(data.data() + paletteStart);
        extPal->number = 0x0C;  // Palette marker

        // Fill with some test palette data
        for (int i = 0; i < 256; i++) {
            extPal->pal256[i].red = i;
            extPal->pal256[i].green = 255 - i;
            extPal->pal256[i].blue = i / 2;
        }
    }

    return data;
}

TEST_CASE("SBmpAllocLoadImage", "[bitmap]") {
    SECTION("loads image from MPQ") {
    }

    SECTION("loads image from filesystem") {
    }

    SECTION("fails when image not found") {
    }

    SECTION("populates output args") {
    }

    SECTION("clears output args when format not supported") {
    }

    SECTION("calls custom allocation function when provided") {
    }
}

TEST_CASE("SBmpDecodeImage", "[bitmap]") {
    SECTION("does nothing for invalid format") {
        uint8_t data = 5;
        CHECK_FALSE(SBmpDecodeImage(99, &data, 1, nullptr, nullptr, 0));
    }

    SECTION("resets numeric output arguments") {
        uint8_t data = 5;

        uint32_t width = 100, height = 100, bitdepth = 100;
        CHECK_FALSE(SBmpDecodeImage(99, &data, 1, nullptr, nullptr, 0, &width, &height, &bitdepth));

        CHECK(width == 0);
        CHECK(height == 0);
        CHECK(bitdepth == 0);
    }

    SECTION("populates output args") {
    }

    SECTION("retrieves palette for 8 bit images") {
    }

    SECTION("ignores palette if bit depth is not 8") {
    }
}

TEST_CASE("SBmpDecodeImage - PCX specific tests", "[bitmap][pcx]") {
    SECTION("auto-detects PCX format from signature") {
        auto pcxData = createMinimalPcxFile(10, 10, 8);

        uint32_t width = 0, height = 0, bitdepth = 0;
        int32_t result = SBmpDecodeImage(SBMP_IMAGETYPE_AUTO, pcxData.data(), pcxData.size(),
                                         nullptr, nullptr, 0, &width, &height, &bitdepth);

        CHECK(result == 1);
        CHECK(width == 10);
        CHECK(height == 10);
        CHECK(bitdepth == 8);
    }

    SECTION("handles PCX with explicit type") {
        auto pcxData = createMinimalPcxFile(20, 15, 8);

        uint32_t width = 0, height = 0, bitdepth = 0;
        int32_t result = SBmpDecodeImage(SBMP_IMAGETYPE_PCX, pcxData.data(), pcxData.size(),
                                         nullptr, nullptr, 0, &width, &height, &bitdepth);

        CHECK(result == 1);
        CHECK(width == 20);
        CHECK(height == 15);
        CHECK(bitdepth == 8);
    }

    SECTION("handles different bit depths") {
        SECTION("1-bit monochrome") {
            auto pcxData = createMinimalPcxFile(100, 100, 1);
            uint32_t bitdepth = 0;
            SBmpDecodeImage(SBMP_IMAGETYPE_PCX, pcxData.data(), pcxData.size(),
                           nullptr, nullptr, 0, nullptr, nullptr, &bitdepth);
            CHECK(bitdepth == 1);
        }

        SECTION("4-bit (16 colors)") {
            auto pcxData = createMinimalPcxFile(100, 100, 4);
            uint32_t bitdepth = 0;
            SBmpDecodeImage(SBMP_IMAGETYPE_PCX, pcxData.data(), pcxData.size(),
                           nullptr, nullptr, 0, nullptr, nullptr, &bitdepth);
            CHECK(bitdepth == 4);
        }

        SECTION("8-bit (256 colors)") {
            auto pcxData = createMinimalPcxFile(100, 100, 8);
            uint32_t bitdepth = 0;
            SBmpDecodeImage(SBMP_IMAGETYPE_PCX, pcxData.data(), pcxData.size(),
                           nullptr, nullptr, 0, nullptr, nullptr, &bitdepth);
            CHECK(bitdepth == 8);
        }
    }

    SECTION("handles non-zero origin coordinates") {
        auto pcxData = createMinimalPcxFile(100, 100, 8);
        auto* pcx = reinterpret_cast<PCXFILEREC*>(pcxData.data());

        // Set origin to (10, 20)
        pcx->header.x1 = 10;
        pcx->header.y1 = 20;
        pcx->header.x2 = 109;  // width still 100
        pcx->header.y2 = 119;  // height still 100

        uint32_t width = 0, height = 0;
        SBmpDecodeImage(SBMP_IMAGETYPE_PCX, pcxData.data(), pcxData.size(),
                       nullptr, nullptr, 0, &width, &height, nullptr);

        CHECK(width == 100);
        CHECK(height == 100);
    }

    SECTION("PCX palette marker") {
        SECTION("extracts 256-color palette when present") {
            auto pcxData = createMinimalPcxFile(10, 10, 8, 1, true);

            STORM_PALETTEENTRY palette[256];
            std::memset(palette, 0xFF, sizeof(palette));

            int32_t result = SBmpDecodeImage(SBMP_IMAGETYPE_PCX, pcxData.data(), pcxData.size(),
                                             palette, nullptr, 0);

            CHECK(result == 1);

            // Verify palette was extracted correctly
            CHECK(palette[0].red == 0);
            CHECK(palette[0].green == 255);
            CHECK(palette[0].blue == 0);
            CHECK(palette[0].flags == 0);

            CHECK(palette[255].red == 255);
            CHECK(palette[255].green == 0);
            CHECK(palette[255].blue == 127);
            CHECK(palette[255].flags == 0);

            // Check a middle value
            CHECK(palette[128].red == 128);
            CHECK(palette[128].green == 127);
            CHECK(palette[128].blue == 64);
            CHECK(palette[128].flags == 0);
        }

        SECTION("detects valid palette marker 0x0C") {
            auto pcxData = createMinimalPcxFile(10, 10, 8, 1, true);

            // Verify the marker is set correctly
            size_t markerPos = pcxData.size() - sizeof(PCXEXTPALREC);
            CHECK(pcxData[markerPos] == 0x0C);

            STORM_PALETTEENTRY palette[256] = {};

            int32_t result = SBmpDecodeImage(SBMP_IMAGETYPE_PCX, pcxData.data(), pcxData.size(),
                                             palette, nullptr, 0);

            CHECK(result == 1);
            // Palette should be extracted
            CHECK(palette[1].red == 1);
        }

        SECTION("rejects invalid palette marker") {
            auto pcxData = createMinimalPcxFile(10, 10, 8, 1, true);

            // Corrupt the palette marker
            size_t markerPos = pcxData.size() - sizeof(PCXEXTPALREC);
            pcxData[markerPos] = 0xFF;  // Invalid marker

            STORM_PALETTEENTRY palette[256] = {};

            int32_t result = SBmpDecodeImage(SBMP_IMAGETYPE_PCX, pcxData.data(), pcxData.size(),
                                             palette, nullptr, 0);

            CHECK(result == 1);
            for (auto & i : palette) {
                // Palette should NOT be extracted (remains zero)
                CHECK(i.red == 0);
                CHECK(i.green == 0);
                CHECK(i.blue == 0);
            }
        }

        SECTION("does not extract palette when not present") {
            auto pcxData = createMinimalPcxFile(10, 10, 8, 1, false);

            STORM_PALETTEENTRY palette[256];
            std::memset(palette, 0xFF, sizeof(palette));

            int32_t result = SBmpDecodeImage(SBMP_IMAGETYPE_PCX, pcxData.data(), pcxData.size(),
                                             palette, nullptr, 0);

            CHECK(result == 1);

            // Palette should remain unchanged (all 0xFF)
            CHECK(palette[0].red == 0xFF);
            CHECK(palette[0].green == 0xFF);
            CHECK(palette[0].blue == 0xFF);
        }

        SECTION("does not extract palette for non-8-bit images") {
            auto pcxData = createMinimalPcxFile(10, 10, 4, 1, true);

            STORM_PALETTEENTRY palette[256];
            std::memset(palette, 0xFF, sizeof(palette));

            int32_t result = SBmpDecodeImage(SBMP_IMAGETYPE_PCX, pcxData.data(), pcxData.size(),
                                             palette, nullptr, 0);

            CHECK(result == 1);

            // Palette should remain unchanged
            CHECK(palette[0].red == 0xFF);
        }

        SECTION("handles null palette pointer") {
            auto pcxData = createMinimalPcxFile(10, 10, 8, 1, true);

            // Should not crash with null palette pointer
            int32_t result = SBmpDecodeImage(SBMP_IMAGETYPE_PCX, pcxData.data(), pcxData.size(),
                                             nullptr, nullptr, 0);

            CHECK(result == 1);
        }
    }

    SECTION("PCX file size validation") {
        SECTION("handles minimum valid PCX file") {
            auto pcxData = createMinimalPcxFile(1, 1, 1);

            uint32_t width = 0, height = 0;
            int32_t result = SBmpDecodeImage(SBMP_IMAGETYPE_PCX, pcxData.data(), pcxData.size(),
                                             nullptr, nullptr, 0, &width, &height, nullptr);

            CHECK(result == 1);
            CHECK(width == 1);
            CHECK(height == 1);
        }

        SECTION("handles file with extended palette") {
            auto pcxData = createMinimalPcxFile(10, 10, 8, 1, true);

            // File should be: header (128) + compressed data (2) + palette (769)
            CHECK(pcxData.size() == 128 + 2 + 769);

            uint32_t width = 0;
            int32_t result = SBmpDecodeImage(SBMP_IMAGETYPE_PCX, pcxData.data(), pcxData.size(),
                                             nullptr, nullptr, 0, &width, nullptr, nullptr);

            CHECK(result == 1);
            CHECK(width == 10);
        }

        SECTION("handles file without extended palette") {
            auto pcxData = createMinimalPcxFile(10, 10, 8, 1, false);

            // File should be: header (128) + compressed data (2)
            CHECK(pcxData.size() == 128 + 2);

            uint32_t width = 0;
            int32_t result = SBmpDecodeImage(SBMP_IMAGETYPE_PCX, pcxData.data(), pcxData.size(),
                                             nullptr, nullptr, 0, &width, nullptr, nullptr);

            CHECK(result == 1);
            CHECK(width == 10);
        }
    }

    SECTION("PCX multi-plane images") {
        SECTION("handles 3-plane 24-bit RGB image") {
            auto pcxData = createMinimalPcxFile(10, 10, 8, 3, false);

            uint32_t bitdepth = 0;
            int32_t result = SBmpDecodeImage(SBMP_IMAGETYPE_PCX, pcxData.data(), pcxData.size(),
                                             nullptr, nullptr, 0, nullptr, nullptr, &bitdepth);

            CHECK(result == 1);
            CHECK(bitdepth == 8);  // 8 bits per pixel per plane

            // Verify planes in the header
            auto* pcx = reinterpret_cast<PCXFILEREC*>(pcxData.data());
            CHECK(pcx->info.planes == 3);
        }

        SECTION("handles 4-plane EGA image") {
            auto pcxData = createMinimalPcxFile(10, 10, 1, 4, false);

            uint32_t bitdepth = 0;
            int32_t result = SBmpDecodeImage(SBMP_IMAGETYPE_PCX, pcxData.data(), pcxData.size(),
                                             nullptr, nullptr, 0, nullptr, nullptr, &bitdepth);

            CHECK(result == 1);
            CHECK(bitdepth == 1);

            auto* pcx = reinterpret_cast<PCXFILEREC*>(pcxData.data());
            CHECK(pcx->info.planes == 4);
        }
    }

    SECTION("PCX dimension tests") {
        SECTION("extracts correct dimensions from PCX header") {
            auto pcxData = createMinimalPcxFile(640, 480, 8);

            uint32_t width = 0, height = 0, bitdepth = 0;
            SBmpDecodeImage(SBMP_IMAGETYPE_PCX, pcxData.data(), pcxData.size(),
                           nullptr, nullptr, 0, &width, &height, &bitdepth);

            CHECK(width == 640);
            CHECK(height == 480);
            CHECK(bitdepth == 8);
        }

        SECTION("handles 1x1 image") {
            auto pcxData = createMinimalPcxFile(1, 1, 8);

            uint32_t width = 0, height = 0;
            SBmpDecodeImage(SBMP_IMAGETYPE_PCX, pcxData.data(), pcxData.size(),
                           nullptr, nullptr, 0, &width, &height, nullptr);

            CHECK(width == 1);
            CHECK(height == 1);
        }

        SECTION("handles very wide image") {
            auto pcxData = createMinimalPcxFile(65535, 1, 8);

            uint32_t width = 0, height = 0;
            SBmpDecodeImage(SBMP_IMAGETYPE_PCX, pcxData.data(), pcxData.size(),
                           nullptr, nullptr, 0, &width, &height, nullptr);

            CHECK(width == 65535);
            CHECK(height == 1);
        }

        SECTION("handles very tall image") {
            auto pcxData = createMinimalPcxFile(1, 65535, 8);

            uint32_t width = 0, height = 0;
            SBmpDecodeImage(SBMP_IMAGETYPE_PCX, pcxData.data(), pcxData.size(),
                           nullptr, nullptr, 0, &width, &height, nullptr);

            CHECK(width == 1);
            CHECK(height == 65535);
        }

        SECTION("handles large square image") {
            auto pcxData = createMinimalPcxFile(1024, 1024, 8);

            uint32_t width = 0, height = 0;
            SBmpDecodeImage(SBMP_IMAGETYPE_PCX, pcxData.data(), pcxData.size(),
                           nullptr, nullptr, 0, &width, &height, nullptr);

            CHECK(width == 1024);
            CHECK(height == 1024);
        }
    }

    SECTION("PCX bytesperline calculation") {
        SECTION("verifies bytesperline for 8-bit image") {
            auto pcxData = createMinimalPcxFile(100, 50, 8);

            const auto* pcx = reinterpret_cast<PCXFILEREC*>(pcxData.data());

            // For 100 pixels at 8 bits per pixel = 100 bytes per line
            CHECK(pcx->info.bytesperline == 100);
        }

        SECTION("verifies bytesperline for 1-bit image") {
            auto pcxData = createMinimalPcxFile(100, 50, 1);

            const auto* pcx = reinterpret_cast<PCXFILEREC*>(pcxData.data());

            // For 100 pixels at 1 bit per pixel = 13 bytes per line (rounded up)
            CHECK(pcx->info.bytesperline == 13);
        }

        SECTION("verifies bytesperline for 4-bit image") {
            auto pcxData = createMinimalPcxFile(100, 50, 4);

            const auto* pcx = reinterpret_cast<PCXFILEREC*>(pcxData.data());

            // For 100 pixels at 4 bits per pixel = 50 bytes per line
            CHECK(pcx->info.bytesperline == 50);
        }
    }

    SECTION("PCX RLE decompression", "[rle]") {
        SECTION("decompresses simple literal values") {
            // Create a PCX file with uncompressed data (all literals)
            auto pcxData = createMinimalPcxFile(4, 1, 8);

            // Replace compressed data with: 4 literal bytes
            pcxData.resize(sizeof(PCXFILEREC));
            pcxData.push_back(0x01);  // Literal value 1
            pcxData.push_back(0x02);  // Literal value 2
            pcxData.push_back(0x03);  // Literal value 3
            pcxData.push_back(0x04);  // Literal value 4

            uint8_t buffer[4] = {};
            int32_t result = SBmpDecodeImage(SBMP_IMAGETYPE_PCX, pcxData.data(), pcxData.size(),
                                             nullptr, buffer, sizeof(buffer));

            CHECK(result == 1);
            CHECK(buffer[0] == 0x01);
            CHECK(buffer[1] == 0x02);
            CHECK(buffer[2] == 0x03);
            CHECK(buffer[3] == 0x04);
        }

        SECTION("decompresses run-length encoded data") {
            // Create a PCX file with RLE data
            auto pcxData = createMinimalPcxFile(8, 1, 8);

            // Replace compressed data with: run of 8 zeros
            pcxData.resize(sizeof(PCXFILEREC));
            pcxData.push_back(0xC8);  // Run count of 8 (0xC0 | 8)
            pcxData.push_back(0xAA);  // Value 0xAA

            uint8_t buffer[8] = {};
            int32_t result = SBmpDecodeImage(SBMP_IMAGETYPE_PCX, pcxData.data(), pcxData.size(),
                                             nullptr, buffer, sizeof(buffer));

            CHECK(result == 1);
            for (unsigned char i : buffer) {
                CHECK(i == 0xAA);
            }
        }

        SECTION("decompresses mixed literal and RLE data") {
            auto pcxData = createMinimalPcxFile(10, 1, 8);

            // Mixed data: 2 literals, run of 5, 3 literals
            pcxData.resize(sizeof(PCXFILEREC));
            pcxData.push_back(0x11);  // Literal
            pcxData.push_back(0x22);  // Literal
            pcxData.push_back(0xC5);  // Run count of 5
            pcxData.push_back(0xFF);  // Value 0xFF
            pcxData.push_back(0x33);  // Literal
            pcxData.push_back(0x44);  // Literal
            pcxData.push_back(0x55);  // Literal

            uint8_t buffer[10] = {};
            int32_t result = SBmpDecodeImage(SBMP_IMAGETYPE_PCX, pcxData.data(), pcxData.size(),
                                             nullptr, buffer, sizeof(buffer));

            CHECK(result == 1);
            CHECK(buffer[0] == 0x11);
            CHECK(buffer[1] == 0x22);
            CHECK(buffer[2] == 0xFF);
            CHECK(buffer[3] == 0xFF);
            CHECK(buffer[4] == 0xFF);
            CHECK(buffer[5] == 0xFF);
            CHECK(buffer[6] == 0xFF);
            CHECK(buffer[7] == 0x33);
            CHECK(buffer[8] == 0x44);
            CHECK(buffer[9] == 0x55);
        }

        SECTION("handles maximum run count (63)") {
            auto pcxData = createMinimalPcxFile(63, 1, 8);

            // Maximum run count: 0xC0 | 0x3F = 0xFF
            pcxData.resize(sizeof(PCXFILEREC));
            pcxData.push_back(0xFF);  // Run count of 63
            pcxData.push_back(0x71);  // Value 0x71

            uint8_t buffer[63] = {};
            int32_t result = SBmpDecodeImage(SBMP_IMAGETYPE_PCX, pcxData.data(), pcxData.size(),
                                             nullptr, buffer, sizeof(buffer));

            CHECK(result == 1);
            for (unsigned char i : buffer) {
                CHECK(i == 0x71);
            }
        }

        SECTION("handles minimum run count (1)") {
            auto pcxData = createMinimalPcxFile(1, 1, 8);

            // Minimum run count: 0xC0 | 0x01 = 0xC1
            pcxData.resize(sizeof(PCXFILEREC));
            pcxData.push_back(0xC1);  // Run count of 1
            pcxData.push_back(0x71);  // Value 0x71

            uint8_t buffer[1] = {};
            int32_t result = SBmpDecodeImage(SBMP_IMAGETYPE_PCX, pcxData.data(), pcxData.size(),
                                             nullptr, buffer, sizeof(buffer));

            CHECK(result == 1);
            CHECK(buffer[0] == 0x71);
        }

        SECTION("handles values >= 0xC0 as literals") {
            // Values 0xC0-0xFF need special handling
            // If they appear as literals, they must be encoded as run-count-1
            auto pcxData = createMinimalPcxFile(2, 1, 8);

            pcxData.resize(sizeof(PCXFILEREC));
            pcxData.push_back(0xC1);  // Run count of 1
            pcxData.push_back(0xC0);  // Value 0xC0 (encoded)
            pcxData.push_back(0xC1);  // Run count of 1
            pcxData.push_back(0xFF);  // Value 0xFF (encoded)

            uint8_t buffer[2] = {};
            int32_t result = SBmpDecodeImage(SBMP_IMAGETYPE_PCX, pcxData.data(), pcxData.size(),
                                             nullptr, buffer, sizeof(buffer));

            CHECK(result == 1);
            CHECK(buffer[0] == 0xC0);
            CHECK(buffer[1] == 0xFF);
        }

        SECTION("decompresses multi-line image") {
            // 4x3 image (4 pixels wide, 3 pixels tall)
            auto pcxData = createMinimalPcxFile(4, 3, 8);

            pcxData.resize(sizeof(PCXFILEREC));
            // Line 1: 4 pixels
            pcxData.push_back(0xC4);
            pcxData.push_back(0x11);
            // Line 2: 4 pixels
            pcxData.push_back(0xC4);
            pcxData.push_back(0x22);
            // Line 3: 4 pixels
            pcxData.push_back(0xC4);
            pcxData.push_back(0x33);

            uint8_t buffer[12] = {};
            int32_t result = SBmpDecodeImage(SBMP_IMAGETYPE_PCX, pcxData.data(), pcxData.size(),
                                             nullptr, buffer, sizeof(buffer));

            CHECK(result == 1);
            // Line 1
            CHECK(buffer[0] == 0x11);
            CHECK(buffer[1] == 0x11);
            CHECK(buffer[2] == 0x11);
            CHECK(buffer[3] == 0x11);
            // Line 2
            CHECK(buffer[4] == 0x22);
            CHECK(buffer[5] == 0x22);
            CHECK(buffer[6] == 0x22);
            CHECK(buffer[7] == 0x22);
            // Line 3
            CHECK(buffer[8] == 0x33);
            CHECK(buffer[9] == 0x33);
            CHECK(buffer[10] == 0x33);
            CHECK(buffer[11] == 0x33);
        }

        SECTION("decompresses 8-bit image with extended palette") {
            // This tests if (hasExtendedPalette) { extra = sizeof(PCXEXTPALREC) }
            // We need to decompress an image that has an extended palette
            auto pcxData = createMinimalPcxFile(4, 2, 8, 1, true);

            // Replace compressed data (between header and palette)
            // Find where compressed data should go
            pcxData.resize(sizeof(PCXFILEREC));

            // Add compressed data for 4x2 = 8 pixels
            pcxData.push_back(0xC4);  // Run of 4
            pcxData.push_back(0xAA);  // Value 0xAA (line 1)
            pcxData.push_back(0xC4);  // Run of 4
            pcxData.push_back(0xBB);  // Value 0xBB (line 2)

            // Now add the extended palette
            size_t paletteStart = pcxData.size();
            pcxData.resize(paletteStart + sizeof(PCXEXTPALREC));
            auto* extPal = reinterpret_cast<PCXEXTPALREC*>(pcxData.data() + paletteStart);
            extPal->number = 0x0C;  // Palette marker

            // Fill with test palette data
            for (int i = 0; i < 256; i++) {
                extPal->pal256[i].red = i;
                extPal->pal256[i].green = 255 - i;
                extPal->pal256[i].blue = i / 2;
            }

            // Decompress with both palette and bitmap buffers
            STORM_PALETTEENTRY palette[256] = {};
            uint8_t buffer[8] = {};

            int32_t result = SBmpDecodeImage(SBMP_IMAGETYPE_PCX, pcxData.data(), pcxData.size(),
                                             palette, buffer, sizeof(buffer));

            CHECK(result == 1);

            // Verify decompressed image data
            CHECK(buffer[0] == 0xAA);
            CHECK(buffer[1] == 0xAA);
            CHECK(buffer[2] == 0xAA);
            CHECK(buffer[3] == 0xAA);
            CHECK(buffer[4] == 0xBB);
            CHECK(buffer[5] == 0xBB);
            CHECK(buffer[6] == 0xBB);
            CHECK(buffer[7] == 0xBB);

            // Verify palette was also extracted
            CHECK(palette[0].red == 0);
            CHECK(palette[0].green == 255);
            CHECK(palette[255].red == 255);
            CHECK(palette[255].green == 0);
        }
    }

    SECTION("PCX RLE edge cases", "[rle]") {
        SECTION("handles running out of source data mid-scanline") {
            // This tests the branch where sourceIndex >= sourcebytes in the inner loop of UncompressPcxImage
            // We create an image expecting 8 bytes but only provide 5 bytes of compressed data
            auto pcxData = createMinimalPcxFile(8, 1, 8);

            pcxData.resize(sizeof(PCXFILEREC));
            pcxData.push_back(0x11);  // Literal
            pcxData.push_back(0x22);  // Literal
            pcxData.push_back(0xC2);  // Run count of 2
            pcxData.push_back(0x33);  // Value 0x33
            pcxData.push_back(0x44);  // Literal
            // Source data ends here (5 bytes total, but scan line needs 8 bytes)

            uint8_t buffer[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
            int32_t result = SBmpDecodeImage(SBMP_IMAGETYPE_PCX, pcxData.data(), pcxData.size(),
                                             nullptr, buffer, sizeof(buffer));

            CHECK(result == 1);
            // Should have decoded the 5 bytes we provided
            CHECK(buffer[0] == 0x11);
            CHECK(buffer[1] == 0x22);
            CHECK(buffer[2] == 0x33);
            CHECK(buffer[3] == 0x33);
            CHECK(buffer[4] == 0x44);
            // Rest should remain unchanged (0xFF) because we ran out of source data
            CHECK(buffer[5] == 0xFF);
            CHECK(buffer[6] == 0xFF);
            CHECK(buffer[7] == 0xFF);
        }

        SECTION("handles buffer smaller than image") {
            auto pcxData = createMinimalPcxFile(10, 1, 8);

            pcxData.resize(sizeof(PCXFILEREC));
            pcxData.push_back(0xCA);  // Run count of 10
            pcxData.push_back(0x77);  // Value 0x77

            // Buffer only has space for 5 bytes
            uint8_t buffer[5] = {};
            int32_t result = SBmpDecodeImage(SBMP_IMAGETYPE_PCX, pcxData.data(), pcxData.size(),
                                             nullptr, buffer, 5);

            CHECK(result == 1);
            // Should only fill what fits
            for (unsigned char i : buffer) {
                CHECK(i == 0x77);
            }
        }

        SECTION("handles empty compressed data gracefully") {
            auto pcxData = createMinimalPcxFile(4, 1, 8);
            pcxData.resize(sizeof(PCXFILEREC));  // No compressed data

            uint8_t buffer[4] = {0xFF, 0xFF, 0xFF, 0xFF};
            int32_t result = SBmpDecodeImage(SBMP_IMAGETYPE_PCX, pcxData.data(), pcxData.size(),
                                             nullptr, buffer, sizeof(buffer));

            CHECK(result == 1);
            // Buffer should remain mostly unchanged (incomplete decompression)
        }

        SECTION("handles incomplete run code at end") {
            auto pcxData = createMinimalPcxFile(4, 1, 8);

            pcxData.resize(sizeof(PCXFILEREC));
            pcxData.push_back(0x01);
            pcxData.push_back(0x02);
            pcxData.push_back(0xC4);  // Run count, but no value byte follows

            uint8_t buffer[4] = {};
            int32_t result = SBmpDecodeImage(SBMP_IMAGETYPE_PCX, pcxData.data(), pcxData.size(),
                                             nullptr, buffer, sizeof(buffer));

            CHECK(result == 1);
            CHECK(buffer[0] == 0x01);
            CHECK(buffer[1] == 0x02);
            // Rest should be zero (incomplete)
        }
    }

    SECTION("handles null dimension pointers") {
        auto pcxData = createMinimalPcxFile(10, 10, 8);

        // Should not crash with null dimension pointers
        int32_t result = SBmpDecodeImage(SBMP_IMAGETYPE_PCX, pcxData.data(), pcxData.size(),
                                         nullptr, nullptr, 0, nullptr, nullptr, nullptr);

        CHECK(result == 1);
    }
}

TEST_CASE("SBmpLoadImage", "[bitmap]") {
    SECTION("loads image from MPQ") {
    }

    SECTION("loads image from filesystem") {
    }

    SECTION("fails when image not found") {
    }

    SECTION("populates output args") {
    }

    SECTION("clears output args when format not supported") {
    }

    SECTION("retrieves args even if buffer unspecified") {
    }
}

struct AlignTestCase {
    int32_t width, align, result;
};

std::string GetAlignTestcaseStr(const AlignTestCase &input) {
    std::ostringstream ss;
    ss << "SBmpGetPitchForAlignment(" << input.width << ", " << input.align << ")";
    return ss.str();
}

#if !defined(WHOA_STORMDLL_VERSION) || WHOA_STORMDLL_VERSION >= 2016
TEST_CASE("SBmpGetPitchForAlignment", "[bitmap]") {
    SECTION("returns width if divisible by alignment") {
        auto input = GENERATE(
            AlignTestCase { 0, 1, 0 },
            AlignTestCase { 1, 1, 1 },
            AlignTestCase { 2, 1, 2 },
            AlignTestCase { 99, 1, 99 },
            AlignTestCase { -1, 1, -1 },
            AlignTestCase { 2, 2, 2 },
            AlignTestCase { -20, 4, -20 },
            AlignTestCase { 12, 4, 12 },
            AlignTestCase { 16, 8, 16 }
        );
        INFO(GetAlignTestcaseStr(input));
        CHECK(SBmpGetPitchForAlignment(input.width, input.align) == input.result);
    }

    SECTION("returns width rounded up to alignment") {
        auto input = GENERATE(
            AlignTestCase { 1, 2, 2 },
            AlignTestCase { 1, 4, 4 },
            AlignTestCase { -1, 4, 4 },
            AlignTestCase { -3, 4, 4 },
            AlignTestCase { 18, 4, 20 },
            AlignTestCase { 16, 5, 20 },
            AlignTestCase { -53, 100, 100 },
            AlignTestCase { 152, 100, 200 }
        );
        INFO(GetAlignTestcaseStr(input));
        CHECK(SBmpGetPitchForAlignment(input.width, input.align) == input.result);
    }
}

TEST_CASE("SBmpPadImage", "[bitmap]") {
    SECTION("does nothing if width is aligned") {
    }

    SECTION("pads rows if width is not aligned") {
    }
}
#endif
