typedef unsigned char u8;
typedef signed int s32;
extern u8 *D_80196AF8;
void D_801D8A60(s32 a);
void func_000b6a90(s32 dir) {
    if (D_80196AF8[0x1DFF] >= 2) {
        if (dir == -1) {
            D_801D8A60(2);
            do {
                D_80196AF8[0x9D] = (D_80196AF8[0x9D] == 0) ? 2 : D_80196AF8[0x9D] - 1;
            } while ((D_80196AF8 + D_80196AF8[0x9D])[0x1E00] == 0);
        } else if (dir == 1) {
            D_801D8A60(2);
            do {
                D_80196AF8[0x9D] = (D_80196AF8[0x9D] + 1) % 3;
            } while ((D_80196AF8 + D_80196AF8[0x9D])[0x1E00] == 0);
        }
    }
}
