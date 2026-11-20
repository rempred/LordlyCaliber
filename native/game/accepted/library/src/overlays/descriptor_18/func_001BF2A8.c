typedef unsigned char u8;
typedef signed int s32;
typedef struct { s32 used; u8 b[0x58]; } ScriptRes;
extern ScriptRes D_8022A4E0[];
s32 func_001BF2A8(void) {
    s32 i;
    for (i = 0; i < 4; i++) {
        if (D_8022A4E0[i].used != 0) {
            return 1;
        }
    }
    return 0;
}
