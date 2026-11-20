typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef struct {
    u32 w0;
    u32 w1;
} Gfx;
typedef struct {
    void *p[5];
    Gfx *gfx;
} FbSet;
extern FbSet D_800A81C0[2];
extern u16 D_800C4C08;
extern u16 D_800E7D68;
extern u16 D_800C4A18;
extern u16 D_800E7A1C;
extern u16 D_800C4BCA;
extern u16 D_800C4AD8;
extern u8 D_800AEE72;
extern u8 D_800A81F0;
void bzero(void *p, s32 n);
void *func_000016C4(void *p);
void *func_000016C4(void *ptr);
extern u8 g_resource_state_byte_3;
static void func_0000377C(void);
static u8 func_0000378C(void);
void boot_resource_buffer_reset_flags(void) {
    s32 i;
    for (i = 0; i < 2; i++) {
        func_000016C4(D_800A81C0[i].p[0]);
        func_000016C4(D_800A81C0[i].p[1]);
        func_000016C4(D_800A81C0[i].p[2]);
        func_000016C4(D_800A81C0[i].p[3]);
        func_000016C4(D_800A81C0[i].p[4]);
        func_000016C4(D_800A81C0[i].gfx);
    }
    D_800C4C08 = 0;
    D_800E7D68 = 0;
    D_800C4A18 = 0;
    D_800E7A1C = 0;
    D_800C4BCA = 0;
    D_800C4AD8 = 0;
    D_800AEE72 = 2;
    bzero(D_800A81C0, 0x30);
    D_800A81F0 = 0;
}
static void func_0000377C(void) {
    g_resource_state_byte_3 = 1;
}
static u8 func_0000378C(void) {
    return g_resource_state_byte_3;
}
