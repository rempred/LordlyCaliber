typedef unsigned char u8;
typedef signed int s32;
extern u8 *D_80196AF8;
void func_000b48f0(s32 a, s32 b, s32 c);
u8 *D_801CC410(s32 id);
void func_000b4ab0(s32 a, s32 v) {
    u8 *st = D_80196AF8;
    s32 f;
    if (st[0x98] == 0 || st[0x99] == 1) {
        f = 1;
    } else {
        f = 0;
    }
    func_000b48f0(a, f, 1);
    D_801CC410(0x100)[0xE7] = v;
}
