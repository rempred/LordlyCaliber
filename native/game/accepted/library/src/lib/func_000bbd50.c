typedef signed int s32;
typedef unsigned int u32;
typedef unsigned char u8;

s32 func_000bbd50(s32 arg0)
{
    u32 index;
    s32 result;

    index = arg0 & 0xFF;
    if (index >= 0x51U) {
        result = 0;
    } else {
        result = *(u8 *)(0x801EF2CC + (index * 2)) & 0xF;
    }
    return result;
}
