typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;
typedef struct {
    s32 id;
    s32 chunk;
    s32 *p08;
    s32 kind;
    u32 n;
    void *p14;
    u8 unk18[4];
    void **list;
    u32 *age;
    u8 *p24;
    s32 w28;
    s32 w2C;
    u8 *w30;
    u8 *p34;
    void *p38;
} Rec2A;
extern Rec2A *D_802395B0;
u8 func_002A26D8(s32 id, s32 k) {
    Rec2A *r = D_802395B0;
    s32 i;
    for (i = 40; i-- != 0; r++) {
        if (r->id == id) {
            return r->w30[k];
        }
    }
    return 0;
}
