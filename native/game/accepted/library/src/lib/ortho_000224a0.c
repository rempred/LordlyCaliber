extern void guMtxIdentF(float [4][4]);
extern void guMtxF2L(float [4][4], void *);
inline void func_000224a0(float m[4][4], float left, float right, float bottom, float top,
              float near, float far, float scale)
{
    int row, col;
    guMtxIdentF(m);
    m[0][0] = 2.0f / (right-left);
    m[1][1] = 2.0f / (top-bottom);
    m[2][2] = -2.0f / (far-near);
    m[3][0] = -(right+left) / (right-left);
    m[3][1] = -(top+bottom) / (top-bottom);
    m[3][2] = -(far+near) / (far-near);
    m[3][3] = 1.0f;
    for (row=0; row<4; row++)
        for (col=0; col<4; col++)
            m[row][col] *= scale;
}
void func_000225d0(void *m, float left, float right, float bottom, float top,
             float near, float far, float scale)
{
    float temp[4][4];
    func_000224a0(temp,left,right,bottom,top,near,far,scale);
    guMtxF2L(temp,m);
}
