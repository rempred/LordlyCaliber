#include "game/cutscene_dialogue_handlers.h"

/* Window query entry in dialogue overlay 4. */
extern s32 func_000fa314(u8 windowId);

s32 func_002A0354(s32 operand)
{
    return (u8)func_000fa314((u8)operand);
}
