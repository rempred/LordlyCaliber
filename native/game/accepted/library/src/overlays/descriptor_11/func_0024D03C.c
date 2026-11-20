typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef struct {
    u16 id;
    u8 kind;
    u8 pad;
    u16 flags;
} AnimReq;
extern u8 *(*D_80209740[])(u16 id, u8 x);
extern inline void func_0024CD84(u8 *p, u16 f) {
    s16 v;
    (*(u16 *)((u8 *)(p) + (0xA))) |= f & 0xD000;
    if (f & 0x1000) {
        if (f & 0x400) {
            v = (f & 0x800) ? 10000 : -10000;
        } else {
            v = (f & 0x800) ? 1000 : -1000;
        }
        (*(s32 *)((u8 *)(p) + (0xC))) = v;
    }
}
u8 *func_0024D03C(AnimReq *r, u8 x) {
    u8 *o = D_80209740[r->kind & 3](r->id, x);
    func_0024CD84(o, r->flags);
    return o + 0x1C;
}
