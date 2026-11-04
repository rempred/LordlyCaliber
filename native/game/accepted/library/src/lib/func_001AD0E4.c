#include "game/scenario_item_views.h"
typedef struct { u32 w0, w1; } ItemGfx;
extern u32 D_800E9BA0;
extern s32 D_801F3658;
extern u8 D_80210258[], D_80210298[], D_80210DA8[], D_80213648[];
extern u8 D_8021DF80[], D_8021DF94[], D_8021DFA0[], D_8021DFBC[];
extern u8 D_8021DFCC[], D_8021DFD8[], D_8021DFE8[], D_8021DFF0[];
extern u8 D_8021DFF8[], D_8021E000[], D_8021E008[], D_8021E010[];
extern u8 D_8021E018[], D_8021E024[], D_8021E030[];
extern void func_802027DC(void);
extern void func_802028A4(s32, s32, u16, u16);
extern void func_802012D8(void *);
extern void func_80200DD0(s32, s32, s32);
extern void func_802001A8(s32, s32, u32, s32);
extern void func_80200F70(void *, void *, s32, s32, s32);
extern void func_80179034(void *, s32, s32);
extern u16 func_80201A44(u8, s32, u8);
#define CLIP4(v) (((u32)(v) << 2) & (u32)(~(s32)(s16)((u32)(v) << 2) >> 31) & 0xFFC)
#define COMMAND(a,b) { ItemGfx *g = (ItemGfx *)D_800E9BA0; D_800E9BA0 = (u32)(g+1); g->w0=(u32)(a); g->w1=(u32)(b); }

