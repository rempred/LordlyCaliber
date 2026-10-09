typedef unsigned char u8;
typedef unsigned int u32;
typedef signed int s32;

extern u8 D_80186FBC[];

s32 func_00040f88(u8 first, u8 second, u8 third, u8 fourth, u8 fifth)
{
    u8 values[5];
    u32 order_index;
    s32 slot;

    values[0] = first;
    values[1] = second;
    values[2] = third;
    values[3] = fourth;
    values[4] = fifth;
    for (order_index = 0; order_index < 7; order_index++) {
        const u8 *cursor = values;
        for (slot = 0; slot < 5; slot++) {
            /* A word temporary avoids an extra byte mask in this leaf. */
            u32 value = *cursor++;
            if (value == D_80186FBC[order_index]) {
                return value;
            }
        }
    }
    return 7;
}
