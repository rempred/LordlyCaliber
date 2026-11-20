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
void func_0020BD5C(void) {
    if (D_801D0810->f2 != ((void *)0)) {
        D_801D0810->f2();
    }
}
