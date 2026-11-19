#include "game/dialogue_text.h"

typedef struct CombatDrawCommand {
    u32 first;
    u32 second;
} CombatDrawCommand;

/* Preserve the observed intermediate cursor updates. */
extern CombatDrawCommand *volatile D_800E9BA0;

#define COMBAT_DRAW_FIELD(value, shift, width) \
    (((u32)(value) & ((1u << (width)) - 1)) << (shift))

#define COMBAT_DRAW_PAIR(a, b) { \
    CombatDrawCommand *command = D_800E9BA0++; \
    command->first = (a); \
    command->second = (b); \
}

extern u8 D_8018FC78;
extern s16 D_80190F60;
extern s16 D_80190F64;
extern s16 D_80190F66;
extern u8 *D_80190F68;
extern u8 D_80190F6D;
extern u8 D_80190F6E;
extern s16 D_80190F76;
extern u16 D_80190C88[][4];

extern u8 func_00042c30(void);
extern u8 func_0004f2e4(u8 code, u8 small);
extern s32 func_00050620(u8 **text);
extern void *func_000507e8(s32 glyph);
extern s32 func_00050ce8(s32 glyph);

#define RECT_X(x) COMBAT_DRAW_FIELD(x, 12, 12)
#define RECT_Y(y) COMBAT_DRAW_FIELD(y, 0, 12)

