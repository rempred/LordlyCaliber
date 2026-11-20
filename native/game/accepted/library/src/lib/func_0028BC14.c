typedef unsigned char u8;
typedef signed short s16;
typedef struct {
    s16 x0;
    s16 y0;
    s16 x1;
    s16 y1;
    u8 unk08[0x58];
    u8 col;
    u8 unk61[0xF];
    u8 row;
} Box28;
void func_0028BC14(u8 kind, Box28 *p) {
    p->x0 = p->col * 60 + 12;
    p->y0 = p->row * 72 + 10;
    if (kind == 0xA0) {
        p->x1 = p->col * 60 + 0x30;
        p->y1 = p->row * 72 + 0x3A;
    } else {
        p->x1 = p->col * 60 + 0x2C;
        p->y1 = p->row * 72 + 0x3E;
    }
}
