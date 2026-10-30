#include "common/types.h"

extern u16 D_80220E70[15];
extern u8 D_801CEAA8;
extern u8 D_801CEAA9;
extern u8 D_801CEAAA;
extern u8 D_801CEAAB;

void func_002600E8(u16 mode, u16 *source)
{
    u16 *values = D_80220E70;
    u16 *record = values + 6;

    D_80220E70[11] = values[0];
    D_80220E70[12] = D_80220E70[1];
    D_80220E70[13] = D_80220E70[2];
    if (mode == 1) {
        D_80220E70[14] = D_80220E70[3];
    } else {
        switch (mode) {
        case 2:
            D_80220E70[14] = D_801CEAA8;
            break;
        case 3:
            D_80220E70[14] = D_801CEAA9;
            break;
        case 4:
            D_80220E70[14] = D_801CEAAA;
            break;
        case 5:
            D_80220E70[14] = D_801CEAAB;
            break;
        default:
            /* Retail loops at this path for selectors outside 1 through 5. */
            for (;;) {
            }
        }
    }
    record[0] = source[0];
    record[1] = source[1];
    record[2] = source[2];
    record[3] = source[3];
    record[4] = source[4];
    values[5] = 0;
    values[4] = mode;
}
