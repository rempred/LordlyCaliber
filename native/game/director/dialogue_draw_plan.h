#ifndef LORDLY_DIALOGUE_DRAW_PLAN_H
#define LORDLY_DIALOGUE_DRAW_PLAN_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Host rendering services emitted by the portable adaptation of accepted
 * func_000E6D90.c (8012b84a). Source coordinates retain the game's arithmetic. */
typedef enum {
    LORDLY_DDRAW_TRANSFORM,
    LORDLY_DDRAW_RESET,
    LORDLY_DDRAW_STYLE,
    LORDLY_DDRAW_REPEAT_FRAME,
    LORDLY_DDRAW_FRAME,
    LORDLY_DDRAW_SLICE,
    LORDLY_DDRAW_POINTER_ATLAS,
    LORDLY_DDRAW_RECTANGLE,
    LORDLY_DDRAW_TEXTURE_RECTANGLE,
    LORDLY_DDRAW_PORTRAIT,
    LORDLY_DDRAW_TEXT_RESET,
    LORDLY_DDRAW_TEXT_FORMAT,
    LORDLY_DDRAW_TEXT,
    LORDLY_DDRAW_SCISSOR,
    LORDLY_DDRAW_FINISH,
    LORDLY_DDRAW_RAW
} LordlyDialogueDrawService;
typedef void (*LordlyDialogueDrawSink)(void *, LordlyDialogueDrawService, const int32_t *, int);
/* Text argument zero is a payload-relative byte offset, never a host pointer.
 * -1 means a history/line reference exceeded the native 96-row storage. */
int lordly_dialogue_draw_plan(const uint8_t window[168], const uint8_t payload[1144],
                              LordlyDialogueDrawSink, void *);
#ifdef __cplusplus
}
#endif
#endif
