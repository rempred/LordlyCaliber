typedef signed int s32;
typedef unsigned int u32;

s32 func_00201D8C(u32 value)
{
    if (value < 2U) {
        return 1;
    }
    if (value < 5U) {
        return 2;
    }
    if (value < 9U) {
        return 3;
    }
    if (value < 0x11U) {
        return 4;
    }
    if (value < 0x21U) {
        return 5;
    }
    if (value < 0x41U) {
        return 6;
    }
    if (value < 0x81U) {
        return 7;
    }
    if (value < 0x101U) {
        return 8;
    }
    if (value < 0x201U) {
        return 9;
    }
    return 10;
}
