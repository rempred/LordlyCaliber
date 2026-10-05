/* NTSC display-control adaptation: ROM19E10 drain, 19FC0 blackout,
 * 1A050 request, 19974..19A50 completion. All are resident early-boot code.
 * The host owns graphics submission and VI presentation, not hardware timing. */
#include "display_state.h"
int lordly_display_black(LordlyDisplayState *s) {
    if (s->outstanding)
        return 0;
    s->reveal_request = 0;
    s->black = 1;
    return 1;
}
void lordly_display_reveal(LordlyDisplayState *s) {
    s->reveal_request = 128;
}
void lordly_display_complete(LordlyDisplayState *s, uint16_t event) {
    if (event != 4 && event != 8 && event != 16)
        return;
    if (event == 4 && (s->reveal_request & 128)) {
        s->black = 0;
        s->reveal_request = 1;
    }
    --s->outstanding;
}
