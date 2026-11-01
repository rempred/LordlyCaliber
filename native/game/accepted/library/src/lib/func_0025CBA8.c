#include "common/types.h"
#include "game/effect_presentation_p05.h"
#include "game/effect_presentation_p04.h"

/* Accessed numeric prefixes; these views do not define allocation extents. */



extern s32 func_0020C104(void *);
extern s32 func_0020C120(void *);
/* Preserve the observed table reloads across writes through child pointers. */

void func_0025CBA8(P05PairActor *actor, P05IntegerTriplet *values)
{
    u16 i;

    /* The loop bound uses the unsigned halfword; indexing uses its signed view. */
    for (i = 0; i < 3; i++) {
        if (actor->primary[(s16)i] != 0) {
            actor->primary[(s16)i]->value_1C =
                actor->secondary[(s16)i]->value_1C = values->value_00;
            actor->primary[(s16)i]->value_1E =
                actor->secondary[(s16)i]->value_1E = values->value_04;
            actor->primary[(s16)i]->value_20 =
                actor->secondary[(s16)i]->value_20 = values->value_08;
        }
    }

    if (func_0020C104(actor) != 0) {
        for (i = 0; i < 3; i++) {
            if (actor->primary[(s16)i] != 0) {
                s32 *offsets = p05_s32_801CEDFC + (s16)i * 2;

                actor->primary[(s16)i]->value_1C += offsets[0];
                actor->primary[(s16)i]->value_20 += offsets[1];
                actor->secondary[(s16)i]->value_1C += offsets[0];
                actor->secondary[(s16)i]->value_20 += offsets[1];
            }
        }
    } else if (func_0020C120(actor) != 0) {
        s32 first = -8;
        s32 second = 8;

        if (actor->value_5C < 0) {
            first = 8;
            second = -8;
        }
        if (actor->primary[1] != 0) {
            actor->primary[1]->value_1C += first;
            actor->primary[1]->value_20 += second;
            actor->secondary[1]->value_1C += first;
            actor->secondary[1]->value_20 += second;
        }
    }
}
