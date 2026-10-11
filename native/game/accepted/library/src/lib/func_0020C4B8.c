typedef union DrawMatrix {
    unsigned int words[16];
    double alignment;
} DrawMatrix;

extern void guTranslate(DrawMatrix *, float, float, float);
extern void guScale(DrawMatrix *, float, float, float);
extern void func_00028740(DrawMatrix *, DrawMatrix *, DrawMatrix *);
extern void func_00023018(DrawMatrix *, float, float, float);
extern void guMtxL2F(float [4][4], DrawMatrix *);
extern void func_00210200(float *, float *, float [4][4],
                         float *, float *, float *, float);

void func_0020C4B8(int x, int y, int z, int *outX, int *outY)
{
    float output[2];
    float point[3];
    float matrix[4][4];
    float first[3];
    float second[3];
    float third[3];
    DrawMatrix scaleMatrix;
    DrawMatrix translationMatrix;
    DrawMatrix combinedMatrix;
    float scale;

    guTranslate(&translationMatrix, (float)x, (float)y, (float)z);
    guScale(&scaleMatrix, 0.63f, 0.63f, 0.63f);
    func_00028740(&scaleMatrix, &translationMatrix, &translationMatrix);
    func_00023018(&combinedMatrix, *(float *)0x801CE8EC,
                 *(float *)0x801CE8F0, *(float *)0x801CE8F4);
    func_00028740(&combinedMatrix, &translationMatrix, &scaleMatrix);
    scale = *(float *)0x801D06FC;
    guScale(&translationMatrix, scale, scale, scale);
    func_00028740(&scaleMatrix, &translationMatrix, &combinedMatrix);
    /* Chained stores retain the shared floating zero across guMtxL2F.
     * Separate assignments instead use integer zero stores in this compiler.
     */
    output[0] = output[1] = 0.0f;
    point[0] = point[2] = 0.0f;
    point[1] = 30.0f;
    guMtxL2F(matrix, &combinedMatrix);
    first[0] = *(float *)0x801D0730;
    first[1] = *(float *)0x801D083C;
    first[2] = *(float *)0x801D0700;
    second[0] = *(float *)0x801D0720;
    second[1] = *(float *)0x801D072C;
    second[2] = *(float *)0x801D0704;
    third[2] = third[0] = 0.0f;
    third[1] = 1.0f;
    func_00210200(output, point, matrix, first, second, third,
                  *(float *)0x801D0710);
    if (outX != 0) {
        *outX = (int)output[0];
    }
    if (outY != 0) {
        *outY = 240 - (int)output[1];
    }
}
