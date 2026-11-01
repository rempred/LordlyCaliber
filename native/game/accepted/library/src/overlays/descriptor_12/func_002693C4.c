#include "game/effect_model_e06_internal.h"

#include "game/effect_dispatch_e07.h"

void func_002693C4(E06RepackStateB *state)
{
    E06RepackedB local;
    state->callback = func_00269470;
    func_80093060(state->m, local.m, sizeof(local.m));
    func_80093060(state->unk64, local.unk4C, sizeof(local.unk4C));
    local.unk40 = state->unk5C;
    local.unk4A = state->unk60;
    local.unk64 = state->unk7C;
    local.unk66 = state->unk7E;
    local.unk68 = state->unk80;
    local.unk6C = state->unk84;
    local.unk70 = state->unk88;
    func_80093060(&local, state->m, sizeof(local));
}
