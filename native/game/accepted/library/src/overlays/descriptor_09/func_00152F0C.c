typedef unsigned char u8;
typedef unsigned short u16;
typedef struct {
    u8 unk00[0x11];
    u8 cls;
    u8 baseCls;
    u8 level;
    u8 gender;
    u16 maxHp;
    u16 hp;
    u8 element;
    u8 alignment;
    u16 str;
    u16 vit;
    u16 intl;
    u16 men;
    u16 agi;
    u16 dex;
    u8 luck;
    u16 items[4];
    u8 fatigue;
    u8 status;
} Character;
typedef struct {
    Character c;
    u8 pad[4];
} PlayerChar;
extern PlayerChar D_80193BC0_copy[];
extern Character D_80195560_copy[];
u16 func_00152F0C(u8 side, u8 idx, u8 slot) {
    Character *c;
    u16 v;
    if (side == 0) {
        c = &D_80193BC0_copy[idx].c;
    } else {
        c = &D_80195560_copy[idx];
    }
    v = 0;
    switch (slot) {
        case 1:
            v = c->items[0];
            break;
        case 2:
            v = c->items[1];
            break;
        case 3:
            v = c->items[2];
            break;
        case 4:
            v = c->items[3];
            break;
    }
    return v;
}
