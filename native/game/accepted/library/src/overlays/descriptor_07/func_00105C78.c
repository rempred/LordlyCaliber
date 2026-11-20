typedef signed int s32;
typedef unsigned int u32;
typedef struct {
    u32 w[0x30];
} Unit105;
extern Unit105 D_801F38D8;
void bcopy(void *src, void *dst, s32 n);
void func_00105C78(Unit105 *u) {
    u32 keep = u->w[0x2A];
    bcopy(&D_801F38D8, u, sizeof(Unit105));
    u->w[0x2A] = keep;
}
