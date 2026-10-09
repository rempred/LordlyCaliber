#ifndef OB64_GAME_SCENARIO_SOURCE_RECORD_H
#define OB64_GAME_SCENARIO_SOURCE_RECORD_H

#include "common/types.h"

typedef struct Func001957D0SourceRecord {
    u8 field_00;
    u8 field_01;
    u8 field_02[5];
    u8 field_07[5];
    u8 field_0C;
    u8 field_0D[10];
    u8 field_17;
    u8 field_18;
} Func001957D0SourceRecord;

extern Func001957D0SourceRecord g_func_001957D0_source_records[];

#endif
