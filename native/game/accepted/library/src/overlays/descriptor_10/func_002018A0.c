typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef struct {
    signed char *name;
    u8 size;
    u8 gender;
    u8 leadership;
    u16 hpBase;
    u8 hpGrowth;
    u16 strBase;
    u8 strGrowth;
    u16 vitBase;
    u8 vitGrowth;
    u16 intBase;
    u8 intGrowth;
    u16 menBase;
    u8 menGrowth;
    u16 agiBase;
    u8 agiGrowth;
    u16 dexBase;
    u8 dexGrowth;
    u8 luck;
    u8 alignment;
    u8 resist[7];
    u8 movement;
    u16 defaultItems[4];
    u8 lockedSlots;
    u8 attacks[3][2];
    u8 physAttack;
    u8 magicAttack;
    u8 physDefense;
    u8 magicDefense;
    u8 promotions[4];
    u8 baseClass;
    u8 element;
    u8 squadSlots;
} ClassData;
extern ClassData g_func_0019554C_class_records[];
extern u8 (*D_801D068C)[2];
u8 func_002018A0(s32 cls, s32 base) {
    if (g_func_0019554C_class_records[cls].baseClass == base && D_801D068C[cls][1] == 0xFF &&
        D_801D068C[base][1] == 0xFF) {
        return 0;
    }
    return D_801D068C[base][1];
}
