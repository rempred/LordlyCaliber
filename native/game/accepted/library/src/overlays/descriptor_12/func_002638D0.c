#include "game/effect_geometry_e05_internal.h"

extern float D_801D0710;
extern double D_80220B60;
extern double D_80220B68;
void func_002638D0(float x, float y, float distance, float *outX, float *outY)
{
    float angle = (double)(D_801D0710 / 2.0f) * D_80220B60 / D_80220B68;

    if (distance == 0.0f) {
        *outX = x;
        *outY = y;
    } else {
        distance *= func_80092DB0(angle);
        distance /= func_800907F0(angle);
        distance = -1.0f / distance;
        *outX = x * 120.0f * distance;
        *outY = y * 120.0f * distance;
    }
}
