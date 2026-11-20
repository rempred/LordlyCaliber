typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef struct {
    u8 unk0[4];
    u16 w;
    u16 h;
} SizeCE;
void func_001CEE8C(SizeCE *s, s32 a, s32 b, s32 c, s32 d, s32 e, s32 f);
extern s32 D_801B8090;
static void func_001CF3E0(void);
void func_001CF3AC(SizeCE *s, s32 a, s32 b) {
    func_001CEE8C(s, 0, 0, s->w, s->h, a, b);
}
static void func_001CF3E0(void) {
    D_801B8090 = 0;
}
