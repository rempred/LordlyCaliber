#include "game/actor_queries.h"

typedef struct StoryOverride {
    u8 profile;
    u8 artKey;
} StoryOverride;

extern u8 D_80193BD4[];
extern u8 D_80195574[];
extern StoryOverride table_002868F0[];

s32 func_002AC700(s32 slot)
{
    QueryActor *actor = D_8022A974->actors[slot];
    ActorInput *input;
    s32 profile;
    s32 gender;
    s32 i;
    s32 artKey;

    if (actor == 0) {
        return -1;
    }

    input = &D_801CE8BC->inputs[actor->inputIndex];
    if (input->artKey == 0) {
        return -1;
    }

    profile = input->alignment;
    if (func_0020BFF8(input) == 0) {
        gender = D_80193BD4[input->memberIndex * 56];
    } else {
        gender = D_80195574[input->memberIndex * 52];
    }

    if (profile > 98) {
        profile = 98;
    }
    profile = gender * 3 + profile / 33;
    /* Keep the first key test separate so KMC reloads the loop base. */
    if (*(u8 *)0x8022A921 != 0xFF) {
        artKey = input->artKey;
        i = 0;
        do {
            if (table_002868F0[i].artKey == artKey) {
                profile = table_002868F0[i].profile;
            }
            i++;
        } while (table_002868F0[i].artKey != 0xFF);
    }

    return profile;
}
