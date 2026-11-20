typedef unsigned char u8;
typedef signed int s32;
void bzero(void *p, s32 size);
void func_00145538(s32 kind, void *msg);
void func_001459A4(u8 a) {
    u8 msg[8];
    u8 *p = msg;
    bzero(p, 8);
    p[0] = a;
    func_00145538(6, p);
}
