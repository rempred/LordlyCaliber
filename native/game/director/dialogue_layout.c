/* Portable geometry adaptation of accepted func_000E5968.c at Decomp
 * 8012b84a. Fixed-width fields replace target-global aliases; GNU statement
 * expressions become ordinary C. Text/archive/callback ownership is host-side.
 * Native signed-halfword arithmetic and expression order are retained. */
#include "dialogue_layout.h"
typedef uint8_t u8;
typedef int16_t s16;
typedef uint16_t u16;
typedef int32_t s32;
typedef uint32_t u32;
typedef LordlyDialogueLayout DialogueState;
#define DIALOGUE layout
#define FLAGS options->flags
#define CONTROL options->control
#define BELOW_LEFT(x)                                                                              \
    (!(CONTROL & 4)                                                                                \
         ? (DIALOGUE->style < 2 ? ((DIALOGUE->style == 0 && DIALOGUE->portraitSide == 0)           \
                                       ? (DIALOGUE->portrait >= 0 ? (x) < 56 : (x) < 8)            \
                                       : (x) < 8)                                                  \
                                : (x) < 7)                                                         \
         : ((x) < 0))
#define RIGHT_EDGE                                                                                 \
    (!(CONTROL & 4)                                                                                \
         ? (DIALOGUE->style < 2                                                                    \
                ? (DIALOGUE->style == 0                                                            \
                       ? (DIALOGUE->portraitSide                                                   \
                              ? DIALOGUE->columns * 7 - 1                                          \
                              : DIALOGUE->columns * 7 + (DIALOGUE->portrait >= 0 ? 47 : -1))       \
                       : DIALOGUE->columns * 7 - 1)                                                \
                : DIALOGUE->columns * 7 - 2)                                                       \
         : DIALOGUE->columns * 7 - 9)
#define LEFT_EDGE                                                                                  \
    (!(CONTROL & 4)                                                                                \
         ? (DIALOGUE->style < 2                                                                    \
                ? (DIALOGUE->style == 0                                                            \
                       ? (DIALOGUE->portraitSide ? 8 : (DIALOGUE->portrait >= 0 ? 56 : 8))         \
                       : 8)                                                                        \
                : 7)                                                                               \
         : 0)

