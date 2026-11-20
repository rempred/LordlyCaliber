typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct {
    u8 unk0[6];
    u16 h6;
    u8 unk8[2];
} RecCE;
typedef struct {
    u8 unk0[4];
    u32 n;
    u8 unk8[4];
    RecCE *recs;
    u16 (*links)[2];
} SkelCE;
typedef struct {
    f32 m[4][4];
    s32 pad;
} MtxCE;
void guMtxCatF(void *a, void *b, void *c);
void func_001D013C(void *root, SkelCE *s, MtxCE *out) {
    void *m[32];
    u32 i;
    MtxCE *dst;
    void **pm;
    m[0] = root;
    for (i = 0; i < s->n; i++) {
        dst = &out[s->links[i][0]];
        pm = (void **)(s->links[i][1] * 4 + (s32)m);
        guMtxCatF(dst, *pm, dst);
        pm[1] = dst;
    }
}
