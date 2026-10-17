/* ROM 28740: the fixed-format inputs each convert to a separate 4x4 matrix. */
extern void guMtxL2F(float [4][4], void *);
extern void guMtxCatF(float [4][4], float [4][4], float [4][4]);
extern void guMtxF2L(float [4][4], void *);
void func_00028740(void *a, void *b, void *out)
{
    float af[4][4], bf[4][4], result[4][4];
    guMtxL2F(af, a);
    guMtxL2F(bf, b);
    guMtxCatF(af, bf, result);
    guMtxF2L(result, out);
}
