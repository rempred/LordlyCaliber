#include "game/combat_draw_commands.h"

typedef unsigned int u32;
#define FIELD COMBAT_DRAW_FIELD
#define PAIR COMBAT_DRAW_PAIR
#define LOAD_FLAGS (FIELD(7, 24, 3) | FIELD(cmt, 18, 2) | FIELD(maskt, 14, 4) | \
                    FIELD(shiftt, 10, 4) | FIELD(cms, 8, 2) | FIELD(masks, 4, 4) | FIELD(shifts, 0, 4))
#define RENDER_FLAGS (FIELD(tile, 24, 3) | FIELD(palette, 20, 4) | FIELD(cmt, 18, 2) | FIELD(maskt, 14, 4) | \
                    FIELD(shiftt, 10, 4) | FIELD(cms, 8, 2) | FIELD(masks, 4, 4) | FIELD(shifts, 0, 4))
#define COORD(x, y, shift, prefix) ((prefix) | FIELD((u32)(x) << (shift), 12, 12) | FIELD((u32)(y) << 2, 0, 12))
#define TEXTURE(imageWord, loadWord, renderWord, imageWidth, line, xshift) { \
    PAIR((imageWord) | (((imageWidth) - 1) & 0xFFF), (u32)image); \
    PAIR((loadWord) | (((line) & 0x1FF) << 9) | FIELD(textureMemory, 0, 9), LOAD_FLAGS); \
    PAIR(0xE6000000, 0); \
    PAIR(COORD(x0, y0, xshift, 0xF4000000), COORD(x1, y1, xshift, 0x07000000)); \
    PAIR(0xE7000000, 0); \
    PAIR((renderWord) | (((line) & 0x1FF) << 9) | FIELD(textureMemory, 0, 9), RENDER_FLAGS); \
    PAIR(COORD(x0, y0, 2, 0xF2000000), COORD(x1, y1, 2, FIELD(tile, 24, 3))); \
}

/* The height argument is an unused ABI slot. End coordinates are inclusive.
 * Signed shifts in the line calculations retain the original negative-input
 * rounding; coordinate packing uses unsigned shifts before masking.
 * Plain compound command macros and prefix-first field packing preserve the
 * compiler's command scheduling; do/while wrappers retain extra loop notes.
 */
void func_001FD56C(void *image, u32 textureMemory, u32 tile,
                   int type, int format, int width, int height,
                   int x0, int y0, int x1, int y1, u32 palette,
                   u32 cms, u32 cmt, u32 masks, u32 maskt,
                   u32 shifts, u32 shiftt)
{
    switch (type) {
    case 0:
        switch (format) {
        case 2:
            TEXTURE(0xFD100000, 0xF5100000, 0xF5100000, width,
                    (int)(((u32)x1 - (u32)x0) * 2 + 9) >> 3, 2);
            return;
        case 3:
            TEXTURE(0xFD180000, 0xF5180000, 0xF5180000, width,
                    (int)(((u32)x1 - (u32)x0) * 2 + 9) >> 3, 2);
            return;
        }
        return;
    case 3:
        switch (format) {
        case 0:
            TEXTURE(0xFD680000, 0xF5680000, 0xF5600000, width >> 1,
                    (((int)((u32)x1 - (u32)x0 + 1) >> 1) + 7) >> 3, 1);
            return;
        case 1:
            TEXTURE(0xFD680000, 0xF5680000, 0xF5680000, width,
                    (int)((u32)x1 - (u32)x0 + 8) >> 3, 2);
            return;
        case 2:
            TEXTURE(0xFD700000, 0xF5700000, 0xF5700000, width,
                    (int)(((u32)x1 - (u32)x0) * 2 + 9) >> 3, 2);
            return;
        }
        return;
    case 4:
        switch (format) {
        case 0:
            TEXTURE(0xFD880000, 0xF5880000, 0xF5800000, width >> 1,
                    (((int)((u32)x1 - (u32)x0 + 1) >> 1) + 7) >> 3, 1);
            return;
        case 1:
            TEXTURE(0xFD880000, 0xF5880000, 0xF5880000, width,
                    (int)((u32)x1 - (u32)x0 + 8) >> 3, 2);
            return;
        }
        return;
    case 2:
        switch (format) {
        case 0:
            TEXTURE(0xFD480000, 0xF5480000, 0xF5400000, width >> 1,
                    (((int)((u32)x1 - (u32)x0 + 1) >> 1) + 7) >> 3, 1);
            return;
        case 1:
            TEXTURE(0xFD480000, 0xF5480000, 0xF5480000, width,
                    (int)((u32)x1 - (u32)x0 + 8) >> 3, 2);
            return;
        }
        return;
    }
}
