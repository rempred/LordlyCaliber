#ifndef GAME_EFFECT_RUNTIME_E03_INTERNAL_H
#define GAME_EFFECT_RUNTIME_E03_INTERNAL_H

#include "game/effect_runtime_e03.h"

/* Internal layouts follow the observed 0x80/0x94/0x9C/0x78 allocation strides.
 * Field names describe accessed offsets; unobserved bytes retain unknown names. */
typedef struct E03Pool80 {
    u16 next_00;
    u16 unknown_02;
    u32 field_04;
    u32 field_08;
    u32 field_0C;
    u32 field_10;
    u32 field_14;
    u8 field_18;
    u8 unknown_19[0x67];
} E03Pool80;

typedef struct E03Pool94 E03Pool94;
typedef s32 (*E03Callback94)(u32 command, E03Pool94 *record);
struct E03Pool94 {
    u16 next_00;
    u16 unknown_02;
    E03Callback94 callback_04;
    u16 flags_08;
    u16 flags_0A;
    s32 field_0C;
    s32 field_10;
    s32 field_14;
    s32 field_18;
    u8 unknown_1C[0x78];
};

typedef struct E03Task E03Task;
typedef s32 (*E03TaskCallback)(u32 command, void *payload);
struct E03Task {
    u16 next_free_00;
    u16 unknown_02;
    E03TaskCallback callback_04;
    u16 state_08;
    u16 unknown_0A;
    E03Task *previous_0C;
    E03Task *next_10;
    E03Task *field_14;
    u16 resume_state_18;
    s16 countdown_1A;
    E03Pool94 *field_1C;
    s32 field_20;
    u8 payload_24[0x78];
};

typedef struct E03RenderRecord E03RenderRecord;
typedef void (*E03RenderCallback)(E03RenderRecord *record);
struct E03RenderRecord {
    u16 next_free_00;
    u16 unknown_02;
    E03RenderCallback callback_04;
    float field_08;
    s32 field_0C;
    u16 field_10;
    u16 unknown_12;
    u8 unknown_14[0x64];
};

extern u16 D_80220D50;
extern u32 D_80220D54;
extern E03Pool80 *D_80220D58;
extern void *D_80220D60;
extern u16 D_80220D64;
extern s32 D_80220D68;
extern E03Pool94 *D_80220D6C;
extern E03Pool94 **D_80220D70;
extern s32 D_80220D80;
extern E03Task *D_80220D84;
extern E03Task *D_80220D88;
extern E03Task *D_80220D8C;
extern s32 D_80220D90;
extern E03Task **D_80220D94;
extern u16 D_80220DA0;
extern s32 D_80220DA4;
extern E03RenderRecord *D_80220DA8;
extern E03RenderRecord **D_80220DAC;
extern u16 D_80220E8E;
extern void *D_80220EDC;
extern E03Task *D_80220FE8;

extern void *func_80070F30(s32 size);
extern void func_800712C4(void *memory);
extern void func_001F0F6C(s32 recordWord);
extern void func_001F0F90(void *record);


/* These accepted KEEP providers use an opaque record address. */
extern void func_002623C4(void *record);
extern s32 func_002623E0(void *record);

#endif
