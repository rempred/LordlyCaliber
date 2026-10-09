typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;

typedef struct { u32 flags; u8 source_index; } RuntimeUnit;

extern u8 D_801971F2[][25];
extern u16 D_80193BD8[][28];
extern u16 D_80195578[][26];

s32 func_0012DA10(RuntimeUnit *unit)
{
    u32 source_index = unit->source_index;
    s32 member = D_801971F2[source_index][0];
    s32 result;

    if ((member == 0) | (member >= 100)) {
        return 0;
    }
    if (source_index < 30) {
        result = D_80193BD8[member][0] != 0;
    } else {
        result = D_80195578[member][0] != 0;
    }
    return result;
}
