typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef double f64;
typedef struct RuntimeUnit RuntimeUnit;
#define FIELD(object, type, offset) (*(type *)((u8 *)(object) + (offset)))

extern u8 D_800E7AB9;
extern u8 D_800E7A90[];
extern u16 D_801F36D8[];
extern u8 D_801EB520[][32];
extern f32 D_801F0D98, D_801F0D9C, D_801F0DA0, D_801F0DA4;
extern f64 D_801EE848, D_801EE850;
extern f32 func_0002CB80(f32 dx, f32 dz);
extern f32 func_800907F0(f32 value);
extern f32 func_80092DB0(f32 value);
extern f32 func_801A63FC(s32 selector, f32 x, f32 z);

/* The direct consumer supplies two unit pointers, a byte-valued selector,
 * two three-float points and a full word. It consumes signed status values.
 * Keep the selector as a word: this body performs no entry narrowing. */
s32 func_0012B440(RuntimeUnit *first, RuntimeUnit *second, s32 selector,
                 f32 *current, f32 *requested, s32 bypass)
{
    /* This cursor carries the real requested-point address first. After its
     * last use it carries the selector-byte address across the secondary calls;
     * splitting those phases made KMC rematerialize the latter address. */
    u8 *inputCursor = (u8 *)requested;
    f32 originalZ = current[2];
    f32 lowZ = D_801F0D9C;
    f32 deltaZ = originalZ - lowZ;
    f32 factor = 64.0f;
    /* Keep the real initial high-Z/span and later displacement phases in
     * these work scalars. Splitting them changed KMC's saved FP frontier. */
    f32 workZ = D_801F0DA4;
    f32 workX = workZ - lowZ;
    f32 gridZ = deltaZ * factor / workX;
    f32 originalX = current[0];
    f32 lowX = D_801F0D98;
    f32 deltaX = originalX - lowX;
    f32 highX = D_801F0DA0;
    f32 spanX = highX - lowX;
    f32 gridX = deltaX * factor / spanX;
    s32 origin = ((s32)gridZ << 6) + (s32)gridX;
    f32 point[3];
    f32 x = ((f32 *)inputCursor)[0];
    f32 z;
    f32 primaryDistance;
    s32 status;
    s32 result;

    /* Reject only the original ordered less-than tests. An unordered
     * coordinate does not satisfy one of these comparisons. */
    if (x < lowX)
        goto second_path;
    if (highX < x)
        goto second_path;
    z = ((f32 *)inputCursor)[2];
    if (z < lowZ)
        goto second_path;
    if (workZ < z)
        goto second_path;
    if (bypass == 0) {
        f32 cellZ = (z - lowZ) * 64.0f / (workZ - lowZ);
        f32 cellX = (x - lowX) * 64.0f / (highX - lowX);
        u32 bankIndex = D_800E7A90[0x33];
        u8 *map = *(u8 **)(D_800E7A90 + (bankIndex * 4));
        s32 index = ((s32)cellZ << 6) + (s32)cellX;
        s32 rowOffset = selector << 6;
        u8 cell = map[index];
        u16 *row = (u16 *)((u32)D_801F36D8 + rowOffset);
        if (row[cell] == 0xFFFF)
            goto second_path;
        if (D_801EB520[map[origin]][cell] != 0)
            goto second_path;
    }

    primaryDistance = func_0002CB80(x - originalX, z - originalZ);
    status = 2;
    if (0.005f < primaryDistance) {
        f32 newY;
        f32 inputX = ((f32 *)inputCursor)[0];
        f32 previousX = current[0];
        f32 previousZ = current[2];
        workX = (inputX - previousX) * 0.005f / primaryDistance;
        workZ = (((f32 *)inputCursor)[2] - previousZ) * 0.005f / primaryDistance;
        workX += previousX;
        workZ += previousZ;
        newY = func_801A63FC(D_800E7AB9, workX, workZ);
        status = 1;
        point[0] = workX;
        point[1] = newY;
        point[2] = workZ;
    } else {
        point[0] = ((f32 *)inputCursor)[0];
        point[1] = ((f32 *)inputCursor)[1];
        point[2] = ((f32 *)inputCursor)[2];
    }
    {
    f32 pointLowX, pointHighX, pointLowZ, pointHighZ;
    f32 boundPointX, boundPointZ;
    boundPointX = point[0];
    pointLowX = D_801F0D98;
    if (boundPointX < pointLowX) goto second_path;
    pointHighX = D_801F0DA0;
    if (pointHighX < boundPointX) goto second_path;
    boundPointZ = point[2];
    pointLowZ = D_801F0D9C;
    if (boundPointZ < pointLowZ) goto second_path;
    pointHighZ = D_801F0DA4;
    if (pointHighZ < boundPointZ) goto second_path;
    if (bypass == 0) {
        f32 cellZ = (boundPointZ - pointLowZ) * 64.0f / (pointHighZ - pointLowZ);
        f32 cellX = (boundPointX - pointLowX) * 64.0f / (pointHighX - pointLowX);
        u32 bankIndex = D_800E7A90[0x33];
        u8 *map = *(u8 **)(D_800E7A90 + (bankIndex * 4));
        s32 index = ((s32)cellZ << 6) + (s32)cellX;
        s32 rowOffset = selector << 6;
        u8 cell = map[index];
        u16 *row = (u16 *)((u32)D_801F36D8 + rowOffset);
        if (row[cell] == 0xFFFF)
            goto second_path;
        if (D_801EB520[map[origin]][cell] != 0)
            goto second_path;
    }
    current[0] = boundPointX;
    }
    /* Keep each complete real output arm explicit through allocation. KMC
     * merges their identical tails later, retaining the primary failure jump. */
    current += 1;
    current[0] = point[1];
    current[1] = point[2];
    result = status;
    goto return_result;

second_path:
    {
    f32 secondaryDistance, pointDistance;
    secondaryDistance = func_0002CB80(FIELD(second, f32, 0x08) - current[0],
                            FIELD(second, f32, 0x10) - current[2]);
    status = -2;
    if (0.8f < secondaryDistance) {
        f32 newY;
        f64 value = (f64)FIELD(second, f32, 0x18);
        f64 shifted = value - D_801EE848;
        f64 chosen;
        f32 turn;
        if (0.0 < shifted)
            chosen = shifted;
        else
            chosen = value + D_801EE848;
        turn = (f32)chosen;
        turn = (f32)((f64)(turn + turn) * D_801EE850);
        point[0] = func_800907F0(turn) * 0.8f;
        point[2] = -(func_80092DB0(turn) * 0.8f);
        point[0] += FIELD(second, f32, 0x08);
        /* Assign the real Z component as the height argument. KMC then stores
         * its argument bits into the point in the call delay slot; a separate
         * statement stored from the FP scratch register before argument setup. */
        inputCursor = &D_800E7AB9;
        point[1] = func_801A63FC(inputCursor[0], point[0],
                                 point[2] += FIELD(second, f32, 0x10));
        pointDistance = func_0002CB80(point[0] - current[0], point[2] - current[2]);
        if (0.005f < pointDistance) {
            f32 inputX = point[0];
            f32 previousX = current[0];
            f32 previousZ = current[2];
            workX = (inputX - previousX) * 0.005f / pointDistance;
            workZ = (point[2] - previousZ) * 0.005f / pointDistance;
            workX += previousX;
            workZ += previousZ;
            newY = func_801A63FC(inputCursor[0], workX, workZ);
            status = -1;
        } else {
            workX = point[0];
            workZ = point[2];
            newY = point[1];
            status = -1;
        }
        point[0] = workX;
        point[1] = newY;
        point[2] = workZ;
    } else {
        point[0] = current[0];
        point[2] = current[2];
        point[1] = current[1];
    }
    {
    f32 pointLowX, pointHighX, pointLowZ, pointHighZ;
    f32 boundPointX, boundPointZ;
    boundPointX = point[0];
    pointLowX = D_801F0D98;
    if (boundPointX < pointLowX) goto first_path;
    pointHighX = D_801F0DA0;
    if (pointHighX < boundPointX) goto first_path;
    boundPointZ = point[2];
    pointLowZ = D_801F0D9C;
    if (boundPointZ < pointLowZ) goto first_path;
    pointHighZ = D_801F0DA4;
    if (pointHighZ < boundPointZ) goto first_path;
    if (bypass == 0) {
        f32 cellZ = (boundPointZ - pointLowZ) * 64.0f / (pointHighZ - pointLowZ);
        f32 cellX = (boundPointX - pointLowX) * 64.0f / (pointHighX - pointLowX);
        u32 bankIndex = D_800E7A90[0x33];
        u8 *map = *(u8 **)(D_800E7A90 + (bankIndex * 4));
        s32 index = ((s32)cellZ << 6) + (s32)cellX;
        s32 rowOffset = selector << 6;
        u8 cell = map[index];
        u16 *row = (u16 *)((u32)D_801F36D8 + rowOffset);
        if (row[cell] == 0xFFFF)
            goto first_path;
        if (D_801EB520[map[origin]][cell] != 0)
            goto first_path;
    }
    current[0] = boundPointX;
    }
    current += 1;
    current[0] = point[1];
    current[1] = point[2];
    result = status;
    goto return_result;

    }
first_path:
    {
    f32 firstDistance;
    firstDistance = func_0002CB80(FIELD(first, f32, 0x08) - current[0],
                            FIELD(first, f32, 0x10) - current[2]);
    status = -2;
    if (0.8f < firstDistance) {
        f32 newY;
        if (0.005f < firstDistance) {
            f32 inputX = FIELD(first, f32, 0x08);
            f32 previousX = current[0];
            f32 previousZ = current[2];
            workX = (inputX - previousX) * 0.005f / firstDistance;
            workZ = (FIELD(first, f32, 0x10) - previousZ) * 0.005f / firstDistance;
            workX += previousX;
            workZ += previousZ;
            newY = func_801A63FC(D_800E7AB9, workX, workZ);
            status = -1;
        } else {
            workX = FIELD(first, f32, 0x08);
            workZ = FIELD(first, f32, 0x10);
            newY = FIELD(first, f32, 0x0C);
            status = -1;
        }
        point[0] = workX;
        point[1] = newY;
        point[2] = workZ;
    } else {
        point[0] = current[0];
        point[1] = current[1];
        point[2] = current[2];
    }
    if (point[0] < D_801F0D98) { result = 0; goto return_result; }
    if (D_801F0DA0 < point[0]) { result = 0; goto return_result; }
    if (point[2] < D_801F0D9C) { result = 0; goto return_result; }
    if (D_801F0DA4 < point[2]) { result = 0; goto return_result; }
    current[0] = point[0];
    current += 1;
    current[0] = point[1];
    current[1] = point[2];
    result = status;
    goto return_result;
    }
return_result:
    /* One real result consumer preserves the final zero-exit pocket during
     * early jump optimization. Direct returns rotated it past publication. */
    return result;
}
