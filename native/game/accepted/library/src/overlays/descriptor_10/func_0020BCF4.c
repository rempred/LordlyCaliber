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
s32 func_0020BCF4(void) {
    if (D_801D0810->f0 != ((void *)0)) {
        return D_801D0810->f0();
    }
    return 1;
}
