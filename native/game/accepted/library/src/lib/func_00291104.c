typedef unsigned char u8;
typedef signed short s16;
typedef struct {
    u8 unk0[8];
    s16 a;
    s16 b;
} Pair28;
extern Pair28 *D_8023DE38;
void *func_000016C4(void *p);
void func_00291104(void) {
    func_000016C4(D_8023DE38);
    D_8023DE38 = ((void *)0);
}
