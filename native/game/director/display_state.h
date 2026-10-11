#ifndef LORDLY_DISPLAY_STATE_H
#define LORDLY_DISPLAY_STATE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    uint32_t reveal_request, outstanding;
    uint8_t black;
} LordlyDisplayState;
int lordly_display_black(LordlyDisplayState *);
void lordly_display_reveal(LordlyDisplayState *);
void lordly_display_complete(LordlyDisplayState *, uint16_t event);
#ifdef __cplusplus
}
#endif
#endif
