#include "common/types.h"

extern u16 D_80220E70[15];
extern u8 D_801CEAA8;
extern u8 D_801CEAA9;
extern u8 D_801CEAAA;
extern u8 D_801CEAAB;
extern u8 D_801CEAAC;
extern u8 D_801CEAAD;
extern u8 D_801CEAAE;

void func_0025FEB4(void)
{
    s16 result[4];
    s32 i;
    u16 *values = D_80220E70;
    s16 *record;
    s16 frame;

    if (D_80220E70[4] != 0) {
        frame = ++D_80220E70[5];
        record = (s16 *)(values + 6);
        if (frame < record[0]) {
            for (i = 0; i < 4; i++) {
                result[i] = (record[i + 1] - record[i + 5]) * frame / record[0] + record[i + 5];
            }
        } else {
            for (i = 0; i < 4; i++) {
                result[i] = record[i + 1];
            }
        }
        if (values[4] == 1) {
            values[0] = result[0];
            values[1] = result[1];
            values[2] = result[2];
            values[3] = result[3];
        } else {
            D_801CEAAC = result[0];
            D_801CEAAD = result[1];
            D_801CEAAE = result[2];
            switch (values[4]) {
            case 2:
                D_801CEAA8 = result[3];
                break;
            case 3:
                D_801CEAA9 = result[3];
                break;
            case 4:
                D_801CEAAA = result[3];
                break;
            case 5:
                D_801CEAAB = result[3];
                break;
            }
        }
        if ((s16)values[5] >= (s16)values[6]) {
            values[4] = 0;
        }
    }
}
