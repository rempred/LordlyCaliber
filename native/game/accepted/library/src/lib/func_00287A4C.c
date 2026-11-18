#include "game/chapter_card.h"

void func_00287A4C(void)
{
    s32 first = D_8023E58C;
    s32 second;

    /* Advance each counter, then wrap it at the payload length in words. */
    D_8023E58C = first + 322;
    if (first + 322 >= 19200) {
        D_8023E58C = first + (322 - 19200);
    }

    second = D_8023E590;
    D_8023E590 = second + 318;
    if (second + 318 >= 19200) {
        D_8023E590 = second + (318 - 19200);
    }
}
