typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef struct { s32 used; u8 b[0x58]; } ScriptRes;
extern ScriptRes D_8022A4E0[];
void func_001BA074(u16 a, u8 b);
void func_001C1024(u32 i, s32 v) {
    if (i >= 4) {
        for (;;) {
        }
    }
    D_8022A4E0[i].b[10] = v;
    func_001BA074(i | 0x5200, v);
}
