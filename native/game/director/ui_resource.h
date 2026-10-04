#ifndef LORDLY_UI_RESOURCE_H
#define LORDLY_UI_RESOURCE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { LORDLY_UI_SLOTS = 6, LORDLY_UI_RECORD_BYTES = 168 };
typedef struct { uint8_t bytes[LORDLY_UI_RECORD_BYTES]; } LordlyUiRecord;
typedef struct {
    LordlyUiRecord records[LORDLY_UI_SLOTS];
    int32_t margin, context;
    uint16_t parent;
} LordlyUiPool;
/* ROM735C..745C: preserves all bytes except the nine registration fields.
 * Native exhaustion loops forever; the host returns -1 without writes. */
int lordly_ui_register(LordlyUiPool *, uint16_t flags, uint32_t initializer,
                      uint16_t x, uint16_t y, uint16_t right, uint16_t bottom,
                      uint16_t priority);
/* ROMFA48..FC44: window registration and the original signed clipping rules. */
int lordly_ui_window(LordlyUiPool *, uint32_t initializer, uint16_t x, uint16_t y,
                    uint16_t kind, uint16_t style);
#ifdef __cplusplus
}
#endif
#endif
