#ifndef GAME_EFFECT_MODEL_E06_H
#define GAME_EFFECT_MODEL_E06_H

#include "common/types.h"

/* Numeric parameter views preserve actual widths and access strides. They do
 * not establish complete allocation sizes or stronger field meanings. */
struct E06FlagNode;
struct E06FloatNode;
struct E06TripleNode;
struct E06ResourcePair;
struct Func00269F00Curve;
struct E06TransformOutput;
struct E06TransformInput;
struct E06HierarchyInput;
struct E06Model;
struct E06Node;
struct E06State;
struct E06RenderNode;
struct E06RepackStateA;
struct E06RepackStateB;
struct Func00269470State;

void func_00267750(s32 count, struct E06FlagNode **nodes);
void func_00267788(s32 count, struct E06FloatNode **nodes, s32 increment);
void func_002677D0(s32 count, struct E06TripleNode **nodes, s32 increment);
void func_00267828(struct E06ResourcePair *pairs);
void func_002678A4(struct E06ResourcePair *pairs);
s32 func_00267928(s32 value, struct E06ResourcePair *pairs);
void func_00267D98(float *values, float matrix[4][4]);
void func_00267E80(float value, struct Func00269F00Curve **curves, struct E06TransformOutput *output, float *values);
void func_00267FD0(s32 argument, struct E06TransformInput *input, struct E06TransformOutput *output);
void func_002682A4(float root[4][4], struct E06HierarchyInput *input, struct E06TransformOutput *output);
void func_00268358(s32 count, void **objects);
void func_002683B4(s32 count, struct E06TransformOutput *input, struct E06FloatNode **nodes);
void func_00268400(s32 count, struct E06TransformOutput *input, struct E06TripleNode **nodes);
void func_00268478(s32 count, struct E06TransformOutput *output, float value);
void func_002684AC(struct E06Model *input, void **output);
void func_00268530(struct E06Model *input, struct E06TransformOutput *matrices, struct E06Node **nodes);
void func_00268678(struct E06State *state, s32 key);
void func_00268798(struct E06State *state);
void func_00268800(struct E06State *state);
void func_00268890(struct E06State *state, s32 key, u16 code, s32 offset, u16 unk10, s32 unk14);
void func_00268AD8(struct E06State *state, u16 code, s32 offset, u16 unk0C, s32 unk10);
void func_00268B78(struct E06Model *input, void **output);
void func_00268BFC(struct E06Model *input, struct E06TransformOutput *matrices, struct E06RenderNode **nodes);
void func_00268D4C(struct E06State *state, s32 key);
void func_00268E6C(struct E06State *state);
void func_00268ED4(struct E06State *state);
void func_00268F64(s32 value, struct E06Model *input, struct E06Node **nodes);
void func_0026909C(s32 value, struct E06Model *input, struct E06Node **nodes, u16 resourceCode, s32 offset);
void func_002691EC(s32 value, struct E06Model *input, struct E06RenderNode **nodes);
void func_00269324(struct E06RepackStateA *state);
void func_002693C4(struct E06RepackStateB *state);
u32 func_00269470(u32 selector_arg, struct Func00269470State *state);

/* The two final positions are present in callers and unused by these bodies. */
struct ResourceBlock;
s32 func_00261A44(struct ResourceBlock *block, u16 resourceCode, s32 offset, u16 unused0C, s32 unused10);
void func_00261BD4(struct ResourceBlock *block, u16 resourceCode, s32 offset, u16 unused0C, s32 unused10);

#endif
