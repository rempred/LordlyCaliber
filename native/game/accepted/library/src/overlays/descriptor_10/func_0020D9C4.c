typedef unsigned char u8;
typedef signed int s32;
extern u8 *D_801CE8BC;
typedef struct BattleObj20 {
    void (*fn)(struct BattleObj20 *self);
} BattleObj20;
s32 func_0020D9C4(u8 *p) {
    s32 i;
    for (i = 0; i < *(s32 *)(D_801CE8BC + 0x52BC); i++) {
        BattleObj20 *o = ((BattleObj20 **)(p + 0xC0))[i];
        o->fn(o);
    }
    return 1;
}
