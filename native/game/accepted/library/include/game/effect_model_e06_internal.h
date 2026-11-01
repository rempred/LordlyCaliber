#ifndef GAME_EFFECT_MODEL_E06_INTERNAL_H
#define GAME_EFFECT_MODEL_E06_INTERNAL_H

#include "game/effect_model_e06.h"
#include "game/effect_runtime_e03.h"
#include "game/effect_geometry_e05.h"

struct Func0026A204State;
typedef void (*Func00269470Callback)(u32 reason, void *state_fields, void *argument);

typedef struct E06FlagNode {
    s32 unk00[3];
    s32 unk0C;
} E06FlagNode;

typedef struct E06FloatNode {
    s32 unk00[2];
    float unk08;
} E06FloatNode;

typedef struct E06TripleNode {
    s32 unk00;
    s32 unk04;
    s32 unk08;
    s32 unk0C;
} E06TripleNode;

typedef struct E06ResourcePair {
    s16 unk00;
    s16 unk02;
} E06ResourcePair;

typedef struct E06CurveKey {
    float time;
    float value;
    float unk08;
    float unk0C;
    s16 mode;
    s16 unk12;
} E06CurveKey;

typedef struct Func00269F00Curve {
    E06CurveKey *keys;
    s16 count;
    s16 mode;
    s16 before;
    s16 after;
} Func00269F00Curve;

typedef struct E06TransformOutput {
    float m[4][4];
    float unk40;
} E06TransformOutput;

typedef struct E06TransformInput {
    u32 unk00;
    u32 count;
    struct Func00269F00Curve ***curves;
    u8 unk0C[0x0C];
    float **values;
} E06TransformInput;

typedef struct E06HierarchyPair {
    u16 index;
    u16 level;
} E06HierarchyPair;

typedef struct E06HierarchyInput {
    u32 unk00;
    u32 count;
    u32 unk08[2];
    E06HierarchyPair *pairs;
} E06HierarchyInput;

typedef struct E06Record {
    u16 unk00;
    u16 unk02;
    u16 unk04;
    u16 unk06;
    u16 unk08;
} E06Record;

/* Resource-header view shared by the observed consumers. Transform, hierarchy
 * and keyed-pair readers keep explicit views of the same resource storage. */
typedef struct E06Model {
    s32 unk00;
    u32 count;
    void *unk08;
    E06Record *records;
    void *unk10;
    E06ResourcePair **pairs;
} E06Model;

typedef struct E06NodePayload {
    void *unk00;
    E06TransformOutput *unk04;
    void *unk08;
    void *unk0C;
    u16 unk10;
    u16 unk12;
    void *unk14;
} E06NodePayload;

/* Actual callback word at00 resolves to func_00263CE4, s32(u8*). */
typedef struct E06Node {
    s32 (*unk00)(u8 *);
    u8 unk04[0x14];
    E06NodePayload payload;
} E06Node;

typedef struct E06RenderPayload {
    u16 unk00;
    u16 unk02;
    E06TransformOutput *unk04;
    void *unk08;
    void *unk0C;
    u16 unk10;
    u16 unk12;
    void *unk14;
} E06RenderPayload;

/* Actual callback word at04 resolves to func_00263E10, s32(u8*). */
typedef struct E06RenderNode {
    u32 unk00;
    s32 (*unk04)(u8 *);
    u8 unk08[0xC];
    E06RenderPayload payload;
} E06RenderNode;

/* Exactly the accessed 0x18-byte work record, also embedded at fields+0x4C
 * in the dispatcher. The final allocation pointer is part of that record. */
typedef struct E06State {
    s32 unk00;
    E06Model *model;
    E06TransformOutput *matrices;
    void **nodes;
    u16 unk10;
    u16 unk12;
    void *allocation;
} E06State;

typedef struct E06RepackStateA {
    u32 unk00;
    u32 (*callback)(u16, struct Func0026A204State *);
    u8 unk08[0x14];
    float m[4][4];
    float unk5C;
    u8 unk60[6];
    u16 unk66;
    u8 unk68[0x18];
    u16 unk80;
    u16 unk82;
    u32 unk84;
    u32 unk88;
    u32 unk8C;
} E06RepackStateA;

typedef struct E06RepackedA {
    float m[4][4];
    float unk40;
    u16 unk44;
    u16 unk46;
    u8 unk48[0x18];
    u16 unk60;
    u16 unk62;
    u32 unk64;
    u32 unk68;
    u32 unk6C;
} E06RepackedA;

typedef struct E06RepackStateB {
    u32 unk00;
    u32 (*callback)(u32, struct Func00269470State *);
    u8 unk08[0x14];
    float m[4][4];
    float unk5C;
    u16 unk60;
    u16 unk62;
    u8 unk64[0x18];
    u16 unk7C;
    u16 unk7E;
    u32 unk80;
    u32 unk84;
    u32 unk88;
} E06RepackStateB;

typedef struct E06RepackedB {
    float m[4][4];
    float unk40;
    u8 unk44[6];
    u16 unk4A;
    u8 unk4C[0x18];
    u16 unk64;
    u16 unk66;
    u32 unk68;
    u32 unk6C;
    u32 unk70;
} E06RepackedB;

typedef struct E06FlagsNode {
    u8 unk00[0x18];
    u16 flags18;
} E06FlagsNode;

typedef union Func00269470Value40 {
    float as_float;
    u32 as_u32;
} Func00269470Value40;

/* Matrix00/scalar40/work4C compose observed fields; work4C is not a four-byte
 * array extending through later unrelated declarations. */
typedef struct Func00269470Fields {
    float matrix_00[4][4];
    Func00269470Value40 value_40;
    u8 unused_44[6];
    u16 flags_4A;
    E06State work_4C;
    u16 value_64;
    u16 value_66;
    s32 value_68;
    s32 value_6C;
    Func00269470Callback callback_70;
} Func00269470Fields;

typedef struct Func00269470State {
    u8 unused_00[8];
    u16 flags_08;
    u16 flags_0A;
    s32 value_0C;
    s32 value_10;
    u8 unused_14[8];
    Func00269470Fields fields;
} Func00269470State;

extern void *func_00261560(s32 value);
extern void func_00260CE0(u16 code, s32 key04, s32 key08);
extern void func_00022e90(float matrix[4][4], float first, float second, float third);
extern void guMtxCatF(float first[4][4], float second[4][4], float output[4][4]);
extern void func_001F0F6C(s32 recordWord);
extern void func_80093380(void *destination, u32 bytes);
extern void *func_00260B0C(u16 code, s32 key04, s32 key08);
extern void *func_002611F4(s32 key04);
extern s32 func_002617B0(struct ResourceBlock *block);
extern void *func_80070F30(s32 bytes);
extern void func_002619A4(struct ResourceBlock *block);
extern void func_00260C24(void *data);
extern void func_800712C4(void *memory);
extern void *func_002637DC(u16 code, s32 key04);
extern void func_80093060(const void *source, void *destination, u32 bytes);
extern void func_00028C40(float matrix[4][4]);

#endif
