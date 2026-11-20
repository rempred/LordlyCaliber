typedef unsigned char u8;
typedef unsigned int u32;
typedef struct {
    u32 flag : 1;
    u32 mode : 4;
    u32 rest : 27;
} Bits42;
extern u8 D_80187030;
extern u8 D_80187031;
void func_00042cf4(Bits42 *p);
void func_00042cf4(Bits42 *p) {
    p->flag = D_80187030;
    p->mode = D_80187031;
}
