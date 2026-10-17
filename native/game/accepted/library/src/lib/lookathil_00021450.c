extern const double D_800AE910, D_800AE918, D_800AE920, D_800AE928, D_800AE930;
extern void guMtxIdentF(float [4][4]);
extern void guMtxF2L(float [4][4], void *);
extern float sqrtf(float);
typedef struct { unsigned char color[3], pad, copy[3], pad2; signed char dir[3]; unsigned char unused[5]; } ReflectLight;
typedef struct { int x1, y1, x2, y2; } Hilite;
#define DIR(v) ((v)*128.0f < 127.0f ? (int)((v)*128.0f) : 127)
void func_00021450(float m[4][4], ReflectLight light[2], Hilite *hilite, float ex, float ey, float ez,
               float ax, float ay, float az, float ux, float uy, float uz,
               float l1x, float l1y, float l1z, float l2x, float l2y, float l2z, int width, int height)
{
    float lx, ly, lz, rx, ry, rz, length, hx, hy, hz;
    guMtxIdentF(m);
    lx=ax-ex; ly=ay-ey; lz=az-ez;
    length=D_800AE910/__builtin_sqrtf(lx*lx+ly*ly+lz*lz);
    lx*=length; ly*=length; lz*=length;
    rx=uy*lz-uz*ly;
    ry=uz*lx-ux*lz;
    rz=ux*ly-uy*lx;
    length=D_800AE918/__builtin_sqrtf(rx*rx+ry*ry+rz*rz);
    rx*=length; ry*=length; rz*=length;
    ux=ly*rz-lz*ry;
    uy=lz*rx-lx*rz;
    uz=lx*ry-ly*rx;
    length=D_800AE918/__builtin_sqrtf(ux*ux+uy*uy+uz*uz);
    ux*=length; uy*=length; uz*=length;
    length=D_800AE918/__builtin_sqrtf(l1x*l1x+l1y*l1y+l1z*l1z);
    l1x*=length; l1y*=length; l1z*=length;
    hx=l1x+lx; hy=l1y+ly; hz=l1z+lz;
    length=__builtin_sqrtf(hx*hx+hy*hy+hz*hz);
    if (length > D_800AE920) {
        length=D_800AE918/length;
        hx*=length; hy*=length; hz*=length;
        hilite->x1=width*4+2.0f*((hx*rx+hy*ry+hz*rz)*width);
        hilite->y1=height*4+2.0f*((hx*ux+hy*uy+hz*uz)*height);
    } else {
        hilite->x1=width*2; hilite->y1=height*2;
    }
    length=D_800AE928/__builtin_sqrtf(l2x*l2x+l2y*l2y+l2z*l2z);
    l2x*=length; l2y*=length; l2z*=length;
    hx=l2x+lx; hy=l2y+ly; hz=l2z+lz;
    length=__builtin_sqrtf(hx*hx+hy*hy+hz*hz);
    if (length > D_800AE930) {
        length=D_800AE928/length;
        hx*=length; hy*=length; hz*=length;
        hilite->x2=width*4+2.0f*((hx*rx+hy*ry+hz*rz)*width);
        hilite->y2=height*4+2.0f*((hx*ux+hy*uy+hz*uz)*height);
    } else {
        hilite->x2=width*2; hilite->y2=height*2;
    }
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
void func_00021dd4(void *m, ReflectLight light[2], Hilite *hilite, float ex, float ey, float ez,
              float ax, float ay, float az, float ux, float uy, float uz,
               float l1x, float l1y, float l1z, float l2x, float l2y, float l2z, int width, int height)
{
    float temp[4][4];
    func_00021450(temp,light,hilite,ex,ey,ez,ax,ay,az,ux,uy,uz,l1x,l1y,l1z,l2x,l2y,l2z,width,height);
    guMtxF2L(temp,m);
}

