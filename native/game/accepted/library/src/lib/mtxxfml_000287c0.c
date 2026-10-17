extern void guMtxL2F(float [4][4], void *);
extern void mtx_apply_vec3(float [4][4], float, float, float, float *, float *, float *);
void func_000287c0(void *m, float x, float y, float z, float *ox, float *oy, float *oz)
{
    float decoded[4][4];
    guMtxL2F(decoded, m);
    mtx_apply_vec3(decoded, x, y, z, ox, oy, oz);
}
