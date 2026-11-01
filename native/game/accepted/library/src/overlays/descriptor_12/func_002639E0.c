#include "game/effect_geometry_e05_internal.h"

extern CombatDrawCommand D_80220250[];
extern CombatDrawCommand D_802202D0[];

void func_002639E0(E05DrawObject *object)
{
    switch (object->mode_10) {
    case 0: {
        CombatDrawCommand *command = D_800E9BA0;
        D_800E9BA0 = command + 1;
        command->first = 0xDE000000;
        command->second = (u32)D_80220250;
        break;
    }
    case 1: {
        CombatDrawCommand *command = D_800E9BA0;
        D_800E9BA0 = command + 1;
        command->first = 0xDE000000;
        command->second = (u32)D_802202D0;
        break;
    }
    default:
        /* The original dispatch sends unsupported values to a self-loop. */
        for (;;) {}
    }
    if (object->flags_00 & 2) {
        CombatDrawCommand *command = D_800E9BA0;
        D_800E9BA0 = command + 1;
        command->first = 0xE200001C;
        command->second = 0x004041C8;
    }
    if (object->flags_00 & 1) {
        CombatDrawCommand *command = D_800E9BA0;
        D_800E9BA0 = command + 1;
        command->first = 0xD9FFFFFF;
        command->second = 0x400;
    }
    {
        CombatDrawCommand *command = D_800E9BA0;
        D_800E9BA0 = command + 1;
        command->first = 0xE7000000;
        command->second = 0;
    }
}
