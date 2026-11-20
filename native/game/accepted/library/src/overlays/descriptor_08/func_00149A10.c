typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef struct {
    u32 flags;
    u8 unk04[0x6C];
    s32 unk70;
} MapUnit;
typedef struct {
    MapUnit *u;
    u16 flags;
    u8 b6;
    u8 b7;
    u8 unk08[0x2A];
    u8 b32;
    u8 b33;
} MapMarker;
u8 func_001460B8(MapUnit *u);
void func_00149A10(MapMarker *m) {
    s32 r;
    if (m->b7 == 0) {
        r = 0;
        if (m->u->unk70 == 1) {
            u32 t = m->u->flags & 0x40;
            r = t == 0;
        }
        m->b33 = r;
        m->b32 = func_001460B8(m->u);
        m->b6 = 0;
        m->flags &= 0xFBFF;
    }
}
