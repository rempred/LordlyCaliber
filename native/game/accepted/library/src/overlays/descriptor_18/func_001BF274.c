typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;
typedef struct { s32 used; u8 b[0x58]; } ScriptRes;
extern ScriptRes D_8022A4E0[];
ScriptRes *func_001BF274(u32 i) {
    if (i >= 4) {
        for (;;) {
        }
    }
    return &D_8022A4E0[i];
}