unsigned lordly_dialogue_layout(LordlyDialogueLayout *layout, LordlyDialogueOptions *options,
                                uint16_t measuredWidth, uint16_t measuredLines) {
    s32 preset, value, left;
    s16 nearLeft, nearRight, outsideLeft;
    u16 width, currentX;
    DialogueState *geometry;
    unsigned warnings = 0;
    if (DIALOGUE->style == 1) {
        DIALOGUE->columns = 36;
        DIALOGUE->rows = 3;
    } else {
        if (DIALOGUE->columns == 0)
            DIALOGUE->columns = (u32)measuredWidth / 7U + 1;
        if (DIALOGUE->style == 0)
            DIALOGUE->rows = 3;
        else if (DIALOGUE->rows == 0)
            DIALOGUE->rows = measuredLines;
    }
    if (DIALOGUE->columns >= 37)
        DIALOGUE->columns = 36;
    if (options->portrait != 0) {
        DIALOGUE->portrait = options->portrait;
        if (DIALOGUE->portrait < 0) {
            DIALOGUE->portrait = -DIALOGUE->portrait;
            DIALOGUE->portraitVariant = 1;
        } else {
            DIALOGUE->portraitVariant = 0;
        }
    }
    FLAGS &= ~0x10;
    options->width = DIALOGUE->width =
        !(CONTROL & 4) ? (DIALOGUE->style < 2
                              ? (DIALOGUE->style == 0
                                     ? DIALOGUE->columns * 7 + (DIALOGUE->portrait >= 0 ? 67 : 19)
                                     : DIALOGUE->columns * 7 + 19)
                              : DIALOGUE->columns * 7 + 14)
                       : DIALOGUE->columns * 7;
    options->height = DIALOGUE->height =
        !(CONTROL & 4)
            ? (DIALOGUE->style < 2 ? 61
                                   : ((s32)DIALOGUE->rows * 14 + 14 +
                                      ((s32)DIALOGUE->rows - 1) * (DIALOGUE->style < 2 ? 3 : 2)))
            : (DIALOGUE->rows * 14 + (DIALOGUE->rows - 1) * (DIALOGUE->style < 2 ? 3 : 2));
    if (FLAGS & 4)
        DIALOGUE->portraitSide = 1;
    if (DIALOGUE->portrait >= 999) {
        warnings |= 1;
        DIALOGUE->portrait = -1;
    }
    if (!(CONTROL & 8)) {
        if (!(FLAGS & 0x40)) {
            preset = options->preset >> 4;
            DIALOGUE->targetY = (u16)(preset * 14 + ((s16)DIALOGUE->height / 2 + 19));
            preset = options->preset & 15;
            if (preset >= 8)
                warnings |= 2;
            if (preset < 4)
                DIALOGUE->targetX = (u16)(preset * 22 + ((s16)DIALOGUE->width / 2 + 28));
            else
                DIALOGUE->targetX = (u16)(((preset - 4) * 22 - (DIALOGUE->width - 211)) +
                                          ((s16)DIALOGUE->width / 2 + 16));
        } else {
            DIALOGUE->targetX = (s16)DIALOGUE->width / 2 + options->startX;
            DIALOGUE->targetY = (s16)DIALOGUE->height / 2 + options->startY;
        }
    } else {
        DIALOGUE->targetX = options->targetX;
        DIALOGUE->targetY = options->preset;
    }
    if (FLAGS & 0x80)
        DIALOGUE->targetX = 160;
    DIALOGUE->start.x = options->startX;
    DIALOGUE->start.y = options->startY;
    if ((DIALOGUE->start.x == 0 && DIALOGUE->start.y == 0) || DIALOGUE->style != 0) {
        DIALOGUE->start.x = DIALOGUE->targetX;
        DIALOGUE->start.y = DIALOGUE->targetY;
    }
    DIALOGUE->currentX = DIALOGUE->start.x;
    DIALOGUE->currentY = DIALOGUE->start.y;
    value = options->speed - 2;
    DIALOGUE->steps = (u16)(value > 0 ? value : 1);
    if (((s16)DIALOGUE->targetX - (s16)DIALOGUE->currentX > 0
             ? (s16)DIALOGUE->targetX - (s16)DIALOGUE->currentX
             : (s16)DIALOGUE->currentX - (s16)DIALOGUE->targetX) >=
        ((s16)DIALOGUE->targetY - (s16)DIALOGUE->currentY > 0
             ? (s16)DIALOGUE->targetY - (s16)DIALOGUE->currentY
             : (s16)DIALOGUE->currentY - (s16)DIALOGUE->targetY)) {
        value = (s16)DIALOGUE->targetX - (s16)DIALOGUE->currentX;
        DIALOGUE->steps =
            (u16)((value > 0 ? value : (s16)DIALOGUE->currentX - (s16)DIALOGUE->targetX) /
                  DIALOGUE->steps);
    } else {
        value = (s16)DIALOGUE->targetY - (s16)DIALOGUE->currentY;
        DIALOGUE->steps =
            (u16)((value > 0 ? value : (s16)DIALOGUE->currentY - (s16)DIALOGUE->targetY) /
                  DIALOGUE->steps);
    }
    if (DIALOGUE->steps == 0)
        DIALOGUE->steps = 1;
    geometry = DIALOGUE;
    width = geometry->width;
    {
        s32 halfWidth = (s16)width / 2;
        left = geometry->targetX - halfWidth;
    }
    currentX = geometry->currentX;
    {
        s32 innerLeft = left + 20;
        nearLeft = (s16)(currentX - innerLeft);
    }
    {
        s32 innerRight = width + left - 20;
        nearRight = (s16)(innerRight - currentX);
    }
    {
        s32 outerLeft = left - 12;
        outsideLeft = (s16)(currentX - outerLeft);
    }
    {
        s16 initialX = nearLeft;
        if (DIALOGUE->portraitSide == 0) {
            DialogueState *state = DIALOGUE;
            s32 x;
            state->pointerSide = 0;
            state->pointerX = initialX;
            x = DIALOGUE->pointerX;
            if (BELOW_LEFT(x) && nearLeft <= nearRight) {
                state = DIALOGUE;
                state->pointerX = outsideLeft;
                state->pointerSide = 1;
            }
        } else {
            DialogueState *state = DIALOGUE;
            s32 x;
            state->pointerSide = 1;
            state->pointerX = outsideLeft;
            x = DIALOGUE->pointerX;
            if (RIGHT_EDGE < x && nearLeft >= nearRight) {
                DialogueState *corrected = DIALOGUE;
                corrected->pointerX = initialX;
                corrected->pointerSide = 0;
            }
        }
    }
    {
        s32 x = DIALOGUE->pointerX;
        if (BELOW_LEFT(x))
            DIALOGUE->pointerX = LEFT_EDGE;
    }
    {
        s32 x = DIALOGUE->pointerX;
        if (RIGHT_EDGE < x) {
            DialogueState *state = DIALOGUE;
            state->pointerX =
                !(CONTROL & 4) ? (state->style < 2
                                      ? ((state->style == 0 && state->portraitSide == 0)
                                             ? state->columns * 7 + (state->portrait >= 0 ? 47 : -1)
                                             : state->columns * 7 - 1)
                                      : state->columns * 7 - 2)
                               : state->columns * 7 - 9;
        }
    }
    if (!(FLAGS & 8)) {
        DialogueState *state = DIALOGUE;
        state->pointerEdge = 0;
        state->pointerY = state->height - 6;
    } else {
        DIALOGUE->pointerY = -11;
        DIALOGUE->pointerEdge = 1;
    }
    return warnings;
}
