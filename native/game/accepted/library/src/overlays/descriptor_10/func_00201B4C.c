typedef signed short s16;
typedef signed int s32;
extern struct { s16 x; s16 y; s16 z; } *D_801D06A8;
s16 func_00201B4C(s32 i) {
    return D_801D06A8[i].x;
}
