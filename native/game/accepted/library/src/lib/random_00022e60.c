/* Independently derived from the 44-byte body and retained data at ROM 3AB40. */
extern unsigned int D_800AA740;
unsigned int func_00022e60(void)
{
    unsigned int shifted = D_800AA740 << 2;
    D_800AA740 = ((shifted + 2) * (shifted + 3)) >> 2;
    return D_800AA740;
}
