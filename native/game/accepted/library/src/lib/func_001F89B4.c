extern int func_001F7ADC(void *, float, float, float, short);
/* Named float objects retain the compiler's floating-point loads before
 * transfer to the integer argument registers. Literal pointer accesses
 * instead produce direct integer loads with this compiler.
 */
extern float D_801D0768, D_801D0718, D_801D0814;
extern float D_801D06F0, D_801D0740, D_801D0764;

int func_001F89B4(void *object)
{
    short first = *(short *)0x801D071C;

    if (first != 0) {
        func_001F7ADC(object, D_801D0768, D_801D0718, D_801D0814, first);
    }
    {
        short second = *(short *)0x801D0820;

        if (second != 0) {
            func_001F7ADC(object, D_801D06F0, D_801D0740, D_801D0764, second);
        }
    }
    return 0;
}
