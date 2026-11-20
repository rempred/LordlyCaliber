typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
void D_80180B1C(u8 *n, s16 x0, s16 y0, s16 x1, s16 y1);
typedef struct {
    s32 used;
    void *text;
    u8 flags;
    u8 state;
    s16 hA;
    s16 hC;
    u16 hE;
    u16 h10;
    u16 h12;
    u16 h14;
    s16 h16;
    s16 h18;
    s16 h1A;
    s16 h1C;
    u8 unk1E[2];
} Rec698;
extern Rec698 D_8022A698[];
void func_001C1DF4(void) {
    s32 i;
    Rec698 *r;
    u8 *n;
    s32 st;
    for (i = 0; i < 4; i++) {
        r = &D_8022A698[i];
        if (r->used == 0) {
            continue;
        }
        st = r->state;
        if (st != 0) {
            if (st < 3) {
                if (++r->h1C >= 10) {
                    r->h1C = 10;
                    r->h10 = r->h14;
                    r->h16 = r->h1A;
                    if (r->state == 1) {
                        r->state = 0;
                    } else {
                        r->state = 3;
                    }
                } else {
                    r->h10 = r->h12 + (r->h14 - r->h12) * r->h1C / 10;
                    r->h16 = r->h18 + (r->h1A - r->h18) * r->h1C / 10;
                }
            }
        }
        if (!(r->flags & 2)) {
            continue;
        }
        n = (u8 *)r->used;
        (*(s32 *)((u8 *)(n) + (0x2C))) = r->hA - r->h10 / 2;
        (*(s32 *)((u8 *)(n) + (0x30))) = r->hC - 11;
        (*(s32 *)((u8 *)(n) + (0x34))) = (*(s32 *)((u8 *)(n) + (0x2C))) + r->h10;
        (*(s32 *)((u8 *)(n) + (0x38))) = (*(s32 *)((u8 *)(n) + (0x30))) + 0x15;
        D_80180B1C(n, (*(s16 *)((u8 *)(n) + (0x2E))), (*(s16 *)((u8 *)(n) + (0x32))), (*(s16 *)((u8 *)(n) + (0x36))), (*(s32 *)((u8 *)(n) + (0x38))));
    }
}
