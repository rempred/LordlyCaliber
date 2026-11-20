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
void *func_00001330(s32 size);
void bzero(void *p, s32 n);
void func_002A21E0(void) {
    Rec2A *p = func_00001330(0x960);
    D_802395B0 = p;
    bzero(p, 0x960);
}
