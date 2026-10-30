#include "common/types.h"

extern u8 D_8018FC6C;
extern int func_00051400(u8 **cursor);
extern int func_00050620(u8 **cursor);

void func_00050918(u8 *input, u8 *output)
{
    int saved = D_8018FC6C;
    struct {
        u8 *input;
        u8 unknown_04[12];
    } cursor;
    int value;

    /* The cursor's observed home is sp+0x10. This local carrier preserves
     * the retail frame; its unused bytes do not establish parser fields. */
    cursor.input = input;
    if (saved == 1) {
        D_8018FC6C = 0;
    }
    while (*cursor.input != 0) {
        if (*cursor.input == '{') {
            cursor.input++;
            *output++ = func_00051400(&cursor.input) - 1;
            cursor.input++;
        } else {
            value = (short)func_00050620(&cursor.input);
            if (value >= 0) {
                *output = value;
            } else {
                *output = 0x30;
            }
            output++;
        }
    }
    *output = 0xFF;
    D_8018FC6C = saved;
}
