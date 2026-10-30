#ifndef GAME_EFFECT_MATH_E04_H
#define GAME_EFFECT_MATH_E04_H

/* Numerically named vector, matrix, and effect helpers. Address-view parameters
 * retain local layouts in their producers without asserting that the partially
 * recovered records used by different callers are the same C type. */

int func_0024BE1C(unsigned char value);
int func_0024BE5C(unsigned char value);
void func_0025EE90(void *sourceView, float scale, void *resultView);
void func_0025EEF8(void *leftView, void *rightView, void *resultView);
float func_0025EF78(void *vectorView);
void func_0025EFC8(void *vectorView);
void func_0025F058(void *sourceView, void *resultView);
void func_0025F080(void *scaleView, void *matrixView, void *resultView);
void func_0025F150(void *matrixView, void *valuesView);
void func_0025F178(void *thirdView, void *firstView, void *secondView, void *matrixView);
void func_0025F350(void *firstView, void *secondView, void *matrixView);
void func_0025F438(void *firstView, void *secondView, void *matrixView);
void func_0025F530(void *firstView, void *secondView, void *matrixView);
void func_0025F620(void *matrixView, void *vectorView, void *resultView);
void func_0025F6C0(unsigned short *values, int count);
int func_0025F74C(unsigned int index);
int func_0025F778(unsigned int index);
int func_0025F7A0(unsigned int index);
int func_0025F7C8(unsigned int index);
void func_0026C1F0(void *firstView, void *secondView, void *matrixView);
void func_0026C230(void *vectorView, void *matrixView);
void func_0026C310(void *vectorView, void *matrixView);
void func_0026C3F8(void *matrixView);
void func_0026C4EC(void *object, void *outputView);
void func_0026C55C(void *object, void *matrixView);
void func_0026C684(void *object, void *matrixView);
void func_0026C7C4(void *object, void *outputView);
void func_0026C834(unsigned int index, void *outputView);
void func_0026C8AC(void **objects, unsigned int value);
void func_0026C90C(unsigned int index, unsigned int value);
void func_0026C970(void **objects, void *destination);
void func_0026C994(unsigned int index, void *destination);
void func_0026C9CC(int count, float *a, float *b, float *output);
float func_0026CBDC(float value, int count, float *a, float *b, float *c);
void func_0026CCB4(float first, float second, float third, int value);

#endif
