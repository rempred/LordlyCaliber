#ifndef LORDLY_DIALOGUE_LAYOUT_H
#define LORDLY_DIALOGUE_LAYOUT_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    uint16_t columns, rows, width, height, targetX, targetY, currentX, currentY, steps;
    int16_t portrait, pointerX, pointerY;
    uint8_t style, portraitSide, portraitVariant, pointerSide, pointerEdge;
    struct {
        uint16_t x, y;
    } start;
} LordlyDialogueLayout;
typedef struct {
    uint8_t flags, control, preset, speed;
    uint16_t portrait, startX, startY, targetX, width, height;
} LordlyDialogueOptions;
/* Portable adaptation of accepted func_000E5968 at 8012b84a, after the
 * leading-control parser. Width/line measurements retain separate grammars.
 * Returns diagnostic bits: 1=portrait>=999, 2=horizontal preset>=8. */
unsigned lordly_dialogue_layout(LordlyDialogueLayout *state, LordlyDialogueOptions *options,
                                uint16_t measuredWidth, uint16_t measuredLines);
#ifdef __cplusplus
}
#endif
#endif