void func_001AD0E4(u16 item, s32 x, s32 y)
{
    u32 member = D_801971F2[D_801F3658].members[D_8021E3AA.field00];
    u16 maximum, current, cap, next;
    s32 eligible, invalid;
    u32 displayed_next;
    s32 i, row_y, heal_y;
    u32 stat_member, masked_member;
    switch (item) {
    case 1: case 2:
        eligible = 1;
        func_802027DC();
        if ((u8)member < 100) {
            maximum = D_80193BC0[(u8)member].field16;
            current = D_80193BC0[(u8)member].field18;
            cap = maximum;
            {
                s32 usable = current != 0;
                if (D_80193BC0[(u8)member].field33 & 4)
                    usable = 0;
                eligible = usable;
            }
        } else {
            u8 segments;
            u16 total, third;
            current = *(u16 *)(0x80190EBCU + (u8)member * 2);
            segments = func_00040FF4(0, member);
            total = *(u16 *)0x80190F82;
            third = total / 3;
            cap = third * segments;
            if (segments == 3)
                cap += total % 3;
            maximum = total;
        }
        invalid = eligible ^ 1;
        invalid |= ((u16)current == 0);
        if (invalid) {
            func_80179034(D_8021DF80, x + 0x10, y + 0x46);
        } else {
            func_802028A4(x + 6, y + 0x50, (u16)current, maximum);
            func_802012D8(D_80210258);
            func_80200DD0(x + 0x30, y + 0x5A, 0x3E);
            if (item == 1) {
                if ((u16)current + 100 > (u16)cap)
                    next = cap;
                else
                    next = current + 100;
            } else {
                if ((u16)current + 300 > (u16)cap)
                    next = cap;
                else
                    next = current + 300;
            }
            heal_y = y + 0x64;
            func_80200DD0(x + 0x20, heal_y, 2);
            displayed_next = next;
            if ((u16)current < displayed_next)
                func_802012D8(D_80210298);
            func_802001A8(x + 0x34, heal_y, displayed_next, -3);
            func_80200DD0(x + 0x4A, heal_y, 7);
            func_802012D8(D_80210258);
            func_802001A8(x + 0x52, heal_y, maximum, -3);
            func_80179034(D_8021DF94, x + 0x1C, y + 0x3C);
        }
        break;
    case 6:
        if (func_001AD8CC(item, member, 0))
            func_80179034(D_8021DFA0, x + 0x1C, y + 0x46);
        else
            func_80179034(D_8021DF80, x + 0x10, y + 0x46);
        break;
    case 7:
        if (func_001AD8CC(item, member, 0))
            func_80179034(D_8021DFBC, x + 0x1C, y + 0x46);
        else
            func_80179034(D_8021DF80, x + 0x10, y + 0x46);
        break;
    case 9: case 10: case 11: case 12:
    case 13: case 14: case 15: case 16:
        if (member >= 100) {
            func_80179034(D_8021DF80, x + 0x10, y + 0x46);
            break;
        }
        COMMAND(0xDE000000, D_80213648);
        func_80200F70(D_80210258, D_80210DA8, 0x20, 0x30, 0);
        i = 0;

        do {
            s32 row = 0x56 + i * 10;
            ItemGfx *g = (ItemGfx *)D_800E9BA0;
            ItemGfx *texture;
            u32 texture_word, texture_y, texture_y_field;
            s32 wrapped_x, scaled_x;
            D_800E9BA0 = (u32)(g + 1);
            g[0].w0 = 0xE7000000;
            g[0].w1 = 0;
            D_800E9BA0 = (u32)(g + 2);
            /* Keep the command-byte and lower-coordinate field mask: this
             * compiler otherwise reassociates the ORs and hoists the opcode. */
            g[1].w0 = (CLIP4(x + 0x1B) << 12) | ((CLIP4(y + row + 8) | 0xE4000000) & 0xFF000FFFU);
            wrapped_x = (s16)((u32)(x + 6) << 2);
            g[1].w1 = (CLIP4(x + 6) << 12) | CLIP4(y + row);
            scaled_x = (s32)((u32)wrapped_x << 10);
            texture = g + 2;
            D_800E9BA0 = (u32)(g + 3);
            texture->w0 = 0xE1000000;
            texture_y = (u32)i << 8;
            if (wrapped_x < 0) {
                s32 scaled = scaled_x >> 7;
                if (scaled > 0) scaled = 0;
                texture_word = (u32)(0 - scaled) << 16;
            } else {
                texture_word = 0;
            }
            /* Separate branch stores preserve the cursor reload after clipping. */
            if ((s32)((u32)(y + row) << 2) < 0) {
                s32 scaled = (s32)((u32)(y + row) << 18) >> 13;
                if (scaled > 0) scaled = 0;
                texture_y_field = (u16)(texture_y - scaled);
                texture->w1 = texture_word | texture_y_field;
            } else {
                texture_y_field = (u16)texture_y;
                texture->w1 = texture_word | texture_y_field;
            }

            COMMAND(0xF1000000, 0x04000400);
            COMMAND(0xE7000000, 0);

            i++;
        } while (i < 6);
        func_802027DC();
        func_80200DD0(x + 6, y + 0x42, 1);
        {
            s32 number_x = D_80193BC0[member].field13 < 10 ? x + 0x30 : x + 0x2D;
            u32 shown_field;
            masked_member = member & 0xFF;
            shown_field = D_80193BC0[masked_member].field13;

            func_802001A8(number_x, y + 0x42, shown_field, 0);

        }
        {
            u16 shown_current = D_80193BC0[masked_member].field18;
            u16 shown_maximum = D_80193BC0[masked_member].field16;
            func_802028A4(x + 6, y + 0x4C, shown_current, shown_maximum);
        }
        i = 0;
        stat_member = masked_member;
        row_y = 0x56;
        do {
            current = func_00153898(0, stat_member, i);
            current += func_80201A44(0, stat_member, i);
            if (current >= 1000)
                current = 999;
            func_802001A8(x + 0x26, y + row_y, current, -3);
            i++;
            row_y += 10;
        } while (i < 6);
        if (item == 9) {
            s32 arrow_y, second_arrow_y;
            i = 0;
            second_arrow_y = 0x42;
            arrow_y = 0x41;
            do {
                func_80200DD0(x + 0x52, y + arrow_y, 0x3C);
                func_80200DD0(x + 0x5B, y + second_arrow_y, 0x3A);
                second_arrow_y += 10;
                i++;
                arrow_y += 10;
            } while (i < 8);
        } else {
            s32 offset = (item - 9) * 10;
            s32 first_y = offset + 0x41;
            func_80200DD0(x + 0x52, y + first_y, 0x3C);
            offset += 0x42;
            func_80200DD0(x + 0x5B, y + offset, 0x3A);
        }
        switch (item) {
        case 9: func_80179034(D_8021DFCC, x + 10, y + 0x2E); break;
        case 10: func_80179034(D_8021DFD8, x + 10, y + 0x2E); break;
        case 11: func_80179034(D_8021DFE8, x + 20, y + 0x2E); break;
        case 12: func_80179034(D_8021DFF0, x + 20, y + 0x2E); break;
        case 13: func_80179034(D_8021DFF8, x + 20, y + 0x2E); break;
        case 14: func_80179034(D_8021E000, x + 20, y + 0x2E); break;
        case 15: func_80179034(D_8021E008, x + 20, y + 0x2E); break;
        case 16: func_80179034(D_8021E010, x + 20, y + 0x2E); break;
        }
        break;
    case 17:
        if (member >= 100)
            func_80179034(D_8021DF80, x + 0x10, y + 0x46);
        else
            func_80179034(D_8021E018, x + 10, y + 0x46);
        break;
    case 18:
        if (member >= 100)
            func_80179034(D_8021DF80, x + 0x10, y + 0x46);
        else
            func_80179034(D_8021E024, x + 10, y + 0x46);
        break;
    case 19:
        if (member >= 100)
            func_80179034(D_8021DF80, x + 0x10, y + 0x46);
        else
            func_80179034(D_8021E030, x + 0x22, y + 0x46);
        break;
    }
}

