typedef unsigned char u8;
typedef unsigned int u32;
typedef struct {
    u32 flag : 1;
    u32 mode : 4;
    u32 rest : 27;
} Bits42;
extern u8 D_80187030;
extern u8 D_80187031;
void func_00042ccc(Bits42 *p) {
    D_80187030 = p->flag;
    D_80187031 = p->mode;
}
