typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

u16 func_00202258(u16 color)
{
    u32 green = (color >> 3) & 0xF8;
    u32 red;
    u32 blue = (color << 2) & 0xF8;
    u32 alpha = color & 1;
    u32 maximum = (color >> 8) & 0xF8;

    if (maximum < green) {
        maximum = green;
    }
    if (maximum < blue) {
        maximum = blue;
    }
    if ((s32)maximum < 128) {
        red = (s32)(maximum * 112) / 128 + 24;
        green = (s32)(maximum * 112) / 128 + 28;
        blue = (s32)(maximum * 96) / 128 + 8;
    } else {
        red = (s32)(maximum * 80) / 128 + 56;
        green = (s32)(maximum * 80) / 128 + 60;
        blue = (s32)(maximum * 80) / 128 + 40;
    }
    red &= 0xFF;
    maximum = ((red & 0xF8) << 8) + ((green & 0xF8) << 3) + ((blue & 0xF8) >> 2) + alpha;
    return maximum;
}
