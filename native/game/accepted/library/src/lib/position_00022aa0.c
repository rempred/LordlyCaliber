extern float sinf(float);
extern float func_00020bf0(float);
extern void guMtxF2L(float [4][4], void *);
extern float D_800AA730;
inline void func_00022aa0(float m[4][4], float ax, float ay, float az, float scale,
                 float x, float y, float z)
{
    float sx,cx,sy,cy,sz,cz;
    ax*=D_800AA730; ay*=D_800AA730; az*=D_800AA730;
    sx=sinf(ax); cx=func_00020bf0(ax);
    sy=sinf(ay); cy=func_00020bf0(ay);
    sz=sinf(az); cz=func_00020bf0(az);
    m[0][0]=cy*cz*scale;
    m[0][1]=cy*sz*scale;
    m[0][2]=-sy*scale;
    m[1][0]=(sx*sy*cz-cx*sz)*scale;
    m[1][1]=(sx*sy*sz+cx*cz)*scale;
    m[1][2]=sx*cy*scale;
    m[2][0]=(cx*sy*cz+sx*sz)*scale;
    m[2][1]=(cx*sy*sz-sx*cz)*scale;
    m[2][2]=cx*cy*scale;
    m[0][3]=0; m[1][3]=0; m[2][3]=0;
    m[3][0]=x; m[3][1]=y; m[3][2]=z; m[3][3]=1;
}
void func_00022c78(void *m, float ax, float ay, float az, float scale,
                float x, float y, float z)
{
    float temp[4][4];
    func_00022aa0(temp,ax,ay,az,scale,x,y,z);
    guMtxF2L(temp,m);
}
