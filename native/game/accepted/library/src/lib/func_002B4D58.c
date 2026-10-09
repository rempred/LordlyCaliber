typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;

/* Linear camera motion: per-tick step in X and Y, the tick count, and the
   ticks elapsed so far. Allocated once and cached at +0x1A4C of the
   Director scene block. */
typedef struct {
    f32 stepX;
    f32 stepY;
    s32 duration;
    s32 elapsed;
} CameraMotion;

extern u8 *D_8022A974; /* Director scene block */
extern u8 *D_8022A970; /* Director playback block: +0x83C phase, +0x828 counter, +0x830 total */

extern void *resource_alloc(s32 size);

/*
 * Director camera pan setup. Starts from the explicit start point, or from the
 * current camera position (+0x40/+0x44 of the camera record at +0x1C58) when
 * both start coordinates are -1. Components that move by one unit or less are
 * held at zero. The trailing phase update mirrors the playback block's
 * pan/return states; its exact meaning is a structural observation.
 */
void func_002B4D58(s32 startX, s32 startY, s32 unused, s32 targetX, s32 targetY, s32 reverse, s32 duration)
{
    CameraMotion *motion;
    u8 *camera;
    f32 x;
    f32 y;
    u8 phase;

    motion = *(CameraMotion **)(D_8022A974 + 0x1A4C);
    if (motion == 0) {
        motion = resource_alloc(0x10);
        *(CameraMotion **)(D_8022A974 + 0x1A4C) = motion;
    }

    if ((startX == -1) & (startY == -1)) {
        camera = *(u8 **)(D_8022A974 + 0x1C58);
        x = *(f32 *)(camera + 0x40);
        y = *(f32 *)(camera + 0x44);
    } else {
        x = (f32)startX;
        y = (f32)startY;
    }

    /* The start point is reused as the delta; this variable reuse is what
       keeps the retail float register assignment. */
    x = -x + (f32)targetX;
    y = -y + (f32)targetY;

    if ((x > 1.0f) | (x < -1.0f)) {
        motion->stepX = x / (f32)duration;
    } else {
        motion->stepX = 0.0f;
    }
    if ((y > 1.0f) | (y < -1.0f)) {
        motion->stepY = y / (f32)duration;
    } else {
        motion->stepY = 0.0f;
    }
    motion->elapsed = 0;
    motion->duration = duration;

    if (reverse == 0) {
        phase = *(u8 *)(D_8022A970 + 0x83C);
        if (phase == 3) {
            *(u8 *)(D_8022A970 + 0x83C) = 2;
            *(s32 *)(D_8022A970 + 0x828) = 0;
            *(s32 *)(D_8022A970 + 0x830) = duration;
        } else if (phase == 1) {
            *(u8 *)(D_8022A970 + 0x83C) = 2;
            *(s32 *)(D_8022A970 + 0x828) = *(s32 *)(D_8022A970 + 0x830) - *(s32 *)(D_8022A970 + 0x828);
        }
    } else {
        phase = *(u8 *)(D_8022A970 + 0x83C);
        if (phase == 4) {
            *(u8 *)(D_8022A970 + 0x83C) = 1;
            *(s32 *)(D_8022A970 + 0x828) = 0;
            *(s32 *)(D_8022A970 + 0x830) = duration;
        } else if (phase == 2) {
            *(u8 *)(D_8022A970 + 0x83C) = 1;
            *(s32 *)(D_8022A970 + 0x828) = *(s32 *)(D_8022A970 + 0x830) - *(s32 *)(D_8022A970 + 0x828);
        }
    }
}
