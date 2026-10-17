extern void guMtxIdentF(float [4][4]);
extern void guMtxF2L(float [4][4], void *);
extern float func_00020bf0(float);
extern float sinf(float);
inline void func_00022720(float m[4][4], unsigned short *normalize, float fovy,
                    float aspect, float near, float far, float scale)
{
    float cot;
    int row, col;
    guMtxIdentF(m);
    fovy *= 0.017453292222222222;
    cot = func_00020bf0(fovy/2.0f) / sinf(fovy/2.0f);
    m[0][0] = cot/aspect;
    m[1][1] = cot;
    m[2][2] = (near+far)/(near-far);
    m[2][3] = -1.0f;
    m[3][2] = (2.0f*near*far)/(near-far);
    m[3][3] = 0.0f;
    for (row=0; row<4; row++)
        for (col=0; col<4; col++)
            m[row][col] *= scale;
    if (normalize) {
        if (near+far <= 2.0) *normalize = 0xffff;
        else {
            *normalize = (unsigned short)(131072.0/(near+far));
            if (*normalize == 0) *normalize = 1;
        }
    }
}
void func_000228d0(void *m, unsigned short *normalize, float fovy,
                   float aspect, float near, float far, float scale)
{
    float temp[4][4];
    func_00022720(temp,normalize,fovy,aspect,near,far,scale);
    guMtxF2L(temp,m);
}

