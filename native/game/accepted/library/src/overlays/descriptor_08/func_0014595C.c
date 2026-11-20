typedef signed short s16;
typedef signed int s32;
void bzero(void *p, s32 size);
void func_00145538(s32 kind, void *msg);
void func_0014595C(s16 a) {
    s16 msg[4];
    s16 *p = msg;
    bzero(p, 8);
    p[0] = a;
    func_00145538(5, p);
}
