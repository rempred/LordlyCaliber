#include "game/effect_math_e04.h"

typedef struct {
    float x;
    float y;
    float z;
} E04Vec3;

extern void func_00028C40(float [4][4]);

/* Keeping the basis in this helper gives the negated temporary its own
 * scope under the pinned compiler, as in the C310 source experiment. */
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

extern float D_80220610;
extern float D_80220614;
extern float D_80220618;

void func_0026C3F8(void *matrixView)
{
    float (*matrix)[4] = (float (*)[4])matrixView;
    E04Vec3 temporary;

    temporary.x = -D_80220610;
    temporary.y = -D_80220614;
    temporary.z = -D_80220618;
    e04_c230_inline(&temporary, matrix);
}