s32 func_0004f86c(u8 *text, s32 *loadedPalette)
{
    s32 handledGlyph = 0;

    while (*text != 0) {
        s32 glyph;
        void *image;
        s32 small;
        s32 width;
        s32 height;
        s32 right, bottomOrTextureStep;

        if (text == D_80190F68) {
            break;
        }

        D_80190F76 = func_0004f2e4(*text, D_8018FC78);
        glyph = (s16)func_00050620(&text);
        if (glyph < 0 || D_80190F64 >= 320 || D_80190F66 >= 240) {
            goto advance;
        }

        image = func_000507e8(glyph);
        {
            CombatDrawCommand *draw = D_800E9BA0;

            D_800E9BA0 = draw + 1;
            draw[0].first = 0xE7000000;
            draw[0].second = 0;
            if (D_80190F6E == 0 && D_80190F6D != *loadedPalette) {
                u8 palette = D_80190F6D;

                D_800E9BA0 = draw + 2;
                draw[1].first = 0xFD100000;
                draw[1].second = (u32)D_80190C88[palette];
                D_800E9BA0 = draw + 3;
                draw[2].first = 0xE8000000;
                draw[2].second = 0;
                D_800E9BA0 = draw + 4;
                draw[3].first = 0xF5000100;
                draw[3].second = 0x07000000;
                D_800E9BA0 = draw + 5;
                draw[4].first = 0xE6000000;
                draw[4].second = 0;
                D_800E9BA0 = draw + 6;
                draw[5].first = 0xF0000000;
                draw[5].second = 0x0703C000;
                D_800E9BA0 = draw + 7;
                draw[6].first = 0xE7000000;
                draw[6].second = 0;
                *loadedPalette = palette;
            }
        }

        small = func_00050ce8(glyph);
        {
            CombatDrawCommand *draw = D_800E9BA0;

            D_800E9BA0 = draw + 1;
            draw[0].first = 0xFD500000;
            draw[0].second = (u32)image;
            D_800E9BA0 = draw + 2;
            draw[1].first = 0xF5500000;
            draw[1].second = small == 0 ? 0x07010040 : 0x0700C040;
        }
        {
            CombatDrawCommand *draw = D_800E9BA0;

            D_800E9BA0 = draw + 1;
            draw[0].first = 0xE6000000;
            draw[0].second = 0;
            D_800E9BA0 = draw + 2;
            draw[1].first = 0xF3000000;
            draw[1].second = small == 0 ? 0x07037800 : 0x0701F800;
        }
        {
            CombatDrawCommand *draw = D_800E9BA0;

            D_800E9BA0 = draw + 1;
            draw[0].first = 0xE7000000;
            draw[0].second = 0;
            D_800E9BA0 = draw + 2;
            draw[1].first = 0xF5400200;
            draw[1].second = small == 0 ? 0x00010040 : 0x0000C040;
        }
        COMBAT_DRAW_PAIR(0xF2000000, small == 0 ? 0x0003C034 : 0x0003C01C);
        if (small == 0) {
            width = 11;
            height = 14;
        } else {
            height = 8;
            width = 8;
        }

        if (func_00042c30() == 0) {
            right = (D_80190F64 + width) * 4;
            if (right < 0) {
                goto doneGlyph;
            }
            /* Keep the original signed-bound branch scheduling. */
            do {
                bottomOrTextureStep = (D_80190F66 + height) * 4;
            } while (0);
            if (bottomOrTextureStep < 0) {
                goto doneGlyph;
            }
            if (D_80190F64 * 4 >= 1280 || D_80190F66 * 4 >= 960) {
                goto doneGlyph;
            }
            {
                /* This plain read lets KMC schedule the extent copy in the clip guard. */
                CombatDrawCommand *draw = *(CombatDrawCommand **)&D_800E9BA0;
                CombatDrawCommand *rect;
                s32 clipRight = right;
                s32 clipBottom;
                u32 rightField;

                D_800E9BA0 = draw + 1;
                draw[0].first = 0xE7000000;
                draw[0].second = 0;
                rect = draw + 1;
                D_800E9BA0 = draw + 2;
                if (clipRight > 1280) {
                    clipRight = 1280;
                }
                rightField = RECT_X(clipRight);
                clipBottom = bottomOrTextureStep;
                if (clipBottom > 960) {
                    clipBottom = 960;
                }
                rect->first = rightField | ((RECT_Y(clipBottom) | 0xE4000000) & 0xFF000FFFu);
                rect->second =
                    (D_80190F64 * 4 < 0 ? 0 : RECT_X(D_80190F64 * 4)) |
                    (D_80190F66 * 4 < 0 ? 0 : RECT_Y(D_80190F66 * 4));
            }
            COMBAT_DRAW_PAIR(0xE1000000,
                (D_80190F64 * 4 < 0 ? (u32)-(D_80190F64 * 4) << 19 : 0) |
                (D_80190F66 * 4 < 0 ? ((-(D_80190F66 * 4)) * 8) & 0xFFFF : 0));
            {
                CombatDrawCommand *draw = D_800E9BA0;

                D_800E9BA0 = draw + 1;
                draw[0].first = 0xF1000000;
                draw[0].second = 0x04000400;
                D_800E9BA0 = draw + 2;
                draw[1].first = 0xE7000000;
                draw[1].second = 0;
            }
        } else {
            s32 left;
            s32 top;
            u32 lowerRight;
            u32 upperLeft;
            CombatDrawCommand *draw = D_800E9BA0;

            left = D_80190F64;
            top = D_80190F66;
            D_800E9BA0 = draw + 1;
            D_800E9BA0 = draw + 2;
            {
                u32 lowerX = RECT_X((left + width) * 8);
                u32 lowerY = RECT_Y((top + height) * 8) | 0xE4000000;
                /* The two scale paths share this temporary for KMC allocation. */
                bottomOrTextureStep = 0x02000200;

                lowerX |= lowerY;
                /* Keep the packed corner live across KMC's scheduling boundary. */
                do {
                    lowerRight = lowerX;
                } while (0);
            }
            upperLeft = RECT_X(left * 8) | RECT_Y(top * 8);
            draw[0].first = 0xE7000000;
            draw[0].second = 0;
            draw[1].first = lowerRight;
            draw[1].second = upperLeft;
            D_800E9BA0 = draw + 3;
            draw[2].first = 0xE1000000;
            draw[2].second = 0;
            D_800E9BA0 = draw + 4;
            draw[3].first = 0xF1000000;
            draw[3].second = bottomOrTextureStep;
            D_800E9BA0 = draw + 5;
            draw[4].first = 0xE7000000;
            draw[4].second = 0;
        }

    doneGlyph:
        handledGlyph = 1;
    advance:
        D_80190F64 += D_80190F76 + D_80190F60;
    }
    return handledGlyph;
}
