#include "game/effect_geometry_e05.h"
#include "game/effect_handlers_p07.h"

typedef struct P07Entry72B8C {
    void *value_00;
    u8 unknown_04[0x2C];
    /* Preserve the observed reload after each update; the original qualifier is unknown. */
    volatile u16 value_30, value_32;
    u16 value_34, value_36, value_38;
} P07Entry72B8C;
typedef struct P07State72B8C {
    u8 unknown_00[0x58];
    P07Entry72B8C **value_58;
} P07State72B8C;
extern s32 p07_data_80220AAC[];

void func_00272B8C(u16 message, P07State72B8C *state)
{
    u32 i;
    /* Reuse the consumed dispatch word for the per-entry address below.
     * This keeps GCC 2.7.2's pointer in A0; a separate pointer local takes V1.
     * Only the low halfword participates in dispatch. */
    u32 value = message;
    switch ((u16)value) {
    case 0: {
        P07Entry72B8C *entry;
        u16 first, second;
        for (i = 0; i < 2; i++) {
            state->value_58[p07_data_80220AAC[i]]->value_00 = (void *)func_00263F40;
            entry = state->value_58[p07_data_80220AAC[i]];
            entry->value_30 = 0;
            first = entry->value_30;
            entry->value_32 = 0;
            second = entry->value_32;
            entry->value_38 = 0;
            entry->value_34 = first + 31;
            entry->value_36 = second + 31;
        }
        break;
    }
    case 2: {
        u16 first, second;
        for (i = 0; i < 2; i++) {
            value = (u32)state->value_58[p07_data_80220AAC[i]];
            first = ((P07Entry72B8C *)value)->value_30;
            second = ((P07Entry72B8C *)value)->value_32;
            first += 2;
            ((P07Entry72B8C *)value)->value_30 = first;
            first = ((P07Entry72B8C *)value)->value_30;
            second += 4;
            ((P07Entry72B8C *)value)->value_32 = second;
            second = ((P07Entry72B8C *)value)->value_32;
            first &= 31;
            ((P07Entry72B8C *)value)->value_30 = first;
            first = ((P07Entry72B8C *)value)->value_30;
            second &= 31;
            ((P07Entry72B8C *)value)->value_32 = second;
            ((P07Entry72B8C *)value)->value_34 = first + 31;
            ((P07Entry72B8C *)value)->value_36 = ((P07Entry72B8C *)value)->value_32 + 31;
        }
        break;
    }
    case 1:
        break;
    }
}
