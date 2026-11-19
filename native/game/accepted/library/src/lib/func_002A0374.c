#include "game/cutscene_dialogue_handlers.h"

/* Window resume entry in dialogue overlay 4. */
extern void func_000fa460(u8 windowId);

void func_002A0374(s32 operand)
{
    func_000fa460((u8)operand);
}
