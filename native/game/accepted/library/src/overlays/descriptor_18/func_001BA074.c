typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef struct {
    u8 used;
    u8 value;
    u16 id;
} ScriptSlot;
extern ScriptSlot D_8022A1F0[];
inline void func_001BA074(u16 id, u8 v) {
    s32 i;
    for (i = 0; i < 8; i++) {
        if (D_8022A1F0[i].used == 0) {
            break;
        }
    }
    if (i >= 8) {
        for (;;) {
        }
    }
    D_8022A1F0[i].value = v;
    D_8022A1F0[i].id = id;
    D_8022A1F0[i].used = 1;
}
