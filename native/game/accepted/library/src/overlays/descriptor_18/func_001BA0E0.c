typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef struct {
    u8 used;
    u8 value;
    u16 id;
} ScriptSlot;
extern ScriptSlot D_8022A1F0[];
void func_001BA0E0(u16 id) {
    s32 i;
    for (i = 0; i < 8; i++) {
        if (D_8022A1F0[i].used != 0 && D_8022A1F0[i].id == id) {
            D_8022A1F0[i].value = 0;
            D_8022A1F0[i].id = 0;
            D_8022A1F0[i].used = 0;
            return;
        }
    }
}
