#ifndef TEST_DISPLAY_PBM_H
#define TEST_DISPLAY_PBM_H

extern "C" {
#include "clib/u8g2.h"
}
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/**
 * U8g2 uses a tile-based buffer layout (8 rows per tile, column-major within
 * each tile row). PBM P4 expects row-major, MSB-first bit packing.
 */
static bool savePBM(u8g2_t* u8g2, const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;

    int w = u8g2_GetDisplayWidth(u8g2);
    int h = u8g2_GetDisplayHeight(u8g2);
    fprintf(f, "P4\n%d %d\n", w, h);

    uint8_t* buf = u8g2_GetBufferPtr(u8g2);
    int tileWidth = u8g2_GetBufferTileWidth(u8g2);

    uint8_t rowBuf[16];  // 128/8 = 16

    // U8g2 full-buffer layout: 8 tile rows, each tileWidth*8 bytes wide.
    // Each tile row = w bytes (one per column). Each byte holds 8 vertical
    // pixels, LSB = topmost pixel in the tile.
    int bytesPerTileRow = tileWidth * 8;

    for (int y = 0; y < h; y++) {
        memset(rowBuf, 0, sizeof(rowBuf));
        int tileRow = y / 8;
        int bitInTile = y % 8;

        for (int x = 0; x < w; x++) {
            int byteIdx = tileRow * bytesPerTileRow + x;
            bool pixel = (buf[byteIdx] >> bitInTile) & 1;
            if (pixel) {
                rowBuf[x / 8] |= (0x80 >> (x % 8));
            }
        }
        fwrite(rowBuf, 1, w / 8, f);
    }

    fclose(f);
    return true;
}

static bool bufferHasContent(u8g2_t* u8g2) {
    uint8_t* buf = u8g2_GetBufferPtr(u8g2);
    int bufSize = 8 * u8g2_GetBufferTileHeight(u8g2) *
                  u8g2_GetBufferTileWidth(u8g2);
    for (int i = 0; i < bufSize; i++) {
        if (buf[i] != 0) return true;
    }
    return false;
}

static void ensureDirRecursive(const char* dir) {
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s", dir);
    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
}

// No-op callbacks for native u8g2 usage (buffer-only, no real hardware)
static uint8_t u8x8_byte_noop(u8x8_t* /*u8x8*/, uint8_t /*msg*/,
                               uint8_t /*arg_int*/, void* /*arg_ptr*/) {
    return 1;
}

static uint8_t u8x8_gpio_and_delay_noop(u8x8_t* /*u8x8*/, uint8_t /*msg*/,
                                         uint8_t /*arg_int*/,
                                         void* /*arg_ptr*/) {
    return 1;
}

static void initTestDisplay(u8g2_t* u8g2) {
    u8g2_Setup_ssd1306_128x64_noname_f(u8g2, U8G2_R0, u8x8_byte_noop,
                                        u8x8_gpio_and_delay_noop);
    u8g2_InitDisplay(u8g2);
    u8g2_SetPowerSave(u8g2, 0);
    u8g2_ClearBuffer(u8g2);
}

#endif  // TEST_DISPLAY_PBM_H
