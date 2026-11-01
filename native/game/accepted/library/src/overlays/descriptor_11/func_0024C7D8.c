#include "game/effect_adapters_e08.h"
#include "game/effect_math_e04.h"

typedef struct P01Vec3 {
    float x;
    float y;
    float z;
} P01Vec3;

typedef struct P01PlacementObject P01PlacementObject;

extern void func_00028C40(float matrix[4][4]);

void func_0024C7D8(P01Vec3 *input, P01PlacementObject *object)
{
    /* The three vector blocks are copied as one sequence below. */
    float rows[9];
    float (*matrix)[4] = (float (*)[4])object;
    s32 row;
    s32 column;
    s32 index;

    rows[3] = 0.0f;
    rows[4] = 1.0f;
    rows[5] = 0.0f;
    rows[6] = input->x;
    rows[7] = input->y;
    rows[8] = input->z;

    func_0025EEF8(&rows[3], &rows[6], &rows[0]);
    func_0025EEF8(&rows[6], &rows[0], &rows[3]);
    func_00028C40(matrix);

    for (row = 0, index = 0; row < 3; row++) {
        for (column = 0; column < 3; column++) {
            matrix[row][column] = rows[index++];
        }
    }
}
