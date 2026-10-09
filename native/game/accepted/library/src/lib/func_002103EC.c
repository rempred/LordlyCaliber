#include "game/combat_draw.h"

extern void func_00023780(void *, unsigned int);

void func_002103EC(CombatDrawVertex *vertex, int xInput, int yInput, int uInput, int vInput)
{
    short x = xInput;
    short y = yInput;
    short u = uInput;
    short v = vInput;
    func_00023780(vertex, 16);
    vertex->x = x;
    vertex->y = y;
    vertex->u = (unsigned int)u << 6;
    vertex->v = (unsigned int)v << 6;
    vertex->field_0C[3] = 255;
}
