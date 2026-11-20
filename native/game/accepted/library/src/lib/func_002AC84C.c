typedef unsigned char u8;
typedef signed int s32;
extern u8 *D_8022A974;
extern u8 *D_801CE8BC;
u8 D_8016FF5C(u8 a, u8 b);
void func_002AC84C(u8 slot, u8 kind) {
    s32 i;
    s32 k;
    s32 id;
    s32 off;
    u8 *d;
    u8 *a;
    u8 *b;
    i = 0;
    off = 0x1C4;
    do {
        id = (*(s32 *)((u8 *)(D_801CE8BC + off) + (0x48)));
        if (id != 0 && D_8016FF5C(id, kind)) {
            break;
        }
        i++;
        off += 0xF8;
    } while (i < 20);
    if (i < 20) {
        d = D_8022A974;
        for (k = 0; k < 28; k++) {
            if ((*(u8 * *)((u8 *)(d + k * 4) + (0x18))) != ((void *)0) && (*(u8 * *)((u8 *)(d + k * 4) + (0x18)))[0x147] == i) {
                b = (*(u8 * *)((u8 *)(d + (u8)k * 4) + (0x18)));
                a = (*(u8 * *)((u8 *)(d + slot * 4) + (0x18)));
                (*(u8 * *)((u8 *)(d + (u8)k * 4) + (0x18))) = a;
                (*(u8 * *)((u8 *)(d + slot * 4) + (0x18))) = b;
                if (b != ((void *)0)) {
                    (*(s32 *)((u8 *)(b) + (0xE4))) = slot;
                }
                if (a != ((void *)0)) {
                    (*(s32 *)((u8 *)(a) + (0xE4))) = (u8)k;
                }
                return;
            }
        }
    }
}
