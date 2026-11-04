#include "game/scenario_item_views.h"
#define CHARACTER_BYTE(id, offset) (*(u8 *)(0x80193BC0U + (id) * 56 + (offset)))
#define CHARACTER_HALF(id, offset) (*(u16 *)(0x80193BC0U + (id) * 56 + (offset)))

u8 func_001AD8CC(u16 item, u8 member, s32 group)
{
    u8 eligible = 1;
    u8 *cursor;
    u8 *end;
    switch (item) {
    case 1:
    case 2:
        if (member < 100) {
            u16 current = CHARACTER_HALF(member, 0x18);
            eligible = 0;
            if (current != 0 && !(CHARACTER_BYTE(member, 0x33) & 4))
                eligible = current < CHARACTER_HALF(member, 0x16);
        } else {
            u8 segments = func_00040FF4(0, member);
            u16 total = D_80190F82.entries[0];
            u16 third = total / 3;
            s32 maximum = third * segments;
            u16 current;
            if (segments == 3)
                maximum += (u16)(total % 3);
            current = D_80190F82.entries[member - 99];
            eligible = 0;
            if (current != 0 && !(*(u8 *)(0x80191010U + member) & 4))
                eligible = current < maximum;
        }
        break;
    case 3:
        cursor = D_801971F2[group].members;
        end = cursor + 5;
        do {
            if (*cursor != 0) {
                eligible = func_001AD8CC(1, *cursor, 0);
                if (eligible != 0)
                    break;
            }
            cursor++;
        } while ((s32)cursor < (s32)end);
        break;
    case 6:
        eligible = (*(u8 *)0x8021E3C8 >> 2) & 1;
        break;
    case 7:
        eligible = *(s32 *)0x8021E3C4 == 0;
        break;
    case 9: case 10: case 11: case 12:
    case 13: case 14: case 15: case 16:
        if (member < 100) {
            if (item == 9)
                eligible = CHARACTER_BYTE(member, 0x13) < 99;
            else if (item == 10)
                eligible = CHARACTER_HALF(member, 0x16) < 999;
            else if (func_00153898(0, member, item - 11) >= 999)
                return 0;
        } else {
            return 0;
        }
        break;
    case 17:
        eligible = 0;
        if (member < 100)
            eligible = CHARACTER_BYTE(member, 0x1B) < 100;
        break;
    case 18:
        eligible = 0;
        if (member < 100)
            eligible = CHARACTER_BYTE(member, 0x1B) != 0;
        break;
    case 19:
        eligible = member < 100;
        break;
    }
    return eligible;
}



