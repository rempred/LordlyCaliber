typedef signed int s32;
typedef unsigned int u32;
typedef unsigned char u8;

u8 func_000bbd80(s32 arg0)
{
    u32 index;
    u8 result;

    index = arg0 & 0xFF;
    if (index >= 0x51U) {
        result = 0;
    } else {
        result = *(u8 *)(0x801EF2CD + (index * 2));
    }
    return result;
}
