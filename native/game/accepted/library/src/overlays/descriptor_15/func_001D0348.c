typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
void *func_00001330(s32 size);
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
void *func_00001330(s32 size);
void func_001D0348(SkelCE *s, void **out) {
    u32 i;
    for (i = 0; i < s->n; i++) {
        if (s->recs[i].h6 != 0) {
            out[i] = func_00001330(0x24);
        } else {
            out[i] = ((void *)0);
        }
    }
}
