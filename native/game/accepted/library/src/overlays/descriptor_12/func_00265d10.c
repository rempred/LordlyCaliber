#include "game/effect_geometry_e05_internal.h"
#include "game/effect_math_e04.h"

extern double D_80220B80, D_80220B88;
extern u8 D_80220E90[];
void func_00265d10(E05Geometry265D10 *geometry, float angle, s32 step,
                  E05FloatPoint16 *points, float originX, float originY, void *matrix)
{
    E05PackedPoint16 packed;
    float projected[4];
    E05FloatPoint16 *cursor;
    s16 i, done;

    func_80093380(&packed, 16);
    angle = (double)angle * D_80220B80 / D_80220B88;
    cursor = points;
    i = 0;
    done = 0;
    do {
        s16 distance = step * i;
        float value;

        if (distance >= geometry->value_44) {
            done = 1;
            distance = geometry->value_44;
        }
        value = distance;
        cursor->x = value * func_800907F0(angle);
        cursor->y = value * func_80092DB0(angle);
        cursor->z = 0.0f;
        i++;
        cursor++;
    } while (!done);
    {
        float scale = (float)(geometry->value_44 - geometry->value_42) /
                      (float)geometry->value_44;
        s16 count = i;
        float x = points[count - 1].x;
        float y = points[count - 1].y;
        float z = points[count - 1].z;

        cursor = points;
        for (done = 0; done < count; done++, cursor++) {
            s32 coordinate;

            func_80098450(matrix, cursor->x, cursor->y, cursor->z,
                         &projected[0], &projected[1], &projected[2]);
            func_002638D0(projected[0], projected[1], projected[2],
                          &projected[0], &projected[1]);
            coordinate = projected[0] + geometry->value_4A;
            if (coordinate < 0) coordinate = 0;
            if (coordinate >= geometry->width) coordinate = geometry->width - 1;
            packed.value_04 = coordinate;
            coordinate = -projected[1] + geometry->value_4C;
            if (coordinate < 0) coordinate = 0;
            if (coordinate >= geometry->height) coordinate = geometry->height - 1;
            packed.value_06 = coordinate;

            cursor->x = (cursor->x - x) * scale + x;
            cursor->y = (cursor->y - y) * scale + y;
            cursor->z = (cursor->z - z) * scale + z;
            func_80098450(matrix, cursor->x, cursor->y, cursor->z,
                         &projected[0], &projected[1], &projected[2]);
            projected[3] = 1.0f;
            if (geometry->value_4E) {
                func_0025F620(D_80220E90, projected, cursor);
                packed.value_08 = cursor->z / cursor->w * 16352.0f + 16352.0f;
            }
            func_002638D0(projected[0], projected[1], projected[2],
                          &projected[0], &projected[1]);
            packed.x = projected[0] - originX;
            packed.y = projected[1] - originY;
            func_80093060(&packed, cursor, 16);
        }
    }
}
