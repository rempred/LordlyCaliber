typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef struct { u16 id; u8 count; u8 unk3; } InvEntry;
extern InvEntry D_80196B00[];
extern struct { u8 v; u8 pad[3]; } D_80196B03[];
void func_00041674(void);
void func_00048700(u16 id, s32 n) {
    u16 i;
    u16 free = 0xFFFF;
    if (id == 0) {
        return;
    }
    for (i = 0; i < 0x116; i++) {
        if (D_80196B00[i].id == id) {
            u8 c = D_80196B03[i].v + n;
            D_80196B03[i].v = c;
            if (c >= 100) {
                D_80196B03[i].v = 99;
            }
            break;
        }
        if (D_80196B00[i].id == 0 && free == 0xFFFF) {
            free = i;
        }
    }
    if (i >= 0x116) {
        if (free != 0xFFFF) {
            D_80196B00[free].id = id;
            D_80196B03[free].v = n;
        }
    }
    func_00041674();
}
