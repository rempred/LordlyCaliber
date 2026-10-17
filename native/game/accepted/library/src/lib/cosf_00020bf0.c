extern const double D_800AE890[5];
extern const double D_800AE8B8, D_800AE8C0, D_800AE8C8;
extern const double D_800AE8D8, D_800AE8E0;
extern const float D_800AE8D0;
extern float __libm_qnan_f;
float func_00020bf0(float x)
{
    int exponent, n;
    double y, t, polynomial;
    /* The accepted target inspects the IEEE float representation in its argument slot. */
    exponent=(*(int *)&x >> 22)&0x1ff;
    if (exponent<0x136) {
        y=x>0.0f ? x : -x;
        t=y*D_800AE8B8+D_800AE8D8;
        n=t>=0.0 ? (int)(t+D_800AE8D8) : (int)(t-D_800AE8D8);
        t=(double)n;
        t-=D_800AE8E0;
        y-=t*D_800AE8C0;
        y-=t*D_800AE8C8;
        t=y*y;
        polynomial=((D_800AE890[4]*t+D_800AE890[3])*t+D_800AE890[2])*t+D_800AE890[1];
        t=y*t*polynomial;
        polynomial=y+t;
        if ((n&1)==0) return (float)polynomial;
        return -(float)polynomial;
    }
    if (x!=x) return __libm_qnan_f;
    return D_800AE8D0;
}
