#include "game/effect_math_e04.h"

extern void *func_00001330(unsigned int);
extern void func_000016C4(void *);

void func_0026C9CC(int count, float *a, float *b, float *output)
{
    void *allocation;
    float *h;
    float *d;
    int i;

    /* In the accepted placement, each guard jumps to its own instruction. */
    if (count < 3) {
        for (;;) {
        }
    }
    allocation = func_00001330((unsigned int)count * 8);
    if (allocation == 0) {
        for (;;) {
        }
    }
    h = allocation;
    d = (float *)allocation + count;
    output[0] = 0.0f;
    output[count - 1] = 0.0f;
    for (i = 0; i < count - 1; i++) {
        h[i] = a[i + 1] - a[i];
        d[i + 1] = (b[i + 1] - b[i]) / h[i];
    }
    output[1] = d[2] - d[1] - h[0] * output[0];
    d[1] = 2.0f * (a[2] - a[0]);
    for (i = 1; i < count - 2; i++) {
        float ratio = h[i] / d[i];

        output[i + 1] = d[i + 2] - d[i + 1] - output[i] * ratio;
        d[i + 1] = 2.0f * (a[i + 2] - a[i]) - h[i] * ratio;
    }
    /* The retail body contains both this subtraction and the backward pass. */
    output[count - 2] -= h[count - 2] * output[count - 1];
    for (i = count - 2; i > 0; i--) {
        output[i] = (output[i] - h[i] * output[i + 1]) / d[i];
    }
    func_000016C4(allocation);
}
