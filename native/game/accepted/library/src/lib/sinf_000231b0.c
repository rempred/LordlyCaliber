extern const double D_800AE990[5];
extern const double D_800AE9B8, D_800AE9C0, D_800AE9C8;
extern const double D_800AE9D8, D_800AE9E0;
extern const float D_800AE9D0;
extern float __libm_qnan_f;
float sinf(float x)
{
    int exponent, n;
    double y, t, polynomial;
    /* The accepted target inspects the IEEE float representation in its argument slot. */
    exponent=*(int *)&x >> 22;
    exponent &= 0x1ff;
    if (exponent<0xff) {
        y=x;
        if (exponent>=0xe6) {
            t=y*y;
            polynomial=((D_800AE990[4]*t+D_800AE990[3])*t+D_800AE990[2])*t+D_800AE990[1];
            polynomial=y+y*t*polynomial;
            return (float)polynomial;
        }
        return x;
    }
    if (exponent<0x136) {
        y=x;
        t=y*D_800AE9B8;
        n=t>=0.0 ? (int)(t+D_800AE9D8) : (int)(t-D_800AE9E0);
        t=(double)n;
        y-=t*D_800AE9C0;
        y-=t*D_800AE9C8;
        t=y*y;
        polynomial=((D_800AE990[4]*t+D_800AE990[3])*t+D_800AE990[2])*t+D_800AE990[1];
        polynomial=y+y*t*polynomial;
        if ((n&1)==0) return (float)polynomial;
        return -(float)polynomial;
    }
    if (x!=x) return __libm_qnan_f;
    return D_800AE9D0;
}
