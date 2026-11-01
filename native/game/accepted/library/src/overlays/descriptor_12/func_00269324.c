#include "game/effect_model_e06_internal.h"

#include "game/effect_dispatch_e07.h"

void func_00269324(E06RepackStateA *state)
{
    E06RepackedA local;
    state->callback = func_0026a204;
    func_80093060(state->m, local.m, sizeof(local.m));
    func_80093060(state->unk68, local.unk48, sizeof(local.unk48));
    local.unk40 = state->unk5C;
    local.unk44 = state->unk66;
    local.unk60 = state->unk80;
    local.unk62 = state->unk82;
    local.unk64 = state->unk84;
    local.unk68 = state->unk88;
    local.unk6C = state->unk8C;
    func_80093060(&local, state->m, sizeof(local));
}
