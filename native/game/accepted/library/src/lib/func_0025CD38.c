#include "common/types.h"
#include "game/effect_presentation_p05.h"
#include "game/effect_presentation_p04.h"

/* Numeric views of the same child pairs used by CBA8. */


void func_0025CD38(P05PairActor *actor)
{
    u16 i;

    /* Keep the unsigned loop bound and signed indexed address view. */
    for (i = 0; i < 3; i++) {
        if (actor->primary[(s16)i] != 0) {
            actor->primary[(s16)i]->value_1C = actor->primary[(s16)i]->value_28;
            actor->primary[(s16)i]->value_1E = actor->primary[(s16)i]->value_2A;
            actor->primary[(s16)i]->value_20 = actor->primary[(s16)i]->value_2C;
            actor->secondary[(s16)i]->value_1C = actor->secondary[(s16)i]->value_28;
            actor->secondary[(s16)i]->value_1E = actor->secondary[(s16)i]->value_2A;
            actor->secondary[(s16)i]->value_20 = actor->secondary[(s16)i]->value_2C;
        }
    }
}
