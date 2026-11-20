typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
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
void *func_000016C4(void *p);
void func_002A1380(s32 i) {
    func_000016C4(D_8022A974->objs[i]);
    D_8022A974->objs[i] = ((void *)0);
}
