#include "common/types.h"

extern u16 D_80220E60[6];
extern u8 D_801CEAA8;
extern u8 D_801CEAA9;
extern u8 D_801CEAAA;
extern u8 D_801CEAAB;
extern u8 D_801CEAAC;
extern u8 D_801CEAAD;
extern u8 D_801CEAAE;

void func_00260270(void)
{
    u16 *values = D_80220E60;
    s32 mode = values[0];

    if (mode != 0) {
        D_80220E60[1]--;
        if ((s16)D_80220E60[1] < 0) {
            D_801CEAAC = 0;
            D_801CEAAD = 0;
            D_801CEAAE = 0;
            switch (mode) {
            case 1:
                D_801CEAA8 = 0;
                break;
            case 2:
                D_801CEAA9 = 0;
                break;
            case 3:
                D_801CEAAA = 0;
                break;
            case 4:
                D_801CEAAB = 0;
                break;
            }
            values[0] = 0;
        } else {
            switch (mode) {
            case 1:
                D_801CEAA8 = D_80220E60[5];
                break;
            case 2:
                D_801CEAA9 = D_80220E60[5];
                break;
            case 3:
                D_801CEAAA = D_80220E60[5];
                break;
            case 4:
                D_801CEAAB = D_80220E60[5];
                break;
            }
            D_801CEAAC = values[2];
            D_801CEAAD = values[3];
            D_801CEAAE = values[4];
        }
    }
}
