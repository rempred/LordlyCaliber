#ifndef OB64_GAME_ACTOR_QUERIES_H
#define OB64_GAME_ACTOR_QUERIES_H

#include "common/types.h"

typedef struct QueryActor {
    u8 pad000[0x147];
    u8 inputIndex;
} QueryActor;

typedef struct QueryScene {
    u8 pad00[0x18];
    QueryActor *actors[28];
} QueryScene;

typedef struct ActorInput {
    u8 pad00[0x34];
    u8 alignment;
    u8 pad35[0x13];
    s32 artKey;
    u8 pad4C[0xAA];
    u8 memberIndex;
    u8 padF7;
} ActorInput;

typedef struct QueryResident {
    u8 pad000[0x1C4];
    ActorInput inputs[20];
} QueryResident;

extern QueryScene *D_8022A974;
extern QueryResident *D_801CE8BC;

s32 func_0020BFF8(void *input);

#endif
