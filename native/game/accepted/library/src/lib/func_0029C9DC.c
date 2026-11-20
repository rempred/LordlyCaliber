typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef unsigned int u32;
typedef struct {
    u8 unk000[0xE8];
    s32 wE8;
    s32 wEC;
    s32 wF0;
    s32 wF4;
    u8 unk0F8[0x30];
    s32 w128;
    u8 unk12C[8];
    s16 h134;
    u8 unk136[4];
    s16 h13A;
    u8 unk13C;
    u8 b13D;
    u8 unk13E[7];
    u8 b145;
    u8 b146;
} BUnit;
typedef struct {
    u8 unk0[0xC];
    s16 hC;
    u8 bE;
} BObj;
typedef struct {
    u8 unk00[0x18];
    BUnit *units[0x38];
    BObj *objs[1];
} Battle;
extern Battle *D_8022A974;
void bzero(void *p, s32 n);
extern s16 D_8022A994;
extern u8 D_8018FC19;
extern u8 D_8022A997;
void D_80227FD8(s32 a, s32 b);
void D_8023998C(void);
void D_8023A98C(void);
void D_80227010(void);
void *func_000016C4(void *p);
extern s16 *D_8022A97C;
void *func_00001330(s32 size);
s32 D_8009DAF4(s32 id);
void D_8009DBB8(void *dst, s32 id);
u32 func_0000ABE0(void *p);
void func_0000A510(void *dst, void *src);
void func_002A21E0(void);
void func_0029C9DC(void) {
    void *raw;
    D_8022A974 = func_00001330(0x1CB8);
    bzero(D_8022A974, 0x1CB8);
    D_80227FD8(D_8022A994, 0);
    if (D_8018FC19 == 2) {
        D_8023998C();
    } else if (D_8018FC19 == 0) {
        D_8022A997 = 0;
        D_8023A98C();
    }
    if (D_8022A97C != ((void *)0)) {
        func_000016C4(D_8022A97C);
    }
    raw = func_00001330(D_8009DAF4(0x109EEB0));
    D_8009DBB8(raw, 0x109EEB0);
    D_8022A97C = func_00001330(func_0000ABE0(raw));
    func_0000A510(D_8022A97C, raw);
    func_000016C4(raw);
    func_002A21E0();
    D_80227010();
}
