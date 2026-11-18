#include "common/types.h"

extern u8 *func_00283808(s32);
extern u8 rodata_0029a070[];

u8 *func_00296CE0(s16 subject)
{
    u8 *name;

    if (subject == 0xDC) {
        subject = 0x6D;
    }
    if (subject == 0xDD) {
        subject = 0x63;
    }
    name = func_00283808(subject);
    if (subject == 0x51) {
        if (name == 0) {
            name = func_00283808(0x52);
            if (name == 0) {
                name = func_00283808(0x53);
            }
        }
    }
    if (subject == 0x4D) {
        name = rodata_0029a070;
    } else if (subject == 0x64) {
        name = rodata_0029a070 + 8;
    }
    return name;
}
