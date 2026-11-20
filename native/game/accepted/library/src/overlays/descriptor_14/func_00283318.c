typedef signed short s16;
typedef signed int s32;
typedef struct {
    s16 h0;
    s16 h2;
    s16 h4;
    s16 h6;
} St8C4;
extern St8C4 *D_8022A8C4;
void func_000016C4(void *p);
void func_000118c4(s32 flags, s32 volume);
void func_00283318(void) {
    func_000016C4(D_8022A8C4);
    D_8022A8C4 = ((void *)0);
    func_000118c4(1, 0x7FFF);
}
