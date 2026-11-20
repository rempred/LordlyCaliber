typedef unsigned int u32;
typedef struct {
    u32 w[0x30];
} Unit105;
extern Unit105 D_801F38D8;
void func_00105C20(Unit105 *u) {
    u->w[0] &= ~0x80000;
    D_801F38D8 = *u;
}
