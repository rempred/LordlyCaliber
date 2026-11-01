#include "game/effect_presentation_p06_internal.h"

struct P06Vec79A90;
struct P06State79A90;
u8 func_00279D58(struct P06Vec79A90 *first, struct P06Vec79A90 *second, u8 *state)
{
    func_00279A90((struct P06Vec79A90 *)(first), (struct P06Vec79A90 *)(second), (struct P06State79A90 *)state);
    return func_00279A90((struct P06Vec79A90 *)(first), (struct P06Vec79A90 *)(second), (struct P06State79A90 *)(state + 0x50));
}
