#include "game/effect_runtime_e03_internal.h"
#include "game/combat_draw_commands.h"

typedef struct E03MatrixBlock {
    u8 bytes[0x40];
} E03MatrixBlock;

extern E03MatrixBlock *D_801969B0;
extern void func_000225d0(void *matrix, float left, float right, float bottom,
                          float top, float near, float far, float scale);
extern void guMtxIdent(void *matrix);

void func_00262F50(u16 value)
{
    CombatDrawCommand *command;

    command = D_800E9BA0++;
    command->first = 0xE7000000;
    command->second = 0;
    func_000225d0(D_801969B0, -160.0f, 160.0f, -120.0f, 120.0f,
                 -500.0f, 2000.0f, 1.0f);
    guMtxIdent(D_801969B0 + 1);

    command = D_800E9BA0++;
    command->first = 0xDA380007;
    command->second = (u32)D_801969B0++;
    command = D_800E9BA0++;
    command->first = 0xDA380003;
    command->second = (u32)D_801969B0++;
    func_00262DD4(value);
}
