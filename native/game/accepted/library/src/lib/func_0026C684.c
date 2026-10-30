#include "game/effect_math_e04.h"

typedef struct {
    float x;
    float y;
    float z;
} E04Vec3;

extern void func_00028C40(float [4][4]);
extern int func_0020BFF8(void *);
extern int func_0020C2D8(void *);

/* The separate helper scopes reproduce the intermediate vector storage seen
 * in C310 and C55C. Their original source identity remains unproved. */
static __inline__ void e04_c230_inline(E04Vec3 *vector, float matrix[4][4])
{
    float basis[3][3];
    int row;
    int column;
    int i;

    basis[2][0] = vector->x;
    basis[2][1] = vector->y;
    basis[2][2] = vector->z;
    func_0025EFC8((E04Vec3 *)basis[2]);
    basis[1][0] = 0.0f;
    basis[1][1] = 1.0f;
    basis[1][2] = 0.0f;
    func_0025EEF8(basis[1], basis[2], basis[0]);
    func_0025EEF8(basis[2], basis[0], basis[1]);
    func_00028C40(matrix);
    for (row = 0, i = 0; row < 3; row++) {
        for (column = 0; column < 3; column++) {
            matrix[row][column] = ((float *)basis)[i++];
        }
    }
}

static __inline__ void e04_c310_inline(E04Vec3 *vector, float matrix[4][4])
{
    E04Vec3 temporary;

    temporary.x = -vector->x;
    temporary.y = -vector->y;
    temporary.z = -vector->z;
    e04_c230_inline(&temporary, matrix);
}

static __inline__ void e04_flag_vector(void *object, E04Vec3 *output)
{
    int first = func_0020BFF8(object);

    if (first != func_0020C2D8(object)) {
        output->x = 1.0f;
    } else {
        output->x = -1.0f;
    }
    output->y = 0.0f;
    output->z = 0.0f;
}

void func_0026C684(void *object, void *matrixView)
{
    float (*matrix)[4] = (float (*)[4])matrixView;
    E04Vec3 temporary;

    e04_flag_vector(object, &temporary);
    e04_c310_inline(&temporary, matrix);
}
