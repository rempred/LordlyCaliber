typedef unsigned char u8;
typedef signed int s32;
void bzero(void *p, s32 size);
void func_00145538(s32 kind, void *msg);
typedef struct {
    s32 a;
    u8 b;
    u8 c;
    u8 pad[2];
} Msg8;
void func_001457B0(s32 a, u8 b, u8 c) {
    Msg8 m;
    Msg8 *p = &m;
    bzero(p, 8);
    m.a = a;
    m.b = b;
    m.c = c;
    func_00145538(3, p);
}
