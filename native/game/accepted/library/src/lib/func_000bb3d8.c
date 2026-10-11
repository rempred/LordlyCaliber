typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;

extern u8 D_80193BC0[];
extern u8 D_80195560[];

u16 func_000bb3d8(s32 arg0, u8 index, u8 slot)
{
    u8 *record;

    if ((arg0 & 0xFF) == 0) {
        record = D_80193BC0 + ((s32)index * 0x38);
    } else {
        record = D_80195560 + ((s32)index * 0x34);
    }

    switch (slot) {
    case 1:
        return *(u16 *)(record + 0x2A);
    case 2:
        return *(u16 *)(record + 0x2C);
    case 3:
        return *(u16 *)(record + 0x2E);
    default:
        return *(u16 *)(record + 0x30);
    }
}
