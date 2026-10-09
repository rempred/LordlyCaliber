typedef signed int s32;
typedef unsigned int u32;

u32 func_00201E9C(s32 arg0, u32 arg1)
{
    u32 result;

    if (arg0 == 3) {
        result = ((arg1 + 3) >> 2) << 2;
    } else if (arg0 == 2) {
        result = ((arg1 + 3) >> 2) << 2;
    } else if (arg0 == 1) {
        result = ((arg1 + 7) >> 3) << 3;
    } else if (arg0 != 0) {
        result = ((arg1 + 31) >> 5) << 5;
    } else {
        result = ((arg1 + 15) >> 4) << 4;
    }
    return result;
}
