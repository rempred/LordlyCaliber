#include "game/chapter_card.h"

extern const u8 data_002867A0[];

void func_00282004(s32 slot)
{
    CombatDrawCommand *command = gTitleCardDisplayListCursor;

    /* Keep the volatile cursor advances for all four commands. */
    gTitleCardDisplayListCursor = command + 1;
    command[0].first = 0xDC080008;
    command[0].second = (u32)data_002867A0;
    gTitleCardDisplayListCursor = command + 2;
    command[1].first = 0xE7000000;
    command[1].second = 0;
    gTitleCardDisplayListCursor = command + 3;
    command[2].first = 0xED000000;
    command[2].second = 0x004FC3BC;
    gTitleCardDisplayListCursor = command + 4;
    command[3].first = 0xE7000000;
    command[3].second = 0;

    func_0028A534();
}
