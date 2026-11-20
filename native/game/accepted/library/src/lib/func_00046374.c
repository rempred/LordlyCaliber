typedef unsigned char u8;
typedef signed int s32;
typedef struct { u8 unk0; u8 flags; u8 members[5]; u8 pos[5]; u8 harmony; u8 items[10]; u8 strategy; u8 legion; } SquadRec;
extern SquadRec D_801971F0[];
void func_00046374(s32 squad, s32 i, u8 value) {
    D_801971F0[squad].members[i] = value;
}
