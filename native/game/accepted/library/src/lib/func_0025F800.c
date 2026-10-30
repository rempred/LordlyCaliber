#include "game/effect_math_e04.h"
#include "common/types.h"

typedef struct {
    float x;
    float y;
    float z;
} E01Vec3;

extern float D_801D0710;
extern float D_801D0720;
extern float D_801D072C;
extern float D_801D0704;
extern float D_801D0730;
extern float D_801D083C;
extern float D_801D0700;
extern float D_80220E90[4][4];
extern float D_80220E20[4][4];
extern float D_80220FF0[16];
extern float D_80220EE0[16];
extern float D_80220F28[16];
extern E01Vec3 D_80220ED0;

void func_00022720(float matrix[4][4], u16 *normalization, float fovy,
                   float aspect, float near, float far, float scale);
void func_00020d40(float matrix[4][4], float ex, float ey, float ez,
                   float ax, float ay, float az, float ux, float uy, float uz);

void func_0025F800(void)
{
    float basis[9];
    u16 normalization;
    float *destination;
    float *source;
    s32 i;

    func_00022720(D_80220E90, &normalization, D_801D0710,
                  1.3333333730697632f, 1.0f, 4000.0f, 1.0f);
    func_00020d40(D_80220E20, D_801D0720, D_801D072C, D_801D0704,
                  D_801D0730, D_801D083C, D_801D0700, 0.0f, 1.0f, 0.0f);

    basis[3] = 0.0f;
    basis[4] = 1.0f;
    basis[5] = 0.0f;
    basis[6] = D_801D0720 - D_801D0730;
    basis[7] = D_801D072C - D_801D083C;
    basis[8] = D_801D0704 - D_801D0700;
    func_0025EEF8((E01Vec3 *)&basis[3], (E01Vec3 *)&basis[6], (E01Vec3 *)basis);
    func_0025EEF8((E01Vec3 *)&basis[6], (E01Vec3 *)basis, (E01Vec3 *)&basis[3]);
    func_0025EFC8((E01Vec3 *)basis);
    func_0025EFC8((E01Vec3 *)&basis[3]);
    func_0025EFC8((E01Vec3 *)&basis[6]);

    i = 0;
    destination = D_80220FF0;
    source = basis;
    for (; i < 15; i++, destination++) {
        if (((i & 3) == 3) | (i >= 12)) {
            *destination = 0.0f;
        } else {
            *destination = *source++;
        }
    }
    D_80220FF0[i] = 1.0f;

    basis[3] = 0.0f;
    basis[4] = 1.0f;
    basis[5] = 0.0f;
    D_80220ED0.x = basis[6];
    D_80220ED0.y = basis[7];
    D_80220ED0.z = basis[8];
    basis[6] = D_801D0720 - D_801D0730;
    basis[7] = D_801D072C - D_801D083C;
    basis[8] = D_801D0704 - D_801D0700;
    func_0025EEF8((E01Vec3 *)&basis[3], (E01Vec3 *)&basis[6], (E01Vec3 *)basis);
    func_0025EEF8((E01Vec3 *)basis, (E01Vec3 *)&basis[3], (E01Vec3 *)&basis[6]);
    func_0025EFC8((E01Vec3 *)basis);
    func_0025EFC8((E01Vec3 *)&basis[6]);

    i = 0;
    destination = D_80220EE0;
    source = basis;
    for (; i < 15; i++, destination++) {
        if (((i & 3) == 3) | (i >= 12)) {
            *destination = 0.0f;
        } else {
            *destination = *source++;
        }
    }
    D_80220EE0[i] = 1.0f;

    basis[3] = 0.0f;
    basis[4] = 1.0f;
    basis[5] = 0.0f;
    basis[6] = D_801D0730 - D_801D0720;
    basis[7] = D_801D083C - D_801D072C;
    basis[8] = D_801D0700 - D_801D0704;
    func_0025EEF8((E01Vec3 *)&basis[3], (E01Vec3 *)&basis[6], (E01Vec3 *)basis);
    func_0025EEF8((E01Vec3 *)basis, (E01Vec3 *)&basis[3], (E01Vec3 *)&basis[6]);
    func_0025EFC8((E01Vec3 *)basis);
    func_0025EFC8((E01Vec3 *)&basis[6]);

    i = 0;
    destination = D_80220F28;
    source = basis;
    for (; i < 15; i++, destination++) {
        if (((i & 3) == 3) | (i >= 12)) {
            *destination = 0.0f;
        } else {
            *destination = *source++;
        }
    }
    D_80220F28[i] = 1.0f;
}
