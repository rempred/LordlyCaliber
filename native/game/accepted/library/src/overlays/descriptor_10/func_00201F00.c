typedef unsigned char u8;
typedef unsigned int u32;
typedef struct {
    u8 r;
    u8 g;
    u8 b;
    u8 a;
} Rgba201;
u32 func_00201F00(Rgba201 in) {
    Rgba201 c = in;
    u32 lum = c.r * 30 + c.g * 59 + c.b * 11;
    c.r = lum * 255 / 30000;
    c.g = lum / 150;
    c.b = lum * 130 / 30000;
    return *(u32 *)&c;
}
