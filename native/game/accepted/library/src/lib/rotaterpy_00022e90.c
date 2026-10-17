extern float sinf(float);
extern float func_00020bf0(float);
extern void guMtxIdentF(float [4][4]);
extern void guMtxF2L(float [4][4], void *);
extern float D_800AA750;
inline void func_00022e90(float m[4][4], float ax, float ay, float az)
{
    float sx,cx,sy,cy,sz,cz;
    ax*=D_800AA750; ay*=D_800AA750; az*=D_800AA750;
    sx=sinf(ax); cx=func_00020bf0(ax);
    sy=sinf(ay); cy=func_00020bf0(ay);
    sz=sinf(az); cz=func_00020bf0(az);
    guMtxIdentF(m);
    m[0][0]=cy*cz;
    m[0][1]=cy*sz;
    m[0][2]=-sy;
    m[1][0]=(sx*sy*cz-cx*sz);
    m[1][1]=(sx*sy*sz+cx*cz);
    m[1][2]=sx*cy;
    m[2][0]=(cx*sy*cz+sx*sz);
    m[2][1]=(cx*sy*sz-sx*cz);
    m[2][2]=cx*cy;
}
void func_00023018(void *m, float ax, float ay, float az)
{
    float temp[4][4];
    func_00022e90(temp,ax,ay,az);
    guMtxF2L(temp,m);
}
