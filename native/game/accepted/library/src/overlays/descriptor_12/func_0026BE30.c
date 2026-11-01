#include "game/effect_geometry_e05.h"
#include "game/effect_dispatch_e07.h"

typedef struct Func0026BE30Record {
    void *value_00;
    u8 unknown_04[0x2C];
    u16 value_30;
    u16 value_32;
    u16 value_34;
} Func0026BE30Record;

typedef struct Func0026BE30Entry {
    u32 unknown_00;
    u32 count_04;
} Func0026BE30Entry;

typedef union Func0026BE30Pair {
    u32 as_word;
    struct {
        u16 value_48;
        u16 flags_4A;
    } parts;
} Func0026BE30Pair;

typedef struct Func0026BE30Fields {
    u8 unknown_00[0x44];
    u16 value_44;
    u16 value_46;
    Func0026BE30Pair pair_48;
    u8 unknown_4C[4];
    Func0026BE30Entry *entry_50;
    void *records_54;
    Func0026BE30Record **slots_58;
} Func0026BE30Fields;

/* This is the field view used here, not the full allocation extent. */
typedef struct Func0026BE30State {
    u8 unknown_00[0x1C];
    Func0026BE30Fields fields_1C;
} Func0026BE30State;

struct Func00269470State;

/* These slots store callback addresses; shared declarations retain the
 * recovered input views of the complete providers. */
u32 func_0026BE30(u16 command, Func0026BE30State *state)
{
    u32 result = func_00269470(command, (struct Func00269470State *)state);
    Func0026BE30Fields *fields;
    Func0026BE30Entry *entry;
    Func0026BE30Record **slots;
    u32 item;

    switch (command) {
    case 0: {
        s32 in_range;

        entry = state->fields_1C.entry_50;
        item = 0;
        in_range = item < entry->count_04;
        slots = state->fields_1C.slots_58;
        /* Preserve the initial range test across the one-time view setup. */
        fields = &state->fields_1C;
        if (in_range) {
            for (; item < entry->count_04; item++, slots++) {
                Func0026BE30Record *record = *slots;

                if (record) {
                    if ((fields->pair_48.as_word & 0xA) == 0xA) {
                        record->value_00 = (void *)func_00267668;
                    } else if (fields->pair_48.parts.flags_4A & 2) {
                        record->value_00 = (void *)func_002675B4;
                    } else {
                        record->value_00 = (void *)func_0026756C;
                    }
                }
            }
        }
        fields->value_44 = fields->value_46 = fields->pair_48.parts.value_48 = 0xFF;
        break;
    }

    case 4: {
        fields = &state->fields_1C;
        entry = fields->entry_50;
        slots = fields->slots_58;

        if (fields->pair_48.parts.flags_4A & 8) {
            for (item = 0; item < entry->count_04; item++, slots++) {
                Func0026BE30Record *record = *slots;

                if (record) {
                    record->value_30 = fields->value_44;
                    record->value_32 = fields->value_46;
                    record->value_34 = fields->pair_48.parts.value_48;
                }
            }
        }
        break;
    }
    }
    return result;
}
