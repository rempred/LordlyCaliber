typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
extern u8 *D_801B3E20;
void func_000fbed4(s32 i, void *v);
void func_000fbed4(s32 i, void *v) {
    u8 *p = D_801B3E20;
    p += i * 20;
    (*(s32 *)((u8 *)(p) + (0x214))) = 0;
    (*(void * *)((u8 *)(p) + (0x218))) = v;
    (*(s16 *)((u8 *)(p) + (0x208))) = 1;
}
