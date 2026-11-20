typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef struct {
    u8 unk0[4];
    u16 w;
    u16 h;
} Size1F;
void func_001F8DDC(Size1F *a, s32 b, s32 c, s32 d, s32 e, s32 f, s32 g);
void func_001F913C(Size1F *s, s32 a, s32 b) {
    func_001F8DDC(s, 0, 0, s->w, s->h, a, b);
}
