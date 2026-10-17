extern void guMtxIdentF(float [4][4]);
extern void guMtxF2L(float [4][4], void *);
extern float sqrtf(float);
typedef struct { unsigned char color[3], pad, copy[3], pad2; signed char dir[3]; unsigned char unused[5]; } ReflectLight;
#define DIR(v) ((v)*128.0f < 127.0f ? (int)((v)*128.0f) : 127)
void func_00021eb0(float m[4][4], ReflectLight light[2], float ex, float ey, float ez,
               float ax, float ay, float az, float ux, float uy, float uz)
{
    float lx, ly, lz, rx, ry, rz, length;
    guMtxIdentF(m);
    lx=ax-ex; ly=ay-ey; lz=az-ez;
    length=-1.0/__builtin_sqrtf(lx*lx+ly*ly+lz*lz);
    lx*=length; ly*=length; lz*=length;
    rx=uy*lz-uz*ly;
    ry=uz*lx-ux*lz;
    rz=ux*ly-uy*lx;
    length=1.0/__builtin_sqrtf(rx*rx+ry*ry+rz*rz);
    rx*=length; ry*=length; rz*=length;
    ux=ly*rz-lz*ry;
    uy=lz*rx-lx*rz;
    uz=lx*ry-ly*rx;
    length=1.0/__builtin_sqrtf(ux*ux+uy*uy+uz*uz);
    ux*=length; uy*=length; uz*=length;
    light[0].dir[0]=DIR(rx); light[0].dir[1]=DIR(ry); light[0].dir[2]=DIR(rz);
    light[1].dir[0]=DIR(ux); light[1].dir[1]=DIR(uy); light[1].dir[2]=DIR(uz);
    light[0].color[0]=0; light[0].color[1]=0; light[0].color[2]=0; light[0].pad=0;
    light[0].copy[0]=0; light[0].copy[1]=0; light[0].copy[2]=0; light[0].pad2=0;
    light[1].color[0]=0; light[1].color[1]=0x80; light[1].color[2]=0; light[1].pad=0;
    light[1].copy[0]=0; light[1].copy[1]=0x80; light[1].copy[2]=0; light[1].pad2=0;
    m[0][0]=rx; m[1][0]=ry; m[2][0]=rz;
    m[3][0]=-(ex*rx+ey*ry+ez*rz);
    m[0][1]=ux; m[1][1]=uy; m[2][1]=uz;
    m[3][1]=-(ex*ux+ey*uy+ez*uz);
    m[0][2]=lx; m[1][2]=ly; m[2][2]=lz;
    m[3][2]=-(ex*lx+ey*ly+ez*lz);
    m[0][3]=0; m[1][3]=0; m[2][3]=0; m[3][3]=1;
}
void func_00022428(void *m, ReflectLight light[2], float ex, float ey, float ez,
              float ax, float ay, float az, float ux, float uy, float uz)
{
    float temp[4][4];
    func_00021eb0(temp,light,ex,ey,ez,ax,ay,az,ux,uy,uz);
    guMtxF2L(temp,m);
}
