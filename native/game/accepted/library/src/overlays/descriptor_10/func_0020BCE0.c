typedef signed short s16;
typedef signed int s32;
typedef struct {
    s32 (*f0)(void);
    void (*f1)(void);
    void (*f2)(void);
    void (*f3)(s16);
    void (*f4)(s16);
} BattleCallbacks;
extern BattleCallbacks *D_801D0810;
extern BattleCallbacks D_801CFC74;
void func_0020BCE0(void) {
    D_801D0810 = &D_801CFC74;
}
