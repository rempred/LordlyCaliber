typedef signed int s32;
typedef unsigned int u32;

u32 func_00202198(u32 color)
{
    s32 green = (color >> 16) & 0xFF;
    s32 red;
    s32 blue = (color >> 8) & 0xFF;
    u32 alpha = color & 0xFF;
    s32 maximum = color >> 24;

    if (maximum < green) {
        maximum = green;
    }
    if (maximum < blue) {
        maximum = blue;
    }
    if (maximum < 128) {
        red = maximum * 112 / 128 + 24;
        green = maximum * 112 / 128 + 28;
        blue = maximum * 96 / 128 + 8;
    } else {
        red = maximum * 80 / 128 + 56;
        green = maximum * 80 / 128 + 60;
        blue = maximum * 80 / 128 + 40;
    }
    red <<= 8;
    return (((u32)red << 16) + ((u32)green << 16) + ((u32)blue << 8)) | alpha;
}
