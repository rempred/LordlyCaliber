typedef unsigned char u8;
typedef unsigned int u32;
typedef signed int s32;
typedef struct { u32 flags; u8 source_index; } RuntimeUnit;

extern u8 D_801969BA[][11];
extern s32 func_0012E968(RuntimeUnit *unit);

s32 func_0012E8EC(RuntimeUnit *unit)
{
    s32 row_index = func_0012E968(unit);
    s32 count = 0;
    const u8 *cursor = D_801969BA[row_index];
    const u8 *end = cursor + 5;
    /* Retail compares these five-byte row cursors as signed 32-bit addresses. */
    do {
        count += *cursor++ != 0xFF;
    } while ((s32)cursor < (s32)end);
    return count;
}
