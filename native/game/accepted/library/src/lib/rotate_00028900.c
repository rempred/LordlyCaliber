extern float sinf(float);
extern float func_00020bf0(float);
extern void guNormalize(float *, float *, float *);
extern void guMtxIdentF(float [4][4]);
extern void guMtxF2L(float [4][4], void *);
extern float D_800ABAE0;
inline void func_00028900(float m[4][4], float angle, float x, float y, float z)
{
    /* Reusing t and keeping the off-diagonal stores here preserves the KMC allocation. */
    float s,c,xy,yz,zx,t,xs,ys,zs;
    guNormalize(&x,&y,&z);
    angle*=D_800ABAE0;
    s=sinf(angle); c=func_00020bf0(angle);
    t=1.0f-c;
    xy=x*y*t;
    yz=y*z*t;
    zx=z*x*t;
    guMtxIdentF(m);
    t=x*x;
    xs=x*s; ys=y*s; zs=z*s;
    m[0][0]=t+c*(1.0f-t);
    m[2][1]=yz-xs;
    m[1][2]=yz+xs;
    t=y*y;
    m[1][1]=t+c*(1.0f-t);
    m[2][0]=zx+ys;
    m[0][2]=zx-ys;
    t=z*z;
    m[2][2]=t+c*(1.0f-t);
    m[1][0]=xy-zs;
    m[0][1]=xy+zs;
}
void guRotate(void *m, float angle, float x, float y, float z)
{
    float temp[4][4];
    func_00028900(temp,angle,x,y,z);
    guMtxF2L(temp,m);
}
